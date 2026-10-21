#include "ai/game_preview_evaluator.h"
#include "ai/shot_planner.h"
#include "table_departure_geometry.h"
#include "training/preview_runtime.h"

#include <array>
#include <chrono>
#include <cstdlib>
#include <iostream>
#include <stdexcept>
#include <string>
#include <vector>

namespace {

using billiardgl::ai::Difficulty;
using billiardgl::ai::PreviewEvaluator;
using billiardgl::ai::PreviewScore;
using billiardgl::ai::SemanticShotAction;
using billiardgl::ai::ShotCandidate;

void expect(bool value, const char* message)
{
    if (!value) throw std::runtime_error(message);
}

class SingleCandidateEvaluator final : public PreviewEvaluator {
public:
    PreviewScore evaluate(const SemanticShotAction&,
        std::chrono::steady_clock::time_point) override
    {
        return {true, 1.0f, 0.5f, 0.5f, 0.0f};
    }
};

struct PhysicalScene {
    const char* id = "";
    billiardgl::Point3 cue;
    billiardgl::Point3 object;
    int targetPocket = -1;
    float requestedPower = 0.0f;
};

struct Outcome {
    bool completed = false;
    bool objectPotted = false;
    bool cueSafe = false;
    std::string finalStateSha256;
};

billiardgl::GameState stateFor(const PhysicalScene& scene,
    const billiardgl::PhysicsProfile& profile)
{
    billiardgl::GameState state;
    billiardgl::initializeBalls(state);
    for (auto& ball : state.balls) {
        billiardgl::resetBallMotion(ball);
        billiardgl::resetPocketInteraction(ball);
        ball.pocketed = true;
        ball.physicalDomain = billiardgl::PhysicalBallDomain::ClothSupported;
    }
    const float supportedY = billiardgl::buildTableDepartureGeometry(
        profile).clothSupportedCenterYCm;
    state.balls[0].pocketed = false;
    state.balls[0].position = {scene.cue.x, supportedY, scene.cue.z};
    state.balls[1].pocketed = false;
    state.balls[1].position = {scene.object.x, supportedY, scene.object.z};
    state.pocketedBallCount = billiardgl::kBallCount - 2;
    state.ballsMoving = false;
    billiardgl::clearGameplayEvents(state);
    return state;
}

ShotCandidate idealCandidate(const PhysicalScene& scene,
    const billiardgl::GameState& state,
    const billiardgl::PhysicsProfile& profile)
{
    billiardgl::ai::CandidateGenerationOptions options;
    options.legalTargetBalls = {1};
    options.requestedPowers = {scene.requestedPower};
    options.maximumCandidates = 6;
    const std::vector<ShotCandidate> candidates =
        billiardgl::ai::enumerateTableCandidates(state, profile, options);
    for (const ShotCandidate& candidate : candidates) {
        if (candidate.action.targetBall == 1 &&
            candidate.action.targetPocket == scene.targetPocket)
            return candidate;
    }
    throw std::runtime_error(std::string(scene.id) +
        " has no authoritative target-pocket candidate");
}

Outcome runPhysical(const PhysicalScene& scene, Difficulty difficulty,
    std::uint64_t seed, const billiardgl::PhysicsProfile& profile,
    const billiardgl::ShotControlCapabilities& capabilities)
{
    const billiardgl::GameState state = stateFor(scene, profile);
    const ShotCandidate ideal = idealCandidate(scene, state, profile);
    SingleCandidateEvaluator selection;
    const auto plan = billiardgl::ai::ShotPlanner(difficulty, seed).plan(
        std::vector<ShotCandidate>{ideal}, selection,
        {1, std::chrono::milliseconds(100)});
    if (!plan.found) return {};
    const billiardgl::ShotControlConversion converted =
        billiardgl::convertShotControls(plan.action.controls, capabilities,
            profile);
    if (!converted.ok) return {};
    billiardgl::training::PreviewBudget budget;
    budget.maximumTicks = 2400;
    budget.maximumWallMilliseconds = 10000;
    budget.maximumBalls = billiardgl::kBallCount;
    const billiardgl::training::PreviewResult preview =
        billiardgl::training::PreviewRuntime(state, profile,
            billiardgl::PhysicsBoundaryMode::ProductionTable).run(
                converted.command.cueImpact, budget);
    Outcome result;
    result.completed = preview.ok && !preview.tickBudgetReached &&
        !preview.wallBudgetReached;
    result.objectPotted = result.completed && preview.finalPocketed[1];
    result.cueSafe = result.completed && !preview.finalPocketed[0];
    result.finalStateSha256 = preview.finalStateSha256;
    return result;
}

}  // namespace

int main()
{
    try {
        billiardgl::GameRuntime authority;
        const billiardgl::PhysicsProfile profile =
            billiardgl::interactiveChinesePoolPhysicsProfile();
        const auto capabilities = authority.shotControlCapabilities();
        const std::array<PhysicalScene, 3> scenes{{
            {"long_corner", {-42.0f, 0.0f, -82.0f},
                {0.0f, 0.0f, 0.0f}, 3, 105.0f},
            {"long_side", {-45.0f, 0.0f, -18.0f},
                {8.0f, 0.0f, -4.0f}, 5, 82.0f},
            {"cut_corner", {-48.0f, 0.0f, -58.0f},
                {-6.0f, 0.0f, 18.0f}, 1, 96.0f},
        }};
        constexpr int kSeeds = 16;
        const std::array<Difficulty, 3> difficulties{{Difficulty::Rookie,
            Difficulty::Club, Difficulty::Expert}};
        std::array<int, 3> completed{};
        std::array<int, 3> pots{};
        std::array<int, 3> cueSafePots{};
        std::array<std::array<int, scenes.size()>, difficulties.size()>
            sceneCompleted{};
        std::array<std::array<int, scenes.size()>, difficulties.size()>
            scenePots{};
        std::array<std::array<int, scenes.size()>, difficulties.size()>
            sceneCueSafePots{};
        std::cout << "scope=fixed_single_authoritative_candidate_execution_error_only"
                  << " profile=" << profile.id
                  << " completed_is_not_rules_legal=true\n";
        for (std::size_t level = 0; level < difficulties.size(); ++level) {
            for (std::size_t sceneIndex = 0;
                    sceneIndex < scenes.size(); ++sceneIndex) {
                for (int seed = 0; seed < kSeeds; ++seed) {
                    const std::uint64_t deterministicSeed =
                        static_cast<std::uint64_t>(seed +
                            100 * sceneIndex);
                    const Outcome outcome = runPhysical(scenes[sceneIndex],
                        difficulties[level], deterministicSeed, profile,
                        capabilities);
                    if (outcome.completed) ++completed[level];
                    if (outcome.objectPotted) ++pots[level];
                    if (outcome.objectPotted && outcome.cueSafe)
                        ++cueSafePots[level];
                    if (outcome.completed)
                        ++sceneCompleted[level][sceneIndex];
                    if (outcome.objectPotted)
                        ++scenePots[level][sceneIndex];
                    if (outcome.objectPotted && outcome.cueSafe)
                        ++sceneCueSafePots[level][sceneIndex];
                    if (seed == 0) {
                        const Outcome repeat = runPhysical(
                            scenes[sceneIndex], difficulties[level],
                            deterministicSeed, profile, capabilities);
                        expect(outcome.completed == repeat.completed &&
                            outcome.objectPotted == repeat.objectPotted &&
                            outcome.cueSafe == repeat.cueSafe &&
                            outcome.finalStateSha256 ==
                                repeat.finalStateSha256,
                            "physical AI outcome must repeat exactly for fixed scene and seed");
                    }
                }
                std::cout << billiardgl::ai::difficultyId(
                                 difficulties[level])
                          << " scene=" << scenes[sceneIndex].id
                          << " completed="
                          << sceneCompleted[level][sceneIndex]
                          << '/' << kSeeds << " object_pots="
                          << scenePots[level][sceneIndex]
                          << " cue_safe_pots="
                          << sceneCueSafePots[level][sceneIndex] << '\n';
            }
            std::cout << billiardgl::ai::difficultyId(difficulties[level])
                      << " completed=" << completed[level] << '/' <<
                kSeeds * scenes.size() << " object_pots=" << pots[level]
                      << " cue_safe_pots=" << cueSafePots[level] << '\n';
            expect(completed[level] == kSeeds *
                    static_cast<int>(scenes.size()),
                "every sampled semantic shot must convert and finish within the isolated preview budget");
        }
        expect(cueSafePots[0] < cueSafePots[1] &&
                cueSafePots[1] < cueSafePots[2],
            "the three execution-error profiles must produce a strictly ordered safe-pot count across the complete fixed single-candidate scene set");
        return EXIT_SUCCESS;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return EXIT_FAILURE;
    }
}
