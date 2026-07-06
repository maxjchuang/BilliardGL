#include "shot.h"

#include <cmath>

namespace billiardgl {

Point3 aimDirectionOnTable(float yaw)
{
    return Point3{std::cos(yaw), 0.0f, std::sin(yaw)};
}

Point3 shotVelocityFromAim(float yaw, float power)
{
    const Point3 direction = aimDirectionOnTable(yaw);
    return Point3{direction.x * power, 0.0f, direction.z * power};
}

}  // namespace billiardgl
