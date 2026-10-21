#include "ai/ai_turn_controller.h"
#include "deterministic_hash.h"
#include "game_runtime.h"
#include "table_departure_geometry.h"

#include <algorithm>
#include <chrono>
#include <cstdlib>
#include <iostream>
#include <stdexcept>
#include <string>
#include <thread>
#include <vector>

namespace {

using namespace billiardgl;

void expect(bool value, const char* message)
{
    if (!value) throw std::runtime_error(message);
}

bool containsEvent(const std::vector<RuntimeEvent>& events,
    const char* name)
{
    return std::any_of(events.begin(), events.end(),
        [&](const RuntimeEvent& event) { return event.name == name; });
}

GameRuntime endgameRuntime()
{
    const PhysicsProfile profile = interactiveChinesePoolPhysicsProfile();
    const TableDepartureGeometry geometry = buildTableDepartureGeometry(profile);
    GameState state;
    initializeBalls(state);
    for (auto& ball : state.balls) {
        resetBallMotion(ball);
        resetPocketInteraction(ball);
        ball.pocketed = true;
    }
    state.balls[0].pocketed = false;
    state.balls[0].position = {-45.0f,
        geometry.clothSupportedCenterYCm, -18.0f};
    state.balls[8].pocketed = false;
    state.balls[8].position = {8.0f,
        geometry.clothSupportedCenterYCm, -4.0f};
    state.pocketedBallCount = kBallCount - 2;
    state.ballsMoving = false;
    clearGameplayEvents(state);

    GameRuntime runtime;
    expect(runtime.replaceStateForScenario(state, profile).ok &&
            runtime.physicsProfile().id == profile.id,
        "shipping interactive endgame state must install");
    expect(runtime.setRulesMode(RulesMode::ChineseEightBallV1).ok,
        "endgame fixture must enable product rules");

    // Public rules actions put the focused physical endgame in a valid
    // GroupsAssigned state. The AI's final stroke below is not synthetic: it
    // runs the public semantic controls, physics, shot-fact collector, and
    // automatic runtime adjudication through match completion.
    rules::ShotFacts legalBreak;
    legalBreak.shooter = 0;
    legalBreak.breakShot = true;
    legalBreak.firstContactBall = 1;
    legalBreak.distinctObjectBallsToRail = 4;
    expect(runtime.adjudicateRulesShot(legalBreak).ok &&
            runtime.matchState().phase == rules::MatchPhase::OpenTable &&
            runtime.matchState().currentPlayer == 1,
        "public rules authority must advance the legal setup break");

    rules::ShotFacts assignSolids;
    assignSolids.shooter = 1;
    assignSolids.firstContactBall = 1;
    assignSolids.anyRailAfterFirstContact = true;
    assignSolids.pocketedBallsInOrder = {1};
    expect(runtime.adjudicateRulesShot(assignSolids).ok &&
            runtime.matchState().phase == rules::MatchPhase::GroupsAssigned &&
            runtime.matchState().groups[1] == rules::BallGroup::Solids &&
            runtime.matchState().groups[0] == rules::BallGroup::Stripes &&
            runtime.matchState().currentPlayer == 1,
        "public rules authority must assign the AI group and retain its turn");
    runtime.clearEvents();
    return runtime;
}

ai::TurnResult await(ai::AiTurnController& controller, GameRuntime& runtime)
{
    const auto deadline = std::chrono::steady_clock::now() +
        std::chrono::seconds(3);
    for (;;) {
        const ai::TurnResult result = controller.pollAndApply(runtime);
        if (result.status != ai::TurnPollStatus::Thinking) return result;
        if (std::chrono::steady_clock::now() >= deadline)
            throw std::runtime_error("AI planning timed out");
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }
}

struct EndgameResult {
    ai::PlanResult plan;
    rules::ShotFacts facts;
    rules::AdjudicationResult adjudication;
    rules::MatchState match;
    std::vector<AppliedSemanticAction> actions;
    std::vector<RuntimeEvent> events;
    std::string stateSha256;
    int ticks = 0;
    bool stateGameOver = false;
    bool placementPending = false;
    rules::BallInHandPlacementScope placementScope =
        rules::BallInHandPlacementScope::None;
};

EndgameResult runEndgame(std::uint64_t seed)
{
    GameRuntime runtime = endgameRuntime();
    ai::AiTurnController controller;
    expect(controller.begin(runtime, ai::Difficulty::Expert, seed,
            {24, std::chrono::milliseconds(500)}),
        "AI must begin from the stopped authoritative endgame");
    const ai::TurnResult applied = await(controller, runtime);
    expect(applied.status == ai::TurnPollStatus::Applied &&
            applied.plan.found && applied.plan.candidatesEvaluated == 24 &&
            !applied.plan.wallTimeBudgetReached &&
            applied.plan.action.targetBall == 8 &&
            runtime.state().ballsMoving && runtime.shotFactCollectorActive(),
        "AI must select the 8-ball and apply one public semantic shot");

    int ticks = 0;
    while (runtime.state().ballsMoving && ticks < 2400) {
        expect(runtime.step(1).ok,
            "shipping runtime must advance every AI physics tick");
        ++ticks;
    }
    expect(ticks > 0 && ticks < 2400 && !runtime.state().ballsMoving &&
            runtime.hasLastShotFacts() && runtime.hasLastAdjudication(),
        "AI stroke must stop and reach automatic rules adjudication");

    EndgameResult result;
    result.plan = applied.plan;
    result.facts = runtime.lastShotFacts();
    result.adjudication = runtime.lastAdjudication();
    result.match = runtime.matchState();
    result.actions.assign(runtime.appliedSemanticActions().begin(),
        runtime.appliedSemanticActions().end());
    result.events = runtime.events();
    result.stateSha256 = gameStateSha256(runtime.state());
    result.ticks = ticks;
    result.stateGameOver = runtime.state().gameOver;
    result.placementPending = runtime.ballInHandPlacementPending();
    result.placementScope = runtime.ballInHandPlacementScope();
    return result;
}

void expectPublicActionStream(const EndgameResult& result)
{
    const AppliedSemanticActionType expected[] = {
        AppliedSemanticActionType::SetAimYaw,
        AppliedSemanticActionType::SetShotPower,
        AppliedSemanticActionType::SetTipOffset,
        AppliedSemanticActionType::SetCalledPocket,
        AppliedSemanticActionType::Shoot,
    };
    expect(result.actions.size() == 5,
        "AI must emit exactly five public semantic operations");
    for (std::size_t index = 0; index < result.actions.size(); ++index)
        expect(result.actions[index].type == expected[index],
            "AI public semantic operations must keep canonical order");
    expect(result.facts.calledPocket == result.plan.action.targetPocket,
        "shot facts must bind the called pocket from the public setter");
}

}  // namespace

int main()
{
    try {
        const EndgameResult firstWin = runEndgame(0);
        expectPublicActionStream(firstWin);
        std::cout << "ai_rules_seed0 power="
                  << firstWin.plan.action.controls.requestedPower
                  << " yaw=" << firstWin.plan.action.controls.yawRadians
                  << " pocket=" << firstWin.plan.action.targetPocket
                  << " first=" << firstWin.facts.firstContactBall
                  << " rails=" << firstWin.facts.anyRailAfterFirstContact
                  << " potted=" << firstWin.facts.pocketedBallsInOrder.size()
                  << " foul=" << firstWin.adjudication.foul
                  << " game_over=" << firstWin.adjudication.gameOver << '\n';
        expect(firstWin.facts.shooter == 1 && !firstWin.facts.breakShot &&
                firstWin.facts.firstContactBall == 8 &&
                firstWin.facts.pocketedBallsInOrder ==
                    std::vector<int>{8} &&
                !firstWin.facts.cueBallPocketed &&
                !firstWin.facts.cueBallOffTable &&
                firstWin.facts.eightBallPocket ==
                    firstWin.plan.action.targetPocket &&
                firstWin.adjudication.accepted &&
                !firstWin.adjudication.foul &&
                firstWin.adjudication.gameOver &&
                firstWin.adjudication.winner == 1 &&
                firstWin.match.phase == rules::MatchPhase::Complete &&
                firstWin.stateGameOver &&
                containsEvent(firstWin.events, "rules_adjudicated") &&
                containsEvent(firstWin.events, "rules_game_over") &&
                !containsEvent(firstWin.events, "rules_foul"),
            "rule-aware preview must find a legal called 8-ball win instead of a predictable no-rail foul");

        const EndgameResult win = runEndgame(1);
        expectPublicActionStream(win);
        if (!win.adjudication.gameOver || win.adjudication.foul) {
            std::cerr << "called8 diagnostic power=" << win.plan.action.controls.requestedPower
                      << " yaw=" << win.plan.action.controls.yawRadians
                      << " targetPocket=" << win.plan.action.targetPocket
                      << " actualPocket=" << win.facts.eightBallPocket
                      << " first=" << win.facts.firstContactBall
                      << " scratch=" << win.facts.cueBallPocketed
                      << " offTable=" << win.facts.cueBallOffTable
                      << " ticks=" << win.ticks << " pots=";
            for (int ball : win.facts.pocketedBallsInOrder) std::cerr << ball << ',';
            std::cerr << " reasons=";
            for (auto reason : win.adjudication.reasons)
                std::cerr << rules::foulReasonCode(reason) << ',';
            std::cerr << '\n';
        }
        expect(win.facts.shooter == 1 && !win.facts.breakShot &&
                win.facts.firstContactBall == 8 &&
                win.facts.pocketedBallsInOrder == std::vector<int>{8} &&
                win.facts.solidsRemainingBefore == 0 &&
                !win.facts.cueBallPocketed && !win.facts.cueBallOffTable &&
                win.facts.eightBallPocket == win.plan.action.targetPocket &&
                win.adjudication.accepted && !win.adjudication.foul &&
                win.adjudication.reasons.empty() &&
                win.adjudication.gameOver && win.adjudication.winner == 1 &&
                win.match.phase == rules::MatchPhase::Complete &&
                win.match.winner == 1 && win.stateGameOver &&
                !win.placementPending &&
                containsEvent(win.events, "rules_adjudicated") &&
                containsEvent(win.events, "rules_game_over") &&
                !containsEvent(win.events, "rules_foul"),
            "real called 8-ball pot must complete the match as an accepted legal win");

        const EndgameResult repeat = runEndgame(1);
        expect(repeat.stateSha256 == win.stateSha256 &&
                repeat.ticks == win.ticks &&
                repeat.plan.action.targetPocket ==
                    win.plan.action.targetPocket &&
                repeat.plan.action.controls.yawRadians ==
                    win.plan.action.controls.yawRadians &&
                repeat.plan.action.controls.requestedPower ==
                    win.plan.action.controls.requestedPower &&
                repeat.facts.pocketedBallsInOrder ==
                    win.facts.pocketedBallsInOrder &&
                repeat.adjudication.winner == win.adjudication.winner,
            "fixed AI rules endgame must repeat exactly");

        std::cout << "ai_rules_runtime profile="
                  << interactiveChinesePoolPhysicsProfile().id
                  << " first_win_seed=0 pocket="
                  << firstWin.plan.action.targetPocket
                  << " win_seed=1 pocket=" << win.plan.action.targetPocket
                  << " ticks=" << win.ticks << " state="
                  << win.stateSha256 << '\n';
        return EXIT_SUCCESS;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return EXIT_FAILURE;
    }
}
