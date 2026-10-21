#pragma once

#include "shot_control_model.h"

#include <atomic>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

namespace billiardgl::ai {

enum class Difficulty {
    Rookie,
    Club,
    Expert,
    // Source-compatible names used by the initial planner spike.
    Easy = Rookie,
    Medium = Club,
    Hard = Expert,
};

struct DifficultyProfile {
    float maximumAimErrorRadians = 0.0f;
    float maximumPowerErrorFraction = 0.0f;
    float maximumTipErrorRadius = 0.0f;
};

struct SemanticShotAction {
    ShotControlModel controls;
    int targetBall = -1;
    int targetPocket = -1;
};

struct ShotCandidate {
    SemanticShotAction action;
    float visibility = 0.0f;
};

struct PreviewScore {
    bool valid = false;
    float potProbability = 0.0f;
    float cuePositionQuality = 0.0f;
    float nextShotQuality = 0.0f;
    float foulProbability = 1.0f;
};

// Implementations must evaluate an isolated runtime clone. They may not expose
// solver-only impulses or mutate the authoritative GameRuntime.
class PreviewEvaluator {
public:
    virtual ~PreviewEvaluator() = default;
    virtual PreviewScore evaluate(
        const SemanticShotAction& action,
        std::chrono::steady_clock::time_point deadline) = 0;
};

struct PlanningBudget {
    std::size_t maximumCandidates = 24;
    std::chrono::milliseconds maximumWallTime{8};
};

struct PlanResult {
    bool found = false;
    SemanticShotAction action;
    float score = 0.0f;
    std::size_t candidatesEvaluated = 0;
    bool candidateBudgetReached = false;
    bool wallTimeBudgetReached = false;
    double elapsedMilliseconds = 0.0;
    std::string error;
};

PlanningBudget defaultBudget(Difficulty difficulty) noexcept;
DifficultyProfile difficultyProfile(Difficulty difficulty) noexcept;
const char* difficultyId(Difficulty difficulty) noexcept;

class ShotPlanner {
public:
    explicit ShotPlanner(Difficulty difficulty,
                         std::uint64_t deterministicSeed = 1);

    PlanResult plan(const std::vector<ShotCandidate>& candidates,
                    PreviewEvaluator& evaluator) const;
    PlanResult plan(const std::vector<ShotCandidate>& candidates,
                    PreviewEvaluator& evaluator,
                    PlanningBudget budget,
                    const std::atomic_bool* cancellation = nullptr) const;

    Difficulty difficulty() const noexcept { return difficulty_; }

private:
    Difficulty difficulty_;
    std::uint64_t seed_;
};

}  // namespace billiardgl::ai
