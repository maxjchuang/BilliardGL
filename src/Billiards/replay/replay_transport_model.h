#pragma once

#include "camera/camera_state_machine.h"
#include "replay/replay_v1.h"

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

namespace billiardgl::replay {

enum class ReplayCameraMode {
    Free,
    Broadcast
};

struct TrajectorySample {
    std::uint64_t tick = 0;
    Point3 position;
};

struct ReplayTransportSnapshot {
    bool loaded = false;
    bool paused = true;
    bool finished = false;
    int documentFormatVersion = 0;
    std::size_t rulesReceiptCount = 0;
    bool rulesEffectVerified = false;
    bool rackTerminalActive = false;
    RulesRackTerminalReason rackTerminalReason = RulesRackTerminalReason::None;
    int rackLosingPlayer = -1;
    int rackWinningPlayer = -1;
    std::string rackIdentity;
    std::string rackTerminalDecisionId;
    double playbackRate = 1.0;
    double fixedStepSeconds = 0.0;
    std::uint64_t tick = 0;
    ReplayCameraMode cameraMode = ReplayCameraMode::Free;
    bool cueEndpointVisible = false;
    Point3 cueEndpoint;
    int railCollisionEvents = 0;
    std::vector<TrajectorySample> cueTrajectory;
};

// UI-facing replay controls. Playback rate changes only the number of fixed
// simulation ticks scheduled per presented frame; it never changes dt.
class ReplayTransportModel {
public:
    ValidationResult load(const Document& document,
        const PhysicsProfile& currentProfile,
        const std::string& executableVersion,
        const std::string& contentSha256);
    ValidationResult setPaused(bool paused);
    ValidationResult setPlaybackRate(double rate);
    ValidationResult advancePresentedFrame();
    ValidationResult advancePresentedFrame(double elapsedSeconds);
    ValidationResult stepTick();
    ValidationResult seekShot(std::size_t shotOrdinal);
    ValidationResult setCameraMode(ReplayCameraMode mode);
    ValidationResult adjustCamera(float yaw, float pitch, float zoom);
    ValidationResult resetCamera();
    void setCameraPresentationPreferences(
        camera::PresentationPreferences preferences) noexcept;
    const camera::PresentationPreferences& cameraPresentationPreferences() const noexcept
    {
        return cameraPreferences_;
    }

    ReplayTransportSnapshot snapshot() const;
    camera::Frame presentationCameraFrame() const noexcept;
    const Player& player() const { return player_; }

private:
    ValidationResult stepAndCapture();
    void resetTrajectory();
    void captureCuePosition();

    Player player_;
    bool loaded_ = false;
    bool paused_ = true;
    double playbackRate_ = 1.0;
    double schedulingAccumulator_ = 0.0;
    ReplayCameraMode cameraMode_ = ReplayCameraMode::Free;
    camera::PresentationPreferences cameraPreferences_;
    camera::StateMachine freeCamera_;
    camera::StateMachine broadcastCamera_;
    std::uint64_t lastEventSequence_ = 0;
    int railCollisionEvents_ = 0;
    std::vector<TrajectorySample> cueTrajectory_;
};

}  // namespace billiardgl::replay
