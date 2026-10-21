#import <Cocoa/Cocoa.h>
#include <OpenGL/gl3.h>

#include "render/modern/gpu_resources.h"
#include "render/modern/modern_renderer.h"
#include "presentation_snapshot.h"

#include <algorithm>
#include <cmath>
#include <fstream>
#include <iostream>
#include <iterator>
#include <stdexcept>
#include <string>
#include <vector>

namespace {
void verifyAssetlessRendererBindings()
{
    using namespace billiardgl::render;
    for (int samples : {0, 2, 4}) {
    ModernRendererOptions options;
    options.msaaSamples = samples;
    options.enableBloom = true;
    options.diagnosticPassTrace = true;
    ModernRenderer renderer(options);
    std::string error;
    if (!renderer.initialize({128, 128, false}, error))
        throw std::runtime_error(error);
    for (int iteration = 0; iteration < 2; ++iteration) {
        if (iteration) renderer.resize(256, 128);
        for (int unit : {2, 3, 4, 5, 6}) {
            glActiveTexture(GL_TEXTURE0 + unit);
            GLint width = 0;
            glGetTexLevelParameteriv(unit < 5 ? GL_TEXTURE_2D :
                GL_TEXTURE_CUBE_MAP_POSITIVE_X, 0, GL_TEXTURE_WIDTH, &width);
            if (width != 1)
                throw std::runtime_error("assetless renderer sampler lacks fallback storage");
            if (unit >= 5) {
                unsigned char pixel[4]{};
                glGetTexImage(GL_TEXTURE_CUBE_MAP_POSITIVE_X, 0,
                    GL_RGBA, GL_UNSIGNED_BYTE, pixel);
                if (pixel[0] || pixel[1] || pixel[2])
                    throw std::runtime_error("fallback environment introduces unauthored light");
            }
        }
        glActiveTexture(GL_TEXTURE0);
        renderer.render(billiardgl::PresentationSnapshot{});
        if (renderer.metrics().sceneDepthBits < 24)
            throw std::runtime_error("scene framebuffer lacks 24-bit depth storage");
        const auto& passes = renderer.metrics().passes;
        if (!renderer.metrics().bloomApplied || passes.size() != 6 ||
            passes[2].name != "scene_bloom" || passes[3].name != "overlay" ||
            passes[4].name != "aces_tone_map")
            throw std::runtime_error("scene-first bloom pipeline order missing");
    }
    renderer.shutdown();
    if (glGetError() != GL_NO_ERROR)
        throw std::runtime_error("assetless renderer binding GPU error");
    std::cout << "assetless-renderer-bindings: samples=" << samples
              << " init+resize+scene-first-bloom pass\n";
    }
}

void verifyProductionBloomShader()
{
    using namespace billiardgl::render;
    // Compile the exact production source, not a test-only implementation.
    std::ifstream source(std::string(BILLIARDGL_SOURCE_ROOT) +
        "/src/Billiards/render/modern/modern_renderer.cpp");
    const std::string text{std::istreambuf_iterator<char>(source), {}};
    const std::string marker = "const char *kToneFragment = R\"GLSL(";
    const auto start = text.find(marker);
    const auto end = start == std::string::npos ? std::string::npos :
        text.find(")GLSL\";", start + marker.size());
    if (end == std::string::npos) throw std::runtime_error("production tone shader missing");
    auto vertex = compileShader(ShaderStage::Vertex,
        "#version 330 core\nout vec2 vUv;void main(){"
        "vec2 p=vec2((gl_VertexID<<1)&2,gl_VertexID&2);"
        "vUv=p;gl_Position=vec4(p*2.0-1.0,0.0,1.0);}", "bloom-test.vert");
    auto fragment = compileShader(ShaderStage::Fragment,
        text.substr(start + marker.size(), end - start - marker.size()),
        "production-tone.frag");
    auto program = linkProgram(vertex, fragment, "production-bloom");
    auto vao = createVertexArray();
    auto input = createTexture2D();
    auto output = createTexture2D();
    auto framebuffer = createFramebuffer();
    constexpr int width = 32, height = 1080;
    std::vector<float> hdr(width * height * 4, 0.0f);
    for (int y = 530; y < 550; ++y)
        for (int x = 12; x < 20; ++x)
            for (int c = 0; c < 3; ++c) hdr[(y * width + x) * 4 + c] = 8.0f;
    glActiveTexture(GL_TEXTURE0);
    for (auto texture : {input.id(), output.id()}) {
        glBindTexture(GL_TEXTURE_2D, texture);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA32F, width, height, 0,
            GL_RGBA, GL_FLOAT, texture == input.id() ? hdr.data() : nullptr);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    }
    glBindFramebuffer(GL_FRAMEBUFFER, framebuffer.id());
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0,
        GL_TEXTURE_2D, output.id(), 0);
    if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE)
        throw std::runtime_error("bloom test framebuffer incomplete");
    glViewport(0, 0, width, height);
    glDisable(GL_DEPTH_TEST);
    glDisable(GL_BLEND);
    glDisable(GL_FRAMEBUFFER_SRGB);
    glUseProgram(program.id());
    glBindVertexArray(vao.id());
    glBindTexture(GL_TEXTURE_2D, input.id());
    glUniform1i(glGetUniformLocation(program.id(), "uHdr"), 0);
    glUniform1f(glGetUniformLocation(program.id(), "uExposureScale"), 1.0f);
    auto render = [&](float strength, bool linearOutput = false) {
        glUniform1i(glGetUniformLocation(program.id(), "uOutputLinear"), linearOutput ? 1 : 0);
        glUniform1f(glGetUniformLocation(program.id(), "uBloomStrength"), strength);
        glDrawArrays(GL_TRIANGLES, 0, 3);
        std::vector<float> result(hdr.size());
        glReadPixels(0, 0, width, height, GL_RGBA, GL_FLOAT, result.data());
        if (glGetError() != GL_NO_ERROR) throw std::runtime_error("bloom GPU error");
        return result;
    };
    const auto off = render(0.0f);
    const auto on = render(effectiveBloomStrength(RenderQuality::Medium, true));
    const auto high = render(effectiveBloomStrength(RenderQuality::High, true));
    const auto low = render(effectiveBloomStrength(RenderQuality::Low, true));
    const auto disabled = render(effectiveBloomStrength(RenderQuality::High, false));
    const int halo = (540 * width + 10) * 4;
    if (!(off[halo] == 0.0f && on[halo] > 0.0f) || on != high || off != low || off != disabled)
        throw std::runtime_error("bloom halo/quality/disable pixel contract failed");
    for (std::size_t i = 0; i < on.size(); ++i)
        if (!std::isfinite(on[i]) || on[i] < off[i])
            throw std::runtime_error("bloom darkened base scene or emitted nonfinite output");
    // Below-threshold scenes are exactly unchanged, including neutral tones.
    std::fill(hdr.begin(), hdr.end(), 0.2f);
    glBindTexture(GL_TEXTURE_2D, input.id());
    glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, width, height, GL_RGBA, GL_FLOAT, hdr.data());
    if (render(0.0f) != render(0.06f))
        throw std::runtime_error("bloom affected below-threshold scene");
    // The shipping trajectory renderer guarantees >=4:1 contrast against
    // its sampled backdrop. A later bloom must not fill a dark outline with
    // neighboring bright trajectory ink and invalidate that guarantee.
    std::fill(hdr.begin(), hdr.end(), 8.0f);
    for (int y = 0; y < height; ++y)
        for (int c = 0; c < 3; ++c) hdr[(y * width + 16) * 4 + c] = 0.0f;
    glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, width, height, GL_RGBA, GL_FLOAT, hdr.data());
    const auto outlineOff = render(0.0f);
    const auto outlineOn = render(0.06f);
    const int dark = (540 * width + 16) * 4;
    const int bright = (540 * width + 20) * 4;
    const float originalContrast = (outlineOff[bright] + 0.05f) /
        (outlineOff[dark] + 0.05f);
    const float legacyContrast = (outlineOn[bright] + 0.05f) /
        (outlineOn[dark] + 0.05f);
    // Correct pipeline: bloom only opaque HDR, then composite the sharp
    // trajectory outline, and finally tone map once with bloom disabled.
    std::fill(hdr.begin(), hdr.end(), 8.0f);
    glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, width, height, GL_RGBA, GL_FLOAT, hdr.data());
    auto bloomedScene = render(0.06f, true);
    if (std::fabs(bloomedScene[bright] - 8.42f) > 0.001f)
        throw std::runtime_error("scene bloom must preserve linear HDR without early tone mapping");
    for (int y = 0; y < height; ++y)
        for (int c = 0; c < 3; ++c) bloomedScene[(y * width + 16) * 4 + c] = 0.0f;
    glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, width, height, GL_RGBA, GL_FLOAT, bloomedScene.data());
    const auto composed = render(0.0f);
    const float bloomedContrast = (composed[bright] + 0.05f) /
        (composed[dark] + 0.05f);
    std::cout << "trajectory-tone-contrast: off=" << originalContrast
              << " old_order=" << legacyContrast << " scene_first=" << bloomedContrast << '\n';
    if (legacyContrast >= 4.0f)
        throw std::runtime_error("regression fixture no longer detects the original ordering defect");
    if (originalContrast >= 4.0f && bloomedContrast < 4.0f)
        throw std::runtime_error("post-overlay bloom violates trajectory contrast contract");
    // Sample the production DOF branch with an actual depth texture. Keep
    // color discontinuous so an enabled but disconnected blur cannot pass.
    auto depth = createTexture2D();
    glActiveTexture(GL_TEXTURE1);
    glBindTexture(GL_TEXTURE_2D, depth.id());
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    const float a = -(1800.0f + 2.0f) / (1800.0f - 2.0f);
    const float b = -2.0f * 1800.0f * 2.0f / (1800.0f - 2.0f);
    std::vector<float> depths(width * height);
    auto plane = [&](float distance) {
        std::fill(depths.begin(), depths.end(), (b / distance - a + 1.0f) * 0.5f);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_R32F, width, height, 0,
            GL_RED, GL_FLOAT, depths.data());
    };
    plane(100.0f);
    glUniform1i(glGetUniformLocation(program.id(), "uSceneDepth"), 1);
    glUniform2f(glGetUniformLocation(program.id(), "uDepthProjection"), a, b);
    glUniform1f(glGetUniformLocation(program.id(), "uFocusDistance"), 100.0f);
    glUniform1f(glGetUniformLocation(program.id(), "uDofRadius"), 2.0f);
    const auto focused = render(0.0f);
    if (std::fabs(focused[dark] - composed[dark]) > 0.00001f)
        throw std::runtime_error("DOF blurs its focus plane");
    plane(400.0f);
    const auto defocused = render(0.0f);
    if (!(defocused[dark] > focused[dark] + 0.01f))
        throw std::runtime_error("DOF did not consume actual scene depth");
    // A subpixel tap still within a narrow foreground silhouette must not
    // pick up bilinearly filtered background color from a different depth.
    for (int y = 0; y < height; ++y)
        depths[y * width + 16] = (b / 140.0f - a + 1.0f) * 0.5f;
    glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, width, height, GL_RED,
        GL_FLOAT, depths.data());
    const auto silhouette = render(0.0f);
    std::cout << "dof-foreground-leak: " << silhouette[dark] << '\n';
    if (silhouette[dark] > 0.00001f)
        throw std::runtime_error("DOF leaks filtered background into foreground silhouette");
    glUniform1f(glGetUniformLocation(program.id(), "uDofRadius"), 0.0f);
    if (render(0.0f) != composed)
        throw std::runtime_error("DOF off fails exact restoration");
    for (float channel : defocused)
        if (!std::isfinite(channel)) throw std::runtime_error("DOF nonfinite output");
    glActiveTexture(GL_TEXTURE0);
    std::cout << "production-dof-gpu: focus sharp; far blurred; off exact; finite pass\n";
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    std::cout << "production-bloom-gpu: halo=" << on[halo]
              << "; low/off exact; neutral exact; finite additive pass\n";
}
} // namespace

int main()
{
    @autoreleasepool {
        try {
            const NSOpenGLPixelFormatAttribute attributes[] = {
                NSOpenGLPFAOpenGLProfile,
                static_cast<NSOpenGLPixelFormatAttribute>(NSOpenGLProfileVersion4_1Core),
                NSOpenGLPFAAccelerated,
                static_cast<NSOpenGLPixelFormatAttribute>(0)
            };
            NSOpenGLPixelFormat* format =
                [[NSOpenGLPixelFormat alloc] initWithAttributes:attributes];
            if (format == nil) throw std::runtime_error("4.1 Core pixel format unavailable");
            NSOpenGLContext* context =
                [[NSOpenGLContext alloc] initWithFormat:format shareContext:nil];
            if (context == nil) throw std::runtime_error("4.1 Core context creation failed");
            [context makeCurrentContext];

            verifyProductionBloomShader();
            verifyAssetlessRendererBindings();

            {
                const auto vertex = billiardgl::render::compileShader(
                    billiardgl::render::ShaderStage::Vertex,
                    "#version 410 core\nvoid main(){gl_Position=vec4(0.0);}\n",
                    "smoke.vert");
                const auto fragment = billiardgl::render::compileShader(
                    billiardgl::render::ShaderStage::Fragment,
                    "#version 410 core\nout vec4 color;void main(){color=vec4(1.0);}\n",
                    "smoke.frag");
                auto program = billiardgl::render::linkProgram(vertex, fragment, "smoke");
                auto buffer = billiardgl::render::createBuffer();
                auto vertexArray = billiardgl::render::createVertexArray();
                auto texture = billiardgl::render::createTexture2D();
                auto sampler = billiardgl::render::createSampler();
                auto framebuffer = billiardgl::render::createFramebuffer();

                if (!program || !buffer || !vertexArray || !texture || !sampler || !framebuffer) {
                    throw std::runtime_error("one or more RAII resources are empty");
                }

                try {
                    (void)billiardgl::render::compileShader(
                        billiardgl::render::ShaderStage::Vertex,
                        "#version 410 core\nvoid main(){ definitely_broken; }\n",
                        "intentional-failure.vert");
                    throw std::runtime_error("invalid shader unexpectedly compiled");
                } catch (const std::runtime_error& error) {
                    const std::string diagnostic = error.what();
                    if (diagnostic.find("resource: intentional-failure.vert") == std::string::npos
                        || diagnostic.find("driver log:") == std::string::npos
                        || diagnostic.find("numbered source:") == std::string::npos) {
                        throw std::runtime_error("shader failure omitted required diagnostics");
                    }
                }
            }

            std::cout << "modern-gpu-resources-smoke: pass\n";
            [NSOpenGLContext clearCurrentContext];
            return 0;
        } catch (const std::exception& error) {
            std::cerr << "modern-gpu-resources-smoke: " << error.what() << '\n';
            return 1;
        }
    }
}
