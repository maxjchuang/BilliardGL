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
    int pocketContacts = 0;
    for (const billiardgl::PhysicsContactRecord& contact : pocketTelemetry.contacts) {
        if (contact.kind == billiardgl::PhysicsContactKind::Pocket) ++pocketContacts;
    }
    if (pocketContacts != 1) {
        return fail("a swept capture should produce exactly one pocket contact");
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

    return EXIT_SUCCESS;
}
