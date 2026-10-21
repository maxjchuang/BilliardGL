#pragma once

#include "render/ui/sdf_generator.h"

#include <cstdint>
#include <map>
#include <string>
#include <vector>

namespace billiardgl::render::ui {

struct FontAtlasConfig {
    std::string fontPath;
    long faceIndex = 0;
    int nominalPixelSize = 24;
    float scaleBucket = 1.0f;
    int atlasWidth = 1024;
    int atlasHeight = 1024;
    int glyphPadding = 1;
    int sdfSpread = 6;
};

struct GlyphRecord {
    std::uint32_t codepoint = 0;
    int atlasX = 0;
    int atlasY = 0;
    int atlasWidth = 0;
    int atlasHeight = 0;
    float advancePx = 0.0f;
    float bearingXPx = 0.0f;
    float bearingYPx = 0.0f;
    int bitmapWidth = 0;
    int bitmapHeight = 0;
    int sdfSpread = 0;
};

struct FontAtlasResult {
    bool ok = false;
    std::string errorCode;
};

struct TextMeasureResult {
    bool ok = false;
    std::string errorCode;
    float width = 0.0f;
    float height = 0.0f;
    int lineCount = 0;
};

std::vector<std::uint32_t> requiredHudCodepoints();
bool decodeUtf8(const std::string& text,
    std::vector<std::uint32_t>& codepoints, std::string& errorCode);

class SdfFontAtlas {
public:
    FontAtlasResult initialize(const FontAtlasConfig& config,
        std::vector<std::uint32_t> codepoints = requiredHudCodepoints());

    bool initialized() const { return initialized_; }
    bool frozen() const { return initialized_; }
    const FontAtlasConfig& config() const { return config_; }
    const GlyphRecord* findGlyph(std::uint32_t codepoint) const;
    // Canonical evidence binding for the exact frozen record and SDF texels
    // consumed by a submitted glyph quad. Empty means unavailable.
    std::string glyphRecordSha256(std::uint32_t codepoint) const;
    const std::vector<std::uint8_t>& pixels() const { return pixels_; }
    int width() const { return config_.atlasWidth; }
    int height() const { return config_.atlasHeight; }
    int rasterizationCount() const { return rasterizationCount_; }
    float pixelSize() const { return pixelSize_; }
    float lineHeightPx() const { return lineHeightPx_; }
    float ascenderPx() const { return ascenderPx_; }
    float tabularDigitAdvancePx() const { return tabularDigitAdvancePx_; }
    std::string cacheKey() const;

    TextMeasureResult measureUtf8(const std::string& text,
        float requestedPixelSize, bool tabularDigits = true) const;

private:
    FontAtlasConfig config_;
    bool initialized_ = false;
    int rasterizationCount_ = 0;
    float pixelSize_ = 0.0f;
    float lineHeightPx_ = 0.0f;
    float ascenderPx_ = 0.0f;
    float tabularDigitAdvancePx_ = 0.0f;
    std::map<std::uint32_t, GlyphRecord> glyphs_;
    std::vector<std::uint8_t> pixels_;
};

}  // namespace billiardgl::render::ui
