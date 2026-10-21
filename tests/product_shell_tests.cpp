#include "deterministic_hash.h"
#include "product_shell_controller.h"
#include "render/ui/product_shell_draw_list.h"
#include "render/ui/hud_shipping_tokens.h"
#include "replay/replay_recorder.h"
#include "render_state.h"
#include "product_hud.h"
#include "render/modern/camera_projection.h"

#include <algorithm>
#include <cmath>
#include <cstring>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <iterator>
#include <map>
#include <set>
#include <stdexcept>
#include <string>
#include <vector>

namespace {

void expect(bool value, const char* message)
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

class FakeAiPlayer final : public billiardgl::ai::PlayerControl {
public:
    billiardgl::ActionResult configure(billiardgl::GameRuntime& authority,
        const billiardgl::ai::PlayerConfiguration& configuration) override
    {
        lastConfiguration = configuration;
        rulesModeAtConfigure = authority.rulesMode();
        tickAtConfigure = authority.tick();
        authorityRevisionAtConfigure = authority.rulesAuthorityRevision();
        authorityActionCountAtConfigure =
            authority.rulesAuthorityActions().size();
        ++configureCalls;
        if (failNextConfiguration) {
            failNextConfiguration = false;
            configured = false;
            authority.mutableState().balls[0].position.x = 999.0f;
            return {false, "fake_ai_configuration_failed"};
        }
        configured = configuration.enabled;
        state_.configuration = configuration;
        state_.status = configuration.enabled ?
            billiardgl::ai::PlayerPollStatus::WaitingForTurn :
            billiardgl::ai::PlayerPollStatus::Disabled;
        return {};
    }

    billiardgl::ai::PlayerState state() const override
    {
        auto value = state_;
        value.thinking = forceThinking;
        return value;
    }
    billiardgl::ai::PlayerState poll(billiardgl::GameRuntime&) override
    {
        return state_;
    }

    bool configured = false;
    bool failNextConfiguration = false;
    bool forceThinking = false;
    int configureCalls = 0;
    std::uint64_t tickAtConfigure = 0;
    std::uint64_t authorityRevisionAtConfigure = 0;
    std::size_t authorityActionCountAtConfigure = 0;
    billiardgl::RulesMode rulesModeAtConfigure = billiardgl::RulesMode::Legacy;
    billiardgl::ai::PlayerConfiguration lastConfiguration;

private:
    billiardgl::ai::PlayerState state_;
};

struct Fixture {
    std::filesystem::path settingsPath;
    billiardgl::GameRuntime authority;
    billiardgl::settings::ProductSettingsRuntime settings;
    billiardgl::training::TrainingProductController training;
    billiardgl::replay::ReplayTransportModel replay;
    billiardgl::ProductInputController input;
    FakeAiPlayer ai;
    billiardgl::ProductShellController shell;

    explicit Fixture(const char* name)
        : settingsPath(std::filesystem::temp_directory_path() / name),
          settings(settingsPath.string()), training(authority),
          input(authority, settings, &training, &replay),
          shell(authority, settings, input, training, replay, &ai)
    {
        std::error_code ignored;
        std::filesystem::remove(settingsPath, ignored);
        std::filesystem::remove(settingsPath.string() + ".tmp", ignored);
        settings.start();
    }

    ~Fixture()
    {
        std::error_code ignored;
        std::filesystem::remove(settingsPath, ignored);
        std::filesystem::remove(settingsPath.string() + ".tmp", ignored);
    }
};

std::filesystem::path trainingManifest()
{
    return std::filesystem::path(BILLIARDGL_SOURCE_ROOT) /
        "assets/training/phase4/training_pack_v1.json";
}

std::filesystem::path trainingManifestV2()
{
    return std::filesystem::path(BILLIARDGL_SOURCE_ROOT) /
        "assets/training/phase4/training_pack_v2.json";
}

struct ReplayFile {
    ReplayFile() = default;
    ReplayFile(const ReplayFile&) = delete;
    ReplayFile& operator=(const ReplayFile&) = delete;
    ReplayFile(ReplayFile&&) = default;
    ReplayFile& operator=(ReplayFile&&) = default;
    std::filesystem::path path;
    std::string executable;
    std::string content;

    ~ReplayFile()
    {
        std::error_code ignored;
        std::filesystem::remove(path, ignored);
    }
};

ReplayFile makeReplayFile(const char* name,
    const billiardgl::PhysicsProfile* profile = nullptr)
{
    ReplayFile output;
    output.path = std::filesystem::temp_directory_path() / name;
    output.executable = "product-shell-replay-v1";
    output.content = billiardgl::sha256Hex("product-shell-replay-content");
    billiardgl::GameRuntime runtime;
    if (profile != nullptr) {
        expect(runtime.replaceStateForScenario(runtime.state(), *profile).ok,
            "replay fixture adopts its requested physics profile");
    }
    billiardgl::replay::Recorder recorder;
    expect(recorder.begin(runtime, output.executable, output.content, 77).ok,
        "replay recorder begins");
    billiardgl::GameAction power;
    power.type = billiardgl::ActionType::SetShotPower;
    power.first = 42.0f;
    billiardgl::GameAction shoot;
    shoot.type = billiardgl::ActionType::Shoot;
    expect(runtime.dispatch(power).ok && runtime.dispatch(shoot).ok &&
        runtime.step(12).ok && recorder.finish(runtime, 1).ok,
        "replay fixture records one shot");
    std::ofstream file(output.path, std::ios::binary | std::ios::trunc);
    file << billiardgl::json::stringify(
        billiardgl::replay::serialize(recorder.document())) << '\n';
    expect(static_cast<bool>(file), "replay fixture writes");
    return output;
}

const billiardgl::ShellItemViewModel* itemById(
    const billiardgl::ProductShellSnapshot& snapshot, const std::string& id)
{
    const auto found = std::find_if(snapshot.items.begin(), snapshot.items.end(),
        [&](const billiardgl::ShellItemViewModel& item) {
            return item.id == id;
        });
    return found == snapshot.items.end() ? nullptr : &*found;
}

std::string focusedItemId(
    const billiardgl::ProductShellSnapshot& snapshot)
{
    const auto found = std::find_if(snapshot.items.begin(), snapshot.items.end(),
        [](const billiardgl::ShellItemViewModel& item) {
            return item.focused;
        });
    return found == snapshot.items.end() ? "" : found->id;
}

bool hasInputContext(const Fixture& fixture, const std::string& context)
{
    const auto& active = fixture.settings.input().activeContexts();
    return std::find(active.begin(), active.end(), context) != active.end();
}

bool exactOfficialFixedRack(const billiardgl::GameRuntime& runtime)
{
    const auto contract = billiardgl::rules::buildOfficialRackContract(
        runtime.physicsProfile(),
        billiardgl::rules::kOfficialRackDefaultSeed);
    if (contract.numberedBallOrderIsOfficial ||
        contract.sameGroupAdjacencyCount !=
            contract.minimumPossibleSameGroupAdjacencyCount)
        return false;
    std::array<billiardgl::Point3, billiardgl::kBallCount> expected;
    expected[0] = contract.cueBallBreakPosition;
    for (const auto& slot : contract.slots) expected[slot.ball] = slot.position;
    for (int ball = 0; ball < billiardgl::kBallCount; ++ball) {
        const auto& state = runtime.state().balls[ball];
        if (std::memcmp(&state.position, &expected[ball],
                sizeof(state.position)) != 0 || state.pocketed ||
                state.speed != 0.0f)
            return false;
    }
    const int first = contract.slots[contract.baseCornerSlots[0]].ball;
    const int second = contract.slots[contract.baseCornerSlots[1]].ball;
    const auto solid = [](int ball) { return ball >= 1 && ball <= 7; };
    return contract.slots[contract.eightBallSlot].ball == 8 &&
        solid(first) != solid(second);
}

void testScreenFlowAndApprovedProductEntry()
{
    Fixture fixture("billiardgl_product_shell_flow_v1.json");
    const auto main = fixture.shell.snapshot();
    expect(main.screen == billiardgl::ProductScreen::MainMenu &&
        main.rulesetId == billiardgl::ProductShellController::kApprovedRulesetId &&
        main.rulesetLabel == "WPA Heyball" &&
        fixture.shell.approvedRules().size() == 1 &&
        fixture.shell.approvedRules().front().id == main.rulesetId,
        "main entry exposes exactly the approved WPA Heyball profile");
    for (const auto& entry : main.items) {
        expect(entry.label.find("Legacy") == std::string::npos &&
            entry.label.find("Unapproved") == std::string::npos,
            "unapproved rules are absent from main navigation");
    }

    expect(fixture.shell.navigate(
        billiardgl::ShellNavigationCommand::Activate).ok &&
        fixture.shell.screen() ==
            billiardgl::ProductScreen::SinglePlayerSetup,
        "keyboard activation reaches single-player setup");
    auto setup = fixture.shell.snapshot();
    expect(setup.items.size() == 5 &&
        itemById(setup, "rookie") != nullptr &&
        itemById(setup, "club") != nullptr &&
        itemById(setup, "expert") != nullptr &&
        itemById(setup,
            billiardgl::ProductShellController::kApprovedRulesetId) != nullptr,
        "single-player setup exposes one rule and three approved AI levels");
    expect(fixture.shell.selectDifficulty(
        billiardgl::ai::Difficulty::Expert).ok &&
        itemById(fixture.shell.snapshot(), "expert")->selected &&
        fixture.shell.startSinglePlayer(1234).ok && fixture.ai.configured &&
        fixture.ai.lastConfiguration.difficulty ==
            billiardgl::ai::Difficulty::Expert &&
        fixture.authority.rulesMode() ==
            billiardgl::RulesMode::ChineseEightBallV1 &&
        fixture.shell.screen() == billiardgl::ProductScreen::Match,
        "start request delegates AI/rules configuration and enters Match");
    const auto matchSnapshot=fixture.shell.snapshot();
    const auto* matchReplay=itemById(matchSnapshot,"save_match_replay");
    expect(matchReplay!=nullptr&&!matchReplay->enabled&&
        matchReplay->detail.find("cross-rack replay is unsupported")!=
            std::string::npos,
        "live Match explains the single-rack replay save boundary");

    expect(fixture.shell.openHelp().ok &&
        fixture.shell.screen() == billiardgl::ProductScreen::Help,
        "Help is reachable from the match");
    const auto help = fixture.shell.snapshot();
    const auto rulesHelp = std::find_if(help.items.begin(), help.items.end(),
        [](const billiardgl::ShellItemViewModel& item) {
            return item.id == "help_rules";
        });
    expect(rulesHelp != help.items.end() &&
        !rulesHelp->enabled && rulesHelp->readOnlyInformational &&
        rulesHelp->detail ==
            "Selected profile: WPA II 17(e) non-8 off-table foul; "
            "8-ball off-table loses",
        "product help states the selected non-8 and 8-ball off-table policy");
    const auto* replayHelp = itemById(help, "help_save_rack_replay");
    expect(replayHelp != nullptr && !replayHelp->enabled &&
        replayHelp->readOnlyInformational,
        "match Help replay shortcut remains readable reference text without becoming actionable");
    expect(fixture.shell.navigate(
               billiardgl::ShellNavigationCommand::Back).ok &&
        fixture.shell.screen() == billiardgl::ProductScreen::Match,
        "Help returns to the prior screen");
    expect(fixture.shell.openMainMenu().ok &&
        fixture.shell.openReplayBrowser().ok &&
        fixture.shell.screen() == billiardgl::ProductScreen::ReplayBrowser,
        "ReplayBrowser is reachable from the product shell");
    expect(fixture.shell.navigate(
        billiardgl::ShellNavigationCommand::Activate).appRequest ==
            billiardgl::ProductShellAppRequest::ChooseReplayFile,
        "ReplayBrowser activation emits an explicit native file-picker request");
}

void testFreshMatchRestartAndFailureCleanup()
{
    Fixture fixture("billiardgl_product_shell_fresh_match_v1.json");
    billiardgl::GameRuntime pristine;
    expect(pristine.setRulesMode(
        billiardgl::RulesMode::ChineseEightBallV1).ok,
        "pristine comparison authority enables approved rules");
    const std::string pristineHash =
        billiardgl::gameStateSha256(pristine.state());

    billiardgl::GameRuntime official;
    expect(official.setRulesMode(
            billiardgl::RulesMode::ChineseEightBallV1).ok,
        "official comparison authority enables approved rules");
    billiardgl::rules::RerackFoundationParameters rackParameters;
    rackParameters.deterministicSeed =
        billiardgl::rules::kOfficialRackDefaultSeed;
    rackParameters.breakerPlayerId = 0;
    const auto rackPlan = official.stageOfficialRerack(rackParameters);
    expect(rackPlan.ok && official.commitRackFoundation(
            rackPlan, "test.product.match.start.official-rack-v1").ok,
        "official comparison authority commits the fixed product rack");
    const std::string officialHash =
        billiardgl::gameStateSha256(official.state());

    expect(fixture.shell.startSinglePlayer(11).ok,
        "initial single-player match starts");
    expect(exactOfficialFixedRack(fixture.authority) &&
        billiardgl::gameStateSha256(fixture.authority.state()) == officialHash &&
        fixture.authority.rulesAuthorityActions().size() == 1 &&
        fixture.authority.rulesAuthorityActions().front().actionId ==
            billiardgl::rules::kFoundationRerackActionId &&
        fixture.ai.authorityActionCountAtConfigure == 1 &&
        fixture.ai.authorityRevisionAtConfigure ==
            fixture.authority.rulesAuthorityRevision(),
        "product start commits one exact fixed official rack before AI configuration and first Match visibility");
    billiardgl::GameAction aim;
    aim.type = billiardgl::ActionType::SetAimYaw;
    aim.first = 0.37f;
    billiardgl::GameAction power;
    power.type = billiardgl::ActionType::SetShotPower;
    power.first = 88.0f;
    billiardgl::GameAction shoot;
    shoot.type = billiardgl::ActionType::Shoot;
    expect(fixture.authority.dispatch(aim).ok &&
        fixture.authority.dispatch(power).ok &&
        fixture.authority.dispatch(shoot).ok &&
        fixture.authority.state().ballsMoving,
        "existing match is deliberately polluted with a moving shot");
    expect(fixture.shell.startSinglePlayer(12).ok &&
        fixture.authority.tick() == 0 &&
        fixture.authority.appliedSemanticActions().empty() &&
        billiardgl::gameStateSha256(fixture.authority.state()) == officialHash &&
        fixture.ai.tickAtConfigure == 0 &&
        fixture.ai.rulesModeAtConfigure ==
            billiardgl::RulesMode::ChineseEightBallV1 &&
        fixture.ai.lastConfiguration.deterministicSeed == 12 &&
        fixture.authority.rulesAuthorityActions().size() == 1 &&
        exactOfficialFixedRack(fixture.authority),
        "restart resets moving authority and commits exactly one fixed official rack before AI setup");

    const std::string busyHash =
        billiardgl::gameStateSha256(fixture.authority.state());
    const std::uint64_t busyRevision =
        fixture.authority.rulesAuthorityRevision();
    const auto busyActions = fixture.authority.rulesAuthorityActions();
    const auto busyScreen = fixture.shell.screen();
    const auto busyAiConfiguration = fixture.ai.state().configuration;
    fixture.ai.forceThinking = true;
    const auto busy = fixture.shell.startSinglePlayer(13);
    fixture.ai.forceThinking = false;
    expect(!busy.ok && busy.errorCode == "ai_busy" &&
        fixture.shell.screen() == busyScreen &&
        billiardgl::gameStateSha256(fixture.authority.state()) == busyHash &&
        fixture.authority.rulesAuthorityRevision() == busyRevision &&
        fixture.authority.rulesAuthorityActions().size() ==
            busyActions.size() &&
        fixture.ai.state().configuration.deterministicSeed ==
            busyAiConfiguration.deterministicSeed,
        "busy AI rejects restart before any shell/runtime/rack/AI mutation");

    expect(fixture.shell.loadTrainingCatalog(
        trainingManifest().string(), allCapabilities()).ok,
        "training pack loads for restart contamination coverage");
    const std::string scenarioId = fixture.training.scenarios().front().id;
    expect(fixture.shell.startTrainingScenario(
        scenarioId, "product-shell-hardening-v1", 31).ok &&
        fixture.training.snapshot().state ==
            billiardgl::training::TrainingProductState::AttemptActive &&
        fixture.shell.startSinglePlayer(32).ok &&
        fixture.training.snapshot().state ==
            billiardgl::training::TrainingProductState::PackReady &&
        fixture.training.currentScenario() == nullptr &&
        billiardgl::gameStateSha256(fixture.authority.state()) == officialHash &&
        fixture.authority.rulesAuthorityActions().size() == 1,
        "starting a match abandons training and commits exactly one official rack");
    expect(fixture.shell.startTrainingScenario(
        scenarioId, "product-shell-hardening-v1", 33).ok,
        "abandoned training pack remains reusable after match transition");

    ReplayFile replay = makeReplayFile(
        "billiardgl_product_shell_restart_replay.json",
        &fixture.authority.physicsProfile());
    expect(fixture.shell.loadReplayPath(replay.path.string(), replay.executable,
        replay.content).ok &&
        fixture.shell.dispatchProductInput(
            {"keyboard", "Space", "press", 0.0, 0.0}).ok &&
        !fixture.replay.snapshot().paused &&
        fixture.shell.startSinglePlayer(34).ok &&
        fixture.replay.snapshot().paused &&
        fixture.input.surface() == billiardgl::ProductInputSurface::Play &&
        !hasInputContext(fixture, "replay") &&
        billiardgl::gameStateSha256(fixture.authority.state()) == officialHash &&
        fixture.authority.rulesAuthorityActions().size() == 1,
        "starting from replay creates a fresh official rack and restores play input");

    fixture.authority.mutableState().balls[0].position.x = -777.0f;
    fixture.ai.failNextConfiguration = true;
    const auto failed = fixture.shell.startSinglePlayer(35);
    expect(!failed.ok &&
        failed.errorCode == "fake_ai_configuration_failed" &&
        fixture.shell.screen() ==
            billiardgl::ProductScreen::SinglePlayerSetup &&
        fixture.shell.snapshot().errorCode == failed.errorCode &&
        fixture.authority.rulesMode() ==
            billiardgl::RulesMode::ChineseEightBallV1 &&
        fixture.authority.tick() == 0 &&
        fixture.authority.appliedSemanticActions().empty() &&
        billiardgl::gameStateSha256(fixture.authority.state()) == pristineHash &&
        fixture.authority.rulesAuthorityActions().empty() &&
        !fixture.ai.configured &&
        fixture.ai.state().status ==
            billiardgl::ai::PlayerPollStatus::Disabled &&
        fixture.input.surface() == billiardgl::ProductInputSurface::Play,
        "AI failure atomically rolls runtime, rack receipt/revision surface, shell and AI back to clean setup");
}

void testKeyboardOnlyNavigationAndInputContexts()
{
    Fixture fixture("billiardgl_product_shell_keyboard_paths_v1.json");
    const auto unloaded = fixture.shell.snapshot();
    const auto* unavailableTraining = itemById(unloaded, "training");
    expect(unavailableTraining != nullptr && !unavailableTraining->enabled &&
        unavailableTraining->value == "Pack not loaded",
        "main menu does not claim drills before a validated pack is loaded");
    expect(fixture.shell.navigate(
            billiardgl::ShellNavigationCommand::FocusNext).ok &&
        focusedItemId(fixture.shell.snapshot()) == "replays",
        "keyboard focus skips disabled main-menu training");
    expect(fixture.shell.loadTrainingCatalog(
            "/definitely/missing/training-pack.json", {}).errorCode ==
            "training_pack_io" &&
        !fixture.shell.navigate(
            billiardgl::ShellNavigationCommand::FocusNext).ok,
        "navigation fails only when a screen has no enabled item");

    fixture.shell.openMainMenu();
    expect(fixture.shell.navigate(
            billiardgl::ShellNavigationCommand::Activate).ok &&
        fixture.shell.screen() ==
            billiardgl::ProductScreen::SinglePlayerSetup &&
        focusedItemId(fixture.shell.snapshot()) == "rookie",
        "Main to Setup lands on the first enabled difficulty, not disabled Rules");
    expect(fixture.shell.navigate(
            billiardgl::ShellNavigationCommand::FocusNext).ok &&
        fixture.shell.navigate(
            billiardgl::ShellNavigationCommand::Activate).ok &&
        fixture.shell.selectedDifficulty() == billiardgl::ai::Difficulty::Club &&
        fixture.shell.navigate(
            billiardgl::ShellNavigationCommand::FocusNext).ok &&
        fixture.shell.navigate(
            billiardgl::ShellNavigationCommand::FocusNext).ok &&
        fixture.shell.navigate(
            billiardgl::ShellNavigationCommand::Activate).ok &&
        fixture.shell.screen() == billiardgl::ProductScreen::Match,
        "keyboard-only Setup selects difficulty and reaches Start match");

    expect(fixture.shell.loadTrainingCatalog(
        trainingManifest().string(), allCapabilities()).ok,
        "keyboard training path loads its validated product data");
    fixture.shell.openMainMenu();
    expect(fixture.shell.navigate(
            billiardgl::ShellNavigationCommand::FocusNext).ok &&
        focusedItemId(fixture.shell.snapshot()) == "training" &&
        fixture.shell.navigate(
            billiardgl::ShellNavigationCommand::Activate).ok &&
        fixture.shell.screen() ==
            billiardgl::ProductScreen::TrainingCatalog &&
        fixture.input.surface() == billiardgl::ProductInputSurface::Training &&
        hasInputContext(fixture, "training"),
        "keyboard-only Main to Training uses the training input surface");

    fixture.shell.openReplayBrowser();
    expect(fixture.input.surface() == billiardgl::ProductInputSurface::Replay &&
        hasInputContext(fixture, "replay") &&
        fixture.shell.navigate(
            billiardgl::ShellNavigationCommand::Back).ok &&
        fixture.shell.screen() == billiardgl::ProductScreen::MainMenu &&
        fixture.input.surface() == billiardgl::ProductInputSurface::Play &&
        hasInputContext(fixture, "play") &&
        !hasInputContext(fixture, "replay") &&
        !hasInputContext(fixture, "training"),
        "Replay Back restores the main-menu play contexts without leakage");

    // With a loaded pack: Single player -> Training -> Replays -> Settings.
    for (int index = 0; index < 3; ++index)
        expect(fixture.shell.navigate(
            billiardgl::ShellNavigationCommand::FocusNext).ok,
            "keyboard focus advances toward Settings");
    expect(focusedItemId(fixture.shell.snapshot()) == "settings" &&
        fixture.shell.navigate(
            billiardgl::ShellNavigationCommand::Activate).ok &&
        fixture.shell.screen() == billiardgl::ProductScreen::Settings &&
        hasInputContext(fixture, "settings") &&
        fixture.shell.dispatchProductInput(
            {"keyboard", "+", "press", 0.0, 0.0}).ok &&
        fixture.shell.dispatchProductInput(
            {"keyboard", "Enter", "press", 0.0, 0.0}).ok &&
        fixture.shell.screen() == billiardgl::ProductScreen::MainMenu &&
        !fixture.settings.menu().isOpen() && hasInputContext(fixture, "play"),
        "keyboard-only Settings Apply returns to its source screen and context");

    for (int index = 0; index < 3; ++index)
        expect(fixture.shell.navigate(
            billiardgl::ShellNavigationCommand::FocusNext).ok,
            "keyboard focus advances toward Settings cancel path");
    expect(fixture.shell.navigate(
            billiardgl::ShellNavigationCommand::Activate).ok &&
        fixture.shell.dispatchProductInput(
            {"keyboard", "Escape", "press", 0.0, 0.0}).ok &&
        fixture.shell.screen() == billiardgl::ProductScreen::MainMenu &&
        !fixture.settings.menu().isOpen() && hasInputContext(fixture, "play"),
        "keyboard Escape cancels Settings without becoming an app quit");
}

void testTrainingSettingsAndRebindSnapshots()
{
    Fixture fixture("billiardgl_product_shell_training_settings_v1.json");
    expect(fixture.shell.loadTrainingCatalog(
        trainingManifest().string(), allCapabilities()).ok,
        "shell delegates training pack load");
    const auto catalog = fixture.shell.snapshot();
    expect(catalog.screen == billiardgl::ProductScreen::TrainingCatalog &&
        catalog.items.size() == 12 && catalog.training.scenarioCount == 12,
        "catalog exposes all 12 authoritative launch drills");
    expect(catalog.items.front().label != catalog.items.front().id &&
        !catalog.items.front().value.empty() &&
        !catalog.items.front().detail.empty(),
        "catalog presents retained localized title, assist, and objective metadata");
    expect(fixture.shell.startTrainingScenario(catalog.items.front().id,
        "billiardgl-product-source-v1", 42).ok &&
        fixture.shell.screen() == billiardgl::ProductScreen::TrainingSession &&
        itemById(fixture.shell.snapshot(), "preview_training") != nullptr &&
        itemById(fixture.shell.snapshot(), "save_training_replay") != nullptr,
        "training scenario request delegates and exposes the compact primary preview/save controls");
    const auto session = fixture.shell.snapshot();
    expect(itemById(session, "training_terminal") != nullptr &&
        itemById(session, "training_terminal")->readOnlyInformational &&
        !itemById(session, "training_terminal")->enabled &&
        itemById(session, "watch_training_reference") != nullptr &&
        itemById(session, "training_replay_status") == nullptr &&
        itemById(session, "trajectory_predicted") != nullptr &&
        itemById(session, "trajectory_actual") != nullptr &&
        itemById(session, "training_advanced") != nullptr &&
        session.items.back().id == "training_advanced" &&
        session.items.back().value == "Collapsed",
        "training hierarchy exposes terminal/replay/legend rows with Advanced last and collapsed");
    expect(fixture.shell.focusItem("training_advanced").ok &&
        fixture.shell.navigate(
            billiardgl::ShellNavigationCommand::Activate).ok &&
        itemById(fixture.shell.snapshot(),
            "trajectory_predicted_sample_count") != nullptr &&
        itemById(fixture.shell.snapshot(),
            "trajectory_content_hash") != nullptr &&
        fixture.shell.snapshot().items.back().id == "training_advanced",
        "explicit Advanced activation reflows diagnostics and retains a close action last");
    const auto advanced = fixture.shell.snapshot();
    const char* requiredAdvanced[] = {
        "trajectory_predicted_sample_count", "trajectory_actual_sample_count",
        "trajectory_first_replay_tick", "trajectory_last_replay_tick",
        "trajectory_preview_budget", "trajectory_authority_status",
        "trajectory_trace_hash", "trajectory_content_hash"};
    expect(std::all_of(std::begin(requiredAdvanced), std::end(requiredAdvanced),
            [&](const char* id) {
                const auto* value = itemById(advanced, id);
                return value != nullptr && !value->value.empty();
            }) && itemById(advanced, "trajectory_content_hash")
                    ->accessibleValue.rfind("sha256:", 0) == 0,
        "Advanced exposes all eight correction-gate fields and a full accessible content hash");
    auto validAdvanced = advanced;
    for (auto& value : validAdvanced.items) {
        if (std::find(std::begin(requiredAdvanced), std::end(requiredAdvanced),
                value.id) != std::end(requiredAdvanced)) value.value = "1";
        if (value.id == "trajectory_trace_hash" ||
            value.id == "trajectory_content_hash") {
            value.value = "aaaaaaaaaaaa...aaaaaaaa";
            value.accessibleValue = "sha256:" + std::string(64, 'a');
        }
    }
    expect(billiardgl::render::ui::auditPhase4InspectorContent(
            validAdvanced).ok,
        "complete Advanced eight-field inventory passes the structural content gate");
    for (const char* id : requiredAdvanced) {
        auto missing = validAdvanced;
        missing.items.erase(std::remove_if(missing.items.begin(),
            missing.items.end(), [&](const auto& value) {
                return value.id == id;
            }), missing.items.end());
        expect(!billiardgl::render::ui::auditPhase4InspectorContent(
                missing).ok,
            "each independently missing Advanced field fails closed");
    }
    auto missingRails = validAdvanced;
    for (auto& value : missingRails.items)
        if (value.id == "trajectory_actual") value.detail.clear();
    expect(!billiardgl::render::ui::auditPhase4InspectorContent(
            missingRails).ok,
        "missing Rails N on either Advanced legend row fails closed");
    expect(fixture.shell.focusItem("training_advanced").ok &&
        fixture.shell.navigate(
            billiardgl::ShellNavigationCommand::Activate).ok &&
        itemById(fixture.shell.snapshot(),
            "trajectory_predicted_sample_count") == nullptr,
        "Advanced closes explicitly and emits zero collapsed detail rows");
#ifdef NDEBUG
    const std::string liveReferenceHash =
        billiardgl::replay::replayAuthorityStateSha256(
            fixture.authority.state());
    const std::uint64_t liveReferenceTick = fixture.authority.tick();
    expect(fixture.shell.focusItem("watch_training_reference").ok &&
        fixture.shell.navigate(
            billiardgl::ShellNavigationCommand::Activate).ok &&
        fixture.shell.screen() == billiardgl::ProductScreen::ReplayPlayback &&
        fixture.shell.snapshot().replay.loaded &&
        fixture.shell.snapshot().replay.paused &&
        fixture.shell.snapshot().replay.tick == 0,
        "Watch reference enters a paused isolated verified playback");
    int referenceSteps = 0;
    while (!fixture.shell.snapshot().replay.finished &&
            referenceSteps < 6000) {
        expect(fixture.shell.dispatchProductInput(
            {"keyboard", ".", "press", 0.0, 0.0}).ok,
            "Watch reference advances one authoritative replay tick");
        ++referenceSteps;
    }
    expect(referenceSteps > 0 && referenceSteps < 6000 &&
        fixture.shell.snapshot().replay.finished &&
        fixture.replay.player().finalHashMatches() &&
        liveReferenceHash ==
            billiardgl::replay::replayAuthorityStateSha256(
                fixture.authority.state()) &&
        liveReferenceTick == fixture.authority.tick(),
        "Watch reference reaches its verified endpoint without touching live training");
    expect(fixture.shell.navigate(
            billiardgl::ShellNavigationCommand::Back).ok &&
        fixture.shell.screen() == billiardgl::ProductScreen::TrainingSession,
        "Watch reference returns to the active training session");
#endif
    expect(fixture.shell.focusItem("preview_training").ok &&
        fixture.shell.navigate(
            billiardgl::ShellNavigationCommand::Activate).ok &&
        fixture.training.snapshot().previewReady &&
        fixture.training.snapshot().previewAuthorityUnchanged,
        "training preview control runs an isolated authority-safe prediction");
    billiardgl::training::AttemptFacts terminalFailure;
    terminalFailure.state = fixture.authority.state();
    terminalFailure.numericalFailure = true;
    expect(fixture.training.finishAttempt(terminalFailure).ok &&
        fixture.shell.focusItem("save_training_replay").ok,
        "terminal attempt enables the replay save control");
    const auto saveRequest = fixture.shell.navigate(
        billiardgl::ShellNavigationCommand::Activate);
    expect(saveRequest.ok && saveRequest.appRequest ==
            billiardgl::ProductShellAppRequest::SaveTrainingReplayFile,
        "training replay save emits an explicit platform file callback request");

    expect(fixture.shell.openMainMenu().ok && fixture.shell.openSettings().ok,
        "settings screen opens with a live transaction");
    fixture.shell.dispatchProductInput(
        {"keyboard", "ArrowRight", "press", 0.0, 0.0});
    fixture.shell.dispatchProductInput(
        {"keyboard", "ArrowRight", "press", 0.0, 0.0});
    fixture.shell.dispatchProductInput(
        {"keyboard", "ArrowRight", "press", 0.0, 0.0});
    const auto audioBefore = fixture.shell.snapshot();
    expect(audioBefore.screen == billiardgl::ProductScreen::Settings &&
        audioBefore.items.size() == 8 && audioBefore.settingsPreviewActive &&
        itemById(audioBefore, "music_volume") != nullptr,
        "settings snapshot shows actual audio values and all four buses");
    expect(fixture.shell.dispatchProductInput(
        {"keyboard", "+", "press", 0.0, 0.0}).ok,
        "normalized input updates settings preview");
    const auto audioPreview = fixture.shell.snapshot();
    expect(itemById(audioPreview, "music_volume")->detail.find("Saved:") == 0,
        "changed preview retains visible saved value");

    expect(fixture.shell.beginRebind("toggle_aim").ok &&
        fixture.shell.snapshot().rebindCaptureActive &&
        fixture.shell.dispatchProductInput(
            {"keyboard", "F2", "press", 0.0, 0.0}).ok &&
        !fixture.shell.snapshot().rebindCaptureActive,
        "settings snapshot exposes rebind capture state");
    expect(fixture.shell.dispatchProductInput(
        {"keyboard", "Enter", "press", 0.0, 0.0}).ok &&
        fixture.shell.screen() == billiardgl::ProductScreen::MainMenu &&
        std::filesystem::exists(fixture.settingsPath),
        "apply atomically persists and returns to prior shell screen");
    expect(fixture.shell.dispatchProductInput(
        {"keyboard", "F2", "press", 0.0, 0.0}).ok &&
        fixture.authority.state().aim.mode == billiardgl::AimMode::Aim,
        "shell input immediately observes applied rebind");

    fixture.shell.openSettings();
    const auto badRebind = fixture.shell.beginRebind("missing_action");
    expect(!badRebind.ok && !fixture.shell.snapshot().errorCode.empty() &&
        !fixture.shell.snapshot().errorText.empty(),
        "settings errors remain visible in shipping snapshot");
}

void testCameraSettingsPreviewApplyCancel()
{
    Fixture fixture("billiardgl_product_shell_camera_settings_v1.json");
    fixture.shell.syncCameraPresentationPreferences();
    const auto authored = fixture.authority.cameraFrame().pose;
    const bool savedShake = fixture.authority.cameraPresentationPreferences().impactShakeEnabled;
    expect(savedShake == fixture.settings.current().cameraShake,
        "startup camera shake preference uses the persisted product setting");
    expect(fixture.shell.openSettings().ok,
        "camera settings transaction opens");
    for (int index = 0; index < 4; ++index)
        expect(fixture.shell.dispatchProductInput(
            {"keyboard", "ArrowRight", "press", 0.0, 0.0}).ok,
            "camera settings reaches Display category");
    expect(fixture.shell.focusItem("field_of_view").ok &&
        fixture.shell.navigate(
            billiardgl::ShellNavigationCommand::Activate).ok,
        "pointer-style settings activation previews FOV adjustment");
    const auto fovPreview = fixture.authority.cameraFrame().pose;
    const auto display = fixture.shell.snapshot();
    const auto* fovItem = itemById(display, "field_of_view");
    const auto* distanceItem = itemById(display, "camera_distance");
    expect(close(fovPreview.fieldOfViewDegrees,
                 authored.fieldOfViewDegrees + 1.0f) &&
        close(fixture.replay.cameraPresentationPreferences().
                  fieldOfViewOffsetDegrees, 1.0f) &&
        fovItem != nullptr && fovItem->label == "FOV adjustment" &&
        fovItem->value == "+1 deg" && distanceItem != nullptr &&
        distanceItem->label == "Camera distance scale" &&
        distanceItem->value == "100%",
        "live and replay cameras share truthful relative settings preview");

    expect(fixture.shell.focusItem("camera_distance").ok &&
        fixture.shell.navigate(
            billiardgl::ShellNavigationCommand::Activate).ok,
        "camera distance percentage previews through pointer path");
    expect(close(fixture.authority.cameraFrame().pose.distanceCm,
                 authored.distanceCm * (185.0 / 180.0), 0.001),
        "camera distance preview scales the authored composition once");
    expect(fixture.shell.focusItem("camera_shake").ok &&
        fixture.shell.navigate(billiardgl::ShellNavigationCommand::Activate).ok &&
        fixture.authority.cameraPresentationPreferences().impactShakeEnabled == !savedShake &&
        fixture.replay.cameraPresentationPreferences().impactShakeEnabled == !savedShake,
        "camera shake preview reaches live and replay preferences");
    expect(fixture.shell.navigate(
            billiardgl::ShellNavigationCommand::Back).ok &&
        close(fixture.authority.cameraFrame().pose.fieldOfViewDegrees,
              authored.fieldOfViewDegrees) &&
        close(fixture.authority.cameraFrame().pose.distanceCm,
              authored.distanceCm) &&
        close(fixture.replay.cameraPresentationPreferences().distanceScale,
              1.0f) &&
        fixture.authority.cameraPresentationPreferences().impactShakeEnabled == savedShake &&
        fixture.replay.cameraPresentationPreferences().impactShakeEnabled == savedShake,
        "settings cancel restores live and replay camera preferences");

    expect(fixture.shell.openSettings().ok,
        "camera settings reopens for apply");
    for (int index = 0; index < 4; ++index)
        expect(fixture.shell.dispatchProductInput(
            {"keyboard", "ArrowRight", "press", 0.0, 0.0}).ok,
            "camera settings apply reaches Display category");
    expect(fixture.shell.focusItem("field_of_view").ok &&
        fixture.shell.navigate(
            billiardgl::ShellNavigationCommand::Activate).ok &&
        fixture.shell.dispatchProductInput(
            {"keyboard", "Enter", "press", 0.0, 0.0}).ok &&
        close(fixture.settings.current().fieldOfViewDegrees, 56.0) &&
        close(fixture.authority.cameraFrame().pose.fieldOfViewDegrees,
              authored.fieldOfViewDegrees + 1.0f),
        "settings apply atomically keeps the committed camera adjustment");
    expect(fixture.authority.setCameraPresetInstant(
            billiardgl::camera::SemanticState::AimPlayer,
            billiardgl::camera::Preset::PlayerLow).ok &&
        close(fixture.authority.cameraFrame().pose.fieldOfViewDegrees,
              billiardgl::camera::definition(
                  billiardgl::camera::SemanticState::AimPlayer).
                  fieldOfViewDegrees + 1.0f),
        "committed preference applies once to future semantic camera views");

    std::error_code ignored;
    std::filesystem::remove(fixture.settingsPath, ignored);
    std::filesystem::create_directory(fixture.settingsPath, ignored);
    expect(fixture.shell.openSettings().ok,
        "camera settings opens against a failing persistence path");
    for (int index = 0; index < 4; ++index)
        expect(fixture.shell.dispatchProductInput(
            {"keyboard", "ArrowRight", "press", 0.0, 0.0}).ok,
            "failing camera settings reaches Display category");
    expect(fixture.shell.focusItem("field_of_view").ok &&
        fixture.shell.navigate(
            billiardgl::ShellNavigationCommand::Activate).ok,
        "failing camera settings still previews safely");
    const float failedPreviewFov =
        fixture.authority.cameraFrame().pose.fieldOfViewDegrees;
    const auto rejected = fixture.shell.dispatchProductInput(
        {"keyboard", "Enter", "press", 0.0, 0.0});
    expect(!rejected.ok && fixture.settings.menu().isOpen() &&
        close(fixture.authority.cameraFrame().pose.fieldOfViewDegrees,
              failedPreviewFov),
        "save failure retains recoverable preview without mutating live settings");
    expect(fixture.shell.navigate(
            billiardgl::ShellNavigationCommand::Back).ok &&
        close(fixture.authority.cameraFrame().pose.fieldOfViewDegrees,
              billiardgl::camera::definition(
                  billiardgl::camera::SemanticState::AimPlayer).
                  fieldOfViewDegrees + 1.0f),
        "cancel after save failure restores the last committed camera preference");
    std::filesystem::remove_all(fixture.settingsPath, ignored);
}

void testOptInProductCameraCandidate()
{
    using namespace billiardgl;
    Fixture fixture("billiardgl_optin_product_camera_20261013.json");
    fixture.shell.setM4CameraCandidate(true);
    fixture.shell.syncCameraPresentationPreferences();
    expect(fixture.shell.startSinglePlayer(71).ok,
        "opt-in camera fixture starts official product Match");
    expect(fixture.authority.cameraPresentationPreferences().m4CameraCandidate &&
        fixture.replay.cameraPresentationPreferences().m4CameraCandidate,
        "candidate configuration reaches real runtime and replay cameras");
    const auto authorityHash = fixture.authority.rulesAuthorityStateHash();
    const auto effectHash = fixture.authority.rulesReplayEffectStateHash();
    const auto balls = fixture.authority.state().balls;
    expect(fixture.authority.setCameraPresetInstant(camera::SemanticState::AimOverhead,
        camera::Preset::Overhead).ok, "candidate uses real product overhead state");
    const auto pose=fixture.authority.cameraFrame().pose;
    CameraPresentation c;
    c.eye={pose.eye.x,pose.eye.y,pose.eye.z};
    c.target={pose.target.x,pose.target.y,pose.target.z};c.fieldOfViewDegrees=pose.fieldOfViewDegrees;
    const auto projection=render::buildSharedCameraProjection(c,1920,1080);
    const auto onScreen=[&](Point3 point) {
        const auto clip=render::transformWorldToClip(projection,point);
        return clip.w>0 && std::fabs(clip.x/clip.w)<=.84 && std::fabs(clip.y/clip.w)<=.84;
    };
    for (const auto& ball:balls)
        expect(onScreen({ball.position.x,ball.position.y,ball.position.z}),
            "all sixteen official ball centers are within overhead safe area");
    for (const auto& pocket:buildPocketOpenings(defaultTableSpec(),defaultPocketSpec()))
        expect(onScreen({pocket.centerX,kTableModelPlaySurfaceHeightCm,pocket.centerZ}),
            "all six physical pocket centers are within overhead safe area");
    GameAction yaw;yaw.type=ActionType::SetAimYaw;yaw.first=.62f;
    expect(fixture.authority.dispatch(yaw).ok &&
        fixture.authority.setCameraPresetInstant(camera::SemanticState::AimPlayer,
            camera::Preset::PlayerLow).ok, "real runtime consumes candidate low and shot yaw");
    const auto low=fixture.authority.cameraFrame().pose;
    expect(low.eye.y-kTableModelPlaySurfaceHeightCm>=8 &&
        low.eye.y-kTableModelPlaySurfaceHeightCm<=16 &&
        close(std::cos(low.yawRadians),-std::cos(fixture.authority.shotControl().yawRadians)),
        "real runtime low camera preserves shot axis and required height");
    expect(fixture.authority.rulesAuthorityStateHash()==authorityHash &&
        fixture.authority.rulesReplayEffectStateHash()==effectHash,
        "opt-in camera has no authority or replay effect hash delta");
}

void testReplayPathsAndPlaybackSnapshot()
{
    Fixture fixture("billiardgl_product_shell_replay_settings_v1.json");
    expect(!fixture.shell.loadReplayPath("", "x", "y").ok &&
        fixture.shell.snapshot().errorCode == "shell_replay_path_empty",
        "empty replay path has stable visible failure");
    expect(fixture.shell.openReplayBrowser().ok &&
        fixture.shell.focusItem("choose_replay").ok &&
        !fixture.shell.loadReplayPath("", "x", "y").ok,
        "public focus-then-invalid-load transition succeeds");
    const auto focusedReplayError = fixture.shell.snapshot();
    expect(focusedReplayError.errorCode == "shell_replay_path_empty" &&
        std::count_if(focusedReplayError.items.begin(),
            focusedReplayError.items.end(), [](const auto& item) {
                return item.focused && item.id == "choose_replay";
            }) == 1,
        "public focus-then-invalid-load preserves replay focus and visible error together");
    expect(!fixture.shell.loadReplayPath(
        "/definitely/missing/billiardgl.replay", "x", "y").ok &&
        fixture.shell.snapshot().errorCode == "shell_replay_read_failed",
        "missing replay path stays in browser with stable failure");
    const std::filesystem::path bad = std::filesystem::temp_directory_path() /
        "billiardgl_product_shell_bad_replay.json";
    {
        std::ofstream file(bad, std::ios::binary | std::ios::trunc);
        file << "not-json\n";
    }
    expect(!fixture.shell.loadReplayPath(bad.string(), "x", "y").ok &&
        fixture.shell.snapshot().errorCode == "shell_replay_json_invalid",
        "bad replay JSON is explicit and does not enter playback");
    std::error_code ignored;
    std::filesystem::remove(bad, ignored);

    ReplayFile replay = makeReplayFile(
        "billiardgl_product_shell_valid_replay.json");
    const std::string authorityHash =
        billiardgl::gameStateSha256(fixture.authority.state());
    const std::uint64_t authorityTick = fixture.authority.tick();
    expect(fixture.shell.loadReplayPath(replay.path.string(), replay.executable,
        replay.content).ok &&
        fixture.shell.screen() == billiardgl::ProductScreen::ReplayPlayback,
        "valid Replay v1 delegates to ReplayTransport and enters playback");
    auto playback = fixture.shell.snapshot();
    expect(playback.items.size() == 8 && playback.replay.loaded &&
        playback.replay.paused &&
        itemById(playback, "trajectory_actual") != nullptr &&
        itemById(playback, "trajectory_actual_topology") != nullptr &&
        itemById(playback, "replay_pause") != nullptr &&
        itemById(playback, "replay_step_tick") != nullptr &&
        itemById(playback, "replay_camera_reset") != nullptr &&
        itemById(playback, "trajectory_inset_toggle") != nullptr,
        "playback snapshot exposes Replay actual identity, state, play, and step controls");
    expect(billiardgl::render::ui::auditPhase4InspectorContent(playback).ok,
        "actual-only ReplayPlayback inventory and transport truth pass the structural gate");
    auto noActualLegend = playback;
    noActualLegend.items.erase(std::remove_if(noActualLegend.items.begin(),
        noActualLegend.items.end(), [](const auto& value) {
            return value.id == "trajectory_actual";
        }), noActualLegend.items.end());
    expect(!billiardgl::render::ui::auditPhase4InspectorContent(
            noActualLegend).ok,
        "actual-only geometry without Replay actual legend fails closed");
    expect(fixture.shell.dispatchProductInput(
        {"keyboard", "Space", "press", 0.0, 0.0}).ok &&
        !fixture.shell.snapshot().replay.paused &&
        fixture.shell.dispatchProductInput(
            {"keyboard", "+", "press", 0.0, 0.0}).ok &&
        fixture.shell.snapshot().replay.playbackRate == 2.0 &&
        fixture.shell.dispatchProductInput(
            {"keyboard", "C", "press", 0.0, 0.0}).ok &&
        fixture.shell.snapshot().replay.cameraMode ==
            billiardgl::replay::ReplayCameraMode::Broadcast,
        "playback snapshot updates from normalized replay controls");
    expect(authorityHash == billiardgl::gameStateSha256(fixture.authority.state()) &&
        authorityTick == fixture.authority.tick(),
        "replay shell controls do not mutate match authority");

    const std::filesystem::path mismatchPath =
        std::filesystem::temp_directory_path() /
        "billiardgl_product_shell_final_mismatch.replay.json";
    {
        std::ifstream input(replay.path, std::ios::binary);
        const std::string bytes((std::istreambuf_iterator<char>(input)),
            std::istreambuf_iterator<char>());
        auto decoded = billiardgl::json::parse(bytes);
        expect(decoded.ok, "valid replay decodes for endpoint mismatch control");
        decoded.value["final_summary"]["authority_state_sha256"] =
            billiardgl::json::Value(std::string(64, '0'));
        std::ofstream output(mismatchPath, std::ios::binary | std::ios::trunc);
        output << billiardgl::json::stringify(decoded.value) << '\n';
        expect(static_cast<bool>(output), "endpoint mismatch replay writes");
    }
    expect(fixture.shell.openReplayBrowser().ok &&
        fixture.shell.loadReplayPath(mismatchPath.string(), replay.executable,
            replay.content).ok,
        "well-formed endpoint mismatch enters isolated product playback");
    billiardgl::ProductShellResult endpoint;
    while (!fixture.shell.snapshot().replay.finished) {
        endpoint = fixture.shell.dispatchProductInput(
            {"keyboard", ".", "press", 0.0, 0.0});
    }
    expect(!endpoint.ok && endpoint.errorCode == "replay_final_hash_mismatch" &&
        fixture.shell.snapshot().errorCode == "replay_final_hash_mismatch" &&
        fixture.shell.screen() == billiardgl::ProductScreen::ReplayPlayback &&
        authorityHash == billiardgl::gameStateSha256(fixture.authority.state()) &&
        authorityTick == fixture.authority.tick(),
        "product playback surfaces a stable endpoint mismatch without mutating authority");
    expect(fixture.shell.dispatchProductInput(
            {"keyboard", ".", "press", 0.0, 0.0}).errorCode ==
            "replay_final_hash_mismatch" &&
        fixture.shell.snapshot().errorCode == "replay_final_hash_mismatch",
        "product playback keeps the endpoint mismatch stable after completion");
    std::filesystem::remove(mismatchPath, ignored);

    expect(fixture.shell.loadTrainingCatalog(
            trainingManifest().string(), allCapabilities()).ok &&
        fixture.shell.startTrainingScenario(
            fixture.training.scenarios().front().id,
            "product-shell-comparison-v1", 83).ok &&
        fixture.authority.step(1).ok,
        "training comparison endpoint fixture starts through the product shell");
    billiardgl::training::AttemptFacts terminalFacts;
    terminalFacts.state = fixture.authority.state();
    terminalFacts.numericalFailure = true;
    const std::filesystem::path comparisonPath =
        std::filesystem::temp_directory_path() /
        "billiardgl_product_shell_comparison_mismatch.replay.json";
    expect(fixture.training.finishAttempt(terminalFacts).ok &&
        fixture.training.saveReplay(comparisonPath.string()).ok,
        "training comparison fixture saves a registry-bound replay");
    {
        std::ifstream input(comparisonPath, std::ios::binary);
        const std::string bytes((std::istreambuf_iterator<char>(input)),
            std::istreambuf_iterator<char>());
        auto decoded = billiardgl::json::parse(bytes);
        expect(decoded.ok, "training comparison replay decodes");
        decoded.value["final_summary"]["state_sha256"] =
            billiardgl::json::Value(std::string(64, '0'));
        std::ofstream output(
            comparisonPath, std::ios::binary | std::ios::trunc);
        output << billiardgl::json::stringify(decoded.value) << '\n';
        expect(static_cast<bool>(output), "training comparison mismatch writes");
    }
    const auto trainingState = fixture.training.snapshot();
    const auto comparison = fixture.shell.loadTrainingComparisonReplayPath(
        comparisonPath.string(), trainingState.executableVersion,
        trainingState.scenarioFileSha256);
    expect(!comparison.ok &&
        comparison.errorCode == "replay_final_hash_mismatch" &&
        fixture.shell.snapshot().errorCode == "replay_final_hash_mismatch",
        "training comparison reports a stable endpoint mismatch instead of silent completion");
    std::filesystem::remove(comparisonPath, ignored);
}

billiardgl::render::ui::SdfFontAtlas makeAtlas()
{
    billiardgl::render::ui::FontAtlasConfig config;
    config.fontPath = "/System/Library/Fonts/Menlo.ttc";
    config.faceIndex = 0;
    config.nominalPixelSize = 32;
    config.scaleBucket = 1.0f;
    config.atlasWidth = 1024;
    config.atlasHeight = 1024;
    billiardgl::render::ui::SdfFontAtlas atlas;
    expect(atlas.initialize(config).ok && atlas.frozen(),
        "shipping shell atlas pre-bakes and freezes");
    return atlas;
}

bool overlaps(const billiardgl::product_hud::HudRect& first,
    const billiardgl::product_hud::HudRect& second)
{
    return first.x < second.x + second.width &&
        first.x + first.width > second.x &&
        first.y < second.y + second.height &&
        first.y + first.height > second.y;
}

bool sameVerticalExtent(const billiardgl::product_hud::HudRect& first,
    const billiardgl::product_hud::HudRect& second)
{
    constexpr float epsilon = 0.01f;
    return std::abs(first.y - second.y) <= epsilon &&
        std::abs(first.height - second.height) <= epsilon;
}

void testShippingTextAtlasBoundsAndDescenders()
{
    Fixture fixture("billiardgl_product_shell_text_bounds_v1.json");
    const auto atlas = makeAtlas();
    const std::vector<std::string> strings{
        "Play against the AI",
        "Preview, apply, or rebind",
        "Rules and AI difficulty",
        "Versioned single-rack rules",
        "Training"};
    for (const std::string& text : strings) {
        std::vector<std::uint32_t> codepoints;
        std::string error;
        expect(billiardgl::render::ui::decodeUtf8(
            text, codepoints, error),
            "shipping audit strings decode as UTF-8");
        for (const std::uint32_t codepoint : codepoints) {
            const auto* glyph = atlas.findGlyph(codepoint);
            expect(glyph != nullptr, "shipping audit glyph is pre-baked");
            if (glyph->bitmapWidth == 0 || glyph->bitmapHeight == 0) continue;
            expect(glyph->atlasWidth ==
                    glyph->bitmapWidth + glyph->sdfSpread * 2 &&
                glyph->atlasHeight ==
                    glyph->bitmapHeight + glyph->sdfSpread * 2 &&
                glyph->atlasX >= 0 && glyph->atlasY >= 0 &&
                glyph->atlasX + glyph->atlasWidth <= atlas.width() &&
                glyph->atlasY + glyph->atlasHeight <= atlas.height(),
                "shipping audit glyph keeps full bitmap plus SDF spread inside the atlas");
            bool hasInteriorInk = false;
            for (int y = glyph->sdfSpread;
                    y < glyph->atlasHeight - glyph->sdfSpread; ++y) {
                for (int x = glyph->sdfSpread;
                        x < glyph->atlasWidth - glyph->sdfSpread; ++x) {
                    const std::size_t offset = static_cast<std::size_t>(
                        (glyph->atlasY + y) * atlas.width() +
                        glyph->atlasX + x);
                    hasInteriorInk = hasInteriorInk ||
                        atlas.pixels()[offset] > 128;
                }
            }
            expect(hasInteriorInk,
                "shipping audit glyph retains above-threshold SDF pixels");
        }
    }

    billiardgl::render::ui::ShippingScreenDrawListBuilder builder;
    const billiardgl::render::ui::ShellViewport retina{
        1280, 720, 2560, 1440, 192.0f};
    std::vector<billiardgl::render::ui::ShellDrawList> frames;
    frames.push_back(builder.build(fixture.shell.snapshot(), retina, atlas));
    fixture.shell.openSinglePlayerSetup();
    frames.push_back(builder.build(fixture.shell.snapshot(), retina, atlas));
    for (const auto& frame : frames) {
        if (!frame.draw.ok)
            std::cerr << "retina shipping frame error=" <<
                frame.draw.errorCode << '\n';
        expect(frame.draw.ok, "Retina shipping text frame builds");
        for (const auto& primitive : frame.draw.primitives) {
            if (primitive.kind ==
                    billiardgl::render::ui::UiPrimitiveKind::GlyphQuad) {
                const bool complete = sameVerticalExtent(
                    primitive.glyphInkRect,
                    primitive.glyphInkSourceRect) &&
                    primitive.glyphInkRect.y >=
                        primitive.rect.y - 0.02f &&
                    primitive.glyphInkRect.y +
                        primitive.glyphInkRect.height <=
                        primitive.rect.y + primitive.rect.height + 0.02f;
                if (!complete)
                    std::cerr << "retina SDF clip text='" <<
                        (primitive.textRunIndex < frame.draw.textRuns.size() ?
                            frame.draw.textRuns[primitive.textRunIndex].text :
                            std::string("<invalid>")) << "' cp=" <<
                        primitive.codepoint << " sourceY=" <<
                        primitive.sourceRect.y << '+' <<
                        primitive.sourceRect.height << " clipY=" <<
                        primitive.rect.y << '+' << primitive.rect.height << '\n';
                expect(complete,
                    "Retina shipping glyph keeps complete ink while submitted SDF geometry contains it");
            }
        }
    }
    for (const std::string& text : strings) {
        const bool present = std::any_of(frames.begin(), frames.end(),
            [&](const auto& frame) {
                return std::any_of(frame.draw.textRuns.begin(),
                    frame.draw.textRuns.end(), [&](const auto& run) {
                        return run.text.find(text) != std::string::npos;
                    });
            });
        expect(present, "required shipping audit string reaches a draw-list text run");
    }
}

void validateDraw(const billiardgl::render::ui::ShellDrawList& list)
{
    if (!list.draw.ok)
        throw std::runtime_error("shipping shell draw failed: " +
            list.draw.errorCode);
    expect(list.draw.ok && !list.draw.containsDebug &&
        !list.draw.primitives.empty() && list.draw.glyphQuadCount > 0,
        "shipping shell produces non-debug panel and glyph primitives");
    int focusLines = 0;
    int icons = 0;
    for (const auto& primitive : list.draw.primitives) {
        const auto& safe = list.draw.safeRect;
        expect(primitive.rect.x >= safe.x - 0.001f &&
            primitive.rect.y >= safe.y - 0.001f &&
            primitive.rect.x + primitive.rect.width <=
                safe.x + safe.width + 0.001f &&
            primitive.rect.y + primitive.rect.height <=
                safe.y + safe.height + 0.001f,
            "all shell primitives remain inside safe bounds");
        if (primitive.kind ==
                billiardgl::render::ui::UiPrimitiveKind::Icon)
            ++icons;
        if (primitive.kind ==
                billiardgl::render::ui::UiPrimitiveKind::Line &&
            primitive.token == "keyboard_focus_outline")
            ++focusLines;
    }
    if (!(icons >= static_cast<int>(list.elements.size()) && focusLines >= 4)) {
        std::cerr << "focus/icon diagnostic screen=" << static_cast<int>(list.screen)
            << " viewport=" << list.viewport.physicalWidth << 'x' << list.viewport.physicalHeight
            << " logical=" << list.viewport.logicalWidth << 'x' << list.viewport.logicalHeight
            << " deviceScale=" << list.deviceScale << " effectiveUiScale=" << list.effectiveUiScale
            << " icons=" << icons << " elements=" << list.elements.size()
            << " focusLines=" << focusLines << " visible=";
        for (const auto& element : list.elements)
            std::cerr << element.id << "(focused=" << element.focused << "),";
        std::cerr << '\n';
    }
    expect(icons >= static_cast<int>(list.elements.size()) && focusLines >= 4,
        "items have shape icons and keyboard focus has a visible outline");
    for (std::size_t first = 0; first < list.elements.size(); ++first) {
        const auto& rect = list.elements[first].rect;
        const auto& safe = list.draw.safeRect;
        expect(rect.x >= safe.x && rect.y >= safe.y &&
            rect.x + rect.width <= safe.x + safe.width + 0.001f &&
            rect.y + rect.height <= safe.y + safe.height + 0.001f,
            "shell item layout remains in safe area");
        for (std::size_t second = first + 1;
                second < list.elements.size(); ++second)
            expect(!overlaps(rect, list.elements[second].rect),
                "shell item rectangles do not overlap");
    }
}

void testShippingDrawListResponsiveCoverage()
{
    Fixture fixture("billiardgl_product_shell_draw_v1.json");
    const billiardgl::PhysicsProfile shippingProfile =
        billiardgl::interactiveChinesePoolPhysicsProfile();
    ReplayFile replay = makeReplayFile(
        "billiardgl_product_shell_draw_replay.json", &shippingProfile);
    std::vector<billiardgl::ProductShellSnapshot> screens;
    screens.push_back(fixture.shell.snapshot());
    fixture.shell.openSinglePlayerSetup();
    screens.push_back(fixture.shell.snapshot());
    fixture.shell.startSinglePlayer(1);
    screens.push_back(fixture.shell.snapshot());
    fixture.shell.loadTrainingCatalog(
        trainingManifest().string(), allCapabilities());
    screens.push_back(fixture.shell.snapshot());
    fixture.shell.startTrainingScenario(
        fixture.training.scenarios().front().id, "product-shell-draw-v1", 2);
    screens.push_back(fixture.shell.snapshot());
    fixture.shell.openReplayBrowser();
    screens.push_back(fixture.shell.snapshot());
    fixture.shell.loadReplayPath(
        replay.path.string(), replay.executable, replay.content);
    screens.push_back(fixture.shell.snapshot());
    fixture.shell.openSettings();
    screens.push_back(fixture.shell.snapshot());
    fixture.shell.navigate(billiardgl::ShellNavigationCommand::Back);
    fixture.shell.openHelp();
    screens.push_back(fixture.shell.snapshot());
    expect(screens.size() == 9, "all nine shipping screen states have snapshots");

    const auto atlas = makeAtlas();
    const int rasterizations = atlas.rasterizationCount();
    const billiardgl::render::ui::ShellViewport viewports[] = {
        {1024, 768, 1024, 768, 96.0f},
        {1280, 720, 1280, 720, 96.0f},
        {1920, 1080, 1920, 1080, 96.0f},
        {1280, 720, 1920, 1080, 144.0f},
    };
    billiardgl::render::ui::ShippingScreenDrawListBuilder builder;
    const auto hudTokens = billiardgl::render::ui::loadHudShippingTokenSet(
        std::string(BILLIARDGL_SOURCE_ROOT) +
        "/assets/modern/hud/focused_practice_hud_shipping_tokens_v1.json");
    const auto shellTokens = billiardgl::render::ui::loadShellShippingTokenSet(
        std::string(BILLIARDGL_SOURCE_ROOT) +
        "/assets/modern/hud/focused_practice_shell_shipping_tokens_v1.json");
    const auto registry = billiardgl::render::ui::mergeHudShippingTokenSets(
        {hudTokens, shellTokens});
    expect(hudTokens.ok && shellTokens.ok && shellTokens.icons.size() == 43 &&
        registry.ok && registry.tokenSetIds.size() == 2,
        "nine-screen audit loads the exact HUD and Shell token namespaces");
    std::set<std::string> shellIconTokens;
    for (const auto& model : screens) {
        for (const auto& baseViewport : viewports) {
            for (float uiScale : {0.85f, 1.0f, 1.35f}) {
                auto scaledModel = model;
                scaledModel.uiScale = uiScale;
                // This matrix verifies full inspector/header content. A
                // compact default may legitimately collapse as rows grow;
                // exercise the real explicit Show panel state here. The
                // pointer suite separately verifies collapsed Enter/mouse
                // recovery and exact visible controls.
                if (scaledModel.screen == billiardgl::ProductScreen::TrainingSession ||
                    scaledModel.screen == billiardgl::ProductScreen::ReplayPlayback)
                    scaledModel.inspectorMode = 1;
                const auto draw = builder.build(
                    scaledModel, baseViewport, atlas);
                if (!draw.draw.ok)
                {
                    const auto failedLayout =
                        billiardgl::render::ui::buildShippingScreenLayout(
                            scaledModel, baseViewport);
                    throw std::runtime_error("responsive draw failed screen=" +
                        std::to_string(static_cast<int>(model.screen)) +
                        " physical=" +
                        std::to_string(baseViewport.physicalWidth) + "x" +
                        std::to_string(baseViewport.physicalHeight) +
                        " ui=" + std::to_string(uiScale) + " error=" +
                        draw.draw.errorCode + " items=" +
                        std::to_string(scaledModel.items.size()) + " body=" +
                        std::to_string(failedLayout.bodyRect.height) +
                        " inspector=" +
                        std::to_string(failedLayout.inspectorRect.width) + "x" +
                        std::to_string(failedLayout.inspectorRect.height) +
                        " mintext=" + std::to_string(failedLayout.minimumText));
                }
                validateDraw(draw);
                for (const auto& primitive : draw.draw.primitives) {
                    if (primitive.kind ==
                            billiardgl::render::ui::UiPrimitiveKind::Icon)
                        shellIconTokens.insert(primitive.token);
                }
                for (const auto palette : {
                        billiardgl::render::ui::HudPaletteMode::Standard,
                        billiardgl::render::ui::HudPaletteMode::ColorVisionSafe}) {
                    const auto expanded =
                        billiardgl::render::ui::expandShippingHudIcons(
                            draw.draw, registry, palette,
                            "focused_practice_shell_shipping_tokens_v1");
                    expect(expanded.ok && expanded.draw.ok &&
                        !expanded.draw.triangles.empty() &&
                        std::none_of(expanded.draw.primitives.begin(),
                            expanded.draw.primitives.end(), [](const auto& value) {
                                return value.kind == billiardgl::render::ui::
                                    UiPrimitiveKind::Icon;
                            }),
                        "every nine-screen standard/CVD frame expands all Icons to explicit CPU triangles");
                }
                const auto sharedLayout =
                    billiardgl::render::ui::buildShippingScreenLayout(
                        scaledModel, baseViewport);
                expect(sharedLayout.ok &&
                    sharedLayout.elements.size() == draw.elements.size(),
                    "draw list and pointer hit testing share one CPU layout");
                if (sharedLayout.inspectorCollapsed) {
                    for (const auto& primitive : draw.draw.primitives) {
                        if (primitive.kind != billiardgl::render::ui::UiPrimitiveKind::Panel)
                            continue;
                        const auto exact = [&](const auto& rect) {
                            return primitive.sourceRect.x == rect.x &&
                                primitive.sourceRect.y == rect.y &&
                                primitive.sourceRect.width == rect.width &&
                                primitive.sourceRect.height == rect.height;
                        };
                        bool explicitChrome = primitive.token == "shell_footer_panel" &&
                            exact(sharedLayout.collapsedSummaryRect);
                        explicitChrome |= primitive.token == "inspector_toggle" &&
                            exact(sharedLayout.inspectorToggleRect);
                        for (const auto& element : sharedLayout.elements)
                            explicitChrome |= primitive.token == element.id && exact(element.rect);
                        if (primitive.token == "trajectory_empty_hint") {
                            auto rect = sharedLayout.inspectorToggleRect;
                            rect.y += rect.height + 4.0f * sharedLayout.baseScale;
                            rect.height = 44.0f * sharedLayout.baseScale;
                            explicitChrome |= exact(rect);
                        }
                        expect(explicitChrome,
                            "collapsed backgrounds match only real summary, controls, toggle and empty hint");
                    }
                }
                if (!sharedLayout.inspectorCollapsed &&
                    (model.screen == billiardgl::ProductScreen::TrainingSession ||
                    model.screen == billiardgl::ProductScreen::ReplayPlayback)) {
                    std::vector<billiardgl::product_hud::HudRect> panels;
                    for (const auto& primitive : draw.draw.primitives) {
                        if (primitive.token == "inspector_toggle") {
                            const auto& rect = sharedLayout.inspectorToggleRect;
                            expect(primitive.sourceRect.x == rect.x && primitive.sourceRect.y == rect.y &&
                                primitive.sourceRect.width == rect.width && primitive.sourceRect.height == rect.height,
                                "persistent toggle matches its explicit chrome outside the inspector");
                            const auto hit = billiardgl::render::ui::hitTestShippingScreenLayout(sharedLayout,
                                (rect.x + rect.width / 2) * baseViewport.logicalWidth / baseViewport.physicalWidth,
                                (rect.y + rect.height / 2) * baseViewport.logicalHeight / baseViewport.physicalHeight);
                            expect(hit.hit && hit.id == "inspector_toggle", "toggle drawing and hit target agree");
                            continue;
                        }
                        if (primitive.token == "trajectory_empty_hint") {
                            const auto& toggle = sharedLayout.inspectorToggleRect;
                            const float base = sharedLayout.baseScale;
                            expect(primitive.sourceRect.x == toggle.x &&
                                primitive.sourceRect.y == toggle.y + toggle.height + 4.0f * base &&
                                primitive.sourceRect.width == toggle.width &&
                                primitive.sourceRect.height == 44.0f * base,
                                "empty trajectory hint occupies only its explicit toggle-adjacent rectangle");
                            continue;
                        }
                        if (primitive.kind == billiardgl::render::ui::
                                UiPrimitiveKind::Panel ||
                            primitive.kind == billiardgl::render::ui::
                                UiPrimitiveKind::SolidRect) {
                            panels.push_back(primitive.sourceRect);
                            expect(primitive.color.alpha >= 0.78f &&
                                primitive.color.alpha <= 0.86f,
                                "bounded inspector panel alpha stays inside the correction-gate range");
                            expect(primitive.sourceRect.x >=
                                    sharedLayout.inspectorRect.x &&
                                primitive.sourceRect.y >=
                                    sharedLayout.inspectorRect.y &&
                                primitive.sourceRect.x +
                                    primitive.sourceRect.width <=
                                    sharedLayout.inspectorRect.x +
                                    sharedLayout.inspectorRect.width + 0.001f &&
                                primitive.sourceRect.y +
                                    primitive.sourceRect.height <=
                                    sharedLayout.inspectorRect.y +
                                    sharedLayout.inspectorRect.height + 0.001f,
                                "every inspector background is clipped to the bounded inspector");
                        }
                    }
                    expect(billiardgl::render::ui::
                            auditBoundedInspectorGeometry(
                                sharedLayout, panels, 0.0f).ok,
                        "bounded inspector geometry passes profile spans, area, corridor, and scrim gates");
                    if (baseViewport.physicalWidth == 1920 &&
                        baseViewport.physicalHeight == 1080) {
                        expect(sharedLayout.inspectorRect.width <= 480.0f &&
                            sharedLayout.inspectorRect.height <= 780.0f &&
                            sharedLayout.inspectorViewportCoverage <= 0.18f &&
                            sharedLayout.gameplayCorridorRect.width >= 1240.0f &&
                            sharedLayout.gameplayCorridorRect.height >= 760.0f,
                            "1080 inspector and no-panel corridor meet the correction gate");
                    }
                    if (baseViewport.physicalWidth == 1280 &&
                        baseViewport.physicalHeight == 720) {
                        expect(sharedLayout.inspectorRect.width <= 420.0f &&
                            sharedLayout.inspectorRect.height <= 620.0f &&
                            sharedLayout.gameplayCorridorRect.width >= 760.0f &&
                            sharedLayout.gameplayCorridorRect.height >= 520.0f &&
                            std::all_of(sharedLayout.elements.begin(),
                                sharedLayout.elements.end(), [&](const auto& value) {
                                    return value.rect.x ==
                                        sharedLayout.elements.front().rect.x;
                                }),
                            "720 UI-135 bounded inspector is a single column with a continuous gameplay corridor");
                    }
                }
                expect(!overlaps(sharedLayout.breadcrumbLineRect,
                        sharedLayout.titleLineRect) &&
                    !overlaps(sharedLayout.titleLineRect,
                        sharedLayout.subtitleLineRect) &&
                    !overlaps(sharedLayout.breadcrumbLineRect,
                        sharedLayout.subtitleLineRect) &&
                    sharedLayout.subtitleLineRect.y +
                        sharedLayout.subtitleLineRect.height <=
                        sharedLayout.headerRect.y +
                        sharedLayout.headerRect.height + 0.001f,
                    "shell header breadcrumb, title, and subtitle own disjoint line boxes inside the header");
                const auto headerRun = [&](const std::string& text,
                        const billiardgl::product_hud::HudRect& line) {
                    return std::find_if(draw.draw.textRuns.begin(),
                        draw.draw.textRuns.end(), [&](const auto& run) {
                            return run.text == text &&
                                run.clipRect.y == line.y;
                        });
                };
                const auto titleRun = headerRun(
                    scaledModel.title, sharedLayout.titleLineRect);
                auto subtitleRun = headerRun(
                    scaledModel.subtitle, sharedLayout.subtitleLineRect);
                if (subtitleRun == draw.draw.textRuns.end() &&
                    scaledModel.screen == billiardgl::ProductScreen::TrainingSession &&
                    scaledModel.manualInspectorPolicy) {
                    std::string wrappedSubtitle;
                    for (const auto& run : draw.draw.textRuns) {
                        if (run.clipRect.x == sharedLayout.subtitleLineRect.x &&
                            run.clipRect.y >= sharedLayout.subtitleLineRect.y &&
                            run.clipRect.y < sharedLayout.subtitleLineRect.y +
                                sharedLayout.subtitleLineRect.height)
                            wrappedSubtitle += run.text;
                    }
                    expect(wrappedSubtitle == scaledModel.subtitle,
                        "wrapped Training subtitle preserves the exact scenario id");
                    subtitleRun = std::find_if(draw.draw.textRuns.begin(),
                        draw.draw.textRuns.end(), [&](const auto& run) {
                            return run.clipRect.x ==
                                    sharedLayout.subtitleLineRect.x &&
                                run.clipRect.y >=
                                    sharedLayout.subtitleLineRect.y &&
                                run.clipRect.y <
                                    sharedLayout.subtitleLineRect.y +
                                        sharedLayout.subtitleLineRect.height;
                        });
                }
                expect(titleRun != draw.draw.textRuns.end() &&
                    subtitleRun != draw.draw.textRuns.end() &&
                    titleRun->clipRect.y == sharedLayout.titleLineRect.y &&
                    subtitleRun->clipRect.y ==
                        sharedLayout.subtitleLineRect.y &&
                    titleRun->baselineY < subtitleRun->baselineY,
                    "shell draw list consumes the CPU header line-box contract without shared baselines");
                for (std::size_t index = 0;
                        index < draw.elements.size(); ++index) {
                    const auto& rendered = draw.elements[index];
                    const auto& hittable = sharedLayout.elements[index];
                    expect(rendered.id == hittable.id &&
                        rendered.rect.x == hittable.rect.x &&
                        rendered.rect.y == hittable.rect.y &&
                        rendered.rect.width == hittable.rect.width &&
                        rendered.rect.height == hittable.rect.height,
                        "draw and hit regions cannot drift by scattered coordinates");
                    const double logicalX =
                        (hittable.rect.x + hittable.rect.width * 0.5) *
                        baseViewport.logicalWidth /
                        baseViewport.physicalWidth;
                    const double logicalY =
                        (hittable.rect.y + hittable.rect.height * 0.5) *
                        baseViewport.logicalHeight /
                        baseViewport.physicalHeight;
                    const auto hit =
                        billiardgl::render::ui::hitTestShippingScreenLayout(
                            sharedLayout, logicalX, logicalY);
                    expect(hit.hit && hit.id == rendered.id &&
                        hit.enabled == rendered.enabled,
                        "logical pointer center resolves to the rendered stable id");
                }
                expect(draw.screen == model.screen &&
                    draw.effectiveUiScale >= draw.deviceScale * 0.85f &&
                    draw.effectiveUiScale <= draw.deviceScale * 1.35f,
                    "draw list retains screen identity and bounded UI scale");
            }
        }
    }
    expect(atlas.rasterizationCount() == rasterizations,
        "ordinary shell frames never rasterize new glyphs");

    // The native 720p profile is a 640x360 logical window backed by a
    // 1280x720 Retina drawable.  Validate submitted ink rather than advances:
    // bearings and descenders can cross a clip even when measuredWidth fits.
    const billiardgl::render::ui::ShellViewport native720{
        640, 360, 1280, 720, 192.0f};
    for (const auto& model : screens) {
        for (const float uiScale : {0.85f, 1.0f, 1.35f}) {
            auto scaled = model;
            scaled.uiScale = uiScale;
            const auto frame = builder.build(scaled, native720, atlas);
            if (!frame.draw.ok)
                std::cerr << "native frame failed screen=" <<
                    static_cast<int>(model.screen) << " ui=" << uiScale <<
                    " error=" << frame.draw.errorCode << '\n';
            expect(frame.draw.ok,
                "every native 720p shipping screen builds at every UI scale");
            for (const auto& primitive : frame.draw.primitives) {
                if (primitive.kind != billiardgl::render::ui::
                        UiPrimitiveKind::GlyphQuad)
                    continue;
                const bool fullInk = std::fabs(
                            primitive.glyphInkSourceRect.x -
                            primitive.glyphInkRect.x) <= 0.02f &&
                        std::fabs(primitive.glyphInkSourceRect.y -
                            primitive.glyphInkRect.y) <= 0.02f &&
                        std::fabs(primitive.glyphInkSourceRect.width -
                            primitive.glyphInkRect.width) <= 0.02f &&
                        std::fabs(primitive.glyphInkSourceRect.height -
                            primitive.glyphInkRect.height) <= 0.02f;
                if (!fullInk)
                    std::cerr << "native ink clip screen=" <<
                        static_cast<int>(model.screen) << " ui=" << uiScale <<
                        " cp=" << primitive.codepoint << " source=" <<
                        primitive.glyphInkSourceRect.x << ',' <<
                        primitive.glyphInkSourceRect.y << ',' <<
                        primitive.glyphInkSourceRect.width << ',' <<
                        primitive.glyphInkSourceRect.height << " clipped=" <<
                        primitive.glyphInkRect.x << ',' <<
                        primitive.glyphInkRect.y << ',' <<
                        primitive.glyphInkRect.width << ',' <<
                        primitive.glyphInkRect.height << " text='" <<
                        (primitive.textRunIndex < frame.draw.textRuns.size() ?
                            frame.draw.textRuns[primitive.textRunIndex].text :
                            std::string("<invalid>")) << "'\n";
                expect(fullInk,
                    "native 720p submits every shell glyph's complete ink bounds");
            }
            for (const auto& element : frame.elements) {
                std::map<std::size_t, billiardgl::product_hud::HudRect>
                    runInk;
                for (const auto& primitive : frame.draw.primitives) {
                    if (primitive.kind != billiardgl::render::ui::
                            UiPrimitiveKind::GlyphQuad ||
                        primitive.textRunIndex >= frame.draw.textRuns.size() ||
                        !overlaps(primitive.glyphInkRect, element.rect))
                        continue;
                    const auto found = runInk.find(primitive.textRunIndex);
                    if (found == runInk.end()) {
                        runInk.emplace(primitive.textRunIndex,
                            primitive.glyphInkRect);
                    } else {
                        auto& bounds = found->second;
                        const float right = std::max(bounds.x + bounds.width,
                            primitive.glyphInkRect.x +
                                primitive.glyphInkRect.width);
                        const float bottom = std::max(bounds.y + bounds.height,
                            primitive.glyphInkRect.y +
                                primitive.glyphInkRect.height);
                        bounds.x = std::min(bounds.x,
                            primitive.glyphInkRect.x);
                        bounds.y = std::min(bounds.y,
                            primitive.glyphInkRect.y);
                        bounds.width = right - bounds.x;
                        bounds.height = bottom - bounds.y;
                    }
                }
                for (auto first = runInk.begin(); first != runInk.end();
                        ++first) {
                    for (auto second = std::next(first);
                            second != runInk.end(); ++second) {
                        if (overlaps(first->second, second->second))
                            std::cerr << "native text overlap screen=" <<
                                static_cast<int>(model.screen) << " ui=" <<
                                uiScale << " element=" << element.id <<
                                " first='" << frame.draw.textRuns[
                                    first->first].text << "' second='" <<
                                frame.draw.textRuns[second->first].text <<
                                "'\n";
                        expect(!overlaps(first->second, second->second),
                            "native card text runs have disjoint actual ink bounds");
                    }
                }
            }
            if (model.screen == billiardgl::ProductScreen::TrainingCatalog) {
                const auto firstPage =
                    billiardgl::render::ui::buildShippingScreenLayout(
                        scaled, native720);
                expect(firstPage.ok && firstPage.maximumScrollRow > 0,
                    "compact Training catalog uses an explicit scroll window");
                scaled.inspectorScrollRow = firstPage.maximumScrollRow;
                const auto lastPageLayout =
                    billiardgl::render::ui::buildShippingScreenLayout(
                        scaled, native720);
                const auto lastPage = builder.build(scaled, native720, atlas);
                if (!(lastPageLayout.ok && lastPage.draw.ok &&
                        !lastPageLayout.elements.empty() &&
                        !model.items.empty() &&
                        lastPageLayout.elements.back().id ==
                            model.items.back().id)) {
                    std::cerr << "training last page ui=" << uiScale <<
                        " max=" << firstPage.maximumScrollRow <<
                        " first=" << lastPageLayout.firstVisibleRow <<
                        " visible=" << lastPageLayout.elements.size() <<
                        " items=" << model.items.size() << " last='" <<
                        (lastPageLayout.elements.empty() ? std::string("<none>") :
                            lastPageLayout.elements.back().id) <<
                        "' expected='" << (model.items.empty() ?
                            std::string("<none>") : model.items.back().id) <<
                        "' error='" << lastPage.draw.errorCode << "'\n";
                }
                expect(lastPageLayout.ok && lastPage.draw.ok &&
                    !lastPageLayout.elements.empty() && !model.items.empty() &&
                    lastPageLayout.elements.back().id == model.items.back().id,
                    "scrolling the compact Training catalog exposes its final drill");
            }
        }
    }

    expect(!shellIconTokens.empty(),
        "nine-screen source audit observes emitted shell icon tokens");
    for (const std::string& token : shellIconTokens) {
        billiardgl::render::ui::HudDrawList one;
        one.ok = true;
        billiardgl::render::ui::UiPrimitive icon;
        icon.kind = billiardgl::render::ui::UiPrimitiveKind::Icon;
        icon.rect = {0.0f, 0.0f, 24.0f, 24.0f};
        icon.sourceRect = icon.rect;
        icon.clipRect = icon.rect;
        icon.token = token;
        one.primitives.push_back(icon);
        const auto expanded = billiardgl::render::ui::expandShippingHudIcons(
            one, registry, billiardgl::render::ui::HudPaletteMode::Standard,
            "focused_practice_shell_shipping_tokens_v1");
        expect(expanded.ok && !expanded.draw.triangles.empty() &&
            expanded.draw.primitives.empty(),
            "every emitted ProductShell Icon resolves without opaque fallback");
    }

    fixture.shell.openReplayBrowser();
    const auto cleanReplayDraw = builder.build(fixture.shell.snapshot(),
        {1280, 720, 1280, 720, 96.0f}, atlas);
    validateDraw(cleanReplayDraw);
    expect(std::none_of(cleanReplayDraw.draw.primitives.begin(),
        cleanReplayDraw.draw.primitives.end(), [](const auto& primitive) {
            return primitive.token == "error_cross_octagon" ||
                primitive.token == "error_banner_octagon";
        }), "ordinary replay panels cannot be miscounted as an error shape");
    fixture.shell.focusItem("choose_replay");
    fixture.shell.loadReplayPath("", "x", "y");
    const auto errorDraw = builder.build(fixture.shell.snapshot(),
        {1280, 720, 1280, 720, 96.0f}, atlas);
    validateDraw(errorDraw);
    expect(std::any_of(errorDraw.draw.primitives.begin(),
        errorDraw.draw.primitives.end(), [](const auto& primitive) {
            return primitive.token == "error_cross_octagon";
        }), "visible replay errors use text plus non-color error shape");
}

void testCompact720HelpAndTrainingTextReflow()
{
    const auto atlas = makeAtlas();
    const billiardgl::render::ui::ShellViewport compactRetina{
        640, 360, 1280, 720, 192.0f};
    billiardgl::render::ui::ShippingScreenDrawListBuilder builder;
    const auto runInkFits = [](const auto& draw, std::size_t runIndex,
            float minimumText) {
        if (runIndex >= draw.textRuns.size()) return false;
        const auto& run = draw.textRuns[runIndex];
        if (run.requestedPixelSize < minimumText ||
            run.measuredWidth > run.clipRect.width + 0.02f)
            return false;
        return std::all_of(draw.primitives.begin(), draw.primitives.end(),
            [&](const auto& primitive) {
                if (primitive.kind != billiardgl::render::ui::
                        UiPrimitiveKind::GlyphQuad ||
                    primitive.textRunIndex != runIndex)
                    return true;
                const auto& source = primitive.glyphInkSourceRect;
                const auto& submitted = primitive.glyphInkRect;
                return std::fabs(source.x - submitted.x) <= 0.02f &&
                    std::fabs(source.y - submitted.y) <= 0.02f &&
                    std::fabs(source.width - submitted.width) <= 0.02f &&
                    std::fabs(source.height - submitted.height) <= 0.02f;
            });
    };
    const auto phraseUsesReadableColor = [](const auto& draw,
            const std::string& text) {
        bool foundGlyph = false;
        for (std::size_t index = 0; index < draw.textRuns.size(); ++index) {
            const auto& run = draw.textRuns[index];
            if (run.text.empty() || text.find(run.text) == std::string::npos)
                continue;
            for (const auto& primitive : draw.primitives) {
                if (primitive.kind != billiardgl::render::ui::
                        UiPrimitiveKind::GlyphQuad ||
                    primitive.textRunIndex != index)
                    continue;
                foundGlyph = true;
                // kDisabled.red is 0.42. Both ordinary kText and the read-only
                // kSecondary token remain above this explicit semantic boundary.
                if (primitive.color.red < 0.60f) return false;
            }
        }
        return foundGlyph;
    };

    Fixture helpFixture("billiardgl_help_720_text_reflow.json");
    expect(helpFixture.shell.openHelp().ok,
        "compact Help fixture opens the product screen");
    auto helpModel = helpFixture.shell.snapshot();
    expect(helpModel.subtitle == "Keyboard, mouse, and rules",
        "Help subtitle accurately describes product rules without an approval claim");
    expect(std::all_of(helpModel.items.begin(), helpModel.items.end(),
            [](const auto& item) {
                return item.id == "help_back" ||
                    (!item.enabled && item.readOnlyInformational);
            }),
        "Help reference rows remain non-activatable without using disabled text semantics");
    const auto helpLayout = billiardgl::render::ui::
        buildShippingScreenLayout(helpModel, compactRetina);
    const auto help = builder.build(helpModel, compactRetina, atlas);
    std::set<float> helpColumns;
    for (const auto& element : helpLayout.elements)
        helpColumns.insert(element.rect.x);
    expect(helpLayout.ok && help.draw.ok && helpColumns.size() == 2,
        "720p Help reflows seven cards into two readable columns");
    expect(phraseUsesReadableColor(help.draw, "Aim") &&
        phraseUsesReadableColor(help.draw, "WPA Heyball"),
        "read-only Help reference text is readable without becoming an enabled action");
    for (const std::string expected : {
            std::string("Left mouse release"),
            std::string("Drag / WASD / Wheel")}) {
        const auto found = std::find_if(help.draw.textRuns.begin(),
            help.draw.textRuns.end(), [&](const auto& run) {
                return run.text == expected;
            });
        expect(found != help.draw.textRuns.end() && runInkFits(help.draw,
            static_cast<std::size_t>(std::distance(
                help.draw.textRuns.begin(), found)), helpLayout.minimumText),
            "720p Help retains complete control text with SDF ink margin");
    }

    Fixture trainingFixture("billiardgl_training_720_text_reflow.json");
    expect(trainingFixture.shell.loadTrainingCatalog(
            trainingManifest().string(), allCapabilities()).ok &&
        trainingFixture.shell.startTrainingScenario(
            trainingFixture.training.scenarios().front().id,
            "training-720-text-reflow", 7).ok,
        "compact Training fixture starts an authoritative scenario");
    auto trainingModel = trainingFixture.shell.snapshot();
    trainingModel.manualInspectorPolicy = true;
    trainingModel.inspectorMode = 1;
    trainingModel.subtitle = "launch_straight_draw_intro_v2";
    trainingModel.items = {
        {"training_terminal", "Attempt / score",
            "Attempt active / 0 shots", {}, {}, "rules_shield", false},
        {"trajectory_predicted", "Predicted",
            "X 24.2 cm / Z 0.0 cm", "Rails 0", {},
            "aim_crosshair", false}};
    trainingModel.items.front().readOnlyInformational = true;
    // This two-row text fixture replaces the live action inventory; reset
    // the live focused-action scroll offset to its informational first row.
    trainingModel.inspectorScrollRow = 0;
    const auto trainingLayout = billiardgl::render::ui::
        buildShippingScreenLayout(trainingModel, compactRetina);
    const auto training = builder.build(
        trainingModel, compactRetina, atlas);
    expect(trainingLayout.ok && training.draw.ok &&
        trainingLayout.minimumText >= 28.0f,
        "720p Training fixture retains the >=13 logical-pixel text contract");
    expect(phraseUsesReadableColor(training.draw, "Attempt / score") &&
        phraseUsesReadableColor(training.draw, "Attempt active / 0 shots"),
        "read-only Training result text uses informational rather than disabled color");
    const auto exactWrappedTextFits = [&](const auto& draw,
            float minimumText, const std::string& expected) {
        for (std::size_t first = 0; first < draw.textRuns.size();
                ++first) {
            std::string joined;
            bool fits = true;
            for (std::size_t index = first;
                    index < draw.textRuns.size(); ++index) {
                const auto& run = draw.textRuns[index];
                joined += run.text;
                fits = fits && runInkFits(draw, index, minimumText);
                if (joined == expected) return fits;
                if (joined.size() >= expected.size() ||
                    expected.rfind(joined, 0) != 0)
                    break;
            }
        }
        return false;
    };
    expect(exactWrappedTextFits(training.draw, trainingLayout.minimumText,
            "launch_straight_draw_intro_v2"),
        "720p Training scenario id is complete and unclipped");
    expect(exactWrappedTextFits(training.draw, trainingLayout.minimumText,
            "Attempt / score"),
        "720p Training Attempt label is complete and unclipped");
    expect(exactWrappedTextFits(training.draw, trainingLayout.minimumText,
            "Attempt active / 0 shots"),
        "720p Training Attempt value is complete and unclipped");
    auto scrolledTrainingModel = trainingModel;
    scrolledTrainingModel.inspectorScrollRow = 1;
    const auto scrolledTrainingLayout = billiardgl::render::ui::
        buildShippingScreenLayout(scrolledTrainingModel, compactRetina);
    const auto scrolledTraining = builder.build(
        scrolledTrainingModel, compactRetina, atlas);
    expect(trainingLayout.maximumScrollRow == 1 &&
        scrolledTrainingLayout.ok && scrolledTraining.draw.ok &&
        exactWrappedTextFits(scrolledTraining.draw,
            scrolledTrainingLayout.minimumText,
            "X 24.2 cm / Z 0.0 cm"),
        "720p Training X/Z value is complete and unclipped");

    Fixture catalogFixture("billiardgl_training_all_ids_text_reflow.json");
    expect(catalogFixture.shell.loadTrainingCatalog(
            trainingManifestV2().string(), allCapabilities()).ok &&
        catalogFixture.training.scenarios().size() == 13,
        "complete v2 Training catalog exposes all thirteen product drills");
    const std::string terminalValue =
        "Failed / success_conditions_not_met";
    const std::string terminalDetail =
        "Reason: Ball 1 pocketed; cue moved 18.7cm / max 4.5cm; "
        "Ball 0 rail contacts: 2 / max 0";
    const billiardgl::render::ui::ShellViewport profiles[] = {
        {1280, 720, 1280, 720, 96.0f},
        compactRetina,
        {1280, 720, 2560, 1440, 192.0f},
        {1920, 1080, 1920, 1080, 96.0f},
        {1280, 720, 1920, 1080, 144.0f}};
    for (const auto& scenario : catalogFixture.training.scenarios()) {
        for (const auto& viewport : profiles) {
            for (const float uiScale : {0.85f, 1.0f, 1.35f}) {
                auto complete = trainingModel;
                complete.uiScale = uiScale;
                complete.subtitle = scenario.id;
                complete.items = {{"training_terminal", "Attempt / score",
                    terminalValue, terminalDetail, {}, "rules_shield", false}};
                const auto layout = billiardgl::render::ui::
                    buildShippingScreenLayout(complete, viewport);
                const auto frame = builder.build(complete, viewport, atlas);
                const bool idFits = layout.ok && frame.draw.ok &&
                    exactWrappedTextFits(frame.draw, layout.minimumText,
                        scenario.id);
                const bool labelFits = layout.ok && frame.draw.ok &&
                    exactWrappedTextFits(frame.draw, layout.minimumText,
                        "Attempt / score");
                const bool valueFits = layout.ok && frame.draw.ok &&
                    exactWrappedTextFits(frame.draw, layout.minimumText,
                        terminalValue);
                const bool detailFits = layout.ok && frame.draw.ok &&
                    exactWrappedTextFits(frame.draw, layout.minimumText,
                        terminalDetail);
                if (!idFits || !labelFits || !valueFits || !detailFits) {
                    std::cerr << "compact text diagnostic id=" << scenario.id
                        << " viewport=" << viewport.logicalWidth << 'x'
                        << viewport.logicalHeight << '/' <<
                        viewport.physicalWidth << 'x' <<
                        viewport.physicalHeight << " ui=" << uiScale
                        << " layout=" << layout.errorCode << " draw=" <<
                        frame.draw.errorCode << " fields=" << idFits <<
                        labelFits << valueFits << detailFits << '\n';
                    for (const auto& run : frame.draw.textRuns)
                        std::cerr << "  run='" << run.text << "' y=" <<
                            run.clipRect.y << " h=" << run.clipRect.height <<
                            " w=" << run.measuredWidth << '/' <<
                            run.clipRect.width << '\n';
                }
                expect(idFits && labelFits && valueFits && detailFits,
                    "every v2 drill id and terminal explanation remains complete at every shipping profile");
            }
        }
    }
}

void testBoundedInspectorCorrectionGateAndNegativeFixtures()
{
    Fixture fixture("billiardgl_product_shell_inspector_v3.json");
    expect(fixture.shell.loadTrainingCatalog(
        trainingManifest().string(), allCapabilities()).ok &&
        fixture.shell.startTrainingScenario(
            fixture.training.scenarios().front().id,
            "product-shell-inspector-v3", 77).ok,
        "bounded inspector fixture enters TrainingSession");
    auto model = fixture.shell.snapshot();
    billiardgl::render::ui::ShellViewport fullHd{
        1920, 1080, 1920, 1080, 96.0f};
    fullHd.hasProtectedGameplayRect = true;
    fullHd.protectedGameplayRect = {400.0f, 180.0f, 600.0f, 700.0f};
    const auto layout = billiardgl::render::ui::buildShippingScreenLayout(
        model, fullHd);
    expect(layout.ok && layout.boundedInspector &&
        layout.inspectorDock ==
            billiardgl::render::ui::ShellInspectorDock::Right &&
        layout.inspectorProtectedIntersectionArea == 0.0f,
        "projected protected corridor selects the non-intersecting farther dock");
    const auto valid = billiardgl::render::ui::
        auditBoundedInspectorGeometry(layout, {}, 0.0f);
    expect(valid.ok, "resolved v3 inspector geometry passes the hard audit");
    for (const auto size : {std::pair<int,int>{2048,1536}, {1920,1080}, {1280,720}}) {
        auto blocked = fullHd;
        blocked.logicalWidth = size.first / 2;
        blocked.logicalHeight = size.second / 2;
        blocked.physicalWidth = size.first;
        blocked.physicalHeight = size.second;
        blocked.dpi = 192.0f;
        blocked.protectedGameplayRect = {0, 0, static_cast<float>(size.first),
            static_cast<float>(size.second)};
        const auto collapsed = billiardgl::render::ui::buildShippingScreenLayout(model, blocked);
        expect(collapsed.ok && collapsed.inspectorCollapsed && collapsed.elements.empty() &&
            collapsed.degradationReason == "shell_protected_corridor_unresolvable",
            "both docks obstructed collapses the inspector instead of failing the frame");
        expect(!billiardgl::render::ui::hitTestShippingScreenLayout(collapsed, 80, 160).hit,
            "collapsed controls cannot receive invisible pointer clicks");
    }
    const auto restored = billiardgl::render::ui::buildShippingScreenLayout(model, fullHd);
    auto endedModel = model;
    endedModel.training.state = billiardgl::training::TrainingProductState::AttemptEvaluated;
    auto blockedTerminal = fullHd;
    blockedTerminal.hasProtectedGameplayRect = true;
    blockedTerminal.protectedGameplayRect = {0, 0, 1920, 1080};
    const auto terminalLayout = billiardgl::render::ui::buildShippingScreenLayout(endedModel, blockedTerminal);
    expect(terminalLayout.ok && terminalLayout.inspectorCollapsed &&
        terminalLayout.elements.size() == 1 && terminalLayout.elements[0].id == "retry_training",
        "collapsed terminal inspector retains only the visible retry entry");
    const auto retryRect = terminalLayout.elements[0].rect;
    const auto retryHit = billiardgl::render::ui::hitTestShippingScreenLayout(
        terminalLayout, retryRect.x + retryRect.width / 2, retryRect.y + retryRect.height / 2);
    expect(retryHit.hit && retryHit.enabled && retryHit.id == "retry_training",
        "visible terminal retry has an exact enabled hit region");
    const auto terminalDraw = billiardgl::render::ui::ShippingScreenDrawListBuilder{}.build(
        endedModel, blockedTerminal, makeAtlas());
    expect(terminalDraw.draw.ok && std::any_of(terminalDraw.draw.textRuns.begin(),
        terminalDraw.draw.textRuns.end(), [](const auto& run) { return run.text == "Retry shot"; }),
        "collapsed retry hit target has a rendered readable label");
    expect(restored.ok && !restored.inspectorCollapsed && !restored.elements.empty(),
        "inspector automatically restores when the camera corridor permits it");

    auto tooWide = layout;
    tooWide.inspectorRect.width = 481.0f;
    tooWide.inspectorViewportCoverage = tooWide.inspectorRect.width *
        tooWide.inspectorRect.height / (1920.0f * 1080.0f);
    expect(!billiardgl::render::ui::auditBoundedInspectorGeometry(
            tooWide, {}, 0.0f).ok,
        "one-pixel-over maximum inspector width fails closed");
    auto tooTall = layout;
    tooTall.inspectorRect.height = 781.0f;
    tooTall.inspectorViewportCoverage = tooTall.inspectorRect.width *
        tooTall.inspectorRect.height / (1920.0f * 1080.0f);
    expect(!billiardgl::render::ui::auditBoundedInspectorGeometry(
            tooTall, {}, 0.0f).ok,
        "one-pixel-over maximum inspector height fails closed");
    auto intersects = layout;
    intersects.protectedGameplayRect = intersects.inspectorRect;
    expect(!billiardgl::render::ui::auditBoundedInspectorGeometry(
            intersects, {}, 0.0f).ok,
        "any inspector/protected-corridor intersection fails closed");
    expect(!billiardgl::render::ui::auditBoundedInspectorGeometry(
            layout, {}, 0.01f).ok,
        "any full-frame scrim alpha fails closed");
    const billiardgl::product_hud::HudRect escapedPanel{
        layout.inspectorRect.x - 1.0f, layout.inspectorRect.y,
        10.0f, 10.0f};
    expect(!billiardgl::render::ui::auditBoundedInspectorGeometry(
            layout, {escapedPanel}, 0.0f).ok,
        "a panel escaping the resolved inspector by one pixel fails closed");

    model.uiScale = 1.35f;
    // This content check intentionally inspects the legend page; production
    // keyboard entry now scrolls to its focused action instead.
    model.inspectorScrollRow = 0;
    model.inspectorMode = 1;
    const billiardgl::render::ui::ShellViewport compact{
        1280, 720, 1280, 720, 96.0f};
    const auto atlas = makeAtlas();
    billiardgl::render::ui::ShippingScreenDrawListBuilder builder;
    const auto draw = builder.build(model, compact, atlas);
    expect(draw.draw.ok && std::count_if(draw.draw.textRuns.begin(),
        draw.draw.textRuns.end(), [](const auto& run) {
            return run.text == "Rails 0";
        }) == 2, "720 UI-135 retains exact Rails N text for both legend rows");
    for (const auto& run : draw.draw.textRuns) {
        if (run.measuredWidth > run.clipRect.width + 0.02f)
            std::cerr << "inspector overflow text='" << run.text <<
                "' width=" << run.measuredWidth << " clip=" <<
                run.clipRect.width << '\n';
        expect(run.measuredWidth <= run.clipRect.width + 0.02f,
            "720 UI-135 required inspector text is not clipped or elided");
    }
}

}  // namespace

void testScrolledPanelDrawing()
{
    Fixture fixture("billiardgl_scroll_drawing.json");
    expect(fixture.training.loadPack(trainingManifest().string(), allCapabilities()).ok,
        "scroll fixture loads training catalog");
    expect(fixture.shell.startTrainingScenario(fixture.training.scenarios().front().id,
        "scroll-draw", 0).ok && fixture.shell.focusItem("training_advanced").ok &&
        fixture.shell.navigate(billiardgl::ShellNavigationCommand::Activate).ok,
        "dense scrolling draw fixture starts");
    auto model = fixture.shell.snapshot();
    model.inspectorMode = 1;
    const auto atlas = makeAtlas();
    billiardgl::render::ui::ShippingScreenDrawListBuilder builder;
    for (int backing : {1,2}) {
        const billiardgl::render::ui::ShellViewport viewport{900,430,900*backing,430*backing,96.0f*backing};
        for (std::size_t offset : {std::size_t{0},std::size_t{3},std::size_t{1000}}) {
            model.inspectorScrollRow = offset;
            const auto layout = billiardgl::render::ui::buildShippingScreenLayout(model, viewport);
            const auto draw = builder.build(model, viewport, atlas);
            expect(layout.ok && !layout.inspectorCollapsed && layout.maximumScrollRow > 0 && draw.draw.ok,
                "small manually opened advanced panel renders without layout/text failure");
            for (std::size_t index = 0; index < layout.elements.size(); ++index) {
                const auto& row = layout.elements[index];
                expect(row.id == model.items[layout.firstVisibleRow + index].id &&
                    row.rect.y >= layout.bodyRect.y && row.rect.y + row.rect.height <=
                        layout.bodyRect.y + layout.bodyRect.height + 0.01f,
                    "only visible source rows are drawn inside body");
            }
        }
    }
}

void testProductionManualPanelPolicy()
{
    Fixture fixture("billiardgl_manual_policy.json");
    fixture.shell.setManualInspectorPolicy(true);
    expect(fixture.training.loadPack(trainingManifest().string(), allCapabilities()).ok,
        "manual policy loads training");
    expect(fixture.shell.startTrainingScenario(fixture.training.scenarios().front().id,
        "manual-policy", 0).ok, "manual policy starts training");
    auto model = fixture.shell.snapshot();
    expect(model.inspectorMode == 2 && model.manualInspectorPolicy,
        "production training details start explicitly closed");
    const auto atlas = makeAtlas();
    for (const auto size : {std::pair<int,int>{1920,1080}, {1280,720}, {1024,768}, {900,430}})
    for (float scale : {0.85f,1.35f})
    for (int backing : {1,2}) {
        model.uiScale = scale;
        billiardgl::render::ui::ShellViewport viewport{size.first,size.second,size.first*backing,size.second*backing,96.0f*backing};
        auto closed = billiardgl::render::ui::buildShippingScreenLayout(model, viewport);
        expect(closed.ok && closed.inspectorCollapsed,
            "closed details do not reopen when camera geometry permits");
        auto draw = billiardgl::render::ui::ShippingScreenDrawListBuilder{}.build(model, viewport, atlas);
        expect(draw.draw.ok && std::any_of(draw.draw.textRuns.begin(), draw.draw.textRuns.end(),
            [](const auto& run) { return run.text == "Training panel"; }) &&
            std::any_of(draw.draw.primitives.begin(), draw.draw.primitives.end(),
                [](const auto& primitive) { return primitive.token == "vertical_arrows"; }),
            "persistent explicitly named panel entry is rendered");
        fixture.shell.setInspectorExpanded(true);
        auto openModel = fixture.shell.snapshot();
        openModel.uiScale = scale;
        viewport.hasProtectedGameplayRect = true;
        viewport.protectedGameplayRect = {0,0,static_cast<float>(size.first*backing),static_cast<float>(size.second*backing)};
        const auto open = billiardgl::render::ui::buildShippingScreenLayout(openModel, viewport);
        expect(open.ok && !open.inspectorCollapsed,
            "manual details stay open even if projected trajectory moves under them");
    }
}

void testLiveRackTerminalPresentation()
{
    using namespace billiardgl;
    using namespace billiardgl::rules;
    class Authenticator final : public RefereeAuthorityAuthenticator {
        bool verify(const RefereeAuthenticationInput& input) const override {
            return input.authenticationProof ==
                refereeAuthoritySha256Hex(input.authenticatedBytes);
        }
    } authenticator;
    Fixture fixture("billiardgl_live_rack_terminal.json");
    expect(fixture.shell.startSinglePlayer(9).ok, "terminal fixture starts Match");
    fixture.authority.configureNonphysicalGeneralFoulAuthority(&authenticator);
    const auto commit = [&](NonphysicalGeneralFoulKind kind, const std::string& id) {
        const std::string shot = "runtime.nonphysical_general_foul.shot." +
            std::to_string(fixture.authority.rulesAuthorityRevision());
        for (const auto operation : {RefereeAuthorityOperation::Propose,
                                    RefereeAuthorityOperation::FinalDeclare}) {
            RefereeAuthorityRequest request;
            request.protocolId = kRefereeAuthorityProtocolId;
            request.schemaVersion = 1;
            request.operation = operation;
            request.declarationKind = kNonphysicalGeneralFoulKind;
            request.decisionId = id;
            request.rackIdentity = fixture.authority.nonphysicalGeneralFoulState().rackIdentity;
            const auto& protocol = fixture.authority.nonphysicalGeneralFoulProtocolState();
            request.expectedAuthorityRevision = protocol.authorityRevision;
            request.expectedAuthorityStateHash = protocol.authorityStateHash;
            request.authorityRole = RefereeAuthorityRole::Referee;
            request.authoritySubjectId = "referee.fixture";
            request.credentialId = "credential.fixture";
            request.keyVersion = "v1";
            request.idempotencyKey = id + (operation == RefereeAuthorityOperation::Propose ? ".p" : ".f");
            request.authenticationNonce = request.idempotencyKey;
            request.expiresAtAuthorityTick = 1000;
            request.payloadCanonical = operation == RefereeAuthorityOperation::Propose ?
                canonicalNonphysicalGeneralFoulProposal(kind, 0, shot) :
                canonicalNonphysicalGeneralFoulFinal(kind, 0, id, shot);
            request.authenticationProof = refereeAuthoritySha256Hex(
                canonicalRefereeAuthenticatedBytes(request));
            expect((operation == RefereeAuthorityOperation::Propose ?
                fixture.authority.submitNonphysicalGeneralFoulProposal(request) :
                fixture.authority.submitNonphysicalGeneralFoulFinal(request)).ok,
                "signed fixture event commits through production authority");
        }
    };
    const auto status = [&]() {
        const auto model = fixture.shell.snapshot();
        for (const auto& row : model.items)
            if (row.id == "match_status") return row;
        throw std::runtime_error("missing match status");
    };
    commit(NonphysicalGeneralFoulKind::MobileDeviceDisturbanceGeneralFoul, "shell.mobile.first");
    expect(status().label == "Match in progress", "first violation does not end rack");
    PresentationSnapshot firstPresentation;
    firstPresentation.rules = buildRulesPresentation(fixture.authority);
    firstPresentation.hud.aimMode = true;
    const auto firstHud = product_hud::HudStateReducer::reduce(firstPresentation);
    expect(firstHud.ruleNotices.size() == 1 &&
        firstHud.ruleNotices.front().reasonCode == "mobile_device_disturbance_general_foul" &&
        firstHud.ruleNotices.front().playerText.find("ball in hand") != std::string::npos,
        "first authenticated mobile foul explains its cause and ball in hand");
    expect(fixture.authority.lastAdjudication().reasons.empty(),
        "nonphysical explanation does not invent a physical foul reason");
    commit(NonphysicalGeneralFoulKind::MobileDeviceRepeatLossOfRack, "shell.mobile.repeat");
    expect(fixture.authority.rulesRackTerminalState().active &&
        !fixture.authority.state().gameOver && fixture.authority.matchState().winner == -1,
        "repeat ends rack without inventing match result");
    const auto terminal = status();
    expect(terminal.label == "Rack ended" && terminal.value == "Player 2 wins rack",
        "live Match presents authoritative rack-only winner");
    expect(terminal.detail.find("mobile-device") != std::string::npos,
        "live rack result explains repeat mobile-device violation");
    PresentationSnapshot presentation;
    presentation.rules = buildRulesPresentation(fixture.authority);
    presentation.hud.aimMode = true; // A stale aiming view must not mask terminal feedback.
    const auto hud = product_hud::HudStateReducer::reduce(presentation);
    expect(presentation.rules.rackEnded && !presentation.rules.gameOver &&
        presentation.rules.winner == -1 && presentation.rules.rackWinner == 1,
        "presentation preserves independent rack and match outcomes");
    expect(hud.showFoul && !hud.showPower && hud.ruleNotices.size() == 1 &&
        hud.ruleNotices.front().reasonCode == "mobile_device_repeat_loss_of_rack" &&
        hud.foulResolution.text.find("Player 2") != std::string::npos,
        "persistent HUD explains terminal foul and rack winner without shot-end pulse");
    expect(fixture.shell.startSinglePlayer(10).ok && status().label == "Match in progress",
        "new match clears the prior rack-terminal presentation");
}

int main()
{
    try {
        testScreenFlowAndApprovedProductEntry();
        testFreshMatchRestartAndFailureCleanup();
        testKeyboardOnlyNavigationAndInputContexts();
        testTrainingSettingsAndRebindSnapshots();
        testCameraSettingsPreviewApplyCancel();
        testOptInProductCameraCandidate();
        testReplayPathsAndPlaybackSnapshot();
        testShippingTextAtlasBoundsAndDescenders();
        testShippingDrawListResponsiveCoverage();
        testBoundedInspectorCorrectionGateAndNegativeFixtures();
        testCompact720HelpAndTrainingTextReflow();
        testScrolledPanelDrawing();
        testProductionManualPanelPolicy();
        testLiveRackTerminalPresentation();
        std::cout << "Product shell controller and shipping draw-list checks passed\n";
        return EXIT_SUCCESS;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return EXIT_FAILURE;
    }
}
