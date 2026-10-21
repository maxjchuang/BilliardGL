#include "ai/game_preview_evaluator.h"
#include "deterministic_hash.h"
#include "pocket_boundary.h"
#include "table_departure_geometry.h"

#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdlib>
#include <iostream>
#include <stdexcept>

namespace {

void expect(bool value, const char* message)
{
    if (!value) throw std::runtime_error(message);
}

}  // namespace

int main()
{
    try {
        billiardgl::GameRuntime authority;
        const std::string stateHash = billiardgl::gameStateSha256(authority.state());
        const std::uint64_t tick = authority.tick();
        const std::size_t events = authority.events().size();

        billiardgl::ai::CandidateGenerationOptions options;
        options.legalTargetBalls = {1, 2, 3};
        options.maximumCandidates = 10;
        const auto candidates = billiardgl::ai::enumerateTableCandidates(
            authority.state(), authority.physicsProfile(), options);
        expect(candidates.size() == 10 && candidates.front().action.targetBall == 1 &&
            candidates.front().action.targetPocket >= 0 &&
            candidates.front().action.controls.schemaVersion ==
                billiardgl::ShotControlSchemaVersion::IntentV2,
            "AI enumerates bounded public semantic target/pocket actions");

        billiardgl::GameState geometryState = authority.state();
        geometryState.balls[0].position = {0.0f,
            authority.physicsProfile().ball.radiusCm, 0.0f};
        geometryState.balls[1].position = {12.0f,
            authority.physicsProfile().ball.radiusCm, 24.0f};
        geometryState.balls[0].pocketed = false;
        geometryState.balls[1].pocketed = false;
        billiardgl::ai::CandidateGenerationOptions geometryOptions;
        geometryOptions.legalTargetBalls = {1};
        geometryOptions.requestedPowers = {64.0f};
        geometryOptions.maximumCandidates = 6;
        const auto geometryCandidates =
            billiardgl::ai::enumerateTableCandidates(geometryState,
                authority.physicsProfile(), geometryOptions);
        const auto authoritativePockets =
            billiardgl::buildPocketBoundaryFrames(
                authority.physicsProfile().tableBoundary);
        expect(geometryCandidates.size() == authoritativePockets.size(),
            "AI emits exactly one candidate per authoritative pocket for one power");
        for (std::size_t index = 0;
                index < geometryCandidates.size(); ++index) {
            const auto& frame = authoritativePockets[index];
            const auto& candidate = geometryCandidates[index];
            const auto& cue = geometryState.balls[0].position;
            const auto& object = geometryState.balls[1].position;
            const float pocketDx = frame.mouthCenter.x - object.x;
            const float pocketDz = frame.mouthCenter.z - object.z;
            const float pocketDistance = std::hypot(pocketDx, pocketDz);
            const float ghostX = object.x - pocketDx / pocketDistance *
                (2.0f * authority.physicsProfile().ball.radiusCm);
            const float ghostZ = object.z - pocketDz / pocketDistance *
                (2.0f * authority.physicsProfile().ball.radiusCm);
            const float expectedYaw = std::atan2(
                ghostZ - cue.z, ghostX - cue.x);
            expect(candidate.action.targetBall == 1 &&
                candidate.action.targetPocket == frame.pocketId &&
                std::fabs(candidate.action.controls.yawRadians -
                    expectedYaw) <= 1e-7f,
                "AI targetPocket and ghost aim must use the matching authoritative pocket frame");
        }
        billiardgl::GameState removedTarget = geometryState;
        removedTarget.balls[1].outOfPlay = true;
        expect(billiardgl::ai::enumerateTableCandidates(removedTarget,
                   authority.physicsProfile(), geometryOptions).empty(),
            "AI cannot target an off-table ball removed from active play");
        removedTarget = geometryState;
        removedTarget.balls[0].outOfPlay = true;
        expect(billiardgl::ai::enumerateTableCandidates(removedTarget,
                   authority.physicsProfile(), geometryOptions).empty(),
            "AI cannot plan from an off-table cue ball before authority respots it");

        billiardgl::training::PreviewBudget budget;
        budget.maximumTicks = 24;
        budget.maximumWallMilliseconds = 200;
        budget.maximumBalls = billiardgl::kBallCount;
        billiardgl::ai::GamePreviewEvaluator evaluator(authority, budget);
        const auto score = evaluator.evaluate(candidates.front().action,
            std::chrono::steady_clock::now() + std::chrono::milliseconds(250));
        expect(!score.valid,
            "AI evaluator rejects a bounded preview that never reaches its intended object ball");
        expect(stateHash == billiardgl::gameStateSha256(authority.state()) &&
            tick == authority.tick() && events == authority.events().size(),
            "AI preview cannot mutate authoritative state, tick, or events");

        const billiardgl::PhysicsProfile shippingProfile =
            billiardgl::interactiveChinesePoolPhysicsProfile();
        const float supportedY = billiardgl::buildTableDepartureGeometry(
            shippingProfile).clothSupportedCenterYCm;
        billiardgl::GameState reachableState;
        billiardgl::initializeBalls(reachableState);
        for (auto& ball : reachableState.balls) {
            billiardgl::resetBallMotion(ball);
            billiardgl::resetPocketInteraction(ball);
            ball.pocketed = true;
            ball.physicalDomain =
                billiardgl::PhysicalBallDomain::ClothSupported;
        }
        reachableState.balls[0].pocketed = false;
        reachableState.balls[0].position = {-45.0f, supportedY, -18.0f};
        reachableState.balls[1].pocketed = false;
        reachableState.balls[1].position = {8.0f, supportedY, -4.0f};
        reachableState.pocketedBallCount = billiardgl::kBallCount - 2;
        reachableState.ballsMoving = false;
        billiardgl::clearGameplayEvents(reachableState);
        billiardgl::GameRuntime reachableAuthority;
        expect(reachableAuthority.replaceStateForScenario(
                   reachableState, shippingProfile).ok,
            "reachable preview fixture installs the shipping physics profile");
        billiardgl::ai::CandidateGenerationOptions reachableOptions;
        reachableOptions.legalTargetBalls = {1};
        reachableOptions.requestedPowers = {98.0f};
        reachableOptions.maximumCandidates = 6;
        const auto reachableCandidates =
            billiardgl::ai::enumerateTableCandidates(reachableState,
                shippingProfile, reachableOptions);
        const auto reachable = std::find_if(reachableCandidates.begin(),
            reachableCandidates.end(), [](const auto& candidate) {
                return candidate.action.targetPocket == 5;
            });
        expect(reachable != reachableCandidates.end(),
            "reachable preview fixture binds an authoritative side-pocket candidate");
        billiardgl::training::PreviewBudget reachableBudget;
        reachableBudget.maximumTicks = 1200;
        reachableBudget.maximumWallMilliseconds = 2000;
        reachableBudget.maximumBalls = billiardgl::kBallCount;
        billiardgl::ai::GamePreviewEvaluator reachableEvaluator(
            reachableAuthority, reachableBudget);
        const auto reachableScore = reachableEvaluator.evaluate(
            reachable->action, std::chrono::steady_clock::now() +
                std::chrono::milliseconds(2200));
        expect(reachableScore.valid &&
            reachableScore.potProbability >= 0.0f &&
            reachableScore.potProbability <= 1.0f &&
            reachableScore.foulProbability >= 0.0f &&
            reachableScore.foulProbability <= 1.0f,
            "AI evaluator accepts a shipping-profile preview only after the intended object ball moves");

        const auto noRail = std::find_if(reachableCandidates.begin(),
            reachableCandidates.end(), [](const auto& candidate) {
                return candidate.action.targetPocket == 1;
            });
        expect(noRail != reachableCandidates.end(),
            "no-rail preview fixture binds the requested authoritative pocket");
        billiardgl::ai::SemanticShotAction noRailAction = noRail->action;
        noRailAction.controls.requestedPower = 42.0f;
        const auto noRailScore = reachableEvaluator.evaluate(noRailAction,
            std::chrono::steady_clock::now() +
                std::chrono::milliseconds(2200));
        expect(!noRailScore.valid,
            "AI evaluator rejects a real first-contact shot with neither a pot nor a rail after contact");

        billiardgl::ai::SemanticShotAction invalid = candidates.front().action;
        invalid.controls.requestedPower = 500.0f;
        expect(!evaluator.evaluate(invalid,
            std::chrono::steady_clock::now() + std::chrono::milliseconds(50)).valid,
            "AI cannot bypass player shot-control validation");
        return EXIT_SUCCESS;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return EXIT_FAILURE;
    }
}
