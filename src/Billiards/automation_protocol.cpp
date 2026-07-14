#include "automation_protocol.h"

#include <algorithm>
#include <exception>

namespace billiardgl {
namespace {

json::Value pointValue(const Point3& point)
{
    json::Value value = json::Value::object();
    value["x"] = json::Value(point.x);
    value["y"] = json::Value(point.y);
    value["z"] = json::Value(point.z);
    return value;
}

json::Value floatArray(const float values[3])
{
    json::Value result = json::Value::array();
    for (int i = 0; i < 3; ++i) result.asArray().push_back(json::Value(values[i]));
    return result;
}

json::Value pairValue(const std::array<double, 2>& pair)
{
    json::Value value = json::Value::array();
    value.asArray().push_back(json::Value(pair[0]));
    value.asArray().push_back(json::Value(pair[1]));
    return value;
}

json::Value directionValue(const std::array<double, 3>& direction)
{
    json::Value value = json::Value::object();
    value["x"] = json::Value(direction[0]);
    value["y"] = json::Value(direction[1]);
    value["z"] = json::Value(direction[2]);
    return value;
}

json::Value cueImpactValue(const CueImpactInput& input)
{
    json::Value value = json::Value::object();
    value["chalk_state"] = json::Value(input.chalkState);
    value["cue_ball_index"] = json::Value(input.cueBallIndex);
    value["cue_mass_kg"] = json::Value(input.cueMassKg);
    value["cue_speed_cm_s"] = json::Value(input.cueSpeedCmS);
    value["direction"] = directionValue(input.direction);
    value["elevation_degrees"] = json::Value(input.elevationDegrees);
    value["tip_offset_cm"] = pairValue(input.tipOffsetCm);
    value["tip_offset_radius"] = pairValue(input.tipOffsetRadius);
    return value;
}

json::Value cueImpactSupportValue(
    const CueImpactInput& input, const CueContactResult* result = nullptr)
{
    const CueImpactSupport support = evaluateCueImpactSupport(input, result);
    json::Value supported = json::Value::array();
    for (const std::string& field : support.exactlyConsumableFields)
        supported.asArray().push_back(json::Value(field));
    json::Value unsupported = json::Value::array();
    for (const std::string& code : support.unsupportedCodes)
        unsupported.asArray().push_back(json::Value(code));
    json::Value value = json::Value::object();
    value["exactly_consumable_fields"] = supported;
    value["shot_executed"] = json::Value(support.shotExecuted);
    value["unsupported_codes"] = unsupported;
    return value;
}

json::Value vectorValue(const std::array<double, 3>& vector, double scale = 1.0)
{
    json::Value value = json::Value::object();
    value["x"] = json::Value(vector[0] * scale);
    value["y"] = json::Value(vector[1] * scale);
    value["z"] = json::Value(vector[2] * scale);
    return value;
}

json::Value cueContactValue(const CueContactResult& result)
{
    json::Value value = json::Value::object();
    value["applied"] = json::Value(result.applied);
    value["ball_angular_velocity_after_rad_s"] =
        vectorValue(result.ballAngularVelocityAfterRadS);
    value["ball_angular_velocity_before_rad_s"] =
        vectorValue(result.ballAngularVelocityBeforeRadS);
    value["ball_velocity_after_cm_s"] = vectorValue(result.ballVelocityAfterMS, 100.0);
    value["ball_velocity_before_cm_s"] = vectorValue(result.ballVelocityBeforeMS, 100.0);
    value["contact_arm_cm"] = vectorValue(result.contactArmM, 100.0);
    value["contact_normal"] = vectorValue(result.contactNormal);
    value["cue_velocity_after_cm_s"] = vectorValue(result.cueVelocityAfterMS, 100.0);
    value["cue_velocity_before_cm_s"] = vectorValue(result.cueVelocityBeforeMS, 100.0);
    value["error_code"] = json::Value(result.error);
    value["friction_coefficient"] = json::Value(result.frictionCoefficient);
    value["impulse_ns"] = vectorValue(result.impulseNs);
    value["input_kinetic_energy_j"] = json::Value(result.inputKineticEnergyJ);
    value["normal_impulse_ns"] = json::Value(result.normalImpulseNs);
    value["normal_relative_speed_before_cm_s"] =
        json::Value(result.normalRelativeSpeedBeforeMS * 100.0);
    value["output_kinetic_energy_j"] = json::Value(result.outputKineticEnergyJ);
    value["regime"] = json::Value(cueContactRegimeName(result.regime));
    value["tangential_impulse_ns"] = json::Value(result.tangentialImpulseNs);
    value["tangential_relative_speed_before_cm_s"] =
        json::Value(result.tangentialRelativeSpeedBeforeMS * 100.0);
    value["tangential_relative_velocity_before_cm_s"] =
        vectorValue(result.tangentialRelativeVelocityBeforeMS, 100.0);
    return value;
}

const char* aimModeName(AimMode mode) { return mode == AimMode::Aim ? "aim" : "observe"; }
const char* anchorName(CameraAnchorMode mode) { return mode == CameraAnchorMode::FollowCueBall ? "follow_cue_ball" : "free_look"; }
const char* contactKindName(PhysicsContactKind kind)
{
    if (kind == PhysicsContactKind::Rail) return "rail";
    if (kind == PhysicsContactKind::Pocket) return "pocket";
    return "ball_ball";
}

json::Value errorBody(const std::string& code, const std::string& message)
{
    json::Value error = json::Value::object();
    error["code"] = json::Value(code);
    error["message"] = json::Value(message);
    return error;
}

}  // namespace

AutomationRequestResult parseAutomationRequest(const json::Value& value)
{
    AutomationRequestResult result;
    if (!value.isObject()) {
        result.errorCode = "invalid_request";
        result.errorMessage = "request must be an object";
        return result;
    }
    try {
        if (!value.has("id") || !value.has("version") || !value.has("command") || !value.has("params"))
            throw std::runtime_error("request requires id, version, command, and params");
        result.request.id = value.at("id").asInt();
        result.request.version = value.at("version").asInt();
        result.request.command = value.at("command").asString();
        result.request.params = value.at("params");
        if (!result.request.params.isObject()) throw std::runtime_error("params must be an object");
        if (result.request.version != kAutomationProtocolVersion) {
            result.errorCode = "unsupported_version";
            result.errorMessage = "only protocol version 1 is supported";
            return result;
        }
        if (result.request.command.empty()) throw std::runtime_error("command must not be empty");
        result.ok = true;
    } catch (const std::exception& error) {
        result.errorCode = "invalid_request";
        result.errorMessage = error.what();
    }
    return result;
}

json::Value automationSuccessResponse(int id, const json::Value& result)
{
    json::Value response = json::Value::object();
    response["id"] = json::Value(id);
    response["ok"] = json::Value(true);
    response["result"] = result;
    return response;
}

json::Value automationErrorResponse(int id, const std::string& code, const std::string& message)
{
    json::Value response = json::Value::object();
    response["error"] = errorBody(code, message);
    response["id"] = json::Value(id);
    response["ok"] = json::Value(false);
    return response;
}

json::Value automationProtocolError(const std::string& code, const std::string& message)
{
    json::Value response = json::Value::object();
    response["error"] = errorBody(code, message);
    response["ok"] = json::Value(false);
    return response;
}

json::Value automationReadyEvent(const std::string& mode, const std::string& transport,
    const std::vector<std::string>& capabilities)
{
    std::vector<std::string> sorted = capabilities;
    std::sort(sorted.begin(), sorted.end());
    json::Value list = json::Value::array();
    for (const std::string& capability : sorted) list.asArray().push_back(json::Value(capability));
    json::Value ready = json::Value::object();
    ready["capabilities"] = list;
    ready["event"] = json::Value("ready");
    ready["mode"] = json::Value(mode);
    ready["protocol_version"] = json::Value(kAutomationProtocolVersion);
    ready["sequence"] = json::Value(0);
    ready["tick"] = json::Value(0);
    ready["transport"] = json::Value(transport);
    return ready;
}

json::Value serializeRuntimeEvent(const RuntimeEvent& event)
{
    json::Value value = json::Value::object();
    value["event"] = json::Value(event.name);
    value["sequence"] = json::Value(static_cast<double>(event.sequence));
    value["tick"] = json::Value(static_cast<double>(event.tick));
    return value;
}

json::Value serializePhysicsContact(const PhysicsContactRecord& contact)
{
    json::Value value = json::Value::object();
    value["first_ball"] = json::Value(contact.firstBall);
    value["kind"] = json::Value(contactKindName(contact.kind));
    value["normal"] = pointValue(contact.normal);
    value["contact_tangent"] = pointValue(contact.contactTangent);
    value["first_contact_arm_cm"] = pointValue(contact.firstContactArmCm);
    value["second_contact_arm_cm"] = pointValue(contact.secondContactArmCm);
    value["relative_contact_velocity_before_cm_s"] =
        pointValue(contact.relativeContactVelocityBeforeCmS);
    value["relative_contact_velocity_after_cm_s"] =
        pointValue(contact.relativeContactVelocityAfterCmS);
    value["normal_relative_speed_before_cm_s"] =
        json::Value(contact.normalRelativeSpeedBeforeCmS);
    value["normal_relative_speed_after_cm_s"] =
        json::Value(contact.normalRelativeSpeedAfterCmS);
    value["normal_impulse_ns"] = json::Value(contact.normalImpulseNs);
    value["tangential_impulse_ns"] = json::Value(contact.tangentialImpulseNs);
    value["impulse_on_second_ns"] = pointValue(contact.impulseOnSecondNs);
    value["friction_coefficient"] = json::Value(contact.frictionCoefficient);
    value["regime"] = json::Value(contact.kind == PhysicsContactKind::Rail
        ? cushionContactRegimeName(contact.cushionRegime)
        : ballBallContactRegimeName(contact.regime));
    value["velocity_impulse_applied"] =
        json::Value(contact.velocityImpulseApplied);
    value["kinetic_energy_before_j"] =
        json::Value(contact.kineticEnergyBeforeJ);
    value["kinetic_energy_after_j"] =
        json::Value(contact.kineticEnergyAfterJ);
    value["first_position_correction_cm"] =
        pointValue(contact.firstPositionCorrectionCm);
    value["second_position_correction_cm"] =
        pointValue(contact.secondPositionCorrectionCm);
    value["position_slop_cm"] = json::Value(contact.positionSlopCm);
    value["contact_arm_cm"] = pointValue(contact.cushionContactArmCm);
    value["contact_height_cm"] = json::Value(contact.cushionContactHeightCm);
    value["contact_velocity_before_cm_s"] =
        pointValue(contact.cushionContactVelocityBeforeCmS);
    value["contact_velocity_after_cm_s"] =
        pointValue(contact.cushionContactVelocityAfterCmS);
    value["impulse_on_ball_ns"] = pointValue(contact.impulseOnBallNs);
    value["position_correction_cm"] = pointValue(contact.positionCorrectionCm);
    value["restitution"] = json::Value(contact.restitution);
    value["nose_height_ratio"] = json::Value(contact.noseHeightRatio);
    value["incident_speed_cm_s"] = json::Value(contact.incidentSpeedCmS);
    value["maximum_rigid_incident_speed_cm_s"] =
        json::Value(contact.maximumRigidIncidentSpeedCmS);
    value["rigid_domain_exceeded"] = json::Value(contact.rigidDomainExceeded);
    value["position_corrected"] = json::Value(contact.positionCorrected);
    value["time_of_impact_seconds"] = json::Value(contact.timeOfImpactSeconds);
    value["penetration_cm"] = json::Value(contact.penetrationCm);
    value["second_ball"] = json::Value(contact.secondBall);
    value["pocket_id"] = json::Value(contact.pocketId);
    value["pocket_kind"] = json::Value(
        contact.pocketKind == PocketKind::Corner ? "corner" : "side");
    value["pocket_boundary_event"] = json::Value(
        pocketBoundaryEventKindName(contact.pocketBoundaryEvent));
    value["pocket_phase_before"] = json::Value(
        pocketInteractionPhaseName(contact.pocketPhaseBefore));
    value["pocket_phase_after"] = json::Value(
        pocketInteractionPhaseName(contact.pocketPhaseAfter));
    value["pocket_local_depth_cm"] = json::Value(contact.pocketLocal.depthCm);
    value["pocket_local_offset_cm"] = json::Value(contact.pocketLocal.offsetCm);
    value["pocket_jaw_center_cm"] = pointValue(contact.pocketJawCenterCm);
    value["pocket_jaw_radius_cm"] = json::Value(contact.pocketJawRadiusCm);
    value["pocket_throat_signed_distance_cm"] =
        json::Value(contact.pocketThroatSignedDistanceCm);
    value["pocket_capture_signed_distance_cm"] =
        json::Value(contact.pocketCaptureSignedDistanceCm);
    value["pocket_passable"] = json::Value(contact.pocketPassable);
    value["pocket_capture_sequence"] =
        json::Value(static_cast<double>(contact.pocketCaptureSequence));
    return value;
}

json::Value serializePhysicsFrame(const PhysicsFrame& frame)
{
    json::Value balls = json::Value::array();
    for (int index = 0; index < kBallCount; ++index) {
        const PhysicsBallSample& sample = frame.balls[index];
        json::Value ball = json::Value::object();
        ball["acceleration_cm_s2"] = pointValue(sample.acceleration);
        ball["angular_velocity_rad_s"] = pointValue(sample.angularVelocity);
        ball["contact_slip_speed_cm_s"] =
            json::Value(sample.contactSlipSpeedCmS);
        ball["index"] = json::Value(index);
        ball["pocketed"] = json::Value(sample.pocketed);
        ball["position_cm"] = pointValue(sample.position);
        ball["motion_state"] = json::Value(
            ballMotionStateName(sample.motionState));
        ball["rotational_kinetic_energy_j"] =
            json::Value(sample.rotationalKineticEnergyJ);
        ball["speed_cm_s"] = json::Value(sample.speed);
        ball["velocity_cm_s"] = pointValue(sample.velocity);
        balls.asArray().push_back(ball);
    }

    json::Value contacts = json::Value::array();
    for (const PhysicsContactRecord& contact : frame.contacts) {
        contacts.asArray().push_back(serializePhysicsContact(contact));
    }

    json::Value control = json::Value::object();
    control["aim_yaw_rad"] = json::Value(frame.control.aimYaw);
    control["shot_power"] = json::Value(frame.control.shotPower);
    control["shot_taken"] = json::Value(frame.control.shotTaken);

    json::Value surfaceTransitions = json::Value::array();
    for (const SurfaceMotionStep& step : frame.surfaceTransitions) {
        json::Value transition = json::Value::object();
        transition["after"] = json::Value(ballMotionStateName(step.after));
        transition["angular_acceleration_rad_s2"] =
            pointValue(step.angularAccelerationRadS2);
        transition["ball_index"] = json::Value(step.ballIndex);
        transition["before"] = json::Value(ballMotionStateName(step.before));
        transition["final_slip_speed_cm_s"] =
            json::Value(step.finalSlipSpeedCmS);
        transition["friction_acceleration_cm_s2"] =
            pointValue(step.frictionAccelerationCmS2);
        transition["initial_slip_speed_cm_s"] =
            json::Value(step.initialSlipSpeedCmS);
        transition["transition_time_seconds"] =
            json::Value(step.transitionTimeSeconds);
        surfaceTransitions.asArray().push_back(transition);
    }

    json::Value value = json::Value::object();
    value["balls"] = balls;
    value["contacts"] = contacts;
    value["control"] = control;
    value["delta_seconds"] = json::Value(frame.deltaSeconds);
    value["linear_momentum_kg_mps"] = pointValue(frame.linearMomentum);
    value["maximum_penetration_cm"] = json::Value(frame.maximumPenetrationCm);
    value["physics_profile_id"] = json::Value(frame.physicsProfileId);
    value["rotational_kinetic_energy_j"] =
        json::Value(frame.rotationalKineticEnergyJ);
    value["surface_transitions"] = surfaceTransitions;
    value["tick"] = json::Value(static_cast<double>(frame.tick));
    value["time_seconds"] = json::Value(frame.timeSeconds);
    value["translational_kinetic_energy_j"] =
        json::Value(frame.translationalKineticEnergyJ);
    value["total_kinetic_energy_j"] = json::Value(frame.totalKineticEnergyJ);
    if (frame.hasCueImpactInput) value["cue_impact"] = cueImpactValue(frame.cueImpactInput);
    if (frame.hasCueContactResult)
        value["cue_contact"] = cueContactValue(frame.cueContactResult);
    return value;
}

json::Value serializeAutomationState(const GameRuntime& runtime)
{
    const GameState& state = runtime.state();
    json::Value balls = json::Value::array();
    for (int index = 0; index < kBallCount; ++index) {
        const BallState& ball = state.balls[index];
        json::Value value = json::Value::object();
        value["index"] = json::Value(index);
        value["angular_velocity"] = pointValue(ball.angularVelocity);
        value["pocketed"] = json::Value(ball.pocketed);
        value["position"] = pointValue(ball.position);
        value["rotation_angle"] = json::Value(ball.rotationAngle);
        value["rotation_axis"] = pointValue(ball.rotationAxis);
        value["speed"] = json::Value(ball.speed);
        value["velocity"] = pointValue(ball.velocity);
        balls.asArray().push_back(value);
    }

    json::Value aim = json::Value::object();
    aim["mode"] = json::Value(aimModeName(state.aim.mode));
    aim["show_guide_line"] = json::Value(state.aim.showGuideLine);
    aim["yaw"] = json::Value(state.aim.yaw);

    json::Value input = json::Value::object();
    input["camera_pan"] = json::Value(state.input.cameraPan);
    input["left_mouse_down"] = json::Value(state.input.leftMouseDown);
    input["mouse_x"] = json::Value(state.input.mouseX);
    input["mouse_y"] = json::Value(state.input.mouseY);
    input["shot_power"] = json::Value(state.input.shotPower);

    json::Value players = json::Value::object();
    json::Value assigned = json::Value::array();
    assigned.asArray().push_back(json::Value(state.players.assignedBallType[0]));
    assigned.asArray().push_back(json::Value(state.players.assignedBallType[1]));
    players["assigned_ball_type"] = assigned;
    players["current_player"] = json::Value(state.players.currentPlayer);
    players["illegal_shot"] = json::Value(state.players.illegalShot);
    players["next_player"] = json::Value(state.players.nextPlayer);
    players["shot_taken"] = json::Value(state.players.shotTaken);

    json::Value camera = json::Value::object();
    camera["anchor_mode"] = json::Value(anchorName(state.camera.anchorMode));
    camera["angle_x"] = json::Value(state.camera.angleX);
    camera["angle_y"] = json::Value(state.camera.angleY);
    camera["eye"] = floatArray(state.camera.eye);
    camera["target"] = floatArray(state.camera.target);
    camera["zoom"] = json::Value(state.camera.zoom);

    json::Value hud = json::Value::object();
    hud["show_help"] = json::Value(state.hud.showHelp);

    json::Value events = json::Value::array();
    for (const RuntimeEvent& event : runtime.events()) events.asArray().push_back(serializeRuntimeEvent(event));

    json::Value result = json::Value::object();
    result["aim"] = aim;
    result["balls"] = balls;
    result["balls_moving"] = json::Value(state.ballsMoving);
    result["camera"] = camera;
    result["events"] = events;
    result["game_over"] = json::Value(state.gameOver);
    result["hud"] = hud;
    result["input"] = input;
    result["players"] = players;
    result["tick"] = json::Value(static_cast<double>(runtime.tick()));
    if (runtime.hasCueImpactInput()) {
        result["cue_impact"] = cueImpactValue(runtime.cueImpactInput());
        result["cue_impact_support"] = cueImpactSupportValue(
            runtime.cueImpactInput(), runtime.hasCueContactResult() ?
                &runtime.cueContactResult() : nullptr);
        if (runtime.hasCueContactResult())
            result["cue_contact"] = cueContactValue(runtime.cueContactResult());
    }
    return result;
}

}  // namespace billiardgl
