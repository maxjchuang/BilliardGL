#pragma once

#include "game_state.h"
#include "presentation_snapshot.h"
#include "render/modern/render_data.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <limits>

namespace billiardgl::render {

struct HomogeneousPoint {
    double x = 0.0;
    double y = 0.0;
    double z = 0.0;
    double w = 0.0;
};

struct PhysicalScreenPoint {
    double x = 0.0;
    double y = 0.0;
};

struct SharedCameraProjection {
    Matrix4 view{};
    Matrix4 projection{};
    Matrix4 viewProjection{};
    int physicalWidth = 0;
    int physicalHeight = 0;
    bool valid = false;
};

inline Matrix4 multiplyMatrix4(const Matrix4& first, const Matrix4& second)
{
    Matrix4 result{};
    for (int column = 0; column < 4; ++column)
        for (int row = 0; row < 4; ++row)
            for (int index = 0; index < 4; ++index)
                result[static_cast<std::size_t>(column * 4 + row)] +=
                    first[static_cast<std::size_t>(index * 4 + row)] *
                    second[static_cast<std::size_t>(column * 4 + index)];
    return result;
}

inline Float3 normalizeProjectionVector(Float3 value)
{
    const float length = std::sqrt(value[0] * value[0] +
        value[1] * value[1] + value[2] * value[2]);
    if (!std::isfinite(length) || length <= 1e-6f) return {};
    for (float& component : value) component /= length;
    return value;
}

inline Float3 projectionCross(const Float3& first, const Float3& second)
{
    return {{first[1] * second[2] - first[2] * second[1],
        first[2] * second[0] - first[0] * second[2],
        first[0] * second[1] - first[1] * second[0]}};
}

inline float projectionDot(const Float3& first, const Float3& second)
{
    return first[0] * second[0] + first[1] * second[1] +
        first[2] * second[2];
}

inline Float3 sharedCameraEye(const CameraPresentation& camera)
{
    return {{camera.eye.x + camera.projectionPan.x,
        camera.eye.y + camera.projectionPan.y,
        camera.eye.z + camera.projectionPan.z}};
}

inline Float3 sharedCameraTarget(const CameraPresentation& camera)
{
    return {{camera.target.x + camera.projectionPan.x,
        camera.target.y + camera.projectionPan.y,
        camera.target.z + camera.projectionPan.z}};
}

inline Matrix4 sharedLookAt(const CameraPresentation& camera)
{
    const Float3 eye = sharedCameraEye(camera);
    const Float3 target = sharedCameraTarget(camera);
    const Float3 forward = normalizeProjectionVector({{
        target[0] - eye[0], target[1] - eye[1], target[2] - eye[2]}});
    const Float3 worldUp{{0.0f, 1.0f, 0.0f}};
    const Float3 stableUp = std::fabs(projectionDot(forward, worldUp)) > 0.98f ?
        Float3{{0.0f, 0.0f, 1.0f}} : worldUp;
    const Float3 side = normalizeProjectionVector(
        projectionCross(forward, stableUp));
    const Float3 up = projectionCross(side, forward);
    return {{side[0], up[0], -forward[0], 0.0f,
        side[1], up[1], -forward[1], 0.0f,
        side[2], up[2], -forward[2], 0.0f,
        -projectionDot(side, eye), -projectionDot(up, eye),
        projectionDot(forward, eye), 1.0f}};
}

inline Matrix4 sharedPerspective(float fieldOfViewDegrees, float aspect)
{
    const float clamped = std::clamp(fieldOfViewDegrees, 20.0f, 90.0f);
    const float radians = clamped * 0.01745329251994329577f;
    const float q = 1.0f / std::tan(radians * 0.5f);
    constexpr float nearPlane = 2.0f;
    constexpr float farPlane = 1800.0f;
    Matrix4 result{};
    result[0] = q / aspect;
    result[5] = q;
    result[10] = (farPlane + nearPlane) / (nearPlane - farPlane);
    result[11] = -1.0f;
    result[14] = 2.0f * farPlane * nearPlane /
        (nearPlane - farPlane);
    return result;
}

inline SharedCameraProjection buildSharedCameraProjection(
    const CameraPresentation& camera, int physicalWidth, int physicalHeight)
{
    SharedCameraProjection result;
    result.physicalWidth = physicalWidth;
    result.physicalHeight = physicalHeight;
    if (physicalWidth <= 0 || physicalHeight <= 0 ||
        !std::isfinite(camera.fieldOfViewDegrees))
        return result;
    result.view = sharedLookAt(camera);
    result.projection = sharedPerspective(camera.fieldOfViewDegrees,
        static_cast<float>(physicalWidth) / physicalHeight);
    result.viewProjection = multiplyMatrix4(
        result.projection, result.view);
    result.valid = std::all_of(result.viewProjection.begin(),
        result.viewProjection.end(), [](float value) {
            return std::isfinite(value);
        });
    return result;
}

inline HomogeneousPoint transformWorldToClip(
    const SharedCameraProjection& projection, const Point3& world)
{
    const Matrix4& matrix = projection.viewProjection;
    return {
        matrix[0] * world.x + matrix[4] * world.y +
            matrix[8] * world.z + matrix[12],
        matrix[1] * world.x + matrix[5] * world.y +
            matrix[9] * world.z + matrix[13],
        matrix[2] * world.x + matrix[6] * world.y +
            matrix[10] * world.z + matrix[14],
        matrix[3] * world.x + matrix[7] * world.y +
            matrix[11] * world.z + matrix[15]};
}

inline bool finiteHomogeneous(const HomogeneousPoint& point)
{
    return std::isfinite(point.x) && std::isfinite(point.y) &&
        std::isfinite(point.z) && std::isfinite(point.w);
}

inline HomogeneousPoint interpolateHomogeneous(
    const HomogeneousPoint& first, const HomogeneousPoint& second, double t)
{
    return {first.x + (second.x - first.x) * t,
        first.y + (second.y - first.y) * t,
        first.z + (second.z - first.z) * t,
        first.w + (second.w - first.w) * t};
}

inline bool clipHomogeneousSegment(HomogeneousPoint& first,
    HomogeneousPoint& second)
{
    if (!finiteHomogeneous(first) || !finiteHomogeneous(second)) return false;
    const auto values = [](const HomogeneousPoint& point) {
        return std::array<double, 7>{{point.w + point.x, point.w - point.x,
            point.w + point.y, point.w - point.y, point.w + point.z,
            point.w - point.z, point.w - 1e-5}};
    };
    const HomogeneousPoint originalFirst = first;
    const HomogeneousPoint originalSecond = second;
    const auto firstValues = values(originalFirst);
    const auto secondValues = values(originalSecond);
    double minimum = 0.0;
    double maximum = 1.0;
    for (std::size_t index = 0; index < firstValues.size(); ++index) {
        const double a = firstValues[index];
        const double b = secondValues[index];
        if (a < 0.0 && b < 0.0) return false;
        if (a >= 0.0 && b >= 0.0) continue;
        const double crossing = a / (a - b);
        if (a < 0.0) minimum = std::max(minimum, crossing);
        else maximum = std::min(maximum, crossing);
        if (minimum > maximum) return false;
    }
    first = interpolateHomogeneous(originalFirst, originalSecond, minimum);
    second = interpolateHomogeneous(originalFirst, originalSecond, maximum);
    return first.w > 1e-5 && second.w > 1e-5;
}

inline bool clipToPhysicalScreen(const SharedCameraProjection& projection,
    const HomogeneousPoint& clip, PhysicalScreenPoint& screen)
{
    if (!projection.valid || !finiteHomogeneous(clip) || clip.w <= 1e-5)
        return false;
    const double ndcX = clip.x / clip.w;
    const double ndcY = clip.y / clip.w;
    if (!std::isfinite(ndcX) || !std::isfinite(ndcY)) return false;
    screen.x = (ndcX * 0.5 + 0.5) * projection.physicalWidth;
    screen.y = (1.0 - (ndcY * 0.5 + 0.5)) * projection.physicalHeight;
    return std::isfinite(screen.x) && std::isfinite(screen.y);
}

}  // namespace billiardgl::render
