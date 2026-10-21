#pragma once

#include "game_state.h"
#include "physics_boundary_mode.h"
#include "physics_profile.h"
#include "surface_motion.h"

#include <array>
#include <string>
#include <vector>

namespace billiardgl {

struct TableDepartureGeometry {
    float ballRadiusCm = 0.0f;
    float clothSurfaceYCm = 0.0f;
    float clothSupportedCenterYCm = 0.0f;
    float cushionNoseCenterYCm = 0.0f;
    float railTopSurfaceYCm = 0.0f;
    float railTopSupportedCenterYCm = 0.0f;
    float playfieldHalfWidthCm = 0.0f;
    float playfieldHalfLengthCm = 0.0f;
    float outerHalfWidthCm = 0.0f;
    float outerHalfLengthCm = 0.0f;
    float outsideLandingSurfaceYCm = 0.0f;
    float outsideLandingSupportedCenterYCm = 0.0f;
    float gravityYCmS2 = 0.0f;
};

struct TableDepartureGeometryValidation {
    bool ok = false;
    std::string error;
};

enum class TableDepartureEventKind {
    CushionNoseContact,
    OuterDeparture,
    RailTopLanding,
    OutsideLanding,
    ClothReturn,
    PocketEntry,
    CrossDomainBallContact
};

enum class TableDepartureEventScope {
    PhysicsPrivateDiagnostic
};

enum class TableDepartureClothReturnProvenance {
    NotApplicable,
    DirectAirborneLanding,
    RailTopLanding,
};

struct TableDepartureEvent {
    TableDepartureEventKind kind = TableDepartureEventKind::CushionNoseContact;
    TableDepartureEventScope scope =
        TableDepartureEventScope::PhysicsPrivateDiagnostic;
    int ballIndex = -1;
    int ordinal = -1;
    // Native source metadata retained by the producer.  Unknown associations
    // remain -1/0 and must be completed by the scheduler from actual contact
    // receipts; consumers may not infer them from position.
    std::uint64_t trajectoryGeneration = 0;
    int solverEventId = -1;
    int boundaryFeatureId = -1;
    int railFeatureId = -1;
    int landingSurfaceId = -1;
    int pocketId = -1;
    int linkedPhysicsContactOrdinal = -1;
    std::uint64_t pocketCaptureSequence = 0;
    TableDepartureClothReturnProvenance clothReturnProvenance =
        TableDepartureClothReturnProvenance::NotApplicable;
    double absoluteWholeTickTimeSeconds = 0.0;
    std::string stableIdentity;
    bool transitionWitnessComplete = false;
    // Exact centre velocity immediately before the private transition is
    // applied. This remains available when several solver contacts occur at
    // one TOI and therefore no positive-duration analytic segment exists
    // between the preceding contact and this transition.
    Point3 velocityBeforeCmS;
    bool velocityBeforeWitnessComplete = false;
    Point3 velocityAfterCmS;
    PhysicalBallDomain sourceDomain = PhysicalBallDomain::ClothSupported;
    PhysicalBallDomain targetDomain = PhysicalBallDomain::ClothSupported;
    Point3 secondVelocityAfterCmS;
    PhysicalBallDomain secondTargetDomain =
        PhysicalBallDomain::ClothSupported;
};

enum class TableDepartureFailureCode {
    None,
    InvalidState,
    InvalidGeometry,
    EventBudget,
    AmbiguousInitialPenetration,
    ContradictoryPocketDeparture,
    AmbiguousCrossDomainTableContact
};

struct TableDepartureStepResult {
    bool ok = false;
    TableDepartureFailureCode failureCode =
        TableDepartureFailureCode::InvalidState;
    double ambiguousCrossDomainContactTimeSeconds = 0.0;
    double ambiguousTableContactTimeSeconds = 0.0;
    std::vector<TableDepartureEvent> events;
    std::vector<SurfaceMotionAnalyticSegment> analyticSegments;
};

struct TableDepartureCandidate {
    TableDepartureEventKind kind =
        TableDepartureEventKind::CushionNoseContact;
    int firstBall = -1;
    int secondBall = -1;
    double timeSeconds = 0.0;
    int featureId = -1;
    TableDepartureClothReturnProvenance clothReturnProvenance =
        TableDepartureClothReturnProvenance::NotApplicable;
    // Unit surface normal at an actual swept fixed-table contact.  Plane
    // landings do not need this, but the rounded upper rail edge cannot be
    // represented by the old axis-only cushion feature ID.
    Point3 contactNormal;
    bool contactNormalComplete = false;
};

struct TableDepartureEarliestBatch {
    bool ok = false;
    TableDepartureFailureCode failureCode =
        TableDepartureFailureCode::InvalidState;
    double earliestTimeSeconds = 0.0;
    double ambiguousCrossDomainContactTimeSeconds = 0.0;
    double ambiguousTableContactTimeSeconds = 0.0;
    int failingFirstBall = -1;
    int failingSecondBall = -1;
    double initialPenetrationCm = 0.0;
    std::vector<TableDepartureCandidate> candidates;
};

TableDepartureGeometry buildTableDepartureGeometry(
    const PhysicsProfile& profile);
TableDepartureGeometryValidation validateTableDepartureGeometry(
    const TableDepartureGeometry& geometry);
double tableDepartureMinimumSeparationSquared(
    double radiusSumCm, double penetrationSlopCm);
TableDepartureStepResult advanceTableDepartureMotion(
    BallState& ball, float timeStep, const PhysicsProfile& profile);
TableDepartureStepResult advanceTableDeparturePhysics(
    GameState& state, float timeStep, const PhysicsProfile& profile,
    PhysicsBoundaryMode boundaryMode, int maximumEvents);
TableDepartureEarliestBatch queryEarliestTableDepartureBatch(
    const GameState& state, double maximumTime,
    const PhysicsProfile& profile, PhysicsBoundaryMode boundaryMode);
TableDepartureEarliestBatch queryEarliestTableDepartureBatch(
    const GameState& state, double maximumTime,
    const PhysicsProfile& profile, PhysicsBoundaryMode boundaryMode,
    const std::vector<SurfaceMotionAnalyticSegment>& authoritativeMotion,
    const std::array<bool, kBallCount>& departureOwnershipMask);
TableDepartureStepResult advanceTableDepartureDomains(
    GameState& state, double duration, const PhysicsProfile& profile,
    PhysicsBoundaryMode boundaryMode,
    const std::array<bool, kBallCount>& ownershipMask);
TableDepartureStepResult resolveTableDepartureBatch(
    GameState& state, const TableDepartureEarliestBatch& batch,
    const PhysicsProfile& profile, double absoluteWholeTickTimeSeconds,
    int eventOrdinalBase);
int countTableDepartureEvents(const TableDepartureStepResult& result,
    TableDepartureEventKind kind);

}  // namespace billiardgl
