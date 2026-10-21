#pragma once

#include "game_action.h"
#include "game_state.h"
#include "cue_impact.h"
#include "cue_contact.h"
#include "physics_telemetry.h"
#include "physics_boundary_mode.h"
#include "physics_profile.h"
#include "camera/camera_state_machine.h"
#include "rules/chinese_eight_ball_rules.h"
#include "rules/ball_in_hand_authority.h"
#include "rules/break_foul_authority.h"
#include "rules/break_head_string_offense.h"
#include "rules/eight_on_break_authority.h"
#include "rules/double_hit_close_case_declaration.h"
#include "rules/group_choice_authority.h"
#include "rules/group_confusion_authority.h"
#include "rules/frozen_ball_declaration.h"
#include "rules/frozen_cushion_declaration.h"
#include "rules/rack_executor.h"
#include "rules/push_review_declaration.h"
#include "rules/nonphysical_general_foul_authority.h"
#include "rules/referee_authority_protocol.h"
#include "rules/stalemate_authority.h"
#include "rules/shot_fact_collector.h"
#include "rules/table_departure_telemetry_bridge.h"
#include "rules/unintentional_break_miscue_declaration.h"
#include "shot_control_model.h"

#include <cstdint>
#include <array>
#include <deque>
#include <map>
#include <optional>
#include <set>
#include <string>
#include <vector>

namespace billiardgl {

namespace replay { class Player; }

struct RuntimeEvent {
    RuntimeEvent() = default;
    RuntimeEvent(std::uint64_t sequenceValue, std::uint64_t tickValue, const std::string& eventName)
        : sequence(sequenceValue), tick(tickValue), name(eventName) {}
    std::uint64_t sequence = 0;
    std::uint64_t tick = 0;
    std::string name;
};

struct RuntimePhysicsStepRecord {
    std::uint64_t journalGeneration = 0;
    std::uint64_t attemptSequence = 0;
    // Successful records use the committed authority tick. Failed records use
    // the attempted next tick while GameRuntime::tick() remains unchanged.
    std::uint64_t tick = 0;
    bool publishesEvents = false;
    PhysicsStepTelemetry telemetry;
};

struct CueShotApplication {
    CueContactResult contact;
    ActionResult action;
};

enum class AppliedSemanticActionType {
    SetAimYaw,
    SetShotPower,
    SetTipOffset,
    SetCalledPocket,
    Shoot,
};

struct AppliedSemanticAction {
    std::uint64_t sequence = 0;
    std::uint64_t tick = 0;
    AppliedSemanticActionType type = AppliedSemanticActionType::SetAimYaw;
    float first = 0.0f;
    float second = 0.0f;
};

enum class RulesMode {
    Legacy,
    ChineseEightBallV1
};

enum class RulesRackTerminalReason {
    None,
    MobileDeviceRepeatViolation,
};

struct RulesRackTerminalState {
    bool active = false;
    RulesRackTerminalReason reason = RulesRackTerminalReason::None;
    int losingPlayer = -1;
    int winningPlayer = -1;
    std::string rackIdentity;
    std::string authorityDecisionId;
    std::uint64_t committedAuthorityRevision = 0;
};

enum class RuntimeRulesReplayEventType {
    NonphysicalGeneralFoulCommit,
    BallInHandPlacementCommit,
    GroupChoiceCommit,
    BreakFoulChoiceCommit,
    EightOnBreakChoiceCommit,
    BehindHeadStringRespotCommit,
    PushReviewDeclarationCommit,
    DoubleHitCloseCaseDeclarationCommit,
    FrozenBallDeclarationCommit,
    FrozenCushionDeclarationCommit,
    UnintentionalBreakMiscueDeclarationCommit,
};

enum class RuntimeRulesReplayEffectVersion {
    MobileAndPlacementV1,
    SingleRackChoicesV2,
    SingleRackDeclarationsV3,
    SingleRackMiscueDeclarationV4,
};

struct RuntimeRulesReplayEvent {
    std::uint64_t sequence = 0;
    std::uint64_t tick = 0;
    std::uint64_t afterAppliedSemanticActionSequence = 0;
    RuntimeRulesReplayEventType type =
        RuntimeRulesReplayEventType::NonphysicalGeneralFoulCommit;
    std::string rackIdentity;
    std::string decisionId;
    std::string actionId;
    rules::NonphysicalGeneralFoulKind foulKind =
        rules::NonphysicalGeneralFoulKind::BothFeetOffGroundDuringShot;
    rules::NonphysicalGeneralFoulOutcome foulOutcome =
        rules::NonphysicalGeneralFoulOutcome::None;
    int offendingPlayer = -1;
    int opponentPlayer = -1;
    int actorPlayerId = -1;
    int selectedBallId = -1;
    rules::RefereeAuthorityRole authorityRole =
        rules::RefereeAuthorityRole::Referee;
    std::string authoritySubjectId;
    std::string proposalDecisionId;
    std::string declarationRackIdentity;
    std::string factSchemaId;
    int declarationObjectBallId = -1;
    int declarationCushionId = -1;
    double worldX = 0.0;
    double worldZ = 0.0;
    std::uint64_t previousRulesRevision = 0;
    std::uint64_t committedRulesRevision = 0;
    std::string parentEffectSha256;
    std::string committedEffectSha256;
};

CueShotApplication applyCueShot(GameState& state, const CueImpactInput& input,
    const PhysicsProfile& profile,
    PhysicsBoundaryMode boundaryMode = PhysicsBoundaryMode::ProductionTable);

class GameRuntime {
public:
    static constexpr std::size_t kAppliedSemanticActionJournalCapacity = 10000;
    static constexpr std::size_t kPhysicsStepJournalCapacity = 256;
    static constexpr std::size_t kRulesReplayEventJournalCapacity = 256;

    class ReplayRulesImportToken {
    private:
        ReplayRulesImportToken() = default;
        friend class replay::Player;
    };

    GameRuntime();
    GameRuntime(
        std::vector<rules::RefereeAuthorityConsumerPolicy> refereePolicies,
        const rules::RefereeAuthorityAuthenticator* refereeAuthenticator,
        const rules::RefereeAuthorityCommitGuard* refereeCommitGuard =
            nullptr);
    void configureStalemateAuthority(
        const rules::RefereeAuthorityAuthenticator* authenticator,
        const rules::RefereeAuthorityCommitGuard* commitGuard = nullptr);
    void configureGroupConfusionAuthority(
        const rules::RefereeAuthorityAuthenticator* authenticator,
        const rules::RefereeAuthorityCommitGuard* commitGuard = nullptr);
    void configureFrozenBallDeclarationAuthority(
        const rules::RefereeAuthorityAuthenticator* authenticator,
        const rules::RefereeAuthorityCommitGuard* commitGuard = nullptr);
    void configureFrozenCushionDeclarationAuthority(
        const rules::RefereeAuthorityAuthenticator* authenticator,
        const rules::RefereeAuthorityCommitGuard* commitGuard = nullptr);
    void configureDoubleHitCloseCaseAuthority(
        const rules::RefereeAuthorityAuthenticator* authenticator,
        const rules::RefereeAuthorityCommitGuard* commitGuard = nullptr);
    void configurePushReviewAuthority(
        const rules::RefereeAuthorityAuthenticator* authenticator,
        const rules::RefereeAuthorityCommitGuard* commitGuard = nullptr);
    void configureNonphysicalGeneralFoulAuthority(
        const rules::RefereeAuthorityAuthenticator* authenticator,
        const rules::RefereeAuthorityCommitGuard* commitGuard = nullptr);
    void configureUnintentionalBreakMiscueDeclarationAuthority(
        const rules::RefereeAuthorityAuthenticator* authenticator,
        const rules::RefereeAuthorityCommitGuard* commitGuard = nullptr);

    void reset();
    ActionResult dispatch(const GameAction& action);
    ActionResult step(int count);
    // Fixed-step simulation for isolated previews. It advances the same
    // physics/rules path without publishing RuntimeEvents to external systems.
    ActionResult stepIsolated(int count);

    const GameState& state() const { return state_; }
    GameState& mutableState() { return state_; }
    std::uint64_t tick() const { return tick_; }
    const std::vector<RuntimeEvent>& events() const { return events_; }
    std::vector<RuntimeEvent> eventsSince(std::uint64_t sequence) const;
    std::uint64_t eventJournalGeneration() const
    {
        return eventJournalGeneration_;
    }
    const std::deque<RuntimePhysicsStepRecord>& recentPhysicsSteps() const
    {
        return recentPhysicsSteps_;
    }
    bool hasRecentPhysicsStep() const { return !recentPhysicsSteps_.empty(); }
    const RuntimePhysicsStepRecord& latestPhysicsStep() const
    {
        return recentPhysicsSteps_.back();
    }
    const std::deque<AppliedSemanticAction>& appliedSemanticActions() const
    {
        return appliedSemanticActions_;
    }
    std::vector<AppliedSemanticAction> appliedSemanticActionsSince(
        std::uint64_t sequence) const;
    std::uint64_t oldestAppliedSemanticActionSequence() const
    {
        return appliedSemanticActions_.empty() ? nextAppliedSemanticSequence_ :
            appliedSemanticActions_.front().sequence;
    }
    std::uint64_t appliedSemanticActionJournalGeneration() const
    {
        return appliedSemanticActionJournalGeneration_;
    }
    ActionResult setBall(int index, const BallState& ball);
    ActionResult applyCueImpact(const CueImpactInput& input);
    void replaceState(const GameState& state);
    ActionResult replaceStateForScenario(
        const GameState& state, const CueImpactInput* cueImpact = nullptr);
    ActionResult replaceStateForScenario(
        const GameState& state, const PhysicsProfile& profile,
        const CueImpactInput* cueImpact = nullptr,
        bool executeCueImpact = false,
        PhysicsBoundaryMode boundaryMode = PhysicsBoundaryMode::ProductionTable);
    void clearEvents();
    void setPhysicsTraceEnabled(bool enabled) { physicsTraceEnabled_ = enabled; }
    bool physicsTraceEnabled() const { return physicsTraceEnabled_; }
    void clearPhysicsTrace() { physicsTrace_.clear(); }
    const PhysicsTrace& physicsTrace() const { return physicsTrace_; }
    bool hasCueImpactInput() const { return hasCueImpactInput_; }
    const CueImpactInput& cueImpactInput() const { return cueImpactInput_; }
    bool hasCueContactResult() const { return hasCueContactResult_; }
    const CueContactResult& cueContactResult() const { return cueContactResult_; }
    bool hasCanonicalCueTipStrokeObservation() const
    {
        return hasCanonicalCueTipStrokeObservation_;
    }
    const CanonicalCueTipStrokeObservation& canonicalCueTipStrokeObservation()
        const { return canonicalCueTipStrokeObservation_; }
    bool canonicalCueTipStrokeObservationActive() const
    {
        return canonicalCueTipStrokeObserver_.active();
    }
    const std::string& activeCanonicalCueTipStrokeIdentity() const
    {
        return canonicalCueTipStrokeObserver_.observation().shotIdentity;
    }
    ActionResult commitAuthoritativeCueMotionObservationEnd(
        const std::string& shotIdentity, std::uint64_t endTick);
    bool d5BObservationActive() const { return d5BShotObserver_.active(); }
    bool hasTableDepartureObservation() const { return tableDepartureObserver_.committed(); }
    const rules::CanonicalTableDepartureShotObservation& tableDepartureObservation() const {
        return tableDepartureObserver_.observation();
    }
    const std::string& tableDepartureObservationError() const { return tableDepartureObservationError_; }
    const std::string& rulesShotCompletionError() const { return rulesShotCompletionError_; }
    bool hasCanonicalCueObjectOrderObservation() const
        { return hasCanonicalCueObjectOrderObservation_; }
    const CanonicalCueObjectOrderObservation& canonicalCueObjectOrderObservation()
        const { return canonicalCueObjectOrderObservation_; }
    bool hasCanonicalFrozenCushionShotObservation() const
        { return hasCanonicalFrozenCushionShotObservation_; }
    const CanonicalFrozenCushionShotObservation&
        canonicalFrozenCushionShotObservation() const
        { return canonicalFrozenCushionShotObservation_; }
    const PhysicsProfile& physicsProfile() const { return physicsProfile_; }
    PhysicsBoundaryMode boundaryMode() const { return boundaryMode_; }
    const ShotControlModel& shotControl() const { return shotControl_; }
    const ShotControlCapabilities& shotControlCapabilities() const { return shotControlCapabilities_; }
    const camera::Frame& cameraFrame() const { return cameraStateMachine_.frame(); }
    const camera::PresentationPreferences& cameraPresentationPreferences() const
        { return cameraStateMachine_.presentationPreferences(); }
    void setCameraPresentationPreferences(
        camera::PresentationPreferences preferences) noexcept;
    ActionResult setTipOffset(float x, float y);
    ActionResult setAssistLevel(AssistLevel level);
    ActionResult setCameraPreset(camera::SemanticState state, camera::Preset preset);
    ActionResult setCameraPresetInstant(camera::SemanticState state,
                                        camera::Preset preset);
    ActionResult restoreCameraFrame(const camera::Frame& frame);
    ActionResult observeFromCurrentCameraPose();
    RulesMode rulesMode() const { return rulesMode_; }
    ActionResult setRulesMode(RulesMode mode);
    const rules::MatchState& matchState() const { return matchState_; }
    const RulesRackTerminalState& rulesRackTerminalState() const
    {
        return rulesRackTerminalState_;
    }
    const std::array<rules::MobileDeviceViolationState, 2>&
        mobileDeviceViolationHistory() const
    {
        return mobileDeviceViolationHistory_;
    }
    std::string rulesReplayEffectStateHash(
        RuntimeRulesReplayEffectVersion version =
            RuntimeRulesReplayEffectVersion::SingleRackMiscueDeclarationV4) const;
    const std::deque<RuntimeRulesReplayEvent>& rulesReplayEvents() const
    {
        return rulesReplayEvents_;
    }
    std::uint64_t rulesReplayEventJournalGeneration() const
    {
        return rulesReplayEventJournalGeneration_;
    }
    ActionResult importRulesReplayEvent(const RuntimeRulesReplayEvent& event,
        RuntimeRulesReplayEffectVersion effectVersion,
        const ReplayRulesImportToken&);
    ActionResult importRulesReplayEvent(const RuntimeRulesReplayEvent& event,
        const ReplayRulesImportToken& token)
    {
        return importRulesReplayEvent(event,
            RuntimeRulesReplayEffectVersion::
                SingleRackMiscueDeclarationV4,token);
    }
    ActionResult initializeRulesReplayBaseline(std::uint64_t revision,
        const std::string& effectSha256,
        RuntimeRulesReplayEffectVersion effectVersion,
        const ReplayRulesImportToken&);
    ActionResult initializeRulesReplayBaseline(std::uint64_t revision,
        const std::string& effectSha256,const ReplayRulesImportToken& token)
    {
        return initializeRulesReplayBaseline(revision,effectSha256,
            RuntimeRulesReplayEffectVersion::
                SingleRackMiscueDeclarationV4,token);
    }
    bool hasLastShotFacts() const { return hasLastShotFacts_; }
    const rules::ShotFacts& lastShotFacts() const { return lastShotFacts_; }
    bool hasLastAdjudication() const { return hasLastAdjudication_; }
    const rules::AdjudicationResult& lastAdjudication() const { return lastAdjudication_; }
    // Derived only from a validated live/replayed commit; never grants authority
    // and is deliberately absent from canonical hashes and replay wire formats.
    struct NonphysicalFoulExplanation {
        rules::NonphysicalGeneralFoulKind kind;
        std::string decisionId;
        std::string rackIdentity;
        int offendingPlayer;
        int opponentPlayer;
    };
    const std::optional<NonphysicalFoulExplanation>& nonphysicalFoulExplanation() const {
        return nonphysicalFoulExplanation_;
    }
    bool breakHeadStringReleasePending() const
    {
        return pendingBreakHeadStringRelease_.has_value();
    }
    std::uint32_t breakHeadStringOffenseCount(int player) const
    {
        return breakHeadStringOffenseAuthority_.offenseCount(player);
    }
    std::optional<rules::BreakHeadStringOffenseReceipt>
    lastBreakHeadStringOffense() const
    {
        return breakHeadStringOffenseAuthority_.lastReceipt();
    }
    bool shotFactCollectorActive() const { return shotFactCollector_.active(); }
    bool behindHeadStringFirstContactRestrictionPending() const
    {
        return behindHeadStringFirstContactContext_.has_value();
    }
    ActionResult setCalledPocket(int pocket);
    ActionResult adjudicateRulesShot(const rules::ShotFacts& facts);
    std::optional<rules::GroupChoiceDecisionOffer>
        groupChoiceDecisionOffer() const { return groupChoiceDecisionOffer_; }
    std::uint64_t rulesAuthorityRevision() const
    {
        return rulesAuthorityRevision_;
    }
    std::string rulesAuthorityStateHash() const;
    rules::GroupChoiceDecisionResult submitGroupChoiceDecision(
        const rules::GroupChoiceDecisionRequest& request);
    rules::BallInHandPlacementScope ballInHandPlacementScope() const
    {
        return ballInHandPlacementScope_;
    }
    bool ballInHandPlacementPending() const
    {
        return ballInHandPlacementScope_ !=
            rules::BallInHandPlacementScope::None;
    }
    std::optional<rules::BallInHandPlacementOffer>
        ballInHandPlacementOffer() const { return ballInHandPlacementOffer_; }
    rules::BallInHandPlacementValidation validateBallInHandPlacement(
        const rules::BallInHandPlacementPosition& position) const;
    rules::BallInHandPlacementResult submitBallInHandPlacement(
        const rules::BallInHandPlacementRequest& request);
    std::optional<rules::BehindHeadStringRespotOffer>
        behindHeadStringRespotOffer() const
    {
        return behindHeadStringRespotOffer_;
    }
    rules::BehindHeadStringRespotResult submitBehindHeadStringRespot(
        const rules::BehindHeadStringRespotRequest& request);
    bool breakFoulDecisionPending() const
    {
        return breakFoulDecisionOffer_.has_value();
    }
    std::optional<rules::BreakFoulDecisionOffer>
        breakFoulDecisionOffer() const
    {
        return breakFoulDecisionOffer_;
    }
    rules::BreakFoulDecisionResult submitBreakFoulDecision(
        const rules::BreakFoulDecisionRequest& request);
    bool eightOnBreakDecisionPending() const
    {
        return eightOnBreakDecisionOffer_.has_value();
    }
    std::optional<rules::EightOnBreakDecisionOffer>
        eightOnBreakDecisionOffer() const
    {
        return eightOnBreakDecisionOffer_;
    }
    rules::EightOnBreakDecisionResult submitEightOnBreakDecision(
        const rules::EightOnBreakDecisionRequest& request);
    rules::RackFoundationPlan stageOfficialRerack(
        const rules::RerackFoundationParameters& parameters) const;
    rules::RackFoundationPlan stageBallRespot(
        const rules::RespotFoundationParameters& parameters) const;
    rules::RackFoundationResult commitRackFoundation(
        const rules::RackFoundationPlan& plan,
        const std::string& idempotencyKey);
    const std::deque<rules::RulesAuthorityAction>& rulesAuthorityActions() const
    {
        return rulesAuthorityActions_;
    }
    const rules::RefereeAuthorityState& refereeAuthorityState() const
    {
        return refereeAuthorityProtocol_.state();
    }
    rules::RefereeAuthorityResult submitRefereeAuthority(
        const rules::RefereeAuthorityRequest& request)
    {
        return refereeAuthorityProtocol_.submit(request, tick_);
    }
    const rules::StalemateState& stalemateState() const;
    const rules::RefereeAuthorityState& stalemateProtocolState() const;
    rules::StalemateResult submitStalemateProposal(
        const rules::RefereeAuthorityRequest& request);
    rules::StalemateResult submitStalemateRefusal(
        const rules::RefereeAuthorityRequest& request);
    rules::StalemateResult submitStalemateFinal(
        const rules::RefereeAuthorityRequest& request);
    rules::StalemateResult submitStalemateCancel(
        const rules::RefereeAuthorityRequest& request);
    const rules::GroupConfusionState& groupConfusionState() const;
    const rules::RefereeAuthorityState&
        groupConfusionProtocolState() const;
    rules::GroupConfusionResult submitGroupConfusionProposal(
        const rules::RefereeAuthorityRequest& request);
    rules::GroupConfusionResult submitGroupConfusionFinal(
        const rules::RefereeAuthorityRequest& request);
    const rules::FrozenBallDeclarationState&
        frozenBallDeclarationState() const;
    const rules::RefereeAuthorityState&
        frozenBallDeclarationProtocolState() const;
    rules::FrozenBallDeclarationResult submitFrozenBallDeclarationProposal(
        const rules::RefereeAuthorityRequest& request);
    rules::FrozenBallDeclarationResult submitFrozenBallDeclarationFinal(
        const rules::RefereeAuthorityRequest& request);
    rules::FrozenBallDeclarationResult submitFrozenBallDeclarationCancel(
        const rules::RefereeAuthorityRequest& request);
    const rules::FrozenCushionDeclarationState&
        frozenCushionDeclarationState() const;
    const rules::RefereeAuthorityState&
        frozenCushionDeclarationProtocolState() const;
    rules::FrozenCushionDeclarationResult
        submitFrozenCushionDeclarationProposal(
            const rules::RefereeAuthorityRequest& request);
    rules::FrozenCushionDeclarationResult
        submitFrozenCushionDeclarationFinal(
            const rules::RefereeAuthorityRequest& request);
    rules::FrozenCushionDeclarationResult
        submitFrozenCushionDeclarationCancel(
            const rules::RefereeAuthorityRequest& request);
    const rules::DoubleHitCloseCaseState& doubleHitCloseCaseState() const;
    const rules::RefereeAuthorityState&
        doubleHitCloseCaseProtocolState() const;
    rules::DoubleHitCloseCaseResult submitDoubleHitCloseCaseProposal(
        const rules::RefereeAuthorityRequest& request);
    rules::DoubleHitCloseCaseResult submitDoubleHitCloseCaseFinal(
        const rules::RefereeAuthorityRequest& request);
    rules::DoubleHitCloseCaseResult submitDoubleHitCloseCaseCancel(
        const rules::RefereeAuthorityRequest& request);
    const rules::PushReviewState& pushReviewState() const;
    const rules::RefereeAuthorityState& pushReviewProtocolState() const;
    rules::PushReviewResult submitPushReviewProposal(
        const rules::RefereeAuthorityRequest& request);
    rules::PushReviewResult submitPushReviewFinal(
        const rules::RefereeAuthorityRequest& request);
    rules::PushReviewResult submitPushReviewCancel(
        const rules::RefereeAuthorityRequest& request);
    const rules::NonphysicalGeneralFoulState&
        nonphysicalGeneralFoulState() const;
    const rules::RefereeAuthorityState&
        nonphysicalGeneralFoulProtocolState() const;
    rules::NonphysicalGeneralFoulResult
        submitNonphysicalGeneralFoulProposal(
            const rules::RefereeAuthorityRequest& request);
    rules::NonphysicalGeneralFoulResult submitNonphysicalGeneralFoulFinal(
        const rules::RefereeAuthorityRequest& request);
    rules::NonphysicalGeneralFoulResult submitNonphysicalGeneralFoulCancel(
        const rules::RefereeAuthorityRequest& request);
    const rules::UnintentionalBreakMiscueDeclarationState&
        unintentionalBreakMiscueDeclarationState() const;
    const rules::RefereeAuthorityState&
        unintentionalBreakMiscueDeclarationProtocolState() const;
    rules::UnintentionalBreakMiscueDeclarationResult
        submitUnintentionalBreakMiscueDeclarationProposal(
            const rules::RefereeAuthorityRequest& request);
    rules::UnintentionalBreakMiscueDeclarationResult
        submitUnintentionalBreakMiscueDeclarationFinal(
            const rules::RefereeAuthorityRequest& request);
    rules::UnintentionalBreakMiscueDeclarationResult
        submitUnintentionalBreakMiscueDeclarationCancel(
            const rules::RefereeAuthorityRequest& request);

private:
    ActionResult applyShot();
    ActionResult stepInternal(int count, bool publishEvents);
    void recordEvents();
    void syncShotControlFromLegacyState();
    void resetPresentationModels();
    void updateCameraModel(float deltaSeconds);
    ShotControlModel promotedShotControl() const;
    void resetRulesState();
    void finishRulesShot(bool publishEvents);
    void openGroupConfusionWindowForCompletedShot(
        const rules::ShotFacts& facts);
    void sealPendingRulesDecisionStateHashes();
    ActionResult applyRulesAdjudication(const rules::ShotFacts& facts,
        bool publishEvents,
        const rules::BreakHeadStringReleaseSample* breakRelease = nullptr,
        const CueContactResult* authoritativeStroke = nullptr);
    void recordAppliedSemanticAction(AppliedSemanticActionType type,
        float first = 0.0f, float second = 0.0f);
    void recordPhysicsStep(PhysicsStepTelemetry telemetry,
        std::uint64_t attemptedTick, bool publishesEvents);
    void invalidateGroupChoiceAuthority();
    void invalidateBallInHandAuthority();
    void invalidateBehindHeadStringRespotAuthority();
    void stageBallInHandPlacementAuthority(
        rules::BallInHandPlacementScope scope, int actorPlayerId);
    void invalidateBreakFoulAuthority();
    void invalidateEightOnBreakAuthority();
    std::string currentRulesReplayRackIdentity() const;
    void invalidateRackFoundationAuthority();
    void applyVerifiedRerackFoundation(
        const rules::RackFoundationPlan& plan);
    void applyVerifiedRespotFoundation(
        const rules::RackFoundationPlan& plan);
    ActionResult applyCommittedNonphysicalGeneralFoulEffect(
        rules::NonphysicalGeneralFoulKind kind,
        const rules::NonphysicalGeneralFoulContext& context,
        rules::NonphysicalGeneralFoulOutcome outcome,
        const std::string& decisionId, const std::string& rackIdentity,
        bool appendReplayEvent);
    void appendCommittedDeclarationReplayEvent(
        RuntimeRulesReplayEventType type,
        const rules::RefereeAuthorityRequest& finalRequest,
        const std::string& proposalDecisionId,
        const std::string& rackIdentity,
        const std::string& factSchemaId,
        int objectBallId, int cushionId,
        const std::string& parentEffectSha256,
        int actorPlayerId = -1);
    void resetRulesReplayJournal();
    const char* pendingRulesDecisionError() const;

    struct GroupChoiceReceipt {
        rules::GroupChoiceDecisionRequest request;
        rules::GroupChoiceDecisionAck ack;
    };

    struct BallInHandPlacementReceipt {
        rules::BallInHandPlacementRequest request;
        rules::BallInHandPlacementAck ack;
    };

    struct BehindHeadStringRespotReceipt {
        rules::BehindHeadStringRespotRequest request;
        rules::BehindHeadStringRespotAck ack;
    };

    struct RackFoundationReceipt {
        rules::RackFoundationPlan plan;
        rules::RackFoundationAck ack;
    };

    struct BreakFoulReceipt {
        rules::BreakFoulDecisionRequest request;
        rules::BreakFoulDecisionAck ack;
    };

    struct EightOnBreakReceipt {
        rules::EightOnBreakDecisionRequest request;
        rules::EightOnBreakDecisionAck ack;
    };

    GameState state_;
    std::uint64_t tick_ = 0;
    std::uint64_t nextSequence_ = 1;
    std::uint64_t eventJournalGeneration_ = 0;
    std::vector<RuntimeEvent> events_;
    std::uint64_t nextPhysicsStepAttemptSequence_ = 1;
    std::deque<RuntimePhysicsStepRecord> recentPhysicsSteps_;
    std::uint64_t nextAppliedSemanticSequence_ = 1;
    std::uint64_t appliedSemanticActionJournalGeneration_ = 0;
    std::deque<AppliedSemanticAction> appliedSemanticActions_;
    bool physicsTraceEnabled_ = false;
    PhysicsTrace physicsTrace_{10000};
    bool hasCueImpactInput_ = false;
    CueImpactInput cueImpactInput_;
    bool hasCueContactResult_ = false;
    CueContactResult cueContactResult_;
    bool cueContactPending_ = false;
    bool hasCanonicalCueTipStrokeObservation_ = false;
    bool canonicalCueTipStrokeObservationPending_ = false;
    CanonicalCueTipStrokeObservation canonicalCueTipStrokeObservation_;
    CanonicalCueTipStrokeObserver canonicalCueTipStrokeObserver_;
    CanonicalD5BShotObserver d5BShotObserver_;
    rules::CanonicalTableDepartureObserver tableDepartureObserver_;
    std::array<int, kBallCount> activeOutsideLandingSurfaceIds_ = [] {
        std::array<int, kBallCount> ids{};
        ids.fill(-1);
        return ids;
    }();
    std::string tableDepartureObservationError_;
    std::string rulesShotCompletionError_;
    bool hasCanonicalCueObjectOrderObservation_ = false;
    CanonicalCueObjectOrderObservation canonicalCueObjectOrderObservation_;
    bool hasCanonicalFrozenCushionShotObservation_ = false;
    CanonicalFrozenCushionShotObservation
        canonicalFrozenCushionShotObservation_;
    PhysicsProfile physicsProfile_;
    PhysicsBoundaryMode boundaryMode_ = PhysicsBoundaryMode::ProductionTable;
    ShotControlModel shotControl_;
    ShotControlCapabilities shotControlCapabilities_;
    camera::StateMachine cameraStateMachine_;
    RulesMode rulesMode_ = RulesMode::Legacy;
    rules::ChineseEightBallRules chineseEightBallRules_;
    rules::MatchState matchState_;
    RulesRackTerminalState rulesRackTerminalState_;
    std::array<rules::MobileDeviceViolationState, 2>
        mobileDeviceViolationHistory_{{
            rules::MobileDeviceViolationState::None,
            rules::MobileDeviceViolationState::None}};
    std::string replayRulesRackIdentity_;
    rules::ShotFactCollector shotFactCollector_;
    std::optional<rules::BehindHeadStringFirstContactContext>
        behindHeadStringFirstContactContext_;
    rules::BreakHeadStringOffenseAuthority
        breakHeadStringOffenseAuthority_;
    std::uint64_t nextBreakHeadStringShotIdentity_ = 1;
    std::optional<rules::BreakHeadStringReleaseSample>
        pendingBreakHeadStringRelease_;
    bool hasLastShotFacts_ = false;
    rules::ShotFacts lastShotFacts_;
    bool hasLastAdjudication_ = false;
    rules::AdjudicationResult lastAdjudication_;
    std::optional<NonphysicalFoulExplanation> nonphysicalFoulExplanation_;
    int pendingCalledPocket_ = -1;
    std::uint64_t rulesAuthorityRevision_ = 0;
    std::uint64_t nextGroupChoiceDecisionOrdinal_ = 1;
    std::optional<rules::GroupChoiceDecisionOffer> groupChoiceDecisionOffer_;
    std::string groupChoiceOfferStateHash_;
    std::map<std::string, GroupChoiceReceipt> groupChoiceReceipts_;
    std::set<std::string> staleGroupChoiceDecisionIds_;
    rules::BallInHandPlacementScope ballInHandPlacementScope_ =
        rules::BallInHandPlacementScope::None;
    std::uint64_t nextBallInHandDecisionOrdinal_ = 1;
    std::optional<rules::BallInHandPlacementOffer>
        ballInHandPlacementOffer_;
    std::string ballInHandPlacementOfferStateHash_;
    std::map<std::string, BallInHandPlacementReceipt>
        ballInHandPlacementReceipts_;
    std::set<std::string> staleBallInHandDecisionIds_;
    std::uint64_t nextBehindHeadStringRespotDecisionOrdinal_ = 1;
    std::optional<rules::BehindHeadStringRespotOffer>
        behindHeadStringRespotOffer_;
    std::string behindHeadStringRespotOfferStateHash_;
    std::map<std::string, BehindHeadStringRespotReceipt>
        behindHeadStringRespotReceipts_;
    std::set<std::string> staleBehindHeadStringRespotDecisionIds_;
    std::uint64_t nextBreakFoulDecisionOrdinal_ = 1;
    std::optional<rules::BreakFoulDecisionOffer>
        breakFoulDecisionOffer_;
    std::string breakFoulOfferStateHash_;
    std::map<std::string, BreakFoulReceipt> breakFoulReceipts_;
    std::set<std::string> staleBreakFoulDecisionIds_;
    std::uint64_t nextEightOnBreakDecisionOrdinal_ = 1;
    std::optional<rules::EightOnBreakDecisionOffer>
        eightOnBreakDecisionOffer_;
    std::string eightOnBreakOfferStateHash_;
    std::map<std::string, EightOnBreakReceipt>
        eightOnBreakReceipts_;
    std::set<std::string> staleEightOnBreakDecisionIds_;
    std::map<std::string, RackFoundationReceipt>
        rackFoundationReceipts_;
    std::uint64_t nextRulesAuthorityActionSequence_ = 1;
    std::deque<rules::RulesAuthorityAction> rulesAuthorityActions_;
    std::uint64_t rulesReplayEventJournalGeneration_ = 0;
    std::uint64_t nextRulesReplayEventSequence_ = 1;
    std::deque<RuntimeRulesReplayEvent> rulesReplayEvents_;
    bool replayRulesImportActive_ = false;
    bool replayDeclarationAuthorityState_ = false;
    rules::RefereeAuthorityProtocol refereeAuthorityProtocol_;
    std::optional<rules::StalemateAuthority> stalemateAuthority_;
    std::optional<rules::GroupConfusionAuthority>
        groupConfusionAuthority_;
    std::optional<rules::FrozenBallDeclarationAuthority>
        frozenBallDeclarationAuthority_;
    std::optional<rules::FrozenCushionDeclarationAuthority>
        frozenCushionDeclarationAuthority_;
    std::optional<rules::DoubleHitCloseCaseAuthority>
        doubleHitCloseCaseAuthority_;
    std::optional<rules::PushReviewAuthority> pushReviewAuthority_;
    std::optional<rules::NonphysicalGeneralFoulAuthority>
        nonphysicalGeneralFoulAuthority_;
    std::optional<rules::UnintentionalBreakMiscueDeclarationAuthority>
        unintentionalBreakMiscueDeclarationAuthority_;
    int rackOriginalBreakerPlayerId_ = 0;
    std::array<int, 2> rackOriginalShootingOrder_{{0, 1}};
    std::uint64_t stalemateRackIdentityOrdinal_ = 0;
    bool stalemateRerackCommitInProgress_ = false;
    std::uint64_t groupConfusionRackIdentityOrdinal_ = 0;
    std::uint64_t groupConfusionCompletedShotOrdinal_ = 0;
    bool groupConfusionRerackCommitInProgress_ = false;
    std::uint64_t frozenBallDeclarationRackIdentityOrdinal_ = 0;
    std::uint64_t nextFrozenBallDeclarationShotIdentity_ = 1;
    std::uint64_t frozenCushionDeclarationRackIdentityOrdinal_ = 0;
    std::uint64_t nextFrozenCushionDeclarationShotIdentity_ = 1;
    std::uint64_t doubleHitCloseCaseRackIdentityOrdinal_ = 0;
    std::uint64_t nextDoubleHitCloseCaseShotIdentity_ = 1;
    std::uint64_t pushReviewRackIdentityOrdinal_ = 0;
    std::uint64_t nextPushReviewShotIdentity_ = 1;
    std::uint64_t nonphysicalGeneralFoulRackIdentityOrdinal_ = 0;
    std::uint64_t unintentionalBreakMiscueRackIdentityOrdinal_ = 0;
};

}  // namespace billiardgl
