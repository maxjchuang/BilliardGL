#include "render/ui/hud_draw_list.h"

#include <algorithm>
#include <cmath>
#include <cstdio>

namespace billiardgl::render::ui {
namespace {

constexpr UiColor kPanel{0.027f, 0.047f, 0.059f, 0.90f};
constexpr UiColor kText{0.94f, 0.96f, 0.96f, 1.0f};
constexpr UiColor kSecondary{0.67f, 0.73f, 0.75f, 1.0f};
constexpr UiColor kAccent{0.15f, 0.78f, 0.66f, 1.0f};
constexpr UiColor kWarning{1.0f, 0.74f, 0.24f, 1.0f};
constexpr UiColor kDestructive{1.0f, 0.31f, 0.29f, 1.0f};

bool isDigit(std::uint32_t codepoint)
{
    return codepoint >= static_cast<std::uint32_t>('0') &&
        codepoint <= static_cast<std::uint32_t>('9');
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

bool empty(const product_hud::HudRect& rect)
{
    return rect.width <= 0.0f || rect.height <= 0.0f;
}

void addPrimitive(HudDrawList& list, UiPrimitiveKind kind,
    const product_hud::HudRect& rect, const product_hud::HudRect& clip,
    const UiColor& color, const std::string& token = std::string())
{
    const product_hud::HudRect clipped = intersect(rect, clip);
    if (empty(clipped)) return;
    UiPrimitive primitive;
    primitive.kind = kind;
    primitive.rect = clipped;
    primitive.sourceRect = rect;
    primitive.clipRect = clip;
    primitive.color = color;
    primitive.token = token;
    if (kind == UiPrimitiveKind::Line) primitive.thickness = 1.0f;
    list.primitives.push_back(primitive);
}

bool addText(HudDrawList& list, const SdfFontAtlas& atlas,
    const std::string& text, float x, float baselineY, float requestedSize,
    const product_hud::HudRect& clip, const UiColor& color,
    bool tabularDigits = true)
{
    const TextMeasureResult measured = atlas.measureUtf8(
        text, requestedSize, tabularDigits);
    if (!measured.ok) {
        list.errorCode = measured.errorCode;
        return false;
    }
    const std::uint32_t textRunIndex = static_cast<std::uint32_t>(
        list.textRuns.size());
    list.textRuns.push_back(
        {text, clip, requestedSize, measured.width, x, baselineY,
            tabularDigits});
    std::vector<std::uint32_t> codepoints;
    std::string decodeError;
    if (!decodeUtf8(text, codepoints, decodeError)) {
        list.errorCode = decodeError;
        return false;
    }
    const float scale = requestedSize / atlas.pixelSize();
    float penX = x;
    std::uint32_t codepointOrdinal = 0;
    for (std::uint32_t codepoint : codepoints) {
        const GlyphRecord* glyph = atlas.findGlyph(codepoint);
        if (glyph == nullptr) {
            list.errorCode = "font_glyph_not_prebaked";
            return false;
        }
        const float cellAdvance = tabularDigits && isDigit(codepoint) ?
            atlas.tabularDigitAdvancePx() : glyph->advancePx;
        const float centerOffset = tabularDigits && isDigit(codepoint) ?
            (cellAdvance - glyph->advancePx) * 0.5f * scale : 0.0f;
        if (glyph->atlasWidth > 0 && glyph->atlasHeight > 0) {
            const product_hud::HudRect raw{
                penX + centerOffset + glyph->bearingXPx * scale -
                    glyph->sdfSpread * scale,
                baselineY - glyph->bearingYPx * scale -
                    glyph->sdfSpread * scale,
                glyph->atlasWidth * scale, glyph->atlasHeight * scale};
            const product_hud::HudRect clipped = intersect(raw, clip);
            if (!empty(clipped)) {
                UiPrimitive primitive;
                primitive.kind = UiPrimitiveKind::GlyphQuad;
                primitive.rect = clipped;
                primitive.sourceRect = raw;
                primitive.clipRect = clip;
                primitive.color = color;
                primitive.codepoint = codepoint;
                primitive.textRunIndex = textRunIndex;
                primitive.codepointOrdinal = codepointOrdinal;
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
                primitive.u0 = rawU0 + (rawU1 - rawU0) * x0;
                primitive.v0 = rawV0 + (rawV1 - rawV0) * y0;
                primitive.u1 = rawU0 + (rawU1 - rawU0) * x1;
                primitive.v1 = rawV0 + (rawV1 - rawV0) * y1;
                primitive.glyphInkSourceRect = {
                    penX + centerOffset + glyph->bearingXPx * scale,
                    baselineY - glyph->bearingYPx * scale,
                    glyph->bitmapWidth * scale,
                    glyph->bitmapHeight * scale};
                primitive.glyphInkRect = intersect(
                    primitive.glyphInkSourceRect, clip);
                list.primitives.push_back(primitive);
                ++list.glyphQuadCount;
            }
        }
        penX += cellAdvance * scale;
        ++codepointOrdinal;
    }
    return true;
}

float fittedSize(const SdfFontAtlas& atlas, const std::string& text,
    float preferred, float minimum, float availableWidth)
{
    const TextMeasureResult measured = atlas.measureUtf8(text, preferred, true);
    if (!measured.ok || measured.width <= availableWidth) return preferred;
    return std::max(minimum, preferred * availableWidth / measured.width);
}

product_hud::HudRect inset(const product_hud::HudRect& rect, float amount)
{
    return {rect.x + amount, rect.y + amount,
        std::max(0.0f, rect.width - amount * 2.0f),
        std::max(0.0f, rect.height - amount * 2.0f)};
}

bool addFieldText(HudDrawList& list, const SdfFontAtlas& atlas,
    const std::string& text, const product_hud::HudRect& clip, float y,
    float preferredSize, float minimumSize, const UiColor& color)
{
    const float size = fittedSize(atlas, text, preferredSize, minimumSize,
        clip.width);
    return addText(list, atlas, text, clip.x, y + size, size, clip, color);
}

void addPanel(HudDrawList& list, const product_hud::HudRect& rect,
    const product_hud::HudRect& safe, const char* token)
{
    addPrimitive(list, UiPrimitiveKind::Panel, rect, safe, kPanel, token);
}

bool addPlayerStatus(HudDrawList& list, const SdfFontAtlas& atlas,
    const product_hud::ProductHudViewModel& model,
    const product_hud::HudRect& rect, float minimum)
{
    addPanel(list, rect, list.safeRect, "player_status_panel");
    const auto content = inset(rect, 14.0f);
    const std::string targetLine = model.targetBall.text + " / " +
        model.targetPocket.text;
    const TextMeasureResult targetAtMinimum = atlas.measureUtf8(
        targetLine, minimum, true);
    const bool splitTarget = targetAtMinimum.ok &&
        targetAtMinimum.width > content.width;
    addPrimitive(list, UiPrimitiveKind::Icon,
        {content.x, content.y, 18.0f, 18.0f}, content, kAccent,
        model.groupShapeToken);
    bool ok = addFieldText(list, atlas, model.player.text,
        {content.x + 26.0f, content.y, content.width - 26.0f, 32.0f},
        content.y, minimum * 1.30f, minimum, kText) &&
        addFieldText(list, atlas, model.group.text, content,
            content.y + minimum * 1.8f, minimum, minimum, kAccent);
    if (splitTarget) {
        ok = addFieldText(list, atlas, model.targetBall.text, content,
            content.y + minimum * 3.3f, minimum, minimum, kSecondary) && ok;
        ok = addFieldText(list, atlas, model.targetPocket.text, content,
            content.y + minimum * 4.55f, minimum, minimum, kSecondary) && ok;
    } else {
        ok = addFieldText(list, atlas, targetLine, content,
            content.y + minimum * 3.3f, minimum, minimum, kSecondary) && ok;
    }
    return ok;
}

bool addContext(HudDrawList& list, const SdfFontAtlas& atlas,
    const product_hud::ProductHudViewModel& model,
    const product_hud::HudRect& rect, float minimum)
{
    addPanel(list, rect, list.safeRect, "context_hint_panel");
    const auto content = inset(rect, 12.0f);
    return addFieldText(list, atlas, model.camera.text, content,
        content.y, minimum, minimum, kText) &&
        addFieldText(list, atlas, model.assist.text, content,
            content.y + minimum * 1.7f, minimum, minimum, kSecondary);
}

bool addPower(HudDrawList& list, const SdfFontAtlas& atlas,
    const product_hud::ProductHudViewModel& model,
    const product_hud::HudRect& rect, float minimum)
{
    addPanel(list, rect, list.safeRect, "power_meter_panel");
    const auto content = inset(rect, 14.0f);
    int percent = 0;
    (void)std::sscanf(model.powerPercent.text.c_str(), "%d", &percent);
    percent = std::max(0, std::min(100, percent));
    const float gap = 3.0f;
    const float segmentWidth = (content.width - gap * 9.0f) / 10.0f;
    const float segmentY = content.y + content.height - 24.0f;
    for (int segment = 0; segment < 10; ++segment) {
        addPrimitive(list, UiPrimitiveKind::SolidRect,
            {content.x + segment * (segmentWidth + gap), segmentY,
                segmentWidth, 12.0f}, content,
            segment * 10 < percent ? kAccent : kSecondary,
            "power_segment");
    }
    return addFieldText(list, atlas, model.powerPercent.text, content,
        content.y, minimum * 1.35f, minimum, kText) &&
        addFieldText(list, atlas, model.estimatedCueSpeed.text,
            {content.x + content.width * 0.35f, content.y,
                content.width * 0.65f, content.height}, content.y,
            minimum, minimum, kSecondary);
}

bool addTip(HudDrawList& list, const SdfFontAtlas& atlas,
    const product_hud::ProductHudViewModel& model,
    const product_hud::HudRect& rect, float minimum)
{
    addPanel(list, rect, list.safeRect, "tip_offset_panel");
    const auto content = inset(rect, 12.0f);
    const float diameter = std::min(content.width, content.height - minimum * 2.0f);
    const product_hud::HudRect ball{content.x + (content.width - diameter) * 0.5f,
        content.y, diameter, diameter};
    addPrimitive(list, UiPrimitiveKind::Icon, ball, content, kText,
        "tip_ball_outline");
    addPrimitive(list, UiPrimitiveKind::Line,
        {ball.x, ball.y + diameter * 0.5f, ball.width, 1.0f}, content,
        kSecondary, "tip_crosshair_horizontal");
    addPrimitive(list, UiPrimitiveKind::Line,
        {ball.x + diameter * 0.5f, ball.y, 1.0f, ball.height}, content,
        kSecondary, "tip_crosshair_vertical");
    if (model.tipOffset.available && std::isfinite(model.tipOffsetX) &&
        std::isfinite(model.tipOffsetY) &&
        std::hypot(model.tipOffsetX, model.tipOffsetY) <= 1.0f) {
        // Positive cue Y is topspin; screen Y grows downward.
        addPrimitive(list, UiPrimitiveKind::Icon,
            {ball.x + diameter * (0.46f + model.tipOffsetX * 0.5f),
                ball.y + diameter * (0.46f - model.tipOffsetY * 0.5f),
                diameter * 0.08f, diameter * 0.08f}, content, kAccent,
            "tip_position_marker");
    }
    return addFieldText(list, atlas, model.tipOffset.text, content,
        ball.y + ball.height + 2.0f, minimum, minimum, kText);
}

bool addFoul(HudDrawList& list, const SdfFontAtlas& atlas,
    const product_hud::ProductHudViewModel& model,
    const product_hud::HudRect& rect, float minimum)
{
    addPanel(list, rect, list.safeRect, "foul_banner_panel");
    const auto content = inset(rect, 12.0f);
    const std::string icon = model.ruleNotices.empty() ? "warning_triangle" :
        model.ruleNotices.front().shapeToken;
    addPrimitive(list, UiPrimitiveKind::Icon,
        {content.x, content.y, 28.0f, 28.0f}, content, kDestructive, icon);
    return addFieldText(list, atlas, "FOUL: " + model.foulReason.text,
        {content.x + 38.0f, content.y, content.width - 38.0f,
            content.height * 0.55f}, content.y, minimum * 1.15f,
            minimum, kDestructive) &&
        addFieldText(list, atlas, model.foulResolution.text,
            {content.x + 38.0f, content.y + content.height * 0.5f,
                content.width - 38.0f, content.height * 0.5f},
            content.y + content.height * 0.48f, minimum, minimum, kText);
}

}  // namespace

HudDrawList HudDrawListBuilder::buildProduct(
    const product_hud::ProductHudViewModel& model,
    const product_hud::HudLayoutResult& layout,
    const SdfFontAtlas& atlas) const
{
    HudDrawList list;
    if (!layout.ok) {
        list.errorCode = "hud_layout_unavailable";
        return list;
    }
    if (!atlas.initialized() || !atlas.frozen()) {
        list.errorCode = "font_atlas_not_initialized";
        return list;
    }
    list.atlasCacheKey = atlas.cacheKey();
    list.atlasRasterizationCount = atlas.rasterizationCount();
    list.safeRect = {layout.safeAreaPhysical.left,
        layout.safeAreaPhysical.top,
        std::max(0.0f, static_cast<float>(layout.physicalWidth) -
            layout.safeAreaPhysical.left - layout.safeAreaPhysical.right),
        std::max(0.0f, static_cast<float>(layout.physicalHeight) -
            layout.safeAreaPhysical.top - layout.safeAreaPhysical.bottom)};
    const float minimum = layout.minimumTextPhysicalPx;
    auto element = [&](const char* name) -> const product_hud::HudRect* {
        const auto found = layout.elements.find(name);
        return found == layout.elements.end() ? nullptr : &found->second;
    };
    bool ok = true;
    if (const auto* rect = element("player_status"))
        ok = addPlayerStatus(list, atlas, model, *rect, minimum) && ok;
    if (const auto* rect = element("context_hint"))
        ok = addContext(list, atlas, model, *rect, minimum) && ok;
    if (const auto* rect = element("power_meter"))
        ok = addPower(list, atlas, model, *rect, minimum) && ok;
    if (const auto* rect = element("tip_offset"))
        ok = addTip(list, atlas, model, *rect, minimum) && ok;
    if (const auto* rect = element("foul_banner"))
        ok = addFoul(list, atlas, model, *rect, minimum) && ok;
    if (const auto* rect = element("shot_result")) {
        addPanel(list, *rect, list.safeRect, "shot_result_panel");
        const auto content = inset(*rect, 14.0f);
        ok = addFieldText(list, atlas, model.result.text, content,
            content.y, minimum * 1.25f, minimum, kText) && ok;
    }
    if (const auto* rect = element("help_panel")) {
        if (model.helpLines.size() != 7) {
            list.errorCode = "help_bindings_unavailable";
            return list;
        }
        addPanel(list, *rect, list.safeRect, "help_panel");
        const auto content = inset(*rect, 18.0f);
        float y = content.y;
        for (const std::string& line : model.helpLines) {
            ok = addFieldText(list, atlas, line, content, y,
                minimum, minimum, kText) && ok;
            y += minimum * 1.8f;
        }
    }
    if (!ok) return list;
    if (atlas.rasterizationCount() != list.atlasRasterizationCount) {
        list.errorCode = "font_rasterized_during_frame";
        return list;
    }
    list.ok = true;
    return list;
}

HudDrawList HudDrawListBuilder::buildDebug(
    const product_hud::DebugOverlayViewModel& debug,
    const product_hud::HudLayoutResult& layout, const SdfFontAtlas& atlas,
    bool releaseCapture) const
{
    HudDrawList list;
    if (releaseCapture || !debug.visible) {
        list.ok = true;
        return list;
    }
    if (!layout.ok || !atlas.initialized()) {
        list.errorCode = "debug_hud_dependency_unavailable";
        return list;
    }
    list.containsDebug = true;
    list.atlasCacheKey = atlas.cacheKey();
    list.atlasRasterizationCount = atlas.rasterizationCount();
    list.safeRect = {layout.safeAreaPhysical.left,
        layout.safeAreaPhysical.top, 420.0f, 140.0f};
    addPanel(list, list.safeRect, list.safeRect, "debug_overlay_panel");
    float y = list.safeRect.y + 10.0f;
    for (const std::string& field : debug.fields) {
        if (!addText(list, atlas, field, list.safeRect.x + 10.0f,
                y + 13.0f, 13.0f, list.safeRect, kWarning))
            return list;
        y += 20.0f;
    }
    list.ok = true;
    return list;
}

}  // namespace billiardgl::render::ui
