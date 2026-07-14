#pragma once

#include "game_state.h"
#include "physics_telemetry.h"
#include "physics_profile.h"

namespace billiardgl {

bool collideBalls(BallState& first, BallState& second);
bool collideBalls(BallState& first, BallState& second,
    const PhysicsProfile& profile);
void collideWithTableEdge(BallState& ball);
bool isInPocketMouth(const BallState& ball);
bool isInPocket(const BallState& ball);
bool updatePocketedBall(GameState& state, int ballIndex);
PhysicsStepTelemetry updatePhysics(GameState& state, float timeStep);
PhysicsStepTelemetry updatePhysics(
    GameState& state, float timeStep, const PhysicsProfile& profile);

}  // namespace billiardgl
