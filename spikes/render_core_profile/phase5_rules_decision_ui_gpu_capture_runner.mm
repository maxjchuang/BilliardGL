#import <Cocoa/Cocoa.h>
#import <OpenGL/gl3.h>

#include "automation_json.h"
#include "deterministic_hash.h"
#include "game_runtime.h"
#include "product_hud.h"
#include "render_state.h"
#include "render/modern/modern_renderer.h"
#include "render/ui/product_ui_frame_coordinator.h"
#include "render/ui/rules_decision_ui_draw_list.h"

#include <fcntl.h>
#include <unistd.h>

#include <algorithm>
#include <ctime>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <iterator>
#include <set>
#include <sstream>
#include <stdexcept>
#include <string>
#include <vector>

#ifndef BILLIARDGL_PHASE5_RULES_UI_RUNNER_SOURCE_SHA
#define BILLIARDGL_PHASE5_RULES_UI_RUNNER_SOURCE_SHA "missing"
#endif

#if defined(BILLIARDGL_PHASE5_RULES_UI_V4_EXACT16) && \
    defined(BILLIARDGL_PHASE5_RULES_UI_V3_TARGETED)
#error "Phase5 Rules UI capture runner modes are mutually exclusive"
#endif

namespace {

using namespace billiardgl;
using namespace billiardgl::render;
namespace ui = billiardgl::render::ui;

constexpr const char* kGateSha =
#ifdef BILLIARDGL_PHASE5_RULES_UI_V5_EXACT16
    "657f3ab6f4ccac4db23fb0ab72889bc49d091f27fd5f63bd5610b736b1aa2a10";
#elif defined(BILLIARDGL_PHASE5_RULES_UI_V4_EXACT16)
    "f93ca59634875ed5256f5c22678a55817e0f3e8bf8df582b3b04ac527b886051";
#elif defined(BILLIARDGL_PHASE5_RULES_UI_V3_TARGETED)
    "6b587c67721521607d81a28beaafa59ad4a76d664399805e922ecdc969905e25";
#else
    "26567d0547163561ed77e6e41b32753c4cc55efcd693c9ce22faf847d43f5a08";
#endif
constexpr const char* kHudTokenSha =
    "abf73aa283279c28677b5c97a9b02b3130b0d0125048c3124c85a9c0e72df860";
constexpr const char* kShellTokenSha =
    "c144f1a25d3e7ced14e385bd6f76ca7e0f05b06597dcc8a4bacb4c570fc6bad3";

void expect(bool value, const std::string& message)
{
    if (!value) throw std::runtime_error(message);
}

std::string bytes(const std::filesystem::path& path)
{
    std::ifstream input(path, std::ios::binary);
    expect(static_cast<bool>(input), "phase5_rules_ui_file_unreadable:" +
        path.string());
    return {std::istreambuf_iterator<char>(input),
        std::istreambuf_iterator<char>()};
}

std::string fileSha(const std::filesystem::path& path)
{
    return sha256Hex(bytes(path));
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
    int logicalWidth = 1280;
    int logicalHeight = 720;
    int physicalWidth = 1280;
    int physicalHeight = 720;
    float density = 1.0f;
    double uiScale = 1.0;
    float dpi = 96.0f;
};

CaptureCase parseCase(const std::string& line)
{
    const auto fields = split(line);
    expect(fields.size() == 12, "phase5_rules_ui_plan_field_count");
    return {fields[0], fields[1], fields[2], fields[3], fields[4],
        std::stoi(fields[5]), std::stoi(fields[6]), std::stoi(fields[7]),
        std::stoi(fields[8]), std::stof(fields[9]), std::stod(fields[10]),
        std::stof(fields[11])};
}

std::vector<CaptureCase> loadPlan(const std::filesystem::path& path)
{
    std::ifstream input(path);
    expect(static_cast<bool>(input), "phase5_rules_ui_plan_unreadable");
    std::vector<CaptureCase> result;
    std::string line;
    while (std::getline(input, line))
        if (!line.empty()) result.push_back(parseCase(line));
#ifdef BILLIARDGL_PHASE5_RULES_UI_V4_EXACT16
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
    expect(result.size() == expectedIds.size(),
        "phase5_rules_ui_v4_plan_requires_exactly_16_cases");
    for (std::size_t index = 0; index < expectedIds.size(); ++index)
        expect(result[index].id == expectedIds[index],
            "phase5_rules_ui_v4_plan_id_or_order_mismatch");
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
    expect(result.size() == expectedIds.size(),
        "phase5_rules_ui_v3_plan_requires_exactly_28_cases");
    for (std::size_t index = 0; index < expectedIds.size(); ++index)
        expect(result[index].id == expectedIds[index],
            "phase5_rules_ui_v3_plan_id_or_order_mismatch");
#else
    expect(result.size() == 60, "phase5_rules_ui_plan_requires_60_cases");
#endif
    std::set<std::string> ids;
    for (const auto& item : result)
        expect(ids.insert(item.id).second,
            "phase5_rules_ui_plan_duplicate_case:" + item.id);
    return result;
}

void validateAndConsumeAuthorization(const std::filesystem::path& receiptPath,
    const std::filesystem::path& planPath,
    const std::filesystem::path& billiardsPath,
    const std::filesystem::path& fontPath,
    const std::filesystem::path& bindingsPath,
    const std::filesystem::path& runnerPath,
    const std::filesystem::path& outputPath,
    const std::filesystem::path& readinessPath,
    const std::filesystem::path& dispatcherPath)
{
    const std::string receiptBytes = bytes(receiptPath);
    const json::ParseResult parsed = json::parse(receiptBytes);
    expect(parsed.ok && parsed.value.isObject(),
        "phase5_rules_ui_authorization_invalid_json");
    const json::Value& receipt = parsed.value;
    expect(receipt.at("schema").asString() ==
#ifdef BILLIARDGL_PHASE5_RULES_UI_V4_EXACT16
            "phase5_rules_decision_ui_v4_exact16_gpu_authorization_v2" &&
#elif defined(BILLIARDGL_PHASE5_RULES_UI_V3_TARGETED)
            "phase5_rules_decision_ui_v3_targeted_gpu_authorization_v1" &&
#else
            "phase5_rules_decision_ui_gpu_authorization_v1" &&
#endif
        receipt.at("authorized").asBool() &&
        !receipt.at("consumed").asBool() &&
        receipt.at("case_count").asInt() ==
#ifdef BILLIARDGL_PHASE5_RULES_UI_V4_EXACT16
            16,
#elif defined(BILLIARDGL_PHASE5_RULES_UI_V3_TARGETED)
            28,
#else
            60,
#endif
        "phase5_rules_ui_authorization_not_explicit");
    expect(!receipt.at("authorization_id").asString().empty() &&
        !receipt.at("approved_by").asString().empty() &&
        !receipt.at("approved_at").asString().empty() &&
        receipt.at("expires_unix_seconds").asNumber() >
            static_cast<double>(std::time(nullptr)),
        "phase5_rules_ui_authorization_identity_or_expiry_invalid");
    expect(
#ifndef BILLIARDGL_PHASE5_RULES_UI_V4_EXACT16
        receipt.at(
#ifdef BILLIARDGL_PHASE5_RULES_UI_V3_TARGETED
            "visual_repair_design_gate_sha256"
#else
            "design_gate_sha256"
#endif
            ).asString() == kGateSha &&
#endif
        receipt.at("hud_token_sha256").asString() == kHudTokenSha &&
        receipt.at("shell_token_sha256").asString() == kShellTokenSha &&
        receipt.at("runner_source_sha256").asString() ==
            BILLIARDGL_PHASE5_RULES_UI_RUNNER_SOURCE_SHA,
        "phase5_rules_ui_authorization_frozen_binding_mismatch");
    expect(receipt.at("plan_sha256").asString() == fileSha(planPath) &&
        receipt.at("billiards_binary_sha256").asString() ==
            fileSha(billiardsPath) &&
        receipt.at("font_sha256").asString() == fileSha(fontPath) &&
        receipt.at("source_binding_manifest_sha256").asString() ==
            fileSha(bindingsPath) &&
        receipt.at("runner_binary_sha256").asString() == fileSha(runnerPath),
        "phase5_rules_ui_authorization_runtime_binding_mismatch");
    const auto bindingParsed = json::parse(bytes(bindingsPath));
    expect(bindingParsed.ok &&
        bindingParsed.value.at(
#ifdef BILLIARDGL_PHASE5_RULES_UI_V4_EXACT16
            "gate_sha256"
#elif defined(BILLIARDGL_PHASE5_RULES_UI_V3_TARGETED)
            "visual_repair_design_gate_sha256"
#else
            "design_gate_sha256"
#endif
            ).asString() == kGateSha &&
        bindingParsed.value.at("aggregate_sha256").asString() ==
            receipt.at("source_aggregate_sha256").asString(),
        "phase5_rules_ui_source_binding_manifest_mismatch");

#ifdef BILLIARDGL_PHASE5_RULES_UI_V4_EXACT16
    expect(receipt.at("status").asString() ==
            "AUTHORIZED_FOR_EXACTLY_16_SINGLE_USE_CAPTURES" &&
        receipt.at("single_use").asBool() &&
        receipt.at("exact16_capture_producer_contract").asBool() &&
        receipt.at("authorized_output_canonical_path").asString() ==
            outputPath.string() &&
        receipt.at("ready_manifest_sha256").asString() ==
            fileSha(readinessPath) &&
        receipt.at("dispatcher_sha256").asString() ==
            fileSha(dispatcherPath) &&
        receipt.at("runner_binary_path").asString() == runnerPath.string(),
        "phase5_rules_ui_v4_authorization_dispatch_binding_mismatch");
    const auto readinessParsed = json::parse(bytes(readinessPath));
    expect(readinessParsed.ok &&
        readinessParsed.value.at("schema").asString() ==
#ifdef BILLIARDGL_PHASE5_RULES_UI_V5_EXACT16
            "phase5_rules_decision_ui_v5_exact16_readiness_v2" &&
#else
            "phase5_rules_decision_ui_v4_exact16_readiness_v3" &&
#endif
        readinessParsed.value.at("frozen_bindings").at(
            "plan_sha256").asString() == fileSha(planPath) &&
        readinessParsed.value.at("frozen_bindings").at(
            "source_binding_manifest_sha256").asString() ==
            fileSha(bindingsPath),
        "phase5_rules_ui_v4_readiness_binding_mismatch");
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
    const auto& authorizedIds = receipt.at("scene_ids_in_order").asArray();
    expect(authorizedIds.size() == expectedIds.size(),
        "phase5_rules_ui_v4_authorization_scene_count_mismatch");
    for (std::size_t index = 0; index < expectedIds.size(); ++index)
        expect(authorizedIds[index].asString() == expectedIds[index],
            "phase5_rules_ui_v4_authorization_scene_order_mismatch");
    const std::filesystem::path marker = receiptPath.string() + ".consumed.json";
    const auto markerParsed = json::parse(bytes(marker));
    expect(markerParsed.ok &&
        markerParsed.value.at("authorization_id").asString() ==
            receipt.at("authorization_id").asString() &&
        markerParsed.value.at("authorization_nonce").asString() ==
            receipt.at("authorization_nonce").asString() &&
        markerParsed.value.at("ready_manifest_sha256").asString() ==
            fileSha(readinessPath) &&
        markerParsed.value.at("output_root").asString() == outputPath.string() &&
        markerParsed.value.at("runner_binary_sha256").asString() ==
            fileSha(runnerPath) &&
        markerParsed.value.at("consumed_before_output_or_process").asBool(),
        "phase5_rules_ui_v4_dispatch_consumption_marker_mismatch");
    const std::filesystem::path runnerClaim = marker.string() +
        ".exact16-runner-claimed";
    const int claimDescriptor = ::open(runnerClaim.c_str(),
        O_WRONLY | O_CREAT | O_EXCL, 0600);
    expect(claimDescriptor >= 0,
        "phase5_rules_ui_v4_runner_invocation_already_claimed");
    const std::string claimText = receipt.at("authorization_id").asString() +
        "\n";
    const ssize_t claimWritten = ::write(claimDescriptor, claimText.data(),
        claimText.size());
    expect(claimWritten == static_cast<ssize_t>(claimText.size()) &&
        ::fsync(claimDescriptor) == 0 && ::close(claimDescriptor) == 0,
        "phase5_rules_ui_v4_runner_claim_durability_failed");
    expect(!std::filesystem::exists(outputPath),
        "phase5_rules_ui_v4_output_exists_before_capture");
#else

    // Single-use marker is created atomically before NSApplication, Cocoa,
    // OpenGL, or any output directory is touched.
    const std::filesystem::path marker = receiptPath.string() +
        ".consumed-" + sha256Hex(receiptBytes);
    const int descriptor = ::open(marker.c_str(),
        O_WRONLY | O_CREAT | O_EXCL, 0600);
    expect(descriptor >= 0,
        "phase5_rules_ui_authorization_already_consumed");
    const std::string markerText = receipt.at("authorization_id").asString() +
        "\n";
    const ssize_t written = ::write(descriptor, markerText.data(),
        markerText.size());
    expect(written == static_cast<ssize_t>(markerText.size()),
        "phase5_rules_ui_authorization_marker_write_failed");
    expect(::fsync(descriptor) == 0,
        "phase5_rules_ui_authorization_marker_fsync_failed");
    expect(::close(descriptor) == 0,
        "phase5_rules_ui_authorization_marker_close_failed");
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
    result.offer.decisionId = "phase5.capture." + capture.id;
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
    result.shell.screenId = "phase5_rules_decision_gpu_capture";
    result.shell.uiScale = capture.uiScale;
    result.effectiveSettings = settings::ProductSettings::defaults();
    result.effectiveSettings.uiScale = capture.uiScale;
    result.effectiveSettings.colorVisionMode = capture.palette == "standard" ?
        settings::ColorVisionMode::Default :
        settings::ColorVisionMode::Deuteranopia;
    result.shellViewport = {capture.logicalWidth, capture.logicalHeight,
        capture.physicalWidth, capture.physicalHeight, capture.dpi};
    result.rulesDecisionUiImplementationEnabled = true;
    result.rulesDecisionUiProductExposed = false;
    result.rulesDecisionLocale = capture.locale == "en" ?
        ui::RulesDecisionLocale::En : ui::RulesDecisionLocale::ZhCn;
    result.rulesDecision = snapshotFor(capture);
    return result;
}

PresentationSnapshot presentationFor(const CaptureCase& capture,
    GameRuntime& runtime)
{
    PresentationSnapshot result = buildPresentationSnapshot(
        runtime.state(), runtime.state(), 0.0);
    result.hud.viewportWidth = capture.physicalWidth;
    result.hud.viewportHeight = capture.physicalHeight;
    result.camera.viewportWidth = capture.physicalWidth;
    result.camera.viewportHeight = capture.physicalHeight;
    result.rules.enabled = true;
    result.rules.rulesetId = rules::ui::kRulesDecisionUiRulesetId;
    return result;
}

ModernRendererOptions rendererOptions(const std::string& fontPath)
{
    ModernRendererOptions options;
    options.msaaSamples = 0;
    options.quality = RenderQuality::Medium;
    options.synchronousGpuTiming = true;
    options.enableProductUi = true;
    options.suppressLegacyUi = true;
    options.productUiSourceRoot = BILLIARDGL_SOURCE_ROOT;
    options.productUiFontPath = fontPath;
    options.productUiRulesDecisionImplementationCapability = true;
    options.lookdevManifestPath = std::string(BILLIARDGL_SOURCE_ROOT) +
        "/assets/modern/lookdev/focused_practice_hall_v1.lookdev.json";
    const std::string phase2 = std::string(BILLIARDGL_SOURCE_ROOT) +
        "/assets/modern/phase2/";
    options.pbrParametersPath = phase2 +
        "focused_practice_hall_pbr_v1.params.json";
    options.pbrAssetManifestPath = phase2 +
        "focused_practice_hall_modular_v1.asset.json";
    options.lightingRigPath = phase2 +
        "focused_practice_hall_lighting_rig_v1.json";
    options.gltfScenePath = phase2 +
        "focused_practice_hall_modular_v1.gltf";
    options.venueScenePath = phase2 +
        "focused_practice_hall_venue_v1.gltf";
    return options;
}

ui::SdfFontAtlas auditAtlas(const std::string& fontPath)
{
    std::set<std::uint32_t> unique;
    const auto hud = ui::requiredHudCodepoints();
    unique.insert(hud.begin(), hud.end());
    for (const auto& text : ui::allRulesDecisionUiStrings()) {
        std::vector<std::uint32_t> decoded;
        std::string error;
        expect(ui::decodeUtf8(text, decoded, error),
            "phase5_rules_ui_copy_utf8_invalid");
        unique.insert(decoded.begin(), decoded.end());
    }
    ui::FontAtlasConfig config;
    config.fontPath = fontPath;
    config.nominalPixelSize = 32;
    config.atlasWidth = 4096;
    config.atlasHeight = 4096;
    ui::SdfFontAtlas atlas;
    std::vector<std::uint32_t> codepoints(unique.begin(), unique.end());
    expect(atlas.initialize(config, std::move(codepoints)).ok,
        "phase5_rules_ui_audit_atlas_failed");
    return atlas;
}

json::Value resultJson(const CaptureCase& capture,
    const std::filesystem::path& ppm, const ui::ProductUiFrame& frame,
    const ModernProductUiRuntimeResult& gpu,
    const ModernFrameMetrics& metrics, const std::string& vendor,
    const std::string& device, const std::string& version)
{
    json::Value value = json::Value::object();
    value["capture_id"] = json::Value(capture.id);
    value["state"] = json::Value(capture.state);
    value["ppm_path"] = json::Value(ppm.string());
    value["ppm_sha256"] = json::Value(fileSha(ppm));
    value["canonical_sha256"] = json::Value(frame.evidenceSha256);
    value["gpu_canonical_sha256"] = json::Value(gpu.canonicalSha256);
    value["gpu_draw_calls"] = json::Value(
        static_cast<int>(gpu.gpuMetrics.drawCalls));
    value["gl_vendor"] = json::Value(vendor);
    value["gl_device"] = json::Value(device);
    value["gl_version"] = json::Value(version);
    json::Value hits = json::Value::array();
    for (const auto& hit : frame.rulesDecisionHitRects) {
        json::Value row = json::Value::object();
        row["control_id"] = json::Value(hit.controlId);
        row["action_id"] = json::Value(hit.actionId);
        row["enabled"] = json::Value(hit.enabled);
        row["confirm"] = json::Value(hit.confirm);
        hits.asArray().push_back(row);
    }
    value["hit_rectangles"] = hits;
    json::Value passes = json::Value::array();
    int tone = -1;
    int productUi = -1;
    for (std::size_t index = 0; index < metrics.passes.size(); ++index) {
        passes.asArray().push_back(json::Value(metrics.passes[index].name));
        if (metrics.passes[index].name == "aces_tone_map")
            tone = static_cast<int>(index);
        if (metrics.passes[index].name == "product_ui")
            productUi = static_cast<int>(index);
    }
    expect(tone >= 0 && productUi > tone,
        "phase5_rules_ui_not_after_tone_map:" + capture.id);
    value["render_passes"] = passes;
    value["rules_ui_after_tone_map"] = json::Value(true);
    value["actual_gpu_capture"] = json::Value(true);
#ifdef BILLIARDGL_PHASE5_RULES_UI_V4_EXACT16
    value["physical_width"] = json::Value(capture.physicalWidth);
    value["physical_height"] = json::Value(capture.physicalHeight);
    value["authorization_consumed_before_process"] = json::Value(true);
#endif
    return value;
}

}  // namespace

int main(int argc, const char* const* argv)
{
    @autoreleasepool {
        try {
#ifdef BILLIARDGL_PHASE5_RULES_UI_V4_EXACT16
            expect(argc == 10,
                "usage: phase5_rules_ui_v4_exact16_gpu_runner AUTH PLAN OUTPUT RESULTS BILLIARDS FONT SOURCE_BINDINGS READINESS DISPATCHER");
#else
            expect(argc == 8,
                "usage: phase5_rules_ui_gpu_runner AUTH PLAN OUTPUT RESULTS BILLIARDS FONT SOURCE_BINDINGS");
#endif
            const std::filesystem::path receipt(argv[1]);
            const std::filesystem::path plan(argv[2]);
            const std::filesystem::path output(argv[3]);
            const std::filesystem::path resultsPath(argv[4]);
            const std::filesystem::path billiards(argv[5]);
            const std::filesystem::path font(argv[6]);
            const std::filesystem::path bindings(argv[7]);
            const std::filesystem::path runner(argv[0]);
#ifdef BILLIARDGL_PHASE5_RULES_UI_V4_EXACT16
            const std::filesystem::path readiness(argv[8]);
            const std::filesystem::path dispatcher(argv[9]);
#else
            const std::filesystem::path readiness;
            const std::filesystem::path dispatcher;
#endif
            const auto captures = loadPlan(plan);
            validateAndConsumeAuthorization(receipt, plan, billiards, font,
                bindings, runner, output, readiness, dispatcher);

            std::filesystem::create_directories(output);
#ifdef BILLIARDGL_PHASE5_RULES_UI_V4_EXACT16
            std::filesystem::create_directories(output / "ppm");
#endif
            [NSApplication sharedApplication];
            const NSOpenGLPixelFormatAttribute attributes[] = {
                NSOpenGLPFAOpenGLProfile,
                static_cast<NSOpenGLPixelFormatAttribute>(
                    NSOpenGLProfileVersion4_1Core),
                NSOpenGLPFAAccelerated, NSOpenGLPFADoubleBuffer,
                NSOpenGLPFAColorSize, 24, NSOpenGLPFADepthSize, 24,
                static_cast<NSOpenGLPixelFormatAttribute>(0)};
            NSOpenGLPixelFormat* format = [[NSOpenGLPixelFormat alloc]
                initWithAttributes:attributes];
            expect(format != nil, "phase5_rules_ui_core41_unavailable");
            NSOpenGLContext* context = [[NSOpenGLContext alloc]
                initWithFormat:format shareContext:nil];
            expect(context != nil, "phase5_rules_ui_context_failed");
            NSWindow* window = [[NSWindow alloc] initWithContentRect:
                NSMakeRect(0, 0, 1920, 1080)
                styleMask:NSWindowStyleMaskBorderless
                backing:NSBackingStoreBuffered defer:NO];
            [window setReleasedWhenClosed:NO];
            [context setView:[window contentView]];
            [context makeCurrentContext];
            [context update];
            const auto glText = [](GLenum name) {
                const GLubyte* text = glGetString(name);
                return text == nullptr ? std::string("unavailable") :
                    std::string(reinterpret_cast<const char*>(text));
            };

            ModernRenderer renderer(rendererOptions(font.string()));
            std::string error;
            expect(renderer.initialize({captures.front().physicalWidth,
                captures.front().physicalHeight, true}, error), error);
            const ui::SdfFontAtlas atlas = auditAtlas(font.string());
            const auto manifest = product_hud::loadProductHudManifest(
                std::string(BILLIARDGL_SOURCE_ROOT) +
                "/assets/modern/hud/focused_practice_hud_v1.hud.json");
            const auto registry = ui::mergeHudShippingTokenSets({
                ui::loadHudShippingTokenSet(
                    std::string(BILLIARDGL_SOURCE_ROOT) +
                    "/assets/modern/hud/focused_practice_hud_shipping_tokens_v1.json"),
                ui::loadShellShippingTokenSet(
                    std::string(BILLIARDGL_SOURCE_ROOT) +
                    "/assets/modern/hud/focused_practice_shell_shipping_tokens_v1.json")});
            expect(manifest.ok && registry.ok,
                "phase5_rules_ui_audit_assets_failed");
            ui::ProductUiFrameCoordinator coordinator(manifest, atlas, registry);
            std::ofstream results(resultsPath,
                std::ios::binary | std::ios::trunc);
            expect(static_cast<bool>(results),
                "phase5_rules_ui_results_unwritable");
#ifdef BILLIARDGL_PHASE5_RULES_UI_V4_EXACT16
            std::ofstream telemetry(output /
                "rules-decision-ui-v4-exact16-telemetry.jsonl",
                std::ios::binary | std::ios::trunc);
            expect(static_cast<bool>(telemetry),
                "phase5_rules_ui_v4_telemetry_unwritable");
#endif
            GameRuntime runtime;
            int width = captures.front().physicalWidth;
            int height = captures.front().physicalHeight;
            const ui::ProductUiFrameEnvironment environment{
                true, false, false, true};
            for (std::size_t index = 0; index < captures.size(); ++index) {
                const CaptureCase& capture = captures[index];
                if (capture.physicalWidth != width ||
                        capture.physicalHeight != height) {
                    renderer.resize(capture.physicalWidth,
                        capture.physicalHeight);
                    width = capture.physicalWidth;
                    height = capture.physicalHeight;
                }
                const ModernProductSessionSnapshot session =
                    sessionFor(capture);
                const PresentationSnapshot presentation =
                    presentationFor(capture, runtime);
                const ui::ProductUiFrame frame = coordinator.build(
                    ui::makeProductUiFrameRequest(session, presentation,
                        environment));
                expect(frame.ok && frame.rulesDecisionActive,
                    "phase5_rules_ui_cpu_frame_failed:" + capture.id + ":" +
                        frame.errorCode);
                renderer.renderProductFrame(presentation, session, environment);
                glFinish();
                const ModernProductUiRuntimeResult gpu =
                    renderer.latestProductUiResult();
                expect(gpu.ok && gpu.gpuSubmitted &&
                    gpu.canonicalSha256 == frame.evidenceSha256,
                    "phase5_rules_ui_gpu_submit_mismatch:" + capture.id);
#ifdef BILLIARDGL_PHASE5_RULES_UI_V4_EXACT16
                const std::filesystem::path ppm = output / "ppm" /
                    (capture.id + ".ppm");
#else
                const std::filesystem::path ppm = output /
                    (capture.id + ".ppm");
#endif
                expect(renderer.capture({ppm.string(), capture.physicalWidth,
                    capture.physicalHeight}, error), error);
                results << json::stringify(resultJson(capture, ppm, frame,
                    gpu, renderer.metrics(), glText(GL_VENDOR),
                    glText(GL_RENDERER), glText(GL_VERSION))) << '\n';
                results.flush();
                expect(static_cast<bool>(results),
                    "phase5_rules_ui_result_write_failed");
#ifdef BILLIARDGL_PHASE5_RULES_UI_V4_EXACT16
                json::Value telemetryRow = json::Value::object();
                telemetryRow["capture_id"] = json::Value(capture.id);
                telemetryRow["machine_status"] = json::Value("PASS");
                telemetryRow["actual_gpu_capture"] = json::Value(true);
                telemetryRow["rules_ui_after_tone_map"] = json::Value(true);
                telemetry << json::stringify(telemetryRow) << '\n';
                telemetry.flush();
                expect(static_cast<bool>(telemetry),
                    "phase5_rules_ui_v4_telemetry_write_failed");
#endif
            }
            renderer.shutdown();
            [NSOpenGLContext clearCurrentContext];
#ifdef BILLIARDGL_PHASE5_RULES_UI_V4_EXACT16
            std::cout << "phase5-rules-ui-gpu-capture: EVIDENCE_CAPTURED 16/16\n";
#else
            std::cout << "phase5-rules-ui-gpu-capture: EVIDENCE_CAPTURED 60/60\n";
#endif
            return 0;
        } catch (const std::exception& error) {
            std::cerr << "phase5-rules-ui-gpu-capture: " << error.what()
                      << '\n';
            return 1;
        }
    }
}
