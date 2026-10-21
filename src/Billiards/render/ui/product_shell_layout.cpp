#include "render/ui/product_shell_layout.h"

#include <algorithm>
#include <array>
#include <cctype>
#include <cmath>
#include <functional>

namespace billiardgl::render::ui {
namespace {

int columnsFor(std::size_t count, float bodyHeight,
    float preferredHeight, float gap, float minimumHeight)
{
    for (int columns = 1; columns <= 3; ++columns) {
        const int rows = static_cast<int>((count + columns - 1) / columns);
        const float available = (bodyHeight - gap * (rows - 1)) / rows;
        if (available >= std::min(preferredHeight, minimumHeight)) return columns;
    }
    return 3;
}

float intersectionArea(const product_hud::HudRect& first,
    const product_hud::HudRect& second)
{
    const float width = std::max(0.0f, std::min(first.x + first.width,
        second.x + second.width) - std::max(first.x, second.x));
    const float height = std::max(0.0f, std::min(first.y + first.height,
        second.y + second.height) - std::max(first.y, second.y));
    return width * height;
}

product_hud::HudRect expanded(const product_hud::HudRect& rect, float amount)
{
    return {rect.x - amount, rect.y - amount,
        rect.width + amount * 2.0f, rect.height + amount * 2.0f};
}

struct TrajectoryInsetProfile {
    const char* id; int width; int height; float uiScale;
    float panelWidth; float panelHeight;
    float plotX; float plotY; float plotWidth; float plotHeight;
};

constexpr std::array<TrajectoryInsetProfile, 4> kTrajectoryInsetProfiles{{
    {"1920x1080_ui100", 1920, 1080, 1.0f,
        448, 288, 16, 48, 416, 208},
    {"1920x1080_ui135", 1920, 1080, 1.35f,
        604.8f, 388.8f, 21.6f, 64.8f, 561.6f, 280.8f},
    {"1280x720_ui100", 1280, 720, 1.0f,
        336, 216, 24, 48, 288, 144},
    {"1280x720_ui135", 1280, 720, 1.35f,
        453.6f, 291.6f, 21.6f, 64.8f, 410.4f, 205.2f},
}};

constexpr std::array<const char*, 5> kTrajectoryInsetExclusionIds{{
    "player status HUD", "shot power and tip controls",
    "rules decision panel or modal",
    "caption and accessibility live-region band",
    "safe-area edges and product footer"}};

bool finiteRect(const product_hud::HudRect& rect)
{
    return std::isfinite(rect.x) && std::isfinite(rect.y) &&
        std::isfinite(rect.width) && std::isfinite(rect.height) &&
        rect.width >= 0.0f && rect.height >= 0.0f;
}

bool intersectsExclusion(const product_hud::HudRect& rect,
    const TrajectoryInsetShellExclusion& exclusion)
{
    if (!exclusion.active) return false;
    if (intersectionArea(rect, exclusion.rect) > 0.0f) return true;
    return std::any_of(exclusion.additionalRects.begin(),
        exclusion.additionalRects.end(), [&](const auto& occupied) {
            return intersectionArea(rect, occupied) > 0.0f;
        });
}

bool same(float first, float second, float epsilon = 1e-4f)
{
    return std::fabs(first - second) <= epsilon;
}

bool sameRect(const product_hud::HudRect& first,
    const product_hud::HudRect& second)
{
    return same(first.x, second.x) && same(first.y, second.y) &&
        same(first.width, second.width) && same(first.height, second.height);
}

bool inside(const product_hud::HudRect& inner,
    const product_hud::HudRect& outer)
{
    return inner.x >= outer.x - 1e-4f && inner.y >= outer.y - 1e-4f &&
        inner.x + inner.width <= outer.x + outer.width + 1e-4f &&
        inner.y + inner.height <= outer.y + outer.height + 1e-4f;
}

const TrajectoryInsetProfile* insetProfile(
    const TrajectoryInsetShellRequest& request)
{
    const auto found = std::find_if(kTrajectoryInsetProfiles.begin(),
        kTrajectoryInsetProfiles.end(), [&](const auto& profile) {
            return (request.profileId == profile.id ||
                request.profileId == std::string("product_") + profile.id) &&
                request.physicalWidth == profile.width &&
                request.physicalHeight == profile.height &&
                same(request.uiScale, profile.uiScale);
        });
    return found == kTrajectoryInsetProfiles.end() ? nullptr : &*found;
}

product_hud::HudRect authoritativePanel(const TrajectoryInsetProfile& profile)
{
    const float right = 32.0f * profile.uiScale;
    const float bottom = 112.0f * profile.uiScale;
    return {profile.width - right - profile.panelWidth,
        profile.height - bottom - profile.panelHeight,
        profile.panelWidth, profile.panelHeight};
}

}  // namespace

TrajectoryInsetShellLayout buildTrajectoryInsetShellLayout(
    const TrajectoryInsetShellRequest& request)
{
    TrajectoryInsetShellLayout result;
    result.profileId = request.profileId;
    result.insetControlOn = request.insetControlOn;
    result.worldOverlayControlOn = request.worldOverlayControlOn;
    result.insetControlFocused = request.insetControlFocused;
    result.title = "TRAJECTORY / 轨迹";
    result.toggleLabel = "Trajectory inset / 轨迹小窗";
    result.orientationLabels = {"HEAD / 开球端", "FOOT / 底端"};
    result.legendEntries = {
        "ACTUAL / 实际 — paired continuous rails",
        "PREDICTED / 预测 — centerline dashes",
        "END / 终点 — endpoint marker"};
    if (request.worldOverlayControlOn) {
        result.errorCode = "trajectory_inset_world_overlay_forbidden_in_cp2";
        return result;
    }
    if (request.physicalWidth <= 0 || request.physicalHeight <= 0 ||
        !std::isfinite(request.uiScale) || request.uiScale <= 0.0f) {
        result.errorCode = "trajectory_inset_shell_request_invalid";
        return result;
    }
    if (request.exclusions.size() != kTrajectoryInsetExclusionIds.size()) {
        result.errorCode = "trajectory_inset_exclusion_inventory_invalid";
        return result;
    }
    for (const char* id : kTrajectoryInsetExclusionIds) {
        const auto count = std::count_if(request.exclusions.begin(),
            request.exclusions.end(), [&](const auto& value) {
                return value.id == id;
            });
        if (count != 1) {
            result.errorCode = "trajectory_inset_exclusion_inventory_invalid";
            return result;
        }
    }
    if (std::any_of(request.exclusions.begin(), request.exclusions.end(),
            [](const auto& value) { return !finiteRect(value.rect) ||
                std::any_of(value.additionalRects.begin(), value.additionalRects.end(),
                    [](const auto& rect) { return !finiteRect(rect); }); })) {
        result.errorCode = "trajectory_inset_exclusion_rect_invalid";
        return result;
    }
    const TrajectoryInsetProfile* profile = insetProfile(request);
    TrajectoryInsetProfile responsive{};
    if (profile == nullptr && request.uiScale >= 0.85f &&
            request.uiScale <= 1.35f) {
        // Preserve the four accepted reference layouts exactly; other actual
        // drawable sizes use the same typography and a 2:1 canonical plot.
        const float scale = request.uiScale;
        responsive.width = request.physicalWidth;
        responsive.height = request.physicalHeight;
        responsive.uiScale = scale;
        responsive.panelWidth = std::max(336.0f,
            (request.physicalWidth >= 1600 ? 448.0f : 336.0f) * scale);
        responsive.plotX = 16.0f * scale;
        responsive.plotY = 48.0f * scale;
        responsive.plotWidth = responsive.panelWidth - 32.0f * scale;
        responsive.plotHeight = responsive.plotWidth * 0.5f;
        responsive.panelHeight = std::max(216.0f,
            responsive.plotY + responsive.plotHeight + 32.0f * scale);
        const auto panel = authoritativePanel(responsive);
        if (panel.x >= 16.0f * scale && panel.y >= 16.0f * scale)
            profile = &responsive;
    }
    if (profile == nullptr) {
        result.ok = true;
        result.suppressionReason = "inset_layout_unavailable";
        return result;
    }
    const bool productChrome = request.profileId.rfind("product_", 0) == 0;
    TrajectoryInsetProfile productProfile = *profile;
    if (productChrome) {
        productProfile.plotY = 56.0f * request.uiScale;
        productProfile.panelHeight = productProfile.plotY +
            productProfile.plotHeight + 80.0f * request.uiScale;
        profile = &productProfile;
        result.toggleLabel = "Hide / 收起";
    }
    const float ui = profile->uiScale;
    result.authoritativePanelRect = authoritativePanel(*profile);
    if (profile == &responsive || request.profileId.rfind("product_", 0) == 0) {
        // Native product evidence: the responsive lower-right panel can
        // intersect the real bottom Training HUD. Preserve panel size and
        // plot ratio, choosing the nearest complete free position above UI.
        std::vector<float> candidateY{result.authoritativePanelRect.y};
        for (const auto& exclusion : request.exclusions) {
            if (!exclusion.active) continue;
            const auto add = [&](const auto& rect) {
                candidateY.push_back(rect.y - result.authoritativePanelRect.height -
                    8.0f * ui);
            };
            add(exclusion.rect);
            for (const auto& rect : exclusion.additionalRects) add(rect);
        }
        std::sort(candidateY.begin(), candidateY.end(), std::greater<float>());
        for (float y : candidateY) {
            if (y > result.authoritativePanelRect.y || y < 16.0f * ui) continue;
            auto candidate = result.authoritativePanelRect;
            candidate.y = y;
            if (std::none_of(request.exclusions.begin(), request.exclusions.end(),
                    [&](const auto& exclusion) {
                        return intersectsExclusion(candidate, exclusion);
                    })) {
                result.authoritativePanelRect = candidate;
                break;
            }
        }
    }
    result.panelRect = result.authoritativePanelRect;
    result.plotCutoutRect = {result.panelRect.x + profile->plotX,
        result.panelRect.y + profile->plotY,
        profile->plotWidth, profile->plotHeight};
    result.headerRect = {result.panelRect.x, result.panelRect.y,
        result.panelRect.width, profile->plotY};
    result.toggleLabelRect = {result.panelRect.x + 52.0f * ui,
        result.panelRect.y + 4.0f * ui,
        128.0f * ui, 20.0f * ui};
    result.titleRect = {result.panelRect.x + 184.0f * ui,
        result.panelRect.y + 4.0f * ui,
        result.panelRect.width - 188.0f * ui, 20.0f * ui};
    result.orientationLabelRects = {{
        {result.plotCutoutRect.x + 48.0f * ui,
            result.panelRect.y + 27.0f * ui, 110.0f * ui, 18.0f * ui},
        {result.plotCutoutRect.x + result.plotCutoutRect.width -
            110.0f * ui, result.panelRect.y + 27.0f * ui,
            110.0f * ui, 18.0f * ui}}};
    result.toggleRect = {result.panelRect.x + 4.0f * ui,
        result.panelRect.y + 2.0f * ui, 44.0f * ui, 44.0f * ui};
    result.focusRect = {result.toggleRect.x - 2.0f * ui,
        result.toggleRect.y - 2.0f * ui,
        result.toggleRect.width + 4.0f * ui,
        result.toggleRect.height + 4.0f * ui};
    const float legendY = result.plotCutoutRect.y +
        result.plotCutoutRect.height;
    const float legendHeight = result.panelRect.y + result.panelRect.height -
        legendY;
    const std::array<float, 4> legendEdges{{result.panelRect.x,
        result.panelRect.x + result.panelRect.width / 3.0f,
        result.panelRect.x + result.panelRect.width * (2.0f / 3.0f),
        result.panelRect.x + result.panelRect.width}};
    for (std::size_t index = 0; index < result.legendRects.size(); ++index)
        result.legendRects[index] = {legendEdges[index], legendY,
            legendEdges[index + 1] - legendEdges[index], legendHeight};
    if (productChrome) {
        result.toggleLabelRect = {result.panelRect.x + 52.0f * ui,
            result.panelRect.y + 4.0f * ui, 92.0f * ui, 24.0f * ui};
        result.titleRect = {result.panelRect.x + 148.0f * ui,
            result.panelRect.y + 4.0f * ui,
            result.panelRect.width - 156.0f * ui, 24.0f * ui};
        result.orientationLabelRects = {{
            {result.panelRect.x + 60.0f * ui, result.panelRect.y + 30.0f * ui,
                120.0f * ui, 24.0f * ui},
            {result.panelRect.x + result.panelRect.width - 112.0f * ui,
                result.panelRect.y + 30.0f * ui, 104.0f * ui, 24.0f * ui}}};
        for (std::size_t index = 0; index < result.legendRects.size(); ++index)
            result.legendRects[index] = {result.panelRect.x + 8.0f * ui,
                legendY + (2.0f + 26.0f * index) * ui,
                result.panelRect.width - 16.0f * ui, 24.0f * ui};
    }
    result.legendTextPx = 16.0f * ui;
    const float border = 2.0f * ui;
    result.borderRects = {{
        {result.panelRect.x, result.panelRect.y,
            result.panelRect.width, border},
        {result.panelRect.x, result.panelRect.y + result.panelRect.height -
            border, result.panelRect.width, border},
        {result.panelRect.x, result.panelRect.y, border,
            result.panelRect.height},
        {result.panelRect.x + result.panelRect.width - border,
            result.panelRect.y, border, result.panelRect.height}}};
    if (!request.insetControlOn) {
        result.ok = true;
        result.suppressionReason = "inset_disabled";
        return result;
    }
    const auto modal = std::find_if(request.exclusions.begin(),
        request.exclusions.end(), [](const auto& value) {
            return value.id == "rules decision panel or modal";
        });
    if (request.modalVisible || (modal != request.exclusions.end() &&
            intersectsExclusion(result.panelRect, *modal))) {
        result.ok = true;
        result.suppressionReason = "inset_suppressed_by_modal";
        return result;
    }
    for (const auto& exclusion : request.exclusions) {
        if (intersectsExclusion(result.panelRect, exclusion)) {
            result.ok = true;
            result.suppressionReason = "inset_layout_unavailable";
            return result;
        }
    }
    result.insetVisible = true;
    result.ok = true;
    return result;
}

TrajectoryInsetShellAudit auditTrajectoryInsetShellLayout(
    const TrajectoryInsetShellRequest& request,
    const TrajectoryInsetShellLayout& layout)
{
    TrajectoryInsetShellAudit audit;
    const TrajectoryInsetShellLayout expected =
        buildTrajectoryInsetShellLayout(request);
    if (!expected.ok || !layout.ok) {
        audit.errorCode = "trajectory_inset_layout_not_ok";
        return audit;
    }
    if (layout.profileId != expected.profileId ||
        layout.insetVisible != expected.insetVisible ||
        layout.worldOverlayVisible || layout.worldOverlayControlOn ||
        layout.insetControlFocused != expected.insetControlFocused ||
        layout.insetControlOn != expected.insetControlOn ||
        layout.suppressionReason != expected.suppressionReason ||
        !sameRect(layout.authoritativePanelRect,
            expected.authoritativePanelRect) ||
        !sameRect(layout.panelRect, expected.panelRect) ||
        !sameRect(layout.plotCutoutRect, expected.plotCutoutRect)) {
        audit.errorCode = "trajectory_inset_authoritative_layout_drift";
        return audit;
    }
    if (!layout.insetVisible) {
        audit.ok = true;
        return audit;
    }
    if (layout.panelRect.width < 336.0f ||
        layout.panelRect.height < 216.0f ||
        !same(layout.plotCutoutRect.width /
            layout.plotCutoutRect.height, 2.0f, 1e-5f) ||
        !inside(layout.plotCutoutRect, layout.panelRect) ||
        layout.title != expected.title ||
        layout.toggleLabel != expected.toggleLabel ||
        layout.orientationLabels != expected.orientationLabels ||
        layout.legendEntries != expected.legendEntries ||
        !same(layout.legendTextPx, expected.legendTextPx) ||
        layout.trajectorySemanticMemberCount != 0 ||
        layout.trajectoryProbeCount != 0 ||
        layout.trajectoryDenominatorContribution != 0) {
        audit.errorCode = "trajectory_inset_layout_contract_invalid";
        return audit;
    }
    const std::array<product_hud::HudRect, 8> rects{{layout.titleRect,
        layout.orientationLabelRects[0], layout.orientationLabelRects[1],
        layout.legendRects[0], layout.legendRects[1], layout.legendRects[2],
        layout.toggleLabelRect, layout.toggleRect}};
    const std::array<product_hud::HudRect, 8> expectedRects{{
        expected.titleRect, expected.orientationLabelRects[0],
        expected.orientationLabelRects[1], expected.legendRects[0],
        expected.legendRects[1], expected.legendRects[2],
        expected.toggleLabelRect, expected.toggleRect}};
    for (std::size_t index = 0; index < rects.size(); ++index) {
        if (!sameRect(rects[index], expectedRects[index]) ||
            rects[index].width <= 0.0f || rects[index].height <= 0.0f ||
            !inside(rects[index], layout.panelRect) ||
            intersectionArea(rects[index], layout.plotCutoutRect) > 0.0f) {
            audit.errorCode = "trajectory_inset_ui_rect_invalid";
            return audit;
        }
        for (std::size_t other = index + 1; other < rects.size(); ++other) {
            if (intersectionArea(rects[index], rects[other]) > 0.0f) {
                audit.errorCode = "trajectory_inset_ui_rect_collision";
                return audit;
            }
        }
    }
    if (!sameRect(layout.focusRect, expected.focusRect) ||
        !inside(layout.focusRect, layout.panelRect) ||
        intersectionArea(layout.focusRect, layout.plotCutoutRect) > 0.0f) {
        audit.errorCode = "trajectory_inset_focus_rect_invalid";
        return audit;
    }
    for (std::size_t index = 0; index < layout.borderRects.size(); ++index) {
        if (!sameRect(layout.borderRects[index], expected.borderRects[index]) ||
            !inside(layout.borderRects[index], layout.panelRect) ||
            intersectionArea(layout.borderRects[index],
                layout.plotCutoutRect) > 0.0f) {
            audit.errorCode = "trajectory_inset_border_rect_invalid";
            return audit;
        }
    }
    for (const auto& exclusion : request.exclusions) {
        if (intersectsExclusion(layout.panelRect, exclusion)) {
            audit.errorCode = "trajectory_inset_exclusion_overlap";
            return audit;
        }
    }
    audit.ok = true;
    return audit;
}

static ShellScreenLayout buildStrictShippingScreenLayout(
    const ProductShellSnapshot& model, const ShellViewport& viewport)
{
    ShellScreenLayout result;
    result.screen = model.screen;
    result.viewport = viewport;
    if (viewport.logicalWidth <= 0 || viewport.logicalHeight <= 0 ||
        viewport.physicalWidth <= 0 || viewport.physicalHeight <= 0 ||
        !std::isfinite(viewport.dpi) || viewport.dpi <= 0.0f ||
        !std::isfinite(model.uiScale) || model.uiScale < 0.85f ||
        model.uiScale > 1.35f) {
        result.errorCode = "shell_viewport_invalid";
        return result;
    }
    const float scaleX = static_cast<float>(viewport.physicalWidth) /
        viewport.logicalWidth;
    const float scaleY = static_cast<float>(viewport.physicalHeight) /
        viewport.logicalHeight;
    result.deviceScale = std::min(scaleX, scaleY);
    result.effectiveUiScale = result.deviceScale * model.uiScale;
    result.reflowed = model.uiScale > 1.15f || viewport.logicalWidth < 1200;
    result.baseScale = result.deviceScale;
    const float base = result.baseScale;
    float ui = result.effectiveUiScale;
    if (viewport.physicalWidth == 1920 && viewport.physicalHeight == 1080)
        result.safeRect = {64.0f, 36.0f, 1792.0f, 990.0f};
    else if (viewport.physicalWidth == 1280 && viewport.physicalHeight == 720)
        result.safeRect = {43.0f, 24.0f, 1194.0f, 660.0f};
    else {
        const float margin = 24.0f * base;
        result.safeRect = {margin, margin,
            viewport.physicalWidth - margin * 2.0f,
            viewport.physicalHeight - margin * 2.0f};
    }
    if (result.safeRect.width <= 0.0f || result.safeRect.height <= 0.0f) {
        result.errorCode = "shell_safe_area_empty";
        return result;
    }

    result.boundedInspector = model.screen == ProductScreen::TrainingSession ||
        model.screen == ProductScreen::ReplayPlayback;
    if (result.boundedInspector) {
        // Inspector accessibility scales to the larger of DPI or UI scale.
        // Multiplying both would produce 28.35px Retina text and make the
        // gate's complete required inventory physically impossible inside the
        // fixed 478x780 inspector; the correction profile requires >=21px.
        result.effectiveUiScale = std::max(result.deviceScale, model.uiScale);
        ui = result.effectiveUiScale;
    }
    if (result.boundedInspector) {
        const bool fullHd = viewport.physicalWidth == 1920 &&
            viewport.physicalHeight == 1080;
        const bool compact720 = viewport.physicalWidth == 1280 &&
            viewport.physicalHeight == 720;
        const float inspectorWidth = std::min(result.safeRect.width,
            fullHd ? 478.0f : compact720 ? 410.0f : 410.0f * base);
        const float inspectorHeight = std::min(result.safeRect.height,
            fullHd ? 780.0f : compact720 ? 620.0f : 620.0f * base);
        const float corridorGap = fullHd ? 32.0f : compact720 ? 24.0f :
            24.0f * base;
        const float inspectorY = result.safeRect.y +
            (result.safeRect.height - inspectorHeight) * 0.5f;
        const product_hud::HudRect left{result.safeRect.x, inspectorY,
            inspectorWidth, inspectorHeight};
        const product_hud::HudRect right{
            result.safeRect.x + result.safeRect.width - inspectorWidth,
            inspectorY, inspectorWidth, inspectorHeight};
        result.protectedGameplayRect = viewport.hasProtectedGameplayRect ?
            viewport.protectedGameplayRect : product_hud::HudRect{};
        const bool leftValid = !viewport.hasProtectedGameplayRect ||
            intersectionArea(expanded(left, corridorGap),
                result.protectedGameplayRect) <= 0.0f;
        const bool rightValid = !viewport.hasProtectedGameplayRect ||
            intersectionArea(expanded(right, corridorGap),
                result.protectedGameplayRect) <= 0.0f;
        if (!leftValid && !rightValid) {
            result.errorCode = "shell_protected_corridor_unresolvable";
            return result;
        }
        bool dockRight = false;
        if (!leftValid) dockRight = true;
        else if (rightValid && viewport.hasProtectedGameplayRect) {
            const float protectedCenter = result.protectedGameplayRect.x +
                result.protectedGameplayRect.width * 0.5f;
            const float leftDistance = std::fabs(
                left.x + left.width * 0.5f - protectedCenter);
            const float rightDistance = std::fabs(
                right.x + right.width * 0.5f - protectedCenter);
            dockRight = rightDistance > leftDistance;
        }
        result.inspectorRect = dockRight ? right : left;
        result.inspectorDock = dockRight ? ShellInspectorDock::Right :
            ShellInspectorDock::Left;
        if (dockRight) {
            result.gameplayCorridorRect = {result.safeRect.x,
                result.safeRect.y,
                result.inspectorRect.x - corridorGap - result.safeRect.x,
                result.safeRect.height};
        } else {
            result.gameplayCorridorRect = {
                result.inspectorRect.x + result.inspectorRect.width + corridorGap,
                result.safeRect.y,
                result.safeRect.x + result.safeRect.width -
                    (result.inspectorRect.x + result.inspectorRect.width +
                        corridorGap),
                result.safeRect.height};
        }
        result.inspectorProtectedIntersectionArea =
            viewport.hasProtectedGameplayRect ? intersectionArea(
                result.inspectorRect, result.protectedGameplayRect) : 0.0f;
        result.inspectorViewportCoverage =
            result.inspectorRect.width * result.inspectorRect.height /
            (viewport.physicalWidth * viewport.physicalHeight);
        const float minimumCorridorWidth = fullHd ? 1240.0f :
            compact720 ? 760.0f : viewport.physicalWidth * 0.50f;
        const float minimumCorridorHeight = fullHd ? 760.0f :
            compact720 ? 520.0f : viewport.physicalHeight * 0.50f;
        const float maximumCoverage = fullHd ? 0.18f :
            compact720 ? 0.29f : 0.40f;
        if (model.inspectorMode != 1 && (result.gameplayCorridorRect.width < minimumCorridorWidth ||
            result.gameplayCorridorRect.height < minimumCorridorHeight ||
            result.inspectorViewportCoverage > maximumCoverage ||
            result.inspectorProtectedIntersectionArea > 0.0f)) {
            result.errorCode = "shell_gameplay_corridor_too_narrow";
            return result;
        }
    } else {
        result.inspectorRect = result.safeRect;
        result.gameplayCorridorRect = {};
        result.inspectorViewportCoverage =
            result.safeRect.width * result.safeRect.height /
            (viewport.physicalWidth * viewport.physicalHeight);
    }

    const product_hud::HudRect& contentRect = result.inspectorRect;
    result.minimumText = 14.0f * ui;
    const bool compactHelp = model.screen == ProductScreen::Help;
    const bool wrappedTrainingHeader = result.boundedInspector &&
        model.manualInspectorPolicy &&
        model.screen == ProductScreen::TrainingSession;
    const float headerHeight = result.boundedInspector ?
        result.minimumText * (wrappedTrainingHeader ? 5.0f :
            model.manualInspectorPolicy ? 4.2f : 3.35f) +
            8.0f * base : compactHelp ?
        std::max(72.0f * ui,
            result.minimumText * 4.6f + 8.0f * base) :
        std::max(92.0f * ui,
            result.minimumText * 5.9f + 16.0f * base);
    const float footerHeight = std::max(
        38.0f * ui, result.minimumText * 2.0f);
    result.headerRect = {contentRect.x, contentRect.y,
        contentRect.width, headerHeight};
    const float headerInsetX = 18.0f * base;
    const float headerInsetY = 8.0f * base;
    const float headerTextWidth = result.headerRect.width -
        headerInsetX * 2.0f;
    const float lineGap = result.minimumText * 0.12f;
    result.breadcrumbLineRect = result.boundedInspector ?
        product_hud::HudRect{} : product_hud::HudRect{
            result.headerRect.x + headerInsetX,
            result.headerRect.y + headerInsetY, headerTextWidth,
            result.minimumText * (compactHelp ? 1.15f : 1.55f)};
    result.titleLineRect = {result.headerRect.x + headerInsetX,
        result.boundedInspector ? result.headerRect.y + 4.0f * base :
            result.breadcrumbLineRect.y + result.breadcrumbLineRect.height +
                lineGap,
        headerTextWidth, result.minimumText *
            (result.boundedInspector ?
                (model.manualInspectorPolicy ? 2.2f : 1.60f) :
                (compactHelp ? 1.90f : 2.50f))};
    result.subtitleLineRect = {result.headerRect.x + headerInsetX,
        result.titleLineRect.y + result.titleLineRect.height +
            (result.boundedInspector ? 0.0f : lineGap),
        headerTextWidth, result.minimumText *
            (wrappedTrainingHeader ? 3.10f :
                compactHelp ? 1.15f : 1.55f)};
    result.footerRect = {contentRect.x,
        contentRect.y + contentRect.height - footerHeight,
        contentRect.width, footerHeight};
    result.bodyRect = {contentRect.x,
        result.headerRect.y + result.headerRect.height + 10.0f * base,
        contentRect.width,
        result.footerRect.y -
            (result.headerRect.y + result.headerRect.height) - 20.0f * base};
    if (result.bodyRect.height <= result.minimumText * 2.4f) {
        result.errorCode = "shell_layout_too_small";
        return result;
    }
    if (!model.errorText.empty()) {
        const float errorHeight = std::max(
            42.0f * ui, result.minimumText * 2.1f);
        result.errorRect = {result.bodyRect.x, result.bodyRect.y,
            result.bodyRect.width, errorHeight};
        result.bodyRect.y += errorHeight + 10.0f * base;
        result.bodyRect.height -= errorHeight + 10.0f * base;
    }

    const std::size_t count = std::max<std::size_t>(1, model.items.size());
    const float gap = (result.boundedInspector ? 6.0f : 10.0f) * base;
    const float preferredItemHeight = std::max(
        (model.manualInspectorPolicy ? 180.0f : 68.0f) * ui,
        result.minimumText * (model.manualInspectorPolicy ? 10.0f : 3.8f));
    // A shipping row always reserves two complete text lines (label + value),
    // including descenders/SDF spread and the fixed vertical inset.  The old
    // 2.5-em threshold let eight Audio rows select one column at 720p/ui135;
    // their value baseline then landed below the row clip.  Reflow to more
    // columns instead of shrinking text or dropping the second line.
    // Catalog cards can contain a three-line drill title plus three-line
    // category / assist metadata.  Reserve six complete minimum-size lines;
    // when all
    // cards cannot fit, expose the existing wheel-scroll window rather than
    // clipping or shrinking their content.
    const float minimumItemHeight = result.minimumText *
        (model.screen == ProductScreen::TrainingCatalog ? 6.45f : 3.2f);
    const float contentItemHeight = std::max(
        preferredItemHeight, minimumItemHeight);
    const int columns = result.boundedInspector ? 1 :
        columnsFor(count, result.bodyRect.height,
            contentItemHeight, gap, minimumItemHeight);
    std::size_t visibleCount = count;
    if (result.boundedInspector && model.inspectorMode == 1) {
        const auto capacity = static_cast<std::size_t>(std::max(1.0f,
            std::floor((result.bodyRect.height + gap) / (contentItemHeight + gap))));
        visibleCount = std::min(count, capacity);
        result.maximumScrollRow = count - visibleCount;
        result.firstVisibleRow = std::min(model.inspectorScrollRow, result.maximumScrollRow);
    } else if (!result.boundedInspector) {
        const auto naturalRows = static_cast<std::size_t>(
            (count + static_cast<std::size_t>(columns) - 1) /
            static_cast<std::size_t>(columns));
        const float naturalHeight =
            (result.bodyRect.height - gap * (naturalRows - 1)) /
            static_cast<float>(naturalRows);
        if (naturalHeight < minimumItemHeight) {
            const auto rowsThatFit = static_cast<std::size_t>(std::max(1.0f,
                std::floor((result.bodyRect.height + gap) /
                    (minimumItemHeight + gap))));
            visibleCount = std::min(count,
                rowsThatFit * static_cast<std::size_t>(columns));
            result.maximumScrollRow = count - visibleCount;
            result.firstVisibleRow = std::min(
                model.inspectorScrollRow, result.maximumScrollRow);
        }
    }
    const int rows = static_cast<int>((visibleCount + columns - 1) / columns);
    const float itemWidth =
        (result.bodyRect.width - gap * (columns - 1)) / columns;
    const float itemHeight = std::min(contentItemHeight,
        (result.bodyRect.height - gap * (rows - 1)) / rows);
    if (itemWidth <= result.minimumText * 6.0f ||
        itemHeight < minimumItemHeight) {
        result.errorCode = "shell_layout_item_too_small";
        return result;
    }
    for (std::size_t index = 0; index < visibleCount; ++index) {
        const int column = static_cast<int>(index) % columns;
        const int row = static_cast<int>(index) / columns;
        const product_hud::HudRect rect{
            result.bodyRect.x + column * (itemWidth + gap),
            result.bodyRect.y + row * (itemHeight + gap),
            itemWidth, itemHeight};
        if (model.items.empty()) {
            result.elements.push_back(
                {"shell_empty", rect, false, false});
        } else {
            const ShellItemViewModel& item = model.items[index + result.firstVisibleRow];
            result.elements.push_back(
                {item.id, rect, item.focused, item.enabled});
        }
    }
    result.ok = true;
    return result;
}

ShellScreenLayout buildShippingScreenLayout(
    const ProductShellSnapshot& model, const ShellViewport& viewport)
{
    auto effectiveViewport = viewport;
    auto effectiveModel = model;
    if (model.inspectorMode == 1 || model.manualInspectorPolicy) {
        effectiveViewport.hasProtectedGameplayRect = false;
        effectiveModel.inspectorMode = 1;
    }
    auto result = buildStrictShippingScreenLayout(effectiveModel, effectiveViewport);
    if (!result.boundedInspector) return result;
    result.inspectorToggleRect = {result.safeRect.x + result.safeRect.width - 174.0f * result.baseScale,
        result.safeRect.y, 174.0f * result.baseScale, 34.0f * result.baseScale};
    if (result.ok && model.inspectorMode != 2) return result;
    if (result.ok && model.inspectorMode == 2) {
        result.ok = false;
        result.errorCode = "shell_inspector_user_collapsed";
    }
    if (result.errorCode != "shell_protected_corridor_unresolvable" &&
        result.errorCode != "shell_gameplay_corridor_too_narrow" &&
        result.errorCode != "shell_layout_item_too_small" &&
        result.errorCode != "shell_inspector_user_collapsed") return result;
    // Recoverable gameplay layout pressure must not terminate the application.
    // Remove the inspector controls; only explicitly drawn retry or playback
    // controls may remain. There must be no invisible click targets.
    result.degradationReason = result.errorCode;
    result.errorCode.clear();
    result.inspectorCollapsed = true;
    result.elements.clear();
    result.inspectorRect = {};
    result.inspectorDock = ShellInspectorDock::None;
    result.inspectorViewportCoverage = 0.0f;
    result.inspectorProtectedIntersectionArea = 0.0f;
    result.gameplayCorridorRect = result.safeRect;
    const bool compactTraining = model.manualInspectorPolicy &&
        model.screen == ProductScreen::TrainingSession;
    result.collapsedSummaryRect = {result.safeRect.x, result.safeRect.y,
        std::min(result.safeRect.width - 182.0f * result.baseScale,
            560.0f * result.baseScale),
        (compactTraining ? 116.0f : 34.0f) * result.baseScale};
    if (compactTraining && model.trajectoryInsetEnabled) {
        TrajectoryInsetShellRequest inset;
        inset.profileId = "product_" + std::to_string(viewport.physicalWidth) + "x" +
            std::to_string(viewport.physicalHeight) + "_ui" +
            std::to_string(static_cast<int>(std::lround(model.uiScale * 100)));
        inset.physicalWidth = viewport.physicalWidth;
        inset.physicalHeight = viewport.physicalHeight;
        inset.uiScale = model.uiScale;
        for (const char* id : kTrajectoryInsetExclusionIds)
            inset.exclusions.push_back({id, {}, false});
        const auto reserve = buildTrajectoryInsetShellLayout(inset);
        if (reserve.ok && reserve.insetVisible) {
            result.collapsedSummaryRect.width = std::min(result.collapsedSummaryRect.width,
                reserve.panelRect.x - 8.0f * model.uiScale - result.safeRect.x);
            const float hudHeight = std::min(result.safeRect.height * 0.28f,
                std::max(94.0f * std::max(result.deviceScale, model.uiScale), 88.0f));
            result.collapsedSummaryRect.height = std::min(166.0f * result.baseScale,
                result.safeRect.height - hudHeight - 56.0f * result.baseScale);
        }
    }
    const float collapsedActionY = result.collapsedSummaryRect.y +
        result.collapsedSummaryRect.height + 6.0f * result.baseScale;
    if (model.screen == ProductScreen::TrainingSession &&
        (model.training.state == training::TrainingProductState::AttemptEvaluated ||
         (model.training.attemptFacts.shotCount > 0 && !model.training.shotInProgress && !model.training.shotPrepared))) {
        const float base = result.baseScale;
        result.elements.push_back({"retry_training",
            {result.safeRect.x, compactTraining ? collapsedActionY : result.safeRect.y + 36.0f * base,
             std::min(result.collapsedSummaryRect.width * 0.5f - 4.0f * base,
                200.0f * base), 44.0f * base},
            false, true});
    }
    if (model.screen == ProductScreen::TrainingSession && model.manualInspectorPolicy) {
        const bool hasPrepare = std::any_of(model.items.begin(), model.items.end(),
            [](const auto& item) { return item.id == "prepare_training_shot"; });
        for (const auto& item : model.items) {
            if (item.id != (model.training.evaluation.succeeded ? "next_training" :
                    hasPrepare ? "prepare_training_shot" : "watch_training_reference")) continue;
            result.elements.push_back({item.id,
                {result.safeRect.x + (result.elements.empty() ? 0.0f :
                    result.elements.front().rect.width + 8.0f * result.baseScale),
                 collapsedActionY, std::min(200.0f * result.baseScale,
                    result.collapsedSummaryRect.width * 0.5f - 4.0f * result.baseScale),
                 44.0f * result.baseScale}, item.focused,
                 item.enabled && !model.training.shotInProgress && !model.training.shotPrepared});
        }
    }
    if (model.screen == ProductScreen::ReplayPlayback) {
        const float base = result.baseScale;
        for (const auto& item : model.items) {
            if (item.id != "replay_pause" && item.id != "replay_step_tick" &&
                item.id != "replay_camera_reset") continue;
            result.elements.push_back({item.id,
                {result.safeRect.x, result.safeRect.y +
                    (36.0f + 46.0f * result.elements.size()) * base,
                 std::min(result.safeRect.width, 200.0f * base), 44.0f * base},
                item.focused, item.enabled});
        }
    }
    result.ok = true;
    return result;
}

ShellHitTestResult hitTestShippingScreenLayout(
    const ShellScreenLayout& layout, double logicalX, double logicalY)
{
    ShellHitTestResult result;
    if (!layout.ok || !std::isfinite(logicalX) ||
        !std::isfinite(logicalY) || layout.viewport.logicalWidth <= 0 ||
        layout.viewport.logicalHeight <= 0)
        return result;
    const double physicalX = logicalX * layout.viewport.physicalWidth /
        layout.viewport.logicalWidth;
    const double physicalY = logicalY * layout.viewport.physicalHeight /
        layout.viewport.logicalHeight;
    const auto& toggle = layout.inspectorToggleRect;
    if (layout.boundedInspector && physicalX >= toggle.x && physicalX < toggle.x + toggle.width &&
        physicalY >= toggle.y && physicalY < toggle.y + toggle.height) {
        result.hit = result.enabled = true;
        result.id = "inspector_toggle";
        return result;
    }
    for (std::size_t index = 0; index < layout.elements.size(); ++index) {
        const ShellElementLayout& element = layout.elements[index];
        const product_hud::HudRect& rect = element.rect;
        if (physicalX >= rect.x && physicalX < rect.x + rect.width &&
            physicalY >= rect.y && physicalY < rect.y + rect.height) {
            result.hit = true;
            result.elementIndex = index;
            result.id = element.id;
            result.enabled = element.enabled;
            return result;
        }
    }
    return result;
}

ShellInspectorGeometryAudit auditBoundedInspectorGeometry(
    const ShellScreenLayout& layout,
    const std::vector<product_hud::HudRect>& panelRects,
    float fullFrameScrimAlpha)
{
    ShellInspectorGeometryAudit result;
    if (!layout.ok || !layout.boundedInspector) {
        result.errorCode = "shell_inspector_audit_layout_invalid";
        return result;
    }
    if (!std::isfinite(fullFrameScrimAlpha) || fullFrameScrimAlpha != 0.0f) {
        result.errorCode = "shell_inspector_full_frame_scrim_forbidden";
        return result;
    }
    const bool fullHd = layout.viewport.physicalWidth == 1920 &&
        layout.viewport.physicalHeight == 1080;
    const bool compact720 = layout.viewport.physicalWidth == 1280 &&
        layout.viewport.physicalHeight == 720;
    const float maximumWidth = fullHd ? 480.0f : compact720 ? 420.0f :
        layout.viewport.physicalWidth;
    const float maximumHeight = fullHd ? 780.0f : compact720 ? 620.0f :
        layout.viewport.physicalHeight;
    const float maximumAreaFraction = fullHd ? 0.18f :
        compact720 ? 0.29f : 1.0f;
    const float minimumCorridorWidth = fullHd ? 1240.0f :
        compact720 ? 760.0f : 0.0f;
    const float minimumCorridorHeight = fullHd ? 760.0f :
        compact720 ? 520.0f : 0.0f;
    if (layout.inspectorRect.width > maximumWidth ||
        layout.inspectorRect.height > maximumHeight ||
        layout.inspectorViewportCoverage > maximumAreaFraction ||
        layout.gameplayCorridorRect.width < minimumCorridorWidth ||
        layout.gameplayCorridorRect.height < minimumCorridorHeight) {
        result.errorCode = "shell_inspector_profile_bounds_failed";
        return result;
    }
    result.protectedIntersectionArea = intersectionArea(
        layout.inspectorRect, layout.protectedGameplayRect);
    if (result.protectedIntersectionArea > 0.0f) {
        result.errorCode = "shell_inspector_protected_corridor_intersection";
        return result;
    }
    for (const product_hud::HudRect& panel : panelRects) {
        result.totalPanelArea += panel.width * panel.height;
        if (panel.x < layout.inspectorRect.x ||
            panel.y < layout.inspectorRect.y ||
            panel.x + panel.width >
                layout.inspectorRect.x + layout.inspectorRect.width ||
            panel.y + panel.height >
                layout.inspectorRect.y + layout.inspectorRect.height) {
            result.errorCode = "shell_inspector_panel_outside_bounds";
            return result;
        }
    }
    result.ok = true;
    return result;
}

ShellInspectorContentAudit auditPhase4InspectorContent(
    const ProductShellSnapshot& model)
{
    ShellInspectorContentAudit result;
    const auto count = [&](const std::string& id) {
        return std::count_if(model.items.begin(), model.items.end(),
            [&](const ShellItemViewModel& value) { return value.id == id; });
    };
    const auto find = [&](const std::string& id) -> const ShellItemViewModel* {
        const auto found = std::find_if(model.items.begin(), model.items.end(),
            [&](const ShellItemViewModel& value) { return value.id == id; });
        return found == model.items.end() ? nullptr : &*found;
    };
    const auto validHash = [](const std::string& value) {
        if (value.size() != 71 || value.rfind("sha256:", 0) != 0) return false;
        return std::all_of(value.begin() + 7, value.end(), [](char c) {
            return std::isdigit(static_cast<unsigned char>(c)) ||
                (c >= 'a' && c <= 'f');
        });
    };
    if (model.screen == ProductScreen::TrainingSession) {
        const ShellItemViewModel* predicted = find("trajectory_predicted");
        const ShellItemViewModel* actual = find("trajectory_actual");
        if (predicted == nullptr || actual == nullptr ||
            predicted->label != "Predicted" || actual->label != "Replay actual" ||
            predicted->detail != "Rails " + std::to_string(
                model.trajectoryOverlay.predicted.railCollisionEvents) ||
            actual->detail != "Rails " + std::to_string(
                model.trajectoryOverlay.actual.railCollisionEvents)) {
            result.errorCode = "shell_inspector_legend_or_rails_missing";
            return result;
        }
        if (model.trajectoryOverlay.advancedExpanded) {
            const char* required[] = {
                "trajectory_predicted_sample_count",
                "trajectory_actual_sample_count",
                "trajectory_first_replay_tick",
                "trajectory_last_replay_tick",
                "trajectory_preview_budget",
                "trajectory_authority_status",
                "trajectory_trace_hash",
                "trajectory_content_hash"};
            for (const char* id : required) {
                const ShellItemViewModel* item = find(id);
                if (count(id) != 1 || item == nullptr || item->value.empty() ||
                    item->value == "unavailable" || item->value == "Unavailable") {
                    result.errorCode = std::string(
                        "shell_inspector_advanced_field_missing:") + id;
                    return result;
                }
            }
            if (!validHash(find("trajectory_trace_hash")->accessibleValue) ||
                !validHash(find("trajectory_content_hash")->accessibleValue)) {
                result.errorCode = "shell_inspector_advanced_full_hash_invalid";
                return result;
            }
        } else if (find("training_terminal") == nullptr ||
            (find("training_replay_status") == nullptr &&
                find("watch_training_reference") == nullptr) ||
            (find("preview_training") == nullptr &&
                find("retry_training") == nullptr) ||
            find("save_training_replay") == nullptr) {
            result.errorCode = "shell_inspector_compact_inventory_missing";
            return result;
        }
        if (count("training_advanced") != 1 ||
            model.items.back().id != "training_advanced") {
            result.errorCode = "shell_inspector_advanced_toggle_missing";
            return result;
        }
    } else if (model.screen == ProductScreen::ReplayPlayback) {
        const ShellItemViewModel* actual = find("trajectory_actual");
        const ShellItemViewModel* topology = find("trajectory_actual_topology");
        const ShellItemViewModel* endpoint = find("trajectory_actual_endpoint");
        const ShellItemViewModel* pause = find("replay_pause");
        const ShellItemViewModel* step = find("replay_step_tick");
        if (count("trajectory_actual") != 1 ||
            count("trajectory_predicted") != 0 || actual == nullptr ||
            actual->label != "Replay actual" ||
            actual->detail != "Rails " + std::to_string(
                model.trajectoryOverlay.actual.railCollisionEvents) ||
            topology == nullptr || topology->value != "Paired continuous rails" ||
            endpoint == nullptr || endpoint->value != "Paired capsules") {
            result.errorCode = "shell_inspector_actual_only_legend_invalid";
            return result;
        }
        if (pause == nullptr || step == nullptr ||
            pause->label != (model.replay.paused ? "Play" : "Pause") ||
            step->enabled != model.replay.paused) {
            result.errorCode = "shell_inspector_replay_transport_truth_invalid";
            return result;
        }
    } else {
        result.errorCode = "shell_inspector_content_screen_invalid";
        return result;
    }
    result.ok = true;
    return result;
}

}  // namespace billiardgl::render::ui
