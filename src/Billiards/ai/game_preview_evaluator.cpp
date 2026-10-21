#include "ai/game_preview_evaluator.h"
#include "pocket_boundary.h"

#include <algorithm>
#include <cmath>

namespace billiardgl::ai {
namespace {

float clamp01(float value)
{
    return std::max(0.0f, std::min(1.0f, value));
}

}  // namespace

std::vector<ShotCandidate> enumerateTableCandidates(
    const GameState& state, const PhysicsProfile& profile,
    const CandidateGenerationOptions& options)
{
    std::vector<ShotCandidate> result;
    if (state.balls[0].pocketed || state.balls[0].outOfPlay ||
        options.maximumCandidates == 0) return result;
    const auto pocketFrames = buildPocketBoundaryFrames(
        profile.tableBoundary);
    const Point3 cue = state.balls[0].position;
    for (int target : options.legalTargetBalls) {
        if (target <= 0 || target >= kBallCount ||
            state.balls[target].pocketed || state.balls[target].outOfPlay)
            continue;
        const Point3 ball = state.balls[target].position;
        for (const PocketBoundaryFrame& pocket : pocketFrames) {
            const float pocketDx = pocket.mouthCenter.x - ball.x;
            const float pocketDz = pocket.mouthCenter.z - ball.z;
            const float pocketDistance = std::hypot(pocketDx, pocketDz);
            if (pocketDistance < 0.001f) continue;
            const float ghostX = ball.x - pocketDx / pocketDistance *
                (2.0f * profile.ball.radiusCm);
            const float ghostZ = ball.z - pocketDz / pocketDistance *
                (2.0f * profile.ball.radiusCm);
            const float cueDistance = std::hypot(ghostX - cue.x, ghostZ - cue.z);
            if (cueDistance < 0.001f) continue;
            for (float power : options.requestedPowers) {
                if (!std::isfinite(power) || power < 0.0f || power > 200.0f) continue;
                ShotCandidate candidate;
                candidate.action.targetBall = target;
                candidate.action.targetPocket = pocket.pocketId;
                candidate.action.controls.schemaVersion =
                    ShotControlSchemaVersion::IntentV2;
                candidate.action.controls.yawRadians =
                    std::atan2(ghostZ - cue.z, ghostX - cue.x);
                candidate.action.controls.requestedPower = power;
                candidate.visibility = clamp01(1.0f - cueDistance /
                    profile.tableBoundary.playfieldLengthCm);
                result.push_back(candidate);
                if (result.size() >= options.maximumCandidates) return result;
            }
        }
    }
    return result;
}

GamePreviewEvaluator::GamePreviewEvaluator(const GameRuntime& authority,
    training::PreviewBudget budget)
    : state_(authority.state()), profile_(authority.physicsProfile()),
      boundaryMode_(authority.boundaryMode()),
      capabilities_(authority.shotControlCapabilities()), budget_(budget)
{
}

PreviewScore GamePreviewEvaluator::evaluate(const SemanticShotAction& action,
    std::chrono::steady_clock::time_point deadline)
{
    PreviewScore score;
    const auto remaining = std::chrono::duration_cast<std::chrono::milliseconds>(
        deadline - std::chrono::steady_clock::now()).count();
    if (remaining <= 0) return score;
    training::PreviewBudget bounded = budget_;
    bounded.maximumWallMilliseconds = std::max(1,
        std::min(bounded.maximumWallMilliseconds, static_cast<int>(remaining)));
    const ShotControlConversion converted = convertShotControls(
        action.controls, capabilities_, profile_);
    if (!converted.ok || action.targetBall <= 0 || action.targetBall >= kBallCount)
        return score;
    const training::PreviewResult preview = training::PreviewRuntime(
        state_, profile_, boundaryMode_, true).run(
            converted.command.cueImpact, bounded);
    if (!preview.ok || !preview.hasShotFacts) return score;

    const Point3 targetBefore = state_.balls[action.targetBall].position;
    const Point3 targetAfter = preview.finalPositions[action.targetBall];
    const float targetTravelCm = std::hypot(
        targetAfter.x - targetBefore.x,
        targetAfter.z - targetBefore.z);
    // A completed sandbox run is not automatically a viable shot.  If the
    // intended object ball was neither pocketed nor moved beyond the solver's
    // sealed positional resolution, the candidate did not reach its target.
    // Marking that preview valid made short no-contact rolls outscore every
    // unevaluated candidate under the product wall-time budget.
    const rules::ShotFacts& facts = preview.shotFacts;
    if ((!preview.finalPocketed[action.targetBall] &&
            targetTravelCm <= profile_.solver.penetrationSlopCm) ||
        facts.firstContactBall != action.targetBall ||
        (facts.pocketedBallsInOrder.empty() &&
            !facts.anyRailAfterFirstContact))
        return score;

    score.valid = true;
    score.potProbability = preview.finalPocketed[action.targetBall] ? 1.0f : 0.0f;
    score.foulProbability = facts.cueBallPocketed || facts.cueBallOffTable ?
        1.0f : 0.0f;
    const Point3 cue = preview.finalPositions[0];
    const float halfWidth = profile_.tableBoundary.playfieldWidthCm * 0.5f;
    const float halfLength = profile_.tableBoundary.playfieldLengthCm * 0.5f;
    const float normalizedRadius = std::hypot(cue.x / halfWidth, cue.z / halfLength);
    score.cuePositionQuality = preview.finalPocketed[0] ? 0.0f :
        clamp01(1.0f - normalizedRadius * 0.5f);
    int remainingObjects = 0;
    for (int index = 1; index < kBallCount; ++index)
        if (!preview.finalPocketed[index] && !state_.balls[index].outOfPlay &&
            index != 8)
            ++remainingObjects;
    score.nextShotQuality = clamp01(1.0f - remainingObjects / 14.0f);
    return score;
}

}  // namespace billiardgl::ai
