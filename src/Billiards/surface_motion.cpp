#include "surface_motion.h"

#include <algorithm>
#include <cmath>

namespace billiardgl {
namespace {

float horizontalLength(const Point3& value)
{
    return std::sqrt(value.x * value.x + value.z * value.z);
}

float advanceRollingSegment(
    BallState& ball, float deltaSeconds, const BallProperties& ballProperties,
    const SurfaceProperties& surface)
{
    const float speed = horizontalLength(ball.velocity);
    if (deltaSeconds <= 0.0f || speed <= 0.0f) {
        ball.speed = 0.0f;
        ball.velocity.x = 0.0f;
        ball.velocity.z = 0.0f;
        ball.angularVelocity.x = 0.0f;
        ball.angularVelocity.z = 0.0f;
        ball.motionState = BallMotionState::Stationary;
        return 0.0f;
    }
    const Point3 direction{
        ball.velocity.x / speed, 0.0f, ball.velocity.z / speed};
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
    ball.motionState = segment < deltaSeconds
        ? BallMotionState::Stationary
        : BallMotionState::Rolling;
    return segment;
}

void advanceLegacySlidingSegment(BallState& ball, float deltaSeconds,
    const SurfaceProperties& surface, SurfaceMotionStep& step)
{
    const float speed = horizontalLength(ball.velocity);
    const float deceleration = surface.legacyFrictionAccelerationCmS2;
    if (speed <= 0.0f || deceleration <= 0.0f) {
        ball.position.x += ball.velocity.x * deltaSeconds;
        ball.position.z += ball.velocity.z * deltaSeconds;
        ball.motionState = BallMotionState::Sliding;
        return;
    }
    const Point3 direction{
        ball.velocity.x / speed, 0.0f, ball.velocity.z / speed};
    const float segment = std::min(deltaSeconds, speed / deceleration);
    const float distance = speed * segment -
        0.5f * deceleration * segment * segment;
    ball.position.x += direction.x * distance;
    ball.position.z += direction.z * distance;
    const float finalSpeed = std::max(
        0.0f, speed - deceleration * segment);
    ball.velocity.x = direction.x * finalSpeed;
    ball.velocity.z = direction.z * finalSpeed;
    ball.speed = finalSpeed;
    step.frictionAccelerationCmS2 = Point3{
        -direction.x * deceleration, 0.0f, -direction.z * deceleration};
    if (segment < deltaSeconds ||
        finalSpeed <= surface.slipSpeedEpsilonCmS) {
        ball.velocity.x = 0.0f;
        ball.velocity.z = 0.0f;
        ball.speed = 0.0f;
        ball.motionState = BallMotionState::Stationary;
        step.transitionTimeSeconds = segment;
    } else {
        ball.motionState = BallMotionState::Sliding;
    }
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
    if (step.before == BallMotionState::Rolling && deltaSeconds > 0.0f) {
        const float speed = horizontalLength(ball.velocity);
        const Point3 direction = speed > 0.0f
            ? Point3{ball.velocity.x / speed, 0.0f, ball.velocity.z / speed}
            : Point3{};
        const float resistance = surface.rollingResistanceAccelerationCmS2;
        const float segment = advanceRollingSegment(
            ball, deltaSeconds, ballProperties, surface);
        step.frictionAccelerationCmS2 = Point3{
            -direction.x * resistance, 0.0f, -direction.z * resistance};
        step.angularAccelerationRadS2 = Point3{
            step.frictionAccelerationCmS2.z / ballProperties.radiusCm,
            0.0f,
            -step.frictionAccelerationCmS2.x / ballProperties.radiusCm};
        if (segment < deltaSeconds) {
            step.transitionTimeSeconds = segment;
        }
    } else if (step.before == BallMotionState::Sliding && deltaSeconds > 0.0f) {
        const float coefficient = surface.slidingFrictionCoefficient;
        if (coefficient <= 0.0f || step.initialSlipSpeedCmS <= 0.0f) {
            advanceLegacySlidingSegment(ball, deltaSeconds, surface, step);
        } else {
            const Point3 slip = surfaceContactSlipVelocity(
                ball, ballProperties.radiusCm);
            const float magnitude = coefficient * kStandardGravityCmS2;
            const Point3 acceleration{
                -magnitude * slip.x / step.initialSlipSpeedCmS,
                0.0f,
                -magnitude * slip.z / step.initialSlipSpeedCmS};
            const Point3 angularAcceleration{
                -2.5f * acceleration.z / ballProperties.radiusCm,
                0.0f,
                2.5f * acceleration.x / ballProperties.radiusCm};
            const float transition = step.initialSlipSpeedCmS /
                (3.5f * magnitude);
            const float segment = std::min(deltaSeconds, transition);
            ball.position.x +=
                ball.velocity.x * segment +
                0.5f * acceleration.x * segment * segment;
            ball.position.z +=
                ball.velocity.z * segment +
                0.5f * acceleration.z * segment * segment;
            ball.velocity.x += acceleration.x * segment;
            ball.velocity.z += acceleration.z * segment;
            ball.angularVelocity.x += angularAcceleration.x * segment;
            ball.angularVelocity.z += angularAcceleration.z * segment;
            ball.speed = horizontalLength(ball.velocity);
            step.frictionAccelerationCmS2 = acceleration;
            step.angularAccelerationRadS2 = angularAcceleration;
            if (transition <= deltaSeconds) {
                ball.angularVelocity.x =
                    ball.velocity.z / ballProperties.radiusCm;
                ball.angularVelocity.z =
                    -ball.velocity.x / ballProperties.radiusCm;
                ball.motionState = BallMotionState::Rolling;
                step.transitionTimeSeconds = transition;
                const float remaining = deltaSeconds - transition;
                if (remaining > 0.0f) {
                    advanceRollingSegment(
                        ball, remaining, ballProperties, surface);
                }
            } else {
                ball.motionState = BallMotionState::Sliding;
            }
        }
    } else {
        ball.motionState = step.before;
        if (step.before == BallMotionState::Stationary) {
            ball.speed = 0.0f;
            ball.velocity.x = 0.0f;
            ball.velocity.z = 0.0f;
            ball.angularVelocity.x = 0.0f;
            ball.angularVelocity.z = 0.0f;
        } else {
            ball.speed = horizontalLength(ball.velocity);
        }
    }
    step.after = ball.motionState;
    step.finalSlipSpeedCmS = horizontalLength(
        surfaceContactSlipVelocity(ball, ballProperties.radiusCm));
    const float angularSpeed = std::sqrt(
        ball.angularVelocity.x * ball.angularVelocity.x +
        ball.angularVelocity.y * ball.angularVelocity.y +
        ball.angularVelocity.z * ball.angularVelocity.z);
    if (angularSpeed > 0.0f) {
        ball.rotationAxis.x = ball.angularVelocity.x / angularSpeed;
        ball.rotationAxis.y = ball.angularVelocity.y / angularSpeed;
        ball.rotationAxis.z = ball.angularVelocity.z / angularSpeed;
        ball.rotationAngle +=
            angularSpeed * deltaSeconds * 180.0f / kPi;
    }
    return step;
}

}  // namespace billiardgl
