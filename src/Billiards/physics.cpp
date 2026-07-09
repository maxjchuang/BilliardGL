#include "physics.h"

#include "rules.h"
#include "table_specs.h"

#include <array>
#include <cmath>

namespace billiardgl {
namespace {

std::array<PocketOpening, 6> currentPocketOpenings()
{
    return buildPocketOpenings(defaultTableSpec(), defaultPocketSpec());
}

bool sameSignOrZero(float value, float reference)
{
    if (reference < 0.0f) {
        return value <= 0.0f;
    }
    if (reference > 0.0f) {
        return value >= 0.0f;
    }
    return true;
}

bool isInsideOpeningBand(const BallState& ball, const PocketOpening& opening)
{
    const float halfMouth = opening.mouthWidthCm / 2.0f + kBallRadius;
    if (opening.kind == PocketKind::Side) {
        return std::fabs(ball.position.z - opening.centerZ) <= halfMouth &&
            sameSignOrZero(ball.position.x, opening.centerX);
    }

    return std::fabs(ball.position.x - opening.centerX) <= halfMouth &&
        std::fabs(ball.position.z - opening.centerZ) <= halfMouth;
}

bool hasCrossedPocketDropZone(const BallState& ball, const PocketOpening& opening)
{
    const float halfWidth = kTableInWidth / 2.0f;
    const float halfLength = kTableInLength / 2.0f;
    const float depth = opening.dropZoneDepthCm;

    if (opening.kind == PocketKind::Side) {
        if (opening.centerX < 0.0f) {
            return ball.position.x <= -halfWidth + depth;
        }
        return ball.position.x >= halfWidth - depth;
    }

    const bool crossedX = opening.centerX < 0.0f
        ? ball.position.x <= -halfWidth + depth
        : ball.position.x >= halfWidth - depth;
    const bool crossedZ = opening.centerZ < 0.0f
        ? ball.position.z <= -halfLength + depth
        : ball.position.z >= halfLength - depth;
    return crossedX || crossedZ;
}

}  // namespace

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
    if (isInPocketMouth(ball)) {
        return;
    }

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

bool isInPocketMouth(const BallState& ball)
{
    const std::array<PocketOpening, 6> openings = currentPocketOpenings();
    for (std::size_t i = 0; i < openings.size(); ++i) {
        if (isInsideOpeningBand(ball, openings[i])) {
            return true;
        }
    }
    return false;
}

bool isInPocket(const BallState& ball)
{
    const std::array<PocketOpening, 6> openings = currentPocketOpenings();
    for (std::size_t i = 0; i < openings.size(); ++i) {
        if (isInsideOpeningBand(ball, openings[i]) && hasCrossedPocketDropZone(ball, openings[i])) {
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
        state.events.cueBallPocketed = true;
    } else {
        ball.pocketed = true;
        state.pocketedBallCount += 1;
        state.events.ballPocketed = true;
        ball.position.z = -100.0f + static_cast<float>(state.pocketedBallCount) * 20.0f;
        ball.position.y = kTableHeight - kBallRadius;
        if (ballIndex != 8) {
            ball.position.y = -100.0f;
            assignPlayerBallTypeForPocketedObjectBall(state, ballIndex);
        }
        if (ballIndex == 8) {
            state.gameOver = true;
            state.events.eightBallPocketed = true;
        }
    }

    ball.velocity = Point3{};
    return true;
}

void updatePhysics(GameState& state, float timeStep)
{
    const bool wasMoving = state.ballsMoving;
    clearGameplayEvents(state);
    bool anyMoving = false;
    for (int i = 0; i < kBallCount; ++i) {
        BallState& ball = state.balls[i];
        if (ball.pocketed) {
            continue;
        }
        for (int j = i + 1; j < kBallCount; ++j) {
            if (!state.balls[j].pocketed) {
                if (collideBalls(ball, state.balls[j])) {
                    state.events.ballCollision = true;
                }
            }
        }
        const float previousX = ball.velocity.x;
        const float previousZ = ball.velocity.z;
        collideWithTableEdge(ball);
        if (previousX != ball.velocity.x || previousZ != ball.velocity.z) {
            state.events.railCollision = true;
        }
        updatePocketedBall(state, i);
        applyFrictionAndMove(ball, timeStep, kDefaultFrictionAcceleration);
        if (ball.speed > 0.0f) {
            anyMoving = true;
        }
    }
    state.ballsMoving = anyMoving;
    state.events.shotEnded = wasMoving && !anyMoving && state.players.shotTaken;
}

}  // namespace billiardgl
