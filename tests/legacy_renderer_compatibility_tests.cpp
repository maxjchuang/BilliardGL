#include "render/legacy/legacy_renderer.h"

#include "render_resources.h"

#include <cstdlib>
#include <iostream>
#include <stdexcept>
#include <string>
#include <vector>

namespace {

void expect(bool condition, const char* message)
{
    if (!condition) {
        std::cerr << message << '\n';
        std::exit(EXIT_FAILURE);
    }
}

struct RecordingOperations {
    billiardgl::render::LegacyRendererOperations operations()
    {
        billiardgl::render::LegacyRendererOperations result;
        result.initializeResources = [this](
            billiardgl::RenderResources& resources,
            const billiardgl::render::RendererInitialization& initialization,
            std::string&) {
            calls.push_back("initialize");
            initializedResources = &resources;
            initializedWidth = initialization.framebufferWidth;
            initializedHeight = initialization.framebufferHeight;
            return initializeResult;
        };
        result.shutdownResources = [this](billiardgl::RenderResources& resources) {
            calls.push_back("shutdown");
            ++shutdownCalls;
            shutdownResources = &resources;
        };
        result.resizeViewport = [this](int width, int height) {
            calls.push_back("resize");
            resizedWidth = width;
            resizedHeight = height;
        };
        result.setupCamera = [this](const billiardgl::CameraPresentation& camera) {
            calls.push_back("camera");
            cameraInput = &camera;
            cameraEyeX = camera.eye.x;
        };
        result.renderScene = [this](
            const billiardgl::PresentationSnapshot& snapshot,
            billiardgl::RenderResources& resources) {
            calls.push_back("scene");
            sceneInput = &snapshot;
            sceneResources = &resources;
            ballX = snapshot.balls[3].position.x;
            cueRotation = snapshot.cue.rotationDegrees;
        };
        result.drawHud = [this](const billiardgl::HudViewModel& hud) {
            calls.push_back("hud");
            hudInput = &hud;
            hudPower = hud.shotPower;
        };
        result.captureOffscreen = [this](
            const std::string& path, int width, int height,
            const std::function<void()>& renderFrame) {
            calls.push_back("capture");
            capturePath = path;
            captureWidth = width;
            captureHeight = height;
            renderFrame();
            return captureResult;
        };
        return result;
    }

    std::vector<std::string> calls;
    billiardgl::RenderResources* initializedResources = nullptr;
    billiardgl::RenderResources* shutdownResources = nullptr;
    billiardgl::RenderResources* sceneResources = nullptr;
    const billiardgl::PresentationSnapshot* sceneInput = nullptr;
    const billiardgl::CameraPresentation* cameraInput = nullptr;
    const billiardgl::HudViewModel* hudInput = nullptr;
    std::string capturePath;
    int initializedWidth = 0;
    int initializedHeight = 0;
    int resizedWidth = 0;
    int resizedHeight = 0;
    int captureWidth = 0;
    int captureHeight = 0;
    int shutdownCalls = 0;
    float cameraEyeX = 0.0f;
    float ballX = 0.0f;
    float cueRotation = 0.0f;
    float hudPower = 0.0f;
    bool initializeResult = true;
    bool captureResult = true;
};

}  // namespace

int main()
{
    using namespace billiardgl;
    using namespace billiardgl::render;

    RenderResources resources;
    RecordingOperations recording;
    LegacyRenderer renderer(resources, recording.operations());
    std::string error;
    RendererInitialization initialization;
    initialization.framebufferWidth = 1024;
    initialization.framebufferHeight = 768;
    expect(renderer.initialize(initialization, error), "legacy wrapper initializes");
    expect(error.empty(), "successful initialization has no error");
    expect(recording.initializedResources == &resources,
        "wrapper references the existing RenderResources instance");
    expect(recording.initializedWidth == 1024 && recording.initializedHeight == 768,
        "initial framebuffer dimensions reach the resource lifecycle");
    expect(recording.resizedWidth == 1024 && recording.resizedHeight == 768,
        "initialization applies the legacy viewport dimensions");

    renderer.resize(1920, 1080);
    expect(recording.resizedWidth == 1920 && recording.resizedHeight == 1080,
        "resize maps to the legacy viewport operation");

    PresentationSnapshot snapshot;
    snapshot.camera.eye.x = 41.5f;
    snapshot.balls[3].position.x = -17.25f;
    snapshot.cue.rotationDegrees = 93.0f;
    snapshot.hud.shotPower = 72.0f;
    renderer.render(snapshot);
    expect(recording.cameraInput == &snapshot.camera,
        "wrapper passes the free-function camera subobject without conversion");
    expect(recording.sceneInput == &snapshot,
        "wrapper passes the exact const snapshot to legacy renderScene");
    expect(recording.hudInput == &snapshot.hud,
        "wrapper passes the free-function HUD subobject without conversion");
    expect(recording.sceneResources == &resources,
        "legacy rendering uses the referenced RenderResources instance");
    expect(recording.cameraEyeX == 41.5f && recording.ballX == -17.25f &&
        recording.cueRotation == 93.0f && recording.hudPower == 72.0f,
        "world transforms, cue transform, camera and HUD inputs remain identical");
    const std::vector<std::string> expectedRenderOrder = {
        "initialize", "resize", "resize", "camera", "scene", "hud"};
    expect(recording.calls == expectedRenderOrder,
        "legacy free-function frame order remains camera, scene, HUD");

    RenderCaptureRequest capture;
    capture.outputPath = "compatibility.ppm";
    expect(renderer.capture(capture, error), "capture maps to offscreen legacy path");
    expect(recording.capturePath == "compatibility.ppm" &&
        recording.captureWidth == 1920 && recording.captureHeight == 1080,
        "capture defaults to the latest framebuffer dimensions");
    expect(recording.cameraEyeX == 41.5f && recording.ballX == -17.25f &&
        recording.cueRotation == 93.0f && recording.hudPower == 72.0f,
        "capture redraws an unchanged copy of the last presentation snapshot");

    renderer.shutdown();
    renderer.shutdown();
    expect(recording.shutdownResources == &resources,
        "shutdown maps to the same RenderResources lifecycle");
    expect(recording.calls.back() == "shutdown" && recording.shutdownCalls == 1,
        "shutdown operation is invoked exactly once and is idempotent");

    return 0;
}
