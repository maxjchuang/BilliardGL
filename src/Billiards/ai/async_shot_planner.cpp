#include "ai/async_shot_planner.h"

#include <chrono>
#include <exception>
#include <thread>
#include <utility>

namespace billiardgl::ai {

AsyncShotPlanner::~AsyncShotPlanner()
{
    cancel();
}

bool AsyncShotPlanner::submit(Difficulty difficulty,
    std::uint64_t deterministicSeed, std::vector<ShotCandidate> candidates,
    std::shared_ptr<PreviewEvaluator> evaluator, PlanningBudget budget)
{
    if (!evaluator || pending_.valid() || retiring()) return false;
    retiring_.reset();
    cancellation_ = std::make_shared<std::atomic_bool>(false);
    auto cancellation = cancellation_;
    active_ = std::make_shared<std::atomic_bool>(true);
    auto active = active_;
    std::promise<PlanResult> promise;
    pending_ = promise.get_future();
    std::thread([difficulty, deterministicSeed,
            candidates = std::move(candidates),
            evaluator = std::move(evaluator), budget,
            cancellation = std::move(cancellation),
            active = std::move(active),
            promise = std::move(promise)]() mutable {
        try {
            promise.set_value(ShotPlanner(difficulty, deterministicSeed).plan(
                candidates, *evaluator, budget, cancellation.get()));
        } catch (...) {
            promise.set_exception(std::current_exception());
        }
        active->store(false, std::memory_order_release);
    }).detach();
    return true;
}

bool AsyncShotPlanner::busy() const
{
    return pending_.valid() && pending_.wait_for(std::chrono::milliseconds(0)) !=
        std::future_status::ready;
}

bool AsyncShotPlanner::retiring() const noexcept
{
    if (!retiring_) return false;
    if (retiring_->load(std::memory_order_acquire)) return true;
    retiring_.reset();
    return false;
}

std::optional<PlanResult> AsyncShotPlanner::poll()
{
    if (!pending_.valid() || pending_.wait_for(std::chrono::milliseconds(0)) !=
            std::future_status::ready)
        return std::nullopt;
    auto result = pending_.get();
    cancellation_.reset();
    active_.reset();
    return result;
}

void AsyncShotPlanner::cancel() noexcept
{
    if (cancellation_)
        cancellation_->store(true, std::memory_order_relaxed);
    pending_ = std::future<PlanResult>{};
    if (active_ && active_->load(std::memory_order_acquire))
        retiring_ = active_;
    cancellation_.reset();
    active_.reset();
}

}  // namespace billiardgl::ai
