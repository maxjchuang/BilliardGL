// Modern-only production entry. Historical GLUT/fixed-pipeline callbacks
// remain in billiards.cpp for the separate compatibility test executable.
#include "launch_options.h"
#include "game_runtime.h"
#include "ai/ai_player_controller.h"
#include "automation_controller.h"
#include "automation_runner.h"
#include "stdio_transport.h"
#ifdef __APPLE__
#include "render/modern/modern_application_mac.h"
#endif

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <iostream>

int main(int argc, char* argv[]) {
    using namespace billiardgl;
    const LaunchOptions options = parseLaunchOptions(argc, argv, LaunchPolicy::Product);
    if (!options.ok) {
        std::fprintf(stderr, "%s\n", options.error.c_str());
        return 2;
    }
    if (options.printPhysicsProfile) {
        std::cout << canonicalPhysicsProfileJson(defaultChinesePoolPhysicsProfile());
        return 0;
    }
    if (options.mode == RunMode::AutomationHeadless) {
        GameRuntime runtime;
        ai::AiPlayerController aiPlayer;
        AutomationController controller(runtime, AutomationMode::Headless, &aiPlayer);
        StdioTransport transport(std::cin, std::cout);
        return runAutomation(transport, controller, "headless", "none");
    }
#ifdef __APPLE__
    render::ModernApplicationOptions modern;
    modern.width = options.runtime.width;
    modern.height = options.runtime.height;
    modern.msaaSamples = 4;
    modern.quality = render::RenderQuality::Medium;
    modern.automation = options.mode == RunMode::AutomationRendered;
    modern.productMode = options.productMode;
    modern.aiOpponent = options.aiOpponent;
    modern.aiDifficulty = options.aiDifficulty == "rookie" ? ai::Difficulty::Rookie :
        options.aiDifficulty == "expert" ? ai::Difficulty::Expert : ai::Difficulty::Club;
    modern.showWindow = !modern.automation && options.runtime.screenshotPath.empty();
    modern.lookdevManifestPath = std::string(BILLIARDGL_SOURCE_ROOT) +
        "/assets/modern/lookdev/focused_practice_hall_v1.lookdev.json";
    const char* phase2Override = std::getenv("BILLIARDGL_MODERN_PHASE2_ROOT");
    const std::string phase2Root = phase2Override ? std::string(phase2Override) :
        std::string(BILLIARDGL_SOURCE_ROOT) + "/assets/modern/phase2";
    modern.phase2AssetRoot = phase2Root;
    const char* formalCapture = std::getenv("BILLIARDGL_FORMAL_CAPTURE");
    modern.formalCapture = formalCapture && std::strcmp(formalCapture, "1") == 0;
    modern.pbrParametersPath = phase2Root + "/focused_practice_hall_pbr_v1.params.json";
    modern.pbrAssetManifestPath = phase2Root + "/focused_practice_hall_modular_v1.asset.json";
    modern.lightingRigPath = phase2Root + "/focused_practice_hall_lighting_rig_v1.json";
    modern.gltfScenePath = phase2Root + "/focused_practice_hall_modular_v1.gltf";
    modern.venueScenePath = phase2Root + "/focused_practice_hall_venue_v1.gltf";
    modern.productSettingsPath = options.productSettingsPath;
    modern.productFramePerformanceReportPath = options.productFramePerformancePath;
    modern.trainingManifestPath = std::string(BILLIARDGL_SOURCE_ROOT) +
        "/assets/training/phase4/training_pack_v2.json";
    modern.screenshotPath = options.runtime.screenshotPath;
    switch (options.runtime.screenshotScene) {
    case ScreenshotScene::Default: modern.screenshotScene = "default"; break;
    case ScreenshotScene::Help: modern.screenshotScene = "help"; break;
    case ScreenshotScene::Aim: modern.screenshotScene = "aim"; break;
    case ScreenshotScene::AfterShot: modern.screenshotScene = "after-shot"; break;
    }
    return render::runModernApplication(modern, std::cin, std::cout, std::cerr);
#else
    std::fprintf(stderr, "modern backend unavailable in this build/path\n");
    return 3;
#endif
}
