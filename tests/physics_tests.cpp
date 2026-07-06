#include "game_state.h"

#include <cstdlib>
#include <iostream>

int main()
{
    billiardgl::GameState state;
    billiardgl::initializeBalls(state);
    if (state.balls[0].position.y != billiardgl::kTableHeight + billiardgl::kBallRadius) {
        std::cerr << "cue ball should start on the table" << std::endl;
        return EXIT_FAILURE;
    }
    return EXIT_SUCCESS;
}
