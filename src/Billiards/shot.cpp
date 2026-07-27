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

CueImpactInput cueImpactFromShotControls(float yaw, float shotPower, const PhysicsProfile& profile)
{
    const Point3 direction = aimDirectionOnTable(yaw);
    CueImpactInput input;
    input.cueBallIndex = 0;
    input.cueSpeedCmS = static_cast<double>(shotPower) * profile.cue.cueSpeedPerPowerUnitCmS;
    input.cueMassKg = profile.cue.effectiveMassKg;
    input.direction = {direction.x, 0.0, direction.z};
    input.elevationDegrees = 0.0;
    input.tipOffsetCm = {0.0, 0.0};
    input.tipOffsetRadius = {0.0, 0.0};
    input.chalkState = "CHALKED";
    return input;
}

Point3 cueLineEndFromAim(float yaw, float length)
{
    const Point3 direction = aimDirectionOnTable(yaw);
    return Point3{direction.x * length, 0.0f, direction.z * length};
}

Point3 cueLineStartFromAim(float yaw)
{
    const Point3 direction = aimDirectionOnTable(yaw);
    const float start = kBallRadius + kCueTipRestGapCm;
    return Point3{direction.x * start, 0.0f, direction.z * start};
}

Point3 cueStickPositionFromAim(const Point3& cueBallPosition, float yaw,
    float shotPower, float cueModelTipOffsetCm)
{
    const Point3 direction = aimDirectionOnTable(yaw);
    // cue.obj's origin lies in front of its physical tip. Position the asset
    // from the measured tip endpoint so the visible clearance, rather than the
    // arbitrary model origin, remains stable as ball dimensions change.
    const float tipBackoff = kBallRadius + kCueTipRestGapCm +
        shotPower * kCuePowerBackoffCmPerUnit;
    const float modelOriginBackoff = tipBackoff - cueModelTipOffsetCm;
    return Point3{
        cueBallPosition.x - direction.x * modelOriginBackoff,
        cueBallPosition.y,
        cueBallPosition.z - direction.z * modelOriginBackoff};
}

float cueStickRotationDegreesFromAim(float yaw)
{
    return 270.5f - yaw * 180.0f / kPi;
}

}  // namespace billiardgl
