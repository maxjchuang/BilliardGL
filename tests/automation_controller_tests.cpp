#include "automation_controller.h"

#include <cstdlib>
#include <iostream>

namespace {
void expect(bool value, const char* message) { if (!value) { std::cerr << message << '\n'; std::exit(1); } }

billiardgl::ControllerResult send(billiardgl::AutomationController& controller, int id,
    const std::string& command, const billiardgl::json::Value& params = billiardgl::json::Value::object())
{
    billiardgl::AutomationRequest request;
    request.id = id; request.version = 1; request.command = command; request.params = params;
    return controller.handle(request);
}
}

int main()
{
    billiardgl::GameRuntime runtime;
    billiardgl::AutomationController controller(runtime, billiardgl::AutomationMode::Headless);
    expect(send(controller, 1, "ping").response.at("ok").asBool(), "ping should succeed");
    expect(send(controller, 2, "get_capabilities").response.at("result").at("commands").asArray().size() > 10,
        "capabilities should enumerate commands");

    send(controller, 3, "toggle_aim");
    expect(runtime.state().aim.mode == billiardgl::AimMode::Aim, "toggle aim should use game input logic");
    billiardgl::json::Value yaw = billiardgl::json::Value::object(); yaw["yaw"] = billiardgl::json::Value(0.25);
    expect(send(controller, 4, "set_aim_yaw", yaw).response.at("ok").asBool(), "yaw should succeed");
    billiardgl::json::Value power = billiardgl::json::Value::object(); power["power"] = billiardgl::json::Value(80);
    send(controller, 5, "set_shot_power", power);
    send(controller, 6, "shoot");
    expect(runtime.state().ballsMoving, "shoot should start motion");

    expect(!send(controller, 7, "does_not_exist").response.at("ok").asBool(), "unknown command should fail");
    billiardgl::json::Value badPower = billiardgl::json::Value::object(); badPower["power"] = billiardgl::json::Value(201);
    expect(!send(controller, 8, "set_shot_power", badPower).response.at("ok").asBool(), "bad power should fail");
    expect(send(controller, 9, "quit").quitRequested, "quit should request clean shutdown");
    send(controller, 10, "reset_game");
    expect(runtime.tick() == 0 && !runtime.state().ballsMoving, "reset should restore game");
    return 0;
}
