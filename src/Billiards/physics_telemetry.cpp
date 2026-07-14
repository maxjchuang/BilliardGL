#include "physics_telemetry.h"

#include <cmath>

namespace billiardgl {
namespace {

constexpr double kCentimetersPerMeter = 100.0;

Point3 accelerationBetween(const Point3& before, const Point3& after, float deltaSeconds)
{
    if (deltaSeconds <= 0.0f) {
        return Point3{};
    }
    return Point3{
        (after.x - before.x) / deltaSeconds,
        (after.y - before.y) / deltaSeconds,
        (after.z - before.z) / deltaSeconds};
}

}  // namespace

const char* physicsStepStatusName(PhysicsStepStatus status)
{
    return status == PhysicsStepStatus::Succeeded ? "succeeded" : "failed";
}

const char* physicsFailureCodeName(PhysicsFailureCode code)
{
    switch (code) {
    case PhysicsFailureCode::None: return "none";
    case PhysicsFailureCode::EventBudget: return "event_budget";
    case PhysicsFailureCode::InvalidControls: return "invalid_controls";
    case PhysicsFailureCode::IslandLimit: return "island_limit";
    case PhysicsFailureCode::ResidualLimit: return "residual_limit";
    case PhysicsFailureCode::PenetrationLimit: return "penetration_limit";
    case PhysicsFailureCode::NonfiniteState: return "nonfinite_state";
    case PhysicsFailureCode::NonfiniteEnergy: return "nonfinite_energy";
    case PhysicsFailureCode::PassiveEnergyCreation:
        return "passive_energy_creation";
    case PhysicsFailureCode::ContradictoryTopology:
        return "contradictory_topology";
    }
    return "nonfinite_state";
}

Point3 translationalMomentumKgMps(const GameState& state, float ballMassKg)
{
    Point3 momentum;
    for (const BallState& ball : state.balls) {
        if (ball.pocketed) {
            continue;
        }
        momentum.x += static_cast<float>(ballMassKg * ball.velocity.x / kCentimetersPerMeter);
        momentum.y += static_cast<float>(ballMassKg * ball.velocity.y / kCentimetersPerMeter);
        momentum.z += static_cast<float>(ballMassKg * ball.velocity.z / kCentimetersPerMeter);
    }
    return momentum;
}

double translationalKineticEnergyJ(const GameState& state, float ballMassKg)
{
    double energy = 0.0;
    for (const BallState& ball : state.balls) {
        if (ball.pocketed) {
            continue;
        }
        const double vx = ball.velocity.x / kCentimetersPerMeter;
        const double vy = ball.velocity.y / kCentimetersPerMeter;
        const double vz = ball.velocity.z / kCentimetersPerMeter;
        energy += 0.5 * ballMassKg * (vx * vx + vy * vy + vz * vz);
    }
    return energy;
}

PhysicsFrame capturePhysicsFrame(std::uint64_t tick, double timeSeconds, float deltaSeconds,
    const GameState& before, const GameState& after, const PhysicsStepTelemetry& telemetry,
    const PhysicsProfile& profile, PhysicsBoundaryMode boundaryMode)
{
    PhysicsFrame frame;
    frame.tick = tick;
    frame.timeSeconds = timeSeconds;
    frame.deltaSeconds = deltaSeconds;
    frame.contacts = telemetry.contacts;
    frame.solverEvents = telemetry.solverEvents;
    for (const SurfaceMotionStep& surface : telemetry.surfaceMotion) {
        if (surface.before != surface.after ||
            surface.transitionTimeSeconds >= 0.0f) {
            frame.surfaceTransitions.push_back(surface);
        }
    }
    frame.maximumPenetrationCm = telemetry.maximumPenetrationCm;
    frame.stepStatus = telemetry.stepStatus;
    frame.failureCode = telemetry.failureCode;
    frame.failingEventId = telemetry.failingEventId;
    frame.failingIslandId = telemetry.failingIslandId;
    frame.linearMomentum = translationalMomentumKgMps(after, profile.ball.massKg);
    frame.translationalKineticEnergyJ =
        translationalKineticEnergyJ(after, profile.ball.massKg);
    for (const BallState& ball : after.balls) {
        if (!ball.pocketed) {
            frame.rotationalKineticEnergyJ +=
                rotationalKineticEnergyJ(ball, profile.ball);
        }
    }
    frame.totalKineticEnergyJ =
        frame.translationalKineticEnergyJ + frame.rotationalKineticEnergyJ;
    frame.physicsProfileId = profile.id;
    frame.boundaryMode = boundaryMode;
    frame.control.aimYaw = after.aim.yaw;
    frame.control.shotPower = after.input.shotPower;
    frame.control.shotTaken = after.players.shotTaken;

    for (int index = 0; index < kBallCount; ++index) {
        const BallState& beforeBall = before.balls[index];
        const BallState& afterBall = after.balls[index];
        PhysicsBallSample& sample = frame.balls[index];
        sample.position = afterBall.position;
        sample.velocity = afterBall.velocity;
        sample.acceleration = accelerationBetween(
            beforeBall.velocity, afterBall.velocity, deltaSeconds);
        sample.angularVelocity = afterBall.angularVelocity;
        sample.speed = afterBall.speed;
        sample.pocketed = afterBall.pocketed;
        sample.motionState = afterBall.motionState;
        const Point3 slip = surfaceContactSlipVelocity(
            afterBall, profile.ball.radiusCm);
        sample.contactSlipSpeedCmS = std::sqrt(
            slip.x * slip.x + slip.z * slip.z);
        sample.rotationalKineticEnergyJ =
            rotationalKineticEnergyJ(afterBall, profile.ball);
    }
    return frame;
}

PhysicsFrame capturePhysicsFrame(std::uint64_t tick, double timeSeconds, float deltaSeconds,
    const GameState& before, const GameState& after, const PhysicsStepTelemetry& telemetry)
{
    return capturePhysicsFrame(
        tick, timeSeconds, deltaSeconds, before, after, telemetry,
        defaultChinesePoolPhysicsProfile());
}

void PhysicsTrace::clear()
{
    frames_.clear();
    droppedFrames_ = 0;
}

void PhysicsTrace::append(const PhysicsFrame& frame)
{
    if (capacity_ == 0) {
        ++droppedFrames_;
        return;
    }
    if (frames_.size() == capacity_) {
        frames_.pop_front();
        ++droppedFrames_;
    }
    frames_.push_back(frame);
}

}  // namespace billiardgl
