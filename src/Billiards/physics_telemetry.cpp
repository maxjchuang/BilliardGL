#include "physics_telemetry.h"

#include <cmath>
#include <algorithm>
#include <iomanip>
#include <limits>
#include <set>
#include <sstream>
#include <tuple>

namespace billiardgl {
namespace {

constexpr double kCentimetersPerMeter = 100.0;
bool finitePoint(const Point3& p)
{return std::isfinite(p.x)&&std::isfinite(p.y)&&std::isfinite(p.z);}
std::string d5bDigest(const std::string& bytes)
{
    std::uint64_t h=1469598103934665603ULL;
    for(const unsigned char c:bytes){h^=c;h*=1099511628211ULL;}
    std::ostringstream out;out<<std::hex<<std::setfill('0')<<std::setw(16)<<h;
    return out.str();
}

Point3 accelerationBetween(const Point3& before, const Point3& after, float deltaSeconds)
{
    if (deltaSeconds <= 0.0f) {
        return Point3{};
    }
    return Point3{
        (after.x - before.x) / deltaSeconds,
        (after.y - before.y) / deltaSeconds,
        (after.z - before.z) / deltaSeconds};
}

bool validFrozenBindingForShot(const CanonicalFrozenCushionBinding& binding,
    const std::string& shotIdentity)
{
    const bool validAxis = binding.planeAxis == PhysicsPlaneAxis::X ||
        binding.planeAxis == PhysicsPlaneAxis::Y ||
        binding.planeAxis == PhysicsPlaneAxis::Z;
    const bool validDirection = binding.departureDirection ==
            PhysicsPlaneCrossingDirection::NegativeToPositive ||
        binding.departureDirection ==
            PhysicsPlaneCrossingDirection::PositiveToNegative;
    return !binding.declarationDecisionId.empty() &&
        binding.declaredObjectBallId > 0 &&
        binding.declaredObjectBallId < kBallCount &&
        binding.namedStraightCushionFeatureId >= 0 &&
        binding.namedStraightCushionFeatureId <= 3 &&
        !binding.declarationParentStateHash.empty() &&
        binding.consumedShotIdentity == shotIdentity &&
        !binding.planeObservationId.empty() &&
        std::isfinite(binding.contactPlaneCoordinateCm) && validAxis &&
        validDirection && binding.origin &&
        std::string(binding.origin) ==
            "SIGNED_FROZEN_CUSHION_DECLARATION_V1" &&
        binding.authenticated && binding.committedAndConsumed;
}

}  // namespace

const char* canonicalCueObjectTipOrderName(CanonicalCueObjectTipOrder order)
{
    switch(order){
    case CanonicalCueObjectTipOrder::ContactBeforeTipSeparation:return "CUE_OBJECT_CONTACT_BEFORE_TIP_SEPARATION";
    case CanonicalCueObjectTipOrder::ContactEqualTipSeparation:return "CUE_OBJECT_CONTACT_EQUAL_TIP_SEPARATION";
    case CanonicalCueObjectTipOrder::ContactAfterTipSeparation:return "CUE_OBJECT_CONTACT_AFTER_TIP_SEPARATION";
    case CanonicalCueObjectTipOrder::NoCueObjectContact:return "NO_CUE_OBJECT_CONTACT";
    case CanonicalCueObjectTipOrder::UnorderedIncomplete:return "UNORDERED_INCOMPLETE";
    }
    return "UNKNOWN";
}

CanonicalD5BTelemetryValidation validateCanonicalCueObjectOrderObservation(
    CanonicalCueObjectOrderObservation& observation)
{
    CanonicalD5BTelemetryValidation r;
    if(observation.schemaId!="canonical_cue_object_tip_separation_order_v1"||
       observation.shotIdentity.empty()||observation.actorPlayerId<0||
       observation.actorPlayerId>1||observation.rackIdentity.empty()||
       observation.rulesStateHash.empty()||observation.physicsTickStart==0||
       (observation.physicsTickEnd&&
        observation.physicsTickEnd<observation.physicsTickStart)||
       !std::isfinite(observation.tipSeparationTimeSeconds)||
       observation.tipSeparationTimeSeconds<0.0){r.error="d5b_order_identity";return r;}
    double previous=-1.0;
    std::tuple<double,int,int,int> previousKey{-1.0,-1,-1,-1};
    std::set<std::tuple<std::uint64_t,int,int,int>> evidenceKeys;
    std::ostringstream bytes;
    bytes<<std::setprecision(std::numeric_limits<double>::max_digits10)
         <<observation.shotIdentity<<'|'<<observation.tipSeparationTimeSeconds;
    for(std::size_t i=0;i<observation.contacts.size();++i){const auto& e=observation.contacts[i];
        if(e.objectBallId<1||e.objectBallId>=kBallCount||
           !std::isfinite(e.strokeRelativeTimeSeconds)||e.strokeRelativeTimeSeconds<0||
           !std::isfinite(e.wholeTickTimeOfImpactSeconds)||e.wholeTickTimeOfImpactSeconds<0||
           e.solverEventId<0||e.solverIslandId<0||e.physicsContactOrdinal<0||
           !finitePoint(e.cueBallPositionAtContact)||!finitePoint(e.objectBallPositionAtContact)||
           !std::isfinite(e.normalImpulseNs)||e.normalImpulseNs<0.0||e.origin==nullptr||
           std::string(e.origin)!="AUTHORITATIVE_CONTINUOUS_PHYSICS_SOLVER_V1"||!e.complete||
           (i&&std::make_tuple(e.strokeRelativeTimeSeconds,e.solverEventId,
             e.physicsContactOrdinal,e.objectBallId)<previousKey)||
           !evidenceKeys.insert(std::make_tuple(e.physicsTick,e.solverEventId,
             e.physicsContactOrdinal,e.objectBallId)).second){r.error="d5b_order_contact";return r;}
        previous=e.strokeRelativeTimeSeconds;
        previousKey=std::make_tuple(e.strokeRelativeTimeSeconds,e.solverEventId,
            e.physicsContactOrdinal,e.objectBallId);
        bytes<<'|'<<e.objectBallId<<'|'<<e.strokeRelativeTimeSeconds<<'|'<<e.physicsTick<<'|'<<e.solverEventId<<'|'<<e.physicsContactOrdinal;
    }
    observation.allValuesFinite=true;observation.canonicalOrder=true;
    observation.complete=observation.d5AFullStrokeComplete&&
        observation.successfulPhysicsTicksContiguous&&observation.shotEndObserved;
    if(observation.complete){
        if(observation.contacts.empty())observation.order=CanonicalCueObjectTipOrder::NoCueObjectContact;
        else if(observation.contacts.front().strokeRelativeTimeSeconds<observation.tipSeparationTimeSeconds)observation.order=CanonicalCueObjectTipOrder::ContactBeforeTipSeparation;
        else if(observation.contacts.front().strokeRelativeTimeSeconds==observation.tipSeparationTimeSeconds)observation.order=CanonicalCueObjectTipOrder::ContactEqualTipSeparation;
        else observation.order=CanonicalCueObjectTipOrder::ContactAfterTipSeparation;
    }else observation.order=CanonicalCueObjectTipOrder::UnorderedIncomplete;
    bytes<<'|'<<canonicalCueObjectTipOrderName(observation.order)<<'|'<<observation.complete;
    observation.digest=d5bDigest(bytes.str());r.valid=true;r.complete=observation.complete;
    if(!r.complete)r.error="d5b_order_incomplete";return r;
}

CanonicalD5BTelemetryValidation validateCanonicalFrozenCushionShotObservation(
    CanonicalFrozenCushionShotObservation& observation)
{
    CanonicalD5BTelemetryValidation r;const auto& b=observation.binding;
    if(observation.schemaId!="canonical_frozen_cushion_shot_observation_v1"||
       observation.shotIdentity.empty()||b.declarationDecisionId.empty()||
       b.declaredObjectBallId<1||b.declaredObjectBallId>=kBallCount||
       b.namedStraightCushionFeatureId<0||b.namedStraightCushionFeatureId>3||
       b.declarationParentStateHash.empty()||b.consumedShotIdentity.empty()||
       b.consumedShotIdentity!=observation.shotIdentity||
       b.origin==nullptr||
       std::string(b.origin)!="SIGNED_FROZEN_CUSHION_DECLARATION_V1"||
       !b.authenticated||!b.committedAndConsumed||
       b.planeObservationId.empty()||!std::isfinite(b.contactPlaneCoordinateCm)||
       observation.physicsTickStart==0||
       (observation.physicsTickEnd&&
        observation.physicsTickEnd<observation.physicsTickStart)){
        r.error="d5b_frozen_identity";return r;}
    double previous=-1.0;bool departed=false;std::set<int> other,pocketed;
    std::set<std::tuple<std::string,std::uint64_t,int,int,int,int,
        std::uint64_t,double>> eventKeys;
    observation.sameNamedCushionRecontactAfterDeparture=false;
    std::ostringstream bytes;bytes<<observation.shotIdentity<<'|'<<b.declarationDecisionId;
    for(std::size_t i=0;i<observation.events.size();++i){const auto& e=observation.events[i];
        if(e.ballId<0||e.ballId>=kBallCount||!std::isfinite(e.strokeRelativeTimeSeconds)||
           e.strokeRelativeTimeSeconds<0||(i&&e.strokeRelativeTimeSeconds<previous)||
           e.kind==nullptr||e.origin==nullptr||!e.complete){r.error="d5b_frozen_event";return r;}
        previous=e.strokeRelativeTimeSeconds;const std::string kind=e.kind;
        const std::string origin=e.origin;
        if((kind!="DECLARED_OBJECT_DEPARTURE"&&
            kind!="DECLARED_OBJECT_NAMED_CUSHION_RECONTACT"&&
            kind!="DECLARED_OBJECT_OTHER_STRAIGHT_CUSHION"&&
            kind!="OTHER_OBJECT_STRAIGHT_CUSHION"&&
            kind!="OBJECT_POCKET_CAPTURE")||
           (kind=="DECLARED_OBJECT_DEPARTURE"
                ? origin!="AUTHORITATIVE_CONTINUOUS_SURFACE_INTEGRATOR"
                : origin!="AUTHORITATIVE_CONTINUOUS_PHYSICS_SOLVER_V1")){
            r.error="d5b_frozen_event_kind";return r;}
        if((kind=="DECLARED_OBJECT_DEPARTURE"&&
              (e.ballId!=b.declaredObjectBallId||
               e.straightCushionFeatureId!=-1||e.pocketId!=-1))||
           (kind=="DECLARED_OBJECT_NAMED_CUSHION_RECONTACT"&&
              (e.ballId!=b.declaredObjectBallId||
               e.straightCushionFeatureId!=b.namedStraightCushionFeatureId||
               e.solverEventId<0||e.physicsContactOrdinal<0))||
           (kind=="DECLARED_OBJECT_OTHER_STRAIGHT_CUSHION"&&
              (e.ballId!=b.declaredObjectBallId||
               e.straightCushionFeatureId<0||
               e.straightCushionFeatureId>3||
               e.straightCushionFeatureId==b.namedStraightCushionFeatureId))||
           (kind=="OTHER_OBJECT_STRAIGHT_CUSHION"&&
              (e.ballId<=0||e.ballId==b.declaredObjectBallId||
               e.straightCushionFeatureId<0||
               e.straightCushionFeatureId>3))||
           (kind=="OBJECT_POCKET_CAPTURE"&&
              (e.ballId<=0||e.pocketId<0||e.pocketCaptureSequence==0))||
           !eventKeys.insert(std::make_tuple(kind,e.physicsTick,e.solverEventId,
              e.physicsContactOrdinal,e.ballId,e.pocketId,
              e.pocketCaptureSequence,e.strokeRelativeTimeSeconds)).second){
            r.error="d5b_frozen_event_payload";return r;}
        if(kind=="DECLARED_OBJECT_DEPARTURE")departed=true;
        else if(kind=="DECLARED_OBJECT_NAMED_CUSHION_RECONTACT"&&departed)
            observation.sameNamedCushionRecontactAfterDeparture=true;
        else if(kind=="OTHER_OBJECT_STRAIGHT_CUSHION")other.insert(e.ballId);
        else if(kind=="OBJECT_POCKET_CAPTURE")pocketed.insert(e.ballId);
        bytes<<'|'<<kind<<'|'<<e.ballId<<'|'<<e.straightCushionFeatureId
             <<'|'<<e.pocketId<<'|'<<e.physicsTick<<'|'
             <<e.strokeRelativeTimeSeconds<<'|'<<e.solverEventId<<'|'
             <<e.physicsContactOrdinal<<'|'<<e.pocketCaptureSequence<<'|'
             <<origin<<'|'<<e.complete;
    }
    observation.departureObserved=departed;
    observation.otherObjectStraightCushionBalls.assign(other.begin(),other.end());
    observation.objectPocketedBalls.assign(pocketed.begin(),pocketed.end());
    observation.allValuesFinite=true;observation.canonicalOrder=true;
    observation.complete=observation.successfulPhysicsTicksContiguous&&observation.shotEndObserved;
    observation.digest=d5bDigest(bytes.str());r.valid=true;r.complete=observation.complete;
    if(!r.complete)r.error="d5b_frozen_incomplete";return r;
}

bool CanonicalD5BShotObserver::begin(const CanonicalCueObjectOrderObservation& identity,
    const CanonicalFrozenCushionBinding* binding)
{
    if(active_||committed_||identity.shotIdentity.empty()||
       identity.actorPlayerId<0||identity.actorPlayerId>1||
       identity.rackIdentity.empty()||identity.rulesStateHash.empty()||
       identity.physicsTickStart==0||
       (binding&&!validFrozenBindingForShot(*binding,
           identity.shotIdentity)))return false;
    cueObject_=identity;cueObject_.contacts.clear();cueObject_.complete=false;cueObject_.digest.clear();
    hasFrozen_=binding!=nullptr;if(binding){frozen_=CanonicalFrozenCushionShotObservation{};
        frozen_.shotIdentity=identity.shotIdentity;frozen_.binding=*binding;
        frozen_.physicsTickStart=identity.physicsTickStart;}
    lastPhysicsTick_=0;active_=true;return true;
}
bool CanonicalD5BShotObserver::bindTipObservation(const CanonicalCueTipStrokeObservation& tip)
{
    if(!active_||!tip.fullStrokeComplete||tip.shotIdentity!=cueObject_.shotIdentity)return false;
    cueObject_.d5AFullStrokeComplete=true;cueObject_.tipSeparationTimeSeconds=
        tip.intervals.empty()?0.0:tip.intervals.back().endTimeSeconds;return true;
}
bool CanonicalD5BShotObserver::bindFrozenCushion(
    const CanonicalFrozenCushionBinding& binding)
{
    if(!active_||committed_||hasFrozen_||
       !validFrozenBindingForShot(binding,cueObject_.shotIdentity))return false;
    hasFrozen_=true;frozen_=CanonicalFrozenCushionShotObservation{};
    frozen_.shotIdentity=cueObject_.shotIdentity;frozen_.binding=binding;
    frozen_.physicsTickStart=cueObject_.physicsTickStart;return true;
}
bool CanonicalD5BShotObserver::consumeSuccessfulPhysicsTick(
    const PhysicsStepTelemetry& telemetry,std::uint64_t tick,double tickStart)
{
    std::ostringstream tickBytes;tickBytes<<std::setprecision(
        std::numeric_limits<double>::max_digits10)<<tick<<'|'<<tickStart<<'|'
        <<telemetry.wholeTickDurationSeconds;
    for(const auto& c:telemetry.contacts)tickBytes<<'|'<<static_cast<int>(c.kind)
        <<'|'<<c.firstBall<<'|'<<c.secondBall<<'|'<<c.boundaryFeatureId<<'|'
        <<c.wholeTickTimeOfImpactSeconds<<'|'<<c.solverEventId<<'|'
        <<c.solverIslandId<<'|'<<c.normalImpulseNs<<'|'<<c.velocityImpulseApplied
        <<'|'<<static_cast<int>(c.pocketBoundaryEvent)<<'|'<<c.pocketCaptureSequence;
    for(const auto& c:telemetry.planeCrossings)tickBytes<<"|cross|"
        <<c.observationId<<'|'<<c.ballIndex<<'|'<<static_cast<int>(c.axis)
        <<'|'<<c.planeCoordinateCm<<'|'<<static_cast<int>(c.direction)<<'|'
        <<c.wholeTickTimeOfImpactSeconds<<'|'<<c.segmentOrdinal<<'|'
        <<c.signedDistanceBeforeCm<<'|'<<c.signedDistanceAfterCm<<'|'
        <<c.axisVelocityAtCrossingCmS<<'|'<<c.complete;
    for(const auto& a:telemetry.planeAttestations)tickBytes<<"|attest|"
        <<a.observationId<<'|'<<a.ballIndex<<'|'<<a.wholeTickDurationSeconds
        <<'|'<<a.coveredSegmentCount<<'|'<<a.crossingRecordCount<<'|'
        <<a.complete;
    const std::string tickDigest=d5bDigest(tickBytes.str());
    const auto retained=physicsTickDigests_.find(tick);
    if(retained!=physicsTickDigests_.end())return retained->second==tickDigest;
    if(!active_||telemetry.stepStatus!=PhysicsStepStatus::Succeeded||
       tick<cueObject_.physicsTickStart||
       (!lastPhysicsTick_&&tick!=cueObject_.physicsTickStart)||
       (lastPhysicsTick_&&tick!=lastPhysicsTick_+1)||!std::isfinite(tickStart)){
        cueObject_.successfulPhysicsTicksContiguous=false;if(hasFrozen_)frozen_.successfulPhysicsTicksContiguous=false;return false;}
    if(!std::isfinite(telemetry.wholeTickDurationSeconds)||
       telemetry.wholeTickDurationSeconds<0.0){
        cueObject_.successfulPhysicsTicksContiguous=false;
        if(hasFrozen_)frozen_.successfulPhysicsTicksContiguous=false;return false;}
    for(const auto& c:telemetry.contacts){
        if(!std::isfinite(c.wholeTickTimeOfImpactSeconds)||
           c.wholeTickTimeOfImpactSeconds<0.0||
           c.wholeTickTimeOfImpactSeconds>telemetry.wholeTickDurationSeconds||
           (c.kind==PhysicsContactKind::BallBall&&
            (c.firstBall==0||c.secondBall==0)&&
            (c.solverEventId<0||c.solverIslandId<0))){
            cueObject_.successfulPhysicsTicksContiguous=false;
            if(hasFrozen_)frozen_.successfulPhysicsTicksContiguous=false;return false;}}
    if(hasFrozen_){
        int matchingAttestations=0;
        for(const auto& attestation:telemetry.planeAttestations)
            if(attestation.observationId==frozen_.binding.planeObservationId){
                ++matchingAttestations;
                if(attestation.ballIndex!=frozen_.binding.declaredObjectBallId||
                   !std::isfinite(attestation.wholeTickDurationSeconds)||
                   attestation.wholeTickDurationSeconds!=
                       telemetry.wholeTickDurationSeconds||
                   attestation.coveredSegmentCount<=0||
                   attestation.crossingRecordCount<0||!attestation.complete){
                    cueObject_.successfulPhysicsTicksContiguous=false;
                    frozen_.successfulPhysicsTicksContiguous=false;return false;}}
        if(matchingAttestations!=1){
            cueObject_.successfulPhysicsTicksContiguous=false;
            frozen_.successfulPhysicsTicksContiguous=false;return false;}
    }
    std::map<std::tuple<std::string,int,double,int>,std::string> crossingKeys;
    if(hasFrozen_)for(const auto& crossing:telemetry.planeCrossings)
        if(crossing.observationId==frozen_.binding.planeObservationId){
           const auto key=std::make_tuple(crossing.observationId,
               crossing.ballIndex,crossing.wholeTickTimeOfImpactSeconds,
               crossing.segmentOrdinal);
           std::ostringstream payload;payload<<std::setprecision(
               std::numeric_limits<double>::max_digits10)
               <<static_cast<int>(crossing.axis)<<'|'
               <<crossing.planeCoordinateCm<<'|'
               <<static_cast<int>(crossing.direction)<<'|'
               <<crossing.signedDistanceBeforeCm<<'|'
               <<crossing.signedDistanceAfterCm<<'|'
               <<crossing.axisVelocityAtCrossingCmS<<'|'
               <<crossing.complete;
           const auto retainedCrossing=crossingKeys.find(key);
           if(!std::isfinite(crossing.wholeTickTimeOfImpactSeconds)||
            crossing.wholeTickTimeOfImpactSeconds<0.0||
            crossing.wholeTickTimeOfImpactSeconds>
                telemetry.wholeTickDurationSeconds||
            !crossing.complete||
            (retainedCrossing!=crossingKeys.end()&&
             retainedCrossing->second!=payload.str())){
            cueObject_.successfulPhysicsTicksContiguous=false;
            frozen_.successfulPhysicsTicksContiguous=false;return false;}
           crossingKeys[key]=payload.str();}
    std::map<std::tuple<int,int,int,int>,std::string> contactKeys;
    for(std::size_t i=0;i<telemetry.contacts.size();++i){const auto& c=telemetry.contacts[i];
        const auto key=std::make_tuple(static_cast<int>(c.kind),c.solverEventId,
            c.firstBall,c.secondBall);
        std::ostringstream payload;payload<<std::setprecision(
            std::numeric_limits<double>::max_digits10)<<c.boundaryFeatureId<<'|'
            <<c.wholeTickTimeOfImpactSeconds<<'|'<<c.solverIslandId<<'|'
            <<c.normalImpulseNs<<'|'<<c.velocityImpulseApplied<<'|'
            <<static_cast<int>(c.pocketBoundaryEvent)<<'|'<<c.pocketCaptureSequence;
        const auto retainedKey=contactKeys.find(key);
        if(retainedKey!=contactKeys.end()&&retainedKey->second!=payload.str()){
            cueObject_.successfulPhysicsTicksContiguous=false;
            if(hasFrozen_)frozen_.successfulPhysicsTicksContiguous=false;return false;}
        contactKeys[key]=payload.str();}
    for(std::size_t i=0;i<telemetry.contacts.size();++i){const auto& c=telemetry.contacts[i];
        const double t=tickStart+c.wholeTickTimeOfImpactSeconds;
        if(c.kind==PhysicsContactKind::BallBall&&(c.firstBall==0||c.secondBall==0)){
            CanonicalCueObjectContactEvidence e;e.objectBallId=c.firstBall==0?c.secondBall:c.firstBall;
            e.strokeRelativeTimeSeconds=t;e.physicsTick=tick;e.wholeTickTimeOfImpactSeconds=c.wholeTickTimeOfImpactSeconds;
            e.solverEventId=c.solverEventId;e.solverIslandId=c.solverIslandId;e.physicsContactOrdinal=static_cast<int>(i);
            e.cueBallPositionAtContact=c.firstBall==0?c.firstPositionAtContactCm:c.secondPositionAtContactCm;
            e.objectBallPositionAtContact=c.firstBall==0?c.secondPositionAtContactCm:c.firstPositionAtContactCm;
            e.velocityImpulseApplied=c.velocityImpulseApplied;e.normalImpulseNs=c.normalImpulseNs;e.complete=true;
            const bool duplicate=std::any_of(cueObject_.contacts.begin(),
                cueObject_.contacts.end(),[&](const auto& x){return
                    x.physicsTick==e.physicsTick&&x.solverEventId==e.solverEventId&&
                    x.physicsContactOrdinal==e.physicsContactOrdinal&&
                    x.objectBallId==e.objectBallId;});
            if(!duplicate)cueObject_.contacts.push_back(e);
        }
        if(!hasFrozen_)continue;
        CanonicalFrozenCushionEvent e;e.ballId=c.firstBall;e.physicsTick=tick;e.strokeRelativeTimeSeconds=t;
        e.solverEventId=c.solverEventId;e.physicsContactOrdinal=static_cast<int>(i);e.complete=true;
        if(c.kind==PhysicsContactKind::Rail&&c.velocityImpulseApplied&&c.boundaryFeatureId>=0&&c.boundaryFeatureId<=3){
            if(c.solverEventId<0){cueObject_.successfulPhysicsTicksContiguous=false;
                frozen_.successfulPhysicsTicksContiguous=false;return false;}
            e.straightCushionFeatureId=c.boundaryFeatureId;
            if(c.firstBall==frozen_.binding.declaredObjectBallId&&c.boundaryFeatureId==frozen_.binding.namedStraightCushionFeatureId)e.kind="DECLARED_OBJECT_NAMED_CUSHION_RECONTACT";
            else if(c.firstBall==frozen_.binding.declaredObjectBallId)
                e.kind="DECLARED_OBJECT_OTHER_STRAIGHT_CUSHION";
            else if(c.firstBall>0&&c.firstBall!=frozen_.binding.declaredObjectBallId)e.kind="OTHER_OBJECT_STRAIGHT_CUSHION";
            else continue;
        }else if(c.kind==PhysicsContactKind::Pocket&&c.firstBall>0&&c.pocketBoundaryEvent==PocketBoundaryEventKind::Capture&&c.pocketCaptureSequence>0){e.kind="OBJECT_POCKET_CAPTURE";e.pocketId=c.pocketId;e.pocketCaptureSequence=c.pocketCaptureSequence;}
        else continue;
        const bool duplicate=std::any_of(frozen_.events.begin(),
            frozen_.events.end(),[&](const auto& x){return
                x.physicsTick==e.physicsTick&&x.solverEventId==e.solverEventId&&
                x.physicsContactOrdinal==e.physicsContactOrdinal&&
                x.ballId==e.ballId&&x.straightCushionFeatureId==
                    e.straightCushionFeatureId&&x.pocketId==e.pocketId&&
                x.pocketCaptureSequence==e.pocketCaptureSequence&&
                std::string(x.kind)==std::string(e.kind);});
        if(!duplicate)frozen_.events.push_back(e);
    }
    if(hasFrozen_)for(const auto& crossing:telemetry.planeCrossings)
        if(crossing.observationId==frozen_.binding.planeObservationId&&crossing.ballIndex==frozen_.binding.declaredObjectBallId&&crossing.complete){CanonicalFrozenCushionEvent e;e.ballId=crossing.ballIndex;e.physicsTick=tick;e.strokeRelativeTimeSeconds=tickStart+crossing.wholeTickTimeOfImpactSeconds;e.kind="DECLARED_OBJECT_DEPARTURE";e.origin=crossing.origin;e.complete=true;
            const bool duplicate=std::any_of(frozen_.events.begin(),
                frozen_.events.end(),[&](const auto& x){return
                    x.physicsTick==e.physicsTick&&x.ballId==e.ballId&&
                    x.strokeRelativeTimeSeconds==e.strokeRelativeTimeSeconds&&
                    std::string(x.kind)==std::string(e.kind);});
            if(!duplicate)frozen_.events.push_back(e);}
    physicsTickDigests_[tick]=tickDigest;lastPhysicsTick_=tick;
    cueObject_.physicsTickEnd=tick;if(hasFrozen_)frozen_.physicsTickEnd=tick;
    std::sort(cueObject_.contacts.begin(),cueObject_.contacts.end(),[](const auto& a,const auto& b){return std::tie(a.strokeRelativeTimeSeconds,a.solverEventId,a.physicsContactOrdinal,a.objectBallId)<std::tie(b.strokeRelativeTimeSeconds,b.solverEventId,b.physicsContactOrdinal,b.objectBallId);});
    if(hasFrozen_)std::sort(frozen_.events.begin(),frozen_.events.end(),[](const auto& a,const auto& b){return std::tie(a.strokeRelativeTimeSeconds,a.physicsTick,a.physicsContactOrdinal,a.ballId)<std::tie(b.strokeRelativeTimeSeconds,b.physicsTick,b.physicsContactOrdinal,b.ballId);});
    cueObject_.successfulPhysicsTicksContiguous=true;if(hasFrozen_)frozen_.successfulPhysicsTicksContiguous=true;return true;
}
CanonicalD5BTelemetryValidation CanonicalD5BShotObserver::finish(bool shotEnd)
{
    cueObject_.shotEndObserved=shotEnd;auto r=validateCanonicalCueObjectOrderObservation(cueObject_);
    if(hasFrozen_){frozen_.shotEndObserved=shotEnd;auto fr=validateCanonicalFrozenCushionShotObservation(frozen_);if(!fr.valid||!fr.complete)return fr;}
    if(r.valid&&r.complete){active_=false;committed_=true;}return r;
}
void CanonicalD5BShotObserver::rollback(){active_=false;committed_=false;hasFrozen_=false;lastPhysicsTick_=0;physicsTickDigests_.clear();cueObject_=CanonicalCueObjectOrderObservation{};frozen_=CanonicalFrozenCushionShotObservation{};}

const char* physicsStepStatusName(PhysicsStepStatus status)
{
    return status == PhysicsStepStatus::Succeeded ? "succeeded" : "failed";
}

const char* physicsFailureCodeName(PhysicsFailureCode code)
{
    switch (code) {
    case PhysicsFailureCode::None: return "none";
    case PhysicsFailureCode::EventBudget: return "event_budget";
    case PhysicsFailureCode::InvalidControls: return "invalid_controls";
    case PhysicsFailureCode::IslandLimit: return "island_limit";
    case PhysicsFailureCode::ResidualLimit: return "residual_limit";
    case PhysicsFailureCode::PenetrationLimit: return "penetration_limit";
    case PhysicsFailureCode::NonfiniteState: return "nonfinite_state";
    case PhysicsFailureCode::NonfiniteEnergy: return "nonfinite_energy";
    case PhysicsFailureCode::PassiveEnergyCreation:
        return "passive_energy_creation";
    case PhysicsFailureCode::ContradictoryTopology:
        return "contradictory_topology";
    }
    return "nonfinite_state";
}

Point3 translationalMomentumKgMps(const GameState& state, float ballMassKg)
{
    Point3 momentum;
    for (const BallState& ball : state.balls) {
        if (ball.pocketed) {
            continue;
        }
        momentum.x += static_cast<float>(ballMassKg * ball.velocity.x / kCentimetersPerMeter);
        momentum.y += static_cast<float>(ballMassKg * ball.velocity.y / kCentimetersPerMeter);
        momentum.z += static_cast<float>(ballMassKg * ball.velocity.z / kCentimetersPerMeter);
    }
    return momentum;
}

double translationalKineticEnergyJ(const GameState& state, float ballMassKg)
{
    double energy = 0.0;
    for (const BallState& ball : state.balls) {
        if (ball.pocketed) {
            continue;
        }
        const double vx = ball.velocity.x / kCentimetersPerMeter;
        const double vy = ball.velocity.y / kCentimetersPerMeter;
        const double vz = ball.velocity.z / kCentimetersPerMeter;
        energy += 0.5 * ballMassKg * (vx * vx + vy * vy + vz * vz);
    }
    return energy;
}

PhysicsFrame capturePhysicsFrame(std::uint64_t tick, double timeSeconds, float deltaSeconds,
    const GameState& before, const GameState& after, const PhysicsStepTelemetry& telemetry,
    const PhysicsProfile& profile, PhysicsBoundaryMode boundaryMode)
{
    PhysicsFrame frame;
    frame.tick = tick;
    frame.timeSeconds = timeSeconds;
    frame.deltaSeconds = deltaSeconds;
    frame.contacts = telemetry.contacts;
    frame.solverEvents = telemetry.solverEvents;
    for (const SurfaceMotionStep& surface : telemetry.surfaceMotion) {
        if (surface.before != surface.after ||
            surface.transitionTimeSeconds >= 0.0f) {
            frame.surfaceTransitions.push_back(surface);
        }
    }
    frame.maximumPenetrationCm = telemetry.maximumPenetrationCm;
    frame.stepStatus = telemetry.stepStatus;
    frame.failureCode = telemetry.failureCode;
    frame.failingEventId = telemetry.failingEventId;
    frame.failingIslandId = telemetry.failingIslandId;
    frame.linearMomentum = translationalMomentumKgMps(after, profile.ball.massKg);
    frame.translationalKineticEnergyJ =
        translationalKineticEnergyJ(after, profile.ball.massKg);
    for (const BallState& ball : after.balls) {
        if (!ball.pocketed) {
            frame.rotationalKineticEnergyJ +=
                rotationalKineticEnergyJ(ball, profile.ball);
        }
    }
    frame.totalKineticEnergyJ =
        frame.translationalKineticEnergyJ + frame.rotationalKineticEnergyJ;
    frame.physicsProfileId = profile.id;
    frame.boundaryMode = boundaryMode;
    frame.control.aimYaw = after.aim.yaw;
    frame.control.shotPower = after.input.shotPower;
    frame.control.shotTaken = after.players.shotTaken;

    for (int index = 0; index < kBallCount; ++index) {
        const BallState& beforeBall = before.balls[index];
        const BallState& afterBall = after.balls[index];
        PhysicsBallSample& sample = frame.balls[index];
        sample.position = afterBall.position;
        sample.velocity = afterBall.velocity;
        sample.acceleration = accelerationBetween(
            beforeBall.velocity, afterBall.velocity, deltaSeconds);
        sample.angularVelocity = afterBall.angularVelocity;
        sample.speed = afterBall.speed;
        sample.pocketed = afterBall.pocketed;
        sample.outOfPlay = afterBall.outOfPlay;
        sample.pocketId = afterBall.pocketInteraction.pocketId;
        sample.pocketPhase = afterBall.pocketInteraction.phase;
        sample.pocketCaptureSequence =
            afterBall.pocketInteraction.captureSequence;
        sample.motionState = afterBall.motionState;
        const Point3 slip = surfaceContactSlipVelocity(
            afterBall, profile.ball.radiusCm);
        sample.contactSlipSpeedCmS = std::sqrt(
            slip.x * slip.x + slip.z * slip.z);
        sample.rotationalKineticEnergyJ =
            rotationalKineticEnergyJ(afterBall, profile.ball);
    }
    return frame;
}

PhysicsFrame capturePhysicsFrame(std::uint64_t tick, double timeSeconds, float deltaSeconds,
    const GameState& before, const GameState& after, const PhysicsStepTelemetry& telemetry)
{
    return capturePhysicsFrame(
        tick, timeSeconds, deltaSeconds, before, after, telemetry,
        defaultChinesePoolPhysicsProfile());
}

void PhysicsTrace::clear()
{
    frames_.clear();
    droppedFrames_ = 0;
}

void PhysicsTrace::append(const PhysicsFrame& frame)
{
    if (capacity_ == 0) {
        ++droppedFrames_;
        return;
    }
    if (frames_.size() == capacity_) {
        frames_.pop_front();
        ++droppedFrames_;
    }
    frames_.push_back(frame);
}

}  // namespace billiardgl
