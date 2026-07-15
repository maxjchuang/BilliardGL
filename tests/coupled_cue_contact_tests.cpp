#include "coupled_cue_contact.h"

#include "frozen_cue_topology.h"
#include "game_state.h"

#include <algorithm>
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

billiardgl::FrozenCueTopology topologyFor(const billiardgl::GameState& state,
    const billiardgl::PhysicsProfile& profile,
    billiardgl::PhysicsBoundaryMode boundaryMode =
        billiardgl::PhysicsBoundaryMode::Unbounded)
{
    return billiardgl::detectFrozenCueTopology(
        state, 0, profile, boundaryMode);
}

void testRigidContactsAreCoupledDuringLoading()
{
    billiardgl::PhysicsProfile profile =
        billiardgl::defaultChinesePoolPhysicsProfile();
    profile.frozenCueContact.enabled = true;
    const float diameter = 2.0f * profile.ball.radiusCm;

    billiardgl::GameState pair = isolatedState();
    pair.balls[1].pocketed = false;
    pair.balls[1].position.x = diameter;
    const auto pairResult = billiardgl::solveCoupledCueContact(
        pair, topologyFor(pair, profile), inputAt(0.0), profile);
    expect(pairResult.status == billiardgl::CoupledCueContactStatus::Released &&
        pairResult.state.balls[1].velocity.x > 0.0f,
        "a frozen neighbour receives impulse during cue loading");
    for (const auto& step : pairResult.contact.microsteps) {
        expect(step.maximumPenetrationCm <=
            profile.solver.maximumPenetrationCm &&
            step.solverResidualCmS <= profile.solver.residualToleranceCmS,
            "rigid microsteps remain inside penetration and residual limits");
        expect(step.contacts.size() == 1,
            "the pair constraint is captured in every complete microstep");
    }

    billiardgl::GameState chain = pair;
    chain.balls[2].pocketed = false;
    chain.balls[2].position.x = 2.0f * diameter;
    const auto chainResult = billiardgl::solveCoupledCueContact(
        chain, topologyFor(chain, profile), inputAt(0.0), profile);
    if (chainResult.status != billiardgl::CoupledCueContactStatus::Released) {
        std::cerr << "chain error=" << chainResult.error
                  << " steps=" << chainResult.contact.microsteps.size()
                  << " v1=" << chainResult.state.balls[1].velocity.x
                  << " v2=" << chainResult.state.balls[2].velocity.x << '\n';
    }
    expect(chainResult.status == billiardgl::CoupledCueContactStatus::Released &&
        chainResult.state.balls[1].velocity.x > 0.0f &&
        chainResult.state.balls[2].velocity.x > 0.0f,
        "a three-ball frozen chain is loaded in the same transaction");

    billiardgl::GameState symmetric = isolatedState();
    symmetric.balls[1].pocketed = false;
    symmetric.balls[1].position = billiardgl::Point3{
        diameter * 0.8660254f, 0.0f, diameter * 0.5f};
    symmetric.balls[2].pocketed = false;
    symmetric.balls[2].position = billiardgl::Point3{
        diameter * 0.8660254f, 0.0f, -diameter * 0.5f};
    const billiardgl::FrozenCueTopology symmetricTopology =
        topologyFor(symmetric, profile);
    const auto symmetricResult = billiardgl::solveCoupledCueContact(
        symmetric, symmetricTopology, inputAt(0.0), profile);
    if (symmetricResult.status !=
            billiardgl::CoupledCueContactStatus::Released ||
        !close(symmetricResult.state.balls[1].velocity.x,
            symmetricResult.state.balls[2].velocity.x, 0.01) ||
        !close(symmetricResult.state.balls[1].velocity.z,
            -symmetricResult.state.balls[2].velocity.z, 0.01)) {
        std::cerr << "symmetric error=" << symmetricResult.error
                  << " topology=" << static_cast<int>(symmetricTopology.status)
                  << " contacts=" << symmetricTopology.island.contacts.size()
                  << " v1=" << symmetricResult.state.balls[1].velocity.x
                  << "," << symmetricResult.state.balls[1].velocity.z
                  << " v2=" << symmetricResult.state.balls[2].velocity.x
                  << "," << symmetricResult.state.balls[2].velocity.z << '\n';
    }
    expect(symmetricResult.status ==
            billiardgl::CoupledCueContactStatus::Released &&
        close(symmetricResult.state.balls[1].velocity.x,
            symmetricResult.state.balls[2].velocity.x, 0.01) &&
        close(symmetricResult.state.balls[1].velocity.z,
            -symmetricResult.state.balls[2].velocity.z, 0.01),
        "symmetric frozen neighbours remain mirrored");

    billiardgl::FrozenCueTopology reversed = symmetricTopology;
    std::reverse(reversed.island.contacts.begin(),
        reversed.island.contacts.end());
    const auto reversedResult = billiardgl::solveCoupledCueContact(
        symmetric, reversed, inputAt(0.0), profile);
    expect(reversedResult.status ==
        billiardgl::CoupledCueContactStatus::Released,
        "a permuted contact input still solves");
    for (int index = 0; index < billiardgl::kBallCount; ++index) {
        expect(samePoint(symmetricResult.state.balls[index].position,
                   reversedResult.state.balls[index].position) &&
            samePoint(symmetricResult.state.balls[index].velocity,
                reversedResult.state.balls[index].velocity) &&
            samePoint(symmetricResult.state.balls[index].angularVelocity,
                reversedResult.state.balls[index].angularVelocity),
            "canonical rigid solves ignore input contact permutation");
    }

    billiardgl::GameState rail = isolatedState();
    rail.balls[0].position.x =
        profile.tableBoundary.playfieldWidthCm * 0.5f -
        profile.ball.radiusCm;
    rail.balls[0].position.z = 30.0f;
    const auto railResult = billiardgl::solveCoupledCueContact(rail,
        topologyFor(rail, profile,
            billiardgl::PhysicsBoundaryMode::ProductionTable),
        inputAt(0.0), profile);
    expect(railResult.status == billiardgl::CoupledCueContactStatus::Released &&
        !railResult.contact.microsteps.empty() &&
        !railResult.contact.microsteps.front().contacts.empty(),
        "a frozen cue-ball cushion constraint is solved during loading");

    billiardgl::PhysicsProfile refined = profile;
    refined.frozenCueContact.microstepSeconds *= 0.5;
    const auto refinedPair = billiardgl::solveCoupledCueContact(
        pair, topologyFor(pair, refined), inputAt(0.0), refined);
    expect(refinedPair.status == billiardgl::CoupledCueContactStatus::Released,
        "the halved microstep fixture releases");
    for (int index : std::vector<int>{0, 1}) {
        expect(std::fabs(pairResult.state.balls[index].velocity.x -
                   refinedPair.state.balls[index].velocity.x) <= 0.5 &&
            std::fabs(pairResult.state.balls[index].velocity.z -
                refinedPair.state.balls[index].velocity.z) <= 0.5,
            "halving the microstep changes final velocity by at most 0.5 cm/s");
    }
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
    testRigidContactsAreCoupledDuringLoading();
    return EXIT_SUCCESS;
}
