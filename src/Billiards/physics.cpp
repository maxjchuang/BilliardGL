#include "physics.h"

#include <cmath>

namespace billiardgl {

void applyFrictionAndMove(BallState& ball, float timeStep, float frictionAcceleration)
{
    const float speedSquared = ball.velocity.x * ball.velocity.x + ball.velocity.z * ball.velocity.z;
    if (speedSquared <= 0.1f) {
        ball.speed = 0.0f;
        ball.velocity.x = 0.0f;
        ball.velocity.z = 0.0f;
        return;
    }

    ball.speed = std::sqrt(speedSquared);
    const float vx = ball.velocity.x / ball.speed;
    const float vz = ball.velocity.z / ball.speed;
    ball.speed += frictionAcceleration * timeStep;

    if (ball.speed <= 0.0f) {
        ball.speed = 0.0f;
        ball.velocity.x = 0.0f;
        ball.velocity.z = 0.0f;
        return;
    }

    ball.velocity.x = ball.speed * vx;
    ball.velocity.z = ball.speed * vz;
    ball.position.x += ball.velocity.x * timeStep;
    ball.position.z += ball.velocity.z * timeStep;
    ball.rotationAxis.x = -vz;
    ball.rotationAxis.z = vx;
    ball.rotationAngle += -180.0f * ball.speed * timeStep / (kBallRadius * kPi);
}

bool collideBalls(BallState& first, BallState& second)
{
    const float dx = second.position.x - first.position.x;
    const float dz = second.position.z - first.position.z;
    const float distance = std::sqrt(dx * dx + dz * dz);
    if (distance <= 0.0f || distance >= 2.0f * kBallRadius - 0.5f) {
        return false;
    }

    const float cosValue = dx / distance;
    const float sinValue = dz / distance;
    const float cCos = -sinValue;
    const float cSin = cosValue;
    const float v1c = first.velocity.x * cosValue + first.velocity.z * sinValue;
    const float v1cc = first.velocity.x * cCos + first.velocity.z * cSin;
    const float v2c = second.velocity.x * cosValue + second.velocity.z * sinValue;
    const float v2cc = second.velocity.x * cCos + second.velocity.z * cSin;

    first.velocity.x = v1cc * cCos + v2c * cosValue;
    first.velocity.z = v1cc * cSin + v2c * sinValue;
    second.velocity.x = v1c * cosValue + v2cc * cCos;
    second.velocity.z = v1c * sinValue + v2cc * cSin;
    second.position.x = first.position.x + 2.0f * kBallRadius * cosValue;
    second.position.z = first.position.z + 2.0f * kBallRadius * sinValue;
    return true;
}

void collideWithTableEdge(BallState& ball)
{
    if (std::fabs(ball.position.x) > kTableInWidth / 2.0f - kBallRadius) {
        ball.position.x = ball.position.x > 0.0f
            ? kTableInWidth / 2.0f - kBallRadius
            : -kTableInWidth / 2.0f + kBallRadius;
        ball.velocity.x *= -1.0f;
    }

    if (std::fabs(ball.position.z) > kTableInLength / 2.0f - kBallRadius) {
        ball.position.z = ball.position.z > 0.0f
            ? kTableInLength / 2.0f - kBallRadius
            : -kTableInLength / 2.0f + kBallRadius;
        ball.velocity.z *= -1.0f;
    }
}

bool isInPocket(const BallState& ball)
{
    const float pocketX[6] = {
        -kTableInWidth / 2.0f + kPocketRadius,
        kTableInWidth / 2.0f - kPocketRadius,
        -kTableInWidth / 2.0f + kPocketRadius,
        kTableInWidth / 2.0f - kPocketRadius,
        -kTableInWidth / 2.0f + kPocketRadius,
        kTableInWidth / 2.0f - kPocketRadius,
    };
    const float pocketZ[6] = {
        -kTableInLength / 2.0f + kPocketRadius,
        -kTableInLength / 2.0f + kPocketRadius,
        kTableInLength / 2.0f - kPocketRadius,
        kTableInLength / 2.0f - kPocketRadius,
        0.0f,
        0.0f,
    };

    for (int i = 0; i < 6; ++i) {
        const float dx = ball.position.x - pocketX[i];
        const float dz = ball.position.z - pocketZ[i];
        if (std::sqrt(dx * dx + dz * dz) < kBallRadius / 4.0f) {
            return true;
        }
    }
    return false;
}

bool updatePocketedBall(GameState& state, int ballIndex)
{
    BallState& ball = state.balls[ballIndex];
    if (!isInPocket(ball)) {
        return false;
    }

    if (ballIndex == 0) {
        ball.position = Point3{0.0f, kTableHeight + kBallRadius, -kTableInLength / 4.0f};
        state.players.illegalShot = true;
    } else {
        ball.pocketed = true;
        state.pocketedBallCount += 1;
        ball.position.z = -100.0f + static_cast<float>(state.pocketedBallCount) * 20.0f;
        ball.position.y = kTableHeight - kBallRadius;
        if (ballIndex != 8) {
            ball.position.y = -100.0f;
        }
        if (ballIndex == 8) {
            state.gameOver = true;
        }
    }

    ball.velocity = Point3{};
    return true;
}

void updatePhysics(GameState& state, float timeStep)
{
    bool anyMoving = false;
    for (int i = 0; i < kBallCount; ++i) {
        BallState& ball = state.balls[i];
        if (ball.pocketed) {
            continue;
        }
        for (int j = i + 1; j < kBallCount; ++j) {
            if (!state.balls[j].pocketed) {
                collideBalls(ball, state.balls[j]);
            }
        }
        collideWithTableEdge(ball);
        updatePocketedBall(state, i);
        applyFrictionAndMove(ball, timeStep, kDefaultFrictionAcceleration);
        if (ball.speed > 0.0f) {
            anyMoving = true;
        }
    }
    state.ballsMoving = anyMoving;
}

}  // namespace billiardgl
