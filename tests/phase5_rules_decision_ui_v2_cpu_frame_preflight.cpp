#include "automation_json.h"
#include "product_hud.h"
#include "render/modern/modern_product_session.h"
#include "render/ui/product_ui_frame_coordinator.h"
#include "render/ui/rules_decision_ui_draw_list.h"

#include <cmath>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <limits>
#include <map>
#include <set>
#include <stdexcept>
#include <string>
#include <vector>

namespace {

using namespace billiardgl;
using namespace billiardgl::render;
namespace ui = billiardgl::render::ui;

void expect(bool value, const std::string& message)
{
    if (!value) throw std::runtime_error(message);
}

std::vector<std::string> split(const std::string& line)
{
    std::vector<std::string> result;
    std::size_t start = 0;
    while (start <= line.size()) {
        const std::size_t end = line.find('\t', start);
        result.push_back(line.substr(start, end == std::string::npos ?
            std::string::npos : end - start));
        if (end == std::string::npos) break;
        start = end + 1;
    }
    return result;
}

struct CaptureCase {
    std::string id;
    std::string state;
    std::string locale;
    std::string palette;
    std::string profile;
    int logicalWidth = 0;
    int logicalHeight = 0;
    int physicalWidth = 0;
    int physicalHeight = 0;
    float density = 1.0f;
    double uiScale = 1.0;
    float dpi = 96.0f;
};

CaptureCase parseCase(const std::string& line)
{
    const auto fields = split(line);
    expect(fields.size() == 12, "v2_cpu_plan_field_count");
    return {fields[0], fields[1], fields[2], fields[3], fields[4],
        std::stoi(fields[5]), std::stoi(fields[6]), std::stoi(fields[7]),
        std::stoi(fields[8]), std::stof(fields[9]), std::stod(fields[10]),
        std::stof(fields[11])};
}

std::vector<CaptureCase> loadPlan(const std::filesystem::path& path)
{
    std::ifstream input(path);
    expect(static_cast<bool>(input), "v2_cpu_plan_unreadable");
    std::vector<CaptureCase> result;
    std::string line;
    while (std::getline(input, line))
        if (!line.empty()) result.push_back(parseCase(line));
    return result;
}

bool validPlan(const std::vector<CaptureCase>& plan)
{
#if defined(BILLIARDGL_PHASE5_RULES_UI_V4_EXACT16)
    const std::vector<std::string> expectedIds{
        "group_choice_armed_confirm--p1--zh_cn",
        "group_choice_armed_confirm--p2--en",
        "group_choice_armed_confirm--p3--zh_cn",
        "group_choice_armed_confirm--p4--en",
        "group_choice_open_unselected--p1--en",
        "group_choice_open_unselected--p2--zh_cn",
        "group_choice_open_unselected--p3--en",
        "group_choice_open_unselected--p4--zh_cn",
        "eight_legal_open_unselected--p1--en",
        "break_illegal_open_unselected--p2--zh_cn",
        "submitting_progress--p3--en",
        "authority_error--p4--en",
        "full_table_preview_valid--p1--en",
        "full_table_preview_invalid--p2--en",
        "behind_head_string_preview_valid--p3--en",
        "behind_head_string_preview_invalid--p4--en",
    };
    if (plan.size() != expectedIds.size()) return false;
    for (std::size_t index = 0; index < expectedIds.size(); ++index)
        if (plan[index].id != expectedIds[index]) return false;
#elif defined(BILLIARDGL_PHASE5_RULES_UI_V3_TARGETED)
    const std::vector<std::string> expectedIds{
        "group_choice_armed_confirm--p1--zh_cn",
        "group_choice_armed_confirm--p2--en",
        "group_choice_armed_confirm--p3--zh_cn",
        "group_choice_armed_confirm--p4--en",
        "full_table_preview_valid--p1--en",
        "full_table_preview_valid--p2--zh_cn",
        "full_table_preview_valid--p3--en",
        "full_table_preview_valid--p4--zh_cn",
        "full_table_preview_invalid--p1--zh_cn",
        "full_table_preview_invalid--p2--en",
        "full_table_preview_invalid--p3--zh_cn",
        "full_table_preview_invalid--p4--en",
        "behind_head_string_preview_valid--p1--en",
        "behind_head_string_preview_valid--p2--zh_cn",
        "behind_head_string_preview_valid--p3--en",
        "behind_head_string_preview_valid--p4--zh_cn",
        "behind_head_string_preview_invalid--p1--zh_cn",
        "behind_head_string_preview_invalid--p2--en",
        "behind_head_string_preview_invalid--p3--zh_cn",
        "behind_head_string_preview_invalid--p4--en",
        "group_choice_open_unselected--p1--en",
        "group_choice_open_unselected--p2--zh_cn",
        "group_choice_open_unselected--p3--en",
        "group_choice_open_unselected--p4--zh_cn",
        "eight_legal_open_unselected--p1--en",
        "break_illegal_open_unselected--p2--zh_cn",
        "submitting_progress--p3--en",
        "authority_error--p4--en",
    };
    if (plan.size() != expectedIds.size()) return false;
    for (std::size_t index = 0; index < expectedIds.size(); ++index)
        if (plan[index].id != expectedIds[index]) return false;
#else
    if (plan.size() != 60) return false;
#endif
    const std::set<std::string> expectedStates(
        ui::rulesDecisionCanonicalStateIds().begin(),
        ui::rulesDecisionCanonicalStateIds().end());
    const std::set<std::string> expectedProfiles{"p1", "p2", "p3", "p4"};
    std::set<std::string> ids;
    std::map<std::string, std::set<std::string>> profilesByState;
    for (const auto& item : plan) {
        if (!ids.insert(item.id).second ||
                expectedStates.count(item.state) == 0 ||
                expectedProfiles.count(item.profile) == 0 ||
                !std::isfinite(item.uiScale))
            return false;
        profilesByState[item.state].insert(item.profile);
        const double expectedScale = item.profile == "p1" ? 0.85 :
            item.profile == "p2" ? 1.35 : 1.0;
        if (item.uiScale != expectedScale) return false;
    }
#if defined(BILLIARDGL_PHASE5_RULES_UI_V4_EXACT16)
    return ids.size() == 16;
#elif defined(BILLIARDGL_PHASE5_RULES_UI_V3_TARGETED)
    return ids.size() == 28;
#else
    if (profilesByState.size() != expectedStates.size()) return false;
    for (const auto& state : expectedStates)
        if (profilesByState[state] != expectedProfiles) return false;
    return true;
#endif
}

rules::ui::DecisionUiSnapshot snapshotFor(const CaptureCase& capture)
{
    const ui::RulesDecisionCanonicalFixture fixture =
        ui::rulesDecisionCanonicalFixture(capture.state);
    rules::ui::DecisionUiSnapshot result;
    result.offer.rulesetId = rules::ui::kRulesDecisionUiRulesetId;
    result.offer.flowId = fixture.flowId;
    result.offer.branchId = fixture.branchId;
    result.offer.sourceClause = fixture.sourceClause;
    result.offer.decisionId = "phase5.v2.cpu." + capture.id;
    result.offer.authorityStateRevision = 42;
    result.offer.actorPlayerId = 0;
    result.offer.offeredActionIds = fixture.offeredActionIds;
    result.visibleActionIds = fixture.offeredActionIds;
    result.armedActionId = fixture.armedActionId;
    result.focusedActionId = fixture.focusedActionId;
    result.shotInputBlocked = true;
    result.placementPreviewVisible = fixture.placement;
    result.placementPreviewLegal = fixture.placementValid;
    result.placementPreview = {fixture.previewWorldX, fixture.previewWorldZ};
    if (fixture.flowId == rules::kBehindHeadStringBallInHandFlowId)
        result.headStringWorldZCm = -63.5;
    if (fixture.flowId == rules::ui::kGroupChoiceFlowId)
        result.offer.capabilityId = rules::ui::kGroupChoiceCapabilityId;
    else if (fixture.flowId == rules::kFullTableBallInHandFlowId ||
            fixture.flowId == rules::kBehindHeadStringBallInHandFlowId)
        result.offer.capabilityId = rules::kFreeBallPlacementCapabilityId;
    else if (fixture.flowId == rules::kEightOnBreakFlowId)
        result.offer.capabilityId = rules::kEightOnBreakCapabilityId;
    else if (fixture.flowId == rules::kBreakFoulFlowId)
        result.offer.capabilityId = rules::kBreakFoulCapabilityId;

    if (capture.state == "group_choice_armed_confirm")
        result.state = rules::ui::DecisionUiState::ActionArmedNotSubmitted;
    else if (capture.state.find("preview_valid") != std::string::npos)
        result.state = rules::ui::DecisionUiState::PlacementPreviewLocalValid;
    else if (capture.state.find("preview_invalid") != std::string::npos)
        result.state = rules::ui::DecisionUiState::PlacementPreviewLocalInvalid;
    else if (capture.state == "submitting_progress")
        result.state = rules::ui::DecisionUiState::SubmittingExactRequest;
    else if (capture.state == "authority_error")
        result.state = rules::ui::DecisionUiState::AuthorityErrorBlocking;
    else if (capture.state == "state_drift_refreshing")
        result.state = rules::ui::DecisionUiState::StateDriftRefreshing;
    else if (capture.state == "ai_or_remote_pending")
        result.state = rules::ui::DecisionUiState::AiOrRemotePending;
    else if (capture.state == "section_6g_classification_pending")
        result.state = rules::ui::DecisionUiState::ClassificationPendingBlocked;
    else
        result.state = rules::ui::DecisionUiState::DecisionOpenUnselected;
    return result;
}

ModernProductSessionSnapshot sessionFor(const CaptureCase& capture)
{
    ModernProductSessionSnapshot result;
    result.shell.screen = ProductScreen::Match;
    result.shell.screenId = "phase5_rules_decision_v2_cpu_preflight";
    result.shell.uiScale = static_cast<float>(capture.uiScale);
    result.effectiveSettings = settings::ProductSettings::defaults();
    result.effectiveSettings.uiScale = capture.uiScale;
    result.effectiveSettings.colorVisionMode = capture.palette == "standard" ?
        settings::ColorVisionMode::Default :
        settings::ColorVisionMode::Deuteranopia;
    result.shellViewport.logicalWidth = capture.logicalWidth;
    result.shellViewport.logicalHeight = capture.logicalHeight;
    result.shellViewport.physicalWidth = capture.physicalWidth;
    result.shellViewport.physicalHeight = capture.physicalHeight;
    result.shellViewport.dpi = capture.dpi;
    result.rulesDecisionUiImplementationEnabled = true;
    result.rulesDecisionUiProductExposed = false;
    result.rulesDecisionLocale = capture.locale == "en" ?
        ui::RulesDecisionLocale::En : ui::RulesDecisionLocale::ZhCn;
    result.rulesDecision = snapshotFor(capture);
    return result;
}

PresentationSnapshot presentationFor(const CaptureCase& capture)
{
    PresentationSnapshot value;
    value.hud.currentPlayerNumber = 1;
    value.hud.viewportWidth = capture.physicalWidth;
    value.hud.viewportHeight = capture.physicalHeight;
    value.hud.aimMode = true;
    value.hud.showPowerMeter = true;
    value.hud.shotPower = 0.5f;
    value.shotControl.requestedPower = 50.0f;
    value.rules.enabled = true;
    value.rules.rulesetId = rules::ui::kRulesDecisionUiRulesetId;
    value.rules.groups = {{"open", "open"}};
    value.camera.eye = {0.0f, 300.0f, 260.0f};
    value.camera.target = {0.0f, 86.483976f, -20.0f};
    value.camera.fieldOfViewDegrees = 48.0f;
    return value;
}

std::vector<std::uint32_t> rulesCodepoints()
{
    const auto hud = ui::requiredHudCodepoints();
    std::set<std::uint32_t> unique(hud.begin(), hud.end());
    for (const auto& text : ui::allRulesDecisionUiStrings()) {
        std::vector<std::uint32_t> decoded;
        std::string error;
        expect(ui::decodeUtf8(text, decoded, error), "v2_cpu_copy_utf8_invalid");
        unique.insert(decoded.begin(), decoded.end());
    }
    return {unique.begin(), unique.end()};
}

ui::SdfFontAtlas atlas()
{
    ui::FontAtlasConfig config;
    config.fontPath = "/System/Library/Fonts/Hiragino Sans GB.ttc";
    config.nominalPixelSize = 32;
    config.atlasWidth = 4096;
    config.atlasHeight = 4096;
    ui::SdfFontAtlas result;
    expect(result.initialize(config, rulesCodepoints()).ok,
        "v2_cpu_shipping_atlas_failed");
    return result;
}

json::Value badInput(const std::string& id, const std::string& expected,
    const std::string& actual)
{
    json::Value row = json::Value::object();
    row["id"] = json::Value(id);
    row["expected_error"] = json::Value(expected);
    row["actual_error"] = json::Value(actual);
    row["passed"] = json::Value(expected == actual);
    return row;
}

}  // namespace

int main(int argc, const char* const* argv)
{
    try {
        expect(argc == 3,
            "usage: phase5_rules_ui_v2_cpu_preflight PLAN REPORT");
        const auto plan = loadPlan(argv[1]);
        expect(validPlan(plan), "v2_cpu_plan_not_exact_60x4");
        const ui::SdfFontAtlas font = atlas();
        const auto manifest = product_hud::loadProductHudManifest(
            std::string(BILLIARDGL_SOURCE_ROOT) +
            "/assets/modern/hud/focused_practice_hud_v1.hud.json");
        const auto registry = ui::mergeHudShippingTokenSets({
            ui::loadHudShippingTokenSet(std::string(BILLIARDGL_SOURCE_ROOT) +
                "/assets/modern/hud/focused_practice_hud_shipping_tokens_v1.json"),
            ui::loadShellShippingTokenSet(std::string(BILLIARDGL_SOURCE_ROOT) +
                "/assets/modern/hud/focused_practice_shell_shipping_tokens_v1.json")});
        expect(manifest.ok && registry.ok, "v2_cpu_shipping_assets_failed");
        const ui::ProductUiFrameCoordinator coordinator(
            manifest, font, registry);
        const ui::ProductUiFrameEnvironment environment{
            true, false, false, true};

        json::Value cases = json::Value::array();
        std::map<std::string, int> scaleCounts;
        for (const auto& capture : plan) {
            const auto frame = coordinator.build(ui::makeProductUiFrameRequest(
                sessionFor(capture), presentationFor(capture), environment));
            expect(frame.ok && frame.rulesDecisionActive &&
                    frame.evidenceSha256.size() == 64,
                "v2_cpu_frame_failed:" + capture.id + ":" + frame.errorCode);
            json::Value row = json::Value::object();
            row["capture_id"] = json::Value(capture.id);
            row["profile_id"] = json::Value(capture.profile);
            row["ui_scale"] = json::Value(capture.uiScale);
            row["canonical_sha256"] = json::Value(frame.evidenceSha256);
            row["rules_decision_active"] = json::Value(true);
            row["passed"] = json::Value(true);
            cases.asArray().push_back(row);
            ++scaleCounts[capture.profile == "p1" ? "0.85" :
                capture.profile == "p2" ? "1.35" : "1.0"];
        }
#if defined(BILLIARDGL_PHASE5_RULES_UI_V4_EXACT16)
        expect(scaleCounts == std::map<std::string, int>{{"0.85", 4},
                {"1.0", 8}, {"1.35", 4}},
            "v4_cpu_scale_coverage_invalid");
#elif defined(BILLIARDGL_PHASE5_RULES_UI_V3_TARGETED)
        expect(scaleCounts == std::map<std::string, int>{{"0.85", 7},
                {"1.0", 14}, {"1.35", 7}},
            "v3_cpu_scale_coverage_invalid");
#else
        expect(scaleCounts == std::map<std::string, int>{{"0.85", 15},
                {"1.0", 30}, {"1.35", 15}},
            "v2_cpu_scale_coverage_invalid");
#endif

        const CaptureCase base = plan.front();
        const auto invalidFrame = [&](double settingsScale,
                float shellScale) {
            auto session = sessionFor(base);
            session.effectiveSettings.uiScale = settingsScale;
            session.shell.uiScale = shellScale;
            return coordinator.build(ui::makeProductUiFrameRequest(
                session, presentationFor(base), environment));
        };
        json::Value bad = json::Value::array();
        bad.asArray().push_back(badInput("ui_scale_below_0_85",
            "product_ui_scale_invalid",
            invalidFrame(0.849999, 0.849999f).errorCode));
        bad.asArray().push_back(badInput("ui_scale_above_1_35",
            "product_ui_scale_invalid",
            invalidFrame(1.350001, 1.350001f).errorCode));
        bad.asArray().push_back(badInput("non_finite_ui_scale",
            "product_ui_scale_invalid", invalidFrame(
                std::numeric_limits<double>::infinity(), 1.0f).errorCode));
        bad.asArray().push_back(badInput("shell_settings_scale_mismatch",
            "product_ui_scale_mismatch", invalidFrame(1.0, 1.1f).errorCode));
        auto incomplete = plan;
        incomplete.pop_back();
        auto duplicate = plan;
        duplicate.back() = duplicate.front();
        bad.asArray().push_back(badInput("duplicate_or_incomplete_plan",
            "plan_rejected", !validPlan(incomplete) && !validPlan(duplicate) ?
                "plan_rejected" : "plan_accepted"));
        for (const auto& row : bad.asArray())
            expect(row.at("passed").asBool(),
                "v2_cpu_bad_input_check_failed:" + row.at("id").asString());

        json::Value report = json::Value::object();
#if defined(BILLIARDGL_PHASE5_RULES_UI_V4_EXACT16)
        report["schema"] = json::Value(
            "phase5_rules_decision_ui_v4_exact16_cpu_frame_preflight_v1");
        report["status"] = json::Value(
            "CPU_FRAME_PREFLIGHT_PASS_16_OF_16_NO_GPU");
        report["case_count"] = json::Value(16);
        report["passed_count"] = json::Value(16);
#elif defined(BILLIARDGL_PHASE5_RULES_UI_V3_TARGETED)
        report["schema"] = json::Value(
            "phase5_rules_decision_ui_v3_targeted_cpu_frame_preflight_v1");
        report["status"] = json::Value(
            "CPU_FRAME_PREFLIGHT_PASS_28_OF_28_NO_GPU");
        report["case_count"] = json::Value(28);
        report["passed_count"] = json::Value(28);
#else
        report["schema"] = json::Value(
            "phase5_rules_decision_ui_v2_cpu_frame_preflight_v1");
        report["status"] = json::Value(
            "CPU_FRAME_PREFLIGHT_PASS_60_OF_60_NO_GPU");
        report["case_count"] = json::Value(60);
        report["passed_count"] = json::Value(60);
#endif
        report["bad_input_count"] = json::Value(5);
        report["bad_input_passed_count"] = json::Value(5);
        report["actual_gpu_invoked"] = json::Value(false);
        report["ns_application_invoked"] = json::Value(false);
        report["opengl_invoked"] = json::Value(false);
        report["gpu_output_created"] = json::Value(false);
        report["ui_scale_profile_counts"] = json::Value::object();
        for (const auto& value : scaleCounts)
            report["ui_scale_profile_counts"][value.first] =
                json::Value(value.second);
        report["cases"] = cases;
        report["bad_inputs"] = bad;
        std::ofstream output(argv[2], std::ios::binary | std::ios::trunc);
        expect(static_cast<bool>(output), "v2_cpu_report_unwritable");
        output << json::stringify(report) << '\n';
        expect(static_cast<bool>(output), "v2_cpu_report_write_failed");
#if defined(BILLIARDGL_PHASE5_RULES_UI_V4_EXACT16)
        std::cout << "phase5-rules-ui-v4-cpu-frame-preflight: PASS 16/16 + 5/5 bad inputs\n";
#elif defined(BILLIARDGL_PHASE5_RULES_UI_V3_TARGETED)
        std::cout << "phase5-rules-ui-v3-cpu-frame-preflight: PASS 28/28 + 5/5 bad inputs\n";
#else
        std::cout << "phase5-rules-ui-v2-cpu-frame-preflight: PASS 60/60 + 5/5 bad inputs\n";
#endif
        return 0;
    } catch (const std::exception& error) {
        std::cerr << "phase5-rules-ui-v2-cpu-frame-preflight: "
                  << error.what() << '\n';
        return 1;
    }
}
