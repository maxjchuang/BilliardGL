#include "ai/shot_planner.h"

#include <algorithm>
#include <cmath>
#include <limits>

namespace billiardgl::ai {
namespace {

float clamp01(float value) noexcept
{
    return std::max(0.0f, std::min(1.0f, value));
}

std::uint64_t mix(std::uint64_t value) noexcept
{
    value += 0x9e3779b97f4a7c15ull;
    value = (value ^ (value >> 30)) * 0xbf58476d1ce4e5b9ull;
    value = (value ^ (value >> 27)) * 0x94d049bb133111ebull;
    return value ^ (value >> 31);
}

float signedNoise(std::uint64_t seed, std::size_t index) noexcept
{
    const std::uint64_t bits = mix(seed ^ (static_cast<std::uint64_t>(index) + 1));
    const double unit = static_cast<double>(bits >> 11) * (1.0 / 9007199254740992.0);
    return static_cast<float>(unit * 2.0 - 1.0);
}

float noiseAmplitude(Difficulty difficulty) noexcept
{
    switch (difficulty) {
    case Difficulty::Rookie: return 0.34f;
    case Difficulty::Club: return 0.12f;
    case Difficulty::Expert: return 0.015f;
    }
    return 0.0f;
}

void applyExecutionError(SemanticShotAction& action, Difficulty difficulty,
    std::uint64_t seed, std::size_t candidateIndex) noexcept
{
    const DifficultyProfile profile = difficultyProfile(difficulty);
    const std::uint64_t base = seed ^
        (static_cast<std::uint64_t>(candidateIndex) * 0xd6e8feb86659fd93ull);
    action.controls.yawRadians += signedNoise(base ^ 0x9f6abc1dull, 0) *
        profile.maximumAimErrorRadians;
    const float powerScale = 1.0f + signedNoise(base ^ 0x714e2c31ull, 1) *
        profile.maximumPowerErrorFraction;
    if (action.controls.powerMode == ShotPowerMode::RequestedPower)
        action.controls.requestedPower = std::clamp(
            action.controls.requestedPower * powerScale, 0.0f, 200.0f);
    else
        action.controls.strokeSpeedCmS = std::clamp(
            action.controls.strokeSpeedCmS * powerScale, 0.0f, 2500.0f);
    action.controls.tipOffsetX = std::clamp(action.controls.tipOffsetX +
        signedNoise(base ^ 0x51b42a77ull, 2) * profile.maximumTipErrorRadius,
        -0.45f, 0.45f);
    action.controls.tipOffsetY = std::clamp(action.controls.tipOffsetY +
        signedNoise(base ^ 0x2e9d890full, 3) * profile.maximumTipErrorRadius,
        -0.45f, 0.45f);
}

float score(const ShotCandidate& candidate, const PreviewScore& preview) noexcept
{
    return 0.58f * clamp01(preview.potProbability) +
           0.17f * clamp01(preview.cuePositionQuality) +
           0.12f * clamp01(preview.nextShotQuality) +
           0.08f * clamp01(candidate.visibility) -
           0.55f * clamp01(preview.foulProbability);
}

}  // namespace

PlanningBudget defaultBudget(Difficulty difficulty) noexcept
{
    switch (difficulty) {
    case Difficulty::Rookie: return {24, std::chrono::milliseconds(500)};
    case Difficulty::Club: return {48, std::chrono::milliseconds(1000)};
    case Difficulty::Expert: return {96, std::chrono::milliseconds(2000)};
    }
    return {};
}

DifficultyProfile difficultyProfile(Difficulty difficulty) noexcept
{
    // Uniform deterministic execution error. The values are calibrated by the
    // fixed 600-case acceptance suite, not by accessing hidden solver state.
    switch (difficulty) {
    case Difficulty::Rookie: return {0.0465f, 0.085f, 0.055f};
    case Difficulty::Club: return {0.0310f, 0.040f, 0.025f};
    case Difficulty::Expert: return {0.0240f, 0.012f, 0.008f};
    }
    return {};
}

const char* difficultyId(Difficulty difficulty) noexcept
{
    switch (difficulty) {
    case Difficulty::Rookie: return "rookie";
    case Difficulty::Club: return "club";
    case Difficulty::Expert: return "expert";
    }
    return "unknown";
}

ShotPlanner::ShotPlanner(Difficulty difficulty, std::uint64_t deterministicSeed)
    : difficulty_(difficulty), seed_(deterministicSeed)
{
}

PlanResult ShotPlanner::plan(const std::vector<ShotCandidate>& candidates,
                             PreviewEvaluator& evaluator) const
{
    return plan(candidates, evaluator, defaultBudget(difficulty_));
}

PlanResult ShotPlanner::plan(const std::vector<ShotCandidate>& candidates,
                             PreviewEvaluator& evaluator,
                             PlanningBudget budget,
                             const std::atomic_bool* cancellation) const
{
    PlanResult result;
    if (budget.maximumCandidates == 0 || budget.maximumWallTime.count() <= 0) {
        result.error = "invalid_planning_budget";
        return result;
    }
    const auto start = std::chrono::steady_clock::now();
    const auto deadline = start + budget.maximumWallTime;
    const std::size_t limit = std::min(candidates.size(), budget.maximumCandidates);
    float bestScore = -std::numeric_limits<float>::infinity();
    for (std::size_t index = 0; index < limit; ++index) {
        if (cancellation && cancellation->load(std::memory_order_relaxed)) {
            result.error = "planning_cancelled";
            return result;
        }
        if (std::chrono::steady_clock::now() >= deadline) {
            result.wallTimeBudgetReached = true;
            break;
        }
        // Difficulty noise is part of the action that will actually be sent
        // to GameRuntime, so preview that executed action rather than scoring
        // an ideal line and perturbing it only after selection. The latter can
        // select a nominal pot whose real seeded stroke is a known scratch or
        // no-rail foul.
        ShotCandidate executed = candidates[index];
        applyExecutionError(executed.action, difficulty_, seed_, index);
        const PreviewScore preview = evaluator.evaluate(executed.action, deadline);
        if (cancellation && cancellation->load(std::memory_order_relaxed)) {
            result.error = "planning_cancelled";
            return result;
        }
        ++result.candidatesEvaluated;
        if (!preview.valid || !std::isfinite(preview.potProbability) ||
            !std::isfinite(preview.cuePositionQuality) ||
            !std::isfinite(preview.nextShotQuality) ||
            !std::isfinite(preview.foulProbability)) continue;
        const float evaluated = score(executed, preview) +
            signedNoise(seed_, index) * noiseAmplitude(difficulty_);
        if (!result.found || evaluated > bestScore) {
            result.found = true;
            result.action = executed.action;
            result.score = evaluated;
            bestScore = evaluated;
        }
    }
    result.candidateBudgetReached = candidates.size() > limit;
    if (std::chrono::steady_clock::now() >= deadline &&
        result.candidatesEvaluated < limit) result.wallTimeBudgetReached = true;
    result.elapsedMilliseconds = std::chrono::duration<double, std::milli>(
        std::chrono::steady_clock::now() - start).count();
    if (!result.found && result.error.empty()) result.error = "no_valid_candidate";
    return result;
}

}  // namespace billiardgl::ai
