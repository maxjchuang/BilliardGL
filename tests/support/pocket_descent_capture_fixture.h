#pragma once

#include "physics.h"
#include "pocket_boundary.h"
#include "render_state.h"

#include <stdexcept>
#include <cmath>
#include <string>
#include <vector>

namespace billiardgl::test {

struct PocketDescentStep {
    int tick = 0;
    GameState state;
    PresentationSnapshot presentation;
    bool entry = false;
    bool throat = false;
    bool capture = false;
    PhysicsContactRecord captureContact;
};

struct PocketDescentCheckpoint {
    std::string label;
    std::size_t stepIndex = 0;
};

struct PocketDescentCaptureFixture {
    int pocketId = -1;
    int ballIdentity = 1;
    PhysicsProfile profile;
    std::vector<PocketDescentStep> steps;
    std::vector<PocketDescentCheckpoint> checkpoints;
};

// Test-only replay of the accepted six-pocket vertical-descent entry state.
// Every later state and visible frame comes from production updatePhysics
// and buildPresentationSnapshot, without facts injection or manual Y edits.
inline PocketDescentCaptureFixture buildPocketDescentCaptureFixture(int pocketId) {
    if (pocketId < 0 || pocketId >= 6) throw std::runtime_error("invalid pocket id");
    PocketDescentCaptureFixture result;
    result.pocketId = pocketId;
    result.profile = interactiveChinesePoolPhysicsProfile();
    const auto frames = buildPocketBoundaryFrames(result.profile.tableBoundary);
    GameState state;
    initializeBalls(state);
    for (auto& ball : state.balls) ball.pocketed = true;
    auto& ball = state.balls[result.ballIdentity];
    ball.pocketed = false;
    ball.physicalDomain = PhysicalBallDomain::Airborne;
    ball.position = {frames[pocketId].mouthCenter.x,
        result.profile.tableDeparture.clothSurfaceYCm + result.profile.ball.radiusCm + 1.0f,
        frames[pocketId].mouthCenter.z};
    ball.velocity = {0.0f, -20.0f, 0.0f};
    ball.speed = 20.0f;
    ball.trajectoryGeneration = 9;
    state.ballsMoving = true;
    const auto record = [&](int tick, bool entry, bool throat, bool capture,
                            const PhysicsContactRecord& contact) {
        PocketDescentStep step;
        step.tick=tick; step.state=state;
        step.presentation=buildPresentationSnapshot(state,state,1.0);
        step.entry=entry; step.throat=throat; step.capture=capture;
        step.captureContact=contact;
        result.steps.push_back(std::move(step));
    };
    record(0,false,false,false,{});
    result.checkpoints.push_back({"initial_above_mouth",0});
    bool entered=false, throatSeen=false, centerBelow=false, captured=false;
    int entryTick=-1;
    for(int tick=1;tick<=120 && !captured;++tick) {
        const auto telemetry=updatePhysics(state,1.0f/120.0f,result.profile);
        if(telemetry.stepStatus==PhysicsStepStatus::Failed)
            throw std::runtime_error("pocket descent physics failure:"+telemetry.failureDetail);
        bool entry=false,throat=false,capture=false;
        PhysicsContactRecord captureContact;
        for(const auto& event:telemetry.tableDepartureEvents)
            entry|=event.kind==TableDepartureEventKind::PocketEntry && event.ballIndex==result.ballIdentity;
        for(const auto& contact:telemetry.contacts) {
            if(contact.kind!=PhysicsContactKind::Pocket || contact.firstBall!=result.ballIdentity ||
                contact.pocketId!=pocketId) continue;
            throat|=contact.pocketBoundaryEvent==PocketBoundaryEventKind::Throat;
            if(contact.pocketBoundaryEvent==PocketBoundaryEventKind::Capture) {
                capture=true; captureContact=contact;
            }
        }
        record(tick,entry,throat,capture,captureContact);
        if(entry && !entered) {
            entered=true; entryTick=tick;
            result.checkpoints.push_back({"pocket_channel_entry",result.steps.size()-1});
        }
        throatSeen|=throat;
        if(!centerBelow && !capture && ball.position.y <= result.profile.tableDeparture.clothSurfaceYCm) {
            centerBelow=true;
            result.checkpoints.push_back({"center_below_cloth",result.steps.size()-1});
        }
        if(capture) {
            if(!entered || !throatSeen || tick<=entryTick || !centerBelow ||
                !captureContact.pocketVerticalShaft || !captureContact.pocketCaptureSequence ||
                captureContact.trajectoryGenerationBefore!=9 ||
                std::abs(captureContact.firstPositionAtContactCm.y-
                    (result.profile.tableDeparture.clothSurfaceYCm-result.profile.ball.radiusCm))>0.001f)
                throw std::runtime_error("invalid canonical descent capture lineage");
            if(result.steps.back().presentation.balls[result.ballIdentity].visible)
                throw std::runtime_error("captured storage slot became visible");
            result.checkpoints.push_back({"last_visible_before_capture",result.steps.size()-2});
            result.checkpoints.push_back({"whole_ball_clear_capture_hidden",result.steps.size()-1});
            captured=true;
        }
    }
    if(!captured) throw std::runtime_error("descent did not capture");
    return result;
}

} // namespace billiardgl::test
