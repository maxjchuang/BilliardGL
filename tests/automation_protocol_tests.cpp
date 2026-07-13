#include "automation_protocol.h"

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
