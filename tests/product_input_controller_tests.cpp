#include "deterministic_hash.h"
#include "product_input_controller.h"
#include "diagnostics/playtest_logger.h"
#include "replay/replay_recorder.h"
#include "render/modern/camera_projection.h"

#include <cmath>
#include <chrono>
#include <fstream>
#include <cstdlib>
#include <filesystem>
#include <iostream>
#include <limits>
#include <set>
#include <map>
#ifndef _WIN32
#include <sys/resource.h>
#include <csignal>
#endif
#include <stdexcept>
#include <string>

namespace {

using billiardgl::NormalizedInputEvent;
using billiardgl::ProductInputController;
using billiardgl::ProductInputSurface;

void expect(bool value, const char* message)
{
    if (!value) throw std::runtime_error(message);
}

NormalizedInputEvent input(const char* control, const char* trigger = "press",
    double value = 0.0, double secondary = 0.0,
    const char* device = "keyboard")
{
    return {device, control, trigger, value, secondary};
}

std::set<std::string> allCapabilities()
{
    return {"training_scenario_v1", "authoritative_horizontal_tip_offset_v1",
        "authoritative_longitudinal_tip_offset_v1"};
}

struct SettingsFixture {
    std::filesystem::path path;
    billiardgl::settings::ProductSettingsRuntime runtime;

    explicit SettingsFixture(const char* name)
        : path(std::filesystem::temp_directory_path() / name),
          runtime(path.string())
    {
        std::error_code ignored;
        std::filesystem::remove(path, ignored);
        std::filesystem::remove(path.string() + ".tmp", ignored);
        runtime.start();
    }

    ~SettingsFixture()
    {
        std::error_code ignored;
        std::filesystem::remove(path, ignored);
        std::filesystem::remove(path.string() + ".tmp", ignored);
    }
};

void testSettingsModalAndImmediateRebind()
{
    billiardgl::GameRuntime authority;
    SettingsFixture settings("billiardgl_product_input_settings_v1.json");
    ProductInputController controller(authority, settings.runtime);

    expect(controller.process(input("F10")).ok &&
        settings.runtime.menu().isOpen(),
        "global settings binding opens the modal through settings runtime");
    const billiardgl::AimMode beforeAim = authority.state().aim.mode;
    const auto blocked = controller.process(input("Tab"));
    expect(!blocked.ok && blocked.reasonCode == "input_settings_modal" &&
        authority.state().aim.mode == beforeAim,
        "settings modal blocks play input without mutating authority");

    controller.process(input("ArrowRight"));
    controller.process(input("ArrowRight"));
    controller.process(input("ArrowRight"));
    const double liveMusic = settings.runtime.current().musicVolume;
    expect(controller.process(input("+")).ok &&
        settings.runtime.menu().preview().musicVolume > liveMusic &&
        settings.runtime.current().musicVolume == liveMusic,
        "settings navigation edits transaction preview only");
    expect(controller.process(input("F10")).ok &&
        !settings.runtime.menu().isOpen() &&
        settings.runtime.current().musicVolume == liveMusic,
        "opening shortcut cancels an already-open modal and discards preview");

    expect(controller.process(input("F10")).ok &&
        controller.beginRebind("toggle_aim").ok &&
        controller.process(input("F2")).ok,
        "rebind capture consumes the next normalized event before routing");
    const auto applied = controller.process(input("Enter"));
    if (!applied.ok)
        std::cerr << "settings apply reason: " << applied.reasonCode << '\n';
    expect(applied.ok &&
        !settings.runtime.menu().isOpen() &&
        std::filesystem::exists(settings.path),
        "settings apply atomically persists the captured binding");
    expect(controller.process(input("F2")).ok &&
        authority.state().aim.mode == billiardgl::AimMode::Aim,
        "new binding is active on the first event after apply");
    expect(controller.process(input("Tab")).reasonCode == "input_unbound",
        "replaced default binding no longer bypasses the router");
}

void testPlaytestDiagnostics()
{
    billiardgl::GameRuntime authority;
    const auto root = std::filesystem::temp_directory_path() /
        ("billiardgl-logger-test-" + std::to_string(
            std::chrono::steady_clock::now().time_since_epoch().count()));
    std::string session;
    {
        billiardgl::diagnostics::PlaytestLogger logger(root.string(), "test-build", 8192);
        expect(logger.good(), "diagnostic log opens a unique writable session");
        session = logger.path();
        auto metadata = billiardgl::json::Value::object();
        metadata["test_marker"] = billiardgl::json::Value("preserved");
        logger.record("session_start", authority, metadata);
        for (int n = 0; n < 25; ++n) logger.record("physics_step", authority);
        authority.mutableState().balls[1].position.y = std::numeric_limits<float>::infinity();
        logger.record("nonfinite_probe", authority);
        expect(logger.good(), "nonfinite diagnostic evidence must not break JSON logging");
    }
    int files = 0;
    bool foundNonfinite = false;
    for (const auto& entry : std::filesystem::directory_iterator(session)) {
        if (entry.path().extension() != ".jsonl") continue;
        ++files;
        std::ifstream stream(entry.path());
        std::string line;
        while (std::getline(stream, line)) {
            const auto parsed = billiardgl::json::parse(line);
            expect(parsed.ok, "every flushed diagnostic line is valid JSON");
            if (parsed.value.at("kind").asString() == "segment_start") {
                if (parsed.value.at("segment").asString() != "0")
                    expect(parsed.value.at("session_metadata").has("test_marker"),
                        "successor segments retain session metadata");
            } else {
                expect(parsed.value.at("balls").asArray().size() == 16,
                    "diagnostic records retain all ball identities");
                for (const auto& ball : parsed.value.at("balls").asArray()) {
                    expect(ball.has("speed_cm_s") && ball.has("motion_state"),
                        "diagnostics retain cached speed and motion classification for state-drift analysis");
                }
                foundNonfinite |= parsed.value.at("kind").asString() == "nonfinite_probe";
            }
        }
    }
    expect(files > 4 && foundNonfinite, "full evidence retains every segment and newest probe");
    {
        billiardgl::GameRuntime clean;
        billiardgl::diagnostics::PlaytestLogger benchmark(root.string(), "benchmark");
        const auto start = std::chrono::steady_clock::now();
        for (int i = 0; i < 1200; ++i) benchmark.record("physics_step", clean);
        expect(benchmark.good(), "sustained diagnostic writes succeed");
        std::cout << "Diagnostic 1200 full-state records: " <<
            std::chrono::duration<double, std::milli>(
                std::chrono::steady_clock::now() - start).count() << " ms\n";
    }
    {
        billiardgl::diagnostics::PlaytestLogger invalid(
            (std::filesystem::path(session) / "events-0.jsonl" / "invalid").string(), "test");
        expect(!invalid.good(), "unwritable diagnostic path reports failure without throwing");
    }
    // These are the unique paths created by this test, never an existing session.
    std::filesystem::remove_all(root);
}

void testFullEvidenceLineageAndDiskFailure()
{
    using namespace billiardgl;
    using Logger = diagnostics::PlaytestLogger;
    const char* artifacts=std::getenv("BILLIARDGL_LOGGER_TEST_ARTIFACT_ROOT");
    const auto root = (artifacts ? std::filesystem::path(artifacts) : std::filesystem::temp_directory_path()) /
        ("billiardgl-full-evidence-" + std::to_string(
            std::chrono::steady_clock::now().time_since_epoch().count()));
    const auto bytes = [](const std::filesystem::path& p) {
        std::ifstream input(p,std::ios::binary);
        return std::string(std::istreambuf_iterator<char>(input),{});
    };
    const auto parseFile = [&](const std::filesystem::path& p) {
        const auto parsed=json::parse(bytes(p));
        expect(parsed.ok,"closed lineage receipts are valid JSON");return parsed.value;
    };
    GameRuntime runtime;
    Logger logger(root.string(),"full-evidence-content",4096);
    const auto directory=std::filesystem::path(logger.path());
    struct Expected { std::string kind; std::uint64_t tick; int marker;
        std::array<Point3,kBallCount> positions; float yaw; };
    std::vector<Expected> expected;
    std::map<std::string,std::string> immutable;
    const auto inspectClosed = [&]() {
        for(const auto& entry:std::filesystem::directory_iterator(directory)) {
            if(entry.path().filename().string().rfind("segment-",0)!=0)continue;
            auto receipt=parseFile(entry.path());
            for(const auto& file:{entry.path(),directory/receipt.at("data_file").asString()}) {
                const auto hash=sha256Hex(bytes(file));
                auto found=immutable.find(file.string());
                if(found==immutable.end())immutable.emplace(file.string(),hash);
                else expect(found->second==hash,"later rollovers never mutate an earlier closed file");
            }
        }
    };
    const auto record = [&](const std::string& kind,int marker) {
        auto details=json::Value::object();details["marker"]=json::Value(marker);
        Expected e{kind,runtime.tick(),marker,{},runtime.shotControl().yawRadians};
        for(int id=0;id<kBallCount;++id)e.positions[id]=runtime.state().balls[id].position;
        expected.push_back(e);logger.record(kind,runtime,details);
        expect(logger.good(),"full evidence continues through repeated small-limit rollovers");inspectClosed();
    };
    record("session_start",-1);
    for(int n=0;n<12;++n) {
        record("input_before",n);
        GameAction action;action.type=ActionType::SetAimYaw;action.first=.01f*n;
        expect(runtime.dispatch(action).ok,"fixture commits actual semantic aim input");
        record("input_after",n);
        expect(runtime.step(1).ok,"fixture advances actual production tick");
        record("physics_step",n);
    }
    logger.close();inspectClosed();
    const auto session=parseFile(directory/"session-closed.json");
    expect(session.at("status").asString()=="FULL_EVIDENCE_CLOSED" &&
        session.at("retention_complete").asBool(),"successful explicit close declares complete retention");
    std::size_t cursor=0;std::string parent;
    const auto count=std::stoull(session.at("segment_count").asString());
    for(std::size_t index=0;index<count;++index) {
        const auto receiptPath=directory/("segment-"+std::to_string(index)+".closed.json");
        const auto receipt=parseFile(receiptPath);
        expect(receipt.at("previous_segment_receipt_sha256").asString()==parent &&
            receipt.at("first_event_sequence").asString()==std::to_string(cursor),
            "segment lineage and sequence start are contiguous");
        const auto dataPath=directory/receipt.at("data_file").asString();
        expect(receipt.at("data_sha256").asString()==sha256Hex(bytes(dataPath)),
            "receipt binds actual closed data bytes");
        std::ifstream input(dataPath);std::string line;std::size_t records=0;
        while(std::getline(input,line)) {
            auto parsed=json::parse(line);expect(parsed.ok,"every complete retained data row parses");
            const auto& row=parsed.value;
            if(row.at("kind").asString()=="segment_start") {
                expect(row.at("previous_segment_receipt_sha256").asString()==parent,
                    "new segment header binds the previous closed receipt");continue;
            }
            expect(cursor<expected.size(),"no fabricated extra record");const auto& e=expected[cursor];
            expect(row.at("sequence").asString()==std::to_string(cursor) &&
                row.at("kind").asString()==e.kind && row.at("tick").asString()==std::to_string(e.tick) &&
                row.at("details").at("marker").asNumber()==e.marker &&
                std::fabs(row.at("shot").at("yaw_rad").asNumber()-e.yaw)<1e-7,
                "startup, every input pair and every tick retain exact order and payload");
            const auto& balls=row.at("balls").asArray();expect(balls.size()==kBallCount,"all sixteen paths retained");
            for(int id=0;id<kBallCount;++id) {
                const auto& point=balls[id].at("position_cm").asArray();
                expect(balls[id].at("id").asNumber()==id &&
                    std::fabs(point[0].asNumber()-e.positions[id].x)<1e-6 &&
                    std::fabs(point[1].asNumber()-e.positions[id].y)<1e-6 &&
                    std::fabs(point[2].asNumber()-e.positions[id].z)<1e-6,
                    "each retained row binds all actual ball positions");
            }
            ++cursor;++records;
        }
        expect(receipt.at("event_count").asString()==std::to_string(records) &&
            receipt.at("next_event_sequence").asString()==std::to_string(cursor),
            "closed event count and range agree with actual parsed content");
        parent=sha256Hex(bytes(receiptPath));
    }
    expect(cursor==expected.size() && session.at("next_event_sequence").asString()==std::to_string(cursor) &&
        session.at("last_segment_receipt_sha256").asString()==parent,
        "complete session has no missing startup, input or physics tick");
    // A real exclusive-create filesystem failure must preserve committed data.
    Logger collision(root.string(),"disk-open-failure",4096);
    collision.record("session_start",runtime);const auto failureDir=std::filesystem::path(collision.path());
    const auto retained=bytes(failureDir/"events-0.jsonl");
    std::filesystem::create_directory(failureDir/"events-1.jsonl");
    collision.record("input_before",runtime);
    expect(!collision.good(),"disk/create failure stops recording visibly");collision.close();
    expect(bytes(failureDir/"events-0.jsonl")==retained &&
        parseFile(failureDir/"session-closed.json").at("status").asString()=="FAILED_RECORDING",
        "disk failure retains prior bytes and cannot declare full evidence");
#ifndef _WIN32
    Logger limited(root.string(),"disk-write-failure",4096);
    limited.record("session_start",runtime);limited.record("input_before",runtime);
    const auto limitedDir=std::filesystem::path(limited.path());
    const auto earlier=bytes(limitedDir/"events-0.jsonl");
    struct rlimit original;expect(getrlimit(RLIMIT_FSIZE,&original)==0,"read actual filesystem write limit");
    const auto previousSignal=std::signal(SIGXFSZ,SIG_IGN);
    struct rlimit restriction=original;restriction.rlim_cur=512;
    expect(setrlimit(RLIMIT_FSIZE,&restriction)==0,"set actual EFBIG write limit");
    limited.record("physics_step",runtime);
    const bool stopped=!limited.good();
    setrlimit(RLIMIT_FSIZE,&original);std::signal(SIGXFSZ,previousSignal);
    limited.close();
    expect(stopped && bytes(limitedDir/"events-0.jsonl")==earlier &&
        parseFile(limitedDir/"session-closed.json").at("status").asString()=="FAILED_RECORDING",
        "actual EFBIG partial-write failure preserves earlier closed bytes and reports incomplete");
#endif
    Logger bounded(root.string(),"explicit-bounded",4096,Logger::RetentionMode::BoundedRolling);
    for(int n=0;n<12;++n)bounded.record("physics_step",runtime);
    bounded.close();
    const auto partial=parseFile(std::filesystem::path(bounded.path())/"session-closed.json");
    expect(partial.at("status").asString()=="PARTIAL_BOUNDED_CLOSED" &&
        !partial.at("retention_complete").asBool(),
        "ordinary bounded diagnostics never masquerade as complete evidence");
    if(!artifacts)std::filesystem::remove_all(root);
}

void testPlaytestSameIdProfileReplacement()
{
    using namespace billiardgl;
    GameRuntime runtime;
    const auto root=std::filesystem::temp_directory_path() /
        ("billiardgl-log-profile-cache-"+std::to_string(
            std::chrono::steady_clock::now().time_since_epoch().count()));
    diagnostics::PlaytestLogger logger(root.string(),"actual-profile-replacement");
    const auto original=runtime.physicsProfile();
    const auto originalHash=sha256Hex(canonicalPhysicsProfileText(original));
    logger.record("session_start",runtime);
    auto changed=original;changed.surface.legacyFrictionAccelerationCmS2+=1.0f;
    expect(changed.id==original.id && runtime.replaceStateForScenario(
        runtime.state(),changed).ok,"public runtime scenario setter accepts legal same-ID profile replacement");
    const auto changedHash=sha256Hex(canonicalPhysicsProfileText(runtime.physicsProfile()));
    const auto replayHash=sha256Hex(canonicalPhysicsProfileJson(runtime.physicsProfile()));
    expect(changedHash!=originalHash,"same-ID parameter replacement has a different canonical identity");
    logger.record("scenario_profile_replaced",runtime);
    auto rejected=changed;rejected.ball.radiusCm=-1;
    expect(!runtime.replaceStateForScenario(runtime.state(),rejected).ok,
        "invalid replacement is rejected by the real runtime");
    logger.record("scenario_profile_rejected",runtime);logger.close();
    std::ifstream input(std::filesystem::path(logger.path())/"events-0.jsonl");
    std::string line;std::vector<json::Value> rows;
    while(std::getline(input,line)) {auto parsed=json::parse(line);expect(parsed.ok,"profile logs parse");
        if(parsed.value.has("physics_profile_id"))rows.push_back(parsed.value);}
    expect(rows.size()==3 && rows[0].at("physics_profile_sha256").asString()==originalHash,
        "initial snapshot binds the original actual profile");
    for(std::size_t i=1;i<rows.size();++i)
        expect(rows[i].at("physics_profile_id").asString()==original.id &&
            rows[i].at("physics_profile_sha256").asString()==changedHash &&
            rows[i].at("physics_profile_canonical_text_sha256").asString()==changedHash &&
            rows[i].at("physics_profile_replay_canonical_json_sha256").asString()==replayHash,
            "same-ID cache refresh follows current validated runtime profile and rejected mutation leaves it intact");
    std::filesystem::remove_all(root);
}

void testPlaytestRulesDiagnostics()
{
    using namespace billiardgl;
    GameRuntime runtime;
    const auto root = std::filesystem::temp_directory_path() /
        ("billiardgl-rules-log-test-" + std::to_string(
            std::chrono::steady_clock::now().time_since_epoch().count()));
    diagnostics::PlaytestLogger logger(root.string(), "rules-test");
    expect(runtime.setRulesMode(RulesMode::ChineseEightBallV1).ok,
        "enable rules for component logging fixture");
    // Direct facts establish a component fixture, not native shot evidence.
    rules::ShotFacts shot;
    shot.shooter = 0;
    shot.firstContactBall = 1;
    shot.anyRailAfterFirstContact = true;
    shot.breakShot = true;
    shot.distinctObjectBallsToRail = 4;
    expect(runtime.adjudicateRulesShot(shot).ok, "stage legal break");
    shot.breakShot = false;
    shot.shooter = 1;
    shot.firstContactBalls = {1, 9};
    shot.pocketedBallsInOrder = {1, 9};
    expect(runtime.adjudicateRulesShot(shot).ok &&
        runtime.groupChoiceDecisionOffer().has_value(), "stage group choice");
    const auto offer = *runtime.groupChoiceDecisionOffer();
    const auto beforeHash = runtime.rulesAuthorityStateHash();
    logger.record("input_before", runtime);
    rules::GroupChoiceDecisionRequest request;
    request.rulesetId = rules::kGroupChoiceRulesetId;
    request.capabilityId = rules::kGroupChoiceAuthorityCapabilityId;
    request.flowId = rules::kGroupChoiceAuthorityFlowId;
    request.decisionId = offer.decisionId;
    request.expectedAuthorityStateRevision = offer.authorityStateRevision;
    request.idempotencyKey = "log-choice";
    request.actorPlayerId = offer.actorPlayerId;
    request.actionId = rules::kGroupChoiceSolidsActionId;
    const auto committed = runtime.submitGroupChoiceDecision(request);
    expect(committed.ok, "commit real authority action for logging");
    const auto afterHash = runtime.rulesAuthorityStateHash();
    const auto actionCount = runtime.rulesAuthorityActions().size();
    logger.record("input_after", runtime);
    request.idempotencyKey = "rejected-choice";
    request.actionId = "invalid-action";
    expect(!runtime.submitGroupChoiceDecision(request).ok, "reject invalid request");
    logger.record("input_after", runtime);
    logger.record("physics_step", runtime);
    expect(runtime.rulesAuthorityStateHash() == afterHash &&
        runtime.rulesAuthorityActions().size() == actionCount,
        "logging and rejected input do not mutate authority");
    std::ifstream stream(std::filesystem::path(logger.path()) / "events-0.jsonl");
    std::string line;
    std::vector<json::Value> rows;
    while (std::getline(stream, line)) {
        const auto parsed = json::parse(line);
        expect(parsed.ok, "rules diagnostics valid JSON");
        if (parsed.value.has("rules_authority"))
            rows.push_back(parsed.value.at("rules_authority"));
    }
    expect(rows.size() == 4, "four rules snapshots retained");
    expect(rows[0].at("latest_committed_action").isNull() &&
        rows[0].at("current_authority_hash_sampled").asBool() &&
        rows[0].at("current_authority_hash").asString() == beforeHash,
        "before snapshot has exact hash and explicit empty action");
    const auto& action = rows[1].at("latest_committed_action");
    expect(rows[1].at("current_authority_hash").asString() == afterHash &&
        action.at("decision_id").asString() == offer.decisionId &&
        action.at("action_id").asString() == rules::kGroupChoiceSolidsActionId &&
        action.at("actor_player_id").asInt() == offer.actorPlayerId &&
        action.at("previous_revision").asString() == std::to_string(offer.authorityStateRevision) &&
        action.at("committed_revision").asString() == std::to_string(runtime.rulesAuthorityRevision()) &&
        action.at("ack_authority_hash").asString() == committed.ack.authoritativeStateHash,
        "after snapshot binds exact committed decision and ACK");
    expect(json::stringify(rows[1]) == json::stringify(rows[2]),
        "rejected input does not fabricate a new action");
    expect(!rows[3].at("current_authority_hash_sampled").asBool() &&
        rows[3].at("current_authority_hash").isNull() &&
        json::stringify(rows[3].at("latest_committed_action")) == json::stringify(action),
        "physics snapshot retains ACK without recomputing current hash");
    logger.close();
    std::filesystem::remove_all(root);
}

void testAimTogglePreservesViewedDirection()
{
    billiardgl::GameRuntime authority;
    SettingsFixture settings("billiardgl_product_input_aim_view.json");
    ProductInputController controller(authority, settings.runtime);
    expect(controller.process(input("LeftDrag", "continuous",
        170.0, -30.0, "mouse")).ok, "rotate to a non-default view");
    const auto before = authority.cameraFrame().pose;
    for (int toggle = 0; toggle < 3; ++toggle) {
        expect(controller.process(input("Tab")).ok, "toggle aim view");
        if (authority.state().aim.mode == billiardgl::AimMode::Aim) {
            const float aim = authority.shotControl().yawRadians;
            expect(std::fabs(std::cos(aim) + std::cos(before.yawRadians)) < 1e-5 &&
                std::fabs(std::sin(aim) + std::sin(before.yawRadians)) < 1e-5 &&
                std::fabs(aim - authority.state().aim.yaw) < 1e-5,
                "cue and shot controls point along the displayed camera forward direction");
        }
        for (int frame = 0; frame < 180; ++frame) {
            expect(authority.step(1).ok, "advance aim camera transition");
            const auto pose = authority.cameraFrame().pose;
            expect(std::fabs(pose.yawRadians - before.yawRadians) < 1e-5 &&
                std::fabs(pose.pitchRadians - before.pitchRadians) < 1e-5,
                "Tab never resets the chosen yaw or pitch, including during transition");
        }
        expect(!authority.cameraFrame().transitioning, "aim view transition finishes");
    }
}

void testMouseAngularUnits()
{
    for (const double multiplier : {0.5, 1.0, 2.0}) {
        billiardgl::GameRuntime authority;
        SettingsFixture settings("billiardgl_product_input_mouse_units.json");
        auto configured = settings.runtime.current();
        configured.mouseSensitivity = multiplier;
        configured.invertMouseY = true;
        expect(billiardgl::settings::saveFileAtomic(settings.path.string(),
            configured).ok, "save sensitivity test settings");
        settings.runtime.start();
        ProductInputController controller(authority, settings.runtime);
        const float yaw = authority.state().camera.angleX;
        const float pitch = authority.state().camera.angleY;
        expect(controller.process(input("LeftDrag", "continuous",
            100.0, 20.0, "mouse")).ok &&
            std::fabs(authority.state().camera.angleX - yaw - 0.5 * multiplier) < 1e-5 &&
            std::fabs(authority.state().camera.angleY - pitch + 0.1 * multiplier) < 1e-5,
            "orbit converts points to radians and respects sensitivity/inverted Y");
        expect(controller.process(input("Tab")).ok, "enter aim for mouse units");
        const float aim = authority.shotControl().yawRadians;
        expect(controller.process(input("MoveX", "continuous",
            100.0, 0.0, "mouse")).ok &&
            std::fabs(authority.shotControl().yawRadians - aim - 0.2 * multiplier) < 1e-5,
            "aim uses finer point-to-radian conversion than orbit");
        for (int i = 0; i < 10; ++i)
            expect(controller.process(input("MoveX", "continuous",
                -10.0, 0.0, "mouse")).ok, "split pointer movement accepted");
        expect(std::fabs(authority.shotControl().yawRadians - aim) < 1e-5,
            "angular displacement does not depend on pointer event packetization");
    }
}

void testAimPowerShootAndFailureIsolation()
{
    billiardgl::GameRuntime authority;
    SettingsFixture settings("billiardgl_product_input_shot_v1.json");
    ProductInputController controller(authority, settings.runtime);
    const std::size_t initialJournal =
        authority.appliedSemanticActions().size();

    controller.setAiTurnActive(true);
    const auto aiBlocked = controller.process(input("Tab"));
    expect(!aiBlocked.ok && aiBlocked.reasonCode == "input_ai_turn_active" &&
        authority.appliedSemanticActions().size() == initialJournal &&
        authority.state().aim.mode == billiardgl::AimMode::Observe,
        "AI turn blocks player shot-control actions without journal pollution");
    controller.setAiTurnActive(false);

    expect(controller.process(input("Tab")).ok &&
        authority.state().aim.mode == billiardgl::AimMode::Aim,
        "default toggle binding enters aim through semantic GameRuntime action");
    const float initialYaw = authority.shotControl().yawRadians;
    const float initialPower = authority.shotControl().requestedPower;
    expect(controller.process(input(
        "MoveX", "continuous", 125.0, 0.0, "mouse")).ok &&
        std::fabs(authority.shotControl().yawRadians -
            (initialYaw + 0.25f)) < 0.00001f,
        "continuous normalized aim delta round-trips through SetAimYaw");
    expect(controller.process(input(
        "ArrowKeys", "continuous", 1.0, -1.0)).ok &&
        std::fabs(authority.shotControl().tipOffsetX - 0.05f) < 0.00001f &&
        std::fabs(authority.shotControl().tipOffsetY + 0.05f) < 0.00001f,
        "aim-context arrow input round-trips through public SetTipOffset");
    expect(controller.process(input(
        "WheelUp", "step", 2.0, 0.0, "mouse")).ok &&
        std::fabs(authority.shotControl().requestedPower -
            (initialPower + 10.0f)) < 0.00001f,
        "step magnitude round-trips through SetShotPower");
    expect(controller.process(input(
        "LeftButton", "release", 0.0, 0.0, "mouse")).ok &&
        authority.state().ballsMoving,
        "default shoot binding reaches the authoritative Shoot action");
    expect(authority.appliedSemanticActions().size() == initialJournal + 4,
        "aim, tip, power, and shoot produce exactly their semantic journal entries");

    const std::size_t afterShot = authority.appliedSemanticActions().size();
    const auto movingBlocked = controller.process(input("Tab"));
    expect(!movingBlocked.ok &&
        movingBlocked.reasonCode == "input_balls_moving" &&
        authority.appliedSemanticActions().size() == afterShot,
        "moving balls reject shot control with stable reason and no journal write");
    const auto nonfinite = controller.process(input("MoveX", "continuous",
        std::numeric_limits<double>::quiet_NaN(), 0.0, "mouse"));
    expect(!nonfinite.ok && nonfinite.reasonCode == "input_nonfinite_value" &&
        authority.appliedSemanticActions().size() == afterShot,
        "nonfinite normalized input is rejected before routing");
    expect(controller.process(input("NoSuchControl")).reasonCode ==
            "input_unbound" &&
        controller.process(input(" ")).reasonCode == "input_invalid_identity" &&
        authority.appliedSemanticActions().size() == afterShot,
        "unbound and malformed input cannot pollute authority logs");
    expect(!billiardgl::settings::assistCanAutoAimOrShoot(
        settings.runtime.current()),
        "assist settings remain descriptive and never auto-aim or auto-shoot");
}

void testScreenRelativeMousePan()
{
    using namespace billiardgl;
    for (const auto preset : {camera::Preset::PlayerLow, camera::Preset::Elevated,
            camera::Preset::Overhead}) {
        for (int angle = 0; angle < 4; ++angle) {
            for (int backing : {1, 2}) {
                for (int height : {720, 1080}) {
                    GameRuntime runtime;
                    runtime.setCameraPresentationPreferences(
                        {35.0f, 1.0f, true});
                    SettingsFixture settings("billiardgl_screen_pan.json");
                    ProductInputController controller(runtime, settings.runtime);
                    controller.setLogicalViewportHeight(height);
                    expect(runtime.setCameraPresetInstant(camera::SemanticState::ObserveFree,
                        preset).ok, "pan camera preset is available");
                    GameAction orbit;
                    orbit.type = ActionType::OrbitCamera;
                    orbit.first = angle * 1.57079632679f;
                    expect(runtime.dispatch(orbit).ok, "pan fixture rotates around table");
                    GameAction zoom;
                    zoom.type = ActionType::ZoomCamera;
                    zoom.first = angle % 2 ? 40.0f : -20.0f;
                    expect(runtime.dispatch(zoom).ok, "pan fixture changes viewing distance");
                    const auto anchor = runtime.cameraFrame().pose.target;
                    const auto project = [&]() {
                        const auto& pose = runtime.cameraFrame().pose;
                        CameraPresentation camera;
                        camera.eye = {pose.eye.x, pose.eye.y, pose.eye.z};
                        camera.target = {pose.target.x, pose.target.y, pose.target.z};
                        camera.fieldOfViewDegrees = pose.fieldOfViewDegrees;
                        const auto projection = render::buildSharedCameraProjection(camera,
                            height * 16 / 9 * backing, height * backing);
                        render::PhysicalScreenPoint point;
                        expect(render::clipToPhysicalScreen(projection,
                            render::transformWorldToClip(projection, {anchor.x, anchor.y, anchor.z}), point),
                            "pan anchor projects on screen");
                        return point;
                    };
                    const auto first = project();
                    const auto yaw = runtime.shotControl().yawRadians;
                    const double dx = angle % 2 ? -2.0 : 2.0;
                    const double dy = angle < 2 ? -3.0 : 3.0;
                    expect(controller.process(input("Shift+LeftDrag", "continuous", dx, dy, "mouse")).ok,
                        "screen-relative pan dispatches");
                    const auto last = project();
                    expect(std::fabs((last.x-first.x)/backing - dx) < 0.08 &&
                        std::fabs((last.y-first.y)/backing - dy) < 0.08,
                        "table follows mouse in logical pixels at every heading, pitch, viewport and backing scale");
                    expect(runtime.shotControl().yawRadians == yaw && !runtime.state().ballsMoving,
                        "pan does not aim or shoot");
                }
            }
        }
    }
}

void testTrainingContextAndReset()
{
    billiardgl::GameRuntime authority;
    billiardgl::training::TrainingProductController training(authority);
    const std::filesystem::path manifest =
        std::filesystem::path(BILLIARDGL_SOURCE_ROOT) /
        "assets/training/phase4/training_pack_v1.json";
    expect(training.loadPack(manifest.string(), allCapabilities()).ok &&
        training.startScenario(training.scenarios().front().id,
            "product-input-test-v1", 88).ok,
        "training fixture starts through product controller");
    const float initialYaw = authority.state().aim.yaw;
    SettingsFixture settings("billiardgl_product_input_training_v1.json");
    ProductInputController controller(authority, settings.runtime, &training);
    controller.setSurface(ProductInputSurface::Training);

    expect(controller.process(input("Tab")).ok &&
        controller.process(input(
            "MoveX", "continuous", 200.0, 0.0, "mouse")).ok &&
        std::fabs(authority.state().aim.yaw - initialYaw) > 0.1f,
        "training context retains normal aim controls");
    const auto tipBefore = authority.shotControl();
    const auto lockedTip = controller.process(input("ArrowKeys", "continuous", 1.0, 1.0));
    expect(lockedTip.ok && lockedTip.action == "training_control_locked" &&
        authority.shotControl().tipOffsetX == tipBefore.tipOffsetX &&
        authority.shotControl().tipOffsetY == tipBefore.tipOffsetY,
        "scenario-locked center tip rejects adjustments without error or authority mutation");
    expect(controller.process(input("+")).ok,
        "training shot receives non-zero authority power");
    const auto shot = controller.process(input(
        "LeftButton", "release", 0.0, 0.0, "mouse"));
    expect(shot.ok && authority.state().ballsMoving &&
        training.snapshot().shotInProgress &&
        !training.snapshot().shotPrepared,
        "successful training Shoot marks retry before dispatch and confirms lifecycle after dispatch");
    while (authority.state().ballsMoving) {
        expect(authority.step(1).ok && training.consumeAuthoritySteps().ok,
            "training input shot feeds retained authority steps to lifecycle");
    }
    expect(training.snapshot().attemptFacts.shotCount == 1,
        "training input route completes one actual stopped shot");
    if (training.snapshot().state == billiardgl::training::TrainingProductState::AttemptActive) {
        billiardgl::training::AttemptFacts terminal;
        terminal.state = authority.state();
        terminal.numericalFailure = true;
        expect(training.finishAttempt(terminal).ok, "terminal input fixture evaluates");
    }
    if (authority.state().aim.mode != billiardgl::AimMode::Aim)
        expect(controller.process(input("Tab")).ok, "terminal fixture enters aim");
    const auto before = billiardgl::replay::replayAuthorityStateSha256(authority.state());
    const auto terminalPower = authority.shotControl().requestedPower;
    const auto lockedPower = controller.process(input("+"));
    expect(lockedPower.ok && lockedPower.action == "training_control_locked" &&
        authority.shotControl().requestedPower == terminalPower,
        "completed training locks power even if the user re-enters AIM");
    for (int index = 0; index < 4; ++index) {
        const auto ignored = controller.process(input("LeftButton", "release", 0, 0, "mouse"));
        expect(ignored.ok && ignored.action == "training_attempt_finished" &&
            !authority.state().ballsMoving &&
            before == billiardgl::replay::replayAuthorityStateSha256(authority.state()),
            "repeated terminal clicks neither shoot nor raise an inactive error");
    }
    expect(controller.process(input("R")).ok &&
        std::fabs(authority.state().aim.yaw - initialYaw) < 0.00001f &&
        training.snapshot().state ==
            billiardgl::training::TrainingProductState::AttemptActive,
        "training reset binding delegates to TrainingProductController");
    expect(training.abandonAttempt().ok && training.startScenario(
        "launch_follow_shot_v1", "input-training-axis", 0).ok &&
        controller.process(input("Tab")).ok,
        "vertical tip lesson enters AIM");
    const auto axisBefore = authority.shotControl();
    const auto horizontalOnly = controller.process(input("ArrowKeys", "continuous", 1.0, 0.0));
    expect(horizontalOnly.ok && horizontalOnly.action == "training_control_locked" &&
        authority.shotControl().tipOffsetX == axisBefore.tipOffsetX,
        "vertical-only lesson does not accept horizontal English");
    expect(controller.process(input("ArrowKeys", "continuous", 0.0, -1.0)).ok &&
        authority.shotControl().tipOffsetY < axisBefore.tipOffsetY &&
        authority.shotControl().tipOffsetX == axisBefore.tipOffsetX,
        "vertical-only lesson accepts the allowed longitudinal adjustment");
}

void testTipOffsetUsesProfileRadialLimit()
{
    billiardgl::GameRuntime authority;
    SettingsFixture settings("billiardgl_product_input_tip_limit_v1.json");
    ProductInputController controller(authority, settings.runtime);
    expect(controller.process(input("Tab")).ok,
        "tip-limit fixture enters aim mode");
    for (int index = 0; index < 40; ++index) {
        expect(controller.process(input(
            "ArrowKeys", "continuous", 1.0, 1.0)).ok,
            "repeated diagonal tip adjustment remains valid");
    }
    const double magnitude = std::hypot(
        authority.shotControl().tipOffsetX,
        authority.shotControl().tipOffsetY);
    expect(magnitude <=
            authority.physicsProfile().cue.maximumReliableOffsetRadius + 1e-6,
        "tip adjustment clamps radially to the active physics profile");
}

billiardgl::replay::Document replayDocument(
    billiardgl::PhysicsProfile& profile, std::string& executable,
    std::string& content)
{
    billiardgl::GameRuntime runtime;
    executable = "product-input-replay-v1";
    content = billiardgl::sha256Hex("product-input-replay-content");
    billiardgl::replay::Recorder recorder;
    expect(recorder.begin(runtime, executable, content, 91).ok,
        "replay recorder starts");
    billiardgl::GameAction power;
    power.type = billiardgl::ActionType::SetShotPower;
    power.first = 45.0f;
    billiardgl::GameAction shoot;
    shoot.type = billiardgl::ActionType::Shoot;
    expect(runtime.dispatch(power).ok && runtime.dispatch(shoot).ok &&
        runtime.step(8).ok && recorder.finish(runtime, 1).ok,
        "replay fixture records one authoritative shot");
    profile = runtime.physicsProfile();
    return recorder.document();
}

void testReplayContextAndAuthorityIsolation()
{
    billiardgl::PhysicsProfile profile;
    std::string executable;
    std::string content;
    const billiardgl::replay::Document document =
        replayDocument(profile, executable, content);
    billiardgl::replay::ReplayTransportModel replay;
    expect(replay.load(document, profile, executable, content).ok,
        "replay fixture loads");

    billiardgl::GameRuntime authority;
    const std::string beforeHash = billiardgl::gameStateSha256(authority.state());
    const std::uint64_t beforeTick = authority.tick();
    const std::size_t beforeJournal =
        authority.appliedSemanticActions().size();
    SettingsFixture settings("billiardgl_product_input_replay_v1.json");
    ProductInputController controller(authority, settings.runtime, nullptr, &replay);
    controller.setSurface(ProductInputSurface::Replay);

    const auto defaultView = replay.presentationCameraFrame().pose;
    const auto replayHash = billiardgl::gameStateSha256(replay.player().runtime().state());
    expect(controller.process(input("Shift+LeftDrag", "continuous", 20, -10, "mouse")).ok &&
        controller.process(input("WheelUp", "step", 1, 0, "mouse")).ok,
        "replay accepts Shift-drag orbit and wheel zoom while paused");
    const auto movedView = replay.presentationCameraFrame().pose;
    expect(std::fabs(movedView.yawRadians - defaultView.yawRadians) > 0.05f &&
        movedView.distanceCm < defaultView.distanceCm &&
        replayHash == billiardgl::gameStateSha256(replay.player().runtime().state()),
        "camera moves without changing replay physics");

    expect(controller.process(input("Space")).ok &&
        !replay.snapshot().paused,
        "replay context gives Space to transport instead of play camera binding");
    expect(controller.process(input("+")).ok &&
        replay.snapshot().playbackRate == 2.0,
        "replay rate binding delegates to fixed-step transport policy");
    expect(replay.presentationCameraFrame().pose.yawRadians == movedView.yawRadians,
        "play and rate changes preserve the chosen view");
    expect(replay.resetCamera().ok &&
        replay.presentationCameraFrame().pose.yawRadians == defaultView.yawRadians &&
        replay.presentationCameraFrame().pose.distanceCm == defaultView.distanceCm,
        "reset restores the default camera independently of playback");
    expect(controller.process(input("C")).ok &&
        replay.snapshot().cameraMode ==
            billiardgl::replay::ReplayCameraMode::Broadcast,
        "replay camera binding delegates to transport model");
    expect(controller.process(input(".")).ok &&
        replay.snapshot().tick == 1,
        "replay step binding advances isolated replay authority");
    expect(controller.process(input("PageUp")).ok &&
        replay.snapshot().tick == 0,
        "replay seek binding reconstructs shot zero");
    const auto missingShot = controller.process(input("PageDown"));
    expect(!missingShot.ok && missingShot.reasonCode == "replay_shot_not_found",
        "invalid replay seek preserves stable transport reason");
    expect(controller.process(input("H")).ok,
        "global bindings remain available as replay fallback");
    expect(beforeHash == billiardgl::gameStateSha256(authority.state()) &&
        beforeTick == authority.tick() &&
        beforeJournal == authority.appliedSemanticActions().size(),
        "replay inputs never mutate play authority hash, tick, or journal");
}

}  // namespace

int main()
{
    try {
        testSettingsModalAndImmediateRebind();
        testMouseAngularUnits();
        testScreenRelativeMousePan();
        testAimTogglePreservesViewedDirection();
        testPlaytestDiagnostics();
        testFullEvidenceLineageAndDiskFailure();
        testPlaytestSameIdProfileReplacement();
        testPlaytestRulesDiagnostics();
        testAimPowerShootAndFailureIsolation();
        testTrainingContextAndReset();
        testTipOffsetUsesProfileRadialLimit();
        testReplayContextAndAuthorityIsolation();
        std::cout << "Product normalized input controller checks passed\n";
        return EXIT_SUCCESS;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return EXIT_FAILURE;
    }
}
