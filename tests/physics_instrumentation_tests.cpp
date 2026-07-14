#include "game_state.h"
#include "physics.h"
#include "physics_telemetry.h"

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
    const billiardgl::PhysicsProfile defaultProfile =
        billiardgl::defaultChinesePoolPhysicsProfile();
    expect(billiardgl::collideBalls(expectedFirst, expectedSecond, defaultProfile),
        "profile collision precondition");

    const billiardgl::PhysicsStepTelemetry collision =
        billiardgl::updatePhysics(collisionState, 0.0f);
    expect(collision.contacts.size() == 1, "one overlapping pair emits one contact");
    expect(collision.contacts[0].kind == billiardgl::PhysicsContactKind::BallBall,
        "ball contact kind");
    expect(collision.contacts[0].firstBall == 0 && collision.contacts[0].secondBall == 1,
        "stable ball ids");
    expect(collision.contacts[0].penetrationCm > 0.0, "penetration recorded before correction");
    expect(collision.contacts[0].normalImpulseNs >= 0.0, "nonnegative impulse magnitude");
    expect(collision.contacts[0].velocityImpulseApplied &&
        collision.contacts[0].regime ==
            billiardgl::BallBallContactRegime::Frictionless &&
        collision.contacts[0].normalRelativeSpeedBeforeCmS < 0.0 &&
        collision.contacts[0].normalRelativeSpeedAfterCmS >= 0.0 &&
        collision.contacts[0].kineticEnergyAfterJ <=
            collision.contacts[0].kineticEnergyBeforeJ,
        "production contact emits authoritative approach, regime, and energy diagnostics");
    expect(close(collisionState.balls[0].velocity.x, expectedFirst.velocity.x) &&
        close(collisionState.balls[1].velocity.x, expectedSecond.velocity.x),
        "instrumentation preserves authoritative ball collision velocities");

    billiardgl::GameState lightCollision = collisionState;
    lightCollision.balls[0].position.x = 0.0f;
    lightCollision.balls[1].position.x = 5.0f;
    billiardgl::setBallVelocity(lightCollision.balls[0], 10.0f, 0.0f, 0.0f);
    billiardgl::setBallVelocity(lightCollision.balls[1], 0.0f, 0.0f, 0.0f);
    billiardgl::PhysicsProfile lightProfile = defaultProfile;
    lightProfile.ball.massKg *= 0.5f;
    const billiardgl::PhysicsStepTelemetry lightTelemetry =
        billiardgl::updatePhysics(lightCollision, 0.0f, lightProfile);
    expect(lightTelemetry.contacts.size() == 1 &&
        close(static_cast<float>(lightTelemetry.contacts[0].normalImpulseNs),
            static_cast<float>(collision.contacts[0].normalImpulseNs * 0.5)),
        "profile mass scales collision impulse without falsifying equal-mass velocity physics");

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
    pocketState.balls[3].pocketInteraction.phase =
        billiardgl::PocketInteractionPhase::Captured;
    pocketState.balls[3].pocketInteraction.pocketId = 4;
    pocketState.balls[3].pocketInteraction.captureSequence = 1;
    const billiardgl::PhysicsStepTelemetry pocket = billiardgl::updatePhysics(pocketState, 0.0f);
    expect(pocket.contacts.size() == 1 && pocket.contacts[0].kind == billiardgl::PhysicsContactKind::Pocket,
        "pocket transition emits one contact");
    expect(pocket.contacts[0].firstBall == 3 && pocket.contacts[0].normalImpulseNs == 0.0,
        "captured-state rule event has zero impulse");

    billiardgl::GameState emptyState;
    billiardgl::initializeBalls(emptyState);
    pocketAll(emptyState);
    emptyState.balls[4].pocketed = false;
    emptyState.balls[4].position = billiardgl::Point3{
        0.0f, billiardgl::kTableHeight + billiardgl::kBallRadius, 20.0f};
    const billiardgl::PhysicsStepTelemetry empty = billiardgl::updatePhysics(emptyState, 0.0f);
    expect(empty.contacts.empty() && empty.maximumPenetrationCm == 0.0,
        "no contact produces empty telemetry");

    expect(collision.surfaceMotion.size() == 2,
        "every active collision ball retains surface telemetry at dt zero");
    expect(rail.surfaceMotion.size() == 1 && pocket.surfaceMotion.size() == 1 &&
        empty.surfaceMotion.size() == 1,
        "rail, pocket, and empty steps retain their contact records and surface telemetry");
    return 0;
}
