#include "training/preview_runtime.h"

#include "deterministic_hash.h"
#include "rules/shot_fact_collector.h"

#include <chrono>

namespace billiardgl::training {

PreviewRuntime::PreviewRuntime(const GameState& state,
    const PhysicsProfile& profile, PhysicsBoundaryMode boundaryMode,
    bool collectShotFacts)
    : state_(state), profile_(profile), boundaryMode_(boundaryMode),
      collectShotFacts_(collectShotFacts)
{
}

PreviewResult PreviewRuntime::run(
    const CueImpactInput& shot, const PreviewBudget& budget) const
{
    PreviewResult result;
    result.maximumTicks = budget.maximumTicks;
    result.maximumWallMilliseconds = budget.maximumWallMilliseconds;
    if (budget.maximumTicks < 0 || budget.maximumTicks > 10000 ||
        budget.maximumWallMilliseconds <= 0 ||
        budget.maximumWallMilliseconds > 10000 ||
        budget.maximumBalls <= 0 || budget.maximumBalls > kBallCount) {
        result.errorCode = "preview_invalid_budget";
        return result;
    }
    int activeBalls = 0;
    for (const BallState& ball : state_.balls)
        if (!ball.pocketed && !ball.outOfPlay) ++activeBalls;
    if (activeBalls > budget.maximumBalls) {
        result.errorCode = "preview_ball_budget_exceeded";
        return result;
    }

    GameRuntime sandbox;
    const ActionResult loaded = sandbox.replaceStateForScenario(
        state_, profile_, nullptr, false, boundaryMode_);
    if (!loaded.ok) {
        result.errorCode = loaded.errorCode;
        return result;
    }
    const ActionResult applied = sandbox.applyCueImpact(shot);
    if (!applied.ok) {
        result.errorCode = applied.errorCode;
        return result;
    }
    rules::ShotFactCollector shotFacts;
    if (collectShotFacts_) shotFacts.begin(state_, 0, false);

    const auto started = std::chrono::steady_clock::now();
    for (int tick = 0; tick < budget.maximumTicks; ++tick) {
        const auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::steady_clock::now() - started).count();
        if (elapsed >= budget.maximumWallMilliseconds) {
            result.wallBudgetReached = true;
            break;
        }
        const GameState before = sandbox.state();
        const ActionResult stepped = sandbox.stepIsolated(1);
        if (!stepped.ok) {
            result.errorCode = stepped.errorCode;
            return result;
        }
        ++result.simulatedTicks;
        if (collectShotFacts_) {
            if (!sandbox.hasRecentPhysicsStep()) {
                result.errorCode = "preview_physics_receipt_missing";
                return result;
            }
            shotFacts.consume(before, sandbox.state(),
                sandbox.latestPhysicsStep().telemetry, sandbox.tick());
        }
        if (sandbox.state().events.railCollision)
            ++result.railCollisionEvents;
        PreviewSample sample;
        sample.tick = sandbox.tick();
        sample.positions.reserve(static_cast<std::size_t>(activeBalls));
        for (const BallState& ball : sandbox.state().balls) {
            if (!ball.pocketed && !ball.outOfPlay) sample.positions.push_back(ball.position);
        }
        sample.cueBallVisible = !sandbox.state().balls[0].pocketed &&
            !sandbox.state().balls[0].outOfPlay;
        if (sample.cueBallVisible)
            sample.cueBallPosition = sandbox.state().balls[0].position;
        result.samples.push_back(sample);
        if (!sandbox.state().ballsMoving) break;
    }
    result.tickBudgetReached = sandbox.state().ballsMoving &&
        result.simulatedTicks == budget.maximumTicks;
    if (collectShotFacts_ && !sandbox.state().ballsMoving) {
        result.shotFacts = shotFacts.finish();
        result.hasShotFacts = true;
    }
    result.elapsedWallMilliseconds =
        std::chrono::duration<double, std::milli>(
            std::chrono::steady_clock::now() - started).count();
    result.finalStateSha256 = gameStateSha256(sandbox.state());
    for (int index = 0; index < kBallCount; ++index) {
        result.finalPositions[index] = sandbox.state().balls[index].position;
        result.finalPocketed[index] = sandbox.state().balls[index].pocketed;
    }
    result.eventsEmitted = !sandbox.events().empty();
    result.ok = true;
    return result;
}

PreviewResult previewTrainingAssist(
    const GameRuntime& authority, const PreviewBudget& budget)
{
    const ShotControlConversion converted = convertShotControls(
        authority.shotControl(), authority.shotControlCapabilities(),
        authority.physicsProfile());
    if (!converted.ok) {
        PreviewResult result;
        result.errorCode = converted.error;
        return result;
    }
    return PreviewRuntime(authority.state(), authority.physicsProfile(),
        authority.boundaryMode()).run(converted.command.cueImpact, budget);
}

}  // namespace billiardgl::training
