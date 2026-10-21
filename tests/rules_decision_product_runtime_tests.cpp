#include "render/modern/modern_product_ui_runtime.h"
#include "render/modern/camera_projection.h"
#include "table_specs.h"

#include <algorithm>
#include <iostream>
#include <limits>
#include <set>
#include <stdexcept>
#include <string>
#include <vector>

namespace {

using namespace billiardgl;
using namespace billiardgl::render;
using namespace billiardgl::render::ui;

void expect(bool value, const std::string& message)
{
    if (!value) throw std::runtime_error("rules decision product UI: " + message);
}

std::string asset(const char* relative)
{
    return std::string(BILLIARDGL_SOURCE_ROOT) + relative;
}

std::vector<std::uint32_t> rulesCodepoints()
{
    const auto hud = requiredHudCodepoints();
    std::set<std::uint32_t> unique(hud.begin(), hud.end());
    for (const auto& text : allRulesDecisionUiStrings()) {
        std::vector<std::uint32_t> decoded;
        std::string error;
        expect(decodeUtf8(text, decoded, error), "rules copy is valid UTF-8");
        unique.insert(decoded.begin(), decoded.end());
    }
    return {unique.begin(), unique.end()};
}

SdfFontAtlas atlas()
{
    FontAtlasConfig config;
    config.fontPath = "/System/Library/Fonts/Hiragino Sans GB.ttc";
    config.nominalPixelSize = 32;
    config.atlasWidth = 4096;
    config.atlasHeight = 4096;
    SdfFontAtlas value;
    const auto initialized = value.initialize(config, rulesCodepoints());
    expect(initialized.ok, "CJK production SDF atlas initializes");
    return value;
}

ModernProductSessionSnapshot sessionSnapshot(bool armed = false)
{
    ModernProductSessionSnapshot session;
    session.shell.screen = ProductScreen::Match;
    session.shell.screenId = "phase5_rules_decision_internal";
    session.shell.uiScale = 1.0f;
    session.effectiveSettings = settings::ProductSettings::defaults();
    session.shellViewport = {1280, 720, 1280, 720, 96.0f};
    session.rulesDecisionUiImplementationEnabled = true;
    session.rulesDecisionUiProductExposed = false;
    session.rulesDecisionLocale = RulesDecisionLocale::En;
    session.rulesDecision.state = armed ?
        rules::ui::DecisionUiState::ActionArmedNotSubmitted :
        rules::ui::DecisionUiState::DecisionOpenUnselected;
    session.rulesDecision.offer.rulesetId =
        rules::ui::kRulesDecisionUiRulesetId;
    session.rulesDecision.offer.capabilityId =
        rules::ui::kGroupChoiceCapabilityId;
    session.rulesDecision.offer.flowId = rules::ui::kGroupChoiceFlowId;
    session.rulesDecision.offer.branchId =
        "two_group_simultaneous_first_contact_and_pocket";
    session.rulesDecision.offer.decisionId = "rules.group_choice.42.1";
    session.rulesDecision.offer.authorityStateRevision = 42;
    session.rulesDecision.offer.actorPlayerId = 0;
    session.rulesDecision.offer.offeredActionIds = {
        rules::kGroupChoiceSolidsActionId,
        rules::kGroupChoiceStripesActionId};
    session.rulesDecision.visibleActionIds =
        session.rulesDecision.offer.offeredActionIds;
    if (armed)
        session.rulesDecision.armedActionId =
            rules::kGroupChoiceSolidsActionId;
    if (armed)
        session.rulesDecision.focusedActionId =
            rules::kGroupChoiceSolidsActionId;
    session.rulesDecision.shotInputBlocked = true;
    return session;
}

ModernProductSessionSnapshot placementSession(bool behindHeadString,
    bool valid, RulesDecisionLocale locale = RulesDecisionLocale::En)
{
    ModernProductSessionSnapshot session = sessionSnapshot();
    session.rulesDecisionLocale = locale;
    session.rulesDecision.offer.capabilityId = rules::kFreeBallPlacementCapabilityId;
    session.rulesDecision.offer.flowId = behindHeadString ?
        rules::kBehindHeadStringBallInHandFlowId :
        rules::kFullTableBallInHandFlowId;
    session.rulesDecision.offer.branchId = behindHeadString ?
        "behind_head_string" : "full_table";
    session.rulesDecision.offer.sourceClause = behindHeadString ?
        "WPA-II-08" : "";
    session.rulesDecision.offer.offeredActionIds = {behindHeadString ?
        rules::kConfirmBehindHeadStringPlacementActionId :
        rules::kConfirmFullTablePlacementActionId};
    session.rulesDecision.visibleActionIds =
        session.rulesDecision.offer.offeredActionIds;
    session.rulesDecision.armedActionId.clear();
    session.rulesDecision.focusedActionId.clear();
    session.rulesDecision.state = valid ?
        rules::ui::DecisionUiState::PlacementPreviewLocalValid :
        rules::ui::DecisionUiState::PlacementPreviewLocalInvalid;
    session.rulesDecision.placementPreviewVisible = true;
    session.rulesDecision.placementPreviewLegal = valid;
    session.rulesDecision.placementPreview = {0.0, -36.0};
    session.rulesDecision.headStringWorldZCm = behindHeadString ?
        -kChinesePlayfieldLengthCm * 0.25 :
        std::numeric_limits<double>::quiet_NaN();
    return session;
}

ModernProductSessionSnapshot illegalBreakProductSession()
{
    ModernProductSessionSnapshot session = sessionSnapshot();
    session.rulesDecisionUiImplementationEnabled = false;
    session.rulesDecisionIllegalBreakProductEnabled = true;
    session.rulesDecisionProductActive = true;
    session.rulesDecision.offer.capabilityId = rules::kBreakFoulCapabilityId;
    session.rulesDecision.offer.flowId = rules::kBreakFoulFlowId;
    session.rulesDecision.offer.branchId = "illegal_break_6c";
    session.rulesDecision.offer.sourceClause = rules::kIllegalBreakSourceClause;
    session.rulesDecision.offer.sourcePdfPage = 7;
    session.rulesDecision.offer.offeredActionIds = {
        rules::kBreakFoulAcceptTableActionId,
        rules::kBreakFoulRerackOpponentActionId,
        rules::kBreakFoulRerackOriginalActionId};
    session.rulesDecision.visibleActionIds =
        session.rulesDecision.offer.offeredActionIds;
    return session;
}

ModernProductSessionSnapshot groupChoiceProductSession()
{
    ModernProductSessionSnapshot session = sessionSnapshot();
    session.rulesDecisionUiImplementationEnabled = false;
    session.rulesDecisionGroupChoiceProductEnabled = true;
    session.rulesDecisionProductActive = true;
    return session;
}

ModernProductSessionSnapshot fullTablePlacementProductSession(bool armed)
{
    ModernProductSessionSnapshot session = placementSession(false, true);
    session.rulesDecisionUiImplementationEnabled = false;
    session.rulesDecisionFullTableBallInHandProductEnabled = true;
    session.rulesDecisionProductActive = true;
    if (armed) {
        session.rulesDecision.state =
            rules::ui::DecisionUiState::ActionArmedNotSubmitted;
        session.rulesDecision.armedActionId =
            rules::kConfirmFullTablePlacementActionId;
        session.rulesDecision.focusedActionId =
            rules::kConfirmFullTablePlacementActionId;
    }
    return session;
}

ModernProductSessionSnapshot eightOnBreakProductSession(bool foul,
    bool armed = false)
{
    ModernProductSessionSnapshot session = sessionSnapshot();
    session.rulesDecisionUiImplementationEnabled = false;
    session.rulesDecisionEightOnBreakProductEnabled = true;
    session.rulesDecisionProductActive = true;
    session.rulesDecision.offer.capabilityId =
        rules::kEightOnBreakCapabilityId;
    session.rulesDecision.offer.flowId = rules::kEightOnBreakFlowId;
    session.rulesDecision.offer.branchId = foul ?
        "foul_break" : "legal_break";
    session.rulesDecision.offer.offeredActionIds = foul ?
        std::vector<std::string>{
            rules::kEightOnBreakSpotBallInHandActionId,
            rules::kEightOnBreakRerackOpponentActionId,
            rules::kEightOnBreakRerackOriginalActionId} :
        std::vector<std::string>{
            rules::kEightOnBreakSpotContinueActionId,
            rules::kEightOnBreakRerackOriginalActionId};
    session.rulesDecision.visibleActionIds =
        session.rulesDecision.offer.offeredActionIds;
    if (armed) {
        session.rulesDecision.state =
            rules::ui::DecisionUiState::ActionArmedNotSubmitted;
        session.rulesDecision.armedActionId =
            session.rulesDecision.visibleActionIds.front();
        session.rulesDecision.focusedActionId =
            session.rulesDecision.armedActionId;
    }
    return session;
}

ModernProductSessionSnapshot breakFoulPlacementProductSession(bool valid)
{
    ModernProductSessionSnapshot session = placementSession(true, valid);
    session.rulesDecisionUiImplementationEnabled = false;
    session.rulesDecisionBreakShotFoulProductEnabled = true;
    session.rulesDecisionProductActive = true;
    session.rulesDecision.offer.branchId = "behind_head_string_06e";
    session.rulesDecision.offer.offeredActionIds = {
        rules::kConfirmBehindHeadStringPlacementActionId,
        rules::kRequestNearestLegalRespotActionId};
    session.rulesDecision.visibleActionIds =
        session.rulesDecision.offer.offeredActionIds;
    if (!valid)
        session.rulesDecision.placementPreviewErrorCode =
            "rules_placement_outside_head_string_area";
    return session;
}

ModernProductSessionSnapshot breakFoulRespotTieProductSession(bool armed)
{
    ModernProductSessionSnapshot session = sessionSnapshot();
    session.rulesDecisionUiImplementationEnabled = false;
    session.rulesDecisionBreakShotFoulProductEnabled = true;
    session.rulesDecisionProductActive = true;
    session.rulesDecision.offer.capabilityId =
        rules::kBehindHeadStringRespotCapabilityId;
    session.rulesDecision.offer.flowId = rules::kBehindHeadStringRespotFlowId;
    session.rulesDecision.offer.branchId = "behind_head_string_respot_tie";
    session.rulesDecision.offer.sourceClause =
        rules::kBehindHeadStringSourceClause;
    session.rulesDecision.offer.offeredActionIds = {
        rules::ui::productRespotChoiceActionId(2),
        rules::ui::productRespotChoiceActionId(3)};
    session.rulesDecision.visibleActionIds =
        session.rulesDecision.offer.offeredActionIds;
    if (armed) {
        session.rulesDecision.state =
            rules::ui::DecisionUiState::ActionArmedNotSubmitted;
        session.rulesDecision.armedActionId =
            session.rulesDecision.visibleActionIds.front();
        session.rulesDecision.focusedActionId =
            session.rulesDecision.armedActionId;
    }
    return session;
}

bool hasShapeToken(const HudDrawList& draw, const std::string& token)
{
    return std::any_of(draw.triangles.begin(), draw.triangles.end(),
               [&token](const UiTriangle& item) {
                   return item.sourceToken == token && item.resolvedToken == token;
               }) ||
        std::any_of(draw.primitives.begin(), draw.primitives.end(),
            [&token](const UiPrimitive& item) { return item.token == token; });
}

std::size_t tokenTriangleCount(const HudDrawList& draw,
    const std::string& token)
{
    return static_cast<std::size_t>(std::count_if(
        draw.triangles.begin(), draw.triangles.end(),
        [&token](const UiTriangle& item) {
            return item.sourceToken == token && item.resolvedToken == token;
        }));
}

product_hud::HudRect tokenTriangleBounds(const HudDrawList& draw,
    const std::string& token)
{
    float minimumX = std::numeric_limits<float>::max();
    float minimumY = std::numeric_limits<float>::max();
    float maximumX = std::numeric_limits<float>::lowest();
    float maximumY = std::numeric_limits<float>::lowest();
    for (const auto& triangle : draw.triangles) {
        if (triangle.sourceToken != token || triangle.resolvedToken != token)
            continue;
        for (const UiPoint point : {triangle.first, triangle.second,
                                   triangle.third}) {
            minimumX = std::min(minimumX, point.x);
            minimumY = std::min(minimumY, point.y);
            maximumX = std::max(maximumX, point.x);
            maximumY = std::max(maximumY, point.y);
        }
    }
    if (minimumX == std::numeric_limits<float>::max()) return {};
    return {minimumX, minimumY, maximumX - minimumX, maximumY - minimumY};
}

std::string renderedText(const HudDrawList& draw)
{
    std::string result;
    for (const auto& item : draw.textRuns) result += item.text + " ";
    return result;
}

PresentationSnapshot presentation()
{
    PresentationSnapshot value;
    value.hud.currentPlayerNumber = 1;
    value.hud.viewportWidth = 1280;
    value.hud.viewportHeight = 720;
    value.hud.aimMode = true;
    value.hud.showPowerMeter = true;
    value.hud.shotPower = 0.5f;
    value.shotControl.requestedPower = 50.0f;
    value.rules.enabled = true;
    value.rules.rulesetId = rules::ui::kRulesDecisionUiRulesetId;
    value.rules.groups = {{"open", "open"}};
    value.camera.eye = {0.0f, 300.0f, 260.0f};
    value.camera.target = {0.0f, kTableModelPlaySurfaceHeightCm, -20.0f};
    value.camera.fieldOfViewDegrees = 48.0f;
    return value;
}

void testPhysical720RetinaIllegalBreakLayout()
{
    const SdfFontAtlas font = atlas();
    const RulesDecisionTextMeasure measure = [&font](const std::string& text,
        float pixels) {
        const auto value = font.measureUtf8(text, pixels, false);
        return RulesDecisionTextMetrics{value.ok, value.width, value.height};
    };
    RulesDecisionDisplayProfile profile;
    profile.logicalWidth = 640;
    profile.logicalHeight = 360;
    profile.physicalWidth = 1280;
    profile.physicalHeight = 720;
    profile.density = 2.0f;
    profile.uiScale = 1.0f;
    for (const char* state : {"break_illegal_open_unselected",
            "break_illegal_armed_confirm"}) {
        for (RulesDecisionLocale locale : {RulesDecisionLocale::En,
                RulesDecisionLocale::ZhCn}) {
            for (RulesDecisionPalette palette : {
                    RulesDecisionPalette::Standard,
                    RulesDecisionPalette::ColorVisionSafe}) {
                const RulesDecisionCanonicalFixture fixture =
                    rulesDecisionCanonicalFixture(state);
                const RulesDecisionDrawList draw = buildRulesDecisionDrawList(
                    fixture, locale, palette, profile, measure);
                const RulesDecisionDrawValidation validation =
                    validateRulesDecisionDrawList(draw);
                expect(validation.ok && draw.layoutBuildOk &&
                    draw.offeredActionIds.size() == 3,
                    std::string("physical 1280x720 Retina keeps all three IllegalBreak actions within unchanged panel bounds: ") +
                        state);
                const int actionItems = static_cast<int>(std::count_if(
                    draw.items.begin(), draw.items.end(), [](const auto& item) {
                        return item.kind == RulesDecisionDrawKind::Action;
                    }));
                const int confirmItems = static_cast<int>(std::count_if(
                    draw.items.begin(), draw.items.end(), [](const auto& item) {
                        return item.kind == RulesDecisionDrawKind::Confirm;
                    }));
                expect(actionItems == 3 &&
                    confirmItems == (fixture.armedActionId.empty() ? 0 : 1),
                    std::string("compact IllegalBreak preserves exact action/confirm semantics: ") +
                        state);
            }
        }
    }
}

void testProductionDrawListAndHitRectGate()
{
    const SdfFontAtlas font = atlas();
    const auto manifest = product_hud::loadProductHudManifest(asset(
        "/assets/modern/hud/focused_practice_hud_v1.hud.json"));
    const auto registry = mergeHudShippingTokenSets({
        loadHudShippingTokenSet(asset(
            "/assets/modern/hud/focused_practice_hud_shipping_tokens_v1.json")),
        loadShellShippingTokenSet(asset(
            "/assets/modern/hud/focused_practice_shell_shipping_tokens_v1.json"))});
    expect(manifest.ok && registry.ok, "shipping manifest/tokens load");
    const ProductUiFrameCoordinator builder(manifest, font, registry);
    const auto source = sessionSnapshot();
    const auto base = builder.build(makeProductUiFrameRequest(source,
        presentation(), {true, false, false, false}));
    expect(base.ok && base.overlay == ProductUiOverlayKind::GameplayHud &&
        !base.rulesDecisionActive && base.rulesDecisionHitRects.empty(),
        "normal product environment cannot expose internal rules UI: ok=" +
            std::to_string(base.ok) + " overlay=" +
            std::to_string(static_cast<int>(base.overlay)) + " error=" +
            base.errorCode);

    const auto product06c = builder.build(makeProductUiFrameRequest(
        illegalBreakProductSession(), presentation(),
        {true, false, false, false}));
    expect(product06c.ok &&
        product06c.overlay == ProductUiOverlayKind::RulesDecision &&
        product06c.rulesDecisionActive &&
        product06c.rulesDecisionHitRects.size() == 3 &&
        product06c.rulesDecisionHitRects.front().actionId ==
            rules::kBreakFoulAcceptTableActionId,
        "normal product mode exposes only the typed IllegalBreak branch");

    const auto productGroup = builder.build(makeProductUiFrameRequest(
        groupChoiceProductSession(), presentation(),
        {true, false, false, false}));
    expect(productGroup.ok &&
        productGroup.overlay == ProductUiOverlayKind::RulesDecision &&
        productGroup.rulesDecisionActive &&
        productGroup.rulesDecisionHitRects.size() == 2,
        "typed GroupChoice product capability reaches the shipping draw consumer");

    const auto productFullTable = builder.build(makeProductUiFrameRequest(
        fullTablePlacementProductSession(true), presentation(),
        {true, false, false, false}));
    expect(productFullTable.ok &&
        productFullTable.overlay == ProductUiOverlayKind::RulesDecision &&
        productFullTable.rulesDecisionActive &&
        productFullTable.rulesDecisionHitRects.size() == 2 &&
        productFullTable.rulesDecisionHitRects.back().confirm,
        "typed FullTable BIH capability renders an armed explicit Confirm "
        "within the placement occlusion budget: error=" +
            productFullTable.errorCode);

    auto overheadFullTableSource = fullTablePlacementProductSession(true);
    overheadFullTableSource.shellViewport =
        {1024, 768, 2048, 1536, 192.0f};
    PresentationSnapshot overheadPresentation = presentation();
    overheadPresentation.camera.eye =
        {0.000001f, 466.2525f, -13.261763f};
    overheadPresentation.camera.target = {0.0f, 86.48398f, 0.0f};
    overheadPresentation.camera.fieldOfViewDegrees = 40.0f;
    overheadPresentation.camera.projectionPan = {20.0f, 0.0f, 0.0f};
    const auto overheadFullTable = builder.build(makeProductUiFrameRequest(
        overheadFullTableSource, overheadPresentation,
        {true, false, false, false}));
    expect(overheadFullTable.ok &&
        overheadFullTable.overlay == ProductUiOverlayKind::RulesDecision,
        "shipping 2x overhead FullTable armed frame remains within the exact "
        "projected-table occlusion budget: error=" +
            overheadFullTable.errorCode);

    const auto legal06f = builder.build(makeProductUiFrameRequest(
        eightOnBreakProductSession(false), presentation(),
        {true, false, false, false}));
    const auto foul06f = builder.build(makeProductUiFrameRequest(
        eightOnBreakProductSession(true), presentation(),
        {true, false, false, false}));
    const auto armed06f = builder.build(makeProductUiFrameRequest(
        eightOnBreakProductSession(true, true), presentation(),
        {true, false, false, false}));
    expect(legal06f.ok && foul06f.ok && armed06f.ok &&
        legal06f.overlay == ProductUiOverlayKind::RulesDecision &&
        legal06f.rulesDecisionHitRects.size() == 2 &&
        foul06f.rulesDecisionHitRects.size() == 3 &&
        armed06f.rulesDecisionHitRects.size() == 4 &&
        armed06f.rulesDecisionHitRects.back().confirm &&
        armed06f.rulesDecisionHitRects.back().actionId ==
            rules::kEightOnBreakSpotBallInHandActionId,
        "typed 06F product branch renders exact legal/foul choices and explicit confirm");

    auto armed06cSource = illegalBreakProductSession();
    armed06cSource.rulesDecision.state =
        rules::ui::DecisionUiState::ActionArmedNotSubmitted;
    for (const std::string& action :
            armed06cSource.rulesDecision.visibleActionIds) {
        armed06cSource.rulesDecision.armedActionId = action;
        armed06cSource.rulesDecision.focusedActionId = action;
        const auto armed06c = builder.build(makeProductUiFrameRequest(
            armed06cSource, presentation(), {true, false, false, false}));
        expect(armed06c.ok && armed06c.rulesDecisionHitRects.size() == 4 &&
            armed06c.rulesDecisionHitRects.back().confirm &&
            armed06c.rulesDecisionHitRects.back().actionId == action &&
            hasShapeToken(armed06c.draw, kRulesUiFocusChevronV4Token) &&
            hasShapeToken(armed06c.draw,
                kRulesUiSelectedCheckDiamondV4Token) &&
            hasShapeToken(armed06c.draw,
                kRulesUiConfirmDoubleFrameEnterArrowV4Token) &&
            !hasShapeToken(armed06c.draw, "rules_focus_chevron"),
            "each armed IllegalBreak action gives Confirm the sole focus");
    }

    const auto open = builder.build(makeProductUiFrameRequest(source,
        presentation(), {true, false, false, true}));
    expect(open.ok && open.overlay == ProductUiOverlayKind::RulesDecision &&
        open.rulesDecisionActive && open.rulesDecisionHitRects.size() == 2 &&
        open.rulesDecisionHitRects[0].controlId ==
            std::string("action:") + rules::kGroupChoiceSolidsActionId &&
        open.rulesDecisionHitRects[1].controlId ==
            std::string("action:") + rules::kGroupChoiceStripesActionId &&
        !open.rulesDecisionHitRects[0].confirm &&
        open.evidenceSha256.size() == 64,
        "evidence capability emits exact ordered product hit rectangles");

    const auto armedSource = sessionSnapshot(true);
    const auto armed = builder.build(makeProductUiFrameRequest(armedSource,
        presentation(), {true, false, false, true}));
    expect(armed.ok && armed.rulesDecisionHitRects.size() == 3 &&
        armed.rulesDecisionHitRects.back().confirm &&
        armed.rulesDecisionHitRects.back().controlId ==
            std::string("confirm:") + rules::kGroupChoiceSolidsActionId &&
        armed.evidenceSha256 != open.evidenceSha256,
        "armed state adds a distinct Confirm hit rectangle to canonical evidence");
    const std::string armedText = renderedText(armed.draw);
    expect(armedText.find("Armed") != std::string::npos &&
        armedText.find("Confirm") != std::string::npos &&
        armedText.find("Player 1") != std::string::npos &&
        armedText.find("Choose solids") != std::string::npos &&
        hasShapeToken(armed.draw, kRulesUiFocusChevronV4Token) &&
        hasShapeToken(armed.draw, kRulesUiSelectedCheckDiamondV4Token) &&
        hasShapeToken(armed.draw,
            kRulesUiConfirmDoubleFrameEnterArrowV4Token) &&
        !hasShapeToken(armed.draw, "rules_focus_chevron") &&
        !hasShapeToken(armed.draw, "rules_selected_check_diamond") &&
        !hasShapeToken(armed.draw,
            "rules_confirm_double_frame_plus_enter_arrow"),
        "armed/confirm names exact actor/action and emits three distinct non-color tokens");
    const auto focusBounds = tokenTriangleBounds(
        armed.draw, kRulesUiFocusChevronV4Token);
    const auto diamondBounds = tokenTriangleBounds(
        armed.draw, kRulesUiSelectedCheckDiamondV4Token);
    const auto confirmBounds = tokenTriangleBounds(
        armed.draw, kRulesUiConfirmDoubleFrameEnterArrowV4Token);
    expect(tokenTriangleCount(armed.draw, kRulesUiFocusChevronV4Token) == 4 &&
        tokenTriangleCount(armed.draw,
            kRulesUiSelectedCheckDiamondV4Token) == 12 &&
        tokenTriangleCount(armed.draw,
            kRulesUiConfirmDoubleFrameEnterArrowV4Token) == 24 &&
        focusBounds.width >= 18.0f && focusBounds.height >= 18.0f &&
        diamondBounds.width >= 18.0f && diamondBounds.height >= 18.0f &&
        confirmBounds.width >= 18.0f && confirmBounds.height >= 18.0f,
        "v4 shapes are closed/bent, independently ranged, and at least 18x18 physical pixels");
    auto actorThreeSource = armedSource;
    actorThreeSource.rulesDecision.offer.actorPlayerId = 2;
    const auto actorThree = builder.build(makeProductUiFrameRequest(
        actorThreeSource, presentation(), {true, false, false, true}));
    expect(actorThree.ok && renderedText(actorThree.draw).find("Player 3") !=
            std::string::npos,
        "armed/confirm actor display derives from current stable offer actor ID");

    const auto fullValid = builder.build(makeProductUiFrameRequest(
        placementSession(false, true), presentation(),
        {true, false, false, true}));
    expect(fullValid.ok &&
        hasShapeToken(fullValid.draw, "rules_preview_ghost_cue_ball") &&
        hasShapeToken(fullValid.draw, "rules_preview_crosshair") &&
        !hasShapeToken(fullValid.draw, "rules_error_cross_octagon") &&
        !hasShapeToken(fullValid.draw,
            "rules_invalid_dashed_collision_ring") &&
        fullValid.rulesDecisionHitRects.size() == 1 &&
        fullValid.rulesDecisionHitRects.front().enabled,
        "valid placement emits ghost cue-ball plus crosshair and enables exact confirm");
    expect(!hasShapeToken(fullValid.draw, kRulesUiFocusChevronV4Token) &&
        !hasShapeToken(fullValid.draw, kRulesUiSelectedCheckDiamondV4Token) &&
        !hasShapeToken(fullValid.draw,
            kRulesUiConfirmDoubleFrameEnterArrowV4Token),
        "v4 armed-confirm tokens do not leak into placement controls");

    const auto fullInvalid = builder.build(makeProductUiFrameRequest(
        placementSession(false, false), presentation(),
        {true, false, false, true}));
    expect(fullInvalid.ok &&
        hasShapeToken(fullInvalid.draw, "rules_preview_ghost_cue_ball") &&
        !hasShapeToken(fullInvalid.draw, "rules_preview_crosshair") &&
        hasShapeToken(fullInvalid.draw, "rules_error_cross_octagon") &&
        hasShapeToken(fullInvalid.draw,
            "rules_invalid_dashed_collision_ring") &&
        fullInvalid.rulesDecisionHitRects.size() == 1 &&
        !fullInvalid.rulesDecisionHitRects.front().enabled,
        "invalid placement emits ghost/error-octagon/dashed-ring and disables confirm");

    auto behindExactSource = placementSession(true, true);
    behindExactSource.rulesDecision.placementPreview = {
        12.250000238418579, -36.500000476837158};
    const auto behindEn = builder.build(makeProductUiFrameRequest(
        behindExactSource, presentation(),
        {true, false, false, true}));
    const auto behindZh = builder.build(makeProductUiFrameRequest(
        placementSession(true, false, RulesDecisionLocale::ZhCn), presentation(),
        {true, false, false, true}));
    expect(behindEn.ok && behindZh.ok &&
        hasShapeToken(behindEn.draw, "rules_legal_region_diagonal_hatch") &&
        hasShapeToken(behindEn.draw, "rules_head_string_boundary") &&
        renderedText(behindEn.draw).find("Legal region") != std::string::npos &&
        renderedText(behindEn.draw).find("X 12.25") != std::string::npos &&
        renderedText(behindEn.draw).find("Z -36.5") != std::string::npos &&
        renderedText(behindZh.draw).find("合法区域") != std::string::npos &&
        renderedText(behindZh.draw).find("Z -36.0") != std::string::npos,
        "behind-head overlay emits hatch/boundary and bounded localized world-coordinate copy");
    const auto hatchBounds = tokenTriangleBounds(behindEn.draw,
        "rules_legal_region_diagonal_hatch");
    const SharedCameraProjection tableProjection = buildSharedCameraProjection(
        presentation().camera, 1280, 720);
    float projectedLeft = std::numeric_limits<float>::max();
    float projectedTop = std::numeric_limits<float>::max();
    float projectedRight = std::numeric_limits<float>::lowest();
    float projectedBottom = std::numeric_limits<float>::lowest();
    for (const float x : {-kChinesePlayfieldWidthCm * 0.5f +
                              kChineseBallRadiusCm,
                          kChinesePlayfieldWidthCm * 0.5f -
                              kChineseBallRadiusCm}) {
        for (const float z : {-kChinesePlayfieldLengthCm * 0.5f +
                                  kChineseBallRadiusCm,
                              -kChinesePlayfieldLengthCm * 0.25f}) {
            PhysicalScreenPoint screen;
            expect(clipToPhysicalScreen(tableProjection,
                       transformWorldToClip(tableProjection,
                           {x, kTableModelPlaySurfaceHeightCm + 0.05f, z}),
                       screen),
                "legal-region corner projects with the presentation camera");
            projectedLeft = std::min(projectedLeft,
                static_cast<float>(screen.x));
            projectedTop = std::min(projectedTop,
                static_cast<float>(screen.y));
            projectedRight = std::max(projectedRight,
                static_cast<float>(screen.x));
            projectedBottom = std::max(projectedBottom,
                static_cast<float>(screen.y));
        }
    }
    expect(hatchBounds.width > 0.0f && hatchBounds.height > 0.0f &&
            hatchBounds.x >= projectedLeft - 4.0f &&
            hatchBounds.y >= projectedTop - 4.0f &&
            hatchBounds.x + hatchBounds.width <= projectedRight + 4.0f &&
            hatchBounds.y + hatchBounds.height <= projectedBottom + 4.0f &&
            renderedText(behindEn.draw).find("Camera —") == std::string::npos,
        "placement hatch stays inside the projected table region and modal suppresses background HUD: hatch=" +
            std::to_string(hatchBounds.x) + "," +
            std::to_string(hatchBounds.y) + "," +
            std::to_string(hatchBounds.width) + "," +
            std::to_string(hatchBounds.height) + " projected=" +
            std::to_string(projectedLeft) + "," +
            std::to_string(projectedTop) + "," +
            std::to_string(projectedRight - projectedLeft) + "," +
            std::to_string(projectedBottom - projectedTop) + " text=" +
            renderedText(behindEn.draw));

    const auto productPlacementInvalid = builder.build(
        makeProductUiFrameRequest(
            breakFoulPlacementProductSession(false), presentation(),
            {true, false, false, false}));
    expect(productPlacementInvalid.ok &&
        productPlacementInvalid.overlay ==
            ProductUiOverlayKind::RulesDecision &&
        productPlacementInvalid.rulesDecisionHitRects.size() == 2 &&
        productPlacementInvalid.rulesDecisionHitRects[0].actionId ==
            rules::kConfirmBehindHeadStringPlacementActionId &&
        !productPlacementInvalid.rulesDecisionHitRects[0].enabled &&
        productPlacementInvalid.rulesDecisionHitRects[1].actionId ==
            rules::kRequestNearestLegalRespotActionId &&
        productPlacementInvalid.rulesDecisionHitRects[1].enabled &&
        productPlacementInvalid.rulesDecisionExclusionRects.size() == 1 &&
        renderedText(productPlacementInvalid.draw).find(
            "Cue ball must be behind the head string") !=
            std::string::npos,
        "06E placement shows invalid reason, blocks coordinate confirm, and keeps all-behind request available: ok=" +
            std::to_string(productPlacementInvalid.ok) + " error=" +
            productPlacementInvalid.errorCode + " hits=" +
            std::to_string(productPlacementInvalid.rulesDecisionHitRects.size()) +
            " exclusions=" + std::to_string(
                productPlacementInvalid.rulesDecisionExclusionRects.size()) +
            " text=" + renderedText(productPlacementInvalid.draw));

    const auto productPlacementValid = builder.build(
        makeProductUiFrameRequest(
            breakFoulPlacementProductSession(true), presentation(),
            {true, false, false, false}));
    expect(productPlacementValid.ok &&
        productPlacementValid.rulesDecisionHitRects.size() == 2 &&
        productPlacementValid.rulesDecisionHitRects[0].enabled &&
        productPlacementValid.rulesDecisionHitRects[1].enabled,
        "06E legal pointer preview enables explicit placement without hiding all-behind recovery");
    auto armedPlacementSource = breakFoulPlacementProductSession(true);
    armedPlacementSource.rulesDecision.state =
        rules::ui::DecisionUiState::ActionArmedNotSubmitted;
    armedPlacementSource.rulesDecision.armedActionId =
        rules::kConfirmBehindHeadStringPlacementActionId;
    armedPlacementSource.rulesDecision.focusedActionId =
        rules::kConfirmBehindHeadStringPlacementActionId;
    const auto armedPlacement = builder.build(makeProductUiFrameRequest(
        armedPlacementSource, presentation(), {true, false, false, false}));
    const auto armedPlacementConfirm = std::find_if(
        armedPlacement.rulesDecisionHitRects.begin(),
        armedPlacement.rulesDecisionHitRects.end(), [](const auto& hit) {
            return hit.confirm && hit.actionId ==
                rules::kConfirmBehindHeadStringPlacementActionId;
        });
    expect(armedPlacement.ok &&
        armedPlacement.rulesDecisionHitRects.size() == 3 &&
        armedPlacement.rulesDecisionHitRects[0].enabled &&
        armedPlacementConfirm != armedPlacement.rulesDecisionHitRects.end() &&
        armedPlacementConfirm->enabled,
        "06E armed coordinate placement keeps its full action and explicit confirm readable: ok=" +
            std::to_string(armedPlacement.ok) + " error=" +
            armedPlacement.errorCode + " hits=" + std::to_string(
                armedPlacement.rulesDecisionHitRects.size()));

    const auto tieOpen = builder.build(makeProductUiFrameRequest(
        breakFoulRespotTieProductSession(false), presentation(),
        {true, false, false, false}));
    const auto tieArmed = builder.build(makeProductUiFrameRequest(
        breakFoulRespotTieProductSession(true), presentation(),
        {true, false, false, false}));
    expect(tieOpen.ok && tieOpen.rulesDecisionHitRects.size() == 2 &&
        tieOpen.rulesDecisionHitRects[0].actionId ==
            rules::ui::productRespotChoiceActionId(2) &&
        tieOpen.rulesDecisionHitRects[1].actionId ==
            rules::ui::productRespotChoiceActionId(3) &&
        renderedText(tieOpen.draw).find("Choose ball 2 to respot") !=
            std::string::npos &&
        tieArmed.ok && tieArmed.rulesDecisionHitRects.size() == 3 &&
        tieArmed.rulesDecisionHitRects.back().confirm &&
        tieArmed.rulesDecisionHitRects.back().actionId ==
            rules::ui::productRespotChoiceActionId(2),
        "06E equidistant respot tie renders exact ball choices and a separate explicit confirm");

    auto cvdSource = source;
    cvdSource.effectiveSettings.colorVisionMode =
        settings::ColorVisionMode::Deuteranopia;
    const auto cvd = builder.build(makeProductUiFrameRequest(cvdSource,
        presentation(), {true, false, false, true}));
    expect(cvd.ok && cvd.rulesDecisionHitRects.size() ==
            open.rulesDecisionHitRects.size() &&
        cvd.rulesDecisionHitRects[0].controlId ==
            open.rulesDecisionHitRects[0].controlId &&
        cvd.evidenceSha256 != open.evidenceSha256,
        "CVD changes pixels while preserving action semantics");

    auto forbidden = source;
    forbidden.rulesDecisionUiProductExposed = true;
    expect(builder.build(makeProductUiFrameRequest(forbidden,
            presentation(), {true, false, false, true})).errorCode ==
            "rules_decision_ui_product_exposure_forbidden",
        "product exposure claim hard-fails the coordinator");
}

class FakeDispatch final : public ProductUiGlDispatch {
public:
    UiGpuResult createResources(const UiGpuResourceCreateInfo&,
        UiGpuResourceHandles& handles) override
    {
        handles = {1, 2, 3, 4};
        return {true, {}};
    }
    void destroyResources(UiGpuResourceHandles& handles) noexcept override
    {
        handles = {};
    }
    void abandonResources(UiGpuResourceHandles& handles) noexcept override
    {
        handles = {};
    }
    UiGpuResult beginFrame(const UiGpuResourceHandles&, int, int) override
    {
        return {true, {}};
    }
    UiGpuResult uploadVertices(const UiGpuResourceHandles&,
        const std::vector<UiGpuVertex>&) override { return {true, {}}; }
    UiGpuResult setScissor(const UiGpuScissor&) override { return {true, {}}; }
    UiGpuResult draw(const UiGpuResourceHandles&,
        const UiGpuBatch&) override { return {true, {}}; }
    UiGpuResult endFrame() override { return {true, {}}; }
};

void testRuntimeCapabilityAndRecordingGpuSubmit()
{
    FakeDispatch dispatch;
    ModernProductUiRuntime runtime(dispatch);
    auto config = ModernProductUiRuntimeConfig::trustedDefaults(
        BILLIARDGL_SOURCE_ROOT,
        "/System/Library/Fonts/Hiragino Sans GB.ttc");
    config.rulesDecisionUiImplementationCapability = true;
    config.font.atlasWidth = 4096;
    config.font.atlasHeight = 4096;
    expect(runtime.initialize(config).ok &&
        runtime.resize(1280, 720, 96.0f).ok,
        "internal runtime initializes CJK atlas and recording GPU backend");
    const auto rendered = runtime.render(sessionSnapshot(), presentation(),
        {true, false, false, true});
    expect(rendered.ok && rendered.gpuSubmitted &&
        rendered.overlay == ProductUiOverlayKind::RulesDecision &&
        rendered.canonicalSha256.size() == 64 &&
        rendered.gpuMetrics.drawCalls > 0 && rendered.textRuns.empty() &&
        rendered.glyphQuads.empty(),
        "production runtime submits exact rules UI draw-list without retaining evidence-only text/glyph copies");
    runtime.shutdown();

    FakeDispatch productDispatch;
    ModernProductUiRuntime productRuntime(productDispatch);
    auto productConfig = ModernProductUiRuntimeConfig::trustedDefaults(
        BILLIARDGL_SOURCE_ROOT, "/System/Library/Fonts/Menlo.ttc");
    productConfig.rulesDecisionIllegalBreakProductCapability = true;
    productConfig.retainSubmittedTextEvidence = true;
    productConfig.font.atlasWidth = 4096;
    productConfig.font.atlasHeight = 4096;
    expect(productRuntime.initialize(productConfig).ok &&
        productRuntime.resize(1280, 720, 96.0f).ok,
        "shipping 06C runtime initializes English decision glyphs");
    const auto productRendered = productRuntime.render(
        illegalBreakProductSession(), presentation(),
        {true, false, false, false});
    const bool glyphGeometryValid = std::all_of(
        productRendered.glyphQuads.begin(),
        productRendered.glyphQuads.end(), [](const UiPrimitive& glyph) {
            const bool positiveInk = glyph.glyphInkSourceRect.width > 0.0f &&
                glyph.glyphInkSourceRect.height > 0.0f &&
                glyph.glyphInkRect.width > 0.0f &&
                glyph.glyphInkRect.height > 0.0f;
            return glyph.kind == UiPrimitiveKind::GlyphQuad &&
                glyph.codepoint > 0 && glyph.rect.width > 0.0f &&
                glyph.rect.height > 0.0f && glyph.u0 >= 0.0f &&
                glyph.v0 >= 0.0f && glyph.u1 >= glyph.u0 &&
                glyph.v1 >= glyph.v0 && glyph.u1 <= 1.0f &&
                glyph.v1 <= 1.0f && positiveInk;
        });
    const bool hasVisibleGlyphInk = std::any_of(
        productRendered.glyphQuads.begin(),
        productRendered.glyphQuads.end(), [](const UiPrimitive& glyph) {
            return glyph.glyphInkRect.width > 0.0f &&
                glyph.glyphInkRect.height > 0.0f;
        });
    expect(productRendered.ok && productRendered.gpuSubmitted &&
        productRendered.overlay == ProductUiOverlayKind::RulesDecision &&
        productRendered.rulesDecisionHitRects.size() == 3 &&
        !productRendered.textRuns.empty() &&
        !productRendered.glyphQuads.empty() &&
        glyphGeometryValid && hasVisibleGlyphInk,
        "shipping runtime returns the exact clickable 06C hit rectangles: ok=" +
            std::to_string(productRendered.ok) + " error=" +
            productRendered.errorCode + " overlay=" +
            std::to_string(static_cast<int>(productRendered.overlay)) +
            " hits=" +
            std::to_string(productRendered.rulesDecisionHitRects.size()) +
            " glyphs=" + std::to_string(productRendered.glyphQuads.size()) +
            " glyph_geometry=" + std::to_string(glyphGeometryValid) +
            " visible_ink=" + std::to_string(hasVisibleGlyphInk));
    productRuntime.shutdown();

    FakeDispatch defaultDispatch;
    ModernProductUiRuntime defaultRuntime(defaultDispatch);
    auto defaults = ModernProductUiRuntimeConfig::trustedDefaults(
        BILLIARDGL_SOURCE_ROOT, "/System/Library/Fonts/Menlo.ttc");
    expect(defaultRuntime.initialize(defaults).ok &&
        defaultRuntime.render(sessionSnapshot(), presentation(),
            {true, false, false, true}).errorCode ==
            "product_ui_runtime_rules_decision_capability_disabled",
        "default runtime rejects internal evidence mode before frame submit");
}

}  // namespace

int main()
{
    try {
        testPhysical720RetinaIllegalBreakLayout();
        testProductionDrawListAndHitRectGate();
        testRuntimeCapabilityAndRecordingGpuSubmit();
        std::cout << "Rules decision production draw/runtime tests passed\n";
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
