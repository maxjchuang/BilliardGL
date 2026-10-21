#include "render_state.h"

#include "game_runtime.h"

namespace billiardgl {
namespace {

const char* presentationRulesPhase(rules::MatchPhase phase)
{
    switch (phase) {
    case rules::MatchPhase::Break: return "break";
    case rules::MatchPhase::OpenTable: return "open_table";
    case rules::MatchPhase::GroupsAssigned: return "groups_assigned";
    case rules::MatchPhase::Complete: return "complete";
    }
    return "unknown";
}

const char* presentationBallGroup(rules::BallGroup group)
{
    switch (group) {
    case rules::BallGroup::Open: return "open";
    case rules::BallGroup::Solids: return "solids";
    case rules::BallGroup::Stripes: return "stripes";
    }
    return "unknown";
}

const char* nonphysicalReasonCode(rules::NonphysicalGeneralFoulKind kind)
{
    switch (kind) {
    case rules::NonphysicalGeneralFoulKind::BothFeetOffGroundDuringShot:
        return "both_feet_off_ground_during_shot";
    case rules::NonphysicalGeneralFoulKind::IllegalBodyClothingOrEquipmentBallTouch:
        return "illegal_body_clothing_or_equipment_ball_touch";
    case rules::NonphysicalGeneralFoulKind::OneHandShot:
        return "one_hand_shot";
    case rules::NonphysicalGeneralFoulKind::AuxiliaryEquipmentPlacedOnTableToShoot:
        return "auxiliary_equipment_placed_on_table_to_shoot";
    case rules::NonphysicalGeneralFoulKind::MobileDeviceDisturbanceGeneralFoul:
        return "mobile_device_disturbance_general_foul";
    case rules::NonphysicalGeneralFoulKind::MobileDeviceRepeatLossOfRack:
        return "mobile_device_repeat_loss_of_rack";
    }
    return "unknown";
}

}  // namespace

RulesPresentation buildRulesPresentation(const GameRuntime& runtime)
{
    RulesPresentation result;
    result.enabled = runtime.rulesMode() == RulesMode::ChineseEightBallV1;
    result.rulesetId = result.enabled ? "wpa_heyball_2025_08_16_v1" : "legacy";
    const rules::MatchState& state = runtime.matchState();
    result.phase = presentationRulesPhase(state.phase);
    result.currentPlayer = state.currentPlayer;
    result.groups = {{presentationBallGroup(state.groups[0]),
        presentationBallGroup(state.groups[1])}};
    result.ballInHand = state.ballInHand;
    result.gameOver = state.phase == rules::MatchPhase::Complete;
    result.winner = state.winner;
    result.hasAdjudication = runtime.hasLastAdjudication();
    const auto& terminal = runtime.rulesRackTerminalState();
    if (result.enabled && terminal.active) {
        result.rackEnded = true;
        result.rackWinner = terminal.winningPlayer;
        result.foul = true;
        result.ballInHand = false;
        if (terminal.reason == RulesRackTerminalReason::MobileDeviceRepeatViolation)
            result.reasonCodes.emplace_back("mobile_device_repeat_loss_of_rack");
        return result;
    }
    if (!result.hasAdjudication) return result;
    const rules::AdjudicationResult& adjudication = runtime.lastAdjudication();
    result.foul = adjudication.foul;
    result.continueTurn = adjudication.continueTurn;
    result.gameOver = adjudication.gameOver;
    result.winner = adjudication.winner;
    for (rules::FoulReason reason : adjudication.reasons)
        result.reasonCodes.emplace_back(rules::foulReasonCode(reason));
    result.textKeys = adjudication.textKeys;
    if (const auto& explanation = runtime.nonphysicalFoulExplanation()) {
        const std::string code = nonphysicalReasonCode(explanation->kind);
        result.reasonCodes.push_back(code);
        result.textKeys.push_back("rules.foul." + code);
    }
    // These exact-action authority flows are runtime-only. Until dedicated
    // Product UI exists, do not route their action ids into the legacy
    // break/rerack decision presentation.
    if (!adjudication.groupChoiceRequired &&
        !adjudication.eightBallMustBeSpotted &&
        !runtime.breakFoulDecisionPending() &&
        !runtime.ballInHandPlacementPending())
        result.decisionOptions = adjudication.decisionOptions;
    return result;
}

}  // namespace billiardgl
