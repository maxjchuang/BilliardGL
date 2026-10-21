#include "rules/shot_fact_collector.h"

#include <algorithm>
#include <cstdlib>
#include <iostream>
#include <limits>
#include <stdexcept>
#include <vector>

namespace {

void expect(bool condition, const char* message)
{
    if (!condition) throw std::runtime_error(message);
}

billiardgl::PhysicsContactRecord ballContact(int first, int second)
{
    billiardgl::PhysicsContactRecord value;
    value.kind = billiardgl::PhysicsContactKind::BallBall;
    value.firstBall = first;
    value.secondBall = second;
    return value;
}

billiardgl::PhysicsContactRecord railContact(int ball, int solverEventId = -1,
                                             double normalImpulseNs = 0.0,
                                             bool velocityImpulseApplied = false,
                                             bool positionCorrected = false)
{
    billiardgl::PhysicsContactRecord value;
    value.kind = billiardgl::PhysicsContactKind::Rail;
    value.firstBall = ball;
    value.solverEventId = solverEventId;
    value.solverIslandId = solverEventId >= 0 ? solverEventId + 10 : -1;
    value.normalImpulseNs = normalImpulseNs;
    value.velocityImpulseApplied = velocityImpulseApplied;
    value.positionCorrected = positionCorrected;
    value.solverResidualCmS = solverEventId >= 0 ? 0.001 : 0.0;
    value.solverProjectionCm = positionCorrected ? 0.002 : 0.0;
    return value;
}

billiardgl::PhysicsContactRecord jawEvent(
    int ball, billiardgl::PocketBoundaryEventKind kind,
    double timeOfImpactSeconds)
{
    billiardgl::PhysicsContactRecord value;
    value.kind = billiardgl::PhysicsContactKind::Pocket;
    value.firstBall = ball;
    value.pocketBoundaryEvent = kind;
    value.timeOfImpactSeconds = timeOfImpactSeconds;
    return value;
}

billiardgl::PhysicsContactRecord cueCaptureEvent(
    int pocketId, unsigned long long captureSequence,
    double timeOfImpactSeconds = 0.01)
{
    billiardgl::PhysicsContactRecord value;
    value.kind = billiardgl::PhysicsContactKind::Pocket;
    value.firstBall = 0;
    value.pocketId = pocketId;
    value.pocketBoundaryEvent =
        billiardgl::PocketBoundaryEventKind::Capture;
    value.pocketPhaseBefore =
        billiardgl::PocketInteractionPhase::ThroatCrossed;
    value.pocketPhaseAfter =
        billiardgl::PocketInteractionPhase::Captured;
    value.pocketCaptureSequence = captureSequence;
    value.timeOfImpactSeconds = timeOfImpactSeconds;
    return value;
}

bool sameFacts(const billiardgl::rules::ShotFacts& left,
               const billiardgl::rules::ShotFacts& right)
{
    return left.shooter == right.shooter &&
        left.breakShot == right.breakShot &&
        left.firstContactBall == right.firstContactBall &&
        left.firstContactBalls == right.firstContactBalls &&
        left.breakObjectCushionContacts ==
            right.breakObjectCushionContacts &&
        left.breakEvidenceOrigin == right.breakEvidenceOrigin &&
        left.breakEvidenceComplete == right.breakEvidenceComplete &&
        left.distinctObjectBallsToRail == right.distinctObjectBallsToRail &&
        left.anyRailAfterFirstContact == right.anyRailAfterFirstContact &&
        left.cueBallPocketed == right.cueBallPocketed &&
        left.cueBallScratchEvidenceOrigin ==
            right.cueBallScratchEvidenceOrigin &&
        left.cueBallScratchEvidenceComplete ==
            right.cueBallScratchEvidenceComplete &&
        left.cueBallPocketCaptures == right.cueBallPocketCaptures &&
        left.cueBallOffTable == right.cueBallOffTable &&
        left.objectBallsOffTable == right.objectBallsOffTable &&
        left.pocketedBallsInOrder == right.pocketedBallsInOrder &&
        left.solidsRemainingBefore == right.solidsRemainingBefore &&
        left.stripesRemainingBefore == right.stripesRemainingBefore &&
        left.calledPocket == right.calledPocket &&
        left.eightBallPocket == right.eightBallPocket;
}

billiardgl::rules::CanonicalDoubleHitClassification doubleHitClassification(
    int intervals,
    billiardgl::rules::CanonicalDoubleHitResult result)
{
    billiardgl::rules::CanonicalDoubleHitClassification value;
    value.shotIdentity = "shot.collector.d5c";
    value.result = result;
    value.canonicalTipIntervalCount = intervals;
    value.cueObjectTipOrder =
        billiardgl::CanonicalCueObjectTipOrder::ContactBeforeTipSeparation;
    value.signedCloseCaseBound = result ==
        billiardgl::rules::CanonicalDoubleHitResult::
            SignedRefereeReviewRequired;
    value.cueTipObservationDigest = "tip.sha";
    value.cueObjectObservationDigest = "order.sha";
    const auto validation =
        billiardgl::rules::validateCanonicalDoubleHitClassification(value);
    expect(validation.valid && validation.complete,
        "double-hit fixture validation");
    return value;
}

void runDoubleHitBindingTests()
{
    billiardgl::GameState initial;
    billiardgl::initializeBalls(initial);
    billiardgl::rules::ShotFactCollector collector;
    collector.begin(initial, 0, false);
    const auto foul = doubleHitClassification(2,
        billiardgl::rules::CanonicalDoubleHitResult::DoubleHitFoul);
    expect(collector.bindCanonicalDoubleHitClassification(foul) &&
        collector.bindCanonicalDoubleHitClassification(foul),
        "exact classifier retry is idempotent");
    auto conflict = foul;
    conflict.canonicalTipIntervalCount = 3;
    billiardgl::rules::validateCanonicalDoubleHitClassification(conflict);
    expect(!collector.bindCanonicalDoubleHitClassification(conflict),
        "conflicting classifier retry has zero delta");
    auto forged = foul;
    forged.digest = "forged";
    expect(!collector.bindCanonicalDoubleHitClassification(forged),
        "tampered classifier binding fails closed");
    const auto facts = collector.finish();
    expect(facts.hasCanonicalDoubleHitClassification &&
        facts.canonicalDoubleHitClassification.digest == foul.digest,
        "collector carries only complete immutable classifier result");
    expect(!collector.bindCanonicalDoubleHitClassification(foul),
        "inactive collector rejects late classifier binding");
}

void runD5DBindingTests()
{
    billiardgl::GameState initial;
    billiardgl::initializeBalls(initial);
    billiardgl::rules::ShotFactCollector collector;
    collector.begin(initial, 0, false);
    billiardgl::rules::CanonicalPushClassification push;
    push.shotIdentity = "shot.push.collector";
    push.result = billiardgl::rules::CanonicalPushResult::PushFoul;
    push.durationSeconds = 0.01;
    push.distanceCm = 0.4;
    push.signedPushDecisionId = "push.1";
    push.cueTipObservationDigest = "tip.sha";
    push.pushProposalPayloadSha256 = "proposal.sha";
    push.pushFinalPayloadSha256 = "final.sha";
    expect(billiardgl::rules::validateCanonicalPushClassification(push).complete,
        "push binding fixture validates");
    expect(collector.bindCanonicalPushClassification(push) &&
        collector.bindCanonicalPushClassification(push),
        "exact push retry is idempotent");
    auto conflict = push;
    conflict.distanceCm = 0.5;
    billiardgl::rules::validateCanonicalPushClassification(conflict);
    expect(!collector.bindCanonicalPushClassification(conflict),
        "conflicting push retry has zero delta");
    const auto facts = collector.finish();
    expect(facts.hasCanonicalPushClassification &&
        facts.canonicalPushClassification.digest == push.digest,
        "collector carries immutable push classification");
}

void run()
{
    billiardgl::GameState initial;
    billiardgl::initializeBalls(initial);
    billiardgl::rules::ShotFactCollector collector;
    collector.begin(initial, 0, true);

    billiardgl::PhysicsStepTelemetry firstStep;
    firstStep.contacts = {railContact(4), ballContact(0, 1),
                          railContact(0), railContact(2), railContact(2)};
    billiardgl::GameState afterFirst = initial;
    collector.consume(initial, afterFirst, firstStep, 1);

    billiardgl::GameState afterPocket = afterFirst;
    afterPocket.balls[9].pocketed = true;
    afterPocket.balls[9].pocketInteraction.captureSequence = 7;
    afterPocket.balls[9].pocketInteraction.pocketId = 2;
    afterPocket.balls[3].pocketed = true;
    afterPocket.balls[3].pocketInteraction.captureSequence = 6;
    afterPocket.balls[3].pocketInteraction.pocketId = 1;
    billiardgl::PhysicsStepTelemetry secondStep;
    secondStep.contacts = {railContact(3), railContact(4)};
    collector.consume(afterFirst, afterPocket, secondStep, 2);

    billiardgl::GameState afterScratch = afterPocket;
    afterScratch.events.cueBallPocketed = true;
    billiardgl::PhysicsStepTelemetry scratchStep;
    scratchStep.contacts = {cueCaptureEvent(4, 8)};
    collector.consume(afterPocket, afterScratch, scratchStep, 3);

    const auto facts = collector.finish();
    expect(facts.breakShot && facts.shooter == 0,
           "begin should preserve shot context");
    expect(facts.firstContactBall == 1,
           "first cue/object collision should be captured once");
    expect(facts.firstContactBalls.size() == 1 &&
               facts.firstContactBalls[0] == 1,
           "legacy first contact should project from the canonical set");
    expect(facts.anyRailAfterFirstContact,
           "rail after first contact should satisfy continuation fact");
    expect(facts.distinctObjectBallsToRail == 3,
           "break rail count should de-duplicate object balls");
    expect(facts.pocketedBallsInOrder.size() == 2 &&
               facts.pocketedBallsInOrder[0] == 3 &&
               facts.pocketedBallsInOrder[1] == 9,
           "pockets should be ordered by authoritative capture sequence");
    expect(facts.cueBallPocketed, "cue-ball pocket should be an explicit fact");
    expect(facts.cueBallScratchEvidenceOrigin ==
               billiardgl::rules::CueBallScratchEvidenceOrigin::
                   AuthoritativeShotFactCollector &&
               facts.cueBallScratchEvidenceComplete &&
               facts.cueBallPocketCaptures.size() == 1 &&
               facts.cueBallPocketCaptures[0].pocketId == 4 &&
               facts.cueBallPocketCaptures[0].captureSequence == 8 &&
               facts.cueBallPocketCaptures[0].physicsTick == 3 &&
               facts.cueBallPocketCaptures[0].physicsContactOrdinal == 0 &&
               facts.cueBallPocketCaptures[0].canonicalEventOrdinal == 0,
           "cue scratch must carry complete canonical succeeded-tick evidence");
    expect(facts.solidsRemainingBefore == 7 &&
               facts.stripesRemainingBefore == 7,
           "group counts must be captured before the shot");
    expect(facts.breakEvidenceOrigin ==
               billiardgl::rules::BreakEvidenceOrigin::
                   AuthoritativeShotFactCollector &&
               facts.breakEvidenceComplete,
           "completed break collection must carry authoritative origin and completeness");

    bool rejected = false;
    try {
        billiardgl::rules::ShotFactCollector inactive;
        inactive.consume(initial, initial, {}, 1);
    } catch (const std::logic_error&) { rejected = true; }
    expect(rejected, "collector lifecycle misuse must fail explicitly");
}

void runCueBallScratchEvidenceTests()
{
    using billiardgl::rules::CueBallScratchEvidenceOrigin;
    using billiardgl::rules::ShotFactsValidationError;
    billiardgl::GameState initial;
    billiardgl::initializeBalls(initial);

    {
        billiardgl::rules::ShotFactCollector collector;
        collector.begin(initial, 0, false);
        billiardgl::GameState after = initial;
        after.events.cueBallPocketed = true;
        const auto capture = cueCaptureEvent(2, 17, 0.025);
        billiardgl::PhysicsStepTelemetry telemetry;
        telemetry.contacts = {capture, capture};
        collector.consume(initial, after, telemetry, 41);
        const auto facts = collector.finish();
        expect(facts.cueBallPocketed &&
                   facts.cueBallScratchEvidenceOrigin ==
                       CueBallScratchEvidenceOrigin::
                           AuthoritativeShotFactCollector &&
                   facts.cueBallScratchEvidenceComplete &&
                   facts.cueBallPocketCaptures.size() == 1 &&
                   facts.cueBallPocketCaptures[0].physicsContactOrdinal == 0 &&
                   billiardgl::rules::validateShotFacts(facts) ==
                       ShotFactsValidationError::None,
               "exact duplicate capture records dedupe to one valid canonical event");
    }

    {
        billiardgl::rules::ShotFactCollector collector;
        collector.begin(initial, 0, false);
        billiardgl::GameState after = initial;
        after.events.cueBallPocketed = true;
        billiardgl::PhysicsStepTelemetry failed;
        failed.stepStatus = billiardgl::PhysicsStepStatus::Failed;
        failed.failureCode = billiardgl::PhysicsFailureCode::ResidualLimit;
        failed.contacts = {cueCaptureEvent(1, 19)};
        collector.consume(initial, after, failed, 51);
        billiardgl::PhysicsStepTelemetry succeeded;
        collector.consume(initial, initial, succeeded, 52);
        const auto facts = collector.finish();
        expect(!facts.cueBallPocketed && facts.cueBallPocketCaptures.empty() &&
                   facts.cueBallScratchEvidenceComplete &&
                   billiardgl::rules::validateShotFacts(facts) ==
                       ShotFactsValidationError::None,
               "failed-step capture and event state roll back without partial scratch evidence");
    }

    {
        billiardgl::rules::ShotFactCollector collector;
        collector.begin(initial, 0, false);
        billiardgl::GameState after = initial;
        after.events.cueBallPocketed = true;
        auto partial = cueCaptureEvent(3, 0);
        billiardgl::PhysicsStepTelemetry telemetry;
        telemetry.contacts = {partial};
        collector.consume(initial, after, telemetry, 61);
        const auto facts = collector.finish();
        expect(!facts.cueBallPocketed && facts.cueBallPocketCaptures.empty() &&
                   !facts.cueBallScratchEvidenceComplete &&
                   billiardgl::rules::validateShotFacts(facts) ==
                       ShotFactsValidationError::InvalidCueBallScratchEvidence,
               "partial cue capture event fails completeness and cannot become a foul fact");
    }

    {
        billiardgl::rules::ShotFactCollector collector;
        collector.begin(initial, 0, false);
        billiardgl::GameState after = initial;
        after.events.cueBallPocketed = true;
        billiardgl::PhysicsStepTelemetry telemetry;
        telemetry.contacts = {
            cueCaptureEvent(1, 21, 0.02),
            cueCaptureEvent(4, 21, 0.02)};
        collector.consume(initial, after, telemetry, 66);
        const auto facts = collector.finish();
        expect(!facts.cueBallPocketed && facts.cueBallPocketCaptures.empty() &&
                   !facts.cueBallScratchEvidenceComplete &&
                   billiardgl::rules::validateShotFacts(facts) ==
                       ShotFactsValidationError::InvalidCueBallScratchEvidence,
               "conflicting duplicate capture identity fails closed instead of selecting a pocket");
    }

    {
        billiardgl::rules::ShotFactCollector collector;
        collector.begin(initial, 0, false);
        billiardgl::GameState firstAfter = initial;
        firstAfter.events.cueBallPocketed = true;
        billiardgl::PhysicsStepTelemetry first;
        first.contacts = {cueCaptureEvent(0, 23)};
        collector.consume(initial, firstAfter, first, 71);
        expect(collector.finish().cueBallPocketed,
               "first collector lifecycle records its cue scratch");
        collector.begin(initial, 1, false);
        collector.consume(initial, initial, {}, 1);
        const auto reset = collector.finish();
        expect(!reset.cueBallPocketed && reset.cueBallPocketCaptures.empty() &&
                   reset.cueBallScratchEvidenceComplete,
               "begin resets cue scratch evidence, ordinals, and dedupe identity");
    }
}

void runFirstContactTickSetTests()
{
    billiardgl::GameState state;
    billiardgl::initializeBalls(state);
    auto collect = [&](const std::vector<billiardgl::PhysicsContactRecord>& first,
                       const std::vector<billiardgl::PhysicsContactRecord>& later) {
        billiardgl::rules::ShotFactCollector collector;
        collector.begin(state, 0, false);
        billiardgl::PhysicsStepTelemetry firstTick;
        firstTick.contacts = first;
        collector.consume(state, state, firstTick, 1);
        billiardgl::PhysicsStepTelemetry laterTick;
        laterTick.contacts = later;
        collector.consume(state, state, laterTick, 2);
        return collector.finish();
    };

    const auto ordered = collect(
        {ballContact(0, 9), ballContact(1, 0), ballContact(0, 9),
         ballContact(2, 3)},
        {ballContact(0, 3), ballContact(0, 9)});
    const auto permuted = collect(
        {ballContact(0, 1), ballContact(9, 0), ballContact(1, 0)},
        {ballContact(0, 3)});
    expect(ordered.firstContactBalls == std::vector<int>({1, 9}) &&
               permuted.firstContactBalls == ordered.firstContactBalls &&
               ordered.firstContactBall == 1 &&
               permuted.firstContactBall == 1,
           "same-tick first contacts must be unique, sorted, and permutation invariant");
    expect(std::find(ordered.firstContactBalls.begin(),
                     ordered.firstContactBalls.end(), 3) ==
               ordered.firstContactBalls.end(),
           "object contacts from later ticks must not enter the first-contact set");

    const auto railBefore = collect(
        {railContact(5), ballContact(0, 9), ballContact(1, 0)}, {});
    const auto railAfter = collect(
        {ballContact(1, 0), ballContact(0, 9), railContact(5)}, {});
    expect(sameFacts(railBefore, railAfter) &&
               railBefore.anyRailAfterFirstContact &&
               railBefore.distinctObjectBallsToRail == 1,
           "first-contact-tick rail semantics must be contact-order invariant");

    billiardgl::rules::ShotFactCollector delayed;
    delayed.begin(state, 0, false);
    delayed.consume(state, state, {}, 1);
    billiardgl::PhysicsStepTelemetry firstObjectTick;
    firstObjectTick.contacts = {ballContact(0, 4), ballContact(2, 0),
                                ballContact(0, 4)};
    delayed.consume(state, state, firstObjectTick, 2);
    const auto delayedFacts = delayed.finish();
    expect(delayedFacts.firstContactBalls == std::vector<int>({2, 4}) &&
               delayedFacts.firstContactBall == 2,
           "collector must wait for and capture the first object-contact tick");
}

void runBreakCushionEvidenceTests()
{
    using billiardgl::PocketBoundaryEventKind;
    using billiardgl::rules::BreakCushionBoundaryKind;
    using billiardgl::rules::BreakCushionContactProvenance;
    using billiardgl::rules::BreakEvidenceOrigin;

    billiardgl::GameState state;
    billiardgl::initializeBalls(state);
    billiardgl::rules::ShotFactCollector collector;
    collector.begin(state, 0, true);

    auto jaw = railContact(3, 7, 0.08, true, false);
    jaw.pocketBoundaryEvent = PocketBoundaryEventKind::LeftJaw;
    billiardgl::PhysicsStepTelemetry first;
    first.contacts = {
        railContact(4, 9, 0.12, true, false),
        railContact(2, 5, 0.0, false, true),
        railContact(4, 9, 0.12, true, false),
        railContact(0, 3, 0.2, true, false),
        jaw,
    };
    collector.consume(state, state, first, 11);

    auto discreteJaw = railContact(5, -1, 0.04, true, false);
    discreteJaw.timeOfImpactSeconds = 0.02;
    billiardgl::PhysicsStepTelemetry second;
    second.contacts = {
        jawEvent(5, PocketBoundaryEventKind::RightJaw, 0.02),
        discreteJaw,
        railContact(4, 12, 0.03, true, false),
    };
    collector.consume(state, state, second, 12);

    billiardgl::PhysicsStepTelemetry failed;
    failed.stepStatus = billiardgl::PhysicsStepStatus::Failed;
    failed.failureCode = billiardgl::PhysicsFailureCode::ResidualLimit;
    failed.contacts = {railContact(6, 14, 0.5, true, false)};
    collector.consume(state, state, failed, 13);

    bool duplicateTickRejected = false;
    try {
        collector.consume(state, state, second, 12);
    } catch (const std::logic_error&) {
        duplicateTickRejected = true;
    }
    const auto facts = collector.finish();
    expect(!duplicateTickRejected,
           "an exact duplicate succeeded tick is an idempotent no-op");
    expect(facts.breakEvidenceOrigin ==
               BreakEvidenceOrigin::AuthoritativeShotFactCollector &&
               facts.breakEvidenceComplete,
           "succeeded-step evidence must be complete and authoritative");
    expect(facts.distinctObjectBallsToRail == 4,
           "legacy scalar keeps distinct object identity semantics independent of policy");
    expect(facts.breakObjectCushionContacts.size() == 5,
           "canonical evidence deduplicates exact records but preserves later physical contacts");
    expect(facts.breakObjectCushionContacts[0].objectBallId == 2 &&
               facts.breakObjectCushionContacts[0].positionCorrected &&
               !facts.breakObjectCushionContacts[0].velocityImpulseApplied &&
               facts.breakObjectCushionContacts[0].normalImpulseNs == 0.0,
           "correction-only records are preserved as evidence without classifying them");
    expect(facts.breakObjectCushionContacts[1].objectBallId == 3 &&
               facts.breakObjectCushionContacts[1].boundaryKind ==
                   BreakCushionBoundaryKind::PocketJaw &&
               facts.breakObjectCushionContacts[1].provenance ==
                   BreakCushionContactProvenance::ContinuousSolver,
           "solver jaw evidence preserves boundary and solver provenance");
    expect(facts.breakObjectCushionContacts[2].objectBallId == 4 &&
               facts.breakObjectCushionContacts[2].physicsTick == 11 &&
               facts.breakObjectCushionContacts[2].solverEventId == 9 &&
               facts.breakObjectCushionContacts[3].objectBallId == 4 &&
               facts.breakObjectCushionContacts[3].physicsTick == 12 &&
               facts.breakObjectCushionContacts[3].solverEventId == 12,
           "repeated contacts by one object remain separate canonical evidence while counting once");
    expect(facts.breakObjectCushionContacts[4].objectBallId == 5 &&
               facts.breakObjectCushionContacts[4].boundaryKind ==
                   BreakCushionBoundaryKind::PocketJaw &&
               facts.breakObjectCushionContacts[4].provenance ==
                   BreakCushionContactProvenance::DiscretePhysics &&
               facts.breakObjectCushionContacts[4].solverEventId == -1,
           "discrete rail plus matched pocket event retains jaw and non-solver provenance");
    expect(std::none_of(facts.breakObjectCushionContacts.begin(),
                        facts.breakObjectCushionContacts.end(),
                        [](const auto& contact) {
                            return contact.objectBallId == 0 ||
                                contact.objectBallId == 6;
                        }),
           "cue-ball contacts and failed-step contacts never enter break object evidence");
}

void runEvidenceCompletenessRollbackTests()
{
    billiardgl::GameState state;
    billiardgl::initializeBalls(state);
    billiardgl::rules::ShotFactCollector collector;
    collector.begin(state, 0, true);
    auto invalid = railContact(4, 2, 0.1, true, false);
    invalid.normalImpulseNs = std::numeric_limits<double>::quiet_NaN();
    billiardgl::PhysicsStepTelemetry telemetry;
    telemetry.contacts = {invalid};
    collector.consume(state, state, telemetry, 1);
    const auto facts = collector.finish();
    expect(!facts.breakEvidenceComplete &&
               facts.breakObjectCushionContacts.empty() &&
               facts.distinctObjectBallsToRail == 1,
           "malformed contact attestation fails closed without partial evidence or policy reinterpretation");
}

void runCanonicalAndFailedStepRollbackTests()
{
    billiardgl::GameState state;
    billiardgl::initializeBalls(state);
    const auto a = railContact(7, 8, 0.2, true, false);
    const auto b = railContact(2, 4, 0.0, false, true);
    const auto c = railContact(7, 9, 0.1, true, false);

    auto collect = [&](const std::vector<billiardgl::PhysicsContactRecord>& rows,
                       bool injectFailedStep) {
        billiardgl::rules::ShotFactCollector collector;
        collector.begin(state, 0, true);
        billiardgl::PhysicsStepTelemetry first;
        first.contacts = rows;
        collector.consume(state, state, first, 20);
        if (injectFailedStep) {
            billiardgl::PhysicsStepTelemetry failed;
            failed.stepStatus = billiardgl::PhysicsStepStatus::Failed;
            failed.failureCode =
                billiardgl::PhysicsFailureCode::PenetrationLimit;
            failed.contacts = {railContact(10, 30, 9.0, true, true)};
            collector.consume(state, state, failed, 21);
        }
        billiardgl::PhysicsStepTelemetry second;
        second.contacts = {railContact(3, 12, 0.3, true, false)};
        collector.consume(state, state, second, 22);
        return collector.finish();
    };

    const auto ordered = collect({a, b, c}, false);
    const auto permuted = collect({c, a, b}, false);
    const auto withFailedStep = collect({a, b, c}, true);
    expect(sameFacts(ordered, permuted),
           "canonical break evidence is byte-equivalent under contact permutation");
    expect(sameFacts(ordered, withFailedStep),
           "failed physics steps roll back all break evidence and scalar projections");

    billiardgl::rules::ShotFactCollector failedFourth;
    failedFourth.begin(state, 0, true);
    billiardgl::PhysicsStepTelemetry threeSucceeded;
    threeSucceeded.contacts = {
        railContact(1, 40, 0.1, true, false),
        railContact(2, 41, 0.1, true, false),
        railContact(3, 42, 0.1, true, false)};
    failedFourth.consume(state, state, threeSucceeded, 30);
    billiardgl::PhysicsStepTelemetry rejectedFourth;
    rejectedFourth.stepStatus = billiardgl::PhysicsStepStatus::Failed;
    rejectedFourth.failureCode =
        billiardgl::PhysicsFailureCode::ResidualLimit;
    rejectedFourth.contacts = {
        railContact(4, 43, 1.0, true, false)};
    failedFourth.consume(state, state, rejectedFourth, 31);
    const auto failedFourthFacts = failedFourth.finish();
    expect(failedFourthFacts.breakEvidenceComplete &&
               failedFourthFacts.distinctObjectBallsToRail == 3 &&
               failedFourthFacts.breakObjectCushionContacts.size() == 3 &&
               std::none_of(
                   failedFourthFacts.breakObjectCushionContacts.begin(),
                   failedFourthFacts.breakObjectCushionContacts.end(),
                   [](const auto& contact) {
                       return contact.objectBallId == 4;
                   }),
           "a failed-step fourth straight impulse cannot cross the selected four-object threshold");

    billiardgl::rules::ShotFactCollector noSucceededStep;
    noSucceededStep.begin(state, 0, true);
    const auto empty = noSucceededStep.finish();
    expect(!empty.breakEvidenceComplete,
           "a break with no succeeded authoritative physics step cannot claim completeness");
}

void runHeadStringContinuousOrderTests()
{
    billiardgl::GameState state;
    billiardgl::initializeBalls(state);
    billiardgl::rules::BehindHeadStringFirstContactContext context;
    context.active = true;
    context.actorPlayerId = 0;
    context.placementDecisionId = "placement-08";
    context.placementAckRevision = 9;
    context.headStringWorldZCm = -63.5;
    context.observationId = "wpa_ii_08_head_string_cue_center";

    auto telemetry = [&](double crossingTime, double contactTime,
                         double objectZ, bool complete) {
        billiardgl::PhysicsStepTelemetry value;
        value.wholeTickDurationSeconds = 0.1;
        billiardgl::PhysicsPlaneObservationAttestation attestation;
        attestation.observationId = context.observationId;
        attestation.ballIndex = 0;
        attestation.wholeTickDurationSeconds = 0.1;
        attestation.coveredSegmentCount = 1;
        attestation.crossingRecordCount = crossingTime >= 0.0 ? 1 : 0;
        attestation.complete = complete;
        value.planeAttestations.push_back(attestation);
        if (crossingTime >= 0.0) {
            billiardgl::PhysicsPlaneCrossingRecord crossing;
            crossing.observationId = context.observationId;
            crossing.ballIndex = 0;
            crossing.axis = billiardgl::PhysicsPlaneAxis::Z;
            crossing.direction = billiardgl::PhysicsPlaneCrossingDirection::
                NegativeToPositive;
            crossing.planeCoordinateCm = context.headStringWorldZCm;
            crossing.wholeTickTimeOfImpactSeconds = crossingTime;
            crossing.segmentOrdinal = 2;
            crossing.signedDistanceBeforeCm = -2.0;
            crossing.signedDistanceAfterCm = 3.0;
            crossing.axisVelocityAtCrossingCmS = 50.0;
            crossing.complete = true;
            value.planeCrossings.push_back(crossing);
        }
        if (contactTime >= 0.0) {
            billiardgl::PhysicsContactRecord contact;
            contact.kind = billiardgl::PhysicsContactKind::BallBall;
            contact.firstBall = 0;
            contact.secondBall = 1;
            contact.solverEventId = 4;
            contact.solverIslandId = 0;
            contact.wholeTickTimeOfImpactSeconds = contactTime;
            contact.firstPositionAtContactCm = {0.0f, 92.0f, -60.0f};
            contact.secondPositionAtContactCm = {
                0.0f, 92.0f, static_cast<float>(objectZ)};
            value.contacts.push_back(contact);
        }
        return value;
    };

    billiardgl::rules::ShotFactCollector before;
    before.begin(state, 0, false, &context);
    const auto ordered = telemetry(0.02, 0.04, -80.0, true);
    before.consume(state, state, ordered, 10);
    before.consume(state, state, ordered, 10);
    bool conflictRejected = false;
    try {
        auto conflict = ordered;
        conflict.contacts[0].wholeTickTimeOfImpactSeconds = 0.05;
        before.consume(state, state, conflict, 10);
    } catch (const std::logic_error&) {
        conflictRejected = true;
    }
    const auto beforeFacts = before.finish();
    expect(conflictRejected &&
        beforeFacts.headStringCrossingEvidenceComplete &&
        beforeFacts.cueBallCrossedHeadStringBeforeFirstContact &&
        beforeFacts.cueCenterPlaneCrossingEvents.size() == 1 &&
        beforeFacts.firstCueObjectContactEvidence.size() == 1 &&
        beforeFacts.firstCueObjectContactEvidence[0].
            objectBallCenterWorldZCm == -80.0,
        "strict crossing-before-contact survives exact retry and conflicting retry rolls back");

    billiardgl::rules::ShotFactCollector tie;
    tie.begin(state, 0, false, &context);
    tie.consume(state, state, telemetry(0.04, 0.04, -80.0, true), 11);
    const auto tieFacts = tie.finish();
    expect(tieFacts.headStringCrossingEvidenceComplete &&
        !tieFacts.cueBallCrossedHeadStringBeforeFirstContact,
        "equal crossing/contact time is unordered and never promoted by event order");

    billiardgl::rules::ShotFactCollector earlierTick;
    earlierTick.begin(state, 0, false, &context);
    auto crossingThenCushion = telemetry(0.02, -1.0, -80.0, true);
    billiardgl::PhysicsContactRecord cushion;
    cushion.kind = billiardgl::PhysicsContactKind::Rail;
    cushion.firstBall = 0;
    cushion.wholeTickTimeOfImpactSeconds = 0.06;
    crossingThenCushion.contacts.push_back(cushion);
    earlierTick.consume(state, state, crossingThenCushion, 20);
    earlierTick.consume(state, state,
        telemetry(-1.0, 0.03, -80.0, true), 21);
    const auto earlierFacts = earlierTick.finish();
    expect(earlierFacts.headStringCrossingEvidenceComplete &&
        earlierFacts.cueBallCrossedHeadStringBeforeFirstContact &&
        earlierFacts.cueCenterPlaneCrossingEvents[0].physicsTick == 20 &&
        earlierFacts.firstCueObjectContactEvidence[0].physicsTick == 21,
        "earlier-tick crossing remains before contact after an intervening cushion event");

    billiardgl::rules::ShotFactCollector missing;
    missing.begin(state, 0, false, &context);
    auto incomplete = telemetry(-1.0, 0.04, -80.0, false);
    incomplete.planeAttestations.clear();
    missing.consume(state, state, incomplete, 12);
    const auto missingFacts = missing.finish();
    expect(!missingFacts.headStringCrossingEvidenceComplete &&
        !missingFacts.cueBallCrossedHeadStringBeforeFirstContact,
        "missing attestation fails closed without synthesizing a sampled crossing");

    billiardgl::rules::ShotFactCollector failedRollback;
    failedRollback.begin(state, 0, false, &context);
    auto failed = telemetry(0.01, 0.03, -80.0, true);
    failed.stepStatus = billiardgl::PhysicsStepStatus::Failed;
    failedRollback.consume(state, state, failed, 13);
    failedRollback.consume(state, state,
        telemetry(-1.0, 0.03, -63.5, true), 13);
    const auto rollbackFacts = failedRollback.finish();
    expect(rollbackFacts.headStringCrossingEvidenceComplete &&
        rollbackFacts.cueCenterPlaneCrossingEvents.empty() &&
        rollbackFacts.firstCueObjectContactEvidence[0].
            objectBallCenterWorldZCm == -63.5,
        "failed step contributes zero crossing/contact delta and preserves retry context");
}

}  // namespace

int main()
{
    try {
        billiardgl::GameState removed;
        billiardgl::initializeBalls(removed);
        removed.balls[1].outOfPlay = true;
        billiardgl::rules::ShotFactCollector collector;
        collector.begin(removed, 0, false);
        const auto removalFacts = collector.finish();
        expect(removalFacts.solidsRemainingBefore == 6 &&
            removalFacts.stripesRemainingBefore == 7 && removalFacts.pocketedBallsInOrder.empty() &&
            removalFacts.objectBallsOffTable.empty(),
            "previously adjudicated off-table ball is not remaining, a new pot, or a repeated off-table foul");
        run();
        runFirstContactTickSetTests();
        runCueBallScratchEvidenceTests();
        runBreakCushionEvidenceTests();
        runEvidenceCompletenessRollbackTests();
        runCanonicalAndFailedStepRollbackTests();
        runHeadStringContinuousOrderTests();
        runDoubleHitBindingTests();
        runD5DBindingTests();
        return EXIT_SUCCESS;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return EXIT_FAILURE;
    }
}
