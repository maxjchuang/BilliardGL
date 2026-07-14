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
    return 0;
}
