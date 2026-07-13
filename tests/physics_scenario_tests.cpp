#include "automation_json.h"
#include "physics_scenario.h"

#include <cstdlib>
#include <fstream>
#include <iostream>
#include <sstream>

namespace {

void expect(bool value, const char* message)
{
    if (!value) {
        std::cerr << message << '\n';
        std::exit(1);
    }
}

std::string readFile(const std::string& path)
{
    std::ifstream input(path.c_str());
    std::ostringstream contents;
    contents << input.rdbuf();
    return contents.str();
}

billiardgl::json::Value validDocument()
{
    const std::string path = std::string(BILLIARDGL_SOURCE_ROOT) +
        "/tests/physics_validation/scenarios/free_roll_v1.json";
    const billiardgl::json::ParseResult parsed = billiardgl::json::parse(readFile(path));
    expect(parsed.ok, "fixture JSON should parse");
    return parsed.value;
}

}  // namespace

int main()
{
    const billiardgl::PhysicsScenarioResult parsed =
        billiardgl::parsePhysicsScenario(validDocument());
    expect(parsed.ok, "valid canonical scenario should parse");
    expect(parsed.scenario.id == "free_roll_v1" && parsed.scenario.ticks == 10,
        "scenario metadata should survive parsing");
    expect(!parsed.scenario.balls[0].pocketed, "listed ball should remain active");
    expect(parsed.scenario.balls[1].pocketed, "omitted ball should become pocketed");
    expect(parsed.scenario.expectations.size() == 2, "expectations should parse");
    expect(parsed.scenario.expectations[0].comparison == "eq", "operator maps to comparison");

    billiardgl::GameRuntime runtime;
    runtime.setPhysicsTraceEnabled(true);
    expect(billiardgl::applyPhysicsScenario(runtime, parsed.scenario).ok,
        "valid scenario should apply atomically");
    expect(runtime.physicsTrace().frames().empty(), "scenario load should clear old trace");
    runtime.step(parsed.scenario.ticks);
    expect(runtime.physicsTrace().frames().size() == 10,
        "direct runtime path should execute canonical fixture");

    billiardgl::json::Value unknownVersion = validDocument();
    unknownVersion["schema_version"] = billiardgl::json::Value(2);
    const billiardgl::PhysicsScenarioResult versionResult =
        billiardgl::parsePhysicsScenario(unknownVersion);
    expect(!versionResult.ok && versionResult.errorCode == "unsupported_scenario_version",
        "unknown schema version should have a stable error");

    billiardgl::json::Value duplicate = validDocument();
    duplicate["balls"].asArray().push_back(duplicate.at("balls").asArray()[0]);
    expect(!billiardgl::parsePhysicsScenario(duplicate).ok,
        "duplicate ball indices should fail");

    billiardgl::json::Value badEvidence = validDocument();
    badEvidence["evidence"]["grade"] = billiardgl::json::Value("D");
    expect(!billiardgl::parsePhysicsScenario(badEvidence).ok,
        "unknown evidence grade should fail");

    billiardgl::json::Value badOperator = validDocument();
    badOperator["expectations"].asArray()[0]["operator"] =
        billiardgl::json::Value("approximately");
    expect(!billiardgl::parsePhysicsScenario(badOperator).ok,
        "unknown comparison operator should fail");

    billiardgl::json::Value badTimeStep = validDocument();
    badTimeStep["simulation"]["time_step_seconds"] = billiardgl::json::Value(0.01);
    const billiardgl::PhysicsScenarioResult timeResult =
        billiardgl::parsePhysicsScenario(badTimeStep);
    expect(!timeResult.ok && timeResult.errorCode == "unsupported_time_step",
        "runtime must reject a time step it cannot execute");
    return 0;
}
