#include "render/modern/table_plane_pointer_projection.h"

#include "render/modern/camera_projection.h"

#include <algorithm>
#include <array>
#include <cmath>

namespace billiardgl::render {
namespace {

using DVec3 = std::array<double, 3>;

bool finite(const PresentationPoint& value)
{
    return std::isfinite(value.x) && std::isfinite(value.y) &&
        std::isfinite(value.z);
}

bool finite(const PointerProjectionRect& value)
{
    return std::isfinite(value.x) && std::isfinite(value.y) &&
        std::isfinite(value.width) && std::isfinite(value.height) &&
        value.width >= 0.0 && value.height >= 0.0;
}

DVec3 subtract(const PresentationPoint& first,
    const PresentationPoint& second)
{
    return {{static_cast<double>(first.x) - second.x,
        static_cast<double>(first.y) - second.y,
        static_cast<double>(first.z) - second.z}};
}

double dot(const DVec3& first, const DVec3& second)
{
    return first[0] * second[0] + first[1] * second[1] +
        first[2] * second[2];
}

DVec3 cross(const DVec3& first, const DVec3& second)
{
    return {{first[1] * second[2] - first[2] * second[1],
        first[2] * second[0] - first[0] * second[2],
        first[0] * second[1] - first[1] * second[0]}};
}

bool normalize(DVec3& value)
{
    const double length = std::sqrt(dot(value, value));
    if (!std::isfinite(length) || length <= 1.0e-12) return false;
    for (double& component : value) component /= length;
    return true;
}

PresentationPoint point(const DVec3& value)
{
    return {static_cast<float>(value[0]), static_cast<float>(value[1]),
        static_cast<float>(value[2])};
}

bool insideClipVolume(const HomogeneousPoint& clip)
{
    if (!finiteHomogeneous(clip) || clip.w <= 1.0e-5) return false;
    return clip.x >= -clip.w && clip.x <= clip.w &&
        clip.y >= -clip.w && clip.y <= clip.w &&
        clip.z >= -clip.w && clip.z <= clip.w;
}

TablePlanePointerProjectionResult failure(const std::string& code)
{
    TablePlanePointerProjectionResult result;
    result.errorCode = code;
    return result;
}

}  // namespace

TablePlanePointerProjectionResult projectPointerToTablePlane(
    const TablePlanePointerProjectionRequest& request)
{
    const auto& viewport = request.viewport;
    if (viewport.logicalWidth <= 0 || viewport.logicalHeight <= 0 ||
        viewport.physicalWidth <= 0 || viewport.physicalHeight <= 0)
        return failure("pointer_projection_viewport_invalid");
    if (!std::isfinite(request.logicalX) ||
        !std::isfinite(request.logicalY) ||
        !std::isfinite(request.tablePlaneY) ||
        !finite(request.camera.eye) || !finite(request.camera.target) ||
        !finite(request.camera.projectionPan) ||
        !std::isfinite(request.camera.fieldOfViewDegrees))
        return failure("pointer_projection_nonfinite");
    if (request.logicalX < 0.0 || request.logicalY < 0.0 ||
        request.logicalX >= viewport.logicalWidth ||
        request.logicalY >= viewport.logicalHeight)
        return failure("pointer_projection_outside_viewport");

    TablePlanePointerProjectionResult result;
    result.physicalX = request.logicalX * viewport.physicalWidth /
        viewport.logicalWidth;
    result.physicalY = request.logicalY * viewport.physicalHeight /
        viewport.logicalHeight;
    for (const PointerProjectionRect& rect :
         request.excludedPhysicalRects) {
        if (!finite(rect))
            return failure("pointer_projection_exclusion_invalid");
        if (result.physicalX >= rect.x &&
            result.physicalY >= rect.y &&
            result.physicalX < rect.x + rect.width &&
            result.physicalY < rect.y + rect.height)
            return failure("pointer_occluded_by_ui");
    }

    CameraPresentation camera = request.camera;
    const PresentationPoint eye{
        camera.eye.x + camera.projectionPan.x,
        camera.eye.y + camera.projectionPan.y,
        camera.eye.z + camera.projectionPan.z};
    const PresentationPoint target{
        camera.target.x + camera.projectionPan.x,
        camera.target.y + camera.projectionPan.y,
        camera.target.z + camera.projectionPan.z};
    DVec3 forward = subtract(target, eye);
    if (!normalize(forward))
        return failure("pointer_projection_camera_invalid");
    const DVec3 worldUp{{0.0, 1.0, 0.0}};
    const DVec3 stableUp = std::fabs(dot(forward, worldUp)) > 0.98 ?
        DVec3{{0.0, 0.0, 1.0}} : worldUp;
    DVec3 side = cross(forward, stableUp);
    if (!normalize(side))
        return failure("pointer_projection_camera_invalid");
    const DVec3 up = cross(side, forward);

    const double ndcX = 2.0 * result.physicalX /
        viewport.physicalWidth - 1.0;
    const double ndcY = 1.0 - 2.0 * result.physicalY /
        viewport.physicalHeight;
    const double fov = std::clamp(
        static_cast<double>(camera.fieldOfViewDegrees), 20.0, 90.0);
    const double tangent = std::tan(fov * 3.14159265358979323846 / 360.0);
    const double aspect = static_cast<double>(viewport.physicalWidth) /
        viewport.physicalHeight;
    DVec3 direction{{
        forward[0] + side[0] * ndcX * aspect * tangent +
            up[0] * ndcY * tangent,
        forward[1] + side[1] * ndcX * aspect * tangent +
            up[1] * ndcY * tangent,
        forward[2] + side[2] * ndcX * aspect * tangent +
            up[2] * ndcY * tangent}};
    if (!normalize(direction) || std::fabs(direction[1]) <= 1.0e-12)
        return failure("pointer_projection_parallel_to_table");
    const double distance = (request.tablePlaneY - eye.y) / direction[1];
    if (!std::isfinite(distance) || distance <= 0.0)
        return failure("pointer_projection_table_behind_camera");
    const DVec3 intersection{{
        eye.x + direction[0] * distance,
        request.tablePlaneY,
        eye.z + direction[2] * distance}};
    if (!std::all_of(intersection.begin(), intersection.end(),
            [](double value) { return std::isfinite(value); }))
        return failure("pointer_projection_nonfinite");

    const PresentationPoint tablePoint = point(intersection);
    const Point3 clipPoint{tablePoint.x, tablePoint.y, tablePoint.z};
    const SharedCameraProjection projection = buildSharedCameraProjection(
        request.camera, viewport.physicalWidth, viewport.physicalHeight);
    if (!projection.valid || !insideClipVolume(
            transformWorldToClip(projection, clipPoint)))
        return failure("pointer_projection_outside_clip_volume");

    result.ok = true;
    result.rayOrigin = eye;
    result.rayDirection = point(direction);
    result.tablePoint = tablePoint;
    return result;
}

}  // namespace billiardgl::render
