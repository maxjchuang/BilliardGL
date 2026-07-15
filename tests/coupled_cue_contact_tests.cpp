#include "coupled_cue_contact.h"

#include "frozen_cue_topology.h"
#include "game_state.h"

#include <cmath>
#include <cstdlib>
#include <iostream>

namespace {

void expect(bool condition, const char* message)
{
    if (!condition) {
        std::cerr << message << '\n';
        std::exit(EXIT_FAILURE);
    }
}

bool close(double first, double second, double tolerance = 1e-8)
{
    return std::fabs(first - second) <= tolerance;
}

bool samePoint(const billiardgl::Point3& first, const billiardgl::Point3& second)
{
    return first.x == second.x && first.y == second.y && first.z == second.z;
}

billiardgl::GameState isolatedState()
{
    billiardgl::GameState state;
    billiardgl::initializeBalls(state);
    for (billiardgl::BallState& ball : state.balls) {
        ball.pocketed = true;
        ball.position = billiardgl::Point3{};
        ball.velocity = billiardgl::Point3{};
        ball.angularVelocity = billiardgl::Point3{};
    }
    state.balls[0].pocketed = false;
    return state;
}

billiardgl::CueImpactInput inputAt(double sideOffset)
{
    billiardgl::CueImpactInput input;
    input.cueBallIndex = 0;
    input.cueSpeedCmS = 200.0;
    input.cueMassKg = 0.5;
    input.direction = {{1.0, 0.0, 0.0}};
    input.tipOffsetRadius = {{sideOffset, 0.0}};
    input.chalkState = "CHALKED";
    return input;
}

billiardgl::FrozenCueTopology componentTopology(
    billiardgl::GameState& state, const billiardgl::PhysicsProfile& profile)
{
    state.balls[1].pocketed = false;
    state.balls[1].position.x = 2.0f * profile.ball.radiusCm;
    const billiardgl::FrozenCueTopology topology =
        billiardgl::detectFrozenCueTopology(state, 0, profile,
            billiardgl::PhysicsBoundaryMode::Unbounded);
    state.balls[1].pocketed = true;
    return topology;
}

void expectSameTrace(const billiardgl::CueContactResult& first,
    const billiardgl::CueContactResult& second)
{
    expect(first.microsteps.size() == second.microsteps.size(),
        "deterministic solves have equal trace lengths");
    for (std::size_t index = 0; index < first.microsteps.size(); ++index) {
        const auto& a = first.microsteps[index];
        const auto& b = second.microsteps[index];
        expect(a.index == b.index && a.timeSeconds == b.timeSeconds &&
            a.cuePositionM == b.cuePositionM &&
            a.cueVelocityMS == b.cueVelocityMS &&
            a.cueAccelerationMS2 == b.cueAccelerationMS2 &&
            a.compressionM == b.compressionM &&
            a.compressionRateMS == b.compressionRateMS &&
            a.normalForceN == b.normalForceN &&
            a.tangentialForceN == b.tangentialForceN &&
            a.normalImpulseNs == b.normalImpulseNs &&
            a.tangentialImpulseNs == b.tangentialImpulseNs &&
            a.kineticEnergyJ == b.kineticEnergyJ &&
            a.elasticEnergyJ == b.elasticEnergyJ &&
            a.dissipatedEnergyJ == b.dissipatedEnergyJ &&
            a.energyResidualJ == b.energyResidualJ && a.regime == b.regime,
            "microtrace fields are deterministic");
        expect(a.maximumPenetrationCm == b.maximumPenetrationCm &&
            a.solverResidualCmS == b.solverResidualCmS &&
            a.solverIterations == b.solverIterations &&
            a.contacts.size() == b.contacts.size(),
            "solver microtrace fields are deterministic");
        for (int ballIndex = 0; ballIndex < billiardgl::kBallCount; ++ballIndex) {
            const auto& firstBall = a.balls[ballIndex];
            const auto& secondBall = b.balls[ballIndex];
            expect(firstBall.index == secondBall.index &&
                samePoint(firstBall.positionCm, secondBall.positionCm) &&
                samePoint(firstBall.velocityCmS, secondBall.velocityCmS) &&
                samePoint(firstBall.accelerationCmS2,
                    secondBall.accelerationCmS2) &&
                samePoint(firstBall.angularVelocityRadS,
                    secondBall.angularVelocityRadS),
                "ball microtrace fields are deterministic");
        }
        for (std::size_t contactIndex = 0;
             contactIndex < a.contacts.size(); ++contactIndex) {
            const auto& firstContact = a.contacts[contactIndex];
            const auto& secondContact = b.contacts[contactIndex];
            expect(firstContact.kind == secondContact.kind &&
                firstContact.firstBall == secondContact.firstBall &&
                firstContact.secondBall == secondContact.secondBall &&
                firstContact.featureId == secondContact.featureId &&
                samePoint(firstContact.normal, secondContact.normal) &&
                firstContact.normalImpulseNs ==
                    secondContact.normalImpulseNs &&
                firstContact.tangentialImpulseNs ==
                    secondContact.tangentialImpulseNs &&
                firstContact.penetrationCm == secondContact.penetrationCm &&
                firstContact.residualCmS == secondContact.residualCmS,
                "constraint microtrace fields are deterministic");
        }
    }
}

}  // namespace

int main()
{
    const double force = billiardgl::huntCrossleyNormalForce(
        0.001, 0.2, 1.25e7, 0.05);
    expect(close(force, 1.25e7 * std::pow(0.001, 1.5) * 1.01, 1e-10),
        "Hunt-Crossley force uses the fixed 1.5 exponent");
    expect(billiardgl::huntCrossleyNormalForce(
        0.001, -1000.0, 1.25e7, 0.05) == 0.0,
        "dissipation never creates attraction");

    billiardgl::PhysicsProfile profile =
        billiardgl::defaultChinesePoolPhysicsProfile();
    profile.frozenCueContact.enabled = true;
    billiardgl::GameState state = isolatedState();
    const billiardgl::FrozenCueTopology topology =
        componentTopology(state, profile);

    const auto centered = billiardgl::solveCoupledCueContact(
        state, topology, inputAt(0.0), profile);
    if (centered.status != billiardgl::CoupledCueContactStatus::Released) {
        std::cerr << "centered failure error=" << centered.error
                  << " steps=" << centered.contact.microsteps.size();
        if (!centered.contact.microsteps.empty()) {
            const auto& last = centered.contact.microsteps.back();
            std::cerr << " t=" << last.timeSeconds
                      << " delta=" << last.compressionM
                      << " delta_dot=" << last.compressionRateMS
                      << " force=" << last.normalForceN
                      << " kinetic=" << last.kineticEnergyJ
                      << " elastic=" << last.elasticEnergyJ
                      << " dissipated=" << last.dissipatedEnergyJ
                      << " residual=" << last.energyResidualJ;
        }
        std::cerr << '\n';
    }
    expect(centered.status == billiardgl::CoupledCueContactStatus::Released &&
        centered.contact.applied && !centered.contact.microsteps.empty(),
        "finite contact releases successfully");
    expect(state.balls[0].velocity.x == 0.0f,
        "the component solver never mutates its input state");
    expect(centered.contact.microsteps.back().timeSeconds <=
        profile.frozenCueContact.maximumContactSeconds,
        "contact releases within the configured duration");
    for (const auto& step : centered.contact.microsteps) {
        expect(std::fabs(step.tangentialForceN) <=
            centered.contact.frictionCoefficient * step.normalForceN + 1e-9,
            "every microstep respects the friction cone");
        expect(step.energyResidualJ <= 1e-8,
            "positive damping does not create mechanical energy");
    }

    billiardgl::PhysicsProfile conservative = profile;
    conservative.frozenCueContact.normalDissipationSPerM = 0.0;
    conservative.frozenCueContact.tangentialDampingNsPerM = 0.0;
    const auto undamped = billiardgl::solveCoupledCueContact(
        state, topology, inputAt(0.0), conservative);
    if (undamped.status != billiardgl::CoupledCueContactStatus::Released ||
        (!undamped.contact.microsteps.empty() &&
         std::fabs(undamped.contact.microsteps.back().energyResidualJ) > 1e-8)) {
        std::cerr << "undamped status=" << static_cast<int>(undamped.status)
                  << " error=" << undamped.error
                  << " steps=" << undamped.contact.microsteps.size();
        if (!undamped.contact.microsteps.empty()) {
        const auto& last = undamped.contact.microsteps.back();
        std::cerr << " residual=" << last.energyResidualJ
                  << " kinetic=" << last.kineticEnergyJ
                  << " elastic=" << last.elasticEnergyJ
                  << " delta=" << last.compressionM
                  << " rate=" << last.compressionRateMS;
        }
        std::cerr << '\n';
    }
    expect(undamped.status == billiardgl::CoupledCueContactStatus::Released &&
        std::fabs(undamped.contact.microsteps.back().energyResidualJ) <= 1e-8,
        "zero damping conserves energy within tolerance");

    const auto left = billiardgl::solveCoupledCueContact(
        state, topology, inputAt(-0.2), profile);
    const auto right = billiardgl::solveCoupledCueContact(
        state, topology, inputAt(0.2), profile);
    if (left.status != billiardgl::CoupledCueContactStatus::Released ||
        right.status != billiardgl::CoupledCueContactStatus::Released) {
        std::cerr << "offset failures left=" << left.error
                  << " right=" << right.error
                  << " left_spin=" << left.state.balls[0].angularVelocity.y
                  << " right_spin=" << right.state.balls[0].angularVelocity.y
                  << " left_steps=" << left.contact.microsteps.size();
        if (!left.contact.microsteps.empty()) {
            const auto& last = left.contact.microsteps.back();
            std::cerr << " residual=" << last.energyResidualJ
                      << " kinetic=" << last.kineticEnergyJ
                      << " elastic=" << last.elasticEnergyJ
                      << " delta=" << last.compressionM;
        }
        std::cerr << '\n';
    }
    expect(left.status == billiardgl::CoupledCueContactStatus::Released &&
        right.status == billiardgl::CoupledCueContactStatus::Released &&
        close(left.state.balls[0].angularVelocity.y,
            -right.state.balls[0].angularVelocity.y, 1e-5) &&
        std::fabs(left.state.balls[0].angularVelocity.y) > 0.0,
        "left and right offsets create mirrored spin");

    const auto repeated = billiardgl::solveCoupledCueContact(
        state, topology, inputAt(0.0), profile);
    expectSameTrace(centered.contact, repeated.contact);
    return EXIT_SUCCESS;
}
