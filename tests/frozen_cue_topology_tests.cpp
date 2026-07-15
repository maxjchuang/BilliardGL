#include "frozen_cue_topology.h"

#include "game_state.h"

#include <cstdlib>
#include <iostream>
#include <vector>

namespace {

void expect(bool condition, const char* message)
{
    if (!condition) {
        std::cerr << message << '\n';
        std::exit(EXIT_FAILURE);
    }
}

billiardgl::GameState isolatedCueBall()
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

bool hasRail(const billiardgl::FrozenCueTopology& topology)
{
    for (const billiardgl::ContinuousContactCandidate& contact :
         topology.island.contacts) {
        if (contact.kind == billiardgl::ContinuousContactKind::StraightRail) {
            return true;
        }
    }
    return false;
}

}  // namespace

int main()
{
    const billiardgl::PhysicsProfile profile =
        billiardgl::defaultChinesePoolPhysicsProfile();
    billiardgl::GameState state = isolatedCueBall();

    const billiardgl::FrozenCueTopology ordinary =
        billiardgl::detectFrozenCueTopology(
            state, 0, profile, billiardgl::PhysicsBoundaryMode::Unbounded);
    expect(ordinary.status == billiardgl::FrozenCueTopologyStatus::Valid &&
        !ordinary.frozen && ordinary.island.ballIndices == std::vector<int>({0}),
        "an isolated cue ball selects the ordinary v4 path");

    const float diameter = 2.0f * profile.ball.radiusCm;
    state.balls[1].pocketed = false;
    state.balls[1].position.x = diameter;
    const billiardgl::FrozenCueTopology pair =
        billiardgl::detectFrozenCueTopology(
            state, 0, profile, billiardgl::PhysicsBoundaryMode::Unbounded);
    expect(pair.status == billiardgl::FrozenCueTopologyStatus::Valid &&
        pair.frozen && pair.island.ballIndices == std::vector<int>({0, 1}) &&
        pair.island.contacts.size() == 1,
        "one frozen neighbour forms a canonical two-ball island");

    state.balls[2].pocketed = false;
    state.balls[2].position.x = 2.0f * diameter;
    const billiardgl::FrozenCueTopology chain =
        billiardgl::detectFrozenCueTopology(
            state, 0, profile, billiardgl::PhysicsBoundaryMode::Unbounded);
    expect(chain.frozen &&
        chain.island.ballIndices == std::vector<int>({0, 1, 2}) &&
        chain.island.contacts.size() == 2,
        "touching neighbours are traversed transitively");

    billiardgl::GameState symmetric = isolatedCueBall();
    symmetric.balls[1].pocketed = false;
    symmetric.balls[1].position.x = diameter;
    symmetric.balls[2].pocketed = false;
    symmetric.balls[2].position.x = -diameter;
    const billiardgl::FrozenCueTopology firstSymmetric =
        billiardgl::detectFrozenCueTopology(
            symmetric, 0, profile, billiardgl::PhysicsBoundaryMode::Unbounded);
    const billiardgl::FrozenCueTopology secondSymmetric =
        billiardgl::detectFrozenCueTopology(
            symmetric, 0, profile, billiardgl::PhysicsBoundaryMode::Unbounded);
    expect(firstSymmetric.island.ballIndices == secondSymmetric.island.ballIndices &&
        firstSymmetric.island.contacts.size() == 2 &&
        secondSymmetric.island.contacts.size() == 2,
        "multi-neighbour topology is deterministic");

    billiardgl::PhysicsProfile limited = profile;
    limited.solver.maximumIslandSize = 2;
    const billiardgl::FrozenCueTopology tooLarge =
        billiardgl::detectFrozenCueTopology(
            state, 0, limited, billiardgl::PhysicsBoundaryMode::Unbounded);
    expect(tooLarge.status == billiardgl::FrozenCueTopologyStatus::IslandLimit &&
        tooLarge.error == "contact_island_limit",
        "oversized frozen islands fail before solving");

    billiardgl::GameState overlapping = isolatedCueBall();
    overlapping.balls[1].pocketed = false;
    const billiardgl::FrozenCueTopology contradictory =
        billiardgl::detectFrozenCueTopology(
            overlapping, 0, profile, billiardgl::PhysicsBoundaryMode::Unbounded);
    expect(contradictory.status ==
        billiardgl::FrozenCueTopologyStatus::ContradictoryTopology &&
        contradictory.error == "contradictory_frozen_topology",
        "deep overlap is rejected as contradictory topology");

    billiardgl::GameState railState = isolatedCueBall();
    railState.balls[0].position.x =
        profile.tableBoundary.playfieldWidthCm * 0.5f - profile.ball.radiusCm;
    railState.balls[0].position.z = 30.0f;
    const billiardgl::FrozenCueTopology rail =
        billiardgl::detectFrozenCueTopology(
            railState, 0, profile,
            billiardgl::PhysicsBoundaryMode::ProductionTable);
    expect(rail.frozen && hasRail(rail),
        "a cue ball tangent to a production rail includes that constraint");
    return EXIT_SUCCESS;
}
