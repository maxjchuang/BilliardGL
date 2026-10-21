#pragma once

#include "ai/ai_player_control.h"
#include "automation_protocol.h"
#include "replay/replay_recorder.h"
#include "replay/replay_v1.h"
#include "training/ball_edit_transaction.h"
#include "training/training_session.h"
#include "training/trajectory_overlay_model.h"

#include <memory>

namespace billiardgl {

enum class AutomationMode { Headless, Rendered };

struct ControllerResult {
    json::Value response = json::Value::object();
    bool quitRequested = false;
    bool screenshotRequested = false;
    std::string screenshotPath;
    bool renderPerformanceRequested = false;
    int performanceWarmupFrames = 0;
    int performanceSampleFrames = 0;
};

class AutomationController {
public:
    AutomationController(GameRuntime& runtime, AutomationMode mode,
        ai::PlayerControl* aiControl = nullptr)
        : runtime_(runtime), mode_(mode), trainingSession_(runtime),
          aiControl_(aiControl) {}
    ControllerResult handle(const AutomationRequest& request);
    std::vector<std::string> capabilities() const;

private:
    ControllerResult success(int id, const json::Value& result = json::Value::object()) const;
    ControllerResult failure(int id, const std::string& code, const std::string& message) const;
    GameRuntime& runtime_;
    AutomationMode mode_;
    training::Session trainingSession_;
    std::unique_ptr<training::BallEditTransaction> editTransaction_;
    replay::Player replayPlayer_;
    replay::Recorder replayRecorder_;
    training::PreviewResult lastTrainingPreview_;
    training::TrajectoryOverlayModel trajectoryOverlay_;
    ai::PlayerControl* aiControl_ = nullptr;
};

}  // namespace billiardgl
