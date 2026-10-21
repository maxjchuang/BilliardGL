#include "render/ui/product_ui_frame_coordinator.h"
#include "render/modern/modern_product_session.h"
#include "render/modern/trajectory_inset_atomic_transaction.h"

#include <algorithm>
#include <cstdlib>
#include <fstream>
#include <sstream>
#include <iostream>
#include <stdexcept>
#include <string>
#include <vector>

namespace {

using namespace billiardgl;
using namespace billiardgl::render::ui;

void expect(bool value, const std::string& message)
{
    if (!value) throw std::runtime_error("product UI coordinator: " + message);
}

std::string asset(const char* relative)
{
    return std::string(BILLIARDGL_SOURCE_ROOT) + relative;
}

SdfFontAtlas makeAtlas()
{
    FontAtlasConfig config;
    config.fontPath = "/System/Library/Fonts/Menlo.ttc";
    config.nominalPixelSize = 32;
    config.atlasWidth = 1024;
    config.atlasHeight = 1024;
    SdfFontAtlas atlas;
    const FontAtlasResult loaded = atlas.initialize(config);
    expect(loaded.ok && atlas.frozen(), "frozen SDF atlas loads");
    return atlas;
}

HudShippingTokenSet loadHudTokens()
{
    return loadHudShippingTokenSet(asset(
        "/assets/modern/hud/focused_practice_hud_shipping_tokens_v1.json"));
}

HudShippingTokenSet loadShellTokens()
{
    return loadShellShippingTokenSet(asset(
        "/assets/modern/hud/focused_practice_shell_shipping_tokens_v1.json"));
}

ProductShellSnapshot shellSnapshot(ProductScreen screen)
{
    ProductShellSnapshot shell;
    shell.screen = screen;
    shell.screenId = "coordinator_test";
    shell.title = "BILLIARDGL";
    shell.subtitle = "Focused practice";
    shell.breadcrumb = "PRODUCT / TEST";
    shell.footer = "Enter Confirm   Esc Back";
    shell.uiScale = 1.0f;
    ShellItemViewModel item;
    item.id = "test_item";
    item.label = "Practice";
    item.value = "Ready";
    item.detail = "Deterministic shell control";
    item.shapeToken = screen == ProductScreen::ReplayPlayback ?
        "pause_bars" : screen == ProductScreen::TrainingSession ?
        "reset_arrow" : "camera_frame";
    item.focused = true;
    shell.items.push_back(item);
    if (screen == ProductScreen::TrainingSession)
        shell.training.state = training::TrainingProductState::AttemptActive;
    return shell;
}

PresentationSnapshot presentation()
{
    PresentationSnapshot value;
    value.hud.currentPlayerNumber = 1;
    value.hud.viewportWidth = 1920;
    value.hud.viewportHeight = 1080;
    value.hud.aimMode = true;
    value.hud.showPowerMeter = true;
    value.hud.shotPower = 0.64f;
    value.hud.followCamera = true;
    value.shotControl.requestedPower = 64.0f;
    value.rules.enabled = true;
    value.rules.rulesetId = "wpa_heyball_2025_08_16_v1";
    value.rules.currentPlayer = 0;
    value.rules.groups = {{"solid", "stripe"}};
    return value;
}

ProductUiFrameRequest request(ProductScreen screen)
{
    ProductUiFrameRequest value;
    value.environment.productMode = true;
    value.shell = shellSnapshot(screen);
    value.settings = settings::ProductSettings::defaults();
    value.viewport = {1280, 720, 1920, 1080, 144.0f};
    value.presentation = presentation();
    return value;
}

bool containsIcon(const HudDrawList& draw)
{
    return std::any_of(draw.primitives.begin(), draw.primitives.end(),
        [](const UiPrimitive& primitive) {
            return primitive.kind == UiPrimitiveKind::Icon;
        });
}

bool containsPrimitiveToken(const HudDrawList& draw,
    const std::string& token)
{
    return std::any_of(draw.primitives.begin(), draw.primitives.end(),
        [&](const UiPrimitive& primitive) {
            return primitive.token == token;
        });
}

bool containsText(const HudDrawList& draw, const std::string& text)
{
    return std::any_of(draw.textRuns.begin(), draw.textRuns.end(),
        [&](const TextRunMetrics& run) { return run.text == text; });
}

bool textFitsClip(const HudDrawList& draw, const std::string& text)
{
    const auto found = std::find_if(draw.textRuns.begin(),
        draw.textRuns.end(), [&](const TextRunMetrics& run) {
            return run.text == text;
        });
    return found != draw.textRuns.end() &&
        found->measuredWidth <= found->clipRect.width + 0.02f;
}

bool textFitsAtLeast(const HudDrawList& draw, const std::string& text,
    float minimumPixelSize)
{
    const auto found = std::find_if(draw.textRuns.begin(),
        draw.textRuns.end(), [&](const TextRunMetrics& run) {
            return run.text == text;
        });
    return found != draw.textRuns.end() &&
        found->requestedPixelSize >= minimumPixelSize &&
        found->measuredWidth <= found->clipRect.width + 0.02f;
}

bool glyphIdentitiesMatchTextRuns(const HudDrawList& draw)
{
    for (const UiPrimitive& primitive : draw.primitives) {
        if (primitive.kind != UiPrimitiveKind::GlyphQuad) continue;
        if (primitive.textRunIndex >= draw.textRuns.size()) {
            std::cerr << "glyph run index out of range: " <<
                primitive.textRunIndex << '/' << draw.textRuns.size() << '\n';
            return false;
        }
        std::vector<std::uint32_t> codepoints;
        std::string error;
        if (!decodeUtf8(draw.textRuns[primitive.textRunIndex].text,
            codepoints, error) ||
            primitive.codepointOrdinal >= codepoints.size() ||
            codepoints[primitive.codepointOrdinal] != primitive.codepoint) {
            std::cerr << "glyph identity mismatch: run=" <<
                primitive.textRunIndex << " ordinal=" <<
                primitive.codepointOrdinal << " actual=" <<
                primitive.codepoint << " text=" <<
                draw.textRuns[primitive.textRunIndex].text << '\n';
            return false;
        }
    }
    return true;
}

bool containsTriangleSource(const HudDrawList& draw,
    const std::string& token)
{
    return std::any_of(draw.triangles.begin(), draw.triangles.end(),
        [&](const UiTriangle& triangle) {
            return triangle.sourceToken == token;
        });
}

product_hud::HudRect triangleBounds(const UiTriangle& triangle)
{
    const float left = std::min({triangle.first.x, triangle.second.x,
        triangle.third.x});
    const float right = std::max({triangle.first.x, triangle.second.x,
        triangle.third.x});
    const float top = std::min({triangle.first.y, triangle.second.y,
        triangle.third.y});
    const float bottom = std::max({triangle.first.y, triangle.second.y,
        triangle.third.y});
    return {left, top, right - left, bottom - top};
}

const UiPrimitive* primitiveWithToken(const HudDrawList& draw,
    const std::string& token)
{
    const auto found = std::find_if(draw.primitives.begin(),
        draw.primitives.end(), [&](const UiPrimitive& primitive) {
            return primitive.token == token;
        });
    return found == draw.primitives.end() ? nullptr : &*found;
}

float overlapArea(const product_hud::HudRect& first,
    const product_hud::HudRect& second)
{
    const float x0 = std::max(first.x, second.x);
    const float y0 = std::max(first.y, second.y);
    const float x1 = std::min(first.x + first.width,
        second.x + second.width);
    const float y1 = std::min(first.y + first.height,
        second.y + second.height);
    return std::max(0.0f, x1 - x0) * std::max(0.0f, y1 - y0);
}

void testNineScreenRoutingAndDeterminism()
{
    const product_hud::ProductHudManifest manifest =
        product_hud::loadProductHudManifest(asset(
        "/assets/modern/hud/focused_practice_hud_v1.hud.json"));
    const SdfFontAtlas atlas = makeAtlas();
    const HudShippingTokenRegistry registry = mergeHudShippingTokenSets(
        {loadHudTokens(), loadShellTokens()});
    expect(manifest.ok && registry.ok && registry.tokenSets.size() == 2,
        "strict manifest and explicit two-namespace registry load");
    ProductUiFrameCoordinator coordinator(manifest, atlas, registry);

    const ProductScreen screens[] = {ProductScreen::MainMenu,
        ProductScreen::SinglePlayerSetup, ProductScreen::Match,
        ProductScreen::TrainingCatalog, ProductScreen::TrainingSession,
        ProductScreen::ReplayBrowser, ProductScreen::ReplayPlayback,
        ProductScreen::Settings, ProductScreen::Help};
    for (ProductScreen screen : screens) {
        const ProductUiFrame first = coordinator.build(request(screen));
        const ProductUiFrame second = coordinator.build(request(screen));
        const ProductUiOverlayKind expected = screen == ProductScreen::Match ?
            ProductUiOverlayKind::GameplayHud : ProductUiOverlayKind::Shell;
        expect(first.ok && first.draw.ok && first.overlay == expected &&
            first.overlay != ProductUiOverlayKind::None &&
            !first.draw.triangles.empty() && !containsIcon(first.draw) &&
            !first.canonicalEvidence.empty() &&
            first.evidenceSha256.size() == 64 &&
            first.canonicalEvidence == second.canonicalEvidence &&
            first.evidenceSha256 == second.evidenceSha256,
            "each of nine screens selects exactly one overlay and emits deterministic zero-Icon CPU evidence");
    }

    ProductUiFrameRequest cvdRequest = request(ProductScreen::ReplayPlayback);
    const ProductUiFrame standard = coordinator.build(cvdRequest);
    cvdRequest.settings.colorVisionMode =
        settings::ColorVisionMode::Deuteranopia;
    const ProductUiFrame cvd = coordinator.build(cvdRequest);
    expect(standard.ok && cvd.ok &&
        standard.palette == HudPaletteMode::Standard &&
        cvd.palette == HudPaletteMode::ColorVisionSafe &&
        standard.evidenceSha256 != cvd.evidenceSha256,
        "settings authority selects distinct standard/CVD Shell evidence");

    ProductUiFrameRequest scaled = request(ProductScreen::Match);
    const ProductUiFrame base = coordinator.build(scaled);
    scaled.settings.uiScale = 1.35;
    scaled.shell.uiScale = 1.35f;
    const ProductUiFrame large = coordinator.build(scaled);
    expect(base.ok && large.ok && base.evidenceSha256 != large.evidenceSha256,
        "settings UI scale is consumed by gameplay HUD layout evidence");

    ProductUiFrameRequest compactOpen = request(ProductScreen::Match);
    compactOpen.viewport = {640, 360, 1280, 720, 192.0f};
    compactOpen.presentation.rules.groups = {{"open", "open"}};
    const ProductUiFrame compactOpenFrame = coordinator.build(compactOpen);
    const bool compactGlyphInkUnclipped = std::all_of(
        compactOpenFrame.draw.primitives.begin(),
        compactOpenFrame.draw.primitives.end(), [](const UiPrimitive& value) {
            if (value.kind != UiPrimitiveKind::GlyphQuad) return true;
            const auto& ink = value.glyphInkSourceRect;
            const auto& clip = value.clipRect;
            return ink.width > 0.0f && ink.height > 0.0f &&
                ink.x >= clip.x - 0.01f && ink.y >= clip.y - 0.01f &&
                ink.x + ink.width <= clip.x + clip.width + 0.01f &&
                ink.y + ink.height <= clip.y + clip.height + 0.01f &&
                std::fabs(value.glyphInkRect.x - ink.x) <= 0.01f &&
                std::fabs(value.glyphInkRect.y - ink.y) <= 0.01f &&
                std::fabs(value.glyphInkRect.width - ink.width) <= 0.01f &&
                std::fabs(value.glyphInkRect.height - ink.height) <= 0.01f;
        });
    expect(compactOpenFrame.ok &&
        textFitsClip(compactOpenFrame.draw, "Any legal object ball") &&
        textFitsClip(compactOpenFrame.draw, "Any legal pocket") &&
        !containsText(compactOpenFrame.draw,
            "Any legal object ball / Any legal pocket") &&
        compactGlyphInkUnclipped,
        "physical 1280x720 Retina HUD splits the open-table target and pocket without clipping glyphs");

    ProductUiFrameRequest help = request(ProductScreen::Match);
    ProductUiFrameRequest terminal = request(ProductScreen::Match);
    terminal.presentation.rules.rackEnded = true;
    terminal.presentation.rules.rackWinner = 1;
    terminal.presentation.rules.foul = true;
    terminal.presentation.rules.reasonCodes = {"mobile_device_repeat_loss_of_rack"};
    terminal.presentation.hud.aimMode = true;
    const auto terminalFrame = coordinator.build(terminal);
    expect(terminalFrame.ok && terminalFrame.overlay == ProductUiOverlayKind::GameplayHud &&
        containsText(terminalFrame.draw,
            "FOUL: Repeated mobile-device disturbance: loss of rack.") &&
        containsText(terminalFrame.draw, "Rack ended — Player 2 wins this rack.") &&
        !containsIcon(terminalFrame.draw),
        "actual Match coordinator emits persistent rack result through shipping HUD tokens");
    const auto freshFrame = coordinator.build(request(ProductScreen::Match));
    expect(freshFrame.ok && !containsText(freshFrame.draw,
        "Rack ended — Player 2 wins this rack."),
        "coordinator does not cache a previous rack result across fresh Match frames");
    help.presentation.hud.showHelp = true;
    const ProductUiFrame defaultHelp = coordinator.build(help);
    const std::vector<std::string> expectedDefault{
        "QUICK CONTROLS", "TAB  Enter aim", "MOUSE LEFT  Release shot",
        "ARROWS  Tip offset", "ARROWS  Orbit camera", "W / S  Zoom",
        "H  Close help"};
    expect(defaultHelp.ok && defaultHelp.draw.textRuns.size() == 7 &&
        std::equal(expectedDefault.begin(), expectedDefault.end(),
            defaultHelp.draw.textRuns.begin(),
            [](const std::string& expected, const TextRunMetrics& actual) {
                return expected == actual.text;
            }),
        "coordinator renders the seven truthful default controls from effective settings");

    for (auto& binding : help.settings.bindings) {
        if (binding.action == "toggle_aim") {
            binding.control = "GamepadRightStick";
            break;
        }
    }
    const ProductUiFrame reboundHelp = coordinator.build(help);
    expect(reboundHelp.ok && reboundHelp.draw.textRuns.size() == 7 &&
        reboundHelp.draw.textRuns[1].text ==
            "GAMEPADRIGHTSTICK  Enter aim" &&
        reboundHelp.draw.textRuns[1].measuredWidth <=
            reboundHelp.draw.textRuns[1].clipRect.width + 0.02f &&
        reboundHelp.evidenceSha256 != defaultHelp.evidenceSha256,
        "coordinator consumes a current rebound control, reflows Help, and changes canonical evidence");
}

void testSuppressionAndExplicitFailures()
{
    const product_hud::ProductHudManifest manifest =
        product_hud::loadProductHudManifest(asset(
        "/assets/modern/hud/focused_practice_hud_v1.hud.json"));
    const SdfFontAtlas atlas = makeAtlas();
    const HudShippingTokenSet hud = loadHudTokens();
    const HudShippingTokenSet shell = loadShellTokens();
    const HudShippingTokenRegistry registry = mergeHudShippingTokenSets(
        {hud, shell});
    ProductUiFrameCoordinator coordinator(manifest, atlas, registry);

    for (int hidden = 0; hidden < 3; ++hidden) {
        ProductUiFrameRequest value = request(ProductScreen::MainMenu);
        if (hidden == 0) value.environment.productMode = false;
        if (hidden == 1) value.environment.automation = true;
        if (hidden == 2) value.environment.windowless = true;
        const ProductUiFrame frame = coordinator.build(value);
        expect(frame.ok && frame.overlay == ProductUiOverlayKind::None &&
            frame.draw.primitives.empty() && frame.draw.triangles.empty() &&
            !frame.canonicalEvidence.empty(),
            "non-product, automation, and windowless modes suppress overlays deterministically");
    }

    const HudShippingTokenRegistry hudOnly = mergeHudShippingTokenSets({hud});
    ProductUiFrameCoordinator missingShell(manifest, atlas, hudOnly);
    expect(missingShell.build(request(ProductScreen::MainMenu)).errorCode ==
        "product_ui_shell_token_set_unavailable",
        "Shell route hard-fails when its exact set was not registered");
    const HudShippingTokenRegistry shellOnly =
        mergeHudShippingTokenSets({shell});
    ProductUiFrameCoordinator missingHud(manifest, atlas, shellOnly);
    expect(missingHud.build(request(ProductScreen::Match)).errorCode ==
        "product_ui_hud_token_set_unavailable",
        "Match HUD hard-fails when its exact set was not registered");

    ProductUiFrameRequest mismatch = request(ProductScreen::Settings);
    mismatch.shell.uiScale = 1.2f;
    expect(coordinator.build(mismatch).errorCode ==
        "product_ui_scale_mismatch",
        "session/settings scale drift is explicit");
    ProductUiFrameRequest invalidViewport = request(ProductScreen::Match);
    invalidViewport.viewport.physicalWidth = 0;
    expect(coordinator.build(invalidViewport).errorCode ==
        "product_ui_viewport_invalid",
        "invalid viewport is explicit before layout");
}

void testSessionSnapshotRequestBridge()
{
    render::ModernProductSessionSnapshot session;
    session.shell = shellSnapshot(ProductScreen::ReplayPlayback);
    session.shellViewport = {1280, 720, 1920, 1080, 144.0f};
    session.effectiveSettings = settings::ProductSettings::defaults();
    session.effectiveSettings.uiScale = 1.15;
    session.effectiveSettings.colorVisionMode =
        settings::ColorVisionMode::Tritanopia;
    const ProductUiFrameEnvironment environment{true, false, false};
    const PresentationSnapshot sourcePresentation = presentation();
    const ProductUiFrameRequest bridged = makeProductUiFrameRequest(session,
        sourcePresentation, environment);
    expect(bridged.shell.screen == ProductScreen::ReplayPlayback &&
        bridged.viewport.physicalWidth == 1920 &&
        bridged.viewport.physicalHeight == 1080 &&
        bridged.settings.uiScale == 1.15 &&
        bridged.settings.colorVisionMode ==
            settings::ColorVisionMode::Tritanopia &&
        bridged.presentation.hud.shotPower ==
            sourcePresentation.hud.shotPower &&
        bridged.environment.productMode && !bridged.environment.automation &&
        !bridged.environment.windowless,
        "frame request consumes current session settings, viewport, shell, and presentation state");
}

void testPersistentTrainingControlHud()
{
    const product_hud::ProductHudManifest manifest =
        product_hud::loadProductHudManifest(asset(
            "/assets/modern/hud/focused_practice_hud_v1.hud.json"));
    const SdfFontAtlas atlas = makeAtlas();
    const HudShippingTokenRegistry registry = mergeHudShippingTokenSets(
        {loadHudTokens(), loadShellTokens()});
    ProductUiFrameCoordinator coordinator(manifest, atlas, registry);

    ProductUiFrameRequest expanded = request(ProductScreen::TrainingSession);
    expanded.shell.inspectorMode = 1;
    expanded.presentation.shotControl.requestedPower = 64.0f;
    expanded.presentation.shotControl.tipOffsetX = 0.25f;
    expanded.presentation.shotControl.tipOffsetY = -0.50f;
    expanded.presentation.hud.aimMode = true;
    expanded.presentation.ballsMoving = false;
    auto& hints = expanded.shell.training.teachingHints;
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
    expanded.presentation.camera.eye = {0.0f, 420.0f, 0.0f};
    expanded.presentation.camera.target = {0.0f,
        kTableModelPlaySurfaceHeightCm, 0.0f};
    expanded.presentation.camera.fieldOfViewDegrees = 48.0f;
    expanded.shell.training.powerAdjustmentAllowed = true;
    expanded.shell.training.tipAdjustmentAllowed = false;
    expanded.shell.training.aimAdjustmentAllowed = true;
    const ProductUiFrame aim = coordinator.build(expanded);
    const UiPrimitive* aimPanel = primitiveWithToken(aim.draw,
        "training_control_panel");
    const ShellScreenLayout expandedLayout = buildShippingScreenLayout(
        expanded.shell, expanded.viewport);
    expect(glyphIdentitiesMatchTextRuns(aim.draw),
        "merged Shell and Training draw lists preserve exact glyph-to-text-run identity");
    expect(aim.ok && aim.overlay == ProductUiOverlayKind::Shell &&
        aimPanel != nullptr && containsText(aim.draw, "POWER 32%") &&
        containsText(aim.draw, "TIP X +0.25") &&
        containsText(aim.draw, "Y -0.50") &&
        containsText(aim.draw, "MODE AIM") &&
        containsText(aim.draw, "SHOT SETUP") &&
        containsText(aim.draw, "READY") &&
        containsText(aim.draw, "REC 22.5-37.5%") &&
        containsText(aim.draw, "LOCKED") && textFitsClip(aim.draw, "LOCKED") &&
        containsPrimitiveToken(aim.draw,
            "training_power_recommended_range") &&
        containsTriangleSource(aim.draw, "tip_target") &&
        !containsIcon(aim.draw) && expandedLayout.ok &&
        overlapArea(aimPanel->sourceRect,
            expandedLayout.inspectorToggleRect) == 0.0f,
        "TrainingSession draws current power/tip and ready Aim mode in a bottom strip outside the panel toggle");

    const bool hasGoalGeometry = std::any_of(aim.draw.triangles.begin(),
        aim.draw.triangles.end(), [](const UiTriangle& triangle) {
            return triangle.sourceToken.rfind("training_goal_", 0) == 0;
        });
    bool goalAvoidsChrome = true;
    for (const UiTriangle& triangle : aim.draw.triangles) {
        if (triangle.sourceToken.rfind("training_goal_", 0) != 0) continue;
        for (const UiPrimitive& primitive : aim.draw.primitives) {
            if (primitive.kind != UiPrimitiveKind::Panel &&
                primitive.kind != UiPrimitiveKind::SolidRect) continue;
            if (overlapArea(triangleBounds(triangle), primitive.rect) > 0.0f)
                goalAvoidsChrome = false;
        }
    }
    expect(hasGoalGeometry && goalAvoidsChrome,
        "typed Training goal geometry survives integration but never intersects any panel or solid control footprint");

    ProductUiFrameRequest collapsed = expanded;
    collapsed.shell.inspectorMode = 2;
    collapsed.presentation.hud.aimMode = false;
    const ProductUiFrame observe = coordinator.build(collapsed);
    expect(observe.ok &&
        containsPrimitiveToken(observe.draw, "training_control_panel") &&
        containsText(observe.draw, "POWER 32%") &&
        containsText(observe.draw, "TIP X +0.25") &&
        containsText(observe.draw, "Y -0.50") &&
        containsText(observe.draw, "MODE OBSERVE") &&
        containsText(observe.draw, "VIEW ONLY") &&
        containsText(observe.draw, "REC 22.5-37.5%") &&
        containsText(observe.draw, "LOCKED"),
        "explicitly hiding the Training inspector cannot hide the persistent control HUD and Observe is visibly locked");

    ProductUiFrameRequest motion = collapsed;
    motion.presentation.hud.aimMode = true;
    motion.presentation.ballsMoving = true;
    const ProductUiFrame moving = coordinator.build(motion);
    expect(moving.ok && containsText(moving.draw, "MODE MOTION") &&
        containsText(moving.draw, "SHOT ACTIVE") &&
        moving.evidenceSha256 != observe.evidenceSha256,
        "ball motion has priority over Aim and exposes a locked Motion state");

    ProductUiFrameRequest evaluated = expanded;
    evaluated.shell.training.state =
        training::TrainingProductState::AttemptEvaluated;
    const ProductUiFrame finished = coordinator.build(evaluated);
    expect(finished.ok && containsText(finished.draw, "MODE AIM") &&
        containsText(finished.draw, "ATTEMPT DONE") &&
        containsText(finished.draw, "REC 22.5-37.5%") &&
        containsText(finished.draw, "LOCKED"),
        "an evaluated attempt remains visibly locked even if presentation still reports Aim mode");

    ProductUiFrameRequest changed = expanded;
    changed.presentation.shotControl.requestedPower = 118.0f;
    changed.presentation.shotControl.tipOffsetX = -0.65f;
    changed.presentation.shotControl.tipOffsetY = 0.40f;
    const ProductUiFrame updated = coordinator.build(changed);
    expect(updated.ok && containsText(updated.draw, "POWER 59%") &&
        containsText(updated.draw, "TIP X -0.65") &&
        containsText(updated.draw, "Y +0.40") &&
        updated.evidenceSha256 != aim.evidenceSha256,
        "the Training HUD consumes each current presentation shot-control value rather than a shell or scenario copy");

    for (const bool retina : {false, true}) {
        ProductUiFrameRequest compact = collapsed;
        compact.viewport = retina ? ShellViewport{640, 400, 1280, 800,
            192.0f} : ShellViewport{1024, 640, 1024, 640, 96.0f};
        const ProductUiFrame frame = coordinator.build(compact);
        const UiPrimitive* panel = primitiveWithToken(frame.draw,
            "training_control_panel");
        expect(frame.ok && panel != nullptr &&
            panel->sourceRect.x >= frame.draw.safeRect.x &&
            panel->sourceRect.y >= frame.draw.safeRect.y &&
            panel->sourceRect.x + panel->sourceRect.width <=
                frame.draw.safeRect.x + frame.draw.safeRect.width + 0.02f &&
            panel->sourceRect.y + panel->sourceRect.height <=
                frame.draw.safeRect.y + frame.draw.safeRect.height + 0.02f &&
            containsText(frame.draw, "MODE OBSERVE") &&
            containsText(frame.draw, "VIEW ONLY") &&
            textFitsAtLeast(frame.draw, "LOCKED", retina ? 26.0f : 13.0f) &&
            textFitsAtLeast(frame.draw, "REC 22.5-37.5%",
                retina ? 26.0f : 13.0f) &&
            textFitsAtLeast(frame.draw, "TIP X +0.25",
                retina ? 26.0f : 13.0f) &&
            textFitsAtLeast(frame.draw, "Y -0.50",
                retina ? 26.0f : 13.0f),
            retina ?
                "Retina compact Training window retains a complete in-safe-area control strip" :
                "ordinary compact Training window retains a complete in-safe-area control strip");
    }

    const ProductUiFrame match = coordinator.build(request(ProductScreen::Match));
    expect(match.ok &&
        !containsPrimitiveToken(match.draw, "training_control_panel"),
        "the persistent Training control strip is not drawn on Match or other screens");
}

}  // namespace

void testInsetOneShotExecutionLifecycle()
{
    using billiardgl::render::modern::TrajectoryInsetVisibleResult;
    TrajectoryInsetVisibleCurrentFrame current;
    current.ok = true;
    TrajectoryInsetVisibleProductCallbacks callbacks;
    int executions=0, commits=0, invalidations=0;
    callbacks.executeIntegration = [&](const auto&) {
        ++executions;
        expect(current.executing && !current.consumed,
            "final staging sees a live executing, not already consumed, frame");
        const auto nested=executeTrajectoryInsetVisibleCurrentFrame(current,callbacks);
        expect(!nested.ok && nested.errorCode=="trajectory_inset_visible_current_frame_reused",
            "reentrant use is rejected before a second callback");
        TrajectoryInsetVisibleResult result;
        result.ok=result.presented=true;
        return result;
    };
    callbacks.submitChrome=[](const auto&,const auto&,std::string&){return true;};
    callbacks.commitVisible=[&](){++commits;};
    callbacks.invalidate=[&](){++invalidations;};
    expect(executeTrajectoryInsetVisibleCurrentFrame(current,callbacks).ok &&
        current.consumed && !current.executing && executions==1 && commits==1,
        "successful frame transitions exactly once from executing to consumed");
    expect(!executeTrajectoryInsetVisibleCurrentFrame(current,callbacks).ok && executions==1,
        "successful frame cannot be retried");
    current={}; current.ok=true;
    callbacks.executeIntegration=[&](const auto&){
        ++executions;
        TrajectoryInsetVisibleResult result;
        result.errorCode="injected_failure";
        return result;
    };
    expect(!executeTrajectoryInsetVisibleCurrentFrame(current,callbacks).ok &&
        current.consumed && !current.executing && invalidations==1,
        "failed frame is invalidated and consumed");
    current={}; current.ok=true;
    callbacks.executeIntegration=[&](const auto&)->TrajectoryInsetVisibleResult {
        ++executions; throw std::runtime_error("injected_exception");
    };
    bool threw=false;
    try { (void)executeTrajectoryInsetVisibleCurrentFrame(current,callbacks); }
    catch(const std::runtime_error&) { threw=true; }
    const int attempted=executions;
    expect(threw && current.consumed && !current.executing &&
        !executeTrajectoryInsetVisibleCurrentFrame(current,callbacks).ok && executions==attempted,
        "exceptional attempt remains consumed and cannot be replayed");
}

int main()
{
    try {
        {
            std::ifstream input(std::string(BILLIARDGL_SOURCE_ROOT)+
                "/tests/fixtures/trajectory_inset_retrace_20260930.txt");
            expect(input.good(), "retrace fixture is readable");
            billiardgl::render::ModernProductSessionSnapshot session;
            session.shell.screen=ProductScreen::ReplayPlayback;
            std::string tag,remainder;
            input>>tag>>session.shellViewport.physicalWidth>>session.shellViewport.physicalHeight>>session.shell.uiScale;
            expect(tag=="FIX_LAYOUT","retrace layout tag");
            std::getline(input,remainder);
            session.shellViewport.logicalWidth=session.shellViewport.physicalWidth;
            session.shellViewport.logicalHeight=session.shellViewport.physicalHeight;
            session.shellViewport.dpi=96;
            for(int trackIndex=0;trackIndex<2;++trackIndex){
                int id;std::size_t count;
                input>>tag>>id;
                expect(tag=="FIX_TRACK"&&id==trackIndex,"retrace source track order");
                auto& track=trackIndex?session.shell.trajectoryOverlay.actual:session.shell.trajectoryOverlay.predicted;
                input>>track.visible>>track.endpointVisible>>track.endpoint.x>>track.endpoint.y
                    >>track.endpoint.z>>track.railCollisionEvents>>count;
                for(std::size_t i=0;i<count;++i){
                    training::OverlaySample sample;
                    input>>tag>>sample.tick>>sample.position.x>>sample.position.y>>sample.position.z>>sample.visible;
                    expect(tag=="FIX_SAMPLE","retrace sample tag");
                    track.samples.push_back(sample);
                }
            }
            input>>tag;expect(input.good()&&tag=="FIX_END","retrace source complete");
            TrajectoryInsetVisibleCurrentContext context;
            context.present=context.sessionStable=context.insetToggleOn=true;
            context.sessionSerial=context.frameSerial=context.layoutSerial=1;
            context.cp1GenerationId="v19_generation_1";
            context.sourceContentSha256=trajectoryInsetSourceContentSha256(session.shell);
            const auto current=makeTrajectoryInsetVisibleCurrentFrame(session,presentation(),{true,false,false},context);
            expect(current.ok,"retrace product package: "+current.errorCode);
            expect(std::any_of(current.package.probes.begin(),current.package.probes.end(),
                [](const auto& probe){return !probe.sourceCoreMemberKey.empty();}),
                "real retraced event uses source/evidence provenance");
            expect(current.request.cp1Digests[1]==current.package.orderedProbeDigest,
                "coordinator must preserve CP1 event-visit source/evidence in the probe digest");
        }
        testInsetOneShotExecutionLifecycle();
        {
            billiardgl::render::ModernProductSessionSnapshot session;
            session.shell.screen = ProductScreen::TrainingSession;
            session.shellViewport = {1280,720,1280,720,96.0f};
            session.shell.trajectoryOverlay.actual.visible = false;
            session.shell.trajectoryOverlay.predicted.samples = {
                {0,{0,0,0},true},{1,{30,0,50},true}};
            TrajectoryInsetVisibleCurrentContext context;
            context.present = context.sessionStable = context.insetToggleOn = true;
            context.sessionSerial = context.frameSerial = context.layoutSerial = 1;
            context.cp1GenerationId = "v19_generation_1";
            context.sourceContentSha256 = trajectoryInsetSourceContentSha256(session.shell);
            const auto current = makeTrajectoryInsetVisibleCurrentFrame(session,
                presentation(), {true,false,false}, context);
            expect(current.ok, "actual inset package for digest bridge: " + current.errorCode);
            billiardgl::render::modern::TrajectoryInsetAtomicIdentityContract identity;
            identity.cp1GenerationId=current.package.generationId;
            identity.frameSerial=identity.cp2LayoutSerial=1;
            identity.embeddedCp1Digests={{current.package.orderedParentSourceDigest,
                current.package.orderedProbeDigest,current.package.orderedDashEventDigest,
                current.package.orderedDashContributorDigest,current.package.orderedSourceRouteIntervalDigest}};
            identity.expectedCp1Digests=identity.recomputedCp1Digests=current.request.cp1Digests;
            identity.expectedCp1GeometryDigest=current.request.cp1GeometryDigest;
            identity.recomputedCp1GeometryDigest=canonicalTrajectoryInsetGeometryDigest(current.package);
            identity.expectedCp2LayoutDigest=current.request.cp2LayoutDigest;
            identity.recomputedCp2LayoutDigest=canonicalTrajectoryInsetLayoutDigest(current.layout);
            identity.sourceGroups=current.package.sourceGroupDenominator;
            identity.mappingRows=current.package.mappingRows.size();
            identity.orderedParents=current.package.orderedParentSourceKeys.size();
            identity.semanticMembers=current.package.semanticBatches.size();
            identity.semanticDenominator=current.package.semanticMemberDenominator;
            identity.probes=current.package.probes.size();
            identity.probeDenominator=current.package.probeDenominator;
            identity.orderedProbeIds=current.package.orderedProbeIds.size();
            identity.generationContentDeepValid=true;
            std::string error;
            expect(billiardgl::render::modern::validateTrajectoryInsetAtomicIdentity(identity,error),
                "coordinator package and native canonical digests agree: " + error);
            auto changed=current.package;
            changed.vertices.front().xPhysicalPx+=1.0f;
            identity.recomputedCp1GeometryDigest=canonicalTrajectoryInsetGeometryDigest(changed);
            expect(!billiardgl::render::modern::validateTrajectoryInsetAtomicIdentity(identity,error) &&
                error=="trajectory_inset_content_digest_identity_invalid",
                "mutated actual inset geometry is rejected with unchanged expected digest");
        }
        {
            billiardgl::render::ModernProductSessionSnapshot session;
            session.shell.screen = ProductScreen::TrainingSession;
            TrajectoryInsetVisibleCurrentContext context;
            context.sourceContentSha256 = trajectoryInsetSourceContentSha256(session.shell);
            session.shell.trajectoryOverlay.actual.railCollisionEvents = 1;
            const auto stale = makeTrajectoryInsetVisibleCurrentFrame(session,
                presentation(), {true,false,false}, context);
            expect(!stale.ok && stale.errorCode == "trajectory_inset_source_content_mismatch",
                "changed current trajectory cannot reuse old source binding");
        }
        testNineScreenRoutingAndDeterminism();
        testSuppressionAndExplicitFailures();
        testSessionSnapshotRequestBridge();
        testPersistentTrainingControlHud();
        std::cout << "Product UI CPU frame coordinator checks passed\n";
        return EXIT_SUCCESS;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return EXIT_FAILURE;
    }
}
