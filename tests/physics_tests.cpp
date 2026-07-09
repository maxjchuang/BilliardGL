#include "game_state.h"
#include "physics.h"

#include <cmath>
#include <cstdlib>
#include <iostream>

namespace {

bool nearlyEqual(float a, float b, float epsilon = 0.001f)
{
    return std::fabs(a - b) < epsilon;
}

int fail(const char* message)
{
    std::cerr << message << std::endl;
    return EXIT_FAILURE;
}

}  // namespace

int main()
{
    billiardgl::GameState state;
    billiardgl::initializeBalls(state);
    if (state.balls[0].position.y != billiardgl::kTableHeight + billiardgl::kBallRadius) {
        return fail("cue ball should start on the table");
    }

    state.balls[0].velocity.x = 20.0f;
    billiardgl::applyFrictionAndMove(state.balls[0], 0.1f, -4.0f);
    if (!(state.balls[0].speed < 20.0f)) {
        return fail("friction should reduce speed");
    }
    if (!(state.balls[0].position.x > 0.0f)) {
        return fail("ball should move after physics update");
    }

    state.balls[0].velocity.x = 0.1f;
    state.balls[0].velocity.z = 0.0f;
    billiardgl::applyFrictionAndMove(state.balls[0], 0.1f, -4.0f);
    if (!nearlyEqual(state.balls[0].speed, 0.0f)) {
        return fail("very slow ball should stop");
    }

    state.balls[0].position.x = billiardgl::kTableInWidth / 2.0f;
    state.balls[0].velocity.x = 5.0f;
    billiardgl::collideWithTableEdge(state.balls[0]);
    if (!(state.balls[0].velocity.x < 0.0f)) {
        return fail("wall collision should reverse x velocity");
    }

    state.balls[1].position = billiardgl::Point3{0.0f, billiardgl::kTableHeight + billiardgl::kBallRadius, 0.0f};
    state.balls[2].position = billiardgl::Point3{2.0f * billiardgl::kBallRadius - 0.6f, billiardgl::kTableHeight + billiardgl::kBallRadius, 0.0f};
    state.balls[1].velocity.x = 10.0f;
    state.balls[2].velocity.x = 0.0f;
    billiardgl::collideBalls(state.balls[1], state.balls[2]);
    if (!(state.balls[2].velocity.x > 0.0f)) {
        return fail("ball collision should transfer velocity");
    }

    state.balls[3].position.x = -billiardgl::kTableInWidth / 2.0f - 1.0f;
    state.balls[3].position.z = -billiardgl::kTableInLength / 2.0f + 2.0f;
    if (!billiardgl::isInPocket(state.balls[3])) {
        return fail("ball crossing a corner pocket mouth should be detected as pocketed");
    }

    state.balls[4].position.x = -billiardgl::kTableInWidth / 2.0f - 1.0f;
    state.balls[4].position.z = 0.0f;
    if (!billiardgl::isInPocket(state.balls[4])) {
        return fail("ball crossing a side pocket mouth should be detected as pocketed");
    }

    state.balls[5].position.x = billiardgl::kTableInWidth / 2.0f + 1.0f;
    state.balls[5].position.z = 25.0f;
    if (billiardgl::isInPocket(state.balls[5])) {
        return fail("ball outside a pocket mouth should not be pocketed");
    }

    billiardgl::GameState pocketState;
    billiardgl::initializeBalls(pocketState);
    pocketState.balls[0].position.x = -billiardgl::kTableInWidth / 2.0f - 1.0f;
    pocketState.balls[0].position.z = -billiardgl::kTableInLength / 2.0f + 2.0f;
    if (!billiardgl::updatePocketedBall(pocketState, 0)) {
        return fail("cue ball at pocket center should be updated as pocketed");
    }
    if (!pocketState.players.illegalShot || !pocketState.events.cueBallPocketed) {
        return fail("cue ball pocket should mark illegal shot and cue pocket event");
    }

    billiardgl::GameState eightState;
    billiardgl::initializeBalls(eightState);
    eightState.balls[8].position.x = billiardgl::kTableInWidth / 2.0f + 1.0f;
    eightState.balls[8].position.z = 0.0f;
    if (!billiardgl::updatePocketedBall(eightState, 8)) {
        return fail("eight ball at pocket center should be updated as pocketed");
    }
    if (!eightState.gameOver || !eightState.events.eightBallPocketed) {
        return fail("eight ball pocket should mark game over and eight ball event");
    }

    billiardgl::BallState sideMouthBall;
    sideMouthBall.position = billiardgl::Point3{
        -billiardgl::kTableInWidth / 2.0f - 0.5f,
        billiardgl::kTableHeight + billiardgl::kBallRadius,
        0.0f};
    sideMouthBall.velocity.x = -5.0f;
    billiardgl::collideWithTableEdge(sideMouthBall);
    if (!(sideMouthBall.velocity.x < 0.0f)) {
        return fail("ball inside a pocket mouth should not bounce off the ordinary rail");
    }

    billiardgl::BallState ordinaryRailBall;
    ordinaryRailBall.position = billiardgl::Point3{
        billiardgl::kTableInWidth / 2.0f + 0.5f,
        billiardgl::kTableHeight + billiardgl::kBallRadius,
        25.0f};
    ordinaryRailBall.velocity.x = 5.0f;
    billiardgl::collideWithTableEdge(ordinaryRailBall);
    if (!(ordinaryRailBall.velocity.x < 0.0f)) {
        return fail("ball outside a pocket mouth should bounce off the ordinary rail");
    }

    state.balls[4].speed = 1.0f;
    if (!billiardgl::anyBallMoving(state)) {
        return fail("anyBallMoving should detect active ball speed");
    }
    state.balls[4].speed = 0.0f;
    if (billiardgl::anyBallMoving(state)) {
        return fail("anyBallMoving should be false when all speeds are zero");
    }

    return EXIT_SUCCESS;
}
