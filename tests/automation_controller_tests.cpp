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

    expect(send(controller, 11, "start_physics_trace").response.at("ok").asBool(),
        "trace start should succeed");
    billiardgl::json::Value ticks = billiardgl::json::Value::object();
    ticks["ticks"] = billiardgl::json::Value(2);
    expect(send(controller, 12, "step", ticks).response.at("ok").asBool(),
        "traced step should succeed");

    billiardgl::json::Value firstPageParams = billiardgl::json::Value::object();
    firstPageParams["after_tick"] = billiardgl::json::Value(0);
    firstPageParams["limit"] = billiardgl::json::Value(1);
    const billiardgl::json::Value firstPage =
        send(controller, 13, "get_physics_trace", firstPageParams).response.at("result");
    expect(firstPage.at("frames").asArray().size() == 1, "trace limit should paginate");
    expect(firstPage.at("has_more").asBool(), "first page should report more frames");
    expect(firstPage.at("dropped_frames").asInt() == 0, "new trace should not drop frames");

    const billiardgl::json::Value& frame = firstPage.at("frames").asArray()[0];
    expect(frame.has("balls") && frame.has("contacts") && frame.has("control"),
        "trace frame should include state, contacts, and controls");
    expect(frame.has("linear_momentum_kg_mps") &&
        frame.has("translational_kinetic_energy_j") &&
        frame.has("maximum_penetration_cm"), "trace frame should include physical totals");
    expect(frame.at("balls").asArray()[0].has("acceleration_cm_s2") &&
        frame.at("balls").asArray()[0].has("angular_velocity_rad_s"),
        "ball trace should include acceleration and angular velocity");

    billiardgl::json::Value secondPageParams = billiardgl::json::Value::object();
    secondPageParams["after_tick"] = billiardgl::json::Value(1);
    secondPageParams["limit"] = billiardgl::json::Value(1);
    const billiardgl::json::Value secondPage =
        send(controller, 14, "get_physics_trace", secondPageParams).response.at("result");
    expect(secondPage.at("frames").asArray().size() == 1 && !secondPage.at("has_more").asBool(),
        "last trace page should terminate pagination");

    billiardgl::json::Value zeroLimit = billiardgl::json::Value::object();
    zeroLimit["limit"] = billiardgl::json::Value(0);
    expect(!send(controller, 15, "get_physics_trace", zeroLimit).response.at("ok").asBool(),
        "zero trace limit should fail");
    billiardgl::json::Value largeLimit = billiardgl::json::Value::object();
    largeLimit["limit"] = billiardgl::json::Value(1001);
    expect(!send(controller, 16, "get_physics_trace", largeLimit).response.at("ok").asBool(),
        "oversized trace limit should fail");

    expect(send(controller, 17, "stop_physics_trace").response.at("ok").asBool(),
        "trace stop should succeed");
    expect(send(controller, 18, "clear_physics_trace").response.at("ok").asBool(),
        "trace clear should succeed");
    expect(runtime.physicsTrace().frames().empty(), "controller clear should remove trace frames");
    return 0;
}
