#pragma once

#include "render/modern/modern_product_ui_runtime.h"

#include <cstdint>

namespace billiardgl::render {

struct ModernRendererProductUiMetrics {
    std::uint64_t visibleFrameCalls = 0;
    std::uint64_t hiddenFrameSkips = 0;
    std::uint64_t resizeCalls = 0;
    std::uint64_t failures = 0;
};

// The product-UI branch used by ModernRenderer. Keeping the gate and lifecycle
// here makes the production branch independently recordable without creating a
// GL context or a second product/session model.
class ModernRendererProductUiPass {
public:
    explicit ModernRendererProductUiPass(ModernProductUiBackend& backend)
        : backend_(backend) {}

    ui::UiGpuResult initialize(const ModernProductUiRuntimeConfig& config);
    ModernProductUiRuntimeResult render(
        const ModernProductSessionSnapshot& session,
        const PresentationSnapshot& presentation,
        const ui::ProductUiFrameEnvironment& environment);
    TrajectoryInsetProductUiChromeReceipt submitTrajectoryInsetChrome(
        const ui::TrajectoryInsetVisibleCurrentFrame& current,
        const TrajectoryInsetProductUiChromeTarget& target);
    ui::UiGpuResult reinitializeGpu();
    void contextLost() noexcept;
    void shutdown() noexcept;

    bool initialized() const noexcept { return initialized_; }
    const ModernRendererProductUiMetrics& metrics() const noexcept
    {
        return metrics_;
    }

private:
    ModernProductUiBackend& backend_;
    ModernRendererProductUiMetrics metrics_;
    bool initialized_ = false;
    bool hasViewport_ = false;
    int physicalWidth_ = 0;
    int physicalHeight_ = 0;
    float dpi_ = 96.0f;
};

}  // namespace billiardgl::render
