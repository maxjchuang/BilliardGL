#include "rules/push_classifier.h"

#include <cmath>
namespace billiardgl::rules {

CanonicalPushClassification classifyCanonicalPush(
    const CanonicalCueTipStrokeObservation& input,
    const PushReviewState* signedState)
{
    CanonicalPushClassification result;result.shotIdentity=input.shotIdentity;
    CanonicalCueTipStrokeObservation observation=input;
    const std::string suppliedDigest=observation.digest;
    const auto validation=validateCanonicalCueTipStrokeObservation(observation);
    bool measurements=validation.valid&&validation.complete&&
        observation.digest==suppliedDigest&&!observation.intervals.empty();
    for(const auto& interval:observation.intervals){
        const double duration=interval.endTimeSeconds-interval.startTimeSeconds;
        measurements=measurements&&interval.relativeTipBallPathLengthAttested&&
            std::isfinite(duration)&&duration>0.0&&
            std::isfinite(interval.relativeTipBallPathLengthCm)&&
            interval.relativeTipBallPathLengthCm>=0.0;
        result.durationSeconds+=duration;
        result.distanceCm+=interval.relativeTipBallPathLengthCm;
    }
    result.cueTipObservationDigest=observation.digest;
    bool hasSigned=signedState!=nullptr&&
        signedState->phase==PushReviewPhase::Consumed&&
        signedState->committed&&signedState->reviewNeeded&&
        signedState->consumedShotIdentity==observation.shotIdentity&&
        signedState->rackIdentity==observation.rackIdentity&&
        signedState->context.parentRevision==observation.rulesRevision&&
        signedState->context.parentStateHash==observation.rulesStateHash&&
        signedState->context.cueBallId==0&&
        !signedState->proposalDecisionId.empty()&&
        !signedState->proposalPayloadSha256.empty()&&
        !signedState->finalPayloadSha256.empty();
    if(!measurements||(!hasSigned&&signedState!=nullptr)){
        result.result=CanonicalPushResult::IncompleteNoClassification;
        return result;
    }
    if(hasSigned){
        result.result=CanonicalPushResult::PushFoul;
        result.signedPushDecisionId=signedState->proposalDecisionId;
        result.pushProposalPayloadSha256=signedState->proposalPayloadSha256;
        result.pushFinalPayloadSha256=signedState->finalPayloadSha256;
    }else result.result=CanonicalPushResult::NoPushClassification;
    validateCanonicalPushClassification(result);return result;
}

}  // namespace billiardgl::rules
