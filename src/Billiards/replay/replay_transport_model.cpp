#include "replay/replay_transport_model.h"

#include <cmath>
#include <algorithm>

namespace billiardgl::replay {

ValidationResult ReplayTransportModel::load(const Document& document,
    const PhysicsProfile& currentProfile, const std::string& executableVersion,
    const std::string& contentSha256)
{
    const ValidationResult result = player_.load(document, currentProfile,
        executableVersion, contentSha256);
    if (!result.ok) return result;
    loaded_ = true;
    paused_ = true;
    playbackRate_ = 1.0;
    schedulingAccumulator_ = 0.0;
    cameraMode_ = ReplayCameraMode::Free;
    player_.setPaused(true);
    resetTrajectory();
    resetCamera();
    return {true, ""};
}

ValidationResult ReplayTransportModel::resetCamera()
{
    if (!loaded_) return {false, "replay_not_loaded"};
    PresentationSnapshot target;
    const auto& cue = player_.runtime().state().balls[0].position;
    target.balls[0].position = {cue.x, cue.y, cue.z};
    freeCamera_.reset(camera::SemanticState::ReplayFree, camera::Preset::Elevated, target);
    broadcastCamera_.reset(camera::SemanticState::ReplayBroadcast,
        camera::Preset::Broadcast, target);
    cameraMode_ = ReplayCameraMode::Free;
    return {true, ""};
}

void ReplayTransportModel::setCameraPresentationPreferences(
    camera::PresentationPreferences preferences) noexcept
{
    freeCamera_.setPresentationPreferences(preferences);
    cameraPreferences_ = freeCamera_.presentationPreferences();
    broadcastCamera_.setPresentationPreferences(cameraPreferences_);
}

ValidationResult ReplayTransportModel::adjustCamera(float yaw, float pitch, float zoom)
{
    if (!loaded_) return {false, "replay_not_loaded"};
    if (!std::isfinite(yaw) || !std::isfinite(pitch) || !std::isfinite(zoom))
        return {false, "replay_camera_input_invalid"};
    cameraMode_ = ReplayCameraMode::Free;
    camera::UpdateInput input;
    input.orbitYawDeltaRadians = yaw;
    input.orbitPitchDeltaRadians = pitch;
    input.zoomDeltaCm = zoom;
    freeCamera_.update(0.0f, PresentationSnapshot{}, input);
    return {true, ""};
}

ValidationResult ReplayTransportModel::setPaused(bool paused)
{
    if (!loaded_) return {false, "replay_not_loaded"};
    paused_ = paused;
    player_.setPaused(paused);
    if (paused) {
        freeCamera_.clearTransientEffects();
        broadcastCamera_.clearTransientEffects();
    }
    return {true, ""};
}

ValidationResult ReplayTransportModel::setPlaybackRate(double rate)
{
    if (!loaded_) return {false, "replay_not_loaded"};
    const ValidationResult result = player_.setPlaybackRate(rate);
    if (!result.ok) return result;
    playbackRate_ = rate;
    return {true, ""};
}

ValidationResult ReplayTransportModel::advancePresentedFrame()
{
    if (!loaded_) return {false, "replay_not_loaded"};
    return advancePresentedFrame(
        player_.runtime().physicsProfile().solver.timeStepSeconds);
}

ValidationResult ReplayTransportModel::advancePresentedFrame(
    double elapsedSeconds)
{
    if (!loaded_) return {false, "replay_not_loaded"};
    if (!std::isfinite(elapsedSeconds) || elapsedSeconds < 0.0)
        return {false, "replay_presented_frame_elapsed_invalid"};
    if (player_.finished() && !player_.finalHashMatches())
        return {false, "replay_final_hash_mismatch"};
    if (paused_) return {true, ""};
    const double fixedStepSeconds =
        player_.runtime().physicsProfile().solver.timeStepSeconds;
    if (!std::isfinite(fixedStepSeconds) || fixedStepSeconds <= 0.0)
        return {false, "replay_fixed_step_invalid"};
    schedulingAccumulator_ +=
        elapsedSeconds * playbackRate_ / fixedStepSeconds;
    while (schedulingAccumulator_ >= 1.0 && !player_.finished()) {
        const ValidationResult result = stepAndCapture();
        if (!result.ok) return result;
        schedulingAccumulator_ -= 1.0;
    }
    return {true, ""};
}

ValidationResult ReplayTransportModel::stepTick()
{
    if (!loaded_) return {false, "replay_not_loaded"};
    return stepAndCapture();
}

ValidationResult ReplayTransportModel::seekShot(std::size_t shotOrdinal)
{
    if (!loaded_) return {false, "replay_not_loaded"};
    const ValidationResult result = player_.seekShot(shotOrdinal);
    if (!result.ok) return result;
    schedulingAccumulator_ = 0.0;
    resetTrajectory();
    freeCamera_.clearTransientEffects();
    broadcastCamera_.clearTransientEffects();
    return {true, ""};
}

ValidationResult ReplayTransportModel::setCameraMode(ReplayCameraMode mode)
{
    if (!loaded_) return {false, "replay_not_loaded"};
    cameraMode_ = mode;
    return {true, ""};
}

ValidationResult ReplayTransportModel::stepAndCapture()
{
    const auto previousTick = player_.runtime().tick();
    const ValidationResult result = player_.stepTick();
    if (!result.ok) return result;
    float impactStrength = 0.0f;
    for (const RuntimeEvent& event :
            player_.runtime().eventsSince(lastEventSequence_)) {
        if (event.name == "rail_collision") ++railCollisionEvents_;
        if (event.name == "rail_collision")
            impactStrength = std::max(impactStrength, 0.6f);
        if (event.name == "ball_collision") impactStrength = 1.0f;
        lastEventSequence_ = event.sequence;
    }
    // Consume committed events exactly once per simulation tick, not once per
    // draw/readback. Manual orbit remains independent of timeline seeking.
    if (player_.runtime().tick() != previousTick) {
        camera::UpdateInput input;
        input.impactStrength = impactStrength;
        const float dt = player_.runtime().physicsProfile().solver.timeStepSeconds;
        freeCamera_.update(dt, PresentationSnapshot{}, input);
        broadcastCamera_.update(dt, PresentationSnapshot{}, input);
    }
    if (player_.finished()) {
        freeCamera_.clearTransientEffects();
        broadcastCamera_.clearTransientEffects();
    }
    captureCuePosition();
    return {true, ""};
}

void ReplayTransportModel::resetTrajectory()
{
    cueTrajectory_.clear();
    railCollisionEvents_ = 0;
    lastEventSequence_ = player_.runtime().events().empty() ? 0 :
        player_.runtime().events().back().sequence;
    captureCuePosition();
}

void ReplayTransportModel::captureCuePosition()
{
    const BallState& cue = player_.runtime().state().balls[0];
    if (cue.pocketed) return;
    cueTrajectory_.push_back(
        {player_.runtime().tick(), cue.position});
}

ReplayTransportSnapshot ReplayTransportModel::snapshot() const
{
    ReplayTransportSnapshot result;
    result.loaded = loaded_;
    result.paused = paused_;
    result.finished = loaded_ && player_.finished();
    result.documentFormatVersion = player_.documentFormatVersion();
    result.rulesReceiptCount = player_.rulesReceiptCount();
    result.rulesEffectVerified = player_.rulesEffectVerified();
    result.playbackRate = playbackRate_;
    result.tick = loaded_ ? player_.runtime().tick() : 0;
    result.cameraMode = cameraMode_;
    result.railCollisionEvents = railCollisionEvents_;
    result.cueTrajectory = cueTrajectory_;
    if (loaded_) {
        result.fixedStepSeconds =
            player_.runtime().physicsProfile().solver.timeStepSeconds;
        const RulesRackTerminalState& terminal =
            player_.runtime().rulesRackTerminalState();
        result.rackTerminalActive = terminal.active;
        result.rackTerminalReason = terminal.reason;
        result.rackLosingPlayer = terminal.losingPlayer;
        result.rackWinningPlayer = terminal.winningPlayer;
        result.rackIdentity = terminal.rackIdentity;
        result.rackTerminalDecisionId = terminal.authorityDecisionId;
    }
    if (!cueTrajectory_.empty()) {
        result.cueEndpointVisible = true;
        result.cueEndpoint = cueTrajectory_.back().position;
    }
    return result;
}

camera::Frame ReplayTransportModel::presentationCameraFrame() const noexcept
{
    if (!loaded_) return camera::Frame{};
    if (cameraMode_ == ReplayCameraMode::Free) return freeCamera_.frame();
    return broadcastCamera_.frame();
}

}  // namespace billiardgl::replay
