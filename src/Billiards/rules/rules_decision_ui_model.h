#pragma once

#include "rules/ball_in_hand_authority.h"
#include "rules/break_foul_authority.h"
#include "rules/eight_on_break_authority.h"
#include "rules/group_choice_authority.h"

#include <cstdint>
#include <limits>
#include <optional>
#include <string>
#include <vector>

namespace billiardgl::rules::ui {

inline constexpr const char* kRulesDecisionUiRulesetId =
    "wpa_heyball_2025_08_16_v1";
inline constexpr const char* kGroupChoiceCapabilityId =
    ::billiardgl::rules::kGroupChoiceAuthorityCapabilityId;
inline constexpr const char* kGroupChoiceFlowId =
    ::billiardgl::rules::kGroupChoiceAuthorityFlowId;
inline constexpr const char* kSection6gPendingCode =
    "rules_break_no_rack_contact_classification_pending";
inline constexpr const char* kOffTablePolicyId =
    "off_table_12_3_vs_17_e_interpretation";
inline constexpr const char* kProductRespotChoiceActionPrefix =
    "product.rules.ball_in_hand.choose_equidistant_respot_ball.";

enum class DecisionUiState {
    HiddenNoOffer,
    DecisionOpenUnselected,
    ActionArmedNotSubmitted,
    PlacementPreviewLocalInvalid,
    PlacementPreviewLocalValid,
    SubmittingExactRequest,
    AuthorityAckVerifying,
    AuthorityErrorBlocking,
    StateDriftRefreshing,
    AiOrRemotePending,
    ClassificationPendingBlocked,
    PolicyUnresolvedBlocked,
    CompletedAuthoritative,
};

enum class DecisionErrorDisplayGroup {
    OtherBlocking,
    StaleOrDrift,
    WrongActorOrOffer,
    PlacementInvalid,
    TransactionConflict,
    NoLegalResult,
    UnknownBlocking,
};

struct DecisionOfferIdentity {
    std::string rulesetId;
    std::string capabilityId;
    std::string flowId;
    std::string branchId;
    std::string decisionId;
    std::uint64_t authorityStateRevision = 0;
    int actorPlayerId = -1;
    std::vector<std::string> offeredActionIds;
    std::string sourceClause;
    int sourcePdfPage = 0;
};

struct DecisionSubmissionIntent {
    DecisionOfferIdentity offer;
    std::string actionId;
    std::string idempotencyKey;
    std::string payloadCanonical = "{}";
    bool hasPlacement = false;
    BallInHandPlacementPosition placement;
};

struct DecisionAckIdentity {
    bool accepted = false;
    std::string decisionId;
    std::string actionId;
    std::string idempotencyKey;
    int actorPlayerId = -1;
    std::uint64_t previousStateRevision = 0;
    std::uint64_t authoritativeStateRevision = 0;
    std::string authoritativeStateHash;
};

struct DecisionUiSnapshot {
    DecisionUiState state = DecisionUiState::HiddenNoOffer;
    DecisionOfferIdentity offer;
    std::vector<std::string> visibleActionIds;
    std::string armedActionId;
    std::string focusedActionId;
    std::string blockingCode;
    bool shotInputBlocked = false;
    bool placementPreviewVisible = false;
    bool placementPreviewLegal = false;
    std::string placementPreviewErrorCode;
    BallInHandPlacementPosition placementPreview;
    // Exact authority geometry retained for presentation. This is not a
    // replacement for runtime placement validation.
    double headStringWorldZCm = std::numeric_limits<double>::quiet_NaN();
    std::uint64_t localAuthorityMutationCount = 0;
};

class RulesDecisionUiModel {
public:
    void clear();

    bool present(const GroupChoiceDecisionOffer& offer,
        int localInteractivePlayerId);
    bool present(const BallInHandPlacementOffer& offer,
        int localInteractivePlayerId);
    bool present(const EightOnBreakDecisionOffer& offer,
        int localInteractivePlayerId);
    bool present(const BreakFoulDecisionOffer& offer,
        int localInteractivePlayerId);
    bool presentProductBehindHeadStringPlacement(
        const BallInHandPlacementOffer& offer,
        int localInteractivePlayerId);
    bool presentProductBehindHeadStringRespot(
        const BehindHeadStringRespotOffer& offer,
        int localInteractivePlayerId);
    void presentSection6gClassificationPending();
    void presentOffTablePolicyUnresolved();

    bool focusAction(const std::string& actionId);
    bool armAction(const std::string& actionId);
    bool setPlacementPreview(const BallInHandPlacementPosition& position,
        const BallInHandPlacementValidation& validation,
        const std::string& validatedDecisionId,
        std::uint64_t validatedAuthorityRevision);
    void resetPlacementPreview();
    void escape();
    void windowFocusLost();
    void markStateDrift();

    std::optional<DecisionSubmissionIntent> confirmArmedAction(
        const std::string& idempotencyKey);
    std::optional<DecisionSubmissionIntent> retryExactSubmission() const;
    void beginAckVerification();
    bool acceptAck(const DecisionAckIdentity& ack);
    void authorityError(const std::string& errorCode);

    const DecisionUiSnapshot& snapshot() const { return snapshot_; }
    const std::optional<DecisionSubmissionIntent>& pendingSubmission() const
    {
        return pendingSubmission_;
    }

private:
    bool presentExact(const DecisionOfferIdentity& identity,
        const std::vector<std::string>& exactExpectedActions,
        int localInteractivePlayerId);
    bool isVisibleAction(const std::string& actionId) const;
    bool isPlacementFlow() const;
    bool isFreshLegalPlacement() const;
    void failClosed(const std::string& code);

    DecisionUiSnapshot snapshot_;
    std::optional<DecisionSubmissionIntent> pendingSubmission_;
    std::string validationDecisionId_;
    std::uint64_t validationRevision_ = 0;
};

DecisionAckIdentity ackIdentity(const GroupChoiceDecisionAck& ack);
DecisionAckIdentity ackIdentity(const BallInHandPlacementAck& ack);
DecisionAckIdentity ackIdentity(const EightOnBreakDecisionAck& ack);
DecisionAckIdentity ackIdentity(const BreakFoulDecisionAck& ack);

bool isKnownDecisionErrorCode(const std::string& errorCode);
DecisionErrorDisplayGroup decisionErrorDisplayGroup(
    const std::string& errorCode);

std::optional<GroupChoiceDecisionRequest> groupChoiceRequest(
    const DecisionSubmissionIntent& intent);
std::optional<BallInHandPlacementRequest> ballInHandPlacementRequest(
    const DecisionSubmissionIntent& intent);
std::optional<EightOnBreakDecisionRequest> eightOnBreakRequest(
    const DecisionSubmissionIntent& intent);
std::optional<BreakFoulDecisionRequest> breakFoulRequest(
    const DecisionSubmissionIntent& intent);

inline std::string productRespotChoiceActionId(int ballId)
{
    return std::string(kProductRespotChoiceActionPrefix) +
        std::to_string(ballId);
}

inline std::optional<int> productRespotChoiceBallId(
    const std::string& actionId)
{
    const std::string prefix = kProductRespotChoiceActionPrefix;
    if (actionId.rfind(prefix, 0) != 0 || actionId.size() == prefix.size())
        return std::nullopt;
    int value = 0;
    for (std::size_t index = prefix.size(); index < actionId.size(); ++index) {
        const unsigned char character =
            static_cast<unsigned char>(actionId[index]);
        if (character < '0' || character > '9') return std::nullopt;
        value = value * 10 + (character - '0');
        if (value > 15) return std::nullopt;
    }
    if (value <= 0) return std::nullopt;
    return value;
}

}  // namespace billiardgl::rules::ui
