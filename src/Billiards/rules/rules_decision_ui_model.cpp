#include "rules/rules_decision_ui_model.h"

#include <algorithm>
#include <cmath>
#include <iomanip>
#include <limits>
#include <locale>
#include <set>
#include <sstream>

namespace billiardgl::rules::ui {
namespace {

bool validAuthorityHash(const std::string& value)
{
    return value.size() == 64 && std::all_of(value.begin(), value.end(),
        [](unsigned char character) {
            return (character >= '0' && character <= '9') ||
                (character >= 'a' && character <= 'f');
        });
}

std::string placementPayload(const BallInHandPlacementPosition& position)
{
    std::ostringstream output;
    output.imbue(std::locale::classic());
    output << std::setprecision(std::numeric_limits<double>::max_digits10)
           << "x=" << position.worldX << ";z=" << position.worldZ;
    return output.str();
}

DecisionAckIdentity makeAckIdentity(bool accepted,
    const std::string& decisionId, const std::string& actionId,
    const std::string& idempotencyKey, int actorPlayerId,
    std::uint64_t previousRevision, std::uint64_t revision,
    const std::string& hash)
{
    return {accepted, decisionId, actionId, idempotencyKey, actorPlayerId,
        previousRevision, revision, hash};
}

const std::set<std::string> kKnownErrors{
    "rules_decision_idempotency_key_invalid",
    "rules_decision_idempotency_conflict",
    "rules_decision_context_mismatch",
    "rules_decision_stale",
    "rules_decision_stale_revision",
    "rules_decision_actor_mismatch",
    "rules_decision_action_not_offered",
    "rules_decision_invalid_payload",
    "rules_decision_invalid_state",
    "rules_group_choice_not_pending",
    "rules_placement_not_pending",
    "rules_placement_nonfinite",
    "rules_placement_out_of_bounds",
    "rules_placement_outside_head_string_area",
    "rules_placement_forbidden_geometry",
    "rules_placement_authority_geometry_invalid",
    "rules_placement_overlap",
    "rules_placement_state_changed",
    "rules_break_foul_not_pending",
    "rules_break_foul_state_changed",
    "rules_eight_on_break_not_pending",
    "rules_eight_on_break_state_changed",
    "rules_respot_no_legal_position",
    "rules_rerack_breaker_invalid",
    "rules_rerack_geometry_invalid",
    "rules_foundation_state_changed",
    "rules_foundation_plan_mismatch",
};

bool in(const std::string& value, std::initializer_list<const char*> values)
{
    return std::any_of(values.begin(), values.end(),
        [&value](const char* candidate) { return value == candidate; });
}

}  // namespace

void RulesDecisionUiModel::clear()
{
    snapshot_ = DecisionUiSnapshot{};
    pendingSubmission_.reset();
    validationDecisionId_.clear();
    validationRevision_ = 0;
}

bool RulesDecisionUiModel::presentExact(
    const DecisionOfferIdentity& identity,
    const std::vector<std::string>& exactExpectedActions,
    int localInteractivePlayerId)
{
    clear();
    snapshot_.offer = identity;
    snapshot_.shotInputBlocked = true;
    if (identity.actorPlayerId != localInteractivePlayerId) {
        snapshot_.state = DecisionUiState::AiOrRemotePending;
        return true;
    }
    if (identity.decisionId.empty() || identity.authorityStateRevision == 0 ||
        identity.offeredActionIds != exactExpectedActions) {
        failClosed("rules_decision_context_mismatch");
        return false;
    }
    snapshot_.visibleActionIds = identity.offeredActionIds;
    snapshot_.state = DecisionUiState::DecisionOpenUnselected;
    return true;
}

bool RulesDecisionUiModel::present(const GroupChoiceDecisionOffer& offer,
    int localInteractivePlayerId)
{
    return presentExact({kRulesDecisionUiRulesetId, kGroupChoiceCapabilityId,
        kGroupChoiceFlowId, "two_group_simultaneous_first_contact_and_pocket",
        offer.decisionId, offer.authorityStateRevision, offer.actorPlayerId,
        {offer.offeredActionIds.begin(), offer.offeredActionIds.end()}, "", 0},
        {kGroupChoiceSolidsActionId, kGroupChoiceStripesActionId},
        localInteractivePlayerId);
}

bool RulesDecisionUiModel::present(const BallInHandPlacementOffer& offer,
    int localInteractivePlayerId)
{
    const bool behind = offer.scope == BallInHandPlacementScope::BehindHeadString;
    const bool full = offer.scope == BallInHandPlacementScope::FullTable;
    const bool behindContextValid = !behind ||
        (offer.sourceClause == kBehindHeadStringSourceClause &&
         offer.sourcePdfPage > 0 && std::isfinite(offer.headStringWorldZCm));
    if ((!behind && !full) || !behindContextValid) {
        clear();
        snapshot_.shotInputBlocked = true;
        failClosed("rules_decision_context_mismatch");
        return false;
    }
    const std::string flow = behind ? kBehindHeadStringBallInHandFlowId :
        kFullTableBallInHandFlowId;
    const std::string branch = behind ? "behind_head_string" : "full_table";
    const std::string expected = behind ?
        kConfirmBehindHeadStringPlacementActionId :
        kConfirmFullTablePlacementActionId;
    const bool presented = presentExact({kRulesDecisionUiRulesetId,
        kFreeBallPlacementCapabilityId, flow, branch, offer.decisionId,
        offer.authorityStateRevision, offer.actorPlayerId,
        {offer.offeredActionIds.begin(), offer.offeredActionIds.end()},
        offer.sourceClause, offer.sourcePdfPage}, {expected},
        localInteractivePlayerId);
    if (presented && behind)
        snapshot_.headStringWorldZCm = offer.headStringWorldZCm;
    return presented;
}

bool RulesDecisionUiModel::presentProductBehindHeadStringPlacement(
    const BallInHandPlacementOffer& offer, int localInteractivePlayerId)
{
    const std::vector<std::string> authorityActions{
        offer.offeredActionIds.begin(), offer.offeredActionIds.end()};
    if (offer.scope != BallInHandPlacementScope::BehindHeadString ||
        offer.sourceClause != kBehindHeadStringSourceClause ||
        offer.sourcePdfPage <= 0 || !std::isfinite(offer.headStringWorldZCm) ||
        authorityActions != std::vector<std::string>{
            kConfirmBehindHeadStringPlacementActionId}) {
        clear();
        snapshot_.shotInputBlocked = true;
        failClosed("rules_decision_context_mismatch");
        return false;
    }
    const std::vector<std::string> productActions{
        kConfirmBehindHeadStringPlacementActionId,
        kRequestNearestLegalRespotActionId};
    const bool presented = presentExact({kRulesDecisionUiRulesetId,
        kFreeBallPlacementCapabilityId, kBehindHeadStringBallInHandFlowId,
        "behind_head_string_06e", offer.decisionId,
        offer.authorityStateRevision, offer.actorPlayerId, productActions,
        offer.sourceClause, offer.sourcePdfPage}, productActions,
        localInteractivePlayerId);
    if (presented) snapshot_.headStringWorldZCm = offer.headStringWorldZCm;
    return presented;
}

bool RulesDecisionUiModel::presentProductBehindHeadStringRespot(
    const BehindHeadStringRespotOffer& offer, int localInteractivePlayerId)
{
    if (offer.decisionId.empty() || offer.sourcePlacementDecisionId.empty() ||
        offer.offeredActionIds[0] != kChooseEquidistantRespotBallActionId ||
        offer.tiedCandidateBallIds.size() < 2 ||
        !std::isfinite(offer.headStringWorldZCm)) {
        clear();
        snapshot_.shotInputBlocked = true;
        failClosed("rules_decision_context_mismatch");
        return false;
    }
    std::vector<int> candidates = offer.tiedCandidateBallIds;
    if (!std::is_sorted(candidates.begin(), candidates.end()) ||
        std::adjacent_find(candidates.begin(), candidates.end()) !=
            candidates.end() ||
        std::any_of(candidates.begin(), candidates.end(),
            [](int ball) { return ball <= 0 || ball >= 16; })) {
        clear();
        snapshot_.shotInputBlocked = true;
        failClosed("rules_decision_context_mismatch");
        return false;
    }
    std::vector<std::string> actions;
    actions.reserve(candidates.size());
    for (int ball : candidates)
        actions.push_back(productRespotChoiceActionId(ball));
    const bool presented = presentExact({kRulesDecisionUiRulesetId,
        kBehindHeadStringRespotCapabilityId, kBehindHeadStringRespotFlowId,
        "behind_head_string_respot_tie", offer.decisionId,
        offer.authorityStateRevision, offer.actorPlayerId, actions,
        kBehindHeadStringSourceClause, 0}, actions,
        localInteractivePlayerId);
    if (presented) snapshot_.headStringWorldZCm = offer.headStringWorldZCm;
    return presented;
}

bool RulesDecisionUiModel::present(const EightOnBreakDecisionOffer& offer,
    int localInteractivePlayerId)
{
    const bool foul = offer.branch == EightOnBreakBranch::FoulBreak;
    const std::vector<std::string> expected = foul ?
        std::vector<std::string>{kEightOnBreakSpotBallInHandActionId,
            kEightOnBreakRerackOpponentActionId,
            kEightOnBreakRerackOriginalActionId} :
        std::vector<std::string>{kEightOnBreakSpotContinueActionId,
            kEightOnBreakRerackOriginalActionId};
    return presentExact({kRulesDecisionUiRulesetId,
        kEightOnBreakCapabilityId, kEightOnBreakFlowId,
        foul ? "foul_break" : "legal_break", offer.decisionId,
        offer.authorityStateRevision, offer.actorPlayerId,
        offer.offeredActionIds, "", 0}, expected, localInteractivePlayerId);
}

bool RulesDecisionUiModel::present(const BreakFoulDecisionOffer& offer,
    int localInteractivePlayerId)
{
    const bool foul = offer.branch == BreakFoulBranch::BreakShotFoul;
    const bool noRackMiscue =
        offer.branch == BreakFoulBranch::NoRackContactMiscue;
    const std::vector<std::string> expected = noRackMiscue ?
        std::vector<std::string>{kBreakFoulRerackOpponentActionId,
            kBreakFoulRerackOriginalActionId} : foul ?
        std::vector<std::string>{kBreakFoulBallInHandActionId,
            kBreakFoulRerackOpponentActionId,
            kBreakFoulRerackOriginalActionId} :
        std::vector<std::string>{kBreakFoulAcceptTableActionId,
            kBreakFoulRerackOpponentActionId,
            kBreakFoulRerackOriginalActionId};
    const std::string expectedClause = noRackMiscue ?
        kBreakNoRackContactSourceClause : foul ?
        kBreakShotFoulSourceClause : kIllegalBreakSourceClause;
    if (offer.sourceClause != expectedClause || offer.sourcePdfPage <= 0) {
        clear();
        snapshot_.offer.sourceClause = offer.sourceClause;
        snapshot_.shotInputBlocked = true;
        failClosed("rules_decision_context_mismatch");
        return false;
    }
    return presentExact({kRulesDecisionUiRulesetId, kBreakFoulCapabilityId,
        kBreakFoulFlowId, noRackMiscue ? "no_rack_contact_miscue_6g" :
            (foul ? "break_shot_foul_6e" : "illegal_break_6c"),
        offer.decisionId, offer.authorityStateRevision, offer.actorPlayerId,
        offer.offeredActionIds, offer.sourceClause, offer.sourcePdfPage},
        expected, localInteractivePlayerId);
}

void RulesDecisionUiModel::presentSection6gClassificationPending()
{
    clear();
    snapshot_.state = DecisionUiState::ClassificationPendingBlocked;
    snapshot_.blockingCode = kSection6gPendingCode;
    snapshot_.offer.rulesetId = kRulesDecisionUiRulesetId;
    snapshot_.offer.sourceClause = kBreakNoRackContactSourceClause;
    snapshot_.shotInputBlocked = true;
}

void RulesDecisionUiModel::presentOffTablePolicyUnresolved()
{
    clear();
    snapshot_.state = DecisionUiState::PolicyUnresolvedBlocked;
    snapshot_.blockingCode = kOffTablePolicyId;
    snapshot_.offer.rulesetId = kRulesDecisionUiRulesetId;
    snapshot_.shotInputBlocked = true;
}

bool RulesDecisionUiModel::isVisibleAction(const std::string& actionId) const
{
    return std::find(snapshot_.visibleActionIds.begin(),
        snapshot_.visibleActionIds.end(), actionId) !=
        snapshot_.visibleActionIds.end();
}

bool RulesDecisionUiModel::focusAction(const std::string& actionId)
{
    if (!isVisibleAction(actionId) ||
        (snapshot_.state != DecisionUiState::DecisionOpenUnselected &&
         snapshot_.state != DecisionUiState::ActionArmedNotSubmitted &&
         snapshot_.state != DecisionUiState::PlacementPreviewLocalValid &&
         snapshot_.state != DecisionUiState::PlacementPreviewLocalInvalid))
        return false;
    snapshot_.focusedActionId = actionId;
    return true;
}

bool RulesDecisionUiModel::isPlacementFlow() const
{
    return snapshot_.offer.flowId == kFullTableBallInHandFlowId ||
        snapshot_.offer.flowId == kBehindHeadStringBallInHandFlowId;
}

bool RulesDecisionUiModel::isFreshLegalPlacement() const
{
    return snapshot_.placementPreviewVisible && snapshot_.placementPreviewLegal &&
        validationDecisionId_ == snapshot_.offer.decisionId &&
        validationRevision_ == snapshot_.offer.authorityStateRevision;
}

bool RulesDecisionUiModel::armAction(const std::string& actionId)
{
    const bool placementConfirm =
        actionId == kConfirmFullTablePlacementActionId ||
        actionId == kConfirmBehindHeadStringPlacementActionId;
    if (!isVisibleAction(actionId) || pendingSubmission_ ||
        snapshot_.state == DecisionUiState::AiOrRemotePending ||
        snapshot_.state == DecisionUiState::AuthorityErrorBlocking ||
        snapshot_.state == DecisionUiState::StateDriftRefreshing ||
        (placementConfirm && !isFreshLegalPlacement()))
        return false;
    snapshot_.armedActionId = actionId;
    snapshot_.state = DecisionUiState::ActionArmedNotSubmitted;
    return true;
}

bool RulesDecisionUiModel::setPlacementPreview(
    const BallInHandPlacementPosition& position,
    const BallInHandPlacementValidation& validation,
    const std::string& validatedDecisionId,
    std::uint64_t validatedAuthorityRevision)
{
    if (!isPlacementFlow() || pendingSubmission_ ||
        !std::isfinite(position.worldX) || !std::isfinite(position.worldZ))
        return false;
    snapshot_.placementPreviewVisible = true;
    snapshot_.placementPreview = position;
    snapshot_.placementPreviewLegal = validation.ok &&
        validatedDecisionId == snapshot_.offer.decisionId &&
        validatedAuthorityRevision == snapshot_.offer.authorityStateRevision;
    validationDecisionId_ = validatedDecisionId;
    validationRevision_ = validatedAuthorityRevision;
    snapshot_.placementPreviewErrorCode = validation.ok ?
        std::string{} : validation.errorCode;
    snapshot_.armedActionId.clear();
    snapshot_.state = snapshot_.placementPreviewLegal ?
        DecisionUiState::PlacementPreviewLocalValid :
        DecisionUiState::PlacementPreviewLocalInvalid;
    return true;
}

void RulesDecisionUiModel::resetPlacementPreview()
{
    if (!isPlacementFlow()) return;
    snapshot_.placementPreviewVisible = false;
    snapshot_.placementPreviewLegal = false;
    snapshot_.placementPreviewErrorCode.clear();
    snapshot_.armedActionId.clear();
    validationDecisionId_.clear();
    validationRevision_ = 0;
    if (!pendingSubmission_)
        snapshot_.state = DecisionUiState::DecisionOpenUnselected;
}

void RulesDecisionUiModel::escape()
{
    switch (snapshot_.state) {
    case DecisionUiState::SubmittingExactRequest:
    case DecisionUiState::AuthorityAckVerifying:
    case DecisionUiState::AuthorityErrorBlocking:
        return;
    case DecisionUiState::PlacementPreviewLocalInvalid:
    case DecisionUiState::PlacementPreviewLocalValid:
        resetPlacementPreview();
        return;
    case DecisionUiState::ActionArmedNotSubmitted:
        snapshot_.armedActionId.clear();
        snapshot_.state = isPlacementFlow() && snapshot_.placementPreviewVisible ?
            (snapshot_.placementPreviewLegal ?
                DecisionUiState::PlacementPreviewLocalValid :
                DecisionUiState::PlacementPreviewLocalInvalid) :
            DecisionUiState::DecisionOpenUnselected;
        return;
    default:
        return;
    }
}

void RulesDecisionUiModel::windowFocusLost()
{
    snapshot_.focusedActionId.clear();
    if (snapshot_.state == DecisionUiState::ActionArmedNotSubmitted)
        escape();
}

void RulesDecisionUiModel::markStateDrift()
{
    pendingSubmission_.reset();
    snapshot_.armedActionId.clear();
    snapshot_.focusedActionId.clear();
    snapshot_.visibleActionIds.clear();
    snapshot_.placementPreviewVisible = false;
    snapshot_.placementPreviewLegal = false;
    snapshot_.state = DecisionUiState::StateDriftRefreshing;
    snapshot_.blockingCode = "rules_decision_stale";
    snapshot_.shotInputBlocked = true;
}

std::optional<DecisionSubmissionIntent>
RulesDecisionUiModel::confirmArmedAction(const std::string& idempotencyKey)
{
    const bool placementConfirm =
        snapshot_.armedActionId == kConfirmFullTablePlacementActionId ||
        snapshot_.armedActionId == kConfirmBehindHeadStringPlacementActionId;
    if (snapshot_.state != DecisionUiState::ActionArmedNotSubmitted ||
        snapshot_.armedActionId.empty() || idempotencyKey.empty() ||
        (placementConfirm && !isFreshLegalPlacement()))
        return std::nullopt;
    DecisionSubmissionIntent intent;
    intent.offer = snapshot_.offer;
    intent.actionId = snapshot_.armedActionId;
    intent.idempotencyKey = idempotencyKey;
    if (snapshot_.armedActionId == kConfirmFullTablePlacementActionId ||
        snapshot_.armedActionId == kConfirmBehindHeadStringPlacementActionId) {
        intent.hasPlacement = true;
        intent.placement = snapshot_.placementPreview;
        intent.payloadCanonical = placementPayload(intent.placement);
    }
    pendingSubmission_ = intent;
    snapshot_.state = DecisionUiState::SubmittingExactRequest;
    snapshot_.focusedActionId.clear();
    return intent;
}

std::optional<DecisionSubmissionIntent>
RulesDecisionUiModel::retryExactSubmission() const
{
    if (snapshot_.state != DecisionUiState::SubmittingExactRequest &&
        snapshot_.state != DecisionUiState::AuthorityAckVerifying)
        return std::nullopt;
    return pendingSubmission_;
}

void RulesDecisionUiModel::beginAckVerification()
{
    if (snapshot_.state == DecisionUiState::SubmittingExactRequest)
        snapshot_.state = DecisionUiState::AuthorityAckVerifying;
}

bool RulesDecisionUiModel::acceptAck(const DecisionAckIdentity& ack)
{
    if (!pendingSubmission_) return false;
    const DecisionSubmissionIntent& request = *pendingSubmission_;
    const bool exact = ack.accepted &&
        ack.decisionId == request.offer.decisionId &&
        ack.actionId == request.actionId &&
        ack.idempotencyKey == request.idempotencyKey &&
        ack.actorPlayerId == request.offer.actorPlayerId &&
        ack.previousStateRevision == request.offer.authorityStateRevision &&
        ack.previousStateRevision !=
            std::numeric_limits<std::uint64_t>::max() &&
        ack.authoritativeStateRevision == ack.previousStateRevision + 1 &&
        validAuthorityHash(ack.authoritativeStateHash);
    if (!exact) {
        markStateDrift();
        snapshot_.blockingCode = "rules_decision_context_mismatch";
        return false;
    }
    snapshot_.state = DecisionUiState::CompletedAuthoritative;
    snapshot_.shotInputBlocked = false;
    snapshot_.visibleActionIds.clear();
    snapshot_.armedActionId.clear();
    pendingSubmission_.reset();
    return true;
}

void RulesDecisionUiModel::failClosed(const std::string& code)
{
    snapshot_.state = DecisionUiState::AuthorityErrorBlocking;
    snapshot_.blockingCode = code.empty() ? "unknown_authority_error" : code;
    snapshot_.visibleActionIds.clear();
    snapshot_.armedActionId.clear();
    snapshot_.focusedActionId.clear();
    snapshot_.shotInputBlocked = true;
}

void RulesDecisionUiModel::authorityError(const std::string& errorCode)
{
    failClosed(errorCode);
}

DecisionAckIdentity ackIdentity(const GroupChoiceDecisionAck& ack)
{
    return makeAckIdentity(ack.accepted, ack.decisionId, ack.actionId,
        ack.idempotencyKey, ack.actorPlayerId, ack.previousStateRevision,
        ack.authoritativeStateRevision, ack.authoritativeStateHash);
}

DecisionAckIdentity ackIdentity(const BallInHandPlacementAck& ack)
{
    return makeAckIdentity(ack.accepted, ack.decisionId, ack.actionId,
        ack.idempotencyKey, ack.actorPlayerId, ack.previousStateRevision,
        ack.authoritativeStateRevision, ack.authoritativeStateHash);
}

DecisionAckIdentity ackIdentity(const EightOnBreakDecisionAck& ack)
{
    return makeAckIdentity(ack.accepted, ack.decisionId, ack.actionId,
        ack.idempotencyKey, ack.actorPlayerId, ack.previousStateRevision,
        ack.authoritativeStateRevision, ack.authoritativeStateHash);
}

DecisionAckIdentity ackIdentity(const BreakFoulDecisionAck& ack)
{
    return makeAckIdentity(ack.accepted, ack.decisionId, ack.actionId,
        ack.idempotencyKey, ack.actorPlayerId, ack.previousStateRevision,
        ack.authoritativeStateRevision, ack.authoritativeStateHash);
}

bool isKnownDecisionErrorCode(const std::string& errorCode)
{
    return kKnownErrors.count(errorCode) != 0;
}

DecisionErrorDisplayGroup decisionErrorDisplayGroup(
    const std::string& errorCode)
{
    if (!isKnownDecisionErrorCode(errorCode))
        return DecisionErrorDisplayGroup::UnknownBlocking;
    if (in(errorCode, {"rules_decision_stale",
            "rules_decision_stale_revision",
            "rules_placement_state_changed",
            "rules_break_foul_state_changed",
            "rules_eight_on_break_state_changed",
            "rules_foundation_state_changed"}))
        return DecisionErrorDisplayGroup::StaleOrDrift;
    if (in(errorCode, {"rules_decision_actor_mismatch",
            "rules_decision_action_not_offered",
            "rules_decision_context_mismatch"}))
        return DecisionErrorDisplayGroup::WrongActorOrOffer;
    if (in(errorCode, {"rules_placement_nonfinite",
            "rules_placement_out_of_bounds",
            "rules_placement_outside_head_string_area",
            "rules_placement_forbidden_geometry",
            "rules_placement_authority_geometry_invalid",
            "rules_placement_overlap"}))
        return DecisionErrorDisplayGroup::PlacementInvalid;
    if (in(errorCode, {"rules_decision_idempotency_key_invalid",
            "rules_decision_idempotency_conflict",
            "rules_foundation_plan_mismatch"}))
        return DecisionErrorDisplayGroup::TransactionConflict;
    if (in(errorCode, {"rules_respot_no_legal_position",
            "rules_rerack_breaker_invalid",
            "rules_rerack_geometry_invalid"}))
        return DecisionErrorDisplayGroup::NoLegalResult;
    return DecisionErrorDisplayGroup::OtherBlocking;
}

std::optional<GroupChoiceDecisionRequest> groupChoiceRequest(
    const DecisionSubmissionIntent& intent)
{
    if (intent.offer.rulesetId != kRulesDecisionUiRulesetId ||
        intent.offer.capabilityId != kGroupChoiceCapabilityId ||
        intent.offer.flowId != kGroupChoiceFlowId || intent.hasPlacement)
        return std::nullopt;
    GroupChoiceDecisionRequest request;
    request.rulesetId = intent.offer.rulesetId;
    request.capabilityId = intent.offer.capabilityId;
    request.flowId = intent.offer.flowId;
    request.decisionId = intent.offer.decisionId;
    request.expectedAuthorityStateRevision =
        intent.offer.authorityStateRevision;
    request.idempotencyKey = intent.idempotencyKey;
    request.actorPlayerId = intent.offer.actorPlayerId;
    request.actionId = intent.actionId;
    request.payloadCanonical = intent.payloadCanonical;
    return request;
}

std::optional<BallInHandPlacementRequest> ballInHandPlacementRequest(
    const DecisionSubmissionIntent& intent)
{
    if (intent.offer.rulesetId != kBallInHandRulesetId ||
        intent.offer.capabilityId != kFreeBallPlacementCapabilityId ||
        (intent.offer.flowId != kFullTableBallInHandFlowId &&
         intent.offer.flowId != kBehindHeadStringBallInHandFlowId) ||
        !intent.hasPlacement)
        return std::nullopt;
    BallInHandPlacementRequest request;
    request.rulesetId = intent.offer.rulesetId;
    request.capabilityId = intent.offer.capabilityId;
    request.flowId = intent.offer.flowId;
    request.decisionId = intent.offer.decisionId;
    request.expectedAuthorityStateRevision =
        intent.offer.authorityStateRevision;
    request.idempotencyKey = intent.idempotencyKey;
    request.actorPlayerId = intent.offer.actorPlayerId;
    request.actionId = intent.actionId;
    request.position = intent.placement;
    return request;
}

std::optional<EightOnBreakDecisionRequest> eightOnBreakRequest(
    const DecisionSubmissionIntent& intent)
{
    if (intent.offer.rulesetId != kEightOnBreakRulesetId ||
        intent.offer.capabilityId != kEightOnBreakCapabilityId ||
        intent.offer.flowId != kEightOnBreakFlowId || intent.hasPlacement)
        return std::nullopt;
    EightOnBreakDecisionRequest request;
    request.rulesetId = intent.offer.rulesetId;
    request.capabilityId = intent.offer.capabilityId;
    request.flowId = intent.offer.flowId;
    request.decisionId = intent.offer.decisionId;
    request.expectedAuthorityStateRevision =
        intent.offer.authorityStateRevision;
    request.idempotencyKey = intent.idempotencyKey;
    request.actorPlayerId = intent.offer.actorPlayerId;
    request.actionId = intent.actionId;
    request.payloadCanonical = intent.payloadCanonical;
    return request;
}

std::optional<BreakFoulDecisionRequest> breakFoulRequest(
    const DecisionSubmissionIntent& intent)
{
    if (intent.offer.rulesetId != kBreakFoulRulesetId ||
        intent.offer.capabilityId != kBreakFoulCapabilityId ||
        intent.offer.flowId != kBreakFoulFlowId || intent.hasPlacement)
        return std::nullopt;
    BreakFoulDecisionRequest request;
    request.rulesetId = intent.offer.rulesetId;
    request.capabilityId = intent.offer.capabilityId;
    request.flowId = intent.offer.flowId;
    request.decisionId = intent.offer.decisionId;
    request.expectedAuthorityStateRevision =
        intent.offer.authorityStateRevision;
    request.idempotencyKey = intent.idempotencyKey;
    request.actorPlayerId = intent.offer.actorPlayerId;
    request.actionId = intent.actionId;
    request.payloadCanonical = intent.payloadCanonical;
    return request;
}

}  // namespace billiardgl::rules::ui
