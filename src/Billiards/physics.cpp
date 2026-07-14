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
        Point3 inwardNormal;
        if (xAxis) inwardNormal.x = -side;
        else inwardNormal.z = -side;
        const float radiusM = profile.ball.radiusCm / 100.0f;
        const Point3 armM{
            -inwardNormal.x * radiusM,
            radiusM * (profile.cushion.noseHeightRatio - 1.0f),
            -inwardNormal.z * radiusM};
        const Point3 rotationalVelocityCmS{
            (ball.angularVelocity.y * armM.z -
                ball.angularVelocity.z * armM.y) * 100.0f,
            (ball.angularVelocity.z * armM.x -
                ball.angularVelocity.x * armM.z) * 100.0f,
            (ball.angularVelocity.x * armM.y -
                ball.angularVelocity.y * armM.x) * 100.0f};
        const double contactNormalSpeed =
            (ball.velocity.x + rotationalVelocityCmS.x) * inwardNormal.x +
            (ball.velocity.y + rotationalVelocityCmS.y) * inwardNormal.y +
            (ball.velocity.z + rotationalVelocityCmS.z) * inwardNormal.z;
        if (event.penetrationM * 100.0 <=
                profile.solver.penetrationSlopCm + 1e-9 &&
            contactNormalSpeed >= -profile.solver.residualToleranceCmS) {
            return StraightRailEvent{};
        }
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

int straightRailFeatureId(const StraightRailEvent& event)
{
    const bool positive = event.boundaryCm > 0.0f;
    return (event.xAxis ? 0 : 2) + (positive ? 1 : 0);
}

int pocketFeatureId(int pocketId, PocketBoundaryEventKind kind)
{
    return 100 + pocketId * 8 + static_cast<int>(kind);
}

std::vector<ContinuousContactCandidate> generateBoundaryCandidates(
    const GameState& state, float timeStep, const PhysicsProfile& profile)
{
    std::vector<ContinuousContactCandidate> result;
    const std::array<PocketBoundaryFrame, 6> frames =
        buildPocketBoundaryFrames(profile.tableBoundary);
    for (int ballIndex = 0; ballIndex < kBallCount; ++ballIndex) {
        const BallState& ball = state.balls[ballIndex];
        if (ball.pocketed) continue;
        for (bool xAxis : {true, false}) {
            const StraightRailEvent rail = railCandidate(
                ball, timeStep, profile, xAxis);
            if (!rail.hit) continue;
            ContinuousContactCandidate candidate = boundaryContactCandidate(
                ballIndex, straightRailFeatureId(rail), rail.timeSeconds,
                rail.inwardNormal, PocketBoundaryEventKind::StraightRail);
            candidate.penetrationCm = rail.penetrationM * 100.0;
            result.push_back(candidate);
        }
        if (timeStep <= 0.0f ||
            (std::fabs(ball.velocity.x) <= 1e-9f &&
             std::fabs(ball.velocity.z) <= 1e-9f)) continue;
        const BallState end = advancedCopy(ball, timeStep, profile);
        for (const PocketBoundaryFrame& frame : frames) {
            const PocketLocalPoint startLocal = pocketLocalPoint(
                frame, ball.position);
            const PocketLocalPoint endLocal = pocketLocalPoint(
                frame, end.position);
            const double reach = frame.jawRadiusCm + profile.ball.radiusCm;
            const double minimumDepth = std::min(
                startLocal.depthCm, endLocal.depthCm);
            const double maximumDepth = std::max(
                startLocal.depthCm, endLocal.depthCm);
            const double minimumOffset = std::min(
                startLocal.offsetCm, endLocal.offsetCm);
            const double maximumOffset = std::max(
                startLocal.offsetCm, endLocal.offsetCm);
            const double lateralReach = frame.mouthWidthCm * 0.5 + reach;
            if (maximumDepth < -reach ||
                minimumDepth > frame.captureDepthCm ||
                maximumOffset < -lateralReach ||
                minimumOffset > lateralReach) continue;
            const std::vector<PocketBoundaryEvent> events =
                sweepPocketBoundaryEvents(frame, ball.position, end.position,
                    profile.ball.radiusCm);
            for (const PocketBoundaryEvent& event : events) {
                if (event.kind == PocketBoundaryEventKind::None ||
                    event.kind == PocketBoundaryEventKind::Ambiguous) continue;
                result.push_back(boundaryContactCandidate(ballIndex,
                    pocketFeatureId(frame.pocketId, event.kind),
                    event.fraction * timeStep, event.inwardNormal,
                    event.kind, frame.pocketId));
            }
        }
    }
    std::sort(result.begin(), result.end(), continuousContactLess);
    return result;
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
    GameState& state, float timeStep, const PhysicsProfile& profile,
    PhysicsBoundaryMode boundaryMode, bool resolveBallContacts = true)
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
        for (int j = i + 1; resolveBallContacts && j < kBallCount; ++j) {
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
        StraightRailEvent railEvent;
        PocketStepEvent pocketEvent;
        if (boundaryMode == PhysicsBoundaryMode::ProductionTable) {
            railEvent = earliestRailEvent(
                ball, std::max(0.0f, timeStep), profile);
            pocketEvent = earliestPocketEvent(
                ball, std::max(0.0f, timeStep), profile);
        }
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
            if (boundaryMode == PhysicsBoundaryMode::ProductionTable) {
                classifyFinalPocketState(state, ball, profile, telemetry, i,
                    timeStep);
            }
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
            if (boundaryMode == PhysicsBoundaryMode::ProductionTable) {
                classifyFinalPocketState(state, ball, profile, telemetry, i,
                    timeStep);
            }
        } else {
            surface = advanceSurfaceMotion(
                ball, timeStep, profile.ball, profile.surface);
            if (boundaryMode == PhysicsBoundaryMode::ProductionTable) {
                classifyFinalPocketState(state, ball, profile, telemetry, i,
                    timeStep);
            }
        }
        if (boundaryMode == PhysicsBoundaryMode::ProductionTable &&
            updatePocketedBall(state, i)) {
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

void mergeGameplayEvents(GameplayEvents& target, const GameplayEvents& source)
{
    target.ballCollision = target.ballCollision || source.ballCollision;
    target.railCollision = target.railCollision || source.railCollision;
    target.ballPocketed = target.ballPocketed || source.ballPocketed;
    target.cueBallPocketed = target.cueBallPocketed || source.cueBallPocketed;
    target.eightBallPocketed = target.eightBallPocketed ||
        source.eightBallPocketed;
    target.shotEnded = target.shotEnded || source.shotEnded;
}

void failStep(PhysicsStepTelemetry& telemetry, PhysicsFailureCode code,
    int eventId = -1, int islandId = -1)
{
    if (telemetry.stepStatus == PhysicsStepStatus::Failed) return;
    telemetry.stepStatus = PhysicsStepStatus::Failed;
    telemetry.failureCode = code;
    telemetry.failingEventId = eventId;
    telemetry.failingIslandId = islandId;
}

PhysicsFailureCode batchFailureCode(ContinuousBatchFailureCode code)
{
    switch (code) {
    case ContinuousBatchFailureCode::None: return PhysicsFailureCode::None;
    case ContinuousBatchFailureCode::InvalidControls:
        return PhysicsFailureCode::InvalidControls;
    case ContinuousBatchFailureCode::IslandLimit:
        return PhysicsFailureCode::IslandLimit;
    case ContinuousBatchFailureCode::ContradictoryTopology:
        return PhysicsFailureCode::ContradictoryTopology;
    }
    return PhysicsFailureCode::InvalidControls;
}

PhysicsFailureCode solverFailureCode(ContactSolverStatus status)
{
    switch (status) {
    case ContactSolverStatus::Converged: return PhysicsFailureCode::None;
    case ContactSolverStatus::IterationLimit:
        return PhysicsFailureCode::ResidualLimit;
    case ContactSolverStatus::IslandLimit:
        return PhysicsFailureCode::IslandLimit;
    case ContactSolverStatus::PenetrationLimit:
        return PhysicsFailureCode::PenetrationLimit;
    case ContactSolverStatus::NonfiniteState:
        return PhysicsFailureCode::NonfiniteState;
    }
    return PhysicsFailureCode::NonfiniteState;
}

bool finiteGameState(const GameState& state)
{
    for (const BallState& ball : state.balls) {
        if (!std::isfinite(ball.position.x) ||
            !std::isfinite(ball.position.y) ||
            !std::isfinite(ball.position.z) ||
            !std::isfinite(ball.velocity.x) ||
            !std::isfinite(ball.velocity.y) ||
            !std::isfinite(ball.velocity.z) ||
            !std::isfinite(ball.angularVelocity.x) ||
            !std::isfinite(ball.angularVelocity.y) ||
            !std::isfinite(ball.angularVelocity.z) ||
            !std::isfinite(ball.speed)) return false;
    }
    return true;
}

double totalGameEnergyJ(const GameState& state, const PhysicsProfile& profile)
{
    double energy = translationalKineticEnergyJ(state, profile.ball.massKg);
    for (const BallState& ball : state.balls) {
        if (!ball.pocketed) energy += rotationalKineticEnergyJ(
            ball, profile.ball);
    }
    return energy;
}

CushionContactRegime cushionRegime(BallBallContactRegime regime)
{
    switch (regime) {
    case BallBallContactRegime::Separating:
        return CushionContactRegime::Separating;
    case BallBallContactRegime::Frictionless:
        return CushionContactRegime::Frictionless;
    case BallBallContactRegime::Stick:
        return CushionContactRegime::Stick;
    case BallBallContactRegime::Slip:
        return CushionContactRegime::Slip;
    case BallBallContactRegime::NoContact:
        return CushionContactRegime::NoContact;
    }
    return CushionContactRegime::NoContact;
}

int batchCandidateCount(const ContinuousEventBatch& batch)
{
    int count = static_cast<int>(batch.topologyTransitions.size());
    for (const ContactIsland& island : batch.physicalIslands) {
        count += static_cast<int>(island.contacts.size());
    }
    return count;
}

const PocketBoundaryFrame* pocketFrameById(
    const std::array<PocketBoundaryFrame, 6>& frames, int pocketId)
{
    for (const PocketBoundaryFrame& frame : frames) {
        if (frame.pocketId == pocketId) return &frame;
    }
    return nullptr;
}

bool applyTopologyTransitions(GameState& state,
    std::vector<ContinuousContactCandidate> transitions,
    const PhysicsProfile& profile,
    const std::array<PocketBoundaryFrame, 6>& pocketFrames,
    PhysicsStepTelemetry& telemetry, double timeOfImpactSeconds)
{
    std::sort(transitions.begin(), transitions.end(), continuousContactLess);
    for (std::size_t first = 0; first < transitions.size(); ++first) {
        for (std::size_t second = first + 1; second < transitions.size();
                ++second) {
            if (transitions[first].firstBall !=
                    transitions[second].firstBall) continue;
            if (transitions[first].pocketId != transitions[second].pocketId ||
                transitions[first].pocketEvent ==
                    transitions[second].pocketEvent) return false;
        }
    }
    for (const ContinuousContactCandidate& candidate : transitions) {
        const PocketBoundaryFrame* frame = pocketFrameById(
            pocketFrames, candidate.pocketId);
        if (frame == nullptr) continue;
        BallState& ball = state.balls[candidate.firstBall];
        if (ball.pocketInteraction.phase ==
            PocketInteractionPhase::Captured) continue;
        const PocketBoundaryQuery query = classifyPocketPoint(
            *frame, ball.position, profile.ball.radiusCm);
        const unsigned long long sequence =
            candidate.kind == ContinuousContactKind::Capture
                ? state.nextPocketCaptureSequence : 0;
        const PocketTransitionResult transition = advancePocketInteraction(
            ball.pocketInteraction, frame->pocketId, candidate.pocketEvent,
            query.region, sequence);
        PocketBoundaryEvent event;
        event.kind = candidate.pocketEvent;
        event.pocketId = frame->pocketId;
        event.position = ball.position;
        event.inwardNormal = candidate.normal;
        event.local = query.local;
        event.passable = query.passable;
        if (candidate.kind != ContinuousContactKind::Capture ||
            transition.captureEmitted) {
            appendPocketContact(telemetry, candidate.firstBall, *frame, event,
                query, transition,
                transition.captureEmitted ? sequence : 0,
                timeOfImpactSeconds);
        }
        if (transition.captureEmitted) {
            ++state.nextPocketCaptureSequence;
            updatePocketedBall(state, candidate.firstBall);
        }
    }
    return true;
}

PhysicsStepTelemetry updatePhysicsEventDriven(
    GameState& state, float timeStep, const PhysicsProfile& profile,
    int remainingEvents, PhysicsBoundaryMode boundaryMode)
{
    if (!std::isfinite(timeStep)) {
        PhysicsStepTelemetry failed;
        failStep(failed, PhysicsFailureCode::NonfiniteState);
        return failed;
    }
    if (timeStep <= 0.0f) {
        return updatePhysicsDiscrete(state, timeStep, profile, boundaryMode);
    }
    if (remainingEvents <= 0) {
        PhysicsStepTelemetry failed;
        failStep(failed, PhysicsFailureCode::EventBudget,
            profile.solver.maximumEventsPerTick);
        return failed;
    }
    std::vector<ContinuousContactCandidate> candidates =
        generateBallBallCandidates(state, timeStep, profile.ball.radiusCm,
            profile.solver.toiToleranceSeconds, false,
            profile.solver.residualToleranceCmS);
    if (boundaryMode == PhysicsBoundaryMode::ProductionTable) {
        std::vector<ContinuousContactCandidate> boundary =
            generateBoundaryCandidates(state, timeStep, profile);
        candidates.insert(candidates.end(), boundary.begin(), boundary.end());
    }
    if (candidates.empty()) {
        return updatePhysicsDiscrete(
            state, timeStep, profile, boundaryMode, false);
    }
    const ContinuousEventBatch initialBatch = buildEarliestEventBatch(candidates,
        profile.solver.toiToleranceSeconds, profile.solver.maximumIslandSize);
    const double toi = initialBatch.earliestTimeSeconds;

    PhysicsStepTelemetry prefix;
    for (int index = 0; index < kBallCount; ++index) {
        if (state.balls[index].pocketed) continue;
        SurfaceMotionStep motion = advanceSurfaceMotion(state.balls[index],
            static_cast<float>(toi), profile.ball, profile.surface);
        motion.ballIndex = index;
        prefix.surfaceMotion.push_back(motion);
    }
    std::vector<ContinuousContactCandidate> activeCandidates =
        generateBallBallCandidates(state, 0.0, profile.ball.radiusCm,
            profile.solver.toiToleranceSeconds, true,
            profile.solver.residualToleranceCmS);
    if (boundaryMode == PhysicsBoundaryMode::ProductionTable) {
        std::vector<ContinuousContactCandidate> activeBoundary =
            generateBoundaryCandidates(state, 0.0f, profile);
        activeCandidates.insert(activeCandidates.end(),
            activeBoundary.begin(), activeBoundary.end());
    }
    for (const ContinuousContactCandidate& candidate : candidates) {
        if (candidate.timeOfImpactSeconds >
            toi + profile.solver.toiToleranceSeconds) continue;
        ContinuousContactCandidate active = candidate;
        active.timeOfImpactSeconds = 0.0;
        activeCandidates.push_back(active);
    }
    const ContinuousEventBatch batch = buildEarliestEventBatch(
        activeCandidates, profile.solver.toiToleranceSeconds,
        profile.solver.maximumIslandSize);
    bool ballImpulseApplied = false;
    bool railImpulseApplied = false;
    bool hardFailure = batch.failureCode != ContinuousBatchFailureCode::None;
    std::vector<ContinuousContactCandidate> topologyToCommit =
        batch.topologyTransitions;
    const std::array<PocketBoundaryFrame, 6> pocketFrames =
        buildPocketBoundaryFrames(profile.tableBoundary);
    if (hardFailure) {
        failStep(prefix, batchFailureCode(batch.failureCode),
            profile.solver.maximumEventsPerTick - remainingEvents);
    }
    for (const ContactIsland& island : batch.physicalIslands) {
        const ContactSolverResult solved = solveContactIsland(state, island, profile);
        SolverEventRecord solverEvent;
        solverEvent.eventId = profile.solver.maximumEventsPerTick - remainingEvents;
        solverEvent.islandId = island.islandId;
        solverEvent.candidateCount = batchCandidateCount(batch);
        solverEvent.contactCount = static_cast<int>(island.contacts.size());
        solverEvent.duplicateCandidatesRemoved = batch.duplicateCandidatesRemoved;
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
        if (solved.status != ContactSolverStatus::Converged) {
            failStep(prefix, solverFailureCode(solved.status),
                solverEvent.eventId, island.islandId);
        } else if (!std::isfinite(solved.totalKineticEnergyBeforeJ) ||
            !std::isfinite(solved.totalKineticEnergyAfterJ)) {
            hardFailure = true;
            failStep(prefix, PhysicsFailureCode::NonfiniteEnergy,
                solverEvent.eventId, island.islandId);
        } else if (solved.totalKineticEnergyAfterJ >
            solved.totalKineticEnergyBeforeJ +
                profile.solver.passiveEnergyToleranceJ) {
            hardFailure = true;
            failStep(prefix, PhysicsFailureCode::PassiveEnergyCreation,
                solverEvent.eventId, island.islandId);
        }
        prefix.maximumPenetrationCm = std::max(
            prefix.maximumPenetrationCm, solved.maximumPenetrationCm);
        const std::size_t solvedCount = std::min(
            island.contacts.size(), solved.contacts.size());
        for (std::size_t index = 0; index < solvedCount; ++index) {
            const ContinuousContactCandidate& candidate = island.contacts[index];
            PhysicsContactRecord contact;
            contact.solverEventId = solverEvent.eventId;
            contact.solverIslandId = island.islandId;
            const bool ballBall = candidate.kind ==
                ContinuousContactKind::BallBall;
            contact.kind = ballBall ? PhysicsContactKind::BallBall :
                PhysicsContactKind::Rail;
            contact.firstBall = candidate.firstBall;
            contact.secondBall = candidate.secondBall;
            contact.normal = solved.contacts[index].normal;
            contact.penetrationCm = candidate.penetrationCm;
            contact.timeOfImpactSeconds = toi;
            contact.normalImpulseNs = solved.contacts[index].accumulatedNormalImpulseNs;
            contact.tangentialImpulseNs = std::fabs(
                solved.contacts[index].accumulatedTangentialImpulseNs);
            contact.solverResidualCmS = solved.contacts[index].residualCmS;
            contact.solverProjectionCm = solved.contacts[index].projectionCm;
            contact.velocityImpulseApplied = contact.normalImpulseNs > 0.0;
            contact.regime = solved.contacts[index].regime;
            contact.contactTangent = solved.contacts[index].tangent;
            contact.relativeContactVelocityBeforeCmS =
                solved.contacts[index].relativeVelocityBeforeCmS;
            contact.relativeContactVelocityAfterCmS =
                solved.contacts[index].relativeVelocityAfterCmS;
            contact.firstContactArmCm = Point3{
                solved.contacts[index].firstContactArmM.x * 100.0f,
                solved.contacts[index].firstContactArmM.y * 100.0f,
                solved.contacts[index].firstContactArmM.z * 100.0f};
            contact.secondContactArmCm = Point3{
                solved.contacts[index].secondContactArmM.x * 100.0f,
                solved.contacts[index].secondContactArmM.y * 100.0f,
                solved.contacts[index].secondContactArmM.z * 100.0f};
            const Point3 impulse = Point3{
                static_cast<float>(contact.normal.x * contact.normalImpulseNs +
                    contact.contactTangent.x *
                    solved.contacts[index].accumulatedTangentialImpulseNs),
                static_cast<float>(contact.normal.y * contact.normalImpulseNs +
                    contact.contactTangent.y *
                    solved.contacts[index].accumulatedTangentialImpulseNs),
                static_cast<float>(contact.normal.z * contact.normalImpulseNs +
                    contact.contactTangent.z *
                    solved.contacts[index].accumulatedTangentialImpulseNs)};
            if (ballBall) contact.impulseOnSecondNs = impulse;
            else contact.impulseOnBallNs = impulse;
            contact.restitution = solved.contacts[index].restitution;
            contact.frictionCoefficient =
                solved.contacts[index].frictionCoefficient;
            contact.kineticEnergyBeforeJ = solved.kineticEnergyBeforeJ;
            contact.kineticEnergyAfterJ = solved.kineticEnergyAfterJ;
            contact.normalRelativeSpeedBeforeCmS =
                contact.normal.x * contact.relativeContactVelocityBeforeCmS.x +
                contact.normal.y * contact.relativeContactVelocityBeforeCmS.y +
                contact.normal.z * contact.relativeContactVelocityBeforeCmS.z;
            contact.normalRelativeSpeedAfterCmS =
                contact.normal.x * contact.relativeContactVelocityAfterCmS.x +
                contact.normal.y * contact.relativeContactVelocityAfterCmS.y +
                contact.normal.z * contact.relativeContactVelocityAfterCmS.z;
            contact.positionSlopCm = profile.solver.penetrationSlopCm;
            if (!ballBall) {
                contact.cushionRegime = cushionRegime(contact.regime);
                contact.cushionContactArmCm = contact.firstContactArmCm;
                contact.cushionContactHeightCm =
                    contact.cushionContactArmCm.y + std::hypot(
                        contact.cushionContactArmCm.x,
                        contact.cushionContactArmCm.z);
                contact.cushionContactVelocityBeforeCmS =
                    contact.relativeContactVelocityBeforeCmS;
                contact.cushionContactVelocityAfterCmS =
                    contact.relativeContactVelocityAfterCmS;
                contact.positionCorrected = contact.solverProjectionCm > 0.0;
                contact.positionCorrectionCm = Point3{
                    static_cast<float>(contact.normal.x *
                        contact.solverProjectionCm),
                    static_cast<float>(contact.normal.y *
                        contact.solverProjectionCm),
                    static_cast<float>(contact.normal.z *
                        contact.solverProjectionCm)};
                contact.noseHeightRatio = profile.cushion.noseHeightRatio;
                contact.incidentSpeedCmS = std::max(
                    0.0, -contact.normalRelativeSpeedBeforeCmS);
                contact.maximumRigidIncidentSpeedCmS =
                    profile.cushion.maximumRigidIncidentSpeedCmS;
                contact.rigidDomainExceeded = contact.incidentSpeedCmS >
                    contact.maximumRigidIncidentSpeedCmS;
                contact.pocketId = candidate.pocketId;
                contact.pocketBoundaryEvent = candidate.pocketEvent;
            }
            prefix.contacts.push_back(contact);
            ballImpulseApplied = ballImpulseApplied ||
                (ballBall && contact.velocityImpulseApplied);
            railImpulseApplied = railImpulseApplied ||
                (!ballBall && contact.velocityImpulseApplied);

            if (candidate.kind == ContinuousContactKind::Jaw) {
                topologyToCommit.push_back(candidate);
            }
        }
    }
    if (hardFailure) {
        state.ballsMoving = anyBallMoving(state);
        return prefix;
    }
    if (!applyTopologyTransitions(state, topologyToCommit, profile,
            pocketFrames, prefix, toi)) {
        failStep(prefix, PhysicsFailureCode::ContradictoryTopology,
            profile.solver.maximumEventsPerTick - remainingEvents);
        state.ballsMoving = anyBallMoving(state);
        return prefix;
    }
    const float remaining = std::max(0.0f, timeStep - static_cast<float>(toi));
    if (remaining <= 0.0f) {
        if (ballImpulseApplied) state.events.ballCollision = true;
        if (railImpulseApplied) state.events.railCollision = true;
        state.ballsMoving = anyBallMoving(state);
        return prefix;
    }
    const GameplayEvents prefixEvents = state.events;
    PhysicsStepTelemetry tail = updatePhysicsEventDriven(
        state, remaining, profile, remainingEvents - 1, boundaryMode);
    prependTelemetry(tail, prefix);
    mergeGameplayEvents(state.events, prefixEvents);
    if (ballImpulseApplied) state.events.ballCollision = true;
    if (railImpulseApplied) state.events.railCollision = true;
    if (ballImpulseApplied || railImpulseApplied)
        state.ballsMoving = anyBallMoving(state);
    return tail;
}

}  // namespace

PhysicsStepTelemetry updatePhysics(
    GameState& state, float timeStep, const PhysicsProfile& profile)
{
    return updatePhysics(
        state, timeStep, profile, PhysicsBoundaryMode::ProductionTable);
}

PhysicsStepTelemetry updatePhysics(
    GameState& state, float timeStep, const PhysicsProfile& profile,
    PhysicsBoundaryMode boundaryMode)
{
    const GameState snapshot = state;
    PhysicsStepTelemetry telemetry;
    const double energyBefore = totalGameEnergyJ(snapshot, profile);
    if (!finiteGameState(snapshot) || !std::isfinite(energyBefore) ||
        !std::isfinite(timeStep)) {
        failStep(telemetry, !std::isfinite(energyBefore)
            ? PhysicsFailureCode::NonfiniteEnergy
            : PhysicsFailureCode::NonfiniteState);
        return telemetry;
    }
    telemetry = updatePhysicsEventDriven(
        state, timeStep, profile, profile.solver.maximumEventsPerTick,
        boundaryMode);
    const double energyAfter = totalGameEnergyJ(state, profile);
    if (telemetry.stepStatus == PhysicsStepStatus::Succeeded &&
        (!finiteGameState(state) || !std::isfinite(energyAfter))) {
        failStep(telemetry, !std::isfinite(energyAfter)
            ? PhysicsFailureCode::NonfiniteEnergy
            : PhysicsFailureCode::NonfiniteState);
    }
    if (telemetry.stepStatus == PhysicsStepStatus::Succeeded &&
        energyAfter > energyBefore + profile.solver.passiveEnergyToleranceJ) {
        failStep(telemetry, PhysicsFailureCode::PassiveEnergyCreation);
    }
    if (telemetry.stepStatus == PhysicsStepStatus::Failed) state = snapshot;
    return telemetry;
}

PhysicsStepTelemetry updatePhysics(GameState& state, float timeStep)
{
    return updatePhysics(state, timeStep, defaultChinesePoolPhysicsProfile());
}

}  // namespace billiardgl
