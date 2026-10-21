#include "cue_contact.h"

#include <algorithm>
#include <cmath>
#include <iomanip>
#include <limits>
#include <sstream>

namespace billiardgl {
namespace {
bool finitePoint(const Point3& p)
{
    return std::isfinite(p.x)&&std::isfinite(p.y)&&std::isfinite(p.z);
}
std::string observationDigest(const std::string& bytes)
{
    std::uint64_t hash=1469598103934665603ULL;
    for(const unsigned char value:bytes){hash^=value;hash*=1099511628211ULL;}
    std::ostringstream out;out<<std::hex<<std::setfill('0')<<std::setw(16)<<hash;
    return out.str();
}
void appendPoint(std::ostringstream& out,const Point3& value)
{
    out<<'|'<<value.x<<','<<value.y<<','<<value.z;
}
}

const char* cueTipIntervalOriginName(CueTipIntervalOrigin origin)
{
    switch(origin){
    case CueTipIntervalOrigin::AnalyticCueContactSolver:
        return "ANALYTIC_CUE_CONTACT_SOLVER";
    case CueTipIntervalOrigin::CoupledCueContactSolver:
        return "COUPLED_CUE_CONTACT_SOLVER";
    case CueTipIntervalOrigin::AuthoritativeCueMotionObserver:
        return "AUTHORITATIVE_CUE_MOTION_OBSERVER";
    }
    return "UNKNOWN";
}

CanonicalCueTipStrokeObservationValidation
validateCanonicalCueTipStrokeObservation(
    CanonicalCueTipStrokeObservation& observation)
{
    CanonicalCueTipStrokeObservationValidation result;
    if(observation.schemaId!="canonical_cue_tip_stroke_observation_v1"||
       observation.shotIdentity.empty()||observation.actorPlayerId<0||
       observation.actorPlayerId>1||observation.rackIdentity.empty()||
       observation.rulesStateHash.empty()||
       observation.endTick<observation.startTick){
        result.error="stroke_observation_identity";return result;}
    double previousEnd=-1.0;std::ostringstream canonical;
    canonical<<std::setprecision(std::numeric_limits<double>::max_digits10)
        <<observation.schemaId<<'|'<<observation.shotIdentity<<'|'
        <<observation.actorPlayerId<<'|'<<observation.rackIdentity<<'|'
        <<observation.rulesRevision<<'|'<<observation.rulesStateHash<<'|'
        <<observation.startTick<<'|'<<observation.endTick;
    for(std::size_t i=0;i<observation.intervals.size();++i){
        const auto& in=observation.intervals[i];
        if(in.ordinal!=static_cast<int>(i)||!std::isfinite(in.startTimeSeconds)||
           !std::isfinite(in.endTimeSeconds)||in.startTimeSeconds<0.0||
           in.endTimeSeconds<=in.startTimeSeconds||
           (i&&in.startTimeSeconds<previousEnd)||
           !std::isfinite(in.startTickOffsetSeconds)||
           !std::isfinite(in.endTickOffsetSeconds)||
           in.startTickOffsetSeconds<0.0||
           in.endTickOffsetSeconds<=in.startTickOffsetSeconds||
           !finitePoint(in.contactPoint)||!finitePoint(in.contactNormal)||
           !finitePoint(in.cueVelocityStart)||!finitePoint(in.cueVelocityEnd)||
           !finitePoint(in.cueBallVelocityStart)||!finitePoint(in.cueBallVelocityEnd)||
           !std::isfinite(in.normalImpulseNs)||!std::isfinite(in.tangentialImpulseNs)||
           !std::isfinite(in.relativeTipBallPathLengthCm)||
           in.relativeTipBallPathLengthCm<0.0||
           std::string(cueTipIntervalOriginName(in.origin))=="UNKNOWN"){
            result.error="stroke_observation_interval";return result;}
        previousEnd=in.endTimeSeconds;
        canonical<<'|'<<in.ordinal<<'|'<<in.startTimeSeconds<<'|'
            <<in.endTimeSeconds<<'|'<<in.startTickOffsetSeconds<<'|'
            <<in.endTickOffsetSeconds<<'|'<<cueTipIntervalOriginName(in.origin);
        appendPoint(canonical,in.contactPoint);appendPoint(canonical,in.contactNormal);
        appendPoint(canonical,in.cueVelocityStart);appendPoint(canonical,in.cueVelocityEnd);
        appendPoint(canonical,in.cueBallVelocityStart);
        appendPoint(canonical,in.cueBallVelocityEnd);
        canonical<<'|'<<in.normalImpulseNs<<'|'<<in.tangentialImpulseNs<<'|'
            <<in.relativeTipBallPathLengthCm<<'|'
            <<(in.relativeTipBallPathLengthAttested?1:0);
        for(const int eventId:in.solverEventIds)canonical<<'|'<<eventId;
    }
    observation.allValuesFinite=true;observation.canonicalOrder=true;
    observation.fullStrokeComplete=
        observation.observationStartedBeforeFirstContact&&
        observation.observationEndedAfterAuthoritativeRelease&&
        observation.noDroppedIntervals&&observation.successfulLaunchCommit;
    observation.digest=observationDigest(canonical.str());
    result.valid=true;result.complete=observation.fullStrokeComplete;
    if(!result.complete)result.error="stroke_observation_incomplete";
    return result;
}

bool CanonicalCueTipStrokeObserver::begin(
    const CanonicalCueTipStrokeObservation& identity)
{
    if(active_||identity.shotIdentity.empty()||identity.actorPlayerId<0||
       identity.actorPlayerId>1||identity.rackIdentity.empty()||
       identity.rulesStateHash.empty())return false;
    observation_=identity;observation_.intervals.clear();
    observation_.observationStartedBeforeFirstContact=true;
    observation_.observationEndedAfterAuthoritativeRelease=false;
    observation_.noDroppedIntervals=true;
    observation_.successfulLaunchCommit=true;
    observation_.fullStrokeComplete=false;observation_.digest.clear();
    active_=true;committed_=false;return true;
}

bool CanonicalCueTipStrokeObserver::append(
    const CanonicalCueTipContactInterval& value)
{
    if(!active_&&!committed_)return false;
    CanonicalCueTipContactInterval interval=value;
    if(committed_){
        if(interval.ordinal<0||
           static_cast<std::size_t>(interval.ordinal)>=observation_.intervals.size())
            return false;
        const auto& existing=observation_.intervals[
            static_cast<std::size_t>(interval.ordinal)];
        return interval.startTimeSeconds==existing.startTimeSeconds&&
            interval.endTimeSeconds==existing.endTimeSeconds&&
            interval.origin==existing.origin&&
            interval.solverEventIds==existing.solverEventIds;
    }
    if(!observation_.intervals.empty()){
        auto& previous=observation_.intervals.back();
        if(interval.startTimeSeconds==previous.startTimeSeconds&&
           interval.endTimeSeconds==previous.endTimeSeconds&&
           interval.origin==previous.origin&&
           interval.solverEventIds==previous.solverEventIds)return true;
        if(interval.startTimeSeconds<previous.endTimeSeconds)return false;
        if(interval.startTimeSeconds==previous.endTimeSeconds&&
           interval.origin==previous.origin){
            CanonicalCueTipContactInterval merged=previous;
            merged.endTimeSeconds=interval.endTimeSeconds;
            merged.endTickOffsetSeconds=interval.endTickOffsetSeconds;
            merged.cueVelocityEnd=interval.cueVelocityEnd;
            merged.cueBallVelocityEnd=interval.cueBallVelocityEnd;
            merged.normalImpulseNs+=interval.normalImpulseNs;
            merged.tangentialImpulseNs+=interval.tangentialImpulseNs;
            merged.relativeTipBallPathLengthCm+=
                interval.relativeTipBallPathLengthCm;
            merged.relativeTipBallPathLengthAttested=
                merged.relativeTipBallPathLengthAttested&&
                interval.relativeTipBallPathLengthAttested;
            merged.solverEventIds.insert(merged.solverEventIds.end(),
                interval.solverEventIds.begin(),interval.solverEventIds.end());
            CanonicalCueTipStrokeObservation candidate=observation_;
            candidate.intervals.back()=merged;
            candidate.endTick=candidate.startTick;
            candidate.observationEndedAfterAuthoritativeRelease=true;
            const auto validation=
                validateCanonicalCueTipStrokeObservation(candidate);
            if(!validation.valid)return false;
            previous=std::move(merged);return true;
        }
    }
    interval.ordinal=static_cast<int>(observation_.intervals.size());
    CanonicalCueTipStrokeObservation candidate=observation_;
    candidate.intervals.push_back(interval);candidate.endTick=candidate.startTick;
    candidate.observationEndedAfterAuthoritativeRelease=true;
    auto validation=validateCanonicalCueTipStrokeObservation(candidate);
    if(!validation.valid)return false;
    observation_.intervals.push_back(std::move(interval));return true;
}

CanonicalCueTipStrokeObservationValidation
CanonicalCueTipStrokeObserver::finish(std::uint64_t endTick)
{
    CanonicalCueTipStrokeObservationValidation result;
    if(committed_&&endTick==observation_.endTick){
        result.valid=true;result.complete=observation_.fullStrokeComplete;
        return result;
    }
    if(!active_||committed_||endTick<observation_.startTick){
        result.error="stroke_observation_finish_state";return result;}
    CanonicalCueTipStrokeObservation candidate=observation_;
    candidate.endTick=endTick;
    candidate.observationEndedAfterAuthoritativeRelease=true;
    result=validateCanonicalCueTipStrokeObservation(candidate);
    if(result.valid&&result.complete){observation_=std::move(candidate);
        active_=false;committed_=true;}
    return result;
}

void CanonicalCueTipStrokeObserver::rollback()
{
    active_=false;committed_=false;
    observation_=CanonicalCueTipStrokeObservation{};
}
namespace {

using Vector = std::array<double, 3>;

Vector add(const Vector& first, const Vector& second)
{
    return {{first[0] + second[0], first[1] + second[1], first[2] + second[2]}};
}

Vector subtract(const Vector& first, const Vector& second)
{
    return {{first[0] - second[0], first[1] - second[1], first[2] - second[2]}};
}

Vector multiply(const Vector& value, double scale)
{
    return {{value[0] * scale, value[1] * scale, value[2] * scale}};
}

double dot(const Vector& first, const Vector& second)
{
    return first[0] * second[0] + first[1] * second[1] + first[2] * second[2];
}

Vector cross(const Vector& first, const Vector& second)
{
    return {{
        first[1] * second[2] - first[2] * second[1],
        first[2] * second[0] - first[0] * second[2],
        first[0] * second[1] - first[1] * second[0]}};
}

double magnitude(const Vector& value)
{
    return std::sqrt(dot(value, value));
}

bool finiteVector(const Vector& value)
{
    return std::isfinite(value[0]) && std::isfinite(value[1]) && std::isfinite(value[2]);
}

CueContactResult unsupported(const char* error)
{
    CueContactResult result;
    result.error = error;
    return result;
}

double ballKineticEnergy(const Vector& velocity, const Vector& angularVelocity,
    double massKg, double inertia)
{
    return 0.5 * massKg * dot(velocity, velocity) +
        0.5 * inertia * dot(angularVelocity, angularVelocity);
}

}  // namespace

const char* cueContactRegimeName(CueContactRegime regime)
{
    switch (regime) {
    case CueContactRegime::Stick: return "stick";
    case CueContactRegime::Slip: return "slip";
    case CueContactRegime::Miscue: return "miscue";
    case CueContactRegime::Released: return "released";
    case CueContactRegime::Unsupported: return "unsupported";
    }
    return "unsupported";
}

CueContactResult resolveCueContact(BallState& ball, const CueImpactInput& input,
    const BallProperties& ballProperties, const CueProperties& cueProperties)
{
    if (!std::isfinite(input.cueSpeedCmS) || input.cueSpeedCmS < 0.0 ||
        !std::isfinite(input.cueMassKg) || input.cueMassKg <= 0.0 ||
        !finiteVector(input.direction) || !std::isfinite(input.elevationDegrees) ||
        !std::isfinite(input.tipOffsetRadius[0]) || !std::isfinite(input.tipOffsetRadius[1]) ||
        ballProperties.massKg <= 0.0f || ballProperties.radiusCm <= 0.0f) {
        return unsupported("invalid_cue_contact_input");
    }
    if (std::fabs(input.elevationDegrees) > 1e-9 || std::fabs(input.direction[1]) > 1e-9) {
        return unsupported("cue_elevation_requires_3d");
    }

    Vector direction = input.direction;
    const double directionLength = magnitude(direction);
    if (directionLength <= 0.0) return unsupported("invalid_cue_direction");
    direction = multiply(direction, 1.0 / directionLength);

    const double sideOffset = input.tipOffsetRadius[0];
    const double verticalOffset = input.tipOffsetRadius[1];
    const double offsetFraction = std::sqrt(
        sideOffset * sideOffset + verticalOffset * verticalOffset);
    if (offsetFraction >= 1.0) return unsupported("cue_offset_outside_ball");
    const double radiusM = ballProperties.radiusCm / 100.0;
    const double inertia = 0.4 * ballProperties.massKg * radiusM * radiusM;
    const Vector ballVelocity = {{ball.velocity.x / 100.0,
        ball.velocity.y / 100.0, ball.velocity.z / 100.0}};
    const Vector angularVelocity = {{ball.angularVelocity.x,
        ball.angularVelocity.y, ball.angularVelocity.z}};
    const Vector cueVelocity = multiply(direction, input.cueSpeedCmS / 100.0);
    if (offsetFraction > cueProperties.maximumReliableOffsetRadius) {
        CueContactResult result;
        result.regime = CueContactRegime::Miscue;
        result.error = "miscue_offset_exceeds_reliable_radius";
        result.frictionCoefficient = input.chalkState == "UNCHALKED" ?
            cueProperties.unchalkedFrictionCoefficient :
            cueProperties.chalkedFrictionCoefficient;
        result.cueVelocityBeforeMS = cueVelocity;
        result.cueVelocityAfterMS = cueVelocity;
        result.ballVelocityBeforeMS = ballVelocity;
        result.ballVelocityAfterMS = ballVelocity;
        result.ballAngularVelocityBeforeRadS = angularVelocity;
        result.ballAngularVelocityAfterRadS = angularVelocity;
        result.inputKineticEnergyJ = ballKineticEnergy(
            ballVelocity, angularVelocity, ballProperties.massKg, inertia) +
            0.5 * input.cueMassKg * dot(cueVelocity, cueVelocity);
        result.outputKineticEnergyJ = result.inputKineticEnergyJ;
        return result;
    }

    const Vector side = {{-direction[2], 0.0, direction[0]}};
    const Vector up = {{0.0, 1.0, 0.0}};
    const Vector offset = add(multiply(side, sideOffset * radiusM),
        multiply(up, verticalOffset * radiusM));
    const Vector arm = add(multiply(direction,
        -radiusM * std::sqrt(std::max(0.0, 1.0 - offsetFraction * offsetFraction))), offset);
    const Vector normal = multiply(arm, -1.0 / radiusM);

    const Vector contactVelocity = add(ballVelocity, cross(angularVelocity, arm));
    const Vector relativeVelocity = subtract(cueVelocity, contactVelocity);
    const double approachSpeed = dot(relativeVelocity, direction);
    if (approachSpeed <= 0.0) return unsupported("cue_not_approaching");

    const Vector armCrossDirection = cross(arm, direction);
    const double inverseEffectiveMass = 1.0 / input.cueMassKg +
        1.0 / ballProperties.massKg + dot(armCrossDirection, armCrossDirection) / inertia;
    const double desiredMagnitude =
        (1.0 + cueProperties.normalRestitution) * approachSpeed / inverseEffectiveMass;
    const Vector desiredImpulse = multiply(direction, desiredMagnitude);
    const double normalImpulse = dot(desiredImpulse, normal);
    if (normalImpulse <= 0.0) return unsupported("cue_not_approaching_contact_normal");
    const Vector desiredTangent = subtract(desiredImpulse, multiply(normal, normalImpulse));
    const double desiredTangentialMagnitude = magnitude(desiredTangent);
    const double friction = input.chalkState == "UNCHALKED" ?
        cueProperties.unchalkedFrictionCoefficient : cueProperties.chalkedFrictionCoefficient;

    CueContactResult result;
    result.frictionCoefficient = friction;
    result.normalImpulseNs = normalImpulse;
    result.normalRelativeSpeedBeforeMS = dot(relativeVelocity, normal);
    result.contactArmM = arm;
    result.contactNormal = normal;
    result.tangentialRelativeVelocityBeforeMS = subtract(
        relativeVelocity, multiply(normal, dot(relativeVelocity, normal)));
    result.tangentialRelativeSpeedBeforeMS =
        magnitude(result.tangentialRelativeVelocityBeforeMS);
    result.cueVelocityBeforeMS = cueVelocity;
    result.cueVelocityAfterMS = cueVelocity;
    result.ballVelocityBeforeMS = ballVelocity;
    result.ballVelocityAfterMS = ballVelocity;
    result.ballAngularVelocityBeforeRadS = angularVelocity;
    result.ballAngularVelocityAfterRadS = angularVelocity;
    result.inputKineticEnergyJ = ballKineticEnergy(
        ballVelocity, angularVelocity, ballProperties.massKg, inertia) +
        0.5 * input.cueMassKg * dot(cueVelocity, cueVelocity);
    result.outputKineticEnergyJ = result.inputKineticEnergyJ;

    Vector impulse = desiredImpulse;
    if (desiredTangentialMagnitude <= friction * normalImpulse + 1e-12) {
        result.regime = CueContactRegime::Stick;
        result.tangentialImpulseNs = desiredTangentialMagnitude;
    } else {
        result.regime = CueContactRegime::Slip;
        result.tangentialImpulseNs = friction * normalImpulse;
        impulse = add(multiply(normal, normalImpulse), multiply(
            desiredTangent, result.tangentialImpulseNs / desiredTangentialMagnitude));
        if (std::fabs(impulse[1]) > 1e-9) {
            result.regime = CueContactRegime::Unsupported;
            result.error = "vertical_ball_impulse_requires_3d";
            return result;
        }
    }

    const Vector deltaVelocity = multiply(impulse, 1.0 / ballProperties.massKg);
    const Vector deltaAngularVelocity = multiply(cross(arm, impulse), 1.0 / inertia);
    ball.velocity.x += static_cast<float>(deltaVelocity[0] * 100.0);
    ball.velocity.y += static_cast<float>(deltaVelocity[1] * 100.0);
    ball.velocity.z += static_cast<float>(deltaVelocity[2] * 100.0);
    ball.angularVelocity.x += static_cast<float>(deltaAngularVelocity[0]);
    ball.angularVelocity.y += static_cast<float>(deltaAngularVelocity[1]);
    ball.angularVelocity.z += static_cast<float>(deltaAngularVelocity[2]);
    ball.speed = std::sqrt(ball.velocity.x * ball.velocity.x +
        ball.velocity.y * ball.velocity.y + ball.velocity.z * ball.velocity.z);

    result.impulseNs = impulse;
    result.cueVelocityAfterMS = subtract(cueVelocity,
        multiply(impulse, 1.0 / input.cueMassKg));
    result.ballVelocityAfterMS = add(ballVelocity, deltaVelocity);
    result.ballAngularVelocityAfterRadS = add(
        angularVelocity, deltaAngularVelocity);
    result.outputKineticEnergyJ = ballKineticEnergy(
        result.ballVelocityAfterMS, result.ballAngularVelocityAfterRadS,
        ballProperties.massKg, inertia) + 0.5 * input.cueMassKg *
        dot(result.cueVelocityAfterMS, result.cueVelocityAfterMS);
    result.applied = true;
    CanonicalCueTipContactInterval interval;
    interval.ordinal=0;interval.startTimeSeconds=0.0;
    interval.endTimeSeconds=1e-9;
    interval.startTickOffsetSeconds=0.0;
    interval.endTickOffsetSeconds=1e-9;
    interval.contactNormal=Point3{static_cast<float>(normal[0]),
        static_cast<float>(normal[1]),static_cast<float>(normal[2])};
    interval.cueVelocityStart=Point3{
        static_cast<float>(result.cueVelocityBeforeMS[0]),
        static_cast<float>(result.cueVelocityBeforeMS[1]),
        static_cast<float>(result.cueVelocityBeforeMS[2])};
    interval.cueVelocityEnd=interval.cueVelocityStart;
    interval.cueBallVelocityStart=Point3{
        static_cast<float>(result.ballVelocityBeforeMS[0]),
        static_cast<float>(result.ballVelocityBeforeMS[1]),
        static_cast<float>(result.ballVelocityBeforeMS[2])};
    interval.cueBallVelocityEnd=Point3{
        static_cast<float>(result.ballVelocityAfterMS[0]),
        static_cast<float>(result.ballVelocityAfterMS[1]),
        static_cast<float>(result.ballVelocityAfterMS[2])};
    interval.normalImpulseNs=result.normalImpulseNs;
    interval.tangentialImpulseNs=result.tangentialImpulseNs;
    // The analytic contact interval uses a linear relative-velocity
    // trajectory.  Deterministic Simpson integration measures its line
    // integral; this is deliberately not endpoint displacement.
    constexpr int kPathSlices=64;
    const double duration=interval.endTimeSeconds-interval.startTimeSeconds;
    double weightedSpeed=0.0;
    for(int slice=0;slice<=kPathSlices;++slice){
        const double t=static_cast<double>(slice)/kPathSlices;
        const Vector cue=multiply(result.cueVelocityBeforeMS,1.0-t);
        const Vector cueEnd=multiply(result.cueVelocityAfterMS,t);
        const Vector ballStart=multiply(result.ballVelocityBeforeMS,1.0-t);
        const Vector ballEnd=multiply(result.ballVelocityAfterMS,t);
        const Vector relative=subtract(add(cue,cueEnd),add(ballStart,ballEnd));
        const double speed=std::sqrt(dot(relative,relative));
        const int weight=(slice==0||slice==kPathSlices)?1:(slice%2?4:2);
        weightedSpeed+=weight*speed;
    }
    interval.relativeTipBallPathLengthCm=
        weightedSpeed*duration*100.0/(3.0*kPathSlices);
    interval.relativeTipBallPathLengthAttested=true;
    result.canonicalTipIntervals.push_back(interval);
    return result;
}

}  // namespace billiardgl
