#include "ai/ai_player_controller.h"

#include "deterministic_hash.h"
#include "game_runtime.h"

#include <algorithm>
#include <array>
#include <vector>

namespace billiardgl::ai {
namespace {

template <typename Container>
bool offered(const Container& actions, const char* action)
{
    return std::find(actions.begin(), actions.end(), action) != actions.end();
}

int activeGroupBalls(const GameRuntime& authority, rules::BallGroup group)
{
    const int first = group == rules::BallGroup::Solids ? 1 : 9;
    const int last = group == rules::BallGroup::Solids ? 7 : 15;
    int count = 0;
    for (int ball = first; ball <= last; ++ball)
        if (!authority.state().balls[ball].pocketed &&
            !authority.state().balls[ball].outOfPlay)
            ++count;
    return count;
}

bool findLegalPlacement(const GameRuntime& authority,
    rules::BallInHandPlacementPosition& output)
{
    const auto offer = authority.ballInHandPlacementOffer();
    if (!offer) return false;
    const auto& boundary = authority.physicsProfile().tableBoundary;
    const double radius = authority.physicsProfile().ball.radiusCm;
    const double xLimit = boundary.playfieldWidthCm * 0.5 - radius;
    const double zLimit = boundary.playfieldLengthCm * 0.5 - radius;
    std::vector<double> zCandidates;
    if (offer->scope ==
            rules::BallInHandPlacementScope::BehindHeadString) {
        for (double offset : {18.0, 32.0, 48.0, 64.0})
            zCandidates.push_back(std::max(-zLimit,
                offer->headStringWorldZCm - offset));
    } else {
        zCandidates = {-55.0, -30.0, 0.0, 30.0, 55.0};
    }
    const std::array<double, 9> xCandidates{{0.0, -18.0, 18.0, -36.0,
        36.0, -xLimit * 0.75, xLimit * 0.75, -xLimit, xLimit}};
    for (double z : zCandidates) {
        z = std::clamp(z, -zLimit, zLimit);
        for (double x : xCandidates) {
            const rules::BallInHandPlacementPosition candidate{x, z};
            const auto validation =
                authority.validateBallInHandPlacement(candidate);
            if (validation.ok) {
                output = validation.authoritativePosition;
                return true;
            }
        }
    }
    return false;
}

}  // namespace

const char* playerPollStatusId(PlayerPollStatus status) noexcept
{
    switch (status) {
    case PlayerPollStatus::Disabled: return "disabled";
    case PlayerPollStatus::WaitingForTurn: return "waiting_for_turn";
    case PlayerPollStatus::Thinking: return "thinking";
    case PlayerPollStatus::Applied: return "applied";
    case PlayerPollStatus::NoShot: return "no_shot";
    case PlayerPollStatus::StaleAuthority: return "stale_authority";
    case PlayerPollStatus::Rejected: return "rejected";
    case PlayerPollStatus::MatchComplete: return "match_complete";
    }
    return "unknown";
}

ActionResult AiPlayerController::configure(GameRuntime& authority,
    const PlayerConfiguration& configuration)
{
    if (configuration.player < 0 || configuration.player > 1)
        return ActionResult{false, "ai_player_out_of_range"};
    if (configuration.enabled && authority.state().ballsMoving)
        return ActionResult{false, "invalid_state"};
    if (configuration.enabled &&
        authority.rulesMode() != RulesMode::ChineseEightBallV1) {
        const ActionResult rules = authority.setRulesMode(
            RulesMode::ChineseEightBallV1);
        if (!rules.ok) return rules;
    }
    // Reconfiguration is a UI-owner action.  The planner works only on cloned
    // state, so an in-flight result can be cancelled without waiting for its
    // wall-time budget or risking an authority mutation.
    if (turn_.pending()) turn_.cancel();
    configuration_ = configuration;
    status_ = configuration.enabled ? PlayerPollStatus::WaitingForTurn
                                    : PlayerPollStatus::Disabled;
    error_.clear();
    attemptBlocked_ = false;
    jobsStarted_ = 0;
    lastAttemptHash_.clear();
    lastAttemptTick_ = 0;
    lastAttemptPlayer_ = -1;
    retainedBallInHandDecisionId_.clear();
    decisionOrdinal_ = 0;
    return ActionResult{};
}

PlayerState AiPlayerController::state() const
{
    PlayerState value;
    value.configuration = configuration_;
    value.status = status_;
    // A ready future is still pending until the owner thread polls and applies
    // it. Report that whole interval as thinking so product loops preserve the
    // bound tick instead of stepping between worker completion and collection.
    value.thinking = configuration_.enabled &&
        (turn_.pending() || turn_.retiring());
    value.jobsStarted = jobsStarted_;
    value.error = error_;
    return value;
}

void AiPlayerController::setResult(const TurnResult& result)
{
    error_ = result.error;
    switch (result.status) {
    case TurnPollStatus::Idle:
    case TurnPollStatus::Thinking:
        status_ = PlayerPollStatus::Thinking;
        break;
    case TurnPollStatus::Applied:
        status_ = PlayerPollStatus::Applied;
        attemptBlocked_ = false;
        retainedBallInHandDecisionId_.clear();
        break;
    case TurnPollStatus::NoShot:
        status_ = PlayerPollStatus::NoShot;
        attemptBlocked_ = true;
        break;
    case TurnPollStatus::StaleAuthority:
        status_ = PlayerPollStatus::StaleAuthority;
        attemptBlocked_ = false;
        break;
    case TurnPollStatus::Rejected:
        status_ = PlayerPollStatus::Rejected;
        attemptBlocked_ = true;
        break;
    }
}

PlayerState AiPlayerController::poll(GameRuntime& authority)
{
    if (turn_.pending()) {
        setResult(turn_.pollAndApply(authority));
        return state();
    }
    if (!configuration_.enabled) {
        status_ = PlayerPollStatus::Disabled;
        return state();
    }
    if (authority.state().gameOver) {
        status_ = PlayerPollStatus::MatchComplete;
        return state();
    }
    auto rejectDecision = [&](const std::string& error) {
        status_ = PlayerPollStatus::Rejected;
        error_ = error;
        attemptBlocked_ = true;
        return state();
    };
    auto waitForDecisionActor = [&]() {
        status_ = PlayerPollStatus::WaitingForTurn;
        error_.clear();
        attemptBlocked_ = false;
        return state();
    };
    auto decisionKey = [&](const char* prefix) {
        return std::string(prefix) + "-p" +
            std::to_string(configuration_.player) + '-' +
            std::to_string(++decisionOrdinal_);
    };
    if (authority.breakFoulDecisionPending()) {
        const auto offer = authority.breakFoulDecisionOffer();
        if (!offer) return rejectDecision("rules_break_foul_offer_missing");
        if (offer->actorPlayerId != configuration_.player)
            return waitForDecisionActor();
        const char* action = nullptr;
        if (offered(offer->offeredActionIds,
                rules::kBreakFoulAcceptTableActionId))
            action = rules::kBreakFoulAcceptTableActionId;
        else if (offered(offer->offeredActionIds,
                     rules::kBreakFoulBallInHandActionId))
            action = rules::kBreakFoulBallInHandActionId;
        else if (!offer->offeredActionIds.empty())
            action = offer->offeredActionIds.front().c_str();
        if (!action) return rejectDecision("rules_break_foul_no_action");
        rules::BreakFoulDecisionRequest request;
        request.rulesetId = rules::kBreakFoulRulesetId;
        request.capabilityId = rules::kBreakFoulCapabilityId;
        request.flowId = rules::kBreakFoulFlowId;
        request.decisionId = offer->decisionId;
        request.expectedAuthorityStateRevision =
            offer->authorityStateRevision;
        request.idempotencyKey = decisionKey("ai-break-foul");
        request.actorPlayerId = offer->actorPlayerId;
        request.actionId = action;
        request.payloadCanonical = "{}";
        const auto result = authority.submitBreakFoulDecision(request);
        if (!result.ok) return rejectDecision(result.errorCode);
        return waitForDecisionActor();
    }
    if (authority.eightOnBreakDecisionPending()) {
        const auto offer = authority.eightOnBreakDecisionOffer();
        if (!offer)
            return rejectDecision("rules_eight_on_break_offer_missing");
        if (offer->actorPlayerId != configuration_.player)
            return waitForDecisionActor();
        if (offer->offeredActionIds.empty())
            return rejectDecision("rules_eight_on_break_no_action");
        const char* preferred = offer->branch ==
                rules::EightOnBreakBranch::LegalBreak ?
            rules::kEightOnBreakSpotContinueActionId :
            rules::kEightOnBreakSpotBallInHandActionId;
        const char* action = offered(offer->offeredActionIds, preferred) ?
            preferred : offer->offeredActionIds.front().c_str();
        rules::EightOnBreakDecisionRequest request;
        request.rulesetId = rules::kEightOnBreakRulesetId;
        request.capabilityId = rules::kEightOnBreakCapabilityId;
        request.flowId = rules::kEightOnBreakFlowId;
        request.decisionId = offer->decisionId;
        request.expectedAuthorityStateRevision =
            offer->authorityStateRevision;
        request.idempotencyKey = decisionKey("ai-eight-break");
        request.actorPlayerId = offer->actorPlayerId;
        request.actionId = action;
        request.payloadCanonical = "{}";
        const auto result = authority.submitEightOnBreakDecision(request);
        if (!result.ok) return rejectDecision(result.errorCode);
        return waitForDecisionActor();
    }
    if (authority.matchState().groupChoicePending) {
        const auto offer = authority.groupChoiceDecisionOffer();
        if (!offer) return rejectDecision("rules_group_choice_offer_missing");
        if (offer->actorPlayerId != configuration_.player)
            return waitForDecisionActor();
        const rules::BallGroup choice = activeGroupBalls(authority,
                rules::BallGroup::Solids) <= activeGroupBalls(authority,
                rules::BallGroup::Stripes) ?
            rules::BallGroup::Solids : rules::BallGroup::Stripes;
        const char* action = choice == rules::BallGroup::Solids ?
            rules::kGroupChoiceSolidsActionId :
            rules::kGroupChoiceStripesActionId;
        if (!offered(offer->offeredActionIds, action))
            return rejectDecision("rules_group_choice_no_action");
        rules::GroupChoiceDecisionRequest request;
        request.rulesetId = rules::kGroupChoiceRulesetId;
        request.capabilityId = rules::kGroupChoiceAuthorityCapabilityId;
        request.flowId = rules::kGroupChoiceAuthorityFlowId;
        request.decisionId = offer->decisionId;
        request.expectedAuthorityStateRevision =
            offer->authorityStateRevision;
        request.idempotencyKey = decisionKey("ai-group-choice");
        request.actorPlayerId = offer->actorPlayerId;
        request.actionId = action;
        const auto result = authority.submitGroupChoiceDecision(request);
        if (!result.ok) return rejectDecision(result.errorCode);
        return waitForDecisionActor();
    }
    if (authority.ballInHandPlacementPending()) {
        const auto offer = authority.ballInHandPlacementOffer();
        if (!offer) return rejectDecision("rules_placement_offer_missing");
        if (offer->actorPlayerId != configuration_.player)
            return waitForDecisionActor();
        if (offer->offeredActionIds.empty() ||
            offer->offeredActionIds.front().empty())
            return rejectDecision("rules_placement_no_action");
        if (offer->decisionId != retainedBallInHandDecisionId_) {
            rules::BallInHandPlacementPosition position;
            if (!findLegalPlacement(authority, position))
                return rejectDecision("rules_placement_no_legal_position");
            rules::BallInHandPlacementRequest request;
            request.flowId = offer->scope ==
                    rules::BallInHandPlacementScope::BehindHeadString ?
                rules::kBehindHeadStringBallInHandFlowId :
                rules::kFullTableBallInHandFlowId;
            request.decisionId = offer->decisionId;
            request.expectedAuthorityStateRevision =
                offer->authorityStateRevision;
            request.idempotencyKey = decisionKey("ai-placement");
            request.actorPlayerId = offer->actorPlayerId;
            request.actionId = offer->offeredActionIds.front();
            request.position = position;
            const auto result = authority.submitBallInHandPlacement(request);
            if (!result.ok) return rejectDecision(result.errorCode);
            retainedBallInHandDecisionId_ =
                result.ack.replacementPlacementDecisionId;
            return waitForDecisionActor();
        }
    }
    if (authority.state().ballsMoving ||
        authority.matchState().currentPlayer != configuration_.player) {
        status_ = PlayerPollStatus::WaitingForTurn;
        error_.clear();
        attemptBlocked_ = false;
        return state();
    }
    if (turn_.retiring()) {
        status_ = PlayerPollStatus::Thinking;
        error_.clear();
        attemptBlocked_ = false;
        return state();
    }

    const std::string hash = gameStateSha256(authority.state());
    if (attemptBlocked_ && hash == lastAttemptHash_ &&
        authority.tick() == lastAttemptTick_ &&
        authority.matchState().currentPlayer == lastAttemptPlayer_) {
        return state();
    }
    lastAttemptHash_ = hash;
    lastAttemptTick_ = authority.tick();
    lastAttemptPlayer_ = authority.matchState().currentPlayer;
    if (!turn_.begin(authority, configuration_.difficulty,
            configuration_.deterministicSeed + jobsStarted_,
            defaultBudget(configuration_.difficulty),
            retainedBallInHandDecisionId_)) {
        status_ = PlayerPollStatus::NoShot;
        error_ = "ai_turn_not_started";
        attemptBlocked_ = true;
        return state();
    }
    ++jobsStarted_;
    status_ = PlayerPollStatus::Thinking;
    error_.clear();
    return state();
}

}  // namespace billiardgl::ai
