#include "game_state.h"
#include "rules.h"

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
    state.players.currentPlayer = 0;
    state.players.nextPlayer = 0;
    state.players.illegalShot = false;
    state.players.shotTaken = true;
    state.transitionPerspective = false;

    billiardgl::updatePlayerAfterShot(state);
    if (state.players.currentPlayer != 0) {
        return fail("legal shot with same next player should keep current player");
    }
    if (!state.players.updatedAfterShot) {
        return fail("player update should be marked complete");
    }

    state.players.currentPlayer = 0;
    state.players.nextPlayer = 1;
    state.players.illegalShot = false;
    state.players.updatedAfterShot = false;
    state.players.shotTaken = true;

    billiardgl::updatePlayerAfterShot(state);
    if (state.players.currentPlayer != 1) {
        return fail("next player mismatch should switch player");
    }

    state.players.currentPlayer = 0;
    state.players.nextPlayer = 0;
    state.players.illegalShot = true;
    state.players.updatedAfterShot = false;
    state.players.shotTaken = true;

    billiardgl::updatePlayerAfterShot(state);
    if (state.players.currentPlayer != 1) {
        return fail("illegal shot should switch player");
    }

    billiardgl::GameState assignmentState;
    assignmentState.players.currentPlayer = 0;
    assignmentState.players.nextPlayer = 1;
    billiardgl::assignPlayerBallTypeForPocketedObjectBall(assignmentState, 9);
    if (assignmentState.players.assignedBallType[0] != 1 || assignmentState.players.assignedBallType[1] != 0) {
        return fail("first pocketed stripe should assign stripes to current player");
    }
    if (assignmentState.players.nextPlayer != 0) {
        return fail("first valid pocket should let current player continue");
    }

    billiardgl::GameState wrongTypeState;
    wrongTypeState.players.currentPlayer = 0;
    wrongTypeState.players.assignedBallType[0] = 0;
    wrongTypeState.players.assignedBallType[1] = 1;
    wrongTypeState.players.firstPocketedObjectBall = false;
    billiardgl::assignPlayerBallTypeForPocketedObjectBall(wrongTypeState, 9);
    if (!wrongTypeState.players.illegalShot) {
        return fail("pocketing opponent type should be illegal");
    }

    return EXIT_SUCCESS;
}
