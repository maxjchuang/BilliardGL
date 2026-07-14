#pragma once

#include "continuous_collision.h"

#include <vector>

namespace billiardgl {

struct ContactIsland {
    int islandId = -1;
    double timeOfImpactSeconds = 0.0;
    std::vector<int> ballIndices;
    std::vector<ContinuousContactCandidate> contacts;
    bool limitExceeded = false;
};

struct ContactIslandBuildResult {
    std::vector<ContactIsland> islands;
    int duplicateCandidatesRemoved = 0;
    bool limitExceeded = false;
};

ContactIslandBuildResult buildEarliestContactIslands(
    const std::vector<ContinuousContactCandidate>& candidates,
    double toiToleranceSeconds, int maximumIslandSize);

}  // namespace billiardgl
