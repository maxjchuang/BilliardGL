#include "ai/ai_turn_controller.h"
#include "ai/ai_player_controller.h"
#include "automation_controller.h"
#include "replay/replay_recorder.h"
#include "replay/replay_transport_model.h"

#include <algorithm>
#include <chrono>
#include <cstdlib>
#include <iostream>
#include <stdexcept>
#include <thread>
#include <vector>

namespace {

void expect(bool value, const char* message)
{
    if (!value) throw std::runtime_error(message);
}

billiardgl::ai::TurnResult await(
    billiardgl::ai::AiTurnController& controller,
    billiardgl::GameRuntime& runtime,
    std::vector<double>* pollDurationsMilliseconds = nullptr)
{
    const auto deadline = std::chrono::steady_clock::now() +
        std::chrono::seconds(4);
    billiardgl::ai::TurnResult result;
    do {
        const auto pollStarted = std::chrono::steady_clock::now();
        result = controller.pollAndApply(runtime);
        if (pollDurationsMilliseconds) {
            pollDurationsMilliseconds->push_back(
                std::chrono::duration<double, std::milli>(
                    std::chrono::steady_clock::now() - pollStarted).count());
        }
        if (result.status != billiardgl::ai::TurnPollStatus::Thinking) return result;
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
    } while (std::chrono::steady_clock::now() < deadline);
    throw std::runtime_error("AI turn worker timed out");
}

billiardgl::ControllerResult send(
    billiardgl::AutomationController& controller, int id,
    const std::string& command,
    const billiardgl::json::Value& params = billiardgl::json::Value::object())
{
    billiardgl::AutomationRequest request;
    request.id = id;
    request.version = 1;
    request.command = command;
    request.params = params;
    return controller.handle(request);
}

void attestStraightBreakCushions(
    billiardgl::rules::ShotFacts& facts,
    std::initializer_list<int> objectBallIds)
{
    using namespace billiardgl::rules;
    facts.breakEvidenceOrigin =
        BreakEvidenceOrigin::AuthoritativeShotFactCollector;
    facts.breakEvidenceComplete = true;
    facts.breakObjectCushionContacts.clear();
    int ordinal = 0;
    for (int ball : objectBallIds) {
        BreakObjectCushionContact contact;
        contact.objectBallId = ball;
        contact.boundaryKind =
            BreakCushionBoundaryKind::StraightCushion;
        contact.provenance =
            BreakCushionContactProvenance::ContinuousSolver;
        contact.velocityImpulseApplied = true;
        contact.normalImpulseNs = 0.1;
        contact.solverResidualCmS = 0.001;
        contact.solverEventId = 100 + ordinal;
        contact.solverIslandId = 200 + ordinal;
        contact.physicsTick = 10;
        contact.canonicalContactOrdinal = ordinal++;
        facts.breakObjectCushionContacts.push_back(contact);
    }
    facts.distinctObjectBallsToRail = ordinal;
}

void testNativeWeakBreakDecision(const billiardgl::PhysicsProfile& profile)
{
    // Exercise the authority path from a completed native stroke. A weak but
    // successfully applied cue contact is an illegal break, not an
    // imported/no-rack-contact classification gap.
    billiardgl::GameRuntime runtime;
    expect(runtime.replaceStateForScenario(runtime.state(), profile).ok &&
            runtime.setRulesMode(
                billiardgl::RulesMode::ChineseEightBallV1).ok,
        "physical weak-break fixture installs shipping rules and physics");
    billiardgl::GameAction yaw;
    yaw.type = billiardgl::ActionType::SetAimYaw;
    yaw.first = 0.0f;
    billiardgl::GameAction power;
    power.type = billiardgl::ActionType::SetShotPower;
    power.first = 10.0f;
    billiardgl::GameAction shoot;
    shoot.type = billiardgl::ActionType::Shoot;
    expect(runtime.dispatch(yaw).ok && runtime.dispatch(power).ok &&
            runtime.setTipOffset(0.0f, 0.0f).ok &&
            runtime.setCalledPocket(-1).ok && runtime.dispatch(shoot).ok &&
            runtime.hasCueContactResult() &&
            runtime.cueContactResult().applied &&
            runtime.cueContactResult().normalImpulseNs > 0.0,
        "physical weak break starts from a positive native cue impulse");
    int ticks = 0;
    while (runtime.state().ballsMoving && ticks < 5000) {
        const auto step = runtime.step(1);
        if (!step.ok) {
            throw std::runtime_error(std::string(
                "physical weak break failed while settling: ") +
                step.errorCode + ':' +
                runtime.tableDepartureObservationError() + ':' +
                runtime.rulesShotCompletionError());
        }
        ++ticks;
    }
    expect(!runtime.state().ballsMoving && ticks < 5000 &&
            runtime.rulesShotCompletionError().empty() &&
            runtime.hasLastShotFacts() &&
            runtime.lastShotFacts().breakShot &&
            runtime.hasLastAdjudication() &&
            runtime.lastAdjudication().foul &&
            runtime.breakFoulDecisionPending() &&
            runtime.breakFoulDecisionOffer().has_value(),
        "completed native weak break reaches the real illegal-break decision offer");
    const auto offer = *runtime.breakFoulDecisionOffer();
    billiardgl::ai::AiPlayerController controller;
    billiardgl::ai::PlayerConfiguration configuration;
    configuration.enabled = true;
    configuration.player = offer.actorPlayerId;
    configuration.difficulty = billiardgl::ai::Difficulty::Rookie;
    configuration.deterministicSeed = 0x5745414bull;
    expect(controller.configure(runtime, configuration).ok,
        "product AI configures for the native weak-break offer actor");
    const std::string offerId = offer.decisionId;
    const std::uint64_t offerRevision = offer.authorityStateRevision;
    const auto decision = controller.poll(runtime);
    expect(decision.status ==
                billiardgl::ai::PlayerPollStatus::WaitingForTurn &&
            decision.error.empty() && decision.jobsStarted == 0 &&
            !runtime.breakFoulDecisionPending() &&
            runtime.rulesShotCompletionError().empty(),
        "product AI consumes the actor-bound native weak-break decision without planning a shot");
    std::cout << "ai_native_weak_break ticks=" << ticks
              << " actor=" << offer.actorPlayerId
              << " offer=" << offerId
              << " revision=" << offerRevision
              << " foul=" << runtime.lastAdjudication().foul << '\n';
}

}  // namespace

int main()
{
    try {
        {
            billiardgl::GameRuntime reconfigureAuthority;
            billiardgl::ai::AiPlayerController reconfigurable;
            billiardgl::ai::PlayerConfiguration enabled;
            enabled.enabled = true;
            enabled.player = 0;
            enabled.difficulty = billiardgl::ai::Difficulty::Expert;
            expect(reconfigurable.configure(
                       reconfigureAuthority, enabled).ok,
                "product AI configures before cancellation fixture");
            const auto thinking = reconfigurable.poll(reconfigureAuthority);
            expect(thinking.thinking,
                "product AI cancellation fixture starts background planning");
            auto disabled = enabled;
            disabled.enabled = false;
            const auto configureStarted = std::chrono::steady_clock::now();
            const auto disabledResult = reconfigurable.configure(
                reconfigureAuthority, disabled);
            const double configureMilliseconds =
                std::chrono::duration<double, std::milli>(
                    std::chrono::steady_clock::now() -
                    configureStarted).count();
            expect(disabledResult.ok &&
                    reconfigurable.state().status ==
                        billiardgl::ai::PlayerPollStatus::Disabled &&
                    !reconfigurable.state().thinking &&
                    configureMilliseconds < 25.0,
                "product AI reconfiguration cancels in-flight planning without joining the worker");
            std::cout << "ai_reconfigure_cancel configure_ms="
                      << configureMilliseconds << '\n';
        }

        billiardgl::GameRuntime runtime;
        const billiardgl::PhysicsProfile shippingProfile =
            billiardgl::interactiveChinesePoolPhysicsProfile();
        testNativeWeakBreakDecision(shippingProfile);
        expect(runtime.replaceStateForScenario(
                   runtime.state(), shippingProfile).ok,
            "AI turn fixture installs the shipping interactive profile");
        runtime.setRulesMode(billiardgl::RulesMode::ChineseEightBallV1);
        billiardgl::replay::Recorder aiRecorder;
        const std::string replayContent = billiardgl::sha256Hex(
            "ai-semantic-stream-v1");
        expect(aiRecorder.begin(runtime, "ai-turn-test-v1",
            replayContent, 11).ok,
            "AI recording binds before the turn starts");
        billiardgl::ai::AiTurnController controller;
        expect(controller.begin(runtime, billiardgl::ai::Difficulty::Expert, 11,
            billiardgl::ai::defaultBudget(
                billiardgl::ai::Difficulty::Expert)),
            "AI begins from a stopped authoritative rules state");
        std::vector<double> pollDurationsMilliseconds;
        const auto applied = await(controller, runtime,
            &pollDurationsMilliseconds);
        std::sort(pollDurationsMilliseconds.begin(),
            pollDurationsMilliseconds.end());
        const auto percentile = [&](double quantile) {
            const std::size_t index = std::min(
                pollDurationsMilliseconds.size() - 1,
                static_cast<std::size_t>(quantile *
                    static_cast<double>(pollDurationsMilliseconds.size())));
            return pollDurationsMilliseconds[index];
        };
        const double pollP50 = percentile(0.50);
        const double pollP95 = percentile(0.95);
        const double pollP99 = percentile(0.99);
        std::cout << "ai_poll_nonblocking p50_ms=" << pollP50
                  << " p95_ms=" << pollP95
                  << " p99_ms=" << pollP99
                  << " samples=" << pollDurationsMilliseconds.size()
                  << " planning_ms=" << applied.plan.elapsedMilliseconds
                  << " candidates=" << applied.plan.candidatesEvaluated
                  << " wall_budget=" << applied.plan.wallTimeBudgetReached
                  << '\n';
        expect(applied.status == billiardgl::ai::TurnPollStatus::Applied &&
            applied.plan.found && runtime.state().ballsMoving &&
            runtime.shotFactCollectorActive() &&
            applied.plan.action.controls.requestedPower > 90.0f &&
            pollP99 < 50.0,
            "AI applies the chosen public semantic action through GameRuntime");
        expect(aiRecorder.captureApplied(runtime).ok &&
            aiRecorder.document().actions.size() == 5 &&
            aiRecorder.document().actions[0].type ==
                billiardgl::replay::SemanticActionType::SetAimYaw &&
            aiRecorder.document().actions[1].type ==
                billiardgl::replay::SemanticActionType::SetShotPower &&
            aiRecorder.document().actions[2].type ==
                billiardgl::replay::SemanticActionType::SetTipOffset &&
            aiRecorder.document().actions[3].type ==
                billiardgl::replay::SemanticActionType::SetCalledPocket &&
            aiRecorder.document().actions[4].type ==
                billiardgl::replay::SemanticActionType::Shoot,
            "AI emits the same authoritative semantic action stream consumed by replay");
        int shotTicks = 0;
        while (runtime.state().ballsMoving && shotTicks < 5000) {
            const billiardgl::ActionResult step = runtime.step(1);
            if (!step.ok) {
                throw std::runtime_error(
                    std::string("AI shot advances through authoritative runtime: ") +
                    step.errorCode + ':' +
                    runtime.tableDepartureObservationError() + ":power=" +
                    std::to_string(
                        applied.plan.action.controls.requestedPower) +
                    ":yaw=" + std::to_string(
                        applied.plan.action.controls.yawRadians) +
                    ":detail=" + (runtime.hasRecentPhysicsStep() ?
                        runtime.latestPhysicsStep().telemetry.failureDetail :
                        std::string{}));
            }
            ++shotTicks;
        }
        expect(!runtime.state().ballsMoving && runtime.hasLastShotFacts() &&
            runtime.lastShotFacts().calledPocket ==
                applied.plan.action.targetPocket,
            "AI commits its semantic target pocket through the public rules setter");
        expect(!runtime.matchState().groupChoicePending &&
                !runtime.ballInHandPlacementPending() &&
                !runtime.breakFoulDecisionPending() &&
                !runtime.eightOnBreakDecisionPending() &&
                controller.begin(runtime, billiardgl::ai::Difficulty::Expert,
                    12, billiardgl::ai::defaultBudget(
                            billiardgl::ai::Difficulty::Expert)),
            "the settled opening shot exposes a second replayable AI turn without an unrecorded rules decision");
        const auto secondApplied = await(controller, runtime);
        expect(secondApplied.status ==
                    billiardgl::ai::TurnPollStatus::Applied &&
                secondApplied.plan.found && runtime.state().ballsMoving,
            "the second AI turn applies through the same public semantic path");
        expect(aiRecorder.captureApplied(runtime).ok &&
                aiRecorder.document().actions.size() == 10,
            "recorder captures two complete five-action AI turns");
        for (std::size_t turn = 0; turn < 2; ++turn) {
            const std::size_t firstAction = turn * 5;
            expect(aiRecorder.document().actions[firstAction + 0].type ==
                        billiardgl::replay::SemanticActionType::SetAimYaw &&
                    aiRecorder.document().actions[firstAction + 1].type ==
                        billiardgl::replay::SemanticActionType::SetShotPower &&
                    aiRecorder.document().actions[firstAction + 2].type ==
                        billiardgl::replay::SemanticActionType::SetTipOffset &&
                    aiRecorder.document().actions[firstAction + 3].type ==
                        billiardgl::replay::SemanticActionType::SetCalledPocket &&
                    aiRecorder.document().actions[firstAction + 4].type ==
                        billiardgl::replay::SemanticActionType::Shoot,
                "each recorded AI turn preserves the complete public action schema");
        }
        int secondShotTicks = 0;
        while (runtime.state().ballsMoving && secondShotTicks < 5000) {
            const billiardgl::ActionResult step = runtime.step(1);
            if (!step.ok)
                throw std::runtime_error(
                    std::string("second recorded AI shot failed: ") +
                    step.errorCode + ':' +
                    runtime.tableDepartureObservationError());
            ++secondShotTicks;
        }
        expect(!runtime.state().ballsMoving && secondShotTicks < 5000,
            "second recorded AI shot settles through the authoritative runtime");
        expect(aiRecorder.finish(runtime, 2).ok,
            "two-turn AI semantic stream finishes as a validated replay recording");
        const std::string recordedAuthorityHash =
            billiardgl::gameStateSha256(runtime.state());
        const std::uint64_t recordedAuthorityTick = runtime.tick();
        billiardgl::replay::ReplayTransportModel aiReplay;
        expect(aiReplay.load(aiRecorder.document(), shippingProfile,
                   "ai-turn-test-v1", replayContent).ok &&
                aiReplay.setPaused(false).ok,
            "recorded product AI action stream loads through replay transport");
        while (!aiReplay.snapshot().finished) {
            const auto replayed = aiReplay.stepTick();
            if (!replayed.ok)
                throw std::runtime_error(
                    "recorded AI replay failed: " + replayed.errorCode);
        }
        const auto replaySnapshot = aiReplay.snapshot();
        expect(aiReplay.player().finalHashMatches() &&
                replaySnapshot.tick == recordedAuthorityTick &&
                !replaySnapshot.cueTrajectory.empty() &&
                billiardgl::gameStateSha256(runtime.state()) ==
                    recordedAuthorityHash &&
                runtime.tick() == recordedAuthorityTick,
            "ReplayTransport re-executes the recorded AI public action and matches its final hash without mutating live authority");

        billiardgl::GameRuntime localPlayer;
        expect(localPlayer.setRulesMode(
            billiardgl::RulesMode::ChineseEightBallV1).ok,
            "local semantic stream fixture enables product rules");
        billiardgl::GameAction localYaw;
        localYaw.type = billiardgl::ActionType::SetAimYaw;
        localYaw.first = 1.5707963f;
        billiardgl::GameAction localPower;
        localPower.type = billiardgl::ActionType::SetShotPower;
        localPower.first = 60.0f;
        billiardgl::GameAction localShoot;
        localShoot.type = billiardgl::ActionType::Shoot;
        expect(localPlayer.dispatch(localYaw).ok &&
            localPlayer.dispatch(localPower).ok &&
            localPlayer.setTipOffset(0.0f, 0.0f).ok &&
            localPlayer.setCalledPocket(0).ok,
            "local player applies public semantic setters");
        billiardgl::GameAction invalidPower = localPower;
        invalidPower.first = 201.0f;
        expect(!localPlayer.dispatch(invalidPower).ok &&
            localPlayer.dispatch(localShoot).ok &&
            localPlayer.appliedSemanticActions().size() == 5,
            "failed local semantic action emits no authoritative applied event");
        for (std::size_t index = 0; index < 5; ++index) {
            expect(localPlayer.appliedSemanticActions()[index].type ==
                runtime.appliedSemanticActions()[index].type,
                "local player and AI share one ordered semantic action schema");
        }

        billiardgl::GameRuntime stale;
        stale.setRulesMode(billiardgl::RulesMode::ChineseEightBallV1);
        billiardgl::ai::AiTurnController staleController;
        expect(staleController.begin(stale, billiardgl::ai::Difficulty::Rookie, 12,
            {4, std::chrono::milliseconds(120)}),
            "stale-result fixture submits");
        billiardgl::BallState moved = stale.state().balls[1];
        moved.position.x += 1.0f;
        expect(stale.setBall(1, moved).ok, "authority can change while worker thinks");
        const auto rejected = await(staleController, stale);
        expect(rejected.status == billiardgl::ai::TurnPollStatus::StaleAuthority &&
            !stale.state().ballsMoving,
            "stale AI result is discarded without firing a shot");

        billiardgl::GameRuntime pendingChoice;
        pendingChoice.setRulesMode(billiardgl::RulesMode::ChineseEightBallV1);
        billiardgl::ai::AiTurnController pendingController;
        expect(pendingController.begin(pendingChoice,
            billiardgl::ai::Difficulty::Rookie, 13,
            {4, std::chrono::milliseconds(120)}),
            "AI pending-gate fixture starts before the authority changes");
        billiardgl::rules::ShotFacts legalBreak;
        legalBreak.shooter = 0;
        legalBreak.breakShot = true;
        legalBreak.firstContactBall = 1;
        legalBreak.distinctObjectBallsToRail = 4;
        expect(pendingChoice.adjudicateRulesShot(legalBreak).ok,
            "AI pending-gate fixture completes the break");
        billiardgl::rules::ShotFacts groupChoice;
        groupChoice.shooter = 1;
        groupChoice.firstContactBall = 1;
        groupChoice.firstContactBalls = {1, 9};
        groupChoice.pocketedBallsInOrder = {1, 9};
        expect(pendingChoice.adjudicateRulesShot(groupChoice).ok &&
            pendingChoice.matchState().groupChoicePending,
            "AI pending-gate fixture stages the unresolved group choice");
        const std::string pendingHash =
            billiardgl::gameStateSha256(pendingChoice.state());
        const std::uint64_t pendingTick = pendingChoice.tick();
        const std::size_t pendingEvents = pendingChoice.eventsSince(0).size();
        const std::size_t pendingActions =
            pendingChoice.appliedSemanticActions().size();
        const auto pendingRejected = await(pendingController, pendingChoice);
        expect(pendingRejected.status ==
                billiardgl::ai::TurnPollStatus::Rejected &&
            pendingRejected.error == "rules_group_choice_pending" &&
            billiardgl::gameStateSha256(pendingChoice.state()) == pendingHash &&
            pendingChoice.tick() == pendingTick &&
            pendingChoice.eventsSince(0).size() == pendingEvents &&
            pendingChoice.appliedSemanticActions().size() == pendingActions &&
            pendingChoice.matchState().groupChoicePending,
            "AI apply rejects the pending gate before any authority mutation");
        billiardgl::ai::AiTurnController blockedBegin;
        expect(!blockedBegin.begin(pendingChoice,
            billiardgl::ai::Difficulty::Rookie, 14,
            {4, std::chrono::milliseconds(120)}),
            "AI cannot begin a new plan while group choice is pending");
        billiardgl::ai::AiPlayerController groupChoiceProductAi;
        billiardgl::ai::PlayerConfiguration groupChoiceConfiguration;
        groupChoiceConfiguration.enabled = true;
        groupChoiceConfiguration.player =
            pendingChoice.groupChoiceDecisionOffer()->actorPlayerId;
        billiardgl::ai::AiPlayerController foreignGroupChoiceAi;
        auto foreignGroupChoiceConfiguration = groupChoiceConfiguration;
        foreignGroupChoiceConfiguration.player =
            1 - groupChoiceConfiguration.player;
        expect(foreignGroupChoiceAi.configure(
                   pendingChoice, foreignGroupChoiceConfiguration).ok,
            "foreign AI may configure while group authority belongs to its opponent");
        const std::string groupRulesHash =
            pendingChoice.rulesAuthorityStateHash();
        const auto foreignGroupChoicePoll =
            foreignGroupChoiceAi.poll(pendingChoice);
        expect(foreignGroupChoicePoll.status ==
                billiardgl::ai::PlayerPollStatus::WaitingForTurn &&
            foreignGroupChoicePoll.jobsStarted == 0 &&
            pendingChoice.rulesAuthorityStateHash() == groupRulesHash &&
            pendingChoice.eventsSince(0).size() == pendingEvents &&
            pendingChoice.appliedSemanticActions().size() == pendingActions &&
            pendingChoice.matchState().groupChoicePending,
            "foreign AI leaves actor-bound group authority byte-stable");
        expect(groupChoiceProductAi.configure(
                   pendingChoice, groupChoiceConfiguration).ok,
            "product AI configures for its offered group decision");
        const auto groupChoicePoll =
            groupChoiceProductAi.poll(pendingChoice);
        expect(groupChoicePoll.status ==
                billiardgl::ai::PlayerPollStatus::WaitingForTurn &&
            !pendingChoice.matchState().groupChoicePending &&
            pendingChoice.matchState().phase ==
                billiardgl::rules::MatchPhase::GroupsAssigned &&
            pendingChoice.matchState().groups[
                groupChoiceConfiguration.player] !=
                billiardgl::rules::BallGroup::Open,
            "product AI commits its own revision-bound group choice");

        billiardgl::GameRuntime pendingEight;
        pendingEight.setRulesMode(
            billiardgl::RulesMode::ChineseEightBallV1);
        billiardgl::ai::AiTurnController pendingEightController;
        expect(pendingEightController.begin(pendingEight,
            billiardgl::ai::Difficulty::Rookie, 141,
            {4, std::chrono::milliseconds(120)}),
            "AI eight-on-break gate fixture starts before authority changes");
        billiardgl::rules::ShotFacts eightBreak;
        eightBreak.shooter = 0;
        eightBreak.breakShot = true;
        eightBreak.firstContactBall = 1;
        eightBreak.pocketedBallsInOrder = {8};
        expect(pendingEight.adjudicateRulesShot(eightBreak).ok &&
            pendingEight.eightOnBreakDecisionPending(),
            "AI fixture stages the unresolved 8-on-break choice");
        const std::string pendingEightHash =
            pendingEight.rulesAuthorityStateHash();
        const std::size_t pendingEightEvents =
            pendingEight.eventsSince(0).size();
        const std::size_t pendingEightActions =
            pendingEight.appliedSemanticActions().size();
        const auto pendingEightRejected = await(
            pendingEightController, pendingEight);
        expect(pendingEightRejected.status ==
                billiardgl::ai::TurnPollStatus::Rejected &&
            pendingEightRejected.error ==
                "rules_eight_on_break_decision_pending" &&
            pendingEight.rulesAuthorityStateHash() == pendingEightHash &&
            pendingEight.eventsSince(0).size() == pendingEightEvents &&
            pendingEight.appliedSemanticActions().size() ==
                pendingEightActions,
            "AI apply rejects 8-on-break pending authority before mutation");
        billiardgl::ai::AiTurnController pendingEightBlockedBegin;
        expect(!pendingEightBlockedBegin.begin(pendingEight,
            billiardgl::ai::Difficulty::Rookie, 142,
            {4, std::chrono::milliseconds(120)}),
            "AI cannot begin while 8-on-break authority is pending");
        billiardgl::ai::AiPlayerController pendingEightProductAi;
        billiardgl::ai::PlayerConfiguration pendingEightConfiguration;
        pendingEightConfiguration.enabled = true;
        pendingEightConfiguration.player = 0;
        billiardgl::ai::AiPlayerController foreignEightProductAi;
        auto foreignEightConfiguration = pendingEightConfiguration;
        foreignEightConfiguration.player = 1;
        expect(foreignEightProductAi.configure(
                   pendingEight, foreignEightConfiguration).ok,
            "foreign AI may configure while 8-on-break authority belongs to the breaker");
        const auto foreignEightPoll =
            foreignEightProductAi.poll(pendingEight);
        expect(foreignEightPoll.status ==
                billiardgl::ai::PlayerPollStatus::WaitingForTurn &&
            foreignEightPoll.jobsStarted == 0 &&
            pendingEight.rulesAuthorityStateHash() == pendingEightHash &&
            pendingEight.eventsSince(0).size() == pendingEightEvents &&
            pendingEight.appliedSemanticActions().size() ==
                pendingEightActions &&
            pendingEight.eightOnBreakDecisionPending(),
            "foreign AI leaves actor-bound 8-on-break authority unchanged");
        expect(pendingEightProductAi.configure(
                   pendingEight, pendingEightConfiguration).ok,
            "product AI configures without resolving the authority choice");
        const auto pendingEightPoll =
            pendingEightProductAi.poll(pendingEight);
        expect(pendingEightPoll.status ==
                billiardgl::ai::PlayerPollStatus::WaitingForTurn &&
            pendingEightPoll.error.empty() &&
            pendingEightPoll.jobsStarted == 0 &&
            pendingEight.rulesAuthorityStateHash() != pendingEightHash &&
            !pendingEight.eightOnBreakDecisionPending() &&
            pendingEight.matchState().phase ==
                billiardgl::rules::MatchPhase::OpenTable &&
            !pendingEight.state().balls[8].pocketed,
            "product AI commits its own offered 8-on-break spot-and-continue choice");

        billiardgl::GameRuntime pendingPlacement;
        pendingPlacement.setRulesMode(
            billiardgl::RulesMode::ChineseEightBallV1);
        expect(pendingPlacement.adjudicateRulesShot(legalBreak).ok,
            "AI placement-gate fixture completes the break");
        billiardgl::rules::ShotFacts assignment;
        assignment.shooter = 1;
        assignment.firstContactBall = 1;
        assignment.anyRailAfterFirstContact = true;
        assignment.pocketedBallsInOrder = {1};
        expect(pendingPlacement.adjudicateRulesShot(assignment).ok,
            "AI placement-gate fixture assigns groups");
        billiardgl::rules::ShotFacts foul;
        foul.shooter = 1;
        foul.firstContactBall = 9;
        expect(pendingPlacement.adjudicateRulesShot(foul).ok &&
            pendingPlacement.ballInHandPlacementPending(),
            "AI placement-gate fixture stages full-table ball in hand");
        const std::string placementHash =
            pendingPlacement.rulesAuthorityStateHash();
        const std::size_t placementEvents =
            pendingPlacement.eventsSince(0).size();
        const std::size_t placementActions =
            pendingPlacement.appliedSemanticActions().size();
        const std::string placementStateHash =
            billiardgl::gameStateSha256(pendingPlacement.state());
        const std::uint64_t placementTick = pendingPlacement.tick();
        billiardgl::ai::AiTurnController placementBlockedBegin;
        expect(!placementBlockedBegin.begin(pendingPlacement,
            billiardgl::ai::Difficulty::Rookie, 15,
            {4, std::chrono::milliseconds(120)}) &&
            pendingPlacement.rulesAuthorityStateHash() == placementHash &&
            pendingPlacement.eventsSince(0).size() == placementEvents,
            "AI cannot begin or mutate authority while placement is pending");
        billiardgl::ai::AiPlayerController placementProductAi;
        billiardgl::ai::PlayerConfiguration placementConfiguration;
        placementConfiguration.enabled = true;
        placementConfiguration.player =
            pendingPlacement.matchState().currentPlayer;
        billiardgl::ai::AiPlayerController foreignPlacementAi;
        auto foreignPlacementConfiguration = placementConfiguration;
        foreignPlacementConfiguration.player =
            1 - placementConfiguration.player;
        expect(foreignPlacementAi.configure(
                   pendingPlacement, foreignPlacementConfiguration).ok,
            "foreign AI can remain configured while another actor owns placement");
        const auto foreignPlacementPoll =
            foreignPlacementAi.poll(pendingPlacement);
        expect(foreignPlacementPoll.status ==
                billiardgl::ai::PlayerPollStatus::WaitingForTurn &&
            foreignPlacementPoll.jobsStarted == 0 &&
            pendingPlacement.rulesAuthorityStateHash() == placementHash &&
            pendingPlacement.eventsSince(0).size() == placementEvents &&
            pendingPlacement.appliedSemanticActions().size() ==
                placementActions &&
            billiardgl::gameStateSha256(pendingPlacement.state()) ==
                placementStateHash &&
            pendingPlacement.tick() == placementTick,
            "foreign AI cannot submit or consume another actor's ball-in-hand offer");
        expect(placementProductAi.configure(
                   pendingPlacement, placementConfiguration).ok,
            "AI placement-gate product fixture configures current actor");
        const auto placementPoll = placementProductAi.poll(pendingPlacement);
        expect(placementPoll.status ==
                billiardgl::ai::PlayerPollStatus::WaitingForTurn &&
            placementPoll.error.empty() &&
            placementPoll.jobsStarted == 0 &&
            pendingPlacement.rulesAuthorityStateHash() != placementHash &&
            pendingPlacement.eventsSince(0).size() == placementEvents + 1 &&
            pendingPlacement.ballInHandPlacementPending() &&
            pendingPlacement.ballInHandPlacementOffer() &&
            !pendingPlacement.state().balls[0].pocketed,
            "product AI commits one legal placement while retaining the exact impulse-bound offer");
        billiardgl::ai::AiTurnController wrongRetainedTurn;
        expect(!wrongRetainedTurn.begin(pendingPlacement,
                   billiardgl::ai::Difficulty::Club, 16,
                   {4, std::chrono::milliseconds(120)},
                   "rules.ball_in_hand.foreign-or-stale") &&
            pendingPlacement.ballInHandPlacementPending() &&
            !pendingPlacement.state().ballsMoving,
            "turn planning cannot opt into a foreign or stale retained placement ID");
        auto retainedPlanning = placementProductAi.poll(pendingPlacement);
        expect(retainedPlanning.status ==
                billiardgl::ai::PlayerPollStatus::Thinking &&
            retainedPlanning.jobsStarted == 1,
            "product AI may plan against its exact retained placement offer");
        const auto replacedOffer =
            *pendingPlacement.ballInHandPlacementOffer();
        billiardgl::rules::BallInHandPlacementRequest replaceWhileThinking;
        replaceWhileThinking.flowId =
            billiardgl::rules::kFullTableBallInHandFlowId;
        replaceWhileThinking.decisionId = replacedOffer.decisionId;
        replaceWhileThinking.expectedAuthorityStateRevision =
            replacedOffer.authorityStateRevision;
        replaceWhileThinking.idempotencyKey =
            "ai-placement-external-revision";
        replaceWhileThinking.actorPlayerId = replacedOffer.actorPlayerId;
        replaceWhileThinking.actionId =
            replacedOffer.offeredActionIds.front();
        replaceWhileThinking.position = {
            pendingPlacement.state().balls[0].position.x,
            pendingPlacement.state().balls[0].position.z};
        expect(pendingPlacement.submitBallInHandPlacement(
                   replaceWhileThinking).ok,
            "external authority can replace the retained offer while AI thinks");
        const std::size_t semanticBeforeStale =
            pendingPlacement.appliedSemanticActions().size();
        const std::string rulesHashBeforeStale =
            pendingPlacement.rulesAuthorityStateHash();
        const std::string gameHashBeforeStale =
            billiardgl::gameStateSha256(pendingPlacement.state());
        const std::size_t eventsBeforeStale =
            pendingPlacement.eventsSince(0).size();
        const std::uint64_t tickBeforeStale = pendingPlacement.tick();
        const auto retainedDeadline = std::chrono::steady_clock::now() +
            std::chrono::seconds(2);
        while (retainedPlanning.status ==
                billiardgl::ai::PlayerPollStatus::Thinking &&
            std::chrono::steady_clock::now() < retainedDeadline) {
            std::this_thread::sleep_for(std::chrono::milliseconds(1));
            retainedPlanning = placementProductAi.poll(pendingPlacement);
        }
        expect(retainedPlanning.status ==
                billiardgl::ai::PlayerPollStatus::Rejected &&
            retainedPlanning.error ==
                "rules_ball_in_hand_placement_pending" &&
            pendingPlacement.appliedSemanticActions().size() ==
                semanticBeforeStale &&
            pendingPlacement.rulesAuthorityStateHash() ==
                rulesHashBeforeStale &&
            billiardgl::gameStateSha256(pendingPlacement.state()) ==
                gameHashBeforeStale &&
            pendingPlacement.eventsSince(0).size() == eventsBeforeStale &&
            pendingPlacement.tick() == tickBeforeStale &&
            !pendingPlacement.state().ballsMoving,
            "replacement revision makes the retained AI result stale with zero player mutation");
        const auto reboundPlacement =
            placementProductAi.poll(pendingPlacement);
        expect(reboundPlacement.status ==
                billiardgl::ai::PlayerPollStatus::WaitingForTurn &&
            pendingPlacement.ballInHandPlacementPending(),
            "product AI revalidates and commits the new placement offer once");
        retainedPlanning = placementProductAi.poll(pendingPlacement);
        expect(retainedPlanning.status ==
                billiardgl::ai::PlayerPollStatus::Thinking &&
            retainedPlanning.jobsStarted == 2,
            "product AI replans only after binding the replacement offer");
        const auto reboundDeadline = std::chrono::steady_clock::now() +
            std::chrono::seconds(2);
        while (retainedPlanning.status ==
                billiardgl::ai::PlayerPollStatus::Thinking &&
            std::chrono::steady_clock::now() < reboundDeadline) {
            std::this_thread::sleep_for(std::chrono::milliseconds(1));
            retainedPlanning = placementProductAi.poll(pendingPlacement);
        }
        expect(retainedPlanning.status ==
                billiardgl::ai::PlayerPollStatus::Applied &&
            pendingPlacement.state().ballsMoving &&
            !pendingPlacement.ballInHandPlacementPending() &&
            !pendingPlacement.matchState().ballInHand,
            "product AI public Shoot consumes only its bound retained ball-in-hand entitlement");

        billiardgl::GameRuntime pendingBreakChoice;
        pendingBreakChoice.setRulesMode(
            billiardgl::RulesMode::ChineseEightBallV1);
        billiardgl::rules::ShotFacts weakBreak;
        weakBreak.shooter = 0;
        weakBreak.breakShot = true;
        weakBreak.firstContactBall = 1;
        weakBreak.distinctObjectBallsToRail = 3;
        attestStraightBreakCushions(weakBreak, {1, 2, 3});
        expect(pendingBreakChoice.adjudicateRulesShot(weakBreak).ok &&
            pendingBreakChoice.breakFoulDecisionPending(),
            "AI break-choice fixture stages Core H authority");
        const std::string breakChoiceHash =
            pendingBreakChoice.rulesAuthorityStateHash();
        billiardgl::ai::AiTurnController breakChoiceTurn;
        expect(!breakChoiceTurn.begin(pendingBreakChoice,
            billiardgl::ai::Difficulty::Rookie, 149,
            {4, std::chrono::milliseconds(120)}),
            "AI cannot begin while a break-foul choice is pending");
        billiardgl::ai::AiPlayerController breakChoiceProductAi;
        billiardgl::ai::PlayerConfiguration breakChoiceConfiguration;
        breakChoiceConfiguration.enabled = true;
        breakChoiceConfiguration.player =
            pendingBreakChoice.matchState().currentPlayer;
        billiardgl::ai::AiPlayerController foreignBreakChoiceProductAi;
        auto foreignBreakChoiceConfiguration = breakChoiceConfiguration;
        foreignBreakChoiceConfiguration.player =
            1 - breakChoiceConfiguration.player;
        expect(foreignBreakChoiceProductAi.configure(
                   pendingBreakChoice, foreignBreakChoiceConfiguration).ok,
            "foreign AI may configure while break-foul authority belongs to its opponent");
        const std::size_t breakChoiceEvents =
            pendingBreakChoice.eventsSince(0).size();
        const std::size_t breakChoiceActions =
            pendingBreakChoice.appliedSemanticActions().size();
        const auto foreignBreakChoicePoll =
            foreignBreakChoiceProductAi.poll(pendingBreakChoice);
        expect(foreignBreakChoicePoll.status ==
                billiardgl::ai::PlayerPollStatus::WaitingForTurn &&
            foreignBreakChoicePoll.jobsStarted == 0 &&
            pendingBreakChoice.rulesAuthorityStateHash() == breakChoiceHash &&
            pendingBreakChoice.eventsSince(0).size() == breakChoiceEvents &&
            pendingBreakChoice.appliedSemanticActions().size() ==
                breakChoiceActions &&
            pendingBreakChoice.breakFoulDecisionPending(),
            "foreign AI leaves actor-bound break-foul authority unchanged");
        expect(breakChoiceProductAi.configure(
                   pendingBreakChoice, breakChoiceConfiguration).ok,
            "product AI configures without resolving the break choice");
        const auto breakChoicePoll =
            breakChoiceProductAi.poll(pendingBreakChoice);
        expect(breakChoicePoll.status ==
                billiardgl::ai::PlayerPollStatus::WaitingForTurn &&
            breakChoicePoll.error.empty() &&
            breakChoicePoll.jobsStarted == 0 &&
            pendingBreakChoice.rulesAuthorityStateHash() !=
                breakChoiceHash &&
            !pendingBreakChoice.breakFoulDecisionPending() &&
            !pendingBreakChoice.matchState().ballInHand,
            "product AI accepts the offered post-break table without bypassing authority");

        billiardgl::GameRuntime pendingHeadString;
        pendingHeadString.setRulesMode(
            billiardgl::RulesMode::ChineseEightBallV1);
        billiardgl::rules::ShotFacts breakScratch;
        breakScratch.shooter = 0;
        breakScratch.breakShot = true;
        breakScratch.firstContactBall = 1;
        breakScratch.cueBallOffTable = true;
        const bool parentStaged =
            pendingHeadString.adjudicateRulesShot(breakScratch).ok &&
            pendingHeadString.breakFoulDecisionOffer().has_value();
        billiardgl::rules::BreakFoulDecisionRequest parentRequest;
        if (parentStaged) {
            const auto parentOffer =
                *pendingHeadString.breakFoulDecisionOffer();
            parentRequest.rulesetId =
                billiardgl::rules::kBreakFoulRulesetId;
            parentRequest.capabilityId =
                billiardgl::rules::kBreakFoulCapabilityId;
            parentRequest.flowId = billiardgl::rules::kBreakFoulFlowId;
            parentRequest.decisionId = parentOffer.decisionId;
            parentRequest.expectedAuthorityStateRevision =
                parentOffer.authorityStateRevision;
            parentRequest.idempotencyKey = "ai-head-string-parent";
            parentRequest.actorPlayerId = parentOffer.actorPlayerId;
            parentRequest.actionId =
                billiardgl::rules::kBreakFoulBallInHandActionId;
            parentRequest.payloadCanonical = "{}";
        }
        const auto parentApplied = parentStaged ?
            pendingHeadString.submitBreakFoulDecision(parentRequest) :
            billiardgl::rules::BreakFoulDecisionResult{};
        expect(parentApplied.ok &&
            pendingHeadString.ballInHandPlacementScope() ==
                billiardgl::rules::BallInHandPlacementScope::BehindHeadString &&
            pendingHeadString.ballInHandPlacementOffer(),
            "AI head-string fixture stages the scoped authority offer");
        const std::string headStringHash =
            pendingHeadString.rulesAuthorityStateHash();
        billiardgl::ai::AiTurnController headStringTurn;
        expect(!headStringTurn.begin(pendingHeadString,
            billiardgl::ai::Difficulty::Rookie, 151,
            {4, std::chrono::milliseconds(120)}),
            "AI cannot begin while head-string placement is pending");
        billiardgl::ai::AiPlayerController headStringProductAi;
        billiardgl::ai::PlayerConfiguration headStringConfiguration;
        headStringConfiguration.enabled = true;
        headStringConfiguration.player =
            pendingHeadString.matchState().currentPlayer;
        expect(headStringProductAi.configure(
                   pendingHeadString, headStringConfiguration).ok,
            "product AI configures without resolving head-string placement");
        const auto headStringPoll =
            headStringProductAi.poll(pendingHeadString);
        expect(headStringPoll.status ==
                billiardgl::ai::PlayerPollStatus::WaitingForTurn &&
            headStringPoll.error.empty() &&
            headStringPoll.jobsStarted == 0 &&
            pendingHeadString.rulesAuthorityStateHash() != headStringHash &&
            pendingHeadString.ballInHandPlacementPending() &&
            pendingHeadString.ballInHandPlacementOffer() &&
            pendingHeadString.state().balls[0].position.z <
                pendingHeadString.ballInHandPlacementOffer()->
                    headStringWorldZCm,
            "product AI commits a legal behind-head-string placement and retains authority until Shoot");

        billiardgl::GameRuntime productRuntime;
        billiardgl::ai::AiPlayerController productAi;
        billiardgl::ai::PlayerConfiguration productConfiguration;
        productConfiguration.enabled = true;
        productConfiguration.player = 0;
        productConfiguration.difficulty = billiardgl::ai::Difficulty::Rookie;
        productConfiguration.deterministicSeed = 21;
        expect(productAi.configure(productRuntime, productConfiguration).ok &&
            productRuntime.rulesMode() ==
                billiardgl::RulesMode::ChineseEightBallV1,
            "enabling the product AI selects the complete product ruleset");
        const auto pollStart = std::chrono::steady_clock::now();
        const auto started = productAi.poll(productRuntime);
        const auto pollElapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::steady_clock::now() - pollStart);
        expect(started.status == billiardgl::ai::PlayerPollStatus::Thinking &&
            pollElapsed < std::chrono::milliseconds(50) &&
            !productRuntime.state().ballsMoving,
            "product AI starts background preview without blocking or mutating authority");
        const std::uint64_t boundTick = productRuntime.tick();
        const auto productDeadline = std::chrono::steady_clock::now() +
            std::chrono::seconds(2);
        billiardgl::ai::PlayerState productState = started;
        while (productState.status == billiardgl::ai::PlayerPollStatus::Thinking &&
            std::chrono::steady_clock::now() < productDeadline) {
            productState = productAi.poll(productRuntime);
            if (productState.status == billiardgl::ai::PlayerPollStatus::Thinking)
                std::this_thread::sleep_for(std::chrono::milliseconds(1));
        }
        expect(productState.status == billiardgl::ai::PlayerPollStatus::Applied &&
            productRuntime.state().ballsMoving && productRuntime.tick() == boundTick,
            "nonblocking product poll applies only the still-bound authority result");

        billiardgl::GameRuntime automationRuntime;
        billiardgl::ai::AiPlayerController automationAi;
        billiardgl::AutomationController automation(
            automationRuntime, billiardgl::AutomationMode::Headless,
            &automationAi);
        billiardgl::json::Value configure = billiardgl::json::Value::object();
        configure["schema_version"] = billiardgl::json::Value(1);
        configure["enabled"] = billiardgl::json::Value(true);
        configure["player"] = billiardgl::json::Value(0);
        configure["difficulty"] = billiardgl::json::Value("club");
        configure["deterministic_seed"] = billiardgl::json::Value(31);
        const auto configured = send(
            automation, 1, "configure_ai_player", configure).response;
        expect(configured.at("ok").asBool() &&
            configured.at("result").at("enabled").asBool() &&
            configured.at("result").at("difficulty").asString() == "club",
            "automation enables and configures the real product AI");
        const auto automationStarted = send(automation, 2, "poll_ai").response;
        expect(automationStarted.at("ok").asBool() &&
            automationStarted.at("result").at("status").asString() == "thinking",
            "automation explicitly polls AI without advancing authority tick");
        expect(send(automation, 3, "get_ai_state").response.at("result")
                .at("jobs_started").asInt() == 1 && automationRuntime.tick() == 0,
            "automation query exposes AI job state without mutating the game");
        return EXIT_SUCCESS;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return EXIT_FAILURE;
    }
}
