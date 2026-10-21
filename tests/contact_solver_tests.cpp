#include "contact_solver.h"
#include "surface_motion.h"

#include <cmath>
#include <cstdlib>
#include <iostream>
#include <limits>

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
    expect(solved.contacts[0].relativeVelocityBeforeCmS.x == -100.0f &&
        solved.contacts[0].relativeVelocityAfterCmS.x > 0.0f,
        "solver diagnostics should preserve the contact-relative velocity transition");
    expect(symmetric.balls[0].motionState == BallMotionState::Sliding &&
        symmetric.balls[2].motionState == BallMotionState::Sliding,
        "solver impulses should refresh surface motion classification");

    PhysicsProfile frictional = elastic;
    frictional.ball.normalRestitution = 0.8f;
    frictional.ball.frictionCoefficient = 0.25f;
    GameState oblique = stateFor(2);
    oblique.balls[0].position.x = 0.0f;
    oblique.balls[1].position.x = 2.0f * frictional.ball.radiusCm;
    oblique.balls[0].velocity = Point3{100.0f, 0.0f, 40.0f};
    ContactIsland obliqueIsland;
    obliqueIsland.ballIndices = {0, 1};
    obliqueIsland.contacts = {pair(0, 1)};
    const ContactSolverResult frictionSolved = solveContactIsland(
        oblique, obliqueIsland, frictional);
    expect(frictionSolved.status == ContactSolverStatus::Converged,
        "oblique frictional island should converge");
    expect(std::fabs(
        frictionSolved.contacts[0].accumulatedTangentialImpulseNs) <=
        frictional.ball.frictionCoefficient *
            frictionSolved.contacts[0].accumulatedNormalImpulseNs + 1e-12,
        "accumulated island impulse should obey the Coulomb cone");
    expect(std::fabs(oblique.balls[0].angularVelocity.y) > 0.0f &&
        std::fabs(oblique.balls[1].angularVelocity.y) > 0.0f,
        "island friction should transfer angular momentum");
    expect(frictionSolved.contacts[0].regime == BallBallContactRegime::Stick ||
        frictionSolved.contacts[0].regime == BallBallContactRegime::Slip,
        "island friction should expose a stable stick/slip regime");
    expect(frictionSolved.totalKineticEnergyAfterJ <=
        frictionSolved.totalKineticEnergyBeforeJ + 1e-10,
        "island solve must not create translational plus rotational energy");
    expect(frictionSolved.contacts[0].tangentEffectiveMassKg > 0.0 &&
        frictionSolved.contacts[0].firstContactArmM.x > 0.0f &&
        frictionSolved.contacts[0].secondContactArmM.x < 0.0f,
        "solver diagnostics should expose contact arms and effective mass");

    PhysicsProfile cushion = elastic;
    cushion.formulaVersion = "cushion_collision_v1";
    cushion.cushion.normalRestitution = 0.7f;
    cushion.cushion.restitutionIntercept = 0.7f;
    cushion.cushion.minimumRestitution = 0.7f;
    cushion.cushion.maximumRestitution = 0.7f;
    cushion.cushion.frictionCoefficient = 0.2f;
    cushion.cushion.noseHeightRatio = 1.4f;
    GameState railState = stateFor(1);
    railState.balls[0].velocity = Point3{100.0f, 0.0f, 30.0f};
    ContactIsland railIsland;
    railIsland.ballIndices = {0};
    railIsland.contacts = {boundaryContactCandidate(0, 1, 0.0,
        Point3{-1.0f, 0.0f, 0.0f},
        PocketBoundaryEventKind::StraightRail)};
    const ContactSolverResult railSolved = solveContactIsland(
        railState, railIsland, cushion);
    expect(railSolved.status == ContactSolverStatus::Converged &&
        railSolved.contacts.size() == 1 &&
        railSolved.contacts[0].accumulatedNormalImpulseNs > 0.0,
        "a rail constraint should use the same converged island solver");
    expect(std::fabs(railSolved.contacts[0].firstContactArmM.y -
            cushion.ball.radiusCm / 100.0f * 0.4f) < 1e-6f &&
        std::fabs(railState.balls[0].angularVelocity.y) > 0.0f,
        "rail constraints should apply friction at the configured cushion nose height");
    expect(std::fabs(railSolved.contacts[0].tangent.y) < 1e-12f &&
        std::fabs(railState.balls[0].velocity.y) < 1e-6f,
        "rigid cushion friction must remain tangent to the table plane");
    expect(std::fabs(railSolved.contacts[0].accumulatedTangentialImpulseNs) <=
            cushion.cushion.frictionCoefficient *
                railSolved.contacts[0].accumulatedNormalImpulseNs + 1e-12 &&
        railSolved.totalKineticEnergyAfterJ <=
            railSolved.totalKineticEnergyBeforeJ + 1e-10,
        "rail island impulses must obey the Coulomb cone and remain passive");

    // Regression from the shipping -Z nose path: the supported-cushion arm's
    // elevated Y offset lets x-axis spin contribute a large +Z contact speed,
    // hiding a real -Z COM approach. The explicit airborne nose arm is radial
    // to the queried vertical plane, so spin cannot cancel normal approach.
    PhysicsProfile airborneNoseProfile = cushion;
    airborneNoseProfile.cushion.frictionCoefficient = 0.0f;
    GameState airborneNose = stateFor(1);
    airborneNose.balls[0].physicalDomain =
        PhysicalBallDomain::Airborne;
    airborneNose.balls[0].velocity = Point3{-72.0f, 17.0f, -176.0f};
    airborneNose.balls[0].angularVelocity.x = 2000.0f;
    const BallState airborneNoseBefore = airborneNose.balls[0];
    ContactIsland airborneNoseIsland;
    airborneNoseIsland.ballIndices = {0};
    airborneNoseIsland.contacts = {airborneCushionNoseContactCandidate(
        0, 2, 0.0, Point3{0.0f, 0.0f, 1.0f})};
    const ContactSolverResult airborneNoseSolved = solveContactIsland(
        airborneNose, airborneNoseIsland, airborneNoseProfile);
    expect(airborneNoseSolved.status == ContactSolverStatus::Converged &&
            airborneNoseSolved.contacts.size() == 1 &&
            airborneNoseSolved.contacts[0].relativeVelocityBeforeCmS.z ==
                airborneNoseBefore.velocity.z &&
            airborneNoseSolved.contacts[0].accumulatedNormalImpulseNs > 0.0 &&
            airborneNoseSolved.contacts[0].firstContactArmM.x == 0.0f &&
            airborneNoseSolved.contacts[0].firstContactArmM.y == 0.0f &&
            airborneNoseSolved.contacts[0].firstContactArmM.z < 0.0f &&
            airborneNose.balls[0].velocity.z > 0.0f &&
            airborneNose.balls[0].velocity.y ==
                airborneNoseBefore.velocity.y &&
            airborneNose.balls[0].angularVelocity.x ==
                airborneNoseBefore.angularVelocity.x &&
            airborneNoseSolved.totalKineticEnergyAfterJ <=
                airborneNoseSolved.totalKineticEnergyBeforeJ + 1e-10,
        "airborne nose geometry must resolve COM approach at the radial sphere-plane arm without a spin-induced zero impulse");

    for (const Point3 normal : {Point3{0.0f, 0.6f, 0.8f},
            Point3{0.0f, 0.6f, -0.8f}, Point3{0.8f, 0.6f, 0.0f},
            Point3{-0.8f, 0.6f, 0.0f}}) {
        GameState edge = stateFor(1);
        edge.balls[0].physicalDomain = PhysicalBallDomain::Airborne;
        edge.balls[0].velocity = Point3{-600.0f * normal.x,
            -600.0f * normal.y, -600.0f * normal.z};
        edge.balls[0].angularVelocity = Point3{100.0f, -200.0f, 300.0f};
        const GameState before = edge;
        ContactIsland island;
        island.ballIndices = {0};
        island.contacts = {roundedUpperRailEdgeContactCandidate(0, 0, 0.0, normal)};
        const auto solved = solveContactIsland(edge, island, airborneNoseProfile);
        const double outgoing = edge.balls[0].velocity.x * normal.x +
            edge.balls[0].velocity.y * normal.y + edge.balls[0].velocity.z * normal.z;
        expect(solved.status == ContactSolverStatus::Converged &&
                solved.contacts.size() == 1 && outgoing > 0.0 &&
                solved.contacts[0].accumulatedNormalImpulseNs > 0.0 &&
                solved.totalKineticEnergyAfterJ <= solved.totalKineticEnergyBeforeJ + 1e-10 &&
                std::fabs(solved.contacts[0].firstContactArmM.y +
                    normal.y * airborneNoseProfile.ball.radiusCm / 100.0f) < 1e-7,
            "rounded upper rail edge must resolve radial 3D impact passively on all four sides");
        GameState repeated = before;
        const auto repeatedResult = solveContactIsland(repeated, island, airborneNoseProfile);
        expect(repeatedResult.status == solved.status &&
                repeated.balls[0].velocity.x == edge.balls[0].velocity.x &&
                repeated.balls[0].velocity.y == edge.balls[0].velocity.y &&
                repeated.balls[0].velocity.z == edge.balls[0].velocity.z,
            "rounded edge solve must exactly repeat in the same build");
        island.contacts[0].boundaryGeometry = BoundaryContactGeometry::AirborneCushionNose;
        repeated = before;
        expect(solveContactIsland(repeated, island, airborneNoseProfile).status ==
                ContactSolverStatus::NonfiniteState,
            "vertical nose geometry must continue to reject an oblique edge normal");
    }
    for (const Point3 invalidNormal : {Point3{0.0f, 0.0f, 1.0f},
            Point3{0.0f, 1.0f, 0.0f}, Point3{0.0f, -0.6f, 0.8f},
            Point3{0.0f, 0.6f, 0.4f}, Point3{0.48f, 0.6f, 0.64f}}) {
        GameState edge = stateFor(1);
        ContactIsland island;
        island.ballIndices = {0};
        island.contacts = {roundedUpperRailEdgeContactCandidate(0, 0, 0.0, invalidNormal)};
        expect(solveContactIsland(edge, island, airborneNoseProfile).status ==
                ContactSolverStatus::NonfiniteState,
            "rounded rail edge must reject endpoints, downward, nonunit or two-axis normals");
    }

    GameState oldArmCounterexample = stateFor(1);
    oldArmCounterexample.balls[0] = airborneNoseBefore;
    ContactIsland supportedArmIsland;
    supportedArmIsland.ballIndices = {0};
    supportedArmIsland.contacts = {boundaryContactCandidate(0, 2, 0.0,
        Point3{0.0f, 0.0f, 1.0f},
        PocketBoundaryEventKind::StraightRail)};
    const ContactSolverResult oldArmSolved = solveContactIsland(
        oldArmCounterexample, supportedArmIsland, airborneNoseProfile);
    expect(oldArmSolved.status == ContactSolverStatus::Converged &&
            oldArmSolved.contacts[0].relativeVelocityBeforeCmS.z > 0.0f &&
            oldArmSolved.contacts[0].accumulatedNormalImpulseNs == 0.0 &&
            oldArmCounterexample.balls[0].velocity.z ==
                airborneNoseBefore.velocity.z,
        "the measured elevated supported-cushion arm must reproduce the airborne zero-impulse counterexample without changing its legacy semantics");

    PhysicsProfile supportProfile = elastic;
    supportProfile.cushion.normalRestitution = 0.95f;
    supportProfile.cushion.restitutionIntercept = 0.95f;
    supportProfile.cushion.minimumRestitution = 0.95f;
    supportProfile.cushion.maximumRestitution = 0.95f;
    supportProfile.cushion.frictionCoefficient = 0.8f;
    GameState downward = stateFor(1);
    downward.balls[0].trajectoryGeneration = 7;
    downward.balls[0].position = Point3{4.0f, 9.9f, 6.0f};
    downward.balls[0].velocity = Point3{3.0f, -50.0f, 5.0f};
    downward.balls[0].angularVelocity = Point3{1.0f, 2.0f, 3.0f};
    const BallState downwardBefore = downward.balls[0];
    ContactIsland supportIsland;
    supportIsland.ballIndices = {0};
    supportIsland.contacts = {supportPlaneContactCandidate(0, 10.0)};
    const ContinuousEventBatch supportBatch = buildEarliestEventBatch(
        supportIsland.contacts, supportProfile.solver.toiToleranceSeconds,
        supportProfile.solver.maximumIslandSize);
    expect(supportBatch.failureCode == ContinuousBatchFailureCode::None &&
        supportBatch.physicalIslands.size() == 1 &&
        supportBatch.physicalIslands[0].contacts.size() == 1 &&
        supportBatch.physicalIslands[0].contacts[0].kind ==
            ContinuousContactKind::SupportPlane &&
        supportBatch.topologyTransitions.empty(),
        "typed support plane must be classified as a physical island constraint, never a topology transition");
    const ContactSolverResult supported = solveContactIsland(
        downward, supportIsland, supportProfile);
    expect(supported.status == ContactSolverStatus::Converged &&
        supported.contacts.size() == 1 &&
        supported.contacts[0].normal.x == 0.0f &&
        supported.contacts[0].normal.y == 1.0f &&
        supported.contacts[0].normal.z == 0.0f &&
        supported.contacts[0].firstContactArmM.x == 0.0f &&
        std::fabs(supported.contacts[0].firstContactArmM.y +
            supportProfile.ball.radiusCm / 100.0f) < 1e-7f &&
        supported.contacts[0].firstContactArmM.z == 0.0f &&
        supported.contacts[0].restitution == 0.0 &&
        supported.contacts[0].frictionCoefficient == 0.0 &&
        supported.contacts[0].accumulatedNormalImpulseNs > 0.0 &&
        supported.contacts[0].accumulatedTangentialImpulseNs == 0.0 &&
        downward.balls[0].velocity.y == 0.0f &&
        downward.balls[0].velocity.x == downwardBefore.velocity.x &&
        downward.balls[0].velocity.z == downwardBefore.velocity.z &&
        downward.balls[0].angularVelocity.x ==
            downwardBefore.angularVelocity.x &&
        downward.balls[0].angularVelocity.y ==
            downwardBefore.angularVelocity.y &&
        downward.balls[0].angularVelocity.z ==
            downwardBefore.angularVelocity.z &&
        downward.balls[0].position.y == 10.0f &&
        !supported.positionProjections.empty() &&
        supported.positionProjections.front().ballIndex == 0 &&
        supported.positionProjections.front().trajectoryGeneration == 7 &&
        supported.positionProjections.front().beforeCm.y == 9.9f &&
        supported.positionProjections.back().afterCm.y == 10.0f,
        "typed support plane must apply a +Y inelastic frictionless unilateral impulse and actual generation-bound projection receipts");

    GameState released = stateFor(1);
    released.balls[0].position.y = 10.0f;
    released.balls[0].velocity = Point3{2.0f, 15.0f, 4.0f};
    const BallState releasedBefore = released.balls[0];
    const ContactSolverResult releasedResult = solveContactIsland(
        released, supportIsland, supportProfile);
    expect(releasedResult.status == ContactSolverStatus::Converged &&
        releasedResult.contacts.size() == 1 &&
        releasedResult.contacts[0].accumulatedNormalImpulseNs == 0.0 &&
        releasedResult.contacts[0].regime ==
            BallBallContactRegime::Separating &&
        releasedResult.positionProjections.empty() &&
        released.balls[0].velocity.x == releasedBefore.velocity.x &&
        released.balls[0].velocity.y == releasedBefore.velocity.y &&
        released.balls[0].velocity.z == releasedBefore.velocity.z,
        "support plane must release an upward ball without attraction, bounce or friction");

    GameState unsupported = stateFor(1);
    unsupported.balls[0].position.y = 10.0f;
    unsupported.balls[0].velocity.y = -20.0f;
    const BallState unsupportedBefore = unsupported.balls[0];
    ContactIsland noSupport;
    noSupport.ballIndices = {0};
    const ContactSolverResult unsupportedResult = solveContactIsland(
        unsupported, noSupport, supportProfile);
    expect(unsupportedResult.status == ContactSolverStatus::Converged &&
        unsupportedResult.contacts.empty() &&
        unsupportedResult.positionProjections.empty() &&
        unsupported.balls[0].velocity.y == unsupportedBefore.velocity.y,
        "a ball without an explicit typed support constraint must remain unconstrained");

    GameState coupledSupport = stateFor(2);
    coupledSupport.balls[0].position = Point3{0.0f, 10.0f, 0.0f};
    coupledSupport.balls[1].position = Point3{
        2.0f * supportProfile.ball.radiusCm, 10.0f, 0.0f};
    coupledSupport.balls[0].velocity = Point3{100.0f, -25.0f, 0.0f};
    ContactIsland coupledSupportIsland;
    coupledSupportIsland.ballIndices = {0, 1};
    coupledSupportIsland.contacts = {pair(0, 1),
        supportPlaneContactCandidate(0, 10.0)};
    const ContactSolverResult coupledSupportResult = solveContactIsland(
        coupledSupport, coupledSupportIsland, supportProfile);
    expect(coupledSupportResult.status == ContactSolverStatus::Converged &&
        coupledSupportResult.contacts.size() == 2 &&
        coupledSupportResult.contacts[0].accumulatedNormalImpulseNs > 0.0 &&
        coupledSupportResult.contacts[1].accumulatedNormalImpulseNs > 0.0 &&
        std::fabs(coupledSupport.balls[0].velocity.y) <=
            supportProfile.solver.residualToleranceCmS &&
        coupledSupport.balls[1].velocity.x > 0.0f,
        "typed support and ball-ball constraints must converge together in one island without sequential clamping");

    GameState invalidSupportState = stateFor(1);
    invalidSupportState.balls[0].position.y = 10.0f;
    invalidSupportState.balls[0].velocity.y = -10.0f;
    const BallState invalidSupportBefore = invalidSupportState.balls[0];
    ContactIsland invalidSupportIsland;
    invalidSupportIsland.ballIndices = {0};
    ContinuousContactCandidate invalidSupport = supportPlaneContactCandidate(0,
        std::numeric_limits<double>::quiet_NaN());
    invalidSupportIsland.contacts = {invalidSupport};
    const ContactSolverResult invalidSupportResult = solveContactIsland(
        invalidSupportState, invalidSupportIsland, supportProfile);
    expect(invalidSupportResult.status == ContactSolverStatus::NonfiniteState &&
        invalidSupportResult.contacts.empty() &&
        invalidSupportResult.positionProjections.empty() &&
        invalidSupportState.balls[0].position.y ==
            invalidSupportBefore.position.y &&
        invalidSupportState.balls[0].velocity.y ==
            invalidSupportBefore.velocity.y,
        "invalid typed support input must fail before any velocity or position mutation");

    GameState projection = stateFor(2);
    ContactIsland overlap;
    overlap.ballIndices = {0, 1};
    ContinuousContactCandidate penetrating = pair(0, 1);
    penetrating.penetrationCm = 0.1;
    overlap.contacts = {penetrating};
    const ContactSolverResult projected = solveContactIsland(projection, overlap, elastic);
    expect(projected.status == ContactSolverStatus::Converged &&
        projected.contacts[0].projectionCm > 0.09 &&
        projected.kineticEnergyAfterJ == 0.0 &&
        projected.positionIterations == elastic.solver.positionIterations,
        "position projection should run its fixed count without adding velocity energy");
    expect(!projected.positionProjections.empty(),
        "actual position mutations must expose ordered receipts");
    int projectionOrdinal = 0;
    for (const auto& receipt : projected.positionProjections) {
        expect(receipt.ordinal == projectionOrdinal++ && receipt.contactIndex == 0 &&
            (receipt.ballIndex == 0 || receipt.ballIndex == 1) &&
            (receipt.beforeCm.x != receipt.afterCm.x ||
             receipt.beforeCm.y != receipt.afterCm.y ||
             receipt.beforeCm.z != receipt.afterCm.z),
            "projection receipts bind actual changed participants and order");
    }
    GameState belowUlp = stateFor(1);
    belowUlp.balls[0].position.x = 1000000.0f;
    ContactIsland wall;
    wall.ballIndices = {0};
    auto wallContact = pair(0, -1);
    wallContact.kind = ContinuousContactKind::StraightRail;
    wallContact.penetrationCm = .01;
    wall.contacts.push_back(wallContact);
    const auto roundedAway = solveContactIsland(belowUlp, wall, elastic);
    expect(roundedAway.contacts[0].projectionCm > 0.0 &&
        belowUlp.balls[0].position.x == 1000000.0f &&
        roundedAway.positionProjections.empty(),
        "sub-ULP requested projection is not an actual position mutation");

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

    GameState delegated = stateFor(2);
    delegated.balls[0].velocity.x = 100.0f;
    ContactIsland delegatedIsland;
    delegatedIsland.ballIndices = {0, 1};
    delegatedIsland.contacts = {pair(0, 1)};
    GameState boundedEntry = delegated;
    const ContactSolverResult wrapperResult = solveContactIsland(
        delegated, delegatedIsland, elastic);
    const ContactSolverResult boundedResult = solveContactIslandIteration(
        boundedEntry, delegatedIsland, elastic,
        elastic.solver.velocityIterations,
        elastic.solver.positionIterations);
    expect(wrapperResult.status == boundedResult.status &&
        delegated.balls[0].velocity.x == boundedEntry.balls[0].velocity.x &&
        delegated.balls[1].velocity.x == boundedEntry.balls[1].velocity.x,
        "the legacy solver delegates without changing its output");
    expect(solveContactIslandIteration(boundedEntry, delegatedIsland, elastic,
        0, 1).status == ContactSolverStatus::IterationLimit &&
        solveContactIslandIteration(boundedEntry, delegatedIsland, elastic,
        1, 0).status == ContactSolverStatus::IterationLimit,
        "nonpositive bounded iteration counts are rejected");
    return 0;
}
