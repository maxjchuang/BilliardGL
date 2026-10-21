#pragma once

#include "product_settings.h"
#include "render/ui/hud_draw_list.h"

#include <cstdint>
#include <map>
#include <string>
#include <vector>

namespace billiardgl::render::ui {

enum class HudTokenCommandKind { Rect, Circle, Polyline, Polygon };
enum class HudPaletteMode { Standard, ColorVisionSafe };

struct HudTokenPoint {
    double x = 0.0;
    double y = 0.0;
};

struct HudTokenCommand {
    HudTokenCommandKind kind = HudTokenCommandKind::Rect;
    std::string fillRole = "none";
    std::string strokeRole = "none";
    double strokeWidth = 0.0;
    double x = 0.0;
    double y = 0.0;
    double width = 0.0;
    double height = 0.0;
    double centerX = 0.0;
    double centerY = 0.0;
    double radius = 0.0;
    std::vector<HudTokenPoint> points;
    bool roundCap = false;
    bool roundJoin = false;
};

struct HudTokenLabelSlot {
    bool present = false;
    HudTokenPoint center;
    std::string fontRole;
    std::string text;
};

struct HudIconToken {
    std::vector<HudTokenCommand> commands;
    HudTokenLabelSlot labelSlot;
    std::vector<std::string> screens;
    std::vector<std::string> states;
};

struct HudTokenPalette {
    std::map<std::string, UiColor> roles;
};

struct HudShippingTokenSet {
    bool ok = false;
    std::string errorCode;
    int schemaVersion = 0;
    std::string tokenSetId;
    std::string authorship;
    bool externalAssets = true;
    std::string productNamespace;
    std::string debugNamespace;
    std::map<std::string, HudIconToken> icons;
    std::map<std::string, std::string> aliases;
    std::map<std::string, std::vector<std::string>> stateIconUsage;
    std::map<std::string, std::vector<std::string>> screenIconUsage;
    std::map<HudPaletteMode, HudTokenPalette> palettes;

    // Resolves direct tokens and alias chains. Unknowns/cycles are errors.
    bool resolve(const std::string& sourceToken, std::string& resolvedToken,
        std::string& error) const;
};

struct HudShippingTokenRegistry {
    bool ok = false;
    std::string errorCode;
    std::vector<std::string> tokenSetIds;
    std::string productNamespace;
    std::string debugNamespace;
    std::vector<std::string> productNamespaces;
    std::vector<std::string> debugNamespaces;
    std::map<std::string, HudIconToken> icons;
    std::map<std::string, std::string> aliases;
    std::map<HudPaletteMode, HudTokenPalette> palettes;
    std::map<std::string, HudShippingTokenSet> tokenSets;

    bool resolve(const std::string& sourceToken, std::string& resolvedToken,
        std::string& error) const;
};

struct HudIconExpansionResult {
    bool ok = false;
    std::string errorCode;
    HudDrawList draw;
};

HudShippingTokenSet parseHudShippingTokenSet(const std::string& jsonBytes);
HudShippingTokenSet loadHudShippingTokenSet(const std::string& path);
HudShippingTokenSet parseShellShippingTokenSet(const std::string& jsonBytes);
HudShippingTokenSet loadShellShippingTokenSet(const std::string& path);
HudShippingTokenSet parseShippingTokenSet(const std::string& jsonBytes);
HudShippingTokenSet loadShippingTokenSet(const std::string& path);
HudShippingTokenRegistry mergeHudShippingTokenSets(
    const std::vector<HudShippingTokenSet>& tokenSets);

HudPaletteMode hudPaletteModeFor(
    settings::ColorVisionMode colorVisionMode) noexcept;

// Pure CPU transform: copies non-icon primitives and replaces every Icon with
// clipped triangles. It never mutates settings, gameplay, or physics authority.
HudIconExpansionResult expandShippingHudIcons(const HudDrawList& input,
    const HudShippingTokenSet& tokens, HudPaletteMode paletteMode);
HudIconExpansionResult expandShippingHudIcons(const HudDrawList& input,
    const HudShippingTokenRegistry& registry, HudPaletteMode paletteMode);
HudIconExpansionResult expandShippingHudIcons(const HudDrawList& input,
    const HudShippingTokenRegistry& registry, HudPaletteMode paletteMode,
    const std::string& tokenSetId);

// Stable little-endian, quantized evidence stream for deterministic tests and
// cache keys. This is geometry evidence, not a GPU capture.
std::vector<std::uint8_t> canonicalHudTriangleBytes(
    const std::vector<UiTriangle>& triangles);

}  // namespace billiardgl::render::ui
