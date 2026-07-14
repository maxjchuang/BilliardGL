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
        "chinese_pool_pocket_boundary_v1", "trace should identify its physics profile");
    const billiardgl::json::Value& traceBall =
        physicsFrame.at("balls").asArray()[0];
    expect(traceBall.at("motion_state").asString() == "stationary",
        "trace should serialize stable lowercase motion state");
    expect(traceBall.has("contact_slip_speed_cm_s") &&
        traceBall.has("rotational_kinetic_energy_j"),
        "trace should serialize per-ball surface quantities");
    expect(physicsFrame.has("rotational_kinetic_energy_j") &&
        physicsFrame.has("total_kinetic_energy_j") &&
        physicsFrame.has("surface_transitions") &&
        physicsFrame.has("solver_events"),
        "trace should serialize frame surface energy and transitions");

    billiardgl::PhysicsContactRecord ballContact;
    ballContact.kind = billiardgl::PhysicsContactKind::BallBall;
    ballContact.regime = billiardgl::BallBallContactRegime::Slip;
    ballContact.velocityImpulseApplied = true;
    ballContact.solverEventId = 2;
    ballContact.solverIslandId = 1;
    ballContact.solverResidualCmS = 0.0001;
    ballContact.solverProjectionCm = 0.0002;
    ballContact.relativeContactVelocityBeforeCmS =
        billiardgl::Point3{-100.0f, 0.0f, 20.0f};
    ballContact.relativeContactVelocityAfterCmS =
        billiardgl::Point3{90.0f, 0.0f, 5.0f};
    ballContact.normalRelativeSpeedBeforeCmS = -100.0;
    ballContact.normalRelativeSpeedAfterCmS = 90.0;
    ballContact.normalImpulseNs = 0.1;
    ballContact.tangentialImpulseNs = 0.02;
    ballContact.frictionCoefficient = 0.2;
    ballContact.kineticEnergyBeforeJ = 1.0;
    ballContact.kineticEnergyAfterJ = 0.9;
    ballContact.firstPositionCorrectionCm =
        billiardgl::Point3{-0.01f, 0.0f, 0.0f};
    ballContact.secondPositionCorrectionCm =
        billiardgl::Point3{0.01f, 0.0f, 0.0f};
    const billiardgl::json::Value serializedContact =
        billiardgl::serializePhysicsContact(ballContact);
    expect(serializedContact.at("regime").asString() == "slip" &&
        serializedContact.at("velocity_impulse_applied").asBool() &&
        serializedContact.has("relative_contact_velocity_before_cm_s") &&
        serializedContact.has("relative_contact_velocity_after_cm_s") &&
        serializedContact.has("normal_relative_speed_before_cm_s") &&
        serializedContact.has("normal_relative_speed_after_cm_s") &&
        serializedContact.has("tangential_impulse_ns") &&
        serializedContact.has("friction_coefficient") &&
        serializedContact.has("kinetic_energy_before_j") &&
        serializedContact.has("kinetic_energy_after_j") &&
        serializedContact.has("first_position_correction_cm") &&
        serializedContact.has("second_position_correction_cm"),
        "ball contact serialization should use explicit units and lowercase regime");
    expect(serializedContact.at("solver_event_id").asInt() == 2 &&
        serializedContact.at("solver_island_id").asInt() == 1 &&
        serializedContact.has("solver_residual_cm_s") &&
        serializedContact.has("solver_projection_cm"),
        "solver contact serialization should retain canonical event identity");

    billiardgl::PhysicsContactRecord railContact;
    railContact.kind = billiardgl::PhysicsContactKind::Rail;
    railContact.cushionRegime = billiardgl::CushionContactRegime::Stick;
    railContact.cushionContactArmCm = billiardgl::Point3{2.0f, 1.0f, 0.0f};
    railContact.cushionContactHeightCm = 3.625;
    railContact.cushionContactVelocityBeforeCmS = billiardgl::Point3{-100.0f, 0.0f, 10.0f};
    railContact.cushionContactVelocityAfterCmS = billiardgl::Point3{80.0f, 0.0f, 0.0f};
    railContact.impulseOnBallNs = billiardgl::Point3{-0.3f, 0.0f, -0.02f};
    railContact.positionCorrectionCm = billiardgl::Point3{-0.01f, 0.0f, 0.0f};
    railContact.restitution = 0.8;
    railContact.noseHeightRatio = 1.4;
    railContact.incidentSpeedCmS = 100.0;
    railContact.maximumRigidIncidentSpeedCmS = 250.0;
    railContact.rigidDomainExceeded = false;
    railContact.positionCorrected = true;
    railContact.timeOfImpactSeconds = 0.004;
    const billiardgl::json::Value serializedRail =
        billiardgl::serializePhysicsContact(railContact);
    expect(serializedRail.at("regime").asString() == "stick" &&
        serializedRail.has("contact_arm_cm") &&
        serializedRail.has("contact_height_cm") &&
        serializedRail.has("contact_velocity_before_cm_s") &&
        serializedRail.has("contact_velocity_after_cm_s") &&
        serializedRail.has("impulse_on_ball_ns") &&
        serializedRail.has("position_correction_cm") &&
        serializedRail.has("restitution") &&
        serializedRail.has("nose_height_ratio") &&
        serializedRail.has("incident_speed_cm_s") &&
        serializedRail.has("maximum_rigid_incident_speed_cm_s") &&
        serializedRail.has("rigid_domain_exceeded") &&
        serializedRail.has("position_corrected") &&
        serializedRail.has("time_of_impact_seconds"),
        "rail contact serialization should expose complete diagnostics with units");

    billiardgl::PhysicsContactRecord pocketContact;
    pocketContact.kind = billiardgl::PhysicsContactKind::Pocket;
    pocketContact.pocketId = 5;
    pocketContact.pocketKind = billiardgl::PocketKind::Side;
    pocketContact.pocketBoundaryEvent = billiardgl::PocketBoundaryEventKind::Capture;
    pocketContact.pocketPhaseBefore = billiardgl::PocketInteractionPhase::ThroatCrossed;
    pocketContact.pocketPhaseAfter = billiardgl::PocketInteractionPhase::Captured;
    pocketContact.pocketLocal.depthCm = 6.0;
    pocketContact.pocketLocal.offsetCm = -0.25;
    pocketContact.pocketPassable = true;
    pocketContact.pocketCaptureSequence = 12;
    const billiardgl::json::Value serializedPocket =
        billiardgl::serializePhysicsContact(pocketContact);
    expect(serializedPocket.at("pocket_id").asInt() == 5 &&
        serializedPocket.at("pocket_kind").asString() == "side" &&
        serializedPocket.at("pocket_boundary_event").asString() == "capture" &&
        serializedPocket.at("pocket_phase_before").asString() == "throat_crossed" &&
        serializedPocket.at("pocket_phase_after").asString() == "captured" &&
        serializedPocket.at("pocket_capture_sequence").asInt() == 12 &&
        serializedPocket.has("pocket_local_depth_cm") &&
        serializedPocket.has("pocket_local_offset_cm") &&
        serializedPocket.has("pocket_jaw_center_cm") &&
        serializedPocket.has("pocket_jaw_radius_cm") &&
        serializedPocket.has("pocket_throat_signed_distance_cm") &&
        serializedPocket.has("pocket_capture_signed_distance_cm") &&
        serializedPocket.has("pocket_passable"),
        "pocket contact serialization should expose complete geometry and state");

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
