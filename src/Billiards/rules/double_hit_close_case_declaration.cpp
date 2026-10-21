#include "rules/double_hit_close_case_declaration.h"

namespace billiardgl::rules {
namespace {

RefereeAuthorityConsumerPolicy policy()
{
    RefereeAuthorityConsumerPolicy value;
    value.declarationKind = kDoubleHitCloseCaseDeclarationKind;
    value.allowedActorRoles = {RefereeAuthorityRole::Referee};
    value.proposalPayloadKeyTypes = {
        {"close_case_attested", RefereeAuthorityPayloadType::Bool},
        {"cue_ball_id", RefereeAuthorityPayloadType::Number},
        {"object_ball_id", RefereeAuthorityPayloadType::Number},
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

DoubleHitCloseCaseResult reject(const std::string& code)
{
    DoubleHitCloseCaseResult result;
    result.errorCode = code;
    return result;
}

std::string proposalPayload(int object)
{
    return std::string(
        "{\"close_case_attested\":true,\"cue_ball_id\":0,") +
        "\"object_ball_id\":" + std::to_string(object) +
        ",\"relation\":\"CUE_OBJECT_CLOSE_CASE_REQUIRES_POST_SHOT_REVIEW\"}";
}

std::string finalPayload(int object, const std::string& decision)
{
    return std::string(
        "{\"close_case_attested\":true,\"cue_ball_id\":0,") +
        "\"object_ball_id\":" + std::to_string(object) +
        ",\"proposal_decision_id\":\"" + decision +
        "\",\"relation\":\"CUE_OBJECT_CLOSE_CASE_REQUIRES_POST_SHOT_REVIEW\"}";
}

std::string cancelPayload(const std::string& decision)
{
    return std::string("{\"proposal_decision_id\":\"") + decision +
        "\"}";
}

bool same(const DoubleHitCloseCaseContext& left,
    const DoubleHitCloseCaseContext& right)
{
    return left.cueBallId == right.cueBallId &&
        left.objectBallId == right.objectBallId &&
        left.parentRevision == right.parentRevision &&
        left.parentStateHash == right.parentStateHash;
}

}  // namespace

DoubleHitCloseCaseAuthority::DoubleHitCloseCaseAuthority(
    const RefereeAuthorityAuthenticator* authenticator,
    const RefereeAuthorityCommitGuard* commitGuard)
    : protocol_({policy()}, authenticator, commitGuard)
{
}

void DoubleHitCloseCaseAuthority::reset(const std::string& rackIdentity)
{
    protocol_.reset(rackIdentity);
    state_ = DoubleHitCloseCaseState{};
    state_.rackIdentity = rackIdentity;
}

DoubleHitCloseCaseResult DoubleHitCloseCaseAuthority::propose(
    const RefereeAuthorityRequest& request,
    const DoubleHitCloseCaseContext& context, std::uint64_t tick)
{
    if (state_.phase != DoubleHitCloseCasePhase::Inactive &&
        state_.phase != DoubleHitCloseCasePhase::ProposalOpen)
        return reject("double_hit_close_case_state");
    if (context.cueBallId != 0 || context.objectBallId < 1 ||
        context.objectBallId > 15 || context.parentStateHash.empty() ||
        request.payloadCanonical != proposalPayload(context.objectBallId))
        return reject("double_hit_close_case_context");
    const auto applied = protocol_.submit(request, tick);
    if (!applied.ok) return reject(applied.errorCode);
    if (state_.phase == DoubleHitCloseCasePhase::Inactive) {
        state_.phase = DoubleHitCloseCasePhase::ProposalOpen;
        state_.context = context;
        state_.proposalDecisionId = request.decisionId;
        state_.refereeSubjectId = request.authoritySubjectId;
        state_.proposalPayloadSha256 =
            refereeAuthoritySha256Hex(request.payloadCanonical);
    }
    return {true,"",applied.ack};
}

DoubleHitCloseCaseResult DoubleHitCloseCaseAuthority::finalDeclare(
    const RefereeAuthorityRequest& request,
    const DoubleHitCloseCaseContext& context, std::uint64_t tick)
{
    if (state_.phase != DoubleHitCloseCasePhase::ProposalOpen &&
        state_.phase != DoubleHitCloseCasePhase::Active &&
        state_.phase != DoubleHitCloseCasePhase::Consumed)
        return reject("double_hit_close_case_final_state");
    if (!same(context,state_.context) ||
        request.authoritySubjectId != state_.refereeSubjectId ||
        request.payloadCanonical != finalPayload(
            context.objectBallId,state_.proposalDecisionId))
        return reject("double_hit_close_case_final_context");
    const auto previous = state_.phase;
    const auto applied = protocol_.submit(request,tick);
    if (!applied.ok) return reject(applied.errorCode);
    if (previous == DoubleHitCloseCasePhase::ProposalOpen) {
        state_.phase = DoubleHitCloseCasePhase::Active;
        state_.committed = true;
        state_.reviewNeeded = true;
        state_.finalPayloadSha256 =
            refereeAuthoritySha256Hex(request.payloadCanonical);
    }
    return {true,"",applied.ack};
}

DoubleHitCloseCaseResult DoubleHitCloseCaseAuthority::cancel(
    const RefereeAuthorityRequest& request, std::uint64_t tick)
{
    if ((state_.phase != DoubleHitCloseCasePhase::ProposalOpen &&
         state_.phase != DoubleHitCloseCasePhase::Cancelled) ||
        request.authoritySubjectId != state_.refereeSubjectId ||
        request.payloadCanonical != cancelPayload(state_.proposalDecisionId))
        return reject("double_hit_close_case_cancel_context");
    const auto previous = state_.phase;
    const auto applied = protocol_.submit(request,tick);
    if (!applied.ok) return reject(applied.errorCode);
    if (previous == DoubleHitCloseCasePhase::ProposalOpen)
        state_.phase = DoubleHitCloseCasePhase::Cancelled;
    return {true,"",applied.ack};
}

bool DoubleHitCloseCaseAuthority::consume(
    const std::string& shotIdentity, std::uint64_t tick)
{
    if (state_.phase != DoubleHitCloseCasePhase::Active ||
        !state_.committed || shotIdentity.empty())
        return false;
    state_.phase = DoubleHitCloseCasePhase::Consumed;
    state_.consumedShotIdentity = shotIdentity;
    state_.consumedAuthorityTick = tick;
    return true;
}

void DoubleHitCloseCaseAuthority::invalidate()
{
    if (state_.phase != DoubleHitCloseCasePhase::Consumed &&
        state_.phase != DoubleHitCloseCasePhase::Cancelled)
        state_.phase = DoubleHitCloseCasePhase::Invalidated;
    state_.committed = false;
    state_.reviewNeeded = false;
}

void DoubleHitCloseCaseAuthority::restoreCommittedForReplay(
    const std::string& rackIdentity,
    const DoubleHitCloseCaseContext& context,
    const std::string& proposalDecisionId,
    const std::string& refereeSubjectId)
{
    reset(rackIdentity);
    state_.phase = DoubleHitCloseCasePhase::Active;
    state_.context = context;
    state_.proposalDecisionId = proposalDecisionId;
    state_.refereeSubjectId = refereeSubjectId;
    state_.proposalPayloadSha256 = refereeAuthoritySha256Hex(
        proposalPayload(context.objectBallId));
    state_.finalPayloadSha256 = refereeAuthoritySha256Hex(
        finalPayload(context.objectBallId, proposalDecisionId));
    state_.committed = true;
    state_.reviewNeeded = true;
}

}  // namespace billiardgl::rules
