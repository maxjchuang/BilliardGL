#pragma once

#include "game_state.h"

namespace billiardgl {

Point3 aimDirectionOnTable(float yaw);
Point3 shotVelocityFromAim(float yaw, float power);
Point3 cueLineStartFromAim(float yaw);
Point3 cueLineEndFromAim(float yaw, float length);
Point3 cueStickPositionFromAim(const Point3& cueBallPosition, float yaw, float shotPower);
float cueStickRotationDegreesFromAim(float yaw);

}  // namespace billiardgl
