#include "ai/ai_turn_controller.h"

#include <exception>
#include <memory>

namespace billiardgl::ai {

void AiTurnController::cancel() noexcept
{
    worker_.cancel();
    awaitingResult_ = false;
    authorityHash_.clear();
    authorityRulesHash_.clear();
    retainedBallInHandDecisionId_.clear();
    authorityTick_ = 0;
    authorityPlayer_ = -1;
}

std::vector<int> AiTurnController::legalTargets(const GameRuntime& authority)
{
    std::vector<int> targets;
    const int player = authority.matchState().currentPlayer;
    const rules::BallGroup group = player >= 0 && player < 2 ?
        authority.matchState().groups[player] : rules::BallGroup::Open;
    if (group == rules::BallGroup::Open) {
        for (int ball = 1; ball < kBallCount; ++ball)
            if (ball != 8 && !authority.state().balls[ball].pocketed &&
                !authority.state().balls[ball].outOfPlay)
                targets.push_back(ball);
        return targets;
    }
    const int first = group == rules::BallGroup::Solids ? 1 : 9;
    const int last = group == rules::BallGroup::Solids ? 7 : 15;
    for (int ball = first; ball <= last; ++ball)
        if (!authority.state().balls[ball].pocketed &&
            !authority.state().balls[ball].outOfPlay)
            targets.push_back(ball);
    if (targets.empty() && !authority.state().balls[8].pocketed &&
        !authority.state().balls[8].outOfPlay)
        targets.push_back(8);
    return targets;
}

bool AiTurnController::begin(const GameRuntime& authority, Difficulty difficulty,
    std::uint64_t deterministicSeed, PlanningBudget budget,
    const std::string& retainedBallInHandDecisionId)
{
    const auto placement = authority.ballInHandPlacementOffer();
    const bool retainedPlacement = authority.ballInHandPlacementPending() &&
        !retainedBallInHandDecisionId.empty() && placement &&
        placement->decisionId == retainedBallInHandDecisionId &&
        placement->actorPlayerId == authority.matchState().currentPlayer &&
        authority.matchState().ballInHand;
    if (awaitingResult_ || authority.rulesMode() != RulesMode::ChineseEightBallV1 ||
        authority.state().ballsMoving || authority.state().gameOver ||
        authority.matchState().groupChoicePending ||
        (authority.ballInHandPlacementPending() && !retainedPlacement) ||
        authority.breakFoulDecisionPending() ||
        authority.eightOnBreakDecisionPending())
        return false;
    CandidateGenerationOptions options;
    options.legalTargetBalls = legalTargets(authority);
    // Candidate enumeration is target/pocket/power ordered and planning has a
    // real-time deadline.  Put the table-reaching powers first so a bounded
    // search cannot repeatedly select a short no-contact roll merely because
    // it was the first preview to finish.
    options.requestedPowers = authority.matchState().phase ==
            rules::MatchPhase::Break ?
        std::vector<float>{155.0f, 125.0f, 95.0f} :
        std::vector<float>{98.0f, 76.0f, 58.0f, 42.0f};
    options.maximumCandidates = budget.maximumCandidates;
    std::vector<ShotCandidate> candidates = enumerateTableCandidates(
        authority.state(), authority.physicsProfile(), options);
    if (candidates.empty()) return false;
    training::PreviewBudget previewBudget;
    // A legal table-length shot can remain in motion well beyond the former
    // 360-tick window.  Use the same bounded horizon as the shipping physical
    // acceptance scenes so reachability and pocket outcomes are observed,
    // while the planner's wall deadline remains the hard CPU limit.
    previewBudget.maximumTicks = 2400;
    previewBudget.maximumWallMilliseconds =
        static_cast<int>(budget.maximumWallTime.count());
    previewBudget.maximumBalls = kBallCount;
    auto evaluator = std::make_shared<GamePreviewEvaluator>(authority, previewBudget);
    if (!worker_.submit(difficulty, deterministicSeed, std::move(candidates),
            std::move(evaluator), budget))
        return false;
    authorityHash_ = gameStateSha256(authority.state());
    authorityRulesHash_ = authority.rulesAuthorityStateHash();
    retainedBallInHandDecisionId_ = retainedPlacement ?
        retainedBallInHandDecisionId : std::string{};
    authorityTick_ = authority.tick();
    authorityPlayer_ = authority.matchState().currentPlayer;
    awaitingResult_ = true;
    return true;
}

TurnResult AiTurnController::pollAndApply(GameRuntime& authority)
{
    TurnResult output;
    if (!awaitingResult_) return output;
    std::optional<PlanResult> ready;
    try {
        ready = worker_.poll();
    } catch (const std::exception& error) {
        awaitingResult_ = false;
        output.status = TurnPollStatus::Rejected;
        output.error = std::string("ai_worker_failed: ") + error.what();
        return output;
    } catch (...) {
        awaitingResult_ = false;
        output.status = TurnPollStatus::Rejected;
        output.error = "ai_worker_failed";
        return output;
    }
    if (!ready) {
        output.status = TurnPollStatus::Thinking;
        return output;
    }
    awaitingResult_ = false;
    output.plan = *ready;
    const auto placement = authority.ballInHandPlacementOffer();
    const bool retainedPlacement = authority.ballInHandPlacementPending() &&
        !retainedBallInHandDecisionId_.empty() && placement &&
        placement->decisionId == retainedBallInHandDecisionId_ &&
        placement->actorPlayerId == authorityPlayer_ &&
        authority.matchState().currentPlayer == authorityPlayer_ &&
        authority.matchState().ballInHand;
    if (authority.matchState().groupChoicePending ||
        (authority.ballInHandPlacementPending() && !retainedPlacement) ||
        authority.breakFoulDecisionPending() ||
        authority.eightOnBreakDecisionPending()) {
        output.status = TurnPollStatus::Rejected;
        output.error = authority.breakFoulDecisionPending() ?
            "rules_break_foul_decision_pending" :
            authority.eightOnBreakDecisionPending() ?
            "rules_eight_on_break_decision_pending" :
            authority.matchState().groupChoicePending ?
            "rules_group_choice_pending" :
            "rules_ball_in_hand_placement_pending";
        return output;
    }
    if (authority.tick() != authorityTick_ ||
        gameStateSha256(authority.state()) != authorityHash_ ||
        authority.rulesAuthorityStateHash() != authorityRulesHash_ ||
        authority.matchState().currentPlayer != authorityPlayer_ ||
        authority.state().ballsMoving || authority.state().gameOver) {
        output.status = TurnPollStatus::StaleAuthority;
        output.error = "ai_authority_changed";
        return output;
    }
    if (!ready->found) {
        output.status = TurnPollStatus::NoShot;
        output.error = ready->error;
        return output;
    }
    const ShotControlConversion checked = convertShotControls(
        ready->action.controls, authority.shotControlCapabilities(),
        authority.physicsProfile());
    if (!checked.ok) {
        output.status = TurnPollStatus::Rejected;
        output.error = checked.error;
        return output;
    }
    if (ready->action.targetPocket < 0 || ready->action.targetPocket >= 6) {
        output.status = TurnPollStatus::Rejected;
        output.error = "ai_target_pocket_out_of_range";
        return output;
    }

    // All values are validated before the first public player mutation. With
    // the authority hash/tick/player still bound, the following setters and
    // dispatches are an atomic semantic action on the single owner thread.
    ActionResult applied;
    GameAction yaw;
    yaw.type = ActionType::SetAimYaw;
    yaw.first = ready->action.controls.yawRadians;
    applied = authority.dispatch(yaw);
    GameAction power;
    power.type = ActionType::SetShotPower;
    power.first = ready->action.controls.requestedPower;
    if (applied.ok) applied = authority.dispatch(power);
    if (applied.ok) applied = authority.setTipOffset(
        ready->action.controls.tipOffsetX, ready->action.controls.tipOffsetY);
    if (applied.ok) applied = authority.setCalledPocket(
        ready->action.targetPocket);
    GameAction shoot;
    shoot.type = ActionType::Shoot;
    if (applied.ok) applied = authority.dispatch(shoot);
    if (!applied.ok) {
        output.status = TurnPollStatus::Rejected;
        output.error = applied.errorCode;
        return output;
    }
    output.status = TurnPollStatus::Applied;
    return output;
}

}  // namespace billiardgl::ai
