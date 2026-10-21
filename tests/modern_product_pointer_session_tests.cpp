#include "deterministic_hash.h"
#include "render_state.h"
#include "render/modern/modern_product_session.h"
#include "replay/replay_recorder.h"

#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <iterator>
#include <stdexcept>
#include <string>

void togglePanel(billiardgl::render::ModernProductSession& session);

namespace {

using namespace billiardgl;
using namespace billiardgl::render;

void expect(bool value, const std::string& message)
{
    if (!value) throw std::runtime_error("product pointer E2E: " + message);
}

PlatformInputEvent key(const std::string& control)
{
    PlatformInputEvent event;
    event.kind = PlatformInputKind::KeyDown;
    event.control = control;
    return event;
}

struct Point { double x = 0.0; double y = 0.0; };

Point itemCenter(const ModernProductSession& session, const std::string& id)
{
    const ui::ShellScreenLayout layout = session.shellLayout();
    expect(layout.ok, "current shell layout is valid");
    const auto found = std::find_if(layout.elements.begin(),
        layout.elements.end(), [&](const ui::ShellElementLayout& item) {
            return item.id == id;
        });
    expect(found != layout.elements.end(), "layout contains stable id " + id);
    return {
        (found->rect.x + found->rect.width * 0.5) *
            layout.viewport.logicalWidth / layout.viewport.physicalWidth,
        (found->rect.y + found->rect.height * 0.5) *
            layout.viewport.logicalHeight / layout.viewport.physicalHeight};
}

ModernProductEventResult pointer(ModernProductSession& session,
    PlatformInputKind kind, const Point& point,
    const std::string& button = {})
{
    PlatformInputEvent event;
    event.kind = kind;
    event.control = button;
    event.logicalX = point.x;
    event.logicalY = point.y;
    const auto viewport = session.snapshot().shellViewport;
    event.backingScale = viewport.logicalWidth > 0 ?
        static_cast<double>(viewport.physicalWidth) / viewport.logicalWidth : 1.0;
    return session.handlePlatformInput(event);
}

ModernProductEventResult click(ModernProductSession& session,
    const std::string& id)
{
    const Point point = itemCenter(session, id);
    expect(pointer(session, PlatformInputKind::PointerMove, point).handled,
        "pointer move is handled on shell");
    expect(session.snapshot().hoveredShellItemId == id,
        "hover exposes the stable semantic id");
    const auto shell = session.snapshot().shell;
    const auto focused = std::find_if(shell.items.begin(), shell.items.end(),
        [&](const ShellItemViewModel& item) {
            return item.id == id && item.focused;
        });
    expect(focused != shell.items.end(),
        "hover focuses the exact enabled semantic row");
    expect(pointer(session, PlatformInputKind::PointerButtonDown,
        point, "LeftButton").handled, "left down captures an enabled row");
    return pointer(session, PlatformInputKind::PointerButtonUp,
        point, "LeftButton");
}

ModernProductSession makeSession(GameRuntime& runtime,
    ai::AiPlayerController& ai, const std::string& suffix,
    bool loadTraining = true,
    ProductFileDialogAdapter* fileDialogAdapter = nullptr)
{
    ModernProductSessionOptions options;
    options.settingsPath = (std::filesystem::temp_directory_path() /
        ("billiardgl-pointer-" + suffix + ".json")).string();
    if (loadTraining) options.trainingManifestPath =
        std::string(BILLIARDGL_SOURCE_ROOT) +
        "/assets/training/phase4/training_pack_v1.json";
    options.executableVersion = "product-pointer-session-attestation-v1";
    options.fileDialogAdapter = fileDialogAdapter;
    return ModernProductSession(runtime, ai, std::move(options));
}

void stagePointerIllegalBreak(GameRuntime& runtime)
{
    rules::ShotFacts shot;
    shot.shooter = runtime.matchState().currentPlayer;
    shot.firstContactBall = 1;
    shot.anyRailAfterFirstContact = true;
    shot.breakShot = true;
    shot.breakEvidenceOrigin =
        rules::BreakEvidenceOrigin::AuthoritativeShotFactCollector;
    shot.breakEvidenceComplete = true;
    for (int index = 0; index < 3; ++index) {
        rules::BreakObjectCushionContact contact;
        contact.objectBallId = index + 1;
        contact.boundaryKind = rules::BreakCushionBoundaryKind::StraightCushion;
        contact.provenance =
            rules::BreakCushionContactProvenance::DiscretePhysics;
        contact.velocityImpulseApplied = true;
        contact.normalImpulseNs = 0.1;
        contact.physicsTick = 1;
        contact.canonicalContactOrdinal = index;
        contact.timeOfImpactSeconds = 0.001 * index;
        shot.breakObjectCushionContacts.push_back(contact);
    }
    shot.distinctObjectBallsToRail = 3;
    expect(runtime.adjudicateRulesShot(shot).ok &&
        runtime.breakFoulDecisionOffer().has_value(),
        "pointer fixture stages IllegalBreak authority offer");
}

class FakeFileDialogAdapter final : public ProductFileDialogAdapter {
public:
    ProductFileDialogSelection choose;
    ProductFileDialogSelection save;
    int chooseCalls = 0;
    int saveCalls = 0;
    std::string suggestedFilename;

    ProductFileDialogSelection chooseReplayFile() override
    {
        ++chooseCalls;
        return choose;
    }

    ProductFileDialogSelection saveTrainingReplayFile(
        const std::string& suggested) override
    {
        ++saveCalls;
        suggestedFilename = suggested;
        return save;
    }
};

class ProductReplayAuthenticator final :
    public rules::RefereeAuthorityAuthenticator {
public:
    bool verify(const rules::RefereeAuthenticationInput& input) const override
    {
        return input.authenticationProof==sha256Hex(
            "product-replay-secret|"+input.authenticatedBytes);
    }
};

rules::RefereeAuthorityRequest replayRefereeRequest(
    const GameRuntime& runtime,rules::RefereeAuthorityOperation operation,
    const std::string& decision,const std::string& key,
    const std::string& payload)
{
    rules::RefereeAuthorityRequest request;
    request.protocolId=rules::kRefereeAuthorityProtocolId;
    request.schemaVersion=1;request.operation=operation;
    request.declarationKind=rules::kNonphysicalGeneralFoulKind;
    request.decisionId=decision;
    request.rackIdentity=runtime.nonphysicalGeneralFoulState().rackIdentity;
    request.expectedAuthorityRevision=
        runtime.nonphysicalGeneralFoulProtocolState().authorityRevision;
    request.expectedAuthorityStateHash=
        runtime.nonphysicalGeneralFoulProtocolState().authorityStateHash;
    request.authorityRole=rules::RefereeAuthorityRole::Referee;
    request.authoritySubjectId="product.replay.referee";
    request.credentialId="product.replay.credential";
    request.keyVersion="v1";request.authenticationNonce=key+".nonce";
    request.expiresAtAuthorityTick=10000;request.idempotencyKey=key;
    request.payloadCanonical=payload;
    request.authenticationProof=sha256Hex("product-replay-secret|"+
        rules::canonicalRefereeAuthenticatedBytes(request));
    return request;
}

struct ReplayFixture {
    std::filesystem::path path;
    std::string executable = "product-pointer-replay-v1";
    std::string content = sha256Hex("product-pointer-replay-content");
};

ReplayFixture makeReplay(const PhysicsProfile& profile,
    PhysicsBoundaryMode boundaryMode)
{
    ReplayFixture output;
    output.path = std::filesystem::temp_directory_path() /
        "billiardgl-product-pointer.replay.json";
    GameRuntime runtime;
    expect(runtime.replaceStateForScenario(runtime.state(), profile, nullptr,
        false, boundaryMode).ok,
        "replay fixture adopts the active product physics contract");
    replay::Recorder recorder;
    expect(recorder.begin(runtime, output.executable, output.content, 77).ok,
        "replay fixture begins recording");
    GameAction power;
    power.type = ActionType::SetShotPower;
    power.first = 42.0f;
    GameAction shoot;
    shoot.type = ActionType::Shoot;
    expect(runtime.dispatch(power).ok && runtime.dispatch(shoot).ok &&
        runtime.step(12).ok && recorder.finish(runtime, 1).ok,
        "replay fixture records an authority shot");
    std::ofstream file(output.path, std::ios::binary | std::ios::trunc);
    file << json::stringify(replay::serialize(recorder.document())) << '\n';
    expect(static_cast<bool>(file), "replay fixture is written");
    return output;
}

ReplayFixture makeOfficialRackTerminalReplay(const PhysicsProfile& profile)
{
    ReplayFixture output;
    output.path=std::filesystem::temp_directory_path()/
        "billiardgl-product-pointer-official-rack-v2.replay.json";
    output.executable="product-pointer-session-attestation-v1";
    const auto binding=replay::officialSingleRackReplayBinding(profile);
    output.content=binding.contentSha256;
    GameRuntime runtime;
    expect(runtime.replaceStateForScenario(runtime.state(),profile,nullptr,false,
            PhysicsBoundaryMode::ProductionTable).ok&&
        runtime.setRulesMode(RulesMode::ChineseEightBallV1).ok,
        "official match replay adopts the product physics/rules contract");
    rules::RerackFoundationParameters rackParameters;
    rackParameters.deterministicSeed=rules::kOfficialRackDefaultSeed;
    rackParameters.breakerPlayerId=0;
    const auto rack=runtime.stageOfficialRerack(rackParameters);
    expect(rack.ok&&runtime.commitRackFoundation(
            rack,"product.match.start.official-rack-v1").ok,
        "official match replay uses the same fixed rack as Start match");
    ProductReplayAuthenticator authenticator;
    runtime.configureNonphysicalGeneralFoulAuthority(&authenticator);
    replay::Recorder recorder;
    expect(recorder.begin(runtime,output.executable,output.content,91).ok,
        "official match replay begins from the trusted rack binding");
    const std::string firstShot="runtime.nonphysical_general_foul.shot."+
        std::to_string(runtime.rulesAuthorityRevision());
    auto firstProposal=replayRefereeRequest(runtime,
        rules::RefereeAuthorityOperation::Propose,"product.replay.first",
        "product.replay.first.p",
        rules::canonicalNonphysicalGeneralFoulProposal(
            rules::NonphysicalGeneralFoulKind::
                MobileDeviceDisturbanceGeneralFoul,0,firstShot));
    expect(runtime.submitNonphysicalGeneralFoulProposal(firstProposal).ok,
        "official match replay opens first mobile foul");
    auto firstFinal=replayRefereeRequest(runtime,
        rules::RefereeAuthorityOperation::FinalDeclare,"product.replay.first",
        "product.replay.first.f",rules::canonicalNonphysicalGeneralFoulFinal(
            rules::NonphysicalGeneralFoulKind::
                MobileDeviceDisturbanceGeneralFoul,0,
            "product.replay.first",firstShot));
    expect(runtime.submitNonphysicalGeneralFoulFinal(firstFinal).ok&&
        runtime.ballInHandPlacementOffer(),
        "official match replay commits first mobile foul");
    const auto offer=*runtime.ballInHandPlacementOffer();
    rules::BallInHandPlacementRequest placement;
    placement.rulesetId=rules::kBallInHandRulesetId;
    placement.capabilityId=rules::kFreeBallPlacementCapabilityId;
    placement.flowId=rules::kFullTableBallInHandFlowId;
    placement.decisionId=offer.decisionId;
    placement.expectedAuthorityStateRevision=offer.authorityStateRevision;
    placement.idempotencyKey="product.replay.placement";
    placement.actorPlayerId=offer.actorPlayerId;
    placement.actionId=rules::kConfirmFullTablePlacementActionId;
    placement.position={runtime.state().balls[0].position.x,
        runtime.state().balls[0].position.z};
    expect(runtime.submitBallInHandPlacement(placement).ok,
        "official match replay commits the BIH action");
    const std::string repeatShot="runtime.nonphysical_general_foul.shot."+
        std::to_string(runtime.rulesAuthorityRevision());
    auto repeatProposal=replayRefereeRequest(runtime,
        rules::RefereeAuthorityOperation::Propose,"product.replay.repeat",
        "product.replay.repeat.p",rules::canonicalNonphysicalGeneralFoulProposal(
            rules::NonphysicalGeneralFoulKind::MobileDeviceRepeatLossOfRack,
            0,repeatShot));
    expect(runtime.submitNonphysicalGeneralFoulProposal(repeatProposal).ok,
        "official match replay opens repeat mobile foul");
    auto repeatFinal=replayRefereeRequest(runtime,
        rules::RefereeAuthorityOperation::FinalDeclare,"product.replay.repeat",
        "product.replay.repeat.f",rules::canonicalNonphysicalGeneralFoulFinal(
            rules::NonphysicalGeneralFoulKind::MobileDeviceRepeatLossOfRack,
            0,"product.replay.repeat",repeatShot));
    expect(runtime.submitNonphysicalGeneralFoulFinal(repeatFinal).ok&&
        runtime.step(1).ok&&recorder.finish(runtime,0).ok&&
        recorder.saveAtomic(output.path.string()).ok,
        "official match replay atomically saves its rack terminal");
    return output;
}

void testSharedLayoutHitPolicy()
{
    GameRuntime runtime;
    ai::AiPlayerController ai;
    auto session = makeSession(runtime, ai, "layout");
    const ui::ShellViewport viewports[] = {
        {1280, 720, 1280, 720, 96.0f},
        {1920, 1080, 1920, 1080, 96.0f},
        {1280, 720, 2560, 1440, 192.0f},
    };
    for (const ui::ShellViewport& viewport : viewports) {
        session.setShellViewport(viewport);
        const ui::ShellScreenLayout layout = session.shellLayout();
        expect(layout.ok && !layout.elements.empty(),
            "1280p, 1080p, and Retina layouts are CPU-valid");
        const auto& row = layout.elements.front().rect;
        const double scaleX = static_cast<double>(viewport.physicalWidth) /
            viewport.logicalWidth;
        const double scaleY = static_cast<double>(viewport.physicalHeight) /
            viewport.logicalHeight;
        expect(ui::hitTestShippingScreenLayout(layout,
            row.x / scaleX, row.y / scaleY).id == layout.elements.front().id,
            "left/top boundary is inclusive");
        expect(!ui::hitTestShippingScreenLayout(layout,
            (row.x + row.width) / scaleX,
            (row.y + row.height * 0.5f) / scaleY).hit,
            "right boundary is exclusive and the inter-row gap is blank");
        expect(!ui::hitTestShippingScreenLayout(layout, 0.0, 0.0).hit,
            "safe-area margin is blank");
    }
}

void testShellNavigationAndPointerCapture()
{
    GameRuntime runtime;
    ai::AiPlayerController ai;
    auto session = makeSession(runtime, ai, "navigation");
    session.setShellViewport({1280, 720, 2560, 1440, 192.0f});

    const Point training = itemCenter(session, "training");
    expect(pointer(session, PlatformInputKind::PointerButtonDown,
        training, "LeftButton").handled, "press captures training");
    expect(pointer(session, PlatformInputKind::PointerMove, {0.0, 0.0}).handled,
        "move-out updates shell hover");
    expect(pointer(session, PlatformInputKind::PointerButtonUp,
        {0.0, 0.0}, "LeftButton").handled &&
        session.snapshot().shell.screen == ProductScreen::MainMenu,
        "press then release outside cancels activation");

    const Point single = itemCenter(session, "single_player");
    expect(!pointer(session, PlatformInputKind::PointerButtonDown,
        single, "RightButton").handled &&
        !pointer(session, PlatformInputKind::PointerButtonUp,
        single, "RightButton").handled &&
        session.snapshot().shell.screen == ProductScreen::MainMenu,
        "wrong button never activates a shell row");
    expect(click(session, "single_player").ok &&
        session.snapshot().shell.screen == ProductScreen::SinglePlayerSetup,
        "Main Menu pointer activation enters setup");
    expect(click(session, "expert").ok &&
        session.snapshot().shell.aiDifficulty == ai::Difficulty::Expert,
        "setup pointer activation selects an exact semantic row");
    expect(click(session, "start_match").ok &&
        session.snapshot().shell.screen == ProductScreen::Match,
        "setup pointer activation starts the authoritative match");

    expect(session.handlePlatformInput(key("Tab")).ok,
        "gameplay aim remains available after pointer shell navigation");
    PlatformInputEvent shoot;
    shoot.kind = PlatformInputKind::PointerButtonUp;
    shoot.control = "LeftButton";
    shoot.logicalX = single.x;
    shoot.logicalY = single.y;
    expect(session.handlePlatformInput(shoot).ok && runtime.state().ballsMoving,
        "Match mouse release follows gameplay shoot path, not shell hit testing");
}

void testRecordedPocketStorageVisibility(bool finishAimTransition)
{
    GameRuntime runtime;
    ai::AiPlayerController ai;
    auto session = makeSession(runtime, ai, finishAimTransition ?
        "captured-storage-steady-camera" : "captured-storage-transition-camera");
    session.setShellViewport({1920, 1080, 1920, 1080, 96.0f});
    expect(click(session, "training").ok, "open training for recorded shot");
    expect(click(session, session.snapshot().shell.items.front().id).ok &&
        session.training().snapshot().scenarioId == "launch_stop_shot_center_v1",
        "load the user's stop-shot scenario");
    GameAction orbit;
    orbit.type = ActionType::OrbitCamera;
    orbit.first = 0.65f;
    orbit.second = -0.12f;
    expect(runtime.dispatch(orbit).ok, "choose a non-default observation angle");
    auto exactCameraPreferences=runtime.cameraPresentationPreferences();
    exactCameraPreferences.impactShakeEnabled=false;
    runtime.setCameraPresentationPreferences(exactCameraPreferences);
    expect(!runtime.cameraPresentationPreferences().impactShakeEnabled,
        "exact manual-camera preservation fixture disables transient shake");
    expect(session.handlePlatformInput(key("Tab")).ok, "enter aim for recorded shot");
    if (finishAimTransition) expect(runtime.step(180).ok, "finish aim transition before shooting");
    GameAction aim;
    aim.type = ActionType::SetAimYaw;
    aim.first = -1.5772027969360352f;
    expect(runtime.dispatch(aim).ok, "apply recorded shot yaw");
    GameAction power;
    power.type = ActionType::SetShotPower;
    power.first = 200.0f;
    expect(runtime.dispatch(power).ok, "apply recorded shot power");
    PlatformInputEvent shot;
    shot.kind = PlatformInputKind::PointerButtonUp;
    shot.control = "LeftButton";
    const auto chosenView = runtime.cameraFrame().pose;
    const auto assertChosenView = [&]() {
        const auto& actual = runtime.cameraFrame().pose;
        const auto near = [](float a, float b) { return std::fabs(a - b) < 0.0001f; };
        expect(near(actual.eye.x, chosenView.eye.x) && near(actual.eye.y, chosenView.eye.y) &&
            near(actual.eye.z, chosenView.eye.z) && near(actual.target.x, chosenView.target.x) &&
            near(actual.target.y, chosenView.target.y) && near(actual.target.z, chosenView.target.z) &&
            near(actual.yawRadians, chosenView.yawRadians) &&
            near(actual.pitchRadians, chosenView.pitchRadians) &&
            near(actual.distanceCm, chosenView.distanceCm) &&
            near(actual.fieldOfViewDegrees, chosenView.fieldOfViewDegrees) &&
            !runtime.cameraFrame().transitioning,
            "shooting and settling preserve the complete chosen camera pose without automatic follow");
    };
    expect(session.handlePlatformInput(shot).ok && runtime.state().ballsMoving,
        "recorded shot uses the real training shot controller");
    assertChosenView();
    bool captured = false;
    for (int tick = 0; tick < 2400; ++tick) {
        const auto previous = runtime.state();
        expect(runtime.step(1).ok, "recorded shot physics advances successfully");
        session.consumeAuthoritySteps();
        assertChosenView();
        const auto beforeRender = gameStateSha256(runtime.state());
        const int score = session.training().snapshot().evaluation.score;
        for (double alpha : {0.0, 0.5, 1.0}) {
            const auto frame = buildPresentationSnapshot(previous, runtime.state(), alpha);
            for (int id = 1; id < kBallCount; ++id) {
                const auto& ball = runtime.state().balls[id];
                if (ball.pocketed && ball.pocketInteraction.phase == PocketInteractionPhase::Captured)
                    expect(!frame.balls[id].visible && !frame.balls[id].castsShadow,
                        "real captured storage positions never enter the visible ball set");
            }
        }
        expect(gameStateSha256(runtime.state()) == beforeRender &&
            session.training().snapshot().evaluation.score == score,
            "render sampling cannot mutate physics, pocket results or training score");
        captured |= runtime.state().balls[1].pocketed;
        if (!runtime.state().ballsMoving) break;
    }
    expect(captured && runtime.state().balls[1].pocketInteraction.pocketId == 1 &&
        !runtime.state().balls[1].outOfPlay && !runtime.state().ballsMoving,
        "recorded target-ball capture and settled authority remain intact");
    expect(runtime.step(120).ok, "advance after shot end");
    assertChosenView();
    expect(runtime.dispatch(orbit).ok &&
        std::fabs(runtime.cameraFrame().pose.yawRadians - chosenView.yawRadians) > 0.1f,
        "manual orbit remains available after a shot");
    expect(session.training().snapshot().state == training::TrainingProductState::AttemptActive &&
        session.training().snapshot().evaluation.feedback.find("outside target") != std::string::npos,
        "pocketed target with distant cue ball remains unscored with an explicit reason");
    if (finishAimTransition) {
        expect(click(session, "training_advanced").ok, "open details before active retry");
        ui::ShellViewport blocked{1024,768,2048,1536,192.0f};
        blocked.hasProtectedGameplayRect = true;
        blocked.protectedGameplayRect = {0,0,2048,1536};
        session.setShellViewport(blocked);
        expect(session.shellLayout().inspectorCollapsed, "active retry survives collapsed advanced panel");
    }
    expect(click(session, "retry_training").ok && !runtime.state().ballsMoving &&
        session.training().snapshot().attemptFacts.shotCount == 0 &&
        session.training().snapshot().evaluation.feedback.empty() &&
        !runtime.state().balls[1].pocketed &&
        std::fabs(runtime.state().balls[0].position.z - 58.0f) < 0.01f,
        "active stopped retry restores pre-shot balls and clears feedback without click-through");
}

void testTrainingReplayHelpAndSettings()
{
    GameRuntime runtime;
    ai::AiPlayerController ai;
    auto session = makeSession(runtime, ai, "screens");
    session.setShellViewport({1920, 1080, 1920, 1080, 96.0f});

    expect(click(session, "help").ok &&
        session.snapshot().shell.screen == ProductScreen::Help,
        "Help opens by pointer");
    const Point disabled = itemCenter(session, "help_aim");
    expect(pointer(session, PlatformInputKind::PointerMove, disabled).handled &&
        session.snapshot().hoveredShellItemId == "help_aim",
        "disabled Help row still has hover semantics");
    expect(!pointer(session, PlatformInputKind::PointerButtonDown,
        disabled, "LeftButton").handled,
        "disabled Help row cannot be captured");
    expect(click(session, "help_back").ok &&
        session.snapshot().shell.screen == ProductScreen::MainMenu,
        "enabled Help Back row returns to Main");

    const camera::Pose cameraBeforeSettings = runtime.cameraFrame().pose;
    expect(click(session, "settings").ok &&
        session.snapshot().shell.screen == ProductScreen::Settings,
        "Settings opens by pointer");
    const settings::AssistMode assist = session.settings().menu().preview().assist;
    expect(click(session, "assist_level").ok &&
        session.settings().menu().preview().assist != assist,
        "settings row click uses the same authoritative Activate/Toggle action");
    expect(session.handlePlatformInput(key("ArrowRight")).ok,
        "keyboard category navigation remains interoperable");
    const bool invert = session.settings().menu().preview().invertMouseY;
    expect(click(session, "invert_mouse_y").ok &&
        session.settings().menu().preview().invertMouseY != invert,
        "settings toggle with shared row geometry updates preview");
    for (int index = 0; index < 3; ++index)
        expect(session.handlePlatformInput(key("ArrowRight")).ok,
            "pointer settings fixture reaches Display category");
    expect(click(session, "field_of_view").ok &&
        std::fabs(runtime.cameraFrame().pose.fieldOfViewDegrees -
            cameraBeforeSettings.fieldOfViewDegrees - 1.0f) < 1.0e-5f,
        "pointer activation previews the relative FOV adjustment immediately");
    expect(session.handlePlatformInput(key("Escape")).ok &&
        session.snapshot().shell.screen == ProductScreen::MainMenu &&
        std::fabs(runtime.cameraFrame().pose.fieldOfViewDegrees -
            cameraBeforeSettings.fieldOfViewDegrees) < 1.0e-5f &&
        std::fabs(runtime.cameraFrame().pose.distanceCm -
            cameraBeforeSettings.distanceCm) < 1.0e-5f,
        "Settings cancel atomically restores the camera preview");

    expect(click(session, "training").ok &&
        session.snapshot().shell.screen == ProductScreen::TrainingCatalog,
        "Training catalog opens by pointer");
    const std::string scenario = session.snapshot().shell.items.front().id;
    expect(click(session, scenario).ok &&
        session.snapshot().shell.screen == ProductScreen::TrainingSession,
        "training semantic row starts a validated scenario");
    const std::string observeState = gameStateSha256(runtime.state());
    PlatformInputEvent observeRelease;
    observeRelease.kind = PlatformInputKind::PointerButtonUp;
    observeRelease.control = "LeftButton";
    const ModernProductEventResult ignoredObserveRelease =
        session.handlePlatformInput(observeRelease);
    expect(ignoredObserveRelease.ok && ignoredObserveRelease.handled &&
        ignoredObserveRelease.errorCode.empty() &&
        session.snapshot().shell.errorCode.empty() &&
        gameStateSha256(runtime.state()) == observeState,
        "TrainingSession consumes an observe-mode blank left release without an input error or gameplay mutation");
    PlatformInputEvent move;
    move.kind = PlatformInputKind::PointerMove;
    move.deltaX = 2.0;
    (void)session.handlePlatformInput(move);
    expect(session.snapshot().hoveredShellItemId.empty(),
        "TrainingSession mouse move does not enter shell hover routing");
    expect(session.handlePlatformInput(key("Tab")).ok,
        "TrainingSession retains gameplay aim binding");
    PlatformInputEvent trainingShot;
    trainingShot.kind = PlatformInputKind::PointerButtonUp;
    trainingShot.control = "LeftButton";
    const ModernProductEventResult aimedRelease =
        session.handlePlatformInput(trainingShot);
    expect(aimedRelease.ok && aimedRelease.handled &&
        runtime.state().ballsMoving,
        "TrainingSession left release still shoots through the gameplay input controller while Aim is active");
    expect(session.handlePlatformInput(key("Escape")).ok &&
        session.handlePlatformInput(key("Escape")).ok,
        "TrainingSession returns through catalog to Main");

    expect(click(session, "replays").ok &&
        session.snapshot().shell.screen == ProductScreen::ReplayBrowser,
        "Replay Browser opens by pointer");
    const ModernProductEventResult choose = click(session, "choose_replay");
    expect(choose.ok &&
        choose.appRequest == ProductShellAppRequest::ChooseReplayFile,
        "Replay Browser click emits the existing native chooser request");
    const ReplayFixture replay = makeReplay(runtime.physicsProfile(),
        runtime.boundaryMode());
    expect(session.shell().loadReplayPath(replay.path.string(),
        replay.executable, replay.content).ok &&
        session.snapshot().shell.screen == ProductScreen::ReplayPlayback,
        "validated replay enters playback");
    const std::uint64_t pausedReplayTick =
        session.snapshot().shell.replay.tick;
    const Point3 pausedReplayCuePosition =
        session.presentationRuntime().state().balls[0].position;
    const Point3 liveCuePosition = runtime.state().balls[0].position;
    expect(session.advancePresentedFrame().ok &&
        session.snapshot().shell.replay.tick == pausedReplayTick,
        "a paused replay remains fixed when the product presents a frame");
    expect(session.handlePlatformInput(key("Space")).ok &&
        session.handlePlatformInput(key("+")).ok &&
        session.snapshot().shell.replay.playbackRate == 2.0 &&
        session.advancePresentedFrame().ok &&
        session.snapshot().shell.replay.tick == pausedReplayTick + 2,
        "the product presentation seam advances an unpaused replay at its selected rate");
    const Point3 advancedReplayCuePosition =
        session.presentationRuntime().state().balls[0].position;
    expect(advancedReplayCuePosition.x != pausedReplayCuePosition.x ||
        advancedReplayCuePosition.y != pausedReplayCuePosition.y ||
        advancedReplayCuePosition.z != pausedReplayCuePosition.z,
        "the presented table follows the isolated replay ball motion");
    expect(runtime.state().balls[0].position.x == liveCuePosition.x &&
        runtime.state().balls[0].position.y == liveCuePosition.y &&
        runtime.state().balls[0].position.z == liveCuePosition.z,
        "rendering replay motion leaves the live authority unchanged");
    expect(session.handlePlatformInput(key("Escape")).ok &&
        session.snapshot().shell.screen == ProductScreen::ReplayBrowser,
        "ReplayPlayback can return to its browser");
    const std::uint64_t hiddenReplayTick =
        session.snapshot().shell.replay.tick;
    const ModernProductEventResult hiddenFrame =
        session.advancePresentedFrame();
    expect(hiddenFrame.ok && !hiddenFrame.handled &&
        session.snapshot().shell.replay.tick == hiddenReplayTick &&
        &session.presentationRuntime() == &runtime,
        "presented frames never advance replay authority outside ReplayPlayback");
    expect(session.shell().loadReplayPath(replay.path.string(),
        replay.executable, replay.content).ok,
        "the replay can be reopened after the hidden-frame control");
    const camera::Frame freeReplayCamera =
        session.presentationCameraFrame();
    expect(session.handlePlatformInput(key("C")).ok,
        "replay camera toggle is accepted by the product input route");
    const camera::Frame broadcastReplayCamera =
        session.presentationCameraFrame();
    expect(freeReplayCamera.state == camera::SemanticState::ReplayFree &&
        broadcastReplayCamera.state ==
            camera::SemanticState::ReplayBroadcast &&
        broadcastReplayCamera.preset == camera::Preset::Broadcast &&
        (broadcastReplayCamera.pose.eye.x != freeReplayCamera.pose.eye.x ||
            broadcastReplayCamera.pose.eye.y != freeReplayCamera.pose.eye.y ||
            broadcastReplayCamera.pose.eye.z != freeReplayCamera.pose.eye.z),
        "Free and Broadcast select visibly distinct replay camera presentations");

    const double replayFixedStep =
        session.snapshot().shell.replay.fixedStepSeconds;
    const auto expectElapsedSchedule = [&](int frameCount,
                                            double frameElapsed,
                                            double rate,
                                            const std::string& label) {
        expect(session.shell().loadReplayPath(replay.path.string(),
            replay.executable, replay.content).ok,
            label + " reloads the isolated replay");
        if (rate != 1.0) {
            expect(session.handlePlatformInput(key("-")).ok &&
                session.handlePlatformInput(key("-")).ok,
                label + " selects quarter speed");
        }
        expect(session.handlePlatformInput(key("Space")).ok,
            label + " starts playback");
        for (int frame = 0; frame < frameCount; ++frame)
            expect(session.advancePresentedFrame(frameElapsed).ok,
                label + " presented frame advances");
        expect(session.snapshot().shell.replay.tick == 12 &&
            session.snapshot().shell.replay.finished,
            label + " schedules exactly twelve 120 Hz ticks");
    };
    expectElapsedSchedule(3, replayFixedStep * 4.0, 1.0,
        "30 Hz at 1x");
    expectElapsedSchedule(6, replayFixedStep * 2.0, 1.0,
        "60 Hz at 1x");
    expectElapsedSchedule(12, replayFixedStep, 1.0,
        "120 Hz at 1x");
    expectElapsedSchedule(48, replayFixedStep, 0.25,
        "120 Hz at quarter speed");
    expect(session.shell().loadReplayPath(replay.path.string(),
        replay.executable, replay.content).ok &&
        session.advancePresentedFrame(10.0).ok &&
        session.snapshot().shell.replay.tick == 0 &&
        session.handlePlatformInput(key("Space")).ok &&
        session.advancePresentedFrame(replayFixedStep).ok &&
        session.snapshot().shell.replay.tick == 1,
        "paused wall time is discarded instead of creating a catch-up burst");
    PlatformInputEvent replayRelease;
    replayRelease.kind = PlatformInputKind::PointerButtonUp;
    replayRelease.control = "LeftButton";
    const ModernProductEventResult replayInput =
        session.handlePlatformInput(replayRelease);
    expect(replayInput.handled && replayInput.ok &&
        replayInput.errorCode.empty() &&
        session.snapshot().shell.screen == ProductScreen::ReplayPlayback,
        "replay camera release is consumed harmlessly without an unbound error");
    for (const auto viewport : {ui::ShellViewport{1280,720,1280,720,96.0f},
            ui::ShellViewport{1024,768,2048,1536,192.0f}}) {
        session.setShellViewport(viewport);
        expect(session.shell().loadReplayPath(replay.path.string(), replay.executable, replay.content).ok,
            "mouse playback fixture reloads paused");
        const auto play = itemCenter(session, "replay_pause");
        expect(pointer(session, PlatformInputKind::PointerButtonDown, play, "LeftButton").handled,
            "Play captures mouse down");
        session.setShellViewport(viewport);
        const auto started = pointer(session, PlatformInputKind::PointerButtonUp, play, "LeftButton");
        expect(started.ok && started.action == "replay_pause" && !session.snapshot().shell.replay.paused &&
            session.advancePresentedFrame(replayFixedStep * 3).ok && session.snapshot().shell.replay.tick == 3,
            "actual Play click advances replay ticks");
        expect(click(session, "replay_pause").ok && session.snapshot().shell.replay.paused &&
            session.advancePresentedFrame(0.1).ok && session.snapshot().shell.replay.tick == 3,
            "actual Pause click stops replay ticks");
        expect(click(session, "replay_step_tick").ok && session.snapshot().shell.replay.tick == 4,
            "actual step button advances exactly one tick");
        const auto point = itemCenter(session, "replay_pause");
        expect(pointer(session, PlatformInputKind::PointerButtonDown, point, "LeftButton").handled,
            "cancelled replay press captures");
        pointer(session, PlatformInputKind::PointerButtonUp, {0,0}, "LeftButton");
        expect(session.snapshot().shell.replay.paused, "releasing outside Play does not activate it");
    }
    const std::string authorityHash = gameStateSha256(runtime.state());
    const std::uint64_t authorityTick = runtime.tick();
    const std::filesystem::path mismatchPath =
        std::filesystem::temp_directory_path() /
        "billiardgl-product-pointer-final-mismatch.replay.json";
    {
        std::ifstream input(replay.path, std::ios::binary);
        const std::string bytes((std::istreambuf_iterator<char>(input)),
            std::istreambuf_iterator<char>());
        auto decoded = json::parse(bytes);
        expect(decoded.ok, "product replay decodes for endpoint mismatch control");
        decoded.value["final_summary"]["state_sha256"] =
            json::Value(std::string(64, '0'));
        std::ofstream output(mismatchPath,
            std::ios::binary | std::ios::trunc);
        output << json::stringify(decoded.value) << '\n';
        expect(static_cast<bool>(output),
            "endpoint mismatch product replay writes");
    }
    expect(session.shell().loadReplayPath(mismatchPath.string(),
        replay.executable, replay.content).ok &&
        session.handlePlatformInput(key("Space")).ok,
        "well-formed endpoint mismatch enters active product playback");
    ModernProductEventResult endpoint;
    for (int frame = 0; frame < 32 && endpoint.ok; ++frame)
        endpoint = session.advancePresentedFrame();
    expect(!endpoint.ok && endpoint.handled &&
        endpoint.errorCode == "replay_final_hash_mismatch" &&
        session.snapshot().shell.screen == ProductScreen::ReplayPlayback &&
        session.snapshot().shell.errorCode == "replay_final_hash_mismatch" &&
        session.snapshot().lastError == "replay_final_hash_mismatch" &&
        gameStateSha256(runtime.state()) == authorityHash &&
        runtime.tick() == authorityTick,
        "product-frame playback exposes terminal hash drift without mutating live authority");
    const ModernProductEventResult repeatedEndpoint =
        session.advancePresentedFrame();
    expect(!repeatedEndpoint.ok &&
        repeatedEndpoint.errorCode == "replay_final_hash_mismatch" &&
        session.snapshot().shell.errorCode == "replay_final_hash_mismatch",
        "terminal playback failure remains stable on later presented frames");
    std::error_code ignored;
    std::filesystem::remove(mismatchPath, ignored);
    std::filesystem::remove(replay.path, ignored);
}

void testTrajectoryInsetFrameLifecycle()
{
    {
        ui::TrajectoryInsetShellRequest request;
        request.profileId = "2560x1440_ui100";
        request.physicalWidth = 2560;
        request.physicalHeight = 1440;
        request.uiScale = 1.0f;
        request.exclusions = {
            {"player status HUD", {48,100,820,156}, true,
                {{48,276,820,360}, {48,648,820,360}, {2164,48,348,68}}},
            {"shot power and tip controls", {1034,1204,1360,188}, true},
            {"rules decision panel or modal", {}, false},
            {"caption and accessibility live-region band", {}, false},
            {"safe-area edges and product footer", {48,1264,820,76}, true}};
        const auto layout = ui::buildTrajectoryInsetShellLayout(request);
        expect(layout.ok && layout.insetVisible &&
            layout.panelRect.x == 2080 && layout.panelRect.y == 908 &&
            layout.panelRect.width == 448 && layout.panelRect.height == 288 &&
            ui::auditTrajectoryInsetShellLayout(request, layout).ok,
            "native v7 occupied rectangles place full-size inset above Training HUD");
    }
    for (float scale : {0.85f, 1.0f, 1.1f, 1.35f}) {
        ui::TrajectoryInsetShellRequest request;
        request.profileId = "1536x960_ui" +
            std::to_string(static_cast<int>(std::lround(scale * 100)));
        request.physicalWidth = 1536;
        request.physicalHeight = 960;
        request.uiScale = scale;
        request.insetControlOn = true;
        request.exclusions = {
            {"player status HUD", {}, false},
            {"shot power and tip controls", {}, false},
            {"rules decision panel or modal", {}, false},
            {"caption and accessibility live-region band", {}, false},
            {"safe-area edges and product footer", {}, false}};
        const auto layout = ui::buildTrajectoryInsetShellLayout(request);
        expect(layout.ok && layout.insetVisible &&
            ui::auditTrajectoryInsetShellLayout(request, layout).ok &&
            std::abs(layout.plotCutoutRect.width /
                layout.plotCutoutRect.height - 2.0f) < 1e-5f &&
            std::abs(layout.legendTextPx - 16.0f * scale) < 1e-4f,
            "actual drawable and scale preserve plot and typography contracts");
        request.exclusions[0].active = true;
        request.exclusions[0].rect = {0, 0, 300, 900};
        request.exclusions[0].additionalRects = {{1300, 0, 200, 80}};
        const auto separated = ui::buildTrajectoryInsetShellLayout(request);
        expect(separated.ok && separated.insetVisible &&
            ui::auditTrajectoryInsetShellLayout(request, separated).ok,
            "separated panels cannot manufacture a cross-screen occlusion box");
        request.exclusions[1].active = true;
        request.exclusions[1].rect = {0, 0, 1536, 960};
        const auto blocked = ui::buildTrajectoryInsetShellLayout(request);
        expect(blocked.ok && !blocked.insetVisible &&
            blocked.suppressionReason == "inset_layout_unavailable",
            "real occupied UI keeps explicit suppression instead of empty GPU chrome");
    }
    {
        ui::TrajectoryInsetShellRequest request;
        request.profileId = "1280x720_ui100";
        request.physicalWidth = 1280;
        request.physicalHeight = 720;
        request.exclusions = {
            {"player status HUD", {}, false},
            {"shot power and tip controls", {222.1f,590,835.8f,94}, true},
            {"rules decision panel or modal", {}, false},
            {"caption and accessibility live-region band", {}, false},
            {"safe-area edges and product footer", {}, false}};
        const auto reference = ui::buildTrajectoryInsetShellLayout(request);
        expect(reference.panelRect.y == 392 && !reference.insetVisible,
            "frozen reference geometry remains unchanged under true occupancy");
        request.profileId = "product_1280x720_ui100";
        const auto product = ui::buildTrajectoryInsetShellLayout(request);
        expect(product.insetVisible && product.panelRect.y == 302 &&
            product.panelRect.width == reference.panelRect.width &&
            product.plotCutoutRect.width == reference.plotCutoutRect.width &&
            product.plotCutoutRect.height == reference.plotCutoutRect.height &&
            product.panelRect.height == 280,
            "identified product derivative reserves eight pixels above real HUD");
    }
    GameRuntime runtime;
    ai::AiPlayerController ai;
    auto session = makeSession(runtime, ai, "inset-frame");
    session.setShellViewport({1280, 720, 1280, 720, 96.0f});
    session.prepareTrajectoryInsetCurrentFrame(4);
    expect(!session.snapshot().trajectoryInsetVisibleCurrentContext,
        "MainMenu cannot implicitly enable an inset");
    const auto& card = session.training().scenarios().front();
    expect(session.shell().startTrainingScenario(card.id, "inset-test", 1).ok,
        "inset fixture starts real training");
    expect(session.training().refreshPreview().ok, "inset uses a real preview");
    session.shell().setInspectorExpanded(false);
    const auto keyboardRecoveryHash = gameStateSha256(runtime.state());
    expect(session.handlePlatformInput(key("Enter")).action == "inspector_expand" &&
        !session.shellLayout().inspectorCollapsed &&
        gameStateSha256(runtime.state()) == keyboardRecoveryHash,
        "collapsed Show panel has a real Enter recovery without hidden action or shot");
    const auto before = runtime.rulesAuthorityStateHash();
    session.prepareTrajectoryInsetCurrentFrame(4);
    const auto first = session.snapshot();
    expect(first.trajectoryInsetVisibleCurrentContext.has_value(),
        "real preview creates explicit current-frame inset request");
    const auto context = *first.trajectoryInsetVisibleCurrentContext;
    expect(context.present && context.samples == 4 &&
        context.sourceContentSha256.size() == 64 && context.cp1GenerationId ==
            "v19_generation_" + std::to_string(context.frameSerial) && context.frameSerial > 0 &&
        !first.shell.worldTrajectoryOverlayEnabled,
        "inset request binds source digest and frame without world fallback");
    expect(session.snapshot().trajectoryInsetVisibleCurrentContext->frameSerial ==
        context.frameSerial, "paired snapshots never advance frame identity");
    session.prepareTrajectoryInsetCurrentFrame(4);
    expect(session.snapshot().trajectoryInsetVisibleCurrentContext->frameSerial >
        context.frameSerial &&
        session.snapshot().trajectoryInsetVisibleCurrentContext->sourceContentSha256 ==
            context.sourceContentSha256,
        "new frame serial does not invent a new source generation");
    session.setShellViewport({1280, 720, 1920, 1080, 144.0f});
    expect(!session.snapshot().trajectoryInsetVisibleCurrentContext,
        "resize invalidates old prepared context");
    const auto clickInsetMenu = [&]() {
        if (session.shellLayout().inspectorCollapsed) togglePanel(session);
        for (unsigned step = 0; step < 100; ++step) {
            const auto layout = session.shellLayout();
            if (std::any_of(layout.elements.begin(), layout.elements.end(),
                    [](const auto& element) {
                        return element.id == "trajectory_inset_toggle";
                    })) {
                expect(click(session, "trajectory_inset_toggle").ok,
                    "visible inset menu item responds to real pointer events");
                return;
            }
            PlatformInputEvent scroll;
            scroll.kind = PlatformInputKind::Scroll;
            scroll.logicalX = (layout.bodyRect.x + layout.bodyRect.width * 0.5) *
                layout.viewport.logicalWidth / layout.viewport.physicalWidth;
            scroll.logicalY = (layout.bodyRect.y + layout.bodyRect.height * 0.5) *
                layout.viewport.logicalHeight / layout.viewport.physicalHeight;
            scroll.scrollY = -1;
            expect(session.handlePlatformInput(scroll).handled,
                "pointer scroll exposes the inset control");
        }
        expect(false, "inset toggle must remain reachable in visible panel");
    };
    clickInsetMenu();
    session.prepareTrajectoryInsetCurrentFrame(4);
    expect(!session.snapshot().shell.trajectoryInsetEnabled &&
        !session.snapshot().trajectoryInsetVisibleCurrentContext,
        "toggle off cannot retain old inset frame");
    if (!session.shellLayout().inspectorCollapsed) togglePanel(session);
    clickInsetMenu();
    session.prepareTrajectoryInsetCurrentFrame(4);
    expect(session.snapshot().trajectoryInsetVisibleCurrentContext.has_value(),
        "reopened inset prepares fresh current source");
    expect(runtime.rulesAuthorityStateHash() == before,
        "inset presentation does not mutate rules authority");
    expect(session.handlePlatformInput(key("Escape")).ok,
        "leave training using normal input");
    session.prepareTrajectoryInsetCurrentFrame(4);
    expect(!session.snapshot().trajectoryInsetVisibleCurrentContext,
        "leaving accepted screen invalidates request");
}

void testReplayKeyboardFocusScroll()
{
    GameRuntime runtime;
    ai::AiPlayerController ai;
    auto session = makeSession(runtime, ai, "replay-focus-scroll");
    const ReplayFixture replay = makeReplay(runtime.physicsProfile(),runtime.boundaryMode());
    expect(session.shell().loadReplayPath(replay.path.string(),replay.executable,replay.content).ok,
        "focus fixture loads real replay");
    session.settings().openMenu();
    for (int i=0;i<5 && session.settings().menu().category()!=settings::SettingsCategory::Accessibility;++i)
        session.settings().handleMenu(settings::MenuCommand::CategoryNext);
    expect(session.settings().menu().focusItem("ui_scale").ok,"focus actual scale control");
    for(int i=0;i<10;++i) session.settings().handleMenu(settings::MenuCommand::Increase);
    expect(session.settings().handleMenu(settings::MenuCommand::Apply).ok,"apply UI135");
    session.setShellViewport({1024,768,1024,768,96});
    session.shell().setInspectorExpanded(false);
    expect(session.handlePlatformInput(key("Enter")).action=="inspector_expand",
        "Enter opens the actual collapsed Replay panel");
    const auto focusedVisible = [&]() {
        const auto layout=session.shellLayout();
        return std::any_of(layout.elements.begin(),layout.elements.end(),
            [](const auto& item){return item.focused && item.enabled;});
    };
    expect(focusedVisible() && session.snapshot().shell.replay.paused,
        "default Play keyboard focus is on a visible row without playing");
    expect(session.handlePlatformInput(key("ArrowDown")).ok && focusedVisible(),
        "keyboard focus advance scrolls to a visible action");
    auto layout=session.shellLayout();
    PlatformInputEvent scroll;
    scroll.kind=PlatformInputKind::Scroll;
    scroll.logicalX=layout.bodyRect.x+layout.bodyRect.width/2;
    scroll.logicalY=layout.bodyRect.y+layout.bodyRect.height/2;
    scroll.scrollY=1;
    for(int i=0;i<20;++i) session.handlePlatformInput(scroll);
    expect(session.shellLayout().firstVisibleRow==0,
        "manual wheel can inspect earlier rows without focus snapping back");
    expect(session.handlePlatformInput(key("ArrowDown")).ok && focusedVisible(),
        "subsequent keyboard navigation restores visible focus");
    std::error_code ignored;
    std::filesystem::remove(replay.path,ignored);
}

void testInjectedTrainingReplayFileCallbacks()
{
    const std::filesystem::path replayPath =
        std::filesystem::temp_directory_path() /
        "billiardgl-product-session-dialog.replay.json";
    std::error_code ignored;
    std::filesystem::remove(replayPath, ignored);
    FakeFileDialogAdapter dialogs;
    dialogs.save = {true, replayPath.string()};
    dialogs.choose = {true, replayPath.string()};
    GameRuntime runtime;
    ai::AiPlayerController ai;
    auto session = makeSession(runtime, ai, "file-dialog", true, &dialogs);
    expect(dialogs.chooseCalls == 0 && dialogs.saveCalls == 0,
        "headless fake file adapter is inert until an explicit control call");
    const auto& card = session.training().scenarios().front();
    expect(session.shell().startTrainingScenario(card.id,
        "legacy-shell-version-is-overridden", 999).ok,
        "injected session attestation starts a card-seeded shell attempt");
    training::AttemptFacts failed;
    failed.state = runtime.state();
    failed.numericalFailure = true;
    expect(session.training().finishAttempt(failed).ok &&
        session.training().snapshot().replayReady,
        "terminal attempt prepares a saveable replay");
    session.setShellViewport({1280, 720, 1280, 720, 96.0f});
    const auto saved = click(session, "save_training_replay");
    expect(saved.ok && saved.handled && dialogs.saveCalls == 1 &&
        dialogs.suggestedFilename == card.id + ".replay.json" &&
        session.snapshot().lastSavedReplayPath == replayPath.string() &&
        std::filesystem::exists(replayPath),
        "save control invokes only the injected callback and persists Replay v1");
    expect(session.handlePlatformInput(key("Escape")).ok &&
        session.snapshot().shell.screen == ProductScreen::TrainingCatalog &&
        session.training().snapshot().state ==
            training::TrainingProductState::PackReady &&
        session.shell().openReplayBrowser().ok,
        "leaving training abandons the attempt before opening the replay browser");
    GameRuntime coldRuntime;
    ai::AiPlayerController coldAi;
    auto coldSession = makeSession(
        coldRuntime, coldAi, "file-dialog-cold", true, &dialogs);
    expect(coldSession.training().snapshot().state ==
            training::TrainingProductState::PackReady &&
        coldSession.snapshot().lastSavedReplayPath.empty() &&
        coldSession.shell().openReplayBrowser().ok,
        "cold product session has only the validated training registry");
    const auto opened = coldSession.chooseReplayFile();
    expect(opened.ok && opened.handled && dialogs.chooseCalls == 1 &&
        coldSession.snapshot().shell.screen == ProductScreen::ReplayPlayback &&
        !coldSession.snapshot().chooseReplayRequested,
        "cold-start chooser binds a persisted replay through the trusted scenario registry: " +
            opened.errorCode);

    ReplayFixture official=makeOfficialRackTerminalReplay(
        coldRuntime.physicsProfile());
    expect(coldSession.shell().openReplayBrowser().ok,
        "product returns to browser before selecting an official rack replay");
    dialogs.choose={true,official.path.string()};
    const auto officialOpened=coldSession.chooseReplayFile();
    expect(officialOpened.ok&&officialOpened.handled&&dialogs.chooseCalls==2&&
        coldSession.snapshot().shell.screen==ProductScreen::ReplayPlayback&&
        coldSession.snapshot().shell.replay.documentFormatVersion==2&&
        !coldSession.snapshot().shell.replay.rulesEffectVerified,
        "trusted official single-rack v2 enters product playback unverified");
    expect(coldSession.shell().dispatchProductInput(
            {"keyboard",".","press",0.0,0.0}).ok,
        "official rack replay advances through the product transport");
    const auto rackTerminal=coldSession.snapshot().shell;
    const auto terminalItem=std::find_if(rackTerminal.items.begin(),
        rackTerminal.items.end(),[](const auto& item){
            return item.id=="replay_rack_terminal";
        });
    expect(rackTerminal.replay.finished&&
        rackTerminal.replay.rulesEffectVerified&&
        rackTerminal.replay.rackTerminalActive&&
        !coldSession.presentationRuntime().state().gameOver&&
        terminalItem!=rackTerminal.items.end()&&
        terminalItem->label=="Rack ended"&&
        terminalItem->detail.find("match remains distinct")!=std::string::npos,
        "product presents verified rack loss without declaring match completion");
    const auto retainedReplay=rackTerminal.replay;

    const std::filesystem::path unknownPath =
        std::filesystem::temp_directory_path() /
        "billiardgl-product-session-dialog-unknown.replay.json";
    {
        std::ifstream input(replayPath, std::ios::binary);
        const std::string bytes((std::istreambuf_iterator<char>(input)),
            std::istreambuf_iterator<char>());
        const auto decoded = json::parse(bytes);
        expect(decoded.ok, "saved replay decodes for registry negative control");
        json::Value unknown = decoded.value;
        unknown["content_sha256"] = json::Value(
            sha256Hex("not-in-the-validated-training-registry"));
        std::ofstream output(unknownPath, std::ios::binary | std::ios::trunc);
        output << json::stringify(unknown) << '\n';
        expect(static_cast<bool>(output), "unknown-content replay fixture writes");
    }
    expect(coldSession.shell().openReplayBrowser().ok,
        "cold session returns to replay browser for untrusted control");
    dialogs.choose = {true, unknownPath.string()};
    const auto untrusted = coldSession.chooseReplayFile();
    expect(!untrusted.ok && untrusted.handled && dialogs.chooseCalls == 3 &&
        untrusted.errorCode == "product_replay_content_untrusted" &&
        coldSession.snapshot().shell.screen == ProductScreen::ReplayBrowser&&
        coldSession.snapshot().shell.replay.documentFormatVersion==
            retainedReplay.documentFormatVersion&&
        coldSession.snapshot().shell.replay.rackTerminalDecisionId==
            retainedReplay.rackTerminalDecisionId,
        "unknown embedded content is rejected without replacing current replay");

    dialogs.choose = {};
    expect(coldSession.chooseReplayFile().ok && dialogs.chooseCalls == 4,
        "cancelled fake chooser is a successful no-op without native UI");
    std::filesystem::remove(official.path, ignored);
    std::filesystem::remove(unknownPath, ignored);
    std::filesystem::remove(replayPath, ignored);
}

}  // namespace

void togglePanel(ModernProductSession& session)
{
    const auto layout = session.shellLayout();
    const auto rect = layout.inspectorToggleRect;
    Point point{(rect.x + rect.width / 2) * layout.viewport.logicalWidth / layout.viewport.physicalWidth,
        (rect.y + rect.height / 2) * layout.viewport.logicalHeight / layout.viewport.physicalHeight};
    expect(pointer(session, PlatformInputKind::PointerButtonDown, point, "LeftButton").handled,
        "panel toggle captures native pointer");
    session.setShellViewport(session.snapshot().shellViewport);
    const auto clicked = pointer(session, PlatformInputKind::PointerButtonUp, point, "LeftButton");
    expect(clicked.ok && (clicked.action == "inspector_expand" || clicked.action == "inspector_collapse"),
        "panel toggle activates without falling through to gameplay");
}

void testManualPanelToggle()
{
    GameRuntime runtime;
    ai::AiPlayerController ai;
    auto session = makeSession(runtime, ai, "panel-toggle");
    expect(session.shell().startTrainingScenario(session.training().scenarios().front().id,
        "panel-toggle", 0).ok, "panel toggle enters training");
    for (int screen = 0; screen < 2; ++screen) {
        ui::ShellViewport viewport{1024,768,2048,1536,192.0f};
        viewport.hasProtectedGameplayRect = true;
        viewport.protectedGameplayRect = {0,0,2048,1536};
        session.setShellViewport(viewport);
        expect(session.shellLayout().inspectorCollapsed, "protected corridor initially hides panel");
        const auto hash = gameStateSha256(runtime.state());
        togglePanel(session);
        expect(!session.shellLayout().inspectorCollapsed && session.snapshot().shell.inspectorMode == 1,
            "manual Show panel opens despite projected corridor conflict");
        session.setShellViewport(viewport);
        expect(!session.shellLayout().inspectorCollapsed, "manual expansion survives the next frame");
        togglePanel(session);
        viewport.hasProtectedGameplayRect = false;
        session.setShellViewport(viewport);
        expect(session.shellLayout().inspectorCollapsed && session.snapshot().shell.inspectorMode == 2,
            "manual Hide panel stays collapsed even when automatic layout would fit");
        togglePanel(session);
        expect(!session.shellLayout().inspectorCollapsed && gameStateSha256(runtime.state()) == hash,
            "toggle restores panel without moving balls or camera");
        if (screen == 0) {
            const auto replay = makeReplay(runtime.physicsProfile(), runtime.boundaryMode());
            expect(session.shell().loadReplayPath(replay.path.string(), replay.executable, replay.content).ok,
                "repeat toggle regression in replay");
        }
    }
}

void testCollapsedTerminalRetry()
{
    GameRuntime runtime;
    ai::AiPlayerController ai;
    auto session = makeSession(runtime, ai, "terminal-retry");
    expect(session.shell().startTrainingScenario(session.training().scenarios().front().id,
        "terminal-retry", 0).ok, "terminal retry starts training");
    training::AttemptFacts failed;
    failed.state = runtime.state();
    failed.numericalFailure = true;
    expect(session.training().finishAttempt(failed).ok, "terminal retry fixture ends");
    ui::ShellViewport viewport{1024, 768, 2048, 1536, 192.0f};
    viewport.hasProtectedGameplayRect = true;
    viewport.protectedGameplayRect = {0, 0, 2048, 1536};
    session.setShellViewport(viewport);
    expect(session.shellLayout().inspectorCollapsed, "terminal fixture collapses");
    expect(session.handlePlatformInput(key("Tab")).ok, "terminal aim remains observable");
    for (int index = 0; index < 4; ++index) {
        const auto result = pointer(session, PlatformInputKind::PointerButtonUp,
            {650, 450}, "LeftButton");
        expect(result.ok && !runtime.state().ballsMoving && session.snapshot().shell.errorText.empty(),
            "terminal clicks do not create errors or shots");
    }
    GameAction orbit;
    orbit.type = ActionType::OrbitCamera;
    orbit.first = 0.1f;
    expect(runtime.dispatch(orbit).ok, "terminal view can still orbit");
    const auto point = itemCenter(session, "retry_training");
    expect(pointer(session, PlatformInputKind::PointerButtonDown, point, "LeftButton").handled,
        "visible collapsed retry captures pointer");
    session.setShellViewport(viewport); // Native loop refreshes between down/up.
    const auto retried = pointer(session, PlatformInputKind::PointerButtonUp, point, "LeftButton");
    expect(retried.ok && retried.action == "retry_training" &&
        session.training().snapshot().state == training::TrainingProductState::AttemptActive &&
        !runtime.state().ballsMoving, "collapsed retry reactivates without click-through shooting");
    if (runtime.state().aim.mode != AimMode::Aim)
        expect(session.handlePlatformInput(key("Tab")).ok, "retry can aim again");
    GameAction power;
    power.type = ActionType::SetShotPower;
    power.first = 100.0f;
    expect(runtime.dispatch(power).ok, "retry sets shot power");
    const auto nextShot = pointer(session, PlatformInputKind::PointerButtonUp, {650,450}, "LeftButton");
    expect(nextShot.ok && nextShot.action == "shoot" && runtime.state().ballsMoving &&
        session.training().snapshot().shotInProgress, "retry permits a fresh recorded shot");
}

void testProductionPanelStability()
{
    GameRuntime runtime;
    ai::AiPlayerController ai;
    auto session = makeSession(runtime, ai, "production-panel-stability");
    session.shell().setManualInspectorPolicy(true);
    expect(session.shell().startTrainingScenario(session.training().scenarios().front().id,
        "production-panel-stability", 0).ok, "manual production training starts");
    ui::ShellViewport viewport{1024,768,2048,1536,192.0f};
    session.setShellViewport(viewport);
    expect(session.shellLayout().inspectorCollapsed,
        "production details are closed without requiring a blocked corridor");
    togglePanel(session);
    expect(!session.shellLayout().inspectorCollapsed,
        "production fixed toggle opens via pointer");
    for (int index = 0; index < 5; ++index) {
        GameAction orbit;
        orbit.type = ActionType::OrbitCamera;
        orbit.first = 0.11f;
        expect(runtime.dispatch(orbit).ok, "camera orbit remains available");
        viewport.hasProtectedGameplayRect = index % 2 != 0;
        viewport.protectedGameplayRect = {0,0,2048,1536};
        session.setShellViewport(viewport);
        expect(!session.shellLayout().inspectorCollapsed,
            "camera and protection changes never undo manual open");
    }
    togglePanel(session);
    viewport.hasProtectedGameplayRect = false;
    session.setShellViewport(viewport);
    expect(session.shellLayout().inspectorCollapsed,
        "closed details stay closed after camera becomes unobstructed");
    training::AttemptFacts failed;
    failed.state = runtime.state();
    failed.numericalFailure = true;
    expect(session.training().finishAttempt(failed).ok,
        "production fixture completes a failed attempt");
    session.setShellViewport(viewport);
    expect(click(session, "retry_training").ok &&
        session.snapshot().shell.training.state == training::TrainingProductState::AttemptActive,
        "compact retry uses the visible hit target and starts a fresh attempt");
    expect(session.shellLayout().inspectorCollapsed,
        "retry preserves the user's closed details preference");
}

void testScrollablePanel()
{
    GameRuntime runtime;
    ai::AiPlayerController ai;
    auto session = makeSession(runtime, ai, "scroll-panel");
    expect(session.shell().startTrainingScenario(session.training().scenarios().front().id,
        "scroll-panel", 0).ok && session.shell().focusItem("training_advanced").ok &&
        session.shell().navigate(ShellNavigationCommand::Activate).ok, "open dense advanced panel");
    ui::ShellViewport viewport{900,430,1800,860,192.0f};
    session.setShellViewport(viewport);
    expect(session.shellLayout().inspectorCollapsed, "short advanced panel reproduces height failure");
    togglePanel(session);
    auto layout = session.shellLayout();
    expect(layout.ok && !layout.inspectorCollapsed && layout.maximumScrollRow > 0,
        "Show panel recovers height failure with readable scrolling rows");
    const auto hash = gameStateSha256(runtime.state());
    PlatformInputEvent scroll;
    scroll.kind = PlatformInputKind::Scroll;
    scroll.logicalX = (layout.bodyRect.x + layout.bodyRect.width / 2) / 2;
    scroll.logicalY = (layout.bodyRect.y + layout.bodyRect.height / 2) / 2;
    scroll.scrollY = -1;
    for (std::size_t row = 0; row < layout.maximumScrollRow + 5; ++row)
        expect(session.handlePlatformInput(scroll).action == "inspector_scroll", "wheel routes to panel");
    layout = session.shellLayout();
    expect(layout.firstVisibleRow == layout.maximumScrollRow &&
        layout.elements.back().id == "training_advanced" && gameStateSha256(runtime.state()) == hash,
        "scroll reaches last row without zooming, aiming or shooting");
    expect(click(session, "training_advanced").ok, "scrolled row activates its semantic action");
    session.setShellViewport(viewport);
    expect(session.shellLayout().firstVisibleRow <= session.shellLayout().maximumScrollRow,
        "shorter content clamps retained scroll offset");
    scroll.scrollY = 1;
    for (int index = 0; index < 40; ++index) session.handlePlatformInput(scroll);
    expect(session.shellLayout().firstVisibleRow == 0, "upward scroll reaches first row");
}

void testIllegalBreakPointerAndKeyboardCommit()
{
    GameRuntime runtime;
    ai::AiPlayerController ai;
    ModernProductSessionOptions options;
    options.settingsPath = (std::filesystem::temp_directory_path() /
        "billiardgl-pointer-illegal-break.json").string();
    options.rulesDecisionProductCapabilities.illegalBreakChoice = true;
    ModernProductSession session(runtime, ai, options);
    session.setShellViewport({1280, 720, 1280, 720, 96.0f});
    expect(session.shell().startSinglePlayer(701).ok,
        "IllegalBreak pointer fixture starts Match");
    ai::PlayerConfiguration disabled;
    expect(ai.configure(runtime, disabled).ok,
        "pointer fixture exposes local two-player actor");
    stagePointerIllegalBreak(runtime);
    session.syncRulesDecisionAuthority();

    const ui::RulesDecisionUiHitRect action{
        std::string("action:") + rules::kBreakFoulAcceptTableActionId,
        rules::kBreakFoulAcceptTableActionId, {100, 100, 220, 48}, true,
        false};
    session.setRulesDecisionHitRects({action});
    const auto revision = runtime.rulesAuthorityRevision();
    expect(pointer(session, PlatformInputKind::PointerButtonDown,
            {150, 120}, "LeftButton").handled &&
        pointer(session, PlatformInputKind::PointerButtonUp,
            {400, 300}, "LeftButton").handled &&
        runtime.rulesAuthorityRevision() == revision &&
        session.snapshot().rulesDecision.armedActionId.empty(),
        "press inside and release outside is consumed without arming or gameplay leakage");
    expect(pointer(session, PlatformInputKind::PointerButtonDown,
            {150, 120}, "LeftButton").handled &&
        pointer(session, PlatformInputKind::PointerButtonUp,
            {150, 120}, "LeftButton").handled &&
        session.snapshot().rulesDecision.armedActionId ==
            rules::kBreakFoulAcceptTableActionId &&
        runtime.rulesAuthorityRevision() == revision,
        "matching pointer down/up arms but does not submit authority");

    const ui::RulesDecisionUiHitRect confirm{
        std::string("confirm:") + rules::kBreakFoulAcceptTableActionId,
        rules::kBreakFoulAcceptTableActionId, {360, 100, 180, 48}, true,
        true};
    const ui::RulesDecisionUiHitRect rerackAction{
        std::string("action:") + rules::kBreakFoulRerackOpponentActionId,
        rules::kBreakFoulRerackOpponentActionId, {100, 180, 220, 48}, true,
        false};
    session.setRulesDecisionHitRects({action, rerackAction, confirm});
    expect(pointer(session, PlatformInputKind::PointerButtonDown,
            {150, 200}, "LeftButton").handled &&
        pointer(session, PlatformInputKind::PointerButtonUp,
            {150, 200}, "LeftButton").handled &&
        session.snapshot().rulesDecision.armedActionId ==
            rules::kBreakFoulRerackOpponentActionId,
        "a second action may arm before the next rendered frame");
    expect(pointer(session, PlatformInputKind::PointerButtonDown,
            {450, 120}, "LeftButton").handled &&
        pointer(session, PlatformInputKind::PointerButtonUp,
            {450, 120}, "LeftButton").handled &&
        runtime.rulesAuthorityRevision() == revision &&
        runtime.breakFoulDecisionOffer().has_value(),
        "stale Confirm(A) is consumed but cannot submit newly armed B");
    const ui::RulesDecisionUiHitRect rerackConfirm{
        std::string("confirm:") + rules::kBreakFoulRerackOpponentActionId,
        rules::kBreakFoulRerackOpponentActionId, {360, 180, 180, 48}, true,
        true};
    session.setRulesDecisionHitRects({action, rerackAction, rerackConfirm});
    expect(pointer(session, PlatformInputKind::PointerButtonDown,
            {450, 200}, "LeftButton").handled &&
        pointer(session, PlatformInputKind::PointerButtonUp,
            {450, 200}, "LeftButton").ok &&
        runtime.rulesAuthorityRevision() == revision + 1 &&
        !runtime.breakFoulDecisionOffer().has_value(),
        "fresh Confirm(B) submits one exact authority ACK");

    GameRuntime keyboardRuntime;
    ai::AiPlayerController keyboardAi;
    ModernProductSession keyboardSession(keyboardRuntime, keyboardAi, options);
    keyboardSession.setShellViewport({1280, 720, 1280, 720, 96.0f});
    expect(keyboardSession.shell().startSinglePlayer(702).ok &&
        keyboardAi.configure(keyboardRuntime, disabled).ok,
        "keyboard fixture starts local Match");
    stagePointerIllegalBreak(keyboardRuntime);
    keyboardSession.syncRulesDecisionAuthority();
    const auto keyboardRevision = keyboardRuntime.rulesAuthorityRevision();
    expect(keyboardSession.handlePlatformInput(key("Tab")).handled &&
        keyboardSession.handlePlatformInput(key("Enter")).handled &&
        keyboardRuntime.rulesAuthorityRevision() == keyboardRevision &&
        !keyboardSession.snapshot().rulesDecision.armedActionId.empty(),
        "keyboard focus and first confirm arm without submitting");
    expect(keyboardSession.handlePlatformInput(key("Enter")).ok &&
        keyboardRuntime.rulesAuthorityRevision() == keyboardRevision + 1,
        "second keyboard confirm submits the selected action");
}

void testPersistedCameraSettingsSyncOnceAtStartup()
{
    const std::filesystem::path path = std::filesystem::temp_directory_path() /
        "billiardgl-pointer-camera-startup.json";
    std::error_code ignored;
    std::filesystem::remove(path, ignored);
    std::filesystem::remove(path.string() + ".tmp", ignored);
    settings::ProductSettings saved = settings::ProductSettings::defaults();
    saved.fieldOfViewDegrees = 65.0;
    saved.cameraDistanceCm = 270.0;
    saved.dynamicCamera = false;
    expect(settings::saveFileAtomic(path.string(), saved).ok,
        "camera startup fixture saves compatible settings-v1 fields");

    GameRuntime runtime;
    ai::AiPlayerController ai;
    ModernProductSessionOptions options;
    options.settingsPath = path.string();
    ModernProductSession session(runtime, ai, options);
    const auto& preferences = runtime.cameraPresentationPreferences();
    expect(std::fabs(preferences.fieldOfViewOffsetDegrees - 10.0f) < 1.0e-6f &&
        std::fabs(preferences.distanceScale - 1.5f) < 1.0e-6f &&
        !preferences.dynamicTrackingEnabled &&
        std::fabs(runtime.cameraFrame().pose.fieldOfViewDegrees - 58.0f) < 1.0e-5f &&
        std::fabs(runtime.cameraFrame().pose.distanceCm - 330.0f) < 1.0e-5f,
        "session startup maps stored absolute fields to relative camera preferences once");
    std::filesystem::remove(path, ignored);
    std::filesystem::remove(path.string() + ".tmp", ignored);
}

int main()
{
    try {
        testSharedLayoutHitPolicy();
        testCollapsedTerminalRetry();
        testManualPanelToggle();
        testProductionPanelStability();
        testScrollablePanel();
        testIllegalBreakPointerAndKeyboardCommit();
        testPersistedCameraSettingsSyncOnceAtStartup();
        testShellNavigationAndPointerCapture();
        testTrainingReplayHelpAndSettings();
        testRecordedPocketStorageVisibility(false);
        testRecordedPocketStorageVisibility(true);
        testInjectedTrainingReplayFileCallbacks();
        testTrajectoryInsetFrameLifecycle();
        testReplayKeyboardFocusScroll();
        std::cout << "Modern product pointer session E2E passed\n";
        return EXIT_SUCCESS;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return EXIT_FAILURE;
    }
}
