#pragma once

#include "game_state.h"

namespace billiardgl {

void applyFrictionAndMove(BallState& ball, float timeStep, float frictionAcceleration);
bool collideBalls(BallState& first, BallState& second);
void collideWithTableEdge(BallState& ball);
bool isInPocketMouth(const BallState& ball);
bool isInPocket(const BallState& ball);
bool updatePocketedBall(GameState& state, int ballIndex);
void updatePhysics(GameState& state, float timeStep);

}  // namespace billiardgl
