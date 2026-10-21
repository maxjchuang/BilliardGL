#include "ai/async_shot_planner.h"

#include <atomic>
#include <chrono>
#include <cstdlib>
#include <iostream>
#include <memory>
#include <stdexcept>
#include <thread>

namespace {

class SlowPreview final : public billiardgl::ai::PreviewEvaluator {
public:
    billiardgl::ai::PreviewScore evaluate(
        const billiardgl::ai::SemanticShotAction& action,
        std::chrono::steady_clock::time_point) override
    {
        std::this_thread::sleep_for(std::chrono::milliseconds(3));
        return {true, 1.0f - 0.1f * action.targetBall, 0.7f, 0.6f, 0.0f};
    }
};

class CancellationPreview final : public billiardgl::ai::PreviewEvaluator {
public:
    billiardgl::ai::PreviewScore evaluate(
        const billiardgl::ai::SemanticShotAction&,
        std::chrono::steady_clock::time_point) override
    {
        ++calls;
        started.store(true, std::memory_order_release);
        std::this_thread::sleep_for(std::chrono::milliseconds(200));
        finished.store(true, std::memory_order_release);
        return {true, 0.5f, 0.5f, 0.5f, 0.0f};
    }

    std::atomic_bool started{false};
    std::atomic_bool finished{false};
    std::atomic_int calls{0};
};

class ThrowingPreview final : public billiardgl::ai::PreviewEvaluator {
public:
    billiardgl::ai::PreviewScore evaluate(
        const billiardgl::ai::SemanticShotAction&,
        std::chrono::steady_clock::time_point) override
    {
        throw std::runtime_error("expected_preview_failure");
    }
};

void expect(bool value, const char* message)
{
    if (!value) throw std::runtime_error(message);
}

std::vector<billiardgl::ai::ShotCandidate> candidates()
{
    std::vector<billiardgl::ai::ShotCandidate> result(8);
    for (std::size_t index = 0; index < result.size(); ++index) {
        result[index].action.targetBall = static_cast<int>(index);
        result[index].action.controls.schemaVersion =
            billiardgl::ShotControlSchemaVersion::IntentV2;
    }
    return result;
}

}  // namespace

int main()
{
    try {
        billiardgl::ai::AsyncShotPlanner worker;
        const auto before = std::chrono::steady_clock::now();
        expect(worker.submit(billiardgl::ai::Difficulty::Club, 91, candidates(),
            std::make_shared<SlowPreview>(),
            {8, std::chrono::milliseconds(40)}),
            "first AI worker job should submit");
        const auto submitMilliseconds = std::chrono::duration<double, std::milli>(
            std::chrono::steady_clock::now() - before).count();
        expect(submitMilliseconds < 5.0,
            "AI submission must not run preview work on the main thread");
        expect(worker.busy() && !worker.poll().has_value(),
            "non-blocking poll reports an in-flight job");
        expect(!worker.submit(billiardgl::ai::Difficulty::Rookie, 92, candidates(),
            std::make_shared<SlowPreview>(),
            {8, std::chrono::milliseconds(40)}),
            "a second job cannot replace in-flight AI work");

        std::optional<billiardgl::ai::PlanResult> result;
        const auto deadline = std::chrono::steady_clock::now() +
            std::chrono::milliseconds(250);
        while (!result && std::chrono::steady_clock::now() < deadline) {
            std::this_thread::sleep_for(std::chrono::milliseconds(1));
            result = worker.poll();
        }
        expect(result.has_value() && result->found &&
            result->candidatesEvaluated == 8,
            "worker returns the bounded semantic shot result");
        expect(!worker.busy() && !worker.poll().has_value(),
            "completed result is consumed exactly once");

        expect(worker.submit(billiardgl::ai::Difficulty::Club, 92,
                   candidates(), std::make_shared<ThrowingPreview>(),
                   {8, std::chrono::milliseconds(40)}),
            "exception fixture submits on the detached worker");
        bool exceptionDelivered = false;
        const auto exceptionDeadline = std::chrono::steady_clock::now() +
            std::chrono::milliseconds(100);
        while (!exceptionDelivered &&
            std::chrono::steady_clock::now() < exceptionDeadline) {
            try {
                const auto ignored = worker.poll();
                if (!ignored) std::this_thread::yield();
            } catch (const std::runtime_error& error) {
                exceptionDelivered =
                    std::string(error.what()) == "expected_preview_failure";
            }
        }
        expect(exceptionDelivered && !worker.busy(),
            "detached worker completes its promise with the preview exception");

        auto cancellationPreview = std::make_shared<CancellationPreview>();
        expect(worker.submit(billiardgl::ai::Difficulty::Expert, 93,
                   candidates(), cancellationPreview,
                   {8, std::chrono::milliseconds(2000)}),
            "cancellation fixture submits a long-running cloned preview");
        const auto startDeadline = std::chrono::steady_clock::now() +
            std::chrono::milliseconds(100);
        while (!cancellationPreview->started.load(std::memory_order_acquire) &&
            std::chrono::steady_clock::now() < startDeadline)
            std::this_thread::yield();
        expect(cancellationPreview->started.load(std::memory_order_acquire),
            "cancellation fixture starts its background preview");
        const auto cancelStarted = std::chrono::steady_clock::now();
        worker.cancel();
        const double cancelMilliseconds =
            std::chrono::duration<double, std::milli>(
                std::chrono::steady_clock::now() - cancelStarted).count();
        expect(cancelMilliseconds < 25.0,
            "AI worker cancellation does not join the remaining planning budget");
        for (int attempt = 0; attempt < 64; ++attempt) {
            expect(!worker.submit(billiardgl::ai::Difficulty::Rookie,
                       100 + static_cast<std::uint64_t>(attempt), candidates(),
                       std::make_shared<SlowPreview>(),
                       {8, std::chrono::milliseconds(40)}),
                "repeated reconfiguration cannot accumulate detached planning workers");
        }
        const auto finishDeadline = std::chrono::steady_clock::now() +
            std::chrono::milliseconds(500);
        while (!cancellationPreview->finished.load(std::memory_order_acquire) &&
            std::chrono::steady_clock::now() < finishDeadline)
            std::this_thread::sleep_for(std::chrono::milliseconds(1));
        expect(cancellationPreview->finished.load(std::memory_order_acquire) &&
                cancellationPreview->calls.load(std::memory_order_relaxed) == 1,
            "cancelled detached work finishes its current clone preview and starts no successor");
        const auto retirementDeadline = std::chrono::steady_clock::now() +
            std::chrono::milliseconds(100);
        while (worker.retiring() &&
            std::chrono::steady_clock::now() < retirementDeadline)
            std::this_thread::yield();
        expect(!worker.retiring() &&
                worker.submit(billiardgl::ai::Difficulty::Club, 194,
                    candidates(), std::make_shared<SlowPreview>(),
                    {8, std::chrono::milliseconds(40)}),
            "one replacement job may start after the cancelled worker retires");
        worker.cancel();

        auto destructorPreview = std::make_shared<CancellationPreview>();
        auto cancellable = std::make_unique<billiardgl::ai::AsyncShotPlanner>();
        expect(cancellable->submit(billiardgl::ai::Difficulty::Expert, 195,
                   candidates(), destructorPreview,
                   {8, std::chrono::milliseconds(2000)}),
            "destructor fixture submits a long-running cloned preview");
        while (!destructorPreview->started.load(std::memory_order_acquire))
            std::this_thread::yield();
        const auto destroyStarted = std::chrono::steady_clock::now();
        cancellable.reset();
        const double destroyMilliseconds =
            std::chrono::duration<double, std::milli>(
                std::chrono::steady_clock::now() - destroyStarted).count();
        expect(destroyMilliseconds < 25.0,
            "AI worker destruction cancels without joining the remaining planning budget");
        while (!destructorPreview->finished.load(std::memory_order_acquire))
            std::this_thread::sleep_for(std::chrono::milliseconds(1));
        expect(destructorPreview->calls.load(std::memory_order_relaxed) == 1,
            "destroyed worker starts no preview after cancellation");
        std::cout << "ai_worker_cancel cancel_ms=" << cancelMilliseconds
                  << " destroy_ms=" << destroyMilliseconds
                  << " calls=" << cancellationPreview->calls.load() << '\n';
        return EXIT_SUCCESS;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return EXIT_FAILURE;
    }
}
