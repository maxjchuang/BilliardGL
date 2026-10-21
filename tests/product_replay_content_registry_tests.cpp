#include "deterministic_hash.h"
#include "render/modern/modern_product_session.h"
#include "replay/product_replay_content_registry.h"
#include "replay/replay_recorder.h"
#include "rules/rack_executor.h"

#include <algorithm>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <stdexcept>

namespace {

using namespace billiardgl;
using namespace billiardgl::render;

void expect(bool value,const std::string& message)
{
    if(!value)throw std::runtime_error(message);
}

class Authenticator final:public rules::RefereeAuthorityAuthenticator {
public:
    bool verify(const rules::RefereeAuthenticationInput& input)const override
    {
        return input.authenticationProof==sha256Hex(
            "product-registry-secret|"+input.authenticatedBytes);
    }
};

class Dialog final:public ProductFileDialogAdapter {
public:
    ProductFileDialogSelection selection;
    ProductFileDialogSelection saveSelection;
    ProductFileDialogSelection chooseReplayFile()override{return selection;}
    ProductFileDialogSelection saveTrainingReplayFile(
        const std::string&)override{return saveSelection;}
};

rules::RefereeAuthorityRequest request(const GameRuntime& runtime,
    rules::RefereeAuthorityOperation operation,const std::string& decision,
    const std::string& key,const std::string& payload)
{
    rules::RefereeAuthorityRequest value;
    value.protocolId=rules::kRefereeAuthorityProtocolId;
    value.schemaVersion=1;value.operation=operation;
    value.declarationKind=rules::kNonphysicalGeneralFoulKind;
    value.decisionId=decision;
    value.rackIdentity=runtime.nonphysicalGeneralFoulState().rackIdentity;
    value.expectedAuthorityRevision=
        runtime.nonphysicalGeneralFoulProtocolState().authorityRevision;
    value.expectedAuthorityStateHash=
        runtime.nonphysicalGeneralFoulProtocolState().authorityStateHash;
    value.authorityRole=rules::RefereeAuthorityRole::Referee;
    value.authoritySubjectId="product.registry.referee";
    value.credentialId="product.registry.credential";
    value.keyVersion="v1";value.authenticationNonce=key+".nonce";
    value.expiresAtAuthorityTick=10000;value.idempotencyKey=key;
    value.payloadCanonical=payload;
    value.authenticationProof=sha256Hex("product-registry-secret|"+
        rules::canonicalRefereeAuthenticatedBytes(value));
    return value;
}

void commitOfficialRackTerminal(GameRuntime& runtime,
    ModernProductSession& session,Authenticator& authenticator)
{
    runtime.configureNonphysicalGeneralFoulAuthority(&authenticator);
    auto shot=[&]{return std::string("runtime.nonphysical_general_foul.shot.")+
        std::to_string(runtime.rulesAuthorityRevision());};
    const std::string firstShot=shot();
    auto firstProposal=request(runtime,rules::RefereeAuthorityOperation::Propose,
        "registry.first","registry.first.p",
        rules::canonicalNonphysicalGeneralFoulProposal(
            rules::NonphysicalGeneralFoulKind::
                MobileDeviceDisturbanceGeneralFoul,0,firstShot));
    expect(runtime.submitNonphysicalGeneralFoulProposal(firstProposal).ok,
        "first proposal failed");
    auto firstFinal=request(runtime,
        rules::RefereeAuthorityOperation::FinalDeclare,"registry.first",
        "registry.first.f",rules::canonicalNonphysicalGeneralFoulFinal(
            rules::NonphysicalGeneralFoulKind::
                MobileDeviceDisturbanceGeneralFoul,0,"registry.first",firstShot));
    expect(runtime.submitNonphysicalGeneralFoulFinal(firstFinal).ok&&
        runtime.ballInHandPlacementOffer(),"first final failed");
    const auto offer=*runtime.ballInHandPlacementOffer();
    rules::BallInHandPlacementRequest placement;
    placement.rulesetId=rules::kBallInHandRulesetId;
    placement.capabilityId=rules::kFreeBallPlacementCapabilityId;
    placement.flowId=rules::kFullTableBallInHandFlowId;
    placement.decisionId=offer.decisionId;
    placement.expectedAuthorityStateRevision=offer.authorityStateRevision;
    placement.idempotencyKey="registry.placement";
    placement.actorPlayerId=offer.actorPlayerId;
    placement.actionId=rules::kConfirmFullTablePlacementActionId;
    placement.position={runtime.state().balls[0].position.x,
        runtime.state().balls[0].position.z};
    expect(runtime.submitBallInHandPlacement(placement).ok,
        "placement failed");
    const std::string repeatShot=shot();
    auto repeatProposal=request(runtime,rules::RefereeAuthorityOperation::Propose,
        "registry.repeat","registry.repeat.p",
        rules::canonicalNonphysicalGeneralFoulProposal(
            rules::NonphysicalGeneralFoulKind::MobileDeviceRepeatLossOfRack,
            0,repeatShot));
    expect(runtime.submitNonphysicalGeneralFoulProposal(repeatProposal).ok,
        "repeat proposal failed");
    auto repeatFinal=request(runtime,
        rules::RefereeAuthorityOperation::FinalDeclare,"registry.repeat",
        "registry.repeat.f",rules::canonicalNonphysicalGeneralFoulFinal(
            rules::NonphysicalGeneralFoulKind::MobileDeviceRepeatLossOfRack,
            0,"registry.repeat",repeatShot));
    expect(runtime.submitNonphysicalGeneralFoulFinal(repeatFinal).ok,
        "repeat final failed");
    session.consumeAuthoritySteps();
    expect(runtime.step(1).ok,"terminal replay final tick advance failed");
    session.consumeAuthoritySteps();
}

}  // namespace

int main()
{
    try {
        GameRuntime authority;
        ai::AiPlayerController ai;
        Dialog dialog;
        ModernProductSessionOptions options;
        options.settingsPath=(std::filesystem::temp_directory_path()/
            "billiardgl-product-registry-settings.json").string();
        const std::string settingsPath=options.settingsPath;
        options.executableVersion="product-registry-test";
        options.fileDialogAdapter=&dialog;
        ModernProductSession session(authority,ai,std::move(options));
        expect(session.shell().openSinglePlayerSetup().ok&&
            session.shell().focusItem("start_match").ok,
            "open actual product match setup failed");
        PlatformInputEvent enter;
        enter.kind=PlatformInputKind::KeyDown;enter.control="Enter";
        expect(session.handlePlatformInput(enter).ok&&
            session.snapshot().matchReplayRecording,
            "actual product start did not begin trusted recording");
        GameAction aim;aim.type=ActionType::SetAimYaw;
        aim.first=1.57079632679f;
        GameAction power;power.type=ActionType::SetShotPower;
        power.first=200.0f;
        GameAction shoot;shoot.type=ActionType::Shoot;
        expect(authority.dispatch(aim).ok&&authority.dispatch(power).ok&&
            authority.dispatch(shoot).ok,"real product break dispatch failed");
        for(int tick=0;tick<7200&&authority.state().ballsMoving;++tick){
            expect(authority.step(1).ok,"real product break step failed");
            session.consumeAuthoritySteps();
        }
        expect(!authority.state().ballsMoving&&
            !authority.breakFoulDecisionPending()&&
            !authority.eightOnBreakDecisionPending()&&
            !authority.ballInHandPlacementPending(),
            "real product break did not settle without a pending branch");
        Authenticator authenticator;
        commitOfficialRackTerminal(authority,session,authenticator);
        const auto recorded=session.snapshot();
        expect(recorded.matchReplayReady&&recorded.matchReplayError.empty(),
            "single-rack terminal did not seal the product recorder: "+
                recorded.matchReplayError+" active="+
                std::to_string(recorded.matchReplayRecording)+" tick="+
                std::to_string(authority.tick()));
        const std::string liveFinalHash=gameStateSha256(authority.state());
        const auto path=std::filesystem::temp_directory_path()/
            "billiardgl-product-registry-v2.replay.json";
        dialog.saveSelection={true,path.string()};
        PlatformInputEvent saveKey;
        saveKey.kind=PlatformInputKind::KeyDown;saveKey.control="F9";
        const auto saved=session.handlePlatformInput(saveKey);
        expect(saved.ok&&
            session.snapshot().lastSavedReplayPath==path.string(),
            "product Match save entry did not commit the atomic replay: "+
                saved.errorCode+"/"+
                session.snapshot().lastSavedReplayPath);
        dialog.selection={true,path.string()};
        expect(session.shell().openReplayBrowser().ok&&
            session.chooseReplayFile().ok,"trusted official replay load failed");
        expect(session.snapshot().shell.replay.documentFormatVersion==2&&
            !session.snapshot().shell.replay.rulesEffectVerified,
            "load must not pre-verify rules effects");
        for(int tick=0;tick<7200&&
                !session.snapshot().shell.replay.finished;++tick)
            expect(session.shell().dispatchProductInput(
                {"keyboard",".","press",0.0,0.0}).ok,
                "rack replay step failed");
        const auto terminal=session.snapshot().shell;
        const auto item=std::find_if(terminal.items.begin(),terminal.items.end(),
            [](const auto& value){return value.id=="replay_rack_terminal";});
        expect(terminal.replay.finished&&terminal.replay.rulesEffectVerified&&
            terminal.replay.rackTerminalActive&&
            gameStateSha256(session.presentationRuntime().state())==
                liveFinalHash&&
            !session.presentationRuntime().state().gameOver&&
            item!=terminal.items.end()&&item->label=="Rack ended"&&
            item->detail.find("match remains distinct")!=std::string::npos,
            "rack terminal was misrepresented as match completion");
        const auto retained=terminal.replay;
        std::ifstream input(path,std::ios::binary);
        const std::string bytes((std::istreambuf_iterator<char>(input)),
            std::istreambuf_iterator<char>());
        auto parsed=json::parse(bytes);
        expect(parsed.ok,"saved replay parse failed");
        parsed.value["content_sha256"]=json::Value(
            sha256Hex("untrusted-product-content"));
        const auto bad=path.string()+".untrusted";
        std::ofstream output(bad,std::ios::binary|std::ios::trunc);
        output<<json::stringify(parsed.value)<<'\n';output.close();
        dialog.selection={true,bad};
        expect(session.shell().openReplayBrowser().ok,
            "return to browser failed");
        const auto rejected=session.chooseReplayFile();
        expect(!rejected.ok&&
            rejected.errorCode=="product_replay_content_untrusted"&&
            session.snapshot().shell.replay.rackTerminalDecisionId==
                retained.rackTerminalDecisionId,
            "untrusted load replaced current transport");
        GameRuntime rerackAuthority;
        ai::AiPlayerController rerackAi;
        Dialog rerackDialog;
        ModernProductSessionOptions rerackOptions;
        rerackOptions.settingsPath=(std::filesystem::temp_directory_path()/
            "billiardgl-product-rerack-settings.json").string();
        const std::string rerackSettingsPath=rerackOptions.settingsPath;
        rerackOptions.executableVersion="product-registry-test";
        rerackOptions.fileDialogAdapter=&rerackDialog;
        ModernProductSession rerackSession(
            rerackAuthority,rerackAi,std::move(rerackOptions));
        expect(rerackSession.shell().openSinglePlayerSetup().ok&&
            rerackSession.shell().focusItem("start_match").ok&&
            rerackSession.handlePlatformInput(enter).ok,
            "cross-rack fixture did not start product recording");
        rules::RerackFoundationParameters rerackParameters;
        rerackParameters.deterministicSeed=rules::kOfficialRackDefaultSeed;
        rerackParameters.breakerPlayerId=0;
        const auto rerackPlan=rerackAuthority.stageOfficialRerack(
            rerackParameters);
        expect(rerackPlan.ok&&rerackAuthority.commitRackFoundation(
            rerackPlan,"product.registry.unsupported.second-rack").ok,
            "cross-rack fixture rerack failed");
        rerackSession.consumeAuthoritySteps();
        expect(rerackSession.snapshot().matchReplayError==
                "product_match_replay_cross_rack_unsupported"&&
            !rerackSession.snapshot().matchReplayReady,
            "cross-rack recording did not fail closed with a readable limit");
        rerackDialog.saveSelection={true,path.string()};
        const auto crossRackSave=rerackSession.handlePlatformInput(saveKey);
        expect(!crossRackSave.ok&&crossRackSave.errorCode==
                "product_match_replay_cross_rack_unsupported",
            "cross-rack save must not overwrite a valid single-rack replay");
        std::ifstream preserved(path,std::ios::binary);
        const std::string preservedBytes(
            (std::istreambuf_iterator<char>(preserved)),
            std::istreambuf_iterator<char>());
        expect(preservedBytes==bytes,
            "rejected cross-rack save changed the existing replay file");
        GameRuntime invalidAuthority;
        ai::AiPlayerController invalidAi;
        Dialog invalidDialog;
        ModernProductSessionOptions invalidOptions;
        invalidOptions.settingsPath=(std::filesystem::temp_directory_path()/
            "billiardgl-product-invalid-recorder-settings.json").string();
        const std::string invalidSettingsPath=invalidOptions.settingsPath;
        invalidOptions.fileDialogAdapter=&invalidDialog;
        ModernProductSession invalidSession(
            invalidAuthority,invalidAi,std::move(invalidOptions));
        expect(invalidSession.shell().openSinglePlayerSetup().ok&&
            invalidSession.shell().focusItem("start_match").ok,
            "invalid recorder fixture setup failed");
        const auto invalidStart=invalidSession.handlePlatformInput(enter);
        expect(!invalidStart.ok&&invalidStart.errorCode==
                "replay_recording_binding_invalid"&&
            invalidSession.snapshot().matchReplayError==
                "replay_recording_binding_invalid"&&
            invalidSession.snapshot().shell.screen==ProductScreen::Match,
            "recorder begin failure was not surfaced after atomic match start");
        std::error_code ignored;
        std::filesystem::remove(path,ignored);
        std::filesystem::remove(bad,ignored);
        std::filesystem::remove(settingsPath,ignored);
        std::filesystem::remove(rerackSettingsPath,ignored);
        std::filesystem::remove(invalidSettingsPath,ignored);
        std::cout<<"Product replay registry checks passed\n";
        return 0;
    }catch(const std::exception& error){
        std::cerr<<error.what()<<'\n';return 1;
    }
}
