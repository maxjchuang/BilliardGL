#include "render_state.h"

#include <algorithm>
#include <cmath>

namespace billiardgl {
namespace {

static_assert(kPresentationBallCount == kBallCount,
    "presentation and authoritative ball counts must remain aligned");

float lerp(float from, float to, float alpha)
{
    return from + (to - from) * alpha;
}

Point3 lerpPoint(const Point3& from, const Point3& to, float alpha)
{
    return Point3{
        lerp(from.x, to.x, alpha),
        lerp(from.y, to.y, alpha),
        lerp(from.z, to.z, alpha)};
}

bool isStoredPocketBall(const BallState& ball)
{
    // Captured object balls are administratively moved to storage by physics.
    // That storage coordinate is not part of their visible pocket trajectory.
    return ball.pocketed &&
        ball.pocketInteraction.phase == PocketInteractionPhase::Captured;
}

bool sameContinuousBall(const BallState& previous, const BallState& current)
{
    return previous.pocketed == current.pocketed && previous.outOfPlay == current.outOfPlay &&
        previous.trajectoryGeneration == current.trajectoryGeneration &&
        isStoredPocketBall(previous) == isStoredPocketBall(current) &&
        previous.pocketInteraction.pocketId ==
            current.pocketInteraction.pocketId &&
        previous.pocketInteraction.captureSequence ==
            current.pocketInteraction.captureSequence;
}

PresentationPoint presentationPoint(const Point3& point)
{
    return PresentationPoint{point.x, point.y, point.z};
}

PresentationPoint presentationPoint(const float point[3])
{
    return PresentationPoint{point[0], point[1], point[2]};
}

void applyShotControlPresentation(PresentationSnapshot& snapshot,
                                  const ShotControlModel& model,
                                  const ShotControlCapabilities& capabilities)
{
    snapshot.shotControl.schemaVersion = static_cast<std::uint32_t>(model.schemaVersion);
    snapshot.shotControl.powerMode = static_cast<std::uint32_t>(model.powerMode);
    snapshot.shotControl.assistLevel = static_cast<std::uint32_t>(model.assistLevel);
    snapshot.shotControl.yawRadians = model.yawRadians;
    snapshot.shotControl.tipOffsetX = model.tipOffsetX;
    snapshot.shotControl.tipOffsetY = model.tipOffsetY;
    snapshot.shotControl.requestedPower = model.requestedPower;
    snapshot.shotControl.strokeSpeedCmS = model.strokeSpeedCmS;
    snapshot.shotControl.cueElevationDegrees = model.cueElevationDegrees;
    snapshot.shotControl.chalkIntent = static_cast<std::uint32_t>(model.chalk);

    snapshot.cue.shotPower = model.requestedPower;
    snapshot.cue.tipOffsetX = model.tipOffsetX;
    snapshot.cue.tipOffsetY = model.tipOffsetY;
    snapshot.cue.elevationDegrees = model.cueElevationDegrees;
    const float yaw = model.yawRadians;
    snapshot.cue.direction = PresentationPoint{std::cos(yaw), 0.0f, std::sin(yaw)};
    constexpr float kVisualTipRestGapCm = 1.0f;
    constexpr float kVisualPowerBackoffCmPerUnit = 0.025f;
    constexpr float kGuideLengthCm = 150.0f;
    const float tipBackoff = kBallRadius + kVisualTipRestGapCm +
        snapshot.cue.shotPower * kVisualPowerBackoffCmPerUnit;
    snapshot.cue.modelOriginBackoffCm = tipBackoff;
    const float guideStart = kBallRadius + kVisualTipRestGapCm;
    snapshot.cue.guideStart = PresentationPoint{
        snapshot.cue.direction.x * guideStart, 0.0f,
        snapshot.cue.direction.z * guideStart};
    snapshot.cue.guideEnd = PresentationPoint{
        snapshot.cue.direction.x * kGuideLengthCm, 0.0f,
        snapshot.cue.direction.z * kGuideLengthCm};
    snapshot.cue.rotationDegrees = 270.5f - yaw * 180.0f / kPi;

    snapshot.hud.shotPower = model.requestedPower;
    snapshot.hud.tipOffsetX = model.tipOffsetX;
    snapshot.hud.tipOffsetY = model.tipOffsetY;
    snapshot.hud.assistLevel = static_cast<std::uint32_t>(model.assistLevel);
    snapshot.hud.showCueElevation = capabilities.cueElevation &&
        model.schemaVersion == ShotControlSchemaVersion::IntentV2;
    snapshot.hud.showChalkState = capabilities.chalkState &&
        model.schemaVersion == ShotControlSchemaVersion::IntentV2;
}

GameState interpolatePresentationState(const GameState& previous,
    const GameState& current, double alpha)
{
    GameState rendered = current;
    const float fraction = static_cast<float>(
        std::clamp(alpha, 0.0, 1.0));
    // A collision makes the trajectory inside this physics tick piecewise
    // linear. Interpolating the two tick endpoints with one straight segment
    // can draw a ball through a cushion or another ball even though both
    // solved endpoints are valid. Present the post-solve positions for that
    // tick; ordinary motion remains interpolated below.
    const bool collisionTick = current.events.ballCollision ||
        current.events.railCollision;

    for (int index = 0; index < kBallCount; ++index) {
        const BallState& from = previous.balls[index];
        const BallState& to = current.balls[index];
        if (!sameContinuousBall(from, to)) continue;
        if (!collisionTick) {
            rendered.balls[index].position = lerpPoint(
                from.position, to.position, fraction);
        }
        rendered.balls[index].rotationAngle = lerp(
            from.rotationAngle, to.rotationAngle, fraction);
    }

    if (rendered.camera.anchorMode == CameraAnchorMode::FollowCueBall) {
        const Point3& cuePosition = rendered.balls[0].position;
        rendered.camera.target[0] = cuePosition.x;
        rendered.camera.target[1] = cuePosition.y;
        rendered.camera.target[2] = cuePosition.z;
        updateCameraEye(rendered);
    }

    return rendered;
}

}  // namespace

PresentationSnapshot buildPresentationSnapshot(
    const GameState& previous, const GameState& current, double alpha)
{
    const GameState rendered = interpolatePresentationState(
        previous, current, alpha);
    PresentationSnapshot snapshot;

    for (int index = 0; index < kPresentationBallCount; ++index) {
        const BallState& ball = rendered.balls[index];
        PresentationBall& output = snapshot.balls[index];
        output.identity = index;
        output.position = presentationPoint(ball.position);
        output.rotationAxis = presentationPoint(ball.rotationAxis);
        output.rotationAngleDegrees = ball.rotationAngle;
        output.legacyTexture = ball.texture;
        // Sparse scenarios use a pocketed ball in the Outside phase as an
        // inactive slot. A ball still descending through the throat remains
        // visible, but a completed capture's storage slot must never render.
        const bool inactiveScenarioSlot = ball.pocketed &&
            ball.pocketInteraction.phase == PocketInteractionPhase::Outside;
        output.visible = !ball.outOfPlay && !inactiveScenarioSlot &&
            !isStoredPocketBall(ball);
        output.castsShadow = !ball.pocketed && !ball.outOfPlay;
        output.pocketed = ball.pocketed;
    }

    snapshot.cue.visible = rendered.players.aimingAtCueBall;
    snapshot.cue.showGuideLine = rendered.aim.showGuideLine;
    snapshot.cue.shotPower = rendered.input.shotPower;
    const float yaw = rendered.aim.yaw;
    snapshot.cue.direction = PresentationPoint{
        std::cos(yaw), 0.0f, std::sin(yaw)};
    const PresentationPoint cueBall = snapshot.balls[0].position;
    constexpr float kVisualTipRestGapCm = 1.0f;
    constexpr float kVisualPowerBackoffCmPerUnit = 0.025f;
    constexpr float kGuideLengthCm = 150.0f;
    const float tipBackoff = kBallRadius + kVisualTipRestGapCm +
        snapshot.cue.shotPower * kVisualPowerBackoffCmPerUnit;
    snapshot.cue.referencePosition = cueBall;
    snapshot.cue.modelOriginBackoffCm = tipBackoff;
    const float guideStart = kBallRadius + kVisualTipRestGapCm;
    snapshot.cue.guideStart = PresentationPoint{
        snapshot.cue.direction.x * guideStart,
        0.0f,
        snapshot.cue.direction.z * guideStart};
    snapshot.cue.guideEnd = PresentationPoint{
        snapshot.cue.direction.x * kGuideLengthCm,
        0.0f,
        snapshot.cue.direction.z * kGuideLengthCm};
    snapshot.cue.rotationDegrees = 270.5f - yaw * 180.0f / kPi;

    snapshot.camera.eye = presentationPoint(rendered.camera.eye);
    snapshot.camera.target = presentationPoint(rendered.camera.target);
    snapshot.camera.projectionPan = PresentationPoint{
        rendered.camera.panX, rendered.camera.panY, rendered.camera.panZ};
    snapshot.camera.zoom = rendered.camera.zoom;
    snapshot.camera.viewportWidth = rendered.config.width;
    snapshot.camera.viewportHeight = rendered.config.height;
    snapshot.camera.followsCueBall =
        rendered.camera.anchorMode == CameraAnchorMode::FollowCueBall;

    snapshot.hud.currentPlayerNumber = rendered.players.currentPlayer + 1;
    snapshot.hud.shotPower = rendered.input.shotPower;
    snapshot.hud.viewportWidth = rendered.config.width;
    snapshot.hud.viewportHeight = rendered.config.height;
    snapshot.hud.aimMode = rendered.aim.mode == AimMode::Aim;
    snapshot.hud.followCamera = snapshot.camera.followsCueBall;
    snapshot.hud.showHelp = rendered.hud.showHelp;
    snapshot.hud.showPowerMeter = snapshot.hud.aimMode;

    snapshot.events.ballCollision = rendered.events.ballCollision;
    snapshot.events.railCollision = rendered.events.railCollision;
    snapshot.events.ballPocketed = rendered.events.ballPocketed;
    snapshot.events.cueBallPocketed = rendered.events.cueBallPocketed;
    snapshot.events.eightBallPocketed = rendered.events.eightBallPocketed;
    snapshot.events.shotEnded = rendered.events.shotEnded;
    snapshot.ballsMoving = rendered.ballsMoving;
    snapshot.gameOver = rendered.gameOver;
    applyShotControlPresentation(snapshot,
        legacyShotControls(rendered.aim.yaw, rendered.input.shotPower), {});
    return snapshot;
}

PresentationSnapshot buildPresentationSnapshot(
    const GameState& previous, const GameState& current, double alpha,
    const ShotControlModel& shotControl,
    const ShotControlCapabilities& shotCapabilities,
    const camera::Frame& cameraFrame)
{
    PresentationSnapshot snapshot = buildPresentationSnapshot(previous, current, alpha);
    applyShotControlPresentation(snapshot, shotControl, shotCapabilities);
    snapshot.camera.eye = PresentationPoint{
        cameraFrame.pose.eye.x, cameraFrame.pose.eye.y, cameraFrame.pose.eye.z};
    snapshot.camera.target = PresentationPoint{
        cameraFrame.pose.target.x, cameraFrame.pose.target.y, cameraFrame.pose.target.z};
    snapshot.camera.zoom = cameraFrame.pose.distanceCm;
    snapshot.camera.fieldOfViewDegrees = cameraFrame.pose.fieldOfViewDegrees;
    snapshot.camera.semanticState = static_cast<std::uint32_t>(cameraFrame.state);
    snapshot.camera.preset = static_cast<std::uint32_t>(cameraFrame.preset);
    snapshot.camera.transitioning = cameraFrame.transitioning;
    snapshot.camera.followsCueBall =
        camera::definition(cameraFrame.state).targetBehavior == camera::TargetBehavior::CueBall;
    return snapshot;
}

PresentationSnapshot buildPresentationSnapshot(
    const GameState& previous, const GameState& current, double alpha,
    const ShotControlModel& shotControl,
    const ShotControlCapabilities& shotCapabilities,
    const camera::Frame& cameraFrame,
    const RulesPresentation& rulesPresentation)
{
    PresentationSnapshot snapshot = buildPresentationSnapshot(previous, current,
        alpha, shotControl, shotCapabilities, cameraFrame);
    snapshot.rules = rulesPresentation;
    return snapshot;
}

}  // namespace billiardgl
