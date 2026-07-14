#include "game_runtime.h"

#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdlib>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <numeric>
#include <string>
#include <vector>
#include <sys/resource.h>

namespace {

struct Metrics {
    double meanMs = 0.0;
    double p95Ms = 0.0;
    double p99Ms = 0.0;
    std::size_t peakRssBytes = 0;
    std::size_t artifactBytesPerTick = 0;
};

Metrics measure()
{
    billiardgl::GameRuntime runtime;
    runtime.step(3);
    runtime.setPhysicsTraceEnabled(true);
    billiardgl::CueImpactInput input;
    input.cueBallIndex = 0;
    input.cueSpeedCmS = 120.0;
    input.cueMassKg = runtime.physicsProfile().cue.effectiveMassKg;
    input.direction = {{0.0, 0.0, 1.0}};
    input.tipOffsetCm = {{0.0, 0.0}};
    input.tipOffsetRadius = {{0.0, 0.0}};
    input.chalkState = "chalked";
    runtime.applyCueImpact(input);
    std::vector<double> durations;
    durations.reserve(1000);
    for (int tick = 0; tick < 1000; ++tick) {
        const auto start = std::chrono::steady_clock::now();
        runtime.step(1);
        const auto end = std::chrono::steady_clock::now();
        durations.push_back(std::chrono::duration<double, std::milli>(end - start).count());
    }
    Metrics metrics;
    metrics.meanMs = std::accumulate(durations.begin(), durations.end(), 0.0) /
        durations.size();
    std::sort(durations.begin(), durations.end());
    metrics.p95Ms = durations[949];
    metrics.p99Ms = durations[989];
    struct rusage usage {};
    getrusage(RUSAGE_SELF, &usage);
#if defined(__APPLE__)
    metrics.peakRssBytes = static_cast<std::size_t>(usage.ru_maxrss);
#else
    metrics.peakRssBytes = static_cast<std::size_t>(usage.ru_maxrss) * 1024;
#endif
    std::size_t bytes = 0;
    for (const billiardgl::PhysicsFrame& frame : runtime.physicsTrace().frames()) {
        bytes += sizeof(frame);
        bytes += frame.contacts.size() * sizeof(billiardgl::PhysicsContactRecord);
        bytes += frame.solverEvents.size() * sizeof(billiardgl::SolverEventRecord);
        bytes += frame.surfaceTransitions.size() * sizeof(billiardgl::SurfaceMotionStep);
    }
    metrics.artifactBytesPerTick = bytes / runtime.physicsTrace().frames().size();
    return metrics;
}

}  // namespace

int main(int argc, char** argv)
{
    const Metrics metrics = measure();
    if (!std::isfinite(metrics.meanMs) || metrics.meanMs > 1.0 ||
        metrics.p95Ms > 2.0 || metrics.p99Ms > 5.0 ||
        metrics.peakRssBytes > 536870912ULL ||
        metrics.artifactBytesPerTick > 100000) {
        std::cerr << "performance budget exceeded: mean=" << metrics.meanMs
                  << " p95=" << metrics.p95Ms << " p99=" << metrics.p99Ms
                  << " rss=" << metrics.peakRssBytes
                  << " artifact=" << metrics.artifactBytesPerTick << '\n';
        return EXIT_FAILURE;
    }
    if (argc == 3 && std::string(argv[1]) == "--write") {
        std::ofstream output(argv[2]);
        if (!output) return EXIT_FAILURE;
        output << std::setprecision(17)
               << "{\n  \"artifact_bytes_per_tick\": " << metrics.artifactBytesPerTick
               << ",\n  \"mean_step_ms\": " << metrics.meanMs
               << ",\n  \"p95_step_ms\": " << metrics.p95Ms
               << ",\n  \"p99_step_ms\": " << metrics.p99Ms
               << ",\n  \"peak_rss_bytes\": " << metrics.peakRssBytes
               << ",\n  \"schema_version\": 1,\n  \"ticks\": 1000\n}\n";
    }
    return EXIT_SUCCESS;
}
