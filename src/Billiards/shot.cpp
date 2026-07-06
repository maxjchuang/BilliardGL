#include "shot.h"

#include <cmath>

namespace billiardgl {
namespace {

constexpr float kCueTipGap = 1.0f;
constexpr float kCuePowerBackoffScale = 0.1f;
constexpr float kCueBaseBackoff = kBallRadius * 3.0f;
constexpr float kCueLineHeight = kBallRadius * 1.2f;

}  // namespace

Point3 aimDirectionOnTable(float yaw)
{
    return Point3{std::cos(yaw), 0.0f, std::sin(yaw)};
}

Point3 shotVelocityFromAim(float yaw, float power)
{
    const Point3 direction = aimDirectionOnTable(yaw);
    return Point3{direction.x * power, 0.0f, direction.z * power};
}

Point3 cueLineEndFromAim(float yaw, float length)
{
    const Point3 direction = aimDirectionOnTable(yaw);
    return Point3{direction.x * length, kCueLineHeight, direction.z * length};
}

Point3 cueLineStartFromAim(float yaw)
{
    const Point3 direction = aimDirectionOnTable(yaw);
    const float start = kBallRadius + kCueTipGap;
    return Point3{direction.x * start, kCueLineHeight, direction.z * start};
}

Point3 cueStickPositionFromAim(const Point3& cueBallPosition, float yaw, float shotPower)
{
    const Point3 direction = aimDirectionOnTable(yaw);
    const float backoff = kCueBaseBackoff + kCueTipGap + shotPower * kCuePowerBackoffScale;
    return Point3{
        cueBallPosition.x - direction.x * backoff,
        cueBallPosition.y,
        cueBallPosition.z - direction.z * backoff};
}

}  // namespace billiardgl
