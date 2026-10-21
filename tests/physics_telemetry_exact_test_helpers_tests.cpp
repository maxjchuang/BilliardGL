#include "physics_telemetry_exact_test_helpers.h"

#include <cstdlib>
#include <iostream>
#include <limits>
#include <string>

namespace {

using billiardgl::PhysicsStepTelemetry;

void expect(bool condition, const char* message)
{
    if (!condition) {
        std::cerr << message << '\n';
        std::exit(EXIT_FAILURE);
    }
}

PhysicsStepTelemetry populatedTelemetry()
{
    using namespace billiardgl;
    PhysicsStepTelemetry value;
    value.stepStatus = PhysicsStepStatus::Failed;
    value.failureDetail = "same-size mutation fixture";
    value.failureCode = PhysicsFailureCode::ResidualLimit;
    value.failingEventId = 7;
    value.failingIslandId = 3;
    value.wholeTickDurationSeconds = 0.01;
    value.maximumPenetrationCm = 0.002;

    PhysicsContactRecord contact;
    contact.solverEventId = 7;
    contact.solverIslandId = 3;
    contact.solverResidualCmS = 0.001;
    contact.solverProjectionCm = 0.002;
    contact.kind = PhysicsContactKind::Pocket;
    contact.firstBall = 0;
    contact.secondBall = 4;
    contact.boundaryFeatureId = 5;
    contact.normal = {1.0f, 2.0f, 3.0f};
    contact.normalImpulseNs = 0.4;
    contact.penetrationCm = 0.5;
    contact.regime = BallBallContactRegime::Slip;
    contact.velocityImpulseApplied = true;
    contact.contactTangent = {4.0f, 5.0f, 6.0f};
    contact.firstContactArmCm = {7.0f, 8.0f, 9.0f};
    contact.secondContactArmCm = {10.0f, 11.0f, 12.0f};
    contact.relativeContactVelocityBeforeCmS = {13.0f, 14.0f, 15.0f};
    contact.relativeContactVelocityAfterCmS = {16.0f, 17.0f, 18.0f};
    contact.impulseOnSecondNs = {19.0f, 20.0f, 21.0f};
    contact.firstPositionCorrectionCm = {22.0f, 23.0f, 24.0f};
    contact.secondPositionCorrectionCm = {25.0f, 26.0f, 27.0f};
    contact.normalRelativeSpeedBeforeCmS = 28.0;
    contact.normalRelativeSpeedAfterCmS = 29.0;
    contact.tangentialImpulseNs = 30.0;
    contact.frictionCoefficient = 31.0;
    contact.kineticEnergyBeforeJ = 32.0;
    contact.kineticEnergyAfterJ = 33.0;
    contact.positionSlopCm = 34.0;
    contact.cushionRegime = CushionContactRegime::Stick;
    contact.cushionContactArmCm = {35.0f, 36.0f, 37.0f};
    contact.cushionContactHeightCm = 38.0;
    contact.cushionContactVelocityBeforeCmS = {39.0f, 40.0f, 41.0f};
    contact.cushionContactVelocityAfterCmS = {42.0f, 43.0f, 44.0f};
    contact.impulseOnBallNs = {45.0f, 46.0f, 47.0f};
    contact.positionCorrectionCm = {48.0f, 49.0f, 50.0f};
    contact.restitution = 0.9;
    contact.noseHeightRatio = 1.4;
    contact.incidentSpeedCmS = 53.0;
    contact.maximumRigidIncidentSpeedCmS = 54.0;
    contact.rigidDomainExceeded = true;
    contact.positionCorrected = true;
    contact.timeOfImpactSeconds = 0.003;
    contact.wholeTickTimeOfImpactSeconds = 0.004;
    contact.firstPositionAtContactCm = {55.0f, 56.0f, 57.0f};
    contact.secondPositionAtContactCm = {58.0f, 59.0f, 60.0f};
    contact.pocketId = 2;
    contact.pocketKind = PocketKind::Side;
    contact.pocketBoundaryEvent = PocketBoundaryEventKind::Capture;
    contact.pocketPhaseBefore = PocketInteractionPhase::ThroatCrossed;
    contact.pocketPhaseAfter = PocketInteractionPhase::Captured;
    contact.pocketLocal = {61.0, 62.0};
    contact.pocketJawCenterCm = {63.0f, 64.0f, 65.0f};
    contact.pocketJawRadiusCm = 66.0;
    contact.pocketThroatSignedDistanceCm = 67.0;
    contact.pocketCaptureSignedDistanceCm = 68.0;
    contact.pocketPassable = true;
    contact.pocketVerticalShaft = true;
    contact.pocketCaptureSequence = 69;
    contact.administrativeResetAfterCapture = true;
    contact.trajectoryGenerationBefore = 70;
    contact.trajectoryGenerationAfter = 71;
    contact.administrativeResetPositionCm = {72.0f, 73.0f, 74.0f};
    value.contacts.push_back(contact);

    SolverPositionProjection projection;
    projection.ordinal = 1;
    projection.iteration = 2;
    projection.contactIndex = 3;
    projection.ballIndex = 4;
    projection.trajectoryGeneration = 75;
    projection.beforeCm = {76.0f, 77.0f, 78.0f};
    projection.afterCm = {79.0f, 80.0f, 81.0f};
    SolverEventRecord solver;
    solver.eventId = 7;
    solver.islandId = 3;
    solver.candidateCount = 2;
    solver.contactCount = 1;
    solver.duplicateCandidatesRemoved = 1;
    solver.velocityIterations = 8;
    solver.positionIterations = 4;
    solver.maximumResidualCmS = 82.0;
    solver.maximumPenetrationCm = 83.0;
    solver.kineticEnergyBeforeJ = 84.0;
    solver.kineticEnergyAfterJ = 85.0;
    solver.islandLimitExceeded = true;
    solver.failureCode = "fixture_failure";
    solver.wholeTickTimeOfImpactSeconds = 0.006;
    solver.positionProjections.push_back(projection);
    value.solverEvents.push_back(solver);

    SurfaceMotionAnalyticSegment segment;
    segment.ballIndex = 0;
    segment.trajectoryGeneration = 86;
    segment.segmentOrdinal = 1;
    segment.motionState = BallMotionState::Rolling;
    segment.wholeTickStartOffsetSeconds = 0.001;
    segment.durationSeconds = 0.002;
    segment.startPositionCm = {87.0f, 88.0f, 89.0f};
    segment.endPositionCm = {90.0f, 91.0f, 92.0f};
    segment.startVelocityCmS = {93.0f, 94.0f, 95.0f};
    segment.constantAccelerationCmS2 = {96.0f, 97.0f, 98.0f};
    segment.analyticPathComplete = true;
    value.analyticSurfaceMotion.push_back(segment);
    SurfaceMotionStep motion;
    motion.ballIndex = 0;
    motion.before = BallMotionState::Sliding;
    motion.after = BallMotionState::Rolling;
    motion.initialSlipSpeedCmS = 99.0f;
    motion.finalSlipSpeedCmS = 100.0f;
    motion.transitionTimeSeconds = 0.003f;
    motion.frictionAccelerationCmS2 = {101.0f, 102.0f, 103.0f};
    motion.angularAccelerationRadS2 = {104.0f, 105.0f, 106.0f};
    motion.analyticSegments.push_back(segment);
    value.surfaceMotion.push_back(motion);

    TableDepartureEvent departure;
    departure.kind = TableDepartureEventKind::OutsideLanding;
    departure.ballIndex = 0;
    departure.ordinal = 2;
    departure.trajectoryGeneration = 3;
    departure.solverEventId = 4;
    departure.boundaryFeatureId = 5;
    departure.railFeatureId = 6;
    departure.landingSurfaceId = 7;
    departure.pocketId = 8;
    departure.linkedPhysicsContactOrdinal = 9;
    departure.pocketCaptureSequence = 10;
    departure.clothReturnProvenance =
        billiardgl::TableDepartureClothReturnProvenance::RailTopLanding;
    departure.absoluteWholeTickTimeSeconds = 0.007;
    departure.stableIdentity = "outside:0:2";
    departure.transitionWitnessComplete = true;
    departure.velocityBeforeCmS = {106.0f, -12.0f, 107.0f};
    departure.velocityBeforeWitnessComplete = true;
    departure.velocityAfterCmS = {107.0f, 0.0f, 108.0f};
    departure.sourceDomain = PhysicalBallDomain::Airborne;
    departure.targetDomain = PhysicalBallDomain::OutsideLandingSupported;
    departure.secondVelocityAfterCmS = {109.0f, 110.0f, 111.0f};
    departure.secondTargetDomain = PhysicalBallDomain::Airborne;
    value.tableDepartureEvents.push_back(departure);

    PhysicsPlaneCrossingRecord crossing;
    crossing.observationId = "plane.1";
    crossing.ballIndex = 0;
    crossing.axis = PhysicsPlaneAxis::X;
    crossing.planeCoordinateCm = 112.0;
    crossing.direction = PhysicsPlaneCrossingDirection::PositiveToNegative;
    crossing.wholeTickTimeOfImpactSeconds = 0.008;
    crossing.segmentOrdinal = 1;
    crossing.signedDistanceBeforeCm = 113.0;
    crossing.signedDistanceAfterCm = 114.0;
    crossing.axisVelocityAtCrossingCmS = 115.0;
    crossing.origin = "fixture_origin";
    crossing.complete = true;
    value.planeCrossings.push_back(crossing);
    PhysicsPlaneObservationAttestation attestation;
    attestation.observationId = "plane.1";
    attestation.ballIndex = 0;
    attestation.wholeTickDurationSeconds = 0.01;
    attestation.coveredSegmentCount = 1;
    attestation.crossingRecordCount = 1;
    attestation.complete = true;
    value.planeAttestations.push_back(attestation);
    return value;
}

template <typename Mutation>
void rejects(const PhysicsStepTelemetry& baseline, Mutation mutation,
    const char* message)
{
    PhysicsStepTelemetry changed = baseline;
    mutation(changed);
    expect(!billiardgl::test::exactPhysicsStepTelemetry(baseline, changed),
        message);
}

}  // namespace

int main()
{
    const PhysicsStepTelemetry baseline = populatedTelemetry();
    expect(billiardgl::test::exactPhysicsStepTelemetry(baseline, baseline),
        "identical telemetry must compare exact");
    PhysicsStepTelemetry signedZero = baseline;
    signedZero.maximumPenetrationCm = -0.0;
    PhysicsStepTelemetry positiveZero = baseline;
    positiveZero.maximumPenetrationCm = 0.0;
    expect(billiardgl::test::exactPhysicsStepTelemetry(signedZero, positiveZero),
        "signed zero is the documented canonical equivalence");

    rejects(baseline, [](auto& value) { value.failureCode =
        billiardgl::PhysicsFailureCode::EventBudget; },
        "top-level failure mutation must be rejected");
    rejects(baseline, [](auto& value) { value.contacts[0].normalImpulseNs += 1.0; },
        "same-size contact impulse mutation must be rejected");
    rejects(baseline, [](auto& value) {
        ++value.contacts[0].trajectoryGenerationAfter;
    }, "same-size capture generation mutation must be rejected");
    rejects(baseline, [](auto& value) {
        value.contacts[0].administrativeResetAfterCapture = false;
    }, "capture/reset mutation must be rejected");
    rejects(baseline, [](auto& value) {
        value.contacts[0].pocketVerticalShaft = false;
    }, "same-size vertical pocket shaft mutation must be rejected");
    rejects(baseline, [](auto& value) {
        ++value.solverEvents[0].positionProjections[0].trajectoryGeneration;
    }, "same-size projection receipt mutation must be rejected");
    rejects(baseline, [](auto& value) {
        value.solverEvents[0].maximumResidualCmS += 1.0;
    }, "same-size solver diagnostic mutation must be rejected");
    rejects(baseline, [](auto& value) {
        ++value.analyticSurfaceMotion[0].trajectoryGeneration;
    }, "same-size analytic generation mutation must be rejected");
    rejects(baseline, [](auto& value) {
        value.surfaceMotion[0].analyticSegments[0].endPositionCm.x += 1.0f;
    }, "nested surface analytic mutation must be rejected");
    rejects(baseline, [](auto& value) {
        value.tableDepartureEvents[0].stableIdentity[0] = 'X';
    }, "same-size private departure identity mutation must be rejected");
    rejects(baseline, [](auto& value) {
        ++value.tableDepartureEvents[0].trajectoryGeneration;
    }, "same-size private departure generation mutation must be rejected");
    rejects(baseline, [](auto& value) {
        ++value.tableDepartureEvents[0].solverEventId;
    }, "same-size private departure solver id mutation must be rejected");
    rejects(baseline, [](auto& value) {
        ++value.tableDepartureEvents[0].boundaryFeatureId;
    }, "same-size private departure boundary mutation must be rejected");
    rejects(baseline, [](auto& value) {
        ++value.tableDepartureEvents[0].railFeatureId;
    }, "same-size private departure rail mutation must be rejected");
    rejects(baseline, [](auto& value) {
        ++value.tableDepartureEvents[0].landingSurfaceId;
    }, "same-size private departure landing mutation must be rejected");
    rejects(baseline, [](auto& value) {
        ++value.tableDepartureEvents[0].pocketId;
    }, "same-size private departure pocket mutation must be rejected");
    rejects(baseline, [](auto& value) {
        ++value.tableDepartureEvents[0].linkedPhysicsContactOrdinal;
    }, "same-size private departure contact link mutation must be rejected");
    rejects(baseline, [](auto& value) {
        ++value.tableDepartureEvents[0].pocketCaptureSequence;
    }, "same-size private departure capture sequence mutation must be rejected");
    rejects(baseline, [](auto& value) {
        value.tableDepartureEvents[0].clothReturnProvenance =
            billiardgl::TableDepartureClothReturnProvenance::
                DirectAirborneLanding;
    }, "same-size private departure provenance mutation must be rejected");
    rejects(baseline, [](auto& value) {
        value.tableDepartureEvents[0].velocityBeforeCmS.y += 1.0f;
    }, "same-size private departure incoming velocity mutation must be rejected");
    rejects(baseline, [](auto& value) {
        value.tableDepartureEvents[0].velocityBeforeWitnessComplete = false;
    }, "same-size private departure incoming witness mutation must be rejected");
    rejects(baseline, [](auto& value) {
        value.tableDepartureEvents[0].sourceDomain =
            billiardgl::PhysicalBallDomain::ClothSupported;
    }, "same-size private departure source domain mutation must be rejected");
    rejects(baseline, [](auto& value) {
        value.planeCrossings[0].signedDistanceAfterCm += 1.0;
    }, "same-size plane record mutation must be rejected");
    rejects(baseline, [](auto& value) {
        ++value.planeAttestations[0].coveredSegmentCount;
    }, "same-size plane attestation mutation must be rejected");
    rejects(baseline, [](auto& value) { value.contacts.push_back(value.contacts[0]); },
        "vector cardinality mutation must be rejected");
    return EXIT_SUCCESS;
}
