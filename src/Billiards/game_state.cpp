#include "game_state.h"

#include "surface_motion.h"

#include <cmath>

namespace billiardgl {

void initializeBalls(GameState& state)
{
    const Point3 cueBallStart{0.0f, kTableHeight + kBallRadius, -kTableInLength / 4.0f};
    const Point3 rackApex{0.0f, kTableHeight + kBallRadius, kTableInLength / 4.0f};
    const float xDis = kBallRadius;
    const float yDis = std::sqrt(3.0f) * kBallRadius;

    state.balls[0].position = cueBallStart;
    state.balls[1].position = Point3{rackApex.x, rackApex.y, rackApex.z};
    state.balls[2].position = Point3{rackApex.x - xDis, rackApex.y, rackApex.z + yDis};
    state.balls[3].position = Point3{rackApex.x + xDis, rackApex.y, rackApex.z + yDis};
    state.balls[4].position = Point3{rackApex.x - 2.0f * xDis, rackApex.y, rackApex.z + 2.0f * yDis};
    state.balls[8].position = Point3{rackApex.x, rackApex.y, rackApex.z + 2.0f * yDis};
    state.balls[6].position = Point3{rackApex.x + 2.0f * xDis, rackApex.y, rackApex.z + 2.0f * yDis};
    state.balls[7].position = Point3{rackApex.x - 3.0f * xDis, rackApex.y, rackApex.z + 3.0f * yDis};
    state.balls[5].position = Point3{rackApex.x - xDis, rackApex.y, rackApex.z + 3.0f * yDis};
    state.balls[9].position = Point3{rackApex.x + xDis, rackApex.y, rackApex.z + 3.0f * yDis};
    state.balls[10].position = Point3{rackApex.x + 3.0f * xDis, rackApex.y, rackApex.z + 3.0f * yDis};
    state.balls[11].position = Point3{rackApex.x - 4.0f * xDis, rackApex.y, rackApex.z + 4.0f * yDis};
    state.balls[12].position = Point3{rackApex.x - 2.0f * xDis, rackApex.y, rackApex.z + 4.0f * yDis};
    state.balls[13].position = Point3{rackApex.x, rackApex.y, rackApex.z + 4.0f * yDis};
    state.balls[14].position = Point3{rackApex.x + 2.0f * xDis, rackApex.y, rackApex.z + 4.0f * yDis};
    state.balls[15].position = Point3{rackApex.x + 4.0f * xDis, rackApex.y, rackApex.z + 4.0f * yDis};

    for (BallState& ball : state.balls) {
        ball.velocity = Point3{};
        ball.angularVelocity = Point3{};
        ball.rotationAxis = Point3{};
        ball.speed = 0.0f;
        ball.rotationAngle = 0.0f;
        ball.pocketed = false;
        ball.motionState = BallMotionState::Stationary;
    }
}

void updateCameraFromCueBall(GameState& state)
{
    state.camera.target[0] = state.balls[0].position.x;
    state.camera.target[1] = state.balls[0].position.y;
    state.camera.target[2] = state.balls[0].position.z;
    state.camera.eye[0] = state.camera.zoom * std::cos(state.camera.angleX) + state.camera.target[0];
    state.camera.eye[1] = state.camera.zoom * std::cos(state.camera.angleY) + state.camera.target[1];
    state.camera.eye[2] = state.camera.zoom * std::sin(state.camera.angleX) * std::sin(state.camera.angleY) + state.camera.target[2];
}

void resetBallMotion(BallState& ball)
{
    ball.velocity = Point3{};
    ball.angularVelocity = Point3{};
    ball.rotationAxis = Point3{};
    ball.speed = 0.0f;
    ball.rotationAngle = 0.0f;
    ball.motionState = BallMotionState::Stationary;
}

void setBallVelocity(BallState& ball, float x, float y, float z)
{
    ball.velocity.x = x;
    ball.velocity.y = y;
    ball.velocity.z = z;
    ball.motionState = (x == 0.0f && y == 0.0f && z == 0.0f)
        ? BallMotionState::Stationary
        : BallMotionState::Sliding;
}

bool anyBallMoving(const GameState& state)
{
    for (const BallState& ball : state.balls) {
        if (!ball.pocketed && ball.speed > 0.0f) {
            return true;
        }
    }
    return false;
}

void clearGameplayEvents(GameState& state)
{
    state.events = GameplayEvents{};
}

void copyBallStateToLegacy(const GameState& state, std::array<LegacyBallAdapter, kBallCount>& legacyBalls)
{
    for (int i = 0; i < kBallCount; ++i) {
        legacyBalls[i].position = state.balls[i].position;
        legacyBalls[i].velocity = state.balls[i].velocity;
        legacyBalls[i].rotationAxis = state.balls[i].rotationAxis;
        legacyBalls[i].speed = state.balls[i].speed;
        legacyBalls[i].rotationAngle = state.balls[i].rotationAngle;
        legacyBalls[i].pocketed = state.balls[i].pocketed;
        legacyBalls[i].texture = state.balls[i].texture;
    }
}

void copyLegacyTexturesToState(const std::array<LegacyBallAdapter, kBallCount>& legacyBalls, GameState& state)
{
    for (int i = 0; i < kBallCount; ++i) {
        state.balls[i].texture = legacyBalls[i].texture;
    }
}

}  // namespace billiardgl
