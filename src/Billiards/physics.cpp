#include "physics.h"

#include "ball_ball_contact.h"
#include "cushion_contact.h"
#include "continuous_collision.h"
#include "contact_island.h"
#include "contact_solver.h"
#include "pocket_boundary.h"
#include "rules.h"
#include "table_specs.h"

#include <algorithm>
#include <array>
#include <cmath>

namespace billiardgl {
namespace {

bool isTraversablePocketMouth(const Point3& position,
    const PhysicsProfile& profile)
{
    const std::array<PocketBoundaryFrame, 6> frames =
        buildPocketBoundaryFrames(profile.tableBoundary);
    for (const PocketBoundaryFrame& frame : frames) {
        const PocketBoundaryQuery query = classifyPocketPoint(
            frame, position, profile.ball.radiusCm);
        if (query.passable && query.local.depthCm >= -profile.ball.radiusCm * 2.0 &&
            query.local.depthCm <= frame.captureDepthCm) {
            return true;
        }
    }
    return false;
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
    if (ball.pocketInteraction.phase == PocketInteractionPhase::ThroatCrossed ||
        ball.pocketInteraction.phase == PocketInteractionPhase::Captured) {
        return event;
    }
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
    if (isTraversablePocketMouth(contact.position, profile)) {
        return StraightRailEvent{};
    }
    event.hit = true;
    return event;
}

struct PocketStepEvent {
    bool hit = false;
    PocketBoundaryEvent boundary;
    PocketBoundaryFrame frame;
    float timeSeconds = 0.0f;
};

PocketStepEvent earliestPocketEvent(const BallState& ball, float timeStep,
    const PhysicsProfile& profile)
{
    PocketStepEvent best;
    if (timeStep <= 0.0f) return best;
    const BallState end = advancedCopy(ball, timeStep, profile);
    const std::array<PocketBoundaryFrame, 6> frames =
        buildPocketBoundaryFrames(profile.tableBoundary);
    double bestFraction = 1.0 + 1e-10;
    for (const PocketBoundaryFrame& frame : frames) {
        const PocketBoundaryEvent event = sweepPocketBoundary(
            frame, ball.position, end.position, profile.ball.radiusCm);
        if (event.kind != PocketBoundaryEventKind::None &&
            event.kind != PocketBoundaryEventKind::Ambiguous &&
            event.fraction < bestFraction - 1e-10) {
            best.hit = true;
            best.boundary = event;
            best.frame = frame;
            bestFraction = event.fraction;
        }
    }
    best.timeSeconds = best.hit
        ? static_cast<float>(bestFraction * timeStep) : timeStep;
    return best;
}

const PocketBoundaryFrame* activePocketFrame(
    const std::array<PocketBoundaryFrame, 6>& frames, int pocketId)
{
    for (const PocketBoundaryFrame& frame : frames) {
        if (frame.pocketId == pocketId) return &frame;
    }
    return nullptr;
}

void appendPocketContact(PhysicsStepTelemetry& telemetry, int ballIndex,
    const PocketBoundaryFrame& frame, const PocketBoundaryEvent& event,
    const PocketBoundaryQuery& query, const PocketTransitionResult& transition,
    unsigned long long captureSequence, double timeOfImpactSeconds);

void classifyFinalPocketState(GameState& state, BallState& ball,
    const PhysicsProfile& profile, PhysicsStepTelemetry& telemetry,
    int ballIndex, double timeOfImpactSeconds)
{
    const std::array<PocketBoundaryFrame, 6> frames =
        buildPocketBoundaryFrames(profile.tableBoundary);
    const PocketBoundaryFrame* selected = activePocketFrame(
        frames, ball.pocketInteraction.pocketId);
    if (selected == nullptr) {
        for (const PocketBoundaryFrame& frame : frames) {
            const PocketBoundaryQuery query = classifyPocketPoint(
                frame, ball.position, profile.ball.radiusCm);
            if (query.region == PocketBoundaryRegion::Approaching) {
                advancePocketInteraction(ball.pocketInteraction, frame.pocketId,
                    PocketBoundaryEventKind::None, query.region, 0);
                return;
            }
        }
        return;
    }
    const PocketBoundaryQuery query = classifyPocketPoint(
        *selected, ball.position, profile.ball.radiusCm);
    PocketBoundaryEventKind event = PocketBoundaryEventKind::None;
    unsigned long long sequence = 0;
    if (query.region == PocketBoundaryRegion::Capture &&
        ball.pocketInteraction.phase == PocketInteractionPhase::ThroatCrossed) {
        event = PocketBoundaryEventKind::Capture;
        sequence = state.nextPocketCaptureSequence;
    }
    const PocketTransitionResult transition = advancePocketInteraction(
        ball.pocketInteraction, selected->pocketId, event, query.region, sequence);
    if (transition.captureEmitted) {
        PocketBoundaryEvent boundary;
        boundary.kind = PocketBoundaryEventKind::Capture;
        boundary.pocketId = selected->pocketId;
        boundary.position = ball.position;
        boundary.inwardNormal = selected->inward;
        boundary.local = query.local;
        boundary.passable = query.passable;
        appendPocketContact(telemetry, ballIndex, *selected, boundary, query,
            transition, sequence, timeOfImpactSeconds);
        ++state.nextPocketCaptureSequence;
    }
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

void appendPocketContact(PhysicsStepTelemetry& telemetry, int ballIndex,
    const PocketBoundaryFrame& frame, const PocketBoundaryEvent& event,
    const PocketBoundaryQuery& query, const PocketTransitionResult& transition,
    unsigned long long captureSequence, double timeOfImpactSeconds)
{
    PhysicsContactRecord contact;
    contact.kind = PhysicsContactKind::Pocket;
    contact.firstBall = ballIndex;
    contact.normal = event.inwardNormal;
    contact.timeOfImpactSeconds = timeOfImpactSeconds;
    contact.pocketId = frame.pocketId;
    contact.pocketKind = frame.kind;
    contact.pocketBoundaryEvent = event.kind;
    contact.pocketPhaseBefore = transition.previous;
    contact.pocketPhaseAfter = transition.current;
    contact.pocketLocal = query.local;
    if (event.kind == PocketBoundaryEventKind::LeftJaw ||
        event.kind == PocketBoundaryEventKind::RightJaw) {
        contact.pocketJawCenterCm = pocketJawCenter(frame,
            event.kind == PocketBoundaryEventKind::LeftJaw ? -1 : 1);
    }
    contact.pocketJawRadiusCm = frame.jawRadiusCm;
    contact.pocketThroatSignedDistanceCm = query.throatSignedDistanceCm;
    contact.pocketCaptureSignedDistanceCm = query.captureSignedDistanceCm;
    contact.pocketPassable = query.passable;
    contact.pocketCaptureSequence = captureSequence;
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
    if (isTraversablePocketMouth(ball.position, profile)) {
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
    return isTraversablePocketMouth(
        ball.position, defaultChinesePoolPhysicsProfile());
}

bool isInPocket(const BallState& ball)
{
    const PhysicsProfile& profile = defaultChinesePoolPhysicsProfile();
    const std::array<PocketBoundaryFrame, 6> frames =
        buildPocketBoundaryFrames(profile.tableBoundary);
    for (const PocketBoundaryFrame& frame : frames) {
        if (classifyPocketPoint(frame, ball.position, profile.ball.radiusCm).region ==
            PocketBoundaryRegion::Capture) return true;
    }
    return false;
}

bool updatePocketedBall(GameState& state, int ballIndex)
{
    BallState& ball = state.balls[ballIndex];
    if (ball.pocketInteraction.phase != PocketInteractionPhase::Captured) {
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
    if (ballIndex == 0) {
        resetPocketInteraction(ball);
    }
    return true;
}

PhysicsStepTelemetry updatePhysicsDiscrete(
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
        const PocketStepEvent pocketEvent = earliestPocketEvent(
            ball, std::max(0.0f, timeStep), profile);
        SurfaceMotionStep surface;
        const bool usePocket = pocketEvent.hit && (!railEvent.hit ||
            pocketEvent.timeSeconds <= railEvent.timeSeconds + 0.0000001f);
        if (usePocket) {
            const SurfaceMotionStep beforeContact = advanceSurfaceMotion(ball,
                pocketEvent.timeSeconds, profile.ball, profile.surface);
            ball.position = pocketEvent.boundary.position;
            const PocketBoundaryQuery query = classifyPocketPoint(
                pocketEvent.frame, ball.position, profile.ball.radiusCm);
            const PocketTransitionResult transition = advancePocketInteraction(
                ball.pocketInteraction, pocketEvent.frame.pocketId,
                pocketEvent.boundary.kind, query.region,
                pocketEvent.boundary.kind == PocketBoundaryEventKind::Capture
                    ? state.nextPocketCaptureSequence : 0);
            if (transition.captureEmitted) ++state.nextPocketCaptureSequence;
            if (pocketEvent.boundary.kind == PocketBoundaryEventKind::LeftJaw ||
                pocketEvent.boundary.kind == PocketBoundaryEventKind::RightJaw) {
                const CushionContactResult jaw = resolveCushionContact(
                    ball, pocketEvent.boundary.inwardNormal, 0.0,
                    profile.ball, profile.cushion);
                if (jaw.velocityImpulseApplied || jaw.positionCorrected) {
                    appendRailContact(telemetry, i, jaw,
                        pocketEvent.timeSeconds);
                }
                if (jaw.velocityImpulseApplied) state.events.railCollision = true;
            }
            appendPocketContact(telemetry, i, pocketEvent.frame,
                pocketEvent.boundary, query, transition,
                transition.captureEmitted
                    ? ball.pocketInteraction.captureSequence : 0,
                pocketEvent.timeSeconds);
            const float remaining = std::max(
                0.0f, timeStep - pocketEvent.timeSeconds);
            const SurfaceMotionStep afterContact = advanceSurfaceMotion(
                ball, remaining, profile.ball, profile.surface);
            surface = beforeContact;
            surface.after = afterContact.after;
            surface.finalSlipSpeedCmS = afterContact.finalSlipSpeedCmS;
            classifyFinalPocketState(state, ball, profile, telemetry, i,
                timeStep);
        } else if (railEvent.hit) {
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
            classifyFinalPocketState(state, ball, profile, telemetry, i,
                timeStep);
        } else {
            surface = advanceSurfaceMotion(
                ball, timeStep, profile.ball, profile.surface);
            classifyFinalPocketState(state, ball, profile, telemetry, i,
                timeStep);
        }
        if (updatePocketedBall(state, i)) {
            const bool alreadyRecorded = !telemetry.contacts.empty() &&
                telemetry.contacts.back().kind == PhysicsContactKind::Pocket &&
                telemetry.contacts.back().firstBall == i &&
                telemetry.contacts.back().pocketBoundaryEvent ==
                    PocketBoundaryEventKind::Capture;
            if (!alreadyRecorded) {
                PhysicsContactRecord contact;
                contact.kind = PhysicsContactKind::Pocket;
                contact.firstBall = i;
                contact.pocketBoundaryEvent = PocketBoundaryEventKind::Capture;
                telemetry.contacts.push_back(contact);
            }
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

namespace {

double earliestBoundaryTime(const GameState& state, float timeStep,
    const PhysicsProfile& profile)
{
    double earliest = timeStep + 1.0;
    for (int index = 0; index < kBallCount; ++index) {
        if (state.balls[index].pocketed) continue;
        const StraightRailEvent rail = earliestRailEvent(
            state.balls[index], timeStep, profile);
        if (rail.hit) earliest = std::min(
            earliest, static_cast<double>(rail.timeSeconds));
        const PocketStepEvent pocket = earliestPocketEvent(
            state.balls[index], timeStep, profile);
        if (pocket.hit) earliest = std::min(
            earliest, static_cast<double>(pocket.timeSeconds));
    }
    return earliest;
}

void prependTelemetry(PhysicsStepTelemetry& tail,
    const PhysicsStepTelemetry& prefix)
{
    tail.contacts.insert(tail.contacts.begin(),
        prefix.contacts.begin(), prefix.contacts.end());
    tail.solverEvents.insert(tail.solverEvents.begin(),
        prefix.solverEvents.begin(), prefix.solverEvents.end());
    tail.surfaceMotion.insert(tail.surfaceMotion.begin(),
        prefix.surfaceMotion.begin(), prefix.surfaceMotion.end());
    tail.maximumPenetrationCm = std::max(
        tail.maximumPenetrationCm, prefix.maximumPenetrationCm);
}

PhysicsStepTelemetry updatePhysicsEventDriven(
    GameState& state, float timeStep, const PhysicsProfile& profile,
    int remainingEvents)
{
    if (timeStep <= 0.0f || remainingEvents <= 0) {
        return updatePhysicsDiscrete(state, timeStep, profile);
    }
    const std::vector<ContinuousContactCandidate> candidates =
        generateBallBallCandidates(state, timeStep, profile.ball.radiusCm,
            profile.solver.toiToleranceSeconds);
    if (candidates.empty()) return updatePhysicsDiscrete(state, timeStep, profile);
    const double toi = candidates.front().timeOfImpactSeconds;
    const double boundaryTime = earliestBoundaryTime(state, timeStep, profile);
    if (toi > boundaryTime + profile.solver.toiToleranceSeconds) {
        return updatePhysicsDiscrete(state, timeStep, profile);
    }

    PhysicsStepTelemetry prefix;
    for (int index = 0; index < kBallCount; ++index) {
        if (state.balls[index].pocketed) continue;
        SurfaceMotionStep motion = advanceSurfaceMotion(state.balls[index],
            static_cast<float>(toi), profile.ball, profile.surface);
        motion.ballIndex = index;
        prefix.surfaceMotion.push_back(motion);
    }
    const ContactIslandBuildResult built = buildEarliestContactIslands(
        candidates, profile.solver.toiToleranceSeconds,
        profile.solver.maximumIslandSize);
    bool impulseApplied = false;
    bool hardFailure = false;
    for (const ContactIsland& island : built.islands) {
        const ContactSolverResult solved = solveContactIsland(state, island, profile);
        SolverEventRecord solverEvent;
        solverEvent.eventId = profile.solver.maximumEventsPerTick - remainingEvents;
        solverEvent.islandId = island.islandId;
        solverEvent.candidateCount = static_cast<int>(candidates.size());
        solverEvent.contactCount = static_cast<int>(island.contacts.size());
        solverEvent.duplicateCandidatesRemoved = built.duplicateCandidatesRemoved;
        solverEvent.velocityIterations = solved.velocityIterations;
        solverEvent.positionIterations = solved.positionIterations;
        solverEvent.maximumResidualCmS = solved.maximumResidualCmS;
        solverEvent.maximumPenetrationCm = solved.maximumPenetrationCm;
        solverEvent.kineticEnergyBeforeJ = solved.kineticEnergyBeforeJ;
        solverEvent.kineticEnergyAfterJ = solved.kineticEnergyAfterJ;
        solverEvent.islandLimitExceeded = island.limitExceeded;
        solverEvent.failureCode = contactSolverStatusName(solved.status);
        prefix.solverEvents.push_back(solverEvent);
        hardFailure = hardFailure || solved.status != ContactSolverStatus::Converged;
        prefix.maximumPenetrationCm = std::max(
            prefix.maximumPenetrationCm, solved.maximumPenetrationCm);
        const std::size_t solvedCount = std::min(
            island.contacts.size(), solved.contacts.size());
        for (std::size_t index = 0; index < solvedCount; ++index) {
            const ContinuousContactCandidate& candidate = island.contacts[index];
            PhysicsContactRecord contact;
            contact.solverEventId = solverEvent.eventId;
            contact.solverIslandId = island.islandId;
            contact.kind = PhysicsContactKind::BallBall;
            contact.firstBall = candidate.firstBall;
            contact.secondBall = candidate.secondBall;
            contact.normal = candidate.normal;
            contact.penetrationCm = candidate.penetrationCm;
            contact.timeOfImpactSeconds = toi;
            contact.normalImpulseNs = solved.contacts[index].accumulatedNormalImpulseNs;
            contact.solverResidualCmS = solved.contacts[index].residualCmS;
            contact.solverProjectionCm = solved.contacts[index].projectionCm;
            contact.velocityImpulseApplied = contact.normalImpulseNs > 0.0;
            contact.kineticEnergyBeforeJ = solved.kineticEnergyBeforeJ;
            contact.kineticEnergyAfterJ = solved.kineticEnergyAfterJ;
            contact.normalRelativeSpeedBeforeCmS =
                -solved.contacts[index].targetNormalSpeedCmS /
                std::max(0.000001f, profile.ball.normalRestitution);
            contact.normalRelativeSpeedAfterCmS =
                solved.contacts[index].targetNormalSpeedCmS -
                solved.contacts[index].residualCmS;
            contact.positionSlopCm = profile.solver.penetrationSlopCm;
            prefix.contacts.push_back(contact);
            impulseApplied = impulseApplied || contact.velocityImpulseApplied;
        }
    }
    if (hardFailure) {
        state.ballsMoving = anyBallMoving(state);
        return prefix;
    }
    const float remaining = std::max(0.0f, timeStep - static_cast<float>(toi));
    PhysicsStepTelemetry tail = updatePhysicsEventDriven(
        state, remaining, profile, remainingEvents - 1);
    prependTelemetry(tail, prefix);
    if (impulseApplied) {
        state.events.ballCollision = true;
        state.ballsMoving = anyBallMoving(state);
    }
    return tail;
}

}  // namespace

PhysicsStepTelemetry updatePhysics(
    GameState& state, float timeStep, const PhysicsProfile& profile)
{
    return updatePhysicsEventDriven(
        state, timeStep, profile, profile.solver.maximumEventsPerTick);
}

PhysicsStepTelemetry updatePhysics(GameState& state, float timeStep)
{
    return updatePhysics(state, timeStep, defaultChinesePoolPhysicsProfile());
}

}  // namespace billiardgl
