#pragma once

#include "cue_contact.h"
#include "physics_telemetry.h"

#include <cstdint>
#include <iomanip>
#include <sstream>
#include <string>

namespace billiardgl::rules {

inline constexpr const char* kD5CRootSelectedPolicySha256 =
    "c3529ff92ccb1540513f9be3fbbf725832dfd5c81ce78394d53a9b8b8c994ef6";

enum class CanonicalDoubleHitResult {
    DoubleHitFoul,
    SignedRefereeReviewRequired,
    NoDoubleHitObserved,
    IncompleteNoClassification,
};

struct CanonicalDoubleHitClassification {
    std::string schemaId = "canonical_double_hit_classification_v1";
    std::string shotIdentity;
    CanonicalDoubleHitResult result =
        CanonicalDoubleHitResult::IncompleteNoClassification;
    int canonicalTipIntervalCount = 0;
    CanonicalCueObjectTipOrder cueObjectTipOrder =
        CanonicalCueObjectTipOrder::UnorderedIncomplete;
    bool signedCloseCaseBound = false;
    std::string cueTipObservationDigest;
    std::string cueObjectObservationDigest;
    std::string selectedPolicySha256 = kD5CRootSelectedPolicySha256;
    const char* origin =
        "D5C_ROOT_SELECTED_EXACT_MACHINE_CLASSIFIER_V1";
    bool complete = false;
    std::string digest;
};

struct CanonicalDoubleHitClassificationValidation {
    bool valid = false;
    bool complete = false;
    std::string error;
};

inline const char* canonicalDoubleHitResultName(
    CanonicalDoubleHitResult result) noexcept
{
    switch (result) {
    case CanonicalDoubleHitResult::DoubleHitFoul:
        return "DOUBLE_HIT_FOUL";
    case CanonicalDoubleHitResult::SignedRefereeReviewRequired:
        return "SIGNED_REFEREE_REVIEW_REQUIRED";
    case CanonicalDoubleHitResult::NoDoubleHitObserved:
        return "NO_DOUBLE_HIT_OBSERVED";
    case CanonicalDoubleHitResult::IncompleteNoClassification:
        return "INCOMPLETE_NO_CLASSIFICATION";
    }
    return "INCOMPLETE_NO_CLASSIFICATION";
}

inline std::string canonicalDoubleHitDigest(const std::string& bytes)
{
    std::uint64_t value = 1469598103934665603ULL;
    for (const unsigned char byte : bytes) {
        value ^= byte;
        value *= 1099511628211ULL;
    }
    std::ostringstream stream;
    stream << std::hex << std::setfill('0') << std::setw(16) << value;
    return stream.str();
}

inline const char* doubleHitOrderName(
    CanonicalCueObjectTipOrder order) noexcept
{
    switch (order) {
    case CanonicalCueObjectTipOrder::ContactBeforeTipSeparation:
        return "CUE_OBJECT_CONTACT_BEFORE_TIP_SEPARATION";
    case CanonicalCueObjectTipOrder::ContactEqualTipSeparation:
        return "CUE_OBJECT_CONTACT_EQUAL_TIP_SEPARATION";
    case CanonicalCueObjectTipOrder::ContactAfterTipSeparation:
        return "CUE_OBJECT_CONTACT_AFTER_TIP_SEPARATION";
    case CanonicalCueObjectTipOrder::NoCueObjectContact:
        return "NO_CUE_OBJECT_CONTACT";
    case CanonicalCueObjectTipOrder::UnorderedIncomplete:
        return "UNORDERED_INCOMPLETE";
    }
    return "UNORDERED_INCOMPLETE";
}

inline CanonicalDoubleHitClassificationValidation
validateCanonicalDoubleHitClassification(
    CanonicalDoubleHitClassification& classification)
{
    CanonicalDoubleHitClassificationValidation result;
    if (classification.schemaId !=
            "canonical_double_hit_classification_v1" ||
        classification.shotIdentity.empty() ||
        classification.canonicalTipIntervalCount < 0 ||
        classification.cueTipObservationDigest.empty() ||
        classification.cueObjectObservationDigest.empty() ||
        classification.selectedPolicySha256 !=
            kD5CRootSelectedPolicySha256 ||
        classification.origin == nullptr ||
        std::string(classification.origin) !=
            "D5C_ROOT_SELECTED_EXACT_MACHINE_CLASSIFIER_V1") {
        result.error = "d5c_identity";
        return result;
    }
    if (classification.result ==
            CanonicalDoubleHitResult::DoubleHitFoul &&
        classification.canonicalTipIntervalCount <= 1) {
        result.error = "d5c_multiple_interval_required";
        return result;
    }
    if (classification.canonicalTipIntervalCount > 1 &&
        classification.result != CanonicalDoubleHitResult::DoubleHitFoul) {
        result.error = "d5c_multiple_interval_precedence";
        return result;
    }
    if (classification.result ==
            CanonicalDoubleHitResult::SignedRefereeReviewRequired &&
        (!classification.signedCloseCaseBound ||
         classification.canonicalTipIntervalCount > 1)) {
        result.error = "d5c_signed_review";
        return result;
    }
    classification.complete = classification.result !=
        CanonicalDoubleHitResult::IncompleteNoClassification;
    std::ostringstream bytes;
    bytes << classification.schemaId << '|'
          << classification.shotIdentity << '|'
          << canonicalDoubleHitResultName(classification.result) << '|'
          << classification.canonicalTipIntervalCount << '|'
          << doubleHitOrderName(classification.cueObjectTipOrder) << '|'
          << classification.signedCloseCaseBound << '|'
          << classification.cueTipObservationDigest << '|'
          << classification.cueObjectObservationDigest << '|'
          << classification.selectedPolicySha256 << '|'
          << classification.origin << '|' << classification.complete;
    classification.digest = canonicalDoubleHitDigest(bytes.str());
    result.valid = true;
    result.complete = classification.complete;
    if (!result.complete) result.error = "d5c_incomplete";
    return result;
}

inline CanonicalDoubleHitClassification classifyCanonicalDoubleHit(
    const CanonicalCueTipStrokeObservation& tipObservation,
    const CanonicalCueObjectOrderObservation& orderObservation,
    bool signedCloseCaseBound)
{
    CanonicalCueTipStrokeObservation verifiedTip = tipObservation;
    CanonicalCueObjectOrderObservation verifiedOrder = orderObservation;
    const std::string suppliedTipDigest = tipObservation.digest;
    const std::string suppliedOrderDigest = orderObservation.digest;
    const auto tipValidation =
        validateCanonicalCueTipStrokeObservation(verifiedTip);
    const auto orderValidation =
        validateCanonicalCueObjectOrderObservation(verifiedOrder);
    CanonicalDoubleHitClassification result;
    result.shotIdentity = tipObservation.shotIdentity;
    result.canonicalTipIntervalCount =
        static_cast<int>(tipObservation.intervals.size());
    result.cueObjectTipOrder = orderObservation.order;
    result.signedCloseCaseBound = signedCloseCaseBound;
    result.cueTipObservationDigest = tipObservation.digest;
    result.cueObjectObservationDigest = orderObservation.digest;
    const bool identitiesMatch =
        tipObservation.shotIdentity == orderObservation.shotIdentity &&
        tipObservation.actorPlayerId == orderObservation.actorPlayerId &&
        tipObservation.rackIdentity == orderObservation.rackIdentity &&
        tipObservation.rulesRevision == orderObservation.rulesRevision &&
        tipObservation.rulesStateHash == orderObservation.rulesStateHash;
    if (!tipValidation.valid || !tipValidation.complete ||
        !orderValidation.valid || !orderValidation.complete ||
        suppliedTipDigest.empty() || suppliedOrderDigest.empty() ||
        verifiedTip.digest != suppliedTipDigest ||
        verifiedOrder.digest != suppliedOrderDigest || !identitiesMatch) {
        result.result =
            CanonicalDoubleHitResult::IncompleteNoClassification;
    } else if (tipObservation.intervals.size() > 1) {
        result.result = CanonicalDoubleHitResult::DoubleHitFoul;
    } else if (signedCloseCaseBound) {
        result.result =
            CanonicalDoubleHitResult::SignedRefereeReviewRequired;
    } else {
        result.result = CanonicalDoubleHitResult::NoDoubleHitObserved;
    }
    validateCanonicalDoubleHitClassification(result);
    return result;
}

}  // namespace billiardgl::rules
