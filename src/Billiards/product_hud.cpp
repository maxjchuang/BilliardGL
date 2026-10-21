#include "product_hud.h"

#include "automation_json.h"
#include "camera/camera_state_machine.h"

#include <algorithm>
#include <cctype>
#include <cmath>
#include <fstream>
#include <iomanip>
#include <iterator>
#include <sstream>
#include <stdexcept>

namespace billiardgl::product_hud {
namespace {

constexpr std::size_t kDefaultLongestHelpLineCharacters = 24;
constexpr float kHelpLineCount = 7.0f;
constexpr float kHelpLineStepEm = 1.8f;
// The shipping HUD uses the frozen Menlo SDF atlas.  Its advance is about
// 0.60 em; 0.64 em retains shaping/SDF headroom without returning to a modal
// whose empty area is several times larger than its seven lines of content.
constexpr float kHelpAdvanceBudgetEm = 0.64f;
constexpr float kHelpPhysicalInsetPx = 18.0f;

const settings::InputBinding* firstBinding(
    const settings::ProductSettings& settings, const char* action)
{
    const auto found = std::find_if(settings.bindings.begin(),
        settings.bindings.end(), [&](const settings::InputBinding& binding) {
            return binding.action == action;
        });
    return found == settings.bindings.end() ? nullptr : &*found;
}

std::string uppercaseAscii(std::string value)
{
    std::transform(value.begin(), value.end(), value.begin(),
        [](unsigned char character) {
            return static_cast<char>(std::toupper(character));
        });
    return value;
}

std::string bindingLabel(const settings::InputBinding* binding)
{
    if (binding == nullptr) return "UNBOUND";
    if (binding->control == "LeftButton") return "MOUSE LEFT";
    if (binding->control == "RightButton") return "MOUSE RIGHT";
    if (binding->control == "MiddleButton") return "MOUSE MIDDLE";
    if (binding->control == "LeftDrag") return "MOUSE DRAG";
    if (binding->control == "ArrowKeys") return "ARROWS";
    if (binding->control == "WheelUp") return "WHEEL UP";
    if (binding->control == "WheelDown") return "WHEEL DOWN";
    if (binding->control == "Escape") return "ESC";
    return uppercaseAscii(binding->control);
}

std::string actionLine(const settings::ProductSettings& settings,
    const char* action, const char* description)
{
    return bindingLabel(firstBinding(settings, action)) + "  " + description;
}

DisplayField field(bool available, const std::string& value,
    const char* fallback)
{
    return {available, available ? value : fallback};
}

std::string fixed(float value, int precision)
{
    std::ostringstream output;
    output << std::fixed << std::setprecision(precision) << value;
    return output.str();
}

bool validPlayer(int player) { return player >= 0 && player <= 1; }

std::string cameraText(std::uint32_t preset)
{
    switch (static_cast<camera::Preset>(preset)) {
    case camera::Preset::PlayerLow: return "Player camera";
    case camera::Preset::Elevated: return "Elevated camera";
    case camera::Preset::Overhead: return "Overhead camera";
    case camera::Preset::Broadcast: return "Broadcast camera";
    }
    return std::string();
}

std::string assistText(std::uint32_t assist)
{
    switch (assist) {
    case 0: return "Assist off";
    case 1: return "Basic assist";
    case 2: return "Training assist";
    default: return std::string();
    }
}

RuleNotice noticeForCode(const std::string& code)
{
    if (code == "both_feet_off_ground_during_shot")
        return {code, "Both feet were off the ground during the shot. Opponent has ball in hand.",
            "warning_triangle", "outlined_triangle"};
    if (code == "illegal_body_clothing_or_equipment_ball_touch")
        return {code, "A ball was touched illegally by body, clothing or equipment. Opponent has ball in hand.",
            "warning_triangle", "outlined_triangle"};
    if (code == "one_hand_shot")
        return {code, "The shot was played with one hand. Opponent has ball in hand.",
            "warning_triangle", "outlined_triangle"};
    if (code == "auxiliary_equipment_placed_on_table_to_shoot")
        return {code, "Auxiliary equipment was placed on the table to shoot. Opponent has ball in hand.",
            "warning_triangle", "outlined_triangle"};
    if (code == "mobile_device_disturbance_general_foul")
        return {code, "Mobile-device disturbance: general foul. Opponent has ball in hand.",
            "warning_triangle", "outlined_triangle"};
    if (code == "mobile_device_repeat_loss_of_rack")
        return {code, "Repeated mobile-device disturbance: loss of rack.",
            "warning_triangle", "outlined_triangle"};
    if (code == "illegal_break")
        return {code, "Break requirements were not met.",
            "warning_triangle", "outlined_triangle"};
    if (code == "no_object_ball_contact")
        return {code, "No object ball was contacted.",
            "no_contact", "crossed_circle"};
    if (code == "wrong_first_contact")
        return {code, "The wrong ball was contacted first.",
            "wrong_ball", "numbered_ball_cross"};
    if (code == "no_rail_after_contact")
        return {code, "No ball reached a rail after contact.",
            "rail_missing", "rail_with_slash"};
    if (code == "cue_ball_scratch")
        return {code, "The cue ball was pocketed.",
            "cue_scratch", "cue_ball_down_arrow"};
    if (code == "cue_ball_off_table")
        return {code, "The cue ball left the table.",
            "ball_off_table", "ball_outside_rectangle"};
    if (code == "object_ball_off_table")
        return {code, "An object ball left the table.",
            "object_ball_off_table", "numbered_ball_outside_rectangle"};
    if (code == "break_cue_ball_past_head_string")
        return {code, "The break began with the cue ball past the head string.",
            "warning_triangle", "outlined_triangle"};
    if (code == "double_hit")
        return {code, "The cue ball was struck more than once.",
            "warning_triangle", "outlined_triangle"};
    if (code == "push")
        return {code, "The stroke was judged to be a push shot.",
            "warning_triangle", "outlined_triangle"};
    if (code == "frozen_cushion_obligation")
        return {code, "The required cushion contact for a frozen ball was not made.",
            "rail_missing", "rail_with_slash"};
    if (code == "illegal_eight_ball")
        return {code, "The 8-ball was pocketed illegally.",
            "illegal_eight", "eight_ball_octagon"};
    return {code.empty() ? "unknown" : code, "Rule decision unavailable.",
        "rule_unknown", "question_diamond"};
}

std::string resolutionText(const RulesPresentation& rules)
{
    if (rules.rackEnded)
        return validPlayer(rules.rackWinner) ? "Rack ended — Player " +
            std::to_string(rules.rackWinner + 1) + " wins this rack." :
            "This rack has ended.";
    if (rules.gameOver && validPlayer(rules.winner))
        return "Game over — Player " + std::to_string(rules.winner + 1) +
            " wins.";
    if (rules.ballInHand) return "Opponent has ball in hand.";
    if (!rules.decisionOptions.empty())
        return "Choose whether to accept the table or rerack.";
    if (rules.continueTurn) return "Turn continues.";
    if (rules.hasAdjudication) return "Turn passes to the opponent.";
    return std::string();
}

HudRuntimeState stateFor(const PresentationSnapshot& snapshot)
{
    if (snapshot.hud.showHelp) return HudRuntimeState::Help;
    // A referee commit is not a physical shot-end pulse. Keep its explanation
    // visible during the resulting placement interaction, including stale aim.
    if (snapshot.rules.foul && snapshot.rules.ballInHand &&
        snapshot.rules.hasAdjudication &&
        std::any_of(snapshot.rules.reasonCodes.begin(), snapshot.rules.reasonCodes.end(),
            [](const std::string& code) {
                return code == "both_feet_off_ground_during_shot" ||
                    code == "illegal_body_clothing_or_equipment_ball_touch" ||
                    code == "one_hand_shot" ||
                    code == "auxiliary_equipment_placed_on_table_to_shoot" ||
                    code == "mobile_device_disturbance_general_foul";
            }))
        return HudRuntimeState::Foul;
    if (snapshot.hud.aimMode && !snapshot.ballsMoving &&
        !snapshot.rules.gameOver && !snapshot.rules.rackEnded)
        return HudRuntimeState::Aiming;
    const bool currentDecision = snapshot.events.shotEnded ||
        snapshot.rules.gameOver || snapshot.rules.rackEnded;
    if (currentDecision && snapshot.rules.foul) return HudRuntimeState::Foul;
    if (currentDecision) return HudRuntimeState::PostShot;
    return HudRuntimeState::Default;
}

void applyVisibility(ProductHudViewModel& model)
{
    if (model.state == HudRuntimeState::Help) {
        model.showHelp = true;
        return;
    }
    model.showPlayerStatus = true;
    model.showContextHint = true;
    if (model.state == HudRuntimeState::Aiming) {
        model.showPower = true;
        model.showTipOffset = true;
    } else if (model.state == HudRuntimeState::PostShot) {
        model.showShotResult = true;
    } else if (model.state == HudRuntimeState::Foul) {
        model.showFoul = true;
    }
}

HudRect parseRect(const json::Value& value)
{
    const auto& parts = value.asArray();
    if (parts.size() != 4) throw std::runtime_error("hud_manifest_rect");
    return {static_cast<float>(parts[0].asNumber()),
        static_cast<float>(parts[1].asNumber()),
        static_cast<float>(parts[2].asNumber()),
        static_cast<float>(parts[3].asNumber())};
}

HudProfileSpec parseProfile(const std::string& id,
    const json::Value& value)
{
    HudProfileSpec result;
    result.id = id;
    const auto& viewport = value.at("viewport_px").asArray();
    result.referenceWidth = viewport.at(0).asInt();
    result.referenceHeight = viewport.at(1).asInt();
    const json::Value& safe = value.at("safe_area_px");
    result.safeArea = {static_cast<float>(safe.at("left").asNumber()),
        static_cast<float>(safe.at("top").asNumber()),
        static_cast<float>(safe.at("right").asNumber()),
        static_cast<float>(safe.at("bottom").asNumber())};
    result.minimumTextPx = static_cast<float>(
        value.at("minimum_text_px").asNumber());
    for (const auto& item : value.at("element_rects_px").asObject())
        result.referenceRects[item.first] = parseRect(item.second);
    return result;
}

float clampScale(float value, float minimum, float maximum)
{
    return std::max(minimum, std::min(maximum, value));
}

HudRect physicalRect(const HudRect& logical, float scale)
{
    return {std::round(logical.x * scale), std::round(logical.y * scale),
        std::round(logical.width * scale),
        std::round(logical.height * scale)};
}

bool overlaps(const HudRect& left, const HudRect& right)
{
    return left.x < right.x + right.width &&
        left.x + left.width > right.x &&
        left.y < right.y + right.height &&
        left.y + left.height > right.y;
}

bool stateMatches(FirstShotEventType type, HudRuntimeState state)
{
    if (type == FirstShotEventType::HudDefaultVisible)
        return state == HudRuntimeState::Default;
    if (type == FirstShotEventType::AimToggled ||
        type == FirstShotEventType::PowerHoldStarted ||
        type == FirstShotEventType::PowerSampled)
        return state == HudRuntimeState::Aiming;
    if (type == FirstShotEventType::HudPostShotVisible)
        return state == HudRuntimeState::PostShot ||
            state == HudRuntimeState::Foul;
    return true;
}

}  // namespace

std::vector<std::string> buildHelpLines(
    const settings::ProductSettings& settings)
{
    const settings::InputBinding* shoot = firstBinding(settings, "shoot");
    const char* shootDescription = shoot != nullptr &&
            shoot->trigger == "release" ?
        "Release shot" : "Shoot";
    return {"QUICK CONTROLS",
        actionLine(settings, "toggle_aim", "Enter aim"),
        actionLine(settings, "shoot", shootDescription),
        actionLine(settings, "adjust_tip_offset", "Tip offset"),
        actionLine(settings, "camera_orbit", "Orbit camera"),
        bindingLabel(firstBinding(settings, "zoom_in")) + " / " +
            bindingLabel(firstBinding(settings, "zoom_out")) + "  Zoom",
        actionLine(settings, "toggle_help", "Close help")};
}

std::size_t longestHelpLineCharacters(
    const std::vector<std::string>& lines) noexcept
{
    std::size_t result = 0;
    for (const std::string& line : lines)
        result = std::max(result, line.size());
    return result;
}

ProductHudViewModel HudStateReducer::reduce(
    const PresentationSnapshot& snapshot,
    const HudProductCapabilities& capabilities)
{
    ProductHudViewModel result;
    result.state = stateFor(snapshot);
    applyVisibility(result);

    const int player = snapshot.rules.enabled ? snapshot.rules.currentPlayer :
        snapshot.hud.currentPlayerNumber - 1;
    result.player = field(validPlayer(player), validPlayer(player) ?
        "Player " + std::to_string(player + 1) : std::string(), "Player —");

    std::string group;
    if (snapshot.rules.enabled && validPlayer(player))
        group = snapshot.rules.groups[static_cast<std::size_t>(player)];
    if (group == "solids") {
        result.group = field(true, "SOLIDS", "Group —");
        result.groupShapeToken = "group_solid_filled_circle";
    } else if (group == "stripes") {
        result.group = field(true, "STRIPES", "Group —");
        result.groupShapeToken = "group_stripe_banded_ring";
    } else if (group == "open" || !snapshot.rules.enabled) {
        result.group = field(true, "OPEN TABLE", "Group —");
        result.groupShapeToken = "group_open_diamond";
    } else {
        result.group = field(false, "", "Group —");
        result.groupShapeToken = "group_unknown_question_diamond";
    }

    int target = -1;
    if (group == "solids" || group == "stripes") {
        const int first = group == "solids" ? 1 : 9;
        const int last = group == "solids" ? 7 : 15;
        for (int ball = first; ball <= last; ++ball) {
            if (!snapshot.balls[static_cast<std::size_t>(ball)].pocketed) {
                target = ball;
                break;
            }
        }
        if (target < 0 && !snapshot.balls[8].pocketed) target = 8;
    }
    if (group == "open" || !snapshot.rules.enabled)
        result.targetBall = field(true, "Any legal object ball", "Target —");
    else
        result.targetBall = field(target >= 0, target >= 0 ?
            "Ball " + std::to_string(target) : std::string(), "Target —");
    result.targetPocket = field(target >= 0 || group == "open" ||
        !snapshot.rules.enabled, "Any legal pocket", "Pocket —");

    const float power = snapshot.shotControl.requestedPower;
    const bool validPower = std::isfinite(power) && power >= 0.0f && power <= 200.0f;
    result.powerPercent = field(validPower,
        validPower ? std::to_string(static_cast<int>(std::round(power * 0.5f))) + "%" :
            std::string(), "—%");
    float speedMps = snapshot.shotControl.strokeSpeedCmS / 100.0f;
    if (snapshot.shotControl.powerMode == 0 && validPower)
        speedMps = power * 1.675f / 100.0f;
    const bool validSpeed = std::isfinite(speedMps) && speedMps >= 0.0f &&
        (snapshot.shotControl.powerMode != 0 || validPower);
    result.estimatedCueSpeed = field(validSpeed,
        validSpeed ? "EST. " + fixed(speedMps, 2) + " m/s" : std::string(),
        "EST. — m/s");

    const bool validTip = std::isfinite(snapshot.shotControl.tipOffsetX) &&
        std::isfinite(snapshot.shotControl.tipOffsetY) &&
        std::fabs(snapshot.shotControl.tipOffsetX) <= 1.0f &&
        std::fabs(snapshot.shotControl.tipOffsetY) <= 1.0f;
    result.tipOffset = field(validTip, validTip ?
        "X " + fixed(snapshot.shotControl.tipOffsetX, 2) + " / Y " +
            fixed(snapshot.shotControl.tipOffsetY, 2) : std::string(),
        "X — / Y —");
    result.tipOffsetX = validTip ? snapshot.shotControl.tipOffsetX : 0.0f;
    result.tipOffsetY = validTip ? snapshot.shotControl.tipOffsetY : 0.0f;

    const std::string camera = cameraText(snapshot.camera.preset);
    result.camera = field(!camera.empty(), camera, "Camera —");
    const std::string assist = assistText(snapshot.shotControl.assistLevel);
    result.assist = field(!assist.empty(), assist, "Assist —");

    const bool showCurrentDecision = result.state == HudRuntimeState::Foul ||
        result.state == HudRuntimeState::PostShot;
    if (showCurrentDecision) {
        for (const std::string& code : snapshot.rules.reasonCodes)
            result.ruleNotices.push_back(noticeForCode(code));
    }
    if (!result.ruleNotices.empty())
        result.foulReason = field(true, result.ruleNotices.front().playerText,
            "Rule decision unavailable.");
    else
        result.foulReason = field(false, "", "No current foul.");
    const std::string resolution = showCurrentDecision ?
        resolutionText(snapshot.rules) : std::string();
    result.foulResolution = field(!resolution.empty(), resolution,
        "Resolution unavailable.");
    if (snapshot.rules.rackEnded && validPlayer(snapshot.rules.rackWinner))
        result.result = field(true, "Player " +
            std::to_string(snapshot.rules.rackWinner + 1) + " wins rack", "Result —");
    else if (snapshot.rules.gameOver && validPlayer(snapshot.rules.winner))
        result.result = field(true, "Player " +
            std::to_string(snapshot.rules.winner + 1) + " wins", "Result —");
    else if (result.state == HudRuntimeState::PostShot)
        result.result = field(true, "Shot complete", "Result —");
    else
        result.result = field(false, "", "Result pending");

    result.showCueElevation = result.state == HudRuntimeState::Aiming &&
        capabilities.cueElevationUi &&
        capabilities.cueElevationPhysicsPromoted &&
        snapshot.hud.showCueElevation;
    result.showChalk = result.state == HudRuntimeState::Aiming &&
        capabilities.chalkUi && capabilities.chalkPhysicsPromoted &&
        snapshot.hud.showChalkState;
    return result;
}

ProductHudManifest loadProductHudManifest(const std::string& path)
{
    ProductHudManifest result;
    try {
        std::ifstream input(path, std::ios::binary);
        if (!input) {
            result.errorCode = "hud_manifest_io";
            return result;
        }
        const std::string bytes{std::istreambuf_iterator<char>(input),
            std::istreambuf_iterator<char>()};
        const json::ParseResult parsed = json::parse(bytes);
        if (!parsed.ok) {
            result.errorCode = "hud_manifest_json";
            return result;
        }
        const json::Value& root = parsed.value;
        if (root.at("schema_version").asInt() != 1 ||
            root.at("manifest_id").asString() != "focused_practice_hud_v1") {
            result.errorCode = "hud_manifest_schema";
            return result;
        }
        result.manifestId = root.at("manifest_id").asString();
        const auto& scaleRange = root.at("dpi_scaling")
            .at("user_scale_range").asArray();
        result.minimumUserScale = static_cast<float>(scaleRange.at(0).asNumber());
        result.maximumUserScale = static_cast<float>(scaleRange.at(1).asNumber());
        result.compact = parseProfile("compact_1024x768",
            root.at("profiles").at("compact_1024x768"));
        result.wide = parseProfile("wide_1920x1080",
            root.at("profiles").at("wide_1920x1080"));
        const json::Value& proxy = root.at("first_shot_automation_proxies");
        result.firstShot.deadlineMs = proxy.at("deadline_ms").asInt();
        result.firstShot.statusVisibleByMs =
            proxy.at("status_card_visible_by_ms").asInt();
        result.firstShot.aimEnteredByMs =
            proxy.at("aim_state_entered_by_ms").asInt();
        result.firstShot.aimControlsLatencyMaxMs =
            proxy.at("aim_controls_visible_after_tab_max_ms").asInt();
        result.firstShot.minimumPowerSamples =
            proxy.at("power_samples_while_holding_min").asInt();
        result.firstShot.postShotLatencyMaxMs =
            proxy.at("post_shot_state_after_release_max_ms").asInt();
        result.firstShot.maximumPrompts =
            proxy.at("maximum_simultaneous_prompts").asInt();
        result.debugOverlayId = root.at("debug_overlay")
            .at("separate_overlay_id").asString();
        const auto& elevationGate = root.at("capability_gated_placeholders")
            .at("cue_elevation").at("visible_if").asArray();
        const auto& chalkGate = root.at("capability_gated_placeholders")
            .at("chalk").at("visible_if").asArray();
        const json::Value& debug = root.at("debug_overlay");
        if (result.minimumUserScale != 0.85f ||
            result.maximumUserScale != 1.35f || elevationGate.size() != 2 ||
            elevationGate[0].asString() != "capability.cue_elevation_ui" ||
            elevationGate[1].asString() != "physics.cue_elevation_promoted" ||
            chalkGate.size() != 2 ||
            chalkGate[0].asString() != "capability.chalk_ui" ||
            chalkGate[1].asString() != "physics.chalk_model_promoted" ||
            debug.at("shares_product_layout_nodes").asBool() ||
            !debug.at("forbidden_in_release_capture").asBool() ||
            !proxy.at("automatic_aim_or_shot_forbidden").asBool()) {
            result.errorCode = "hud_manifest_contract";
            return result;
        }
        result.ok = true;
        return result;
    } catch (const std::exception&) {
        result.errorCode = "hud_manifest_format";
        return result;
    }
}

HudLayoutResult ResponsiveHudLayoutSolver::solve(
    const HudLayoutRequest& request) const
{
    HudLayoutResult result;
    if (!manifest_.ok) {
        result.errorCode = "hud_manifest_unavailable";
        return result;
    }
    if (request.physicalWidth < 640 || request.physicalHeight < 480 ||
        !std::isfinite(request.dpi) || request.dpi <= 0.0f ||
        !std::isfinite(request.userScale) ||
        request.userScale < manifest_.minimumUserScale ||
        request.userScale > manifest_.maximumUserScale) {
        result.errorCode = "hud_layout_invalid_request";
        return result;
    }
    const float density = clampScale(request.dpi / 96.0f, 0.85f, 1.5f);
    result.resolvedScale = density * request.userScale;
    const float logicalWidth = request.physicalWidth / result.resolvedScale;
    const float logicalHeight = request.physicalHeight / result.resolvedScale;
    // Profile breakpoints describe the OS-density-normalized viewport. User
    // accessibility scale enlarges the chosen layout; it must not silently
    // switch a 1280-wide product viewport to the 13 px compact typography.
    const float viewportLogicalWidth = request.physicalWidth / density;
    const float viewportLogicalHeight = request.physicalHeight / density;
    const bool compact = viewportLogicalWidth / viewportLogicalHeight < 1.55f ||
        viewportLogicalWidth < 1280.0f;
    const HudProfileSpec& profile = compact ? manifest_.compact : manifest_.wide;
    result.profileId = profile.id;
    result.physicalWidth = request.physicalWidth;
    result.physicalHeight = request.physicalHeight;

    const float xRatio = logicalWidth / profile.referenceWidth;
    const float yRatio = logicalHeight / profile.referenceHeight;
    HudSafeArea safe{profile.safeArea.left * xRatio,
        profile.safeArea.top * yRatio, profile.safeArea.right * xRatio,
        profile.safeArea.bottom * yRatio};
    result.safeAreaPhysical = {std::round(safe.left * result.resolvedScale),
        std::round(safe.top * result.resolvedScale),
        std::round(safe.right * result.resolvedScale),
        std::round(safe.bottom * result.resolvedScale)};
    result.minimumTextPhysicalPx =
        profile.minimumTextPx * result.resolvedScale;

    const float contentWidth = logicalWidth - safe.left - safe.right;
    const float contentHeight = logicalHeight - safe.top - safe.bottom;
    if (contentWidth <= 0.0f || contentHeight <= 0.0f) {
        result.errorCode = "hud_layout_no_safe_content";
        return result;
    }
    auto reference = [&](const char* name) {
        return profile.referenceRects.at(name);
    };
    HudRect player = reference("player_status");
    player.x = safe.left;
    player.y = safe.top;
    player.width = std::min(player.width, contentWidth * 0.45f);
    HudRect context = reference("context_hint");
    context.width = std::min(context.width, contentWidth * 0.43f);
    context.x = logicalWidth - safe.right - context.width;
    context.y = safe.top;
    HudRect tip = reference("tip_offset");
    tip.width = std::min(tip.width, contentWidth * 0.27f);
    tip.height = tip.width;
    tip.x = logicalWidth - safe.right - tip.width;
    tip.y = logicalHeight - safe.bottom - tip.height;
    HudRect power = reference("power_meter");
    power.x = safe.left;
    power.y = logicalHeight - safe.bottom - power.height;
    power.width = std::min(power.width,
        std::max(1.0f, tip.x - 16.0f - power.x));
    HudRect shot = reference("shot_result");
    shot.x = safe.left;
    const float toastWidth = compact ? 300.0f : 320.0f;
    shot.width = std::min({shot.width, toastWidth, contentWidth});
    // Post-shot is a single-line acknowledgement, not a persistent results
    // drawer.  Its height is content-driven while still leaving enough room
    // for the minimum text, SDF spread and padding at every supported scale.
    shot.height = std::max(48.0f, profile.minimumTextPx * 2.9f);
    shot.y = logicalHeight - safe.bottom - shot.height;
    HudRect foul = reference("foul_banner");
    foul.width = std::min(foul.width, contentWidth);
    foul.x = safe.left + (contentWidth - foul.width) * 0.5f;
    // The upper/middle playfield contains the rack and the shot line in all
    // non-broadcast gameplay cameras.  Reserve it for play and anchor the
    // two-line foul banner in the bottom safe band instead.
    foul.y = logicalHeight - safe.bottom - foul.height;
    HudRect help = reference("help_panel");
    const float helpPaddingLogical =
        kHelpPhysicalInsetPx * 2.0f / result.resolvedScale;
    const std::size_t helpCharacters = request.helpLongestLineCharacters == 0 ?
        kDefaultLongestHelpLineCharacters : request.helpLongestLineCharacters;
    const float helpContentWidth = profile.minimumTextPx *
        static_cast<float>(helpCharacters) *
        kHelpAdvanceBudgetEm;
    const float helpContentHeight = profile.minimumTextPx *
        (1.0f + (kHelpLineCount - 1.0f) * kHelpLineStepEm);
    help.width = std::min({help.width, contentWidth,
        helpPaddingLogical + helpContentWidth});
    help.height = std::min({help.height, contentHeight,
        helpPaddingLogical + helpContentHeight});
    help.x = safe.left + (contentWidth - help.width) * 0.5f;
    help.y = safe.top + (contentHeight - help.height) * 0.5f;

    auto add = [&](const char* name, const HudRect& rect) {
        result.elements[name] = physicalRect(rect, result.resolvedScale);
    };
    if (request.state == HudRuntimeState::Help) {
        add("help_panel", help);
    } else {
        add("player_status", player);
        add("context_hint", context);
        if (request.state == HudRuntimeState::Aiming) {
            add("power_meter", power);
            add("tip_offset", tip);
        } else if (request.state == HudRuntimeState::PostShot) {
            add("shot_result", shot);
        } else if (request.state == HudRuntimeState::Foul) {
            add("foul_banner", foul);
        }
    }
    result.ok = true;
    return result;
}

DebugOverlayViewModel buildDebugOverlayViewModel(bool requestedVisible,
    bool releaseCapture)
{
    DebugOverlayViewModel result;
    result.visible = requestedVisible && !releaseCapture;
    result.fields = {"fps", "frame_ms", "draw_calls", "triangles",
        "snapshot_tick"};
    return result;
}

FirstShotProxyResult FirstShotAutomationProxy::observe(
    const FirstShotEvent& event, const PresentationSnapshot& snapshot)
{
    if (event.elapsedMs < 0 || event.elapsedMs > contract_.deadlineMs ||
        (!events_.empty() && event.elapsedMs < events_.back().elapsedMs))
        return {false, false, "first_shot_time_invalid"};
    if (event.visiblePromptCount < 0 ||
        event.visiblePromptCount > contract_.maximumPrompts)
        return {false, false, "first_shot_prompt_budget"};
    if ((event.type == FirstShotEventType::AimToggled ||
            event.type == FirstShotEventType::ShotReleased) &&
        event.source == EventSource::Automatic)
        return {false, false, "first_shot_automatic_action_forbidden"};
    const HudRuntimeState state = stateFor(snapshot);
    if (!stateMatches(event.type, state))
        return {false, false, "first_shot_hud_state_mismatch"};

    if (event.type == FirstShotEventType::HudDefaultVisible) {
        if (stage_ != 0 || event.elapsedMs > contract_.statusVisibleByMs)
            return {false, false, "first_shot_default_deadline"};
        stage_ = 1;
    } else if (event.type == FirstShotEventType::AimToggled) {
        if (stage_ != 1 || event.elapsedMs > contract_.aimEnteredByMs ||
            event.presentationLatencyMs > contract_.aimControlsLatencyMaxMs)
            return {false, false, "first_shot_aim_deadline"};
        stage_ = 2;
    } else if (event.type == FirstShotEventType::PowerHoldStarted) {
        if (stage_ != 2) return {false, false, "first_shot_event_order"};
        stage_ = 3;
    } else if (event.type == FirstShotEventType::PowerSampled) {
        if (stage_ != 3 || !std::isfinite(event.powerPercent) ||
            event.powerPercent < lastPower_)
            return {false, false, "first_shot_power_not_monotonic"};
        lastPower_ = event.powerPercent;
        ++powerSamples_;
    } else if (event.type == FirstShotEventType::ShotReleased) {
        if (stage_ != 3 || powerSamples_ < contract_.minimumPowerSamples ||
            ++shotReleases_ != 1)
            return {false, false, "first_shot_release_invalid"};
        releaseTimeMs_ = event.elapsedMs;
        stage_ = 4;
    } else if (event.type == FirstShotEventType::HudPostShotVisible) {
        if (stage_ != 4 || releaseTimeMs_ < 0 ||
            event.elapsedMs - releaseTimeMs_ > contract_.postShotLatencyMaxMs)
            return {false, false, "first_shot_post_shot_deadline"};
        stage_ = 5;
    }
    events_.push_back(event);
    return {true, stage_ == 5, ""};
}

}  // namespace billiardgl::product_hud
