#include "game_runtime.h"

#include "input.h"
#include "physics.h"
#include "rules.h"
#include "shot.h"

#include <cmath>

namespace billiardgl {

namespace {

const char* stableContactError(const std::string& error)
{
    if (error == "invalid_cue_contact_input") return "invalid_cue_contact_input";
    if (error == "cue_elevation_requires_3d") return "cue_elevation_requires_3d";
    if (error == "invalid_cue_direction") return "invalid_cue_direction";
    if (error == "cue_offset_outside_ball") return "cue_offset_outside_ball";
    if (error == "miscue_offset_exceeds_reliable_radius")
        return "miscue_offset_exceeds_reliable_radius";
    if (error == "cue_not_approaching") return "cue_not_approaching";
    if (error == "cue_not_approaching_contact_normal")
        return "cue_not_approaching_contact_normal";
    if (error == "vertical_ball_impulse_requires_3d")
        return "vertical_ball_impulse_requires_3d";
    return "cue_contact_failed";
}

}  // namespace

CueShotApplication applyCueShot(GameState& state, const CueImpactInput& input,
    const PhysicsProfile& profile)
{
    CueShotApplication application;
    if (input.cueBallIndex != 0) {
        application.action = ActionResult{false, "cue_ball_index_not_supported"};
        application.contact.error = "cue_ball_index_not_supported";
        return application;
    }
    if (state.balls[0].pocketed) {
        application.action = ActionResult{false, "cue_ball_not_in_play"};
        application.contact.error = "cue_ball_not_in_play";
        return application;
    }

    application.contact = resolveCueContact(
        state.balls[0], input, profile.ball, profile.cue);
    if (!application.contact.applied) {
        application.action = ActionResult{
            false, stableContactError(application.contact.error)};
        return application;
    }

    state.players.nextPlayer = 1 - state.players.currentPlayer;
    state.players.illegalShot = false;
    state.players.shotTaken = true;
    state.players.updatedAfterShot = false;
    state.ballsMoving = true;
    state.camera.anchorMode = CameraAnchorMode::FreeLook;
    state.transitionPerspective = false;
    state.perspectiveRecorded = false;
    state.aim.mode = AimMode::Observe;
    state.players.aimingAtCueBall = false;
    state.input.hitRequested = false;
    return application;
}

CueImpactSupport evaluateCueImpactSupport(
    const CueImpactInput& input, const CueContactResult* result)
{
    CueImpactSupport support;
    if (result) {
        support.shotExecuted = result->applied;
        if (result->applied) {
            support.exactlyConsumableFields.push_back("cue_ball_index");
            support.exactlyConsumableFields.push_back("cue_speed_cm_s");
            support.exactlyConsumableFields.push_back("cue_mass_kg");
            support.exactlyConsumableFields.push_back("horizontal_direction");
            support.exactlyConsumableFields.push_back("horizontal_tip_offset");
            support.exactlyConsumableFields.push_back("vertical_tip_offset");
            support.exactlyConsumableFields.push_back("chalk_state");
        }
        if (!result->applied && !result->error.empty())
            support.unsupportedCodes.push_back(result->error);
        return support;
    }
    if (input.cueBallIndex == 0) support.exactlyConsumableFields.push_back("cue_ball_index");
    else support.unsupportedCodes.push_back("cue_ball_index_not_supported");
    if (std::fabs(input.direction[1]) <= 0.000001 &&
        std::fabs(input.elevationDegrees) <= 0.000001) {
        support.exactlyConsumableFields.push_back("horizontal_direction");
    } else {
        support.unsupportedCodes.push_back("cue_elevation_not_modeled");
    }
    support.unsupportedCodes.push_back("cue_speed_to_power_mapping_missing");
    support.unsupportedCodes.push_back("cue_mass_not_modeled");
    if (std::fabs(input.tipOffsetCm[0]) > 0.000001)
        support.unsupportedCodes.push_back("horizontal_tip_offset_not_modeled");
    if (std::fabs(input.tipOffsetCm[1]) > 0.000001)
        support.unsupportedCodes.push_back("vertical_tip_offset_not_modeled");
    support.unsupportedCodes.push_back("chalk_state_not_modeled");
    return support;
}

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
    physicsTraceEnabled_ = false;
    physicsTrace_.clear();
    hasCueImpactInput_ = false;
    cueImpactInput_ = CueImpactInput{};
    hasCueContactResult_ = false;
    cueContactResult_ = CueContactResult{};
    cueContactPending_ = false;
    physicsProfile_ = defaultChinesePoolPhysicsProfile();
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
        if (state_.input.hitRequested) return applyShot();
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
        return applyShot();
    default:
        return ActionResult{false, "unsupported_action"};
    }
}

ActionResult GameRuntime::applyShot()
{
    return applyCueImpact(cueImpactFromShotControls(
        state_.aim.yaw, state_.input.shotPower, physicsProfile_));
}

ActionResult GameRuntime::step(int count)
{
    if (count < 0) return ActionResult{false, "invalid_argument"};
    for (int i = 0; i < count; ++i) {
        const GameState before = state_;
        const float timeStep = physicsProfile_.solver.timeStepSeconds;
        const PhysicsStepTelemetry telemetry = updatePhysics(state_, timeStep, physicsProfile_);
        ++tick_;
        if (physicsTraceEnabled_) {
            PhysicsFrame frame = capturePhysicsFrame(
                tick_, static_cast<double>(tick_) * timeStep,
                timeStep, before, state_, telemetry, physicsProfile_);
            frame.hasCueImpactInput = hasCueImpactInput_;
            frame.cueImpactInput = cueImpactInput_;
            frame.hasCueContactResult = cueContactPending_;
            frame.cueContactResult = cueContactResult_;
            physicsTrace_.append(frame);
            cueContactPending_ = false;
        }
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
    if (events_.size() > 10000) events_.erase(events_.begin(), events_.begin() + (events_.size() - 10000));
}

std::vector<RuntimeEvent> GameRuntime::eventsSince(std::uint64_t sequence) const
{
    std::vector<RuntimeEvent> result;
    for (const RuntimeEvent& event : events_) {
        if (event.sequence > sequence) result.push_back(event);
    }
    return result;
}

ActionResult GameRuntime::setBall(int index, const BallState& ball)
{
    if (index < 0 || index >= kBallCount) return ActionResult{false, "invalid_argument"};
    state_.balls[index] = ball;
    state_.ballsMoving = anyBallMoving(state_);
    return ActionResult{};
}

ActionResult GameRuntime::applyCueImpact(const CueImpactInput& input)
{
    if (state_.ballsMoving) return ActionResult{false, "invalid_state"};
    GameState candidate = state_;
    const CueShotApplication application = applyCueShot(candidate, input, physicsProfile_);
    hasCueImpactInput_ = true;
    cueImpactInput_ = input;
    hasCueContactResult_ = true;
    cueContactResult_ = application.contact;
    cueContactPending_ = true;
    if (application.action.ok) state_ = candidate;
    return application.action;
}

void GameRuntime::replaceState(const GameState& state)
{
    state_ = state;
    state_.ballsMoving = anyBallMoving(state_);
}

ActionResult GameRuntime::replaceStateForScenario(
    const GameState& state, const CueImpactInput* cueImpact)
{
    return replaceStateForScenario(
        state, defaultChinesePoolPhysicsProfile(), cueImpact);
}

ActionResult GameRuntime::replaceStateForScenario(
    const GameState& state, const PhysicsProfile& profile,
    const CueImpactInput* cueImpact, bool executeCueImpact)
{
    const PhysicsProfileValidation validation = validatePhysicsProfile(profile);
    if (!validation.ok) {
        return ActionResult{false, "invalid_physics_profile"};
    }
    GameState candidate = state;
    CueShotApplication application;
    if (cueImpact && executeCueImpact) {
        application = applyCueShot(candidate, *cueImpact, profile);
        if (!application.action.ok) return application.action;
    }
    replaceState(candidate);
    physicsProfile_ = profile;
    tick_ = 0;
    nextSequence_ = 1;
    events_.clear();
    clearGameplayEvents(state_);
    physicsTrace_.clear();
    hasCueImpactInput_ = cueImpact != nullptr;
    cueImpactInput_ = cueImpact ? *cueImpact : CueImpactInput{};
    hasCueContactResult_ = cueImpact && executeCueImpact;
    cueContactResult_ = hasCueContactResult_ ?
        application.contact : CueContactResult{};
    cueContactPending_ = hasCueContactResult_;
    return ActionResult{};
}

void GameRuntime::clearEvents()
{
    events_.clear();
    clearGameplayEvents(state_);
}

}  // namespace billiardgl
