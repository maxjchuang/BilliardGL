#include "automation_controller.h"
#include "deterministic_hash.h"
#include "replay/replay_v1.h"
#include "shot.h"
#include "training/ball_edit_transaction.h"
#include "training/preview_runtime.h"
#include "training/training_scenario.h"
#include "training/training_session.h"

#include <cmath>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <iterator>
#include <limits>
#include <set>
#include <string>
#include <vector>

namespace {

int failures = 0;

void expect(bool condition, const std::string& message)
{
    if (condition) return;
    ++failures;
    std::cerr << "FAIL: " << message << '\n';
}

billiardgl::ControllerResult send(billiardgl::AutomationController& controller,
    int id, const std::string& command,
    const billiardgl::json::Value& params = billiardgl::json::Value::object())
{
    billiardgl::AutomationRequest request;
    request.id = id;
    request.version = 1;
    request.command = command;
    request.params = params;
    return controller.handle(request);
}

billiardgl::json::Value object(
    std::initializer_list<std::pair<const char*, billiardgl::json::Value>> fields)
{
    billiardgl::json::Value value = billiardgl::json::Value::object();
    for (const auto& field : fields) value[field.first] = field.second;
    return value;
}

std::set<std::string> allCapabilities()
{
    return {"training_scenario_v1", "authoritative_horizontal_tip_offset_v1",
        "authoritative_longitudinal_tip_offset_v1"};
}

std::vector<std::filesystem::path> scenarioPaths()
{
    const std::filesystem::path directory = std::filesystem::path(
        BILLIARDGL_SOURCE_ROOT) / "assets/training/phase4/scenarios";
    std::vector<std::filesystem::path> paths;
    for (const auto& entry : std::filesystem::directory_iterator(directory)) {
        if (entry.path().extension() == ".json") paths.push_back(entry.path());
    }
    std::sort(paths.begin(), paths.end());
    return paths;
}

billiardgl::training::AttemptFacts successfulFacts(
    const billiardgl::training::Scenario& scenario)
{
    billiardgl::training::AttemptFacts facts;
    billiardgl::GameRuntime runtime;
    expect(billiardgl::applyPhysicsScenario(runtime, scenario.initialScenario).ok,
        scenario.id + " initial scenario applies");
    facts.state = runtime.state();
    facts.shotCount = scenario.scoring.at("par_shots").asInt();
    facts.traceSha256 = std::string(64, 'a');
    for (const billiardgl::json::Value& condition : scenario.success.asArray()) {
        const std::string name = condition.at("predicate").asString();
        if (name == "ball_pocketed")
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
        } else if (name == "rail_contact_count_gte") {
            facts.railContacts[condition.at("ball_index").asInt()] =
                condition.at("minimum").asInt();
        } else if (name == "distinct_object_balls_with_rail_contact_gte") {
            facts.distinctObjectBallsWithRailContact = condition.at("minimum").asInt();
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

void testAllLaunchScenarios()
{
    const std::vector<std::filesystem::path> paths = scenarioPaths();
    expect(paths.size() == 12, "launch training pack contains exactly 12 scenarios");
    for (const std::filesystem::path& path : paths) {
        const auto loaded = billiardgl::training::loadScenarioFile(
            path.string(), allCapabilities());
        expect(loaded.ok, path.filename().string() + " loads: " +
            loaded.errorCode + " " + loaded.errorMessage);
        if (!loaded.ok) continue;
        billiardgl::training::AttemptFacts success = successfulFacts(loaded.scenario);
        const auto passed = billiardgl::training::evaluate(loaded.scenario, success);
        expect(passed.succeeded && !passed.failed && passed.scoreEligible,
            loaded.scenario.id + " has an automated success case");
        billiardgl::training::AttemptFacts failure = success;
        failure.numericalFailure = true;
        const auto failed = billiardgl::training::evaluate(loaded.scenario, failure);
        expect(failed.failed && !failed.succeeded,
            loaded.scenario.id + " has an automated failure case");
    }
}

void testCapabilityGateAndSession()
{
    const auto paths = scenarioPaths();
    const auto gated = billiardgl::training::loadScenarioFile(
        (std::filesystem::path(BILLIARDGL_SOURCE_ROOT) /
            "assets/training/phase4/scenarios/launch_follow_shot_v1.training.json").string(),
        {"training_scenario_v1"});
    expect(gated.ok && !gated.scenario.scoreEnabled &&
        gated.missingCapabilities.size() == 1,
        "missing promoted spin capability loads visible but locked for scoring");

    {
        std::ifstream input(paths.front(), std::ios::binary);
        const std::string bytes((std::istreambuf_iterator<char>(input)),
            std::istreambuf_iterator<char>());
        billiardgl::json::Value tampered = billiardgl::json::parse(bytes).value;
        tampered["initial_scenario"]["scenario"]["balls"].asArray()[0]
            ["position_cm"].asArray()[0] = billiardgl::json::Value(12.25);
        const auto rejected = billiardgl::training::parseScenario(
            tampered, allCapabilities());
        expect(!rejected.ok && rejected.errorCode == "E_TRAINING_HASH_MISMATCH",
            "training loader rejects modified initial content before adapter normalization");
    }

    billiardgl::GameRuntime runtime;
    billiardgl::training::Session session(runtime);
    const auto loaded = session.load(paths.front().string(), allCapabilities());
    expect(loaded.ok && session.active(), "training session loads initial scenario");
    const std::string initialHash = billiardgl::gameStateSha256(runtime.state());

    billiardgl::training::BallEditTransaction overlap(
        runtime.state(), runtime.physicsProfile());
    expect(overlap.setBall(1, runtime.state().balls[0].position).ok,
        "overlap edit can be staged");
    billiardgl::GameState untouched = runtime.state();
    expect(!session.commit(overlap).ok &&
        billiardgl::gameStateSha256(runtime.state()) ==
            billiardgl::gameStateSha256(untouched),
        "overlap rejection leaves authoritative state unchanged");

    billiardgl::training::BallEditTransaction boundary(
        runtime.state(), runtime.physicsProfile());
    boundary.setBall(0, billiardgl::Point3{1000.0f,
        runtime.state().balls[0].position.y, 0.0f});
    expect(boundary.validate().errorCode == "edit_position_outside_table",
        "table boundary is validated");
    billiardgl::training::BallEditTransaction pocket(
        runtime.state(), runtime.physicsProfile());
    pocket.setBall(0, billiardgl::Point3{
        runtime.physicsProfile().tableBoundary.playfieldWidthCm * 0.5f,
        runtime.state().balls[0].position.y, 0.0f});
    expect(pocket.validate().errorCode == "edit_position_inside_pocket",
        "pocket interior is validated");
    billiardgl::training::BallEditTransaction cueMissing(
        runtime.state(), runtime.physicsProfile());
    cueMissing.setBall(0, runtime.state().balls[0].position, true);
    expect(cueMissing.validate().errorCode == "edit_cue_ball_required",
        "cue ball presence is validated");

    billiardgl::training::BallEditTransaction valid(
        runtime.state(), runtime.physicsProfile());
    billiardgl::Point3 moved = runtime.state().balls[0].position;
    moved.x += 7.0f;
    expect(valid.setBall(0, moved).ok && session.commit(valid).ok,
        "valid free-ball transaction commits atomically");
    expect(billiardgl::gameStateSha256(runtime.state()) != initialHash,
        "valid edit changes session state");
    expect(session.undo() && billiardgl::gameStateSha256(runtime.state()) == initialHash,
        "undo restores previous edit state");
    session.markShotStart();
    billiardgl::BallState ball = runtime.state().balls[0];
    ball.position.x += 5.0f;
    runtime.setBall(0, ball);
    expect(session.retry() && billiardgl::gameStateSha256(runtime.state()) == initialHash,
        "retry restores pre-shot state");
    expect(session.reset() && billiardgl::gameStateSha256(runtime.state()) == initialHash,
        "reset atomically reloads embedded initial state");

    const auto savePath = std::filesystem::temp_directory_path() /
        "billiardgl_phase4_saved_training.json";
    std::string error;
    expect(session.save(savePath.string(), error) &&
        std::filesystem::file_size(savePath) > 0,
        "session saves a versioned local training scenario");
    std::filesystem::remove(savePath);
}

void testRailContactUpperBoundPredicate()
{
    using namespace billiardgl;
    using namespace billiardgl::training;

    Scenario scenario;
    scenario.scoreEnabled = true;
    scenario.success = json::Value::array();
    scenario.success.asArray().push_back(object({
        {"predicate", json::Value("ball_pocketed")},
        {"ball_index", json::Value(1)}}));
    scenario.success.asArray().push_back(object({
        {"predicate", json::Value("ball_final_position_in_zone")},
        {"ball_index", json::Value(0)},
        {"center_cm", json::Value::array()},
        {"radius_cm", json::Value(3.0)}}));
    scenario.success.asArray().back()["center_cm"].asArray().push_back(
        json::Value(24.0));
    scenario.success.asArray().back()["center_cm"].asArray().push_back(
        json::Value(-12.0));
    scenario.success.asArray().push_back(object({
        {"predicate", json::Value("rail_contact_count_lte")},
        {"ball_index", json::Value(0)},
        {"maximum", json::Value(0)}}));
    scenario.failure = json::Value::array();
    scenario.scoring = object({
        {"deterministic", json::Value(true)},
        {"minimum", json::Value(0)},
        {"maximum", json::Value(100)},
        {"base_points", json::Value(100.0)},
        {"par_shots", json::Value(1)},
        {"deductions", json::Value::array()}});

    AttemptFacts noRail;
    noRail.state.balls[1].pocketed = true;
    noRail.state.balls[0].position = {24.0f, kBallRadius, -12.0f};
    noRail.shotCount = 1;
    const Evaluation accepted = evaluate(scenario, noRail);
    expect(accepted.succeeded && !accepted.failed &&
        accepted.reason == "success",
        "identical pocket and landing facts pass when the cue ball has zero rail contacts");

    AttemptFacts touchedRail = noRail;
    touchedRail.railContacts[0] = 1;
    const Evaluation rejected = evaluate(scenario, touchedRail);
    expect(!rejected.succeeded && !rejected.failed &&
        rejected.reason == "success_conditions_not_met" &&
        rejected.feedback == "Ball 0 rail contacts: 1 / max 0",
        "one cue-ball rail contact rejects the otherwise identical result with an explicit measured upper-bound message");

    Scenario invalid = scenario;
    invalid.success.asArray().back()["maximum"] = json::Value(-1);
    const Evaluation invalidResult = evaluate(invalid, noRail);
    expect(!invalidResult.succeeded && !invalidResult.failed &&
        invalidResult.feedback == "Invalid rail contact limit",
        "negative rail-contact maxima fail closed with explicit feedback");

    scenario.failure.asArray().push_back(object({
        {"predicate", json::Value("cue_ball_pocketed_during_attempt")}}));
    expect(evaluate(scenario, noRail).succeeded,
        "scratch-history guard preserves a clean successful shot");
    AttemptFacts respottedScratch = noRail;
    respottedScratch.pocketOrder.push_back(0);
    const Evaluation scratched = evaluate(scenario, respottedScratch);
    expect(scratched.failed && !scratched.succeeded &&
        scratched.reason == "cue_ball_pocketed_during_attempt",
        "cue-ball capture history fails even after the cue ball is respotted");

    scenario.scoring["optional_bonus_zone"] = scenario.success.asArray()[1];
    scenario.success.asArray().erase(scenario.success.asArray().begin() + 1);
    scenario.scoring["deductions"].asArray().push_back(object({
        {"metric", json::Value("cue_ball_bonus_zone_missed")},
        {"quantization", json::Value(1.0)}, {"points_per_unit", json::Value(20.0)},
        {"cap_points", json::Value(20.0)}}));
    expect(evaluate(scenario, noRail).score == 100,
        "optional ring awards the full 100 points");
    AttemptFacts outsideBonus = noRail;
    outsideBonus.state.balls[0].position.x += 20.0f;
    const auto outsideResult = evaluate(scenario, outsideBonus);
    expect(outsideResult.succeeded && !outsideResult.failed && outsideResult.score == 80,
        "outside optional ring still passes with base 80 points");
    expect(evaluate(scenario, respottedScratch).failed,
        "optional bonus never bypasses scratch failure");
}

void testExactPocketIdentityPredicate()
{
    using namespace billiardgl;
    using namespace billiardgl::training;

    Scenario scenario;
    scenario.scoreEnabled = true;
    scenario.success = json::Value::array();
    scenario.success.asArray().push_back(object({
        {"predicate", json::Value("ball_pocketed_in_pocket")},
        {"ball_index", json::Value(4)},
        {"pocket_id", json::Value(0)}}));
    scenario.failure = json::Value::array();
    scenario.scoring = object({
        {"deterministic", json::Value(true)},
        {"minimum", json::Value(0)},
        {"maximum", json::Value(100)},
        {"base_points", json::Value(100.0)},
        {"par_shots", json::Value(1)},
        {"deductions", json::Value::array()}});

    AttemptFacts exact;
    exact.state.balls[4].pocketed = true;
    exact.pocketIds[4] = 0;
    const Evaluation accepted = evaluate(scenario, exact);
    expect(accepted.succeeded && accepted.reason == "success",
        "captured object ball passes only for the requested canonical pocket id");

    AttemptFacts wrongPocket = exact;
    wrongPocket.pocketIds[4] = 4;
    const Evaluation rejected = evaluate(scenario, wrongPocket);
    expect(!rejected.succeeded && !rejected.failed &&
        rejected.feedback == "Ball 4 pocket: 4 / target 0",
        "the same pocketed ball in another pocket is rejected with actual and target ids");

    AttemptFacts finalStateDerived;
    finalStateDerived.state.balls[4].pocketed = true;
    finalStateDerived.state.balls[4].pocketInteraction.phase =
        PocketInteractionPhase::Captured;
    finalStateDerived.state.balls[4].pocketInteraction.pocketId = 0;
    expect(evaluate(scenario, finalStateDerived).succeeded,
        "automation scoring can derive a canonical capture id from authoritative final state without caller-supplied pocket facts");

    Scenario invalid = scenario;
    invalid.success.asArray().front()["pocket_id"] = json::Value(6);
    const Evaluation invalidResult = evaluate(invalid, exact);
    expect(!invalidResult.succeeded &&
        invalidResult.feedback == "Invalid target pocket",
        "out-of-range target pocket ids fail closed");
}

void testPreviewIsolation()
{
    billiardgl::GameRuntime runtime;
    billiardgl::training::Session session(runtime);
    const auto path = std::filesystem::path(BILLIARDGL_SOURCE_ROOT) /
        "assets/training/phase4/scenarios/launch_stop_shot_center_v1.training.json";
    expect(session.load(path.string(), allCapabilities()).ok,
        "preview fixture training scenario loads");
    const std::string before = billiardgl::gameStateSha256(runtime.state());
    const std::uint64_t tickBefore = runtime.tick();
    const std::size_t eventsBefore = runtime.events().size();
    billiardgl::training::PreviewBudget budget;
    budget.maximumTicks = 12;
    budget.maximumWallMilliseconds = 1000;
    budget.maximumBalls = 2;
    const auto preview = billiardgl::training::previewTrainingAssist(runtime, budget);
    expect(preview.ok && preview.simulatedTicks <= budget.maximumTicks &&
        !preview.eventsEmitted, "training assist uses bounded event-free preview runtime");
    expect(before == billiardgl::gameStateSha256(runtime.state()) &&
        tickBefore == runtime.tick() && eventsBefore == runtime.events().size(),
        "preview leaves authoritative state hash, tick, and events unchanged");
    budget.maximumBalls = 1;
    expect(billiardgl::training::previewTrainingAssist(runtime, budget).errorCode ==
        "preview_ball_budget_exceeded", "preview enforces ball budget");
}

billiardgl::replay::Document replayFixture()
{
    billiardgl::GameRuntime runtime;
    const std::string executable = "billiardgl-test-v1";
    const std::string content = billiardgl::sha256Hex("phase4-content-v1");
    billiardgl::replay::Recorder recorder;
    expect(recorder.begin(runtime, executable, content, 17).ok,
        "replay fixture recorder begins");
    billiardgl::GameAction action;
    action.type = billiardgl::ActionType::SetAimYaw;
    action.first = 1.5707963f;
    expect(runtime.dispatch(action).ok, "replay fixture aim applies");
    action.type = billiardgl::ActionType::SetShotPower;
    action.first = 10.0f;
    expect(runtime.dispatch(action).ok, "replay fixture first power applies");
    action.first = 45.0f;
    expect(runtime.dispatch(action).ok, "replay fixture second power applies");
    action.type = billiardgl::ActionType::Shoot;
    expect(runtime.dispatch(action).ok && runtime.step(80).ok &&
        recorder.finish(runtime, 1).ok,
        "replay fixture records and seals an authoritative final hash");
    return recorder.document();
}

void runToEnd(billiardgl::replay::Player& player)
{
    while (!player.finished()) expect(player.stepTick().ok, "replay step succeeds");
}

void testReplayV1()
{
    billiardgl::replay::Document document = replayFixture();
    billiardgl::GameRuntime referenceRuntime;
    billiardgl::replay::Player reference;
    expect(reference.load(document, referenceRuntime.physicsProfile(),
        document.executableVersion, document.contentSha256).ok,
        "replay loads with matching executable/content/profile binding");
    runToEnd(reference);
    expect(std::fabs(reference.runtime().cueImpactInput().cueSpeedCmS -
        billiardgl::cueImpactFromShotControls(1.5707963f, 45.0f,
            referenceRuntime.physicsProfile()).cueSpeedCmS) < 1e-9,
        "same-tick replay actions preserve document order before shoot");
    expect(document.finalSummary.stateSha256 == reference.finalStateSha256() &&
        document.finalSummary.authorityStateSchema ==
            billiardgl::replay::kReplayAuthorityStateSchema &&
        document.finalSummary.authorityStateSha256 ==
            billiardgl::replay::replayAuthorityStateSha256(
                reference.runtime().state()),
        "new Replay v1 recordings bind legacy and D6 authority final hashes");

    billiardgl::replay::Player first;
    billiardgl::replay::Player second;
    expect(first.load(document, referenceRuntime.physicsProfile(),
        document.executableVersion, document.contentSha256).ok &&
        second.load(document, referenceRuntime.physicsProfile(),
        document.executableVersion, document.contentSha256).ok,
        "bound replay loads twice");
    runToEnd(first);
    runToEnd(second);
    expect(first.finalHashMatches() && second.finalHashMatches() &&
        first.finalStateSha256() == second.finalStateSha256(),
        "repeated replay execution yields identical verified final hash");
    auto legacy = document;
    legacy.finalSummary.authorityStateSchema.clear();
    legacy.finalSummary.authorityStateSha256.clear();
    billiardgl::replay::Player legacyPlayer;
    expect(legacyPlayer.load(legacy, referenceRuntime.physicsProfile(),
        document.executableVersion, document.contentSha256).ok,
        "legacy Replay v1 with a valid base final hash remains loadable");
    runToEnd(legacyPlayer);
    expect(legacyPlayer.finalHashMatches(),
        "legacy Replay v1 retains its explicitly narrower base-hash verification");

    billiardgl::replay::Document parsed;
    expect(billiardgl::replay::parse(
        billiardgl::replay::serialize(document), parsed).ok &&
        parsed.actions.size() == document.actions.size(),
        "replay v1 format round-trips initial scenario and semantic actions");
    auto mismatch = document;
    mismatch.physicsProfileSha256 = std::string(64, '0');
    expect(!first.load(mismatch, referenceRuntime.physicsProfile(),
        document.executableVersion, document.contentSha256).ok,
        "profile hash mismatch is explicitly rejected");
    mismatch = document;
    mismatch.contentSha256 = std::string(64, '0');
    expect(!first.load(mismatch, referenceRuntime.physicsProfile(),
        document.executableVersion, document.contentSha256).ok,
        "content hash mismatch is explicitly rejected");
    mismatch = document;
    mismatch.finalSummary.stateSha256.clear();
    const auto emptyFinalHash = first.load(mismatch,
        referenceRuntime.physicsProfile(), document.executableVersion,
        document.contentSha256);
    expect(!emptyFinalHash.ok &&
        emptyFinalHash.errorCode == "replay_final_hash_invalid",
        "empty Replay final hash is rejected before playback");
    mismatch = document;
    mismatch.finalSummary.authorityStateSha256.clear();
    const auto halfAuthority = first.load(mismatch,
        referenceRuntime.physicsProfile(), document.executableVersion,
        document.contentSha256);
    expect(!halfAuthority.ok &&
        halfAuthority.errorCode == "replay_authority_hash_invalid",
        "half-present D6 authority binding is rejected");
    mismatch = document;
    mismatch.finalSummary.authorityStateSchema = "unknown_authority_v9";
    const auto unknownAuthority = first.load(mismatch,
        referenceRuntime.physicsProfile(), document.executableVersion,
        document.contentSha256);
    expect(!unknownAuthority.ok &&
        unknownAuthority.errorCode == "replay_authority_schema_unknown",
        "unknown Replay authority schema is rejected");
    mismatch = document;
    mismatch.finalSummary.authorityStateSha256 = std::string(64, '0');
    billiardgl::replay::Player forgedAuthority;
    expect(forgedAuthority.load(mismatch, referenceRuntime.physicsProfile(),
        document.executableVersion, document.contentSha256).ok,
        "well-formed forged final authority hash reaches execution verification");
    billiardgl::replay::ValidationResult terminal;
    while (!forgedAuthority.finished()) terminal = forgedAuthority.stepTick();
    expect(!terminal.ok &&
        terminal.errorCode == "replay_final_hash_mismatch" &&
        !forgedAuthority.finalHashMatches(),
        "forged D6 authority digest fails explicitly at the replay endpoint");
    expect(forgedAuthority.stepTick().errorCode ==
            "replay_final_hash_mismatch" &&
        forgedAuthority.advanceScheduledFrame().errorCode ==
            "replay_final_hash_mismatch",
        "endpoint verification error remains stable on repeated playback calls");

    billiardgl::json::Value malformed = billiardgl::replay::serialize(document);
    malformed["semantic_actions"].asArray()[0]["tick"] =
        billiardgl::json::Value(-1);
    expect(!billiardgl::replay::parse(malformed, parsed).ok,
        "negative replay action tick is rejected before unsigned conversion");
    malformed = billiardgl::replay::serialize(document);
    malformed["seed"] = billiardgl::json::Value(1.5);
    expect(!billiardgl::replay::parse(malformed, parsed).ok,
        "fractional replay seed is rejected before unsigned conversion");
    malformed = billiardgl::replay::serialize(document);
    malformed["final_summary"]["shot_count"] = billiardgl::json::Value(-1);
    expect(!billiardgl::replay::parse(malformed, parsed).ok,
        "negative final shot count is rejected");
    malformed = billiardgl::replay::serialize(document);
    malformed["semantic_actions"].asArray()[0]["first"] =
        billiardgl::json::Value(std::numeric_limits<double>::infinity());
    expect(!billiardgl::replay::parse(malformed, parsed).ok,
        "nonfinite replay action value is rejected");
    mismatch = document;
    mismatch.actions.back().tick = mismatch.finalSummary.tick;
    expect(!billiardgl::replay::validate(mismatch,
        referenceRuntime.physicsProfile(), document.executableVersion,
        document.contentSha256).ok,
        "action at final tick is rejected because it cannot execute");

    billiardgl::replay::Player controls;
    expect(controls.load(document, referenceRuntime.physicsProfile(),
        document.executableVersion, document.contentSha256).ok,
        "replay control fixture loads");
    controls.setPaused(false);
    expect(controls.setPlaybackRate(0.25).ok, "quarter-speed is supported");
    controls.advanceScheduledFrame();
    controls.advanceScheduledFrame();
    controls.advanceScheduledFrame();
    expect(controls.runtime().tick() == 0, "quarter-speed changes scheduling only");
    controls.advanceScheduledFrame();
    expect(controls.runtime().tick() == 1, "quarter-speed schedules one fixed tick per four frames");
    expect(controls.setPlaybackRate(0.5).ok && controls.setPlaybackRate(1.0).ok &&
        controls.setPlaybackRate(2.0).ok && !controls.setPlaybackRate(0.75).ok,
        "only 1/4x, 1/2x, 1x, and 2x replay rates are accepted");
    expect(controls.seekShot(0).ok && controls.runtime().tick() == 0,
        "seek-to-shot restores deterministic pre-shot state");
}

void testAutomationIntegration()
{
    billiardgl::GameRuntime runtime;
    billiardgl::AutomationController controller(
        runtime, billiardgl::AutomationMode::Headless);
    const auto capabilities = controller.capabilities();
    for (const char* required : {"training_load", "training_preview",
            "training_edit_commit", "replay_load", "replay_step_tick"}) {
        expect(std::find(capabilities.begin(), capabilities.end(), required) !=
            capabilities.end(), std::string("Phase4 automation advertises ") + required);
    }
    const std::string scenarioPath = (std::filesystem::path(BILLIARDGL_SOURCE_ROOT) /
        "assets/training/phase4/scenarios/launch_stop_shot_center_v1.training.json").string();
    const auto loaded = send(controller, 100, "training_load", object({
        {"schema_version", billiardgl::json::Value(1)},
        {"path", billiardgl::json::Value(scenarioPath)}}));
    expect(loaded.response.at("ok").asBool() &&
        loaded.response.at("result").at("active").asBool(),
        "automation loads a versioned training scenario");
    const std::string authorityBefore = billiardgl::gameStateSha256(runtime.state());
    const auto preview = send(controller, 101, "training_preview", object({
        {"schema_version", billiardgl::json::Value(1)},
        {"maximum_ticks", billiardgl::json::Value(4)},
        {"maximum_wall_milliseconds", billiardgl::json::Value(1000)},
        {"maximum_balls", billiardgl::json::Value(2)}}));
    expect(preview.response.at("ok").asBool() &&
        !preview.response.at("result").at("events_emitted").asBool() &&
        authorityBefore == billiardgl::gameStateSha256(runtime.state()),
        "automation preview is isolated from authority");
    expect(send(controller, 102, "training_edit_begin").response.at("ok").asBool(),
        "automation begins isolated edit transaction");
    billiardgl::json::Value position = billiardgl::json::Value::object();
    position["x"] = billiardgl::json::Value(runtime.state().balls[0].position.x);
    position["y"] = billiardgl::json::Value(runtime.state().balls[0].position.y);
    position["z"] = billiardgl::json::Value(runtime.state().balls[0].position.z);
    expect(send(controller, 103, "training_edit_set_ball", object({
        {"index", billiardgl::json::Value(1)}, {"position", position}}))
        .response.at("ok").asBool(), "automation stages ball edit");
    const auto rejected = send(controller, 104, "training_edit_commit");
    expect(!rejected.response.at("ok").asBool() &&
        rejected.response.at("error").at("code").asString() == "edit_ball_overlap" &&
        authorityBefore == billiardgl::gameStateSha256(runtime.state()),
        "automation edit validation rejects overlap atomically");
    send(controller, 105, "training_edit_cancel");

    const auto recordingBegun = send(controller, 106, "replay_record_begin", object({
        {"schema_version", billiardgl::json::Value(1)},
        {"executable_version", billiardgl::json::Value("phase4-automation-v1")},
        {"seed", billiardgl::json::Value(41)}}));
    expect(recordingBegun.response.at("ok").asBool() &&
        recordingBegun.response.at("result").at("active").asBool(),
        "training automation begins Replay v1 recording with scenario content binding");
    expect(!send(controller, 107, "training_reset").response.at("ok").asBool(),
        "unrepresentable authority edits are rejected while recording");
    expect(!send(controller, 108, "set_shot_power", object({
        {"power", billiardgl::json::Value(201)}})).response.at("ok").asBool(),
        "failed semantic action is rejected while recording");
    expect(send(controller, 109, "set_aim_yaw", object({
        {"yaw", billiardgl::json::Value(1.5707963)}})).response.at("ok").asBool() &&
        send(controller, 110, "set_shot_power", object({
        {"power", billiardgl::json::Value(60)}})).response.at("ok").asBool() &&
        send(controller, 111, "shoot").response.at("ok").asBool(),
        "successful training semantic actions enter the recorder");
    expect(!send(controller, 112, "shoot").response.at("ok").asBool(),
        "failed Shoot is rejected after the first shot starts moving");
    expect(send(controller, 113, "step", object({
        {"ticks", billiardgl::json::Value(2)}})).response.at("ok").asBool(),
        "recorded training authority advances beyond action tick");
    const auto recordingState = send(
        controller, 114, "replay_record_get_state").response.at("result");
    expect(recordingState.at("action_count").asInt() == 3 &&
        recordingState.at("shot_count").asInt() == 1,
        "recorder contains only three successfully applied semantic actions");
    expect(send(controller, 115, "replay_record_finish", object({
        {"schema_version", billiardgl::json::Value(1)}})).response.at("ok").asBool(),
        "automation finishes a validated Replay v1 recording");
    const std::filesystem::path recordedPath =
        std::filesystem::temp_directory_path() /
        "billiardgl_phase4_automation_recording.json";
    std::error_code removeError;
    std::filesystem::remove(recordedPath, removeError);
    expect(send(controller, 116, "replay_record_save", object({
        {"schema_version", billiardgl::json::Value(1)},
        {"path", billiardgl::json::Value(recordedPath.string())}}))
        .response.at("ok").asBool() && std::filesystem::exists(recordedPath),
        "automation atomically saves the finished replay beside its destination");
    std::ifstream recordedInput(recordedPath, std::ios::binary);
    const std::string recordedBytes((std::istreambuf_iterator<char>(recordedInput)),
        std::istreambuf_iterator<char>());
    const auto recordedJson = billiardgl::json::parse(recordedBytes);
    billiardgl::replay::Document document;
    const auto recordedParsed = recordedJson.ok ? billiardgl::replay::parse(
        recordedJson.value, document) :
        billiardgl::replay::ValidationResult{false, "replay_format_invalid"};
    expect(recordedParsed.ok,
        "saved shipping-profile training replay parses for playback");
    expect(send(controller, 117, "training_retry").response.at("ok").asBool() &&
        billiardgl::gameStateSha256(runtime.state()) == authorityBefore,
        "failed Shoot neither changes training retry history nor replaces successful pre-shot state");

    const auto replayLoaded = send(controller, 118, "replay_load", object({
        {"schema_version", billiardgl::json::Value(1)},
        {"replay", billiardgl::replay::serialize(document)},
        {"executable_version", billiardgl::json::Value(document.executableVersion)},
        {"content_sha256", billiardgl::json::Value(document.contentSha256)}}));
    // Playback uses the replay recorded from this exact shipping training
    // profile/content binding rather than a legacy default-profile fixture.
    expect(replayLoaded.response.at("ok").asBool(),
        "automation loads Replay v1 with explicit bindings");
    expect(send(controller, 119, "replay_set_speed", object({
        {"rate", billiardgl::json::Value(0.5)}})).response.at("ok").asBool() &&
        send(controller, 120, "replay_step_tick").response.at("ok").asBool(),
        "automation exposes replay speed and fixed-tick stepping");
    std::filesystem::remove(recordedPath, removeError);
}

}  // namespace

int main()
{
    expect(billiardgl::sha256Hex("abc") ==
        "ba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015ad",
        "production SHA-256 implementation matches standard vector");
    testAllLaunchScenarios();
    testCapabilityGateAndSession();
    testRailContactUpperBoundPredicate();
    testExactPocketIdentityPredicate();
    testPreviewIsolation();
    testReplayV1();
    testAutomationIntegration();
    if (failures != 0) {
        std::cerr << failures << " Phase4 system checks failed\n";
        return 1;
    }
    std::cout << "Phase4 training, edit, preview, and replay checks passed\n";
    return 0;
}
