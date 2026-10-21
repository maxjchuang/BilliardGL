#import <Cocoa/Cocoa.h>
#import <OpenGL/gl3.h>
#include <mach-o/dyld.h>

#include "render/modern/modern_application_mac.h"
#include "render/modern/formal_asset_attestation.h"

#include "ai/ai_player_controller.h"
#include "automation_controller.h"
#include "automation_runner.h"
#include "game_runtime.h"
#include "frame_timing.h"
#include "diagnostics/playtest_logger.h"
#include "deterministic_hash.h"
#include "platform_audio.h"
#include "render_state.h"
#include "render/modern/camera_projection.h"
#include "render/modern/modern_renderer.h"
#include "render/modern/modern_product_session.h"
#include "render/modern/table_plane_pointer_projection.h"
#include "render/ui/product_ui_frame_coordinator.h"
#include "stdio_transport.h"
#include "training/trajectory_overlay_renderer.h"

#include <algorithm>
#include <array>
#include <chrono>
#include <cmath>
#include <cctype>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <iterator>
#include <limits>
#include <map>
#include <set>
#include <memory>
#include <optional>
#include <sstream>
#include <stdexcept>
#include <thread>
#include <vector>

namespace billiardgl::render {
namespace {

#if defined(BILLIARDGL_VISIBLE_DRAWABLE_SELF_TEST)
// Explicit recorded-source regression input, not a normal game session or a
// runtime authority mutation. Only the E2E target compiles this input seam.
training::TrajectoryOverlaySnapshot loadInsetMarkerRecordedSource(const std::string& path, float& uiScale)
{
    std::ifstream input(path);
    std::string tag;
    input >> tag;
    if (tag != "FIX_LAYOUT") throw std::runtime_error("inset_marker_fixture_layout");
    double ignored;
    input >> ignored >> ignored >> uiScale;
    for (int i = 0; i < 8; ++i) input >> ignored;
    training::TrajectoryOverlaySnapshot result;
    for (int t = 0; t < 2; ++t) {
        int id = -1;
        std::size_t count = 0;
        input >> tag >> id;
        if (tag != "FIX_TRACK" || id != t) throw std::runtime_error("inset_marker_fixture_track");
        auto& track = t ? result.actual : result.predicted;
        input >> track.visible >> track.endpointVisible >> track.endpoint.x
            >> track.endpoint.y >> track.endpoint.z >> track.railCollisionEvents >> count;
        if (count > 10000) throw std::runtime_error("inset_marker_fixture_sample_bound");
        for (std::size_t i = 0; i < count; ++i) {
            training::OverlaySample sample;
            input >> tag >> sample.tick >> sample.position.x >> sample.position.y
                >> sample.position.z >> sample.visible;
            if (tag != "FIX_SAMPLE") throw std::runtime_error("inset_marker_fixture_sample");
            track.samples.push_back(sample);
        }
    }
    input >> tag;
    if (!input || tag != "FIX_END") throw std::runtime_error("inset_marker_fixture_truncated");
    return result;
}
#endif

struct ContextWindow {
    NSOpenGLPixelFormat* format = nil;
    NSOpenGLContext* context = nil;
    NSOpenGLView* openGlView = nil;
    NSWindow* window = nil;
    NSRect lastWindowFrame = NSZeroRect;
    NSSize lastBackingSize = NSZeroSize;

    NSSize backingSize() const
    {
        if (window == nil) return NSZeroSize;
        NSView* view = [window contentView];
        return [view convertRectToBacking:[view bounds]].size;
    }

    void updateDrawable()
    {
        if (context == nil || window == nil) return;
        if (openGlView != nil)
            [openGlView update];
        else
            [context update];
        lastWindowFrame = [window frame];
        lastBackingSize = backingSize();
    }

    void updateDrawableIfNeeded()
    {
        if (window == nil) return;
        const NSRect frame = [window frame];
        const NSSize backing = backingSize();
        if (!NSEqualRects(frame, lastWindowFrame) ||
            !NSEqualSizes(backing, lastBackingSize)) {
            updateDrawable();
        }
    }

    explicit ContextWindow(const ModernApplicationOptions& options)
    {
        [NSApplication sharedApplication];
        const NSOpenGLPixelFormatAttribute attributes[] = {
            NSOpenGLPFAOpenGLProfile,
            static_cast<NSOpenGLPixelFormatAttribute>(NSOpenGLProfileVersion4_1Core),
            NSOpenGLPFAAccelerated,
            NSOpenGLPFADoubleBuffer,
            NSOpenGLPFAColorSize, 24,
            NSOpenGLPFADepthSize, 24,
            static_cast<NSOpenGLPixelFormatAttribute>(0)
        };
        format = [[NSOpenGLPixelFormat alloc] initWithAttributes:attributes];
        if (format == nil)
            throw std::runtime_error("ModernApplication requires macOS OpenGL 4.1 Core");
        if (options.showWindow) {
            static bool applicationFinishedLaunching = false;
            [NSApp setActivationPolicy:NSApplicationActivationPolicyRegular];
            if (![NSApp isRunning] && !applicationFinishedLaunching) {
                [NSApp finishLaunching];
                applicationFinishedLaunching = true;
            }
            const NSRect frame = NSMakeRect(80, 80, options.width, options.height);
            window = [[NSWindow alloc] initWithContentRect:frame
                styleMask:(NSWindowStyleMaskTitled|NSWindowStyleMaskClosable|NSWindowStyleMaskResizable)
                backing:NSBackingStoreBuffered defer:NO];
            [window setTitle:[NSString stringWithUTF8String:options.title.c_str()]];
            [window setReleasedWhenClosed:NO];
            [window setAcceptsMouseMovedEvents:YES];
            openGlView = [[NSOpenGLView alloc]
                initWithFrame:NSMakeRect(0, 0, options.width, options.height)
                  pixelFormat:format];
            if (openGlView == nil)
                throw std::runtime_error("ModernApplication cannot create its OpenGL view");
            [openGlView setWantsBestResolutionOpenGLSurface:YES];
            [openGlView setAutoresizingMask:
                NSViewWidthSizable | NSViewHeightSizable];
            [window setContentView:openGlView];
            context = [[NSOpenGLContext alloc] initWithFormat:format
                                                shareContext:nil];
            [openGlView setOpenGLContext:context];
            [window makeKeyAndOrderFront:nil];
            const char* physicalDrawable =
                std::getenv("BILLIARDGL_VISIBLE_PIXEL_MATRIX_PHYSICAL_SIZE");
            if (!options.productFramePerformanceReportPath.empty() ||
                    (physicalDrawable != nullptr &&
                     std::string(physicalDrawable) == "1")) {
                const CGFloat scale = std::max<CGFloat>(
                    1.0, [window backingScaleFactor]);
                [window setContentSize:NSMakeSize(
                    static_cast<CGFloat>(options.width) / scale,
                    static_cast<CGFloat>(options.height) / scale)];
            }
            [window layoutIfNeeded];
            [window displayIfNeeded];
            [NSApp activateIgnoringOtherApps:YES];
            [NSApp updateWindows];
        } else {
            context = [[NSOpenGLContext alloc] initWithFormat:format
                                                shareContext:nil];
        }
        if (context == nil)
            throw std::runtime_error("ModernApplication requires macOS OpenGL 4.1 Core");
        [context makeCurrentContext];
        if (openGlView != nil) {
            [openGlView prepareOpenGL];
            [openGlView reshape];
        }
        // Synchronize the initial drawable after the window is ordered front;
        // subsequent window/backing changes are synchronized by the frame loop.
        updateDrawable();
        GLint swapInterval = 1;
        [context setValues:&swapInterval forParameter:NSOpenGLContextParameterSwapInterval];
#if defined(BILLIARDGL_VISIBLE_DRAWABLE_SELF_TEST)
        const char* visibleTest =
            std::getenv("BILLIARDGL_VISIBLE_DRAWABLE_SELF_TEST");
        if (visibleTest != nullptr && std::string(visibleTest) == "1") {
            glBindFramebuffer(GL_FRAMEBUFFER, 0);
            const GLenum status = glCheckFramebufferStatus(GL_FRAMEBUFFER);
            const GLenum statusError = glGetError();
            if (status != GL_FRAMEBUFFER_COMPLETE || statusError != GL_NO_ERROR) {
                throw std::runtime_error(
                    "visible_drawable_initial_default_framebuffer_invalid:status=" +
                    std::to_string(status) + ":error=" +
                    std::to_string(statusError));
            }
        }
#endif
    }

    ~ContextWindow() { [NSOpenGLContext clearCurrentContext]; }
};

// The legacy screenshot_gl translation unit pulls GLEW/fixed GL into R2.
// This read-only Core readback mirrors the existing native back-buffer seam.
void captureRequestedProductFrame(const char* directory, int width, int height,
    const PresentationSnapshot& frame, const GameRuntime& runtime,
    const ModernApplicationOptions& options, const std::string& profile,
    std::uint64_t frameId, const std::string& screen)
{
    if (!directory || !*directory) return;
    const std::filesystem::path root(directory), request=root/"capture.request";
    if (!std::filesystem::exists(request)) return;
    std::ifstream input(request); std::string id; input >> id;
    if (id.empty() || id.size()>64 || !std::all_of(id.begin(),id.end(),[](unsigned char c) {
        return std::isalnum(c) || c=='_' || c=='-';
    })) throw std::runtime_error("product_capture_request_invalid");
    const auto output=root/id;
    if (!std::filesystem::create_directory(output))
        throw std::runtime_error("product_capture_fresh_output_required");
    std::filesystem::rename(request,output/"request.txt");
    GLint previousFbo=0,previousBuffer=0,previousPackBuffer=0,alignment=0,rowLength=0,skipRows=0,skipPixels=0;
    glGetIntegerv(GL_READ_FRAMEBUFFER_BINDING,&previousFbo);
    glGetIntegerv(GL_READ_BUFFER,&previousBuffer);
    glGetIntegerv(GL_PIXEL_PACK_BUFFER_BINDING,&previousPackBuffer);
    glGetIntegerv(GL_PACK_ALIGNMENT,&alignment);glGetIntegerv(GL_PACK_ROW_LENGTH,&rowLength);
    glGetIntegerv(GL_PACK_SKIP_ROWS,&skipRows);glGetIntegerv(GL_PACK_SKIP_PIXELS,&skipPixels);
    glBindBuffer(GL_PIXEL_PACK_BUFFER,0);glPixelStorei(GL_PACK_ALIGNMENT,1);
    glPixelStorei(GL_PACK_ROW_LENGTH,0);glPixelStorei(GL_PACK_SKIP_ROWS,0);glPixelStorei(GL_PACK_SKIP_PIXELS,0);
    glBindFramebuffer(GL_READ_FRAMEBUFFER,0);glReadBuffer(GL_BACK);
    std::vector<unsigned char> pixels(static_cast<std::size_t>(width)*height*4);
    glReadPixels(0,0,width,height,GL_RGBA,GL_UNSIGNED_BYTE,pixels.data());
    const auto readError=glGetError();
    glBindFramebuffer(GL_READ_FRAMEBUFFER,previousFbo);glReadBuffer(previousBuffer);
    glBindBuffer(GL_PIXEL_PACK_BUFFER,previousPackBuffer);glPixelStorei(GL_PACK_ALIGNMENT,alignment);
    glPixelStorei(GL_PACK_ROW_LENGTH,rowLength);glPixelStorei(GL_PACK_SKIP_ROWS,skipRows);
    glPixelStorei(GL_PACK_SKIP_PIXELS,skipPixels);
    if (readError!=GL_NO_ERROR || glGetError()!=GL_NO_ERROR)
        throw std::runtime_error("product_capture_native_read_failed");
    std::ofstream image(output/"frame.ppm",std::ios::binary);
    image<<"P6\n"<<width<<' '<<height<<"\n255\n";
    for(int y=height-1;y>=0;--y) for(int x=0;x<width;++x)
        image.write(reinterpret_cast<const char*>(pixels.data()+
            (static_cast<std::size_t>(y)*width+x)*4),3);
    image.close(); if (!image) throw std::runtime_error("product_capture_write_failed");
    const auto fileHash=[](const std::string& path) {
        std::ifstream file(path,std::ios::binary);
        if(!file) throw std::runtime_error("product_capture_hash_open_failed");
        const std::string bytes{std::istreambuf_iterator<char>(file),std::istreambuf_iterator<char>()};
        if(file.bad()) throw std::runtime_error("product_capture_hash_read_failed");
        return sha256Hex(bytes);
    };
    char executable[4096];std::uint32_t capacity=sizeof(executable);
    if(_NSGetExecutablePath(executable,&capacity)!=0)
        throw std::runtime_error("product_capture_executable_path_failed");
    json::Value data=json::Value::object();
    data["schema"]=json::Value("actual_product_native_capture_v1");
    data["profile_id"]=json::Value(profile);data["screen_id"]=json::Value(screen);
    data["frame_id"]=json::Value(std::to_string(frameId));
    data["tick"]=json::Value(std::to_string(runtime.tick()));
    data["binary_sha256"]=json::Value(fileHash(executable));
    data["frame_sha256"]=json::Value(fileHash((output/"frame.ppm").string()));
    data["width"]=json::Value(width);data["height"]=json::Value(height);
    data["game_state_sha256"]=json::Value(gameStateSha256(runtime.state()));
    data["physics_profile_id"]=json::Value(runtime.physicsProfile().id);
    data["physics_profile_canonical_text_sha256"]=json::Value(
        sha256Hex(canonicalPhysicsProfileText(runtime.physicsProfile())));
    data["physics_profile_replay_canonical_json_sha256"]=json::Value(
        sha256Hex(canonicalPhysicsProfileJson(runtime.physicsProfile())));
    data["shot_yaw"]=json::Value(frame.shotControl.yawRadians);
    json::Value::Array balls;
    for(const auto& ball:frame.balls) {
        json::Value item=json::Value::object();item["id"]=json::Value(ball.identity);
        item["x"]=json::Value(ball.position.x);item["y"]=json::Value(ball.position.y);
        item["z"]=json::Value(ball.position.z);item["visible"]=json::Value(ball.visible);
        balls.push_back(std::move(item));
    }
    data["balls"]=json::Value(std::move(balls));
    json::Value camera=json::Value::object();
    camera["eye_x"]=json::Value(frame.camera.eye.x);camera["eye_y"]=json::Value(frame.camera.eye.y);
    camera["eye_z"]=json::Value(frame.camera.eye.z);camera["target_x"]=json::Value(frame.camera.target.x);
    camera["target_y"]=json::Value(frame.camera.target.y);camera["target_z"]=json::Value(frame.camera.target.z);
    camera["fov"]=json::Value(frame.camera.fieldOfViewDegrees);
    camera["pan_x"]=json::Value(frame.camera.projectionPan.x);
    camera["pan_y"]=json::Value(frame.camera.projectionPan.y);
    camera["pan_z"]=json::Value(frame.camera.projectionPan.z);
    camera["semantic_state"]=json::Value(static_cast<int>(frame.camera.semanticState));
    camera["preset"]=json::Value(static_cast<int>(frame.camera.preset));
    camera["transitioning"]=json::Value(frame.camera.transitioning);data["camera"]=std::move(camera);
    json::Value content=json::Value::object();
    for(const auto& path:{options.gltfScenePath,options.venueScenePath,options.pbrParametersPath,
        options.pbrAssetManifestPath,options.lightingRigPath})
        content[path]=json::Value(fileHash(path));
    data["content_sha256_by_path"]=std::move(content);
    std::ofstream receipt(output/"metadata.json");receipt<<json::stringify(data)<<'\n';
    receipt.close();if(!receipt) throw std::runtime_error("product_capture_metadata_write_failed");
}

#if defined(BILLIARDGL_VISIBLE_DRAWABLE_SELF_TEST)
void verifyVisibleDefaultFramebuffer(int width, int height,
    const std::string& capturePath = {},
    std::vector<unsigned char>* capturedPixels = nullptr)
{
    if (width <= 0 || height <= 0)
        throw std::runtime_error("visible_drawable_invalid_backing_size");
    GLint previousReadFramebuffer = 0;
    GLint previousReadBuffer = GL_COLOR_ATTACHMENT0;
    glGetIntegerv(GL_READ_FRAMEBUFFER_BINDING, &previousReadFramebuffer);
    glGetIntegerv(GL_READ_BUFFER, &previousReadBuffer);
    if (glGetError() != GL_NO_ERROR)
        throw std::runtime_error("visible_drawable_preflight_gl_error");
    glBindFramebuffer(GL_READ_FRAMEBUFFER, 0);
    glReadBuffer(GL_BACK);
    const GLenum status = glCheckFramebufferStatus(GL_READ_FRAMEBUFFER);
    std::vector<unsigned char> pixels(static_cast<std::size_t>(width) *
                                      static_cast<std::size_t>(height) * 4u);
    if (status == GL_FRAMEBUFFER_COMPLETE)
        glReadPixels(0, 0, width, height, GL_RGBA, GL_UNSIGNED_BYTE,
                     pixels.data());
    const GLenum readError = glGetError();
    glBindFramebuffer(GL_READ_FRAMEBUFFER,
                      static_cast<GLuint>(previousReadFramebuffer));
    glReadBuffer(static_cast<GLenum>(previousReadBuffer));
    if (glGetError() != GL_NO_ERROR)
        throw std::runtime_error("visible_drawable_restore_gl_error");
    if (status != GL_FRAMEBUFFER_COMPLETE)
        throw std::runtime_error("visible_drawable_default_framebuffer_incomplete");
    if (readError != GL_NO_ERROR)
        throw std::runtime_error("visible_drawable_back_buffer_read_failed");
    bool nonWhite = false;
    bool varied = false;
    const unsigned char firstR = pixels[0], firstG = pixels[1], firstB = pixels[2];
    for (std::size_t i = 0; i < pixels.size(); i += 4u) {
        nonWhite = nonWhite || pixels[i] != 255u || pixels[i + 1] != 255u ||
            pixels[i + 2] != 255u;
        varied = varied || pixels[i] != firstR || pixels[i + 1] != firstG ||
            pixels[i + 2] != firstB;
    }
    if (!nonWhite || !varied)
        throw std::runtime_error("visible_drawable_back_buffer_has_no_presented_content");
    if (capturedPixels != nullptr) *capturedPixels = pixels;
    if (!capturePath.empty()) {
        std::ofstream output(capturePath, std::ios::binary);
        if (!output) throw std::runtime_error("visible_drawable_capture_open_failed");
        output << "P6\n" << width << ' ' << height << "\n255\n";
        for (int y = height - 1; y >= 0; --y)
            for (int x = 0; x < width; ++x)
                output.write(reinterpret_cast<const char*>(pixels.data() +
                    (static_cast<std::size_t>(y) * width + x) * 4u), 3);
        if (!output) throw std::runtime_error("visible_drawable_capture_write_failed");
    }
}

std::string visibleCaptureFileSha256(const std::string& path)
{
    std::ifstream input(path, std::ios::binary);
    if (!input)
        throw std::runtime_error("visible_drawable_capture_hash_open_failed");
    const std::string bytes{std::istreambuf_iterator<char>(input),
        std::istreambuf_iterator<char>()};
    if (input.bad())
        throw std::runtime_error("visible_drawable_capture_hash_read_failed");
    return sha256Hex(bytes);
}

void verifyVisibleCaptureFileSha256Contract(
    const std::filesystem::path& directory)
{
    const auto emptyPath = directory / ".pixel-hash-empty.tmp";
    const auto contentPath = directory / ".pixel-hash-content.tmp";
    {
        std::ofstream empty(emptyPath, std::ios::binary | std::ios::trunc);
        std::ofstream content(contentPath, std::ios::binary | std::ios::trunc);
        content << "abc";
        if (!empty || !content)
            throw std::runtime_error(
                "visible_drawable_capture_hash_fixture_write_failed");
    }
    const bool valid = visibleCaptureFileSha256(emptyPath.string()) ==
            "e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855" &&
        visibleCaptureFileSha256(contentPath.string()) ==
            "ba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015ad";
    std::filesystem::remove(emptyPath);
    std::filesystem::remove(contentPath);
    bool missingRejected = false;
    try {
        (void)visibleCaptureFileSha256(contentPath.string());
    } catch (const std::runtime_error&) {
        missingRejected = true;
    }
    if (!valid || !missingRejected)
        throw std::runtime_error(
            "visible_drawable_capture_hash_contract_failed");
}
#endif

ModernRendererOptions rendererOptions(const ModernApplicationOptions& options)
{
    ModernRendererOptions value;
    value.msaaSamples = options.msaaSamples;
    value.debugErrors = true;
    value.synchronousGpuTiming = false;
    value.quality = options.quality;
    value.lookdevManifestPath = options.lookdevManifestPath;
    value.pbrParametersPath = options.pbrParametersPath;
    value.pbrAssetManifestPath = options.pbrAssetManifestPath;
    value.lightingRigPath = options.lightingRigPath;
    value.gltfScenePath = options.gltfScenePath;
    value.venueScenePath = options.venueScenePath;
    value.enableProductUi = options.productMode;
    if (options.productMode) {
        value.productUiSourceRoot = BILLIARDGL_SOURCE_ROOT;
        value.productUiFontPath = "/System/Library/Fonts/Menlo.ttc";
        value.productUiRulesDecisionIllegalBreakCapability = true;
#if defined(BILLIARDGL_VISIBLE_DRAWABLE_SELF_TEST)
        const char* pixelMatrix =
            std::getenv("BILLIARDGL_VISIBLE_PIXEL_MATRIX_SELF_TEST");
        const bool pixelMatrixEvidence =
            pixelMatrix != nullptr && std::string(pixelMatrix) == "1";
        value.productUiRetainSubmittedTextEvidence = pixelMatrixEvidence;
        value.enableFinalOwnershipInstrumentation = pixelMatrixEvidence;
#endif
    }
    return value;
}

BenchmarkPercentiles percentiles(std::vector<double> values)
{
    if (values.empty()) return {};
    std::sort(values.begin(), values.end());
    const auto at = [&](double quantile) {
        const auto index = static_cast<std::size_t>(std::ceil(quantile * values.size()) - 1.0);
        return values[std::min(index, values.size() - 1)];
    };
    return {at(0.50), at(0.95), at(0.99)};
}

constexpr int kProductPerformanceWarmupFrames = 120;
constexpr int kProductPerformanceSampleFrames = 300;
constexpr float kProductPerformanceShotPower = 200.0f;

struct ProductFramePerformanceCollector {
    int presentedFrames = 0;
    int movingSampleFrames = 0;
    int firstMovingSampleFrame = 0;
    int lastMovingSampleFrame = 0;
    int physicsSteps = 0;
    int catchupClampedFrames = 0;
    int actualMsaaSamples = 0;
    RenderQuality actualQuality = RenderQuality::Medium;
    double droppedWallSeconds = 0.0;
    std::vector<double> loopThroughSwapMilliseconds;
    std::vector<double> swapToSwapMilliseconds;
    std::vector<double> swapMilliseconds;
    std::vector<double> physicsMilliseconds;
    std::vector<double> renderMilliseconds;
    std::map<std::string, std::vector<double>> gpuPassMilliseconds;
    std::map<std::string, int> gpuCompletedSampleFrames;
    std::map<std::string, int> gpuZeroElapsedSampleFrames;
    std::map<std::string, std::vector<double>> cpuPassMilliseconds;
    std::uint64_t nativeShotDispatchTick = 0;
    std::uint64_t firstSampleTick = 0;
    std::uint64_t lastSampleTick = 0;

    bool sampling() const noexcept {
        return presentedFrames >= kProductPerformanceWarmupFrames;
    }
    bool complete() const noexcept {
        return presentedFrames >= kProductPerformanceWarmupFrames +
            kProductPerformanceSampleFrames;
    }
    void observe(const PresentedFrameTiming& timing,
        const ModernFrameMetrics& metrics, double physicsMs, double renderMs,
        int steps, bool ballsMoving, std::uint64_t physicsTick,
        const FrameStepResult& frameSteps)
    {
        ++presentedFrames;
        if (presentedFrames <= kProductPerformanceWarmupFrames) return;
        if (!timing.swapToSwapMilliseconds)
            throw std::runtime_error(
                "product_performance_cadence_missing_after_warmup");
        if (metrics.msaaSamples != 4 || metrics.quality != RenderQuality::Medium)
            throw std::runtime_error(
                "product_performance_renderer_configuration_changed");
        actualMsaaSamples = metrics.msaaSamples;
        actualQuality = metrics.quality;
        const int sampleFrame = presentedFrames - kProductPerformanceWarmupFrames;
        if (sampleFrame == 1) firstSampleTick = physicsTick;
        lastSampleTick = physicsTick;
        loopThroughSwapMilliseconds.push_back(
            timing.loopThroughSwapMilliseconds);
        swapToSwapMilliseconds.push_back(*timing.swapToSwapMilliseconds);
        swapMilliseconds.push_back(timing.swapMilliseconds);
        physicsMilliseconds.push_back(physicsMs);
        renderMilliseconds.push_back(renderMs);
        physicsSteps += steps;
        if (ballsMoving) {
            ++movingSampleFrames;
            if (firstMovingSampleFrame == 0) firstMovingSampleFrame = sampleFrame;
            lastMovingSampleFrame = sampleFrame;
        }
        catchupClampedFrames += frameSteps.clamped ? 1 : 0;
        droppedWallSeconds += frameSteps.droppedSeconds;
        for (const RenderPassMetric& pass : metrics.passes) {
            cpuPassMilliseconds[pass.name].push_back(pass.cpuMilliseconds);
            if (pass.gpuTimingAvailable) {
                ++gpuCompletedSampleFrames[pass.name];
                if (pass.gpuMilliseconds > 0.0)
                    gpuPassMilliseconds[pass.name].push_back(pass.gpuMilliseconds);
                else
                    ++gpuZeroElapsedSampleFrames[pass.name];
            }
        }
    }
};

json::Value percentileValue(const std::vector<double>& samples)
{
    const BenchmarkPercentiles measured = percentiles(samples);
    json::Value value = json::Value::object();
    value["p50_ms"] = json::Value(measured.p50);
    value["p95_ms"] = json::Value(measured.p95);
    value["p99_ms"] = json::Value(measured.p99);
    value["sample_count"] = json::Value(static_cast<int>(samples.size()));
    return value;
}

void writeProductFramePerformanceReport(const std::string& path,
    const ProductFramePerformanceCollector& collector,
    const ModernApplicationOptions& options, const std::string& playtestLogPath)
{
    if (collector.loopThroughSwapMilliseconds.size() !=
            kProductPerformanceSampleFrames ||
        collector.swapToSwapMilliseconds.size() !=
            kProductPerformanceSampleFrames ||
        collector.swapMilliseconds.size() != kProductPerformanceSampleFrames)
        throw std::runtime_error("product_performance_sample_count_incomplete");
    json::Value report = json::Value::object();
    report["schema"] = json::Value(
        "billiardgl_product_frame_performance_v1");
    report["measurement_scope"] = json::Value(
        "visible_product_event_loop_through_swap_completion");
    report["budget_reference_series"] = json::Value(
        "swap_to_swap_cadence");
    report["loop_through_swap_includes_post_swap_logging"] =
        json::Value(false);
    report["swap_to_swap_cadence_includes_previous_post_swap_logging"] =
        json::Value(true);
    report["budget_verdict"] = json::Value("not_evaluated");
    report["reference_budget_p95_ms"] = json::Value(16.7);
    report["warmup_frames"] = json::Value(kProductPerformanceWarmupFrames);
    report["sample_frames"] = json::Value(kProductPerformanceSampleFrames);
    report["drawable_width_px"] = json::Value(options.width);
    report["drawable_height_px"] = json::Value(options.height);
    report["render_quality"] = json::Value("medium");
    report["requested_msaa_samples"] = json::Value(options.msaaSamples);
    report["actual_msaa_samples"] = json::Value(collector.actualMsaaSamples);
    report["actual_render_quality"] = json::Value(
        collector.actualQuality == RenderQuality::Medium ? "medium" : "invalid");
    report["includes_product_ui"] = json::Value(true);
    report["includes_simulation"] = json::Value(true);
    report["includes_display_swap"] = json::Value(true);
    report["includes_shipping_playtest_logging"] = json::Value(true);
    report["measures_scanout"] = json::Value(false);
    report["forces_diagnostic_gl_finish"] = json::Value(false);
    report["gpu_timing_method"] = json::Value(
        "production_nonblocking_timer_query_ring");
    report["playtest_log_directory"] = json::Value(playtestLogPath);
    report["executable_version"] = json::Value(options.executableVersion);
    const GLubyte* vendor = glGetString(GL_VENDOR);
    const GLubyte* renderer = glGetString(GL_RENDERER);
    report["gl_vendor"] = json::Value(vendor ?
        reinterpret_cast<const char*>(vendor) : "unavailable");
    report["gl_renderer"] = json::Value(renderer ?
        reinterpret_cast<const char*>(renderer) : "unavailable");
    json::Value fullFrame = json::Value::object();
    fullFrame["loop_through_swap"] = percentileValue(
        collector.loopThroughSwapMilliseconds);
    fullFrame["swap_to_swap_cadence"] = percentileValue(
        collector.swapToSwapMilliseconds);
    fullFrame["swap"] = percentileValue(collector.swapMilliseconds);
    fullFrame["physics"] = percentileValue(collector.physicsMilliseconds);
    fullFrame["render_submission"] = percentileValue(
        collector.renderMilliseconds);
    report["full_frame_percentiles"] = std::move(fullFrame);
    json::Value gpuPasses = json::Value::object();
    for (const auto& entry : collector.cpuPassMilliseconds) {
        const auto measured = collector.gpuPassMilliseconds.find(entry.first);
        const auto completed = collector.gpuCompletedSampleFrames.find(entry.first);
        const auto zero = collector.gpuZeroElapsedSampleFrames.find(entry.first);
        const int completedFrames = completed == collector.gpuCompletedSampleFrames.end() ?
            0 : completed->second;
        const int zeroFrames = zero == collector.gpuZeroElapsedSampleFrames.end() ?
            0 : zero->second;
        json::Value pass = measured != collector.gpuPassMilliseconds.end() &&
                !measured->second.empty() ? percentileValue(measured->second) :
                json::Value::object();
        const bool available = measured != collector.gpuPassMilliseconds.end() &&
            !measured->second.empty();
        pass["available"] = json::Value(available);
        pass["completed_sample_frames"] = json::Value(completedFrames);
        pass["zero_elapsed_sample_frames"] = json::Value(zeroFrames);
        if (!available) {
            pass["sample_count"] = json::Value(0);
            pass["unavailable_reason"] = json::Value(
                completedFrames > 0 ? "completed_queries_returned_zero_elapsed" :
                    "no_completed_timer_query");
        }
        gpuPasses[entry.first] = std::move(pass);
    }
    report["gpu_pass_percentiles"] = std::move(gpuPasses);
    json::Value cpuPasses = json::Value::object();
    for (const auto& entry : collector.cpuPassMilliseconds)
        cpuPasses[entry.first] = percentileValue(entry.second);
    report["cpu_pass_percentiles"] = std::move(cpuPasses);
    report["moving_sample_frames"] = json::Value(
        collector.movingSampleFrames);
    report["first_moving_sample_frame"] = json::Value(
        collector.firstMovingSampleFrame);
    report["last_moving_sample_frame"] = json::Value(
        collector.lastMovingSampleFrame);
    report["physics_steps"] = json::Value(collector.physicsSteps);
    report["native_shot_requested_power"] = json::Value(
        kProductPerformanceShotPower);
    report["native_shot_dispatch_presented_frame"] = json::Value(
        kProductPerformanceWarmupFrames);
    report["native_shot_dispatch_tick"] = json::Value(
        std::to_string(collector.nativeShotDispatchTick));
    report["first_sample_tick"] = json::Value(
        std::to_string(collector.firstSampleTick));
    report["last_sample_tick"] = json::Value(
        std::to_string(collector.lastSampleTick));
    report["catchup_clamped_frames"] = json::Value(
        collector.catchupClampedFrames);
    report["dropped_wall_seconds"] = json::Value(
        collector.droppedWallSeconds);
    const std::filesystem::path reportPath(path);
    if (!reportPath.parent_path().empty())
        std::filesystem::create_directories(reportPath.parent_path());
    std::ofstream output(path, std::ios::binary | std::ios::trunc);
    if (!output)
        throw std::runtime_error("product_performance_report_open_failed");
    output << json::stringify(report) << '\n';
    if (!output)
        throw std::runtime_error("product_performance_report_write_failed");
}

void dispatchCompatibilityKey(GameRuntime& runtime, unsigned char key)
{
    GameAction action;
    if (key == '\r') action.type = ActionType::Shoot;
    else { action.type = ActionType::KeyDown; action.firstInt = key; }
    (void)runtime.dispatch(action);
}

std::string defaultProductSettingsPath()
{
    NSArray<NSString*>* locations = NSSearchPathForDirectoriesInDomains(
        NSApplicationSupportDirectory, NSUserDomainMask, YES);
    NSString* base = [locations firstObject];
    if (base == nil) base = NSTemporaryDirectory();
    NSString* directory = [base stringByAppendingPathComponent:@"BilliardGL"];
    NSError* error = nil;
    if (![[NSFileManager defaultManager] createDirectoryAtPath:directory
            withIntermediateDirectories:YES attributes:nil error:&error]) {
        throw std::runtime_error(std::string("cannot create product settings directory: ") +
            (error == nil ? "unknown error" : [[error localizedDescription] UTF8String]));
    }
    return [[directory stringByAppendingPathComponent:@"product-settings-v1.json"]
        UTF8String];
}

class MacProductFileDialogAdapter final : public ProductFileDialogAdapter {
public:
    ProductFileDialogSelection chooseReplayFile() override
    {
        NSOpenPanel* panel = [NSOpenPanel openPanel];
        [panel setCanChooseDirectories:NO];
        [panel setAllowsMultipleSelection:NO];
        [panel setAllowedFileTypes:@[@"json"]];
        if ([panel runModal] != NSModalResponseOK || [panel URL] == nil)
            return {};
        return {true, std::string([[[panel URL] path] fileSystemRepresentation])};
    }

    ProductFileDialogSelection saveTrainingReplayFile(
        const std::string& suggestedFilename) override
    {
        NSSavePanel* panel = [NSSavePanel savePanel];
        [panel setAllowedFileTypes:@[@"json"]];
        [panel setNameFieldStringValue:
            [NSString stringWithUTF8String:suggestedFilename.c_str()]];
        if ([panel runModal] != NSModalResponseOK || [panel URL] == nil)
            return {};
        return {true, std::string([[[panel URL] path] fileSystemRepresentation])};
    }
};

std::uint32_t productModifiers(NSEventModifierFlags flags)
{
    std::uint32_t modifiers = PlatformModifierNone;
    if ((flags & NSEventModifierFlagShift) != 0)
        modifiers |= PlatformModifierShift;
    if ((flags & NSEventModifierFlagControl) != 0)
        modifiers |= PlatformModifierControl;
    if ((flags & NSEventModifierFlagOption) != 0)
        modifiers |= PlatformModifierOption;
    if ((flags & NSEventModifierFlagCommand) != 0)
        modifiers |= PlatformModifierCommand;
    return modifiers;
}

std::string canonicalKeyControl(NSEvent* event)
{
    switch ([event keyCode]) {
    case 36: return "Enter";
    case 48: return "Tab";
    case 49: return "Space";
    case 53: return "Escape";
    case 123: return "ArrowLeft";
    case 124: return "ArrowRight";
    case 125: return "ArrowDown";
    case 126: return "ArrowUp";
    case 116: return "PageUp";
    case 121: return "PageDown";
    case 122: return "F1";
    case 120: return "F2";
    case 99: return "F3";
    case 118: return "F4";
    case 96: return "F5";
    case 97: return "F6";
    case 98: return "F7";
    case 100: return "F8";
    case 101: return "F9";
    case 109: return "F10";
    case 103: return "F11";
    case 111: return "F12";
    case 69: return "+";
    case 78: return "-";
    case 24: return ([event modifierFlags] & NSEventModifierFlagShift) != 0 ?
        "+" : "=";
    case 27: return "-";
    default: break;
    }
    NSString* characters = [event charactersIgnoringModifiers];
    if ([characters length] == 0) return {};
    const unichar character = [characters characterAtIndex:0];
    if (character == '\r' || character == '\n') return "Enter";
    if (character == 27) return "Escape";
    if (character <= 0x7f) {
        const unsigned char ascii = static_cast<unsigned char>(character);
        return std::string(1, static_cast<char>(std::isalpha(ascii) ?
            std::toupper(ascii) : ascii));
    }
    return {};
}

std::string modifierControl(unsigned short keyCode)
{
    if (keyCode == 56 || keyCode == 60) return "Shift";
    if (keyCode == 59 || keyCode == 62) return "Control";
    if (keyCode == 58 || keyCode == 61) return "Option";
    if (keyCode == 55 || keyCode == 54) return "Command";
    return {};
}

struct NativeInputOwnership {
    ProductPointerOwnership pointer;
    id focusObserver = nil;
    id resizeObserver = nil;
    std::uint64_t focusLossNotificationCount = 0;
    NativeInputOwnership(NSWindow* window, ModernProductSession* session) {
        auto* state = &pointer;
        auto* focusLossCount = &focusLossNotificationCount;
        void (^cancel)(NSNotification*) = ^(NSNotification*) {
            state->cancel();
            if (session) session->cancelPointerInteraction();
        };
        void (^focusCancel)(NSNotification*) = ^(NSNotification*) {
            ++(*focusLossCount);
            cancel(nil);
        };
        focusObserver = [[NSNotificationCenter defaultCenter]
            addObserverForName:NSWindowDidResignKeyNotification object:window queue:nil usingBlock:focusCancel];
        resizeObserver = [[NSNotificationCenter defaultCenter]
            addObserverForName:NSWindowWillStartLiveResizeNotification object:window queue:nil usingBlock:cancel];
    }
    ~NativeInputOwnership() {
        [[NSNotificationCenter defaultCenter] removeObserver:focusObserver];
        [[NSNotificationCenter defaultCenter] removeObserver:resizeObserver];
    }
};

std::optional<PlatformInputEvent> productPlatformEvent(
    NSEvent* event, NSView* contentView, ProductPointerOwnership& ownership,
    ModernProductSession& session)
{
    const NSEventType type = [event type];
    const bool input = type == NSEventTypeKeyDown || type == NSEventTypeKeyUp ||
        type == NSEventTypeFlagsChanged || type == NSEventTypeScrollWheel ||
        type == NSEventTypeMouseMoved || type == NSEventTypeLeftMouseDragged ||
        type == NSEventTypeRightMouseDragged || type == NSEventTypeLeftMouseDown ||
        type == NSEventTypeLeftMouseUp || type == NSEventTypeRightMouseDown ||
        type == NSEventTypeRightMouseUp;
    if (!input) return std::nullopt; // App/window bookkeeping cannot cancel a press.
    NSWindow* window = [contentView window];
    // Trust Cocoa's destination window, not isKeyWindow: this manually pumped
    // application can receive targeted input while the key flag is false.
    // Resign-key notifications still cancel an existing gesture immediately.
    if ([event window] != window || [contentView inLiveResize]) {
        ownership.cancel();
        session.cancelPointerInteraction();
        return std::nullopt;
    }
    PlatformInputEvent translated;
    translated.modifiers = productModifiers([event modifierFlags]);
    if (type == NSEventTypeKeyDown || type == NSEventTypeKeyUp) {
        translated.kind = type == NSEventTypeKeyDown ?
            PlatformInputKind::KeyDown : PlatformInputKind::KeyUp;
        translated.control = canonicalKeyControl(event);
        if (translated.control.empty()) return std::nullopt;
        return translated;
    }
    if (type == NSEventTypeFlagsChanged) {
        translated.kind = PlatformInputKind::ModifierChanged;
        translated.control = modifierControl([event keyCode]);
        if (translated.control.empty()) return std::nullopt;
        return translated;
    }
    if (type == NSEventTypeScrollWheel) {
        const NSPoint location = [contentView convertPoint:[event locationInWindow] fromView:nil];
        if (!NSPointInRect(location, NSInsetRect([contentView bounds], 4.0, 4.0)))
            return std::nullopt;
        translated.kind = PlatformInputKind::Scroll;
        translated.logicalX = location.x;
        translated.logicalY = [contentView isFlipped] ? location.y : [contentView bounds].size.height - location.y;
        translated.backingScale = [window backingScaleFactor];
        translated.scrollY = [event scrollingDeltaY];
        return translated;
    }
    const bool move = type == NSEventTypeMouseMoved ||
        type == NSEventTypeLeftMouseDragged ||
        type == NSEventTypeRightMouseDragged;
    const bool button = type == NSEventTypeLeftMouseDown ||
        type == NSEventTypeLeftMouseUp ||
        type == NSEventTypeRightMouseDown ||
        type == NSEventTypeRightMouseUp;
    if (!move && !button) return std::nullopt;
    const NSPoint point = [contentView convertPoint:[event locationInWindow]
        fromView:nil];
    const NSRect bounds = [contentView bounds];
    translated.logicalX = point.x;
    translated.logicalY = [contentView isFlipped] ? point.y :
        bounds.size.height - point.y;
    translated.backingScale = [[contentView window] backingScaleFactor];
    translated.deltaX = [event deltaX];
    translated.deltaY = [event deltaY];
    const NSUInteger pressed = [NSEvent pressedMouseButtons];
    translated.leftButtonDown = type == NSEventTypeLeftMouseDragged || (pressed & 1u) != 0;
    translated.rightButtonDown = type == NSEventTypeRightMouseDragged || (pressed & 2u) != 0;
    if (move) {
        translated.kind = PlatformInputKind::PointerMove;
    } else {
        const bool down = type == NSEventTypeLeftMouseDown ||
            type == NSEventTypeRightMouseDown;
        translated.kind = down ? PlatformInputKind::PointerButtonDown :
            PlatformInputKind::PointerButtonUp;
        translated.control = type == NSEventTypeRightMouseDown ||
            type == NSEventTypeRightMouseUp ? "RightButton" : "LeftButton";
    }
    const bool inside = NSPointInRect(point, NSInsetRect(bounds, 4.0, 4.0));
    if (!ownership.accept(translated, true, true, inside)) {
        session.cancelPointerInteraction();
        return std::nullopt;
    }
    return translated;
}

ui::ShellViewport productShellViewport(NSView* contentView)
{
    const NSRect logical = [contentView bounds];
    const NSRect backing = [contentView convertRectToBacking:logical];
    const int logicalWidth = std::max(1,
        static_cast<int>(std::lround(logical.size.width)));
    const int logicalHeight = std::max(1,
        static_cast<int>(std::lround(logical.size.height)));
    const int physicalWidth = std::max(1,
        static_cast<int>(std::lround(backing.size.width)));
    const int physicalHeight = std::max(1,
        static_cast<int>(std::lround(backing.size.height)));
    const float scale = std::min(
        static_cast<float>(physicalWidth) / logicalWidth,
        static_cast<float>(physicalHeight) / logicalHeight);
    return {logicalWidth, logicalHeight, physicalWidth, physicalHeight,
        96.0f * scale};
}

void dispatchOrThrow(GameRuntime& runtime, const GameAction& action,
                     const char* operation)
{
    const ActionResult result = runtime.dispatch(action);
    if (!result.ok) {
        throw std::runtime_error(
            std::string("ModernApplication rejected ") + operation + ": "
            + result.errorCode);
    }
}

void prepareStandaloneScreenshotScene(
    GameRuntime& runtime, const std::string& scene)
{
    GameAction action;
    if (scene == "default") return;
    if (scene == "help") {
        action.type = ActionType::ToggleHelp;
        dispatchOrThrow(runtime, action, "help screenshot scene");
        return;
    }
    if (scene == "aim") {
        action.type = ActionType::ToggleAim;
        dispatchOrThrow(runtime, action, "aim screenshot scene");
        action.type = ActionType::SetAimYaw;
        action.first = 1.5707963f;
        dispatchOrThrow(runtime, action, "aim yaw");
        return;
    }
    if (scene == "after-shot") {
        action.type = ActionType::SetShotPower;
        action.first = 60.0f;
        dispatchOrThrow(runtime, action, "after-shot power");
        action.type = ActionType::Shoot;
        dispatchOrThrow(runtime, action, "after-shot cue impact");
        const ActionResult stepped = runtime.step(30);
        if (!stepped.ok) {
            throw std::runtime_error(
                std::string("ModernApplication rejected after-shot steps: ")
                + stepped.errorCode);
        }
        return;
    }
    throw std::runtime_error("ModernApplication unknown screenshot scene: " + scene);
}

json::Value formalAssetAttestationValue(
    const FormalAssetPreflightResult& preflight)
{
    json::Value value = json::Value::object();
    value["schema"] = json::Value("billiardgl_formal_asset_attestation_v1");
    value["asset_root"] = json::Value(preflight.canonicalAssetRoot);
    value["load_state"] = json::Value("loaded");
    value["silent_material_fallback_allowed"] =
        json::Value(preflight.silentMaterialFallbackAllowed);
    json::Value required = json::Value::array();
    for (const FormalAssetBinding& binding : preflight.requiredAssets) {
        json::Value item = json::Value::object();
        item["id"] = json::Value(binding.id);
        item["canonical_path"] = json::Value(binding.canonicalPath);
        item["sha256"] = json::Value(binding.sha256);
        required.asArray().push_back(item);
    }
    value["required_assets"] = required;
    json::Value dependencies = json::Value::array();
    for (const FormalAssetBinding& binding : preflight.runtimeDependencies) {
        json::Value item = json::Value::object();
        item["id"] = json::Value(binding.id);
        item["canonical_path"] = json::Value(binding.canonicalPath);
        item["sha256"] = json::Value(binding.sha256);
        dependencies.asArray().push_back(item);
    }
    value["runtime_dependencies"] = dependencies;
    value["load_errors"] = json::Value::array();
    return value;
}

}  // namespace

int runModernApplication(const ModernApplicationOptions& options,
                         std::istream& input, std::ostream& output,
                         std::ostream& errors)
{
    @autoreleasepool {
        try {
            const bool collectProductFramePerformance =
                !options.productFramePerformanceReportPath.empty();
            if (collectProductFramePerformance &&
                    (!options.productMode || options.automation ||
                    !options.showWindow || options.width != 1920 ||
                    options.height != 1080 || options.msaaSamples != 4 ||
                    options.quality != RenderQuality::Medium)) {
                throw std::runtime_error(
                    "product frame performance requires visible product mode at physical 1920x1080 Medium MSAA4");
            }
            if (options.productMode && (options.automation ||
                    !options.screenshotPath.empty() || !options.showWindow)) {
                throw std::runtime_error(
                    "product mode requires an interactive visible window and cannot use automation/screenshot paths");
            }
            if (options.productMode && options.aiOpponent) {
                throw std::runtime_error(
                    "product mode configures its shared AI through single-player setup");
            }
            FormalAssetPreflightResult formalAssets;
            if (options.formalCapture) {
                if (!options.automation) {
                    throw std::runtime_error(
                        "formal capture requires rendered automation");
                }
                formalAssets = preflightFormalAssetPackage(
                    options.phase2AssetRoot);
                if (!formalAssets.ok) {
                    throw std::runtime_error(formalAssets.errors.empty() ?
                        "formal_asset_load_error code=formal_asset_preflight_failed file=<unknown> field=$ message=no structured error" :
                        formatFormalAssetLoadError(formalAssets.errors.front()));
                }
            }
            ContextWindow platform(options);
            const ui::ShellViewport initialProductViewport =
                options.productMode ? productShellViewport(
                    [platform.window contentView]) : ui::ShellViewport{};
            const int initialFramebufferWidth = options.productMode ?
                initialProductViewport.physicalWidth : options.width;
            const int initialFramebufferHeight = options.productMode ?
                initialProductViewport.physicalHeight : options.height;
            ModernRenderer renderer(rendererOptions(options));
            std::string error;
            const auto failRendererLoad = [&](const std::string& code,
                                              const std::string& file,
                                              const std::string& field,
                                              const std::string& message) {
                if (options.formalCapture) {
                    throw std::runtime_error(formatFormalAssetLoadError(
                        {code, file, field, message}));
                }
                throw std::runtime_error(message);
            };
            if (!renderer.initialize({initialFramebufferWidth,
                    initialFramebufferHeight, true}, error)) {
                failRendererLoad("formal_renderer_initialize_failed",
                    formalAssets.canonicalAssetRoot, "$.renderer.initialize",
                    error);
            }
#if defined(BILLIARDGL_VISIBLE_DRAWABLE_SELF_TEST)
            if (std::getenv("BILLIARDGL_VISIBLE_INSET_SELF_TEST") != nullptr &&
                std::getenv("BILLIARDGL_INSET_SKIP_PROVEN_CODEC_SELF_TEST") == nullptr) {
                std::string codecError;
                if (!renderer.verifyTrajectoryInsetTokenCodec(codecError))
                    throw std::runtime_error("inset_codec_selftest_failed:" + codecError);
                output << "inset_codec_selftest tokens=0x01000000,0x80000001,0xffffffff,1 samples=4"
                    << " roundtrip=PASS color_only_reference=byte_identical state_restore=PASS\n";
            }
#endif
            if (!renderer.pbrUpload().loadedPackage) {
                failRendererLoad("formal_asset_actual_load_failed",
                    options.pbrParametersPath, "$.texture_sets",
                    "production modern renderer requires the Phase-2 PBR package; silent material fallback is forbidden");
            }
            if (!renderer.pbrUpload().warnings.empty()) {
                failRendererLoad("formal_asset_degraded_load_forbidden",
                    options.pbrParametersPath, "$.texture_sets",
                    "production modern renderer rejected degraded PBR resources; silent material fallback is forbidden: "
                        + renderer.pbrUpload().warnings.front());
            }
            if (renderer.pbrUpload().environmentGpuBytes == 0) {
                failRendererLoad("formal_asset_actual_load_failed",
                    options.lightingRigPath, "$.environment",
                    "production modern renderer requires environment lighting resources");
            }
            if (!renderer.gltfSceneLoaded()) {
                failRendererLoad("formal_asset_actual_load_failed",
                    options.gltfScenePath, "$.meshes",
                    "production modern renderer requires the modular glTF venue");
            }
            if (!renderer.venueSceneLoaded()) {
                failRendererLoad("formal_asset_actual_load_failed",
                    options.venueScenePath, "$.meshes",
                    "production modern renderer requires the dedicated venue glTF package");
            }
            GameRuntime runtime;
            ai::AiPlayerController aiPlayer;
            GameAction initialResize;
            initialResize.type = ActionType::Resize;
            initialResize.firstInt = initialFramebufferWidth;
            initialResize.secondInt = initialFramebufferHeight;
            (void)runtime.dispatch(initialResize);
            if (options.aiOpponent) {
                ai::PlayerConfiguration configuration;
                configuration.enabled = true;
                configuration.player = 1;
                configuration.difficulty = options.aiDifficulty;
                const ActionResult configured = aiPlayer.configure(
                    runtime, configuration);
                if (!configured.ok) {
                    throw std::runtime_error(
                        std::string("failed to configure AI opponent: ")
                        + configured.errorCode);
                }
            }
            std::unique_ptr<audio::SdlMixerAudioCueConsumer> productAudioConsumer;
            std::unique_ptr<ModernProductSession> productSession;
            MacProductFileDialogAdapter productFileDialogs;
            std::string productCameraProfileId = "production_camera_v1";
            if (options.productMode) {
                ModernProductSessionOptions productOptions;
                if (const char* candidate = std::getenv("BILLIARDGL_PRODUCT_CAMERA_CANDIDATE")) {
                    if (std::string(candidate) != "product_camera_m4_candidate_v1")
                        throw std::runtime_error("unsupported product camera candidate");
                    productOptions.m4CameraCandidate = true;
                    productCameraProfileId = "product_camera_m4_candidate_v1";
                }
                std::cerr << "product_camera_profile=" << productCameraProfileId << '\n';
                productOptions.manualInspectors = true;
                productOptions.settingsPath = options.productSettingsPath.empty() ?
                    defaultProductSettingsPath() : options.productSettingsPath;
                productOptions.trainingManifestPath = options.trainingManifestPath;
                productOptions.executableVersion = options.executableVersion;
                productOptions.fileDialogAdapter = &productFileDialogs;
                productOptions.rulesDecisionProductCapabilities.
                    groupChoice = true;
                productOptions.rulesDecisionProductCapabilities.
                    fullTableBallInHand = true;
                productOptions.rulesDecisionProductCapabilities.
                    illegalBreakChoice = true;
                productOptions.rulesDecisionProductCapabilities.
                    breakShotFoulChoice = true;
                productOptions.rulesDecisionProductCapabilities.
                    eightOnBreakChoice = true;
                // Construction is device-free. Interactive startup explicitly
                // warms the device/cache before accepting gameplay input.
                productAudioConsumer =
                    std::make_unique<audio::SdlMixerAudioCueConsumer>(
                        audio::candidateAudioReviewOptionsFromEnvironment());
                productSession = std::make_unique<ModernProductSession>(
                    runtime, aiPlayer, std::move(productOptions),
                    productAudioConsumer.get());
                productSession->setShellViewport(initialProductViewport);
            }
            if (!options.automation && !options.screenshotPath.empty()) {
                prepareStandaloneScreenshotScene(runtime, options.screenshotScene);
            }
            GameState previous = runtime.state();
            const auto initialPresentation = buildPresentationSnapshot(previous,
                runtime.state(), 1.0,
                runtime.shotControl(), runtime.shotControlCapabilities(), runtime.cameraFrame(),
                buildRulesPresentation(runtime));
            if (productSession != nullptr) {
                productSession->prepareTrajectoryInsetCurrentFrame(
                    std::max(1, options.msaaSamples));
                renderer.renderProductFrame(initialPresentation,
                    productSession->snapshot(), {true, false, false});
            } else {
                renderer.render(initialPresentation);
            }

            if (options.automation) {
                AutomationController controller(
                    runtime, AutomationMode::Rendered, &aiPlayer);
                StdioTransport transport(input, output);
                json::Value::Object readyMetadata;
                readyMetadata["pbr_loaded"] = json::Value(renderer.pbrUpload().loadedPackage);
                readyMetadata["pbr_texture_count"] = json::Value(renderer.pbrUpload().uploadedTextureCount);
                readyMetadata["environment_enabled"] = json::Value(renderer.pbrUpload().environmentGpuBytes > 0);
                readyMetadata["gltf_scene_loaded"] = json::Value(renderer.gltfSceneLoaded());
                readyMetadata["venue_scene_loaded"] = json::Value(renderer.venueSceneLoaded());
                readyMetadata["render_quality"] = json::Value(options.quality == RenderQuality::Medium ? "medium" : (options.quality == RenderQuality::High ? "high" : "low"));
                readyMetadata["msaa_samples"] = json::Value(options.msaaSamples);
                readyMetadata["shadow_map_size"] = json::Value(qualitySettings(options.quality).shadowMapSize);
                readyMetadata["formal_capture"] =
                    json::Value(options.formalCapture);
                if (options.formalCapture) {
                    readyMetadata["formal_asset_attestation"] =
                        formalAssetAttestationValue(formalAssets);
                }
                const int result = runAutomation(transport, controller, "rendered", "modern",
                    [&](const std::string& path) {
                        const int width = runtime.state().config.width;
                        const int height = runtime.state().config.height;
                        if (width != options.width || height != options.height) renderer.resize(width, height);
                        renderer.render(buildPresentationSnapshot(previous, runtime.state(), 1.0,
                            runtime.shotControl(), runtime.shotControlCapabilities(), runtime.cameraFrame(),
                            buildRulesPresentation(runtime)));
                        std::string captureError;
                        return renderer.capture({path, width, height}, captureError);
                    }, readyMetadata,
                    [&](int warmupFrames, int sampleFrames) {
                        const int measurementWidth = runtime.state().config.width;
                        const int measurementHeight = runtime.state().config.height;
                        renderer.resize(measurementWidth, measurementHeight);
                        std::vector<double> totalFrameMilliseconds;
                        std::map<std::string, std::vector<double>> cpuPassSamples;
                        std::map<std::string, std::vector<double>> gpuPassSamples;
                        totalFrameMilliseconds.reserve(
                            static_cast<std::size_t>(sampleFrames));
                        std::uint32_t maximumDrawCalls = 0;
                        bool gpuTimerQueriesObserved = false;
                        for (int frame = 0;
                             frame < warmupFrames + sampleFrames; ++frame) {
                            const auto before = std::chrono::steady_clock::now();
                            renderer.render(buildPresentationSnapshot(
                                previous, runtime.state(), 1.0,
                                runtime.shotControl(),
                                runtime.shotControlCapabilities(),
                                runtime.cameraFrame(),
                                buildRulesPresentation(runtime)));
                            glFlush();
                            const auto after = std::chrono::steady_clock::now();
                            std::uint32_t drawCalls = 0;
                            for (const RenderPassMetric& pass :
                                 renderer.metrics().passes) {
                                drawCalls += pass.drawCalls;
                                gpuTimerQueriesObserved =
                                    gpuTimerQueriesObserved ||
                                    pass.gpuTimingAvailable;
                                if (frame >= warmupFrames) {
                                    cpuPassSamples[pass.name].push_back(pass.cpuMilliseconds);
                                    if (pass.gpuTimingAvailable)
                                        gpuPassSamples[pass.name].push_back(pass.gpuMilliseconds);
                                }
                            }
                            maximumDrawCalls = std::max(
                                maximumDrawCalls, drawCalls);
                            if (frame >= warmupFrames) {
                                totalFrameMilliseconds.push_back(
                                    std::chrono::duration<double, std::milli>(
                                        after - before).count());
                            }
                        }
                        const BenchmarkPercentiles total = percentiles(
                            std::move(totalFrameMilliseconds));
                        const double residentMib = static_cast<double>(
                            renderer.metrics().pbrGpuBytes +
                            renderer.metrics().environmentGpuBytes) /
                            (1024.0 * 1024.0);
                        json::Value performance = json::Value::object();
                        performance["schema"] = json::Value(
                            "billiardgl_render_performance_v1");
                        performance["measurement_method"] = json::Value(
                            "gpu_timer_query_plus_cpu_frame");
                        // The legacy total-frame keys time render submission plus
                        // glFlush only. They are not an end-to-end product frame:
                        // this automation loop neither advances physics nor swaps.
                        performance["measurement_scope"] = json::Value(
                            "render_submission_and_flush_only");
                        performance["includes_simulation"] = json::Value(false);
                        performance["includes_display_swap"] = json::Value(false);
                        performance["qualifies_as_product_frame_budget"] = json::Value(false);
                        performance["warmup_frames"] =
                            json::Value(warmupFrames);
                        performance["sample_frames"] =
                            json::Value(sampleFrames);
                        performance["render_width"] = json::Value(measurementWidth);
                        performance["render_height"] = json::Value(measurementHeight);
                        performance["p95_total_frame_ms"] =
                            json::Value(total.p95);
                        performance["p50_total_frame_ms"] = json::Value(total.p50);
                        performance["p99_total_frame_ms"] = json::Value(total.p99);
                        const auto passPercentiles = [](const auto& samples) {
                            json::Value result = json::Value::object();
                            for (const auto& entry : samples) {
                                const auto measured = percentiles(entry.second);
                                json::Value values = json::Value::object();
                                values["p50_ms"] = json::Value(measured.p50);
                                values["p95_ms"] = json::Value(measured.p95);
                                values["p99_ms"] = json::Value(measured.p99);
                                values["sample_count"] = json::Value(
                                    static_cast<int>(entry.second.size()));
                                result[entry.first] = std::move(values);
                            }
                            return result;
                        };
                        performance["cpu_pass_percentiles"] = passPercentiles(cpuPassSamples);
                        performance["gpu_pass_percentiles"] = passPercentiles(gpuPassSamples);
                        performance["gpu_sample_method"] = json::Value(
                            "latest_newly_completed_query_per_pass_per_sampling_frame");
                        performance["draw_calls_complete_scene"] =
                            json::Value(static_cast<int>(maximumDrawCalls));
                        performance["texture_resident_mib"] =
                            json::Value(residentMib);
                        performance["gpu_timer_queries_observed"] =
                            json::Value(gpuTimerQueriesObserved);
                        performance["cpu_frame_clock"] = json::Value(
                            "std::chrono::steady_clock");
                        performance["draw_call_method"] = json::Value(
                            "sum_of_modern_frame_pass_draw_calls_max_over_samples");
                        performance["texture_residency_method"] = json::Value(
                            "renderer_uploaded_pbr_plus_environment_bytes");
                        return performance;
                    });
                renderer.shutdown();
                return result;
            }

            if (!options.screenshotPath.empty()) {
                std::string captureError;
                if (!renderer.capture(
                        {options.screenshotPath, options.width, options.height},
                        captureError)) {
                    throw std::runtime_error(captureError);
                }
                renderer.shutdown();
                return 0;
            }

            auto last = std::chrono::steady_clock::now();
            std::unique_ptr<diagnostics::PlaytestLogger> playtestLog;
            if (options.productMode) {
                const char* logDirectory = std::getenv("BILLIARDGL_PLAYTEST_LOG_DIR");
                const auto directory = logDirectory && *logDirectory ?
                    std::filesystem::path(logDirectory) :
                    std::filesystem::path(defaultProductSettingsPath())
                        .parent_path() / "playtest-logs";
                playtestLog = std::make_unique<diagnostics::PlaytestLogger>(
                    directory.string(), options.executableVersion, 32 * 1024 * 1024,
                    logDirectory && *logDirectory ?
                        diagnostics::PlaytestLogger::RetentionMode::FullEvidence :
                        diagnostics::PlaytestLogger::RetentionMode::BoundedRolling);
                json::Value metadata = json::Value::object();
                metadata["build_stamp"] = json::Value(__DATE__ " " __TIME__);
                const std::string executable = [[[NSProcessInfo processInfo]
                    arguments][0] UTF8String];
                metadata["executable_name"] = json::Value(
                    std::filesystem::path(executable).filename().string());
                std::ifstream binary(executable, std::ios::binary);
                if (binary) metadata["executable_sha256"] = json::Value(sha256Hex(
                    std::string(std::istreambuf_iterator<char>(binary), {})));
                metadata["fixed_step_seconds"] = json::Value(1.0 / 120.0);
                metadata["retention_mode"] = json::Value(playtestLog->fullEvidence() ?
                    "full_evidence" : "bounded_rolling_partial");
                metadata["explicit_log_directory"] = json::Value(logDirectory && *logDirectory);
                metadata["initial_settings"] = settings::serialize(
                    productSession->snapshot().effectiveSettings);
                playtestLog->record("session_start", runtime, metadata);
                errors << "Playtest diagnostics: " << playtestLog->path() << '\n';
                if (!playtestLog->good()) errors << "WARNING: playtest log unavailable\n";
            }
            const auto logContext = [&]() {
                json::Value context = json::Value::object();
                if (productSession) {
                    const auto training = productSession->training().snapshot();
                    context["scenario_id"] = json::Value(training.scenarioId);
                    context["training_pack_id"] = json::Value(training.packId);
                    context["scenario_sha256"] = json::Value(training.scenarioFileSha256);
                    context["attempt_seed"] = json::Value(static_cast<double>(training.attemptSeed));
                    context["training_state"] = json::Value(static_cast<int>(training.state));
                    context["training_shot_count"] = json::Value(static_cast<double>(training.attemptFacts.shotCount));
                    context["training_terminal_reason"] = json::Value(training.evaluation.reason);
                    context["training_feedback"] = json::Value(training.evaluation.feedback);
                    context["training_succeeded"] = json::Value(training.evaluation.succeeded);
                    context["training_failed"] = json::Value(training.evaluation.failed);
                    context["training_score"] = json::Value(training.evaluation.score);
                    context["training_step_index"] = json::Value(training.teachingStepIndex);
                    context["training_step_count"] = json::Value(training.teachingStepCount);
                    context["training_cue_rail_contacts"] = json::Value(training.attemptFacts.railContacts[0]);
                    if (training.teachingHints.available) {
                        context["training_target_pocket"] = json::Value(training.teachingHints.targetPocketId);
                        context["training_stop_zone_radius_cm"] = json::Value(training.teachingHints.cueBallTargetRadiusCm);
                        context["training_stop_zone_enabled"] = json::Value(training.teachingHints.cueBallTargetEnabled);
                        context["training_stop_zone_center_x_cm"] = json::Value(training.teachingHints.cueBallTargetCenterCm[0]);
                        context["training_stop_zone_center_z_cm"] = json::Value(training.teachingHints.cueBallTargetCenterCm[1]);
                        context["training_recommended_power_min_percent"] = json::Value(training.teachingHints.minimumPowerPercent);
                        context["training_recommended_power_max_percent"] = json::Value(training.teachingHints.maximumPowerPercent);
                    }
                    if (training.attemptFacts.hasCueBallPositionAtFirstContact) {
                        context["training_contact_cue_x_cm"] = json::Value(training.attemptFacts.cueBallPositionAtFirstContactCm.x);
                        context["training_contact_cue_z_cm"] = json::Value(training.attemptFacts.cueBallPositionAtFirstContactCm.z);
                    }
                    const auto shell = productSession->snapshot().shell;
                    context["screen_id"] = json::Value(shell.screenId);
                    context["inspector_mode"] = json::Value(shell.inspectorMode);
                    context["manual_inspector_policy"] = json::Value(shell.manualInspectorPolicy);
                    context["completed_training_count"] = json::Value(static_cast<double>(shell.completedTrainingCount));
                    context["inspector_scroll_row"] = json::Value(static_cast<double>(shell.inspectorScrollRow));
                    context["replay_tick"] = json::Value(std::to_string(shell.replay.tick));
                    context["replay_paused"] = json::Value(shell.replay.paused);
                    context["replay_finished"] = json::Value(shell.replay.finished);
                }
                return context;
            };
            bool logFailureReported = false;
            struct DiagnosticExitGuard {
                diagnostics::PlaytestLogger* logger;
                const GameRuntime& runtime;
                bool ended = false;
                ~DiagnosticExitGuard() {
                    if (logger && !ended) {
                        try { logger->record("session_interrupted", runtime); }
                        catch (...) {} // Diagnostics must never replace an application error.
                    }
                }
            } diagnosticExit{playtestLog.get(), runtime};
            std::uint64_t diagnosticFrame = 0;
            if (productAudioConsumer) {
                const auto prepared = productAudioConsumer->prepare();
                auto details = logContext();
                details["ok"] = json::Value(prepared.result.ok);
                details["error"] = json::Value(prepared.result.code);
                details["device_ms"] = json::Value(prepared.deviceMilliseconds);
                details["assets_ms"] = json::Value(prepared.assetMilliseconds);
                details["cached_assets"] = json::Value(static_cast<int>(
                    productAudioConsumer->status().cachedAssetCount));
                details["asset_pack"] = json::Value(productAudioConsumer->status().assetPackId);
                details["candidate_not_content_accepted"] = json::Value(
                    productAudioConsumer->status().candidateReview);
                if (playtestLog) playtestLog->record("audio_prepare", runtime, details);
                if (!prepared.result.ok)
                    errors << "WARNING: audio preparation failed: " << prepared.result.code << '\n';
                if (prepared.result.ok && productAudioConsumer->status().candidateReview)
                    productSession->startAudioLoops();
            }
            std::optional<ProductFramePerformanceCollector>
                productPerformance;
            bool productPerformanceShotDispatched = false;
            if (collectProductFramePerformance) {
                if (!playtestLog || !playtestLog->good())
                    throw std::runtime_error(
                        "product_performance_shipping_log_unavailable");
                const auto started =
                    productSession->shell().startSinglePlayer(0x4d3450455246554cULL);
                GameAction aim;
                aim.type = ActionType::SetAimYaw;
                aim.first = 1.57079632679489661923f;
                GameAction power;
                power.type = ActionType::SetShotPower;
                power.first = kProductPerformanceShotPower;
                if (!started.ok || !runtime.dispatch(aim).ok ||
                    !runtime.dispatch(power).ok ||
                    !runtime.setTipOffset(0.0f, 0.0f).ok ||
                    !runtime.setCalledPocket(-1).ok ||
                    runtime.state().ballsMoving) {
                    throw std::runtime_error(
                        "product_performance_native_shot_setup_failed");
                }
                previous = runtime.state();
                productPerformance.emplace();
            }
            last = std::chrono::steady_clock::now();
            double accumulator = 0.0;
            std::string lastLayoutDegradation;
            const auto syncInputViewport = [&]() {
                if (!productSession) return;
                const auto& authority = productSession->presentationRuntime();
                const auto frame = buildPresentationSnapshot(authority.state(), authority.state(), 1.0,
                    authority.shotControl(), authority.shotControlCapabilities(),
                    productSession->presentationCameraFrame(), buildRulesPresentation(authority));
                productSession->setRulesDecisionProjectionCamera(frame.camera);
                productSession->setShellViewport(ui::resolveProtectedShellViewport(
                    productSession->snapshot().shell, frame,
                    productShellViewport([platform.window contentView])));
            };
            constexpr double fixedStep = 1.0 / 120.0;
            NativeInputOwnership nativeInput(platform.window, productSession.get());
#if defined(BILLIARDGL_VISIBLE_DRAWABLE_SELF_TEST)
            const char* visibleTest =
                std::getenv("BILLIARDGL_VISIBLE_DRAWABLE_SELF_TEST");
            const bool verifyVisibleDrawable = visibleTest != nullptr &&
                std::string(visibleTest) == "1";
            const bool verifyVisibleInset = std::getenv("BILLIARDGL_VISIBLE_INSET_SELF_TEST") != nullptr;
            const char* insetMarkerFixture = std::getenv("BILLIARDGL_INSET_MARKER_FIXTURE");
            const bool verifyInsetMarkers = verifyVisibleInset && insetMarkerFixture != nullptr;
            float insetMarkerUiScale = 1.0f;
            const auto insetMarkerSource = verifyInsetMarkers ?
                loadInsetMarkerRecordedSource(insetMarkerFixture, insetMarkerUiScale) : training::TrajectoryOverlaySnapshot{};
            int visibleInsetStage = 0;
            const char* pixelMatrixTest =
                std::getenv("BILLIARDGL_VISIBLE_PIXEL_MATRIX_SELF_TEST");
            const bool verifyVisiblePixelMatrix = pixelMatrixTest != nullptr &&
                std::string(pixelMatrixTest) == "1";
            const char* pixelMatrixProfileValue =
                std::getenv("BILLIARDGL_VISIBLE_PIXEL_MATRIX_PROFILE");
            const std::string visiblePixelMatrixProfile =
                pixelMatrixProfileValue != nullptr ? pixelMatrixProfileValue : "";
            int visiblePixelMatrixStage = 0;
            std::unique_ptr<std::ofstream> visiblePixelMatrixReceipt;
            if (verifyVisiblePixelMatrix) {
                const char* captureDirectory =
                    std::getenv("BILLIARDGL_VISIBLE_CAPTURE_DIR");
                if (captureDirectory == nullptr || *captureDirectory == '\0' ||
                    visiblePixelMatrixProfile.empty()) {
                    throw std::runtime_error(
                        "visible_pixel_matrix_environment_incomplete");
                }
                const auto receiptPath = std::filesystem::path(captureDirectory) /
                    "native-pixel-matrix-receipt.jsonl";
                visiblePixelMatrixReceipt = std::make_unique<std::ofstream>(
                    receiptPath, std::ios::binary | std::ios::trunc);
                if (!*visiblePixelMatrixReceipt)
                    throw std::runtime_error(
                        "visible_pixel_matrix_receipt_open_failed");
                verifyVisibleCaptureFileSha256Contract(captureDirectory);
            }
            int verifiedVisibleFrames = 0;
            std::uint64_t verifiedPausedReplayTick = 0;
            int visibleRules06CStage = 0;
            std::uint64_t visibleRules06CRevision = 0;
            std::string visibleRules06CDecisionId;
            const std::string visibleRules06CActionId = []() {
                const char* requested = std::getenv(
                    "BILLIARDGL_VISIBLE_RULES_06C_ACTION");
                const std::string action = requested != nullptr ? requested :
                    rules::kBreakFoulAcceptTableActionId;
                if (action != rules::kBreakFoulAcceptTableActionId &&
                    action != rules::kBreakFoulRerackOpponentActionId &&
                    action != rules::kBreakFoulRerackOriginalActionId)
                    throw std::runtime_error(
                        "visible_06c_fixture_action_invalid");
                return action;
            }();
            std::string visibleRules06CStateHash;
            GameState visibleRules06CGameState;
            std::array<float, kBallCount> visibleRules06CSpeeds{};
            std::array<BallMotionState, kBallCount>
                visibleRules06CMotionStates{};
            std::uint64_t visibleRules06CArmFocusLossBaseline = 0;
            int visibleRules06CArmStage = 0;
            int visibleRules06EStage = 0;
            std::uint64_t visibleRules06EParentRevision = 0;
            std::uint64_t visibleRules06EChildRevision = 0;
            std::string visibleRules06EDecisionId;
            std::string visibleRules06EChildDecisionId;
            int visibleRules06EActor = -1;
            rules::BallInHandPlacementPosition visibleRules06EPlacement{};
            int visibleRules06FStage = 0;
            std::uint64_t visibleRules06FParentRevision = 0;
            std::uint64_t visibleRules06FChildRevision = 0;
            std::string visibleRules06FDecisionId;
            std::string visibleRules06FChildDecisionId;
            int visibleRules06FActor = -1;
            rules::BallInHandPlacementPosition visibleRules06FPlacement{};
            int visibleRulesGroupStage = 0;
            std::uint64_t visibleRulesGroupRevision = 0;
            std::string visibleRulesGroupDecisionId;
            int visibleRulesFullTableStage = 0;
            std::uint64_t visibleRulesFullTableRevision = 0;
            std::string visibleRulesFullTableDecisionId;
            int visibleRulesFullTableActor = -1;
            rules::BallInHandPlacementPosition
                visibleRulesFullTablePlacement{};
            NSSize firstVerifiedBackingSize = NSZeroSize;
#endif
            std::optional<double> previousSwapEndMilliseconds;
            const auto monotonicMilliseconds = [](const auto& timestamp) {
                return std::chrono::duration<double, std::milli>(
                    timestamp.time_since_epoch()).count();
            };
            std::optional<int> nativeReviewMaxSeconds;
            if (options.productMode) {
                if (const char* value = std::getenv("BILLIARDGL_PRODUCT_NATIVE_REVIEW_MAX_SECONDS")) {
                    const std::string text(value);
                    if (text.empty() || text.size() > 3 ||
                        !std::all_of(text.begin(), text.end(), [](char c) { return c >= '0' && c <= '9'; }))
                        throw std::runtime_error("native review max seconds must be an integer from 1 to 600");
                    const int seconds = std::stoi(text);
                    const char* captureDirectory = std::getenv("BILLIARDGL_PRODUCT_NATIVE_CAPTURE_DIR");
                    const char* logDirectory = std::getenv("BILLIARDGL_PLAYTEST_LOG_DIR");
                    if (seconds < 1 || seconds > 600 || !captureDirectory || !*captureDirectory ||
                        !logDirectory || !*logDirectory)
                        throw std::runtime_error("native review deadline requires capture and full evidence log directories");
                    nativeReviewMaxSeconds = seconds;
                    auto details = logContext();
                    details["native_review_max_seconds"] = json::Value(seconds);
                    playtestLog->record("native_review_config", runtime, std::move(details));
                }
            }
            const auto nativeReviewStarted = std::chrono::steady_clock::now();
            while (platform.window != nil && [platform.window isVisible]) {
                const auto frameLoopStart = std::chrono::steady_clock::now();
                if (nativeReviewMaxSeconds &&
                    frameLoopStart - nativeReviewStarted >= std::chrono::seconds(*nativeReviewMaxSeconds)) {
                    auto details = logContext();
                    details["termination_reason"] = json::Value("native_review_deadline");
                    details["native_review_max_seconds"] = json::Value(*nativeReviewMaxSeconds);
                    playtestLog->record("native_review_deadline", runtime, std::move(details));
                    break; // Normal session_end/receipt closure below; never synthesize a shot or drop a logged event.
                }
                ++diagnosticFrame;
                const double logStartMs = playtestLog ? playtestLog->recordMilliseconds() : 0.0;
                double physicsMs = 0.0, consumersMs = 0.0, audioMs = 0.0;
                syncInputViewport();
                if (productSession != nullptr)
                    productSession->setRulesDecisionHitRects(
                        renderer.latestProductUiResult().rulesDecisionHitRects);
                if (productSession != nullptr) {
                    std::vector<PointerProjectionRect> exclusions;
                    for (const auto& rect : renderer.latestProductUiResult().
                            rulesDecisionExclusionRects)
                        exclusions.push_back({rect.x, rect.y, rect.width,
                            rect.height});
                    productSession->setRulesDecisionExclusionRects(
                        std::move(exclusions));
                }
                NSEvent* event = nil;
                while ((event = [NSApp nextEventMatchingMask:NSEventMaskAny
                    untilDate:[NSDate distantPast] inMode:NSDefaultRunLoopMode dequeue:YES])) {
#if defined(BILLIARDGL_VISIBLE_DRAWABLE_SELF_TEST)
                    // Ambient host cursor motion is not part of the scripted
                    // visible test.  In Aim it is a real, explicit product
                    // adjustment, so letting an operator's hover enter this
                    // deterministic harness would overwrite verified lesson
                    // defaults.  Scripted native pointer events (including
                    // 06E placement) are dispatched directly below and are
                    // deliberately unaffected by this test-only isolation.
                    if ((verifyVisibleDrawable || verifyVisiblePixelMatrix || verifyVisibleInset) &&
                            [event type] == NSEventTypeMouseMoved &&
                            ([NSEvent pressedMouseButtons] & 3u) == 0)
                        continue;
                    // The inset self-test drives explicit product input below.
                    // A real keyboard event queued while this short-lived test
                    // window activates must not mutate its scripted state.
                    if (verifyVisibleInset &&
                        ([event type] == NSEventTypeKeyDown ||
                         [event type] == NSEventTypeKeyUp ||
                         [event type] == NSEventTypeFlagsChanged))
                        continue;
#endif
                    if (productSession != nullptr) {
                        syncInputViewport();
                        const auto translated = productPlatformEvent(
                            event, [platform.window contentView], nativeInput.pointer, *productSession);
                        if (translated.has_value()) {
                            auto inputLog = logContext();
                            inputLog["kind"] = json::Value(static_cast<int>(translated->kind));
                            inputLog["control"] = json::Value(translated->control);
                            inputLog["modifiers"] = json::Value(static_cast<int>(translated->modifiers));
                            inputLog["x"] = json::Value(translated->logicalX);
                            inputLog["y"] = json::Value(translated->logicalY);
                            inputLog["dx"] = json::Value(translated->deltaX);
                            inputLog["dy"] = json::Value(translated->deltaY);
                            inputLog["scroll_y"] = json::Value(translated->scrollY);
                            inputLog["backing_scale"] = json::Value(translated->backingScale);
                            inputLog["left_down"] = json::Value(translated->leftButtonDown);
                            inputLog["right_down"] = json::Value(translated->rightButtonDown);
                            if (playtestLog) playtestLog->record("input_before", runtime, inputLog);
                            const ModernProductEventResult result =
                                productSession->handlePlatformInput(*translated);
                            inputLog["ok"] = json::Value(result.ok);
                            inputLog["handled"] = json::Value(result.handled);
                            inputLog["error"] = json::Value(result.errorCode);
                            inputLog["resolved_action"] = json::Value(result.action);
                            inputLog["after_context"] = logContext();
                            if (playtestLog) playtestLog->record("input_after", runtime, inputLog);
                            if (result.appRequest == ProductShellAppRequest::Quit)
                                [platform.window close];
                        }
                    } else {
                        const bool aiControlsTurn = options.aiOpponent &&
                            runtime.matchState().currentPlayer == 1;
                        if ([event type] == NSEventTypeKeyDown) {
                            NSString* characters = [event charactersIgnoringModifiers];
                            if ([characters length] > 0) {
                                const unsigned char key = static_cast<unsigned char>([characters characterAtIndex:0]);
                                if (key == 27) [platform.window close];
                                else if (!aiControlsTurn)
                                    dispatchCompatibilityKey(runtime, key);
                            }
                        } else if ([event type] == NSEventTypeScrollWheel) {
                            GameAction action; action.type = ActionType::MouseWheel;
                            action.firstInt = [event scrollingDeltaY] > 0 ? 1 : -1;
                            if (!aiControlsTurn) (void)runtime.dispatch(action);
                        } else if ([event type] == NSEventTypeMouseMoved
                                   || [event type] == NSEventTypeLeftMouseDragged
                                   || [event type] == NSEventTypeRightMouseDragged) {
                            const NSPoint point = [event locationInWindow];
                            GameAction action; action.type = ActionType::MouseMove;
                            action.firstInt = static_cast<int>(point.x);
                            action.secondInt = static_cast<int>(point.y);
                            if (!aiControlsTurn) (void)runtime.dispatch(action);
                        } else if ([event type] == NSEventTypeLeftMouseDown
                                   || [event type] == NSEventTypeLeftMouseUp
                                   || [event type] == NSEventTypeRightMouseDown
                                   || [event type] == NSEventTypeRightMouseUp) {
                            const bool right = [event type] == NSEventTypeRightMouseDown
                                || [event type] == NSEventTypeRightMouseUp;
                            const bool down = [event type] == NSEventTypeLeftMouseDown
                                || [event type] == NSEventTypeRightMouseDown;
                            const NSPoint point = [event locationInWindow];
                            GameAction action; action.type = ActionType::MouseButton;
                            action.firstInt = right ? 1 : 0;
                            action.secondInt = down ? 0 : 1;
                            action.thirdInt = static_cast<int>(point.x);
                            action.first = static_cast<float>(point.y);
                            if (!aiControlsTurn) (void)runtime.dispatch(action);
                        }
                    }
                    [NSApp sendEvent:event];
                }
                [NSApp updateWindows];
                [platform.context makeCurrentContext];
                platform.updateDrawableIfNeeded();
                const NSRect backing = [[platform.window contentView] convertRectToBacking:
                    [[platform.window contentView] bounds]];
                const int framebufferWidth = std::max(1, static_cast<int>(backing.size.width));
                const int framebufferHeight = std::max(1, static_cast<int>(backing.size.height));
                if (collectProductFramePerformance &&
                        (framebufferWidth != options.width ||
                        framebufferHeight != options.height)) {
                    throw std::runtime_error(
                        "product_performance_drawable_size_changed");
                }
                if (runtime.state().config.width != framebufferWidth
                    || runtime.state().config.height != framebufferHeight) {
                    GameAction resize; resize.type=ActionType::Resize;
                    resize.firstInt=framebufferWidth;resize.secondInt=framebufferHeight;
                    (void)runtime.dispatch(resize);renderer.resize(framebufferWidth,framebufferHeight);
                }
                const auto now = std::chrono::steady_clock::now();
                const double presentedFrameElapsed =
                    std::chrono::duration<double>(now - last).count();
                last = now;
                const ai::PlayerState aiState = productSession != nullptr ?
                    productSession->pollAi() : aiPlayer.poll(runtime);
                const bool preserveAiAuthority = aiState.thinking &&
                    !runtime.state().ballsMoving;
                FrameStepResult frameSteps;
                if (preserveAiAuthority) accumulator = 0.0;
                else frameSteps = advanceFixedStepAccumulator(
                    accumulator, presentedFrameElapsed, fixedStep, 8);
                int steps = 0;
                while (steps < frameSteps.steps) {
                    previous = runtime.state();
                    const auto physicsStart = std::chrono::steady_clock::now();
                    const auto stepResult = runtime.step(1);
                    physicsMs += std::chrono::duration<double, std::milli>(
                        std::chrono::steady_clock::now() - physicsStart).count();
                    if (playtestLog) {
                        auto details = logContext();
                        details["ok"] = json::Value(stepResult.ok);
                        details["error"] = json::Value(stepResult.errorCode);
                        json::Value::Array beforeBalls;
                        for (int id = 0; id < kBallCount; ++id) {
                            const auto& ball = previous.balls[id];
                            json::Value entry = json::Value::object();
                            entry["id"] = json::Value(id);
                            entry["x"] = json::Value(ball.position.x);
                            entry["y"] = json::Value(ball.position.y);
                            entry["z"] = json::Value(ball.position.z);
                            entry["vx"] = json::Value(ball.velocity.x);
                            entry["vy"] = json::Value(ball.velocity.y);
                            entry["vz"] = json::Value(ball.velocity.z);
                            entry["out_of_play"] = json::Value(ball.outOfPlay);
                            entry["pocketed"] = json::Value(ball.pocketed);
                            beforeBalls.push_back(std::move(entry));
                        }
                        details["before_balls"] = json::Value(beforeBalls);
                        playtestLog->record("physics_step", runtime, details);
                    }
                    const auto consumersStart = std::chrono::steady_clock::now();
                    if (productSession != nullptr) {
                        productSession->consumeAuthoritySteps();
                        audioMs += productSession->lastAudioConsumeMilliseconds();
                    }
                    consumersMs += std::chrono::duration<double, std::milli>(
                        std::chrono::steady_clock::now() - consumersStart).count();
                    ++steps;
                }
                if (preserveAiAuthority) accumulator = 0.0;
                if (productSession != nullptr)
                    (void)productSession->advancePresentedFrame(
                        presentedFrameElapsed);
                const GameRuntime& presentationRuntime =
                    productSession != nullptr ?
                    productSession->presentationRuntime() : runtime;
                const bool presentingLiveAuthority =
                    &presentationRuntime == &runtime;
                const auto presentation = buildPresentationSnapshot(
                    presentingLiveAuthority ? previous :
                        presentationRuntime.state(),
                    presentationRuntime.state(),
                    presentingLiveAuthority ? frameInterpolationAlpha(accumulator, fixedStep) : 1.0,
                    presentationRuntime.shotControl(),
                    presentationRuntime.shotControlCapabilities(),
                    productSession != nullptr ?
                        productSession->presentationCameraFrame() :
                        presentationRuntime.cameraFrame(),
                    buildRulesPresentation(presentationRuntime));
                if (playtestLog) {
                    auto details = logContext();
                    details["frame_seconds"] = json::Value(presentedFrameElapsed);
                    const auto protectedViewport = ui::resolveProtectedShellViewport(
                        productSession->snapshot().shell, presentation,
                        productShellViewport([platform.window contentView]));
                    const auto layout = ui::buildShippingScreenLayout(
                        productSession->snapshot().shell, protectedViewport);
                    details["inspector_collapsed"] = json::Value(layout.inspectorCollapsed);
                    details["layout_degradation"] = json::Value(layout.degradationReason);
                    if (layout.degradationReason != lastLayoutDegradation) {
                        auto change = logContext();
                        change["reason"] = json::Value(layout.degradationReason);
                        change["collapsed"] = json::Value(layout.inspectorCollapsed);
                        playtestLog->record("ui_layout_changed", presentationRuntime, change);
                        lastLayoutDegradation = layout.degradationReason;
                    }
                    details["physics_steps"] = json::Value(steps);
                    details["accumulator_seconds"] = json::Value(accumulator);
                    details["catchup_clamped"] = json::Value(frameSteps.clamped);
                    details["dropped_wall_seconds"] = json::Value(frameSteps.droppedSeconds);
                    details["physics_ms"] = json::Value(physicsMs);
                    details["authority_consumers_ms"] = json::Value(consumersMs);
                    details["audio_ms"] = json::Value(audioMs);
                    details["live_authority"] = json::Value(presentingLiveAuthority);
                    details["frame_id"] = json::Value(std::to_string(diagnosticFrame));
                    json::Value view = json::Value::object();
                    view["profile_id"] = json::Value(productCameraProfileId);
                    view["eye_x"] = json::Value(presentation.camera.eye.x);
                    view["eye_y"] = json::Value(presentation.camera.eye.y);
                    view["eye_z"] = json::Value(presentation.camera.eye.z);
                    view["target_x"] = json::Value(presentation.camera.target.x);
                    view["target_y"] = json::Value(presentation.camera.target.y);
                    view["target_z"] = json::Value(presentation.camera.target.z);
                    view["fov_degrees"] = json::Value(presentation.camera.fieldOfViewDegrees);
                    view["width"] = json::Value(presentation.camera.viewportWidth);
                    view["height"] = json::Value(presentation.camera.viewportHeight);
                    details["rendered_camera"] = std::move(view);
                    json::Value::Array balls;
                    for (const auto& ball : presentation.balls) {
                        json::Value item = json::Value::object();
                        item["id"] = json::Value(ball.identity);
                        item["x"] = json::Value(ball.position.x);
                        item["y"] = json::Value(ball.position.y);
                        item["z"] = json::Value(ball.position.z);
                        item["visible"] = json::Value(ball.visible);
                        balls.push_back(std::move(item));
                    }
                    details["rendered_balls"] = json::Value(std::move(balls));
                    playtestLog->record("presentation_prepare", presentationRuntime, details);
                    if (!playtestLog->good() && !logFailureReported) {
                        errors << "WARNING: playtest logging failed; diagnostics incomplete\n";
                        logFailureReported = true;
                    }
                }
                const auto renderStart = std::chrono::steady_clock::now();
                std::optional<ModernProductSessionSnapshot> presentedProductSnapshot;
                try {
                if (productSession != nullptr) {
                    productSession->setShellViewport(ui::resolveProtectedShellViewport(
                        productSession->snapshot().shell, presentation,
                        productShellViewport([platform.window contentView])));
                    productSession->prepareTrajectoryInsetCurrentFrame(
                        std::max(1, options.msaaSamples));
                    presentedProductSnapshot = productSession->snapshot();
#if defined(BILLIARDGL_VISIBLE_DRAWABLE_SELF_TEST)
                    if (verifyInsetMarkers && visibleInsetStage == 1) {
                        // Rebind the immutable presentation copy to the explicit
                        // recorded preview/replay input. The game authority and
                        // ProductSession remain untouched by this fixture.
                        presentedProductSnapshot->shell.trajectoryOverlay = insetMarkerSource;
                        presentedProductSnapshot->shell.uiScale = insetMarkerUiScale;
                        // Fixture text round-trips a float (1.35000002); settings
                        // retain their exact authored hundredth within [0.85,1.35].
                        presentedProductSnapshot->effectiveSettings.uiScale =
                            std::round(static_cast<double>(insetMarkerUiScale) * 100.0) / 100.0;
                        const bool cvd = std::getenv("BILLIARDGL_INSET_MARKER_CVD") != nullptr;
                        presentedProductSnapshot->effectiveSettings.colorVisionMode = cvd ?
                            settings::ColorVisionMode::Deuteranopia : settings::ColorVisionMode::Default;
                        auto& context = presentedProductSnapshot->trajectoryInsetVisibleCurrentContext;
                        if (!context) throw std::runtime_error("inset_marker_current_context_missing");
                        context->sourceContentSha256 = ui::trajectoryInsetSourceContentSha256(
                            presentedProductSnapshot->shell);
                        context->palette = cvd ? training::TrajectoryInsetPalette::ColorVisionSafe :
                            training::TrajectoryInsetPalette::Standard;
                    }
                    // This historical fixture explicitly audits the optional
                    // world plane. It is not evidence for the primary inset.
                    if (verifyVisiblePixelMatrix) {
                        presentedProductSnapshot->shell.trajectoryInsetEnabled = false;
                        presentedProductSnapshot->shell.worldTrajectoryOverlayEnabled = true;
                        presentedProductSnapshot->trajectoryInsetVisibleCurrentContext.reset();
                    }
#endif
                    renderer.renderProductFrame(presentation,
                        *presentedProductSnapshot, {true, false, false});
                    productSession->setPresentedTrajectoryInsetToggle(
                        renderer.metrics().productPresentPerformed ?
                            renderer.latestTrajectoryInsetToggleRect() : std::nullopt,
                        renderer.latestTrajectoryInsetFrameSerial());
                    if (renderer.metrics().productPresentPerformed)
                        productSession->setTrajectoryInsetPresentationStatus(
                            renderer.metrics().trajectoryInsetVisibleErrorCode);
                } else {
                    renderer.render(presentation);
                }
                } catch (const std::exception& error) {
                    if (playtestLog) {
                        auto details = logContext();
                        details["error"] = json::Value(error.what());
                        playtestLog->record("render_error", presentationRuntime, details);
                    }
                    throw;
                }
#if defined(BILLIARDGL_VISIBLE_DRAWABLE_SELF_TEST)
                if (verifyVisibleInset) {
                    const bool expectedVisible = visibleInsetStage == 1 ||
                        visibleInsetStage == 3 || visibleInsetStage == 4 ||
                        visibleInsetStage == 5 || visibleInsetStage == 6 || visibleInsetStage == 7;
                    const bool visible = renderer.publishedProductOutputFramebuffer() != 0;
                    output << "inset_layout stage=" << visibleInsetStage << ' '
                        << renderer.metrics().trajectoryInsetLayoutDiagnostic << '\n';
                    const char* directory = std::getenv("BILLIARDGL_VISIBLE_CAPTURE_DIR");
                    std::vector<unsigned char> insetBackBuffer;
                    if (directory && renderer.metrics().productPresentPerformed)
                        verifyVisibleDefaultFramebuffer(framebufferWidth, framebufferHeight,
                            (std::filesystem::path(directory) / ("inset-stage-" +
                                std::to_string(visibleInsetStage) + ".ppm")).string(), &insetBackBuffer);
                    if (visible != expectedVisible || !renderer.metrics().productPresentPerformed)
                        throw std::runtime_error("visible_inset_stage_" + std::to_string(visibleInsetStage) +
                            ":" + renderer.metrics().trajectoryInsetVisibleErrorCode);
                    if (visible) {
                        const auto plot=renderer.latestTrajectoryInsetPlotRect();
                        if (!plot || insetBackBuffer.empty()) throw std::runtime_error("inset_native_plot_missing");
                        const auto encoded=[](double linear) {
                            return static_cast<int>(std::lround(255.0*(linear<=.0031308 ?
                                12.92*linear : 1.055*std::pow(linear,1.0/2.4)-.055)));
                        };
                        const bool cvd=presentedProductSnapshot->effectiveSettings.colorVisionMode != settings::ColorVisionMode::Default;
                        const std::array<int,3> predicted{{255,encoded(cvd?.4:.78),encoded(cvd?.02:.08)}};
                        const std::array<int,3> actual{{encoded(cvd?.05:.01),encoded(cvd?.22:.28),encoded(cvd?.80:.55)}};
                        std::size_t predictedPixels=0,actualPixels=0;
                        for(int y=std::max(0,static_cast<int>(std::ceil(plot->y)));
                            y<std::min(framebufferHeight,static_cast<int>(std::floor(plot->y+plot->height)));++y)
                            for(int x=std::max(0,static_cast<int>(std::ceil(plot->x)));
                                x<std::min(framebufferWidth,static_cast<int>(std::floor(plot->x+plot->width)));++x) {
                                const auto index=(static_cast<std::size_t>(framebufferHeight-1-y)*framebufferWidth+x)*4;
                                const auto matches=[&](const auto& color) {
                                    for(int c=0;c<3;++c)
                                        if(std::abs(static_cast<int>(insetBackBuffer[index+c])-color[c])>2) return false;
                                    return true;
                                };
                                predictedPixels+=matches(predicted); actualPixels+=matches(actual);
                            }
                        output << "inset_plot_pixels stage=" << visibleInsetStage << " predicted=" << predictedPixels
                            << " actual=" << actualPixels << " rect=" << plot->x << ',' << plot->y << ','
                            << plot->width << ',' << plot->height << '\n';
                        const auto& source=presentedProductSnapshot->shell.trajectoryOverlay;
                        if ((source.predicted.visible && source.predicted.samples.size()>=2 && predictedPixels<2) ||
                            (source.actual.visible && source.actual.samples.size()>=2 && actualPixels<2))
                            throw std::runtime_error("inset_native_route_not_visible_inside_plot");
                    }
                    output << "visible_inset stage=" << visibleInsetStage << " visible=" << visible
                        << " frame=" << renderer.latestTrajectoryInsetFrameSerial()
                        << " width=" << framebufferWidth << " height=" << framebufferHeight
                        << " world_vertices=" << renderer.metrics().trajectoryOverlayVertices << '\n';
                    if (verifyInsetMarkers && visibleInsetStage == 1) {
                        GLint previousRead = 0, previousBuffer = 0;
                        glGetIntegerv(GL_READ_FRAMEBUFFER_BINDING, &previousRead);
                        glGetIntegerv(GL_READ_BUFFER, &previousBuffer);
                        glBindFramebuffer(GL_READ_FRAMEBUFFER, renderer.publishedProductOutputFramebuffer());
                        glReadBuffer(GL_COLOR_ATTACHMENT1);
                        std::vector<std::uint32_t> owners(static_cast<std::size_t>(framebufferWidth) * framebufferHeight);
                        glReadPixels(0, 0, framebufferWidth, framebufferHeight, GL_RED_INTEGER,
                            GL_UNSIGNED_INT, owners.data());
                        const auto readError = glGetError();
                        glBindFramebuffer(GL_READ_FRAMEBUFFER, static_cast<GLuint>(previousRead));
                        glReadBuffer(static_cast<GLenum>(previousBuffer));
                        if (readError != GL_NO_ERROR || glGetError() != GL_NO_ERROR)
                            throw std::runtime_error("inset_marker_native_owner_readback_failed");
                        if (!directory) throw std::runtime_error("inset_marker_capture_directory_missing");
                        std::ofstream ownerFile(std::filesystem::path(directory) / "final-semantic-owner.u32",
                            std::ios::binary);
                        ownerFile.write(reinterpret_cast<const char*>(owners.data()),
                            static_cast<std::streamsize>(owners.size() * sizeof(std::uint32_t)));
                        if (!ownerFile) throw std::runtime_error("inset_marker_owner_write_failed");
                        output << "inset_marker_fixture origin=explicit_recorded_preview_replay_fixture"
                            << " path=" << insetMarkerFixture
                            << " fixture_sha256=" << visibleCaptureFileSha256(insetMarkerFixture)
                            << " source_content_sha256=" << presentedProductSnapshot->trajectoryInsetVisibleCurrentContext->sourceContentSha256
                            << " native_owner_readback=PASS\n";
                        [platform.window close];
                        ++visibleInsetStage;
                        continue;
                    }
                    const auto activate = [&](const char* id) {
                        productSession->shell().setInspectorExpanded(true);
                        syncInputViewport();
                        std::optional<product_hud::HudRect> target;
                        for (std::size_t row=0;row<32 && !target;++row) {
                            productSession->shell().setInspectorScrollRow(row);
                            const auto layout=productSession->shellLayout();
                            for (const auto& element:layout.elements)
                                if(element.id==id && element.enabled) target=element.rect;
                        }
                        if (!target) throw std::runtime_error(std::string("inset_control_missing:")+id);
                        const auto viewport=productSession->snapshot().shellViewport;
                        PlatformInputEvent event;
                        event.control = "LeftButton";
                        event.logicalX=(target->x+target->width*.5)*viewport.logicalWidth/viewport.physicalWidth;
                        event.logicalY=(target->y+target->height*.5)*viewport.logicalHeight/viewport.physicalHeight;
                        event.kind = PlatformInputKind::PointerButtonDown;
                        if (!productSession->handlePlatformInput(event).ok)
                            throw std::runtime_error("inset_control_pointer_down_failed");
                        event.kind = PlatformInputKind::PointerButtonUp;
                        const auto activated = productSession->handlePlatformInput(event);
                        if (!activated.ok)
                            throw std::runtime_error(std::string("inset_control_failed:") + id + ":" + activated.errorCode);
                    };
                    if (visibleInsetStage == 0) {
                        const auto started = productSession->shell().startTrainingScenario(
                            "launch_straight_draw_intro_v2", "billiardgl-product-source-v1", 0);
                        if (!started.ok) throw std::runtime_error("inset_training_setup:" + started.errorCode);
                        if (runtime.state().aim.mode != AimMode::Aim) {
                            PlatformInputEvent aim;
                            aim.kind = PlatformInputKind::KeyDown;
                            aim.control = "Tab";
                            if (!productSession->handlePlatformInput(aim).ok)
                                throw std::runtime_error("inset_training_aim_failed");
                        }
                        activate("preview_training");
                        if (verifyInsetMarkers) productSession->shell().setInspectorExpanded(false);
                        previous = runtime.state();
                    } else if (visibleInsetStage == 1) {
                        const auto rect = renderer.latestTrajectoryInsetToggleRect();
                        if (!rect) throw std::runtime_error("inset_toggle_not_presented");
                        const auto viewport = presentedProductSnapshot->shellViewport;
                        PlatformInputEvent event;
                        event.control = "LeftButton";
                        event.logicalX = (rect->x + rect->width*.5) * viewport.logicalWidth / viewport.physicalWidth;
                        event.logicalY = (rect->y + rect->height*.5) * viewport.logicalHeight / viewport.physicalHeight;
                        event.kind = PlatformInputKind::PointerButtonDown;
                        if (!productSession->handlePlatformInput(event).ok)
                            throw std::runtime_error("inset_toggle_down_failed");
                        event.kind = PlatformInputKind::PointerButtonUp;
                        if (!productSession->handlePlatformInput(event).ok)
                            throw std::runtime_error("inset_toggle_up_failed");
                    } else if (visibleInsetStage == 2) {
                        activate("trajectory_inset_toggle");
                    } else if (visibleInsetStage == 3) {
                        const auto result = productSession->shell().watchTrainingReferenceReplay();
                        if (!result.ok) throw std::runtime_error("inset_demo_setup:" + result.errorCode);
                        for (int i = 0; i < 900 &&
                            productSession->snapshot().shell.replay.cueTrajectory.size() < 24; ++i)
                            activate("replay_step_tick");
                    } else if (visibleInsetStage == 4) {
                        [platform.window setContentSize:NSMakeSize(1100, 740)];
                    } else if (visibleInsetStage == 5) {
                        PlatformInputEvent seek;
                        seek.kind=PlatformInputKind::KeyDown;
                        seek.control="PageUp";
                        const auto result=productSession->handlePlatformInput(seek);
                        if (!result.ok) throw std::runtime_error("inset_seek_failed:"+result.errorCode);
                    } else if (visibleInsetStage == 6) {
                        for (int i = 0; i < 900 &&
                            productSession->snapshot().shell.replay.cueTrajectory.size() < 24; ++i)
                            activate("replay_step_tick");
                    } else {
                        [platform.window close];
                    }
                    ++visibleInsetStage;
                } else if (verifyVisiblePixelMatrix) {
                    static constexpr std::array<const char*, 7> kStateNames{{
                        "default", "aim", "post_shot", "foul_modal",
                        "help", "training", "replay"}};
                    static constexpr std::array<const char*, 7> kOrigins{{
                        "real_product_startup",
                        "real_match_aim_controls",
                        "real_match_shot_completion",
                        "explicit_authoritative_rules_facts_fixture",
                        "real_shell_navigation",
                        "explicit_training_controller_fixture",
                        "trusted_reference_replay_consumer"}};
                    static constexpr std::array<ProductScreen, 7> kScreens{{
                        ProductScreen::MainMenu,
                        ProductScreen::Match,
                        ProductScreen::Match,
                        ProductScreen::Match,
                        ProductScreen::Help,
                        ProductScreen::TrainingSession,
                        ProductScreen::ReplayPlayback}};
                    if (visiblePixelMatrixStage < 0 ||
                        visiblePixelMatrixStage >= static_cast<int>(kStateNames.size()))
                        throw std::runtime_error(
                            "visible_pixel_matrix_stage_out_of_range");
                    const int expectedWidth =
                        visiblePixelMatrixProfile == "retina-1280x720" ?
                            2560 : options.width;
                    const int expectedHeight =
                        visiblePixelMatrixProfile == "retina-1280x720" ?
                            1440 : options.height;
                    if (framebufferWidth != expectedWidth ||
                        framebufferHeight != expectedHeight) {
                        throw std::runtime_error(
                            "visible_pixel_matrix_drawable_mismatch:expected=" +
                            std::to_string(expectedWidth) + "x" +
                            std::to_string(expectedHeight) + ":actual=" +
                            std::to_string(framebufferWidth) + "x" +
                            std::to_string(framebufferHeight));
                    }
                    const auto& sessionSnapshot = *presentedProductSnapshot;
                    if (sessionSnapshot.shell.screen !=
                        kScreens[static_cast<std::size_t>(visiblePixelMatrixStage)])
                        throw std::runtime_error(
                            "visible_pixel_matrix_screen_mismatch:" +
                            std::string(kStateNames[static_cast<std::size_t>(
                                visiblePixelMatrixStage)]));
                    const ModernProductUiRuntimeResult uiResult =
                        renderer.latestProductUiResult();
                    const ModernFrameMetrics normalFrameMetrics =
                        renderer.metrics();
                    if (!normalFrameMetrics.productPresentPerformed)
                        throw std::runtime_error(
                            "visible_pixel_matrix_product_present_blocked:" +
                            normalFrameMetrics.
                                trajectoryInsetVisibleErrorCode);
                    if (!uiResult.ok || !uiResult.gpuSubmitted ||
                        uiResult.gpuMetrics.drawCalls == 0 ||
                        uiResult.gpuMetrics.sdfVertices == 0 ||
                        !uiResult.glyphAtlasBindingVerified ||
                        uiResult.glyphQuads.empty() ||
                        uiResult.glyphAtlasRecordSha256.size() !=
                            uiResult.glyphQuads.size())
                        throw std::runtime_error(
                            "visible_pixel_matrix_product_ui_not_submitted");
                    const double backingScale = std::max<double>(1.0,
                        [platform.window backingScaleFactor]);
                    const auto palette =
                        sessionSnapshot.effectiveSettings.colorVisionMode ==
                            settings::ColorVisionMode::Default ?
                            training::TrajectoryOverlayPalette::Standard :
                            training::TrajectoryOverlayPalette::ColorVisionSafe;
                    const auto trajectory =
                        training::buildTrajectoryOverlayDrawPackage(
                            sessionSnapshot.shell.trajectoryOverlay,
                            presentation.camera,
                            {framebufferWidth, framebufferHeight, backingScale,
                                sessionSnapshot.effectiveSettings.uiScale},
                            palette);
                    const bool predictedRequired = visiblePixelMatrixStage == 5;
                    const bool actualRequired = visiblePixelMatrixStage == 6;
                    if ((predictedRequired && (!trajectory.ok ||
                            trajectory.predicted.visibleStrokeCount == 0 ||
                            trajectory.predicted.vertexCount == 0)) ||
                        (actualRequired && (!trajectory.ok ||
                            trajectory.actual.visibleStrokeCount == 0 ||
                            trajectory.actual.vertexCount == 0)))
                        throw std::runtime_error(
                            "visible_pixel_matrix_trajectory_missing:" +
                            std::string(kStateNames[static_cast<std::size_t>(
                                visiblePixelMatrixStage)]));
                    const char* captureDirectory =
                        std::getenv("BILLIARDGL_VISIBLE_CAPTURE_DIR");
                    const auto capturePath =
                        std::filesystem::path(captureDirectory) /
                        (std::string(kStateNames[static_cast<std::size_t>(
                            visiblePixelMatrixStage)]) + ".ppm");
                    std::vector<unsigned char> normalBackBuffer;
                    verifyVisibleDefaultFramebuffer(framebufferWidth,
                        framebufferHeight, capturePath.string(),
                        &normalBackBuffer);
                    std::string normalBackBufferRgb;
                    normalBackBufferRgb.reserve(normalBackBuffer.size() / 4u *
                        3u);
                    for (std::size_t byte = 0; byte < normalBackBuffer.size();
                            byte += 4u) {
                        normalBackBufferRgb.push_back(static_cast<char>(
                            normalBackBuffer[byte]));
                        normalBackBufferRgb.push_back(static_cast<char>(
                            normalBackBuffer[byte + 1u]));
                        normalBackBufferRgb.push_back(static_cast<char>(
                            normalBackBuffer[byte + 2u]));
                    }
                    training::IntegratedFinalOwnershipFrame ownershipFrame;
                    std::string ownershipCaptureError;
                    const std::string ownershipCaptureId =
                        "native-pixel-matrix:" + visiblePixelMatrixProfile +
                        ":" + kStateNames[static_cast<std::size_t>(
                            visiblePixelMatrixStage)];
                    if (!renderer.captureIntegratedFinalOwnership(
                            ownershipCaptureId, ownershipFrame,
                            ownershipCaptureError))
                        throw std::runtime_error(
                            "visible_pixel_matrix_ownership_capture_failed:" +
                            ownershipCaptureError);
                    if (ownershipFrame.colorContentSha256 !=
                            sha256Hex(normalBackBufferRgb))
                        throw std::runtime_error(
                            "visible_pixel_matrix_ownership_color_unbound");
                    training::IntegratedFinalOwnershipRequest
                        ownershipRequest;
                    ownershipRequest.frame = std::move(ownershipFrame);
                    ownershipRequest.expectedSourceProbeCount =
                        trajectory.contrastProbes.size();
                    ownershipRequest.minimumVisibleCoverage = 0.90;
                    ownershipRequest.minimumEdgeContrastRatio = 4.0;
                    for (const auto& probe : trajectory.contrastProbes) {
                        training::FinalOwnershipSourceProbe source;
                        source.source.captureId = ownershipCaptureId;
                        source.source.track = probe.track ==
                                training::TrajectoryContrastTrack::Predicted ?
                            training::CompoundVisibilityTrack::Predicted :
                            training::CompoundVisibilityTrack::Actual;
                        source.source.primitiveId = probe.primitiveId;
                        source.source.componentRole = probe.componentRole;
                        source.source.arcSegmentId = probe.arcSegmentId;
                        source.source.probeOrdinal = probe.probeOrdinal;
                        source.source.physicalProfile =
                            visiblePixelMatrixProfile;
                        source.source.xPhysicalPx =
                            probe.strokeXPhysicalPx;
                        source.source.yPhysicalPx =
                            probe.strokeYPhysicalPx;
                        source.source.outerHaloRadiusPx =
                            trajectory.style.lineWidthPx * 0.5 +
                            trajectory.style.haloWidthPx +
                            trajectory.style.outerContrastRimWidthPx;
                        source.source.halfTickLengthPx =
                            trajectory.style.actualTickLengthPx * 0.5;
                        source.source.sourceProbeSpacingPx =
                            probe.componentRole ==
                                    training::CompoundComponentRole::Tick ?
                                trajectory.style.actualTickSpacingPx :
                                trajectory.style.dashOnPx +
                                    trajectory.style.dashOffPx;
                        source.source.insideFinalViewport =
                            probe.strokeXPhysicalPx >= 0 &&
                            probe.strokeYPhysicalPx >= 0 &&
                            probe.strokeXPhysicalPx < framebufferWidth &&
                            probe.strokeYPhysicalPx < framebufferHeight;
                        source.tangentX = probe.localTangentX;
                        source.tangentY = probe.localTangentY;
                        source.normalX = probe.localNormalX;
                        source.normalY = probe.localNormalY;
                        ownershipRequest.sourceProbes.push_back(
                            std::move(source));
                    }
                    const training::IntegratedFinalOwnershipResult
                        ownershipResult =
                            training::resolveIntegratedFinalSemanticOwnership(
                                ownershipRequest);
                    // Contrast probes intentionally alternate core/halo
                    // samples and the predicted endpoint samples its ring
                    // edge.  Geometry needs one canonical compound identity
                    // per authored primitive and the actual endpoint center;
                    // do not reinterpret those contrast coordinates inside
                    // the ownership resolver above.
                    std::vector<training::FinalOwnershipSourceProbe>
                        geometrySources = ownershipRequest.sourceProbes;
                    for (auto& source : geometrySources) {
                        if (source.source.componentRole == training::CompoundComponentRole::Tick) {
                            double left = std::numeric_limits<double>::infinity();
                            double top = left, right = -left, bottom = -left;
                            for (const auto& batch : trajectory.semanticBatches) {
                                if (batch.primitiveId != source.source.primitiveId) continue;
                                for (std::size_t i = batch.firstVertex;
                                     i < batch.firstVertex + batch.vertexCount; ++i) {
                                    const auto& vertex = trajectory.vertices[i];
                                    left = std::min(left, static_cast<double>(vertex.xPhysicalPx));
                                    right = std::max(right, static_cast<double>(vertex.xPhysicalPx));
                                    top = std::min(top, static_cast<double>(vertex.yPhysicalPx));
                                    bottom = std::max(bottom, static_cast<double>(vertex.yPhysicalPx));
                                }
                            }
                            if (std::isfinite(left))
                                source.geometryCenter = std::array<double, 2>{{(left + right) * 0.5, (top + bottom) * 0.5}};
                        }
                        if (source.source.componentRole ==
                                training::CompoundComponentRole::
                                    OuterHaloEdge) {
                            const std::string suffix = "_halo";
                            if (source.source.primitiveId.size() >=
                                    suffix.size() &&
                                    source.source.primitiveId.compare(
                                        source.source.primitiveId.size() -
                                            suffix.size(),
                                        suffix.size(), suffix) == 0) {
                                source.source.primitiveId.replace(
                                    source.source.primitiveId.size() -
                                        suffix.size(),
                                    suffix.size(), "_core");
                                source.source.componentRole =
                                    training::CompoundComponentRole::Core;
                            }
                        }
                        if (source.source.componentRole ==
                                    training::CompoundComponentRole::Endpoint &&
                                source.source.track ==
                                    training::CompoundVisibilityTrack::
                                        Predicted) {
                            source.source.xPhysicalPx = static_cast<int>(
                                std::lround(source.source.xPhysicalPx -
                                    trajectory.style.
                                        endpointOuterDiameterPx * 0.5));
                        }
                    }
                    const training::FinalTrajectoryGeometryAudit
                        ownershipGeometry =
                            training::auditIntegratedFinalTrajectoryGeometry(
                                ownershipRequest.frame, geometrySources,
                                trajectory.style);

                    const auto frozenStateDigest = [&](const auto& result) {
                        json::Value frozen = json::Value::object();
                        frozen["game_state_sha256"] = json::Value(
                            gameStateSha256(presentationRuntime.state()));
                        frozen["rules_authority_state_hash"] = json::Value(
                            presentationRuntime.rulesAuthorityStateHash());
                        frozen["runtime_tick"] = json::Value(
                            std::to_string(presentationRuntime.tick()));
                        frozen["product_ui_canonical_sha256"] = json::Value(
                            result.canonicalSha256);
                        frozen["screen"] = json::Value(static_cast<int>(
                            sessionSnapshot.shell.screen));
                        frozen["screen_id"] = json::Value(
                            sessionSnapshot.shell.screenId);
                        json::Value camera = json::Value::object();
                        camera["profile_id"] = json::Value(productCameraProfileId);
                        camera["eye_x"] = json::Value(presentation.camera.eye.x);
                        camera["eye_y"] = json::Value(presentation.camera.eye.y);
                        camera["eye_z"] = json::Value(presentation.camera.eye.z);
                        camera["target_x"] = json::Value(
                            presentation.camera.target.x);
                        camera["target_y"] = json::Value(
                            presentation.camera.target.y);
                        camera["target_z"] = json::Value(
                            presentation.camera.target.z);
                        camera["pan_x"] = json::Value(
                            presentation.camera.projectionPan.x);
                        camera["pan_y"] = json::Value(
                            presentation.camera.projectionPan.y);
                        camera["pan_z"] = json::Value(
                            presentation.camera.projectionPan.z);
                        camera["zoom"] = json::Value(presentation.camera.zoom);
                        camera["fov"] = json::Value(
                            presentation.camera.fieldOfViewDegrees);
                        camera["semantic_state"] = json::Value(
                            static_cast<int>(presentation.camera.semanticState));
                        camera["preset"] = json::Value(
                            static_cast<int>(presentation.camera.preset));
                        camera["transitioning"] = json::Value(
                            presentation.camera.transitioning);
                        camera["width"] = json::Value(
                            presentation.camera.viewportWidth);
                        camera["height"] = json::Value(
                            presentation.camera.viewportHeight);
                        frozen["camera"] = std::move(camera);
                        frozen["shot_yaw"] = json::Value(
                            presentation.shotControl.yawRadians);
                        frozen["shot_power"] = json::Value(
                            presentation.shotControl.requestedPower);
                        frozen["tip_x"] = json::Value(
                            presentation.shotControl.tipOffsetX);
                        frozen["tip_y"] = json::Value(
                            presentation.shotControl.tipOffsetY);
                        return sha256Hex(json::stringify(frozen));
                    };
                    const std::string frozenStateSha256 =
                        frozenStateDigest(uiResult);
                    std::string glyphEvidenceError;
                    if (!renderer.setProductUiGlyphEvidenceSuppressed(
                            true, glyphEvidenceError))
                        throw std::runtime_error(
                            "visible_pixel_matrix_glyph_suppression_failed:" +
                            glyphEvidenceError);
                    ModernProductUiRuntimeResult suppressedUiResult;
                    ModernProductUiRuntimeResult restoredUiResult;
                    std::vector<unsigned char> suppressedBackBuffer;
                    std::vector<unsigned char> restoredBackBuffer;
                    const auto suppressedCapturePath =
                        std::filesystem::path(captureDirectory) /
                        (std::string(kStateNames[static_cast<std::size_t>(
                            visiblePixelMatrixStage)]) + "-no-glyphs.ppm");
                    const auto restoredCapturePath =
                        std::filesystem::path(captureDirectory) /
                        (std::string(kStateNames[static_cast<std::size_t>(
                            visiblePixelMatrixStage)]) + "-restored.ppm");
                    try {
                        renderer.renderProductFrameEvidenceReplay(presentation,
                            sessionSnapshot, {true, false, false});
                        if (!renderer.metrics().productPresentPerformed)
                            throw std::runtime_error(
                                "visible_pixel_matrix_glyph_pair_present_blocked:" +
                                renderer.metrics().
                                    trajectoryInsetVisibleErrorCode);
                        suppressedUiResult = renderer.latestProductUiResult();
                        if (!suppressedUiResult.ok ||
                            !suppressedUiResult.glyphEvidenceSuppressed ||
                            suppressedUiResult.canonicalSha256 !=
                                uiResult.canonicalSha256 ||
                            suppressedUiResult.fontAtlasCacheKey !=
                                uiResult.fontAtlasCacheKey ||
                            suppressedUiResult.glyphAtlasRecordSha256 !=
                                uiResult.glyphAtlasRecordSha256 ||
                            suppressedUiResult.gpuMetrics.
                                suppressedGlyphPrimitives !=
                                uiResult.glyphQuads.size() ||
                            suppressedUiResult.gpuMetrics.
                                suppressedIconLabels != 0)
                            throw std::runtime_error(
                                "visible_pixel_matrix_glyph_pair_mismatch");
                        verifyVisibleDefaultFramebuffer(framebufferWidth,
                            framebufferHeight,
                            suppressedCapturePath.string(),
                            &suppressedBackBuffer);
                        if (!renderer.setProductUiGlyphEvidenceSuppressed(
                                false, glyphEvidenceError))
                            throw std::runtime_error(
                                "visible_pixel_matrix_glyph_restore_failed:" +
                                glyphEvidenceError);
                        renderer.renderProductFrameEvidenceReplay(presentation,
                            sessionSnapshot, {true, false, false});
                        if (!renderer.metrics().productPresentPerformed)
                            throw std::runtime_error(
                                "visible_pixel_matrix_glyph_restore_present_blocked:" +
                                renderer.metrics().
                                    trajectoryInsetVisibleErrorCode);
                        restoredUiResult = renderer.latestProductUiResult();
                        verifyVisibleDefaultFramebuffer(framebufferWidth,
                            framebufferHeight, restoredCapturePath.string(),
                            &restoredBackBuffer);
                    } catch (...) {
                        std::string ignored;
                        (void)renderer.setProductUiGlyphEvidenceSuppressed(
                            false, ignored);
                        throw;
                    }
                    const bool restoredCanonicalMatches =
                        restoredUiResult.canonicalSha256 ==
                            uiResult.canonicalSha256;
                    const bool restoredAtlasMatches =
                        restoredUiResult.fontAtlasCacheKey ==
                            uiResult.fontAtlasCacheKey &&
                        restoredUiResult.glyphAtlasRecordSha256 ==
                            uiResult.glyphAtlasRecordSha256;
                    const bool restoredPixelsMatch =
                        restoredBackBuffer == normalBackBuffer;
                    std::size_t restoredPixelByteDifferenceCount = 0;
                    std::size_t firstRestoredPixelByteDifference =
                        normalBackBuffer.size();
                    for (std::size_t byte = 0;
                            byte < std::min(normalBackBuffer.size(),
                                restoredBackBuffer.size()); ++byte) {
                        if (normalBackBuffer[byte] ==
                                restoredBackBuffer[byte])
                            continue;
                        if (firstRestoredPixelByteDifference ==
                                normalBackBuffer.size())
                            firstRestoredPixelByteDifference = byte;
                        ++restoredPixelByteDifferenceCount;
                    }
                    const bool suppressionChangedPixels =
                        suppressedBackBuffer != normalBackBuffer;
                    const bool frozenDigestMatches =
                        frozenStateDigest(restoredUiResult) ==
                            frozenStateSha256;
                    if (!restoredUiResult.ok ||
                        restoredUiResult.glyphEvidenceSuppressed ||
                        !restoredCanonicalMatches || !restoredAtlasMatches ||
                        !restoredPixelsMatch || !suppressionChangedPixels ||
                        !frozenDigestMatches)
                        throw std::runtime_error(
                            "visible_pixel_matrix_frozen_pair_drift:" +
                            std::string("result=") +
                            (restoredUiResult.ok ? "1" : "0") +
                            ":suppression=" +
                            (restoredUiResult.glyphEvidenceSuppressed ?
                                "1" : "0") +
                            ":canonical=" +
                            (restoredCanonicalMatches ? "1" : "0") +
                            ":atlas=" +
                            (restoredAtlasMatches ? "1" : "0") +
                            ":pixels=" +
                            (restoredPixelsMatch ? "1" : "0") +
                            ":pixel_diff_bytes=" +
                            std::to_string(
                                restoredPixelByteDifferenceCount) +
                            ":pixel_first_diff=" +
                            std::to_string(
                                firstRestoredPixelByteDifference) +
                            ":changed=" +
                            (suppressionChangedPixels ? "1" : "0") +
                            ":frozen=" +
                            (frozenDigestMatches ? "1" : "0"));

                    json::Value row = json::Value::object();
                    row["schema"] = json::Value(
                        "billiardgl_native_product_pixel_matrix_v1");
                    row["profile"] = json::Value(visiblePixelMatrixProfile);
                    row["state"] = json::Value(kStateNames[
                        static_cast<std::size_t>(visiblePixelMatrixStage)]);
                    row["origin"] = json::Value(kOrigins[
                        static_cast<std::size_t>(visiblePixelMatrixStage)]);
                    row["screen_id"] = json::Value(
                        sessionSnapshot.shell.screenId);
                    row["width_px"] = json::Value(framebufferWidth);
                    row["height_px"] = json::Value(framebufferHeight);
                    row["backing_scale"] = json::Value(backingScale);
                    row["ui_scale"] = json::Value(
                        sessionSnapshot.effectiveSettings.uiScale);
                    row["color_vision_mode"] = json::Value(static_cast<int>(
                        sessionSnapshot.effectiveSettings.colorVisionMode));
                    row["product_ui_overlay"] = json::Value(
                        static_cast<int>(uiResult.overlay));
                    row["product_ui_draw_calls"] = json::Value(static_cast<int>(
                        uiResult.gpuMetrics.drawCalls));
                    row["product_ui_sdf_vertices"] = json::Value(
                        static_cast<double>(uiResult.gpuMetrics.sdfVertices));
                    row["product_ui_clipped_items"] = json::Value(
                        static_cast<double>(uiResult.gpuMetrics.clippedItems));
                    row["product_ui_canonical_sha256"] = json::Value(
                        uiResult.canonicalSha256);
                    row["paired_frozen_state_sha256"] = json::Value(
                        frozenStateSha256);
                    row["paired_frozen_state_before_sha256"] = json::Value(
                        frozenStateSha256);
                    row["paired_frozen_state_after_sha256"] = json::Value(
                        frozenStateDigest(restoredUiResult));
                    row["paired_runtime_tick"] = json::Value(
                        std::to_string(presentationRuntime.tick()));
                    row["paired_normal_rgba_sha256"] = json::Value(sha256Hex(
                        std::string(reinterpret_cast<const char*>(
                            normalBackBuffer.data()),
                            normalBackBuffer.size())));
                    row["paired_no_glyph_rgba_sha256"] = json::Value(
                        sha256Hex(std::string(reinterpret_cast<const char*>(
                            suppressedBackBuffer.data()),
                            suppressedBackBuffer.size())));
                    row["paired_restored_rgba_sha256"] = json::Value(
                        sha256Hex(std::string(reinterpret_cast<const char*>(
                            restoredBackBuffer.data()),
                            restoredBackBuffer.size())));
                    row["paired_normal_ppm_sha256"] = json::Value(
                        visibleCaptureFileSha256(capturePath.string()));
                    row["paired_no_glyph_ppm_sha256"] = json::Value(
                        visibleCaptureFileSha256(
                            suppressedCapturePath.string()));
                    row["paired_no_glyph_image"] = json::Value(
                        suppressedCapturePath.filename().string());
                    row["paired_no_glyph_canonical_sha256"] = json::Value(
                        suppressedUiResult.canonicalSha256);
                    row["paired_restored_canonical_sha256"] = json::Value(
                        restoredUiResult.canonicalSha256);
                    row["paired_no_glyph_suppressed_count"] = json::Value(
                        static_cast<double>(suppressedUiResult.gpuMetrics.
                            suppressedGlyphPrimitives));
                    row["paired_no_glyph_suppressed_icon_labels"] =
                        json::Value(static_cast<double>(
                            suppressedUiResult.gpuMetrics.
                                suppressedIconLabels));
                    row["paired_restored_user_visible"] = json::Value(true);
                    row["glyph_atlas_binding_verified"] = json::Value(
                        uiResult.glyphAtlasBindingVerified);
                    row["font_atlas_cache_key"] = json::Value(
                        uiResult.fontAtlasCacheKey);
                    row["final_ownership_capture_id"] = json::Value(
                        ownershipRequest.frame.captureId);
                    row["final_ownership_color_sha256"] = json::Value(
                        ownershipRequest.frame.colorContentSha256);
                    row["final_ownership_semantic_sha256"] = json::Value(
                        ownershipRequest.frame.semanticContentSha256);
                    row["final_ownership_msaa_samples"] = json::Value(
                        ownershipRequest.frame.msaaSamples);
                    row["final_ownership_ppm_color_bound"] =
                        json::Value(true);
                    row["final_ownership_resolver_ok"] = json::Value(
                        ownershipResult.ok);
                    row["final_ownership_resolver_error"] = json::Value(
                        ownershipResult.errorCode);
                    json::Value compound = json::Value::object();
                    compound["source"] = json::Value(static_cast<double>(ownershipResult.audit.sourceProbeCount));
                    compound["visible"] = json::Value(static_cast<double>(ownershipResult.audit.visibleProbeCount));
                    compound["coverage"] = json::Value(ownershipResult.audit.visibleProbeCoverage);
                    compound["predicted_source"] = json::Value(static_cast<double>(ownershipResult.audit.predicted.source));
                    compound["predicted_visible"] = json::Value(static_cast<double>(ownershipResult.audit.predicted.visible));
                    compound["predicted_coverage"] = json::Value(ownershipResult.audit.predicted.coverage);
                    compound["actual_source"] = json::Value(static_cast<double>(ownershipResult.audit.actual.source));
                    compound["actual_visible"] = json::Value(static_cast<double>(ownershipResult.audit.actual.visible));
                    compound["actual_coverage"] = json::Value(ownershipResult.audit.actual.coverage);
                    compound["design_target_contrast_coverage"] = json::Value(ownershipResult.audit.designTargetContrastCoverage);
                    json::Value hiddenReasons = json::Value::object();
                    for (const auto& entry : ownershipResult.audit.hiddenReasonCounts)
                        hiddenReasons[entry.first] = json::Value(static_cast<double>(entry.second));
                    compound["hidden_reasons"] = std::move(hiddenReasons);
                    json::Value hiddenProbes = json::Value::array();
                    for (const auto& hidden : ownershipResult.audit.hiddenProbes) {
                        json::Value item = json::Value::object();
                        item["id"] = json::Value(hidden.sourceProbeId);
                        item["track"] = json::Value(training::compoundTrackName(hidden.track));
                        item["reason"] = json::Value(training::compoundHiddenReasonName(hidden.reason));
                        item["x"] = json::Value(hidden.xPhysicalPx);
                        item["y"] = json::Value(hidden.yPhysicalPx);
                        hiddenProbes.asArray().push_back(std::move(item));
                    }
                    compound["hidden_probes"] = std::move(hiddenProbes);
                    row["compound_visibility_audit"] = std::move(compound);
                    row["final_geometry_ok"] = json::Value(
                        ownershipGeometry.ok);
                    row["final_geometry_error"] = json::Value(
                        ownershipGeometry.errorCode);
                    row["final_geometry_partial_ui_overlap_pixels"] =
                        json::Value(static_cast<double>(ownershipGeometry.
                            partialUiOverlapPixelCount));
                    json::Value overlapDiagnostics = json::Value::array();
                    std::map<std::string, std::size_t> overlapTokenCounts;
                    std::size_t overlapUnmatched = 0;
                    std::size_t diagnosticCount = 0;
                    for (const auto& pixel : ownershipRequest.frame.pixels) {
                        if (!(pixel.laterUiCoverage > 0.0 && pixel.laterUiCoverage < 1.0) ||
                            std::none_of(pixel.sampleOwnerTokens.begin(), pixel.sampleOwnerTokens.end(),
                                [](std::uint32_t token) { return token != 0; })) continue;
                        ++diagnosticCount;
                        const float x = pixel.xPhysicalPx + 0.5f;
                        const float y = pixel.yPhysicalPx + 0.5f;
                        const auto insideRect = [&](const product_hud::HudRect& rect) {
                            return x >= rect.x && y >= rect.y &&
                                x < rect.x + rect.width && y < rect.y + rect.height;
                        };
                        json::Value diagnostic = json::Value::object();
                        diagnostic["x"] = json::Value(pixel.xPhysicalPx);
                        diagnostic["y"] = json::Value(pixel.yPhysicalPx);
                        diagnostic["coverage"] = json::Value(pixel.laterUiCoverage);
                        json::Value contributors = json::Value::array();
                        std::set<std::string> pixelTokens;
                        std::size_t order = 0;
                        const auto appendContributor = [&](const std::string& token, float alpha,
                            const char* kind, const product_hud::HudRect& rect) {
                            json::Value item = json::Value::object();
                            item["token"] = json::Value(token);
                            item["alpha"] = json::Value(static_cast<double>(alpha));
                            item["kind"] = json::Value(kind);
                            item["draw_order"] = json::Value(static_cast<double>(order));
                            item["rect_x"] = json::Value(static_cast<double>(rect.x));
                            item["rect_y"] = json::Value(static_cast<double>(rect.y));
                            item["rect_width"] = json::Value(static_cast<double>(rect.width));
                            item["rect_height"] = json::Value(static_cast<double>(rect.height));
                            contributors.asArray().push_back(std::move(item));
                            pixelTokens.insert(token);
                        };
                        for (const auto& primitive : uiResult.diagnosticPrimitives) {
                            if (insideRect(primitive.rect) && insideRect(primitive.clipRect))
                                appendContributor(primitive.token, primitive.color.alpha,
                                    "primitive", primitive.rect);
                            ++order;
                        }
                        for (const auto& triangle : uiResult.diagnosticTriangles) {
                            const auto cross = [&](const ui::UiPoint& a, const ui::UiPoint& b) {
                                return (b.x-a.x)*(y-a.y)-(b.y-a.y)*(x-a.x);
                            };
                            const float a = cross(triangle.first, triangle.second);
                            const float b = cross(triangle.second, triangle.third);
                            const float c = cross(triangle.third, triangle.first);
                            const bool interior = (a >= 0 && b >= 0 && c >= 0) ||
                                (a <= 0 && b <= 0 && c <= 0);
                            if (interior && insideRect(triangle.clipRect))
                                appendContributor(triangle.sourceToken, triangle.color.alpha,
                                    "triangle", triangle.clipRect);
                            ++order;
                        }
                        diagnostic["contributors"] = std::move(contributors);
                        if (diagnosticCount <= 20)
                            overlapDiagnostics.asArray().push_back(std::move(diagnostic));
                        for (const auto& token : pixelTokens) ++overlapTokenCounts[token];
                        if (pixelTokens.empty()) ++overlapUnmatched;
                    }
                    row["diagnostic_partial_ui_overlap"] = std::move(overlapDiagnostics);
                    json::Value overlapCounts = json::Value::object();
                    for (const auto& entry : overlapTokenCounts)
                        overlapCounts[entry.first] = json::Value(static_cast<double>(entry.second));
                    row["diagnostic_overlap_token_pixel_counts"] = std::move(overlapCounts);
                    row["diagnostic_overlap_unmatched"] = json::Value(static_cast<double>(overlapUnmatched));
                    row["final_geometry_fully_ui_covered_pixels"] =
                        json::Value(static_cast<double>(ownershipGeometry.
                            fullyUiCoveredTrajectoryPixelCount));
                    json::Value geometryProbes = json::Value::array();
                    for (const auto& geometry : ownershipGeometry.probes) {
                        json::Value item = json::Value::object();
                        item["source_probe_id"] = json::Value(
                            geometry.sourceProbeId);
                        item["component_role"] = json::Value(
                            static_cast<int>(geometry.componentRole));
                        item["measured_normal_width_px"] = json::Value(
                            geometry.measuredNormalWidthPx);
                        item["expected_normal_width_px"] = json::Value(
                            geometry.expectedNormalWidthPx);
                        item["measured_tangent_span_px"] = json::Value(
                            geometry.measuredTangentSpanPx);
                        item["expected_tangent_span_px"] = json::Value(
                            geometry.expectedTangentSpanPx);
                        item["measured_guaranteed_area_px2"] = json::Value(
                            geometry.measuredGuaranteedAreaPx2);
                        item["center_owner_track"] = json::Value(
                            geometry.centerOwnerTrack);
                        item["center_owner_component"] = json::Value(
                            geometry.centerOwnerComponent);
                        item["center_owner_primitive_id"] = json::Value(
                            geometry.centerOwnerPrimitiveId);
                        item["required"] = json::Value(geometry.required);
                        item["guaranteed_connected"] = json::Value(
                            geometry.guaranteedConnected);
                        item["normal_width_passed"] = json::Value(
                            geometry.normalWidthPassed);
                        item["tangent_span_passed"] = json::Value(
                            geometry.tangentSpanPassed);
                        item["shape_passed"] = json::Value(
                            geometry.shapePassed);
                        item["passed"] = json::Value(geometry.passed);
                        geometryProbes.asArray().push_back(std::move(item));
                    }
                    row["final_geometry_probes"] =
                        std::move(geometryProbes);
                    json::Value tickSamples = json::Value::array();
                    for (const auto& source : geometrySources) {
                        if (source.source.componentRole != training::CompoundComponentRole::Tick)
                            continue;
                        json::Value tick = json::Value::object();
                        tick["primitive"] = json::Value(source.source.primitiveId);
                        tick["center_x"] = json::Value(source.source.xPhysicalPx);
                        tick["center_y"] = json::Value(source.source.yPhysicalPx);
                        tick["tangent_x"] = json::Value(source.tangentX);
                        tick["tangent_y"] = json::Value(source.tangentY);
                        tick["normal_x"] = json::Value(source.normalX);
                        tick["normal_y"] = json::Value(source.normalY);
                        tick["tick_length"] = json::Value(trajectory.style.actualTickLengthPx);
                        if (source.geometryCenter) {
                            tick["authored_center_x"] = json::Value((*source.geometryCenter)[0]);
                            tick["authored_center_y"] = json::Value((*source.geometryCenter)[1]);
                        }
                        json::Value samples = json::Value::array();
                        const double bound = trajectory.style.actualTickLengthPx * 0.5 +
                            trajectory.style.haloWidthPx + trajectory.style.outerContrastRimWidthPx + 2.0;
                        for (const auto& pixel : ownershipRequest.frame.pixels) {
                            if (std::fabs(pixel.xPhysicalPx - source.source.xPhysicalPx) > bound ||
                                std::fabs(pixel.yPhysicalPx - source.source.yPhysicalPx) > bound) continue;
                            for (std::size_t index = 0; index < pixel.sampleOwnerTokens.size(); ++index) {
                                const auto token = pixel.sampleOwnerTokens[index];
                                if (token == 0) continue;
                                const auto& owner = ownershipRequest.frame.ownerTable.at(token);
                                const auto& offset = ownershipRequest.frame.msaaSamplePositions[index];
                                json::Value sample = json::Value::object();
                                sample["x"] = json::Value(pixel.xPhysicalPx + static_cast<double>(offset[0]));
                                sample["y"] = json::Value(pixel.yPhysicalPx + 1.0 - static_cast<double>(offset[1]));
                                sample["sample_index"] = json::Value(static_cast<double>(index));
                                sample["owner"] = json::Value(owner.primitiveId);
                                sample["layer"] = json::Value(owner.visualSublayer);
                                sample["total_overlay_coverage"] = json::Value(pixel.laterUiCoverage);
                                sample["chrome_coverage"] = json::Value(pixel.interactiveChromeCoverage.value_or(pixel.laterUiCoverage));
                                samples.asArray().push_back(std::move(sample));
                            }
                        }
                        tick["samples"] = std::move(samples);
                        tickSamples.asArray().push_back(std::move(tick));
                    }
                    row["diagnostic_tick_owner_samples"] = std::move(tickSamples);
                    json::Value ownershipCandidates = json::Value::array();
                    for (const auto& candidate :
                            ownershipResult.candidateTelemetry) {
                        json::Value item = json::Value::object();
                        item["source_probe_id"] = json::Value(
                            candidate.sourceProbeId);
                        item["x"] = json::Value(candidate.xPhysicalPx);
                        item["y"] = json::Value(candidate.yPhysicalPx);
                        item["resolved_sample_count"] = json::Value(
                            static_cast<double>(candidate.resolvedSampleCount));
                        item["resolved_pixel_area"] = json::Value(
                            candidate.resolvedPixelEquivalentArea);
                        item["owner_track"] = json::Value(
                            candidate.ownerTrack);
                        item["owner_component"] = json::Value(
                            candidate.ownerComponent);
                        item["owner_primitive_id"] = json::Value(
                            candidate.ownerPrimitiveId);
                        item["owner_visual_sublayer"] = json::Value(
                            candidate.ownerVisualSublayer);
                        item["edge_contrast_ratio"] = json::Value(
                            candidate.edgeContrastRatio);
                        item["reason"] = json::Value(
                            candidate.eligibilityOrRejectionReason);
                        ownershipCandidates.asArray().push_back(
                            std::move(item));
                    }
                    row["final_ownership_candidates"] =
                        std::move(ownershipCandidates);
                    json::Value selectedBoundaries = json::Value::array();
                    for (const auto& boundary :
                            ownershipResult.boundaryTelemetry) {
                        if (!boundary.selected) continue;
                        json::Value item = json::Value::object();
                        item["source_probe_id"] = json::Value(
                            boundary.sourceProbeId);
                        item["owner_x"] = json::Value(
                            boundary.ownerXPhysicalPx);
                        item["owner_y"] = json::Value(
                            boundary.ownerYPhysicalPx);
                        item["adjacent_x"] = json::Value(
                            boundary.adjacentXPhysicalPx);
                        item["adjacent_y"] = json::Value(
                            boundary.adjacentYPhysicalPx);
                        item["owner_track"] = json::Value(
                            boundary.ownerTrack);
                        item["owner_component"] = json::Value(
                            boundary.ownerComponent);
                        item["owner_primitive_id"] = json::Value(
                            boundary.ownerPrimitiveId);
                        item["adjacent_track"] = json::Value(
                            boundary.adjacentTrack);
                        item["adjacent_component"] = json::Value(
                            boundary.adjacentComponent);
                        item["adjacent_ui_owned"] = json::Value(
                            boundary.adjacentLaterUiOwned);
                        item["edge_contrast_ratio"] = json::Value(
                            boundary.edgeContrastRatio);
                        item["boundary_class"] = json::Value(
                            boundary.boundaryClass);
                        selectedBoundaries.asArray().push_back(
                            std::move(item));
                    }
                    row["final_ownership_selected_boundaries"] =
                        std::move(selectedBoundaries);
                    json::Value textRuns = json::Value::array();
                    for (const auto& run : uiResult.textRuns) {
                        json::Value item = json::Value::object();
                        item["text"] = json::Value(run.text);
                        item["requested_px"] = json::Value(
                            run.requestedPixelSize);
                        item["measured_width_px"] = json::Value(
                            run.measuredWidth);
                        item["origin_x"] = json::Value(run.originX);
                        item["baseline_y"] = json::Value(run.baselineY);
                        item["clip_x"] = json::Value(run.clipRect.x);
                        item["clip_y"] = json::Value(run.clipRect.y);
                        item["clip_width"] = json::Value(run.clipRect.width);
                        item["clip_height"] = json::Value(run.clipRect.height);
                        textRuns.asArray().push_back(std::move(item));
                    }
                    row["product_ui_text_runs"] = std::move(textRuns);
                    json::Value glyphQuads = json::Value::array();
                    for (std::size_t glyphIndex = 0;
                            glyphIndex < uiResult.glyphQuads.size();
                            ++glyphIndex) {
                        const auto& glyph = uiResult.glyphQuads[glyphIndex];
                        json::Value item = json::Value::object();
                        item["codepoint"] = json::Value(
                            static_cast<double>(glyph.codepoint));
                        item["text_run_index"] = json::Value(
                            static_cast<double>(glyph.textRunIndex));
                        item["codepoint_ordinal"] = json::Value(
                            static_cast<double>(glyph.codepointOrdinal));
                        item["x"] = json::Value(glyph.rect.x);
                        item["y"] = json::Value(glyph.rect.y);
                        item["width"] = json::Value(glyph.rect.width);
                        item["height"] = json::Value(glyph.rect.height);
                        item["source_x"] = json::Value(glyph.sourceRect.x);
                        item["source_y"] = json::Value(glyph.sourceRect.y);
                        item["source_width"] = json::Value(
                            glyph.sourceRect.width);
                        item["source_height"] = json::Value(
                            glyph.sourceRect.height);
                        item["ink_x"] = json::Value(glyph.glyphInkRect.x);
                        item["ink_y"] = json::Value(glyph.glyphInkRect.y);
                        item["ink_width"] = json::Value(
                            glyph.glyphInkRect.width);
                        item["ink_height"] = json::Value(
                            glyph.glyphInkRect.height);
                        item["ink_source_x"] = json::Value(
                            glyph.glyphInkSourceRect.x);
                        item["ink_source_y"] = json::Value(
                            glyph.glyphInkSourceRect.y);
                        item["ink_source_width"] = json::Value(
                            glyph.glyphInkSourceRect.width);
                        item["ink_source_height"] = json::Value(
                            glyph.glyphInkSourceRect.height);
                        item["clip_x"] = json::Value(glyph.clipRect.x);
                        item["clip_y"] = json::Value(glyph.clipRect.y);
                        item["clip_width"] = json::Value(
                            glyph.clipRect.width);
                        item["clip_height"] = json::Value(
                            glyph.clipRect.height);
                        item["u0"] = json::Value(glyph.u0);
                        item["v0"] = json::Value(glyph.v0);
                        item["u1"] = json::Value(glyph.u1);
                        item["v1"] = json::Value(glyph.v1);
                        item["atlas_record_sha256"] = json::Value(
                            uiResult.glyphAtlasRecordSha256[glyphIndex]);
                        glyphQuads.asArray().push_back(std::move(item));
                    }
                    row["product_ui_glyph_quads"] = std::move(glyphQuads);
                    row["trajectory_package_ok"] = json::Value(trajectory.ok);
                    row["trajectory_error"] = json::Value(trajectory.errorCode);
                    row["predicted_vertices"] = json::Value(
                        static_cast<double>(trajectory.predicted.vertexCount));
                    row["predicted_strokes"] = json::Value(
                        static_cast<double>(trajectory.predicted.visibleStrokeCount));
                    row["predicted_line_width_px"] = json::Value(
                        trajectory.style.lineWidthPx);
                    row["actual_vertices"] = json::Value(
                        static_cast<double>(trajectory.actual.vertexCount));
                    row["actual_strokes"] = json::Value(
                        static_cast<double>(trajectory.actual.visibleStrokeCount));
                    row["actual_tick_count"] = json::Value(
                        static_cast<double>(trajectory.actual.tickMarkCount));
                    json::Value probes = json::Value::array();
                    for (const auto& probe : trajectory.contrastProbes) {
                        json::Value item = json::Value::object();
                        item["track"] = json::Value(
                            probe.track == training::TrajectoryContrastTrack::Predicted ?
                                "predicted" : "actual");
                        item["x"] = json::Value(probe.strokeXPhysicalPx);
                        item["y"] = json::Value(probe.strokeYPhysicalPx);
                        item["background_x"] = json::Value(
                            probe.backgroundXPhysicalPx);
                        item["background_y"] = json::Value(
                            probe.backgroundYPhysicalPx);
                        item["halo_x"] = json::Value(probe.haloXPhysicalPx);
                        item["halo_y"] = json::Value(probe.haloYPhysicalPx);
                        item["halo_valid"] = json::Value(
                            probe.componentRole ==
                                training::CompoundComponentRole::Core ||
                            probe.componentRole ==
                                training::CompoundComponentRole::Tick);
                        item["primitive_id"] = json::Value(probe.primitiveId);
                        item["arc_segment_id"] = json::Value(
                            probe.arcSegmentId);
                        item["component_role"] = json::Value(
                            static_cast<int>(probe.componentRole));
                        item["probe_ordinal"] = json::Value(
                            static_cast<double>(probe.probeOrdinal));
                        item["tangent_x"] = json::Value(probe.localTangentX);
                        item["tangent_y"] = json::Value(probe.localTangentY);
                        item["normal_x"] = json::Value(probe.localNormalX);
                        item["normal_y"] = json::Value(probe.localNormalY);
                        probes.asArray().push_back(std::move(item));
                    }
                    row["trajectory_probes"] = std::move(probes);
                    *visiblePixelMatrixReceipt << json::stringify(row) << '\n';
                    visiblePixelMatrixReceipt->flush();
                    if (!*visiblePixelMatrixReceipt)
                        throw std::runtime_error(
                            "visible_pixel_matrix_receipt_write_failed");
                    output << "visible_pixel_matrix profile=" <<
                        visiblePixelMatrixProfile << " state=" <<
                        kStateNames[static_cast<std::size_t>(
                            visiblePixelMatrixStage)] << " origin=" <<
                        kOrigins[static_cast<std::size_t>(
                            visiblePixelMatrixStage)] << '\n';
                    ++visiblePixelMatrixStage;
                } else if (verifyVisibleDrawable) {
                    const char* captureDirectory = std::getenv("BILLIARDGL_VISIBLE_CAPTURE_DIR");
                    const int captureFrame = verifiedVisibleFrames + 1;
                    std::string capturePath;
                    if (captureDirectory != nullptr) {
                        const char* rulesCapture = visibleRules06CStage == 2 ?
                                "rules-06c-offer.ppm" :
                            visibleRules06CStage == 8 ?
                                "rules-06c-armed.ppm" :
                            visibleRules06CStage == 10 ?
                                "rules-06c-committed.ppm" :
                            visibleRules06EStage == 2 ?
                                "rules-06e-parent-offer.ppm" :
                            visibleRules06EStage == 4 ?
                                "rules-06e-parent-armed.ppm" :
                            visibleRules06EStage == 7 ?
                                "rules-06e-bih-preview.ppm" :
                            visibleRules06EStage == 8 ?
                                "rules-06e-bih-armed.ppm" :
                            visibleRules06EStage == 9 ?
                                "rules-06e-committed.ppm" :
                            visibleRules06FStage == 2 ?
                                "rules-06f-parent-offer.ppm" :
                            visibleRules06FStage == 4 ?
                                "rules-06f-parent-armed.ppm" :
                            visibleRules06FStage == 7 ?
                                "rules-06f-bih-preview.ppm" :
                            visibleRules06FStage == 8 ?
                                "rules-06f-bih-armed.ppm" :
                            visibleRules06FStage == 9 ?
                                "rules-06f-committed.ppm" :
                            visibleRulesGroupStage == 2 ?
                                "rules-group-choice-offer.ppm" :
                            visibleRulesGroupStage == 3 ?
                                "rules-group-choice-armed.ppm" :
                            visibleRulesGroupStage == 4 ?
                                "rules-group-choice-committed.ppm" :
                            visibleRulesFullTableStage == 2 ?
                                "rules-full-table-bih-offer.ppm" :
                            visibleRulesFullTableStage == 3 ?
                                "rules-full-table-bih-preview.ppm" :
                            visibleRulesFullTableStage == 4 ?
                                "rules-full-table-bih-armed.ppm" :
                            visibleRulesFullTableStage == 5 ?
                                "rules-full-table-bih-committed.ppm" : nullptr;
                        if (rulesCapture != nullptr) {
                            capturePath = (std::filesystem::path(captureDirectory) /
                                rulesCapture).string();
                        } else if (captureFrame == 1 || captureFrame >= 18) {
                            capturePath = (std::filesystem::path(captureDirectory) /
                                ("training-frame-" + std::to_string(captureFrame) + ".ppm")).string();
                        }
                    }
                    verifyVisibleDefaultFramebuffer(framebufferWidth,
                        framebufferHeight, capturePath);
                    ++verifiedVisibleFrames;
                }
#endif
                // The renderer deliberately restores its offscreen LDR FBO
                // after copying into the Cocoa back buffer.  Select the
                // window drawable explicitly for the AppKit swap.
                if (productSession && productCameraProfileId == "product_camera_m4_candidate_v1")
                    captureRequestedProductFrame(std::getenv("BILLIARDGL_PRODUCT_NATIVE_CAPTURE_DIR"),
                        framebufferWidth,framebufferHeight,presentation,presentationRuntime,options,
                        productCameraProfileId,diagnosticFrame,productSession->snapshot().shell.screenId);
                const auto swapStart = std::chrono::steady_clock::now();
                glBindFramebuffer(GL_DRAW_FRAMEBUFFER, 0);
                glDrawBuffer(GL_BACK);
                [platform.context flushBuffer];
                const auto swapEnd = std::chrono::steady_clock::now();
                const double renderMilliseconds =
                    std::chrono::duration<double, std::milli>(
                        swapStart - renderStart).count();
                const auto presentedTiming = measurePresentedFrame(
                    monotonicMilliseconds(frameLoopStart),
                    monotonicMilliseconds(swapStart),
                    monotonicMilliseconds(swapEnd), previousSwapEndMilliseconds);
                previousSwapEndMilliseconds = monotonicMilliseconds(swapEnd);
                if (playtestLog) {
                    auto details = json::Value::object();
                    details["frame_id"] = json::Value(std::to_string(diagnosticFrame));
                    details["render_ms"] = json::Value(renderMilliseconds);
                    details["swap_ms"] = json::Value(std::chrono::duration<double, std::milli>(
                        swapEnd - swapStart).count());
                    details["timing_scope"] = json::Value("event_loop_through_swap_completion");
                    details["drawable_width_px"] = json::Value(framebufferWidth);
                    details["drawable_height_px"] = json::Value(framebufferHeight);
                    details["render_quality_enum"] = json::Value(static_cast<int>(options.quality));
                    details["requested_msaa_samples"] = json::Value(options.msaaSamples);
                    details["includes_simulation"] = json::Value(true);
                    details["includes_display_swap"] = json::Value(true);
                    details["measures_scanout"] = json::Value(false);
                    details["frame_timing_valid"] = json::Value(presentedTiming.has_value());
                    if (presentedTiming) {
                        details["loop_through_swap_ms"] = json::Value(
                            presentedTiming->loopThroughSwapMilliseconds);
                        if (presentedTiming->swapToSwapMilliseconds)
                            details["swap_to_swap_ms"] = json::Value(
                                *presentedTiming->swapToSwapMilliseconds);
                    }
                    details["log_ms_before_swap_record"] = json::Value(
                        playtestLog->recordMilliseconds() - logStartMs);
                    playtestLog->record("swap_submitted", presentationRuntime, details);
                }
                if (productPerformance) {
                    if (!presentedTiming)
                        throw std::runtime_error(
                            "product_performance_frame_timing_invalid");
                    productPerformance->observe(*presentedTiming,
                        renderer.metrics(), physicsMs, renderMilliseconds,
                        steps, runtime.state().ballsMoving, runtime.tick(),
                        frameSteps);
                    if (productPerformance->presentedFrames ==
                            kProductPerformanceWarmupFrames) {
                        GameAction shoot;
                        shoot.type = ActionType::Shoot;
                        if (productPerformanceShotDispatched ||
                            !runtime.dispatch(shoot).ok ||
                            !runtime.state().ballsMoving ||
                            !runtime.hasCueContactResult() ||
                            !runtime.cueContactResult().applied) {
                            throw std::runtime_error(
                                "product_performance_native_shot_dispatch_failed");
                        }
                        productPerformanceShotDispatched = true;
                        productPerformance->nativeShotDispatchTick =
                            runtime.tick();
                        previous = runtime.state();
                    }
                    if (productPerformance->complete()) {
                        writeProductFramePerformanceReport(
                            options.productFramePerformanceReportPath,
                            *productPerformance, options, playtestLog->path());
                        [platform.window close];
                    }
                }
#if defined(BILLIARDGL_VISIBLE_DRAWABLE_SELF_TEST)
                if (verifyVisiblePixelMatrix) {
                    const auto enterAim = [&]() {
                        if (runtime.state().aim.mode == AimMode::Aim) return;
                        PlatformInputEvent tab;
                        tab.kind = PlatformInputKind::KeyDown;
                        tab.control = "Tab";
                        const auto result = productSession->handlePlatformInput(tab);
                        if (!result.ok || runtime.state().aim.mode != AimMode::Aim)
                            throw std::runtime_error(
                                "visible_pixel_matrix_enter_aim_failed");
                    };
                    const auto startDrawTraining = [&]() {
                        const auto result = productSession->shell().
                            startTrainingScenario(
                                "launch_straight_draw_intro_v2",
                                "billiardgl-product-source-v1", 0);
                        if (!result.ok)
                            throw std::runtime_error(
                                "visible_pixel_matrix_training_setup_failed:" +
                                result.errorCode);
                        enterAim();
                    };
                    if (visiblePixelMatrixStage == 1) {
                        const auto started = productSession->shell().
                            startSinglePlayer(0x504958454c41494dULL);
                        if (!started.ok)
                            throw std::runtime_error(
                                "visible_pixel_matrix_aim_match_failed:" +
                                started.errorCode);
                        enterAim();
                        GameAction power;
                        power.type = ActionType::SetShotPower;
                        power.first = 200.0f;
                        const auto powerResult = runtime.dispatch(power);
                        if (!powerResult.ok)
                            throw std::runtime_error(
                                std::string("visible_pixel_matrix_max_power_failed:") +
                                powerResult.errorCode);
                        const auto tipResult = runtime.setTipOffset(
                            runtime.physicsProfile().cue.
                                maximumReliableOffsetRadius, 0.0f);
                        if (!tipResult.ok)
                            throw std::runtime_error(
                                std::string("visible_pixel_matrix_max_tip_failed:") +
                                tipResult.errorCode);
                    } else if (visiblePixelMatrixStage == 2) {
                        GameAction power;
                        power.type = ActionType::SetShotPower;
                        power.first = 60.0f;
                        GameAction yaw;
                        yaw.type = ActionType::SetAimYaw;
                        yaw.first = -1.57079632679489661923f;
                        if (!runtime.dispatch(power).ok ||
                            !runtime.dispatch(yaw).ok ||
                            !runtime.setTipOffset(0.0f, 0.0f).ok)
                            throw std::runtime_error(
                                "visible_pixel_matrix_post_shot_setup_failed");
                        PlatformInputEvent shoot;
                        shoot.kind = PlatformInputKind::PointerButtonUp;
                        shoot.control = "LeftButton";
                        if (!productSession->handlePlatformInput(shoot).ok)
                            throw std::runtime_error(
                                "visible_pixel_matrix_training_shot_failed");
                        int ticks = 0;
                        while (runtime.state().ballsMoving && ticks < 5000) {
                            const auto stepped = runtime.step(1);
                            if (!stepped.ok)
                                throw std::runtime_error(
                                    std::string("visible_pixel_matrix_training_step_failed:") +
                                    stepped.errorCode);
                            productSession->consumeAuthoritySteps();
                            ++ticks;
                        }
                        if (runtime.state().ballsMoving || ticks >= 5000 ||
                            !runtime.hasCueContactResult() ||
                            !runtime.cueContactResult().applied)
                            throw std::runtime_error(
                                "visible_pixel_matrix_post_shot_terminal_missing");
                        previous = runtime.state();
                        accumulator = 0.0;
                    } else if (visiblePixelMatrixStage == 3) {
                        const auto started = productSession->shell().
                            startSinglePlayer(0x504958454c4d4154ULL);
                        if (!started.ok)
                            throw std::runtime_error(
                                "visible_pixel_matrix_match_setup_failed:" +
                                started.errorCode);
                        // ProductShell currently exposes player-vs-AI setup.
                        // This explicit pixel fixture needs the opponent-owned
                        // IllegalBreak modal, so model the existing local
                        // two-player component-test boundary after the real
                        // rack commit.  This is not a shipping menu path and
                        // does not grant a remote AI actor local authority.
                        ai::PlayerConfiguration localTwoPlayer;
                        localTwoPlayer.enabled = false;
                        const auto localPlayers = aiPlayer.configure(
                            runtime, localTwoPlayer);
                        if (!localPlayers.ok)
                            throw std::runtime_error(
                                std::string("visible_pixel_matrix_local_players_failed:") +
                                localPlayers.errorCode);
                        rules::ShotFacts weak;
                        weak.shooter = runtime.matchState().currentPlayer;
                        weak.firstContactBall = 1;
                        weak.anyRailAfterFirstContact = true;
                        weak.breakShot = true;
                        weak.breakEvidenceOrigin =
                            rules::BreakEvidenceOrigin::
                                AuthoritativeShotFactCollector;
                        weak.breakEvidenceComplete = true;
                        for (int index = 0; index < 3; ++index) {
                            rules::BreakObjectCushionContact contact;
                            contact.objectBallId = index + 1;
                            contact.boundaryKind =
                                rules::BreakCushionBoundaryKind::StraightCushion;
                            contact.provenance =
                                rules::BreakCushionContactProvenance::
                                    DiscretePhysics;
                            contact.velocityImpulseApplied = true;
                            contact.normalImpulseNs = 0.1;
                            contact.physicsTick = 1;
                            contact.canonicalContactOrdinal = index;
                            contact.timeOfImpactSeconds = 0.001 * index;
                            weak.breakObjectCushionContacts.push_back(contact);
                        }
                        weak.distinctObjectBallsToRail = 3;
                        if (!runtime.adjudicateRulesShot(weak).ok)
                            throw std::runtime_error(
                                "visible_pixel_matrix_foul_fixture_failed");
                        productSession->syncRulesDecisionAuthority();
                        if (!runtime.breakFoulDecisionOffer() ||
                            !productSession->snapshot().rulesDecisionProductActive)
                            throw std::runtime_error(
                                "visible_pixel_matrix_foul_modal_missing");
                    } else if (visiblePixelMatrixStage == 4) {
                        if (!productSession->shell().openHelp().ok)
                            throw std::runtime_error(
                                "visible_pixel_matrix_help_failed");
                    } else if (visiblePixelMatrixStage == 5) {
                        startDrawTraining();
                        productSession->shell().setInspectorExpanded(true);
                        if (!productSession->refreshTrainingPreview().ok)
                            throw std::runtime_error(
                                "visible_pixel_matrix_training_preview_failed");
                    } else if (visiblePixelMatrixStage == 6) {
                        const auto watched = productSession->shell().
                            watchTrainingReferenceReplay();
                        if (!watched.ok)
                            throw std::runtime_error(
                                "visible_pixel_matrix_replay_load_failed:" +
                                watched.errorCode);
                        for (int tick = 0; tick < 900; ++tick) {
                            const auto replay =
                                productSession->snapshot().shell.replay;
                            if (replay.cueTrajectory.size() >= 24) break;
                            if (replay.finished) break;
                            if (!productSession->shell().focusItem(
                                    "replay_step_tick").ok ||
                                !productSession->shell().navigate(
                                    ShellNavigationCommand::Activate).ok)
                                throw std::runtime_error(
                                    "visible_pixel_matrix_replay_step_failed");
                        }
                        const auto replay =
                            productSession->snapshot().shell.replay;
                        if (!replay.loaded || replay.cueTrajectory.size() < 2)
                            throw std::runtime_error(
                                "visible_pixel_matrix_replay_trajectory_missing");
                    } else if (visiblePixelMatrixStage == 7) {
                        output << "visible_pixel_matrix_complete profile=" <<
                            visiblePixelMatrixProfile << " states=7\n";
                        [platform.window close];
                    }
                } else if (verifyVisibleDrawable && verifiedVisibleFrames == 1) {
                    // Exercise the exact production native gate, without focus overrides.
                    NSView* view = [platform.window contentView];
                    const NSRect bounds = [view bounds];
                    const NSPoint inside = [view convertPoint:NSMakePoint(NSMidX(bounds), NSMidY(bounds)) toView:nil];
                    const NSPoint chrome = [view convertPoint:NSMakePoint(20,
                        [view isFlipped] ? NSMinY(bounds) - 12 : NSMaxY(bounds) + 12) toView:nil];
                    auto nativeEvent = [&](NSEventType type, NSPoint point) {
                        return [NSEvent mouseEventWithType:type location:point modifierFlags:0
                            timestamp:0 windowNumber:[platform.window windowNumber] context:nil
                            eventNumber:0 clickCount:1 pressure:0];
                    };
                    ProductPointerOwnership testOwnership;
                    if (productPlatformEvent(nativeEvent(NSEventTypeLeftMouseDown, chrome), view,
                            testOwnership, *productSession) ||
                        productPlatformEvent(nativeEvent(NSEventTypeLeftMouseDragged, inside), view,
                            testOwnership, *productSession) ||
                        productPlatformEvent(nativeEvent(NSEventTypeLeftMouseUp, inside), view,
                            testOwnership, *productSession))
                        throw std::runtime_error("native_window_chrome_leaked_into_gameplay");
                    if (!productPlatformEvent(nativeEvent(NSEventTypeLeftMouseDown, inside), view,
                            testOwnership, *productSession) ||
                        !productPlatformEvent(nativeEvent(NSEventTypeLeftMouseDragged, inside), view,
                            testOwnership, *productSession) ||
                        !productPlatformEvent(nativeEvent(NSEventTypeLeftMouseUp, inside), view,
                            testOwnership, *productSession))
                        throw std::runtime_error(std::string("native_content_drag_was_blocked:key=") +
                            ([platform.window isKeyWindow] ? "yes" : "no") +
                            ":window=" + ([nativeEvent(NSEventTypeLeftMouseDown, inside) window] == platform.window ? "same" : "other"));
                    firstVerifiedBackingSize = platform.backingSize();
                    syncInputViewport();
                    const auto menuLayout = productSession->shellLayout();
                    const auto menuButton = std::find_if(menuLayout.elements.begin(), menuLayout.elements.end(),
                        [](const auto& element) { return element.id == "help"; });
                    if (menuButton == menuLayout.elements.end())
                        throw std::runtime_error("native_menu_help_missing");
                    const double menuX = (menuButton->rect.x + menuButton->rect.width * 0.5) *
                        menuLayout.viewport.logicalWidth / menuLayout.viewport.physicalWidth;
                    const double menuY = (menuButton->rect.y + menuButton->rect.height * 0.5) *
                        menuLayout.viewport.logicalHeight / menuLayout.viewport.physicalHeight;
                    const NSPoint menuPoint = [view convertPoint:NSMakePoint(menuX,
                        [view isFlipped] ? menuY : bounds.size.height - menuY) toView:nil];
                    auto menuDown = productPlatformEvent(nativeEvent(NSEventTypeLeftMouseDown, menuPoint),
                        view, testOwnership, *productSession);
                    if (!menuDown || !productSession->handlePlatformInput(*menuDown).handled)
                        throw std::runtime_error("native_menu_down_not_received");
                    NSEvent* bookkeeping = [NSEvent otherEventWithType:NSEventTypeApplicationDefined
                        location:NSZeroPoint modifierFlags:0 timestamp:0 windowNumber:0 context:nil
                        subtype:0 data1:0 data2:0];
                    (void)productPlatformEvent(bookkeeping, view, testOwnership, *productSession);
                    auto menuUp = productPlatformEvent(nativeEvent(NSEventTypeLeftMouseUp, menuPoint),
                        view, testOwnership, *productSession);
                    if (!menuUp || !productSession->handlePlatformInput(*menuUp).ok ||
                        productSession->snapshot().shell.screen != ProductScreen::Help)
                        throw std::runtime_error("native_menu_click_did_not_activate");
                    (void)productSession->shell().navigate(ShellNavigationCommand::Back);
                    const double scale = [platform.window backingScaleFactor];
                    [platform.window setContentSize:NSMakeSize(2048.0 / scale, 1536.0 / scale)];
                    if (!productSession->shell().startTrainingScenario(
                            "launch_stop_shot_center_v1", "layout-recovery-test", 0).ok)
                        throw std::runtime_error("layout_recovery_training_setup_failed");
                    (void)runtime.setCameraPresetInstant(camera::SemanticState::AimPlayer,
                        camera::Preset::PlayerLow);
                    PlatformInputEvent tab;
                    tab.kind = PlatformInputKind::KeyDown;
                    tab.control = "Tab";
                    (void)productSession->handlePlatformInput(tab);
                    GameAction aim;
                    aim.type = ActionType::SetAimYaw;
                    aim.first = -1.587201714515686f;
                    (void)runtime.dispatch(aim);
                    GameAction power;
                    power.type = ActionType::SetShotPower;
                    power.first = 200.0f;
                    (void)runtime.dispatch(power);
                    PlatformInputEvent shoot;
                    shoot.kind = PlatformInputKind::PointerButtonUp;
                    shoot.control = "LeftButton";
                    if (!productSession->handlePlatformInput(shoot).ok)
                        throw std::runtime_error("layout_recovery_shot_setup_failed");
                    for (int step = 0; step < 2400 && runtime.state().ballsMoving; ++step) {
                        if (!runtime.step(1).ok)
                            throw std::runtime_error("layout_recovery_shot_step_failed");
                        productSession->consumeAuthoritySteps();
                    }
                    if (!productSession->refreshTrainingPreview().ok)
                        throw std::runtime_error("layout_recovery_preview_setup_failed");
                    GameAction orbit;
                    orbit.type = ActionType::OrbitCamera;
                    orbit.first = std::atan2(138.250656f - 58.0f, 111.843887f) + 1.5707963f;
                    orbit.second = std::asin((134.902191f - 89.341476f) / 145.0f) -
                        20.0f * 3.14159265f / 180.0f;
                    (void)runtime.dispatch(orbit);
                } else if (verifyVisibleDrawable && verifiedVisibleFrames >= 2) {
                    if (NSEqualSizes(firstVerifiedBackingSize,
                                     platform.backingSize())) {
                        throw std::runtime_error(
                            "visible_drawable_resize_was_not_observed");
                    }
                    if (verifiedVisibleFrames == 2 && lastLayoutDegradation != "shell_inspector_user_collapsed")
                        throw std::runtime_error("recorded_layout_conflict_was_not_reproduced");
                    if (verifiedVisibleFrames == 8) {
                        if (!productSession->shell().focusItem("watch_training_reference").ok ||
                            !productSession->shell().navigate(ShellNavigationCommand::Activate).ok)
                            throw std::runtime_error("visible_replay_reference_load_failed");
                    } else if (verifiedVisibleFrames == 9 || verifiedVisibleFrames == 15) {
                        syncInputViewport();
                        const auto layout = productSession->shellLayout();
                        const auto button = std::find_if(layout.elements.begin(), layout.elements.end(),
                            [](const auto& element) { return element.id == "replay_pause"; });
                        if (button == layout.elements.end())
                            throw std::runtime_error("visible_replay_play_button_missing");
                        PlatformInputEvent click;
                        click.control = "LeftButton";
                        click.logicalX = (button->rect.x + button->rect.width * 0.5) *
                            layout.viewport.logicalWidth / layout.viewport.physicalWidth;
                        click.logicalY = (button->rect.y + button->rect.height * 0.5) *
                            layout.viewport.logicalHeight / layout.viewport.physicalHeight;
                        click.kind = PlatformInputKind::PointerButtonDown;
                        (void)productSession->handlePlatformInput(click);
                        syncInputViewport();
                        click.kind = PlatformInputKind::PointerButtonUp;
                        const auto clicked = productSession->handlePlatformInput(click);
                        if (!clicked.ok || clicked.action != "replay_pause")
                            throw std::runtime_error("visible_replay_mouse_activation_failed");
                        const auto replay = productSession->snapshot().shell.replay;
                        if (verifiedVisibleFrames == 9 && replay.paused)
                            throw std::runtime_error("visible_replay_mouse_play_failed");
                        if (verifiedVisibleFrames == 15) {
                            if (!replay.paused || replay.tick == 0)
                                throw std::runtime_error("visible_replay_mouse_pause_failed");
                            verifiedPausedReplayTick = replay.tick;
                        }
                    } else if (verifiedVisibleFrames == 10 || verifiedVisibleFrames == 16) {
                        syncInputViewport();
                        const auto layout = productSession->shellLayout();
                        const auto before = productSession->presentationCameraFrame().pose;
                        PlatformInputEvent drag;
                        drag.control = "LeftButton";
                        drag.logicalX = layout.viewport.logicalWidth * 0.55;
                        drag.logicalY = layout.viewport.logicalHeight * 0.6;
                        drag.modifiers = PlatformModifierShift;
                        drag.kind = PlatformInputKind::PointerButtonDown;
                        (void)productSession->handlePlatformInput(drag);
                        drag.kind = PlatformInputKind::PointerMove;
                        drag.leftButtonDown = true;
                        drag.deltaX = 20;
                        drag.deltaY = -10;
                        drag.logicalX += 20;
                        drag.logicalY -= 10;
                        if (!productSession->handlePlatformInput(drag).ok ||
                            std::fabs(productSession->presentationCameraFrame().pose.yawRadians - before.yawRadians) < 0.05f)
                            throw std::runtime_error("visible_replay_camera_drag_failed");
                        drag.kind = PlatformInputKind::PointerButtonUp;
                        drag.leftButtonDown = false;
                        (void)productSession->handlePlatformInput(drag);
                        if (verifiedVisibleFrames == 16 &&
                            (!productSession->shell().focusItem("replay_camera_reset").ok ||
                             !productSession->shell().navigate(ShellNavigationCommand::Activate).ok))
                            throw std::runtime_error("visible_replay_camera_reset_failed");
                    } else if (visibleRules06CStage > 0 ||
                               visibleRules06EStage > 0 ||
                               visibleRules06FStage > 0 ||
                               visibleRulesGroupStage > 0 ||
                               visibleRulesFullTableStage > 0) {
                        NSView* view = [platform.window contentView];
                        const NSRect bounds = [view bounds];
                        auto nativeMouseEvent = [&](NSEventType type, NSPoint point) {
                            return [NSEvent mouseEventWithType:type location:point modifierFlags:0
                                timestamp:0 windowNumber:[platform.window windowNumber] context:nil
                                eventNumber:0 clickCount:1 pressure:0];
                        };
                        const auto nativeKeyEvent = [&](unsigned short keyCode,
                                                        NSString* characters) {
                            return [NSEvent keyEventWithType:NSEventTypeKeyDown
                                location:NSZeroPoint modifierFlags:0 timestamp:0
                                windowNumber:[platform.window windowNumber] context:nil
                                characters:characters charactersIgnoringModifiers:characters
                                isARepeat:NO keyCode:keyCode];
                        };
                        const auto dispatchNative = [&](NSEvent* nativeEvent,
                                                        const char* operation) {
                            const auto translated = productPlatformEvent(nativeEvent, view,
                                nativeInput.pointer, *productSession);
                            if (!translated)
                                throw std::runtime_error(std::string(operation) +
                                    "_native_translation_failed");
                            const ModernProductEventResult result =
                                productSession->handlePlatformInput(*translated);
                            if (!result.ok)
                                throw std::runtime_error(std::string(operation) + ':' +
                                    result.errorCode);
                            return result;
                        };
                        const auto rulesControlPoint = [&](const ui::RulesDecisionUiHitRect& hit) {
                            const double scale = [platform.window backingScaleFactor];
                            const double logicalX = (hit.physicalRect.x +
                                hit.physicalRect.width * 0.5) / scale;
                            const double logicalY = (hit.physicalRect.y +
                                hit.physicalRect.height * 0.5) / scale;
                            const NSPoint pointInView = NSMakePoint(logicalX,
                                [view isFlipped] ? logicalY : bounds.size.height - logicalY);
                            return [view convertPoint:pointInView toView:nil];
                        };
                        const auto physicalPoint = [&](double physicalX,
                                                       double physicalY) {
                            const double scale =
                                [platform.window backingScaleFactor];
                            const double logicalX = physicalX / scale;
                            const double logicalY = physicalY / scale;
                            const NSPoint pointInView = NSMakePoint(logicalX,
                                [view isFlipped] ? logicalY :
                                    bounds.size.height - logicalY);
                            return [view convertPoint:pointInView toView:nil];
                        };
                        const auto clickRulesControl = [&](const ui::RulesDecisionUiHitRect& hit,
                                                           const char* operation) {
                            const NSPoint point = rulesControlPoint(hit);
                            (void)dispatchNative(nativeMouseEvent(
                                NSEventTypeLeftMouseDown, point), operation);
                            return dispatchNative(nativeMouseEvent(
                                NSEventTypeLeftMouseUp, point), operation);
                        };
                        const auto requireRenderedProductFrame = [&](ui::ProductUiOverlayKind overlay,
                                                                     const char* operation) {
                            const auto& result = renderer.latestProductUiResult();
                            if (!result.ok || !result.gpuSubmitted ||
                                result.screen != ProductScreen::Match ||
                                result.overlay != overlay || result.canonicalSha256.empty())
                                throw std::runtime_error(std::string(operation) +
                                    "_gpu_evidence_incomplete");
                        };
                        const auto throwConfirmDiagnostic = [&](const char* operation,
                                                                 const auto& hits) {
                            const auto decision = productSession->snapshot().rulesDecision;
                    const auto& uiResult = renderer.latestProductUiResult();
                            std::ostringstream detail;
                            detail << operation
                                << ":stage=" << visibleRules06CStage
                                << ":ui_state=" << static_cast<int>(decision.state)
                                << ":armed=" << decision.armedActionId
                                << ":focused=" << decision.focusedActionId
                                << ":visible_actions=" << decision.visibleActionIds.size()
                                << ":hit_count=" << hits.size()
                                << ":hits=";
                            for (const auto& hit : hits) {
                                detail << '[' << hit.actionId
                                    << ",enabled=" << hit.enabled
                                    << ",confirm=" << hit.confirm << ']';
                            }
                            detail << ":revision=" << runtime.rulesAuthorityRevision()
                                << ":expected_revision=" << visibleRules06CRevision
                                << ":product_active="
                                << productSession->snapshot().rulesDecisionProductActive
                                << ":ui_ok=" << uiResult.ok
                                << ":gpu_submitted=" << uiResult.gpuSubmitted
                                << ":screen=" << static_cast<int>(uiResult.screen)
                                << ":overlay=" << static_cast<int>(uiResult.overlay)
                                << ":key_window=" << [platform.window isKeyWindow]
                                << ":focus_loss_notifications="
                                << nativeInput.focusLossNotificationCount
                                << ":arm_focus_loss_baseline="
                                << visibleRules06CArmFocusLossBaseline
                                << ":arm_stage=" << visibleRules06CArmStage
                                << ":focus_loss_since_arm="
                                << (nativeInput.focusLossNotificationCount -
                                    visibleRules06CArmFocusLossBaseline);
                            throw std::runtime_error(detail.str());
                        };
                        const auto postDeterministicCocoaFocusLoss = [&]() {
                            // Exercise the production Cocoa notification observer exactly.
                            // This deterministic notification is not evidence of a human
                            // Alt-Tab or of a menu operation.
                            [[NSNotificationCenter defaultCenter]
                                postNotificationName:NSWindowDidResignKeyNotification
                                object:platform.window];
                        };

                        if (visibleRules06CStage > 1 &&
                            visibleRules06CStage < 10 &&
                            runtime.rulesAuthorityStateHash() !=
                                visibleRules06CStateHash) {
                            const auto sameAuthorityFloat = [](float left,
                                                               float right) {
                                return left == right &&
                                    (left != 0.0f ||
                                     std::signbit(left) == std::signbit(right));
                            };
                            std::ostringstream detail;
                            detail << "visible_06c_authority_drift:stage="
                                << visibleRules06CStage << ":balls=";
                            for (int ball = 0; ball < kBallCount; ++ball) {
                                const auto& settled =
                                    visibleRules06CGameState.balls[ball];
                                const auto& current = runtime.state().balls[ball];
                                if (sameAuthorityFloat(current.position.x,
                                        settled.position.x) &&
                                    sameAuthorityFloat(current.position.y,
                                        settled.position.y) &&
                                    sameAuthorityFloat(current.position.z,
                                        settled.position.z) &&
                                    sameAuthorityFloat(current.velocity.x,
                                        settled.velocity.x) &&
                                    sameAuthorityFloat(current.velocity.y,
                                        settled.velocity.y) &&
                                    sameAuthorityFloat(current.velocity.z,
                                        settled.velocity.z) &&
                                    sameAuthorityFloat(current.angularVelocity.x,
                                        settled.angularVelocity.x) &&
                                    sameAuthorityFloat(current.angularVelocity.y,
                                        settled.angularVelocity.y) &&
                                    sameAuthorityFloat(current.angularVelocity.z,
                                        settled.angularVelocity.z) &&
                                    sameAuthorityFloat(current.speed,
                                        visibleRules06CSpeeds[ball]) &&
                                    current.motionState ==
                                        visibleRules06CMotionStates[ball])
                                    continue;
                                detail << ball << "@p="
                                    << settled.position.x << ','
                                    << settled.position.y << ','
                                    << settled.position.z << "->"
                                    << current.position.x << ','
                                    << current.position.y << ','
                                    << current.position.z << ",v="
                                    << settled.velocity.x << ','
                                    << settled.velocity.y << ','
                                    << settled.velocity.z << "->"
                                    << current.velocity.x << ','
                                    << current.velocity.y << ','
                                    << current.velocity.z << ",w="
                                    << settled.angularVelocity.x << ','
                                    << settled.angularVelocity.y << ','
                                    << settled.angularVelocity.z << "->"
                                    << current.angularVelocity.x << ','
                                    << current.angularVelocity.y << ','
                                    << current.angularVelocity.z << ",speed="
                                    << visibleRules06CSpeeds[ball] << "->"
                                    << current.speed << ",motion="
                                    << static_cast<int>(
                                        visibleRules06CMotionStates[ball])
                                    << "->" << static_cast<int>(
                                        current.motionState) << ';';
                            }
                            detail << ":settled_hash="
                                << visibleRules06CStateHash
                                << ":current_hash="
                                << runtime.rulesAuthorityStateHash();
                            throw std::runtime_error(detail.str());
                        }

                        if (visibleRules06CStage == 1) {
                            // This fixture-only configuration represents a local two-player
                            // product match.  The decision itself must still originate from
                            // an applied native cue impulse and the real rules collector.
                            if (!productSession->shell().startSinglePlayer(0x06cULL).ok)
                                throw std::runtime_error("visible_06c_match_setup_failed");
                            ai::PlayerConfiguration localPlayers;
                            if (!aiPlayer.configure(runtime, localPlayers).ok)
                                throw std::runtime_error("visible_06c_local_actor_setup_failed");
                            (void)dispatchNative(nativeKeyEvent(48, @"\t"),
                                "visible_06c_enter_aim");
                            // The measured 50-power stroke stops at z=21.0966,
                            // 36.6884 cm before first rack contact. One real
                            // product binding raises the default 60 to 65,
                            // above the derived 59.8683 contact threshold but
                            // far below the calibrated legal-break range.
                            (void)dispatchNative(nativeKeyEvent(69, @"+"),
                                "visible_06c_raise_power");
                            if (runtime.state().aim.mode != AimMode::Aim ||
                                std::fabs(runtime.shotControl().requestedPower - 65.0f) > 1e-5f)
                                throw std::runtime_error("visible_06c_native_controls_not_applied");
                            const NSPoint centerInView = NSMakePoint(NSMidX(bounds), NSMidY(bounds));
                            const NSPoint center = [view convertPoint:centerInView toView:nil];
                            (void)dispatchNative(nativeMouseEvent(
                                NSEventTypeLeftMouseDown, center), "visible_06c_shoot_down");
                            (void)dispatchNative(nativeMouseEvent(
                                NSEventTypeLeftMouseUp, center), "visible_06c_shoot_up");
                            if (!runtime.hasCueContactResult() ||
                                !runtime.cueContactResult().applied ||
                                !(runtime.cueContactResult().normalImpulseNs > 0.0))
                                throw std::runtime_error("visible_06c_native_cue_impulse_missing");
                            int ticks = 0;
                            while (runtime.state().ballsMoving && ticks < 5000) {
                                const ActionResult stepped = runtime.step(1);
                                if (!stepped.ok) {
                                    std::ostringstream detail;
                                    detail << "visible_06c_physics_step_failed:"
                                        << stepped.errorCode
                                        << ":committed_tick=" << runtime.tick()
                                        << ":local_step=" << ticks
                                        << ":completion=" << runtime.rulesShotCompletionError()
                                        << ":table_departure=" <<
                                            runtime.tableDepartureObservationError()
                                        << ":event_budget=" <<
                                            runtime.physicsProfile().solver.maximumEventsPerTick;
                                    if (runtime.hasRecentPhysicsStep()) {
                                        const auto& failed = runtime.latestPhysicsStep();
                                        const auto& telemetry = failed.telemetry;
                                        detail << ":attempt_tick=" << failed.tick
                                            << ":attempt_sequence=" << failed.attemptSequence
                                            << ":failure_code=" <<
                                                physicsFailureCodeName(telemetry.failureCode)
                                            << ":failure_detail=" << telemetry.failureDetail
                                            << ":failing_event=" << telemetry.failingEventId
                                            << ":failing_island=" << telemetry.failingIslandId
                                            << ":contacts=" << telemetry.contacts.size()
                                            << ":solver_events=" << telemetry.solverEvents.size()
                                            << ":departures=" <<
                                                telemetry.tableDepartureEvents.size()
                                            << ":segments=" <<
                                                telemetry.analyticSurfaceMotion.size()
                                            << ":max_penetration=" <<
                                                telemetry.maximumPenetrationCm;
                                    }
                                    detail << ":active_balls=";
                                    for (int ball = 0; ball < kBallCount; ++ball) {
                                        const auto& state = runtime.state().balls[ball];
                                        if (state.pocketed || state.outOfPlay ||
                                            (state.velocity.x == 0.0f &&
                                             state.velocity.y == 0.0f &&
                                             state.velocity.z == 0.0f)) continue;
                                        detail << ball << '@' << state.position.x << ','
                                            << state.position.y << ',' << state.position.z
                                            << "v" << state.velocity.x << ','
                                            << state.velocity.y << ',' << state.velocity.z
                                            << "w" << state.angularVelocity.x << ','
                                            << state.angularVelocity.y << ','
                                            << state.angularVelocity.z << ';';
                                    }
                                    throw std::runtime_error(detail.str());
                                }
                                productSession->consumeAuthoritySteps();
                                ++ticks;
                            }
                            previous = runtime.state();
                            accumulator = 0.0;
                            productSession->syncRulesDecisionAuthority();
                            const auto offer = runtime.breakFoulDecisionOffer();
                            const auto decision = productSession->snapshot().rulesDecision;
                            const std::vector<std::string> exactActions{
                                rules::kBreakFoulAcceptTableActionId,
                                rules::kBreakFoulRerackOpponentActionId,
                                rules::kBreakFoulRerackOriginalActionId};
                            if (runtime.state().ballsMoving || ticks >= 5000 ||
                                !runtime.rulesShotCompletionError().empty() ||
                                !runtime.hasLastShotFacts() ||
                                !runtime.lastShotFacts().breakShot ||
                                !runtime.hasLastAdjudication() ||
                                runtime.lastAdjudication().foul || !offer ||
                                offer->branch != rules::BreakFoulBranch::IllegalBreak ||
                                offer->offeredActionIds != exactActions ||
                                decision.visibleActionIds != exactActions ||
                                decision.state != rules::ui::DecisionUiState::DecisionOpenUnselected ||
                                !productSession->snapshot().rulesDecisionProductActive) {
                                std::ostringstream detail;
                                detail << "visible_06c_real_offer_not_reached"
                                    << ":ticks=" << ticks
                                    << ":moving=" << runtime.state().ballsMoving
                                    << ":completion=" << runtime.rulesShotCompletionError()
                                    << ":power=" << runtime.shotControl().requestedPower
                                    << ":yaw=" << runtime.shotControl().yawRadians
                                    << ":cue_final=" << runtime.state().balls[0].position.x
                                    << ',' << runtime.state().balls[0].position.z
                                    << ":facts=" << runtime.hasLastShotFacts();
                                if (runtime.hasLastShotFacts()) {
                                    const auto& facts = runtime.lastShotFacts();
                                    detail << ":break=" << facts.breakShot
                                        << ":first=" << facts.firstContactBall
                                        << ":first_count=" << facts.firstContactBalls.size()
                                        << ":rails=" << facts.distinctObjectBallsToRail
                                        << ":break_evidence=" << facts.breakEvidenceComplete
                                        << ":pockets=" << facts.pocketedBallsInOrder.size();
                                }
                                detail << ":adjudication=" << runtime.hasLastAdjudication();
                                if (runtime.hasLastAdjudication()) {
                                    const auto& adjudication = runtime.lastAdjudication();
                                    detail << ":foul=" << adjudication.foul << ":reasons=";
                                    for (const auto reason : adjudication.reasons)
                                        detail << rules::foulReasonCode(reason) << ',';
                                }
                                detail << ":offer=" << static_cast<bool>(offer);
                                if (offer)
                                    detail << ":offer_branch=" << static_cast<int>(offer->branch)
                                        << ":offer_actions=" << offer->offeredActionIds.size();
                                detail << ":decision_state=" << static_cast<int>(decision.state)
                                    << ":decision_actions=" << decision.visibleActionIds.size()
                                    << ":product_active=" <<
                                        productSession->snapshot().rulesDecisionProductActive;
                                throw std::runtime_error(detail.str());
                            }
                            visibleRules06CRevision = runtime.rulesAuthorityRevision();
                            visibleRules06CDecisionId = offer->decisionId;
                            visibleRules06CStateHash =
                                runtime.rulesAuthorityStateHash();
                            visibleRules06CGameState = runtime.state();
                            for (int ball = 0; ball < kBallCount; ++ball) {
                                visibleRules06CSpeeds[ball] =
                                    runtime.state().balls[ball].speed;
                                visibleRules06CMotionStates[ball] =
                                    runtime.state().balls[ball].motionState;
                            }
                            visibleRules06CStage = 2;
                        } else if (visibleRules06CStage == 2) {
                            requireRenderedProductFrame(
                                ui::ProductUiOverlayKind::RulesDecision,
                                "visible_06c_offer");
                            const auto& hits =
                                renderer.latestProductUiResult().rulesDecisionHitRects;
                            std::vector<std::string> actionIds;
                            for (const auto& hit : hits)
                                if (hit.enabled && !hit.confirm)
                                    actionIds.push_back(hit.actionId);
                            const std::vector<std::string> exactActions{
                                rules::kBreakFoulAcceptTableActionId,
                                rules::kBreakFoulRerackOpponentActionId,
                                rules::kBreakFoulRerackOriginalActionId};
                            if (actionIds != exactActions)
                                throw std::runtime_error("visible_06c_three_buttons_not_rendered");
                            visibleRules06CStage = 3; // Import these rendered rects next frame.
                        } else if (visibleRules06CStage == 3) {
                            const auto& hits =
                                renderer.latestProductUiResult().rulesDecisionHitRects;
                            const auto action = std::find_if(hits.begin(), hits.end(),
                                [&](const auto& hit) {
                                    return hit.enabled && !hit.confirm &&
                                        hit.actionId == visibleRules06CActionId;
                                });
                            if (action == hits.end())
                                throw std::runtime_error("visible_06c_selected_button_missing");
                            const NSPoint point = rulesControlPoint(*action);
                            (void)dispatchNative(nativeMouseEvent(
                                NSEventTypeLeftMouseDown, point),
                                "visible_06c_focus_cancel_down");
                            postDeterministicCocoaFocusLoss();
                            const auto cancelledUp = productPlatformEvent(nativeMouseEvent(
                                NSEventTypeLeftMouseUp, point), view,
                                nativeInput.pointer, *productSession);
                            const auto retained = runtime.breakFoulDecisionOffer();
                            if (cancelledUp || !retained ||
                                retained->decisionId != visibleRules06CDecisionId ||
                                runtime.rulesAuthorityRevision() != visibleRules06CRevision ||
                                !productSession->snapshot().rulesDecision.armedActionId.empty() ||
                                productSession->snapshot().rulesDecision.state !=
                                    rules::ui::DecisionUiState::DecisionOpenUnselected)
                                throw std::runtime_error(
                                    "visible_06c_focus_cancelled_gesture_mutated_authority");
                            [platform.window makeKeyAndOrderFront:nil];
                            visibleRules06CStage = 4;
                        } else if (visibleRules06CStage == 4) {
                            requireRenderedProductFrame(
                                ui::ProductUiOverlayKind::RulesDecision,
                                "visible_06c_focus_recovery_offer");
                            const auto& hits =
                                renderer.latestProductUiResult().rulesDecisionHitRects;
                            const auto action = std::find_if(hits.begin(), hits.end(),
                                [&](const auto& hit) {
                                    return hit.enabled && !hit.confirm &&
                                        hit.actionId == visibleRules06CActionId;
                                });
                            if (action == hits.end())
                                throw std::runtime_error(
                                    "visible_06c_focus_recovery_action_missing");
                            (void)clickRulesControl(*action,
                                "visible_06c_focus_recovery_arm");
                            visibleRules06CArmFocusLossBaseline =
                                nativeInput.focusLossNotificationCount;
                            visibleRules06CArmStage = visibleRules06CStage;
                            const auto retained = runtime.breakFoulDecisionOffer();
                            if (!retained || retained->decisionId != visibleRules06CDecisionId ||
                                runtime.rulesAuthorityRevision() != visibleRules06CRevision ||
                                productSession->snapshot().rulesDecision.armedActionId !=
                                    visibleRules06CActionId)
                                throw std::runtime_error(
                                    "visible_06c_focus_recovery_arm_mutated_authority");
                            visibleRules06CStage = 5;
                        } else if (visibleRules06CStage == 5) {
                            requireRenderedProductFrame(
                                ui::ProductUiOverlayKind::RulesDecision,
                                "visible_06c_armed_before_focus_loss");
                            const auto& hits =
                                renderer.latestProductUiResult().rulesDecisionHitRects;
                            const auto confirm = std::find_if(hits.begin(), hits.end(),
                                [&](const auto& hit) {
                                    return hit.enabled && hit.confirm &&
                                        hit.actionId == visibleRules06CActionId;
                                });
                            if (confirm == hits.end() ||
                                runtime.rulesAuthorityRevision() != visibleRules06CRevision)
                                throwConfirmDiagnostic(
                                    "visible_06c_confirm_not_rendered", hits);
                            postDeterministicCocoaFocusLoss();
                            const auto retained = runtime.breakFoulDecisionOffer();
                            if (!retained || retained->decisionId != visibleRules06CDecisionId ||
                                runtime.rulesAuthorityRevision() != visibleRules06CRevision ||
                                !productSession->snapshot().rulesDecision.armedActionId.empty() ||
                                productSession->snapshot().rulesDecision.state !=
                                    rules::ui::DecisionUiState::DecisionOpenUnselected)
                                throw std::runtime_error(
                                    "visible_06c_focus_cancelled_arm_mutated_authority");
                            [platform.window makeKeyAndOrderFront:nil];
                            visibleRules06CStage = 6;
                        } else if (visibleRules06CStage == 6) {
                            requireRenderedProductFrame(
                                ui::ProductUiOverlayKind::RulesDecision,
                                "visible_06c_recovered_offer");
                            const auto& hits =
                                renderer.latestProductUiResult().rulesDecisionHitRects;
                            const auto action = std::find_if(hits.begin(), hits.end(),
                                [&](const auto& hit) {
                                    return hit.enabled && !hit.confirm &&
                                        hit.actionId == visibleRules06CActionId;
                                });
                            if (action == hits.end())
                                throw std::runtime_error(
                                    "visible_06c_recovered_action_not_rendered");
                            visibleRules06CStage = 7; // Import the recovered action rect.
                        } else if (visibleRules06CStage == 7) {
                            const auto& hits =
                                renderer.latestProductUiResult().rulesDecisionHitRects;
                            const auto action = std::find_if(hits.begin(), hits.end(),
                                [&](const auto& hit) {
                                    return hit.enabled && !hit.confirm &&
                                        hit.actionId == visibleRules06CActionId;
                                });
                            if (action == hits.end())
                                throw std::runtime_error(
                                    "visible_06c_fresh_recovered_action_missing");
                            (void)clickRulesControl(*action, "visible_06c_recovered_arm");
                            visibleRules06CArmFocusLossBaseline =
                                nativeInput.focusLossNotificationCount;
                            visibleRules06CArmStage = visibleRules06CStage;
                            if (runtime.rulesAuthorityRevision() != visibleRules06CRevision ||
                                productSession->snapshot().rulesDecision.armedActionId !=
                                    visibleRules06CActionId)
                                throw std::runtime_error(
                                    "visible_06c_recovered_arm_mutated_authority");
                            visibleRules06CStage = 8;
                        } else if (visibleRules06CStage == 8) {
                            requireRenderedProductFrame(
                                ui::ProductUiOverlayKind::RulesDecision,
                                "visible_06c_recovered_armed");
                            const auto& hits =
                                renderer.latestProductUiResult().rulesDecisionHitRects;
                            const auto confirm = std::find_if(hits.begin(), hits.end(),
                                [&](const auto& hit) {
                                    return hit.enabled && hit.confirm &&
                                        hit.actionId == visibleRules06CActionId;
                                });
                            if (confirm == hits.end() ||
                                runtime.rulesAuthorityRevision() != visibleRules06CRevision)
                                throwConfirmDiagnostic(
                                    "visible_06c_recovered_confirm_not_rendered", hits);
                            visibleRules06CStage = 9; // Import the recovered confirm rect.
                        } else if (visibleRules06CStage == 9) {
                            const auto& hits =
                                renderer.latestProductUiResult().rulesDecisionHitRects;
                            const auto confirm = std::find_if(hits.begin(), hits.end(),
                                [&](const auto& hit) {
                                    return hit.enabled && hit.confirm &&
                                        hit.actionId == visibleRules06CActionId;
                                });
                            if (confirm == hits.end())
                                throwConfirmDiagnostic(
                                    "visible_06c_fresh_confirm_missing", hits);
                            const auto retainedOffer =
                                runtime.breakFoulDecisionOffer();
                            if (!retainedOffer || retainedOffer->decisionId !=
                                    visibleRules06CDecisionId)
                                throw std::runtime_error(
                                    "visible_06c_offer_missing_before_commit");
                            const bool rerack = visibleRules06CActionId !=
                                rules::kBreakFoulAcceptTableActionId;
                            const int expectedBreaker =
                                visibleRules06CActionId ==
                                    rules::kBreakFoulRerackOpponentActionId ?
                                1 - retainedOffer->originalBreakerPlayerId :
                                retainedOffer->originalBreakerPlayerId;
                            rules::RackFoundationPlan expectedRerack;
                            if (rerack) {
                                rules::RerackFoundationParameters parameters;
                                parameters.breakerPlayerId = expectedBreaker;
                                // Build an immutable expectation with the same
                                // pure official-rack constructor used by the
                                // authority.  The public staging API correctly
                                // rejects while this live 06C offer is pending.
                                expectedRerack =
                                    rules::buildRerackFoundationPlan(
                                        runtime.state(),
                                        runtime.physicsProfile(), parameters,
                                        runtime.rulesAuthorityRevision(),
                                        runtime.rulesAuthorityStateHash());
                                if (!expectedRerack.ok ||
                                    expectedRerack.placements.size() != kBallCount) {
                                    std::ostringstream detail;
                                    detail <<
                                        "visible_06c_expected_rerack_invalid:code="
                                        << expectedRerack.errorCode
                                        << ":placements=" <<
                                            expectedRerack.placements.size()
                                        << ":original_breaker=" <<
                                            retainedOffer->originalBreakerPlayerId
                                        << ":expected_breaker=" << expectedBreaker
                                        << ":revision=" <<
                                            runtime.rulesAuthorityRevision();
                                    throw std::runtime_error(detail.str());
                                }
                            }
                            (void)clickRulesControl(*confirm, "visible_06c_confirm");
                            const std::uint64_t committedRevision =
                                runtime.rulesAuthorityRevision();
                            bool exactRackFoundation = !rerack;
                            if (rerack) {
                                exactRackFoundation = std::all_of(
                                    expectedRerack.placements.begin(),
                                    expectedRerack.placements.end(),
                                    [&](const auto& placement) {
                                        const BallState& ball =
                                            runtime.state().balls[
                                                placement.first];
                                        return std::fabs(ball.position.x -
                                                    placement.second.x) <
                                                1e-5f &&
                                            std::fabs(ball.position.y -
                                                    placement.second.y) <
                                                1e-5f &&
                                            std::fabs(ball.position.z -
                                                    placement.second.z) <
                                                1e-5f &&
                                            ball.velocity.x == 0.0f &&
                                            ball.velocity.y == 0.0f &&
                                            ball.velocity.z == 0.0f &&
                                            ball.angularVelocity.x == 0.0f &&
                                            ball.angularVelocity.y == 0.0f &&
                                            ball.angularVelocity.z == 0.0f &&
                                            ball.speed == 0.0f &&
                                            !ball.pocketed && !ball.outOfPlay &&
                                            ball.physicalDomain ==
                                                PhysicalBallDomain::ClothSupported &&
                                            ball.pocketInteraction.phase ==
                                                PocketInteractionPhase::Outside &&
                                            ball.pocketInteraction.pocketId == -1;
                                    });
                            }
                            const auto& committedAction =
                                runtime.rulesAuthorityActions().back();
                            const bool exactRerackState = !rerack ||
                                (exactRackFoundation &&
                                    !runtime.state().ballsMoving &&
                                    runtime.state().pocketedBallCount == 0 &&
                                    runtime.state().nextPocketCaptureSequence ==
                                        1 &&
                                    !runtime.state().input.hitRequested &&
                                    !runtime.shotFactCollectorActive() &&
                                    !runtime.hasLastShotFacts() &&
                                    !runtime.hasLastAdjudication() &&
                                    runtime.matchState().currentPlayer ==
                                        expectedBreaker &&
                                    runtime.state().players.currentPlayer ==
                                        expectedBreaker &&
                                    runtime.state().players.nextPlayer ==
                                        expectedBreaker &&
                                    !runtime.matchState().ballInHand &&
                                    !runtime.ballInHandPlacementOffer() &&
                                    !runtime.groupChoiceDecisionOffer() &&
                                    !runtime.eightOnBreakDecisionOffer() &&
                                    !runtime.rulesRackTerminalState().active);
                            if (committedRevision != visibleRules06CRevision + 1 ||
                                runtime.breakFoulDecisionOffer().has_value() ||
                                committedAction.decisionId !=
                                    visibleRules06CDecisionId ||
                                committedAction.actionId !=
                                    visibleRules06CActionId ||
                                committedAction.actorPlayerId !=
                                    retainedOffer->actorPlayerId ||
                                committedAction.previousStateRevision !=
                                    visibleRules06CRevision ||
                                committedAction.authoritativeStateRevision !=
                                    committedRevision ||
                                committedAction.authoritativeStateHash !=
                                    runtime.rulesAuthorityStateHash() ||
                                runtime.matchState().phase !=
                                    (rerack ? rules::MatchPhase::Break :
                                        rules::MatchPhase::OpenTable) ||
                                runtime.matchState().groups[0] != rules::BallGroup::Open ||
                                runtime.matchState().groups[1] != rules::BallGroup::Open ||
                                runtime.matchState().winner != -1 ||
                                !exactRerackState)
                                throw std::runtime_error("visible_06c_authority_ack_mismatch");
                            // The completed overlay disappears immediately. The old screen
                            // coordinate is therefore no longer a rules control: its release
                            // must receive the exact gameplay input-unbound rejection, with
                            // no second authority mutation.
                            const std::string committedHash =
                                runtime.rulesAuthorityStateHash();
                            const NSPoint stalePoint = rulesControlPoint(*confirm);
                            const auto staleDown = productPlatformEvent(
                                nativeMouseEvent(NSEventTypeLeftMouseDown,
                                    stalePoint), view, nativeInput.pointer,
                                *productSession);
                            if (!staleDown)
                                throw std::runtime_error(
                                    "visible_06c_stale_confirm_translation_failed");
                            const ModernProductEventResult staleDownResult =
                                productSession->handlePlatformInput(*staleDown);
                            const auto staleUp = productPlatformEvent(
                                nativeMouseEvent(NSEventTypeLeftMouseUp,
                                    stalePoint), view, nativeInput.pointer,
                                *productSession);
                            if (!staleUp)
                                throw std::runtime_error(
                                    "visible_06c_stale_confirm_translation_failed");
                            const ModernProductEventResult staleUpResult =
                                productSession->handlePlatformInput(*staleUp);
                            if (!staleDownResult.ok || staleUpResult.ok ||
                                !staleUpResult.handled ||
                                staleUpResult.errorCode != "input_unbound" ||
                                runtime.rulesAuthorityRevision() != committedRevision ||
                                runtime.rulesAuthorityStateHash() != committedHash)
                                throw std::runtime_error(
                                    "visible_06c_stale_confirm_contract_failed");
                            visibleRules06CStage = 10;
                        } else if (visibleRules06CStage == 10) {
                            requireRenderedProductFrame(
                                ui::ProductUiOverlayKind::GameplayHud,
                                "visible_06c_committed");
                            if (!renderer.latestProductUiResult().rulesDecisionHitRects.empty() ||
                                productSession->snapshot().rulesDecisionProductActive)
                                throw std::runtime_error(
                                    "visible_06c_committed_decision_still_visible");
                            const auto& receiptAction =
                                runtime.rulesAuthorityActions().back();
                            if (playtestLog) {
                                auto receipt = json::Value::object();
                                receipt["decision_id"] = json::Value(
                                    visibleRules06CDecisionId);
                                receipt["action_id"] = json::Value(
                                    visibleRules06CActionId);
                                receipt["actor_player_id"] = json::Value(
                                    receiptAction.actorPlayerId);
                                receipt["revision_before"] = json::Value(
                                    std::to_string(visibleRules06CRevision));
                                receipt["revision_after"] = json::Value(
                                    std::to_string(
                                        runtime.rulesAuthorityRevision()));
                                receipt["authority_state_hash"] = json::Value(
                                    runtime.rulesAuthorityStateHash());
                                receipt["physical_collector"] = json::Value(
                                    "weak_break");
                                receipt["rack_rebuilt"] = json::Value(
                                    visibleRules06CActionId !=
                                        rules::kBreakFoulAcceptTableActionId);
                                receipt["breaker_player_id"] = json::Value(
                                    runtime.matchState().currentPlayer);
                                receipt["focus_cancel"] = json::Value(
                                    "gesture,armed");
                                receipt["stale_confirm_error"] = json::Value(
                                    "input_unbound");
                                receipt["gpu_submitted"] = json::Value(
                                    "offer,armed,committed");
                                playtestLog->record(
                                    "rules_decision_ack", runtime, receipt);
                            }
                            output << "visible_rules_06c actions=3 decision="
                                << visibleRules06CDecisionId << " revision_before="
                                << visibleRules06CRevision << " revision_after="
                                << runtime.rulesAuthorityRevision()
                                << " action=" << visibleRules06CActionId
                                << " physical_collector=weak_break"
                                << " rack_rebuilt=" <<
                                    (visibleRules06CActionId ==
                                        rules::kBreakFoulAcceptTableActionId ?
                                        0 : 1)
                                << " breaker=" <<
                                    runtime.matchState().currentPlayer
                                << " authority_hash=" <<
                                    runtime.rulesAuthorityStateHash()
                                << " focus_cancel=gesture,armed"
                                << " local_actor_fixture=ai_disabled_after_product_start"
                                << " gpu_submitted=offer,armed,committed\n";
                            visibleRules06CStage = 0;
                            if (std::getenv(
                                    "BILLIARDGL_VISIBLE_RULES_06C_ACTION") !=
                                nullptr)
                                [platform.window close];
                            else
                                visibleRules06EStage = 1;
                        } else if (visibleRules06EStage == 1) {
                            // Fixture setup only: move the cue ball to the left side-pocket
                            // mouth and align it toward the capture channel. This is not a
                            // standard full-rack break layout. The subsequent stroke still
                            // enters through native product input, and the 06E offer must be
                            // produced by succeeded physics telemetry, the shot-fact
                            // collector, and normal rules adjudication.
                            if (!productSession->shell().startSinglePlayer(
                                    0x06eULL).ok)
                                throw std::runtime_error(
                                    "visible_06e_match_setup_failed");
                            ai::PlayerConfiguration localPlayers;
                            if (!aiPlayer.configure(runtime, localPlayers).ok)
                                throw std::runtime_error(
                                    "visible_06e_local_actor_setup_failed");
                            BallState cue = runtime.state().balls[0];
                            cue.position = Point3{
                                -runtime.physicsProfile().tableBoundary.
                                        playfieldWidthCm * 0.5f +
                                    runtime.physicsProfile().ball.radiusCm,
                                kTableHeight +
                                    runtime.physicsProfile().ball.radiusCm,
                                0.0f};
                            cue.velocity = {};
                            cue.angularVelocity = {};
                            cue.speed = 0.0f;
                            cue.motionState = BallMotionState::Stationary;
                            cue.pocketed = false;
                            cue.outOfPlay = false;
                            cue.pocketInteraction = {};
                            if (!runtime.setBall(0, cue).ok)
                                throw std::runtime_error(
                                    "visible_06e_cue_setup_failed");
                            (void)dispatchNative(nativeKeyEvent(48, @"\t"),
                                "visible_06e_enter_aim");
                            GameAction fixtureAim;
                            fixtureAim.type = ActionType::SetAimYaw;
                            fixtureAim.first = 3.14159265358979323846f;
                            if (!runtime.dispatch(fixtureAim).ok)
                                throw std::runtime_error(
                                    "visible_06e_fixture_aim_failed");
                            const NSPoint centerInView = NSMakePoint(
                                NSMidX(bounds), NSMidY(bounds));
                            const NSPoint center = [view convertPoint:
                                centerInView toView:nil];
                            (void)dispatchNative(nativeMouseEvent(
                                NSEventTypeLeftMouseDown, center),
                                "visible_06e_shoot_down");
                            (void)dispatchNative(nativeMouseEvent(
                                NSEventTypeLeftMouseUp, center),
                                "visible_06e_shoot_up");
                            if (!runtime.hasCueContactResult() ||
                                !runtime.cueContactResult().applied ||
                                !(runtime.cueContactResult().normalImpulseNs >
                                    0.0))
                                throw std::runtime_error(
                                    "visible_06e_native_cue_impulse_missing");
                            int ticks = 0;
                            while (runtime.state().ballsMoving && ticks < 240) {
                                const ActionResult stepped = runtime.step(1);
                                if (!stepped.ok)
                                    throw std::runtime_error(
                                        std::string(
                                            "visible_06e_physics_step_failed:") +
                                        stepped.errorCode);
                                productSession->consumeAuthoritySteps();
                                ++ticks;
                            }
                            previous = runtime.state();
                            accumulator = 0.0;
                            productSession->syncRulesDecisionAuthority();
                            const auto offer = runtime.breakFoulDecisionOffer();
                            const auto decision =
                                productSession->snapshot().rulesDecision;
                            const std::vector<std::string> exactActions{
                                rules::kBreakFoulBallInHandActionId,
                                rules::kBreakFoulRerackOpponentActionId,
                                rules::kBreakFoulRerackOriginalActionId};
                            bool cueScratch = false;
                            if (runtime.hasLastAdjudication()) {
                                const auto& reasons =
                                    runtime.lastAdjudication().reasons;
                                cueScratch = std::find(reasons.begin(),
                                    reasons.end(),
                                    rules::FoulReason::CueBallScratch) !=
                                    reasons.end();
                            }
                            const bool canonicalScratch =
                                runtime.hasLastShotFacts() &&
                                runtime.lastShotFacts().breakShot &&
                                runtime.lastShotFacts().cueBallPocketed &&
                                runtime.lastShotFacts().
                                    cueBallScratchEvidenceOrigin ==
                                    rules::CueBallScratchEvidenceOrigin::
                                        AuthoritativeShotFactCollector &&
                                runtime.lastShotFacts().
                                    cueBallScratchEvidenceComplete &&
                                runtime.lastShotFacts().
                                    cueBallPocketCaptures.size() == 1;
                            if (runtime.state().ballsMoving || ticks >= 240 ||
                                !runtime.rulesShotCompletionError().empty() ||
                                !canonicalScratch || !cueScratch || !offer ||
                                offer->branch !=
                                    rules::BreakFoulBranch::BreakShotFoul ||
                                offer->sourceClause !=
                                    rules::kBreakShotFoulSourceClause ||
                                offer->offeredActionIds != exactActions ||
                                decision.visibleActionIds != exactActions ||
                                decision.state != rules::ui::DecisionUiState::
                                    DecisionOpenUnselected ||
                                !productSession->snapshot().
                                    rulesDecisionProductActive) {
                                std::ostringstream detail;
                                detail << "visible_06e_real_offer_not_reached"
                                    << ":ticks=" << ticks
                                    << ":moving=" <<
                                        runtime.state().ballsMoving
                                    << ":completion=" <<
                                        runtime.rulesShotCompletionError()
                                    << ":facts=" <<
                                        runtime.hasLastShotFacts()
                                    << ":canonical_scratch=" <<
                                        canonicalScratch
                                    << ":scratch_reason=" << cueScratch
                                    << ":offer=" <<
                                        static_cast<bool>(offer)
                                    << ":decision_state=" <<
                                        static_cast<int>(decision.state);
                                if (offer)
                                    detail << ":offer_branch=" <<
                                        static_cast<int>(offer->branch)
                                        << ":offer_clause=" <<
                                            offer->sourceClause
                                        << ":offer_actions=" <<
                                            offer->offeredActionIds.size();
                                throw std::runtime_error(detail.str());
                            }
                            visibleRules06EParentRevision =
                                runtime.rulesAuthorityRevision();
                            visibleRules06EDecisionId = offer->decisionId;
                            visibleRules06EStage = 2;
                        } else if (visibleRules06EStage == 2) {
                            requireRenderedProductFrame(
                                ui::ProductUiOverlayKind::RulesDecision,
                                "visible_06e_parent_offer");
                            const auto& hits = renderer.latestProductUiResult().
                                rulesDecisionHitRects;
                            std::vector<std::string> actions;
                            for (const auto& hit : hits)
                                if (hit.enabled && !hit.confirm)
                                    actions.push_back(hit.actionId);
                            const std::vector<std::string> expected{
                                rules::kBreakFoulBallInHandActionId,
                                rules::kBreakFoulRerackOpponentActionId,
                                rules::kBreakFoulRerackOriginalActionId};
                            if (actions != expected)
                                throw std::runtime_error(
                                    "visible_06e_three_buttons_not_rendered");
                            visibleRules06EStage = 3;
                        } else if (visibleRules06EStage == 3) {
                            const auto& hits = renderer.latestProductUiResult().
                                rulesDecisionHitRects;
                            const auto action = std::find_if(hits.begin(),
                                hits.end(), [](const auto& hit) {
                                    return hit.enabled && !hit.confirm &&
                                        hit.actionId == rules::
                                            kBreakFoulBallInHandActionId;
                                });
                            if (action == hits.end())
                                throw std::runtime_error(
                                    "visible_06e_bih_button_missing");
                            (void)clickRulesControl(*action,
                                "visible_06e_parent_arm");
                            if (runtime.rulesAuthorityRevision() !=
                                    visibleRules06EParentRevision ||
                                productSession->snapshot().rulesDecision.
                                    armedActionId != rules::
                                        kBreakFoulBallInHandActionId)
                                throw std::runtime_error(
                                    "visible_06e_parent_arm_mutated_authority");
                            visibleRules06EStage = 4;
                        } else if (visibleRules06EStage == 4) {
                            requireRenderedProductFrame(
                                ui::ProductUiOverlayKind::RulesDecision,
                                "visible_06e_parent_armed");
                            const auto& hits = renderer.latestProductUiResult().
                                rulesDecisionHitRects;
                            const auto confirm = std::find_if(hits.begin(),
                                hits.end(), [](const auto& hit) {
                                    return hit.enabled && hit.confirm &&
                                        hit.actionId == rules::
                                            kBreakFoulBallInHandActionId;
                                });
                            if (confirm == hits.end())
                                throw std::runtime_error(
                                    "visible_06e_parent_confirm_missing");
                            // Renderer output is imported into the session at
                            // the next loop boundary.  Do not click a freshly
                            // rendered Confirm against the preceding action
                            // rectangles.
                            visibleRules06EStage = 40;
                        } else if (visibleRules06EStage == 40) {
                            const auto& hits = renderer.latestProductUiResult().
                                rulesDecisionHitRects;
                            const auto confirm = std::find_if(hits.begin(),
                                hits.end(), [](const auto& hit) {
                                    return hit.enabled && hit.confirm &&
                                        hit.actionId == rules::
                                            kBreakFoulBallInHandActionId;
                                });
                            if (confirm == hits.end())
                                throw std::runtime_error(
                                    "visible_06e_fresh_parent_confirm_missing");
                            (void)clickRulesControl(*confirm,
                                "visible_06e_parent_confirm");
                            const auto placement =
                                runtime.ballInHandPlacementOffer();
                            const auto parentAfter = productSession->snapshot();
                            const bool parentAckMatches =
                                runtime.rulesAuthorityRevision() ==
                                    visibleRules06EParentRevision + 1 &&
                                !runtime.breakFoulDecisionOffer().has_value() &&
                                placement && placement->scope ==
                                    rules::BallInHandPlacementScope::
                                        BehindHeadString &&
                                parentAfter.rulesDecision.offer.flowId ==
                                    rules::kBehindHeadStringBallInHandFlowId;
                            if (!parentAckMatches) {
                                std::ostringstream detail;
                                detail << "visible_06e_parent_ack_mismatch"
                                    << ":revision=" <<
                                        runtime.rulesAuthorityRevision()
                                    << ":expected_revision=" <<
                                        visibleRules06EParentRevision + 1
                                    << ":parent_offer=" <<
                                        runtime.breakFoulDecisionOffer().has_value()
                                    << ":placement_offer=" <<
                                        static_cast<bool>(placement)
                                    << ":match_bih=" <<
                                        runtime.matchState().ballInHand
                                    << ":runtime_scope=" << static_cast<int>(
                                        runtime.ballInHandPlacementScope())
                                    << ":ui_active=" <<
                                        parentAfter.rulesDecisionProductActive
                                    << ":ui_state=" << static_cast<int>(
                                        parentAfter.rulesDecision.state)
                                    << ":ui_flow=" <<
                                        parentAfter.rulesDecision.offer.flowId
                                    << ":ui_decision=" <<
                                        parentAfter.rulesDecision.offer.decisionId
                                    << ":ui_actions=" <<
                                        parentAfter.rulesDecision.
                                            visibleActionIds.size()
                                    << ":authority_actions=" <<
                                        runtime.rulesAuthorityActions().size();
                                if (placement) {
                                    detail << ":placement_scope=" <<
                                        static_cast<int>(placement->scope)
                                        << ":placement_actor=" <<
                                            placement->actorPlayerId
                                        << ":placement_revision=" <<
                                            placement->authorityStateRevision
                                        << ":placement_decision=" <<
                                            placement->decisionId;
                                }
                                throw std::runtime_error(detail.str());
                            }
                            visibleRules06EChildRevision =
                                runtime.rulesAuthorityRevision();
                            visibleRules06EChildDecisionId =
                                placement->decisionId;
                            visibleRules06EActor = placement->actorPlayerId;
                            // Exercise the renderer's legacy projection-pan lane as
                            // well as the session inverse projection. This is
                            // fixture camera state, not a menu interaction.
                            runtime.mutableState().camera.panX = 13.0f;
                            runtime.mutableState().camera.panZ = -7.0f;
                            visibleRules06EStage = 5;
                        } else if (visibleRules06EStage == 5) {
                            requireRenderedProductFrame(
                                ui::ProductUiOverlayKind::RulesDecision,
                                "visible_06e_child_offer");
                            // Exercise the shipping wheel-zoom input seam
                            // from the dedicated overhead framing before
                            // choosing a coordinate.  This
                            // is presentation-only and must not acknowledge
                            // either authority offer.
                            const float distanceBefore = productSession->
                                presentationCameraFrame().pose.distanceCm;
                            PlatformInputEvent zoom;
                            zoom.kind = PlatformInputKind::Scroll;
                            zoom.scrollY = 8.0;
                            const auto zoomed = productSession->
                                handlePlatformInput(zoom);
                            const float distanceAfter = productSession->
                                presentationCameraFrame().pose.distanceCm;
                            if (!zoomed.ok || !zoomed.handled ||
                                distanceAfter > distanceBefore - 70.0f ||
                                runtime.rulesAuthorityRevision() !=
                                    visibleRules06EChildRevision)
                                throw std::runtime_error(
                                    "visible_06e_placement_zoom_failed");
                            // Wait one rendered frame so its exact child modal
                            // exclusions are imported at the next loop boundary.
                            visibleRules06EStage = 6;
                        } else if (visibleRules06EStage == 6) {
                            const auto& uiResult =
                                renderer.latestProductUiResult();
                            requireRenderedProductFrame(
                                ui::ProductUiOverlayKind::RulesDecision,
                                "visible_06e_child_pointer");
                            const auto& authority =
                                productSession->presentationRuntime();
                            const auto frame = buildPresentationSnapshot(
                                authority.state(), authority.state(), 1.0,
                                authority.shotControl(),
                                authority.shotControlCapabilities(),
                                productSession->presentationCameraFrame(),
                                buildRulesPresentation(authority));
                            if (std::fabs(frame.camera.projectionPan.x) < 1e-5f ||
                                std::fabs(frame.camera.projectionPan.z) < 1e-5f)
                                throw std::runtime_error(
                                    "visible_06e_projection_pan_missing");
                            const auto& exclusions =
                                uiResult.rulesDecisionExclusionRects;
                            if (exclusions.empty())
                                throw std::runtime_error(
                                    "visible_06e_modal_exclusion_missing");
                            const auto& modal = exclusions.front();
                            (void)dispatchNative(nativeMouseEvent(
                                NSEventTypeMouseMoved,
                                physicalPoint(modal.x + modal.width * 0.5,
                                    modal.y + modal.height * 0.5)),
                                "visible_06e_modal_pointer");
                            if (productSession->snapshot().rulesDecision.
                                    placementPreviewVisible ||
                                runtime.rulesAuthorityRevision() !=
                                    visibleRules06EChildRevision)
                                throw std::runtime_error(
                                    "visible_06e_modal_pointer_leaked");

                            const auto layout = productSession->shellLayout();
                            bool foundPlacement = false;
                            PhysicalScreenPoint placementScreen;
                            // Search the actual visible pointer domain, then
                            // feed its inverse-projected table point to the
                            // runtime validator.  A fixed world-space grid can
                            // be entirely hidden by the modal for a valid
                            // orbit/projection-pan camera even though legal
                            // table pixels remain available.
                            TablePlanePointerProjectionRequest request;
                            request.camera = frame.camera;
                            request.viewport = {
                                layout.viewport.logicalWidth,
                                layout.viewport.logicalHeight,
                                layout.viewport.physicalWidth,
                                layout.viewport.physicalHeight};
                            request.tablePlaneY = kTableHeight;
                            for (const auto& rect : exclusions)
                                request.excludedPhysicalRects.push_back(
                                    {rect.x, rect.y, rect.width, rect.height});
                            int projectedCount = 0;
                            int occludedCount = 0;
                            int projectionFailureCount = 0;
                            int runtimeRejectedCount = 0;
                            std::string firstProjectionError;
                            std::string firstValidationError;
                            double minimumProjectedX =
                                std::numeric_limits<double>::infinity();
                            double maximumProjectedX =
                                -std::numeric_limits<double>::infinity();
                            double minimumProjectedZ =
                                std::numeric_limits<double>::infinity();
                            double maximumProjectedZ =
                                -std::numeric_limits<double>::infinity();
                            for (int row = 1; row < 20 && !foundPlacement;
                                    ++row) {
                                for (int column = 1; column < 20; ++column) {
                                    const double physicalX =
                                        layout.viewport.physicalWidth *
                                        column / 20.0;
                                    const double physicalY =
                                        layout.viewport.physicalHeight *
                                        row / 20.0;
                                    request.logicalX = physicalX *
                                        layout.viewport.logicalWidth /
                                        layout.viewport.physicalWidth;
                                    request.logicalY = physicalY *
                                        layout.viewport.logicalHeight /
                                        layout.viewport.physicalHeight;
                                    const auto projected =
                                        projectPointerToTablePlane(request);
                                    if (!projected.ok) {
                                        if (projected.errorCode ==
                                                "pointer_occluded_by_ui")
                                            ++occludedCount;
                                        else
                                            ++projectionFailureCount;
                                        if (firstProjectionError.empty())
                                            firstProjectionError =
                                                projected.errorCode;
                                        continue;
                                    }
                                    ++projectedCount;
                                    minimumProjectedX = std::min(
                                        minimumProjectedX,
                                        static_cast<double>(
                                            projected.tablePoint.x));
                                    maximumProjectedX = std::max(
                                        maximumProjectedX,
                                        static_cast<double>(
                                            projected.tablePoint.x));
                                    minimumProjectedZ = std::min(
                                        minimumProjectedZ,
                                        static_cast<double>(
                                            projected.tablePoint.z));
                                    maximumProjectedZ = std::max(
                                        maximumProjectedZ,
                                        static_cast<double>(
                                            projected.tablePoint.z));
                                    const rules::BallInHandPlacementPosition
                                        candidate{projected.tablePoint.x,
                                            projected.tablePoint.z};
                                    const auto validation =
                                        runtime.validateBallInHandPlacement(
                                            candidate);
                                    if (!validation.ok) {
                                        ++runtimeRejectedCount;
                                        if (firstValidationError.empty())
                                            firstValidationError =
                                                validation.errorCode;
                                        continue;
                                    }
                                    visibleRules06EPlacement =
                                        validation.authoritativePosition;
                                    placementScreen = {physicalX, physicalY};
                                    foundPlacement = true;
                                    break;
                                }
                            }
                            if (!foundPlacement) {
                                std::ostringstream detail;
                                detail << "visible_06e_legal_pointer_not_found"
                                    << ":projected=" << projectedCount
                                    << ":occluded=" << occludedCount
                                    << ":projection_failed=" <<
                                        projectionFailureCount
                                    << ":runtime_rejected=" <<
                                        runtimeRejectedCount
                                    << ":projection_error=" <<
                                        firstProjectionError
                                    << ":validation_error=" <<
                                        firstValidationError
                                    << ":projected_x=" << minimumProjectedX
                                        << ',' << maximumProjectedX
                                    << ":projected_z=" << minimumProjectedZ
                                        << ',' << maximumProjectedZ
                                    << ":camera_eye=" << frame.camera.eye.x
                                        << ',' << frame.camera.eye.y << ','
                                        << frame.camera.eye.z
                                    << ":camera_target=" <<
                                        frame.camera.target.x << ','
                                        << frame.camera.target.y << ','
                                        << frame.camera.target.z
                                    << ":camera_pan=" <<
                                        frame.camera.projectionPan.x << ','
                                        << frame.camera.projectionPan.y << ','
                                        << frame.camera.projectionPan.z
                                    << ":viewport=" <<
                                        layout.viewport.physicalWidth << 'x'
                                        << layout.viewport.physicalHeight
                                    << ":exclusions=" << exclusions.size();
                                throw std::runtime_error(detail.str());
                            }
                            const NSPoint placementPoint = physicalPoint(
                                placementScreen.x, placementScreen.y);
                            (void)dispatchNative(nativeMouseEvent(
                                NSEventTypeMouseMoved, placementPoint),
                                "visible_06e_placement_preview");
                            const auto preview =
                                productSession->snapshot().rulesDecision;
                            if (!preview.placementPreviewVisible ||
                                !preview.placementPreviewLegal ||
                                std::fabs(preview.placementPreview.worldX -
                                    visibleRules06EPlacement.worldX) > 0.02 ||
                                std::fabs(preview.placementPreview.worldZ -
                                    visibleRules06EPlacement.worldZ) > 0.02 ||
                                runtime.rulesAuthorityRevision() !=
                                    visibleRules06EChildRevision)
                                throw std::runtime_error(
                                    "visible_06e_placement_preview_invalid");
                            (void)dispatchNative(nativeKeyEvent(53, @"\x1b"),
                                "visible_06e_preview_cancel");
                            if (productSession->snapshot().rulesDecision.
                                    placementPreviewVisible ||
                                runtime.rulesAuthorityRevision() !=
                                    visibleRules06EChildRevision)
                                throw std::runtime_error(
                                    "visible_06e_preview_cancel_mutated_authority");
                            (void)dispatchNative(nativeMouseEvent(
                                NSEventTypeMouseMoved, placementPoint),
                                "visible_06e_placement_preview_restore");
                            if (!productSession->snapshot().rulesDecision.
                                    placementPreviewLegal)
                                throw std::runtime_error(
                                    "visible_06e_preview_restore_failed");
                            visibleRules06EStage = 7;
                        } else if (visibleRules06EStage == 7) {
                            requireRenderedProductFrame(
                                ui::ProductUiOverlayKind::RulesDecision,
                                "visible_06e_bih_preview");
                            const auto& hits = renderer.latestProductUiResult().
                                rulesDecisionHitRects;
                            const auto action = std::find_if(hits.begin(),
                                hits.end(), [](const auto& hit) {
                                    return hit.enabled && !hit.confirm &&
                                        hit.actionId == rules::
                                            kConfirmBehindHeadStringPlacementActionId;
                                });
                            if (action == hits.end())
                                throw std::runtime_error(
                                    "visible_06e_placement_action_missing");
                            // The legal preview changed the enabled action set
                            // after this frame began.  Import this rendered
                            // preview before arming its action.
                            visibleRules06EStage = 70;
                        } else if (visibleRules06EStage == 70) {
                            const auto& hits = renderer.latestProductUiResult().
                                rulesDecisionHitRects;
                            const auto action = std::find_if(hits.begin(),
                                hits.end(), [](const auto& hit) {
                                    return hit.enabled && !hit.confirm &&
                                        hit.actionId == rules::
                                            kConfirmBehindHeadStringPlacementActionId;
                                });
                            if (action == hits.end())
                                throw std::runtime_error(
                                    "visible_06e_fresh_placement_action_missing");
                            (void)clickRulesControl(*action,
                                "visible_06e_placement_arm");
                            if (runtime.rulesAuthorityRevision() !=
                                    visibleRules06EChildRevision)
                                throw std::runtime_error(
                                    "visible_06e_placement_arm_mutated_authority");
                            visibleRules06EStage = 8;
                        } else if (visibleRules06EStage == 8) {
                            requireRenderedProductFrame(
                                ui::ProductUiOverlayKind::RulesDecision,
                                "visible_06e_bih_armed");
                            const auto& hits = renderer.latestProductUiResult().
                                rulesDecisionHitRects;
                            const auto confirm = std::find_if(hits.begin(),
                                hits.end(), [](const auto& hit) {
                                    return hit.enabled && hit.confirm &&
                                        hit.actionId == rules::
                                            kConfirmBehindHeadStringPlacementActionId;
                                });
                            if (confirm == hits.end())
                                throw std::runtime_error(
                                    "visible_06e_placement_confirm_missing");
                            visibleRules06EStage = 80;
                        } else if (visibleRules06EStage == 80) {
                            const auto& hits = renderer.latestProductUiResult().
                                rulesDecisionHitRects;
                            const auto confirm = std::find_if(hits.begin(),
                                hits.end(), [](const auto& hit) {
                                    return hit.enabled && hit.confirm &&
                                        hit.actionId == rules::
                                            kConfirmBehindHeadStringPlacementActionId;
                                });
                            if (confirm == hits.end())
                                throw std::runtime_error(
                                    "visible_06e_fresh_placement_confirm_missing");
                            (void)clickRulesControl(*confirm,
                                "visible_06e_placement_confirm");
                            const auto& placed = runtime.state().balls[0].position;
                            const auto replacement =
                                runtime.ballInHandPlacementOffer();
                            if (runtime.rulesAuthorityRevision() !=
                                    visibleRules06EChildRevision + 1 ||
                                !replacement ||
                                replacement->decisionId ==
                                    visibleRules06EChildDecisionId ||
                                replacement->actorPlayerId !=
                                    visibleRules06EActor ||
                                replacement->scope != rules::
                                    BallInHandPlacementScope::BehindHeadString ||
                                !runtime.matchState().ballInHand ||
                                std::fabs(placed.x -
                                    visibleRules06EPlacement.worldX) > 0.02 ||
                                std::fabs(placed.z -
                                    visibleRules06EPlacement.worldZ) > 0.02)
                                throw std::runtime_error(
                                    "visible_06e_placement_ack_mismatch");
                            visibleRules06EStage = 9;
                        } else if (visibleRules06EStage == 9) {
                            requireRenderedProductFrame(
                                ui::ProductUiOverlayKind::GameplayHud,
                                "visible_06e_committed");
                            if (!renderer.latestProductUiResult().
                                    rulesDecisionHitRects.empty() ||
                                productSession->snapshot().
                                    rulesDecisionProductActive)
                                throw std::runtime_error(
                                    "visible_06e_committed_decision_still_visible");
                            output << "visible_rules_06e actions=3 decision="
                                << visibleRules06EDecisionId
                                << " physical_collector=cue_scratch"
                                << " fixture_setup=cue_inside_side_mouth"
                                << " projection_pan=nonzero"
                                << " modal_blocked=1 preview_cancelled=1"
                                << " authority_commits=2"
                                << " gpu_submitted=parent_offer,parent_armed,"
                                   "bih_preview,bih_armed,committed\n";
                            visibleRules06EStage = 0;
                            visibleRules06FStage = 1;
                        } else if (visibleRules06FStage == 1) {
                            // Fixture setup only: opposite side-pocket mouths let
                            // the real physics collector observe both cue scratch
                            // and ball-8 capture during one native product stroke.
                            // This is intentionally not represented as a standard
                            // full-rack break layout.
                            if (!productSession->shell().startSinglePlayer(
                                    0x06fULL).ok)
                                throw std::runtime_error(
                                    "visible_06f_match_setup_failed");
                            ai::PlayerConfiguration localPlayers;
                            if (!aiPlayer.configure(runtime, localPlayers).ok)
                                throw std::runtime_error(
                                    "visible_06f_local_actor_setup_failed");
                            const float halfWidth = runtime.physicsProfile().
                                tableBoundary.playfieldWidthCm * 0.5f;
                            const float radius =
                                runtime.physicsProfile().ball.radiusCm;
                            BallState cue = runtime.state().balls[0];
                            cue.position = {-halfWidth + radius,
                                kTableHeight + radius, 0.0f};
                            cue.velocity = {};
                            cue.angularVelocity = {};
                            cue.speed = 0.0f;
                            cue.motionState = BallMotionState::Stationary;
                            cue.pocketed = false;
                            cue.outOfPlay = false;
                            cue.pocketInteraction = {};
                            BallState eight = runtime.state().balls[8];
                            eight.position = {halfWidth - radius,
                                kTableHeight + radius, 0.0f};
                            eight.velocity = {};
                            eight.angularVelocity = {};
                            eight.speed = 0.0f;
                            eight.motionState = BallMotionState::Stationary;
                            eight.pocketed = false;
                            eight.outOfPlay = false;
                            eight.pocketInteraction = {};
                            if (!runtime.setBall(0, cue).ok ||
                                !runtime.setBall(8, eight).ok)
                                throw std::runtime_error(
                                    "visible_06f_ball_setup_failed");
                            (void)dispatchNative(nativeKeyEvent(48, @"\t"),
                                "visible_06f_enter_aim");
                            GameAction fixtureAim;
                            fixtureAim.type = ActionType::SetAimYaw;
                            fixtureAim.first = 3.14159265358979323846f;
                            if (!runtime.dispatch(fixtureAim).ok)
                                throw std::runtime_error(
                                    "visible_06f_fixture_aim_failed");
                            const NSPoint centerInView = NSMakePoint(
                                NSMidX(bounds), NSMidY(bounds));
                            const NSPoint center = [view convertPoint:
                                centerInView toView:nil];
                            (void)dispatchNative(nativeMouseEvent(
                                NSEventTypeLeftMouseDown, center),
                                "visible_06f_shoot_down");
                            (void)dispatchNative(nativeMouseEvent(
                                NSEventTypeLeftMouseUp, center),
                                "visible_06f_shoot_up");
                            if (!runtime.hasCueContactResult() ||
                                !runtime.cueContactResult().applied ||
                                !(runtime.cueContactResult().normalImpulseNs >
                                    0.0))
                                throw std::runtime_error(
                                    "visible_06f_native_cue_impulse_missing");
                            int ticks = 0;
                            while (runtime.state().ballsMoving && ticks < 480) {
                                const ActionResult stepped = runtime.step(1);
                                if (!stepped.ok)
                                    throw std::runtime_error(std::string(
                                        "visible_06f_physics_step_failed:") +
                                        stepped.errorCode);
                                productSession->consumeAuthoritySteps();
                                ++ticks;
                            }
                            previous = runtime.state();
                            accumulator = 0.0;
                            productSession->syncRulesDecisionAuthority();
                            const auto offer =
                                runtime.eightOnBreakDecisionOffer();
                            const auto decision =
                                productSession->snapshot().rulesDecision;
                            const std::vector<std::string> exactActions{
                                rules::kEightOnBreakSpotBallInHandActionId,
                                rules::kEightOnBreakRerackOpponentActionId,
                                rules::kEightOnBreakRerackOriginalActionId};
                            const bool capturedEight =
                                runtime.hasLastShotFacts() &&
                                std::find(runtime.lastShotFacts().
                                        pocketedBallsInOrder.begin(),
                                    runtime.lastShotFacts().
                                        pocketedBallsInOrder.end(), 8) !=
                                    runtime.lastShotFacts().
                                        pocketedBallsInOrder.end();
                            if (runtime.state().ballsMoving || ticks >= 480 ||
                                !runtime.rulesShotCompletionError().empty() ||
                                !runtime.hasLastShotFacts() ||
                                !runtime.lastShotFacts().cueBallPocketed ||
                                !capturedEight || !offer ||
                                offer->branch !=
                                    rules::EightOnBreakBranch::FoulBreak ||
                                offer->offeredActionIds != exactActions ||
                                decision.visibleActionIds != exactActions ||
                                !productSession->snapshot().
                                    rulesDecisionProductActive)
                                throw std::runtime_error(
                                    "visible_06f_real_offer_not_reached");
                            visibleRules06FParentRevision =
                                runtime.rulesAuthorityRevision();
                            visibleRules06FDecisionId = offer->decisionId;
                            visibleRules06FActor = offer->actorPlayerId;
                            visibleRules06FStage = 2;
                        } else if (visibleRules06FStage == 2) {
                            requireRenderedProductFrame(
                                ui::ProductUiOverlayKind::RulesDecision,
                                "visible_06f_parent_offer");
                            const auto& hits = renderer.latestProductUiResult().
                                rulesDecisionHitRects;
                            if (hits.size() != 3)
                                throw std::runtime_error(
                                    "visible_06f_three_buttons_not_rendered");
                            visibleRules06FStage = 3;
                        } else if (visibleRules06FStage == 3) {
                            const auto& hits = renderer.latestProductUiResult().
                                rulesDecisionHitRects;
                            const auto action = std::find_if(hits.begin(),
                                hits.end(), [](const auto& hit) {
                                    return hit.enabled && !hit.confirm &&
                                        hit.actionId == rules::
                                            kEightOnBreakSpotBallInHandActionId;
                                });
                            if (action == hits.end())
                                throw std::runtime_error(
                                    "visible_06f_bih_button_missing");
                            (void)clickRulesControl(*action,
                                "visible_06f_parent_arm");
                            if (runtime.rulesAuthorityRevision() !=
                                    visibleRules06FParentRevision)
                                throw std::runtime_error(
                                    "visible_06f_parent_arm_mutated_authority");
                            visibleRules06FStage = 4;
                        } else if (visibleRules06FStage == 4) {
                            requireRenderedProductFrame(
                                ui::ProductUiOverlayKind::RulesDecision,
                                "visible_06f_parent_armed");
                            const auto& hits = renderer.latestProductUiResult().
                                rulesDecisionHitRects;
                            const auto confirm = std::find_if(hits.begin(),
                                hits.end(), [](const auto& hit) {
                                    return hit.enabled && hit.confirm &&
                                        hit.actionId == rules::
                                            kEightOnBreakSpotBallInHandActionId;
                                });
                            if (confirm == hits.end())
                                throw std::runtime_error(
                                    "visible_06f_parent_confirm_missing");
                            visibleRules06FStage = 40;
                        } else if (visibleRules06FStage == 40) {
                            const auto& hits = renderer.latestProductUiResult().
                                rulesDecisionHitRects;
                            const auto confirm = std::find_if(hits.begin(),
                                hits.end(), [](const auto& hit) {
                                    return hit.enabled && hit.confirm &&
                                        hit.actionId == rules::
                                            kEightOnBreakSpotBallInHandActionId;
                                });
                            if (confirm == hits.end())
                                throw std::runtime_error(
                                    "visible_06f_fresh_parent_confirm_missing");
                            (void)clickRulesControl(*confirm,
                                "visible_06f_parent_confirm");
                            const auto placement =
                                runtime.ballInHandPlacementOffer();
                            if (runtime.rulesAuthorityRevision() !=
                                    visibleRules06FParentRevision + 1 ||
                                runtime.eightOnBreakDecisionOffer() ||
                                !placement ||
                                placement->actorPlayerId !=
                                    visibleRules06FActor ||
                                placement->scope != rules::
                                    BallInHandPlacementScope::BehindHeadString ||
                                !runtime.matchState().ballInHand)
                                throw std::runtime_error(
                                    "visible_06f_parent_ack_mismatch");
                            visibleRules06FChildRevision =
                                runtime.rulesAuthorityRevision();
                            visibleRules06FChildDecisionId =
                                placement->decisionId;
                            runtime.mutableState().camera.panX = 13.0f;
                            runtime.mutableState().camera.panZ = -7.0f;
                            visibleRules06FStage = 5;
                        } else if (visibleRules06FStage == 5) {
                            requireRenderedProductFrame(
                                ui::ProductUiOverlayKind::RulesDecision,
                                "visible_06f_child_offer");
                            PlatformInputEvent zoom;
                            zoom.kind = PlatformInputKind::Scroll;
                            zoom.scrollY = 8.0;
                            const auto zoomed = productSession->
                                handlePlatformInput(zoom);
                            if (!zoomed.ok || !zoomed.handled ||
                                runtime.rulesAuthorityRevision() !=
                                    visibleRules06FChildRevision)
                                throw std::runtime_error(
                                    "visible_06f_placement_zoom_failed");
                            visibleRules06FStage = 6;
                        } else if (visibleRules06FStage == 6) {
                            const auto& uiResult =
                                renderer.latestProductUiResult();
                            requireRenderedProductFrame(
                                ui::ProductUiOverlayKind::RulesDecision,
                                "visible_06f_child_pointer");
                            const auto& authority =
                                productSession->presentationRuntime();
                            const auto frame = buildPresentationSnapshot(
                                authority.state(), authority.state(), 1.0,
                                authority.shotControl(),
                                authority.shotControlCapabilities(),
                                productSession->presentationCameraFrame(),
                                buildRulesPresentation(authority));
                            const auto layout = productSession->shellLayout();
                            TablePlanePointerProjectionRequest request;
                            request.camera = frame.camera;
                            request.viewport = {
                                layout.viewport.logicalWidth,
                                layout.viewport.logicalHeight,
                                layout.viewport.physicalWidth,
                                layout.viewport.physicalHeight};
                            request.tablePlaneY = kTableHeight;
                            for (const auto& rect : uiResult.
                                    rulesDecisionExclusionRects)
                                request.excludedPhysicalRects.push_back(
                                    {rect.x, rect.y, rect.width, rect.height});
                            bool found = false;
                            PhysicalScreenPoint point;
                            for (int row = 1; row < 20 && !found; ++row) {
                                for (int column = 1; column < 20; ++column) {
                                    const double physicalX = layout.viewport.
                                        physicalWidth * column / 20.0;
                                    const double physicalY = layout.viewport.
                                        physicalHeight * row / 20.0;
                                    request.logicalX = physicalX * layout.
                                        viewport.logicalWidth /
                                        layout.viewport.physicalWidth;
                                    request.logicalY = physicalY * layout.
                                        viewport.logicalHeight /
                                        layout.viewport.physicalHeight;
                                    const auto projected =
                                        projectPointerToTablePlane(request);
                                    if (!projected.ok) continue;
                                    const auto validation = runtime.
                                        validateBallInHandPlacement(
                                            {projected.tablePoint.x,
                                             projected.tablePoint.z});
                                    if (!validation.ok) continue;
                                    visibleRules06FPlacement =
                                        validation.authoritativePosition;
                                    point = {physicalX, physicalY};
                                    found = true;
                                    break;
                                }
                            }
                            if (!found)
                                throw std::runtime_error(
                                    "visible_06f_legal_pointer_not_found");
                            (void)dispatchNative(nativeMouseEvent(
                                NSEventTypeMouseMoved,
                                physicalPoint(point.x, point.y)),
                                "visible_06f_placement_preview");
                            const auto preview =
                                productSession->snapshot().rulesDecision;
                            if (!preview.placementPreviewVisible ||
                                !preview.placementPreviewLegal ||
                                runtime.rulesAuthorityRevision() !=
                                    visibleRules06FChildRevision)
                                throw std::runtime_error(
                                    "visible_06f_placement_preview_invalid");
                            visibleRules06FStage = 7;
                        } else if (visibleRules06FStage == 7) {
                            requireRenderedProductFrame(
                                ui::ProductUiOverlayKind::RulesDecision,
                                "visible_06f_bih_preview");
                            const auto& hits = renderer.latestProductUiResult().
                                rulesDecisionHitRects;
                            const auto action = std::find_if(hits.begin(),
                                hits.end(), [](const auto& hit) {
                                    return hit.enabled && !hit.confirm &&
                                        hit.actionId == rules::
                                            kConfirmBehindHeadStringPlacementActionId;
                                });
                            if (action == hits.end())
                                throw std::runtime_error(
                                    "visible_06f_placement_action_missing");
                            visibleRules06FStage = 70;
                        } else if (visibleRules06FStage == 70) {
                            const auto& hits = renderer.latestProductUiResult().
                                rulesDecisionHitRects;
                            const auto action = std::find_if(hits.begin(),
                                hits.end(), [](const auto& hit) {
                                    return hit.enabled && !hit.confirm &&
                                        hit.actionId == rules::
                                            kConfirmBehindHeadStringPlacementActionId;
                                });
                            if (action == hits.end())
                                throw std::runtime_error(
                                    "visible_06f_fresh_placement_action_missing");
                            (void)clickRulesControl(*action,
                                "visible_06f_placement_arm");
                            if (runtime.rulesAuthorityRevision() !=
                                    visibleRules06FChildRevision)
                                throw std::runtime_error(
                                    "visible_06f_placement_arm_mutated_authority");
                            visibleRules06FStage = 8;
                        } else if (visibleRules06FStage == 8) {
                            requireRenderedProductFrame(
                                ui::ProductUiOverlayKind::RulesDecision,
                                "visible_06f_bih_armed");
                            visibleRules06FStage = 80;
                        } else if (visibleRules06FStage == 80) {
                            const auto& hits = renderer.latestProductUiResult().
                                rulesDecisionHitRects;
                            const auto confirm = std::find_if(hits.begin(),
                                hits.end(), [](const auto& hit) {
                                    return hit.enabled && hit.confirm &&
                                        hit.actionId == rules::
                                            kConfirmBehindHeadStringPlacementActionId;
                                });
                            if (confirm == hits.end())
                                throw std::runtime_error(
                                    "visible_06f_placement_confirm_missing");
                            (void)clickRulesControl(*confirm,
                                "visible_06f_placement_confirm");
                            const auto replacement =
                                runtime.ballInHandPlacementOffer();
                            const auto& placed =
                                runtime.state().balls[0].position;
                            if (runtime.rulesAuthorityRevision() !=
                                    visibleRules06FChildRevision + 1 ||
                                !replacement || replacement->decisionId ==
                                    visibleRules06FChildDecisionId ||
                                !runtime.matchState().ballInHand ||
                                std::fabs(placed.x - visibleRules06FPlacement.
                                    worldX) > 0.02 ||
                                std::fabs(placed.z - visibleRules06FPlacement.
                                    worldZ) > 0.02)
                                throw std::runtime_error(
                                    "visible_06f_placement_ack_mismatch");
                            visibleRules06FStage = 9;
                        } else if (visibleRules06FStage == 9) {
                            requireRenderedProductFrame(
                                ui::ProductUiOverlayKind::GameplayHud,
                                "visible_06f_committed");
                            if (!renderer.latestProductUiResult().
                                    rulesDecisionHitRects.empty() ||
                                productSession->snapshot().
                                    rulesDecisionProductActive)
                                throw std::runtime_error(
                                    "visible_06f_committed_decision_still_visible");
                            output << "visible_rules_06f actions=3 decision="
                                << visibleRules06FDecisionId
                                << " physical_collector=cue_scratch+eight_capture"
                                << " fixture_setup=opposite_side_mouths"
                                << " authority_commits=2"
                                << " gpu_submitted=parent_offer,parent_armed,"
                                   "bih_preview,bih_armed,committed\n";
                            visibleRules06FStage = 0;
                            visibleRulesGroupStage = 1;
                        } else if (visibleRulesGroupStage == 1) {
                            // The offer fixture freezes the rare simultaneous
                            // first-contact/pocket rule condition.  All product
                            // interaction below is real Cocoa input and exact
                            // runtime authority; this setup is not represented
                            // as a naturally played stroke.
                            if (!productSession->shell().startSinglePlayer(
                                    0x104ULL).ok)
                                throw std::runtime_error(
                                    "visible_group_match_setup_failed");
                            ai::PlayerConfiguration localPlayers;
                            if (!aiPlayer.configure(runtime, localPlayers).ok)
                                throw std::runtime_error(
                                    "visible_group_local_actor_setup_failed");
                            rules::ShotFacts breakShot;
                            breakShot.shooter = 0;
                            breakShot.breakShot = true;
                            breakShot.firstContactBall = 1;
                            breakShot.firstContactBalls = {1};
                            breakShot.anyRailAfterFirstContact = true;
                            breakShot.distinctObjectBallsToRail = 4;
                            if (!runtime.adjudicateRulesShot(breakShot).ok)
                                throw std::runtime_error(
                                    "visible_group_break_setup_failed");
                            rules::ShotFacts choice;
                            choice.shooter = runtime.matchState().currentPlayer;
                            choice.firstContactBall = 1;
                            choice.firstContactBalls = {1, 9};
                            choice.anyRailAfterFirstContact = true;
                            choice.pocketedBallsInOrder = {1, 9};
                            if (!runtime.adjudicateRulesShot(choice).ok)
                                throw std::runtime_error(
                                    "visible_group_offer_setup_failed");
                            productSession->syncRulesDecisionAuthority();
                            const auto offer =
                                runtime.groupChoiceDecisionOffer();
                            const auto decision =
                                productSession->snapshot().rulesDecision;
                            const std::array<std::string, 2> exactActions{
                                rules::kGroupChoiceSolidsActionId,
                                rules::kGroupChoiceStripesActionId};
                            const std::vector<std::string> visibleActions(
                                exactActions.begin(), exactActions.end());
                            if (!offer ||
                                offer->offeredActionIds != exactActions ||
                                decision.visibleActionIds != visibleActions ||
                                !productSession->snapshot().
                                    rulesDecisionGroupChoiceProductEnabled ||
                                !productSession->snapshot().
                                    rulesDecisionProductActive)
                                throw std::runtime_error(
                                    "visible_group_offer_not_reached");
                            visibleRulesGroupDecisionId = offer->decisionId;
                            visibleRulesGroupRevision =
                                runtime.rulesAuthorityRevision();
                            previous = runtime.state();
                            accumulator = 0.0;
                            visibleRulesGroupStage = 2;
                        } else if (visibleRulesGroupStage == 2) {
                            requireRenderedProductFrame(
                                ui::ProductUiOverlayKind::RulesDecision,
                                "visible_group_offer");
                            // The rendered hit map is imported at the next
                            // loop boundary.  Do not click it in the frame
                            // that first proves the offer was rendered.
                            visibleRulesGroupStage = 20;
                        } else if (visibleRulesGroupStage == 20) {
                            const auto& hits = renderer.latestProductUiResult().
                                rulesDecisionHitRects;
                            const auto action = std::find_if(hits.begin(),
                                hits.end(), [](const auto& hit) {
                                    return hit.enabled && !hit.confirm &&
                                        hit.actionId == rules::
                                            kGroupChoiceStripesActionId;
                                });
                            if (action == hits.end())
                                throw std::runtime_error(
                                    "visible_group_action_missing");
                            const auto armed = clickRulesControl(*action,
                                "visible_group_arm");
                            if (!armed.ok || runtime.rulesAuthorityRevision() !=
                                    visibleRulesGroupRevision ||
                                productSession->snapshot().rulesDecision.
                                    armedActionId != rules::
                                        kGroupChoiceStripesActionId)
                                throw std::runtime_error(
                                    "visible_group_arm_mutated_authority");
                            visibleRulesGroupStage = 3;
                        } else if (visibleRulesGroupStage == 3) {
                            requireRenderedProductFrame(
                                ui::ProductUiOverlayKind::RulesDecision,
                                "visible_group_armed");
                            const auto& hits = renderer.latestProductUiResult().
                                rulesDecisionHitRects;
                            const auto confirm = std::find_if(hits.begin(),
                                hits.end(), [](const auto& hit) {
                                    return hit.enabled && hit.confirm &&
                                        hit.actionId == rules::
                                            kGroupChoiceStripesActionId;
                                });
                            if (confirm == hits.end())
                                throw std::runtime_error(
                                    "visible_group_confirm_missing");
                            // Import the freshly rendered Confirm rectangle
                            // before dispatching its Cocoa click.
                            visibleRulesGroupStage = 30;
                        } else if (visibleRulesGroupStage == 30) {
                            const auto& hits = renderer.latestProductUiResult().
                                rulesDecisionHitRects;
                            const auto confirm = std::find_if(hits.begin(),
                                hits.end(), [](const auto& hit) {
                                    return hit.enabled && hit.confirm &&
                                        hit.actionId == rules::
                                            kGroupChoiceStripesActionId;
                                });
                            if (confirm == hits.end())
                                throw std::runtime_error(
                                    "visible_group_fresh_confirm_missing");
                            const auto committed = clickRulesControl(*confirm,
                                "visible_group_confirm");
                            if (!committed.ok || runtime.rulesAuthorityRevision() !=
                                    visibleRulesGroupRevision + 1 ||
                                runtime.groupChoiceDecisionOffer() ||
                                runtime.rulesAuthorityActions().empty() ||
                                runtime.rulesAuthorityActions().back().
                                    actionId !=
                                    rules::kGroupChoiceStripesActionId)
                                throw std::runtime_error(
                                    "visible_group_ack_mismatch");
                            visibleRulesGroupStage = 4;
                        } else if (visibleRulesGroupStage == 4) {
                            requireRenderedProductFrame(
                                ui::ProductUiOverlayKind::GameplayHud,
                                "visible_group_committed");
                            if (!renderer.latestProductUiResult().
                                    rulesDecisionHitRects.empty() ||
                                productSession->snapshot().
                                    rulesDecisionProductActive)
                                throw std::runtime_error(
                                    "visible_group_decision_still_visible");
                            output << "visible_rules_group_choice actions=2 decision="
                                << visibleRulesGroupDecisionId
                                << " fixture=simultaneous-contact-rule-facts"
                                << " cocoa_arm_confirm=1 authority_commits=1"
                                << " gpu_submitted=offer,armed,committed\n";
                            visibleRulesGroupStage = 0;
                            visibleRulesFullTableStage = 1;
                        } else if (visibleRulesFullTableStage == 1) {
                            // Establish a normal post-break match state, then
                            // use a real native stroke into the side mouth.
                            // The ordinary FullTable BIH offer must come from
                            // succeeded physics, collector, and adjudication.
                            if (!productSession->shell().startSinglePlayer(
                                    0x809ULL).ok)
                                throw std::runtime_error(
                                    "visible_full_bih_match_setup_failed");
                            ai::PlayerConfiguration localPlayers;
                            if (!aiPlayer.configure(runtime, localPlayers).ok)
                                throw std::runtime_error(
                                    "visible_full_bih_local_actor_setup_failed");
                            rules::ShotFacts breakShot;
                            breakShot.shooter = 0;
                            breakShot.breakShot = true;
                            breakShot.firstContactBall = 1;
                            breakShot.firstContactBalls = {1};
                            breakShot.anyRailAfterFirstContact = true;
                            breakShot.distinctObjectBallsToRail = 4;
                            if (!runtime.adjudicateRulesShot(breakShot).ok)
                                throw std::runtime_error(
                                    "visible_full_bih_break_setup_failed");
                            rules::ShotFacts assignment;
                            assignment.shooter =
                                runtime.matchState().currentPlayer;
                            assignment.firstContactBall = 1;
                            assignment.firstContactBalls = {1};
                            assignment.anyRailAfterFirstContact = true;
                            assignment.pocketedBallsInOrder = {1};
                            if (!runtime.adjudicateRulesShot(assignment).ok ||
                                runtime.matchState().phase !=
                                    rules::MatchPhase::GroupsAssigned)
                                throw std::runtime_error(
                                    "visible_full_bih_group_setup_failed");
                            const float halfWidth = runtime.physicsProfile().
                                tableBoundary.playfieldWidthCm * 0.5f;
                            const float radius =
                                runtime.physicsProfile().ball.radiusCm;
                            BallState cue = runtime.state().balls[0];
                            cue.position = {-halfWidth + radius,
                                kTableHeight + radius, 0.0f};
                            cue.velocity = {};
                            cue.angularVelocity = {};
                            cue.speed = 0.0f;
                            cue.motionState = BallMotionState::Stationary;
                            cue.pocketed = false;
                            cue.outOfPlay = false;
                            cue.pocketInteraction = {};
                            if (!runtime.setBall(0, cue).ok)
                                throw std::runtime_error(
                                    "visible_full_bih_cue_setup_failed");
                            (void)dispatchNative(nativeKeyEvent(48, @"\t"),
                                "visible_full_bih_enter_aim");
                            GameAction fixtureAim;
                            fixtureAim.type = ActionType::SetAimYaw;
                            fixtureAim.first = 3.14159265358979323846f;
                            if (!runtime.dispatch(fixtureAim).ok)
                                throw std::runtime_error(
                                    "visible_full_bih_fixture_aim_failed");
                            const NSPoint centerInView = NSMakePoint(
                                NSMidX(bounds), NSMidY(bounds));
                            const NSPoint center = [view convertPoint:
                                centerInView toView:nil];
                            (void)dispatchNative(nativeMouseEvent(
                                NSEventTypeLeftMouseDown, center),
                                "visible_full_bih_shoot_down");
                            (void)dispatchNative(nativeMouseEvent(
                                NSEventTypeLeftMouseUp, center),
                                "visible_full_bih_shoot_up");
                            if (!runtime.hasCueContactResult() ||
                                !runtime.cueContactResult().applied ||
                                !(runtime.cueContactResult().normalImpulseNs >
                                    0.0))
                                throw std::runtime_error(
                                    "visible_full_bih_native_cue_impulse_missing");
                            int ticks = 0;
                            while (runtime.state().ballsMoving && ticks < 240) {
                                const ActionResult stepped = runtime.step(1);
                                if (!stepped.ok)
                                    throw std::runtime_error(std::string(
                                        "visible_full_bih_physics_step_failed:") +
                                        stepped.errorCode);
                                productSession->consumeAuthoritySteps();
                                ++ticks;
                            }
                            previous = runtime.state();
                            accumulator = 0.0;
                            productSession->syncRulesDecisionAuthority();
                            const auto offer =
                                runtime.ballInHandPlacementOffer();
                            bool cueScratch = false;
                            if (runtime.hasLastAdjudication()) {
                                const auto& reasons =
                                    runtime.lastAdjudication().reasons;
                                cueScratch = std::find(reasons.begin(),
                                    reasons.end(),
                                    rules::FoulReason::CueBallScratch) !=
                                    reasons.end();
                            }
                            const bool canonicalScratch =
                                runtime.hasLastShotFacts() &&
                                !runtime.lastShotFacts().breakShot &&
                                runtime.lastShotFacts().cueBallPocketed &&
                                runtime.lastShotFacts().
                                    cueBallScratchEvidenceOrigin ==
                                    rules::CueBallScratchEvidenceOrigin::
                                        AuthoritativeShotFactCollector &&
                                runtime.lastShotFacts().
                                    cueBallScratchEvidenceComplete;
                            if (runtime.state().ballsMoving || ticks >= 240 ||
                                !canonicalScratch || !cueScratch || !offer ||
                                offer->scope !=
                                    rules::BallInHandPlacementScope::FullTable ||
                                offer->offeredActionIds.size() != 1 ||
                                offer->offeredActionIds[0] !=
                                    rules::kConfirmFullTablePlacementActionId ||
                                !productSession->snapshot().
                                    rulesDecisionFullTableBallInHandProductEnabled ||
                                !productSession->snapshot().
                                    rulesDecisionProductActive)
                                throw std::runtime_error(
                                    "visible_full_bih_real_offer_not_reached");
                            visibleRulesFullTableDecisionId = offer->decisionId;
                            visibleRulesFullTableActor = offer->actorPlayerId;
                            visibleRulesFullTableRevision =
                                runtime.rulesAuthorityRevision();
                            visibleRulesFullTableStage = 2;
                        } else if (visibleRulesFullTableStage == 2) {
                            requireRenderedProductFrame(
                                ui::ProductUiOverlayKind::RulesDecision,
                                "visible_full_bih_offer");
                            const auto& exclusions =
                                renderer.latestProductUiResult().
                                    rulesDecisionExclusionRects;
                            if (exclusions.empty())
                                throw std::runtime_error(
                                    "visible_full_bih_modal_exclusion_missing");
                            bool found = false;
                            for (double y = 80.0; y < framebufferHeight - 40.0 &&
                                    !found; y += 12.0) {
                                for (double x = 40.0; x < framebufferWidth - 40.0;
                                        x += 12.0) {
                                    const auto translated = productPlatformEvent(
                                        nativeMouseEvent(NSEventTypeMouseMoved,
                                            physicalPoint(x, y)), view,
                                        nativeInput.pointer, *productSession);
                                    if (!translated) continue;
                                    const auto moved =
                                        productSession->handlePlatformInput(
                                            *translated);
                                    const auto decision =
                                        productSession->snapshot().rulesDecision;
                                    if (moved.ok &&
                                        decision.placementPreviewVisible &&
                                        decision.placementPreviewLegal) {
                                        visibleRulesFullTablePlacement =
                                            decision.placementPreview;
                                        found = true;
                                        break;
                                    }
                                }
                            }
                            if (!found || runtime.rulesAuthorityRevision() !=
                                    visibleRulesFullTableRevision)
                                throw std::runtime_error(
                                    "visible_full_bih_legal_pointer_not_found");
                            visibleRulesFullTableStage = 3;
                        } else if (visibleRulesFullTableStage == 3) {
                            requireRenderedProductFrame(
                                ui::ProductUiOverlayKind::RulesDecision,
                                "visible_full_bih_preview");
                            // Import the preview frame's placement control
                            // before using its hit rectangle.
                            visibleRulesFullTableStage = 30;
                        } else if (visibleRulesFullTableStage == 30) {
                            const auto& hits = renderer.latestProductUiResult().
                                rulesDecisionHitRects;
                            const auto action = std::find_if(hits.begin(),
                                hits.end(), [](const auto& hit) {
                                    return hit.enabled && !hit.confirm &&
                                        hit.actionId == rules::
                                            kConfirmFullTablePlacementActionId;
                                });
                            if (action == hits.end())
                                throw std::runtime_error(
                                    "visible_full_bih_action_missing");
                            const auto armed = clickRulesControl(*action,
                                "visible_full_bih_arm");
                            if (!armed.ok || runtime.rulesAuthorityRevision() !=
                                    visibleRulesFullTableRevision ||
                                productSession->snapshot().rulesDecision.
                                    armedActionId != rules::
                                        kConfirmFullTablePlacementActionId)
                                throw std::runtime_error(
                                    "visible_full_bih_arm_mutated_authority");
                            visibleRulesFullTableStage = 4;
                        } else if (visibleRulesFullTableStage == 4) {
                            requireRenderedProductFrame(
                                ui::ProductUiOverlayKind::RulesDecision,
                                "visible_full_bih_armed");
                            const auto& hits = renderer.latestProductUiResult().
                                rulesDecisionHitRects;
                            const auto confirm = std::find_if(hits.begin(),
                                hits.end(), [](const auto& hit) {
                                    return hit.enabled && hit.confirm &&
                                        hit.actionId == rules::
                                            kConfirmFullTablePlacementActionId;
                                });
                            if (confirm == hits.end())
                                throw std::runtime_error(
                                    "visible_full_bih_confirm_missing");
                            // Import the armed frame's Confirm rectangle
                            // before committing the placement.
                            visibleRulesFullTableStage = 40;
                        } else if (visibleRulesFullTableStage == 40) {
                            const auto& hits = renderer.latestProductUiResult().
                                rulesDecisionHitRects;
                            const auto confirm = std::find_if(hits.begin(),
                                hits.end(), [](const auto& hit) {
                                    return hit.enabled && hit.confirm &&
                                        hit.actionId == rules::
                                            kConfirmFullTablePlacementActionId;
                                });
                            if (confirm == hits.end())
                                throw std::runtime_error(
                                    "visible_full_bih_fresh_confirm_missing");
                            const auto committed = clickRulesControl(*confirm,
                                "visible_full_bih_confirm");
                            const auto replacement =
                                runtime.ballInHandPlacementOffer();
                            const auto& placed =
                                runtime.state().balls[0].position;
                            if (!committed.ok || runtime.rulesAuthorityRevision() !=
                                    visibleRulesFullTableRevision + 1 ||
                                !replacement || replacement->decisionId ==
                                    visibleRulesFullTableDecisionId ||
                                replacement->actorPlayerId !=
                                    visibleRulesFullTableActor ||
                                replacement->scope !=
                                    rules::BallInHandPlacementScope::FullTable ||
                                std::fabs(placed.x -
                                    visibleRulesFullTablePlacement.worldX) > 0.02 ||
                                std::fabs(placed.z -
                                    visibleRulesFullTablePlacement.worldZ) > 0.02)
                                throw std::runtime_error(
                                    "visible_full_bih_ack_mismatch");
                            visibleRulesFullTableStage = 5;
                        } else if (visibleRulesFullTableStage == 5) {
                            requireRenderedProductFrame(
                                ui::ProductUiOverlayKind::GameplayHud,
                                "visible_full_bih_committed");
                            if (!renderer.latestProductUiResult().
                                    rulesDecisionHitRects.empty() ||
                                productSession->snapshot().
                                    rulesDecisionProductActive)
                                throw std::runtime_error(
                                    "visible_full_bih_decision_still_visible");
                            output << "visible_rules_full_table_bih decision="
                                << visibleRulesFullTableDecisionId
                                << " physical_collector=ordinary_cue_scratch"
                                << " pointer_preview=actual_camera"
                                << " authority_commits=1"
                                << " gpu_submitted=offer,preview,armed,committed\n";
                            [platform.window close];
                        }
                    } else if (verifiedVisibleFrames >= 17) {
                        if (productSession->snapshot().shell.replay.tick != verifiedPausedReplayTick)
                            if (verifiedVisibleFrames == 17)
                                throw std::runtime_error("visible_replay_advanced_while_paused");
                        const auto clickVisible = [&](const std::string& id) {
                            syncInputViewport();
                            const auto layout = productSession->shellLayout();
                            product_hud::HudRect rect;
                            if (id == "inspector_toggle") rect = layout.inspectorToggleRect;
                            else {
                                const auto found = std::find_if(layout.elements.begin(), layout.elements.end(),
                                    [&](const auto& item) { return item.id == id; });
                                if (found == layout.elements.end())
                                    throw std::runtime_error("visible_training_button_missing:" + id);
                                rect = found->rect;
                            }
                            PlatformInputEvent event;
                            event.control = "LeftButton";
                            event.logicalX = (rect.x + rect.width * 0.5) * layout.viewport.logicalWidth / layout.viewport.physicalWidth;
                            event.logicalY = (rect.y + rect.height * 0.5) * layout.viewport.logicalHeight / layout.viewport.physicalHeight;
                            event.kind = PlatformInputKind::PointerButtonDown;
                            (void)productSession->handlePlatformInput(event);
                            syncInputViewport();
                            event.kind = PlatformInputKind::PointerButtonUp;
                            if (!productSession->handlePlatformInput(event).ok)
                                throw std::runtime_error("visible_training_click_failed:" + id);
                        };
                        const auto shootTutorial = [&](bool requireComplete = true) {
                            if (runtime.state().aim.mode != AimMode::Aim) {
                                PlatformInputEvent tab;
                                tab.kind = PlatformInputKind::KeyDown;
                                tab.control = "Tab";
                                if (!productSession->handlePlatformInput(tab).ok)
                                    throw std::runtime_error("visible_training_aim_failed");
                            }
                            const auto hints = productSession->training().snapshot().teachingHints;
                            const auto& control = runtime.shotControl();
                            if (!hints.available || std::fabs(control.yawRadians - hints.aimYawRadians) > 1e-4f ||
                                std::fabs(control.requestedPower - hints.defaultPowerPercent * 2.0f) > 1e-4f ||
                                std::fabs(control.tipOffsetX - hints.tipXBallRadii) > 1e-4f ||
                                std::fabs(control.tipOffsetY - hints.tipYBallRadii) > 1e-4f)
                                throw std::runtime_error("visible_training_defaults_not_applied");
                            PlatformInputEvent shoot;
                            shoot.kind = PlatformInputKind::PointerButtonUp;
                            shoot.control = "LeftButton";
                            if (!productSession->handlePlatformInput(shoot).ok)
                                throw std::runtime_error("visible_training_shoot_failed");
                            for (int tick = 0; tick < 2400 && runtime.state().ballsMoving; ++tick) {
                                if (!runtime.step(1).ok) throw std::runtime_error("visible_training_step_failed");
                                productSession->consumeAuthoritySteps();
                            }
                            const auto evaluation = productSession->training().snapshot().evaluation;
                            if (runtime.state().ballsMoving || evaluation.failed ||
                                (requireComplete && !evaluation.succeeded))
                                throw std::runtime_error("visible_training_tutorial_not_passed");
                        };
                        if (verifiedVisibleFrames == 17) {
                            (void)productSession->training().abandonAttempt();
                            const auto manifest = std::string(BILLIARDGL_SOURCE_ROOT) +
                                "/assets/training/phase4/training_pack_v2.json";
                            if (!productSession->shell().loadTrainingCatalog(manifest,
                                    ModernProductSessionOptions{}.trainingCapabilities).ok ||
                                !productSession->shell().startTrainingScenario(
                                    "launch_straight_pot_intro_v2", "visible-training-v2", 0).ok)
                                throw std::runtime_error("visible_training_v2_setup_failed");
                        } else if (verifiedVisibleFrames == 18) {
                            clickVisible("inspector_toggle");
                            if (productSession->shellLayout().inspectorCollapsed)
                                throw std::runtime_error("visible_training_panel_did_not_open");
                        } else if (verifiedVisibleFrames == 19) {
                            clickVisible("inspector_toggle");
                            shootTutorial();
                        } else if (verifiedVisibleFrames == 20) {
                            clickVisible("next_training");
                            if (productSession->training().snapshot().scenarioId != "launch_straight_stop_intro_v2")
                                throw std::runtime_error("visible_training_next_stage_failed");
                            if (productSession->snapshot().shell.completedTrainingCount != 1)
                                throw std::runtime_error("visible_training_first_completion_lost");
                        } else if (verifiedVisibleFrames == 21) {
                            shootTutorial();
                        } else if (verifiedVisibleFrames == 22) {
                            clickVisible("retry_training");
                            if (productSession->training().snapshot().attemptFacts.shotCount != 0)
                                throw std::runtime_error("visible_training_retry_failed");
                            if (productSession->snapshot().shell.completedTrainingCount != 2)
                                throw std::runtime_error("visible_training_completion_lost_on_retry");
                        } else if (verifiedVisibleFrames == 23) {
                            shootTutorial();
                        } else if (verifiedVisibleFrames == 24) {
                            clickVisible("next_training");
                            const auto hints = productSession->training().snapshot().teachingHints;
                            if (!hints.cueBallMustAvoidRails || hints.cueBallTargetTracksFirstContact)
                                throw std::runtime_error("visible_follow_teaching_contract_missing");
                        } else if (verifiedVisibleFrames == 25) {
                            const auto before = productSession->training().snapshot().teachingHints;
                            shootTutorial();
                            const auto after = productSession->training().snapshot();
                            if (after.attemptFacts.railContacts[0] != 0 ||
                                after.teachingHints.cueBallTargetCenterCm != before.cueBallTargetCenterCm)
                                throw std::runtime_error("visible_follow_rail_or_target_moved");
                        } else if (verifiedVisibleFrames == 26) {
                            clickVisible("next_training");
                            const auto snapshot = productSession->training().snapshot();
                            const auto hints = snapshot.teachingHints;
                            if (snapshot.scenarioId != "launch_straight_draw_intro_v2" ||
                                !hints.cueBallMustAvoidRails || hints.cueBallTargetTracksFirstContact ||
                                hints.tipYBallRadii >= 0.0f)
                                throw std::runtime_error("visible_draw_teaching_contract_missing");
                        } else if (verifiedVisibleFrames == 27) {
                            const auto before = productSession->training().snapshot().teachingHints;
                            shootTutorial();
                            const auto after = productSession->training().snapshot();
                            const auto& facts = after.attemptFacts;
                            const auto& end = runtime.state().balls[0].position;
                            const auto& contact = facts.cueBallPositionAtFirstContactCm;
                            const float forward = (end.x - contact.x) * std::cos(before.aimYawRadians) +
                                (end.z - contact.z) * std::sin(before.aimYawRadians);
                            if (!facts.hasCueBallPositionAtFirstContact || forward >= -1.0f ||
                                facts.railContacts[0] != 0 ||
                                after.teachingHints.cueBallTargetCenterCm != before.cueBallTargetCenterCm)
                                throw std::runtime_error("visible_draw_not_backward_or_rail_or_target_moved");
                        } else if (verifiedVisibleFrames >= 28) {
                            const auto before = productSession->training().snapshot();
                            const auto& cards = productSession->training().scenarios();
                            if (before.evaluation.succeeded) {
                                if (before.scenarioId == cards.back().id) {
                                    visibleRules06CStage = 1;
                                } else {
                                    clickVisible("next_training");
                                    const auto next = productSession->training().snapshot();
                                    if (!next.teachingHints.available || next.teachingHints.goalEn.empty() ||
                                        next.scenarioId.size() < 3 || next.scenarioId.substr(next.scenarioId.size() - 3) != "_v2")
                                        throw std::runtime_error("visible_remaining_legacy_or_missing_hints");
                                }
                            } else {
                                if (before.teachingStepCount > 1 && before.attemptFacts.shotCount > 0)
                                    clickVisible("prepare_training_shot");
                                shootTutorial(before.teachingStepIndex + 1 >= before.teachingStepCount);
                            }
                            if (verifiedVisibleFrames > 64)
                                throw std::runtime_error("visible_training_catalog_not_completed");
                        }
                    } else if (verifiedVisibleFrames < 8) {
                        GameAction drag;
                        drag.type = ActionType::OrbitCamera;
                        drag.first = verifiedVisibleFrames % 2 ? -0.04f : 0.04f;
                        (void)runtime.dispatch(drag);
                    }
                }
#endif
                std::this_thread::sleep_for(std::chrono::milliseconds(1));
            }
            if (playtestLog) playtestLog->record("session_end", runtime, logContext());
            diagnosticExit.ended = true;
            renderer.shutdown();
            return 0;
        } catch (const std::exception& error) {
            errors << "ModernApplication: " << error.what() << '\n';
            return 2;
        }
    }
}

ModernBenchmarkResult runModernRendererBenchmark(
    const ModernApplicationOptions& options, int warmupFrames, int measuredFrames)
{
    @autoreleasepool {
        ContextWindow platform(options);
        ModernRenderer renderer(rendererOptions(options));
        std::string error;
        if (!renderer.initialize({options.width, options.height, true}, error)) throw std::runtime_error(error);
        GameRuntime runtime;
        const auto snapshot = buildPresentationSnapshot(runtime.state(), runtime.state(), 1.0,
            runtime.shotControl(), runtime.shotControlCapabilities(), runtime.cameraFrame(),
            buildRulesPresentation(runtime));
        std::map<std::string, std::vector<double>> cpu, gpu;
        for (int frame = 0; frame < warmupFrames + measuredFrames; ++frame) {
            renderer.render(snapshot);
            glFlush();
            if (frame >= warmupFrames) {
                for (const auto& pass : renderer.metrics().passes) {
                    cpu[pass.name].push_back(pass.cpuMilliseconds);
                    if (pass.gpuTimingAvailable) gpu[pass.name].push_back(pass.gpuMilliseconds);
                }
            }
        }
        ModernBenchmarkResult result; result.warmupFrames=warmupFrames;result.measuredFrames=measuredFrames;
        for (auto& entry : cpu) result.cpuPassMilliseconds[entry.first]=percentiles(std::move(entry.second));
        for (auto& entry : gpu) result.gpuPassMilliseconds[entry.first]=percentiles(std::move(entry.second));
        renderer.shutdown();
        return result;
    }
}

}  // namespace billiardgl::render
