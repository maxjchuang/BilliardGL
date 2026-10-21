#import <Cocoa/Cocoa.h>
#import <OpenGL/gl3.h>

#include "presentation_snapshot.h"
#include "ai/ai_player_controller.h"
#include "deterministic_hash.h"
#include "game_runtime.h"
#include "render/modern/modern_product_session.h"
#include "render/modern/modern_renderer.h"
#include "render/ui/product_ui_frame_coordinator.h"

#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <iterator>
#include <stdexcept>
#include <string>

namespace {

using namespace billiardgl;
using namespace billiardgl::render;

void expect(bool value, const std::string& message)
{
    if (!value) throw std::runtime_error(message);
}

bool containsItem(const ModernProductSessionSnapshot& snapshot,
    const std::string& id)
{
    return std::any_of(snapshot.shell.items.begin(), snapshot.shell.items.end(),
        [&](const ShellItemViewModel& item) { return item.id == id; });
}

void expectNonProductPassContract(const ModernRenderer& renderer)
{
    static const char* expected[]{
        "shadow_depth", "hdr_opaque", "overlay", "aces_tone_map", "ui"};
    const auto& passes = renderer.metrics().passes;
    expect(passes.size() == 5,
        "suppressed product request must retain the five-pass non-product contract");
    for (std::size_t index = 0; index < passes.size(); ++index)
        expect(passes[index].name == expected[index],
            "suppressed product request changed non-product pass ordering");
    const auto& product = renderer.productUiMetrics();
    expect(product.visibleFrameCalls == 0 &&
        product.hiddenFrameSkips == 0 && product.resizeCalls == 0 &&
        product.failures == 0,
        "suppressed product request must perform zero product backend calls");
    expect(!renderer.metrics().bloomApplied && renderer.metrics().bloomStrength == 0.0f,
        "suppressed product request must not leak the user's bloom preference into lookdev");
}

}  // namespace

int main(int argc, const char* const* argv)
{
    @autoreleasepool {
        try {
            expect(argc == 2, "expected temporary capture output path");
            [NSApplication sharedApplication];
            const NSOpenGLPixelFormatAttribute attributes[] = {
                NSOpenGLPFAOpenGLProfile,
                static_cast<NSOpenGLPixelFormatAttribute>(
                    NSOpenGLProfileVersion4_1Core),
                NSOpenGLPFAAccelerated,
                NSOpenGLPFADoubleBuffer,
                NSOpenGLPFAColorSize, 24,
                NSOpenGLPFADepthSize, 24,
                static_cast<NSOpenGLPixelFormatAttribute>(0)};
            NSOpenGLPixelFormat* format =
                [[NSOpenGLPixelFormat alloc] initWithAttributes:attributes];
            expect(format != nil, "4.1 Core pixel format unavailable");
            NSOpenGLContext* context =
                [[NSOpenGLContext alloc] initWithFormat:format shareContext:nil];
            expect(context != nil, "4.1 Core context creation failed");
            NSWindow* window = [[NSWindow alloc] initWithContentRect:
                NSMakeRect(0, 0, 1280, 720)
                styleMask:NSWindowStyleMaskBorderless
                backing:NSBackingStoreBuffered defer:NO];
            [window setReleasedWhenClosed:NO];
            [context setView:[window contentView]];
            [context makeCurrentContext];
            [context update];

            const NSRect logical = [[window contentView] bounds];
            const NSRect backing = [[window contentView]
                convertRectToBacking:logical];
            const int logicalWidth = std::max(1,
                static_cast<int>(std::lround(logical.size.width)));
            const int logicalHeight = std::max(1,
                static_cast<int>(std::lround(logical.size.height)));
            const int physicalWidth = std::max(1,
                static_cast<int>(std::lround(backing.size.width)));
            const int physicalHeight = std::max(1,
                static_cast<int>(std::lround(backing.size.height)));
            const float dpi = 96.0f * std::min(
                static_cast<float>(physicalWidth) / logicalWidth,
                static_cast<float>(physicalHeight) / logicalHeight);

            ModernRendererOptions options;
            options.msaaSamples = 0;
            options.diagnosticPassTrace = true;
            options.enableProductUi = true;
            options.productUiSourceRoot = BILLIARDGL_SOURCE_ROOT;
            options.productUiFontPath = "/System/Library/Fonts/Menlo.ttc";
            ModernRenderer renderer(std::move(options));
            std::string error;
            std::cerr << "product-gpu-stage: initialize\n";
            expect(renderer.initialize({physicalWidth, physicalHeight, true},
                error), error);
            glFinish();
            std::cerr << "product-gpu-stage: initialized\n";
            // Asset-less rendering still links the complete PBR program. Its
            // unused samplers need complete fallback bindings on real drivers.
            for (int unit : {2, 3, 4, 5, 6}) {
                glActiveTexture(GL_TEXTURE0 + unit);
                GLint width = 0;
                glGetTexLevelParameteriv(unit < 5 ? GL_TEXTURE_2D :
                    GL_TEXTURE_CUBE_MAP_POSITIVE_X, 0, GL_TEXTURE_WIDTH, &width);
                expect(width == 1, "plain shader sampler has complete fallback storage");
            }
            glActiveTexture(GL_TEXTURE0);

            PresentationSnapshot presentation;
            presentation.hud.viewportWidth = physicalWidth;
            presentation.hud.viewportHeight = physicalHeight;
            GameRuntime runtime;
            ai::AiPlayerController aiPlayer;
            ModernProductSessionOptions sessionOptions;
            sessionOptions.settingsPath = (std::filesystem::temp_directory_path() /
                "billiardgl_modern_product_ui_smoke_settings.json").string();
            sessionOptions.trainingManifestPath = std::string(
                BILLIARDGL_SOURCE_ROOT) +
                "/assets/training/phase4/training_pack_v1.json";
            std::error_code ignored;
            std::filesystem::remove(sessionOptions.settingsPath, ignored);
            ModernProductSession productSession(runtime, aiPlayer,
                sessionOptions);
            productSession.setShellViewport({logicalWidth, logicalHeight,
                physicalWidth, physicalHeight, dpi});
            const ModernProductSessionSnapshot session =
                productSession.snapshot();
            expect(session.shell.screen == ProductScreen::MainMenu &&
                session.shell.items.size() == 6 &&
                containsItem(session, "single_player") &&
                containsItem(session, "training") &&
                containsItem(session, "replays") &&
                containsItem(session, "settings"),
                "real Main session must expose all six items and the four required product entries");
            for (const ui::ProductUiFrameEnvironment hidden : {
                    ui::ProductUiFrameEnvironment{false, false, false},
                    ui::ProductUiFrameEnvironment{true, true, false},
                    ui::ProductUiFrameEnvironment{true, false, true}}) {
                renderer.renderProductFrame(presentation, session, hidden);
                glFinish();
                std::cerr << "product-gpu-stage: suppressed-frame-complete\n";
                expectNonProductPassContract(renderer);
            }
            renderer.renderProductFrame(presentation, session,
                {true, false, false});
            glFinish();
            std::cerr << "product-gpu-stage: main-complete\n";
            expect(renderer.productUiMetrics().visibleFrameCalls == 1 &&
                renderer.productUiMetrics().resizeCalls == 1 &&
                renderer.metrics().passes.size() == 7 &&
                renderer.metrics().passes[2].name == "scene_bloom" &&
                renderer.metrics().passes[3].name == "overlay" &&
                renderer.metrics().passes[5].name == "product_ui" &&
                renderer.metrics().passes[5].drawCalls > 0 &&
                renderer.metrics().passes[6].name == "product_present",
                "visible product frame submits UI draws before product present");
            expect(renderer.capture({argv[1], physicalWidth, physicalHeight},
                error), error);
            expect(session.effectiveSettings.bloom &&
                renderer.metrics().bloomApplied &&
                renderer.metrics().bloomStrength == 0.06f &&
                renderer.metrics().passes[4].name == "aces_tone_map" &&
                renderer.metrics().passes[5].name == "product_ui",
                "default product bloom consumes live settings before UI composition");
            expect(productSession.shell().openSettings().ok,
                "open actual product settings transaction");
            for (int category = 0; category < 4; ++category)
                expect(productSession.shell().dispatchProductInput(
                    {"keyboard", "ArrowRight", "press", 0.0, 0.0}).ok,
                    "navigate actual Display settings category");
            expect(productSession.shell().focusItem("bloom").ok &&
                productSession.shell().navigate(ShellNavigationCommand::Activate).ok,
                "actual bloom menu action previews disabled value");
            const auto preview = productSession.snapshot();
            expect(!preview.effectiveSettings.bloom,
                "product frame snapshot carries unsaved bloom preview");
            renderer.renderProductFrame(presentation, preview, {true, false, false});
            glFinish();
            std::cerr << "product-gpu-stage: bloom-preview-complete\n";
            expect(!renderer.metrics().bloomApplied &&
                renderer.metrics().bloomStrength == 0.0f &&
                renderer.metrics().passes[4].name == "product_ui" &&
                renderer.metrics().passes[4].drawCalls > 0,
                "preview disables actual post-process while retaining later UI draws");
            expect(productSession.shell().navigate(ShellNavigationCommand::Back).ok,
                "cancel unsaved bloom setting");
            const auto restored = productSession.snapshot();
            renderer.renderProductFrame(presentation, restored, {true, false, false});
            glFinish();
            std::cerr << "product-gpu-stage: bloom-cancel-complete\n";
            expect(restored.effectiveSettings.bloom && renderer.metrics().bloomApplied &&
                renderer.metrics().bloomStrength == 0.06f,
                "cancel restores actual product post-process consumer");
            renderer.shutdown();
            // Renderer-consumer fixture, not a claim of file-dialog/playback
            // E2E: a Replay snapshot must consume the opt-in setting only at High.
            for (int samples : {0, 4}) {
                ModernRendererOptions highOptions;
                highOptions.quality = RenderQuality::High;
                highOptions.msaaSamples = samples;
                highOptions.enableProductUi = true;
                highOptions.productUiSourceRoot = BILLIARDGL_SOURCE_ROOT;
                highOptions.productUiFontPath = "/System/Library/Fonts/Menlo.ttc";
                ModernRenderer highRenderer(highOptions);
                expect(glGetError() == GL_NO_ERROR, "previous renderer shutdown left GL error");
                expect(highRenderer.initialize({physicalWidth, physicalHeight, true}, error), error);
                expect(glGetError() == GL_NO_ERROR, "High renderer initialize left GL error");
                auto replayFixture = restored;
                replayFixture.shell.screen = ProductScreen::ReplayPlayback;
                replayFixture.effectiveSettings.depthOfField = true;
                highRenderer.renderProductFrame(presentation, replayFixture, {true, false, false});
                const auto& passes = highRenderer.metrics().passes;
                expect(highRenderer.metrics().depthOfFieldRadius == 2.0f &&
                    passes[2].name == "scene_bloom" && passes[3].name == "scene_dof" &&
                    passes[4].name == "overlay" && passes[5].name == "aces_tone_map" &&
                    passes[6].name == "product_ui",
                    "High replay DOF runs before sharp trajectory and HUD");
                replayFixture.effectiveSettings.depthOfField = false;
                highRenderer.renderProductFrame(presentation, replayFixture, {true, false, false});
                expect(highRenderer.metrics().depthOfFieldRadius == 0.0f,
                    "replay DOF switch disables consumer");
                replayFixture.effectiveSettings.depthOfField = true;
                replayFixture.shell.screen = ProductScreen::MainMenu;
                highRenderer.renderProductFrame(presentation, replayFixture, {true, false, false});
                expect(highRenderer.metrics().depthOfFieldRadius == 0.0f,
                    "DOF setting cannot blur non-replay product screens");
                highRenderer.shutdown();
            }
            expect(std::filesystem::exists(argv[1]) &&
                std::filesystem::file_size(argv[1]) >
                    static_cast<std::uintmax_t>(physicalWidth * physicalHeight),
                "product Main/default capture is empty");
            std::ifstream capture(argv[1], std::ios::binary);
            const std::string captureBytes{
                std::istreambuf_iterator<char>(capture),
                std::istreambuf_iterator<char>()};
            expect(!captureBytes.empty(), "could not read product capture");
            const std::string manifestPath = std::string(argv[1]) +
                ".manifest.json";
            std::ofstream manifest(manifestPath, std::ios::binary |
                std::ios::trunc);
            manifest << "{\n"
                << "  \"format\": \"billiardgl_modern_product_ui_smoke_v1\",\n"
                << "  \"result\": \"PASS\",\n"
                << "  \"screen\": \"main_menu\",\n"
                << "  \"state\": \"default\",\n"
                << "  \"palette\": \"standard\",\n"
                << "  \"scope\": \"Main/default-only\",\n"
                << "  \"is_594_case_completion\": false,\n"
                << "  \"session_item_count\": "
                << session.shell.items.size() << ",\n"
                << "  \"required_item_ids\": [\"single_player\", \"training\", \"replays\", \"settings\"],\n"
                << "  \"logical_width\": " << logicalWidth << ",\n"
                << "  \"logical_height\": " << logicalHeight << ",\n"
                << "  \"physical_width\": " << physicalWidth << ",\n"
                << "  \"physical_height\": " << physicalHeight << ",\n"
                << "  \"dpi\": " << dpi << ",\n"
                << "  \"capture_sha256\": \""
                << sha256Hex(captureBytes) << "\"\n"
                << "}\n";
            expect(static_cast<bool>(manifest),
                "product capture manifest write failed");
            std::filesystem::remove(sessionOptions.settingsPath, ignored);
            expect(glGetError() == GL_NO_ERROR,
                "OpenGL error remained after integrated product capture");
            [NSOpenGLContext clearCurrentContext];
            std::cout << "modern-product-ui-integration-smoke: pass capture="
                      << argv[1]
                      << " scope=Main/default-only not_594_case_completion\n";
            return EXIT_SUCCESS;
        } catch (const std::exception& error) {
            std::cerr << "modern-product-ui-integration-smoke: "
                      << error.what() << '\n';
            return EXIT_FAILURE;
        }
    }
}
