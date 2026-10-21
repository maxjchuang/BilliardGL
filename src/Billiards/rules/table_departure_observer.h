#pragma once

#include "game_state.h"
#include "physics_boundary_mode.h"

#include <array>
#include <cstddef>
#include <cstdint>
#include <map>
#include <set>
#include <string>
#include <vector>

namespace billiardgl::rules {

enum class CanonicalTableDepartureBallKind {
    CueBall,
    ObjectBall,
};

enum class CanonicalTableDepartureEventKind {
    OuterDeparture,
    RailTopLanding,
    ClothReturn,
    OutsideLanding,
    OutsideRest,
    PocketCapture,
    AdministrativeReset,
};

// Exact projection inventory from the authoritative private physics stream.
// The first seven values are D6-B measurement-eligible; only the final two
// diagnostic kinds may be excluded from the canonical measurement stream.
enum class CanonicalTableDeparturePrivateEventKind {
    OuterDeparture,
    RailTopLanding,
    ClothReturn,
    OutsideLanding,
    OutsideRest,
    PocketCapture,
    AdministrativeReset,
    PocketEntryDiagnostic,
    NoseContactDiagnostic,
    CrossContactDiagnostic,
};

enum class CanonicalTableDepartureProjectionDisposition {
    EmittedCanonical,
    ExcludedDiagnostic,
    ObservedNonSettlingSupport,
    ObservedPendingPocketEntry,
};

enum class CanonicalTableDepartureExclusionReason {
    None,
    NoseContactNotTableDepartureMeasurement,
    CrossContactNotTableDepartureMeasurement,
    PocketEntryBoundToCanonicalCapture,
    SupportContactImmediateRelease,
    PocketEntryAwaitingCapture,
};

enum class CanonicalTableDepartureSourceOrigin {
    PrivateEvent,
    AnalyticTransition,
    CanonicalContact,
};

enum class CanonicalTableDepartureAnalyticState {
    NotApplicable,
    OutsideRolling,
    OutsideLandingPostState,
    OutsideStationary,
};

struct CanonicalTableDepartureSourceReceipt {
    int sourceOrdinal = -1;
    CanonicalTableDepartureSourceOrigin sourceOrigin =
        CanonicalTableDepartureSourceOrigin::PrivateEvent;
    int privateEventOrdinal = -1;
    int physicsContactOrdinal = -1;
    int canonicalContactOrdinal = -1;
    int precedingSegmentOrdinal = -1;
    int sourceSegmentOrdinal = -1;
    std::string sourceIdentity;
    CanonicalTableDeparturePrivateEventKind kind =
        CanonicalTableDeparturePrivateEventKind::OuterDeparture;
    int ballId = -1;
    int solverEventId = -1;
    std::uint64_t trajectoryGeneration = 0;
    double wholeTickTimeSeconds = 0.0;
    Point3 transitionPositionCm;
    Point3 velocityBeforeCmS;
    Point3 velocityAfterCmS;
    PhysicalBallDomain sourceDomain = PhysicalBallDomain::ClothSupported;
    PhysicalBallDomain targetDomain = PhysicalBallDomain::ClothSupported;
    CanonicalTableDepartureAnalyticState sourceAnalyticState =
        CanonicalTableDepartureAnalyticState::NotApplicable;
    CanonicalTableDepartureAnalyticState targetAnalyticState =
        CanonicalTableDepartureAnalyticState::NotApplicable;
    std::string analyticPartitionDigest;
    CanonicalTableDepartureProjectionDisposition disposition =
        CanonicalTableDepartureProjectionDisposition::EmittedCanonical;
    int canonicalEventOrdinal = -1;
    int canonicalEventCount = 0;
    std::string canonicalEventDigest;
    CanonicalTableDepartureExclusionReason exclusionReason =
        CanonicalTableDepartureExclusionReason::None;
    bool complete = false;
};

struct CanonicalTableDepartureEvent {
    CanonicalTableDepartureEventKind kind =
        CanonicalTableDepartureEventKind::OuterDeparture;
    int ballId = -1;
    std::uint64_t physicsTick = 0;
    int eventOrdinal = -1;
    int solverEventId = -1;
    std::uint64_t trajectoryGeneration = 0;
    std::uint64_t previousTrajectoryGeneration = 0;
    double wholeTickTimeSeconds = 0.0;
    PhysicalBallDomain sourceDomain = PhysicalBallDomain::ClothSupported;
    PhysicalBallDomain targetDomain = PhysicalBallDomain::ClothSupported;
    Point3 positionCm;
    Point3 velocityBeforeCmS;
    Point3 velocityAfterCmS;
    float ballRadiusCm = 0.0f;
    int boundaryFeatureId = -1;
    int railFeatureId = -1;
    int landingSurfaceId = -1;
    int pocketId = -1;
    std::uint64_t pocketCaptureSequence = 0;
    std::string origin =
        "AUTHORITATIVE_CONTINUOUS_TABLE_DEPARTURE_SOLVER_V1";
    bool complete = false;
    std::string stableIdentity;
};

struct CanonicalTableDepartureTickAttestation {
    std::uint64_t physicsTick = 0;
    double wholeTickDurationSeconds = 0.0;
    std::string solverProfileId;
    PhysicsBoundaryMode boundaryMode = PhysicsBoundaryMode::ProductionTable;
    // sourceEventCount is the count of emitted D6-B canonical events across
    // private-event, analytic-transition, and canonical-contact origins.
    // The source denominators remain separate; physics-private diagnostics
    // and one-contact-to-many-event projections may never disappear under a
    // no-dropped-samples claim.
    std::size_t sourceEventCount = 0;
    std::size_t privatePhysicsEventCount = 0;
    std::size_t analyticTransitionSourceCount = 0;
    std::size_t canonicalContactSourceCount = 0;
    std::size_t excludedPhysicsDiagnosticEventCount = 0;
    std::size_t nonSettlingSupportContactSourceCount = 0;
    std::size_t pendingPocketEntrySourceCount = 0;
    std::size_t emittedEventCount = 0;
    std::vector<CanonicalTableDepartureSourceReceipt> orderedSourceReceipts;
    std::string orderedSourceDigest;
    std::string orderedEventDigest;
    bool successfulStep = false;
    bool canonicalOrder = false;
    bool noDroppedSamples = false;
    bool complete = false;
};

struct CanonicalTableDepartureTick {
    std::uint64_t physicsTick = 0;
    double wholeTickDurationSeconds = 0.0;
    std::string solverProfileId;
    PhysicsBoundaryMode boundaryMode = PhysicsBoundaryMode::ProductionTable;
    std::vector<CanonicalTableDepartureEvent> events;
    CanonicalTableDepartureTickAttestation attestation;
};

struct CanonicalTableDepartureObserverIdentity {
    std::string shotIdentity;
    std::string rackIdentity;
    std::string solverProfileId;
    PhysicsBoundaryMode boundaryMode = PhysicsBoundaryMode::ProductionTable;
    std::uint64_t firstPhysicsTick = 0;
    std::array<std::uint64_t, kBallCount> initialTrajectoryGenerations{};
};

struct CanonicalBallTableDepartureMeasurement {
    int ballId = -1;
    CanonicalTableDepartureBallKind ballKind =
        CanonicalTableDepartureBallKind::ObjectBall;
    std::uint64_t trajectoryGeneration = 0;
    bool departureObserved = false;
    bool railTopLandingObserved = false;
    bool directClothLandingObserved = false;
    bool returnedOverRailTop = false;
    bool outsideLandingObserved = false;
    bool outsideRestObserved = false;
    bool cameToRestOffTable = false;
    bool capturedInPocket = false;
    bool administrativeResetObserved = false;
    bool complete = false;
};

struct CanonicalTableDepartureShotObservation {
    std::string schemaId = "canonical_table_departure_observation_v2";
    CanonicalTableDepartureObserverIdentity identity;
    std::vector<CanonicalTableDepartureEvent> events;
    std::vector<CanonicalBallTableDepartureMeasurement> balls;
    std::vector<std::string> orderedSuccessfulTickDigests;
    std::uint64_t lastSuccessfulPhysicsTick = 0;
    bool successfulPhysicsTicksContiguous = false;
    bool shotEndObserved = false;
    bool canonicalOrder = false;
    bool observationComplete = false;
    std::size_t pendingPocketEntryCount = 0;
    std::string digest;
};

struct CanonicalTableDepartureObservationValidation {
    bool valid = false;
    bool complete = false;
    std::string error;
};

std::string canonicalTableDepartureEventDigest(
    const std::vector<CanonicalTableDepartureEvent>& events);
std::string canonicalTableDepartureSourceDigest(
    const std::vector<CanonicalTableDepartureSourceReceipt>& receipts);

class CanonicalTableDepartureObserver {
public:
    bool begin(const CanonicalTableDepartureObserverIdentity& identity);
    bool consumeSuccessfulPhysicsTick(
        const CanonicalTableDepartureTick& tick);
    CanonicalTableDepartureObservationValidation finish(
        bool shotEndObserved);
    void rollback();

    bool active() const noexcept { return active_; }
    bool committed() const noexcept { return committed_; }
    const CanonicalTableDepartureShotObservation& observation() const noexcept
        { return observation_; }

private:
    bool active_ = false;
    bool committed_ = false;
    std::set<std::pair<int, std::uint64_t>> pendingPocketEntries_;
    std::map<std::uint64_t, std::string> successfulTickDigests_;
    CanonicalTableDepartureShotObservation observation_;
};

}  // namespace billiardgl::rules
