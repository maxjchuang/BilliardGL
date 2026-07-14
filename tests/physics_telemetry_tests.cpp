#include "game_state.h"
#include "physics_telemetry.h"
#include "surface_motion.h"
#include "game_runtime.h"
#include "shot.h"

#include <cmath>
#include <cstdlib>
#include <iostream>

namespace {

void expect(bool value, const char* message)
{
    if (!value) {
        std::cerr << message << '\n';
        std::exit(1);
    }
}

bool close(double a, double b, double epsilon = 1e-6)
{
    return std::fabs(a - b) <= epsilon;
}

}  // namespace

int main()
{
    billiardgl::GameState before;
    billiardgl::initializeBalls(before);
    for (int i = 1; i < billiardgl::kBallCount; ++i) {
        before.balls[i].pocketed = true;
    }
    billiardgl::setBallVelocity(before.balls[0], 100.0f, 0.0f, 0.0f);
    before.balls[0].speed = 100.0f;
    before.balls[0].angularVelocity = billiardgl::Point3{0.0f, 2.0f, 0.0f};
    before.balls[0].motionState = billiardgl::BallMotionState::Sliding;

    billiardgl::GameState after = before;
    after.balls[0].position.x += 10.0f;
    after.balls[0].velocity.x = 96.0f;
    after.balls[0].speed = 96.0f;
    after.balls[0].angularVelocity.z = 4.0f / billiardgl::kBallRadius;
    after.balls[0].motionState = billiardgl::BallMotionState::Sliding;

    billiardgl::PhysicsStepTelemetry step;
    billiardgl::SurfaceMotionStep transition;
    transition.ballIndex = 0;
    transition.before = billiardgl::BallMotionState::Sliding;
    transition.after = billiardgl::BallMotionState::Rolling;
    transition.transitionTimeSeconds = 0.05f;
    step.surfaceMotion.push_back(transition);
    billiardgl::PhysicsContactRecord contact;
    contact.kind = billiardgl::PhysicsContactKind::BallBall;
    contact.regime = billiardgl::BallBallContactRegime::Slip;
    contact.velocityImpulseApplied = true;
    contact.relativeContactVelocityBeforeCmS = billiardgl::Point3{-100.0f, 0.0f, 20.0f};
    contact.relativeContactVelocityAfterCmS = billiardgl::Point3{90.0f, 0.0f, 5.0f};
    contact.normalRelativeSpeedBeforeCmS = -100.0;
    contact.normalRelativeSpeedAfterCmS = 90.0;
    contact.tangentialImpulseNs = 0.01;
    contact.frictionCoefficient = 0.2;
    contact.kineticEnergyBeforeJ = 1.0;
    contact.kineticEnergyAfterJ = 0.9;
    contact.firstPositionCorrectionCm = billiardgl::Point3{-0.01f, 0.0f, 0.0f};
    contact.secondPositionCorrectionCm = billiardgl::Point3{0.01f, 0.0f, 0.0f};
    step.contacts.push_back(contact);
    const billiardgl::PhysicsFrame frame =
        billiardgl::capturePhysicsFrame(7, 0.7, 0.1f, before, after, step);

    expect(frame.tick == 7 && close(frame.timeSeconds, 0.7), "tick and time");
    expect(close(frame.balls[0].acceleration.x, -40.0), "acceleration derives from velocity delta");
    expect(close(frame.balls[0].angularVelocity.y, 2.0), "angular velocity is authoritative state");
    expect(frame.balls[0].motionState == billiardgl::BallMotionState::Sliding,
        "explicit motion state is traced");
    expect(close(frame.balls[0].contactSlipSpeedCmS, 100.0),
        "contact slip speed is traced");
    expect(close(frame.control.shotPower, before.input.shotPower), "available shot input is recorded");
    expect(close(frame.linearMomentum.x, 0.17 * 0.96), "momentum uses SI units");
    expect(close(frame.translationalKineticEnergyJ, 0.5 * 0.17 * 0.96 * 0.96), "energy uses SI units");
    expect(frame.rotationalKineticEnergyJ > 0.0,
        "rotational kinetic energy is included");
    expect(frame.balls[0].rotationalKineticEnergyJ > 0.0,
        "per-ball rotational kinetic energy is traced");
    expect(close(frame.totalKineticEnergyJ,
        frame.translationalKineticEnergyJ + frame.rotationalKineticEnergyJ),
        "total energy has one definition");
    expect(frame.surfaceTransitions.size() == 1 &&
        frame.surfaceTransitions[0].transitionTimeSeconds == 0.05f,
        "surface transition records survive frame capture");
    expect(frame.contacts.size() == 1 &&
        frame.contacts[0].regime == billiardgl::BallBallContactRegime::Slip &&
        frame.contacts[0].velocityImpulseApplied &&
        close(frame.contacts[0].relativeContactVelocityBeforeCmS.x, -100.0) &&
        close(frame.contacts[0].kineticEnergyAfterJ, 0.9),
        "ball contact diagnostics survive frame capture without reconstruction");

    billiardgl::PhysicsTrace trace(2);
    trace.append(frame);
    trace.append(frame);
    trace.append(frame);
    expect(trace.frames().size() == 2 && trace.droppedFrames() == 1,
        "bounded trace reports drops");

    billiardgl::PhysicsTrace disabled(0);
    disabled.append(frame);
    expect(disabled.frames().empty() && disabled.droppedFrames() == 1,
        "zero-capacity trace drops every frame");
    disabled.clear();
    expect(disabled.droppedFrames() == 0, "clear resets dropped count");

    billiardgl::GameRuntime cueRuntime;
    cueRuntime.setPhysicsTraceEnabled(true);
    const billiardgl::CueImpactInput cue = billiardgl::cueImpactFromShotControls(
        0.0f, 40.0f, cueRuntime.physicsProfile());
    expect(cueRuntime.applyCueImpact(cue).ok && cueRuntime.step(2).ok,
        "applied cue contact should remain independently traceable");
    expect(cueRuntime.physicsTrace().frames()[0].hasCueContactResult &&
        cueRuntime.physicsTrace().frames()[0].cueContactResult.applied,
        "first post-shot frame should contain cue contact telemetry");
    expect(!cueRuntime.physicsTrace().frames()[1].hasCueContactResult,
        "cue contact is an event and should not repeat on later frames");
    const billiardgl::CueContactResult& cueResult =
        cueRuntime.physicsTrace().frames()[0].cueContactResult;
    expect(std::isfinite(cueResult.normalImpulseNs) &&
        std::isfinite(cueResult.tangentialImpulseNs) &&
        std::isfinite(cueResult.inputKineticEnergyJ) &&
        std::isfinite(cueResult.outputKineticEnergyJ),
        "cue contact impulse and energy telemetry should be finite");
    return 0;
}
