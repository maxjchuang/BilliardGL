#include "render/modern/modern_product_ui_runtime.h"

#include "deterministic_hash.h"

#include <cmath>
#include <algorithm>
#include <array>
#include <cstring>
#include <fstream>
#include <iterator>
#include <set>
#include <type_traits>

namespace billiardgl::render {
namespace {

constexpr const char* kHudManifestSha =
    "969d3b8bc7de46600d03f77a10e577fe560e2106a612e708739a96a73c27a6e1";
constexpr const char* kHudTokenSha =
    "abf73aa283279c28677b5c97a9b02b3130b0d0125048c3124c85a9c0e72df860";
constexpr const char* kShellTokenSha =
    "c144f1a25d3e7ced14e385bd6f76ca7e0f05b06597dcc8a4bacb4c570fc6bad3";

template <typename Value>
void appendBits(std::string& output, const Value& value)
{
    output.append(reinterpret_cast<const char*>(&value), sizeof(value));
}

void appendField(std::string& output, const std::string& value)
{
    const std::uint64_t size = value.size();
    appendBits(output, size);
    output += value;
}

std::string chromeDigest(const ui::TrajectoryInsetChromeDrawList& chrome)
{
    std::string canonical;
    appendBits(canonical, chrome.primitives.size());
    for (const auto& primitive : chrome.primitives) {
        appendBits(canonical, primitive.kind);
        appendBits(canonical, primitive.rect.x);
        appendBits(canonical, primitive.rect.y);
        appendBits(canonical, primitive.rect.width);
        appendBits(canonical, primitive.rect.height);
        appendField(canonical, primitive.token);
        appendBits(canonical, primitive.opacity);
        appendBits(canonical, primitive.excludedFromTrajectoryDenominator);
    }
    for (const auto& text : chrome.orderedText) appendField(canonical, text);
    return sha256Hex(canonical);
}

std::string batchDigest(const std::vector<ui::UiGpuBatch>& batches)
{
    std::string canonical;
    appendBits(canonical, batches.size());
    for (const auto& batch : batches) {
        appendBits(canonical, batch.kind);
        appendBits(canonical, batch.scissor.x);
        appendBits(canonical, batch.scissor.y);
        appendBits(canonical, batch.scissor.width);
        appendBits(canonical, batch.scissor.height);
        appendBits(canonical, batch.firstVertex);
        appendBits(canonical, batch.vertexCount);
    }
    return sha256Hex(canonical);
}

bool closeFloat(float first, float second)
{
    return std::isfinite(first) && std::isfinite(second) &&
        std::fabs(first - second) <= 1e-4f;
}

bool sameRect(const product_hud::HudRect& first,
    const product_hud::HudRect& second)
{
    return closeFloat(first.x, second.x) &&
        closeFloat(first.y, second.y) &&
        closeFloat(first.width, second.width) &&
        closeFloat(first.height, second.height);
}

product_hud::HudRect intersectRect(const product_hud::HudRect& first,
    const product_hud::HudRect& second)
{
    const float x0 = std::max(first.x, second.x);
    const float y0 = std::max(first.y, second.y);
    const float x1 = std::min(first.x + first.width,
        second.x + second.width);
    const float y1 = std::min(first.y + first.height,
        second.y + second.height);
    return {x0, y0, std::max(0.0f, x1 - x0),
        std::max(0.0f, y1 - y0)};
}

bool isDigit(std::uint32_t codepoint)
{
    return codepoint >= static_cast<std::uint32_t>('0') &&
        codepoint <= static_cast<std::uint32_t>('9');
}

bool validateGlyphAtlasBinding(const ui::SdfFontAtlas& atlas,
    const std::vector<ui::TextRunMetrics>& runs,
    const ui::UiPrimitive& primitive, std::string& recordSha256)
{
    if (primitive.kind != ui::UiPrimitiveKind::GlyphQuad ||
        primitive.textRunIndex >= runs.size())
        return false;
    std::vector<std::uint32_t> codepoints;
    std::string error;
    if (!ui::decodeUtf8(runs[primitive.textRunIndex].text,
            codepoints, error) || primitive.codepointOrdinal >=
            codepoints.size() || codepoints[primitive.codepointOrdinal] !=
            primitive.codepoint)
        return false;
    const ui::TextRunMetrics& run = runs[primitive.textRunIndex];
    if (!std::isfinite(run.requestedPixelSize) ||
        run.requestedPixelSize <= 0.0f || !std::isfinite(run.originX) ||
        !std::isfinite(run.baselineY) ||
        !sameRect(primitive.clipRect, run.clipRect))
        return false;
    const ui::TextMeasureResult measured = atlas.measureUtf8(run.text,
        run.requestedPixelSize, run.tabularDigits);
    if (!measured.ok || !closeFloat(run.measuredWidth, measured.width))
        return false;
    const float scale = run.requestedPixelSize / atlas.pixelSize();
    float penX = run.originX;
    for (std::uint32_t ordinal = 0;
            ordinal < primitive.codepointOrdinal; ++ordinal) {
        const ui::GlyphRecord* preceding = atlas.findGlyph(
            codepoints[ordinal]);
        if (preceding == nullptr) return false;
        penX += (run.tabularDigits && isDigit(codepoints[ordinal]) ?
            atlas.tabularDigitAdvancePx() : preceding->advancePx) * scale;
    }
    const ui::GlyphRecord* glyph = atlas.findGlyph(primitive.codepoint);
    if (glyph == nullptr || glyph->atlasWidth <= 0 ||
        glyph->atlasHeight <= 0 || primitive.sourceRect.width <= 0.0f ||
        primitive.sourceRect.height <= 0.0f)
        return false;
    const float advance = run.tabularDigits && isDigit(primitive.codepoint) ?
        atlas.tabularDigitAdvancePx() : glyph->advancePx;
    const float centerOffset = run.tabularDigits &&
            isDigit(primitive.codepoint) ?
        (advance - glyph->advancePx) * 0.5f * scale : 0.0f;
    const product_hud::HudRect expectedSource{
        penX + centerOffset + glyph->bearingXPx * scale -
            glyph->sdfSpread * scale,
        run.baselineY - glyph->bearingYPx * scale -
            glyph->sdfSpread * scale,
        glyph->atlasWidth * scale, glyph->atlasHeight * scale};
    const product_hud::HudRect expectedRect = intersectRect(expectedSource,
        run.clipRect);
    const product_hud::HudRect expectedInkSource{
        penX + centerOffset + glyph->bearingXPx * scale,
        run.baselineY - glyph->bearingYPx * scale,
        glyph->bitmapWidth * scale, glyph->bitmapHeight * scale};
    const product_hud::HudRect expectedInk = intersectRect(
        expectedInkSource, run.clipRect);
    if (expectedRect.width <= 0.0f || expectedRect.height <= 0.0f ||
        !sameRect(primitive.sourceRect, expectedSource) ||
        !sameRect(primitive.rect, expectedRect) ||
        !sameRect(primitive.glyphInkSourceRect, expectedInkSource) ||
        !sameRect(primitive.glyphInkRect, expectedInk))
        return false;
    const float rawU0 = static_cast<float>(glyph->atlasX) / atlas.width();
    const float rawV0 = static_cast<float>(glyph->atlasY) / atlas.height();
    const float rawU1 = static_cast<float>(glyph->atlasX +
        glyph->atlasWidth) / atlas.width();
    const float rawV1 = static_cast<float>(glyph->atlasY +
        glyph->atlasHeight) / atlas.height();
    const float x0 = (primitive.rect.x - primitive.sourceRect.x) /
        primitive.sourceRect.width;
    const float y0 = (primitive.rect.y - primitive.sourceRect.y) /
        primitive.sourceRect.height;
    const float x1 = (primitive.rect.x + primitive.rect.width -
        primitive.sourceRect.x) / primitive.sourceRect.width;
    const float y1 = (primitive.rect.y + primitive.rect.height -
        primitive.sourceRect.y) / primitive.sourceRect.height;
    if (!closeFloat(primitive.u0, rawU0 + (rawU1 - rawU0) * x0) ||
        !closeFloat(primitive.v0, rawV0 + (rawV1 - rawV0) * y0) ||
        !closeFloat(primitive.u1, rawU0 + (rawU1 - rawU0) * x1) ||
        !closeFloat(primitive.v1, rawV0 + (rawV1 - rawV0) * y1))
        return false;
    recordSha256 = atlas.glyphRecordSha256(primitive.codepoint);
    return recordSha256.size() == 64;
}

struct Point { double x = 0.0, y = 0.0; };
double edge(const Point& a, const Point& b, const Point& p)
{
    return (b.x - a.x) * (p.y - a.y) -
        (b.y - a.y) * (p.x - a.x);
}
bool topLeft(const Point& a, const Point& b)
{
    const double dx = b.x - a.x, dy = b.y - a.y;
    return dy < 0.0 || (dy == 0.0 && dx > 0.0);
}
bool covers(Point a, Point b, Point c, const Point& p)
{
    if (edge(a, b, c) < 0.0) std::swap(b, c);
    const std::array<double, 3> values{{edge(a, b, p), edge(b, c, p),
        edge(c, a, p)}};
    const std::array<bool, 3> owned{{topLeft(a, b), topLeft(b, c),
        topLeft(c, a)}};
    for (std::size_t i = 0; i < values.size(); ++i)
        if (values[i] < -1e-9 ||
            (std::abs(values[i]) <= 1e-9 && !owned[i])) return false;
    return true;
}

std::vector<TrajectoryInsetChromePhysicalSample> rasterChrome(
    const std::vector<ui::UiGpuVertex>& vertices,
    const std::vector<ui::UiGpuBatch>& batches, int width, int height,
    int samples)
{
    const std::array<std::array<double, 2>, 4> sample4{{
        {{0.125, 0.625}}, {{0.375, 0.125}},
        {{0.625, 0.875}}, {{0.875, 0.375}}}};
    const std::array<std::array<double, 2>, 2> sample2{{
        {{0.25, 0.75}}, {{0.75, 0.25}}}};
    const auto samplePosition = [&](int index) {
        if (samples == 1) return std::array<double, 2>{{0.5, 0.5}};
        if (samples == 2) return sample2[static_cast<std::size_t>(index)];
        return sample4[static_cast<std::size_t>(index)];
    };
    const auto physical = [&](const ui::UiGpuVertex& v) {
        return Point{(v.ndcX + 1.0) * 0.5 * width,
            (1.0 - v.ndcY) * 0.5 * height};
    };
    std::set<std::string> unique;
    for (const auto& batch : batches) {
        if (batch.firstVertex + batch.vertexCount > vertices.size() ||
            batch.vertexCount % 3 != 0) return {};
        for (std::size_t o = 0; o < batch.vertexCount; o += 3) {
            const Point a = physical(vertices[batch.firstVertex + o]);
            const Point b = physical(vertices[batch.firstVertex + o + 1]);
            const Point c = physical(vertices[batch.firstVertex + o + 2]);
            const auto& clip = vertices[batch.firstVertex + o];
            const int minX = std::max(0, static_cast<int>(std::floor(
                std::min({a.x, b.x, c.x}))));
            const int maxX = std::min(width - 1, static_cast<int>(std::ceil(
                std::max({a.x, b.x, c.x}))));
            const int minY = std::max(0, static_cast<int>(std::floor(
                std::min({a.y, b.y, c.y}))));
            const int maxY = std::min(height - 1, static_cast<int>(std::ceil(
                std::max({a.y, b.y, c.y}))));
            for (int y = minY; y <= maxY; ++y)
                for (int x = minX; x <= maxX; ++x)
                    for (int s = 0; s < samples; ++s) {
                        const auto sub = samplePosition(s);
                        const Point p{x + sub[0], y + sub[1]};
                        const double bottomY = height - p.y;
                        if (p.x < batch.scissor.x ||
                            p.x >= batch.scissor.x + batch.scissor.width ||
                            bottomY < batch.scissor.y ||
                            bottomY >= batch.scissor.y + batch.scissor.height ||
                            p.x < clip.clipLeft || p.x >= clip.clipRight ||
                            bottomY < clip.clipBottom ||
                            bottomY >= clip.clipTop ||
                            !covers(a, b, c, p)) continue;
                        std::string key;
                        appendBits(key, x); appendBits(key, y); appendBits(key, s);
                        unique.insert(key);
                    }
        }
    }
    std::vector<TrajectoryInsetChromePhysicalSample> result;
    for (const auto& key : unique) {
        TrajectoryInsetChromePhysicalSample sample;
        std::memcpy(&sample.x, key.data(), sizeof(sample.x));
        std::memcpy(&sample.y, key.data() + sizeof(sample.x), sizeof(sample.y));
        std::memcpy(&sample.sampleIndex,
            key.data() + sizeof(sample.x) + sizeof(sample.y),
            sizeof(sample.sampleIndex));
        result.push_back(sample);
    }
    return result;
}

std::string sampleDigest(
    const std::vector<TrajectoryInsetChromePhysicalSample>& samples)
{
    std::string canonical;
    for (const auto& sample : samples) {
        appendBits(canonical, sample.x);
        appendBits(canonical, sample.y);
        appendBits(canonical, sample.sampleIndex);
    }
    return sha256Hex(canonical);
}

std::array<double, 2> chromeSamplePosition(int samples, int index)
{
    static constexpr std::array<std::array<double, 2>, 4> sample4{{
        {{0.125, 0.625}}, {{0.375, 0.125}},
        {{0.625, 0.875}}, {{0.875, 0.375}}}};
    static constexpr std::array<std::array<double, 2>, 2> sample2{{
        {{0.25, 0.75}}, {{0.75, 0.25}}}};
    if (samples == 1) return {{0.5, 0.5}};
    if (samples == 2) return sample2[static_cast<std::size_t>(index)];
    return sample4[static_cast<std::size_t>(index)];
}

std::string join(const std::string& root, const std::string& relative)
{
    if (root.empty()) return relative;
    if (root.back() == '/') return root + relative;
    return root + "/" + relative;
}

bool readBytes(const std::string& path, std::string& bytes)
{
    std::ifstream input(path, std::ios::binary);
    if (!input) return false;
    bytes.assign(std::istreambuf_iterator<char>(input),
        std::istreambuf_iterator<char>());
    return static_cast<bool>(input) || input.eof();
}

ui::UiGpuResult verifyFile(const std::string& path,
    const std::string& expectedSha, const char* id, std::string& bytes,
    bool shaOptional = false)
{
    if (path.empty())
        return {false, std::string("product_ui_runtime_asset_path_empty:") + id};
    if (!readBytes(path, bytes))
        return {false, std::string("product_ui_runtime_asset_unreadable:") + id};
    if (expectedSha.empty()) {
        if (shaOptional) return {true, {}};
        return {false, std::string("product_ui_runtime_asset_sha_missing:") +
            id};
    }
    if (expectedSha.size() != 64 || sha256Hex(bytes) != expectedSha)
        return {false, std::string("product_ui_runtime_asset_sha_mismatch:") +
            id};
    return {true, {}};
}

ModernProductUiRuntimeResult hiddenResult(
    const ModernProductSessionSnapshot& session,
    const ui::ProductUiFrameEnvironment& environment)
{
    ModernProductUiRuntimeResult result;
    result.ok = true;
    result.screen = session.shell.screen;
    const std::string evidence = "billiardgl_product_ui_hidden_v1|" +
        std::to_string(static_cast<int>(session.shell.screen)) + "|" +
        (environment.productMode ? "1" : "0") +
        (environment.automation ? "1" : "0") +
        (environment.windowless ? "1" : "0") +
        (environment.rulesDecisionEvidenceMode ? "1" : "0");
    result.canonicalSha256 = sha256Hex(evidence);
    return result;
}

ui::UiGpuResult prefixed(const char* prefix, const std::string& error)
{
    return {false, std::string(prefix) +
        (error.empty() ? "unknown" : error)};
}

bool isShellScreen(ProductScreen screen)
{
    return screen != ProductScreen::Match;
}

void appendCacheString(std::string& output, const std::string& value)
{
    const std::uint64_t size = value.size();
    output.append(reinterpret_cast<const char*>(&size), sizeof(size));
    output.append(value);
}

template <typename Value>
void appendCacheValue(std::string& output, const Value& value)
{
    static_assert(std::is_trivially_copyable<Value>::value,
        "cache-key values must have stable object bytes");
    output.append(reinterpret_cast<const char*>(&value), sizeof(value));
}

std::string shellFrameCacheKey(const ModernProductSessionSnapshot& session)
{
    const ProductShellSnapshot& shell = session.shell;
    std::string key;
    key.reserve(256 + shell.items.size() * 128);
    appendCacheValue(key, shell.screen);
    appendCacheString(key, shell.screenId);
    appendCacheString(key, shell.title);
    appendCacheString(key, shell.subtitle);
    appendCacheString(key, shell.breadcrumb);
    appendCacheString(key, shell.errorText);
    appendCacheString(key, shell.footer);
    appendCacheValue(key, shell.uiScale);
    const std::uint64_t itemCount = shell.items.size();
    appendCacheValue(key, itemCount);
    for (const ShellItemViewModel& item : shell.items) {
        appendCacheString(key, item.id);
        appendCacheString(key, item.label);
        appendCacheString(key, item.value);
        appendCacheString(key, item.detail);
        appendCacheString(key, item.accessibleValue);
        appendCacheString(key, item.shapeToken);
        appendCacheValue(key, item.enabled);
        appendCacheValue(key, item.selected);
        appendCacheValue(key, item.focused);
    }
    appendCacheValue(key, session.effectiveSettings.uiScale);
    appendCacheValue(key, session.effectiveSettings.colorVisionMode);
    appendCacheValue(key, session.shellViewport.logicalWidth);
    appendCacheValue(key, session.shellViewport.logicalHeight);
    appendCacheValue(key, session.shellViewport.physicalWidth);
    appendCacheValue(key, session.shellViewport.physicalHeight);
    appendCacheValue(key, session.shellViewport.dpi);
    return key;
}

}  // namespace

GlyphAtlasBindingAudit auditSubmittedGlyphAtlasBindings(
    const ui::SdfFontAtlas& atlas,
    const std::vector<ui::TextRunMetrics>& runs,
    const std::vector<ui::UiPrimitive>& glyphs)
{
    GlyphAtlasBindingAudit result;
    if (!atlas.initialized() || !atlas.frozen()) {
        result.errorCode = "glyph_atlas_not_frozen";
        return result;
    }
    result.atlasCacheKey = atlas.cacheKey();
    result.glyphRecordSha256.reserve(glyphs.size());
    for (const ui::UiPrimitive& glyph : glyphs) {
        std::string recordSha256;
        if (!validateGlyphAtlasBinding(atlas, runs, glyph, recordSha256)) {
            result.errorCode = "glyph_atlas_binding_invalid";
            return result;
        }
        result.glyphRecordSha256.push_back(std::move(recordSha256));
    }
    result.ok = true;
    return result;
}

std::string trajectoryInsetChromeSemanticDigest(
    const ui::TrajectoryInsetChromeDrawList& chrome)
{
    return chromeDigest(chrome);
}

ModernProductUiRuntimeConfig ModernProductUiRuntimeConfig::trustedDefaults(
    const std::string& sourceRoot, const std::string& fontPath,
    long faceIndex)
{
    ModernProductUiRuntimeConfig result;
    const std::string hudRoot = join(sourceRoot, "assets/modern/hud");
    result.hudManifestPath = join(hudRoot,
        "focused_practice_hud_v1.hud.json");
    result.hudManifestSha256 = kHudManifestSha;
    result.hudTokenPath = join(hudRoot,
        "focused_practice_hud_shipping_tokens_v1.json");
    result.hudTokenSha256 = kHudTokenSha;
    result.shellTokenPath = join(hudRoot,
        "focused_practice_shell_shipping_tokens_v1.json");
    result.shellTokenSha256 = kShellTokenSha;
    result.font.fontPath = fontPath;
    result.font.faceIndex = faceIndex;
    result.font.nominalPixelSize = 32;
    result.font.scaleBucket = 1.0f;
    result.font.atlasWidth = 1024;
    result.font.atlasHeight = 1024;
    result.font.glyphPadding = 1;
    result.font.sdfSpread = 6;
    return result;
}

ModernProductUiRuntime::~ModernProductUiRuntime() { shutdown(); }

ui::UiGpuResult ModernProductUiRuntime::initialize(
    const ModernProductUiRuntimeConfig& config)
{
    shutdown();
    config_ = config;
    std::string manifestBytes;
    std::string hudBytes;
    std::string shellBytes;
    std::string fontBytes;
    ui::UiGpuResult verified = verifyFile(config.hudManifestPath,
        config.hudManifestSha256, "hud_manifest", manifestBytes);
    if (!verified.ok) return verified;
    verified = verifyFile(config.hudTokenPath, config.hudTokenSha256,
        "hud_tokens", hudBytes);
    if (!verified.ok) return verified;
    verified = verifyFile(config.shellTokenPath, config.shellTokenSha256,
        "shell_tokens", shellBytes);
    if (!verified.ok) return verified;
    verified = verifyFile(config.font.fontPath, config.fontSha256, "font",
        fontBytes, true);
    if (!verified.ok) return verified;

    manifest_ = product_hud::loadProductHudManifest(config.hudManifestPath);
    if (!manifest_.ok)
        return prefixed("product_ui_runtime_hud_manifest_invalid:",
            manifest_.errorCode);
    hudTokens_ = ui::parseHudShippingTokenSet(hudBytes);
    if (!hudTokens_.ok)
        return prefixed("product_ui_runtime_hud_tokens_invalid:",
            hudTokens_.errorCode);
    shellTokens_ = ui::parseShellShippingTokenSet(shellBytes);
    if (!shellTokens_.ok)
        return prefixed("product_ui_runtime_shell_tokens_invalid:",
            shellTokens_.errorCode);
    registry_ = ui::mergeHudShippingTokenSets({hudTokens_, shellTokens_});
    if (!registry_.ok)
        return prefixed("product_ui_runtime_registry_invalid:",
            registry_.errorCode);
    std::vector<std::uint32_t> codepoints = ui::requiredHudCodepoints();
    if (config.rulesDecisionUiImplementationCapability ||
            config.rulesDecisionIllegalBreakProductCapability) {
        std::set<std::uint32_t> unique(codepoints.begin(), codepoints.end());
        for (const std::string& text : ui::allRulesDecisionUiStrings()) {
            std::vector<std::uint32_t> decoded;
            std::string error;
            if (!ui::decodeUtf8(text, decoded, error))
                return {false, "product_ui_runtime_rules_copy_utf8_invalid"};
            if (!config.rulesDecisionUiImplementationCapability &&
                std::any_of(decoded.begin(), decoded.end(),
                    [](std::uint32_t codepoint) {
                        return codepoint >= 0x3400 && codepoint <= 0x9fff;
                    }))
                continue;
            unique.insert(decoded.begin(), decoded.end());
        }
        if (!config.rulesDecisionUiImplementationCapability) {
            // English product copy also composes metadata at runtime. Keep its
            // complete alphabet/punctuation inventory explicit instead of
            // relying on characters embedded only in a Chinese fixture row.
            const std::string dynamicEnglish =
                "Actor: Player 0123456789 · Revision: 42 · Clause: "
                "WPA-II-06-C Armed — Confirm • Preview X -36.0 cm, Z 0.0 cm";
            std::vector<std::uint32_t> decoded;
            std::string error;
            if (!ui::decodeUtf8(dynamicEnglish, decoded, error))
                return {false,
                    "product_ui_runtime_rules_dynamic_copy_utf8_invalid"};
            unique.insert(decoded.begin(), decoded.end());
        }
        codepoints.assign(unique.begin(), unique.end());
    }
    if (!config.supplementalUtf8Strings.empty()) {
        std::set<std::uint32_t> unique(codepoints.begin(), codepoints.end());
        for (const auto& text : config.supplementalUtf8Strings) {
            std::vector<std::uint32_t> decoded;
            std::string error;
            if (!ui::decodeUtf8(text, decoded, error))
                return {false, "product_ui_runtime_supplemental_utf8_invalid"};
            unique.insert(decoded.begin(), decoded.end());
        }
        codepoints.assign(unique.begin(), unique.end());
    }
    const ui::FontAtlasResult atlasResult = atlas_.initialize(
        config.font, std::move(codepoints));
    if (!atlasResult.ok)
        return prefixed("product_ui_runtime_atlas_invalid:",
            atlasResult.errorCode);

    if (config.insetChromeFont) {
        std::string fontBytes;
        const auto verified = verifyFile(config.insetChromeFont->fontPath, {},
            "inset_chrome_font", fontBytes, true);
        if (!verified.ok) return verified;
        std::set<std::uint32_t> unique;
        const std::vector<std::string> labels{
            "TRAJECTORY / 轨迹", "HEAD / 开球端", "FOOT / 底端",
            "ACTUAL / 实际 — paired continuous rails",
            "PREDICTED / 预测 — centerline dashes",
            "END / 终点 — endpoint marker", "Trajectory inset / 轨迹小窗",
            "Hide / 收起"};
        for (const auto& label : labels) {
            std::vector<std::uint32_t> decoded;
            std::string error;
            if (!ui::decodeUtf8(label, decoded, error))
                return {false, "inset_chrome_copy_utf8_invalid"};
            unique.insert(decoded.begin(), decoded.end());
        }
        // Atlas tabular-digit setup requires the complete numeric inventory.
        for (std::uint32_t c='0';c<='9';++c) unique.insert(c);
        const auto loaded = insetChromeAtlas_.initialize(*config.insetChromeFont,
            std::vector<std::uint32_t>(unique.begin(), unique.end()));
        if (!loaded.ok) return prefixed("inset_chrome_atlas_invalid:", loaded.errorCode);
    }

    coordinator_ = std::make_unique<ui::ProductUiFrameCoordinator>(manifest_,
        atlas_, registry_);
    gpuRenderer_ = std::make_unique<ui::ProductUiGpuRenderer>(dispatch_);
    assetsInitialized_ = true;
    const ui::UiGpuResult gpuResult = gpuRenderer_->initialize(atlas_);
    if (!gpuResult.ok)
        return prefixed("product_ui_runtime_gpu_initialize_failed:",
            gpuResult.errorCode);
    if (insetChromeAtlas_.initialized()) {
        insetChromeRenderer_ = std::make_unique<ui::ProductUiGpuRenderer>(dispatch_);
        const auto chromeResult = insetChromeRenderer_->initialize(insetChromeAtlas_);
        if (!chromeResult.ok) return prefixed("inset_chrome_gpu_initialize_failed:", chromeResult.errorCode);
    }
    gpuInitialized_ = true;
    return {true, {}};
}

ui::UiGpuResult ModernProductUiRuntime::reinitializeGpu()
{
    if (!assetsInitialized_ || gpuRenderer_ == nullptr)
        return {false, "product_ui_runtime_assets_not_initialized"};
    gpuRenderer_->shutdown();
    gpuInitialized_ = false;
    const ui::UiGpuResult result = gpuRenderer_->initialize(atlas_);
    if (!result.ok)
        return prefixed("product_ui_runtime_gpu_initialize_failed:",
            result.errorCode);
    if (insetChromeRenderer_) {
        insetChromeRenderer_->shutdown();
        const auto chromeResult = insetChromeRenderer_->initialize(insetChromeAtlas_);
        if (!chromeResult.ok) return prefixed("inset_chrome_gpu_initialize_failed:", chromeResult.errorCode);
    }
    gpuInitialized_ = true;
    return {true, {}};
}

ui::UiGpuResult ModernProductUiRuntime::resize(int physicalWidth,
    int physicalHeight, float dpi)
{
    if (physicalWidth <= 0 || physicalHeight <= 0 || !std::isfinite(dpi) ||
        dpi <= 0.0f)
        return {false, "product_ui_runtime_resize_invalid"};
    hasResize_ = true;
    physicalWidth_ = physicalWidth;
    physicalHeight_ = physicalHeight;
    dpi_ = dpi;
    return {true, {}};
}

const ui::ProductUiFrame& ModernProductUiRuntime::prepareFrame(
    const ModernProductSessionSnapshot& session,
    const PresentationSnapshot& presentation,
    const ui::ProductUiFrameEnvironment& environment)
{
    preparedFrame_ = coordinator_ ? coordinator_->build(
        ui::makeProductUiFrameRequest(session, presentation, environment)) :
        ui::ProductUiFrame{};
    preparedSession_ = &session;
    preparedPresentation_ = &presentation;
    preparedEnvironment_ = &environment;
    return *preparedFrame_;
}

ModernProductUiRuntimeResult ModernProductUiRuntime::render(
    const ModernProductSessionSnapshot& session,
    const PresentationSnapshot& presentation,
    const ui::ProductUiFrameEnvironment& environment)
{
    if (!environment.productMode || environment.automation ||
        environment.windowless)
        return hiddenResult(session, environment);
    ModernProductUiRuntimeResult result;
    result.screen = session.shell.screen;
    if (environment.rulesDecisionEvidenceMode &&
            !config_.rulesDecisionUiImplementationCapability)
        return {false, "product_ui_runtime_rules_decision_capability_disabled",
            ui::ProductUiOverlayKind::None, session.shell.screen};
    if (environment.rulesDecisionEvidenceMode &&
            !session.rulesDecisionUiImplementationEnabled)
        return {false, "product_ui_session_rules_decision_capability_disabled",
            ui::ProductUiOverlayKind::None, session.shell.screen};
    if (session.rulesDecisionUiProductExposed)
        return {false, "rules_decision_ui_product_exposure_forbidden",
            ui::ProductUiOverlayKind::None, session.shell.screen};
    if (!assetsInitialized_ || coordinator_ == nullptr)
        return {false, "product_ui_runtime_not_initialized",
            ui::ProductUiOverlayKind::None, session.shell.screen};
    if (hasResize_ && (session.shellViewport.physicalWidth != physicalWidth_ ||
            session.shellViewport.physicalHeight != physicalHeight_ ||
            std::fabs(session.shellViewport.dpi - dpi_) > 1e-5f))
        return {false, "product_ui_runtime_viewport_stale",
            ui::ProductUiOverlayKind::None, session.shell.screen};

    const ui::ProductUiFrame* frame = nullptr;
    ui::ProductUiFrame transientFrame;
    // Training and replay are live gameplay overlays: their geometry and
    // control readouts depend on the presentation camera, shot state, and
    // manual panel state even when the menu rows themselves are unchanged.
    // Cache static menus only; the atlas and GPU assets remain shared.
    if (preparedFrame_ && preparedSession_ == &session &&
        preparedPresentation_ == &presentation && preparedEnvironment_ == &environment) {
        transientFrame = std::move(*preparedFrame_);
        frame = &transientFrame;
    } else if (isShellScreen(session.shell.screen) &&
        session.shell.screen != ProductScreen::TrainingSession &&
        session.shell.screen != ProductScreen::ReplayPlayback) {
        const std::string key = shellFrameCacheKey(session);
        if (!hasCachedShellFrame_ || key != cachedShellFrameKey_) {
            cachedShellFrame_ = coordinator_->build(
                ui::makeProductUiFrameRequest(
                    session, presentation, environment));
            cachedShellFrameKey_ = key;
            hasCachedShellFrame_ = true;
        }
        frame = &cachedShellFrame_;
    } else {
        transientFrame = coordinator_->build(ui::makeProductUiFrameRequest(
            session, presentation, environment));
        frame = &transientFrame;
    }
    preparedFrame_.reset();
    preparedSession_ = nullptr;
    preparedPresentation_ = nullptr;
    preparedEnvironment_ = nullptr;
    result.overlay = frame->overlay;
    result.screen = frame->screen;
    result.canonicalSha256 = frame->evidenceSha256;
    result.rulesDecisionHitRects = frame->rulesDecisionHitRects;
    result.rulesDecisionExclusionRects =
        frame->rulesDecisionExclusionRects;
    if (config_.retainSubmittedTextEvidence) {
        result.diagnosticPrimitives = frame->draw.primitives;
        result.diagnosticTriangles = frame->draw.triangles;
        result.textRuns = frame->draw.textRuns;
        result.glyphQuads.reserve(frame->draw.glyphQuadCount);
        for (const ui::UiPrimitive& primitive : frame->draw.primitives) {
            if (primitive.kind == ui::UiPrimitiveKind::GlyphQuad)
                result.glyphQuads.push_back(primitive);
        }
        const GlyphAtlasBindingAudit audit = auditSubmittedGlyphAtlasBindings(
            atlas_, result.textRuns, result.glyphQuads);
        if (!audit.ok || result.glyphQuads.size() !=
                frame->draw.glyphQuadCount) {
            result.errorCode =
                "product_ui_runtime_glyph_atlas_binding_invalid";
            return result;
        }
        result.fontAtlasCacheKey = audit.atlasCacheKey;
        result.glyphAtlasRecordSha256 = audit.glyphRecordSha256;
        result.glyphAtlasBindingVerified = true;
    }
    if (!frame->ok) {
        result.errorCode = frame->errorCode.empty() ?
            "product_ui_runtime_frame_build_failed" : frame->errorCode;
        return result;
    }
    if (!gpuInitialized_ || gpuRenderer_ == nullptr) {
        result.errorCode = "product_ui_runtime_gpu_not_initialized";
        return result;
    }
    const ui::ProductUiGpuFrameResult submitted = gpuRenderer_->render(*frame,
        glyphEvidenceSuppressed_ ?
            ui::ProductUiGpuEvidenceMode::SuppressGlyphs :
            ui::ProductUiGpuEvidenceMode::Normal);
    result.gpuMetrics = submitted.metrics;
    result.glyphEvidenceSuppressed =
        submitted.metrics.glyphEvidenceSuppressed;
    if (!submitted.ok) {
        result.errorCode = submitted.errorCode.empty() ?
            "product_ui_runtime_gpu_submit_failed" : submitted.errorCode;
        return result;
    }
    result.ok = true;
    result.gpuSubmitted = submitted.metrics.drawCalls > 0;
    return result;
}

ui::UiGpuResult ModernProductUiRuntime::setGlyphEvidenceSuppressed(
    bool suppressed)
{
    if (suppressed && !config_.retainSubmittedTextEvidence)
        return {false, "product_ui_runtime_glyph_suppression_not_authorized"};
    glyphEvidenceSuppressed_ = suppressed;
    return {true, {}};
}

TrajectoryInsetProductUiChromeReceipt
TrajectoryInsetProductUiChromeReceipt::echoForTest(
    const ui::TrajectoryInsetVisibleCurrentFrame& current,
    const TrajectoryInsetProductUiChromeTarget& target)
{
    TrajectoryInsetProductUiChromeReceipt result;
    result.sessionSerial = target.sessionSerial;
    result.frameSerial = target.frameSerial;
    result.layoutSerial = target.layoutSerial;
    result.cp1Digests = target.cp1Digests;
    result.cp1GeometryDigest = target.cp1GeometryDigest;
    result.cp2LayoutDigest = target.cp2LayoutDigest;
    result.cp2ExclusionDigest = target.cp2ExclusionDigest;
    result.chromeSemanticDigest = target.chromeSemanticDigest;
    result.atlasCacheKey = target.atlasCacheKey;
    result.palette = target.palette;
    result.physicalWidth = target.physicalWidth;
    result.physicalHeight = target.physicalHeight;
    result.targetCurrentLdrFramebuffer = target.currentLdrFramebuffer;
    result.targetCurrentLdrColorTexture = target.currentLdrColorTexture;
    result.semanticPrimitiveCount = current.chrome.primitives.size();
    return result;
}

bool ModernProductUiRuntime::trajectoryInsetChromeAtlasContainsExactCodepoints(
    const std::vector<std::uint32_t>& codepoints) const
{
    const auto& atlas = insetChromeAtlas_.initialized() ? insetChromeAtlas_ : atlas_;
    return atlas.initialized() && !codepoints.empty() &&
        std::all_of(codepoints.begin(), codepoints.end(),
            [&](std::uint32_t codepoint) {
                return atlas.findGlyph(codepoint) != nullptr;
            });
}

TrajectoryInsetProductUiChromeReceipt
ModernProductUiRuntime::submitTrajectoryInsetChrome(
    const ui::TrajectoryInsetVisibleCurrentFrame& current,
    const TrajectoryInsetProductUiChromeTarget& target,
    const TrajectoryInsetProductUiChromeReceipt&)
{
    TrajectoryInsetProductUiChromeReceipt result;
    result.errorCode = "trajectory_inset_chrome_echo_receipt_forbidden";
    return result;
}

TrajectoryInsetProductUiChromeReceipt
ModernProductUiRuntime::submitTrajectoryInsetChrome(
    const ui::TrajectoryInsetVisibleCurrentFrame& current,
    const TrajectoryInsetProductUiChromeTarget& target)
{
    TrajectoryInsetProductUiChromeReceipt result;
    if (trajectoryInsetChromeContextPoisoned_) {
        result.adapterContextPoisoned = true;
        result.errorCode = "trajectory_inset_chrome_context_poisoned";
        return result;
    }
    const auto fail = [&](const char* error) {
        result.errorCode = error;
        return result;
    };
    if (!assetsInitialized_ || !gpuInitialized_ || gpuRenderer_ == nullptr)
        return fail("trajectory_inset_chrome_runtime_not_initialized");
    if (!current.ok || current.consumed || !current.request.present ||
        current.request.sessionSerial != target.sessionSerial ||
        current.request.frameSerial != target.frameSerial ||
        current.request.layoutSerial != target.layoutSerial ||
        current.request.cp1GenerationId != target.cp1GenerationId ||
        current.request.cp1Digests != target.cp1Digests ||
        current.request.cp1GeometryDigest != target.cp1GeometryDigest ||
        current.request.cp2LayoutDigest != target.cp2LayoutDigest ||
        current.request.cp2ExclusionDigest != target.cp2ExclusionDigest ||
        current.request.palette != target.palette ||
        current.request.samples != target.samples ||
        current.layout.insetControlFocused != target.insetControlFocused ||
        current.shellRequest.physicalWidth != target.physicalWidth ||
        current.shellRequest.physicalHeight != target.physicalHeight ||
        target.currentLdrFramebuffer == 0 || target.currentLdrColorTexture == 0)
        return fail("trajectory_inset_chrome_current_identity_invalid");
    if (chromeDigest(current.chrome) != target.chromeSemanticDigest ||
        trajectoryInsetChromeAtlasCacheKey() != target.atlasCacheKey ||
        !ui::auditTrajectoryInsetChromeDrawList(
            current.layout, current.chrome).ok)
        return fail("trajectory_inset_chrome_semantic_or_atlas_invalid");
    try {
        auto* chromeRenderer = insetChromeRenderer_ ? insetChromeRenderer_.get() : gpuRenderer_.get();
        auto submitted = chromeRenderer->renderTrajectoryInsetChrome(
            current.chrome, current.layout, target.physicalWidth,
            target.physicalHeight);
        if (!submitted.ok) {
            if (submitted.restoreFailed)
                trajectoryInsetChromeContextPoisoned_ = true;
            result.adapterContextPoisoned = trajectoryInsetChromeContextPoisoned_;
            result.errorCode = submitted.errorCode;
            return result;
        }
        if (submitted.target.framebuffer != target.currentLdrFramebuffer ||
            submitted.target.colorTexture != target.currentLdrColorTexture ||
            submitted.adjacentGlError != 0 || !submitted.stateRestored)
            return fail("trajectory_inset_chrome_actual_target_or_gl_invalid");
        result.sessionSerial = target.sessionSerial;
        result.frameSerial = target.frameSerial;
        result.layoutSerial = target.layoutSerial;
        result.cp1Digests = target.cp1Digests;
        result.cp1GeometryDigest = target.cp1GeometryDigest;
        result.cp2LayoutDigest = target.cp2LayoutDigest;
        result.cp2ExclusionDigest = target.cp2ExclusionDigest;
        result.chromeSemanticDigest = chromeDigest(current.chrome);
        result.atlasCacheKey = trajectoryInsetChromeAtlasCacheKey();
        result.palette = target.palette;
        result.physicalWidth = target.physicalWidth;
        result.physicalHeight = target.physicalHeight;
        result.targetCurrentLdrFramebuffer = submitted.target.framebuffer;
        result.targetCurrentLdrColorTexture = submitted.target.colorTexture;
        result.semanticPrimitiveCount = current.chrome.primitives.size();
        result.submittedSolidVertexCount = submitted.metrics.solidVertices;
        result.submittedSdfVertexCount = submitted.metrics.sdfVertices;
        result.submittedBatchCount = submitted.batches.size();
        result.submittedDrawCallCount = submitted.metrics.drawCalls;
        result.submittedBatches = submitted.batches;
        result.orderedPrimitiveDigest = chromeDigest(current.chrome);
        result.orderedBatchDigest = batchDigest(submitted.batches);
        for (const auto& row : submitted.rows) {
            TrajectoryInsetChromeSemanticRow converted;
            converted.token = row.token;
            converted.kind = row.kind;
            converted.rect = row.rect;
            converted.vertexRanges = row.vertexRanges;
            converted.batchRanges = row.batchRanges;
            converted.firstVertex = row.vertexRanges.front().first;
            converted.vertexCount = row.vertexRanges.front().count;
            converted.firstBatch = row.batchRanges.front().first;
            converted.batchCount = row.batchRanges.front().count;
            result.orderedSemanticRows.push_back(std::move(converted));
        }
        result.coveredPhysicalSamples = rasterChrome(submitted.vertices,
            submitted.batches, target.physicalWidth, target.physicalHeight,
            target.samples);
        if (result.coveredPhysicalSamples.empty())
            return fail("trajectory_inset_chrome_physical_coverage_empty");
        for (const auto& sample : result.coveredPhysicalSamples) {
            const auto& cutout = current.layout.plotCutoutRect;
            const auto sub = chromeSamplePosition(
                target.samples, sample.sampleIndex);
            const double physicalX = sample.x + sub[0];
            const double physicalY = sample.y + sub[1];
            if (physicalX >= cutout.x &&
                physicalX < cutout.x + cutout.width &&
                physicalY >= cutout.y &&
                physicalY < cutout.y + cutout.height)
                return fail("trajectory_inset_chrome_physical_cutout_covered");
        }
        result.coveredPhysicalSampleCount =
            result.coveredPhysicalSamples.size();
        result.cutoutZeroCoverageProofDigest =
            sampleDigest(result.coveredPhysicalSamples);
        result.glError = submitted.adjacentGlError;
        result.stateRestored = submitted.stateRestored;
        result.ok = true;
        return result;
    } catch (...) {
        trajectoryInsetChromeContextPoisoned_ = true;
        result.adapterContextPoisoned = true;
        result.errorCode = "trajectory_inset_chrome_restore_or_dispatch_throw";
        return result;
    }
}

void ModernProductUiRuntime::contextLost() noexcept
{
    preparedFrame_.reset();
    if (insetChromeRenderer_) insetChromeRenderer_->contextLost();
    if (gpuRenderer_ != nullptr) gpuRenderer_->contextLost();
    gpuInitialized_ = false;
    trajectoryInsetChromeContextPoisoned_ = true;
}

void ModernProductUiRuntime::shutdown() noexcept
{
    preparedFrame_.reset();
    if (gpuRenderer_ != nullptr) gpuRenderer_->shutdown();
    if (insetChromeRenderer_) insetChromeRenderer_->shutdown();
    insetChromeRenderer_.reset();
    gpuRenderer_.reset();
    coordinator_.reset();
    registry_ = {};
    shellTokens_ = {};
    hudTokens_ = {};
    manifest_ = {};
    atlas_ = {};
    insetChromeAtlas_ = {};
    config_ = {};
    assetsInitialized_ = false;
    gpuInitialized_ = false;
    hasResize_ = false;
    physicalWidth_ = 0;
    physicalHeight_ = 0;
    dpi_ = 96.0f;
    cachedShellFrameKey_.clear();
    glyphEvidenceSuppressed_ = false;
    cachedShellFrame_ = {};
    hasCachedShellFrame_ = false;
    trajectoryInsetChromeContextPoisoned_ = false;
}

}  // namespace billiardgl::render
