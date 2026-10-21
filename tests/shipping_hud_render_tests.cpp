#include "product_hud.h"
#include "render/ui/hud_draw_list.h"
#include "render/ui/hud_shipping_tokens.h"
#include "render/ui/sdf_font_atlas.h"
#include "render/ui/sdf_generator.h"
#include "render/ui/system_font_resolver.h"

#include <algorithm>
#include <cmath>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <stdexcept>
#include <string>
#include <vector>

namespace {

using namespace billiardgl::render::ui;
namespace product_hud = billiardgl::product_hud;

void expect(bool value, const std::string& message)
{
    if (!value) throw std::runtime_error(message);
}

FontAtlasConfig knownFontConfig()
{
    FontAtlasConfig config;
    config.fontPath = "/System/Library/Fonts/Menlo.ttc";
    config.faceIndex = 0;
    config.nominalPixelSize = 24;
    config.scaleBucket = 1.0f;
    config.atlasWidth = 1024;
    config.atlasHeight = 1024;
    config.glyphPadding = 1;
    config.sdfSpread = 6;
    return config;
}

bool contains(const product_hud::HudRect& outer,
    const product_hud::HudRect& inner)
{
    return inner.x >= outer.x - 0.01f && inner.y >= outer.y - 0.01f &&
        inner.x + inner.width <= outer.x + outer.width + 0.01f &&
        inner.y + inner.height <= outer.y + outer.height + 0.01f;
}

product_hud::ProductHudViewModel viewModel()
{
    product_hud::ProductHudViewModel model;
    model.player = {true, "Player 2"};
    model.group = {true, "STRIPES"};
    model.groupShapeToken = "group_stripe_banded_ring";
    model.targetBall = {true, "Ball 12"};
    model.targetPocket = {true, "Any legal pocket"};
    model.powerPercent = {true, "68%"};
    model.estimatedCueSpeed = {true, "EST. 2.28 m/s"};
    model.tipOffset = {true, "X -0.25 / Y 0.50"};
    model.camera = {true, "Player camera"};
    model.assist = {true, "Basic assist"};
    model.foulReason = {true, "The wrong ball was contacted first."};
    model.foulResolution = {true, "Opponent has ball in hand."};
    model.result = {true, "Shot complete"};
    model.ruleNotices.push_back({"wrong_first_contact",
        "The wrong ball was contacted first.", "wrong_ball",
        "numbered_ball_cross"});
    model.helpLines = {"QUICK CONTROLS", "TAB  Enter aim",
        "MOUSE LEFT  Release shot", "ARROWS  Tip offset",
        "ARROWS  Orbit camera", "W / S  Zoom", "H  Close help"};
    return model;
}

void testSdfDeterminismAndDistance()
{
    CoverageBitmap source;
    source.width = 3;
    source.height = 3;
    source.stride = 3;
    source.pixels.assign(9, 0);
    source.pixels[4] = 255;
    const SdfBitmap first = generateSignedDistanceField(source, 2);
    const SdfBitmap second = generateSignedDistanceField(source, 2);
    expect(first.ok && second.ok && first.width == 7 && first.height == 7 &&
        first.pixels == second.pixels,
        "SDF generation is byte deterministic");
    const auto at = [&](int x, int y) {
        return first.pixels[static_cast<std::size_t>(y * first.width + x)];
    };
    expect(at(3, 3) > 128 && at(2, 3) < 128 &&
        at(3, 2) == at(3, 4) && at(2, 3) == at(4, 3) &&
        at(0, 0) == 1,
        "SDF sign, distance falloff, and symmetry are stable");
    expect(!generateSignedDistanceField({}, 2).ok &&
        !generateSignedDistanceField(source, 0).ok,
        "SDF rejects invalid bitmap and spread");
}

void testFontResolverAtlasAndFailures(SdfFontAtlas& atlas)
{
    const auto resolved = resolveMacSystemHudFont("monospace");
    expect(resolved.ok && resolved.path.rfind("/System/Library/Fonts/", 0) == 0 &&
        std::filesystem::is_regular_file(resolved.path),
        "macOS resolver returns an installed path without repository copying");
    const FontAtlasConfig config = knownFontConfig();
    const auto initialized = atlas.initialize(config);
    expect(initialized.ok && atlas.initialized() && atlas.frozen() &&
        atlas.rasterizationCount() ==
            static_cast<int>(requiredHudCodepoints().size()) &&
        atlas.findGlyph('A') != nullptr && atlas.findGlyph('9') != nullptr &&
        atlas.findGlyph(0x2014) != nullptr,
        "FreeType initialization pre-bakes required Latin, digits, and symbols");
    expect(atlas.cacheKey().find(config.fontPath) == 0 &&
        !atlas.pixels().empty(),
        "atlas cache key includes path/face/size/scale and owns SDF pixels");
    const auto ones = atlas.measureUtf8("111.11", 24.0f, true);
    const auto eights = atlas.measureUtf8("888.88", 24.0f, true);
    expect(ones.ok && eights.ok && ones.width == eights.width,
        "tabular digit measurement is stable across numeric values");

    SdfFontAtlas deterministicA;
    SdfFontAtlas deterministicB;
    const std::vector<std::uint32_t> subset{'A', 'B', '0', '1', '%'};
    expect(deterministicA.initialize(config, subset).ok &&
        deterministicB.initialize(config, subset).ok &&
        deterministicA.pixels() == deterministicB.pixels() &&
        deterministicA.findGlyph('B')->atlasX ==
            deterministicB.findGlyph('B')->atlasX,
        "FreeType raster plus shelf packing produces a deterministic atlas");

    FontAtlasConfig overflow = config;
    overflow.atlasWidth = 16;
    overflow.atlasHeight = 16;
    SdfFontAtlas overflowAtlas;
    const auto overflowResult = overflowAtlas.initialize(overflow, {'W'});
    expect(!overflowResult.ok &&
        overflowResult.errorCode == "font_atlas_overflow",
        "atlas overflow is explicit");

    SdfFontAtlas missing;
    const auto missingResult = missing.initialize(config, {0x10ffff});
    expect(!missingResult.ok &&
        missingResult.errorCode.find("font_missing_glyph_U+") == 0,
        "missing glyph is explicit during pre-bake");

    const std::filesystem::path badPath =
        std::filesystem::temp_directory_path() / "billiardgl_bad_font.ttf";
    {
        std::ofstream output(badPath, std::ios::binary | std::ios::trunc);
        output << "not a font";
    }
    FontAtlasConfig bad = config;
    bad.fontPath = badPath.string();
    SdfFontAtlas badAtlas;
    const auto badResult = badAtlas.initialize(bad, {'A'});
    expect(!badResult.ok && badResult.errorCode == "font_face_load_failed",
        "bad font data fails explicitly");
    std::error_code ignored;
    std::filesystem::remove(badPath, ignored);

    FontAtlasConfig missingFile = config;
    missingFile.fontPath = "/tmp/billiardgl_font_does_not_exist.ttf";
    expect(SdfFontAtlas{}.initialize(missingFile, {'A'}).errorCode ==
        "font_file_not_found",
        "missing font path fails explicitly");
    FontAtlasConfig badFace = config;
    badFace.faceIndex = 9999;
    expect(SdfFontAtlas{}.initialize(badFace, {'A'}).errorCode ==
        "font_face_load_failed",
        "invalid configured face index fails explicitly");
}

void testTipMarkerFollowsSnapshot(const SdfFontAtlas& atlas)
{
    const auto manifest = product_hud::loadProductHudManifest(
        std::string(BILLIARDGL_SOURCE_ROOT) +
        "/assets/modern/hud/focused_practice_hud_v1.hud.json");
    expect(manifest.ok, "tip marker regression loads manifest");
    product_hud::ResponsiveHudLayoutSolver solver(manifest);
    const struct { int width, height; float dpi; } sizes[] = {
        {1280, 720, 96.0f}, {1920, 1080, 96.0f}, {2560, 1440, 192.0f}};
    const float offsets[][2] = {
        {0.0f, 0.0f}, {0.8f, 0.0f}, {-0.8f, 0.0f},
        {0.0f, 0.8f}, {0.0f, -0.8f}, {-0.25f, 0.5f}};
    for (const auto& size : sizes) for (float scale : {0.85f, 1.0f, 1.35f}) {
        const auto layout = solver.solve({size.width, size.height, size.dpi,
            scale, product_hud::HudRuntimeState::Aiming});
        for (const auto& offset : offsets) {
            auto model = viewModel();
            model.state = product_hud::HudRuntimeState::Aiming;
            model.tipOffsetX = offset[0];
            model.tipOffsetY = offset[1];
            const auto draw = HudDrawListBuilder{}.buildProduct(model, layout, atlas);
            expect(draw.ok, "tip marker layout is valid");
            const auto find = [&](const char* token) {
                return std::find_if(draw.primitives.begin(), draw.primitives.end(),
                    [&](const auto& primitive) { return primitive.token == token; });
            };
            const auto ball = find("tip_ball_outline");
            const auto marker = find("tip_position_marker");
            expect(ball != draw.primitives.end() && marker != draw.primitives.end(),
                "valid tip has ball outline and position marker");
            const auto& b = ball->sourceRect;
            const auto& m = marker->sourceRect;
            expect(std::fabs((m.x + m.width * 0.5f) -
                    (b.x + b.width * (0.5f + offset[0] * 0.5f))) < 0.01f &&
                std::fabs((m.y + m.height * 0.5f) -
                    (b.y + b.height * (0.5f - offset[1] * 0.5f))) < 0.01f,
                "marker center follows signed normalized tip, not fixed center");
            expect(contains(b, m), "supported tip marker stays inside ball outline");
        }
        auto unavailable = viewModel();
        unavailable.tipOffset.available = false;
        const auto draw = HudDrawListBuilder{}.buildProduct(unavailable, layout, atlas);
        expect(std::none_of(draw.primitives.begin(), draw.primitives.end(),
            [](const auto& p) { return p.token == "tip_position_marker"; }),
            "unavailable tip does not masquerade as a center hit");
    }
}

void testDrawLists(const SdfFontAtlas& atlas)
{
    const auto manifest = product_hud::loadProductHudManifest(
        std::string(BILLIARDGL_SOURCE_ROOT) +
        "/assets/modern/hud/focused_practice_hud_v1.hud.json");
    expect(manifest.ok, "shipping draw list loads HUD manifest");
    product_hud::ResponsiveHudLayoutSolver solver(manifest);
    HudDrawListBuilder builder;
    const HudShippingTokenSet tokens = loadHudShippingTokenSet(
        std::string(BILLIARDGL_SOURCE_ROOT) +
        "/assets/modern/hud/focused_practice_hud_shipping_tokens_v1.json");
    expect(tokens.ok, "shipping draw test loads the strict token contract");
    const struct Case {
        int width;
        int height;
        float dpi;
        float scale;
    } cases[] = {{1024, 768, 96.0f, 1.0f},
        {1280, 720, 96.0f, 1.0f}, {1920, 1080, 96.0f, 1.0f},
        {1920, 1080, 144.0f, 1.0f}};
    const int rasterizedAtInitialization = atlas.rasterizationCount();
    for (const Case& size : cases) {
        const auto layout = solver.solve({size.width, size.height, size.dpi,
            size.scale, product_hud::HudRuntimeState::Aiming});
        auto model = viewModel();
        model.state = product_hud::HudRuntimeState::Aiming;
        const HudDrawList list = builder.buildProduct(model, layout, atlas);
        expect(list.ok && list.glyphQuadCount > 0 &&
            list.atlasRasterizationCount == rasterizedAtInitialization &&
            atlas.rasterizationCount() == rasterizedAtInitialization &&
            !list.containsDebug,
            "ordinary product frame uses frozen atlas with no rasterization/debug");
        const auto expanded = expandShippingHudIcons(list, tokens,
            hudPaletteModeFor(
                billiardgl::settings::ColorVisionMode::Default));
        expect(expanded.ok && expanded.draw.ok &&
            std::none_of(expanded.draw.primitives.begin(),
                expanded.draw.primitives.end(), [](const UiPrimitive& value) {
                    return value.kind == UiPrimitiveKind::Icon;
                }) && !expanded.draw.triangles.empty(),
            "real aiming HUD replaces approved Icons with CPU triangles");
        const product_hud::HudRect safe{layout.safeAreaPhysical.left,
            layout.safeAreaPhysical.top,
            static_cast<float>(size.width) - layout.safeAreaPhysical.left -
                layout.safeAreaPhysical.right,
            static_cast<float>(size.height) - layout.safeAreaPhysical.top -
                layout.safeAreaPhysical.bottom};
        bool panel = false;
        bool icon = false;
        bool line = false;
        bool powerRect = false;
        bool glyph = false;
        for (const UiPrimitive& primitive : list.primitives) {
            expect(contains(safe, primitive.rect) &&
                contains(safe, primitive.clipRect),
                "draw primitive and clip remain inside resolved safe area");
            if (primitive.kind == UiPrimitiveKind::GlyphQuad) {
                glyph = true;
                expect(primitive.u0 >= 0.0f && primitive.v0 >= 0.0f &&
                    primitive.u1 <= 1.0f && primitive.v1 <= 1.0f &&
                    primitive.u0 <= primitive.u1 && primitive.v0 <= primitive.v1,
                    "glyph quad has normalized ordered atlas coordinates");
            } else if (primitive.kind == UiPrimitiveKind::Panel) panel = true;
            else if (primitive.kind == UiPrimitiveKind::Icon) icon = true;
            else if (primitive.kind == UiPrimitiveKind::Line) line = true;
            else if (primitive.kind == UiPrimitiveKind::SolidRect) powerRect = true;
        }
        expect(panel && icon && line && powerRect && glyph,
            "aiming draw list contains panel/icon/line/power/glyph primitives");
    }

    const auto foulLayout = solver.solve({1280, 720, 96.0f, 1.0f,
        product_hud::HudRuntimeState::Foul});
    auto foulModel = viewModel();
    foulModel.state = product_hud::HudRuntimeState::Foul;
    const HudDrawList foulDraw = builder.buildProduct(
        foulModel, foulLayout, atlas);
    const auto hasFoulBanner = [](const HudDrawList& draw) {
        return std::any_of(draw.primitives.begin(), draw.primitives.end(),
            [](const UiPrimitive& primitive) {
                return primitive.token == "foul_banner_panel";
            });
    };
    expect(foulDraw.ok && expandShippingHudIcons(foulDraw, tokens,
        HudPaletteMode::ColorVisionSafe).ok && hasFoulBanner(foulDraw),
        "foul text/alias icon expands and exposes the real foul banner selector");
    auto ordinaryModel = viewModel();
    ordinaryModel.state = product_hud::HudRuntimeState::Default;
    const auto ordinaryLayout = solver.solve({1280, 720, 96.0f, 1.0f,
        product_hud::HudRuntimeState::Default});
    const HudDrawList ordinaryDraw = builder.buildProduct(
        ordinaryModel, ordinaryLayout, atlas);
    expect(ordinaryDraw.ok && !hasFoulBanner(ordinaryDraw),
        "non-foul HUD cannot be miscounted as a foul error indicator");
    const auto helpLayout = solver.solve({1024, 768, 96.0f, 1.0f,
        product_hud::HudRuntimeState::Help});
    auto helpModel = viewModel();
    helpModel.state = product_hud::HudRuntimeState::Help;
    const auto verifyContentSizedHelp = [](const HudDrawList& helpDraw,
            const product_hud::HudLayoutResult& helpLayout) {
        const auto helpPanel = std::find_if(helpDraw.primitives.begin(),
            helpDraw.primitives.end(), [](const UiPrimitive& primitive) {
            return primitive.kind == UiPrimitiveKind::Panel &&
                primitive.token == "help_panel";
        });
        const std::vector<std::string> helpLines{
            "QUICK CONTROLS", "TAB  Enter aim",
            "MOUSE LEFT  Release shot", "ARROWS  Tip offset",
            "ARROWS  Orbit camera", "W / S  Zoom", "H  Close help"};
        expect(helpDraw.ok && helpPanel != helpDraw.primitives.end() &&
            helpDraw.textRuns.size() == helpLines.size(),
            "help draw list contains one content-sized panel and all seven pre-baked lines");
        float longestWidth = 0.0f;
        std::size_t longestIndex = 0;
        for (std::size_t index = 0; index < helpLines.size(); ++index) {
            const auto& text = helpDraw.textRuns[index];
            expect(text.text == helpLines[index] &&
                text.measuredWidth <= text.clipRect.width + 0.02f,
                "help lines preserve their authoritative order without truncation");
            if (text.measuredWidth > longestWidth) {
                longestWidth = text.measuredWidth;
                longestIndex = index;
            }
            if (index > 0) {
                expect(std::fabs(text.baselineY -
                        helpDraw.textRuns[index - 1].baselineY -
                        helpLayout.minimumTextPhysicalPx * 1.8f) <= 0.02f,
                    "help baselines use the sealed 1.8-em line spacing");
            }
        }
        const auto& firstHelpLine = helpDraw.textRuns.front();
        const auto& lastHelpLine = helpDraw.textRuns.back();
        const auto& longestHelpLine = helpDraw.textRuns[longestIndex];
        const float leftInset = longestHelpLine.originX - helpPanel->sourceRect.x;
        const float rightInset = helpPanel->sourceRect.x +
            helpPanel->sourceRect.width - longestHelpLine.originX - longestWidth;
        const float topInset = firstHelpLine.baselineY -
            firstHelpLine.requestedPixelSize - helpPanel->sourceRect.y;
        const float bottomInset = helpPanel->sourceRect.y +
            helpPanel->sourceRect.height - lastHelpLine.baselineY;
        expect(std::fabs(leftInset - 18.0f) <= 1.1f &&
            rightInset >= 18.0f &&
            rightInset <= 18.0f +
                helpLayout.minimumTextPhysicalPx * 2.0f + 1.1f &&
            std::fabs(topInset - 18.0f) <= 1.1f &&
            std::fabs(bottomInset - 18.0f) <= 1.1f,
            "help panel dimensions follow the real longest SDF line and seven-line vertical extent");
    };
    const HudDrawList helpDraw = builder.buildProduct(
        helpModel, helpLayout, atlas);
    verifyContentSizedHelp(helpDraw, helpLayout);

    auto reboundHelpModel = helpModel;
    reboundHelpModel.helpLines[1] = "GAMEPADRIGHTSTICK  Enter aim";
    const auto reboundHelpLayout = solver.solve({1024, 768, 96.0f, 1.0f,
        product_hud::HudRuntimeState::Help,
        product_hud::longestHelpLineCharacters(reboundHelpModel.helpLines)});
    const HudDrawList reboundHelpDraw = builder.buildProduct(
        reboundHelpModel, reboundHelpLayout, atlas);
    expect(reboundHelpDraw.ok && reboundHelpDraw.textRuns.size() == 7 &&
        reboundHelpDraw.textRuns[1].text ==
            "GAMEPADRIGHTSTICK  Enter aim" &&
        reboundHelpDraw.textRuns[1].measuredWidth <=
            reboundHelpDraw.textRuns[1].clipRect.width + 0.02f &&
        reboundHelpLayout.elements.at("help_panel").width >
            helpLayout.elements.at("help_panel").width,
        "shipping Help renders a genuine longer rebound label and expands its safe content panel");

    for (product_hud::HudRuntimeState state : {
            product_hud::HudRuntimeState::Default,
            product_hud::HudRuntimeState::Aiming,
            product_hud::HudRuntimeState::PostShot,
            product_hud::HudRuntimeState::Foul,
            product_hud::HudRuntimeState::Help}) {
        auto stateModel = viewModel();
        stateModel.state = state;
        const auto stateLayout = solver.solve({1920, 1080, 96.0f, 1.0f,
            state});
        const HudDrawList stateDraw = builder.buildProduct(
            stateModel, stateLayout, atlas);
        const HudIconExpansionResult stateExpanded = expandShippingHudIcons(
            stateDraw, tokens, HudPaletteMode::Standard);
        expect(stateDraw.ok && stateExpanded.ok &&
            std::none_of(stateExpanded.draw.primitives.begin(),
                stateExpanded.draw.primitives.end(), [](const UiPrimitive& value) {
                    return value.kind == UiPrimitiveKind::Icon;
                }), "all five actual Product HUD states have no opaque Icon after CPU expansion");
    }

    const struct MatrixViewport { int width; int height; float dpi; } matrix[] = {
        {1280, 720, 96.0f}, {1920, 1080, 96.0f},
        {1920, 1080, 144.0f}};
    for (const MatrixViewport& viewport : matrix) {
        for (const float userScale : {0.85f, 1.0f, 1.35f}) {
            for (const product_hud::HudRuntimeState state : {
                    product_hud::HudRuntimeState::Default,
                    product_hud::HudRuntimeState::Aiming,
                    product_hud::HudRuntimeState::PostShot,
                    product_hud::HudRuntimeState::Foul,
                    product_hud::HudRuntimeState::Help}) {
                auto matrixModel = viewModel();
                matrixModel.state = state;
                const auto matrixLayout = solver.solve({viewport.width,
                    viewport.height, viewport.dpi, userScale, state});
                const HudDrawList matrixDraw = builder.buildProduct(
                    matrixModel, matrixLayout, atlas);
                const float expectedMinimum = 14.0f *
                    (viewport.dpi / 96.0f) * userScale;
                expect(matrixDraw.ok && std::all_of(matrixDraw.textRuns.begin(),
                    matrixDraw.textRuns.end(), [&](const TextRunMetrics& text) {
                        return text.requestedPixelSize + 0.01f >= expectedMinimum &&
                            text.measuredWidth <= text.clipRect.width + 0.02f;
                    }), "exact Phase3 HUD viewport/scale/state matrix has no text truncation or compact-font downgrade");
                if (state == product_hud::HudRuntimeState::Help)
                    verifyContentSizedHelp(matrixDraw, matrixLayout);
            }
        }
    }

    // The rack-only result is persistent: no shot-ended pulse is required.
    // Exercise its actual copy, not the shorter generic foul fixture above.
    billiardgl::PresentationSnapshot rackSnapshot;
    rackSnapshot.rules.enabled = true;
    rackSnapshot.rules.rackEnded = true;
    rackSnapshot.rules.rackWinner = 1;
    rackSnapshot.rules.foul = true;
    rackSnapshot.rules.reasonCodes = {"mobile_device_repeat_loss_of_rack"};
    const auto rackModel = product_hud::HudStateReducer::reduce(rackSnapshot);
    for (const MatrixViewport& viewport : matrix) {
        for (const float userScale : {0.85f, 1.0f, 1.35f}) {
            const auto layout = solver.solve({viewport.width, viewport.height,
                viewport.dpi, userScale, rackModel.state});
            const auto draw = builder.buildProduct(rackModel, layout, atlas);
            expect(draw.ok && std::all_of(draw.textRuns.begin(), draw.textRuns.end(),
                [](const TextRunMetrics& text) {
                    return text.measuredWidth <= text.clipRect.width + 0.02f;
                }), "rack-terminal HUD copy fits every shipping viewport/scale");
            expect(std::any_of(draw.textRuns.begin(), draw.textRuns.end(),
                [&](const TextRunMetrics& text) {
                    return text.text == rackModel.foulResolution.text;
                }), "rack winner resolution survives actual draw-list generation");
        }
    }

    auto missingModel = viewModel();
    missingModel.player.text = "Player \xe4\xb8\xad\xe6\x96\x87";
    const auto defaultLayout = solver.solve({1280, 720, 96.0f, 1.0f,
        product_hud::HudRuntimeState::Default});
    const HudDrawList missing = builder.buildProduct(
        missingModel, defaultLayout, atlas);
    expect(!missing.ok && missing.errorCode == "font_glyph_not_prebaked" &&
        atlas.rasterizationCount() == rasterizedAtInitialization,
        "ordinary frame reports unbaked glyph without rasterizing it");

    const auto debug = product_hud::buildDebugOverlayViewModel(true, false);
    const HudDrawList releaseDebug = builder.buildDebug(
        debug, defaultLayout, atlas, true);
    const HudDrawList developmentDebug = builder.buildDebug(
        debug, defaultLayout, atlas, false);
    expect(releaseDebug.ok && !releaseDebug.containsDebug &&
        releaseDebug.primitives.empty() && developmentDebug.ok &&
        developmentDebug.containsDebug && !developmentDebug.primitives.empty(),
        "release draw list excludes debug while development keeps it separate");
}

}  // namespace

int main()
{
    try {
        testSdfDeterminismAndDistance();
        SdfFontAtlas atlas;
        testFontResolverAtlasAndFailures(atlas);
        testTipMarkerFollowsSnapshot(atlas);
        testDrawLists(atlas);
        std::cout << "FreeType SDF atlas and shipping HUD draw-list checks passed\n";
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
