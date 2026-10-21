#pragma once

#include "rules/double_hit_classifier.h"
#include "rules/frozen_cushion_classifier.h"
#include "rules/push_classifier.h"

#include <array>
#include <cstdint>
#include <string>
#include <vector>

namespace billiardgl::rules {

enum class BallGroup {
    Open,
    Solids,
    Stripes,
};

enum class MatchPhase {
    Break,
    OpenTable,
    GroupsAssigned,
    Complete,
};

enum class FoulReason {
    None,
    IllegalBreak,
    NoObjectBallContact,
    WrongFirstContact,
    NoRailAfterContact,
    CueBallScratch,
    CueBallOffTable,
    ObjectBallOffTable,
    BreakCueBallPastHeadString,
    DoubleHit,
    Push,
    FrozenCushionObligation,
    IllegalEightBall,
};

struct ChineseEightBallRuleset {
    std::string id = "wpa_heyball_2025_08_16_v1";
    int schemaVersion = 1;
    int minimumBreakObjectBallsToRail = 4;
    bool pocketedObjectBallMakesLegalBreak = true;
    bool requireCalledEightPocket = false;
};

struct MatchState {
    MatchPhase phase = MatchPhase::Break;
    int currentPlayer = 0;
    std::array<BallGroup, 2> groups{{BallGroup::Open, BallGroup::Open}};
    bool ballInHand = false;
    bool groupChoicePending = false;
    int winner = -1;
};

enum class BreakCushionBoundaryKind {
    StraightCushion,
    PocketJaw,
};

enum class BreakCushionContactProvenance {
    DiscretePhysics,
    ContinuousSolver,
};

enum class BreakEvidenceOrigin {
    DirectOrSynthetic,
    AuthoritativeShotFactCollector,
};

enum class CueBallScratchEvidenceOrigin {
    DirectOrSynthetic,
    AuthoritativeShotFactCollector,
};

enum class HeadStringCrossingEvidenceOrigin {
    DirectOrSynthetic,
    AuthoritativeShotFactCollector,
};

struct CueCenterPlaneCrossingEvidence {
    std::string observationId;
    std::uint64_t physicsTick = 0;
    double wholeTickTimeOfImpactSeconds = 0.0;
    int segmentOrdinal = -1;
    double signedDistanceBeforeCm = 0.0;
    double signedDistanceAfterCm = 0.0;
    double axisVelocityAtCrossingCmS = 0.0;
    int canonicalEventOrdinal = -1;
};

struct FirstCueObjectContactEvidence {
    int objectBallId = -1;
    std::uint64_t physicsTick = 0;
    double wholeTickTimeOfImpactSeconds = 0.0;
    int solverEventId = -1;
    int physicsContactOrdinal = -1;
    double objectBallCenterWorldXCm = 0.0;
    double objectBallCenterWorldZCm = 0.0;
};

// Canonical policy-independent proof that the physics pocket topology emitted
// one cue-ball capture on a succeeded tick. Cue-ball replacement deliberately
// leaves BallState::pocketed false, so rules must consume this event evidence
// rather than infer a scratch from the replacement state.
struct CueBallPocketCaptureEvidence {
    int pocketId = -1;
    std::uint64_t captureSequence = 0;
    std::uint64_t physicsTick = 0;
    int physicsContactOrdinal = -1;
    int canonicalEventOrdinal = -1;
    double timeOfImpactSeconds = 0.0;
};

inline bool operator==(const CueBallPocketCaptureEvidence& left,
                       const CueBallPocketCaptureEvidence& right) noexcept
{
    return left.pocketId == right.pocketId &&
        left.captureSequence == right.captureSequence &&
        left.physicsTick == right.physicsTick &&
        left.physicsContactOrdinal == right.physicsContactOrdinal &&
        left.canonicalEventOrdinal == right.canonicalEventOrdinal &&
        left.timeOfImpactSeconds == right.timeOfImpactSeconds;
}

// Policy-independent evidence from succeeded physics steps. These records do
// not decide whether a jaw, correction-only contact, or positive impulse
// counts toward the legal-break threshold.
struct BreakObjectCushionContact {
    int objectBallId = -1;
    BreakCushionBoundaryKind boundaryKind =
        BreakCushionBoundaryKind::StraightCushion;
    BreakCushionContactProvenance provenance =
        BreakCushionContactProvenance::DiscretePhysics;
    bool velocityImpulseApplied = false;
    bool positionCorrected = false;
    double normalImpulseNs = 0.0;
    double solverResidualCmS = 0.0;
    double solverProjectionCm = 0.0;
    double timeOfImpactSeconds = 0.0;
    int solverEventId = -1;
    int solverIslandId = -1;
    std::uint64_t physicsTick = 0;
    int canonicalContactOrdinal = -1;
};

inline bool operator==(const BreakObjectCushionContact& left,
                       const BreakObjectCushionContact& right) noexcept
{
    return left.objectBallId == right.objectBallId &&
        left.boundaryKind == right.boundaryKind &&
        left.provenance == right.provenance &&
        left.velocityImpulseApplied == right.velocityImpulseApplied &&
        left.positionCorrected == right.positionCorrected &&
        left.normalImpulseNs == right.normalImpulseNs &&
        left.solverResidualCmS == right.solverResidualCmS &&
        left.solverProjectionCm == right.solverProjectionCm &&
        left.timeOfImpactSeconds == right.timeOfImpactSeconds &&
        left.solverEventId == right.solverEventId &&
        left.solverIslandId == right.solverIslandId &&
        left.physicsTick == right.physicsTick &&
        left.canonicalContactOrdinal == right.canonicalContactOrdinal;
}

// Physics and collision collection report facts only. No rule interpretation
// belongs in the solver.
struct ShotFacts {
    int shooter = 0;
    bool breakShot = false;
    int firstContactBall = -1;
    // Canonical, strictly increasing object-ball set from the first physics
    // tick that contains cue/object contact. Empty preserves scalar-only v1
    // callers; when both are present the scalar is the set's first element.
    std::vector<int> firstContactBalls;
    std::vector<BreakObjectCushionContact> breakObjectCushionContacts;
    BreakEvidenceOrigin breakEvidenceOrigin =
        BreakEvidenceOrigin::DirectOrSynthetic;
    bool breakEvidenceComplete = false;
    int distinctObjectBallsToRail = 0;
    bool anyRailAfterFirstContact = false;
    bool cueBallPocketed = false;
    std::vector<CueBallPocketCaptureEvidence> cueBallPocketCaptures;
    CueBallScratchEvidenceOrigin cueBallScratchEvidenceOrigin =
        CueBallScratchEvidenceOrigin::DirectOrSynthetic;
    bool cueBallScratchEvidenceComplete = false;
    bool behindHeadStringFirstContactRestrictionActive = false;
    double headStringWorldZCm = 0.0;
    std::vector<CueCenterPlaneCrossingEvidence>
        cueCenterPlaneCrossingEvents;
    std::vector<FirstCueObjectContactEvidence>
        firstCueObjectContactEvidence;
    bool cueBallCrossedHeadStringBeforeFirstContact = false;
    HeadStringCrossingEvidenceOrigin headStringCrossingEvidenceOrigin =
        HeadStringCrossingEvidenceOrigin::DirectOrSynthetic;
    bool headStringCrossingEvidenceComplete = false;
    bool cueBallOffTable = false;
    std::vector<int> objectBallsOffTable;
    std::vector<int> pocketedBallsInOrder;
    int solidsRemainingBefore = 7;
    int stripesRemainingBefore = 7;
    int calledPocket = -1;
    int eightBallPocket = -1;
    bool hasCanonicalDoubleHitClassification = false;
    CanonicalDoubleHitClassification canonicalDoubleHitClassification;
    bool hasCanonicalPushClassification = false;
    CanonicalPushClassification canonicalPushClassification;
    bool hasCanonicalFrozenCushionClassification = false;
    CanonicalFrozenCushionClassification
        canonicalFrozenCushionClassification;
};

enum class ShotFactsValidationError {
    None,
    OutOfRange,
    DuplicateBall,
    NonCanonicalOrder,
    ConflictingBallState,
    InvalidBreakEvidence,
    InvalidCueBallScratchEvidence,
    InvalidHeadStringCrossingEvidence,
    InvalidDoubleHitClassification,
    InvalidPushClassification,
    InvalidFrozenCushionClassification,
};

struct AdjudicationResult {
    bool accepted = false;
    bool foul = false;
    bool continueTurn = false;
    bool ballInHand = false;
    bool ballInHandBehindHeadString = false;
    bool eightBallMustBeSpotted = false;
    bool rerackAllowed = false;
    bool gameOver = false;
    bool groupChoiceRequired = false;
    int nextPlayer = 0;
    int winner = -1;
    MatchState nextState;
    std::vector<FoulReason> reasons;
    std::vector<std::string> textKeys;
    std::vector<std::string> decisionOptions;
};

class ChineseEightBallRules {
public:
    explicit ChineseEightBallRules(ChineseEightBallRuleset ruleset = {});

    const ChineseEightBallRuleset& ruleset() const noexcept { return ruleset_; }
    AdjudicationResult adjudicate(const MatchState& state,
                                  const ShotFacts& facts) const;

private:
    ChineseEightBallRuleset ruleset_;
};

// Shared by adjudication and the WPA-II-08 all-behind authority. `groupCleared`
// is an authoritative pre-shot fact computed from the same active-ball set.
bool isLegalFirstContactBall(const MatchState& state, int ball,
                             bool groupCleared) noexcept;

const char* foulReasonCode(FoulReason reason) noexcept;
const char* foulReasonTextKey(FoulReason reason) noexcept;
ShotFactsValidationError validateShotFacts(const ShotFacts& facts) noexcept;
const char* shotFactsValidationErrorCode(
    ShotFactsValidationError error) noexcept;

}  // namespace billiardgl::rules
