#pragma once

#include "ai/shot_planner.h"
#include "game_runtime.h"
#include "training/preview_runtime.h"

#include <vector>

namespace billiardgl::ai {

struct CandidateGenerationOptions {
    std::vector<int> legalTargetBalls;
    std::vector<float> requestedPowers{48.0f, 68.0f};
    std::size_t maximumCandidates = 96;
};

std::vector<ShotCandidate> enumerateTableCandidates(
    const GameState& state, const PhysicsProfile& profile,
    const CandidateGenerationOptions& options);

class GamePreviewEvaluator final : public PreviewEvaluator {
public:
    explicit GamePreviewEvaluator(const GameRuntime& authority,
        training::PreviewBudget budget = {});

    PreviewScore evaluate(const SemanticShotAction& action,
        std::chrono::steady_clock::time_point deadline) override;

private:
    GameState state_;
    PhysicsProfile profile_;
    PhysicsBoundaryMode boundaryMode_ = PhysicsBoundaryMode::ProductionTable;
    ShotControlCapabilities capabilities_;
    training::PreviewBudget budget_;
};

}  // namespace billiardgl::ai
