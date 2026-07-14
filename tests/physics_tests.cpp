#include "ball_ball_contact.h"
#include "cushion_contact.h"
#include "game_state.h"
#include "physics.h"
#include "pocket_boundary.h"
#include "surface_motion.h"

#include <cmath>
#include <cstdlib>
#include <iostream>

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
        billiardgl::defaultChinesePoolPhysicsProfile();
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
        billiardgl::defaultChinesePoolPhysicsProfile();
    cushionProfile.cushion.normalRestitution = 0.5f;
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
    if (!(sweptState.balls[0].velocity.x < 0.0f) ||
        sweptState.balls[0].position.x > xLimit + 0.001f ||
        sweptTelemetry.contacts.size() != 1) {
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
        billiardgl::defaultChinesePoolPhysicsProfile();
    contactProfile.ball.normalRestitution = 0.5f;
    contactProfile.ball.frictionCoefficient = 0.2f;
    billiardgl::GameState contactState;
    billiardgl::initializeBalls(contactState);
    for (int index = 2; index < billiardgl::kBallCount; ++index) {
        contactState.balls[index].pocketed = true;
    }
    contactState.balls[0].position = billiardgl::Point3{0.0f, 92.715f, 20.0f};
    contactState.balls[1].position = billiardgl::Point3{5.7f, 92.715f, 20.0f};
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
    if (contactTelemetry.contacts.size() != 1 ||
        !nearlyEqual(static_cast<float>(contactTelemetry.contacts[0].normalImpulseNs),
            static_cast<float>(direct.normalImpulseNs))) {
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
        billiardgl::defaultChinesePoolPhysicsProfile();
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
        billiardgl::defaultChinesePoolPhysicsProfile();
    eventProfile.ball.normalRestitution = 1.0f;
    eventProfile.ball.frictionCoefficient = 0.0f;
    eventProfile.surface.slidingFrictionCoefficient = 0.0f;
    eventProfile.surface.rollingResistanceAccelerationCmS2 = 0.0f;
    eventProfile.surface.legacyFrictionAccelerationCmS2 = 0.0f;
    billiardgl::GameState highSpeed;
    for (billiardgl::BallState& ball : highSpeed.balls) ball.pocketed = true;
    highSpeed.balls[0].pocketed = false;
    highSpeed.balls[1].pocketed = false;
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
    const billiardgl::PhysicsStepTelemetry ballRailTelemetry =
        billiardgl::updatePhysics(ballRail, 0.01f, eventProfile);
    if (ballRailTelemetry.solverEvents.size() != 1 ||
        ballRailTelemetry.solverEvents[0].contactCount != 2 ||
        ballRailTelemetry.contacts.size() < 2 ||
        ballRailTelemetry.contacts[0].solverIslandId !=
            ballRailTelemetry.contacts[1].solverIslandId) {
        return fail("same-TOI ball and straight-rail contacts must solve in one island");
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
    for (billiardgl::BallState& ball : simultaneous.balls) ball.pocketed = true;
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
    for (billiardgl::BallState& ball : chained.balls) ball.pocketed = true;
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
    for (billiardgl::BallState& ball : singleTick.balls) ball.pocketed = true;
    singleTick.balls[0].pocketed = singleTick.balls[1].pocketed = false;
    singleTick.balls[0].position.x = -50.0f;
    singleTick.balls[1].position.x = 50.0f;
    singleTick.balls[0].velocity.x = 1000.0f;
    singleTick.balls[0].speed = 1000.0f;
    billiardgl::GameState subdivided = singleTick;
    billiardgl::updatePhysics(singleTick, 0.1f, eventProfile);
    billiardgl::updatePhysics(subdivided, 0.05f, eventProfile);
    billiardgl::updatePhysics(subdivided, 0.05f, eventProfile);
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
    if (limitedTelemetry.solverEvents.size() != 1 ||
        std::string(limitedTelemetry.solverEvents[0].failureCode) !=
            "penetration_limit") {
        return fail("solver hard limit should stop the tick with one explicit failure");
    }

    return EXIT_SUCCESS;
}
