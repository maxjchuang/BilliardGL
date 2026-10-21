"""Execute the actual private lineage consumer against valid and corrupt records."""
from pathlib import Path
import shutil
import subprocess
import tempfile
import unittest

ROOT = Path(__file__).resolve().parents[1]
HARNESS = r'''
int main() {
    PhysicsProfile p;
    GameState before;
    for (auto& b : before.balls) b.pocketed = true;
    before.balls[0].pocketed = false;
    before.balls[0].physicalDomain = PhysicalBallDomain::ClothSupported;
    GameState after = before;
    TableDepartureStepResult good;
    good.ok = true;
    good.failureCode = TableDepartureFailureCode::None;
    SurfaceMotionAnalyticSegment s;
    s.ballIndex = 0; s.segmentOrdinal = 0;
    s.motionState = BallMotionState::Stationary;
    s.durationSeconds = 0.1; s.analyticPathComplete = true;
    good.analyticSegments.push_back(s);
    int passed = 0;
    auto expect = [&](bool value, const char* name) {
        if (!value) { std::cerr << "FAIL " << name << '\n'; std::exit(1); }
        ++passed;
    };
    expect(verifyPrivateAnalyticLineage(good,before,after,0.1,p),"stationary valid");
    auto reject = [&](const char* name, auto mutate) {
        auto bad=good; auto b=before; auto a=after;
        mutate(bad,b,a);
        expect(!verifyPrivateAnalyticLineage(bad,b,a,0.1,p),name);
    };
    reject("missing",[](auto& r,auto&,auto&){r.analyticSegments.clear();});
    reject("incomplete",[](auto& r,auto&,auto&){r.analyticSegments[0].analyticPathComplete=false;});
    reject("ordinal",[](auto& r,auto&,auto&){r.analyticSegments[0].segmentOrdinal=1;});
    reject("foreign ball",[](auto& r,auto&,auto&){r.analyticSegments[0].ballIndex=1;});
    reject("negative ball",[](auto& r,auto&,auto&){r.analyticSegments[0].ballIndex=-1;});
    reject("gap",[](auto& r,auto&,auto&){r.analyticSegments[0].wholeTickStartOffsetSeconds=.01;});
    reject("short coverage",[](auto& r,auto&,auto&){r.analyticSegments[0].durationSeconds=.09;});
    reject("long coverage",[](auto& r,auto&,auto&){r.analyticSegments[0].durationSeconds=.11;});
    reject("nan",[](auto& r,auto&,auto&){r.analyticSegments[0].durationSeconds=std::numeric_limits<double>::quiet_NaN();});
    reject("translated",[](auto& r,auto&,auto&){r.analyticSegments[0].startPositionCm.x=1;r.analyticSegments[0].endPositionCm.x=1;});
    reject("end anchor",[](auto&,auto&,auto& a){a.balls[0].position.x=1;});
    reject("start velocity",[](auto&,auto& b,auto&){b.balls[0].velocity.x=1;});
    reject("end velocity",[](auto&,auto&,auto& a){a.balls[0].velocity.x=1;});
    reject("motion state",[](auto& r,auto&,auto&){r.analyticSegments[0].motionState=BallMotionState::Rolling;});
    reject("duplicate",[](auto& r,auto&,auto&){r.analyticSegments.push_back(r.analyticSegments[0]);});
    reject("inactive channel",[](auto&,auto& b,auto&){b.balls[0].physicalDomain=PhysicalBallDomain::PocketChannel;});
    reject("failed result",[](auto& r,auto&,auto&){r.ok=false;});
    reject("forged event",[](auto& r,auto&,auto& a){
        TableDepartureEvent e;e.kind=TableDepartureEventKind::CrossDomainBallContact;
        e.ballIndex=0;e.ordinal=0;e.absoluteWholeTickTimeSeconds=.1;
        e.stableIdentity="cross_domain_ball_contact:0:1:0:0.1";
        r.events.push_back(e);a.balls[0].velocity.x=1;
    });
    {
        auto r=good; auto b=before; auto a=after;
        b.balls[0].physicalDomain=PhysicalBallDomain::Airborne;
        b.balls[0].position.y=110; b.balls[0].velocity.y=20;
        auto& seg=r.analyticSegments[0];
        seg.motionState=BallMotionState::Sliding;
        seg.startPositionCm=b.balls[0].position;
        seg.startVelocityCmS=b.balls[0].velocity;
        seg.constantAccelerationCmS2.y=p.tableDeparture.gravityYCmS2;
        a=b;
        a.balls[0].position.y=static_cast<float>(110.0+20.0*.1+
            .5*double(p.tableDeparture.gravityYCmS2)*.1*.1);
        a.balls[0].velocity.y=static_cast<float>(20.0+
            double(p.tableDeparture.gravityYCmS2)*.1);
        seg.endPositionCm=a.balls[0].position;
        expect(verifyPrivateAnalyticLineage(r,b,a,.1,p),"ballistic valid");
    }
    {
        auto r=good; auto b=before; auto a=after;
        b.balls[0].velocity.x=20;
        auto& seg=r.analyticSegments[0];
        seg.motionState=BallMotionState::Rolling;
        seg.startVelocityCmS=b.balls[0].velocity;
        seg.constantAccelerationCmS2.x=-p.tableDeparture.supportedRollingDecelerationCmS2;
        a=b;
        a.balls[0].position.x=static_cast<float>(20.0*.1+
            .5*double(seg.constantAccelerationCmS2.x)*.1*.1);
        a.balls[0].velocity.x=static_cast<float>(20.0+
            double(seg.constantAccelerationCmS2.x)*.1);
        seg.endPositionCm=a.balls[0].position;
        expect(verifyPrivateAnalyticLineage(r,b,a,.1,p),"rolling valid");
    }
    TableDepartureStepResult zero;zero.ok=true;
    expect(verifyPrivateAnalyticLineage(zero,before,before,0,p),"zero valid");
    auto mutated=before;mutated.balls[0].angularVelocity.x=1;
    expect(!verifyPrivateAnalyticLineage(zero,before,mutated,0,p),"zero spin mutation");
    PhysicsStepTelemetry combined;
    combined.wholeTickDurationSeconds=.1;
    combined.analyticSurfaceMotion=good.analyticSegments;
    expect(verifyComposedAnalyticPartition(combined,before,after,p),"combined valid");
    {
        auto r=combined; auto a=after;
        r.analyticSurfaceMotion[0].durationSeconds=.05;
        auto next=r.analyticSurfaceMotion[0];
        next.segmentOrdinal=1; next.wholeTickStartOffsetSeconds=.05;
        next.trajectoryGeneration=1;
        next.startPositionCm.x=20; next.endPositionCm.x=20;
        r.analyticSurfaceMotion.push_back(next);
        a.balls[0].position.x=20; a.balls[0].trajectoryGeneration=1;
        PhysicsContactRecord c;
        c.kind=PhysicsContactKind::Pocket; c.firstBall=0;
        c.pocketBoundaryEvent=PocketBoundaryEventKind::Capture;
        c.pocketPhaseAfter=PocketInteractionPhase::Captured;
        c.pocketPassable=true; c.pocketId=2; c.pocketCaptureSequence=1;
        c.wholeTickTimeOfImpactSeconds=.05;
        c.administrativeResetAfterCapture=true;
        c.trajectoryGenerationAfter=1; c.administrativeResetPositionCm.x=20;
        r.contacts.push_back(c);
        expect(verifyComposedAnalyticPartition(r,before,a,p),"capture reset starts new physical generation");
        auto rejectReset=[&](const char* name,auto mutate) {
            auto bad=r; auto badAfter=a; mutate(bad,badAfter);
            expect(!verifyComposedAnalyticPartition(bad,before,badAfter,p),name);
        };
        rejectReset("reset requires capture receipt",[](auto& x,auto&){x.contacts.clear();});
        rejectReset("reset generation cannot skip",[](auto& x,auto&){x.contacts[0].trajectoryGenerationAfter=2;});
        rejectReset("reset capture time must match",[](auto& x,auto&){x.contacts[0].wholeTickTimeOfImpactSeconds=.04;});
        rejectReset("reset origin must match",[](auto& x,auto&){x.contacts[0].firstPositionAtContactCm.x=1;});
        rejectReset("reset destination must match",[](auto& x,auto&){x.contacts[0].administrativeResetPositionCm.x=21;});
        rejectReset("reset requires complete capture",[](auto& x,auto&){x.contacts[0].pocketPassable=false;});
        rejectReset("reset must be consumed once",[](auto& x,auto&){x.contacts.push_back(x.contacts[0]);});
        rejectReset("old generation cannot continue after reset",[](auto& x,auto&){x.analyticSurfaceMotion[1].trajectoryGeneration=0;});
        rejectReset("final generation must match",[](auto&,auto& x){x.balls[0].trajectoryGeneration=0;});
        r.analyticSurfaceMotion.resize(1);
        r.analyticSurfaceMotion[0].durationSeconds=.1;
        r.contacts[0].wholeTickTimeOfImpactSeconds=.1;
        expect(verifyComposedAnalyticPartition(r,before,a,p),"terminal capture reset has no invented tail");
    }
    {
        auto r=combined; auto a=after;
        r.analyticSurfaceMotion[0].startPositionCm.x=.05f;
        r.analyticSurfaceMotion[0].endPositionCm.x=.05f;
        a.balls[0].position.x=.05f;
        PhysicsContactRecord contact;
        contact.kind=PhysicsContactKind::Rail; contact.firstBall=0;
        contact.solverEventId=0; contact.solverIslandId=0;
        contact.solverProjectionCm=.05; contact.positionCorrected=true;
        contact.firstPositionCorrectionCm.x=.05f;
        r.contacts.push_back(contact);
        SolverEventRecord event;
        event.eventId=0; event.islandId=0; event.contactCount=1;
        event.positionIterations=1;
        SolverPositionProjection projection;
        projection.ordinal=0; projection.iteration=0;
        projection.contactIndex=0; projection.ballIndex=0;
        projection.afterCm.x=.05f;
        event.positionProjections.push_back(projection);
        r.solverEvents.push_back(event);
        expect(verifyComposedAnalyticPartition(r,before,a,p),"actual t0 solver projection");
        auto rejectProjection=[&](const char* name,auto mutate) {
            auto bad=r; mutate(bad);
            expect(!verifyComposedAnalyticPartition(bad,before,a,p),name);
        };
        rejectProjection("missing projection",[](auto& x){x.solverEvents.clear();});
        rejectProjection("duplicate projection",[](auto& x){x.solverEvents.push_back(x.solverEvents[0]);});
        rejectProjection("wrong projection direction",[](auto& x){x.solverEvents[0].positionProjections[0].afterCm.x=-.05f;});
        rejectProjection("foreign projection ball",[](auto& x){x.solverEvents[0].positionProjections[0].ballIndex=1;});
        rejectProjection("wrong projection time",[](auto& x){x.solverEvents[0].wholeTickTimeOfImpactSeconds=.05;});
        rejectProjection("wrong projection island",[](auto& x){x.solverEvents[0].islandId=1;});
        rejectProjection("failed projection solver",[](auto& x){x.solverEvents[0].failureCode="iteration_limit";});
        rejectProjection("unmarked projection",[](auto& x){x.contacts[0].positionCorrected=false;});
        rejectProjection("wrong projection origin",[](auto& x){x.solverEvents[0].positionProjections[0].beforeCm.x=.02f;});
        rejectProjection("unbounded projection iteration",[](auto& x){x.solverEvents[0].positionProjections[0].iteration=1;});
        rejectProjection("no-op projection",[](auto& x){x.solverEvents[0].positionProjections[0].afterCm={};});
        rejectProjection("sub-slop projection origin",[](auto& x){x.solverEvents[0].positionProjections[0].beforeCm.x=.00001f;});
        rejectProjection("sub-slop projection destination",[](auto& x){x.solverEvents[0].positionProjections[0].afterCm.x+=.00001f;});
        rejectProjection("wrong projection contact",[](auto& x){x.solverEvents[0].positionProjections[0].contactIndex=1;});
        rejectProjection("wrong projection ordinal",[](auto& x){x.solverEvents[0].positionProjections[0].ordinal=1;});
        rejectProjection("projection ledger drift",[](auto& x){x.contacts[0].firstPositionCorrectionCm.x+=.00001f;});
        rejectProjection("extra projection",[](auto& x){x.solverEvents[0].positionProjections.push_back(x.solverEvents[0].positionProjections[0]);});
        auto& receipts=r.solverEvents[0].positionProjections;
        receipts[0].afterCm.x=.02f;
        auto second=receipts[0]; second.ordinal=1; second.iteration=1;
        second.beforeCm.x=.02f; second.afterCm.x=.05f;
        receipts.push_back(second); r.solverEvents[0].positionIterations=2;
        expect(verifyComposedAnalyticPartition(r,before,a,p),"ordered multi-iteration projection");
        rejectProjection("dropped projection chain",[](auto& x){x.solverEvents[0].positionProjections.erase(x.solverEvents[0].positionProjections.begin());});
        rejectProjection("reordered projection chain",[](auto& x){std::reverse(x.solverEvents[0].positionProjections.begin(),x.solverEvents[0].positionProjections.end());});
        auto secondContact=r.contacts[0];
        secondContact.firstPositionCorrectionCm.x=.05f-.02f;
        r.contacts[0].firstPositionCorrectionCm.x=.02f;
        r.contacts.push_back(secondContact);
        receipts[1].iteration=0; receipts[1].contactIndex=1;
        r.solverEvents[0].contactCount=2;
        expect(verifyComposedAnalyticPartition(r,before,a,p),"same ball two-contact exact projection chain");
        auto small=combined; auto smallAfter=after;
        small.analyticSurfaceMotion[0].startPositionCm.x=.00001f;
        small.analyticSurfaceMotion[0].endPositionCm.x=.00001f;
        smallAfter.balls[0].position.x=.00001f;
        contact.firstPositionCorrectionCm.x=.00001f;
        small.contacts.push_back(contact);
        expect(!verifyComposedAnalyticPartition(small,before,smallAfter,p),"sub-slop missing projection event");
        event.positionProjections.clear(); small.solverEvents.push_back(event);
        expect(!verifyComposedAnalyticPartition(small,before,smallAfter,p),"sub-slop dropped whole projection receipt bundle");
    }
    auto rejectCombined = [&](const char* name, auto mutate) {
        auto r=combined; auto b=before; auto a=after;
        mutate(r,b,a);
        expect(!verifyComposedAnalyticPartition(r,b,a,p),name);
    };
    rejectCombined("combined missing",[](auto& r,auto&,auto&){r.analyticSurfaceMotion.clear();});
    rejectCombined("combined duplicate",[](auto& r,auto&,auto&){r.analyticSurfaceMotion.push_back(r.analyticSurfaceMotion[0]);});
    rejectCombined("combined gap",[](auto& r,auto&,auto&){r.analyticSurfaceMotion[0].wholeTickStartOffsetSeconds=.01;});
    rejectCombined("combined short",[](auto& r,auto&,auto&){r.analyticSurfaceMotion[0].durationSeconds=.09;});
    rejectCombined("combined ordinal",[](auto& r,auto&,auto&){r.analyticSurfaceMotion[0].segmentOrdinal=1;});
    rejectCombined("combined translated",[](auto& r,auto&,auto&){r.analyticSurfaceMotion[0].startPositionCm.x=1;r.analyticSurfaceMotion[0].endPositionCm.x=1;});
    rejectCombined("combined final",[](auto&,auto&,auto& a){a.balls[0].position.x=1;});
    rejectCombined("combined inactive",[](auto&,auto& b,auto&){b.balls[0].physicalDomain=PhysicalBallDomain::PocketChannel;});
    rejectCombined("combined nan duration",[](auto& r,auto&,auto&){r.wholeTickDurationSeconds=std::numeric_limits<double>::quiet_NaN();});
    rejectCombined("combined incomplete",[](auto& r,auto&,auto&){r.analyticSurfaceMotion[0].analyticPathComplete=false;});
    {
        auto b=before;
        b.balls[1]=before.balls[0]; b.balls[0].pocketed=true;
        auto a=b;
        a.balls[1].pocketed=true;
        a.balls[1].physicalDomain=PhysicalBallDomain::PocketChannel;
        a.balls[1].pocketInteraction.phase=PocketInteractionPhase::Captured;
        a.balls[1].pocketInteraction.pocketId=2;
        a.balls[1].pocketInteraction.captureSequence=1;
        a.balls[1].position={10,-100,30};
        auto captured=combined;
        captured.analyticSurfaceMotion[0].ballIndex=1;
        PhysicsContactRecord contact;
        contact.kind=PhysicsContactKind::Pocket; contact.firstBall=1;
        contact.pocketBoundaryEvent=PocketBoundaryEventKind::Capture;
        contact.pocketPhaseAfter=PocketInteractionPhase::Captured;
        contact.pocketPassable=true; contact.pocketId=2;
        contact.pocketCaptureSequence=1;
        contact.wholeTickTimeOfImpactSeconds=.1;
        captured.contacts.push_back(contact);
        expect(verifyComposedAnalyticPartition(captured,b,a,p),"capture display relocation valid");
        auto bad=captured; bad.contacts.clear();
        expect(!verifyComposedAnalyticPartition(bad,b,a,p),"capture requires event");
        bad=captured; bad.contacts.push_back(contact);
        expect(!verifyComposedAnalyticPartition(bad,b,a,p),"capture duplicate event");
        bad=captured; bad.contacts[0].wholeTickTimeOfImpactSeconds=.09;
        expect(!verifyComposedAnalyticPartition(bad,b,a,p),"capture exact time");
        bad=captured; bad.contacts[0].firstPositionAtContactCm.x=1;
        expect(!verifyComposedAnalyticPartition(bad,b,a,p),"capture exact position");
        bad=captured; bad.contacts[0].pocketCaptureSequence=2;
        expect(!verifyComposedAnalyticPartition(bad,b,a,p),"capture exact sequence");
    }
    {
        PhysicsPlaneObservationRequest request;
        request.observationId="arc"; request.ballIndex=0;
        request.axis=PhysicsPlaneAxis::Y; request.planeCoordinateCm=0;
        request.direction=PhysicsPlaneCrossingDirection::NegativeToPositive;
        SurfaceMotionAnalyticSegment arc=s;
        arc.startPositionCm.y=-1; arc.endPositionCm.y=-1;
        arc.startVelocityCmS.y=4; arc.constantAccelerationCmS2.y=-4;
        arc.durationSeconds=2;
        PhysicsPlaneCrossingRecord crossing;
        expect(crossingForSegment(request,arc,crossing) &&
            std::fabs(crossing.wholeTickTimeOfImpactSeconds-(1-std::sqrt(.5)))<1e-12 &&
            crossing.signedDistanceBeforeCm<0 && crossing.signedDistanceAfterCm>0,
            "same-side arc upward crossing");
        request.direction=PhysicsPlaneCrossingDirection::PositiveToNegative;
        expect(crossingForSegment(request,arc,crossing) &&
            std::fabs(crossing.wholeTickTimeOfImpactSeconds-(1+std::sqrt(.5)))<1e-12,
            "same-side arc downward crossing");
        arc.startPositionCm.y=1; arc.endPositionCm.y=1;
        arc.startVelocityCmS.y=-4; arc.constantAccelerationCmS2.y=4;
        expect(crossingForSegment(request,arc,crossing) &&
            std::fabs(crossing.wholeTickTimeOfImpactSeconds-(1-std::sqrt(.5)))<1e-12,
            "same-side dip downward crossing");
        request.direction=PhysicsPlaneCrossingDirection::NegativeToPositive;
        expect(crossingForSegment(request,arc,crossing) &&
            std::fabs(crossing.wholeTickTimeOfImpactSeconds-(1+std::sqrt(.5)))<1e-12,
            "same-side dip upward crossing");
        arc.startPositionCm.y=-1; arc.endPositionCm.y=0;
        arc.startVelocityCmS.y=1; arc.constantAccelerationCmS2.y=0;
        arc.durationSeconds=1;
        expect(crossingForSegment(request,arc,crossing,true) &&
            crossing.wholeTickTimeOfImpactSeconds==1 && crossing.signedDistanceAfterCm==0,
            "final endpoint included");
        expect(!crossingForSegment(request,arc,crossing,false),"internal endpoint excluded");
        arc.startVelocityCmS.y=2; arc.constantAccelerationCmS2.y=-2;
        arc.durationSeconds=2; arc.endPositionCm.y=-1;
        expect(!crossingForSegment(request,arc,crossing,true),"tangent is not crossing");
        arc.startPositionCm.y=-3; arc.endPositionCm.y=-3;
        expect(!crossingForSegment(request,arc,crossing,true),"arc below plane has no crossing");
        arc.startPositionCm.y=-1; arc.startVelocityCmS.y=1.3f;
        arc.constantAccelerationCmS2.y=.7f; arc.durationSeconds=1.0/3.0;
        const double timeTolerance=p.solver.toiToleranceSeconds;
        const double positionTolerance=p.solver.penetrationSlopCm;
        const auto positionAt = [&](double t) {
            return double(arc.startPositionCm.y)+double(arc.startVelocityCmS.y)*t+
                .5*double(arc.constantAccelerationCmS2.y)*t*t;
        };
        request.planeCoordinateCm=positionAt(arc.durationSeconds)+1e-10;
        expect(crossingForSegment(request,arc,crossing,true,timeTolerance,positionTolerance) &&
            crossing.wholeTickTimeOfImpactSeconds==arc.durationSeconds,
            "nontrivial quadratic endpoint uses sealed tolerances");
        request.planeCoordinateCm=positionAt(arc.durationSeconds+1.01*timeTolerance);
        expect(!crossingForSegment(request,arc,crossing,true,timeTolerance,positionTolerance),
            "root beyond final time tolerance rejected");
        request.planeCoordinateCm=positionAt(arc.durationSeconds-1.01*timeTolerance);
        expect(crossingForSegment(request,arc,crossing,true,timeTolerance,positionTolerance) &&
            crossing.wholeTickTimeOfImpactSeconds<arc.durationSeconds,
            "interior root retains its actual time");
        arc.startVelocityCmS.y=1000000;
        request.planeCoordinateCm=positionAt(arc.durationSeconds)+.99*positionTolerance;
        expect(crossingForSegment(request,arc,crossing,true,timeTolerance,positionTolerance),
            "endpoint within both tolerance bounds accepted");
        expect(!crossingForSegment(request,arc,crossing,false,timeTolerance,positionTolerance),
            "internal endpoint never gains final tolerance exception");
        request.planeCoordinateCm=positionAt(arc.durationSeconds)+1.01*positionTolerance;
        expect(!crossingForSegment(request,arc,crossing,true,timeTolerance,positionTolerance),
            "endpoint beyond position tolerance rejected");
    }
    for (int landing=0; landing<3; ++landing) {
        auto b=before; b.balls[0].physicalDomain=PhysicalBallDomain::Airborne;
        auto r=good; auto& seg=r.analyticSegments[0];
        seg.motionState=BallMotionState::Sliding;
        seg.constantAccelerationCmS2.y=p.tableDeparture.gravityYCmS2;
        seg.endPositionCm.y=static_cast<float>(.5*double(p.tableDeparture.gravityYCmS2)*.1*.1);
        auto a=b; a.balls[0].position=seg.endPositionCm;
        TableDepartureEvent event;
        const char* names[]={"cloth_return","rail_top","outside_landing"};
        const TableDepartureEventKind kinds[]={TableDepartureEventKind::ClothReturn,
            TableDepartureEventKind::RailTopLanding,TableDepartureEventKind::OutsideLanding};
        const PhysicalBallDomain domains[]={PhysicalBallDomain::ClothSupported,
            PhysicalBallDomain::RailTopSupported,PhysicalBallDomain::OutsideLandingSupported};
        event.kind=kinds[landing]; event.ballIndex=0; event.ordinal=0;
        event.absoluteWholeTickTimeSeconds=.1;
        event.stableIdentity=std::string("0:0:")+names[landing]+":0.1";
        event.transitionWitnessComplete=true; event.targetDomain=domains[landing];
        a.balls[0].physicalDomain=event.targetDomain;
        r.events.push_back(event);
        expect(verifyPrivateAnalyticLineage(r,b,a,.1,p),"terminal landing receipt binds post-state");
        auto badAfter=a; badAfter.balls[0].velocity.x=1;
        expect(!verifyPrivateAnalyticLineage(r,b,badAfter,.1,p),"terminal landing velocity mutation");
        badAfter=a; badAfter.balls[0].physicalDomain=PhysicalBallDomain::Airborne;
        expect(!verifyPrivateAnalyticLineage(r,b,badAfter,.1,p),"terminal landing domain mutation");
        auto bad=r; bad.events[0].transitionWitnessComplete=false;
        expect(!verifyPrivateAnalyticLineage(bad,b,a,.1,p),"terminal witness missing");
        bad=r; bad.events[0].velocityAfterCmS.y=1;
        badAfter=a; badAfter.balls[0].velocity.y=1;
        expect(!verifyPrivateAnalyticLineage(bad,b,badAfter,.1,p),"supported reaction law rejects forged witness");
    }
    std::cout << "actual_private_consumer_checks=" << passed << " PASS\n";
}


'''
INCLUDES = r'''
#include "table_departure_geometry.h"
#include "physics_telemetry.h"
#include <algorithm>
#include <array>
#include <cmath>
#include <tuple>
#include <sstream>
#include <iostream>
#include <cstdlib>
#include <limits>
using namespace billiardgl;
'''

class PrivateLineageConsumerRuntimeTests(unittest.TestCase):
    def test_actual_consumer_rejects_corruption_at_both_optimization_levels(self):
        compiler = shutil.which("c++")
        self.assertIsNotNone(compiler, "C++ compiler is required")
        source = (ROOT / "src/Billiards/physics.cpp").read_text()
        start = source.index("bool verifyPrivateAnalyticLineage(")
        end = source.index("void appendRebasedPrivateAnalyticSegments(", start)
        actual = source[start:end]
        partition_start = source.index("bool verifyComposedAnalyticPartition(")
        partition_end = source.index("void mergeComposedState(", partition_start)
        actual += source[partition_start:partition_end]
        axis_start = source.index("double planeAxisValue(")
        axis_end = source.index("bool validPlaneObservationRequest(", axis_start)
        actual += source[axis_start:axis_end]
        crossing_start = source.index("bool crossingForSegment(")
        crossing_end = source.index("void appendPlaneObservationEvidence(", crossing_start)
        actual += source[crossing_start:crossing_end]
        with tempfile.TemporaryDirectory(prefix="billiardgl-lineage-consumer-") as work:
            path = Path(work)
            fixture = path / "consumer.cpp"
            fixture.write_text(INCLUDES + actual + HARNESS)
            for optimization in ("-O0", "-O2"):
                with self.subTest(optimization=optimization):
                    binary = path / ("consumer" + optimization)
                    subprocess.run([compiler, "-std=c++17", optimization,
                        "-I" + str(ROOT / "src/Billiards"), str(fixture),
                        "-o", str(binary)], check=True, capture_output=True, text=True)
                    result = subprocess.run([str(binary)], check=False,
                        capture_output=True, text=True)
                    self.assertEqual(result.returncode, 0, result.stdout + result.stderr)
                    self.assertEqual(result.stdout.strip(),
                        "actual_private_consumer_checks=104 PASS")

if __name__ == "__main__":
    unittest.main()
