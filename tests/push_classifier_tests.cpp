#include "rules/push_classifier.h"

#include <cstdlib>
#include <iostream>
#include <limits>

using namespace billiardgl;
using namespace billiardgl::rules;

namespace {
void expect(bool value, const char* message)
{
    if (!value) { std::cerr << message << '\n'; std::exit(1); }
}

CanonicalCueTipStrokeObservation stroke()
{
    CanonicalCueTipStrokeObservation value;
    value.shotIdentity = "shot.7";
    value.actorPlayerId = 0;
    value.rackIdentity = "rack.2";
    value.rulesRevision = 4;
    value.rulesStateHash = "state.4";
    value.startTick = 10;
    value.endTick = 12;
    CanonicalCueTipContactInterval interval;
    interval.ordinal = 0;
    interval.endTimeSeconds = 0.02;
    interval.endTickOffsetSeconds = 0.02;
    interval.relativeTipBallPathLengthCm = 1.25;
    interval.relativeTipBallPathLengthAttested = true;
    value.intervals.push_back(interval);
    value.observationStartedBeforeFirstContact = true;
    value.observationEndedAfterAuthoritativeRelease = true;
    value.noDroppedIntervals = true;
    value.successfulLaunchCommit = true;
    expect(validateCanonicalCueTipStrokeObservation(value).complete,
           "COMPLETE_DURATION_AND_DISTANCE_BIND_SAME_SHOT_AND_SOURCE_DIGESTS");
    return value;
}

PushReviewState signedState(const CanonicalCueTipStrokeObservation& value)
{
    PushReviewState state;
    state.phase = PushReviewPhase::Consumed;
    state.rackIdentity = value.rackIdentity;
    state.context.parentRevision = value.rulesRevision;
    state.context.parentStateHash = value.rulesStateHash;
    state.proposalDecisionId = "push.1";
    state.proposalPayloadSha256 = "proposal";
    state.finalPayloadSha256 = "final";
    state.refereeSubjectId = "referee";
    state.committed = true;
    state.reviewNeeded = true;
    state.consumedShotIdentity = value.shotIdentity;
    return state;
}
}

int main()
{
    auto value = stroke();
    auto none = classifyCanonicalPush(value, nullptr);
    expect(none.complete && none.result == CanonicalPushResult::NoPushClassification,
           "MEASUREMENT_ALONE_NEVER_PUSH");
    auto signedFinal = signedState(value);
    auto push = classifyCanonicalPush(value, &signedFinal);
    expect(push.complete && push.result == CanonicalPushResult::PushFoul &&
           push.durationSeconds == 0.02 && push.distanceCm == 1.25,
           "SIGNED_OBVIOUS_PUSH_FINAL_PLUS_COMPLETE_MEASUREMENTS_CLASSIFIES_PUSH_FOUL");
    expect(validateCanonicalPushClassification(push).complete,
           "push classification validates");
    signedFinal.consumedShotIdentity = "other";
    expect(classifyCanonicalPush(value, &signedFinal).result ==
               CanonicalPushResult::IncompleteNoClassification,
           "SIGNED_FINAL_WRONG_SHOT_NEVER_PUSH");
    value.intervals[0].relativeTipBallPathLengthAttested = false;
    expect(classifyCanonicalPush(value, nullptr).result ==
               CanonicalPushResult::IncompleteNoClassification,
           "INCOMPLETE_DURATION_OR_DISTANCE_NO_PUSH_CLASSIFICATION");
    value = stroke();
    value.intervals[0].relativeTipBallPathLengthCm =
        std::numeric_limits<double>::infinity();
    expect(!validateCanonicalCueTipStrokeObservation(value).valid,
           "NONFINITE_NEGATIVE_OR_ENDPOINT_APPROXIMATED_PATH_REJECTED");
    std::cout << "push_classifier_tests: PASS\n";
}
