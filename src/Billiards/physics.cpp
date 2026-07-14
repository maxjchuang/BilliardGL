#include "physics.h"

#include "ball_ball_contact.h"
#include "rules.h"
#include "table_specs.h"

#include <algorithm>
#include <array>
#include <cmath>

namespace billiardgl {
namespace {

std::array<PocketOpening, 6> currentPocketOpenings()
{
    return buildPocketOpenings(defaultTableSpec(), defaultPocketSpec());
}

bool sameSignOrZero(float value, float reference)
{
    if (reference < 0.0f) {
        return value <= 0.0f;
    }
    if (reference > 0.0f) {
        return value >= 0.0f;
    }
    return true;
}

bool isInsideOpeningBand(const BallState& ball, const PocketOpening& opening)
{
    const float halfMouth = opening.mouthWidthCm / 2.0f + kBallRadius;
    if (opening.kind == PocketKind::Side) {
        return std::fabs(ball.position.z - opening.centerZ) <= halfMouth &&
            sameSignOrZero(ball.position.x, opening.centerX);
    }

    return std::fabs(ball.position.x - opening.centerX) <= halfMouth &&
        std::fabs(ball.position.z - opening.centerZ) <= halfMouth;
}

bool hasCrossedPocketDropZone(const BallState& ball, const PocketOpening& opening)
{
    const float halfWidth = kTableInWidth / 2.0f;
    const float halfLength = kTableInLength / 2.0f;
    const float depth = opening.dropZoneDepthCm;

    if (opening.kind == PocketKind::Side) {
        if (opening.centerX < 0.0f) {
            return ball.position.x <= -halfWidth + depth;
        }
        return ball.position.x >= halfWidth - depth;
    }

    const bool crossedX = opening.centerX < 0.0f
        ? ball.position.x <= -halfWidth + depth
        : ball.position.x >= halfWidth - depth;
    const bool crossedZ = opening.centerZ < 0.0f
        ? ball.position.z <= -halfLength + depth
        : ball.position.z >= halfLength - depth;
    return crossedX || crossedZ;
}

double impulseFromVelocityChange(const Point3& before, const Point3& after, const Point3& normal)
{
    const double deltaCentimetersPerSecond = std::fabs(
        (after.x - before.x) * normal.x +
        (after.y - before.y) * normal.y +
        (after.z - before.z) * normal.z);
    return kDefaultBallMassKg * deltaCentimetersPerSecond / 100.0;
}

void appendRailContact(PhysicsStepTelemetry& telemetry, int ballIndex,
    const Point3& beforePosition, const Point3& beforeVelocity,
    const Point3& afterVelocity, bool xAxis)
{
    PhysicsContactRecord contact;
    contact.kind = PhysicsContactKind::Rail;
    contact.firstBall = ballIndex;
    const float coordinate = xAxis ? beforePosition.x : beforePosition.z;
    if (xAxis) {
        contact.normal.x = coordinate > 0.0f ? -1.0f : 1.0f;
    } else {
        contact.normal.z = coordinate > 0.0f ? -1.0f : 1.0f;
    }
    const double limit = xAxis
        ? kTableInWidth / 2.0 - kBallRadius
        : kTableInLength / 2.0 - kBallRadius;
    contact.penetrationCm = std::max(0.0, std::fabs(static_cast<double>(coordinate)) - limit);
    contact.normalImpulseNs = impulseFromVelocityChange(
        beforeVelocity, afterVelocity, contact.normal);
    telemetry.maximumPenetrationCm = std::max(
        telemetry.maximumPenetrationCm, contact.penetrationCm);
    telemetry.contacts.push_back(contact);
}

}  // namespace

bool collideBalls(BallState& first, BallState& second)
{
    return collideBalls(first, second, defaultChinesePoolPhysicsProfile());
}

bool collideBalls(BallState& first, BallState& second,
    const PhysicsProfile& profile)
{
    const BallBallContactResult result = resolveBallBallContact(
        first, second, profile.ball, profile.ball);
    return result.velocityImpulseApplied || result.positionCorrected;
}

void collideWithTableEdge(BallState& ball)
{
    if (isInPocketMouth(ball)) {
        return;
    }

    if (std::fabs(ball.position.x) > kTableInWidth / 2.0f - kBallRadius) {
        ball.position.x = ball.position.x > 0.0f
            ? kTableInWidth / 2.0f - kBallRadius
            : -kTableInWidth / 2.0f + kBallRadius;
        ball.velocity.x *= -1.0f;
    }

    if (std::fabs(ball.position.z) > kTableInLength / 2.0f - kBallRadius) {
        ball.position.z = ball.position.z > 0.0f
            ? kTableInLength / 2.0f - kBallRadius
            : -kTableInLength / 2.0f + kBallRadius;
        ball.velocity.z *= -1.0f;
    }
}

bool isInPocketMouth(const BallState& ball)
{
    const std::array<PocketOpening, 6> openings = currentPocketOpenings();
    for (std::size_t i = 0; i < openings.size(); ++i) {
        if (isInsideOpeningBand(ball, openings[i])) {
            return true;
        }
    }
    return false;
}

bool isInPocket(const BallState& ball)
{
    const std::array<PocketOpening, 6> openings = currentPocketOpenings();
    for (std::size_t i = 0; i < openings.size(); ++i) {
        if (isInsideOpeningBand(ball, openings[i]) && hasCrossedPocketDropZone(ball, openings[i])) {
            return true;
        }
    }
    return false;
}

bool updatePocketedBall(GameState& state, int ballIndex)
{
    BallState& ball = state.balls[ballIndex];
    if (!isInPocket(ball)) {
        return false;
    }

    if (ballIndex == 0) {
        ball.position = Point3{0.0f, kTableHeight + kBallRadius, -kTableInLength / 4.0f};
        state.players.illegalShot = true;
        state.events.cueBallPocketed = true;
    } else {
        ball.pocketed = true;
        state.pocketedBallCount += 1;
        state.events.ballPocketed = true;
        ball.position.z = -100.0f + static_cast<float>(state.pocketedBallCount) * 20.0f;
        ball.position.y = kTableHeight - kBallRadius;
        if (ballIndex != 8) {
            ball.position.y = -100.0f;
            assignPlayerBallTypeForPocketedObjectBall(state, ballIndex);
        }
        if (ballIndex == 8) {
            state.gameOver = true;
            state.events.eightBallPocketed = true;
        }
    }

    resetBallMotion(ball);
    return true;
}

PhysicsStepTelemetry updatePhysics(
    GameState& state, float timeStep, const PhysicsProfile& profile)
{
    PhysicsStepTelemetry telemetry;
    const bool wasMoving = state.ballsMoving;
    clearGameplayEvents(state);
    bool anyMoving = false;
    for (int i = 0; i < kBallCount; ++i) {
        BallState& ball = state.balls[i];
        if (ball.pocketed) {
            continue;
        }
        for (int j = i + 1; j < kBallCount; ++j) {
            if (!state.balls[j].pocketed) {
                const BallBallContactResult result = resolveBallBallContact(
                    ball, state.balls[j], profile.ball, profile.ball);
                if (result.velocityImpulseApplied || result.positionCorrected) {
                    state.events.ballCollision = true;
                    PhysicsContactRecord contact;
                    contact.kind = PhysicsContactKind::BallBall;
                    contact.firstBall = i;
                    contact.secondBall = j;
                    contact.normal = Point3{
                        static_cast<float>(result.contactNormal[0]),
                        static_cast<float>(result.contactNormal[1]),
                        static_cast<float>(result.contactNormal[2])};
                    contact.penetrationCm = result.penetrationM * 100.0;
                    contact.normalImpulseNs = result.normalImpulseNs;
                    telemetry.maximumPenetrationCm = std::max(
                        telemetry.maximumPenetrationCm, contact.penetrationCm);
                    telemetry.contacts.push_back(contact);
                }
            }
        }
        const Point3 previousPosition = ball.position;
        const Point3 previousVelocity = ball.velocity;
        const float previousX = ball.velocity.x;
        const float previousZ = ball.velocity.z;
        collideWithTableEdge(ball);
        if (previousX != ball.velocity.x || previousZ != ball.velocity.z) {
            state.events.railCollision = true;
        }
        if (previousX != ball.velocity.x) {
            appendRailContact(telemetry, i, previousPosition, previousVelocity, ball.velocity, true);
        }
        if (previousZ != ball.velocity.z) {
            appendRailContact(telemetry, i, previousPosition, previousVelocity, ball.velocity, false);
        }
        if (updatePocketedBall(state, i)) {
            PhysicsContactRecord contact;
            contact.kind = PhysicsContactKind::Pocket;
            contact.firstBall = i;
            telemetry.contacts.push_back(contact);
        }
        SurfaceMotionStep surface = advanceSurfaceMotion(
            ball, timeStep, profile.ball, profile.surface);
        surface.ballIndex = i;
        telemetry.surfaceMotion.push_back(surface);
        if (ball.speed > 0.0f) {
            anyMoving = true;
        }
    }
    state.ballsMoving = anyMoving;
    state.events.shotEnded = wasMoving && !anyMoving && state.players.shotTaken;
    return telemetry;
}

PhysicsStepTelemetry updatePhysics(GameState& state, float timeStep)
{
    return updatePhysics(state, timeStep, defaultChinesePoolPhysicsProfile());
}

}  // namespace billiardgl
