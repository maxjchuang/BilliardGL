#pragma once

#include "game_action.h"
#include "game_state.h"

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
    void replaceState(const GameState& state);
    void clearEvents();

private:
    void applyShot();
    void recordEvents();

    GameState state_;
    std::uint64_t tick_ = 0;
    std::uint64_t nextSequence_ = 1;
    std::vector<RuntimeEvent> events_;
};

}  // namespace billiardgl
