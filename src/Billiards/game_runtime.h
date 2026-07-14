#pragma once

#include "game_action.h"
#include "game_state.h"
#include "cue_impact.h"
#include "cue_contact.h"
#include "physics_telemetry.h"
#include "physics_boundary_mode.h"
#include "physics_profile.h"

#include <cstdint>
#include <string>
#include <vector>

namespace billiardgl {

struct RuntimeEvent {
    RuntimeEvent() = default;
    RuntimeEvent(std::uint64_t sequenceValue, std::uint64_t tickValue, const std::string& eventName)
        : sequence(sequenceValue), tick(tickValue), name(eventName) {}
    std::uint64_t sequence = 0;
    std::uint64_t tick = 0;
    std::string name;
};

struct CueShotApplication {
    CueContactResult contact;
    ActionResult action;
};

CueShotApplication applyCueShot(GameState& state, const CueImpactInput& input,
    const PhysicsProfile& profile);

class GameRuntime {
public:
    GameRuntime();

    void reset();
    ActionResult dispatch(const GameAction& action);
    ActionResult step(int count);

    const GameState& state() const { return state_; }
    GameState& mutableState() { return state_; }
    std::uint64_t tick() const { return tick_; }
    const std::vector<RuntimeEvent>& events() const { return events_; }
    std::vector<RuntimeEvent> eventsSince(std::uint64_t sequence) const;
    ActionResult setBall(int index, const BallState& ball);
    ActionResult applyCueImpact(const CueImpactInput& input);
    void replaceState(const GameState& state);
    ActionResult replaceStateForScenario(
        const GameState& state, const CueImpactInput* cueImpact = nullptr);
    ActionResult replaceStateForScenario(
        const GameState& state, const PhysicsProfile& profile,
        const CueImpactInput* cueImpact = nullptr,
        bool executeCueImpact = false,
        PhysicsBoundaryMode boundaryMode = PhysicsBoundaryMode::ProductionTable);
    void clearEvents();
    void setPhysicsTraceEnabled(bool enabled) { physicsTraceEnabled_ = enabled; }
    bool physicsTraceEnabled() const { return physicsTraceEnabled_; }
    void clearPhysicsTrace() { physicsTrace_.clear(); }
    const PhysicsTrace& physicsTrace() const { return physicsTrace_; }
    bool hasCueImpactInput() const { return hasCueImpactInput_; }
    const CueImpactInput& cueImpactInput() const { return cueImpactInput_; }
    bool hasCueContactResult() const { return hasCueContactResult_; }
    const CueContactResult& cueContactResult() const { return cueContactResult_; }
    const PhysicsProfile& physicsProfile() const { return physicsProfile_; }
    PhysicsBoundaryMode boundaryMode() const { return boundaryMode_; }

private:
    ActionResult applyShot();
    void recordEvents();

    GameState state_;
    std::uint64_t tick_ = 0;
    std::uint64_t nextSequence_ = 1;
    std::vector<RuntimeEvent> events_;
    bool physicsTraceEnabled_ = false;
    PhysicsTrace physicsTrace_{10000};
    bool hasCueImpactInput_ = false;
    CueImpactInput cueImpactInput_;
    bool hasCueContactResult_ = false;
    CueContactResult cueContactResult_;
    bool cueContactPending_ = false;
    PhysicsProfile physicsProfile_;
    PhysicsBoundaryMode boundaryMode_ = PhysicsBoundaryMode::ProductionTable;
};

}  // namespace billiardgl
