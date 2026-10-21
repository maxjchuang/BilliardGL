#include "rules/unintentional_break_miscue_declaration.h"

namespace billiardgl::rules {
namespace {

RefereeAuthorityConsumerPolicy policy()
{
    RefereeAuthorityConsumerPolicy value;
    value.declarationKind = kUnintentionalBreakMiscueDeclarationKind;
    value.allowedActorRoles = {RefereeAuthorityRole::Referee};
    value.proposalPayloadKeyTypes = {
        {"actor_player_id", RefereeAuthorityPayloadType::Number},
        {"relation", RefereeAuthorityPayloadType::String}};
    value.finalPayloadKeyTypes = value.proposalPayloadKeyTypes;
    value.finalPayloadKeyTypes["proposal_decision_id"] =
        RefereeAuthorityPayloadType::String;
    value.cancelPayloadKeyTypes = {
        {"proposal_decision_id", RefereeAuthorityPayloadType::String}};
    value.finalPayloadMayDifferFromProposal = true;
    value.cancelPayloadMayDifferFromProposal = true;
    value.maxPayloadBytes = 1024;
    value.maxLifetimeTicks = 100000;
    value.proposalRequired = true;
    value.sameActorCancelRequired = true;
    return value;
}

UnintentionalBreakMiscueDeclarationResult reject(const char* code)
{
    UnintentionalBreakMiscueDeclarationResult result;
    result.errorCode = code;
    return result;
}

std::string proposalPayload(int actorPlayerId)
{
    return std::string("{\"actor_player_id\":") +
        std::to_string(actorPlayerId) +
        ",\"relation\":\"UNINTENTIONAL_BREAK_MISCUE_IF_NATIVE_MISCUE_NO_RACK_CONTACT\"}";
}

std::string finalPayload(int actorPlayerId,
    const std::string& proposalDecisionId)
{
    return std::string("{\"actor_player_id\":") +
        std::to_string(actorPlayerId) +
        ",\"proposal_decision_id\":\"" + proposalDecisionId +
        "\",\"relation\":\"UNINTENTIONAL_BREAK_MISCUE_IF_NATIVE_MISCUE_NO_RACK_CONTACT\"}";
}

std::string cancelPayload(const std::string& proposalDecisionId)
{
    return std::string("{\"proposal_decision_id\":\"") +
        proposalDecisionId + "\"}";
}

bool same(const UnintentionalBreakMiscueDeclarationContext& left,
    const UnintentionalBreakMiscueDeclarationContext& right)
{
    return left.actorPlayerId == right.actorPlayerId &&
        left.parentRevision == right.parentRevision &&
        left.parentStateHash == right.parentStateHash;
}

}  // namespace

UnintentionalBreakMiscueDeclarationAuthority::
UnintentionalBreakMiscueDeclarationAuthority(
    const RefereeAuthorityAuthenticator* authenticator,
    const RefereeAuthorityCommitGuard* commitGuard)
    : protocol_({policy()}, authenticator, commitGuard)
{
}

void UnintentionalBreakMiscueDeclarationAuthority::reset(
    const std::string& rackIdentity)
{
    protocol_.reset(rackIdentity);
    state_ = UnintentionalBreakMiscueDeclarationState{};
    state_.rackIdentity = rackIdentity;
}

UnintentionalBreakMiscueDeclarationResult
UnintentionalBreakMiscueDeclarationAuthority::propose(
    const RefereeAuthorityRequest& request,
    const UnintentionalBreakMiscueDeclarationContext& context,
    std::uint64_t tick)
{
    if (state_.phase != UnintentionalBreakMiscueDeclarationPhase::Inactive &&
        state_.phase !=
            UnintentionalBreakMiscueDeclarationPhase::ProposalOpen)
        return reject("unintentional_break_miscue_declaration_state");
    if ((context.actorPlayerId != 0 && context.actorPlayerId != 1) ||
        context.parentStateHash.empty() ||
        request.payloadCanonical != proposalPayload(context.actorPlayerId))
        return reject("unintentional_break_miscue_declaration_context");
    const auto applied = protocol_.submit(request, tick);
    if (!applied.ok) {
        UnintentionalBreakMiscueDeclarationResult result;
        result.errorCode = applied.errorCode;
        return result;
    }
    if (state_.phase == UnintentionalBreakMiscueDeclarationPhase::Inactive) {
        state_.phase =
            UnintentionalBreakMiscueDeclarationPhase::ProposalOpen;
        state_.context = context;
        state_.proposalDecisionId = request.decisionId;
        state_.refereeSubjectId = request.authoritySubjectId;
        state_.proposalPayloadSha256 =
            refereeAuthoritySha256Hex(request.payloadCanonical);
    }
    return {true, "", applied.ack};
}

UnintentionalBreakMiscueDeclarationResult
UnintentionalBreakMiscueDeclarationAuthority::finalDeclare(
    const RefereeAuthorityRequest& request,
    const UnintentionalBreakMiscueDeclarationContext& context,
    std::uint64_t tick)
{
    if (state_.phase !=
            UnintentionalBreakMiscueDeclarationPhase::ProposalOpen &&
        state_.phase != UnintentionalBreakMiscueDeclarationPhase::Active &&
        state_.phase != UnintentionalBreakMiscueDeclarationPhase::Consumed)
        return reject("unintentional_break_miscue_declaration_final_state");
    if (!same(context, state_.context) ||
        request.authoritySubjectId != state_.refereeSubjectId ||
        request.payloadCanonical != finalPayload(
            context.actorPlayerId, state_.proposalDecisionId))
        return reject("unintentional_break_miscue_declaration_final_context");
    const auto previous = state_.phase;
    const auto applied = protocol_.submit(request, tick);
    if (!applied.ok) {
        UnintentionalBreakMiscueDeclarationResult result;
        result.errorCode = applied.errorCode;
        return result;
    }
    if (previous ==
            UnintentionalBreakMiscueDeclarationPhase::ProposalOpen) {
        state_.phase = UnintentionalBreakMiscueDeclarationPhase::Active;
        state_.committed = true;
        state_.finalPayloadSha256 =
            refereeAuthoritySha256Hex(request.payloadCanonical);
    }
    return {true, "", applied.ack};
}

UnintentionalBreakMiscueDeclarationResult
UnintentionalBreakMiscueDeclarationAuthority::cancel(
    const RefereeAuthorityRequest& request, std::uint64_t tick)
{
    if ((state_.phase !=
             UnintentionalBreakMiscueDeclarationPhase::ProposalOpen &&
         state_.phase !=
             UnintentionalBreakMiscueDeclarationPhase::Cancelled) ||
        request.authoritySubjectId != state_.refereeSubjectId ||
        request.payloadCanonical != cancelPayload(state_.proposalDecisionId))
        return reject("unintentional_break_miscue_declaration_cancel_context");
    const auto previous = state_.phase;
    const auto applied = protocol_.submit(request, tick);
    if (!applied.ok) {
        UnintentionalBreakMiscueDeclarationResult result;
        result.errorCode = applied.errorCode;
        return result;
    }
    if (previous ==
            UnintentionalBreakMiscueDeclarationPhase::ProposalOpen)
        state_.phase = UnintentionalBreakMiscueDeclarationPhase::Cancelled;
    return {true, "", applied.ack};
}

bool UnintentionalBreakMiscueDeclarationAuthority::consume(
    const std::string& shotIdentity, std::uint64_t tick,
    const std::string& strokeBindingSha256)
{
    if (state_.phase != UnintentionalBreakMiscueDeclarationPhase::Active ||
        !state_.committed || shotIdentity.empty())
        return false;
    state_.phase = UnintentionalBreakMiscueDeclarationPhase::Consumed;
    state_.consumedShotIdentity = shotIdentity;
    state_.consumedStrokeBindingSha256 = strokeBindingSha256;
    state_.consumedAuthorityTick = tick;
    return true;
}

void UnintentionalBreakMiscueDeclarationAuthority::invalidate()
{
    if (state_.phase != UnintentionalBreakMiscueDeclarationPhase::Consumed &&
        state_.phase != UnintentionalBreakMiscueDeclarationPhase::Cancelled)
        state_.phase = UnintentionalBreakMiscueDeclarationPhase::Invalidated;
    state_.committed = false;
}

void UnintentionalBreakMiscueDeclarationAuthority::restoreCommittedForReplay(
    const std::string& rackIdentity,
    const UnintentionalBreakMiscueDeclarationContext& context,
    const std::string& proposalDecisionId,
    const std::string& refereeSubjectId)
{
    // Replay imports an already-verified, secret-free receipt.  It does not
    // run the live authenticator or create a path for a player to self-sign.
    reset(rackIdentity);
    state_.phase = UnintentionalBreakMiscueDeclarationPhase::Active;
    state_.context = context;
    state_.proposalDecisionId = proposalDecisionId;
    state_.refereeSubjectId = refereeSubjectId;
    state_.proposalPayloadSha256 = refereeAuthoritySha256Hex(
        proposalPayload(context.actorPlayerId));
    state_.finalPayloadSha256 = refereeAuthoritySha256Hex(
        finalPayload(context.actorPlayerId, proposalDecisionId));
    state_.committed = true;
}

}  // namespace billiardgl::rules
