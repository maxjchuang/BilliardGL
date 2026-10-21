#include "deterministic_hash.h"
#include "replay/replay_recorder.h"
#include "replay/replay_transport_model.h"
#include "training/preview_runtime.h"
#include "training/training_product_controller.h"
#include "training/trajectory_overlay_model.h"

#include <algorithm>
#include <cmath>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <iterator>
#include <set>
#include <stdexcept>
#include <string>

namespace {

void expect(bool value, const std::string& message)
{
    if (!value) throw std::runtime_error(message);
}

bool close(double first, double second, double tolerance = 0.0001)
{
    return std::fabs(first - second) <= tolerance;
}

std::set<std::string> allCapabilities()
{
    return {"training_scenario_v1", "authoritative_horizontal_tip_offset_v1",
        "authoritative_longitudinal_tip_offset_v1"};
}

billiardgl::training::AttemptFacts successfulFacts(
    const billiardgl::training::Scenario& scenario)
{
    billiardgl::training::AttemptFacts facts;
    billiardgl::GameRuntime runtime;
    expect(billiardgl::applyPhysicsScenario(runtime, scenario.initialScenario).ok,
        scenario.id + " fixture applies");
    facts.state = runtime.state();
    facts.shotCount = scenario.scoring.at("par_shots").asInt();
    facts.traceSha256 = std::string(64, 'a');
    for (const billiardgl::json::Value& condition : scenario.success.asArray()) {
        const std::string name = condition.at("predicate").asString();
        if (name == "ball_pocketed_in_pocket") {
            const int ball = condition.at("ball_index").asInt();
            facts.state.balls[ball].pocketed = true;
            facts.pocketIds[ball] = condition.at("pocket_id").asInt();
        } else if (name == "ball_pocketed")
            facts.state.balls[condition.at("ball_index").asInt()].pocketed = true;
        else if (name == "ball_not_pocketed")
            facts.state.balls[condition.at("ball_index").asInt()].pocketed = false;
        else if (name == "ball_final_position_in_zone") {
            const int index = condition.at("ball_index").asInt();
            facts.state.balls[index].position.x = static_cast<float>(
                condition.at("center_cm").asArray()[0].asNumber());
            facts.state.balls[index].position.z = static_cast<float>(
                condition.at("center_cm").asArray()[1].asNumber());
        } else if (name == "first_contact_ball") {
            facts.firstContactBall = condition.at("ball_index").asInt();
        } else if (name == "cue_ball_final_near_first_contact_point") {
            facts.hasCueBallPositionAtFirstContact = true;
            facts.cueBallPositionAtFirstContactCm =
                facts.state.balls[0].position;
        } else if (name == "rail_contact_count_gte") {
            facts.railContacts[condition.at("ball_index").asInt()] =
                condition.at("minimum").asInt();
        } else if (name == "distinct_object_balls_with_rail_contact_gte") {
            facts.distinctObjectBallsWithRailContact =
                condition.at("minimum").asInt();
        } else if (name == "ordered_ball_pockets") {
            for (const billiardgl::json::Value& value :
                    condition.at("ball_indices").asArray())
                facts.pocketOrder.push_back(value.asInt());
        } else if (name == "shot_count_lte") {
            facts.shotCount = std::min(facts.shotCount,
                condition.at("maximum").asInt());
        }
    }
    return facts;
}

void testTrainingProductController()
{
    billiardgl::GameRuntime authority;
    billiardgl::training::TrainingProductController controller(authority);
    const std::filesystem::path manifest =
        std::filesystem::path(BILLIARDGL_SOURCE_ROOT) /
        "assets/training/phase4/training_pack_v1.json";
    expect(controller.loadPack(manifest.string(), allCapabilities()).ok,
        "Phase4 product controller loads the manifest-bound pack");
    expect(controller.scenarios().size() == 12 &&
        controller.snapshot().scenarioCount == 12,
        "product controller enumerates exactly the launch 12");

    std::size_t index = 0;
    for (const auto& card : controller.scenarios()) {
        expect(card.scoreEnabled && card.fileSha256.size() == 64,
            card.id + " is score-enabled and hash-bound");
        expect(controller.startScenario(card.id, "phase4-product-test-v1",
            static_cast<std::uint32_t>(100 + index)).ok,
            card.id + " starts through the product boundary");
        const billiardgl::training::Scenario* scenario =
            controller.currentScenario();
        expect(scenario != nullptr && scenario->id == card.id,
            card.id + " exposes current scenario data to UI");

        expect(controller.markShotStart().ok && controller.retryAttempt().ok,
            card.id + " supports retry from a marked pre-shot state");
        const auto successFacts = successfulFacts(*scenario);
        expect(controller.finishAttempt(successFacts).ok &&
            controller.snapshot().evaluation.succeeded &&
            controller.snapshot().evaluation.scoreEligible,
            card.id + " reaches its automated success result");
        const std::filesystem::path replayPath =
            std::filesystem::temp_directory_path() /
            ("billiardgl_" + card.id + ".replay.json");
        std::error_code ignored;
        std::filesystem::remove(replayPath, ignored);
        expect(controller.saveReplay(replayPath.string()).ok &&
            std::filesystem::file_size(replayPath) > 0,
            card.id + " saves a manifest-bound Replay v1");
        std::filesystem::remove(replayPath, ignored);

        expect(controller.resetAttempt().ok,
            card.id + " supports reset into a fresh recorded attempt");
        auto failureFacts = successFacts;
        failureFacts.numericalFailure = true;
        expect(controller.finishAttempt(failureFacts).ok &&
            controller.snapshot().evaluation.failed &&
            !controller.snapshot().evaluation.succeeded,
            card.id + " reaches its automated failure result");
        ++index;
    }

    expect(!controller.startScenario("missing_scenario",
        "phase4-product-test-v1", 1).ok,
        "unknown product scenario is rejected");
    expect(controller.startScenario(controller.scenarios().front().id,
        "phase4-product-test-v1", 999).ok,
        "product controller can launch a new scenario after evaluation");
    expect(!controller.startScenario(controller.scenarios().back().id,
        "phase4-product-test-v1", 1000).ok &&
        !controller.saveReplay(
            (std::filesystem::temp_directory_path() /
                "billiardgl_unfinished.replay.json").string()).ok,
        "active attempt rejects a competing start and premature save");
    expect(controller.markShotStart().ok,
        "invalid-state fixture marks its pre-shot state");
    billiardgl::GameAction movingPower;
    movingPower.type = billiardgl::ActionType::SetShotPower;
    movingPower.first = 25.0f;
    billiardgl::GameAction movingShot;
    movingShot.type = billiardgl::ActionType::Shoot;
    expect(authority.dispatch(movingPower).ok &&
        authority.dispatch(movingShot).ok && authority.state().ballsMoving &&
        !controller.resetAttempt().ok && !controller.retryAttempt().ok &&
        !controller.finishAttempt(billiardgl::training::AttemptFacts{}).ok,
        "moving authority rejects reset, retry, and attempt finalization");

    std::ifstream input(manifest, std::ios::binary);
    const std::string bytes((std::istreambuf_iterator<char>(input)),
        std::istreambuf_iterator<char>());
    billiardgl::json::Value tampered = billiardgl::json::parse(bytes).value;
    tampered["scenarios"].asArray()[0]["sha256"] =
        billiardgl::json::Value(std::string(64, '0'));
    const std::filesystem::path badRoot =
        std::filesystem::temp_directory_path() /
        "billiardgl_phase4_bad_binding_pack";
    std::error_code ignored;
    std::filesystem::remove_all(badRoot, ignored);
    std::filesystem::create_directories(badRoot / "scenarios");
    std::filesystem::copy_file(manifest.parent_path() /
        tampered["scenarios"].asArray()[0].at("file").asString(),
        badRoot / tampered["scenarios"].asArray()[0].at("file").asString());
    const std::filesystem::path badManifest = badRoot / "training_pack_v1.json";
    {
        std::ofstream output(badManifest, std::ios::binary | std::ios::trunc);
        output << billiardgl::json::stringify(tampered) << '\n';
    }
    billiardgl::GameRuntime badAuthority;
    billiardgl::training::TrainingProductController badController(badAuthority);
    const auto bad = badController.loadPack(badManifest.string(), allCapabilities());
    expect(!bad.ok && bad.errorCode == "training_pack_hash_mismatch",
        "manifest/scenario bad binding is rejected before launch");
    std::filesystem::remove_all(badRoot, ignored);
}

void runActualTrainingShot(billiardgl::GameRuntime& authority,
    billiardgl::training::TrainingProductController& controller,
    float power)
{
    billiardgl::GameAction setPower;
    setPower.type = billiardgl::ActionType::SetShotPower;
    setPower.first = power;
    billiardgl::GameAction shoot;
    shoot.type = billiardgl::ActionType::Shoot;
    expect(authority.dispatch(setPower).ok && controller.markShotStart().ok &&
        authority.dispatch(shoot).ok && controller.confirmShotDispatched().ok,
        "actual training Shoot uses the pre-dispatch lifecycle seam");
    int steps = 0;
    do {
        expect(authority.step(1).ok && controller.consumeAuthoritySteps().ok,
            "actual training shot consumes each authority step");
        ++steps;
    } while (authority.state().ballsMoving && steps < 6000);
    expect(!authority.state().ballsMoving && steps < 6000,
        "actual training shot reaches a stopped authority state");
}

void runFixedShippingTrainingShot(billiardgl::GameRuntime& authority,
    billiardgl::training::TrainingProductController& controller,
    float yaw, float power, float tipX, float tipY)
{
    billiardgl::GameAction aim;
    aim.type = billiardgl::ActionType::SetAimYaw;
    aim.first = yaw;
    billiardgl::GameAction setPower;
    setPower.type = billiardgl::ActionType::SetShotPower;
    setPower.first = power;
    billiardgl::GameAction shoot;
    shoot.type = billiardgl::ActionType::Shoot;
    expect(authority.dispatch(aim).ok &&
        authority.dispatch(setPower).ok &&
        authority.setTipOffset(tipX, tipY).ok &&
        controller.markShotStart().ok &&
        authority.dispatch(shoot).ok &&
        controller.confirmShotDispatched().ok,
        "fixed shipping reference action is accepted by the real controller");
    int steps = 0;
    do {
        expect(authority.step(1).ok && controller.consumeAuthoritySteps().ok,
            "fixed shipping reference consumes each real authority step");
        ++steps;
    } while (authority.state().ballsMoving && steps < 6000);
    expect(!authority.state().ballsMoving && steps < 6000,
        "fixed shipping reference settles within the bounded horizon");
}

void testFixedShippingFollowAndDrawReferences()
{
    struct Reference {
        const char* scenarioId;
        float yaw;
        float power;
        float tipY;
        int expectedTicks;
        int expectedScore;
    };
    const Reference references[] = {
        {"launch_follow_shot_v1", -1.6127963267948966f, 160.0f,
            0.25f, 1325, 88},
        {"launch_draw_shot_v1", -1.6167963267948966f, 159.0f,
            -0.4f, 1196, 93}};
    const std::filesystem::path manifest =
        std::filesystem::path(BILLIARDGL_SOURCE_ROOT) /
        "assets/training/phase4/training_pack_v1.json";
    for (const Reference& reference : references) {
        billiardgl::GameRuntime authority;
        billiardgl::training::TrainingProductController controller(
            authority, "billiardgl-product-source-v1");
        expect(controller.loadPack(manifest.string(), allCapabilities()).ok &&
            controller.startScenario(reference.scenarioId).ok,
            std::string(reference.scenarioId) +
                " loads through the real training product controller");
        expect(authority.physicsProfile().id ==
                "chinese_pool_interactive_120hz_v5" &&
            authority.boundaryMode() ==
                billiardgl::PhysicsBoundaryMode::ProductionTable,
            std::string(reference.scenarioId) +
                " uses the explicit shipping physics contract");
        runFixedShippingTrainingShot(authority, controller, reference.yaw,
            reference.power, 0.0f, reference.tipY);
        const auto snapshot = controller.snapshot();
        expect(snapshot.state ==
                billiardgl::training::TrainingProductState::AttemptEvaluated &&
            snapshot.evaluation.succeeded &&
            snapshot.evaluation.score == reference.expectedScore &&
            snapshot.attemptFacts.elapsedTicks == reference.expectedTicks &&
            snapshot.attemptFacts.shotCount == 1 && snapshot.replayReady &&
            controller.replayDocument() != nullptr &&
            controller.replayDocument()->finalSummary.shotCount == 1,
            std::string(reference.scenarioId) +
                " records and evaluates the independently repeated shipping action");
        const billiardgl::replay::Document& localReplay =
            *controller.replayDocument();
        const auto localAudit = billiardgl::training::auditReferenceReplay(
            localReplay, *controller.currentScenario(), authority.physicsProfile(),
            localReplay.executableVersion, localReplay.contentSha256);
        billiardgl::replay::ReplayTransportModel localTransport;
        expect(localAudit.result.ok &&
            localTransport.load(localReplay, authority.physicsProfile(),
                localReplay.executableVersion, localReplay.contentSha256).ok,
            std::string(reference.scenarioId) +
                " replays a same-build Recorder document with a strict terminal hash");
        while (!localTransport.snapshot().finished)
            expect(localTransport.stepTick().ok,
                "same-build reference transport advances exactly one fixed tick");
        expect(localTransport.player().finalHashMatches(),
            std::string(reference.scenarioId) +
                " same-build Recorder and Player agree at the endpoint");
    }
}

billiardgl::replay::Document loadReplayDocument(
    const std::filesystem::path& path)
{
    std::ifstream input(path, std::ios::binary);
    const std::string bytes((std::istreambuf_iterator<char>(input)),
        std::istreambuf_iterator<char>());
    const auto decoded = billiardgl::json::parse(bytes);
    billiardgl::replay::Document document;
    const auto parsed = decoded.ok ? billiardgl::replay::parse(
        decoded.value, document) :
        billiardgl::replay::ValidationResult{false, "replay_format_invalid"};
    expect(parsed.ok, "reference Replay v1 fixture parses");
    return document;
}

void testAllBoundReferenceReplays()
{
    billiardgl::GameRuntime authority;
    billiardgl::training::TrainingProductController controller(
        authority, "billiardgl-product-source-v1");
    const std::filesystem::path manifest =
        std::filesystem::path(BILLIARDGL_SOURCE_ROOT) /
        "assets/training/phase4/training_pack_v1.json";
    expect(controller.loadPack(manifest.string(), allCapabilities()).ok,
        "bound reference pack loads through the product controller");
    for (const auto& card : controller.scenarios()) {
        expect(card.referenceReplay.bound &&
            card.referenceReplay.reviewStatus == "automated_verified",
            card.id + " exposes only the truthfully automated reference");
        expect(controller.startScenario(card.id).ok,
            card.id + " reference scenario starts");
        const std::string liveHash = billiardgl::replay::replayAuthorityStateSha256(
            authority.state());
        const std::uint64_t liveTick = authority.tick();
#ifdef NDEBUG
        billiardgl::replay::ReplayTransportModel transport;
        const auto loaded = controller.loadReferenceReplay(transport);
        expect(loaded.ok && transport.snapshot().loaded &&
            transport.snapshot().paused && transport.snapshot().tick == 0,
            card.id + " shipping Release reference is fully replayed/scored then loaded paused");
#endif
        expect(liveHash == billiardgl::replay::replayAuthorityStateSha256(
                authority.state()) && liveTick == authority.tick(),
            card.id + " reference verification never mutates live training authority");

        const billiardgl::replay::Document document = loadReplayDocument(
            card.referenceReplay.filePath);
        auto wrongInitial = document;
        wrongInitial.initialState.balls[0].position.x += 0.25f;
        const auto initialRejected =
            billiardgl::training::auditReferenceReplay(wrongInitial,
                *controller.currentScenario(), authority.physicsProfile(),
                document.executableVersion, card.fileSha256);
        expect(!initialRejected.result.ok &&
            initialRejected.result.errorCode ==
                "training_reference_initial_state_mismatch",
            card.id + " rejects a replay from a different initial layout");

        auto excessiveTick = document;
        excessiveTick.finalSummary.tick = 18001;
        const auto tickRejected =
            billiardgl::training::auditReferenceReplay(excessiveTick,
                *controller.currentScenario(), authority.physicsProfile(),
                document.executableVersion, card.fileSha256);
        expect(!tickRejected.result.ok &&
            tickRejected.result.errorCode ==
                "training_reference_tick_limit_exceeded",
            card.id + " rejects an excessive endpoint before replaying it");
        expect(controller.abandonAttempt().ok,
            card.id + " exits without changing replay or live authority");
    }
}

void testActualMultiShotTerminalLifecycle()
{
    billiardgl::GameRuntime authority;
    billiardgl::training::TrainingProductController controller(
        authority, "phase4-session-attestation-v1");
    const std::filesystem::path manifest =
        std::filesystem::path(BILLIARDGL_SOURCE_ROOT) /
        "assets/training/phase4/training_pack_v1.json";
    expect(controller.loadPack(manifest.string(), allCapabilities()).ok,
        "actual lifecycle loads the bound launch pack");
    const auto& card = controller.scenarios().front();
    expect(!card.titleEn.empty() && !card.titleZhCn.empty() &&
        !card.descriptionEn.empty() && !card.descriptionZhCn.empty() &&
        !card.recommendedAssist.empty() && !card.successSummary.empty() &&
        !card.successEvaluation.empty() && !card.failureEvaluation.empty() &&
        !card.resetMode.empty() && !card.resetPreserve.empty(),
        "catalog card retains locale, assist, success, failure, and reset metadata");
    expect(controller.startScenario(card.id).ok,
        "session-attested executable version starts the scenario");
    const auto launched = controller.snapshot();
    expect(launched.executableVersion == "phase4-session-attestation-v1" &&
        launched.attemptSeed == card.attemptSeed &&
        launched.attemptSeed == 0 &&
        launched.scenarioFileSha256 == card.fileSha256,
        "replay binding uses injected executable attestation and card reset seed");
    expect(controller.refreshPreview().ok &&
        controller.snapshot().previewReady &&
        controller.snapshot().previewAuthorityUnchanged,
        "preview control produces an isolated authority-safe result");

    runActualTrainingShot(authority, controller, 1.0f);
    const auto first = controller.snapshot();
    expect(first.state ==
            billiardgl::training::TrainingProductState::AttemptActive &&
        first.attemptFacts.shotCount == 1 && !first.replayReady &&
        !first.evaluation.succeeded && !first.evaluation.failed,
        "non-terminal stopped shot accumulates facts without sealing replay");

    runActualTrainingShot(authority, controller, 1.0f);
    const auto terminal = controller.snapshot();
    expect(terminal.state ==
            billiardgl::training::TrainingProductState::AttemptEvaluated &&
        terminal.attemptFacts.shotCount == 2 && terminal.replayReady &&
        terminal.evaluation.failed &&
        terminal.evaluation.reason == "shot_limit_exceeded",
        "second stopped shot reaches terminal failure and only then seals replay");
    const billiardgl::replay::Document* replay = controller.replayDocument();
    expect(replay != nullptr && replay->finalSummary.shotCount == 2 &&
        replay->seed == card.attemptSeed &&
        replay->executableVersion == "phase4-session-attestation-v1",
        "terminal replay contains the accumulated two-shot authority contract");

    expect(controller.retryAttempt().ok,
        "terminal retry restores pre-shot authority into a fresh attempt");
    const auto retried = controller.snapshot();
    expect(retried.state ==
            billiardgl::training::TrainingProductState::AttemptActive &&
        retried.attemptFacts.shotCount == 0 &&
        retried.attemptFacts.elapsedTicks == 0 && !retried.replayReady &&
        controller.replayDocument() == nullptr,
        "retry clears accumulator and recorder without cross-attempt contamination");
}

void testBeginnerV2TeachingDefaultsAndDynamicStopZone()
{
    billiardgl::GameRuntime authority;
    billiardgl::training::TrainingProductController controller(
        authority, "phase4-beginner-v2-test");
    const std::filesystem::path manifest =
        std::filesystem::path(BILLIARDGL_SOURCE_ROOT) /
        "assets/training/phase4/training_pack_v2.json";
    expect(controller.loadPack(manifest.string(), allCapabilities()).ok &&
        controller.scenarios().size() == 13,
        "v2 launch catalog loads both beginner introductions");
    const auto& pot = controller.scenarios()[0];
    const auto& stop = controller.scenarios()[1];
    expect(pot.id == "launch_straight_pot_intro_v2" &&
        stop.id == "launch_straight_stop_intro_v2" &&
        !pot.tipAdjustmentAllowed && pot.tipAdjustmentAxis == "none" &&
        stop.tipAdjustmentAllowed && stop.tipAdjustmentAxis == "vertical",
        "v2 cards expose exact beginner order and tip-axis permissions");
    expect(controller.startScenario(stop.id).ok,
        "straight stop introduction starts");
    const auto launched = controller.snapshot();
    expect(launched.teachingHints.available &&
        launched.teachingHints.cueBallTargetEnabled &&
        launched.tipAdjustmentAllowed &&
        std::fabs(authority.state().aim.yaw) < 1.0e-6f &&
        std::fabs(authority.state().input.shotPower - 50.0f) < 1.0e-6f &&
        std::fabs(authority.shotControl().tipOffsetX) < 1.0e-6f &&
        std::fabs(authority.shotControl().tipOffsetY) < 1.0e-6f,
        "start applies the advertised yaw, power, and centre-tip defaults");

    billiardgl::GameAction changedPower;
    changedPower.type = billiardgl::ActionType::SetShotPower;
    changedPower.first = 47.0f;
    expect(authority.dispatch(changedPower).ok &&
        authority.setTipOffset(0.0f, -0.15f).ok &&
        controller.markShotStart().ok && controller.retryAttempt().ok &&
        std::fabs(authority.state().input.shotPower - 47.0f) < 1.0e-6f &&
        std::fabs(authority.shotControl().tipOffsetY + 0.15f) < 1.0e-6f,
        "retry restores the user's pre-shot controls instead of teaching defaults");
    expect(authority.state().aim.mode == billiardgl::AimMode::Observe,
        "retry returns to Observe instead of leaving the shot armed");
    const float retriedYaw = authority.shotControl().yawRadians;
    billiardgl::GameAction retryToggle;
    retryToggle.type = billiardgl::ActionType::ToggleAim;
    expect(authority.dispatch(retryToggle).ok &&
        std::fabs(std::remainder(authority.shotControl().yawRadians - retriedYaw,
            6.283185307179586f)) < 1.0e-5f,
        "Tab after retry preserves the restored shot direction");

    const auto* scenario = controller.currentScenario();
    expect(scenario != nullptr, "v2 stop scenario remains active");
    auto facts = successfulFacts(*scenario);
    facts.hasCueBallPositionAtFirstContact = true;
    facts.cueBallPositionAtFirstContactCm = {34.285f, 89.341476f, 0.0f};
    facts.state.balls[0].position = {34.285f, 89.341476f, 0.0f};
    expect(billiardgl::training::evaluate(*scenario, facts).succeeded,
        "dynamic stop predicate accepts the actual first-contact centre");
    expect(controller.finishAttempt(facts).ok &&
        std::fabs(controller.snapshot().teachingHints.cueBallTargetCenterCm[0] -
            34.285f) < 1.0e-5f,
        "active teaching overlay follows the recorded first-contact centre");
    facts.state.balls[0].position.x += 4.51f;
    expect(!billiardgl::training::evaluate(*scenario, facts).succeeded,
        "dynamic stop predicate rejects outside its sealed radius");

    expect(controller.startScenario("launch_straight_follow_intro_v2").ok,
        "straight follow introduction starts after stop");
    const auto followHints = controller.snapshot().teachingHints;
    expect(followHints.available && followHints.cueBallMustAvoidRails &&
        !followHints.cueBallTargetTracksFirstContact && followHints.tipYBallRadii > 0.0f,
        "follow advertises forward tip and a fixed no-rail target");
    auto followFacts = successfulFacts(*controller.currentScenario());
    followFacts.hasCueBallPositionAtFirstContact = true;
    followFacts.cueBallPositionAtFirstContactCm = {34.285f, 89.341476f, 0.0f};
    // The default nonzero tip is a recorded semantic action at tick zero.
    // A synthetic finish must still obey the recorder's final-tick contract.
    expect(authority.step(1).ok, "follow fixture advances beyond default tip action");
    const auto followFinished = controller.finishAttempt(followFacts);
    expect(followFinished.ok, "follow fixture finish: " + followFinished.errorCode);
    expect(controller.snapshot().teachingHints.cueBallTargetCenterCm ==
            followHints.cueBallTargetCenterCm,
        "follow target must not jump backward to the first-contact position");

    expect(controller.startScenario("launch_straight_draw_intro_v2").ok,
        "straight draw introduction starts after follow");
    const auto drawHints = controller.snapshot().teachingHints;
    expect(drawHints.available && drawHints.cueBallMustAvoidRails &&
        !drawHints.cueBallTargetTracksFirstContact && drawHints.tipYBallRadii < 0.0f,
        "draw advertises low tip and a fixed no-rail target");
    auto drawFacts = successfulFacts(*controller.currentScenario());
    drawFacts.hasCueBallPositionAtFirstContact = true;
    drawFacts.cueBallPositionAtFirstContactCm = {34.285f, 89.341476f, 0.0f};
    expect(authority.step(1).ok && controller.finishAttempt(drawFacts).ok &&
        controller.snapshot().teachingHints.cueBallTargetCenterCm == drawHints.cueBallTargetCenterCm,
        "draw target stays fixed after contact");
    drawFacts.railContacts[0] = 1;
    expect(!billiardgl::training::evaluate(*controller.currentScenario(), drawFacts).succeeded,
        "draw disallows a rail-assisted return to the same successful endpoint");
    drawFacts.railContacts[0] = 0;
    drawFacts.state.balls[0].position = drawFacts.cueBallPositionAtFirstContactCm;
    expect(!billiardgl::training::evaluate(*controller.currentScenario(), drawFacts).succeeded,
        "stopping at first contact must not pass the draw lesson");
}

void testCompleteV2TeachingCatalog()
{
    billiardgl::GameRuntime authority;
    billiardgl::training::TrainingProductController controller(authority, "billiardgl-product-source-v1");
    expect(controller.loadPack((std::filesystem::path(BILLIARDGL_SOURCE_ROOT) /
        "assets/training/phase4/training_pack_v2.json").string(), allCapabilities()).ok,
        "complete v2 teaching catalog loads");
    for (const auto& card : controller.scenarios()) {
        // Each card is an independent attempt, including demo-only checks.
        expect(controller.abandonAttempt().ok, "previous teaching attempt leaves cleanly");
        expect(card.id.size() >= 3 && card.id.substr(card.id.size() - 3) == "_v2" &&
            card.teachingHints.available, "no legacy drill remains in the shipping catalog");
        expect(controller.startScenario(card.id).ok, "v2 teaching starts");
        const auto initialHints = controller.snapshot().teachingHints;
        const float initialYaw = authority.shotControl().yawRadians;
        for (int frame = 0; frame < 8; ++frame)
            expect(authority.step(1).ok,
                "idle teaching camera/physics frame advances");
        expect(std::fabs(initialYaw - initialHints.aimYawRadians) < 1e-4f &&
            std::fabs(authority.shotControl().yawRadians - initialYaw) < 1e-4f,
            "idle camera handoff never rewrites the authoritative teaching yaw; only explicit aim input may do so");
#ifdef NDEBUG
        billiardgl::replay::ReplayTransportModel demo;
        const auto loaded = controller.loadReferenceReplay(demo);
        expect(loaded.ok && demo.snapshot().paused,
            card.id + " actual Watch demo loading contract: " + loaded.errorCode);
#endif
        if (card.teachingSteps.size() < 2) continue;
        for (std::size_t step = 0; step < card.teachingSteps.size(); ++step) {
            const auto before = controller.snapshot();
            expect(before.teachingStepIndex == static_cast<int>(step), "next target follows actual pocket progress");
            if (step > 0) {
                billiardgl::GameAction tab;
                tab.type = billiardgl::ActionType::ToggleAim;
                expect(authority.dispatch(tab).ok && controller.prepareTeachingShot().ok,
                    "explicit preparation also works during an Aim transition");
                const float preparedYaw = authority.shotControl().yawRadians;
                expect(authority.dispatch(tab).ok && authority.dispatch(tab).ok &&
                    std::fabs(std::remainder(authority.shotControl().yawRadians - preparedYaw, 6.2831853f)) < 1e-4f,
                    "Tab after preparation preserves the next shot alignment");
            }
            const auto hints = controller.snapshot().teachingHints;
            const float power = authority.shotControl().requestedPower;
            expect(std::fabs(power - 2.0f * hints.defaultPowerPercent) < 1e-4f,
                "per-shot recommended power is applied");
            expect(controller.markShotStart().ok, "sequence shot marked");
            billiardgl::GameAction shoot;
            shoot.type = billiardgl::ActionType::Shoot;
            expect(authority.dispatch(shoot).ok && controller.confirmShotDispatched().ok,
                "sequence shot dispatched");
            for (int tick = 0; tick < 4000 && authority.state().ballsMoving; ++tick)
                expect(authority.step(1).ok && controller.consumeAuthoritySteps().ok, "sequence physics consumed");
            expect(!authority.state().ballsMoving &&
                authority.state().balls[hints.objectBallIndex].pocketed &&
                authority.shotControl().requestedPower == power,
                "pocketing advances hints but does not silently apply next controls");
            if (controller.snapshot().evaluation.succeeded)
                expect(controller.snapshot().teachingHints.shotLineObjectCenterCm == hints.shotLineObjectCenterCm,
                    "finished teaching line never follows the pocketed ball's storage position");
        }
        expect(controller.snapshot().evaluation.succeeded, "default multi-shot lesson passes real physics");
        expect(controller.retryAttempt().ok && controller.snapshot().teachingStepIndex == 0 &&
            authority.state().aim.mode == billiardgl::AimMode::Observe &&
            controller.snapshot().attemptFacts.shotCount == 0 &&
            !authority.state().balls[card.teachingSteps.front().objectBallIndex].pocketed,
            "restart drill restores the full sequence rather than losing prior-shot facts");
    }
}

billiardgl::replay::Document recordedReplay(
    billiardgl::PhysicsProfile& profile, std::string& executable,
    std::string& content, int ticks = 24)
{
    billiardgl::GameRuntime runtime;
    executable = "phase4-transport-test-v1";
    content = billiardgl::sha256Hex("phase4-transport-content");
    billiardgl::replay::Recorder recorder;
    expect(recorder.begin(runtime, executable, content, 91).ok,
        "transport fixture recording starts");
    billiardgl::GameAction yaw;
    yaw.type = billiardgl::ActionType::SetAimYaw;
    yaw.first = 1.5707963f;
    billiardgl::GameAction power;
    power.type = billiardgl::ActionType::SetShotPower;
    power.first = 55.0f;
    billiardgl::GameAction shoot;
    shoot.type = billiardgl::ActionType::Shoot;
    expect(runtime.dispatch(yaw).ok && runtime.dispatch(power).ok &&
        runtime.dispatch(shoot).ok && runtime.step(ticks).ok,
        "transport fixture advances fixed-step authority");
    expect(recorder.finish(runtime, 1).ok,
        "transport fixture recording finishes");
    profile = runtime.physicsProfile();
    return recorder.document();
}

void testReplayImpactCameraBoundaries()
{
    using namespace billiardgl;
    PhysicsProfile profile;
    std::string executable, content;
    const auto document = recordedReplay(profile, executable, content, 400);
    replay::ReplayTransportModel shaken, reference;
    for (auto* transport : {&shaken, &reference}) {
        expect(transport->load(document, profile, executable, content).ok &&
            transport->adjustCamera(0.4f, -0.1f, -20.0f).ok,
            "impact replay fixture loads with manual orbit");
    }
    camera::PresentationPreferences preferences;
    preferences.impactShakeEnabled = true;
    shaken.setCameraPresentationPreferences(preferences);
    bool sawImpact = false, sawFreeImpact = false, sawBroadcastImpact = false;
    auto sameEye = [](const camera::Pose& a, const camera::Pose& b) {
        return close(a.eye.x, b.eye.x) && close(a.eye.y, b.eye.y) &&
            close(a.eye.z, b.eye.z);
    };
    for (std::uint64_t tick = 0; tick < document.finalSummary.tick; ++tick) {
        expect(shaken.stepTick().ok && reference.stepTick().ok,
            "both camera settings replay identical fixed ticks");
        for (auto mode : {replay::ReplayCameraMode::Free,
                          replay::ReplayCameraMode::Broadcast}) {
            expect(shaken.setCameraMode(mode).ok && reference.setCameraMode(mode).ok,
                "impact checks both replay camera modes");
            const auto actual = shaken.presentationCameraFrame().pose;
            const auto base = reference.presentationCameraFrame().pose;
            expect(close(actual.yawRadians, base.yawRadians) &&
                close(actual.pitchRadians, base.pitchRadians) &&
                close(actual.target.x, base.target.x) &&
                close(actual.target.z, base.target.z),
                "replay impulse never changes authored orbit or target");
            if (!sameEye(actual, base)) {
                sawImpact = true;
                if (mode == replay::ReplayCameraMode::Free) sawFreeImpact = true;
                else sawBroadcastImpact = true;
                expect(sameEye(actual, shaken.presentationCameraFrame().pose),
                    "repeated frame readback does not reconsume impact");
            }
        }
        if (sawImpact) break;
    }
    expect(sawImpact && sawFreeImpact && sawBroadcastImpact,
        "actual replay collision drives both enabled camera modes");
    expect(shaken.setPaused(true).ok, "pause clears transient replay motion");
    for (auto mode : {replay::ReplayCameraMode::Free,
                      replay::ReplayCameraMode::Broadcast}) {
        shaken.setCameraMode(mode);
        reference.setCameraMode(mode);
        expect(sameEye(shaken.presentationCameraFrame().pose,
                       reference.presentationCameraFrame().pose),
            "pause restores exact base eye in both modes");
    }
    const auto pausedTick = shaken.snapshot().tick;
    expect(shaken.advancePresentedFrame(1.0).ok &&
        shaken.snapshot().tick == pausedTick,
        "paused frames neither advance physics nor replay stale impulses");
    expect(shaken.seekShot(0).ok && reference.seekShot(0).ok,
        "seek resets transient history");
    shaken.setCameraMode(replay::ReplayCameraMode::Free);
    reference.setCameraMode(replay::ReplayCameraMode::Free);
    expect(sameEye(shaken.presentationCameraFrame().pose,
                   reference.presentationCameraFrame().pose),
        "seek retains selected manual view without old impulse offset");
    bool seekDuringImpulse = false;
    for (std::uint64_t tick = shaken.snapshot().tick;
         tick < document.finalSummary.tick; ++tick) {
        expect(shaken.stepTick().ok && reference.stepTick().ok,
            "seek fixture reaches a fresh committed collision");
        if (!sameEye(shaken.presentationCameraFrame().pose,
                     reference.presentationCameraFrame().pose)) {
            seekDuringImpulse = true;
            break;
        }
    }
    expect(seekDuringImpulse && shaken.seekShot(0).ok && reference.seekShot(0).ok &&
        sameEye(shaken.presentationCameraFrame().pose,
                reference.presentationCameraFrame().pose),
        "seeking directly during active shake removes it without resetting orbit");
    for (std::uint64_t tick = shaken.snapshot().tick;
         tick < document.finalSummary.tick; ++tick)
        expect(shaken.stepTick().ok && reference.stepTick().ok,
            "seeked replay completes with either camera setting");
    expect(shaken.player().finalHashMatches() && reference.player().finalHashMatches() &&
        sameEye(shaken.presentationCameraFrame().pose,
                reference.presentationCameraFrame().pose),
        "finished replay has identical terminal physics and no residual shake");
}

void testReplayTransportAndOverlayIsolation()
{
    billiardgl::PhysicsProfile profile;
    std::string executable;
    std::string content;
    const billiardgl::replay::Document document =
        recordedReplay(profile, executable, content);
    billiardgl::replay::ReplayTransportModel transport;
    expect(!transport.setPlaybackRate(3.0).ok,
        "transport controls reject invalid unloaded state");
    expect(transport.load(document, profile, executable, content).ok,
        "transport loads validated Replay v1");
    billiardgl::replay::ReplayTransportModel cameraReplay;
    expect(cameraReplay.load(document, profile, executable, content).ok &&
        cameraReplay.adjustCamera(0.4f, -0.1f, -20.0f).ok,
        "replay camera accepts independent orbit and zoom");
    const auto selectedPose = cameraReplay.presentationCameraFrame().pose;
    cameraReplay.setCameraPresentationPreferences({5.0f, 1.2f, false});
    const auto preferredFreePose = cameraReplay.presentationCameraFrame().pose;
    expect(close(preferredFreePose.fieldOfViewDegrees,
                 selectedPose.fieldOfViewDegrees + 5.0f) &&
        close(preferredFreePose.distanceCm,
              selectedPose.distanceCm * 1.2f),
        "replay free camera consumes relative presentation preferences");
    for (std::uint64_t tick = 0; tick < document.finalSummary.tick; ++tick)
        expect(cameraReplay.stepTick().ok, "camera-controlled replay advances normally");
    expect(cameraReplay.player().finalHashMatches() &&
        cameraReplay.presentationCameraFrame().pose.yawRadians == preferredFreePose.yawRadians &&
        cameraReplay.presentationCameraFrame().pose.distanceCm == preferredFreePose.distanceCm,
        "manual camera persists to replay end with identical terminal physics hash");
    expect(cameraReplay.setCameraMode(
            billiardgl::replay::ReplayCameraMode::Broadcast).ok &&
        close(cameraReplay.presentationCameraFrame().pose.fieldOfViewDegrees,
              billiardgl::camera::definition(
                  billiardgl::camera::SemanticState::ReplayBroadcast).
                  fieldOfViewDegrees + 5.0f) &&
        close(cameraReplay.presentationCameraFrame().pose.distanceCm,
              billiardgl::camera::definition(
                  billiardgl::camera::Preset::Broadcast).distanceCm * 1.2f),
        "replay broadcast camera consumes the same relative preferences");
    expect(cameraReplay.resetCamera().ok &&
        close(cameraReplay.presentationCameraFrame().pose.fieldOfViewDegrees,
              billiardgl::camera::definition(
                  billiardgl::camera::SemanticState::ReplayFree).
                  fieldOfViewDegrees + 5.0f),
        "replay camera reset preserves presentation preferences");
    expect(transport.snapshot().cameraMode ==
        billiardgl::replay::ReplayCameraMode::Free &&
        transport.setPlaybackRate(0.5).ok &&
        transport.setPlaybackRate(1.0).ok,
        "transport defaults to free camera and exposes 1/2x and 1x");
    expect(!transport.setPlaybackRate(0.75).ok,
        "transport rejects rates outside 1/4x, 1/2x, 1x, 2x");
    expect(transport.setPlaybackRate(0.25).ok &&
        transport.setPaused(false).ok,
        "transport accepts 1/4x playback");
    const double fixedStep = transport.snapshot().fixedStepSeconds;
    for (int frame = 0; frame < 3; ++frame)
        expect(transport.advancePresentedFrame().ok,
            "slow-motion frame schedules cleanly");
    expect(transport.snapshot().tick == 0,
        "1/4x schedules no partial physics ticks");
    expect(transport.advancePresentedFrame().ok &&
        transport.snapshot().tick == 1,
        "fourth 1/4x frame schedules one whole fixed tick");
    expect(transport.setPlaybackRate(2.0).ok &&
        transport.advancePresentedFrame().ok &&
        transport.snapshot().tick == 3 &&
        transport.snapshot().fixedStepSeconds == fixedStep,
        "2x schedules two fixed ticks without changing solver dt");
    expect(transport.setPaused(true).ok,
        "transport can pause");
    const std::uint64_t pausedTick = transport.snapshot().tick;
    expect(transport.advancePresentedFrame().ok &&
        transport.snapshot().tick == pausedTick,
        "paused transport does not advance");
    expect(transport.stepTick().ok &&
        transport.snapshot().tick == pausedTick + 1,
        "single-step advances exactly one tick while paused");
    expect(transport.seekShot(0).ok && transport.snapshot().tick == 0,
        "seek-to-shot reconstructs isolated replay state");
    expect(!transport.seekShot(99).ok,
        "seek rejects absent shot ordinals");
    expect(transport.setCameraMode(
        billiardgl::replay::ReplayCameraMode::Broadcast).ok &&
        transport.snapshot().cameraMode ==
            billiardgl::replay::ReplayCameraMode::Broadcast,
        "transport exposes free/broadcast camera policy");
    expect(transport.stepTick().ok && transport.snapshot().tick == 1,
        "transactional load control advances the original replay");
    const auto beforeRejectedLoad = transport.snapshot();
    const std::string beforeRejectedHash =
        transport.player().finalStateSha256();
    auto invalidZeroTick = document;
    invalidZeroTick.actions.clear();
    invalidZeroTick.finalSummary.tick = 0;
    invalidZeroTick.finalSummary.shotCount = 0;
    invalidZeroTick.finalSummary.stateSha256 = std::string(64, '0');
    const auto rejectedLoad = transport.load(
        invalidZeroTick, profile, executable, content);
    const auto afterRejectedLoad = transport.snapshot();
    expect(!rejectedLoad.ok &&
        rejectedLoad.errorCode == "replay_final_hash_mismatch" &&
        afterRejectedLoad.loaded == beforeRejectedLoad.loaded &&
        afterRejectedLoad.paused == beforeRejectedLoad.paused &&
        afterRejectedLoad.playbackRate == beforeRejectedLoad.playbackRate &&
        afterRejectedLoad.cameraMode == beforeRejectedLoad.cameraMode &&
        afterRejectedLoad.tick == beforeRejectedLoad.tick &&
        afterRejectedLoad.cueTrajectory.size() ==
            beforeRejectedLoad.cueTrajectory.size() &&
        transport.player().finalStateSha256() == beforeRejectedHash,
        "failed tick-zero endpoint load preserves the complete active transport");
    expect(transport.stepTick().ok &&
        transport.snapshot().tick == beforeRejectedLoad.tick + 1,
        "original replay remains playable after a rejected replacement load");

    billiardgl::GameRuntime authority;
    billiardgl::GameAction power;
    power.type = billiardgl::ActionType::SetShotPower;
    power.first = 40.0f;
    expect(authority.dispatch(power).ok,
        "preview authority accepts a shot power");
    const std::string beforeHash =
        billiardgl::gameStateSha256(authority.state());
    const std::uint64_t beforeTick = authority.tick();
    const std::size_t beforeEvents = authority.events().size();
    billiardgl::training::PreviewBudget budget;
    budget.maximumTicks = 8;
    budget.maximumWallMilliseconds = 1000;
    const auto preview = billiardgl::training::previewTrainingAssist(
        authority, budget);
    expect(preview.ok &&
        beforeHash == billiardgl::gameStateSha256(authority.state()) &&
        beforeTick == authority.tick() &&
        beforeEvents == authority.events().size(),
        "trajectory preview isolates authority hash/tick/events");

    expect(transport.stepTick().ok,
        "actual replay trajectory advances after seek");
    billiardgl::training::TrajectoryOverlayModel overlay;
    overlay.setPredicted(preview);
    overlay.setActual(transport.snapshot());
    const auto compact = overlay.snapshot();
    expect(!compact.advancedExpanded && compact.legend.size() == 2,
        "advanced trajectory data is collapsed by default with a two-item legend");
    expect(compact.predicted.encoding.color != compact.actual.encoding.color &&
        compact.predicted.encoding.lineStyle != compact.actual.encoding.lineStyle &&
        compact.predicted.encoding.marker != compact.actual.encoding.marker &&
        compact.predicted.encoding.legendLabel !=
            compact.actual.encoding.legendLabel,
        "predicted/actual trajectories differ by color, line style, marker, and legend");
    expect(compact.predicted.endpointVisible &&
        compact.actual.endpointVisible &&
        compact.predicted.railCollisionEvents >= 0 &&
        compact.actual.railCollisionEvents >= 0,
        "overlay exposes cue endpoint and rail-event count for both tracks");
    overlay.setAdvancedExpanded(true);
    expect(overlay.snapshot().advancedExpanded,
        "advanced trajectory details expand only on explicit UI request");
    expect(beforeHash == billiardgl::gameStateSha256(authority.state()) &&
        beforeTick == authority.tick() &&
        beforeEvents == authority.events().size(),
        "replay/overlay work never mutates training authority");
}

}  // namespace

int auditReferenceFromCommandLine(const char* scenarioPath,
    const char* replayPath)
{
    const auto scenario = billiardgl::training::loadScenarioFile(
        scenarioPath, allCapabilities());
    if (!scenario.ok) {
        std::cerr << scenario.errorCode << '\n';
        return 2;
    }
    std::ifstream input(replayPath, std::ios::binary);
    const std::string bytes((std::istreambuf_iterator<char>(input)),
        std::istreambuf_iterator<char>());
    const auto decoded = billiardgl::json::parse(bytes);
    billiardgl::replay::Document document;
    const auto parsed = decoded.ok ? billiardgl::replay::parse(
        decoded.value, document) :
        billiardgl::replay::ValidationResult{false, "replay_format_invalid"};
    if (!parsed.ok) {
        std::cerr << parsed.errorCode << '\n';
        return 2;
    }
    const auto audit = billiardgl::training::auditReferenceReplay(
        document, scenario.scenario, scenario.scenario.initialScenario.physicsProfile,
        document.executableVersion, document.contentSha256);
    billiardgl::json::Value result = billiardgl::json::Value::object();
    result["ok"] = billiardgl::json::Value(audit.result.ok);
    result["error_code"] = billiardgl::json::Value(audit.result.errorCode);
    result["action_log_sha256"] = billiardgl::json::Value(
        audit.actionLogSha256);
    result["trace_sha256"] = billiardgl::json::Value(audit.traceSha256);
    result["score"] = billiardgl::json::Value(audit.evaluation.score);
    std::cout << billiardgl::json::stringify(result) << '\n';
    return audit.result.ok ? 0 : 2;
}

int main(int argc, char** argv)
{
    if (argc == 4 && std::string(argv[1]) == "--audit-reference")
        return auditReferenceFromCommandLine(argv[2], argv[3]);
    try {
        testTrainingProductController();
        testActualMultiShotTerminalLifecycle();
        testBeginnerV2TeachingDefaultsAndDynamicStopZone();
        testCompleteV2TeachingCatalog();
        testFixedShippingFollowAndDrawReferences();
        testAllBoundReferenceReplays();
        testReplayTransportAndOverlayIsolation();
        testReplayImpactCameraBoundaries();
        std::cout << "Phase4 product controller, replay transport, and trajectory overlay checks passed\n";
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
