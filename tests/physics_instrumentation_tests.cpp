#include "game_state.h"
#include "physics.h"
#include "physics_telemetry.h"

#include <cmath>
#include <array>
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

bool close(float first, float second, float epsilon = 0.0001f)
{
    return std::fabs(first - second) <= epsilon;
}

void pocketAll(billiardgl::GameState& state)
{
    for (billiardgl::BallState& ball : state.balls) {
        ball.pocketed = true;
        ball.speed = 0.0f;
        ball.velocity = billiardgl::Point3{};
    }
}

}  // namespace

int main()
{
    billiardgl::GameState collisionState;
    billiardgl::initializeBalls(collisionState);
    pocketAll(collisionState);
    collisionState.balls[0].pocketed = false;
    collisionState.balls[1].pocketed = false;
    collisionState.balls[0].position = billiardgl::Point3{0.0f, billiardgl::kTableHeight + billiardgl::kBallRadius, 20.0f};
    collisionState.balls[1].position = billiardgl::Point3{5.0f, billiardgl::kTableHeight + billiardgl::kBallRadius, 20.0f};
    billiardgl::setBallVelocity(collisionState.balls[0], 10.0f, 0.0f, 0.0f);
    collisionState.balls[0].speed = 10.0f;

    billiardgl::BallState expectedFirst = collisionState.balls[0];
    billiardgl::BallState expectedSecond = collisionState.balls[1];
    expect(billiardgl::collideBalls(expectedFirst, expectedSecond), "legacy collision precondition");

    const billiardgl::PhysicsStepTelemetry collision =
        billiardgl::updatePhysics(collisionState, 0.0f);
    expect(collision.contacts.size() == 1, "one overlapping pair emits one contact");
    expect(collision.contacts[0].kind == billiardgl::PhysicsContactKind::BallBall,
        "ball contact kind");
    expect(collision.contacts[0].firstBall == 0 && collision.contacts[0].secondBall == 1,
        "stable ball ids");
    expect(collision.contacts[0].penetrationCm > 0.0, "penetration recorded before correction");
    expect(collision.contacts[0].normalImpulseNs >= 0.0, "nonnegative impulse magnitude");
    expect(close(collisionState.balls[0].velocity.x, expectedFirst.velocity.x) &&
        close(collisionState.balls[1].velocity.x, expectedSecond.velocity.x),
        "instrumentation preserves legacy ball collision velocities");

    billiardgl::GameState railState;
    billiardgl::initializeBalls(railState);
    pocketAll(railState);
    railState.balls[2].pocketed = false;
    railState.balls[2].position = billiardgl::Point3{
        billiardgl::kTableInWidth / 2.0f,
        billiardgl::kTableHeight + billiardgl::kBallRadius,
        25.0f};
    billiardgl::setBallVelocity(railState.balls[2], 5.0f, 0.0f, 0.0f);
    railState.balls[2].speed = 5.0f;
    const billiardgl::PhysicsStepTelemetry rail = billiardgl::updatePhysics(railState, 0.0f);
    expect(rail.contacts.size() == 1 && rail.contacts[0].kind == billiardgl::PhysicsContactKind::Rail,
        "ordinary rail emits one contact");
    expect(rail.contacts[0].firstBall == 2 && rail.contacts[0].normal.x < 0.0f,
        "rail contact identifies ball and inward normal");

    billiardgl::GameState pocketState;
    billiardgl::initializeBalls(pocketState);
    pocketAll(pocketState);
    pocketState.balls[3].pocketed = false;
    pocketState.balls[3].position = billiardgl::Point3{
        -billiardgl::kTableInWidth / 2.0f - 1.0f,
        billiardgl::kTableHeight + billiardgl::kBallRadius,
        0.0f};
    const billiardgl::PhysicsStepTelemetry pocket = billiardgl::updatePhysics(pocketState, 0.0f);
    expect(pocket.contacts.size() == 1 && pocket.contacts[0].kind == billiardgl::PhysicsContactKind::Pocket,
        "pocket transition emits one contact");
    expect(pocket.contacts[0].firstBall == 3 && pocket.contacts[0].normalImpulseNs == 0.0,
        "current teleport pocket has zero impulse");

    billiardgl::GameState emptyState;
    billiardgl::initializeBalls(emptyState);
    pocketAll(emptyState);
    emptyState.balls[4].pocketed = false;
    emptyState.balls[4].position = billiardgl::Point3{
        0.0f, billiardgl::kTableHeight + billiardgl::kBallRadius, 20.0f};
    const billiardgl::PhysicsStepTelemetry empty = billiardgl::updatePhysics(emptyState, 0.0f);
    expect(empty.contacts.empty() && empty.maximumPenetrationCm == 0.0,
        "no contact produces empty telemetry");

    billiardgl::GameState goldenState;
    billiardgl::initializeBalls(goldenState);
    pocketAll(goldenState);
    goldenState.balls[0].pocketed = false;
    goldenState.balls[1].pocketed = false;
    goldenState.balls[0].position = billiardgl::Point3{
        0.0f, billiardgl::kTableHeight + billiardgl::kBallRadius, 20.0f};
    goldenState.balls[1].position = billiardgl::Point3{
        10.0f, billiardgl::kTableHeight + billiardgl::kBallRadius, 20.0f};
    billiardgl::setBallVelocity(goldenState.balls[0], 20.0f, 0.0f, 0.0f);
    const billiardgl::GameState goldenInitial = goldenState;
    for (int tick = 0; tick < 5; ++tick) {
        billiardgl::updatePhysics(goldenState, billiardgl::kDefaultTimeStep);
    }

    std::array<billiardgl::Point3, billiardgl::kBallCount> expectedPositions;
    std::array<billiardgl::Point3, billiardgl::kBallCount> expectedVelocities;
    for (int index = 0; index < billiardgl::kBallCount; ++index) {
        expectedPositions[index] = goldenInitial.balls[index].position;
        expectedVelocities[index] = goldenInitial.balls[index].velocity;
    }
    expectedPositions[0] = billiardgl::Point3{
        5.76f, billiardgl::kTableHeight + billiardgl::kBallRadius, 20.0f};
    expectedVelocities[0] = billiardgl::Point3{};
    expectedPositions[1] = billiardgl::Point3{
        15.115f, billiardgl::kTableHeight + billiardgl::kBallRadius, 20.0f};
    expectedVelocities[1] = billiardgl::Point3{18.0f, 0.0f, 0.0f};
    for (int index = 0; index < billiardgl::kBallCount; ++index) {
        const billiardgl::BallState& actual = goldenState.balls[index];
        expect(close(actual.position.x, expectedPositions[index].x) &&
            close(actual.position.y, expectedPositions[index].y) &&
            close(actual.position.z, expectedPositions[index].z),
            "five-tick golden position must preserve pre-instrumentation behavior");
        expect(close(actual.velocity.x, expectedVelocities[index].x) &&
            close(actual.velocity.y, expectedVelocities[index].y) &&
            close(actual.velocity.z, expectedVelocities[index].z),
            "five-tick golden velocity must preserve pre-instrumentation behavior");
    }
    return 0;
}
