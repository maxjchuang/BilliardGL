#include "shot.h"

#include <algorithm>
#include <cmath>

namespace billiardgl {
namespace {

constexpr double kInteractiveReferencePower = 60.0;
constexpr double kInteractiveMaximumPower = 200.0;
constexpr double kInteractiveMaximumBallSpeedCmS = 1000.0;
constexpr double kInteractiveBreakSpinStartPower = 100.0;
constexpr double kInteractiveMaximumTopOffsetRadius = 0.40;

bool usesInteractiveBreakMapping(const PhysicsProfile& profile)
{
    return profile.id == "chinese_pool_interactive_120hz_v5";
}

double interactiveTopOffsetRadius(double shotPower)
{
    if (shotPower <= kInteractiveBreakSpinStartPower) return 0.0;
    const double fraction = (shotPower - kInteractiveBreakSpinStartPower) /
        (kInteractiveMaximumPower - kInteractiveBreakSpinStartPower);
    return kInteractiveMaximumTopOffsetRadius *
        std::max(0.0, std::min(1.0, fraction));
}

double cueSpeedForTargetBallSpeed(double ballSpeedCmS, double offsetRadius,
    const PhysicsProfile& profile)
{
    const double inverseEffectiveMass =
        1.0 / profile.cue.effectiveMassKg +
        1.0 / profile.ball.massKg +
        offsetRadius * offsetRadius /
            (profile.ball.inertiaFactor * profile.ball.massKg);
    const double ballToCueSpeedRatio =
        (1.0 + profile.cue.normalRestitution) /
        (profile.ball.massKg * inverseEffectiveMass);
    return ballSpeedCmS / ballToCueSpeedRatio;
}

double interactiveCueSpeedCmS(double shotPower,
    const PhysicsProfile& profile)
{
    const double linearSpeed =
        shotPower * profile.cue.cueSpeedPerPowerUnitCmS;
    if (shotPower <= kInteractiveReferencePower) return linearSpeed;

    // Continue from the ordinary-shot linear curve with the same first
    // derivative, then accelerate quadratically toward a 10 m/s maximum
    // cue-ball launch. This preserves low-speed finesse without making the
    // break range artificially weak.
    const double referenceSpeed = kInteractiveReferencePower *
        profile.cue.cueSpeedPerPowerUnitCmS;
    const double maximumCueSpeed = cueSpeedForTargetBallSpeed(
        kInteractiveMaximumBallSpeedCmS,
        kInteractiveMaximumTopOffsetRadius, profile);
    const double range =
        kInteractiveMaximumPower - kInteractiveReferencePower;
    const double delta = std::min(shotPower, kInteractiveMaximumPower) -
        kInteractiveReferencePower;
    const double curvature = (maximumCueSpeed - referenceSpeed -
        profile.cue.cueSpeedPerPowerUnitCmS * range) / (range * range);
    return referenceSpeed + profile.cue.cueSpeedPerPowerUnitCmS * delta +
        curvature * delta * delta;
}

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

CueImpactInput cueImpactFromShotControls(float yaw, float shotPower, const PhysicsProfile& profile)
{
    const Point3 direction = aimDirectionOnTable(yaw);
    CueImpactInput input;
    input.cueBallIndex = 0;
    input.cueSpeedCmS = usesInteractiveBreakMapping(profile)
        ? interactiveCueSpeedCmS(shotPower, profile)
        : static_cast<double>(shotPower) *
            profile.cue.cueSpeedPerPowerUnitCmS;
    input.cueMassKg = profile.cue.effectiveMassKg;
    input.direction = {direction.x, 0.0, direction.z};
    input.elevationDegrees = 0.0;
    const double topOffset = usesInteractiveBreakMapping(profile)
        ? interactiveTopOffsetRadius(shotPower) : 0.0;
    input.tipOffsetCm = {0.0,
        topOffset * static_cast<double>(profile.ball.radiusCm)};
    input.tipOffsetRadius = {0.0, topOffset};
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
