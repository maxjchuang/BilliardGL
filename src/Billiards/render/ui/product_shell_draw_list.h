#pragma once

#include "render/ui/hud_draw_list.h"
#include "render/ui/product_shell_layout.h"

#include <string>
#include <utility>
#include <vector>

namespace billiardgl::render::ui {

struct ShellDrawList {
    HudDrawList draw;
    ProductScreen screen = ProductScreen::MainMenu;
    ShellViewport viewport;
    bool reflowed = false;
    float deviceScale = 1.0f;
    float effectiveUiScale = 1.0f;
    std::vector<ShellElementLayout> elements;
};

enum class TrajectoryInsetChromeKind {
    OpaquePanel,
    Border,
    Text,
    Toggle,
    Focus,
};

struct TrajectoryInsetChromePrimitive {
    TrajectoryInsetChromePrimitive() = default;
    TrajectoryInsetChromePrimitive(TrajectoryInsetChromeKind valueKind,
        product_hud::HudRect valueRect, std::string valueToken,
        float valueOpacity = 1.0f, bool excluded = true,
        std::string valueSourceToken = {},
        product_hud::HudRect valueSourceRect = {})
        : kind(valueKind), rect(valueRect), token(std::move(valueToken)),
          opacity(valueOpacity),
          excludedFromTrajectoryDenominator(excluded),
          sourceToken(std::move(valueSourceToken)),
          sourceRect(valueSourceRect) {}
    TrajectoryInsetChromeKind kind = TrajectoryInsetChromeKind::OpaquePanel;
    product_hud::HudRect rect;
    std::string token;
    float opacity = 1.0f;
    bool excludedFromTrajectoryDenominator = true;
    std::string sourceToken;
    product_hud::HudRect sourceRect;
};

struct TrajectoryInsetChromeCoverageClaim {
    TrajectoryInsetChromeKind kind = TrajectoryInsetChromeKind::OpaquePanel;
    std::string sourceToken;
    product_hud::HudRect sourceRect;
    std::vector<product_hud::HudRect> orderedExpectedVisibleRects;
    float expectedVisibleArea = 0.0f;
    float opacity = 1.0f;
    bool excludedFromTrajectoryDenominator = true;
};

struct TrajectoryInsetChromeDrawList {
    bool ok = false;
    std::string errorCode;
    bool visible = false;
    bool cutoutExplicit = false;
    product_hud::HudRect transparentCutoutRect;
    std::vector<TrajectoryInsetChromePrimitive> primitives;
    std::vector<TrajectoryInsetChromeCoverageClaim> coverageClaims;
    std::vector<std::string> orderedText;
    std::size_t trajectorySemanticMemberCount = 0;
    std::size_t trajectoryProbeCount = 0;
    std::size_t trajectoryDenominatorContribution = 0;
};

struct TrajectoryInsetChromeAudit {
    bool ok = false;
    std::string errorCode;
};

TrajectoryInsetChromeDrawList buildTrajectoryInsetChromeDrawList(
    const TrajectoryInsetShellLayout& layout,
    const std::vector<TrajectoryInsetChromePrimitive>& surrounding = {});

TrajectoryInsetChromeAudit auditTrajectoryInsetChromeDrawList(
    const TrajectoryInsetShellLayout& layout,
    const TrajectoryInsetChromeDrawList& draw,
    const std::vector<TrajectoryInsetChromePrimitive>& surrounding = {});

class ShippingScreenDrawListBuilder {
public:
    ShellDrawList build(const ProductShellSnapshot& model,
        const ShellViewport& viewport, const SdfFontAtlas& atlas) const;
};

}  // namespace billiardgl::render::ui
