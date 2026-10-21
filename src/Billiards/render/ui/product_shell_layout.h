#pragma once

#include "product_shell_controller.h"
#include "product_hud.h"

#include <cstddef>
#include <array>
#include <string>
#include <vector>

namespace billiardgl::render::ui {

struct ShellViewport {
    int logicalWidth = 0;
    int logicalHeight = 0;
    int physicalWidth = 0;
    int physicalHeight = 0;
    float dpi = 96.0f;
    bool hasProtectedGameplayRect = false;
    product_hud::HudRect protectedGameplayRect;
};

enum class ShellInspectorDock { None, Left, Right };

struct ShellElementLayout {
    std::string id;
    product_hud::HudRect rect;
    bool focused = false;
    bool enabled = true;
};

struct ShellScreenLayout {
    bool ok = false;
    std::string errorCode;
    ProductScreen screen = ProductScreen::MainMenu;
    ShellViewport viewport;
    bool reflowed = false;
    float deviceScale = 1.0f;
    float effectiveUiScale = 1.0f;
    float baseScale = 1.0f;
    float minimumText = 14.0f;
    product_hud::HudRect safeRect;
    // Training and replay are gameplay-adjacent inspectors, not modal shells.
    // Their opaque UI is bounded to inspectorRect, leaving one continuous
    // gameplayCorridorRect for the table and trajectory comparison.
    bool boundedInspector = false;
    bool inspectorCollapsed = false;
    product_hud::HudRect inspectorToggleRect;
    product_hud::HudRect collapsedSummaryRect;
    std::size_t firstVisibleRow = 0;
    std::size_t maximumScrollRow = 0;
    std::string degradationReason;
    product_hud::HudRect inspectorRect;
    product_hud::HudRect gameplayCorridorRect;
    product_hud::HudRect protectedGameplayRect;
    ShellInspectorDock inspectorDock = ShellInspectorDock::None;
    float inspectorViewportCoverage = 1.0f;
    float inspectorProtectedIntersectionArea = 0.0f;
    product_hud::HudRect headerRect;
    // Dedicated line boxes are the CPU-side header typography contract.  A
    // line's glyphs may only clip against its own box, so a larger UI scale
    // cannot make the title and subtitle share a baseline region.
    product_hud::HudRect breadcrumbLineRect;
    product_hud::HudRect titleLineRect;
    product_hud::HudRect subtitleLineRect;
    product_hud::HudRect footerRect;
    product_hud::HudRect bodyRect;
    product_hud::HudRect errorRect;
    std::vector<ShellElementLayout> elements;
};

struct ShellHitTestResult {
    bool hit = false;
    std::size_t elementIndex = 0;
    std::string id;
    bool enabled = false;
};

struct ShellInspectorGeometryAudit {
    bool ok = false;
    std::string errorCode;
    float totalPanelArea = 0.0f;
    float protectedIntersectionArea = 0.0f;
};

struct ShellInspectorContentAudit {
    bool ok = false;
    std::string errorCode;
};

// V19 CP2 owns product-shell chrome only. The scene/table viewport is never an
// input, so camera or scene content cannot relocate this authoritative panel.
struct TrajectoryInsetShellExclusion {
    std::string id;
    product_hud::HudRect rect;
    bool active = true;
    // Disjoint occupied rectangles in the same semantic category. Never
    // replace these with a bounding box spanning unoccupied screen space.
    std::vector<product_hud::HudRect> additionalRects;
};

struct TrajectoryInsetShellRequest {
    std::string profileId;
    int physicalWidth = 0;
    int physicalHeight = 0;
    float uiScale = 1.0f;
    bool insetControlOn = true;
    bool worldOverlayControlOn = false;
    bool insetControlFocused = false;
    bool modalVisible = false;
    std::vector<TrajectoryInsetShellExclusion> exclusions;
};

struct TrajectoryInsetShellLayout {
    bool ok = false;
    std::string errorCode;
    std::string profileId;
    bool insetVisible = false;
    bool worldOverlayVisible = false;
    bool insetControlOn = true;
    bool worldOverlayControlOn = false;
    bool insetControlFocused = false;
    std::string suppressionReason;
    product_hud::HudRect authoritativePanelRect;
    product_hud::HudRect panelRect;
    product_hud::HudRect plotCutoutRect;
    product_hud::HudRect headerRect;
    product_hud::HudRect titleRect;
    std::array<product_hud::HudRect, 2> orientationLabelRects;
    std::array<product_hud::HudRect, 3> legendRects;
    product_hud::HudRect toggleRect;
    product_hud::HudRect toggleLabelRect;
    product_hud::HudRect focusRect;
    std::array<product_hud::HudRect, 4> borderRects;
    std::string title;
    std::string toggleLabel;
    std::array<std::string, 2> orientationLabels;
    std::array<std::string, 3> legendEntries;
    float legendTextPx = 0.0f;
    std::size_t trajectorySemanticMemberCount = 0;
    std::size_t trajectoryProbeCount = 0;
    std::size_t trajectoryDenominatorContribution = 0;
};

struct TrajectoryInsetShellAudit {
    bool ok = false;
    std::string errorCode;
};

TrajectoryInsetShellLayout buildTrajectoryInsetShellLayout(
    const TrajectoryInsetShellRequest& request);

TrajectoryInsetShellAudit auditTrajectoryInsetShellLayout(
    const TrajectoryInsetShellRequest& request,
    const TrajectoryInsetShellLayout& layout);

ShellInspectorGeometryAudit auditBoundedInspectorGeometry(
    const ShellScreenLayout& layout,
    const std::vector<product_hud::HudRect>& panelRects,
    float fullFrameScrimAlpha);

ShellInspectorContentAudit auditPhase4InspectorContent(
    const ProductShellSnapshot& model);

// This is the single geometry source used by both the CPU draw list and native
// product pointer navigation. Element rectangles are in physical pixels.
ShellScreenLayout buildShippingScreenLayout(
    const ProductShellSnapshot& model, const ShellViewport& viewport);

// Pointer coordinates are logical, top-left-origin application coordinates.
// Left/top edges are inclusive; right/bottom edges are exclusive.
ShellHitTestResult hitTestShippingScreenLayout(
    const ShellScreenLayout& layout, double logicalX, double logicalY);

}  // namespace billiardgl::render::ui
