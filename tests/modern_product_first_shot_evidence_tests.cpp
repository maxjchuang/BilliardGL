#include "automation_json.h"
#include "product_settings.h"
#include "render/modern/modern_product_session.h"

#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <stdexcept>
#include <string>

namespace {

using namespace billiardgl;
using namespace billiardgl::render;

constexpr auto kWallBudget = std::chrono::milliseconds(30000);

void require(bool value, const std::string& message)
{
    if (!value) throw std::runtime_error(message);
}

const char* semanticActionName(AppliedSemanticActionType type)
{
    switch (type) {
    case AppliedSemanticActionType::SetAimYaw: return "set_aim_yaw";
    case AppliedSemanticActionType::SetShotPower: return "set_shot_power";
    case AppliedSemanticActionType::SetTipOffset: return "set_tip_offset";
    case AppliedSemanticActionType::SetCalledPocket: return "set_called_pocket";
    case AppliedSemanticActionType::Shoot: return "shoot";
    }
    return "unknown";
}

json::Value shotControlValue(const ShotControlModel& model)
{
    json::Value value = json::Value::object();
    value["schema_version"] = json::Value(
        static_cast<int>(model.schemaVersion));
    value["aim_yaw_radians"] = json::Value(model.yawRadians);
    value["requested_power"] = json::Value(model.requestedPower);
    value["tip_offset_x_radius"] = json::Value(model.tipOffsetX);
    value["tip_offset_y_radius"] = json::Value(model.tipOffsetY);
    value["assist_level"] = json::Value(static_cast<int>(model.assistLevel));
    return value;
}

struct Point {
    double x = 0.0;
    double y = 0.0;
};

class EvidenceRecorder {
public:
    EvidenceRecorder(GameRuntime& runtime, ModernProductSession& session,
        std::filesystem::path path)
        : runtime_(runtime), session_(session), path_(std::move(path)),
          started_(std::chrono::steady_clock::now())
    {
        root_["format"] = json::Value(
            "billiardgl_engineering_first_shot_proxy_v1");
        root_["evidence_type"] = json::Value("engineering_proxy");
        root_["user_research"] = json::Value(false);
        root_["phase3_five_plus_three_user_study"] = json::Value("NOT_RUN");
        root_["wall_time_budget_ms"] = json::Value(30000);
        root_["product_input_only"] = json::Value(true);
        root_["direct_game_state_mutation"] = json::Value(false);
        root_["private_shot_mutation"] = json::Value(false);
        root_["scenario_jump"] = json::Value(false);
        root_["ai_poll_calls"] = json::Value(0);
        root_["timeline"] = json::Value::array();
    }

    double elapsedMilliseconds() const
    {
        return std::chrono::duration<double, std::milli>(
            std::chrono::steady_clock::now() - started_).count();
    }

    bool withinBudget() const
    {
        return std::chrono::steady_clock::now() - started_ < kWallBudget;
    }

    void observation(const std::string& action,
        const std::string& detail = {})
    {
        json::Value entry = baseEntry(action, "observation");
        entry["detail"] = json::Value(detail);
        root_["timeline"].asArray().push_back(entry);
        trackHelp();
    }

    void platformResult(const std::string& action,
        const std::string& intendedSemantic,
        const PlatformInputEvent& input,
        const ModernProductEventResult& result)
    {
        json::Value entry = baseEntry(action, "platform_input");
        entry["intended_semantic"] = json::Value(intendedSemantic);
        entry["control"] = json::Value(input.control);
        entry["logical_x"] = json::Value(input.logicalX);
        entry["logical_y"] = json::Value(input.logicalY);
        entry["delta_x"] = json::Value(input.deltaX);
        entry["delta_y"] = json::Value(input.deltaY);
        entry["scroll_y"] = json::Value(input.scrollY);
        entry["backing_scale"] = json::Value(input.backingScale);
        entry["public_result_ok"] = json::Value(result.ok);
        entry["public_result_handled"] = json::Value(result.handled);
        entry["public_error_code"] = json::Value(result.errorCode);
        entry["public_app_request"] = json::Value(
            static_cast<int>(result.appRequest));
        root_["timeline"].asArray().push_back(entry);
        trackHelp();
    }

    void stepResult(int count, std::uint64_t tickBefore,
        const ActionResult& result)
    {
        json::Value entry = baseEntry("authority_step_batch", "authority_step");
        entry["requested_ticks"] = json::Value(count);
        entry["tick_before"] = json::Value(static_cast<double>(tickBefore));
        entry["tick_after"] = json::Value(static_cast<double>(runtime_.tick()));
        entry["public_result_ok"] = json::Value(result.ok);
        entry["public_error_code"] = json::Value(result.errorCode);
        root_["timeline"].asArray().push_back(entry);
        trackHelp();
    }

    void finish(bool passed, const std::string& failure = {})
    {
        const double wall = elapsedMilliseconds();
        root_["result"] = json::Value(passed ? "PASS" : "FAIL");
        root_["failure"] = json::Value(failure);
        root_["wall_time_ms"] = json::Value(wall);
        root_["within_wall_time_budget"] = json::Value(wall <= 30000.0);
        root_["help_opened"] = json::Value(helpOpened_);
        root_["final_authority_tick"] = json::Value(
            static_cast<double>(runtime_.tick()));
        root_["final_shot_control"] = shotControlValue(runtime_.shotControl());
        root_["final_balls_moving"] = json::Value(runtime_.state().ballsMoving);

        json::Value actions = json::Value::array();
        for (const AppliedSemanticAction& action :
                runtime_.appliedSemanticActions()) {
            json::Value item = json::Value::object();
            item["sequence"] = json::Value(static_cast<double>(action.sequence));
            item["authority_tick"] = json::Value(static_cast<double>(action.tick));
            item["type"] = json::Value(semanticActionName(action.type));
            item["first"] = json::Value(action.first);
            item["second"] = json::Value(action.second);
            actions.asArray().push_back(item);
        }
        root_["authority_semantic_actions"] = actions;

        json::Value events = json::Value::array();
        for (const RuntimeEvent& event : runtime_.events()) {
            json::Value item = json::Value::object();
            item["sequence"] = json::Value(static_cast<double>(event.sequence));
            item["authority_tick"] = json::Value(static_cast<double>(event.tick));
            item["name"] = json::Value(event.name);
            events.asArray().push_back(item);
        }
        root_["authority_events"] = events;

        std::filesystem::create_directories(path_.parent_path());
        std::ofstream output(path_, std::ios::binary | std::ios::trunc);
        output << json::stringify(root_) << '\n';
        if (!output) throw std::runtime_error(
            "could not write first-shot engineering evidence");
    }

    json::Value& root() { return root_; }
    const std::filesystem::path& path() const { return path_; }

private:
    json::Value baseEntry(const std::string& action, const char* kind)
    {
        json::Value entry = json::Value::object();
        entry["sequence"] = json::Value(++sequence_);
        entry["elapsed_ms"] = json::Value(elapsedMilliseconds());
        entry["kind"] = json::Value(kind);
        entry["action"] = json::Value(action);
        entry["screen"] = json::Value(
            productScreenId(session_.snapshot().shell.screen));
        entry["authority_tick"] = json::Value(
            static_cast<double>(runtime_.tick()));
        entry["balls_moving"] = json::Value(runtime_.state().ballsMoving);
        entry["aim_mode"] = json::Value(
            runtime_.state().aim.mode == AimMode::Aim ? "aim" : "observe");
        entry["shot_control"] = shotControlValue(runtime_.shotControl());
        return entry;
    }

    void trackHelp()
    {
        helpOpened_ = helpOpened_ || runtime_.state().hud.showHelp;
    }

    GameRuntime& runtime_;
    ModernProductSession& session_;
    std::filesystem::path path_;
    std::chrono::steady_clock::time_point started_;
    json::Value root_ = json::Value::object();
    int sequence_ = 0;
    bool helpOpened_ = false;
};

Point itemCenter(const ModernProductSession& session, const std::string& id)
{
    const ui::ShellScreenLayout layout = session.shellLayout();
    require(layout.ok, "shipping shell layout is invalid");
    const auto found = std::find_if(layout.elements.begin(),
        layout.elements.end(), [&](const ui::ShellElementLayout& item) {
            return item.id == id;
        });
    require(found != layout.elements.end(), "missing shell item " + id);
    return {
        (found->rect.x + found->rect.width * 0.5) *
            layout.viewport.logicalWidth / layout.viewport.physicalWidth,
        (found->rect.y + found->rect.height * 0.5) *
            layout.viewport.logicalHeight / layout.viewport.physicalHeight};
}

ModernProductEventResult submit(ModernProductSession& session,
    EvidenceRecorder& evidence, PlatformInputEvent event,
    const std::string& label, const std::string& semantic)
{
    const ModernProductEventResult result = session.handlePlatformInput(event);
    evidence.platformResult(label, semantic, event, result);
    require(evidence.withinBudget(), "30-second wall-time budget exceeded");
    return result;
}

ModernProductEventResult click(ModernProductSession& session,
    EvidenceRecorder& evidence, const std::string& id)
{
    const Point point = itemCenter(session, id);
    const auto viewport = session.snapshot().shellViewport;
    const double scale = viewport.logicalWidth > 0.0f ?
        static_cast<double>(viewport.physicalWidth) / viewport.logicalWidth : 1.0;

    PlatformInputEvent move;
    move.kind = PlatformInputKind::PointerMove;
    move.logicalX = point.x;
    move.logicalY = point.y;
    move.backingScale = scale;
    require(submit(session, evidence, move, "pointer_move:" + id,
        "focus_shell_item").handled, "pointer move was not handled for " + id);

    PlatformInputEvent down = move;
    down.kind = PlatformInputKind::PointerButtonDown;
    down.control = "LeftButton";
    require(submit(session, evidence, down, "pointer_down:" + id,
        "capture_shell_item").handled, "pointer down was not handled for " + id);

    PlatformInputEvent up = down;
    up.kind = PlatformInputKind::PointerButtonUp;
    return submit(session, evidence, up, "pointer_release:" + id,
        "activate_shell_item");
}

PlatformInputEvent key(const std::string& control)
{
    PlatformInputEvent event;
    event.kind = PlatformInputKind::KeyDown;
    event.control = control;
    return event;
}

std::filesystem::path evidencePath()
{
#ifdef BILLIARDGL_FIRST_SHOT_EVIDENCE_ROOT
    return std::filesystem::path(BILLIARDGL_FIRST_SHOT_EVIDENCE_ROOT) /
        "engineering-first-shot-proxy-v1.json";
#else
    return std::filesystem::temp_directory_path() /
        "billiardgl-engineering-first-shot-proxy-v1.json";
#endif
}

void runFirstShotProxy(EvidenceRecorder& evidence,
    GameRuntime& runtime, ai::AiPlayerController& ai,
    ModernProductSession& session)
{
    evidence.root()["coverage"] = json::Value(
        "single hybrid session: Retina shell pointer plus keyboard/mouse gameplay");
    evidence.root()["not_covered"] = json::Value(
        "native Cocoa event synthesis or a second keyboard-only full session");
    evidence.root()["assist_can_auto_aim_or_shoot"] = json::Value(
        settings::assistCanAutoAimOrShoot(session.settings().current()));
    evidence.observation("session_created", "main menu, no gameplay mutation");

    require(click(session, evidence, "single_player").ok &&
        session.snapshot().shell.screen == ProductScreen::SinglePlayerSetup,
        "Main to SinglePlayerSetup failed");
    const ProductShellSnapshot setup = session.snapshot().shell;
    require(setup.rulesetId == ProductShellController::kApprovedRulesetId &&
        setup.rulesetLabel == "WPA Heyball",
        "approved WPA Heyball profile is not selected");
    evidence.root()["ruleset_id"] = json::Value(setup.rulesetId);
    evidence.root()["ruleset_label"] = json::Value(setup.rulesetLabel);
    evidence.observation("approved_rules_verified", setup.rulesetId);

    require(click(session, evidence, "expert").ok &&
        session.snapshot().shell.aiDifficulty == ai::Difficulty::Expert,
        "approved Expert AI selection failed");
    require(click(session, evidence, "start_match").ok &&
        session.snapshot().shell.screen == ProductScreen::Match,
        "SinglePlayerSetup to Match failed");
    const ai::PlayerState configuredAi = ai.state();
    require(configuredAi.configuration.enabled &&
        configuredAi.configuration.player == 1 &&
        configuredAi.configuration.difficulty == ai::Difficulty::Expert &&
        configuredAi.jobsStarted == 0,
        "AI configuration is not the selected non-player turn owner");
    require(runtime.rulesMode() == RulesMode::ChineseEightBallV1 &&
        runtime.matchState().currentPlayer == 0,
        "Match did not enter approved rules with player zero at the table");

    const ShotControlModel before = runtime.shotControl();
    evidence.root()["shot_control_before"] = shotControlValue(before);
    evidence.observation("observe_match",
        "player inspects authoritative opening layout before aiming");
    require(runtime.state().aim.mode == AimMode::Observe,
        "match must begin in observe mode");

    const ModernProductEventResult aim = submit(session, evidence, key("Tab"),
        "key_down:Tab", "toggle_aim");
    require(aim.ok && aim.handled && runtime.state().aim.mode == AimMode::Aim,
        "Tab did not enter aim mode through product input");

    PlatformInputEvent move;
    move.kind = PlatformInputKind::PointerMove;
    // Native mouse deltas are logical points. Exercise a deliberate visible
    // aim gesture rather than a sub-pixel nudge: 0.30 points only produces
    // 0.0006 radians at the shipping 0.002 rad/point sensitivity and cannot
    // satisfy this proxy's intentionally meaningful >0.01-radian evidence.
    move.deltaX = 12.0;
    move.deltaY = 0.0;
    move.backingScale = 2.0;
    const ModernProductEventResult adjusted = submit(session, evidence, move,
        "pointer_move:aim", "adjust_aim");
    require(adjusted.ok && adjusted.handled &&
        std::fabs(runtime.shotControl().yawRadians - before.yawRadians) > 0.01f,
        "mouse motion did not set a non-default aim yaw");

    require(submit(session, evidence, key("ArrowRight"),
        "key_down:ArrowRight", "adjust_tip_offset").ok,
        "right tip input failed");
    require(submit(session, evidence, key("ArrowDown"),
        "key_down:ArrowDown", "adjust_tip_offset").ok,
        "down tip input failed");
    require(std::fabs(runtime.shotControl().tipOffsetX) > 0.001f &&
        std::fabs(runtime.shotControl().tipOffsetY) > 0.001f,
        "tip offset remained at the default center");

    PlatformInputEvent wheel;
    wheel.kind = PlatformInputKind::Scroll;
    wheel.scrollY = 2.0;
    require(submit(session, evidence, wheel, "scroll:WheelUp",
        "power_increase").ok &&
        std::fabs(runtime.shotControl().requestedPower - before.requestedPower) >
            0.001f,
        "wheel input did not set non-default power");
    evidence.root()["shot_control_before_release"] =
        shotControlValue(runtime.shotControl());

    PlatformInputEvent release;
    release.kind = PlatformInputKind::PointerButtonUp;
    release.control = "LeftButton";
    release.backingScale = 2.0;
    const ModernProductEventResult shot = submit(session, evidence, release,
        "pointer_release:LeftButton", "shoot");
    require(shot.ok && shot.handled && runtime.state().ballsMoving,
        "left release did not produce an authoritative shot");
    require(runtime.hasCueImpactInput() && runtime.hasCueContactResult() &&
        runtime.cueContactResult().applied &&
        runtime.cueContactResult().error.empty(),
        "authority did not confirm an applied cue contact");
    evidence.observation("authority_shot_confirmed",
        cueContactRegimeName(runtime.cueContactResult().regime));

    int steppedTicks = 0;
    while (runtime.state().ballsMoving) {
        require(evidence.withinBudget(),
            "30-second wall-time budget exceeded before post-shot");
        const std::uint64_t beforeTick = runtime.tick();
        const ActionResult step = runtime.step(16);
        steppedTicks += 16;
        if (!step.ok || !runtime.state().ballsMoving)
            evidence.stepResult(16, beforeTick, step);
        require(step.ok, std::string("authority physics step failed: ") +
            step.errorCode);
        require(steppedTicks <= 20000,
            "post-shot did not settle inside deterministic tick guard");
    }

    const bool shotEnded = std::any_of(runtime.events().begin(),
        runtime.events().end(), [](const RuntimeEvent& event) {
            return event.name == "shot_ended";
        });
    const std::size_t shootCount = static_cast<std::size_t>(std::count_if(
        runtime.appliedSemanticActions().begin(),
        runtime.appliedSemanticActions().end(),
        [](const AppliedSemanticAction& action) {
            return action.type == AppliedSemanticActionType::Shoot;
        }));
    require(shotEnded && shootCount == 1,
        "post-shot requires one shoot semantic action and a shot_ended event");
    require(!runtime.state().hud.showHelp,
        "help was unexpectedly opened during first-shot flow");
    const ai::PlayerState finalAi = ai.state();
    require(finalAi.jobsStarted == 0,
        "AI/assist must not aim or shoot for the player proxy");
    require(!settings::assistCanAutoAimOrShoot(session.settings().current()),
        "product assist unexpectedly gained auto-aim or auto-shoot authority");
    evidence.root()["post_shot_settled_ticks"] = json::Value(steppedTicks);
    evidence.root()["authority_shoot_action_count"] = json::Value(
        static_cast<int>(shootCount));
    evidence.root()["authority_cue_contact_applied"] = json::Value(true);
    evidence.root()["authority_shot_ended_event"] = json::Value(true);
    evidence.observation("post_shot",
        "balls settled and authority published shot_ended");
}

}  // namespace

int main()
{
    const std::filesystem::path output = evidencePath();
    const std::filesystem::path settingsPath = output.parent_path() /
        "engineering-first-shot-settings.json";
    std::error_code ignored;
    std::filesystem::remove(settingsPath, ignored);
    std::filesystem::remove(settingsPath.string() + ".tmp", ignored);

    GameRuntime runtime;
    ai::AiPlayerController ai;
    ModernProductSessionOptions options;
    options.settingsPath = settingsPath.string();
    // Match startup begins the real single-rack recorder. Supply this
    // engineering fixture's identity instead of bypassing its binding checks.
    options.executableVersion = "engineering-first-shot-proxy-v1";
    ModernProductSession session(runtime, ai, std::move(options));
    session.setShellViewport({1280, 720, 2560, 1440, 192.0f});
    EvidenceRecorder evidence(runtime, session, output);

    try {
        runFirstShotProxy(evidence, runtime, ai, session);
        evidence.finish(true);
        std::filesystem::remove(settingsPath, ignored);
        std::filesystem::remove(settingsPath.string() + ".tmp", ignored);
        std::cout << "Engineering first-shot proxy evidence: "
                  << evidence.path() << '\n';
        return EXIT_SUCCESS;
    } catch (const std::exception& error) {
        try {
            evidence.finish(false, error.what());
        } catch (const std::exception& writeError) {
            std::cerr << "evidence write failure: " << writeError.what() << '\n';
        }
        std::filesystem::remove(settingsPath, ignored);
        std::filesystem::remove(settingsPath.string() + ".tmp", ignored);
        std::cerr << "Engineering first-shot proxy failed: "
                  << error.what() << '\n';
        return EXIT_FAILURE;
    }
}
