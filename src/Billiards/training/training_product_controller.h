#pragma once

#include "replay/replay_recorder.h"
#include "replay/replay_transport_model.h"
#include "training/preview_runtime.h"
#include "training/training_session.h"

#include <cstdint>
#include <set>
#include <string>
#include <vector>

namespace billiardgl::training {

struct TrainingProductResult {
    bool ok = false;
    std::string errorCode;
};

struct TrainingReferenceReplayAudit {
    TrainingProductResult result;
    std::string actionLogSha256;
    std::string traceSha256;
    Evaluation evaluation;
};

// Replays a candidate reference under the scenario's actual fixed-step
// authority.  The returned digests are suitable for an immutable training
// binding only when result.ok is true.
TrainingReferenceReplayAudit auditReferenceReplay(
    const replay::Document& document, const Scenario& scenario,
    const PhysicsProfile& profile, const std::string& executableVersion,
    const std::string& contentSha256);

struct TrainingReferenceReplayBinding {
    bool bound = false;
    std::string id;
    std::string filePath;
    std::string fileSha256;
    std::string physicsProfileId;
    std::string actionLogSha256;
    std::string traceSha256;
    std::string reviewStatus;
};

struct TrainingTeachingHints {
    bool available = false;
    std::string goalEn;
    bool targetPocketEnabled = true;
    bool aimAtPocket = false;
    float minimumPowerPercent = 0.0f;
    float maximumPowerPercent = 0.0f;
    float defaultPowerPercent = 0.0f;
    float tipXBallRadii = 0.0f;
    float tipYBallRadii = 0.0f;
    std::string tipLabel;
    float aimYawRadians = 0.0f;
    float aimToleranceDegrees = 0.0f;
    std::string targetPocketId;
    std::array<float, 2> targetPocketCenterCm{};
    std::array<float, 2> cueBallTargetCenterCm{};
    float cueBallTargetRadiusCm = 0.0f;
    bool cueBallTargetEnabled = false;
    bool cueBallTargetTracksFirstContact = false;
    bool cueBallMustAvoidRails = false;
    int cueBallIndex = -1;
    int objectBallIndex = -1;
    std::array<float, 2> shotLineStartCm{};
    std::array<float, 2> shotLineObjectCenterCm{};
};

struct TrainingScenarioCard {
    std::string id;
    std::string category;
    std::string filePath;
    std::string fileSha256;
    bool scoreEnabled = false;
    std::string titleEn;
    std::string titleZhCn;
    std::string descriptionEn;
    std::string descriptionZhCn;
    std::string recommendedAssist;
    std::string successSummary;
    std::string successEvaluation;
    std::string failureEvaluation;
    std::string resetMode;
    std::vector<std::string> resetPreserve;
    std::uint32_t attemptSeed = 0;
    std::vector<std::string> missingCapabilities;
    PhysicsProfile physicsProfile;
    TrainingReferenceReplayBinding referenceReplay;
    TrainingTeachingHints teachingHints;
    std::vector<TrainingTeachingHints> teachingSteps;
    bool powerAdjustmentAllowed = false;
    bool tipAdjustmentAllowed = false;
    std::string tipAdjustmentAxis;
    bool aimAdjustmentAllowed = false;
};

enum class TrainingProductState {
    Empty,
    PackReady,
    AttemptActive,
    AttemptEvaluated
};

struct TrainingProductSnapshot {
    TrainingProductState state = TrainingProductState::Empty;
    std::string packId;
    std::string scenarioId;
    int teachingStepIndex = 0;
    int teachingStepCount = 1;
    std::size_t scenarioCount = 0;
    bool replayReady = false;
    bool shotPrepared = false;
    bool shotInProgress = false;
    std::string executableVersion;
    std::string scenarioFileSha256;
    std::uint32_t attemptSeed = 0;
    AttemptFacts attemptFacts;
    bool previewReady = false;
    int previewSampleCount = 0;
    bool previewAuthorityUnchanged = false;
    TrainingTeachingHints teachingHints;
    bool powerAdjustmentAllowed = false;
    bool tipAdjustmentAllowed = false;
    bool aimAdjustmentAllowed = false;
    Evaluation evaluation;
};

// Product lifecycle boundary shared by desktop UI and automation. It owns no
// alternate authority: scenario edits and attempts always use the supplied
// GameRuntime, while replay playback remains isolated in ReplayTransportModel.
class TrainingProductController {
public:
    explicit TrainingProductController(GameRuntime& runtime,
        std::string executableVersion = {});

    void setExecutableVersion(std::string executableVersion);

    TrainingProductResult loadPack(const std::string& manifestPath,
        const std::set<std::string>& capabilities);
    TrainingProductResult startScenario(const std::string& scenarioId,
        const std::string& executableVersion, std::uint32_t seed);
    TrainingProductResult startScenario(const std::string& scenarioId);
    TrainingProductResult captureAppliedActions();
    // Called before dispatching Shoot so retry captures the exact pre-shot
    // authority. The dispatch result must then be confirmed or cancelled.
    TrainingProductResult markShotStart();
    TrainingProductResult confirmShotDispatched();
    void cancelShotStart() noexcept;
    // Consumes every newly retained authority step exactly once. A stopped
    // shot is evaluated immediately; only success/failure seals the replay.
    TrainingProductResult consumeAuthoritySteps();
    TrainingProductResult refreshPreview(
        const PreviewBudget& budget = PreviewBudget{});
    TrainingProductResult finishAttempt(const AttemptFacts& facts);
    // Leaves the product attempt lifecycle without mutating GameRuntime. The
    // caller that owns the next mode is responsible for establishing its own
    // authoritative state afterwards.
    TrainingProductResult abandonAttempt();
    TrainingProductResult resetAttempt();
    TrainingProductResult retryAttempt();
    TrainingProductResult prepareTeachingShot();
    TrainingProductResult saveReplay(const std::string& path) const;
    // Validates the immutable pack/file/profile/action/trace bindings, then
    // replays the document to its real terminal state and scores that state
    // against the current drill before loading a paused isolated transport.
    TrainingProductResult loadReferenceReplay(
        replay::ReplayTransportModel& transport) const;

    const std::vector<TrainingScenarioCard>& scenarios() const
    {
        return scenarios_;
    }
    const Scenario* currentScenario() const;
    const TrainingScenarioCard* currentCard() const;
    const replay::Document* replayDocument() const;
    const PreviewResult* previewResult() const;
    TrainingProductSnapshot snapshot() const;

private:
    TrainingProductResult restartRecorder();
    TrainingProductResult applyTeachingDefaults();
    int currentTeachingStepIndex() const;
    TrainingTeachingHints currentTeachingHints() const;
    TrainingProductResult finishIfTerminal();
    void resetAttemptAccumulator();
    void consumeTelemetry(const PhysicsStepTelemetry& telemetry);
    const TrainingScenarioCard* findScenario(const std::string& id) const;

    GameRuntime& runtime_;
    Session session_;
    replay::Recorder recorder_;
    TrainingProductState state_ = TrainingProductState::Empty;
    std::string packId_;
    std::vector<TrainingScenarioCard> scenarios_;
    std::set<std::string> capabilities_;
    std::string currentScenarioId_;
    std::string configuredExecutableVersion_;
    std::string executableVersion_;
    std::string contentSha256_;
    std::uint32_t seed_ = 0;
    AttemptFacts attemptFacts_;
    bool shotPrepared_ = false;
    bool shotInProgress_ = false;
    TrainingTeachingHints inFlightTeachingHints_;
    int inFlightTeachingStep_ = 0;
    ShotControlModel retryShotControl_;
    bool hasRetryShotControl_ = false;
    bool hasPhysicsJournalGeneration_ = false;
    std::uint64_t physicsJournalGeneration_ = 0;
    std::uint64_t lastPhysicsStepAttemptSequence_ = 0;
    std::array<bool, kBallCount> pocketRecorded_{};
    PreviewResult preview_;
    bool previewAuthorityUnchanged_ = false;
    Evaluation evaluation_;
};

}  // namespace billiardgl::training
