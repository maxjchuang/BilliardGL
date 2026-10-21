#include "product_input_controller.h"
#include "render/modern/camera_projection.h"

#include <algorithm>
#include <cmath>
#include <cctype>
#include <limits>
#include <string>
#include <vector>

namespace billiardgl {
namespace {

// Native pointer deltas are logical points, not angular displacements.
// Aim is deliberately finer than orbit; neither depends on frame/event rate.
constexpr double kAimRadiansPerPoint = 0.002;
constexpr double kOrbitRadiansPerPoint = 0.005;
// ArrowKeys contains unit direction steps, whereas OrbitCamera takes radians.
constexpr double kOrbitRadiansPerKeyboardStep = 3.14159265358979323846 / 180.0;

ProductInputResult success(const std::string& action,
    ProductAppRequest request = ProductAppRequest::None)
{
    return {true, true, action, "", request};
}

ProductInputResult failure(const std::string& action,
    const std::string& reason, bool handled = true)
{
    return {false, handled, action, reason, ProductAppRequest::None};
}

bool containsVisibleCharacter(const std::string& value)
{
    return std::any_of(value.begin(), value.end(),
        [](unsigned char character) { return std::isspace(character) == 0; });
}

bool validFloat(double value)
{
    return std::isfinite(value) &&
        std::fabs(value) <= std::numeric_limits<float>::max();
}

bool isShotAuthorityAction(const std::string& action)
{
    return action == "toggle_aim" || action == "adjust_aim" ||
        action == "adjust_tip_offset" ||
        action == "power_increase" || action == "power_decrease" ||
        action == "shoot";
}

settings::MenuCommand settingsCommand(const std::string& action, bool& known)
{
    known = true;
    if (action == "settings_focus_next")
        return settings::MenuCommand::FocusNext;
    if (action == "settings_focus_previous")
        return settings::MenuCommand::FocusPrevious;
    if (action == "settings_category_next")
        return settings::MenuCommand::CategoryNext;
    if (action == "settings_category_previous")
        return settings::MenuCommand::CategoryPrevious;
    if (action == "settings_increase")
        return settings::MenuCommand::Increase;
    if (action == "settings_decrease")
        return settings::MenuCommand::Decrease;
    if (action == "settings_toggle")
        return settings::MenuCommand::Toggle;
    if (action == "settings_apply")
        return settings::MenuCommand::Apply;
    known = false;
    return settings::MenuCommand::Cancel;
}

AssistLevel assistLevel(settings::AssistMode mode)
{
    switch (mode) {
    case settings::AssistMode::Off: return AssistLevel::Off;
    case settings::AssistMode::Basic: return AssistLevel::Basic;
    case settings::AssistMode::Guided: return AssistLevel::Training;
    }
    return AssistLevel::Off;
}

}  // namespace

ProductInputController::ProductInputController(GameRuntime& runtime,
    settings::ProductSettingsRuntime& settingsRuntime,
    training::TrainingProductController* trainingController,
    replay::ReplayTransportModel* replayTransport)
    : runtime_(runtime), settingsRuntime_(settingsRuntime),
      trainingController_(trainingController), replayTransport_(replayTransport)
{
    refreshContexts();
}

void ProductInputController::setSurface(ProductInputSurface surface)
{
    if (surface_ != surface && surface == ProductInputSurface::Replay)
        replayShotOrdinal_ = 0;
    surface_ = surface;
    refreshContexts();
}

void ProductInputController::setTrainingController(
    training::TrainingProductController* controller) noexcept
{
    trainingController_ = controller;
}

void ProductInputController::setReplayTransport(
    replay::ReplayTransportModel* transport) noexcept
{
    replayTransport_ = transport;
    replayShotOrdinal_ = 0;
}

void ProductInputController::refreshContexts()
{
    if (settingsRuntime_.menu().isOpen()) {
        settingsRuntime_.input().setActiveContexts({"settings"});
        return;
    }
    if (surface_ == ProductInputSurface::Replay) {
        settingsRuntime_.input().setActiveContexts({"replay"});
        return;
    }
    const bool aiming = runtime_.state().aim.mode == AimMode::Aim;
    std::vector<std::string> contexts;
    contexts.push_back(aiming ? "aim" : "observe");
    if (surface_ == ProductInputSurface::Training)
        contexts.push_back("training");
    contexts.push_back("play");
    settingsRuntime_.input().setActiveContexts(std::move(contexts));
}

ProductInputResult ProductInputController::process(
    const NormalizedInputEvent& event)
{
    if (!containsVisibleCharacter(event.device) ||
        !containsVisibleCharacter(event.control) ||
        !containsVisibleCharacter(event.trigger))
        return failure("", "input_invalid_identity", false);
    if (!std::isfinite(event.value) || !std::isfinite(event.secondaryValue))
        return failure("", "input_nonfinite_value", false);

    if (settingsRuntime_.menu().isOpen() &&
        settingsRuntime_.menu().waitingForBinding()) {
        const settings::MenuResult captured = settingsRuntime_.captureRebind(
            event.device, event.control, event.trigger);
        return captured.ok ? success("rebind_capture") :
            failure("rebind_capture", captured.errorCode);
    }

    refreshContexts();
    const settings::BindingResolution resolved = settingsRuntime_.input().resolve(
        event.device, event.control, event.trigger);
    if (!resolved.matched) {
        return settingsRuntime_.menu().isOpen() ?
            failure("", "input_settings_modal") :
            failure("", "input_unbound", false);
    }
    return execute(resolved.action, event);
}

ProductInputResult ProductInputController::beginRebind(
    const std::string& action)
{
    if (!settingsRuntime_.menu().isOpen())
        return failure("begin_rebind", "input_settings_not_open");
    const settings::MenuResult result = settingsRuntime_.beginRebind(action);
    return result.ok ? success("begin_rebind") :
        failure("begin_rebind", result.errorCode);
}

bool ProductInputController::syncAssist() noexcept
{
    return runtime_.setAssistLevel(
        assistLevel(settingsRuntime_.current().assist)).ok;
}

ProductInputResult ProductInputController::execute(
    const std::string& action, const NormalizedInputEvent& event)
{
    if (settingsRuntime_.menu().isOpen()) {
        if (action == "quit" || action == "open_settings") {
            const settings::MenuResult result = settingsRuntime_.handleMenu(
                settings::MenuCommand::Cancel);
            refreshContexts();
            return result.ok ? success(action) : failure(action, result.errorCode);
        }
        if (action == "toggle_help") {
            GameAction game;
            game.type = ActionType::ToggleHelp;
            return dispatchGame(action, game);
        }
        return executeSettings(action);
    }

    if (action == "open_settings") {
        settingsRuntime_.openMenu();
        refreshContexts();
        return success(action);
    }
    if (action == "quit") return success(action, ProductAppRequest::Quit);
    if (action == "toggle_help") {
        GameAction game;
        game.type = ActionType::ToggleHelp;
        return dispatchGame(action, game);
    }
    if (surface_ == ProductInputSurface::Training && trainingController_ &&
        isShotAuthorityAction(action) && action != "toggle_aim") {
        const auto state = trainingController_->snapshot();
        if (state.state == training::TrainingProductState::AttemptEvaluated && action != "shoot")
            return success("training_control_locked");
        if (state.state == training::TrainingProductState::AttemptActive) {
            const auto* card = trainingController_->currentCard();
            if (card && ((action == "adjust_tip_offset" && !card->tipAdjustmentAllowed) ||
                (action == "adjust_aim" && !card->aimAdjustmentAllowed) ||
                ((action == "power_increase" || action == "power_decrease") && !card->powerAdjustmentAllowed)))
                return success("training_control_locked");
        }
    }
    if (isShotAuthorityAction(action)) {
        if (aiTurnActive_) return failure(action, "input_ai_turn_active");
        if (runtime_.state().ballsMoving)
            return failure(action, "input_balls_moving");
        if (!syncAssist()) return failure(action, "input_assist_sync_failed");
    }

    GameAction game;
    if (action == "toggle_aim") {
        game.type = ActionType::ToggleAim;
        return dispatchGame(action, game);
    }
    if (action == "adjust_aim") {
        const double yaw = runtime_.shotControl().yawRadians + event.value *
            (event.device == "mouse" ? kAimRadiansPerPoint : 1.0) *
            settingsRuntime_.current().mouseSensitivity;
        if (!validFloat(yaw)) return failure(action, "input_value_out_of_range");
        game.type = ActionType::SetAimYaw;
        game.first = static_cast<float>(yaw);
        return dispatchGame(action, game);
    }
    if (action == "adjust_tip_offset") {
        constexpr double kTipStepRadius = 0.05;
        double horizontal = event.value;
        double vertical = event.secondaryValue;
        if (surface_ == ProductInputSurface::Training && trainingController_) {
            if (const auto* card = trainingController_->currentCard()) {
                if (card->tipAdjustmentAxis == "vertical") horizontal = 0.0;
                if (card->tipAdjustmentAxis == "horizontal") vertical = 0.0;
                if (horizontal == 0.0 && vertical == 0.0)
                    return success("training_control_locked");
            }
        }
        double x = static_cast<double>(runtime_.shotControl().tipOffsetX) +
            horizontal * kTipStepRadius;
        double y = static_cast<double>(runtime_.shotControl().tipOffsetY) +
            vertical * kTipStepRadius;
        if (!validFloat(x) || !validFloat(y))
            return failure(action, "input_value_out_of_range");
        const double maximum =
            runtime_.physicsProfile().cue.maximumReliableOffsetRadius;
        const double magnitude = std::hypot(x, y);
        if (magnitude > maximum && magnitude > 0.0) {
            x *= maximum / magnitude;
            y *= maximum / magnitude;
        }
        const ActionResult result = runtime_.setTipOffset(
            static_cast<float>(x), static_cast<float>(y));
        refreshContexts();
        return result.ok ? success(action) :
            failure(action, result.errorCode);
    }
    if (action == "power_increase" || action == "power_decrease") {
        const double steps = event.value == 0.0 ? 1.0 : std::fabs(event.value);
        const double direction = action == "power_increase" ? 1.0 : -1.0;
        const double power = std::clamp(
            static_cast<double>(runtime_.shotControl().requestedPower) +
                direction * steps * 5.0,
            0.0, 200.0);
        if (!validFloat(power)) return failure(action, "input_value_out_of_range");
        game.type = ActionType::SetShotPower;
        game.first = static_cast<float>(power);
        return dispatchGame(action, game);
    }
    if (action == "shoot") {
        if (surface_ == ProductInputSurface::Training && trainingController_ &&
            trainingController_->snapshot().state ==
                training::TrainingProductState::AttemptEvaluated)
            return success("training_attempt_finished");
        game.type = ActionType::Shoot;
        if (surface_ != ProductInputSurface::Training ||
            trainingController_ == nullptr)
            return dispatchGame(action, game);
        const training::TrainingProductResult prepared =
            trainingController_->markShotStart();
        if (!prepared.ok) return failure(action, prepared.errorCode);
        ProductInputResult dispatched = dispatchGame(action, game);
        if (!dispatched.ok) {
            trainingController_->cancelShotStart();
            return dispatched;
        }
        const training::TrainingProductResult confirmed =
            trainingController_->confirmShotDispatched();
        return confirmed.ok ? dispatched : failure(action, confirmed.errorCode);
    }
    if (action == "camera_orbit") {
        const double sensitivity = settingsRuntime_.current().mouseSensitivity *
            (event.device == "mouse" ? kOrbitRadiansPerPoint :
                kOrbitRadiansPerKeyboardStep);
        const double yaw = event.value * sensitivity;
        const double pitch = event.secondaryValue * sensitivity *
            (settingsRuntime_.current().invertMouseY ? -1.0 : 1.0);
        if (!validFloat(yaw) || !validFloat(pitch))
            return failure(action, "input_value_out_of_range");
        if (surface_ == ProductInputSurface::Replay) {
            if (!replayTransport_) return failure(action, "input_replay_unavailable");
            const auto result = replayTransport_->adjustCamera(yaw, pitch, 0.0f);
            return result.ok ? success(action) : failure(action, result.errorCode);
        }
        game.type = ActionType::OrbitCamera;
        game.first = static_cast<float>(yaw);
        game.second = static_cast<float>(pitch);
        return dispatchGame(action, game);
    }
    if (action == "camera_pan") {
        if (!validFloat(event.value) || !validFloat(event.secondaryValue))
            return failure(action, "input_value_out_of_range");
        game.type = ActionType::PanCamera;
        game.first = static_cast<float>(event.value);
        game.second = static_cast<float>(event.secondaryValue);
        if (event.device == "mouse") {
            const auto& pose = runtime_.cameraFrame().pose;
            CameraPresentation camera;
            camera.eye = {pose.eye.x, pose.eye.y, pose.eye.z};
            camera.target = {pose.target.x, pose.target.y, pose.target.z};
            const auto view = render::sharedLookAt(camera);
            // Invert the screen-right/up projection of the table's X/Z plane.
            // Use the renderer's basis, including its near-overhead stable up.
            const double determinant = view[0] * view[9] - view[8] * view[1];
            if (!std::isfinite(determinant) || std::fabs(determinant) < 1e-4)
                return success(action); // Edge-on plane has no stable inverse.
            const double scale = 2.0 * pose.distanceCm * std::tan(
                std::clamp(static_cast<double>(pose.fieldOfViewDegrees), 20.0, 90.0) *
                3.141592653589793 / 360.0) / logicalViewportHeight_ *
                settingsRuntime_.current().mouseSensitivity;
            // Camera moves opposite the grabbed image; native Y grows down.
            const double right = -event.value * scale;
            const double up = event.secondaryValue * scale;
            const double x = (right * view[9] - view[8] * up) / determinant;
            const double z = (view[0] * up - right * view[1]) / determinant;
            if (!validFloat(x) || !validFloat(z))
                return failure(action, "input_value_out_of_range");
            game.first = static_cast<float>(x);
            game.second = static_cast<float>(z);
        }
        return dispatchGame(action, game);
    }
    if (action == "zoom_in" || action == "zoom_out") {
        const double steps = event.value == 0.0 ? 1.0 : std::fabs(event.value);
        const double delta = (action == "zoom_in" ? -10.0 : 10.0) * steps;
        if (!validFloat(delta)) return failure(action, "input_value_out_of_range");
        if (surface_ == ProductInputSurface::Replay) {
            if (!replayTransport_) return failure(action, "input_replay_unavailable");
            const auto result = replayTransport_->adjustCamera(0.0f, 0.0f, delta);
            return result.ok ? success(action) : failure(action, result.errorCode);
        }
        game.type = ActionType::ZoomCamera;
        game.first = static_cast<float>(delta);
        return dispatchGame(action, game);
    }
    if (action == "toggle_camera_anchor") {
        game.type = ActionType::ToggleCameraAnchor;
        return dispatchGame(action, game);
    }
    if (action == "return_to_cue_ball") {
        game.type = ActionType::ReturnToCueBall;
        return dispatchGame(action, game);
    }
    if (action == "reset_training_attempt") {
        if (trainingController_ == nullptr)
            return failure(action, "input_training_unavailable");
        const training::TrainingProductResult result =
            trainingController_->resetAttempt();
        return result.ok ? success(action) : failure(action, result.errorCode);
    }
    if (action.rfind("replay_", 0) == 0) return executeReplay(action);
    return failure(action, "input_action_unsupported");
}

ProductInputResult ProductInputController::executeSettings(
    const std::string& action)
{
    bool known = false;
    const settings::MenuCommand command = settingsCommand(action, known);
    if (!known) return failure(action, "input_settings_action_unsupported");
    const settings::MenuResult result = settingsRuntime_.handleMenu(command);
    if (result.ok && command == settings::MenuCommand::Apply &&
        !runtime_.state().ballsMoving)
        (void)syncAssist();
    refreshContexts();
    return result.ok ? success(action) : failure(action, result.errorCode);
}

ProductInputResult ProductInputController::executeReplay(
    const std::string& action)
{
    if (replayTransport_ == nullptr)
        return failure(action, "input_replay_unavailable");
    replay::ValidationResult result;
    if (action == "replay_toggle_pause") {
        result = replayTransport_->setPaused(
            !replayTransport_->snapshot().paused);
    } else if (action == "replay_step") {
        result = replayTransport_->stepTick();
    } else if (action == "replay_rate_increase" ||
            action == "replay_rate_decrease") {
        constexpr double rates[] = {0.25, 0.5, 1.0, 2.0};
        const double current = replayTransport_->snapshot().playbackRate;
        std::size_t index = 0;
        while (index + 1 < 4 && rates[index] < current) ++index;
        if (action == "replay_rate_increase") index = std::min<std::size_t>(3, index + 1);
        else if (index > 0) --index;
        result = replayTransport_->setPlaybackRate(rates[index]);
    } else if (action == "replay_camera_toggle") {
        const replay::ReplayCameraMode current =
            replayTransport_->snapshot().cameraMode;
        result = replayTransport_->setCameraMode(
            current == replay::ReplayCameraMode::Free ?
                replay::ReplayCameraMode::Broadcast :
                replay::ReplayCameraMode::Free);
    } else if (action == "replay_seek_previous") {
        const std::size_t candidate = replayShotOrdinal_ == 0 ? 0 :
            replayShotOrdinal_ - 1;
        result = replayTransport_->seekShot(candidate);
        if (result.ok) replayShotOrdinal_ = candidate;
    } else if (action == "replay_seek_next") {
        const std::size_t candidate = replayShotOrdinal_ + 1;
        result = replayTransport_->seekShot(candidate);
        if (result.ok) replayShotOrdinal_ = candidate;
    } else {
        return failure(action, "input_replay_action_unsupported");
    }
    return result.ok ? success(action) : failure(action, result.errorCode);
}

ProductInputResult ProductInputController::dispatchGame(
    const std::string& action, const GameAction& gameAction)
{
    const ActionResult result = runtime_.dispatch(gameAction);
    refreshContexts();
    return result.ok ? success(action) : failure(action, result.errorCode);
}

}  // namespace billiardgl
