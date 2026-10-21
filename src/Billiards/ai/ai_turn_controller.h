#pragma once

#include "ai/async_shot_planner.h"
#include "ai/game_preview_evaluator.h"
#include "deterministic_hash.h"

#include <cstdint>
#include <string>
#include <vector>

namespace billiardgl::ai {

enum class TurnPollStatus {
    Idle,
    Thinking,
    Applied,
    NoShot,
    StaleAuthority,
    Rejected,
};

struct TurnResult {
    TurnPollStatus status = TurnPollStatus::Idle;
    std::string error;
    PlanResult plan;
};

class AiTurnController {
public:
    bool begin(const GameRuntime& authority, Difficulty difficulty,
        std::uint64_t deterministicSeed,
        PlanningBudget budget = defaultBudget(Difficulty::Club),
        const std::string& retainedBallInHandDecisionId = {});
    TurnResult pollAndApply(GameRuntime& authority);
    void cancel() noexcept;
    bool thinking() const { return worker_.busy(); }
    bool pending() const { return awaitingResult_; }
    bool retiring() const { return worker_.retiring(); }

private:
    static std::vector<int> legalTargets(const GameRuntime& authority);

    AsyncShotPlanner worker_;
    bool awaitingResult_ = false;
    std::string authorityHash_;
    std::string authorityRulesHash_;
    std::string retainedBallInHandDecisionId_;
    std::uint64_t authorityTick_ = 0;
    int authorityPlayer_ = -1;
};

}  // namespace billiardgl::ai
