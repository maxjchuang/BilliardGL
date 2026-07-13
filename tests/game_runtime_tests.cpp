#include "game_runtime.h"

#include <cmath>
#include <cstdlib>
#include <iostream>

namespace {

void expect(bool value, const char* message)
{
    if (!value) {
        std::cerr << message << '\n';
        std::exit(1);
    }
}

bool closeEnough(float lhs, float rhs)
{
    return std::fabs(lhs - rhs) < 0.0001f;
}

void applyShot(billiardgl::GameRuntime& runtime)
{
    billiardgl::GameAction aim;
    aim.type = billiardgl::ActionType::SetAimYaw;
    aim.first = 0.0f;
    expect(runtime.dispatch(aim).ok, "setting aim should succeed");

    billiardgl::GameAction power;
    power.type = billiardgl::ActionType::SetShotPower;
    power.first = 40.0f;
    expect(runtime.dispatch(power).ok, "setting power should succeed");

    billiardgl::GameAction shoot;
    shoot.type = billiardgl::ActionType::Shoot;
    expect(runtime.dispatch(shoot).ok, "shooting should succeed");
}

}  // namespace

int main()
{
    billiardgl::GameRuntime first;
    billiardgl::GameRuntime second;

    expect(first.tick() == 0, "new runtime should start at tick zero");
    expect(first.state().balls[0].position.z < 0.0f, "reset should install the cue ball");

    applyShot(first);
    applyShot(second);
    expect(first.state().balls[0].velocity.x > 39.9f, "shot should use normal aim logic");

    expect(first.step(5).ok, "stepping should succeed");
    expect(second.step(5).ok, "second stepping should succeed");
    expect(first.tick() == 5, "step should advance the exact tick count");
    expect(closeEnough(first.state().balls[0].position.x, second.state().balls[0].position.x),
        "same command sequence should produce the same position");
    return 0;
}
