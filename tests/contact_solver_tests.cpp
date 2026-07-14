#include "contact_solver.h"

#include <cmath>
#include <cstdlib>
#include <iostream>

namespace {
void expect(bool value, const char* message)
{
    if (!value) { std::cerr << message << '\n'; std::exit(1); }
}

billiardgl::ContinuousContactCandidate pair(int first, int second, float nx = 1.0f)
{
    billiardgl::ContinuousContactCandidate value;
    value.valid = true;
    value.kind = billiardgl::ContinuousContactKind::BallBall;
    value.firstBall = first;
    value.secondBall = second;
    value.normal.x = nx;
    return value;
}

billiardgl::GameState stateFor(int count)
{
    billiardgl::GameState state;
    for (billiardgl::BallState& ball : state.balls) ball.pocketed = true;
    for (int index = 0; index < count; ++index) state.balls[index].pocketed = false;
    return state;
}
}

int main()
{
    using namespace billiardgl;
    PhysicsProfile elastic = defaultChinesePoolPhysicsProfile();
    elastic.ball.normalRestitution = 1.0f;
    elastic.ball.frictionCoefficient = 0.0f;
    elastic.solver.velocityIterations = 40;
    elastic.solver.residualToleranceCmS = 0.0001f;

    GameState symmetric = stateFor(3);
    symmetric.balls[0].velocity.x = 100.0f;
    symmetric.balls[2].velocity.x = -100.0f;
    ContactIsland island;
    island.ballIndices = {0, 1, 2};
    island.contacts = {pair(0, 1), pair(1, 2)};
    const ContactSolverResult solved = solveContactIsland(symmetric, island, elastic);
    expect(solved.status == ContactSolverStatus::Converged,
        "symmetric island should converge inside the iteration bound");
    expect(std::fabs(symmetric.balls[1].velocity.x) < 0.001f &&
        std::fabs(symmetric.balls[0].velocity.x + 100.0f) < 0.01f &&
        std::fabs(symmetric.balls[2].velocity.x - 100.0f) < 0.01f,
        "two-sided simultaneous impact should preserve mirror symmetry");
    expect(solved.kineticEnergyAfterJ <= solved.kineticEnergyBeforeJ + 1e-8,
        "island impulse must not create kinetic energy");
    expect(solved.contacts.size() == 2 &&
        solved.contacts[0].accumulatedNormalImpulseNs > 0.0 &&
        solved.contacts[1].accumulatedNormalImpulseNs > 0.0,
        "each simultaneous constraint should expose one accumulated impulse");

    GameState projection = stateFor(2);
    ContactIsland overlap;
    overlap.ballIndices = {0, 1};
    ContinuousContactCandidate penetrating = pair(0, 1);
    penetrating.penetrationCm = 0.1;
    overlap.contacts = {penetrating};
    const ContactSolverResult projected = solveContactIsland(projection, overlap, elastic);
    expect(projected.status == ContactSolverStatus::Converged &&
        projected.contacts[0].projectionCm > 0.09 &&
        projected.kineticEnergyAfterJ == 0.0,
        "position projection should remove overlap without adding velocity energy");

    ContactIsland tooLarge = island;
    tooLarge.limitExceeded = true;
    GameState unchanged = symmetric;
    expect(solveContactIsland(unchanged, tooLarge, elastic).status ==
        ContactSolverStatus::IslandLimit,
        "island-size failure should be explicit");

    ContactIsland tooDeep = overlap;
    tooDeep.contacts[0].penetrationCm = 1.0;
    expect(solveContactIsland(projection, tooDeep, elastic).status ==
        ContactSolverStatus::PenetrationLimit,
        "penetration safety failure should be explicit");

    PhysicsProfile bounded = elastic;
    bounded.solver.velocityIterations = 1;
    bounded.solver.residualToleranceCmS = 0.0f;
    GameState chain = stateFor(3);
    chain.balls[0].velocity.x = 100.0f;
    const ContactSolverResult limited = solveContactIsland(chain, island, bounded);
    expect(limited.status == ContactSolverStatus::IterationLimit,
        "unconverged chain should return an iteration-limit status");
    return 0;
}
