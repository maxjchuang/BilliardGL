#include "full_game_component_cases.h"

#include "full_game_invariants.h"

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

billiardgl::ActionResult dispatchValue(
    billiardgl::GameRuntime& runtime, billiardgl::ActionType type, float value)
{
    billiardgl::GameAction action;
    action.type = type;
    action.first = value;
    return runtime.dispatch(action);
}

billiardgl::ActionResult aimAndShoot(
    billiardgl::GameRuntime& runtime, float yawRadians, float power)
{
    billiardgl::ActionResult result = dispatchValue(
        runtime, billiardgl::ActionType::SetAimYaw, yawRadians);
    if (!result.ok) return result;
    result = dispatchValue(
        runtime, billiardgl::ActionType::SetShotPower, power);
    if (!result.ok) return result;
    billiardgl::GameAction shoot;
    shoot.type = billiardgl::ActionType::Shoot;
    return runtime.dispatch(shoot);
}

FullGameCaseResult collectResult(billiardgl::GameRuntime& runtime,
    const std::string& caseId, std::uint32_t seed,
    const std::vector<int>& objectBalls)
{
    FullGameCaseResult result;
    result.caseId = caseId;
    result.seed = seed;
    result.droppedTraceFrames = runtime.physicsTrace().droppedFrames();
    result.frames.assign(runtime.physicsTrace().frames().begin(),
        runtime.physicsTrace().frames().end());
    result.events = runtime.events();
    for (const billiardgl::PhysicsFrame& frame : result.frames) {
        result.surfaceTransitions += static_cast<int>(
            frame.surfaceTransitions.size());
    }
    for (const billiardgl::RuntimeEvent& event : result.events) {
        if (event.name == "ball_collision") ++result.ballCollisions;
        else if (event.name == "rail_collision") ++result.railCollisions;
        else if (event.name == "cue_ball_pocketed") ++result.cueBallCaptures;
    }
    for (const int index : objectBalls) {
        if (runtime.state().balls[index].pocketed)
            ++result.objectBallCaptures;
    }
    if (runtime.hasCueContactResult()) {
        result.cueContactApplied = runtime.cueContactResult().applied;
        result.cueContactMiscue = runtime.cueContactResult().regime ==
            billiardgl::CueContactRegime::Miscue;
    }
    const FullGameInvariantResult invariants = evaluateFullGameInvariants(
        result.frames, result.events, result.droppedTraceFrames, 0.0, 0);
    result.passed = invariants.passed;
    result.maximumPenetrationCm = invariants.maximumPenetrationCm;
    result.maximumResidualCmS = invariants.maximumResidualCmS;
    result.duplicateContacts = invariants.duplicateContacts;
    result.stepFailures = invariants.stepFailures;
    result.deterministicHash = invariants.deterministicHash;
    if (!result.passed && !invariants.failures.empty())
        result.failure = invariants.failures.front();
    return result;
}

bool prepare(billiardgl::GameRuntime& runtime,
    const billiardgl::GameState& state,
    billiardgl::PhysicsBoundaryMode boundaryMode,
    FullGameCaseResult& failure,
    const std::string& caseId,
    std::uint32_t seed)
{
    const billiardgl::ActionResult applied = runtime.replaceStateForScenario(
        state, runtime.physicsProfile(), nullptr, false, boundaryMode);
    if (!applied.ok) {
        failure.caseId = caseId;
        failure.seed = seed;
        failure.failure = applied.errorCode;
        return false;
    }
    runtime.setPhysicsTraceEnabled(true);
    return true;
}

bool advance(billiardgl::GameRuntime& runtime, int ticks,
    FullGameCaseResult& failure)
{
    const billiardgl::ActionResult stepped = runtime.step(ticks);
    if (stepped.ok) return true;
    failure.failure = stepped.errorCode;
    failure.frames.assign(runtime.physicsTrace().frames().begin(),
        runtime.physicsTrace().frames().end());
    failure.events = runtime.events();
    failure.stepFailures = 1;
    return false;
}

FullGameCaseResult runControlledShot(const std::string& caseId,
    billiardgl::GameRuntime& runtime, std::uint32_t seed,
    const FullGameRunOptions& options, const billiardgl::GameState& state,
    billiardgl::PhysicsBoundaryMode boundaryMode, float yaw, float power,
    const std::vector<int>& objectBalls)
{
    FullGameCaseResult failure;
    if (!prepare(runtime, state, boundaryMode, failure, caseId, seed))
        return failure;
    const billiardgl::ActionResult shot = aimAndShoot(runtime, yaw, power);
    if (!shot.ok) {
        failure.failure = shot.errorCode;
        return failure;
    }
    if (!advance(runtime, options.ticksPerRepeat, failure)) return failure;
    return collectResult(runtime, caseId, seed, objectBalls);
}

void requireObservation(FullGameCaseResult& result, bool observed,
    const char* failure)
{
    if (!observed) {
        result.passed = false;
        result.failure = failure;
    }
}

}  // namespace

FullGameCaseResult runCueCenterHit(billiardgl::GameRuntime& runtime,
    std::uint32_t seed, const FullGameRunOptions& options)
{
    billiardgl::GameState state = isolatedState();
    activateBall(state, 0, -20.0f, 0.0f);
    FullGameCaseResult result = runControlledShot("cue_center_hit", runtime,
        seed, options, state, billiardgl::PhysicsBoundaryMode::Unbounded,
        0.0f, 30.0f, {});
    requireObservation(result, result.cueContactApplied,
        "center_cue_contact_not_applied");
    return result;
}

FullGameCaseResult runCueNearMiscue(billiardgl::GameRuntime& runtime,
    std::uint32_t seed, const FullGameRunOptions& options)
{
    const std::string caseId = "cue_near_miscue";
    billiardgl::GameState state = isolatedState();
    activateBall(state, 0, -20.0f, 0.0f);
    FullGameCaseResult failure;
    if (!prepare(runtime, state, billiardgl::PhysicsBoundaryMode::Unbounded,
            failure, caseId, seed)) return failure;
    billiardgl::CueImpactInput cue;
    cue.cueBallIndex = 0;
    cue.cueSpeedCmS = 60.0;
    cue.cueMassKg = runtime.physicsProfile().cue.effectiveMassKg;
    cue.direction = {{1.0, 0.0, 0.0}};
    cue.tipOffsetRadius = {{0.79, 0.0}};
    cue.tipOffsetCm = {{
        0.79 * runtime.physicsProfile().ball.radiusCm, 0.0}};
    cue.chalkState = "CHALKED";
    const billiardgl::ActionResult shot = runtime.applyCueImpact(cue);
    if (!shot.ok) {
        failure.failure = shot.errorCode;
        return failure;
    }
    if (!advance(runtime, options.ticksPerRepeat, failure)) return failure;
    FullGameCaseResult result = collectResult(runtime, caseId, seed, {});
    requireObservation(result,
        result.cueContactApplied && !result.cueContactMiscue,
        "near_miscue_outside_reliable_envelope");
    return result;
}

FullGameCaseResult runSlidingToRolling(billiardgl::GameRuntime& runtime,
    std::uint32_t seed, const FullGameRunOptions& options)
{
    billiardgl::GameState state = isolatedState();
    activateBall(state, 0, -20.0f, 0.0f);
    FullGameCaseResult result = runControlledShot("sliding_to_rolling", runtime,
        seed, options, state, billiardgl::PhysicsBoundaryMode::Unbounded,
        0.0f, 40.0f, {});
    requireObservation(result, result.surfaceTransitions >= 1,
        "sliding_to_rolling_transition_missing");
    return result;
}

FullGameCaseResult runObliqueBallCollision(billiardgl::GameRuntime& runtime,
    std::uint32_t seed, const FullGameRunOptions& options)
{
    billiardgl::GameState state = isolatedState();
    activateBall(state, 0, -18.0f, -3.0f);
    activateBall(state, 1, 0.0f, 0.0f);
    FullGameCaseResult result = runControlledShot("oblique_ball_collision",
        runtime, seed, options, state,
        billiardgl::PhysicsBoundaryMode::Unbounded,
        0.0f, 55.0f, {1});
    requireObservation(result, result.ballCollisions == 1,
        "oblique_ball_collision_missing");
    return result;
}

FullGameCaseResult runRailRebound(billiardgl::GameRuntime& runtime,
    std::uint32_t seed, const FullGameRunOptions& options)
{
    billiardgl::GameState state = isolatedState();
    activateBall(state, 0, 42.0f, 25.0f);
    FullGameCaseResult result = runControlledShot("rail_rebound", runtime,
        seed, options, state, billiardgl::PhysicsBoundaryMode::ProductionTable,
        0.0f, 55.0f, {});
    requireObservation(result, result.railCollisions >= 1,
        "rail_rebound_missing");
    return result;
}

FullGameCaseResult runSidePocketCapture(billiardgl::GameRuntime& runtime,
    std::uint32_t seed, const FullGameRunOptions& options)
{
    billiardgl::GameState state = isolatedState();
    activateBall(state, 0, 28.0f, 0.0f);
    activateBall(state, 1, 43.0f, 0.0f);
    FullGameCaseResult result = runControlledShot("side_pocket_capture", runtime,
        seed, options, state, billiardgl::PhysicsBoundaryMode::ProductionTable,
        0.0f, 180.0f, {1});
    requireObservation(result, result.objectBallCaptures == 1,
        "side_pocket_capture_missing");
    return result;
}

}  // namespace fullgame
