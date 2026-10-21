#include "ai/game_preview_evaluator.h"
#include "ai/ai_player_controller.h"
#include "ai/shot_planner.h"
#include "deterministic_hash.h"
#include "game_runtime.h"
#include "replay/replay_recorder.h"
#include "rules/ball_in_hand_authority.h"
#include "rules/break_foul_authority.h"
#include "rules/eight_on_break_authority.h"
#include "rules/group_choice_authority.h"

#include <algorithm>
#include <array>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <cstdlib>
#include <iostream>
#include <iomanip>
#include <sstream>
#include <stdexcept>
#include <string>
#include <thread>
#include <vector>

namespace {

using namespace billiardgl;

void expect(bool value, const std::string& message)
{
    if (!value) throw std::runtime_error(message);
}

std::string key(const char* prefix, std::uint64_t ordinal)
{
    return std::string(prefix) + '-' + std::to_string(ordinal);
}

bool offered(const std::vector<std::string>& actions, const char* action)
{
    return std::find(actions.begin(), actions.end(), action) != actions.end();
}

std::vector<int> legalTargets(const GameRuntime& runtime)
{
    std::vector<int> result;
    const int player = runtime.matchState().currentPlayer;
    const rules::BallGroup group = runtime.matchState().groups[player];
    if (group == rules::BallGroup::Open) {
        for (int ball = 1; ball < kBallCount; ++ball)
            if (ball != 8 && !runtime.state().balls[ball].pocketed &&
                !runtime.state().balls[ball].outOfPlay)
                result.push_back(ball);
        return result;
    }
    const int first = group == rules::BallGroup::Solids ? 1 : 9;
    const int last = group == rules::BallGroup::Solids ? 7 : 15;
    for (int ball = first; ball <= last; ++ball)
        if (!runtime.state().balls[ball].pocketed &&
            !runtime.state().balls[ball].outOfPlay)
            result.push_back(ball);
    if (result.empty() && !runtime.state().balls[8].pocketed &&
        !runtime.state().balls[8].outOfPlay)
        result.push_back(8);
    return result;
}

int activeGroupBalls(const GameRuntime& runtime, rules::BallGroup group)
{
    const int first = group == rules::BallGroup::Solids ? 1 : 9;
    const int last = group == rules::BallGroup::Solids ? 7 : 15;
    int count = 0;
    for (int ball = first; ball <= last; ++ball)
        if (!runtime.state().balls[ball].pocketed &&
            !runtime.state().balls[ball].outOfPlay)
            ++count;
    return count;
}

struct DecisionCounters {
    std::uint64_t group = 0;
    std::uint64_t breakFoul = 0;
    std::uint64_t eight = 0;
    std::uint64_t placement = 0;
};

void resolveGroupChoice(GameRuntime& runtime, DecisionCounters& counters)
{
    if (!runtime.matchState().groupChoicePending) return;
    expect(runtime.groupChoiceDecisionOffer().has_value(),
        "group choice is pending without an authority offer");
    const auto offer = *runtime.groupChoiceDecisionOffer();
    const rules::BallGroup choice =
        activeGroupBalls(runtime, rules::BallGroup::Solids) <=
                activeGroupBalls(runtime, rules::BallGroup::Stripes) ?
            rules::BallGroup::Solids : rules::BallGroup::Stripes;
    rules::GroupChoiceDecisionRequest request;
    request.rulesetId = rules::kGroupChoiceRulesetId;
    request.capabilityId = rules::kGroupChoiceAuthorityCapabilityId;
    request.flowId = rules::kGroupChoiceAuthorityFlowId;
    request.decisionId = offer.decisionId;
    request.expectedAuthorityStateRevision = offer.authorityStateRevision;
    request.idempotencyKey = key("full-match-group", ++counters.group);
    request.actorPlayerId = offer.actorPlayerId;
    request.actionId = choice == rules::BallGroup::Solids ?
        rules::kGroupChoiceSolidsActionId :
        rules::kGroupChoiceStripesActionId;
    expect(runtime.submitGroupChoiceDecision(request).ok,
        "AI authority failed to commit the offered group choice");
}

void resolveBreakFoul(GameRuntime& runtime, DecisionCounters& counters)
{
    if (!runtime.breakFoulDecisionPending()) return;
    const auto offer = *runtime.breakFoulDecisionOffer();
    const char* action = nullptr;
    if (offered(offer.offeredActionIds, rules::kBreakFoulAcceptTableActionId))
        action = rules::kBreakFoulAcceptTableActionId;
    else if (offered(offer.offeredActionIds,
                 rules::kBreakFoulBallInHandActionId))
        action = rules::kBreakFoulBallInHandActionId;
    else if (!offer.offeredActionIds.empty())
        action = offer.offeredActionIds.front().c_str();
    expect(action != nullptr,
        "break-foul authority offered no action to the AI actor");
    rules::BreakFoulDecisionRequest request;
    request.rulesetId = rules::kBreakFoulRulesetId;
    request.capabilityId = rules::kBreakFoulCapabilityId;
    request.flowId = rules::kBreakFoulFlowId;
    request.decisionId = offer.decisionId;
    request.expectedAuthorityStateRevision = offer.authorityStateRevision;
    request.idempotencyKey = key("full-match-break", ++counters.breakFoul);
    request.actorPlayerId = offer.actorPlayerId;
    request.actionId = action;
    request.payloadCanonical = "{}";
    const auto result = runtime.submitBreakFoulDecision(request);
    expect(result.ok, "AI authority failed break-foul decision: " +
        result.errorCode);
}

void resolveEightOnBreak(GameRuntime& runtime, DecisionCounters& counters)
{
    if (!runtime.eightOnBreakDecisionPending()) return;
    const auto offer = *runtime.eightOnBreakDecisionOffer();
    const char* preferred = offer.branch == rules::EightOnBreakBranch::LegalBreak ?
        rules::kEightOnBreakSpotContinueActionId :
        rules::kEightOnBreakSpotBallInHandActionId;
    expect(!offer.offeredActionIds.empty(),
        "8-on-break authority offered no action to the AI actor");
    const char* action = offered(offer.offeredActionIds, preferred) ?
        preferred : offer.offeredActionIds.front().c_str();
    rules::EightOnBreakDecisionRequest request;
    request.rulesetId = rules::kEightOnBreakRulesetId;
    request.capabilityId = rules::kEightOnBreakCapabilityId;
    request.flowId = rules::kEightOnBreakFlowId;
    request.decisionId = offer.decisionId;
    request.expectedAuthorityStateRevision = offer.authorityStateRevision;
    request.idempotencyKey = key("full-match-eight", ++counters.eight);
    request.actorPlayerId = offer.actorPlayerId;
    request.actionId = action;
    request.payloadCanonical = "{}";
    const auto result = runtime.submitEightOnBreakDecision(request);
    expect(result.ok, "AI authority failed eight-on-break decision: " +
        result.errorCode);
}

rules::BallInHandPlacementPosition findPlacement(const GameRuntime& runtime)
{
    const auto offer = *runtime.ballInHandPlacementOffer();
    const auto& boundary = runtime.physicsProfile().tableBoundary;
    const double radius = runtime.physicsProfile().ball.radiusCm;
    const double xLimit = boundary.playfieldWidthCm * 0.5 - radius;
    const double zLimit = boundary.playfieldLengthCm * 0.5 - radius;
    std::vector<double> zCandidates;
    if (offer.scope == rules::BallInHandPlacementScope::BehindHeadString) {
        for (double offset : {18.0, 32.0, 48.0, 64.0})
            zCandidates.push_back(std::max(-zLimit,
                offer.headStringWorldZCm - offset));
    } else {
        zCandidates = {-55.0, -30.0, 0.0, 30.0, 55.0};
    }
    const std::array<double, 9> xCandidates{{0.0, -18.0, 18.0, -36.0,
        36.0, -xLimit * 0.75, xLimit * 0.75, -xLimit, xLimit}};
    for (double z : zCandidates) {
        z = std::clamp(z, -zLimit, zLimit);
        for (double x : xCandidates) {
            const rules::BallInHandPlacementPosition candidate{x, z};
            if (runtime.validateBallInHandPlacement(candidate).ok)
                return candidate;
        }
    }
    throw std::runtime_error("AI found no legal ball-in-hand placement");
}

void resolvePlacement(GameRuntime& runtime, DecisionCounters& counters)
{
    if (!runtime.ballInHandPlacementPending()) return;
    expect(runtime.ballInHandPlacementOffer().has_value(),
        "ball in hand is pending without placement authority");
    const auto offer = *runtime.ballInHandPlacementOffer();
    rules::BallInHandPlacementRequest request;
    request.flowId = offer.scope ==
            rules::BallInHandPlacementScope::BehindHeadString ?
        rules::kBehindHeadStringBallInHandFlowId :
        rules::kFullTableBallInHandFlowId;
    request.decisionId = offer.decisionId;
    request.expectedAuthorityStateRevision = offer.authorityStateRevision;
    request.idempotencyKey = key("full-match-placement", ++counters.placement);
    request.actorPlayerId = offer.actorPlayerId;
    expect(!offer.offeredActionIds.empty() &&
            !offer.offeredActionIds.front().empty(),
        "ball-in-hand authority offered no placement action");
    request.actionId = offer.offeredActionIds.front();
    request.position = findPlacement(runtime);
    const auto result = runtime.submitBallInHandPlacement(request);
    expect(result.ok, "AI authority failed ball-in-hand placement: " +
        result.errorCode);
}

void resolvePendingDecisions(GameRuntime& runtime,
    DecisionCounters& counters)
{
    resolveEightOnBreak(runtime, counters);
    resolveBreakFoul(runtime, counters);
    resolveGroupChoice(runtime, counters);
    // A successful placement deliberately stages a replacement offer: the
    // entitlement remains live until the next positive cue impulse consumes
    // it. Commit exactly one legal coordinate and leave that replacement
    // pending for the public Shoot action instead of relocating repeatedly.
    resolvePlacement(runtime, counters);
}

ai::PlanResult chooseShot(const GameRuntime& runtime, std::uint64_t seed)
{
    ai::CandidateGenerationOptions options;
    options.legalTargetBalls = legalTargets(runtime);
    options.requestedPowers = runtime.matchState().phase ==
            rules::MatchPhase::Break ?
        std::vector<float>{95.0f, 125.0f, 155.0f} :
        std::vector<float>{42.0f, 58.0f, 76.0f, 98.0f};
    options.maximumCandidates = 144;
    const auto candidates = ai::enumerateTableCandidates(runtime.state(),
        runtime.physicsProfile(), options);
    training::PreviewBudget previewBudget;
    previewBudget.maximumTicks = 2400;
    previewBudget.maximumWallMilliseconds = 5000;
    previewBudget.maximumBalls = kBallCount;
    ai::GamePreviewEvaluator evaluator(runtime, previewBudget);
    return ai::ShotPlanner(ai::Difficulty::Expert, seed).plan(candidates,
        evaluator, {96, std::chrono::milliseconds(5000)});
}

void applyPublicShot(GameRuntime& runtime,
    const ai::SemanticShotAction& action)
{
    GameAction yaw;
    yaw.type = ActionType::SetAimYaw;
    yaw.first = action.controls.yawRadians;
    expect(runtime.dispatch(yaw).ok, "AI SetAimYaw failed");
    GameAction power;
    power.type = ActionType::SetShotPower;
    power.first = action.controls.requestedPower;
    expect(runtime.dispatch(power).ok, "AI SetShotPower failed");
    expect(runtime.setTipOffset(action.controls.tipOffsetX,
               action.controls.tipOffsetY).ok,
        "AI SetTipOffset failed");
    expect(runtime.setCalledPocket(action.targetPocket).ok,
        "AI SetCalledPocket failed");
    GameAction shoot;
    shoot.type = ActionType::Shoot;
    const auto result = runtime.dispatch(shoot);
    expect(result.ok, std::string("AI Shoot failed: ") + result.errorCode);
}

struct MatchRun {
    bool complete = false;
    int winner = -1;
    int shots = 0;
    int physicsTicks = 0;
    int fouls = 0;
    int pots = 0;
    int terminalShooter = -1;
    int terminalFirstContact = -1;
    int terminalCalledPocket = -1;
    int terminalEightPocket = -1;
    bool terminalFoul = false;
    bool terminalCueScratch = false;
    std::vector<int> terminalPotted;
    std::string terminalReason;
    std::string semanticActionSha256;
    std::vector<std::string> shotStateSha256;
    std::string finalStateCanonical;
    std::string finalStateSha256;
    std::string blocker;
};

std::string semanticActionSha256(const GameRuntime& runtime)
{
    std::ostringstream canonical;
    canonical << std::hexfloat;
    for (const AppliedSemanticAction& action :
            runtime.appliedSemanticActions()) {
        canonical << action.sequence << ':' << action.tick << ':'
                  << static_cast<int>(action.type) << ':'
                  << action.first << ':' << action.second << ';';
    }
    return sha256Hex(canonical.str());
}

MatchRun runMatch(std::uint64_t seed)
{
    GameRuntime runtime;
    const PhysicsProfile profile = interactiveChinesePoolPhysicsProfile();
    expect(runtime.replaceStateForScenario(runtime.state(), profile).ok,
        "shipping interactive initial rack must install");
    expect(runtime.setRulesMode(RulesMode::ChineseEightBallV1).ok,
        "full match must enable Chinese eight-ball rules");
    DecisionCounters decisions;
    MatchRun run;
    for (int shot = 0; shot < 96 && !runtime.state().gameOver; ++shot) {
        resolvePendingDecisions(runtime, decisions);
        if (runtime.state().gameOver) break;
        const auto plan = chooseShot(runtime, seed +
            static_cast<std::uint64_t>(shot) * 0x9e3779b97f4a7c15ull);
        if (!plan.found) {
            run.blocker = "no_shot:" + plan.error;
            break;
        }
        applyPublicShot(runtime, plan.action);
        const int beforePocketed = runtime.state().pocketedBallCount;
        int ticks = 0;
        while (runtime.state().ballsMoving && ticks < 3600) {
            const ActionResult step = runtime.step(1);
            if (!step.ok) {
                std::ostringstream error;
                error << "step:" << step.errorCode << ":shot=" << shot
                      << ":tick=" << ticks
                      << ":observation="
                      << runtime.tableDepartureObservationError();
                if (runtime.hasRecentPhysicsStep()) {
                    const auto& telemetry =
                        runtime.latestPhysicsStep().telemetry;
                    error << ":detail=" << telemetry.failureDetail
                          << ":contacts=" << telemetry.contacts.size()
                          << ":private_events="
                          << telemetry.tableDepartureEvents.size()
                          << ":segments="
                          << telemetry.analyticSurfaceMotion.size();
                    for (const auto& event :
                            telemetry.tableDepartureEvents) {
                        error << ":event=" << static_cast<int>(event.kind)
                              << ',' << event.ballIndex << ','
                              << event.absoluteWholeTickTimeSeconds << ','
                              << event.stableIdentity;
                    }
                }
                run.blocker = error.str();
                return run;
            }
            ++ticks;
        }
        ++run.shots;
        run.physicsTicks += ticks;
        if (ticks == 3600 && runtime.state().ballsMoving) {
            run.blocker = "physics_tick_budget";
            break;
        }
        if (!runtime.hasLastAdjudication()) {
            run.blocker = "missing_rules_adjudication";
            break;
        }
        if (runtime.lastAdjudication().foul) ++run.fouls;
        run.pots += std::max(0,
            runtime.state().pocketedBallCount - beforePocketed);
        std::cout << "shot=" << shot + 1
                  << " player=" << runtime.lastShotFacts().shooter
                  << " first=" << runtime.lastShotFacts().firstContactBall
                  << " potted="
                  << runtime.lastShotFacts().pocketedBallsInOrder.size()
                  << " foul=" << runtime.lastAdjudication().foul
                  << " phase=" << static_cast<int>(runtime.matchState().phase)
                  << " next=" << runtime.matchState().currentPlayer
                  << " ticks=" << ticks << '\n';
        run.shotStateSha256.push_back(gameStateSha256(runtime.state()));
    }
    resolvePendingDecisions(runtime, decisions);
    run.complete = runtime.state().gameOver &&
        runtime.matchState().phase == rules::MatchPhase::Complete;
    run.winner = runtime.matchState().winner;
    run.finalStateSha256 = gameStateSha256(runtime.state());
    run.semanticActionSha256 = semanticActionSha256(runtime);
    run.finalStateCanonical = canonicalGameState(runtime.state());
    if (runtime.hasLastShotFacts()) {
        run.terminalShooter = runtime.lastShotFacts().shooter;
        run.terminalFirstContact = runtime.lastShotFacts().firstContactBall;
        run.terminalCalledPocket = runtime.lastShotFacts().calledPocket;
        run.terminalEightPocket = runtime.lastShotFacts().eightBallPocket;
        run.terminalCueScratch = runtime.lastShotFacts().cueBallPocketed ||
            runtime.lastShotFacts().cueBallOffTable;
        run.terminalPotted = runtime.lastShotFacts().pocketedBallsInOrder;
    }
    if (runtime.hasLastAdjudication())
        run.terminalFoul = runtime.lastAdjudication().foul;
    if (run.complete && std::find(run.terminalPotted.begin(),
            run.terminalPotted.end(), 8) != run.terminalPotted.end()) {
        if (!run.terminalFoul && !run.terminalCueScratch &&
            run.terminalShooter == run.winner &&
            run.terminalFirstContact == 8)
            run.terminalReason = "legal_eight_win";
        else if (run.terminalFoul || run.terminalCueScratch)
            run.terminalReason = "foul_eight_loss";
        else
            run.terminalReason = "early_or_wrong_eight_loss";
    } else if (run.complete) {
        run.terminalReason = "rules_terminal_without_eight";
    }
    if (!run.complete && run.blocker.empty()) run.blocker = "shot_limit";
    return run;
}

int authorityActor(const GameRuntime& runtime)
{
    if (runtime.breakFoulDecisionOffer())
        return runtime.breakFoulDecisionOffer()->actorPlayerId;
    if (runtime.eightOnBreakDecisionOffer())
        return runtime.eightOnBreakDecisionOffer()->actorPlayerId;
    if (runtime.groupChoiceDecisionOffer())
        return runtime.groupChoiceDecisionOffer()->actorPlayerId;
    if (runtime.ballInHandPlacementOffer())
        return runtime.ballInHandPlacementOffer()->actorPlayerId;
    return runtime.matchState().currentPlayer;
}

std::string physicsFailure(const GameRuntime& runtime,
    const ActionResult& step, int shot, int tick)
{
    std::ostringstream error;
    error << std::setprecision(17);
    error << "step:" << step.errorCode << ":shot=" << shot
          << ":tick=" << tick << ":observation="
          << runtime.tableDepartureObservationError();
    if (runtime.hasRecentPhysicsStep()) {
        const auto& telemetry = runtime.latestPhysicsStep().telemetry;
        error << ":detail=" << telemetry.failureDetail
              << ":contacts=" << telemetry.contacts.size()
              << ":private_events="
              << telemetry.tableDepartureEvents.size()
              << ":segments=" << telemetry.analyticSurfaceMotion.size();
        for (const auto& event : telemetry.tableDepartureEvents)
            error << ":event=" << static_cast<int>(event.kind) << ','
                  << event.ballIndex << ','
                  << event.absoluteWholeTickTimeSeconds << ','
                  << event.ordinal << ',' << event.solverEventId << ','
                  << event.linkedPhysicsContactOrdinal << ','
                  << static_cast<int>(event.sourceDomain) << ','
                  << static_cast<int>(event.targetDomain) << ','
                  << event.velocityAfterCmS.y << ','
                  << static_cast<int>(event.clothReturnProvenance) << ','
                  << event.railFeatureId << ',' << event.stableIdentity;
        for (const auto& event : telemetry.tableDepartureEvents) {
            if (event.linkedPhysicsContactOrdinal < 0 ||
                static_cast<std::size_t>(event.linkedPhysicsContactOrdinal) >=
                    telemetry.contacts.size())
                continue;
            const auto& contact = telemetry.contacts[static_cast<std::size_t>(
                event.linkedPhysicsContactOrdinal)];
            error << ":linked=" << event.ordinal << ','
                  << event.linkedPhysicsContactOrdinal << ','
                  << static_cast<int>(contact.kind) << ','
                  << contact.firstBall << ',' << contact.secondBall << ','
                  << contact.solverEventId << ',' << contact.solverIslandId
                  << ',' << contact.wholeTickTimeOfImpactSeconds;
        }
    }
    return error.str();
}

MatchRun runShippingControllers(std::uint64_t seed)
{
    GameRuntime runtime;
    const PhysicsProfile profile = interactiveChinesePoolPhysicsProfile();
    expect(runtime.replaceStateForScenario(runtime.state(), profile).ok,
        "shipping AI initial rack must install");
    expect(runtime.setRulesMode(RulesMode::ChineseEightBallV1).ok,
        "shipping AI match must enable Chinese eight-ball rules");
    replay::Recorder recorder;
    const std::string replayExecutable = "current-ai-full-rack-acceptance";
    const std::string replayContent = sha256Hex("official-ai-full-rack-fixture");
    expect(recorder.begin(runtime, replayExecutable, replayContent,
        static_cast<std::uint32_t>(seed)).ok, "AI full-rack recording starts");
    std::array<ai::AiPlayerController, 2> players;
    for (int player = 0; player < 2; ++player) {
        ai::PlayerConfiguration configuration;
        configuration.enabled = true;
        configuration.player = player;
        configuration.difficulty = ai::Difficulty::Expert;
        configuration.deterministicSeed = seed +
            static_cast<std::uint64_t>(player) * 0x517cc1b727220a95ull;
        expect(players[player].configure(runtime, configuration).ok,
            "shipping AI player must configure on the initial rack");
    }

    MatchRun run;
    int maximumShots = 96;
    if (const char* configured = std::getenv("BILLIARDGL_AI_MAX_SHOTS"))
        maximumShots = std::max(1, std::atoi(configured));
    while (!runtime.state().gameOver && run.shots < maximumShots) {
        int polls = 0;
        bool applied = false;
        while (!runtime.state().ballsMoving && !runtime.state().gameOver &&
            polls++ < 6000) {
            const int actor = authorityActor(runtime);
            expect(actor == 0 || actor == 1,
                "shipping authority must name one configured AI actor");
            const ai::PlayerState state = players[actor].poll(runtime);
            const auto captured = recorder.captureApplied(runtime);
            expect(captured.ok, "AI full-rack recording capture: " + captured.errorCode);
            if (state.status == ai::PlayerPollStatus::Applied) {
                applied = true;
                break;
            }
            if (state.status == ai::PlayerPollStatus::NoShot ||
                state.status == ai::PlayerPollStatus::Rejected) {
                run.blocker = std::string(ai::playerPollStatusId(state.status)) +
                    ':' + state.error + ":actor=" + std::to_string(actor) +
                    ":jobs=" + std::to_string(state.jobsStarted);
                return run;
            }
            std::this_thread::sleep_for(std::chrono::milliseconds(1));
        }
        if (runtime.state().gameOver) break;
        if (!applied || !runtime.state().ballsMoving) {
            run.blocker = "controller_poll_budget";
            break;
        }

        const ShotControlModel appliedControls = runtime.shotControl();
        const Point3 cueAtShot = runtime.state().balls[0].position;
        const int solidsBefore = activeGroupBalls(
            runtime, rules::BallGroup::Solids);
        const int stripesBefore = activeGroupBalls(
            runtime, rules::BallGroup::Stripes);
        const int beforePocketed = runtime.state().pocketedBallCount;
        int ticks = 0;
        while (runtime.state().ballsMoving && ticks < 3600) {
            const ActionResult step = runtime.step(1);
            if (!step.ok) {
                run.blocker = physicsFailure(runtime, step, run.shots, ticks);
                return run;
            }
            ++ticks;
        }
        ++run.shots;
        run.physicsTicks += ticks;
        if (ticks == 3600 && runtime.state().ballsMoving) {
            run.blocker = "physics_tick_budget";
            break;
        }
        if (!runtime.hasLastAdjudication()) {
            std::ostringstream diagnostic;
            diagnostic << physicsFailure(runtime,
                ActionResult{false, "missing_rules_adjudication"},
                run.shots - 1, ticks)
                       << ":runtime_tick=" << runtime.tick()
                       << ":balls_moving=" << runtime.state().ballsMoving
                       << ":shot_ended=" << runtime.state().events.shotEnded
                       << ":collector_active="
                       << runtime.shotFactCollectorActive()
                       << ":table_observation_committed="
                       << runtime.hasTableDepartureObservation()
                       << ":has_shot_facts=" << runtime.hasLastShotFacts()
                       << ":completion_error="
                       << runtime.rulesShotCompletionError()
                       << ":phase="
                       << static_cast<int>(runtime.matchState().phase)
                       << ":current_player="
                       << runtime.matchState().currentPlayer;
            run.blocker = diagnostic.str();
            break;
        }
        if (runtime.lastAdjudication().foul) ++run.fouls;
        run.pots += std::max(0,
            runtime.state().pocketedBallCount - beforePocketed);
        std::cout << "product_shot=" << run.shots
                  << " player=" << runtime.lastShotFacts().shooter
                  << " first=" << runtime.lastShotFacts().firstContactBall
                  << " potted="
                  << runtime.lastShotFacts().pocketedBallsInOrder.size()
                  << " foul=" << runtime.lastAdjudication().foul
                  << " phase=" << static_cast<int>(runtime.matchState().phase)
                  << " next=" << runtime.matchState().currentPlayer
                  << " ticks=" << ticks
                  << " power=" << appliedControls.requestedPower
                  << " yaw=" << appliedControls.yawRadians
                  << " cue=" << cueAtShot.x << ',' << cueAtShot.z
                  << " groups_before=" << solidsBefore << ','
                  << stripesBefore << " reasons=";
        if (runtime.lastAdjudication().reasons.empty()) {
            std::cout << "none";
        } else {
            for (std::size_t index = 0;
                    index < runtime.lastAdjudication().reasons.size(); ++index) {
                if (index != 0) std::cout << ',';
                std::cout << rules::foulReasonCode(
                    runtime.lastAdjudication().reasons[index]);
            }
        }
        std::cout << '\n';
        run.shotStateSha256.push_back(gameStateSha256(runtime.state()));
    }
    run.complete = runtime.state().gameOver &&
        runtime.matchState().phase == rules::MatchPhase::Complete;
    run.winner = runtime.matchState().winner;
    run.finalStateSha256 = gameStateSha256(runtime.state());
    run.semanticActionSha256 = semanticActionSha256(runtime);
    run.finalStateCanonical = canonicalGameState(runtime.state());
    if (runtime.hasLastShotFacts()) {
        run.terminalShooter = runtime.lastShotFacts().shooter;
        run.terminalFirstContact = runtime.lastShotFacts().firstContactBall;
        run.terminalCalledPocket = runtime.lastShotFacts().calledPocket;
        run.terminalEightPocket = runtime.lastShotFacts().eightBallPocket;
        run.terminalCueScratch = runtime.lastShotFacts().cueBallPocketed ||
            runtime.lastShotFacts().cueBallOffTable;
        run.terminalPotted = runtime.lastShotFacts().pocketedBallsInOrder;
    }
    if (runtime.hasLastAdjudication())
        run.terminalFoul = runtime.lastAdjudication().foul;
    if (run.complete && std::find(run.terminalPotted.begin(),
            run.terminalPotted.end(), 8) != run.terminalPotted.end()) {
        if (!run.terminalFoul && !run.terminalCueScratch &&
            run.terminalShooter == run.winner &&
            run.terminalFirstContact == 8)
            run.terminalReason = "legal_eight_win";
        else if (run.terminalFoul || run.terminalCueScratch)
            run.terminalReason = "foul_eight_loss";
        else
            run.terminalReason = "early_or_wrong_eight_loss";
    } else if (run.complete) {
        run.terminalReason = "rules_terminal_without_eight";
    }
    if (!run.complete && run.blocker.empty()) run.blocker = "shot_limit";
    if (run.complete) {
        const auto finished = recorder.finish(runtime, run.shots);
        expect(finished.ok, "AI full-rack recording finish: " + finished.errorCode);
        replay::Document document;
        const auto parsed = replay::parse(replay::serialize(recorder.document()), document);
        expect(parsed.ok, "AI complete recording round-trip: " + parsed.errorCode);
        replay::Player playback;
        const auto loaded = playback.load(document, profile, replayExecutable, replayContent);
        expect(loaded.ok, "AI complete recording load: " + loaded.errorCode);
        while (!playback.finished()) {
            const auto step = playback.stepTick();
            expect(step.ok, "AI complete recording playback: " + step.errorCode);
        }
        expect(playback.finalHashMatches() &&
            playback.runtime().matchState().winner == run.winner &&
            playback.runtime().rulesReplayEffectStateHash() == runtime.rulesReplayEffectStateHash(),
            "AI complete single-rack playback preserves winner and final physical/rules digests");
        for (const int ordinal : {0, run.shots / 2, run.shots - 1}) {
            const auto sought = playback.seekShot(static_cast<std::size_t>(ordinal));
            expect(sought.ok, "AI full-rack seek: " + sought.errorCode);
            while (!playback.finished()) {
                const auto step = playback.stepTick();
                expect(step.ok, "AI playback after seek: " + step.errorCode);
            }
            expect(playback.finalHashMatches() &&
                playback.runtime().matchState().winner == run.winner &&
                playback.runtime().rulesReplayEffectStateHash() == runtime.rulesReplayEffectStateHash(),
                "full-rack seek preserves final winner and physical/rules digests");
            std::cout << "full_match_seek ordinal=" << ordinal << " verified=1\n";
        }
        std::cout << "full_match_replay verified=1 actions=" << document.actions.size()
                  << " receipts=" << document.rulesReceipts.size() << '\n';
    }
    return run;
}

}  // namespace

int main()
{
    try {
        const MatchRun run = runShippingControllers(0x4d41544348ull);
        std::cout << "full_match complete=" << run.complete
                  << " winner=" << run.winner << " shots=" << run.shots
                  << " ticks=" << run.physicsTicks
                  << " pots=" << run.pots << " fouls=" << run.fouls
                  << " terminal_shooter=" << run.terminalShooter
                  << " terminal_first=" << run.terminalFirstContact
                  << " terminal_called=" << run.terminalCalledPocket
                  << " terminal_eight_pocket=" << run.terminalEightPocket
                  << " terminal_foul=" << run.terminalFoul
                  << " terminal_scratch=" << run.terminalCueScratch
                  << " terminal_call_matches="
                  << (run.terminalCalledPocket == run.terminalEightPocket)
                  << " rules_require_called_eight="
                  << rules::ChineseEightBallRuleset{}.requireCalledEightPocket
                  << " terminal_reason=" << run.terminalReason
                  << " action_sha=" << run.semanticActionSha256
                  << " state=" << run.finalStateSha256
                  << " blocker=" << run.blocker << '\n';
        if (std::getenv("BILLIARDGL_AI_DUMP_STATE")) {
            for (std::size_t shot = 0; shot < run.shotStateSha256.size(); ++shot)
                std::cout << "shot_state=" << shot + 1 << ':'
                          << run.shotStateSha256[shot] << '\n';
            std::cout << "final_state_canonical_begin\n"
                      << run.finalStateCanonical
                      << "final_state_canonical_end\n";
        }
        expect(run.complete,
            "initial-rack AI match did not reach a rules-complete terminal state: " +
                run.blocker);
        expect(run.winner == 0 || run.winner == 1,
            "completed AI match must declare one winner");
        expect(std::find(run.terminalPotted.begin(), run.terminalPotted.end(),
                   8) != run.terminalPotted.end() &&
                run.terminalEightPocket >= 0 &&
                run.terminalReason != "rules_terminal_without_eight",
            "fixed full match must report the concrete 8-ball terminal reason");
        if (rules::ChineseEightBallRuleset{}.requireCalledEightPocket &&
            run.terminalReason == "legal_eight_win")
            expect(run.terminalCalledPocket == run.terminalEightPocket,
                "called-eight ruleset must reject a wrong-pocket 8-ball");
        return EXIT_SUCCESS;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return EXIT_FAILURE;
    }
}
