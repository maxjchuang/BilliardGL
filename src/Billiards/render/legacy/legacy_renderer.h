#pragma once

#include "presentation_snapshot.h"
#include "render/modern/renderer_backend.h"

#include <functional>
#include <memory>
#include <string>

namespace billiardgl {

struct GameState;
struct RenderResources;

namespace render {

// The operation table keeps the backend lifecycle testable without creating an
// OpenGL context. makeLegacyRendererOperations supplies the production mapping
// to the existing free-function renderer and RenderResources lifecycle.
struct LegacyRendererOperations {
    std::function<bool(RenderResources&, const RendererInitialization&, std::string&)>
        initializeResources;
    std::function<void(RenderResources&)> shutdownResources;
    std::function<void(int, int)> resizeViewport;
    std::function<void(const CameraPresentation&)> setupCamera;
    std::function<void(const PresentationSnapshot&, RenderResources&)> renderScene;
    std::function<void(const HudViewModel&)> drawHud;
    std::function<bool(
        const std::string&, int, int, const std::function<void()>&)> captureOffscreen;
};

LegacyRendererOperations makeLegacyRendererOperations(GameState& resourceState);

class LegacyRenderer final : public RendererBackend {
public:
    LegacyRenderer(RenderResources& resources, LegacyRendererOperations operations);
    ~LegacyRenderer() override;

    bool initialize(const RendererInitialization& initialization,
                    std::string& error) override;
    void resize(int framebufferWidth, int framebufferHeight) override;
    void render(const PresentationSnapshot& snapshot) override;
    bool capture(const RenderCaptureRequest& request,
                 std::string& error) override;
    void shutdown() noexcept override;

private:
    bool hasCompleteOperations() const noexcept;
    void drawLegacyFrame(const PresentationSnapshot& snapshot);

    RenderResources& resources_;
    LegacyRendererOperations operations_;
    std::unique_ptr<PresentationSnapshot> lastSnapshot_;
    int framebufferWidth_ = 1;
    int framebufferHeight_ = 1;
    bool initialized_ = false;
};

}  // namespace render
}  // namespace billiardgl
