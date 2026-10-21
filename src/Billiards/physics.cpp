#include "physics.h"

#include "ball_ball_contact.h"
#include "cushion_contact.h"
#include "continuous_collision.h"
#include "contact_island.h"
#include "contact_solver.h"
#include "pocket_boundary.h"
#include "rules.h"
#include "table_specs.h"
#include "table_departure_geometry.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <limits>
#include <tuple>
#include <sstream>

namespace billiardgl {
namespace {

void appendAnalyticSegments(PhysicsStepTelemetry& telemetry,
    SurfaceMotionStep& motion, int ballIndex, double wholeTickOffsetSeconds,
    std::uint64_t trajectoryGeneration)
{
    motion.ballIndex = ballIndex;
    for (SurfaceMotionAnalyticSegment& segment : motion.analyticSegments) {
        segment.ballIndex = ballIndex;
        segment.trajectoryGeneration = trajectoryGeneration;
        segment.wholeTickStartOffsetSeconds += wholeTickOffsetSeconds;
        telemetry.analyticSurfaceMotion.push_back(segment);
    }
}

struct TraversablePocketMouth {
    bool hit = false;
    int pocketId = -1;
    Point3 inward;
};

TraversablePocketMouth traversablePocketMouth(const Point3& position,
    const PhysicsProfile& profile)
{
    TraversablePocketMouth result;
    const std::array<PocketBoundaryFrame, 6> frames =
        buildPocketBoundaryFrames(profile.tableBoundary);
    for (const PocketBoundaryFrame& frame : frames) {
        const PocketBoundaryQuery query = classifyPocketPoint(
            frame, position, profile.ball.radiusCm);
        if (query.passable && query.local.depthCm >= -profile.ball.radiusCm * 2.0 &&
            query.local.depthCm <= frame.captureDepthCm) {
            result.hit = true;
            result.pocketId = frame.pocketId;
            result.inward = frame.inward;
            return result;
        }
    }
    return result;
}

struct StraightRailEvent {
    bool hit = false;
    bool mouthCrossed = false;
    int pocketId = -1;
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
    const bool legacy = profile.formulaVersion == "legacy_v1";
    if ((legacy && (ball.pocketInteraction.phase ==
                PocketInteractionPhase::Approaching ||
            ball.pocketInteraction.phase ==
                PocketInteractionPhase::JawContact)) ||
        ball.pocketInteraction.phase == PocketInteractionPhase::ThroatCrossed ||
        ball.pocketInteraction.phase == PocketInteractionPhase::Captured) {
        return event;
    }
    event.xAxis = xAxis;
    const float limit = (xAxis ? kTableInWidth : kTableInLength) / 2.0f -
        profile.ball.radiusCm;
    const float start = coordinate(ball, xAxis);
    const float component = velocity(ball, xAxis);
    bool crossedFromPlayfield = false;
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
        crossedFromPlayfield = true;
    }
    event.boundaryCm = side * limit;
    if (xAxis) event.inwardNormal.x = -side;
    else event.inwardNormal.z = -side;

    BallState contact = advancedCopy(ball, event.timeSeconds, profile);
    setCoordinate(contact, xAxis, event.boundaryCm);
    const TraversablePocketMouth mouth = traversablePocketMouth(
        contact.position, profile);
    if (mouth.hit) {
        if (legacy) {
            event.mouthCrossed = crossedFromPlayfield;
            event.pocketId = mouth.pocketId;
            event.inwardNormal = mouth.inward;
        }
        return event;
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

const PocketBoundaryFrame* legacyCaptureFrame(const BallState& ball,
    const PhysicsProfile& profile,
    const std::array<PocketBoundaryFrame, 6>& frames)
{
    if (profile.formulaVersion != "legacy_v1") return nullptr;
    const double halfWidth = profile.tableBoundary.playfieldWidthCm * 0.5;
    const double halfLength = profile.tableBoundary.playfieldLengthCm * 0.5;
    for (const PocketBoundaryFrame& frame : frames) {
        const double halfMouth = frame.mouthWidthCm * 0.5 +
            profile.ball.radiusCm;
        bool insideBand = false;
        bool crossedDropZone = false;
        if (frame.kind == PocketKind::Side) {
            insideBand = std::fabs(ball.position.z - frame.mouthCenter.z) <=
                    halfMouth &&
                (frame.mouthCenter.x < 0.0f
                    ? ball.position.x <= 0.0f : ball.position.x >= 0.0f);
            crossedDropZone = frame.mouthCenter.x < 0.0f
                ? ball.position.x <= -halfWidth + frame.captureDepthCm
                : ball.position.x >= halfWidth - frame.captureDepthCm;
        } else {
            insideBand =
                std::fabs(ball.position.x - frame.mouthCenter.x) <= halfMouth &&
                std::fabs(ball.position.z - frame.mouthCenter.z) <= halfMouth;
            const bool crossedX = frame.mouthCenter.x < 0.0f
                ? ball.position.x <= -halfWidth + frame.captureDepthCm
                : ball.position.x >= halfWidth - frame.captureDepthCm;
            const bool crossedZ = frame.mouthCenter.z < 0.0f
                ? ball.position.z <= -halfLength + frame.captureDepthCm
                : ball.position.z >= halfLength - frame.captureDepthCm;
            crossedDropZone = crossedX || crossedZ;
        }
        if (insideBand && crossedDropZone) return &frame;
    }
    return nullptr;
}

const PocketBoundaryFrame* activePocketFrame(
    const std::array<PocketBoundaryFrame, 6>& frames, int pocketId);

std::vector<ContinuousContactCandidate> generateBoundaryCandidates(
    const GameState& state, float timeStep, const PhysicsProfile& profile)
{
    std::vector<ContinuousContactCandidate> result;
    const std::array<PocketBoundaryFrame, 6> frames =
        buildPocketBoundaryFrames(profile.tableBoundary);
    for (int ballIndex = 0; ballIndex < kBallCount; ++ballIndex) {
        const BallState& ball = state.balls[ballIndex];
        if (ball.pocketed) continue;
        if (ball.physicalDomain == PhysicalBallDomain::PocketChannel) {
            const auto* frame = activePocketFrame(frames, ball.pocketInteraction.pocketId);
            if (!frame) continue;
            const bool capture = ball.pocketInteraction.phase == PocketInteractionPhase::ThroatCrossed;
            const double plane = profile.tableDeparture.clothSurfaceYCm -
                (capture ? profile.ball.radiusCm : 0.0f);
            const double gravity = profile.tableDeparture.gravityYCmS2;
            const double height = double(ball.position.y) - plane;
            const double vy = ball.velocity.y;
            const double discriminant = vy * vy - 2.0 * gravity * height;
            double toi = std::numeric_limits<double>::infinity();
            if (height <= 0.0 && vy <= 0.0) toi = 0.0;
            else if (gravity < 0.0 && discriminant >= 0.0)
                toi = (-vy - std::sqrt(discriminant)) / gravity;
            if (toi >= 0.0 && toi <= timeStep) {
                const auto event = capture ? PocketBoundaryEventKind::Capture : PocketBoundaryEventKind::Throat;
                result.push_back(boundaryContactCandidate(ballIndex,
                    pocketFeatureId(frame->pocketId, event), toi, Point3{0.0f, -1.0f, 0.0f}, event, frame->pocketId));
            }
            continue;
        }
        const PocketBoundaryFrame* legacyFrame = legacyCaptureFrame(
            ball, profile, frames);
        if (legacyFrame != nullptr &&
            ball.pocketInteraction.phase != PocketInteractionPhase::Rejected &&
            ball.pocketInteraction.phase != PocketInteractionPhase::Captured) {
            PocketBoundaryEventKind event = PocketBoundaryEventKind::Mouth;
            if (ball.pocketInteraction.phase ==
                    PocketInteractionPhase::Approaching ||
                ball.pocketInteraction.phase ==
                    PocketInteractionPhase::JawContact) {
                event = PocketBoundaryEventKind::Throat;
            } else if (ball.pocketInteraction.phase ==
                    PocketInteractionPhase::ThroatCrossed) {
                event = PocketBoundaryEventKind::Capture;
            }
            result.push_back(boundaryContactCandidate(ballIndex,
                pocketFeatureId(legacyFrame->pocketId, event), 0.0,
                legacyFrame->inward, event, legacyFrame->pocketId));
            continue;
        }
        if (profile.formulaVersion == "legacy_v1" &&
            ball.pocketInteraction.phase !=
                PocketInteractionPhase::Rejected &&
            ball.pocketInteraction.phase != PocketInteractionPhase::Captured &&
            ball.pocketInteraction.pocketId >= 0) {
            const PocketBoundaryFrame* frame = activePocketFrame(
                frames, ball.pocketInteraction.pocketId);
            if (frame != nullptr) {
                const PocketBoundaryQuery query = classifyPocketPoint(
                    *frame, ball.position, profile.ball.radiusCm);
                const bool needsRegionSync =
                    query.region == PocketBoundaryRegion::Solid ||
                    (ball.pocketInteraction.phase ==
                            PocketInteractionPhase::ThroatCrossed &&
                        (query.region == PocketBoundaryRegion::Outside ||
                         query.region == PocketBoundaryRegion::Approaching));
                if (needsRegionSync) {
                    PocketInteractionState next = ball.pocketInteraction;
                    const PocketTransitionResult transition =
                        advancePocketInteraction(next, frame->pocketId,
                            PocketBoundaryEventKind::Mouth, query.region, 0);
                    if (transition.changed) {
                        result.push_back(boundaryContactCandidate(ballIndex,
                            pocketFeatureId(frame->pocketId,
                                PocketBoundaryEventKind::Mouth),
                            0.0, frame->inward,
                            PocketBoundaryEventKind::Mouth,
                            frame->pocketId));
                    }
                }
            }
        }
        for (bool xAxis : {true, false}) {
            const StraightRailEvent rail = railCandidate(
                ball, timeStep, profile, xAxis);
            if (rail.mouthCrossed) {
                result.push_back(boundaryContactCandidate(ballIndex,
                    pocketFeatureId(rail.pocketId,
                        PocketBoundaryEventKind::Mouth),
                    rail.timeSeconds, rail.inwardNormal,
                    PocketBoundaryEventKind::Mouth, rail.pocketId));
            }
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
    const CushionContactResult& result, double timeOfImpactSeconds,
    int boundaryFeatureId = -1)
{
    PhysicsContactRecord contact;
    contact.kind = PhysicsContactKind::Rail;
    contact.firstBall = ballIndex;
    contact.boundaryFeatureId = boundaryFeatureId;
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
    contact.wholeTickTimeOfImpactSeconds = timeOfImpactSeconds;
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
    contact.wholeTickTimeOfImpactSeconds = timeOfImpactSeconds;
    contact.pocketId = frame.pocketId;
    contact.pocketKind = frame.kind;
    contact.pocketBoundaryEvent = event.kind;
    contact.pocketPhaseBefore = transition.previous;
    contact.pocketPhaseAfter = transition.current;
    contact.firstPositionAtContactCm = event.position;
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
    if (traversablePocketMouth(ball.position, profile).hit) {
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
    return traversablePocketMouth(
        ball.position, defaultChinesePoolPhysicsProfile()).hit;
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
        if (ball.trajectoryGeneration == std::numeric_limits<std::uint64_t>::max())
            return false;
        ++ball.trajectoryGeneration;
        ball.position = Point3{0.0f, kTableHeight + kBallRadius, -kTableInLength / 4.0f};
        ball.physicalDomain = PhysicalBallDomain::ClothSupported;
        state.players.illegalShot = true;
        state.events.cueBallPocketed = true;
    } else {
        ball.pocketed = true;
        ball.physicalDomain = PhysicalBallDomain::PocketChannel;
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
    PhysicsBoundaryMode boundaryMode, bool resolveBallContacts = true,
    double wholeTickOffsetSeconds = 0.0)
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
                const Point3 firstPosition = ball.position;
                const Point3 secondPosition = state.balls[j].position;
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
                    contact.timeOfImpactSeconds = wholeTickOffsetSeconds;
                    contact.wholeTickTimeOfImpactSeconds =
                        wholeTickOffsetSeconds;
                    contact.firstPositionAtContactCm = firstPosition;
                    contact.secondPositionAtContactCm = secondPosition;
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
            SurfaceMotionStep beforeContact = advanceSurfaceMotion(ball,
                pocketEvent.timeSeconds, profile.ball, profile.surface);
            appendAnalyticSegments(telemetry, beforeContact, i,
                wholeTickOffsetSeconds, ball.trajectoryGeneration);
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
                        wholeTickOffsetSeconds + pocketEvent.timeSeconds);
                }
                if (jaw.velocityImpulseApplied) state.events.railCollision = true;
            }
            appendPocketContact(telemetry, i, pocketEvent.frame,
                pocketEvent.boundary, query, transition,
                transition.captureEmitted
                    ? ball.pocketInteraction.captureSequence : 0,
                wholeTickOffsetSeconds + pocketEvent.timeSeconds);
            const float remaining = std::max(
                0.0f, timeStep - pocketEvent.timeSeconds);
            SurfaceMotionStep afterContact = advanceSurfaceMotion(
                ball, remaining, profile.ball, profile.surface);
            appendAnalyticSegments(telemetry, afterContact, i,
                wholeTickOffsetSeconds + pocketEvent.timeSeconds, ball.trajectoryGeneration);
            surface = beforeContact;
            surface.after = afterContact.after;
            surface.finalSlipSpeedCmS = afterContact.finalSlipSpeedCmS;
            if (boundaryMode == PhysicsBoundaryMode::ProductionTable) {
                classifyFinalPocketState(state, ball, profile, telemetry, i,
                    wholeTickOffsetSeconds + timeStep);
            }
        } else if (railEvent.hit) {
            SurfaceMotionStep beforeContact = advanceSurfaceMotion(
                ball, railEvent.timeSeconds, profile.ball, profile.surface);
            appendAnalyticSegments(telemetry, beforeContact, i,
                wholeTickOffsetSeconds, ball.trajectoryGeneration);
            setCoordinate(ball, railEvent.xAxis, railEvent.boundaryCm);
            const CushionContactResult rail = resolveCushionContact(
                ball, railEvent.inwardNormal, railEvent.penetrationM,
                profile.ball, profile.cushion);
            if (rail.velocityImpulseApplied || rail.positionCorrected) {
                appendRailContact(telemetry, i, rail,
                    wholeTickOffsetSeconds + railEvent.timeSeconds,
                    straightRailFeatureId(railEvent));
            }
            if (rail.velocityImpulseApplied) state.events.railCollision = true;
            const float remaining = std::max(
                0.0f, timeStep - railEvent.timeSeconds);
            SurfaceMotionStep afterContact = advanceSurfaceMotion(
                ball, remaining, profile.ball, profile.surface);
            appendAnalyticSegments(telemetry, afterContact, i,
                wholeTickOffsetSeconds + railEvent.timeSeconds, ball.trajectoryGeneration);
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
                    wholeTickOffsetSeconds + timeStep);
            }
        } else {
            surface = advanceSurfaceMotion(
                ball, timeStep, profile.ball, profile.surface);
            appendAnalyticSegments(telemetry, surface, i,
                wholeTickOffsetSeconds, ball.trajectoryGeneration);
            if (boundaryMode == PhysicsBoundaryMode::ProductionTable) {
                classifyFinalPocketState(state, ball, profile, telemetry, i,
                    wholeTickOffsetSeconds + timeStep);
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
                contact.timeOfImpactSeconds =
                    wholeTickOffsetSeconds + timeStep;
                contact.wholeTickTimeOfImpactSeconds =
                    wholeTickOffsetSeconds + timeStep;
                contact.pocketBoundaryEvent = PocketBoundaryEventKind::Capture;
                telemetry.contacts.push_back(contact);
            }
        }
        surface.ballIndex = i;
        telemetry.surfaceMotion.push_back(surface);
        if (ball.speed > 0.0f ||
            (profile.surface.torsionalSpinDecelerationRadS2 > 0.0f &&
                ball.angularVelocity.y != 0.0f)) {
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
    tail.tableDepartureEvents.insert(tail.tableDepartureEvents.begin(),
        prefix.tableDepartureEvents.begin(), prefix.tableDepartureEvents.end());
    tail.surfaceMotion.insert(tail.surfaceMotion.begin(),
        prefix.surfaceMotion.begin(), prefix.surfaceMotion.end());
    tail.analyticSurfaceMotion.insert(tail.analyticSurfaceMotion.begin(),
        prefix.analyticSurfaceMotion.begin(),
        prefix.analyticSurfaceMotion.end());
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

double totalMechanicalEnergyIncludingGravityJ(const GameState& state,
    const PhysicsProfile& profile)
{
    double energy = totalGameEnergyJ(state, profile);
    for (const BallState& ball : state.balls) {
        if (ball.pocketed) continue;
        energy += -static_cast<double>(profile.ball.massKg) *
            static_cast<double>(profile.tableDeparture.gravityYCmS2) *
            static_cast<double>(ball.position.y) / 10000.0;
    }
    return energy;
}

void clearCanonicalTelemetry(PhysicsStepTelemetry& telemetry)
{
    telemetry.contacts.clear();
    telemetry.solverEvents.clear();
    telemetry.tableDepartureEvents.clear();
    telemetry.surfaceMotion.clear();
    telemetry.analyticSurfaceMotion.clear();
    telemetry.planeCrossings.clear();
    telemetry.planeAttestations.clear();
}

bool verifyPrivateAnalyticLineage(const TableDepartureStepResult& result,
    const GameState& before, const GameState& after, double duration,
    const PhysicsProfile& profile)
{
    if (!result.ok || !std::isfinite(duration) || duration < 0.0)
        return false;
    if (duration == 0.0) {
        if (!result.analyticSegments.empty() || !result.events.empty()) return false;
        for (int i = 0; i < kBallCount; ++i) {
            const auto& a = before.balls[i];
            const auto& b = after.balls[i];
            if (a.position.x != b.position.x || a.position.y != b.position.y ||
                a.position.z != b.position.z || a.velocity.x != b.velocity.x ||
                a.velocity.y != b.velocity.y || a.velocity.z != b.velocity.z ||
                a.angularVelocity.x != b.angularVelocity.x ||
                a.angularVelocity.y != b.angularVelocity.y ||
                a.angularVelocity.z != b.angularVelocity.z ||
                a.rotationAxis.x != b.rotationAxis.x ||
                a.rotationAxis.y != b.rotationAxis.y ||
                a.rotationAxis.z != b.rotationAxis.z ||
                a.speed != b.speed || a.rotationAngle != b.rotationAngle ||
                a.motionState != b.motionState || a.texture != b.texture ||
                a.trajectoryGeneration != b.trajectoryGeneration ||
                a.physicalDomain != b.physicalDomain || a.pocketed != b.pocketed ||
                a.pocketInteraction.phase != b.pocketInteraction.phase ||
                a.pocketInteraction.pocketId != b.pocketInteraction.pocketId ||
                a.pocketInteraction.captureSequence != b.pocketInteraction.captureSequence)
                return false;
        }
        return true;
    }
    const double timeTolerance = profile.solver.toiToleranceSeconds;
    const double positionTolerance = profile.solver.penetrationSlopCm;
    const double velocityTolerance = profile.solver.residualToleranceCmS;
    const auto finitePoint = [](const Point3& p) {
        return std::isfinite(p.x) && std::isfinite(p.y) && std::isfinite(p.z);
    };
    const auto samePosition = [positionTolerance](const Point3& a,
        const Point3& b) {
        return std::fabs(double(a.x) - b.x) <= positionTolerance &&
            std::fabs(double(a.y) - b.y) <= positionTolerance &&
            std::fabs(double(a.z) - b.z) <= positionTolerance;
    };
    const auto eventForBallAt = [&](int ballIndex, double time)
        -> const TableDepartureEvent* {
        for (auto it = result.events.rbegin(); it != result.events.rend(); ++it) {
            const auto& event = *it;
            if (!std::isfinite(event.absoluteWholeTickTimeSeconds) ||
                std::fabs(event.absoluteWholeTickTimeSeconds - time) > timeTolerance)
                continue;
            if (event.ballIndex == ballIndex) return &event;
            if (event.kind == TableDepartureEventKind::CrossDomainBallContact) {
                const std::string prefix = "cross_domain_ball_contact:" +
                    std::to_string(event.ballIndex) + ":" +
                    std::to_string(ballIndex) + ":";
                if (event.stableIdentity.compare(0, prefix.size(), prefix) == 0)
                    return &event;
            }
        }
        return nullptr;
    };
    const auto velocityMatches = [velocityTolerance](double expected,
        float actual) {
        return std::isfinite(expected) && std::isfinite(actual) &&
            std::fabs(expected - actual) <= velocityTolerance;
    };
    std::vector<std::string> eventIdentities;
    for (const auto& event : result.events) {
        const double time = event.absoluteWholeTickTimeSeconds;
        if (event.ballIndex < 0 || event.ballIndex >= kBallCount ||
            before.balls[event.ballIndex].pocketed ||
            before.balls[event.ballIndex].physicalDomain == PhysicalBallDomain::PocketChannel ||
            !std::isfinite(time) || time <= 0.0 || time > duration + timeTolerance ||
            event.scope != TableDepartureEventScope::PhysicsPrivateDiagnostic ||
            !event.transitionWitnessComplete || !finitePoint(event.velocityAfterCmS) ||
            event.stableIdentity.empty() ||
            std::find(eventIdentities.begin(), eventIdentities.end(),
                event.stableIdentity) != eventIdentities.end())
            return false;
        eventIdentities.push_back(event.stableIdentity);
        int second = -1;
        const char* kindName = nullptr;
        switch (event.kind) {
        case TableDepartureEventKind::CushionNoseContact: kindName = "nose"; break;
        case TableDepartureEventKind::OuterDeparture: kindName = "departure"; break;
        case TableDepartureEventKind::RailTopLanding: kindName = "rail_top"; break;
        case TableDepartureEventKind::OutsideLanding: kindName = "outside_landing"; break;
        case TableDepartureEventKind::ClothReturn: kindName = "cloth_return"; break;
        case TableDepartureEventKind::PocketEntry: kindName = "pocket_entry"; break;
        case TableDepartureEventKind::CrossDomainBallContact: break;
        default: return false;
        }
        if (event.kind == TableDepartureEventKind::CrossDomainBallContact) {
            for (int i = 0; i < kBallCount; ++i) {
                const std::string prefix = "cross_domain_ball_contact:" +
                    std::to_string(event.ballIndex) + ":" + std::to_string(i) + ":";
                if (event.stableIdentity.compare(0, prefix.size(), prefix) == 0)
                    second = i;
            }
            if (second < 0 || second == event.ballIndex || before.balls[second].pocketed ||
                before.balls[second].physicalDomain == PhysicalBallDomain::PocketChannel)
                return false;
        }
        std::ostringstream expectedIdentity;
        if (second >= 0)
            expectedIdentity << "cross_domain_ball_contact:" << event.ballIndex
                << ':' << second << ':' << event.ordinal << ':' << time;
        else
            expectedIdentity << event.ballIndex << ':' << event.ordinal << ':'
                << kindName << ':' << time;
        if (event.ordinal != static_cast<int>(eventIdentities.size()) - 1 ||
            event.stableIdentity != expectedIdentity.str()) return false;
        const auto validPostState = [&](const Point3& velocity, PhysicalBallDomain domain) {
            if (!finitePoint(velocity)) return false;
            switch (domain) {
            case PhysicalBallDomain::Airborne:
            case PhysicalBallDomain::PocketChannel: return true;
            case PhysicalBallDomain::ClothSupported:
            case PhysicalBallDomain::RailTopSupported:
            case PhysicalBallDomain::OutsideLandingSupported: return velocity.y == 0.0f;
            }
            return false;
        };
        if (!validPostState(event.velocityAfterCmS, event.targetDomain) ||
            (second >= 0 && (!validPostState(event.secondVelocityAfterCmS,
                event.secondTargetDomain) || event.targetDomain == PhysicalBallDomain::PocketChannel ||
                event.secondTargetDomain == PhysicalBallDomain::PocketChannel))) return false;
        switch (event.kind) {
        case TableDepartureEventKind::ClothReturn:
            if (event.targetDomain != PhysicalBallDomain::ClothSupported) return false; break;
        case TableDepartureEventKind::RailTopLanding:
            if (event.targetDomain != PhysicalBallDomain::RailTopSupported) return false; break;
        case TableDepartureEventKind::OutsideLanding:
            if (event.targetDomain != PhysicalBallDomain::OutsideLandingSupported) return false; break;
        case TableDepartureEventKind::PocketEntry:
            if (event.targetDomain != PhysicalBallDomain::PocketChannel) return false; break;
        case TableDepartureEventKind::CushionNoseContact:
        case TableDepartureEventKind::OuterDeparture:
            if (event.targetDomain != PhysicalBallDomain::Airborne) return false; break;
        case TableDepartureEventKind::CrossDomainBallContact: break;
        }
        for (int i : {event.ballIndex, second}) {
            if (i < 0) continue;
            bool preceding = time == 0.0;
            bool following = time >= duration ||
                event.kind == TableDepartureEventKind::PocketEntry;
            for (const auto& segment : result.analyticSegments) {
                if (segment.ballIndex != i) continue;
                preceding = preceding || std::fabs(
                    segment.wholeTickStartOffsetSeconds + segment.durationSeconds -
                    time) <= timeTolerance;
                following = following || std::fabs(
                    segment.wholeTickStartOffsetSeconds - time) <= timeTolerance;
                if (segment.wholeTickStartOffsetSeconds < time - timeTolerance &&
                    segment.wholeTickStartOffsetSeconds + segment.durationSeconds >
                        time + timeTolerance)
                    return false;
            }
            if (!preceding || !following) return false;
        }
    }
    std::array<double, kBallCount> covered{};
    std::array<int, kBallCount> ordinals{};
    std::array<Point3, kBallCount> endpoints{};
    std::array<const SurfaceMotionAnalyticSegment*, kBallCount> last{};
    std::tuple<double, int, int> previousKey{-1.0, -1, -1};
    for (int i = 0; i < kBallCount; ++i)
        endpoints[i] = before.balls[i].position;
    for (const auto& segment : result.analyticSegments) {
        const int i = segment.ballIndex;
        const auto key = std::make_tuple(segment.wholeTickStartOffsetSeconds,
            i, segment.segmentOrdinal);
        if (i < 0 || i >= kBallCount || before.balls[i].pocketed ||
            before.balls[i].physicalDomain == PhysicalBallDomain::PocketChannel ||
            !segment.analyticPathComplete ||
            segment.trajectoryGeneration != before.balls[i].trajectoryGeneration ||
            after.balls[i].trajectoryGeneration != before.balls[i].trajectoryGeneration ||
            segment.segmentOrdinal != ordinals[i]++ ||
            !std::isfinite(segment.wholeTickStartOffsetSeconds) ||
            !std::isfinite(segment.durationSeconds) ||
            segment.durationSeconds <= 0.0 ||
            !finitePoint(segment.startPositionCm) ||
            !finitePoint(segment.endPositionCm) ||
            !finitePoint(segment.startVelocityCmS) ||
            !finitePoint(segment.constantAccelerationCmS2) ||
            key <= previousKey ||
            (segment.motionState != BallMotionState::Stationary &&
             segment.motionState != BallMotionState::Sliding &&
             segment.motionState != BallMotionState::Rolling) ||
            std::fabs(segment.wholeTickStartOffsetSeconds - covered[i]) >
                timeTolerance ||
            !samePosition(endpoints[i], segment.startPositionCm))
            return false;
        previousKey = key;
        const bool ballistic = segment.constantAccelerationCmS2.y ==
            profile.tableDeparture.gravityYCmS2;
        const bool stationary = segment.startVelocityCmS.x == 0.0f &&
            segment.startVelocityCmS.y == 0.0f && segment.startVelocityCmS.z == 0.0f &&
            segment.constantAccelerationCmS2.x == 0.0f &&
            segment.constantAccelerationCmS2.y == 0.0f &&
            segment.constantAccelerationCmS2.z == 0.0f;
        if (segment.motionState != (ballistic ? BallMotionState::Sliding :
                stationary ? BallMotionState::Stationary : BallMotionState::Rolling))
            return false;
        if (ballistic) {
            if (segment.constantAccelerationCmS2.x != 0.0f ||
                segment.constantAccelerationCmS2.z != 0.0f) return false;
        } else if (!stationary) {
            const double speed = std::hypot(segment.startVelocityCmS.x,
                segment.startVelocityCmS.z);
            const float deceleration =
                profile.tableDeparture.supportedRollingDecelerationCmS2;
            if (!(speed > 0.0) || segment.startVelocityCmS.y != 0.0f ||
                segment.constantAccelerationCmS2.y != 0.0f ||
                segment.constantAccelerationCmS2.x != static_cast<float>(
                    -deceleration * segment.startVelocityCmS.x / speed) ||
                segment.constantAccelerationCmS2.z != static_cast<float>(
                    -deceleration * segment.startVelocityCmS.z / speed) ||
                (deceleration > 0.0 && segment.durationSeconds >
                    speed / deceleration + timeTolerance)) return false;
        }
        const auto* boundaryEvent = eventForBallAt(i, segment.wholeTickStartOffsetSeconds);
        if (!boundaryEvent) {
            const auto* preceding = last[i];
            Point3 initialVelocity = before.balls[i].velocity;
            // The supported normal reaction is applied before the first interval.
            if (!preceding && before.balls[i].physicalDomain !=
                    PhysicalBallDomain::Airborne && initialVelocity.y < 0.0f)
                initialVelocity.y = 0.0f;
            const auto atBoundary = [&](float initial, float start,
                float acceleration) {
                return preceding ? double(start) + double(acceleration) *
                    preceding->durationSeconds : double(initial);
            };
            if (!velocityMatches(atBoundary(initialVelocity.x,
                    preceding ? preceding->startVelocityCmS.x : 0.0f,
                    preceding ? preceding->constantAccelerationCmS2.x : 0.0f),
                    segment.startVelocityCmS.x) ||
                !velocityMatches(atBoundary(initialVelocity.y,
                    preceding ? preceding->startVelocityCmS.y : 0.0f,
                    preceding ? preceding->constantAccelerationCmS2.y : 0.0f),
                    segment.startVelocityCmS.y) ||
                !velocityMatches(atBoundary(initialVelocity.z,
                    preceding ? preceding->startVelocityCmS.z : 0.0f,
                    preceding ? preceding->constantAccelerationCmS2.z : 0.0f),
                    segment.startVelocityCmS.z))
                return false;
        } else {
            const auto& velocity = boundaryEvent->ballIndex == i ?
                boundaryEvent->velocityAfterCmS : boundaryEvent->secondVelocityAfterCmS;
            if (!velocityMatches(velocity.x, segment.startVelocityCmS.x) ||
                !velocityMatches(velocity.y, segment.startVelocityCmS.y) ||
                !velocityMatches(velocity.z, segment.startVelocityCmS.z)) return false;
        }
        if (segment.motionState == BallMotionState::Stationary &&
            (segment.startVelocityCmS.x != 0.0f ||
             segment.startVelocityCmS.y != 0.0f ||
             segment.startVelocityCmS.z != 0.0f ||
             segment.constantAccelerationCmS2.x != 0.0f ||
             segment.constantAccelerationCmS2.y != 0.0f ||
             segment.constantAccelerationCmS2.z != 0.0f))
            return false;
        const double t = segment.durationSeconds;
        const auto integratedCoordinate = [t](float p, float v, float a) {
            return double(p) + double(v) * t + 0.5 * double(a) * t * t;
        };
        if (std::fabs(integratedCoordinate(segment.startPositionCm.x,
                segment.startVelocityCmS.x, segment.constantAccelerationCmS2.x) -
                segment.endPositionCm.x) > positionTolerance ||
            std::fabs(integratedCoordinate(segment.startPositionCm.y,
                segment.startVelocityCmS.y, segment.constantAccelerationCmS2.y) -
                segment.endPositionCm.y) > positionTolerance ||
            std::fabs(integratedCoordinate(segment.startPositionCm.z,
                segment.startVelocityCmS.z, segment.constantAccelerationCmS2.z) -
                segment.endPositionCm.z) > positionTolerance)
            return false;
        covered[i] = segment.wholeTickStartOffsetSeconds + t;
        if (covered[i] > duration + timeTolerance) return false;
        endpoints[i] = segment.endPositionCm;
        last[i] = &segment;
    }
    for (int i = 0; i < kBallCount; ++i) {
        if (before.balls[i].pocketed || before.balls[i].physicalDomain ==
                PhysicalBallDomain::PocketChannel)
            continue;
        double requiredDuration = duration;
        if (after.balls[i].physicalDomain == PhysicalBallDomain::PocketChannel) {
            bool foundEntry = false;
            for (const auto& event : result.events) {
                if (event.ballIndex == i && event.kind ==
                        TableDepartureEventKind::PocketEntry) {
                    if (foundEntry) return false;
                    requiredDuration = event.absoluteWholeTickTimeSeconds;
                    foundEntry = true;
                }
            }
            if (!foundEntry) return false;
        }
        if (ordinals[i] == 0 ||
            std::fabs(covered[i] - requiredDuration) > timeTolerance ||
            !samePosition(endpoints[i], after.balls[i].position))
            return false;
        const auto* terminalEvent = eventForBallAt(i, requiredDuration);
        if (terminalEvent) {
            const auto& velocity = terminalEvent->ballIndex == i ?
                terminalEvent->velocityAfterCmS : terminalEvent->secondVelocityAfterCmS;
            const auto domain = terminalEvent->ballIndex == i ?
                terminalEvent->targetDomain : terminalEvent->secondTargetDomain;
            if (after.balls[i].physicalDomain != domain ||
                !velocityMatches(velocity.x, after.balls[i].velocity.x) ||
                !velocityMatches(velocity.y, after.balls[i].velocity.y) ||
                !velocityMatches(velocity.z, after.balls[i].velocity.z)) return false;
        } else if (after.balls[i].physicalDomain !=
                PhysicalBallDomain::PocketChannel) {
            const auto& segment = *last[i];
            const auto velocityMatches = [&](float start, float acceleration,
                float final) {
                const double expected = double(start) + double(acceleration) *
                    segment.durationSeconds;
                return std::isfinite(expected) &&
                    std::fabs(expected - final) <=
                        profile.solver.residualToleranceCmS;
            };
            if (!velocityMatches(segment.startVelocityCmS.x,
                    segment.constantAccelerationCmS2.x, after.balls[i].velocity.x) ||
                !velocityMatches(segment.startVelocityCmS.y,
                    segment.constantAccelerationCmS2.y, after.balls[i].velocity.y) ||
                !velocityMatches(segment.startVelocityCmS.z,
                    segment.constantAccelerationCmS2.z, after.balls[i].velocity.z))
                return false;
        }
    }
    return true;
}

void appendRebasedPrivateAnalyticSegments(PhysicsStepTelemetry& telemetry,
    const TableDepartureStepResult& result, double prefixDuration)
{
    for (auto segment : result.analyticSegments) {
        segment.wholeTickStartOffsetSeconds += prefixDuration;
        segment.segmentOrdinal = static_cast<int>(telemetry.analyticSurfaceMotion.size());
        telemetry.analyticSurfaceMotion.push_back(segment);
    }
}

bool verifyComposedAnalyticPartition(const PhysicsStepTelemetry& telemetry,
    const GameState& before, const GameState& after,
    const PhysicsProfile& profile, std::string* diagnostic = nullptr)
{
    const auto reject = [&](int line) {
        if (diagnostic && diagnostic->empty())
            *diagnostic = "composed_partition_line:" + std::to_string(line);
        return false;
    };
    if (!std::isfinite(telemetry.wholeTickDurationSeconds) ||
        telemetry.wholeTickDurationSeconds < 0.0) return reject(__LINE__);
    std::array<double, kBallCount> covered{};
    std::array<Point3, kBallCount> endpoints{};
    std::array<std::uint64_t, kBallCount> generations{};
    for (const auto& ball : before.balls) {
        if (ball.pocketed || ball.physicalDomain != PhysicalBallDomain::PocketChannel) continue;
        if (ball.pocketInteraction.pocketId < 0 || ball.pocketInteraction.pocketId >= 6 ||
            (ball.pocketInteraction.phase != PocketInteractionPhase::Approaching &&
             ball.pocketInteraction.phase != PocketInteractionPhase::JawContact &&
             ball.pocketInteraction.phase != PocketInteractionPhase::ThroatCrossed) ||
            ball.velocity.y > 0.0f)
            return reject(__LINE__);
    }
    for (int i = 0; i < kBallCount; ++i) endpoints[i] = before.balls[i].position;
    for (int i = 0; i < kBallCount; ++i) generations[i] = before.balls[i].trajectoryGeneration;
    const auto samePosition = [&](const Point3& a, const Point3& b) {
        return std::isfinite(a.x) && std::isfinite(a.y) && std::isfinite(a.z) &&
            std::isfinite(b.x) && std::isfinite(b.y) && std::isfinite(b.z) &&
            std::fabs(double(a.x) - b.x) <= profile.solver.penetrationSlopCm &&
            std::fabs(double(a.y) - b.y) <= profile.solver.penetrationSlopCm &&
            std::fabs(double(a.z) - b.z) <= profile.solver.penetrationSlopCm;
    };
    std::vector<const SolverPositionProjection*> consumedProjections;
    const auto exactPosition = [](const Point3& a, const Point3& b) {
        return a.x == b.x && a.y == b.y && a.z == b.z;
    };
    for (const auto& contact : telemetry.contacts) {
        if (!contact.positionCorrected &&
            exactPosition(contact.firstPositionCorrectionCm, Point3{}) &&
            exactPosition(contact.secondPositionCorrectionCm, Point3{})) continue;
        int matchingEvents = 0;
        for (const auto& event : telemetry.solverEvents)
            if (event.eventId == contact.solverEventId &&
                event.islandId == contact.solverIslandId &&
                !event.positionProjections.empty() &&
                std::fabs(event.wholeTickTimeOfImpactSeconds -
                    contact.wholeTickTimeOfImpactSeconds) <= profile.solver.toiToleranceSeconds)
                ++matchingEvents;
        if (matchingEvents != 1) return reject(__LINE__);
    }
    std::vector<std::pair<int, int>> projectionEventKeys;
    for (const auto& event : telemetry.solverEvents) {
        if (event.positionProjections.empty()) continue;
        const auto eventKey = std::make_pair(event.eventId, event.islandId);
        if (std::find(projectionEventKeys.begin(), projectionEventKeys.end(), eventKey) !=
                projectionEventKeys.end()) return reject(__LINE__);
        projectionEventKeys.push_back(eventKey);
        if (!event.failureCode || std::string(event.failureCode) != "converged" ||
            event.eventId < 0 || event.islandId < 0 || event.islandLimitExceeded ||
            !std::isfinite(event.wholeTickTimeOfImpactSeconds) ||
            event.wholeTickTimeOfImpactSeconds < 0.0 ||
            event.wholeTickTimeOfImpactSeconds > telemetry.wholeTickDurationSeconds)
            return reject(__LINE__);
        std::vector<const PhysicsContactRecord*> contacts;
        for (const auto& contact : telemetry.contacts)
            if (contact.solverEventId == event.eventId &&
                contact.solverIslandId == event.islandId &&
                std::fabs(contact.wholeTickTimeOfImpactSeconds -
                    event.wholeTickTimeOfImpactSeconds) <= profile.solver.toiToleranceSeconds)
                contacts.push_back(&contact);
        if (static_cast<int>(contacts.size()) != event.contactCount) return reject(__LINE__);
        std::vector<std::array<Point3, 2>> corrections(contacts.size());
        int expectedOrdinal = 0;
        std::tuple<int, int, int> previous{-1, -1, -1};
        for (const auto& receipt : event.positionProjections) {
            if (receipt.ordinal != expectedOrdinal++ || receipt.iteration < 0 ||
                receipt.iteration >= event.positionIterations ||
                receipt.contactIndex < 0 || receipt.contactIndex >= event.contactCount ||
                receipt.ballIndex < 0 || receipt.ballIndex >= kBallCount ||
                before.balls[receipt.ballIndex].pocketed ||
                !samePosition(receipt.beforeCm, receipt.beforeCm) ||
                !samePosition(receipt.afterCm, receipt.afterCm) ||
                exactPosition(receipt.beforeCm, receipt.afterCm)) return reject(__LINE__);
            const auto& contact = *contacts[receipt.contactIndex];
            const int participant = receipt.ballIndex == contact.firstBall ? 0 :
                (receipt.ballIndex == contact.secondBall ? 1 : -1);
            const auto key = std::make_tuple(receipt.iteration, receipt.contactIndex, participant);
            if (participant < 0 || key <= previous ||
                !contact.positionCorrected ||
                !(contact.solverProjectionCm > 0.0) ||
                !std::isfinite(contact.solverProjectionCm)) return reject(__LINE__);
            previous = key;
            auto& delta = corrections[receipt.contactIndex][participant];
            delta.x += receipt.afterCm.x - receipt.beforeCm.x;
            delta.y += receipt.afterCm.y - receipt.beforeCm.y;
            delta.z += receipt.afterCm.z - receipt.beforeCm.z;
        }
        for (std::size_t i = 0; i < contacts.size(); ++i)
            if (!exactPosition(corrections[i][0], contacts[i]->firstPositionCorrectionCm) ||
                !exactPosition(corrections[i][1], contacts[i]->secondPositionCorrectionCm))
                return reject(__LINE__);
    }
    const auto applyProjections = [&](int ballIndex, double boundaryTime) {
        for (const auto& event : telemetry.solverEvents) {
            if (std::fabs(event.wholeTickTimeOfImpactSeconds - boundaryTime) >
                    profile.solver.toiToleranceSeconds) continue;
            for (const auto& receipt : event.positionProjections) {
                if (receipt.ballIndex != ballIndex || receipt.trajectoryGeneration != generations[ballIndex] ||
                    std::find(consumedProjections.begin(), consumedProjections.end(),
                        &receipt) != consumedProjections.end()) continue;
                if (!exactPosition(endpoints[ballIndex], receipt.beforeCm)) return reject(__LINE__);
                endpoints[ballIndex] = receipt.afterCm;
                consumedProjections.push_back(&receipt);
            }
        }
        return true;
    };
    std::vector<const PhysicsContactRecord*> consumedResets;
    const auto applyResets = [&](int ballIndex, double time, std::uint64_t targetGeneration) {
        if (targetGeneration < generations[ballIndex]) return reject(__LINE__);
        while (generations[ballIndex] < targetGeneration) {
            const PhysicsContactRecord* reset = nullptr;
            for (const auto& contact : telemetry.contacts) {
                if (!contact.administrativeResetAfterCapture || contact.firstBall != ballIndex ||
                    contact.trajectoryGenerationBefore != generations[ballIndex] ||
                    std::fabs(contact.wholeTickTimeOfImpactSeconds - time) >
                        profile.solver.toiToleranceSeconds) continue;
                if (reset || ballIndex != 0 || contact.kind != PhysicsContactKind::Pocket ||
                    contact.pocketBoundaryEvent != PocketBoundaryEventKind::Capture ||
                    contact.pocketPhaseAfter != PocketInteractionPhase::Captured ||
                    !contact.pocketPassable || contact.pocketId < 0 || contact.pocketCaptureSequence == 0 ||
                    contact.trajectoryGenerationBefore == std::numeric_limits<std::uint64_t>::max() ||
                    contact.trajectoryGenerationAfter != contact.trajectoryGenerationBefore + 1 ||
                    !std::isfinite(contact.wholeTickTimeOfImpactSeconds) ||
                    !samePosition(contact.administrativeResetPositionCm, contact.administrativeResetPositionCm) ||
                    !samePosition(endpoints[ballIndex], contact.firstPositionAtContactCm)) return reject(__LINE__);
                reset = &contact;
            }
            if (!reset) return reject(__LINE__);
            endpoints[ballIndex] = reset->administrativeResetPositionCm;
            generations[ballIndex] = reset->trajectoryGenerationAfter;
            consumedResets.push_back(reset);
            if (!applyProjections(ballIndex, time)) return reject(__LINE__);
        }
        return true;
    };
    int ordinal = 0;
    std::tuple<double, int, int> previousKey{-1.0, -1, -1};
    for (const auto& segment : telemetry.analyticSurfaceMotion) {
        const int i = segment.ballIndex;
        const auto key = std::make_tuple(segment.wholeTickStartOffsetSeconds,
            i, segment.segmentOrdinal);
        if (i < 0 || i >= kBallCount || before.balls[i].pocketed ||
            !segment.analyticPathComplete || segment.segmentOrdinal != ordinal++ ||
            key <= previousKey ||
            !std::isfinite(segment.wholeTickStartOffsetSeconds) ||
            !std::isfinite(segment.durationSeconds) || segment.durationSeconds <= 0.0 ||
            std::fabs(segment.wholeTickStartOffsetSeconds - covered[i]) >
                profile.solver.toiToleranceSeconds ||
            !applyProjections(i, segment.wholeTickStartOffsetSeconds) ||
            !applyResets(i, segment.wholeTickStartOffsetSeconds, segment.trajectoryGeneration) ||
            !samePosition(endpoints[i], segment.startPositionCm)) {
            if (diagnostic && diagnostic->empty()) {
                std::ostringstream detail;
                detail << "composed_segment ball=" << i << " ordinal=" << segment.segmentOrdinal
                       << " time=" << segment.wholeTickStartOffsetSeconds
                       << " duration=" << segment.durationSeconds << " generation=" << segment.trajectoryGeneration;
                if (i >= 0 && i < kBallCount)
                    detail << " covered=" << covered[i] << " position_delta="
                           << segment.startPositionCm.x - endpoints[i].x << ','
                           << segment.startPositionCm.y - endpoints[i].y << ','
                           << segment.startPositionCm.z - endpoints[i].z;
                *diagnostic = detail.str();
            }
            return reject(__LINE__);
        }
        previousKey = key;
        covered[i] = segment.wholeTickStartOffsetSeconds + segment.durationSeconds;
        endpoints[i] = segment.endPositionCm;
        if (covered[i] > telemetry.wholeTickDurationSeconds +
                profile.solver.toiToleranceSeconds) return reject(__LINE__);
    }
    for (int i = 0; i < kBallCount; ++i) {
        if (before.balls[i].pocketed) continue;
        if (after.balls[i].pocketed) {
            const PhysicsContactRecord* capture = nullptr;
            for (const auto& contact : telemetry.contacts) {
                if (contact.firstBall != i || contact.kind != PhysicsContactKind::Pocket ||
                    contact.pocketBoundaryEvent != PocketBoundaryEventKind::Capture)
                    continue;
                if (capture || contact.pocketPhaseAfter != PocketInteractionPhase::Captured ||
                    !contact.pocketPassable || contact.pocketCaptureSequence == 0 ||
                    contact.pocketCaptureSequence != after.balls[i].pocketInteraction.captureSequence ||
                    contact.pocketId != after.balls[i].pocketInteraction.pocketId ||
                    !std::isfinite(contact.wholeTickTimeOfImpactSeconds) ||
                    contact.wholeTickTimeOfImpactSeconds < 0.0 ||
                    contact.wholeTickTimeOfImpactSeconds > telemetry.wholeTickDurationSeconds)
                    return reject(__LINE__);
                capture = &contact;
            }
            // Storage/display relocation after an authoritative capture is not
            // part of the ball's physical trajectory. Bind its terminal segment
            // to the actual capture point and time, never to the display slot.
            if (!capture || i == 0 || after.balls[i].physicalDomain !=
                    PhysicalBallDomain::PocketChannel ||
                after.balls[i].trajectoryGeneration != generations[i] ||
                after.balls[i].pocketInteraction.phase != PocketInteractionPhase::Captured ||
                std::fabs(covered[i] - capture->wholeTickTimeOfImpactSeconds) >
                    profile.solver.toiToleranceSeconds ||
                !samePosition(endpoints[i], capture->firstPositionAtContactCm))
                return reject(__LINE__);
            continue;
        }
        if (!applyProjections(i, covered[i]) ||
            !applyResets(i, covered[i], after.balls[i].trajectoryGeneration) ||
            !samePosition(endpoints[i], after.balls[i].position)) {
            return reject(__LINE__);
        }
        if (!after.balls[i].pocketed &&
            std::fabs(covered[i] - telemetry.wholeTickDurationSeconds) >
                profile.solver.toiToleranceSeconds) return reject(__LINE__);
    }
    std::size_t projectionCount = 0;
    for (const auto& event : telemetry.solverEvents)
        projectionCount += event.positionProjections.size();
    if (consumedProjections.size() != projectionCount) return reject(__LINE__);
    std::size_t resetCount = 0;
    for (const auto& contact : telemetry.contacts)
        if (contact.administrativeResetAfterCapture) ++resetCount;
    if (consumedResets.size() != resetCount) return reject(__LINE__);
    return true;
}

void mergeComposedState(GameState& staged, const GameState& clothState,
    const GameState& departureState,
    const std::array<bool, kBallCount>& departureBalls)
{
    staged = clothState;
    for (int index = 0; index < kBallCount; ++index) {
        if (departureBalls[index])
            staged.balls[index] = departureState.balls[index];
    }
    staged.nextPocketCaptureSequence = std::max(
        clothState.nextPocketCaptureSequence,
        departureState.nextPocketCaptureSequence);
    staged.pocketedBallCount = 0;
    for (const BallState& ball : staged.balls)
        if (ball.pocketed) ++staged.pocketedBallCount;
    staged.ballsMoving = anyBallMoving(staged);
}

bool validateComposedFinalState(const GameState& snapshot,
    const GameState& staged, const PhysicsProfile& profile,
    PhysicsStepTelemetry& telemetry)
{
    const double before = totalMechanicalEnergyIncludingGravityJ(
        snapshot, profile);
    const double after = totalMechanicalEnergyIncludingGravityJ(
        staged, profile);
    if (!finiteGameState(staged) || !std::isfinite(before) ||
        !std::isfinite(after)) {
        failStep(telemetry, std::isfinite(after)
            ? PhysicsFailureCode::NonfiniteState
            : PhysicsFailureCode::NonfiniteEnergy);
        return false;
    }
    double representationBound = 0.0;
    for (const BallState& ball : staged.balls) {
        if (ball.pocketed) continue;
        representationBound = std::max(representationBound,
            2.0 * static_cast<double>(profile.ball.massKg) *
            std::fabs(static_cast<double>(
                profile.tableDeparture.gravityYCmS2)) *
            static_cast<double>(std::numeric_limits<float>::epsilon()) *
            std::fabs(static_cast<double>(ball.position.y)) / 10000.0);
    }
    if (after > before + profile.solver.passiveEnergyToleranceJ +
            representationBound) {
        failStep(telemetry, PhysicsFailureCode::PassiveEnergyCreation);
        return false;
    }
    return true;
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
        if (candidate.kind == ContinuousContactKind::Capture && candidate.firstBall == 0 &&
            ball.trajectoryGeneration == std::numeric_limits<std::uint64_t>::max())
            return false;
        if (ball.pocketInteraction.phase ==
            PocketInteractionPhase::Captured) continue;
        PocketBoundaryQuery query = classifyPocketPoint(
            *frame, ball.position, profile.ball.radiusCm);
        if (ball.physicalDomain == PhysicalBallDomain::PocketChannel) {
            // The accepted open shaft has a vertical throat at the cloth
            // plane. Capture occurs only after the entire sphere clears it.
            const bool capture = candidate.kind == ContinuousContactKind::Capture;
            const double plane = profile.tableDeparture.clothSurfaceYCm -
                (capture ? profile.ball.radiusCm : 0.0f);
            if (ball.velocity.y > 0.0f || ball.position.y > plane +
                    profile.solver.penetrationSlopCm) return false;
            query.region = capture ? PocketBoundaryRegion::Capture : PocketBoundaryRegion::Throat;
            query.passable = true;
            query.throatSignedDistanceCm = profile.tableDeparture.clothSurfaceYCm - ball.position.y;
            query.captureSignedDistanceCm = profile.tableDeparture.clothSurfaceYCm -
                profile.ball.radiusCm - ball.position.y;
        }
        if (profile.formulaVersion == "legacy_v1") {
            if (candidate.pocketEvent == PocketBoundaryEventKind::Throat) {
                query.region = PocketBoundaryRegion::Throat;
                query.passable = true;
            } else if (candidate.pocketEvent ==
                    PocketBoundaryEventKind::Capture) {
                query.region = PocketBoundaryRegion::Capture;
                query.passable = true;
            }
        }
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
            telemetry.contacts.back().pocketVerticalShaft =
                ball.physicalDomain == PhysicalBallDomain::PocketChannel;
            telemetry.contacts.back().trajectoryGenerationBefore = ball.trajectoryGeneration;
            telemetry.contacts.back().trajectoryGenerationAfter = ball.trajectoryGeneration;
        }
        if (transition.captureEmitted) {
            const auto generationBefore = ball.trajectoryGeneration;
            ++state.nextPocketCaptureSequence;
            if (!updatePocketedBall(state, candidate.firstBall)) return false;
            auto& capture = telemetry.contacts.back();
            capture.trajectoryGenerationBefore = generationBefore;
            capture.trajectoryGenerationAfter = ball.trajectoryGeneration;
            if (candidate.firstBall == 0) {
                if (profile.formulaVersion != "legacy_v1")
                    ball.position.y = profile.tableDeparture.clothSurfaceYCm + profile.ball.radiusCm;
                capture.administrativeResetAfterCapture = true;
                capture.administrativeResetPositionCm = ball.position;
            }
        }
    }
    return true;
}

std::vector<ContinuousContactCandidate> queryLegacyContactCandidates(
    const GameState& state, double maximumTime, const PhysicsProfile& profile,
    PhysicsBoundaryMode boundaryMode)
{
    auto pairState = state;
    for (auto& ball : pairState.balls)
        if (ball.physicalDomain == PhysicalBallDomain::PocketChannel) ball.pocketed = true;
    auto candidates = generateBallBallCandidates(pairState, maximumTime,
        profile.ball.radiusCm, profile.solver.toiToleranceSeconds, false,
        profile.solver.residualToleranceCmS);
    if (boundaryMode == PhysicsBoundaryMode::ProductionTable) {
        const auto boundary = generateBoundaryCandidates(
            state, static_cast<float>(maximumTime), profile);
        candidates.insert(candidates.end(), boundary.begin(), boundary.end());
    }
    return candidates;
}

SurfaceMotionStep advanceOwnedFreeMotion(BallState& ball, double duration,
    const PhysicsProfile& profile)
{
    if (ball.physicalDomain != PhysicalBallDomain::PocketChannel)
        return advanceSurfaceMotion(ball, static_cast<float>(duration), profile.ball, profile.surface);
    SurfaceMotionStep motion;
    motion.before = motion.after = BallMotionState::Sliding;
    if (!(duration > 0.0)) return motion;
    SurfaceMotionAnalyticSegment segment;
    segment.durationSeconds = duration;
    segment.motionState = BallMotionState::Sliding;
    segment.startPositionCm = ball.position;
    segment.startVelocityCmS = ball.velocity;
    segment.constantAccelerationCmS2 = {0.0f, profile.tableDeparture.gravityYCmS2, 0.0f};
    ball.position.x = static_cast<float>(double(ball.position.x) + double(ball.velocity.x) * duration);
    ball.position.y = static_cast<float>(double(ball.position.y) + double(ball.velocity.y) * duration +
        0.5 * double(profile.tableDeparture.gravityYCmS2) * duration * duration);
    ball.position.z = static_cast<float>(double(ball.position.z) + double(ball.velocity.z) * duration);
    ball.velocity.y = static_cast<float>(double(ball.velocity.y) +
        double(profile.tableDeparture.gravityYCmS2) * duration);
    ball.speed = std::sqrt(ball.velocity.x * ball.velocity.x + ball.velocity.y * ball.velocity.y +
        ball.velocity.z * ball.velocity.z);
    segment.endPositionCm = ball.position;
    segment.analyticPathComplete = true;
    motion.analyticSegments.push_back(segment);
    return motion;
}

PhysicsStepTelemetry advanceLegacyFreeMotion(GameState& state, float duration,
    const PhysicsProfile& profile, double wholeTickOffsetSeconds)
{
    PhysicsStepTelemetry telemetry;
    telemetry.wholeTickDurationSeconds = duration;
    for (int i = 0; i < kBallCount; ++i) {
        if (state.balls[i].pocketed) continue;
        auto motion = advanceOwnedFreeMotion(state.balls[i], duration, profile);
        appendAnalyticSegments(telemetry, motion, i, wholeTickOffsetSeconds,
            state.balls[i].trajectoryGeneration);
        telemetry.surfaceMotion.push_back(motion);
    }
    state.ballsMoving = anyBallMoving(state);
    return telemetry;
}

PhysicsStepTelemetry updatePhysicsEventDriven(
    GameState& state, float timeStep, const PhysicsProfile& profile,
    int remainingEvents, PhysicsBoundaryMode boundaryMode,
    double wholeTickOffsetSeconds, bool stopAfterFirstBatch = false,
    const std::vector<ContinuousContactCandidate>* instantaneousCandidates = nullptr)
{
    if (!std::isfinite(timeStep)) {
        PhysicsStepTelemetry failed;
        failStep(failed, PhysicsFailureCode::NonfiniteState);
        return failed;
    }
    // A zero-duration handoff still owns a real contact batch. Routing it
    // through the discrete fallback would project positions without the
    // event/island mutation receipts needed by the composed trajectory.
    if (timeStep < 0.0f) {
        PhysicsStepTelemetry failed;
        failStep(failed, PhysicsFailureCode::InvalidControls);
        return failed;
    }
    if (remainingEvents <= 0) {
        PhysicsStepTelemetry failed;
        failStep(failed, PhysicsFailureCode::EventBudget,
            profile.solver.maximumEventsPerTick);
        return failed;
    }
    const auto candidates = instantaneousCandidates ? *instantaneousCandidates :
        queryLegacyContactCandidates(state, timeStep, profile, boundaryMode);
    if (candidates.empty()) {
        if (stopAfterFirstBatch)
            return advanceLegacyFreeMotion(state, timeStep, profile,
                wholeTickOffsetSeconds);
        return updatePhysicsDiscrete(
            state, timeStep, profile, boundaryMode, false,
            wholeTickOffsetSeconds);
    }
    const ContinuousEventBatch initialBatch = buildEarliestEventBatch(candidates,
        profile.solver.toiToleranceSeconds, profile.solver.maximumIslandSize);
    const double toi = initialBatch.earliestTimeSeconds;

    PhysicsStepTelemetry prefix;
    prefix.wholeTickDurationSeconds = toi;
    for (int index = 0; index < kBallCount; ++index) {
        if (state.balls[index].pocketed) continue;
        SurfaceMotionStep motion = advanceOwnedFreeMotion(state.balls[index], toi, profile);
        appendAnalyticSegments(prefix, motion, index,
            wholeTickOffsetSeconds, state.balls[index].trajectoryGeneration);
        prefix.surfaceMotion.push_back(motion);
    }
    auto pairState = state;
    for (auto& ball : pairState.balls)
        if (ball.physicalDomain == PhysicalBallDomain::PocketChannel) ball.pocketed = true;
    std::vector<ContinuousContactCandidate> activeCandidates =
        generateBallBallCandidates(pairState, 0.0, profile.ball.radiusCm,
            profile.solver.toiToleranceSeconds, true,
            profile.solver.residualToleranceCmS);
    if (boundaryMode == PhysicsBoundaryMode::ProductionTable && !instantaneousCandidates) {
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
        std::array<Point3, kBallCount> positionsAtContact{};
        for (int ballIndex = 0; ballIndex < kBallCount; ++ballIndex)
            positionsAtContact[ballIndex] = state.balls[ballIndex].position;
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
        solverEvent.wholeTickTimeOfImpactSeconds = wholeTickOffsetSeconds + toi;
        solverEvent.positionProjections = solved.positionProjections;
        prefix.solverEvents.push_back(solverEvent);
        hardFailure = hardFailure || solved.status != ContactSolverStatus::Converged;
        if (solved.status != ContactSolverStatus::Converged) {
            failStep(prefix, solverFailureCode(solved.status),
                solverEvent.eventId, island.islandId);
            std::ostringstream detail;
            detail << "solver:" << contactSolverStatusName(solved.status);
            for (const auto& contact : island.contacts)
                detail << " contact=" << int(contact.kind) << ':'
                       << int(contact.boundaryGeometry) << ':' << contact.valid << ':'
                       << contact.firstBall << ':' << contact.secondBall << " normal="
                       << contact.normal.x << ',' << contact.normal.y << ',' << contact.normal.z;
            prefix.failureDetail = detail.str();
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
            const bool support = candidate.kind == ContinuousContactKind::SupportPlane;
            contact.kind = ballBall ? PhysicsContactKind::BallBall :
                (support ? PhysicsContactKind::Support : PhysicsContactKind::Rail);
            contact.firstBall = candidate.firstBall;
            contact.secondBall = candidate.secondBall;
            contact.boundaryFeatureId = candidate.featureId;
            contact.normal = solved.contacts[index].normal;
            contact.penetrationCm = candidate.penetrationCm;
            contact.timeOfImpactSeconds = wholeTickOffsetSeconds + toi;
            contact.wholeTickTimeOfImpactSeconds =
                wholeTickOffsetSeconds + toi;
            contact.firstPositionAtContactCm =
                positionsAtContact[candidate.firstBall];
            if (ballBall)
                contact.secondPositionAtContactCm =
                    positionsAtContact[candidate.secondBall];
            contact.normalImpulseNs = solved.contacts[index].accumulatedNormalImpulseNs;
            contact.tangentialImpulseNs = std::fabs(
                solved.contacts[index].accumulatedTangentialImpulseNs);
            contact.solverResidualCmS = solved.contacts[index].residualCmS;
            contact.solverProjectionCm = solved.contacts[index].projectionCm;
            for (const auto& receipt : solved.positionProjections) {
                if (receipt.contactIndex != static_cast<int>(index)) continue;
                contact.positionCorrected = true;
                Point3& correction = receipt.ballIndex == candidate.firstBall ?
                    contact.firstPositionCorrectionCm : contact.secondPositionCorrectionCm;
                correction.x += receipt.afterCm.x - receipt.beforeCm.x;
                correction.y += receipt.afterCm.y - receipt.beforeCm.y;
                correction.z += receipt.afterCm.z - receipt.beforeCm.z;
            }
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
            if (!ballBall && !support) {
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
                contact.positionCorrectionCm = contact.firstPositionCorrectionCm;
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
                (!ballBall && !support && contact.velocityImpulseApplied);

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
            pocketFrames, prefix, wholeTickOffsetSeconds + toi)) {
        failStep(prefix, PhysicsFailureCode::ContradictoryTopology,
            profile.solver.maximumEventsPerTick - remainingEvents);
        state.ballsMoving = anyBallMoving(state);
        return prefix;
    }
    const float remaining = std::max(0.0f, timeStep - static_cast<float>(toi));
    if (remaining <= 0.0f || stopAfterFirstBatch) {
        if (ballImpulseApplied) state.events.ballCollision = true;
        if (railImpulseApplied) state.events.railCollision = true;
        state.ballsMoving = anyBallMoving(state);
        return prefix;
    }
    const GameplayEvents prefixEvents = state.events;
    PhysicsStepTelemetry tail = updatePhysicsEventDriven(
        state, remaining, profile, remainingEvents - 1, boundaryMode,
        wholeTickOffsetSeconds + toi);
    prependTelemetry(tail, prefix);
    mergeGameplayEvents(state.events, prefixEvents);
    if (ballImpulseApplied) state.events.ballCollision = true;
    if (railImpulseApplied) state.events.railCollision = true;
    if (ballImpulseApplied || railImpulseApplied)
        state.ballsMoving = anyBallMoving(state);
    return tail;
}

PhysicsStepTelemetry runComposedEventScheduler(GameState& staged, float timeStep,
    const PhysicsProfile& profile, PhysicsBoundaryMode boundaryMode)
{
    PhysicsStepTelemetry telemetry;
    const bool wasMoving = staged.ballsMoving;
    clearGameplayEvents(staged);
    const auto fail = [&](PhysicsFailureCode code, const std::string& detail = "") {
        failStep(telemetry, code);
        telemetry.failureDetail = detail;
        clearCanonicalTelemetry(telemetry);
        return telemetry;
    };
    const auto departureFailure = [](TableDepartureFailureCode code) {
        if (code == TableDepartureFailureCode::EventBudget)
            return PhysicsFailureCode::EventBudget;
        if (code == TableDepartureFailureCode::ContradictoryPocketDeparture ||
            code == TableDepartureFailureCode::AmbiguousCrossDomainTableContact)
            return PhysicsFailureCode::ContradictoryTopology;
        return PhysicsFailureCode::InvalidControls;
    };
    if (!std::isfinite(timeStep) || timeStep < 0.0f || !finiteGameState(staged))
        return fail(PhysicsFailureCode::InvalidControls);
    double elapsed = 0.0;
    int consumedEvents = 0;
    int privateOrdinal = 0;
    std::vector<std::string> recentEventBatches;
    bool firstIteration = true;
    while (firstIteration || elapsed < timeStep) {
        firstIteration = false;
        const double remaining = double(timeStep) - elapsed;
        std::array<bool, kBallCount> departureOwned{};
        GameState legacy = staged;
        GameState departure = staged;
        for (int i = 0; i < kBallCount; ++i) {
            auto& ball = staged.balls[i];
            if (!ball.pocketed && ball.physicalDomain == PhysicalBallDomain::PocketChannel) {
                const auto phase = ball.pocketInteraction.phase;
                if (ball.pocketInteraction.pocketId < 0 || ball.pocketInteraction.pocketId >= 6 ||
                    (phase != PocketInteractionPhase::Approaching &&
                     phase != PocketInteractionPhase::JawContact &&
                     phase != PocketInteractionPhase::ThroatCrossed) || ball.velocity.y > 0.0f ||
                    ball.position.y > profile.tableDeparture.clothSurfaceYCm +
                        profile.ball.radiusCm + profile.solver.penetrationSlopCm)
                    return fail(PhysicsFailureCode::InvalidControls, "invalid_vertical_pocket_channel");
            }
            if (!ball.pocketed && ball.physicalDomain != PhysicalBallDomain::Airborne &&
                ball.physicalDomain != PhysicalBallDomain::PocketChannel) {
                if (ball.velocity.y > 0.0f) ball.physicalDomain = PhysicalBallDomain::Airborne;
            }
            departureOwned[i] = !ball.pocketed &&
                ball.physicalDomain != PhysicalBallDomain::ClothSupported &&
                ball.physicalDomain != PhysicalBallDomain::PocketChannel;
            legacy.balls[i] = ball;
            departure.balls[i] = ball;
            if (departureOwned[i]) legacy.balls[i].pocketed = true;
            else departure.balls[i].pocketed = true;
        }
        // A legacy island may leave a small separating overlap after its
        // bounded projection iterations. Before handing either participant
        // to a swept domain query, resolve that overlap through an actual
        // zero-time island. Never silently accept an intersecting trajectory
        // or synthesize a projection receipt in the observer.
        std::vector<ContinuousContactCandidate> initialMixedContacts;
        const double minimumMixedSeparationSquared =
            tableDepartureMinimumSeparationSquared(2.0 * profile.ball.radiusCm,
                profile.solver.penetrationSlopCm);
        for (int first = 0; first < kBallCount; ++first) {
            const auto& a = staged.balls[first];
            if (a.pocketed || a.physicalDomain == PhysicalBallDomain::PocketChannel) continue;
            for (int second = first + 1; second < kBallCount; ++second) {
                const auto& b = staged.balls[second];
                if (b.pocketed || b.physicalDomain == PhysicalBallDomain::PocketChannel ||
                    (!departureOwned[first] && !departureOwned[second])) continue;
                const Point3 difference{b.position.x - a.position.x,
                    b.position.y - a.position.y, b.position.z - a.position.z};
                const double separationSquared = difference.x * difference.x +
                    difference.y * difference.y + difference.z * difference.z;
                if (!(separationSquared < minimumMixedSeparationSquared)) continue;
                const auto contact = sweptBallBallCandidate(a, first, b, second,
                    0.0, 2.0 * profile.ball.radiusCm, profile.solver.toiToleranceSeconds,
                    true, profile.solver.residualToleranceCmS);
                if (contact.valid) initialMixedContacts.push_back(contact);
            }
        }
        if (!initialMixedContacts.empty()) {
            if (consumedEvents >= profile.solver.maximumEventsPerTick - 8) {
                std::ostringstream mixedBatch;
                mixedBatch.precision(17);
                mixedBatch << "elapsed=" << elapsed << ",contacts=";
                bool wroteContact = false;
                for (const auto& contact : initialMixedContacts) {
                    const auto& first = staged.balls[contact.firstBall];
                    const auto& second = staged.balls[contact.secondBall];
                    const double normalSpeed =
                        (second.velocity.x - first.velocity.x) * contact.normal.x +
                        (second.velocity.y - first.velocity.y) * contact.normal.y +
                        (second.velocity.z - first.velocity.z) * contact.normal.z;
                    mixedBatch << (wroteContact ? ";" : "") << contact.firstBall << ':'
                               << contact.secondBall << "/p" << contact.penetrationCm
                               << "/vn" << normalSpeed << "/d"
                               << int(first.physicalDomain) << ':'
                               << int(second.physicalDomain) << "/vy"
                               << first.velocity.y << ':' << second.velocity.y;
                    wroteContact = true;
                }
                recentEventBatches.push_back("mixed:" + mixedBatch.str());
                if (recentEventBatches.size() > 8) recentEventBatches.erase(
                    recentEventBatches.begin());
            }
            if (consumedEvents >= profile.solver.maximumEventsPerTick) {
                std::ostringstream detail;
                detail << "mixed_overlap_projection_budget:consumed=" << consumedEvents;
                for (std::size_t index = 0; index < recentEventBatches.size(); ++index)
                    detail << ":batch" << index << '=' << recentEventBatches[index];
                return fail(PhysicsFailureCode::EventBudget, detail.str());
            }
            const auto geometry = buildTableDepartureGeometry(profile);
            std::array<bool, kBallCount> involved{};
            for (const auto& contact : initialMixedContacts)
                involved[contact.firstBall] = involved[contact.secondBall] = true;
            const auto touching = generateBallBallCandidates(staged, 0.0,
                profile.ball.radiusCm, profile.solver.toiToleranceSeconds,
                true, profile.solver.residualToleranceCmS);
            for (int pass = 0; pass < kBallCount; ++pass) {
                bool changed = false;
                for (const auto& contact : touching) {
                    if (involved[contact.firstBall] == involved[contact.secondBall]) continue;
                    involved[contact.firstBall] = involved[contact.secondBall] = true;
                    changed = true;
                }
                if (!changed) break;
            }
            for (int ball = 0; ball < kBallCount; ++ball) {
                const auto& sample = staged.balls[ball];
                if (sample.pocketed || !involved[ball]) continue;
                double plane = 0.0;
                if (sample.physicalDomain == PhysicalBallDomain::ClothSupported)
                    plane = geometry.clothSupportedCenterYCm;
                else if (sample.physicalDomain == PhysicalBallDomain::RailTopSupported)
                    plane = geometry.railTopSupportedCenterYCm;
                else if (sample.physicalDomain == PhysicalBallDomain::OutsideLandingSupported)
                    plane = geometry.outsideLandingSupportedCenterYCm;
                else continue;
                initialMixedContacts.push_back(supportPlaneContactCandidate(ball, plane));
            }
            auto corrected = updatePhysicsEventDriven(staged, 0.0f, profile,
                profile.solver.maximumEventsPerTick - consumedEvents,
                boundaryMode, elapsed, true, &initialMixedContacts);
            if (corrected.stepStatus == PhysicsStepStatus::Failed)
                return fail(corrected.failureCode, "mixed_overlap_projection");
            prependTelemetry(corrected, telemetry);
            telemetry = std::move(corrected);
            ++consumedEvents;
            continue;
        }
        auto legacyCandidates = queryLegacyContactCandidates(legacy,
            remaining, profile, boundaryMode);
        // A downward supported velocity is an actual unilateral contact,
        // not a state-normalization operation. Resolve it at t=0 through
        // the same island solver and publish the resulting impulse.
        const auto supportGeometry = buildTableDepartureGeometry(profile);
        for (int i = 0; i < kBallCount; ++i) {
            const auto& ball = staged.balls[i];
            if (ball.pocketed || !(ball.velocity.y < 0.0f)) continue;
            double plane = 0.0;
            if (ball.physicalDomain == PhysicalBallDomain::ClothSupported)
                plane = supportGeometry.clothSupportedCenterYCm;
            else if (ball.physicalDomain == PhysicalBallDomain::RailTopSupported)
                plane = supportGeometry.railTopSupportedCenterYCm;
            else if (ball.physicalDomain == PhysicalBallDomain::OutsideLandingSupported)
                plane = supportGeometry.outsideLandingSupportedCenterYCm;
            else continue;
            legacyCandidates.push_back(supportPlaneContactCandidate(i, plane));
        }
        const bool hasDepartureOwner = std::any_of(departureOwned.begin(),
            departureOwned.end(), [](bool owned) { return owned; });
        const auto legacyBatch = buildEarliestEventBatch(legacyCandidates,
            profile.solver.toiToleranceSeconds, profile.solver.maximumIslandSize);
        if (legacyBatch.failureCode != ContinuousBatchFailureCode::None)
            return fail(batchFailureCode(legacyBatch.failureCode));
        double horizon = legacyCandidates.empty() ? remaining :
            std::min(remaining, legacyBatch.earliestTimeSeconds);
        // This first query contains only departure-owned balls. Its horizon
        // cannot be corrupted by predicting legacy cloth with a different law.
        TableDepartureEarliestBatch departureLimit;
        departureLimit.ok = true;
        departureLimit.failureCode = TableDepartureFailureCode::None;
        if (hasDepartureOwner) departureLimit = queryEarliestTableDepartureBatch(
            departure, horizon, profile, boundaryMode);
        if (!departureLimit.ok) {
            std::ostringstream detail;
            detail.precision(17);
            detail << "departure_owned_query:" << int(departureLimit.failureCode)
                   << " pair=" << departureLimit.failingFirstBall << ':'
                   << departureLimit.failingSecondBall << " penetration="
                   << departureLimit.initialPenetrationCm << " elapsed=" << elapsed;
            return fail(departureFailure(departureLimit.failureCode), detail.str());
        }
        if (!departureLimit.candidates.empty())
            horizon = std::min(horizon, departureLimit.earliestTimeSeconds);
        auto predictedDeparture = departure;
        TableDepartureStepResult departurePrediction;
        departurePrediction.ok = true;
        departurePrediction.failureCode = TableDepartureFailureCode::None;
        if (hasDepartureOwner) departurePrediction = advanceTableDepartureDomains(
            predictedDeparture, horizon, profile, boundaryMode, departureOwned);
        if (!departurePrediction.ok) return fail(PhysicsFailureCode::InvalidControls,
            "departure_prediction:" + std::to_string(int(departurePrediction.failureCode)));
        auto predictedLegacy = legacy;
        const auto legacyPrediction = advanceLegacyFreeMotion(predictedLegacy,
            static_cast<float>(horizon), profile, 0.0);
        auto prediction = legacyPrediction.analyticSurfaceMotion;
        prediction.insert(prediction.end(), departurePrediction.analyticSegments.begin(),
            departurePrediction.analyticSegments.end());
        TableDepartureEarliestBatch departureBatch;
        departureBatch.ok = true;
        departureBatch.failureCode = TableDepartureFailureCode::None;
        if (hasDepartureOwner) departureBatch = queryEarliestTableDepartureBatch(staged,
            horizon, profile, boundaryMode, prediction, departureOwned);
        if (!departureBatch.ok) {
            std::ostringstream detail;
            detail.precision(17);
            detail << "departure_injected_query:" << int(departureBatch.failureCode)
                   << " pair=" << departureBatch.failingFirstBall << ':'
                   << departureBatch.failingSecondBall << " penetration="
                   << departureBatch.initialPenetrationCm << " elapsed=" << elapsed;
            for (int ball : {departureBatch.failingFirstBall, departureBatch.failingSecondBall}) {
                if (ball < 0 || ball >= kBallCount) continue;
                const auto& sample = staged.balls[ball];
                detail << " ball=" << ball << " domain=" << int(sample.physicalDomain)
                       << " position=" << sample.position.x << ',' << sample.position.y << ',' << sample.position.z
                       << " velocity=" << sample.velocity.x << ',' << sample.velocity.y << ',' << sample.velocity.z;
            }
            return fail(departureFailure(departureBatch.failureCode),
                detail.str());
        }
        const double duration = departureBatch.candidates.empty() ? horizon :
            std::min(horizon, departureBatch.earliestTimeSeconds);
        const bool resolveLegacy = !legacyCandidates.empty() &&
            std::fabs(legacyBatch.earliestTimeSeconds - duration) <= profile.solver.toiToleranceSeconds;
        const bool resolveDeparture = !departureBatch.candidates.empty() &&
            std::fabs(departureBatch.earliestTimeSeconds - duration) <= profile.solver.toiToleranceSeconds;
        const bool resolveCrossIsland = resolveDeparture &&
            std::any_of(departureBatch.candidates.begin(), departureBatch.candidates.end(),
                [](const auto& candidate) {
                    return candidate.kind == TableDepartureEventKind::CrossDomainBallContact;
                });
        const auto isLanding = [](TableDepartureEventKind kind) {
            return kind == TableDepartureEventKind::RailTopLanding ||
                kind == TableDepartureEventKind::OutsideLanding ||
                kind == TableDepartureEventKind::ClothReturn;
        };
        const bool resolveLanding = resolveDeparture &&
            std::any_of(departureBatch.candidates.begin(), departureBatch.candidates.end(),
                [&](const auto& candidate) { return isLanding(candidate.kind); });
        const bool resolveNose = resolveDeparture &&
            std::any_of(departureBatch.candidates.begin(), departureBatch.candidates.end(),
                [](const auto& candidate) {
                    return candidate.kind == TableDepartureEventKind::CushionNoseContact;
                });
        const bool resolveSharedIsland = resolveCrossIsland || resolveLanding || resolveNose ||
            (resolveLegacy && std::any_of(legacyBatch.physicalIslands.begin(),
                legacyBatch.physicalIslands.end(), [](const auto& island) {
                    return std::any_of(island.contacts.begin(), island.contacts.end(),
                        [](const auto& candidate) {
                            return candidate.kind == ContinuousContactKind::SupportPlane ||
                                candidate.kind == ContinuousContactKind::BallBall;
                        });
                }));
        if ((resolveLegacy || resolveDeparture) &&
            consumedEvents >= profile.solver.maximumEventsPerTick - 8) {
            std::ostringstream batch;
            batch.precision(17);
            batch << "elapsed=" << elapsed << ",duration=" << duration
                  << ",legacy=";
            bool wrote = false;
            for (const auto& candidate : legacyCandidates) {
                if (std::fabs(candidate.timeOfImpactSeconds - duration) >
                        profile.solver.toiToleranceSeconds) continue;
                const auto key = continuousContactStableKey(candidate);
                const auto& first = staged.balls[candidate.firstBall];
                double normalSpeed = 0.0;
                if (candidate.secondBall >= 0) {
                    const auto& second = staged.balls[candidate.secondBall];
                    normalSpeed = (second.velocity.x - first.velocity.x) * candidate.normal.x +
                        (second.velocity.y - first.velocity.y) * candidate.normal.y +
                        (second.velocity.z - first.velocity.z) * candidate.normal.z;
                }
                batch << (wrote ? ";" : "") << std::get<0>(key) << ':'
                      << std::get<1>(key) << ':' << std::get<2>(key) << ':'
                      << std::get<3>(key) << "@" << candidate.timeOfImpactSeconds
                      << "/p" << candidate.penetrationCm << "/vn" << normalSpeed;
                wrote = true;
            }
            batch << ",departure=";
            wrote = false;
            if (resolveDeparture) {
                for (const auto& candidate : departureBatch.candidates) {
                    if (std::fabs(candidate.timeSeconds - duration) >
                            profile.solver.toiToleranceSeconds) continue;
                    batch << (wrote ? ";" : "") << int(candidate.kind) << ':'
                          << candidate.firstBall << ':' << candidate.secondBall << ':'
                          << candidate.featureId << '@' << candidate.timeSeconds;
                    wrote = true;
                }
            }
            recentEventBatches.push_back(batch.str());
            if (recentEventBatches.size() > 8) recentEventBatches.erase(
                recentEventBatches.begin());
        }
        if ((resolveLegacy || resolveDeparture) &&
            consumedEvents >= profile.solver.maximumEventsPerTick) {
            std::ostringstream detail;
            detail << "composed_event_budget:consumed=" << consumedEvents;
            for (std::size_t index = 0; index < recentEventBatches.size(); ++index)
                detail << ":batch" << index << '=' << recentEventBatches[index];
            return fail(PhysicsFailureCode::EventBudget, detail.str());
        }
        const GameState departureBefore = departure;
        TableDepartureStepResult departureMotion;
        departureMotion.ok = true;
        departureMotion.failureCode = TableDepartureFailureCode::None;
        if (hasDepartureOwner) departureMotion = advanceTableDepartureDomains(departure,
            duration, profile, boundaryMode, departureOwned);
        if (!departureMotion.ok || !verifyPrivateAnalyticLineage(departureMotion,
                departureBefore, departure, duration, profile))
            return fail(PhysicsFailureCode::InvalidControls, "departure_advance_or_lineage");
        auto legacyMotion = resolveLegacy && !resolveSharedIsland ? updatePhysicsEventDriven(legacy,
            static_cast<float>(duration), profile,
            profile.solver.maximumEventsPerTick - consumedEvents,
            boundaryMode, elapsed, true, &legacyCandidates) : advanceLegacyFreeMotion(legacy,
                static_cast<float>(duration), profile, elapsed);
        if (legacyMotion.stepStatus == PhysicsStepStatus::Failed)
            return fail(legacyMotion.failureCode,
                "composed_legacy_motion:" + legacyMotion.failureDetail);
        if (resolveLegacy && !resolveSharedIsland) ++consumedEvents;
        prependTelemetry(legacyMotion, telemetry);
        telemetry = std::move(legacyMotion);
        appendRebasedPrivateAnalyticSegments(telemetry, departureMotion, elapsed);
        mergeComposedState(staged, legacy, departure, departureOwned);
        if (resolveSharedIsland) {
            std::vector<TableDepartureEvent> pendingTableEvents;
            auto tableBatch = departureBatch;
            // A legacy island may occur strictly before the next departure
            // event. Its solve must not commit that future landing early.
            if (!resolveDeparture) tableBatch.candidates.clear();
            tableBatch.candidates.erase(std::remove_if(tableBatch.candidates.begin(),
                tableBatch.candidates.end(), [](const auto& candidate) {
                    return candidate.kind == TableDepartureEventKind::CrossDomainBallContact;
                }), tableBatch.candidates.end());
            if (!tableBatch.candidates.empty()) {
                if (consumedEvents + static_cast<int>(tableBatch.candidates.size()) >=
                        profile.solver.maximumEventsPerTick)
                    return fail(PhysicsFailureCode::EventBudget);
                // Resolve domain/feature intent on a copy. Incoming velocity
                // and position remain untouched until the coupled physical
                // island solves support and ball impulses together.
                auto intended = staged;
                const auto landing = resolveTableDepartureBatch(intended, tableBatch,
                    profile, elapsed + duration, privateOrdinal);
                if (!landing.ok) return fail(PhysicsFailureCode::ContradictoryTopology,
                    "simultaneous_landing:" + std::to_string(int(landing.failureCode)));
                privateOrdinal += static_cast<int>(landing.events.size());
                pendingTableEvents = landing.events;
                for (const auto& event : pendingTableEvents) {
                    if (isLanding(event.kind))
                        staged.balls[event.ballIndex].physicalDomain =
                            intended.balls[event.ballIndex].physicalDomain;
                    else if (event.kind != TableDepartureEventKind::CushionNoseContact)
                        staged.balls[event.ballIndex] = intended.balls[event.ballIndex];
                }
                consumedEvents += static_cast<int>(landing.events.size());
            }
            std::vector<ContinuousContactCandidate> instantaneous;
            for (auto candidate : legacyCandidates) {
                if (std::fabs(candidate.timeOfImpactSeconds - duration) >
                        profile.solver.toiToleranceSeconds) continue;
                candidate.timeOfImpactSeconds = 0.0;
                instantaneous.push_back(candidate);
            }
            for (const auto& cross : departureBatch.candidates) {
                if (!resolveDeparture) continue;
                if (cross.kind == TableDepartureEventKind::CushionNoseContact) {
                    const auto& ball = staged.balls[cross.firstBall];
                    const bool xAxis = cross.featureId == 0;
                    if (!xAxis && cross.featureId != 1)
                        return fail(PhysicsFailureCode::InvalidControls);
                    const bool positive = (xAxis ? ball.position.x : ball.position.z) > 0.0f;
                    const float inward = positive ? -1.0f : 1.0f;
                    const Point3 normal = cross.contactNormalComplete ? cross.contactNormal :
                        (xAxis ? Point3{inward, 0.0f, 0.0f} : Point3{0.0f, 0.0f, inward});
                    const double normalLengthSquared = double(normal.x) * normal.x +
                        double(normal.y) * normal.y + double(normal.z) * normal.z;
                    if (!std::isfinite(normalLengthSquared) ||
                        std::fabs(normalLengthSquared - 1.0) > 2e-4)
                        return fail(PhysicsFailureCode::InvalidControls, "departure_contact_normal");
                    const int feature = (xAxis ? 0 : 2) + (positive ? 1 : 0);
                    instantaneous.push_back(cross.contactNormalComplete ?
                        roundedUpperRailEdgeContactCandidate(cross.firstBall, feature, 0.0, normal) :
                        airborneCushionNoseContactCandidate(cross.firstBall, feature, 0.0, normal));
                    continue;
                }
                if (cross.kind != TableDepartureEventKind::CrossDomainBallContact)
                    continue;
                const auto& first = staged.balls[cross.firstBall];
                const auto& second = staged.balls[cross.secondBall];
                const double x = double(second.position.x) - first.position.x;
                const double y = double(second.position.y) - first.position.y;
                const double z = double(second.position.z) - first.position.z;
                const double distance = std::sqrt(x*x + y*y + z*z);
                if (!(distance > 0.0) || !std::isfinite(distance) ||
                    std::fabs(distance - 2.0 * profile.ball.radiusCm) >
                        profile.solver.penetrationSlopCm)
                    return fail(PhysicsFailureCode::InvalidControls);
                ContinuousContactCandidate candidate;
                candidate.valid = true;
                candidate.kind = ContinuousContactKind::BallBall;
                candidate.firstBall = cross.firstBall;
                candidate.secondBall = cross.secondBall;
                candidate.normal = Point3{static_cast<float>(x/distance),
                    static_cast<float>(y/distance), static_cast<float>(z/distance)};
                candidate.penetrationCm = std::max(0.0, 2.0 * profile.ball.radiusCm - distance);
                instantaneous.push_back(candidate);
            }
            const auto geometry = buildTableDepartureGeometry(profile);
            std::array<bool, kBallCount> involved{};
            for (const auto& candidate : instantaneous) {
                if (candidate.firstBall >= 0) involved[candidate.firstBall] = true;
                if (candidate.secondBall >= 0) involved[candidate.secondBall] = true;
            }
            for (const auto& event : pendingTableEvents)
                if (isLanding(event.kind)) involved[event.ballIndex] = true;
            // Include supports reachable through touching ball constraints,
            // but do not manufacture unrelated stationary support islands.
            for (int pass = 0; pass < kBallCount; ++pass) {
                bool changed = false;
                for (int i = 0; i < kBallCount; ++i) {
                    if (staged.balls[i].pocketed) continue;
                    for (int j = i + 1; j < kBallCount; ++j) {
                        if (staged.balls[j].pocketed || involved[i] == involved[j]) continue;
                        const auto& a = staged.balls[i].position;
                        const auto& b = staged.balls[j].position;
                        const double x = double(a.x) - b.x;
                        const double y = double(a.y) - b.y;
                        const double z = double(a.z) - b.z;
                        const double reach = 2.0 * profile.ball.radiusCm +
                            profile.solver.penetrationSlopCm;
                        if (x*x + y*y + z*z <= reach*reach) {
                            involved[i] = involved[j] = true;
                            changed = true;
                        }
                    }
                }
                if (!changed) break;
            }
            for (int i = 0; i < kBallCount; ++i) {
                const auto& ball = staged.balls[i];
                const bool pendingLanding = std::any_of(pendingTableEvents.begin(),
                    pendingTableEvents.end(), [&](const auto& event) {
                        return event.ballIndex == i && isLanding(event.kind);
                    });
                if (!involved[i] || ball.pocketed ||
                    (ball.velocity.y > 0.0f && !pendingLanding)) continue;
                double plane = 0.0;
                if (ball.physicalDomain == PhysicalBallDomain::ClothSupported)
                    plane = geometry.clothSupportedCenterYCm;
                else if (ball.physicalDomain == PhysicalBallDomain::RailTopSupported)
                    plane = geometry.railTopSupportedCenterYCm;
                else if (ball.physicalDomain == PhysicalBallDomain::OutsideLandingSupported)
                    plane = geometry.outsideLandingSupportedCenterYCm;
                else continue;
                instantaneous.push_back(supportPlaneContactCandidate(i, plane));
            }
            auto contactTelemetry = updatePhysicsEventDriven(staged, 0.0f, profile,
                profile.solver.maximumEventsPerTick - consumedEvents, boundaryMode,
                elapsed + duration, true, &instantaneous);
            if (contactTelemetry.stepStatus == PhysicsStepStatus::Failed)
                return fail(contactTelemetry.failureCode,
                    "composed_contact_island:" + contactTelemetry.failureDetail);
            for (auto& event : pendingTableEvents) {
                auto& ball = staged.balls[event.ballIndex];
                if (isLanding(event.kind) && ball.velocity.y > 0.0f)
                    ball.physicalDomain = PhysicalBallDomain::Airborne;
                event.velocityAfterCmS = ball.velocity;
                event.targetDomain = ball.physicalDomain;
                if (isLanding(event.kind) ||
                    event.kind == TableDepartureEventKind::CushionNoseContact) {
                    const auto expectedKind = isLanding(event.kind) ?
                        PhysicsContactKind::Support : PhysicsContactKind::Rail;
                    for (std::size_t index = 0; index < contactTelemetry.contacts.size(); ++index) {
                        const auto& contact = contactTelemetry.contacts[index];
                        if (contact.kind != expectedKind ||
                            contact.firstBall != event.ballIndex ||
                            contact.wholeTickTimeOfImpactSeconds !=
                                event.absoluteWholeTickTimeSeconds) continue;
                        event.solverEventId = contact.solverEventId;
                        event.linkedPhysicsContactOrdinal = static_cast<int>(
                            telemetry.contacts.size() + index);
                    }
                }
            }
            contactTelemetry.tableDepartureEvents.insert(
                contactTelemetry.tableDepartureEvents.end(),
                pendingTableEvents.begin(), pendingTableEvents.end());
            ++consumedEvents;
            prependTelemetry(contactTelemetry, telemetry);
            telemetry = std::move(contactTelemetry);
        } else if (resolveDeparture) {
            if (consumedEvents + static_cast<int>(departureBatch.candidates.size()) >
                    profile.solver.maximumEventsPerTick)
                return fail(PhysicsFailureCode::EventBudget);
            const auto resolved = resolveTableDepartureBatch(staged, departureBatch,
                profile, elapsed + duration, privateOrdinal);
            if (!resolved.ok) return fail(PhysicsFailureCode::ContradictoryTopology,
                "departure_resolver:" + std::to_string(int(resolved.failureCode)));
            privateOrdinal += static_cast<int>(resolved.events.size());
            telemetry.tableDepartureEvents.insert(telemetry.tableDepartureEvents.end(),
                resolved.events.begin(), resolved.events.end());
            consumedEvents += static_cast<int>(resolved.events.size());
            for (const auto& event : resolved.events) {
                if (event.kind == TableDepartureEventKind::CrossDomainBallContact)
                    staged.events.ballCollision = true;
                if (event.kind == TableDepartureEventKind::CushionNoseContact)
                    staged.events.railCollision = true;
            }
        }
        for (const auto& event : telemetry.tableDepartureEvents) {
            if (event.kind != TableDepartureEventKind::PocketEntry) continue;
            auto& ball = staged.balls[event.ballIndex];
            if (ball.physicalDomain != PhysicalBallDomain::PocketChannel || ball.pocketed ||
                ball.trajectoryGeneration != event.trajectoryGeneration) continue;
            if (ball.pocketInteraction.phase == PocketInteractionPhase::Outside) {
                advancePocketInteraction(ball.pocketInteraction, event.pocketId,
                    PocketBoundaryEventKind::Mouth, PocketBoundaryRegion::Approaching, 0);
            }
            if (ball.pocketInteraction.pocketId != event.pocketId ||
                ball.pocketInteraction.phase == PocketInteractionPhase::Rejected)
                return fail(PhysicsFailureCode::ContradictoryTopology, "vertical_pocket_entry_state");
        }
        if (duration == 0.0 && !resolveLegacy && !resolveDeparture) {
            if (remaining == 0.0) break;
            return fail(PhysicsFailureCode::InvalidControls);
        }
        elapsed += duration;
    }
    staged.ballsMoving = anyBallMoving(staged);
    // Bind private pocket-entry diagnostics only to an actual later capture
    // of the same trajectory. Entry alone is never promoted into a capture.
    for (auto& event : telemetry.tableDepartureEvents) {
        if (event.kind != TableDepartureEventKind::PocketEntry) continue;
        for (std::size_t index = 0; index < telemetry.contacts.size(); ++index) {
            const auto& contact = telemetry.contacts[index];
            if (contact.kind != PhysicsContactKind::Pocket ||
                contact.pocketBoundaryEvent != PocketBoundaryEventKind::Capture ||
                contact.firstBall != event.ballIndex || contact.pocketId != event.pocketId ||
                contact.trajectoryGenerationBefore != event.trajectoryGeneration ||
                contact.wholeTickTimeOfImpactSeconds < event.absoluteWholeTickTimeSeconds)
                continue;
            event.linkedPhysicsContactOrdinal = static_cast<int>(index);
            event.pocketCaptureSequence = contact.pocketCaptureSequence;
            event.solverEventId = contact.solverEventId;
            break;
        }
    }
    if (profile.surface.torsionalSpinDecelerationRadS2 > 0.0f)
        for (const auto& ball : staged.balls)
            if (!ball.pocketed && ball.angularVelocity.y != 0.0f)
                staged.ballsMoving = true;
    staged.events.shotEnded = wasMoving && !staged.ballsMoving && staged.players.shotTaken;
    telemetry.wholeTickDurationSeconds = timeStep;
    return telemetry;
}

double planeAxisValue(const Point3& value, PhysicsPlaneAxis axis)
{
    switch (axis) {
    case PhysicsPlaneAxis::X: return value.x;
    case PhysicsPlaneAxis::Y: return value.y;
    case PhysicsPlaneAxis::Z: return value.z;
    }
    return std::numeric_limits<double>::quiet_NaN();
}

bool validPlaneObservationRequest(
    const PhysicsPlaneObservationRequest& request)
{
    if (request.observationId.empty() || request.observationId.size() > 128 ||
        request.ballIndex < 0 || request.ballIndex >= kBallCount ||
        !std::isfinite(request.planeCoordinateCm))
        return false;
    const bool validAxis = request.axis == PhysicsPlaneAxis::X ||
        request.axis == PhysicsPlaneAxis::Y ||
        request.axis == PhysicsPlaneAxis::Z;
    const bool validDirection = request.direction ==
            PhysicsPlaneCrossingDirection::NegativeToPositive ||
        request.direction ==
            PhysicsPlaneCrossingDirection::PositiveToNegative;
    return validAxis && validDirection;
}

bool sameCrossingPayload(const PhysicsPlaneCrossingRecord& first,
    const PhysicsPlaneCrossingRecord& second)
{
    return first.observationId == second.observationId &&
        first.ballIndex == second.ballIndex && first.axis == second.axis &&
        first.planeCoordinateCm == second.planeCoordinateCm &&
        first.direction == second.direction &&
        first.wholeTickTimeOfImpactSeconds ==
            second.wholeTickTimeOfImpactSeconds &&
        first.segmentOrdinal == second.segmentOrdinal &&
        first.signedDistanceBeforeCm == second.signedDistanceBeforeCm &&
        first.signedDistanceAfterCm == second.signedDistanceAfterCm &&
        first.axisVelocityAtCrossingCmS ==
            second.axisVelocityAtCrossingCmS &&
        std::string(first.origin) == second.origin &&
        first.complete == second.complete;
}

bool crossingForSegment(const PhysicsPlaneObservationRequest& request,
    const SurfaceMotionAnalyticSegment& segment,
    PhysicsPlaneCrossingRecord& record, bool includeFinalEndpoint = false,
    double timeTolerance = 0.0, double positionTolerance = 0.0)
{
    const double directionSign = request.direction ==
            PhysicsPlaneCrossingDirection::NegativeToPositive ? 1.0 : -1.0;
    const double position = planeAxisValue(segment.startPositionCm,
        request.axis);
    const double velocity = directionSign * planeAxisValue(
        segment.startVelocityCmS, request.axis);
    const double acceleration = directionSign * planeAxisValue(
        segment.constantAccelerationCmS2, request.axis);
    const double startDistance = directionSign *
        (position - request.planeCoordinateCm);
    const double duration = segment.durationSeconds;
    const double endDistance = startDistance + velocity * duration +
        0.5 * acceleration * duration * duration;
    if (!std::isfinite(startDistance) || !std::isfinite(endDistance) ||
        !std::isfinite(velocity) || !std::isfinite(acceleration) ||
        !std::isfinite(duration) || duration < 0.0)
        return false;
    if (request.allowStartOnPlaneDeparture &&
        segment.wholeTickStartOffsetSeconds == 0.0 &&
        startDistance == 0.0 && endDistance > 0.0 && velocity > 0.0) {
        record.observationId = request.observationId;
        record.ballIndex = request.ballIndex;
        record.axis = request.axis;
        record.planeCoordinateCm = request.planeCoordinateCm;
        record.direction = request.direction;
        record.wholeTickTimeOfImpactSeconds = 0.0;
        record.segmentOrdinal = segment.segmentOrdinal;
        record.signedDistanceBeforeCm = 0.0;
        record.signedDistanceAfterCm = endDistance;
        record.axisVelocityAtCrossingCmS = velocity;
        record.complete = true;
        return true;
    }
    double root = std::numeric_limits<double>::quiet_NaN();
    if (acceleration == 0.0) {
        if (!(startDistance < 0.0 && (endDistance > 0.0 ||
                (includeFinalEndpoint && std::fabs(endDistance) <= positionTolerance))))
            return false;
        if (!(velocity > 0.0)) return false;
        root = -startDistance / velocity;
    } else {
        const double discriminant = velocity * velocity -
            2.0 * acceleration * startDistance;
        if (!std::isfinite(discriminant) || discriminant < 0.0) return false;
        const double squareRoot = std::sqrt(discriminant);
        const double candidates[2] = {
            (-velocity - squareRoot) / acceleration,
            (-velocity + squareRoot) / acceleration,
        };
        for (double candidate : candidates) {
            if (!std::isfinite(candidate) || candidate < 0.0 ||
                candidate > duration + (includeFinalEndpoint ? timeTolerance : 0.0))
                continue;
            const double derivative = velocity + acceleration * candidate;
            if (!(derivative > 0.0)) continue;
            if (!std::isfinite(root) || candidate < root) root = candidate;
        }
    }
    if (!std::isfinite(root) || root < 0.0 ||
        root > duration + (includeFinalEndpoint ? timeTolerance : 0.0)) return false;
    const bool finalEndpointWithinTolerance = includeFinalEndpoint &&
        std::fabs(root - duration) <= timeTolerance &&
        std::fabs(endDistance) <= positionTolerance;
    if (root > duration && !finalEndpointWithinTolerance) return false;
    double derivative = velocity + acceleration * root;
    if (!(derivative > 0.0) || !std::isfinite(derivative)) return false;

    // A ballistic interval can cross a plane and return to its original side.
    // Use the containing monotonic leg as the directional witness; requiring
    // opposite signs at the entire interval's endpoints loses such crossings.
    double beforeWitness = startDistance;
    double afterWitness = endDistance;
    if (acceleration != 0.0) {
        const double turningTime = -velocity / acceleration;
        if (turningTime > 0.0 && turningTime < duration) {
            const double turningDistance = startDistance +
                velocity * turningTime + 0.5 * acceleration * turningTime * turningTime;
            if (turningTime < root) beforeWitness = turningDistance;
            else afterWitness = turningDistance;
        }
    }
    if (finalEndpointWithinTolerance && afterWitness <= 0.0) {
        root = duration;
        afterWitness = 0.0;
        derivative = velocity + acceleration * root;
        if (!(derivative > 0.0) || !std::isfinite(derivative)) return false;
    }
    if (!(beforeWitness < 0.0 && (afterWitness > 0.0 ||
            (finalEndpointWithinTolerance && afterWitness == 0.0))))
        return false;

    record.observationId = request.observationId;
    record.ballIndex = request.ballIndex;
    record.axis = request.axis;
    record.planeCoordinateCm = request.planeCoordinateCm;
    record.direction = request.direction;
    record.wholeTickTimeOfImpactSeconds =
        segment.wholeTickStartOffsetSeconds + root;
    record.segmentOrdinal = segment.segmentOrdinal;
    record.signedDistanceBeforeCm = beforeWitness;
    record.signedDistanceAfterCm = afterWitness;
    record.axisVelocityAtCrossingCmS = derivative;
    record.complete = true;
    return std::isfinite(record.wholeTickTimeOfImpactSeconds);
}

void appendPlaneObservationEvidence(PhysicsStepTelemetry& telemetry,
    const std::vector<PhysicsPlaneObservationRequest>& requests,
    const PhysicsProfile& profile)
{
    for (const PhysicsPlaneObservationRequest& request : requests) {
        PhysicsPlaneObservationAttestation attestation;
        attestation.observationId = request.observationId;
        attestation.ballIndex = request.ballIndex;
        attestation.wholeTickDurationSeconds =
            telemetry.wholeTickDurationSeconds;
        bool complete = true;
        double coveredUntil = 0.0;
        std::vector<PhysicsPlaneCrossingRecord> records;
        std::vector<const SurfaceMotionAnalyticSegment*> coveredSegments;
        const SurfaceMotionAnalyticSegment* finalSegment = nullptr;
        for (const auto& segment : telemetry.analyticSurfaceMotion)
            if (segment.ballIndex == request.ballIndex) finalSegment = &segment;
        for (const SurfaceMotionAnalyticSegment& segment :
             telemetry.analyticSurfaceMotion) {
            if (segment.ballIndex != request.ballIndex) continue;
            coveredSegments.push_back(&segment);
            ++attestation.coveredSegmentCount;
            if (!segment.analyticPathComplete ||
                !std::isfinite(segment.wholeTickStartOffsetSeconds) ||
                !std::isfinite(segment.durationSeconds) ||
                segment.durationSeconds < 0.0 ||
                std::fabs(segment.wholeTickStartOffsetSeconds -
                    coveredUntil) > 1e-6) {
                complete = false;
            }
            coveredUntil = segment.wholeTickStartOffsetSeconds +
                segment.durationSeconds;
            PhysicsPlaneCrossingRecord crossing;
            const bool includeFinalEndpoint = &segment == finalSegment &&
                std::fabs(coveredUntil - telemetry.wholeTickDurationSeconds) <= 1e-6;
            if (crossingForSegment(request, segment, crossing, includeFinalEndpoint,
                    profile.solver.toiToleranceSeconds, profile.solver.penetrationSlopCm))
                records.push_back(crossing);
        }
        const double directionSign = request.direction ==
                PhysicsPlaneCrossingDirection::NegativeToPositive
            ? 1.0 : -1.0;
        for (std::size_t index = 1; index < coveredSegments.size(); ++index) {
            const SurfaceMotionAnalyticSegment& previous =
                *coveredSegments[index - 1];
            const SurfaceMotionAnalyticSegment& next =
                *coveredSegments[index];
            const double previousStart = directionSign *
                (planeAxisValue(previous.startPositionCm, request.axis) -
                    request.planeCoordinateCm);
            const double previousEnd = directionSign *
                (planeAxisValue(previous.endPositionCm, request.axis) -
                    request.planeCoordinateCm);
            const double nextStart = directionSign *
                (planeAxisValue(next.startPositionCm, request.axis) -
                    request.planeCoordinateCm);
            const double nextEnd = directionSign *
                (planeAxisValue(next.endPositionCm, request.axis) -
                    request.planeCoordinateCm);
            const double boundaryVelocity = directionSign *
                planeAxisValue(next.startVelocityCmS, request.axis);
            if (!(previousStart < 0.0 && previousEnd == 0.0 &&
                  nextStart == 0.0 && nextEnd > 0.0 &&
                  boundaryVelocity > 0.0))
                continue;
            PhysicsPlaneCrossingRecord crossing;
            crossing.observationId = request.observationId;
            crossing.ballIndex = request.ballIndex;
            crossing.axis = request.axis;
            crossing.planeCoordinateCm = request.planeCoordinateCm;
            crossing.direction = request.direction;
            crossing.wholeTickTimeOfImpactSeconds =
                next.wholeTickStartOffsetSeconds;
            crossing.segmentOrdinal = previous.segmentOrdinal;
            crossing.signedDistanceBeforeCm = previousStart;
            crossing.signedDistanceAfterCm = nextEnd;
            crossing.axisVelocityAtCrossingCmS = boundaryVelocity;
            crossing.complete = true;
            records.push_back(crossing);
        }
        if (std::fabs(coveredUntil - telemetry.wholeTickDurationSeconds) >
                1e-6 || attestation.coveredSegmentCount == 0)
            complete = false;
        std::sort(records.begin(), records.end(),
            [](const auto& left, const auto& right) {
                return std::tie(left.observationId, left.ballIndex,
                           left.wholeTickTimeOfImpactSeconds,
                           left.segmentOrdinal, left.direction) <
                    std::tie(right.observationId, right.ballIndex,
                           right.wholeTickTimeOfImpactSeconds,
                           right.segmentOrdinal, right.direction);
            });
        std::vector<PhysicsPlaneCrossingRecord> canonical;
        for (const PhysicsPlaneCrossingRecord& record : records) {
            if (!canonical.empty() &&
                canonical.back().observationId == record.observationId &&
                canonical.back().ballIndex == record.ballIndex &&
                canonical.back().wholeTickTimeOfImpactSeconds ==
                    record.wholeTickTimeOfImpactSeconds &&
                canonical.back().direction == record.direction) {
                if (!sameCrossingPayload(canonical.back(), record))
                    complete = false;
                continue;
            }
            canonical.push_back(record);
        }
        if (!complete) canonical.clear();
        attestation.crossingRecordCount =
            static_cast<int>(canonical.size());
        attestation.complete = complete;
        telemetry.planeCrossings.insert(telemetry.planeCrossings.end(),
            canonical.begin(), canonical.end());
        telemetry.planeAttestations.push_back(attestation);
    }
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
    return updatePhysics(state, timeStep, profile, boundaryMode, {});
}

PhysicsStepTelemetry updatePhysics(
    GameState& state, float timeStep, const PhysicsProfile& profile,
    PhysicsBoundaryMode boundaryMode,
    const std::vector<PhysicsPlaneObservationRequest>& planeObservations)
{
    const GameState snapshot = state;
    PhysicsStepTelemetry telemetry;
    if (std::any_of(state.balls.begin(), state.balls.end(),
            [](const BallState& ball) { return ball.outOfPlay; })) {
        // Reuse the solver's inactive-participant mask internally, retaining
        // the explicit off-table state at the public boundary. No capture,
        // relocation or pocket-count increment is synthesized.
        auto active = state;
        for (auto& ball : active.balls) if (ball.outOfPlay) {
            ball.pocketed = true;
            ball.outOfPlay = false;
        }
        telemetry = updatePhysics(active, timeStep, profile, boundaryMode, planeObservations);
        if (telemetry.stepStatus == PhysicsStepStatus::Failed) return telemetry;
        for (int ball = 0; ball < kBallCount; ++ball)
            if (snapshot.balls[ball].outOfPlay) active.balls[ball] = snapshot.balls[ball];
        active.pocketedBallCount = static_cast<int>(std::count_if(active.balls.begin(), active.balls.end(),
            [](const BallState& ball) { return ball.pocketed; }));
        state = std::move(active);
        return telemetry;
    }
    bool needsDepartureFoundation = false;
    for (const BallState& ball : state.balls) {
        if (!ball.pocketed && ball.physicalDomain !=
                PhysicalBallDomain::ClothSupported) {
            needsDepartureFoundation = true;
            break;
        }
    }
    std::vector<std::string> observationIds;
    for (const PhysicsPlaneObservationRequest& request : planeObservations) {
        if (!validPlaneObservationRequest(request) ||
            std::find(observationIds.begin(), observationIds.end(),
                request.observationId) != observationIds.end()) {
            failStep(telemetry, PhysicsFailureCode::InvalidControls);
            return telemetry;
        }
        observationIds.push_back(request.observationId);
    }
    // The shipping interactive profile opts into coupled table support for
    // ordinary cloth contacts. Preserved evidence profiles retain their
    // original planar solver unless an explicit airborne/departure domain
    // actually requires the composed scheduler. Nonzero vertical velocity
    // produced by their old cushion law is not an opt-in to a different model.
    if (boundaryMode == PhysicsBoundaryMode::ProductionTable &&
        profile.id == "chinese_pool_interactive_120hz_v5")
        needsDepartureFoundation = true;
    if (needsDepartureFoundation) {
        GameState staged = snapshot;
        telemetry = runComposedEventScheduler(staged, timeStep, profile, boundaryMode);
        if (telemetry.stepStatus == PhysicsStepStatus::Failed) {
            state = snapshot;
            clearCanonicalTelemetry(telemetry);
            return telemetry;
        }
        if (!validateComposedFinalState(snapshot, staged, profile,
                telemetry)) {
            state = snapshot;
            clearCanonicalTelemetry(telemetry);
            return telemetry;
        }
        telemetry.wholeTickDurationSeconds = timeStep;
        std::stable_sort(telemetry.analyticSurfaceMotion.begin(),
            telemetry.analyticSurfaceMotion.end(), [](const auto& a, const auto& b) {
                return std::tie(a.wholeTickStartOffsetSeconds, a.ballIndex,
                           a.segmentOrdinal) <
                    std::tie(b.wholeTickStartOffsetSeconds, b.ballIndex,
                           b.segmentOrdinal);
            });
        int mixedSegmentOrdinal = 0;
        for (SurfaceMotionAnalyticSegment& segment :
             telemetry.analyticSurfaceMotion)
            segment.segmentOrdinal = mixedSegmentOrdinal++;
        if (!verifyComposedAnalyticPartition(telemetry, snapshot, staged, profile,
                &telemetry.failureDetail)) {
            state = snapshot;
            failStep(telemetry, PhysicsFailureCode::InvalidControls);
            clearCanonicalTelemetry(telemetry);
            return telemetry;
        }
        appendPlaneObservationEvidence(telemetry, planeObservations, profile);
        for (const auto& attestation : telemetry.planeAttestations) {
            if (!attestation.complete) {
                state = snapshot;
                failStep(telemetry, PhysicsFailureCode::InvalidControls);
                clearCanonicalTelemetry(telemetry);
                return telemetry;
            }
        }
        state = staged;
        return telemetry;
    }
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
        boundaryMode, 0.0);
    telemetry.wholeTickDurationSeconds = timeStep;
    int analyticSegmentOrdinal = 0;
    for (SurfaceMotionAnalyticSegment& segment :
         telemetry.analyticSurfaceMotion)
        segment.segmentOrdinal = analyticSegmentOrdinal++;
    const double energyAfter = totalGameEnergyJ(state, profile);
    if (telemetry.stepStatus == PhysicsStepStatus::Succeeded &&
        (!finiteGameState(state) || !std::isfinite(energyAfter))) {
        failStep(telemetry, !std::isfinite(energyAfter)
            ? PhysicsFailureCode::NonfiniteEnergy
            : PhysicsFailureCode::NonfiniteState);
    }
    // Each converged solver event already enforces the configured passive
    // energy tolerance. The tick-level legacy gate therefore budgets the sum
    // of those independent floating-point bounds instead of applying one
    // island's bound to an arbitrarily long event chain.
    const double tickEnergyTolerance = profile.formulaVersion == "legacy_v1"
        ? profile.solver.passiveEnergyToleranceJ * std::max<std::size_t>(
            1, telemetry.solverEvents.size())
        : profile.solver.passiveEnergyToleranceJ;
    if (telemetry.stepStatus == PhysicsStepStatus::Succeeded &&
        energyAfter > energyBefore + tickEnergyTolerance) {
        failStep(telemetry, PhysicsFailureCode::PassiveEnergyCreation);
    }
    if (telemetry.stepStatus == PhysicsStepStatus::Failed) {
        state = snapshot;
        clearCanonicalTelemetry(telemetry);
    }
    if (telemetry.stepStatus == PhysicsStepStatus::Succeeded)
        appendPlaneObservationEvidence(telemetry, planeObservations, profile);
    return telemetry;
}

PhysicsStepTelemetry updatePhysics(GameState& state, float timeStep)
{
    return updatePhysics(state, timeStep, defaultChinesePoolPhysicsProfile());
}

}  // namespace billiardgl
