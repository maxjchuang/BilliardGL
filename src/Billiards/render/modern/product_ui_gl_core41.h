#pragma once

#include "render/ui/product_ui_gpu_renderer.h"

namespace billiardgl::render::ui {

// OpenGL 4.1 Core implementation for the injectable UI command boundary.
// The caller must own a current context for every method except
// abandonResources(). It renders into the currently bound framebuffer.
class Core41ProductUiGlDispatch final : public ProductUiGlDispatch {
public:
    UiGpuResult createResources(const UiGpuResourceCreateInfo& info,
        UiGpuResourceHandles& handles) override;
    void destroyResources(UiGpuResourceHandles& handles) noexcept override;
    void abandonResources(UiGpuResourceHandles& handles) noexcept override;
    UiGpuResult beginFrame(const UiGpuResourceHandles& handles,
        int physicalWidth, int physicalHeight) override;
    UiGpuBeginReceipt beginFrameReceipt(const UiGpuResourceHandles& handles,
        int physicalWidth, int physicalHeight) override;
    UiGpuResult uploadVertices(const UiGpuResourceHandles& handles,
        const std::vector<UiGpuVertex>& vertices) override;
    UiGpuResult setScissor(const UiGpuScissor& scissor) override;
    UiGpuResult draw(const UiGpuResourceHandles& handles,
        const UiGpuBatch& batch) override;
    UiGpuResult endFrame() override;

private:
    struct StateSnapshot {
        bool valid = false;
        bool blend = false;
        bool depthTest = false;
        bool scissorTest = false;
        bool framebufferSrgb = false;
        int program = 0;
        int vertexArray = 0;
        int arrayBuffer = 0;
        int activeTexture = 0;
        int texture2DUnit0 = 0;
        int blendSrcRgb = 0;
        int blendDstRgb = 0;
        int blendSrcAlpha = 0;
        int blendDstAlpha = 0;
        int blendEquationRgb = 0;
        int blendEquationAlpha = 0;
        int scissor[4]{0, 0, 0, 0};
    } state_;
};

}  // namespace billiardgl::render::ui
