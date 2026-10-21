#include "automation_controller.h"
#include "automation_protocol.h"
#include "deterministic_hash.h"
#include "physics_scenario.h"
#include "render/modern/camera_projection.h"
#include "table_specs.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <cstdlib>
#include <fstream>
#include <iostream>
#include <limits>
#include <sstream>

namespace {
void expect(bool value, const char* message) { if (!value) { std::cerr << message << '\n'; std::exit(1); } }

class StalemateAutomationAuthenticator final :
    public billiardgl::rules::RefereeAuthorityAuthenticator {
public:
    bool verify(const billiardgl::rules::RefereeAuthenticationInput& input)
        const override
    {
        return input.authenticationProof == billiardgl::sha256Hex(
            "automation-secret|" + input.authenticatedBytes);
    }
};

billiardgl::json::Value authorityParams(
    const billiardgl::rules::RefereeAuthorityRequest& request)
{
    using billiardgl::json::Value;
    Value value = Value::object();
    value["protocol_id"] = Value(request.protocolId);
    value["schema_version"] = Value(request.schemaVersion);
    value["operation"] = Value(
        billiardgl::rules::refereeAuthorityOperationName(request.operation));
    value["declaration_kind"] = Value(request.declarationKind);
    value["decision_id"] = Value(request.decisionId);
    value["rack_identity"] = Value(request.rackIdentity);
    value["authority_revision"] = Value(
        static_cast<double>(request.expectedAuthorityRevision));
    value["authority_state_hash"] = Value(
        request.expectedAuthorityStateHash);
    value["authority_role"] = Value(
        billiardgl::rules::refereeAuthorityRoleName(request.authorityRole));
    value["authority_subject_id"] = Value(request.authoritySubjectId);
    value["credential_id"] = Value(request.credentialId);
    value["key_version"] = Value(request.keyVersion);
    value["authentication_nonce"] = Value(request.authenticationNonce);
    value["expires_at_authority_tick"] = Value(
        static_cast<double>(request.expiresAtAuthorityTick));
    value["idempotency_key"] = Value(request.idempotencyKey);
    value["payload"] = billiardgl::json::parse(
        request.payloadCanonical).value;
    value["authentication_proof"] = Value(request.authenticationProof);
    return value;
}

billiardgl::rules::RefereeAuthorityRequest nonphysicalRequest(
    const billiardgl::GameRuntime& runtime,
    billiardgl::rules::RefereeAuthorityOperation operation,
    const std::string& decisionId,const std::string& key,
    const std::string& payload)
{
    using namespace billiardgl::rules;
    RefereeAuthorityRequest request;
    request.protocolId=kRefereeAuthorityProtocolId;
    request.schemaVersion=1;
    request.operation=operation;
    request.declarationKind=kNonphysicalGeneralFoulKind;
    request.decisionId=decisionId;
    request.rackIdentity=runtime.nonphysicalGeneralFoulState().rackIdentity;
    request.expectedAuthorityRevision=
        runtime.nonphysicalGeneralFoulProtocolState().authorityRevision;
    request.expectedAuthorityStateHash=
        runtime.nonphysicalGeneralFoulProtocolState().authorityStateHash;
    request.authorityRole=RefereeAuthorityRole::Referee;
    request.authoritySubjectId="automation.referee";
    request.credentialId="automation.credential";
    request.keyVersion="v1";
    request.authenticationNonce=key+".nonce";
    request.expiresAtAuthorityTick=10000;
    request.idempotencyKey=key;
    request.payloadCanonical=payload;
    request.authenticationProof=billiardgl::sha256Hex(
        "automation-secret|"+canonicalRefereeAuthenticatedBytes(request));
    return request;
}

billiardgl::rules::RefereeAuthorityRequest unintentionalBreakMiscueRequest(
    const billiardgl::GameRuntime& runtime,
    billiardgl::rules::RefereeAuthorityOperation operation,
    const std::string& key, const std::string& payload)
{
    using namespace billiardgl::rules;
    RefereeAuthorityRequest request;
    request.protocolId=kRefereeAuthorityProtocolId;
    request.schemaVersion=1;
    request.operation=operation;
    request.declarationKind=kUnintentionalBreakMiscueDeclarationKind;
    request.decisionId="automation.unintentional_break_miscue.1";
    request.rackIdentity=
        runtime.unintentionalBreakMiscueDeclarationState().rackIdentity;
    request.expectedAuthorityRevision=
        runtime.unintentionalBreakMiscueDeclarationProtocolState().
            authorityRevision;
    request.expectedAuthorityStateHash=
        runtime.unintentionalBreakMiscueDeclarationProtocolState().
            authorityStateHash;
    request.authorityRole=RefereeAuthorityRole::Referee;
    request.authoritySubjectId="automation.referee.06g";
    request.credentialId="automation.credential";
    request.keyVersion="v1";
    request.authenticationNonce=key+".nonce";
    request.expiresAtAuthorityTick=10000;
    request.idempotencyKey=key;
    request.payloadCanonical=payload;
    request.authenticationProof=billiardgl::sha256Hex(
        "automation-secret|"+canonicalRefereeAuthenticatedBytes(request));
    return request;
}

billiardgl::ControllerResult send(billiardgl::AutomationController& controller, int id,
    const std::string& command, const billiardgl::json::Value& params = billiardgl::json::Value::object())
{
    billiardgl::AutomationRequest request;
    request.id = id; request.version = 1; request.command = command; request.params = params;
    return controller.handle(request);
}

billiardgl::json::Value fixture(const char* name)
{
    const std::string path = std::string(BILLIARDGL_SOURCE_ROOT) +
        "/tests/physics_validation/scenarios/" + name;
    std::ifstream input(path.c_str());
    std::ostringstream contents;
    contents << input.rdbuf();
    const billiardgl::json::ParseResult parsed = billiardgl::json::parse(contents.str());
    expect(parsed.ok, "canonical fixture should parse as JSON");
    return parsed.value;
}

billiardgl::CameraPresentation cameraPresentation(
    const billiardgl::camera::Frame& frame)
{
    billiardgl::CameraPresentation camera;
    camera.eye = {frame.pose.eye.x, frame.pose.eye.y, frame.pose.eye.z};
    camera.target = {
        frame.pose.target.x, frame.pose.target.y, frame.pose.target.z};
    camera.zoom = frame.pose.distanceCm;
    camera.fieldOfViewDegrees = frame.pose.fieldOfViewDegrees;
    return camera;
}

bool projectsInto(const billiardgl::render::SharedCameraProjection& projection,
                  const billiardgl::Point3& world,
                  double x0, double y0, double x1, double y1)
{
    const auto clip = billiardgl::render::transformWorldToClip(
        projection, world);
    billiardgl::render::PhysicalScreenPoint screen;
    return clip.w > 0.0 && billiardgl::render::clipToPhysicalScreen(
        projection, clip, screen) &&
        screen.x >= x0 * projection.physicalWidth &&
        screen.x <= x1 * projection.physicalWidth &&
        screen.y >= y0 * projection.physicalHeight &&
        screen.y <= y1 * projection.physicalHeight;
}

billiardgl::json::Value legacyScenarioBalls(
    bool includeGeneration = false,
    const billiardgl::json::Value& generation = billiardgl::json::Value())
{
    billiardgl::json::Value balls = billiardgl::json::Value::array();
    for (int index = 0; index < billiardgl::kBallCount; ++index) {
        billiardgl::json::Value point = billiardgl::json::Value::object();
        point["x"] = billiardgl::json::Value(static_cast<double>(index));
        point["y"] = billiardgl::json::Value(
            billiardgl::kTableHeight + billiardgl::kBallRadius);
        point["z"] = billiardgl::json::Value(0.0);
        billiardgl::json::Value zero = billiardgl::json::Value::object();
        zero["x"] = billiardgl::json::Value(0.0);
        zero["y"] = billiardgl::json::Value(0.0);
        zero["z"] = billiardgl::json::Value(0.0);
        billiardgl::json::Value ball = billiardgl::json::Value::object();
        ball["position"] = point;
        ball["velocity"] = zero;
        ball["pocketed"] = billiardgl::json::Value(false);
        if (includeGeneration && index == 0)
            ball["trajectory_generation"] = generation;
        balls.asArray().push_back(ball);
    }
    return balls;
}
}

int main()
{
    billiardgl::GameRuntime runtime;
    billiardgl::AutomationController controller(runtime, billiardgl::AutomationMode::Headless);
    expect(send(controller, 1, "ping").response.at("ok").asBool(), "ping should succeed");
    const auto initialCapabilities=send(controller,2,"get_capabilities")
        .response.at("result").at("commands").asArray();
    bool hasReplayV1=false,hasReplayV2RulesReceipts=false;
    for(const auto& capability:initialCapabilities){
        hasReplayV1|=capability.asString()=="replay_v1";
        hasReplayV2RulesReceipts|=
            capability.asString()=="replay_v2_rules_receipts";
    }
    expect(initialCapabilities.size()>10&&hasReplayV1&&
        hasReplayV2RulesReceipts,
        "capabilities preserve replay v1 and advertise v2 rules receipts");
    const auto emptyReplayState=send(controller,2009,"replay_get_state")
        .response.at("result");
    expect(emptyReplayState.at("document_format_version").asInt()==0&&
        emptyReplayState.at("rules_receipt_count").asInt()==0&&
        !emptyReplayState.at("rules_effect_verified").asBool()&&
        !emptyReplayState.at("rack_terminal").at("active").asBool()&&
        emptyReplayState.at("rack_terminal").at("reason").asString()=="NONE",
        "automation replay state distinguishes unloaded and rack-terminal state");
    billiardgl::GameRuntime recordingStateRuntime;
    billiardgl::AutomationController recordingStateController(
        recordingStateRuntime,billiardgl::AutomationMode::Headless);
    billiardgl::json::Value recordingBegin=billiardgl::json::Value::object();
    recordingBegin["schema_version"]=billiardgl::json::Value(1);
    recordingBegin["executable_version"]=billiardgl::json::Value("automation-test");
    recordingBegin["content_sha256"]=billiardgl::json::Value(
        billiardgl::sha256Hex("automation-replay-content"));
    recordingBegin["seed"]=billiardgl::json::Value(7);
    const auto recordingState=send(recordingStateController,2010,
        "replay_record_begin",recordingBegin).response.at("result");
    expect(recordingState.at("document_format_version").asInt()==1&&
        recordingState.at("rules_receipt_count").asInt()==0,
        "automation recording state reports the actual replay document version");
    {
        using namespace billiardgl;
        using namespace billiardgl::rules;
        GameRuntime v2Source;
        StalemateAutomationAuthenticator replayAuthenticator;
        expect(v2Source.setRulesMode(RulesMode::ChineseEightBallV1).ok,
            "automation v2 fixture enables Chinese rules");
        v2Source.configureNonphysicalGeneralFoulAuthority(&replayAuthenticator);
        replay::Recorder recorder;
        const std::string replayExecutable="automation-replay-v2";
        const std::string replayContent=sha256Hex("automation-replay-v2-content");
        expect(recorder.begin(v2Source,replayExecutable,replayContent,18).ok,
            "automation v2 fixture begins recording");
        const std::string firstShot="runtime.nonphysical_general_foul.shot."+
            std::to_string(v2Source.rulesAuthorityRevision());
        auto firstProposal=nonphysicalRequest(v2Source,
            RefereeAuthorityOperation::Propose,"automation.replay.first",
            "automation.replay.first.p",canonicalNonphysicalGeneralFoulProposal(
                NonphysicalGeneralFoulKind::MobileDeviceDisturbanceGeneralFoul,
                0,firstShot));
        const auto firstProposed=
            v2Source.submitNonphysicalGeneralFoulProposal(firstProposal);
        if(!firstProposed.ok)throw std::runtime_error(
            "automation v2 fixture opens first foul: "+firstProposed.errorCode);
        auto firstFinal=nonphysicalRequest(v2Source,
            RefereeAuthorityOperation::FinalDeclare,"automation.replay.first",
            "automation.replay.first.f",canonicalNonphysicalGeneralFoulFinal(
                NonphysicalGeneralFoulKind::MobileDeviceDisturbanceGeneralFoul,
                0,"automation.replay.first",firstShot));
        expect(v2Source.submitNonphysicalGeneralFoulFinal(firstFinal).ok&&
            v2Source.ballInHandPlacementOffer(),
            "automation v2 fixture commits first foul");
        const auto placementOffer=*v2Source.ballInHandPlacementOffer();
        BallInHandPlacementRequest placement;
        placement.rulesetId=kBallInHandRulesetId;
        placement.capabilityId=kFreeBallPlacementCapabilityId;
        placement.flowId=kFullTableBallInHandFlowId;
        placement.decisionId=placementOffer.decisionId;
        placement.expectedAuthorityStateRevision=
            placementOffer.authorityStateRevision;
        placement.idempotencyKey="automation.replay.placement";
        placement.actorPlayerId=placementOffer.actorPlayerId;
        placement.actionId=kConfirmFullTablePlacementActionId;
        placement.position={v2Source.state().balls[0].position.x,
            v2Source.state().balls[0].position.z};
        expect(v2Source.submitBallInHandPlacement(placement).ok,
            "automation v2 fixture confirms placement");
        const std::string repeatShot="runtime.nonphysical_general_foul.shot."+
            std::to_string(v2Source.rulesAuthorityRevision());
        auto repeatProposal=nonphysicalRequest(v2Source,
            RefereeAuthorityOperation::Propose,"automation.replay.repeat",
            "automation.replay.repeat.p",canonicalNonphysicalGeneralFoulProposal(
                NonphysicalGeneralFoulKind::MobileDeviceRepeatLossOfRack,
                0,repeatShot));
        expect(v2Source.submitNonphysicalGeneralFoulProposal(repeatProposal).ok,
            "automation v2 fixture opens repeat foul");
        auto repeatFinal=nonphysicalRequest(v2Source,
            RefereeAuthorityOperation::FinalDeclare,"automation.replay.repeat",
            "automation.replay.repeat.f",canonicalNonphysicalGeneralFoulFinal(
                NonphysicalGeneralFoulKind::MobileDeviceRepeatLossOfRack,
                0,"automation.replay.repeat",repeatShot));
        expect(v2Source.submitNonphysicalGeneralFoulFinal(repeatFinal).ok&&
            v2Source.step(1).ok&&recorder.finish(v2Source,0).ok,
            "automation v2 fixture seals repeat rack loss");
        GameRuntime v2PlaybackAuthority;
        AutomationController v2Playback(v2PlaybackAuthority,
            AutomationMode::Headless);
        json::Value load=json::Value::object();
        load["schema_version"]=json::Value(1);
        load["replay"]=replay::serialize(recorder.document());
        load["executable_version"]=json::Value(replayExecutable);
        load["content_sha256"]=json::Value(replayContent);
        const auto loaded=send(v2Playback,2011,"replay_load",load)
            .response.at("result");
        expect(loaded.at("document_format_version").asInt()==2&&
            loaded.at("rules_receipt_count").asInt()==3&&
            !loaded.at("rules_effect_verified").asBool()&&
            !loaded.at("rack_terminal").at("active").asBool(),
            "automation reports loaded v2 without premature verification");
        const auto terminal=send(v2Playback,2012,"replay_step_tick")
            .response.at("result");
        expect(terminal.at("finished").asBool()&&
            terminal.at("rules_effect_verified").asBool()&&
            terminal.at("rack_terminal").at("active").asBool()&&
            terminal.at("rack_terminal").at("reason").asString()==
                "MOBILE_DEVICE_REPEAT_VIOLATION"&&
            terminal.at("rack_terminal").at("losing_player").asInt()==0&&
            terminal.at("rack_terminal").at("winning_player").asInt()==1,
            "automation exposes verified rack loss without match completion");
    }
    const auto refereeReadback = send(
        controller, 2010, "get_referee_authority_state");
    expect(refereeReadback.response.at("ok").asBool() &&
        refereeReadback.response.at("result").at("protocol_id").asString() ==
            "authenticated_referee_authority_v1" &&
        refereeReadback.response.at("result").at("authority_revision").asInt() == 0 &&
        !refereeReadback.response.at("result").at("proposal_pending").asBool() &&
        !refereeReadback.response.at("result").at("final_declaration_committed").asBool() &&
        !refereeReadback.response.at("result").has("credential_id") &&
        !refereeReadback.response.at("result").has("authentication_proof"),
        "AUTOMATION_READBACK_OMITS_SECRET_AND_PROOF");
    const auto stalemateReadback = send(
        controller, 2011, "get_stalemate_authority_state");
    expect(stalemateReadback.response.at("ok").asBool() &&
        stalemateReadback.response.at("result").at("phase").asString() ==
            "INACTIVE" &&
        !stalemateReadback.response.at("result").has("credential_id") &&
        !stalemateReadback.response.at("result").has("authentication_proof") &&
        !stalemateReadback.response.at("result").has("payload_canonical") &&
        !stalemateReadback.response.at("result")
            .at("credential_material_exposed").asBool(),
        "AUTOMATION_SECRET_FREE_STALEMATE_STATE_ROUNDTRIP");
    const std::vector<std::string> capabilities = controller.capabilities();
    expect(std::find(capabilities.begin(), capabilities.end(),
        "physics_scenario_v2_cue_input") != capabilities.end(),
        "capabilities should advertise cue-input schema v2");
    expect(std::find(capabilities.begin(), capabilities.end(),
        "training_get_overlay_state") != capabilities.end(),
        "capabilities should advertise the presentation-only overlay state");
    expect(std::find(capabilities.begin(), capabilities.end(),
        "collect_render_performance_v1") == capabilities.end(),
        "headless automation must not advertise render performance");
    expect(std::find(capabilities.begin(), capabilities.end(),
            "wpa_ii_21_stalemate_referee_consumer_v1") !=
            capabilities.end() &&
        std::find(capabilities.begin(), capabilities.end(),
            "rules.stalemate_referee_propose") != capabilities.end() &&
        std::find(capabilities.begin(), capabilities.end(),
            "rules.foundation.rerack") == capabilities.end(),
        "HEADLESS_STALEMATE_COMMANDS_EXPOSE_NO_DIRECT_CORE_E_ACTION");
    expect(std::find(capabilities.begin(), capabilities.end(),
            "wpa_ii_19_group_confusion_referee_consumer_v1") !=
            capabilities.end() &&
        std::find(capabilities.begin(), capabilities.end(),
            "rules.group_confusion_propose") != capabilities.end() &&
        std::find(capabilities.begin(), capabilities.end(),
            "rules.group_confusion_final") != capabilities.end() &&
        std::find(capabilities.begin(), capabilities.end(),
            "rules.group_confusion_cancel") == capabilities.end() &&
        std::find(capabilities.begin(), capabilities.end(),
            "rules.foundation.rerack") == capabilities.end(),
        "HEADLESS_GROUP_CONFUSION_EXPOSES_NO_CANCEL_OR_CORE_E");
    const auto confusionReadback = send(controller, 2016,
        "get_group_confusion_authority_state");
    expect(confusionReadback.response.at("ok").asBool() &&
        confusionReadback.response.at("result").at("phase").asString() ==
            "INACTIVE" &&
        !confusionReadback.response.at("result").has("credential_id") &&
        !confusionReadback.response.at("result").has(
            "authentication_proof") &&
        !confusionReadback.response.at("result").has(
            "payload_canonical") &&
        !confusionReadback.response.at("result").at(
            "credential_material_exposed").asBool(),
        "AUTOMATION_GROUP_CONFUSION_READBACK_IS_SECRET_FREE");
    billiardgl::json::Value incompleteConfusion =
        billiardgl::json::Value::object();
    incompleteConfusion["schema_version"] = billiardgl::json::Value(1);
    expect(!send(controller, 2017, "rules.group_confusion_propose",
            incompleteConfusion).response.at("ok").asBool() &&
        send(controller, 2018, "get_group_confusion_authority_state")
            .response.at("result").at("phase").asString() == "INACTIVE",
        "AUTOMATION_GROUP_CONFUSION_BAD_ENVELOPE_FAILS_CLOSED");
    expect(std::find(capabilities.begin(),capabilities.end(),
            "wpa_ii_15_1_signed_frozen_declaration_v1")!=capabilities.end()&&
        std::find(capabilities.begin(),capabilities.end(),
            "rules.frozen_cue_object_propose")!=capabilities.end()&&
        std::find(capabilities.begin(),capabilities.end(),
            "rules.frozen_cue_object_final")!=capabilities.end()&&
        std::find(capabilities.begin(),capabilities.end(),
            "rules.frozen_cue_object_cancel")!=capabilities.end(),
        "HEADLESS_FROZEN_DECLARATION_CAPABILITY_IS_EXACT");
    const auto frozenReadback=send(controller,2019,
        "get_frozen_cue_object_declaration_state");
    expect(frozenReadback.response.at("ok").asBool()&&
        frozenReadback.response.at("result").at("phase").asString()==
            "INACTIVE"&&
        !frozenReadback.response.at("result").has("credential_id")&&
        !frozenReadback.response.at("result").has("authentication_proof")&&
        !frozenReadback.response.at("result").has("payload_canonical")&&
        !frozenReadback.response.at("result").at(
            "credential_material_exposed").asBool(),
        "AUTOMATION_FROZEN_DECLARATION_READBACK_IS_SECRET_FREE");
    billiardgl::json::Value incompleteFrozen=billiardgl::json::Value::object();
    incompleteFrozen["schema_version"]=billiardgl::json::Value(1);
    expect(!send(controller,2020,"rules.frozen_cue_object_propose",
            incompleteFrozen).response.at("ok").asBool()&&
        send(controller,2021,"get_frozen_cue_object_declaration_state")
            .response.at("result").at("phase").asString()=="INACTIVE",
        "AUTOMATION_FROZEN_BAD_ENVELOPE_FAILS_CLOSED");
    expect(std::find(capabilities.begin(),capabilities.end(),
            "wpa_ii_15_2_signed_frozen_cushion_declaration_v1")!=
            capabilities.end()&&
        std::find(capabilities.begin(),capabilities.end(),
            "rules.frozen_object_cushion_propose")!=capabilities.end()&&
        std::find(capabilities.begin(),capabilities.end(),
            "rules.frozen_object_cushion_final")!=capabilities.end()&&
        std::find(capabilities.begin(),capabilities.end(),
            "rules.frozen_object_cushion_cancel")!=capabilities.end(),
        "HEADLESS_FROZEN_CUSHION_CAPABILITY_IS_EXACT");
    const auto cushionReadback=send(controller,2022,
        "get_frozen_object_cushion_declaration_state");
    expect(cushionReadback.response.at("ok").asBool()&&
        cushionReadback.response.at("result").at("phase").asString()==
            "INACTIVE"&&
        !cushionReadback.response.at("result").at(
            "section_15_2_result_classified").asBool()&&
        !cushionReadback.response.at("result").has("credential_id")&&
        !cushionReadback.response.at("result").has("authentication_proof")&&
        !cushionReadback.response.at("result").has("payload_canonical")&&
        !cushionReadback.response.at("result").at(
            "credential_material_exposed").asBool(),
        "AUTOMATION_FROZEN_CUSHION_READBACK_IS_SECRET_FREE_NO_RESULT");
    billiardgl::json::Value incompleteCushion=
        billiardgl::json::Value::object();
    incompleteCushion["schema_version"]=billiardgl::json::Value(1);
    expect(!send(controller,2023,"rules.frozen_object_cushion_propose",
            incompleteCushion).response.at("ok").asBool()&&
        send(controller,2024,"get_frozen_object_cushion_declaration_state")
            .response.at("result").at("phase").asString()=="INACTIVE",
        "AUTOMATION_FROZEN_CUSHION_BAD_ENVELOPE_FAILS_CLOSED");
    expect(std::find(capabilities.begin(),capabilities.end(),
            "wpa_ii_12_5_6_signed_close_case_declaration_v1")!=capabilities.end()&&
        std::find(capabilities.begin(),capabilities.end(),
            "rules.double_hit_close_case_propose")!=capabilities.end()&&
        std::find(capabilities.begin(),capabilities.end(),
            "rules.double_hit_close_case_final")!=capabilities.end()&&
        std::find(capabilities.begin(),capabilities.end(),
            "rules.double_hit_close_case_cancel")!=capabilities.end(),
        "HEADLESS_CLOSE_CASE_CAPABILITY_IS_EXACT");
    const auto closeReadback=send(controller,2025,"get_double_hit_close_case_state");
    expect(closeReadback.response.at("ok").asBool()&&
        closeReadback.response.at("result").at("phase").asString()=="INACTIVE"&&
        !closeReadback.response.at("result").at("review_needed").asBool()&&
        !closeReadback.response.at("result").at("double_hit_classified").asBool()&&
        !closeReadback.response.at("result").at("foul_classified").asBool()&&
        !closeReadback.response.at("result").has("credential_id")&&
        !closeReadback.response.at("result").has("authentication_proof")&&
        !closeReadback.response.at("result").has("payload_canonical"),
        "AUTOMATION_CLOSE_CASE_READBACK_SECRET_FREE_RESULT_FALSE");
    billiardgl::json::Value incompleteClose=billiardgl::json::Value::object();
    incompleteClose["schema_version"]=billiardgl::json::Value(1);
    expect(!send(controller,2026,"rules.double_hit_close_case_propose",
            incompleteClose).response.at("ok").asBool()&&
        send(controller,2027,"get_double_hit_close_case_state")
            .response.at("result").at("phase").asString()=="INACTIVE",
        "AUTOMATION_CLOSE_CASE_BAD_ENVELOPE_FAILS_CLOSED");
    expect(std::find(capabilities.begin(),capabilities.end(),
            "wpa_ii_12_7_signed_push_review_declaration_v1")!=capabilities.end()&&
        std::find(capabilities.begin(),capabilities.end(),"rules.push_review_propose")!=capabilities.end()&&
        std::find(capabilities.begin(),capabilities.end(),"rules.push_review_final")!=capabilities.end()&&
        std::find(capabilities.begin(),capabilities.end(),"rules.push_review_cancel")!=capabilities.end(),
        "HEADLESS_PUSH_REVIEW_CAPABILITY_EXACT");
    const auto pushReadback=send(controller,2028,"get_push_review_state");
    expect(pushReadback.response.at("ok").asBool()&&
        !pushReadback.response.at("result").at("measurement_complete").asBool()&&
        !pushReadback.response.at("result").at("push_classified").asBool()&&
        !pushReadback.response.at("result").at("foul_classified").asBool()&&
        !pushReadback.response.at("result").has("credential_id"),
        "PUSH_REVIEW_READBACK_SECRET_FREE_RESULT_FALSE");
    billiardgl::json::Value incompletePush=billiardgl::json::Value::object();
    incompletePush["schema_version"]=billiardgl::json::Value(1);
    expect(!send(controller,2029,"rules.push_review_propose",incompletePush)
            .response.at("ok").asBool()&&
        send(controller,2030,"get_push_review_state")
            .response.at("result").at("phase").asString()=="INACTIVE",
        "AUTOMATION_PUSH_REVIEW_BAD_ENVELOPE_FAILS_CLOSED");
    expect(std::find(capabilities.begin(),capabilities.end(),
            "wpa_ii_17_exact_nonphysical_referee_foul_subset_v1")!=
            capabilities.end()&&
        std::find(capabilities.begin(),capabilities.end(),
            "rules.nonphysical_general_foul_propose")!=capabilities.end()&&
        std::find(capabilities.begin(),capabilities.end(),
            "rules.nonphysical_general_foul_final")!=capabilities.end()&&
        std::find(capabilities.begin(),capabilities.end(),
            "rules.nonphysical_general_foul_cancel")!=capabilities.end(),
        "HEADLESS_NONPHYSICAL_GENERAL_FOUL_CAPABILITY_EXACT");
    const auto foulReadback=send(controller,2031,
        "get_nonphysical_general_foul_state");
    expect(foulReadback.response.at("ok").asBool()&&
        foulReadback.response.at("result").at("phase").asString()=="INACTIVE"&&
        !foulReadback.response.at("result").at("full_wpa_ii_17_complete").asBool()&&
        !foulReadback.response.at("result").at("d5_complete").asBool()&&
        !foulReadback.response.at("result").at("d6_complete").asBool()&&
        !foulReadback.response.at("result").has("credential_id"),
        "NONPHYSICAL_GENERAL_FOUL_READBACK_SECRET_FREE_PARTIAL");
    billiardgl::json::Value incompleteFoul=billiardgl::json::Value::object();
    incompleteFoul["schema_version"]=billiardgl::json::Value(1);
    expect(!send(controller,2032,"rules.nonphysical_general_foul_propose",
            incompleteFoul).response.at("ok").asBool()&&
        send(controller,2033,"get_nonphysical_general_foul_state")
            .response.at("result").at("phase").asString()=="INACTIVE",
        "NONPHYSICAL_GENERAL_FOUL_BAD_ENVELOPE_FAILS_CLOSED");
    expect(std::find(capabilities.begin(),capabilities.end(),
            "wpa_ii_06_g_signed_unintentional_break_miscue_v1")!=
            capabilities.end()&&
        std::find(capabilities.begin(),capabilities.end(),
            "rules.unintentional_break_miscue_propose")!=capabilities.end()&&
        std::find(capabilities.begin(),capabilities.end(),
            "rules.unintentional_break_miscue_final")!=capabilities.end()&&
        std::find(capabilities.begin(),capabilities.end(),
            "rules.unintentional_break_miscue_cancel")!=capabilities.end(),
        "HEADLESS_06G_SIGNED_DECLARATION_CAPABILITY_IS_EXACT");
    {
        using namespace billiardgl;
        using namespace billiardgl::rules;
        GameRuntime declarationRuntime;
        expect(declarationRuntime.setRulesMode(
                RulesMode::ChineseEightBallV1).ok,
            "06G automation fixture enables Chinese rules");
        StalemateAutomationAuthenticator authenticator;
        declarationRuntime.
            configureUnintentionalBreakMiscueDeclarationAuthority(
                &authenticator);
        AutomationController declarationController(
            declarationRuntime, AutomationMode::Headless);
        const auto initial = send(declarationController, 2034,
            "get_unintentional_break_miscue_state");
        expect(initial.response.at("ok").asBool()&&
            initial.response.at("result").at("phase").asString()==
                "INACTIVE"&&
            !initial.response.at("result").has("credential_id")&&
            !initial.response.at("result").has("authentication_proof")&&
            !initial.response.at("result").has("payload_canonical"),
            "06G automation readback is secret-free before declaration");
        const std::string proposalPayload=
            "{\"actor_player_id\":0,\"relation\":\"UNINTENTIONAL_BREAK_MISCUE_IF_NATIVE_MISCUE_NO_RACK_CONTACT\"}";
        auto proposal=unintentionalBreakMiscueRequest(declarationRuntime,
            RefereeAuthorityOperation::Propose,"automation.06g.proposal",
            proposalPayload);
        auto unsignedProposal=proposal;
        unsignedProposal.authenticationProof.clear();
        const auto unsignedResult=send(declarationController,2035,
            "rules.unintentional_break_miscue_propose",
            authorityParams(unsignedProposal));
        expect(!unsignedResult.response.at("ok").asBool()&&
            send(declarationController,2036,
                "get_unintentional_break_miscue_state")
                .response.at("result").at("phase").asString()=="INACTIVE",
            "06G automation rejects unsigned referee input without mutation");
        const auto proposed=send(declarationController,2037,
            "rules.unintentional_break_miscue_propose",
            authorityParams(proposal));
        const std::string finalPayload=
            "{\"actor_player_id\":0,\"proposal_decision_id\":\"automation.unintentional_break_miscue.1\",\"relation\":\"UNINTENTIONAL_BREAK_MISCUE_IF_NATIVE_MISCUE_NO_RACK_CONTACT\"}";
        auto final=unintentionalBreakMiscueRequest(declarationRuntime,
            RefereeAuthorityOperation::FinalDeclare,"automation.06g.final",
            finalPayload);
        const auto committed=send(declarationController,2038,
            "rules.unintentional_break_miscue_final",authorityParams(final));
        const auto active=send(declarationController,2039,
            "get_unintentional_break_miscue_state");
        expect(proposed.response.at("ok").asBool()&&
            committed.response.at("ok").asBool()&&
            active.response.at("result").at("phase").asString()=="ACTIVE"&&
            active.response.at("result").at("actor_player_id").asInt()==0&&
            active.response.at("result").at("committed").asBool()&&
            !active.response.at("result").has("credential_id")&&
            !active.response.at("result").has("authentication_proof"),
            "06G automation accepts exact signed proposal/final and exposes only secret-free state");
    }
    billiardgl::json::Value incompleteStalemate =
        billiardgl::json::Value::object();
    incompleteStalemate["schema_version"] = billiardgl::json::Value(1);
    const auto rejectedStalemate = send(controller, 2012,
        "rules.stalemate_referee_propose", incompleteStalemate);
    expect(!rejectedStalemate.response.at("ok").asBool() &&
        send(controller, 2013, "get_stalemate_authority_state")
            .response.at("result").at("phase").asString() == "INACTIVE",
        "AUTOMATION_INCOMPLETE_AUTHORITY_ENVELOPE_FAILS_CLOSED");
    StalemateAutomationAuthenticator stalemateAuthenticator;
    runtime.configureStalemateAuthority(&stalemateAuthenticator);
    billiardgl::rules::RefereeAuthorityRequest stalemateProposal;
    stalemateProposal.protocolId =
        billiardgl::rules::kRefereeAuthorityProtocolId;
    stalemateProposal.schemaVersion = 1;
    stalemateProposal.operation =
        billiardgl::rules::RefereeAuthorityOperation::Propose;
    stalemateProposal.declarationKind =
        billiardgl::rules::kStalemateDirectiveKind;
    stalemateProposal.decisionId = "automation.stalemate.1";
    stalemateProposal.rackIdentity = runtime.stalemateState().rackIdentity;
    stalemateProposal.expectedAuthorityRevision =
        runtime.stalemateProtocolState().authorityRevision;
    stalemateProposal.expectedAuthorityStateHash =
        runtime.stalemateProtocolState().authorityStateHash;
    stalemateProposal.authorityRole =
        billiardgl::rules::RefereeAuthorityRole::Referee;
    stalemateProposal.authoritySubjectId = "automation.referee";
    stalemateProposal.credentialId = "automation.credential";
    stalemateProposal.keyVersion = "v1";
    stalemateProposal.authenticationNonce = "automation.nonce.1";
    stalemateProposal.expiresAtAuthorityTick = 100;
    stalemateProposal.idempotencyKey = "automation.key.1";
    stalemateProposal.payloadCanonical =
        "{\"directive_kind\":\"DEFAULT_ADVISORY_THREE_EACH\","
        "\"per_player_shot_allowance\":3,\"stalemate_attested\":true}";
    stalemateProposal.authenticationProof = billiardgl::sha256Hex(
        "automation-secret|" +
        billiardgl::rules::canonicalRefereeAuthenticatedBytes(
            stalemateProposal));
    const auto automationProposal = send(controller, 2014,
        "rules.stalemate_referee_propose",
        authorityParams(stalemateProposal));
    const auto automationState = send(controller, 2015,
        "get_stalemate_authority_state");
    expect(automationProposal.response.at("ok").asBool() &&
        automationState.response.at("result").at("phase").asString() ==
            "PROPOSAL_OPEN" &&
        automationState.response.at("result")
            .at("proposal_decision_id").asString() ==
            "automation.stalemate.1" &&
        !automationState.response.at("result").has("credential_id") &&
        !automationState.response.at("result").has("authentication_proof"),
        "AUTOMATION_SECRET_FREE_STATE_ROUNDTRIP");
    billiardgl::json::Value performanceParams =
        billiardgl::json::Value::object();
    performanceParams["warmup_frames"] = billiardgl::json::Value(120);
    performanceParams["sample_frames"] = billiardgl::json::Value(300);
    expect(!send(controller, 200, "collect_render_performance_v1",
            performanceParams).response.at("ok").asBool(),
        "headless render performance request fails closed");
    billiardgl::GameRuntime renderedRuntime;
    billiardgl::AutomationController renderedController(
        renderedRuntime, billiardgl::AutomationMode::Rendered);
    const auto renderedPerformance = send(renderedController, 201,
        "collect_render_performance_v1", performanceParams);
    expect(renderedPerformance.response.at("ok").asBool() &&
        renderedPerformance.renderPerformanceRequested &&
        renderedPerformance.performanceWarmupFrames == 120 &&
        renderedPerformance.performanceSampleFrames == 300,
        "rendered automation stages exact formal performance collection");
    performanceParams["sample_frames"] = billiardgl::json::Value(299);
    expect(!send(renderedController, 202, "collect_render_performance_v1",
            performanceParams).response.at("ok").asBool(),
        "undersampled formal performance collection is rejected");

    billiardgl::GameRuntime legacyLowAimRuntime;
    billiardgl::AutomationController legacyLowAimController(
        legacyLowAimRuntime, billiardgl::AutomationMode::Rendered);
    billiardgl::json::Value legacyOrbit = billiardgl::json::Value::object();
    legacyOrbit["yaw_delta"] = billiardgl::json::Value(0.0);
    legacyOrbit["pitch_delta"] = billiardgl::json::Value(0.40);
    billiardgl::json::Value legacyZoom = billiardgl::json::Value::object();
    legacyZoom["delta"] = billiardgl::json::Value(-38.0);
    expect(send(legacyLowAimController, 203, "orbit_camera", legacyOrbit)
            .response.at("ok").asBool() &&
        send(legacyLowAimController, 204, "zoom_camera", legacyZoom)
            .response.at("ok").asBool() &&
        send(legacyLowAimController, 205, "toggle_aim")
            .response.at("ok").asBool(),
        "v3 low-aim camera commands should reproduce on CPU");
    const auto legacyLowAimPresentation = send(
        legacyLowAimController, 206, "get_presentation_state")
            .response.at("result");
    expect(legacyLowAimPresentation.at("camera").at("state").asString() ==
            "aim_player" &&
        legacyLowAimPresentation.at("camera").at("preset").asString() ==
            "player_low" &&
        legacyLowAimPresentation.at("camera").at("transitioning").asBool(),
        "v3 low-aim sequence leaves the semantic camera at transition alpha zero");

    billiardgl::GameRuntime formalLowAimRuntime;
    billiardgl::AutomationController formalLowAimController(
        formalLowAimRuntime, billiardgl::AutomationMode::Rendered);
    billiardgl::json::Value instantPlayerLow =
        billiardgl::json::Value::object();
    instantPlayerLow["schema_version"] = billiardgl::json::Value(1);
    instantPlayerLow["preset"] = billiardgl::json::Value("player_low");
    instantPlayerLow["instant"] = billiardgl::json::Value(true);
    expect(send(formalLowAimController, 207, "toggle_aim")
            .response.at("ok").asBool() &&
        send(formalLowAimController, 208, "set_camera_preset",
            instantPlayerLow).response.at("ok").asBool(),
        "formal low-aim fixes PlayerLow after entering aim without physics steps");
    const auto formalLowAimPresentation = send(
        formalLowAimController, 210, "get_presentation_state")
            .response.at("result");
    expect(formalLowAimRuntime.tick() == 0 &&
        formalLowAimPresentation.at("camera").at("state").asString() ==
            "aim_player" &&
        formalLowAimPresentation.at("camera").at("preset").asString() ==
            "player_low" &&
        !formalLowAimPresentation.at("camera").at("transitioning").asBool() &&
        std::abs(formalLowAimPresentation.at("camera").at("distance_cm")
            .asNumber() - 145.0) < 0.001 &&
        formalLowAimPresentation.at("camera").at("eye_cm").at("y")
            .asNumber() < 221.0,
        "formal low-aim camera is settled at the expected distance without advancing physics");
    billiardgl::json::Value badInstant = billiardgl::json::Value::object();
    badInstant["preset"] = billiardgl::json::Value("player_low");
    badInstant["instant"] = billiardgl::json::Value("true");
    expect(!send(formalLowAimController, 211, "set_camera_preset", badInstant)
            .response.at("ok").asBool(),
        "non-boolean instant camera option is rejected");

    const auto& lowFrame = formalLowAimRuntime.cameraFrame();
    const auto lowProjection = billiardgl::render::buildSharedCameraProjection(
        cameraPresentation(lowFrame), 1920, 1080);
    const auto& cueBall = formalLowAimRuntime.state().balls[0].position;
    constexpr float lampBottomCm = billiardgl::kTableModelPlaySurfaceHeightCm +
        135.0f - 0.6f;
    expect(lowProjection.valid && lowFrame.pose.target.x == cueBall.x &&
        lowFrame.pose.target.y == cueBall.y &&
        lowFrame.pose.target.z == cueBall.z &&
        lampBottomCm - lowFrame.pose.eye.y >= 50.0f,
        "formal low-aim camera targets cue ball with required lamp clearance");
    expect(projectsInto(lowProjection, cueBall,
            0.18, 0.36, 0.82, 0.78),
        "formal low-aim cue-ball center projects into unchanged ROI");
    expect(projectsInto(lowProjection,
            billiardgl::Point3{cueBall.x, cueBall.y, cueBall.z - 30.0f},
            0.18, 0.36, 0.82, 0.78),
        "formal low-aim cue silhouette anchor projects into unchanged ROI");
    expect(projectsInto(lowProjection,
            billiardgl::Point3{cueBall.x, cueBall.y, cueBall.z + 30.0f},
            0.18, 0.36, 0.82, 0.78),
        "formal low-aim aim-guide anchor projects into unchanged ROI");

    billiardgl::GameRuntime breakRuntime;
    billiardgl::AutomationController breakController(
        breakRuntime, billiardgl::AutomationMode::Rendered);
    billiardgl::json::Value instantElevated = billiardgl::json::Value::object();
    instantElevated["schema_version"] = billiardgl::json::Value(1);
    instantElevated["preset"] = billiardgl::json::Value("elevated");
    instantElevated["instant"] = billiardgl::json::Value(true);
    billiardgl::json::Value breakOrbit = billiardgl::json::Value::object();
    breakOrbit["yaw_delta"] = billiardgl::json::Value(0.0);
    breakOrbit["pitch_delta"] = billiardgl::json::Value(
        20.0 * 3.14159265358979323846 / 180.0 -
        42.0 * 3.14159265358979323846 / 180.0);
    billiardgl::json::Value breakZoom = billiardgl::json::Value::object();
    breakZoom["delta"] = billiardgl::json::Value(100.0);
    const std::string rulesBefore = breakRuntime.rulesAuthorityStateHash();
    const auto ballsBefore = breakRuntime.state().balls;
    expect(send(breakController, 212, "set_camera_preset", instantElevated)
            .response.at("ok").asBool() &&
        send(breakController, 213, "orbit_camera", breakOrbit)
            .response.at("ok").asBool() &&
        send(breakController, 214, "zoom_camera", breakZoom)
            .response.at("ok").asBool(),
        "formal break starts from settled elevated semantic camera");
    const auto& breakFrame = breakRuntime.cameraFrame();
    const auto breakProjection = billiardgl::render::buildSharedCameraProjection(
        cameraPresentation(breakFrame), 1920, 1080);
    bool ballsUnchanged = true;
    for (std::size_t index = 0; index < ballsBefore.size(); ++index) {
        ballsUnchanged = ballsUnchanged &&
            breakRuntime.state().balls[index].position.x == ballsBefore[index].position.x &&
            breakRuntime.state().balls[index].position.y == ballsBefore[index].position.y &&
            breakRuntime.state().balls[index].position.z == ballsBefore[index].position.z;
    }
    const float playY = billiardgl::kTableModelPlaySurfaceHeightCm;
    expect(breakRuntime.tick() == 0 && !breakFrame.transitioning &&
        breakFrame.state == billiardgl::camera::SemanticState::ObserveFree &&
        breakFrame.preset == billiardgl::camera::Preset::Elevated &&
        breakProjection.valid && breakFrame.pose.eye.y < lampBottomCm &&
        rulesBefore == breakRuntime.rulesAuthorityStateHash() && ballsUnchanged,
        "formal break instant anchor preserves simulation and settles below lamp");
    expect(projectsInto(breakProjection,
            billiardgl::Point3{-63.5f, playY, -127.0f},
            0.10, 0.16, 0.90, 0.84),
        "formal break near-left playfield corner projects into unchanged ROI");
    expect(projectsInto(breakProjection,
            billiardgl::Point3{63.5f, playY, 127.0f},
            0.10, 0.16, 0.90, 0.84),
        "formal break far-right playfield corner projects into unchanged ROI");
    expect(projectsInto(breakProjection, breakRuntime.state().balls[0].position,
            0.10, 0.16, 0.90, 0.84),
        "formal break cue ball projects into unchanged ROI");
    expect(projectsInto(breakProjection, breakRuntime.state().balls[1].position,
            0.10, 0.16, 0.90, 0.84),
        "formal break rack apex projects into unchanged ROI");

    billiardgl::GameRuntime interactiveCameraRuntime;
    billiardgl::AutomationController interactiveCameraController(
        interactiveCameraRuntime, billiardgl::AutomationMode::Rendered);
    billiardgl::json::Value normalPlayerLow = billiardgl::json::Value::object();
    normalPlayerLow["schema_version"] = billiardgl::json::Value(1);
    normalPlayerLow["preset"] = billiardgl::json::Value("player_low");
    expect(send(interactiveCameraController, 214, "set_camera_preset",
            normalPlayerLow).response.at("ok").asBool() &&
        interactiveCameraRuntime.cameraFrame().transitioning,
        "default semantic preset preserves normal product transition behavior");
    const billiardgl::json::Value overlay = send(controller, 201,
        "training_get_overlay_state").response.at("result");
    expect(overlay.at("advanced_expanded").asBool() == false &&
        overlay.at("authority_role").asString() == "presentation_only" &&
        overlay.at("gpu_evidence_status").asString() == "NOT_RUN" &&
        overlay.at("predicted").at("style").asString() == "dashed" &&
        overlay.at("actual").at("style").asString() ==
            "solid_with_perpendicular_ticks",
        "automation exposes machine-readable overlay semantics without pass claims");

    send(controller, 3, "toggle_aim");
    expect(runtime.state().aim.mode == billiardgl::AimMode::Aim, "toggle aim should use game input logic");
    billiardgl::json::Value yaw = billiardgl::json::Value::object(); yaw["yaw"] = billiardgl::json::Value(0.25);
    expect(send(controller, 4, "set_aim_yaw", yaw).response.at("ok").asBool(), "yaw should succeed");
    billiardgl::json::Value power = billiardgl::json::Value::object(); power["power"] = billiardgl::json::Value(80);
    send(controller, 5, "set_shot_power", power);
    send(controller, 6, "shoot");
    expect(runtime.state().ballsMoving, "shoot should start motion");

    expect(!send(controller, 7, "does_not_exist").response.at("ok").asBool(), "unknown command should fail");
    billiardgl::json::Value badPower = billiardgl::json::Value::object(); badPower["power"] = billiardgl::json::Value(201);
    expect(!send(controller, 8, "set_shot_power", badPower).response.at("ok").asBool(), "bad power should fail");
    expect(send(controller, 9, "quit").quitRequested, "quit should request clean shutdown");
    send(controller, 10, "reset_game");
    expect(runtime.tick() == 0 && !runtime.state().ballsMoving, "reset should restore game");

    expect(send(controller, 11, "start_physics_trace").response.at("ok").asBool(),
        "trace start should succeed");
    billiardgl::json::Value ticks = billiardgl::json::Value::object();
    ticks["ticks"] = billiardgl::json::Value(2);
    expect(send(controller, 12, "step", ticks).response.at("ok").asBool(),
        "traced step should succeed");

    billiardgl::json::Value firstPageParams = billiardgl::json::Value::object();
    firstPageParams["after_tick"] = billiardgl::json::Value(0);
    firstPageParams["limit"] = billiardgl::json::Value(1);
    const billiardgl::json::Value firstPage =
        send(controller, 13, "get_physics_trace", firstPageParams).response.at("result");
    expect(firstPage.at("frames").asArray().size() == 1, "trace limit should paginate");
    expect(firstPage.at("has_more").asBool(), "first page should report more frames");
    expect(firstPage.at("dropped_frames").asInt() == 0, "new trace should not drop frames");

    const billiardgl::json::Value& frame = firstPage.at("frames").asArray()[0];
    expect(frame.has("balls") && frame.has("contacts") && frame.has("control"),
        "trace frame should include state, contacts, and controls");
    expect(frame.has("linear_momentum_kg_mps") &&
        frame.has("translational_kinetic_energy_j") &&
        frame.has("maximum_penetration_cm"), "trace frame should include physical totals");
    expect(frame.at("balls").asArray()[0].has("acceleration_cm_s2") &&
        frame.at("balls").asArray()[0].has("angular_velocity_rad_s"),
        "ball trace should include acceleration and angular velocity");

    billiardgl::json::Value secondPageParams = billiardgl::json::Value::object();
    secondPageParams["after_tick"] = billiardgl::json::Value(1);
    secondPageParams["limit"] = billiardgl::json::Value(1);
    const billiardgl::json::Value secondPage =
        send(controller, 14, "get_physics_trace", secondPageParams).response.at("result");
    expect(secondPage.at("frames").asArray().size() == 1 && !secondPage.at("has_more").asBool(),
        "last trace page should terminate pagination");

    billiardgl::json::Value zeroLimit = billiardgl::json::Value::object();
    zeroLimit["limit"] = billiardgl::json::Value(0);
    expect(!send(controller, 15, "get_physics_trace", zeroLimit).response.at("ok").asBool(),
        "zero trace limit should fail");
    billiardgl::json::Value largeLimit = billiardgl::json::Value::object();
    largeLimit["limit"] = billiardgl::json::Value(1001);
    expect(!send(controller, 16, "get_physics_trace", largeLimit).response.at("ok").asBool(),
        "oversized trace limit should fail");
    billiardgl::json::Value largeAfterTick = billiardgl::json::Value::object();
    largeAfterTick["after_tick"] = billiardgl::json::Value(3000000000.0);
    expect(send(controller, 161, "get_physics_trace", largeAfterTick).response.at("ok").asBool(),
        "64-bit after_tick should be accepted");

    expect(send(controller, 17, "stop_physics_trace").response.at("ok").asBool(),
        "trace stop should succeed");
    expect(send(controller, 18, "clear_physics_trace").response.at("ok").asBool(),
        "trace clear should succeed");
    expect(runtime.physicsTrace().frames().empty(), "controller clear should remove trace frames");

    billiardgl::json::Value canonical = billiardgl::json::Value::object();
    canonical["scenario"] = fixture("free_roll_v1.json");
    expect(send(controller, 19, "load_scenario", canonical).response.at("ok").asBool(),
        "controller should load canonical scenario v1");
    expect(!runtime.state().balls[0].pocketed && runtime.state().balls[1].pocketed,
        "canonical scenario should atomically activate only listed balls");

    billiardgl::GameRuntime legacyGenerationRuntime;
    billiardgl::AutomationController legacyGenerationController(
        legacyGenerationRuntime, billiardgl::AutomationMode::Headless);
    billiardgl::json::Value legacyGeneration = billiardgl::json::Value::object();
    legacyGeneration["balls"] = legacyScenarioBalls(true,
        billiardgl::json::Value("18446744073709551615"));
    expect(send(legacyGenerationController, 191, "load_scenario", legacyGeneration)
            .response.at("ok").asBool() &&
        legacyGenerationRuntime.state().balls[0].trajectoryGeneration ==
            std::numeric_limits<std::uint64_t>::max(),
        "legacy load_scenario preserves full-width trajectory generation");
    billiardgl::json::Value legacyMissing = billiardgl::json::Value::object();
    legacyMissing["balls"] = legacyScenarioBalls();
    expect(send(legacyGenerationController, 192, "load_scenario", legacyMissing)
            .response.at("ok").asBool() &&
        legacyGenerationRuntime.state().balls[0].trajectoryGeneration == 0,
        "legacy load_scenario maps a missing trajectory generation to zero");
    const std::array<billiardgl::json::Value, 4> invalidGenerations = {{
        billiardgl::json::Value(-1), billiardgl::json::Value("01"),
        billiardgl::json::Value("-1"),
        billiardgl::json::Value("18446744073709551616")}};
    for (const billiardgl::json::Value& invalid : invalidGenerations) {
        billiardgl::json::Value mutation = billiardgl::json::Value::object();
        mutation["balls"] = legacyScenarioBalls(true, invalid);
        const billiardgl::GameState before = legacyGenerationRuntime.state();
        expect(!send(legacyGenerationController, 193, "load_scenario", mutation)
                .response.at("ok").asBool() &&
            legacyGenerationRuntime.state().balls[0].trajectoryGeneration ==
                before.balls[0].trajectoryGeneration,
            "legacy load_scenario rejects malformed or overflowing generation atomically");
    }

    billiardgl::GameRuntime directRuntime;
    const billiardgl::PhysicsScenarioResult parsed =
        billiardgl::parsePhysicsScenario(fixture("free_roll_v1.json"));
    expect(parsed.ok && billiardgl::applyPhysicsScenario(directRuntime, parsed.scenario).ok,
        "direct runtime should load the same canonical scenario");
    directRuntime.setPhysicsTraceEnabled(true);
    runtime.setPhysicsTraceEnabled(true);
    directRuntime.step(2);
    billiardgl::json::Value twoTicks = billiardgl::json::Value::object();
    twoTicks["ticks"] = billiardgl::json::Value(2);
    expect(send(controller, 20, "step", twoTicks).response.at("ok").asBool(),
        "controller path should step the canonical scenario");
    expect(directRuntime.physicsTrace().frames().size() == runtime.physicsTrace().frames().size(),
        "direct and controller paths should emit the same frame count");
    for (std::size_t index = 0; index < directRuntime.physicsTrace().frames().size(); ++index) {
        const std::string direct = billiardgl::json::stringify(
            billiardgl::serializePhysicsFrame(directRuntime.physicsTrace().frames()[index]));
        const std::string controlled = billiardgl::json::stringify(
            billiardgl::serializePhysicsFrame(runtime.physicsTrace().frames()[index]));
        expect(direct == controlled,
            "direct and controller paths should serialize identical authoritative frames");
    }

    billiardgl::GameRuntime userRuntime;
    billiardgl::GameRuntime authoritativeClone;
    billiardgl::AutomationController userController(
        userRuntime, billiardgl::AutomationMode::Headless);
    billiardgl::json::Value userYaw = billiardgl::json::Value::object();
    userYaw["yaw"] = billiardgl::json::Value(1.1);
    billiardgl::json::Value userPower = billiardgl::json::Value::object();
    userPower["power"] = billiardgl::json::Value(72.0);
    expect(send(userController, 201, "set_aim_yaw", userYaw).response.at("ok").asBool() &&
        send(userController, 202, "set_shot_power", userPower).response.at("ok").asBool() &&
        send(userController, 203, "shoot").response.at("ok").asBool(),
        "user-operation path should construct a shot through public controls");
    authoritativeClone.mutableState().aim.yaw = 1.1f;
    authoritativeClone.mutableState().input.shotPower = 72.0f;
    expect(userRuntime.hasCueImpactInput() &&
        authoritativeClone.applyCueImpact(userRuntime.cueImpactInput()).ok,
        "direct path should accept the identical public cue-impact input");
    userRuntime.setPhysicsTraceEnabled(true);
    authoritativeClone.setPhysicsTraceEnabled(true);
    billiardgl::json::Value fiveTicks = billiardgl::json::Value::object();
    fiveTicks["ticks"] = billiardgl::json::Value(5);
    expect(send(userController, 204, "step", fiveTicks).response.at("ok").asBool(),
        "user-operation path should step after the shot");
    authoritativeClone.step(5);
    expect(userRuntime.physicsTrace().frames().size() ==
        authoritativeClone.physicsTrace().frames().size(),
        "user and cloned authoritative paths should emit the same frame count");
    for (std::size_t index = 0;
         index < authoritativeClone.physicsTrace().frames().size(); ++index) {
        const std::string user = billiardgl::json::stringify(
            billiardgl::serializePhysicsFrame(userRuntime.physicsTrace().frames()[index]));
        const std::string direct = billiardgl::json::stringify(
            billiardgl::serializePhysicsFrame(
                authoritativeClone.physicsTrace().frames()[index]));
        expect(user == direct,
            "matching first authoritative state must produce identical subsequent traces");
    }

    billiardgl::json::Value cueV2 = fixture("cue_impact_v2_contract.json");
    billiardgl::json::Value cueParams = billiardgl::json::Value::object();
    cueParams["scenario"] = cueV2;
    expect(send(controller, 21, "load_scenario", cueParams).response.at("ok").asBool(),
        "controller should load canonical cue-impact scenario v2");
    const billiardgl::json::Value cueState =
        send(controller, 22, "get_state").response.at("result");
    expect(cueState.has("cue_impact") &&
        cueState.at("cue_impact").at("cue_speed_cm_s").asInt() == 100,
        "state should round-trip requested cue input");
    expect(!cueState.at("cue_impact_support").at("shot_executed").asBool(),
        "unsupported physical cue input must not fire a surrogate shot");
    bool hasSpeedMappingLimitation = false;
    bool hasVerticalOffsetLimitation = false;
    for (const billiardgl::json::Value& item :
            cueState.at("cue_impact_support").at("unsupported_codes").asArray()) {
        hasSpeedMappingLimitation |= item.asString() == "cue_speed_to_power_mapping_missing";
        hasVerticalOffsetLimitation |= item.asString() == "vertical_tip_offset_not_modeled";
    }
    expect(hasSpeedMappingLimitation && hasVerticalOffsetLimitation,
        "state should enumerate unsupported production cue fields");
    runtime.setPhysicsTraceEnabled(true);
    expect(send(controller, 23, "step", twoTicks).response.at("ok").asBool(),
        "cue-impact scenario should remain traceable without synthesizing a shot");
    const billiardgl::json::Value cueFrame = billiardgl::serializePhysicsFrame(
        runtime.physicsTrace().frames().front());
    expect(cueFrame.has("cue_impact"), "trace should round-trip requested cue input");

    billiardgl::json::Value cueV4 = fixture("profile_override_v3.json");
    cueV4["schema_version"] = billiardgl::json::Value(4);
    cueV4["physics_profile"]["cue"]["normal_restitution"] = billiardgl::json::Value(0.0);
    cueV4["physics_profile"]["cue"]["chalked_friction_coefficient"] = billiardgl::json::Value(0.6);
    cueV4["physics_profile"]["cue"]["unchalked_friction_coefficient"] = billiardgl::json::Value(0.1);
    cueV4["physics_profile"]["cue"]["maximum_reliable_offset_radius"] = billiardgl::json::Value(0.8);
    cueV4["physics_profile"]["cue"]["cue_speed_per_power_unit_cm_s"] = billiardgl::json::Value(1.34);
    cueV4["cue_impact"] = cueV2.at("cue_impact");
    cueV4["cue_impact"]["tip_offset_cm"] = billiardgl::json::parse("[0.0,0.0]").value;
    cueV4["cue_impact"]["tip_offset_radius"] = billiardgl::json::parse("[0.0,0.0]").value;
    cueV4["cue_impact"]["chalk_state"] = billiardgl::json::Value("CHALKED");
    cueParams["scenario"] = cueV4;
    expect(send(controller, 231, "load_scenario", cueParams).response.at("ok").asBool(),
        "automation should execute supported v4 cue contact");
    expect(runtime.state().balls[0].velocity.x > 0.0f &&
        runtime.hasCueContactResult() && runtime.cueContactResult().applied,
        "automation and scenario path should retain the applied contact");

    billiardgl::json::Value malformed = cueV2;
    malformed["cue_impact"]["cue_speed_cm_s"] = billiardgl::json::Value(-1.0);
    cueParams["scenario"] = malformed;
    expect(!send(controller, 24, "load_scenario", cueParams).response.at("ok").asBool(),
        "malformed cue-impact scenario should fail");
    expect(send(controller, 25, "get_state").response.at("result").at("cue_impact")
        .at("cue_speed_cm_s").asInt() == 100,
        "malformed scenario rejection must not mutate runtime state");
    return 0;
}
