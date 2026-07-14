#pragma once

#include "game_state.h"
#include "physics_telemetry.h"
#include "physics_profile.h"

namespace billiardgl {

void applyFrictionAndMove(BallState& ball, float timeStep, float frictionAcceleration);
bool collideBalls(BallState& first, BallState& second);
void collideWithTableEdge(BallState& ball);
bool isInPocketMouth(const BallState& ball);
bool isInPocket(const BallState& ball);
bool updatePocketedBall(GameState& state, int ballIndex);
PhysicsStepTelemetry updatePhysics(GameState& state, float timeStep);
PhysicsStepTelemetry updatePhysics(
    GameState& state, float timeStep, const PhysicsProfile& profile);

}  // namespace billiardgl
