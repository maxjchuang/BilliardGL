#pragma once
#include "physics.h"
#include "pocket_boundary.h"
#include "render_state.h"
#include <cmath>
#include <optional>
#include <stdexcept>
#include <string>
#include <vector>

namespace billiardgl::test {
struct HorizontalPocketStep {
    int tick=0;
    GameState state;
    PresentationSnapshot presentation;
    PhysicsStepTelemetry telemetry;
    bool entry=false,throat=false,capture=false;
};
struct HorizontalPocketCheckpoint {
    std::string label;
    std::optional<std::size_t> stepIndex;
};
struct SixPocketHorizontalEntryFixture {
    int pocketId=-1;
    PhysicsProfile profile;
    PocketBoundaryFrame frame;
    std::vector<HorizontalPocketStep> steps;
    std::vector<HorizontalPocketCheckpoint> checkpoints;
    std::string outcome,failReason;
};

// Explicit initial-state physics diagnostic, not a player shoot/legal input
// receipt. No state/position/domain/contact is edited after initialization.
inline SixPocketHorizontalEntryFixture buildSixPocketHorizontalEntryFixture(int id) {
    if(id<0 || id>=6)throw std::runtime_error("invalid pocket id");
    SixPocketHorizontalEntryFixture f;
    f.pocketId=id;f.profile=interactiveChinesePoolPhysicsProfile();
    f.frame=buildPocketBoundaryFrames(f.profile.tableBoundary)[id];
    GameState state;initializeBalls(state);
    for(auto& ball:state.balls)ball.pocketed=true;
    auto& ball=state.balls[1];
    ball.pocketed=false;ball.physicalDomain=PhysicalBallDomain::ClothSupported;
    ball.position={f.frame.mouthCenter.x+12*f.frame.inward.x,
        f.profile.tableDeparture.clothSurfaceYCm+f.profile.ball.radiusCm,
        f.frame.mouthCenter.z+12*f.frame.inward.z};
    ball.velocity={-40*f.frame.inward.x,0,-40*f.frame.inward.z};
    ball.angularVelocity={ball.velocity.z/f.profile.ball.radiusCm,0,
        -ball.velocity.x/f.profile.ball.radiusCm};
    ball.speed=40;ball.trajectoryGeneration=9;state.ballsMoving=true;
    const auto record=[&](int tick,PhysicsStepTelemetry telemetry) {
        HorizontalPocketStep s;s.tick=tick;s.state=state;
        s.presentation=buildPresentationSnapshot(state,state,1.0);
        s.telemetry=std::move(telemetry);
        for(const auto& e:s.telemetry.tableDepartureEvents)
            if(e.ballIndex==1 && e.kind==TableDepartureEventKind::PocketEntry)s.entry=true;
        for(const auto& c:s.telemetry.contacts)
            if(c.firstBall==1 && c.pocketId==id && c.kind==PhysicsContactKind::Pocket) {
                s.throat|=c.pocketBoundaryEvent==PocketBoundaryEventKind::Throat;
                s.capture|=c.pocketBoundaryEvent==PocketBoundaryEventKind::Capture;
            }
        f.steps.push_back(std::move(s));
    };
    record(0,{});
    std::optional<std::size_t> entry,throat,capture,lastCloth;
    bool reflected=false;
    for(int tick=1;tick<=480;++tick) {
        auto telemetry=updatePhysics(state,1.0f/120.0f,f.profile);
        record(tick,std::move(telemetry));
        const auto index=f.steps.size()-1;
        const auto& s=f.steps.back();
        if(s.entry && !entry) {
            entry=index;
            for(std::size_t prior=index;prior>0;--prior)
                if(f.steps[prior-1].state.balls[1].physicalDomain==PhysicalBallDomain::ClothSupported) {
                    lastCloth=prior-1;break;
                }
        }
        if(s.throat && !throat)throat=index;
        if(s.capture && !capture)capture=index;
        reflected|=(ball.velocity.x*f.frame.inward.x+ball.velocity.z*f.frame.inward.z)>0.01f;
        if(s.telemetry.stepStatus==PhysicsStepStatus::Failed) {
            f.outcome="physics_failed";f.failReason=s.telemetry.failureDetail;break;
        }
        if(capture) {
            f.outcome=entry?"captured_actual":"captured_actual_missing_entry";
            if(!entry)f.failReason="actual capture receipt exists but no TableDeparture PocketEntry/PocketChannel descent was observed";
            break;
        }
        if(!state.ballsMoving) {
            f.outcome=reflected?"rebounded_then_stopped":"stopped_without_capture";
            f.failReason="production physics stopped before any capture receipt";break;
        }
    }
    if(f.outcome.empty()) {
        f.outcome=reflected?"rebounded_no_capture_by_limit":"no_capture_by_limit";
        f.failReason="480 fixed-step diagnostic limit reached without capture receipt";
    }
    std::optional<std::size_t> lastVisible;
    if(capture)for(std::size_t prior=*capture;prior>0;--prior)
        if(f.steps[prior-1].presentation.balls[1].visible) {lastVisible=prior-1;break;}
    f.checkpoints={{"initial_on_cloth",0},{"last_cloth_before_entry",lastCloth},
        {"first_pocket_entry",entry},{"first_throat",throat},
        {"last_visible_before_capture",lastVisible},{"final_actual_state",f.steps.size()-1}};
    return f;
}
} // namespace billiardgl::test
