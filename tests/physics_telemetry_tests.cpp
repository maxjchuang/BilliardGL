#include "game_state.h"
#include "physics_telemetry.h"
#include "surface_motion.h"
#include "game_runtime.h"
#include "shot.h"

#include <cmath>
#include <cstdlib>
#include <iostream>
#include <limits>

namespace {

void expect(bool value, const char* message)
{
    if (!value) {
        std::cerr << message << '\n';
        std::exit(1);
    }
}

bool close(double a, double b, double epsilon = 1e-6)
{
    return std::fabs(a - b) <= epsilon;
}

}  // namespace

int main()
{
    billiardgl::CanonicalCueObjectOrderObservation d5bOrder;
    d5bOrder.shotIdentity = "d5b.order";d5bOrder.actorPlayerId=0;
    d5bOrder.rackIdentity="rack";d5bOrder.rulesStateHash="state";
    d5bOrder.physicsTickStart=1;d5bOrder.physicsTickEnd=1;
    d5bOrder.tipSeparationTimeSeconds=0.005;
    d5bOrder.d5AFullStrokeComplete=true;
    d5bOrder.successfulPhysicsTicksContiguous=true;
    d5bOrder.shotEndObserved=true;
    billiardgl::CanonicalCueObjectContactEvidence d5bContact;
    d5bContact.objectBallId=1;d5bContact.strokeRelativeTimeSeconds=0.004;
    d5bContact.physicsTick=1;d5bContact.wholeTickTimeOfImpactSeconds=0.004;
    d5bContact.solverEventId=0;d5bContact.solverIslandId=0;
    d5bContact.physicsContactOrdinal=0;d5bContact.complete=true;
    d5bOrder.contacts.push_back(d5bContact);
    const auto d5bValidation=
        billiardgl::validateCanonicalCueObjectOrderObservation(d5bOrder);
    expect(d5bValidation.complete&&d5bOrder.order==
        billiardgl::CanonicalCueObjectTipOrder::ContactBeforeTipSeparation,
        "D5-B records strict contact-before-separation as a raw complete fact");
    d5bOrder.contacts[0].strokeRelativeTimeSeconds=0.005;
    expect(billiardgl::validateCanonicalCueObjectOrderObservation(d5bOrder).complete&&
        d5bOrder.order==billiardgl::CanonicalCueObjectTipOrder::
            ContactEqualTipSeparation,
        "D5-B preserves exact equality without classifying a foul");
    d5bOrder.contacts[0].strokeRelativeTimeSeconds=
        std::numeric_limits<double>::quiet_NaN();
    expect(!billiardgl::validateCanonicalCueObjectOrderObservation(d5bOrder).valid,
        "D5-B rejects nonfinite total-order evidence");
    billiardgl::CanonicalCueObjectOrderObservation identity;
    identity.shotIdentity="d5b.observer";identity.actorPlayerId=0;
    identity.rackIdentity="rack";identity.rulesStateHash="state";
    identity.physicsTickStart=1;
    billiardgl::CanonicalD5BShotObserver observer;
    expect(observer.begin(identity),"D5-B observer begins before physics");
    billiardgl::CanonicalCueTipStrokeObservation tip;
    tip.shotIdentity=identity.shotIdentity;tip.fullStrokeComplete=true;
    billiardgl::CanonicalCueTipContactInterval tipInterval;
    tipInterval.endTimeSeconds=0.005;tip.intervals.push_back(tipInterval);
    expect(observer.bindTipObservation(tip),"D5-B binds complete D5-A observation");
    billiardgl::PhysicsStepTelemetry orderedTick;
    orderedTick.wholeTickDurationSeconds=0.01;
    billiardgl::PhysicsContactRecord cueObject;
    cueObject.kind=billiardgl::PhysicsContactKind::BallBall;
    cueObject.firstBall=0;cueObject.secondBall=1;
    cueObject.solverEventId=0;cueObject.solverIslandId=0;
    cueObject.wholeTickTimeOfImpactSeconds=0.004;
    cueObject.velocityImpulseApplied=true;cueObject.normalImpulseNs=0.1;
    orderedTick.contacts.push_back(cueObject);
    expect(observer.consumeSuccessfulPhysicsTick(orderedTick,1,0.0)&&
        observer.consumeSuccessfulPhysicsTick(orderedTick,1,0.0),
        "D5-B exact physics tick retry dedupes");
    auto conflictingTick=orderedTick;
    conflictingTick.contacts[0].normalImpulseNs=0.2;
    expect(!observer.consumeSuccessfulPhysicsTick(conflictingTick,1,0.0)&&
        observer.cueObjectObservation().contacts.size()==1,
        "D5-B conflicting tick retry has zero observation delta");
    expect(observer.finish(true).complete&&
        observer.cueObjectObservation().order==
            billiardgl::CanonicalCueObjectTipOrder::ContactBeforeTipSeparation,
        "D5-B complete observer resolves strict total order");
    expect(observer.finish(true).complete&&
        observer.cueObjectObservation().contacts.size()==1,
        "D5-B exact final retry preserves one immutable observation");

    auto afterIdentity=identity;afterIdentity.shotIdentity="d5b.after";
    billiardgl::CanonicalD5BShotObserver afterObserver;
    auto afterTip=tip;afterTip.shotIdentity=afterIdentity.shotIdentity;
    auto afterTick=orderedTick;
    afterTick.contacts[0].wholeTickTimeOfImpactSeconds=0.006;
    expect(afterObserver.begin(afterIdentity)&&
        afterObserver.bindTipObservation(afterTip)&&
        afterObserver.consumeSuccessfulPhysicsTick(afterTick,1,0.0)&&
        afterObserver.finish(true).complete&&
        afterObserver.cueObjectObservation().order==
            billiardgl::CanonicalCueObjectTipOrder::ContactAfterTipSeparation,
        "D5-B records strict contact-after-separation");
    auto simultaneousIdentity=identity;
    simultaneousIdentity.shotIdentity="d5b.simultaneous";
    auto simultaneousTip=tip;
    simultaneousTip.shotIdentity=simultaneousIdentity.shotIdentity;
    auto simultaneousTick=orderedTick;
    auto simultaneousContact=simultaneousTick.contacts[0];
    simultaneousContact.secondBall=2;
    simultaneousContact.solverEventId=1;
    simultaneousTick.contacts.insert(simultaneousTick.contacts.begin(),
        simultaneousContact);
    billiardgl::CanonicalD5BShotObserver simultaneousObserver;
    expect(simultaneousObserver.begin(simultaneousIdentity)&&
        simultaneousObserver.bindTipObservation(simultaneousTip)&&
        simultaneousObserver.consumeSuccessfulPhysicsTick(
            simultaneousTick,1,0.0)&&simultaneousObserver.finish(true).complete&&
        simultaneousObserver.cueObjectObservation().contacts.size()==2&&
        simultaneousObserver.cueObjectObservation().contacts[0].objectBallId==1&&
        simultaneousObserver.cueObjectObservation().contacts[1].objectBallId==2,
        "simultaneous cue-object contacts retain deterministic solver-event order");
    auto noContactIdentity=identity;noContactIdentity.shotIdentity="d5b.none";
    auto noContactTip=tip;noContactTip.shotIdentity=noContactIdentity.shotIdentity;
    billiardgl::CanonicalD5BShotObserver noContactObserver;
    billiardgl::PhysicsStepTelemetry emptyTick;
    emptyTick.wholeTickDurationSeconds=0.01;
    expect(noContactObserver.begin(noContactIdentity)&&
        noContactObserver.bindTipObservation(noContactTip)&&
        noContactObserver.consumeSuccessfulPhysicsTick(emptyTick,1,0.0)&&
        noContactObserver.finish(true).complete&&
        noContactObserver.cueObjectObservation().order==
            billiardgl::CanonicalCueObjectTipOrder::NoCueObjectContact,
        "complete empty solver observation records no cue-object contact");

    billiardgl::CanonicalFrozenCushionBinding binding;
    binding.declarationDecisionId="decl";binding.declaredObjectBallId=1;
    binding.namedStraightCushionFeatureId=0;
    binding.declarationParentStateHash="parent";
    binding.consumedShotIdentity=identity.shotIdentity;
    binding.planeObservationId="departure";
    binding.authenticated=true;binding.committedAndConsumed=true;
    billiardgl::CanonicalD5BShotObserver frozenObserver;
    expect(frozenObserver.begin(identity,&binding)&&
        frozenObserver.bindTipObservation(tip),
        "signed declaration starts frozen lifecycle observer");
    billiardgl::PhysicsStepTelemetry frozenTick;
    frozenTick.wholeTickDurationSeconds=0.01;
    billiardgl::PhysicsPlaneCrossingRecord departure;
    departure.observationId="departure";departure.ballIndex=1;
    departure.wholeTickTimeOfImpactSeconds=0.001;
    departure.complete=true;frozenTick.planeCrossings.push_back(departure);
    billiardgl::PhysicsPlaneObservationAttestation departureAttestation;
    departureAttestation.observationId="departure";
    departureAttestation.ballIndex=1;
    departureAttestation.wholeTickDurationSeconds=0.01;
    departureAttestation.coveredSegmentCount=1;
    departureAttestation.crossingRecordCount=1;
    departureAttestation.complete=true;
    frozenTick.planeAttestations.push_back(departureAttestation);
    billiardgl::PhysicsContactRecord namedRail;
    namedRail.kind=billiardgl::PhysicsContactKind::Rail;
    namedRail.firstBall=1;namedRail.boundaryFeatureId=0;
    namedRail.velocityImpulseApplied=true;namedRail.solverEventId=1;
    namedRail.wholeTickTimeOfImpactSeconds=0.002;
    frozenTick.contacts.push_back(namedRail);
    auto otherRail=namedRail;otherRail.firstBall=2;
    otherRail.boundaryFeatureId=3;otherRail.solverEventId=2;
    otherRail.wholeTickTimeOfImpactSeconds=0.003;
    frozenTick.contacts.push_back(otherRail);
    billiardgl::PhysicsContactRecord objectPocket;
    objectPocket.kind=billiardgl::PhysicsContactKind::Pocket;
    objectPocket.firstBall=3;objectPocket.pocketId=2;
    objectPocket.pocketBoundaryEvent=billiardgl::PocketBoundaryEventKind::Capture;
    objectPocket.pocketCaptureSequence=4;
    objectPocket.wholeTickTimeOfImpactSeconds=0.004;
    frozenTick.contacts.push_back(objectPocket);
    auto jaw=objectPocket;jaw.firstBall=4;
    jaw.pocketBoundaryEvent=billiardgl::PocketBoundaryEventKind::LeftJaw;
    jaw.pocketCaptureSequence=0;frozenTick.contacts.push_back(jaw);
    expect(frozenObserver.consumeSuccessfulPhysicsTick(frozenTick,1,0.0)&&
        frozenObserver.finish(true).complete&&
        frozenObserver.frozenObservation().departureObserved&&
        frozenObserver.frozenObservation().sameNamedCushionRecontactAfterDeparture&&
        frozenObserver.frozenObservation().otherObjectStraightCushionBalls==
            std::vector<int>{2}&&
        frozenObserver.frozenObservation().objectPocketedBalls==
            std::vector<int>{3},
        "D5-B freezes departure/recontact and straight-rail/pocket alternatives while excluding jaw");
    auto staleBinding=binding;
    staleBinding.consumedShotIdentity="wrong.shot";
    billiardgl::CanonicalD5BShotObserver staleObserver;
    expect(!staleObserver.begin(identity,&staleBinding),
        "stale declaration consumed by a different shot cannot start lifecycle");
    auto geometryBinding=binding;geometryBinding.authenticated=false;
    billiardgl::CanonicalD5BShotObserver geometryObserver;
    expect(!geometryObserver.begin(identity,&geometryBinding),
        "geometry-only frozen relation cannot publish complete lifecycle");
    auto noDepartureIdentity=identity;
    noDepartureIdentity.shotIdentity="d5b.no_departure";
    auto noDepartureTip=tip;
    noDepartureTip.shotIdentity=noDepartureIdentity.shotIdentity;
    auto noDepartureBinding=binding;
    noDepartureBinding.consumedShotIdentity=noDepartureIdentity.shotIdentity;
    billiardgl::CanonicalD5BShotObserver noDepartureObserver;
    auto recontactOnly=frozenTick;
    recontactOnly.planeCrossings.clear();
    recontactOnly.planeAttestations[0].crossingRecordCount=0;
    expect(noDepartureObserver.begin(noDepartureIdentity,&noDepartureBinding)&&
        noDepartureObserver.bindTipObservation(noDepartureTip)&&
        noDepartureObserver.consumeSuccessfulPhysicsTick(
            recontactOnly,1,0.0)&&noDepartureObserver.finish(true).complete&&
        !noDepartureObserver.frozenObservation().departureObserved&&
        !noDepartureObserver.frozenObservation().
            sameNamedCushionRecontactAfterDeparture,
        "named-cushion recontact before departure never satisfies lifecycle");
    billiardgl::CanonicalD5BShotObserver failedObserver;
    expect(failedObserver.begin(identity)&&failedObserver.bindTipObservation(tip),
        "failed tick fixture begins staged observer");
    auto failedTick=orderedTick;
    failedTick.stepStatus=billiardgl::PhysicsStepStatus::Failed;
    expect(!failedObserver.consumeSuccessfulPhysicsTick(failedTick,1,0.0)&&
        !failedObserver.finish(true).complete,
        "failed physics tick makes the staged whole-shot observation incomplete");
    billiardgl::CanonicalD5BShotObserver gapObserver;
    expect(gapObserver.begin(identity)&&gapObserver.bindTipObservation(tip)&&
        gapObserver.consumeSuccessfulPhysicsTick(orderedTick,1,0.0)&&
        !gapObserver.consumeSuccessfulPhysicsTick(orderedTick,3,0.02)&&
        !gapObserver.finish(true).complete,
        "gapped successful tick sequence fails closed");
    billiardgl::GameState before;
    billiardgl::initializeBalls(before);
    for (int i = 1; i < billiardgl::kBallCount; ++i) {
        before.balls[i].pocketed = true;
    }
    billiardgl::setBallVelocity(before.balls[0], 100.0f, 0.0f, 0.0f);
    before.balls[0].speed = 100.0f;
    before.balls[0].angularVelocity = billiardgl::Point3{0.0f, 2.0f, 0.0f};
    before.balls[0].motionState = billiardgl::BallMotionState::Sliding;

    billiardgl::GameState after = before;
    after.balls[0].position.x += 10.0f;
    after.balls[0].velocity.x = 96.0f;
    after.balls[0].speed = 96.0f;
    after.balls[0].angularVelocity.z = 4.0f / billiardgl::kBallRadius;
    after.balls[0].motionState = billiardgl::BallMotionState::Sliding;

    billiardgl::PhysicsStepTelemetry step;
    billiardgl::SurfaceMotionStep transition;
    transition.ballIndex = 0;
    transition.before = billiardgl::BallMotionState::Sliding;
    transition.after = billiardgl::BallMotionState::Rolling;
    transition.transitionTimeSeconds = 0.05f;
    step.surfaceMotion.push_back(transition);
    billiardgl::PhysicsContactRecord contact;
    contact.kind = billiardgl::PhysicsContactKind::BallBall;
    contact.regime = billiardgl::BallBallContactRegime::Slip;
    contact.velocityImpulseApplied = true;
    contact.relativeContactVelocityBeforeCmS = billiardgl::Point3{-100.0f, 0.0f, 20.0f};
    contact.relativeContactVelocityAfterCmS = billiardgl::Point3{90.0f, 0.0f, 5.0f};
    contact.normalRelativeSpeedBeforeCmS = -100.0;
    contact.normalRelativeSpeedAfterCmS = 90.0;
    contact.tangentialImpulseNs = 0.01;
    contact.frictionCoefficient = 0.2;
    contact.kineticEnergyBeforeJ = 1.0;
    contact.kineticEnergyAfterJ = 0.9;
    contact.firstPositionCorrectionCm = billiardgl::Point3{-0.01f, 0.0f, 0.0f};
    contact.secondPositionCorrectionCm = billiardgl::Point3{0.01f, 0.0f, 0.0f};
    step.contacts.push_back(contact);
    billiardgl::SolverEventRecord solverEvent;
    solverEvent.eventId = 3;
    solverEvent.islandId = 1;
    solverEvent.contactCount = 2;
    solverEvent.velocityIterations = 4;
    solverEvent.maximumResidualCmS = 0.0001;
    step.solverEvents.push_back(solverEvent);
    step.stepStatus = billiardgl::PhysicsStepStatus::Failed;
    step.failureCode = billiardgl::PhysicsFailureCode::ResidualLimit;
    step.failingEventId = 3;
    step.failingIslandId = 1;
    const billiardgl::PhysicsFrame frame =
        billiardgl::capturePhysicsFrame(
            7, 0.7, 0.1f, before, after, step,
            billiardgl::defaultChinesePoolPhysicsProfile(),
            billiardgl::PhysicsBoundaryMode::Unbounded);

    expect(frame.tick == 7 && close(frame.timeSeconds, 0.7), "tick and time");
    expect(frame.stepStatus == billiardgl::PhysicsStepStatus::Failed &&
        frame.failureCode == billiardgl::PhysicsFailureCode::ResidualLimit &&
        frame.failingEventId == 3 && frame.failingIslandId == 1,
        "step failure identity survives frame capture");
    expect(frame.boundaryMode == billiardgl::PhysicsBoundaryMode::Unbounded,
        "frame capture should retain the apparatus boundary identity");
    expect(close(frame.balls[0].acceleration.x, -40.0), "acceleration derives from velocity delta");
    expect(close(frame.balls[0].angularVelocity.y, 2.0), "angular velocity is authoritative state");
    expect(frame.balls[0].motionState == billiardgl::BallMotionState::Sliding,
        "explicit motion state is traced");
    expect(close(frame.balls[0].contactSlipSpeedCmS, 100.0),
        "contact slip speed is traced");
    expect(close(frame.control.shotPower, before.input.shotPower), "available shot input is recorded");
    expect(close(frame.linearMomentum.x, 0.17 * 0.96), "momentum uses SI units");
    expect(close(frame.translationalKineticEnergyJ, 0.5 * 0.17 * 0.96 * 0.96), "energy uses SI units");
    expect(frame.rotationalKineticEnergyJ > 0.0,
        "rotational kinetic energy is included");
    expect(frame.balls[0].rotationalKineticEnergyJ > 0.0,
        "per-ball rotational kinetic energy is traced");
    expect(close(frame.totalKineticEnergyJ,
        frame.translationalKineticEnergyJ + frame.rotationalKineticEnergyJ),
        "total energy has one definition");
    expect(frame.surfaceTransitions.size() == 1 &&
        frame.surfaceTransitions[0].transitionTimeSeconds == 0.05f,
        "surface transition records survive frame capture");
    expect(frame.contacts.size() == 1 &&
        frame.contacts[0].regime == billiardgl::BallBallContactRegime::Slip &&
        frame.contacts[0].velocityImpulseApplied &&
        close(frame.contacts[0].relativeContactVelocityBeforeCmS.x, -100.0) &&
        close(frame.contacts[0].kineticEnergyAfterJ, 0.9),
        "ball contact diagnostics survive frame capture without reconstruction");
    expect(frame.solverEvents.size() == 1 &&
        frame.solverEvents[0].eventId == 3 &&
        frame.solverEvents[0].contactCount == 2,
        "solver event diagnostics survive frame capture without reconstruction");

    billiardgl::PhysicsContactRecord railContact;
    railContact.kind = billiardgl::PhysicsContactKind::Rail;
    railContact.cushionRegime = billiardgl::CushionContactRegime::Slip;
    railContact.cushionContactArmCm = billiardgl::Point3{2.625f, 1.05f, 0.0f};
    railContact.cushionContactHeightCm = 3.675;
    railContact.contactTangent = billiardgl::Point3{0.0f, 0.0f, -1.0f};
    railContact.cushionContactVelocityBeforeCmS = billiardgl::Point3{-100.0f, 0.0f, 20.0f};
    railContact.cushionContactVelocityAfterCmS = billiardgl::Point3{80.0f, 0.0f, 5.0f};
    railContact.impulseOnBallNs = billiardgl::Point3{-0.3f, 0.0f, -0.02f};
    railContact.positionCorrectionCm = billiardgl::Point3{-0.01f, 0.0f, 0.0f};
    railContact.restitution = 0.8;
    railContact.noseHeightRatio = 1.4;
    railContact.incidentSpeedCmS = 100.0;
    railContact.maximumRigidIncidentSpeedCmS = 250.0;
    railContact.rigidDomainExceeded = false;
    railContact.positionCorrected = true;
    railContact.timeOfImpactSeconds = 0.004;
    railContact.wholeTickTimeOfImpactSeconds = 0.014;
    railContact.firstPositionAtContactCm =
        billiardgl::Point3{10.0f, 92.0f, -20.0f};
    step.contacts.clear();
    step.contacts.push_back(railContact);
    const billiardgl::PhysicsFrame railFrame =
        billiardgl::capturePhysicsFrame(8, 0.8, 0.1f, before, after, step);
    expect(railFrame.contacts.size() == 1 &&
        railFrame.contacts[0].cushionRegime == billiardgl::CushionContactRegime::Slip &&
        close(railFrame.contacts[0].cushionContactHeightCm, 3.675) &&
        close(railFrame.contacts[0].timeOfImpactSeconds, 0.004) &&
        close(railFrame.contacts[0].wholeTickTimeOfImpactSeconds, 0.014) &&
        close(railFrame.contacts[0].firstPositionAtContactCm.z, -20.0),
        "cushion and whole-tick contact provenance survive frame capture without reconstruction");

    billiardgl::PhysicsContactRecord pocketContact;
    pocketContact.kind = billiardgl::PhysicsContactKind::Pocket;
    pocketContact.pocketId = 4;
    pocketContact.pocketKind = billiardgl::PocketKind::Side;
    pocketContact.pocketBoundaryEvent = billiardgl::PocketBoundaryEventKind::Capture;
    pocketContact.pocketPhaseBefore = billiardgl::PocketInteractionPhase::ThroatCrossed;
    pocketContact.pocketPhaseAfter = billiardgl::PocketInteractionPhase::Captured;
    pocketContact.pocketLocal.depthCm = 6.0;
    pocketContact.pocketLocal.offsetCm = 0.25;
    pocketContact.pocketJawRadiusCm = 0.8;
    pocketContact.pocketPassable = true;
    pocketContact.pocketCaptureSequence = 9;
    step.contacts.clear();
    step.contacts.push_back(pocketContact);
    const billiardgl::PhysicsFrame pocketFrame =
        billiardgl::capturePhysicsFrame(9, 0.9, 0.1f, before, after, step);
    expect(pocketFrame.contacts[0].pocketId == 4 &&
        pocketFrame.contacts[0].pocketPhaseAfter ==
            billiardgl::PocketInteractionPhase::Captured &&
        pocketFrame.contacts[0].pocketCaptureSequence == 9,
        "pocket geometry and transition diagnostics survive frame capture");

    billiardgl::PhysicsTrace trace(2);
    trace.append(frame);
    trace.append(frame);
    trace.append(frame);
    expect(trace.frames().size() == 2 && trace.droppedFrames() == 1,
        "bounded trace reports drops");

    billiardgl::PhysicsTrace disabled(0);
    disabled.append(frame);
    expect(disabled.frames().empty() && disabled.droppedFrames() == 1,
        "zero-capacity trace drops every frame");
    disabled.clear();
    expect(disabled.droppedFrames() == 0, "clear resets dropped count");

    billiardgl::GameRuntime cueRuntime;
    cueRuntime.setPhysicsTraceEnabled(true);
    const billiardgl::CueImpactInput cue = billiardgl::cueImpactFromShotControls(
        0.0f, 40.0f, cueRuntime.physicsProfile());
    expect(cueRuntime.applyCueImpact(cue).ok &&
        cueRuntime.canonicalCueTipStrokeObservationActive() &&
        !cueRuntime.hasCanonicalCueTipStrokeObservation(),
        "accepted launch stages but does not publish a partial stroke observation");
    const std::string strokeIdentity=
        cueRuntime.activeCanonicalCueTipStrokeIdentity();
    expect(!cueRuntime.commitAuthoritativeCueMotionObservationEnd(
            "wrong.stroke",cueRuntime.tick()).ok &&
        !cueRuntime.hasCanonicalCueTipStrokeObservation(),
        "wrong stroke identity cannot publish staged intervals");
    expect(cueRuntime.commitAuthoritativeCueMotionObservationEnd(
            strokeIdentity,cueRuntime.tick()).ok &&
        !cueRuntime.hasCanonicalCueTipStrokeObservation() &&
        cueRuntime.canonicalCueTipStrokeObservation().fullStrokeComplete &&
        !cueRuntime.canonicalCueTipStrokeObservation().digest.empty() &&
        cueRuntime.step(2).ok,
        "complete observation publishes only after a successful physics tick");
    expect(cueRuntime.hasCanonicalCueTipStrokeObservation(),
        "successful physics tick commits runtime observation readback");
    expect(cueRuntime.physicsTrace().frames()[0].hasCueContactResult &&
        cueRuntime.physicsTrace().frames()[0].cueContactResult.applied &&
        cueRuntime.physicsTrace().frames()[0].hasCanonicalCueTipStrokeObservation &&
        cueRuntime.physicsTrace().frames()[0].canonicalCueTipStrokeObservation.digest==
            cueRuntime.canonicalCueTipStrokeObservation().digest,
        "first post-shot frame contains the immutable complete observation");
    expect(!cueRuntime.physicsTrace().frames()[1].hasCueContactResult,
        "cue contact is an event and should not repeat on later frames");
    expect(!cueRuntime.physicsTrace().frames()[1].hasCanonicalCueTipStrokeObservation,
        "complete stroke observation is an event and does not repeat");
    const billiardgl::CueContactResult& cueResult =
        cueRuntime.physicsTrace().frames()[0].cueContactResult;
    expect(std::isfinite(cueResult.normalImpulseNs) &&
        std::isfinite(cueResult.tangentialImpulseNs) &&
        std::isfinite(cueResult.inputKineticEnergyJ) &&
        std::isfinite(cueResult.outputKineticEnergyJ),
        "cue contact impulse and energy telemetry should be finite");
    return 0;
}
