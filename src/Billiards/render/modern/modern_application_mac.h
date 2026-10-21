#pragma once

#include "ai/shot_planner.h"
#include "render/modern/modern_renderer.h"

#include <iosfwd>
#include <map>
#include <string>

namespace billiardgl::render {

struct ModernApplicationOptions {
    int width = 1920;
    int height = 1080;
    int msaaSamples = 4;
    RenderQuality quality = RenderQuality::Medium;
    bool automation = false;
    bool productMode = false;
    bool aiOpponent = false;
    ai::Difficulty aiDifficulty = ai::Difficulty::Club;
    bool showWindow = true;
    std::string title = "BilliardGL Modern";
    std::string lookdevManifestPath;
    std::string pbrParametersPath;
    std::string pbrAssetManifestPath;
    std::string lightingRigPath;
    std::string gltfScenePath;
    std::string venueScenePath;
    std::string phase2AssetRoot;
    bool formalCapture = false;
    std::string productSettingsPath;
    std::string trainingManifestPath;
    std::string executableVersion = "billiardgl-product-source-v1";
    std::string screenshotPath;
    std::string screenshotScene = "default";
    // Visible production-loop performance evidence. Empty disables collection.
    // The formal entry requires a 1920x1080 Medium/MSAA4 drawable and records
    // 120 warmup plus 300 sampled presented frames.
    std::string productFramePerformanceReportPath;
};

struct BenchmarkPercentiles {
    double p50 = 0.0;
    double p95 = 0.0;
    double p99 = 0.0;
};

struct ModernBenchmarkResult {
    std::map<std::string, BenchmarkPercentiles> cpuPassMilliseconds;
    std::map<std::string, BenchmarkPercentiles> gpuPassMilliseconds;
    int warmupFrames = 0;
    int measuredFrames = 0;
};

// Intended production bridge: main can call this before any GLUT setup when
// renderer selection is modern. Automation uses the supplied streams.
int runModernApplication(const ModernApplicationOptions& options,
                         std::istream& input, std::ostream& output,
                         std::ostream& errors);

ModernBenchmarkResult runModernRendererBenchmark(
    const ModernApplicationOptions& options, int warmupFrames, int measuredFrames);

}  // namespace billiardgl::render
