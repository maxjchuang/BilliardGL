#include "product_hud.h"

#include "game_runtime.h"
#include "render_state.h"

#include <cmath>
#include <iostream>
#include <limits>
#include <stdexcept>
#include <string>
#include <vector>

namespace {

using billiardgl::product_hud::FirstShotEvent;
using billiardgl::product_hud::FirstShotEventType;
using billiardgl::product_hud::HudRuntimeState;

void expect(bool value, const std::string& message)
{
    if (!value) throw std::runtime_error(message);
}

billiardgl::PresentationSnapshot snapshotFor(
    const billiardgl::GameRuntime& runtime)
{
    return billiardgl::buildPresentationSnapshot(runtime.state(),
        runtime.state(), 1.0, runtime.shotControl(),
        runtime.shotControlCapabilities(), runtime.cameraFrame(),
        billiardgl::buildRulesPresentation(runtime));
}

billiardgl::rules::ShotFacts facts(int shooter, int firstContact)
{
    billiardgl::rules::ShotFacts result;
    result.shooter = shooter;
    result.firstContactBall = firstContact;
    result.anyRailAfterFirstContact = true;
    return result;
}

bool overlaps(const billiardgl::product_hud::HudRect& left,
    const billiardgl::product_hud::HudRect& right)
{
    return left.x < right.x + right.width &&
        left.x + left.width > right.x &&
        left.y < right.y + right.height &&
        left.y + left.height > right.y;
}

void testReducerStatesFallbackAndGates()
{
    billiardgl::PresentationSnapshot snapshot;
    snapshot.hud.currentPlayerNumber = 1;
    snapshot.camera.preset = static_cast<std::uint32_t>(
        billiardgl::camera::Preset::Elevated);
    snapshot.shotControl.assistLevel = 1;
    auto model = billiardgl::product_hud::HudStateReducer::reduce(snapshot);
    expect(model.state == HudRuntimeState::Default &&
        model.showPlayerStatus && model.showContextHint &&
        !model.showPower && model.player.text == "Player 1" &&
        model.groupShapeToken == "group_open_diamond",
        "default reducer state exposes player/context with non-color group token");

    snapshot.hud.aimMode = true;
    snapshot.shotControl.requestedPower = 84.0f;
    snapshot.shotControl.tipOffsetX = -0.25f;
    snapshot.shotControl.tipOffsetY = 0.50f;
    snapshot.hud.showCueElevation = true;
    snapshot.hud.showChalkState = true;
    billiardgl::product_hud::HudProductCapabilities noPhysics;
    noPhysics.cueElevationUi = true;
    noPhysics.chalkUi = true;
    model = billiardgl::product_hud::HudStateReducer::reduce(snapshot, noPhysics);
    expect(model.state == HudRuntimeState::Aiming && model.showPower &&
        model.showTipOffset && model.powerPercent.text == "42%" &&
        model.estimatedCueSpeed.text.find("EST. ") == 0 &&
        model.estimatedCueSpeed.text.find(" m/s") != std::string::npos &&
        model.tipOffset.text == "X -0.25 / Y 0.50" &&
        model.tipOffsetX == -0.25f && model.tipOffsetY == 0.50f &&
        !model.showCueElevation && !model.showChalk,
        "aiming state exposes numeric controls while either capability gate hides promoted controls");
    noPhysics.cueElevationPhysicsPromoted = true;
    noPhysics.chalkPhysicsPromoted = true;
    model = billiardgl::product_hud::HudStateReducer::reduce(snapshot, noPhysics);
    expect(model.showCueElevation && model.showChalk,
        "cue elevation and chalk require both UI and physics promotion gates");

    snapshot.hud.aimMode = false;
    snapshot.events.shotEnded = true;
    model = billiardgl::product_hud::HudStateReducer::reduce(snapshot);
    expect(model.state == HudRuntimeState::PostShot && model.showShotResult &&
        model.result.text == "Shot complete",
        "post-shot reducer state is independent");

    snapshot.rules.foul = true;
    snapshot.rules.hasAdjudication = true;
    snapshot.rules.ballInHand = true;
    snapshot.rules.reasonCodes = {"cue_ball_scratch"};
    model = billiardgl::product_hud::HudStateReducer::reduce(snapshot);
    expect(model.state == HudRuntimeState::Foul && model.showFoul &&
        model.ruleNotices.size() == 1 &&
        model.ruleNotices[0].playerText == "The cue ball was pocketed." &&
        !model.ruleNotices[0].iconToken.empty() &&
        !model.ruleNotices[0].shapeToken.empty() &&
        model.foulResolution.text == "Opponent has ball in hand.",
        "foul state carries readable reason, resolution, icon and shape");

    snapshot.events.shotEnded = false;
    model = billiardgl::product_hud::HudStateReducer::reduce(snapshot);
    expect(model.state == HudRuntimeState::Default &&
        model.ruleNotices.empty() && !model.foulReason.available &&
        model.foulReason.text == "No current foul." &&
        !model.foulResolution.available,
        "next default frame does not retain stale foul presentation");

    snapshot.hud.showHelp = true;
    model = billiardgl::product_hud::HudStateReducer::reduce(snapshot);
    expect(model.state == HudRuntimeState::Help && model.showHelp &&
        !model.showPlayerStatus,
        "help is an independent reducer state");

    billiardgl::settings::ProductSettings bindings;
    bindings.bindings = {
        {"toggle_help", "global", "keyboard", "H", "press"},
        {"toggle_aim", "play", "keyboard", "Tab", "press"},
        {"shoot", "aim", "mouse", "LeftButton", "release"},
        {"adjust_tip_offset", "aim", "keyboard", "ArrowKeys", "continuous"},
        {"camera_orbit", "observe", "keyboard", "ArrowKeys", "continuous"},
        {"zoom_in", "observe", "keyboard", "W", "continuous"},
        {"zoom_out", "observe", "keyboard", "S", "continuous"}};
    const std::vector<std::string> defaultHelp{
        "QUICK CONTROLS", "TAB  Enter aim", "MOUSE LEFT  Release shot",
        "ARROWS  Tip offset", "ARROWS  Orbit camera", "W / S  Zoom",
        "H  Close help"};
    expect(billiardgl::product_hud::buildHelpLines(bindings) == defaultHelp &&
        billiardgl::product_hud::longestHelpLineCharacters(defaultHelp) == 24,
        "Help derives all seven truthful default labels from ProductSettings");
    for (auto& binding : bindings.bindings) {
        if (binding.action == "shoot") {
            binding.device = "mouse";
            binding.control = "RightButton";
            binding.trigger = "press";
            break;
        }
    }
    const auto reboundHelp =
        billiardgl::product_hud::buildHelpLines(bindings);
    expect(reboundHelp.size() == 7 &&
        reboundHelp[2] == "MOUSE RIGHT  Shoot" &&
        std::find(reboundHelp.begin(), reboundHelp.end(),
            "SPACE  Hold and release power") == reboundHelp.end() &&
        std::find(reboundHelp.begin(), reboundHelp.end(),
            "ESC  Close help") == reboundHelp.end(),
        "Help follows a rebound shoot control and never advertises unrelated Space/Escape actions");

    billiardgl::PresentationSnapshot invalid;
    invalid.hud.currentPlayerNumber = 99;
    invalid.shotControl.requestedPower =
        std::numeric_limits<float>::quiet_NaN();
    invalid.shotControl.tipOffsetX = 2.0f;
    invalid.camera.preset = 99;
    invalid.shotControl.assistLevel = 99;
    invalid.rules.enabled = true;
    invalid.rules.currentPlayer = -1;
    invalid.rules.groups = {{"unknown", "unknown"}};
    const auto fallback =
        billiardgl::product_hud::HudStateReducer::reduce(invalid);
    expect(!fallback.player.available && fallback.player.text == "Player —" &&
        !fallback.group.available && fallback.group.text == "Group —" &&
        !fallback.targetBall.available && fallback.targetBall.text == "Target —" &&
        !fallback.powerPercent.available && fallback.powerPercent.text == "—%" &&
        !fallback.estimatedCueSpeed.available &&
            fallback.estimatedCueSpeed.text == "EST. — m/s" &&
        !fallback.tipOffset.available && fallback.tipOffset.text == "X — / Y —" &&
        !fallback.camera.available && fallback.camera.text == "Camera —" &&
        !fallback.assist.available && fallback.assist.text == "Assist —" &&
        fallback.result.text == "Result pending",
        "all authoritative HUD fields have explicit neutral fallbacks");
}

void testManifestAndResponsiveLayout()
{
    const auto manifest = billiardgl::product_hud::loadProductHudManifest(
        std::string(BILLIARDGL_SOURCE_ROOT) +
        "/assets/modern/hud/focused_practice_hud_v1.hud.json");
    expect(manifest.ok && manifest.manifestId == "focused_practice_hud_v1" &&
        manifest.debugOverlayId == "billiardgl_debug_telemetry_v1",
        "runtime product layer consumes the TA HUD manifest contract");
    billiardgl::product_hud::ResponsiveHudLayoutSolver solver(manifest);
    const struct Case { int width; int height; float dpi; } cases[] = {
        {1024, 768, 96.0f}, {1280, 720, 96.0f},
        {1920, 1080, 96.0f}, {1920, 1080, 144.0f}};
    const float scales[] = {0.85f, 1.0f, 1.35f};
    const HudRuntimeState states[] = {HudRuntimeState::Default,
        HudRuntimeState::Aiming, HudRuntimeState::PostShot,
        HudRuntimeState::Foul, HudRuntimeState::Help};
    for (const Case& size : cases) {
        for (float scale : scales) {
            for (HudRuntimeState state : states) {
                const auto layout = solver.solve(
                    {size.width, size.height, size.dpi, scale, state});
                expect(layout.ok && layout.minimumTextPhysicalPx > 0.0f,
                    "responsive layout resolves with minimum physical text size");
                if (size.width / (size.dpi / 96.0f) >= 1280.0f &&
                        (static_cast<float>(size.width) / size.height) >= 1.55f)
                    expect(std::fabs(layout.minimumTextPhysicalPx -
                        14.0f * (size.dpi / 96.0f) * scale) < 0.01f,
                        "wide breakpoint minimum follows DPI and user scale without compact switching");
                const float left = layout.safeAreaPhysical.left;
                const float top = layout.safeAreaPhysical.top;
                const float right = size.width - layout.safeAreaPhysical.right;
                const float bottom = size.height - layout.safeAreaPhysical.bottom;
                for (const auto& item : layout.elements) {
                    const auto& rect = item.second;
                    expect(rect.x >= left - 1.0f && rect.y >= top - 1.0f &&
                        rect.x + rect.width <= right + 1.0f &&
                        rect.y + rect.height <= bottom + 1.0f,
                        "HUD element remains inside physical safe area");
                }
                for (auto first = layout.elements.begin();
                        first != layout.elements.end(); ++first) {
                    for (auto second = std::next(first);
                            second != layout.elements.end(); ++second) {
                        expect(!overlaps(first->second, second->second),
                            "active HUD elements do not overlap");
                    }
                }
                if (state == HudRuntimeState::PostShot) {
                    const auto& toast = layout.elements.at("shot_result");
                    expect(toast.height <=
                            layout.minimumTextPhysicalPx * 3.8f &&
                        toast.width <=
                            layout.minimumTextPhysicalPx * 24.0f &&
                        toast.y + toast.height >= bottom - 1.0f,
                        "single-line post-shot acknowledgement is a compact content-sized bottom toast");
                }
                if (state == HudRuntimeState::Foul) {
                    const auto& banner = layout.elements.at("foul_banner");
                    expect(banner.y >= size.height * 0.70f &&
                        banner.y + banner.height >= bottom - 1.0f,
                        "foul banner stays in the lower safe band outside the rack and shot corridor");
                }
                if (state == HudRuntimeState::Help) {
                    const auto& panel = layout.elements.at("help_panel");
                    constexpr float helpLineCount = 7.0f;
                    constexpr float helpLineStep = 1.8f;
                    constexpr std::size_t longestHelpLineCharacters = 24;
                    constexpr float conservativeGlyphAdvanceEm = 0.64f;
                    constexpr float physicalInset = 18.0f;
                    const float expectedWidth = physicalInset * 2.0f +
                        layout.minimumTextPhysicalPx *
                        static_cast<float>(longestHelpLineCharacters) *
                        conservativeGlyphAdvanceEm;
                    const float expectedHeight = physicalInset * 2.0f +
                        layout.minimumTextPhysicalPx *
                        (1.0f + (helpLineCount - 1.0f) * helpLineStep);
                    const float safeCenterX = (left + right) * 0.5f;
                    const float safeCenterY = (top + bottom) * 0.5f;
                    expect(std::fabs(panel.width - expectedWidth) <= 1.1f &&
                        std::fabs(panel.height - expectedHeight) <= 1.1f &&
                        std::fabs(panel.x + panel.width * 0.5f -
                            safeCenterX) <= 1.1f &&
                        std::fabs(panel.y + panel.height * 0.5f -
                            safeCenterY) <= 1.1f,
                        "seven-line help panel is content-sized and centered in the safe area");
                }
            }
        }
    }
    expect(!solver.solve({1280, 720, 96.0f, 0.70f,
        HudRuntimeState::Default}).ok,
        "layout rejects UI scales outside manifest 85–135 percent");
    expect(solver.solve({1024, 768, 96.0f, 1.0f,
        HudRuntimeState::Default}).profileId == "compact_1024x768" &&
        solver.solve({1280, 720, 96.0f, 1.0f,
        HudRuntimeState::Default}).profileId == "wide_1920x1080" &&
        solver.solve({1920, 1080, 144.0f, 1.0f,
        HudRuntimeState::Default}).profileId == "wide_1920x1080",
        "manifest breakpoints select compact, minimum-wide, and 144-DPI wide profiles");

    const auto reboundLayout = solver.solve({1280, 720, 96.0f, 1.0f,
        HudRuntimeState::Help, 31});
    const auto defaultHelpLayout = solver.solve({1280, 720, 96.0f, 1.0f,
        HudRuntimeState::Help, 24});
    expect(reboundLayout.ok && defaultHelpLayout.ok &&
        reboundLayout.elements.at("help_panel").width >
            defaultHelpLayout.elements.at("help_panel").width &&
        reboundLayout.elements.at("help_panel").x >=
            reboundLayout.safeAreaPhysical.left - 1.0f &&
        reboundLayout.elements.at("help_panel").x +
            reboundLayout.elements.at("help_panel").width <=
            1280.0f - reboundLayout.safeAreaPhysical.right + 1.0f,
        "Help panel expands from the genuine rebound-line length without leaving the safe area");

    const auto debug = billiardgl::product_hud::buildDebugOverlayViewModel(
        true, false);
    const auto releaseDebug =
        billiardgl::product_hud::buildDebugOverlayViewModel(true, true);
    expect(debug.visible && !debug.sharesProductLayoutNodes &&
        debug.fields.size() == 5 && !releaseDebug.visible &&
        releaseDebug.forbiddenInReleaseCapture,
        "debug overlay remains separate and is suppressed for release capture");
}

void testFirstShotAutomationProxy()
{
    const auto manifest = billiardgl::product_hud::loadProductHudManifest(
        std::string(BILLIARDGL_SOURCE_ROOT) +
        "/assets/modern/hud/focused_practice_hud_v1.hud.json");
    billiardgl::product_hud::FirstShotAutomationProxy proxy(manifest.firstShot);
    billiardgl::PresentationSnapshot view;
    view.hud.currentPlayerNumber = 1;
    expect(proxy.observe({FirstShotEventType::HudDefaultVisible, 100,
        billiardgl::product_hud::EventSource::PresentationObserved,
        0.0f, 0, 2}, view).accepted,
        "proxy observes default HUD within 500 ms");
    view.hud.aimMode = true;
    expect(proxy.observe({FirstShotEventType::AimToggled, 6000,
        billiardgl::product_hud::EventSource::UserObserved,
        0.0f, 80, 2}, view).accepted &&
        proxy.observe({FirstShotEventType::PowerHoldStarted, 18000,
        billiardgl::product_hud::EventSource::UserObserved,
        0.0f, 0, 2}, view).accepted,
        "proxy observes user aim and power hold without issuing commands");
    for (int index = 0; index < 3; ++index) {
        expect(proxy.observe({FirstShotEventType::PowerSampled,
            20000 + index * 2000,
            billiardgl::product_hud::EventSource::PresentationObserved,
            20.0f + index * 20.0f, 0, 2}, view).accepted,
            "proxy accepts monotonic power presentation sample");
    }
    expect(proxy.observe({FirstShotEventType::ShotReleased, 27500,
        billiardgl::product_hud::EventSource::UserObserved,
        60.0f, 0, 2}, view).accepted,
        "proxy observes exactly one user shot release by 30 seconds");
    view.hud.aimMode = false;
    view.events.shotEnded = true;
    const auto complete = proxy.observe({FirstShotEventType::HudPostShotVisible,
        27600, billiardgl::product_hud::EventSource::PresentationObserved,
        0.0f, 100, 2}, view);
    expect(complete.accepted && complete.complete && proxy.events().size() == 8,
        "30-second engineering proxy reaches ordered post-shot completion");

    billiardgl::product_hud::FirstShotAutomationProxy forbidden(
        manifest.firstShot);
    expect(forbidden.observe({FirstShotEventType::HudDefaultVisible, 100,
        billiardgl::product_hud::EventSource::PresentationObserved},
        billiardgl::PresentationSnapshot{}).accepted,
        "forbidden-action fixture starts");
    billiardgl::PresentationSnapshot aiming;
    aiming.hud.aimMode = true;
    const auto automatic = forbidden.observe({FirstShotEventType::AimToggled,
        1000, billiardgl::product_hud::EventSource::Automatic}, aiming);
    expect(!automatic.accepted &&
        automatic.errorCode == "first_shot_automatic_action_forbidden",
        "first-shot proxy explicitly forbids automatic aim");
    billiardgl::product_hud::FirstShotAutomationProxy forbiddenShot(
        manifest.firstShot);
    const auto automaticShot = forbiddenShot.observe({
        FirstShotEventType::ShotReleased, 1000,
        billiardgl::product_hud::EventSource::Automatic},
        billiardgl::PresentationSnapshot{});
    expect(!automaticShot.accepted &&
        automaticShot.errorCode == "first_shot_automatic_action_forbidden",
        "first-shot proxy explicitly forbids automatic shoot");
}

void testRuleReasonCatalog()
{
    billiardgl::PresentationSnapshot snapshot;
    snapshot.events.shotEnded = true;
    snapshot.rules.enabled = true;
    snapshot.rules.hasAdjudication = true;
    snapshot.rules.foul = true;
    snapshot.rules.ballInHand = true;
    snapshot.rules.reasonCodes = {"illegal_break", "no_object_ball_contact",
        "wrong_first_contact", "no_rail_after_contact", "cue_ball_scratch",
        "cue_ball_off_table", "object_ball_off_table",
        "break_cue_ball_past_head_string", "double_hit", "push",
        "frozen_cushion_obligation", "illegal_eight_ball"};
    const std::vector<std::string> expectedPlayerText = {
        "Break requirements were not met.",
        "No object ball was contacted.",
        "The wrong ball was contacted first.",
        "No ball reached a rail after contact.",
        "The cue ball was pocketed.",
        "The cue ball left the table.",
        "An object ball left the table.",
        "The break began with the cue ball past the head string.",
        "The cue ball was struck more than once.",
        "The stroke was judged to be a push shot.",
        "The required cushion contact for a frozen ball was not made.",
        "The 8-ball was pocketed illegally."};
    const auto hud = billiardgl::product_hud::HudStateReducer::reduce(snapshot);
    expect(hud.ruleNotices.size() == snapshot.rules.reasonCodes.size(),
        "every rules reason code has a product notice");
    for (std::size_t index = 0; index < hud.ruleNotices.size(); ++index) {
        const auto& notice = hud.ruleNotices[index];
        expect(notice.reasonCode == snapshot.rules.reasonCodes[index] &&
            notice.playerText == expectedPlayerText[index] &&
            notice.playerText != "Rule decision unavailable." &&
            !notice.iconToken.empty() &&
            !notice.shapeToken.empty(),
            "every defined foul has specific English text and non-color icon/shape tokens");
    }
    snapshot.rules.reasonCodes = {"both_feet_off_ground_during_shot",
        "illegal_body_clothing_or_equipment_ball_touch", "one_hand_shot",
        "auxiliary_equipment_placed_on_table_to_shoot",
        "mobile_device_disturbance_general_foul"};
    const auto nonphysical = billiardgl::product_hud::HudStateReducer::reduce(snapshot);
    expect(nonphysical.ruleNotices.size() == 5,
        "all existing nonphysical committed kinds have notices");
    for (const auto& notice : nonphysical.ruleNotices)
        expect(notice.playerText.find("ball in hand") != std::string::npos &&
            notice.playerText != "Rule decision unavailable.",
            "nonphysical cause explains the existing BIH consequence");
}

void testRulesetProductStateE2E()
{
    billiardgl::GameRuntime runtime;
    expect(runtime.setRulesMode(
        billiardgl::RulesMode::ChineseEightBallV1).ok,
        "product-state E2E enables the ruleset");
    auto opening = facts(0, 1);
    opening.breakShot = true;
    opening.distinctObjectBallsToRail = 4;
    expect(runtime.adjudicateRulesShot(opening).ok,
        "legal opening break adjudicates");
    auto breakSnapshot = snapshotFor(runtime);
    breakSnapshot.events.shotEnded = true;
    auto hud = billiardgl::product_hud::HudStateReducer::reduce(breakSnapshot);
    expect(hud.state == HudRuntimeState::PostShot &&
        hud.player.text == "Player 2" && hud.group.text == "OPEN TABLE",
        "legal break reaches open-table product state");

    auto assignment = facts(1, 1);
    assignment.pocketedBallsInOrder = {1};
    expect(runtime.adjudicateRulesShot(assignment).ok,
        "first legal pocket assigns solids");
    runtime.mutableState().balls[1].pocketed = true;
    auto assignedSnapshot = snapshotFor(runtime);
    assignedSnapshot.events.shotEnded = true;
    hud = billiardgl::product_hud::HudStateReducer::reduce(assignedSnapshot);
    expect(hud.group.text == "SOLIDS" &&
        hud.groupShapeToken == "group_solid_filled_circle" &&
        hud.targetBall.text == "Ball 2",
        "group assignment exposes text/shape and next target");

    for (int ball = 2; ball <= 7; ++ball) {
        auto clear = facts(1, ball);
        clear.solidsRemainingBefore = 8 - ball;
        clear.pocketedBallsInOrder = {ball};
        expect(runtime.adjudicateRulesShot(clear).ok,
            "legal group clearance shot adjudicates");
        runtime.mutableState().balls[ball].pocketed = true;
    }
    auto eightReady = snapshotFor(runtime);
    hud = billiardgl::product_hud::HudStateReducer::reduce(eightReady);
    expect(hud.targetBall.text == "Ball 8",
        "clearing every solid promotes the 8-ball target");

    auto eight = facts(1, 8);
    eight.solidsRemainingBefore = 0;
    eight.pocketedBallsInOrder = {8};
    expect(runtime.adjudicateRulesShot(eight).ok,
        "legal 8-ball shot adjudicates");
    runtime.mutableState().balls[8].pocketed = true;
    auto winSnapshot = snapshotFor(runtime);
    winSnapshot.events.shotEnded = true;
    hud = billiardgl::product_hud::HudStateReducer::reduce(winSnapshot);
    expect(hud.state == HudRuntimeState::PostShot &&
        hud.result.text == "Player 2 wins" && hud.result.available,
        "full legal clearance reaches explicit winner HUD state");

    billiardgl::GameRuntime foulRuntime;
    foulRuntime.setRulesMode(billiardgl::RulesMode::ChineseEightBallV1);
    opening.shooter = 0;
    foulRuntime.adjudicateRulesShot(opening);
    foulRuntime.adjudicateRulesShot(assignment);
    auto foul = facts(1, 9);
    foul.anyRailAfterFirstContact = false;
    expect(foulRuntime.adjudicateRulesShot(foul).ok,
        "typical wrong-contact/no-rail foul adjudicates");
    auto foulSnapshot = snapshotFor(foulRuntime);
    foulSnapshot.events.shotEnded = true;
    hud = billiardgl::product_hud::HudStateReducer::reduce(foulSnapshot);
    expect(hud.state == HudRuntimeState::Foul &&
        hud.ruleNotices.size() == 2 &&
        hud.foulResolution.text == "Opponent has ball in hand." &&
        hud.ruleNotices[0].iconToken != hud.ruleNotices[0].shapeToken,
        "typical foul HUD explains every reason without color-only encoding");
}

}  // namespace

int main()
{
    try {
        testReducerStatesFallbackAndGates();
        testManifestAndResponsiveLayout();
        testFirstShotAutomationProxy();
        testRuleReasonCatalog();
        testRulesetProductStateE2E();
        std::cout << "Product HUD reducer, layout, proxy, and rules E2E checks passed\n";
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
