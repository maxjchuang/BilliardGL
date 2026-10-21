#include "rules/chinese_eight_ball_rules.h"

#include <cstdlib>
#include <iostream>
#include <limits>
#include <stdexcept>
#include <utility>

namespace {

using namespace billiardgl::rules;

void expect(bool condition, const char* message)
{
    if (!condition) throw std::runtime_error(message);
}

bool has(const AdjudicationResult& result, FoulReason reason)
{
    for (FoulReason candidate : result.reasons)
        if (candidate == reason) return true;
    return false;
}

MatchState assigned(BallGroup shooter)
{
    MatchState state;
    state.phase = MatchPhase::GroupsAssigned;
    state.groups[0] = shooter;
    state.groups[1] = shooter == BallGroup::Solids ? BallGroup::Stripes
                                                   : BallGroup::Solids;
    return state;
}

ShotFacts legalShot()
{
    ShotFacts facts;
    facts.firstContactBall = 1;
    facts.anyRailAfterFirstContact = true;
    return facts;
}

CanonicalDoubleHitClassification doubleHitClassification(
    int intervals, CanonicalDoubleHitResult result)
{
    CanonicalDoubleHitClassification value;
    value.shotIdentity = "shot.rules.d5c";
    value.result = result;
    value.canonicalTipIntervalCount = intervals;
    value.cueObjectTipOrder =
        billiardgl::CanonicalCueObjectTipOrder::ContactBeforeTipSeparation;
    value.signedCloseCaseBound = result ==
        CanonicalDoubleHitResult::SignedRefereeReviewRequired;
    value.cueTipObservationDigest = "tip.sha";
    value.cueObjectObservationDigest = "order.sha";
    const auto validation = validateCanonicalDoubleHitClassification(value);
    expect(validation.valid && validation.complete,
        "double-hit rules fixture validates");
    return value;
}

void runDoubleHitAdjudicationTests()
{
    ChineseEightBallRules rules;
    MatchState state = assigned(BallGroup::Solids);
    ShotFacts foul = legalShot();
    foul.hasCanonicalDoubleHitClassification = true;
    foul.canonicalDoubleHitClassification = doubleHitClassification(
        2, CanonicalDoubleHitResult::DoubleHitFoul);
    const auto result = rules.adjudicate(state, foul);
    expect(result.accepted && result.foul &&
        has(result, FoulReason::DoubleHit) && result.ballInHand &&
        result.nextPlayer == 1,
        "automatic double-hit adds exact foul and opponent ball in hand");
    const auto retry = rules.adjudicate(state, foul);
    expect(retry.accepted && retry.reasons == result.reasons &&
        retry.nextState.currentPlayer == result.nextState.currentPlayer &&
        retry.ballInHand == result.ballInHand,
        "exact adjudication retry is deterministic and zero-delta");

    ShotFacts review = legalShot();
    review.hasCanonicalDoubleHitClassification = true;
    review.canonicalDoubleHitClassification = doubleHitClassification(
        1, CanonicalDoubleHitResult::SignedRefereeReviewRequired);
    const auto reviewResult = rules.adjudicate(state, review);
    expect(reviewResult.accepted && !reviewResult.foul &&
        !has(reviewResult, FoulReason::DoubleHit),
        "signed close-case review does not classify a foul");

    ShotFacts tampered = foul;
    tampered.canonicalDoubleHitClassification.digest = "tampered";
    expect(validateShotFacts(tampered) ==
        ShotFactsValidationError::InvalidDoubleHitClassification &&
        !rules.adjudicate(state, tampered).accepted,
        "tampered classification rejects adjudication atomically");
}

void attestCueScratch(ShotFacts& facts, int pocketId = 2,
                      std::uint64_t tick = 7,
                      std::uint64_t captureSequence = 11)
{
    facts.cueBallPocketed = true;
    facts.cueBallScratchEvidenceOrigin =
        CueBallScratchEvidenceOrigin::AuthoritativeShotFactCollector;
    facts.cueBallScratchEvidenceComplete = true;
    CueBallPocketCaptureEvidence evidence;
    evidence.pocketId = pocketId;
    evidence.captureSequence = captureSequence;
    evidence.physicsTick = tick;
    evidence.physicsContactOrdinal = 0;
    evidence.canonicalEventOrdinal = 0;
    evidence.timeOfImpactSeconds = 0.01;
    facts.cueBallPocketCaptures = {evidence};
}

BreakObjectCushionContact breakContact(
    int ball, std::uint64_t tick, int ordinal,
    BreakCushionBoundaryKind boundary =
        BreakCushionBoundaryKind::StraightCushion,
    bool velocityImpulseApplied = true,
    bool positionCorrected = false)
{
    BreakObjectCushionContact contact;
    contact.objectBallId = ball;
    contact.boundaryKind = boundary;
    contact.provenance =
        BreakCushionContactProvenance::ContinuousSolver;
    contact.velocityImpulseApplied = velocityImpulseApplied;
    contact.positionCorrected = positionCorrected;
    contact.normalImpulseNs = velocityImpulseApplied ? 0.1 : 0.0;
    contact.solverResidualCmS = 0.001;
    contact.solverProjectionCm = positionCorrected ? 0.002 : 0.0;
    contact.solverEventId = ordinal + 10;
    contact.solverIslandId = ordinal + 20;
    contact.physicsTick = tick;
    contact.canonicalContactOrdinal = ordinal;
    return contact;
}

void runScenarioTable()
{
    ChineseEightBallRules rules;
    {
        MatchState state;
        ShotFacts facts;
        facts.breakShot = true;
        facts.firstContactBall = 1;
        facts.distinctObjectBallsToRail = 4;
        const auto result = rules.adjudicate(state, facts);
        expect(result.accepted && !result.foul, "four-rail break should be legal");
        expect(result.nextState.phase == MatchPhase::OpenTable,
               "legal dry break should open table and transfer turn");
        expect(result.nextPlayer == 1, "dry break should transfer turn");
    }
    {
        MatchState state;
        ShotFacts facts;
        facts.breakShot = true;
        facts.firstContactBall = 1;
        facts.distinctObjectBallsToRail = 3;
        const auto result = rules.adjudicate(state, facts);
        expect(!result.foul && has(result, FoulReason::IllegalBreak),
               "three-rail dry break should report illegal_break");
        expect(!result.ballInHand && result.nextPlayer == 1 &&
               result.rerackAllowed && result.decisionOptions.size() == 3,
               "illegal break should expose accept/rerack options without ball in hand");
    }
    {
        MatchState state;
        ShotFacts facts;
        facts.breakShot = true;
        facts.firstContactBall = 1;
        facts.pocketedBallsInOrder = {8};
        const auto result = rules.adjudicate(state, facts);
        expect(!result.gameOver && result.eightBallMustBeSpotted &&
               result.continueTurn && result.rerackAllowed,
               "eight on a legal break should spot-or-rerack, never win or lose");
        expect(!has(result, FoulReason::IllegalBreak),
               "eight on the break counts as an object ball pocket");
    }
    {
        MatchState state;
        state.phase = MatchPhase::OpenTable;
        ShotFacts facts;
        facts.firstContactBall = 9;
        facts.pocketedBallsInOrder = {9};
        const auto result = rules.adjudicate(state, facts);
        expect(!result.foul && result.continueTurn,
               "legal open-table pocket should continue");
        expect(result.nextState.groups[0] == BallGroup::Stripes &&
               result.nextState.groups[1] == BallGroup::Solids,
               "first legal stripe should assign groups");
    }
    {
        MatchState state;
        state.phase = MatchPhase::OpenTable;
        ShotFacts facts;
        facts.firstContactBall = 8;
        facts.anyRailAfterFirstContact = true;
        const auto result = rules.adjudicate(state, facts);
        expect(has(result, FoulReason::WrongFirstContact),
               "8-ball first on an open table should be a foul");
    }
    {
        MatchState state = assigned(BallGroup::Solids);
        ShotFacts facts = legalShot();
        facts.firstContactBall = 9;
        facts.anyRailAfterFirstContact = false;
        const auto result = rules.adjudicate(state, facts);
        expect(has(result, FoulReason::WrongFirstContact) &&
               has(result, FoulReason::NoRailAfterContact),
               "one shot may report all independently explainable fouls");
        expect(result.textKeys.size() == result.reasons.size(),
               "every foul must provide a HUD text key");
    }
    {
        MatchState state = assigned(BallGroup::Solids);
        ShotFacts facts = legalShot();
        attestCueScratch(facts);
        facts.pocketedBallsInOrder = {3};
        const auto result = rules.adjudicate(state, facts);
        expect(has(result, FoulReason::CueBallScratch) && result.ballInHand,
               "scratch should be explicit and award ball in hand");
        expect(!result.continueTurn && result.nextPlayer == 1,
               "a pocket cannot retain turn after scratch");
    }
    {
        MatchState state = assigned(BallGroup::Solids);
        ShotFacts facts = legalShot();
        facts.solidsRemainingBefore = 0;
        facts.firstContactBall = 8;
        facts.anyRailAfterFirstContact = false;
        facts.pocketedBallsInOrder = {8};
        const auto result = rules.adjudicate(state, facts);
        expect(result.gameOver && result.winner == 0 && !result.foul,
               "legal eight after clearing group should win");
        expect(!has(result, FoulReason::NoRailAfterContact),
               "pocketing the 8 satisfies the post-contact pocket-or-rail condition");
    }
    {
        MatchState state = assigned(BallGroup::Solids);
        ShotFacts facts = legalShot();
        facts.solidsRemainingBefore = 2;
        facts.firstContactBall = 8;
        facts.pocketedBallsInOrder = {8};
        const auto result = rules.adjudicate(state, facts);
        expect(result.gameOver && result.winner == 1 &&
               has(result, FoulReason::IllegalEightBall),
               "early eight should lose with reason code");
    }
    {
        MatchState state = assigned(BallGroup::Solids);
        ShotFacts facts = legalShot();
        facts.solidsRemainingBefore = 1;
        facts.firstContactBall = 7;
        facts.anyRailAfterFirstContact = false;
        facts.pocketedBallsInOrder = {7, 8};
        const auto result = rules.adjudicate(state, facts);
        expect(result.gameOver && result.winner == 1 &&
               has(result, FoulReason::IllegalEightBall),
               "last group ball and 8 on one stroke is an early-eight loss");
        expect(!has(result, FoulReason::NoRailAfterContact),
               "the pocketed numbered balls satisfy the rail condition");
    }
    {
        MatchState state = assigned(BallGroup::Solids);
        ShotFacts facts = legalShot();
        facts.objectBallsOffTable = {8};
        const auto result = rules.adjudicate(state, facts);
        expect(result.gameOver && result.winner == 1 && result.foul &&
               has(result, FoulReason::IllegalEightBall),
               "8-ball off table is an immediate loss");
        expect(has(result, FoulReason::ObjectBallOffTable),
               "8-ball off table also preserves the general off-table reason");
    }
    {
        MatchState state = assigned(BallGroup::Solids);
        ShotFacts facts = legalShot();
        facts.objectBallsOffTable = {3};
        const auto result = rules.adjudicate(state, facts);
        expect(result.foul && !result.gameOver &&
               has(result, FoulReason::ObjectBallOffTable) &&
               !has(result, FoulReason::IllegalEightBall),
               "non-8 off-table balls retain the current general-foul policy");
    }
    {
        MatchState complete;
        complete.phase = MatchPhase::Complete;
        complete.winner = 1;
        const auto result = rules.adjudicate(complete, legalShot());
        expect(!result.accepted, "completed match should reject further shots");
    }
}

void runFactValidationTests()
{
    ChineseEightBallRules rules;
    MatchState state;
    auto expectInvalid = [&](const ShotFacts& facts,
                             ShotFactsValidationError expected,
                             const char* message) {
        expect(validateShotFacts(facts) == expected, message);
        expect(!rules.adjudicate(state, facts).accepted,
               "invalid facts must fail closed in the rule core");
    };

    ShotFacts duplicateFirstContacts = legalShot();
    duplicateFirstContacts.firstContactBalls = {1, 1};
    expectInvalid(duplicateFirstContacts,
                  ShotFactsValidationError::DuplicateBall,
                  "duplicate simultaneous first contacts are rejected");

    ShotFacts unsortedFirstContacts = legalShot();
    unsortedFirstContacts.firstContactBalls = {9, 1};
    expectInvalid(unsortedFirstContacts,
                  ShotFactsValidationError::NonCanonicalOrder,
                  "simultaneous first contacts require canonical sorting");

    ShotFacts firstContactOutOfRange = legalShot();
    firstContactOutOfRange.firstContactBalls = {0};
    expectInvalid(firstContactOutOfRange,
                  ShotFactsValidationError::OutOfRange,
                  "simultaneous first contacts must be object balls");

    ShotFacts scalarSetConflict = legalShot();
    scalarSetConflict.firstContactBall = 9;
    scalarSetConflict.firstContactBalls = {1, 9};
    expectInvalid(scalarSetConflict,
                  ShotFactsValidationError::ConflictingBallState,
                  "legacy scalar must match the canonical set projection");

    ShotFacts setOnly = legalShot();
    setOnly.firstContactBall = -1;
    setOnly.firstContactBalls = {1, 9};
    expect(validateShotFacts(setOnly) == ShotFactsValidationError::None,
           "new callers may provide a canonical set without the legacy scalar");

    ShotFacts duplicatePocket = legalShot();
    duplicatePocket.pocketedBallsInOrder = {1, 1};
    expectInvalid(duplicatePocket, ShotFactsValidationError::DuplicateBall,
                  "duplicate pocket facts are rejected");

    ShotFacts duplicateOffTable = legalShot();
    duplicateOffTable.objectBallsOffTable = {2, 2};
    expectInvalid(duplicateOffTable, ShotFactsValidationError::DuplicateBall,
                  "duplicate off-table facts are rejected");

    ShotFacts conflicting = legalShot();
    conflicting.pocketedBallsInOrder = {3};
    conflicting.objectBallsOffTable = {3};
    expectInvalid(conflicting,
                  ShotFactsValidationError::ConflictingBallState,
                  "one ball cannot be pocketed and off table");

    ShotFacts conflictingEight = legalShot();
    conflictingEight.pocketedBallsInOrder = {8};
    conflictingEight.objectBallsOffTable = {8};
    expectInvalid(conflictingEight,
                  ShotFactsValidationError::ConflictingBallState,
                  "8-ball pocket/off-table conflict is rejected");

    ShotFacts conflictingCue = legalShot();
    attestCueScratch(conflictingCue);
    conflictingCue.cueBallOffTable = true;
    expectInvalid(conflictingCue,
                  ShotFactsValidationError::ConflictingBallState,
                  "cue ball cannot be pocketed and off table");

    ShotFacts untrustedScratch = legalShot();
    untrustedScratch.cueBallPocketed = true;
    expectInvalid(untrustedScratch,
                  ShotFactsValidationError::InvalidCueBallScratchEvidence,
                  "direct cue scratch cannot bypass authoritative capture evidence");

    ShotFacts incompleteScratch = legalShot();
    incompleteScratch.cueBallScratchEvidenceOrigin =
        CueBallScratchEvidenceOrigin::AuthoritativeShotFactCollector;
    expectInvalid(incompleteScratch,
                  ShotFactsValidationError::InvalidCueBallScratchEvidence,
                  "authoritative origin without completeness fails closed");

    ShotFacts conflictingCueList = legalShot();
    conflictingCueList.cueBallOffTable = true;
    conflictingCueList.pocketedBallsInOrder = {0};
    expectInvalid(conflictingCueList,
                  ShotFactsValidationError::ConflictingBallState,
                  "cue pocket list conflicts with cue ball off table");

    ShotFacts orphanEightPocket = legalShot();
    orphanEightPocket.eightBallPocket = 2;
    expectInvalid(orphanEightPocket,
                  ShotFactsValidationError::ConflictingBallState,
                  "8-ball pocket id requires an 8-ball pocket fact");

    ShotFacts pocketOutOfRange = legalShot();
    pocketOutOfRange.pocketedBallsInOrder = {16};
    expectInvalid(pocketOutOfRange, ShotFactsValidationError::OutOfRange,
                  "out-of-range pocket facts are rejected");

    ShotFacts offTableOutOfRange = legalShot();
    offTableOutOfRange.objectBallsOffTable = {0};
    expectInvalid(offTableOutOfRange, ShotFactsValidationError::OutOfRange,
                  "cue ball is invalid in the object off-table list");

    ShotFacts scalarOutOfRange = legalShot();
    scalarOutOfRange.firstContactBall = 16;
    expectInvalid(scalarOutOfRange, ShotFactsValidationError::OutOfRange,
                  "out-of-range scalar facts are rejected");

    expect(std::string(shotFactsValidationErrorCode(
               ShotFactsValidationError::DuplicateBall)) ==
               "rules_facts_duplicate" &&
           std::string(shotFactsValidationErrorCode(
               ShotFactsValidationError::ConflictingBallState)) ==
               "rules_facts_conflict",
           "fact validation errors expose stable runtime codes");
}

void runBreakEvidenceValidationTests()
{
    ChineseEightBallRules rules;
    MatchState breakState;
    auto authoritative = [] {
        ShotFacts facts;
        facts.breakShot = true;
        facts.firstContactBall = 1;
        facts.distinctObjectBallsToRail = 3;
        facts.breakEvidenceOrigin =
            BreakEvidenceOrigin::AuthoritativeShotFactCollector;
        facts.breakEvidenceComplete = true;
        facts.breakObjectCushionContacts = {
            breakContact(2, 4, 0),
            breakContact(3, 4, 1,
                BreakCushionBoundaryKind::PocketJaw),
            breakContact(4, 5, 2,
                BreakCushionBoundaryKind::StraightCushion, false, true),
        };
        return facts;
    };
    auto expectBreakEvidenceInvalid = [&](const ShotFacts& facts,
                                          const char* message) {
        expect(validateShotFacts(facts) ==
                   ShotFactsValidationError::InvalidBreakEvidence,
               message);
        expect(!rules.adjudicate(breakState, facts).accepted,
               "invalid authoritative break evidence must fail closed");
    };

    const ShotFacts valid = authoritative();
    expect(validateShotFacts(valid) == ShotFactsValidationError::None,
           "canonical complete collector evidence is accepted");
    const auto result = rules.adjudicate(breakState, valid);
    expect(result.accepted && has(result, FoulReason::IllegalBreak),
           "policy-neutral jaw and correction metadata must not reinterpret the legacy three-object threshold");

    ShotFacts direct = valid;
    direct.breakEvidenceOrigin = BreakEvidenceOrigin::DirectOrSynthetic;
    direct.breakEvidenceComplete = false;
    direct.breakObjectCushionContacts.clear();
    expect(validateShotFacts(direct) == ShotFactsValidationError::None &&
               rules.adjudicate(breakState, direct).accepted,
           "legacy direct facts remain valid for CPU callers without product provenance");

    ShotFacts incomplete = valid;
    incomplete.breakEvidenceComplete = false;
    expectBreakEvidenceInvalid(incomplete,
        "authoritative break evidence requires a complete attestation");

    ShotFacts unsorted = valid;
    std::swap(unsorted.breakObjectCushionContacts[0],
              unsorted.breakObjectCushionContacts[1]);
    expectBreakEvidenceInvalid(unsorted,
        "authoritative contact evidence must use canonical ordering");

    ShotFacts duplicate = valid;
    duplicate.breakObjectCushionContacts.insert(
        duplicate.breakObjectCushionContacts.begin() + 1,
        duplicate.breakObjectCushionContacts.front());
    for (std::size_t index = 0;
         index < duplicate.breakObjectCushionContacts.size(); ++index)
        duplicate.breakObjectCushionContacts[index].canonicalContactOrdinal =
            static_cast<int>(index);
    expectBreakEvidenceInvalid(duplicate,
        "duplicate physical evidence rows are rejected");

    ShotFacts wrongOrdinal = valid;
    wrongOrdinal.breakObjectCushionContacts[1].canonicalContactOrdinal = 7;
    expectBreakEvidenceInvalid(wrongOrdinal,
        "canonical evidence ordinals are contiguous and bound to order");

    ShotFacts countMismatch = valid;
    countMismatch.distinctObjectBallsToRail = 2;
    expectBreakEvidenceInvalid(countMismatch,
        "authoritative evidence identities must match the legacy scalar projection");

    ShotFacts nonfinite = valid;
    nonfinite.breakObjectCushionContacts[0].normalImpulseNs =
        std::numeric_limits<double>::quiet_NaN();
    expectBreakEvidenceInvalid(nonfinite,
        "nonfinite physics evidence is rejected");

    ShotFacts directWithEvidence = valid;
    directWithEvidence.breakEvidenceOrigin =
        BreakEvidenceOrigin::DirectOrSynthetic;
    expectBreakEvidenceInvalid(directWithEvidence,
        "direct facts cannot claim authoritative collector evidence");

    ShotFacts nonBreak = valid;
    nonBreak.breakShot = false;
    expectBreakEvidenceInvalid(nonBreak,
        "break evidence cannot be attached to a non-break shot");

    expect(std::string(shotFactsValidationErrorCode(
               ShotFactsValidationError::InvalidBreakEvidence)) ==
               "rules_facts_break_evidence_invalid",
           "break evidence validation exposes a stable fail-closed code");
}

void runSelectedBreakThresholdPolicyTests()
{
    ChineseEightBallRules rules;
    MatchState state;
    auto canonical = [] {
        ShotFacts facts;
        facts.breakShot = true;
        facts.firstContactBall = 1;
        facts.breakEvidenceOrigin =
            BreakEvidenceOrigin::AuthoritativeShotFactCollector;
        facts.breakEvidenceComplete = true;
        return facts;
    };
    auto setContacts = [](ShotFacts& facts,
                          std::vector<BreakObjectCushionContact> contacts) {
        facts.breakObjectCushionContacts = std::move(contacts);
        facts.distinctObjectBallsToRail = 0;
        int previousBall = -1;
        for (std::size_t index = 0; index <
             facts.breakObjectCushionContacts.size(); ++index) {
            auto& contact = facts.breakObjectCushionContacts[index];
            contact.canonicalContactOrdinal = static_cast<int>(index);
            if (contact.objectBallId != previousBall) {
                ++facts.distinctObjectBallsToRail;
                previousBall = contact.objectBallId;
            }
        }
    };

    ShotFacts fourStraight = canonical();
    setContacts(fourStraight, {
        breakContact(1, 10, 0), breakContact(2, 10, 1),
        breakContact(3, 10, 2), breakContact(4, 10, 3)});
    const auto legalRails = rules.adjudicate(state, fourStraight);
    expect(legalRails.accepted &&
               !has(legalRails, FoulReason::IllegalBreak),
           "four distinct straight-cushion velocity impulses satisfy the selected break threshold");

    ShotFacts mixed = canonical();
    setContacts(mixed, {
        breakContact(1, 10, 0), breakContact(2, 10, 1),
        breakContact(3, 10, 2),
        breakContact(4, 10, 3, BreakCushionBoundaryKind::PocketJaw),
        breakContact(5, 10, 4,
            BreakCushionBoundaryKind::StraightCushion, false, true)});
    const auto illegalMixed = rules.adjudicate(state, mixed);
    expect(illegalMixed.accepted &&
               has(illegalMixed, FoulReason::IllegalBreak),
           "jaw and correction-only contacts remain canonical facts but never satisfy the selected threshold");

    ShotFacts repeat = canonical();
    setContacts(repeat, {
        breakContact(1, 10, 0), breakContact(1, 11, 1),
        breakContact(2, 10, 2), breakContact(3, 10, 3)});
    const auto repeatedObject = rules.adjudicate(state, repeat);
    expect(repeatedObject.accepted &&
               has(repeatedObject, FoulReason::IllegalBreak),
           "repeated qualifying contacts by one object ball count only once");

    ShotFacts objectPocket = canonical();
    objectPocket.pocketedBallsInOrder = {9};
    const auto legalPocket = rules.adjudicate(state, objectPocket);
    expect(legalPocket.accepted &&
               !has(legalPocket, FoulReason::IllegalBreak),
           "one object-ball pocket is the independent legal-break alternative");

    ShotFacts cueScratch = canonical();
    attestCueScratch(cueScratch);
    const auto scratch = rules.adjudicate(state, cueScratch);
    expect(scratch.accepted && has(scratch, FoulReason::CueBallScratch) &&
               has(scratch, FoulReason::IllegalBreak),
           "cue scratch remains an independent foul and cannot satisfy the object-pocket alternative");

    ShotFacts incomplete = fourStraight;
    incomplete.breakEvidenceComplete = false;
    expect(validateShotFacts(incomplete) ==
               ShotFactsValidationError::InvalidBreakEvidence &&
               !rules.adjudicate(state, incomplete).accepted,
           "incomplete authoritative provenance fails closed before threshold classification");
}

void runSimultaneousFirstContactTests()
{
    ChineseEightBallRules rules;
    {
        MatchState state = assigned(BallGroup::Solids);
        ShotFacts facts = legalShot();
        facts.firstContactBalls = {1, 9};
        const auto result = rules.adjudicate(state, facts);
        expect(!has(result, FoulReason::WrongFirstContact),
               "assigned-group legal plus illegal simultaneous contact is presumed legal");
    }
    {
        MatchState state = assigned(BallGroup::Solids);
        ShotFacts facts = legalShot();
        facts.firstContactBall = 9;
        facts.firstContactBalls = {9, 10};
        const auto result = rules.adjudicate(state, facts);
        expect(has(result, FoulReason::WrongFirstContact),
               "assigned-group all-illegal simultaneous contacts remain a foul");
    }
    {
        MatchState state;
        state.phase = MatchPhase::OpenTable;
        ShotFacts facts = legalShot();
        facts.firstContactBall = 8;
        facts.firstContactBalls = {8, 9};
        facts.pocketedBallsInOrder = {9};
        const auto result = rules.adjudicate(state, facts);
        expect(!has(result, FoulReason::WrongFirstContact) &&
                   result.nextState.groups[0] == BallGroup::Stripes,
               "open-table 8 plus non-8 contact uses the legal-contact presumption");
    }
    {
        MatchState state;
        state.phase = MatchPhase::OpenTable;
        ShotFacts facts = legalShot();
        facts.firstContactBall = 8;
        facts.firstContactBalls = {8};
        const auto result = rules.adjudicate(state, facts);
        expect(has(result, FoulReason::WrongFirstContact),
               "open-table all-illegal 8-only contact remains a foul");
    }
    {
        MatchState state;
        state.phase = MatchPhase::OpenTable;
        ShotFacts facts = legalShot();
        facts.firstContactBalls = {1, 9};
        facts.pocketedBallsInOrder = {1, 9};
        const auto result = rules.adjudicate(state, facts);
        expect(result.groupChoiceRequired && !result.foul &&
                   result.continueTurn &&
                   result.nextState.phase == MatchPhase::OpenTable &&
                   result.nextState.groups[0] == BallGroup::Open &&
                   result.nextState.groups[1] == BallGroup::Open,
               "two-group simultaneous contact and pocket stages a group choice");
        expect(result.decisionOptions == std::vector<std::string>({
                   "rules.group_choice.choose_solids",
                   "rules.group_choice.choose_stripes"}),
               "group-choice staging uses the frozen fully-qualified action ids");
    }
    {
        MatchState state;
        state.phase = MatchPhase::OpenTable;
        ShotFacts facts = legalShot();
        facts.firstContactBalls = {1, 9};
        facts.pocketedBallsInOrder = {9};
        const auto result = rules.adjudicate(state, facts);
        expect(!result.groupChoiceRequired &&
                   result.nextState.groups[0] == BallGroup::Stripes &&
                   result.nextState.phase == MatchPhase::GroupsAssigned,
               "one pocketed group among simultaneous contacts assigns deterministically");
    }
    {
        MatchState state;
        state.phase = MatchPhase::OpenTable;
        ShotFacts facts = legalShot();
        facts.pocketedBallsInOrder = {1, 9};
        const auto result = rules.adjudicate(state, facts);
        expect(!result.groupChoiceRequired &&
                   result.nextState.groups[0] == BallGroup::Solids,
               "legacy scalar-only facts retain deterministic assignment");
    }
}

void runConfigurationTests()
{
    ChineseEightBallRuleset called;
    called.requireCalledEightPocket = true;
    ChineseEightBallRules rules(called);
    MatchState state = assigned(BallGroup::Solids);
    ShotFacts facts = legalShot();
    facts.solidsRemainingBefore = 0;
    facts.firstContactBall = 8;
    facts.pocketedBallsInOrder = {8};
    facts.calledPocket = 2;
    facts.eightBallPocket = 3;
    expect(rules.adjudicate(state, facts).winner == 1,
           "explicit called-pocket variant should reject mismatch");

    facts.eightBallPocket = -1;
    expect(!rules.adjudicate(state, facts).accepted,
           "called-pocket variant rejects a pocketed 8 with no pocket id");

    ChineseEightBallRules defaultRules;
    expect(defaultRules.adjudicate(state, facts).accepted,
           "default no-call rules do not require an 8-ball pocket id");

    called.schemaVersion = 2;
    bool rejected = false;
    try { ChineseEightBallRules invalid(called); }
    catch (const std::invalid_argument&) { rejected = true; }
    expect(rejected, "unknown ruleset schema must fail explicitly");
}

ShotFacts headStringFacts(double objectZ, double crossingTime,
                          double contactTime, bool complete)
{
    ShotFacts facts = legalShot();
    facts.firstContactBalls = {1};
    facts.behindHeadStringFirstContactRestrictionActive = true;
    facts.headStringWorldZCm = -63.5;
    facts.headStringCrossingEvidenceOrigin =
        HeadStringCrossingEvidenceOrigin::AuthoritativeShotFactCollector;
    facts.headStringCrossingEvidenceComplete = complete;
    FirstCueObjectContactEvidence contact;
    contact.objectBallId = 1;
    contact.physicsTick = 8;
    contact.wholeTickTimeOfImpactSeconds = contactTime;
    contact.solverEventId = 2;
    contact.physicsContactOrdinal = 0;
    contact.objectBallCenterWorldXCm = 0.0;
    contact.objectBallCenterWorldZCm = objectZ;
    facts.firstCueObjectContactEvidence.push_back(contact);
    if (crossingTime >= 0.0) {
        CueCenterPlaneCrossingEvidence crossing;
        crossing.observationId = "wpa_ii_08_head_string_cue_center";
        crossing.physicsTick = 8;
        crossing.wholeTickTimeOfImpactSeconds = crossingTime;
        crossing.segmentOrdinal = 1;
        crossing.signedDistanceBeforeCm = -4.0;
        crossing.signedDistanceAfterCm = 2.0;
        crossing.axisVelocityAtCrossingCmS = 80.0;
        crossing.canonicalEventOrdinal = 0;
        facts.cueCenterPlaneCrossingEvents.push_back(crossing);
    }
    facts.cueBallCrossedHeadStringBeforeFirstContact =
        complete && crossingTime >= 0.0 && crossingTime < contactTime;
    return facts;
}

void runHeadStringContinuousOrderPolicyTests()
{
    ChineseEightBallRules rules;
    MatchState state;
    state.phase = MatchPhase::OpenTable;

    const auto crossed = rules.adjudicate(
        state, headStringFacts(-80.0, 0.02, 0.04, true));
    expect(crossed.accepted &&
        !has(crossed, FoulReason::WrongFirstContact),
        "strict authoritative crossing before a behind-line contact is legal");

    const auto contactFirst = rules.adjudicate(
        state, headStringFacts(-80.0, 0.04, 0.02, true));
    expect(contactFirst.accepted &&
        has(contactFirst, FoulReason::WrongFirstContact),
        "contact before crossing fails the behind-head-string restriction");

    const auto tied = rules.adjudicate(
        state, headStringFacts(-80.0, 0.03, 0.03, true));
    expect(tied.accepted && has(tied, FoulReason::WrongFirstContact),
        "equal continuous times are unordered and fail closed");

    const auto onLine = rules.adjudicate(
        state, headStringFacts(-63.5, -1.0, 0.02, true));
    const auto footSide = rules.adjudicate(
        state, headStringFacts(-20.0, -1.0, 0.02, true));
    expect(onLine.accepted && footSide.accepted &&
        !has(onLine, FoulReason::WrongFirstContact) &&
        !has(footSide, FoulReason::WrongFirstContact),
        "on-line and foot-side objects remain legal direct contacts without crossing");

    const auto incomplete = rules.adjudicate(
        state, headStringFacts(-80.0, -1.0, 0.02, false));
    const auto partial = rules.adjudicate(
        state, headStringFacts(-80.0, 0.01, 0.02, false));
    expect(incomplete.accepted && partial.accepted &&
        has(incomplete, FoulReason::WrongFirstContact) &&
        has(partial, FoulReason::WrongFirstContact),
        "missing or partial incomplete provenance fails closed for a behind-line object");

    ShotFacts synthetic = headStringFacts(-80.0, 0.01, 0.02, true);
    synthetic.headStringCrossingEvidenceOrigin =
        HeadStringCrossingEvidenceOrigin::DirectOrSynthetic;
    expect(validateShotFacts(synthetic) ==
            ShotFactsValidationError::InvalidHeadStringCrossingEvidence &&
        !rules.adjudicate(state, synthetic).accepted,
        "direct or synthetic facts cannot claim complete crossing telemetry");
}

void runSharedLegalFirstContactPredicateTests()
{
    MatchState open;
    open.currentPlayer = 0;
    open.groups = {BallGroup::Open, BallGroup::Open};
    expect(isLegalFirstContactBall(open, 1, false) &&
        isLegalFirstContactBall(open, 15, false) &&
        !isLegalFirstContactBall(open, 8, false) &&
        !isLegalFirstContactBall(open, 0, false),
        "shared open-table candidate predicate matches adjudication");
    MatchState solids = assigned(BallGroup::Solids);
    solids.currentPlayer = 0;
    expect(isLegalFirstContactBall(solids, 2, false) &&
        !isLegalFirstContactBall(solids, 10, false) &&
        !isLegalFirstContactBall(solids, 8, false) &&
        isLegalFirstContactBall(solids, 8, true) &&
        !isLegalFirstContactBall(solids, 2, true),
        "shared assigned/group-cleared candidate predicate is exact");
}

void runPushAndFrozenAdjudicationTests()
{
    ChineseEightBallRules rules;
    MatchState state = assigned(BallGroup::Solids);
    ShotFacts push = legalShot();
    push.hasCanonicalPushClassification = true;
    auto& pc = push.canonicalPushClassification;
    pc.shotIdentity = "shot.push.1";
    pc.result = CanonicalPushResult::PushFoul;
    pc.durationSeconds = 0.01;
    pc.distanceCm = 0.5;
    pc.signedPushDecisionId = "push.1";
    pc.cueTipObservationDigest = "tip.sha";
    pc.pushProposalPayloadSha256 = "proposal.sha";
    pc.pushFinalPayloadSha256 = "final.sha";
    expect(validateCanonicalPushClassification(pc).complete,
        "push classification fixture validates");
    auto adjudicated = rules.adjudicate(state, push);
    expect(adjudicated.accepted && adjudicated.foul &&
        adjudicated.ballInHand && has(adjudicated, FoulReason::Push),
        "PUSH_FOUL_GRANTS_OPPONENT_BALL_IN_HAND");

    ShotFacts frozen = legalShot();
    frozen.hasCanonicalFrozenCushionClassification = true;
    auto& fc = frozen.canonicalFrozenCushionClassification;
    fc.shotIdentity = "shot.frozen.1";
    fc.result = CanonicalFrozenCushionResult::FrozenCushionFoul;
    fc.frozenDeclarationDecisionId = "frozen.1";
    fc.firstLegalObjectBallId = 1;
    fc.firstLegalObjectContactTimeSeconds = 0.01;
    fc.frozenObservationDigest = "frozen.sha";
    fc.cueObjectObservationDigest = "order.sha";
    expect(validateCanonicalFrozenCushionClassification(fc).complete,
        "frozen classification fixture validates");
    adjudicated = rules.adjudicate(state, frozen);
    expect(adjudicated.accepted && adjudicated.foul &&
        adjudicated.ballInHand &&
        has(adjudicated, FoulReason::FrozenCushionObligation),
        "frozen cushion obligation uses general foul ball in hand");

    pc.digest = "tampered";
    expect(validateShotFacts(push) ==
            ShotFactsValidationError::InvalidPushClassification,
        "failed push classification digest is fail closed");
}

}  // namespace

int main()
{
    try {
        runScenarioTable();
        runConfigurationTests();
        runFactValidationTests();
        runBreakEvidenceValidationTests();
        runSelectedBreakThresholdPolicyTests();
        runSimultaneousFirstContactTests();
        runHeadStringContinuousOrderPolicyTests();
        runSharedLegalFirstContactPredicateTests();
        runDoubleHitAdjudicationTests();
        runPushAndFrozenAdjudicationTests();
        return EXIT_SUCCESS;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return EXIT_FAILURE;
    }
}
