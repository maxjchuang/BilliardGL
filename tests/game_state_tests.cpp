#include "game_state.h"

#include <array>
#include <cstdlib>
#include <iostream>

namespace {

int fail(const char* message)
{
    std::cerr << message << std::endl;
    return EXIT_FAILURE;
}

}  // namespace

int main()
{
    billiardgl::GameState state;
    if (state.aim.showGuideLine) {
        return fail("aim guide line should be disabled by default");
    }
    billiardgl::initializeBalls(state);
    state.balls[0].position = billiardgl::Point3{1.0f, 2.0f, 3.0f};
    state.balls[0].velocity = billiardgl::Point3{4.0f, 0.0f, 5.0f};
    state.balls[0].rotationAxis = billiardgl::Point3{0.0f, 1.0f, 0.0f};
    state.balls[0].speed = 6.0f;
    state.balls[0].rotationAngle = 7.0f;
    state.balls[0].pocketed = true;
    state.balls[0].texture = 42;

    std::array<billiardgl::LegacyBallAdapter, billiardgl::kBallCount> legacyBalls;
    billiardgl::copyBallStateToLegacy(state, legacyBalls);

    if (legacyBalls[0].position.x != 1.0f || legacyBalls[0].position.y != 2.0f || legacyBalls[0].position.z != 3.0f) {
        return fail("legacy adapter should copy position from GameState");
    }
    if (legacyBalls[0].velocity.x != 4.0f || legacyBalls[0].velocity.z != 5.0f) {
        return fail("legacy adapter should copy velocity from GameState");
    }
    if (!legacyBalls[0].pocketed || legacyBalls[0].texture != 42) {
        return fail("legacy adapter should copy pocket and texture fields");
    }

    legacyBalls[1].texture = 99;
    billiardgl::copyLegacyTexturesToState(legacyBalls, state);
    if (state.balls[1].texture != 99) {
        return fail("texture copy should preserve render-loaded texture IDs in GameState");
    }

    return EXIT_SUCCESS;
}
