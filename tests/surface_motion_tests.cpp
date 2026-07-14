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
    return 0;
}
