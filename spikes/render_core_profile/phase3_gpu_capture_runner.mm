#import <Cocoa/Cocoa.h>
#import <OpenGL/gl3.h>

#include "ai/ai_player_controller.h"
#include "automation_json.h"
#include "deterministic_hash.h"
#include "game_runtime.h"
#include "product_hud.h"
#include "product_settings.h"
#include "render_state.h"
#include "render/modern/modern_product_session.h"
#include "render/modern/modern_renderer.h"
#include "render/ui/product_ui_frame_coordinator.h"
#include "render/ui/sdf_font_atlas.h"
#include "replay/replay_recorder.h"

#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <iterator>
#include <memory>
#include <map>
#include <set>
#include <sstream>
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
    std::string surface;
    std::string state;
    std::string scenario;
    std::string camera;
    std::string viewport;
    std::string palette;
    double uiScale = 1.0;
    int logicalWidth = 1280;
    int logicalHeight = 720;
    int physicalWidth = 1280;
    int physicalHeight = 720;
    float dpi = 96.0f;
    float minimumTextPhysicalPx = 0.0f;
};

struct AssetBinding {
    std::filesystem::path root;
    std::filesystem::path gltf;
    std::filesystem::path manifest;
    std::string gltfSha256;
    std::string manifestSha256;
};

std::string readBinaryFile(const std::filesystem::path& path)
{
    std::ifstream stream(path, std::ios::binary);
    expect(static_cast<bool>(stream), "phase3_asset_binding_read_failed:" +
        path.string());
    return std::string(std::istreambuf_iterator<char>(stream),
        std::istreambuf_iterator<char>());
}

AssetBinding captureAssetBinding()
{
    const char* overrideRoot = std::getenv("BILLIARDGL_MODERN_PHASE2_ROOT");
    const std::filesystem::path selected = overrideRoot != nullptr
        ? std::filesystem::path(overrideRoot)
        : std::filesystem::path(BILLIARDGL_SOURCE_ROOT) /
            "assets/modern/phase2";
    AssetBinding binding;
    binding.root = std::filesystem::canonical(selected);
    binding.gltf = binding.root / "focused_practice_hall_modular_v1.gltf";
    binding.manifest = binding.root /
        "focused_practice_hall_modular_v1.asset.json";
    binding.gltfSha256 = sha256Hex(readBinaryFile(binding.gltf));
    binding.manifestSha256 = sha256Hex(readBinaryFile(binding.manifest));
    return binding;
}

CaptureCase parseCase(const std::string& line)
{
    const std::vector<std::string> fields = split(line);
    expect(fields.size() == 14, "phase3_plan_field_count");
    CaptureCase result;
    result.id = fields[0];
    result.surface = fields[1];
    result.state = fields[2];
    result.scenario = fields[3];
    result.camera = fields[4];
    result.viewport = fields[5];
    result.palette = fields[6];
    result.uiScale = std::stod(fields[7]);
    result.logicalWidth = std::stoi(fields[8]);
    result.logicalHeight = std::stoi(fields[9]);
    result.physicalWidth = std::stoi(fields[10]);
    result.physicalHeight = std::stoi(fields[11]);
    result.dpi = std::stof(fields[12]);
    result.minimumTextPhysicalPx = std::stof(fields[13]);
    return result;
}

struct ReplayFixture {
    std::filesystem::path path;
    std::string executable = "phase3-gpu-capture-replay-v1";
    std::string content = sha256Hex("phase3-gpu-capture-replay-content");

    ~ReplayFixture()
    {
        std::error_code ignored;
        std::filesystem::remove(path, ignored);
    }
};

ReplayFixture makeReplay()
{
    ReplayFixture output;
    output.path = std::filesystem::temp_directory_path() /
        "billiardgl-phase3-gpu-capture.replay.json";
    GameRuntime runtime;
    replay::Recorder recorder;
    expect(recorder.begin(runtime, output.executable, output.content, 77).ok,
        "phase3_replay_begin_failed");
    GameAction power;
    power.type = ActionType::SetShotPower;
    power.first = 42.0f;
    GameAction shoot;
    shoot.type = ActionType::Shoot;
    expect(runtime.dispatch(power).ok && runtime.dispatch(shoot).ok &&
        runtime.step(12).ok && recorder.finish(runtime, 1).ok,
        "phase3_replay_record_failed");
    std::ofstream stream(output.path, std::ios::binary | std::ios::trunc);
    stream << json::stringify(replay::serialize(recorder.document())) << '\n';
    expect(static_cast<bool>(stream), "phase3_replay_write_failed");
    return output;
}

std::set<std::string> trainingCapabilities()
{
    return {"training_scenario_v1",
        "authoritative_horizontal_tip_offset_v1",
        "authoritative_longitudinal_tip_offset_v1"};
}

class SessionFixture {
public:
    SessionFixture(const CaptureCase& capture, int serial,
        const ReplayFixture& replay)
    {
        settingsPath_ = std::filesystem::temp_directory_path() /
            ("billiardgl-phase3-capture-settings-" +
                std::to_string(serial) + ".json");
        settings::ProductSettings settings =
            settings::ProductSettings::defaults();
        settings.uiScale = capture.uiScale;
        settings.colorVisionMode = capture.palette == "standard" ?
            settings::ColorVisionMode::Default :
            settings::ColorVisionMode::Deuteranopia;
        expect(settings::saveFileAtomic(settingsPath_.string(), settings).ok,
            "phase3_settings_fixture_write_failed");
        ModernProductSessionOptions options;
        options.settingsPath = settingsPath_.string();
        if (capture.surface == "hud" ||
                capture.scenario == "main_menu_training_available" ||
                capture.scenario == "training_session_active")
            options.trainingManifestPath = trainingManifest();
        session_ = std::make_unique<ModernProductSession>(
            runtime_, ai_, std::move(options));
        session_->setShellViewport({capture.logicalWidth,
            capture.logicalHeight, capture.physicalWidth,
            capture.physicalHeight, capture.dpi});
        if (capture.surface == "hud") configureHud();
        else configureShell(capture.scenario, replay);
    }

    ~SessionFixture()
    {
        session_.reset();
        std::error_code ignored;
        std::filesystem::remove(settingsPath_, ignored);
        std::filesystem::remove(settingsPath_.string() + ".tmp", ignored);
    }

    ModernProductSession& session() { return *session_; }
    GameRuntime& runtime() { return runtime_; }

private:
    static std::string trainingManifest()
    {
        return std::string(BILLIARDGL_SOURCE_ROOT) +
            "/assets/training/phase4/training_pack_v1.json";
    }

    void configureHud()
    {
        expect(session_->shell().openSinglePlayerSetup().ok,
            "phase3_hud_open_setup_failed");
        expect(session_->shell().selectDifficulty(ai::Difficulty::Club).ok,
            "phase3_hud_select_difficulty_failed");
        expect(session_->shell().startSinglePlayer(594).ok &&
            session_->snapshot().shell.screen == ProductScreen::Match,
            "phase3_hud_start_match_failed");
    }

    void configureShell(const std::string& scenario,
        const ReplayFixture& replay)
    {
        ProductShellController& shell = session_->shell();
        if (scenario == "main_menu_training_available") {
            expect(shell.snapshot().training.scenarioCount > 0,
                "phase3_main_training_not_available");
            expect(shell.focusItem("training").ok,
                "phase3_main_training_focus_failed");
            return;
        }
        if (scenario == "main_menu_training_unavailable") {
            expect(shell.snapshot().training.scenarioCount == 0 &&
                shell.focusItem("single_player").ok,
                "phase3_main_training_unavailable_fixture_failed");
            return;
        }
        if (scenario == "single_player_setup_club_selected") {
            expect(shell.openSinglePlayerSetup().ok &&
                shell.selectDifficulty(ai::Difficulty::Club).ok &&
                shell.focusItem("rookie").ok,
                "phase3_setup_fixture_failed");
            return;
        }
        if (scenario == "training_catalog_invalid_pack_error") {
            expect(!shell.loadTrainingCatalog(
                "/tmp/billiardgl-phase3-missing-training-pack.json",
                trainingCapabilities()).ok &&
                shell.snapshot().screen == ProductScreen::TrainingCatalog &&
                !shell.snapshot().errorText.empty(),
                "phase3_training_invalid_pack_fixture_failed");
            return;
        }
        if (scenario == "training_session_active") {
            expect(shell.loadTrainingCatalog(trainingManifest(),
                trainingCapabilities()).ok, "phase3_training_catalog_failed");
            const auto cards = shell.snapshot().items;
            expect(!cards.empty() && shell.startTrainingScenario(
                cards.front().id, "phase3-gpu-capture-v1", 594).ok &&
                shell.focusItem("reset_training").ok,
                "phase3_training_session_fixture_failed");
            return;
        }
        if (scenario == "replay_browser_invalid_path_error") {
            expect(shell.openReplayBrowser().ok &&
                shell.focusItem("choose_replay").ok,
                "phase3_replay_browser_failed");
            expect(!shell.loadReplayPath("", replay.executable,
                replay.content).ok,
                "phase3_replay_error_fixture_failed");
            return;
        }
        if (scenario == "replay_playback_paused") {
            expect(shell.openReplayBrowser().ok && shell.loadReplayPath(
                replay.path.string(), replay.executable, replay.content).ok &&
                shell.focusItem("replay_pause").ok,
                "phase3_replay_playback_fixture_failed");
            return;
        }
        if (scenario.rfind("settings_", 0) == 0) {
            expect(shell.openSettings().ok,
                "phase3_settings_open_failed");
            int category = 0;
            std::string focus = "assist_level";
            if (scenario == "settings_input") {
                category = 1; focus = "mouse_sensitivity";
            } else if (scenario == "settings_accessibility") {
                category = 2; focus = "ui_scale";
            } else if (scenario == "settings_audio") {
                category = 3; focus = "music_volume";
            } else if (scenario == "settings_display") {
                category = 4; focus = "field_of_view";
            }
            for (int step = 0; step < category; ++step)
                expect(session_->settings().handleMenu(
                    settings::MenuCommand::CategoryNext).ok,
                    "phase3_settings_category_failed");
            expect(shell.focusItem(focus).ok,
                "phase3_settings_focus_failed");
            return;
        }
        if (scenario == "help_back_focused") {
            expect(shell.openHelp().ok && shell.focusItem("help_back").ok,
                "phase3_help_fixture_failed");
            return;
        }
        throw std::runtime_error("phase3_scenario_unknown:" + scenario);
    }

    std::filesystem::path settingsPath_;
    GameRuntime runtime_;
    ai::AiPlayerController ai_;
    std::unique_ptr<ModernProductSession> session_;
};

camera::Preset cameraPreset(const std::string& id)
{
    if (id == "player_low") return camera::Preset::PlayerLow;
    if (id == "elevated") return camera::Preset::Elevated;
    if (id == "overhead") return camera::Preset::Overhead;
    if (id == "broadcast") return camera::Preset::Broadcast;
    throw std::runtime_error("phase3_camera_unknown:" + id);
}

PresentationSnapshot presentationFor(const CaptureCase& capture,
    GameRuntime& runtime)
{
    PresentationSnapshot seed = buildPresentationSnapshot(runtime.state(),
        runtime.state(), 0.0);
    camera::StateMachine camera;
    const camera::Preset preset = capture.surface == "hud" ?
        cameraPreset(capture.camera) : camera::Preset::Elevated;
    const camera::SemanticState semantic = preset == camera::Preset::Broadcast ?
        camera::SemanticState::ReplayBroadcast :
        preset == camera::Preset::Overhead ? camera::SemanticState::AimOverhead :
        preset == camera::Preset::PlayerLow ? camera::SemanticState::AimPlayer :
        camera::SemanticState::ObserveFree;
    camera.reset(semantic, preset, seed);
    PresentationSnapshot result = buildPresentationSnapshot(runtime.state(),
        runtime.state(), 0.0, runtime.shotControl(),
        runtime.shotControlCapabilities(), camera.frame(),
        buildRulesPresentation(runtime));
    result.hud.viewportWidth = capture.physicalWidth;
    result.hud.viewportHeight = capture.physicalHeight;
    result.camera.viewportWidth = capture.physicalWidth;
    result.camera.viewportHeight = capture.physicalHeight;
    result.hud.aimMode = false;
    result.hud.showHelp = false;
    result.events.shotEnded = false;
    result.rules.foul = false;
    result.rules.gameOver = false;
    result.gameOver = false;
    result.ballsMoving = false;
    if (capture.state == "aiming") {
        result.hud.aimMode = true;
        result.hud.showPowerMeter = true;
        result.shotControl.requestedPower = 64.0f;
        result.shotControl.tipOffsetX = -0.25f;
        result.shotControl.tipOffsetY = 0.50f;
    } else if (capture.state == "post_shot") {
        result.events.shotEnded = true;
    } else if (capture.state == "foul") {
        result.events.shotEnded = true;
        result.rules.enabled = true;
        result.rules.foul = true;
        result.rules.hasAdjudication = true;
        result.rules.reasonCodes = {"wrong_first_contact"};
    } else if (capture.state == "help") {
        result.hud.showHelp = true;
    }
    return result;
}

bool contains(const product_hud::HudRect& outer,
    const product_hud::HudRect& inner)
{
    constexpr float epsilon = 0.02f;
    return inner.x + epsilon >= outer.x && inner.y + epsilon >= outer.y &&
        inner.x + inner.width <= outer.x + outer.width + epsilon &&
        inner.y + inner.height <= outer.y + outer.height + epsilon;
}

bool overlaps(const product_hud::HudRect& first,
    const product_hud::HudRect& second)
{
    return first.x < second.x + second.width &&
        first.x + first.width > second.x &&
        first.y < second.y + second.height &&
        first.y + first.height > second.y;
}

struct MachineMetrics {
    int safeAreaViolations = 0;
    int truncatedTextRuns = 0;
    int overlapPairs = 0;
    double minimumRenderedTextPx = 0.0;
    int expandedIconCount = 0;
    int opaqueIconCount = 0;
    int unexpandedIconCount = 0;
    int focusIndicatorCount = 0;
    int selectedIndicatorCount = 0;
    int errorIndicatorCount = 0;
    int disabledShapeCount = 0;
    int debugNodeCount = 0;
    int glyphRasterizationsDuringFrame = 0;
};

struct PerformanceMetrics {
    int warmupFrames = 0;
    int sampleFrames = 0;
    double p95TotalFrameMs = 0.0;
    double textureResidentMiB = 0.0;
    std::string batchId;
    std::string benchmarkCaptureId;
};

std::string focusedItemId(const ProductShellSnapshot& shell)
{
    for (const ShellItemViewModel& item : shell.items)
        if (item.focused) return item.id;
    return {};
}

void auditPublicState(const CaptureCase& capture,
    const ModernProductSessionSnapshot& session)
{
    expect(std::abs(session.effectiveSettings.uiScale - capture.uiScale) < 0.001,
        "phase3_ui_scale_selector_mismatch:" + capture.id);
    const bool colorVisionSafe = capture.palette != "standard";
    expect((session.effectiveSettings.colorVisionMode !=
        settings::ColorVisionMode::Default) == colorVisionSafe,
        "phase3_palette_selector_mismatch:" + capture.id);
    if (capture.surface == "hud") {
        expect(session.shell.screen == ProductScreen::Match,
            "phase3_hud_not_public_match:" + capture.id);
        return;
    }
    std::string expectedScreen;
    std::string expectedFocus;
    if (capture.scenario == "main_menu_training_unavailable") {
        expectedScreen = "main_menu"; expectedFocus = "single_player";
        expect(session.shell.training.scenarioCount == 0,
            "phase3_training_unavailable_selector_mismatch:" + capture.id);
    } else if (capture.scenario == "main_menu_training_available") {
        expectedScreen = "main_menu"; expectedFocus = "training";
        expect(session.shell.training.scenarioCount > 0,
            "phase3_training_available_selector_mismatch:" + capture.id);
    } else if (capture.scenario == "single_player_setup_club_selected") {
        expectedScreen = "single_player_setup"; expectedFocus = "rookie";
    } else if (capture.scenario == "training_catalog_invalid_pack_error") {
        expectedScreen = "training_catalog";
        expect(!session.shell.errorText.empty(),
            "phase3_training_error_selector_mismatch:" + capture.id);
    } else if (capture.scenario == "training_session_active") {
        expectedScreen = "training_session"; expectedFocus = "reset_training";
    } else if (capture.scenario == "replay_browser_invalid_path_error") {
        expectedScreen = "replay_browser"; expectedFocus = "choose_replay";
        expect(!session.shell.errorText.empty(),
            "phase3_replay_error_selector_mismatch:" + capture.id);
    } else if (capture.scenario == "replay_playback_paused") {
        expectedScreen = "replay_playback"; expectedFocus = "replay_pause";
        expect(session.shell.replay.paused,
            "phase3_replay_pause_selector_mismatch:" + capture.id);
    } else if (capture.scenario.rfind("settings_", 0) == 0) {
        expectedScreen = "settings";
        if (capture.scenario == "settings_gameplay") expectedFocus = "assist_level";
        else if (capture.scenario == "settings_input") expectedFocus = "mouse_sensitivity";
        else if (capture.scenario == "settings_accessibility") expectedFocus = "ui_scale";
        else if (capture.scenario == "settings_audio") expectedFocus = "music_volume";
        else if (capture.scenario == "settings_display") expectedFocus = "field_of_view";
    } else if (capture.scenario == "help_back_focused") {
        expectedScreen = "help"; expectedFocus = "help_back";
    } else {
        throw std::runtime_error("phase3_selector_audit_unknown:" + capture.scenario);
    }
    expect(session.shell.screenId == expectedScreen,
        "phase3_screen_selector_mismatch:" + capture.id);
    expect(focusedItemId(session.shell) == expectedFocus,
        "phase3_focus_selector_mismatch:" + capture.id);
}

int completeSceneDrawCalls(const ModernFrameMetrics& metrics)
{
    int result = 0;
    for (const RenderPassMetric& pass : metrics.passes)
        result += static_cast<int>(pass.drawCalls);
    return result;
}

double textureResidentMiB(const ModernFrameMetrics& metrics,
    int physicalWidth, int physicalHeight)
{
    const std::size_t framebufferBytes = static_cast<std::size_t>(physicalWidth) *
        static_cast<std::size_t>(physicalHeight) * 16u;
    const std::size_t shadowBytes = static_cast<std::size_t>(metrics.shadowMapSize) *
        static_cast<std::size_t>(metrics.shadowMapSize) * 4u;
    constexpr std::size_t productUiAtlasBytes = 1024u * 1024u;
    const std::size_t total = metrics.pbrGpuBytes + metrics.environmentGpuBytes +
        framebufferBytes + shadowBytes + productUiAtlasBytes;
    return static_cast<double>(total) / (1024.0 * 1024.0);
}

PerformanceMetrics benchmark(ModernRenderer& renderer,
    const CaptureCase& capture, const PresentationSnapshot& presentation,
    const ModernProductSessionSnapshot& session,
    const ui::ProductUiFrameEnvironment& environment,
    int warmupFrames, int sampleFrames)
{
    PerformanceMetrics result;
    result.warmupFrames = warmupFrames;
    result.sampleFrames = sampleFrames;
    result.batchId = capture.surface + "__" + capture.viewport;
    result.benchmarkCaptureId = capture.id;
    for (int frame = 0; frame < warmupFrames; ++frame) {
        renderer.renderProductFrame(presentation, session, environment);
        glFinish();
    }
    std::vector<double> samples;
    samples.reserve(static_cast<std::size_t>(sampleFrames));
    for (int frame = 0; frame < sampleFrames; ++frame) {
        const auto started = std::chrono::steady_clock::now();
        renderer.renderProductFrame(presentation, session, environment);
        glFinish();
        samples.push_back(std::chrono::duration<double, std::milli>(
            std::chrono::steady_clock::now() - started).count());
    }
    std::sort(samples.begin(), samples.end());
    const std::size_t p95Index = static_cast<std::size_t>(
        std::ceil(0.95 * static_cast<double>(samples.size()))) - 1u;
    result.p95TotalFrameMs = samples.at(p95Index);
    result.textureResidentMiB = textureResidentMiB(renderer.metrics(),
        capture.physicalWidth, capture.physicalHeight);
    return result;
}

MachineMetrics measure(const ui::ProductUiFrame& frame,
    const ModernProductSessionSnapshot& session, int rasterizationsBefore,
    int rasterizationsAfter)
{
    MachineMetrics result;
    result.expandedIconCount = frame.draw.expandedIconCount;
    result.debugNodeCount = frame.draw.containsDebug ? 1 : 0;
    result.glyphRasterizationsDuringFrame =
        rasterizationsAfter - rasterizationsBefore;
    std::vector<product_hud::HudRect> itemPanels;
    for (const ui::UiPrimitive& primitive : frame.draw.primitives) {
        if (!contains(frame.draw.safeRect, primitive.rect) ||
                !contains(frame.draw.safeRect, primitive.clipRect))
            ++result.safeAreaViolations;
        if (primitive.kind == ui::UiPrimitiveKind::Icon) {
            ++result.opaqueIconCount;
            ++result.unexpandedIconCount;
        }
        if (primitive.token == "keyboard_focus_outline")
            result.focusIndicatorCount = 1;
        if (primitive.token == "error_banner_octagon")
            result.errorIndicatorCount = 1;
        if (frame.overlay == ui::ProductUiOverlayKind::GameplayHud &&
                primitive.token == "foul_banner_panel")
            result.errorIndicatorCount = 1;
        if (primitive.kind == ui::UiPrimitiveKind::Panel &&
                primitive.token == "shell_item_panel")
            itemPanels.push_back(primitive.sourceRect);
    }
    for (const ui::UiTriangle& triangle : frame.draw.triangles) {
        if (triangle.sourceToken == "selected_check_diamond")
            result.selectedIndicatorCount = 1;
        if (triangle.sourceToken == "error_cross_octagon")
            result.errorIndicatorCount = 1;
    }
    for (const ui::TextRunMetrics& text : frame.draw.textRuns) {
        if (text.measuredWidth > text.clipRect.width + 0.02f)
            ++result.truncatedTextRuns;
        if (result.minimumRenderedTextPx == 0.0 ||
                text.requestedPixelSize < result.minimumRenderedTextPx)
            result.minimumRenderedTextPx = text.requestedPixelSize;
    }
    for (std::size_t first = 0; first < itemPanels.size(); ++first)
        for (std::size_t second = first + 1; second < itemPanels.size(); ++second)
            if (overlaps(itemPanels[first], itemPanels[second]))
                ++result.overlapPairs;
    if (frame.overlay == ui::ProductUiOverlayKind::Shell) {
        result.disabledShapeCount = static_cast<int>(std::count_if(
            session.shell.items.begin(), session.shell.items.end(),
            [](const ShellItemViewModel& item) { return !item.enabled; }));
    }
    return result;
}

json::Value resultJson(const CaptureCase& capture,
    const std::string& ppmPath, const MachineMetrics& machine,
    const ModernProductUiRuntimeResult& gpu,
    const ModernFrameMetrics& renderer, double elapsedMs,
    const PerformanceMetrics& performance,
    const std::string& glVendor, const std::string& glDevice,
    const std::string& glVersion, const ui::ProductUiFrame& frame,
    const AssetBinding& assetBinding)
{
    json::Value root = json::Value::object();
    root["capture_id"] = json::Value(capture.id);
    root["ppm_path"] = json::Value(ppmPath);
    root["elapsed_ms"] = json::Value(elapsedMs);
    root["canonical_sha256"] = json::Value(gpu.canonicalSha256);
    root["gpu_draw_calls"] = json::Value(
        static_cast<int>(gpu.gpuMetrics.drawCalls));
    root["gpu_batches"] = json::Value(
        static_cast<int>(gpu.gpuMetrics.batches));
    const int completeDraws = completeSceneDrawCalls(renderer);
    root["complete_scene_draw_calls"] = json::Value(completeDraws);
    root["gl_vendor"] = json::Value(glVendor);
    root["gl_device"] = json::Value(glDevice);
    root["gl_version"] = json::Value(glVersion);
    json::Value assets = json::Value::object();
    assets["asset_root"] = json::Value(assetBinding.root.string());
    assets["gltf_path"] = json::Value(assetBinding.gltf.string());
    assets["gltf_sha256"] = json::Value(
        "sha256:" + assetBinding.gltfSha256);
    assets["manifest_path"] = json::Value(assetBinding.manifest.string());
    assets["manifest_sha256"] = json::Value(
        "sha256:" + assetBinding.manifestSha256);
    root["asset_binding"] = assets;
    json::Value performanceJson = json::Value::object();
    performanceJson["warmup_frames"] = json::Value(performance.warmupFrames);
    performanceJson["sample_frames"] = json::Value(performance.sampleFrames);
    performanceJson["p95_total_frame_ms"] =
        json::Value(performance.p95TotalFrameMs);
    performanceJson["draw_calls_complete_scene"] = json::Value(completeDraws);
    performanceJson["texture_resident_mib"] =
        json::Value(performance.textureResidentMiB);
    performanceJson["measurement_method"] =
        json::Value("gpu_timer_query_plus_cpu_frame");
    performanceJson["batch_id"] = json::Value(performance.batchId);
    performanceJson["benchmark_capture_id"] =
        json::Value(performance.benchmarkCaptureId);
    performanceJson["texture_residency_method"] =
        json::Value("allocated_format_bytes_plus_uploaded_texture_bytes");
    root["performance"] = performanceJson;
    json::Value textRuns = json::Value::array();
    for (std::size_t index = 0; index < frame.draw.textRuns.size(); ++index) {
        const ui::TextRunMetrics& text = frame.draw.textRuns[index];
        json::Value diagnostic = json::Value::object();
        diagnostic["run_index"] = json::Value(static_cast<int>(index));
        diagnostic["text"] = json::Value(text.text);
        diagnostic["requested_pixel_size"] =
            json::Value(text.requestedPixelSize);
        diagnostic["contract_minimum_pixel_size"] =
            json::Value(capture.minimumTextPhysicalPx);
        diagnostic["measured_width"] = json::Value(text.measuredWidth);
        diagnostic["source_origin_x"] = json::Value(text.originX);
        diagnostic["source_baseline_y"] = json::Value(text.baselineY);
        diagnostic["source_width"] = json::Value(text.measuredWidth);
        diagnostic["source_nominal_height"] =
            json::Value(text.requestedPixelSize);
        diagnostic["clip_x"] = json::Value(text.clipRect.x);
        diagnostic["clip_y"] = json::Value(text.clipRect.y);
        diagnostic["clip_width"] = json::Value(text.clipRect.width);
        diagnostic["clip_height"] = json::Value(text.clipRect.height);
        diagnostic["overflow_px"] = json::Value(std::max(0.0f,
            text.measuredWidth - text.clipRect.width));
        diagnostic["below_contract_minimum"] = json::Value(
            text.requestedPixelSize + 0.001f < capture.minimumTextPhysicalPx);
        textRuns.asArray().push_back(diagnostic);
    }
    root["text_run_diagnostics"] = textRuns;
    json::Value metrics = json::Value::object();
    metrics["safe_area_violations"] = json::Value(machine.safeAreaViolations);
    metrics["truncated_text_runs"] = json::Value(machine.truncatedTextRuns);
    metrics["overlap_pairs"] = json::Value(machine.overlapPairs);
    metrics["minimum_rendered_text_px"] =
        json::Value(machine.minimumRenderedTextPx);
    metrics["expanded_icon_count"] = json::Value(machine.expandedIconCount);
    metrics["opaque_icon_count"] = json::Value(machine.opaqueIconCount);
    metrics["unexpanded_icon_count"] = json::Value(machine.unexpandedIconCount);
    metrics["focus_indicator_count"] =
        json::Value(machine.focusIndicatorCount);
    metrics["selected_indicator_count"] =
        json::Value(machine.selectedIndicatorCount);
    metrics["error_indicator_count"] =
        json::Value(machine.errorIndicatorCount);
    metrics["disabled_shape_count"] =
        json::Value(machine.disabledShapeCount);
    metrics["debug_node_count"] = json::Value(machine.debugNodeCount);
    metrics["glyph_rasterizations_during_frame"] =
        json::Value(machine.glyphRasterizationsDuringFrame);
    root["machine_metrics"] = metrics;
    return root;
}

ModernRendererOptions rendererOptions(const AssetBinding& assetBinding)
{
    ModernRendererOptions options;
    options.msaaSamples = 0;
    options.quality = RenderQuality::Medium;
    options.synchronousGpuTiming = true;
    options.enableProductUi = true;
    options.productUiSourceRoot = BILLIARDGL_SOURCE_ROOT;
    options.productUiFontPath = "/System/Library/Fonts/Menlo.ttc";
    options.lookdevManifestPath = std::string(BILLIARDGL_SOURCE_ROOT) +
        "/assets/modern/lookdev/focused_practice_hall_v1.lookdev.json";
    const std::string phase2 = assetBinding.root.string() + "/";
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

}  // namespace

int main(int argc, const char* const* argv)
{
    @autoreleasepool {
        try {
            expect(argc == 4 || argc == 6,
                "usage: phase3_gpu_capture_runner plan.tsv output-dir results.jsonl [warmup sample]");
            const int warmupFrames = argc == 6 ? std::stoi(argv[4]) : 4;
            const int sampleFrames = argc == 6 ? std::stoi(argv[5]) : 12;
            expect(warmupFrames > 0 && sampleFrames > 0,
                "phase3_performance_frame_count_invalid");
            const std::filesystem::path outputDirectory(argv[2]);
            std::filesystem::create_directories(outputDirectory);
            std::ifstream plan(argv[1]);
            expect(static_cast<bool>(plan), "phase3_plan_read_failed");
            std::vector<CaptureCase> captures;
            std::string line;
            while (std::getline(plan, line))
                if (!line.empty()) captures.push_back(parseCase(line));
            expect(!captures.empty(), "phase3_plan_empty");

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
            expect(format != nil, "phase3_core41_pixel_format_unavailable");
            NSOpenGLContext* context = [[NSOpenGLContext alloc]
                initWithFormat:format shareContext:nil];
            expect(context != nil, "phase3_core41_context_failed");
            NSWindow* window = [[NSWindow alloc] initWithContentRect:
                NSMakeRect(0, 0, 1920, 1080)
                styleMask:NSWindowStyleMaskBorderless
                backing:NSBackingStoreBuffered defer:NO];
            [window setReleasedWhenClosed:NO];
            [context setView:[window contentView]];
            [context makeCurrentContext];
            [context update];
            const auto glText = [](GLenum name) {
                const GLubyte* value = glGetString(name);
                return value == nullptr ? std::string("unavailable") :
                    std::string(reinterpret_cast<const char*>(value));
            };
            const std::string glVendor = glText(GL_VENDOR);
            const std::string glDevice = glText(GL_RENDERER);
            const std::string glVersion = glText(GL_VERSION);

            const AssetBinding assetBinding = captureAssetBinding();
            ModernRenderer renderer(rendererOptions(assetBinding));
            std::string error;
            expect(renderer.initialize({captures.front().physicalWidth,
                captures.front().physicalHeight, true}, error), error);
            expect(renderer.pbrUpload().loadedPackage &&
                renderer.gltfSceneLoaded() && renderer.venueSceneLoaded(),
                "phase3_complete_scene_assets_not_loaded");

            ui::FontAtlasConfig atlasConfig;
            atlasConfig.fontPath = "/System/Library/Fonts/Menlo.ttc";
            atlasConfig.nominalPixelSize = 32;
            atlasConfig.atlasWidth = 1024;
            atlasConfig.atlasHeight = 1024;
            ui::SdfFontAtlas atlas;
            expect(atlas.initialize(atlasConfig).ok,
                "phase3_audit_atlas_failed");
            const auto manifest = product_hud::loadProductHudManifest(
                std::string(BILLIARDGL_SOURCE_ROOT) +
                "/assets/modern/hud/focused_practice_hud_v1.hud.json");
            const auto registry = ui::mergeHudShippingTokenSets({
                ui::loadHudShippingTokenSet(std::string(BILLIARDGL_SOURCE_ROOT) +
                    "/assets/modern/hud/focused_practice_hud_shipping_tokens_v1.json"),
                ui::loadShellShippingTokenSet(std::string(BILLIARDGL_SOURCE_ROOT) +
                    "/assets/modern/hud/focused_practice_shell_shipping_tokens_v1.json")});
            expect(manifest.ok && registry.ok,
                "phase3_audit_assets_failed");
            ui::ProductUiFrameCoordinator coordinator(manifest, atlas, registry);
            const ReplayFixture replay = makeReplay();
            std::ofstream results(argv[3], std::ios::binary | std::ios::trunc);
            expect(static_cast<bool>(results), "phase3_results_open_failed");

            int previousWidth = captures.front().physicalWidth;
            int previousHeight = captures.front().physicalHeight;
            std::map<std::string, PerformanceMetrics> performanceBatches;
            for (std::size_t index = 0; index < captures.size(); ++index) {
                const CaptureCase& capture = captures[index];
                if (capture.physicalWidth != previousWidth ||
                        capture.physicalHeight != previousHeight) {
                    renderer.resize(capture.physicalWidth,
                        capture.physicalHeight);
                    previousWidth = capture.physicalWidth;
                    previousHeight = capture.physicalHeight;
                }
                SessionFixture fixture(capture, static_cast<int>(index), replay);
                const ModernProductSessionSnapshot session =
                    fixture.session().snapshot();
                auditPublicState(capture, session);
                const PresentationSnapshot presentation = presentationFor(
                    capture, fixture.runtime());
                const ui::ProductUiFrameEnvironment environment{
                    true, false, false};
                const std::string performanceBatchId = capture.surface + "__" +
                    capture.viewport;
                if (performanceBatches.find(performanceBatchId) ==
                        performanceBatches.end()) {
                    performanceBatches.emplace(performanceBatchId,
                        benchmark(renderer, capture, presentation, session,
                            environment, warmupFrames, sampleFrames));
                }
                // Prime the independent audit atlas before the measured build;
                // the shipping frame contract forbids on-frame glyph rasterization.
                const ui::ProductUiFrame primed = coordinator.build(
                    ui::makeProductUiFrameRequest(session, presentation,
                        environment));
                expect(primed.ok, "phase3_audit_prime_failed:" + capture.id);
                const int rasterizationsBefore = atlas.rasterizationCount();
                const ui::ProductUiFrame frame = coordinator.build(
                    ui::makeProductUiFrameRequest(session, presentation,
                        environment));
                const int rasterizationsAfter = atlas.rasterizationCount();
                expect(frame.ok, "phase3_audit_frame_failed:" +
                    capture.id + ":" + frame.errorCode);
                const auto started = std::chrono::steady_clock::now();
                renderer.renderProductFrame(presentation, session, environment);
                glFinish();
                const auto finished = std::chrono::steady_clock::now();
                const ModernProductUiRuntimeResult gpu =
                    renderer.latestProductUiResult();
                expect(gpu.ok && gpu.gpuSubmitted &&
                    gpu.canonicalSha256 == frame.evidenceSha256 &&
                    gpu.gpuMetrics.drawCalls > 0,
                    "phase3_gpu_submit_mismatch:" + capture.id);
                const std::filesystem::path ppm = outputDirectory /
                    (capture.id + ".ppm");
                expect(renderer.capture({ppm.string(), capture.physicalWidth,
                    capture.physicalHeight}, error), error);
                const MachineMetrics machine = measure(frame, session,
                    rasterizationsBefore, rasterizationsAfter);
                results << json::stringify(resultJson(capture, ppm.string(),
                    machine, gpu, renderer.metrics(),
                    std::chrono::duration<double, std::milli>(
                        finished - started).count(),
                    performanceBatches.at(performanceBatchId), glVendor,
                    glDevice, glVersion, frame, assetBinding)) << '\n';
                results.flush();
                expect(static_cast<bool>(results),
                    "phase3_results_write_failed");
                std::cout << "phase3-micro-capture " << index + 1 << '/'
                    << captures.size() << ' ' << capture.id << '\n' << std::flush;
            }
            renderer.shutdown();
            [NSOpenGLContext clearCurrentContext];
            std::cout << "phase3-gpu-capture-runner: EVIDENCE_CAPTURED captures="
                << captures.size() << '\n';
            return EXIT_SUCCESS;
        } catch (const std::exception& error) {
            std::cerr << "phase3-gpu-capture-runner: " << error.what() << '\n';
            return EXIT_FAILURE;
        }
    }
}
