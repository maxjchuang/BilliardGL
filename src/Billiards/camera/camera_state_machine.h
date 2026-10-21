#pragma once

#include "presentation_snapshot.h"

#include <cstdint>

namespace billiardgl { namespace camera {

enum class SemanticState : std::uint8_t {
    ObserveFree,
    AimPlayer,
    AimOverhead,
    ShotFollow,
    ReplayFree,
    ReplayBroadcast,
};

enum class Preset : std::uint8_t {
    PlayerLow,
    Elevated,
    Overhead,
    Broadcast,
};

enum class TargetBehavior : std::uint8_t {
    Free,
    CueBall,
    TableCenter,
};

struct InputPermissions {
    bool orbit = false;
    bool pan = false;
    bool zoom = false;
    bool changeAim = false;
};

struct StateDefinition {
    SemanticState state = SemanticState::ObserveFree;
    Preset defaultPreset = Preset::Elevated;
    TargetBehavior targetBehavior = TargetBehavior::Free;
    float fieldOfViewDegrees = 48.0f;
    float minimumDistanceCm = 80.0f;
    float maximumDistanceCm = 420.0f;
    float enterTransitionSeconds = 0.45f;
    float exitTransitionSeconds = 0.35f;
    float maximumTargetSpeedCmS = 260.0f;
    float maximumDistanceSpeedCmS = 360.0f;
    float maximumAngularSpeedRadiansS = 3.2f;
    float maximumFovSpeedDegreesS = 90.0f;
    InputPermissions input;
};

struct PresetDefinition {
    Preset preset = Preset::Elevated;
    float yawRadians = -1.5707963f;
    float pitchRadians = 0.75f;
    float distanceCm = 220.0f;
};

struct Vec3 {
    float x = 0.0f;
    float y = 0.0f;
    float z = 0.0f;
};

struct Pose {
    Vec3 eye;
    Vec3 target;
    float yawRadians = -1.5707963f;
    float pitchRadians = 0.75f;
    float distanceCm = 220.0f;
    float fieldOfViewDegrees = 48.0f;
};

// Presentation preferences are deliberately relative to the authored camera
// compositions.  The product setting must present these as a signed FOV
// adjustment and a distance percentage, rather than implying that every
// semantic camera uses one absolute FOV or distance.
//
// Defaults are identity.  The accepted ranges preserve compatibility with the
// existing 35..90 degree and 10..500 cm settings around their historical
// 55 degree / 180 cm defaults.
struct PresentationPreferences {
    float fieldOfViewOffsetDegrees = 0.0f;  // [-20, +35]
    float distanceScale = 1.0f;             // [10/180, 500/180]
    bool dynamicTrackingEnabled = true;
    bool impactShakeEnabled = false;
    bool m4CameraCandidate = false; // opt-in presentation only; never persisted
};

struct UpdateInput {
    float orbitYawDeltaRadians = 0.0f;
    float orbitPitchDeltaRadians = 0.0f;
    float panXDeltaCm = 0.0f;
    float panZDeltaCm = 0.0f;
    float zoomDeltaCm = 0.0f;
    bool fineAimActive = false;
    // Presentation-only impulse, supplied once per committed physics update.
    float impactStrength = 0.0f;
};

struct Frame {
    SemanticState state = SemanticState::ObserveFree;
    Preset preset = Preset::Elevated;
    Pose pose;
    InputPermissions input;
    bool transitioning = false;
    float transitionAlpha = 1.0f;
};

const StateDefinition& definition(SemanticState state) noexcept;
const PresetDefinition& definition(Preset preset) noexcept;

class StateMachine {
public:
    StateMachine();

    void reset(SemanticState state, Preset preset,
               const PresentationSnapshot& snapshot) noexcept;
    void transitionTo(SemanticState state, Preset preset,
                      const PresentationSnapshot& snapshot,
                      bool preserveOrientation = false) noexcept;
    Frame update(float deltaSeconds, const PresentationSnapshot& snapshot,
                 const UpdateInput& input = {}) noexcept;

    // Re-evaluates the displayed optics from an unclamped authored pose.
    // Keeping that authored pose separate makes preview -> cancel exact even
    // if the previewed distance was clipped by a semantic state's limits.
    void setPresentationPreferences(PresentationPreferences preferences) noexcept;
    const PresentationPreferences& presentationPreferences() const noexcept
    {
        return preferences_;
    }

    const Frame& frame() const noexcept { return frame_; }
    // Remove presentation-only impulses without resetting the user's orbit.
    void clearTransientEffects() noexcept;
    // End an automatic/aim transition without changing the displayed pose.
    // ObserveFree has no automatic target tracking and permits manual orbit.
    void observeFromCurrentPose() noexcept;
    // Restores a previously emitted product frame without changing the
    // current presentation preferences. Invalid or out-of-range frames are
    // rejected atomically.
    bool restoreFrame(const Frame& frame) noexcept;

private:
    Pose authoredPoseFor(SemanticState state, Preset preset,
                         const PresentationSnapshot& snapshot) const noexcept;
    Pose displayedPose(const Pose& authored, SemanticState state) const noexcept;
    void clampSpatialPose(Pose& pose) const noexcept;
    void refreshDisplayedPose() noexcept;

    Frame frame_;
    Pose authoredPose_;
    Pose transitionStart_;
    Pose transitionGoal_;
    SemanticState transitionStartState_ = SemanticState::ObserveFree;
    float transitionElapsed_ = 0.0f;
    float transitionDuration_ = 0.0f;
    PresentationPreferences preferences_;
    float impactAmplitudeCm_ = 0.0f;
    float impactPhaseRadians_ = 0.0f;
};

} }  // namespace billiardgl::camera
