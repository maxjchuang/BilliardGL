#include "automation_controller.h"

#include <algorithm>
#include <cmath>
#include <exception>

namespace billiardgl {
namespace {

double numberParam(const json::Value& params, const char* name)
{
    if (!params.has(name) || !params.at(name).isNumber()) throw std::runtime_error(std::string(name) + " must be a number");
    const double value = params.at(name).asNumber();
    if (!std::isfinite(value)) throw std::runtime_error(std::string(name) + " must be finite");
    return value;
}

int intParam(const json::Value& params, const char* name)
{
    if (!params.has(name)) throw std::runtime_error(std::string(name) + " is required");
    return params.at(name).asInt();
}

std::string stringParam(const json::Value& params, const char* name)
{
    if (!params.has(name) || !params.at(name).isString()) throw std::runtime_error(std::string(name) + " must be a string");
    return params.at(name).asString();
}

json::Value commandList(const std::vector<std::string>& commands)
{
    json::Value result = json::Value::array();
    for (const std::string& command : commands) result.asArray().push_back(json::Value(command));
    return result;
}

}  // namespace

std::vector<std::string> AutomationController::capabilities() const
{
    std::vector<std::string> values = {
        "clear_events", "get_capabilities", "get_events", "get_state", "key_down", "key_up",
        "load_scenario", "mouse_button", "mouse_move", "mouse_wheel", "orbit_camera", "pan_camera",
        "ping", "quit", "reset_game", "resize", "run_until", "set_aim_yaw", "set_ball",
        "set_player_state", "set_shot_power", "shoot", "special_key", "step", "toggle_aim",
        "toggle_help", "zoom_camera"
    };
    if (mode_ == AutomationMode::Rendered) values.push_back("screenshot");
    std::sort(values.begin(), values.end());
    return values;
}

ControllerResult AutomationController::success(int id, const json::Value& result) const
{
    ControllerResult value;
    value.response = automationSuccessResponse(id, result);
    return value;
}

ControllerResult AutomationController::failure(int id, const std::string& code, const std::string& message) const
{
    ControllerResult value;
    value.response = automationErrorResponse(id, code, message);
    return value;
}

ControllerResult AutomationController::handle(const AutomationRequest& request)
{
    try {
        const std::string& command = request.command;
        const json::Value& params = request.params;
        if (command == "ping") { json::Value value = json::Value::object(); value["pong"] = json::Value(true); return success(request.id, value); }
        if (command == "get_capabilities") { json::Value value = json::Value::object(); value["commands"] = commandList(capabilities()); value["protocol_version"] = json::Value(1); return success(request.id, value); }
        if (command == "get_state") return success(request.id, serializeAutomationState(runtime_));
        if (command == "reset_game") { runtime_.reset(); return success(request.id); }
        if (command == "quit") { ControllerResult value = success(request.id); value.quitRequested = true; return value; }

        GameAction action;
        bool hasAction = true;
        if (command == "toggle_aim") action.type = ActionType::ToggleAim;
        else if (command == "toggle_help") action.type = ActionType::ToggleHelp;
        else if (command == "shoot") action.type = ActionType::Shoot;
        else if (command == "set_aim_yaw") { action.type = ActionType::SetAimYaw; action.first = static_cast<float>(numberParam(params, "yaw")); }
        else if (command == "set_shot_power") { action.type = ActionType::SetShotPower; action.first = static_cast<float>(numberParam(params, "power")); }
        else if (command == "key_down" || command == "key_up") { action.type = command == "key_down" ? ActionType::KeyDown : ActionType::KeyUp; const std::string key = stringParam(params, "key"); if (key.size() != 1) throw std::runtime_error("key must contain one character"); action.firstInt = static_cast<unsigned char>(key[0]); }
        else if (command == "special_key") { action.type = ActionType::SpecialKey; const std::string key = stringParam(params, "key"); if (key == "left") action.firstInt = 0; else if (key == "right") action.firstInt = 1; else if (key == "up") action.firstInt = 2; else if (key == "down") action.firstInt = 3; else throw std::runtime_error("unknown special key"); }
        else if (command == "mouse_move") { action.type = ActionType::MouseMove; action.firstInt = intParam(params, "x"); action.secondInt = intParam(params, "y"); }
        else if (command == "mouse_wheel") { action.type = ActionType::MouseWheel; action.firstInt = intParam(params, "direction"); }
        else if (command == "mouse_button") { action.type = ActionType::MouseButton; const std::string button = stringParam(params, "button"); const std::string state = stringParam(params, "state"); action.firstInt = button == "left" ? 0 : button == "right" ? 1 : button == "other" ? 2 : -1; if (action.firstInt < 0 || (state != "down" && state != "up")) throw std::runtime_error("invalid mouse button or state"); action.secondInt = state == "down" ? 0 : 1; action.thirdInt = params.has("x") ? params.at("x").asInt() : 0; action.first = static_cast<float>(params.has("y") ? params.at("y").asInt() : 0); }
        else if (command == "resize") { action.type = ActionType::Resize; action.firstInt = intParam(params, "width"); action.secondInt = intParam(params, "height"); }
        else if (command == "orbit_camera") { action.type = ActionType::OrbitCamera; action.first = static_cast<float>(numberParam(params, "yaw_delta")); action.second = static_cast<float>(numberParam(params, "pitch_delta")); }
        else if (command == "pan_camera") { action.type = ActionType::PanCamera; action.first = static_cast<float>(numberParam(params, "x_delta")); action.second = static_cast<float>(numberParam(params, "z_delta")); }
        else if (command == "zoom_camera") { action.type = ActionType::ZoomCamera; action.first = static_cast<float>(numberParam(params, "delta")); }
        else hasAction = false;

        if (hasAction) {
            const ActionResult result = runtime_.dispatch(action);
            if (!result.ok) return failure(request.id, result.errorCode, "game action was rejected");
            return success(request.id);
        }
        if (command == "step") {
            const int ticks = intParam(params, "ticks");
            if (ticks < 0 || ticks > 100000) return failure(request.id, "invalid_argument", "ticks must be between 0 and 100000");
            runtime_.step(ticks); json::Value value = json::Value::object(); value["tick"] = json::Value(static_cast<double>(runtime_.tick())); return success(request.id, value);
        }
        if (command == "screenshot") {
            if (mode_ != AutomationMode::Rendered) return failure(request.id, "unsupported_in_mode", "screenshot requires rendered mode");
            ControllerResult value = success(request.id); value.screenshotRequested = true; value.screenshotPath = stringParam(params, "path"); return value;
        }
        return failure(request.id, "unknown_command", "unknown command: " + command);
    } catch (const std::exception& error) {
        return failure(request.id, "invalid_argument", error.what());
    }
}

}  // namespace billiardgl
