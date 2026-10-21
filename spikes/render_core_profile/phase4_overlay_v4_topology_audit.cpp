#include "automation_json.h"
#include "camera/camera_state_machine.h"
#include "replay/replay_transport_model.h"
#include "replay/replay_v1.h"
#include "training/training_product_controller.h"
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
#include <vector>

namespace {

using namespace billiardgl;

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

CameraPresentation cameraFor(const std::string& id)
{
    camera::Preset preset = camera::Preset::Elevated;
    float fov = 48.0f;
    if (id == "player_low") { preset = camera::Preset::PlayerLow; fov = 42.0f; }
    else if (id == "elevated") { preset = camera::Preset::Elevated; fov = 48.0f; }
    else if (id == "overhead") { preset = camera::Preset::Overhead; fov = 40.0f; }
    else if (id == "broadcast") { preset = camera::Preset::Broadcast; fov = 45.0f; }
    else throw std::runtime_error("unknown_camera:" + id);
    const camera::PresetDefinition& definition = camera::definition(preset);
    const float horizontal = std::cos(definition.pitchRadians) *
        definition.distanceCm;
    CameraPresentation result;
    result.target = {0.0f, 86.5f, 0.0f};
    result.eye = {
        std::cos(definition.yawRadians) * horizontal,
        result.target.y + std::sin(definition.pitchRadians) *
            definition.distanceCm,
        std::sin(definition.yawRadians) * horizontal};
    result.fieldOfViewDegrees = fov;
    result.viewportWidth = 1920;
    result.viewportHeight = 1080;
    return result;
}

bool sameColor(const std::array<float, 4>& first,
    const std::array<float, 4>& second)
{
    for (int index = 0; index < 4; ++index) {
        if (std::fabs(first[index] - second[index]) > 1e-6f) return false;
    }
    return true;
}

double edge(double ax, double ay, double bx, double by, double px, double py)
{
    return (px - ax) * (by - ay) - (py - ay) * (bx - ax);
}

void rasterizeCore(const training::TrajectoryOverlayDrawPackage& package,
    const std::array<float, 4>& coreColor, int width, int height,
    std::vector<unsigned char>& mask)
{
    mask.assign(static_cast<std::size_t>(width) * height, 0);
    for (std::size_t index = 0; index + 2 < package.vertices.size(); index += 3) {
        const auto& a = package.vertices[index];
        const auto& b = package.vertices[index + 1];
        const auto& c = package.vertices[index + 2];
        if (!sameColor(a.linearRgba, coreColor) ||
            !sameColor(b.linearRgba, coreColor) ||
            !sameColor(c.linearRgba, coreColor))
            continue;
        const int left = std::max(0, static_cast<int>(std::floor(
            std::min({a.xPhysicalPx, b.xPhysicalPx, c.xPhysicalPx}))));
        const int right = std::min(width - 1, static_cast<int>(std::ceil(
            std::max({a.xPhysicalPx, b.xPhysicalPx, c.xPhysicalPx}))));
        const int top = std::max(0, static_cast<int>(std::floor(
            std::min({a.yPhysicalPx, b.yPhysicalPx, c.yPhysicalPx}))));
        const int bottom = std::min(height - 1, static_cast<int>(std::ceil(
            std::max({a.yPhysicalPx, b.yPhysicalPx, c.yPhysicalPx}))));
        const double orientation = edge(a.xPhysicalPx, a.yPhysicalPx,
            b.xPhysicalPx, b.yPhysicalPx, c.xPhysicalPx, c.yPhysicalPx);
        if (std::fabs(orientation) < 1e-9) continue;
        for (int y = top; y <= bottom; ++y) {
            for (int x = left; x <= right; ++x) {
                const double px = x + 0.5;
                const double py = y + 0.5;
                const double first = edge(a.xPhysicalPx, a.yPhysicalPx,
                    b.xPhysicalPx, b.yPhysicalPx, px, py);
                const double second = edge(b.xPhysicalPx, b.yPhysicalPx,
                    c.xPhysicalPx, c.yPhysicalPx, px, py);
                const double third = edge(c.xPhysicalPx, c.yPhysicalPx,
                    a.xPhysicalPx, a.yPhysicalPx, px, py);
                if ((orientation > 0.0 && first >= 0.0 && second >= 0.0 &&
                        third >= 0.0) ||
                    (orientation < 0.0 && first <= 0.0 && second <= 0.0 &&
                        third <= 0.0))
                    mask[static_cast<std::size_t>(y) * width + x] = 1;
            }
        }
    }
}

}  // namespace

int main(int argc, char** argv)
{
    try {
        if (argc != 4) {
            std::cerr << "usage: topology-audit <scenario-id> <replay> <camera>\n";
            return 2;
        }
        const std::string scenarioId = argv[1];
        const std::filesystem::path replayPath = argv[2];
        const std::string cameraId = argv[3];
        GameRuntime authority;
        training::TrainingProductController controller(authority);
        const std::filesystem::path pack =
            std::filesystem::path(BILLIARDGL_SOURCE_ROOT) /
            "assets/training/phase4/training_pack_v1.json";
        if (!controller.loadPack(pack.string(), capabilities()).ok ||
            !controller.startScenario(scenarioId,
                "phase4-overlay-gpu-fixture-v1", 77).ok ||
            !controller.refreshPreview().ok || controller.previewResult() == nullptr)
            throw std::runtime_error("preview_setup_failed:" + scenarioId);
        replay::Document document;
        if (!replay::parse(readJson(replayPath), document).ok)
            throw std::runtime_error("replay_parse_failed:" + scenarioId);
        replay::ReplayTransportModel transport;
        if (!transport.load(document, authority.physicsProfile(),
                document.executableVersion, document.contentSha256).ok)
            throw std::runtime_error("replay_load_failed:" + scenarioId);
        for (int step = 0; step < 10000 && !transport.snapshot().finished; ++step) {
            if (!transport.stepTick().ok)
                throw std::runtime_error("replay_step_failed:" + scenarioId);
        }
        if (!transport.snapshot().finished)
            throw std::runtime_error("replay_timeout:" + scenarioId);
        training::TrajectoryOverlayModel model;
        model.setPredicted(*controller.previewResult());
        model.setActual(transport.snapshot());
        const training::TrajectoryOverlaySnapshot snapshot = model.snapshot();
        const auto package = training::buildTrajectoryOverlayDrawPackage(snapshot,
            cameraFor(cameraId), {1920, 1080, 1.0, 1.0},
            training::TrajectoryOverlayPalette::Standard);
        if (!package.ok) throw std::runtime_error(package.errorCode);
        std::vector<unsigned char> predicted;
        std::vector<unsigned char> actual;
        rasterizeCore(package, snapshot.predicted.encoding.color,
            1920, 1080, predicted);
        rasterizeCore(package, snapshot.actual.encoding.color,
            1920, 1080, actual);
        std::size_t predictedArea = 0;
        std::size_t actualArea = 0;
        std::size_t overlapArea = 0;
        for (std::size_t index = 0; index < predicted.size(); ++index) {
            predictedArea += predicted[index] != 0;
            actualArea += actual[index] != 0;
            overlapArea += predicted[index] != 0 && actual[index] != 0;
        }
        std::size_t predictedProbes = 0;
        std::size_t actualProbes = 0;
        for (const auto& probe : package.contrastProbes) {
            if (probe.track == training::TrajectoryContrastTrack::Predicted)
                ++predictedProbes;
            else
                ++actualProbes;
        }
        const std::size_t denominator = std::min(predictedArea, actualArea);
        const double fraction = denominator == 0 ? 1.0 :
            static_cast<double>(overlapArea) / denominator;
        json::Value result = json::Value::object();
        result["capture_id"] = json::Value(
            "core__" + scenarioId + "__" + cameraId);
        result["scenario_id"] = json::Value(scenarioId);
        result["camera_preset"] = json::Value(cameraId);
        result["predicted_core_area_px"] = json::Value(
            static_cast<double>(predictedArea));
        result["actual_core_area_px"] = json::Value(
            static_cast<double>(actualArea));
        result["core_overlap_area_px"] = json::Value(
            static_cast<double>(overlapArea));
        result["overlap_fraction_of_smaller_core"] = json::Value(fraction);
        result["predicted_source_probe_inventory"] = json::Value(
            static_cast<double>(predictedProbes));
        result["actual_source_probe_inventory"] = json::Value(
            static_cast<double>(actualProbes));
        result["both_tracks_have_source_probes"] = json::Value(
            predictedProbes > 0 && actualProbes > 0);
        result["audit_source"] = json::Value(
            "unchanged_projected_core_triangles_pre_render_cpu_topology");
        std::cout << json::stringify(result) << '\n';
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
