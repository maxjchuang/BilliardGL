#import <Cocoa/Cocoa.h>
#import <OpenGL/gl3.h>

#include "automation_json.h"
#include "deterministic_hash.h"
#include "product_settings.h"
#include "render_state.h"
#include "render/modern/modern_product_session.h"
#include "render/modern/modern_renderer.h"

#include <algorithm>
#include <array>
#include <chrono>
#include <cmath>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <iterator>
#include <memory>
#include <set>
#include <stdexcept>
#include <string>
#include <vector>

namespace {

using namespace billiardgl;
using namespace billiardgl::render;

void expect(bool value, const std::string& message)
{
    if (!value) throw std::runtime_error(message);
}

class ReplayDialog final : public ProductFileDialogAdapter {
public:
    ProductFileDialogSelection selection;
    ProductFileDialogSelection chooseReplayFile() override { return selection; }
    ProductFileDialogSelection saveTrainingReplayFile(
        const std::string&) override { return {}; }
};

struct ReplayFixture {
    GameRuntime live;
    ai::AiPlayerController ai;
    ReplayDialog dialog;
    std::string settingsPath;
    std::unique_ptr<ModernProductSession> session;

    ReplayFixture(bool depthOfField, const std::string& replayPath,
        int logicalWidth, int logicalHeight, int physicalWidth,
        int physicalHeight, float dpi)
    {
        settingsPath = (std::filesystem::temp_directory_path() /
            (depthOfField ? "billiardgl-replay-dof-on-settings.json" :
                "billiardgl-replay-dof-off-settings.json")).string();
        std::error_code ignored;
        std::filesystem::remove(settingsPath, ignored);
        settings::ProductSettings product = settings::ProductSettings::defaults();
        product.depthOfField = depthOfField;
        expect(settings::saveFileAtomic(settingsPath, product).ok,
            "could not write isolated DOF product settings");
        ModernProductSessionOptions options;
        options.settingsPath = settingsPath;
        options.trainingManifestPath = std::string(BILLIARDGL_SOURCE_ROOT) +
            "/assets/training/phase4/training_pack_v2.json";
        options.executableVersion = "billiardgl-product-source-v1";
        options.fileDialogAdapter = &dialog;
        session = std::make_unique<ModernProductSession>(live, ai,
            std::move(options));
        session->setShellViewport({logicalWidth, logicalHeight,
            physicalWidth, physicalHeight, dpi});
        dialog.selection = {true, replayPath};
        expect(session->snapshot().trainingLoadError.empty() &&
            session->shell().openReplayBrowser().ok &&
            session->chooseReplayFile().ok,
            "trusted training replay did not load through the product registry");
        expect(session->snapshot().shell.screen == ProductScreen::ReplayPlayback &&
            session->snapshot().shell.replay.loaded &&
            session->snapshot().effectiveSettings.depthOfField == depthOfField,
            "loaded replay or persisted DOF setting did not reach ReplayPlayback");
        for (int tick = 0; tick < 120; ++tick)
            expect(session->shell().dispatchProductInput(
                {"keyboard", ".", "press", 0.0, 0.0}).ok,
                "real replay fixed-step input failed");
        expect(session->snapshot().shell.replay.tick == 120 &&
            !session->snapshot().shell.replay.finished,
            "real replay fixture did not stop at the requested moving tick");
    }

    ~ReplayFixture()
    {
        std::error_code ignored;
        std::filesystem::remove(settingsPath, ignored);
    }
};

PresentationSnapshot presentation(ModernProductSession& session)
{
    const GameRuntime& runtime = session.presentationRuntime();
    return buildPresentationSnapshot(runtime.state(), runtime.state(), 1.0,
        runtime.shotControl(), runtime.shotControlCapabilities(),
        session.presentationCameraFrame(), buildRulesPresentation(runtime));
}

struct PpmImage {
    int width = 0;
    int height = 0;
    std::vector<unsigned char> rgb;
};

PpmImage readPpm(const std::string& path)
{
    std::ifstream input(path, std::ios::binary);
    PpmImage image;
    std::string magic;
    int maximum = 0;
    input >> magic >> image.width >> image.height >> maximum;
    input.get();
    expect(input && magic == "P6" && image.width > 0 && image.height > 0 &&
        maximum == 255, "invalid replay DOF capture header");
    image.rgb.resize(static_cast<std::size_t>(image.width) * image.height * 3u);
    input.read(reinterpret_cast<char*>(image.rgb.data()),
        static_cast<std::streamsize>(image.rgb.size()));
    expect(input && input.peek() == std::char_traits<char>::eof(),
        "replay DOF capture payload is incomplete");
    return image;
}

double meanAbsoluteDifference(const PpmImage& first, const PpmImage& second)
{
    expect(first.width == second.width && first.height == second.height &&
        first.rgb.size() == second.rgb.size(), "DOF capture dimensions differ");
    double total = 0.0;
    for (std::size_t index = 0; index < first.rgb.size(); ++index)
        total += std::abs(static_cast<int>(first.rgb[index]) -
            static_cast<int>(second.rgb[index]));
    return total / static_cast<double>(first.rgb.size());
}

struct ForegroundComparison {
    int firstPixels = 0;
    int secondPixels = 0;
    int sharedPixels = 0;
    int exactlyEqualSharedPixels = 0;
};

ForegroundComparison compareForegroundCore(const PpmImage& first,
    const PpmImage& second, int x0, int y0, int x1, int y1,
    bool cyanHud)
{
    expect(first.width == second.width && first.height == second.height,
        "foreground comparison dimensions differ");
    ForegroundComparison result;
    for (int y = y0; y < y1; ++y) {
        for (int x = x0; x < x1; ++x) {
            const std::size_t offset = static_cast<std::size_t>(
                (y * first.width + x) * 3);
            const auto foreground = [&](const std::vector<unsigned char>& rgb) {
                const int r = rgb[offset + 0];
                const int g = rgb[offset + 1];
                const int b = rgb[offset + 2];
                return cyanHud ? (r < 100 && g > 130 && b > 110 && g - r > 60)
                    : (r > 150 && g > 175 && b > 185);
            };
            const bool firstForeground = foreground(first.rgb);
            const bool secondForeground = foreground(second.rgb);
            result.firstPixels += firstForeground ? 1 : 0;
            result.secondPixels += secondForeground ? 1 : 0;
            if (!firstForeground || !secondForeground) continue;
            ++result.sharedPixels;
            if (first.rgb[offset + 0] == second.rgb[offset + 0] &&
                first.rgb[offset + 1] == second.rgb[offset + 1] &&
                first.rgb[offset + 2] == second.rgb[offset + 2])
                ++result.exactlyEqualSharedPixels;
        }
    }
    return result;
}

std::string fileSha256(const std::string& path)
{
    std::ifstream input(path, std::ios::binary);
    expect(static_cast<bool>(input), "could not open captured replay frame");
    const std::string bytes((std::istreambuf_iterator<char>(input)),
        std::istreambuf_iterator<char>());
    return sha256Hex(bytes);
}

std::size_t passIndex(const ModernFrameMetrics& metrics, const std::string& name)
{
    const auto found = std::find_if(metrics.passes.begin(), metrics.passes.end(),
        [&](const RenderPassMetric& pass) { return pass.name == name; });
    expect(found != metrics.passes.end(), "missing renderer pass: " + name);
    return static_cast<std::size_t>(found - metrics.passes.begin());
}

ModernFrameMetrics renderCapture(ModernRenderer& renderer,
    ModernProductSession& session, const std::string& path,
    int width, int height)
{
    const ModernProductSessionSnapshot product = session.snapshot();
    renderer.renderProductFrame(presentation(session), product,
        {true, false, false});
    glFinish();
    std::string error;
    expect(renderer.capture({path, width, height}, error), error);
    expect(glGetError() == GL_NO_ERROR, "replay DOF capture left GL error");
    return renderer.metrics();
}

double percentile(std::vector<double> values, double quantile)
{
    expect(!values.empty(), "DOF GPU samples are empty");
    std::sort(values.begin(), values.end());
    const std::size_t index = static_cast<std::size_t>(
        std::ceil(quantile * values.size()) - 1.0);
    return values[std::min(index, values.size() - 1)];
}

void bindProductionAssets(ModernRendererOptions& options)
{
    const std::string root = std::string(BILLIARDGL_SOURCE_ROOT);
    const std::string phase2 = root + "/assets/modern/phase2";
    options.lookdevManifestPath = root +
        "/assets/modern/lookdev/focused_practice_hall_v1.lookdev.json";
    options.pbrParametersPath = phase2 +
        "/focused_practice_hall_pbr_v1.params.json";
    options.pbrAssetManifestPath = phase2 +
        "/focused_practice_hall_modular_v1.asset.json";
    options.lightingRigPath = phase2 +
        "/focused_practice_hall_lighting_rig_v1.json";
    options.gltfScenePath = phase2 +
        "/focused_practice_hall_modular_v1.gltf";
    options.venueScenePath = phase2 +
        "/focused_practice_hall_venue_v1.gltf";
}

}  // namespace

int main(int argc, const char* const* argv)
{
    @autoreleasepool {
        try {
            expect(argc == 2, "expected output directory");
            const std::filesystem::path output(argv[1]);
            std::filesystem::create_directories(output);
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
            NSOpenGLPixelFormat* format = [[NSOpenGLPixelFormat alloc]
                initWithAttributes:attributes];
            expect(format != nil, "4.1 Core pixel format unavailable");
            NSOpenGLContext* context = [[NSOpenGLContext alloc]
                initWithFormat:format shareContext:nil];
            expect(context != nil, "4.1 Core context creation failed");
            NSWindow* window = [[NSWindow alloc] initWithContentRect:
                NSMakeRect(0, 0, 1920, 1080)
                styleMask:NSWindowStyleMaskBorderless
                backing:NSBackingStoreBuffered defer:NO];
            [window setReleasedWhenClosed:NO];
            const CGFloat scale = std::max<CGFloat>(1.0,
                [window backingScaleFactor]);
            [window setContentSize:NSMakeSize(1920.0 / scale, 1080.0 / scale)];
            [context setView:[window contentView]];
            [context makeCurrentContext];
            [context update];
            const NSRect logical = [[window contentView] bounds];
            const NSRect backing = [[window contentView] convertRectToBacking:logical];
            const int logicalWidth = static_cast<int>(std::lround(logical.size.width));
            const int logicalHeight = static_cast<int>(std::lround(logical.size.height));
            const int physicalWidth = static_cast<int>(std::lround(backing.size.width));
            const int physicalHeight = static_cast<int>(std::lround(backing.size.height));
            expect(physicalWidth == 1920 && physicalHeight == 1080,
                "High replay DOF drawable is not physical 1920x1080");
            const float dpi = 96.0f * std::min(
                static_cast<float>(physicalWidth) / logicalWidth,
                static_cast<float>(physicalHeight) / logicalHeight);
            const std::string replayPath = std::string(BILLIARDGL_SOURCE_ROOT) +
                "/assets/training/phase4/replays_v2/launch_straight_follow_intro_v2.replay.json";
            ReplayFixture off(false, replayPath, logicalWidth, logicalHeight,
                physicalWidth, physicalHeight, dpi);
            ReplayFixture on(true, replayPath, logicalWidth, logicalHeight,
                physicalWidth, physicalHeight, dpi);
            expect(gameStateSha256(off.session->presentationRuntime().state()) ==
                    gameStateSha256(on.session->presentationRuntime().state()) &&
                off.session->snapshot().shell.replay.tick ==
                    on.session->snapshot().shell.replay.tick,
                "DOF settings changed the validated replay authority");

            ModernRendererOptions options;
            options.quality = RenderQuality::High;
            options.msaaSamples = 4;
            options.enableProductUi = true;
            options.productUiSourceRoot = BILLIARDGL_SOURCE_ROOT;
            options.productUiFontPath = "/System/Library/Fonts/Menlo.ttc";
            bindProductionAssets(options);
            ModernRenderer renderer(options);
            std::string error;
            expect(renderer.initialize({physicalWidth, physicalHeight, true}, error),
                error);
            const std::string offPath = (output / "replay-dof-off.ppm").string();
            const std::string onPath = (output / "replay-dof-on.ppm").string();
            const std::string focusPath = (output / "replay-dof-focus.ppm").string();
            const std::string repeatPath =
                (output / "replay-dof-focus-repeat.ppm").string();
            const ModernFrameMetrics offMetrics = renderCapture(renderer,
                *off.session, offPath, physicalWidth, physicalHeight);
            const ModernFrameMetrics onMetrics = renderCapture(renderer,
                *on.session, onPath, physicalWidth, physicalHeight);
            expect(offMetrics.depthOfFieldRadius == 0.0f &&
                onMetrics.depthOfFieldRadius == 2.0f,
                "actual ReplayPlayback did not consume the persisted DOF setting");
            const std::size_t dof = passIndex(onMetrics, "scene_dof");
            const std::size_t overlay = passIndex(onMetrics, "overlay");
            const std::size_t tone = passIndex(onMetrics, "aces_tone_map");
            const std::size_t productUi = passIndex(onMetrics, "product_ui");
            expect(dof < overlay && overlay < tone && tone < productUi &&
                onMetrics.trajectoryOverlayActualDrawSubmitted &&
                onMetrics.trajectoryOverlayVertices > 0 &&
                onMetrics.passes[productUi].drawCalls > 0,
                "DOF did not leave real replay trajectory and HUD in later sharp passes");
            const PpmImage offImage = readPpm(offPath);
            const PpmImage onImage = readPpm(onPath);
            const double dofDifference = meanAbsoluteDifference(offImage, onImage);
            expect(dofDifference > 0.01,
                "High replay DOF produced no measurable output difference");
            const ForegroundComparison hudForeground = compareForegroundCore(
                offImage, onImage, 40, 35, 500, 385, true);
            const ForegroundComparison trajectoryForeground =
                compareForegroundCore(offImage, onImage, 680, 440, 950, 550,
                    false);
            expect(hudForeground.firstPixels >= 1000 &&
                hudForeground.firstPixels == hudForeground.secondPixels &&
                hudForeground.sharedPixels == hudForeground.firstPixels &&
                hudForeground.exactlyEqualSharedPixels ==
                    hudForeground.sharedPixels,
                "High replay DOF changed opaque HUD glyph coverage or pixels");
            expect(trajectoryForeground.firstPixels >= 500 &&
                trajectoryForeground.firstPixels ==
                    trajectoryForeground.secondPixels &&
                trajectoryForeground.sharedPixels ==
                    trajectoryForeground.firstPixels &&
                trajectoryForeground.exactlyEqualSharedPixels * 100 >=
                    trajectoryForeground.sharedPixels * 99,
                "High replay DOF blurred or displaced the trajectory foreground");

            const auto beforeCamera = on.session->presentationCameraFrame().pose;
            const std::string replayHash = gameStateSha256(
                on.session->presentationRuntime().state());
            for (int step = 0; step < 4; ++step)
                expect(on.session->shell().dispatchProductInput(
                    {"mouse", "WheelUp", "step", 1.0, 0.0}).ok,
                    "real ReplayPlayback camera zoom failed");
            const auto afterCamera = on.session->presentationCameraFrame().pose;
            expect(afterCamera.distanceCm < beforeCamera.distanceCm &&
                gameStateSha256(on.session->presentationRuntime().state()) == replayHash,
                "replay focus change mutated physics or failed to move the camera");
            const ModernFrameMetrics focusMetrics = renderCapture(renderer,
                *on.session, focusPath, physicalWidth, physicalHeight);
            const ModernFrameMetrics repeatMetrics = renderCapture(renderer,
                *on.session, repeatPath, physicalWidth, physicalHeight);
            expect(focusMetrics.depthOfFieldRadius == 2.0f &&
                repeatMetrics.depthOfFieldRadius == 2.0f &&
                fileSha256(focusPath) == fileSha256(repeatPath) &&
                meanAbsoluteDifference(onImage, readPpm(focusPath)) > 0.01,
                "replay DOF focus change was unstable or visually inert");

            std::vector<double> dofGpuMilliseconds;
            std::vector<double> bloomGpuMilliseconds;
            std::vector<double> toneMapGpuMilliseconds;
            std::vector<double> totalPostProcessGpuMilliseconds;
            const auto captureStartedAt = std::chrono::system_clock::now();
            std::array<double, 3> loadAverage{{0.0, 0.0, 0.0}};
            const int loadAverageSamples = ::getloadavg(loadAverage.data(),
                static_cast<int>(loadAverage.size()));
            int completedDofSamples = 0;
            int zeroElapsedDofSamples = 0;
            int alignedPostProcessSamples = 0;
            int unalignedPostProcessSamples = 0;
            int zeroElapsedPostProcessSamples = 0;
            int duplicatePostProcessSourceFrames = 0;
            std::set<std::uint64_t> postProcessSourceFrames;
            for (int frame = 0; frame < 420; ++frame) {
                renderer.renderProductFrame(presentation(*on.session),
                    on.session->snapshot(), {true, false, false});
                glFlush();
                if (frame < 120) continue;
                const auto& metrics = renderer.metrics();
                const auto& pass = metrics.passes[passIndex(metrics, "scene_dof")];
                if (!pass.gpuTimingAvailable) continue;
                ++completedDofSamples;
                if (pass.gpuMilliseconds > 0.0)
                    dofGpuMilliseconds.push_back(pass.gpuMilliseconds);
                else
                    ++zeroElapsedDofSamples;
                const auto& bloom = metrics.passes[passIndex(metrics,
                    "scene_bloom")];
                const auto& toneMap = metrics.passes[passIndex(metrics,
                    "aces_tone_map")];
                const bool aligned = bloom.gpuTimingAvailable &&
                    pass.gpuTimingAvailable && toneMap.gpuTimingAvailable &&
                    bloom.gpuTimingSourceFrame != 0 &&
                    bloom.gpuTimingSourceFrame == pass.gpuTimingSourceFrame &&
                    pass.gpuTimingSourceFrame == toneMap.gpuTimingSourceFrame;
                if (!aligned) {
                    ++unalignedPostProcessSamples;
                    continue;
                }
                ++alignedPostProcessSamples;
                if (!postProcessSourceFrames.insert(
                        pass.gpuTimingSourceFrame).second)
                    ++duplicatePostProcessSourceFrames;
                const double total = bloom.gpuMilliseconds +
                    pass.gpuMilliseconds + toneMap.gpuMilliseconds;
                if (bloom.gpuMilliseconds > 0.0 &&
                    pass.gpuMilliseconds > 0.0 &&
                    toneMap.gpuMilliseconds > 0.0 && total > 0.0) {
                    bloomGpuMilliseconds.push_back(bloom.gpuMilliseconds);
                    toneMapGpuMilliseconds.push_back(toneMap.gpuMilliseconds);
                    totalPostProcessGpuMilliseconds.push_back(total);
                } else
                    ++zeroElapsedPostProcessSamples;
            }
            glFinish();
            const double dofGpuP50 = percentile(dofGpuMilliseconds, 0.50);
            const double dofGpuP95 = percentile(dofGpuMilliseconds, 0.95);
            const double dofGpuP99 = percentile(dofGpuMilliseconds, 0.99);
            const double bloomGpuP50 = percentile(bloomGpuMilliseconds, 0.50);
            const double bloomGpuP95 = percentile(bloomGpuMilliseconds, 0.95);
            const double bloomGpuP99 = percentile(bloomGpuMilliseconds, 0.99);
            const double toneMapGpuP50 = percentile(toneMapGpuMilliseconds, 0.50);
            const double toneMapGpuP95 = percentile(toneMapGpuMilliseconds, 0.95);
            const double toneMapGpuP99 = percentile(toneMapGpuMilliseconds, 0.99);
            const double totalPostProcessGpuP50 = percentile(
                totalPostProcessGpuMilliseconds, 0.50);
            const double totalPostProcessGpuP95 = percentile(
                totalPostProcessGpuMilliseconds, 0.95);
            const double totalPostProcessGpuP99 = percentile(
                totalPostProcessGpuMilliseconds, 0.99);
            std::cout << "high-replay-dof-consumer: gpu-diagnostic"
                      << " dof_positive=" << dofGpuMilliseconds.size()
                      << " dof_p95_ms=" << dofGpuP95
                      << " post_positive="
                      << totalPostProcessGpuMilliseconds.size()
                      << " post_aligned=" << alignedPostProcessSamples
                      << " post_unaligned=" << unalignedPostProcessSamples
                      << " post_zero=" << zeroElapsedPostProcessSamples
                      << " post_p95_ms=" << totalPostProcessGpuP95 << '\n';
            json::Value report = json::Value::object();
            report["schema"] = json::Value(
                "billiardgl_high_replay_dof_consumer_v1");
            report["replay_path"] = json::Value(replayPath);
            report["replay_sha256"] = json::Value(fileSha256(replayPath));
            report["replay_tick"] = json::Value(std::to_string(
                on.session->snapshot().shell.replay.tick));
            report["drawable_width_px"] = json::Value(physicalWidth);
            report["drawable_height_px"] = json::Value(physicalHeight);
            report["quality"] = json::Value("high");
            report["msaa_samples"] = json::Value(4);
            report["warmup_frames"] = json::Value(120);
            report["sample_frames"] = json::Value(300);
            report["capture_started_epoch_ms"] = json::Value(
                static_cast<double>(std::chrono::duration_cast<
                    std::chrono::milliseconds>(captureStartedAt.time_since_epoch()).count()));
            report["host_load_average_1m_at_capture_start"] = json::Value(
                loadAverageSamples > 0 ? loadAverage[0] : -1.0);
            report["host_load_average_5m_at_capture_start"] = json::Value(
                loadAverageSamples > 1 ? loadAverage[1] : -1.0);
            report["host_load_average_15m_at_capture_start"] = json::Value(
                loadAverageSamples > 2 ? loadAverage[2] : -1.0);
            report["dof_radius_px"] = json::Value(2.0);
            report["dof_off_on_mean_absolute_rgb_difference"] =
                json::Value(dofDifference);
            report["focus_distance_before_cm"] = json::Value(
                beforeCamera.distanceCm);
            report["focus_distance_after_cm"] = json::Value(
                afterCamera.distanceCm);
            report["trajectory_vertices"] = json::Value(static_cast<double>(
                onMetrics.trajectoryOverlayVertices));
            report["product_ui_draw_calls"] = json::Value(static_cast<double>(
                onMetrics.passes[productUi].drawCalls));
            report["hud_foreground_pixels"] = json::Value(
                hudForeground.firstPixels);
            report["hud_foreground_exact_shared_pixels"] = json::Value(
                hudForeground.exactlyEqualSharedPixels);
            report["trajectory_foreground_pixels"] = json::Value(
                trajectoryForeground.firstPixels);
            report["trajectory_foreground_exact_shared_pixels"] = json::Value(
                trajectoryForeground.exactlyEqualSharedPixels);
            report["dof_gpu_positive_sample_count"] = json::Value(
                static_cast<int>(dofGpuMilliseconds.size()));
            report["dof_gpu_completed_sample_frames"] = json::Value(
                completedDofSamples);
            report["dof_gpu_zero_elapsed_sample_frames"] = json::Value(
                zeroElapsedDofSamples);
            report["dof_gpu_p50_ms"] = json::Value(dofGpuP50);
            report["dof_gpu_p95_ms"] = json::Value(dofGpuP95);
            report["dof_gpu_p99_ms"] = json::Value(dofGpuP99);
            report["bloom_gpu_p50_ms"] = json::Value(bloomGpuP50);
            report["bloom_gpu_p95_ms"] = json::Value(bloomGpuP95);
            report["bloom_gpu_p99_ms"] = json::Value(bloomGpuP99);
            report["tone_map_gpu_p50_ms"] = json::Value(toneMapGpuP50);
            report["tone_map_gpu_p95_ms"] = json::Value(toneMapGpuP95);
            report["tone_map_gpu_p99_ms"] = json::Value(toneMapGpuP99);
            report["postprocess_gpu_positive_aligned_sample_count"] =
                json::Value(static_cast<int>(
                    totalPostProcessGpuMilliseconds.size()));
            report["postprocess_gpu_aligned_completed_sample_frames"] =
                json::Value(alignedPostProcessSamples);
            report["postprocess_gpu_unaligned_sample_frames"] =
                json::Value(unalignedPostProcessSamples);
            report["postprocess_gpu_unique_source_frame_count"] =
                json::Value(static_cast<int>(postProcessSourceFrames.size()));
            report["postprocess_gpu_duplicate_source_frame_count"] =
                json::Value(duplicatePostProcessSourceFrames);
            report["postprocess_gpu_min_source_frame"] = json::Value(
                std::to_string(*postProcessSourceFrames.begin()));
            report["postprocess_gpu_max_source_frame"] = json::Value(
                std::to_string(*postProcessSourceFrames.rbegin()));
            report["postprocess_gpu_zero_elapsed_sample_frames"] =
                json::Value(zeroElapsedPostProcessSamples);
            report["postprocess_gpu_p50_ms"] =
                json::Value(totalPostProcessGpuP50);
            report["postprocess_gpu_p95_ms"] =
                json::Value(totalPostProcessGpuP95);
            report["postprocess_gpu_p99_ms"] =
                json::Value(totalPostProcessGpuP99);
            report["postprocess_gpu_budget_ms"] = json::Value(2.0);
            report["postprocess_gpu_budget_pass"] = json::Value(
                totalPostProcessGpuMilliseconds.size() >= 250 &&
                totalPostProcessGpuP95 < 2.0);
            report["performance_scope"] = json::Value(
                "same_source_frame_scene_bloom_plus_scene_dof_plus_aces_tone_map_nonblocking_timer_queries");
            report["captures_stable"] = json::Value(true);
            std::ofstream reportFile(output / "report.json",
                std::ios::binary | std::ios::trunc);
            reportFile << json::stringify(report) << '\n';
            expect(static_cast<bool>(reportFile), "could not write DOF report");
            expect(dofGpuMilliseconds.size() >= 250,
                "High 1080p replay scene_dof lacks sufficient GPU evidence");
            expect(duplicatePostProcessSourceFrames == 0 &&
                postProcessSourceFrames.size() ==
                    static_cast<std::size_t>(alignedPostProcessSamples),
                "High replay postprocess reused an asynchronous source frame");
            expect(totalPostProcessGpuMilliseconds.size() >= 250 &&
                totalPostProcessGpuP95 < 2.0,
                "High 1080p replay total postprocess exceeded or lacked aligned 2ms GPU evidence");
            renderer.shutdown();
            expect(glGetError() == GL_NO_ERROR,
                "High replay DOF consumer left GL error");
            [NSOpenGLContext clearCurrentContext];
            std::cout << "high-replay-dof-consumer: pass replay_tick=120"
                      << " dof_gpu_p95_ms=" << dofGpuP95
                      << " postprocess_gpu_p95_ms=" << totalPostProcessGpuP95
                      << " samples=" << dofGpuMilliseconds.size() << '\n';
            return EXIT_SUCCESS;
        } catch (const std::exception& error) {
            std::cerr << "high-replay-dof-consumer: " << error.what() << '\n';
            return EXIT_FAILURE;
        }
    }
}
