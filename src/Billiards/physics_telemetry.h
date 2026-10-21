#pragma once

#include "ball_ball_contact.h"
#include "game_state.h"
#include "cue_impact.h"
#include "cue_contact.h"
#include "cushion_contact.h"
#include "physics_profile.h"
#include "pocket_boundary.h"
#include "physics_boundary_mode.h"
#include "surface_motion.h"
#include "solver_position_projection.h"
#include "table_departure_geometry.h"

#include <array>
#include <cstddef>
#include <cstdint>
#include <deque>
#include <map>
#include <string>
#include <vector>

namespace billiardgl {

constexpr float kDefaultBallMassKg = 0.17f;

enum class PhysicsContactKind {
    BallBall,
    Rail,
    Pocket,
    Support
};

enum class PhysicsStepStatus {
    Succeeded,
    Failed
};

enum class PhysicsPlaneAxis {
    X,
    Y,
    Z,
};

enum class PhysicsPlaneCrossingDirection {
    NegativeToPositive,
    PositiveToNegative,
};

enum class CanonicalCueObjectTipOrder {
    ContactBeforeTipSeparation,
    ContactEqualTipSeparation,
    ContactAfterTipSeparation,
    NoCueObjectContact,
    UnorderedIncomplete,
};

struct CanonicalCueObjectContactEvidence {
    int objectBallId = -1;
    double strokeRelativeTimeSeconds = 0.0;
    std::uint64_t physicsTick = 0;
    double wholeTickTimeOfImpactSeconds = 0.0;
    int solverEventId = -1;
    int solverIslandId = -1;
    int physicsContactOrdinal = -1;
    Point3 cueBallPositionAtContact;
    Point3 objectBallPositionAtContact;
    bool velocityImpulseApplied = false;
    double normalImpulseNs = 0.0;
    const char* origin = "AUTHORITATIVE_CONTINUOUS_PHYSICS_SOLVER_V1";
    bool complete = false;
};

struct CanonicalCueObjectOrderObservation {
    std::string schemaId = "canonical_cue_object_tip_separation_order_v1";
    std::string shotIdentity;
    int actorPlayerId = -1;
    std::string rackIdentity;
    std::uint64_t rulesRevision = 0;
    std::string rulesStateHash;
    std::uint64_t physicsTickStart = 0;
    std::uint64_t physicsTickEnd = 0;
    double tipSeparationTimeSeconds = 0.0;
    std::vector<CanonicalCueObjectContactEvidence> contacts;
    CanonicalCueObjectTipOrder order =
        CanonicalCueObjectTipOrder::UnorderedIncomplete;
    bool d5AFullStrokeComplete = false;
    bool successfulPhysicsTicksContiguous = false;
    bool shotEndObserved = false;
    bool allValuesFinite = false;
    bool canonicalOrder = false;
    bool complete = false;
    std::string digest;
};

struct CanonicalFrozenCushionBinding {
    std::string declarationDecisionId;
    int declaredObjectBallId = -1;
    int namedStraightCushionFeatureId = -1;
    std::uint64_t declarationParentRevision = 0;
    std::string declarationParentStateHash;
    std::string consumedShotIdentity;
    std::string planeObservationId;
    PhysicsPlaneAxis planeAxis = PhysicsPlaneAxis::X;
    double contactPlaneCoordinateCm = 0.0;
    PhysicsPlaneCrossingDirection departureDirection =
        PhysicsPlaneCrossingDirection::NegativeToPositive;
    const char* origin = "SIGNED_FROZEN_CUSHION_DECLARATION_V1";
    bool authenticated = false;
    bool committedAndConsumed = false;
};

struct CanonicalFrozenCushionEvent {
    int ballId = -1;
    int straightCushionFeatureId = -1;
    int pocketId = -1;
    std::uint64_t physicsTick = 0;
    double strokeRelativeTimeSeconds = 0.0;
    int solverEventId = -1;
    int physicsContactOrdinal = -1;
    std::uint64_t pocketCaptureSequence = 0;
    const char* kind = "";
    const char* origin = "AUTHORITATIVE_CONTINUOUS_PHYSICS_SOLVER_V1";
    bool complete = false;
};

struct CanonicalFrozenCushionShotObservation {
    std::string schemaId = "canonical_frozen_cushion_shot_observation_v1";
    std::string shotIdentity;
    CanonicalFrozenCushionBinding binding;
    std::uint64_t physicsTickStart = 0;
    std::uint64_t physicsTickEnd = 0;
    std::vector<CanonicalFrozenCushionEvent> events;
    bool departureObserved = false;
    bool sameNamedCushionRecontactAfterDeparture = false;
    std::vector<int> otherObjectStraightCushionBalls;
    std::vector<int> objectPocketedBalls;
    bool successfulPhysicsTicksContiguous = false;
    bool shotEndObserved = false;
    bool allValuesFinite = false;
    bool canonicalOrder = false;
    bool complete = false;
    std::string digest;
};

struct CanonicalD5BTelemetryValidation {
    bool valid = false;
    bool complete = false;
    std::string error;
};

struct PhysicsStepTelemetry;

CanonicalD5BTelemetryValidation validateCanonicalCueObjectOrderObservation(
    CanonicalCueObjectOrderObservation& observation);
CanonicalD5BTelemetryValidation validateCanonicalFrozenCushionShotObservation(
    CanonicalFrozenCushionShotObservation& observation);
const char* canonicalCueObjectTipOrderName(CanonicalCueObjectTipOrder order);

class CanonicalD5BShotObserver {
public:
    bool begin(const CanonicalCueObjectOrderObservation& identity,
        const CanonicalFrozenCushionBinding* frozenBinding = nullptr);
    bool bindTipObservation(
        const CanonicalCueTipStrokeObservation& tipObservation);
    bool bindFrozenCushion(
        const CanonicalFrozenCushionBinding& frozenBinding);
    bool consumeSuccessfulPhysicsTick(const PhysicsStepTelemetry& telemetry,
        std::uint64_t physicsTick, double tickStartStrokeSeconds);
    CanonicalD5BTelemetryValidation finish(bool shotEndObserved);
    void rollback();
    bool active() const { return active_; }
    bool committed() const { return committed_; }
    bool hasFrozenObservation() const { return hasFrozen_; }
    const CanonicalCueObjectOrderObservation& cueObjectObservation() const
        { return cueObject_; }
    const CanonicalFrozenCushionShotObservation& frozenObservation() const
        { return frozen_; }
    const CanonicalFrozenCushionBinding& frozenBinding() const
        { return frozen_.binding; }
    std::uint64_t firstPhysicsTick() const
        { return cueObject_.physicsTickStart; }
private:
    bool active_ = false;
    bool committed_ = false;
    bool hasFrozen_ = false;
    std::uint64_t lastPhysicsTick_ = 0;
    std::map<std::uint64_t,std::string> physicsTickDigests_;
    CanonicalCueObjectOrderObservation cueObject_;
    CanonicalFrozenCushionShotObservation frozen_;
};

struct PhysicsPlaneObservationRequest {
    std::string observationId;
    int ballIndex = -1;
    PhysicsPlaneAxis axis = PhysicsPlaneAxis::Z;
    double planeCoordinateCm = 0.0;
    PhysicsPlaneCrossingDirection direction =
        PhysicsPlaneCrossingDirection::NegativeToPositive;
    // D5-B only: a signed frozen-cushion declaration makes the initial
    // contact plane authoritative, so motion away from an exact boundary
    // may be recorded at the start of the first analytic segment.  Ordinary
    // plane crossings (including WPA-II-08) remain strict by default.
    bool allowStartOnPlaneDeparture = false;
};

struct PhysicsPlaneCrossingRecord {
    std::string observationId;
    int ballIndex = -1;
    PhysicsPlaneAxis axis = PhysicsPlaneAxis::Z;
    double planeCoordinateCm = 0.0;
    PhysicsPlaneCrossingDirection direction =
        PhysicsPlaneCrossingDirection::NegativeToPositive;
    double wholeTickTimeOfImpactSeconds = 0.0;
    int segmentOrdinal = -1;
    double signedDistanceBeforeCm = 0.0;
    double signedDistanceAfterCm = 0.0;
    double axisVelocityAtCrossingCmS = 0.0;
    const char* origin = "AUTHORITATIVE_CONTINUOUS_SURFACE_INTEGRATOR";
    bool complete = false;
};

struct PhysicsPlaneObservationAttestation {
    std::string observationId;
    int ballIndex = -1;
    double wholeTickDurationSeconds = 0.0;
    int coveredSegmentCount = 0;
    int crossingRecordCount = 0;
    bool complete = false;
};

enum class PhysicsFailureCode {
    None,
    EventBudget,
    InvalidControls,
    IslandLimit,
    ResidualLimit,
    PenetrationLimit,
    NonfiniteState,
    NonfiniteEnergy,
    PassiveEnergyCreation,
    ContradictoryTopology
};

const char* physicsStepStatusName(PhysicsStepStatus status);
const char* physicsFailureCodeName(PhysicsFailureCode code);

struct PhysicsContactRecord {
    int solverEventId = -1;
    int solverIslandId = -1;
    double solverResidualCmS = 0.0;
    double solverProjectionCm = 0.0;
    PhysicsContactKind kind = PhysicsContactKind::BallBall;
    int firstBall = -1;
    int secondBall = -1;
    int boundaryFeatureId = -1;
    Point3 normal;
    double normalImpulseNs = 0.0;
    double penetrationCm = 0.0;
    BallBallContactRegime regime = BallBallContactRegime::NoContact;
    bool velocityImpulseApplied = false;
    Point3 contactTangent;
    Point3 firstContactArmCm;
    Point3 secondContactArmCm;
    Point3 relativeContactVelocityBeforeCmS;
    Point3 relativeContactVelocityAfterCmS;
    Point3 impulseOnSecondNs;
    Point3 firstPositionCorrectionCm;
    Point3 secondPositionCorrectionCm;
    double normalRelativeSpeedBeforeCmS = 0.0;
    double normalRelativeSpeedAfterCmS = 0.0;
    double tangentialImpulseNs = 0.0;
    double frictionCoefficient = 0.0;
    double kineticEnergyBeforeJ = 0.0;
    double kineticEnergyAfterJ = 0.0;
    double positionSlopCm = 0.0;
    CushionContactRegime cushionRegime = CushionContactRegime::NoContact;
    Point3 cushionContactArmCm;
    double cushionContactHeightCm = 0.0;
    Point3 cushionContactVelocityBeforeCmS;
    Point3 cushionContactVelocityAfterCmS;
    Point3 impulseOnBallNs;
    Point3 positionCorrectionCm;
    double restitution = 0.0;
    double noseHeightRatio = 0.0;
    double incidentSpeedCmS = 0.0;
    double maximumRigidIncidentSpeedCmS = 0.0;
    bool rigidDomainExceeded = false;
    bool positionCorrected = false;
    double timeOfImpactSeconds = 0.0;
    double wholeTickTimeOfImpactSeconds = 0.0;
    Point3 firstPositionAtContactCm;
    Point3 secondPositionAtContactCm;
    int pocketId = -1;
    PocketKind pocketKind = PocketKind::Corner;
    PocketBoundaryEventKind pocketBoundaryEvent = PocketBoundaryEventKind::None;
    PocketInteractionPhase pocketPhaseBefore = PocketInteractionPhase::Outside;
    PocketInteractionPhase pocketPhaseAfter = PocketInteractionPhase::Outside;
    PocketLocalPoint pocketLocal;
    Point3 pocketJawCenterCm;
    double pocketJawRadiusCm = 0.0;
    double pocketThroatSignedDistanceCm = 0.0;
    double pocketCaptureSignedDistanceCm = 0.0;
    bool pocketPassable = false;
    // Vertical open-shaft crossings use downward plane distances, not the
    // horizontal pocket-frame depth used by rolling entries.
    bool pocketVerticalShaft = false;
    unsigned long long pocketCaptureSequence = 0;
    bool administrativeResetAfterCapture = false;
    std::uint64_t trajectoryGenerationBefore = 0;
    std::uint64_t trajectoryGenerationAfter = 0;
    Point3 administrativeResetPositionCm;
};

struct SolverEventRecord {
    int eventId = -1;
    int islandId = -1;
    int candidateCount = 0;
    int contactCount = 0;
    int duplicateCandidatesRemoved = 0;
    int velocityIterations = 0;
    int positionIterations = 0;
    double maximumResidualCmS = 0.0;
    double maximumPenetrationCm = 0.0;
    double kineticEnergyBeforeJ = 0.0;
    double kineticEnergyAfterJ = 0.0;
    bool islandLimitExceeded = false;
    const char* failureCode = "converged";
    double wholeTickTimeOfImpactSeconds = 0.0;
    std::vector<SolverPositionProjection> positionProjections;
};

struct PhysicsBallSample {
    Point3 position;
    Point3 velocity;
    Point3 acceleration;
    Point3 angularVelocity;
    float speed = 0.0f;
    bool pocketed = false;
    bool outOfPlay = false;
    int pocketId = -1;
    PocketInteractionPhase pocketPhase = PocketInteractionPhase::Outside;
    std::uint64_t pocketCaptureSequence = 0;
    BallMotionState motionState = BallMotionState::Stationary;
    float contactSlipSpeedCmS = 0.0f;
    double rotationalKineticEnergyJ = 0.0;
};

struct PhysicsControlSample {
    float aimYaw = 0.0f;
    float shotPower = 0.0f;
    bool shotTaken = false;
};

struct PhysicsStepTelemetry {
    PhysicsStepStatus stepStatus = PhysicsStepStatus::Succeeded;
    // Diagnostic context only; never consumed as a gameplay/rules fact.
    std::string failureDetail;
    PhysicsFailureCode failureCode = PhysicsFailureCode::None;
    int failingEventId = -1;
    int failingIslandId = -1;
    std::vector<PhysicsContactRecord> contacts;
    std::vector<SolverEventRecord> solverEvents;
    // Typed private source evidence; rule eligibility is decided by observers.
    std::vector<TableDepartureEvent> tableDepartureEvents;
    std::vector<SurfaceMotionStep> surfaceMotion;
    std::vector<SurfaceMotionAnalyticSegment> analyticSurfaceMotion;
    std::vector<PhysicsPlaneCrossingRecord> planeCrossings;
    std::vector<PhysicsPlaneObservationAttestation> planeAttestations;
    double wholeTickDurationSeconds = 0.0;
    double maximumPenetrationCm = 0.0;
};

struct PhysicsFrame {
    std::uint64_t tick = 0;
    double timeSeconds = 0.0;
    float deltaSeconds = 0.0f;
    std::array<PhysicsBallSample, kBallCount> balls;
    PhysicsControlSample control;
    std::vector<PhysicsContactRecord> contacts;
    std::vector<SolverEventRecord> solverEvents;
    Point3 linearMomentum;
    double translationalKineticEnergyJ = 0.0;
    double rotationalKineticEnergyJ = 0.0;
    double totalKineticEnergyJ = 0.0;
    double maximumPenetrationCm = 0.0;
    PhysicsStepStatus stepStatus = PhysicsStepStatus::Succeeded;
    PhysicsFailureCode failureCode = PhysicsFailureCode::None;
    int failingEventId = -1;
    int failingIslandId = -1;
    bool hasCueImpactInput = false;
    CueImpactInput cueImpactInput;
    bool hasCueContactResult = false;
    CueContactResult cueContactResult;
    bool hasCanonicalCueTipStrokeObservation = false;
    CanonicalCueTipStrokeObservation canonicalCueTipStrokeObservation;
    std::string physicsProfileId;
    PhysicsBoundaryMode boundaryMode = PhysicsBoundaryMode::ProductionTable;
    std::vector<SurfaceMotionStep> surfaceTransitions;
};

Point3 translationalMomentumKgMps(
    const GameState& state, float ballMassKg = kDefaultBallMassKg);
double translationalKineticEnergyJ(
    const GameState& state, float ballMassKg = kDefaultBallMassKg);
PhysicsFrame capturePhysicsFrame(std::uint64_t tick, double timeSeconds, float deltaSeconds,
    const GameState& before, const GameState& after, const PhysicsStepTelemetry& telemetry);
PhysicsFrame capturePhysicsFrame(std::uint64_t tick, double timeSeconds, float deltaSeconds,
    const GameState& before, const GameState& after, const PhysicsStepTelemetry& telemetry,
    const PhysicsProfile& profile,
    PhysicsBoundaryMode boundaryMode = PhysicsBoundaryMode::ProductionTable);

class PhysicsTrace {
public:
    explicit PhysicsTrace(std::size_t capacity = 10000) : capacity_(capacity) {}

    void clear();
    void append(const PhysicsFrame& frame);
    const std::deque<PhysicsFrame>& frames() const { return frames_; }
    std::size_t droppedFrames() const { return droppedFrames_; }

private:
    std::size_t capacity_;
    std::size_t droppedFrames_ = 0;
    std::deque<PhysicsFrame> frames_;
};

}  // namespace billiardgl
