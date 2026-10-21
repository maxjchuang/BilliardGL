#include "rules/double_hit_classifier.h"

#include <cstdlib>
#include <iostream>

namespace {
using namespace billiardgl;
using namespace billiardgl::rules;

void expect(bool value, const char* message)
{
    if (!value) {
        std::cerr << "FAIL: " << message << '\n';
        std::exit(1);
    }
}

CanonicalCueTipStrokeObservation tipObservation(int intervalCount)
{
    CanonicalCueTipStrokeObservation observation;
    observation.shotIdentity = "shot.d5c.1";
    observation.actorPlayerId = 0;
    observation.rackIdentity = "rack.d5c.1";
    observation.rulesRevision = 17;
    observation.rulesStateHash = "rules.sha.d5c.1";
    observation.startTick = 100;
    observation.endTick = 103;
    observation.observationStartedBeforeFirstContact = true;
    observation.observationEndedAfterAuthoritativeRelease = true;
    observation.noDroppedIntervals = true;
    observation.successfulLaunchCommit = true;
    for (int index = 0; index < intervalCount; ++index) {
        CanonicalCueTipContactInterval interval;
        interval.ordinal = index;
        interval.startTimeSeconds = 0.001 + index * 0.003;
        interval.endTimeSeconds = 0.002 + index * 0.003;
        interval.startTickOffsetSeconds = interval.startTimeSeconds;
        interval.endTickOffsetSeconds = interval.endTimeSeconds;
        interval.contactNormal = Point3{0.0f, 0.0f, 1.0f};
        interval.cueVelocityStart = Point3{0.0f, 0.0f, 150.0f};
        interval.cueVelocityEnd = Point3{0.0f, 0.0f, 100.0f};
        interval.cueBallVelocityEnd = Point3{0.0f, 0.0f, 50.0f};
        interval.normalImpulseNs = 1.0;
        interval.origin = CueTipIntervalOrigin::AuthoritativeCueMotionObserver;
        interval.solverEventIds = {100 + index};
        observation.intervals.push_back(interval);
    }
    const auto validation = validateCanonicalCueTipStrokeObservation(
        observation);
    expect(validation.valid && validation.complete,
        "canonical tip fixture validates");
    return observation;
}

CanonicalCueObjectOrderObservation orderObservation(double contactTime,
                                                      bool complete = true)
{
    CanonicalCueObjectOrderObservation observation;
    observation.shotIdentity = "shot.d5c.1";
    observation.actorPlayerId = 0;
    observation.rackIdentity = "rack.d5c.1";
    observation.rulesRevision = 17;
    observation.rulesStateHash = "rules.sha.d5c.1";
    observation.physicsTickStart = 101;
    observation.physicsTickEnd = 103;
    observation.tipSeparationTimeSeconds = 0.004;
    CanonicalCueObjectContactEvidence contact;
    contact.objectBallId = 1;
    contact.strokeRelativeTimeSeconds = contactTime;
    contact.physicsTick = 101;
    contact.wholeTickTimeOfImpactSeconds = 0.001;
    contact.solverEventId = 11;
    contact.solverIslandId = 3;
    contact.physicsContactOrdinal = 0;
    contact.velocityImpulseApplied = true;
    contact.normalImpulseNs = 1.0;
    contact.complete = true;
    observation.contacts = {contact};
    observation.d5AFullStrokeComplete = true;
    observation.successfulPhysicsTicksContiguous = true;
    observation.shotEndObserved = complete;
    const auto validation = validateCanonicalCueObjectOrderObservation(
        observation);
    expect(validation.valid && validation.complete == complete,
        "canonical order fixture validates");
    return observation;
}

void testIntervalCountIsTheOnlyAutomaticPredicate()
{
    for (int count : {2, 3}) {
        auto classification = classifyCanonicalDoubleHit(
            tipObservation(count), orderObservation(0.003), false);
        expect(classification.complete && classification.result ==
            CanonicalDoubleHitResult::DoubleHitFoul,
            "TWO_OR_THREE_COMPLETE_INTERVALS_AUTOMATIC_DOUBLE_HIT_FOUL");
        expect(classification.canonicalTipIntervalCount == count,
            "IMMUTABLE_RESULT_BINDS_INTERVAL_COUNT");
        const std::string digest = classification.digest;
        const auto validation = validateCanonicalDoubleHitClassification(
            classification);
        expect(validation.valid && validation.complete &&
            classification.digest == digest,
            "EXACT_CLASSIFIER_RETRY_ZERO_DELTA");
    }
    for (int count : {0, 1}) {
        auto classification = classifyCanonicalDoubleHit(
            tipObservation(count), orderObservation(0.005), false);
        expect(classification.complete && classification.result ==
            CanonicalDoubleHitResult::NoDoubleHitObserved,
            "ZERO_OR_ONE_INTERVAL_NEVER_AUTOMATIC_FOUL");
    }
}

void testSignedReviewDoesNotCreateOrOverrideFoul()
{
    for (double contactTime : {0.003, 0.004}) {
        auto classification = classifyCanonicalDoubleHit(
            tipObservation(1), orderObservation(contactTime), true);
        expect(classification.result ==
            CanonicalDoubleHitResult::SignedRefereeReviewRequired,
            "SIGNED_CLOSE_CASE_RAW_BEFORE_OR_EQUAL_REVIEW_ONLY");
    }
    auto signedAfter = classifyCanonicalDoubleHit(
        tipObservation(1), orderObservation(0.005), true);
    expect(signedAfter.result ==
        CanonicalDoubleHitResult::SignedRefereeReviewRequired,
        "SIGNED_OTHER_CLOSE_CASE_REMAINS_REVIEW_ONLY");
    auto unsignedAfter = classifyCanonicalDoubleHit(
        tipObservation(1), orderObservation(0.005), false);
    expect(unsignedAfter.result ==
        CanonicalDoubleHitResult::NoDoubleHitObserved,
        "RAW_AFTER_NEVER_AUTOMATIC_FOUL");
    auto unsignedBefore = classifyCanonicalDoubleHit(
        tipObservation(1), orderObservation(0.003), false);
    expect(unsignedBefore.result ==
        CanonicalDoubleHitResult::NoDoubleHitObserved,
        "RAW_BEFORE_WITHOUT_SIGNED_CLOSE_CASE_NEVER_AUTOMATIC_FOUL");
    auto proven = classifyCanonicalDoubleHit(
        tipObservation(2), orderObservation(0.004), true);
    expect(proven.result == CanonicalDoubleHitResult::DoubleHitFoul,
        "SIGNED_REVIEW_NEVER_OVERRIDES_MULTI_INTERVAL_FOUL");
}

void testIncompleteMismatchedOrTamperedInputsFailClosed()
{
    auto incompleteTip = tipObservation(2);
    incompleteTip.noDroppedIntervals = false;
    validateCanonicalCueTipStrokeObservation(incompleteTip);
    auto incomplete = classifyCanonicalDoubleHit(
        incompleteTip, orderObservation(0.003), true);
    expect(!incomplete.complete && incomplete.result ==
        CanonicalDoubleHitResult::IncompleteNoClassification,
        "INCOMPLETE_D5_A_NO_CLASSIFICATION");

    auto incompleteOrder = orderObservation(0.003, false);
    incomplete = classifyCanonicalDoubleHit(
        tipObservation(2), incompleteOrder, true);
    expect(!incomplete.complete,
        "INCOMPLETE_D5_B_NO_CLASSIFICATION");

    auto mismatched = orderObservation(0.003);
    mismatched.actorPlayerId = 1;
    validateCanonicalCueObjectOrderObservation(mismatched);
    incomplete = classifyCanonicalDoubleHit(
        tipObservation(2), mismatched, false);
    expect(!incomplete.complete,
        "MISMATCHED_ACTOR_RACK_REVISION_OR_STATE_HASH_REJECTED");

    auto tampered = tipObservation(2);
    tampered.intervals[0].normalImpulseNs += 0.25;
    incomplete = classifyCanonicalDoubleHit(
        tampered, orderObservation(0.003), false);
    expect(!incomplete.complete,
        "MISSING_OR_CHANGED_SOURCE_DIGEST_REJECTED");
}

void testClassificationValidationIsFailClosed()
{
    auto valid = classifyCanonicalDoubleHit(
        tipObservation(2), orderObservation(0.003), false);
    auto badPolicy = valid;
    badPolicy.selectedPolicySha256 = "changed";
    expect(!validateCanonicalDoubleHitClassification(badPolicy).valid,
        "CHANGED_POLICY_DIGEST_REJECTED");
    auto forgedReview = valid;
    forgedReview.result =
        CanonicalDoubleHitResult::SignedRefereeReviewRequired;
    forgedReview.signedCloseCaseBound = true;
    expect(!validateCanonicalDoubleHitClassification(forgedReview).valid,
        "MULTI_INTERVAL_CANNOT_BE_REWRITTEN_AS_REVIEW");
    auto forgedFoul = classifyCanonicalDoubleHit(
        tipObservation(1), orderObservation(0.003), true);
    forgedFoul.result = CanonicalDoubleHitResult::DoubleHitFoul;
    expect(!validateCanonicalDoubleHitClassification(forgedFoul).valid,
        "ONE_INTERVAL_CANNOT_BE_REWRITTEN_AS_FOUL");
}
}

int main()
{
    testIntervalCountIsTheOnlyAutomaticPredicate();
    testSignedReviewDoesNotCreateOrOverrideFoul();
    testIncompleteMismatchedOrTamperedInputsFailClosed();
    testClassificationValidationIsFailClosed();
    std::cout << "double_hit_classifier_tests: PASS\n";
}
