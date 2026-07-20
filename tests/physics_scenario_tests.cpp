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

billiardgl::json::Value validV5Document()
{
    billiardgl::json::Value value = validV4Document();
    value["schema_version"] = billiardgl::json::Value(5);
    billiardgl::json::Value& ball = value["physics_profile"]["ball"];
    ball["inertia_factor"] = billiardgl::json::Value(0.4);
    ball["normal_restitution"] = billiardgl::json::Value(0.91);
    ball["friction_coefficient"] = billiardgl::json::Value(0.08);
    return value;
}

billiardgl::json::Value validV6Document()
{
    billiardgl::json::Value value = validV5Document();
    value["schema_version"] = billiardgl::json::Value(6);
    billiardgl::json::Value& cushion = value["physics_profile"]["cushion"];
    cushion["nose_height_ratio"] = billiardgl::json::Value(1.4);
    cushion["maximum_rigid_incident_speed_cm_s"] = billiardgl::json::Value(250.0);
    cushion["material"] = billiardgl::json::Value("riley_snooker_cushion");
    return value;
}

billiardgl::json::Value validV7Document()
{
    billiardgl::json::Value value = validV6Document();
    value["schema_version"] = billiardgl::json::Value(7);
    billiardgl::json::Value boundary = billiardgl::json::Value::object();
    boundary["playfield_width_cm"] = billiardgl::json::Value(127.0);
    boundary["playfield_length_cm"] = billiardgl::json::Value(254.0);
    boundary["corner_mouth_width_cm"] = billiardgl::json::Value(13.2);
    boundary["side_mouth_width_cm"] = billiardgl::json::Value(8.6);
    boundary["corner_throat_width_cm"] = billiardgl::json::Value(10.0);
    boundary["side_throat_width_cm"] = billiardgl::json::Value(7.0);
    boundary["jaw_radius_cm"] = billiardgl::json::Value(2.5);
    boundary["throat_depth_cm"] = billiardgl::json::Value(3.0);
    boundary["capture_depth_cm"] = billiardgl::json::Value(6.0);
    boundary["geometry_id"] = billiardgl::json::Value("wpa_chinese_pool_v1");
    boundary["material"] = billiardgl::json::Value("competition_table_boundary");
    value["physics_profile"]["table_boundary"] = boundary;
    return value;
}

billiardgl::json::Value validV8Document()
{
    billiardgl::json::Value value = validV7Document();
    value["schema_version"] = billiardgl::json::Value(8);
    billiardgl::json::Value& solver = value["physics_profile"]["solver"];
    solver["toi_tolerance_seconds"] = billiardgl::json::Value(0.0000001);
    solver["maximum_island_size"] = billiardgl::json::Value(16);
    solver["velocity_iterations"] = billiardgl::json::Value(12);
    solver["position_iterations"] = billiardgl::json::Value(4);
    solver["penetration_slop_cm"] = billiardgl::json::Value(0.001);
    solver["maximum_penetration_cm"] = billiardgl::json::Value(0.5);
    solver["residual_tolerance_cm_s"] = billiardgl::json::Value(0.001);
    return value;
}

billiardgl::json::Value validV9Document(const char* boundaryMode = "unbounded")
{
    billiardgl::json::Value value = validV8Document();
    value["schema_version"] = billiardgl::json::Value(9);
    value["boundary_mode"] = billiardgl::json::Value(boundaryMode);
    return value;
}

billiardgl::json::Value validV10Document()
{
    billiardgl::json::Value value = validV9Document();
    value["schema_version"] = billiardgl::json::Value(10);
    value["physics_profile"]["solver"]["passive_energy_tolerance_j"] =
        billiardgl::json::Value(0.0000000001);
    return value;
}

billiardgl::json::Value validV11Document()
{
    billiardgl::json::Value value = validV10Document();
    value["schema_version"] = billiardgl::json::Value(11);
    billiardgl::json::Value& cushion = value["physics_profile"]["cushion"];
    cushion["restitution_intercept"] = billiardgl::json::Value(1.0);
    cushion["restitution_slope_per_mps"] = billiardgl::json::Value(0.056);
    cushion["minimum_restitution"] = billiardgl::json::Value(0.0);
    cushion["maximum_restitution"] = billiardgl::json::Value(0.93);
    return value;
}

billiardgl::json::Value validV12Document()
{
    billiardgl::json::Value value = validV11Document();
    value["schema_version"] = billiardgl::json::Value(12);
    billiardgl::json::Value contact = billiardgl::json::Value::object();
    contact["enabled"] = billiardgl::json::Value(true);
    contact["normal_stiffness_n_per_m32"] =
        billiardgl::json::Value(12500000.0);
    contact["normal_dissipation_s_per_m"] = billiardgl::json::Value(0.05);
    contact["tangential_stiffness_n_per_m"] =
        billiardgl::json::Value(400000.0);
    contact["tangential_damping_ns_per_m"] = billiardgl::json::Value(25.0);
    contact["microstep_seconds"] = billiardgl::json::Value(0.00001);
    contact["maximum_contact_seconds"] = billiardgl::json::Value(0.006);
    contact["release_compression_m"] = billiardgl::json::Value(0.00000001);
    contact["maximum_compression_m"] = billiardgl::json::Value(0.004);
    contact["maximum_normal_force_n"] = billiardgl::json::Value(10000.0);
    value["physics_profile"]["frozen_cue_contact"] = contact;
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

    const billiardgl::PhysicsScenarioResult v8 =
        billiardgl::parsePhysicsScenario(validV8Document());
    expect(v8.ok && v8.scenario.physicsProfile.solver.maximumIslandSize == 16 &&
        v8.scenario.physicsProfile.solver.velocityIterations == 12 &&
        std::fabs(v8.scenario.physicsProfile.solver.maximumPenetrationCm - 0.5f) < 0.0001f,
        "v8 deterministic solver controls should survive parsing");
    billiardgl::json::Value invalidV8 = validV8Document();
    invalidV8["physics_profile"]["solver"]["maximum_island_size"] =
        billiardgl::json::Value(0);
    expect(!billiardgl::parsePhysicsScenario(invalidV8).ok,
        "v8 should reject an empty island limit");

    const billiardgl::PhysicsScenarioResult v9 =
        billiardgl::parsePhysicsScenario(validV9Document());
    expect(v9.ok &&
        v9.scenario.boundaryMode == billiardgl::PhysicsBoundaryMode::Unbounded,
        "v9 unbounded boundary mode should survive parsing");
    const billiardgl::PhysicsScenarioResult v10 =
        billiardgl::parsePhysicsScenario(validV10Document());
    expect(v10.ok &&
        v10.scenario.physicsProfile.solver.passiveEnergyToleranceJ == 1e-10,
        "v10 passive-energy tolerance should survive parsing");
    billiardgl::json::Value epsilonDocument = validV9Document();
    epsilonDocument["initial_contact_epsilon_cm"] =
        billiardgl::json::Value(0.02);
    const billiardgl::PhysicsScenarioResult epsilonScenario =
        billiardgl::parsePhysicsScenario(epsilonDocument);
    expect(epsilonScenario.ok &&
        std::fabs(epsilonScenario.scenario.initialContactEpsilonCm - 0.02f) <
            0.000001f,
        "v9 should retain an explicit initial-contact epsilon");
    epsilonDocument["initial_contact_epsilon_cm"] =
        billiardgl::json::Value(-0.01);
    expect(!billiardgl::parsePhysicsScenario(epsilonDocument).ok,
        "negative initial-contact epsilon should fail closed");
    billiardgl::json::Value missingBoundary = validV9Document();
    missingBoundary.asObject().erase("boundary_mode");
    const billiardgl::PhysicsScenarioResult defaultBoundary =
        billiardgl::parsePhysicsScenario(missingBoundary);
    expect(defaultBoundary.ok && defaultBoundary.scenario.boundaryMode ==
        billiardgl::PhysicsBoundaryMode::ProductionTable,
        "missing boundary mode should retain production-table behavior");
    const billiardgl::PhysicsScenarioResult invalidBoundary =
        billiardgl::parsePhysicsScenario(validV9Document("periodic"));
    expect(!invalidBoundary.ok &&
        invalidBoundary.errorCode == "INVALID_BOUNDARY_MODE",
        "unknown boundary mode should fail closed with a stable code");
    billiardgl::GameRuntime v9Runtime;
    v9Runtime.setPhysicsTraceEnabled(true);
    expect(billiardgl::applyPhysicsScenario(v9Runtime, v9.scenario).ok &&
        v9Runtime.boundaryMode() == billiardgl::PhysicsBoundaryMode::Unbounded &&
        v9Runtime.step(1).ok &&
        v9Runtime.physicsTrace().frames().front().boundaryMode ==
            billiardgl::PhysicsBoundaryMode::Unbounded,
        "applied boundary identity should reach every runtime trace frame");

    billiardgl::PhysicsScenario openBench = v9.scenario;
    const float xLimit =
        openBench.physicsProfile.tableBoundary.playfieldWidthCm * 0.5f -
        openBench.physicsProfile.ball.radiusCm;
    openBench.balls[0].position.x = xLimit;
    openBench.balls[0].velocity = billiardgl::Point3{100.0f, 0.0f, 0.0f};
    openBench.balls[0].speed = 100.0f;
    billiardgl::GameRuntime openBenchRuntime;
    openBenchRuntime.setPhysicsTraceEnabled(true);
    expect(billiardgl::applyPhysicsScenario(openBenchRuntime, openBench).ok &&
        openBenchRuntime.step(1).ok &&
        openBenchRuntime.state().balls[0].position.x > xLimit &&
        openBenchRuntime.physicsTrace().frames()[0].contacts.empty(),
        "unbounded apparatus should not generate rail or pocket contacts");

    billiardgl::json::Value substepDocument = validV9Document();
    substepDocument["simulation"]["time_step_seconds"] =
        billiardgl::json::Value(0.001);
    substepDocument["physics_profile"]["solver"]["time_step_seconds"] =
        billiardgl::json::Value(0.001);
    expect(billiardgl::parsePhysicsScenario(substepDocument).ok,
        "v9 apparatus should support an explicit positive physics substep");

    billiardgl::GameRuntime runtime;
    runtime.setPhysicsTraceEnabled(true);
    expect(billiardgl::applyPhysicsScenario(runtime, parsed.scenario).ok,
        "valid scenario should apply atomically");
    expect(runtime.physicsTrace().frames().empty(), "scenario load should clear old trace");
    runtime.step(parsed.scenario.ticks);
    expect(runtime.physicsTrace().frames().size() == 10,
        "direct runtime path should execute canonical fixture");

    const billiardgl::Point3 stateBeforeRejectedScenario =
        runtime.state().balls[0].position;
    billiardgl::PhysicsScenario overlappingScenario = parsed.scenario;
    overlappingScenario.balls[1] = overlappingScenario.balls[0];
    overlappingScenario.balls[1].pocketed = false;
    const billiardgl::ActionResult rejectedGeometry =
        billiardgl::applyPhysicsScenario(runtime, overlappingScenario);
    expect(!rejectedGeometry.ok &&
        std::string(rejectedGeometry.errorCode) == "INTEGRATION_MISMATCH" &&
        runtime.tick() == 10 &&
        runtime.state().balls[0].position.x == stateBeforeRejectedScenario.x &&
        runtime.state().balls[0].position.z == stateBeforeRejectedScenario.z,
        "invalid geometry should fail before mutating runtime state");

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
    expect(std::fabs(v3.scenario.physicsProfile.ball.inertiaFactor - 0.4f) < 0.0001f &&
        std::fabs(v3.scenario.physicsProfile.ball.normalRestitution - 1.0f) < 0.0001f &&
        std::fabs(v3.scenario.physicsProfile.ball.frictionCoefficient) < 0.0001f,
        "v3 profile should receive ball-contact compatibility defaults");
    const billiardgl::PhysicsScenarioResult v5 =
        billiardgl::parsePhysicsScenario(validV5Document());
    expect(v5.ok, "valid ball-contact profile scenario v5 should parse");
    expect(std::fabs(v5.scenario.physicsProfile.ball.inertiaFactor - 0.4f) < 0.0001f &&
        std::fabs(v5.scenario.physicsProfile.ball.normalRestitution - 0.91f) < 0.0001f &&
        std::fabs(v5.scenario.physicsProfile.ball.frictionCoefficient - 0.08f) < 0.0001f,
        "v5 ball contact properties should survive parsing exactly");
    expect(std::fabs(v5.scenario.physicsProfile.cushion.noseHeightRatio - 1.0f) < 0.0001f &&
        v5.scenario.physicsProfile.cushion.material == "legacy_rigid_rail",
        "v5 profile should receive cushion compatibility defaults");
    const billiardgl::PhysicsScenarioResult v6 =
        billiardgl::parsePhysicsScenario(validV6Document());
    expect(v6.ok, "valid cushion profile scenario v6 should parse");
    expect(std::fabs(v6.scenario.physicsProfile.cushion.noseHeightRatio - 1.4f) < 0.0001f &&
        std::fabs(v6.scenario.physicsProfile.cushion.maximumRigidIncidentSpeedCmS - 250.0f) < 0.0001f &&
        v6.scenario.physicsProfile.cushion.material == "riley_snooker_cushion",
        "v6 cushion properties should survive parsing exactly");
    expect(std::fabs(v6.scenario.physicsProfile.tableBoundary.jawRadiusCm - 0.0001f) < 0.00001f &&
        v6.scenario.physicsProfile.tableBoundary.geometryId == "legacy_opening_band",
        "v6 profile should receive historical pocket-boundary defaults");
    const billiardgl::PhysicsScenarioResult v7 =
        billiardgl::parsePhysicsScenario(validV7Document());
    expect(v7.ok, "valid pocket-boundary profile scenario v7 should parse");
    expect(std::fabs(v7.scenario.physicsProfile.tableBoundary.cornerThroatWidthCm - 10.0f) < 0.0001f &&
        std::fabs(v7.scenario.physicsProfile.tableBoundary.captureDepthCm - 6.0f) < 0.0001f &&
        v7.scenario.physicsProfile.tableBoundary.geometryId == "wpa_chinese_pool_v1",
        "v7 pocket-boundary properties should survive parsing exactly");
    billiardgl::json::Value invalidThroat = validV7Document();
    invalidThroat["physics_profile"]["table_boundary"]["corner_throat_width_cm"] =
        billiardgl::json::Value(14.0);
    expect(!billiardgl::parsePhysicsScenario(invalidThroat).ok,
        "v7 should reject a throat wider than its mouth");
    billiardgl::json::Value sourceApparatus = validV5Document();
    sourceApparatus["evidence"]["equipment"] =
        billiardgl::json::Value("SOURCE_LABORATORY_APPARATUS");
    expect(billiardgl::parsePhysicsScenario(sourceApparatus).ok,
        "v5 should explicitly admit source laboratory apparatus profiles");
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
    expect(freshRuntime.physicsProfile().id == "chinese_pool_legacy_v1",
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
    billiardgl::json::Value invalidInertia = validV5Document();
    invalidInertia["physics_profile"]["ball"]["inertia_factor"] =
        billiardgl::json::Value(0.0);
    expect(!billiardgl::parsePhysicsScenario(invalidInertia).ok,
        "v5 profile should reject nonpositive inertia");
    billiardgl::json::Value invalidRestitution = validV5Document();
    invalidRestitution["physics_profile"]["ball"]["normal_restitution"] =
        billiardgl::json::Value(1.1);
    expect(!billiardgl::parsePhysicsScenario(invalidRestitution).ok,
        "v5 profile should reject restitution above one");
    billiardgl::json::Value invalidContactFriction = validV5Document();
    invalidContactFriction["physics_profile"]["ball"]["friction_coefficient"] =
        billiardgl::json::Value(-0.1);
    expect(!billiardgl::parsePhysicsScenario(invalidContactFriction).ok,
        "v5 profile should reject negative contact friction");
    billiardgl::json::Value missingV5Field = validV5Document();
    missingV5Field["physics_profile"]["ball"].asObject().erase("inertia_factor");
    expect(!billiardgl::parsePhysicsScenario(missingV5Field).ok,
        "v5 profile should require every ball contact field");
    billiardgl::json::Value invalidNoseHeight = validV6Document();
    invalidNoseHeight["physics_profile"]["cushion"]["nose_height_ratio"] =
        billiardgl::json::Value(2.0);
    expect(!billiardgl::parsePhysicsScenario(invalidNoseHeight).ok,
        "v6 profile should reject cushion nose height outside the ball");
    billiardgl::json::Value invalidRigidSpeed = validV6Document();
    invalidRigidSpeed["physics_profile"]["cushion"]["maximum_rigid_incident_speed_cm_s"] =
        billiardgl::json::Value(0.0);
    expect(!billiardgl::parsePhysicsScenario(invalidRigidSpeed).ok,
        "v6 profile should reject a nonpositive rigid speed domain");
    billiardgl::json::Value missingCushionMaterial = validV6Document();
    missingCushionMaterial["physics_profile"]["cushion"].asObject().erase("material");
    expect(!billiardgl::parsePhysicsScenario(missingCushionMaterial).ok,
        "v6 profile should require every cushion field");
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
    const billiardgl::PhysicsScenarioResult v11 =
        billiardgl::parsePhysicsScenario(validV11Document());
    expect(v11.ok && std::fabs(
        v11.scenario.physicsProfile.cushion.restitutionSlopePerMps - 0.056f) < 1e-6f,
        "v11 should parse the bounded affine cushion law");
    const billiardgl::PhysicsScenarioResult v12 =
        billiardgl::parsePhysicsScenario(validV12Document());
    expect(v12.ok && v12.scenario.physicsProfile.frozenCueContact.enabled &&
        std::fabs(v12.scenario.physicsProfile.frozenCueContact.
            normalStiffnessNPerM32 - 12500000.0) < 0.001,
        "v12 should parse the complete frozen cue-contact contract");
    billiardgl::json::Value missingV12Field = validV12Document();
    missingV12Field["physics_profile"]["frozen_cue_contact"].
        asObject().erase("maximum_normal_force_n");
    expect(!billiardgl::parsePhysicsScenario(missingV12Field).ok,
        "v12 should reject a missing frozen cue-contact field");
    billiardgl::json::Value extraV12Field = validV12Document();
    extraV12Field["physics_profile"]["frozen_cue_contact"]["unknown"] =
        billiardgl::json::Value(1.0);
    expect(!billiardgl::parsePhysicsScenario(extraV12Field).ok,
        "v12 should reject an unknown frozen cue-contact field");
    billiardgl::json::Value unknownVersion = validDocument();
    unknownVersion["schema_version"] = billiardgl::json::Value(13);
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
