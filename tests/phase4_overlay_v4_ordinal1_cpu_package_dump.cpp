#include "automation_json.h"
#include "camera/camera_state_machine.h"
#include "deterministic_hash.h"
#include "replay/replay_transport_model.h"
#include "replay/replay_v1.h"
#include "render_state.h"
#include "training/training_product_controller.h"
#include "training/trajectory_compound_visibility.h"
#include "training/trajectory_overlay_model.h"
#include "training/trajectory_overlay_renderer.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <set>
#include <stdexcept>
#include <string>

namespace {

using namespace billiardgl;

constexpr const char* kCaptureId = "camera_player_low_1080";
constexpr const char* kScenarioId = "launch_stop_shot_center_v1";

json::Value readJson(const std::filesystem::path& path)
{
    std::ifstream input(path, std::ios::binary);
    if (!input) throw std::runtime_error("cannot_open:" + path.string());
    const std::string bytes((std::istreambuf_iterator<char>(input)),
        std::istreambuf_iterator<char>());
    const json::ParseResult parsed = json::parse(bytes);
    if (!parsed.ok) throw std::runtime_error("invalid_json:" + path.string());
    return parsed.value;
}

std::set<std::string> capabilities()
{
    return {"training_scenario_v1",
        "authoritative_horizontal_tip_offset_v1",
        "authoritative_longitudinal_tip_offset_v1"};
}

CameraPresentation playerLowCamera(GameRuntime& runtime)
{
    const PresentationSnapshot seed = buildPresentationSnapshot(
        runtime.state(), runtime.state(), 0.0);
    camera::StateMachine camera;
    camera.reset(camera::SemanticState::AimPlayer,
        camera::Preset::PlayerLow, seed);
    CameraPresentation result = buildPresentationSnapshot(runtime.state(),
        runtime.state(), 0.0, runtime.shotControl(),
        runtime.shotControlCapabilities(), camera.frame(),
        buildRulesPresentation(runtime)).camera;
    result.viewportWidth = 1920;
    result.viewportHeight = 1080;
    return result;
}

json::Value point(double x, double y)
{
    json::Value result = json::Value::array();
    result.asArray().push_back(json::Value(x));
    result.asArray().push_back(json::Value(y));
    return result;
}

training::CompoundVisibilityTrack compoundTrack(
    training::TrajectoryContrastTrack track)
{
    return track == training::TrajectoryContrastTrack::Predicted ?
        training::CompoundVisibilityTrack::Predicted :
        training::CompoundVisibilityTrack::Actual;
}

const char* contrastTrackName(training::TrajectoryContrastTrack track)
{
    return track == training::TrajectoryContrastTrack::Predicted ?
        "predicted" : "actual";
}

}  // namespace

int main()
{
    try {
        GameRuntime authority;
        training::TrainingProductController controller(authority);
        const std::filesystem::path root(BILLIARDGL_SOURCE_ROOT);
        const std::filesystem::path pack =
            root / "assets/training/phase4/training_pack_v1.json";
        const std::filesystem::path replayPath = root /
            "docs/art/phase4-training-replay-overlay-fixtures-v3/"
            "launch_stop_shot_center_v1.overlay.replay.json";
        if (!controller.loadPack(pack.string(), capabilities()).ok ||
            !controller.startScenario(kScenarioId,
                "phase4-overlay-gpu-fixture-v1", 77).ok ||
            !controller.refreshPreview().ok || controller.previewResult() == nullptr)
            throw std::runtime_error("preview_setup_failed");

        replay::Document document;
        if (!replay::parse(readJson(replayPath), document).ok)
            throw std::runtime_error("replay_parse_failed");
        replay::ReplayTransportModel transport;
        if (!transport.load(document, authority.physicsProfile(),
                document.executableVersion, document.contentSha256).ok)
            throw std::runtime_error("replay_load_failed");
        for (int step = 0; step < 10000 && !transport.snapshot().finished; ++step)
            if (!transport.stepTick().ok)
                throw std::runtime_error("replay_step_failed");
        if (!transport.snapshot().finished)
            throw std::runtime_error("replay_timeout");

        training::TrajectoryOverlayModel model;
        model.setPredicted(*controller.previewResult());
        model.setActual(transport.snapshot());
        const training::TrajectoryOverlaySnapshot snapshot = model.snapshot();
        const auto package = training::buildTrajectoryOverlayDrawPackage(snapshot,
            playerLowCamera(authority), {1920, 1080, 1.0, 1.0},
            training::TrajectoryOverlayPalette::Standard);
        if (!package.ok) throw std::runtime_error(package.errorCode);
        if (package.contrastProbes.size() != 95)
            throw std::runtime_error("ordinal1_source_probe_count_not_95:" +
                std::to_string(package.contrastProbes.size()));

        json::Value result = json::Value::object();
        result["schema"] = json::Value(
            "billiardgl.phase4.overlay_v4_ordinal1_cpu_package_dump.v1");
        result["status"] = json::Value("CPU_ONLY_FROZEN_PACKAGE_RECONSTRUCTED");
        result["capture_id"] = json::Value(kCaptureId);
        result["scenario_id"] = json::Value(kScenarioId);
        result["camera_preset"] = json::Value("player_low");
        result["physical_viewport_px"] = json::Value::array();
        result["physical_viewport_px"].asArray().push_back(json::Value(1920));
        result["physical_viewport_px"].asArray().push_back(json::Value(1080));
        result["actual_gpu_capture"] = json::Value(false);
        result["cocoa_or_opengl_used"] = json::Value(false);
        result["source_probe_count"] = json::Value(
            static_cast<int>(package.contrastProbes.size()));
        result["vertex_count"] = json::Value(
            static_cast<int>(package.vertices.size()));
        result["semantic_batch_count"] = json::Value(
            static_cast<int>(package.semanticBatches.size()));
        result["style"] = json::Value::object();
        result["style"]["line_width_px"] = json::Value(package.style.lineWidthPx);
        result["style"]["halo_width_px"] = json::Value(package.style.haloWidthPx);
        result["style"]["dash_on_px"] = json::Value(package.style.dashOnPx);
        result["style"]["dash_off_px"] = json::Value(package.style.dashOffPx);
        result["style"]["actual_tick_length_px"] =
            json::Value(package.style.actualTickLengthPx);
        result["style"]["actual_tick_spacing_px"] =
            json::Value(package.style.actualTickSpacingPx);
        result["style"]["predicted_lateral_offset_px"] =
            json::Value(package.style.predictedLateralOffsetPx);
        result["style"]["actual_lateral_offset_px"] =
            json::Value(package.style.actualLateralOffsetPx);

        const std::string physicalProfile = "1920x1080|line=" +
            std::to_string(package.style.lineWidthPx) + "|halo=" +
            std::to_string(package.style.haloWidthPx);
        result["source_probes"] = json::Value::array();
        std::string orderedIds;
        for (const auto& probe : package.contrastProbes) {
            training::CompoundSourceProbe source;
            source.captureId = kCaptureId;
            source.track = compoundTrack(probe.track);
            source.primitiveId = probe.primitiveId;
            source.componentRole = probe.componentRole;
            source.arcSegmentId = probe.arcSegmentId;
            source.probeOrdinal = probe.probeOrdinal;
            source.physicalProfile = physicalProfile;
            source.xPhysicalPx = probe.componentRole ==
                training::CompoundComponentRole::OuterHaloEdge ?
                    probe.haloXPhysicalPx : probe.strokeXPhysicalPx;
            source.yPhysicalPx = probe.componentRole ==
                training::CompoundComponentRole::OuterHaloEdge ?
                    probe.haloYPhysicalPx : probe.strokeYPhysicalPx;
            source.outerHaloRadiusPx = package.style.lineWidthPx * 0.5 +
                package.style.haloWidthPx;
            source.halfTickLengthPx = package.style.actualTickLengthPx * 0.5;
            source.sourceProbeSpacingPx = source.track ==
                training::CompoundVisibilityTrack::Predicted ?
                    package.style.dashOnPx + package.style.dashOffPx :
                    package.style.actualTickSpacingPx;
            source.insideFinalViewport = source.xPhysicalPx >= 0 &&
                source.yPhysicalPx >= 0 && source.xPhysicalPx < 1920 &&
                source.yPhysicalPx < 1080;
            const std::string id = training::stableCompoundSourceProbeId(source);
            orderedIds += id + "\n";

            double nx = static_cast<double>(probe.haloXPhysicalPx -
                probe.strokeXPhysicalPx);
            double ny = static_cast<double>(probe.haloYPhysicalPx -
                probe.strokeYPhysicalPx);
            if (std::hypot(nx, ny) < 0.5) {
                nx = static_cast<double>(probe.backgroundXPhysicalPx -
                    probe.strokeXPhysicalPx);
                ny = static_cast<double>(probe.backgroundYPhysicalPx -
                    probe.strokeYPhysicalPx);
            }
            const double normalLength = std::max(std::hypot(nx, ny), 1e-9);
            nx /= normalLength;
            ny /= normalLength;
            const double tx = ny;
            const double ty = -nx;
            const double normalBound = probe.componentRole ==
                training::CompoundComponentRole::Tick ?
                    source.halfTickLengthPx + 2.0 :
                    source.outerHaloRadiusPx + 2.0;
            const double longitudinalBound = std::min(12.0,
                source.sourceProbeSpacingPx * 0.5);

            json::Value row = json::Value::object();
            row["source_probe_id"] = json::Value(id);
            row["probe_ordinal"] = json::Value(
                static_cast<double>(probe.probeOrdinal));
            row["track"] = json::Value(contrastTrackName(probe.track));
            row["component_role"] = json::Value(
                training::compoundComponentRoleName(probe.componentRole));
            row["primitive_id"] = json::Value(probe.primitiveId);
            row["arc_segment_id"] = json::Value(probe.arcSegmentId);
            row["physical_profile"] = json::Value(physicalProfile);
            row["physical_coordinate_px"] = point(source.xPhysicalPx,
                source.yPhysicalPx);
            row["stroke_coordinate_px"] = point(probe.strokeXPhysicalPx,
                probe.strokeYPhysicalPx);
            row["halo_coordinate_px"] = point(probe.haloXPhysicalPx,
                probe.haloYPhysicalPx);
            row["background_coordinate_px"] = point(
                probe.backgroundXPhysicalPx, probe.backgroundYPhysicalPx);
            row["local_tangent"] = point(tx, ty);
            row["local_normal"] = point(nx, ny);
            row["arc_interval_local_px"] = point(-longitudinalBound,
                longitudinalBound);
            row["normal_bound_px"] = json::Value(normalBound);
            row["longitudinal_bound_px"] = json::Value(longitudinalBound);
            row["inside_final_viewport"] =
                json::Value(source.insideFinalViewport);
            result["source_probes"].asArray().push_back(row);
        }
        result["ordered_source_probe_digest"] =
            json::Value(sha256Hex(orderedIds));

        result["semantic_triangles"] = json::Value::array();
        int triangleOrdinal = 0;
        for (std::size_t batchIndex = 0;
             batchIndex < package.semanticBatches.size(); ++batchIndex) {
            const auto& batch = package.semanticBatches[batchIndex];
            if (batch.firstVertex + batch.vertexCount > package.vertices.size() ||
                batch.vertexCount % 3 != 0)
                throw std::runtime_error("semantic_batch_range_invalid");
            for (std::size_t offset = 0; offset < batch.vertexCount; offset += 3) {
                const auto& a = package.vertices[batch.firstVertex + offset];
                const auto& b = package.vertices[batch.firstVertex + offset + 1];
                const auto& c = package.vertices[batch.firstVertex + offset + 2];
                const double left = std::min({static_cast<double>(a.xPhysicalPx),
                    static_cast<double>(b.xPhysicalPx),
                    static_cast<double>(c.xPhysicalPx)});
                const double right = std::max({static_cast<double>(a.xPhysicalPx),
                    static_cast<double>(b.xPhysicalPx),
                    static_cast<double>(c.xPhysicalPx)});
                const double top = std::min({static_cast<double>(a.yPhysicalPx),
                    static_cast<double>(b.yPhysicalPx),
                    static_cast<double>(c.yPhysicalPx)});
                const double bottom = std::max({static_cast<double>(a.yPhysicalPx),
                    static_cast<double>(b.yPhysicalPx),
                    static_cast<double>(c.yPhysicalPx)});
                json::Value row = json::Value::object();
                row["production_draw_ordinal"] = json::Value(triangleOrdinal++);
                row["semantic_batch_ordinal"] =
                    json::Value(static_cast<int>(batchIndex));
                row["track"] = json::Value(contrastTrackName(batch.track));
                row["component_role"] = json::Value(
                    training::compoundComponentRoleName(batch.componentRole));
                row["primitive_id"] = json::Value(batch.primitiveId);
                row["arc_segment_id"] = json::Value(batch.arcSegmentId);
                row["vertices_px"] = json::Value::array();
                row["vertices_px"].asArray().push_back(
                    point(a.xPhysicalPx, a.yPhysicalPx));
                row["vertices_px"].asArray().push_back(
                    point(b.xPhysicalPx, b.yPhysicalPx));
                row["vertices_px"].asArray().push_back(
                    point(c.xPhysicalPx, c.yPhysicalPx));
                row["bounds_px"] = json::Value::array();
                for (double value : {left, top, right, bottom})
                    row["bounds_px"].asArray().push_back(json::Value(value));
                result["semantic_triangles"].asArray().push_back(row);
            }
        }
        result["semantic_triangle_count"] = json::Value(triangleOrdinal);
        result["limitations"] = json::Value::array();
        result["limitations"].asArray().push_back(json::Value(
            "CPU semantic triangles are analytic pre-composite geometry; they "
            "do not prove resolved final-frame ownership."));
        result["limitations"].asArray().push_back(json::Value(
            "Local tangent/normal is reconstructed from the immutable probe "
            "stroke-to-halo/background sampling frame."));

        std::cout << json::stringify(result) << '\n';
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
