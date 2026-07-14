#include "surface_motion.h"

#include "physics.h"

#include <cmath>

namespace billiardgl {
namespace {

float horizontalLength(const Point3& value)
{
    return std::sqrt(value.x * value.x + value.z * value.z);
}

}  // namespace

const char* ballMotionStateName(BallMotionState state)
{
    switch (state) {
    case BallMotionState::Stationary:
        return "stationary";
    case BallMotionState::Sliding:
        return "sliding";
    case BallMotionState::Rolling:
        return "rolling";
    }
    return "stationary";
}

Point3 surfaceContactSlipVelocity(const BallState& ball, float radiusCm)
{
    return Point3{
        ball.velocity.x + radiusCm * ball.angularVelocity.z,
        0.0f,
        ball.velocity.z - radiusCm * ball.angularVelocity.x};
}

double rotationalKineticEnergyJ(
    const BallState& ball, const BallProperties& ballProperties)
{
    const double radiusMeters = ballProperties.radiusCm / 100.0;
    const double momentOfInertia =
        0.4 * ballProperties.massKg * radiusMeters * radiusMeters;
    const double omegaSquared =
        ball.angularVelocity.x * ball.angularVelocity.x +
        ball.angularVelocity.y * ball.angularVelocity.y +
        ball.angularVelocity.z * ball.angularVelocity.z;
    return 0.5 * momentOfInertia * omegaSquared;
}

BallMotionState classifySurfaceMotion(
    const BallState& ball, const BallProperties& ballProperties,
    const SurfaceProperties& surface)
{
    if (ball.pocketed) {
        return BallMotionState::Stationary;
    }
    const double speedMetersPerSecondSquared =
        (ball.velocity.x * ball.velocity.x +
         ball.velocity.y * ball.velocity.y +
         ball.velocity.z * ball.velocity.z) / 10000.0;
    const double totalEnergy =
        0.5 * ballProperties.massKg * speedMetersPerSecondSquared +
        rotationalKineticEnergyJ(ball, ballProperties);
    if (totalEnergy <= surface.stopEnergyThresholdJ) {
        return BallMotionState::Stationary;
    }
    const Point3 slip = surfaceContactSlipVelocity(
        ball, ballProperties.radiusCm);
    return horizontalLength(slip) <= surface.slipSpeedEpsilonCmS
        ? BallMotionState::Rolling
        : BallMotionState::Sliding;
}

SurfaceMotionStep advanceSurfaceMotion(
    BallState& ball, float deltaSeconds, const BallProperties& ballProperties,
    const SurfaceProperties& surface)
{
    SurfaceMotionStep step;
    step.before = classifySurfaceMotion(ball, ballProperties, surface);
    step.initialSlipSpeedCmS = horizontalLength(
        surfaceContactSlipVelocity(ball, ballProperties.radiusCm));
    const Point3 initialVelocity = ball.velocity;
    applyFrictionAndMove(
        ball, deltaSeconds, -surface.legacyFrictionAccelerationCmS2);
    if (deltaSeconds > 0.0f) {
        step.frictionAccelerationCmS2 = Point3{
            (ball.velocity.x - initialVelocity.x) / deltaSeconds,
            (ball.velocity.y - initialVelocity.y) / deltaSeconds,
            (ball.velocity.z - initialVelocity.z) / deltaSeconds};
    }
    ball.motionState = classifySurfaceMotion(ball, ballProperties, surface);
    step.after = ball.motionState;
    step.finalSlipSpeedCmS = horizontalLength(
        surfaceContactSlipVelocity(ball, ballProperties.radiusCm));
    return step;
}

}  // namespace billiardgl
