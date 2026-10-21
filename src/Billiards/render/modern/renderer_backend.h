#pragma once

#include <string>

namespace billiardgl {

struct PresentationSnapshot;

namespace render {

struct RendererInitialization {
    int framebufferWidth = 1;
    int framebufferHeight = 1;
    bool enableSrgbOutput = true;
};

struct RenderCaptureRequest {
    std::string outputPath;
    int width = 0;
    int height = 0;
};

// PresentationSnapshot is deliberately const: a renderer may cache GPU-side
// presentation data, but cannot feed transforms or outcomes back to simulation.
class RendererBackend {
public:
    virtual ~RendererBackend() = default;

    virtual bool initialize(const RendererInitialization& initialization,
                            std::string& error) = 0;
    virtual void resize(int framebufferWidth, int framebufferHeight) = 0;
    virtual void render(const PresentationSnapshot& snapshot) = 0;
    virtual bool capture(const RenderCaptureRequest& request,
                         std::string& error) = 0;
    virtual void shutdown() noexcept = 0;
};

}  // namespace render
}  // namespace billiardgl
