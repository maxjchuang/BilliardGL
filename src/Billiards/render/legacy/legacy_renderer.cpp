#include "render/legacy/legacy_renderer.h"

#include <exception>
#include <stdexcept>
#include <utility>

namespace billiardgl::render {

LegacyRenderer::LegacyRenderer(
    RenderResources& resources, LegacyRendererOperations operations)
    : resources_(resources), operations_(std::move(operations))
{
}

LegacyRenderer::~LegacyRenderer()
{
    shutdown();
}

bool LegacyRenderer::hasCompleteOperations() const noexcept
{
    return operations_.initializeResources
        && operations_.shutdownResources
        && operations_.resizeViewport
        && operations_.setupCamera
        && operations_.renderScene
        && operations_.drawHud
        && operations_.captureOffscreen;
}

bool LegacyRenderer::initialize(
    const RendererInitialization& initialization, std::string& error)
{
    error.clear();
    if (initialized_) {
        error = "LegacyRenderer is already initialized";
        return false;
    }
    if (initialization.framebufferWidth <= 0 ||
        initialization.framebufferHeight <= 0) {
        error = "LegacyRenderer requires positive framebuffer dimensions";
        return false;
    }
    if (!hasCompleteOperations()) {
        error = "LegacyRenderer operations are incomplete";
        return false;
    }

    try {
        if (!operations_.initializeResources(resources_, initialization, error)) {
            if (error.empty()) error = "LegacyRenderer resource initialization failed";
            return false;
        }
        framebufferWidth_ = initialization.framebufferWidth;
        framebufferHeight_ = initialization.framebufferHeight;
        operations_.resizeViewport(framebufferWidth_, framebufferHeight_);
        initialized_ = true;
        return true;
    } catch (const std::exception& failure) {
        error = std::string("LegacyRenderer initialization failed: ") + failure.what();
        try {
            operations_.shutdownResources(resources_);
        } catch (...) {
        }
        return false;
    } catch (...) {
        error = "LegacyRenderer initialization failed: unknown error";
        try {
            operations_.shutdownResources(resources_);
        } catch (...) {
        }
        return false;
    }
}

void LegacyRenderer::resize(int framebufferWidth, int framebufferHeight)
{
    if (!initialized_) {
        throw std::runtime_error("LegacyRenderer resize before initialize");
    }
    if (framebufferWidth <= 0 || framebufferHeight <= 0) {
        throw std::runtime_error("LegacyRenderer resize requires positive dimensions");
    }
    framebufferWidth_ = framebufferWidth;
    framebufferHeight_ = framebufferHeight;
    operations_.resizeViewport(framebufferWidth_, framebufferHeight_);
}

void LegacyRenderer::drawLegacyFrame(const PresentationSnapshot& snapshot)
{
    // These are the exact inputs used by the pre-wrapper legacy free functions:
    // camera and HUD remain subobjects of the same immutable snapshot.
    operations_.setupCamera(snapshot.camera);
    operations_.renderScene(snapshot, resources_);
    operations_.drawHud(snapshot.hud);
}

void LegacyRenderer::render(const PresentationSnapshot& snapshot)
{
    if (!initialized_) {
        throw std::runtime_error("LegacyRenderer render before initialize");
    }
    lastSnapshot_ = std::make_unique<PresentationSnapshot>(snapshot);
    drawLegacyFrame(snapshot);
}

bool LegacyRenderer::capture(const RenderCaptureRequest& request, std::string& error)
{
    error.clear();
    if (!initialized_) {
        error = "LegacyRenderer capture before initialize";
        return false;
    }
    if (!lastSnapshot_) {
        error = "LegacyRenderer capture requires a rendered snapshot";
        return false;
    }
    if (request.outputPath.empty()) {
        error = "LegacyRenderer capture requires an output path";
        return false;
    }
    if (request.width < 0 || request.height < 0) {
        error = "LegacyRenderer capture dimensions cannot be negative";
        return false;
    }

    const int width = request.width == 0 ? framebufferWidth_ : request.width;
    const int height = request.height == 0 ? framebufferHeight_ : request.height;
    try {
        const bool captured = operations_.captureOffscreen(
            request.outputPath, width, height,
            [this]() { drawLegacyFrame(*lastSnapshot_); });
        if (!captured) {
            error = "LegacyRenderer capture failed: " + request.outputPath;
        }
        return captured;
    } catch (const std::exception& failure) {
        error = std::string("LegacyRenderer capture failed: ") + failure.what();
        return false;
    } catch (...) {
        error = "LegacyRenderer capture failed: unknown error";
        return false;
    }
}

void LegacyRenderer::shutdown() noexcept
{
    if (!initialized_) return;
    try {
        operations_.shutdownResources(resources_);
    } catch (...) {
    }
    lastSnapshot_.reset();
    initialized_ = false;
}

}  // namespace billiardgl::render
