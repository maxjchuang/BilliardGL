#include "render/ui/rules_decision_product_draw.h"

#include "render/modern/camera_projection.h"
#include "table_specs.h"

#include <algorithm>
#include <array>
#include <cmath>

namespace billiardgl::render::ui {
namespace {

constexpr UiColor kPanelStandard{0.020f, 0.039f, 0.052f, 0.96f};
constexpr UiColor kPanelCvd{0.025f, 0.035f, 0.070f, 0.97f};
constexpr UiColor kText{0.96f, 0.97f, 0.98f, 1.0f};
constexpr UiColor kAction{0.09f, 0.20f, 0.23f, 0.98f};
constexpr UiColor kActionCvd{0.10f, 0.17f, 0.31f, 0.98f};
constexpr UiColor kDisabled{0.37f, 0.42f, 0.44f, 1.0f};
constexpr UiColor kAccent{0.16f, 0.80f, 0.67f, 1.0f};
constexpr UiColor kAccentCvd{0.98f, 0.70f, 0.18f, 1.0f};
constexpr UiColor kError{1.0f, 0.34f, 0.30f, 1.0f};
constexpr float kPi = 3.14159265358979323846f;

bool active(rules::ui::DecisionUiState state)
{
    return state != rules::ui::DecisionUiState::HiddenNoOffer &&
        state != rules::ui::DecisionUiState::CompletedAuthoritative;
}

RulesDecisionCanonicalFixture fixtureFor(
    const rules::ui::DecisionUiSnapshot& snapshot)
{
    using rules::ui::DecisionUiState;
    RulesDecisionCanonicalFixture fixture;
    fixture.flowId = snapshot.offer.flowId;
    fixture.branchId = snapshot.offer.branchId;
    fixture.sourceClause = snapshot.offer.sourceClause;
    fixture.actorPlayerId = snapshot.offer.actorPlayerId;
    fixture.previewWorldX = snapshot.placementPreview.worldX;
    fixture.previewWorldZ = snapshot.placementPreview.worldZ;
    fixture.placementErrorCode = snapshot.placementPreviewErrorCode;
    fixture.offeredActionIds = snapshot.visibleActionIds;
    fixture.armedActionId = snapshot.armedActionId;
    fixture.focusedActionId = snapshot.focusedActionId;
    fixture.shotInputBlocked = snapshot.shotInputBlocked;
    const bool placement = snapshot.offer.flowId ==
            rules::kFullTableBallInHandFlowId ||
        snapshot.offer.flowId == rules::kBehindHeadStringBallInHandFlowId;
    fixture.placement = placement;
    fixture.placementValid = snapshot.placementPreviewLegal;
    switch (snapshot.state) {
    case DecisionUiState::DecisionOpenUnselected:
    case DecisionUiState::ActionArmedNotSubmitted:
    case DecisionUiState::PlacementPreviewLocalInvalid:
    case DecisionUiState::PlacementPreviewLocalValid:
        if (placement) {
            fixture.stateId = snapshot.offer.flowId ==
                    rules::kBehindHeadStringBallInHandFlowId ?
                (snapshot.placementPreviewLegal ?
                    "behind_head_string_preview_valid" :
                    "behind_head_string_preview_invalid") :
                (snapshot.placementPreviewLegal ?
                    "full_table_preview_valid" :
                    "full_table_preview_invalid");
        } else if (snapshot.offer.flowId ==
                rules::kBehindHeadStringRespotFlowId) {
            fixture.stateId = snapshot.armedActionId.empty() ?
                "behind_head_string_respot_open_unselected" :
                "behind_head_string_respot_armed_confirm";
        } else if (snapshot.offer.flowId ==
                rules::ui::kGroupChoiceFlowId) {
            fixture.stateId = snapshot.armedActionId.empty() ?
                "group_choice_open_unselected" :
                "group_choice_armed_confirm";
        } else if (snapshot.offer.flowId == rules::kEightOnBreakFlowId) {
            const bool armed = !snapshot.armedActionId.empty();
            fixture.stateId = snapshot.offer.branchId == "foul_break" ?
                (armed ? "eight_foul_armed_confirm" :
                    "eight_foul_open_unselected") :
                (armed ? "eight_legal_armed_confirm" :
                    "eight_legal_open_unselected");
        } else {
            const bool armed = !snapshot.armedActionId.empty();
            fixture.stateId = snapshot.offer.branchId ==
                    "break_shot_foul_6e" ?
                (armed ? "break_shot_foul_armed_confirm" :
                    "break_shot_foul_open_unselected") :
                (armed ? "break_illegal_armed_confirm" :
                    "break_illegal_open_unselected");
        }
        break;
    case DecisionUiState::SubmittingExactRequest:
    case DecisionUiState::AuthorityAckVerifying:
        fixture.stateId = "submitting_progress";
        fixture.offeredActionIds.clear();
        fixture.armedActionId.clear();
        fixture.placement = false;
        break;
    case DecisionUiState::AuthorityErrorBlocking:
        fixture.stateId = "authority_error";
        fixture.offeredActionIds.clear();
        fixture.armedActionId.clear();
        fixture.placement = false;
        break;
    case DecisionUiState::StateDriftRefreshing:
        fixture.stateId = "state_drift_refreshing";
        fixture.offeredActionIds.clear();
        fixture.armedActionId.clear();
        fixture.placement = false;
        break;
    case DecisionUiState::AiOrRemotePending:
        fixture.stateId = "ai_or_remote_pending";
        fixture.offeredActionIds.clear();
        fixture.armedActionId.clear();
        fixture.placement = false;
        break;
    case DecisionUiState::ClassificationPendingBlocked:
        fixture.stateId = "section_6g_classification_pending";
        fixture.offeredActionIds.clear();
        fixture.armedActionId.clear();
        fixture.placement = false;
        break;
    case DecisionUiState::PolicyUnresolvedBlocked:
        fixture.stateId = "authority_error";
        fixture.offeredActionIds.clear();
        fixture.armedActionId.clear();
        fixture.placement = false;
        break;
    case DecisionUiState::HiddenNoOffer:
    case DecisionUiState::CompletedAuthoritative:
        break;
    }
    return fixture;
}

product_hud::HudRect physical(const RulesDecisionRect& rect, float density)
{
    return {rect.x * density, rect.y * density, rect.width * density,
        rect.height * density};
}

product_hud::HudRect intersect(const product_hud::HudRect& left,
    const product_hud::HudRect& right)
{
    const float x0 = std::max(left.x, right.x);
    const float y0 = std::max(left.y, right.y);
    const float x1 = std::min(left.x + left.width, right.x + right.width);
    const float y1 = std::min(left.y + left.height, right.y + right.height);
    return {x0, y0, std::max(0.0f, x1 - x0),
        std::max(0.0f, y1 - y0)};
}

void primitive(HudDrawList& draw, UiPrimitiveKind kind,
    const product_hud::HudRect& rect, const UiColor& color,
    const std::string& token)
{
    const product_hud::HudRect clipped = intersect(rect, draw.safeRect);
    if (clipped.width <= 0.0f || clipped.height <= 0.0f) return;
    UiPrimitive value;
    value.kind = kind;
    value.rect = clipped;
    value.sourceRect = rect;
    value.clipRect = draw.safeRect;
    value.color = color;
    value.token = token;
    value.thickness = kind == UiPrimitiveKind::Line ?
        std::max(1.0f, rect.height) : 0.0f;
    draw.primitives.push_back(value);
}

void triangle(HudDrawList& draw, UiPoint first, UiPoint second, UiPoint third,
    const UiColor& color, const std::string& token)
{
    draw.triangles.push_back({first, second, third, color, draw.safeRect,
        token, token});
}

void segment(HudDrawList& draw, UiPoint first, UiPoint second,
    float thickness, const UiColor& color, const std::string& token)
{
    const float dx = second.x - first.x;
    const float dy = second.y - first.y;
    const float length = std::sqrt(dx * dx + dy * dy);
    if (length <= 0.0001f) return;
    const float nx = -dy * thickness * 0.5f / length;
    const float ny = dx * thickness * 0.5f / length;
    const UiPoint a{first.x + nx, first.y + ny};
    const UiPoint b{second.x + nx, second.y + ny};
    const UiPoint c{second.x - nx, second.y - ny};
    const UiPoint d{first.x - nx, first.y - ny};
    triangle(draw, a, b, c, color, token);
    triangle(draw, a, c, d, color, token);
}

bool projectedSegment(HudDrawList& draw,
    const SharedCameraProjection& projection, Point3 firstWorld,
    Point3 secondWorld, float thickness, const UiColor& color,
    const std::string& token)
{
    HomogeneousPoint first = transformWorldToClip(projection, firstWorld);
    HomogeneousPoint second = transformWorldToClip(projection, secondWorld);
    if (!clipHomogeneousSegment(first, second)) return false;
    PhysicalScreenPoint firstScreen, secondScreen;
    if (!clipToPhysicalScreen(projection, first, firstScreen) ||
        !clipToPhysicalScreen(projection, second, secondScreen))
        return false;
    segment(draw,
        {static_cast<float>(firstScreen.x), static_cast<float>(firstScreen.y)},
        {static_cast<float>(secondScreen.x), static_cast<float>(secondScreen.y)},
        thickness, color, token);
    return true;
}

bool bindProjectedTableFootprint(RulesDecisionDisplayProfile& profile,
    const CameraPresentation& camera)
{
    const SharedCameraProjection projection = buildSharedCameraProjection(
        camera, profile.physicalWidth, profile.physicalHeight);
    if (!projection.valid || !(profile.density > 0.0f)) return false;
    const float halfWidth = kChinesePlayfieldWidthCm * 0.5f;
    const float halfLength = kChinesePlayfieldLengthCm * 0.5f;
    const float y = kTableModelPlaySurfaceHeightCm;
    const std::array<Point3, 4> world{{
        {-halfWidth, y, -halfLength},
        {halfWidth, y, -halfLength},
        {halfWidth, y, halfLength},
        {-halfWidth, y, halfLength}}};
    for (std::size_t index = 0; index < world.size(); ++index) {
        const HomogeneousPoint clip = transformWorldToClip(
            projection, world[index]);
        PhysicalScreenPoint screen;
        if (!clipToPhysicalScreen(projection, clip, screen)) return false;
        profile.projectedTableCorners[index] = {
            static_cast<float>(screen.x / profile.density),
            static_cast<float>(screen.y / profile.density)};
    }
    profile.projectedTableAvailable = true;
    return true;
}

bool emitBehindHeadStringRegion(HudDrawList& draw,
    const CameraPresentation& camera, int physicalWidth, int physicalHeight,
    double headStringWorldZCm, float thickness, const UiColor& color)
{
    if (!std::isfinite(headStringWorldZCm)) return false;
    const SharedCameraProjection projection = buildSharedCameraProjection(
        camera, physicalWidth, physicalHeight);
    if (!projection.valid) return false;
    const float radius = kChineseBallRadiusCm;
    const float x0 = -kChinesePlayfieldWidthCm * 0.5f + radius;
    const float x1 = kChinesePlayfieldWidthCm * 0.5f - radius;
    const float z0 = -kChinesePlayfieldLengthCm * 0.5f + radius;
    const float z1 = static_cast<float>(headStringWorldZCm);
    const float y = kTableModelPlaySurfaceHeightCm + 0.05f;
    if (!(z1 > z0 && z1 < kChinesePlayfieldLengthCm * 0.5f)) return false;

    const auto world = [=](float u, float v) {
        return Point3{x0 + (x1 - x0) * u, y, z0 + (z1 - z0) * v};
    };
    bool boundary = projectedSegment(draw, projection, world(0.0f, 1.0f),
        world(1.0f, 1.0f), thickness, color,
        "rules_head_string_boundary");
    projectedSegment(draw, projection, world(0.0f, 0.0f), world(1.0f, 0.0f),
        thickness, color, "rules_legal_region_boundary");
    projectedSegment(draw, projection, world(0.0f, 0.0f), world(0.0f, 1.0f),
        thickness, color, "rules_legal_region_boundary");
    projectedSegment(draw, projection, world(1.0f, 0.0f), world(1.0f, 1.0f),
        thickness, color, "rules_legal_region_boundary");

    // Lines of constant u-v stay within the exact legal center rectangle.
    // Projection and homogeneous clipping use the same camera math as the
    // table renderer and pointer-to-table adapter.
    int hatchCount = 0;
    for (int index = -8; index <= 8; ++index) {
        const float c = static_cast<float>(index) / 8.0f;
        UiPoint uvFirst, uvSecond;
        if (c < 0.0f) {
            uvFirst = {0.0f, -c};
            uvSecond = {1.0f + c, 1.0f};
        } else {
            uvFirst = {c, 0.0f};
            uvSecond = {1.0f, 1.0f - c};
        }
        if (projectedSegment(draw, projection,
                world(uvFirst.x, uvFirst.y), world(uvSecond.x, uvSecond.y),
                thickness, color, "rules_legal_region_diagonal_hatch"))
            ++hatchCount;
    }
    return boundary && hatchCount > 0;
}

UiPoint radial(float cx, float cy, float radius, float angle)
{
    return {cx + std::cos(angle) * radius, cy + std::sin(angle) * radius};
}

void ring(HudDrawList& draw, float cx, float cy, float radius,
    float thickness, int segments, bool dashed, const UiColor& color,
    const std::string& token)
{
    for (int index = 0; index < segments; ++index) {
        if (dashed && index % 2 != 0) continue;
        const float a0 = 2.0f * kPi * index / segments;
        const float a1 = 2.0f * kPi * (index + (dashed ? 0.62f : 1.0f)) /
            segments;
        segment(draw, radial(cx, cy, radius, a0),
            radial(cx, cy, radius, a1), thickness, color, token);
    }
}

void filledCircle(HudDrawList& draw, float cx, float cy, float radius,
    const UiColor& color, const std::string& token)
{
    constexpr int kSegments = 24;
    for (int index = 0; index < kSegments; ++index)
        triangle(draw, {cx, cy},
            radial(cx, cy, radius, 2.0f * kPi * index / kSegments),
            radial(cx, cy, radius, 2.0f * kPi * (index + 1) / kSegments),
            color, token);
}

void frame(HudDrawList& draw, const product_hud::HudRect& rect,
    float inset, float thickness, const UiColor& color,
    const std::string& token)
{
    const float x0 = rect.x + inset;
    const float y0 = rect.y + inset;
    const float x1 = rect.x + rect.width - inset;
    const float y1 = rect.y + rect.height - inset;
    segment(draw, {x0, y0}, {x1, y0}, thickness, color, token);
    segment(draw, {x1, y0}, {x1, y1}, thickness, color, token);
    segment(draw, {x1, y1}, {x0, y1}, thickness, color, token);
    segment(draw, {x0, y1}, {x0, y0}, thickness, color, token);
}

bool glyphLine(HudDrawList& draw, const SdfFontAtlas& atlas,
    const std::string& text, float x, float baseline, float size,
    const product_hud::HudRect& clip, const UiColor& color)
{
    const TextMeasureResult measured = atlas.measureUtf8(text, size, false);
    if (!measured.ok) {
        draw.errorCode = measured.errorCode;
        return false;
    }
    const std::uint32_t textRunIndex = static_cast<std::uint32_t>(
        draw.textRuns.size());
    draw.textRuns.push_back(
        {text, clip, size, measured.width, x, baseline, false});
    std::vector<std::uint32_t> codepoints;
    std::string error;
    if (!decodeUtf8(text, codepoints, error)) {
        draw.errorCode = error;
        return false;
    }
    const float scale = size / atlas.pixelSize();
    float pen = x;
    std::uint32_t codepointOrdinal = 0;
    for (std::uint32_t codepoint : codepoints) {
        const GlyphRecord* glyph = atlas.findGlyph(codepoint);
        if (glyph == nullptr) {
            draw.errorCode = "font_glyph_not_prebaked";
            return false;
        }
        if (glyph->atlasWidth > 0 && glyph->atlasHeight > 0) {
            const product_hud::HudRect raw{
                pen + glyph->bearingXPx * scale - glyph->sdfSpread * scale,
                baseline - glyph->bearingYPx * scale -
                    glyph->sdfSpread * scale,
                glyph->atlasWidth * scale, glyph->atlasHeight * scale};
            const product_hud::HudRect clipped = intersect(raw, clip);
            if (clipped.width > 0.0f && clipped.height > 0.0f) {
                UiPrimitive value;
                value.kind = UiPrimitiveKind::GlyphQuad;
                value.rect = clipped;
                value.sourceRect = raw;
                value.clipRect = clip;
                value.color = color;
                value.codepoint = codepoint;
                value.textRunIndex = textRunIndex;
                value.codepointOrdinal = codepointOrdinal;
                const float rawU0 = static_cast<float>(glyph->atlasX) /
                    atlas.width();
                const float rawV0 = static_cast<float>(glyph->atlasY) /
                    atlas.height();
                const float rawU1 = static_cast<float>(glyph->atlasX +
                    glyph->atlasWidth) / atlas.width();
                const float rawV1 = static_cast<float>(glyph->atlasY +
                    glyph->atlasHeight) / atlas.height();
                const float x0 = (clipped.x - raw.x) / raw.width;
                const float y0 = (clipped.y - raw.y) / raw.height;
                const float x1 = (clipped.x + clipped.width - raw.x) /
                    raw.width;
                const float y1 = (clipped.y + clipped.height - raw.y) /
                    raw.height;
                value.u0 = rawU0 + (rawU1 - rawU0) * x0;
                value.v0 = rawV0 + (rawV1 - rawV0) * y0;
                value.u1 = rawU0 + (rawU1 - rawU0) * x1;
                value.v1 = rawV0 + (rawV1 - rawV0) * y1;
                value.glyphInkSourceRect = {
                    pen + glyph->bearingXPx * scale,
                    baseline - glyph->bearingYPx * scale,
                    glyph->bitmapWidth * scale,
                    glyph->bitmapHeight * scale};
                value.glyphInkRect = intersect(
                    value.glyphInkSourceRect, clip);
                draw.primitives.push_back(value);
                ++draw.glyphQuadCount;
            }
        }
        pen += glyph->advancePx * scale;
        ++codepointOrdinal;
    }
    return true;
}

bool text(HudDrawList& draw, const SdfFontAtlas& atlas,
    const RulesDecisionDrawItem& item, float density, const UiColor& color)
{
    const product_hud::HudRect clip = physical(item.bounds, density);
    const float size = item.fontLogicalPx * density;
    float baseline = clip.y + size;
    std::size_t start = 0;
    while (start <= item.text.size()) {
        const std::size_t end = item.text.find('\n', start);
        const std::string line = item.text.substr(start,
            end == std::string::npos ? std::string::npos : end - start);
        if (!line.empty() && !glyphLine(draw, atlas, line, clip.x, baseline,
                size, clip, color))
            return false;
        if (end == std::string::npos) break;
        start = end + 1;
        baseline += size * 1.25f;
    }
    return true;
}

}  // namespace

RulesDecisionProductDraw buildRulesDecisionProductDraw(
    const rules::ui::DecisionUiSnapshot& snapshot,
    const CameraPresentation& camera,
    RulesDecisionLocale locale, HudPaletteMode palette,
    const ShellViewport& viewport, float uiScale, const SdfFontAtlas& atlas,
    bool englishOnlyFont)
{
    RulesDecisionProductDraw result;
    result.active = active(snapshot.state);
    result.draw.ok = true;
    result.draw.atlasCacheKey = atlas.cacheKey();
    result.draw.atlasRasterizationCount = atlas.rasterizationCount();
    if (!result.active) {
        result.ok = true;
        return result;
    }
    if (!atlas.initialized() || viewport.logicalWidth <= 0 ||
        viewport.logicalHeight <= 0 || viewport.physicalWidth <= 0 ||
        viewport.physicalHeight <= 0) {
        result.errorCode = "rules_decision_product_draw_invalid_context";
        result.draw.ok = false;
        result.draw.errorCode = result.errorCode;
        return result;
    }
    const float density = static_cast<float>(viewport.physicalWidth) /
        viewport.logicalWidth;
    if (std::fabs(viewport.physicalHeight /
            static_cast<float>(viewport.logicalHeight) - density) > 0.01f) {
        result.errorCode = "rules_decision_product_draw_density_mismatch";
        result.draw.ok = false;
        result.draw.errorCode = result.errorCode;
        return result;
    }
    RulesDecisionDisplayProfile profile;
    profile.logicalWidth = viewport.logicalWidth;
    profile.logicalHeight = viewport.logicalHeight;
    profile.physicalWidth = viewport.physicalWidth;
    profile.physicalHeight = viewport.physicalHeight;
    profile.density = density;
    profile.uiScale = uiScale;
    if (fixtureFor(snapshot).placement &&
        !bindProjectedTableFootprint(profile, camera)) {
        result.errorCode = "rules_decision_table_projection_invalid";
        result.draw.ok = false;
        result.draw.errorCode = result.errorCode;
        return result;
    }
    const RulesDecisionTextMeasure measure = [&atlas, density, locale,
            englishOnlyFont](
        const std::string& value, float logicalSize) {
        const std::string measuredValue = englishOnlyFont &&
                locale == RulesDecisionLocale::En && value == "Ag国" ?
            "Ag" : value;
        const auto measured = atlas.measureUtf8(
            measuredValue, logicalSize * density, false);
        return RulesDecisionTextMetrics{measured.ok,
            measured.width / density, measured.height / density};
    };
    const RulesDecisionDrawList logical = buildRulesDecisionDrawList(
        fixtureFor(snapshot), locale,
        palette == HudPaletteMode::Standard ?
            RulesDecisionPalette::Standard :
            RulesDecisionPalette::ColorVisionSafe,
        profile, measure);
    const RulesDecisionDrawValidation validation =
        validateRulesDecisionDrawList(logical);
    if (!validation.ok) {
        result.errorCode = validation.errors.empty() ?
            "rules_decision_product_draw_invalid" : validation.errors.front();
        result.draw.ok = false;
        result.draw.errorCode = result.errorCode;
        return result;
    }
    const float safe = 24.0f;
    result.draw.safeRect = {safe, safe,
        viewport.physicalWidth - safe * 2.0f,
        viewport.physicalHeight - safe * 2.0f};
    const bool cvd = palette != HudPaletteMode::Standard;
    bool emittedBehindHeadStringRegion = false;
    for (const RulesDecisionDrawItem& item : logical.items) {
        const product_hud::HudRect rect = physical(item.bounds, density);
        switch (item.kind) {
        case RulesDecisionDrawKind::ModalPanel:
        case RulesDecisionDrawKind::PlacementRibbon:
            primitive(result.draw, UiPrimitiveKind::Panel, rect,
                cvd ? kPanelCvd : kPanelStandard, item.id);
            result.exclusionRects.push_back(rect);
            break;
        case RulesDecisionDrawKind::Text:
            if (!text(result.draw, atlas, item, density, kText)) {
                result.errorCode = result.draw.errorCode;
                result.draw.ok = false;
                return result;
            }
            break;
        case RulesDecisionDrawKind::Action:
        case RulesDecisionDrawKind::Confirm: {
            const bool confirm = item.kind == RulesDecisionDrawKind::Confirm;
            primitive(result.draw, UiPrimitiveKind::Panel, rect,
                !item.enabled ? kDisabled : confirm || item.armed ?
                    (cvd ? kAccentCvd : kAccent) :
                    (cvd ? kActionCvd : kAction),
                confirm ? "rules_decision_confirm" :
                    "rules_decision_action");
            if (!text(result.draw, atlas, item, density,
                    item.enabled ? kText : kPanelStandard)) {
                result.errorCode = result.draw.errorCode;
                result.draw.ok = false;
                return result;
            }
            result.hitRects.push_back({
                (confirm ? "confirm:" : "action:") + item.actionId,
                item.actionId, rect, item.enabled, confirm});
            break;
        }
        case RulesDecisionDrawKind::HeadStringBoundary:
            if (!emittedBehindHeadStringRegion) {
                emittedBehindHeadStringRegion = emitBehindHeadStringRegion(
                    result.draw, camera, viewport.physicalWidth,
                    viewport.physicalHeight, snapshot.headStringWorldZCm,
                    std::max(2.0f, density * 1.5f),
                    cvd ? kAccentCvd : kAccent);
                if (!emittedBehindHeadStringRegion) {
                    result.errorCode = "rules_behind_head_string_projection_invalid";
                    result.draw.ok = false;
                    result.draw.errorCode = result.errorCode;
                    return result;
                }
            }
            break;
        case RulesDecisionDrawKind::LegalRegionHatch:
            // Emitted together with the projected head-string boundary.
            break;
        case RulesDecisionDrawKind::PreviewGhostBall: {
            const float radius = std::min(rect.width, rect.height) * 0.5f;
            const float cx = rect.x + rect.width * 0.5f;
            const float cy = rect.y + rect.height * 0.5f;
            filledCircle(result.draw, cx, cy, radius,
                {kText.red, kText.green, kText.blue, 0.38f},
                "rules_preview_ghost_cue_ball");
            ring(result.draw, cx, cy, radius, std::max(2.0f, density * 1.5f),
                24, false, kText, "rules_preview_ghost_cue_ball_outline");
            break;
        }
        case RulesDecisionDrawKind::PreviewCrosshair: {
            const float cx = rect.x + rect.width * 0.5f;
            const float cy = rect.y + rect.height * 0.5f;
            segment(result.draw, {rect.x, cy}, {rect.x + rect.width, cy},
                std::max(2.0f, density * 1.5f), cvd ? kAccentCvd : kAccent,
                "rules_preview_crosshair");
            segment(result.draw, {cx, rect.y}, {cx, rect.y + rect.height},
                std::max(2.0f, density * 1.5f), cvd ? kAccentCvd : kAccent,
                "rules_preview_crosshair");
            break;
        }
        case RulesDecisionDrawKind::FocusChevron: {
            const bool exactV4 = item.id == "focus_chevron_v4";
            const char* token = exactV4 ? kRulesUiFocusChevronV4Token :
                "rules_focus_chevron";
            const float cx = rect.x + rect.width *
                (exactV4 ? 0.84f : 0.42f);
            const float cy = rect.y + rect.height * 0.5f;
            const UiColor roleForeground = cvd ? kPanelCvd : kPanelStandard;
            segment(result.draw, {rect.x, rect.y}, {cx, cy},
                std::max(2.0f, density * 2.0f), roleForeground,
                token);
            segment(result.draw, {cx, cy}, {rect.x, rect.y + rect.height},
                std::max(2.0f, density * 2.0f), roleForeground,
                token);
            break;
        }
        case RulesDecisionDrawKind::ArmedDiamond: {
            const bool exactV4 = item.id == "armed_diamond_v4";
            const char* token = exactV4 ?
                kRulesUiSelectedCheckDiamondV4Token :
                "rules_selected_check_diamond";
            const UiColor roleForeground = cvd ? kPanelCvd : kPanelStandard;
            const float cx = rect.x + rect.width * 0.5f;
            const float cy = rect.y + rect.height * 0.5f;
            const float half = std::min(rect.width, rect.height) * 0.48f;
            const UiPoint top{cx, cy - half}, right{cx + half, cy};
            const UiPoint bottom{cx, cy + half}, left{cx - half, cy};
            segment(result.draw, top, right, 2.0f * density, roleForeground,
                token);
            segment(result.draw, right, bottom, 2.0f * density, roleForeground,
                token);
            segment(result.draw, bottom, left, 2.0f * density, roleForeground,
                token);
            segment(result.draw, left, top, 2.0f * density, roleForeground,
                token);
            const float start = exactV4 ? 0.38f : 0.48f;
            const float middleX = exactV4 ? 0.08f : 0.10f;
            const float middleY = exactV4 ? 0.26f : 0.35f;
            const float endX = exactV4 ? 0.38f : 0.52f;
            const float endY = exactV4 ? 0.26f : 0.34f;
            segment(result.draw, {cx - half * start, cy},
                {cx - half * middleX, cy + half * middleY}, 2.0f * density,
                roleForeground, token);
            segment(result.draw, {cx - half * middleX,
                    cy + half * middleY},
                {cx + half * endX, cy - half * endY}, 2.0f * density,
                roleForeground, token);
            break;
        }
        case RulesDecisionDrawKind::ConfirmDoubleFrameEnterArrow: {
            const bool exactV4 =
                item.id == "confirm_double_frame_enter_arrow_v4";
            const char* token = exactV4 ?
                kRulesUiConfirmDoubleFrameEnterArrowV4Token :
                "rules_confirm_double_frame_plus_enter_arrow";
            const UiColor roleForeground = cvd ? kPanelCvd : kPanelStandard;
            const float thickness = std::max(2.0f, density * 2.0f);
            frame(result.draw, rect, thickness * 0.5f, thickness,
                roleForeground,
                token);
            const float innerInset = exactV4 ?
                thickness * 1.5f + 2.0f : thickness * 2.2f;
            frame(result.draw, rect, innerInset, thickness, roleForeground,
                token);
            const float cy = rect.y + rect.height * 0.5f;
            if (exactV4) {
                const float left = rect.x + innerInset + thickness * 1.4f;
                const float right = rect.x + rect.width - innerInset -
                    thickness * 1.4f;
                const float top = rect.y + innerInset + thickness * 1.2f;
                segment(result.draw, {right, top}, {right, cy}, thickness,
                    roleForeground, token);
                segment(result.draw, {right, cy}, {left, cy}, thickness,
                    roleForeground, token);
                segment(result.draw, {left + thickness * 1.5f,
                        cy - thickness * 1.5f}, {left, cy}, thickness,
                    roleForeground, token);
                segment(result.draw, {left + thickness * 1.5f,
                        cy + thickness * 1.5f}, {left, cy}, thickness,
                    roleForeground, token);
            } else {
                const float right = rect.x + rect.width - thickness * 5.0f;
                segment(result.draw, {right - 14.0f * density, cy},
                    {right, cy}, thickness, kText, token);
                segment(result.draw, {right - 5.0f * density,
                        cy - 5.0f * density}, {right, cy}, thickness,
                    kText, token);
                segment(result.draw, {right - 5.0f * density,
                        cy + 5.0f * density}, {right, cy}, thickness,
                    kText, token);
            }
            break;
        }
        case RulesDecisionDrawKind::InvalidOctagon: {
            const float cx = rect.x + rect.width * 0.5f;
            const float cy = rect.y + rect.height * 0.5f;
            const float radius = std::min(rect.width, rect.height) * 0.5f;
            ring(result.draw, cx, cy, radius, std::max(2.0f, density * 2.0f),
                8, false, kError, "rules_error_cross_octagon");
            const float arm = radius * 0.48f;
            segment(result.draw, {cx - arm, cy - arm}, {cx + arm, cy + arm},
                std::max(2.0f, density * 2.0f), kError,
                "rules_error_cross_octagon");
            segment(result.draw, {cx + arm, cy - arm}, {cx - arm, cy + arm},
                std::max(2.0f, density * 2.0f), kError,
                "rules_error_cross_octagon");
            break;
        }
        case RulesDecisionDrawKind::InvalidCollisionRing: {
            const float cx = rect.x + rect.width * 0.5f;
            const float cy = rect.y + rect.height * 0.5f;
            ring(result.draw, cx, cy, std::min(rect.width, rect.height) * 0.5f,
                std::max(2.0f, density * 1.5f), 24, true, kError,
                "rules_invalid_dashed_collision_ring");
            break;
        }
        case RulesDecisionDrawKind::BlockingShape:
            primitive(result.draw, UiPrimitiveKind::Line, rect, kError,
                "rules_blocking_shape");
            break;
        }
    }
    result.draw.ok = true;
    result.ok = true;
    return result;
}

}  // namespace billiardgl::render::ui
