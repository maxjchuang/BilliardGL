#include "render/ui/product_ui_gpu_renderer.h"

#include <algorithm>
#include <cmath>
#include <limits>
#include <stdexcept>

namespace billiardgl::render::ui {
namespace {

constexpr const char* kVertexShader = R"GLSL(#version 410 core
layout(location = 0) in vec2 aPositionNdc;
layout(location = 1) in vec2 aUv;
layout(location = 2) in vec4 aColorSrgb;
layout(location = 3) in float aSdf;
layout(location = 4) in vec4 aClipPixels;
layout(location = 5) in float aInteractiveChrome;
out vec2 vUv;
out vec4 vColorLinear;
flat out float vSdf;
flat out vec4 vClipPixels;
flat out float vInteractiveChrome;
vec3 srgbToLinear(vec3 value) {
    vec3 low = value / 12.92;
    vec3 high = pow((value + vec3(0.055)) / 1.055, vec3(2.4));
    return mix(low, high, step(vec3(0.04045), value));
}
void main() {
    gl_Position = vec4(aPositionNdc, 0.0, 1.0);
    vUv = aUv;
    // A UI primitive has one constant token color.  Convert its six vertices
    // once instead of executing three pow() calls for every covered pixel.
    vColorLinear = vec4(srgbToLinear(aColorSrgb.rgb), aColorSrgb.a);
    vSdf = aSdf;
    vClipPixels = aClipPixels;
    vInteractiveChrome = aInteractiveChrome;
}
)GLSL";

constexpr const char* kFragmentShader = R"GLSL(#version 410 core
uniform sampler2D uSdfAtlas;
in vec2 vUv;
in vec4 vColorLinear;
flat in float vSdf;
flat in vec4 vClipPixels;
flat in float vInteractiveChrome;
layout(location = 0) out vec4 outColor;
// Ignored by normal product framebuffers. The capture-only ownership target
// attaches location 1 so later UI coverage is recorded by this exact draw,
// including the same shader clip/discard and SDF coverage.
layout(location = 1) out vec4 outFinalOwnershipUiCoverage;
layout(location = 2) out vec4 outInteractiveChromeCoverage;
vec3 srgbToLinear(vec3 value) {
    vec3 low = value / 12.92;
    vec3 high = pow((value + vec3(0.055)) / 1.055, vec3(2.4));
    return mix(low, high, step(vec3(0.04045), value));
}
void main() {
    if (gl_FragCoord.x < vClipPixels.x || gl_FragCoord.y < vClipPixels.y ||
        gl_FragCoord.x >= vClipPixels.z || gl_FragCoord.y >= vClipPixels.w)
        discard;
    float coverage = 1.0;
    if (vSdf > 0.5) {
        float distanceValue = texture(uSdfAtlas, vUv).r;
        float width = max(fwidth(distanceValue), 1.0 / 255.0);
        coverage = smoothstep(0.5 - width, 0.5 + width, distanceValue);
    }
    outColor = vec4(vColorLinear.rgb, vColorLinear.a * coverage);
    float finalCoverage = vColorLinear.a * coverage;
    outFinalOwnershipUiCoverage = vec4(finalCoverage);
    // World teaching markers still contribute to total coverage above. Alpha
    // zero here preserves any earlier chrome contribution under them.
    outInteractiveChromeCoverage = vec4(finalCoverage * vInteractiveChrome);
}
)GLSL";

bool finite(float value) { return std::isfinite(value); }

bool validColor(const UiColor& color)
{
    return finite(color.red) && finite(color.green) && finite(color.blue) &&
        finite(color.alpha) && color.red >= 0.0f && color.red <= 1.0f &&
        color.green >= 0.0f && color.green <= 1.0f &&
        color.blue >= 0.0f && color.blue <= 1.0f &&
        color.alpha >= 0.0f && color.alpha <= 1.0f;
}

bool validRect(const product_hud::HudRect& rect)
{
    return finite(rect.x) && finite(rect.y) && finite(rect.width) &&
        finite(rect.height) && rect.width >= 0.0f && rect.height >= 0.0f;
}

product_hud::HudRect intersection(const product_hud::HudRect& first,
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

UiGpuScissor scissorFor(const product_hud::HudRect& clip, int width,
    int height)
{
    const product_hud::HudRect viewport{0.0f, 0.0f,
        static_cast<float>(width), static_cast<float>(height)};
    const product_hud::HudRect visible = intersection(clip, viewport);
    if (empty(visible)) return {};
    const int left = std::max(0, static_cast<int>(std::floor(visible.x)));
    const int right = std::min(width,
        static_cast<int>(std::ceil(visible.x + visible.width)));
    const int top = std::max(0, static_cast<int>(std::floor(visible.y)));
    const int bottom = std::min(height,
        static_cast<int>(std::ceil(visible.y + visible.height)));
    return {left, height - bottom, std::max(0, right - left),
        std::max(0, bottom - top)};
}

UiGpuVertex vertex(float x, float y, float u, float v,
    const UiColor& color, bool sdf, int width, int height)
{
    return {2.0f * x / static_cast<float>(width) - 1.0f,
        1.0f - 2.0f * y / static_cast<float>(height), u, v, color,
        sdf ? 1.0f : 0.0f};
}

struct BuildState {
    int width = 0;
    int height = 0;
    std::vector<UiGpuVertex> vertices;
    std::vector<UiGpuBatch> batches;
    ProductUiGpuMetrics metrics;

    void append(UiGpuBatchKind kind, const product_hud::HudRect& clip,
        const std::vector<UiGpuVertex>& incoming)
    {
        const UiGpuScissor scissor = scissorFor(clip, width, height);
        if (scissor.width <= 0 || scissor.height <= 0 || incoming.empty()) {
            ++metrics.clippedItems;
            return;
        }
        const std::size_t first = vertices.size();
        for (UiGpuVertex value : incoming) {
            value.clipLeft = static_cast<float>(scissor.x);
            value.clipBottom = static_cast<float>(scissor.y);
            value.clipRight = static_cast<float>(scissor.x + scissor.width);
            value.clipTop = static_cast<float>(scissor.y + scissor.height);
            vertices.push_back(value);
        }
        // All shipping UI uses one shader, one immutable atlas, one ordered
        // vertex stream, and one straight-alpha blend mode. Per-vertex clip
        // bounds retain the original per-item scissor semantics, so the stream
        // can be submitted in one real glDrawArrays call without reordering.
        if (!batches.empty() &&
            batches.back().firstVertex + batches.back().vertexCount == first) {
            batches.back().vertexCount += incoming.size();
        } else {
            batches.push_back({kind, {0, 0, width, height}, first,
                incoming.size()});
        }
        if (kind == UiGpuBatchKind::Sdf)
            metrics.sdfVertices += incoming.size();
        else
            metrics.solidVertices += incoming.size();
    }
};

void appendQuad(BuildState& state, const product_hud::HudRect& rect,
    const product_hud::HudRect& clip, const UiColor& color, float u0,
    float v0, float u1, float v1, bool sdf)
{
    if (empty(rect)) {
        ++state.metrics.clippedItems;
        return;
    }
    const std::vector<UiGpuVertex> vertices{
        vertex(rect.x, rect.y, u0, v0, color, sdf, state.width, state.height),
        vertex(rect.x + rect.width, rect.y, u1, v0, color, sdf,
            state.width, state.height),
        vertex(rect.x + rect.width, rect.y + rect.height, u1, v1, color, sdf,
            state.width, state.height),
        vertex(rect.x, rect.y, u0, v0, color, sdf, state.width, state.height),
        vertex(rect.x + rect.width, rect.y + rect.height, u1, v1, color, sdf,
            state.width, state.height),
        vertex(rect.x, rect.y + rect.height, u0, v1, color, sdf,
            state.width, state.height)};
    state.append(sdf ? UiGpuBatchKind::Sdf : UiGpuBatchKind::Solid, clip,
        vertices);
}

void appendTriangle(BuildState& state, const UiTriangle& triangle)
{
    std::vector<UiGpuVertex> vertices{
        vertex(triangle.first.x, triangle.first.y, 0.0f, 0.0f,
            triangle.color, false, state.width, state.height),
        vertex(triangle.second.x, triangle.second.y, 0.0f, 0.0f,
            triangle.color, false, state.width, state.height),
        vertex(triangle.third.x, triangle.third.y, 0.0f, 0.0f,
            triangle.color, false, state.width, state.height)};
    for (auto& value : vertices)
        value.interactiveChrome = triangle.worldAnnotation ? 0.0f : 1.0f;
    state.append(UiGpuBatchKind::Solid, triangle.clipRect, vertices);
}

bool appendIconLabel(BuildState& state, const UiIconLabel& label,
    const SdfFontAtlas& atlas, std::string& error, bool requireCompleteInk = false)
{
    if (!finite(label.center.x) || !finite(label.center.y) ||
        !finite(label.requestedPixelSize) || label.requestedPixelSize <= 0.0f ||
        !validColor(label.color) || !validRect(label.clipRect)) {
        error = "product_ui_gpu_icon_label_invalid";
        return false;
    }
    const TextMeasureResult measured = atlas.measureUtf8(label.text,
        label.requestedPixelSize, true);
    if (!measured.ok) {
        error = measured.errorCode;
        return false;
    }
    std::vector<std::uint32_t> codepoints;
    if (!decodeUtf8(label.text, codepoints, error)) return false;
    const float scale = label.requestedPixelSize / atlas.pixelSize();
    float pen = label.center.x - measured.width * 0.5f;
    const float baseline = label.center.y +
        (atlas.ascenderPx() - atlas.lineHeightPx() * 0.5f) * scale;
    for (const std::uint32_t codepoint : codepoints) {
        const GlyphRecord* glyph = atlas.findGlyph(codepoint);
        if (glyph == nullptr) {
            error = "font_glyph_not_prebaked";
            return false;
        }
        const bool digit = codepoint >= '0' && codepoint <= '9';
        const float advance = digit ? atlas.tabularDigitAdvancePx() :
            glyph->advancePx;
        const float centerOffset = digit ?
            (advance - glyph->advancePx) * 0.5f * scale : 0.0f;
        if (glyph->atlasWidth > 0 && glyph->atlasHeight > 0) {
            const product_hud::HudRect ink{pen + centerOffset + glyph->bearingXPx * scale,
                baseline - glyph->bearingYPx * scale,
                glyph->bitmapWidth * scale, glyph->bitmapHeight * scale};
            if (requireCompleteInk && (ink.x < label.clipRect.x ||
                    ink.y < label.clipRect.y ||
                    ink.x + ink.width > label.clipRect.x + label.clipRect.width ||
                    ink.y + ink.height > label.clipRect.y + label.clipRect.height)) {
                error = "trajectory_inset_text_ink_exceeds_clip:" + label.text;
                return false;
            }
            const product_hud::HudRect raw{pen + centerOffset +
                    glyph->bearingXPx * scale - glyph->sdfSpread * scale,
                baseline - glyph->bearingYPx * scale -
                    glyph->sdfSpread * scale,
                glyph->atlasWidth * scale, glyph->atlasHeight * scale};
            const product_hud::HudRect clipped = intersection(raw,
                label.clipRect);
            if (!empty(clipped)) {
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
                appendQuad(state, clipped, label.clipRect, label.color,
                    rawU0 + (rawU1 - rawU0) * x0,
                    rawV0 + (rawV1 - rawV0) * y0,
                    rawU0 + (rawU1 - rawU0) * x1,
                    rawV0 + (rawV1 - rawV0) * y1, true);
            } else {
                ++state.metrics.clippedItems;
            }
        }
        pen += advance * scale;
    }
    return true;
}

ProductUiGpuFrameResult failure(const std::string& error,
    const ProductUiGpuMetrics& metrics = {})
{
    ProductUiGpuFrameResult result;
    result.errorCode = error;
    result.metrics = metrics;
    return result;
}

}  // namespace

ProductUiGpuRenderer::~ProductUiGpuRenderer() { shutdown(); }

UiGpuResult ProductUiGpuRenderer::initialize(const SdfFontAtlas& atlas)
{
    shutdown();
    if (!atlas.initialized() || !atlas.frozen() || atlas.width() <= 0 ||
        atlas.height() <= 0 || atlas.pixels().size() !=
            static_cast<std::size_t>(atlas.width() * atlas.height()))
        return {false, "product_ui_gpu_atlas_invalid"};
    UiGpuResourceCreateInfo info;
    info.vertexShader = kVertexShader;
    info.fragmentShader = kFragmentShader;
    info.atlasWidth = atlas.width();
    info.atlasHeight = atlas.height();
    info.atlasPixels = &atlas.pixels();
    UiGpuResourceHandles created;
    const UiGpuResult result = dispatch_.createResources(info, created);
    if (!result.ok) {
        dispatch_.destroyResources(created);
        return {false, result.errorCode.empty() ?
            "product_ui_gpu_resource_create_failed" : result.errorCode};
    }
    if (!created.complete()) {
        dispatch_.destroyResources(created);
        return {false, "product_ui_gpu_resource_incomplete"};
    }
    resources_ = created;
    atlas_ = &atlas;
    atlasCacheKey_ = atlas.cacheKey();
    return {true, {}};
}

ProductUiGpuFrameResult ProductUiGpuRenderer::render(
    const ProductUiFrame& frame, ProductUiGpuEvidenceMode evidenceMode)
{
    if (frame.overlay == ProductUiOverlayKind::None) {
        ProductUiGpuFrameResult result;
        result.ok = true;
        return result;
    }
    if (!frame.ok || !frame.draw.ok)
        return failure(frame.errorCode.empty() ?
            "product_ui_gpu_frame_invalid" : frame.errorCode);
    if (!initialized() || atlas_ == nullptr)
        return failure("product_ui_gpu_not_initialized");
    if (frame.physicalWidth <= 0 || frame.physicalHeight <= 0 ||
        !finite(frame.dpi) || frame.dpi <= 0.0f)
        return failure("product_ui_gpu_viewport_invalid");
    if (frame.draw.containsDebug)
        return failure("product_ui_gpu_debug_draw_forbidden");
    if (frame.draw.atlasCacheKey != atlasCacheKey_)
        return failure("product_ui_gpu_atlas_mismatch");

    BuildState state;
    state.width = frame.physicalWidth;
    state.height = frame.physicalHeight;
    state.metrics.inputPrimitives = frame.draw.primitives.size();
    state.metrics.inputTriangles = frame.draw.triangles.size();
    state.metrics.inputIconLabels = frame.draw.iconLabels.size();
    state.metrics.glyphEvidenceSuppressed =
        evidenceMode == ProductUiGpuEvidenceMode::SuppressGlyphs;

    // Preserve the backend-neutral stream order: ordinary primitives first,
    // expanded icon triangles second, and icon-label SDF requests last.
    for (const UiPrimitive& primitive : frame.draw.primitives) {
        if (primitive.kind == UiPrimitiveKind::Icon) {
            state.metrics.rejectedOpaqueIcons = 1;
            return failure("product_ui_gpu_opaque_icon_forbidden",
                state.metrics);
        }
        if (!validRect(primitive.rect) || !validRect(primitive.clipRect) ||
            !validColor(primitive.color))
            return failure("product_ui_gpu_primitive_invalid", state.metrics);
        if (primitive.kind != UiPrimitiveKind::Panel &&
            primitive.kind != UiPrimitiveKind::SolidRect &&
            primitive.kind != UiPrimitiveKind::Line &&
            primitive.kind != UiPrimitiveKind::GlyphQuad)
            return failure("product_ui_gpu_primitive_kind_unsupported",
                state.metrics);
        const bool sdf = primitive.kind == UiPrimitiveKind::GlyphQuad;
        if (sdf && (!finite(primitive.u0) || !finite(primitive.v0) ||
                !finite(primitive.u1) || !finite(primitive.v1)))
            return failure("product_ui_gpu_glyph_uv_invalid", state.metrics);
        if (sdf && evidenceMode ==
                ProductUiGpuEvidenceMode::SuppressGlyphs) {
            ++state.metrics.suppressedGlyphPrimitives;
            continue;
        }
        appendQuad(state, primitive.rect, primitive.clipRect,
            primitive.color, primitive.u0, primitive.v0, primitive.u1,
            primitive.v1, sdf);
    }
    for (const UiTriangle& triangle : frame.draw.triangles) {
        if (!finite(triangle.first.x) || !finite(triangle.first.y) ||
            !finite(triangle.second.x) || !finite(triangle.second.y) ||
            !finite(triangle.third.x) || !finite(triangle.third.y) ||
            !validRect(triangle.clipRect) || !validColor(triangle.color))
            return failure("product_ui_gpu_triangle_invalid", state.metrics);
        const float area = (triangle.second.x - triangle.first.x) *
                (triangle.third.y - triangle.first.y) -
            (triangle.second.y - triangle.first.y) *
                (triangle.third.x - triangle.first.x);
        if (!finite(area) || std::fabs(area) <= 1e-7f)
            return failure("product_ui_gpu_triangle_degenerate",
                state.metrics);
        appendTriangle(state, triangle);
    }
    for (const UiIconLabel& label : frame.draw.iconLabels) {
        // Icon labels are expanded only inside this GPU backend and therefore
        // do not carry the source-run/ordinal/atlas identity evidence attached
        // to ordinary GlyphQuad primitives.  Keep them identical in the
        // counterfactual frame: suppressing an untracked label would create a
        // legitimate pixel delta outside the auditable glyph union.
        std::string error;
        if (!appendIconLabel(state, label, *atlas_, error))
            return failure(error, state.metrics);
    }
    state.metrics.batches = state.batches.size();
    if (state.vertices.empty()) {
        ProductUiGpuFrameResult result;
        result.ok = true;
        result.metrics = state.metrics;
        return result;
    }

    const UiGpuBeginReceipt begin = dispatch_.beginFrameReceipt(
        resources_, state.width, state.height);
    if (!begin.setupOk()) {
        if (begin.stateCaptured() || begin.restoreRequired()) {
            const UiGpuResult restored = dispatch_.endFrame();
            if (!restored.ok)
                return failure(restored.errorCode.empty() ?
                    "product_ui_gpu_end_failed" : restored.errorCode,
                    state.metrics);
        }
        return failure(begin.errorCode().empty() ?
            "product_ui_gpu_begin_failed" : begin.errorCode(), state.metrics);
    }
    if (!begin.stateCaptured() || !begin.restoreRequired())
        return failure("product_ui_gpu_begin_receipt_invalid", state.metrics);
    UiGpuResult operation;
    operation = dispatch_.uploadVertices(resources_, state.vertices);
    if (!operation.ok) {
        dispatch_.endFrame();
        return failure(operation.errorCode.empty() ?
            "product_ui_gpu_upload_failed" : operation.errorCode,
            state.metrics);
    }
    for (const UiGpuBatch& batch : state.batches) {
        operation = dispatch_.setScissor(batch.scissor);
        if (operation.ok) operation = dispatch_.draw(resources_, batch);
        if (!operation.ok) {
            dispatch_.endFrame();
            return failure(operation.errorCode.empty() ?
                "product_ui_gpu_draw_failed" : operation.errorCode,
                state.metrics);
        }
        ++state.metrics.drawCalls;
    }
    operation = dispatch_.endFrame();
    if (!operation.ok)
        return failure(operation.errorCode.empty() ?
            "product_ui_gpu_end_failed" : operation.errorCode, state.metrics);
    ProductUiGpuFrameResult result;
    result.ok = true;
    result.metrics = state.metrics;
    result.batches = std::move(state.batches);
    result.vertices = std::move(state.vertices);
    return result;
}

TrajectoryInsetChromeGpuResult ProductUiGpuRenderer::renderTrajectoryInsetChrome(
    const TrajectoryInsetChromeDrawList& chrome,
    const TrajectoryInsetShellLayout& layout, int physicalWidth,
    int physicalHeight)
{
    TrajectoryInsetChromeGpuResult result;
    if (!initialized() || atlas_ == nullptr) {
        result.errorCode = "trajectory_inset_chrome_gpu_not_initialized";
        return result;
    }
    if (!chrome.ok || !layout.ok || physicalWidth <= 0 || physicalHeight <= 0 ||
        !auditTrajectoryInsetChromeDrawList(layout, chrome).ok) {
        result.errorCode = "trajectory_inset_chrome_gpu_input_invalid";
        return result;
    }
    result.target = dispatch_.bindAndQueryTrajectoryInsetChromeTarget();
    if (!result.target.valid || result.target.framebuffer == 0 ||
        result.target.colorTexture == 0) {
        result.errorCode = "trajectory_inset_chrome_gpu_target_invalid";
        return result;
    }

    BuildState aggregate;
    aggregate.width = physicalWidth;
    aggregate.height = physicalHeight;
    aggregate.metrics.inputPrimitives = chrome.primitives.size();
    std::size_t textIndex = 0;
    for (const auto& primitive : chrome.primitives) {
        BuildState local;
        local.width = physicalWidth;
        local.height = physicalHeight;
        std::string error;
        if (primitive.kind == TrajectoryInsetChromeKind::Text) {
            if (textIndex >= chrome.orderedText.size()) {
                result.errorCode = "trajectory_inset_chrome_text_order_invalid";
                return result;
            }
            UiIconLabel label;
            label.center = {primitive.rect.x + primitive.rect.width * 0.5f,
                primitive.rect.y + primitive.rect.height * 0.5f};
            label.clipRect = primitive.rect;
            label.color = {0.94f, 0.94f, 0.94f, primitive.opacity};
            const bool productText = layout.profileId.rfind("product_", 0) == 0;
            label.requestedPixelSize = productText ? layout.legendTextPx : std::max(8.0f,
                std::min(16.0f, primitive.rect.height * 0.62f));
            label.text = chrome.orderedText[textIndex++];
            label.fontRole = "caption_sdf";
            const auto measured = atlas_->measureUtf8(label.text, label.requestedPixelSize, true);
            if (productText && (!measured.ok || label.requestedPixelSize < 13.0f ||
                    measured.width + 8.0f * label.requestedPixelSize / 16.0f > primitive.rect.width)) {
                result.errorCode = "trajectory_inset_text_width_exceeds_clip:" + label.text;
                return result;
            }
            if (!appendIconLabel(local, label, *atlas_, error, productText)) {
                result.errorCode = error.empty() ?
                    "trajectory_inset_chrome_text_conversion_failed" : error;
                return result;
            }
        } else {
            UiColor color{0.10f, 0.12f, 0.16f, primitive.opacity};
            if (primitive.kind == TrajectoryInsetChromeKind::Border)
                color = {0.72f, 0.78f, 0.86f, primitive.opacity};
            else if (primitive.kind == TrajectoryInsetChromeKind::Toggle)
                color = {0.20f, 0.86f, 0.46f, primitive.opacity};
            else if (primitive.kind == TrajectoryInsetChromeKind::Focus)
                color = {1.0f, 0.82f, 0.18f, primitive.opacity};
            appendQuad(local, primitive.rect, primitive.rect, color,
                0.0f, 0.0f, 0.0f, 0.0f, false);
        }
        if (local.vertices.empty() || local.batches.empty()) {
            result.errorCode = "trajectory_inset_chrome_converted_empty";
            return result;
        }
        TrajectoryInsetChromeGpuRow row;
        row.token = primitive.token;
        row.kind = primitive.kind;
        row.rect = primitive.rect;
        row.vertexRanges.push_back(
            {aggregate.vertices.size(), local.vertices.size()});
        row.batchRanges.push_back(
            {aggregate.batches.size(), local.batches.size()});
        const std::size_t vertexBase = aggregate.vertices.size();
        aggregate.vertices.insert(aggregate.vertices.end(),
            local.vertices.begin(), local.vertices.end());
        for (auto batch : local.batches) {
            batch.firstVertex += vertexBase;
            aggregate.batches.push_back(batch);
        }
        aggregate.metrics.solidVertices += local.metrics.solidVertices;
        aggregate.metrics.sdfVertices += local.metrics.sdfVertices;
        result.rows.push_back(std::move(row));
    }
    if (textIndex != chrome.orderedText.size()) {
        result.errorCode = "trajectory_inset_chrome_text_count_invalid";
        return result;
    }
    aggregate.metrics.batches = aggregate.batches.size();
    bool restoreAttempted = false;
    auto restoreExactlyOnce = [&]() -> UiGpuResult {
        if (restoreAttempted)
            return {false, "trajectory_inset_chrome_restore_repeated"};
        restoreAttempted = true;
        return dispatch_.endFrame();
    };
    try {
        const UiGpuBeginReceipt begin = dispatch_.beginFrameReceipt(
            resources_, physicalWidth, physicalHeight);
        if (!begin.setupOk()) {
            UiGpuResult restored{true, {}};
            if (begin.stateCaptured() || begin.restoreRequired())
                restored = restoreExactlyOnce();
            result.stateRestored = restored.ok;
            result.restoreFailed = !restored.ok;
            result.errorCode = !restored.ok ?
                "trajectory_inset_chrome_restore_failed" : begin.errorCode();
            return result;
        }
        if (!begin.stateCaptured() || !begin.restoreRequired()) {
            result.errorCode = "trajectory_inset_chrome_begin_receipt_invalid";
            return result;
        }
        UiGpuResult operation;
        operation = dispatch_.uploadVertices(resources_, aggregate.vertices);
        if (!operation.ok) {
            const auto restored = restoreExactlyOnce();
            result.stateRestored = restored.ok;
            result.restoreFailed = !restored.ok;
            result.errorCode = !restored.ok ?
                "trajectory_inset_chrome_restore_failed" : operation.errorCode;
            return result;
        }
        for (const auto& batch : aggregate.batches) {
            operation = dispatch_.setScissor(batch.scissor);
            if (operation.ok) operation = dispatch_.draw(resources_, batch);
            if (!operation.ok) {
                const auto restored = restoreExactlyOnce();
                result.stateRestored = restored.ok;
                result.restoreFailed = !restored.ok;
                result.errorCode = !restored.ok ?
                    "trajectory_inset_chrome_restore_failed" : operation.errorCode;
                return result;
            }
            ++aggregate.metrics.drawCalls;
        }
        operation = restoreExactlyOnce();
        if (!operation.ok) {
            result.restoreFailed = true;
            result.errorCode = operation.errorCode;
            return result;
        }
    } catch (...) {
        if (!restoreAttempted) {
            try {
                const auto restored = restoreExactlyOnce();
                if (!restored.ok)
                    throw std::runtime_error(
                        "trajectory_inset_chrome_restore_failed");
            } catch (...) {
                throw std::runtime_error(
                    "trajectory_inset_chrome_restore_failed");
            }
        }
        throw;
    }
    result.adjacentGlError = dispatch_.adjacentTrajectoryInsetChromeGlError();
    if (result.adjacentGlError != 0) {
        result.errorCode = "trajectory_inset_chrome_adjacent_gl_error";
        return result;
    }
    result.ok = true;
    result.stateRestored = true;
    result.metrics = aggregate.metrics;
    result.vertices = std::move(aggregate.vertices);
    result.batches = std::move(aggregate.batches);
    return result;
}

void ProductUiGpuRenderer::shutdown() noexcept
{
    if (resources_.program != 0 || resources_.vertexArray != 0 ||
        resources_.vertexBuffer != 0 || resources_.atlasTexture != 0)
        dispatch_.destroyResources(resources_);
    resources_ = {};
    atlas_ = nullptr;
    atlasCacheKey_.clear();
}

void ProductUiGpuRenderer::contextLost() noexcept
{
    dispatch_.abandonResources(resources_);
    resources_ = {};
    atlas_ = nullptr;
    atlasCacheKey_.clear();
}

}  // namespace billiardgl::render::ui
