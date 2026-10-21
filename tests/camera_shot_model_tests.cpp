#include "camera/camera_state_machine.h"
#include "render/modern/camera_projection.h"
#include "shot.h"
#include "shot_control_model.h"

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

bool close(double a, double b, double tolerance = 0.0001)
{
    return std::fabs(a - b) <= tolerance;
}

billiardgl::PresentationSnapshot snapshotAt(float cueX, float cueZ)
{
    billiardgl::PresentationSnapshot snapshot;
    snapshot.balls[0].position = {cueX, billiardgl::kTableModelPlaySurfaceHeightCm +
        billiardgl::kChineseBallRadiusCm, cueZ};
    snapshot.balls[0].visible = true;
    snapshot.camera.target = {7.0f, 88.0f, -11.0f};
    snapshot.camera.eye = {20.0f, 230.0f, -140.0f};
    snapshot.hud.shotPower = 73.0f;
    return snapshot;
}

void testDefinitionsAndTransitions()
{
    using namespace billiardgl::camera;
    const SemanticState states[] = {
        SemanticState::ObserveFree, SemanticState::AimPlayer,
        SemanticState::AimOverhead, SemanticState::ShotFollow,
        SemanticState::ReplayFree, SemanticState::ReplayBroadcast,
    };
    for (SemanticState state : states) {
        const StateDefinition& value = definition(state);
        expect(value.state == state, "semantic camera state definition is addressable");
        expect(value.fieldOfViewDegrees >= 20.0f && value.fieldOfViewDegrees <= 90.0f,
               "camera state defines bounded FOV");
        expect(value.minimumDistanceCm > 0.0f && value.maximumDistanceCm > value.minimumDistanceCm,
               "camera state defines a valid distance range");
        expect(value.enterTransitionSeconds > 0.0f && value.exitTransitionSeconds > 0.0f &&
               value.maximumTargetSpeedCmS > 0.0f,
               "camera state defines bounded enter/exit transition motion");
    }
    expect(definition(SemanticState::AimPlayer).input.changeAim &&
           !definition(SemanticState::AimPlayer).input.orbit,
           "AimPlayer reserves input for aim rather than camera orbit");
    expect(definition(SemanticState::ObserveFree).input.orbit &&
           definition(SemanticState::ObserveFree).input.pan,
           "ObserveFree permits camera inspection");
    expect(!definition(SemanticState::ShotFollow).input.orbit &&
           !definition(SemanticState::ReplayBroadcast).input.pan,
           "automatic states reject manual camera drift");

    const Preset presets[] = {Preset::PlayerLow, Preset::Elevated,
                              Preset::Overhead, Preset::Broadcast};
    for (Preset preset : presets) {
        const PresetDefinition& value = definition(preset);
        expect(value.preset == preset && value.distanceCm > 0.0f,
               "four camera presets are explicitly defined");
    }

    const billiardgl::PresentationSnapshot snapshot = snapshotAt(0.0f, -40.0f);
    StateMachine camera;
    camera.reset(SemanticState::ObserveFree, Preset::Elevated, snapshot);
    camera.transitionTo(SemanticState::AimOverhead, Preset::Overhead, snapshot);
    expect(camera.frame().state == SemanticState::AimOverhead && camera.frame().transitioning,
           "semantic state transition begins immediately");
    for (int i = 0; i < 120; ++i) camera.update(1.0f / 120.0f, snapshot);
    expect(!camera.frame().transitioning &&
           close(camera.frame().pose.fieldOfViewDegrees,
                 definition(SemanticState::AimOverhead).fieldOfViewDegrees),
           "semantic state transition reaches the requested pose");
}

void testProductionCompositionContracts()
{
    using namespace billiardgl::camera;
    const billiardgl::PresentationSnapshot snapshot = snapshotAt(0.0f, -70.0f);
    StateMachine camera;
    camera.reset(SemanticState::AimOverhead, Preset::Overhead, snapshot);
    const Pose overhead = camera.frame().pose;
    const float visibleHalfHeight = overhead.distanceCm * std::tan(
        overhead.fieldOfViewDegrees * 3.14159265358979323846f / 360.0f);
    expect(definition(SemanticState::AimOverhead).targetBehavior ==
               TargetBehavior::TableCenter &&
           close(overhead.target.x, 0.0f) &&
           close(overhead.target.z, 0.0f) &&
           visibleHalfHeight >= billiardgl::kChinesePlayfieldLengthCm * 0.5f,
           "overhead production framing includes cue-ball, rack, and both table ends");

    camera.reset(SemanticState::ReplayBroadcast, Preset::Broadcast, snapshot);
    const Pose broadcast = camera.frame().pose;
    const float horizontalDistance = std::hypot(
        broadcast.eye.x - broadcast.target.x,
        broadcast.eye.z - broadcast.target.z);
    constexpr float kAuthoredLampUndersideCm = 221.0f;
    expect(broadcast.eye.y < kAuthoredLampUndersideCm &&
           horizontalDistance > billiardgl::kChinesePlayfieldLengthCm * 0.8f &&
           close(broadcast.target.x, 0.0f) && close(broadcast.target.z, 0.0f),
           "broadcast production framing remains below the lamp plane while retaining venue context");
}

billiardgl::camera::Pose transitionAtRate(int hertz)
{
    using namespace billiardgl::camera;
    const billiardgl::PresentationSnapshot snapshot = snapshotAt(21.0f, -35.0f);
    StateMachine camera;
    camera.reset(SemanticState::ObserveFree, Preset::Elevated, snapshot);
    camera.transitionTo(SemanticState::AimOverhead, Preset::Overhead, snapshot);
    for (int i = 0; i < hertz / 4; ++i)
        camera.update(1.0f / static_cast<float>(hertz), snapshot);
    return camera.frame().pose;
}

void testFrameRateConsistencyAndSpeedBounds()
{
    using namespace billiardgl::camera;
    const Pose hz60 = transitionAtRate(60);
    const Pose hz120 = transitionAtRate(120);
    const Pose hz144 = transitionAtRate(144);
    const auto same = [](const Pose& a, const Pose& b) {
        return close(a.target.x, b.target.x, 0.002) &&
               close(a.target.y, b.target.y, 0.002) &&
               close(a.target.z, b.target.z, 0.002) &&
               close(a.distanceCm, b.distanceCm, 0.002) &&
               close(a.yawRadians, b.yawRadians, 0.0001) &&
               close(a.pitchRadians, b.pitchRadians, 0.0001) &&
               close(a.fieldOfViewDegrees, b.fieldOfViewDegrees, 0.002);
    };
    expect(same(hz60, hz120) && same(hz120, hz144),
           "camera transitions are time-consistent at 60/120/144 Hz");

    const billiardgl::PresentationSnapshot snapshot = snapshotAt(50.0f, 90.0f);
    StateMachine camera;
    camera.reset(SemanticState::ObserveFree, Preset::PlayerLow, snapshot);
    camera.transitionTo(SemanticState::AimOverhead, Preset::Overhead, snapshot);
    const StateDefinition& limits = definition(SemanticState::AimOverhead);
    Pose previous = camera.frame().pose;
    constexpr float dt = 1.0f / 144.0f;
    for (int i = 0; i < 200 && camera.frame().transitioning; ++i) {
        const Pose current = camera.update(dt, snapshot).pose;
        expect(std::fabs(current.distanceCm - previous.distanceCm) <=
                   limits.maximumDistanceSpeedCmS * dt * 1.02f,
               "transition distance respects maximum speed");
        expect(std::fabs(current.pitchRadians - previous.pitchRadians) <=
                   limits.maximumAngularSpeedRadiansS * dt * 1.02f,
               "transition angle respects maximum speed");
        previous = current;
    }
}

void testFineAimClampAndSnapshotImmutability()
{
    using namespace billiardgl::camera;
    billiardgl::PresentationSnapshot snapshot = snapshotAt(0.0f, -20.0f);
    const billiardgl::PresentationSnapshot before = snapshot;
    StateMachine camera;
    camera.reset(SemanticState::AimPlayer, Preset::PlayerLow, snapshot);
    const Vec3 locked = camera.frame().pose.target;
    snapshot.balls[0].position.x = 50.0f;
    UpdateInput fine;
    fine.fineAimActive = true;
    camera.update(0.25f, snapshot, fine);
    expect(close(camera.frame().pose.target.x, locked.x),
           "fine aim disables automatic target drift");
    fine.fineAimActive = false;
    camera.update(0.25f, snapshot, fine);
    expect(camera.frame().pose.target.x > locked.x && camera.frame().pose.target.x <= 45.0f,
           "automatic tracking resumes with bounded target speed");

    camera.reset(SemanticState::ObserveFree, Preset::Elevated, snapshot);
    UpdateInput excessive;
    excessive.panXDeltaCm = 10000.0f;
    excessive.panZDeltaCm = -10000.0f;
    excessive.zoomDeltaCm = 10000.0f;
    excessive.orbitPitchDeltaRadians = 100.0f;
    const Frame clamped = camera.update(0.1f, snapshot, excessive);
    expect(clamped.pose.target.x <= billiardgl::kChinesePlayfieldWidthCm * 0.5f &&
           clamped.pose.target.z >= -billiardgl::kChinesePlayfieldLengthCm * 0.5f,
           "camera target is clamped to table edges");
    expect(close(clamped.pose.distanceCm,
                 definition(SemanticState::ObserveFree).maximumDistanceCm),
           "camera distance is clamped to state maximum");
    excessive.zoomDeltaCm = -10000.0f;
    expect(close(camera.update(0.1f, snapshot, excessive).pose.distanceCm,
                 definition(SemanticState::ObserveFree).minimumDistanceCm),
           "camera distance is clamped to state minimum");

    expect(before.balls[0].position.x == 0.0f && before.camera.eye.x == 20.0f &&
           before.hud.shotPower == 73.0f,
           "snapshot baseline is stable");
    const float snapshotX = snapshot.balls[0].position.x;
    const float snapshotEye = snapshot.camera.eye.x;
    const float snapshotPower = snapshot.hud.shotPower;
    camera.transitionTo(SemanticState::ReplayBroadcast, Preset::Broadcast, snapshot);
    camera.update(0.2f, snapshot);
    expect(snapshot.balls[0].position.x == snapshotX &&
           snapshot.camera.eye.x == snapshotEye && snapshot.hud.shotPower == snapshotPower,
           "camera state machine never mutates PresentationSnapshot");
}

void testRelativePresentationPreferencesAndExactCancel()
{
    using namespace billiardgl::camera;
    const billiardgl::PresentationSnapshot snapshot = snapshotAt(0.0f, -40.0f);
    StateMachine camera;
    camera.reset(SemanticState::ObserveFree, Preset::Elevated, snapshot);
    const Pose authoredDefault = camera.frame().pose;
    expect(close(authoredDefault.distanceCm, definition(Preset::Elevated).distanceCm) &&
           close(authoredDefault.fieldOfViewDegrees,
                 definition(SemanticState::ObserveFree).fieldOfViewDegrees),
           "identity presentation preferences preserve authored camera composition");

    PresentationPreferences adjusted;
    adjusted.fieldOfViewOffsetDegrees = 10.0f;
    adjusted.distanceScale = 1.25f;
    camera.setPresentationPreferences(adjusted);
    expect(close(camera.frame().pose.distanceCm, authoredDefault.distanceCm * 1.25f) &&
           close(camera.frame().pose.fieldOfViewDegrees,
                 authoredDefault.fieldOfViewDegrees + 10.0f),
           "FOV adjustment and camera distance scale are relative to authored optics");

    camera.reset(SemanticState::AimPlayer, Preset::PlayerLow, snapshot);
    expect(close(camera.frame().pose.distanceCm,
                 definition(Preset::PlayerLow).distanceCm * 1.25f) &&
           close(camera.frame().pose.fieldOfViewDegrees,
                 definition(SemanticState::AimPlayer).fieldOfViewDegrees + 10.0f),
           "relative preferences apply once to future semantic camera compositions");

    camera.setPresentationPreferences({0.0f, 1.0f, true});
    camera.reset(SemanticState::ObserveFree, Preset::Elevated, snapshot);
    camera.transitionTo(SemanticState::AimOverhead, Preset::Overhead, snapshot);
    camera.update(0.1f, snapshot);
    const Frame beforePreview = camera.frame();
    camera.setPresentationPreferences({35.0f, 500.0f / 180.0f, true});
    expect(camera.frame().pose.distanceCm <
               beforePreview.pose.distanceCm * (500.0f / 180.0f) &&
           camera.frame().pose.distanceCm <=
               definition(SemanticState::ObserveFree).maximumDistanceCm,
           "preview safely clips in-flight authored endpoints to semantic distance limits");
    camera.setPresentationPreferences({0.0f, 1.0f, true});
    const Frame afterCancel = camera.frame();
    expect(afterCancel.transitioning == beforePreview.transitioning &&
           close(afterCancel.transitionAlpha, beforePreview.transitionAlpha) &&
           close(afterCancel.pose.target.x, beforePreview.pose.target.x) &&
           close(afterCancel.pose.target.y, beforePreview.pose.target.y) &&
           close(afterCancel.pose.target.z, beforePreview.pose.target.z) &&
           close(afterCancel.pose.distanceCm, beforePreview.pose.distanceCm) &&
           close(afterCancel.pose.yawRadians, beforePreview.pose.yawRadians) &&
           close(afterCancel.pose.pitchRadians, beforePreview.pose.pitchRadians) &&
           close(afterCancel.pose.fieldOfViewDegrees,
                 beforePreview.pose.fieldOfViewDegrees),
           "cancel recomputes from authored transition endpoints after clipped preview");

    PresentationPreferences invalid;
    invalid.fieldOfViewOffsetDegrees = std::numeric_limits<float>::infinity();
    invalid.distanceScale = std::numeric_limits<float>::quiet_NaN();
    camera.setPresentationPreferences(invalid);
    expect(close(camera.presentationPreferences().fieldOfViewOffsetDegrees, 0.0f) &&
           close(camera.presentationPreferences().distanceScale, 1.0f),
           "non-finite presentation preferences fail to identity values");
}

void testDynamicTrackingPreference()
{
    using namespace billiardgl::camera;
    billiardgl::PresentationSnapshot snapshot = snapshotAt(0.0f, -40.0f);
    StateMachine camera;
    camera.setPresentationPreferences({0.0f, 1.0f, false});
    camera.reset(SemanticState::AimPlayer, Preset::PlayerLow, snapshot);
    const Vec3 fixedTarget = camera.frame().pose.target;
    snapshot.balls[0].position.x = 40.0f;
    camera.update(0.25f, snapshot);
    expect(close(camera.frame().pose.target.x, fixedTarget.x),
           "dynamic tracking off does not follow a moving cue ball");

    camera.transitionTo(SemanticState::AimOverhead, Preset::Overhead, snapshot);
    expect(!camera.frame().transitioning &&
           camera.frame().state == SemanticState::AimOverhead &&
           close(camera.frame().pose.target.x, 0.0f) &&
           close(camera.frame().pose.target.z, 0.0f),
           "explicit view selection remains available and cuts when dynamic tracking is off");

    camera.reset(SemanticState::ObserveFree, Preset::Elevated, snapshot);
    const Pose manualBefore = camera.frame().pose;
    UpdateInput manual;
    manual.orbitYawDeltaRadians = 0.2f;
    manual.panXDeltaCm = 5.0f;
    manual.zoomDeltaCm = -10.0f;
    const Pose manualAfter = camera.update(0.1f, snapshot, manual).pose;
    expect(close(manualAfter.yawRadians, manualBefore.yawRadians + 0.2f) &&
           close(manualAfter.target.x, manualBefore.target.x + 5.0f) &&
           close(manualAfter.distanceCm, manualBefore.distanceCm - 10.0f),
           "dynamic tracking off leaves manual orbit, pan, and zoom enabled");
    camera.setPresentationPreferences({12.0f, 1.4f, false});
    camera.setPresentationPreferences({0.0f, 1.0f, false});
    expect(close(camera.frame().pose.yawRadians, manualAfter.yawRadians) &&
           close(camera.frame().pose.target.x, manualAfter.target.x) &&
           close(camera.frame().pose.target.z, manualAfter.target.z) &&
           close(camera.frame().pose.distanceCm, manualAfter.distanceCm),
           "preference preview and cancel preserve the user's manual camera pose");

    camera.setPresentationPreferences({0.0f, 1.0f, true});
    camera.reset(SemanticState::AimPlayer, Preset::PlayerLow, snapshotAt(0.0f, -40.0f));
    camera.update(0.25f, snapshot);
    expect(camera.frame().pose.target.x > 0.0f,
           "re-enabling dynamic tracking resumes bounded cue-ball tracking");
}

void testSharedProjectionSupportsSettingsRange()
{
    const auto twenty = billiardgl::render::sharedPerspective(20.0f, 1.0f);
    const auto twentyFive = billiardgl::render::sharedPerspective(25.0f, 1.0f);
    const auto seventyFive = billiardgl::render::sharedPerspective(75.0f, 1.0f);
    const auto eightyFive = billiardgl::render::sharedPerspective(85.0f, 1.0f);
    const auto ninety = billiardgl::render::sharedPerspective(90.0f, 1.0f);
    expect(twenty[5] > twentyFive[5] &&
           eightyFive[5] < seventyFive[5] && ninety[5] < eightyFive[5] &&
           close(ninety[5], 1.0f, 0.0001),
           "shared projection preserves the full 20..90 degree camera range");
    billiardgl::CameraPresentation camera;
    camera.eye = {0.0f, 180.0f, -180.0f};
    camera.target = {0.0f, billiardgl::kTableModelPlaySurfaceHeightCm, 0.0f};
    camera.fieldOfViewDegrees = 90.0f;
    expect(billiardgl::render::buildSharedCameraProjection(
               camera, 1920, 1080).valid,
           "90 degree product camera produces a finite shared projection");
}

bool sameCueImpact(const billiardgl::CueImpactInput& a,
                   const billiardgl::CueImpactInput& b)
{
    return a.cueBallIndex == b.cueBallIndex && close(a.cueSpeedCmS, b.cueSpeedCmS, 1e-9) &&
           close(a.cueMassKg, b.cueMassKg, 1e-9) && a.direction == b.direction &&
           close(a.elevationDegrees, b.elevationDegrees, 1e-9) &&
           a.tipOffsetCm == b.tipOffsetCm && a.tipOffsetRadius == b.tipOffsetRadius &&
           a.chalkState == b.chalkState;
}

billiardgl::PhysicsProfile interactiveProfile()
{
    billiardgl::PhysicsProfile profile;
    profile.id = "chinese_pool_interactive_120hz_v5";
    return profile;
}

void testLegacyShotEquivalence()
{
    const billiardgl::PhysicsProfile profile = interactiveProfile();
    const float yaws[] = {-1.2f, 0.0f, 1.5707963f};
    const float powers[] = {0.0f, 40.0f, 120.0f, 200.0f};
    for (float yaw : yaws) {
        for (float power : powers) {
            const billiardgl::CueImpactInput old =
                billiardgl::cueImpactFromShotControls(yaw, power, profile);
            const billiardgl::ShotControlConversion converted =
                billiardgl::convertShotControls(
                    billiardgl::legacyShotControls(yaw, power), {}, profile);
            expect(converted.ok && sameCueImpact(old, converted.command.cueImpact),
                   "legacy yaw+power conversion remains field-for-field equivalent");
        }
    }
}

void testVersionedShotIntentAndCapabilities()
{
    const billiardgl::PhysicsProfile profile = interactiveProfile();
    billiardgl::ShotControlModel model;
    model.schemaVersion = billiardgl::ShotControlSchemaVersion::IntentV2;
    model.yawRadians = 0.0f;
    model.requestedPower = 80.0f;
    model.tipOffsetX = 0.30f;
    model.tipOffsetY = -0.20f;
    model.assistLevel = billiardgl::AssistLevel::Training;
    const billiardgl::ShotControlConversion converted =
        billiardgl::convertShotControls(model, {}, profile);
    expect(converted.ok, "v2 explicit shot intent converts");
    if (converted.ok) {
        expect(close(converted.command.cueImpact.direction[0], 1.0) &&
               close(converted.command.cueImpact.direction[1], 0.0) &&
               close(converted.command.cueImpact.direction[2], 0.0),
               "shot yaw is the sole direction source");
        expect(close(converted.command.cueImpact.tipOffsetRadius[0], 0.30) &&
               close(converted.command.cueImpact.tipOffsetRadius[1], -0.20) &&
               close(converted.command.cueImpact.tipOffsetCm[0], 0.30 * profile.ball.radiusCm) &&
               close(converted.command.cueImpact.tipOffsetCm[1], -0.20 * profile.ball.radiusCm),
               "tip offset radius and centimetres come from one conversion");
        expect(converted.command.assistLevel == billiardgl::AssistLevel::Training,
               "assist level crosses the shot command boundary without changing physics");
        expect(!converted.visibleControls.cueElevation && !converted.visibleControls.chalkState,
               "unpromoted elevation/chalk controls are hidden in ordinary mode");
    }

    model.powerMode = billiardgl::ShotPowerMode::StrokeSpeedCmS;
    model.strokeSpeedCmS = 321.5f;
    const billiardgl::ShotControlConversion stroke =
        billiardgl::convertShotControls(model, {}, profile);
    expect(stroke.ok && close(stroke.command.cueImpact.cueSpeedCmS, 321.5),
           "stroke-speed mode maps directly to physical cue speed");

    model.cueElevationDegrees = 5.0f;
    expect(billiardgl::convertShotControls(model, {}, profile).error ==
               "cue_elevation_capability_required",
           "unpromoted cue elevation is rejected");
    model.cueElevationDegrees = 0.0f;
    model.chalk = billiardgl::ChalkIntent::Unchalked;
    expect(billiardgl::convertShotControls(model, {}, profile).error ==
               "chalk_state_capability_required",
           "unpromoted chalk state is rejected");
    model.cueElevationDegrees = 8.0f;
    const billiardgl::ShotControlConversion promoted =
        billiardgl::convertShotControls(model, {true, true}, profile);
    expect(promoted.ok && promoted.visibleControls.cueElevation &&
           promoted.visibleControls.chalkState &&
           promoted.command.cueImpact.chalkState == "UNCHALKED" &&
           close(promoted.command.cueImpact.elevationDegrees, 8.0),
           "promoted elevation/chalk capabilities become visible and consumable");
}

void testShotBoundsAndNoMutation()
{
    const billiardgl::PhysicsProfile profile = interactiveProfile();
    billiardgl::ShotControlModel legacy = billiardgl::legacyShotControls(0.0f, 60.0f);
    legacy.tipOffsetX = 0.1f;
    expect(billiardgl::convertShotControls(legacy, {true, true}, profile).error ==
               "legacy_shot_contains_v2_fields",
           "v1 never silently discards v2-only fields");

    billiardgl::ShotControlModel model;
    model.requestedPower = 201.0f;
    expect(billiardgl::convertShotControls(model, {}, profile).error == "requested_power_out_of_range",
           "requested power upper boundary is enforced");
    model.requestedPower = 60.0f;
    model.tipOffsetX = profile.cue.maximumReliableOffsetRadius;
    model.tipOffsetY = 0.01f;
    expect(billiardgl::convertShotControls(model, {}, profile).error == "tip_offset_out_of_range",
           "radial tip-offset boundary is enforced");
    model.tipOffsetX = model.tipOffsetY = 0.0f;
    model.cueElevationDegrees = 90.0f;
    expect(billiardgl::convertShotControls(model, {true, true}, profile).error == "cue_elevation_out_of_range",
           "cue elevation boundary is enforced");
    model.cueElevationDegrees = 0.0f;
    model.powerMode = billiardgl::ShotPowerMode::StrokeSpeedCmS;
    model.strokeSpeedCmS = 3000.0f;
    expect(billiardgl::convertShotControls(model, {}, profile).error == "stroke_speed_out_of_range",
           "stroke speed boundary is enforced");
    model.strokeSpeedCmS = std::numeric_limits<float>::quiet_NaN();
    expect(billiardgl::convertShotControls(model, {}, profile).error == "shot_control_nonfinite",
           "non-finite shot input is rejected");

    model = billiardgl::ShotControlModel{};
    const billiardgl::ShotControlModel before = model;
    (void)billiardgl::convertShotControls(model, {}, profile);
    expect(model.yawRadians == before.yawRadians &&
           model.requestedPower == before.requestedPower &&
           model.tipOffsetX == before.tipOffsetX && model.tipOffsetY == before.tipOffsetY &&
           model.assistLevel == before.assistLevel,
           "shot conversion does not mutate the authoritative control model");
}

void testImpactShakeIsBoundedAndReversible()
{
    using namespace billiardgl::camera;
    const auto snapshot = snapshotAt(0, 0);
    StateMachine baseline, shaken;
    PresentationPreferences preferences;
    preferences.impactShakeEnabled = true;
    shaken.setPresentationPreferences(preferences);
    baseline.reset(SemanticState::ObserveFree, Preset::Elevated, snapshot);
    shaken.reset(SemanticState::ObserveFree, Preset::Elevated, snapshot);
    UpdateInput impact;
    impact.impactStrength = 100.0f;
    const auto original = baseline.update(1.0f / 120, snapshot, impact);
    const auto moved = shaken.update(1.0f / 120, snapshot, impact);
    const double dx = moved.pose.eye.x - original.pose.eye.x;
    const double dz = moved.pose.eye.z - original.pose.eye.z;
    expect(dx * dx + dz * dz > 0.00001 && dx * dx + dz * dz <= 0.35 * 0.35 + 0.0001,
           "impact shake is visible and bounded even for excessive input");
    expect(close(moved.pose.target.x, original.pose.target.x) &&
           close(moved.pose.yawRadians, original.pose.yawRadians),
           "impact shake does not move the aiming target or authored orientation");
    preferences.impactShakeEnabled = false;
    shaken.setPresentationPreferences(preferences);
    expect(close(shaken.frame().pose.eye.x, original.pose.eye.x) &&
           close(shaken.frame().pose.eye.z, original.pose.eye.z),
           "disabling shake immediately restores the exact base camera");
    preferences.impactShakeEnabled = true;
    shaken.setPresentationPreferences(preferences);
    const auto zeroDt = shaken.update(0, snapshot, impact);
    expect(close(zeroDt.pose.eye.x, original.pose.eye.x),
           "zero-time input updates cannot create impact impulses");
    shaken.update(1.0f / 120, snapshot, impact);
    for (int i = 0; i < 240; ++i) shaken.update(1.0f / 120, snapshot);
    expect(close(shaken.frame().pose.eye.x, original.pose.eye.x) &&
           close(shaken.frame().pose.eye.z, original.pose.eye.z),
           "impact decays without leaving camera drift");
    impact.fineAimActive = true;
    const auto fine = shaken.update(1.0f / 120, snapshot, impact);
    expect(close(fine.pose.eye.x, original.pose.eye.x),
           "fine aim suppresses impact shake");
}

void testExactFrameRestorePreservesCurrentPreferences()
{
    using namespace billiardgl::camera;
    const auto snapshot = snapshotAt(0, 0);
    StateMachine camera;
    PresentationPreferences preferences;
    preferences.fieldOfViewOffsetDegrees = 5.0f;
    preferences.distanceScale = 1.2f;
    camera.setPresentationPreferences(preferences);
    camera.reset(SemanticState::ObserveFree, Preset::Elevated, snapshot);
    UpdateInput orbit;
    orbit.orbitYawDeltaRadians = 0.24f;
    orbit.orbitPitchDeltaRadians = -0.08f;
    orbit.panXDeltaCm = 7.0f;
    orbit.panZDeltaCm = -11.0f;
    orbit.zoomDeltaCm = 18.0f;
    const Frame saved = camera.update(0.0f, snapshot, orbit);

    camera.reset(SemanticState::AimOverhead, Preset::Overhead, snapshot);
    expect(camera.restoreFrame(saved),
        "a finite in-range product camera frame restores");
    const Frame restored = camera.frame();
    expect(restored.state == saved.state && restored.preset == saved.preset &&
        close(restored.pose.eye.x, saved.pose.eye.x) &&
        close(restored.pose.eye.y, saved.pose.eye.y) &&
        close(restored.pose.eye.z, saved.pose.eye.z) &&
        close(restored.pose.target.x, saved.pose.target.x) &&
        close(restored.pose.target.z, saved.pose.target.z) &&
        close(restored.pose.yawRadians, saved.pose.yawRadians) &&
        close(restored.pose.distanceCm, saved.pose.distanceCm) &&
        close(restored.pose.fieldOfViewDegrees,
            saved.pose.fieldOfViewDegrees),
        "restore returns the exact user-composed camera frame");
    expect(close(camera.presentationPreferences().distanceScale, 1.2f) &&
        close(camera.presentationPreferences().fieldOfViewOffsetDegrees, 5.0f),
        "restore does not overwrite current presentation preferences");

    Frame invalid = saved;
    invalid.pose.eye.x = std::numeric_limits<float>::infinity();
    const Frame beforeInvalid = camera.frame();
    expect(!camera.restoreFrame(invalid) &&
        close(camera.frame().pose.eye.x, beforeInvalid.pose.eye.x),
        "non-finite restore input is rejected atomically");
    invalid = saved;
    invalid.pose.distanceCm = 10000.0f;
    expect(!camera.restoreFrame(invalid) &&
        close(camera.frame().pose.distanceCm,
            beforeInvalid.pose.distanceCm),
        "out-of-range restore input is rejected atomically");
}

void testOptInM4CameraCandidate()
{
    using namespace billiardgl::camera;
    auto snapshot = snapshotAt(0.0f, -70.0f);
    snapshot.shotControl.yawRadians = 0.37f;
    StateMachine machine;
    PresentationPreferences preferences;
    preferences.m4CameraCandidate = true;
    machine.setPresentationPreferences(preferences);
    machine.reset(SemanticState::AimPlayer, Preset::PlayerLow, snapshot);
    const auto low = machine.frame().pose;
    const double height = low.eye.y - billiardgl::kTableModelPlaySurfaceHeightCm;
    const double diameter = billiardgl::kChineseBallDiameterCm /
        (2 * low.distanceCm * std::tan(low.fieldOfViewDegrees * 3.141592653589793 / 360));
    expect(height >= 8 && height <= 16 && diameter >= .08 && diameter <= .14,
        "opt-in real PlayerLow meets height and cue diameter CPU constraints");
    const double behind = std::cos(low.yawRadians) * std::cos(snapshot.shotControl.yawRadians) +
        std::sin(low.yawRadians) * std::sin(snapshot.shotControl.yawRadians);
    expect(behind < -.9999, "candidate camera is behind authoritative shot intent");
    snapshot.shotControl.yawRadians = -1.24f;
    machine.update(0.0f, snapshot);
    expect(close(std::cos(machine.frame().pose.yawRadians),
        -std::cos(snapshot.shotControl.yawRadians)),
        "candidate camera follows semantic aim adjustment without changing intent");
    const auto saved = machine.frame();
    expect(machine.restoreFrame(saved), "candidate low frame restores for BIH return");
    machine.reset(SemanticState::AimOverhead, Preset::Overhead, snapshot);
    const auto overhead = machine.frame().pose;
    billiardgl::CameraPresentation c;
    c.eye = {overhead.eye.x,overhead.eye.y,overhead.eye.z};
    c.target = {overhead.target.x,overhead.target.y,overhead.target.z};
    c.fieldOfViewDegrees = overhead.fieldOfViewDegrees;
    const auto projection = billiardgl::render::buildSharedCameraProjection(c,1920,1080);
    double top=1080,bottom=0;
    for (float x : {-63.5f,63.5f}) for (float z : {-127.0f,127.0f}) {
        billiardgl::render::PhysicalScreenPoint screen;
        expect(billiardgl::render::clipToPhysicalScreen(projection,
            billiardgl::render::transformWorldToClip(projection,
                {x,billiardgl::kTableModelPlaySurfaceHeightCm,z}),screen),
            "candidate projects every playfield corner");
        top=std::min(top,screen.y);bottom=std::max(bottom,screen.y);
    }
    expect(top/1080>=.08 && top/1080<=.12 && (1080-bottom)/1080>=.08 &&
        (1080-bottom)/1080<=.12, "candidate overhead real projection has required end margins");
    StateMachine ordinary;
    ordinary.reset(SemanticState::AimPlayer,Preset::PlayerLow,snapshot);
    expect(close(ordinary.frame().pose.distanceCm,145),
        "ordinary camera remains unchanged without opt-in");
}

}  // namespace

int main()
{
    testDefinitionsAndTransitions();
    testOptInM4CameraCandidate();
    testProductionCompositionContracts();
    testFrameRateConsistencyAndSpeedBounds();
    testFineAimClampAndSnapshotImmutability();
    testRelativePresentationPreferencesAndExactCancel();
    testDynamicTrackingPreference();
    testSharedProjectionSupportsSettingsRange();
    testImpactShakeIsBoundedAndReversible();
    testExactFrameRestorePreservesCurrentPreferences();
    testLegacyShotEquivalence();
    testVersionedShotIntentAndCapabilities();
    testShotBoundsAndNoMutation();
    if (failures) {
        std::cerr << failures << " camera/shot model assertion(s) failed\n";
        return EXIT_FAILURE;
    }
    std::cout << "camera state machine and shot control model tests passed\n";
    return EXIT_SUCCESS;
}
