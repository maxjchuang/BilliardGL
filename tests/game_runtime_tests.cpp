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

bool closeEnough(float lhs, float rhs)
{
    return std::fabs(lhs - rhs) < 0.0001f;
}

void applyShot(billiardgl::GameRuntime& runtime)
{
    billiardgl::GameAction aim;
    aim.type = billiardgl::ActionType::SetAimYaw;
    aim.first = 0.0f;
    expect(runtime.dispatch(aim).ok, "setting aim should succeed");

    billiardgl::GameAction power;
    power.type = billiardgl::ActionType::SetShotPower;
    power.first = 40.0f;
    expect(runtime.dispatch(power).ok, "setting power should succeed");

    billiardgl::GameAction shoot;
    shoot.type = billiardgl::ActionType::Shoot;
    expect(runtime.dispatch(shoot).ok, "shooting should succeed");
}

}  // namespace

int main()
{
    billiardgl::GameRuntime first;
    billiardgl::GameRuntime second;

    expect(first.tick() == 0, "new runtime should start at tick zero");
    expect(first.state().balls[0].position.z < 0.0f, "reset should install the cue ball");

    applyShot(first);
    applyShot(second);
    expect(first.state().balls[0].velocity.x > 39.9f, "shot should use normal aim logic");
    expect(first.hasCueContactResult() && first.cueContactResult().applied,
        "player shot should retain the authoritative cue contact result");

    billiardgl::GameRuntime explicitImpact;
    const billiardgl::CueImpactInput equivalent = billiardgl::cueImpactFromShotControls(
        0.0f, 40.0f, explicitImpact.physicsProfile());
    expect(explicitImpact.applyCueImpact(equivalent).ok,
        "equivalent explicit cue input should execute");
    expect(first.state().balls[0].velocity.x == explicitImpact.state().balls[0].velocity.x &&
        first.state().balls[0].velocity.y == explicitImpact.state().balls[0].velocity.y &&
        first.state().balls[0].velocity.z == explicitImpact.state().balls[0].velocity.z &&
        first.state().balls[0].angularVelocity.x == explicitImpact.state().balls[0].angularVelocity.x &&
        first.state().balls[0].angularVelocity.y == explicitImpact.state().balls[0].angularVelocity.y &&
        first.state().balls[0].angularVelocity.z == explicitImpact.state().balls[0].angularVelocity.z,
        "UI and explicit input should produce byte-equal motion state");

    billiardgl::GameRuntime rejectedImpact;
    billiardgl::CueImpactInput vertical = equivalent;
    vertical.tipOffsetRadius = {{0.0, 0.7}};
    vertical.tipOffsetCm = {{0.0, 0.7 * billiardgl::kBallRadius}};
    const billiardgl::BallState beforeRejected = rejectedImpact.state().balls[0];
    const billiardgl::ActionResult rejection = rejectedImpact.applyCueImpact(vertical);
    expect(!rejection.ok && std::string(rejection.errorCode) ==
        "vertical_ball_impulse_requires_3d", "vertical slip should expose a stable action error");
    expect(rejectedImpact.state().balls[0].velocity.x == beforeRejected.velocity.x &&
        rejectedImpact.state().balls[0].angularVelocity.z == beforeRejected.angularVelocity.z,
        "rejected contact should not mutate ball state");
    billiardgl::CueImpactInput miscue = equivalent;
    miscue.tipOffsetRadius = {{0.81, 0.0}};
    miscue.tipOffsetCm = {{0.81 * billiardgl::kBallRadius, 0.0}};
    billiardgl::GameRuntime miscueRuntime;
    expect(miscueRuntime.applyCueImpact(miscue).ok &&
        miscueRuntime.hasCueContactResult() &&
        miscueRuntime.cueContactResult().regime == billiardgl::CueContactRegime::Miscue &&
        !miscueRuntime.cueContactResult().applied,
        "miscue should be a traceable shot attempt without a ball impulse");
    expect(miscueRuntime.state().balls[0].velocity.x == 0.0f,
        "miscue outcome should leave the ball unchanged");

    expect(first.step(5).ok, "stepping should succeed");
    expect(second.step(5).ok, "second stepping should succeed");
    expect(first.tick() == 5, "step should advance the exact tick count");
    expect(closeEnough(first.state().balls[0].position.x, second.state().balls[0].position.x),
        "same command sequence should produce the same position");

    billiardgl::GameRuntime traced;
    traced.setPhysicsTraceEnabled(true);
    expect(traced.step(3).ok, "traced stepping should succeed");
    expect(traced.physicsTrace().frames().size() == 3, "one trace frame per physics tick");
    expect(traced.physicsTrace().frames()[0].tick == 1, "first completed tick is one");
    expect(traced.physicsTrace().frames()[2].tick == 3, "third completed tick is three");
    traced.setPhysicsTraceEnabled(false);
    expect(traced.step(1).ok, "untraced stepping should succeed");
    expect(traced.physicsTrace().frames().size() == 3, "disabled trace does not append");
    traced.clearPhysicsTrace();
    expect(traced.physicsTrace().frames().empty(), "clear removes retained frames");
    traced.setPhysicsTraceEnabled(true);
    traced.step(1);
    traced.reset();
    expect(!traced.physicsTraceEnabled() && traced.physicsTrace().frames().empty(),
        "reset clears and disables tracing");

    billiardgl::GameRuntime profiled;
    expect(profiled.physicsProfile().id == "chinese_pool_full_game_v1",
        "runtime should own the production profile by default");
    billiardgl::PhysicsProfile experiment =
        billiardgl::defaultChinesePoolPhysicsProfile();
    experiment.id = "experiment_surface_v1";
    experiment.ball.massKg = 0.205f;
    const billiardgl::GameState replacement = profiled.state();
    expect(profiled.replaceStateForScenario(replacement, experiment).ok,
        "valid profile should apply atomically");
    expect(profiled.physicsProfile().id == "experiment_surface_v1",
        "runtime should retain the scenario profile");

    billiardgl::PhysicsProfile invalid = experiment;
    invalid.ball.radiusCm = 0.0f;
    expect(!profiled.replaceStateForScenario(replacement, invalid).ok,
        "invalid profile should be rejected");
    expect(profiled.physicsProfile().id == "experiment_surface_v1",
        "failed replacement should preserve the previous profile");

    billiardgl::GameState before = profiled.state();
    for (int index = 1; index < billiardgl::kBallCount; ++index) {
        before.balls[index].pocketed = true;
    }
    billiardgl::GameState after = before;
    after.balls[0].velocity.x = 100.0f;
    const billiardgl::PhysicsFrame frame = billiardgl::capturePhysicsFrame(
        1, 0.1, 0.1f, before, after,
        billiardgl::PhysicsStepTelemetry{}, experiment);
    expect(frame.physicsProfileId == "experiment_surface_v1",
        "trace should include the active profile ID");
    expect(closeEnough(frame.linearMomentum.x, 0.205f),
        "trace momentum should use the active profile mass");

    billiardgl::GameState rollingState;
    billiardgl::initializeBalls(rollingState);
    for (int index = 1; index < billiardgl::kBallCount; ++index) {
        rollingState.balls[index].pocketed = true;
    }
    rollingState.balls[0].position.z = 20.0f;
    rollingState.balls[0].velocity.x = 20.0f;
    rollingState.balls[0].speed = 20.0f;
    rollingState.balls[0].angularVelocity.z =
        -20.0f / billiardgl::kBallRadius;
    rollingState.balls[0].motionState = billiardgl::BallMotionState::Rolling;
    billiardgl::PhysicsProfile lowResistance =
        billiardgl::defaultChinesePoolPhysicsProfile();
    lowResistance.id = "rolling_resistance_4";
    lowResistance.surface.rollingResistanceAccelerationCmS2 = 4.0f;
    billiardgl::PhysicsProfile highResistance = lowResistance;
    highResistance.id = "rolling_resistance_12_5";
    highResistance.surface.rollingResistanceAccelerationCmS2 = 12.5f;
    billiardgl::GameRuntime lowRuntime;
    billiardgl::GameRuntime highRuntime;
    expect(lowRuntime.replaceStateForScenario(
        rollingState, lowResistance).ok, "low-resistance profile installs");
    expect(highRuntime.replaceStateForScenario(
        rollingState, highResistance).ok, "high-resistance profile installs");
    expect(lowRuntime.step(1).ok && highRuntime.step(1).ok,
        "profiled runtimes step independently");
    expect(lowRuntime.state().balls[0].position.x >
        highRuntime.state().balls[0].position.x,
        "runtime-owned rolling resistance changes production motion");
    billiardgl::GameRuntime productionRuntime;
    expect(productionRuntime.physicsProfile().id == "chinese_pool_full_game_v1",
        "a fresh runtime retains the registered production profile");
    return 0;
}
