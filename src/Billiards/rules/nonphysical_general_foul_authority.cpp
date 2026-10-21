#include "rules/nonphysical_general_foul_authority.h"

namespace billiardgl::rules {
namespace {

RefereeAuthorityConsumerPolicy policy()
{
    RefereeAuthorityConsumerPolicy value;
    value.declarationKind=kNonphysicalGeneralFoulKind;
    value.allowedActorRoles={RefereeAuthorityRole::Referee};
    value.proposalPayloadKeyTypes={{"event_kind",RefereeAuthorityPayloadType::String},
        {"offending_player",RefereeAuthorityPayloadType::Number},
        {"shot_identity",RefereeAuthorityPayloadType::String}};
    value.finalPayloadKeyTypes=value.proposalPayloadKeyTypes;
    value.finalPayloadKeyTypes["proposal_decision_id"]=
        RefereeAuthorityPayloadType::String;
    value.cancelPayloadKeyTypes={{"proposal_decision_id",
        RefereeAuthorityPayloadType::String}};
    value.finalPayloadMayDifferFromProposal=true;
    value.cancelPayloadMayDifferFromProposal=true;
    value.maxPayloadBytes=1024;value.maxLifetimeTicks=100000;
    value.proposalRequired=true;value.sameActorCancelRequired=true;
    return value;
}
NonphysicalGeneralFoulResult reject(const std::string& code)
{
    NonphysicalGeneralFoulResult result;result.errorCode=code;return result;
}
std::string cancelPayload(const std::string& decision)
{
    return std::string("{\"proposal_decision_id\":\"")+decision+"\"}";
}
bool same(const NonphysicalGeneralFoulContext& a,
    const NonphysicalGeneralFoulContext& b)
{
    return a.offendingPlayer==b.offendingPlayer&&
        a.opponentPlayer==b.opponentPlayer&&a.shotIdentity==b.shotIdentity&&
        a.parentRevision==b.parentRevision&&a.parentStateHash==b.parentStateHash;
}
bool identifyProposal(const RefereeAuthorityRequest& request,
    const NonphysicalGeneralFoulContext& context,
    NonphysicalGeneralFoulKind& result)
{
    for(const auto kind:{NonphysicalGeneralFoulKind::BothFeetOffGroundDuringShot,
        NonphysicalGeneralFoulKind::IllegalBodyClothingOrEquipmentBallTouch,
        NonphysicalGeneralFoulKind::OneHandShot,
        NonphysicalGeneralFoulKind::AuxiliaryEquipmentPlacedOnTableToShoot,
        NonphysicalGeneralFoulKind::MobileDeviceDisturbanceGeneralFoul,
        NonphysicalGeneralFoulKind::MobileDeviceRepeatLossOfRack}) {
        const std::string expected=canonicalNonphysicalGeneralFoulProposal(
            kind,context.offendingPlayer,context.shotIdentity);
        if(request.payloadCanonical==expected){result=kind;return true;}
    }
    return false;
}
}

const char* nonphysicalGeneralFoulKindName(NonphysicalGeneralFoulKind kind)
{
    switch(kind){
    case NonphysicalGeneralFoulKind::BothFeetOffGroundDuringShot:
        return "BOTH_FEET_OFF_GROUND_DURING_SHOT";
    case NonphysicalGeneralFoulKind::IllegalBodyClothingOrEquipmentBallTouch:
        return "ILLEGAL_BODY_CLOTHING_OR_EQUIPMENT_BALL_TOUCH";
    case NonphysicalGeneralFoulKind::OneHandShot:return "ONE_HAND_SHOT";
    case NonphysicalGeneralFoulKind::AuxiliaryEquipmentPlacedOnTableToShoot:
        return "AUXILIARY_EQUIPMENT_PLACED_ON_TABLE_TO_SHOOT";
    case NonphysicalGeneralFoulKind::MobileDeviceDisturbanceGeneralFoul:
        return "MOBILE_DEVICE_DISTURBANCE_GENERAL_FOUL";
    case NonphysicalGeneralFoulKind::MobileDeviceRepeatLossOfRack:
        return "MOBILE_DEVICE_REPEAT_LOSS_OF_RACK";
    }
    return "UNKNOWN";
}
std::string canonicalNonphysicalGeneralFoulProposal(
    NonphysicalGeneralFoulKind kind,int player,const std::string& shot)
{
    return std::string("{\"event_kind\":\"")+nonphysicalGeneralFoulKindName(kind)+
        "\",\"offending_player\":"+std::to_string(player)+
        ",\"shot_identity\":\""+shot+"\"}";
}
std::string canonicalNonphysicalGeneralFoulFinal(
    NonphysicalGeneralFoulKind kind,int player,const std::string& proposal,
    const std::string& shot)
{
    return std::string("{\"event_kind\":\"")+nonphysicalGeneralFoulKindName(kind)+
        "\",\"offending_player\":"+std::to_string(player)+
        ",\"proposal_decision_id\":\""+proposal+
        "\",\"shot_identity\":\""+shot+"\"}";
}

NonphysicalGeneralFoulAuthority::NonphysicalGeneralFoulAuthority(
    const RefereeAuthorityAuthenticator* authenticator,
    const RefereeAuthorityCommitGuard* guard,
    NonphysicalGeneralFoulConsumerCapabilities capabilities)
    :protocol_({policy()},authenticator,guard),capabilities_(capabilities){}
void NonphysicalGeneralFoulAuthority::reset(const std::string& rack)
{
    protocol_.reset(rack);state_=NonphysicalGeneralFoulState{};
    state_.rackIdentity=rack;receiptOutcomes_.clear();
}
NonphysicalGeneralFoulResult NonphysicalGeneralFoulAuthority::exactReceipt(
    const RefereeAuthorityRequest& request) const
{
    const RefereeAuthorityResult receipt=protocol_.exactReceipt(request);
    if(!receipt.ok)return reject(receipt.errorCode);
    const auto outcome=receiptOutcomes_.find(request.idempotencyKey);
    if(outcome==receiptOutcomes_.end())
        return reject("nonphysical_general_foul_receipt_missing");
    NonphysicalGeneralFoulResult result;
    result.ok=true;result.ack=receipt.ack;result.outcome=outcome->second;
    result.idempotentReplay=true;return result;
}
NonphysicalGeneralFoulResult NonphysicalGeneralFoulAuthority::propose(
    const RefereeAuthorityRequest& request,
    const NonphysicalGeneralFoulContext& context,std::uint64_t tick)
{
    const auto receipt=protocol_.exactReceipt(request);
    if(receipt.ok)return exactReceipt(request);
    if(receipt.errorCode!="referee_authority_receipt_missing")
        return reject(receipt.errorCode);
    if(state_.phase==NonphysicalGeneralFoulPhase::Invalidated||
       (!capabilities_.structuredRackOutcomes&&
        state_.phase!=NonphysicalGeneralFoulPhase::Inactive&&
        state_.phase!=NonphysicalGeneralFoulPhase::ProposalOpen))
        return reject("nonphysical_general_foul_state");
    if(request.rackIdentity!=protocol_.state().rackIdentity||
       request.expectedAuthorityRevision!=protocol_.state().authorityRevision||
       request.expectedAuthorityStateHash!=protocol_.state().authorityStateHash)
        return reject("referee_authority_state_stale");
    NonphysicalGeneralFoulKind kind;
    if(context.offendingPlayer<0||context.offendingPlayer>1||
       context.opponentPlayer!=1-context.offendingPlayer||
       context.shotIdentity.empty()||context.parentStateHash.empty()||
       !identifyProposal(request,context,kind))
        return reject("nonphysical_general_foul_context");
    if(kind==NonphysicalGeneralFoulKind::MobileDeviceRepeatLossOfRack&&
       !capabilities_.structuredRackOutcomes)
        return reject("nonphysical_general_foul_consumer_unavailable");
    const auto mobile=state_.mobileDeviceViolations[context.offendingPlayer];
    if((kind==NonphysicalGeneralFoulKind::MobileDeviceDisturbanceGeneralFoul&&
        mobile!=MobileDeviceViolationState::None)||
       (kind==NonphysicalGeneralFoulKind::MobileDeviceRepeatLossOfRack&&
        mobile!=MobileDeviceViolationState::GeneralFoulCommitted))
        return reject("nonphysical_general_foul_mobile_sequence");
    const auto applied=protocol_.submit(request,tick);
    if(!applied.ok)return reject(applied.errorCode);
    state_.phase=NonphysicalGeneralFoulPhase::ProposalOpen;
    state_.eventKind=kind;state_.context=context;
    state_.proposalDecisionId=request.decisionId;
    state_.refereeSubjectId=request.authoritySubjectId;
    state_.proposalPayloadSha256=
        refereeAuthoritySha256Hex(request.payloadCanonical);
    state_.finalPayloadSha256.clear();state_.committed=false;
    receiptOutcomes_.emplace(request.idempotencyKey,
        NonphysicalGeneralFoulOutcome::None);
    NonphysicalGeneralFoulResult result;
    result.ok=true;result.ack=applied.ack;return result;
}
NonphysicalGeneralFoulResult NonphysicalGeneralFoulAuthority::finalDeclare(
    const RefereeAuthorityRequest& request,
    const NonphysicalGeneralFoulContext& context,std::uint64_t tick)
{
    const auto receipt=protocol_.exactReceipt(request);
    if(receipt.ok)return exactReceipt(request);
    if(receipt.errorCode!="referee_authority_receipt_missing")
        return reject(receipt.errorCode);
    if(state_.phase!=NonphysicalGeneralFoulPhase::ProposalOpen&&
       state_.phase!=NonphysicalGeneralFoulPhase::Committed)
        return reject("nonphysical_general_foul_final_state");
    if(!same(context,state_.context)||
       request.authoritySubjectId!=state_.refereeSubjectId||
       request.payloadCanonical!=canonicalNonphysicalGeneralFoulFinal(
           state_.eventKind,context.offendingPlayer,state_.proposalDecisionId,
           context.shotIdentity))
        return reject("nonphysical_general_foul_final_context");
    const auto previous=state_.phase;const auto applied=protocol_.submit(request,tick);
    if(!applied.ok)return reject(applied.errorCode);
    NonphysicalGeneralFoulOutcome outcome=
        NonphysicalGeneralFoulOutcome::GeneralFoulBallInHand;
    if(previous==NonphysicalGeneralFoulPhase::ProposalOpen){
        state_.phase=NonphysicalGeneralFoulPhase::Committed;state_.committed=true;
        state_.finalPayloadSha256=refereeAuthoritySha256Hex(request.payloadCanonical);
        if(state_.eventKind==
                NonphysicalGeneralFoulKind::MobileDeviceDisturbanceGeneralFoul)
            state_.mobileDeviceViolations[context.offendingPlayer]=
                MobileDeviceViolationState::GeneralFoulCommitted;
        else if(state_.eventKind==
                NonphysicalGeneralFoulKind::MobileDeviceRepeatLossOfRack){
            state_.mobileDeviceViolations[context.offendingPlayer]=
                MobileDeviceViolationState::RackLossCommitted;
            outcome=NonphysicalGeneralFoulOutcome::LossOfRack;
        }
    }
    receiptOutcomes_.emplace(request.idempotencyKey,outcome);
    NonphysicalGeneralFoulResult result;
    result.ok=true;result.ack=applied.ack;result.outcome=outcome;
    result.newlyCommitted=previous==NonphysicalGeneralFoulPhase::ProposalOpen;
    return result;
}
NonphysicalGeneralFoulResult NonphysicalGeneralFoulAuthority::cancel(
    const RefereeAuthorityRequest& request,std::uint64_t tick)
{
    const auto receipt=protocol_.exactReceipt(request);
    if(receipt.ok)return exactReceipt(request);
    if(receipt.errorCode!="referee_authority_receipt_missing")
        return reject(receipt.errorCode);
    if((state_.phase!=NonphysicalGeneralFoulPhase::ProposalOpen&&
        state_.phase!=NonphysicalGeneralFoulPhase::Cancelled)||
       request.authoritySubjectId!=state_.refereeSubjectId||
       request.payloadCanonical!=cancelPayload(state_.proposalDecisionId))
        return reject("nonphysical_general_foul_cancel_context");
    const auto previous=state_.phase;const auto applied=protocol_.submit(request,tick);
    if(!applied.ok)return reject(applied.errorCode);
    if(previous==NonphysicalGeneralFoulPhase::ProposalOpen)
        state_.phase=NonphysicalGeneralFoulPhase::Cancelled;
    receiptOutcomes_.emplace(request.idempotencyKey,
        NonphysicalGeneralFoulOutcome::None);
    NonphysicalGeneralFoulResult result;
    result.ok=true;result.ack=applied.ack;return result;
}
void NonphysicalGeneralFoulAuthority::invalidate()
{
    if(state_.phase!=NonphysicalGeneralFoulPhase::Committed&&
       state_.phase!=NonphysicalGeneralFoulPhase::Cancelled)
        state_.phase=NonphysicalGeneralFoulPhase::Invalidated;
    state_.committed=false;
}

}  // namespace billiardgl::rules
