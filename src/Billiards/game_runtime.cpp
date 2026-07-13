#include "game_runtime.h"

#include "input.h"
#include "physics.h"
#include "rules.h"
#include "shot.h"

#include <cmath>

namespace billiardgl {

GameRuntime::GameRuntime()
{
    reset();
}

void GameRuntime::reset()
{
    state_ = GameState{};
    initializeBalls(state_);
    tick_ = 0;
    nextSequence_ = 1;
    events_.clear();
    updateCameraFromCueBall(state_);
}

ActionResult GameRuntime::dispatch(const GameAction& action)
{
    switch (action.type) {
    case ActionType::SetAimYaw:
        if (!std::isfinite(action.first)) return ActionResult{false, "invalid_argument"};
        state_.aim.yaw = action.first;
        return ActionResult{};
    case ActionType::SetShotPower:
        if (!std::isfinite(action.first) || action.first < 0.0f || action.first > 200.0f)
            return ActionResult{false, "invalid_argument"};
        state_.input.shotPower = action.first;
        return ActionResult{};
    case ActionType::ToggleAim:
        handleAimToggleKey(state_);
        return ActionResult{};
    case ActionType::ToggleHelp:
        handleHelpKey(state_);
        return ActionResult{};
    case ActionType::Shoot:
        if (state_.ballsMoving) return ActionResult{false, "invalid_state"};
        applyShot();
        return ActionResult{};
    default:
        return ActionResult{false, "unsupported_action"};
    }
}

void GameRuntime::applyShot()
{
    const Point3 velocity = shotVelocityFromAim(state_.aim.yaw, state_.input.shotPower);
    setBallVelocity(state_.balls[0], velocity.x, velocity.y, velocity.z);
    state_.players.nextPlayer = 1 - state_.players.currentPlayer;
    state_.players.illegalShot = false;
    state_.players.shotTaken = true;
    state_.players.updatedAfterShot = false;
    state_.ballsMoving = true;
    state_.camera.anchorMode = CameraAnchorMode::FreeLook;
    state_.transitionPerspective = false;
    state_.perspectiveRecorded = false;
    state_.aim.mode = AimMode::Observe;
    state_.players.aimingAtCueBall = false;
    state_.input.hitRequested = false;
}

ActionResult GameRuntime::step(int count)
{
    if (count < 0) return ActionResult{false, "invalid_argument"};
    for (int i = 0; i < count; ++i) {
        updatePhysics(state_, kDefaultTimeStep);
        ++tick_;
        recordEvents();
        if (state_.events.shotEnded || (!state_.transitionPerspective && state_.players.shotTaken))
            updatePlayerAfterShot(state_);
        if (state_.camera.anchorMode == CameraAnchorMode::FollowCueBall)
            updateCameraFromCueBall(state_);
    }
    return ActionResult{};
}

void GameRuntime::recordEvents()
{
    const struct NamedFlag { const char* name; bool value; } flags[] = {
        {"ball_collision", state_.events.ballCollision},
        {"rail_collision", state_.events.railCollision},
        {"ball_pocketed", state_.events.ballPocketed},
        {"cue_ball_pocketed", state_.events.cueBallPocketed},
        {"eight_ball_pocketed", state_.events.eightBallPocketed},
        {"shot_ended", state_.events.shotEnded}
    };
    for (const NamedFlag& flag : flags) {
        if (flag.value) events_.push_back(RuntimeEvent{nextSequence_++, tick_, flag.name});
    }
}

std::vector<RuntimeEvent> GameRuntime::eventsSince(std::uint64_t sequence) const
{
    std::vector<RuntimeEvent> result;
    for (const RuntimeEvent& event : events_) {
        if (event.sequence > sequence) result.push_back(event);
    }
    return result;
}

}  // namespace billiardgl
