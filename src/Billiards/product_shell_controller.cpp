#include "product_shell_controller.h"

#include "automation_json.h"
#include "replay/replay_v1.h"

#include <algorithm>
#include <cctype>
#include <cmath>
#include <fstream>
#include <iomanip>
#include <iterator>
#include <sstream>

namespace billiardgl {
namespace {

ProductShellResult success(
    ProductShellAppRequest request = ProductShellAppRequest::None)
{
    return {true, "", request};
}

ProductShellResult failure(const std::string& code)
{
    return {false, code, ProductShellAppRequest::None};
}

const char* shellDifficultyId(ai::Difficulty value)
{
    switch (value) {
    case ai::Difficulty::Rookie: return "rookie";
    case ai::Difficulty::Club: return "club";
    case ai::Difficulty::Expert: return "expert";
    }
    return "club";
}

const char* shellDifficultyLabel(ai::Difficulty value)
{
    switch (value) {
    case ai::Difficulty::Rookie: return "Rookie";
    case ai::Difficulty::Club: return "Club";
    case ai::Difficulty::Expert: return "Expert";
    }
    return "Club";
}

std::string percent(double value)
{
    std::ostringstream output;
    output << std::fixed << std::setprecision(0) << value * 100.0 << '%';
    return output.str();
}

std::string decimal(double value, int precision = 2)
{
    std::ostringstream output;
    output << std::fixed << std::setprecision(precision) << value;
    return output.str();
}

std::string signedDegrees(double value)
{
    return (value >= 0.0 ? "+" : "") + decimal(value, 0) + " deg";
}

camera::PresentationPreferences cameraPreferences(
    const settings::ProductSettings& settings)
{
    camera::PresentationPreferences result;
    // Preserve settings-v1 storage compatibility while exposing truthful
    // relative controls in the product UI.
    result.fieldOfViewOffsetDegrees = static_cast<float>(
        settings.fieldOfViewDegrees - 55.0);
    result.distanceScale = static_cast<float>(settings.cameraDistanceCm / 180.0);
    result.dynamicTrackingEnabled = settings.dynamicCamera;
    result.impactShakeEnabled = settings.cameraShake;
    return result;
}

std::string compactHash(const std::string& value)
{
    if (value.empty()) return "unavailable";
    if (value.size() <= 24) return value;
    return value.substr(0, 12) + "..." + value.substr(value.size() - 8);
}

const char* onOff(bool value) { return value ? "On" : "Off"; }
const char* muted(bool value) { return value ? "Muted" : "Active"; }

const char* assistName(settings::AssistMode value)
{
    switch (value) {
    case settings::AssistMode::Off: return "Off";
    case settings::AssistMode::Basic: return "Basic";
    case settings::AssistMode::Guided: return "Guided";
    }
    return "Off";
}

std::string trainingObjectiveLabel(
    const training::TrainingScenarioCard& card)
{
    const bool pocketsBall =
        card.successSummary.find("ball_pocketed") != std::string::npos;
    const bool holdsPosition = card.successSummary.find(
        "ball_final_position_in_zone") != std::string::npos;
    const bool limitsShots =
        card.successSummary.find("shot_count_lte") != std::string::npos;
    if (pocketsBall && holdsPosition && limitsShots)
        return "Pocket + position + shot limit";

    const std::size_t conditions = card.successSummary.empty() ? 0 :
        static_cast<std::size_t>(1 + std::count(
            card.successSummary.begin(), card.successSummary.end(), ','));
    return conditions == 1 ? "1 success condition" :
        std::to_string(conditions) + " success conditions";
}

std::string trainingEvaluationLabel(
    const training::TrainingScenarioCard& card)
{
    if (card.successEvaluation == "attempt_end_after_balls_stopped")
        return "Checked after balls stop";
    return "Checked at attempt end";
}

std::string trainingAssistLabel(const std::string& assist)
{
    if (assist.empty()) return {};
    std::string label = assist;
    label.front() = static_cast<char>(std::toupper(
        static_cast<unsigned char>(label.front())));
    return label;
}

std::string trainingResetLabel(const training::TrainingScenarioCard& card)
{
    if (card.resetMode == "atomic_reload_embedded_initial_scenario")
        return "Atomic reset / seed " + std::to_string(card.attemptSeed);
    return "Reset / seed " + std::to_string(card.attemptSeed);
}

const char* subtitleName(settings::SubtitleMode value)
{
    switch (value) {
    case settings::SubtitleMode::Off: return "Off";
    case settings::SubtitleMode::Essential: return "Essential";
    case settings::SubtitleMode::All: return "All";
    }
    return "Off";
}

const char* colorModeName(settings::ColorVisionMode value)
{
    switch (value) {
    case settings::ColorVisionMode::Default: return "Default";
    case settings::ColorVisionMode::Deuteranopia: return "Deuteranopia";
    case settings::ColorVisionMode::Protanopia: return "Protanopia";
    case settings::ColorVisionMode::Tritanopia: return "Tritanopia";
    case settings::ColorVisionMode::HighContrastPatterns:
        return "High contrast patterns";
    }
    return "Default";
}

std::string settingsCategoryName(settings::SettingsCategory category)
{
    switch (category) {
    case settings::SettingsCategory::Gameplay: return "Gameplay";
    case settings::SettingsCategory::Input: return "Input";
    case settings::SettingsCategory::Accessibility: return "Accessibility";
    case settings::SettingsCategory::Audio: return "Audio";
    case settings::SettingsCategory::Display: return "Display";
    }
    return "Settings";
}

ShellItemViewModel item(const std::string& id, const std::string& label,
    const std::string& value, const std::string& detail,
    const std::string& shape, bool enabled = true,
    bool readOnlyInformational = false)
{
    ShellItemViewModel result;
    result.id = id;
    result.label = label;
    result.value = value;
    result.detail = detail;
    result.shapeToken = shape;
    result.enabled = enabled;
    result.readOnlyInformational = readOnlyInformational;
    return result;
}

}  // namespace

ProductShellController::ProductShellController(GameRuntime& authority,
    settings::ProductSettingsRuntime& settingsRuntime,
    ProductInputController& inputController,
    training::TrainingProductController& trainingController,
    replay::ReplayTransportModel& replayTransport,
    ai::PlayerControl* aiPlayer)
    : authority_(authority), settingsRuntime_(settingsRuntime),
      inputController_(inputController), trainingController_(trainingController),
      replayTransport_(replayTransport), aiPlayer_(aiPlayer),
      approvedRules_{{kApprovedRulesetId, "WPA Heyball"}}
{
    inputController_.setSurface(ProductInputSurface::Play);
}

void ProductShellController::syncCameraPresentationPreferences() noexcept
{
    const settings::ProductSettings& effective =
        settingsRuntime_.menu().isOpen() ? settingsRuntime_.menu().preview() :
            settingsRuntime_.current();
    camera::PresentationPreferences preferences =
        cameraPreferences(effective);
    preferences.m4CameraCandidate = m4CameraCandidate_;
    authority_.setCameraPresentationPreferences(preferences);
    replayTransport_.setCameraPresentationPreferences(preferences);
}

ProductShellResult ProductShellController::openMainMenu()
{
    rememberTrainingCompletion();
    clearError();
    setScreen(ProductScreen::MainMenu);
    return success();
}

ProductShellResult ProductShellController::openSinglePlayerSetup()
{
    clearError();
    setScreen(ProductScreen::SinglePlayerSetup);
    return success();
}

ProductShellResult ProductShellController::selectDifficulty(
    ai::Difficulty difficulty)
{
    if (difficulty != ai::Difficulty::Rookie &&
        difficulty != ai::Difficulty::Club &&
        difficulty != ai::Difficulty::Expert) {
        setError("shell_ai_difficulty_invalid");
        return failure(errorCode_);
    }
    difficulty_ = difficulty;
    clearError();
    return success();
}

ProductShellResult ProductShellController::startSinglePlayer(
    std::uint64_t seed)
{
    if (aiPlayer_ == nullptr) {
        setError("shell_ai_unavailable");
        return failure(errorCode_);
    }
    // A pending worker cannot be cancelled through PlayerControl. Reject the
    // restart before touching training, shell, runtime or rack authority so a
    // busy AI preserves the complete current product session atomically.
    if (aiPlayer_->state().thinking) {
        setError("ai_busy");
        return failure(errorCode_);
    }
    (void)trainingController_.abandonAttempt();
    inputController_.setAiTurnActive(false);
    auto rollback = [&](const std::string& error) {
        authority_.reset();
        (void)authority_.setRulesMode(RulesMode::ChineseEightBallV1);
        ai::PlayerConfiguration disabled;
        disabled.enabled = false;
        const ActionResult aiRollback =
            aiPlayer_->configure(authority_, disabled);
        inputController_.setAiTurnActive(false);
        setScreen(ProductScreen::SinglePlayerSetup);
        setError(aiRollback.ok ? error : "shell_ai_rollback_failed");
        return failure(errorCode_);
    };
    authority_.reset();
    const ActionResult rulesResult = authority_.setRulesMode(
        RulesMode::ChineseEightBallV1);
    if (!rulesResult.ok) return rollback(rulesResult.errorCode);

    // Product Match visibility is downstream of one authoritative Core-E
    // commit. The fixed seed completes only the unspecified numbered-ball
    // order; OfficialRackContract keeps that completion explicitly
    // non-normative while enforcing every WPA II §4 geometry/group rule.
    rules::RerackFoundationParameters rackParameters;
    rackParameters.deterministicSeed = rules::kOfficialRackDefaultSeed;
    rackParameters.breakerPlayerId = 0;
    const rules::RackFoundationPlan rack =
        authority_.stageOfficialRerack(rackParameters);
    if (!rack.ok) return rollback(rack.errorCode);
    const rules::RackFoundationResult rackCommit =
        authority_.commitRackFoundation(
            rack, "product.match.start.official-rack-v1");
    if (!rackCommit.ok) return rollback(rackCommit.errorCode);

    ai::PlayerConfiguration configuration;
    configuration.enabled = true;
    configuration.player = 1;
    configuration.difficulty = difficulty_;
    configuration.deterministicSeed = seed;
    const ActionResult configured = aiPlayer_->configure(authority_, configuration);
    if (!configured.ok) {
        // A failed external AI configuration cannot retain the committed rack,
        // its receipt/action, a half-configured AI, or Match visibility.
        return rollback(configured.errorCode);
    }
    clearError();
    setScreen(ProductScreen::Match);
    return success();
}

ProductShellResult ProductShellController::loadTrainingCatalog(
    const std::string& manifestPath,
    const std::set<std::string>& capabilities)
{
    const training::TrainingProductResult result =
        trainingController_.loadPack(manifestPath, capabilities);
    if (!result.ok) {
        setError(result.errorCode);
        setScreen(ProductScreen::TrainingCatalog);
        return failure(errorCode_);
    }
    clearError();
    setScreen(ProductScreen::TrainingCatalog);
    return success();
}

void ProductShellController::rememberTrainingCompletion()
{
    const auto state = trainingController_.snapshot();
    if (!state.evaluation.succeeded || !state.evaluation.scoreEligible || state.scenarioFileSha256.empty()) return;
    auto& best = completedTrainingScores_[state.scenarioFileSha256];
    best = std::max(best, state.evaluation.score);
}

ProductShellResult ProductShellController::startTrainingScenario(
    const std::string& scenarioId, const std::string& executableVersion,
    std::uint32_t seed)
{
    rememberTrainingCompletion();
    const training::TrainingProductResult result =
        trainingController_.startScenario(scenarioId, executableVersion, seed);
    if (!result.ok) {
        setError(result.errorCode);
        return failure(errorCode_);
    }
    clearError();
    replayReturnsToTraining_ = false;
    trajectoryAdvancedExpanded_ = false;
    showTrainingComparison_ = false;
    setScreen(ProductScreen::TrainingSession);
    return success();
}

ProductShellResult ProductShellController::openReplayBrowser()
{
    replayReturnsToTraining_ = false;
    clearError();
    setScreen(ProductScreen::ReplayBrowser);
    return success();
}

ProductShellResult ProductShellController::loadReplayPath(
    const std::string& path, const std::string& executableVersion,
    const std::string& contentSha256)
{
    return loadReplayPath(path, executableVersion, contentSha256,
        authority_.physicsProfile());
}

ProductShellResult ProductShellController::loadReplayPath(
    const std::string& path, const std::string& executableVersion,
    const std::string& contentSha256, const PhysicsProfile& replayProfile)
{
    replayReturnsToTraining_ = false;
    setScreen(ProductScreen::ReplayBrowser);
    if (path.empty()) {
        setError("shell_replay_path_empty");
        return failure(errorCode_);
    }
    std::ifstream input(path, std::ios::binary);
    if (!input) {
        setError("shell_replay_read_failed");
        return failure(errorCode_);
    }
    const std::string bytes((std::istreambuf_iterator<char>(input)),
        std::istreambuf_iterator<char>());
    const json::ParseResult decoded = json::parse(bytes);
    if (!decoded.ok) {
        setError("shell_replay_json_invalid");
        return failure(errorCode_);
    }
    replay::Document document;
    const replay::ValidationResult parsed = replay::parse(decoded.value, document);
    if (!parsed.ok) {
        setError(parsed.errorCode);
        return failure(errorCode_);
    }
    const replay::ValidationResult loaded = replayTransport_.load(document,
        replayProfile, executableVersion, contentSha256);
    if (!loaded.ok) {
        setError(loaded.errorCode);
        return failure(errorCode_);
    }
    replayPath_ = path;
    clearError();
    setScreen(ProductScreen::ReplayPlayback);
    return success();
}

ProductShellResult ProductShellController::advanceReplayPresentedFrame()
{
    if (screen_ != ProductScreen::ReplayPlayback)
        return failure("shell_replay_playback_inactive");
    const replay::ValidationResult advanced =
        replayTransport_.advancePresentedFrame();
    if (!advanced.ok) {
        setError(advanced.errorCode);
        return failure(errorCode_);
    }
    clearError();
    return success();
}

ProductShellResult ProductShellController::advanceReplayPresentedFrame(
    double elapsedSeconds)
{
    if (screen_ != ProductScreen::ReplayPlayback)
        return failure("shell_replay_playback_inactive");
    const replay::ValidationResult advanced =
        replayTransport_.advancePresentedFrame(elapsedSeconds);
    if (!advanced.ok) {
        setError(advanced.errorCode);
        return failure(advanced.errorCode);
    }
    clearError();
    return success();
}

ProductShellResult ProductShellController::loadTrainingComparisonReplayPath(
    const std::string& path, const std::string& executableVersion,
    const std::string& contentSha256)
{
    if (screen_ != ProductScreen::TrainingSession) {
        setError("shell_training_comparison_requires_session");
        return failure(errorCode_);
    }
    if (path.empty()) {
        setError("shell_replay_path_empty");
        return failure(errorCode_);
    }
    std::ifstream input(path, std::ios::binary);
    if (!input) {
        setError("shell_replay_read_failed");
        return failure(errorCode_);
    }
    const std::string bytes((std::istreambuf_iterator<char>(input)),
        std::istreambuf_iterator<char>());
    const json::ParseResult decoded = json::parse(bytes);
    if (!decoded.ok) {
        setError("shell_replay_json_invalid");
        return failure(errorCode_);
    }
    replay::Document document;
    const replay::ValidationResult parsed = replay::parse(decoded.value, document);
    if (!parsed.ok) {
        setError(parsed.errorCode);
        return failure(errorCode_);
    }
    const replay::ValidationResult loaded = replayTransport_.load(document,
        authority_.physicsProfile(), executableVersion, contentSha256);
    if (!loaded.ok) {
        setError(loaded.errorCode);
        return failure(errorCode_);
    }
    while (!replayTransport_.snapshot().finished) {
        const replay::ValidationResult stepped = replayTransport_.stepTick();
        if (!stepped.ok) {
            setError(stepped.errorCode);
            return failure(errorCode_);
        }
    }
    showTrainingComparison_ = true;
    clearError();
    return success();
}

ProductShellResult ProductShellController::watchTrainingReferenceReplay()
{
    if (screen_ != ProductScreen::TrainingSession) {
        setError("shell_training_reference_requires_session");
        return failure(errorCode_);
    }
    const training::TrainingProductResult loaded =
        trainingController_.loadReferenceReplay(replayTransport_);
    if (!loaded.ok) {
        setError(loaded.errorCode);
        return failure(errorCode_);
    }
    replayReturnsToTraining_ = true;
    showTrainingComparison_ = true;
    clearError();
    setScreen(ProductScreen::ReplayPlayback);
    return success();
}

ProductShellResult ProductShellController::openSettings()
{
    clearError();
    returnScreen_ = screen_;
    if (!settingsRuntime_.menu().isOpen()) settingsRuntime_.openMenu();
    syncCameraPresentationPreferences();
    setScreen(ProductScreen::Settings);
    return success();
}

ProductShellResult ProductShellController::openHelp()
{
    clearError();
    returnScreen_ = screen_;
    setScreen(ProductScreen::Help);
    return success();
}

ProductShellResult ProductShellController::beginRebind(
    const std::string& action)
{
    const ProductInputResult result = inputController_.beginRebind(action);
    if (!result.ok) {
        setError(result.reasonCode);
        return failure(errorCode_);
    }
    clearError();
    return success();
}

ProductShellResult ProductShellController::dispatchProductInput(
    const NormalizedInputEvent& event)
{
    rememberTrainingCompletion();
    const bool settingsWasOpen = settingsRuntime_.menu().isOpen();
    const ProductInputResult result = inputController_.process(event);
    if (settingsWasOpen || settingsRuntime_.menu().isOpen())
        syncCameraPresentationPreferences();
    if (!result.ok) {
        setError(result.reasonCode);
        auto rejected = failure(errorCode_);
        rejected.action = result.action;
        return rejected;
    }
    clearError();
    if (result.action == "open_settings") {
        if (settingsRuntime_.menu().isOpen()) {
            returnScreen_ = screen_;
            setScreen(ProductScreen::Settings);
        } else {
            setScreen(returnScreen_);
        }
    } else if (result.action == "settings_apply" &&
        !settingsRuntime_.menu().isOpen()) {
        setScreen(returnScreen_);
    } else if (result.action == "quit" &&
        screen_ == ProductScreen::Settings &&
        !settingsRuntime_.menu().isOpen()) {
        // Escape is a modal cancel while Settings is open, not an app quit.
        setScreen(returnScreen_);
    } else if (result.action == "toggle_help" &&
        screen_ != ProductScreen::Settings) {
        if (screen_ == ProductScreen::Help) setScreen(returnScreen_);
        else {
            returnScreen_ = screen_;
            setScreen(ProductScreen::Help);
        }
    }
    auto dispatched = success(result.appRequest == ProductAppRequest::Quit ?
        ProductShellAppRequest::Quit : ProductShellAppRequest::None);
    dispatched.action = result.action;
    return dispatched;
}

ProductShellResult ProductShellController::navigate(
    ShellNavigationCommand command)
{
    if (screen_ == ProductScreen::Settings) {
        if (command == ShellNavigationCommand::Back) return back();
        settings::MenuCommand menuCommand = settings::MenuCommand::Toggle;
        if (command == ShellNavigationCommand::FocusNext)
            menuCommand = settings::MenuCommand::FocusNext;
        else if (command == ShellNavigationCommand::FocusPrevious)
            menuCommand = settings::MenuCommand::FocusPrevious;
        const settings::MenuResult result =
            settingsRuntime_.handleMenu(menuCommand);
        syncCameraPresentationPreferences();
        if (!result.ok) {
            setError(result.errorCode);
            return failure(errorCode_);
        }
        clearError();
        return success();
    }
    if (command == ShellNavigationCommand::Back) return back();
    std::vector<ShellItemViewModel> items = itemsForScreen();
    const auto firstEnabled = std::find_if(items.begin(), items.end(),
        [](const ShellItemViewModel& value) { return value.enabled; });
    if (firstEnabled == items.end()) return failure("shell_no_focusable_item");
    if (focusIndex_ >= items.size() || !items[focusIndex_].enabled)
        focusIndex_ = static_cast<std::size_t>(
            std::distance(items.begin(), firstEnabled));
    if (command == ShellNavigationCommand::FocusNext) {
        const auto next = std::find_if(
            items.begin() + static_cast<std::ptrdiff_t>(focusIndex_ + 1),
            items.end(),
            [](const ShellItemViewModel& value) { return value.enabled; });
        if (next != items.end())
            focusIndex_ = static_cast<std::size_t>(
                std::distance(items.begin(), next));
        if (screen_ == ProductScreen::TrainingSession || screen_ == ProductScreen::ReplayPlayback)
            inspectorScrollRow_ = focusIndex_;
        return success();
    }
    if (command == ShellNavigationCommand::FocusPrevious) {
        auto previous = items.begin() + static_cast<std::ptrdiff_t>(focusIndex_);
        while (previous != items.begin()) {
            --previous;
            if (previous->enabled) {
                focusIndex_ = static_cast<std::size_t>(
                    std::distance(items.begin(), previous));
                break;
            }
        }
        if (screen_ == ProductScreen::TrainingSession || screen_ == ProductScreen::ReplayPlayback)
            inspectorScrollRow_ = focusIndex_;
        return success();
    }
    return activateFocused();
}

ProductShellResult ProductShellController::focusItem(const std::string& id)
{
    if (screen_ == ProductScreen::Settings) {
        const settings::MenuResult result = settingsRuntime_.menu().focusItem(id);
        if (!result.ok) {
            setError(result.errorCode);
            return failure(errorCode_);
        }
        clearError();
        return success();
    }
    const std::vector<ShellItemViewModel> items = itemsForScreen();
    const auto found = std::find_if(items.begin(), items.end(),
        [&](const ShellItemViewModel& value) {
            return value.id == id && value.enabled;
        });
    if (found == items.end()) return failure("shell_item_unavailable");
    focusIndex_ = static_cast<std::size_t>(std::distance(items.begin(), found));
    clearError();
    return success();
}

ProductShellResult ProductShellController::activateFocused()
{
    const std::vector<ShellItemViewModel> items = itemsForScreen();
    if (items.empty() || focusIndex_ >= items.size() ||
        !items[focusIndex_].enabled)
        return failure("shell_item_unavailable");
    const std::string id = items[focusIndex_].id;
    if ((screen_ == ProductScreen::TrainingSession ||
            screen_ == ProductScreen::ReplayPlayback) &&
            id == "trajectory_inset_toggle") {
        trajectoryInsetEnabled_ = !trajectoryInsetEnabled_;
        clearError();
        return success();
    }
    if (screen_ == ProductScreen::MainMenu) {
        if (id == "single_player") return openSinglePlayerSetup();
        if (id == "training") {
            setScreen(ProductScreen::TrainingCatalog);
            return success();
        }
        if (id == "replays") return openReplayBrowser();
        if (id == "settings") return openSettings();
        if (id == "help") return openHelp();
        if (id == "quit") return success(ProductShellAppRequest::Quit);
    } else if (screen_ == ProductScreen::SinglePlayerSetup) {
        if (id == "rookie") return selectDifficulty(ai::Difficulty::Rookie);
        if (id == "club") return selectDifficulty(ai::Difficulty::Club);
        if (id == "expert") return selectDifficulty(ai::Difficulty::Expert);
        if (id == "start_match") return startSinglePlayer();
    } else if (screen_ == ProductScreen::Match) {
        if (id == "settings") return openSettings();
        if (id == "help") return openHelp();
        if (id == "save_match_replay") {
            if (!authority_.state().gameOver &&
                !authority_.rulesRackTerminalState().active) {
                setError("match_replay_single_rack_not_terminal");
                return failure(errorCode_);
            }
            clearError();
            return success(ProductShellAppRequest::SaveMatchReplayFile);
        }
    } else if (screen_ == ProductScreen::TrainingCatalog) {
        return startTrainingScenario(id, "product-shell-v1", 1);
    } else if (screen_ == ProductScreen::TrainingSession) {
        if (id == "training_advanced") {
            trajectoryAdvancedExpanded_ = !trajectoryAdvancedExpanded_;
            return success();
        }
        if (id == "preview_training") {
            const training::TrainingProductResult result =
                trainingController_.refreshPreview();
            if (!result.ok) {
                setError(result.errorCode);
                return failure(errorCode_);
            }
            clearError();
            return success();
        }
        if (id == "prepare_training_shot") {
            const auto result = trainingController_.prepareTeachingShot();
            if (!result.ok) { setError(result.errorCode); return failure(errorCode_); }
            clearError();
            return success();
        }
        if (id == "retry_training") {
            rememberTrainingCompletion();
            const training::TrainingProductResult result =
                trainingController_.retryAttempt();
            if (!result.ok) {
                setError(result.errorCode);
                return failure(errorCode_);
            }
            clearError();
            return success();
        }
        if (id == "reset_training") {
            rememberTrainingCompletion();
            const training::TrainingProductResult result =
                trainingController_.resetAttempt();
            if (!result.ok) {
                setError(result.errorCode);
                return failure(errorCode_);
            }
            clearError();
            return success();
        }
        if (id == "save_training_replay") {
            if (!trainingController_.snapshot().replayReady) {
                setError("training_replay_not_ready");
                return failure(errorCode_);
            }
            clearError();
            return success(ProductShellAppRequest::SaveTrainingReplayFile);
        }
        if (id == "watch_training_reference")
            return watchTrainingReferenceReplay();
        if (id == "next_training") {
            if (!trainingController_.snapshot().evaluation.succeeded)
                return failure("training_goal_not_completed");
            const auto& cards = trainingController_.scenarios();
            const auto current = trainingController_.snapshot().scenarioId;
            for (std::size_t i = 0; i + 1 < cards.size(); ++i)
                if (cards[i].id == current) return startTrainingScenario(cards[i + 1].id,
                    trainingController_.snapshot().executableVersion, cards[i + 1].attemptSeed);
            return failure("training_next_unavailable");
        }
        if (id == "training_catalog") {
            rememberTrainingCompletion();
            (void)trainingController_.abandonAttempt();
            trajectoryAdvancedExpanded_ = false;
            setScreen(ProductScreen::TrainingCatalog);
            return success();
        }
    } else if (screen_ == ProductScreen::ReplayBrowser) {
        if (id == "choose_replay")
            return success(ProductShellAppRequest::ChooseReplayFile);
    } else if (screen_ == ProductScreen::ReplayPlayback) {
        if (id == "replay_camera_reset") {
            const auto result = replayTransport_.resetCamera();
            return result.ok ? success() : failure(result.errorCode);
        }
        const NormalizedInputEvent commands[] = {
            {"keyboard", "Space", "press", 0.0, 0.0},
            {"keyboard", ".", "press", 0.0, 0.0},
            {"keyboard", "-", "press", 0.0, 0.0},
            {"keyboard", "+", "press", 0.0, 0.0},
            {"keyboard", "C", "press", 0.0, 0.0},
            {"keyboard", "PageUp", "press", 0.0, 0.0},
            {"keyboard", "PageDown", "press", 0.0, 0.0}};
        const char* ids[] = {"replay_pause", "replay_step_tick", "replay_slower", "replay_faster",
            "replay_camera", "replay_previous", "replay_next"};
        for (std::size_t index = 0; index < 7; ++index)
            if (id == ids[index]) return dispatchProductInput(commands[index]);
        if (id == "replay_browser") return openReplayBrowser();
    } else if (screen_ == ProductScreen::Help) {
        return back();
    }
    return failure("shell_action_unsupported");
}

ProductShellResult ProductShellController::back()
{
    rememberTrainingCompletion();
    clearError();
    if (screen_ == ProductScreen::Settings) {
        if (settingsRuntime_.menu().isOpen()) {
            const settings::MenuResult result = settingsRuntime_.handleMenu(
                settings::MenuCommand::Cancel);
            if (!result.ok) return failure(result.errorCode);
            syncCameraPresentationPreferences();
        }
        setScreen(returnScreen_);
        return success();
    }
    if (screen_ == ProductScreen::Help) {
        setScreen(returnScreen_);
        return success();
    }
    if (screen_ == ProductScreen::TrainingSession) {
        (void)trainingController_.abandonAttempt();
        trajectoryAdvancedExpanded_ = false;
        setScreen(ProductScreen::TrainingCatalog);
        return success();
    }
    if (screen_ == ProductScreen::ReplayPlayback) {
        if (replayReturnsToTraining_) {
            replayReturnsToTraining_ = false;
            setScreen(ProductScreen::TrainingSession);
            return success();
        }
        return openReplayBrowser();
    }
    if (screen_ != ProductScreen::MainMenu) return openMainMenu();
    return success(ProductShellAppRequest::Quit);
}

void ProductShellController::setScreen(ProductScreen screen)
{
    if (screen != screen_) {
        inspectorMode_ = manualInspectorPolicy_ &&
            (screen == ProductScreen::TrainingSession || screen == ProductScreen::ReplayPlayback) ? 2 : 0;
        inspectorScrollRow_ = 0;
    }
    if (screen_ == ProductScreen::ReplayPlayback &&
        screen != ProductScreen::ReplayPlayback &&
        replayTransport_.snapshot().loaded)
        (void)replayTransport_.setPaused(true);
    screen_ = screen;
    ProductInputSurface surface = inputController_.surface();
    if (screen == ProductScreen::TrainingCatalog ||
        screen == ProductScreen::TrainingSession)
        surface = ProductInputSurface::Training;
    else if (screen == ProductScreen::ReplayBrowser ||
        screen == ProductScreen::ReplayPlayback)
        surface = ProductInputSurface::Replay;
    else if (screen != ProductScreen::Settings && screen != ProductScreen::Help)
        surface = ProductInputSurface::Play;
    inputController_.setSurface(surface);

    const std::vector<ShellItemViewModel> items = itemsForScreen();
    const auto firstEnabled = std::find_if(items.begin(), items.end(),
        [](const ShellItemViewModel& value) { return value.enabled; });
    focusIndex_ = firstEnabled == items.end() ? 0 :
        static_cast<std::size_t>(std::distance(items.begin(), firstEnabled));
    if (screen_ == ProductScreen::TrainingSession || screen_ == ProductScreen::ReplayPlayback)
        inspectorScrollRow_ = focusIndex_;
}

void ProductShellController::setError(const std::string& code)
{
    errorCode_ = code.empty() ? "shell_unknown_error" : code;
}

void ProductShellController::clearError()
{
    errorCode_.clear();
}

std::vector<ShellItemViewModel> ProductShellController::settingsItems() const
{
    const settings::ProductSettings& live = settingsRuntime_.current();
    const settings::ProductSettings& value = settingsRuntime_.menu().preview();
    const settings::SettingsCategory category = settingsRuntime_.menu().category();
    std::vector<ShellItemViewModel> items;
    const auto setting = [&](const char* id, const char* label,
            const std::string& preview, const std::string& saved,
            const char* shape) {
        items.push_back(item(id, label, preview,
            preview == saved ? "Saved value" : "Saved: " + saved, shape));
    };
    if (category == settings::SettingsCategory::Gameplay) {
        setting("assist_level", "Assist level", assistName(value.assist),
            assistName(live.assist), "assist_rings");
    } else if (category == settings::SettingsCategory::Input) {
        setting("mouse_sensitivity", "Mouse sensitivity",
            decimal(value.mouseSensitivity), decimal(live.mouseSensitivity),
            "mouse_axis");
        setting("invert_mouse_y", "Invert mouse Y", onOff(value.invertMouseY),
            onOff(live.invertMouseY), "vertical_arrows");
        items.push_back(item("rebind_actions", "Rebind controls",
            settingsRuntime_.menu().waitingForBinding() ?
                "Press a new control" : std::to_string(value.bindings.size()) +
                    " bindings",
            "Context conflicts are blocked", "keyboard_outline"));
    } else if (category == settings::SettingsCategory::Accessibility) {
        setting("ui_scale", "UI scale", percent(value.uiScale),
            percent(live.uiScale), "text_scale");
        setting("color_vision_mode", "Color vision mode",
            colorModeName(value.colorVisionMode),
            colorModeName(live.colorVisionMode), "pattern_circle");
        setting("subtitles", "Subtitles", subtitleName(value.subtitles),
            subtitleName(live.subtitles), "caption_box");
    } else if (category == settings::SettingsCategory::Audio) {
        setting("music_volume", "Music volume", percent(value.musicVolume),
            percent(live.musicVolume), "music_note");
        setting("ambience_volume", "Venue ambience",
            percent(value.ambienceVolume), percent(live.ambienceVolume),
            "venue_waves");
        setting("effects_volume", "Effects volume", percent(value.effectsVolume),
            percent(live.effectsVolume), "impact_burst");
        setting("ui_volume", "UI volume", percent(value.uiVolume),
            percent(live.uiVolume), "ui_click");
        setting("music_muted", "Music bus", muted(value.musicMuted),
            muted(live.musicMuted), "music_note");
        setting("ambience_muted", "Ambience bus", muted(value.ambienceMuted),
            muted(live.ambienceMuted), "venue_waves");
        setting("effects_muted", "Effects bus", muted(value.effectsMuted),
            muted(live.effectsMuted), "impact_burst");
        setting("ui_muted", "UI bus", muted(value.uiMuted),
            muted(live.uiMuted), "ui_click");
    } else if (category == settings::SettingsCategory::Display) {
        const std::string fovPreview = signedDegrees(
            value.fieldOfViewDegrees - 55.0);
        const std::string fovSaved = signedDegrees(
            live.fieldOfViewDegrees - 55.0);
        items.push_back(item("field_of_view", "FOV adjustment", fovPreview,
            fovPreview == fovSaved ? "Relative to each authored view" :
                "Saved: " + fovSaved + "; relative to authored view",
            "camera_frame"));
        const std::string distancePreview = percent(
            value.cameraDistanceCm / 180.0);
        const std::string distanceSaved = percent(
            live.cameraDistanceCm / 180.0);
        items.push_back(item("camera_distance", "Camera distance scale",
            distancePreview,
            distancePreview == distanceSaved ?
                "Authored distance; semantic safety limits apply" :
                "Saved: " + distanceSaved + "; semantic limits apply",
            "distance_arrow"));
        setting("bloom", "Bloom", onOff(value.bloom), onOff(live.bloom),
            "glow_circle");
        items.push_back(item("depth_of_field", "Depth of field", onOff(value.depthOfField),
            value.depthOfField == live.depthOfField ? "High quality replay only" :
                std::string("Saved: ") + onOff(live.depthOfField) + "; High quality replay only",
            "focus_brackets"));
        setting("dynamic_camera", "Dynamic camera", onOff(value.dynamicCamera),
            onOff(live.dynamicCamera), "camera_motion");
        setting("camera_shake", "Camera shake", onOff(value.cameraShake),
            onOff(live.cameraShake), "shake_lines");
    }
    const int focus = settingsRuntime_.menu().focusIndex();
    if (focus >= 0 && static_cast<std::size_t>(focus) < items.size())
        items[static_cast<std::size_t>(focus)].focused = true;
    return items;
}

std::vector<ShellItemViewModel> ProductShellController::itemsForScreen() const
{
    std::vector<ShellItemViewModel> items;
    if (screen_ == ProductScreen::MainMenu) {
        const training::TrainingProductSnapshot training =
            trainingController_.snapshot();
        const bool trainingReady =
            training.state != training::TrainingProductState::Empty &&
            training.scenarioCount > 0;
        items = {
            item("single_player", "Single player", "WPA Heyball",
                "Play against the AI", "single_player_badge"),
            item("training", "Training", trainingReady ?
                    std::to_string(training.scenarioCount) + " launch drills" :
                    "Pack not loaded",
                trainingReady ? "Practice with authoritative scenarios" :
                    "Load a validated training pack to continue",
                "training_target", trainingReady),
            item("replays", "Replays", "Replay v1",
                "Open a validated local replay", "replay_triangle"),
            item("settings", "Settings", "Input and accessibility",
                "Preview, apply, or rebind", "settings_sliders"),
            item("help", "Help", "Controls and rules",
                "Keyboard and mouse reference", "help_circle"),
            item("quit", "Quit", "Return to desktop", "", "exit_door")};
    } else if (screen_ == ProductScreen::SinglePlayerSetup) {
        items.push_back(item(kApprovedRulesetId, "Rules", "WPA Heyball",
            "Versioned single-rack rules", "rules_shield", false));
        for (ai::Difficulty option : {ai::Difficulty::Rookie,
                ai::Difficulty::Club, ai::Difficulty::Expert}) {
            ShellItemViewModel difficulty = item(shellDifficultyId(option),
                shellDifficultyLabel(option), option == ai::Difficulty::Rookie ?
                    "Learning pace" : option == ai::Difficulty::Club ?
                    "Balanced routes" : "Runout pressure",
                "No hidden-state access", "ai_diamond");
            difficulty.selected = option == difficulty_;
            items.push_back(difficulty);
        }
        items.push_back(item("start_match", "Start match", "Player vs AI",
            "WPA Heyball", "play_triangle", aiPlayer_ != nullptr));
    } else if (screen_ == ProductScreen::Match) {
        const auto& terminal = authority_.rulesRackTerminalState();
        const bool rackTerminal=authority_.state().gameOver||
            terminal.active;
        const int winner = terminal.active ? terminal.winningPlayer :
            authority_.matchState().winner;
        const std::string statusValue = rackTerminal ?
            (winner >= 0 ? "Player " + std::to_string(winner + 1) +
                " wins rack" : "Rack complete") : "WPA Heyball";
        const std::string statusDetail = terminal.active ?
            (terminal.reason == RulesRackTerminalReason::MobileDeviceRepeatViolation ?
                "Repeat mobile-device violation; this rack has ended" :
                "This rack has ended") :
            (rackTerminal ? "Final rack result" :
                std::string("AI: ") + shellDifficultyLabel(difficulty_));
        items = {
            item("match_status", rackTerminal ? "Rack ended" : "Match in progress",
                statusValue, statusDetail,
                "rules_shield", false),
            item("save_match_replay", "Save rack replay",
                rackTerminal ? "F9 / single rack complete" :
                    "Recording current rack",
                rackTerminal ? "Choose a local destination" :
                    "Available when this rack ends; cross-rack replay is unsupported",
                "replay_file",rackTerminal),
            item("settings", "Settings", "F10", "Open product settings",
                "settings_sliders"),
            item("help", "Help", "H", "Show controls", "help_circle")};
    } else if (screen_ == ProductScreen::TrainingCatalog) {
        for (const training::TrainingScenarioCard& card :
                trainingController_.scenarios()) {
            const std::string title = card.titleEn.empty() ? card.id :
                card.titleEn;
            const std::string assist = card.recommendedAssist.empty() ?
                card.category : card.category + " / " +
                    card.recommendedAssist + " assist";
            const std::string detail = card.descriptionEn.empty() ?
                (card.scoreEnabled ? "Scored drill" : "Practice only") :
                card.descriptionEn;
            items.push_back(item(card.id, title, assist, detail,
                "training_target", card.missingCapabilities.empty()));
            const auto completed = completedTrainingScores_.find(card.fileSha256);
            if (completed != completedTrainingScores_.end())
                items.back().value = "Completed / best " + std::to_string(completed->second);
        }
        if (items.empty()) items.push_back(item("training_empty",
            "No training catalog loaded", "Load the launch pack",
            "No drill is being claimed available", "empty_box", false));
    } else if (screen_ == ProductScreen::TrainingSession) {
        const training::TrainingProductSnapshot state =
            trainingController_.snapshot();
        const training::TrainingScenarioCard* card =
            trainingController_.currentCard();
        const bool attemptActive = state.state ==
            training::TrainingProductState::AttemptActive;
        const bool stopped = !authority_.state().ballsMoving && !state.shotInProgress && !state.shotPrepared;
        const bool retryReady = stopped && (state.attemptFacts.shotCount > 0 || !attemptActive);
        const bool unmet = retryReady && attemptActive && !state.evaluation.succeeded;
        const std::string result = state.state ==
                training::TrainingProductState::AttemptEvaluated ?
            (state.evaluation.succeeded ? "Succeeded / score " +
                std::to_string(state.evaluation.score) :
                "Failed / " + state.evaluation.reason) :
            unmet ? "Shot complete - goal not met" : "Attempt active / " + std::to_string(
                state.attemptFacts.shotCount) + " shots";
        const training::TrajectoryOverlaySnapshot overlay =
            trajectoryOverlaySnapshot();
        const auto& predicted = overlay.legend[0];
        const auto& actual = overlay.legend[1];
        const std::string terminalReason = state.state ==
                training::TrainingProductState::AttemptEvaluated ?
            (state.evaluation.reason.empty() ?
                (state.evaluation.succeeded ? "Success" : "No reason supplied") :
                state.evaluation.reason) : unmet ? state.evaluation.feedback : "Pending terminal evaluation";
        const ShellItemViewModel predictedLegend = item(
            "trajectory_predicted", "Predicted", predicted.endpointText,
            predicted.railText, "aim_crosshair", false);
        const ShellItemViewModel actualLegend = item(
            "trajectory_actual", "Replay actual", actual.endpointText,
            actual.railText, "replay_file", false);
        items = {predictedLegend, actualLegend};
        if (trajectoryAdvancedExpanded_) {
            const training::PreviewResult* preview =
                trainingController_.previewResult();
            const auto firstTick = [](const training::OverlayTrack& track) {
                return track.samples.empty() ? std::string("unavailable") :
                    std::to_string(track.samples.front().tick);
            };
            const auto lastTick = [](const training::OverlayTrack& track) {
                return track.samples.empty() ? std::string("unavailable") :
                    std::to_string(track.samples.back().tick);
            };
            const std::string budgetOutcome = preview == nullptr ? "Unavailable" :
                preview->tickBudgetReached ? "Tick limit " +
                    std::to_string(preview->simulatedTicks) + "/" +
                    std::to_string(preview->maximumTicks) :
                preview->wallBudgetReached ? "Wall limit " +
                    decimal(preview->elapsedWallMilliseconds, 1) + "/" +
                    std::to_string(preview->maximumWallMilliseconds) + " ms" :
                "Within tick budget " + std::to_string(preview->simulatedTicks) +
                    "/" + std::to_string(preview->maximumTicks);
            items.push_back(item("trajectory_predicted_sample_count",
                "Predicted sample count",
                std::to_string(overlay.predicted.samples.size()), {},
                "aim_crosshair", false));
            items.push_back(item("trajectory_actual_sample_count",
                "Actual sample count",
                std::to_string(overlay.actual.samples.size()), {},
                "replay_file", false));
            items.push_back(item("trajectory_first_replay_tick",
                "First replay tick", firstTick(overlay.actual), {},
                "seek_previous", false));
            items.push_back(item("trajectory_last_replay_tick",
                "Last replay tick", lastTick(overlay.actual), {},
                "seek_next", false));
            items.push_back(item("trajectory_preview_budget",
                "Preview budget outcome", budgetOutcome, {},
                "rules_shield", false));
            items.push_back(item("trajectory_authority_status",
                "Authority status", state.previewAuthorityUnchanged ?
                    "Unchanged" : "Check unavailable", {},
                "rules_shield", false));
            ShellItemViewModel trace = item("trajectory_trace_hash",
                "Trace hash", compactHash(state.attemptFacts.traceSha256), {},
                "replay_file", false);
            trace.accessibleValue = state.attemptFacts.traceSha256.empty() ?
                "unavailable" : "sha256:" + state.attemptFacts.traceSha256;
            items.push_back(trace);
            ShellItemViewModel content = item("trajectory_content_hash",
                "Content hash", compactHash(state.scenarioFileSha256), {},
                "rules_shield", false);
            content.accessibleValue = state.scenarioFileSha256.empty() ?
                "unavailable" : "sha256:" + state.scenarioFileSha256;
            items.push_back(content);
        } else {
            const bool referenceReady = card != nullptr &&
                card->referenceReplay.bound;
            items.insert(items.begin(), item("training_terminal",
                "Attempt / score", result, "Reason: " + terminalReason,
                "rules_shield", false, true));
            // The verified reference row carries the playback status when a
            // drill has one.  Keeping the older read-only replay-status row as
            // well would make the bounded 1024x768 accessibility inspector
            // exceed its seven-row readable inventory.
            if (!referenceReady) {
                items.insert(items.begin() + 1,
                    item("training_replay_status", "Replay status",
                        state.replayReady ? "Replay ready" :
                            "Replay unavailable",
                        state.replayReady ? "Ready; save enabled" :
                            "Needs terminal result",
                        "replay_file", false));
            }
            items.push_back(attemptActive ?
                item("preview_training", "Preview trajectory", "Predicted",
                    state.previewReady ?
                        std::to_string(state.previewSampleCount) + " samples" :
                        "Run safe preview", "aim_crosshair",
                    !state.shotInProgress && !state.shotPrepared) :
                item("retry_training", "Retry shot", "Pre-shot state",
                    "Fresh recorder and attempt facts", "back_arrow",
                    !state.shotInProgress && !state.shotPrepared));
            if (attemptActive && retryReady)
                items.push_back(item("retry_training", "Retry shot", "Pre-shot state",
                    "Reset and try again", "back_arrow"));
            else items.push_back(item("save_training_replay", "Save replay",
                state.replayReady ? "Replay ready" : "Replay unavailable",
                state.replayReady ? "Choose a local destination" :
                    "Available after terminal success or failure",
                "replay_file", state.replayReady));
            if (referenceReady) {
                items.push_back(item("watch_training_reference",
                    "Watch reference", "Verified Replay v1",
                    "Automated successful attempt", "replay_triangle"));
            }
        }
        items.push_back(item("training_advanced", "Advanced details",
            trajectoryAdvancedExpanded_ ? "Expanded" : "Collapsed",
            "Presentation diagnostics only", trajectoryAdvancedExpanded_ ?
            "back_arrow" : "focus_chevron"));
        if (state.teachingStepCount > 1 &&
            state.state == training::TrainingProductState::AttemptActive &&
            state.attemptFacts.shotCount > 0)
            items.push_back(item("prepare_training_shot", "Prepare shot",
                "Shot " + std::to_string(state.teachingStepIndex + 1) + "/" + std::to_string(state.teachingStepCount),
                "Apply this shot's recommended aim, power and tip; does not shoot", "aim_crosshair",
                !state.shotInProgress && !state.shotPrepared));
        if (manualInspectorPolicy_ && state.evaluation.succeeded) {
            const auto& cards = trainingController_.scenarios();
            for (std::size_t i = 0; i + 1 < cards.size(); ++i)
                if (cards[i].id == state.scenarioId)
                    items.push_back(item("next_training", "Next drill", cards[i + 1].titleEn,
                        "Continue after this completed stage", "play_triangle"));
        }
        if (trajectoryAdvancedExpanded_ && retryReady)
            items.insert(items.end() - 1, item("retry_training", "Retry shot",
                "Pre-shot state", "Start a fresh attempt", "back_arrow"));
        if (state.teachingStepCount > 1)
            for (auto& entry : items) if (entry.id == "retry_training") {
                entry.label = "Restart drill";
                entry.value = "All balls restored";
                entry.detail = "Restart the complete sequence from shot 1";
            }
    } else if (screen_ == ProductScreen::ReplayBrowser) {
        items.push_back(item("choose_replay", "Choose replay file",
            "Replay v1/v2", "Open the system file picker", "replay_file"));
        if (replayPath_.empty()) items.push_back(item("replay_empty",
            "No replay selected", "No local file loaded",
            "Empty and invalid paths are reported", "empty_box", false));
        else items.push_back(item("recent_replay", "Recent replay", replayPath_,
            "Validated local replay", "replay_file", false));
    } else if (screen_ == ProductScreen::ReplayPlayback) {
        const replay::ReplayTransportSnapshot state = replayTransport_.snapshot();
        const training::TrajectoryOverlaySnapshot overlay =
            trajectoryOverlaySnapshot();
        const auto& actual = overlay.legend[1];
        items = {
            item("trajectory_actual", "Replay actual", actual.endpointText,
                actual.railText, "replay_file", false),
            item("trajectory_actual_topology", "Actual topology",
                "Paired continuous rails",
                "Inset actual rails flank the canonical route",
                "aim_crosshair", false),
            item("trajectory_actual_endpoint", "Endpoint marker",
                "Paired capsules", "Inset replay terminus marker",
                "cue_ball", false),
            item("replay_state", "Replay state",
                "Tick " + std::to_string(state.tick) + " / " +
                    decimal(state.playbackRate, 2) + "x",
                state.finished ? "Finished" : "In progress",
                "replay_file", false),
            item("replay_pause", state.paused ? "Play" : "Pause", "Space",
                "Fixed-step playback", state.paused ? "play_triangle" :
                    "pause_bars"),
            item("replay_step_tick", "Step one tick", ".",
                "Advance exactly one authoritative fixed step",
                "seek_next", state.paused),
            item("replay_camera_reset", "Reset view", "Shift+drag / Wheel",
                "Rotate / zoom; reset restores the default view", "camera_frame")};
        if (state.rackTerminalActive) {
            const std::string winner = state.rackWinningPlayer >= 0 ?
                "Player " + std::to_string(state.rackWinningPlayer + 1) +
                    " wins rack" : "Rack ended";
            items.push_back(item("replay_rack_terminal", "Rack ended", winner,
                state.rackTerminalReason ==
                        RulesRackTerminalReason::MobileDeviceRepeatViolation ?
                    "Repeat mobile-device violation; match remains distinct" :
                    "Verified rack-terminal receipt; match remains distinct",
                "rules_book", false));
        }
    } else if (screen_ == ProductScreen::Settings) {
        items = settingsItems();
    } else if (screen_ == ProductScreen::Help) {
        items = {
            item("help_aim", "Aim", "Tab / Mouse X",
                "Enter aim and adjust direction", "aim_crosshair", false, true),
            item("help_power", "Power", "+ / - / Wheel",
                "Adjust before shooting", "power_bars", false, true),
            item("help_shoot", "Shoot", "Left mouse release",
                "Only while aim is active", "cue_ball", false, true),
            item("help_camera", "Camera", "Drag / WASD / Wheel",
                "Orbit, pan, and zoom", "camera_frame", false, true),
            item("help_rules", "Rules", "WPA Heyball",
                "Selected profile: WPA II 17(e) non-8 off-table foul; "
                "8-ball off-table loses", "rules_shield", false, true),
            item("help_back", "Back", "Escape", "Return", "back_arrow")};
        if(returnScreen_==ProductScreen::Match)
            items.insert(items.end()-1,item("help_save_rack_replay",
                "Save rack replay","F9 after rack end",
                "Single-rack only; a rerack starts an unsupported replay boundary",
                "replay_file",false, true));
    }
    if (screen_ == ProductScreen::TrainingSession ||
            screen_ == ProductScreen::ReplayPlayback) {
        const auto advanced = std::find_if(items.begin(), items.end(),
            [](const auto& value) { return value.id == "training_advanced"; });
        const auto overlay = trajectoryOverlaySnapshot();
        const bool hasRoute =
            (overlay.predicted.visible && overlay.predicted.samples.size() >= 2) ||
            (overlay.actual.visible && overlay.actual.samples.size() >= 2);
        items.insert(advanced, item("trajectory_inset_toggle", "Trajectory inset",
            trajectoryInsetEnabled_ ? "On" : "Off",
            trajectoryInsetEnabled_ && !trajectoryInsetPresentationStatus_.empty() ?
                "Map unavailable: " + trajectoryInsetPresentationStatus_ :
            hasRoute ? "Show or hide the trajectory map" :
                "No trajectory available yet", "camera_frame"));
    }
    if (screen_ != ProductScreen::Settings && !items.empty() &&
        focusIndex_ < items.size() && items[focusIndex_].enabled) {
        const std::size_t focused = std::min(focusIndex_, items.size() - 1);
        items[focused].focused = true;
    }
    return items;
}

ProductShellSnapshot ProductShellController::snapshot() const
{
    ProductShellSnapshot result;
    result.trajectoryInsetEnabled = trajectoryInsetEnabled_;
    result.worldTrajectoryOverlayEnabled = worldTrajectoryOverlayEnabled_;
    result.inspectorMode = inspectorMode_;
    result.manualInspectorPolicy = manualInspectorPolicy_;
    result.inspectorScrollRow = inspectorScrollRow_;
    result.screen = screen_;
    result.screenId = productScreenId(screen_);
    result.rulesetId = kApprovedRulesetId;
    result.rulesetLabel = "WPA Heyball";
    result.aiDifficulty = difficulty_;
    result.training = trainingController_.snapshot();
    result.completedTrainingCount = completedTrainingScores_.size();
    if (result.training.evaluation.succeeded && result.training.evaluation.scoreEligible &&
        completedTrainingScores_.count(result.training.scenarioFileSha256) == 0)
        ++result.completedTrainingCount;
    if (const auto* card = trainingController_.currentCard()) {
        result.trainingTitle = card->titleEn;
        result.trainingGoal = card->successSummary;
        if (card->teachingHints.available)
            result.trainingGoal = "Pot ball " + std::to_string(card->teachingHints.objectBallIndex) +
                (card->teachingHints.cueBallTargetEnabled ?
                    " / cue ball inside target ring" : " / keep cue ball on table");
        if (card->teachingHints.cueBallMustAvoidRails)
            result.trainingGoal = card->teachingHints.tipYBallRadii < 0.0f ?
                "Pot ball / draw back into ring / cue: no rails" :
                "Pot ball / follow into ring / cue: no rails";
        if (!result.training.teachingHints.goalEn.empty())
            result.trainingGoal = result.training.teachingHints.goalEn;
        if (result.training.teachingStepCount > 1)
            result.trainingGoal = "Shot " + std::to_string(result.training.teachingStepIndex + 1) +
                "/" + std::to_string(result.training.teachingStepCount) + ": " + result.trainingGoal;
    }
    result.replay = replayTransport_.snapshot();
    result.trajectoryOverlay = trajectoryOverlaySnapshot();
    result.settingsPreviewActive = settingsRuntime_.menu().isOpen();
    result.uiScale = static_cast<float>(result.settingsPreviewActive ?
        settingsRuntime_.menu().preview().uiScale :
        settingsRuntime_.current().uiScale);
    result.rebindCaptureActive = settingsRuntime_.menu().waitingForBinding();
    result.errorCode = errorCode_;
    result.errorText = errorCode_.empty() ? "" : "Action unavailable: " + errorCode_;
    result.items = itemsForScreen();
    result.breadcrumb = screen_ == ProductScreen::MainMenu ? "BilliardGL" :
        std::string("BilliardGL / ") + result.screenId;
    switch (screen_) {
    case ProductScreen::MainMenu:
        result.title = "BilliardGL";
        result.subtitle = "WPA Heyball, training, and verified replays";
        result.footer = "Arrows Navigate   Enter Select   F10 Settings   H Help";
        break;
    case ProductScreen::SinglePlayerSetup:
        result.title = "Single player";
        result.subtitle = "Rules and AI difficulty";
        result.footer = "Choose Rookie, Club, or Expert";
        break;
    case ProductScreen::Match:
        result.title = "WPA Heyball match";
        result.subtitle = std::string("Single player / ") +
            shellDifficultyLabel(difficulty_);
        result.footer = "Tab Aim   Mouse Shoot   F9 Save rack   F10 Settings   H Help";
        break;
    case ProductScreen::TrainingCatalog:
        result.title = "Training catalog";
        result.subtitle = std::to_string(trainingController_.scenarios().size()) +
            " authoritative launch drills";
        result.footer = "Arrows Navigate   Enter Start   Escape Back";
        break;
    case ProductScreen::TrainingSession:
        result.title = "Training session";
        result.subtitle = result.training.scenarioId;
        if (result.training.state == training::TrainingProductState::AttemptEvaluated)
            result.subtitle = "Attempt finished - select Retry shot";
        result.footer = "R Reset drill   Escape Catalog";
        break;
    case ProductScreen::ReplayBrowser:
        result.title = "Replay browser";
        result.subtitle = "Open a local validated Replay v1/v2 file";
        result.footer = "Invalid or empty paths remain visible as errors";
        break;
    case ProductScreen::ReplayPlayback:
        result.title = "Replay playback";
        result.subtitle = "Tick " + std::to_string(result.replay.tick) +
            " / " + decimal(result.replay.playbackRate, 2) + "x";
        result.footer = std::string("Space ") +
            (result.replay.paused ? "Play" : "Pause") +
            "   +/- Rate   C Camera   PageUp/PageDown Seek";
        break;
    case ProductScreen::Settings:
        result.title = "Settings / " +
            settingsCategoryName(settingsRuntime_.menu().category());
        result.subtitle = result.rebindCaptureActive ?
            "Press a new control" : "Preview changes before atomic apply";
        result.footer = "Arrows Focus/Category   +/- Change   Enter Apply   F10 Cancel";
        break;
    case ProductScreen::Help:
        result.title = "Help";
        result.subtitle = "Keyboard, mouse, and rules";
        result.footer = "Escape Back";
        break;
    }
    return result;
}

training::TrajectoryOverlaySnapshot
ProductShellController::trajectoryOverlaySnapshot() const
{
    training::TrajectoryOverlayModel model;
    if (const training::PreviewResult* preview =
            trainingController_.previewResult())
        model.setPredicted(*preview);
    if (!manualInspectorPolicy_ || screen_ != ProductScreen::TrainingSession || showTrainingComparison_)
        model.setActual(replayTransport_.snapshot());
    model.setAdvancedExpanded(trajectoryAdvancedExpanded_);
    return model.snapshot();
}

const char* productScreenId(ProductScreen screen) noexcept
{
    switch (screen) {
    case ProductScreen::MainMenu: return "main_menu";
    case ProductScreen::SinglePlayerSetup: return "single_player_setup";
    case ProductScreen::Match: return "match";
    case ProductScreen::TrainingCatalog: return "training_catalog";
    case ProductScreen::TrainingSession: return "training_session";
    case ProductScreen::ReplayBrowser: return "replay_browser";
    case ProductScreen::ReplayPlayback: return "replay_playback";
    case ProductScreen::Settings: return "settings";
    case ProductScreen::Help: return "help";
    }
    return "unknown";
}

}  // namespace billiardgl
