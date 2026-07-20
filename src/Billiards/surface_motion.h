#pragma once

#include "game_state.h"
#include "physics_profile.h"

namespace billiardgl {

constexpr float kStandardGravityCmS2 = 980.665f;

enum class BallMotionState : int {
    Stationary,
    Sliding,
    Rolling
};

struct SurfaceMotionStep {
    int ballIndex = -1;
    BallMotionState before = BallMotionState::Stationary;
    BallMotionState after = BallMotionState::Stationary;
    float initialSlipSpeedCmS = 0.0f;
    float finalSlipSpeedCmS = 0.0f;
    float transitionTimeSeconds = -1.0f;
    Point3 frictionAccelerationCmS2;
    Point3 angularAccelerationRadS2;
};

const char* ballMotionStateName(BallMotionState state);
Point3 surfaceContactSlipVelocity(const BallState& ball, float radiusCm);
BallMotionState classifySurfaceMotion(
    const BallState& ball, const BallProperties& ballProperties,
    const SurfaceProperties& surface);
double rotationalKineticEnergyJ(
    const BallState& ball, const BallProperties& ballProperties);
SurfaceMotionStep advanceSurfaceMotion(
    BallState& ball, float deltaSeconds, const BallProperties& ballProperties,
    const SurfaceProperties& surface);

}  // namespace billiardgl
