#include "automation_controller.h"
#include "automation_protocol.h"
#include "shot.h"

#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <iostream>
#include <limits>
#include <string>
#include <vector>

namespace {

int failures = 0;

void expect(bool condition, const std::string& message)
{
    if (!condition) {
        std::cerr << "FAIL: " << message << '\n';
        ++failures;
    }
}

billiardgl::ControllerResult send(billiardgl::AutomationController& controller,
                                   int id, const std::string& command,
                                   const billiardgl::json::Value& params = billiardgl::json::Value::object())
{
    billiardgl::AutomationRequest request;
    request.id = id;
    request.version = 1;
    request.command = command;
    request.params = params;
    return controller.handle(request);
}

billiardgl::json::Value object(std::initializer_list<std::pair<const char*, billiardgl::json::Value>> fields)
{
    billiardgl::json::Value value = billiardgl::json::Value::object();
    for (const auto& field : fields) value[field.first] = field.second;
    return value;
}

std::string errorCode(const billiardgl::ControllerResult& result)
{
    return result.response.at("error").at("code").asString();
}

bool sameImpact(const billiardgl::CueImpactInput& a,
                const billiardgl::CueImpactInput& b)
{
    return a.cueBallIndex == b.cueBallIndex &&
        std::fabs(a.cueSpeedCmS - b.cueSpeedCmS) < 1e-9 &&
        std::fabs(a.cueMassKg - b.cueMassKg) < 1e-9 &&
        a.direction == b.direction && a.elevationDegrees == b.elevationDegrees &&
        a.tipOffsetCm == b.tipOffsetCm && a.tipOffsetRadius == b.tipOffsetRadius &&
        a.chalkState == b.chalkState;
}

void testCapabilitiesAndInitialRoundTrip()
{
    billiardgl::GameRuntime runtime;
    billiardgl::AutomationController controller(runtime, billiardgl::AutomationMode::Headless);
    const std::vector<std::string> capabilities = controller.capabilities();
    const char* required[] = {"get_presentation_state", "presentation_state_v1",
                              "set_tip_offset", "set_camera_preset",
                              "set_assist_level", "shot_control_v2",
                              "begin_stroke_unsupported"};
    for (const char* capability : required) {
        expect(std::find(capabilities.begin(), capabilities.end(), capability) != capabilities.end(),
               std::string("capability is advertised: ") + capability);
    }
    const billiardgl::json::Value presentation =
        send(controller, 1, "get_presentation_state").response.at("result");
    expect(presentation.at("schema_version").asInt() == 1 &&
           presentation.at("shot_control").at("schema_version").asInt() == 1,
           "initial presentation round-trips legacy shot schema v1");
    expect(presentation.at("camera").at("state").asString() == "observe_free" &&
           presentation.at("camera").at("preset").asString() == "elevated",
           "initial semantic camera round-trips");
    expect(!presentation.at("capabilities").at("cue_elevation").asBool() &&
           !presentation.at("capabilities").at("chalk_state").asBool() &&
           !presentation.at("capabilities").at("stroke_input").asBool(),
           "unpromoted controls are explicit capabilities");
}

void testLegacyOrdinaryShotEquivalence()
{
    billiardgl::GameRuntime runtime;
    billiardgl::AutomationController controller(runtime, billiardgl::AutomationMode::Headless);
    expect(send(controller, 10, "set_aim_yaw", object({{"yaw", billiardgl::json::Value(0.42)}}))
               .response.at("ok").asBool(), "legacy yaw command succeeds");
    expect(send(controller, 11, "set_shot_power", object({{"power", billiardgl::json::Value(120.0)}}))
               .response.at("ok").asBool(), "legacy power command succeeds");
    const billiardgl::CueImpactInput expected = billiardgl::cueImpactFromShotControls(
        0.42f, 120.0f, runtime.physicsProfile());
    expect(send(controller, 12, "shoot").response.at("ok").asBool(),
           "legacy ordinary shot executes");
    expect(runtime.hasCueImpactInput() && sameImpact(runtime.cueImpactInput(), expected),
           "runtime keeps field-for-field legacy yaw+power CueImpactInput equivalence");
}

void testVersionedClosedLoopAndMovingState()
{
    billiardgl::GameRuntime runtime;
    billiardgl::AutomationController controller(runtime, billiardgl::AutomationMode::Headless);
    send(controller, 20, "set_aim_yaw", object({{"yaw", billiardgl::json::Value(0.0)}}));
    send(controller, 21, "set_shot_power", object({{"power", billiardgl::json::Value(40.0)}}));

    const billiardgl::ControllerResult tip = send(controller, 22, "set_tip_offset", object({
        {"schema_version", billiardgl::json::Value(2)},
        {"x", billiardgl::json::Value(0.25)},
        {"y", billiardgl::json::Value(0.0)},
    }));
    expect(tip.response.at("ok").asBool(), "versioned tip offset succeeds");
    const billiardgl::json::Value& tipState = tip.response.at("result");
    expect(tipState.at("shot_control").at("schema_version").asInt() == 2 &&
           std::fabs(tipState.at("shot_control").at("tip_offset_radius").at("x").asNumber() - 0.25) < 1e-6,
           "tip offset command round-trips shot schema v2");
    expect(tipState.at("cue").at("tip_offset_x").asNumber() ==
               tipState.at("hud").at("tip_offset_x").asNumber() &&
           tipState.at("hud").at("tip_offset_x").asNumber() ==
               tipState.at("shot_control").at("tip_offset_radius").at("x").asNumber(),
           "UI and visual cue consume the same ShotControlModel value");

    const billiardgl::ControllerResult assist = send(controller, 23, "set_assist_level", object({
        {"schema_version", billiardgl::json::Value(2)},
        {"level", billiardgl::json::Value("training")},
    }));
    expect(assist.response.at("ok").asBool() &&
           assist.response.at("result").at("hud").at("assist_level").asString() == "training",
           "assist level round-trips through HUD presentation");

    const billiardgl::ControllerResult preset = send(controller, 24, "set_camera_preset", object({
        {"schema_version", billiardgl::json::Value(1)},
        {"preset", billiardgl::json::Value("overhead")},
    }));
    expect(preset.response.at("ok").asBool() &&
           preset.response.at("result").at("camera").at("state").asString() == "aim_overhead" &&
           preset.response.at("result").at("camera").at("preset").asString() == "overhead",
           "camera preset command round-trips semantic state");

    const std::string authoritativeBefore = billiardgl::json::stringify(
        billiardgl::serializeAutomationState(runtime));
    const std::string previewFirst = billiardgl::json::stringify(
        send(controller, 25, "get_presentation_state").response.at("result"));
    const std::string previewSecond = billiardgl::json::stringify(
        send(controller, 26, "get_presentation_state").response.at("result"));
    const std::string authoritativeAfter = billiardgl::json::stringify(
        billiardgl::serializeAutomationState(runtime));
    expect(previewFirst == previewSecond,
           "presentation preview is deterministic without a simulation step");
    expect(authoritativeBefore == authoritativeAfter,
           "get_presentation_state never mutates authoritative state");

    const auto viewBeforeShot = runtime.cameraFrame().pose;
    expect(send(controller, 27, "shoot").response.at("ok").asBool(),
           "v2 off-center shot executes through the single conversion boundary");
    expect(runtime.hasCueImpactInput() &&
           std::fabs(runtime.cueImpactInput().tipOffsetRadius[0] - 0.25) < 1e-9,
           "CueImpactInput consumes the same tip offset as UI and cue presentation");
    expect(runtime.cameraFrame().state == billiardgl::camera::SemanticState::ObserveFree &&
           !runtime.cameraFrame().transitioning &&
           runtime.cameraFrame().pose.yawRadians == viewBeforeShot.yawRadians &&
           runtime.cameraFrame().pose.pitchRadians == viewBeforeShot.pitchRadians &&
           runtime.cameraFrame().pose.distanceCm == viewBeforeShot.distanceCm &&
           runtime.cameraFrame().pose.fieldOfViewDegrees == viewBeforeShot.fieldOfViewDegrees,
           "shot preserves the chosen view instead of automatically entering ShotFollow");
    const billiardgl::ShotControlModel controlBefore = runtime.shotControl();
    const billiardgl::camera::Frame cameraBefore = runtime.cameraFrame();

    expect(errorCode(send(controller, 28, "set_tip_offset", object({
               {"schema_version", billiardgl::json::Value(2)},
               {"x", billiardgl::json::Value(0.1)}, {"y", billiardgl::json::Value(0.1)}}))) == "invalid_state",
           "tip offset rejects moving state");
    expect(errorCode(send(controller, 29, "set_assist_level", object({
               {"schema_version", billiardgl::json::Value(2)},
               {"level", billiardgl::json::Value("off")}}))) == "invalid_state",
           "assist change rejects moving state");
    expect(errorCode(send(controller, 30, "set_camera_preset", object({
               {"schema_version", billiardgl::json::Value(1)},
               {"preset", billiardgl::json::Value("broadcast")}}))) == "invalid_state",
           "camera preset command retains its moving-shot rejection policy");
    expect(runtime.shotControl().tipOffsetX == controlBefore.tipOffsetX &&
           runtime.shotControl().assistLevel == controlBefore.assistLevel &&
           runtime.cameraFrame().state == cameraBefore.state &&
           runtime.cameraFrame().preset == cameraBefore.preset,
           "moving-state rejection is atomic");
    billiardgl::GameRuntime optedIn;
    expect(optedIn.setCameraPresetInstant(billiardgl::camera::SemanticState::ShotFollow,
               billiardgl::camera::Preset::Elevated).ok,
           "explicit follow selection remains available to camera API callers");
    billiardgl::GameAction shot;
    shot.type = billiardgl::ActionType::Shoot;
    expect(optedIn.dispatch(shot).ok &&
           optedIn.cameraFrame().state == billiardgl::camera::SemanticState::ShotFollow,
           "explicitly selected follow is not confused with the default fixed view");
}

void testBadInputAndUnsupportedStroke()
{
    billiardgl::GameRuntime runtime;
    billiardgl::AutomationController controller(runtime, billiardgl::AutomationMode::Headless);
    const std::string before = billiardgl::json::stringify(
        send(controller, 40, "get_presentation_state").response.at("result"));
    expect(errorCode(send(controller, 41, "set_tip_offset", object({
               {"schema_version", billiardgl::json::Value(1)},
               {"x", billiardgl::json::Value(0.1)}, {"y", billiardgl::json::Value(0.0)}}))) == "invalid_argument",
           "wrong shot schema is rejected");
    expect(errorCode(send(controller, 42, "set_tip_offset", object({
               {"schema_version", billiardgl::json::Value(2)},
               {"x", billiardgl::json::Value(0.81)}, {"y", billiardgl::json::Value(0.0)}}))) == "tip_offset_out_of_range",
           "out-of-range tip offset is rejected");
    expect(errorCode(send(controller, 43, "set_tip_offset", object({
               {"schema_version", billiardgl::json::Value(2)},
               {"x", billiardgl::json::Value(std::numeric_limits<double>::quiet_NaN())},
               {"y", billiardgl::json::Value(0.0)}}))) == "invalid_argument",
           "non-finite tip offset is rejected");
    expect(errorCode(send(controller, 44, "set_assist_level", object({
               {"schema_version", billiardgl::json::Value(2)},
               {"level", billiardgl::json::Value("expert")}}))) == "invalid_argument",
           "unknown assist level is rejected");
    expect(errorCode(send(controller, 45, "set_camera_preset", object({
               {"schema_version", billiardgl::json::Value(1)},
               {"preset", billiardgl::json::Value("cinematic_magic")}}))) == "invalid_argument",
           "unknown camera preset is rejected");
    expect(errorCode(send(controller, 46, "begin_stroke")) == "unsupported_capability" &&
           errorCode(send(controller, 47, "update_stroke")) == "unsupported_capability" &&
           errorCode(send(controller, 48, "release_stroke")) == "unsupported_capability",
           "stroke command family reports explicit unsupported capability");
    const std::string after = billiardgl::json::stringify(
        send(controller, 49, "get_presentation_state").response.at("result"));
    expect(before == after, "bad input and unsupported commands do not mutate presentation intent");
}

}  // namespace

int main()
{
    testCapabilitiesAndInitialRoundTrip();
    testLegacyOrdinaryShotEquivalence();
    testVersionedClosedLoopAndMovingState();
    testBadInputAndUnsupportedStroke();
    if (failures) {
        std::cerr << failures << " Phase3 integration assertion(s) failed\n";
        return EXIT_FAILURE;
    }
    std::cout << "Phase3 camera/shot/presentation automation integration tests passed\n";
    return EXIT_SUCCESS;
}
