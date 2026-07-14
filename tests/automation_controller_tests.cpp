#include "automation_controller.h"
#include "automation_protocol.h"
#include "physics_scenario.h"

#include <algorithm>
#include <cstdlib>
#include <fstream>
#include <iostream>
#include <sstream>

namespace {
void expect(bool value, const char* message) { if (!value) { std::cerr << message << '\n'; std::exit(1); } }

billiardgl::ControllerResult send(billiardgl::AutomationController& controller, int id,
    const std::string& command, const billiardgl::json::Value& params = billiardgl::json::Value::object())
{
    billiardgl::AutomationRequest request;
    request.id = id; request.version = 1; request.command = command; request.params = params;
    return controller.handle(request);
}

billiardgl::json::Value fixture(const char* name)
{
    const std::string path = std::string(BILLIARDGL_SOURCE_ROOT) +
        "/tests/physics_validation/scenarios/" + name;
    std::ifstream input(path.c_str());
    std::ostringstream contents;
    contents << input.rdbuf();
    const billiardgl::json::ParseResult parsed = billiardgl::json::parse(contents.str());
    expect(parsed.ok, "canonical fixture should parse as JSON");
    return parsed.value;
}
}

int main()
{
    billiardgl::GameRuntime runtime;
    billiardgl::AutomationController controller(runtime, billiardgl::AutomationMode::Headless);
    expect(send(controller, 1, "ping").response.at("ok").asBool(), "ping should succeed");
    expect(send(controller, 2, "get_capabilities").response.at("result").at("commands").asArray().size() > 10,
        "capabilities should enumerate commands");
    const std::vector<std::string> capabilities = controller.capabilities();
    expect(std::find(capabilities.begin(), capabilities.end(),
        "physics_scenario_v2_cue_input") != capabilities.end(),
        "capabilities should advertise cue-input schema v2");

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
    billiardgl::json::Value largeAfterTick = billiardgl::json::Value::object();
    largeAfterTick["after_tick"] = billiardgl::json::Value(3000000000.0);
    expect(send(controller, 161, "get_physics_trace", largeAfterTick).response.at("ok").asBool(),
        "64-bit after_tick should be accepted");

    expect(send(controller, 17, "stop_physics_trace").response.at("ok").asBool(),
        "trace stop should succeed");
    expect(send(controller, 18, "clear_physics_trace").response.at("ok").asBool(),
        "trace clear should succeed");
    expect(runtime.physicsTrace().frames().empty(), "controller clear should remove trace frames");

    billiardgl::json::Value canonical = billiardgl::json::Value::object();
    canonical["scenario"] = fixture("free_roll_v1.json");
    expect(send(controller, 19, "load_scenario", canonical).response.at("ok").asBool(),
        "controller should load canonical scenario v1");
    expect(!runtime.state().balls[0].pocketed && runtime.state().balls[1].pocketed,
        "canonical scenario should atomically activate only listed balls");

    billiardgl::GameRuntime directRuntime;
    const billiardgl::PhysicsScenarioResult parsed =
        billiardgl::parsePhysicsScenario(fixture("free_roll_v1.json"));
    expect(parsed.ok && billiardgl::applyPhysicsScenario(directRuntime, parsed.scenario).ok,
        "direct runtime should load the same canonical scenario");
    directRuntime.setPhysicsTraceEnabled(true);
    runtime.setPhysicsTraceEnabled(true);
    directRuntime.step(2);
    billiardgl::json::Value twoTicks = billiardgl::json::Value::object();
    twoTicks["ticks"] = billiardgl::json::Value(2);
    expect(send(controller, 20, "step", twoTicks).response.at("ok").asBool(),
        "controller path should step the canonical scenario");
    expect(directRuntime.physicsTrace().frames().size() == runtime.physicsTrace().frames().size(),
        "direct and controller paths should emit the same frame count");
    for (std::size_t index = 0; index < directRuntime.physicsTrace().frames().size(); ++index) {
        const std::string direct = billiardgl::json::stringify(
            billiardgl::serializePhysicsFrame(directRuntime.physicsTrace().frames()[index]));
        const std::string controlled = billiardgl::json::stringify(
            billiardgl::serializePhysicsFrame(runtime.physicsTrace().frames()[index]));
        expect(direct == controlled,
            "direct and controller paths should serialize identical authoritative frames");
    }

    billiardgl::json::Value cueV2 = fixture("cue_impact_v2_contract.json");
    billiardgl::json::Value cueParams = billiardgl::json::Value::object();
    cueParams["scenario"] = cueV2;
    expect(send(controller, 21, "load_scenario", cueParams).response.at("ok").asBool(),
        "controller should load canonical cue-impact scenario v2");
    const billiardgl::json::Value cueState =
        send(controller, 22, "get_state").response.at("result");
    expect(cueState.has("cue_impact") &&
        cueState.at("cue_impact").at("cue_speed_cm_s").asInt() == 100,
        "state should round-trip requested cue input");
    expect(!cueState.at("cue_impact_support").at("shot_executed").asBool(),
        "unsupported physical cue input must not fire a surrogate shot");
    bool hasSpeedMappingLimitation = false;
    bool hasVerticalOffsetLimitation = false;
    for (const billiardgl::json::Value& item :
            cueState.at("cue_impact_support").at("unsupported_codes").asArray()) {
        hasSpeedMappingLimitation |= item.asString() == "cue_speed_to_power_mapping_missing";
        hasVerticalOffsetLimitation |= item.asString() == "vertical_tip_offset_not_modeled";
    }
    expect(hasSpeedMappingLimitation && hasVerticalOffsetLimitation,
        "state should enumerate unsupported production cue fields");
    runtime.setPhysicsTraceEnabled(true);
    expect(send(controller, 23, "step", twoTicks).response.at("ok").asBool(),
        "cue-impact scenario should remain traceable without synthesizing a shot");
    const billiardgl::json::Value cueFrame = billiardgl::serializePhysicsFrame(
        runtime.physicsTrace().frames().front());
    expect(cueFrame.has("cue_impact"), "trace should round-trip requested cue input");

    billiardgl::json::Value cueV4 = fixture("profile_override_v3.json");
    cueV4["schema_version"] = billiardgl::json::Value(4);
    cueV4["physics_profile"]["cue"]["normal_restitution"] = billiardgl::json::Value(0.0);
    cueV4["physics_profile"]["cue"]["chalked_friction_coefficient"] = billiardgl::json::Value(0.6);
    cueV4["physics_profile"]["cue"]["unchalked_friction_coefficient"] = billiardgl::json::Value(0.1);
    cueV4["physics_profile"]["cue"]["maximum_reliable_offset_radius"] = billiardgl::json::Value(0.8);
    cueV4["physics_profile"]["cue"]["cue_speed_per_power_unit_cm_s"] = billiardgl::json::Value(1.34);
    cueV4["cue_impact"] = cueV2.at("cue_impact");
    cueV4["cue_impact"]["tip_offset_cm"] = billiardgl::json::parse("[0.0,0.0]").value;
    cueV4["cue_impact"]["tip_offset_radius"] = billiardgl::json::parse("[0.0,0.0]").value;
    cueV4["cue_impact"]["chalk_state"] = billiardgl::json::Value("CHALKED");
    cueParams["scenario"] = cueV4;
    expect(send(controller, 231, "load_scenario", cueParams).response.at("ok").asBool(),
        "automation should execute supported v4 cue contact");
    expect(runtime.state().balls[0].velocity.x > 0.0f &&
        runtime.hasCueContactResult() && runtime.cueContactResult().applied,
        "automation and scenario path should retain the applied contact");

    billiardgl::json::Value malformed = cueV2;
    malformed["cue_impact"]["cue_speed_cm_s"] = billiardgl::json::Value(-1.0);
    cueParams["scenario"] = malformed;
    expect(!send(controller, 24, "load_scenario", cueParams).response.at("ok").asBool(),
        "malformed cue-impact scenario should fail");
    expect(send(controller, 25, "get_state").response.at("result").at("cue_impact")
        .at("cue_speed_cm_s").asInt() == 100,
        "malformed scenario rejection must not mutate runtime state");
    return 0;
}
