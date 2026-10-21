#include "render/ui/training_goal_overlay.h"

#include "render/modern/camera_projection.h"
#include "training/training_product_controller.h"

#include <algorithm>
#include <cmath>
#include <limits>

namespace billiardgl::render::ui {
namespace {

constexpr int kCircleSegments = 64;
constexpr float kSurfaceBiasCm = 0.15f;
constexpr UiColor kDarkHalo{0.0f, 0.0f, 0.0f, 0.88f};
constexpr UiColor kStopZone{1.0f, 0.78f, 0.16f, 0.94f};
constexpr UiColor kTargetPocket{0.22f, 0.92f, 0.70f, 0.96f};
constexpr UiColor kDirection{0.94f, 0.96f, 0.96f, 0.90f};
constexpr std::array<const char*, 6> kPocketIds{{"corner_nx_nz",
    "corner_px_nz", "corner_nx_pz", "corner_px_pz", "side_nx",
    "side_px"}};

bool finite(float value) { return std::isfinite(value); }

bool finite(const TrainingGoalPointCm& point)
{
    return finite(point.x) && finite(point.z);
}

bool clipSegmentToPlayfield(const TableSpec& table,
    TrainingGoalPointCm& first, TrainingGoalPointCm& second)
{
    const float halfWidth = table.playfieldWidthCm * 0.5f;
    const float halfLength = table.playfieldLengthCm * 0.5f;
    const TrainingGoalPointCm originalFirst = first;
    const TrainingGoalPointCm originalSecond = second;
    const double dx = originalSecond.x - originalFirst.x;
    const double dz = originalSecond.z - originalFirst.z;
    double minimum = 0.0;
    double maximum = 1.0;
    const auto boundary = [&](double p, double q) {
        if (std::fabs(p) <= 1e-12) return q >= 0.0;
        const double ratio = q / p;
        if (p < 0.0) minimum = std::max(minimum, ratio);
        else maximum = std::min(maximum, ratio);
        return minimum <= maximum;
    };
    if (!boundary(-dx, originalFirst.x + halfWidth) ||
        !boundary(dx, halfWidth - originalFirst.x) ||
        !boundary(-dz, originalFirst.z + halfLength) ||
        !boundary(dz, halfLength - originalFirst.z))
        return false;
    first = {static_cast<float>(originalFirst.x + dx * minimum),
        static_cast<float>(originalFirst.z + dz * minimum)};
    second = {static_cast<float>(originalFirst.x + dx * maximum),
        static_cast<float>(originalFirst.z + dz * maximum)};
    return true;
}

bool insideClip(const HomogeneousPoint& point)
{
    return finiteHomogeneous(point) && point.w > 1e-5 &&
        point.x >= -point.w && point.x <= point.w &&
        point.y >= -point.w && point.y <= point.w &&
        point.z >= -point.w && point.z <= point.w;
}

bool project(const SharedCameraProjection& projection,
    float clothSurfaceYCm, const TrainingGoalPointCm& point, UiPoint& output)
{
    const Point3 world{point.x, clothSurfaceYCm + kSurfaceBiasCm, point.z};
    const HomogeneousPoint clip = transformWorldToClip(projection, world);
    PhysicalScreenPoint physical;
    if (!insideClip(clip) || !clipToPhysicalScreen(projection, clip, physical))
        return false;
    output = {static_cast<float>(physical.x), static_cast<float>(physical.y)};
    return finite(output.x) && finite(output.y);
}

void appendTriangle(HudDrawList& draw, const UiPoint& first,
    const UiPoint& second, const UiPoint& third, const UiColor& color,
    const std::string& token)
{
    draw.triangles.push_back(
        {first, second, third, color, draw.safeRect, token, token, true});
}

bool appendStroke(HudDrawList& draw, const UiPoint& first,
    const UiPoint& second, float widthPx, const UiColor& color,
    const std::string& token)
{
    const float dx = second.x - first.x;
    const float dy = second.y - first.y;
    const float length = std::hypot(dx, dy);
    if (!finite(length) || length <= 1e-4f || !finite(widthPx) ||
        widthPx <= 0.0f)
        return false;
    const float nx = -dy / length * widthPx * 0.5f;
    const float ny = dx / length * widthPx * 0.5f;
    const UiPoint a{first.x + nx, first.y + ny};
    const UiPoint b{second.x + nx, second.y + ny};
    const UiPoint c{second.x - nx, second.y - ny};
    const UiPoint d{first.x - nx, first.y - ny};
    appendTriangle(draw, a, b, c, color, token);
    appendTriangle(draw, a, c, d, color, token);
    return true;
}

std::size_t appendCircle(HudDrawList& draw,
    const SharedCameraProjection& projection, float clothSurfaceYCm,
    const TableSpec& table, const TrainingGoalCircle& circle, float scale,
    const UiColor& color, const std::string& token)
{
    std::size_t count = 0;
    constexpr double tau = 6.28318530717958647692;
    for (int index = 0; index < kCircleSegments; ++index) {
        const double firstAngle = tau * index / kCircleSegments;
        const double secondAngle = tau * (index + 1) / kCircleSegments;
        TrainingGoalPointCm first{
            circle.center.x + circle.radiusCm * static_cast<float>(std::cos(firstAngle)),
            circle.center.z + circle.radiusCm * static_cast<float>(std::sin(firstAngle))};
        TrainingGoalPointCm second{
            circle.center.x + circle.radiusCm * static_cast<float>(std::cos(secondAngle)),
            circle.center.z + circle.radiusCm * static_cast<float>(std::sin(secondAngle))};
        if (!clipSegmentToPlayfield(table, first, second)) continue;
        UiPoint firstPhysical;
        UiPoint secondPhysical;
        if (!project(projection, clothSurfaceYCm, first, firstPhysical) ||
            !project(projection, clothSurfaceYCm, second, secondPhysical))
            continue;
        if (appendStroke(draw, firstPhysical, secondPhysical, 5.0f * scale,
                kDarkHalo, token + "_halo") &&
            appendStroke(draw, firstPhysical, secondPhysical, 2.25f * scale,
                color, token))
            ++count;
    }
    return count;
}

std::size_t appendDashedLine(HudDrawList& draw,
    const SharedCameraProjection& projection, float clothSurfaceYCm,
    const TableSpec& table, const std::vector<TrainingGoalPointCm>& points,
    float scale, std::size_t& arrowStrokeCount)
{
    std::size_t dashes = 0;
    const float dashPx = 12.0f * scale;
    const float gapPx = 7.0f * scale;
    for (std::size_t index = 1; index < points.size(); ++index) {
        TrainingGoalPointCm firstWorld = points[index - 1];
        TrainingGoalPointCm secondWorld = points[index];
        if (!clipSegmentToPlayfield(table, firstWorld, secondWorld)) continue;
        UiPoint first;
        UiPoint second;
        if (!project(projection, clothSurfaceYCm, firstWorld, first) ||
            !project(projection, clothSurfaceYCm, secondWorld, second))
            continue;
        const float dx = second.x - first.x;
        const float dy = second.y - first.y;
        const float length = std::hypot(dx, dy);
        if (!finite(length) || length <= 1e-4f) continue;
        for (float cursor = 0.0f; cursor < length; cursor += dashPx + gapPx) {
            const float end = std::min(length, cursor + dashPx);
            const UiPoint a{first.x + dx * cursor / length,
                first.y + dy * cursor / length};
            const UiPoint b{first.x + dx * end / length,
                first.y + dy * end / length};
            const bool halo = appendStroke(draw, a, b, 4.5f * scale,
                kDarkHalo, "training_goal_direction_halo");
            const bool core = appendStroke(draw, a, b, 1.75f * scale,
                kDirection, "training_goal_direction");
            if (halo && core) ++dashes;
        }
    }
    TrainingGoalPointCm arrowStartWorld = points[points.size() - 2];
    TrainingGoalPointCm arrowEndWorld = points.back();
    if (clipSegmentToPlayfield(table, arrowStartWorld, arrowEndWorld)) {
        UiPoint start;
        UiPoint end;
        if (project(projection, clothSurfaceYCm, arrowStartWorld, start) &&
            project(projection, clothSurfaceYCm, arrowEndWorld, end)) {
            const float dx = end.x - start.x;
            const float dy = end.y - start.y;
            const float length = std::hypot(dx, dy);
            if (length > 1e-4f) {
                const float ux = dx / length;
                const float uy = dy / length;
                const float nx = -uy;
                const float ny = ux;
                const float back = 12.0f * scale;
                const float spread = 6.0f * scale;
                const UiPoint left{end.x - ux * back + nx * spread,
                    end.y - uy * back + ny * spread};
                const UiPoint right{end.x - ux * back - nx * spread,
                    end.y - uy * back - ny * spread};
                if (appendStroke(draw, left, end, 4.5f * scale, kDarkHalo,
                        "training_goal_direction_arrow_halo") &&
                    appendStroke(draw, left, end, 1.75f * scale, kDirection,
                        "training_goal_direction_arrow"))
                    ++arrowStrokeCount;
                if (appendStroke(draw, right, end, 4.5f * scale, kDarkHalo,
                        "training_goal_direction_arrow_halo") &&
                    appendStroke(draw, right, end, 1.75f * scale, kDirection,
                        "training_goal_direction_arrow"))
                    ++arrowStrokeCount;
            }
        }
    }
    return dashes;
}

bool collinear(const std::vector<TrainingGoalPointCm>& points)
{
    if (points.size() < 2) return true;
    const double dx = points.back().x - points.front().x;
    const double dz = points.back().z - points.front().z;
    const double length = std::hypot(dx, dz);
    if (length <= 1e-6) return false;
    for (std::size_t index = 1; index + 1 < points.size(); ++index) {
        const double rx = points[index].x - points.front().x;
        const double rz = points[index].z - points.front().z;
        if (std::fabs(dx * rz - dz * rx) / length > 1e-3) return false;
    }
    return true;
}

bool validTable(const TableSpec& table, const PocketSpec& pockets)
{
    return finite(table.playfieldWidthCm) && table.playfieldWidthCm > 0.0f &&
        finite(table.playfieldLengthCm) && table.playfieldLengthCm > 0.0f &&
        finite(table.clothSurfaceYCm) && finite(pockets.cornerMouthWidthCm) &&
        pockets.cornerMouthWidthCm > 0.0f &&
        finite(pockets.sideMouthWidthCm) && pockets.sideMouthWidthCm > 0.0f;
}

}  // namespace

TrainingGoalOverlayRequest makeTrainingGoalOverlayRequest(
    const training::TrainingTeachingHints& hints,
    const CameraPresentation& camera, int physicalWidth, int physicalHeight,
    float uiScale, const TableSpec& table, const PocketSpec& pockets)
{
    TrainingGoalOverlayRequest request;
    request.camera = camera;
    request.physicalWidth = physicalWidth;
    request.physicalHeight = physicalHeight;
    request.uiScale = uiScale;
    request.table = table;
    request.pockets = pockets;
    if (!hints.available) return request;
    request.cueBallTargetZone.visible = hints.cueBallTargetEnabled;
    request.cueBallTargetZone.center = {
        hints.cueBallTargetCenterCm[0], hints.cueBallTargetCenterCm[1]};
    request.cueBallTargetZone.radiusCm = hints.cueBallTargetRadiusCm;
    request.targetPocket.visible = hints.targetPocketEnabled && !hints.targetPocketId.empty();
    request.targetPocket.canonicalId = hints.targetPocketId;
    request.targetPocket.center = {
        hints.targetPocketCenterCm[0], hints.targetPocketCenterCm[1]};
    if (request.targetPocket.visible) {
        const auto openings = buildPocketOpenings(table, pockets);
        for (std::size_t index = 0; index < openings.size(); ++index) {
            const PocketOpening& opening = openings[index];
            if (std::hypot(request.targetPocket.center.x - opening.centerX,
                    request.targetPocket.center.z - opening.centerZ) > 1e-3f)
                continue;
            request.targetPocket.pocketId = static_cast<int>(index);
            request.targetPocket.markerRadiusCm = opening.mouthWidthCm * 0.5f;
            break;
        }
    }
    request.collinearDirectionPoints = {
        {hints.shotLineStartCm[0], hints.shotLineStartCm[1]},
        {hints.shotLineObjectCenterCm[0], hints.shotLineObjectCenterCm[1]}};
    if (hints.targetPocketEnabled)
        request.collinearDirectionPoints.push_back(
            {hints.targetPocketCenterCm[0], hints.targetPocketCenterCm[1]});
    if (hints.targetPocketEnabled && !collinear(request.collinearDirectionPoints)) {
        const auto cue = request.collinearDirectionPoints[0];
        const auto object = request.collinearDirectionPoints[1];
        const auto pocket = request.collinearDirectionPoints[2];
        const float dx = pocket.x - object.x, dz = pocket.z - object.z;
        const float length = std::hypot(dx, dz);
        if (length > 1e-5f) {
            const TrainingGoalPointCm ghost{object.x - 2.0f * kChineseBallRadiusCm * dx / length,
                object.z - 2.0f * kChineseBallRadiusCm * dz / length};
            const float approachLength = std::hypot(ghost.x - cue.x, ghost.z - cue.z);
            // Side-spin lessons calibrate aim compensation; do not replace
            // their recommended yaw with an uncompensated geometric ghost.
            request.cueApproachPoints = {cue,
                {cue.x + approachLength * std::cos(hints.aimYawRadians),
                 cue.z + approachLength * std::sin(hints.aimYawRadians)}};
        }
        request.collinearDirectionPoints = {object, pocket};
    }
    return request;
}

TrainingGoalOverlayDrawPackage buildTrainingGoalOverlayDrawPackage(
    const TrainingGoalOverlayRequest& request)
{
    TrainingGoalOverlayDrawPackage result;
    result.draw.safeRect = {0.0f, 0.0f,
        static_cast<float>(request.physicalWidth),
        static_cast<float>(request.physicalHeight)};
    if (request.physicalWidth <= 0 || request.physicalHeight <= 0 ||
        !finite(request.uiScale) || request.uiScale < 0.85f ||
        request.uiScale > 1.35f || !validTable(request.table, request.pockets)) {
        result.errorCode = "training_goal_overlay_request_invalid";
        result.draw.errorCode = result.errorCode;
        return result;
    }
    const bool anyVisible = request.cueBallTargetZone.visible ||
        request.targetPocket.visible || !request.collinearDirectionPoints.empty();
    if (!anyVisible) {
        result.errorCode = "training_goal_overlay_empty";
        result.draw.errorCode = result.errorCode;
        return result;
    }
    if ((request.cueBallTargetZone.visible &&
            (!finite(request.cueBallTargetZone.center) ||
                !finite(request.cueBallTargetZone.radiusCm) ||
                request.cueBallTargetZone.radiusCm <= 0.0f)) ||
        (request.targetPocket.visible &&
            (!finite(request.targetPocket.center) ||
                !finite(request.targetPocket.markerRadiusCm) ||
                request.targetPocket.markerRadiusCm <= 0.0f ||
                request.targetPocket.pocketId < 0 ||
                request.targetPocket.pocketId >= 6))) {
        result.errorCode = "training_goal_overlay_marker_invalid";
        result.draw.errorCode = result.errorCode;
        return result;
    }
    for (const TrainingGoalPointCm& point : request.collinearDirectionPoints) {
        if (!finite(point)) {
            result.errorCode = "training_goal_overlay_line_invalid";
            result.draw.errorCode = result.errorCode;
            return result;
        }
    }
    if (!request.cueApproachPoints.empty() &&
        (request.cueApproachPoints.size() != 2 || !finite(request.cueApproachPoints[0]) ||
            !finite(request.cueApproachPoints[1]))) {
        result.errorCode = "training_goal_overlay_approach_invalid";
        result.draw.errorCode = result.errorCode;
        return result;
    }
    if (!request.collinearDirectionPoints.empty() &&
        (request.collinearDirectionPoints.size() < 2 ||
            !collinear(request.collinearDirectionPoints))) {
        result.errorCode = "training_goal_overlay_line_not_collinear";
        result.draw.errorCode = result.errorCode;
        return result;
    }
    if (request.targetPocket.visible) {
        const auto openings = buildPocketOpenings(request.table, request.pockets);
        const PocketOpening& opening =
            openings[static_cast<std::size_t>(request.targetPocket.pocketId)];
        if ((!request.targetPocket.canonicalId.empty() &&
                request.targetPocket.canonicalId !=
                    kPocketIds[static_cast<std::size_t>(
                        request.targetPocket.pocketId)]) ||
            std::hypot(request.targetPocket.center.x - opening.centerX,
                request.targetPocket.center.z - opening.centerZ) > 1e-3f) {
            result.errorCode = "training_goal_overlay_pocket_identity_mismatch";
            result.draw.errorCode = result.errorCode;
            return result;
        }
    }

    const SharedCameraProjection projection = buildSharedCameraProjection(
        request.camera, request.physicalWidth, request.physicalHeight);
    if (!projection.valid) {
        result.errorCode = "training_goal_overlay_projection_invalid";
        result.draw.errorCode = result.errorCode;
        return result;
    }
    result.clothSurfaceYCm = request.table.clothSurfaceYCm;
    result.usedSharedCameraProjection = true;
    result.physicalPixelCoordinates = true;
    result.transparentUnfilledMarkers = true;
    result.playfieldCpuClipApplied = true;
    const float scale = request.uiScale * request.physicalHeight / 720.0f;

    if (request.cueBallTargetZone.visible) {
        UiPoint center;
        result.stopZoneCenterVisible = project(projection,
            request.table.clothSurfaceYCm,
            request.cueBallTargetZone.center, center);
        if (result.stopZoneCenterVisible)
            result.stopZoneCenterPhysicalPx = {{center.x, center.y}};
        result.stopZoneStrokeSegments = appendCircle(result.draw, projection,
            request.table.clothSurfaceYCm, request.table,
            request.cueBallTargetZone, scale, kStopZone,
            "training_goal_stop_zone");
    }
    if (request.targetPocket.visible) {
        UiPoint center;
        result.targetPocketCenterVisible = project(projection,
            request.table.clothSurfaceYCm, request.targetPocket.center, center);
        if (result.targetPocketCenterVisible)
            result.targetPocketCenterPhysicalPx = {{center.x, center.y}};
        const TrainingGoalCircle pocketCircle{true,
            request.targetPocket.center, request.targetPocket.markerRadiusCm};
        result.targetPocketStrokeSegments = appendCircle(result.draw, projection,
            request.table.clothSurfaceYCm, request.table, pocketCircle, scale,
            kTargetPocket, "training_goal_target_pocket");
    }
    if (!request.collinearDirectionPoints.empty())
        result.directionDashCount = appendDashedLine(result.draw, projection,
            request.table.clothSurfaceYCm, request.table,
            request.collinearDirectionPoints, scale,
            result.directionArrowStrokeCount);
    if (!request.cueApproachPoints.empty())
        result.directionDashCount += appendDashedLine(result.draw, projection,
            request.table.clothSurfaceYCm, request.table, request.cueApproachPoints,
            scale, result.directionArrowStrokeCount);

    const bool anyDrawable = result.stopZoneStrokeSegments > 0 ||
        result.targetPocketStrokeSegments > 0 || result.directionDashCount > 0 ||
        result.directionArrowStrokeCount > 0;
    if (!anyDrawable) {
        result.draw.triangles.clear();
        result.errorCode = "training_goal_overlay_not_visible";
        result.draw.errorCode = result.errorCode;
        return result;
    }
    result.ok = true;
    result.draw.ok = true;
    return result;
}

}  // namespace billiardgl::render::ui
