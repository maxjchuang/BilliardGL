#include "physics.h"

#include "ball_ball_contact.h"
#include "cushion_contact.h"
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

struct StraightRailEvent {
    bool hit = false;
    bool xAxis = true;
    float timeSeconds = 0.0f;
    float boundaryCm = 0.0f;
    Point3 inwardNormal;
    double penetrationM = 0.0;
};

float coordinate(const BallState& ball, bool xAxis)
{
    return xAxis ? ball.position.x : ball.position.z;
}

float velocity(const BallState& ball, bool xAxis)
{
    return xAxis ? ball.velocity.x : ball.velocity.z;
}

void setCoordinate(BallState& ball, bool xAxis, float value)
{
    if (xAxis) ball.position.x = value;
    else ball.position.z = value;
}

BallState advancedCopy(const BallState& source, float timeSeconds,
    const PhysicsProfile& profile)
{
    BallState copy = source;
    advanceSurfaceMotion(copy, timeSeconds, profile.ball, profile.surface);
    return copy;
}

StraightRailEvent railCandidate(const BallState& ball, float timeStep,
    const PhysicsProfile& profile, bool xAxis)
{
    StraightRailEvent event;
    event.xAxis = xAxis;
    const float limit = (xAxis ? kTableInWidth : kTableInLength) / 2.0f -
        profile.ball.radiusCm;
    const float start = coordinate(ball, xAxis);
    const float component = velocity(ball, xAxis);
    float side = 0.0f;
    if (std::fabs(start) >= limit - 0.000001f) {
        side = start >= 0.0f ? 1.0f : -1.0f;
        event.timeSeconds = 0.0f;
        event.penetrationM = std::max(
            0.0, (std::fabs(static_cast<double>(start)) - limit) / 100.0);
    } else {
        if (timeStep <= 0.0f || component == 0.0f) return event;
        side = component > 0.0f ? 1.0f : -1.0f;
        const float target = side * limit;
        const BallState end = advancedCopy(ball, timeStep, profile);
        const float endCoordinate = coordinate(end, xAxis);
        if ((side > 0.0f && endCoordinate < target) ||
            (side < 0.0f && endCoordinate > target)) {
            return event;
        }
        float lower = 0.0f;
        float upper = timeStep;
        for (int iteration = 0; iteration < 40; ++iteration) {
            const float middle = (lower + upper) * 0.5f;
            const float middleCoordinate = coordinate(
                advancedCopy(ball, middle, profile), xAxis);
            const bool crossed = side > 0.0f ?
                middleCoordinate >= target : middleCoordinate <= target;
            if (crossed) upper = middle;
            else lower = middle;
        }
        event.timeSeconds = upper;
    }
    event.boundaryCm = side * limit;
    if (xAxis) event.inwardNormal.x = -side;
    else event.inwardNormal.z = -side;

    BallState contact = advancedCopy(ball, event.timeSeconds, profile);
    setCoordinate(contact, xAxis, event.boundaryCm);
    if (isInPocketMouth(contact)) return StraightRailEvent{};
    event.hit = true;
    return event;
}

StraightRailEvent earliestRailEvent(const BallState& ball, float timeStep,
    const PhysicsProfile& profile)
{
    const StraightRailEvent x = railCandidate(ball, timeStep, profile, true);
    const StraightRailEvent z = railCandidate(ball, timeStep, profile, false);
    if (!x.hit) return z;
    if (!z.hit) return x;
    return x.timeSeconds <= z.timeSeconds + 0.0000001f ? x : z;
}

void appendRailContact(PhysicsStepTelemetry& telemetry, int ballIndex,
    const CushionContactResult& result, double timeOfImpactSeconds)
{
    PhysicsContactRecord contact;
    contact.kind = PhysicsContactKind::Rail;
    contact.firstBall = ballIndex;
    contact.normal = Point3{
        static_cast<float>(result.contactNormal[0]),
        static_cast<float>(result.contactNormal[1]),
        static_cast<float>(result.contactNormal[2])};
    contact.penetrationCm = result.penetrationM * 100.0;
    contact.normalImpulseNs = result.normalImpulseNs;
    contact.tangentialImpulseNs = result.tangentialImpulseNs;
    contact.frictionCoefficient = result.frictionCoefficient;
    contact.velocityImpulseApplied = result.velocityImpulseApplied;
    contact.kineticEnergyBeforeJ = result.kineticEnergyBeforeJ;
    contact.kineticEnergyAfterJ = result.kineticEnergyAfterJ;
    contact.positionSlopCm = result.positionSlopM * 100.0;
    contact.cushionRegime = result.regime;
    contact.cushionContactArmCm = Point3{
        static_cast<float>(result.contactArmM[0] * 100.0),
        static_cast<float>(result.contactArmM[1] * 100.0),
        static_cast<float>(result.contactArmM[2] * 100.0)};
    contact.cushionContactHeightCm = 100.0 * (
        result.contactArmM[1] + std::sqrt(
            result.contactArmM[0] * result.contactArmM[0] +
            result.contactArmM[2] * result.contactArmM[2]));
    contact.contactTangent = Point3{
        static_cast<float>(result.contactTangent[0]),
        static_cast<float>(result.contactTangent[1]),
        static_cast<float>(result.contactTangent[2])};
    contact.cushionContactVelocityBeforeCmS = Point3{
        static_cast<float>(result.contactVelocityBeforeMS[0] * 100.0),
        static_cast<float>(result.contactVelocityBeforeMS[1] * 100.0),
        static_cast<float>(result.contactVelocityBeforeMS[2] * 100.0)};
    contact.cushionContactVelocityAfterCmS = Point3{
        static_cast<float>(result.contactVelocityAfterMS[0] * 100.0),
        static_cast<float>(result.contactVelocityAfterMS[1] * 100.0),
        static_cast<float>(result.contactVelocityAfterMS[2] * 100.0)};
    contact.impulseOnBallNs = Point3{
        static_cast<float>(result.impulseOnBallNs[0]),
        static_cast<float>(result.impulseOnBallNs[1]),
        static_cast<float>(result.impulseOnBallNs[2])};
    contact.positionCorrectionCm = Point3{
        static_cast<float>(result.positionCorrectionM[0] * 100.0),
        static_cast<float>(result.positionCorrectionM[1] * 100.0),
        static_cast<float>(result.positionCorrectionM[2] * 100.0)};
    contact.normalRelativeSpeedBeforeCmS =
        result.normalRelativeSpeedBeforeMS * 100.0;
    contact.normalRelativeSpeedAfterCmS =
        result.normalRelativeSpeedAfterMS * 100.0;
    contact.restitution = result.restitution;
    contact.noseHeightRatio = result.noseHeightRatio;
    contact.incidentSpeedCmS = result.incidentSpeedMS * 100.0;
    contact.maximumRigidIncidentSpeedCmS =
        result.maximumRigidIncidentSpeedMS * 100.0;
    contact.rigidDomainExceeded = result.rigidDomainExceeded;
    contact.positionCorrected = result.positionCorrected;
    contact.timeOfImpactSeconds = timeOfImpactSeconds;
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
    collideWithTableEdge(ball, defaultChinesePoolPhysicsProfile());
}

CushionContactResult collideWithTableEdge(
    BallState& ball, const PhysicsProfile& profile)
{
    if (isInPocketMouth(ball)) {
        return CushionContactResult{};
    }
    const StraightRailEvent event = earliestRailEvent(ball, 0.0f, profile);
    if (!event.hit) return CushionContactResult{};
    setCoordinate(ball, event.xAxis, event.boundaryCm);
    return resolveCushionContact(
        ball, event.inwardNormal, event.penetrationM,
        profile.ball, profile.cushion);
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
                    contact.regime = result.regime;
                    contact.velocityImpulseApplied = result.velocityImpulseApplied;
                    contact.contactTangent = Point3{
                        static_cast<float>(result.contactTangent[0]),
                        static_cast<float>(result.contactTangent[1]),
                        static_cast<float>(result.contactTangent[2])};
                    contact.firstContactArmCm = Point3{
                        static_cast<float>(result.firstContactArmM[0] * 100.0),
                        static_cast<float>(result.firstContactArmM[1] * 100.0),
                        static_cast<float>(result.firstContactArmM[2] * 100.0)};
                    contact.secondContactArmCm = Point3{
                        static_cast<float>(result.secondContactArmM[0] * 100.0),
                        static_cast<float>(result.secondContactArmM[1] * 100.0),
                        static_cast<float>(result.secondContactArmM[2] * 100.0)};
                    contact.relativeContactVelocityBeforeCmS = Point3{
                        static_cast<float>(result.relativeContactVelocityBeforeMS[0] * 100.0),
                        static_cast<float>(result.relativeContactVelocityBeforeMS[1] * 100.0),
                        static_cast<float>(result.relativeContactVelocityBeforeMS[2] * 100.0)};
                    contact.relativeContactVelocityAfterCmS = Point3{
                        static_cast<float>(result.relativeContactVelocityAfterMS[0] * 100.0),
                        static_cast<float>(result.relativeContactVelocityAfterMS[1] * 100.0),
                        static_cast<float>(result.relativeContactVelocityAfterMS[2] * 100.0)};
                    contact.impulseOnSecondNs = Point3{
                        static_cast<float>(result.impulseOnSecondNs[0]),
                        static_cast<float>(result.impulseOnSecondNs[1]),
                        static_cast<float>(result.impulseOnSecondNs[2])};
                    contact.firstPositionCorrectionCm = Point3{
                        static_cast<float>(result.firstPositionCorrectionM[0] * 100.0),
                        static_cast<float>(result.firstPositionCorrectionM[1] * 100.0),
                        static_cast<float>(result.firstPositionCorrectionM[2] * 100.0)};
                    contact.secondPositionCorrectionCm = Point3{
                        static_cast<float>(result.secondPositionCorrectionM[0] * 100.0),
                        static_cast<float>(result.secondPositionCorrectionM[1] * 100.0),
                        static_cast<float>(result.secondPositionCorrectionM[2] * 100.0)};
                    contact.normalRelativeSpeedBeforeCmS =
                        result.normalRelativeSpeedBeforeMS * 100.0;
                    contact.normalRelativeSpeedAfterCmS =
                        result.normalRelativeSpeedAfterMS * 100.0;
                    contact.tangentialImpulseNs = result.tangentialImpulseNs;
                    contact.frictionCoefficient = result.frictionCoefficient;
                    contact.kineticEnergyBeforeJ = result.kineticEnergyBeforeJ;
                    contact.kineticEnergyAfterJ = result.kineticEnergyAfterJ;
                    contact.positionSlopCm = result.positionSlopM * 100.0;
                    telemetry.maximumPenetrationCm = std::max(
                        telemetry.maximumPenetrationCm, contact.penetrationCm);
                    telemetry.contacts.push_back(contact);
                }
            }
        }
        const StraightRailEvent railEvent = earliestRailEvent(
            ball, std::max(0.0f, timeStep), profile);
        SurfaceMotionStep surface;
        if (railEvent.hit) {
            const SurfaceMotionStep beforeContact = advanceSurfaceMotion(
                ball, railEvent.timeSeconds, profile.ball, profile.surface);
            setCoordinate(ball, railEvent.xAxis, railEvent.boundaryCm);
            const CushionContactResult rail = resolveCushionContact(
                ball, railEvent.inwardNormal, railEvent.penetrationM,
                profile.ball, profile.cushion);
            if (rail.velocityImpulseApplied || rail.positionCorrected) {
                appendRailContact(telemetry, i, rail, railEvent.timeSeconds);
            }
            if (rail.velocityImpulseApplied) state.events.railCollision = true;
            const float remaining = std::max(
                0.0f, timeStep - railEvent.timeSeconds);
            const SurfaceMotionStep afterContact = advanceSurfaceMotion(
                ball, remaining, profile.ball, profile.surface);
            surface = beforeContact;
            surface.after = afterContact.after;
            surface.finalSlipSpeedCmS = afterContact.finalSlipSpeedCmS;
            if (surface.transitionTimeSeconds < 0.0f &&
                afterContact.transitionTimeSeconds >= 0.0f) {
                surface.transitionTimeSeconds = railEvent.timeSeconds +
                    afterContact.transitionTimeSeconds;
            }
        } else {
            surface = advanceSurfaceMotion(
                ball, timeStep, profile.ball, profile.surface);
        }
        if (updatePocketedBall(state, i)) {
            PhysicsContactRecord contact;
            contact.kind = PhysicsContactKind::Pocket;
            contact.firstBall = i;
            telemetry.contacts.push_back(contact);
        }
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
