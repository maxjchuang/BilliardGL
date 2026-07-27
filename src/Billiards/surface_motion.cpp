#include "surface_motion.h"

#include <algorithm>
#include <cmath>

namespace billiardgl {
namespace {

float horizontalLength(const Point3& value)
{
    return std::sqrt(value.x * value.x + value.z * value.z);
}

struct Quaternion {
    float x = 0.0f;
    float y = 0.0f;
    float z = 0.0f;
    float w = 1.0f;
};

Quaternion multiplied(const Quaternion& first, const Quaternion& second)
{
    return Quaternion{
        first.w * second.x + first.x * second.w +
            first.y * second.z - first.z * second.y,
        first.w * second.y - first.x * second.z +
            first.y * second.w + first.z * second.x,
        first.w * second.z + first.x * second.y -
            first.y * second.x + first.z * second.w,
        first.w * second.w - first.x * second.x -
            first.y * second.y - first.z * second.z};
}

Quaternion orientation(const BallState& ball)
{
    const float axisLength = std::sqrt(
        ball.rotationAxis.x * ball.rotationAxis.x +
        ball.rotationAxis.y * ball.rotationAxis.y +
        ball.rotationAxis.z * ball.rotationAxis.z);
    if (axisLength <= 0.0f || ball.rotationAngle == 0.0f) {
        return Quaternion{};
    }
    const float halfAngle = ball.rotationAngle * kPi / 360.0f;
    const float scale = std::sin(halfAngle) / axisLength;
    return Quaternion{ball.rotationAxis.x * scale,
        ball.rotationAxis.y * scale, ball.rotationAxis.z * scale,
        std::cos(halfAngle)};
}

void advanceOrientation(BallState& ball, const Point3& rotationVectorRad)
{
    const float angle = std::sqrt(
        rotationVectorRad.x * rotationVectorRad.x +
        rotationVectorRad.y * rotationVectorRad.y +
        rotationVectorRad.z * rotationVectorRad.z);
    if (angle <= 0.0f) return;
    const float halfAngle = 0.5f * angle;
    const float scale = std::sin(halfAngle) / angle;
    const Quaternion delta{rotationVectorRad.x * scale,
        rotationVectorRad.y * scale, rotationVectorRad.z * scale,
        std::cos(halfAngle)};
    Quaternion updated = multiplied(delta, orientation(ball));
    const float length = std::sqrt(updated.x * updated.x +
        updated.y * updated.y + updated.z * updated.z +
        updated.w * updated.w);
    if (length <= 0.0f) return;
    updated.x /= length;
    updated.y /= length;
    updated.z /= length;
    updated.w /= length;
    const float vectorLength = std::sqrt(updated.x * updated.x +
        updated.y * updated.y + updated.z * updated.z);
    if (vectorLength <= 0.000001f) {
        ball.rotationAxis = Point3{};
        ball.rotationAngle = 0.0f;
        return;
    }
    ball.rotationAxis = Point3{updated.x / vectorLength,
        updated.y / vectorLength, updated.z / vectorLength};
    ball.rotationAngle = 2.0f * std::atan2(vectorLength, updated.w) *
        180.0f / kPi;
}

float advanceTorsionalSpin(BallState& ball, float deltaSeconds,
    const SurfaceProperties& surface, SurfaceMotionStep& step)
{
    const float initial = ball.angularVelocity.y;
    if (deltaSeconds <= 0.0f || initial == 0.0f) return 0.0f;
    const float deceleration = surface.torsionalSpinDecelerationRadS2;
    if (deceleration <= 0.0f) return initial * deltaSeconds;
    const float direction = initial < 0.0f ? -1.0f : 1.0f;
    const float magnitude = std::fabs(initial);
    const float duration = std::min(deltaSeconds, magnitude / deceleration);
    const float integratedAngle = direction *
        (magnitude * duration - 0.5f * deceleration * duration * duration);
    const float availableDecay = deceleration * deltaSeconds;
    const float finalMagnitude = magnitude <= availableDecay + 0.000001f
        ? 0.0f : magnitude - availableDecay;
    ball.angularVelocity.y = direction * finalMagnitude;
    step.angularAccelerationRadS2.y =
        (ball.angularVelocity.y - initial) / deltaSeconds;
    return integratedAngle;
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
    if (resistance <= 0.0f) {
        // A zero-resistance step is exact free rolling.  Re-normalizing the
        // velocity and re-projecting angular velocity through v/r can add
        // float-rounding energy even though no force acted on the ball.
        ball.speed = speed;
        ball.motionState = BallMotionState::Rolling;
        return segment;
    }
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
    const float torsionalRotation = advanceTorsionalSpin(
        ball, deltaSeconds, surface, step);
    step.after = ball.motionState;
    step.finalSlipSpeedCmS = horizontalLength(
        surfaceContactSlipVelocity(ball, ballProperties.radiusCm));
    advanceOrientation(ball, Point3{
        ball.angularVelocity.x * deltaSeconds,
        torsionalRotation,
        ball.angularVelocity.z * deltaSeconds});
    return step;
}

}  // namespace billiardgl
