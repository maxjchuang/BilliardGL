#pragma once

#include "game_runtime.h"
#include "product_settings_runtime.h"
#include "replay/replay_transport_model.h"
#include "training/training_product_controller.h"

#include <cstddef>
#include <string>

namespace billiardgl {

enum class ProductInputSurface {
    Play,
    Training,
    Replay,
};

enum class ProductAppRequest {
    None,
    Quit,
};

struct NormalizedInputEvent {
    std::string device;
    std::string control;
    std::string trigger;
    // Mouse motion uses native logical-point deltas, independent of Retina
    // backing scale. Angular conversion belongs to the semantic controller.
    // Other payloads are control-specific (steps, axes or button coordinates).
    double value = 0.0;
    double secondaryValue = 0.0;
};

struct ProductInputResult {
    bool ok = false;
    bool handled = false;
    std::string action;
    std::string reasonCode;
    ProductAppRequest appRequest = ProductAppRequest::None;
};

// Pure C++ app-facing input seam. The platform layer normalizes native input,
// submits it here, and acts only on ProductAppRequest. All game mutations are
// delegated to public semantic controller APIs.
class ProductInputController {
public:
    ProductInputController(GameRuntime& runtime,
        settings::ProductSettingsRuntime& settingsRuntime,
        training::TrainingProductController* trainingController = nullptr,
        replay::ReplayTransportModel* replayTransport = nullptr);

    void setSurface(ProductInputSurface surface);
    ProductInputSurface surface() const noexcept { return surface_; }
    void setAiTurnActive(bool active) noexcept { aiTurnActive_ = active; }
    bool aiTurnActive() const noexcept { return aiTurnActive_; }
    void setTrainingController(
        training::TrainingProductController* controller) noexcept;
    void setReplayTransport(
        replay::ReplayTransportModel* transport) noexcept;

    ProductInputResult process(const NormalizedInputEvent& event);
    void setLogicalViewportHeight(int height) noexcept {
        if (height > 0) logicalViewportHeight_ = height;
    }
    ProductInputResult beginRebind(const std::string& action);

private:
    void refreshContexts();
    ProductInputResult execute(const std::string& action,
        const NormalizedInputEvent& event);
    ProductInputResult executeSettings(const std::string& action);
    ProductInputResult executeReplay(const std::string& action);
    ProductInputResult dispatchGame(const std::string& action,
        const GameAction& gameAction);
    bool syncAssist() noexcept;

    GameRuntime& runtime_;
    settings::ProductSettingsRuntime& settingsRuntime_;
    training::TrainingProductController* trainingController_ = nullptr;
    replay::ReplayTransportModel* replayTransport_ = nullptr;
    ProductInputSurface surface_ = ProductInputSurface::Play;
    bool aiTurnActive_ = false;
    int logicalViewportHeight_ = 720;
    std::size_t replayShotOrdinal_ = 0;
};

}  // namespace billiardgl
