#include "automation_controller.h"
#include "physics_scenario.h"

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

std::uint64_t tickParam(const json::Value& params, const char* name, std::uint64_t fallback)
{
    if (!params.has(name)) return fallback;
    if (!params.at(name).isNumber()) {
        throw std::runtime_error(std::string(name) + " must be a number");
    }
    const double value = params.at(name).asNumber();
    constexpr double kMaximumExactJsonInteger = 9007199254740991.0;
    if (!std::isfinite(value) || value < 0.0 || value > kMaximumExactJsonInteger ||
        std::floor(value) != value) {
        throw std::runtime_error(std::string(name) + " must be a nonnegative exact integer");
    }
    return static_cast<std::uint64_t>(value);
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

Point3 pointParam(const json::Value& params, const char* name)
{
    if (!params.has(name) || !params.at(name).isObject()) throw std::runtime_error(std::string(name) + " must be an object");
    const json::Value& value = params.at(name);
    return Point3{static_cast<float>(numberParam(value, "x")), static_cast<float>(numberParam(value, "y")), static_cast<float>(numberParam(value, "z"))};
}

bool eventConditionMet(const GameRuntime& runtime, const std::string& condition, std::uint64_t after)
{
    if (condition == "balls_stopped") return !runtime.state().ballsMoving;
    for (const RuntimeEvent& event : runtime.eventsSince(after)) if (event.name == condition) return true;
    return false;
}

}  // namespace

std::vector<std::string> AutomationController::capabilities() const
{
    std::vector<std::string> values = {
        "clear_events", "get_capabilities", "get_events", "get_state", "key_down", "key_up",
        "clear_physics_trace", "get_physics_trace", "load_scenario", "mouse_button", "mouse_move", "mouse_wheel", "orbit_camera", "pan_camera",
        "physics_scenario_v1", "physics_scenario_v2_cue_input", "ping", "quit", "reset_game", "resize", "run_until", "set_aim_yaw", "set_ball",
        "set_player_state", "set_shot_power", "shoot", "special_key", "start_physics_trace", "step", "stop_physics_trace", "toggle_aim",
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
        if (command == "get_events") {
            const std::uint64_t after = params.has("after_sequence") ? static_cast<std::uint64_t>(params.at("after_sequence").asNumber()) : 0;
            json::Value list = json::Value::array(); for (const RuntimeEvent& event : runtime_.eventsSince(after)) list.asArray().push_back(serializeRuntimeEvent(event));
            json::Value value = json::Value::object(); value["events"] = list; return success(request.id, value);
        }
        if (command == "reset_game") { runtime_.reset(); return success(request.id); }
        if (command == "clear_events") { runtime_.clearEvents(); return success(request.id); }
        if (command == "start_physics_trace") { runtime_.setPhysicsTraceEnabled(true); return success(request.id); }
        if (command == "stop_physics_trace") { runtime_.setPhysicsTraceEnabled(false); return success(request.id); }
        if (command == "clear_physics_trace") { runtime_.clearPhysicsTrace(); return success(request.id); }
        if (command == "get_physics_trace") {
            const std::uint64_t afterTick = tickParam(params, "after_tick", 0);
            const int limit = params.has("limit") ? params.at("limit").asInt() : 1000;
            if (limit < 1 || limit > 1000) return failure(request.id, "invalid_argument", "limit must be between 1 and 1000");

            json::Value frames = json::Value::array();
            bool hasMore = false;
            for (const PhysicsFrame& frame : runtime_.physicsTrace().frames()) {
                if (frame.tick <= afterTick) continue;
                if (static_cast<int>(frames.asArray().size()) == limit) {
                    hasMore = true;
                    break;
                }
                frames.asArray().push_back(serializePhysicsFrame(frame));
            }
            json::Value value = json::Value::object();
            value["dropped_frames"] = json::Value(
                static_cast<double>(runtime_.physicsTrace().droppedFrames()));
            value["frames"] = frames;
            value["has_more"] = json::Value(hasMore);
            return success(request.id, value);
        }
        if (command == "quit") { ControllerResult value = success(request.id); value.quitRequested = true; return value; }

        if (command == "set_ball") {
            const int index = intParam(params, "index");
            if (index < 0 || index >= kBallCount) return failure(request.id, "invalid_argument", "ball index must be between 0 and 15");
            BallState ball = runtime_.state().balls[index];
            if (params.has("position")) ball.position = pointParam(params, "position");
            if (params.has("velocity")) ball.velocity = pointParam(params, "velocity");
            if (params.has("angular_velocity")) ball.angularVelocity = pointParam(params, "angular_velocity");
            if (params.has("rotation_axis")) ball.rotationAxis = pointParam(params, "rotation_axis");
            if (params.has("rotation_angle")) ball.rotationAngle = static_cast<float>(numberParam(params, "rotation_angle"));
            if (params.has("pocketed")) { if (!params.at("pocketed").isBool()) throw std::runtime_error("pocketed must be boolean"); ball.pocketed = params.at("pocketed").asBool(); }
            ball.speed = std::sqrt(ball.velocity.x*ball.velocity.x + ball.velocity.y*ball.velocity.y + ball.velocity.z*ball.velocity.z);
            runtime_.setBall(index, ball); return success(request.id);
        }
        if (command == "set_player_state") {
            GameState state = runtime_.state();
            if (params.has("current_player")) state.players.currentPlayer = params.at("current_player").asInt();
            if (params.has("next_player")) state.players.nextPlayer = params.at("next_player").asInt();
            if ((state.players.currentPlayer < 0 || state.players.currentPlayer > 1) || (state.players.nextPlayer < 0 || state.players.nextPlayer > 1)) throw std::runtime_error("player index must be 0 or 1");
            if (params.has("illegal_shot")) state.players.illegalShot = params.at("illegal_shot").asBool();
            runtime_.replaceState(state); return success(request.id);
        }
        if (command == "load_scenario") {
            if (params.has("scenario")) {
                const PhysicsScenarioResult parsed = parsePhysicsScenario(params.at("scenario"));
                if (!parsed.ok) return failure(request.id, parsed.errorCode, parsed.errorMessage);
                const ActionResult applied = applyPhysicsScenario(runtime_, parsed.scenario);
                if (!applied.ok) return failure(request.id, applied.errorCode, "scenario was rejected");
                return success(request.id);
            }
            if (!params.has("balls") || !params.at("balls").isArray() || params.at("balls").asArray().size() != kBallCount) throw std::runtime_error("balls must contain exactly 16 entries");
            GameState state = runtime_.state();
            for (int index=0; index<kBallCount; ++index) {
                const json::Value& item=params.at("balls").asArray()[index]; if (!item.isObject()) throw std::runtime_error("each ball must be an object");
                BallState ball=state.balls[index]; ball.position=pointParam(item,"position"); ball.velocity=pointParam(item,"velocity");
                ball.pocketed=item.has("pocketed") ? item.at("pocketed").asBool() : false;
                ball.speed=std::sqrt(ball.velocity.x*ball.velocity.x+ball.velocity.y*ball.velocity.y+ball.velocity.z*ball.velocity.z); state.balls[index]=ball;
            }
            runtime_.replaceState(state); return success(request.id);
        }

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
        if (command == "run_until") {
            const std::string condition=stringParam(params,"condition"); const int maxSteps=params.has("max_steps") ? params.at("max_steps").asInt() : 10000;
            if (maxSteps < 0 || maxSteps > 1000000) return failure(request.id,"invalid_argument","max_steps must be between 0 and 1000000");
            const std::uint64_t sequence = runtime_.events().empty() ? 0 : runtime_.events().back().sequence;
            int steps=0; while (steps<maxSteps && !eventConditionMet(runtime_,condition,sequence)) { runtime_.step(1); ++steps; }
            json::Value value=json::Value::object(); value["tick"]=json::Value(static_cast<double>(runtime_.tick())); value["steps"]=json::Value(steps); value["balls_moving"]=json::Value(runtime_.state().ballsMoving);
            if (!eventConditionMet(runtime_,condition,sequence)) { ControllerResult result=failure(request.id,"condition_not_met","condition was not met before max_steps"); result.response["result"]=value; return result; }
            return success(request.id,value);
        }
        if (command == "screenshot") {
            if (mode_ != AutomationMode::Rendered) return failure(request.id, "unsupported_in_mode", "screenshot requires rendered mode");
            json::Value details=json::Value::object(); details["tick"]=json::Value(static_cast<double>(runtime_.tick()));
            ControllerResult value = success(request.id,details); value.screenshotRequested = true; value.screenshotPath = stringParam(params, "path"); return value;
        }
        return failure(request.id, "unknown_command", "unknown command: " + command);
    } catch (const std::exception& error) {
        return failure(request.id, "invalid_argument", error.what());
    }
}

}  // namespace billiardgl
