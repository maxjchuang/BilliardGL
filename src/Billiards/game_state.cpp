#include "game_state.h"

#include <cmath>

namespace billiardgl {

void initializeBalls(GameState& state)
{
    const Point3 start{0.0f, kTableHeight + kBallRadius, 0.0f};
    const float xDis = 2.0f * kBallRadius;
    const float yDis = 2.0f * kBallRadius * std::sin(kPi / 3.0f);

    state.balls[0].position = Point3{start.x, start.y, -start.z};
    state.balls[1].position = Point3{start.x, start.y, start.z};
    state.balls[2].position = Point3{start.x - xDis, start.y, start.z + yDis};
    state.balls[3].position = Point3{start.x + xDis, start.y, start.z + yDis};
    state.balls[4].position = Point3{start.x - 2.0f * xDis, start.y, start.z + 2.0f * yDis};
    state.balls[8].position = Point3{start.x, start.y, start.z + 2.0f * yDis};
    state.balls[6].position = Point3{start.x + 2.0f * xDis, start.y, start.z + 2.0f * yDis};
    state.balls[7].position = Point3{start.x - 3.0f * xDis, start.y, start.z + 3.0f * yDis};
    state.balls[5].position = Point3{start.x - xDis, start.y, start.z + 3.0f * yDis};
    state.balls[9].position = Point3{start.x + xDis, start.y, start.z + 3.0f * yDis};
    state.balls[10].position = Point3{start.x + 3.0f * xDis, start.y, start.z + 3.0f * yDis};
    state.balls[11].position = Point3{start.x - 4.0f * xDis, start.y, start.z + 4.0f * yDis};
    state.balls[12].position = Point3{start.x - 2.0f * xDis, start.y, start.z + 4.0f * yDis};
    state.balls[13].position = Point3{start.x, start.y, start.z + 4.0f * yDis};
    state.balls[14].position = Point3{start.x + 2.0f * xDis, start.y, start.z + 4.0f * yDis};
    state.balls[15].position = Point3{start.x + 4.0f * xDis, start.y, start.z + 4.0f * yDis};

    for (BallState& ball : state.balls) {
        ball.velocity = Point3{};
        ball.rotationAxis = Point3{};
        ball.speed = 0.0f;
        ball.rotationAngle = 0.0f;
        ball.pocketed = false;
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

void setBallVelocity(BallState& ball, float x, float y, float z)
{
    ball.velocity.x = x;
    ball.velocity.y = y;
    ball.velocity.z = z;
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
