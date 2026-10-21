#include "render/modern/modern_renderer_product_ui.h"

#include <algorithm>
#include <cstdlib>
#include <iostream>
#include <stdexcept>
#include <string>
#include <vector>

namespace {

using namespace billiardgl;
using namespace billiardgl::render;

void expect(bool value, const std::string& message)
{
    if (!value) throw std::runtime_error(
        "modern renderer product UI branch: " + message);
}

class RecordingBackend final : public ModernProductUiBackend {
public:
    ui::UiGpuResult initialize(
        const ModernProductUiRuntimeConfig&) override
    {
        ++initializeCalls;
        return initializeResult;
    }

    ui::UiGpuResult reinitializeGpu() override
    {
        ++reinitializeCalls;
        return {true, {}};
    }

    ui::UiGpuResult resize(int width, int height, float dpi) override
    {
        ++resizeCalls;
        physicalWidth = width;
        physicalHeight = height;
        physicalDpi = dpi;
        return {true, {}};
    }

    ModernProductUiRuntimeResult render(
        const ModernProductSessionSnapshot& session,
        const PresentationSnapshot&,
        const ui::ProductUiFrameEnvironment&) override
    {
        ++renderCalls;
        screen = session.shell.screen;
        viewport = session.shellViewport;
        colorVision = session.effectiveSettings.colorVisionMode;
        itemIds.clear();
        itemLabels.clear();
        for (const ShellItemViewModel& item : session.shell.items) {
            itemIds.push_back(item.id);
            itemLabels.push_back(item.label);
        }
        ModernProductUiRuntimeResult result;
        result.ok = renderError.empty();
        result.errorCode = renderError;
        result.screen = screen;
        result.overlay = screen == ProductScreen::Match ?
            ui::ProductUiOverlayKind::GameplayHud :
            ui::ProductUiOverlayKind::Shell;
        result.gpuSubmitted = result.ok;
        result.gpuMetrics.drawCalls = result.ok ? 3 : 0;
        return result;
    }

    void contextLost() noexcept override { ++contextLostCalls; }
    void shutdown() noexcept override { ++shutdownCalls; }

    int initializeCalls = 0;
    int reinitializeCalls = 0;
    int resizeCalls = 0;
    int renderCalls = 0;
    int contextLostCalls = 0;
    int shutdownCalls = 0;
    int physicalWidth = 0;
    int physicalHeight = 0;
    float physicalDpi = 0.0f;
    ProductScreen screen = ProductScreen::MainMenu;
    ui::ShellViewport viewport;
    settings::ColorVisionMode colorVision =
        settings::ColorVisionMode::Default;
    std::vector<std::string> itemIds;
    std::vector<std::string> itemLabels;
    ui::UiGpuResult initializeResult{true, {}};
    std::string renderError;
};

ModernProductSessionSnapshot productSnapshot(ProductScreen screen)
{
    ModernProductSessionSnapshot snapshot;
    snapshot.shell.screen = screen;
    snapshot.shellViewport = {1280, 720, 2560, 1440, 192.0f};
    snapshot.effectiveSettings = settings::ProductSettings::defaults();
    snapshot.effectiveSettings.colorVisionMode =
        settings::ColorVisionMode::Deuteranopia;
    if (screen == ProductScreen::MainMenu) {
        snapshot.shell.items = {
            {"single_player", "Single player"},
            {"training", "Training"},
            {"replays", "Replays"},
            {"settings", "Settings"}};
    }
    return snapshot;
}

bool contains(const std::vector<std::string>& values,
    const std::string& expected)
{
    return std::find(values.begin(), values.end(), expected) != values.end();
}

void testHiddenBranchesNeverCallBackend()
{
    RecordingBackend backend;
    ModernRendererProductUiPass pass(backend);
    const ModernProductSessionSnapshot snapshot =
        productSnapshot(ProductScreen::MainMenu);
    const PresentationSnapshot presentation;
    for (const ui::ProductUiFrameEnvironment environment : {
            ui::ProductUiFrameEnvironment{false, false, false},
            ui::ProductUiFrameEnvironment{true, true, false},
            ui::ProductUiFrameEnvironment{true, false, true}}) {
        const ModernProductUiRuntimeResult result = pass.render(
            snapshot, presentation, environment);
        expect(result.ok && result.overlay == ui::ProductUiOverlayKind::None,
            "hidden branch returns an explicit successful no-overlay result");
    }
    expect(backend.initializeCalls == 0 && backend.resizeCalls == 0 &&
        backend.renderCalls == 0 &&
        pass.metrics().hiddenFrameSkips == 3,
        "legacy, automation, and windowless branches perform zero backend calls");
}

void testVisibleBranchForwardsOneAuthoritativeSnapshot()
{
    RecordingBackend backend;
    ModernRendererProductUiPass pass(backend);
    ModernProductUiRuntimeConfig config;
    expect(pass.initialize(config).ok && backend.initializeCalls == 1,
        "renderer branch initializes its backend exactly once");
    const ModernProductSessionSnapshot snapshot =
        productSnapshot(ProductScreen::MainMenu);
    const ModernProductUiRuntimeResult result = pass.render(snapshot,
        PresentationSnapshot{}, {true, false, false});
    expect(result.ok && result.gpuSubmitted &&
        result.overlay == ui::ProductUiOverlayKind::Shell &&
        backend.resizeCalls == 1 && backend.renderCalls == 1,
        "one visible product frame performs one resize and one render call");
    expect(backend.screen == ProductScreen::MainMenu &&
        backend.viewport.logicalWidth == 1280 &&
        backend.viewport.logicalHeight == 720 &&
        backend.viewport.physicalWidth == 2560 &&
        backend.viewport.physicalHeight == 1440 &&
        backend.viewport.dpi == 192.0f &&
        backend.colorVision == settings::ColorVisionMode::Deuteranopia,
        "the same session screen, Retina viewport, DPI, and CVD mode reach the pass");
    expect(pass.metrics().visibleFrameCalls == 1 &&
        pass.metrics().resizeCalls == 1 && pass.metrics().failures == 0,
        "renderer branch metrics prove exactly one visible submission");
    expect(result.gpuMetrics.drawCalls == 3 &&
        backend.itemIds.size() == 4 && backend.itemLabels.size() == 4 &&
        contains(backend.itemIds, "single_player") &&
        contains(backend.itemIds, "training") &&
        contains(backend.itemIds, "replays") &&
        contains(backend.itemIds, "settings") &&
        contains(backend.itemLabels, "Training"),
        "recording submission traces the four core Main item IDs/labels and nonzero GPU draw metrics");

    pass.contextLost();
    expect(backend.contextLostCalls == 1 && pass.reinitializeGpu().ok &&
        backend.reinitializeCalls == 1,
        "context loss and GPU reinitialization reach the same backend");
    pass.shutdown();
    expect(backend.shutdownCalls == 1 && !pass.initialized(),
        "shutdown releases the initialized backend exactly once");
}

void testFailuresAreStableAndVisible()
{
    RecordingBackend backend;
    ModernRendererProductUiPass pass(backend);
    backend.initializeResult = {false, "recording_create_failed"};
    expect(pass.initialize({}).errorCode ==
            "modern_renderer_product_ui_initialize_failed:recording_create_failed",
        "initialization failure is prefixed with a stable renderer error");

    RecordingBackend submitBackend;
    ModernRendererProductUiPass submitPass(submitBackend);
    expect(submitPass.initialize({}).ok, "submit failure fixture initializes");
    submitBackend.renderError = "recording_submit_failed";
    const ModernProductUiRuntimeResult failed = submitPass.render(
        productSnapshot(ProductScreen::Match), PresentationSnapshot{},
        {true, false, false});
    expect(!failed.ok && failed.errorCode == "recording_submit_failed" &&
        failed.screen == ProductScreen::Match &&
        submitPass.metrics().failures == 1,
        "submit failure remains explicit and cannot silently degrade");
}

}  // namespace

int main()
{
    try {
        testHiddenBranchesNeverCallBackend();
        testVisibleBranchForwardsOneAuthoritativeSnapshot();
        testFailuresAreStableAndVisible();
        std::cout << "ModernRenderer product UI branch checks passed\n";
        return EXIT_SUCCESS;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return EXIT_FAILURE;
    }
}
