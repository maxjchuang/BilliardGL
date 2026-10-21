#pragma once

#include "automation_json.h"
#include "physics_scenario.h"

#include <array>
#include <cstdint>
#include <set>
#include <string>
#include <vector>

namespace billiardgl::training {

constexpr int kTrainingScenarioVersion = 1;

struct Scenario {
    int schemaVersion = kTrainingScenarioVersion;
    std::string id;
    int version = 1;
    std::string category;
    std::string scenarioSha256;
    PhysicsScenario initialScenario;
    json::Value success = json::Value::array();
    json::Value failure = json::Value::array();
    json::Value scoring = json::Value::object();
    std::vector<std::string> requiredCapabilities;
    bool scoreEnabled = true;
};

struct LoadResult {
    bool ok = false;
    Scenario scenario;
    std::string errorCode;
    std::string errorMessage;
    std::vector<std::string> missingCapabilities;
};

struct AttemptFacts {
    GameState state;
    int shotCount = 0;
    std::uint64_t elapsedTicks = 0;
    bool numericalFailure = false;
    int firstContactBall = -1;
    bool hasCueBallPositionAtFirstContact = false;
    Point3 cueBallPositionAtFirstContactCm;
    std::array<int, kBallCount> railContacts{};
    int distinctObjectBallsWithRailContact = 0;
    std::vector<int> pocketOrder;
    std::array<int, kBallCount> pocketIds = [] {
        std::array<int, kBallCount> result{};
        result.fill(-1);
        return result;
    }();
    std::string traceSha256;
};

struct Evaluation {
    bool succeeded = false;
    bool failed = false;
    bool scoreEligible = false;
    int score = 0;
    std::string reason;
    std::string feedback;
};

LoadResult parseScenario(const json::Value& value,
    const std::set<std::string>& capabilities);
LoadResult loadScenarioFile(const std::string& path,
    const std::set<std::string>& capabilities);
Evaluation evaluate(const Scenario& scenario, const AttemptFacts& facts);

}  // namespace billiardgl::training
