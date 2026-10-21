#include "render/modern/modern_renderer_product_ui.h"

#include <cmath>

namespace billiardgl::render {
namespace {

ModernProductUiRuntimeResult hidden(
    const ModernProductSessionSnapshot& session)
{
    ModernProductUiRuntimeResult result;
    result.ok = true;
    result.screen = session.shell.screen;
    return result;
}

ModernProductUiRuntimeResult failure(ProductScreen screen,
    const std::string& error)
{
    ModernProductUiRuntimeResult result;
    result.screen = screen;
    result.errorCode = error;
    return result;
}

}  // namespace

ui::UiGpuResult ModernRendererProductUiPass::initialize(
    const ModernProductUiRuntimeConfig& config)
{
    if (initialized_)
        return {false, "modern_renderer_product_ui_already_initialized"};
    const ui::UiGpuResult result = backend_.initialize(config);
    if (!result.ok) {
        ++metrics_.failures;
        return {false, "modern_renderer_product_ui_initialize_failed:" +
            (result.errorCode.empty() ? std::string("unknown") :
                result.errorCode)};
    }
    initialized_ = true;
    return {true, {}};
}

ModernProductUiRuntimeResult ModernRendererProductUiPass::render(
    const ModernProductSessionSnapshot& session,
    const PresentationSnapshot& presentation,
    const ui::ProductUiFrameEnvironment& environment)
{
    if (!environment.productMode || environment.automation ||
        environment.windowless) {
        ++metrics_.hiddenFrameSkips;
        return hidden(session);
    }
    if (!initialized_) {
        ++metrics_.failures;
        return failure(session.shell.screen,
            "modern_renderer_product_ui_not_initialized");
    }
    const ui::ShellViewport& viewport = session.shellViewport;
    if (viewport.physicalWidth <= 0 || viewport.physicalHeight <= 0 ||
        !std::isfinite(viewport.dpi) || viewport.dpi <= 0.0f) {
        ++metrics_.failures;
        return failure(session.shell.screen,
            "modern_renderer_product_ui_viewport_invalid");
    }
    if (!hasViewport_ || viewport.physicalWidth != physicalWidth_ ||
        viewport.physicalHeight != physicalHeight_ ||
        std::fabs(viewport.dpi - dpi_) > 1e-5f) {
        const ui::UiGpuResult resized = backend_.resize(
            viewport.physicalWidth, viewport.physicalHeight, viewport.dpi);
        if (!resized.ok) {
            ++metrics_.failures;
            return failure(session.shell.screen,
                "modern_renderer_product_ui_resize_failed:" +
                (resized.errorCode.empty() ? std::string("unknown") :
                    resized.errorCode));
        }
        hasViewport_ = true;
        physicalWidth_ = viewport.physicalWidth;
        physicalHeight_ = viewport.physicalHeight;
        dpi_ = viewport.dpi;
        ++metrics_.resizeCalls;
    }
    ++metrics_.visibleFrameCalls;
    ModernProductUiRuntimeResult result = backend_.render(
        session, presentation, environment);
    if (!result.ok) ++metrics_.failures;
    return result;
}

TrajectoryInsetProductUiChromeReceipt
ModernRendererProductUiPass::submitTrajectoryInsetChrome(
    const ui::TrajectoryInsetVisibleCurrentFrame& current,
    const TrajectoryInsetProductUiChromeTarget& target)
{
    if (!initialized_) {
        TrajectoryInsetProductUiChromeReceipt result;
        result.errorCode = "modern_renderer_product_ui_not_initialized";
        return result;
    }
    auto result = backend_.submitTrajectoryInsetChrome(current, target);
    if (!result.ok) ++metrics_.failures;
    return result;
}

ui::UiGpuResult ModernRendererProductUiPass::reinitializeGpu()
{
    if (!initialized_)
        return {false, "modern_renderer_product_ui_not_initialized"};
    const ui::UiGpuResult result = backend_.reinitializeGpu();
    if (!result.ok) {
        ++metrics_.failures;
        return {false, "modern_renderer_product_ui_reinitialize_failed:" +
            (result.errorCode.empty() ? std::string("unknown") :
                result.errorCode)};
    }
    return {true, {}};
}

void ModernRendererProductUiPass::contextLost() noexcept
{
    if (initialized_) backend_.contextLost();
}

void ModernRendererProductUiPass::shutdown() noexcept
{
    if (initialized_) backend_.shutdown();
    initialized_ = false;
    hasViewport_ = false;
    physicalWidth_ = 0;
    physicalHeight_ = 0;
    dpi_ = 96.0f;
}

}  // namespace billiardgl::render
