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
    assert(closeEnough(state.aim.yaw, billiardgl::kPi / 2.0f));
    assert(closeEnough(state.aim.sensitivity, 0.01f));
}

void testDefaultAimPointsFromCueBallTowardRack()
{
    billiardgl::GameState state;
    const billiardgl::Point3 velocity = billiardgl::shotVelocityFromAim(state.aim.yaw, 20.0f);

    assert(closeEnough(velocity.x, 0.0f));
    assert(velocity.z > 0.0f);
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

void testCueLinePointsInShotVelocityDirection()
{
    const float yaw = -billiardgl::kPi / 2.0f;
    const billiardgl::Point3 lineStart = billiardgl::cueLineStartFromAim(yaw);
    const billiardgl::Point3 lineEnd = billiardgl::cueLineEndFromAim(yaw, 150.0f);
    const billiardgl::Point3 velocity = billiardgl::shotVelocityFromAim(yaw, 20.0f);
    const float dot = lineEnd.x * velocity.x + lineEnd.z * velocity.z;
    const float startDistance = std::sqrt(lineStart.x * lineStart.x + lineStart.z * lineStart.z);

    assert(dot > 0.0f);
    assert(startDistance > billiardgl::kBallRadius);
    assert(lineStart.y > 0.0f);
    assert(lineEnd.y > 0.0f);
}

void testCueStickStaysBehindCueBallOutsideBallRadius()
{
    const billiardgl::Point3 cueBall{10.0f, billiardgl::kTableHeight + billiardgl::kBallRadius, 20.0f};
    const float yaw = 0.0f;
    const billiardgl::Point3 cuePosition = billiardgl::cueStickPositionFromAim(cueBall, yaw, 0.0f);
    const billiardgl::Point3 velocity = billiardgl::shotVelocityFromAim(yaw, 20.0f);
    const float offsetX = cuePosition.x - cueBall.x;
    const float offsetZ = cuePosition.z - cueBall.z;
    const float dot = offsetX * velocity.x + offsetZ * velocity.z;
    const float distance = std::sqrt(offsetX * offsetX + offsetZ * offsetZ);

    assert(dot < 0.0f);
    assert(distance >= billiardgl::kBallRadius * 3.0f);
}

}  // namespace

int main()
{
    testDefaultAimState();
    testDefaultAimPointsFromCueBallTowardRack();
    testAimDirectionIsHorizontalAndNormalized();
    testShotVelocityUsesAimYawAndPower();
    testCueLinePointsInShotVelocityDirection();
    testCueStickStaysBehindCueBallOutsideBallRadius();
    return EXIT_SUCCESS;
}
