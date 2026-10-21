#pragma once

#include "ai/ai_player_control.h"
#include "ai/ai_turn_controller.h"

#include <string>

namespace billiardgl::ai {

class AiPlayerController final : public PlayerControl {
public:
    ActionResult configure(GameRuntime& authority,
        const PlayerConfiguration& configuration) override;
    PlayerState state() const override;
    PlayerState poll(GameRuntime& authority) override;

private:
    void setResult(const TurnResult& result);

    PlayerConfiguration configuration_;
    AiTurnController turn_;
    PlayerPollStatus status_ = PlayerPollStatus::Disabled;
    std::uint64_t jobsStarted_ = 0;
    std::string error_;
    std::string lastAttemptHash_;
    std::uint64_t lastAttemptTick_ = 0;
    int lastAttemptPlayer_ = -1;
    bool attemptBlocked_ = false;
    std::string retainedBallInHandDecisionId_;
    std::uint64_t decisionOrdinal_ = 0;
};

}  // namespace billiardgl::ai
