#include "surface_motion.h"

#include "physics.h"

#include <algorithm>
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
    const float horizontalSpeed = horizontalLength(ball.velocity);
    const float coupledAngularSpeedCmS = ballProperties.radiusCm * std::sqrt(
        ball.angularVelocity.x * ball.angularVelocity.x +
        ball.angularVelocity.z * ball.angularVelocity.z);
    if (horizontalSpeed <= surface.slipSpeedEpsilonCmS &&
        coupledAngularSpeedCmS <= surface.slipSpeedEpsilonCmS) {
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
    const Point3 initialAngularVelocity = ball.angularVelocity;
    if (step.before == BallMotionState::Rolling && deltaSeconds > 0.0f) {
        const float speed = horizontalLength(ball.velocity);
        const Point3 direction = speed > 0.0f
            ? Point3{ball.velocity.x / speed, 0.0f, ball.velocity.z / speed}
            : Point3{};
        const float resistance = surface.rollingResistanceAccelerationCmS2;
        const float segment = resistance > 0.0f
            ? std::min(deltaSeconds, speed / resistance)
            : deltaSeconds;
        const float distance = resistance > 0.0f
            ? speed * segment - 0.5f * resistance * segment * segment
            : speed * segment;
        ball.position.x += direction.x * distance;
        ball.position.z += direction.z * distance;
        const float finalSpeed = resistance > 0.0f
            ? std::max(0.0f, speed - resistance * segment)
            : speed;
        ball.velocity.x = direction.x * finalSpeed;
        ball.velocity.z = direction.z * finalSpeed;
        ball.speed = finalSpeed;
        ball.angularVelocity.x = ball.velocity.z / ballProperties.radiusCm;
        ball.angularVelocity.z = -ball.velocity.x / ballProperties.radiusCm;
        ball.rotationAxis.x = -direction.z;
        ball.rotationAxis.z = direction.x;
        ball.rotationAngle +=
            -180.0f * distance / (ballProperties.radiusCm * kPi);
        if (segment < deltaSeconds) {
            ball.motionState = BallMotionState::Stationary;
            step.transitionTimeSeconds = segment;
        } else {
            ball.motionState = BallMotionState::Rolling;
        }
    } else {
        applyFrictionAndMove(
            ball, deltaSeconds, -surface.legacyFrictionAccelerationCmS2);
        ball.motionState = classifySurfaceMotion(ball, ballProperties, surface);
    }
    if (deltaSeconds > 0.0f) {
        step.frictionAccelerationCmS2 = Point3{
            (ball.velocity.x - initialVelocity.x) / deltaSeconds,
            (ball.velocity.y - initialVelocity.y) / deltaSeconds,
            (ball.velocity.z - initialVelocity.z) / deltaSeconds};
        step.angularAccelerationRadS2 = Point3{
            (ball.angularVelocity.x - initialAngularVelocity.x) / deltaSeconds,
            (ball.angularVelocity.y - initialAngularVelocity.y) / deltaSeconds,
            (ball.angularVelocity.z - initialAngularVelocity.z) / deltaSeconds};
    }
    step.after = ball.motionState;
    step.finalSlipSpeedCmS = horizontalLength(
        surfaceContactSlipVelocity(ball, ballProperties.radiusCm));
    return step;
}

}  // namespace billiardgl
