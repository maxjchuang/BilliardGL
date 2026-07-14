#include "automation_protocol.h"
#include "shot.h"

#include <cstdlib>
#include <iostream>

namespace {
void expect(bool value, const char* message)
{
    if (!value) { std::cerr << message << '\n'; std::exit(1); }
}
}

int main()
{
    const billiardgl::json::ParseResult json = billiardgl::json::parse(
        "{\"id\":7,\"version\":1,\"command\":\"get_state\",\"params\":{}}");
    const billiardgl::AutomationRequestResult request = billiardgl::parseAutomationRequest(json.value);
    expect(request.ok, "valid request should be accepted");
    expect(request.request.id == 7 && request.request.command == "get_state", "request fields should survive");

    billiardgl::GameRuntime runtime;
    const billiardgl::json::Value state = billiardgl::serializeAutomationState(runtime);
    expect(state.at("tick").asInt() == 0, "state should include tick");
    expect(state.at("balls").asArray().size() == 16, "state should include every ball");
    const billiardgl::json::Value& firstBall = state.at("balls").asArray()[0];
    expect(firstBall.has("angular_velocity"), "state should include authoritative angular velocity");
    expect(firstBall.at("angular_velocity").has("x") && firstBall.at("angular_velocity").has("y") &&
        firstBall.at("angular_velocity").has("z"), "angular velocity should include every axis");
    expect(state.at("aim").has("yaw"), "state should include aim");
    expect(state.at("camera").has("target"), "state should include camera");
    expect(state.at("players").has("current_player"), "state should include players");

    runtime.setPhysicsTraceEnabled(true);
    expect(runtime.step(1).ok, "runtime should produce a trace frame");
    const billiardgl::json::Value physicsFrame =
        billiardgl::serializePhysicsFrame(runtime.physicsTrace().frames().front());
    expect(physicsFrame.at("physics_profile_id").asString() ==
        "chinese_pool_cue_contact_v1", "trace should identify its physics profile");
    const billiardgl::json::Value& traceBall =
        physicsFrame.at("balls").asArray()[0];
    expect(traceBall.at("motion_state").asString() == "stationary",
        "trace should serialize stable lowercase motion state");
    expect(traceBall.has("contact_slip_speed_cm_s") &&
        traceBall.has("rotational_kinetic_energy_j"),
        "trace should serialize per-ball surface quantities");
    expect(physicsFrame.has("rotational_kinetic_energy_j") &&
        physicsFrame.has("total_kinetic_energy_j") &&
        physicsFrame.has("surface_transitions"),
        "trace should serialize frame surface energy and transitions");

    billiardgl::GameRuntime cueRuntime;
    cueRuntime.setPhysicsTraceEnabled(true);
    const billiardgl::CueImpactInput cue = billiardgl::cueImpactFromShotControls(
        0.0f, 40.0f, cueRuntime.physicsProfile());
    expect(cueRuntime.applyCueImpact(cue).ok && cueRuntime.step(1).ok,
        "cue contact should execute before serialization");
    const billiardgl::json::Value cueFrame = billiardgl::serializePhysicsFrame(
        cueRuntime.physicsTrace().frames().front());
    const billiardgl::json::Value& contact = cueFrame.at("cue_contact");
    expect(contact.at("regime").asString() == "stick" &&
        contact.at("applied").asBool(), "contact regime should be stable lowercase");
    expect(contact.has("cue_velocity_before_cm_s") &&
        contact.has("cue_velocity_after_cm_s") &&
        contact.has("contact_arm_cm") && contact.has("contact_normal") &&
        contact.has("normal_relative_speed_before_cm_s") &&
        contact.has("tangential_relative_velocity_before_cm_s") &&
        contact.has("normal_impulse_ns") && contact.has("tangential_impulse_ns") &&
        contact.has("impulse_ns") && contact.has("input_kinetic_energy_j") &&
        contact.has("output_kinetic_energy_j") && contact.has("error_code"),
        "cue contact should serialize every physical diagnostic with units");
    const billiardgl::json::Value cueState =
        billiardgl::serializeAutomationState(cueRuntime);
    expect(cueState.at("cue_impact_support").at("shot_executed").asBool(),
        "state support should report the actual applied result");

    const std::string error = billiardgl::json::stringify(
        billiardgl::automationErrorResponse(7, "invalid_argument", "bad value"));
    expect(error == "{\"error\":{\"code\":\"invalid_argument\",\"message\":\"bad value\"},\"id\":7,\"ok\":false}",
        "error schema should remain stable");

    const billiardgl::json::Value ready = billiardgl::automationReadyEvent("headless", "stdio", {"ping", "get_state"});
    expect(ready.at("event").asString() == "ready", "ready should be an event");
    expect(ready.at("protocol_version").asInt() == 1, "ready should report protocol version");
    expect(ready.at("capabilities").asArray().size() == 2, "ready should list capabilities");
    return 0;
}
