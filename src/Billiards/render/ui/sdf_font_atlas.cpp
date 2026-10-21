#include "render/ui/sdf_font_atlas.h"

#include "deterministic_hash.h"

#include <ft2build.h>
#include FT_FREETYPE_H

#include <algorithm>
#include <cstring>
#include <cmath>
#include <filesystem>
#include <iomanip>
#include <sstream>

namespace billiardgl::render::ui {
namespace {

bool validConfig(const FontAtlasConfig& config)
{
    return !config.fontPath.empty() && config.faceIndex >= 0 &&
        config.nominalPixelSize > 0 && config.nominalPixelSize <= 256 &&
        std::isfinite(config.scaleBucket) && config.scaleBucket >= 0.5f &&
        config.scaleBucket <= 3.0f && config.atlasWidth > 0 &&
        config.atlasHeight > 0 && config.atlasWidth <= 8192 &&
        config.atlasHeight <= 8192 && config.glyphPadding >= 0 &&
        config.glyphPadding <= 16 && config.sdfSpread > 0 &&
        config.sdfSpread <= 64;
}

CoverageBitmap coverageFromFreeType(const FT_Bitmap& bitmap)
{
    CoverageBitmap result;
    result.width = static_cast<int>(bitmap.width);
    result.height = static_cast<int>(bitmap.rows);
    result.stride = result.width;
    result.pixels.resize(static_cast<std::size_t>(result.width * result.height));
    for (int y = 0; y < result.height; ++y) {
        const int sourceY = bitmap.pitch >= 0 ? y : result.height - 1 - y;
        const unsigned char* row = bitmap.buffer +
            static_cast<std::ptrdiff_t>(sourceY * std::abs(bitmap.pitch));
        for (int x = 0; x < result.width; ++x) {
            if (bitmap.pixel_mode == FT_PIXEL_MODE_GRAY)
                result.pixels[static_cast<std::size_t>(y * result.width + x)] = row[x];
            else if (bitmap.pixel_mode == FT_PIXEL_MODE_MONO)
                result.pixels[static_cast<std::size_t>(y * result.width + x)] =
                    (row[x / 8] & (0x80u >> (x % 8))) ? 255 : 0;
        }
    }
    return result;
}

bool isDigit(std::uint32_t codepoint)
{
    return codepoint >= static_cast<std::uint32_t>('0') &&
        codepoint <= static_cast<std::uint32_t>('9');
}

}  // namespace

std::vector<std::uint32_t> requiredHudCodepoints()
{
    std::vector<std::uint32_t> result;
    for (std::uint32_t codepoint = 32; codepoint <= 126; ++codepoint)
        result.push_back(codepoint);
    result.push_back(0x2014);  // em dash used by neutral fallbacks
    return result;
}

bool decodeUtf8(const std::string& text,
    std::vector<std::uint32_t>& codepoints, std::string& errorCode)
{
    codepoints.clear();
    for (std::size_t index = 0; index < text.size();) {
        const unsigned char first = static_cast<unsigned char>(text[index]);
        std::uint32_t codepoint = 0;
        std::size_t count = 0;
        if (first < 0x80) {
            codepoint = first;
            count = 1;
        } else if ((first & 0xe0u) == 0xc0u) {
            codepoint = first & 0x1fu;
            count = 2;
        } else if ((first & 0xf0u) == 0xe0u) {
            codepoint = first & 0x0fu;
            count = 3;
        } else if ((first & 0xf8u) == 0xf0u) {
            codepoint = first & 0x07u;
            count = 4;
        } else {
            errorCode = "font_utf8_invalid";
            return false;
        }
        if (index + count > text.size()) {
            errorCode = "font_utf8_invalid";
            return false;
        }
        for (std::size_t continuation = 1; continuation < count; ++continuation) {
            const unsigned char value = static_cast<unsigned char>(
                text[index + continuation]);
            if ((value & 0xc0u) != 0x80u) {
                errorCode = "font_utf8_invalid";
                return false;
            }
            codepoint = (codepoint << 6u) | (value & 0x3fu);
        }
        const bool overlong = (count == 2 && codepoint < 0x80) ||
            (count == 3 && codepoint < 0x800) ||
            (count == 4 && codepoint < 0x10000);
        if (overlong || codepoint > 0x10ffff ||
            (codepoint >= 0xd800 && codepoint <= 0xdfff)) {
            errorCode = "font_utf8_invalid";
            return false;
        }
        codepoints.push_back(codepoint);
        index += count;
    }
    errorCode.clear();
    return true;
}

FontAtlasResult SdfFontAtlas::initialize(const FontAtlasConfig& config,
    std::vector<std::uint32_t> codepoints)
{
    initialized_ = false;
    glyphs_.clear();
    pixels_.clear();
    rasterizationCount_ = 0;
    pixelSize_ = 0.0f;
    lineHeightPx_ = 0.0f;
    ascenderPx_ = 0.0f;
    tabularDigitAdvancePx_ = 0.0f;
    if (!validConfig(config)) return {false, "font_atlas_invalid_config"};
    if (!std::filesystem::is_regular_file(config.fontPath))
        return {false, "font_file_not_found"};
    std::sort(codepoints.begin(), codepoints.end());
    codepoints.erase(std::unique(codepoints.begin(), codepoints.end()),
        codepoints.end());
    if (codepoints.empty()) return {false, "font_codepoint_set_empty"};

    FT_Library library = nullptr;
    if (FT_Init_FreeType(&library) != 0)
        return {false, "freetype_init_failed"};
    FT_Face face = nullptr;
    if (FT_New_Face(library, config.fontPath.c_str(), config.faceIndex,
            &face) != 0) {
        FT_Done_FreeType(library);
        return {false, "font_face_load_failed"};
    }
    const int pixelSize = static_cast<int>(std::round(
        config.nominalPixelSize * config.scaleBucket));
    if (pixelSize <= 0 || FT_Set_Pixel_Sizes(face, 0,
            static_cast<FT_UInt>(pixelSize)) != 0) {
        FT_Done_Face(face);
        FT_Done_FreeType(library);
        return {false, "font_pixel_size_failed"};
    }

    config_ = config;
    pixelSize_ = static_cast<float>(pixelSize);
    lineHeightPx_ = static_cast<float>(face->size->metrics.height) / 64.0f;
    ascenderPx_ = static_cast<float>(face->size->metrics.ascender) / 64.0f;
    pixels_.assign(static_cast<std::size_t>(
        config.atlasWidth * config.atlasHeight), 0);
    int cursorX = config.glyphPadding;
    int cursorY = config.glyphPadding;
    int shelfHeight = 0;
    FontAtlasResult result{true, ""};
    for (std::uint32_t codepoint : codepoints) {
        if (FT_Get_Char_Index(face, codepoint) == 0) {
            result = {false, "font_missing_glyph_U+"};
            std::ostringstream suffix;
            suffix << std::uppercase << std::hex << codepoint;
            result.errorCode += suffix.str();
            break;
        }
        if (FT_Load_Char(face, codepoint,
                FT_LOAD_RENDER | FT_LOAD_TARGET_NORMAL) != 0) {
            result = {false, "font_glyph_rasterize_failed"};
            break;
        }
        ++rasterizationCount_;
        const FT_GlyphSlot slot = face->glyph;
        GlyphRecord glyph;
        glyph.codepoint = codepoint;
        glyph.advancePx = static_cast<float>(slot->advance.x) / 64.0f;
        glyph.bearingXPx = static_cast<float>(slot->bitmap_left);
        glyph.bearingYPx = static_cast<float>(slot->bitmap_top);
        glyph.bitmapWidth = static_cast<int>(slot->bitmap.width);
        glyph.bitmapHeight = static_cast<int>(slot->bitmap.rows);
        glyph.sdfSpread = config.sdfSpread;
        if (glyph.bitmapWidth > 0 && glyph.bitmapHeight > 0) {
            const SdfBitmap sdf = generateSignedDistanceField(
                coverageFromFreeType(slot->bitmap), config.sdfSpread);
            if (!sdf.ok) {
                result = {false, sdf.errorCode};
                break;
            }
            const int packedWidth = sdf.width + config.glyphPadding * 2;
            const int packedHeight = sdf.height + config.glyphPadding * 2;
            if (cursorX + packedWidth > config.atlasWidth) {
                cursorX = config.glyphPadding;
                cursorY += shelfHeight;
                shelfHeight = 0;
            }
            if (cursorY + packedHeight > config.atlasHeight) {
                result = {false, "font_atlas_overflow"};
                break;
            }
            glyph.atlasX = cursorX + config.glyphPadding;
            glyph.atlasY = cursorY + config.glyphPadding;
            glyph.atlasWidth = sdf.width;
            glyph.atlasHeight = sdf.height;
            for (int y = 0; y < sdf.height; ++y) {
                std::copy_n(sdf.pixels.begin() +
                    static_cast<std::ptrdiff_t>(y * sdf.width), sdf.width,
                    pixels_.begin() + static_cast<std::ptrdiff_t>(
                        (glyph.atlasY + y) * config.atlasWidth + glyph.atlasX));
            }
            cursorX += packedWidth;
            shelfHeight = std::max(shelfHeight, packedHeight);
        }
        glyphs_[codepoint] = glyph;
        if (isDigit(codepoint))
            tabularDigitAdvancePx_ = std::max(
                tabularDigitAdvancePx_, glyph.advancePx);
    }
    FT_Done_Face(face);
    FT_Done_FreeType(library);
    if (!result.ok) {
        glyphs_.clear();
        pixels_.clear();
        return result;
    }
    initialized_ = true;
    return {true, ""};
}

const GlyphRecord* SdfFontAtlas::findGlyph(std::uint32_t codepoint) const
{
    const auto found = glyphs_.find(codepoint);
    return found == glyphs_.end() ? nullptr : &found->second;
}

std::string SdfFontAtlas::glyphRecordSha256(std::uint32_t codepoint) const
{
    const GlyphRecord* glyph = findGlyph(codepoint);
    if (!initialized_ || glyph == nullptr || glyph->atlasWidth <= 0 ||
        glyph->atlasHeight <= 0 || glyph->atlasX < 0 || glyph->atlasY < 0 ||
        glyph->atlasX + glyph->atlasWidth > width() ||
        glyph->atlasY + glyph->atlasHeight > height())
        return {};
    std::string bytes("billiardgl_sdf_glyph_record_v1\0", 31);
    const auto append = [&](const auto& value) {
        const char* raw = reinterpret_cast<const char*>(&value);
        bytes.append(raw, sizeof(value));
    };
    append(glyph->codepoint);
    append(glyph->atlasX); append(glyph->atlasY);
    append(glyph->atlasWidth); append(glyph->atlasHeight);
    append(glyph->advancePx); append(glyph->bearingXPx);
    append(glyph->bearingYPx); append(glyph->bitmapWidth);
    append(glyph->bitmapHeight); append(glyph->sdfSpread);
    for (int row = 0; row < glyph->atlasHeight; ++row) {
        const std::size_t offset = static_cast<std::size_t>(
            glyph->atlasY + row) * static_cast<std::size_t>(width()) +
            static_cast<std::size_t>(glyph->atlasX);
        bytes.append(reinterpret_cast<const char*>(pixels_.data() + offset),
            static_cast<std::size_t>(glyph->atlasWidth));
    }
    return sha256Hex(bytes);
}

std::string SdfFontAtlas::cacheKey() const
{
    std::ostringstream output;
    output << config_.fontPath << '#' << config_.faceIndex << '#'
        << config_.nominalPixelSize << '#' << std::fixed <<
        std::setprecision(2) << config_.scaleBucket;
    return output.str();
}

TextMeasureResult SdfFontAtlas::measureUtf8(const std::string& text,
    float requestedPixelSize, bool tabularDigits) const
{
    if (!initialized_) return {false, "font_atlas_not_initialized", 0, 0, 0};
    if (!std::isfinite(requestedPixelSize) || requestedPixelSize <= 0.0f)
        return {false, "font_measure_invalid_size", 0, 0, 0};
    std::vector<std::uint32_t> codepoints;
    std::string error;
    if (!decodeUtf8(text, codepoints, error))
        return {false, error, 0, 0, 0};
    const float scale = requestedPixelSize / pixelSize_;
    float width = 0.0f;
    float maximumWidth = 0.0f;
    int lines = 1;
    for (std::uint32_t codepoint : codepoints) {
        if (codepoint == '\n') {
            maximumWidth = std::max(maximumWidth, width);
            width = 0.0f;
            ++lines;
            continue;
        }
        const GlyphRecord* glyph = findGlyph(codepoint);
        if (glyph == nullptr)
            return {false, "font_glyph_not_prebaked", 0, 0, 0};
        width += (tabularDigits && isDigit(codepoint) ?
            tabularDigitAdvancePx_ : glyph->advancePx) * scale;
    }
    maximumWidth = std::max(maximumWidth, width);
    return {true, "", maximumWidth, lines * lineHeightPx_ * scale, lines};
}

}  // namespace billiardgl::render::ui
