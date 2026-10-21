#include "launch_options.h"

#include <cerrno>
#include <cstdlib>
#include <cstring>

namespace billiardgl {
namespace {

constexpr int kMinimumWindowDimension = 320;
constexpr int kMaximumWindowDimension = 16384;

bool parseWindowDimension(const char* text, int& value)
{
    if (text == nullptr || text[0] == '\0') {
        return false;
    }
    errno = 0;
    char* end = nullptr;
    const long parsed = std::strtol(text, &end, 10);
    if (errno != 0 || end == text || *end != '\0' ||
        parsed < kMinimumWindowDimension || parsed > kMaximumWindowDimension) {
        return false;
    }
    value = static_cast<int>(parsed);
    return true;
}

bool requireValue(int argc, int index)
{
    return index + 1 < argc;
}

}  // namespace

LaunchOptions parseLaunchOptions(int argc, char* argv[], LaunchPolicy policy)
{
    LaunchOptions result;
    bool automation = false;
    bool headless = false;
    bool rendered = false;
    bool legacyRendererRequested = false;

    for (int i = 1; i < argc && result.ok; ++i) {
        if (!std::strcmp(argv[i], "--automation")) {
            automation = true;
        } else if (!std::strcmp(argv[i], "--headless")) {
            headless = true;
        } else if (!std::strcmp(argv[i], "--rendered")) {
            rendered = true;
        } else if (!std::strcmp(argv[i], "--print-physics-profile")) {
            result.printPhysicsProfile = true;
        } else if (!std::strcmp(argv[i], "--product-mode")) {
            result.productMode = true;
            result.runtime.rendererBackend = RendererBackendSelection::Modern;
        } else if (!std::strcmp(argv[i], "--settings-path")) {
            if (!requireValue(argc, i) || argv[i + 1][0] == '\0') {
                result.ok = false;
                result.error = "settings path must not be empty";
                continue;
            }
            result.productSettingsPath = argv[++i];
        } else if (!std::strcmp(argv[i], "--product-frame-performance")) {
            if (!requireValue(argc, i) || argv[i + 1][0] == '\0') {
                result.ok = false;
                result.error = "product frame performance report path must not be empty";
                continue;
            }
            result.productFramePerformancePath = argv[++i];
        } else if (!std::strcmp(argv[i], "--ai-opponent")) {
            result.aiOpponent = true;
            result.runtime.rendererBackend = RendererBackendSelection::Modern;
        } else if (!std::strcmp(argv[i], "--ai-difficulty")) {
            if (!requireValue(argc, i)) {
                result.ok = false;
                result.error = "AI difficulty must be rookie, club, or expert";
                continue;
            }
            result.aiDifficulty = argv[++i];
            if (result.aiDifficulty != "rookie" && result.aiDifficulty != "club" &&
                result.aiDifficulty != "expert") {
                result.ok = false;
                result.error = "AI difficulty must be rookie, club, or expert";
                continue;
            }
            result.aiOpponent = true;
            result.runtime.rendererBackend = RendererBackendSelection::Modern;
        } else if (!std::strcmp(argv[i], "--transport") && requireValue(argc, i)) {
            result.transport = argv[++i];
        } else if (!std::strcmp(argv[i], "--windowed")) {
            result.runtime.windowedMode = true;
        } else if (!std::strcmp(argv[i], "--fullscreen")) {
            result.runtime.windowedMode = false;
        } else if (!std::strcmp(argv[i], "--renderer")) {
            if (!requireValue(argc, i)) {
                result.ok = false;
                result.error = "renderer must be legacy or modern";
                continue;
            }
            const char* renderer = argv[++i];
            if (!std::strcmp(renderer, "legacy")) {
                result.runtime.rendererBackend = RendererBackendSelection::Legacy;
                legacyRendererRequested = true;
            } else if (!std::strcmp(renderer, "modern")) {
                result.runtime.rendererBackend = RendererBackendSelection::Modern;
            } else {
                result.ok = false;
                result.error = "renderer must be legacy or modern";
            }
        } else if (!std::strcmp(argv[i], "--width") && requireValue(argc, i)) {
            if (!parseWindowDimension(argv[++i], result.runtime.width)) {
                result.ok = false;
                result.error = "width must be an integer between 320 and 16384";
            }
        } else if (!std::strcmp(argv[i], "--height") && requireValue(argc, i)) {
            if (!parseWindowDimension(argv[++i], result.runtime.height)) {
                result.ok = false;
                result.error = "height must be an integer between 320 and 16384";
            }
        } else if (!std::strcmp(argv[i], "--screenshot") && requireValue(argc, i)) {
            result.runtime.screenshotPath = argv[++i];
            result.runtime.windowedMode = true;
        } else if (!std::strcmp(argv[i], "--screenshot-scene") && requireValue(argc, i)) {
            const char* scene = argv[++i];
            if (!std::strcmp(scene, "default")) {
                result.runtime.screenshotScene = ScreenshotScene::Default;
            } else if (!std::strcmp(scene, "help")) {
                result.runtime.screenshotScene = ScreenshotScene::Help;
            } else if (!std::strcmp(scene, "aim")) {
                result.runtime.screenshotScene = ScreenshotScene::Aim;
            } else if (!std::strcmp(scene, "after-shot")) {
                result.runtime.screenshotScene = ScreenshotScene::AfterShot;
            } else {
                result.ok = false;
                result.error = "unknown screenshot scene";
            }
        } else {
            result.ok = false;
            result.error = std::string("unknown option: ") + argv[i];
        }
    }

    if (policy == LaunchPolicy::Product) {
        if (legacyRendererRequested) {
            result.ok = false;
            result.error = "legacy renderer retired from production; use BilliardsLegacyCompatibility for historical tests";
            return result;
        }
        result.runtime.rendererBackend = RendererBackendSelection::Modern;
        // Only visible ordinary interaction enters the product shell.
        // Automation/profile/capture retain their existing independent routes.
        // The historical direct AI shortcut remains a modern diagnostic route.
        if (!automation && !headless && !rendered && !result.printPhysicsProfile &&
            result.runtime.screenshotPath.empty() && !result.aiOpponent) {
            result.productMode = true;
        }
    }

    if ((headless || rendered) && !automation) {
        result.ok = false;
        result.error = "mode requires automation";
    }
    if (automation && headless == rendered) {
        result.ok = false;
        result.error = "choose exactly one automation mode";
    }
    if (automation && result.transport != "stdio") {
        result.ok = false;
        result.error = "unsupported transport";
    }
    if (result.printPhysicsProfile && (automation || headless || rendered)) {
        result.ok = false;
        result.error = "physics profile query cannot be combined with automation";
    }
    if (automation && result.aiOpponent) {
        result.ok = false;
        result.error = "configure AI through automation commands";
    }
    if (!result.productMode && !result.productSettingsPath.empty()) {
        result.ok = false;
        result.error = "settings path requires product mode";
    }
    if (!result.productMode && !result.productFramePerformancePath.empty()) {
        result.ok = false;
        result.error = "product frame performance requires product mode";
    }
    if (result.productMode && legacyRendererRequested) {
        result.ok = false;
        result.error = "product mode requires the modern renderer";
    }
    if (result.productMode && automation) {
        result.ok = false;
        result.error = "product mode cannot be combined with automation";
    }
    if (!result.productFramePerformancePath.empty() && automation) {
        result.ok = false;
        result.error = "product frame performance requires the visible interactive loop";
    }
    if (result.productMode && result.printPhysicsProfile) {
        result.ok = false;
        result.error = "product mode cannot be combined with physics profile query";
    }
    if (result.productMode && result.aiOpponent) {
        result.ok = false;
        result.error = "product mode configures AI through single-player setup";
    }
    if (result.productMode && !result.runtime.screenshotPath.empty()) {
        result.ok = false;
        result.error = "product mode cannot be combined with legacy screenshot capture";
    }
    if (automation) {
        result.mode = headless ? RunMode::AutomationHeadless : RunMode::AutomationRendered;
    }
    if (result.aiOpponent)
        result.runtime.rendererBackend = RendererBackendSelection::Modern;
    if (result.productMode)
        result.runtime.rendererBackend = RendererBackendSelection::Modern;
    return result;
}

}  // namespace billiardgl
