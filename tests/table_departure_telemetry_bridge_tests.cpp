#include "physics.h"
#include "game_runtime.h"
#include "rules/table_departure_telemetry_bridge.h"

#include <algorithm>
#include <cmath>
#include <iostream>
#include <stdexcept>
#include <utility>

namespace {

using namespace billiardgl;
using namespace billiardgl::rules;

void require(bool condition, const char* message)
{
    if (!condition) throw std::runtime_error(message);
}

const PhysicsContactRecord& captureContact(
    const PhysicsStepTelemetry& telemetry)
{
    const PhysicsContactRecord* match = nullptr;
    for (const auto& contact : telemetry.contacts) {
        if (contact.kind != PhysicsContactKind::Pocket ||
            contact.pocketBoundaryEvent != PocketBoundaryEventKind::Capture)
            continue;
        require(match == nullptr, "capture contact unique");
        match = &contact;
    }
    require(match != nullptr, "capture contact present");
    return *match;
}

TableDepartureTelemetryBridgeContext contextFor(
    const PhysicsStepTelemetry&, const PhysicsProfile& profile,
    std::uint64_t tick)
{
    TableDepartureTelemetryBridgeContext context;
    context.physicsTick = tick;
    context.solverProfileId = profile.id;
    context.boundaryMode = PhysicsBoundaryMode::ProductionTable;
    context.successfulStep = true;
    context.complete = true;
    return context;
}

TableDepartureTelemetryBridgeContext privateContextFor(
    const PhysicsStepTelemetry& telemetry, const PhysicsProfile& profile,
    std::uint64_t tick)
{
    return contextFor(telemetry, profile, tick);
}

struct ScratchFixture {
    PhysicsProfile profile = phase3V5CandidatePhysicsProfile();
    GameState before;
    GameState after;
    PhysicsStepTelemetry telemetry;
};

ScratchFixture highSpeedCueScratch()
{
    ScratchFixture fixture;
    initializeBalls(fixture.before);
    for (int index = 1; index < kBallCount; ++index)
        fixture.before.balls[index].pocketed = true;
    fixture.before.balls[0].position = {
        -kTableInWidth / 2.0f + kBallRadius,
        kTableHeight + kBallRadius, 0.0f};
    fixture.before.balls[0].velocity.x = -400.0f;
    fixture.before.balls[0].speed = 400.0f;
    fixture.before.ballsMoving = true;
    fixture.after = fixture.before;
    fixture.telemetry = updatePhysics(fixture.after, .1f, fixture.profile,
        PhysicsBoundaryMode::ProductionTable);
    return fixture;
}

GameState simultaneousRailLandingReleaseState(
    const PhysicsProfile& profile)
{
    const auto geometry = buildTableDepartureGeometry(profile);
    const double diameter = 2.0 * profile.ball.radiusCm;
    constexpr double verticalSeparation = 1.0;
    const double horizontalSeparation = std::sqrt(
        diameter * diameter - verticalSeparation * verticalSeparation);
    GameState state;
    initializeBalls(state);
    for (auto& ball : state.balls) {
        resetBallMotion(ball);
        resetPocketInteraction(ball);
        ball.pocketed = true;
    }
    auto& landing = state.balls[0];
    landing.pocketed = false;
    landing.position = {68.0f, geometry.railTopSupportedCenterYCm, 30.0f};
    landing.velocity = {0.0f, -1.0f, 0.0f};
    landing.speed = 1.0f;
    landing.physicalDomain = PhysicalBallDomain::Airborne;
    auto& striker = state.balls[1];
    striker.pocketed = false;
    striker.position = {static_cast<float>(68.0 - horizontalSeparation),
        static_cast<float>(geometry.railTopSupportedCenterYCm -
            verticalSeparation), 30.0f};
    striker.velocity = {50.0f, 0.0f, 0.0f};
    striker.speed = 50.0f;
    striker.physicalDomain = PhysicalBallDomain::Airborne;
    state.pocketedBallCount = kBallCount - 2;
    state.ballsMoving = true;
    clearGameplayEvents(state);
    return state;
}

void testRealCaptureResetBridge()
{
    const auto fixture = highSpeedCueScratch();
    const auto& contact = captureContact(fixture.telemetry);
    const auto entry = std::find_if(
        fixture.telemetry.tableDepartureEvents.begin(),
        fixture.telemetry.tableDepartureEvents.end(), [](const auto& event) {
            return event.kind == TableDepartureEventKind::PocketEntry;
        });
    const bool nativeEntryExact =
        entry == fixture.telemetry.tableDepartureEvents.end() ||
        (entry->pocketId == contact.pocketId &&
         entry->linkedPhysicsContactOrdinal ==
            static_cast<int>(&contact - fixture.telemetry.contacts.data()) &&
         entry->pocketCaptureSequence == contact.pocketCaptureSequence &&
         entry->solverEventId == contact.solverEventId);
    require(fixture.telemetry.stepStatus == PhysicsStepStatus::Succeeded &&
            nativeEntryExact &&
            contact.administrativeResetAfterCapture &&
            contact.trajectoryGenerationBefore == 0 &&
            contact.trajectoryGenerationAfter == 1 &&
            fixture.after.balls[0].trajectoryGeneration == 1,
        "real scratch generation evidence");
    const auto context = contextFor(fixture.telemetry, fixture.profile, 1);
    const auto adapted = adaptPhysicsStepTelemetryTick(fixture.telemetry,
        fixture.before, fixture.after, fixture.profile, context);
    if (!adapted.complete) {
        std::cerr << "bridge_error=" << adapted.error
                  << " first=" << contact.firstBall
                  << " second=" << contact.secondBall
                  << " solver=" << contact.solverEventId
                  << " pocket=" << contact.pocketId
                  << " passable=" << contact.pocketPassable
                  << " phaseAfter=" << int(contact.pocketPhaseAfter)
                  << " sequence=" << contact.pocketCaptureSequence
                  << " toi=" << contact.wholeTickTimeOfImpactSeconds
                  << " duration=" << fixture.telemetry.wholeTickDurationSeconds
                  << '\n';
    }
    require(adapted.complete && adapted.tick.events.size() == 2,
        "real scratch bridge complete");
    require(adapted.tick.events[0].kind ==
                CanonicalTableDepartureEventKind::PocketCapture &&
            adapted.tick.events[1].kind ==
                CanonicalTableDepartureEventKind::AdministrativeReset &&
            adapted.tick.events[0].trajectoryGeneration == 0 &&
            adapted.tick.events[1].previousTrajectoryGeneration == 0 &&
            adapted.tick.events[1].trajectoryGeneration == 1 &&
            adapted.tick.events[1].positionCm.x ==
                contact.administrativeResetPositionCm.x &&
            adapted.tick.events[1].positionCm.y ==
                contact.administrativeResetPositionCm.y &&
            adapted.tick.events[1].positionCm.z ==
                contact.administrativeResetPositionCm.z,
        "capture reset ordered exact values");
    require(adapted.tick.attestation.canonicalContactSourceCount == 1 &&
            adapted.tick.attestation.sourceEventCount == 2 &&
            adapted.tick.attestation.emittedEventCount == 2,
        "one canonical contact projects ordered capture reset range");

    bool sawContact = false;
    bool sawPocketEntry = false;
    for (const auto& receipt :
            adapted.tick.attestation.orderedSourceReceipts) {
        if (receipt.sourceOrigin ==
                CanonicalTableDepartureSourceOrigin::CanonicalContact) {
            sawContact = receipt.canonicalEventCount == 2 &&
                receipt.physicsContactOrdinal >= 0 &&
                receipt.canonicalEventDigest ==
                    canonicalTableDepartureEventDigest(adapted.tick.events);
        }
        if (receipt.kind ==
                CanonicalTableDeparturePrivateEventKind::
                    PocketEntryDiagnostic) {
            sawPocketEntry = receipt.disposition ==
                    CanonicalTableDepartureProjectionDisposition::
                        ExcludedDiagnostic &&
                receipt.exclusionReason ==
                    CanonicalTableDepartureExclusionReason::
                        PocketEntryBoundToCanonicalCapture &&
                receipt.canonicalContactOrdinal == 0;
        }
    }
    require(sawContact, "canonical contact receipt exact range");
    if (!fixture.telemetry.tableDepartureEvents.empty())
        require(sawPocketEntry, "private pocket entry retained as diagnostic");

    CanonicalTableDepartureObserverIdentity identity;
    identity.shotIdentity = "real-high-speed-cue-scratch";
    identity.rackIdentity = "real-high-speed-cue-scratch-rack";
    identity.solverProfileId = fixture.profile.id;
    identity.boundaryMode = PhysicsBoundaryMode::ProductionTable;
    identity.firstPhysicsTick = 1;
    identity.initialTrajectoryGenerations[0] = 0;
    CanonicalTableDepartureObserver observer;
    require(observer.begin(identity) &&
            observer.consumeSuccessfulPhysicsTick(adapted.tick) &&
            observer.finish(true).complete,
        "real scratch reaches complete observer lifecycle");
    require(observer.observation().balls.size() == 2 &&
            observer.observation().balls[0].capturedInPocket &&
            observer.observation().balls[0].complete &&
            observer.observation().balls[1].administrativeResetObserved &&
            observer.observation().balls[1].complete &&
            !observer.observation().balls[0].cameToRestOffTable &&
            !observer.observation().balls[1].cameToRestOffTable,
        "normal cue scratch complete and never off-table");

    auto malformed = adapted.tick;
    auto contactReceipt = std::find_if(
        malformed.attestation.orderedSourceReceipts.begin(),
        malformed.attestation.orderedSourceReceipts.end(),
        [](const auto& receipt) {
            return receipt.sourceOrigin ==
                CanonicalTableDepartureSourceOrigin::CanonicalContact;
        });
    require(contactReceipt != malformed.attestation.orderedSourceReceipts.end(),
        "contact receipt mutation target");
    contactReceipt->canonicalEventCount = 1;
    malformed.attestation.orderedSourceDigest =
        canonicalTableDepartureSourceDigest(
            malformed.attestation.orderedSourceReceipts);
    CanonicalTableDepartureObserver rejected;
    require(rejected.begin(identity) &&
            !rejected.consumeSuccessfulPhysicsTick(malformed),
        "capture reset range mutation rejected");
}

void testPocketEntryCannotMasqueradeAsCapture()
{
    auto fixture = highSpeedCueScratch();
    const auto& capture = captureContact(fixture.telemetry);
    TableDepartureEvent entry;
    entry.kind = TableDepartureEventKind::PocketEntry;
    entry.ballIndex = 0;
    entry.ordinal = 0;
    entry.trajectoryGeneration = capture.trajectoryGenerationBefore;
    entry.solverEventId = capture.solverEventId;
    entry.pocketId = capture.pocketId;
    entry.linkedPhysicsContactOrdinal = static_cast<int>(
        &capture - fixture.telemetry.contacts.data());
    entry.pocketCaptureSequence = capture.pocketCaptureSequence;
    entry.absoluteWholeTickTimeSeconds =
        capture.wholeTickTimeOfImpactSeconds;
    entry.stableIdentity = "synthetic_private_pocket_entry_bound_to_real_capture";
    entry.transitionWitnessComplete = true;
    entry.targetDomain = PhysicalBallDomain::PocketChannel;
    fixture.telemetry.tableDepartureEvents = {entry};
    auto missing = fixture.telemetry;
    missing.contacts.clear();
    missing.analyticSurfaceMotion.erase(std::remove_if(
        missing.analyticSurfaceMotion.begin(),
        missing.analyticSurfaceMotion.end(), [](const auto& segment) {
            return segment.ballIndex == 0 &&
                segment.trajectoryGeneration != 0;
        }), missing.analyticSurfaceMotion.end());
    missing.tableDepartureEvents.front().solverEventId = -1;
    missing.tableDepartureEvents.front().linkedPhysicsContactOrdinal = -1;
    missing.tableDepartureEvents.front().pocketCaptureSequence = 0;
    missing.tableDepartureEvents.front().sourceDomain =
        PhysicalBallDomain::Airborne;
    GameState pendingAfter = fixture.before;
    pendingAfter.balls[0].physicalDomain = PhysicalBallDomain::PocketChannel;
    auto context = contextFor(missing, fixture.profile, 1);
    const auto result = adaptPhysicsStepTelemetryTick(missing,
        fixture.before, pendingAfter, fixture.profile, context);
    if (!result.complete)
        std::cerr << "pending_entry_error=" << result.error << '\n';
    require(result.complete && result.tick.events.empty() &&
            result.tick.attestation.pendingPocketEntrySourceCount == 1 &&
            result.tick.attestation.orderedSourceReceipts.size() == 1 &&
            result.tick.attestation.orderedSourceReceipts[0].disposition ==
                CanonicalTableDepartureProjectionDisposition::
                    ObservedPendingPocketEntry,
        "pocket entry without capture remains an exact pending source");

    CanonicalTableDepartureObserverIdentity identity;
    identity.shotIdentity = "cross-tick-pocket-entry";
    identity.rackIdentity = "cross-tick-pocket-entry-rack";
    identity.solverProfileId = fixture.profile.id;
    identity.boundaryMode = PhysicsBoundaryMode::ProductionTable;
    identity.firstPhysicsTick = 1;
    CanonicalTableDepartureObserver observer;
    const bool began = observer.begin(identity);
    const bool consumedPending = began &&
        observer.consumeSuccessfulPhysicsTick(result.tick);
    const bool pendingCountExact = consumedPending &&
        observer.observation().pendingPocketEntryCount == 1;
    const bool pendingFinishIncomplete = pendingCountExact &&
        !observer.finish(true).complete;
    if (!pendingFinishIncomplete)
        std::cerr << "pending_observer began=" << began
                  << " consumed=" << consumedPending << " count="
                  << observer.observation().pendingPocketEntryCount
                  << " sourceCount="
                  << result.tick.attestation.pendingPocketEntrySourceCount
                  << " private="
                  << result.tick.attestation.privatePhysicsEventCount
                  << " digest="
                  << result.tick.attestation.orderedSourceDigest << '\n';
    require(pendingFinishIncomplete,
        "shot cannot complete while pocket entry awaits capture");

    auto capturedTelemetry = fixture.telemetry;
    capturedTelemetry.tableDepartureEvents.clear();
    const auto captured = adaptPhysicsStepTelemetryTick(capturedTelemetry,
        fixture.before, fixture.after, fixture.profile,
        contextFor(capturedTelemetry, fixture.profile, 2));
    require(captured.complete &&
            observer.consumeSuccessfulPhysicsTick(captured.tick) &&
            observer.observation().pendingPocketEntryCount == 0 &&
            observer.finish(true).complete,
        "later real capture resolves pending pocket entry lifecycle");

    auto wrongPocketTelemetry = fixture.telemetry;
    ++wrongPocketTelemetry.tableDepartureEvents.front().pocketId;
    const auto wrongPocket = adaptPhysicsStepTelemetryTick(wrongPocketTelemetry,
        fixture.before, fixture.after, fixture.profile,
        contextFor(wrongPocketTelemetry, fixture.profile, 1));
    require(!wrongPocket.complete && wrongPocket.tick.events.empty(),
        "pocket entry pocket binding mismatch rejected");
}

void testRealVerticalPocketChannelResolvesPendingEntry()
{
    const auto profile = interactiveChinesePoolPhysicsProfile();
    const auto frames = buildPocketBoundaryFrames(profile.tableBoundary);
    constexpr int ballId = 1;
    constexpr int pocketId = 0;

    GameState state;
    initializeBalls(state);
    for (auto& ball : state.balls) ball.pocketed = true;
    auto& ball = state.balls[ballId];
    ball.pocketed = false;
    ball.physicalDomain = PhysicalBallDomain::Airborne;
    ball.position = {frames[pocketId].mouthCenter.x,
        profile.tableDeparture.clothSurfaceYCm + profile.ball.radiusCm + 1.0f,
        frames[pocketId].mouthCenter.z};
    ball.velocity = {0.0f, -20.0f, 0.0f};
    ball.speed = 20.0f;
    state.ballsMoving = true;

    CanonicalTableDepartureObserverIdentity identity;
    identity.shotIdentity = "real-vertical-pocket-channel";
    identity.rackIdentity = "real-vertical-pocket-channel-rack";
    identity.solverProfileId = profile.id;
    identity.boundaryMode = PhysicsBoundaryMode::ProductionTable;
    identity.firstPhysicsTick = 1;
    CanonicalTableDepartureObserver observer;
    require(observer.begin(identity), "begin real vertical pocket observer");

    bool sawPendingEntry = false;
    bool sawThroat = false;
    bool sawCapture = false;
    for (std::uint64_t tick = 1; tick <= 120 && !sawCapture; ++tick) {
        const GameState before = state;
        const auto telemetry = updatePhysics(state, 1.0f / 120.0f, profile,
            PhysicsBoundaryMode::ProductionTable);
        require(telemetry.stepStatus == PhysicsStepStatus::Succeeded,
            "real vertical pocket producer tick succeeds");
        const auto adapted = adaptPhysicsStepTelemetryTick(telemetry, before,
            state, profile, contextFor(telemetry, profile, tick));
        if (!adapted.complete)
            std::cerr << "vertical_bridge_tick=" << tick
                      << " error=" << adapted.error << '\n';
        require(adapted.complete &&
                observer.consumeSuccessfulPhysicsTick(adapted.tick),
            "real vertical pocket tick bridges with native bindings");

        sawPendingEntry = sawPendingEntry ||
            adapted.tick.attestation.pendingPocketEntrySourceCount == 1;
        for (const auto& contact : telemetry.contacts) {
            if (contact.kind != PhysicsContactKind::Pocket ||
                contact.firstBall != ballId || contact.pocketId != pocketId)
                continue;
            if (contact.pocketBoundaryEvent == PocketBoundaryEventKind::Throat)
                sawThroat = contact.pocketVerticalShaft;
            if (contact.pocketBoundaryEvent != PocketBoundaryEventKind::Capture)
                continue;
            require(contact.pocketVerticalShaft &&
                    contact.normal.x == 0.0f &&
                    contact.normal.y == -1.0f &&
                    contact.normal.z == 0.0f &&
                    contact.trajectoryGenerationBefore == 0 &&
                    contact.trajectoryGenerationAfter == 0 &&
                    !contact.administrativeResetAfterCapture,
                "vertical object capture retains exact shaft and generation");
            const auto event = std::find_if(adapted.tick.events.begin(),
                adapted.tick.events.end(), [](const auto& candidate) {
                    return candidate.kind ==
                        CanonicalTableDepartureEventKind::PocketCapture;
                });
            require(event != adapted.tick.events.end() &&
                    event->ballId == ballId && event->pocketId == pocketId &&
                    event->sourceDomain == PhysicalBallDomain::PocketChannel &&
                    event->targetDomain == PhysicalBallDomain::PocketChannel,
                "real vertical capture binds PocketChannel source domain");

            auto missingShaft = telemetry;
            auto& changed = *std::find_if(missingShaft.contacts.begin(),
                missingShaft.contacts.end(), [](const auto& candidate) {
                    return candidate.kind == PhysicsContactKind::Pocket &&
                        candidate.pocketBoundaryEvent ==
                            PocketBoundaryEventKind::Capture;
                });
            changed.pocketVerticalShaft = false;
            const auto rejected = adaptPhysicsStepTelemetryTick(missingShaft,
                before, state, profile,
                contextFor(missingShaft, profile, tick));
            require(!rejected.complete && rejected.tick.events.empty(),
                "vertical capture without shaft provenance is rejected");
            sawCapture = true;
        }
    }

    const auto validation = observer.finish(true);
    const auto& observation = observer.observation();
    require(sawPendingEntry && sawThroat && sawCapture && validation.complete &&
            observation.pendingPocketEntryCount == 0 &&
            observation.balls.size() == 1 &&
            observation.balls[0].ballId == ballId &&
            observation.balls[0].capturedInPocket &&
            !observation.balls[0].cameToRestOffTable,
        "real cross-tick pocket entry resolves only at actual shaft capture");
}

void testRealTelemetryPrivateDeparturePath()
{
    const PhysicsProfile profile = phase3V5CandidatePhysicsProfile();
    const auto geometry = buildTableDepartureGeometry(profile);
    GameState before;
    initializeBalls(before);
    for (int index = 1; index < kBallCount; ++index)
        before.balls[index].pocketed = true;
    before.balls[0].position = {20.0f,
        geometry.clothSupportedCenterYCm + 3.0f, 10.0f};
    before.balls[0].velocity = {12.0f, -20.0f, 4.0f};
    before.balls[0].speed = std::sqrt(560.0f);
    before.balls[0].physicalDomain = PhysicalBallDomain::Airborne;
    before.ballsMoving = true;
    GameState after = before;
    const auto telemetry = updatePhysics(after, .2f, profile,
        PhysicsBoundaryMode::ProductionTable);
    require(telemetry.stepStatus == PhysicsStepStatus::Succeeded &&
            !telemetry.tableDepartureEvents.empty() &&
            !telemetry.analyticSurfaceMotion.empty(),
        "real updatePhysics retains private events and analytic segments");
    const auto adapted = adaptPhysicsStepTelemetryTick(telemetry, before,
        after, profile, privateContextFor(telemetry, profile, 7));
    if (!adapted.complete) {
        std::cerr << "private_bridge_error=" << adapted.error << '\n';
        for (const auto& event : telemetry.tableDepartureEvents) {
            std::cerr << "  private ordinal=" << event.ordinal
                      << " kind=" << static_cast<int>(event.kind)
                      << " solver=" << event.solverEventId
                      << " generation=" << event.trajectoryGeneration
                      << " source=" << static_cast<int>(event.sourceDomain)
                      << " target=" << static_cast<int>(event.targetDomain)
                      << " boundary=" << event.boundaryFeatureId
                      << " rail=" << event.railFeatureId
                      << " landing=" << event.landingSurfaceId
                      << " provenance="
                      << static_cast<int>(event.clothReturnProvenance)
                      << " time=" << event.absoluteWholeTickTimeSeconds
                      << " duration=" << telemetry.wholeTickDurationSeconds
                      << " witness=" << event.transitionWitnessComplete
                      << " identity=" << event.stableIdentity
                      << '\n';
        }
    }
    require(adapted.complete &&
            adapted.tick.attestation.privatePhysicsEventCount ==
                telemetry.tableDepartureEvents.size() &&
            adapted.tick.attestation.canonicalContactSourceCount == 0 &&
            adapted.tick.events.front().trajectoryGeneration ==
                before.balls[0].trajectoryGeneration,
        "real private producer rows bridge without ordinal or generation rewrite");
}

void testNativeSupportedLandingMetadata()
{
    const PhysicsProfile profile = phase3V5CandidatePhysicsProfile();
    const auto geometry = buildTableDepartureGeometry(profile);
    auto run = [&](Point3 position, Point3 velocity,
                   PhysicalBallDomain domain, double duration,
                   TableDepartureEventKind expectedKind,
                   std::uint64_t tick) {
        GameState before;
        initializeBalls(before);
        for (int index = 1; index < kBallCount; ++index)
            before.balls[index].pocketed = true;
        before.balls[0].position = position;
        before.balls[0].velocity = velocity;
        before.balls[0].speed = std::sqrt(velocity.x * velocity.x +
            velocity.y * velocity.y + velocity.z * velocity.z);
        before.balls[0].physicalDomain = domain;
        before.ballsMoving = true;
        GameState after = before;
        const auto telemetry = updatePhysics(after,
            static_cast<float>(duration), profile,
            PhysicsBoundaryMode::ProductionTable);
        const auto event = std::find_if(telemetry.tableDepartureEvents.begin(),
            telemetry.tableDepartureEvents.end(),
            [expectedKind](const auto& candidate) {
                return candidate.kind == expectedKind;
            });
        require(telemetry.stepStatus == PhysicsStepStatus::Succeeded &&
                event != telemetry.tableDepartureEvents.end() &&
                event->solverEventId >= 0 &&
                event->linkedPhysicsContactOrdinal >= 0 &&
                static_cast<std::size_t>(event->linkedPhysicsContactOrdinal) <
                    telemetry.contacts.size(),
            "real supported landing carries native contact metadata");
        const auto& contact = telemetry.contacts[static_cast<std::size_t>(
            event->linkedPhysicsContactOrdinal)];
        require(contact.kind == PhysicsContactKind::Support &&
                contact.firstBall == event->ballIndex &&
                contact.solverEventId == event->solverEventId &&
                contact.wholeTickTimeOfImpactSeconds ==
                    event->absoluteWholeTickTimeSeconds,
            "native landing link resolves exact support contact");
        const auto adapted = adaptPhysicsStepTelemetryTick(telemetry, before,
            after, profile, contextFor(telemetry, profile, tick));
        if (!adapted.complete)
            std::cerr << "native_landing_bridge_error=" << adapted.error
                      << " kind=" << static_cast<int>(expectedKind) << '\n';
        require(adapted.complete,
            "native supported landing bridges without external binding");
        auto movedContact = telemetry;
        movedContact.contacts[static_cast<std::size_t>(
            event->linkedPhysicsContactOrdinal)].firstPositionAtContactCm.x +=
                2.0f * profile.solver.penetrationSlopCm;
        const auto rejected = adaptPhysicsStepTelemetryTick(movedContact,
            before, after, profile,
            contextFor(movedContact, profile, tick));
        require(!rejected.complete && rejected.tick.events.empty(),
            "native support contact position drift is rejected");
        return std::make_pair(telemetry, adapted);
    };

    const float railX = (geometry.playfieldHalfWidthCm +
        geometry.ballRadiusCm + geometry.outerHalfWidthCm) * 0.5f;
    const auto railLanding = run({railX,
            geometry.railTopSupportedCenterYCm + 3.0f, 18.0f},
        {0.0f, -20.0f, 0.0f}, PhysicalBallDomain::Airborne, 0.2,
        TableDepartureEventKind::RailTopLanding, 11);
    const auto railEvent = std::find_if(
        railLanding.first.tableDepartureEvents.begin(),
        railLanding.first.tableDepartureEvents.end(), [](const auto& event) {
            return event.kind == TableDepartureEventKind::RailTopLanding;
        });
    require(railEvent->railFeatureId >= 0,
        "rail landing retains native rail feature");

    const auto outsideLanding = run({geometry.outerHalfWidthCm + 8.0f,
            geometry.outsideLandingSupportedCenterYCm + 3.0f, 12.0f},
        {0.0f, -20.0f, 0.0f}, PhysicalBallDomain::Airborne, 0.2,
        TableDepartureEventKind::OutsideLanding, 12);
    const auto outsideEvent = std::find_if(
        outsideLanding.first.tableDepartureEvents.begin(),
        outsideLanding.first.tableDepartureEvents.end(), [](const auto& event) {
            return event.kind == TableDepartureEventKind::OutsideLanding;
        });
    require(outsideEvent->landingSurfaceId >= 0,
        "outside landing retains native surface feature");
}

void testSimultaneousLandingImmediateReleaseIsNotSettled()
{
    PhysicsProfile profile = interactiveChinesePoolPhysicsProfile();
    profile.solver.maximumEventsPerTick = 2;
    const GameState before = simultaneousRailLandingReleaseState(profile);
    GameState after = before;
    const auto telemetry = updatePhysics(after, 0.001f, profile,
        PhysicsBoundaryMode::ProductionTable);
    require(telemetry.stepStatus == PhysicsStepStatus::Succeeded &&
            telemetry.tableDepartureEvents.size() == 1,
        "simultaneous landing release real producer fixture");
    const auto& source = telemetry.tableDepartureEvents.front();
    require(source.kind == TableDepartureEventKind::RailTopLanding &&
            source.sourceDomain == PhysicalBallDomain::Airborne &&
            source.targetDomain == PhysicalBallDomain::Airborne &&
            source.velocityAfterCmS.y >
                profile.solver.residualToleranceCmS &&
            source.solverEventId >= 0 &&
            source.linkedPhysicsContactOrdinal >= 0,
        "private witness retains positive-vy immediate release");
    const auto adapted = adaptPhysicsStepTelemetryTick(telemetry, before,
        after, profile, contextFor(telemetry, profile, 19));
    require(adapted.complete && adapted.tick.events.empty() &&
            adapted.tick.attestation.sourceEventCount == 0 &&
            adapted.tick.attestation.nonSettlingSupportContactSourceCount == 1 &&
            adapted.tick.attestation.excludedPhysicsDiagnosticEventCount == 0 &&
            adapted.tick.attestation.orderedSourceReceipts.size() == 1 &&
            adapted.tick.attestation.orderedSourceReceipts[0].disposition ==
                CanonicalTableDepartureProjectionDisposition::
                    ObservedNonSettlingSupport &&
            adapted.tick.attestation.orderedSourceReceipts[0].exclusionReason ==
                CanonicalTableDepartureExclusionReason::
                    SupportContactImmediateRelease &&
            adapted.tick.attestation.orderedSourceReceipts[0].
                physicsContactOrdinal == source.linkedPhysicsContactOrdinal &&
            adapted.tick.attestation.orderedSourceReceipts[0].
                velocityAfterCmS.y == source.velocityAfterCmS.y,
        "immediate release remains typed source without settled landing event");

    auto falseSettled = adapted.tick;
    falseSettled.attestation.nonSettlingSupportContactSourceCount = 0;
    CanonicalTableDepartureObserverIdentity identity;
    identity.shotIdentity = "immediate-release";
    identity.rackIdentity = "immediate-release-rack";
    identity.solverProfileId = profile.id;
    identity.boundaryMode = PhysicsBoundaryMode::ProductionTable;
    identity.firstPhysicsTick = 19;
    CanonicalTableDepartureObserver rejected;
    require(rejected.begin(identity) &&
            !rejected.consumeSuccessfulPhysicsTick(falseSettled),
        "non-settling source count mutation is fail-closed");
}

void testTypedSolverPositionProjectionBridge()
{
    const PhysicsProfile profile = defaultChinesePoolPhysicsProfile();
    constexpr double duration = .01;
    GameState before;
    initializeBalls(before);
    for (int ball = 1; ball < kBallCount; ++ball)
        before.balls[ball].pocketed = true;
    before.balls[0].position = {0.0f, 10.0f, 0.0f};
    before.balls[0].velocity = {100.0f, 0.0f, 0.0f};
    before.balls[0].speed = 100.0f;
    before.balls[0].physicalDomain = PhysicalBallDomain::Airborne;
    before.ballsMoving = true;
    GameState after = before;
    after.balls[0].position = {2.0f, 10.0f, 0.0f};

    PhysicsStepTelemetry telemetry;
    telemetry.wholeTickDurationSeconds = duration;
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
    telemetry.analyticSurfaceMotion = {first, second};

    TableDepartureEvent departure;
    departure.kind = TableDepartureEventKind::OuterDeparture;
    departure.ballIndex = 0;
    departure.ordinal = 0;
    departure.boundaryFeatureId = 2;
    departure.absoluteWholeTickTimeSeconds = .002;
    departure.stableIdentity = "projection-bridge-outer-departure";
    departure.transitionWitnessComplete = true;
    departure.velocityBeforeCmS = {100.0f, 0.0f, 0.0f};
    departure.velocityBeforeWitnessComplete = true;
    departure.velocityAfterCmS = {100.0f, 0.0f, 0.0f};
    departure.sourceDomain = PhysicalBallDomain::Airborne;
    departure.targetDomain = PhysicalBallDomain::Airborne;
    telemetry.tableDepartureEvents = {departure};

    PhysicsContactRecord contact;
    contact.kind = PhysicsContactKind::Support;
    contact.firstBall = 0;
    contact.secondBall = -1;
    contact.solverEventId = 7;
    contact.solverIslandId = 3;
    contact.wholeTickTimeOfImpactSeconds = .005;
    contact.positionCorrected = true;
    contact.solverProjectionCm = 1.0;
    contact.firstPositionCorrectionCm = {1.0f, 0.0f, 0.0f};
    telemetry.contacts = {contact};
    SolverEventRecord solver;
    solver.eventId = 7;
    solver.islandId = 3;
    solver.contactCount = 1;
    solver.positionIterations = 1;
    solver.failureCode = "converged";
    solver.wholeTickTimeOfImpactSeconds = .005;
    SolverPositionProjection projection;
    projection.ordinal = 0;
    projection.iteration = 0;
    projection.contactIndex = 0;
    projection.ballIndex = 0;
    projection.trajectoryGeneration = 0;
    projection.beforeCm = first.endPositionCm;
    projection.afterCm = second.startPositionCm;
    solver.positionProjections = {projection};
    telemetry.solverEvents = {solver};

    const auto context = contextFor(telemetry, profile, 23);
    require(adaptPhysicsStepTelemetryTick(telemetry, before, after, profile,
                context).complete,
        "bridge consumes an exact typed solver position projection");
    auto mutateProjection = [&](auto mutation) {
        auto changed = telemetry;
        mutation(changed.solverEvents[0],
            changed.solverEvents[0].positionProjections[0]);
        return adaptPhysicsStepTelemetryTick(changed, before, after, profile,
            context);
    };
    require(!mutateProjection([](auto&, auto& receipt) {
                receipt.beforeCm.x += 1.0f;
            }).complete,
        "bridge rejects a forged solver projection before position");
    require(!mutateProjection([](auto&, auto& receipt) {
                ++receipt.ordinal;
            }).complete,
        "bridge rejects a noncanonical projection ordinal");
    require(!mutateProjection([](auto&, auto& receipt) {
                ++receipt.trajectoryGeneration;
            }).complete,
        "bridge rejects projection generation drift");
    require(!mutateProjection([](auto& event, auto& receipt) {
                receipt.contactIndex = event.contactCount;
            }).complete,
        "bridge rejects projection without solver contact authority");
}

void testRealMixedOverlapRequiresProjectionReceipt()
{
    const auto profile = interactiveChinesePoolPhysicsProfile();
    GameState state;
    initializeBalls(state);
    for (auto& ball : state.balls) ball.pocketed = true;
    auto& airborne = state.balls[1];
    auto& cloth = state.balls[2];
    airborne.pocketed = cloth.pocketed = false;
    const float plane = profile.tableDeparture.clothSurfaceYCm +
        profile.ball.radiusCm;
    constexpr double penetration = .00109;
    const double separation = 2.0 * profile.ball.radiusCm - penetration;
    airborne.position = {0.0f, plane + .1f, 0.0f};
    airborne.physicalDomain = PhysicalBallDomain::Airborne;
    cloth.position = {static_cast<float>(
        std::sqrt(separation * separation - .01)), plane, 0.0f};
    cloth.velocity = {1.0f, 0.0f, 0.0f};
    cloth.speed = 1.0f;
    state.ballsMoving = true;
    const GameState before = state;
    const auto telemetry = updatePhysics(state, 0.0f, profile,
        PhysicsBoundaryMode::ProductionTable);
    bool sawCorrectedContact = false;
    std::size_t projectionCount = 0;
    for (const auto& contact : telemetry.contacts) {
        sawCorrectedContact = sawCorrectedContact ||
            (contact.kind == PhysicsContactKind::BallBall &&
             contact.firstBall == 1 && contact.secondBall == 2 &&
             contact.normalImpulseNs == 0.0 && contact.positionCorrected);
    }
    for (const auto& event : telemetry.solverEvents)
        projectionCount += event.positionProjections.size();
    const auto context = contextFor(telemetry, profile, 24);
    require(telemetry.stepStatus == PhysicsStepStatus::Succeeded &&
            sawCorrectedContact && projectionCount > 0 &&
            adaptPhysicsStepTelemetryTick(telemetry, before, state, profile,
                context).complete,
        "real mixed overlap publishes a consumable zero-impulse projection");

    auto missing = telemetry;
    for (auto& event : missing.solverEvents)
        event.positionProjections.clear();
    const auto rejected = adaptPhysicsStepTelemetryTick(missing, before,
        state, profile, context);
    require(!rejected.complete && rejected.tick.events.empty() &&
            rejected.error ==
                "table_departure_position_projection_source_missing",
        "bridge cannot accept a corrected contact with its receipt removed");
}

void testInteractiveBreakEveryTickUsesNativeBindings()
{
    GameRuntime runtime;
    GameAction aim;
    aim.type = ActionType::SetAimYaw;
    aim.first = static_cast<float>(std::acos(-1.0) * 0.5);
    GameAction power;
    power.type = ActionType::SetShotPower;
    power.first = 200.0f;
    GameAction shoot;
    shoot.type = ActionType::Shoot;
    require(runtime.dispatch(aim).ok && runtime.dispatch(power).ok &&
            runtime.dispatch(shoot).ok,
        "interactive break shot accepted");

    std::array<int, kBallCount> activeOutsideSurfaces{};
    activeOutsideSurfaces.fill(-1);
    std::size_t bridgedTicks = 0;
    std::size_t privateEvents = 0;
    std::size_t canonicalEvents = 0;
    for (int step = 0; step < 3600 && runtime.state().ballsMoving; ++step) {
        const GameState before = runtime.state();
        const auto stepped = runtime.step(1);
        require(stepped.ok && runtime.hasRecentPhysicsStep(),
            "interactive break physics tick succeeds");
        const auto& telemetry = runtime.latestPhysicsStep().telemetry;
        TableDepartureTelemetryBridgeContext context = contextFor(telemetry,
            runtime.physicsProfile(), runtime.tick());
        context.activeOutsideLandingSurfaceIds = activeOutsideSurfaces;
        require(context.orderedEventBindings.empty(),
            "interactive smoke never supplies external event bindings");
        const auto adapted = adaptPhysicsStepTelemetryTick(telemetry, before,
            runtime.state(), runtime.physicsProfile(), context);
        if (!adapted.complete) {
            std::cerr << "interactive_bridge_incomplete tick=" << runtime.tick()
                      << " error=" << adapted.error
                      << " private=" << telemetry.tableDepartureEvents.size()
                      << " contacts=" << telemetry.contacts.size()
                      << " segments=" << telemetry.analyticSurfaceMotion.size()
                      << '\n';
            for (const auto& event : telemetry.tableDepartureEvents) {
                std::cerr << "  event ordinal=" << event.ordinal
                          << " kind=" << static_cast<int>(event.kind)
                          << " solver=" << event.solverEventId
                          << " contact=" << event.linkedPhysicsContactOrdinal
                          << " source=" << static_cast<int>(event.sourceDomain)
                          << " target=" << static_cast<int>(event.targetDomain)
                          << " vy=" << event.velocityAfterCmS.y << '\n';
            }
            for (const auto& contact : telemetry.contacts) {
                if (contact.kind != PhysicsContactKind::Pocket ||
                    contact.pocketBoundaryEvent !=
                        PocketBoundaryEventKind::Capture)
                    continue;
                std::cerr << "  capture ball=" << contact.firstBall
                          << " solver=" << contact.solverEventId
                          << " pocket=" << contact.pocketId
                          << " generation="
                          << contact.trajectoryGenerationBefore
                          << " toi=" << contact.wholeTickTimeOfImpactSeconds
                          << " position=" << contact.firstPositionAtContactCm.x
                          << ',' << contact.firstPositionAtContactCm.y << ','
                          << contact.firstPositionAtContactCm.z << '\n';
                for (const auto& segment : telemetry.analyticSurfaceMotion) {
                    if (segment.ballIndex != contact.firstBall) continue;
                    std::cerr << "    segment ordinal="
                              << segment.segmentOrdinal << " generation="
                              << segment.trajectoryGeneration << " begin="
                              << segment.wholeTickStartOffsetSeconds << " end="
                              << segment.wholeTickStartOffsetSeconds +
                                    segment.durationSeconds << " endPosition="
                              << segment.endPositionCm.x << ','
                              << segment.endPositionCm.y << ','
                              << segment.endPositionCm.z << '\n';
                }
            }
        }
        require(adapted.complete,
            "every interactive break tick bridges or reports exact incomplete");
        ++bridgedTicks;
        privateEvents += telemetry.tableDepartureEvents.size();
        canonicalEvents += adapted.tick.events.size();
        for (const auto& event : adapted.tick.events) {
            if (event.kind ==
                    CanonicalTableDepartureEventKind::OutsideLanding)
                activeOutsideSurfaces[static_cast<std::size_t>(event.ballId)] =
                    event.landingSurfaceId;
            if (event.kind ==
                    CanonicalTableDepartureEventKind::PocketCapture ||
                event.kind ==
                    CanonicalTableDepartureEventKind::AdministrativeReset)
                activeOutsideSurfaces[static_cast<std::size_t>(event.ballId)] =
                    -1;
        }
    }
    require(bridgedTicks > 0 && !runtime.state().ballsMoving,
        "interactive break reaches rest through 120Hz bridge smoke");
    std::cout << " interactive_ticks=" << bridgedTicks
              << " private_events=" << privateEvents
              << " canonical_events=" << canonicalEvents;
}

}  // namespace

int main()
{
    try {
        testRealCaptureResetBridge();
        testPocketEntryCannotMasqueradeAsCapture();
        testRealVerticalPocketChannelResolvesPendingEntry();
        testRealTelemetryPrivateDeparturePath();
        testNativeSupportedLandingMetadata();
        testSimultaneousLandingImmediateReleaseIsNotSettled();
        testTypedSolverPositionProjectionBridge();
        testRealMixedOverlapRequiresProjectionReceipt();
        testInteractiveBreakEveryTickUsesNativeBindings();
        testNativeSupportedLandingMetadata();
        std::cout << "table_departure_telemetry_bridge_tests: PASS\n";
        return 0;
    } catch (const std::exception& error) {
        std::cerr << "table_departure_telemetry_bridge_tests: FAIL: "
                  << error.what() << '\n';
        return 1;
    }
}
