#pragma once

#include "game_state.h"

namespace billiardgl {

void assignPlayerBallTypeForPocketedObjectBall(GameState& state, int ballIndex);
void updatePlayerAfterShot(GameState& state);

}  // namespace billiardgl
