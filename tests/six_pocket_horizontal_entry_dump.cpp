#include "support/six_pocket_horizontal_entry_fixture.h"
#include "automation_json.h"
#include <cstdio>
#include <iostream>
#include <memory>

using namespace billiardgl;
using namespace billiardgl::json;
namespace {
Value point(const Point3& p){return Value(Value::Array{Value(double(p.x)),Value(double(p.y)),Value(double(p.z))});}
Value contact(const PhysicsContactRecord& c) {
    Value::Object o;
    o["kind"]=Value(int(c.kind));o["first_ball"]=Value(c.firstBall);o["second_ball"]=Value(c.secondBall);
    o["pocket_id"]=Value(c.pocketId);o["pocket_event"]=Value(std::string(pocketBoundaryEventKindName(c.pocketBoundaryEvent)));
    o["solver_event_id"]=Value(c.solverEventId);o["solver_island_id"]=Value(c.solverIslandId);
    o["boundary_feature_id"]=Value(c.boundaryFeatureId);o["whole_tick_toi_seconds"]=Value(c.wholeTickTimeOfImpactSeconds);
    o["position_at_contact_cm"]=point(c.firstPositionAtContactCm);o["normal"]=point(c.normal);
    o["normal_impulse_ns"]=Value(c.normalImpulseNs);o["velocity_impulse_applied"]=Value(c.velocityImpulseApplied);
    o["vertical_shaft"]=Value(c.pocketVerticalShaft);o["capture_sequence"]=Value(double(c.pocketCaptureSequence));
    o["generation_before"]=Value(double(c.trajectoryGenerationBefore));o["generation_after"]=Value(double(c.trajectoryGenerationAfter));
    o["phase_before"]=Value(int(c.pocketPhaseBefore));o["phase_after"]=Value(int(c.pocketPhaseAfter));
    o["pocket_depth_cm"]=Value(c.pocketLocal.depthCm);o["pocket_offset_cm"]=Value(c.pocketLocal.offsetCm);
    o["administrative_reset"]=Value(c.administrativeResetAfterCapture);
    o["administrative_reset_position_cm"]=point(c.administrativeResetPositionCm);
    return Value(o);
}
Value step(const test::HorizontalPocketStep& s) {
    const auto& b=s.state.balls[1];const auto& p=s.presentation.balls[1];
    Value::Object o;
    o["tick"]=Value(s.tick);o["attempt_time_seconds"]=Value(s.tick/120.0);
    o["position_cm"]=point(b.position);o["velocity_cm_s"]=point(b.velocity);o["angular_velocity_rad_s"]=point(b.angularVelocity);
    o["presentation_position_cm"]=point({p.position.x,p.position.y,p.position.z});
    o["visible"]=Value(p.visible);o["casts_shadow"]=Value(p.castsShadow);o["pocketed"]=Value(b.pocketed);
    o["physical_domain"]=Value(int(b.physicalDomain));o["pocket_phase"]=Value(int(b.pocketInteraction.phase));
    o["trajectory_generation"]=Value(double(b.trajectoryGeneration));o["balls_moving"]=Value(s.state.ballsMoving);
    o["entry_event"]=Value(s.entry);o["throat_event"]=Value(s.throat);o["capture_event"]=Value(s.capture);
    o["physics_status"]=Value(int(s.telemetry.stepStatus));o["failure_detail"]=Value(s.telemetry.failureDetail);
    o["physics_failure_code"]=Value(int(s.telemetry.failureCode));
    Value::Array cs;for(const auto& c:s.telemetry.contacts)cs.push_back(contact(c));o["contacts"]=Value(cs);
    Value::Array es;
    for(const auto& e:s.telemetry.tableDepartureEvents) {
        Value::Object v;v["kind"]=Value(int(e.kind));v["ball"]=Value(e.ballIndex);v["pocket_id"]=Value(e.pocketId);
        v["ordinal"]=Value(e.ordinal);v["solver_event_id"]=Value(e.solverEventId);
        v["linked_contact_ordinal"]=Value(e.linkedPhysicsContactOrdinal);v["stable_identity"]=Value(e.stableIdentity);
        v["capture_sequence"]=Value(double(e.pocketCaptureSequence));v["generation"]=Value(double(e.trajectoryGeneration));
        v["transition_witness_complete"]=Value(e.transitionWitnessComplete);v["time_seconds"]=Value(e.absoluteWholeTickTimeSeconds);
        v["source_domain"]=Value(int(e.sourceDomain));v["target_domain"]=Value(int(e.targetDomain));
        v["boundary_feature_id"]=Value(e.boundaryFeatureId);v["rail_feature_id"]=Value(e.railFeatureId);
        v["velocity_before_cm_s"]=point(e.velocityBeforeCmS);v["velocity_after_cm_s"]=point(e.velocityAfterCmS);
        es.push_back(Value(v));
    }
    o["table_departure_events"]=Value(es);return Value(o);
}
}
int main(int argc,char** argv) {
    try {
        if(argc!=2)throw std::runtime_error("usage: six_pocket_horizontal_entry_dump NEW_output.json");
        std::unique_ptr<std::FILE,decltype(&std::fclose)> out(std::fopen(argv[1],"wbx"),&std::fclose);
        if(!out)throw std::runtime_error("fresh-only output required; existing file never truncated");
        Value::Object root;
        root["schema"]=Value("six-pocket-horizontal-entry-physics-diagnostic-v1");
        root["state_source"]=Value("production updatePhysics(state,1/120,interactiveChinesePoolPhysicsProfile)");
        root["presentation_source"]=Value("buildPresentationSnapshot(state,state,1)");
        root["initial_state_source"]=Value("explicit test-only cloth-supported initial velocity/spin, not player shoot/legal input receipt");
        root["tick_scope"]=Value("diagnostic fixed-step attempt index, not GameRuntime/event-authority tick");
        root["step_seconds"]=Value(1.0/120);root["initial_speed_cm_s"]=Value(40);root["start_distance_cm"]=Value(12);
        root["post_tick_state_edits"]=Value(false);root["additional_capabilities_enabled"]=Value(false);
        Value::Array fixtures;
        for(int id=0;id<6;++id) {
            const auto f=test::buildSixPocketHorizontalEntryFixture(id);
            Value::Object row;row["pocket_id"]=Value(id);row["ball_identity"]=Value(1);
            row["profile"]=parse(canonicalPhysicsProfileJson(f.profile)).value;
            row["mouth_center_cm"]=point(f.frame.mouthCenter);row["inward"]=point(f.frame.inward);
            row["cloth_y_cm"]=Value(double(f.profile.tableDeparture.clothSurfaceYCm));row["radius_cm"]=Value(double(f.profile.ball.radiusCm));
            row["outcome"]=Value(f.outcome);row["fail_reason"]=Value(f.failReason);
            Value::Array ss;for(const auto& s:f.steps)ss.push_back(step(s));row["steps"]=Value(ss);
            Value::Array cc;
            for(const auto& c:f.checkpoints) {
                Value::Object v;v["label"]=Value(c.label);v["available"]=Value(c.stepIndex.has_value());
                if(c.stepIndex) {v["step_index"]=Value(int(*c.stepIndex));v["actual_snapshot"]=step(f.steps[*c.stepIndex]);}
                else {v["step_index"]=Value();v["actual_snapshot"]=Value();v["unavailable_reason"]=Value("stage not observed in actual physics trace");}
                cc.push_back(Value(v));
            }
            row["checkpoints"]=Value(cc);fixtures.push_back(Value(row));
            std::cout<<"pocket="<<id<<" outcome="<<f.outcome<<" ticks="<<f.steps.back().tick<<" reason="<<f.failReason<<'\n';
        }
        root["fixtures"]=Value(fixtures);const auto encoded=stringify(Value(root))+"\n";
        if(std::fwrite(encoded.data(),1,encoded.size(),out.get())!=encoded.size() || std::fflush(out.get()))
            throw std::runtime_error("fresh trace write failed");
        return 0;
    }catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}
}
