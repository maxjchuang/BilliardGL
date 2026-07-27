#include "surface_motion.h"

#include <cmath>
#include <cstdlib>
#include <iostream>
#include <string>

namespace {

void expect(bool condition, const char* message)
{
    if (!condition) {
        std::cerr << message << '\n';
        std::exit(1);
    }
}

bool close(float first, float second, float epsilon = 0.0001f)
{
    return std::fabs(first - second) <= epsilon;
}

billiardgl::BallState pureRollingBall(float speed, float radius)
{
    billiardgl::BallState ball;
    ball.velocity.x = speed;
    ball.speed = std::fabs(speed);
    ball.angularVelocity.z = -speed / radius;
    ball.motionState = billiardgl::BallMotionState::Rolling;
    return ball;
}

double totalEnergy(
    const billiardgl::BallState& ball,
    const billiardgl::BallProperties& properties)
{
    const double speedMetersSquared =
        (ball.velocity.x * ball.velocity.x +
         ball.velocity.y * ball.velocity.y +
         ball.velocity.z * ball.velocity.z) / 10000.0;
    return 0.5 * properties.massKg * speedMetersSquared +
        billiardgl::rotationalKineticEnergyJ(ball, properties);
}

}  // namespace

int main()
{
    const billiardgl::PhysicsProfile profile =
        billiardgl::defaultChinesePoolPhysicsProfile();
    billiardgl::BallState ball;
    expect(billiardgl::classifySurfaceMotion(
        ball, profile.ball, profile.surface) ==
        billiardgl::BallMotionState::Stationary,
        "a reset ball is stationary");

    ball.velocity.x = 20.0f;
    ball.speed = 20.0f;
    expect(billiardgl::classifySurfaceMotion(
        ball, profile.ball, profile.surface) ==
        billiardgl::BallMotionState::Sliding,
        "a moving zero-spin ball is sliding");
    expect(close(billiardgl::surfaceContactSlipVelocity(
        ball, profile.ball.radiusCm).x, 20.0f),
        "zero-spin contact slip equals center velocity");

    ball.angularVelocity.z = -20.0f / profile.ball.radiusCm;
    expect(billiardgl::classifySurfaceMotion(
        ball, profile.ball, profile.surface) ==
        billiardgl::BallMotionState::Rolling,
        "roll-coupled angular velocity is rolling");
    const billiardgl::Point3 slip = billiardgl::surfaceContactSlipVelocity(
        ball, profile.ball.radiusCm);
    expect(close(slip.x, 0.0f) && close(slip.z, 0.0f),
        "pure rolling has zero contact slip");
    expect(std::string(billiardgl::ballMotionStateName(
        billiardgl::BallMotionState::Rolling)) == "rolling",
        "motion state has a stable protocol name");

    billiardgl::SurfaceProperties rollingSurface = profile.surface;
    rollingSurface.rollingResistanceAccelerationCmS2 = 12.5f;
    ball = pureRollingBall(20.0f, profile.ball.radiusCm);
    const billiardgl::SurfaceMotionStep rollingStep =
        billiardgl::advanceSurfaceMotion(
            ball, 0.1f, profile.ball, rollingSurface);
    expect(close(ball.position.x, 1.9375f, 0.00001f),
        "rolling position uses exact constant-acceleration integration");
    expect(close(ball.velocity.x, 18.75f, 0.00001f),
        "rolling speed decreases by resistance times time");
    expect(close(ball.angularVelocity.z,
        -18.75f / profile.ball.radiusCm, 0.00001f),
        "rolling constraint remains exact");
    expect(rollingStep.after == billiardgl::BallMotionState::Rolling,
        "a moving pure-roll segment remains rolling");

    billiardgl::SurfaceProperties zeroResistance = rollingSurface;
    zeroResistance.rollingResistanceAccelerationCmS2 = 0.0f;
    billiardgl::BallProperties freeRollingBallProperties = profile.ball;
    freeRollingBallProperties.radiusCm = 3.05f;
    ball = pureRollingBall(80.0f, freeRollingBallProperties.radiusCm);
    const float initialVelocity = ball.velocity.x;
    const float initialAngularVelocity = ball.angularVelocity.z;
    const double initialFreeRollingEnergy = totalEnergy(
        ball, freeRollingBallProperties);
    billiardgl::advanceSurfaceMotion(
        ball, 0.1f, freeRollingBallProperties, zeroResistance);
    expect(ball.velocity.x == initialVelocity &&
        ball.angularVelocity.z == initialAngularVelocity &&
        totalEnergy(ball, freeRollingBallProperties) ==
            initialFreeRollingEnergy,
        "zero rolling resistance preserves velocity and energy exactly");

    ball = pureRollingBall(0.5f, profile.ball.radiusCm);
    ball.angularVelocity.y = 3.0f;
    const billiardgl::SurfaceMotionStep stopStep =
        billiardgl::advanceSurfaceMotion(
            ball, 0.1f, profile.ball, rollingSurface);
    expect(close(ball.position.x, 0.01f, 0.00001f),
        "a slow rolling ball travels only to its analytic stop point");
    expect(close(ball.velocity.x, 0.0f) && close(ball.velocity.z, 0.0f),
        "a stopped rolling ball never reverses");
    expect(close(ball.angularVelocity.x, 0.0f) &&
        close(ball.angularVelocity.z, 0.0f),
        "horizontal roll-coupled angular velocity stops with translation");
    expect(close(ball.angularVelocity.y, 3.0f),
        "rolling resistance does not consume torsional sidespin");
    expect(stopStep.after == billiardgl::BallMotionState::Stationary,
        "the completed rolling stop is stationary");

    billiardgl::SurfaceProperties torsionalSurface = rollingSurface;
    torsionalSurface.torsionalSpinDecelerationRadS2 = 12.0f;
    billiardgl::BallState spinning;
    spinning.angularVelocity.y = 3.0f;
    spinning.rotationAxis = billiardgl::Point3{1.0f, 0.0f, 0.0f};
    spinning.rotationAngle = 90.0f;
    const billiardgl::SurfaceMotionStep spinningStep =
        billiardgl::advanceSurfaceMotion(
            spinning, 0.1f, profile.ball, torsionalSurface);
    expect(close(spinning.angularVelocity.y, 1.8f) &&
        close(spinningStep.angularAccelerationRadS2.y, -12.0f),
        "interaction torsional friction decays vertical-axis spin");
    expect(std::fabs(spinning.rotationAxis.x) > 0.1f &&
        std::fabs(spinning.rotationAxis.y) > 0.1f,
        "orientation composition preserves history when the spin axis changes");
    billiardgl::advanceSurfaceMotion(
        spinning, 0.2f, profile.ball, torsionalSurface);
    expect(spinning.angularVelocity.y == 0.0f,
        "torsional spin reaches exact zero without reversing");

    billiardgl::BallState residual = pureRollingBall(
        profile.surface.slipSpeedEpsilonCmS * 0.25f,
        profile.ball.radiusCm);
    billiardgl::advanceSurfaceMotion(
        residual, 0.1f, profile.ball, rollingSurface);
    expect(residual.motionState == billiardgl::BallMotionState::Stationary &&
        residual.speed == 0.0f && residual.velocity.x == 0.0f &&
        residual.velocity.z == 0.0f &&
        residual.angularVelocity.x == 0.0f &&
        residual.angularVelocity.z == 0.0f,
        "sub-threshold stationary state is normalized to exact horizontal rest");

    billiardgl::BallState oneStep =
        pureRollingBall(20.0f, profile.ball.radiusCm);
    billiardgl::BallState splitSteps = oneStep;
    billiardgl::advanceSurfaceMotion(
        oneStep, 0.1f, profile.ball, rollingSurface);
    for (int index = 0; index < 10; ++index) {
        billiardgl::advanceSurfaceMotion(
            splitSteps, 0.01f, profile.ball, rollingSurface);
    }
    expect(close(oneStep.position.x, splitSteps.position.x, 0.00001f) &&
        close(oneStep.velocity.x, splitSteps.velocity.x, 0.00001f) &&
        close(oneStep.angularVelocity.z, splitSteps.angularVelocity.z, 0.00001f),
        "rolling integration is invariant to an exact split of the time step");

    billiardgl::BallState legacySliding;
    legacySliding.velocity.x = 5.0f;
    legacySliding.speed = 5.0f;
    legacySliding.motionState = billiardgl::BallMotionState::Sliding;
    const billiardgl::SurfaceMotionStep legacyStop =
        billiardgl::advanceSurfaceMotion(
            legacySliding, 2.0f, profile.ball, profile.surface);
    expect(close(legacySliding.position.x, 3.125f, 0.00001f) &&
        legacySliding.velocity.x == 0.0f && legacySliding.speed == 0.0f &&
        legacyStop.after == billiardgl::BallMotionState::Stationary,
        "legacy zero-spin motion consumes configured deceleration and stops");

    billiardgl::SurfaceProperties slidingSurface = rollingSurface;
    slidingSurface.slidingFrictionCoefficient = 0.20f;
    ball = billiardgl::BallState{};
    ball.velocity.x = 100.0f;
    ball.speed = 100.0f;
    ball.motionState = billiardgl::BallMotionState::Sliding;
    const double initialSlidingEnergy = totalEnergy(ball, profile.ball);
    const billiardgl::SurfaceMotionStep slidingStep =
        billiardgl::advanceSurfaceMotion(
            ball, 0.1f, profile.ball, slidingSurface);
    const float slidingAcceleration =
        -0.20f * billiardgl::kStandardGravityCmS2;
    const float angularAcceleration =
        2.5f * slidingAcceleration / profile.ball.radiusCm;
    expect(close(slidingStep.frictionAccelerationCmS2.x,
        slidingAcceleration, 0.001f),
        "sliding friction uses mu times standard gravity");
    expect(close(slidingStep.angularAccelerationRadS2.z,
        angularAcceleration, 0.001f),
        "sliding friction couples to sphere angular acceleration");
    expect(slidingStep.after == billiardgl::BallMotionState::Sliding,
        "a segment before analytic transition remains sliding");
    expect(totalEnergy(ball, profile.ball) < initialSlidingEnergy,
        "sliding friction decreases total kinetic energy");

    const float transition = 100.0f /
        (3.5f * 0.20f * billiardgl::kStandardGravityCmS2);
    billiardgl::BallState atTransition;
    atTransition.velocity.x = 100.0f;
    atTransition.speed = 100.0f;
    atTransition.motionState = billiardgl::BallMotionState::Sliding;
    const billiardgl::SurfaceMotionStep transitionStep =
        billiardgl::advanceSurfaceMotion(
            atTransition, transition, profile.ball, slidingSurface);
    expect(transitionStep.after == billiardgl::BallMotionState::Rolling,
        "analytic contact-slip exhaustion transitions to rolling");
    expect(close(transitionStep.transitionTimeSeconds, transition, 0.00001f),
        "transition timestamp is recorded inside the tick");
    expect(close(billiardgl::surfaceContactSlipVelocity(
        atTransition, profile.ball.radiusCm).x, 0.0f, 0.0001f),
        "contact slip is exactly zero at rolling transition");

    billiardgl::BallState crossing;
    crossing.velocity.x = 100.0f;
    crossing.speed = 100.0f;
    crossing.motionState = billiardgl::BallMotionState::Sliding;
    billiardgl::advanceSurfaceMotion(
        crossing, transition + 0.1f, profile.ball, slidingSurface);
    billiardgl::BallState splitCrossing;
    splitCrossing.velocity.x = 100.0f;
    splitCrossing.speed = 100.0f;
    splitCrossing.motionState = billiardgl::BallMotionState::Sliding;
    billiardgl::advanceSurfaceMotion(
        splitCrossing, transition, profile.ball, slidingSurface);
    billiardgl::advanceSurfaceMotion(
        splitCrossing, 0.1f, profile.ball, slidingSurface);
    expect(close(crossing.position.x, splitCrossing.position.x, 0.0001f) &&
        close(crossing.velocity.x, splitCrossing.velocity.x, 0.0001f) &&
        close(crossing.angularVelocity.z,
            splitCrossing.angularVelocity.z, 0.0001f),
        "a sliding-to-rolling event is invariant to splitting at the event");

    billiardgl::BallState mirrored;
    mirrored.velocity.x = -100.0f;
    mirrored.speed = 100.0f;
    mirrored.motionState = billiardgl::BallMotionState::Sliding;
    billiardgl::advanceSurfaceMotion(
        mirrored, 0.1f, profile.ball, slidingSurface);
    expect(close(mirrored.position.x, -ball.position.x, 0.0001f) &&
        close(mirrored.velocity.x, -ball.velocity.x, 0.0001f) &&
        close(mirrored.angularVelocity.z, -ball.angularVelocity.z, 0.0001f),
        "sliding dynamics have negative-velocity mirror symmetry");

    billiardgl::BallState zShot;
    zShot.velocity.z = 100.0f;
    zShot.speed = 100.0f;
    zShot.motionState = billiardgl::BallMotionState::Sliding;
    const billiardgl::SurfaceMotionStep zStep =
        billiardgl::advanceSurfaceMotion(
            zShot, 0.1f, profile.ball, slidingSurface);
    expect(close(zStep.frictionAccelerationCmS2.z,
        slidingAcceleration, 0.001f) &&
        zStep.angularAccelerationRadS2.x > 0.0f,
        "z-axis sliding uses the same coupled dynamics");

    billiardgl::BallState topspin;
    topspin.velocity.x = 20.0f;
    topspin.speed = 20.0f;
    topspin.angularVelocity.z = -40.0f / profile.ball.radiusCm;
    topspin.motionState = billiardgl::BallMotionState::Sliding;
    const double topspinEnergy = totalEnergy(topspin, profile.ball);
    billiardgl::advanceSurfaceMotion(
        topspin, 0.01f, profile.ball, slidingSurface);
    expect(topspin.velocity.x > 20.0f,
        "topspin can accelerate the center toward pure rolling");
    expect(totalEnergy(topspin, profile.ball) < topspinEnergy,
        "topspin transfer still dissipates total kinetic energy");
    return 0;
}
