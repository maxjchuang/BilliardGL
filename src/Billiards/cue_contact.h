#pragma once

#include "cue_impact.h"
#include "game_state.h"
#include "physics_profile.h"

#include <array>
#include <cstdint>
#include <string>
#include <vector>

namespace billiardgl {

enum class CueTipIntervalOrigin {
    AnalyticCueContactSolver,
    CoupledCueContactSolver,
    AuthoritativeCueMotionObserver,
};

struct CanonicalCueTipContactInterval {
    int ordinal = -1;
    double startTimeSeconds = 0.0;
    double endTimeSeconds = 0.0;
    double startTickOffsetSeconds = 0.0;
    double endTickOffsetSeconds = 0.0;
    Point3 contactPoint;
    Point3 contactNormal;
    Point3 cueVelocityStart;
    Point3 cueVelocityEnd;
    Point3 cueBallVelocityStart;
    Point3 cueBallVelocityEnd;
    double normalImpulseNs = 0.0;
    double tangentialImpulseNs = 0.0;
    // Canonical line integral of cue-tip motion relative to the cue-ball
    // centre while this interval is active.  It is report-only: rules must
    // never compare it with a numeric push threshold.
    double relativeTipBallPathLengthCm = 0.0;
    bool relativeTipBallPathLengthAttested = false;
    CueTipIntervalOrigin origin =
        CueTipIntervalOrigin::AnalyticCueContactSolver;
    std::vector<int> solverEventIds;
};

struct CanonicalCueTipStrokeObservation {
    std::string schemaId = "canonical_cue_tip_stroke_observation_v1";
    std::string shotIdentity;
    int actorPlayerId = -1;
    std::string rackIdentity;
    std::uint64_t rulesRevision = 0;
    std::string rulesStateHash;
    std::uint64_t startTick = 0;
    std::uint64_t endTick = 0;
    std::vector<CanonicalCueTipContactInterval> intervals;
    bool observationStartedBeforeFirstContact = false;
    bool observationEndedAfterAuthoritativeRelease = false;
    bool noDroppedIntervals = false;
    bool allValuesFinite = false;
    bool canonicalOrder = false;
    bool successfulLaunchCommit = false;
    bool fullStrokeComplete = false;
    std::string digest;
};

struct CanonicalCueTipStrokeObservationValidation {
    bool valid = false;
    bool complete = false;
    std::string error;
};

class CanonicalCueTipStrokeObserver {
public:
    bool begin(const CanonicalCueTipStrokeObservation& identity);
    bool append(const CanonicalCueTipContactInterval& interval);
    CanonicalCueTipStrokeObservationValidation finish(
        std::uint64_t endTick);
    void rollback();
    bool active() const { return active_; }
    bool committed() const { return committed_; }
    const CanonicalCueTipStrokeObservation& observation() const
    {
        return observation_;
    }
private:
    bool active_ = false;
    bool committed_ = false;
    CanonicalCueTipStrokeObservation observation_;
};

CanonicalCueTipStrokeObservationValidation
validateCanonicalCueTipStrokeObservation(
    CanonicalCueTipStrokeObservation& observation);
const char* cueTipIntervalOriginName(CueTipIntervalOrigin origin);

enum class CueContactRegime {
    Unsupported,
    Stick,
    Slip,
    Miscue,
    Released
};

struct CueContactBallSample {
    int index = -1;
    Point3 positionCm;
    Point3 velocityCmS;
    Point3 accelerationCmS2;
    Point3 angularVelocityRadS;
};

struct CueContactConstraintSample {
    int kind = 0;
    int firstBall = -1;
    int secondBall = -1;
    int featureId = -1;
    Point3 normal;
    double normalImpulseNs = 0.0;
    double tangentialImpulseNs = 0.0;
    double penetrationCm = 0.0;
    double residualCmS = 0.0;
};

struct CueContactMicrostep {
    int index = 0;
    double timeSeconds = 0.0;
    double cuePositionM = 0.0;
    double cueVelocityMS = 0.0;
    double cueAccelerationMS2 = 0.0;
    double compressionM = 0.0;
    double compressionRateMS = 0.0;
    double normalForceN = 0.0;
    double tangentialForceN = 0.0;
    double normalImpulseNs = 0.0;
    double tangentialImpulseNs = 0.0;
    double kineticEnergyJ = 0.0;
    double elasticEnergyJ = 0.0;
    double dissipatedEnergyJ = 0.0;
    double energyResidualJ = 0.0;
    double maximumPenetrationCm = 0.0;
    double solverResidualCmS = 0.0;
    int solverIterations = 0;
    CueContactRegime regime = CueContactRegime::Stick;
    std::array<CueContactBallSample, kBallCount> balls;
    std::vector<CueContactConstraintSample> contacts;
};

struct CueContactResult {
    CueContactRegime regime = CueContactRegime::Unsupported;
    bool applied = false;
    std::string error;
    double frictionCoefficient = 0.0;
    double normalImpulseNs = 0.0;
    double tangentialImpulseNs = 0.0;
    double normalRelativeSpeedBeforeMS = 0.0;
    double tangentialRelativeSpeedBeforeMS = 0.0;
    double inputKineticEnergyJ = 0.0;
    double outputKineticEnergyJ = 0.0;
    std::array<double, 3> tangentialRelativeVelocityBeforeMS{{0.0, 0.0, 0.0}};
    std::array<double, 3> contactArmM{{0.0, 0.0, 0.0}};
    std::array<double, 3> contactNormal{{0.0, 0.0, 0.0}};
    std::array<double, 3> impulseNs{{0.0, 0.0, 0.0}};
    std::array<double, 3> cueVelocityBeforeMS{{0.0, 0.0, 0.0}};
    std::array<double, 3> cueVelocityAfterMS{{0.0, 0.0, 0.0}};
    std::array<double, 3> ballVelocityBeforeMS{{0.0, 0.0, 0.0}};
    std::array<double, 3> ballVelocityAfterMS{{0.0, 0.0, 0.0}};
    std::array<double, 3> ballAngularVelocityBeforeRadS{{0.0, 0.0, 0.0}};
    std::array<double, 3> ballAngularVelocityAfterRadS{{0.0, 0.0, 0.0}};
    int microtraceSchemaVersion = 1;
    std::vector<CueContactMicrostep> microsteps;
    std::vector<CanonicalCueTipContactInterval> canonicalTipIntervals;
};

CueContactResult resolveCueContact(BallState& ball, const CueImpactInput& input,
    const BallProperties& ballProperties, const CueProperties& cueProperties);
const char* cueContactRegimeName(CueContactRegime regime);

}  // namespace billiardgl
