#include "rules/stalemate_authority.h"


namespace billiardgl::rules {
namespace {

RefereeAuthorityConsumerPolicy refusalPolicy()
{
    RefereeAuthorityConsumerPolicy policy;
    policy.declarationKind = kStalemateRefusalKind;
    policy.allowedActorRoles = {
        RefereeAuthorityRole::Player0, RefereeAuthorityRole::Player1};
    policy.exactPayloadKeyTypes = {
        {"proposal_decision_id", RefereeAuthorityPayloadType::String},
        {"refusing_player_id", RefereeAuthorityPayloadType::Number}};
    policy.maxPayloadBytes = 512;
    policy.maxLifetimeTicks = 100000;
    return policy;
}

RefereeAuthorityConsumerPolicy directivePolicy()
{
    RefereeAuthorityConsumerPolicy policy;
    policy.declarationKind = kStalemateDirectiveKind;
    policy.allowedActorRoles = {RefereeAuthorityRole::Referee};
    policy.proposalPayloadKeyTypes = {
        {"directive_kind", RefereeAuthorityPayloadType::String},
        {"per_player_shot_allowance", RefereeAuthorityPayloadType::Number},
        {"stalemate_attested", RefereeAuthorityPayloadType::Bool}};
    policy.finalPayloadKeyTypes = {
        {"counter_snapshot_sha256", RefereeAuthorityPayloadType::String},
        {"directive_kind", RefereeAuthorityPayloadType::String},
        {"per_player_completed_shots",
            RefereeAuthorityPayloadType::ArrayTwoNumbers},
        {"per_player_shot_allowance", RefereeAuthorityPayloadType::Number},
        {"proposal_decision_id", RefereeAuthorityPayloadType::String},
        {"stalemate_still_present_attested",
            RefereeAuthorityPayloadType::Bool}};
    policy.cancelPayloadKeyTypes = {
        {"proposal_decision_id", RefereeAuthorityPayloadType::String}};
    policy.finalPayloadMayDifferFromProposal = true;
    policy.cancelPayloadMayDifferFromProposal = true;
    policy.maxPayloadBytes = 1024;
    policy.maxLifetimeTicks = 100000;
    policy.proposalRequired = true;
    policy.sameActorCancelRequired = true;
    return policy;
}

StalemateResult rejected(const std::string& code)
{
    StalemateResult result;
    result.errorCode = code;
    return result;
}

std::string proposalPayload(const std::string& directive, int allowance)
{
    return std::string("{\"directive_kind\":\"") + directive +
        "\",\"per_player_shot_allowance\":" + std::to_string(allowance) +
        ",\"stalemate_attested\":true}";
}

std::string refusalPayload(const std::string& proposal, int player)
{
    return std::string("{\"proposal_decision_id\":\"") + proposal +
        "\",\"refusing_player_id\":" + std::to_string(player) + "}";
}

std::string finalPayload(const StalemateState& state)
{
    return std::string("{\"counter_snapshot_sha256\":\"") +
        state.counterSnapshotSha256 +
        "\",\"directive_kind\":\"" + state.directiveKind +
        "\",\"per_player_completed_shots\":[" +
        std::to_string(state.completedShots[0]) + "," +
        std::to_string(state.completedShots[1]) +
        "],\"per_player_shot_allowance\":" +
        std::to_string(state.perPlayerShotAllowance) +
        ",\"proposal_decision_id\":\"" + state.proposalDecisionId +
        "\",\"stalemate_still_present_attested\":true}";
}

std::string cancelPayload(const std::string& proposal)
{
    return std::string("{\"proposal_decision_id\":\"") + proposal + "\"}";
}

std::string requestFingerprint(const RefereeAuthorityRequest& request)
{
    return canonicalRefereeAuthenticatedBytes(request) + "|" +
        request.authenticationProof;
}

}  // namespace

StalemateAuthority::StalemateAuthority(
    const RefereeAuthorityAuthenticator* authenticator,
    const RefereeAuthorityCommitGuard* guard)
    : protocol_({directivePolicy(), refusalPolicy()}, authenticator, guard),
      guard_(guard)
{
}

void StalemateAuthority::reset(const std::string& rackIdentity,
                               int originalBreaker,
                               const std::array<int, 2>& order)
{
    protocol_.reset(rackIdentity);
    state_ = StalemateState{};
    state_.rackIdentity = rackIdentity;
    state_.originalBreakerPlayerId = originalBreaker;
    state_.originalShootingOrder = order;
    state_.orderSnapshotSha256 = refereeAuthoritySha256Hex(rackIdentity + "|" +
        std::to_string(originalBreaker) + "|" + std::to_string(order[0]) +
        "|" + std::to_string(order[1]));
    countedShots_.clear();
    refusalReceipts_.clear();
    finalReceipts_.clear();
    cancelReceipts_.clear();
    detachedNonces_.clear();
    updateCounterDigest();
}

void StalemateAuthority::updateCounterDigest()
{
    state_.counterSnapshotSha256 = refereeAuthoritySha256Hex(state_.proposalDecisionId + "|" +
        state_.rackIdentity + "|" +
        std::to_string(state_.windowStartAuthorityRevision) + "|" +
        std::to_string(state_.completedShots[0]) + "|" +
        std::to_string(state_.completedShots[1]) + "|" +
        state_.lastCountedShotIdentity[0] + "|" +
        state_.lastCountedShotIdentity[1]);
}

StalemateResult StalemateAuthority::retryOrConflict(
    const std::map<std::string, Receipt>& receipts,
    const RefereeAuthorityRequest& request) const
{
    const auto found = receipts.find(request.idempotencyKey);
    if (found == receipts.end())
        return rejected("stalemate_receipt_not_found");
    if (found->second.authenticatedBytes !=
            canonicalRefereeAuthenticatedBytes(request) ||
        found->second.authenticationProof != request.authenticationProof)
        return rejected("referee_authority_idempotency_conflict");
    return found->second.result;
}

StalemateResult StalemateAuthority::propose(
    const RefereeAuthorityRequest& request, std::uint64_t tick)
{
    if (state_.phase != StalematePhase::Inactive) {
        const RefereeAuthorityResult retry = protocol_.submit(request, tick);
        if (retry.ok) return StalemateResult{true, "", retry.ack};
        return rejected(retry.errorCode);
    }

    std::string directive;
    int allowance = 0;
    if (request.payloadCanonical ==
            proposalPayload("DEFAULT_ADVISORY_THREE_EACH", 3)) {
        directive = "DEFAULT_ADVISORY_THREE_EACH";
        allowance = 3;
    } else {
        for (int candidate = 1; candidate <= 255; ++candidate) {
            if (request.payloadCanonical == proposalPayload(
                    "EXPLICIT_FINITE_ALTERNATIVE", candidate)) {
                directive = "EXPLICIT_FINITE_ALTERNATIVE";
                allowance = candidate;
                break;
            }
        }
    }
    if (directive.empty()) return rejected("stalemate_payload_invalid");

    const RefereeAuthorityResult verified = protocol_.submit(request, tick);
    if (!verified.ok) return rejected(verified.errorCode);
    state_.perPlayerShotAllowance = allowance;
    state_.phase = StalematePhase::ProposalOpen;
    state_.proposalDecisionId = request.decisionId;
    state_.proposalPayloadSha256 = refereeAuthoritySha256Hex(request.payloadCanonical);
    state_.refereeSubjectId = request.authoritySubjectId;
    state_.directiveKind = directive;
    updateCounterDigest();
    return StalemateResult{true, "", verified.ack};
}

StalemateResult StalemateAuthority::refuse(
    const RefereeAuthorityRequest& request, std::uint64_t tick)
{
    if (refusalReceipts_.count(request.idempotencyKey))
        return retryOrConflict(refusalReceipts_, request);
    if (state_.phase != StalematePhase::ProposalOpen &&
        state_.phase != StalematePhase::ContinuationActive &&
        state_.phase != StalematePhase::AwaitingRefereeFinal)
        return rejected("stalemate_refusal_state");

    const int rolePlayer = request.authorityRole ==
            RefereeAuthorityRole::Player0 ? 0 :
        request.authorityRole == RefereeAuthorityRole::Player1 ? 1 : -1;
    if (rolePlayer < 0 || request.payloadCanonical !=
            refusalPayload(state_.proposalDecisionId, rolePlayer))
        return rejected("stalemate_refusal_actor");

    const std::string fingerprint = requestFingerprint(request);
    const auto priorNonce = detachedNonces_.find(request.authenticationNonce);
    if (priorNonce != detachedNonces_.end())
        return rejected(priorNonce->second == fingerprint ?
            "referee_authority_nonce_replay" :
            "referee_authority_nonce_conflict");
    const RefereeAuthorityResult verified = protocol_.verifyDetached(
        request, tick);
    if (!verified.ok) return rejected(verified.errorCode);
    if (guard_ && !guard_->allowCommit())
        return rejected("stalemate_commit_failed");

    if (state_.refusalHistory.empty()) {
        state_.phase = StalematePhase::ContinuationActive;
        state_.windowStartAuthorityRevision =
            protocol_.state().authorityRevision;
    }
    state_.refusalHistory.push_back(rolePlayer);
    state_.refusalAuditDigests.push_back(refereeAuthoritySha256Hex(fingerprint));
    updateCounterDigest();
    StalemateResult result{true, "", verified.ack};
    refusalReceipts_[request.idempotencyKey] = Receipt{
        canonicalRefereeAuthenticatedBytes(request),
        request.authenticationProof, result};
    detachedNonces_[request.authenticationNonce] = fingerprint;
    return result;
}

bool StalemateAuthority::recordCompletedShot(
    int player, const std::string& shotIdentity,
    const std::string& rackIdentity)
{
    if ((state_.phase != StalematePhase::ContinuationActive &&
         state_.phase != StalematePhase::AwaitingRefereeFinal) ||
        player < 0 || player > 1 || rackIdentity != state_.rackIdentity ||
        shotIdentity.empty() || !countedShots_.insert(shotIdentity).second)
        return false;
    ++state_.completedShots[player];
    state_.lastCountedShotIdentity[player] = shotIdentity;
    if (state_.completedShots[0] >= state_.perPlayerShotAllowance ||
        state_.completedShots[1] >= state_.perPlayerShotAllowance)
        state_.phase = StalematePhase::AwaitingRefereeFinal;
    updateCounterDigest();
    return true;
}

StalemateResult StalemateAuthority::finalDeclare(
    const RefereeAuthorityRequest& request, std::uint64_t tick)
{
    if (finalReceipts_.count(request.idempotencyKey))
        return retryOrConflict(finalReceipts_, request);
    if (state_.phase != StalematePhase::ContinuationActive &&
        state_.phase != StalematePhase::AwaitingRefereeFinal)
        return rejected("stalemate_final_state");

    if (request.authorityRole != RefereeAuthorityRole::Referee ||
        request.authoritySubjectId != state_.refereeSubjectId ||
        request.payloadCanonical != finalPayload(state_))
        return rejected("stalemate_final_context");

    const RefereeAuthorityResult verified = protocol_.submit(request, tick);
    if (!verified.ok) return rejected(verified.errorCode);
    state_.finalAuthorized = true;
    StalemateResult result{true, "", verified.ack};
    finalReceipts_[request.idempotencyKey] = Receipt{
        canonicalRefereeAuthenticatedBytes(request),
        request.authenticationProof, result};
    return result;
}

StalemateResult StalemateAuthority::cancel(
    const RefereeAuthorityRequest& request, std::uint64_t tick)
{
    if (cancelReceipts_.count(request.idempotencyKey))
        return retryOrConflict(cancelReceipts_, request);
    if (state_.phase == StalematePhase::Inactive || state_.rerackCommitted ||
        request.authorityRole != RefereeAuthorityRole::Referee ||
        request.authoritySubjectId != state_.refereeSubjectId ||
        request.payloadCanonical != cancelPayload(state_.proposalDecisionId))
        return rejected("stalemate_cancel_context");
    const RefereeAuthorityResult verified = protocol_.submit(request, tick);
    if (!verified.ok) return rejected(verified.errorCode);
    state_.phase = StalematePhase::Cancelled;
    StalemateResult result{true, "", verified.ack};
    cancelReceipts_[request.idempotencyKey] = Receipt{
        canonicalRefereeAuthenticatedBytes(request),
        request.authenticationProof, result};
    return result;
}

void StalemateAuthority::markRerackCommitted()
{
    if (!state_.finalAuthorized) return;
    state_.phase = StalematePhase::RerackCommitted;
    state_.rerackCommitted = true;
}

void StalemateAuthority::forceResetState()
{
    const std::string rackIdentity = state_.rackIdentity;
    const int breaker = state_.originalBreakerPlayerId;
    const std::array<int, 2> order = state_.originalShootingOrder;
    reset(rackIdentity, breaker, order);
}

}  // namespace billiardgl::rules
