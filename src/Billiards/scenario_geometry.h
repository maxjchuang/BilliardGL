#pragma once

#include "physics_scenario.h"

#include <string>

namespace billiardgl {

struct ScenarioGeometryResult {
    ScenarioGeometryResult(bool success = false,
        const char* errorCode = "", int first = -1, int second = -1)
        : ok(success), code(errorCode), firstBall(first), secondBall(second) {}

    bool ok;
    std::string code;
    int firstBall;
    int secondBall;
};

ScenarioGeometryResult validateScenarioGeometry(
    const PhysicsScenario& scenario);

}  // namespace billiardgl
