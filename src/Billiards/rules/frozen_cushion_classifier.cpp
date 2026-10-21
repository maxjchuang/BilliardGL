#include "rules/frozen_cushion_classifier.h"
#include "rules/push_classifier.h"

#include <algorithm>
#include <set>

namespace billiardgl::rules {
namespace {
bool legal(const std::vector<int>& ids,int id)
{
    return std::find(ids.begin(),ids.end(),id)!=ids.end();
}
}

CanonicalFrozenCushionClassification classifyCanonicalFrozenCushion(
    const CanonicalFrozenCushionShotObservation& frozenInput,
    const CanonicalCueObjectOrderObservation& cueInput,
    const std::vector<int>& legalIds)
{
    CanonicalFrozenCushionClassification result;
    result.shotIdentity=frozenInput.shotIdentity;
    CanonicalFrozenCushionShotObservation frozen=frozenInput;
    CanonicalCueObjectOrderObservation cue=cueInput;
    const std::string frozenDigest=frozen.digest,cueDigest=cue.digest;
    const auto frozenValidation=
        validateCanonicalFrozenCushionShotObservation(frozen);
    const auto cueValidation=validateCanonicalCueObjectOrderObservation(cue);
    result.frozenObservationDigest=frozen.digest;
    result.cueObjectObservationDigest=cue.digest;
    result.frozenDeclarationDecisionId=
        frozen.binding.declarationDecisionId;
    if(!frozenValidation.valid||!frozenValidation.complete||
       !cueValidation.valid||!cueValidation.complete||
       frozen.digest!=frozenDigest||cue.digest!=cueDigest||
       frozen.shotIdentity!=cue.shotIdentity||
       frozen.binding.consumedShotIdentity!=frozen.shotIdentity||
       !frozen.binding.authenticated||
       !frozen.binding.committedAndConsumed||cue.contacts.empty())return result;
    const double firstTime=cue.contacts.front().strokeRelativeTimeSeconds;
    const auto atFirst=[firstTime](const auto& contact){
        return contact.strokeRelativeTimeSeconds==firstTime;};
    auto firstLegal=std::find_if(cue.contacts.begin(),cue.contacts.end(),
        [&](const auto& contact){return atFirst(contact)&&
            legal(legalIds,contact.objectBallId);});
    if(firstLegal==cue.contacts.end())return result;
    result.firstLegalObjectBallId=firstLegal->objectBallId;
    result.firstLegalObjectContactTimeSeconds=firstTime;
    bool departed=false;
    for(const auto& event:frozen.events){
        if(!event.complete||event.strokeRelativeTimeSeconds<=firstTime)
            continue;
        const std::string kind=event.kind?event.kind:"";
        if(kind=="DECLARED_OBJECT_DEPARTURE")departed=true;
        else if(kind=="DECLARED_OBJECT_NAMED_CUSHION_RECONTACT"&&
                departed&&
                event.straightCushionFeatureId==
                    frozen.binding.namedStraightCushionFeatureId){
            result.declaredBallDepartureThenNamedRecontact=true;
            result.prerequisiteSatisfied=true;
        }
        else if(kind=="DECLARED_OBJECT_OTHER_STRAIGHT_CUSHION")
            result.prerequisiteSatisfied=true;
        else if(kind=="OTHER_OBJECT_STRAIGHT_CUSHION")
            result.otherObjectStraightCushionAlternative=true;
        else if(kind=="OBJECT_POCKET_CAPTURE")
            result.completeObjectPocketAlternative=true;
        // The declared ball's required recontact is the §15.2 alternative,
        // not evidence that the prerequisite was already satisfied.
        if(kind=="OTHER_OBJECT_STRAIGHT_CUSHION"||
           kind=="OBJECT_POCKET_CAPTURE")
            result.prerequisiteSatisfied=true;
    }
    if(result.prerequisiteSatisfied||
       result.declaredBallDepartureThenNamedRecontact||
       result.otherObjectStraightCushionAlternative||
       result.completeObjectPocketAlternative)
        result.result=CanonicalFrozenCushionResult::NoFoul;
    else result.result=CanonicalFrozenCushionResult::FrozenCushionFoul;
    validateCanonicalFrozenCushionClassification(result);return result;
}

}  // namespace billiardgl::rules
