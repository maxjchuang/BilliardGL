#include "ai/shot_planner.h"

#include <chrono>
#include <cstdlib>
#include <iostream>
#include <stdexcept>
#include <thread>
#include <vector>

namespace {

using namespace billiardgl::ai;

void expect(bool value, const char* message)
{
    if (!value) throw std::runtime_error(message);
}

class FixedPreview final : public PreviewEvaluator {
public:
    explicit FixedPreview(bool slow = false) : slow_(slow) {}

    PreviewScore evaluate(const SemanticShotAction& action,
                          std::chrono::steady_clock::time_point) override
    {
        ++calls;
        if (slow_) std::this_thread::sleep_for(std::chrono::milliseconds(2));
        const float quality = 1.0f - 0.12f * static_cast<float>(action.targetBall);
        return {action.targetBall >= 0, quality, quality * 0.8f,
                quality * 0.7f, 0.02f * static_cast<float>(action.targetBall)};
    }

    int calls = 0;

private:
    bool slow_;
};

class RecordingPreview final : public PreviewEvaluator {
public:
    PreviewScore evaluate(const SemanticShotAction& action,
                          std::chrono::steady_clock::time_point) override
    {
        seen = action;
        called = true;
        return {true, 1.0f, 1.0f, 1.0f, 0.0f};
    }

    SemanticShotAction seen;
    bool called = false;
};

std::vector<ShotCandidate> candidates(int count)
{
    std::vector<ShotCandidate> result;
    for (int index = 0; index < count; ++index) {
        ShotCandidate candidate;
        candidate.action.targetBall = index;
        candidate.action.targetPocket = index % 6;
        candidate.action.controls.schemaVersion =
            billiardgl::ShotControlSchemaVersion::IntentV2;
        candidate.action.controls.requestedPower = 40.0f + index;
        candidate.visibility = 1.0f - 0.03f * index;
        result.push_back(candidate);
    }
    return result;
}

void testSemanticAndBudgets()
{
    FixedPreview preview;
    ShotPlanner planner(Difficulty::Hard, 7);
    const PlanResult result = planner.plan(candidates(100), preview,
                                           {11, std::chrono::milliseconds(50)});
    expect(result.found && result.action.targetBall == 0,
           "hard planner should choose the best semantic action");
    expect(result.action.controls.schemaVersion ==
               billiardgl::ShotControlSchemaVersion::IntentV2,
           "AI must return the same versioned shot action used by players");
    expect(result.candidatesEvaluated == 11 && preview.calls == 11 &&
               result.candidateBudgetReached,
           "candidate budget must be a hard upper bound");

    FixedPreview slow(true);
    const PlanResult timed = planner.plan(candidates(40), slow,
                                          {40, std::chrono::milliseconds(5)});
    expect(timed.wallTimeBudgetReached && timed.candidatesEvaluated < 40,
           "deadline must stop further preview work");
}

void testDifficultyIsDeterministicAndMeasurable()
{
    int easyWins = 0, mediumWins = 0, hardWins = 0;
    for (std::uint64_t seed = 1; seed <= 120; ++seed) {
        const auto set = candidates(6);
        FixedPreview easyPreview, mediumPreview, hardPreview;
        easyWins += ShotPlanner(Difficulty::Easy, seed)
                        .plan(set, easyPreview, {6, std::chrono::milliseconds(50)})
                        .action.targetBall == 0;
        mediumWins += ShotPlanner(Difficulty::Medium, seed)
                          .plan(set, mediumPreview, {6, std::chrono::milliseconds(50)})
                          .action.targetBall == 0;
        hardWins += ShotPlanner(Difficulty::Hard, seed)
                        .plan(set, hardPreview, {6, std::chrono::milliseconds(50)})
                        .action.targetBall == 0;
    }
    expect(easyWins < mediumWins && mediumWins < hardWins,
           "three AI levels need a measurable success-rate ordering");
    expect(hardWins >= 115, "hard AI should reliably select the best fixed candidate");

    FixedPreview firstPreview, secondPreview;
    const auto set = candidates(6);
    const auto first = ShotPlanner(Difficulty::Easy, 99)
                           .plan(set, firstPreview, {6, std::chrono::milliseconds(50)});
    const auto second = ShotPlanner(Difficulty::Easy, 99)
                            .plan(set, secondPreview, {6, std::chrono::milliseconds(50)});
    expect(first.action.targetBall == second.action.targetBall &&
               first.score == second.score,
           "AI selection must be replayable from its deterministic seed");
}

void testExecutedActionIsThePreviewedAction()
{
    const auto set = candidates(1);
    RecordingPreview preview;
    const PlanResult result = ShotPlanner(Difficulty::Rookie, 0x45584543ull)
        .plan(set, preview, {1, std::chrono::milliseconds(50)});
    expect(result.found && preview.called &&
            result.action.controls.yawRadians ==
                preview.seen.controls.yawRadians &&
            result.action.controls.requestedPower ==
                preview.seen.controls.requestedPower &&
            result.action.controls.tipOffsetX ==
                preview.seen.controls.tipOffsetX &&
            result.action.controls.tipOffsetY ==
                preview.seen.controls.tipOffsetY &&
            result.action.targetBall == preview.seen.targetBall &&
            result.action.targetPocket == preview.seen.targetPocket &&
            (result.action.controls.requestedPower !=
                    set.front().action.controls.requestedPower ||
                result.action.controls.yawRadians !=
                    set.front().action.controls.yawRadians ||
                result.action.controls.tipOffsetX !=
                    set.front().action.controls.tipOffsetX ||
                result.action.controls.tipOffsetY !=
                    set.front().action.controls.tipOffsetY),
        "planner must preview and return the same deterministic executed action, not perturb an ideal action after scoring");
}

void testInvalidInputs()
{
    FixedPreview preview;
    ShotPlanner planner(Difficulty::Medium);
    expect(planner.plan({}, preview).error == "no_valid_candidate",
           "empty candidate set should fail explicitly");
    expect(planner.plan(candidates(1), preview,
                        {0, std::chrono::milliseconds(1)}).error ==
               "invalid_planning_budget",
           "zero budget should fail explicitly");
}

}  // namespace

int main()
{
    try {
        testSemanticAndBudgets();
        testDifficultyIsDeterministicAndMeasurable();
        testExecutedActionIsThePreviewedAction();
        testInvalidInputs();
        return EXIT_SUCCESS;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return EXIT_FAILURE;
    }
}
