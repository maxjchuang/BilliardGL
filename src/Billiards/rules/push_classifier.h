#pragma once

#include "cue_contact.h"
#include "rules/push_review_declaration.h"

#include <string>
#include <cmath>
#include <cstdint>
#include <iomanip>
#include <limits>
#include <sstream>

namespace billiardgl::rules {

inline constexpr const char* kD5DRootSelectedPolicySha256 =
    "ef93c3ae59d2a408ad3433f7a718d1634969a898f57a4e062109223d32ca611d";

enum class CanonicalPushResult {
    PushFoul,
    NoPushClassification,
    IncompleteNoClassification,
};

struct CanonicalPushClassification {
    std::string schemaId = "canonical_push_classification_v1";
    std::string shotIdentity;
    CanonicalPushResult result =
        CanonicalPushResult::IncompleteNoClassification;
    double durationSeconds = 0.0;
    double distanceCm = 0.0;
    std::string signedPushDecisionId;
    std::string cueTipObservationDigest;
    std::string pushProposalPayloadSha256;
    std::string pushFinalPayloadSha256;
    std::string selectedPolicySha256 = kD5DRootSelectedPolicySha256;
    const char* origin = "D5D_ROOT_SELECTED_SIGNED_PUSH_CLASSIFIER_V1";
    bool complete = false;
    std::string digest;
};

struct CanonicalPushClassificationValidation {
    bool valid = false;
    bool complete = false;
    std::string error;
};

CanonicalPushClassification classifyCanonicalPush(
    const CanonicalCueTipStrokeObservation& observation,
    const PushReviewState* signedPushState);
inline const char* canonicalPushResultName(CanonicalPushResult result) noexcept
{
    switch(result){
    case CanonicalPushResult::PushFoul:return "PUSH_FOUL";
    case CanonicalPushResult::NoPushClassification:
        return "NO_PUSH_CLASSIFICATION";
    case CanonicalPushResult::IncompleteNoClassification:
        return "INCOMPLETE_NO_CLASSIFICATION";
    }
    return "INCOMPLETE_NO_CLASSIFICATION";
}

inline std::string d5dClassificationDigest(const std::string& bytes)
{
    std::uint64_t hash=1469598103934665603ULL;
    for(unsigned char byte:bytes){hash^=byte;hash*=1099511628211ULL;}
    std::ostringstream out;out<<std::hex<<std::setfill('0')
        <<std::setw(16)<<hash;return out.str();
}

inline CanonicalPushClassificationValidation
validateCanonicalPushClassification(CanonicalPushClassification& value)
{
    CanonicalPushClassificationValidation result;
    if(value.schemaId!="canonical_push_classification_v1"||
       value.shotIdentity.empty()||!std::isfinite(value.durationSeconds)||
       !std::isfinite(value.distanceCm)||value.durationSeconds<0.0||
       value.distanceCm<0.0||value.cueTipObservationDigest.empty()||
       value.selectedPolicySha256!=kD5DRootSelectedPolicySha256||
       value.origin==nullptr||std::string(value.origin)!=
           "D5D_ROOT_SELECTED_SIGNED_PUSH_CLASSIFIER_V1"){
        result.error="d5d_push_identity";return result;}
    if(value.result==CanonicalPushResult::PushFoul&&
       (value.signedPushDecisionId.empty()||
        value.pushProposalPayloadSha256.empty()||
        value.pushFinalPayloadSha256.empty())){
        result.error="d5d_push_signed_final";return result;}
    if(value.result!=CanonicalPushResult::PushFoul&&
       (!value.signedPushDecisionId.empty()||
        !value.pushProposalPayloadSha256.empty()||
        !value.pushFinalPayloadSha256.empty())){
        result.error="d5d_push_unsigned_result";return result;}
    std::ostringstream canonical;canonical.imbue(std::locale::classic());
    canonical<<std::setprecision(std::numeric_limits<double>::max_digits10)
        <<value.schemaId<<'|'<<value.shotIdentity<<'|'
        <<canonicalPushResultName(value.result)<<'|'<<value.durationSeconds
        <<'|'<<value.distanceCm<<'|'<<value.signedPushDecisionId<<'|'
        <<value.cueTipObservationDigest<<'|'
        <<value.pushProposalPayloadSha256<<'|'
        <<value.pushFinalPayloadSha256<<'|'<<value.selectedPolicySha256
        <<'|'<<value.origin;
    value.digest=d5dClassificationDigest(canonical.str());value.complete=true;
    result.valid=true;result.complete=true;return result;
}

}  // namespace billiardgl::rules
