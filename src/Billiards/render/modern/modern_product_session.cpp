#include "render/modern/modern_product_session.h"
#include "deterministic_hash.h"


#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstddef>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <iomanip>
#include <locale>
#include <sstream>
#include <utility>

namespace billiardgl::render {
namespace {

bool isShellNavigationScreen(ProductScreen screen)
{
    return screen == ProductScreen::MainMenu ||
        screen == ProductScreen::SinglePlayerSetup ||
        screen == ProductScreen::TrainingCatalog ||
        screen == ProductScreen::ReplayBrowser ||
        screen == ProductScreen::Help;
}

bool isShellPointerScreen(ProductScreen screen)
{
    return isShellNavigationScreen(screen) || screen == ProductScreen::Settings ||
        screen == ProductScreen::TrainingSession || screen == ProductScreen::ReplayPlayback;
}

bool isGameplayScreen(ProductScreen screen)
{
    return screen == ProductScreen::Match ||
        screen == ProductScreen::TrainingSession ||
        screen == ProductScreen::ReplayPlayback;
}

bool navigationCommand(const NormalizedInputEvent& event,
    ShellNavigationCommand& command)
{
    if (event.device != "keyboard" || event.trigger != "press") return false;
    if (event.control == "ArrowDown" || event.control == "ArrowRight" ||
        event.control == "Tab") {
        command = ShellNavigationCommand::FocusNext;
        return true;
    }
    if (event.control == "ArrowUp" || event.control == "ArrowLeft") {
        command = ShellNavigationCommand::FocusPrevious;
        return true;
    }
    if (event.control == "Enter" || event.control == "Space") {
        command = ShellNavigationCommand::Activate;
        return true;
    }
    if (event.control == "Escape") {
        command = ShellNavigationCommand::Back;
        return true;
    }
    return false;
}

bool isExactDirectionalKey(const NormalizedInputEvent& event)
{
    return event.control == "ArrowDown" || event.control == "ArrowUp" ||
        event.control == "ArrowLeft" || event.control == "ArrowRight" ||
        event.control == "A" || event.control == "D";
}

bool sameRulesOffer(const rules::ui::DecisionUiSnapshot& current,
    const std::string& decisionId, std::uint64_t revision, int actor,
    const std::vector<std::string>& actions)
{
    return current.offer.decisionId == decisionId &&
        current.offer.authorityStateRevision == revision &&
        current.offer.actorPlayerId == actor &&
        current.offer.offeredActionIds == actions &&
        current.state != rules::ui::DecisionUiState::HiddenNoOffer &&
        current.state != rules::ui::DecisionUiState::CompletedAuthoritative;
}

bool validAuthorityHash(const std::string& value)
{
    return value.size() == 64 && std::all_of(value.begin(), value.end(),
        [](unsigned char character) {
            return (character >= '0' && character <= '9') ||
                (character >= 'a' && character <= 'f');
        });
}

bool exactGroupChoiceAck(const rules::GroupChoiceDecisionRequest& request,
                         const rules::GroupChoiceDecisionAck& ack)
{
    return ack.accepted && ack.decisionId == request.decisionId &&
        ack.actionId == request.actionId &&
        ack.idempotencyKey == request.idempotencyKey &&
        ack.actorPlayerId == request.actorPlayerId &&
        ack.previousStateRevision == request.expectedAuthorityStateRevision &&
        ack.authoritativeStateRevision == ack.previousStateRevision + 1 &&
        validAuthorityHash(ack.authoritativeStateHash);
}

bool exactEightOnBreakAck(const rules::EightOnBreakDecisionRequest& request,
                          const rules::EightOnBreakDecisionAck& ack)
{
    if (!ack.accepted || ack.decisionId != request.decisionId ||
        ack.actionId != request.actionId ||
        ack.idempotencyKey != request.idempotencyKey ||
        ack.actorPlayerId != request.actorPlayerId ||
        ack.previousStateRevision != request.expectedAuthorityStateRevision ||
        ack.authoritativeStateRevision != ack.previousStateRevision + 1 ||
        !validAuthorityHash(ack.authoritativeStateHash) ||
        ack.effects.decisionPending)
        return false;

    const bool legal =
        ack.effects.branch == rules::EightOnBreakBranch::LegalBreak;
    const int originalBreaker = ack.effects.originalBreakerPlayerId;
    if ((legal && request.actorPlayerId != originalBreaker) ||
        (!legal && request.actorPlayerId != 1 - originalBreaker))
        return false;

    const bool spot =
        request.actionId == rules::kEightOnBreakSpotContinueActionId ||
        request.actionId == rules::kEightOnBreakSpotBallInHandActionId;
    if (spot) {
        const bool foulSpot =
            request.actionId == rules::kEightOnBreakSpotBallInHandActionId;
        return ack.effects.eightBallRespotted &&
            !ack.effects.rackRebuilt && ack.effects.breakerPlayerId == -1 &&
            ack.effects.currentPlayer == request.actorPlayerId &&
            ack.effects.nextPlayer == request.actorPlayerId &&
            ack.effects.turnContinues == !foulSpot &&
            ack.effects.ballInHand == foulSpot &&
            ack.effects.ballInHandScope == (foulSpot ?
                rules::BallInHandPlacementScope::BehindHeadString :
                rules::BallInHandPlacementScope::None);
    }

    const int expectedBreaker =
        request.actionId == rules::kEightOnBreakRerackOriginalActionId ?
            originalBreaker : request.actorPlayerId;
    return !ack.effects.eightBallRespotted && ack.effects.rackRebuilt &&
        ack.effects.breakerPlayerId == expectedBreaker &&
        ack.effects.currentPlayer == expectedBreaker &&
        ack.effects.nextPlayer == expectedBreaker &&
        !ack.effects.turnContinues && !ack.effects.ballInHand &&
        ack.effects.ballInHandScope ==
            rules::BallInHandPlacementScope::None &&
        ack.effects.phase == rules::MatchPhase::Break;
}

bool exactBreakFoulAck(const rules::BreakFoulDecisionRequest& request,
                       const rules::BreakFoulDecisionAck& ack)
{
    if (!ack.accepted || ack.decisionId != request.decisionId ||
        ack.actionId != request.actionId ||
        ack.idempotencyKey != request.idempotencyKey ||
        ack.actorPlayerId != request.actorPlayerId ||
        ack.previousStateRevision != request.expectedAuthorityStateRevision ||
        ack.authoritativeStateRevision != ack.previousStateRevision + 1 ||
        ack.effects.branch != rules::BreakFoulBranch::BreakShotFoul ||
        ack.effects.tableAccepted ||
        ack.effects.originalBreakerPlayerId != 1 - request.actorPlayerId ||
        !validAuthorityHash(ack.authoritativeStateHash))
        return false;

    if (request.actionId == rules::kBreakFoulBallInHandActionId) {
        return !ack.effects.rackRebuilt && ack.effects.breakerPlayerId == -1 &&
            ack.effects.currentPlayer == request.actorPlayerId &&
            ack.effects.nextPlayer == request.actorPlayerId &&
            ack.effects.ballInHand && ack.effects.ballInHandScope ==
                rules::BallInHandPlacementScope::BehindHeadString &&
            ack.effects.childDecisionPending;
    }
    const int expectedBreaker =
        request.actionId == rules::kBreakFoulRerackOpponentActionId ?
            request.actorPlayerId : 1 - request.actorPlayerId;
    return ack.effects.rackRebuilt &&
        ack.effects.breakerPlayerId == expectedBreaker &&
        ack.effects.currentPlayer == expectedBreaker &&
        ack.effects.nextPlayer == expectedBreaker &&
        !ack.effects.ballInHand && ack.effects.ballInHandScope ==
            rules::BallInHandPlacementScope::None &&
        !ack.effects.childDecisionPending &&
        ack.effects.phase == rules::MatchPhase::Break;
}

bool exactIllegalBreakAck(const rules::BreakFoulDecisionRequest& request,
                          const rules::BreakFoulDecisionAck& ack)
{
    if (!ack.accepted || ack.decisionId != request.decisionId ||
        ack.actionId != request.actionId ||
        ack.idempotencyKey != request.idempotencyKey ||
        ack.actorPlayerId != request.actorPlayerId ||
        ack.previousStateRevision != request.expectedAuthorityStateRevision ||
        ack.authoritativeStateRevision != ack.previousStateRevision + 1 ||
        ack.effects.branch != rules::BreakFoulBranch::IllegalBreak ||
        ack.effects.originalBreakerPlayerId != 1 - request.actorPlayerId ||
        !validAuthorityHash(ack.authoritativeStateHash) ||
        ack.effects.ballInHand || ack.effects.childDecisionPending)
        return false;

    if (request.actionId == rules::kBreakFoulAcceptTableActionId) {
        return ack.effects.tableAccepted && !ack.effects.rackRebuilt &&
            ack.effects.breakerPlayerId == -1 &&
            ack.effects.currentPlayer == request.actorPlayerId &&
            ack.effects.nextPlayer == request.actorPlayerId &&
            ack.effects.phase == rules::MatchPhase::OpenTable &&
            ack.effects.groups[0] == rules::BallGroup::Open &&
            ack.effects.groups[1] == rules::BallGroup::Open &&
            ack.effects.winner == -1;
    }
    const int expectedBreaker =
        request.actionId == rules::kBreakFoulRerackOpponentActionId ?
            request.actorPlayerId : 1 - request.actorPlayerId;
    return !ack.effects.tableAccepted && ack.effects.rackRebuilt &&
        ack.effects.breakerPlayerId == expectedBreaker &&
        ack.effects.currentPlayer == expectedBreaker &&
        ack.effects.nextPlayer == expectedBreaker &&
        ack.effects.phase == rules::MatchPhase::Break &&
        ack.effects.groups[0] == rules::BallGroup::Open &&
        ack.effects.groups[1] == rules::BallGroup::Open &&
        ack.effects.winner == -1;
}

bool exactNoRackContactMiscueAck(
    const rules::BreakFoulDecisionRequest& request,
    const rules::BreakFoulDecisionAck& ack)
{
    if (!ack.accepted || ack.decisionId != request.decisionId ||
        ack.actionId != request.actionId ||
        ack.idempotencyKey != request.idempotencyKey ||
        ack.actorPlayerId != request.actorPlayerId ||
        ack.previousStateRevision != request.expectedAuthorityStateRevision ||
        ack.authoritativeStateRevision != ack.previousStateRevision + 1 ||
        ack.effects.branch != rules::BreakFoulBranch::NoRackContactMiscue ||
        ack.effects.originalBreakerPlayerId != 1 - request.actorPlayerId ||
        !validAuthorityHash(ack.authoritativeStateHash) ||
        ack.effects.tableAccepted || ack.effects.ballInHand ||
        ack.effects.childDecisionPending || !ack.effects.rackRebuilt ||
        ack.effects.ballInHandScope !=
            rules::BallInHandPlacementScope::None ||
        ack.effects.phase != rules::MatchPhase::Break)
        return false;
    const int expectedBreaker =
        request.actionId == rules::kBreakFoulRerackOpponentActionId ?
            request.actorPlayerId : 1 - request.actorPlayerId;
    return (request.actionId == rules::kBreakFoulRerackOpponentActionId ||
            request.actionId == rules::kBreakFoulRerackOriginalActionId) &&
        ack.effects.breakerPlayerId == expectedBreaker &&
        ack.effects.currentPlayer == expectedBreaker &&
        ack.effects.nextPlayer == expectedBreaker;
}

bool samePlacementPosition(
    const rules::BallInHandPlacementPosition& first,
    const rules::BallInHandPlacementPosition& second)
{
    return first.worldX == second.worldX && first.worldZ == second.worldZ;
}

bool exactBallInHandPlacementAck(
    const rules::BallInHandPlacementRequest& request,
    const rules::BallInHandPlacementAck& ack)
{
    const auto expectedScope = request.flowId ==
            rules::kBehindHeadStringBallInHandFlowId ?
        rules::BallInHandPlacementScope::BehindHeadString :
        rules::BallInHandPlacementScope::FullTable;
    return ack.accepted && ack.decisionId == request.decisionId &&
        ack.actionId == request.actionId &&
        ack.idempotencyKey == request.idempotencyKey &&
        ack.actorPlayerId == request.actorPlayerId &&
        ack.previousStateRevision == request.expectedAuthorityStateRevision &&
        ack.authoritativeStateRevision == ack.previousStateRevision + 1 &&
        samePlacementPosition(ack.effects.cueBallPosition,
            request.position) &&
        ack.effects.ballInHand && ack.effects.placementPending &&
        ack.effects.ballInHandScope == expectedScope &&
        !ack.replacementPlacementDecisionId.empty() &&
        validAuthorityHash(ack.authoritativeStateHash);
}

bool exactBehindHeadStringRespotAck(
    const rules::BehindHeadStringRespotRequest& request,
    const rules::BehindHeadStringRespotAck& ack)
{
    return ack.accepted && ack.decisionId == request.decisionId &&
        ack.actionId == request.actionId &&
        ack.idempotencyKey == request.idempotencyKey &&
        ack.actorPlayerId == request.actorPlayerId &&
        ack.previousStateRevision == request.expectedAuthorityStateRevision &&
        ack.authoritativeStateRevision == ack.previousStateRevision + 1 &&
        validAuthorityHash(ack.authoritativeStateHash) &&
        !ack.replacementPlacementDecisionId.empty() &&
        (ack.respotCommitted != ack.tieChoiceStaged);
}

}  // namespace

ModernProductSession::ModernProductSession(GameRuntime& runtime,
    ai::AiPlayerController& aiPlayer, ModernProductSessionOptions options,
    audio::AudioCueConsumer* audioConsumer)
    : runtime_(runtime), aiPlayer_(aiPlayer), settings_(options.settingsPath),
      training_(runtime, options.executableVersion),
      input_(runtime, settings_, &training_, &replay_),
      shell_(runtime, settings_, input_, training_, replay_, &aiPlayer),
      executableVersion_(std::move(options.executableVersion)),
      fileDialogAdapter_(options.fileDialogAdapter),
      rulesDecisionUiImplementationEnabled_(
          options.internalCapabilities.count(
              kRulesDecisionUiImplementationCapability) != 0),
      rulesDecisionProductCapabilities_(
          options.rulesDecisionProductCapabilities),
      rulesDecisionLocalPlayerId_(options.rulesDecisionLocalPlayerId),
      rulesDecisionLocale_(options.rulesDecisionLocale)
{
    settingsStartup_ = settings_.start();
    shell_.setM4CameraCandidate(options.m4CameraCandidate);
    shell_.syncCameraPresentationPreferences();
    shell_.setManualInspectorPolicy(options.manualInspectors);
    if (audioConsumer != nullptr) {
        audioController_ = std::make_unique<audio::ProductAudioController>(
            *audioConsumer, settings_.current());
        applyAudioSettings();
    }
    if (!options.trainingManifestPath.empty()) {
        const training::TrainingProductResult loaded = training_.loadPack(
            options.trainingManifestPath, options.trainingCapabilities);
        if (!loaded.ok) trainingLoadError_ = loaded.errorCode;
    }
    const replay::ProductReplayContentRegistryResult matchBinding =
        replayContentRegistry_.addOfficialSingleRack(runtime_.physicsProfile());
    if (!matchBinding.ok && trainingLoadError_.empty())
        trainingLoadError_ = matchBinding.errorCode;
    if (trainingLoadError_.empty()) {
        for (const training::TrainingScenarioCard& card :
                training_.scenarios()) {
            const replay::ProductReplayContentRegistryResult added =
                replayContentRegistry_.addTrainingAttempt(
                    card.fileSha256,card.physicsProfile);
            if (!added.ok) {
                trainingLoadError_ = added.errorCode;
                break;
            }
        }
    }
    (void)shell_.openMainMenu();
    refreshAiGate();
    syncRulesDecisionAuthority();
}

const settings::ProductSettings&
ModernProductSession::effectiveAudioSettings() const
{
    return settings_.menu().isOpen() ? settings_.menu().preview() :
        settings_.current();
}

void ModernProductSession::captureAudioResult(
    const audio::ProductAudioResult& result)
{
    if (result.ok) {
        const audio::AudioConsumerStatus status =
            audioController_ == nullptr ? audio::AudioConsumerStatus{} :
                audioController_->snapshot().consumer;
        if (status.state != audio::AudioConsumerState::DeviceFailed &&
            status.state != audio::AudioConsumerState::AssetFailed)
            audioError_.clear();
        return;
    }
    ++audioFailureCount_;
    audioError_ = result.errorCode.empty() ?
        "audio_consumer_failed" : result.errorCode;
}

void ModernProductSession::applyAudioSettings()
{
    if (audioController_ == nullptr) return;
    captureAudioResult(audioController_->applySettings(
        effectiveAudioSettings()));
}

void ModernProductSession::startAudioLoops()
{
    if (audioController_ != nullptr)
        captureAudioResult(audioController_->startPersistentLoops(effectiveAudioSettings()));
}

void ModernProductSession::consumeAuthoritySteps()
{
    lastAudioConsumeMilliseconds_ = 0.0;
    const training::TrainingProductResult trainingResult =
        training_.consumeAuthoritySteps();
    if (!trainingResult.ok) lastError_ = trainingResult.errorCode;
    shell_.rememberTrainingCompletion();
    syncRulesDecisionAuthority();
    if (audioController_ == nullptr) return;
    const std::uint64_t generation = runtime_.eventJournalGeneration();
    if (!hasAudioJournalGeneration_ || generation != audioJournalGeneration_) {
        hasAudioJournalGeneration_ = true;
        audioJournalGeneration_ = generation;
        lastPhysicsStepAttemptSequence_ = 0;
    }
    for (const RuntimePhysicsStepRecord& record : runtime_.recentPhysicsSteps()) {
        if (record.journalGeneration != generation ||
            record.attemptSequence <= lastPhysicsStepAttemptSequence_)
            continue;
        lastPhysicsStepAttemptSequence_ = record.attemptSequence;
        if (!record.publishesEvents ||
            record.telemetry.stepStatus != PhysicsStepStatus::Succeeded)
            continue;
        audio::ProductAudioStep step;
        step.generation = generation;
        step.tick = record.tick;
        step.telemetry = record.telemetry;
        step.runtimeEvents = runtime_.events();
        const auto audioStart = std::chrono::steady_clock::now();
        const audio::ProductAudioResult result = audioController_->consumeStep(
            step, effectiveAudioSettings());
        captureAudioResult(result);
        lastAudioConsumeMilliseconds_ += std::chrono::duration<double, std::milli>(
            std::chrono::steady_clock::now() - audioStart).count();
        ++audioConsumedAuthorityTicks_;
    }
}

void ModernProductSession::beginMatchReplayRecording()
{
    matchReplayRecorder_=replay::Recorder{};
    matchReplayError_.clear();
    const replay::TrustedReplayContentBinding binding=
        replay::officialSingleRackReplayBinding(runtime_.physicsProfile());
    if(gameStateSha256(runtime_.state())!=binding.initialStateSha256||
       replay::replayAuthorityStateSha256(runtime_.state())!=
            binding.initialAuthorityStateSha256||
       runtime_.rulesAuthorityRevision()!=binding.initialRulesRevision||
       runtime_.rulesReplayEffectStateHash()!=
            binding.initialRulesEffectSha256){
        matchReplayError_="product_match_replay_initial_authority_mismatch";
        lastError_=matchReplayError_;
        return;
    }
    const replay::ValidationResult begun=matchReplayRecorder_.begin(
        runtime_,executableVersion_,binding.contentSha256,1);
    if(!begun.ok){
        matchReplayError_=begun.errorCode.empty()?
            "product_match_replay_begin_failed":begun.errorCode;
        lastError_=matchReplayError_;
    }
}

void ModernProductSession::captureMatchReplayProgress()
{
    if(!matchReplayRecorder_.active())return;
    const replay::ValidationResult captured=
        matchReplayRecorder_.captureApplied(runtime_);
    if(!captured.ok){
        matchReplayError_=(captured.errorCode=="replay_action_journal_reset"||
            captured.errorCode=="replay_rules_event_journal_reset")?
            "product_match_replay_cross_rack_unsupported":
            captured.errorCode;
        lastError_=matchReplayError_;
        return;
    }
    const bool terminal=runtime_.state().gameOver||
        runtime_.rulesRackTerminalState().active;
    if(!terminal||runtime_.state().ballsMoving)return;
    const replay::Document& document=matchReplayRecorder_.document();
    if((!document.actions.empty()&&
            document.actions.back().tick>=runtime_.tick())||
       (!document.rulesReceipts.empty()&&
            document.rulesReceipts.back().tick>=runtime_.tick()))
        return;
    const replay::ValidationResult finished=matchReplayRecorder_.finish(
        runtime_,matchReplayRecorder_.shotCount());
    if(!finished.ok&&
       finished.errorCode!="replay_final_tick_must_follow_actions"){
        matchReplayError_=finished.errorCode;
        lastError_=matchReplayError_;
    }
}

void ModernProductSession::discardRulesDecisionPlacementCamera() noexcept
{
    rulesDecisionPlacementSavedCamera_.reset();
    rulesDecisionPlacementSavedProjectionPan_.reset();
    rulesDecisionPlacementCameraDecisionId_.clear();
    rulesDecisionPlacementCameraJournalGeneration_ = 0;
}

void ModernProductSession::restoreRulesDecisionPlacementProjectionPan() noexcept
{
    if (!rulesDecisionPlacementSavedProjectionPan_) return;
    runtime_.mutableState().camera.panX =
        (*rulesDecisionPlacementSavedProjectionPan_)[0];
    runtime_.mutableState().camera.panY =
        (*rulesDecisionPlacementSavedProjectionPan_)[1];
    runtime_.mutableState().camera.panZ =
        (*rulesDecisionPlacementSavedProjectionPan_)[2];
    rulesDecisionPlacementSavedProjectionPan_.reset();
}

void ModernProductSession::syncRulesDecisionPlacementCamera(
    const std::string& activeChildDecisionId)
{
    if (!activeChildDecisionId.empty()) {
        if (rulesDecisionPlacementSavedCamera_ &&
            rulesDecisionPlacementCameraJournalGeneration_ !=
                runtime_.eventJournalGeneration()) {
            // A new rack/session owns its own presentation state. Never write
            // the previous rack's saved user pan or camera frame into it. A
            // reset or scenario replacement has already installed the new
            // generation's camera, which may intentionally be nonzero.
            discardRulesDecisionPlacementCamera();
        }
        if (!rulesDecisionPlacementSavedCamera_) {
            const auto placementOffer = runtime_.ballInHandPlacementOffer();
            const bool fullTablePlacement = placementOffer &&
                placementOffer->decisionId == activeChildDecisionId &&
                placementOffer->scope ==
                    rules::BallInHandPlacementScope::FullTable;
            rulesDecisionPlacementSavedCamera_ = runtime_.cameraFrame();
            rulesDecisionPlacementCameraJournalGeneration_ =
                runtime_.eventJournalGeneration();
            const ActionResult framed = runtime_.setCameraPresetInstant(
                camera::SemanticState::AimOverhead,
                camera::Preset::Overhead);
            if (!framed.ok) {
                discardRulesDecisionPlacementCamera();
                lastError_ = framed.errorCode;
                return;
            }
            if (fullTablePlacement) {
                // Dynamic TableCenter tracking owns the overhead target, so a
                // one-frame eye/target offset would decay before the modal is
                // usable. Canonicalize only the presentation projection pan
                // for this child; renderer and inverse pointer projection read
                // the same CameraPresentation. Restore the exact user pan on
                // exit and leave the accepted BehindHeadString view unchanged.
                auto& camera = runtime_.mutableState().camera;
                rulesDecisionPlacementSavedProjectionPan_ =
                    std::array<float, 3>{
                        camera.panX, camera.panY, camera.panZ};
                camera.panX = 20.0f;
                camera.panY = 0.0f;
                camera.panZ = 0.0f;
            }
        }
        // Replacement/respot child decision ids may advance while retaining
        // the same placement interaction. Do not reframe and erase the
        // user's subsequent orbit/zoom.
        rulesDecisionPlacementCameraDecisionId_ = activeChildDecisionId;
        return;
    }
    if (!rulesDecisionPlacementSavedCamera_) return;
    const camera::Frame saved = *rulesDecisionPlacementSavedCamera_;
    const bool sameSession = rulesDecisionPlacementCameraJournalGeneration_ ==
        runtime_.eventJournalGeneration();
    if (sameSession) {
        restoreRulesDecisionPlacementProjectionPan();
    }
    discardRulesDecisionPlacementCamera();
    if (!sameSession) return;
    const ActionResult restored = runtime_.restoreCameraFrame(saved);
    if (!restored.ok) lastError_ = restored.errorCode;
}

void ModernProductSession::syncRulesDecisionAuthority()
{
    captureMatchReplayProgress();
    rulesDecisionProductActive_ = false;
    const bool product06c =
        rulesDecisionProductCapabilities_.illegalBreakChoice &&
        shell_.screen() == ProductScreen::Match;
    const bool product06e =
        rulesDecisionProductCapabilities_.breakShotFoulChoice &&
        shell_.screen() == ProductScreen::Match;
    const bool product06f =
        rulesDecisionProductCapabilities_.eightOnBreakChoice &&
        shell_.screen() == ProductScreen::Match;
    const bool product06g =
        rulesDecisionProductCapabilities_.noRackContactMiscueChoice &&
        shell_.screen() == ProductScreen::Match;
    const bool productGroup =
        rulesDecisionProductCapabilities_.groupChoice &&
        shell_.screen() == ProductScreen::Match;
    const bool productFullTable =
        rulesDecisionProductCapabilities_.fullTableBallInHand &&
        shell_.screen() == ProductScreen::Match;
    const bool productRules = product06c || product06e || product06f ||
        product06g || productGroup || productFullTable;
    const auto respot = runtime_.behindHeadStringRespotOffer();
    const auto placement = runtime_.ballInHandPlacementOffer();
    std::string activePlacementChild;
    if (placement && productFullTable && placement->scope ==
            rules::BallInHandPlacementScope::FullTable &&
        rulesDecisionActorIsLocal(placement->actorPlayerId) &&
        placement->decisionId !=
            productFullTableRetainedPlacementDecisionId_)
        activePlacementChild = placement->decisionId;
    else if (placement && placement->scope ==
            rules::BallInHandPlacementScope::BehindHeadString &&
        rulesDecisionActorIsLocal(placement->actorPlayerId) &&
        ((product06e && !product06ePlacementDecisionId_.empty() &&
             placement->decisionId == product06ePlacementDecisionId_) ||
         (product06f && !product06fPlacementDecisionId_.empty() &&
             placement->decisionId == product06fPlacementDecisionId_)))
        activePlacementChild = placement->decisionId;
    else if (respot && rulesDecisionActorIsLocal(respot->actorPlayerId) &&
        ((product06e && !product06ePlacementDecisionId_.empty() &&
             respot->sourcePlacementDecisionId ==
                 product06ePlacementDecisionId_) ||
         (product06f && !product06fPlacementDecisionId_.empty() &&
             respot->sourcePlacementDecisionId ==
                 product06fPlacementDecisionId_)))
        activePlacementChild = respot->decisionId;
    syncRulesDecisionPlacementCamera(activePlacementChild);
    if (!rulesDecisionUiImplementationEnabled_ && !productRules) {
        rulesDecisionUi_.clear();
        return;
    }
    const auto exposeUnsupportedLocal = [&](auto&& offer) {
        if (!productRules || !rulesDecisionActorIsLocal(offer.actorPlayerId))
            return false;
        (void)rulesDecisionUi_.present(offer, offer.actorPlayerId);
        rulesDecisionUi_.authorityError(
            "rules_decision_product_branch_unavailable");
        rulesDecisionProductActive_ = true;
        return true;
    };
    const auto group = runtime_.groupChoiceDecisionOffer();
    if (group) {
        const std::vector<std::string> actions(
            group->offeredActionIds.begin(), group->offeredActionIds.end());
        if (!rulesDecisionUiImplementationEnabled_) {
            if (productGroup &&
                    rulesDecisionActorIsLocal(group->actorPlayerId)) {
                if (!sameRulesOffer(rulesDecisionUi_.snapshot(),
                        group->decisionId, group->authorityStateRevision,
                        group->actorPlayerId, actions))
                    (void)rulesDecisionUi_.present(
                        *group, group->actorPlayerId);
                rulesDecisionProductActive_ = true;
            } else {
                (void)exposeUnsupportedLocal(*group);
            }
            return;
        }
        if (!sameRulesOffer(rulesDecisionUi_.snapshot(), group->decisionId,
                group->authorityStateRevision, group->actorPlayerId, actions))
            (void)rulesDecisionUi_.present(
                *group, rulesDecisionLocalPlayerId_);
        return;
    }
    if (respot) {
        if (!rulesDecisionUiImplementationEnabled_) {
            const bool authorized06e = product06e &&
                rulesDecisionActorIsLocal(respot->actorPlayerId) &&
                !product06ePlacementDecisionId_.empty() &&
                respot->sourcePlacementDecisionId ==
                    product06ePlacementDecisionId_;
            const bool authorized06f = product06f &&
                rulesDecisionActorIsLocal(respot->actorPlayerId) &&
                !product06fPlacementDecisionId_.empty() &&
                respot->sourcePlacementDecisionId ==
                    product06fPlacementDecisionId_;
            if (authorized06e || authorized06f) {
                std::vector<std::string> actions;
                for (int ball : respot->tiedCandidateBallIds)
                    actions.push_back(
                        rules::ui::productRespotChoiceActionId(ball));
                if (!sameRulesOffer(rulesDecisionUi_.snapshot(),
                        respot->decisionId,
                        respot->authorityStateRevision,
                        respot->actorPlayerId, actions))
                    (void)rulesDecisionUi_.
                        presentProductBehindHeadStringRespot(
                            *respot, respot->actorPlayerId);
                rulesDecisionProductActive_ = true;
            } else {
                if (productRules &&
                    rulesDecisionActorIsLocal(respot->actorPlayerId)) {
                    (void)rulesDecisionUi_.
                        presentProductBehindHeadStringRespot(
                            *respot, respot->actorPlayerId);
                    rulesDecisionUi_.authorityError(
                        "rules_decision_product_branch_unavailable");
                    rulesDecisionProductActive_ = true;
                }
            }
            return;
        }
        (void)rulesDecisionUi_.presentProductBehindHeadStringRespot(
            *respot, rulesDecisionLocalPlayerId_);
        return;
    }
    if (placement) {
        if (!rulesDecisionUiImplementationEnabled_) {
            const bool committedFullTable = productFullTable &&
                rulesDecisionActorIsLocal(placement->actorPlayerId) &&
                placement->scope ==
                    rules::BallInHandPlacementScope::FullTable &&
                !productFullTableRetainedPlacementDecisionId_.empty() &&
                placement->decisionId ==
                    productFullTableRetainedPlacementDecisionId_;
            const bool committed06ePlacement = product06e &&
                rulesDecisionActorIsLocal(placement->actorPlayerId) &&
                placement->scope ==
                    rules::BallInHandPlacementScope::BehindHeadString &&
                !product06eRetainedPlacementDecisionId_.empty() &&
                placement->decisionId ==
                    product06eRetainedPlacementDecisionId_;
            const bool committed06fPlacement = product06f &&
                rulesDecisionActorIsLocal(placement->actorPlayerId) &&
                placement->scope ==
                    rules::BallInHandPlacementScope::BehindHeadString &&
                !product06fRetainedPlacementDecisionId_.empty() &&
                placement->decisionId ==
                    product06fRetainedPlacementDecisionId_;
            if (committed06ePlacement || committed06fPlacement ||
                    committedFullTable) {
                // Placement is complete, but GameRuntime intentionally keeps
                // a replacement entitlement until this actor's positive cue
                // impulse consumes it.  Do not re-open a blocking modal for
                // that retained authority state.
                rulesDecisionUi_.clear();
                return;
            }
            const bool authorizedFullTable = productFullTable &&
                rulesDecisionActorIsLocal(placement->actorPlayerId) &&
                placement->scope ==
                    rules::BallInHandPlacementScope::FullTable;
            const bool authorized06eChild = product06e &&
                rulesDecisionActorIsLocal(placement->actorPlayerId) &&
                placement->scope ==
                    rules::BallInHandPlacementScope::BehindHeadString &&
                !product06ePlacementDecisionId_.empty() &&
                placement->decisionId == product06ePlacementDecisionId_;
            const bool authorized06fChild = product06f &&
                rulesDecisionActorIsLocal(placement->actorPlayerId) &&
                placement->scope ==
                    rules::BallInHandPlacementScope::BehindHeadString &&
                !product06fPlacementDecisionId_.empty() &&
                placement->decisionId == product06fPlacementDecisionId_;
            if (authorizedFullTable) {
                const std::vector<std::string> actions(
                    placement->offeredActionIds.begin(),
                    placement->offeredActionIds.end());
                productFullTablePlacementDecisionId_ =
                    placement->decisionId;
                if (!sameRulesOffer(rulesDecisionUi_.snapshot(),
                        placement->decisionId,
                        placement->authorityStateRevision,
                        placement->actorPlayerId, actions))
                    (void)rulesDecisionUi_.present(
                        *placement, placement->actorPlayerId);
                rulesDecisionProductActive_ = true;
            } else if (authorized06eChild || authorized06fChild) {
                const std::vector<std::string> actions{
                    rules::kConfirmBehindHeadStringPlacementActionId,
                    rules::kRequestNearestLegalRespotActionId};
                if (!sameRulesOffer(rulesDecisionUi_.snapshot(),
                        placement->decisionId,
                        placement->authorityStateRevision,
                        placement->actorPlayerId, actions))
                    (void)rulesDecisionUi_.
                        presentProductBehindHeadStringPlacement(
                            *placement, placement->actorPlayerId);
                rulesDecisionProductActive_ = true;
            } else {
                (void)exposeUnsupportedLocal(*placement);
            }
            return;
        }
        const std::vector<std::string> actions(
            placement->offeredActionIds.begin(),
            placement->offeredActionIds.end());
        if (!sameRulesOffer(rulesDecisionUi_.snapshot(), placement->decisionId,
                placement->authorityStateRevision,
                placement->actorPlayerId, actions))
            (void)rulesDecisionUi_.present(
                *placement, rulesDecisionLocalPlayerId_);
        return;
    }
    const auto breakFoul = runtime_.breakFoulDecisionOffer();
    if (breakFoul) {
        if (!rulesDecisionUiImplementationEnabled_) {
            const bool authorized06c = product06c &&
                breakFoul->branch == rules::BreakFoulBranch::IllegalBreak &&
                breakFoul->sourceClause == rules::kIllegalBreakSourceClause &&
                rulesDecisionActorIsLocal(breakFoul->actorPlayerId);
            const bool authorized06e = product06e &&
                breakFoul->branch == rules::BreakFoulBranch::BreakShotFoul &&
                breakFoul->sourceClause ==
                    rules::kBreakShotFoulSourceClause &&
                rulesDecisionActorIsLocal(breakFoul->actorPlayerId);
            const bool authorized06g = product06g &&
                breakFoul->branch ==
                    rules::BreakFoulBranch::NoRackContactMiscue &&
                breakFoul->sourceClause ==
                    rules::kBreakNoRackContactSourceClause &&
                rulesDecisionActorIsLocal(breakFoul->actorPlayerId);
            if (authorized06c || authorized06e || authorized06g) {
                if (!sameRulesOffer(rulesDecisionUi_.snapshot(),
                        breakFoul->decisionId,
                        breakFoul->authorityStateRevision,
                        breakFoul->actorPlayerId,
                        breakFoul->offeredActionIds))
                    (void)rulesDecisionUi_.present(
                        *breakFoul, breakFoul->actorPlayerId);
                rulesDecisionProductActive_ = true;
            } else {
                (void)exposeUnsupportedLocal(*breakFoul);
            }
            return;
        }
        if (!sameRulesOffer(rulesDecisionUi_.snapshot(), breakFoul->decisionId,
                breakFoul->authorityStateRevision,
                breakFoul->actorPlayerId, breakFoul->offeredActionIds))
            (void)rulesDecisionUi_.present(
                *breakFoul, rulesDecisionLocalPlayerId_);
        return;
    }
    const auto eight = runtime_.eightOnBreakDecisionOffer();
    if (eight) {
        if (!rulesDecisionUiImplementationEnabled_) {
            if (product06f && rulesDecisionActorIsLocal(
                    eight->actorPlayerId)) {
                if (!sameRulesOffer(rulesDecisionUi_.snapshot(),
                        eight->decisionId, eight->authorityStateRevision,
                        eight->actorPlayerId, eight->offeredActionIds))
                    (void)rulesDecisionUi_.present(
                        *eight, eight->actorPlayerId);
                rulesDecisionProductActive_ = true;
            } else {
                (void)exposeUnsupportedLocal(*eight);
            }
            return;
        }
        if (!sameRulesOffer(rulesDecisionUi_.snapshot(), eight->decisionId,
                eight->authorityStateRevision, eight->actorPlayerId,
                eight->offeredActionIds))
            (void)rulesDecisionUi_.present(
                *eight, rulesDecisionLocalPlayerId_);
        return;
    }
    rulesDecisionUi_.clear();
}

bool ModernProductSession::rulesDecisionActorIsLocal(int actorPlayerId) const
{
    if (actorPlayerId < 0 || actorPlayerId > 1) return false;
    const ai::PlayerState ai = aiPlayer_.state();
    // With no configured AI this is a local two-player match, so either
    // authoritative actor may operate the decision. In single-player, the AI
    // actor remains remote and only the human opponent is interactive.
    return !ai.configuration.enabled || actorPlayerId != ai.configuration.player;
}

ModernProductEventResult
ModernProductSession::submitProductAuthorizedGroupChoice(
    const std::string& actionId, const std::string& idempotencyKey)
{
    ModernProductEventResult result;
    result.handled = true;
    auto reject = [&](const std::string& error) {
        result.ok = false;
        result.errorCode = error;
        return result;
    };
    if (shell_.screen() != ProductScreen::Match)
        return reject("rules_product_group_choice_not_in_match");
    if (actionId != rules::kGroupChoiceSolidsActionId &&
            actionId != rules::kGroupChoiceStripesActionId)
        return reject("rules_product_group_choice_action_not_authorized");

    const auto retained = productAuthorizedGroupChoiceRequests_.find(
        idempotencyKey);
    if (retained != productAuthorizedGroupChoiceRequests_.end()) {
        if (retained->second.actionId != actionId)
            return reject("rules_decision_idempotency_conflict");
        const rules::GroupChoiceDecisionResult response =
            runtime_.submitGroupChoiceDecision(retained->second);
        if (!response.ok) return reject(response.errorCode.empty() ?
            "rules_decision_context_mismatch" : response.errorCode);
        if (!exactGroupChoiceAck(retained->second, response.ack))
            return reject("rules_decision_ack_mismatch");
        syncRulesDecisionAuthority();
        refreshAiGate();
        return result;
    }

    const auto offer = runtime_.groupChoiceDecisionOffer();
    if (!offer) return reject("rules_group_choice_not_pending");
    if (!rulesDecisionActorIsLocal(offer->actorPlayerId))
        return reject("rules_decision_actor_mismatch");
    if (actionId != offer->offeredActionIds[0] &&
        actionId != offer->offeredActionIds[1])
        return reject("rules_decision_action_not_offered");

    rules::GroupChoiceDecisionRequest request;
    request.rulesetId = rules::kGroupChoiceRulesetId;
    request.capabilityId = rules::kGroupChoiceAuthorityCapabilityId;
    request.flowId = rules::kGroupChoiceAuthorityFlowId;
    request.decisionId = offer->decisionId;
    request.expectedAuthorityStateRevision =
        offer->authorityStateRevision;
    request.idempotencyKey = idempotencyKey;
    request.actorPlayerId = offer->actorPlayerId;
    request.actionId = actionId;
    request.payloadCanonical = "{}";
    const rules::GroupChoiceDecisionResult response =
        runtime_.submitGroupChoiceDecision(request);
    if (!response.ok) return reject(response.errorCode.empty() ?
        "rules_decision_context_mismatch" : response.errorCode);
    const bool exactAck = exactGroupChoiceAck(request, response.ack) &&
        response.ack.authoritativeStateHash ==
            runtime_.rulesAuthorityStateHash();
    if (!exactAck) return reject("rules_decision_ack_mismatch");
    productAuthorizedGroupChoiceRequests_.emplace(
        request.idempotencyKey, request);
    syncRulesDecisionAuthority();
    refreshAiGate();
    return result;
}

ModernProductEventResult
ModernProductSession::submitProductAuthorizedBreakFoulChoice(
    const std::string& actionId, const std::string& idempotencyKey,
    const std::string& payloadCanonical)
{
    ModernProductEventResult result;
    result.handled = true;
    auto reject = [&](const std::string& error) {
        result.ok = false;
        result.errorCode = error;
        return result;
    };
    if (!rulesDecisionProductCapabilities_.breakShotFoulChoice)
        return reject("rules_product_break_shot_foul_capability_disabled");
    if (shell_.screen() != ProductScreen::Match)
        return reject("rules_product_break_foul_not_in_match");
    if (actionId != rules::kBreakFoulBallInHandActionId &&
            actionId != rules::kBreakFoulRerackOpponentActionId &&
            actionId != rules::kBreakFoulRerackOriginalActionId)
        return reject("rules_product_break_foul_action_not_authorized");
    if (payloadCanonical != "{}")
        return reject("rules_decision_invalid_payload");
    if (idempotencyKey.empty() || idempotencyKey.size() > 128)
        return reject("rules_decision_idempotency_key_invalid");

    const auto retained = productAuthorizedBreakFoulRequests_.find(
        idempotencyKey);
    if (retained != productAuthorizedBreakFoulRequests_.end()) {
        if (retained->second.actionId != actionId ||
            retained->second.payloadCanonical != payloadCanonical)
            return reject("rules_decision_idempotency_conflict");
        const rules::BreakFoulDecisionResult response =
            runtime_.submitBreakFoulDecision(retained->second);
        if (!response.ok) return reject(response.errorCode.empty() ?
            "rules_decision_context_mismatch" : response.errorCode);
        if (!exactBreakFoulAck(retained->second, response.ack))
            return reject("rules_decision_ack_mismatch");
        syncRulesDecisionAuthority();
        refreshAiGate();
        return result;
    }

    const auto offer = runtime_.breakFoulDecisionOffer();
    if (!offer) return reject("rules_break_foul_not_pending");
    const std::vector<std::string> expectedActions{
        rules::kBreakFoulBallInHandActionId,
        rules::kBreakFoulRerackOpponentActionId,
        rules::kBreakFoulRerackOriginalActionId};
    if (offer->branch != rules::BreakFoulBranch::BreakShotFoul ||
        offer->sourceClause != rules::kBreakShotFoulSourceClause ||
        offer->actorPlayerId != 1 - offer->originalBreakerPlayerId ||
        offer->frozenNextPlayer != offer->actorPlayerId ||
        offer->offeredActionIds != expectedActions)
        return reject("rules_product_break_foul_context_not_authorized");
    if (!rulesDecisionActorIsLocal(offer->actorPlayerId))
        return reject("rules_decision_actor_mismatch");
    if (std::find(expectedActions.begin(), expectedActions.end(), actionId) ==
        expectedActions.end())
        return reject("rules_decision_action_not_offered");
    const std::string frozenStateHash = runtime_.rulesAuthorityStateHash();
    if (!validAuthorityHash(frozenStateHash))
        return reject("rules_product_break_foul_state_hash_invalid");

    rules::BreakFoulDecisionRequest request;
    request.rulesetId = rules::kBreakFoulRulesetId;
    request.capabilityId = rules::kBreakFoulCapabilityId;
    request.flowId = rules::kBreakFoulFlowId;
    request.decisionId = offer->decisionId;
    request.expectedAuthorityStateRevision = offer->authorityStateRevision;
    request.idempotencyKey = idempotencyKey;
    request.actorPlayerId = offer->actorPlayerId;
    request.actionId = actionId;
    request.payloadCanonical = payloadCanonical;
    const rules::BreakFoulDecisionResult response =
        runtime_.submitBreakFoulDecision(request);
    if (!response.ok) return reject(response.errorCode.empty() ?
        "rules_decision_context_mismatch" : response.errorCode);
    if (!exactBreakFoulAck(request, response.ack) ||
        response.ack.authoritativeStateHash !=
            runtime_.rulesAuthorityStateHash())
        return reject("rules_decision_ack_mismatch");
    if (actionId == rules::kBreakFoulBallInHandActionId) {
        const auto placement = runtime_.ballInHandPlacementOffer();
        if (!placement || placement->actorPlayerId != request.actorPlayerId ||
            placement->scope !=
                rules::BallInHandPlacementScope::BehindHeadString ||
            placement->offeredActionIds[0] !=
                rules::kConfirmBehindHeadStringPlacementActionId)
            return reject("rules_decision_ack_mismatch");
        product06ePlacementDecisionId_ = placement->decisionId;
        product06eRetainedPlacementDecisionId_.clear();
    } else {
        product06ePlacementDecisionId_.clear();
        product06eRetainedPlacementDecisionId_.clear();
    }
    productAuthorizedBreakFoulRequests_.emplace(
        request.idempotencyKey, request);
    syncRulesDecisionAuthority();
    refreshAiGate();
    return result;
}

ModernProductEventResult
ModernProductSession::submitProductAuthorizedIllegalBreakChoice(
    const std::string& actionId, const std::string& idempotencyKey,
    const std::string& payloadCanonical)
{
    ModernProductEventResult result;
    result.handled = true;
    auto reject = [&](const std::string& error) {
        result.ok = false;
        result.errorCode = error;
        return result;
    };
    if (!rulesDecisionProductCapabilities_.illegalBreakChoice)
        return reject("rules_product_illegal_break_capability_disabled");
    if (shell_.screen() != ProductScreen::Match)
        return reject("rules_product_illegal_break_not_in_match");
    const std::vector<std::string> expectedActions{
        rules::kBreakFoulAcceptTableActionId,
        rules::kBreakFoulRerackOpponentActionId,
        rules::kBreakFoulRerackOriginalActionId};
    if (std::find(expectedActions.begin(), expectedActions.end(), actionId) ==
            expectedActions.end())
        return reject("rules_product_illegal_break_action_not_authorized");
    if (payloadCanonical != "{}")
        return reject("rules_decision_invalid_payload");
    if (idempotencyKey.empty() || idempotencyKey.size() > 128)
        return reject("rules_decision_idempotency_key_invalid");

    const auto retained = productAuthorizedIllegalBreakRequests_.find(
        idempotencyKey);
    if (retained != productAuthorizedIllegalBreakRequests_.end()) {
        if (retained->second.actionId != actionId ||
            retained->second.payloadCanonical != payloadCanonical)
            return reject("rules_decision_idempotency_conflict");
        const auto response = runtime_.submitBreakFoulDecision(retained->second);
        if (!response.ok) return reject(response.errorCode.empty() ?
            "rules_decision_context_mismatch" : response.errorCode);
        if (!exactIllegalBreakAck(retained->second, response.ack))
            return reject("rules_decision_ack_mismatch");
        syncRulesDecisionAuthority();
        refreshAiGate();
        return result;
    }

    const auto offer = runtime_.breakFoulDecisionOffer();
    if (!offer) return reject("rules_break_foul_not_pending");
    if (offer->branch != rules::BreakFoulBranch::IllegalBreak ||
        offer->sourceClause != rules::kIllegalBreakSourceClause ||
        offer->sourcePdfPage <= 0 ||
        offer->actorPlayerId != 1 - offer->originalBreakerPlayerId ||
        offer->frozenNextPlayer != offer->actorPlayerId ||
        offer->offeredActionIds != expectedActions)
        return reject("rules_product_illegal_break_context_not_authorized");
    if (!rulesDecisionActorIsLocal(offer->actorPlayerId))
        return reject("rules_decision_actor_mismatch");
    const std::string frozenStateHash = runtime_.rulesAuthorityStateHash();
    if (!validAuthorityHash(frozenStateHash))
        return reject("rules_product_illegal_break_state_hash_invalid");

    rules::BreakFoulDecisionRequest request;
    request.rulesetId = rules::kBreakFoulRulesetId;
    request.capabilityId = rules::kBreakFoulCapabilityId;
    request.flowId = rules::kBreakFoulFlowId;
    request.decisionId = offer->decisionId;
    request.expectedAuthorityStateRevision = offer->authorityStateRevision;
    request.idempotencyKey = idempotencyKey;
    request.actorPlayerId = offer->actorPlayerId;
    request.actionId = actionId;
    request.payloadCanonical = payloadCanonical;
    const auto response = runtime_.submitBreakFoulDecision(request);
    if (!response.ok) return reject(response.errorCode.empty() ?
        "rules_decision_context_mismatch" : response.errorCode);
    if (!exactIllegalBreakAck(request, response.ack) ||
        response.ack.authoritativeStateHash != runtime_.rulesAuthorityStateHash())
        return reject("rules_decision_ack_mismatch");
    productAuthorizedIllegalBreakRequests_.emplace(idempotencyKey, request);
    syncRulesDecisionAuthority();
    refreshAiGate();
    return result;
}

ModernProductEventResult
ModernProductSession::submitProductAuthorizedNoRackContactMiscueChoice(
    const std::string& actionId, const std::string& idempotencyKey,
    const std::string& payloadCanonical)
{
    ModernProductEventResult result;
    result.handled = true;
    auto reject = [&](const std::string& error) {
        result.ok = false;
        result.errorCode = error;
        return result;
    };
    if (!rulesDecisionProductCapabilities_.noRackContactMiscueChoice)
        return reject("rules_product_no_rack_miscue_capability_disabled");
    if (shell_.screen() != ProductScreen::Match)
        return reject("rules_product_no_rack_miscue_not_in_match");
    const std::vector<std::string> expectedActions{
        rules::kBreakFoulRerackOpponentActionId,
        rules::kBreakFoulRerackOriginalActionId};
    if (std::find(expectedActions.begin(), expectedActions.end(), actionId) ==
            expectedActions.end())
        return reject("rules_product_no_rack_miscue_action_not_authorized");
    if (payloadCanonical != "{}")
        return reject("rules_decision_invalid_payload");
    if (idempotencyKey.empty() || idempotencyKey.size() > 128)
        return reject("rules_decision_idempotency_key_invalid");

    const auto retained =
        productAuthorizedNoRackContactMiscueRequests_.find(idempotencyKey);
    if (retained != productAuthorizedNoRackContactMiscueRequests_.end()) {
        if (retained->second.actionId != actionId ||
            retained->second.payloadCanonical != payloadCanonical)
            return reject("rules_decision_idempotency_conflict");
        const auto response = runtime_.submitBreakFoulDecision(
            retained->second);
        if (!response.ok) return reject(response.errorCode.empty() ?
            "rules_decision_context_mismatch" : response.errorCode);
        if (!exactNoRackContactMiscueAck(retained->second, response.ack))
            return reject("rules_decision_ack_mismatch");
        syncRulesDecisionAuthority();
        refreshAiGate();
        return result;
    }

    const auto offer = runtime_.breakFoulDecisionOffer();
    if (!offer) return reject("rules_break_foul_not_pending");
    if (offer->branch != rules::BreakFoulBranch::NoRackContactMiscue ||
        offer->sourceClause != rules::kBreakNoRackContactSourceClause ||
        offer->sourcePdfPage != 8 ||
        offer->actorPlayerId != 1 - offer->originalBreakerPlayerId ||
        offer->frozenNextPlayer != offer->actorPlayerId ||
        offer->offeredActionIds != expectedActions)
        return reject("rules_product_no_rack_miscue_context_not_authorized");
    if (!rulesDecisionActorIsLocal(offer->actorPlayerId))
        return reject("rules_decision_actor_mismatch");
    const std::string frozenStateHash = runtime_.rulesAuthorityStateHash();
    if (!validAuthorityHash(frozenStateHash))
        return reject("rules_product_no_rack_miscue_state_hash_invalid");

    rules::BreakFoulDecisionRequest request;
    request.rulesetId = rules::kBreakFoulRulesetId;
    request.capabilityId = rules::kBreakFoulCapabilityId;
    request.flowId = rules::kBreakFoulFlowId;
    request.decisionId = offer->decisionId;
    request.expectedAuthorityStateRevision = offer->authorityStateRevision;
    request.idempotencyKey = idempotencyKey;
    request.actorPlayerId = offer->actorPlayerId;
    request.actionId = actionId;
    request.payloadCanonical = payloadCanonical;
    const auto response = runtime_.submitBreakFoulDecision(request);
    if (!response.ok) return reject(response.errorCode.empty() ?
        "rules_decision_context_mismatch" : response.errorCode);
    if (!exactNoRackContactMiscueAck(request, response.ack) ||
        response.ack.authoritativeStateHash != runtime_.rulesAuthorityStateHash())
        return reject("rules_decision_ack_mismatch");
    productAuthorizedNoRackContactMiscueRequests_.emplace(
        idempotencyKey, request);
    syncRulesDecisionAuthority();
    refreshAiGate();
    return result;
}

ModernProductEventResult
ModernProductSession::previewProductAuthorizedBallInHandPlacement(
    const rules::BallInHandPlacementPosition& position)
{
    ModernProductEventResult result;
    result.handled = true;
    auto reject = [&](const std::string& error) {
        productAuthorizedPlacementPreview_.reset();
        result.ok = false;
        result.errorCode = error;
        return result;
    };
    if (shell_.screen() != ProductScreen::Match)
        return reject("rules_product_placement_not_in_match");
    const auto offer = runtime_.ballInHandPlacementOffer();
    if (!offer) return reject("rules_placement_not_pending");
    if (!rulesDecisionActorIsLocal(offer->actorPlayerId))
        return reject("rules_decision_actor_mismatch");
    const bool behind = offer->scope ==
        rules::BallInHandPlacementScope::BehindHeadString;
    const bool fullTable = offer->scope ==
        rules::BallInHandPlacementScope::FullTable;
    if (!behind && !fullTable)
        return reject("rules_decision_context_mismatch");
    const std::string expectedAction = behind ?
        rules::kConfirmBehindHeadStringPlacementActionId :
        rules::kConfirmFullTablePlacementActionId;
    if (offer->offeredActionIds[0] != expectedAction)
        return reject("rules_decision_context_mismatch");
    const std::string authorityHash = runtime_.rulesAuthorityStateHash();
    if (!validAuthorityHash(authorityHash))
        return reject("rules_product_placement_state_hash_invalid");
    const rules::BallInHandPlacementValidation validation =
        runtime_.validateBallInHandPlacement(position);
    if (!validation.ok) return reject(validation.errorCode.empty() ?
        "rules_product_placement_preview_rejected" : validation.errorCode);

    ProductAuthorizedPlacementPreview preview;
    preview.decisionId = offer->decisionId;
    preview.authorityStateRevision = offer->authorityStateRevision;
    preview.authorityStateHash = authorityHash;
    preview.actorPlayerId = offer->actorPlayerId;
    preview.scope = offer->scope;
    preview.actionId = expectedAction;
    preview.position = validation.authoritativePosition;
    productAuthorizedPlacementPreview_ = std::move(preview);
    return result;
}

ModernProductEventResult
ModernProductSession::confirmProductAuthorizedBallInHandPlacement(
    const std::string& actionId,
    const rules::BallInHandPlacementPosition& position,
    const std::string& idempotencyKey)
{
    ModernProductEventResult result;
    result.handled = true;
    auto reject = [&](const std::string& error) {
        result.ok = false;
        result.errorCode = error;
        return result;
    };
    if (shell_.screen() != ProductScreen::Match)
        return reject("rules_product_placement_not_in_match");
    if (actionId != rules::kConfirmFullTablePlacementActionId &&
            actionId != rules::kConfirmBehindHeadStringPlacementActionId)
        return reject("rules_product_placement_action_not_authorized");

    const auto retained = productAuthorizedPlacementRequests_.find(
        idempotencyKey);
    if (retained != productAuthorizedPlacementRequests_.end()) {
        if (retained->second.actionId != actionId ||
            !samePlacementPosition(retained->second.position, position))
            return reject("rules_decision_idempotency_conflict");
        const rules::BallInHandPlacementResult response =
            runtime_.submitBallInHandPlacement(retained->second);
        if (!response.ok) return reject(response.errorCode.empty() ?
            "rules_decision_context_mismatch" : response.errorCode);
        if (!exactBallInHandPlacementAck(retained->second, response.ack))
            return reject("rules_decision_ack_mismatch");
        syncRulesDecisionAuthority();
        refreshAiGate();
        return result;
    }

    const auto offer = runtime_.ballInHandPlacementOffer();
    if (!offer) return reject("rules_placement_not_pending");
    if (!rulesDecisionActorIsLocal(offer->actorPlayerId))
        return reject("rules_decision_actor_mismatch");
    if (offer->offeredActionIds[0] != actionId)
        return reject("rules_product_placement_action_not_authorized");
    if (!productAuthorizedPlacementPreview_)
        return reject("rules_product_placement_preview_not_current");
    const ProductAuthorizedPlacementPreview& preview =
        *productAuthorizedPlacementPreview_;
    if (preview.decisionId != offer->decisionId ||
        preview.authorityStateRevision != offer->authorityStateRevision ||
        preview.actorPlayerId != offer->actorPlayerId ||
        preview.scope != offer->scope || preview.actionId != actionId ||
        !samePlacementPosition(preview.position, position))
        return reject("rules_product_placement_preview_not_current");
    if (runtime_.rulesAuthorityStateHash() != preview.authorityStateHash)
        return reject("rules_product_placement_preview_state_changed");

    rules::BallInHandPlacementRequest request;
    request.rulesetId = rules::kBallInHandRulesetId;
    request.capabilityId = rules::kFreeBallPlacementCapabilityId;
    request.flowId = offer->scope ==
            rules::BallInHandPlacementScope::BehindHeadString ?
        rules::kBehindHeadStringBallInHandFlowId :
        rules::kFullTableBallInHandFlowId;
    request.decisionId = offer->decisionId;
    request.expectedAuthorityStateRevision = offer->authorityStateRevision;
    request.idempotencyKey = idempotencyKey;
    request.actorPlayerId = offer->actorPlayerId;
    request.actionId = actionId;
    request.position = preview.position;
    const rules::BallInHandPlacementResult response =
        runtime_.submitBallInHandPlacement(request);
    if (!response.ok) return reject(response.errorCode.empty() ?
        "rules_decision_context_mismatch" : response.errorCode);
    if (!exactBallInHandPlacementAck(request, response.ack) ||
        response.ack.authoritativeStateHash !=
            runtime_.rulesAuthorityStateHash())
        return reject("rules_decision_ack_mismatch");
    productAuthorizedPlacementRequests_.emplace(
        request.idempotencyKey, request);
    if (!product06ePlacementDecisionId_.empty() &&
        request.decisionId == product06ePlacementDecisionId_) {
        product06eRetainedPlacementDecisionId_ =
            response.ack.replacementPlacementDecisionId;
        product06ePlacementDecisionId_.clear();
    }
    if (!product06fPlacementDecisionId_.empty() &&
        request.decisionId == product06fPlacementDecisionId_) {
        product06fRetainedPlacementDecisionId_ =
            response.ack.replacementPlacementDecisionId;
        product06fPlacementDecisionId_.clear();
    }
    if (!productFullTablePlacementDecisionId_.empty() &&
        request.decisionId == productFullTablePlacementDecisionId_) {
        productFullTableRetainedPlacementDecisionId_ =
            response.ack.replacementPlacementDecisionId;
        productFullTablePlacementDecisionId_.clear();
    }
    productAuthorizedPlacementPreview_.reset();
    syncRulesDecisionAuthority();
    refreshAiGate();
    return result;
}

ModernProductEventResult
ModernProductSession::submitProductAuthorizedEightOnBreakChoice(
    const std::string& actionId, const std::string& idempotencyKey,
    const std::string& payloadCanonical)
{
    ModernProductEventResult result;
    result.handled = true;
    auto reject = [&](const std::string& error) {
        result.ok = false;
        result.errorCode = error;
        return result;
    };
    if (!rulesDecisionProductCapabilities_.eightOnBreakChoice)
        return reject("rules_product_eight_on_break_capability_disabled");
    if (shell_.screen() != ProductScreen::Match)
        return reject("rules_product_respot_not_in_match");
    if (actionId != rules::kEightOnBreakSpotContinueActionId &&
            actionId != rules::kEightOnBreakSpotBallInHandActionId &&
            actionId != rules::kEightOnBreakRerackOpponentActionId &&
            actionId != rules::kEightOnBreakRerackOriginalActionId)
        return reject("rules_product_respot_action_not_authorized");
    if (payloadCanonical != "{}")
        return reject("rules_decision_invalid_payload");
    if (idempotencyKey.empty() || idempotencyKey.size() > 128)
        return reject("rules_decision_idempotency_key_invalid");

    const auto retained = productAuthorizedEightOnBreakRequests_.find(
        idempotencyKey);
    if (retained != productAuthorizedEightOnBreakRequests_.end()) {
        if (retained->second.actionId != actionId ||
            retained->second.payloadCanonical != payloadCanonical)
            return reject("rules_decision_idempotency_conflict");
        const rules::EightOnBreakDecisionResult response =
            runtime_.submitEightOnBreakDecision(retained->second);
        if (!response.ok) return reject(response.errorCode.empty() ?
            "rules_decision_context_mismatch" : response.errorCode);
        if (!exactEightOnBreakAck(retained->second, response.ack))
            return reject("rules_decision_ack_mismatch");
        syncRulesDecisionAuthority();
        refreshAiGate();
        return result;
    }

    const auto offer = runtime_.eightOnBreakDecisionOffer();
    if (!offer) return reject("rules_eight_on_break_not_pending");
    const bool legal =
        offer->branch == rules::EightOnBreakBranch::LegalBreak;
    const std::vector<std::string> expectedActions = legal ?
        std::vector<std::string>{
            rules::kEightOnBreakSpotContinueActionId,
            rules::kEightOnBreakRerackOriginalActionId} :
        std::vector<std::string>{
            rules::kEightOnBreakSpotBallInHandActionId,
            rules::kEightOnBreakRerackOpponentActionId,
            rules::kEightOnBreakRerackOriginalActionId};
    const int expectedActor = legal ? offer->originalBreakerPlayerId :
        1 - offer->originalBreakerPlayerId;
    if (offer->actorPlayerId != expectedActor ||
        offer->frozenNextPlayer != expectedActor ||
        offer->frozenContinueTurn != legal ||
        offer->offeredActionIds != expectedActions)
        return reject("rules_product_respot_context_not_authorized");
    if (!rulesDecisionActorIsLocal(offer->actorPlayerId))
        return reject("rules_decision_actor_mismatch");
    if (std::find(expectedActions.begin(), expectedActions.end(), actionId) ==
        expectedActions.end())
        return reject("rules_decision_action_not_offered");
    const std::string frozenStateHash = runtime_.rulesAuthorityStateHash();
    if (!validAuthorityHash(frozenStateHash))
        return reject("rules_product_respot_state_hash_invalid");

    rules::EightOnBreakDecisionRequest request;
    request.rulesetId = rules::kEightOnBreakRulesetId;
    request.capabilityId = rules::kEightOnBreakCapabilityId;
    request.flowId = rules::kEightOnBreakFlowId;
    request.decisionId = offer->decisionId;
    request.expectedAuthorityStateRevision = offer->authorityStateRevision;
    request.idempotencyKey = idempotencyKey;
    request.actorPlayerId = offer->actorPlayerId;
    request.actionId = actionId;
    request.payloadCanonical = payloadCanonical;
    const rules::EightOnBreakDecisionResult response =
        runtime_.submitEightOnBreakDecision(request);
    if (!response.ok) return reject(response.errorCode.empty() ?
        "rules_decision_context_mismatch" : response.errorCode);
    if (!exactEightOnBreakAck(request, response.ack) ||
        response.ack.effects.branch != offer->branch ||
        response.ack.authoritativeStateHash !=
            runtime_.rulesAuthorityStateHash())
        return reject("rules_decision_ack_mismatch");

    const bool foulSpot =
        actionId == rules::kEightOnBreakSpotBallInHandActionId;
    const auto placement = runtime_.ballInHandPlacementOffer();
    if (foulSpot) {
        if (!runtime_.matchState().ballInHand ||
            runtime_.ballInHandPlacementScope() !=
                rules::BallInHandPlacementScope::BehindHeadString ||
            !placement || placement->actorPlayerId != offer->actorPlayerId ||
            placement->scope !=
                rules::BallInHandPlacementScope::BehindHeadString ||
            placement->offeredActionIds[0] !=
                rules::kConfirmBehindHeadStringPlacementActionId)
            return reject("rules_decision_ack_mismatch");
        product06fPlacementDecisionId_ = placement->decisionId;
        product06fRetainedPlacementDecisionId_.clear();
    } else if (runtime_.matchState().ballInHand || placement ||
            runtime_.ballInHandPlacementScope() !=
                rules::BallInHandPlacementScope::None) {
        return reject("rules_decision_ack_mismatch");
    } else {
        product06fPlacementDecisionId_.clear();
        product06fRetainedPlacementDecisionId_.clear();
    }
    productAuthorizedEightOnBreakRequests_.emplace(
        request.idempotencyKey, request);
    syncRulesDecisionAuthority();
    refreshAiGate();
    return result;
}

ModernProductEventResult
ModernProductSession::submitProductAuthorizedEightOnBreakRespot(
    const std::string& actionId, const std::string& idempotencyKey,
    const std::string& payloadCanonical)
{
    return submitProductAuthorizedEightOnBreakChoice(
        actionId, idempotencyKey, payloadCanonical);
}

ModernProductEventResult
ModernProductSession::submitProductAuthorizedBehindHeadStringRespot(
    const std::string& actionId, int selectedBallId,
    const std::string& idempotencyKey,
    const std::string& payloadCanonical)
{
    ModernProductEventResult result;
    result.handled = true;
    auto reject = [&](const std::string& error) {
        result.ok = false; result.errorCode = error; return result;
    };
    if (shell_.screen() != ProductScreen::Match)
        return reject("rules_product_head_string_respot_not_in_match");
    if (actionId != rules::kRequestNearestLegalRespotActionId &&
        actionId != rules::kChooseEquidistantRespotBallActionId)
        return reject("rules_product_head_string_respot_action_not_authorized");
    if (idempotencyKey.empty() || idempotencyKey.size() > 128)
        return reject("rules_decision_idempotency_key_invalid");
    const auto retained = productAuthorizedBehindHeadStringRespotRequests_.find(
        idempotencyKey);
    if (retained != productAuthorizedBehindHeadStringRespotRequests_.end()) {
        if (retained->second.actionId != actionId ||
            retained->second.selectedBallId != selectedBallId ||
            retained->second.payloadCanonical != payloadCanonical)
            return reject("rules_decision_idempotency_conflict");
        const auto response = runtime_.submitBehindHeadStringRespot(
            retained->second);
        if (!response.ok) return reject(response.errorCode);
        if (!exactBehindHeadStringRespotAck(retained->second, response.ack))
            return reject("rules_decision_ack_mismatch");
        syncRulesDecisionAuthority(); refreshAiGate(); return result;
    }

    rules::BehindHeadStringRespotRequest request;
    request.expectedAuthorityStateRevision = runtime_.rulesAuthorityRevision();
    request.expectedAuthorityStateHash = runtime_.rulesAuthorityStateHash();
    request.idempotencyKey = idempotencyKey;
    request.actionId = actionId;
    request.selectedBallId = selectedBallId;
    request.payloadCanonical = payloadCanonical;
    bool product06eChain = false;
    bool product06fChain = false;
    if (actionId == rules::kRequestNearestLegalRespotActionId) {
        const auto offer = runtime_.ballInHandPlacementOffer();
        if (!offer || offer->scope !=
                rules::BallInHandPlacementScope::BehindHeadString)
            return reject("rules_ball_in_hand_behind_head_string_not_pending");
        if (!rulesDecisionActorIsLocal(offer->actorPlayerId))
            return reject("rules_decision_actor_mismatch");
        if (selectedBallId != -1 || payloadCanonical != "{}")
            return reject("rules_decision_invalid_payload");
        request.decisionId = offer->decisionId;
        request.expectedAuthorityStateRevision = offer->authorityStateRevision;
        request.actorPlayerId = offer->actorPlayerId;
        product06eChain = !product06ePlacementDecisionId_.empty() &&
            offer->decisionId == product06ePlacementDecisionId_;
        product06fChain = !product06fPlacementDecisionId_.empty() &&
            offer->decisionId == product06fPlacementDecisionId_;
    } else {
        const auto offer = runtime_.behindHeadStringRespotOffer();
        if (!offer)
            return reject("rules_head_string_respot_choice_not_pending");
        if (!rulesDecisionActorIsLocal(offer->actorPlayerId))
            return reject("rules_decision_actor_mismatch");
        if (std::find(offer->tiedCandidateBallIds.begin(),
                offer->tiedCandidateBallIds.end(), selectedBallId) ==
            offer->tiedCandidateBallIds.end())
            return reject("rules_decision_action_not_offered");
        std::ostringstream expected;
        expected << "{\"selected_ball_id\":" << selectedBallId << '}';
        if (payloadCanonical != expected.str())
            return reject("rules_decision_invalid_payload");
        request.decisionId = offer->decisionId;
        request.expectedAuthorityStateRevision = offer->authorityStateRevision;
        request.actorPlayerId = offer->actorPlayerId;
        product06eChain = !product06ePlacementDecisionId_.empty() &&
            offer->sourcePlacementDecisionId ==
                product06ePlacementDecisionId_;
        product06fChain = !product06fPlacementDecisionId_.empty() &&
            offer->sourcePlacementDecisionId ==
                product06fPlacementDecisionId_;
    }
    const auto response = runtime_.submitBehindHeadStringRespot(request);
    if (!response.ok) return reject(response.errorCode.empty() ?
        "rules_decision_context_mismatch" : response.errorCode);
    if (!exactBehindHeadStringRespotAck(request, response.ack) ||
        response.ack.authoritativeStateHash != runtime_.rulesAuthorityStateHash())
        return reject("rules_decision_ack_mismatch");
    productAuthorizedBehindHeadStringRespotRequests_.emplace(
        idempotencyKey, request);
    const auto replacement = response.ack.replacementPlacementDecisionId;
    if (product06eChain)
        product06ePlacementDecisionId_ = replacement;
    if (product06fChain)
        product06fPlacementDecisionId_ = replacement;
    productAuthorizedPlacementPreview_.reset();
    syncRulesDecisionAuthority(); refreshAiGate(); return result;
}

ModernProductEventResult ModernProductSession::armRulesDecisionAction(
    const std::string& actionId)
{
    ModernProductEventResult result;
    result.handled = true;
    if (!rulesDecisionUiImplementationEnabled_) {
        result.ok = false;
        result.errorCode = "rules_decision_ui_capability_disabled";
        return result;
    }
    syncRulesDecisionAuthority();
    if (!rulesDecisionUi_.armAction(actionId)) {
        result.ok = false;
        result.errorCode = "rules_decision_ui_action_not_armable";
    }
    return result;
}

ModernProductEventResult ModernProductSession::previewRulesDecisionPlacement(
    const rules::BallInHandPlacementPosition& position)
{
    ModernProductEventResult result;
    result.handled = true;
    if (!rulesDecisionUiImplementationEnabled_) {
        result.ok = false;
        result.errorCode = "rules_decision_ui_capability_disabled";
        return result;
    }
    syncRulesDecisionAuthority();
    const auto offer = runtime_.ballInHandPlacementOffer();
    if (!offer || offer->decisionId !=
            rulesDecisionUi_.snapshot().offer.decisionId) {
        result.ok = false;
        result.errorCode = "rules_placement_not_pending";
        return result;
    }
    const rules::BallInHandPlacementValidation validation =
        runtime_.validateBallInHandPlacement(position);
    if (!rulesDecisionUi_.setPlacementPreview(position, validation,
            offer->decisionId, offer->authorityStateRevision)) {
        result.ok = false;
        result.errorCode = validation.errorCode.empty() ?
            "rules_decision_ui_preview_rejected" : validation.errorCode;
        return result;
    }
    // A legal/illegal preview is successfully represented; legality controls
    // Confirm and is not itself a transport failure.
    return result;
}

ModernProductEventResult ModernProductSession::confirmRulesDecisionAction(
    const std::string& idempotencyKey)
{
    ModernProductEventResult result;
    result.handled = true;
    if (!rulesDecisionUiImplementationEnabled_) {
        result.ok = false;
        result.errorCode = "rules_decision_ui_capability_disabled";
        return result;
    }
    syncRulesDecisionAuthority();
    const auto intent = rulesDecisionUi_.confirmArmedAction(idempotencyKey);
    if (!intent) {
        result.ok = false;
        result.errorCode = "rules_decision_ui_confirm_not_armed";
        return result;
    }
    rulesDecisionUi_.beginAckVerification();
    bool accepted = false;
    std::string error;
    if (intent->offer.capabilityId == rules::ui::kGroupChoiceCapabilityId) {
        const auto request = rules::ui::groupChoiceRequest(*intent);
        if (!request) error = "rules_decision_context_mismatch";
        else {
            const auto response = runtime_.submitGroupChoiceDecision(*request);
            error = response.errorCode;
            accepted = response.ok && rulesDecisionUi_.acceptAck(
                rules::ui::ackIdentity(response.ack));
        }
    } else if (intent->offer.capabilityId ==
            rules::kFreeBallPlacementCapabilityId) {
        const auto request = rules::ui::ballInHandPlacementRequest(*intent);
        if (!request) error = "rules_decision_context_mismatch";
        else {
            const auto response = runtime_.submitBallInHandPlacement(*request);
            error = response.errorCode;
            accepted = response.ok && rulesDecisionUi_.acceptAck(
                rules::ui::ackIdentity(response.ack));
        }
    } else if (intent->offer.capabilityId ==
            rules::kEightOnBreakCapabilityId) {
        const auto request = rules::ui::eightOnBreakRequest(*intent);
        if (!request) error = "rules_decision_context_mismatch";
        else {
            const auto response = runtime_.submitEightOnBreakDecision(*request);
            error = response.errorCode;
            accepted = response.ok && rulesDecisionUi_.acceptAck(
                rules::ui::ackIdentity(response.ack));
        }
    } else if (intent->offer.capabilityId ==
            rules::kBreakFoulCapabilityId) {
        const auto request = rules::ui::breakFoulRequest(*intent);
        if (!request) error = "rules_decision_context_mismatch";
        else {
            const auto response = runtime_.submitBreakFoulDecision(*request);
            error = response.errorCode;
            accepted = response.ok && rulesDecisionUi_.acceptAck(
                rules::ui::ackIdentity(response.ack));
        }
    } else {
        error = "rules_decision_context_mismatch";
    }
    if (!accepted) {
        if (rulesDecisionUi_.snapshot().state !=
                rules::ui::DecisionUiState::StateDriftRefreshing)
            rulesDecisionUi_.authorityError(error.empty() ?
                "rules_decision_context_mismatch" : error);
        result.ok = false;
        result.errorCode = error.empty() ?
            "rules_decision_ack_mismatch" : error;
    }
    return result;
}

void ModernProductSession::escapeRulesDecisionUi()
{
    if (rulesDecisionUiImplementationEnabled_) rulesDecisionUi_.escape();
}

ModernProductEventResult ModernProductSession::chooseReplayFile()
{
    ModernProductEventResult result;
    result.handled = true;
    chooseReplayRequested_ = true;
    if (fileDialogAdapter_ == nullptr) {
        result.appRequest = ProductShellAppRequest::ChooseReplayFile;
        return result;
    }
    const ProductFileDialogSelection selection =
        fileDialogAdapter_->chooseReplayFile();
    chooseReplayRequested_ = false;
    if (!selection.accepted) return result;
    if (selection.path.empty()) {
        result.ok = false;
        result.errorCode = "shell_replay_path_empty";
        lastError_ = result.errorCode;
        return result;
    }
    if (executableVersion_.empty()) {
        result.ok = false;
        result.errorCode = "training_replay_binding_unavailable";
        lastError_ = result.errorCode;
        return result;
    }
    std::ifstream input(selection.path, std::ios::binary);
    if (!input) {
        result.ok = false;
        result.errorCode = "shell_replay_read_failed";
        lastError_ = result.errorCode;
        return result;
    }
    const std::string bytes((std::istreambuf_iterator<char>(input)),
        std::istreambuf_iterator<char>());
    const json::ParseResult decoded = json::parse(bytes);
    if (!decoded.ok) {
        result.ok = false;
        result.errorCode = "shell_replay_json_invalid";
        lastError_ = result.errorCode;
        return result;
    }
    replay::Document document;
    const replay::ValidationResult parsed = replay::parse(decoded.value, document);
    if (!parsed.ok) {
        result.ok = false;
        result.errorCode = parsed.errorCode;
        lastError_ = result.errorCode;
        return result;
    }
    const replay::TrustedReplayContentBinding* trusted = nullptr;
    const replay::ProductReplayContentRegistryResult authorized =
        replayContentRegistry_.authorize(document,trusted);
    if (!authorized.ok || trusted == nullptr) {
        result.ok = false;
        result.errorCode = authorized.errorCode.empty() ?
            "product_replay_content_untrusted" : authorized.errorCode;
        lastError_ = result.errorCode;
        return result;
    }
    return dispatchShellResult(shell_.loadReplayPath(selection.path,
        executableVersion_, trusted->contentSha256,
        trusted->physicsProfile), true);
}

ModernProductEventResult ModernProductSession::saveTrainingReplayFile()
{
    ModernProductEventResult result;
    result.handled = true;
    if (fileDialogAdapter_ == nullptr) {
        result.ok = false;
        result.errorCode = "training_replay_file_dialog_unavailable";
        lastError_ = result.errorCode;
        return result;
    }
    const training::TrainingProductSnapshot state = training_.snapshot();
    if (!state.replayReady) {
        result.ok = false;
        result.errorCode = "training_replay_not_ready";
        lastError_ = result.errorCode;
        return result;
    }
    const std::string suggested = state.scenarioId.empty() ?
        "training-attempt.replay.json" :
        state.scenarioId + ".replay.json";
    const ProductFileDialogSelection selection =
        fileDialogAdapter_->saveTrainingReplayFile(suggested);
    if (!selection.accepted) return result;
    const training::TrainingProductResult saved =
        training_.saveReplay(selection.path);
    if (!saved.ok) {
        result.ok = false;
        result.errorCode = saved.errorCode;
        lastError_ = result.errorCode;
        return result;
    }
    lastSavedReplayPath_ = selection.path;
    lastSavedReplayExecutableVersion_ = state.executableVersion;
    lastSavedReplayContentSha256_ = state.scenarioFileSha256;
    lastError_.clear();
    return result;
}

ModernProductEventResult ModernProductSession::saveMatchReplayFile()
{
    ModernProductEventResult result;
    result.handled=true;
    captureMatchReplayProgress();
    if(!matchReplayError_.empty()){
        result.ok=false;result.errorCode=matchReplayError_;
        lastError_=result.errorCode;return result;
    }
    if(!matchReplayRecorder_.finished()){
        result.ok=false;result.errorCode="product_match_replay_not_ready";
        lastError_=result.errorCode;return result;
    }
    if(fileDialogAdapter_==nullptr){
        result.ok=false;
        result.errorCode="product_match_replay_file_dialog_unavailable";
        lastError_=result.errorCode;return result;
    }
    const ProductFileDialogSelection selection=
        fileDialogAdapter_->saveMatchReplayFile("single-rack.replay.json");
    if(!selection.accepted)return result;
    const replay::ValidationResult saved=
        matchReplayRecorder_.saveAtomic(selection.path);
    if(!saved.ok){
        result.ok=false;result.errorCode=saved.errorCode;
        lastError_=result.errorCode;return result;
    }
    const auto binding=replay::officialSingleRackReplayBinding(
        runtime_.physicsProfile());
    lastSavedReplayPath_=selection.path;
    lastSavedReplayExecutableVersion_=executableVersion_;
    lastSavedReplayContentSha256_=binding.contentSha256;
    lastError_.clear();
    return result;
}

ModernProductEventResult ModernProductSession::retryTrainingAttempt()
{
    const training::TrainingProductResult retried = training_.retryAttempt();
    ModernProductEventResult result;
    result.handled = true;
    result.ok = retried.ok;
    result.errorCode = retried.errorCode;
    if (retried.ok) lastError_.clear();
    else lastError_ = retried.errorCode;
    return result;
}

ModernProductEventResult ModernProductSession::refreshTrainingPreview(
    const training::PreviewBudget& budget)
{
    const training::TrainingProductResult previewed =
        training_.refreshPreview(budget);
    ModernProductEventResult result;
    result.handled = true;
    result.ok = previewed.ok;
    result.errorCode = previewed.errorCode;
    if (previewed.ok) lastError_.clear();
    else lastError_ = previewed.errorCode;
    return result;
}

void ModernProductSession::refreshAiGate()
{
    const ai::PlayerState state = aiPlayer_.state();
    const bool aiOwnsTurn = shell_.screen() == ProductScreen::Match &&
        state.configuration.enabled &&
        runtime_.matchState().currentPlayer == state.configuration.player;
    input_.setAiTurnActive(aiOwnsTurn);
}

ModernProductEventResult ModernProductSession::dispatchShellResult(
    const ProductShellResult& result, bool handled)
{
    clearTrajectoryInsetVisibleCurrentContext();
    ModernProductEventResult translated;
    translated.ok = result.ok;
    translated.handled = handled;
    translated.errorCode = result.errorCode;
    translated.appRequest = result.appRequest;
    translated.action = result.action;
    if (result.ok) lastError_.clear();
    else lastError_ = result.errorCode;
    if (result.appRequest == ProductShellAppRequest::Quit)
        quitRequested_ = true;
    else if (result.appRequest == ProductShellAppRequest::ChooseReplayFile) {
        if (fileDialogAdapter_ == nullptr)
            chooseReplayRequested_ = true;
        else
            return chooseReplayFile();
    }
    else if (result.appRequest ==
            ProductShellAppRequest::SaveTrainingReplayFile)
        return saveTrainingReplayFile();
    else if (result.appRequest ==
            ProductShellAppRequest::SaveMatchReplayFile)
        return saveMatchReplayFile();
    refreshAiGate();
    applyAudioSettings();
    return translated;
}

ModernProductEventResult ModernProductSession::dispatchNormalized(
    const NormalizedInputEvent& event)
{
    const ProductScreen previousScreen = shell_.screen();
    ModernProductEventResult result = dispatchShellResult(
        shell_.dispatchProductInput(event), true);
    if (shell_.screen() != previousScreen) clearShellPointerState();
    return handleMatchLifecycleTransition(previousScreen,std::move(result));
}

ModernProductEventResult ModernProductSession::navigate(
    ShellNavigationCommand command)
{
    const ProductScreen previousScreen = shell_.screen();
    ModernProductEventResult result = dispatchShellResult(
        shell_.navigate(command), true);
    if (shell_.screen() != previousScreen) clearShellPointerState();
    return handleMatchLifecycleTransition(previousScreen,std::move(result));
}

ModernProductEventResult ModernProductSession::handleMatchLifecycleTransition(
    ProductScreen previousScreen,ModernProductEventResult result)
{
    if (shell_.screen() != previousScreen)
        syncRulesDecisionPlacementCamera("");
    if(result.ok&&previousScreen==ProductScreen::SinglePlayerSetup&&
       shell_.screen()==ProductScreen::Match)
        beginMatchReplayRecording();
    if(!matchReplayError_.empty()&&result.ok){
        result.ok=false;result.errorCode=matchReplayError_;
    }
    return result;
}

void ModernProductSession::setShellViewport(const ui::ShellViewport& viewport)
{
    if (viewport.physicalWidth != shellViewport_.physicalWidth ||
            viewport.physicalHeight != shellViewport_.physicalHeight ||
            viewport.logicalWidth != shellViewport_.logicalWidth ||
            viewport.logicalHeight != shellViewport_.logicalHeight) {
        presentedInsetToggle_.reset();
        insetTogglePressed_ = false;
    }
    clearTrajectoryInsetVisibleCurrentContext();
    shellViewport_ = viewport;
    input_.setLogicalViewportHeight(viewport.logicalHeight);
    const ui::ShellScreenLayout layout = shellLayout();
    const auto visible = [&](const std::string& id) {
        if (id == "inspector_toggle") return layout.boundedInspector;
        return std::any_of(layout.elements.begin(), layout.elements.end(),
            [&](const auto& element) { return element.id == id && element.enabled; });
    };
    if (!layout.ok) clearShellPointerState();
    else {
        if (!visible(pressedShellItemId_)) pressedShellItemId_.clear();
        if (!visible(hoveredShellItemId_)) hoveredShellItemId_.clear();
    }
}

ui::ShellScreenLayout ModernProductSession::shellLayout() const
{
    return ui::buildShippingScreenLayout(shell_.snapshot(), shellViewport_);
}

void ModernProductSession::clearShellPointerState()
{
    hoveredShellItemId_.clear();
    pressedShellItemId_.clear();
}

void ModernProductSession::cancelPointerInteraction()
{
    presentedInsetToggle_.reset();
    insetTogglePressed_ = false;
    clearTrajectoryInsetVisibleCurrentContext();
    clearShellPointerState();
    pressedRulesDecisionControlId_.clear();
    rulesDecisionUi_.windowFocusLost();
}

void ModernProductSession::setRulesDecisionHitRects(
    std::vector<ui::RulesDecisionUiHitRect> hitRects)
{
    rulesDecisionHitRects_ = std::move(hitRects);
    const bool retained = std::any_of(rulesDecisionHitRects_.begin(),
        rulesDecisionHitRects_.end(), [&](const auto& hit) {
            return hit.controlId == pressedRulesDecisionControlId_ &&
                hit.enabled;
        });
    if (!retained) pressedRulesDecisionControlId_.clear();
}

void ModernProductSession::setRulesDecisionExclusionRects(
    std::vector<PointerProjectionRect> exclusionRects)
{
    rulesDecisionExclusionRects_ = std::move(exclusionRects);
}

void ModernProductSession::setRulesDecisionProjectionCamera(
    const CameraPresentation& camera)
{
    rulesDecisionProjectionCamera_ = camera;
}

ModernProductEventResult ModernProductSession::handleRulesDecisionInput(
    const PlatformInputEvent& event, const ProductInputAdaptation& adapted)
{
    ModernProductEventResult handled;
    handled.handled = true;
    const auto commit = [&]() {
        const auto& decision = rulesDecisionUi_.snapshot();
        if (decision.armedActionId.empty()) return handled;
        const bool product = rulesDecisionProductActive_ &&
            !rulesDecisionUiImplementationEnabled_;
        const std::string key = std::string(product ? "product.rules." :
            "internal.rules.") +
            std::to_string(decision.offer.authorityStateRevision) + "." +
            std::to_string(++rulesDecisionSubmissionOrdinal_);
        if (product) {
            const auto intent = rulesDecisionUi_.confirmArmedAction(key);
            if (!intent) {
                handled.ok = false;
                handled.errorCode = "rules_decision_ui_confirm_not_armed";
                return handled;
            }
            ModernProductEventResult submitted;
            if (decision.offer.flowId == rules::kBreakFoulFlowId &&
                    decision.offer.branchId == "illegal_break_6c") {
                submitted = submitProductAuthorizedIllegalBreakChoice(
                    decision.armedActionId, key, "{}");
            } else if (decision.offer.flowId == rules::kBreakFoulFlowId &&
                    decision.offer.branchId == "break_shot_foul_6e") {
                submitted = submitProductAuthorizedBreakFoulChoice(
                    decision.armedActionId, key, "{}");
            } else if (decision.offer.flowId == rules::kBreakFoulFlowId &&
                    decision.offer.branchId ==
                        "no_rack_contact_miscue_6g") {
                submitted = submitProductAuthorizedNoRackContactMiscueChoice(
                    decision.armedActionId, key, "{}");
            } else if (decision.offer.flowId ==
                    rules::kEightOnBreakFlowId) {
                submitted = submitProductAuthorizedEightOnBreakChoice(
                    decision.armedActionId, key, "{}");
            } else if (decision.offer.flowId ==
                    rules::kGroupChoiceAuthorityFlowId) {
                submitted = submitProductAuthorizedGroupChoice(
                    decision.armedActionId, key);
            } else if (decision.offer.flowId ==
                    rules::kFullTableBallInHandFlowId &&
                    decision.armedActionId ==
                        rules::kConfirmFullTablePlacementActionId) {
                submitted = confirmProductAuthorizedBallInHandPlacement(
                    decision.armedActionId, decision.placementPreview, key);
            } else if (decision.offer.flowId ==
                    rules::kBehindHeadStringBallInHandFlowId &&
                    decision.armedActionId ==
                        rules::kConfirmBehindHeadStringPlacementActionId) {
                submitted = confirmProductAuthorizedBallInHandPlacement(
                    decision.armedActionId, decision.placementPreview, key);
            } else if (decision.offer.flowId ==
                    rules::kBehindHeadStringBallInHandFlowId &&
                    decision.armedActionId ==
                        rules::kRequestNearestLegalRespotActionId) {
                submitted = submitProductAuthorizedBehindHeadStringRespot(
                    rules::kRequestNearestLegalRespotActionId, -1, key, "{}");
            } else if (decision.offer.flowId ==
                    rules::kBehindHeadStringRespotFlowId) {
                const auto ball = rules::ui::productRespotChoiceBallId(
                    decision.armedActionId);
                if (!ball) {
                    submitted.ok = false;
                    submitted.handled = true;
                    submitted.errorCode =
                        "rules_product_head_string_respot_action_not_authorized";
                } else {
                    std::ostringstream payload;
                    payload << "{\"selected_ball_id\":" << *ball << '}';
                    submitted = submitProductAuthorizedBehindHeadStringRespot(
                        rules::kChooseEquidistantRespotBallActionId, *ball,
                        key, payload.str());
                }
            } else {
                submitted.ok = false;
                submitted.handled = true;
                submitted.errorCode =
                    "rules_decision_product_branch_unavailable";
            }
            if (!submitted.ok)
                rulesDecisionUi_.authorityError(submitted.errorCode);
            return submitted;
        }
        return confirmRulesDecisionAction(key);
    };

    // Behind-head-string placement can require revealing the legal half of
    // the table when the modal covers it.  Wheel zoom remains a presentation
    // action: it neither previews a coordinate nor mutates rules authority.
    // All other decision-modal input continues through the fail-closed rules
    // handler below.
    const bool productPlacementFlow =
        rulesDecisionUi_.snapshot().offer.flowId ==
            rules::kBehindHeadStringBallInHandFlowId ||
        rulesDecisionUi_.snapshot().offer.flowId ==
            rules::kFullTableBallInHandFlowId;
    if (event.kind == PlatformInputKind::Scroll &&
        rulesDecisionProductActive_ &&
        !rulesDecisionUiImplementationEnabled_ &&
        productPlacementFlow &&
        !adapted.events.empty())
        return dispatchNormalized(adapted.events.front());

    const auto hitAt = [&](double x, double y) ->
            const ui::RulesDecisionUiHitRect* {
        for (const auto& hit : rulesDecisionHitRects_) {
            const auto& rect = hit.physicalRect;
            if (hit.enabled && x >= rect.x && y >= rect.y &&
                x < rect.x + rect.width && y < rect.y + rect.height)
                return &hit;
        }
        return nullptr;
    };

    if (event.kind == PlatformInputKind::PointerButtonDown ||
        event.kind == PlatformInputKind::PointerButtonUp) {
        if (event.control != "LeftButton") return handled;
        const double scale = std::isfinite(event.backingScale) &&
                event.backingScale > 0.0 ? event.backingScale : 1.0;
        const auto* hit = hitAt(event.logicalX * scale, event.logicalY * scale);
        if (event.kind == PlatformInputKind::PointerButtonDown) {
            pressedRulesDecisionControlId_ = hit == nullptr ?
                std::string{} : hit->controlId;
            if (hit != nullptr && !hit->confirm && !hit->actionId.empty())
                (void)rulesDecisionUi_.focusAction(hit->actionId);
            if (hit != nullptr) return handled;
            // A table press in placement mode updates only the local preview.
            // It never arms or confirms the authority action.
            if (!productPlacementFlow)
                return handled;
        }
        if (event.kind == PlatformInputKind::PointerButtonUp) {
            const std::string pressed =
                std::move(pressedRulesDecisionControlId_);
            pressedRulesDecisionControlId_.clear();
            if (hit == nullptr || hit->controlId != pressed) return handled;
            if (hit->confirm) {
                // Hit rectangles describe a rendered authority state. A new
                // arm can arrive before the next frame; never let stale
                // Confirm(A) submit newly armed B.
                if (hit->actionId !=
                        rulesDecisionUi_.snapshot().armedActionId)
                    return handled;
                return commit();
            }
            if (!hit->actionId.empty()) {
                handled.ok = rulesDecisionUi_.armAction(hit->actionId);
                if (!handled.ok) handled.errorCode =
                    "rules_decision_action_not_available";
            }
            return handled;
        }
    }
    if ((event.kind == PlatformInputKind::PointerMove ||
            (event.kind == PlatformInputKind::PointerButtonDown &&
             event.control == "LeftButton")) &&
        rulesDecisionProductActive_ &&
        !rulesDecisionUiImplementationEnabled_ &&
        productPlacementFlow) {
        TablePlanePointerProjectionRequest request;
        if (rulesDecisionProjectionCamera_) {
            request.camera = *rulesDecisionProjectionCamera_;
        } else {
            // Headless/component callers without a rendered frame retain the
            // same no-pan camera fallback used before the product bridge.
            const camera::Frame frame = presentationCameraFrame();
            request.camera.eye = {frame.pose.eye.x, frame.pose.eye.y,
                frame.pose.eye.z};
            request.camera.target = {frame.pose.target.x, frame.pose.target.y,
                frame.pose.target.z};
            request.camera.fieldOfViewDegrees = frame.pose.fieldOfViewDegrees;
        }
        request.viewport = {shellViewport_.logicalWidth,
            shellViewport_.logicalHeight, shellViewport_.physicalWidth,
            shellViewport_.physicalHeight};
        request.logicalX = event.logicalX;
        request.logicalY = event.logicalY;
        request.tablePlaneY = kTableHeight;
        request.excludedPhysicalRects = rulesDecisionExclusionRects_;
        const auto projected = projectPointerToTablePlane(request);
        if (!projected.ok) return handled;
        const auto offer = runtime_.ballInHandPlacementOffer();
        const bool authorized06e = offer &&
            offer->decisionId == product06ePlacementDecisionId_;
        const bool authorized06f = offer &&
            offer->decisionId == product06fPlacementDecisionId_;
        const bool authorizedFullTable = offer &&
            rulesDecisionProductCapabilities_.fullTableBallInHand &&
            offer->scope == rules::BallInHandPlacementScope::FullTable &&
            offer->decisionId == productFullTablePlacementDecisionId_;
        if (!offer || (!authorized06e && !authorized06f &&
                !authorizedFullTable) ||
            !rulesDecisionActorIsLocal(offer->actorPlayerId)) {
            handled.ok = false;
            handled.errorCode = "rules_placement_not_pending";
            return handled;
        }
        const rules::BallInHandPlacementPosition position{
            projected.tablePoint.x, projected.tablePoint.z};
        const rules::BallInHandPlacementValidation validation =
            runtime_.validateBallInHandPlacement(position);
        (void)rulesDecisionUi_.setPlacementPreview(position, validation,
            offer->decisionId, offer->authorityStateRevision);
        if (validation.ok) {
            const auto preview =
                previewProductAuthorizedBallInHandPlacement(position);
            if (!preview.ok) return preview;
        } else {
            productAuthorizedPlacementPreview_.reset();
        }
        return handled;
    }
    if (event.kind == PlatformInputKind::PointerMove ||
        event.kind == PlatformInputKind::Scroll ||
        event.kind == PlatformInputKind::ModifierChanged ||
        event.kind == PlatformInputKind::KeyUp)
        return handled;
    if (adapted.events.empty()) return handled;
    const auto& key = adapted.events.front();
    if (key.device != "keyboard" || key.trigger != "press") return handled;
    if (key.control == "Escape") {
        rulesDecisionUi_.escape();
        return handled;
    }
    auto decision = rulesDecisionUi_.snapshot();
    if (key.control == "Tab" || key.control == "ArrowDown" ||
        key.control == "ArrowRight" || key.control == "ArrowUp" ||
        key.control == "ArrowLeft") {
        if (decision.visibleActionIds.empty()) return handled;
        auto current = std::find(decision.visibleActionIds.begin(),
            decision.visibleActionIds.end(), decision.focusedActionId);
        std::ptrdiff_t index = current == decision.visibleActionIds.end() ? -1 :
            std::distance(decision.visibleActionIds.begin(), current);
        const bool backwards = key.control == "ArrowUp" ||
            key.control == "ArrowLeft";
        const auto count = static_cast<std::ptrdiff_t>(
            decision.visibleActionIds.size());
        index = backwards ? (index <= 0 ? count - 1 : index - 1) :
            (index + 1) % count;
        (void)rulesDecisionUi_.focusAction(
            decision.visibleActionIds[static_cast<std::size_t>(index)]);
        return handled;
    }
    if (key.control == "Enter" || key.control == "Space") {
        if (!decision.armedActionId.empty()) return commit();
        const std::string action = decision.focusedActionId.empty() &&
                !decision.visibleActionIds.empty() ?
            decision.visibleActionIds.front() : decision.focusedActionId;
        if (!action.empty()) {
            (void)rulesDecisionUi_.focusAction(action);
            handled.ok = rulesDecisionUi_.armAction(action);
            if (!handled.ok) handled.errorCode =
                "rules_decision_action_not_available";
        }
        return handled;
    }
    return handled;
}

ModernProductEventResult ModernProductSession::handleShellPointer(
    const PlatformInputEvent& event)
{
    const ui::ShellScreenLayout layout = shellLayout();
    if (!layout.ok) return {};
    const ui::ShellHitTestResult hit = ui::hitTestShippingScreenLayout(
        layout, event.logicalX, event.logicalY);
    if (event.kind == PlatformInputKind::PointerMove) {
        hoveredShellItemId_ = hit.hit ? hit.id : std::string{};
        if (hit.hit && hit.enabled) {
            if (hit.id != "inspector_toggle") {
                const ProductShellResult focused = shell_.focusItem(hit.id);
                if (!focused.ok) return dispatchShellResult(focused, true);
            }
        }
        ModernProductEventResult result;
        result.handled = true;
        return result;
    }
    if (event.control != "LeftButton") return {};
    if (event.kind == PlatformInputKind::PointerButtonDown) {
        hoveredShellItemId_ = hit.hit ? hit.id : std::string{};
        pressedShellItemId_ = hit.hit && hit.enabled ? hit.id : std::string{};
        if (pressedShellItemId_.empty()) return {};
        if (pressedShellItemId_ == "inspector_toggle") {
            ModernProductEventResult captured;
            captured.handled = true;
            return captured;
        }
        return dispatchShellResult(shell_.focusItem(pressedShellItemId_), true);
    }
    if (event.kind != PlatformInputKind::PointerButtonUp) return {};
    const std::string pressed = std::move(pressedShellItemId_);
    pressedShellItemId_.clear();
    hoveredShellItemId_ = hit.hit ? hit.id : std::string{};
    if (pressed.empty()) return {};
    ModernProductEventResult result;
    result.handled = true;
    if (!hit.hit || !hit.enabled || hit.id != pressed) return result;
    if (hit.id == "inspector_toggle") {
        shell_.setInspectorExpanded(layout.inspectorCollapsed);
        result.action = layout.inspectorCollapsed ? "inspector_expand" : "inspector_collapse";
        return result;
    }
    const ProductShellResult focused = shell_.focusItem(hit.id);
    if (!focused.ok) return dispatchShellResult(focused, true);
    auto activated = navigate(ShellNavigationCommand::Activate);
    activated.action = hit.id;
    return activated;
}

ModernProductEventResult ModernProductSession::handlePlatformInput(
    const PlatformInputEvent& platformEvent)
{
    const auto screenBefore = shell_.screen();
    const bool insetScreen = screenBefore == ProductScreen::TrainingSession ||
        screenBefore == ProductScreen::ReplayPlayback;
    if (!insetScreen || presentedInsetScreen_ != screenBefore ||
            presentedInsetTick_ != presentationRuntime().tick() ||
            !shell_.snapshot().trajectoryInsetEnabled) {
        presentedInsetToggle_.reset();
        insetTogglePressed_ = false;
    }
    if (insetScreen && presentedInsetToggle_ &&
            platformEvent.control == "LeftButton" &&
            (platformEvent.kind == PlatformInputKind::PointerButtonDown ||
             platformEvent.kind == PlatformInputKind::PointerButtonUp)) {
        const auto& rect = *presentedInsetToggle_;
        const double x = platformEvent.logicalX * shellViewport_.physicalWidth /
            std::max(1, shellViewport_.logicalWidth);
        const double y = platformEvent.logicalY * shellViewport_.physicalHeight /
            std::max(1, shellViewport_.logicalHeight);
        const bool inside = x >= rect.x && x < rect.x + rect.width &&
            y >= rect.y && y < rect.y + rect.height;
        if (platformEvent.kind == PlatformInputKind::PointerButtonDown && inside) {
            insetTogglePressed_ = true;
            ModernProductEventResult result;
            result.handled = true;
            return result;
        }
        if (platformEvent.kind == PlatformInputKind::PointerButtonUp && insetTogglePressed_) {
            insetTogglePressed_ = false;
            if (inside) {
                (void)shell_.focusItem("trajectory_inset_toggle");
                presentedInsetToggle_.reset();
                return navigate(ShellNavigationCommand::Activate);
            }
            ModernProductEventResult result;
            result.handled = true;
            return result;
        }
    }
    clearTrajectoryInsetVisibleCurrentContext();
    refreshAiGate();
    syncRulesDecisionAuthority();
    if(shell_.screen()==ProductScreen::Match&&
       platformEvent.kind==PlatformInputKind::KeyDown&&
       platformEvent.control=="F9")
        return saveMatchReplayFile();
    const ProductInputAdaptation adapted = adapter_.adapt(platformEvent);
    if (!adapted.handled || adapted.events.empty()) return {};

    const ProductScreen screen = shell_.screen();
    if ((screen == ProductScreen::TrainingSession || screen == ProductScreen::ReplayPlayback) &&
            platformEvent.kind == PlatformInputKind::KeyDown &&
            !shellLayout().inspectorCollapsed) {
        if (platformEvent.control == "ArrowDown")
            return navigate(ShellNavigationCommand::FocusNext);
        if (platformEvent.control == "ArrowUp")
            return navigate(ShellNavigationCommand::FocusPrevious);
        if (platformEvent.control == "Enter")
            return navigate(ShellNavigationCommand::Activate);
    }
    if ((screen == ProductScreen::TrainingSession || screen == ProductScreen::ReplayPlayback) &&
            platformEvent.kind == PlatformInputKind::KeyDown &&
            platformEvent.control == "Enter" && shellLayout().inspectorCollapsed) {
        shell_.setInspectorExpanded(true);
        clearShellPointerState();
        ModernProductEventResult result;
        result.handled = true;
        result.action = "inspector_expand";
        return result;
    }
    if (platformEvent.kind == PlatformInputKind::Scroll &&
        (screen == ProductScreen::TrainingSession || screen == ProductScreen::ReplayPlayback)) {
        const auto layout = shellLayout();
        const double x = platformEvent.logicalX * layout.viewport.physicalWidth / std::max(1, layout.viewport.logicalWidth);
        const double y = platformEvent.logicalY * layout.viewport.physicalHeight / std::max(1, layout.viewport.logicalHeight);
        const auto& rect = layout.inspectorRect;
        if (layout.ok && !layout.inspectorCollapsed && x >= rect.x && x < rect.x + rect.width &&
            y >= rect.y && y < rect.y + rect.height) {
            std::size_t row = layout.firstVisibleRow;
            if (platformEvent.scrollY < 0 && row < layout.maximumScrollRow) ++row;
            if (platformEvent.scrollY > 0 && row > 0) --row;
            shell_.setInspectorScrollRow(row);
            clearShellPointerState();
            ModernProductEventResult scrolled;
            scrolled.handled = true;
            scrolled.action = "inspector_scroll";
            return scrolled;
        }
    }
    const bool pointer = platformEvent.kind == PlatformInputKind::PointerMove ||
        platformEvent.kind == PlatformInputKind::PointerButtonDown ||
        platformEvent.kind == PlatformInputKind::PointerButtonUp;
    if (pointer && isShellPointerScreen(screen) &&
        !(screen == ProductScreen::Settings &&
            settings_.menu().waitingForBinding())) {
        // Training keeps the gameplay pointer path outside visible controls;
        // only an actual row hit (or a captured row press) is consumed by the
        // shell. This keeps left-release Shoot available over the table.
        if ((screen != ProductScreen::TrainingSession && screen != ProductScreen::ReplayPlayback) ||
            !pressedShellItemId_.empty())
            return handleShellPointer(platformEvent);
        const ui::ShellScreenLayout layout = shellLayout();
        const ui::ShellHitTestResult hit = ui::hitTestShippingScreenLayout(
            layout, platformEvent.logicalX, platformEvent.logicalY);
        if (hit.hit) return handleShellPointer(platformEvent);
        if (platformEvent.kind == PlatformInputKind::PointerMove)
            hoveredShellItemId_.clear();
    }

    if (screen == ProductScreen::Settings) {
        // Exact key events drive settings; the aggregate ArrowKeys event is
        // reserved for gameplay camera motion.
        return dispatchNormalized(adapted.events.front());
    }

    if (isShellNavigationScreen(screen)) {
        ShellNavigationCommand command = ShellNavigationCommand::Activate;
        if (navigationCommand(adapted.events.front(), command))
            return navigate(command);
        const NormalizedInputEvent& event = adapted.events.front();
        if (event.device == "keyboard" && event.trigger == "press" &&
            (event.control == "F10" || event.control == "H"))
            return dispatchNormalized(event);
        return {};
    }

    if (!isGameplayScreen(screen)) return {};
    const NormalizedInputEvent& first = adapted.events.front();
    if ((rulesDecisionUiImplementationEnabled_ ||
            rulesDecisionProductActive_) &&
            rulesDecisionUi_.snapshot().state !=
                rules::ui::DecisionUiState::HiddenNoOffer &&
            rulesDecisionUi_.snapshot().state !=
                rules::ui::DecisionUiState::CompletedAuthoritative) {
        return handleRulesDecisionInput(platformEvent, adapted);
    }
    if (first.device == "keyboard" && first.control == "Escape" &&
        first.trigger == "press")
        return navigate(ShellNavigationCommand::Back);
    if ((screen == ProductScreen::TrainingSession || screen == ProductScreen::ReplayPlayback) &&
        platformEvent.kind == PlatformInputKind::PointerButtonUp &&
        first.control == "LeftButton" &&
        (screen == ProductScreen::ReplayPlayback || runtime_.state().aim.mode != AimMode::Aim)) {
        // Releasing an observe-mode orbit gesture, including the release that
        // follows entry from the catalog, has no gameplay action.  Shooting
        // remains a release binding once the player explicitly enters Aim.
        ModernProductEventResult ignored;
        ignored.handled = true;
        return ignored;
    }
    if (platformEvent.kind == PlatformInputKind::KeyUp ||
        platformEvent.kind == PlatformInputKind::PointerButtonDown ||
        platformEvent.kind == PlatformInputKind::ModifierChanged)
        return {};
    if (first.control == "MoveX" && runtime_.state().aim.mode != AimMode::Aim)
        return {};
    if (first.control == "RightDrag" || first.control == "RightButton")
        return {};
    for (const NormalizedInputEvent& event : adapted.events) {
        if (isExactDirectionalKey(event) && adapted.events.size() > 1) continue;
        return dispatchNormalized(event);
    }
    return {};
}

ModernProductEventResult ModernProductSession::beginRebind(
    const std::string& action)
{
    return dispatchShellResult(shell_.beginRebind(action), true);
}

ai::PlayerState ModernProductSession::pollAi()
{
    ai::PlayerState state = aiPlayer_.state();
    if (shell_.screen() == ProductScreen::Match)
        state = aiPlayer_.poll(runtime_);
    refreshAiGate();
    return state;
}

ModernProductEventResult ModernProductSession::advancePresentedFrame()
{
    clearTrajectoryInsetVisibleCurrentContext();
    if (shell_.screen() != ProductScreen::ReplayPlayback)
        return ModernProductEventResult{};
    return dispatchShellResult(shell_.advanceReplayPresentedFrame(), true);
}

ModernProductEventResult ModernProductSession::advancePresentedFrame(
    double elapsedSeconds)
{
    clearTrajectoryInsetVisibleCurrentContext();
    if (shell_.screen() != ProductScreen::ReplayPlayback)
        return ModernProductEventResult{};
    return dispatchShellResult(
        shell_.advanceReplayPresentedFrame(elapsedSeconds), true);
}

const GameRuntime& ModernProductSession::presentationRuntime() const noexcept
{
    if (shell_.screen() == ProductScreen::ReplayPlayback &&
        replay_.snapshot().loaded)
        return replay_.player().runtime();
    return runtime_;
}

camera::Frame ModernProductSession::presentationCameraFrame() const noexcept
{
    if (shell_.screen() == ProductScreen::ReplayPlayback &&
        replay_.snapshot().loaded)
        return replay_.presentationCameraFrame();
    return runtime_.cameraFrame();
}

void ModernProductSession::setPresentedTrajectoryInsetToggle(
    std::optional<product_hud::HudRect> rect, std::uint64_t frameSerial)
{
    if (!trajectoryInsetVisibleCurrentContext_ ||
            trajectoryInsetVisibleCurrentContext_->frameSerial != frameSerial ||
            !rect || !std::isfinite(rect->x) || !std::isfinite(rect->y) ||
            !std::isfinite(rect->width) || !std::isfinite(rect->height) ||
            rect->width <= 0 || rect->height <= 0) {
        presentedInsetToggle_.reset();
        insetTogglePressed_ = false;
        return;
    }
    if (presentedInsetToggle_ && (presentedInsetToggle_->x != rect->x ||
            presentedInsetToggle_->y != rect->y ||
            presentedInsetToggle_->width != rect->width ||
            presentedInsetToggle_->height != rect->height))
        insetTogglePressed_ = false;
    presentedInsetToggle_ = rect;
    presentedInsetScreen_ = shell_.screen();
    presentedInsetTick_ = presentationRuntime().tick();
}

void ModernProductSession::prepareTrajectoryInsetCurrentFrame(int samples)
{
    clearTrajectoryInsetVisibleCurrentContext();
    const auto shell = shell_.snapshot();
    if ((shell.screen != ProductScreen::TrainingSession &&
            shell.screen != ProductScreen::ReplayPlayback) ||
            !shell.trajectoryInsetEnabled || shell.worldTrajectoryOverlayEnabled ||
            shell.settingsPreviewActive || !shell.errorCode.empty() ||
            rulesDecisionProductActive_ ||
            (samples != 1 && samples != 2 && samples != 4) ||
            shellViewport_.physicalWidth <= 0 || shellViewport_.physicalHeight <= 0)
        return;
    const auto drawableTrack = [](const training::OverlayTrack& track) {
        return track.visible && track.samples.size() >= 2;
    };
    if (!drawableTrack(shell.trajectoryOverlay.predicted) &&
            !drawableTrack(shell.trajectoryOverlay.actual))
        return; // No fabricated route or silent world-guide fallback.
    // Bind the generation to the exact source geometry and content, not the
    // presentation serial. Seeking or replacing a preview changes this digest.
    ui::TrajectoryInsetVisibleCurrentContext context;
    context.present = true;
    context.sessionStable = true;
    context.insetToggleOn = true;
    context.sessionSerial = context.frameSerial = context.layoutSerial =
        ++trajectoryInsetFrameSerial_;
    context.samples = samples;
    context.cp1GenerationId = "v19_generation_" + std::to_string(context.frameSerial);
    context.sourceContentSha256 = ui::trajectoryInsetSourceContentSha256(shell);
    context.palette = effectiveAudioSettings().colorVisionMode ==
            settings::ColorVisionMode::Default ?
        training::TrajectoryInsetPalette::Standard :
        training::TrajectoryInsetPalette::ColorVisionSafe;
    trajectoryInsetPreparedScreen_ = shell.screen;
    trajectoryInsetPreparedTick_ = presentationRuntime().tick();
    trajectoryInsetVisibleCurrentContext_ = std::move(context);
}

ModernProductSessionSnapshot ModernProductSession::snapshot() const
{
    ModernProductSessionSnapshot value;
    value.shell = shell_.snapshot();
    value.settingsStartup = settingsStartup_;
    value.effectiveSettings = effectiveAudioSettings();
    value.aiPlayer = aiPlayer_.state();
    value.inputSurface = input_.surface();
    value.pointer = adapter_.pointer();
    value.audioEnabled = audioController_ != nullptr;
    if (audioController_ != nullptr) value.audio = audioController_->snapshot();
    value.audioConsumedAuthorityTicks = audioConsumedAuthorityTicks_;
    value.audioFailureCount = audioFailureCount_;
    value.audioError = audioError_;
    value.quitRequested = quitRequested_;
    value.chooseReplayRequested = chooseReplayRequested_;
    value.matchReplayRecording=matchReplayRecorder_.active();
    value.matchReplayReady=matchReplayRecorder_.finished();
    value.matchReplayError=matchReplayError_;
    value.lastSavedReplayPath = lastSavedReplayPath_;
    value.trainingLoadError = trainingLoadError_;
    value.lastError = lastError_;
    value.shellViewport = shellViewport_;
    value.hoveredShellItemId = hoveredShellItemId_;
    value.pressedShellItemId = pressedShellItemId_;
    value.rulesDecisionUiImplementationEnabled =
        rulesDecisionUiImplementationEnabled_;
    value.rulesDecisionUiProductExposed = false;
    value.rulesDecisionGroupChoiceProductEnabled =
        rulesDecisionProductCapabilities_.groupChoice;
    value.rulesDecisionFullTableBallInHandProductEnabled =
        rulesDecisionProductCapabilities_.fullTableBallInHand;
    value.rulesDecisionIllegalBreakProductEnabled =
        rulesDecisionProductCapabilities_.illegalBreakChoice;
    value.rulesDecisionBreakShotFoulProductEnabled =
        rulesDecisionProductCapabilities_.breakShotFoulChoice;
    value.rulesDecisionEightOnBreakProductEnabled =
        rulesDecisionProductCapabilities_.eightOnBreakChoice;
    value.rulesDecisionNoRackContactMiscueProductEnabled =
        rulesDecisionProductCapabilities_.noRackContactMiscueChoice;
    value.rulesDecisionProductActive = rulesDecisionProductActive_;
    value.rulesDecisionLocale = rulesDecisionLocale_;
    value.rulesDecision = rulesDecisionUi_.snapshot();
    value.trajectoryInsetVisibleCurrentContext =
        trajectoryInsetVisibleCurrentContext_;
    if (value.shell.screen != trajectoryInsetPreparedScreen_ ||
            !value.shell.trajectoryInsetEnabled ||
            value.shell.worldTrajectoryOverlayEnabled ||
            presentationRuntime().tick() != trajectoryInsetPreparedTick_ ||
            !value.shell.errorCode.empty())
        value.trajectoryInsetVisibleCurrentContext.reset();
    return value;
}

}  // namespace billiardgl::render
