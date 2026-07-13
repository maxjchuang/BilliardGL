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
    case ActionType::KeyUp:
        return ActionResult{};
    case ActionType::KeyDown: {
        const unsigned char key = static_cast<unsigned char>(action.firstInt);
        if (key == 'h' || key == 'H') handleHelpKey(state_);
        else if (key == '\t') handleAimToggleKey(state_);
        else if (key == 'c' || key == 'C') handleCameraAnchorToggleKey(state_);
        else if (key == ' ') handleCameraReturnToCueBallKey(state_);
        else if (key == 'w' || key == 'W') state_.camera.zoom = std::fmax(10.0f, state_.camera.zoom - 10.0f);
        else if (key == 's' || key == 'S') state_.camera.zoom = std::fmin(500.0f, state_.camera.zoom + 10.0f);
        else if (key == 'a' || key == 'A') state_.camera.panX = std::fmin(490.0f, state_.camera.panX + 10.0f);
        else if (key == 'd' || key == 'D') state_.camera.panX = std::fmax(-490.0f, state_.camera.panX - 10.0f);
        else if ((key == '+' || key == '-') && state_.aim.mode == AimMode::Aim) handleMouseWheel(state_, key == '+' ? 1 : -1, 10.0f, 20.0f, 200.0f);
        else return ActionResult{false, "unknown_key"};
        return ActionResult{};
    }
    case ActionType::SpecialKey:
        handleSpecialKey(state_, 0, 1, 2, 3, action.firstInt); return ActionResult{};
    case ActionType::MouseMove:
        if (!state_.ballsMoving) handleMouseMove(state_, action.firstInt, action.secondInt); return ActionResult{};
    case ActionType::MouseWheel:
        if (!state_.ballsMoving) handleMouseWheel(state_, action.firstInt, 10.0f, 20.0f, 200.0f); return ActionResult{};
    case ActionType::MouseButton: {
        if (state_.ballsMoving) return ActionResult{false, "invalid_state"};
        const MouseButton button = action.firstInt == 0 ? MouseButton::Left : action.firstInt == 1 ? MouseButton::Right : MouseButton::Other;
        handleMouseButton(state_, button, action.secondInt == 0 ? ButtonState::Down : ButtonState::Up, action.thirdInt, static_cast<int>(action.first));
        if (state_.input.hitRequested) applyShot();
        return ActionResult{};
    }
    case ActionType::Resize:
        if (action.firstInt <= 0 || action.secondInt <= 0) return ActionResult{false, "invalid_argument"};
        state_.config.width = action.firstInt; state_.config.height = action.secondInt; return ActionResult{};
    case ActionType::OrbitCamera:
        state_.camera.angleX += action.first; state_.camera.angleY += action.second; clampCameraAngles(state_); return ActionResult{};
    case ActionType::PanCamera:
        state_.camera.anchorMode = CameraAnchorMode::FreeLook; state_.camera.target[0] += action.first; state_.camera.target[2] += action.second; return ActionResult{};
    case ActionType::ZoomCamera:
        state_.camera.zoom = std::fmax(10.0f, std::fmin(500.0f, state_.camera.zoom + action.first)); return ActionResult{};
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
