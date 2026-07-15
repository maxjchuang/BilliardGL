#include "physics_scenario.h"
#include "scenario_geometry.h"
#include "table_specs.h"

#include <cmath>
#include <exception>
#include <initializer_list>
#include <set>
#include <stdexcept>

namespace billiardgl {
namespace {

const json::Value& required(const json::Value& object, const char* name)
{
    if (!object.isObject() || !object.has(name)) {
        throw std::runtime_error(std::string(name) + " is required");
    }
    return object.at(name);
}

std::string requiredString(const json::Value& object, const char* name)
{
    const json::Value& value = required(object, name);
    if (!value.isString() || value.asString().empty()) {
        throw std::runtime_error(std::string(name) + " must be a nonempty string");
    }
    return value.asString();
}

double requiredNumber(const json::Value& object, const char* name)
{
    const json::Value& value = required(object, name);
    if (!value.isNumber() || !std::isfinite(value.asNumber())) {
        throw std::runtime_error(std::string(name) + " must be finite");
    }
    return value.asNumber();
}

Point3 pointArray(const json::Value& object, const char* name)
{
    const json::Value& value = required(object, name);
    if (!value.isArray() || value.asArray().size() != 3) {
        throw std::runtime_error(std::string(name) + " must contain three numbers");
    }
    Point3 point;
    float* components[] = {&point.x, &point.y, &point.z};
    for (std::size_t index = 0; index < 3; ++index) {
        const json::Value& component = value.asArray()[index];
        if (!component.isNumber() || !std::isfinite(component.asNumber())) {
            throw std::runtime_error(std::string(name) + " must contain finite numbers");
        }
        *components[index] = static_cast<float>(component.asNumber());
    }
    return point;
}

bool supportedComparison(const std::string& value)
{
    return value == "eq" || value == "lte" || value == "gte";
}

std::array<double, 2> numberPair(const json::Value& object, const char* name)
{
    const json::Value& value = required(object, name);
    if (!value.isArray() || value.asArray().size() != 2) {
        throw std::runtime_error(std::string(name) + " must contain two numbers");
    }
    std::array<double, 2> result;
    for (std::size_t index = 0; index < 2; ++index) {
        if (!value.asArray()[index].isNumber() ||
            !std::isfinite(value.asArray()[index].asNumber())) {
            throw std::runtime_error(std::string(name) + " must contain finite numbers");
        }
        result[index] = value.asArray()[index].asNumber();
    }
    return result;
}

std::array<double, 3> numberTriple(const json::Value& object, const char* name)
{
    const json::Value& value = required(object, name);
    if (!value.isArray() || value.asArray().size() != 3) {
        throw std::runtime_error(std::string(name) + " must contain three numbers");
    }
    std::array<double, 3> result;
    for (std::size_t index = 0; index < 3; ++index) {
        if (!value.asArray()[index].isNumber() ||
            !std::isfinite(value.asArray()[index].asNumber())) {
            throw std::runtime_error(std::string(name) + " must contain finite numbers");
        }
        result[index] = value.asArray()[index].asNumber();
    }
    return result;
}

double optionalTolerance(const json::Value& value, const char* name)
{
    if (!value.has(name)) return 0.0;
    const double tolerance = requiredNumber(value, name);
    if (tolerance < 0.0) throw std::runtime_error(std::string(name) + " must be nonnegative");
    return tolerance;
}

void requireExactKeys(const json::Value& value,
    std::initializer_list<const char*> expected, const char* name)
{
    if (!value.isObject()) {
        throw std::runtime_error(std::string(name) + " must be an object");
    }
    std::set<std::string> keys;
    for (const char* key : expected) keys.insert(key);
    if (value.asObject().size() != keys.size()) {
        throw std::runtime_error(std::string(name) + " keys do not match schema");
    }
    for (const auto& item : value.asObject()) {
        if (keys.count(item.first) == 0) {
            throw std::runtime_error(
                std::string(name) + " has unknown field " + item.first);
        }
    }
    for (const std::string& key : keys) {
        if (!value.has(key)) {
            throw std::runtime_error(
                std::string(name) + " is missing field " + key);
        }
    }
}

PhysicsProfile parsePhysicsProfile(const json::Value& value, int scenarioVersion)
{
    if (scenarioVersion >= 12) {
        requireExactKeys(value,
            {"id", "formula_version", "ball", "surface", "cue", "cushion",
             "table_boundary", "solver", "frozen_cue_contact"},
            "physics_profile");
    } else if (scenarioVersion >= 7) {
        requireExactKeys(value,
            {"id", "formula_version", "ball", "surface", "cue", "cushion",
             "table_boundary", "solver"}, "physics_profile");
    } else {
        requireExactKeys(value,
            {"id", "formula_version", "ball", "surface", "cue", "cushion", "solver"},
            "physics_profile");
    }
    const json::Value& ball = required(value, "ball");
    const json::Value& surface = required(value, "surface");
    const json::Value& cue = required(value, "cue");
    const json::Value& cushion = required(value, "cushion");
    const json::Value& solver = required(value, "solver");
    const json::Value* frozenCueContact = scenarioVersion >= 12 ?
        &required(value, "frozen_cue_contact") : nullptr;
    const json::Value* tableBoundary = scenarioVersion >= 7 ?
        &required(value, "table_boundary") : nullptr;
    if (scenarioVersion >= 5) {
        requireExactKeys(ball,
            {"mass_kg", "radius_cm", "inertia_factor", "normal_restitution",
             "friction_coefficient", "material"},
            "physics_profile.ball");
    } else {
        requireExactKeys(ball, {"mass_kg", "radius_cm", "material"},
            "physics_profile.ball");
    }
    requireExactKeys(surface,
        {"legacy_friction_acceleration_cm_s2", "sliding_friction_coefficient",
         "rolling_resistance_acceleration_cm_s2", "torsional_spin_deceleration_rad_s2",
         "slip_speed_epsilon_cm_s", "stop_energy_threshold_j", "material"},
        "physics_profile.surface");
    if (scenarioVersion >= 4) {
        requireExactKeys(cue,
            {"effective_mass_kg", "normal_restitution",
             "chalked_friction_coefficient", "unchalked_friction_coefficient",
             "maximum_reliable_offset_radius", "cue_speed_per_power_unit_cm_s"},
            "physics_profile.cue");
    } else {
        requireExactKeys(cue, {"effective_mass_kg"}, "physics_profile.cue");
    }
    if (scenarioVersion >= 11) {
        requireExactKeys(cushion,
            {"normal_restitution", "restitution_intercept",
             "restitution_slope_per_mps", "minimum_restitution",
             "maximum_restitution", "friction_coefficient", "nose_height_ratio",
             "maximum_rigid_incident_speed_cm_s", "material"},
            "physics_profile.cushion");
    } else if (scenarioVersion >= 6) {
        requireExactKeys(cushion,
            {"normal_restitution", "friction_coefficient", "nose_height_ratio",
             "maximum_rigid_incident_speed_cm_s", "material"},
            "physics_profile.cushion");
    } else {
        requireExactKeys(cushion, {"normal_restitution", "friction_coefficient"},
            "physics_profile.cushion");
    }
    if (scenarioVersion >= 10) {
        requireExactKeys(solver,
            {"time_step_seconds", "maximum_events_per_tick",
             "toi_tolerance_seconds", "maximum_island_size",
             "velocity_iterations", "position_iterations",
             "penetration_slop_cm", "maximum_penetration_cm",
             "residual_tolerance_cm_s", "passive_energy_tolerance_j"},
            "physics_profile.solver");
    } else if (scenarioVersion >= 8) {
        requireExactKeys(solver,
            {"time_step_seconds", "maximum_events_per_tick",
             "toi_tolerance_seconds", "maximum_island_size",
             "velocity_iterations", "position_iterations",
             "penetration_slop_cm", "maximum_penetration_cm",
             "residual_tolerance_cm_s"}, "physics_profile.solver");
    } else {
        requireExactKeys(solver, {"time_step_seconds", "maximum_events_per_tick"},
            "physics_profile.solver");
    }
    if (tableBoundary != nullptr) {
        requireExactKeys(*tableBoundary,
            {"playfield_width_cm", "playfield_length_cm",
             "corner_mouth_width_cm", "side_mouth_width_cm",
             "corner_throat_width_cm", "side_throat_width_cm",
             "jaw_radius_cm", "throat_depth_cm", "capture_depth_cm",
             "geometry_id", "material"}, "physics_profile.table_boundary");
    }
    if (frozenCueContact != nullptr) {
        requireExactKeys(*frozenCueContact,
            {"enabled", "normal_stiffness_n_per_m32",
             "normal_dissipation_s_per_m", "tangential_stiffness_n_per_m",
             "tangential_damping_ns_per_m", "microstep_seconds",
             "maximum_contact_seconds", "release_compression_m",
             "maximum_compression_m", "maximum_normal_force_n"},
            "physics_profile.frozen_cue_contact");
        if (!required(*frozenCueContact, "enabled").isBool()) {
            throw std::runtime_error(
                "physics_profile.frozen_cue_contact.enabled must be boolean");
        }
    }

    PhysicsProfile profile;
    profile.id = requiredString(value, "id");
    profile.formulaVersion = requiredString(value, "formula_version");
    profile.ball.massKg = static_cast<float>(requiredNumber(ball, "mass_kg"));
    profile.ball.radiusCm = static_cast<float>(requiredNumber(ball, "radius_cm"));
    profile.ball.material = requiredString(ball, "material");
    if (scenarioVersion >= 5) {
        profile.ball.inertiaFactor = static_cast<float>(
            requiredNumber(ball, "inertia_factor"));
        profile.ball.normalRestitution = static_cast<float>(
            requiredNumber(ball, "normal_restitution"));
        profile.ball.frictionCoefficient = static_cast<float>(
            requiredNumber(ball, "friction_coefficient"));
    }
    profile.surface.legacyFrictionAccelerationCmS2 = static_cast<float>(
        requiredNumber(surface, "legacy_friction_acceleration_cm_s2"));
    profile.surface.slidingFrictionCoefficient = static_cast<float>(
        requiredNumber(surface, "sliding_friction_coefficient"));
    profile.surface.rollingResistanceAccelerationCmS2 = static_cast<float>(
        requiredNumber(surface, "rolling_resistance_acceleration_cm_s2"));
    profile.surface.torsionalSpinDecelerationRadS2 = static_cast<float>(
        requiredNumber(surface, "torsional_spin_deceleration_rad_s2"));
    profile.surface.slipSpeedEpsilonCmS = static_cast<float>(
        requiredNumber(surface, "slip_speed_epsilon_cm_s"));
    profile.surface.stopEnergyThresholdJ = static_cast<float>(
        requiredNumber(surface, "stop_energy_threshold_j"));
    profile.surface.material = requiredString(surface, "material");
    profile.cue.effectiveMassKg = static_cast<float>(
        requiredNumber(cue, "effective_mass_kg"));
    if (scenarioVersion >= 4) {
        profile.cue.normalRestitution = static_cast<float>(
            requiredNumber(cue, "normal_restitution"));
        profile.cue.chalkedFrictionCoefficient = static_cast<float>(
            requiredNumber(cue, "chalked_friction_coefficient"));
        profile.cue.unchalkedFrictionCoefficient = static_cast<float>(
            requiredNumber(cue, "unchalked_friction_coefficient"));
        profile.cue.maximumReliableOffsetRadius = static_cast<float>(
            requiredNumber(cue, "maximum_reliable_offset_radius"));
        profile.cue.cueSpeedPerPowerUnitCmS = static_cast<float>(
            requiredNumber(cue, "cue_speed_per_power_unit_cm_s"));
    }
    if (frozenCueContact != nullptr) {
        profile.frozenCueContact.enabled =
            required(*frozenCueContact, "enabled").asBool();
        profile.frozenCueContact.normalStiffnessNPerM32 = requiredNumber(
            *frozenCueContact, "normal_stiffness_n_per_m32");
        profile.frozenCueContact.normalDissipationSPerM = requiredNumber(
            *frozenCueContact, "normal_dissipation_s_per_m");
        profile.frozenCueContact.tangentialStiffnessNPerM = requiredNumber(
            *frozenCueContact, "tangential_stiffness_n_per_m");
        profile.frozenCueContact.tangentialDampingNsPerM = requiredNumber(
            *frozenCueContact, "tangential_damping_ns_per_m");
        profile.frozenCueContact.microstepSeconds = requiredNumber(
            *frozenCueContact, "microstep_seconds");
        profile.frozenCueContact.maximumContactSeconds = requiredNumber(
            *frozenCueContact, "maximum_contact_seconds");
        profile.frozenCueContact.releaseCompressionM = requiredNumber(
            *frozenCueContact, "release_compression_m");
        profile.frozenCueContact.maximumCompressionM = requiredNumber(
            *frozenCueContact, "maximum_compression_m");
        profile.frozenCueContact.maximumNormalForceN = requiredNumber(
            *frozenCueContact, "maximum_normal_force_n");
    }
    profile.cushion.normalRestitution = static_cast<float>(
        requiredNumber(cushion, "normal_restitution"));
    profile.cushion.restitutionIntercept = profile.cushion.normalRestitution;
    profile.cushion.restitutionSlopePerMps = 0.0f;
    profile.cushion.minimumRestitution = profile.cushion.normalRestitution;
    profile.cushion.maximumRestitution = profile.cushion.normalRestitution;
    profile.cushion.frictionCoefficient = static_cast<float>(
        requiredNumber(cushion, "friction_coefficient"));
    if (scenarioVersion >= 11) {
        profile.cushion.restitutionIntercept = static_cast<float>(
            requiredNumber(cushion, "restitution_intercept"));
        profile.cushion.restitutionSlopePerMps = static_cast<float>(
            requiredNumber(cushion, "restitution_slope_per_mps"));
        profile.cushion.minimumRestitution = static_cast<float>(
            requiredNumber(cushion, "minimum_restitution"));
        profile.cushion.maximumRestitution = static_cast<float>(
            requiredNumber(cushion, "maximum_restitution"));
    }
    if (scenarioVersion >= 6) {
        profile.cushion.noseHeightRatio = static_cast<float>(
            requiredNumber(cushion, "nose_height_ratio"));
        profile.cushion.maximumRigidIncidentSpeedCmS = static_cast<float>(
            requiredNumber(cushion, "maximum_rigid_incident_speed_cm_s"));
        profile.cushion.material = requiredString(cushion, "material");
    }
    if (tableBoundary != nullptr) {
        profile.tableBoundary.playfieldWidthCm = static_cast<float>(
            requiredNumber(*tableBoundary, "playfield_width_cm"));
        profile.tableBoundary.playfieldLengthCm = static_cast<float>(
            requiredNumber(*tableBoundary, "playfield_length_cm"));
        profile.tableBoundary.cornerMouthWidthCm = static_cast<float>(
            requiredNumber(*tableBoundary, "corner_mouth_width_cm"));
        profile.tableBoundary.sideMouthWidthCm = static_cast<float>(
            requiredNumber(*tableBoundary, "side_mouth_width_cm"));
        profile.tableBoundary.cornerThroatWidthCm = static_cast<float>(
            requiredNumber(*tableBoundary, "corner_throat_width_cm"));
        profile.tableBoundary.sideThroatWidthCm = static_cast<float>(
            requiredNumber(*tableBoundary, "side_throat_width_cm"));
        profile.tableBoundary.jawRadiusCm = static_cast<float>(
            requiredNumber(*tableBoundary, "jaw_radius_cm"));
        profile.tableBoundary.throatDepthCm = static_cast<float>(
            requiredNumber(*tableBoundary, "throat_depth_cm"));
        profile.tableBoundary.captureDepthCm = static_cast<float>(
            requiredNumber(*tableBoundary, "capture_depth_cm"));
        profile.tableBoundary.geometryId = requiredString(
            *tableBoundary, "geometry_id");
        profile.tableBoundary.material = requiredString(*tableBoundary, "material");
    }
    profile.solver.timeStepSeconds = static_cast<float>(
        requiredNumber(solver, "time_step_seconds"));
    profile.solver.maximumEventsPerTick = required(solver, "maximum_events_per_tick").asInt();
    if (scenarioVersion >= 8) {
        profile.solver.toiToleranceSeconds = static_cast<float>(
            requiredNumber(solver, "toi_tolerance_seconds"));
        profile.solver.maximumIslandSize = required(solver, "maximum_island_size").asInt();
        profile.solver.velocityIterations = required(solver, "velocity_iterations").asInt();
        profile.solver.positionIterations = required(solver, "position_iterations").asInt();
        profile.solver.penetrationSlopCm = static_cast<float>(
            requiredNumber(solver, "penetration_slop_cm"));
        profile.solver.maximumPenetrationCm = static_cast<float>(
            requiredNumber(solver, "maximum_penetration_cm"));
        profile.solver.residualToleranceCmS = static_cast<float>(
            requiredNumber(solver, "residual_tolerance_cm_s"));
        if (scenarioVersion >= 10) {
            profile.solver.passiveEnergyToleranceJ = requiredNumber(
                solver, "passive_energy_tolerance_j");
        }
    }
    const PhysicsProfileValidation validation = validatePhysicsProfile(profile);
    if (!validation.ok) throw std::runtime_error(validation.error);
    return profile;
}

}  // namespace

PhysicsScenarioResult parsePhysicsScenario(const json::Value& value)
{
    PhysicsScenarioResult result;
    result.errorCode = "invalid_scenario";
    try {
        if (!value.isObject()) throw std::runtime_error("scenario must be an object");
        const int version = required(value, "schema_version").asInt();
        if (version < 1 || version > kPhysicsScenarioVersion) {
            result.errorCode = "unsupported_scenario_version";
            result.errorMessage =
                "only physics scenario versions 1 through 12 are supported";
            return result;
        }

        PhysicsScenario scenario;
        scenario.schemaVersion = version;
        scenario.id = requiredString(value, "id");
        scenario.description = requiredString(value, "description");

        const json::Value& evidence = required(value, "evidence");
        scenario.evidenceGrade = requiredString(evidence, "grade");
        if (scenario.evidenceGrade != "A" && scenario.evidenceGrade != "B" &&
            scenario.evidenceGrade != "C") {
            throw std::runtime_error("evidence grade must be A, B, or C");
        }
        scenario.evidenceSource = requiredString(evidence, "source");
        scenario.equipment = requiredString(evidence, "equipment");
        if (scenario.equipment != "WPA_POOL" &&
            !(version >= 5 &&
              scenario.equipment == "SOURCE_LABORATORY_APPARATUS")) {
            throw std::runtime_error(
                "equipment must be WPA_POOL or a version 5+ source laboratory apparatus");
        }

        if (value.has("boundary_mode")) {
            if (version < 9 || !value.at("boundary_mode").isString()) {
                result.errorCode = "INVALID_BOUNDARY_MODE";
                result.errorMessage =
                    "boundary_mode requires scenario version 9 and a string value";
                return result;
            }
            const std::string mode = value.at("boundary_mode").asString();
            if (mode == "production_table") {
                scenario.boundaryMode = PhysicsBoundaryMode::ProductionTable;
            } else if (mode == "unbounded") {
                scenario.boundaryMode = PhysicsBoundaryMode::Unbounded;
            } else {
                result.errorCode = "INVALID_BOUNDARY_MODE";
                result.errorMessage =
                    "boundary_mode must be production_table or unbounded";
                return result;
            }
        }

        const json::Value& simulation = required(value, "simulation");
        scenario.ticks = required(simulation, "ticks").asInt();
        if (scenario.ticks < 1 || scenario.ticks > 1000000) {
            throw std::runtime_error("ticks must be between 1 and 1000000");
        }
        scenario.timeStepSeconds = static_cast<float>(
            requiredNumber(simulation, "time_step_seconds"));
        if (scenario.timeStepSeconds <= 0.0f) {
            throw std::runtime_error("time_step_seconds must be positive");
        }
        if (version < 3 &&
            std::fabs(scenario.timeStepSeconds - kDefaultTimeStep) > 0.000001f) {
            result.errorCode = "unsupported_time_step";
            result.errorMessage =
                "legacy scenarios without a physics profile require the default 0.1 second time step";
            return result;
        }

        if (version >= 3) {
            scenario.physicsProfile = parsePhysicsProfile(
                required(value, "physics_profile"), version);
            if (std::fabs(
                    scenario.physicsProfile.solver.timeStepSeconds -
                    scenario.timeStepSeconds) > 0.000001f) {
                throw std::runtime_error(
                    "physics profile and scenario time steps must agree");
            }
        } else if (value.has("physics_profile")) {
            throw std::runtime_error("physics_profile requires schema version 3");
        }
        if (value.has("initial_contact_epsilon_cm")) {
            scenario.initialContactEpsilonCm = static_cast<float>(
                requiredNumber(value, "initial_contact_epsilon_cm"));
            if (scenario.initialContactEpsilonCm < 0.0f ||
                scenario.initialContactEpsilonCm >=
                    2.0f * scenario.physicsProfile.ball.radiusCm) {
                throw std::runtime_error(
                    "initial_contact_epsilon_cm must be nonnegative and smaller than the ball diameter");
            }
        }

        GameState initial;
        initializeBalls(initial);
        for (BallState& ball : initial.balls) {
            resetBallMotion(ball);
            ball.pocketed = true;
        }
        scenario.balls = initial.balls;

        const json::Value& balls = required(value, "balls");
        if (!balls.isArray()) throw std::runtime_error("balls must be an array");
        bool seen[kBallCount] = {};
        for (const json::Value& item : balls.asArray()) {
            if (!item.isObject()) throw std::runtime_error("each ball must be an object");
            const int index = required(item, "index").asInt();
            if (index < 0 || index >= kBallCount) throw std::runtime_error("ball index must be between 0 and 15");
            if (seen[index]) throw std::runtime_error("ball indices must be unique");
            seen[index] = true;

            BallState ball = scenario.balls[index];
            ball.position = pointArray(item, "position_cm");
            ball.velocity = pointArray(item, "velocity_cm_s");
            ball.angularVelocity = pointArray(item, "angular_velocity_rad_s");
            if (item.has("pocketed")) {
                if (!item.at("pocketed").isBool()) throw std::runtime_error("pocketed must be boolean");
                ball.pocketed = item.at("pocketed").asBool();
            } else {
                ball.pocketed = false;
            }
            ball.speed = std::sqrt(
                ball.velocity.x * ball.velocity.x + ball.velocity.z * ball.velocity.z);
            scenario.balls[index] = ball;
        }

        if (version == 2 || (version >= 3 && value.has("cue_impact"))) {
            const json::Value& cue = required(value, "cue_impact");
            if (!cue.isObject()) throw std::runtime_error("cue_impact must be an object");
            CueImpactInput input;
            input.cueBallIndex = required(cue, "cue_ball_index").asInt();
            if (input.cueBallIndex < 0 || input.cueBallIndex >= kBallCount ||
                !seen[input.cueBallIndex] || scenario.balls[input.cueBallIndex].pocketed) {
                throw std::runtime_error("cue_ball_index must identify one active listed ball");
            }
            input.cueSpeedCmS = requiredNumber(cue, "cue_speed_cm_s");
            input.cueMassKg = requiredNumber(cue, "cue_mass_kg");
            if (input.cueSpeedCmS < 0.0) throw std::runtime_error("cue_speed_cm_s must be nonnegative");
            if (input.cueMassKg <= 0.0) throw std::runtime_error("cue_mass_kg must be positive");
            input.direction = numberTriple(cue, "direction");
            const double directionLength = std::sqrt(
                input.direction[0] * input.direction[0] +
                input.direction[1] * input.direction[1] +
                input.direction[2] * input.direction[2]);
            if (std::fabs(directionLength - 1.0) > 0.000001) {
                throw std::runtime_error("direction must be a unit vector");
            }
            if (std::fabs(input.direction[1]) > 0.000001) {
                throw std::runtime_error(
                    "direction must be a table-plane heading; use elevation_degrees separately");
            }
            input.elevationDegrees = requiredNumber(cue, "elevation_degrees");
            if (input.elevationDegrees < -90.0 || input.elevationDegrees > 90.0) {
                throw std::runtime_error("elevation_degrees must be between -90 and 90");
            }
            input.tipOffsetCm = numberPair(cue, "tip_offset_cm");
            input.tipOffsetRadius = numberPair(cue, "tip_offset_radius");
            input.chalkState = requiredString(cue, "chalk_state");
            const double cmMagnitude = std::hypot(input.tipOffsetCm[0], input.tipOffsetCm[1]);
            const double radiusMagnitude = std::hypot(
                input.tipOffsetRadius[0], input.tipOffsetRadius[1]);
            const double ballRadiusCm = scenario.physicsProfile.ball.radiusCm;
            if (cmMagnitude > ballRadiusCm + 0.000001 || radiusMagnitude > 1.000001) {
                throw std::runtime_error("tip offset must remain within the cue-ball radius");
            }
            for (std::size_t index = 0; index < 2; ++index) {
                if (std::fabs(input.tipOffsetCm[index] /
                    ballRadiusCm - input.tipOffsetRadius[index]) > 0.0001) {
                    throw std::runtime_error("tip_offset_cm and tip_offset_radius are inconsistent");
                }
            }
            scenario.hasCueImpact = true;
            scenario.cueImpact = input;
        } else if (value.has("cue_impact")) {
            throw std::runtime_error(
                "cue_impact requires schema version 2, 3, 4, or 5");
        }

        const json::Value& expectations = required(value, "expectations");
        if (!expectations.isArray() || expectations.asArray().empty()) {
            throw std::runtime_error("expectations must be a nonempty array");
        }
        for (const json::Value& item : expectations.asArray()) {
            if (!item.isObject()) throw std::runtime_error("each expectation must be an object");
            PhysicsExpectation expectation;
            expectation.metric = requiredString(item, "metric");
            expectation.comparison = requiredString(item, "operator");
            if (!supportedComparison(expectation.comparison)) {
                throw std::runtime_error("operator must be eq, lte, or gte");
            }
            expectation.value = required(item, "value");
            expectation.absoluteTolerance = optionalTolerance(item, "absolute_tolerance");
            expectation.relativeTolerance = optionalTolerance(item, "relative_tolerance");
            scenario.expectations.push_back(expectation);
        }

        result.ok = true;
        result.scenario = scenario;
        result.errorCode.clear();
        result.errorMessage.clear();
    } catch (const std::exception& error) {
        result.errorMessage = error.what();
    }
    return result;
}

ActionResult applyPhysicsScenario(GameRuntime& runtime, const PhysicsScenario& scenario)
{
    const ScenarioGeometryResult geometry = validateScenarioGeometry(scenario);
    if (!geometry.ok) {
        return ActionResult{false, "INTEGRATION_MISMATCH"};
    }
    GameState state;
    initializeBalls(state);
    state.balls = scenario.balls;
    return runtime.replaceStateForScenario(
        state, scenario.physicsProfile,
        scenario.hasCueImpact ? &scenario.cueImpact : nullptr,
        scenario.hasCueImpact && scenario.schemaVersion >= 4,
        scenario.boundaryMode);
}

}  // namespace billiardgl
