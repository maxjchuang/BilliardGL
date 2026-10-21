#include "rules/table_departure_observer.h"

#include <cmath>
#include <iostream>
#include <stdexcept>
#include <string>
#include <vector>

namespace {

using namespace billiardgl;
using namespace billiardgl::rules;

void require(bool condition, const char* message)
{
    if (!condition) throw std::runtime_error(message);
}

CanonicalTableDepartureEvent event(CanonicalTableDepartureEventKind kind,
    int ballId, std::uint64_t tick, int ordinal, int solverEventId,
    std::uint64_t generation, double time)
{
    CanonicalTableDepartureEvent result;
    result.kind = kind;
    result.ballId = ballId;
    result.physicsTick = tick;
    result.eventOrdinal = ordinal;
    result.solverEventId = solverEventId;
    result.trajectoryGeneration = generation;
    result.wholeTickTimeSeconds = time;
    switch (kind) {
    case CanonicalTableDepartureEventKind::OuterDeparture:
        result.sourceDomain = PhysicalBallDomain::Airborne;
        result.targetDomain = PhysicalBallDomain::Airborne;
        break;
    case CanonicalTableDepartureEventKind::RailTopLanding:
        result.sourceDomain = PhysicalBallDomain::Airborne;
        result.targetDomain = PhysicalBallDomain::RailTopSupported;
        break;
    case CanonicalTableDepartureEventKind::ClothReturn:
        result.sourceDomain = PhysicalBallDomain::Airborne;
        result.targetDomain = PhysicalBallDomain::ClothSupported;
        break;
    case CanonicalTableDepartureEventKind::OutsideLanding:
        result.sourceDomain = PhysicalBallDomain::Airborne;
        result.targetDomain = PhysicalBallDomain::OutsideLandingSupported;
        break;
    case CanonicalTableDepartureEventKind::OutsideRest:
        result.sourceDomain = PhysicalBallDomain::OutsideLandingSupported;
        result.targetDomain = PhysicalBallDomain::OutsideLandingSupported;
        break;
    case CanonicalTableDepartureEventKind::PocketCapture:
        result.sourceDomain = PhysicalBallDomain::ClothSupported;
        result.targetDomain = PhysicalBallDomain::PocketChannel;
        break;
    case CanonicalTableDepartureEventKind::AdministrativeReset:
        result.sourceDomain = PhysicalBallDomain::PocketChannel;
        result.targetDomain = PhysicalBallDomain::ClothSupported;
        break;
    }
    result.positionCm = {10.0f + ordinal, 92.0f, 20.0f};
    result.velocityBeforeCmS = {12.0f, -3.0f, 4.0f};
    result.velocityAfterCmS = {8.0f, 0.0f, 3.0f};
    if (kind == CanonicalTableDepartureEventKind::OutsideRest) {
        result.solverEventId = -1;
        result.velocityBeforeCmS = {};
        result.velocityAfterCmS = {};
    }
    result.ballRadiusCm = 2.8575f;
    result.boundaryFeatureId = kind ==
        CanonicalTableDepartureEventKind::OuterDeparture ? 2 : -1;
    result.railFeatureId = kind ==
        CanonicalTableDepartureEventKind::RailTopLanding || kind ==
        CanonicalTableDepartureEventKind::ClothReturn ? 2 : -1;
    result.landingSurfaceId = kind ==
        CanonicalTableDepartureEventKind::OutsideLanding || kind ==
        CanonicalTableDepartureEventKind::OutsideRest ? 1 : -1;
    result.complete = true;
    return result;
}

CanonicalTableDepartureTick tick(std::uint64_t physicsTick,
    std::vector<CanonicalTableDepartureEvent> events)
{
    CanonicalTableDepartureTick result;
    result.physicsTick = physicsTick;
    result.wholeTickDurationSeconds = 1.0;
    result.solverProfileId = "d6_profile_v1";
    result.boundaryMode = PhysicsBoundaryMode::ProductionTable;
    result.events = std::move(events);
    result.attestation.physicsTick = physicsTick;
    result.attestation.wholeTickDurationSeconds = 1.0;
    result.attestation.solverProfileId = result.solverProfileId;
    result.attestation.boundaryMode = result.boundaryMode;
    result.attestation.sourceEventCount = result.events.size();
    result.attestation.emittedEventCount = result.events.size();
    int privateOrdinal = 0;
    for (std::size_t index = 0; index < result.events.size(); ++index) {
        const auto& source = result.events[index];
        CanonicalTableDepartureSourceReceipt receipt;
        receipt.sourceOrdinal = static_cast<int>(index);
        const bool analyticRest = source.kind ==
            CanonicalTableDepartureEventKind::OutsideRest;
        receipt.sourceOrigin = analyticRest ?
            CanonicalTableDepartureSourceOrigin::AnalyticTransition :
            CanonicalTableDepartureSourceOrigin::PrivateEvent;
        receipt.privateEventOrdinal = analyticRest ? -1 : privateOrdinal++;
        receipt.precedingSegmentOrdinal = analyticRest ?
            static_cast<int>(2 * index) : -1;
        receipt.sourceSegmentOrdinal = analyticRest ?
            static_cast<int>(2 * index + 1) : -1;
        receipt.sourceIdentity = std::string(analyticRest ?
            "analytic-source-" : "private-source-") +
            std::to_string(physicsTick) + "-" + std::to_string(index);
        receipt.kind = static_cast<CanonicalTableDeparturePrivateEventKind>(
            static_cast<int>(source.kind));
        receipt.ballId = source.ballId;
        receipt.solverEventId = source.solverEventId;
        receipt.trajectoryGeneration = source.trajectoryGeneration;
        receipt.wholeTickTimeSeconds = source.wholeTickTimeSeconds;
        if (analyticRest) {
            receipt.transitionPositionCm = source.positionCm;
            receipt.velocityBeforeCmS = {};
            receipt.velocityAfterCmS = {};
            receipt.sourceDomain =
                PhysicalBallDomain::OutsideLandingSupported;
            receipt.targetDomain =
                PhysicalBallDomain::OutsideLandingSupported;
            receipt.sourceAnalyticState =
                CanonicalTableDepartureAnalyticState::OutsideRolling;
            receipt.targetAnalyticState =
                CanonicalTableDepartureAnalyticState::OutsideStationary;
            receipt.analyticPartitionDigest = "fixture-partition-" +
                std::to_string(physicsTick) + "-" + std::to_string(index);
        } else {
            receipt.transitionPositionCm = source.positionCm;
            receipt.velocityBeforeCmS = source.velocityBeforeCmS;
            receipt.velocityAfterCmS = source.velocityAfterCmS;
            receipt.sourceDomain = source.sourceDomain;
            receipt.targetDomain = source.targetDomain;
        }
        receipt.canonicalEventOrdinal = static_cast<int>(index);
        receipt.canonicalEventCount = 1;
        receipt.canonicalEventDigest =
            canonicalTableDepartureEventDigest({source});
        receipt.complete = true;
        result.attestation.orderedSourceReceipts.push_back(receipt);
    }
    result.attestation.privatePhysicsEventCount =
        static_cast<std::size_t>(privateOrdinal);
    result.attestation.analyticTransitionSourceCount =
        result.events.size() - result.attestation.privatePhysicsEventCount;
    result.attestation.orderedSourceDigest =
        canonicalTableDepartureSourceDigest(
            result.attestation.orderedSourceReceipts);
    result.attestation.orderedEventDigest =
        canonicalTableDepartureEventDigest(result.events);
    result.attestation.successfulStep = true;
    result.attestation.canonicalOrder = true;
    result.attestation.noDroppedSamples = true;
    result.attestation.complete = true;
    return result;
}

CanonicalTableDepartureObserverIdentity identity(std::uint64_t firstTick = 1)
{
    CanonicalTableDepartureObserverIdentity result;
    result.shotIdentity = "shot-d6-b-1";
    result.rackIdentity = "rack-d6-b-1";
    result.solverProfileId = "d6_profile_v1";
    result.boundaryMode = PhysicsBoundaryMode::ProductionTable;
    result.firstPhysicsTick = firstTick;
    return result;
}

void testCueAndObjectOutsideRest()
{
    CanonicalTableDepartureObserver observer;
    require(observer.begin(identity()), "begin cue observer");
    require(observer.consumeSuccessfulPhysicsTick(tick(1, {
        event(CanonicalTableDepartureEventKind::OuterDeparture, 0, 1, 0, 7, 0, .10),
        event(CanonicalTableDepartureEventKind::OutsideLanding, 0, 1, 1, 8, 0, .40),
        event(CanonicalTableDepartureEventKind::OutsideRest, 0, 1, 2, 9, 0, .75),
    })), "consume cue outside lifecycle");
    const auto validation = observer.finish(true);
    require(validation.valid && validation.complete,
        "cue outside lifecycle complete");
    require(observer.observation().balls.size() == 1,
        "cue observation one generation");
    const auto& cue = observer.observation().balls.front();
    require(cue.ballId == 0 && cue.ballKind ==
            CanonicalTableDepartureBallKind::CueBall &&
            cue.departureObserved && cue.outsideLandingObserved &&
            cue.outsideRestObserved && cue.cameToRestOffTable &&
            !cue.returnedOverRailTop && !cue.capturedInPocket && cue.complete,
        "cue measurements exact");

    observer.rollback();
    auto objectIdentity = identity(4);
    objectIdentity.shotIdentity = "shot-d6-b-object";
    objectIdentity.initialTrajectoryGenerations[3] = 6;
    require(observer.begin(objectIdentity), "begin object observer");
    require(observer.consumeSuccessfulPhysicsTick(tick(4, {
        event(CanonicalTableDepartureEventKind::OuterDeparture, 3, 4, 0, 2, 6, .12),
    })), "consume object departure tick");
    require(observer.consumeSuccessfulPhysicsTick(tick(5, {
        event(CanonicalTableDepartureEventKind::OutsideLanding, 3, 5, 0, 3, 6, .45),
    })), "consume object landing tick");
    require(observer.consumeSuccessfulPhysicsTick(tick(6, {
        event(CanonicalTableDepartureEventKind::OutsideRest, 3, 6, 0, 4, 6, .90),
    })), "consume object rest tick");
    require(observer.finish(true).complete &&
            observer.observation().balls.front().ballKind ==
                CanonicalTableDepartureBallKind::ObjectBall &&
            observer.observation().balls.front().cameToRestOffTable &&
            observer.observation().orderedSuccessfulTickDigests.size() == 3,
        "object outside measurement complete without policy");
}

void testRailReturnExclusion()
{
    CanonicalTableDepartureObserver observer;
    require(observer.begin(identity()), "begin rail return");
    require(observer.consumeSuccessfulPhysicsTick(tick(1, {
        event(CanonicalTableDepartureEventKind::RailTopLanding, 2, 1, 0, 4, 0, .20),
        event(CanonicalTableDepartureEventKind::ClothReturn, 2, 1, 1, 5, 0, .60),
    })), "consume rail return");
    require(observer.finish(true).complete, "rail return complete");
    const auto& ball = observer.observation().balls.front();
    require(ball.returnedOverRailTop && !ball.cameToRestOffTable &&
            !ball.outsideLandingObserved && !ball.outsideRestObserved,
        "rail return excluded from outside rest");
}

void testDirectClothLandingIsNotRailReturn()
{
    CanonicalTableDepartureObserver observer;
    require(observer.begin(identity()), "begin direct cloth observer");
    auto direct = event(CanonicalTableDepartureEventKind::ClothReturn,
        5, 1, 0, 3, 0, .4);
    direct.railFeatureId = -1;
    require(observer.consumeSuccessfulPhysicsTick(tick(1, {direct})),
        "consume direct cloth landing");
    require(observer.finish(true).complete,
        "direct cloth landing is a complete exclusion");
    const auto& ball = observer.observation().balls.front();
    require(ball.directClothLandingObserved &&
            !ball.returnedOverRailTop && !ball.cameToRestOffTable,
        "direct cloth landing is not mislabeled as rail return");

    observer.rollback();
    require(observer.begin(identity()), "begin fake rail provenance observer");
    direct.railFeatureId = 2;
    require(!observer.consumeSuccessfulPhysicsTick(tick(1, {direct})),
        "direct cloth landing cannot invent rail provenance");

    auto wrongRailReturn = tick(1, {
        event(CanonicalTableDepartureEventKind::RailTopLanding,
            5, 1, 0, 4, 0, .2),
        event(CanonicalTableDepartureEventKind::ClothReturn,
            5, 1, 1, 5, 0, .4),
    });
    wrongRailReturn.events[1].railFeatureId = 3;
    wrongRailReturn.attestation.orderedEventDigest =
        canonicalTableDepartureEventDigest(wrongRailReturn.events);
    wrongRailReturn.attestation.orderedSourceReceipts[1].kind =
        CanonicalTableDeparturePrivateEventKind::ClothReturn;
    wrongRailReturn.attestation.orderedSourceReceipts[1].
        canonicalEventDigest = canonicalTableDepartureEventDigest(
            {wrongRailReturn.events[1]});
    wrongRailReturn.attestation.orderedSourceDigest =
        canonicalTableDepartureSourceDigest(
            wrongRailReturn.attestation.orderedSourceReceipts);
    require(!observer.consumeSuccessfulPhysicsTick(wrongRailReturn),
        "rail return must preserve exact rail provenance");
}

void testCuePocketResetGenerationBoundary()
{
    CanonicalTableDepartureObserver observer;
    auto scratchIdentity = identity();
    scratchIdentity.initialTrajectoryGenerations[0] = 5;
    require(observer.begin(scratchIdentity), "begin scratch observer");
    auto capture = event(CanonicalTableDepartureEventKind::PocketCapture,
        0, 1, 0, 12, 5, .25);
    capture.pocketId = 4;
    capture.pocketCaptureSequence = 17;
    auto reset = event(CanonicalTableDepartureEventKind::AdministrativeReset,
        0, 1, 1, 13, 6, .25);
    reset.previousTrajectoryGeneration = 5;
    reset.pocketId = 4;
    reset.pocketCaptureSequence = 17;
    reset.positionCm = {0.0f, 80.8575f, -63.5f};
    reset.velocityBeforeCmS = {};
    reset.velocityAfterCmS = {};
    require(observer.consumeSuccessfulPhysicsTick(tick(1, {capture, reset})),
        "consume scratch generation boundary");
    require(observer.finish(true).complete,
        "normal scratch remains a complete physical observation");
    require(observer.observation().balls.size() == 2,
        "capture and reset are separate generations");
    const auto& captured = observer.observation().balls[0];
    const auto& respawned = observer.observation().balls[1];
    require(captured.trajectoryGeneration == 5 && captured.capturedInPocket &&
            captured.complete && !captured.cameToRestOffTable &&
            respawned.trajectoryGeneration == 6 &&
            respawned.administrativeResetObserved && respawned.complete &&
            !respawned.cameToRestOffTable,
        "scratch terminal and reset generation are not concatenated");
}

void testContinuedEpisodesWithinGeneration()
{
    CanonicalTableDepartureObserver observer;
    require(observer.begin(identity()), "begin continued episode observer");
    require(observer.consumeSuccessfulPhysicsTick(tick(1, {
        event(CanonicalTableDepartureEventKind::RailTopLanding, 4, 1, 0, 1, 0, .10),
        event(CanonicalTableDepartureEventKind::ClothReturn, 4, 1, 1, 2, 0, .20),
        event(CanonicalTableDepartureEventKind::OuterDeparture, 4, 1, 2, 3, 0, .30),
        event(CanonicalTableDepartureEventKind::OutsideLanding, 4, 1, 3, 4, 0, .60),
        event(CanonicalTableDepartureEventKind::OutsideRest, 4, 1, 4, 5, 0, .90),
    })), "return then later outside lifecycle");
    require(observer.finish(true).complete,
        "later complete episode closes observation");
    const auto& ball = observer.observation().balls.front();
    require(ball.returnedOverRailTop && ball.cameToRestOffTable &&
            ball.complete,
        "early return does not mask later outside rest");

    observer.rollback();
    auto resetIdentity = identity();
    resetIdentity.shotIdentity = "shot-reset-then-depart";
    resetIdentity.initialTrajectoryGenerations[0] = 5;
    require(observer.begin(resetIdentity), "begin reset continuation");
    auto capture = event(CanonicalTableDepartureEventKind::PocketCapture,
        0, 1, 0, 10, 5, .10);
    capture.pocketId = 2;
    capture.pocketCaptureSequence = 4;
    auto reset = event(CanonicalTableDepartureEventKind::AdministrativeReset,
        0, 1, 1, 11, 6, .10);
    reset.previousTrajectoryGeneration = 5;
    reset.pocketId = 2;
    reset.pocketCaptureSequence = 4;
    reset.positionCm = {0.0f, 80.8575f, -63.5f};
    reset.velocityBeforeCmS = {};
    reset.velocityAfterCmS = {};
    require(observer.consumeSuccessfulPhysicsTick(tick(1, {
        capture, reset,
        event(CanonicalTableDepartureEventKind::OuterDeparture, 0, 1, 2, 12, 6, .30),
        event(CanonicalTableDepartureEventKind::OutsideLanding, 0, 1, 3, 13, 6, .60),
        event(CanonicalTableDepartureEventKind::OutsideRest, 0, 1, 4, 14, 6, .90),
    })), "reset generation may continue same tick");
    require(observer.finish(true).complete &&
            observer.observation().balls.size() == 2 &&
            observer.observation().balls[0].capturedInPocket &&
            observer.observation().balls[1].administrativeResetObserved &&
            observer.observation().balls[1].cameToRestOffTable,
        "reset starts active independent generation");
}

void testContiguousRetryAndRollback()
{
    CanonicalTableDepartureObserver observer;
    require(observer.begin(identity()), "begin retry observer");
    const auto first = tick(1, {
        event(CanonicalTableDepartureEventKind::OuterDeparture, 1, 1, 0, 2, 0, .2),
    });
    require(observer.consumeSuccessfulPhysicsTick(first), "consume first tick");
    const std::string before = observer.observation().digest;
    const std::size_t count = observer.observation().events.size();
    require(observer.consumeSuccessfulPhysicsTick(first) &&
            observer.observation().events.size() == count &&
            observer.observation().digest == before,
        "exact retry idempotent");

    auto conflict = first;
    conflict.events[0].positionCm.x += 0.25f;
    conflict.attestation.orderedEventDigest =
        canonicalTableDepartureEventDigest(conflict.events);
    require(!observer.consumeSuccessfulPhysicsTick(conflict) &&
            observer.observation().events.size() == count &&
            observer.observation().digest == before,
        "conflicting retry zero delta");

    require(!observer.consumeSuccessfulPhysicsTick(tick(3, {})) &&
            observer.observation().events.size() == count &&
            observer.observation().digest == before,
        "noncontiguous successful tick zero delta");
    require(!observer.finish(true).complete,
        "departure without landing and rest incomplete");
    observer.rollback();
    require(!observer.active() && observer.observation().events.empty(),
        "rollback clears incomplete observation");
}

void testFailClosedAttestationAndOrdering()
{
    CanonicalTableDepartureObserver observer;
    require(observer.begin(identity()), "begin negative observer");
    auto missing = tick(1, {});
    missing.attestation.noDroppedSamples = false;
    require(!observer.consumeSuccessfulPhysicsTick(missing) &&
            observer.observation().events.empty(),
        "dropped sample attestation rejected");

    auto droppedDiagnostic = tick(1, {});
    droppedDiagnostic.attestation.privatePhysicsEventCount = 1;
    require(!observer.consumeSuccessfulPhysicsTick(droppedDiagnostic),
        "unaccounted private diagnostic event rejected");
    auto accountedDiagnostic = tick(1, {});
    CanonicalTableDepartureSourceReceipt diagnostic;
    diagnostic.sourceOrdinal = 0;
    diagnostic.privateEventOrdinal = 0;
    diagnostic.sourceIdentity = "private-nose-contact-1";
    diagnostic.kind = CanonicalTableDeparturePrivateEventKind::
        NoseContactDiagnostic;
    diagnostic.ballId = 1;
    diagnostic.solverEventId = 31;
    diagnostic.wholeTickTimeSeconds = .25;
    diagnostic.disposition = CanonicalTableDepartureProjectionDisposition::
        ExcludedDiagnostic;
    diagnostic.exclusionReason = CanonicalTableDepartureExclusionReason::
        NoseContactNotTableDepartureMeasurement;
    diagnostic.complete = true;
    accountedDiagnostic.attestation.orderedSourceReceipts = {diagnostic};
    accountedDiagnostic.attestation.privatePhysicsEventCount = 1;
    accountedDiagnostic.attestation.excludedPhysicsDiagnosticEventCount = 1;
    accountedDiagnostic.attestation.orderedSourceDigest =
        canonicalTableDepartureSourceDigest(
            accountedDiagnostic.attestation.orderedSourceReceipts);
    require(observer.consumeSuccessfulPhysicsTick(accountedDiagnostic),
        "explicit diagnostic exclusion retained in source denominator");
    observer.rollback();
    require(observer.begin(identity()),
        "restart after diagnostic denominator fixture");

    auto eligibleDisguisedAsExcluded = tick(1, {});
    diagnostic.sourceIdentity = "private-outside-landing-hidden";
    diagnostic.kind = CanonicalTableDeparturePrivateEventKind::OutsideLanding;
    diagnostic.exclusionReason = CanonicalTableDepartureExclusionReason::
        NoseContactNotTableDepartureMeasurement;
    eligibleDisguisedAsExcluded.attestation.orderedSourceReceipts =
        {diagnostic};
    eligibleDisguisedAsExcluded.attestation.privatePhysicsEventCount = 1;
    eligibleDisguisedAsExcluded.attestation.
        excludedPhysicsDiagnosticEventCount = 1;
    eligibleDisguisedAsExcluded.attestation.orderedSourceDigest =
        canonicalTableDepartureSourceDigest(
            eligibleDisguisedAsExcluded.attestation.
                orderedSourceReceipts);
    require(!observer.consumeSuccessfulPhysicsTick(eligibleDisguisedAsExcluded),
        "eligible outside landing cannot masquerade as excluded diagnostic");

    auto projectedEligible = tick(1, {
        event(CanonicalTableDepartureEventKind::OuterDeparture,
            1, 1, 0, 42, 0, .2),
    });
    projectedEligible.attestation.orderedSourceReceipts[0].disposition =
        CanonicalTableDepartureProjectionDisposition::ExcludedDiagnostic;
    projectedEligible.attestation.orderedSourceReceipts[0].
        canonicalEventOrdinal = -1;
    projectedEligible.attestation.orderedSourceReceipts[0].
        canonicalEventDigest.clear();
    projectedEligible.attestation.orderedSourceReceipts[0].
        exclusionReason = CanonicalTableDepartureExclusionReason::
            CrossContactNotTableDepartureMeasurement;
    projectedEligible.attestation.sourceEventCount = 0;
    projectedEligible.attestation.emittedEventCount = 0;
    projectedEligible.attestation.excludedPhysicsDiagnosticEventCount = 1;
    projectedEligible.attestation.orderedSourceDigest =
        canonicalTableDepartureSourceDigest(
            projectedEligible.attestation.orderedSourceReceipts);
    require(!observer.consumeSuccessfulPhysicsTick(projectedEligible),
        "eligible departure cannot be relabeled as excluded cross contact");

    auto sourceOrdinalGap = accountedDiagnostic;
    sourceOrdinalGap.attestation.orderedSourceReceipts[0].sourceOrdinal = 1;
    sourceOrdinalGap.attestation.orderedSourceDigest =
        canonicalTableDepartureSourceDigest(
            sourceOrdinalGap.attestation.orderedSourceReceipts);
    require(!observer.consumeSuccessfulPhysicsTick(sourceOrdinalGap),
        "private source ordinals must be contiguous");

    auto sourceDigestDrift = accountedDiagnostic;
    sourceDigestDrift.attestation.orderedSourceDigest[0] =
        sourceDigestDrift.attestation.orderedSourceDigest[0] == '0' ?
            '1' : '0';
    require(!observer.consumeSuccessfulPhysicsTick(sourceDigestDrift),
        "private source digest drift rejected");

    auto unordered = tick(1, {
        event(CanonicalTableDepartureEventKind::OutsideLanding, 1, 1, 0, 2, 0, .6),
        event(CanonicalTableDepartureEventKind::OuterDeparture, 1, 1, 1, 3, 0, .2),
    });
    require(!observer.consumeSuccessfulPhysicsTick(unordered) &&
            observer.observation().events.empty(),
        "unordered events rejected");

    auto restOnly = tick(1, {
        event(CanonicalTableDepartureEventKind::OutsideRest, 1, 1, 0, 2, 0, .5),
    });
    require(!observer.consumeSuccessfulPhysicsTick(restOnly),
        "outside rest without departure and landing rejected");

    auto wrongBoundary = tick(1, {});
    wrongBoundary.boundaryMode = PhysicsBoundaryMode::Unbounded;
    wrongBoundary.attestation.boundaryMode = wrongBoundary.boundaryMode;
    require(!observer.consumeSuccessfulPhysicsTick(wrongBoundary),
        "unbounded telemetry rejected");

    auto foreignGeneration = tick(1, {
        event(CanonicalTableDepartureEventKind::OuterDeparture, 1, 1, 0, 7, 9, .2),
    });
    require(!observer.consumeSuccessfulPhysicsTick(foreignGeneration),
        "unbound trajectory generation rejected");

    auto capture = event(CanonicalTableDepartureEventKind::PocketCapture,
        0, 1, 0, 9, 0, .2);
    capture.pocketId = 1;
    capture.pocketCaptureSequence = 3;
    auto wrongReset = event(
        CanonicalTableDepartureEventKind::AdministrativeReset,
        0, 1, 1, 10, 1, .25);
    wrongReset.previousTrajectoryGeneration = 0;
    wrongReset.pocketId = 1;
    wrongReset.pocketCaptureSequence = 3;
    wrongReset.velocityBeforeCmS = {};
    wrongReset.velocityAfterCmS = {};
    require(!observer.consumeSuccessfulPhysicsTick(tick(1,
            {capture, wrongReset})),
        "reset must bind capture tick and exact TOI");
}

void testSupportedDomainPostStateInvariants()
{
    CanonicalTableDepartureObserver observer;
    require(observer.begin(identity()), "begin supported-state observer");

    auto railLanding = event(
        CanonicalTableDepartureEventKind::RailTopLanding,
        2, 1, 0, 1, 0, .2);
    railLanding.velocityAfterCmS.y = -.01f;
    require(!observer.consumeSuccessfulPhysicsTick(tick(1, {railLanding})),
        "rail-top landing rejects nonzero post-state vertical velocity");

    auto directCloth = event(CanonicalTableDepartureEventKind::ClothReturn,
        2, 1, 0, 2, 0, .2);
    directCloth.railFeatureId = -1;
    directCloth.velocityAfterCmS.y = .01f;
    require(!observer.consumeSuccessfulPhysicsTick(tick(1, {directCloth})),
        "direct cloth landing rejects nonzero post-state vertical velocity");

    auto rail = event(CanonicalTableDepartureEventKind::RailTopLanding,
        2, 1, 0, 3, 0, .1);
    auto railCloth = event(CanonicalTableDepartureEventKind::ClothReturn,
        2, 1, 1, 4, 0, .3);
    railCloth.velocityAfterCmS.y = -.02f;
    require(!observer.consumeSuccessfulPhysicsTick(tick(1,
            {rail, railCloth})),
        "rail cloth return rejects nonzero post-state vertical velocity");

    auto outsideLanding = event(
        CanonicalTableDepartureEventKind::OutsideLanding,
        2, 1, 1, 6, 0, .4);
    outsideLanding.velocityAfterCmS.y = .02f;
    require(!observer.consumeSuccessfulPhysicsTick(tick(1, {
            event(CanonicalTableDepartureEventKind::OuterDeparture,
                2, 1, 0, 5, 0, .1),
            outsideLanding,
        })),
        "outside landing rejects nonzero post-state vertical velocity");

    auto resetIdentity = identity();
    resetIdentity.initialTrajectoryGenerations[0] = 5;
    observer.rollback();
    require(observer.begin(resetIdentity), "begin stationary reset observer");
    auto capture = event(CanonicalTableDepartureEventKind::PocketCapture,
        0, 1, 0, 7, 5, .2);
    capture.pocketId = 1;
    capture.pocketCaptureSequence = 9;
    auto movingReset = event(
        CanonicalTableDepartureEventKind::AdministrativeReset,
        0, 1, 1, 8, 6, .2);
    movingReset.previousTrajectoryGeneration = 5;
    movingReset.pocketId = 1;
    movingReset.pocketCaptureSequence = 9;
    movingReset.velocityBeforeCmS = {};
    movingReset.velocityAfterCmS = {.01f, 0.0f, 0.0f};
    require(!observer.consumeSuccessfulPhysicsTick(tick(1,
            {capture, movingReset})),
        "administrative reset requires a stationary post-reset state");
}

void testTypedAnalyticSourceDenominator()
{
    const auto valid = tick(1, {
        event(CanonicalTableDepartureEventKind::OuterDeparture,
            3, 1, 0, 1, 0, .1),
        event(CanonicalTableDepartureEventKind::OutsideLanding,
            3, 1, 1, 2, 0, .4),
        event(CanonicalTableDepartureEventKind::OutsideRest,
            3, 1, 2, -1, 0, .8),
    });
    require(valid.attestation.privatePhysicsEventCount == 2 &&
            valid.attestation.analyticTransitionSourceCount == 1 &&
            valid.attestation.orderedSourceReceipts[2].sourceOrigin ==
                CanonicalTableDepartureSourceOrigin::AnalyticTransition,
        "analytic source has an independent denominator");
    CanonicalTableDepartureObserver observer;
    require(observer.begin(identity()) &&
            observer.consumeSuccessfulPhysicsTick(valid) &&
            observer.finish(true).complete &&
            observer.observation().balls.front().cameToRestOffTable,
        "typed analytic rest completes an outside lifecycle");

    const auto rejects = [&](CanonicalTableDepartureTick mutation,
            const char* message) {
        CanonicalTableDepartureObserver candidate;
        require(candidate.begin(identity()), "begin analytic mutation");
        require(!candidate.consumeSuccessfulPhysicsTick(mutation), message);
    };
    auto mutation = valid;
    mutation.attestation.analyticTransitionSourceCount = 0;
    rejects(mutation, "analytic denominator drift rejected");

    mutation = valid;
    mutation.attestation.orderedSourceReceipts[2].analyticPartitionDigest.clear();
    mutation.attestation.orderedSourceDigest = canonicalTableDepartureSourceDigest(
        mutation.attestation.orderedSourceReceipts);
    rejects(mutation, "missing analytic partition digest rejected");

    mutation = valid;
    mutation.attestation.orderedSourceReceipts[2].sourceSegmentOrdinal =
        mutation.attestation.orderedSourceReceipts[2].precedingSegmentOrdinal;
    mutation.attestation.orderedSourceDigest = canonicalTableDepartureSourceDigest(
        mutation.attestation.orderedSourceReceipts);
    rejects(mutation, "aliased transition segment ordinals rejected");

    mutation = valid;
    mutation.attestation.orderedSourceReceipts[2].velocityBeforeCmS.x = .1f;
    mutation.events[2].velocityBeforeCmS.x = .1f;
    mutation.attestation.orderedSourceReceipts[2].canonicalEventDigest =
        canonicalTableDepartureEventDigest({mutation.events[2]});
    mutation.attestation.orderedEventDigest =
        canonicalTableDepartureEventDigest(mutation.events);
    mutation.attestation.orderedSourceDigest = canonicalTableDepartureSourceDigest(
        mutation.attestation.orderedSourceReceipts);
    rejects(mutation, "nonzero analytic stop boundary velocity rejected");
}

}  // namespace

int main()
{
    try {
        testCueAndObjectOutsideRest();
        testRailReturnExclusion();
        testDirectClothLandingIsNotRailReturn();
        testCuePocketResetGenerationBoundary();
        testContinuedEpisodesWithinGeneration();
        testContiguousRetryAndRollback();
        testFailClosedAttestationAndOrdering();
        testSupportedDomainPostStateInvariants();
        testTypedAnalyticSourceDenominator();
        std::cout << "PASS_D6_B_TABLE_DEPARTURE_OBSERVER policy=0 gpu=0\n";
        return 0;
    } catch (const std::exception& failure) {
        std::cerr << "FAIL " << failure.what() << '\n';
        return 1;
    }
}
