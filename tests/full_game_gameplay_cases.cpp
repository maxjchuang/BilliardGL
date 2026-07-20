#include "full_game_gameplay_cases.h"

#include "full_game_invariants.h"
#include "full_game_prng.h"

#include <algorithm>
#include <cmath>
#include <string>
#include <vector>

namespace fullgame {
namespace {

billiardgl::GameState isolatedState()
{
    billiardgl::GameState state;
    for (billiardgl::BallState& ball : state.balls) {
        ball = billiardgl::BallState{};
        ball.pocketed = true;
        ball.pocketInteraction.phase =
            billiardgl::PocketInteractionPhase::Captured;
    }
    return state;
}

void activateBall(billiardgl::GameState& state, int index, float x, float z)
{
    billiardgl::BallState& ball = state.balls[index];
    ball = billiardgl::BallState{};
    ball.position = billiardgl::Point3{
        x, billiardgl::kTableHeight + billiardgl::kBallRadius, z};
    ball.pocketed = false;
    ball.motionState = billiardgl::BallMotionState::Stationary;
}

billiardgl::ActionResult control(billiardgl::GameRuntime& runtime,
    billiardgl::ActionType type, float value = 0.0f)
{
    billiardgl::GameAction action;
    action.type = type;
    action.first = value;
    return runtime.dispatch(action);
}

billiardgl::ActionResult aimAndShoot(
    billiardgl::GameRuntime& runtime, float yaw, float power)
{
    billiardgl::ActionResult result = control(
        runtime, billiardgl::ActionType::SetAimYaw, yaw);
    if (!result.ok) return result;
    result = control(runtime, billiardgl::ActionType::SetShotPower, power);
    if (!result.ok) return result;
    return control(runtime, billiardgl::ActionType::Shoot);
}

bool operationallyLegal(const billiardgl::GameRuntime& runtime)
{
    return !runtime.state().ballsMoving && !runtime.state().gameOver &&
        !runtime.state().balls[0].pocketed;
}

void appendRuntime(FullGameCaseResult& result,
    const billiardgl::GameRuntime& runtime)
{
    result.frames.insert(result.frames.end(),
        runtime.physicsTrace().frames().begin(),
        runtime.physicsTrace().frames().end());
    result.events.insert(result.events.end(), runtime.events().begin(),
        runtime.events().end());
    result.droppedTraceFrames += runtime.physicsTrace().droppedFrames();
}

void countEvents(FullGameCaseResult& result)
{
    for (const billiardgl::PhysicsFrame& frame : result.frames)
        result.surfaceTransitions += static_cast<int>(
            frame.surfaceTransitions.size());
    for (const billiardgl::RuntimeEvent& event : result.events) {
        if (event.name == "ball_collision") ++result.ballCollisions;
        else if (event.name == "rail_collision") ++result.railCollisions;
        else if (event.name == "ball_pocketed") ++result.objectBallCaptures;
        else if (event.name == "cue_ball_pocketed") ++result.cueBallCaptures;
        else if (event.name == "score_updated") ++result.scoreEvents;
        else if (event.name == "foul") ++result.foulEvents;
        else if (event.name == "turn_transferred") ++result.turnTransfers;
        else if (event.name == "shot_ended") ++result.completedShots;
    }
}

void finalize(FullGameCaseResult& result)
{
    countEvents(result);
    const FullGameInvariantResult invariants = evaluateFullGameInvariants(
        result.frames, result.events, result.droppedTraceFrames, 0.0, 0);
    result.maximumPenetrationCm = invariants.maximumPenetrationCm;
    result.maximumResidualCmS = invariants.maximumResidualCmS;
    result.duplicateContacts = invariants.duplicateContacts;
    result.stepFailures = invariants.stepFailures;
    result.deterministicHash = invariants.deterministicHash;
    result.passed = invariants.passed;
    if (!result.passed && !invariants.failures.empty())
        result.failure = invariants.failures.front();
}

void fail(FullGameCaseResult& result, const char* message)
{
    result.passed = false;
    result.failure = message;
}

bool install(billiardgl::GameRuntime& runtime,
    const billiardgl::GameState& state,
    billiardgl::PhysicsBoundaryMode mode,
    FullGameCaseResult& result)
{
    const billiardgl::ActionResult applied = runtime.replaceStateForScenario(
        state, runtime.physicsProfile(), nullptr, false, mode);
    if (!applied.ok) {
        fail(result, applied.errorCode);
        return false;
    }
    runtime.setPhysicsTraceEnabled(true);
    return true;
}

bool step(billiardgl::GameRuntime& runtime, int ticks,
    FullGameCaseResult& result)
{
    const billiardgl::ActionResult stepped = runtime.step(ticks);
    if (stepped.ok) return true;
    fail(result, stepped.errorCode);
    appendRuntime(result, runtime);
    return false;
}

}  // namespace

FullGameCaseResult runSeededBreak(billiardgl::GameRuntime& runtime,
    std::uint32_t seed, const FullGameRunOptions& options)
{
    FullGameCaseResult result;
    result.caseId = "seeded_break";
    result.seed = seed;
    runtime.reset();
    if (!runtime.step(3).ok) {
        fail(result, "rack_settle_failed");
        return result;
    }
    runtime.setPhysicsTraceEnabled(true);
    XorShift32 random(seed);
    const float yaw = billiardgl::kPi * 0.5f +
        (random.unit() - 0.5f) * 0.015f;
    const float power = 90.0f + random.unit() * 10.0f;
    const billiardgl::ActionResult shot = aimAndShoot(runtime, yaw, power);
    const int breakTicks = std::max(options.ticksPerRepeat, 360);
    if (!shot.ok || !step(runtime, breakTicks, result)) {
        if (!shot.ok) fail(result, shot.errorCode);
        return result;
    }
    appendRuntime(result, runtime);
    finalize(result);
    if (result.ballCollisions < 1 || result.completedShots != 1)
        fail(result, "break_terminal_expectation_failed");
    return result;
}

FullGameCaseResult runContinuousScoring(billiardgl::GameRuntime& runtime,
    std::uint32_t seed, const FullGameRunOptions&)
{
    FullGameCaseResult result;
    result.caseId = "continuous_scoring";
    result.seed = seed;
    result.declaredShots = 3;
    for (int shotIndex = 0; shotIndex < 3; ++shotIndex) {
        billiardgl::GameState state = isolatedState();
        activateBall(state, 0, 39.0f, 0.0f);
        activateBall(state, shotIndex + 1, 54.0f, 0.0f);
        if (!install(runtime, state,
                billiardgl::PhysicsBoundaryMode::ProductionTable, result))
            return result;
        const billiardgl::ActionResult shot = aimAndShoot(runtime, 0.0f, 70.0f);
        if (!shot.ok || !step(runtime, 120, result)) {
            if (!shot.ok) fail(result, shot.errorCode);
            return result;
        }
        appendRuntime(result, runtime);
    }
    finalize(result);
    if (result.objectBallCaptures < 3 || result.scoreEvents < 3)
        fail(result, "continuous_scoring_expectation_failed");
    return result;
}

FullGameCaseResult runCueBallScratch(billiardgl::GameRuntime& runtime,
    std::uint32_t seed, const FullGameRunOptions&)
{
    FullGameCaseResult result;
    result.caseId = "cue_ball_scratch";
    result.seed = seed;
    result.declaredShots = 1;
    billiardgl::GameState state = isolatedState();
    activateBall(state, 0, 43.0f, 0.0f);
    if (!install(runtime, state,
            billiardgl::PhysicsBoundaryMode::ProductionTable, result))
        return result;
    const billiardgl::ActionResult shot = aimAndShoot(runtime, 0.0f, 100.0f);
    if (!shot.ok || !step(runtime, 120, result)) {
        if (!shot.ok) fail(result, shot.errorCode);
        return result;
    }
    appendRuntime(result, runtime);
    finalize(result);
    if (result.cueBallCaptures != 1 || result.foulEvents != 1 ||
        result.turnTransfers != 1)
        fail(result, "scratch_transition_expectation_failed");
    return result;
}

FullGameCaseResult runRandomizedLegalSequence(
    billiardgl::GameRuntime& runtime, std::uint32_t seed,
    const FullGameRunOptions& options)
{
    FullGameCaseResult result;
    result.caseId = "randomized_legal_sequence";
    result.seed = seed;
    result.declaredShots = 3;
    billiardgl::GameState state = isolatedState();
    activateBall(state, 0, 0.0f, 0.0f);
    if (!install(runtime, state, billiardgl::PhysicsBoundaryMode::Unbounded,
            result)) return result;
    XorShift32 random(seed);
    for (int shotIndex = 0; shotIndex < result.declaredShots; ++shotIndex) {
        if (!operationallyLegal(runtime)) {
            ++result.illegalActionAttempts;
            fail(result, "random_action_not_operationally_legal");
            break;
        }
        const float yaw = (random.unit() * 2.0f - 1.0f) * billiardgl::kPi;
        const float power = 20.0f + random.unit() * 20.0f;
        const billiardgl::ActionResult shot = aimAndShoot(runtime, yaw, power);
        if (!shot.ok) {
            ++result.illegalActionAttempts;
            fail(result, shot.errorCode);
            break;
        }
        bool completed = false;
        for (int tick = 0; tick < options.ticksPerRepeat; ++tick) {
            if (!runtime.step(1).ok) {
                fail(result, "random_sequence_step_failed");
                break;
            }
            if (runtime.state().events.shotEnded) {
                completed = true;
                break;
            }
        }
        if (!completed) {
            fail(result, "random_shot_timeout");
            break;
        }
    }
    appendRuntime(result, runtime);
    finalize(result);
    result.gameOver = runtime.state().gameOver;
    if (result.illegalActionAttempts != 0 ||
        (result.completedShots != result.declaredShots && !result.gameOver))
        fail(result, "random_sequence_terminal_expectation_failed");
    return result;
}

}  // namespace fullgame
