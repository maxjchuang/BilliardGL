#import <Cocoa/Cocoa.h>
#import <OpenGL/gl3.h>

#include "ai/ai_player_controller.h"
#include "automation_json.h"
#include "deterministic_hash.h"
#include "product_hud.h"
#include "product_settings.h"
#include "render_state.h"
#include "render/modern/modern_product_session.h"
#include "render/modern/modern_renderer.h"
#include "render/ui/product_ui_frame_coordinator.h"
#include "render/ui/sdf_font_atlas.h"
#include "training/trajectory_overlay_renderer.h"

#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <limits>
#include <memory>
#include <map>
#include <numeric>
#include <set>
#include <stdexcept>
#include <string>
#include <vector>

namespace {
using namespace billiardgl;
using namespace billiardgl::render;
namespace ui = billiardgl::render::ui;

void require(bool value, const std::string& error)
{
    if (!value) throw std::runtime_error(error);
}

std::vector<std::string> split(const std::string& line)
{
    std::vector<std::string> fields;
    std::size_t begin = 0;
    while (begin <= line.size()) {
        const std::size_t end = line.find('\t', begin);
        fields.push_back(line.substr(begin, end == std::string::npos ?
            std::string::npos : end - begin));
        if (end == std::string::npos) break;
        begin = end + 1;
    }
    return fields;
}

std::string fileSha256(const std::string& path)
{
    std::ifstream input(path, std::ios::binary);
    require(static_cast<bool>(input), "phase4_overlay_binding_read_failed:" + path);
    const std::string bytes((std::istreambuf_iterator<char>(input)),
        std::istreambuf_iterator<char>());
    return sha256Hex(bytes);
}

struct CaptureCase {
    std::string id;
    std::string scenarioId;
    std::string scenarioSha256;
    std::string replayPath;
    std::string replaySha256;
    std::string executableVersion;
    std::string contentSha256;
    std::string camera;
    int logicalWidth = 1280;
    int logicalHeight = 720;
    int physicalWidth = 1920;
    int physicalHeight = 1080;
    float dpi = 96.0f;
    double deviceScale = 1.0;
    std::string palette;
    double uiScale = 1.0;
    bool advanced = false;
    std::string overlayState;
    std::string terminalOutcome;
};

CaptureCase parseCase(const std::string& line)
{
    const auto fields = split(line);
    require(fields.size() == 19, "phase4_overlay_plan_field_count");
    CaptureCase result;
    result.id = fields[0]; result.scenarioId = fields[1];
    result.scenarioSha256 = fields[2]; result.replayPath = fields[3];
    result.replaySha256 = fields[4]; result.executableVersion = fields[5];
    result.contentSha256 = fields[6]; result.camera = fields[7];
    result.logicalWidth = std::stoi(fields[8]);
    result.logicalHeight = std::stoi(fields[9]);
    result.physicalWidth = std::stoi(fields[10]);
    result.physicalHeight = std::stoi(fields[11]);
    result.dpi = std::stof(fields[12]);
    result.deviceScale = std::stod(fields[13]);
    result.palette = fields[14]; result.uiScale = std::stod(fields[15]);
    result.advanced = fields[16] == "1";
    result.overlayState = fields[17];
    result.terminalOutcome = fields[18];
    require(result.overlayState == "predicted_only_active_collapsed" ||
        result.overlayState == "comparison_ready" ||
        result.overlayState == "comparison_ready_collapsed" ||
        result.overlayState == "comparison_terminal_collapsed" ||
        result.overlayState == "actual_replay_paused_collapsed" ||
        result.overlayState ==
            "comparison_terminal_explicit_advanced_expanded",
        "phase4_overlay_state_invalid:" + result.id);
    require((result.advanced && result.overlayState ==
            "comparison_terminal_explicit_advanced_expanded") ||
        (!result.advanced && result.overlayState !=
            "comparison_terminal_explicit_advanced_expanded"),
        "phase4_overlay_advanced_state_mismatch:" + result.id);
    require(result.terminalOutcome == "success" ||
        result.terminalOutcome == "failure" ||
        result.terminalOutcome == "not_terminal",
        "phase4_overlay_terminal_outcome_invalid:" + result.id);
    return result;
}

std::set<std::string> trainingCapabilities()
{
    return {"training_scenario_v1",
        "authoritative_horizontal_tip_offset_v1",
        "authoritative_longitudinal_tip_offset_v1"};
}

camera::Preset preset(const std::string& id)
{
    if (id == "player_low") return camera::Preset::PlayerLow;
    if (id == "elevated") return camera::Preset::Elevated;
    if (id == "overhead") return camera::Preset::Overhead;
    if (id == "broadcast") return camera::Preset::Broadcast;
    throw std::runtime_error("phase4_overlay_camera_invalid:" + id);
}

training::AttemptFacts successfulFacts(const training::Scenario& scenario)
{
    training::AttemptFacts facts;
    GameRuntime isolated;
    require(applyPhysicsScenario(isolated, scenario.initialScenario).ok,
        "phase4_overlay_success_fixture_apply_failed:" + scenario.id);
    facts.state = isolated.state();
    facts.shotCount = scenario.scoring.at("par_shots").asInt();
    facts.traceSha256 = std::string(64, 'a');
    for (const json::Value& condition : scenario.success.asArray()) {
        const std::string name = condition.at("predicate").asString();
        if (name == "ball_pocketed")
            facts.state.balls[condition.at("ball_index").asInt()].pocketed = true;
        else if (name == "ball_not_pocketed")
            facts.state.balls[condition.at("ball_index").asInt()].pocketed = false;
        else if (name == "ball_final_position_in_zone") {
            const int index = condition.at("ball_index").asInt();
            facts.state.balls[index].position.x = static_cast<float>(
                condition.at("center_cm").asArray()[0].asNumber());
            facts.state.balls[index].position.z = static_cast<float>(
                condition.at("center_cm").asArray()[1].asNumber());
        } else if (name == "first_contact_ball") {
            facts.firstContactBall = condition.at("ball_index").asInt();
        } else if (name == "rail_contact_count_gte") {
            facts.railContacts[condition.at("ball_index").asInt()] =
                condition.at("minimum").asInt();
        } else if (name == "distinct_object_balls_with_rail_contact_gte") {
            facts.distinctObjectBallsWithRailContact =
                condition.at("minimum").asInt();
        } else if (name == "ordered_ball_pockets") {
            for (const json::Value& value :
                    condition.at("ball_indices").asArray())
                facts.pocketOrder.push_back(value.asInt());
        } else if (name == "shot_count_lte") {
            facts.shotCount = std::min(facts.shotCount,
                condition.at("maximum").asInt());
        }
    }
    return facts;
}

PresentationSnapshot presentationFor(const CaptureCase& capture,
    GameRuntime& runtime)
{
    PresentationSnapshot seed = buildPresentationSnapshot(
        runtime.state(), runtime.state(), 0.0);
    camera::StateMachine camera;
    const camera::Preset selected = preset(capture.camera);
    const camera::SemanticState semantic =
        selected == camera::Preset::PlayerLow ? camera::SemanticState::AimPlayer :
        selected == camera::Preset::Overhead ? camera::SemanticState::AimOverhead :
        selected == camera::Preset::Broadcast ?
            camera::SemanticState::ReplayBroadcast :
            camera::SemanticState::ObserveFree;
    camera.reset(semantic, selected, seed);
    PresentationSnapshot result = buildPresentationSnapshot(runtime.state(),
        runtime.state(), 0.0, runtime.shotControl(),
        runtime.shotControlCapabilities(), camera.frame(),
        buildRulesPresentation(runtime));
    result.camera.viewportWidth = capture.physicalWidth;
    result.camera.viewportHeight = capture.physicalHeight;
    result.hud.viewportWidth = capture.physicalWidth;
    result.hud.viewportHeight = capture.physicalHeight;
    result.hud.aimMode = false;
    result.hud.showHelp = false;
    return result;
}

class SessionFixture {
public:
    SessionFixture(const CaptureCase& capture, int serial)
    {
        require(fileSha256(capture.replayPath) == capture.replaySha256,
            "phase4_overlay_replay_sha_mismatch:" + capture.id);
        settingsPath_ = std::filesystem::temp_directory_path() /
            ("billiardgl-phase4-overlay-settings-" +
                std::to_string(serial) + ".json");
        settings::ProductSettings settings = settings::ProductSettings::defaults();
        settings.uiScale = capture.uiScale;
        settings.colorVisionMode = capture.palette == "standard" ?
            settings::ColorVisionMode::Default :
            settings::ColorVisionMode::Deuteranopia;
        require(settings::saveFileAtomic(settingsPath_.string(), settings).ok,
            "phase4_overlay_settings_write_failed");
        ModernProductSessionOptions options;
        options.settingsPath = settingsPath_.string();
        options.trainingManifestPath = std::string(BILLIARDGL_SOURCE_ROOT) +
            "/assets/training/phase4/training_pack_v1.json";
        options.executableVersion = capture.executableVersion;
        session_ = std::make_unique<ModernProductSession>(
            runtime_, ai_, std::move(options));
        session_->setShellViewport({capture.logicalWidth, capture.logicalHeight,
            capture.physicalWidth, capture.physicalHeight, capture.dpi});
        ProductShellController& shell = session_->shell();
        std::string before;
        std::uint64_t beforeTick = 0;
        if (capture.overlayState == "actual_replay_paused_collapsed") {
            require(capture.terminalOutcome == "not_terminal",
                "phase4_overlay_actual_state_terminal_outcome:" + capture.id);
            before = gameStateSha256(runtime_.state());
            beforeTick = runtime_.tick();
            require(shell.loadReplayPath(capture.replayPath,
                capture.executableVersion, capture.contentSha256).ok,
                "phase4_overlay_actual_replay_load_failed:" + capture.id);
            for (int step = 0;
                 step < 10000 && !session_->snapshot().shell.replay.finished;
                 ++step) {
                require(shell.dispatchProductInput(
                    {"keyboard", ".", "press", 0.0, 0.0}).ok,
                    "phase4_overlay_actual_replay_step_failed:" + capture.id);
            }
        } else {
            require(shell.startTrainingScenario(capture.scenarioId,
                capture.executableVersion, 77).ok,
                "phase4_overlay_training_start_failed:" + capture.id);
            require(session_->refreshTrainingPreview().ok,
                "phase4_overlay_preview_failed:" + capture.id);
            before = gameStateSha256(runtime_.state());
            beforeTick = runtime_.tick();
            if (capture.overlayState != "predicted_only_active_collapsed") {
                training::AttemptFacts facts;
                if (capture.terminalOutcome == "success") {
                    const training::Scenario* scenario =
                        session_->training().currentScenario();
                    require(scenario != nullptr,
                        "phase4_overlay_success_scenario_missing:" + capture.id);
                    facts = successfulFacts(*scenario);
                } else {
                    require(capture.terminalOutcome == "failure",
                        "phase4_overlay_comparison_outcome_missing:" + capture.id);
                    facts.state = runtime_.state();
                    facts.numericalFailure = true;
                }
                require(session_->training().finishAttempt(facts).ok,
                    "phase4_overlay_terminal_fixture_failed:" + capture.id);
                require(shell.loadTrainingComparisonReplayPath(capture.replayPath,
                    capture.executableVersion, capture.contentSha256).ok,
                    "phase4_overlay_comparison_load_failed:" + capture.id);
            } else {
                require(capture.terminalOutcome == "not_terminal",
                    "phase4_overlay_predicted_state_terminal_outcome:" +
                        capture.id);
            }
        }
        require(before == gameStateSha256(runtime_.state()) &&
            beforeTick == runtime_.tick(),
            "phase4_overlay_comparison_mutated_authority:" + capture.id);
        if (capture.advanced) {
            require(shell.focusItem("training_advanced").ok &&
                shell.navigate(ShellNavigationCommand::Activate).ok,
                "phase4_overlay_advanced_open_failed:" + capture.id);
        }
        const auto snapshot = session_->snapshot();
        const bool predictedOnly = capture.overlayState ==
            "predicted_only_active_collapsed";
        const bool actualOnly = capture.overlayState ==
            "actual_replay_paused_collapsed";
        if (actualOnly) {
            require(snapshot.shell.screen == ProductScreen::ReplayPlayback &&
                snapshot.shell.replay.paused && snapshot.shell.replay.finished &&
                snapshot.shell.trajectoryOverlay.predicted.samples.empty() &&
                snapshot.shell.trajectoryOverlay.actual.samples.size() > 1 &&
                !snapshot.shell.trajectoryOverlay.advancedExpanded,
                "phase4_overlay_actual_state_incomplete:" + capture.id);
        } else if (predictedOnly) {
            require(snapshot.shell.screen == ProductScreen::TrainingSession &&
                snapshot.shell.training.previewReady &&
                !snapshot.shell.training.replayReady &&
                snapshot.shell.trajectoryOverlay.predicted.samples.size() > 1 &&
                snapshot.shell.trajectoryOverlay.actual.samples.empty() &&
                !snapshot.shell.trajectoryOverlay.advancedExpanded,
                "phase4_overlay_predicted_state_incomplete:" + capture.id);
        } else {
            require(snapshot.shell.screen == ProductScreen::TrainingSession &&
                snapshot.shell.training.previewReady &&
                snapshot.shell.training.replayReady &&
                snapshot.shell.trajectoryOverlay.predicted.samples.size() > 1 &&
                snapshot.shell.trajectoryOverlay.actual.samples.size() > 1 &&
                snapshot.shell.trajectoryOverlay.advancedExpanded ==
                    capture.advanced &&
                snapshot.shell.training.evaluation.succeeded ==
                    (capture.terminalOutcome == "success") &&
                snapshot.shell.training.evaluation.failed ==
                    (capture.terminalOutcome == "failure") &&
                (!snapshot.shell.training.evaluation.failed ||
                    !snapshot.shell.training.evaluation.reason.empty()),
                "phase4_overlay_comparison_state_incomplete:" + capture.id);
        }
        authorityHash_ = gameStateSha256(runtime_.state());
        authorityTick_ = runtime_.tick();
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
    bool authorityUnchanged() const
    {
        return authorityHash_ == gameStateSha256(runtime_.state()) &&
            authorityTick_ == runtime_.tick();
    }

private:
    std::filesystem::path settingsPath_;
    GameRuntime runtime_;
    ai::AiPlayerController ai_;
    std::unique_ptr<ModernProductSession> session_;
    std::string authorityHash_;
    std::uint64_t authorityTick_ = 0;
};

double percentile(std::vector<double> values, double fraction)
{
    std::sort(values.begin(), values.end());
    return values.at(std::min(values.size() - 1,
        static_cast<std::size_t>(std::ceil(fraction * values.size()) - 1.0)));
}

struct Performance {
    double p50 = 0.0;
    double p95 = 0.0;
    double p99 = 0.0;
    double overlayCpuP95 = 0.0;
    double uiOverlayGpuP95 = 0.0;
    std::map<std::string, double> passCpuP95;
    std::map<std::string, double> passGpuP95;
};

double uiOverlayGpuMs(const ModernFrameMetrics& metrics)
{
    double total = 0.0;
    for (const auto& pass : metrics.passes)
        if (pass.name == "overlay" || pass.name == "product_ui")
            total += pass.gpuMilliseconds;
    return total;
}

Performance benchmark(ModernRenderer& renderer,
    const PresentationSnapshot& presentation,
    const ModernProductSessionSnapshot& session,
    const ui::ProductUiFrameEnvironment& environment,
    int warmup, int samples)
{
    for (int frame = 0; frame < warmup; ++frame) {
        renderer.renderProductFrame(presentation, session, environment);
        glFinish();
    }
    std::vector<double> total, overlayCpu, uiOverlayGpu;
    std::map<std::string, std::vector<double>> passCpu;
    std::map<std::string, std::vector<double>> passGpu;
    for (int frame = 0; frame < samples; ++frame) {
        const auto begin = std::chrono::steady_clock::now();
        renderer.renderProductFrame(presentation, session, environment);
        glFinish();
        total.push_back(std::chrono::duration<double, std::milli>(
            std::chrono::steady_clock::now() - begin).count());
        overlayCpu.push_back(
            renderer.metrics().trajectoryOverlayCpuBuildMilliseconds);
        uiOverlayGpu.push_back(uiOverlayGpuMs(renderer.metrics()));
        for (const auto& pass : renderer.metrics().passes) {
            passCpu[pass.name].push_back(pass.cpuMilliseconds);
            if (pass.gpuTimingAvailable)
                passGpu[pass.name].push_back(pass.gpuMilliseconds);
        }
    }
    Performance result;
    for (auto& entry : passCpu) {
        result.passCpuP95[entry.first] = percentile(entry.second, 0.95);
        std::cout << "phase4-overlay-pass name=" << entry.first
            << " cpu_p95_ms=" << result.passCpuP95[entry.first];
        auto gpu = passGpu.find(entry.first);
        if (gpu != passGpu.end() && !gpu->second.empty()) {
            result.passGpuP95[entry.first] = percentile(gpu->second, 0.95);
            std::cout << " gpu_p95_ms=" << result.passGpuP95[entry.first];
        }
        std::cout << '\n';
    }
    result.p50 = percentile(total, 0.50);
    result.p95 = percentile(total, 0.95);
    result.p99 = percentile(total, 0.99);
    result.overlayCpuP95 = percentile(overlayCpu, 0.95);
    result.uiOverlayGpuP95 = percentile(uiOverlayGpu, 0.95);
    return result;
}

bool contains(const product_hud::HudRect& outer,
    const product_hud::HudRect& inner)
{
    return inner.x + 0.02f >= outer.x && inner.y + 0.02f >= outer.y &&
        inner.x + inner.width <= outer.x + outer.width + 0.02f &&
        inner.y + inner.height <= outer.y + outer.height + 0.02f;
}

struct UiAudit {
    int safe = 0;
    int truncation = 0;
    int overlap = 0;
    bool boundedInspector = false;
    bool gameplayCorridor = false;
    bool inspectorBackdropBounded = false;
    double inspectorViewportCoverage = 1.0;
    double gameplayCorridorWidthFraction = 0.0;
    bool geometryGate = false;
    double protectedIntersectionArea = 0.0;
    int debugNodes = 0;
    int unexpandedIcons = 0;
    int opaqueIcons = 0;
    int panelAlphaViolations = 0;
};

UiAudit audit(const ui::ProductUiFrame& frame,
    const ProductShellSnapshot& model, const ui::ShellViewport& viewport)
{
    UiAudit result;
    result.debugNodes = frame.draw.containsDebug ? 1 : 0;
    const ui::ShellScreenLayout layout =
        ui::buildShippingScreenLayout(model, viewport);
    result.boundedInspector = layout.ok && layout.boundedInspector;
    result.inspectorViewportCoverage = layout.inspectorViewportCoverage;
    result.gameplayCorridorWidthFraction = layout.ok ?
        layout.gameplayCorridorRect.width / viewport.physicalWidth : 0.0;
    result.gameplayCorridor = layout.ok &&
        layout.gameplayCorridorRect.width >= viewport.physicalWidth * 0.50f &&
        layout.gameplayCorridorRect.height == layout.safeRect.height;
    std::vector<product_hud::HudRect> panels;
    std::vector<product_hud::HudRect> allPanelBackgrounds;
    for (const auto& primitive : frame.draw.primitives) {
        if (primitive.kind == ui::UiPrimitiveKind::Icon) {
            ++result.unexpandedIcons;
            ++result.opaqueIcons;
        }
        if (!contains(frame.draw.safeRect, primitive.rect) ||
            !contains(frame.draw.safeRect, primitive.clipRect)) ++result.safe;
        if (primitive.kind == ui::UiPrimitiveKind::Panel &&
            primitive.token == "shell_item_panel") panels.push_back(primitive.sourceRect);
        if (primitive.kind == ui::UiPrimitiveKind::Panel ||
            primitive.kind == ui::UiPrimitiveKind::SolidRect) {
            allPanelBackgrounds.push_back(primitive.sourceRect);
            if (primitive.color.alpha < 0.78f || primitive.color.alpha > 0.86f)
                ++result.panelAlphaViolations;
        }
        if (primitive.kind == ui::UiPrimitiveKind::SolidRect &&
            primitive.token == "shipping_bounded_inspector_background")
            result.inspectorBackdropBounded = layout.ok &&
                contains(layout.inspectorRect, primitive.sourceRect) &&
                primitive.sourceRect.width < layout.safeRect.width * 0.47f;
    }
    for (const auto& text : frame.draw.textRuns)
        if (text.measuredWidth > text.clipRect.width + 0.02f) ++result.truncation;
    for (std::size_t a = 0; a < panels.size(); ++a)
        for (std::size_t b = a + 1; b < panels.size(); ++b)
            if (panels[a].x < panels[b].x + panels[b].width &&
                panels[a].x + panels[a].width > panels[b].x &&
                panels[a].y < panels[b].y + panels[b].height &&
                panels[a].y + panels[a].height > panels[b].y) ++result.overlap;
    const ui::ShellInspectorGeometryAudit geometry =
        ui::auditBoundedInspectorGeometry(layout, allPanelBackgrounds, 0.0f);
    result.geometryGate = geometry.ok;
    result.protectedIntersectionArea = geometry.protectedIntersectionArea;
    return result;
}

int textRunCount(const ui::ProductUiFrame& frame, const std::string& text)
{
    return static_cast<int>(std::count_if(frame.draw.textRuns.begin(),
        frame.draw.textRuns.end(), [&](const auto& run) {
            return run.text.find(text) != std::string::npos;
        }));
}

bool exactPhysicalStyle(const CaptureCase& capture,
    const training::TrajectoryOverlayStyle& style)
{
    const auto close = [](double first, double second) {
        return std::fabs(first - second) <= 1e-6;
    };
    if (capture.physicalWidth == 1280 && capture.physicalHeight == 720 &&
        std::fabs(capture.uiScale - 1.35) <= 1e-6)
        return close(style.lineWidthPx, 4.5) && close(style.haloWidthPx, 2.0) &&
            close(style.dashOnPx, 16.0) && close(style.dashOffPx, 10.0) &&
            close(style.endpointOuterDiameterPx, 24.0) &&
            close(style.endpointRingStrokePx, 4.5) &&
            close(style.actualTickLengthPx, 12.0) &&
            close(style.actualTickSpacingPx, 42.0) &&
            close(style.actualEndpointDotDiameterPx, 11.0);
    if (capture.physicalWidth == 1920 && capture.physicalHeight == 1080 &&
        std::fabs(capture.uiScale - 1.35) <= 1e-6 &&
        capture.deviceScale > 1.0)
        return close(style.lineWidthPx, 6.5) && close(style.haloWidthPx, 3.0) &&
            close(style.dashOnPx, 24.0) && close(style.dashOffPx, 17.0) &&
            close(style.endpointOuterDiameterPx, 36.0) &&
            close(style.endpointRingStrokePx, 6.5) &&
            close(style.actualTickLengthPx, 18.0) &&
            close(style.actualTickSpacingPx, 66.0) &&
            close(style.actualEndpointDotDiameterPx, 16.0);
    return capture.physicalWidth == 1920 && capture.physicalHeight == 1080 &&
        close(style.lineWidthPx, 4.5) && close(style.haloWidthPx, 2.0) &&
        close(style.dashOnPx, 18.0) && close(style.dashOffPx, 12.0) &&
        close(style.endpointOuterDiameterPx, 27.0) &&
        close(style.endpointRingStrokePx, 4.5) &&
        close(style.actualTickLengthPx, 13.0) &&
        close(style.actualTickSpacingPx, 48.0) &&
        close(style.actualEndpointDotDiameterPx, 12.0);
}

struct PpmImage {
    int width = 0;
    int height = 0;
    std::vector<std::uint8_t> pixels;
};

PpmImage readPpm(const std::filesystem::path& path)
{
    std::ifstream input(path, std::ios::binary);
    require(static_cast<bool>(input), "phase4_overlay_ppm_read_failed");
    std::string magic;
    int maximum = 0;
    PpmImage result;
    input >> magic >> result.width >> result.height >> maximum;
    require(magic == "P6" && result.width > 0 && result.height > 0 &&
        maximum == 255, "phase4_overlay_ppm_header_invalid");
    input.get();
    result.pixels.resize(static_cast<std::size_t>(
        result.width * result.height * 3));
    input.read(reinterpret_cast<char*>(result.pixels.data()),
        static_cast<std::streamsize>(result.pixels.size()));
    require(input.gcount() == static_cast<std::streamsize>(result.pixels.size()),
        "phase4_overlay_ppm_pixels_incomplete");
    return result;
}

std::array<std::uint8_t, 3> ppmPixel(
    const PpmImage& image, int x, int y)
{
    require(x >= 0 && y >= 0 && x < image.width && y < image.height,
        "phase4_overlay_ppm_sample_out_of_bounds");
    const std::size_t offset = static_cast<std::size_t>(
        (y * image.width + x) * 3);
    return {{image.pixels[offset], image.pixels[offset + 1],
        image.pixels[offset + 2]}};
}

std::array<std::uint8_t, 3> ppmNeighborhood(
    const PpmImage& image, int x, int y)
{
    std::array<int, 3> sum{{0, 0, 0}};
    int count = 0;
    for (int dy = -2; dy <= 2; ++dy) for (int dx = -2; dx <= 2; ++dx) {
        const int sampleX = x + dx;
        const int sampleY = y + dy;
        if (sampleX < 0 || sampleY < 0 || sampleX >= image.width ||
            sampleY >= image.height) continue;
        const auto pixel = ppmPixel(image, sampleX, sampleY);
        for (int channel = 0; channel < 3; ++channel)
            sum[channel] += pixel[channel];
        ++count;
    }
    require(count > 0, "phase4_overlay_ppm_neighborhood_empty");
    return {{static_cast<std::uint8_t>(sum[0] / count),
        static_cast<std::uint8_t>(sum[1] / count),
        static_cast<std::uint8_t>(sum[2] / count)}};
}

struct NearestRolePixel {
    std::array<std::uint8_t, 3> rgb{{0, 0, 0}};
    int squaredDistance = std::numeric_limits<int>::max();
};

NearestRolePixel nearestRolePixel(const PpmImage& image, int x, int y,
    const std::array<std::uint8_t, 3>& target, int radius)
{
    NearestRolePixel result;
    for (int dy = -radius; dy <= radius; ++dy)
        for (int dx = -radius; dx <= radius; ++dx) {
            const int sampleX = x + dx;
            const int sampleY = y + dy;
            if (sampleX < 0 || sampleY < 0 || sampleX >= image.width ||
                sampleY >= image.height) continue;
            const auto rgb = ppmPixel(image, sampleX, sampleY);
            int distance = 0;
            for (int channel = 0; channel < 3; ++channel) {
                const int delta = static_cast<int>(rgb[channel]) -
                    static_cast<int>(target[channel]);
                distance += delta * delta;
            }
            if (distance < result.squaredDistance) {
                result.rgb = rgb;
                result.squaredDistance = distance;
            }
        }
    return result;
}

training::TrajectoryContrastAudit contrastAudit(
    const PpmImage& image,
    const training::TrajectoryOverlayDrawPackage& package,
    training::TrajectoryOverlayPalette palette,
    bool predictedRequired, bool actualRequired)
{
    std::vector<training::TrajectoryContrastSample> samples;
    std::array<int, 3> predictedSum{{0, 0, 0}};
    std::array<int, 3> actualSum{{0, 0, 0}};
    int predictedCount = 0;
    int actualCount = 0;
    std::size_t sourceProbeCount = 0;
    std::size_t hiddenProbeCount = 0;
    const std::array<std::uint8_t, 3> predictedTarget{{255, 254, 115}};
    const std::array<std::uint8_t, 3> actualTarget{{1, 22, 32}};
    const std::array<std::uint8_t, 3> predictedHaloTarget{{0, 0, 0}};
    const std::array<std::uint8_t, 3> actualHaloTarget{{255, 255, 255}};
    for (const auto& probe : package.contrastProbes) {
        ++sourceProbeCount;
        if (probe.strokeXPhysicalPx < 0 || probe.strokeYPhysicalPx < 0 ||
            probe.strokeXPhysicalPx >= image.width ||
            probe.strokeYPhysicalPx >= image.height ||
            probe.backgroundXPhysicalPx < 0 ||
            probe.backgroundYPhysicalPx < 0 ||
            probe.backgroundXPhysicalPx >= image.width ||
            probe.backgroundYPhysicalPx >= image.height) continue;
        const bool predicted =
            probe.track == training::TrajectoryContrastTrack::Predicted;
        const auto strokeMatch = nearestRolePixel(image,
            probe.strokeXPhysicalPx, probe.strokeYPhysicalPx,
            predicted ? predictedTarget : actualTarget, 2);
        const auto haloMatch = nearestRolePixel(image,
            probe.haloXPhysicalPx, probe.haloYPhysicalPx,
            predicted ? predictedHaloTarget : actualHaloTarget, 1);
        const bool visible = strokeMatch.squaredDistance <= 5000 &&
            haloMatch.squaredDistance <= (predicted ? 5000 : 12000);
        if (!visible) {
            ++hiddenProbeCount;
            continue;
        }
        const auto stroke = strokeMatch.rgb;
        samples.push_back({probe.track, probe.strokeXPhysicalPx,
            probe.strokeYPhysicalPx, stroke, ppmNeighborhood(image,
                probe.backgroundXPhysicalPx, probe.backgroundYPhysicalPx),
                haloMatch.rgb, true});
        auto& sum = predicted ?
            predictedSum : actualSum;
        int& count = predicted ?
            predictedCount : actualCount;
        for (int channel = 0; channel < 3; ++channel) sum[channel] += stroke[channel];
        ++count;
    }
    const auto average = [](const std::array<int, 3>& sum, int count,
            std::array<std::uint8_t, 3> fallback) {
        if (count == 0) return fallback;
        return std::array<std::uint8_t, 3>{{
            static_cast<std::uint8_t>(sum[0] / count),
            static_cast<std::uint8_t>(sum[1] / count),
            static_cast<std::uint8_t>(sum[2] / count)}};
    };
    const std::array<std::uint8_t, 3> predictedFallback = predictedTarget;
    const std::array<std::uint8_t, 3> actualFallback = actualTarget;
    const auto linear = [](double value) {
        value /= 255.0;
        return value <= 0.04045 ? value / 12.92 :
            std::pow((value + 0.055) / 1.055, 2.4);
    };
    const auto luminance = [&](const auto& rgb) {
        return 0.2126 * linear(rgb[0]) + 0.7152 * linear(rgb[1]) +
            0.0722 * linear(rgb[2]);
    };
    const auto ratio = [](double a, double b) {
        return (std::max(a, b) + 0.05) / (std::min(a, b) + 0.05);
    };
    for (const auto& sample : samples) {
        const double stroke = luminance(sample.strokeSrgb8);
        const double halo = luminance(sample.haloSrgb8);
        const double background = luminance(sample.localBackgroundSrgb8);
        const double resolved = std::min(ratio(stroke, halo),
            std::max(ratio(stroke, background), ratio(halo, background)));
        if (resolved >= 4.5) continue;
        std::cerr << "phase4-overlay-contrast-probe track=" <<
            (sample.track == training::TrajectoryContrastTrack::Predicted ?
                "predicted" : "actual")
            << " coordinate=" << sample.xPhysicalPx << ',' << sample.yPhysicalPx
            << " stroke=" << static_cast<int>(sample.strokeSrgb8[0]) << ','
            << static_cast<int>(sample.strokeSrgb8[1]) << ','
            << static_cast<int>(sample.strokeSrgb8[2])
            << " halo=" << static_cast<int>(sample.haloSrgb8[0]) << ','
            << static_cast<int>(sample.haloSrgb8[1]) << ','
            << static_cast<int>(sample.haloSrgb8[2])
            << " background=" <<
                static_cast<int>(sample.localBackgroundSrgb8[0]) << ','
            << static_cast<int>(sample.localBackgroundSrgb8[1]) << ','
            << static_cast<int>(sample.localBackgroundSrgb8[2])
            << " ratio=" << resolved << '\n';
    }
    training::TrajectoryContrastAudit result =
        training::auditTrajectoryContrastSamples(samples,
        average(predictedSum, predictedCount, predictedFallback),
        average(actualSum, actualCount, actualFallback), palette,
        predictedRequired, actualRequired);
    result.sourceProbeCount = sourceProbeCount;
    result.visibleProbeCount = samples.size();
    result.hiddenProbeCount = hiddenProbeCount +
        (sourceProbeCount - hiddenProbeCount - samples.size());
    result.visibleProbeCoverage = sourceProbeCount == 0 ? 0.0 :
        static_cast<double>(samples.size()) / sourceProbeCount;
    if (result.ok && result.visibleProbeCoverage < 0.90) {
        result.ok = false;
        result.errorCode = "trajectory_visible_probe_coverage_below_90_percent";
    }
    return result;
}

bool longReplayFailsClosed(const CameraPresentation& camera)
{
    training::TrajectoryOverlaySnapshot snapshot;
    snapshot.actual.visible = true;
    snapshot.actual.endpointVisible = true;
    for (std::uint64_t tick = 0; tick < 1200; ++tick) {
        const float x = tick % 2 == 0 ? -60.0f : 60.0f;
        const float z = -110.0f + static_cast<float>(tick % 100) * 2.2f;
        snapshot.actual.samples.push_back({tick + 1, {x, 86.5f, z}, true});
    }
    snapshot.actual.endpoint = snapshot.actual.samples.back().position;
    const auto package = training::buildTrajectoryOverlayDrawPackage(snapshot,
        camera, {1920, 1080, 1.0, 1.0});
    return !package.ok &&
        package.errorCode == "trajectory_overlay_actual_vertex_budget";
}

ModernRendererOptions rendererOptions()
{
    ModernRendererOptions options;
    options.msaaSamples = 0; options.quality = RenderQuality::Medium;
    // Measure the production non-blocking query-ring path.  The synchronous
    // option waits after every render pass and turns query readback latency
    // into frame time; it is intentionally documented as test-only.
    options.synchronousGpuTiming = false; options.enableProductUi = true;
    options.productUiSourceRoot = BILLIARDGL_SOURCE_ROOT;
    options.productUiFontPath = "/System/Library/Fonts/Menlo.ttc";
    options.lookdevManifestPath = std::string(BILLIARDGL_SOURCE_ROOT) +
        "/assets/modern/lookdev/focused_practice_hall_v1.lookdev.json";
    const std::string phase2 = std::string(BILLIARDGL_SOURCE_ROOT) +
        "/assets/modern/phase2/";
    options.pbrParametersPath = phase2 + "focused_practice_hall_pbr_v1.params.json";
    options.pbrAssetManifestPath = phase2 + "focused_practice_hall_modular_v1.asset.json";
    options.lightingRigPath = phase2 + "focused_practice_hall_lighting_rig_v1.json";
    options.gltfScenePath = phase2 + "focused_practice_hall_modular_v1.gltf";
    options.venueScenePath = phase2 + "focused_practice_hall_venue_v1.gltf";
    return options;
}

}  // namespace

int main(int argc, const char* const* argv)
{
    @autoreleasepool {
        try {
            require(argc == 6,
                "usage: phase4_overlay_gpu_capture_runner plan.tsv output-dir results.jsonl warmup samples");
            const int warmup = std::stoi(argv[4]);
            const int samples = std::stoi(argv[5]);
            require(warmup >= 120 && samples >= 300,
                "phase4_overlay_formal_performance_counts_required");
            std::ifstream plan(argv[1]);
            require(static_cast<bool>(plan), "phase4_overlay_plan_read_failed");
            std::vector<CaptureCase> captures;
            std::string line;
            while (std::getline(plan, line))
                if (!line.empty()) captures.push_back(parseCase(line));
            require(captures.size() == 7 || captures.size() == 68,
                "phase4_overlay_plan_requires_exact_7_or_68");
            std::set<std::string> captureIds;
            for (const CaptureCase& capture : captures)
                require(captureIds.insert(capture.id).second,
                    "phase4_overlay_capture_id_duplicate:" + capture.id);
            const bool formal = captures.size() == 68;
            const char* machineStatus = formal ?
                "GPU_MACHINE_PASS_PENDING_TA" : "PASS_PREFLIGHT_ONLY";
            std::filesystem::create_directories(argv[2]);

            [NSApplication sharedApplication];
            const NSOpenGLPixelFormatAttribute attributes[] = {
                NSOpenGLPFAOpenGLProfile,
                static_cast<NSOpenGLPixelFormatAttribute>(NSOpenGLProfileVersion4_1Core),
                NSOpenGLPFAAccelerated, NSOpenGLPFADoubleBuffer,
                NSOpenGLPFAColorSize, 24, NSOpenGLPFADepthSize, 24,
                static_cast<NSOpenGLPixelFormatAttribute>(0)};
            NSOpenGLPixelFormat* format = [[NSOpenGLPixelFormat alloc]
                initWithAttributes:attributes];
            require(format != nil, "phase4_overlay_core41_unavailable");
            NSOpenGLContext* context = [[NSOpenGLContext alloc]
                initWithFormat:format shareContext:nil];
            require(context != nil, "phase4_overlay_context_failed");
            NSWindow* window = [[NSWindow alloc] initWithContentRect:
                NSMakeRect(0, 0, 1920, 1080) styleMask:NSWindowStyleMaskBorderless
                backing:NSBackingStoreBuffered defer:NO];
            [window setReleasedWhenClosed:NO];
            [context setView:[window contentView]];
            [context makeCurrentContext]; [context update];

            ModernRenderer renderer(rendererOptions());
            std::string error;
            require(renderer.initialize({captures.front().physicalWidth,
                captures.front().physicalHeight, true}, error), error);
            require(renderer.pbrUpload().loadedPackage &&
                renderer.gltfSceneLoaded() && renderer.venueSceneLoaded(),
                "phase4_overlay_production_scene_unavailable");

            ui::FontAtlasConfig font;
            font.fontPath = "/System/Library/Fonts/Menlo.ttc";
            font.nominalPixelSize = 32; font.atlasWidth = font.atlasHeight = 1024;
            ui::SdfFontAtlas atlas;
            require(atlas.initialize(font).ok, "phase4_overlay_audit_font_failed");
            const auto hud = product_hud::loadProductHudManifest(
                std::string(BILLIARDGL_SOURCE_ROOT) +
                "/assets/modern/hud/focused_practice_hud_v1.hud.json");
            const auto tokens = ui::mergeHudShippingTokenSets({
                ui::loadHudShippingTokenSet(std::string(BILLIARDGL_SOURCE_ROOT) +
                    "/assets/modern/hud/focused_practice_hud_shipping_tokens_v1.json"),
                ui::loadShellShippingTokenSet(std::string(BILLIARDGL_SOURCE_ROOT) +
                    "/assets/modern/hud/focused_practice_shell_shipping_tokens_v1.json")});
            require(hud.ok && tokens.ok, "phase4_overlay_audit_assets_failed");
            ui::ProductUiFrameCoordinator coordinator(hud, atlas, tokens);
            std::ofstream results(argv[3], std::ios::binary | std::ios::trunc);
            require(static_cast<bool>(results), "phase4_overlay_results_open_failed");

            int width = captures.front().physicalWidth;
            int height = captures.front().physicalHeight;
            Performance performance;
            bool benchmarked = false;
            for (std::size_t index = 0; index < captures.size(); ++index) {
                const CaptureCase& capture = captures[index];
                if (capture.physicalWidth != width || capture.physicalHeight != height) {
                    renderer.resize(capture.physicalWidth, capture.physicalHeight);
                    width = capture.physicalWidth; height = capture.physicalHeight;
                }
                SessionFixture fixture(capture, static_cast<int>(index));
                const ModernProductSessionSnapshot session = fixture.session().snapshot();
                const PresentationSnapshot presentation = presentationFor(capture,
                    fixture.runtime());
                const ui::ProductUiFrameEnvironment environment{true, false, false};
                const ui::ProductUiFrameRequest frameRequest =
                    ui::makeProductUiFrameRequest(session, presentation, environment);
                const ui::ProductUiFrame frame = coordinator.build(frameRequest);
                require(frame.ok, "phase4_overlay_ui_frame_failed:" + capture.id);
                const UiAudit uiAudit = audit(
                    frame, session.shell, frameRequest.viewport);
                if (!benchmarked && capture.advanced) {
                    performance = benchmark(renderer, presentation, session,
                        environment, warmup, samples);
                    benchmarked = true;
                }
                renderer.renderProductFrame(presentation, session, environment);
                glFinish();
                const ModernFrameMetrics metrics = renderer.metrics();
                const training::TrajectoryOverlayPalette overlayPalette =
                    capture.palette == "standard" ?
                        training::TrajectoryOverlayPalette::Standard :
                        training::TrajectoryOverlayPalette::ColorVisionSafe;
                const auto cpuPackage = training::buildTrajectoryOverlayDrawPackage(
                    session.shell.trajectoryOverlay, presentation.camera,
                    {capture.physicalWidth, capture.physicalHeight,
                        capture.deviceScale, capture.uiScale},
                    overlayPalette);
                const bool predictedRequired = capture.overlayState !=
                    "actual_replay_paused_collapsed";
                const bool actualRequired = capture.overlayState !=
                    "predicted_only_active_collapsed";
                const bool endpointContract =
                    (!predictedRequired ||
                        cpuPackage.predicted.endpointMarkerVisible ||
                        cpuPackage.predicted.endpointOffscreen) &&
                    (!actualRequired ||
                        cpuPackage.actual.endpointMarkerVisible ||
                        cpuPackage.actual.endpointOffscreen);
                const bool trackContract =
                    (predictedRequired ?
                        cpuPackage.predicted.visibleStrokeCount > 0 :
                        cpuPackage.predicted.visibleStrokeCount == 0) &&
                    (actualRequired ?
                        cpuPackage.actual.visibleStrokeCount > 0 &&
                            cpuPackage.actual.tickMarkCount > 0 :
                        cpuPackage.actual.visibleStrokeCount == 0 &&
                            cpuPackage.actual.tickMarkCount == 0);
                const bool comparisonRequired = predictedRequired && actualRequired;
                const bool topologyContract = !comparisonRequired ||
                    cpuPackage.nonColorTopologySeparationApplied;
                const bool physicalStyleContract =
                    exactPhysicalStyle(capture, cpuPackage.style);
                const std::string predictedRails = "Rails " + std::to_string(
                    session.shell.trajectoryOverlay.predicted.railCollisionEvents);
                const std::string actualRails = "Rails " + std::to_string(
                    session.shell.trajectoryOverlay.actual.railCollisionEvents);
                const bool railsTextContract = actualRequired &&
                    textRunCount(frame, actualRails) ==
                        (predictedRequired && predictedRails == actualRails ? 2 : 1) &&
                    (!predictedRequired || predictedRails == actualRails ||
                        textRunCount(frame, predictedRails) == 1);
                const bool actualOnlyLegendContract =
                    capture.overlayState != "actual_replay_paused_collapsed" ||
                    (textRunCount(frame, "Replay actual") >= 1 &&
                        textRunCount(frame, "Predicted") == 0 &&
                        textRunCount(frame, "Actual topology") >= 1 &&
                        textRunCount(frame, "Solid + ticks") >= 1 &&
                        textRunCount(frame, "Filled endpoint") >= 1);
                const bool advancedTextContract = !capture.advanced ||
                    (textRunCount(frame, "Predicted sample count") >= 1 &&
                        textRunCount(frame, "Actual sample count") >= 1 &&
                        textRunCount(frame, "First replay tick") >= 1 &&
                        textRunCount(frame, "Last replay tick") >= 1 &&
                        textRunCount(frame, "Preview budget outcome") >= 1 &&
                        textRunCount(frame, "Authority status") >= 1 &&
                        textRunCount(frame, "Trace hash") >= 1 &&
                        textRunCount(frame, "Content hash") >= 1);
                const ui::ShellInspectorContentAudit contentAudit =
                    ui::auditPhase4InspectorContent(session.shell);
                std::uint32_t completeDrawCalls = 0;
                for (const RenderPassMetric& pass : metrics.passes)
                    completeDrawCalls += pass.drawCalls;
                const std::size_t textureResidentBytes = metrics.pbrGpuBytes +
                    metrics.environmentGpuBytes;
                const std::filesystem::path ppm = std::filesystem::path(argv[2]) /
                    (capture.id + ".ppm");
                require(renderer.capture({ppm.string(), capture.physicalWidth,
                    capture.physicalHeight}, error), error);
                const training::TrajectoryContrastAudit contrast =
                    contrastAudit(readPpm(ppm), cpuPackage, overlayPalette,
                        predictedRequired, actualRequired);
                const bool machinePass = cpuPackage.ok && endpointContract &&
                    trackContract && topologyContract && physicalStyleContract &&
                    railsTextContract &&
                    actualOnlyLegendContract && advancedTextContract &&
                    contentAudit.ok &&
                    metrics.trajectoryOverlayDrawCalls == 1 &&
                    metrics.trajectoryOverlayTextureCount == 0 &&
                    metrics.trajectoryOverlayUsedSharedProjection &&
                    metrics.trajectoryOverlayHomogeneousClip &&
                    metrics.trajectoryOverlaySafeAreaClip &&
                    metrics.trajectoryOverlayPlaySurfaceClip &&
                    metrics.trajectoryOverlayErrorCode.empty() &&
                    completeDrawCalls <= 100 &&
                    textureResidentBytes <= 256u * 1024u * 1024u &&
                    cpuPackage.predicted.vertexCount <= 8192 &&
                    cpuPackage.actual.vertexCount <= 8192 &&
                    cpuPackage.textureResidencyDeltaBytes == 0 &&
                    uiAudit.safe == 0 && uiAudit.truncation == 0 &&
                    uiAudit.overlap == 0 && uiAudit.boundedInspector &&
                    uiAudit.gameplayCorridor &&
                    uiAudit.inspectorBackdropBounded &&
                    uiAudit.geometryGate &&
                    uiAudit.protectedIntersectionArea == 0.0 &&
                    uiAudit.debugNodes == 0 && uiAudit.unexpandedIcons == 0 &&
                    uiAudit.opaqueIcons == 0 &&
                    uiAudit.panelAlphaViolations == 0 &&
                    contrast.ok && contrast.minimumLocalContrastRatio >= 4.0 &&
                    contrast.designTargetCoverage >= 0.99 &&
                    fixture.authorityUnchanged();
                if (!machinePass) {
                    std::cerr << "phase4-overlay-diagnostic id=" << capture.id
                        << " package=" << cpuPackage.ok
                        << " error=" << cpuPackage.errorCode
                        << " endpoints=" << endpointContract
                        << " predicted_strokes=" << cpuPackage.predicted.visibleStrokeCount
                        << " actual_strokes=" << cpuPackage.actual.visibleStrokeCount
                        << " ticks=" << cpuPackage.actual.tickMarkCount
                        << " topology=" << topologyContract
                        << " physical_style=" << physicalStyleContract
                        << " rails_text=" << railsTextContract
                        << " actual_legend=" << actualOnlyLegendContract
                        << " advanced_text=" << advancedTextContract
                        << " content_gate=" << contentAudit.ok
                        << " content_error=" << contentAudit.errorCode
                        << " draws=" << metrics.trajectoryOverlayDrawCalls
                        << " textures=" << metrics.trajectoryOverlayTextureCount
                        << " shared=" << metrics.trajectoryOverlayUsedSharedProjection
                        << " clip=" << metrics.trajectoryOverlayHomogeneousClip
                        << " safe_clip=" << metrics.trajectoryOverlaySafeAreaClip
                        << " table_clip=" << metrics.trajectoryOverlayPlaySurfaceClip
                        << " render_error=" << metrics.trajectoryOverlayErrorCode
                        << " ui_safe=" << uiAudit.safe
                        << " trunc=" << uiAudit.truncation
                        << " overlap=" << uiAudit.overlap
                        << " inspector=" << uiAudit.boundedInspector
                        << " inspector_coverage=" << uiAudit.inspectorViewportCoverage
                        << " corridor_fraction=" <<
                            uiAudit.gameplayCorridorWidthFraction
                        << " backdrop=" << uiAudit.inspectorBackdropBounded
                        << " geometry_gate=" << uiAudit.geometryGate
                        << " protected_intersection=" <<
                            uiAudit.protectedIntersectionArea
                        << " contrast_ok=" << contrast.ok
                        << " contrast_error=" << contrast.errorCode
                        << " contrast_min=" << contrast.minimumLocalContrastRatio
                        << " contrast_p01=" << contrast.p01LocalContrastRatio
                        << " contrast_target_coverage=" <<
                            contrast.designTargetCoverage
                        << " role_luma_delta=" <<
                            contrast.predictedActualLumaDeltaSrgb8
                        << " contrast_visible_probes=" <<
                            contrast.visibleProbeCount
                        << " contrast_hidden_probes=" <<
                            contrast.hiddenProbeCount
                        << " contrast_visible_coverage=" <<
                            contrast.visibleProbeCoverage
                        << " debug_nodes=" << uiAudit.debugNodes
                        << " unexpanded_icons=" << uiAudit.unexpandedIcons
                        << " panel_alpha_violations=" <<
                            uiAudit.panelAlphaViolations
                        << " authority=" << fixture.authorityUnchanged() << '\n';
                    if (!contrast.failureCoordinates.empty()) {
                        std::cerr << "phase4-overlay-contrast-failures id="
                            << capture.id << " coordinates=";
                        for (std::size_t coordinateIndex = 0;
                             coordinateIndex < contrast.failureCoordinates.size() &&
                                 coordinateIndex < 24;
                             ++coordinateIndex) {
                            if (coordinateIndex != 0) std::cerr << ';';
                            std::cerr <<
                                contrast.failureCoordinates[coordinateIndex][0]
                                << ',' <<
                                contrast.failureCoordinates[coordinateIndex][1];
                        }
                        std::cerr << '\n';
                    }
                    for (const auto& text : frame.draw.textRuns) {
                        if (text.measuredWidth > text.clipRect.width + 0.02f)
                            std::cerr << "phase4-overlay-truncated-text id="
                                << capture.id << " text=" << text.text
                                << " measured=" << text.measuredWidth
                                << " available=" << text.clipRect.width << '\n';
                    }
                }
                require(machinePass, "phase4_overlay_machine_gate_failed:" + capture.id);
                json::Value row = json::Value::object();
                row["capture_id"] = json::Value(capture.id);
                row["scenario_id"] = json::Value(capture.scenarioId);
                row["scenario_sha256"] = json::Value(capture.scenarioSha256);
                row["replay_sha256"] = json::Value(capture.replaySha256);
                row["content_sha256"] = json::Value(capture.contentSha256);
                row["overlay_state"] = json::Value(capture.overlayState);
                row["terminal_outcome"] = json::Value(capture.terminalOutcome);
                row["ppm_path"] = json::Value(ppm.string());
                row["machine_status"] = json::Value(machineStatus);
                row["authority_before_equals_after"] = json::Value(true);
                row["overlay_vertices"] = json::Value(
                    static_cast<int>(metrics.trajectoryOverlayVertices));
                row["predicted_vertices"] = json::Value(
                    static_cast<int>(cpuPackage.predicted.vertexCount));
                row["actual_vertices"] = json::Value(
                    static_cast<int>(cpuPackage.actual.vertexCount));
                row["overlay_draw_calls"] = json::Value(1);
                row["overlay_new_textures"] = json::Value(0);
                row["overlay_texture_residency_delta_bytes"] = json::Value(0);
                row["complete_scene_draw_calls"] = json::Value(
                    static_cast<int>(completeDrawCalls));
                row["texture_resident_bytes"] = json::Value(
                    static_cast<double>(textureResidentBytes));
                row["safe_area_violations"] = json::Value(uiAudit.safe);
                row["truncated_text_runs"] = json::Value(uiAudit.truncation);
                row["overlap_pairs"] = json::Value(uiAudit.overlap);
                row["bounded_inspector"] = json::Value(uiAudit.boundedInspector);
                row["inspector_viewport_coverage"] = json::Value(
                    uiAudit.inspectorViewportCoverage);
                row["gameplay_corridor_width_fraction"] = json::Value(
                    uiAudit.gameplayCorridorWidthFraction);
                row["non_color_topology_separation"] = json::Value(
                    !comparisonRequired ||
                        cpuPackage.nonColorTopologySeparationApplied);
                row["exact_physical_style_contract"] = json::Value(
                    physicalStyleContract);
                row["rails_text_contract"] = json::Value(railsTextContract);
                row["actual_only_legend_contract"] = json::Value(
                    actualOnlyLegendContract);
                row["advanced_text_contract"] = json::Value(
                    advancedTextContract);
                row["inspector_content_contract"] = json::Value(
                    contentAudit.ok);
                row["protected_corridor_intersection_area_px"] = json::Value(
                    uiAudit.protectedIntersectionArea);
                row["minimum_local_contrast_ratio"] = json::Value(
                    contrast.minimumLocalContrastRatio);
                row["p01_local_contrast_ratio"] = json::Value(
                    contrast.p01LocalContrastRatio);
                row["contrast_4_5_target_coverage"] = json::Value(
                    contrast.designTargetCoverage);
                row["predicted_actual_luma_delta_srgb8"] = json::Value(
                    contrast.predictedActualLumaDeltaSrgb8);
                row["contrast_failure_coordinate_count"] = json::Value(
                    static_cast<int>(contrast.failureCoordinates.size()));
                row["contrast_source_probe_count"] = json::Value(
                    static_cast<int>(contrast.sourceProbeCount));
                row["contrast_visible_probe_count"] = json::Value(
                    static_cast<int>(contrast.visibleProbeCount));
                row["contrast_hidden_probe_count"] = json::Value(
                    static_cast<int>(contrast.hiddenProbeCount));
                row["contrast_visible_probe_coverage"] = json::Value(
                    contrast.visibleProbeCoverage);
                row["contrast_failure_coordinates"] = json::Value::array();
                for (const auto& coordinate : contrast.failureCoordinates) {
                    json::Value point = json::Value::array();
                    point.asArray().push_back(json::Value(coordinate[0]));
                    point.asArray().push_back(json::Value(coordinate[1]));
                    row["contrast_failure_coordinates"].asArray().push_back(point);
                }
                row["debug_node_count"] = json::Value(uiAudit.debugNodes);
                row["opaque_icon_count"] = json::Value(uiAudit.opaqueIcons);
                row["unexpanded_icon_count"] = json::Value(
                    uiAudit.unexpandedIcons);
                row["panel_alpha_violations"] = json::Value(
                    uiAudit.panelAlphaViolations);
                results << json::stringify(row) << '\n'; results.flush();
                std::cout << "phase4-overlay-preflight-capture " << index + 1 << '/'
                    << captures.size() << ' ' << capture.id << '\n' << std::flush;
            }
            require(benchmarked, "phase4_overlay_advanced_benchmark_missing");
            std::cout << "phase4-overlay-performance p50_ms=" << performance.p50
                << " p95_ms=" << performance.p95
                << " p99_ms=" << performance.p99
                << " overlay_cpu_p95_ms=" << performance.overlayCpuP95
                << " ui_overlay_gpu_p95_ms=" << performance.uiOverlayGpuP95
                << '\n' << std::flush;
            require(performance.p95 <= 16.7 && performance.overlayCpuP95 <= 0.5 &&
                performance.uiOverlayGpuP95 <= 1.0,
                "phase4_overlay_performance_gate_failed");
            GameRuntime longReplayAuthority;
            require(longReplayFailsClosed(presentationFor(captures.front(),
                longReplayAuthority).camera),
                "phase4_overlay_long_replay_did_not_fail_closed");
            json::Value summary = json::Value::object();
            summary["record_type"] = json::Value("summary");
            summary["machine_status"] = json::Value(machineStatus);
            summary["capture_count"] = json::Value(static_cast<int>(captures.size()));
            summary["warmup_frames"] = json::Value(warmup);
            summary["sample_frames"] = json::Value(samples);
            summary["p50_total_frame_ms"] = json::Value(performance.p50);
            summary["p95_total_frame_ms"] = json::Value(performance.p95);
            summary["p99_total_frame_ms"] = json::Value(performance.p99);
            summary["p95_overlay_cpu_build_ms"] = json::Value(performance.overlayCpuP95);
            summary["p95_ui_plus_overlay_gpu_ms"] = json::Value(performance.uiOverlayGpuP95);
            summary["per_pass_cpu_p95_ms"] = json::Value::object();
            for (const auto& value : performance.passCpuP95)
                summary["per_pass_cpu_p95_ms"][value.first] =
                    json::Value(value.second);
            summary["per_pass_gpu_p95_ms"] = json::Value::object();
            for (const auto& value : performance.passGpuP95)
                summary["per_pass_gpu_p95_ms"][value.first] =
                    json::Value(value.second);
            summary["long_replay_8192_fail_closed"] = json::Value(true);
            summary["ta_status"] = json::Value("NOT_RUN");
            summary["manual_status"] = json::Value("NOT_RUN");
            summary["m3_status"] = json::Value("NOT_CLAIMED");
            results << json::stringify(summary) << '\n'; results.flush();
            renderer.shutdown(); [NSOpenGLContext clearCurrentContext];
            std::cout << "phase4-overlay-gpu-capture: " << machineStatus
                << " captures=" << captures.size() << " TA_NOT_RUN\n";
            return EXIT_SUCCESS;
        } catch (const std::exception& error) {
            std::cerr << "phase4-overlay-gpu-preflight: " << error.what() << '\n';
            return EXIT_FAILURE;
        }
    }
}
