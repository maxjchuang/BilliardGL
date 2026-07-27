#pragma once

#include "cue_impact.h"
#include "game_state.h"
#include "physics_profile.h"

namespace billiardgl {

constexpr float kCueTipRestGapCm = 1.0f;
constexpr float kCuePowerBackoffCmPerUnit = 0.025f;

Point3 aimDirectionOnTable(float yaw);
Point3 shotVelocityFromAim(float yaw, float power);
CueImpactInput cueImpactFromShotControls(float yaw, float shotPower, const PhysicsProfile& profile);
Point3 cueLineStartFromAim(float yaw);
Point3 cueLineEndFromAim(float yaw, float length);
Point3 cueStickPositionFromAim(const Point3& cueBallPosition, float yaw,
    float shotPower, float cueModelTipOffsetCm);
float cueStickRotationDegreesFromAim(float yaw);

}  // namespace billiardgl
