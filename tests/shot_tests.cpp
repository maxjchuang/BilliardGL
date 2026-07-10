#include "game_state.h"
#include "shot.h"

#include <cmath>
#include <cstdlib>
#include <iostream>

namespace {

void expect(bool condition, const char* expression)
{
    if (!condition) {
        std::cerr << "Expectation failed: " << expression << std::endl;
        std::exit(EXIT_FAILURE);
    }
}

void expect(bool condition)
{
    expect(condition, "condition");
}

bool closeEnough(float a, float b)
{
    return std::fabs(a - b) < 0.0001f;
}

void testDefaultAimState()
{
    billiardgl::GameState state;
    expect(state.aim.mode == billiardgl::AimMode::Observe, "state.aim.mode == billiardgl::AimMode::Observe");
    expect(closeEnough(state.aim.yaw, billiardgl::kPi / 2.0f));
    expect(closeEnough(state.aim.sensitivity, 0.01f));
}

void testDefaultAimPointsFromCueBallTowardRack()
{
    billiardgl::GameState state;
    const billiardgl::Point3 velocity = billiardgl::shotVelocityFromAim(state.aim.yaw, 20.0f);

    expect(closeEnough(velocity.x, 0.0f));
    expect(velocity.z > 0.0f, "velocity.z > 0.0f");
}

void testAimDirectionIsHorizontalAndNormalized()
{
    const billiardgl::Point3 forward = billiardgl::aimDirectionOnTable(-billiardgl::kPi / 2.0f);
    expect(closeEnough(forward.x, 0.0f));
    expect(closeEnough(forward.y, 0.0f));
    expect(closeEnough(forward.z, -1.0f));

    const billiardgl::Point3 right = billiardgl::aimDirectionOnTable(0.0f);
    expect(closeEnough(right.x, 1.0f));
    expect(closeEnough(right.y, 0.0f));
    expect(closeEnough(right.z, 0.0f));
}

void testShotVelocityUsesAimYawAndPower()
{
    const billiardgl::Point3 velocity = billiardgl::shotVelocityFromAim(0.0f, 42.0f);
    expect(closeEnough(velocity.x, 42.0f));
    expect(closeEnough(velocity.y, 0.0f));
    expect(closeEnough(velocity.z, 0.0f));
}

void testCueLinePointsInShotVelocityDirection()
{
    const float yaw = -billiardgl::kPi / 2.0f;
    const billiardgl::Point3 lineStart = billiardgl::cueLineStartFromAim(yaw);
    const billiardgl::Point3 lineEnd = billiardgl::cueLineEndFromAim(yaw, 150.0f);
    const billiardgl::Point3 velocity = billiardgl::shotVelocityFromAim(yaw, 20.0f);
    const float dot = lineEnd.x * velocity.x + lineEnd.z * velocity.z;
    const float startDistance = std::sqrt(lineStart.x * lineStart.x + lineStart.z * lineStart.z);

    expect(dot > 0.0f, "dot > 0.0f");
    expect(startDistance > billiardgl::kBallRadius, "startDistance > billiardgl::kBallRadius");
    expect(closeEnough(lineStart.y, 0.0f));
    expect(closeEnough(lineEnd.y, 0.0f));
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

    expect(dot < 0.0f, "dot < 0.0f");
    expect(distance >= billiardgl::kBallRadius * 3.0f, "distance >= billiardgl::kBallRadius * 3.0f");
}

void testCueStickModelTailPointsAwayFromShotDirection()
{
    const float yaw = billiardgl::kPi / 2.0f;
    const float rotationRadians = billiardgl::cueStickRotationDegreesFromAim(yaw) * billiardgl::kPi / 180.0f;
    const billiardgl::Point3 localPositiveZAfterRotation{
        std::sin(rotationRadians),
        0.0f,
        std::cos(rotationRadians)};
    const billiardgl::Point3 velocity = billiardgl::shotVelocityFromAim(yaw, 20.0f);
    const float dot = localPositiveZAfterRotation.x * velocity.x + localPositiveZAfterRotation.z * velocity.z;

    expect(dot < 0.0f, "dot < 0.0f");
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
    testCueStickModelTailPointsAwayFromShotDirection();
    return EXIT_SUCCESS;
}
