#pragma once

#include "ball_ball_contact.h"
#include "game_state.h"
#include "cue_impact.h"
#include "cue_contact.h"
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
    BallBallContactRegime regime = BallBallContactRegime::NoContact;
    bool velocityImpulseApplied = false;
    Point3 contactTangent;
    Point3 firstContactArmCm;
    Point3 secondContactArmCm;
    Point3 relativeContactVelocityBeforeCmS;
    Point3 relativeContactVelocityAfterCmS;
    Point3 impulseOnSecondNs;
    Point3 firstPositionCorrectionCm;
    Point3 secondPositionCorrectionCm;
    double normalRelativeSpeedBeforeCmS = 0.0;
    double normalRelativeSpeedAfterCmS = 0.0;
    double tangentialImpulseNs = 0.0;
    double frictionCoefficient = 0.0;
    double kineticEnergyBeforeJ = 0.0;
    double kineticEnergyAfterJ = 0.0;
    double positionSlopCm = 0.0;
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
    double rotationalKineticEnergyJ = 0.0;
};

struct PhysicsControlSample {
    float aimYaw = 0.0f;
    float shotPower = 0.0f;
    bool shotTaken = false;
};

struct PhysicsStepTelemetry {
    std::vector<PhysicsContactRecord> contacts;
    std::vector<SurfaceMotionStep> surfaceMotion;
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
    bool hasCueContactResult = false;
    CueContactResult cueContactResult;
    std::string physicsProfileId;
    std::vector<SurfaceMotionStep> surfaceTransitions;
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
