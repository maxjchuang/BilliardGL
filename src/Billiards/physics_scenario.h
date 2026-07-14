#pragma once

#include "automation_json.h"
#include "game_runtime.h"

#include <array>
#include <string>
#include <vector>

namespace billiardgl {

constexpr int kPhysicsScenarioVersion = 4;

struct PhysicsExpectation {
    std::string metric;
    std::string comparison;
    json::Value value;
    double absoluteTolerance = 0.0;
    double relativeTolerance = 0.0;
};

struct PhysicsScenario {
    int schemaVersion = kPhysicsScenarioVersion;
    std::string id;
    std::string description;
    std::string evidenceGrade;
    std::string evidenceSource;
    std::string equipment;
    int ticks = 0;
    float timeStepSeconds = kDefaultTimeStep;
    std::array<BallState, kBallCount> balls;
    std::vector<PhysicsExpectation> expectations;
    bool hasCueImpact = false;
    CueImpactInput cueImpact;
    PhysicsProfile physicsProfile = defaultChinesePoolPhysicsProfile();
};

struct PhysicsScenarioResult {
    bool ok = false;
    PhysicsScenario scenario;
    std::string errorCode;
    std::string errorMessage;
};

PhysicsScenarioResult parsePhysicsScenario(const json::Value& value);
ActionResult applyPhysicsScenario(GameRuntime& runtime, const PhysicsScenario& scenario);

}  // namespace billiardgl
