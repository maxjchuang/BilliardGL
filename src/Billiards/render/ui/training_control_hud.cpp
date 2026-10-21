#include "render/ui/training_control_hud.h"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <iomanip>
#include <locale>
#include <sstream>
#include <utility>
#include <vector>

namespace billiardgl::render::ui {
namespace {

constexpr UiColor kPanel{0.012f, 0.024f, 0.031f, 0.88f};
constexpr UiColor kText{0.94f, 0.96f, 0.96f, 1.0f};
constexpr UiColor kSecondary{0.67f, 0.73f, 0.75f, 1.0f};
constexpr UiColor kAccent{0.15f, 0.78f, 0.66f, 1.0f};
constexpr UiColor kLocked{1.0f, 0.74f, 0.24f, 1.0f};

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

void addPrimitive(HudDrawList& draw, UiPrimitiveKind kind,
    const product_hud::HudRect& rect, const product_hud::HudRect& clip,
    const UiColor& color, const char* token)
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
    draw.primitives.push_back(std::move(primitive));
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
    const std::uint32_t textRunIndex = static_cast<std::uint32_t>(
        draw.textRuns.size());
    draw.textRuns.push_back({text, clip, pixelSize, measured.width, x,
        baseline, tabularDigits});
    std::vector<std::uint32_t> codepoints;
    std::string decodeError;
    if (!decodeUtf8(text, codepoints, decodeError)) {
        draw.errorCode = decodeError;
        return false;
    }
    const float scale = pixelSize / atlas.pixelSize();
    float penX = x;
    std::uint32_t codepointOrdinal = 0;
    for (const std::uint32_t codepoint : codepoints) {
        const GlyphRecord* glyph = atlas.findGlyph(codepoint);
        if (glyph == nullptr) {
            draw.errorCode = "font_glyph_not_prebaked";
            return false;
        }
        const float advance = tabularDigits && isDigit(codepoint) ?
            atlas.tabularDigitAdvancePx() : glyph->advancePx;
        const float centerOffset = tabularDigits && isDigit(codepoint) ?
            (advance - glyph->advancePx) * 0.5f * scale : 0.0f;
        if (glyph->atlasWidth > 0 && glyph->atlasHeight > 0) {
            const product_hud::HudRect raw{
                penX + centerOffset + glyph->bearingXPx * scale -
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
                    penX + centerOffset + glyph->bearingXPx * scale,
                    baseline - glyph->bearingYPx * scale,
                    glyph->bitmapWidth * scale,
                    glyph->bitmapHeight * scale};
                primitive.glyphInkRect = intersect(
                    primitive.glyphInkSourceRect, clip);
                draw.primitives.push_back(std::move(primitive));
                ++draw.glyphQuadCount;
            }
        }
        penX += advance * scale;
        ++codepointOrdinal;
    }
    return true;
}

float fittedSize(const SdfFontAtlas& atlas, const std::string& text,
    float preferred, float minimum, float width)
{
    const TextMeasureResult measured = atlas.measureUtf8(text, preferred, true);
    if (!measured.ok || measured.width <= width) return preferred;
    return std::max(minimum, preferred * width / measured.width);
}

bool addFittedText(HudDrawList& draw, const SdfFontAtlas& atlas,
    const std::string& text, const product_hud::HudRect& rect,
    float preferred, float minimum, const UiColor& color)
{
    const float size = fittedSize(atlas, text, preferred, minimum, rect.width);
    return addText(draw, atlas, text, rect.x, rect.y + size, size, rect,
        color);
}

std::string fixedSigned(float value)
{
    std::ostringstream output;
    output.imbue(std::locale::classic());
    output << std::showpos << std::fixed << std::setprecision(2) << value;
    return output.str();
}

std::string fixedOne(float value)
{
    std::ostringstream output;
    output.imbue(std::locale::classic());
    output << std::fixed << std::setprecision(1) << value;
    return output.str();
}

bool inside(const product_hud::HudRect& inner,
    const product_hud::HudRect& outer)
{
    constexpr float epsilon = 0.02f;
    return inner.x + epsilon >= outer.x && inner.y + epsilon >= outer.y &&
        inner.x + inner.width <= outer.x + outer.width + epsilon &&
        inner.y + inner.height <= outer.y + outer.height + epsilon;
}

}  // namespace

const char* trainingControlModeName(TrainingControlMode mode) noexcept
{
    switch (mode) {
    case TrainingControlMode::Observe: return "OBSERVE";
    case TrainingControlMode::Aim: return "AIM";
    case TrainingControlMode::Motion: return "MOTION";
    }
    return "OBSERVE";
}

TrainingControlHudResult buildTrainingControlHud(
    const PresentationSnapshot& presentation,
    const training::TrainingProductSnapshot& training,
    const ShellViewport& viewport,
    const product_hud::HudRect& safeRect,
    const product_hud::HudRect& availableRect,
    float userScale,
    const SdfFontAtlas& atlas)
{
    TrainingControlHudResult result;
    if (!atlas.initialized() || !atlas.frozen()) {
        result.errorCode = "training_control_hud_font_unavailable";
        return result;
    }
    if (viewport.logicalWidth <= 0 || viewport.logicalHeight <= 0 ||
        viewport.physicalWidth <= 0 || viewport.physicalHeight <= 0 ||
        !std::isfinite(viewport.dpi) || viewport.dpi <= 0.0f ||
        !std::isfinite(userScale) || userScale < 0.85f || userScale > 1.35f ||
        empty(safeRect) || empty(availableRect)) {
        result.errorCode = "training_control_hud_layout_invalid";
        return result;
    }

    result.mode = presentation.ballsMoving ? TrainingControlMode::Motion :
        presentation.hud.aimMode ? TrainingControlMode::Aim :
        TrainingControlMode::Observe;
    const auto& teachingHints = training.teachingHints;
    const bool runtimeAdjustmentReady =
        training.state == training::TrainingProductState::AttemptActive &&
        result.mode == TrainingControlMode::Aim &&
        !presentation.rules.gameOver && !presentation.gameOver;
    result.powerAdjustmentLocked = !runtimeAdjustmentReady ||
        !training.powerAdjustmentAllowed;
    result.tipAdjustmentLocked = !runtimeAdjustmentReady ||
        !training.tipAdjustmentAllowed;

    const float scaleX = static_cast<float>(viewport.physicalWidth) /
        viewport.logicalWidth;
    const float scaleY = static_cast<float>(viewport.physicalHeight) /
        viewport.logicalHeight;
    const float deviceScale = std::min(scaleX, scaleY);
    const float scale = std::max(deviceScale, userScale);
    const float margin = std::max(8.0f, 10.0f * deviceScale);
    const float preferredWidth = std::max(680.0f * scale,
        availableRect.width * 0.70f);
    const float panelWidth = std::min(availableRect.width,
        preferredWidth);
    const float panelHeight = std::min(availableRect.height * 0.28f,
        std::max(94.0f * scale, 88.0f));
    result.panelRect = {
        availableRect.x + (availableRect.width - panelWidth) * 0.5f,
        safeRect.y + safeRect.height - panelHeight,
        panelWidth, panelHeight};
    if (result.panelRect.y < availableRect.y + margin)
        result.panelRect.y = availableRect.y + margin;
    if (!inside(result.panelRect, safeRect) ||
        !inside(result.panelRect, availableRect) ||
        result.panelRect.width < 420.0f || result.panelRect.height < 72.0f) {
        result.errorCode = "training_control_hud_insufficient_space";
        return result;
    }

    HudDrawList& draw = result.draw;
    draw.atlasCacheKey = atlas.cacheKey();
    draw.atlasRasterizationCount = atlas.rasterizationCount();
    draw.safeRect = safeRect;
    addPrimitive(draw, UiPrimitiveKind::Panel, result.panelRect, safeRect,
        kPanel, "training_control_panel");

    const float inner = std::max(8.0f, 10.0f * deviceScale);
    const product_hud::HudRect content{
        result.panelRect.x + inner, result.panelRect.y + inner,
        result.panelRect.width - inner * 2.0f,
        result.panelRect.height - inner * 2.0f};
    const float gap = std::max(8.0f, 10.0f * deviceScale);
    const float usable = content.width - gap * 2.0f;
    // The 720p/Retina product profile needs the recommendation and both tip
    // axes at the contractual minimum size.  Give POWER enough horizontal
    // room for its numeric range, then stack the two tip axes vertically.
    const float powerWidth = usable * 0.47f;
    const float tipWidth = usable * 0.29f;
    const product_hud::HudRect powerRect{content.x, content.y,
        powerWidth, content.height};
    const product_hud::HudRect tipRect{powerRect.x + powerRect.width + gap,
        content.y, tipWidth, content.height};
    const product_hud::HudRect modeRect{tipRect.x + tipRect.width + gap,
        content.y, content.x + content.width -
            (tipRect.x + tipRect.width + gap), content.height};
    addPrimitive(draw, UiPrimitiveKind::Line,
        {powerRect.x + powerRect.width + gap * 0.5f, content.y, 1.0f,
            content.height}, result.panelRect, kSecondary,
        "training_control_separator");
    addPrimitive(draw, UiPrimitiveKind::Line,
        {tipRect.x + tipRect.width + gap * 0.5f, content.y, 1.0f,
            content.height}, result.panelRect, kSecondary,
        "training_control_separator");

    const bool validPower =
        std::isfinite(presentation.shotControl.requestedPower) &&
        presentation.shotControl.requestedPower >= 0.0f &&
        presentation.shotControl.requestedPower <= 200.0f;
    result.displayedPowerPercent = validPower ?
        presentation.shotControl.requestedPower * 0.5f : 0.0f;
    const std::string powerText = validPower ?
        "POWER " + std::to_string(static_cast<int>(std::lround(
            result.displayedPowerPercent))) + "%" : "POWER --";
    const float minimumText = std::max(13.0f * deviceScale,
        13.0f * userScale);
    const float preferredText = std::max(14.0f * deviceScale,
        14.0f * userScale);
    bool ok = addFittedText(draw, atlas, powerText,
        {powerRect.x, powerRect.y, powerRect.width,
        preferredText * 1.5f}, preferredText, minimumText, kText);
    const bool validRecommendedPower = teachingHints.available &&
        std::isfinite(teachingHints.minimumPowerPercent) &&
        std::isfinite(teachingHints.maximumPowerPercent) &&
        teachingHints.minimumPowerPercent >= 0.0f &&
        teachingHints.maximumPowerPercent <= 100.0f &&
        teachingHints.minimumPowerPercent <=
            teachingHints.maximumPowerPercent;
    const bool validRecommendedTip = teachingHints.available &&
        std::isfinite(teachingHints.tipXBallRadii) &&
        std::isfinite(teachingHints.tipYBallRadii) &&
        std::fabs(teachingHints.tipXBallRadii) <= 1.0f &&
        std::fabs(teachingHints.tipYBallRadii) <= 1.0f;
    if (teachingHints.available &&
        (!validRecommendedPower || !validRecommendedTip)) {
        result.errorCode = "training_control_hud_teaching_hints_invalid";
        return result;
    }
    {
        const std::string powerStatus = result.powerAdjustmentLocked ?
            "LOCKED" : "READY";
        ok = addFittedText(draw, atlas, powerStatus,
            {powerRect.x, powerRect.y + preferredText * 1.55f,
                powerRect.width, preferredText * 1.45f},
            minimumText, minimumText, kSecondary) && ok;
        if (validRecommendedPower) {
            const std::string recommended = "REC " +
                fixedOne(teachingHints.minimumPowerPercent) + "-" +
                fixedOne(teachingHints.maximumPowerPercent) + "%";
            ok = addFittedText(draw, atlas, recommended,
                {powerRect.x,
                    powerRect.y + preferredText * 1.55f +
                        minimumText * 1.15f,
                    powerRect.width, preferredText * 1.35f},
                minimumText, minimumText, kSecondary) && ok;
        }
    }
    constexpr int segmentCount = 10;
    const float segmentGap = std::max(2.0f, 2.5f * deviceScale);
    const float segmentWidth = (powerRect.width -
        segmentGap * (segmentCount - 1)) / segmentCount;
    const float segmentHeight = std::max(8.0f, 9.0f * deviceScale);
    const float segmentY = powerRect.y + powerRect.height - segmentHeight;
    if (validRecommendedPower) {
        const float rangeX = powerRect.x + powerRect.width *
            teachingHints.minimumPowerPercent / 100.0f;
        const float rangeWidth = std::max(2.0f, powerRect.width *
            (teachingHints.maximumPowerPercent -
                teachingHints.minimumPowerPercent) / 100.0f);
        addPrimitive(draw, UiPrimitiveKind::SolidRect,
            {rangeX, segmentY - std::max(3.0f, 3.0f * deviceScale),
                std::min(rangeWidth,
                    powerRect.x + powerRect.width - rangeX),
                std::max(2.0f, 2.0f * deviceScale)}, powerRect, kLocked,
            "training_power_recommended_range");
    }
    for (int segment = 0; segment < segmentCount; ++segment) {
        addPrimitive(draw, UiPrimitiveKind::SolidRect,
            {powerRect.x + segment * (segmentWidth + segmentGap), segmentY,
                segmentWidth, segmentHeight}, powerRect,
            validPower && segment * 10.0f < result.displayedPowerPercent ?
                kAccent : kSecondary,
            "training_power_segment");
    }

    const bool validTip = std::isfinite(presentation.shotControl.tipOffsetX) &&
        std::isfinite(presentation.shotControl.tipOffsetY) &&
        std::fabs(presentation.shotControl.tipOffsetX) <= 1.0f &&
        std::fabs(presentation.shotControl.tipOffsetY) <= 1.0f;
    result.displayedTipX = validTip ? presentation.shotControl.tipOffsetX : 0.0f;
    result.displayedTipY = validTip ? presentation.shotControl.tipOffsetY : 0.0f;
    const std::string tipXText = validTip ?
        "TIP X " + fixedSigned(result.displayedTipX) : "TIP X --";
    const std::string tipYText = validTip ?
        "Y " + fixedSigned(result.displayedTipY) : "Y --";
    ok = addFittedText(draw, atlas, tipXText,
        {tipRect.x, tipRect.y, tipRect.width, preferredText * 1.45f},
        preferredText, minimumText, kText) && ok;
    ok = addFittedText(draw, atlas, tipYText,
        {tipRect.x, tipRect.y + preferredText * 1.15f,
            tipRect.width, preferredText * 1.45f},
        minimumText, minimumText, kText) && ok;
    const float ballSize = std::min({tipRect.height - preferredText * 2.55f,
        tipRect.width * 0.34f, 48.0f * scale});
    const product_hud::HudRect ball{
        tipRect.x + std::max(0.0f, 4.0f * deviceScale),
        tipRect.y + tipRect.height - ballSize, ballSize, ballSize};
    addPrimitive(draw, UiPrimitiveKind::Icon, ball, tipRect, kText,
        "tip_ball_outline");
    const float markerSize = std::max(5.0f, ballSize * 0.12f);
    const float travel = ballSize * 0.34f;
    const product_hud::HudRect marker{
        ball.x + ball.width * 0.5f + result.displayedTipX * travel -
            markerSize * 0.5f,
        ball.y + ball.height * 0.5f - result.displayedTipY * travel -
            markerSize * 0.5f,
        markerSize, markerSize};
    if (validRecommendedTip) {
        const product_hud::HudRect target{
            ball.x + ball.width * 0.5f +
                teachingHints.tipXBallRadii * travel - markerSize * 0.75f,
            ball.y + ball.height * 0.5f -
                teachingHints.tipYBallRadii * travel - markerSize * 0.75f,
            markerSize * 1.5f, markerSize * 1.5f};
        addPrimitive(draw, UiPrimitiveKind::Icon, target, tipRect, kLocked,
            "tip_target");
        const product_hud::HudRect recommendedText{
            ball.x + ball.width + std::max(5.0f, 6.0f * deviceScale),
            ball.y + (ball.height - preferredText) * 0.5f,
            tipRect.x + tipRect.width -
                (ball.x + ball.width +
                    std::max(5.0f, 6.0f * deviceScale)),
            preferredText * 1.5f};
        ok = addFittedText(draw, atlas,
            result.tipAdjustmentLocked ? "LOCKED" : "READY",
            recommendedText, minimumText, minimumText, kSecondary) && ok;
    } else {
        const product_hud::HudRect statusText{
            ball.x + ball.width + std::max(5.0f, 6.0f * deviceScale),
            ball.y + (ball.height - preferredText) * 0.5f,
            tipRect.x + tipRect.width -
                (ball.x + ball.width +
                    std::max(5.0f, 6.0f * deviceScale)),
            preferredText * 1.5f};
        ok = addFittedText(draw, atlas,
            result.tipAdjustmentLocked ? "TIP LOCKED" : "TIP READY",
            statusText, minimumText, minimumText,
            result.tipAdjustmentLocked ? kLocked : kSecondary) && ok;
    }
    // Draw the current shot-control marker last so a coincident recommendation
    // ring never hides the authority value.
    addPrimitive(draw, UiPrimitiveKind::Icon, marker, tipRect,
        validTip ? kAccent : kLocked, "tip_position_marker");

    const std::string modeText = std::string("MODE ") +
        trainingControlModeName(result.mode);
    const std::string stateText =
        training.state == training::TrainingProductState::AttemptEvaluated ?
        "ATTEMPT DONE" :
        training.state != training::TrainingProductState::AttemptActive ?
        "NOT ACTIVE" : result.mode == TrainingControlMode::Motion ?
        "SHOT ACTIVE" : result.mode == TrainingControlMode::Aim ?
        "SHOT SETUP" : "VIEW ONLY";
    ok = addFittedText(draw, atlas, modeText,
        {modeRect.x, modeRect.y, modeRect.width, preferredText * 1.6f},
        preferredText, minimumText,
        result.mode == TrainingControlMode::Aim ? kAccent : kText) && ok;
    ok = addFittedText(draw, atlas, stateText,
        {modeRect.x, modeRect.y + preferredText * 1.9f, modeRect.width,
            modeRect.height - preferredText * 1.9f},
        preferredText, minimumText,
        result.mode == TrainingControlMode::Aim ? kSecondary : kLocked) && ok;

    if (!ok) {
        result.errorCode = draw.errorCode.empty() ?
            "training_control_hud_text_failed" : draw.errorCode;
        return result;
    }
    if (atlas.rasterizationCount() != draw.atlasRasterizationCount) {
        result.errorCode = "training_control_hud_font_rasterized_during_frame";
        return result;
    }
    draw.ok = true;
    result.ok = true;
    return result;
}

}  // namespace billiardgl::render::ui
