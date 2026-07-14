#pragma once

#include "cue_impact.h"
#include "game_state.h"
#include "physics_profile.h"

namespace billiardgl {

Point3 aimDirectionOnTable(float yaw);
Point3 shotVelocityFromAim(float yaw, float power);
CueImpactInput cueImpactFromShotControls(float yaw, float shotPower, const PhysicsProfile& profile);
Point3 cueLineStartFromAim(float yaw);
Point3 cueLineEndFromAim(float yaw, float length);
Point3 cueStickPositionFromAim(const Point3& cueBallPosition, float yaw, float shotPower);
float cueStickRotationDegreesFromAim(float yaw);

}  // namespace billiardgl
