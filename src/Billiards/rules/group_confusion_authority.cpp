#include "rules/group_confusion_authority.h"

#include <utility>

namespace billiardgl::rules {
namespace {

RefereeAuthorityConsumerPolicy groupConfusionPolicy()
{
    RefereeAuthorityConsumerPolicy policy;
    policy.declarationKind = kGroupConfusionDeclarationKind;
    policy.allowedActorRoles = {RefereeAuthorityRole::Referee,
        RefereeAuthorityRole::Player0, RefereeAuthorityRole::Player1};
    policy.proposalPayloadKeyTypes = {
        {"assignment_snapshot_sha256", RefereeAuthorityPayloadType::String},
        {"claimed_confusion_kind", RefereeAuthorityPayloadType::String},
        {"completed_shot_identity", RefereeAuthorityPayloadType::String},
        {"realization_attested", RefereeAuthorityPayloadType::Bool},
        {"window_generation", RefereeAuthorityPayloadType::Number}};
    policy.finalPayloadKeyTypes = {
        {"assignment_snapshot_sha256", RefereeAuthorityPayloadType::String},
        {"claimed_confusion_kind", RefereeAuthorityPayloadType::String},
        {"completed_shot_identity", RefereeAuthorityPayloadType::String},
        {"proposal_decision_id", RefereeAuthorityPayloadType::String},
        {"rack_order_snapshot_sha256", RefereeAuthorityPayloadType::String},
        {"realization_still_attested", RefereeAuthorityPayloadType::Bool},
        {"window_generation", RefereeAuthorityPayloadType::Number}};
    policy.finalPayloadMayDifferFromProposal = true;
    policy.maxPayloadBytes = 2048;
    policy.maxLifetimeTicks = 100000;
    policy.proposalRequired = true;
    return policy;
}

GroupConfusionResult rejected(const std::string& code)
{
    GroupConfusionResult result;
    result.errorCode = code;
    return result;
}

std::string proposalPayload(const GroupConfusionState& state)
{
    return std::string("{\"assignment_snapshot_sha256\":\"") +
        state.window.assignmentSnapshotSha256 +
        "\",\"claimed_confusion_kind\":\"ASSIGNMENT_OR_SEQUENCE_CONFUSION\"," +
        "\"completed_shot_identity\":\"" +
        state.window.completedShotIdentity +
        "\",\"realization_attested\":true,\"window_generation\":" +
        std::to_string(state.windowGeneration) + "}";
}

std::string finalPayload(const GroupConfusionState& state)
{
    return std::string("{\"assignment_snapshot_sha256\":\"") +
        state.window.assignmentSnapshotSha256 +
        "\",\"claimed_confusion_kind\":\"ASSIGNMENT_OR_SEQUENCE_CONFUSION\"," +
        "\"completed_shot_identity\":\"" +
        state.window.completedShotIdentity +
        "\",\"proposal_decision_id\":\"" + state.proposalDecisionId +
        "\",\"rack_order_snapshot_sha256\":\"" +
        state.rackOrderSnapshotSha256 +
        "\",\"realization_still_attested\":true,\"window_generation\":" +
        std::to_string(state.windowGeneration) + "}";
}

bool validWindow(const GroupConfusionWindow& value,
                 const std::string& rackIdentity)
{
    return !rackIdentity.empty() && value.rackIdentity == rackIdentity &&
        (value.originalBreakerPlayerId == 0 ||
         value.originalBreakerPlayerId == 1) &&
        value.originalShootingOrder[0] == value.originalBreakerPlayerId &&
        value.originalShootingOrder[1] == 1-value.originalBreakerPlayerId &&
        !value.assignmentSnapshotSha256.empty() &&
        !value.completedShotIdentity.empty() &&
        value.completedShotOrdinal > 0 &&
        (value.completedShooter == 0 || value.completedShooter == 1) &&
        !value.completedStateHash.empty();
}

}  // namespace

GroupConfusionAuthority::GroupConfusionAuthority(
    const RefereeAuthorityAuthenticator* authenticator,
    const RefereeAuthorityCommitGuard* guard)
    : protocol_({groupConfusionPolicy()}, authenticator, guard)
{
}

void GroupConfusionAuthority::reset(
    const std::string& rackIdentity, int originalBreaker,
    const std::array<int, 2>& originalOrder)
{
    protocol_.reset(rackIdentity);
    resetRackIdentity_ = rackIdentity;
    resetOriginalBreaker_ = originalBreaker;
    resetOriginalOrder_ = originalOrder;
    state_ = GroupConfusionState{};
    state_.window.rackIdentity = rackIdentity;
    state_.window.originalBreakerPlayerId = originalBreaker;
    state_.window.originalShootingOrder = originalOrder;
    state_.rackOrderSnapshotSha256 = refereeAuthoritySha256Hex(
        rackIdentity + "|" + std::to_string(originalBreaker) + "|" +
        std::to_string(originalOrder[0]) + "|" +
        std::to_string(originalOrder[1]));
}

bool GroupConfusionAuthority::openWindow(
    const GroupConfusionWindow& window)
{
    if (state_.phase == GroupConfusionPhase::RealizationProposed ||
        state_.phase == GroupConfusionPhase::RerackCommitting ||
        state_.phase == GroupConfusionPhase::RerackCommitted ||
        !validWindow(window, resetRackIdentity_) ||
        window.originalBreakerPlayerId != resetOriginalBreaker_ ||
        window.originalShootingOrder != resetOriginalOrder_)
        return false;
    state_.window = window;
    ++state_.windowGeneration;
    state_.phase = GroupConfusionPhase::TimelyWindowOpen;
    state_.proposalDecisionId.clear();
    state_.proposalActorSubjectId.clear();
    state_.proposalPayloadSha256.clear();
    state_.finalAuthorized = false;
    state_.rerackCommitted = false;
    return true;
}

void GroupConfusionAuthority::expireWindowOnAcceptedShoot()
{
    if (state_.phase == GroupConfusionPhase::TimelyWindowOpen)
        state_.phase = GroupConfusionPhase::WindowExpired;
}

GroupConfusionResult GroupConfusionAuthority::propose(
    const RefereeAuthorityRequest& request, std::uint64_t tick)
{
    if (state_.phase != GroupConfusionPhase::TimelyWindowOpen &&
        state_.phase != GroupConfusionPhase::RealizationProposed)
        return rejected("group_confusion_window_not_open");
    if (request.payloadCanonical != proposalPayload(state_))
        return rejected("group_confusion_payload_invalid");
    const RefereeAuthorityResult verified = protocol_.submit(request, tick);
    if (!verified.ok) return rejected(verified.errorCode);
    if (state_.phase == GroupConfusionPhase::TimelyWindowOpen) {
        state_.phase = GroupConfusionPhase::RealizationProposed;
        state_.proposalDecisionId = request.decisionId;
        state_.proposalActorRole = request.authorityRole;
        state_.proposalActorSubjectId = request.authoritySubjectId;
        state_.proposalPayloadSha256 =
            refereeAuthoritySha256Hex(request.payloadCanonical);
    }
    return GroupConfusionResult{true, "", verified.ack};
}

GroupConfusionResult GroupConfusionAuthority::finalDeclare(
    const RefereeAuthorityRequest& request, std::uint64_t tick)
{
    if (state_.phase != GroupConfusionPhase::RealizationProposed &&
        state_.phase != GroupConfusionPhase::RerackCommitted)
        return rejected("group_confusion_final_state");
    if (request.authorityRole != state_.proposalActorRole ||
        request.authoritySubjectId != state_.proposalActorSubjectId ||
        request.payloadCanonical != finalPayload(state_))
        return rejected("group_confusion_final_context");
    const RefereeAuthorityResult verified = protocol_.submit(request, tick);
    if (!verified.ok) return rejected(verified.errorCode);
    state_.finalAuthorized = true;
    return GroupConfusionResult{true, "", verified.ack};
}

GroupConfusionResult GroupConfusionAuthority::cancel(
    const RefereeAuthorityRequest&, std::uint64_t) const
{
    return rejected("group_confusion_cancel_unsupported");
}

void GroupConfusionAuthority::markRerackCommitting()
{
    if (state_.finalAuthorized &&
        state_.phase == GroupConfusionPhase::RealizationProposed)
        state_.phase = GroupConfusionPhase::RerackCommitting;
}

void GroupConfusionAuthority::markRerackCommitted()
{
    if (!state_.finalAuthorized) return;
    state_.phase = GroupConfusionPhase::RerackCommitted;
    state_.rerackCommitted = true;
}

}  // namespace billiardgl::rules
