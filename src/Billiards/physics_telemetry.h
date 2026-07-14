#pragma once

#include "game_state.h"
#include "cue_impact.h"
#include "physics_profile.h"
#include "surface_motion.h"

#include <array>
#include <cstddef>
#include <cstdint>
#include <deque>
#include <vector>

namespace billiardgl {

constexpr float kDefaultBallMassKg = 0.17f;

enum class PhysicsContactKind {
    BallBall,
    Rail,
    Pocket
};

struct PhysicsContactRecord {
    PhysicsContactKind kind = PhysicsContactKind::BallBall;
    int firstBall = -1;
    int secondBall = -1;
    Point3 normal;
    double normalImpulseNs = 0.0;
    double penetrationCm = 0.0;
};

struct PhysicsBallSample {
    Point3 position;
    Point3 velocity;
    Point3 acceleration;
    Point3 angularVelocity;
    float speed = 0.0f;
    bool pocketed = false;
    BallMotionState motionState = BallMotionState::Stationary;
    float contactSlipSpeedCmS = 0.0f;
};

struct PhysicsControlSample {
    float aimYaw = 0.0f;
    float shotPower = 0.0f;
    bool shotTaken = false;
};

struct PhysicsStepTelemetry {
    std::vector<PhysicsContactRecord> contacts;
    double maximumPenetrationCm = 0.0;
};

struct PhysicsFrame {
    std::uint64_t tick = 0;
    double timeSeconds = 0.0;
    float deltaSeconds = 0.0f;
    std::array<PhysicsBallSample, kBallCount> balls;
    PhysicsControlSample control;
    std::vector<PhysicsContactRecord> contacts;
    Point3 linearMomentum;
    double translationalKineticEnergyJ = 0.0;
    double rotationalKineticEnergyJ = 0.0;
    double totalKineticEnergyJ = 0.0;
    double maximumPenetrationCm = 0.0;
    bool hasCueImpactInput = false;
    CueImpactInput cueImpactInput;
    std::string physicsProfileId;
};

Point3 translationalMomentumKgMps(
    const GameState& state, float ballMassKg = kDefaultBallMassKg);
double translationalKineticEnergyJ(
    const GameState& state, float ballMassKg = kDefaultBallMassKg);
PhysicsFrame capturePhysicsFrame(std::uint64_t tick, double timeSeconds, float deltaSeconds,
    const GameState& before, const GameState& after, const PhysicsStepTelemetry& telemetry);
PhysicsFrame capturePhysicsFrame(std::uint64_t tick, double timeSeconds, float deltaSeconds,
    const GameState& before, const GameState& after, const PhysicsStepTelemetry& telemetry,
    const PhysicsProfile& profile);

class PhysicsTrace {
public:
    explicit PhysicsTrace(std::size_t capacity = 10000) : capacity_(capacity) {}

    void clear();
    void append(const PhysicsFrame& frame);
    const std::deque<PhysicsFrame>& frames() const { return frames_; }
    std::size_t droppedFrames() const { return droppedFrames_; }

private:
    std::size_t capacity_;
    std::size_t droppedFrames_ = 0;
    std::deque<PhysicsFrame> frames_;
};

}  // namespace billiardgl
