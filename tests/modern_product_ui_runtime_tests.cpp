#include "automation_json.h"
#include "deterministic_hash.h"
#include "render/modern/modern_product_ui_runtime.h"

#include <cstdlib>
#include <chrono>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <iterator>
#include <stdexcept>
#include <string>
#include <vector>

namespace {

using namespace billiardgl;
using namespace billiardgl::render;
using namespace billiardgl::render::ui;

void expect(bool value, const std::string& message)
{
    if (!value) throw std::runtime_error("product UI runtime: " + message);
}

class FakeDispatch final : public ProductUiGlDispatch {
public:
    UiGpuResult createResources(const UiGpuResourceCreateInfo& info,
        UiGpuResourceHandles& handles) override
    {
        ++creates;
        atlasBytes = info.atlasPixels == nullptr ? 0 :
            info.atlasPixels->size();
        if (!createError.empty()) return {false, createError};
        handles = {101, 102, 103, 104};
        return {true, {}};
    }
    void destroyResources(UiGpuResourceHandles& handles) noexcept override
    {
        ++destroys;
        handles = {};
    }
    void abandonResources(UiGpuResourceHandles& handles) noexcept override
    {
        ++abandons;
        handles = {};
    }
    UiGpuResult beginFrame(const UiGpuResourceHandles&, int width,
        int height) override
    {
        ++begins;
        lastWidth = width;
        lastHeight = height;
        return {true, {}};
    }
    UiGpuResult uploadVertices(const UiGpuResourceHandles&,
        const std::vector<UiGpuVertex>& value) override
    {
        ++uploads;
        lastVertices = value;
        return uploadError.empty() ? UiGpuResult{true, {}} :
            UiGpuResult{false, uploadError};
    }
    UiGpuResult setScissor(const UiGpuScissor& value) override
    {
        scissors.push_back(value);
        return {true, {}};
    }
    UiGpuResult draw(const UiGpuResourceHandles&,
        const UiGpuBatch& batch) override
    {
        draws.push_back(batch);
        return drawError.empty() ? UiGpuResult{true, {}} :
            UiGpuResult{false, drawError};
    }
    UiGpuResult endFrame() override
    {
        ++ends;
        return {true, {}};
    }

    int creates = 0;
    int destroys = 0;
    int abandons = 0;
    int begins = 0;
    int uploads = 0;
    int ends = 0;
    int lastWidth = 0;
    int lastHeight = 0;
    std::size_t atlasBytes = 0;
    std::vector<UiGpuVertex> lastVertices;
    std::vector<UiGpuScissor> scissors;
    std::vector<UiGpuBatch> draws;
    std::string createError;
    std::string uploadError;
    std::string drawError;
};

ModernProductUiRuntimeConfig config()
{
    return ModernProductUiRuntimeConfig::trustedDefaults(
        BILLIARDGL_SOURCE_ROOT, "/System/Library/Fonts/Menlo.ttc");
}

ModernProductSessionSnapshot session(ProductScreen screen)
{
    ModernProductSessionSnapshot value;
    value.effectiveSettings = settings::ProductSettings::defaults();
    value.shellViewport = {1280, 720, 1920, 1080, 144.0f};
    value.shell.screen = screen;
    value.shell.screenId = "runtime_test";
    value.shell.title = "BILLIARDGL";
    value.shell.subtitle = "Focused practice";
    value.shell.breadcrumb = "PRODUCT / RUNTIME";
    value.shell.footer = "Enter Confirm   Esc Back";
    value.shell.uiScale = 1.0f;
    ShellItemViewModel item;
    item.id = "runtime_item";
    item.label = "Practice";
    item.value = "Ready";
    item.detail = "One production UI facade";
    item.shapeToken = screen == ProductScreen::ReplayPlayback ?
        "pause_bars" : "camera_frame";
    item.focused = true;
    value.shell.items.push_back(item);
    return value;
}

PresentationSnapshot presentation()
{
    PresentationSnapshot value;
    value.hud.currentPlayerNumber = 1;
    value.hud.viewportWidth = 1920;
    value.hud.viewportHeight = 1080;
    value.hud.aimMode = true;
    value.hud.showPowerMeter = true;
    value.hud.shotPower = 0.72f;
    value.shotControl.requestedPower = 72.0f;
    value.rules.enabled = true;
    value.rules.rulesetId = "wpa_heyball_2025_08_16_v1";
    value.rules.groups = {{"solid", "stripe"}};
    return value;
}

bool sameVertex(const UiGpuVertex& first, const UiGpuVertex& second)
{
    return first.ndcX == second.ndcX && first.ndcY == second.ndcY &&
        first.u == second.u && first.v == second.v &&
        first.color.red == second.color.red &&
        first.color.green == second.color.green &&
        first.color.blue == second.color.blue &&
        first.color.alpha == second.color.alpha && first.sdf == second.sdf &&
        first.clipLeft == second.clipLeft &&
        first.clipBottom == second.clipBottom &&
        first.clipRight == second.clipRight && first.clipTop == second.clipTop;
}

bool sameVertices(const std::vector<UiGpuVertex>& first,
    const std::vector<UiGpuVertex>& second)
{
    return first.size() == second.size() && std::equal(first.begin(),
        first.end(), second.begin(), sameVertex);
}

void testHiddenModesNeedNoAssetsOrGpu()
{
    FakeDispatch dispatch;
    ModernProductUiRuntime runtime(dispatch);
    const ModernProductSessionSnapshot snapshot =
        session(ProductScreen::MainMenu);
    for (const ProductUiFrameEnvironment environment : {
            ProductUiFrameEnvironment{false, false, false},
            ProductUiFrameEnvironment{true, true, false},
            ProductUiFrameEnvironment{true, false, true}}) {
        const ModernProductUiRuntimeResult result = runtime.render(snapshot,
            presentation(), environment);
        expect(result.ok && result.overlay == ProductUiOverlayKind::None &&
            !result.gpuSubmitted && result.gpuMetrics.drawCalls == 0 &&
            result.canonicalSha256.size() == 64,
            "hidden mode returns deterministic zero draw before initialization");
    }
    expect(dispatch.creates == 0 && dispatch.begins == 0,
        "hidden modes never touch resource or frame dispatch");
    expect(runtime.render(snapshot, presentation(), {true, false, false})
            .errorCode == "product_ui_runtime_not_initialized",
        "normal product mode requires explicit initialization");
}

void testIndependentInsetChromeAtlasLifecycle()
{
    auto primary = config();
    primary.retainSubmittedTextEvidence = true;
    FakeDispatch baselineDispatch;
    ModernProductUiRuntime baseline(baselineDispatch);
    expect(baseline.initialize(primary).ok, "primary atlas initializes");
    const auto snapshot = session(ProductScreen::MainMenu);
    const auto before = baseline.render(snapshot, presentation(), {true,false,false});
    auto bilingual = primary;
    bilingual.insetChromeFont = primary.font;
    bilingual.insetChromeFont->fontPath = "/System/Library/Fonts/Supplemental/Arial Unicode.ttf";
    bilingual.insetChromeFont->faceIndex = 0;
    FakeDispatch dispatch;
    ModernProductUiRuntime runtime(dispatch);
    const auto initialized = runtime.initialize(bilingual);
    expect(initialized.ok && dispatch.creates == 2,
        "independent bilingual chrome atlas and GPU resource initialize: " + initialized.errorCode);
    const auto after = runtime.render(snapshot, presentation(), {true,false,false});
    expect(before.ok && after.ok && before.fontAtlasCacheKey == after.fontAtlasCacheKey &&
        before.canonicalSha256 == after.canonicalSha256 &&
        runtime.trajectoryInsetChromeAtlasCacheKey() != after.fontAtlasCacheKey,
        "inset font does not alter primary Menlo atlas or menu draw evidence");
    expect(runtime.trajectoryInsetChromeAtlasContainsExactCodepoints({0x8f68,0x8ff9,0x7a97}),
        "inset Chinese glyphs are prebaked without fallback boxes");
    runtime.contextLost();
    expect(dispatch.abandons == 2 && runtime.reinitializeGpu().ok && dispatch.creates == 4,
        "both independent resource sets follow context loss and restoration");
    runtime.shutdown();
    expect(dispatch.destroys == 2, "both restored resource sets are released");
    bilingual.insetChromeFont->fontPath = "/missing-inset-font.ttf";
    FakeDispatch missingDispatch;
    ModernProductUiRuntime missing(missingDispatch);
    const auto failed = missing.initialize(bilingual);
    expect(!failed.ok && failed.errorCode.find("inset_chrome_font") != std::string::npos,
        "missing inset font reports a specific readable resource error");
}

void testOneCallShellHudLifecycle()
{
    FakeDispatch dispatch;
    ModernProductUiRuntime runtime(dispatch);
    const UiGpuResult initialized = runtime.initialize(config());
    expect(initialized.ok && runtime.assetsInitialized() &&
        runtime.gpuInitialized() && dispatch.creates == 1 &&
        dispatch.atlasBytes == 1024u * 1024u,
        "one initialization validates assets, builds atlas, and creates GPU resources");
    expect(runtime.resize(1920, 1080, 144.0f).ok,
        "explicit physical resize is accepted without resource rebuild");

    ModernProductSessionSnapshot shellSession =
        session(ProductScreen::ReplayPlayback);
    const ModernProductUiRuntimeResult shell = runtime.render(shellSession,
        presentation(), {true, false, false});
    expect(shell.ok && shell.overlay == ProductUiOverlayKind::Shell &&
        shell.screen == ProductScreen::ReplayPlayback &&
        shell.canonicalSha256.size() == 64 && shell.gpuSubmitted &&
        shell.gpuMetrics.drawCalls > 0 &&
        shell.gpuMetrics.rejectedOpaqueIcons == 0 &&
        dispatch.lastWidth == 1920 && dispatch.lastHeight == 1080,
        "single render call builds and submits replay Shell controls");

    ModernProductSessionSnapshot matchSession = session(ProductScreen::Match);
    matchSession.effectiveSettings.colorVisionMode =
        settings::ColorVisionMode::Deuteranopia;
    const ModernProductUiRuntimeResult match = runtime.render(matchSession,
        presentation(), {true, false, false});
    expect(match.ok && match.overlay == ProductUiOverlayKind::GameplayHud &&
        match.screen == ProductScreen::Match && match.gpuSubmitted &&
        match.gpuMetrics.solidVertices > 0 &&
        match.gpuMetrics.sdfVertices > 0 &&
        match.canonicalSha256 != shell.canonicalSha256,
        "same facade routes Match through expanded Gameplay HUD and GPU batches");

    ModernProductSessionSnapshot stale = shellSession;
    stale.shellViewport.physicalWidth = 1280;
    const int begins = dispatch.begins;
    expect(runtime.render(stale, presentation(), {true, false, false})
            .errorCode == "product_ui_runtime_viewport_stale" &&
        dispatch.begins == begins,
        "stale session viewport fails before layout or GPU dispatch");

    runtime.contextLost();
    expect(runtime.assetsInitialized() && !runtime.gpuInitialized() &&
        dispatch.abandons == 1,
        "context loss retains validated CPU assets and abandons stale GL names");
    const ModernProductUiRuntimeResult afterLoss = runtime.render(shellSession,
        presentation(), {true, false, false});
    expect(!afterLoss.ok && afterLoss.errorCode ==
            "product_ui_runtime_gpu_not_initialized" &&
        afterLoss.canonicalSha256.size() == 64,
        "after context loss frame evidence remains available but submit fails closed");
    expect(runtime.render(shellSession, presentation(), {true, true, false}).ok,
        "automation remains zero draw while context is absent");
    expect(runtime.reinitializeGpu().ok && runtime.gpuInitialized() &&
        dispatch.creates == 2 &&
        runtime.render(shellSession, presentation(), {true, false, false}).ok,
        "replacement context rebuilds only GPU resources and resumes one-call render");
    runtime.shutdown();
    expect(!runtime.assetsInitialized() && !runtime.gpuInitialized() &&
        dispatch.destroys == 1,
        "shutdown releases the post-recovery resource set exactly once");
}

void testDynamicTrainingFramesBypassStaticShellCache()
{
    FakeDispatch dispatch;
    ModernProductUiRuntime runtime(dispatch);
    expect(runtime.initialize(config()).ok &&
        runtime.resize(1920, 1080, 144.0f).ok,
        "dynamic Training cache fixture initializes");

    ModernProductSessionSnapshot training =
        session(ProductScreen::TrainingSession);
    training.shell.training.state =
        training::TrainingProductState::AttemptActive;
    training.shell.training.powerAdjustmentAllowed = true;
    training.shell.training.tipAdjustmentAllowed = false;
    auto& hints = training.shell.training.teachingHints;
    hints.available = true;
    hints.minimumPowerPercent = 22.5f;
    hints.maximumPowerPercent = 37.5f;
    hints.defaultPowerPercent = 30.0f;
    hints.tipXBallRadii = 0.0f;
    hints.tipYBallRadii = 0.0f;
    hints.tipLabel = "center_ball";
    hints.aimYawRadians = 0.0f;
    hints.aimToleranceDegrees = 1.0f;
    hints.targetPocketId = "side_px";
    hints.targetPocketCenterCm = {{63.5f, 0.0f}};
    hints.cueBallTargetEnabled = true;
    hints.cueBallTargetCenterCm = {{38.5f, 0.0f}};
    hints.cueBallTargetRadiusCm = 4.5f;
    hints.cueBallIndex = 0;
    hints.objectBallIndex = 1;
    hints.shotLineStartCm = {{10.0f, 0.0f}};
    hints.shotLineObjectCenterCm = {{40.0f, 0.0f}};
    PresentationSnapshot firstPresentation = presentation();
    firstPresentation.shotControl.requestedPower = 54.0f;
    firstPresentation.shotControl.tipOffsetX = 0.0f;
    firstPresentation.shotControl.tipOffsetY = 0.0f;
    firstPresentation.camera.eye = {0.0f, 420.0f, 0.0f};
    firstPresentation.camera.target = {0.0f,
        kTableModelPlaySurfaceHeightCm, 0.0f};
    const ProductUiFrameEnvironment preparedEnvironment{true, false, false};
    const auto& prepared = runtime.prepareFrame(training, firstPresentation,
        preparedEnvironment);
    const auto preparedDigest = prepared.evidenceSha256;
    expect(prepared.ok && dispatch.uploads == 0 && dispatch.begins == 0,
        "inset exclusions consume CPU preparation without a GPU submission");
    const ModernProductUiRuntimeResult first = runtime.render(training,
        firstPresentation, preparedEnvironment);
    expect(first.canonicalSha256 == preparedDigest && dispatch.uploads == 1,
        "normal submission consumes exactly the prepared same-frame UI layout");
    const std::vector<UiGpuVertex> firstVertices = dispatch.lastVertices;

    ModernProductSessionSnapshot expanded = training;
    expanded.shell.inspectorMode = 1;
    PresentationSnapshot changedPresentation = firstPresentation;
    changedPresentation.shotControl.requestedPower = 118.0f;
    changedPresentation.shotControl.tipOffsetX = -0.65f;
    changedPresentation.shotControl.tipOffsetY = 0.40f;
    const ModernProductUiRuntimeResult changed = runtime.render(expanded,
        changedPresentation, {true, false, false});
    expect(first.ok && changed.ok && first.gpuSubmitted &&
        changed.gpuSubmitted && dispatch.uploads == 2 &&
        first.canonicalSha256 != changed.canonicalSha256 &&
        !sameVertices(firstVertices, dispatch.lastVertices),
        "Training mode and shot controls rebuild CPU evidence and uploaded GPU vertices despite identical shell rows");

    const std::vector<UiGpuVertex> changedVertices = dispatch.lastVertices;
    const std::string changedSha = changed.canonicalSha256;
    changedPresentation.camera.projectionPan = {17.0f, 0.0f, 0.0f};
    const ModernProductUiRuntimeResult cameraChanged = runtime.render(expanded,
        changedPresentation, {true, false, false});
    expect(cameraChanged.ok && cameraChanged.gpuSubmitted &&
        dispatch.uploads == 3 && cameraChanged.canonicalSha256 != changedSha &&
        !sameVertices(changedVertices, dispatch.lastVertices),
        "Training camera changes independently rebuild projected goal evidence and uploaded GPU vertices");
}

std::string readFile(const std::string& path)
{
    std::ifstream input(path, std::ios::binary);
    expect(static_cast<bool>(input), "failure fixture source opens");
    return {std::istreambuf_iterator<char>(input),
        std::istreambuf_iterator<char>()};
}

void testAssetAndGpuFailures()
{
    {
        FakeDispatch dispatch;
        ModernProductUiRuntime runtime(dispatch);
        auto bad = config();
        bad.shellTokenSha256 = std::string(64, '0');
        expect(runtime.initialize(bad).errorCode ==
                "product_ui_runtime_asset_sha_mismatch:shell_tokens" &&
            dispatch.creates == 0,
            "tampered asset identity fails before parse, atlas, or GL");
    }
    {
        FakeDispatch dispatch;
        ModernProductUiRuntime runtime(dispatch);
        auto missing = config();
        missing.hudManifestPath = "/definitely/missing/hud.json";
        expect(runtime.initialize(missing).errorCode ==
                "product_ui_runtime_asset_unreadable:hud_manifest" &&
            dispatch.creates == 0,
            "missing asset path fails explicitly before GL");
    }
    {
        FakeDispatch dispatch;
        ModernProductUiRuntime runtime(dispatch);
        auto invalidFont = config();
        invalidFont.font.faceIndex = 9999;
        expect(runtime.initialize(invalidFont).errorCode.rfind(
                "product_ui_runtime_atlas_invalid:", 0) == 0 &&
            dispatch.creates == 0,
            "invalid font face fails during atlas initialization before GL");
    }
    {
        auto invalidTokens = config();
        json::ParseResult parsed = json::parse(readFile(
            invalidTokens.shellTokenPath));
        expect(parsed.ok, "Shell mutation fixture parses");
        parsed.value["icon_tokens"].asObject().erase("camera_frame");
        const std::string bytes = json::stringify(parsed.value);
        const std::filesystem::path path =
            std::filesystem::temp_directory_path() /
            ("billiardgl_invalid_shell_tokens_runtime_" +
                std::to_string(std::chrono::steady_clock::now()
                    .time_since_epoch().count()) + ".json");
        {
            std::ofstream output(path, std::ios::binary | std::ios::trunc);
            output << bytes;
        }
        invalidTokens.shellTokenPath = path.string();
        invalidTokens.shellTokenSha256 = sha256Hex(bytes);
        FakeDispatch dispatch;
        ModernProductUiRuntime runtime(dispatch);
        const UiGpuResult result = runtime.initialize(invalidTokens);
        std::filesystem::remove(path);
        expect(result.errorCode.rfind(
                "product_ui_runtime_shell_tokens_invalid:", 0) == 0 &&
            dispatch.creates == 0,
            "SHA-valid but unknown/incomplete token inventory fails strict schema before GL");
    }
    {
        FakeDispatch dispatch;
        dispatch.createError = "injected_context_create_failure";
        ModernProductUiRuntime runtime(dispatch);
        const UiGpuResult failed = runtime.initialize(config());
        expect(failed.errorCode ==
                "product_ui_runtime_gpu_initialize_failed:injected_context_create_failure" &&
            runtime.assetsInitialized() && !runtime.gpuInitialized(),
            "GPU/context initialization error preserves validated CPU assets for retry");
        dispatch.createError.clear();
        expect(runtime.reinitializeGpu().ok,
            "GPU initialization can retry without reloading or re-laying out assets");
        dispatch.uploadError = "injected_runtime_upload_failure";
        const ModernProductUiRuntimeResult submitted = runtime.render(
            session(ProductScreen::MainMenu), presentation(),
            {true, false, false});
        expect(submitted.errorCode == "injected_runtime_upload_failure" &&
            submitted.canonicalSha256.size() == 64 &&
            !submitted.gpuSubmitted,
            "per-frame GPU error returns overlay evidence and explicit dispatch failure");
    }
}

void testGlyphEvidenceCounterfactualAndAtlasBinding()
{
    {
        FakeDispatch dispatch;
        ModernProductUiRuntime shipping(dispatch);
        expect(shipping.initialize(config()).ok &&
            !shipping.setGlyphEvidenceSuppressed(true).ok,
            "shipping defaults reject glyph suppression without evidence retention");
    }

    FakeDispatch dispatch;
    ModernProductUiRuntime runtime(dispatch);
    ModernProductUiRuntimeConfig evidenceConfig = config();
    evidenceConfig.retainSubmittedTextEvidence = true;
    expect(runtime.initialize(evidenceConfig).ok &&
        runtime.resize(1920, 1080, 144.0f).ok,
        "glyph evidence fixture initializes only with explicit retention");
    const auto snapshot = session(ProductScreen::MainMenu);
    const auto authored = presentation();
    const ModernProductUiRuntimeResult normal = runtime.render(
        snapshot, authored, {true, false, false});
    const auto normalVertices = dispatch.lastVertices;
    expect(normal.ok && !normal.glyphEvidenceSuppressed &&
        normal.glyphAtlasBindingVerified &&
        !normal.fontAtlasCacheKey.empty() &&
        !normal.glyphQuads.empty() &&
        normal.glyphAtlasRecordSha256.size() == normal.glyphQuads.size() &&
        std::all_of(normal.glyphAtlasRecordSha256.begin(),
            normal.glyphAtlasRecordSha256.end(), [](const auto& value) {
                return value.size() == 64;
            }),
        "normal frame binds every retained glyph identity to exact atlas texels: error=" +
            normal.errorCode + " glyphs=" +
            std::to_string(normal.glyphQuads.size()) + " hashes=" +
            std::to_string(normal.glyphAtlasRecordSha256.size()) +
            " atlas=" + normal.fontAtlasCacheKey);

    expect(runtime.setGlyphEvidenceSuppressed(true).ok,
        "authorized native evidence can enable paired glyph suppression");
    const ModernProductUiRuntimeResult suppressed = runtime.render(
        snapshot, authored, {true, false, false});
    expect(suppressed.ok && suppressed.glyphEvidenceSuppressed &&
        suppressed.canonicalSha256 == normal.canonicalSha256 &&
        suppressed.fontAtlasCacheKey == normal.fontAtlasCacheKey &&
        suppressed.glyphAtlasRecordSha256 ==
            normal.glyphAtlasRecordSha256 &&
        suppressed.gpuMetrics.sdfVertices < normal.gpuMetrics.sdfVertices &&
        suppressed.gpuMetrics.suppressedGlyphPrimitives ==
            normal.glyphQuads.size() &&
        suppressed.gpuMetrics.suppressedIconLabels == 0 &&
        suppressed.gpuMetrics.solidVertices == normal.gpuMetrics.solidVertices &&
        dispatch.lastVertices.size() < normalVertices.size(),
        "no-glyph pair preserves authored frame identity and removes only SDF submissions");
    expect(runtime.setGlyphEvidenceSuppressed(false).ok,
        "paired glyph suppression restores explicitly");
    const ModernProductUiRuntimeResult restored = runtime.render(
        snapshot, authored, {true, false, false});
    expect(restored.ok && !restored.glyphEvidenceSuppressed &&
        restored.canonicalSha256 == normal.canonicalSha256 &&
        sameVertices(dispatch.lastVertices, normalVertices),
        "restored normal frame reproduces exact submitted GPU vertices");

    SdfFontAtlas atlas;
    expect(atlas.initialize(evidenceConfig.font).ok,
        "independent frozen atlas initializes for identity mutation tests");
    expect(auditSubmittedGlyphAtlasBindings(atlas, normal.textRuns,
            normal.glyphQuads).ok,
        "unaltered glyph evidence revalidates against frozen atlas");
    std::size_t second = 1;
    while (second < normal.glyphQuads.size() &&
        normal.glyphQuads[second].codepoint ==
            normal.glyphQuads.front().codepoint)
        ++second;
    expect(second < normal.glyphQuads.size(),
        "identity fixture contains distinct glyphs");
    auto wrongCodepoint = normal.glyphQuads;
    wrongCodepoint.front().codepoint = wrongCodepoint[second].codepoint;
    expect(!auditSubmittedGlyphAtlasBindings(
            atlas, normal.textRuns, wrongCodepoint).ok,
        "swapping a glyph identity without its source ordinal fails closed");
    auto wrongUv = normal.glyphQuads;
    wrongUv.front().u0 = wrongUv[second].u0;
    wrongUv.front().v0 = wrongUv[second].v0;
    wrongUv.front().u1 = wrongUv[second].u1;
    wrongUv.front().v1 = wrongUv[second].v1;
    expect(!auditSubmittedGlyphAtlasBindings(
            atlas, normal.textRuns, wrongUv).ok,
        "swapping atlas UVs while retaining text identity fails closed");
    auto jointlyMoved = normal.glyphQuads;
    jointlyMoved.front().sourceRect.x += 3.0f;
    jointlyMoved.front().rect.x += 3.0f;
    jointlyMoved.front().glyphInkSourceRect.x += 3.0f;
    jointlyMoved.front().glyphInkRect.x += 3.0f;
    expect(!auditSubmittedGlyphAtlasBindings(
            atlas, normal.textRuns, jointlyMoved).ok,
        "moving clipped and source geometry together still fails run-derived placement");
    auto jointlyScaled = normal.glyphQuads;
    jointlyScaled.front().sourceRect.width *= 0.8f;
    jointlyScaled.front().rect.width *= 0.8f;
    jointlyScaled.front().glyphInkSourceRect.width *= 0.8f;
    jointlyScaled.front().glyphInkRect.width *= 0.8f;
    expect(!auditSubmittedGlyphAtlasBindings(
            atlas, normal.textRuns, jointlyScaled).ok,
        "scaling clipped and source geometry together still fails exact atlas geometry");
}

}  // namespace

int main()
{
    try {
        testHiddenModesNeedNoAssetsOrGpu();
        testOneCallShellHudLifecycle();
        testIndependentInsetChromeAtlasLifecycle();
        testDynamicTrainingFramesBypassStaticShellCache();
        testGlyphEvidenceCounterfactualAndAtlasBinding();
        testAssetAndGpuFailures();
        std::cout << "Modern Product UI runtime facade checks passed\n";
        return EXIT_SUCCESS;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return EXIT_FAILURE;
    }
}
