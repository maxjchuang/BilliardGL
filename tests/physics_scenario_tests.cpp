#include "automation_json.h"
#include "physics_scenario.h"

#include <cstdlib>
#include <cmath>
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

billiardgl::json::Value validV2Document()
{
    billiardgl::json::Value value = validDocument();
    value["schema_version"] = billiardgl::json::Value(2);
    billiardgl::json::Value cue = billiardgl::json::Value::object();
    cue["cue_ball_index"] = billiardgl::json::Value(0);
    cue["cue_speed_cm_s"] = billiardgl::json::Value(100.0);
    cue["cue_mass_kg"] = billiardgl::json::Value(0.5);
    cue["direction"] = billiardgl::json::parse("[1.0,0.0,0.0]").value;
    cue["elevation_degrees"] = billiardgl::json::Value(0.0);
    cue["tip_offset_cm"] = billiardgl::json::parse("[0.0,0.99995]").value;
    cue["tip_offset_radius"] = billiardgl::json::parse("[0.0,0.35]").value;
    cue["chalk_state"] = billiardgl::json::Value("SOURCE_DECLARED");
    value["cue_impact"] = cue;
    return value;
}

billiardgl::json::Value validV3Document()
{
    billiardgl::json::Value value = validDocument();
    value["schema_version"] = billiardgl::json::Value(3);
    const billiardgl::json::ParseResult profile = billiardgl::json::parse(R"json({
      "id": "domenech_billiard_pvc_v1",
      "formula_version": "legacy_v1",
      "ball": {
        "mass_kg": 0.205,
        "radius_cm": 3.05,
        "material": "billiard_resin"
      },
      "surface": {
        "legacy_friction_acceleration_cm_s2": 4.0,
        "sliding_friction_coefficient": 0.0,
        "rolling_resistance_acceleration_cm_s2": 4.0,
        "torsional_spin_deceleration_rad_s2": 0.0,
        "slip_speed_epsilon_cm_s": 0.0001,
        "stop_energy_threshold_j": 0.000000001,
        "material": "pvc"
      },
      "cue": {"effective_mass_kg": 0.5},
      "cushion": {
        "normal_restitution": 1.0,
        "friction_coefficient": 0.0
      },
      "solver": {
        "time_step_seconds": 0.1,
        "maximum_events_per_tick": 64
      }
    })json");
    expect(profile.ok, "v3 profile fixture should parse");
    value["physics_profile"] = profile.value;
    return value;
}

billiardgl::json::Value validV4Document()
{
    billiardgl::json::Value value = validV3Document();
    value["schema_version"] = billiardgl::json::Value(4);
    billiardgl::json::Value& cue = value["physics_profile"]["cue"];
    cue["normal_restitution"] = billiardgl::json::Value(0.0);
    cue["chalked_friction_coefficient"] = billiardgl::json::Value(0.6);
    cue["unchalked_friction_coefficient"] = billiardgl::json::Value(0.1);
    cue["maximum_reliable_offset_radius"] = billiardgl::json::Value(0.8);
    cue["cue_speed_per_power_unit_cm_s"] = billiardgl::json::Value(1.34);
    return value;
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

    const billiardgl::PhysicsScenarioResult v2 =
        billiardgl::parsePhysicsScenario(validV2Document());
    expect(v2.ok && v2.scenario.hasCueImpact, "valid cue-impact scenario v2 should parse");
    expect(v2.scenario.cueImpact.cueBallIndex == 0 &&
        std::fabs(v2.scenario.cueImpact.cueSpeedCmS - 100.0) < 0.0001,
        "cue-impact input should survive parsing exactly");
    billiardgl::GameRuntime v2Runtime;
    expect(billiardgl::applyPhysicsScenario(v2Runtime, v2.scenario).ok &&
        v2Runtime.hasCueImpactInput(), "runtime should retain requested cue input");

    const billiardgl::PhysicsScenarioResult v3 =
        billiardgl::parsePhysicsScenario(validV3Document());
    expect(v3.ok, "valid profile scenario v3 should parse");
    expect(std::fabs(v3.scenario.physicsProfile.ball.massKg - 0.205f) < 0.0001f &&
        std::fabs(v3.scenario.physicsProfile.ball.radiusCm - 3.05f) < 0.0001f,
        "v3 ball properties should survive parsing exactly");
    expect(v3.scenario.physicsProfile.surface.material == "pvc",
        "v3 surface material should survive parsing exactly");
    billiardgl::GameRuntime v3Runtime;
    expect(billiardgl::applyPhysicsScenario(v3Runtime, v3.scenario).ok,
        "v3 profile scenario should apply atomically");
    expect(v3Runtime.physicsProfile().id == "domenech_billiard_pvc_v1",
        "runtime should retain the v3 profile");
    const billiardgl::PhysicsScenarioResult v4 =
        billiardgl::parsePhysicsScenario(validV4Document());
    expect(v4.ok, "valid extended cue profile scenario v4 should parse");
    expect(std::fabs(v4.scenario.physicsProfile.cue.chalkedFrictionCoefficient - 0.6f) < 0.0001f &&
        std::fabs(v4.scenario.physicsProfile.cue.cueSpeedPerPowerUnitCmS - 1.34f) < 0.0001f,
        "v4 cue properties should survive parsing exactly");
    expect(std::fabs(v3.scenario.physicsProfile.cue.chalkedFrictionCoefficient - 0.6f) < 0.0001f,
        "v3 profile should receive extended cue compatibility defaults");
    billiardgl::json::Value v4ImpactDocument = validV4Document();
    billiardgl::json::Value centeredCue = validV2Document().at("cue_impact");
    centeredCue["tip_offset_cm"] = billiardgl::json::parse("[0.0,0.0]").value;
    centeredCue["tip_offset_radius"] = billiardgl::json::parse("[0.0,0.0]").value;
    centeredCue["chalk_state"] = billiardgl::json::Value("CHALKED");
    v4ImpactDocument["cue_impact"] = centeredCue;
    const billiardgl::PhysicsScenarioResult v4Impact =
        billiardgl::parsePhysicsScenario(v4ImpactDocument);
    billiardgl::GameRuntime v4ImpactRuntime;
    expect(v4Impact.ok && billiardgl::applyPhysicsScenario(
        v4ImpactRuntime, v4Impact.scenario).ok, "supported v4 cue impact should execute");
    expect(v4ImpactRuntime.state().balls[0].velocity.x > 0.0f &&
        v4ImpactRuntime.hasCueContactResult() &&
        v4ImpactRuntime.cueContactResult().applied,
        "v4 scenario should apply its cue contact exactly once at load");
    billiardgl::GameRuntime freshRuntime;
    expect(freshRuntime.physicsProfile().id == "chinese_pool_surface_motion_v1",
        "scenario override should not change production defaults");

    billiardgl::json::Value missingProfileField = validV3Document();
    missingProfileField["physics_profile"]["ball"].asObject().erase("mass_kg");
    expect(!billiardgl::parsePhysicsScenario(missingProfileField).ok,
        "v3 profile should require every field");
    billiardgl::json::Value negativeFriction = validV3Document();
    negativeFriction["physics_profile"]["surface"]["sliding_friction_coefficient"] =
        billiardgl::json::Value(-0.1);
    expect(!billiardgl::parsePhysicsScenario(negativeFriction).ok,
        "v3 profile should reject invalid physical values");
    billiardgl::json::Value extraProfileField = validV3Document();
    extraProfileField["physics_profile"]["ball"]["unknown"] =
        billiardgl::json::Value(1);
    expect(!billiardgl::parsePhysicsScenario(extraProfileField).ok,
        "v3 profile should reject unknown fields");
    billiardgl::json::Value mismatchedProfileStep = validV3Document();
    mismatchedProfileStep["physics_profile"]["solver"]["time_step_seconds"] =
        billiardgl::json::Value(0.05);
    expect(!billiardgl::parsePhysicsScenario(mismatchedProfileStep).ok,
        "profile and scenario time steps should agree");

    billiardgl::json::Value inconsistent = validV2Document();
    inconsistent["cue_impact"]["tip_offset_radius"].asArray()[1] = billiardgl::json::Value(0.2);
    expect(!billiardgl::parsePhysicsScenario(inconsistent).ok,
        "physical and dimensionless offsets must agree");
    billiardgl::json::Value nonUnit = validV2Document();
    nonUnit["cue_impact"]["direction"].asArray()[0] = billiardgl::json::Value(2.0);
    expect(!billiardgl::parsePhysicsScenario(nonUnit).ok, "cue direction must be unit length");
    billiardgl::json::Value nonPlanarDirection = validV2Document();
    nonPlanarDirection["cue_impact"]["direction"] =
        billiardgl::json::parse("[0.8,0.6,0.0]").value;
    expect(!billiardgl::parsePhysicsScenario(nonPlanarDirection).ok,
        "cue direction is a table-plane heading; elevation is declared separately");
    billiardgl::json::Value unknownVersion = validDocument();
    unknownVersion["schema_version"] = billiardgl::json::Value(5);
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
