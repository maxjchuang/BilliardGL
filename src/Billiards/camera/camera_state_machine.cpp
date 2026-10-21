#include "camera/camera_state_machine.h"

#include "table_specs.h"

#include <algorithm>
#include <array>
#include <cmath>

namespace billiardgl { namespace camera {
namespace {

constexpr float kPi = 3.14159265358979323846f;
constexpr float kMinimumPitch = 8.0f * kPi / 180.0f;
constexpr float kMaximumPitch = 89.0f * kPi / 180.0f;
constexpr float kMinimumFovOffsetDegrees = -20.0f;
constexpr float kMaximumFovOffsetDegrees = 35.0f;
constexpr float kMinimumDistanceScale = 10.0f / 180.0f;
constexpr float kMaximumDistanceScale = 500.0f / 180.0f;

const std::array<StateDefinition, 6> kStates{{
    {SemanticState::ObserveFree, Preset::Elevated, TargetBehavior::Free,
     48.0f, 80.0f, 420.0f, 0.45f, 0.35f, 260.0f, 360.0f, 3.2f, 90.0f,
     {true, true, true, false}},
    {SemanticState::AimPlayer, Preset::PlayerLow, TargetBehavior::CueBall,
     42.0f, 95.0f, 210.0f, 0.38f, 0.32f, 180.0f, 260.0f, 2.8f, 90.0f,
     {false, false, true, true}},
    {SemanticState::AimOverhead, Preset::Overhead, TargetBehavior::TableCenter,
     40.0f, 220.0f, 380.0f, 0.42f, 0.34f, 180.0f, 280.0f, 2.8f, 90.0f,
     {false, false, true, true}},
    {SemanticState::ShotFollow, Preset::Elevated, TargetBehavior::CueBall,
     50.0f, 120.0f, 330.0f, 0.32f, 0.28f, 300.0f, 360.0f, 3.5f, 120.0f,
     {false, false, false, false}},
    {SemanticState::ReplayFree, Preset::Elevated, TargetBehavior::Free,
     48.0f, 70.0f, 500.0f, 0.45f, 0.35f, 320.0f, 420.0f, 3.6f, 120.0f,
     {true, true, true, false}},
    {SemanticState::ReplayBroadcast, Preset::Broadcast, TargetBehavior::TableCenter,
     45.0f, 180.0f, 420.0f, 0.55f, 0.45f, 220.0f, 320.0f, 2.5f, 75.0f,
     {false, false, false, false}},
}};

const std::array<PresetDefinition, 4> kPresets{{
    {Preset::PlayerLow, -1.5707963f, 20.0f * kPi / 180.0f, 145.0f},
    {Preset::Elevated, -1.5707963f, 42.0f * kPi / 180.0f, 220.0f},
    {Preset::Overhead, -1.5707963f, 88.0f * kPi / 180.0f, 380.0f},
    // The broadcast camera stays below the 221 cm lamp plane while retaining
    // the venue and full table angle.  Other presets keep the authored lamp.
    {Preset::Broadcast, -0.96f, 30.0f * kPi / 180.0f, 255.0f},
}};

float clamp(float value, float minimum, float maximum)
{
    return std::max(minimum, std::min(maximum, value));
}

float smoothstep(float value)
{
    const float t = clamp(value, 0.0f, 1.0f);
    return t * t * (3.0f - 2.0f * t);
}

float wrapAngle(float value)
{
    while (value > kPi) value -= 2.0f * kPi;
    while (value < -kPi) value += 2.0f * kPi;
    return value;
}

float angleDelta(float from, float to)
{
    return wrapAngle(to - from);
}

float lerp(float from, float to, float alpha)
{
    return from + (to - from) * alpha;
}

Vec3 lerp(const Vec3& from, const Vec3& to, float alpha)
{
    return {lerp(from.x, to.x, alpha), lerp(from.y, to.y, alpha),
            lerp(from.z, to.z, alpha)};
}

float distance(const Vec3& a, const Vec3& b)
{
    return std::sqrt((a.x - b.x) * (a.x - b.x) +
                     (a.y - b.y) * (a.y - b.y) +
                     (a.z - b.z) * (a.z - b.z));
}

Vec3 moveToward(const Vec3& from, const Vec3& to, float maximumDelta)
{
    const float length = distance(from, to);
    if (length <= maximumDelta || length <= 0.000001f) return to;
    const float scale = maximumDelta / length;
    return {from.x + (to.x - from.x) * scale,
            from.y + (to.y - from.y) * scale,
            from.z + (to.z - from.z) * scale};
}

void updateEye(Pose& pose)
{
    const float horizontal = std::cos(pose.pitchRadians) * pose.distanceCm;
    pose.eye = {
        pose.target.x + std::cos(pose.yawRadians) * horizontal,
        pose.target.y + std::sin(pose.pitchRadians) * pose.distanceCm,
        pose.target.z + std::sin(pose.yawRadians) * horizontal,
    };
}

Vec3 cueBallTarget(const PresentationSnapshot& snapshot)
{
    const PresentationPoint& cue = snapshot.balls[0].position;
    return {cue.x, cue.y, cue.z};
}

}  // namespace

const StateDefinition& definition(SemanticState state) noexcept
{
    return kStates[static_cast<std::size_t>(state)];
}

const PresetDefinition& definition(Preset preset) noexcept
{
    return kPresets[static_cast<std::size_t>(preset)];
}

StateMachine::StateMachine()
{
    PresentationSnapshot empty;
    reset(SemanticState::ObserveFree, Preset::Elevated, empty);
}

Pose StateMachine::authoredPoseFor(SemanticState state, Preset preset,
                                   const PresentationSnapshot& snapshot) const noexcept
{
    const StateDefinition& stateDefinition = definition(state);
    const PresetDefinition& presetDefinition = definition(preset);
    Pose pose;
    pose.yawRadians = presetDefinition.yawRadians;
    pose.pitchRadians = presetDefinition.pitchRadians;
    pose.distanceCm = presetDefinition.distanceCm;
    pose.fieldOfViewDegrees = stateDefinition.fieldOfViewDegrees;
    if (preferences_.m4CameraCandidate && state == SemanticState::AimOverhead)
        pose.fieldOfViewDegrees = 46.0f;
    if (preferences_.m4CameraCandidate && state == SemanticState::AimPlayer &&
        preset == Preset::PlayerLow) {
        pose.distanceCm = 70.0f;
        pose.pitchRadians = 8.0f * kPi / 180.0f;
        pose.yawRadians = snapshot.shotControl.yawRadians + kPi;
    }
    if (stateDefinition.targetBehavior == TargetBehavior::CueBall)
        pose.target = cueBallTarget(snapshot);
    else if (stateDefinition.targetBehavior == TargetBehavior::TableCenter)
        pose.target = {0.0f, kTableModelPlaySurfaceHeightCm, 0.0f};
    else
        pose.target = authoredPose_.target;
    clampSpatialPose(pose);
    updateEye(pose);
    return pose;
}

void StateMachine::clampSpatialPose(Pose& pose) const noexcept
{
    pose.target.x = clamp(pose.target.x, -kChinesePlayfieldWidthCm * 0.5f,
                          kChinesePlayfieldWidthCm * 0.5f);
    pose.target.z = clamp(pose.target.z, -kChinesePlayfieldLengthCm * 0.5f,
                          kChinesePlayfieldLengthCm * 0.5f);
    pose.target.y = clamp(pose.target.y, kTableModelPlaySurfaceHeightCm - 10.0f,
                          kTableModelPlaySurfaceHeightCm + 30.0f);
    pose.pitchRadians = clamp(pose.pitchRadians, kMinimumPitch, kMaximumPitch);
    pose.yawRadians = wrapAngle(pose.yawRadians);
}

Pose StateMachine::displayedPose(const Pose& authored, SemanticState state) const noexcept
{
    Pose displayed = authored;
    const StateDefinition& stateDefinition = definition(state);
    displayed.distanceCm = clamp(authored.distanceCm * preferences_.distanceScale,
                                 preferences_.m4CameraCandidate && state == SemanticState::AimPlayer ?
                                     60.0f : stateDefinition.minimumDistanceCm,
                                 stateDefinition.maximumDistanceCm);
    displayed.fieldOfViewDegrees = clamp(
        authored.fieldOfViewDegrees + preferences_.fieldOfViewOffsetDegrees,
        20.0f, 90.0f);
    clampSpatialPose(displayed);
    updateEye(displayed);
    return displayed;
}

void StateMachine::refreshDisplayedPose() noexcept
{
    if (!frame_.transitioning) {
        frame_.pose = displayedPose(authoredPose_, frame_.state);
        return;
    }
    const float linearAlpha = transitionDuration_ > 0.0f
        ? clamp(transitionElapsed_ / transitionDuration_, 0.0f, 1.0f)
        : 1.0f;
    const float alpha = smoothstep(linearAlpha);
    const Pose from = displayedPose(transitionStart_, transitionStartState_);
    const Pose to = displayedPose(transitionGoal_, frame_.state);
    frame_.pose.target = lerp(from.target, to.target, alpha);
    frame_.pose.distanceCm = lerp(from.distanceCm, to.distanceCm, alpha);
    frame_.pose.yawRadians = wrapAngle(
        from.yawRadians + angleDelta(from.yawRadians, to.yawRadians) * alpha);
    frame_.pose.pitchRadians = lerp(from.pitchRadians, to.pitchRadians, alpha);
    frame_.pose.fieldOfViewDegrees = lerp(
        from.fieldOfViewDegrees, to.fieldOfViewDegrees, alpha);
    updateEye(frame_.pose);
}

void StateMachine::setPresentationPreferences(PresentationPreferences preferences) noexcept
{
    if (!std::isfinite(preferences.fieldOfViewOffsetDegrees))
        preferences.fieldOfViewOffsetDegrees = 0.0f;
    if (!std::isfinite(preferences.distanceScale))
        preferences.distanceScale = 1.0f;
    preferences.fieldOfViewOffsetDegrees = clamp(
        preferences.fieldOfViewOffsetDegrees,
        kMinimumFovOffsetDegrees, kMaximumFovOffsetDegrees);
    preferences.distanceScale = clamp(
        preferences.distanceScale,
        kMinimumDistanceScale, kMaximumDistanceScale);
    preferences_ = preferences;
    if (!preferences_.impactShakeEnabled) impactAmplitudeCm_ = 0.0f;
    refreshDisplayedPose();
}

void StateMachine::clearTransientEffects() noexcept
{
    impactAmplitudeCm_ = impactPhaseRadians_ = 0.0f;
    refreshDisplayedPose();
}

void StateMachine::reset(SemanticState state, Preset preset,
                         const PresentationSnapshot& snapshot) noexcept
{
    impactAmplitudeCm_ = impactPhaseRadians_ = 0.0f;
    frame_.state = state;
    frame_.preset = preset;
    frame_.input = definition(state).input;
    authoredPose_ = authoredPoseFor(state, preset, snapshot);
    frame_.transitioning = false;
    frame_.transitionAlpha = 1.0f;
    transitionStart_ = transitionGoal_ = authoredPose_;
    transitionStartState_ = state;
    transitionElapsed_ = transitionDuration_ = 0.0f;
    refreshDisplayedPose();
}

void StateMachine::transitionTo(SemanticState state, Preset preset,
                                const PresentationSnapshot& snapshot,
                                bool preserveOrientation) noexcept
{
    transitionStart_ = authoredPose_;
    transitionStartState_ = frame_.state;
    const float exitSeconds = definition(frame_.state).exitTransitionSeconds;
    frame_.state = state;
    frame_.preset = preset;
    frame_.input = definition(state).input;
    transitionGoal_ = authoredPoseFor(state, preset, snapshot);
    if (preserveOrientation) {
        transitionGoal_.yawRadians = transitionStart_.yawRadians;
        if (!(preferences_.m4CameraCandidate && state == SemanticState::AimPlayer))
            transitionGoal_.pitchRadians = transitionStart_.pitchRadians;
        else
            transitionGoal_.yawRadians = snapshot.shotControl.yawRadians + kPi;
        updateEye(transitionGoal_);
    }
    if (!preferences_.dynamicTrackingEnabled) {
        authoredPose_ = transitionGoal_;
        frame_.transitioning = false;
        frame_.transitionAlpha = 1.0f;
        transitionStart_ = transitionGoal_ = authoredPose_;
        transitionStartState_ = state;
        transitionElapsed_ = transitionDuration_ = 0.0f;
        refreshDisplayedPose();
        return;
    }
    const StateDefinition& target = definition(state);
    const Pose displayedStart = displayedPose(transitionStart_, transitionStartState_);
    const Pose displayedGoal = displayedPose(transitionGoal_, state);
    const float targetSeconds = 1.5f * distance(displayedStart.target, displayedGoal.target) /
                                std::max(target.maximumTargetSpeedCmS, 0.001f);
    const float distanceSeconds = 1.5f * std::fabs(displayedGoal.distanceCm - displayedStart.distanceCm) /
                                  std::max(target.maximumDistanceSpeedCmS, 0.001f);
    const float angleRadians = std::max(std::fabs(angleDelta(transitionStart_.yawRadians, transitionGoal_.yawRadians)),
                                        std::fabs(transitionGoal_.pitchRadians - transitionStart_.pitchRadians));
    const float angleSeconds = 1.5f * angleRadians /
                               std::max(target.maximumAngularSpeedRadiansS, 0.001f);
    const float fovSeconds = 1.5f * std::fabs(displayedGoal.fieldOfViewDegrees - displayedStart.fieldOfViewDegrees) /
                             std::max(target.maximumFovSpeedDegreesS, 0.001f);
    transitionDuration_ = std::max({target.enterTransitionSeconds, exitSeconds, targetSeconds,
                                    distanceSeconds, angleSeconds, fovSeconds});
    transitionElapsed_ = 0.0f;
    frame_.transitioning = transitionDuration_ > 0.0f;
    frame_.transitionAlpha = frame_.transitioning ? 0.0f : 1.0f;
    refreshDisplayedPose();
}

void StateMachine::observeFromCurrentPose() noexcept
{
    frame_.state = SemanticState::ObserveFree;
    frame_.input = definition(frame_.state).input;
    frame_.transitioning = false;
    frame_.transitionAlpha = 1.0f;
    authoredPose_ = frame_.pose;
    authoredPose_.distanceCm = frame_.pose.distanceCm / preferences_.distanceScale;
    authoredPose_.fieldOfViewDegrees =
        frame_.pose.fieldOfViewDegrees - preferences_.fieldOfViewOffsetDegrees;
    transitionStart_ = transitionGoal_ = authoredPose_;
    transitionStartState_ = frame_.state;
    transitionElapsed_ = transitionDuration_ = 0.0f;
}

bool StateMachine::restoreFrame(const Frame& saved) noexcept
{
    const auto finiteVec = [](const Vec3& value) {
        return std::isfinite(value.x) && std::isfinite(value.y) &&
            std::isfinite(value.z);
    };
    if (static_cast<unsigned>(saved.state) >= kStates.size() ||
        static_cast<unsigned>(saved.preset) >= kPresets.size() ||
        !finiteVec(saved.pose.eye) || !finiteVec(saved.pose.target) ||
        !std::isfinite(saved.pose.yawRadians) ||
        !std::isfinite(saved.pose.pitchRadians) ||
        !std::isfinite(saved.pose.distanceCm) ||
        !std::isfinite(saved.pose.fieldOfViewDegrees))
        return false;
    const StateDefinition& state = definition(saved.state);
    if (saved.pose.target.x < -kChinesePlayfieldWidthCm * 0.5f ||
        saved.pose.target.x > kChinesePlayfieldWidthCm * 0.5f ||
        saved.pose.target.z < -kChinesePlayfieldLengthCm * 0.5f ||
        saved.pose.target.z > kChinesePlayfieldLengthCm * 0.5f ||
        saved.pose.target.y < kTableModelPlaySurfaceHeightCm - 10.0f ||
        saved.pose.target.y > kTableModelPlaySurfaceHeightCm + 30.0f ||
        saved.pose.pitchRadians < kMinimumPitch ||
        saved.pose.pitchRadians > kMaximumPitch ||
        saved.pose.distanceCm < (preferences_.m4CameraCandidate && saved.state == SemanticState::AimPlayer ?
            60.0f : state.minimumDistanceCm) ||
        saved.pose.distanceCm > state.maximumDistanceCm ||
        saved.pose.fieldOfViewDegrees < 20.0f ||
        saved.pose.fieldOfViewDegrees > 90.0f)
        return false;

    Pose authored = saved.pose;
    authored.distanceCm = saved.pose.distanceCm / preferences_.distanceScale;
    authored.fieldOfViewDegrees = saved.pose.fieldOfViewDegrees -
        preferences_.fieldOfViewOffsetDegrees;
    clampSpatialPose(authored);
    const Pose restored = displayedPose(authored, saved.state);
    const auto close = [](float first, float second) {
        return std::fabs(first - second) <= 0.001f;
    };
    if (!close(restored.target.x, saved.pose.target.x) ||
        !close(restored.target.y, saved.pose.target.y) ||
        !close(restored.target.z, saved.pose.target.z) ||
        !close(restored.yawRadians, saved.pose.yawRadians) ||
        !close(restored.pitchRadians, saved.pose.pitchRadians) ||
        !close(restored.distanceCm, saved.pose.distanceCm) ||
        !close(restored.fieldOfViewDegrees,
            saved.pose.fieldOfViewDegrees) ||
        !close(restored.eye.x, saved.pose.eye.x) ||
        !close(restored.eye.y, saved.pose.eye.y) ||
        !close(restored.eye.z, saved.pose.eye.z))
        return false;

    impactAmplitudeCm_ = impactPhaseRadians_ = 0.0f;
    frame_ = saved;
    frame_.input = state.input;
    frame_.transitioning = false;
    frame_.transitionAlpha = 1.0f;
    authoredPose_ = authored;
    transitionStart_ = transitionGoal_ = authoredPose_;
    transitionStartState_ = saved.state;
    transitionElapsed_ = transitionDuration_ = 0.0f;
    refreshDisplayedPose();
    return true;
}

Frame StateMachine::update(float deltaSeconds, const PresentationSnapshot& snapshot,
                           const UpdateInput& input) noexcept
{
    const float dt = clamp(deltaSeconds, 0.0f, 0.25f);
    const StateDefinition& state = definition(frame_.state);
    const bool transitionStep = frame_.transitioning;
    if (preferences_.m4CameraCandidate && frame_.state == SemanticState::AimPlayer &&
        std::isfinite(snapshot.shotControl.yawRadians)) {
        if (transitionStep)
            transitionGoal_.yawRadians = wrapAngle(snapshot.shotControl.yawRadians + kPi);
        else
            authoredPose_.yawRadians = wrapAngle(snapshot.shotControl.yawRadians + kPi);
    }
    if (transitionStep) {
        transitionElapsed_ = std::min(transitionElapsed_ + dt, transitionDuration_);
        const float linearAlpha = transitionDuration_ > 0.0f ? transitionElapsed_ / transitionDuration_ : 1.0f;
        const float alpha = smoothstep(linearAlpha);
        authoredPose_.target = lerp(transitionStart_.target, transitionGoal_.target, alpha);
        authoredPose_.distanceCm = lerp(transitionStart_.distanceCm, transitionGoal_.distanceCm, alpha);
        authoredPose_.yawRadians = wrapAngle(transitionStart_.yawRadians +
                                           angleDelta(transitionStart_.yawRadians, transitionGoal_.yawRadians) * alpha);
        authoredPose_.pitchRadians = lerp(transitionStart_.pitchRadians, transitionGoal_.pitchRadians, alpha);
        authoredPose_.fieldOfViewDegrees = lerp(transitionStart_.fieldOfViewDegrees,
                                              transitionGoal_.fieldOfViewDegrees, alpha);
        frame_.transitionAlpha = linearAlpha;
        frame_.transitioning = transitionElapsed_ < transitionDuration_;
        if (frame_.transitioning) {
            refreshDisplayedPose();
        } else {
            authoredPose_ = transitionGoal_;
            refreshDisplayedPose();
        }
    } else {
        const bool lockForFineAim = input.fineAimActive && state.input.changeAim;
        if (!lockForFineAim && preferences_.dynamicTrackingEnabled) {
            Vec3 automaticTarget = authoredPose_.target;
            if (state.targetBehavior == TargetBehavior::CueBall)
                automaticTarget = cueBallTarget(snapshot);
            else if (state.targetBehavior == TargetBehavior::TableCenter)
                automaticTarget = {0.0f, kTableModelPlaySurfaceHeightCm, 0.0f};
            authoredPose_.target = moveToward(authoredPose_.target, automaticTarget,
                                            state.maximumTargetSpeedCmS * dt);
        }
        if (state.input.orbit) {
            authoredPose_.yawRadians += input.orbitYawDeltaRadians;
            authoredPose_.pitchRadians += input.orbitPitchDeltaRadians;
        }
        if (state.input.pan) {
            authoredPose_.target.x += input.panXDeltaCm;
            authoredPose_.target.z += input.panZDeltaCm;
        }
        if (state.input.zoom) {
            const float displayedDistance = clamp(
                frame_.pose.distanceCm + input.zoomDeltaCm,
                preferences_.m4CameraCandidate && frame_.state == SemanticState::AimPlayer ?
                    60.0f : state.minimumDistanceCm, state.maximumDistanceCm);
            authoredPose_.distanceCm = displayedDistance / preferences_.distanceScale;
        }
        clampSpatialPose(authoredPose_);
        refreshDisplayedPose();
        frame_.transitionAlpha = 1.0f;
    }
    // Both transition endpoints are clamped for their respective states.
    // Clamping their interpolation to the destination state's distance range
    // would introduce a first-frame jump when ranges do not overlap.
    // Only perturb the derived eye, never the authored pose, aim or simulation.
    // No impulses are consumed by dt=0 input updates; fine aim and disabling
    // the setting immediately return the exact unperturbed camera.
    if (!preferences_.impactShakeEnabled || input.fineAimActive) {
        impactAmplitudeCm_ = 0.0f;
    } else if (std::isfinite(deltaSeconds) && deltaSeconds > 0.0f) {
        impactAmplitudeCm_ *= std::exp(-18.0f * dt);
        if (std::isfinite(input.impactStrength))
            impactAmplitudeCm_ = std::max(impactAmplitudeCm_,
                0.35f * clamp(input.impactStrength, 0.0f, 1.0f));
        impactPhaseRadians_ = std::fmod(impactPhaseRadians_ + 70.0f * dt, 6.2831853f);
        if (impactAmplitudeCm_ < 0.0001f) impactAmplitudeCm_ = 0.0f;
    }
    const float offset = impactAmplitudeCm_ * std::sin(impactPhaseRadians_);
    frame_.pose.eye.x += offset * std::sin(frame_.pose.yawRadians);
    frame_.pose.eye.z -= offset * std::cos(frame_.pose.yawRadians);
    return frame_;
}

} }  // namespace billiardgl::camera
