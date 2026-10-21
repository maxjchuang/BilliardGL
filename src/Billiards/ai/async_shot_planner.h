#pragma once

#include "ai/shot_planner.h"

#include <atomic>
#include <future>
#include <memory>
#include <optional>
#include <vector>

namespace billiardgl::ai {

// Owns one bounded planning job. submit() and poll() never wait for preview
// work; the evaluator is retained by the worker until the result is collected.
class AsyncShotPlanner {
public:
    AsyncShotPlanner() = default;
    ~AsyncShotPlanner();

    AsyncShotPlanner(const AsyncShotPlanner&) = delete;
    AsyncShotPlanner& operator=(const AsyncShotPlanner&) = delete;

    bool submit(Difficulty difficulty, std::uint64_t deterministicSeed,
        std::vector<ShotCandidate> candidates,
        std::shared_ptr<PreviewEvaluator> evaluator,
        PlanningBudget budget);
    bool busy() const;
    bool retiring() const noexcept;
    std::optional<PlanResult> poll();
    void cancel() noexcept;

private:
    mutable std::future<PlanResult> pending_;
    std::shared_ptr<std::atomic_bool> cancellation_;
    std::shared_ptr<std::atomic_bool> active_;
    mutable std::shared_ptr<std::atomic_bool> retiring_;
};

}  // namespace billiardgl::ai
