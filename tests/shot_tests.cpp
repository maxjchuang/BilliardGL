#include "game_state.h"
#include "shot.h"

#include <cassert>
#include <cmath>
#include <cstdlib>

namespace {

bool closeEnough(float a, float b)
{
    return std::fabs(a - b) < 0.0001f;
}

void testDefaultAimState()
{
    billiardgl::GameState state;
    assert(state.aim.mode == billiardgl::AimMode::Observe);
    assert(closeEnough(state.aim.yaw, -billiardgl::kPi / 2.0f));
    assert(closeEnough(state.aim.sensitivity, 0.01f));
}

void testAimDirectionIsHorizontalAndNormalized()
{
    const billiardgl::Point3 forward = billiardgl::aimDirectionOnTable(-billiardgl::kPi / 2.0f);
    assert(closeEnough(forward.x, 0.0f));
    assert(closeEnough(forward.y, 0.0f));
    assert(closeEnough(forward.z, -1.0f));

    const billiardgl::Point3 right = billiardgl::aimDirectionOnTable(0.0f);
    assert(closeEnough(right.x, 1.0f));
    assert(closeEnough(right.y, 0.0f));
    assert(closeEnough(right.z, 0.0f));
}

void testShotVelocityUsesAimYawAndPower()
{
    const billiardgl::Point3 velocity = billiardgl::shotVelocityFromAim(0.0f, 42.0f);
    assert(closeEnough(velocity.x, 42.0f));
    assert(closeEnough(velocity.y, 0.0f));
    assert(closeEnough(velocity.z, 0.0f));
}

}  // namespace

int main()
{
    testDefaultAimState();
    testAimDirectionIsHorizontalAndNormalized();
    testShotVelocityUsesAimYawAndPower();
    return EXIT_SUCCESS;
}
