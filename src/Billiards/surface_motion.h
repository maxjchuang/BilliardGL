#pragma once

#include "game_state.h"
#include "physics_profile.h"

#include <vector>

namespace billiardgl {

constexpr float kStandardGravityCmS2 = 980.665f;

enum class BallMotionState : int {
    Stationary,
    Sliding,
    Rolling
};

// One exact constant-acceleration segment emitted by the same surface
// integrator that mutates the ball.  Offsets are local until physics rebases
// them onto the containing whole tick.
struct SurfaceMotionAnalyticSegment {
    int ballIndex = -1;
    std::uint64_t trajectoryGeneration = 0;
    int segmentOrdinal = -1;
    BallMotionState motionState = BallMotionState::Stationary;
    double wholeTickStartOffsetSeconds = 0.0;
    double durationSeconds = 0.0;
    Point3 startPositionCm;
    Point3 endPositionCm;
    Point3 startVelocityCmS;
    Point3 constantAccelerationCmS2;
    bool analyticPathComplete = false;
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
    std::vector<SurfaceMotionAnalyticSegment> analyticSegments;
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
