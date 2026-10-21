#pragma once

#include "product_hud.h"
#include "render/ui/sdf_font_atlas.h"

#include <cstdint>
#include <string>
#include <vector>

namespace billiardgl::render::ui {

struct UiColor {
    float red = 1.0f;
    float green = 1.0f;
    float blue = 1.0f;
    float alpha = 1.0f;
};

enum class UiPrimitiveKind {
    Panel,
    SolidRect,
    Line,
    Icon,
    GlyphQuad
};

struct UiPrimitive {
    UiPrimitiveKind kind = UiPrimitiveKind::Panel;
    product_hud::HudRect rect;
    product_hud::HudRect clipRect;
    UiColor color;
    float u0 = 0.0f;
    float v0 = 0.0f;
    float u1 = 0.0f;
    float v1 = 0.0f;
    float thickness = 0.0f;
    std::uint32_t codepoint = 0;
    // Source text-run identity makes a fully clipped (therefore omitted)
    // glyph distinguishable from intentionally inkless whitespace.
    std::uint32_t textRunIndex = 0xffffffffu;
    std::uint32_t codepointOrdinal = 0xffffffffu;
    std::string token;
    // Unclipped placement retained for deterministic icon expansion.
    product_hud::HudRect sourceRect;
    // Exact unpadded FreeType coverage bounds for SDF glyph evidence.  The
    // ordinary sourceRect includes the configured SDF spread; these fields
    // retain the actual raster ink box before and after the same UI clip.
    product_hud::HudRect glyphInkSourceRect;
    product_hud::HudRect glyphInkRect;
};

struct UiPoint {
    float x = 0.0f;
    float y = 0.0f;
};

struct UiTriangle {
    UiPoint first;
    UiPoint second;
    UiPoint third;
    UiColor color;
    product_hud::HudRect clipRect;
    std::string sourceToken;
    std::string resolvedToken;
    bool worldAnnotation = false;
};

struct UiIconLabel {
    UiPoint center;
    product_hud::HudRect clipRect;
    UiColor color;
    float requestedPixelSize = 0.0f;
    std::string text;
    std::string fontRole;
    std::string sourceToken;
    std::string resolvedToken;
};

struct TextRunMetrics {
    std::string text;
    product_hud::HudRect clipRect;
    float requestedPixelSize = 0.0f;
    float measuredWidth = 0.0f;
    float originX = 0.0f;
    float baselineY = 0.0f;
    bool tabularDigits = true;
};

struct HudDrawList {
    bool ok = false;
    std::string errorCode;
    std::string atlasCacheKey;
    product_hud::HudRect safeRect;
    std::vector<UiPrimitive> primitives;
    std::vector<UiTriangle> triangles;
    std::vector<UiIconLabel> iconLabels;
    std::vector<TextRunMetrics> textRuns;
    int glyphQuadCount = 0;
    // Number of source Icon primitives successfully replaced by token
    // triangles. Capture evidence uses this retained count after the opaque
    // Icon primitives themselves have been removed.
    int expandedIconCount = 0;
    int atlasRasterizationCount = 0;
    bool containsDebug = false;
};

class HudDrawListBuilder {
public:
    HudDrawList buildProduct(const product_hud::ProductHudViewModel& model,
        const product_hud::HudLayoutResult& layout,
        const SdfFontAtlas& atlas) const;

    HudDrawList buildDebug(const product_hud::DebugOverlayViewModel& debug,
        const product_hud::HudLayoutResult& layout,
        const SdfFontAtlas& atlas, bool releaseCapture) const;
};

}  // namespace billiardgl::render::ui
