#include "ai/shot_planner.h"

#include <array>
#include <cmath>
#include <cstdlib>
#include <iomanip>
#include <iostream>
#include <stdexcept>
#include <vector>

namespace {

using billiardgl::ai::Difficulty;
using billiardgl::ai::PreviewEvaluator;
using billiardgl::ai::PreviewScore;
using billiardgl::ai::SemanticShotAction;
using billiardgl::ai::ShotCandidate;
using billiardgl::ai::ShotPlanner;

class AcceptancePreview final : public PreviewEvaluator {
public:
    PreviewScore evaluate(const SemanticShotAction&,
        std::chrono::steady_clock::time_point) override
    {
        return {true, 0.85f, 0.70f, 0.60f, 0.02f};
    }
};

struct Interval {
    double low = 0.0;
    double high = 0.0;
};

Interval wilson95(int successes, int attempts)
{
    constexpr double z = 1.959963984540054;
    const double n = attempts;
    const double p = successes / n;
    const double denominator = 1.0 + z * z / n;
    const double center = (p + z * z / (2.0 * n)) / denominator;
    const double radius = z * std::sqrt((p * (1.0 - p) + z * z / (4.0 * n)) / n) /
        denominator;
    return {center - radius, center + radius};
}

int fixedAcceptanceRun(Difficulty difficulty)
{
    int successes = 0;
    constexpr std::array<float, 3> targetYaw{{0.35f, 1.55f, 2.65f}};
    for (int stratum = 0; stratum < 3; ++stratum) {
        for (int seed = 0; seed < 200; ++seed) {
            ShotCandidate only;
            only.action.targetBall = 1 + stratum;
            only.action.targetPocket = stratum * 2;
            only.action.controls.schemaVersion =
                billiardgl::ShotControlSchemaVersion::IntentV2;
            only.action.controls.yawRadians = targetYaw[stratum];
            only.action.controls.requestedPower = 52.0f + 8.0f * stratum;
            only.visibility = 1.0f;
            AcceptancePreview preview;
            const std::uint64_t deterministicSeed =
                static_cast<std::uint64_t>(seed + stratum * 200);
            const auto result = ShotPlanner(difficulty, deterministicSeed).plan(
                std::vector<ShotCandidate>{only}, preview,
                {1, std::chrono::milliseconds(50)});
            if (!result.found) throw std::runtime_error("acceptance planner found no shot");
            // The rack fixture declares the reference policy beaten when the
            // semantic shot remains inside its 0.02-radian execution window.
            // All difficulty effects are applied to public player controls.
            if (std::fabs(result.action.controls.yawRadians - targetYaw[stratum]) <=
                0.020f) ++successes;
        }
    }
    return successes;
}

void requireRate(const char* id, int successes, double minimum, double maximum)
{
    constexpr int attempts = 600;
    const double rate = successes / static_cast<double>(attempts);
    const Interval interval = wilson95(successes, attempts);
    std::cout << id << " " << successes << "/" << attempts << " rate="
              << std::fixed << std::setprecision(4) << rate
              << " wilson95=[" << interval.low << "," << interval.high << "]\n";
    if (rate < minimum || rate > maximum)
        throw std::runtime_error(std::string(id) + " outside approved target range");
}

}  // namespace

int main()
{
    try {
        const int rookie = fixedAcceptanceRun(Difficulty::Rookie);
        const int club = fixedAcceptanceRun(Difficulty::Club);
        const int expert = fixedAcceptanceRun(Difficulty::Expert);
        requireRate("rookie", rookie, 0.35, 0.50);
        requireRate("club", club, 0.58, 0.72);
        requireRate("expert", expert, 0.78, 0.88);
        if (!(rookie < club && club < expert))
            throw std::runtime_error("AI difficulty success rates are not monotonic");
        return EXIT_SUCCESS;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return EXIT_FAILURE;
    }
}
