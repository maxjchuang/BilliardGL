#include "render/legacy/legacy_renderer.h"

#include "render_resources.h"

#include <cstdlib>
#include <iostream>
#include <stdexcept>
#include <string>

namespace {

void expect(bool condition, const char* message)
{
    if (!condition) {
        std::cerr << message << '\n';
        std::exit(EXIT_FAILURE);
    }
}

struct LifecycleProbe {
    billiardgl::render::LegacyRendererOperations operations()
    {
        billiardgl::render::LegacyRendererOperations result;
        result.initializeResources = [this](
            billiardgl::RenderResources&,
            const billiardgl::render::RendererInitialization&,
            std::string& error) {
            ++initializeCalls;
            if (throwDuringInitialize) throw std::runtime_error("injected init failure");
            if (!initializeResult) error = initializeError;
            return initializeResult;
        };
        result.shutdownResources = [this](billiardgl::RenderResources&) {
            ++shutdownCalls;
        };
        result.resizeViewport = [this](int width, int height) {
            ++resizeCalls;
            lastWidth = width;
            lastHeight = height;
        };
        result.setupCamera = [](const billiardgl::CameraPresentation&) {};
        result.renderScene = [](
            const billiardgl::PresentationSnapshot&,
            billiardgl::RenderResources&) {};
        result.drawHud = [](const billiardgl::HudViewModel&) {};
        result.captureOffscreen = [this](
            const std::string&, int width, int height,
            const std::function<void()>& renderFrame) {
            ++captureCalls;
            lastWidth = width;
            lastHeight = height;
            renderFrame();
            return captureResult;
        };
        return result;
    }

    std::string initializeError;
    int initializeCalls = 0;
    int shutdownCalls = 0;
    int resizeCalls = 0;
    int captureCalls = 0;
    int lastWidth = 0;
    int lastHeight = 0;
    bool initializeResult = true;
    bool throwDuringInitialize = false;
    bool captureResult = true;
};

}  // namespace

int main()
{
    using namespace billiardgl;
    using namespace billiardgl::render;

    RenderResources resources;
    RendererInitialization initialization;
    initialization.framebufferWidth = 1280;
    initialization.framebufferHeight = 720;
    std::string error;

    LifecycleProbe invalidDimensionProbe;
    LegacyRenderer invalidDimensions(resources, invalidDimensionProbe.operations());
    RendererInitialization invalidInitialization = initialization;
    invalidInitialization.framebufferWidth = 0;
    expect(!invalidDimensions.initialize(invalidInitialization, error) &&
        error == "LegacyRenderer requires positive framebuffer dimensions",
        "invalid initialization dimensions fail before resource work");
    expect(invalidDimensionProbe.initializeCalls == 0,
        "invalid dimensions do not invoke resource initialization");

    LegacyRendererOperations incompleteOperations;
    LegacyRenderer incomplete(resources, incompleteOperations);
    expect(!incomplete.initialize(initialization, error) &&
        error == "LegacyRenderer operations are incomplete",
        "incomplete injected operations fail explicitly");

    LifecycleProbe failedProbe;
    failedProbe.initializeResult = false;
    failedProbe.initializeError = "injected resource rejection";
    LegacyRenderer failed(resources, failedProbe.operations());
    expect(!failed.initialize(initialization, error) &&
        error == "injected resource rejection",
        "resource initialization error crosses the backend boundary");
    expect(failedProbe.resizeCalls == 0 && failedProbe.shutdownCalls == 0,
        "rejected resource initialization performs no viewport or duplicate cleanup");

    LifecycleProbe throwingProbe;
    throwingProbe.throwDuringInitialize = true;
    LegacyRenderer throwing(resources, throwingProbe.operations());
    expect(!throwing.initialize(initialization, error) &&
        error == "LegacyRenderer initialization failed: injected init failure",
        "initialization exceptions become stable errors");
    expect(throwingProbe.shutdownCalls == 1,
        "exceptional partial initialization invokes lifecycle cleanup");

    LifecycleProbe probe;
    LegacyRenderer renderer(resources, probe.operations());
    PresentationSnapshot snapshot;
    RenderCaptureRequest capture;
    capture.outputPath = "frame.ppm";
    expect(!renderer.capture(capture, error) &&
        error == "LegacyRenderer capture before initialize",
        "capture before initialize fails without calling capture operation");
    try {
        renderer.resize(640, 480);
        expect(false, "resize before initialize must throw");
    } catch (const std::runtime_error&) {
    }
    try {
        renderer.render(snapshot);
        expect(false, "render before initialize must throw");
    } catch (const std::runtime_error&) {
    }

    expect(renderer.initialize(initialization, error),
        "valid injected lifecycle initializes without GL");
    expect(probe.initializeCalls == 1 && probe.resizeCalls == 1 &&
        probe.lastWidth == 1280 && probe.lastHeight == 720,
        "initialize establishes the requested viewport once");
    expect(!renderer.initialize(initialization, error) &&
        error == "LegacyRenderer is already initialized",
        "double initialization fails explicitly");
    expect(!renderer.capture(capture, error) &&
        error == "LegacyRenderer capture requires a rendered snapshot",
        "capture requires a prior presentation snapshot");

    renderer.render(snapshot);
    expect(renderer.capture(capture, error) && probe.captureCalls == 1 &&
        probe.lastWidth == 1280 && probe.lastHeight == 720,
        "capture defaults to current framebuffer dimensions");
    probe.captureResult = false;
    expect(!renderer.capture(capture, error) &&
        error == "LegacyRenderer capture failed: frame.ppm",
        "capture operation failure crosses the backend boundary");

    renderer.shutdown();
    renderer.shutdown();
    expect(probe.shutdownCalls == 1,
        "shutdown is idempotent and releases the injected lifecycle once");
    return 0;
}
