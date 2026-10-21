#import <Cocoa/Cocoa.h>
#import <OpenGL/gl3.h>

#include "render/modern/product_ui_gl_core41.h"

#include <array>
#include <cstdlib>
#include <iostream>
#include <stdexcept>

namespace {

using namespace billiardgl::render::ui;

void expect(bool value, const char* message)
{
    if (!value) throw std::runtime_error(message);
}

SdfFontAtlas makeAtlas()
{
    FontAtlasConfig config;
    config.fontPath = "/System/Library/Fonts/Menlo.ttc";
    config.nominalPixelSize = 32;
    config.atlasWidth = 1024;
    config.atlasHeight = 1024;
    SdfFontAtlas atlas;
    expect(atlas.initialize(config).ok, "SDF atlas initialization failed");
    return atlas;
}

}  // namespace

int main()
{
    @autoreleasepool {
        try {
            const NSOpenGLPixelFormatAttribute attributes[] = {
                NSOpenGLPFAOpenGLProfile,
                static_cast<NSOpenGLPixelFormatAttribute>(
                    NSOpenGLProfileVersion4_1Core),
                NSOpenGLPFAAccelerated,
                static_cast<NSOpenGLPixelFormatAttribute>(0)};
            NSOpenGLPixelFormat* format =
                [[NSOpenGLPixelFormat alloc] initWithAttributes:attributes];
            expect(format != nil, "4.1 Core pixel format unavailable");
            NSOpenGLContext* context =
                [[NSOpenGLContext alloc] initWithFormat:format shareContext:nil];
            expect(context != nil, "4.1 Core context creation failed");
            [context makeCurrentContext];

            GLuint color = 0;
            GLuint framebuffer = 0;
            glGenTextures(1, &color);
            glBindTexture(GL_TEXTURE_2D, color);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
            glTexImage2D(GL_TEXTURE_2D, 0, GL_SRGB8_ALPHA8, 64, 64, 0,
                GL_RGBA, GL_UNSIGNED_BYTE, nullptr);
            glGenFramebuffers(1, &framebuffer);
            glBindFramebuffer(GL_FRAMEBUFFER, framebuffer);
            glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0,
                GL_TEXTURE_2D, color, 0);
            expect(glCheckFramebufferStatus(GL_FRAMEBUFFER) ==
                GL_FRAMEBUFFER_COMPLETE, "UI smoke framebuffer incomplete");
            glViewport(0, 0, 64, 64);
            glClearColor(0.0f, 0.0f, 0.0f, 0.0f);
            glClear(GL_COLOR_BUFFER_BIT);

            const SdfFontAtlas atlas = makeAtlas();
            Core41ProductUiGlDispatch dispatch;
            ProductUiGpuRenderer renderer(dispatch);
            const UiGpuResult initialized = renderer.initialize(atlas);
            expect(initialized.ok, initialized.errorCode.c_str());
            glDisable(GL_BLEND);
            glEnable(GL_DEPTH_TEST);
            glDisable(GL_SCISSOR_TEST);
            glDisable(GL_FRAMEBUFFER_SRGB);
            glScissor(3, 4, 5, 6);
            ProductUiFrame frame;
            frame.ok = true;
            frame.overlay = ProductUiOverlayKind::Shell;
            frame.physicalWidth = 64;
            frame.physicalHeight = 64;
            frame.dpi = 96.0f;
            frame.draw.ok = true;
            frame.draw.atlasCacheKey = atlas.cacheKey();
            UiPrimitive panel;
            panel.kind = UiPrimitiveKind::Panel;
            panel.rect = {8.0f, 8.0f, 48.0f, 48.0f};
            panel.clipRect = {16.0f, 16.0f, 32.0f, 32.0f};
            panel.color = {1.0f, 0.0f, 0.0f, 1.0f};
            frame.draw.primitives.push_back(panel);
            const ProductUiGpuFrameResult submitted = renderer.render(frame);
            expect(submitted.ok, submitted.errorCode.c_str());
            expect(submitted.metrics.drawCalls == 1 &&
                submitted.metrics.solidVertices == 6,
                "unexpected UI GPU smoke metrics");
            GLint restoredScissor[4]{};
            glGetIntegerv(GL_SCISSOR_BOX, restoredScissor);
            expect(glIsEnabled(GL_BLEND) == GL_FALSE &&
                glIsEnabled(GL_DEPTH_TEST) == GL_TRUE &&
                glIsEnabled(GL_SCISSOR_TEST) == GL_FALSE &&
                glIsEnabled(GL_FRAMEBUFFER_SRGB) == GL_FALSE &&
                restoredScissor[0] == 3 && restoredScissor[1] == 4 &&
                restoredScissor[2] == 5 && restoredScissor[3] == 6,
                "UI submission did not restore caller GL state");
            glFinish();
            std::array<unsigned char, 4> inside{};
            std::array<unsigned char, 4> outside{};
            glReadPixels(32, 32, 1, 1, GL_RGBA, GL_UNSIGNED_BYTE,
                inside.data());
            glReadPixels(10, 10, 1, 1, GL_RGBA, GL_UNSIGNED_BYTE,
                outside.data());
            expect(inside[0] > 240 && inside[1] < 8 && inside[2] < 8 &&
                inside[3] > 240, "Core 4.1 UI panel did not reach FBO");
            expect(outside[0] == 0 && outside[1] == 0 && outside[2] == 0 &&
                outside[3] == 0, "GL scissor failed to preserve outside pixel");
            expect(glGetError() == GL_NO_ERROR,
                "OpenGL error remained after UI FBO smoke");
            renderer.shutdown();
            glDeleteFramebuffers(1, &framebuffer);
            glDeleteTextures(1, &color);
            [NSOpenGLContext clearCurrentContext];
            std::cout << "product-ui-gpu-smoke: pass\n";
            return EXIT_SUCCESS;
        } catch (const std::exception& error) {
            std::cerr << "product-ui-gpu-smoke: " << error.what() << '\n';
            return EXIT_FAILURE;
        }
    }
}
