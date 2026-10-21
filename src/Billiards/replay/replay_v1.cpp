#include "replay/replay_v1.h"

#include <algorithm>
#include <cmath>
#include <iomanip>
#include <limits>
#include <sstream>
#include <stdexcept>
#include <utility>

namespace billiardgl::replay {
namespace {

const char* actionName(SemanticActionType type)
{
    switch (type) {
    case SemanticActionType::SetAimYaw: return "set_aim_yaw";
    case SemanticActionType::SetShotPower: return "set_shot_power";
    case SemanticActionType::SetTipOffset: return "set_tip_offset";
    case SemanticActionType::SetCalledPocket: return "set_called_pocket";
    case SemanticActionType::Shoot: return "shoot";
    }
    return "unknown";
}

bool parseActionName(const std::string& name, SemanticActionType& type)
{
    if (name == "set_aim_yaw") type = SemanticActionType::SetAimYaw;
    else if (name == "set_shot_power") type = SemanticActionType::SetShotPower;
    else if (name == "set_tip_offset") type = SemanticActionType::SetTipOffset;
    else if (name == "set_called_pocket") type = SemanticActionType::SetCalledPocket;
    else if (name == "shoot") type = SemanticActionType::Shoot;
    else return false;
    return true;
}

const char* rulesEventName(RuntimeRulesReplayEventType type)
{
    switch(type){
    case RuntimeRulesReplayEventType::NonphysicalGeneralFoulCommit:
        return "nonphysical_general_foul_commit";
    case RuntimeRulesReplayEventType::BallInHandPlacementCommit:
        return "ball_in_hand_placement_commit";
    case RuntimeRulesReplayEventType::GroupChoiceCommit:
        return "group_choice_commit";
    case RuntimeRulesReplayEventType::BreakFoulChoiceCommit:
        return "break_foul_choice_commit";
    case RuntimeRulesReplayEventType::EightOnBreakChoiceCommit:
        return "eight_on_break_choice_commit";
    case RuntimeRulesReplayEventType::BehindHeadStringRespotCommit:
        return "behind_head_string_respot_commit";
    case RuntimeRulesReplayEventType::PushReviewDeclarationCommit:
        return "push_review_declaration_commit";
    case RuntimeRulesReplayEventType::DoubleHitCloseCaseDeclarationCommit:
        return "double_hit_close_case_declaration_commit";
    case RuntimeRulesReplayEventType::FrozenBallDeclarationCommit:
        return "frozen_ball_declaration_commit";
    case RuntimeRulesReplayEventType::FrozenCushionDeclarationCommit:
        return "frozen_cushion_declaration_commit";
    case RuntimeRulesReplayEventType::UnintentionalBreakMiscueDeclarationCommit:
        return "unintentional_break_miscue_declaration_commit";
    }
    return "unknown";
}

bool parseRulesEventName(const std::string& name,
    RuntimeRulesReplayEventType& type)
{
    if(name=="nonphysical_general_foul_commit")
        type=RuntimeRulesReplayEventType::NonphysicalGeneralFoulCommit;
    else if(name=="ball_in_hand_placement_commit")
        type=RuntimeRulesReplayEventType::BallInHandPlacementCommit;
    else if(name=="group_choice_commit")
        type=RuntimeRulesReplayEventType::GroupChoiceCommit;
    else if(name=="break_foul_choice_commit")
        type=RuntimeRulesReplayEventType::BreakFoulChoiceCommit;
    else if(name=="eight_on_break_choice_commit")
        type=RuntimeRulesReplayEventType::EightOnBreakChoiceCommit;
    else if(name=="behind_head_string_respot_commit")
        type=RuntimeRulesReplayEventType::BehindHeadStringRespotCommit;
    else if(name=="push_review_declaration_commit")
        type=RuntimeRulesReplayEventType::PushReviewDeclarationCommit;
    else if(name=="double_hit_close_case_declaration_commit")
        type=RuntimeRulesReplayEventType::DoubleHitCloseCaseDeclarationCommit;
    else if(name=="frozen_ball_declaration_commit")
        type=RuntimeRulesReplayEventType::FrozenBallDeclarationCommit;
    else if(name=="frozen_cushion_declaration_commit")
        type=RuntimeRulesReplayEventType::FrozenCushionDeclarationCommit;
    else if(name=="unintentional_break_miscue_declaration_commit")
        type=RuntimeRulesReplayEventType::
            UnintentionalBreakMiscueDeclarationCommit;
    else return false;
    return true;
}

bool parseFoulKind(const std::string& name,
    rules::NonphysicalGeneralFoulKind& result)
{
    for(const auto kind:{
        rules::NonphysicalGeneralFoulKind::BothFeetOffGroundDuringShot,
        rules::NonphysicalGeneralFoulKind::IllegalBodyClothingOrEquipmentBallTouch,
        rules::NonphysicalGeneralFoulKind::OneHandShot,
        rules::NonphysicalGeneralFoulKind::AuxiliaryEquipmentPlacedOnTableToShoot,
        rules::NonphysicalGeneralFoulKind::MobileDeviceDisturbanceGeneralFoul,
        rules::NonphysicalGeneralFoulKind::MobileDeviceRepeatLossOfRack}){
        if(name==rules::nonphysicalGeneralFoulKindName(kind)){
            result=kind;return true;
        }
    }
    return false;
}

const char* outcomeName(rules::NonphysicalGeneralFoulOutcome outcome)
{
    return outcome==rules::NonphysicalGeneralFoulOutcome::GeneralFoulBallInHand?
        "general_foul_ball_in_hand":outcome==
        rules::NonphysicalGeneralFoulOutcome::LossOfRack?
        "loss_of_rack":"none";
}

bool parseOutcome(const std::string& name,
    rules::NonphysicalGeneralFoulOutcome& outcome)
{
    if(name=="general_foul_ball_in_hand")
        outcome=rules::NonphysicalGeneralFoulOutcome::GeneralFoulBallInHand;
    else if(name=="loss_of_rack")
        outcome=rules::NonphysicalGeneralFoulOutcome::LossOfRack;
    else if(name=="none")
        outcome=rules::NonphysicalGeneralFoulOutcome::None;
    else return false;
    return true;
}

json::Value pointValue(const Point3& point)
{
    json::Value result = json::Value::array();
    result.asArray().push_back(json::Value(point.x));
    result.asArray().push_back(json::Value(point.y));
    result.asArray().push_back(json::Value(point.z));
    return result;
}

Point3 parsePoint(const json::Value& value)
{
    const json::Value::Array& parts = value.asArray();
    if (parts.size() != 3) throw std::runtime_error("point must have 3 components");
    return Point3{static_cast<float>(parts[0].asNumber()),
        static_cast<float>(parts[1].asNumber()),
        static_cast<float>(parts[2].asNumber())};
}

bool validHash(const std::string& value)
{
    return value.size() == 64 && std::all_of(value.begin(), value.end(),
        [](unsigned char c) { return (c >= '0' && c <= '9') || (c >= 'a' && c <= 'f'); });
}

bool comprehensiveRulesSchema(const std::string& schema)
{
    return schema==kReplaySingleRackChoiceReceiptSchema||
        schema==kReplaySingleRackDeclarationReceiptSchema||
        schema==kReplaySingleRackMiscueDeclarationReceiptSchema;
}

bool declarationRulesSchema(const std::string& schema)
{
    return schema==kReplaySingleRackDeclarationReceiptSchema||
        schema==kReplaySingleRackMiscueDeclarationReceiptSchema;
}

bool miscueDeclarationRulesSchema(const std::string& schema)
{
    return schema==kReplaySingleRackMiscueDeclarationReceiptSchema;
}

bool declarationEvent(RuntimeRulesReplayEventType type)
{
    return type==RuntimeRulesReplayEventType::PushReviewDeclarationCommit||
        type==RuntimeRulesReplayEventType::DoubleHitCloseCaseDeclarationCommit||
        type==RuntimeRulesReplayEventType::FrozenBallDeclarationCommit||
        type==RuntimeRulesReplayEventType::FrozenCushionDeclarationCommit||
        type==RuntimeRulesReplayEventType::
            UnintentionalBreakMiscueDeclarationCommit;
}

RuntimeRulesReplayEffectVersion effectVersionFor(
    const std::string& schema)
{
    if(miscueDeclarationRulesSchema(schema))
        return RuntimeRulesReplayEffectVersion::
            SingleRackMiscueDeclarationV4;
    if(declarationRulesSchema(schema))
        return RuntimeRulesReplayEffectVersion::SingleRackDeclarationsV3;
    return comprehensiveRulesSchema(schema)?
        RuntimeRulesReplayEffectVersion::SingleRackChoicesV2:
        RuntimeRulesReplayEffectVersion::MobileAndPlacementV1;
}

std::uint64_t exactUnsigned(const json::Value& value,
    std::uint64_t maximum, const char* name)
{
    if (!value.isNumber()) throw std::runtime_error(std::string(name) + " must be number");
    const double number = value.asNumber();
    if (!std::isfinite(number) || number < 0.0 || std::floor(number) != number ||
        number > static_cast<double>(maximum))
        throw std::runtime_error(std::string(name) + " must be an exact unsigned integer");
    return static_cast<std::uint64_t>(number);
}

}  // namespace

std::string rulesReceiptSha256(const RulesReceipt& receipt)
{
    const auto& event=receipt.event;
    std::ostringstream canonical;
    canonical << "replay_rules_receipt_v1|" << receipt.tick << '|'
              << receipt.sequence << '|'
              << receipt.afterSemanticActionOrdinal << '|'
              << rulesEventName(event.type) << '|'
              << event.rackIdentity.size() << ':' << event.rackIdentity << '|'
              << event.decisionId.size() << ':' << event.decisionId << '|'
              << event.actionId.size() << ':' << event.actionId << '|'
              << rules::nonphysicalGeneralFoulKindName(event.foulKind) << '|'
              << outcomeName(event.foulOutcome) << '|'
              << event.offendingPlayer << '|' << event.opponentPlayer << '|'
              << event.actorPlayerId << '|';
    if(event.type==RuntimeRulesReplayEventType::BehindHeadStringRespotCommit)
        canonical << event.selectedBallId << '|';
    if(declarationEvent(event.type))
        canonical << rules::refereeAuthorityRoleName(event.authorityRole) << '|'
                  << event.authoritySubjectId.size() << ':'
                  << event.authoritySubjectId << '|'
                  << event.proposalDecisionId.size() << ':'
                  << event.proposalDecisionId << '|'
                  << event.declarationRackIdentity.size() << ':'
                  << event.declarationRackIdentity << '|'
                  << event.factSchemaId.size() << ':' << event.factSchemaId
                  << '|' << event.declarationObjectBallId << '|'
                  << event.declarationCushionId << '|';
    canonical << std::setprecision(std::numeric_limits<double>::max_digits10)
              << event.worldX << '|' << event.worldZ << '|'
              << event.previousRulesRevision << '|'
              << event.committedRulesRevision << '|'
              << event.parentEffectSha256 << '|'
              << event.committedEffectSha256;
    return sha256Hex(canonical.str());
}

std::string physicsProfileSha256(const PhysicsProfile& profile)
{
    return sha256Hex(canonicalPhysicsProfileJson(profile));
}

std::string replayAuthorityStateSha256(const GameState& state)
{
    json::Value authority = json::Value::object();
    authority["base_canonical_game_state"] =
        json::Value(canonicalGameState(state));
    authority["next_pocket_capture_sequence"] =
        json::Value(std::to_string(state.nextPocketCaptureSequence));
    authority["schema"] = json::Value(kReplayAuthorityStateSchema);
    json::Value balls = json::Value::array();
    for (std::size_t index = 0; index < state.balls.size(); ++index) {
        const BallState& ball = state.balls[index];
        json::Value item = json::Value::object();
        item["capture_sequence"] =
            json::Value(std::to_string(ball.pocketInteraction.captureSequence));
        item["index"] = json::Value(static_cast<int>(index));
        item["out_of_play"] = json::Value(ball.outOfPlay);
        item["physical_domain"] =
            json::Value(static_cast<int>(ball.physicalDomain));
        item["pocket_id"] = json::Value(ball.pocketInteraction.pocketId);
        item["pocket_phase"] =
            json::Value(static_cast<int>(ball.pocketInteraction.phase));
        item["trajectory_generation"] =
            json::Value(std::to_string(ball.trajectoryGeneration));
        balls.asArray().push_back(std::move(item));
    }
    authority["balls"] = std::move(balls);
    return sha256Hex(json::stringify(authority));
}

ValidationResult validate(const Document& document,
    const PhysicsProfile& currentProfile,
    const std::string& executableVersion,
    const std::string& contentSha256)
{
    if (document.formatVersion != kReplayFormatVersion &&
        document.formatVersion != kReplayRulesFormatVersion)
        return {false, "replay_format_mismatch"};
    if (document.executableVersion != executableVersion)
        return {false, "replay_executable_mismatch"};
    if (document.contentVersion != 1 || document.contentSha256 != contentSha256 ||
        !validHash(document.contentSha256))
        return {false, "replay_content_hash_mismatch"};
    if (document.physicsProfileId != currentProfile.id)
        return {false, "replay_profile_id_mismatch"};
    if (document.physicsProfileSha256 != physicsProfileSha256(currentProfile))
        return {false, "replay_profile_hash_mismatch"};
    if (!std::is_sorted(document.actions.begin(), document.actions.end(),
            [](const SemanticAction& left, const SemanticAction& right) {
                return left.tick < right.tick;
            }))
        return {false, "replay_actions_not_tick_sorted"};
    if (document.finalSummary.shotCount < 0)
        return {false, "replay_final_shot_count_invalid"};
    for (const SemanticAction& action : document.actions) {
        if (action.tick >= document.finalSummary.tick)
            return {false, "replay_action_outside_timeline"};
        if (!std::isfinite(action.first) || !std::isfinite(action.second))
            return {false, "replay_action_nonfinite"};
        if (action.type == SemanticActionType::SetCalledPocket &&
            (std::floor(action.first) != action.first || action.first < -1.0f ||
                action.first > 5.0f || action.second != 0.0f))
            return {false, "replay_called_pocket_invalid"};
    }
    if (!validHash(document.finalSummary.stateSha256))
        return {false, "replay_final_hash_invalid"};
    const bool hasAuthoritySchema =
        !document.finalSummary.authorityStateSchema.empty();
    const bool hasAuthorityHash =
        !document.finalSummary.authorityStateSha256.empty();
    if (hasAuthoritySchema != hasAuthorityHash ||
        (hasAuthorityHash &&
            !validHash(document.finalSummary.authorityStateSha256)))
        return {false, "replay_authority_hash_invalid"};
    if (hasAuthoritySchema && document.finalSummary.authorityStateSchema !=
            kReplayAuthorityStateSchema)
        return {false, "replay_authority_schema_unknown"};
    if(document.formatVersion==kReplayFormatVersion){
        if(!document.rulesReceiptSchema.empty()||
           !document.rulesReceipts.empty()||
           !document.finalSummary.rulesEffectStateSha256.empty())
            return {false,"replay_rules_extension_requires_v2"};
    }else{
            const bool comprehensive=comprehensiveRulesSchema(
                document.rulesReceiptSchema);
            const bool declarations=declarationRulesSchema(
                document.rulesReceiptSchema);
        if((document.rulesReceiptSchema!=kReplayRulesReceiptSchema&&
            !comprehensive)||
           document.rulesReceipts.empty()||
           !validHash(document.initialRulesEffectSha256)||
           !validHash(document.finalSummary.rulesEffectStateSha256))
            return {false,"replay_rules_receipt_schema_invalid"};
        std::uint64_t lastTick=0,lastSequence=0;
        bool first=true;
        for(const RulesReceipt& receipt:document.rulesReceipts){
            const auto& event=receipt.event;
            if(receipt.sequence==0||receipt.afterSemanticActionOrdinal>
                    document.actions.size()||
               receipt.receiptSha256!=rulesReceiptSha256(receipt)||
               !validHash(event.parentEffectSha256)||
               !validHash(event.committedEffectSha256)||
               event.tick!=receipt.tick||event.sequence!=receipt.sequence||
               event.rackIdentity.empty()||event.decisionId.empty()||
               ((!declarationEvent(event.type)&&
                    event.committedRulesRevision!=
                        event.previousRulesRevision+1)||
                (declarationEvent(event.type)&&
                    event.committedRulesRevision!=
                        event.previousRulesRevision))||
               receipt.tick>=document.finalSummary.tick||
               (!first&&(receipt.tick<lastTick||
                 (receipt.tick==lastTick&&receipt.sequence<=lastSequence))))
                return {false,"replay_rules_receipt_invalid"};
            if(receipt.afterSemanticActionOrdinal>0&&
               document.actions[receipt.afterSemanticActionOrdinal-1].tick>
                    receipt.tick)
                return {false,"replay_rules_receipt_order_invalid"};
            if(receipt.afterSemanticActionOrdinal<document.actions.size()&&
               document.actions[receipt.afterSemanticActionOrdinal].tick<
                    receipt.tick)
                return {false,"replay_rules_receipt_order_invalid"};
            if(declarations&&!declarationEvent(event.type)&&
               (!event.authoritySubjectId.empty()||
                !event.proposalDecisionId.empty()||
                !event.declarationRackIdentity.empty()||
                !event.factSchemaId.empty()||
                event.declarationObjectBallId!=-1||
                event.declarationCushionId!=-1))
                return {false,"replay_rules_receipt_invalid"};
            if(event.type==RuntimeRulesReplayEventType::
                    NonphysicalGeneralFoulCommit){
                if(event.actionId.size()!=0||event.offendingPlayer<0||
                   event.offendingPlayer>1||
                   event.opponentPlayer!=1-event.offendingPlayer||
                   event.actorPlayerId!=-1||event.selectedBallId!=-1||
                   event.worldX!=0.0||
                   event.worldZ!=0.0||
                   (event.foulOutcome!=rules::NonphysicalGeneralFoulOutcome::
                        GeneralFoulBallInHand&&
                    event.foulOutcome!=rules::NonphysicalGeneralFoulOutcome::
                        LossOfRack))
                    return {false,"replay_rules_receipt_invalid"};
            }else if(event.type==RuntimeRulesReplayEventType::
                    BallInHandPlacementCommit){
                if((comprehensive&&event.actionId!=
                        rules::kConfirmFullTablePlacementActionId&&
                        event.actionId!=
                            rules::kConfirmBehindHeadStringPlacementActionId)||
                   event.actionId.empty()||event.actorPlayerId<0||
                   event.actorPlayerId>1||!std::isfinite(event.worldX)||
                   !std::isfinite(event.worldZ)||event.selectedBallId!=-1||
                   event.offendingPlayer!=-1||
                   event.opponentPlayer!=-1||event.foulOutcome!=
                        rules::NonphysicalGeneralFoulOutcome::None)
                    return {false,"replay_rules_receipt_invalid"};
            }else if(comprehensive&&event.type==
                    RuntimeRulesReplayEventType::GroupChoiceCommit){
                if((event.actionId!=rules::kGroupChoiceSolidsActionId&&
                    event.actionId!=rules::kGroupChoiceStripesActionId)||
                   event.actorPlayerId<0||event.actorPlayerId>1||
                   event.selectedBallId!=-1||event.worldX!=0.0||
                   event.worldZ!=0.0||event.offendingPlayer!=-1||
                   event.opponentPlayer!=-1||event.foulOutcome!=
                        rules::NonphysicalGeneralFoulOutcome::None)
                    return {false,"replay_rules_receipt_invalid"};
            }else if(comprehensive&&event.type==
                    RuntimeRulesReplayEventType::BreakFoulChoiceCommit){
                if((event.actionId!=rules::kBreakFoulAcceptTableActionId&&
                    event.actionId!=rules::kBreakFoulBallInHandActionId)||
                   event.actorPlayerId<0||event.actorPlayerId>1||
                   event.selectedBallId!=-1||event.worldX!=0.0||
                   event.worldZ!=0.0||event.offendingPlayer!=-1||
                   event.opponentPlayer!=-1||event.foulOutcome!=
                        rules::NonphysicalGeneralFoulOutcome::None)
                    return {false,"replay_rules_receipt_invalid"};
            }else if(comprehensive&&event.type==
                    RuntimeRulesReplayEventType::EightOnBreakChoiceCommit){
                if((event.actionId!=rules::kEightOnBreakSpotContinueActionId&&
                    event.actionId!=rules::kEightOnBreakSpotBallInHandActionId)||
                   event.actorPlayerId<0||event.actorPlayerId>1||
                   event.selectedBallId!=-1||event.worldX!=0.0||
                   event.worldZ!=0.0||event.offendingPlayer!=-1||
                   event.opponentPlayer!=-1||event.foulOutcome!=
                        rules::NonphysicalGeneralFoulOutcome::None)
                    return {false,"replay_rules_receipt_invalid"};
            }else if(comprehensive&&event.type==
                    RuntimeRulesReplayEventType::BehindHeadStringRespotCommit){
                const bool nearest=event.actionId==
                    rules::kRequestNearestLegalRespotActionId&&
                    event.selectedBallId==-1;
                const bool chosen=event.actionId==
                    rules::kChooseEquidistantRespotBallActionId&&
                    event.selectedBallId>=1&&event.selectedBallId<kBallCount;
                if((!nearest&&!chosen)||event.actorPlayerId<0||
                   event.actorPlayerId>1||event.worldX!=0.0||
                   event.worldZ!=0.0||event.offendingPlayer!=-1||
                   event.opponentPlayer!=-1||event.foulOutcome!=
                        rules::NonphysicalGeneralFoulOutcome::None)
                    return {false,"replay_rules_receipt_invalid"};
            }else if(declarations&&declarationEvent(event.type)){
                const bool miscue=event.type==RuntimeRulesReplayEventType::
                    UnintentionalBreakMiscueDeclarationCommit;
                const bool common=event.authorityRole==
                        rules::RefereeAuthorityRole::Referee&&
                    !event.authoritySubjectId.empty()&&
                    event.authoritySubjectId.size()<=128&&
                    !event.proposalDecisionId.empty()&&
                    event.proposalDecisionId.size()<=128&&
                    !event.declarationRackIdentity.empty()&&
                    event.declarationRackIdentity.size()<=256&&
                    ((miscue&&event.actorPlayerId>=0&&
                        event.actorPlayerId<=1)||
                     (!miscue&&event.actorPlayerId==-1))&&
                    event.selectedBallId==-1&&
                    event.worldX==0.0&&event.worldZ==0.0&&
                    event.offendingPlayer==-1&&event.opponentPlayer==-1&&
                    event.foulOutcome==
                        rules::NonphysicalGeneralFoulOutcome::None;
                bool typed=false;
                if(event.type==RuntimeRulesReplayEventType::
                        PushReviewDeclarationCommit)
                    typed=event.actionId==rules::kPushReviewDeclarationKind&&
                        event.factSchemaId=="canonical_push_classification_v1"&&
                        event.declarationObjectBallId==-1&&
                        event.declarationCushionId==-1;
                else if(event.type==RuntimeRulesReplayEventType::
                        DoubleHitCloseCaseDeclarationCommit)
                    typed=event.actionId==
                            rules::kDoubleHitCloseCaseDeclarationKind&&
                        event.factSchemaId==
                            "canonical_double_hit_classification_v1"&&
                        event.declarationObjectBallId>=1&&
                        event.declarationObjectBallId<kBallCount&&
                        event.declarationCushionId==-1;
                else if(event.type==RuntimeRulesReplayEventType::
                        FrozenBallDeclarationCommit)
                    typed=event.actionId==
                            rules::kFrozenCueObjectDeclarationKind&&
                        event.factSchemaId==
                            "frozen_cue_object_declaration_v1"&&
                        event.declarationObjectBallId>=1&&
                        event.declarationObjectBallId<kBallCount&&
                        event.declarationCushionId==-1;
                else if(event.type==RuntimeRulesReplayEventType::
                        FrozenCushionDeclarationCommit)
                    typed=event.actionId==
                            rules::kFrozenCushionDeclarationKind&&
                        event.factSchemaId==
                            "canonical_frozen_cushion_shot_observation_v1"&&
                        event.declarationObjectBallId>=1&&
                        event.declarationObjectBallId<kBallCount&&
                        event.declarationCushionId>=0&&
                        event.declarationCushionId<=3;
                else if(miscue)
                    typed=miscueDeclarationRulesSchema(
                            document.rulesReceiptSchema)&&
                        event.actionId==rules::
                            kUnintentionalBreakMiscueDeclarationKind&&
                        event.factSchemaId==
                            "wpa_ii_06_g_unintentional_break_miscue_declaration_v1"&&
                        event.declarationObjectBallId==-1&&
                        event.declarationCushionId==-1;
                if(!common||!typed)
                    return {false,"replay_rules_receipt_invalid"};
            }else return {false,"replay_rules_receipt_invalid"};
            first=false;lastTick=receipt.tick;lastSequence=receipt.sequence;
        }
    }
    return {true, ""};
}

json::Value serialize(const Document& document)
{
    json::Value root = json::Value::object();
    root["content_sha256"] = json::Value(document.contentSha256);
    root["content_version"] = json::Value(document.contentVersion);
    root["executable_version"] = json::Value(document.executableVersion);
    root["format"] = json::Value(document.formatVersion==
        kReplayRulesFormatVersion?"billiardgl_replay_v2":
        "billiardgl_replay_v1");
    root["format_version"] = json::Value(document.formatVersion);
    root["physics_profile_id"] = json::Value(document.physicsProfileId);
    root["physics_profile_sha256"] = json::Value(document.physicsProfileSha256);
    root["ruleset_id"] = json::Value(document.rulesMode ==
        RulesMode::ChineseEightBallV1 ? "wpa_heyball_2025_08_16_v1" :
        "legacy");
    root["seed"] = json::Value(static_cast<double>(document.seed));

    json::Value balls = json::Value::array();
    for (int index = 0; index < kBallCount; ++index) {
        const BallState& ball = document.initialState.balls[index];
        json::Value value = json::Value::object();
        value["angular_velocity"] = pointValue(ball.angularVelocity);
        value["index"] = json::Value(index);
        value["pocketed"] = json::Value(ball.pocketed);
        if (ball.outOfPlay) value["out_of_play"] = json::Value(true);
        value["position"] = pointValue(ball.position);
        value["trajectory_generation"] = json::Value(
            canonicalTrajectoryGeneration(ball.trajectoryGeneration));
        value["velocity"] = pointValue(ball.velocity);
        balls.asArray().push_back(value);
    }
    root["initial_scenario"] = balls;

    json::Value actions = json::Value::array();
    for (const SemanticAction& action : document.actions) {
        json::Value value = json::Value::object();
        value["first"] = json::Value(action.first);
        value["second"] = json::Value(action.second);
        value["tick"] = json::Value(static_cast<double>(action.tick));
        value["type"] = json::Value(actionName(action.type));
        actions.asArray().push_back(value);
    }
    root["semantic_actions"] = actions;
    if(document.formatVersion==kReplayRulesFormatVersion){
        root["initial_rules_revision"]=json::Value(
            static_cast<double>(document.initialRulesRevision));
        root["initial_rules_effect_sha256"]=
            json::Value(document.initialRulesEffectSha256);
        root["referee_receipt_schema"]=
            json::Value(document.rulesReceiptSchema);
        json::Value receipts=json::Value::array();
        for(const RulesReceipt& receipt:document.rulesReceipts){
            const auto& event=receipt.event;
            json::Value value=json::Value::object();
            value["tick"]=json::Value(static_cast<double>(receipt.tick));
            value["sequence"]=json::Value(
                static_cast<double>(receipt.sequence));
            value["after_semantic_action_ordinal"]=json::Value(
                static_cast<double>(receipt.afterSemanticActionOrdinal));
            value["type"]=json::Value(rulesEventName(event.type));
            value["rack_identity"]=json::Value(event.rackIdentity);
            value["decision_id"]=json::Value(event.decisionId);
            value["action_id"]=json::Value(event.actionId);
            value["event_kind"]=json::Value(
                rules::nonphysicalGeneralFoulKindName(event.foulKind));
            value["outcome"]=json::Value(outcomeName(event.foulOutcome));
            value["offending_player"]=json::Value(event.offendingPlayer);
            value["opponent_player"]=json::Value(event.opponentPlayer);
            value["actor_player"]=json::Value(event.actorPlayerId);
            if(comprehensiveRulesSchema(document.rulesReceiptSchema))
                value["selected_ball_id"]=json::Value(event.selectedBallId);
            if(declarationRulesSchema(document.rulesReceiptSchema)){
                value["authority_role"]=json::Value(
                    rules::refereeAuthorityRoleName(event.authorityRole));
                value["authority_subject_id"]=json::Value(
                    event.authoritySubjectId);
                value["proposal_decision_id"]=json::Value(
                    event.proposalDecisionId);
                value["declaration_rack_identity"]=json::Value(
                    event.declarationRackIdentity);
                value["fact_schema_id"]=json::Value(event.factSchemaId);
                value["declaration_object_ball_id"]=json::Value(
                    event.declarationObjectBallId);
                value["declaration_cushion_id"]=json::Value(
                    event.declarationCushionId);
            }
            value["world_x"]=json::Value(event.worldX);
            value["world_z"]=json::Value(event.worldZ);
            value["previous_rules_revision"]=json::Value(
                static_cast<double>(event.previousRulesRevision));
            value["committed_rules_revision"]=json::Value(
                static_cast<double>(event.committedRulesRevision));
            value["parent_effect_sha256"]=
                json::Value(event.parentEffectSha256);
            value["committed_effect_sha256"]=
                json::Value(event.committedEffectSha256);
            value["receipt_sha256"]=json::Value(receipt.receiptSha256);
            receipts.asArray().push_back(std::move(value));
        }
        root["referee_receipts"]=std::move(receipts);
    }
    json::Value final = json::Value::object();
    final["shot_count"] = json::Value(document.finalSummary.shotCount);
    final["state_sha256"] = json::Value(document.finalSummary.stateSha256);
    if (!document.finalSummary.authorityStateSchema.empty()) {
        final["authority_state_schema"] =
            json::Value(document.finalSummary.authorityStateSchema);
        final["authority_state_sha256"] =
            json::Value(document.finalSummary.authorityStateSha256);
    }
    if(document.formatVersion==kReplayRulesFormatVersion)
        final["rules_effect_state_sha256"]=
            json::Value(document.finalSummary.rulesEffectStateSha256);
    final["tick"] = json::Value(static_cast<double>(document.finalSummary.tick));
    root["final_summary"] = final;
    return root;
}

ValidationResult parse(const json::Value& value, Document& document)
{
    try {
        if (!value.isObject())
            return {false, "replay_format_mismatch"};
        Document parsed;
        parsed.formatVersion = value.at("format_version").asInt();
        const std::string format=value.at("format").asString();
        if((parsed.formatVersion==kReplayFormatVersion&&
                format!="billiardgl_replay_v1")||
           (parsed.formatVersion==kReplayRulesFormatVersion&&
                format!="billiardgl_replay_v2")||
           (parsed.formatVersion!=kReplayFormatVersion&&
                parsed.formatVersion!=kReplayRulesFormatVersion))
            return {false,"replay_format_mismatch"};
        parsed.executableVersion = value.at("executable_version").asString();
        parsed.contentVersion = value.at("content_version").asInt();
        parsed.contentSha256 = value.at("content_sha256").asString();
        parsed.physicsProfileId = value.at("physics_profile_id").asString();
        parsed.physicsProfileSha256 = value.at("physics_profile_sha256").asString();
        if (value.has("ruleset_id")) {
            const std::string ruleset = value.at("ruleset_id").asString();
            if (ruleset == "legacy") parsed.rulesMode = RulesMode::Legacy;
            else if (ruleset == "wpa_heyball_2025_08_16_v1")
                parsed.rulesMode = RulesMode::ChineseEightBallV1;
            else return {false, "replay_ruleset_unknown"};
        }
        parsed.seed = static_cast<std::uint32_t>(exactUnsigned(value.at("seed"),
            std::numeric_limits<std::uint32_t>::max(), "seed"));
        initializeBalls(parsed.initialState);
        const json::Value::Array& balls = value.at("initial_scenario").asArray();
        if (balls.size() != kBallCount) return {false, "replay_initial_scenario_invalid"};
        bool seen[kBallCount] = {};
        for (const json::Value& item : balls) {
            const int index = item.at("index").asInt();
            if (index < 0 || index >= kBallCount || seen[index])
                return {false, "replay_initial_scenario_invalid"};
            seen[index] = true;
            BallState& ball = parsed.initialState.balls[index];
            ball.position = parsePoint(item.at("position"));
            ball.velocity = parsePoint(item.at("velocity"));
            ball.angularVelocity = parsePoint(item.at("angular_velocity"));
            ball.pocketed = item.at("pocketed").asBool();
            ball.outOfPlay = false;
            if (item.has("out_of_play")) {
                if (!item.at("out_of_play").isBool())
                    return {false, "replay_out_of_play_invalid"};
                ball.outOfPlay = item.at("out_of_play").asBool();
                if (ball.outOfPlay && ball.pocketed)
                    return {false, "replay_out_of_play_invalid"};
            }
            ball.trajectoryGeneration = 0;
            if (item.has("trajectory_generation") &&
                (!item.at("trajectory_generation").isString() ||
                    !parseCanonicalTrajectoryGeneration(
                        item.at("trajectory_generation").asString(),
                        ball.trajectoryGeneration)))
                return {false, "replay_trajectory_generation_invalid"};
            ball.speed = std::hypot(ball.velocity.x, ball.velocity.z);
        }
        for (const json::Value& item : value.at("semantic_actions").asArray()) {
            SemanticAction action;
            action.tick = exactUnsigned(item.at("tick"), 9007199254740991ull,
                "action.tick");
            if (!parseActionName(item.at("type").asString(), action.type))
                return {false, "replay_action_unknown"};
            const double first = item.at("first").asNumber();
            const double second = item.at("second").asNumber();
            if (!std::isfinite(first) || !std::isfinite(second) ||
                std::fabs(first) > std::numeric_limits<float>::max() ||
                std::fabs(second) > std::numeric_limits<float>::max())
                return {false, "replay_action_nonfinite"};
            action.first = static_cast<float>(first);
            action.second = static_cast<float>(second);
            parsed.actions.push_back(action);
        }
        if(parsed.formatVersion==kReplayRulesFormatVersion){
            parsed.initialRulesRevision=exactUnsigned(
                value.at("initial_rules_revision"),9007199254740991ull,
                "initial_rules_revision");
            parsed.initialRulesEffectSha256=
                value.at("initial_rules_effect_sha256").asString();
            parsed.rulesReceiptSchema=
                value.at("referee_receipt_schema").asString();
            const bool comprehensive=comprehensiveRulesSchema(
                parsed.rulesReceiptSchema);
            const bool declarations=declarationRulesSchema(
                parsed.rulesReceiptSchema);
            for(const json::Value& item:
                    value.at("referee_receipts").asArray()){
                if(!item.isObject()||item.asObject().size()!=
                        (declarations?27u:comprehensive?20u:19u))
                    return {false,"replay_rules_receipt_invalid"};
                RulesReceipt receipt;
                receipt.tick=exactUnsigned(item.at("tick"),
                    9007199254740991ull,"receipt.tick");
                receipt.sequence=exactUnsigned(item.at("sequence"),
                    9007199254740991ull,"receipt.sequence");
                receipt.afterSemanticActionOrdinal=static_cast<std::size_t>(
                    exactUnsigned(item.at("after_semantic_action_ordinal"),
                        9007199254740991ull,"receipt.action_ordinal"));
                auto& event=receipt.event;
                event.tick=receipt.tick;event.sequence=receipt.sequence;
                if(!parseRulesEventName(item.at("type").asString(),event.type)||
                   !parseFoulKind(item.at("event_kind").asString(),
                        event.foulKind)||
                   !parseOutcome(item.at("outcome").asString(),
                        event.foulOutcome))
                    return {false,"replay_rules_receipt_invalid"};
                event.rackIdentity=item.at("rack_identity").asString();
                event.decisionId=item.at("decision_id").asString();
                event.actionId=item.at("action_id").asString();
                event.offendingPlayer=item.at("offending_player").asInt();
                event.opponentPlayer=item.at("opponent_player").asInt();
                event.actorPlayerId=item.at("actor_player").asInt();
                if(comprehensive)
                    event.selectedBallId=
                        item.at("selected_ball_id").asInt();
                if(declarations){
                    if(item.at("authority_role").asString()!="REFEREE")
                        return {false,"replay_rules_receipt_invalid"};
                    event.authorityRole=rules::RefereeAuthorityRole::Referee;
                    event.authoritySubjectId=
                        item.at("authority_subject_id").asString();
                    event.proposalDecisionId=
                        item.at("proposal_decision_id").asString();
                    event.declarationRackIdentity=
                        item.at("declaration_rack_identity").asString();
                    event.factSchemaId=item.at("fact_schema_id").asString();
                    event.declarationObjectBallId=
                        item.at("declaration_object_ball_id").asInt();
                    event.declarationCushionId=
                        item.at("declaration_cushion_id").asInt();
                }
                event.worldX=item.at("world_x").asNumber();
                event.worldZ=item.at("world_z").asNumber();
                event.previousRulesRevision=exactUnsigned(
                    item.at("previous_rules_revision"),
                    9007199254740991ull,"receipt.previous_revision");
                event.committedRulesRevision=exactUnsigned(
                    item.at("committed_rules_revision"),
                    9007199254740991ull,"receipt.committed_revision");
                event.parentEffectSha256=
                    item.at("parent_effect_sha256").asString();
                event.committedEffectSha256=
                    item.at("committed_effect_sha256").asString();
                receipt.receiptSha256=item.at("receipt_sha256").asString();
                parsed.rulesReceipts.push_back(std::move(receipt));
            }
        }
        const json::Value& final = value.at("final_summary");
        parsed.finalSummary.tick = exactUnsigned(final.at("tick"),
            9007199254740991ull, "final_summary.tick");
        parsed.finalSummary.shotCount = final.at("shot_count").asInt();
        if (parsed.finalSummary.shotCount < 0)
            return {false, "replay_final_shot_count_invalid"};
        parsed.finalSummary.stateSha256 = final.at("state_sha256").asString();
        const bool hasAuthoritySchema = final.has("authority_state_schema");
        const bool hasAuthorityHash = final.has("authority_state_sha256");
        if (hasAuthoritySchema != hasAuthorityHash)
            return {false, "replay_authority_hash_invalid"};
        if (hasAuthoritySchema) {
            if (!final.at("authority_state_schema").isString() ||
                !final.at("authority_state_sha256").isString())
                return {false, "replay_authority_hash_invalid"};
            parsed.finalSummary.authorityStateSchema =
                final.at("authority_state_schema").asString();
            parsed.finalSummary.authorityStateSha256 =
                final.at("authority_state_sha256").asString();
            if (parsed.finalSummary.authorityStateSchema.empty() ||
                parsed.finalSummary.authorityStateSha256.empty())
                return {false, "replay_authority_hash_invalid"};
        }
        if(parsed.formatVersion==kReplayRulesFormatVersion)
            parsed.finalSummary.rulesEffectStateSha256=
                final.at("rules_effect_state_sha256").asString();
        document = parsed;
        return {true, ""};
    } catch (const std::exception&) {
        return {false, "replay_format_invalid"};
    }
}

ValidationResult Player::load(const Document& document,
    const PhysicsProfile& currentProfile,
    const std::string& executableVersion,
    const std::string& contentSha256)
{
    const ValidationResult checked = validate(document, currentProfile,
        executableVersion, contentSha256);
    if (!checked.ok) return checked;
    GameRuntime candidateRuntime;
    const ActionResult reset = candidateRuntime.replaceStateForScenario(
        document.initialState, currentProfile);
    if (!reset.ok) return {false, reset.errorCode};
    const ActionResult rules = candidateRuntime.setRulesMode(document.rulesMode);
    if (!rules.ok) return {false, rules.errorCode};
    if(document.formatVersion==kReplayRulesFormatVersion){
        const GameRuntime::ReplayRulesImportToken token;
        const ActionResult baseline=candidateRuntime.initializeRulesReplayBaseline(
            document.initialRulesRevision,
            document.initialRulesEffectSha256,
            effectVersionFor(document.rulesReceiptSchema),token);
        if(!baseline.ok)return {false,baseline.errorCode};
    }
    if (document.finalSummary.tick == 0 &&
        (gameStateSha256(candidateRuntime.state()) !=
                document.finalSummary.stateSha256 ||
            document.finalSummary.shotCount != 0 ||
            (!document.finalSummary.authorityStateSchema.empty() &&
                replayAuthorityStateSha256(candidateRuntime.state()) !=
                    document.finalSummary.authorityStateSha256)))
        return {false, "replay_final_hash_mismatch"};
    document_ = document;
    profile_ = currentProfile;
    runtime_ = std::move(candidateRuntime);
    nextAction_ = 0;
    nextRulesReceipt_ = 0;
    shotCount_ = 0;
    loaded_ = true;
    paused_ = true;
    playbackRate_ = 1.0;
    schedulingAccumulator_ = 0.0;
    return {true, ""};
}

ValidationResult Player::resetToInitial()
{
    if (!loaded_) return {false, "replay_not_loaded"};
    // Reconstruct the same clean authority used by load(). Reusing a runtime
    // that has completed a rack can retain derived decision/shot identities
    // even when the visible balls and initial effect digest are restored.
    GameRuntime candidateRuntime;
    const ActionResult reset = candidateRuntime.replaceStateForScenario(
        document_.initialState, profile_);
    if (!reset.ok) return {false, reset.errorCode};
    const ActionResult rules = candidateRuntime.setRulesMode(document_.rulesMode);
    if (!rules.ok) return {false, rules.errorCode};
    if(document_.formatVersion==kReplayRulesFormatVersion){
        const GameRuntime::ReplayRulesImportToken token;
        const ActionResult baseline=candidateRuntime.initializeRulesReplayBaseline(
            document_.initialRulesRevision,
            document_.initialRulesEffectSha256,
            effectVersionFor(document_.rulesReceiptSchema),token);
        if(!baseline.ok)return {false,baseline.errorCode};
    }
    runtime_ = std::move(candidateRuntime);
    nextAction_ = 0;
    nextRulesReceipt_ = 0;
    shotCount_ = 0;
    schedulingAccumulator_ = 0.0;
    return {true, ""};
}

ValidationResult Player::setPlaybackRate(double rate)
{
    if (rate != 0.25 && rate != 0.5 && rate != 1.0 && rate != 2.0)
        return {false, "replay_speed_unsupported"};
    playbackRate_ = rate;
    return {true, ""};
}

ValidationResult Player::applyAction(const SemanticAction& action)
{
    ActionResult result;
    if (action.type == SemanticActionType::SetAimYaw) {
        GameAction gameAction;
        gameAction.type = ActionType::SetAimYaw;
        gameAction.first = action.first;
        result = runtime_.dispatch(gameAction);
    } else if (action.type == SemanticActionType::SetShotPower) {
        GameAction gameAction;
        gameAction.type = ActionType::SetShotPower;
        gameAction.first = action.first;
        result = runtime_.dispatch(gameAction);
    } else if (action.type == SemanticActionType::SetTipOffset) {
        result = runtime_.setTipOffset(action.first, action.second);
    } else if (action.type == SemanticActionType::SetCalledPocket) {
        result = runtime_.setCalledPocket(static_cast<int>(action.first));
    } else {
        GameAction gameAction;
        gameAction.type = ActionType::Shoot;
        result = runtime_.dispatch(gameAction);
        if (result.ok) ++shotCount_;
    }
    return result.ok ? ValidationResult{true, ""} :
        ValidationResult{false, result.errorCode};
}

ValidationResult Player::applyActionsAtCurrentTick(std::size_t stopBefore)
{
    return applyTimelineAtCurrentTick(stopBefore);
}

ValidationResult Player::applyTimelineAtCurrentTick(std::size_t stopBefore)
{
    for(;;){
        const bool receiptAtTick=nextRulesReceipt_<
                document_.rulesReceipts.size()&&
            document_.rulesReceipts[nextRulesReceipt_].tick==runtime_.tick();
        if(receiptAtTick&&document_.rulesReceipts[nextRulesReceipt_]
                .afterSemanticActionOrdinal==nextAction_){
            const GameRuntime::ReplayRulesImportToken token;
            const ActionResult imported=runtime_.importRulesReplayEvent(
                document_.rulesReceipts[nextRulesReceipt_].event,
                effectVersionFor(document_.rulesReceiptSchema),token);
            if(!imported.ok)return {false,imported.errorCode};
            ++nextRulesReceipt_;continue;
        }
        const bool actionAtTick=nextAction_<document_.actions.size()&&
            document_.actions[nextAction_].tick==runtime_.tick()&&
            nextAction_!=stopBefore;
        if(actionAtTick){
            const ValidationResult result=applyAction(
                document_.actions[nextAction_]);
            if(!result.ok)return result;
            ++nextAction_;continue;
        }
        if(receiptAtTick&&document_.rulesReceipts[nextRulesReceipt_]
                .afterSemanticActionOrdinal<nextAction_)
            return {false,"replay_rules_receipt_order_invalid"};
        break;
    }
    return {true,""};
}

ValidationResult Player::stepTick()
{
    if (!loaded_) return {false, "replay_not_loaded"};
    if (runtime_.tick() >= document_.finalSummary.tick)
        return finalHashMatches() ?
            ValidationResult{false, "replay_finished"} :
            ValidationResult{false, "replay_final_hash_mismatch"};
    const ValidationResult actions = applyActionsAtCurrentTick();
    if (!actions.ok) return actions;
    const ActionResult stepped = runtime_.step(1);
    if (!stepped.ok) return {false, stepped.errorCode};
    if (finished() && !finalHashMatches())
        return {false, "replay_final_hash_mismatch"};
    return {true, ""};
}

ValidationResult Player::advanceScheduledFrame()
{
    if (!loaded_) return {false, "replay_not_loaded"};
    if (finished() && !finalHashMatches())
        return {false, "replay_final_hash_mismatch"};
    if (paused_) return {true, ""};
    schedulingAccumulator_ += playbackRate_;
    while (schedulingAccumulator_ >= 1.0 && !finished()) {
        const ValidationResult stepped = stepTick();
        if (!stepped.ok) return stepped;
        schedulingAccumulator_ -= 1.0;
    }
    return {true, ""};
}

ValidationResult Player::seekShot(std::size_t shotOrdinal)
{
    if (!loaded_) return {false, "replay_not_loaded"};
    std::size_t ordinal = 0;
    std::size_t target = document_.actions.size();
    for (std::size_t index = 0; index < document_.actions.size(); ++index) {
        if (document_.actions[index].type != SemanticActionType::Shoot) continue;
        if (ordinal++ == shotOrdinal) { target = index; break; }
    }
    if (target == document_.actions.size()) return {false, "replay_shot_not_found"};
    ValidationResult reset = resetToInitial();
    if (!reset.ok) return reset;
    while (runtime_.tick() < document_.actions[target].tick) {
        const ValidationResult stepped = stepTick();
        if (!stepped.ok) return stepped;
    }
    return applyActionsAtCurrentTick(target);
}

bool Player::finished() const
{
    return loaded_ && runtime_.tick() >= document_.finalSummary.tick;
}

std::string Player::finalStateSha256() const
{
    return loaded_ ? gameStateSha256(runtime_.state()) : std::string();
}

bool Player::finalHashMatches() const
{
    if (!finished() || finalStateSha256() !=
            document_.finalSummary.stateSha256 ||
        shotCount_ != document_.finalSummary.shotCount)
        return false;
    if (!document_.finalSummary.authorityStateSchema.empty() &&
        replayAuthorityStateSha256(runtime_.state()) !=
            document_.finalSummary.authorityStateSha256)
        return false;
    return document_.formatVersion != kReplayRulesFormatVersion ||
        runtime_.rulesReplayEffectStateHash(
            effectVersionFor(document_.rulesReceiptSchema)) ==
            document_.finalSummary.rulesEffectStateSha256;
}

}  // namespace billiardgl::replay
