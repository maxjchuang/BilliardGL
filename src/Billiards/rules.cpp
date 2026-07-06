#include "rules.h"

namespace billiardgl {

void assignPlayerBallTypeForPocketedObjectBall(GameState& state, int ballIndex)
{
    if (ballIndex <= 0 || ballIndex == 8) {
        return;
    }

    const int current = state.players.currentPlayer;
    const int other = 1 - current;
    const int pocketedType = ballIndex > 8 ? 1 : 0;

    if (state.players.firstPocketedObjectBall) {
        state.players.assignedBallType[current] = pocketedType;
        state.players.assignedBallType[other] = 1 - pocketedType;
        state.players.firstPocketedObjectBall = false;
        state.players.nextPlayer = current;
        return;
    }

    if (state.players.assignedBallType[current] == pocketedType) {
        state.players.nextPlayer = current;
    } else {
        state.players.illegalShot = true;
    }
}

void updatePlayerAfterShot(GameState& state)
{
    if (state.players.updatedAfterShot) {
        return;
    }
    if (state.transitionPerspective || !state.players.shotTaken) {
        return;
    }
    if (state.players.illegalShot || state.players.nextPlayer != state.players.currentPlayer) {
        state.players.currentPlayer = 1 - state.players.currentPlayer;
    }
    state.players.updatedAfterShot = true;
}

}  // namespace billiardgl
