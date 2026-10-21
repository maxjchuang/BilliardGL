#pragma once

#include "physics_telemetry.h"

#include <cstdint>
#include <cstring>
#include <string_view>
#include <type_traits>

namespace billiardgl::test {
namespace detail {

template <typename Float>
inline bool exactFloating(Float first, Float second)
{
    static_assert(std::is_same<Float, float>::value ||
        std::is_same<Float, double>::value, "floating point required");
    // Physics treats signed zero as the same canonical value. All other
    // finite values, infinities, and NaN payloads must remain bit-exact.
    if (first == Float{0} && second == Float{0}) return true;
    using Bits = typename std::conditional<sizeof(Float) == sizeof(std::uint32_t),
        std::uint32_t, std::uint64_t>::type;
    Bits firstBits = 0;
    Bits secondBits = 0;
    std::memcpy(&firstBits, &first, sizeof(Float));
    std::memcpy(&secondBits, &second, sizeof(Float));
    return firstBits == secondBits;
}

inline bool exactCString(const char* first, const char* second)
{
    if (first == nullptr || second == nullptr) return first == second;
    return std::string_view(first) == std::string_view(second);
}

inline bool exactPoint(const Point3& first, const Point3& second)
{
    return exactFloating(first.x, second.x) &&
        exactFloating(first.y, second.y) &&
        exactFloating(first.z, second.z);
}

template <typename T, typename Comparator>
inline bool exactVector(const std::vector<T>& first,
    const std::vector<T>& second, Comparator comparator)
{
    if (first.size() != second.size()) return false;
    for (std::size_t index = 0; index < first.size(); ++index) {
        if (!comparator(first[index], second[index])) return false;
    }
    return true;
}

inline bool exactProjection(const SolverPositionProjection& first,
    const SolverPositionProjection& second)
{
    return first.ordinal == second.ordinal &&
        first.iteration == second.iteration &&
        first.contactIndex == second.contactIndex &&
        first.ballIndex == second.ballIndex &&
        first.trajectoryGeneration == second.trajectoryGeneration &&
        exactPoint(first.beforeCm, second.beforeCm) &&
        exactPoint(first.afterCm, second.afterCm);
}

inline bool exactContact(const PhysicsContactRecord& first,
    const PhysicsContactRecord& second)
{
    return first.solverEventId == second.solverEventId &&
        first.solverIslandId == second.solverIslandId &&
        exactFloating(first.solverResidualCmS, second.solverResidualCmS) &&
        exactFloating(first.solverProjectionCm, second.solverProjectionCm) &&
        first.kind == second.kind &&
        first.firstBall == second.firstBall &&
        first.secondBall == second.secondBall &&
        first.boundaryFeatureId == second.boundaryFeatureId &&
        exactPoint(first.normal, second.normal) &&
        exactFloating(first.normalImpulseNs, second.normalImpulseNs) &&
        exactFloating(first.penetrationCm, second.penetrationCm) &&
        first.regime == second.regime &&
        first.velocityImpulseApplied == second.velocityImpulseApplied &&
        exactPoint(first.contactTangent, second.contactTangent) &&
        exactPoint(first.firstContactArmCm, second.firstContactArmCm) &&
        exactPoint(first.secondContactArmCm, second.secondContactArmCm) &&
        exactPoint(first.relativeContactVelocityBeforeCmS,
            second.relativeContactVelocityBeforeCmS) &&
        exactPoint(first.relativeContactVelocityAfterCmS,
            second.relativeContactVelocityAfterCmS) &&
        exactPoint(first.impulseOnSecondNs, second.impulseOnSecondNs) &&
        exactPoint(first.firstPositionCorrectionCm,
            second.firstPositionCorrectionCm) &&
        exactPoint(first.secondPositionCorrectionCm,
            second.secondPositionCorrectionCm) &&
        exactFloating(first.normalRelativeSpeedBeforeCmS,
            second.normalRelativeSpeedBeforeCmS) &&
        exactFloating(first.normalRelativeSpeedAfterCmS,
            second.normalRelativeSpeedAfterCmS) &&
        exactFloating(first.tangentialImpulseNs, second.tangentialImpulseNs) &&
        exactFloating(first.frictionCoefficient, second.frictionCoefficient) &&
        exactFloating(first.kineticEnergyBeforeJ, second.kineticEnergyBeforeJ) &&
        exactFloating(first.kineticEnergyAfterJ, second.kineticEnergyAfterJ) &&
        exactFloating(first.positionSlopCm, second.positionSlopCm) &&
        first.cushionRegime == second.cushionRegime &&
        exactPoint(first.cushionContactArmCm, second.cushionContactArmCm) &&
        exactFloating(first.cushionContactHeightCm,
            second.cushionContactHeightCm) &&
        exactPoint(first.cushionContactVelocityBeforeCmS,
            second.cushionContactVelocityBeforeCmS) &&
        exactPoint(first.cushionContactVelocityAfterCmS,
            second.cushionContactVelocityAfterCmS) &&
        exactPoint(first.impulseOnBallNs, second.impulseOnBallNs) &&
        exactPoint(first.positionCorrectionCm, second.positionCorrectionCm) &&
        exactFloating(first.restitution, second.restitution) &&
        exactFloating(first.noseHeightRatio, second.noseHeightRatio) &&
        exactFloating(first.incidentSpeedCmS, second.incidentSpeedCmS) &&
        exactFloating(first.maximumRigidIncidentSpeedCmS,
            second.maximumRigidIncidentSpeedCmS) &&
        first.rigidDomainExceeded == second.rigidDomainExceeded &&
        first.positionCorrected == second.positionCorrected &&
        exactFloating(first.timeOfImpactSeconds,
            second.timeOfImpactSeconds) &&
        exactFloating(first.wholeTickTimeOfImpactSeconds,
            second.wholeTickTimeOfImpactSeconds) &&
        exactPoint(first.firstPositionAtContactCm,
            second.firstPositionAtContactCm) &&
        exactPoint(first.secondPositionAtContactCm,
            second.secondPositionAtContactCm) &&
        first.pocketId == second.pocketId &&
        first.pocketKind == second.pocketKind &&
        first.pocketBoundaryEvent == second.pocketBoundaryEvent &&
        first.pocketPhaseBefore == second.pocketPhaseBefore &&
        first.pocketPhaseAfter == second.pocketPhaseAfter &&
        exactFloating(first.pocketLocal.depthCm, second.pocketLocal.depthCm) &&
        exactFloating(first.pocketLocal.offsetCm, second.pocketLocal.offsetCm) &&
        exactPoint(first.pocketJawCenterCm, second.pocketJawCenterCm) &&
        exactFloating(first.pocketJawRadiusCm, second.pocketJawRadiusCm) &&
        exactFloating(first.pocketThroatSignedDistanceCm,
            second.pocketThroatSignedDistanceCm) &&
        exactFloating(first.pocketCaptureSignedDistanceCm,
            second.pocketCaptureSignedDistanceCm) &&
        first.pocketPassable == second.pocketPassable &&
        first.pocketVerticalShaft == second.pocketVerticalShaft &&
        first.pocketCaptureSequence == second.pocketCaptureSequence &&
        first.administrativeResetAfterCapture ==
            second.administrativeResetAfterCapture &&
        first.trajectoryGenerationBefore ==
            second.trajectoryGenerationBefore &&
        first.trajectoryGenerationAfter == second.trajectoryGenerationAfter &&
        exactPoint(first.administrativeResetPositionCm,
            second.administrativeResetPositionCm);
}

inline bool exactSolverEvent(const SolverEventRecord& first,
    const SolverEventRecord& second)
{
    return first.eventId == second.eventId &&
        first.islandId == second.islandId &&
        first.candidateCount == second.candidateCount &&
        first.contactCount == second.contactCount &&
        first.duplicateCandidatesRemoved == second.duplicateCandidatesRemoved &&
        first.velocityIterations == second.velocityIterations &&
        first.positionIterations == second.positionIterations &&
        exactFloating(first.maximumResidualCmS, second.maximumResidualCmS) &&
        exactFloating(first.maximumPenetrationCm,
            second.maximumPenetrationCm) &&
        exactFloating(first.kineticEnergyBeforeJ, second.kineticEnergyBeforeJ) &&
        exactFloating(first.kineticEnergyAfterJ, second.kineticEnergyAfterJ) &&
        first.islandLimitExceeded == second.islandLimitExceeded &&
        exactCString(first.failureCode, second.failureCode) &&
        exactFloating(first.wholeTickTimeOfImpactSeconds,
            second.wholeTickTimeOfImpactSeconds) &&
        exactVector(first.positionProjections, second.positionProjections,
            exactProjection);
}

inline bool exactAnalyticSegment(const SurfaceMotionAnalyticSegment& first,
    const SurfaceMotionAnalyticSegment& second)
{
    return first.ballIndex == second.ballIndex &&
        first.trajectoryGeneration == second.trajectoryGeneration &&
        first.segmentOrdinal == second.segmentOrdinal &&
        first.motionState == second.motionState &&
        exactFloating(first.wholeTickStartOffsetSeconds,
            second.wholeTickStartOffsetSeconds) &&
        exactFloating(first.durationSeconds, second.durationSeconds) &&
        exactPoint(first.startPositionCm, second.startPositionCm) &&
        exactPoint(first.endPositionCm, second.endPositionCm) &&
        exactPoint(first.startVelocityCmS, second.startVelocityCmS) &&
        exactPoint(first.constantAccelerationCmS2,
            second.constantAccelerationCmS2) &&
        first.analyticPathComplete == second.analyticPathComplete;
}

inline bool exactSurfaceMotion(const SurfaceMotionStep& first,
    const SurfaceMotionStep& second)
{
    return first.ballIndex == second.ballIndex &&
        first.before == second.before && first.after == second.after &&
        exactFloating(first.initialSlipSpeedCmS, second.initialSlipSpeedCmS) &&
        exactFloating(first.finalSlipSpeedCmS, second.finalSlipSpeedCmS) &&
        exactFloating(first.transitionTimeSeconds,
            second.transitionTimeSeconds) &&
        exactPoint(first.frictionAccelerationCmS2,
            second.frictionAccelerationCmS2) &&
        exactPoint(first.angularAccelerationRadS2,
            second.angularAccelerationRadS2) &&
        exactVector(first.analyticSegments, second.analyticSegments,
            exactAnalyticSegment);
}

inline bool exactTableDeparture(const TableDepartureEvent& first,
    const TableDepartureEvent& second)
{
    return first.kind == second.kind && first.scope == second.scope &&
        first.ballIndex == second.ballIndex &&
        first.ordinal == second.ordinal &&
        first.trajectoryGeneration == second.trajectoryGeneration &&
        first.solverEventId == second.solverEventId &&
        first.boundaryFeatureId == second.boundaryFeatureId &&
        first.railFeatureId == second.railFeatureId &&
        first.landingSurfaceId == second.landingSurfaceId &&
        first.pocketId == second.pocketId &&
        first.linkedPhysicsContactOrdinal ==
            second.linkedPhysicsContactOrdinal &&
        first.pocketCaptureSequence == second.pocketCaptureSequence &&
        first.clothReturnProvenance == second.clothReturnProvenance &&
        exactFloating(first.absoluteWholeTickTimeSeconds,
            second.absoluteWholeTickTimeSeconds) &&
        first.stableIdentity == second.stableIdentity &&
        first.transitionWitnessComplete == second.transitionWitnessComplete &&
        exactPoint(first.velocityBeforeCmS, second.velocityBeforeCmS) &&
        first.velocityBeforeWitnessComplete ==
            second.velocityBeforeWitnessComplete &&
        exactPoint(first.velocityAfterCmS, second.velocityAfterCmS) &&
        first.sourceDomain == second.sourceDomain &&
        first.targetDomain == second.targetDomain &&
        exactPoint(first.secondVelocityAfterCmS,
            second.secondVelocityAfterCmS) &&
        first.secondTargetDomain == second.secondTargetDomain;
}

inline bool exactPlaneCrossing(const PhysicsPlaneCrossingRecord& first,
    const PhysicsPlaneCrossingRecord& second)
{
    return first.observationId == second.observationId &&
        first.ballIndex == second.ballIndex && first.axis == second.axis &&
        exactFloating(first.planeCoordinateCm, second.planeCoordinateCm) &&
        first.direction == second.direction &&
        exactFloating(first.wholeTickTimeOfImpactSeconds,
            second.wholeTickTimeOfImpactSeconds) &&
        first.segmentOrdinal == second.segmentOrdinal &&
        exactFloating(first.signedDistanceBeforeCm,
            second.signedDistanceBeforeCm) &&
        exactFloating(first.signedDistanceAfterCm,
            second.signedDistanceAfterCm) &&
        exactFloating(first.axisVelocityAtCrossingCmS,
            second.axisVelocityAtCrossingCmS) &&
        exactCString(first.origin, second.origin) &&
        first.complete == second.complete;
}

inline bool exactPlaneAttestation(
    const PhysicsPlaneObservationAttestation& first,
    const PhysicsPlaneObservationAttestation& second)
{
    return first.observationId == second.observationId &&
        first.ballIndex == second.ballIndex &&
        exactFloating(first.wholeTickDurationSeconds,
            second.wholeTickDurationSeconds) &&
        first.coveredSegmentCount == second.coveredSegmentCount &&
        first.crossingRecordCount == second.crossingRecordCount &&
        first.complete == second.complete;
}

}  // namespace detail

inline bool exactPhysicsStepTelemetry(const PhysicsStepTelemetry& first,
    const PhysicsStepTelemetry& second)
{
    return first.stepStatus == second.stepStatus &&
        first.failureDetail == second.failureDetail &&
        first.failureCode == second.failureCode &&
        first.failingEventId == second.failingEventId &&
        first.failingIslandId == second.failingIslandId &&
        detail::exactVector(first.contacts, second.contacts,
            detail::exactContact) &&
        detail::exactVector(first.solverEvents, second.solverEvents,
            detail::exactSolverEvent) &&
        detail::exactVector(first.tableDepartureEvents,
            second.tableDepartureEvents, detail::exactTableDeparture) &&
        detail::exactVector(first.surfaceMotion, second.surfaceMotion,
            detail::exactSurfaceMotion) &&
        detail::exactVector(first.analyticSurfaceMotion,
            second.analyticSurfaceMotion, detail::exactAnalyticSegment) &&
        detail::exactVector(first.planeCrossings, second.planeCrossings,
            detail::exactPlaneCrossing) &&
        detail::exactVector(first.planeAttestations,
            second.planeAttestations, detail::exactPlaneAttestation) &&
        detail::exactFloating(first.wholeTickDurationSeconds,
            second.wholeTickDurationSeconds) &&
        detail::exactFloating(first.maximumPenetrationCm,
            second.maximumPenetrationCm);
}

}  // namespace billiardgl::test
