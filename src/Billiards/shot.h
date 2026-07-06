#pragma once

#include "game_state.h"

namespace billiardgl {

Point3 aimDirectionOnTable(float yaw);
Point3 shotVelocityFromAim(float yaw, float power);

}  // namespace billiardgl
