#pragma once

#include "ai/shot_planner.h"
#include "game_action.h"

#include <cstdint>
#include <string>

namespace billiardgl {

class GameRuntime;

namespace ai {

enum class PlayerPollStatus {
    Disabled,
    WaitingForTurn,
    Thinking,
    Applied,
    NoShot,
    StaleAuthority,
    Rejected,
    MatchComplete,
};

struct PlayerConfiguration {
    bool enabled = false;
    int player = 1;
    Difficulty difficulty = Difficulty::Club;
    std::uint64_t deterministicSeed = 1;
};

struct PlayerState {
    PlayerConfiguration configuration;
    PlayerPollStatus status = PlayerPollStatus::Disabled;
    bool thinking = false;
    std::uint64_t jobsStarted = 0;
    std::string error;
};

const char* playerPollStatusId(PlayerPollStatus status) noexcept;

// Narrow product/automation boundary. Automation can drive a real AI player
// without taking a compile-time dependency on its worker implementation.
class PlayerControl {
public:
    virtual ~PlayerControl() = default;
    virtual ActionResult configure(
        GameRuntime& authority, const PlayerConfiguration& configuration) = 0;
    virtual PlayerState state() const = 0;
    virtual PlayerState poll(GameRuntime& authority) = 0;
};

}  // namespace ai
}  // namespace billiardgl
