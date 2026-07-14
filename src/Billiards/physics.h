#pragma once

#include "game_state.h"
#include "cushion_contact.h"
#include "physics_telemetry.h"
#include "physics_profile.h"

namespace billiardgl {

bool collideBalls(BallState& first, BallState& second);
bool collideBalls(BallState& first, BallState& second,
    const PhysicsProfile& profile);
void collideWithTableEdge(BallState& ball);
CushionContactResult collideWithTableEdge(
    BallState& ball, const PhysicsProfile& profile);
bool isInPocketMouth(const BallState& ball);
bool isInPocket(const BallState& ball);
bool updatePocketedBall(GameState& state, int ballIndex);
PhysicsStepTelemetry updatePhysics(GameState& state, float timeStep);
PhysicsStepTelemetry updatePhysics(
    GameState& state, float timeStep, const PhysicsProfile& profile);
PhysicsStepTelemetry updatePhysics(
    GameState& state, float timeStep, const PhysicsProfile& profile,
    PhysicsBoundaryMode boundaryMode);

}  // namespace billiardgl
