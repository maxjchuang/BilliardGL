#include "physics.h"
#include "physics_profile.h"
#include "game_runtime.h"
#include "cue_contact.h"
#include "shot.h"
#include "surface_motion.h"
#include "table_departure_geometry.h"
#include "ball_ball_contact.h"
#include "cushion_contact.h"
#include "physics_telemetry_exact_test_helpers.h"

#include <chrono>
#include <algorithm>
#include <array>
#include <cmath>
#include <cstring>
#include <cstdlib>
#include <fstream>
#include <iostream>
#include <sstream>
#include <string>
#include <vector>

namespace {

void expect(bool condition, const char* message)
{
    if (!condition) {
        std::cerr << message << '\n';
        std::exit(EXIT_FAILURE);
    }
}

bool close(float first, float second, float tolerance = 0.001f)
{
    return std::fabs(first - second) <= tolerance;
}

bool sameFloatBits(float first, float second)
{
    unsigned int left = 0;
    unsigned int right = 0;
    std::memcpy(&left, &first, sizeof(left));
    std::memcpy(&right, &second, sizeof(right));
    return left == right;
}

bool sameTransactionalState(const billiardgl::GameState& first,
    const billiardgl::GameState& second)
{
    if (first.nextPocketCaptureSequence != second.nextPocketCaptureSequence ||
        first.pocketedBallCount != second.pocketedBallCount ||
        first.ballsMoving != second.ballsMoving ||
        first.events.ballCollision != second.events.ballCollision ||
        first.events.railCollision != second.events.railCollision ||
        first.events.ballPocketed != second.events.ballPocketed ||
        first.events.cueBallPocketed != second.events.cueBallPocketed ||
        first.events.eightBallPocketed != second.events.eightBallPocketed ||
        first.events.shotEnded != second.events.shotEnded)
        return false;
    for (int index = 0; index < billiardgl::kBallCount; ++index) {
        const auto& a = first.balls[index];
        const auto& b = second.balls[index];
        if (!sameFloatBits(a.position.x, b.position.x) ||
            !sameFloatBits(a.position.y, b.position.y) ||
            !sameFloatBits(a.position.z, b.position.z) ||
            !sameFloatBits(a.velocity.x, b.velocity.x) ||
            !sameFloatBits(a.velocity.y, b.velocity.y) ||
            !sameFloatBits(a.velocity.z, b.velocity.z) ||
            !sameFloatBits(a.angularVelocity.x, b.angularVelocity.x) ||
            !sameFloatBits(a.angularVelocity.y, b.angularVelocity.y) ||
            !sameFloatBits(a.angularVelocity.z, b.angularVelocity.z) ||
            !sameFloatBits(a.rotationAxis.x, b.rotationAxis.x) ||
            !sameFloatBits(a.rotationAxis.y, b.rotationAxis.y) ||
            !sameFloatBits(a.rotationAxis.z, b.rotationAxis.z) ||
            !sameFloatBits(a.rotationAngle, b.rotationAngle) ||
            !sameFloatBits(a.speed, b.speed) ||
            a.motionState != b.motionState || a.pocketed != b.pocketed ||
            a.outOfPlay != b.outOfPlay ||
            a.trajectoryGeneration != b.trajectoryGeneration ||
            a.physicalDomain != b.physicalDomain ||
            a.pocketInteraction.phase != b.pocketInteraction.phase ||
            a.pocketInteraction.pocketId != b.pocketInteraction.pocketId ||
            a.pocketInteraction.captureSequence !=
                b.pocketInteraction.captureSequence)
            return false;
    }
    return true;
}

void expectSameTickCompositionStaticBoundary()
{
#ifndef BILLIARDGL_SOURCE_ROOT
#define BILLIARDGL_SOURCE_ROOT "."
#endif
    const std::string root = BILLIARDGL_SOURCE_ROOT;
    std::ifstream physics(root + "/src/Billiards/physics.cpp");
    expect(physics.good(), "C12 physics composition source must be readable");
    std::ostringstream physicsText;
    physicsText << physics.rdbuf();
    const std::string source = physicsText.str();
    expect(source.find("shot_fact") == std::string::npos &&
        source.find("modern_product") == std::string::npos &&
        source.find("renderer") == std::string::npos &&
        source.find("rules_decision") == std::string::npos,
        "C12 private composition seam must not name rules/product/UI/render consumers");
    std::ifstream cmake(root + "/CMakeLists.txt");
    expect(cmake.good(), "C12 CMake source must be readable");
    std::ostringstream cmakeText;
    cmakeText << cmake.rdbuf();
    expect(cmakeText.str().find("legacy_d6_same_tick") == std::string::npos,
        "C12 same-tick correction must not add a target or public build seam");
}

billiardgl::BallState interactiveShot(float power,
    const billiardgl::PhysicsProfile& profile)
{
    billiardgl::BallState ball;
    const billiardgl::CueContactResult result = billiardgl::resolveCueContact(
        ball, billiardgl::cueImpactFromShotControls(0.0f, power, profile),
        profile.ball, profile.cue);
    expect(result.applied, "centered interactive shot must apply");
    return ball;
}

billiardgl::GameState isolatedCueBall()
{
    billiardgl::GameState state;
    billiardgl::initializeBalls(state);
    for (int index = 1; index < billiardgl::kBallCount; ++index) {
        state.balls[index].pocketed = true;
    }
    state.balls[0].position = billiardgl::Point3{
        0.0f, billiardgl::kTableHeight + billiardgl::kBallRadius, 30.0f};
    return state;
}

billiardgl::GameState simulate(float stepSeconds, float durationSeconds,
    const billiardgl::Point3& position, const billiardgl::Point3& velocity)
{
    billiardgl::PhysicsProfile profile =
        billiardgl::interactiveChinesePoolPhysicsProfile();
    profile.solver.timeStepSeconds = stepSeconds;
    billiardgl::GameState state = isolatedCueBall();
    state.balls[0].position = position;
    billiardgl::setBallVelocity(state.balls[0],
        velocity.x, velocity.y, velocity.z);
    state.balls[0].speed = std::hypot(velocity.x, velocity.z);
    state.ballsMoving = state.balls[0].speed > 0.0f;
    const int ticks = static_cast<int>(
        std::lround(durationSeconds / stepSeconds));
    for (int tick = 0; tick < ticks; ++tick) {
        const billiardgl::PhysicsStepTelemetry result =
            billiardgl::updatePhysics(state, stepSeconds, profile);
        expect(result.stepStatus == billiardgl::PhysicsStepStatus::Succeeded,
            "interactive convergence simulation must remain valid");
    }
    return state;
}

}  // namespace

int main()
{
    expectSameTickCompositionStaticBoundary();
    const billiardgl::PhysicsProfile interactive =
        billiardgl::interactiveChinesePoolPhysicsProfile();
    billiardgl::BallState defaultShot = interactiveShot(60.0f, interactive);
    expect(close(defaultShot.speed, 75.0f, 0.001f),
        "default power must launch the cue ball at 75 cm/s");
    const billiardgl::BallState maximumShot = interactiveShot(200.0f, interactive);
    expect(close(maximumShot.speed, 1000.0f, 0.01f),
        "maximum power must reach the measured break-speed range");
    expect(close(maximumShot.angularVelocity.z,
        -maximumShot.speed / interactive.ball.radiusCm, 0.01f),
        "maximum break must launch with roll-matched forward spin");

    int freeRunTicks = 0;
    while (billiardgl::classifySurfaceMotion(defaultShot,
               interactive.ball, interactive.surface) !=
            billiardgl::BallMotionState::Stationary &&
        freeRunTicks < 1200) {
        billiardgl::advanceSurfaceMotion(defaultShot,
            interactive.solver.timeStepSeconds,
            interactive.ball, interactive.surface);
        ++freeRunTicks;
    }
    expect(freeRunTicks < 1200,
        "default shot must settle within ten seconds");
    expect(close(defaultShot.position.x, 121.8f, 0.2f),
        "default power free run must reach the rack-facing playable gap");
    expect(freeRunTicks >= 525 && freeRunTicks <= 530,
        "default power must settle in about 4.4 seconds");

    billiardgl::BallState striker;
    striker.position.x = 0.0f;
    striker.velocity.x = 100.0f;
    striker.speed = 100.0f;
    billiardgl::BallState objectBall;
    objectBall.position.x = 2.0f * interactive.ball.radiusCm;
    const billiardgl::BallBallContactResult ballContact =
        billiardgl::resolveBallBallContact(striker, objectBall,
            interactive.ball, interactive.ball);
    expect(ballContact.velocityImpulseApplied &&
        close(striker.velocity.x, 1.5f, 0.001f) &&
        close(objectBall.velocity.x, 98.5f, 0.001f),
        "head-on ball contact must obey the calibrated restitution");
    expect(ballContact.kineticEnergyAfterJ < ballContact.kineticEnergyBeforeJ,
        "interactive ball contact must dissipate energy");

    billiardgl::BallState railBall;
    railBall.velocity.x = 100.0f;
    railBall.speed = 100.0f;
    railBall.angularVelocity.z = -100.0f / interactive.ball.radiusCm;
    const billiardgl::CushionContactResult railContact =
        billiardgl::resolveCushionContact(railBall,
            billiardgl::Point3{-1.0f, 0.0f, 0.0f}, 0.0,
            interactive.ball, interactive.cushion);
    expect(railContact.velocityImpulseApplied,
        "one metre-per-second rail impact must apply an impulse");
    expect(close(railContact.restitution, 0.9216f, 0.001f),
        "rolling contact speed must drive the calibrated cushion restitution");
    expect(railBall.velocity.x < 0.0f &&
        std::fabs(railBall.velocity.x) < 100.0f,
        "one metre-per-second rail impact must rebound below incident speed");
    expect(railContact.kineticEnergyAfterJ < railContact.kineticEnergyBeforeJ,
        "interactive cushion contact must dissipate energy");

    const billiardgl::Point3 tableCenter{
        0.0f, billiardgl::kTableHeight + billiardgl::kBallRadius, 30.0f};
    const billiardgl::GameState free60 = simulate(
        1.0f / 60.0f, 1.0f, tableCenter, {50.0f, 0.0f, 0.0f});
    const billiardgl::GameState free120 = simulate(
        1.0f / 120.0f, 1.0f, tableCenter, {50.0f, 0.0f, 0.0f});
    const billiardgl::GameState free240 = simulate(
        1.0f / 240.0f, 1.0f, tableCenter, {50.0f, 0.0f, 0.0f});
    expect(close(free120.balls[0].position.x, free240.balls[0].position.x) &&
        close(free120.balls[0].speed, free240.balls[0].speed),
        "120 Hz free motion must converge to the 240 Hz reference");
    expect(close(free60.balls[0].position.x, free120.balls[0].position.x) &&
        close(free60.balls[0].speed, free120.balls[0].speed),
        "free-motion integration must be stable across common fixed steps");

    const billiardgl::Point3 railStart{
        35.0f, billiardgl::kTableHeight + billiardgl::kBallRadius, 30.0f};
    const billiardgl::GameState rail120 = simulate(
        1.0f / 120.0f, 0.5f, railStart, {100.0f, 0.0f, 0.0f});
    const billiardgl::GameState rail240 = simulate(
        1.0f / 240.0f, 0.5f, railStart, {100.0f, 0.0f, 0.0f});
    expect(rail120.balls[0].velocity.x < 0.0f &&
        rail240.balls[0].velocity.x < 0.0f,
        "rail convergence case must include a real rebound");
    expect(close(rail120.balls[0].position.x, rail240.balls[0].position.x,
            0.01f) &&
        close(rail120.balls[0].velocity.x, rail240.balls[0].velocity.x,
            0.01f),
        "120 Hz rail response must converge to the 240 Hz reference");

    const billiardgl::GameState stopped = simulate(
        1.0f / 120.0f, 1.0f, tableCenter, {2.0f, 0.0f, 0.0f});
    expect(stopped.balls[0].motionState ==
            billiardgl::BallMotionState::Stationary &&
        close(stopped.balls[0].speed, 0.0f) && !stopped.ballsMoving,
        "low-speed interactive motion must settle completely");

    billiardgl::GameState spinOnly = isolatedCueBall();
    spinOnly.balls[0].angularVelocity.y = 1.2f;
    spinOnly.ballsMoving = true;
    billiardgl::updatePhysics(spinOnly, 0.05f, interactive);
    expect(close(spinOnly.balls[0].angularVelocity.y, 0.6f) &&
        spinOnly.ballsMoving,
        "residual sidespin keeps interactive physics active while it decays");
    billiardgl::updatePhysics(spinOnly, 0.05f, interactive);
    expect(spinOnly.balls[0].angularVelocity.y == 0.0f &&
        !spinOnly.ballsMoving,
        "interactive physics stops only after residual sidespin reaches zero");

    billiardgl::GameRuntime breakRuntime;
    breakRuntime.setPhysicsTraceEnabled(true);
    const billiardgl::GameState initialRack = breakRuntime.state();
    expect(breakRuntime.replaceStateForScenario(initialRack,
            billiardgl::interactiveChinesePoolPhysicsProfile()).ok,
        "interactive full-game runtime must select the 120 Hz profile");
    billiardgl::GameAction aim;
    aim.type = billiardgl::ActionType::SetAimYaw;
    aim.first = billiardgl::kPi / 2.0f;
    expect(breakRuntime.dispatch(aim).ok,
        "interactive break aim must be accepted");
    billiardgl::GameAction power;
    power.type = billiardgl::ActionType::SetShotPower;
    power.first = 80.0f;
    expect(breakRuntime.dispatch(power).ok,
        "interactive break power must be accepted");
    billiardgl::GameAction shoot;
    shoot.type = billiardgl::ActionType::Shoot;
    expect(breakRuntime.dispatch(shoot).ok &&
        breakRuntime.state().ballsMoving,
        "interactive break must start ball motion");

    const float cameraAngleBefore = breakRuntime.state().camera.angleX;
    billiardgl::GameAction orbit;
    orbit.type = billiardgl::ActionType::OrbitCamera;
    orbit.first = 0.2f;
    orbit.second = 0.1f;
    expect(breakRuntime.dispatch(orbit).ok &&
        breakRuntime.state().camera.angleX != cameraAngleBefore,
        "camera control must remain responsive during interactive motion");

    double executionSeconds = 0.0;
    int breakTicks = 0;
    bool observedZeroTimeProjectionHandoff = false;
    // Exercise projection deterministically; a correctly resolved rack need
    // not happen to begin a later tick with penetration above the slop.
    {
        auto overlap = isolatedCueBall();
        overlap.balls[1].pocketed = false;
        overlap.balls[1].position = overlap.balls[0].position;
        overlap.balls[1].position.x += 2.0f * interactive.ball.radiusCm - 0.02f;
        billiardgl::setBallVelocity(overlap.balls[0], 10.0f, 0.0f, 0.0f);
        const auto before = overlap;
        const auto result = billiardgl::updatePhysics(overlap,
            interactive.solver.timeStepSeconds, interactive);
        expect(result.stepStatus == billiardgl::PhysicsStepStatus::Succeeded,
            "dedicated initial-overlap projection fixture must converge");
        auto projected = before.balls;
        std::array<bool, billiardgl::kBallCount> corrected{};
        for (const auto& event : result.solverEvents) {
            if (event.wholeTickTimeOfImpactSeconds != 0.0) continue;
            for (const auto& receipt : event.positionProjections) {
                auto& p = projected[receipt.ballIndex].position;
                expect(p.x == receipt.beforeCm.x && p.y == receipt.beforeCm.y &&
                    p.z == receipt.beforeCm.z,
                    "fixture projection receipts must form an exact chain");
                p = receipt.afterCm;
                corrected[receipt.ballIndex] = true;
                observedZeroTimeProjectionHandoff = true;
            }
        }
        expect(observedZeroTimeProjectionHandoff,
            "dedicated overlap must produce real zero-time projection receipts");
        for (int i = 0; i < billiardgl::kBallCount; ++i) {
            if (!corrected[i]) continue;
            const auto first = std::find_if(result.analyticSurfaceMotion.begin(),
                result.analyticSurfaceMotion.end(), [i](const auto& s) {
                    return s.ballIndex == i;
                });
            expect(first != result.analyticSurfaceMotion.end() &&
                first->wholeTickStartOffsetSeconds == 0.0 &&
                first->startPositionCm.x == projected[i].position.x &&
                first->startPositionCm.y == projected[i].position.y &&
                first->startPositionCm.z == projected[i].position.z,
                "dedicated projection endpoint must anchor actual free motion");
        }
    }
    bool observedAirborneRelease = false;
    std::vector<double> tickMicroseconds;
    billiardgl::GameState preStepState = breakRuntime.state();
    while (breakRuntime.state().ballsMoving && breakTicks < 20000) {
        preStepState = breakRuntime.state();
        const auto started = std::chrono::steady_clock::now();
        const billiardgl::ActionResult breakStep = breakRuntime.step(1);
        const double tickSeconds = std::chrono::duration<double>(
            std::chrono::steady_clock::now() - started).count();
        executionSeconds += tickSeconds;
        tickMicroseconds.push_back(tickSeconds * 1.0e6);
        if (!breakStep.ok) {
            std::cerr << "interactive break failed at tick " << breakTicks
                      << ": " << breakStep.errorCode << ' '
                      << breakRuntime.latestPhysicsStep().telemetry.failureDetail << '\n';
            for (int ballIndex = 0; ballIndex < billiardgl::kBallCount; ++ballIndex) {
                const auto& ball = preStepState.balls[ballIndex];
                if (!ball.pocketed && (ball.velocity.y != 0.0f ||
                    ball.physicalDomain != billiardgl::PhysicalBallDomain::ClothSupported)) {
                    std::cerr << "  input ball " << ballIndex << " domain="
                              << static_cast<int>(ball.physicalDomain) << " y="
                              << ball.position.y << " vy=" << ball.velocity.y << '\n';
                }
            }
            expect(sameTransactionalState(breakRuntime.state(), preStepState),
                "C09 real same-tick failure must rollback every transactional field");
        }
        expect(breakStep.ok,
            "every 120 Hz break tick must converge without physics failure");
        auto repeatedState = preStepState;
        const auto repeated = billiardgl::updatePhysics(repeatedState,
            interactive.solver.timeStepSeconds, interactive,
            billiardgl::PhysicsBoundaryMode::ProductionTable);
        expect(repeated.stepStatus == billiardgl::PhysicsStepStatus::Succeeded &&
            sameTransactionalState(repeatedState, breakRuntime.state()),
            "identical pre-tick state must reproduce the exact public physics result");
        const auto& recorded = breakRuntime.latestPhysicsStep().telemetry;
        expect(billiardgl::test::exactPhysicsStepTelemetry(repeated, recorded),
            "deterministic replay must reproduce all contact, solver, projection and private evidence");
        expect(repeated.contacts.size() == recorded.contacts.size() &&
            repeated.solverEvents.size() == recorded.solverEvents.size() &&
            repeated.analyticSurfaceMotion.size() == recorded.analyticSurfaceMotion.size(),
            "deterministic replay must preserve contact, solver and trajectory ledger sizes");
        for (std::size_t i = 0; i < repeated.analyticSurfaceMotion.size(); ++i) {
            const auto& a = repeated.analyticSurfaceMotion[i];
            const auto& b = recorded.analyticSurfaceMotion[i];
            expect(a.ballIndex == b.ballIndex && a.segmentOrdinal == b.segmentOrdinal &&
                a.trajectoryGeneration == b.trajectoryGeneration &&
                a.wholeTickStartOffsetSeconds == b.wholeTickStartOffsetSeconds &&
                a.durationSeconds == b.durationSeconds && a.motionState == b.motionState &&
                a.analyticPathComplete == b.analyticPathComplete &&
                a.startPositionCm.x == b.startPositionCm.x &&
                a.startPositionCm.y == b.startPositionCm.y &&
                a.startPositionCm.z == b.startPositionCm.z &&
                a.endPositionCm.x == b.endPositionCm.x &&
                a.endPositionCm.y == b.endPositionCm.y &&
                a.endPositionCm.z == b.endPositionCm.z &&
                a.startVelocityCmS.x == b.startVelocityCmS.x &&
                a.startVelocityCmS.y == b.startVelocityCmS.y &&
                a.startVelocityCmS.z == b.startVelocityCmS.z &&
                a.constantAccelerationCmS2.x == b.constantAccelerationCmS2.x &&
                a.constantAccelerationCmS2.y == b.constantAccelerationCmS2.y &&
                a.constantAccelerationCmS2.z == b.constantAccelerationCmS2.z,
                "deterministic replay must reproduce every analytic segment");
        }
        for (const auto& ball : breakRuntime.state().balls)
            observedAirborneRelease = observedAirborneRelease ||
                (!ball.pocketed && ball.physicalDomain == billiardgl::PhysicalBallDomain::Airborne);
        {
            const auto& frames = breakRuntime.physicsTrace().frames();
            expect(!frames.empty(),
                "C11 real break must retain authoritative tick telemetry");
            const billiardgl::PhysicsFrame& frame = frames.back();
            const auto& analytic = breakRuntime.latestPhysicsStep().telemetry.analyticSurfaceMotion;
            // The former discrete zero-time fallback had an incidental ball-15
            // contact. Lock the actual failure boundary instead: every genuine
            // t=0 projection must reach the first trajectory segment exactly.
            auto projected = preStepState.balls;
            std::array<bool, billiardgl::kBallCount> corrected{};
            for (const auto& event : frame.solverEvents) {
                if (event.wholeTickTimeOfImpactSeconds != 0.0) continue;
                expect(std::string(event.failureCode) == "converged",
                    "zero-time handoff projection must come from a converged island");
                for (const auto& receipt : event.positionProjections) {
                    auto& position = projected[receipt.ballIndex].position;
                    expect(position.x == receipt.beforeCm.x &&
                        position.y == receipt.beforeCm.y && position.z == receipt.beforeCm.z,
                        "zero-time handoff receipts must form an exact ordered chain");
                    position = receipt.afterCm;
                    corrected[receipt.ballIndex] = true;
                    observedZeroTimeProjectionHandoff = true;
                }
            }
            for (int i = 0; i < billiardgl::kBallCount; ++i) {
                if (!corrected[i]) continue;
                const auto first = std::find_if(analytic.begin(),
                    analytic.end(), [i](const auto& s) {
                        return s.ballIndex == i;
                    });
                expect(first != analytic.end() &&
                    first->wholeTickStartOffsetSeconds == 0.0 &&
                    first->startPositionCm.x == projected[i].position.x &&
                    first->startPositionCm.y == projected[i].position.y &&
                    first->startPositionCm.z == projected[i].position.z,
                    "zero-time projection endpoint must anchor the first analytic segment");
            }
        }
        ++breakTicks;
    }
    expect(!breakRuntime.state().ballsMoving && breakTicks < 20000,
        "interactive break must reach a complete stop");
    expect(observedAirborneRelease,
        "the real break must exercise an authoritative airborne release");
    expect(executionSeconds < 5.0,
        "120 Hz interactive break must stay within the CPU regression budget");
    std::sort(tickMicroseconds.begin(), tickMicroseconds.end());
    const auto percentile = [&](double fraction) {
        return tickMicroseconds[static_cast<std::size_t>(std::ceil(
            fraction * tickMicroseconds.size())) - 1];
    };
    std::cout << "interactive_break ticks=" << breakTicks
        << " simulation_seconds=" << breakTicks * interactive.solver.timeStepSeconds
        << " measured_cpu_seconds=" << executionSeconds
        << " tick_us_p50=" << percentile(0.50)
        << " tick_us_p95=" << percentile(0.95)
        << " tick_us_p99=" << percentile(0.99)
        << " exact_replay=pass\n";
    return EXIT_SUCCESS;
}
