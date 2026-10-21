#pragma once

#include "ai/ai_player_control.h"
#include "product_input_controller.h"
#include "replay/product_replay_content_registry.h"
#include "training/trajectory_overlay_model.h"

#include <cstdint>
#include <map>
#include <set>
#include <string>
#include <vector>

namespace billiardgl {

enum class ProductScreen {
    MainMenu,
    SinglePlayerSetup,
    Match,
    TrainingCatalog,
    TrainingSession,
    ReplayBrowser,
    ReplayPlayback,
    Settings,
    Help,
};

enum class ShellNavigationCommand {
    FocusNext,
    FocusPrevious,
    Activate,
    Back,
};

struct ApprovedRuleEntry {
    std::string id;
    std::string label;
};

struct ShellItemViewModel {
    std::string id;
    std::string label;
    std::string value;
    std::string detail;
    // Full assistive value when the visible shipping string is intentionally
    // abbreviated (for example a SHA-256 in Advanced diagnostics).
    std::string accessibleValue;
    std::string shapeToken;
    bool enabled = true;
    bool selected = false;
    bool focused = false;
    // Read-only rows are not focusable or activatable, but their explanatory
    // text remains ordinary product information rather than a disabled label.
    // Keep this appended so existing aggregate fixtures retain their field
    // meanings.
    bool readOnlyInformational = false;
};

struct ProductShellSnapshot {
    bool trajectoryInsetEnabled = true;
    bool worldTrajectoryOverlayEnabled = false;
    bool manualInspectorPolicy = false;
    int inspectorMode = 0; // 0 automatic, 1 explicitly expanded, 2 explicitly collapsed
    std::size_t inspectorScrollRow = 0;
    ProductScreen screen = ProductScreen::MainMenu;
    std::string screenId;
    std::string title;
    std::string subtitle;
    std::string breadcrumb;
    std::vector<ShellItemViewModel> items;
    std::string rulesetId;
    std::string rulesetLabel;
    ai::Difficulty aiDifficulty = ai::Difficulty::Club;
    training::TrainingProductSnapshot training;
    std::string trainingTitle;
    std::string trainingGoal;
    std::size_t completedTrainingCount = 0;
    replay::ReplayTransportSnapshot replay;
    training::TrajectoryOverlaySnapshot trajectoryOverlay;
    float uiScale = 1.0f;
    bool settingsPreviewActive = false;
    bool rebindCaptureActive = false;
    std::string errorCode;
    std::string errorText;
    std::string footer;
};

enum class ProductShellAppRequest {
    None,
    Quit,
    ChooseReplayFile,
    SaveTrainingReplayFile,
    SaveMatchReplayFile,
};

struct ProductShellResult {
    bool ok = false;
    std::string errorCode;
    ProductShellAppRequest appRequest = ProductShellAppRequest::None;
    std::string action;
};

class ProductShellController {
public:
    void setM4CameraCandidate(bool enabled) { m4CameraCandidate_ = enabled; }
    void setManualInspectorPolicy(bool enabled) { manualInspectorPolicy_ = enabled; }
    void setTrajectoryInsetPresentationStatus(std::string reason)
    { trajectoryInsetPresentationStatus_ = std::move(reason); }
    void setInspectorExpanded(bool expanded) {
        inspectorMode_ = expanded ? 1 : 2;
        if (expanded) inspectorScrollRow_ = focusIndex_;
    }
    void setInspectorScrollRow(std::size_t row) { inspectorScrollRow_ = row; }
    static constexpr const char* kApprovedRulesetId =
        replay::kOfficialSingleRackReplayRulesetId;

    ProductShellController(GameRuntime& authority,
        settings::ProductSettingsRuntime& settingsRuntime,
        ProductInputController& inputController,
        training::TrainingProductController& trainingController,
        replay::ReplayTransportModel& replayTransport,
        ai::PlayerControl* aiPlayer = nullptr);

    const std::vector<ApprovedRuleEntry>& approvedRules() const noexcept
    {
        return approvedRules_;
    }
    ProductScreen screen() const noexcept { return screen_; }
    ai::Difficulty selectedDifficulty() const noexcept { return difficulty_; }

    ProductShellResult openMainMenu();
    ProductShellResult openSinglePlayerSetup();
    ProductShellResult selectDifficulty(ai::Difficulty difficulty);
    ProductShellResult startSinglePlayer(std::uint64_t seed = 1);
    ProductShellResult loadTrainingCatalog(const std::string& manifestPath,
        const std::set<std::string>& capabilities);
    ProductShellResult startTrainingScenario(const std::string& scenarioId,
        const std::string& executableVersion, std::uint32_t seed);
    ProductShellResult openReplayBrowser();
    ProductShellResult loadReplayPath(const std::string& path,
        const std::string& executableVersion,
        const std::string& contentSha256);
    ProductShellResult loadReplayPath(const std::string& path,
        const std::string& executableVersion,
        const std::string& contentSha256,
        const PhysicsProfile& replayProfile);
    // Product-frame playback seam. This is intentionally inactive outside
    // ReplayPlayback so the application render loop cannot advance a hidden
    // replay authority after navigation.
    ProductShellResult advanceReplayPresentedFrame();
    ProductShellResult advanceReplayPresentedFrame(double elapsedSeconds);
    // Capture/review boundary: loads and fully samples a validated Replay v1
    // into the isolated replay transport while retaining TrainingSession.
    // It never mutates the borrowed match/training authority.
    ProductShellResult loadTrainingComparisonReplayPath(
        const std::string& path, const std::string& executableVersion,
        const std::string& contentSha256);
    ProductShellResult watchTrainingReferenceReplay();
    ProductShellResult openSettings();
    ProductShellResult openHelp();
    ProductShellResult beginRebind(const std::string& action);
    ProductShellResult dispatchProductInput(const NormalizedInputEvent& event);
    ProductShellResult focusItem(const std::string& id);
    ProductShellResult navigate(ShellNavigationCommand command);
    // Event-boundary synchronization only. The shell invokes this after a
    // settings preview/apply/cancel and the session invokes it once at startup.
    void syncCameraPresentationPreferences() noexcept;

    ProductShellSnapshot snapshot() const;
    void rememberTrainingCompletion();

private:
    bool m4CameraCandidate_ = false;
    std::map<std::string, int> completedTrainingScores_;
    bool showTrainingComparison_ = false;
    bool trajectoryInsetEnabled_ = true;
    std::string trajectoryInsetPresentationStatus_;
    bool worldTrajectoryOverlayEnabled_ = false;
    ProductShellResult activateFocused();
    ProductShellResult back();
    void setScreen(ProductScreen screen);
    void setError(const std::string& code);
    void clearError();
    std::vector<ShellItemViewModel> itemsForScreen() const;
    std::vector<ShellItemViewModel> settingsItems() const;
    training::TrajectoryOverlaySnapshot trajectoryOverlaySnapshot() const;

    GameRuntime& authority_;
    settings::ProductSettingsRuntime& settingsRuntime_;
    ProductInputController& inputController_;
    training::TrainingProductController& trainingController_;
    replay::ReplayTransportModel& replayTransport_;
    ai::PlayerControl* aiPlayer_ = nullptr;
    std::vector<ApprovedRuleEntry> approvedRules_;
    ProductScreen screen_ = ProductScreen::MainMenu;
    ProductScreen returnScreen_ = ProductScreen::MainMenu;
    ai::Difficulty difficulty_ = ai::Difficulty::Club;
    std::size_t focusIndex_ = 0;
    bool trajectoryAdvancedExpanded_ = false;
    int inspectorMode_ = 0;
    bool manualInspectorPolicy_ = false;
    std::size_t inspectorScrollRow_ = 0;
    std::string replayPath_;
    bool replayReturnsToTraining_ = false;
    std::string errorCode_;
};

const char* productScreenId(ProductScreen screen) noexcept;

}  // namespace billiardgl
