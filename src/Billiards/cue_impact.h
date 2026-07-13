#pragma once

#include <array>
#include <string>
#include <vector>

namespace billiardgl {

struct CueImpactInput {
    int cueBallIndex = 0;
    double cueSpeedCmS = 0.0;
    double cueMassKg = 0.0;
    std::array<double, 3> direction{{0.0, 0.0, 0.0}};
    double elevationDegrees = 0.0;
    std::array<double, 2> tipOffsetCm{{0.0, 0.0}};
    std::array<double, 2> tipOffsetRadius{{0.0, 0.0}};
    std::string chalkState;
};

struct CueImpactSupport {
    std::vector<std::string> exactlyConsumableFields;
    std::vector<std::string> unsupportedCodes;
    bool shotExecuted = false;
};

CueImpactSupport evaluateCueImpactSupport(const CueImpactInput& input);

}  // namespace billiardgl
