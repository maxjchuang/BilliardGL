#include "rules/table_departure_observer_adapter.h"

#include <cmath>
#include <iostream>
#include <stdexcept>

namespace {

using namespace billiardgl;
using namespace billiardgl::rules;

void require(bool condition, const char* message)
{
    if (!condition) throw std::runtime_error(message);
}

BallState ballAt(float x, float y, float z, float vx, float vy, float vz,
    PhysicalBallDomain domain)
{
    BallState ball;
    ball.position = {x, y, z};
    ball.velocity = {vx, vy, vz};
    ball.speed = std::sqrt(vx * vx + vy * vy + vz * vz);
    ball.physicalDomain = domain;
    return ball;
}

GameState stateWithBall(const BallState& ball)
{
    GameState state;
    state.balls[0] = ball;
    return state;
}

TableDepartureObserverAdapterContext contextFor(
    const TableDepartureStepResult& producer, const PhysicsProfile& profile,
    std::uint64_t tick, double duration,
    TableDepartureClothReturnProvenance clothProvenance =
        TableDepartureClothReturnProvenance::DirectAirborneLanding,
    int railFeature = 7)
{
    TableDepartureObserverAdapterContext context;
    context.physicsTick = tick;
    context.wholeTickDurationSeconds = duration;
    context.solverProfileId = profile.id;
    context.boundaryMode = PhysicsBoundaryMode::ProductionTable;
    context.successfulStep = true;
    context.complete = true;
    for (std::size_t index = 0; index < producer.events.size(); ++index) {
        const auto& event = producer.events[index];
        TableDepartureProducerEventBinding binding;
        binding.sourceOrdinal = static_cast<int>(index);
        binding.sourceIdentity = event.stableIdentity;
        binding.canonicalSolverEventId = 100 + static_cast<int>(index);
        if (event.kind == TableDepartureEventKind::OuterDeparture)
            binding.boundaryFeatureId = 2;
        if (event.kind == TableDepartureEventKind::RailTopLanding)
            binding.railFeatureId = railFeature;
        if (event.kind == TableDepartureEventKind::OutsideLanding)
            binding.landingSurfaceId = 9;
        if (event.kind == TableDepartureEventKind::ClothReturn) {
            binding.clothReturnProvenance = clothProvenance;
            binding.railFeatureId = clothProvenance ==
                    TableDepartureClothReturnProvenance::RailTopLanding ?
                railFeature : -1;
        }
        binding.complete = true;
        context.orderedEventBindings.push_back(binding);
    }
    return context;
}

CanonicalTableDepartureObserverIdentity observerIdentity()
{
    CanonicalTableDepartureObserverIdentity identity;
    identity.shotIdentity = "adapter-real-producer-shot";
    identity.rackIdentity = "adapter-real-producer-rack";
    identity.solverProfileId = defaultChinesePoolPhysicsProfile().id;
    identity.boundaryMode = PhysicsBoundaryMode::ProductionTable;
    identity.firstPhysicsTick = 1;
    return identity;
}

void testRealDirectReturnAndNoseExclusion()
{
    const PhysicsProfile profile = defaultChinesePoolPhysicsProfile();
    const TableDepartureGeometry geometry = buildTableDepartureGeometry(profile);

    BallState direct = ballAt(20.0f,
        geometry.clothSupportedCenterYCm + 3.0f, 10.0f,
        12.0f, -20.0f, 4.0f, PhysicalBallDomain::Airborne);
    GameState before = stateWithBall(direct);
    const auto producer = advanceTableDepartureMotion(direct, .2f, profile);
    GameState after = stateWithBall(direct);
    require(producer.ok && producer.events.size() == 1 &&
            producer.events[0].kind == TableDepartureEventKind::ClothReturn,
        "real producer direct cloth fixture");
    const auto adapted = adaptTableDepartureProducerTick(producer, before,
        after, profile, contextFor(producer, profile, 1, .2));
    require(adapted.complete && adapted.tick.events.size() == 1 &&
            adapted.tick.events[0].kind ==
                CanonicalTableDepartureEventKind::ClothReturn &&
            adapted.tick.events[0].railFeatureId == -1 &&
            adapted.tick.attestation.privatePhysicsEventCount == 1 &&
            adapted.tick.attestation.excludedPhysicsDiagnosticEventCount == 0,
        "real direct cloth maps without rail provenance");
    CanonicalTableDepartureObserver observer;
    require(observer.begin(observerIdentity()) &&
            observer.consumeSuccessfulPhysicsTick(adapted.tick) &&
            observer.finish(true).complete &&
            observer.observation().balls.front().directClothLandingObserved &&
            !observer.observation().balls.front().returnedOverRailTop,
        "real direct cloth reaches observer");

    BallState nose = ballAt(55.0f,
        geometry.clothSupportedCenterYCm + .2f, 30.0f,
        400.0f, 0.0f, 0.0f, PhysicalBallDomain::Airborne);
    before = stateWithBall(nose);
    const auto noseProducer = advanceTableDepartureMotion(nose, .1f, profile);
    after = stateWithBall(nose);
    require(noseProducer.ok && !noseProducer.events.empty() &&
            noseProducer.events.front().kind ==
                TableDepartureEventKind::CushionNoseContact,
        "real producer nose fixture");
    const auto noseAdapted = adaptTableDepartureProducerTick(noseProducer,
        before, after, profile,
        contextFor(noseProducer, profile, 1, .1));
    require(noseAdapted.complete &&
            noseAdapted.tick.attestation.privatePhysicsEventCount ==
                noseProducer.events.size() &&
            noseAdapted.tick.attestation.excludedPhysicsDiagnosticEventCount ==
                1 &&
            noseAdapted.tick.attestation.orderedSourceReceipts.front().
                kind == CanonicalTableDeparturePrivateEventKind::
                    NoseContactDiagnostic &&
            noseAdapted.tick.attestation.orderedSourceReceipts.front().
                disposition ==
                    CanonicalTableDepartureProjectionDisposition::
                        ExcludedDiagnostic,
        "real nose retained as typed exclusion receipt");
}

void testRealRailLandingAndReturn()
{
    const PhysicsProfile profile = defaultChinesePoolPhysicsProfile();
    const TableDepartureGeometry geometry = buildTableDepartureGeometry(profile);
    BallState ball = ballAt(70.0f,
        geometry.railTopSupportedCenterYCm + .1f, 30.0f,
        -20.0f, -5.0f, 0.0f, PhysicalBallDomain::Airborne);
    GameState before = stateWithBall(ball);
    const auto landing = advanceTableDepartureMotion(ball, .02f, profile);
    GameState after = stateWithBall(ball);
    require(landing.ok && landing.events.size() == 1 &&
            landing.events[0].kind ==
                TableDepartureEventKind::RailTopLanding,
        "real producer rail landing fixture");
    const auto landingTick = adaptTableDepartureProducerTick(landing, before,
        after, profile, contextFor(landing, profile, 1, .02,
            TableDepartureClothReturnProvenance::RailTopLanding));
    require(landingTick.complete, "adapt real rail landing");

    before = after;
    const auto returning = advanceTableDepartureMotion(ball, .5f, profile);
    after = stateWithBall(ball);
    require(returning.ok && returning.events.size() == 1 &&
            returning.events[0].kind == TableDepartureEventKind::ClothReturn,
        "real producer rail return fixture");
    const auto returnTick = adaptTableDepartureProducerTick(returning, before,
        after, profile, contextFor(returning, profile, 2, .5,
            TableDepartureClothReturnProvenance::RailTopLanding));
    require(returnTick.complete &&
            returnTick.tick.events[0].railFeatureId == 7,
        "adapt real rail return with explicit provenance");

    CanonicalTableDepartureObserver observer;
    require(observer.begin(observerIdentity()) &&
            observer.consumeSuccessfulPhysicsTick(landingTick.tick) &&
            observer.consumeSuccessfulPhysicsTick(returnTick.tick) &&
            observer.finish(true).complete &&
            observer.observation().balls.front().returnedOverRailTop,
        "real rail lifecycle reaches observer");
}

void testRealOutsideAndTypedRestGap()
{
    const PhysicsProfile profile = defaultChinesePoolPhysicsProfile();
    const TableDepartureGeometry geometry = buildTableDepartureGeometry(profile);
    BallState ball = ballAt(geometry.outerHalfWidthCm - 2.0f,
        100.0f, 30.0f, 50.0f, 5.0f, 0.0f,
        PhysicalBallDomain::Airborne);
    GameState before = stateWithBall(ball);
    const auto outside = advanceTableDepartureMotion(ball, .6f, profile);
    GameState after = stateWithBall(ball);
    require(outside.ok && outside.events.size() == 2 &&
            outside.events[0].kind == TableDepartureEventKind::OuterDeparture &&
            outside.events[1].kind == TableDepartureEventKind::OutsideLanding,
        "real producer outside fixture");
    auto outsideContext = contextFor(outside, profile, 1, .6);
    outsideContext.orderedEventBindings[0].canonicalSolverEventId = -1;
    const auto outsideTick = adaptTableDepartureProducerTick(outside, before,
        after, profile, outsideContext);
    require(outsideTick.complete && outsideTick.tick.events.size() == 2,
        "real outside departure and landing enter canonical tick");
    auto invalidOuterContext = outsideContext;
    invalidOuterContext.orderedEventBindings[0].canonicalSolverEventId = -2;
    require(!adaptTableDepartureProducerTick(outside, before, after, profile,
                invalidOuterContext).complete,
        "outer departure rejects impossible solver ids below native sentinel");
    CanonicalTableDepartureObserver observer;
    require(observer.begin(observerIdentity()) &&
            observer.consumeSuccessfulPhysicsTick(outsideTick.tick) &&
            !observer.finish(true).complete,
        "outside observation remains incomplete before physical rest");

    before = after;
    const auto resting = advanceTableDepartureMotion(ball, 5.0f, profile);
    after = stateWithBall(ball);
    require(resting.ok && resting.events.empty() && ball.speed == 0.0f,
        "real producer outside rest analytic fixture");
    auto restContext = contextFor(resting, profile, 2, 5.0);
    restContext.activeOutsideLandingSurfaceIds[0] = 9;
    const auto restTick = adaptTableDepartureProducerTick(resting, before,
        after, profile, restContext);
    require(restTick.complete && restTick.tick.events.size() == 1 &&
            restTick.tick.events[0].kind ==
                CanonicalTableDepartureEventKind::OutsideRest &&
            restTick.tick.events[0].solverEventId == -1 &&
            restTick.tick.attestation.privatePhysicsEventCount == 0 &&
            restTick.tick.attestation.analyticTransitionSourceCount == 1 &&
            restTick.tick.attestation.orderedSourceReceipts.size() == 1 &&
            restTick.tick.attestation.orderedSourceReceipts[0].sourceOrigin ==
                CanonicalTableDepartureSourceOrigin::AnalyticTransition,
        "analytic rest is a distinct typed source, never a private event");
    require(observer.begin(observerIdentity()) == false,
        "incomplete observer cannot be silently restarted");
    require(observer.consumeSuccessfulPhysicsTick(restTick.tick),
        "observer consumes typed analytic rest tick");
    require(observer.finish(true).complete,
        "real outside landing and analytic rest complete observer lifecycle");
    require(observer.observation().balls.front().cameToRestOffTable,
        "real outside rest measurement is retained");

    auto malformedRest = resting;
    for (auto& segment : malformedRest.analyticSegments) {
        if (segment.motionState == BallMotionState::Stationary)
            segment.startVelocityCmS.x = .1f;
    }
    const auto malformedResult = adaptTableDepartureProducerTick(
        malformedRest, before, after, profile, restContext);
    require(!malformedResult.complete && malformedResult.tick.events.empty(),
        "malformed analytic stop state fails without partial tick");

    auto brokenPartition = resting;
    for (auto& segment : brokenPartition.analyticSegments) {
        if (segment.motionState == BallMotionState::Stationary)
            segment.wholeTickStartOffsetSeconds += .01;
    }
    const auto brokenResult = adaptTableDepartureProducerTick(
        brokenPartition, before, after, profile, restContext);
    require(!brokenResult.complete && brokenResult.tick.events.empty(),
        "noncontiguous analytic partition fails without partial tick");

    auto missingSurface = restContext;
    missingSurface.activeOutsideLandingSurfaceIds[0] = -1;
    require(!adaptTableDepartureProducerTick(resting, before, after,
            profile, missingSurface).complete,
        "analytic rest requires explicit landing-surface lineage");
}

void testIncompleteBindingsFailClosed()
{
    const PhysicsProfile profile = defaultChinesePoolPhysicsProfile();
    const TableDepartureGeometry geometry = buildTableDepartureGeometry(profile);
    BallState direct = ballAt(20.0f,
        geometry.clothSupportedCenterYCm + 3.0f, 10.0f,
        12.0f, -20.0f, 4.0f, PhysicalBallDomain::Airborne);
    const GameState before = stateWithBall(direct);
    const auto producer = advanceTableDepartureMotion(direct, .2f, profile);
    const GameState after = stateWithBall(direct);
    auto context = contextFor(producer, profile, 1, .2);
    context.orderedEventBindings[0].sourceIdentity += "-forged";
    require(!adaptTableDepartureProducerTick(producer, before, after,
            profile, context).complete,
        "source identity cannot be rewritten by adapter context");

    context = contextFor(producer, profile, 1, .2);
    context.orderedEventBindings[0].sourceOrdinal = 1;
    require(!adaptTableDepartureProducerTick(producer, before, after,
            profile, context).complete,
        "source ordinal cannot be rewritten by adapter context");

    BallState pocket = ballAt(
        -profile.tableBoundary.playfieldWidthCm * .5f,
        geometry.clothSupportedCenterYCm + 1.0f,
        -profile.tableBoundary.playfieldLengthCm * .5f,
        0.0f, -20.0f, 0.0f, PhysicalBallDomain::Airborne);
    const GameState pocketBefore = stateWithBall(pocket);
    const auto pocketProducer = advanceTableDepartureMotion(
        pocket, .1f, profile);
    const GameState pocketAfter = stateWithBall(pocket);
    require(pocketProducer.ok && pocketProducer.events.size() == 1 &&
            pocketProducer.events[0].kind ==
                TableDepartureEventKind::PocketEntry,
        "real producer pocket entry fixture");
    const auto pocketGap = adaptTableDepartureProducerTick(pocketProducer,
        pocketBefore, pocketAfter, profile,
        contextFor(pocketProducer, profile, 1, .1));
    require(!pocketGap.complete && pocketGap.error ==
            "table_departure_pocket_entry_is_not_capture_evidence",
        "pocket entry cannot masquerade as pocket capture");
}

void testStationaryInitialStateIsNotRestTransition()
{
    const PhysicsProfile profile = defaultChinesePoolPhysicsProfile();
    const TableDepartureGeometry geometry = buildTableDepartureGeometry(profile);
    BallState stationary = ballAt(geometry.outerHalfWidthCm + 5.0f,
        geometry.outsideLandingSupportedCenterYCm, 30.0f,
        0.0f, 0.0f, 0.0f,
        PhysicalBallDomain::OutsideLandingSupported);
    const GameState before = stateWithBall(stationary);
    const auto producer = advanceTableDepartureMotion(
        stationary, 1.0f, profile);
    const GameState after = stateWithBall(stationary);
    auto context = contextFor(producer, profile, 1, 1.0);
    context.activeOutsideLandingSurfaceIds[0] = 9;
    const auto adapted = adaptTableDepartureProducerTick(producer, before,
        after, profile, context);
    require(adapted.complete && adapted.tick.events.empty() &&
            adapted.tick.attestation.privatePhysicsEventCount == 0 &&
            adapted.tick.attestation.analyticTransitionSourceCount == 0,
        "stationary initial outside state cannot fabricate a rest transition");
}

void testImmediateOutsideLandingToRestSource()
{
    const PhysicsProfile profile = defaultChinesePoolPhysicsProfile();
    const TableDepartureGeometry geometry = buildTableDepartureGeometry(profile);
    BallState ball = ballAt(geometry.outerHalfWidthCm + 5.0f,
        12.0f, 30.0f, 0.0f, -30.0f, 0.0f,
        PhysicalBallDomain::Airborne);
    const GameState before = stateWithBall(ball);
    const auto producer = advanceTableDepartureMotion(ball, .5f, profile);
    const GameState after = stateWithBall(ball);
    require(producer.ok && producer.events.size() == 1 &&
            producer.events[0].kind == TableDepartureEventKind::OutsideLanding,
        "real immediate outside landing fixture");
    const auto adapted = adaptTableDepartureProducerTick(producer, before,
        after, profile, contextFor(producer, profile, 1, .5));
    require(adapted.complete && adapted.tick.events.size() == 2 &&
            adapted.tick.events[0].kind ==
                CanonicalTableDepartureEventKind::OutsideLanding &&
            adapted.tick.events[1].kind ==
                CanonicalTableDepartureEventKind::OutsideRest &&
            adapted.tick.attestation.privatePhysicsEventCount == 1 &&
            adapted.tick.attestation.analyticTransitionSourceCount == 1 &&
            adapted.tick.attestation.orderedSourceReceipts[1].
                sourceAnalyticState ==
                    CanonicalTableDepartureAnalyticState::
                        OutsideLandingPostState,
        "landing post-state to stationary is a typed analytic rest source");
}

void testExactEventBoundaryWinsOverAdjacentToleranceMatches()
{
    const PhysicsProfile profile = defaultChinesePoolPhysicsProfile();
    constexpr double duration = .01;
    constexpr double eventTime = .005;
    constexpr float velocityX = 1000.0f;
    GameState before = stateWithBall(ballAt(0.0f, 10.0f, 0.0f,
        velocityX, 0.0f, 0.0f, PhysicalBallDomain::Airborne));
    GameState after = stateWithBall(ballAt(10.0f, 10.0f, 0.0f,
        velocityX, 0.0f, 0.0f, PhysicalBallDomain::ClothSupported));

    TableDepartureStepResult producer;
    producer.ok = true;
    producer.failureCode = TableDepartureFailureCode::None;
    const double boundaries[] = {0.0, .00499998, eventTime,
        .00500002, duration};
    for (int ordinal = 0; ordinal < 4; ++ordinal) {
        SurfaceMotionAnalyticSegment segment;
        segment.ballIndex = 0;
        segment.segmentOrdinal = ordinal;
        segment.motionState = BallMotionState::Sliding;
        segment.wholeTickStartOffsetSeconds = boundaries[ordinal];
        segment.durationSeconds = boundaries[ordinal + 1] -
            boundaries[ordinal];
        segment.startPositionCm = {
            static_cast<float>(velocityX * boundaries[ordinal]), 10.0f, 0.0f};
        segment.endPositionCm = {
            static_cast<float>(velocityX * boundaries[ordinal + 1]),
            10.0f, 0.0f};
        segment.startVelocityCmS = {velocityX, 0.0f, 0.0f};
        segment.constantAccelerationCmS2 = {};
        segment.analyticPathComplete = true;
        producer.analyticSegments.push_back(segment);
    }
    TableDepartureEvent event;
    event.kind = TableDepartureEventKind::ClothReturn;
    event.ballIndex = 0;
    event.ordinal = 0;
    event.solverEventId = 100;
    event.clothReturnProvenance =
        TableDepartureClothReturnProvenance::DirectAirborneLanding;
    event.absoluteWholeTickTimeSeconds = eventTime;
    event.stableIdentity = "exact-boundary-among-nearby-short-segments";
    event.transitionWitnessComplete = true;
    event.velocityAfterCmS = {velocityX, 0.0f, 0.0f};
    event.sourceDomain = PhysicalBallDomain::Airborne;
    event.targetDomain = PhysicalBallDomain::ClothSupported;
    producer.events.push_back(event);

    const auto adapted = adaptTableDepartureProducerTick(producer, before,
        after, profile, contextFor(producer, profile, 1, duration));
    require(adapted.complete && adapted.tick.events.size() == 1 &&
            adapted.tick.events[0].positionCm.x == 5.0f,
        "exact event boundary selects its exact segment before tolerance fallback");

}

void testZeroDurationSupportUsesNativePreContactWitness()
{
    const PhysicsProfile profile = defaultChinesePoolPhysicsProfile();
    constexpr double duration = .01;
    GameState before = stateWithBall(ballAt(0.0f, 10.0f, 0.0f,
        100.0f, -25.0f, 0.0f, PhysicalBallDomain::Airborne));
    GameState after = stateWithBall(ballAt(1.0f, 10.0f, 0.0f,
        100.0f, 0.0f, 0.0f, PhysicalBallDomain::ClothSupported));

    TableDepartureStepResult producer;
    producer.ok = true;
    producer.failureCode = TableDepartureFailureCode::None;
    SurfaceMotionAnalyticSegment postContact;
    postContact.ballIndex = 0;
    postContact.segmentOrdinal = 0;
    postContact.motionState = BallMotionState::Sliding;
    postContact.wholeTickStartOffsetSeconds = 0.0;
    postContact.durationSeconds = duration;
    postContact.startPositionCm = {0.0f, 10.0f, 0.0f};
    postContact.endPositionCm = {1.0f, 10.0f, 0.0f};
    postContact.startVelocityCmS = {100.0f, 0.0f, 0.0f};
    postContact.analyticPathComplete = true;
    producer.analyticSegments.push_back(postContact);

    TableDepartureEvent event;
    event.kind = TableDepartureEventKind::ClothReturn;
    event.ballIndex = 0;
    event.ordinal = 0;
    event.solverEventId = 5;
    event.linkedPhysicsContactOrdinal = 83;
    event.clothReturnProvenance =
        TableDepartureClothReturnProvenance::DirectAirborneLanding;
    event.absoluteWholeTickTimeSeconds = 0.0;
    event.stableIdentity = "same-toi-contact-to-contact-cloth-return";
    event.transitionWitnessComplete = true;
    event.velocityBeforeCmS = {100.0f, -25.0f, 0.0f};
    event.velocityBeforeWitnessComplete = true;
    event.velocityAfterCmS = {100.0f, 0.0f, 0.0f};
    event.sourceDomain = PhysicalBallDomain::Airborne;
    event.targetDomain = PhysicalBallDomain::ClothSupported;
    producer.events.push_back(event);

    auto context = contextFor(producer, profile, 1, duration);
    context.orderedEventBindings[0].canonicalSolverEventId = 5;
    context.orderedEventBindings[0].physicsContactOrdinal = 83;
    context.orderedEventBindings[0].transitionPositionCm =
        postContact.startPositionCm;
    context.orderedEventBindings[0].transitionPositionComplete = true;
    const auto adapted = adaptTableDepartureProducerTick(producer, before,
        after, profile, context);
    require(adapted.complete && adapted.tick.events.size() == 1 &&
            adapted.tick.events[0].velocityBeforeCmS.y == -25.0f &&
            adapted.tick.attestation.orderedSourceReceipts.size() == 1 &&
            adapted.tick.attestation.orderedSourceReceipts[0].
                physicsContactOrdinal == 83,
        "zero-duration support binds native velocity to a real support contact");

    auto missingWitness = producer;
    missingWitness.events[0].velocityBeforeWitnessComplete = false;
    require(!adaptTableDepartureProducerTick(missingWitness, before, after,
                profile, context).complete,
        "zero-duration support rejects a missing native velocity witness");
    auto missingContact = context;
    missingContact.orderedEventBindings[0].physicsContactOrdinal = -1;
    require(!adaptTableDepartureProducerTick(producer, before, after,
                profile, missingContact).complete,
        "zero-duration support rejects an unlinked contact witness");
    auto missingSolver = context;
    missingSolver.orderedEventBindings[0].canonicalSolverEventId = -1;
    require(!adaptTableDepartureProducerTick(producer, before, after,
                profile, missingSolver).complete,
        "zero-duration support rejects an unlinked solver witness");
}

void testPartitionAllowsOnlyTypedAdministrativeResetDiscontinuity()
{
    const PhysicsProfile profile = defaultChinesePoolPhysicsProfile();
    constexpr double duration = .01;
    GameState before = stateWithBall(ballAt(0.0f, 10.0f, 0.0f,
        100.0f, 0.0f, 0.0f, PhysicalBallDomain::Airborne));
    GameState after = stateWithBall(ballAt(50.5f, 10.0f, 0.0f,
        100.0f, 0.0f, 0.0f, PhysicalBallDomain::Airborne));
    after.balls[0].trajectoryGeneration = 1;

    TableDepartureStepResult producer;
    producer.ok = true;
    producer.failureCode = TableDepartureFailureCode::None;
    SurfaceMotionAnalyticSegment first;
    first.ballIndex = 0;
    first.segmentOrdinal = 0;
    first.trajectoryGeneration = 0;
    first.motionState = BallMotionState::Sliding;
    first.wholeTickStartOffsetSeconds = 0.0;
    first.durationSeconds = .005;
    first.startPositionCm = {0.0f, 10.0f, 0.0f};
    first.endPositionCm = {.5f, 10.0f, 0.0f};
    first.startVelocityCmS = {100.0f, 0.0f, 0.0f};
    first.analyticPathComplete = true;
    auto second = first;
    second.segmentOrdinal = 1;
    second.trajectoryGeneration = 1;
    second.wholeTickStartOffsetSeconds = .005;
    second.startPositionCm = {50.0f, 10.0f, 0.0f};
    second.endPositionCm = {50.5f, 10.0f, 0.0f};
    producer.analyticSegments = {first, second};

    TableDepartureEvent event;
    event.kind = TableDepartureEventKind::OuterDeparture;
    event.ballIndex = 0;
    event.ordinal = 0;
    event.boundaryFeatureId = 2;
    event.absoluteWholeTickTimeSeconds = .002;
    event.stableIdentity = "pre-reset-departure";
    event.transitionWitnessComplete = true;
    event.velocityBeforeCmS = {100.0f, 0.0f, 0.0f};
    event.velocityBeforeWitnessComplete = true;
    event.velocityAfterCmS = {100.0f, 0.0f, 0.0f};
    event.sourceDomain = PhysicalBallDomain::Airborne;
    event.targetDomain = PhysicalBallDomain::Airborne;
    producer.events.push_back(event);

    auto context = contextFor(producer, profile, 1, duration);
    TableDepartureTrajectoryGenerationTransition reset;
    reset.ballId = 0;
    reset.physicsContactOrdinal = 4;
    reset.generationBefore = 0;
    reset.generationAfter = 1;
    reset.wholeTickTimeSeconds = .005;
    reset.resetPositionCm = second.startPositionCm;
    reset.complete = true;
    context.trajectoryGenerationTransitions.push_back(reset);
    require(adaptTableDepartureProducerTick(producer, before, after, profile,
                context).complete,
        "typed capture/reset transition owns the generation position jump");

    auto missingContact = context;
    missingContact.trajectoryGenerationTransitions[0].physicsContactOrdinal =
        -1;
    require(!adaptTableDepartureProducerTick(producer, before, after, profile,
                missingContact).complete,
        "reset position jump rejects missing capture contact authority");
    auto wrongGeneration = context;
    wrongGeneration.trajectoryGenerationTransitions[0].generationAfter = 2;
    require(!adaptTableDepartureProducerTick(producer, before, after, profile,
                wrongGeneration).complete,
        "reset position jump rejects noncontiguous generation");
    auto movedReset = context;
    movedReset.trajectoryGenerationTransitions[0].resetPositionCm.x += 1.0f;
    require(!adaptTableDepartureProducerTick(producer, before, after, profile,
                movedReset).complete,
        "reset position jump rejects a position not anchoring the new segment");
}

void testPartitionConsumesExactSolverPositionProjection()
{
    const PhysicsProfile profile = defaultChinesePoolPhysicsProfile();
    constexpr double duration = .01;
    GameState before = stateWithBall(ballAt(0.0f, 10.0f, 0.0f,
        100.0f, 0.0f, 0.0f, PhysicalBallDomain::Airborne));
    GameState after = stateWithBall(ballAt(2.0f, 10.0f, 0.0f,
        100.0f, 0.0f, 0.0f, PhysicalBallDomain::Airborne));

    TableDepartureStepResult producer;
    producer.ok = true;
    producer.failureCode = TableDepartureFailureCode::None;
    SurfaceMotionAnalyticSegment first;
    first.ballIndex = 0;
    first.segmentOrdinal = 0;
    first.motionState = BallMotionState::Sliding;
    first.wholeTickStartOffsetSeconds = 0.0;
    first.durationSeconds = .005;
    first.startPositionCm = {0.0f, 10.0f, 0.0f};
    first.endPositionCm = {.5f, 10.0f, 0.0f};
    first.startVelocityCmS = {100.0f, 0.0f, 0.0f};
    first.analyticPathComplete = true;
    auto second = first;
    second.segmentOrdinal = 1;
    second.wholeTickStartOffsetSeconds = .005;
    second.startPositionCm = {1.5f, 10.0f, 0.0f};
    second.endPositionCm = {2.0f, 10.0f, 0.0f};
    producer.analyticSegments = {first, second};

    TableDepartureEvent event;
    event.kind = TableDepartureEventKind::OuterDeparture;
    event.ballIndex = 0;
    event.ordinal = 0;
    event.boundaryFeatureId = 2;
    event.absoluteWholeTickTimeSeconds = .002;
    event.stableIdentity = "pre-projection-departure";
    event.transitionWitnessComplete = true;
    event.velocityBeforeCmS = {100.0f, 0.0f, 0.0f};
    event.velocityBeforeWitnessComplete = true;
    event.velocityAfterCmS = {100.0f, 0.0f, 0.0f};
    event.sourceDomain = PhysicalBallDomain::Airborne;
    event.targetDomain = PhysicalBallDomain::Airborne;
    producer.events.push_back(event);

    auto context = contextFor(producer, profile, 1, duration);
    TableDeparturePositionProjectionTransition projection;
    projection.ballId = 0;
    projection.solverEventId = 7;
    projection.solverIslandId = 3;
    projection.projectionOrdinal = 0;
    projection.solverIteration = 0;
    projection.solverContactIndex = 0;
    projection.trajectoryGeneration = 0;
    projection.wholeTickTimeSeconds = .005;
    projection.beforeCm = first.endPositionCm;
    projection.afterCm = second.startPositionCm;
    projection.complete = true;
    context.positionProjectionTransitions.push_back(projection);
    require(adaptTableDepartureProducerTick(producer, before, after, profile,
                context).complete,
        "typed solver projection owns same-generation position discontinuity");

    auto missing = context;
    missing.positionProjectionTransitions.clear();
    require(!adaptTableDepartureProducerTick(producer, before, after, profile,
                missing).complete,
        "same-generation position discontinuity rejects missing projection");
    auto movedBefore = context;
    movedBefore.positionProjectionTransitions[0].beforeCm.x += 1.0f;
    require(!adaptTableDepartureProducerTick(producer, before, after, profile,
                movedBefore).complete,
        "solver projection rejects a forged before position");
    auto wrongGeneration = context;
    wrongGeneration.positionProjectionTransitions[0].trajectoryGeneration = 1;
    require(!adaptTableDepartureProducerTick(producer, before, after, profile,
                wrongGeneration).complete,
        "solver projection rejects a different trajectory generation");
    auto duplicate = context;
    duplicate.positionProjectionTransitions.push_back(projection);
    require(!adaptTableDepartureProducerTick(producer, before, after, profile,
                duplicate).complete,
        "solver projection rejects duplicate mutation receipts");
}

}  // namespace

int main()
{
    try {
        testRealDirectReturnAndNoseExclusion();
        testRealRailLandingAndReturn();
        testRealOutsideAndTypedRestGap();
        testIncompleteBindingsFailClosed();
        testStationaryInitialStateIsNotRestTransition();
        testImmediateOutsideLandingToRestSource();
        testExactEventBoundaryWinsOverAdjacentToleranceMatches();
        testZeroDurationSupportUsesNativePreContactWitness();
        testPartitionAllowsOnlyTypedAdministrativeResetDiscontinuity();
        testPartitionConsumesExactSolverPositionProjection();
        std::cout << "PASS_D6_B_TABLE_DEPARTURE_OBSERVER_ADAPTER"
                     " real_producer=1 policy=0 gpu=0\n";
        return 0;
    } catch (const std::exception& failure) {
        std::cerr << "FAIL " << failure.what() << '\n';
        return 1;
    }
}
