#pragma once

#include "game_runtime.h"

#include <array>
#include <cstdint>
#include <string>
#include <vector>

namespace billiardgl::training {

struct PreviewBudget {
    int maximumTicks = 240;
    int maximumWallMilliseconds = 50;
    int maximumBalls = kBallCount;
};

struct PreviewSample {
    std::uint64_t tick = 0;
    std::vector<Point3> positions;
    bool cueBallVisible = false;
    Point3 cueBallPosition;
};

struct PreviewResult {
    bool ok = false;
    bool tickBudgetReached = false;
    bool wallBudgetReached = false;
    bool eventsEmitted = false;
    int simulatedTicks = 0;
    int maximumTicks = 0;
    int maximumWallMilliseconds = 0;
    double elapsedWallMilliseconds = 0.0;
    int railCollisionEvents = 0;
    std::string errorCode;
    std::string finalStateSha256;
    std::array<Point3, kBallCount> finalPositions;
    std::array<bool, kBallCount> finalPocketed{{}};
    bool hasShotFacts = false;
    rules::ShotFacts shotFacts;
    std::vector<PreviewSample> samples;
};

class PreviewRuntime {
public:
    PreviewRuntime(const GameState& state, const PhysicsProfile& profile,
        PhysicsBoundaryMode boundaryMode,
        bool collectShotFacts = false);
    PreviewResult run(const CueImpactInput& shot, const PreviewBudget& budget) const;

private:
    GameState state_;
    PhysicsProfile profile_;
    PhysicsBoundaryMode boundaryMode_;
    bool collectShotFacts_ = false;
};

PreviewResult previewTrainingAssist(
    const GameRuntime& authority, const PreviewBudget& budget);

}  // namespace billiardgl::training
