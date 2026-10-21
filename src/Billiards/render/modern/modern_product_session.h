#pragma once

#include "ai/ai_player_controller.h"
#include "audio/product_audio_controller.h"
#include "product_shell_controller.h"
#include "render/modern/product_input_adapter.h"
#include "render/modern/table_plane_pointer_projection.h"
#include "render/ui/product_shell_layout.h"
#include "render/ui/product_ui_frame_coordinator.h"
#include "render/ui/rules_decision_ui_draw_list.h"
#include "rules/rules_decision_ui_model.h"

#include <set>
#include <map>
#include <memory>
#include <optional>
#include <string>
#include <vector>

namespace billiardgl::render {

inline constexpr const char* kRulesDecisionUiImplementationCapability =
    "internal.rules_decision_ui_implementation_v1";

struct RulesDecisionProductCapabilities {
    // Shipping capability for the exact WPA II 10(4) group-choice parent.
    // This does not enable the generic/internal Rules Decision UI.
    bool groupChoice = false;
    // Shipping capability for an ordinary-foul Core-G FullTable placement.
    // BehindHeadString placement remains owned by the exact 06E/06F chains.
    bool fullTableBallInHand = false;
    // Shipping capability for the exact WPA II 6(c) IllegalBreak parent.
    // Sibling rules branches remain fail-closed.
    bool illegalBreakChoice = false;
    // Shipping capability for the exact WPA II 6(e) BreakShotFoul parent and
    // its BehindHeadString placement/respot child chain.
    bool breakShotFoulChoice = false;
    // Shipping capability for the exact WPA II 6(f) EightOnBreak parent and
    // its foul-branch BehindHeadString placement/respot child chain.
    bool eightOnBreakChoice = false;
    // Shipping capability for the exact WPA II 6(g) signed native-miscue
    // parent.  This exposes only the two rerack choices after Core rules have
    // already consumed a signed declaration and native collector evidence.
    bool noRackContactMiscueChoice = false;
};

struct ProductFileDialogSelection {
    bool accepted = false;
    std::string path;
};

// Platform-owned and injectable. Headless sessions leave this null or supply
// a fake; only the macOS application adapter is allowed to open native panels.
class ProductFileDialogAdapter {
public:
    virtual ~ProductFileDialogAdapter() = default;
    virtual ProductFileDialogSelection chooseReplayFile() = 0;
    virtual ProductFileDialogSelection saveTrainingReplayFile(
        const std::string& suggestedFilename) = 0;
    virtual ProductFileDialogSelection saveMatchReplayFile(
        const std::string& suggestedFilename)
    {
        return saveTrainingReplayFile(suggestedFilename);
    }
};

struct ModernProductSessionOptions {
    bool manualInspectors = false;
    std::string settingsPath;
    std::string trainingManifestPath;
    std::string executableVersion;
    ProductFileDialogAdapter* fileDialogAdapter = nullptr;
    std::set<std::string> trainingCapabilities{
        "training_scenario_v1",
        "authoritative_horizontal_tip_offset_v1",
        "authoritative_longitudinal_tip_offset_v1"};
    // Internal implementation/evidence capabilities are empty in the product
    // application. Enabling this does not authorize product exposure.
    std::set<std::string> internalCapabilities;
    RulesDecisionProductCapabilities rulesDecisionProductCapabilities;
    int rulesDecisionLocalPlayerId = 0;
    ui::RulesDecisionLocale rulesDecisionLocale =
        ui::RulesDecisionLocale::En;
    bool m4CameraCandidate = false;
};

struct ModernProductEventResult {
    bool ok = true;
    bool handled = false;
    std::string errorCode;
    ProductShellAppRequest appRequest = ProductShellAppRequest::None;
    std::string action;
};

struct ModernProductSessionSnapshot {
    ProductShellSnapshot shell;
    settings::StartupResult settingsStartup;
    settings::ProductSettings effectiveSettings =
        settings::ProductSettings::defaults();
    ai::PlayerState aiPlayer;
    ProductInputSurface inputSurface = ProductInputSurface::Play;
    ProductPointerSnapshot pointer;
    bool audioEnabled = false;
    audio::ProductAudioSnapshot audio;
    std::uint64_t audioConsumedAuthorityTicks = 0;
    std::uint64_t audioFailureCount = 0;
    std::string audioError;
    bool quitRequested = false;
    bool chooseReplayRequested = false;
    bool matchReplayRecording = false;
    bool matchReplayReady = false;
    std::string matchReplayError;
    std::string lastSavedReplayPath;
    std::string trainingLoadError;
    std::string lastError;
    ui::ShellViewport shellViewport;
    std::string hoveredShellItemId;
    std::string pressedShellItemId;
    bool rulesDecisionUiImplementationEnabled = false;
    bool rulesDecisionUiProductExposed = false;
    bool rulesDecisionGroupChoiceProductEnabled = false;
    bool rulesDecisionFullTableBallInHandProductEnabled = false;
    bool rulesDecisionIllegalBreakProductEnabled = false;
    bool rulesDecisionBreakShotFoulProductEnabled = false;
    bool rulesDecisionEightOnBreakProductEnabled = false;
    bool rulesDecisionNoRackContactMiscueProductEnabled = false;
    bool rulesDecisionProductActive = false;
    ui::RulesDecisionLocale rulesDecisionLocale =
        ui::RulesDecisionLocale::En;
    rules::ui::DecisionUiSnapshot rulesDecision;
    std::optional<ui::TrajectoryInsetVisibleCurrentContext>
        trajectoryInsetVisibleCurrentContext;
};

// Owns the product-only UI/input models while borrowing the same authoritative
// runtime and AI player used by rendering and fixed-step simulation.
class ModernProductSession {
public:
    ModernProductSession(GameRuntime& runtime, ai::AiPlayerController& aiPlayer,
        ModernProductSessionOptions options,
        audio::AudioCueConsumer* audioConsumer = nullptr);

    ModernProductEventResult handlePlatformInput(
        const PlatformInputEvent& event);
    ModernProductEventResult beginRebind(const std::string& action);
    ai::PlayerState pollAi();
    // Call once for each product presentation frame. Replay scheduling is
    // isolated from the live authority and is a no-op off ReplayPlayback.
    ModernProductEventResult advancePresentedFrame();
    ModernProductEventResult advancePresentedFrame(double elapsedSeconds);
    // Selects the authoritative visual source for the current product screen.
    // Replay playback is rendered from the isolated replay runtime; every
    // other screen continues to render the borrowed live authority.
    const GameRuntime& presentationRuntime() const noexcept;
    camera::Frame presentationCameraFrame() const noexcept;
    // Call after an authority step attempt. All newly retained, successful,
    // externally-published ticks are mapped and consumed exactly once.
    void consumeAuthoritySteps();
    // Called only by the interactive owner explicitly auditioning a loop pack.
    void startAudioLoops();
    double lastAudioConsumeMilliseconds() const noexcept { return lastAudioConsumeMilliseconds_; }
    ModernProductEventResult chooseReplayFile();
    ModernProductEventResult saveTrainingReplayFile();
    ModernProductEventResult saveMatchReplayFile();
    ModernProductEventResult retryTrainingAttempt();
    ModernProductEventResult refreshTrainingPreview(
        const training::PreviewBudget& budget = training::PreviewBudget{});
    void setShellViewport(const ui::ShellViewport& viewport);
    void cancelPointerInteraction();
    void setRulesDecisionHitRects(
        std::vector<ui::RulesDecisionUiHitRect> hitRects);
    void setRulesDecisionExclusionRects(
        std::vector<PointerProjectionRect> exclusionRects);
    // The placement ray must use the exact presentation camera consumed by
    // the renderer, including legacy projection pan retained by render_state.
    // This is presentation-only state and never mutates rules authority.
    void setRulesDecisionProjectionCamera(const CameraPresentation& camera);
    void setTrajectoryInsetVisibleCurrentContext(
        const ui::TrajectoryInsetVisibleCurrentContext& context)
    {
        trajectoryInsetVisibleCurrentContext_ = context;
    }
    // Call once per actual presented frame, then reuse the snapshot for any
    // evidence-only paired redraw of that frame.
    void prepareTrajectoryInsetCurrentFrame(int samples);
    void setTrajectoryInsetPresentationStatus(std::string reason)
    { shell_.setTrajectoryInsetPresentationStatus(std::move(reason)); }
    void setPresentedTrajectoryInsetToggle(
        std::optional<product_hud::HudRect> rect, std::uint64_t frameSerial);
    void clearTrajectoryInsetVisibleCurrentContext() noexcept
    {
        trajectoryInsetVisibleCurrentContext_.reset();
    }
    ui::ShellScreenLayout shellLayout() const;
    void syncRulesDecisionAuthority();
    void syncRulesDecisionPlacementCamera(
        const std::string& activeChildDecisionId);
    void restoreRulesDecisionPlacementProjectionPan() noexcept;
    void discardRulesDecisionPlacementCamera() noexcept;
    // Narrow shipping integration seam for WPA II 6(f). This accepts only an
    // exact action already frozen by the authoritative Core-F parent. It never
    // exposes Core-H/Core-E actions or the global Rules Decision UI surface.
    ModernProductEventResult submitProductAuthorizedEightOnBreakChoice(
        const std::string& actionId, const std::string& idempotencyKey,
        const std::string& payloadCanonical = "{}");
    // Source-compatible name retained for the already-tested narrow authority
    // seam. Shipping exposure is gated by eightOnBreakChoice.
    ModernProductEventResult submitProductAuthorizedEightOnBreakRespot(
        const std::string& actionId, const std::string& idempotencyKey,
        const std::string& payloadCanonical = "{}");
    // Narrow authority-adapter seam for WPA II 10(4). Existing explicit
    // callers supply their own authorization; the shipping pointer/keyboard
    // route reaches this only after groupChoice capability, local actor,
    // current-flow and armed-action gates. Only an action in the live
    // GroupChoice parent offer can enter Core D.
    ModernProductEventResult submitProductAuthorizedGroupChoice(
        const std::string& actionId, const std::string& idempotencyKey);
    // Narrow shipping seam for WPA II 6(e). It consumes only the exact
    // current BreakShotFoul Core-H offer for the local opposing actor. The
    // global Rules UI and all sibling/foundation actions remain unavailable.
    ModernProductEventResult submitProductAuthorizedBreakFoulChoice(
        const std::string& actionId, const std::string& idempotencyKey,
        const std::string& payloadCanonical = "{}");
    // Shipping WPA II 6(c) seam. This is deliberately distinct from the 6(e)
    // consumer even though the authority types share two action identifiers.
    ModernProductEventResult submitProductAuthorizedIllegalBreakChoice(
        const std::string& actionId, const std::string& idempotencyKey,
        const std::string& payloadCanonical = "{}");
    // Shipping WPA II 6(g) seam.  The signed declaration and native evidence
    // are upstream Core responsibilities; this consumer accepts only the
    // frozen two-rerack offer for the local opposing actor.
    ModernProductEventResult submitProductAuthorizedNoRackContactMiscueChoice(
        const std::string& actionId, const std::string& idempotencyKey,
        const std::string& payloadCanonical = "{}");
    // Narrow shipping seam for the Core-G placement child transaction used
    // by WPA II §§8/9. Preview is validation-only. Confirm requires the exact
    // current legal preview and live offer; it does not expose the Rules UI.
    ModernProductEventResult previewProductAuthorizedBallInHandPlacement(
        const rules::BallInHandPlacementPosition& position);
    ModernProductEventResult confirmProductAuthorizedBallInHandPlacement(
        const std::string& actionId,
        const rules::BallInHandPlacementPosition& position,
        const std::string& idempotencyKey);
    // Narrow WPA-II-08 seam. Only the live BehindHeadString Core-G child may
    // request the all-behind parent or choose one ball from its exact tie.
    ModernProductEventResult submitProductAuthorizedBehindHeadStringRespot(
        const std::string& actionId, int selectedBallId,
        const std::string& idempotencyKey,
        const std::string& payloadCanonical = "{}");
    ModernProductEventResult armRulesDecisionAction(
        const std::string& actionId);
    ModernProductEventResult previewRulesDecisionPlacement(
        const rules::BallInHandPlacementPosition& position);
    ModernProductEventResult confirmRulesDecisionAction(
        const std::string& idempotencyKey);
    void escapeRulesDecisionUi();

    ModernProductSessionSnapshot snapshot() const;
    ProductShellController& shell() noexcept { return shell_; }
    const ProductShellController& shell() const noexcept { return shell_; }
    settings::ProductSettingsRuntime& settings() noexcept { return settings_; }
    const settings::ProductSettingsRuntime& settings() const noexcept
    {
        return settings_;
    }
    training::TrainingProductController& training() noexcept
    {
        return training_;
    }
    const training::TrainingProductController& training() const noexcept
    {
        return training_;
    }

private:
    ModernProductEventResult dispatchShellResult(
        const ProductShellResult& result, bool handled);
    ModernProductEventResult dispatchNormalized(
        const NormalizedInputEvent& event);
    ModernProductEventResult navigate(ShellNavigationCommand command);
    ModernProductEventResult handleShellPointer(
        const PlatformInputEvent& event);
    ModernProductEventResult handleMatchLifecycleTransition(
        ProductScreen previousScreen,ModernProductEventResult result);
    void beginMatchReplayRecording();
    void captureMatchReplayProgress();
    void clearShellPointerState();
    void refreshAiGate();
    const settings::ProductSettings& effectiveAudioSettings() const;
    void applyAudioSettings();
    void captureAudioResult(const audio::ProductAudioResult& result);
    bool rulesDecisionActorIsLocal(int actorPlayerId) const;
    ModernProductEventResult handleRulesDecisionInput(
        const PlatformInputEvent& event,
        const ProductInputAdaptation& adapted);

    GameRuntime& runtime_;
    ai::AiPlayerController& aiPlayer_;
    settings::ProductSettingsRuntime settings_;
    training::TrainingProductController training_;
    replay::ReplayTransportModel replay_;
    replay::ProductReplayContentRegistry replayContentRegistry_;
    replay::Recorder matchReplayRecorder_;
    std::string matchReplayError_;
    ProductInputController input_;
    ProductShellController shell_;
    ProductInputAdapter adapter_;
    std::unique_ptr<audio::ProductAudioController> audioController_;
    settings::StartupResult settingsStartup_;
    std::string trainingLoadError_;
    std::string executableVersion_;
    ProductFileDialogAdapter* fileDialogAdapter_ = nullptr;
    std::string lastSavedReplayPath_;
    std::string lastSavedReplayExecutableVersion_;
    std::string lastSavedReplayContentSha256_;
    std::string lastError_;
    bool hasAudioJournalGeneration_ = false;
    std::uint64_t audioJournalGeneration_ = 0;
    std::uint64_t lastPhysicsStepAttemptSequence_ = 0;
    std::uint64_t audioConsumedAuthorityTicks_ = 0;
    std::uint64_t audioFailureCount_ = 0;
    double lastAudioConsumeMilliseconds_ = 0.0;
    std::string audioError_;
    ui::ShellViewport shellViewport_;
    std::optional<ui::TrajectoryInsetVisibleCurrentContext>
        trajectoryInsetVisibleCurrentContext_;
    std::uint64_t trajectoryInsetFrameSerial_ = 0;
    ProductScreen trajectoryInsetPreparedScreen_ = ProductScreen::MainMenu;
    std::uint64_t trajectoryInsetPreparedTick_ = 0;
    std::optional<product_hud::HudRect> presentedInsetToggle_;
    bool insetTogglePressed_ = false;
    ProductScreen presentedInsetScreen_ = ProductScreen::MainMenu;
    std::uint64_t presentedInsetTick_ = 0;
    std::string hoveredShellItemId_;
    std::string pressedShellItemId_;
    bool quitRequested_ = false;
    bool chooseReplayRequested_ = false;
    bool rulesDecisionUiImplementationEnabled_ = false;
    RulesDecisionProductCapabilities rulesDecisionProductCapabilities_;
    bool rulesDecisionProductActive_ = false;
    std::vector<ui::RulesDecisionUiHitRect> rulesDecisionHitRects_;
    std::vector<PointerProjectionRect> rulesDecisionExclusionRects_;
    std::optional<CameraPresentation> rulesDecisionProjectionCamera_;
    std::optional<camera::Frame> rulesDecisionPlacementSavedCamera_;
    std::optional<std::array<float, 3>>
        rulesDecisionPlacementSavedProjectionPan_;
    std::string rulesDecisionPlacementCameraDecisionId_;
    std::uint64_t rulesDecisionPlacementCameraJournalGeneration_ = 0;
    std::string pressedRulesDecisionControlId_;
    std::uint64_t rulesDecisionSubmissionOrdinal_ = 0;
    int rulesDecisionLocalPlayerId_ = 0;
    ui::RulesDecisionLocale rulesDecisionLocale_ =
        ui::RulesDecisionLocale::En;
    rules::ui::RulesDecisionUiModel rulesDecisionUi_;
    std::map<std::string, rules::EightOnBreakDecisionRequest>
        productAuthorizedEightOnBreakRequests_;
    std::map<std::string, rules::GroupChoiceDecisionRequest>
        productAuthorizedGroupChoiceRequests_;
    std::map<std::string, rules::BreakFoulDecisionRequest>
        productAuthorizedBreakFoulRequests_;
    std::map<std::string, rules::BreakFoulDecisionRequest>
        productAuthorizedIllegalBreakRequests_;
    std::map<std::string, rules::BreakFoulDecisionRequest>
        productAuthorizedNoRackContactMiscueRequests_;
    struct ProductAuthorizedPlacementPreview {
        std::string decisionId;
        std::uint64_t authorityStateRevision = 0;
        std::string authorityStateHash;
        int actorPlayerId = -1;
        rules::BallInHandPlacementScope scope =
            rules::BallInHandPlacementScope::None;
        std::string actionId;
        rules::BallInHandPlacementPosition position;
    };
    std::optional<ProductAuthorizedPlacementPreview>
        productAuthorizedPlacementPreview_;
    // Set only by a successfully acknowledged product 06E ball-in-hand
    // branch and advanced by exact placement/respot replacement receipts.
    // It prevents unrelated imported BIH offers from inheriting 06E UI.
    std::string product06ePlacementDecisionId_;
    // A committed placement deliberately retains a replacement runtime BIH
    // entitlement until the same actor strikes.  This id distinguishes that
    // post-placement entitlement from an unhandled placement prompt so the
    // product modal can close without weakening the authority contract.
    std::string product06eRetainedPlacementDecisionId_;
    // 06F has a distinct parent and must not inherit 06E child authority.
    std::string product06fPlacementDecisionId_;
    std::string product06fRetainedPlacementDecisionId_;
    // Ordinary FullTable BIH is a standalone Core-G offer.  The current id
    // is rendered as a modal; the replacement id retained after commit is an
    // entitlement for repositioning before the stroke and must not reopen it.
    std::string productFullTablePlacementDecisionId_;
    std::string productFullTableRetainedPlacementDecisionId_;
    std::map<std::string, rules::BallInHandPlacementRequest>
        productAuthorizedPlacementRequests_;
    std::map<std::string, rules::BehindHeadStringRespotRequest>
        productAuthorizedBehindHeadStringRespotRequests_;
};

}  // namespace billiardgl::render
