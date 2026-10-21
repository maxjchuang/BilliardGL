#include "rules/frozen_cushion_classifier.h"

#include <cstdlib>
#include <iostream>

using namespace billiardgl;
using namespace billiardgl::rules;

namespace {
void expect(bool value, const char* message)
{
    if (!value) { std::cerr << message << '\n'; std::exit(1); }
}

CanonicalCueObjectOrderObservation order()
{
    CanonicalCueObjectOrderObservation value;
    value.shotIdentity = "shot.8";
    value.actorPlayerId = 0;
    value.rackIdentity = "rack.2";
    value.rulesRevision = 4;
    value.rulesStateHash = "state.4";
    value.physicsTickStart = 20;
    value.physicsTickEnd = 22;
    value.tipSeparationTimeSeconds = 0.01;
    CanonicalCueObjectContactEvidence contact;
    contact.objectBallId = 2;
    contact.strokeRelativeTimeSeconds = 0.02;
    contact.physicsTick = 21;
    contact.wholeTickTimeOfImpactSeconds = 0.001;
    contact.solverEventId = 1;
    contact.solverIslandId = 1;
    contact.physicsContactOrdinal = 0;
    contact.normalImpulseNs = 0.1;
    contact.velocityImpulseApplied = true;
    contact.complete = true;
    value.contacts.push_back(contact);
    value.d5AFullStrokeComplete = true;
    value.successfulPhysicsTicksContiguous = true;
    value.shotEndObserved = true;
    expect(validateCanonicalCueObjectOrderObservation(value).complete,
           "complete cue object order");
    return value;
}

CanonicalFrozenCushionShotObservation frozen()
{
    CanonicalFrozenCushionShotObservation value;
    value.shotIdentity = "shot.8";
    value.binding.declarationDecisionId = "frozen.1";
    value.binding.declaredObjectBallId = 2;
    value.binding.namedStraightCushionFeatureId = 3;
    value.binding.declarationParentRevision = 4;
    value.binding.declarationParentStateHash = "state.4";
    value.binding.consumedShotIdentity = value.shotIdentity;
    value.binding.planeObservationId = "plane.2";
    value.binding.authenticated = true;
    value.binding.committedAndConsumed = true;
    value.physicsTickStart = 20;
    value.physicsTickEnd = 22;
    value.successfulPhysicsTicksContiguous = true;
    value.shotEndObserved = true;
    return value;
}

CanonicalFrozenCushionEvent event(const char* kind, double time, int ball,
                                  int feature = -1)
{
    CanonicalFrozenCushionEvent value;
    value.kind = kind;
    value.strokeRelativeTimeSeconds = time;
    value.physicsTick = 21;
    value.ballId = ball;
    value.straightCushionFeatureId = feature;
    value.solverEventId = static_cast<int>(time * 1000.0);
    value.physicsContactOrdinal = 0;
    if (std::string(kind) == "DECLARED_OBJECT_DEPARTURE") {
        value.origin = "AUTHORITATIVE_CONTINUOUS_SURFACE_INTEGRATOR";
        value.solverEventId = -1;
    }
    value.complete = true;
    return value;
}

void seal(CanonicalFrozenCushionShotObservation& value)
{
    auto validation = validateCanonicalFrozenCushionShotObservation(value);
    if (!validation.complete) std::cerr << validation.error << '\n';
    expect(validation.complete, "complete frozen observation");
}
}

int main()
{
    auto contacts = order();
    auto value = frozen();
    value.events.push_back(event("OTHER_OBJECT_STRAIGHT_CUSHION", 0.03, 5, 1));
    value.otherObjectStraightCushionBalls = {5};
    seal(value);
    auto result = classifyCanonicalFrozenCushion(value, contacts, {2});
    expect(result.complete && result.result == CanonicalFrozenCushionResult::NoFoul,
           "FROZEN_CUSHION_PREREQUISITE_STRAIGHT_IMPULSE_AFTER_FIRST_LEGAL_CONTACT_SKIPS_EXTRA_REQUIREMENT");

    value = frozen();
    value.events.push_back(event("DECLARED_OBJECT_DEPARTURE", 0.03, 2));
    value.events.push_back(event("DECLARED_OBJECT_NAMED_CUSHION_RECONTACT", 0.04, 2, 3));
    value.departureObserved = true;
    value.sameNamedCushionRecontactAfterDeparture = true;
    seal(value);
    result = classifyCanonicalFrozenCushion(value, contacts, {2});
    expect(result.complete && result.result == CanonicalFrozenCushionResult::NoFoul,
           "UNMET_PREREQUISITE_DECLARED_BALL_LEAVES_THEN_RECONTACTS_NAMED_STRAIGHT_CUSHION");

    value = frozen();
    value.events.push_back(event(
        "DECLARED_OBJECT_NAMED_CUSHION_RECONTACT", 0.03, 2, 3));
    seal(value);
    result = classifyCanonicalFrozenCushion(value, contacts, {2});
    expect(result.complete && result.result ==
               CanonicalFrozenCushionResult::FrozenCushionFoul,
           "RECONTACT_BEFORE_DECLARED_OBJECT_DEPARTURE_REJECTED");

    value = frozen();
    value.events.push_back(event(
        "DECLARED_OBJECT_OTHER_STRAIGHT_CUSHION", 0.03, 2, 1));
    seal(value);
    result = classifyCanonicalFrozenCushion(value, contacts, {2});
    expect(result.complete && result.result == CanonicalFrozenCushionResult::NoFoul,
           "post-contact declared object straight-cushion impulse meets prerequisite");

    value = frozen();
    seal(value);
    result = classifyCanonicalFrozenCushion(value, contacts, {2});
    expect(result.complete && result.result == CanonicalFrozenCushionResult::FrozenCushionFoul,
           "unmet obligation is foul");
    result = classifyCanonicalFrozenCushion(value, contacts, {9});
    expect(!result.complete && result.result ==
               CanonicalFrozenCushionResult::IncompleteNoClassification,
           "ILLEGAL_FIRST_OBJECT_CONTACT_DOES_NOT_OPEN_PREREQUISITE_WINDOW");
    std::cout << "frozen_cushion_classifier_tests: PASS\n";
}
