#include "physics_telemetry.h"

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
    const GameState& before, const GameState& after, const PhysicsStepTelemetry& telemetry)
{
    PhysicsFrame frame;
    frame.tick = tick;
    frame.timeSeconds = timeSeconds;
    frame.deltaSeconds = deltaSeconds;
    frame.contacts = telemetry.contacts;
    frame.maximumPenetrationCm = telemetry.maximumPenetrationCm;
    frame.linearMomentum = translationalMomentumKgMps(after);
    frame.translationalKineticEnergyJ = translationalKineticEnergyJ(after);
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
    }
    return frame;
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
