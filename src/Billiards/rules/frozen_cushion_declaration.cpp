#include "rules/frozen_cushion_declaration.h"

namespace billiardgl::rules {
namespace {

RefereeAuthorityConsumerPolicy policy()
{
    RefereeAuthorityConsumerPolicy value;
    value.declarationKind = kFrozenCushionDeclarationKind;
    value.allowedActorRoles = {RefereeAuthorityRole::Referee};
    value.proposalPayloadKeyTypes = {
        {"frozen_attested", RefereeAuthorityPayloadType::Bool},
        {"object_ball_id", RefereeAuthorityPayloadType::Number},
        {"relation", RefereeAuthorityPayloadType::String},
        {"straight_cushion_id", RefereeAuthorityPayloadType::String}};
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

FrozenCushionDeclarationResult reject(const std::string& code)
{
    FrozenCushionDeclarationResult result;
    result.errorCode = code;
    return result;
}

std::string proposalPayload(int object, StraightCushionId cushion)
{
    return std::string("{\"frozen_attested\":true,\"object_ball_id\":") +
        std::to_string(object) +
        ",\"relation\":\"OBJECT_BALL_FROZEN_TO_STRAIGHT_CUSHION\"," +
        "\"straight_cushion_id\":\"" + straightCushionIdName(cushion) +
        "\"}";
}

std::string finalPayload(int object, StraightCushionId cushion,
    const std::string& decision)
{
    return std::string("{\"frozen_attested\":true,\"object_ball_id\":") +
        std::to_string(object) +
        ",\"proposal_decision_id\":\"" + decision +
        "\",\"relation\":\"OBJECT_BALL_FROZEN_TO_STRAIGHT_CUSHION\"," +
        "\"straight_cushion_id\":\"" + straightCushionIdName(cushion) +
        "\"}";
}

std::string cancelPayload(const std::string& decision)
{
    return std::string("{\"proposal_decision_id\":\"") + decision +
        "\"}";
}

bool same(const FrozenCushionDeclarationContext& left,
    const FrozenCushionDeclarationContext& right)
{
    return left.objectBallId == right.objectBallId &&
        left.cushionId == right.cushionId &&
        left.parentRevision == right.parentRevision &&
        left.parentStateHash == right.parentStateHash;
}

}  // namespace

const char* straightCushionIdName(StraightCushionId id)
{
    switch (id) {
    case StraightCushionId::Head: return "HEAD";
    case StraightCushionId::Foot: return "FOOT";
    case StraightCushionId::LeftLong: return "LEFT_LONG";
    case StraightCushionId::RightLong: return "RIGHT_LONG";
    }
    return "UNKNOWN";
}

FrozenCushionDeclarationAuthority::FrozenCushionDeclarationAuthority(
    const RefereeAuthorityAuthenticator* authenticator,
    const RefereeAuthorityCommitGuard* commitGuard)
    : protocol_({policy()}, authenticator, commitGuard)
{
}

void FrozenCushionDeclarationAuthority::reset(const std::string& rackIdentity)
{
    protocol_.reset(rackIdentity);
    state_ = FrozenCushionDeclarationState{};
    state_.rackIdentity = rackIdentity;
}

FrozenCushionDeclarationResult FrozenCushionDeclarationAuthority::propose(
    const RefereeAuthorityRequest& request,
    const FrozenCushionDeclarationContext& context,
    std::uint64_t authorityTick)
{
    if (state_.phase != FrozenCushionDeclarationPhase::Inactive &&
        state_.phase != FrozenCushionDeclarationPhase::ProposalOpen)
        return reject("frozen_cushion_declaration_state");
    if (context.objectBallId < 1 || context.objectBallId > 15 ||
        context.parentStateHash.empty() ||
        request.payloadCanonical != proposalPayload(
            context.objectBallId, context.cushionId))
        return reject("frozen_cushion_declaration_context");
    const auto applied = protocol_.submit(request, authorityTick);
    if (!applied.ok) return reject(applied.errorCode);
    if (state_.phase == FrozenCushionDeclarationPhase::Inactive) {
        state_.phase = FrozenCushionDeclarationPhase::ProposalOpen;
        state_.context = context;
        state_.proposalDecisionId = request.decisionId;
        state_.refereeSubjectId = request.authoritySubjectId;
        state_.proposalPayloadSha256 =
            refereeAuthoritySha256Hex(request.payloadCanonical);
    }
    return {true, "", applied.ack};
}

FrozenCushionDeclarationResult FrozenCushionDeclarationAuthority::finalDeclare(
    const RefereeAuthorityRequest& request,
    const FrozenCushionDeclarationContext& context,
    std::uint64_t authorityTick)
{
    if (state_.phase != FrozenCushionDeclarationPhase::ProposalOpen &&
        state_.phase != FrozenCushionDeclarationPhase::Active &&
        state_.phase != FrozenCushionDeclarationPhase::Consumed)
        return reject("frozen_cushion_final_state");
    if (!same(context, state_.context) ||
        request.authoritySubjectId != state_.refereeSubjectId ||
        request.payloadCanonical != finalPayload(context.objectBallId,
            context.cushionId, state_.proposalDecisionId))
        return reject("frozen_cushion_final_context");
    const auto previous = state_.phase;
    const auto applied = protocol_.submit(request, authorityTick);
    if (!applied.ok) return reject(applied.errorCode);
    if (previous == FrozenCushionDeclarationPhase::ProposalOpen) {
        state_.phase = FrozenCushionDeclarationPhase::Active;
        state_.committed = true;
        state_.finalPayloadSha256 =
            refereeAuthoritySha256Hex(request.payloadCanonical);
    }
    return {true, "", applied.ack};
}

FrozenCushionDeclarationResult FrozenCushionDeclarationAuthority::cancel(
    const RefereeAuthorityRequest& request, std::uint64_t authorityTick)
{
    if ((state_.phase != FrozenCushionDeclarationPhase::ProposalOpen &&
         state_.phase != FrozenCushionDeclarationPhase::Cancelled) ||
        request.authoritySubjectId != state_.refereeSubjectId ||
        request.payloadCanonical != cancelPayload(state_.proposalDecisionId))
        return reject("frozen_cushion_cancel_context");
    const auto previous = state_.phase;
    const auto applied = protocol_.submit(request, authorityTick);
    if (!applied.ok) return reject(applied.errorCode);
    if (previous == FrozenCushionDeclarationPhase::ProposalOpen)
        state_.phase = FrozenCushionDeclarationPhase::Cancelled;
    return {true, "", applied.ack};
}

bool FrozenCushionDeclarationAuthority::consume(
    const std::string& shotIdentity, std::uint64_t authorityTick)
{
    if (state_.phase != FrozenCushionDeclarationPhase::Active ||
        !state_.committed || shotIdentity.empty())
        return false;
    state_.phase = FrozenCushionDeclarationPhase::Consumed;
    state_.consumedShotIdentity = shotIdentity;
    state_.consumedAuthorityTick = authorityTick;
    return true;
}

void FrozenCushionDeclarationAuthority::invalidate()
{
    if (state_.phase != FrozenCushionDeclarationPhase::Consumed &&
        state_.phase != FrozenCushionDeclarationPhase::Cancelled)
        state_.phase = FrozenCushionDeclarationPhase::Invalidated;
    state_.committed = false;
}

void FrozenCushionDeclarationAuthority::restoreCommittedForReplay(
    const std::string& rackIdentity,
    const FrozenCushionDeclarationContext& context,
    const std::string& proposalDecisionId,
    const std::string& refereeSubjectId)
{
    reset(rackIdentity);
    state_.phase = FrozenCushionDeclarationPhase::Active;
    state_.context = context;
    state_.proposalDecisionId = proposalDecisionId;
    state_.refereeSubjectId = refereeSubjectId;
    state_.proposalPayloadSha256 = refereeAuthoritySha256Hex(
        proposalPayload(context.objectBallId, context.cushionId));
    state_.finalPayloadSha256 = refereeAuthoritySha256Hex(
        finalPayload(context.objectBallId, context.cushionId,
            proposalDecisionId));
    state_.committed = true;
}

}  // namespace billiardgl::rules
