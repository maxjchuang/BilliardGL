#include "ball_ball_contact.h"
#include "cushion_contact.h"
#include "game_state.h"
#include "physics.h"
#include "physics_telemetry_exact_test_helpers.h"
#include "pocket_boundary.h"
#include "surface_motion.h"

#include <cmath>
#include <cstdlib>
#include <iostream>
#include <limits>

namespace {

bool nearlyEqual(float a, float b, float epsilon = 0.001f)
{
    return std::fabs(a - b) < epsilon;
}

int fail(const char* message)
{
    std::cerr << message << std::endl;
    return EXIT_FAILURE;
}

}  // namespace

int main()
{
    {
        const auto profile = billiardgl::interactiveChinesePoolPhysicsProfile();
        billiardgl::GameState state;
        billiardgl::initializeBalls(state);
        for (auto& ball : state.balls) ball.pocketed = true;
        auto& airborne = state.balls[1];
        auto& cloth = state.balls[2];
        airborne.pocketed = cloth.pocketed = false;
        const float plane = profile.tableDeparture.clothSurfaceYCm + profile.ball.radiusCm;
        const double separation = 2.0 * profile.ball.radiusCm - 0.00109;
        airborne.position = {0.0f, plane + 0.1f, 0.0f};
        airborne.physicalDomain = billiardgl::PhysicalBallDomain::Airborne;
        cloth.position = {static_cast<float>(std::sqrt(separation * separation - 0.01)), plane, 0.0f};
        cloth.velocity = {1.0f, 0.0f, 0.0f};
        cloth.speed = 1.0f;
        const auto original = state;
        const auto correction = billiardgl::updatePhysics(state, 0.0f, profile);
        bool actualProjection = false;
        bool zeroImpulseContact = false;
        for (const auto& solver : correction.solverEvents)
            actualProjection = actualProjection || !solver.positionProjections.empty();
        for (const auto& contact : correction.contacts)
            zeroImpulseContact = zeroImpulseContact ||
                (contact.kind == billiardgl::PhysicsContactKind::BallBall &&
                 contact.firstBall == 1 && contact.secondBall == 2 &&
                 contact.normalImpulseNs == 0.0 && contact.positionCorrected);
        if (correction.stepStatus == billiardgl::PhysicsStepStatus::Failed ||
            !actualProjection || !zeroImpulseContact ||
            state.balls[1].position.x == original.balls[1].position.x)
            return fail("separating mixed overlap requires a real zero-time position correction receipt");
        const auto following = billiardgl::updatePhysics(state, 1.0f / 120.0f, profile);
        if (following.stepStatus == billiardgl::PhysicsStepStatus::Failed)
            return fail("corrected mixed overlap enters the swept domain query without relaxing penetration limits");
        auto invalid = original;
        invalid.balls[2].position.x = 0.1f;
        const auto invalidBefore = invalid;
        const auto rejected = billiardgl::updatePhysics(invalid, 1.0f / 120.0f, profile);
        if (rejected.stepStatus != billiardgl::PhysicsStepStatus::Failed ||
            rejected.failureCode != billiardgl::PhysicsFailureCode::PenetrationLimit ||
            !rejected.contacts.empty() || !rejected.solverEvents.empty() ||
            !rejected.analyticSurfaceMotion.empty() ||
            !billiardgl::test::detail::exactPoint(invalid.balls[1].position, invalidBefore.balls[1].position) ||
            !billiardgl::test::detail::exactPoint(invalid.balls[2].position, invalidBefore.balls[2].position))
            return fail("excessive mixed overlap still fails atomically without publishing a correction");
    }
    {
        auto profile = billiardgl::defaultChinesePoolPhysicsProfile();
        billiardgl::GameState state;
        billiardgl::initializeBalls(state);
        for (auto& ball : state.balls) ball.pocketed = true;
        auto& cue = state.balls[0];
        cue.pocketed = false;
        cue.position = {0.0f, billiardgl::kTableHeight + billiardgl::kBallRadius, 0.0f};
        cue.trajectoryGeneration = 7;
        const auto telemetry = billiardgl::updatePhysics(state, 1.0f / 120.0f, profile);
        if (telemetry.stepStatus == billiardgl::PhysicsStepStatus::Failed ||
            telemetry.analyticSurfaceMotion.empty())
            return fail("post-reset free motion emits an analytic segment");
        for (const auto& segment : telemetry.analyticSurfaceMotion)
            if (segment.ballIndex != 0 || segment.trajectoryGeneration != 7)
                return fail("discrete free motion retains the actual trajectory generation");
    }
    {
        auto profile = billiardgl::interactiveChinesePoolPhysicsProfile();
        billiardgl::GameState state;
        billiardgl::initializeBalls(state);
        for (auto& ball : state.balls) ball.pocketed = true;
        auto& ball = state.balls[1];
        ball.pocketed = false;
        ball.position = {0.0f, profile.tableDeparture.clothSurfaceYCm +
            profile.ball.radiusCm, 0.0f};
        ball.physicalDomain = billiardgl::PhysicalBallDomain::Airborne;
        ball.velocity = {0.0f, 0.00001f, 0.0f};
        state.ballsMoving = true;
        const auto telemetry = billiardgl::updatePhysics(state,
            profile.solver.timeStepSeconds, profile);
        bool landed = false;
        for (const auto& event : telemetry.tableDepartureEvents) {
            if (event.kind != billiardgl::TableDepartureEventKind::ClothReturn) continue;
            landed = true;
            if (event.linkedPhysicsContactOrdinal < 0 ||
                event.linkedPhysicsContactOrdinal >= static_cast<int>(telemetry.contacts.size()))
                return fail("sub-tolerance airborne return retains a real support receipt");
            const auto& contact = telemetry.contacts[event.linkedPhysicsContactOrdinal];
            if (contact.kind != billiardgl::PhysicsContactKind::Support ||
                contact.firstBall != event.ballIndex || contact.solverEventId != event.solverEventId)
                return fail("sub-tolerance landing receipt binds the actual support solver");
        }
        if (telemetry.stepStatus == billiardgl::PhysicsStepStatus::Failed || !landed)
            return fail("tiny airborne excursion completes through the canonical landing island");
    }
    {
        billiardgl::GameState state;
        billiardgl::initializeBalls(state);
        for (auto& ball : state.balls) ball.pocketed = true;
        auto& removed = state.balls[1];
        removed.pocketed = false;
        removed.outOfPlay = true;
        removed.physicalDomain = billiardgl::PhysicalBallDomain::OutsideLandingSupported;
        removed.position = {200.0f, 2.8575f, 0.0f};
        const auto before = removed;
        const auto telemetry = billiardgl::updatePhysics(state, 0.1f,
            billiardgl::interactiveChinesePoolPhysicsProfile());
        if (telemetry.stepStatus == billiardgl::PhysicsStepStatus::Failed ||
            !telemetry.contacts.empty() || !telemetry.tableDepartureEvents.empty() ||
            !telemetry.analyticSurfaceMotion.empty() || removed.pocketed || !removed.outOfPlay ||
            !billiardgl::test::detail::exactPoint(removed.position, before.position) || state.ballsMoving)
            return fail("adjudicated off-table ball is inactive without a fake pocket capture or storage relocation");
    }
    for (int ballId : {0, 1}) for (int pocketId : {0, 1, 2, 3, 4, 5}) {
        const auto profile = billiardgl::interactiveChinesePoolPhysicsProfile();
        const auto frames = billiardgl::buildPocketBoundaryFrames(profile.tableBoundary);
        billiardgl::GameState state;
        billiardgl::initializeBalls(state);
        for (auto& ball : state.balls) ball.pocketed = true;
        auto& ball = state.balls[ballId];
        ball.pocketed = false;
        ball.physicalDomain = billiardgl::PhysicalBallDomain::Airborne;
        ball.position = {frames[pocketId].mouthCenter.x,
            profile.tableDeparture.clothSurfaceYCm + profile.ball.radiusCm + 1.0f,
            frames[pocketId].mouthCenter.z};
        ball.velocity = {0.0f, -20.0f, 0.0f};
        ball.speed = 20.0f;
        ball.trajectoryGeneration = 9;
        state.ballsMoving = true;
        auto replay = state;
        bool entry = false;
        bool capture = false;
        bool throat = false;
        int entryTick = -1;
        for (int tick = 0; tick < 120 && !capture; ++tick) {
            const auto telemetry = billiardgl::updatePhysics(state, 1.0f / 120.0f, profile);
            const auto replayTelemetry = billiardgl::updatePhysics(replay, 1.0f / 120.0f, profile);
            if (!billiardgl::test::exactPhysicsStepTelemetry(telemetry, replayTelemetry) ||
                !billiardgl::test::detail::exactPoint(ball.position, replay.balls[ballId].position) ||
                !billiardgl::test::detail::exactPoint(ball.velocity, replay.balls[ballId].velocity) ||
                ball.trajectoryGeneration != replay.balls[ballId].trajectoryGeneration)
                return fail("vertical channel public replay preserves exact states and complete telemetry");
            if (telemetry.stepStatus == billiardgl::PhysicsStepStatus::Failed) {
                std::cerr << "vertical pocket tick=" << tick << ' ' << telemetry.failureDetail << '\n';
                return fail("vertical pocket descent must remain a successful physical trajectory");
            }
            for (const auto& event : telemetry.tableDepartureEvents) {
                if (event.kind != billiardgl::TableDepartureEventKind::PocketEntry) continue;
                entry = true;
                entryTick = tick;
            }
            for (const auto& contact : telemetry.contacts) {
                if (contact.kind != billiardgl::PhysicsContactKind::Pocket) continue;
                if (contact.pocketBoundaryEvent == billiardgl::PocketBoundaryEventKind::Throat)
                    throat = true;
                if (contact.pocketBoundaryEvent != billiardgl::PocketBoundaryEventKind::Capture) continue;
                capture = contact.firstBall == ballId && contact.pocketId == pocketId &&
                    contact.pocketCaptureSequence != 0 && contact.pocketVerticalShaft &&
                    contact.trajectoryGenerationBefore == 9 && throat && tick > entryTick &&
                    nearlyEqual(contact.firstPositionAtContactCm.y,
                        profile.tableDeparture.clothSurfaceYCm - profile.ball.radiusCm);
                if (ballId == 0 && (!contact.administrativeResetAfterCapture ||
                        contact.trajectoryGenerationAfter != contact.trajectoryGenerationBefore + 1))
                    return fail("vertical cue capture publishes its actual reset generation");
            }
        }
        if (!entry || !capture || (ballId != 0 && !ball.pocketed) ||
            (ballId == 0 && (ball.pocketed || ball.physicalDomain !=
                billiardgl::PhysicalBallDomain::ClothSupported)))
            return fail("vertical pocket entry progresses to a real canonical capture instead of freezing at the mouth");
    }
    {
        const auto profile = billiardgl::interactiveChinesePoolPhysicsProfile();
        billiardgl::GameState state;
        billiardgl::initializeBalls(state);
        for (auto& ball : state.balls) ball.pocketed = true;
        auto& ball = state.balls[1];
        ball.pocketed = false;
        ball.physicalDomain = billiardgl::PhysicalBallDomain::Airborne;
        ball.position = {profile.tableBoundary.playfieldWidthCm * 0.5f -
            profile.ball.radiusCm - 0.1f,
            profile.tableDeparture.clothSurfaceYCm + profile.ball.radiusCm + 0.2f,
            30.0f};
        ball.velocity = {100.0f, 0.0f, 0.0f};
        ball.speed = 100.0f;
        state.ballsMoving = true;
        const auto telemetry = billiardgl::updatePhysics(state, 0.002f, profile);
        bool linkedRailImpulse = false;
        for (const auto& event : telemetry.tableDepartureEvents) {
            if (event.kind != billiardgl::TableDepartureEventKind::CushionNoseContact)
                continue;
            const int index = event.linkedPhysicsContactOrdinal;
            if (index < 0 || index >= static_cast<int>(telemetry.contacts.size())) continue;
            const auto& contact = telemetry.contacts[index];
            linkedRailImpulse = contact.kind == billiardgl::PhysicsContactKind::Rail &&
                contact.firstBall == 1 && contact.boundaryFeatureId == 1 &&
                contact.velocityImpulseApplied && contact.normalImpulseNs > 0.0 &&
                contact.solverEventId == event.solverEventId &&
                contact.wholeTickTimeOfImpactSeconds == event.absoluteWholeTickTimeSeconds &&
                contact.wholeTickTimeOfImpactSeconds > 0.0;
        }
        if (telemetry.stepStatus == billiardgl::PhysicsStepStatus::Failed ||
            !linkedRailImpulse || !(ball.velocity.x < 0.0f) || !state.events.railCollision)
            return fail("airborne nose collision uses authoritative cushion impulse and exact private event link");
    }
    {
        billiardgl::GameState supported;
        billiardgl::initializeBalls(supported);
        for (auto& ball : supported.balls) ball.pocketed = true;
        auto& ball = supported.balls[1];
        const auto profile = billiardgl::interactiveChinesePoolPhysicsProfile();
        ball.pocketed = false;
        ball.position = billiardgl::Point3{0.0f,
            profile.tableDeparture.clothSurfaceYCm + profile.ball.radiusCm, 0.0f};
        ball.velocity = billiardgl::Point3{0.0f, -10.0f, 0.0f};
        ball.speed = 10.0f;
        ball.physicalDomain = billiardgl::PhysicalBallDomain::ClothSupported;
        const auto telemetry = billiardgl::updatePhysics(supported, 0.01f, profile);
        bool supportImpulse = false;
        for (const auto& contact : telemetry.contacts) {
            if (contact.kind == billiardgl::PhysicsContactKind::Support &&
                contact.firstBall == 1 && contact.velocityImpulseApplied &&
                contact.wholeTickTimeOfImpactSeconds == 0.0 &&
                contact.normalImpulseNs > 0.0)
                supportImpulse = true;
        }
        if (telemetry.stepStatus == billiardgl::PhysicsStepStatus::Failed ||
            !supportImpulse || ball.velocity.y != 0.0f ||
            supported.events.railCollision || telemetry.solverEvents.empty()) {
            std::cerr << telemetry.failureDetail << '\n';
            return fail("supported downward velocity produces an authoritative support impulse, never a silent clamp or rail event");
        }
    }
    {
        billiardgl::GameState state;
        billiardgl::initializeBalls(state);
        for(int i=0;i<billiardgl::kBallCount;++i)state.balls[i].pocketed=true;
        state.balls[1].pocketed=false;
        const auto profile=billiardgl::defaultChinesePoolPhysicsProfile();
        state.balls[1].position.x=-profile.tableBoundary.playfieldWidthCm*0.5f+
            profile.ball.radiusCm;
        state.balls[1].velocity.x=-100.0f;state.balls[1].speed=100.0f;
        state.ballsMoving=true;
        const auto telemetry=billiardgl::updatePhysics(state,0.01f,profile);
        bool straight=false;
        for(const auto& c:telemetry.contacts)straight=straight||
            (c.kind==billiardgl::PhysicsContactKind::Rail&&c.firstBall==1&&
             c.boundaryFeatureId==0&&c.velocityImpulseApplied);
        if(!straight)return fail("D5-B straight cushion contact preserves exact feature id");
    }
    {
        billiardgl::GameState landing;
        billiardgl::initializeBalls(landing);
        for (auto& ball : landing.balls) ball.pocketed = true;
        const auto profile = billiardgl::interactiveChinesePoolPhysicsProfile();
        const float plane = profile.tableDeparture.clothSurfaceYCm + profile.ball.radiusCm;
        auto& falling = landing.balls[1];
        auto& supported = landing.balls[2];
        falling.pocketed = supported.pocketed = false;
        falling.position = billiardgl::Point3{0.0f, plane, 0.0f};
        supported.position = billiardgl::Point3{2.0f * profile.ball.radiusCm, plane, 0.0f};
        falling.velocity = billiardgl::Point3{10.0f, -10.0f, 0.0f};
        falling.speed = std::sqrt(200.0f);
        falling.physicalDomain = billiardgl::PhysicalBallDomain::Airborne;
        supported.velocity = billiardgl::Point3{};
        supported.speed = 0.0f;
        supported.physicalDomain = billiardgl::PhysicalBallDomain::ClothSupported;
        const auto telemetry = billiardgl::updatePhysics(landing, 0.001f, profile);
        bool coupled = false;
        for (const auto& support : telemetry.contacts) {
            if (support.kind != billiardgl::PhysicsContactKind::Support ||
                support.firstBall != 1 || !(support.normalImpulseNs > 0.0) ||
                support.wholeTickTimeOfImpactSeconds != 0.0) continue;
            for (const auto& pair : telemetry.contacts) {
                if (pair.kind == billiardgl::PhysicsContactKind::BallBall &&
                    pair.firstBall == 1 && pair.secondBall == 2 &&
                    pair.solverEventId == support.solverEventId &&
                    pair.solverIslandId == support.solverIslandId)
                    coupled = true;
            }
        }
        bool landingReceiptBound = false;
        for (const auto& event : telemetry.tableDepartureEvents) {
            if (event.kind != billiardgl::TableDepartureEventKind::ClothReturn ||
                event.ballIndex != 1 || event.sourceDomain !=
                    billiardgl::PhysicalBallDomain::Airborne ||
                event.trajectoryGeneration != falling.trajectoryGeneration ||
                event.linkedPhysicsContactOrdinal < 0 ||
                static_cast<std::size_t>(event.linkedPhysicsContactOrdinal) >=
                    telemetry.contacts.size()) continue;
            const auto& support = telemetry.contacts[event.linkedPhysicsContactOrdinal];
            landingReceiptBound = support.kind == billiardgl::PhysicsContactKind::Support &&
                support.firstBall == event.ballIndex &&
                support.solverEventId == event.solverEventId &&
                support.wholeTickTimeOfImpactSeconds == event.absoluteWholeTickTimeSeconds;
        }
        if (telemetry.stepStatus == billiardgl::PhysicsStepStatus::Failed ||
            !coupled || !landingReceiptBound) {
            std::cerr << telemetry.failureDetail << '\n';
            return fail("simultaneous landing and ball contact preserve incoming vertical velocity in one authoritative island");
        }
    }
    billiardgl::GameState state;
    billiardgl::initializeBalls(state);
    if (state.balls[0].position.y != billiardgl::kTableHeight + billiardgl::kBallRadius) {
        return fail("cue ball should start on the table");
    }

    billiardgl::GameState profiledMotion;
    billiardgl::initializeBalls(profiledMotion);
    for (int index = 1; index < billiardgl::kBallCount; ++index) {
        profiledMotion.balls[index].pocketed = true;
    }
    profiledMotion.balls[0].position.z = 20.0f;
    profiledMotion.balls[0].velocity.x = 20.0f;
    profiledMotion.balls[0].speed = 20.0f;
    profiledMotion.balls[0].angularVelocity.z =
        -20.0f / billiardgl::kBallRadius;
    profiledMotion.balls[0].motionState = billiardgl::BallMotionState::Rolling;
    billiardgl::PhysicsProfile motionProfile =
        billiardgl::phase3V5CandidatePhysicsProfile();
    motionProfile.surface.rollingResistanceAccelerationCmS2 = 12.5f;
    billiardgl::BallState expectedMotion = profiledMotion.balls[0];
    billiardgl::advanceSurfaceMotion(
        expectedMotion, 0.1f, motionProfile.ball, motionProfile.surface);
    const billiardgl::PhysicsStepTelemetry motionTelemetry =
        billiardgl::updatePhysics(profiledMotion, 0.1f, motionProfile);
    if (!nearlyEqual(profiledMotion.balls[0].position.x, expectedMotion.position.x) ||
        !nearlyEqual(profiledMotion.balls[0].velocity.x, expectedMotion.velocity.x) ||
        !nearlyEqual(profiledMotion.balls[0].angularVelocity.z,
            expectedMotion.angularVelocity.z)) {
        return fail("production update should use profile-based surface motion");
    }
    if (motionTelemetry.surfaceMotion.size() != 1 ||
        motionTelemetry.surfaceMotion[0].ballIndex != 0) {
        return fail("production update should retain per-ball surface telemetry");
    }

    state.balls[0].position.x = billiardgl::kTableInWidth / 2.0f;
    state.balls[0].velocity.x = 5.0f;
    billiardgl::collideWithTableEdge(state.balls[0]);
    if (!(state.balls[0].velocity.x < 0.0f)) {
        return fail("wall collision should reverse x velocity");
    }

    billiardgl::PhysicsProfile cushionProfile =
        billiardgl::phase3V5CandidatePhysicsProfile();
    cushionProfile.cushion.normalRestitution = 0.5f;
    cushionProfile.cushion.restitutionIntercept = 0.5f;
    cushionProfile.cushion.minimumRestitution = 0.5f;
    cushionProfile.cushion.maximumRestitution = 0.5f;
    cushionProfile.cushion.frictionCoefficient = 0.2f;
    cushionProfile.cushion.noseHeightRatio = 1.4f;
    billiardgl::GameState cushionState;
    billiardgl::initializeBalls(cushionState);
    for (int index = 1; index < billiardgl::kBallCount; ++index) {
        cushionState.balls[index].pocketed = true;
    }
    const float xLimit = billiardgl::kTableInWidth / 2.0f -
        cushionProfile.ball.radiusCm;
    cushionState.balls[0].position = billiardgl::Point3{
        xLimit, billiardgl::kTableHeight + cushionProfile.ball.radiusCm, 25.0f};
    billiardgl::setBallVelocity(cushionState.balls[0], 100.0f, 0.0f, 30.0f);
    billiardgl::BallState directCushion = cushionState.balls[0];
    const billiardgl::CushionContactResult directRail =
        billiardgl::resolveCushionContact(
            directCushion, billiardgl::Point3{-1.0f, 0.0f, 0.0f}, 0.0,
            cushionProfile.ball, cushionProfile.cushion);
    const billiardgl::PhysicsStepTelemetry cushionTelemetry =
        billiardgl::updatePhysics(cushionState, 0.0f, cushionProfile);
    if (!directRail.velocityImpulseApplied ||
        !nearlyEqual(cushionState.balls[0].velocity.x, directCushion.velocity.x) ||
        !nearlyEqual(cushionState.balls[0].velocity.z, directCushion.velocity.z) ||
        !nearlyEqual(cushionState.balls[0].angularVelocity.y,
            directCushion.angularVelocity.y) ||
        !nearlyEqual(cushionState.balls[0].angularVelocity.z,
            directCushion.angularVelocity.z)) {
        return fail("production rail collision must match the standalone cushion model");
    }
    if (cushionTelemetry.contacts.size() != 1 ||
        cushionTelemetry.contacts[0].kind != billiardgl::PhysicsContactKind::Rail ||
        !nearlyEqual(static_cast<float>(cushionTelemetry.contacts[0].normalImpulseNs),
            static_cast<float>(directRail.normalImpulseNs))) {
        return fail("production rail telemetry must use the authoritative cushion impulse");
    }

    billiardgl::GameState sweptState;
    billiardgl::initializeBalls(sweptState);
    for (int index = 1; index < billiardgl::kBallCount; ++index) {
        sweptState.balls[index].pocketed = true;
    }
    sweptState.balls[0].position = billiardgl::Point3{
        xLimit - 10.0f, billiardgl::kTableHeight + cushionProfile.ball.radiusCm,
        25.0f};
    billiardgl::setBallVelocity(sweptState.balls[0], 500.0f, 0.0f, 0.0f);
    const billiardgl::PhysicsStepTelemetry sweptTelemetry =
        billiardgl::updatePhysics(sweptState, 0.1f, cushionProfile);
    int sweptRailContacts = 0;
    for (const auto& contact : sweptTelemetry.contacts)
        if (contact.kind == billiardgl::PhysicsContactKind::Rail)
            ++sweptRailContacts;
    if (!(sweptState.balls[0].velocity.x < 0.0f) ||
        sweptState.balls[0].position.x > xLimit + 0.001f ||
        sweptTelemetry.stepStatus == billiardgl::PhysicsStepStatus::Failed ||
        sweptRailContacts != 1) {
        return fail("swept rail contact must stop high-speed tunneling within one tick");
    }
    const billiardgl::PhysicsStepTelemetry adjacentRailTelemetry =
        billiardgl::updatePhysics(sweptState, 0.001f, cushionProfile);
    if (!adjacentRailTelemetry.contacts.empty()) {
        return fail("a swept rail rebound must not repeat on the adjacent tick");
    }

    billiardgl::GameState recedingRail = sweptState;
    recedingRail.balls[0].position.x = xLimit + 0.1f;
    billiardgl::setBallVelocity(recedingRail.balls[0], -20.0f, 0.0f, 0.0f);
    const billiardgl::PhysicsStepTelemetry recedingRailTelemetry =
        billiardgl::updatePhysics(recedingRail, 0.0f, cushionProfile);
    if (!nearlyEqual(recedingRail.balls[0].velocity.x, -20.0f) ||
        (!recedingRailTelemetry.contacts.empty() &&
         recedingRailTelemetry.contacts[0].normalImpulseNs > 0.0)) {
        return fail("receding rail overlap must not receive a duplicate velocity impulse");
    }

    billiardgl::GameState leftMirror;
    billiardgl::initializeBalls(leftMirror);
    for (int index = 1; index < billiardgl::kBallCount; ++index) {
        leftMirror.balls[index].pocketed = true;
    }
    leftMirror.balls[0].position = billiardgl::Point3{
        -xLimit, billiardgl::kTableHeight + cushionProfile.ball.radiusCm, 25.0f};
    billiardgl::setBallVelocity(leftMirror.balls[0], -100.0f, 0.0f, 30.0f);
    billiardgl::updatePhysics(leftMirror, 0.0f, cushionProfile);
    if (!nearlyEqual(cushionState.balls[0].velocity.x,
            -leftMirror.balls[0].velocity.x) ||
        !nearlyEqual(cushionState.balls[0].velocity.z,
            leftMirror.balls[0].velocity.z)) {
        return fail("opposite production rails must preserve mirror equivalence");
    }

    state.balls[1].position = billiardgl::Point3{0.0f, billiardgl::kTableHeight + billiardgl::kBallRadius, 0.0f};
    state.balls[2].position = billiardgl::Point3{2.0f * billiardgl::kBallRadius - 0.6f, billiardgl::kTableHeight + billiardgl::kBallRadius, 0.0f};
    state.balls[1].velocity.x = 10.0f;
    state.balls[2].velocity.x = 0.0f;
    billiardgl::collideBalls(state.balls[1], state.balls[2]);
    if (!(state.balls[2].velocity.x > 0.0f)) {
        return fail("ball collision should transfer velocity");
    }

    billiardgl::PhysicsProfile contactProfile =
        billiardgl::phase3V5CandidatePhysicsProfile();
    contactProfile.ball.normalRestitution = 0.5f;
    contactProfile.ball.frictionCoefficient = 0.2f;
    billiardgl::GameState contactState;
    billiardgl::initializeBalls(contactState);
    for (int index = 2; index < billiardgl::kBallCount; ++index) {
        contactState.balls[index].pocketed = true;
    }
    const float contactCenterY = contactProfile.tableDeparture.clothSurfaceYCm +
        contactProfile.ball.radiusCm;
    contactState.balls[0].position = billiardgl::Point3{0.0f, contactCenterY, 20.0f};
    contactState.balls[1].position = billiardgl::Point3{5.7f, contactCenterY, 20.0f};
    billiardgl::setBallVelocity(contactState.balls[0], 100.0f, 0.0f, 30.0f);
    billiardgl::BallState directFirst = contactState.balls[0];
    billiardgl::BallState directSecond = contactState.balls[1];
    const billiardgl::BallBallContactResult direct =
        billiardgl::resolveBallBallContact(directFirst, directSecond,
            contactProfile.ball, contactProfile.ball);
    const billiardgl::PhysicsStepTelemetry contactTelemetry =
        billiardgl::updatePhysics(contactState, 0.0f, contactProfile);
    if (!direct.velocityImpulseApplied ||
        !nearlyEqual(contactState.balls[0].velocity.x, directFirst.velocity.x) ||
        !nearlyEqual(contactState.balls[0].velocity.z, directFirst.velocity.z) ||
        !nearlyEqual(contactState.balls[1].velocity.x, directSecond.velocity.x) ||
        !nearlyEqual(contactState.balls[1].velocity.z, directSecond.velocity.z)) {
        return fail("production collision must match the standalone contact model");
    }
    int authoritativePairContacts = 0;
    bool authoritativePairImpulse = false;
    for (const auto& contact : contactTelemetry.contacts) {
        if (contact.kind != billiardgl::PhysicsContactKind::BallBall) continue;
        ++authoritativePairContacts;
        authoritativePairImpulse = nearlyEqual(
            static_cast<float>(contact.normalImpulseNs),
            static_cast<float>(direct.normalImpulseNs));
    }
    if (authoritativePairContacts != 1 || !authoritativePairImpulse) {
        return fail("production collision telemetry must use the authoritative impulse");
    }
    if (!nearlyEqual(contactState.balls[0].velocity.x, 25.0f) ||
        !nearlyEqual(contactState.balls[1].velocity.x, 75.0f)) {
        return fail("profile restitution must change production normal motion");
    }
    billiardgl::GameState frictionlessState;
    billiardgl::initializeBalls(frictionlessState);
    for (int index = 2; index < billiardgl::kBallCount; ++index) {
        frictionlessState.balls[index].pocketed = true;
    }
    frictionlessState.balls[0].position = billiardgl::Point3{0.0f, 92.715f, 20.0f};
    frictionlessState.balls[1].position = billiardgl::Point3{5.7f, 92.715f, 20.0f};
    billiardgl::setBallVelocity(frictionlessState.balls[0], 100.0f, 0.0f, 30.0f);
    billiardgl::PhysicsProfile frictionlessProfile = contactProfile;
    frictionlessProfile.ball.frictionCoefficient = 0.0f;
    billiardgl::updatePhysics(frictionlessState, 0.0f, frictionlessProfile);
    if (nearlyEqual(contactState.balls[0].velocity.z,
            frictionlessState.balls[0].velocity.z) ||
        nearlyEqual(contactState.balls[1].velocity.z,
            frictionlessState.balls[1].velocity.z)) {
        return fail("profile friction must change production tangential motion");
    }

    billiardgl::GameState recedingState = contactState;
    recedingState.balls[0].position.x = 0.0f;
    recedingState.balls[1].position.x = 5.0f;
    billiardgl::setBallVelocity(recedingState.balls[0], -20.0f, 0.0f, 0.0f);
    billiardgl::setBallVelocity(recedingState.balls[1], 20.0f, 0.0f, 0.0f);
    billiardgl::updatePhysics(recedingState, 0.0f, contactProfile);
    if (!nearlyEqual(recedingState.balls[0].velocity.x, -20.0f) ||
        !nearlyEqual(recedingState.balls[1].velocity.x, 20.0f)) {
        return fail("receding overlap must not receive a velocity impulse");
    }

    const float separatedFirstVelocity = contactState.balls[0].velocity.x;
    const float separatedSecondVelocity = contactState.balls[1].velocity.x;
    billiardgl::updatePhysics(contactState, 0.0f, contactProfile);
    if (!nearlyEqual(contactState.balls[0].velocity.x, separatedFirstVelocity) ||
        !nearlyEqual(contactState.balls[1].velocity.x, separatedSecondVelocity)) {
        return fail("persistent contact must not receive a duplicate impulse");
    }

    billiardgl::BallState radiusFirst = contactState.balls[0];
    billiardgl::BallState radiusSecond = contactState.balls[1];
    radiusFirst.position.x = 0.0f;
    radiusSecond.position.x = 4.1f;
    billiardgl::setBallVelocity(radiusFirst, 10.0f, 0.0f, 0.0f);
    billiardgl::setBallVelocity(radiusSecond, 0.0f, 0.0f, 0.0f);
    contactProfile.ball.radiusCm = 2.1f;
    if (!billiardgl::collideBalls(radiusFirst, radiusSecond, contactProfile)) {
        return fail("profile-aware collision must use the configured radius");
    }

    const billiardgl::PhysicsProfile pocketProfile =
        billiardgl::phase3V5CandidatePhysicsProfile();
    const std::array<billiardgl::PocketBoundaryFrame, 6> pocketFrames =
        billiardgl::buildPocketBoundaryFrames(pocketProfile.tableBoundary);
    state.balls[3].position.x = pocketFrames[0].mouthCenter.x -
        pocketFrames[0].inward.x * pocketFrames[0].captureDepthCm;
    state.balls[3].position.z = pocketFrames[0].mouthCenter.z -
        pocketFrames[0].inward.z * pocketFrames[0].captureDepthCm;
    if (!billiardgl::isInPocket(state.balls[3])) {
        return fail("ball crossing a corner pocket mouth should be detected as pocketed");
    }

    state.balls[4].position.x = pocketFrames[4].mouthCenter.x -
        pocketFrames[4].inward.x * pocketFrames[4].captureDepthCm;
    state.balls[4].position.z = pocketFrames[4].mouthCenter.z;
    if (!billiardgl::isInPocket(state.balls[4])) {
        return fail("ball crossing a side pocket mouth should be detected as pocketed");
    }

    state.balls[5].position.x = billiardgl::kTableInWidth / 2.0f + 1.0f;
    state.balls[5].position.z = 25.0f;
    if (billiardgl::isInPocket(state.balls[5])) {
        return fail("ball outside a pocket mouth should not be pocketed");
    }

    billiardgl::GameState pocketState;
    billiardgl::initializeBalls(pocketState);
    pocketState.balls[0].pocketInteraction.phase =
        billiardgl::PocketInteractionPhase::Captured;
    pocketState.balls[0].pocketInteraction.pocketId = 0;
    pocketState.balls[0].pocketInteraction.captureSequence = 1;
    if (!billiardgl::updatePocketedBall(pocketState, 0)) {
        return fail("cue ball at pocket center should be updated as pocketed");
    }
    if (!pocketState.players.illegalShot || !pocketState.events.cueBallPocketed) {
        return fail("cue ball pocket should mark illegal shot and cue pocket event");
    }

    billiardgl::GameState eightState;
    billiardgl::initializeBalls(eightState);
    eightState.balls[8].pocketInteraction.phase =
        billiardgl::PocketInteractionPhase::Captured;
    eightState.balls[8].pocketInteraction.pocketId = 5;
    eightState.balls[8].pocketInteraction.captureSequence = 1;
    if (!billiardgl::updatePocketedBall(eightState, 8)) {
        return fail("eight ball at pocket center should be updated as pocketed");
    }
    if (!eightState.gameOver || !eightState.events.eightBallPocketed) {
        return fail("eight ball pocket should mark game over and eight ball event");
    }

    billiardgl::BallState sideMouthBall;
    sideMouthBall.position = billiardgl::Point3{
        -billiardgl::kTableInWidth / 2.0f - 0.5f,
        billiardgl::kTableHeight + billiardgl::kBallRadius,
        0.0f};
    sideMouthBall.velocity.x = -5.0f;
    billiardgl::collideWithTableEdge(sideMouthBall);
    if (!(sideMouthBall.velocity.x < 0.0f)) {
        return fail("ball inside a pocket mouth should not bounce off the ordinary rail");
    }

    billiardgl::BallState ordinaryRailBall;
    ordinaryRailBall.position = billiardgl::Point3{
        billiardgl::kTableInWidth / 2.0f + 0.5f,
        billiardgl::kTableHeight + billiardgl::kBallRadius,
        25.0f};
    ordinaryRailBall.velocity.x = 5.0f;
    billiardgl::collideWithTableEdge(ordinaryRailBall);
    if (!(ordinaryRailBall.velocity.x < 0.0f)) {
        return fail("ball outside a pocket mouth should bounce off the ordinary rail");
    }

    billiardgl::GameState sweptPocketState;
    billiardgl::initializeBalls(sweptPocketState);
    for (int index = 1; index < billiardgl::kBallCount; ++index) {
        sweptPocketState.balls[index].pocketed = true;
    }
    sweptPocketState.balls[0].position = billiardgl::Point3{
        -billiardgl::kTableInWidth / 2.0f + billiardgl::kBallRadius,
        billiardgl::kTableHeight + billiardgl::kBallRadius, 0.0f};
    sweptPocketState.balls[0].velocity.x = -400.0f;
    sweptPocketState.balls[0].speed = 400.0f;
    sweptPocketState.ballsMoving = true;
    const billiardgl::PhysicsStepTelemetry pocketTelemetry =
        billiardgl::updatePhysics(sweptPocketState, 0.1f, pocketProfile);
    if (!sweptPocketState.events.cueBallPocketed ||
        sweptPocketState.nextPocketCaptureSequence != 2) {
        std::cerr << "swept capture failure=" << int(pocketTelemetry.failureCode)
                  << " detail=" << pocketTelemetry.failureDetail << '\n';
        return fail("swept side-pocket capture should not tunnel at high speed");
    }
    int captureContacts = 0;
    for (const billiardgl::PhysicsContactRecord& contact : pocketTelemetry.contacts) {
        if (contact.kind == billiardgl::PhysicsContactKind::Pocket &&
            contact.pocketBoundaryEvent ==
                billiardgl::PocketBoundaryEventKind::Capture) {
            ++captureContacts;
        }
    }
    if (captureContacts != 1) {
        return fail("a swept capture should produce exactly one capture contact");
    }
    billiardgl::updatePhysics(sweptPocketState, 0.1f, pocketProfile);
    if (sweptPocketState.events.cueBallPocketed ||
        sweptPocketState.nextPocketCaptureSequence != 2) {
        return fail("cue-ball replacement must not duplicate its capture event");
    }

    billiardgl::GameState rejectedPocketState;
    billiardgl::initializeBalls(rejectedPocketState);
    for (int index = 1; index < billiardgl::kBallCount; ++index) {
        rejectedPocketState.balls[index].pocketed = true;
    }
    rejectedPocketState.balls[0].position = billiardgl::Point3{
        -billiardgl::kTableInWidth / 2.0f + billiardgl::kBallRadius,
        billiardgl::kTableHeight + billiardgl::kBallRadius, 25.0f};
    rejectedPocketState.balls[0].velocity.x = -100.0f;
    rejectedPocketState.balls[0].speed = 100.0f;
    billiardgl::updatePhysics(rejectedPocketState, 0.1f, pocketProfile);
    if (rejectedPocketState.events.cueBallPocketed ||
        !(rejectedPocketState.balls[0].velocity.x > 0.0f)) {
        return fail("off-mouth trajectories should rebound from visible straight rail");
    }

    for (billiardgl::BallState& ball : state.balls) {
        ball.speed = 0.0f;
    }
    state.balls[4].speed = 1.0f;
    if (!billiardgl::anyBallMoving(state)) {
        return fail("anyBallMoving should detect active ball speed");
    }
    state.balls[4].speed = 0.0f;
    if (billiardgl::anyBallMoving(state)) {
        return fail("anyBallMoving should be false when all speeds are zero");
    }

    billiardgl::PhysicsProfile eventProfile =
        billiardgl::phase3V5CandidatePhysicsProfile();
    eventProfile.ball.normalRestitution = 1.0f;
    eventProfile.ball.frictionCoefficient = 0.0f;
    eventProfile.surface.slidingFrictionCoefficient = 0.0f;
    eventProfile.surface.rollingResistanceAccelerationCmS2 = 0.0f;
    eventProfile.surface.legacyFrictionAccelerationCmS2 = 0.0f;
    billiardgl::GameState highSpeed;
    billiardgl::initializeBalls(highSpeed);
    for (billiardgl::BallState& ball : highSpeed.balls) ball.pocketed = true;
    highSpeed.balls[0].pocketed = false;
    highSpeed.balls[1].pocketed = false;
    highSpeed.balls[0].position.z = highSpeed.balls[1].position.z = 0.0f;
    highSpeed.balls[0].position.x = -50.0f;
    highSpeed.balls[1].position.x = 50.0f;
    highSpeed.balls[0].velocity.x = 1000.0f;
    highSpeed.balls[0].speed = 1000.0f;
    const billiardgl::PhysicsStepTelemetry highSpeedTelemetry =
        billiardgl::updatePhysics(highSpeed, 0.1f, eventProfile);
    if (!(highSpeed.balls[0].velocity.x < 0.01f &&
          highSpeed.balls[1].velocity.x > 999.0f)) {
        return fail("event-driven stepper should prevent high-speed ball tunneling");
    }
    int continuousImpulses = 0;
    for (const billiardgl::PhysicsContactRecord& contact : highSpeedTelemetry.contacts) {
        if (contact.kind == billiardgl::PhysicsContactKind::BallBall &&
            contact.velocityImpulseApplied) ++continuousImpulses;
    }
    if (continuousImpulses != 1) {
        return fail("continuous crossing should receive exactly one velocity impulse");
    }

    billiardgl::GameState ballRail;
    for (billiardgl::BallState& ball : ballRail.balls) ball.pocketed = true;
    ballRail.balls[0].pocketed = ballRail.balls[1].pocketed = false;
    const float rightLimit = billiardgl::kTableInWidth * 0.5f -
        eventProfile.ball.radiusCm;
    ballRail.balls[0].position = billiardgl::Point3{
        rightLimit, billiardgl::kTableHeight + eventProfile.ball.radiusCm, 25.0f};
    ballRail.balls[1].position = billiardgl::Point3{
        rightLimit - 2.0f * eventProfile.ball.radiusCm,
        billiardgl::kTableHeight + eventProfile.ball.radiusCm, 25.0f};
    ballRail.balls[0].velocity.x = 100.0f;
    ballRail.balls[1].velocity.x = 200.0f;
    ballRail.balls[0].speed = 100.0f;
    ballRail.balls[1].speed = 200.0f;
    auto coupledProfile = billiardgl::interactiveChinesePoolPhysicsProfile();
    coupledProfile.ball.normalRestitution = eventProfile.ball.normalRestitution;
    coupledProfile.ball.frictionCoefficient = eventProfile.ball.frictionCoefficient;
    coupledProfile.surface = eventProfile.surface;
    const billiardgl::PhysicsStepTelemetry ballRailTelemetry =
        billiardgl::updatePhysics(ballRail, 0.01f, coupledProfile);
    int ballRailPairs = 0, ballRailRails = 0, ballRailSupports = 0;
    bool oneBallRailIsland = !ballRailTelemetry.contacts.empty();
    for (const auto& contact : ballRailTelemetry.contacts) {
        ballRailPairs += contact.kind == billiardgl::PhysicsContactKind::BallBall;
        ballRailRails += contact.kind == billiardgl::PhysicsContactKind::Rail;
        ballRailSupports += contact.kind == billiardgl::PhysicsContactKind::Support;
        oneBallRailIsland = oneBallRailIsland &&
            contact.solverEventId == ballRailTelemetry.contacts.front().solverEventId &&
            contact.solverIslandId == ballRailTelemetry.contacts.front().solverIslandId;
    }
    if (ballRailTelemetry.solverEvents.size() != 1 ||
        ballRailTelemetry.solverEvents[0].contactCount != 4 ||
        ballRailPairs != 1 || ballRailRails != 1 || ballRailSupports != 2 ||
        !oneBallRailIsland) {
        return fail("same-TOI ball and straight-rail contacts must solve in one island");
    }

    billiardgl::GameState failedPhysicalBatch;
    for (billiardgl::BallState& ball : failedPhysicalBatch.balls)
        ball.pocketed = true;
    for (int index = 0; index < 3; ++index)
        failedPhysicalBatch.balls[index].pocketed = false;
    failedPhysicalBatch.balls[0].position = billiardgl::Point3{
        0.0f, billiardgl::kTableHeight + eventProfile.ball.radiusCm, 25.0f};
    failedPhysicalBatch.balls[1].position = billiardgl::Point3{
        1.0f, billiardgl::kTableHeight + eventProfile.ball.radiusCm, 25.0f};
    const std::array<billiardgl::PocketBoundaryFrame, 6> failedBatchFrames =
        billiardgl::buildPocketBoundaryFrames(eventProfile.tableBoundary);
    const billiardgl::PocketBoundaryFrame& sideFrame = failedBatchFrames[4];
    failedPhysicalBatch.balls[2].position = billiardgl::Point3{
        sideFrame.mouthCenter.x - sideFrame.inward.x *
            sideFrame.throatDepthCm,
        billiardgl::kTableHeight + eventProfile.ball.radiusCm,
        sideFrame.mouthCenter.z - sideFrame.inward.z *
            sideFrame.throatDepthCm};
    failedPhysicalBatch.balls[2].velocity = billiardgl::Point3{
        -sideFrame.inward.x * 100.0f, 0.0f,
        -sideFrame.inward.z * 100.0f};
    failedPhysicalBatch.balls[2].speed = 100.0f;
    const billiardgl::PhysicsStepTelemetry failedBatchTelemetry =
        billiardgl::updatePhysics(failedPhysicalBatch, 0.01f, eventProfile);
    if (failedBatchTelemetry.stepStatus !=
            billiardgl::PhysicsStepStatus::Failed ||
        failedBatchTelemetry.failureCode !=
            billiardgl::PhysicsFailureCode::PenetrationLimit ||
        !failedBatchTelemetry.solverEvents.empty() ||
        !failedBatchTelemetry.contacts.empty() ||
        !failedBatchTelemetry.surfaceMotion.empty() ||
        !failedBatchTelemetry.analyticSurfaceMotion.empty() ||
        !failedBatchTelemetry.planeCrossings.empty() ||
        !failedBatchTelemetry.planeAttestations.empty() ||
        !nearlyEqual(failedPhysicalBatch.balls[0].position.x, 0.0f) ||
        !nearlyEqual(failedPhysicalBatch.balls[1].position.x, 1.0f) ||
        failedPhysicalBatch.balls[2].pocketInteraction.phase !=
            billiardgl::PocketInteractionPhase::Outside) {
        return fail("a failed physical batch must report and roll back atomically");
    }

    billiardgl::GameState islandLimitState;
    for (billiardgl::BallState& ball : islandLimitState.balls)
        ball.pocketed = true;
    for (int index = 0; index < 3; ++index) {
        islandLimitState.balls[index].pocketed = false;
        islandLimitState.balls[index].position = billiardgl::Point3{
            index * 2.0f * eventProfile.ball.radiusCm,
            billiardgl::kTableHeight + eventProfile.ball.radiusCm, 25.0f};
    }
    for (int index = 0; index < 3; ++index) {
        islandLimitState.balls[index].velocity.x =
            300.0f - index * 100.0f;
        islandLimitState.balls[index].speed =
            300.0f - index * 100.0f;
    }
    billiardgl::PhysicsProfile islandLimitProfile = eventProfile;
    islandLimitProfile.solver.maximumIslandSize = 2;
    const billiardgl::PhysicsStepTelemetry islandLimitTelemetry =
        billiardgl::updatePhysics(
            islandLimitState, 0.01f, islandLimitProfile);
    if (islandLimitTelemetry.failureCode !=
            billiardgl::PhysicsFailureCode::IslandLimit ||
        !nearlyEqual(islandLimitState.balls[0].velocity.x, 300.0f)) {
        return fail("island-limit failure must roll back the complete tick");
    }

    billiardgl::GameState residualState = islandLimitState;
    billiardgl::PhysicsProfile residualProfile = eventProfile;
    residualProfile.solver.velocityIterations = 1;
    residualProfile.solver.residualToleranceCmS = 0.0f;
    const billiardgl::PhysicsStepTelemetry residualTelemetry =
        billiardgl::updatePhysics(residualState, 0.01f, residualProfile);
    if (residualTelemetry.failureCode !=
            billiardgl::PhysicsFailureCode::ResidualLimit ||
        !nearlyEqual(residualState.balls[0].velocity.x, 300.0f)) {
        return fail("residual-limit failure must roll back the complete tick");
    }

    billiardgl::GameState passiveGateState;
    for (billiardgl::BallState& ball : passiveGateState.balls)
        ball.pocketed = true;
    passiveGateState.balls[0].pocketed = false;
    passiveGateState.balls[1].pocketed = false;
    passiveGateState.balls[0].position = billiardgl::Point3{
        0.0f, billiardgl::kTableHeight + eventProfile.ball.radiusCm, 25.0f};
    passiveGateState.balls[1].position = billiardgl::Point3{
        2.0f * eventProfile.ball.radiusCm,
        billiardgl::kTableHeight + eventProfile.ball.radiusCm, 25.0f};
    passiveGateState.balls[0].velocity.x = 100.0f;
    passiveGateState.balls[0].speed = 100.0f;
    billiardgl::PhysicsProfile activeContactProfile = eventProfile;
    activeContactProfile.ball.normalRestitution = 2.0f;
    activeContactProfile.solver.passiveEnergyToleranceJ = 0.0;
    const billiardgl::PhysicsStepTelemetry passiveGateTelemetry =
        billiardgl::updatePhysics(
            passiveGateState, 0.01f, activeContactProfile);
    if (passiveGateTelemetry.failureCode !=
            billiardgl::PhysicsFailureCode::PassiveEnergyCreation ||
        !nearlyEqual(passiveGateState.balls[0].velocity.x, 100.0f) ||
        !nearlyEqual(passiveGateState.balls[1].velocity.x, 0.0f)) {
        return fail("passive-energy failure must roll back the complete tick");
    }

    billiardgl::GameState eventBudgetState;
    for (billiardgl::BallState& ball : eventBudgetState.balls)
        ball.pocketed = true;
    for (int index = 0; index < 3; ++index) {
        eventBudgetState.balls[index].pocketed = false;
        eventBudgetState.balls[index].position = billiardgl::Point3{
            index * 10.0f,
            billiardgl::kTableHeight + eventProfile.ball.radiusCm, 25.0f};
    }
    eventBudgetState.balls[0].velocity.x = 300.0f;
    eventBudgetState.balls[0].speed = 300.0f;
    billiardgl::PhysicsProfile eventBudgetProfile = eventProfile;
    eventBudgetProfile.solver.maximumEventsPerTick = 1;
    const billiardgl::PhysicsStepTelemetry eventBudgetTelemetry =
        billiardgl::updatePhysics(
            eventBudgetState, 0.1f, eventBudgetProfile);
    if (eventBudgetTelemetry.failureCode !=
            billiardgl::PhysicsFailureCode::EventBudget ||
        !nearlyEqual(eventBudgetState.balls[0].position.x, 0.0f) ||
        !nearlyEqual(eventBudgetState.balls[0].velocity.x, 300.0f)) {
        return fail("event-budget exhaustion must roll back the complete tick");
    }

    billiardgl::GameState topologyConflict;
    for (billiardgl::BallState& ball : topologyConflict.balls)
        ball.pocketed = true;
    topologyConflict.balls[0].pocketed = false;
    billiardgl::PhysicsProfile topologyProfile = eventProfile;
    topologyProfile.tableBoundary.captureDepthCm =
        topologyProfile.tableBoundary.throatDepthCm;
    const std::array<billiardgl::PocketBoundaryFrame, 6> conflictFrames =
        billiardgl::buildPocketBoundaryFrames(
            topologyProfile.tableBoundary);
    const billiardgl::PocketBoundaryFrame& conflictFrame = conflictFrames[4];
    topologyConflict.balls[0].position = billiardgl::Point3{
        conflictFrame.mouthCenter.x - conflictFrame.inward.x *
            conflictFrame.throatDepthCm,
        billiardgl::kTableHeight + topologyProfile.ball.radiusCm,
        conflictFrame.mouthCenter.z - conflictFrame.inward.z *
            conflictFrame.throatDepthCm};
    topologyConflict.balls[0].velocity = billiardgl::Point3{
        -conflictFrame.inward.x * 100.0f, 0.0f,
        -conflictFrame.inward.z * 100.0f};
    topologyConflict.balls[0].speed = 100.0f;
    const billiardgl::PhysicsStepTelemetry conflictTelemetry =
        billiardgl::updatePhysics(
            topologyConflict, 0.01f, topologyProfile);
    if (conflictTelemetry.failureCode !=
            billiardgl::PhysicsFailureCode::ContradictoryTopology ||
        topologyConflict.balls[0].pocketInteraction.phase !=
            billiardgl::PocketInteractionPhase::Outside) {
        return fail("contradictory topology must fail before state commit");
    }

    billiardgl::GameState nonfiniteEnergy = islandLimitState;
    nonfiniteEnergy.balls[0].angularVelocity.x =
        std::numeric_limits<float>::max();
    const billiardgl::PhysicsStepTelemetry nonfiniteEnergyTelemetry =
        billiardgl::updatePhysics(nonfiniteEnergy, 0.01f, eventProfile);
    if (nonfiniteEnergyTelemetry.failureCode !=
        billiardgl::PhysicsFailureCode::NonfiniteEnergy) {
        return fail("nonfinite energy must fail before authoritative mutation");
    }

    billiardgl::PhysicsProfile tickEndProfile = eventProfile;
    tickEndProfile.ball.radiusCm = 2.3f;
    tickEndProfile.ball.massKg = 0.0464f;
    tickEndProfile.ball.normalRestitution = 0.0f;
    tickEndProfile.surface.slidingFrictionCoefficient = 0.002f;
    tickEndProfile.surface.rollingResistanceAccelerationCmS2 = 0.0f;
    billiardgl::GameState tickEnd;
    for (billiardgl::BallState& ball : tickEnd.balls) ball.pocketed = true;
    tickEnd.balls[0].pocketed = tickEnd.balls[1].pocketed = false;
    tickEnd.balls[0].position = billiardgl::Point3{
        -36.2933008f, 92.3f, 1.6515350f};
    tickEnd.balls[1].position = billiardgl::Point3{0.0f, 92.3f, 0.0f};
    tickEnd.balls[0].velocity.x = 80.0f;
    tickEnd.balls[0].speed = 80.0f;
    tickEnd.balls[0].angularVelocity.z = -80.0f / 2.3f;
    billiardgl::PhysicsStepTelemetry tickEndTelemetry;
    for (int tick = 0; tick < 4; ++tick) {
        tickEndTelemetry = billiardgl::updatePhysics(
            tickEnd, 0.1f, tickEndProfile,
            billiardgl::PhysicsBoundaryMode::Unbounded);
    }
    int tickEndContacts = 0;
    for (const billiardgl::PhysicsContactRecord& contact :
            tickEndTelemetry.contacts) {
        if (contact.kind == billiardgl::PhysicsContactKind::BallBall) {
            ++tickEndContacts;
        }
    }
    if (tickEndContacts != 1 || tickEndTelemetry.contacts[0].solverEventId < 0) {
        return fail("tick-end event must not be re-resolved by a zero-time tail");
    }

    billiardgl::GameState simultaneous;
    for (billiardgl::BallState& ball : simultaneous.balls) {
        ball.pocketed = true;
        ball.position.y = eventProfile.tableDeparture.clothSurfaceYCm + eventProfile.ball.radiusCm;
    }
    for (int index = 0; index < 3; ++index) simultaneous.balls[index].pocketed = false;
    simultaneous.balls[0].position.x = -10.0f;
    simultaneous.balls[1].position.x = 0.0f;
    simultaneous.balls[2].position.x = 10.0f;
    simultaneous.balls[0].velocity.x = 100.0f;
    simultaneous.balls[2].velocity.x = -100.0f;
    simultaneous.balls[0].speed = simultaneous.balls[2].speed = 100.0f;
    billiardgl::updatePhysics(simultaneous, 0.1f, eventProfile);
    if (!(std::fabs(simultaneous.balls[1].velocity.x) < 0.01f &&
          simultaneous.balls[0].velocity.x < -99.0f &&
          simultaneous.balls[2].velocity.x > 99.0f)) {
        std::cerr << "simultaneous velocities: "
                  << simultaneous.balls[0].velocity.x << ", "
                  << simultaneous.balls[1].velocity.x << ", "
                  << simultaneous.balls[2].velocity.x << '\n';
        return fail("simultaneous symmetric contacts should be island-solved without bias");
    }

    billiardgl::GameState chained;
    for (billiardgl::BallState& ball : chained.balls) {
        ball.pocketed = true;
        ball.position.y = eventProfile.tableDeparture.clothSurfaceYCm + eventProfile.ball.radiusCm;
    }
    for (int index = 0; index < 3; ++index) chained.balls[index].pocketed = false;
    chained.balls[0].position.x = -30.0f;
    chained.balls[1].position.x = 0.0f;
    chained.balls[2].position.x = 30.0f;
    chained.balls[0].velocity.x = 1000.0f;
    chained.balls[0].speed = 1000.0f;
    const billiardgl::PhysicsStepTelemetry chainedTelemetry =
        billiardgl::updatePhysics(chained, 0.06f, eventProfile);
    int chainedImpulses = 0;
    for (const billiardgl::PhysicsContactRecord& contact : chainedTelemetry.contacts) {
        if (contact.kind == billiardgl::PhysicsContactKind::BallBall &&
            contact.velocityImpulseApplied) ++chainedImpulses;
    }
    if (chainedImpulses != 2 || chained.balls[2].velocity.x < 999.0f) {
        std::cerr << "chained impulses/velocities: " << chainedImpulses << ", "
                  << chained.balls[0].velocity.x << ", "
                  << chained.balls[1].velocity.x << ", "
                  << chained.balls[2].velocity.x << '\n';
        return fail("event-driven stepper should resolve multiple impacts in one tick");
    }

    billiardgl::GameState singleTick;
    for (billiardgl::BallState& ball : singleTick.balls) {
        ball.pocketed = true;
        ball.position.y = eventProfile.tableDeparture.clothSurfaceYCm + eventProfile.ball.radiusCm;
    }
    singleTick.balls[0].pocketed = singleTick.balls[1].pocketed = false;
    singleTick.balls[0].position.x = -50.0f;
    singleTick.balls[1].position.x = 50.0f;
    singleTick.balls[0].velocity.x = 1000.0f;
    singleTick.balls[0].speed = 1000.0f;
    billiardgl::GameState subdivided = singleTick;
    const auto singleResult = billiardgl::updatePhysics(singleTick, 0.1f, eventProfile);
    const auto firstHalf = billiardgl::updatePhysics(subdivided, 0.05f, eventProfile);
    const auto secondHalf = billiardgl::updatePhysics(subdivided, 0.05f, eventProfile);
    if (singleResult.stepStatus == billiardgl::PhysicsStepStatus::Failed ||
        firstHalf.stepStatus == billiardgl::PhysicsStepStatus::Failed ||
        secondHalf.stepStatus == billiardgl::PhysicsStepStatus::Failed)
        return fail("tick-subdivision comparison requires successful physical steps");
    for (int index = 0; index < 2; ++index) {
        if (std::fabs(singleTick.balls[index].position.x -
                subdivided.balls[index].position.x) > 0.001f ||
            std::fabs(singleTick.balls[index].velocity.x -
                subdivided.balls[index].velocity.x) > 0.001f) {
            return fail("event-driven result should be tick-subdivision equivalent");
        }
    }

    billiardgl::GameState limited;
    for (billiardgl::BallState& ball : limited.balls) ball.pocketed = true;
    limited.balls[0].pocketed = limited.balls[1].pocketed = false;
    limited.balls[0].position.x = -2.5f;
    limited.balls[1].position.x = 2.5f;
    billiardgl::PhysicsProfile limitedProfile = eventProfile;
    limitedProfile.solver.maximumPenetrationCm = 0.5f;
    const billiardgl::PhysicsStepTelemetry limitedTelemetry =
        billiardgl::updatePhysics(limited, 0.1f, limitedProfile);
    if (limitedTelemetry.stepStatus != billiardgl::PhysicsStepStatus::Failed ||
        limitedTelemetry.failureCode != billiardgl::PhysicsFailureCode::PenetrationLimit ||
        !limitedTelemetry.solverEvents.empty() ||
        !limitedTelemetry.contacts.empty() ||
        !limitedTelemetry.surfaceMotion.empty() ||
        !limitedTelemetry.analyticSurfaceMotion.empty() ||
        !limitedTelemetry.planeCrossings.empty() ||
        !limitedTelemetry.planeAttestations.empty() ||
        limited.balls[0].position.x != -2.5f || limited.balls[1].position.x != 2.5f) {
        return fail("solver hard limit should stop the tick with one explicit failure");
    }

    billiardgl::PhysicsProfile crossingProfile = eventProfile;
    crossingProfile.surface.rollingResistanceAccelerationCmS2 = 0.0f;
    billiardgl::GameState crossingState;
    for (auto& ball : crossingState.balls) ball.pocketed = true;
    crossingState.balls[0].pocketed = false;
    crossingState.balls[0].position.z = -5.0f;
    crossingState.balls[0].velocity.z = 100.0f;
    crossingState.balls[0].speed = 100.0f;
    crossingState.balls[0].angularVelocity.x =
        100.0f / crossingProfile.ball.radiusCm;
    crossingState.balls[0].motionState =
        billiardgl::BallMotionState::Rolling;
    billiardgl::PhysicsPlaneObservationRequest plane;
    plane.observationId = "wpa_ii_08_head_string_cue_center";
    plane.ballIndex = 0;
    plane.axis = billiardgl::PhysicsPlaneAxis::Z;
    plane.planeCoordinateCm = 0.0;
    plane.direction = billiardgl::PhysicsPlaneCrossingDirection::
        NegativeToPositive;
    const auto crossingTelemetry = billiardgl::updatePhysics(
        crossingState, 0.1f, crossingProfile,
        billiardgl::PhysicsBoundaryMode::Unbounded, {plane});
    if (crossingTelemetry.stepStatus !=
            billiardgl::PhysicsStepStatus::Succeeded ||
        crossingTelemetry.planeAttestations.size() != 1 ||
        !crossingTelemetry.planeAttestations[0].complete ||
        crossingTelemetry.planeCrossings.size() != 1 ||
        std::fabs(crossingTelemetry.planeCrossings[0].
            wholeTickTimeOfImpactSeconds - 0.05) > 0.00001 ||
        crossingTelemetry.planeCrossings[0].
            axisVelocityAtCrossingCmS <= 0.0) {
        return fail("authoritative rolling segment emits exact cue-center plane crossing and complete attestation");
    }

    billiardgl::GameState departureState = crossingState;
    departureState.balls[0].position.z = 0.0f;
    plane.allowStartOnPlaneDeparture = true;
    const auto departureTelemetry = billiardgl::updatePhysics(
        departureState, 0.1f, crossingProfile,
        billiardgl::PhysicsBoundaryMode::Unbounded, {plane});
    if (departureTelemetry.planeCrossings.size() != 1 ||
        departureTelemetry.planeCrossings[0].
            wholeTickTimeOfImpactSeconds != 0.0 ||
        departureTelemetry.planeCrossings[0].signedDistanceBeforeCm != 0.0 ||
        departureTelemetry.planeCrossings[0].signedDistanceAfterCm <= 0.0 ||
        !departureTelemetry.planeAttestations[0].complete) {
        return fail("signed frozen-boundary request emits exact start-of-segment departure");
    }
    plane.allowStartOnPlaneDeparture = false;
    departureState = crossingState;
    departureState.balls[0].position.z = 0.0f;
    const auto strictDepartureTelemetry = billiardgl::updatePhysics(
        departureState, 0.1f, crossingProfile,
        billiardgl::PhysicsBoundaryMode::Unbounded, {plane});
    if (!strictDepartureTelemetry.planeCrossings.empty()) {
        return fail("ordinary plane request preserves strict side-to-side crossing semantics");
    }

    billiardgl::PhysicsProfile slidingCrossingProfile = crossingProfile;
    slidingCrossingProfile.surface.slidingFrictionCoefficient = 0.20f;
    slidingCrossingProfile.surface.rollingResistanceAccelerationCmS2 = 1.0f;
    billiardgl::GameState slidingCrossing;
    for (auto& ball : slidingCrossing.balls) ball.pocketed = true;
    slidingCrossing.balls[0].pocketed = false;
    slidingCrossing.balls[0].position.z = -5.0f;
    slidingCrossing.balls[0].velocity.z = 100.0f;
    slidingCrossing.balls[0].speed = 100.0f;
    slidingCrossing.balls[0].motionState =
        billiardgl::BallMotionState::Sliding;
    plane.planeCoordinateCm = 0.0;
    const auto slidingCrossingTelemetry = billiardgl::updatePhysics(
        slidingCrossing, 0.1f, slidingCrossingProfile,
        billiardgl::PhysicsBoundaryMode::Unbounded, {plane});
    if (slidingCrossingTelemetry.planeCrossings.size() != 1 ||
        slidingCrossingTelemetry.planeCrossings[0].segmentOrdinal != 0 ||
        !slidingCrossingTelemetry.planeAttestations[0].complete) {
        return fail("quadratic sliding segment emits one authoritative crossing");
    }

    billiardgl::GameState piecewiseCrossing;
    for (auto& ball : piecewiseCrossing.balls) ball.pocketed = true;
    piecewiseCrossing.balls[0].pocketed = false;
    piecewiseCrossing.balls[0].position.z = -10.0f;
    piecewiseCrossing.balls[0].velocity.z = 100.0f;
    piecewiseCrossing.balls[0].speed = 100.0f;
    piecewiseCrossing.balls[0].motionState =
        billiardgl::BallMotionState::Sliding;
    plane.planeCoordinateCm = 5.0;
    const auto piecewiseCrossingTelemetry = billiardgl::updatePhysics(
        piecewiseCrossing, 0.25f, slidingCrossingProfile,
        billiardgl::PhysicsBoundaryMode::Unbounded, {plane});
    if (piecewiseCrossingTelemetry.analyticSurfaceMotion.size() < 2 ||
        piecewiseCrossingTelemetry.planeCrossings.size() != 1 ||
        piecewiseCrossingTelemetry.planeCrossings[0].segmentOrdinal != 1 ||
        !piecewiseCrossingTelemetry.planeAttestations[0].complete) {
        return fail("piecewise sliding-to-rolling coverage orders a crossing in the rolling tail");
    }

    billiardgl::GameState ordered;
    for (auto& ball : ordered.balls) ball.pocketed = true;
    ordered.balls[0].pocketed = ordered.balls[1].pocketed = false;
    ordered.balls[0].position.z = -20.0f;
    ordered.balls[1].position.z = 20.0f;
    ordered.balls[0].velocity.z = 1000.0f;
    ordered.balls[0].speed = 1000.0f;
    ordered.balls[0].angularVelocity.x =
        1000.0f / crossingProfile.ball.radiusCm;
    ordered.balls[0].motionState = billiardgl::BallMotionState::Rolling;
    plane.planeCoordinateCm = -10.0;
    const auto orderedTelemetry = billiardgl::updatePhysics(
        ordered, 0.05f, crossingProfile,
        billiardgl::PhysicsBoundaryMode::Unbounded, {plane});
    double firstContactTime = std::numeric_limits<double>::infinity();
    for (const auto& contact : orderedTelemetry.contacts) {
        if (contact.kind == billiardgl::PhysicsContactKind::BallBall)
            firstContactTime = std::min(firstContactTime,
                contact.wholeTickTimeOfImpactSeconds);
    }
    if (orderedTelemetry.planeCrossings.size() != 1 ||
        !(orderedTelemetry.planeCrossings[0].
            wholeTickTimeOfImpactSeconds < firstContactTime) ||
        !(firstContactTime > 0.0 && firstContactTime <= 0.05)) {
        std::cerr << "ordered crossing/contact: "
                  << orderedTelemetry.planeCrossings.size() << ", "
                  << (orderedTelemetry.planeCrossings.empty() ? -1.0 :
                      orderedTelemetry.planeCrossings[0].
                        wholeTickTimeOfImpactSeconds)
                  << ", " << firstContactTime << ", attest="
                  << orderedTelemetry.planeAttestations.size() << ", complete="
                  << (!orderedTelemetry.planeAttestations.empty() &&
                      orderedTelemetry.planeAttestations[0].complete)
                  << '\n';
        return fail("same-tick continuous crossing is totally ordered before rebased first contact TOI");
    }

    billiardgl::GameState invalidObservationState = crossingState;
    const billiardgl::GameState invalidObservationBefore =
        invalidObservationState;
    plane.planeCoordinateCm =
        std::numeric_limits<double>::quiet_NaN();
    const auto invalidObservation = billiardgl::updatePhysics(
        invalidObservationState, 0.1f, crossingProfile,
        billiardgl::PhysicsBoundaryMode::Unbounded, {plane});
    if (invalidObservation.stepStatus !=
            billiardgl::PhysicsStepStatus::Failed ||
        invalidObservation.failureCode !=
            billiardgl::PhysicsFailureCode::InvalidControls ||
        invalidObservationState.balls[0].position.z !=
            invalidObservationBefore.balls[0].position.z ||
        !invalidObservation.planeCrossings.empty() ||
        !invalidObservation.planeAttestations.empty()) {
        return fail("invalid plane request fails before mutation and publishes no authoritative evidence");
    }

    return EXIT_SUCCESS;
}
