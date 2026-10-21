#include "render/ui/product_shell_draw_list.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <set>

namespace billiardgl::render::ui {
namespace {

constexpr UiColor kBackground{0.012f, 0.024f, 0.031f, 0.98f};
constexpr UiColor kInspectorBackground{0.012f, 0.024f, 0.031f, 0.82f};
constexpr UiColor kPanel{0.027f, 0.047f, 0.059f, 0.94f};
constexpr UiColor kPanelDisabled{0.035f, 0.045f, 0.050f, 0.72f};
constexpr UiColor kInspectorPanel{0.027f, 0.047f, 0.059f, 0.84f};
constexpr UiColor kInspectorPanelDisabled{0.035f, 0.045f, 0.050f, 0.80f};
constexpr UiColor kText{0.94f, 0.96f, 0.96f, 1.0f};
constexpr UiColor kSecondary{0.67f, 0.73f, 0.75f, 1.0f};
constexpr UiColor kDisabled{0.42f, 0.47f, 0.49f, 1.0f};
constexpr UiColor kAccent{0.15f, 0.78f, 0.66f, 1.0f};
constexpr UiColor kFocus{1.0f, 0.78f, 0.22f, 1.0f};
constexpr UiColor kError{0.95f, 0.30f, 0.28f, 1.0f};

product_hud::HudRect intersect(const product_hud::HudRect& first,
    const product_hud::HudRect& second)
{
    const float x0 = std::max(first.x, second.x);
    const float y0 = std::max(first.y, second.y);
    const float x1 = std::min(first.x + first.width, second.x + second.width);
    const float y1 = std::min(first.y + first.height, second.y + second.height);
    return {x0, y0, std::max(0.0f, x1 - x0),
        std::max(0.0f, y1 - y0)};
}

bool empty(const product_hud::HudRect& rect)
{
    return rect.width <= 0.0f || rect.height <= 0.0f;
}

void addPrimitive(HudDrawList& draw, UiPrimitiveKind kind,
    const product_hud::HudRect& rect, const product_hud::HudRect& clip,
    const UiColor& color, const std::string& token)
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
    draw.primitives.push_back(primitive);
}

bool isDigit(std::uint32_t codepoint)
{
    return codepoint >= static_cast<std::uint32_t>('0') &&
        codepoint <= static_cast<std::uint32_t>('9');
}

bool addText(HudDrawList& draw, const SdfFontAtlas& atlas,
    const std::string& text, float x, float baseline, float pixelSize,
    const product_hud::HudRect& clip, const UiColor& color,
    bool tabularDigits = true)
{
    const TextMeasureResult measured = atlas.measureUtf8(
        text, pixelSize, tabularDigits);
    if (!measured.ok) {
        draw.errorCode = measured.errorCode;
        return false;
    }
    std::vector<std::uint32_t> codepoints;
    std::string error;
    if (!decodeUtf8(text, codepoints, error)) {
        draw.errorCode = error;
        return false;
    }
    const float scale = pixelSize / atlas.pixelSize();
    float penOffset = 0.0f;
    float minimumInkX = 0.0f;
    float maximumInkX = 0.0f;
    float minimumInkY = 0.0f;
    float maximumInkY = 0.0f;
    bool hasInk = false;
    for (const std::uint32_t codepoint : codepoints) {
        const GlyphRecord* glyph = atlas.findGlyph(codepoint);
        if (glyph == nullptr) {
            draw.errorCode = "font_glyph_not_prebaked";
            return false;
        }
        const float advance = tabularDigits && isDigit(codepoint) ?
            atlas.tabularDigitAdvancePx() : glyph->advancePx;
        const float center = tabularDigits && isDigit(codepoint) ?
            (advance - glyph->advancePx) * 0.5f * scale : 0.0f;
        if (glyph->bitmapWidth > 0 && glyph->bitmapHeight > 0) {
            const float left = penOffset + center +
                glyph->bearingXPx * scale;
            const float top = -glyph->bearingYPx * scale;
            const float right = left + glyph->bitmapWidth * scale;
            const float bottom = top + glyph->bitmapHeight * scale;
            if (!hasInk) {
                minimumInkX = left;
                maximumInkX = right;
                minimumInkY = top;
                maximumInkY = bottom;
                hasInk = true;
            } else {
                minimumInkX = std::min(minimumInkX, left);
                maximumInkX = std::max(maximumInkX, right);
                minimumInkY = std::min(minimumInkY, top);
                maximumInkY = std::max(maximumInkY, bottom);
            }
        }
        penOffset += advance * scale;
    }
    if (hasInk) {
        if (maximumInkY - minimumInkY > clip.height + 0.02f) {
            draw.errorCode = "text_ink_exceeds_clip:" + text;
            return false;
        }
        if (maximumInkX - minimumInkX <= clip.width + 0.02f)
            x = std::clamp(x, clip.x - minimumInkX,
                clip.x + clip.width - maximumInkX);
        baseline = std::clamp(baseline, clip.y - minimumInkY,
            clip.y + clip.height - maximumInkY);
    }
    const std::uint32_t textRunIndex = static_cast<std::uint32_t>(
        draw.textRuns.size());
    draw.textRuns.push_back(
        {text, clip, pixelSize, measured.width, x, baseline,
            tabularDigits});
    float pen = x;
    std::uint32_t codepointOrdinal = 0;
    for (std::uint32_t codepoint : codepoints) {
        const GlyphRecord* glyph = atlas.findGlyph(codepoint);
        if (glyph == nullptr) {
            draw.errorCode = "font_glyph_not_prebaked";
            return false;
        }
        const float advance = tabularDigits && isDigit(codepoint) ?
            atlas.tabularDigitAdvancePx() : glyph->advancePx;
        const float center = tabularDigits && isDigit(codepoint) ?
            (advance - glyph->advancePx) * 0.5f * scale : 0.0f;
        if (glyph->atlasWidth > 0 && glyph->atlasHeight > 0) {
            const product_hud::HudRect raw{
                pen + center + glyph->bearingXPx * scale -
                    glyph->sdfSpread * scale,
                baseline - glyph->bearingYPx * scale -
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
                    pen + center + glyph->bearingXPx * scale,
                    baseline - glyph->bearingYPx * scale,
                    glyph->bitmapWidth * scale,
                    glyph->bitmapHeight * scale};
                primitive.glyphInkRect = intersect(
                    primitive.glyphInkSourceRect, clip);
                draw.primitives.push_back(primitive);
                ++draw.glyphQuadCount;
            }
        }
        pen += advance * scale;
        ++codepointOrdinal;
    }
    return true;
}

float fittedSize(const SdfFontAtlas& atlas, const std::string& text,
    float preferred, float minimum, float available)
{
    const TextMeasureResult measured = atlas.measureUtf8(text, preferred, true);
    if (!measured.ok || measured.width <= available) return preferred;
    return std::max(minimum, preferred * available / measured.width);
}

std::vector<std::string> semanticWrappedLines(const SdfFontAtlas& atlas,
    const std::string& text, float pixelSize, float available)
{
    const TextMeasureResult whole = atlas.measureUtf8(text, pixelSize, true);
    // Leave room for the SDF spread beyond the advance box.  A string that
    // only just fits by advances can still lose the final glyph's ink.
    // Wrapping is driven close to the advance width; the final placement
    // below validates and clamps the real bitmap ink bounds, so this does not
    // rely on an arbitrary large percentage margin.
    constexpr float kInkSafeWidthFraction = 0.98f;
    if (!whole.ok || whole.width <= available * kInkSafeWidthFraction)
        return {text};

    std::vector<std::string> lines;
    std::string remaining = text;
    while (!remaining.empty()) {
        const TextMeasureResult measured = atlas.measureUtf8(
            remaining, pixelSize, true);
        if (!measured.ok ||
            measured.width <= available * kInkSafeWidthFraction) {
            lines.push_back(remaining);
            break;
        }
        std::size_t chosen = 0;
        for (std::size_t index = 0; index < remaining.size(); ++index) {
            const unsigned char value = static_cast<unsigned char>(
                remaining[index]);
            const bool semanticBreak = remaining[index] == ' ' ||
                remaining[index] == '_' || remaining[index] == '-' ||
                remaining[index] == ';';
            if (!semanticBreak) continue;
            const std::size_t end = index + 1;
            const auto prefix = atlas.measureUtf8(
                remaining.substr(0, end), pixelSize, true);
            if (prefix.ok &&
                prefix.width <= available * kInkSafeWidthFraction)
                chosen = end;
        }
        // A future identifier may contain no separator.  Keep every UTF-8
        // byte by falling back to the longest complete codepoint prefix.
        if (chosen == 0) {
            for (std::size_t end = 1; end <= remaining.size(); ++end) {
                if (end < remaining.size() &&
                    (static_cast<unsigned char>(remaining[end]) & 0xc0u) ==
                        0x80u)
                    continue;
                const auto prefix = atlas.measureUtf8(
                    remaining.substr(0, end), pixelSize, true);
                if (!prefix.ok ||
                    prefix.width > available * kInkSafeWidthFraction)
                    break;
                chosen = end;
            }
        }
        if (chosen == 0 || chosen >= remaining.size()) {
            lines.push_back(remaining);
            break;
        }
        lines.push_back(remaining.substr(0, chosen));
        remaining.erase(0, chosen);
    }
    return lines;
}

bool addWrappedText(HudDrawList& draw, const SdfFontAtlas& atlas,
    const std::vector<std::string>& lines,
    const product_hud::HudRect& rect, float top, float pixelSize,
    float lineHeight, const UiColor& color)
{
    if (top < rect.y || top + lineHeight * lines.size() >
            rect.y + rect.height + 0.02f) {
        draw.errorCode = "text_wrap_failed:vertical_space:";
        for (const auto& line : lines) draw.errorCode += line;
        draw.errorCode += ":top=" + std::to_string(top) +
            ":line_height=" + std::to_string(lineHeight) +
            ":line_count=" + std::to_string(lines.size()) +
            ":rect_y=" + std::to_string(rect.y) +
            ":rect_height=" + std::to_string(rect.height);
        return false;
    }
    bool ok = true;
    for (std::size_t index = 0; index < lines.size(); ++index) {
        const product_hud::HudRect lineRect{rect.x,
            top + lineHeight * static_cast<float>(index), rect.width,
            lineHeight};
        ok = addText(draw, atlas, lines[index], lineRect.x,
            lineRect.y + pixelSize, pixelSize, lineRect, color) && ok;
    }
    return ok;
}

bool addFittedText(HudDrawList& draw, const SdfFontAtlas& atlas,
    const std::string& text, const product_hud::HudRect& rect, float top,
    float preferred, float minimum, const UiColor& color)
{
    const float size = fittedSize(atlas, text, preferred, minimum, rect.width);
    return addText(draw, atlas, text, rect.x, top + size,
        size, rect, color);
}

void addBorder(HudDrawList& draw, const product_hud::HudRect& rect,
    const product_hud::HudRect& safe, float thickness,
    const UiColor& color, const std::string& token)
{
    addPrimitive(draw, UiPrimitiveKind::Line,
        {rect.x, rect.y, rect.width, thickness}, safe, color, token);
    addPrimitive(draw, UiPrimitiveKind::Line,
        {rect.x, rect.y + rect.height - thickness, rect.width, thickness},
        safe, color, token);
    addPrimitive(draw, UiPrimitiveKind::Line,
        {rect.x, rect.y, thickness, rect.height}, safe, color, token);
    addPrimitive(draw, UiPrimitiveKind::Line,
        {rect.x + rect.width - thickness, rect.y, thickness, rect.height},
        safe, color, token);
}

float insetOverlapArea(const product_hud::HudRect& first,
    const product_hud::HudRect& second)
{
    const product_hud::HudRect overlap = intersect(first, second);
    return overlap.width * overlap.height;
}

bool insetSame(float first, float second, float epsilon = 1e-3f)
{
    return std::fabs(first - second) <= epsilon;
}

bool insetSameRect(const product_hud::HudRect& first,
    const product_hud::HudRect& second)
{
    return insetSame(first.x, second.x) && insetSame(first.y, second.y) &&
        insetSame(first.width, second.width) &&
        insetSame(first.height, second.height);
}

void appendChrome(TrajectoryInsetChromeDrawList& draw,
    TrajectoryInsetChromeKind kind, const product_hud::HudRect& rect,
    const std::string& token, float opacity = 1.0f,
    const std::string& sourceToken = {},
    const product_hud::HudRect& sourceRect = {})
{
    if (empty(rect)) return;
    draw.primitives.push_back({kind, rect, token, opacity, true,
        sourceToken, sourceRect});
}

void appendOutsideCutout(TrajectoryInsetChromeDrawList& draw,
    const TrajectoryInsetChromePrimitive& source,
    const product_hud::HudRect& cutout, bool retainSourceIdentity = true)
{
    const product_hud::HudRect overlap = intersect(source.rect, cutout);
    const std::string identity = retainSourceIdentity ? source.token : "";
    if (empty(overlap)) {
        appendChrome(draw, source.kind, source.rect, source.token,
            source.opacity, identity, source.rect);
        return;
    }
    appendChrome(draw, source.kind,
        {source.rect.x, source.rect.y, source.rect.width,
            overlap.y - source.rect.y}, source.token, source.opacity,
        identity, source.rect);
    appendChrome(draw, source.kind,
        {source.rect.x, overlap.y + overlap.height, source.rect.width,
            source.rect.y + source.rect.height -
                (overlap.y + overlap.height)},
        source.token, source.opacity, identity, source.rect);
    appendChrome(draw, source.kind,
        {source.rect.x, overlap.y, overlap.x - source.rect.x,
            overlap.height}, source.token, source.opacity,
        identity, source.rect);
    appendChrome(draw, source.kind,
        {overlap.x + overlap.width, overlap.y,
            source.rect.x + source.rect.width -
                (overlap.x + overlap.width), overlap.height},
        source.token, source.opacity, identity, source.rect);
}

std::vector<product_hud::HudRect> outsideCutoutRects(
    const product_hud::HudRect& source, const product_hud::HudRect& cutout)
{
    const product_hud::HudRect overlap = intersect(source, cutout);
    if (empty(overlap)) return {source};
    const std::array<product_hud::HudRect, 4> candidates{{
        {source.x, source.y, source.width, overlap.y - source.y},
        {source.x, overlap.y + overlap.height, source.width,
            source.y + source.height - (overlap.y + overlap.height)},
        {source.x, overlap.y, overlap.x - source.x, overlap.height},
        {overlap.x + overlap.width, overlap.y,
            source.x + source.width - (overlap.x + overlap.width),
            overlap.height}}};
    std::vector<product_hud::HudRect> result;
    for (const auto& rect : candidates)
        if (!empty(rect)) result.push_back(rect);
    return result;
}

}  // namespace

TrajectoryInsetChromeDrawList buildTrajectoryInsetChromeDrawList(
    const TrajectoryInsetShellLayout& layout,
    const std::vector<TrajectoryInsetChromePrimitive>& surrounding)
{
    TrajectoryInsetChromeDrawList result;
    if (!layout.ok) {
        result.errorCode = "trajectory_inset_chrome_layout_invalid";
        return result;
    }
    result.visible = layout.insetVisible;
    if (!layout.insetVisible) {
        result.ok = true;
        return result;
    }
    result.cutoutExplicit = true;
    result.transparentCutoutRect = layout.plotCutoutRect;
    std::set<std::string> sourceTokens;
    for (const auto& source : surrounding) {
        if (source.token.empty() || source.opacity <= 0.0f ||
            source.opacity > 1.0f || source.rect.width <= 0.0f ||
            source.rect.height <= 0.0f ||
            source.token.rfind("trajectory_inset_", 0) == 0 ||
            !source.excludedFromTrajectoryDenominator ||
            !sourceTokens.insert(source.token).second) {
            result.errorCode = "trajectory_inset_surrounding_primitive_invalid";
            return result;
        }
        result.coverageClaims.push_back({source.kind, source.token, source.rect,
            outsideCutoutRects(source.rect, layout.plotCutoutRect),
            source.rect.width * source.rect.height -
                insetOverlapArea(source.rect, layout.plotCutoutRect),
            source.opacity, source.excludedFromTrajectoryDenominator});
        appendOutsideCutout(result, source, layout.plotCutoutRect);
    }
    const TrajectoryInsetChromePrimitive panel{
        TrajectoryInsetChromeKind::OpaquePanel, layout.panelRect,
        "trajectory_inset_panel", 0.94f, true};
    appendOutsideCutout(result, panel, layout.plotCutoutRect, false);
    for (std::size_t index = 0; index < layout.borderRects.size(); ++index)
        appendChrome(result, TrajectoryInsetChromeKind::Border,
            layout.borderRects[index],
            "trajectory_inset_border_" + std::to_string(index));
    const std::array<product_hud::HudRect, 7> textRects{{layout.titleRect,
        layout.orientationLabelRects[0], layout.orientationLabelRects[1],
        layout.legendRects[0], layout.legendRects[1], layout.legendRects[2],
        layout.toggleLabelRect}};
    result.orderedText = {layout.title, layout.orientationLabels[0],
        layout.orientationLabels[1], layout.legendEntries[0],
        layout.legendEntries[1], layout.legendEntries[2], layout.toggleLabel};
    for (std::size_t index = 0; index < textRects.size(); ++index)
        appendChrome(result, TrajectoryInsetChromeKind::Text,
            textRects[index],
            "trajectory_inset_text_" + std::to_string(index));
    appendChrome(result, TrajectoryInsetChromeKind::Toggle,
        layout.toggleRect, "trajectory_inset_toggle");
    if (layout.insetControlFocused)
        appendChrome(result, TrajectoryInsetChromeKind::Focus,
            layout.focusRect, "trajectory_inset_focus");
    result.ok = true;
    return result;
}

TrajectoryInsetChromeAudit auditTrajectoryInsetChromeDrawList(
    const TrajectoryInsetShellLayout& layout,
    const TrajectoryInsetChromeDrawList& draw,
    const std::vector<TrajectoryInsetChromePrimitive>& surrounding)
{
    TrajectoryInsetChromeAudit audit;
    if (!layout.ok || !draw.ok || draw.visible != layout.insetVisible) {
        audit.errorCode = "trajectory_inset_chrome_state_invalid";
        return audit;
    }
    if (!layout.insetVisible) {
        if (!draw.primitives.empty() || draw.cutoutExplicit ||
            draw.trajectorySemanticMemberCount != 0 ||
            draw.trajectoryProbeCount != 0 ||
            draw.trajectoryDenominatorContribution != 0) {
            audit.errorCode = "trajectory_inset_suppressed_draw_not_empty";
            return audit;
        }
        audit.ok = true;
        return audit;
    }
    if (!draw.cutoutExplicit ||
        !insetSameRect(draw.transparentCutoutRect, layout.plotCutoutRect) ||
        draw.trajectorySemanticMemberCount != 0 ||
        draw.trajectoryProbeCount != 0 ||
        draw.trajectoryDenominatorContribution != 0 ||
        draw.orderedText != std::vector<std::string>{layout.title,
            layout.orientationLabels[0], layout.orientationLabels[1],
            layout.legendEntries[0], layout.legendEntries[1],
            layout.legendEntries[2], layout.toggleLabel}) {
        audit.errorCode = "trajectory_inset_chrome_contract_invalid";
        return audit;
    }
    for (const auto& primitive : draw.primitives) {
        if (primitive.opacity <= 0.0f || primitive.opacity > 1.0f ||
            !primitive.excludedFromTrajectoryDenominator ||
            primitive.rect.width <= 0.0f || primitive.rect.height <= 0.0f ||
            insetOverlapArea(primitive.rect,
                draw.transparentCutoutRect) > 0.0f) {
            audit.errorCode = "trajectory_inset_cutout_covered";
            return audit;
        }
    }
    const std::array<product_hud::HudRect, 8> expectedRects{{
        layout.titleRect, layout.orientationLabelRects[0],
        layout.orientationLabelRects[1], layout.legendRects[0],
        layout.legendRects[1], layout.legendRects[2],
        layout.toggleLabelRect, layout.toggleRect}};
    for (std::size_t index = 0; index < expectedRects.size(); ++index) {
        const std::string token = index == 7 ? "trajectory_inset_toggle" :
            "trajectory_inset_text_" + std::to_string(index);
        const auto count = std::count_if(draw.primitives.begin(),
            draw.primitives.end(), [&](const auto& primitive) {
                return primitive.token == token;
            });
        const auto found = std::find_if(draw.primitives.begin(),
            draw.primitives.end(), [&](const auto& primitive) {
                return primitive.token == token;
            });
        const auto expectedKind = index == 7 ?
            TrajectoryInsetChromeKind::Toggle : TrajectoryInsetChromeKind::Text;
        if (count != 1 || found->kind != expectedKind ||
            !insetSameRect(found->rect, expectedRects[index])) {
            audit.errorCode = "trajectory_inset_chrome_ui_mapping_invalid";
            return audit;
        }
    }
    const auto focusCount = std::count_if(draw.primitives.begin(),
        draw.primitives.end(), [](const auto& primitive) {
            return primitive.token == "trajectory_inset_focus";
        });
    if (focusCount != (layout.insetControlFocused ? 1 : 0)) {
        audit.errorCode = "trajectory_inset_focus_multiplicity_invalid";
        return audit;
    }
    if (layout.insetControlFocused) {
        const auto focus = std::find_if(draw.primitives.begin(),
            draw.primitives.end(), [](const auto& primitive) {
                return primitive.token == "trajectory_inset_focus";
            });
        if (focus->kind != TrajectoryInsetChromeKind::Focus ||
            !insetSameRect(focus->rect, layout.focusRect)) {
            audit.errorCode = "trajectory_inset_focus_mapping_invalid";
            return audit;
        }
    }
    for (std::size_t index = 0; index < layout.borderRects.size(); ++index) {
        const std::string token = "trajectory_inset_border_" +
            std::to_string(index);
        const auto count = std::count_if(draw.primitives.begin(),
            draw.primitives.end(), [&](const auto& primitive) {
                return primitive.token == token;
            });
        const auto found = std::find_if(draw.primitives.begin(),
            draw.primitives.end(), [&](const auto& primitive) {
                return primitive.token == token;
            });
        if (count != 1 || found->kind != TrajectoryInsetChromeKind::Border ||
            !insetSameRect(found->rect, layout.borderRects[index])) {
            audit.errorCode = "trajectory_inset_border_mapping_invalid";
            return audit;
        }
    }
    const auto expectedPanelRects = outsideCutoutRects(
        layout.panelRect, layout.plotCutoutRect);
    std::vector<product_hud::HudRect> observedPanelRects;
    for (const auto& primitive : draw.primitives)
        if (primitive.token == "trajectory_inset_panel") {
            if (primitive.kind != TrajectoryInsetChromeKind::OpaquePanel ||
                !primitive.sourceToken.empty()) {
                audit.errorCode = "trajectory_inset_panel_identity_invalid";
                return audit;
            }
            observedPanelRects.push_back(primitive.rect);
        }
    if (observedPanelRects.size() != expectedPanelRects.size()) {
        audit.errorCode = "trajectory_inset_panel_multiplicity_invalid";
        return audit;
    }
    for (std::size_t index = 0; index < expectedPanelRects.size(); ++index)
        if (!insetSameRect(observedPanelRects[index], expectedPanelRects[index])) {
            audit.errorCode = "trajectory_inset_panel_shape_invalid";
            return audit;
        }
    if (draw.coverageClaims.size() != surrounding.size()) {
        audit.errorCode = "trajectory_inset_coverage_claim_count_invalid";
        return audit;
    }
    std::set<std::string> claimed;
    for (std::size_t claimIndex = 0;
            claimIndex < draw.coverageClaims.size(); ++claimIndex) {
        const auto& claim = draw.coverageClaims[claimIndex];
        const auto& source = surrounding[claimIndex];
        const auto recomputedRects = outsideCutoutRects(
            claim.sourceRect, draw.transparentCutoutRect);
        const float recomputedArea = claim.sourceRect.width *
            claim.sourceRect.height - insetOverlapArea(
                claim.sourceRect, draw.transparentCutoutRect);
        if (claim.sourceToken.empty() ||
            !claimed.insert(claim.sourceToken).second ||
            claim.kind != source.kind || claim.sourceToken != source.token ||
            !insetSameRect(claim.sourceRect, source.rect) ||
            !insetSame(claim.opacity, source.opacity) ||
            claim.excludedFromTrajectoryDenominator !=
                source.excludedFromTrajectoryDenominator ||
            !insetSame(claim.expectedVisibleArea, recomputedArea, 0.05f) ||
            claim.orderedExpectedVisibleRects.size() != recomputedRects.size()) {
            audit.errorCode = "trajectory_inset_coverage_claim_invalid";
            return audit;
        }
        for (std::size_t index = 0; index < recomputedRects.size(); ++index)
            if (!insetSameRect(claim.orderedExpectedVisibleRects[index],
                    recomputedRects[index])) {
                audit.errorCode = "trajectory_inset_coverage_claim_shape_invalid";
                return audit;
            }
        float area = 0.0f;
        std::vector<product_hud::HudRect> observedRects;
        for (const auto& primitive : draw.primitives) {
            if (primitive.sourceToken != claim.sourceToken) continue;
            if (primitive.kind != claim.kind ||
                primitive.token != claim.sourceToken ||
                !insetSameRect(primitive.sourceRect, claim.sourceRect) ||
                !insetSame(primitive.opacity, claim.opacity) ||
                primitive.excludedFromTrajectoryDenominator !=
                    claim.excludedFromTrajectoryDenominator) {
                audit.errorCode = "trajectory_inset_coverage_identity_drift";
                return audit;
            }
            area += primitive.rect.width * primitive.rect.height;
            observedRects.push_back(primitive.rect);
        }
        if (!insetSame(area, claim.expectedVisibleArea, 0.05f) ||
            observedRects.size() != claim.orderedExpectedVisibleRects.size()) {
            audit.errorCode = "trajectory_inset_coverage_area_drift";
            return audit;
        }
        for (std::size_t index = 0; index < observedRects.size(); ++index) {
            if (!insetSameRect(observedRects[index],
                    claim.orderedExpectedVisibleRects[index])) {
                audit.errorCode = "trajectory_inset_coverage_shape_drift";
                return audit;
            }
        }
    }
    for (const auto& primitive : draw.primitives) {
        if (!primitive.sourceToken.empty() &&
            claimed.count(primitive.sourceToken) == 0) {
            audit.errorCode = "trajectory_inset_unclaimed_surrounding_piece";
            return audit;
        }
    }
    std::size_t expectedPrimitiveCount = expectedPanelRects.size() +
        layout.borderRects.size() + expectedRects.size() +
        (layout.insetControlFocused ? 1u : 0u);
    for (const auto& claim : draw.coverageClaims)
        expectedPrimitiveCount += claim.orderedExpectedVisibleRects.size();
    if (draw.primitives.size() != expectedPrimitiveCount) {
        audit.errorCode = "trajectory_inset_generated_chrome_multiplicity_invalid";
        return audit;
    }
    audit.ok = true;
    return audit;
}

ShellDrawList ShippingScreenDrawListBuilder::build(
    const ProductShellSnapshot& model, const ShellViewport& viewport,
    const SdfFontAtlas& atlas) const
{
    ShellDrawList result;
    result.screen = model.screen;
    result.viewport = viewport;
    HudDrawList& draw = result.draw;
    if (!atlas.initialized() || !atlas.frozen()) {
        draw.errorCode = "font_atlas_not_initialized";
        return result;
    }
    const ShellScreenLayout layout = buildShippingScreenLayout(model, viewport);
    if (!layout.ok) {
        draw.errorCode = layout.errorCode;
        return result;
    }
    result.deviceScale = layout.deviceScale;
    result.effectiveUiScale = layout.effectiveUiScale;
    result.reflowed = layout.reflowed;
    result.elements = layout.elements;
    const float base = layout.baseScale;
    const float ui = layout.effectiveUiScale;
    draw.safeRect = layout.safeRect;
    draw.atlasCacheKey = atlas.cacheKey();
    draw.atlasRasterizationCount = atlas.rasterizationCount();
    const auto drawToggle = [&]() {
        if (!layout.boundedInspector) return true;
        const auto& rect = layout.inspectorToggleRect;
        addPrimitive(draw, UiPrimitiveKind::Panel, rect, draw.safeRect, kInspectorPanel, "inspector_toggle");
        if (layout.inspectorCollapsed)
            addBorder(draw, rect, draw.safeRect, std::max(2.0f, 2.0f * base),
                kFocus, "keyboard_focus_outline");
        addPrimitive(draw, UiPrimitiveKind::Icon,
            {rect.x + 8.0f * base, rect.y + 11.0f * base,
                12.0f * base, 12.0f * base}, rect, kAccent, "vertical_arrows");
        const product_hud::HudRect label{rect.x + 26.0f * base, rect.y + 6.0f * base,
            rect.width - 34.0f * base, rect.height - 12.0f * base};
        const std::string panelName = model.screen == ProductScreen::TrainingSession ? "Training panel" : "Replay panel";
        bool ok = addFittedText(draw, atlas, panelName,
            label, label.y, 14.0f * base, 14.0f * base, kAccent);
        const bool hasRoute =
            (model.trajectoryOverlay.predicted.visible && model.trajectoryOverlay.predicted.samples.size() >= 2) ||
            (model.trajectoryOverlay.actual.visible && model.trajectoryOverlay.actual.samples.size() >= 2);
        if (model.trajectoryInsetEnabled && !hasRoute) {
            const product_hud::HudRect hint{rect.x, rect.y + rect.height + 4.0f * base,
                rect.width, 44.0f * base};
            addPrimitive(draw, UiPrimitiveKind::Panel, hint, draw.safeRect,
                kInspectorPanel, "trajectory_empty_hint");
            for (int line = 0; line < 2; ++line) {
                const product_hud::HudRect row{hint.x + 8.0f * base,
                    hint.y + (3.0f + 20.0f * line) * base,
                    hint.width - 16.0f * base, 18.0f * base};
                const std::string text = line == 0 ? "No trajectory yet" :
                    model.screen == ProductScreen::TrainingSession ? "Run preview" : "Play or step";
                ok = addFittedText(draw, atlas, text, row, row.y,
                    14.0f * base, 14.0f * base, kSecondary) && ok;
            }
        }
        return ok;
    };
    if (layout.inspectorCollapsed) {
        // Keep the goal and result visible without reopening the details panel.
        const float textSize = 14.0f * base;
        const bool compactTraining = model.manualInspectorPolicy && model.screen == ProductScreen::TrainingSession;
        const product_hud::HudRect strip = layout.collapsedSummaryRect;
        addPrimitive(draw, UiPrimitiveKind::Panel, strip, draw.safeRect,
            kInspectorPanel, "shell_footer_panel");
        const product_hud::HudRect text{strip.x + 8.0f * base,
            strip.y + 6.0f * base, strip.width - 16.0f * base, strip.height - 12.0f * base};
        if (compactTraining) {
            std::vector<std::string> lines;
            lines.push_back(model.trainingTitle.empty() ? "Training" : model.trainingTitle);
            lines.push_back(model.trainingGoal.empty() ? "Goal: see Training panel" : model.trainingGoal);
            const auto& state = model.training;
            const bool sequenceContinue = state.teachingStepCount > 1 &&
                state.attemptFacts.shotCount > 0 &&
                state.teachingStepIndex == state.attemptFacts.shotCount &&
                state.state == training::TrainingProductState::AttemptActive;
            lines.push_back(state.shotInProgress ? "Shot in motion - evaluating when stopped" :
                state.evaluation.succeeded ? "PASSED / score " + std::to_string(state.evaluation.score) :
                sequenceContinue ? "Target pocketed - prepare the next shot" :
                state.attemptFacts.shotCount > 0 ? "Shot complete - try again" : "Ready - aim, set power, then shoot");
            lines.push_back(!model.errorText.empty() ? model.errorText :
                sequenceContinue ? "Prepare shot applies hints; click the table to shoot" :
                !state.evaluation.feedback.empty() ? state.evaluation.feedback : "Tab: Aim / Observe | R: reset drill");
            draw.ok = true;
            float lineY = text.y;
            for (const auto& line : lines) {
                const auto wrapped = semanticWrappedLines(atlas, line, textSize, text.width);
                for (const auto& part : wrapped) {
                    const product_hud::HudRect row{text.x, lineY, text.width, 24.0f * base};
                    if (row.y + row.height > text.y + text.height + 0.001f) {
                        draw.ok = false;
                        draw.errorCode = "training_summary_wrapped_content_overflow";
                        return result;
                    }
                    draw.ok = addFittedText(draw, atlas, part, row, lineY,
                        textSize, textSize, kText) && draw.ok;
                    lineY += 25.0f * base;
                }
            }
        } else draw.ok = addFittedText(draw, atlas,
            model.screen == ProductScreen::TrainingSession &&
                model.training.state == training::TrainingProductState::AttemptEvaluated ?
                "Attempt finished - select Retry shot" :
                "Details closed - use panel button", text, text.y,
            textSize, textSize, kSecondary);
        for (const auto& element : layout.elements) {
            addPrimitive(draw, UiPrimitiveKind::Panel, element.rect,
                draw.safeRect, kInspectorPanel, element.id);
            auto label = element.rect;
            const auto item = std::find_if(model.items.begin(), model.items.end(),
                [&](const auto& value) { return value.id == element.id; });
            addPrimitive(draw, UiPrimitiveKind::Icon,
                {element.rect.x + 6.0f * base,
                    element.rect.y + (element.rect.height - 24.0f * base) * 0.5f,
                    24.0f * base, 24.0f * base}, element.rect, kAccent,
                item != model.items.end() && !item->shapeToken.empty() ?
                    item->shapeToken : "item_square");
            label.x += 30.0f * base;
            label.width -= 30.0f * base;
            label.x += 8.0f * base;
            label.y += 10.0f * base;
            label.width -= 16.0f * base;
            label.height -= 20.0f * base;
            const std::string title = element.id == "replay_pause" ?
                (model.replay.paused ? "Play" : "Pause") :
                element.id == "replay_step_tick" ? "Step one tick" :
                element.id == "replay_camera_reset" ? "Reset view" :
                element.id == "watch_training_reference" ? "Watch demo" :
                element.id == "prepare_training_shot" ? "Prepare shot" :
                element.id == "next_training" ? "Next drill" :
                model.training.teachingStepCount > 1 ? "Restart drill" : "Retry shot";
            draw.ok = addFittedText(draw, atlas, title, label, label.y,
                textSize, textSize, element.enabled ? kAccent : kSecondary) && draw.ok;
        }
        draw.ok = drawToggle() && draw.ok;
        return result;
    }
    addPrimitive(draw, UiPrimitiveKind::SolidRect, layout.inspectorRect,
        draw.safeRect, layout.boundedInspector ? kInspectorBackground :
            kBackground, layout.boundedInspector ?
            "shipping_bounded_inspector_background" :
            "shipping_shell_background");

    const float minimumText = layout.minimumText;
    const product_hud::HudRect& header = layout.headerRect;
    const product_hud::HudRect& footer = layout.footerRect;
    const product_hud::HudRect& body = layout.bodyRect;

    bool ok = true;
    addPrimitive(draw, UiPrimitiveKind::Panel, header, draw.safeRect,
        layout.boundedInspector ? kInspectorPanel : kPanel,
        "shell_header_panel");
    if (!layout.boundedInspector)
        ok = addFittedText(draw, atlas, model.breadcrumb,
            layout.breadcrumbLineRect, layout.breadcrumbLineRect.y,
            minimumText, minimumText, kAccent) && ok;
    ok = addFittedText(draw, atlas, model.title, layout.titleLineRect,
        layout.titleLineRect.y, minimumText * 1.65f,
        minimumText, kText) && ok;
    if (layout.boundedInspector && model.manualInspectorPolicy &&
        model.screen == ProductScreen::TrainingSession) {
        const auto subtitleLines = semanticWrappedLines(atlas,
            model.subtitle, minimumText, layout.subtitleLineRect.width);
        ok = addWrappedText(draw, atlas, subtitleLines,
            layout.subtitleLineRect, layout.subtitleLineRect.y, minimumText,
            minimumText * 1.30f, kSecondary) && ok;
    } else {
        ok = addFittedText(draw, atlas, model.subtitle,
            layout.subtitleLineRect, layout.subtitleLineRect.y, minimumText,
            minimumText, kSecondary) && ok;
    }

    if (!model.errorText.empty()) {
        const product_hud::HudRect& errorRect = layout.errorRect;
        addPrimitive(draw, UiPrimitiveKind::Panel, errorRect, draw.safeRect,
            kError, "error_banner_octagon");
        addPrimitive(draw, UiPrimitiveKind::Icon,
            {errorRect.x + 12.0f * base, errorRect.y + 10.0f * base,
                22.0f * ui, 22.0f * ui}, errorRect, kText,
            "error_cross_octagon");
        const product_hud::HudRect errorText{errorRect.x + 44.0f * ui,
            errorRect.y + 6.0f * base, errorRect.width - 54.0f * ui,
            errorRect.height - 12.0f * base};
        ok = addFittedText(draw, atlas, model.errorText, errorText,
            errorText.y, minimumText, minimumText, kText) && ok;
    }

    for (std::size_t index = 0; index < layout.elements.size(); ++index) {
        const product_hud::HudRect& rect = layout.elements[index].rect;
        const float itemHeight = rect.height;
        ShellItemViewModel fallback;
        fallback.id = "shell_empty";
        fallback.label = "No items available";
        fallback.value = "Nothing is being claimed available";
        fallback.shapeToken = "empty_box";
        fallback.enabled = false;
        const ShellItemViewModel& item = model.items.empty() ?
            fallback : model.items[index + layout.firstVisibleRow];
        addPrimitive(draw, UiPrimitiveKind::Panel, rect, draw.safeRect,
            layout.boundedInspector ?
                (item.enabled ? kInspectorPanel : kInspectorPanelDisabled) :
                (item.enabled ? kPanel : kPanelDisabled), "shell_item_panel");
        const bool manualTrainingItem = layout.boundedInspector &&
            model.manualInspectorPolicy &&
            model.screen == ProductScreen::TrainingSession;
        const bool compactReflowItem = !layout.boundedInspector &&
            layout.reflowed;
        const float iconSize = manualTrainingItem ?
            std::min(18.0f * ui, itemHeight * 0.20f) :
            compactReflowItem ?
            std::min(std::max(16.0f * base, 8.0f * ui),
                itemHeight * 0.20f) :
            std::min(30.0f * ui, itemHeight * 0.45f);
        const product_hud::HudRect iconRect =
            manualTrainingItem || compactReflowItem ?
            product_hud::HudRect{
                rect.x + rect.width - iconSize - 8.0f * base,
                rect.y + 8.0f * base, iconSize, iconSize} :
            product_hud::HudRect{
                rect.x + (compactReflowItem ? 8.0f : 12.0f) * base,
                rect.y + (rect.height - iconSize) * 0.5f,
                iconSize, iconSize};
        addPrimitive(draw, UiPrimitiveKind::Icon, iconRect, rect,
            item.enabled ? (item.selected ? kFocus : kAccent) : kDisabled,
            item.shapeToken.empty() ? "item_square" : item.shapeToken);
        if (item.selected) {
            addPrimitive(draw, UiPrimitiveKind::Icon,
                {rect.x + rect.width - 24.0f * ui,
                    rect.y + 10.0f * base, 14.0f * ui, 14.0f * ui},
                rect, kFocus, "selected_check_diamond");
        }
        if (item.focused) {
            addBorder(draw, rect, draw.safeRect, std::max(2.0f, 2.0f * base),
                kFocus, "keyboard_focus_outline");
            addPrimitive(draw, UiPrimitiveKind::Icon,
                {rect.x + 4.0f * base,
                    rect.y + (rect.height - 10.0f * ui) * 0.5f,
                    10.0f * ui, 10.0f * ui}, rect, kFocus,
                "focus_chevron");
        }
        const product_hud::HudRect textRect = manualTrainingItem ?
            product_hud::HudRect{rect.x + 8.0f * base,
                rect.y + 2.0f * base,
                iconRect.x - (rect.x + 16.0f * base),
                rect.height - 4.0f * base} : compactReflowItem ?
            product_hud::HudRect{
                rect.x + 14.0f * base,
                rect.y,
                rect.width - 22.0f * base,
                rect.height} :
            product_hud::HudRect{
                iconRect.x + iconRect.width + 12.0f * base,
                rect.y + (layout.boundedInspector ? 2.0f : 7.0f) * base,
                rect.x + rect.width - (iconRect.x + iconRect.width) -
                    (layout.reflowed ? 14.0f : 22.0f) * base,
                rect.height -
                    (layout.boundedInspector ? 4.0f : 14.0f) * base};
        const product_hud::HudRect labelTextRect = compactReflowItem ?
            product_hud::HudRect{textRect.x, textRect.y,
                iconRect.x - textRect.x - 6.0f * base, textRect.height} :
            textRect;
        const UiColor primary = item.enabled || item.readOnlyInformational ?
            kText : kDisabled;
        const UiColor valueColor = item.enabled ? kAccent :
            item.readOnlyInformational ? kSecondary : kDisabled;
        const bool wrappedTrainingInspector = layout.boundedInspector &&
            model.manualInspectorPolicy &&
            model.screen == ProductScreen::TrainingSession;
        float detailTop = textRect.y + minimumText *
            (model.manualInspectorPolicy ? 2.8f : 1.64f);
        if (wrappedTrainingInspector) {
            const float inspectorLineHeight = minimumText * 1.10f;
            const auto labelLines = semanticWrappedLines(atlas, item.label,
                minimumText, textRect.width);
            ok = addWrappedText(draw, atlas, labelLines, textRect,
                textRect.y, minimumText, inspectorLineHeight, primary) && ok;
            float cursor = textRect.y + inspectorLineHeight *
                static_cast<float>(labelLines.size());
            if (!item.value.empty()) {
                const auto valueLines = semanticWrappedLines(atlas,
                    item.value, minimumText, textRect.width);
                ok = addWrappedText(draw, atlas, valueLines, textRect,
                    cursor, minimumText, inspectorLineHeight,
                    valueColor) && ok;
                cursor += inspectorLineHeight *
                    static_cast<float>(valueLines.size());
            }
            detailTop = cursor;
        } else if (compactReflowItem) {
            // High UI scale on the native 720p drawable uses narrow cards.
            // Keep the icon and full text by stacking semantic label/value
            // lines in the ink-height budget instead of clipping either.
            // The compact 720p/135% card has exactly enough height for three
            // minimum-size lines (a label plus a two-line value).  Each line
            // owns its own clip, so 1.06 em preserves the bitmap ink without
            // relying on baseline clamping or overlapping adjacent runs.
            const float compactLineHeight = minimumText * 1.06f;
            const auto labelLines = semanticWrappedLines(atlas, item.label,
                minimumText, labelTextRect.width);
            ok = addWrappedText(draw, atlas, labelLines, labelTextRect,
                labelTextRect.y, minimumText, compactLineHeight,
                primary) && ok;
            if (!item.value.empty()) {
                const auto valueLines = semanticWrappedLines(atlas,
                    item.value, minimumText, labelTextRect.width);
                ok = addWrappedText(draw, atlas, valueLines, labelTextRect,
                    labelTextRect.y + compactLineHeight *
                        static_cast<float>(labelLines.size()),
                    minimumText, compactLineHeight,
                    valueColor) && ok;
            }
        } else {
            ok = addFittedText(draw, atlas, item.label, labelTextRect,
                labelTextRect.y, minimumText * 1.12f, minimumText,
                primary) && ok;
            if (!item.value.empty()) {
                ok = addFittedText(draw, atlas, item.value, textRect,
                    textRect.y + minimumText *
                        (layout.boundedInspector ?
                            (model.manualInspectorPolicy ? 1.4f : 0.82f) :
                            1.45f), minimumText, minimumText,
                    valueColor) && ok;
            }
        }
        // The 135% accessibility profile reflows dense screens into narrower
        // cards.  Keep the primary label/value pair at the guaranteed minimum
        // text size and omit tertiary prose in that compact representation;
        // drawing it as a single clipped line made the accessibility profile
        // claim content that was not actually readable.
        const bool requiredInspectorDetail = layout.boundedInspector &&
            ((item.id == "training_terminal" && wrappedTrainingInspector) ||
                item.id == "trajectory_predicted" ||
                item.id == "trajectory_actual" ||
                item.id == "training_replay_status" ||
                item.id == "watch_training_reference");
        if (requiredInspectorDetail && !item.detail.empty()) {
            if (wrappedTrainingInspector) {
                const auto detailLines = semanticWrappedLines(atlas,
                    item.detail, minimumText, textRect.width);
                ok = addWrappedText(draw, atlas, detailLines, textRect,
                    detailTop, minimumText, minimumText * 1.10f,
                    kSecondary) && ok;
            } else {
                ok = addFittedText(draw, atlas, item.detail, textRect,
                    detailTop, minimumText, minimumText, kSecondary) && ok;
            }
        } else if (!layout.reflowed && !item.detail.empty() &&
            itemHeight >= minimumText * 4.1f) {
            ok = addFittedText(draw, atlas, item.detail, textRect,
                // addFittedText converts this top coordinate to a baseline by
                // adding one em.  Keep a half-em below that baseline for the
                // font descender and the SDF spread; placing it at 2.85 em put
                // the baseline exactly on the clip bottom and turned y/g into
                // visibly clipped glyphs on the Retina product target.
                textRect.y + minimumText * 2.35f, minimumText,
                minimumText, kSecondary) && ok;
        }
    }

    addPrimitive(draw, UiPrimitiveKind::Panel, footer, draw.safeRect,
        layout.boundedInspector ? kInspectorPanel : kPanel,
        "shell_footer_panel");
    const product_hud::HudRect footerText{footer.x + 14.0f * base,
        footer.y + 2.0f * base, footer.width - 28.0f * base,
        footer.height - 4.0f * base};
    std::string compactFooter = model.footer;
    if (layout.maximumScrollRow > 0)
        compactFooter = "Scroll panel: " + std::to_string(layout.firstVisibleRow + 1) +
            "-" + std::to_string(layout.firstVisibleRow + layout.elements.size()) +
            "/" + std::to_string(model.items.size());
    if (layout.boundedInspector &&
        model.screen == ProductScreen::ReplayPlayback && layout.maximumScrollRow == 0) {
        std::string availableAction = "Play";
        for (const ShellItemViewModel& item : model.items)
            if (item.id == "replay_pause") availableAction = item.label;
        compactFooter = availableAction +
            ":Spc Rate:+/- Cam:C Seek:Pg";
    }
    const auto footerLines = semanticWrappedLines(atlas, compactFooter,
        minimumText, footerText.width * 0.96f);
    ok = addWrappedText(draw, atlas, footerLines, footerText, footerText.y,
        minimumText, minimumText * 1.10f, kSecondary) && ok;
    ok = drawToggle() && ok;
    if (!ok) return result;
    if (atlas.rasterizationCount() != draw.atlasRasterizationCount) {
        draw.errorCode = "font_rasterized_during_frame";
        return result;
    }
    draw.ok = true;
    return result;
}

}  // namespace billiardgl::render::ui
