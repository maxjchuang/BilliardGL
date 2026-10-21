#pragma once

#include "physics_telemetry.h"
#include "rules/push_classifier.h"

#include <string>
#include <vector>
#include <cmath>
#include <iomanip>
#include <limits>
#include <sstream>

namespace billiardgl::rules {

enum class CanonicalFrozenCushionResult {
    NoFoul,
    FrozenCushionFoul,
    IncompleteNoClassification,
};

struct CanonicalFrozenCushionClassification {
    std::string schemaId = "canonical_frozen_cushion_classification_v1";
    std::string shotIdentity;
    CanonicalFrozenCushionResult result =
        CanonicalFrozenCushionResult::IncompleteNoClassification;
    std::string frozenDeclarationDecisionId;
    int firstLegalObjectBallId = -1;
    double firstLegalObjectContactTimeSeconds = 0.0;
    bool prerequisiteSatisfied = false;
    bool declaredBallDepartureThenNamedRecontact = false;
    bool otherObjectStraightCushionAlternative = false;
    bool completeObjectPocketAlternative = false;
    std::string frozenObservationDigest;
    std::string cueObjectObservationDigest;
    std::string selectedPolicySha256 = kD5DRootSelectedPolicySha256;
    const char* origin =
        "D5D_ROOT_SELECTED_FROZEN_CUSHION_CLASSIFIER_V1";
    bool complete = false;
    std::string digest;
};

struct CanonicalFrozenCushionClassificationValidation {
    bool valid = false;
    bool complete = false;
    std::string error;
};

CanonicalFrozenCushionClassification classifyCanonicalFrozenCushion(
    const CanonicalFrozenCushionShotObservation& frozenObservation,
    const CanonicalCueObjectOrderObservation& cueObjectObservation,
    const std::vector<int>& legalFirstContactBallIds);
inline const char* canonicalFrozenCushionResultName(
    CanonicalFrozenCushionResult result) noexcept
{
    switch(result){
    case CanonicalFrozenCushionResult::NoFoul:return "NO_FOUL";
    case CanonicalFrozenCushionResult::FrozenCushionFoul:
        return "FROZEN_CUSHION_FOUL";
    case CanonicalFrozenCushionResult::IncompleteNoClassification:
        return "INCOMPLETE_NO_CLASSIFICATION";
    }
    return "INCOMPLETE_NO_CLASSIFICATION";
}

inline CanonicalFrozenCushionClassificationValidation
validateCanonicalFrozenCushionClassification(
    CanonicalFrozenCushionClassification& value)
{
    CanonicalFrozenCushionClassificationValidation result;
    if(value.schemaId!="canonical_frozen_cushion_classification_v1"||
       value.shotIdentity.empty()||value.frozenDeclarationDecisionId.empty()||
       value.firstLegalObjectBallId<1||value.firstLegalObjectBallId>15||
       !std::isfinite(value.firstLegalObjectContactTimeSeconds)||
       value.firstLegalObjectContactTimeSeconds<0.0||
       value.frozenObservationDigest.empty()||
       value.cueObjectObservationDigest.empty()||
       value.selectedPolicySha256!=kD5DRootSelectedPolicySha256||
       value.origin==nullptr||std::string(value.origin)!=
           "D5D_ROOT_SELECTED_FROZEN_CUSHION_CLASSIFIER_V1"||
       value.result==CanonicalFrozenCushionResult::IncompleteNoClassification){
        result.error="d5d_frozen_identity";return result;}
    if(value.result==CanonicalFrozenCushionResult::FrozenCushionFoul&&
       (value.prerequisiteSatisfied||
        value.declaredBallDepartureThenNamedRecontact||
        value.otherObjectStraightCushionAlternative||
        value.completeObjectPocketAlternative)){
        result.error="d5d_frozen_foul_conflict";return result;}
    if(value.result==CanonicalFrozenCushionResult::NoFoul&&
       !(value.prerequisiteSatisfied||
         value.declaredBallDepartureThenNamedRecontact||
         value.otherObjectStraightCushionAlternative||
         value.completeObjectPocketAlternative)){
        result.error="d5d_frozen_legal_basis";return result;}
    std::ostringstream canonical;canonical.imbue(std::locale::classic());
    canonical<<std::setprecision(std::numeric_limits<double>::max_digits10)
        <<value.schemaId<<'|'<<value.shotIdentity<<'|'
        <<canonicalFrozenCushionResultName(value.result)<<'|'
        <<value.frozenDeclarationDecisionId<<'|'
        <<value.firstLegalObjectBallId<<'|'
        <<value.firstLegalObjectContactTimeSeconds<<'|'
        <<value.prerequisiteSatisfied<<'|'
        <<value.declaredBallDepartureThenNamedRecontact<<'|'
        <<value.otherObjectStraightCushionAlternative<<'|'
        <<value.completeObjectPocketAlternative<<'|'
        <<value.frozenObservationDigest<<'|'
        <<value.cueObjectObservationDigest<<'|'
        <<value.selectedPolicySha256<<'|'<<value.origin;
    value.digest=d5dClassificationDigest(canonical.str());value.complete=true;
    result.valid=true;result.complete=true;return result;
}

}  // namespace billiardgl::rules
