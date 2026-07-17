#include "physics.h"
#include "physics_profile.h"
#include "game_runtime.h"
#include "surface_motion.h"

#include <chrono>
#include <cmath>
#include <cstdlib>
#include <iostream>

namespace {

void expect(bool condition, const char* message)
{
    if (!condition) {
        std::cerr << message << '\n';
        std::exit(EXIT_FAILURE);
    }
}

bool close(float first, float second, float tolerance = 0.001f)
{
    return std::fabs(first - second) <= tolerance;
}

billiardgl::GameState isolatedCueBall()
{
    billiardgl::GameState state;
    billiardgl::initializeBalls(state);
    for (int index = 1; index < billiardgl::kBallCount; ++index) {
        state.balls[index].pocketed = true;
    }
    state.balls[0].position = billiardgl::Point3{
        0.0f, billiardgl::kTableHeight + billiardgl::kBallRadius, 30.0f};
    return state;
}

billiardgl::GameState simulate(float stepSeconds, float durationSeconds,
    const billiardgl::Point3& position, const billiardgl::Point3& velocity)
{
    billiardgl::PhysicsProfile profile =
        billiardgl::interactiveChinesePoolPhysicsProfile();
    profile.solver.timeStepSeconds = stepSeconds;
    billiardgl::GameState state = isolatedCueBall();
    state.balls[0].position = position;
    billiardgl::setBallVelocity(state.balls[0],
        velocity.x, velocity.y, velocity.z);
    state.balls[0].speed = std::hypot(velocity.x, velocity.z);
    state.ballsMoving = state.balls[0].speed > 0.0f;
    const int ticks = static_cast<int>(
        std::lround(durationSeconds / stepSeconds));
    for (int tick = 0; tick < ticks; ++tick) {
        const billiardgl::PhysicsStepTelemetry result =
            billiardgl::updatePhysics(state, stepSeconds, profile);
        expect(result.stepStatus == billiardgl::PhysicsStepStatus::Succeeded,
            "interactive convergence simulation must remain valid");
    }
    return state;
}

}  // namespace

int main()
{
    const billiardgl::Point3 tableCenter{
        0.0f, billiardgl::kTableHeight + billiardgl::kBallRadius, 30.0f};
    const billiardgl::GameState free60 = simulate(
        1.0f / 60.0f, 1.0f, tableCenter, {50.0f, 0.0f, 0.0f});
    const billiardgl::GameState free120 = simulate(
        1.0f / 120.0f, 1.0f, tableCenter, {50.0f, 0.0f, 0.0f});
    const billiardgl::GameState free240 = simulate(
        1.0f / 240.0f, 1.0f, tableCenter, {50.0f, 0.0f, 0.0f});
    expect(close(free120.balls[0].position.x, free240.balls[0].position.x) &&
        close(free120.balls[0].speed, free240.balls[0].speed),
        "120 Hz free motion must converge to the 240 Hz reference");
    expect(close(free60.balls[0].position.x, free120.balls[0].position.x) &&
        close(free60.balls[0].speed, free120.balls[0].speed),
        "free-motion integration must be stable across common fixed steps");

    const billiardgl::Point3 railStart{
        35.0f, billiardgl::kTableHeight + billiardgl::kBallRadius, 30.0f};
    const billiardgl::GameState rail120 = simulate(
        1.0f / 120.0f, 0.5f, railStart, {100.0f, 0.0f, 0.0f});
    const billiardgl::GameState rail240 = simulate(
        1.0f / 240.0f, 0.5f, railStart, {100.0f, 0.0f, 0.0f});
    expect(rail120.balls[0].velocity.x < 0.0f &&
        rail240.balls[0].velocity.x < 0.0f,
        "rail convergence case must include a real rebound");
    expect(close(rail120.balls[0].position.x, rail240.balls[0].position.x,
            0.01f) &&
        close(rail120.balls[0].velocity.x, rail240.balls[0].velocity.x,
            0.01f),
        "120 Hz rail response must converge to the 240 Hz reference");

    const billiardgl::GameState stopped = simulate(
        1.0f / 120.0f, 1.0f, tableCenter, {2.0f, 0.0f, 0.0f});
    expect(stopped.balls[0].motionState ==
            billiardgl::BallMotionState::Stationary &&
        close(stopped.balls[0].speed, 0.0f) && !stopped.ballsMoving,
        "low-speed interactive motion must settle completely");

    billiardgl::GameRuntime breakRuntime;
    const billiardgl::GameState initialRack = breakRuntime.state();
    expect(breakRuntime.replaceStateForScenario(initialRack,
            billiardgl::interactiveChinesePoolPhysicsProfile()).ok,
        "interactive full-game runtime must select the 120 Hz profile");
    billiardgl::GameAction aim;
    aim.type = billiardgl::ActionType::SetAimYaw;
    aim.first = billiardgl::kPi / 2.0f;
    expect(breakRuntime.dispatch(aim).ok,
        "interactive break aim must be accepted");
    billiardgl::GameAction power;
    power.type = billiardgl::ActionType::SetShotPower;
    power.first = 80.0f;
    expect(breakRuntime.dispatch(power).ok,
        "interactive break power must be accepted");
    billiardgl::GameAction shoot;
    shoot.type = billiardgl::ActionType::Shoot;
    expect(breakRuntime.dispatch(shoot).ok &&
        breakRuntime.state().ballsMoving,
        "interactive break must start ball motion");

    const float cameraAngleBefore = breakRuntime.state().camera.angleX;
    billiardgl::GameAction orbit;
    orbit.type = billiardgl::ActionType::OrbitCamera;
    orbit.first = 0.2f;
    orbit.second = 0.1f;
    expect(breakRuntime.dispatch(orbit).ok &&
        breakRuntime.state().camera.angleX != cameraAngleBefore,
        "camera control must remain responsive during interactive motion");

    const auto started = std::chrono::steady_clock::now();
    int breakTicks = 0;
    while (breakRuntime.state().ballsMoving && breakTicks < 20000) {
        expect(breakRuntime.step(1).ok,
            "every 120 Hz break tick must converge without physics failure");
        ++breakTicks;
    }
    const double executionSeconds = std::chrono::duration<double>(
        std::chrono::steady_clock::now() - started).count();
    expect(!breakRuntime.state().ballsMoving && breakTicks < 20000,
        "interactive break must reach a complete stop");
    expect(executionSeconds < 5.0,
        "120 Hz interactive break must stay within the CPU regression budget");
    return EXIT_SUCCESS;
}
