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

void testCueStickTipUsesVisibleSurfaceClearance()
{
    const billiardgl::Point3 cueBall{10.0f, billiardgl::kTableHeight + billiardgl::kBallRadius, 20.0f};
    const float yaw = 0.0f;
    const float modelTipOffset = 5.236411f;
    const billiardgl::Point3 cuePosition = billiardgl::cueStickPositionFromAim(
        cueBall, yaw, 0.0f, modelTipOffset);
    const billiardgl::Point3 direction = billiardgl::aimDirectionOnTable(yaw);
    const billiardgl::Point3 tip{
        cuePosition.x - direction.x * modelTipOffset,
        cuePosition.y,
        cuePosition.z - direction.z * modelTipOffset};
    const float tipDistance = std::hypot(
        tip.x - cueBall.x, tip.z - cueBall.z);

    expect(closeEnough(tipDistance,
        billiardgl::kBallRadius + billiardgl::kCueTipRestGapCm));

    const billiardgl::Point3 poweredPosition =
        billiardgl::cueStickPositionFromAim(
            cueBall, yaw, 60.0f, modelTipOffset);
    const float poweredTipDistance = std::fabs(
        poweredPosition.x - modelTipOffset - cueBall.x);
    expect(closeEnough(poweredTipDistance,
        billiardgl::kBallRadius + billiardgl::kCueTipRestGapCm +
            60.0f * billiardgl::kCuePowerBackoffCmPerUnit));
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

void testShotPowerMapsToPhysicalCueInput()
{
    const billiardgl::PhysicsProfile profile =
        billiardgl::defaultChinesePoolPhysicsProfile();
    const billiardgl::CueImpactInput input =
        billiardgl::cueImpactFromShotControls(0.0f, 40.0f, profile);

    expect(input.cueBallIndex == 0, "cue ball index");
    expect(std::fabs(input.cueSpeedCmS - 53.6) < 0.00001,
        "power uses versioned cue-speed scale");
    expect(std::fabs(input.cueMassKg - 0.5) < 0.00001, "profile cue mass");
    expect(std::fabs(input.direction[0] - 1.0) < 0.00001 &&
        std::fabs(input.direction[1]) < 0.00001 &&
        std::fabs(input.direction[2]) < 0.00001, "horizontal unit direction");
    expect(input.elevationDegrees == 0.0, "ordinary shot is horizontal");
    expect(input.tipOffsetCm == std::array<double, 2>{{0.0, 0.0}},
        "ordinary shot is centered");
    expect(input.tipOffsetRadius == std::array<double, 2>{{0.0, 0.0}},
        "dimensionless offset is centered");
    expect(input.chalkState == "CHALKED", "ordinary cue is chalked");

    const billiardgl::CueImpactInput zero =
        billiardgl::cueImpactFromShotControls(0.0f, 0.0f, profile);
    const billiardgl::CueImpactInput maximum =
        billiardgl::cueImpactFromShotControls(0.0f, 200.0f, profile);
    expect(zero.cueSpeedCmS == 0.0 &&
        maximum.cueSpeedCmS > input.cueSpeedCmS,
        "mapping is zero-based and monotonic");
}

}  // namespace

int main()
{
    testDefaultAimState();
    testDefaultAimPointsFromCueBallTowardRack();
    testAimDirectionIsHorizontalAndNormalized();
    testShotVelocityUsesAimYawAndPower();
    testCueLinePointsInShotVelocityDirection();
    testCueStickTipUsesVisibleSurfaceClearance();
    testCueStickModelTailPointsAwayFromShotDirection();
    testShotPowerMapsToPhysicalCueInput();
    return EXIT_SUCCESS;
}
