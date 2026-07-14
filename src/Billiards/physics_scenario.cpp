#include "physics_scenario.h"
#include "table_specs.h"

#include <cmath>
#include <exception>
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

}  // namespace

PhysicsScenarioResult parsePhysicsScenario(const json::Value& value)
{
    PhysicsScenarioResult result;
    result.errorCode = "invalid_scenario";
    try {
        if (!value.isObject()) throw std::runtime_error("scenario must be an object");
        const int version = required(value, "schema_version").asInt();
        if (version != 1 && version != kPhysicsScenarioVersion) {
            result.errorCode = "unsupported_scenario_version";
            result.errorMessage = "only physics scenario versions 1 and 2 are supported";
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
        if (scenario.equipment != "WPA_POOL") {
            throw std::runtime_error("equipment must be WPA_POOL");
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
        if (std::fabs(scenario.timeStepSeconds - kDefaultTimeStep) > 0.000001f) {
            result.errorCode = "unsupported_time_step";
            result.errorMessage = "runtime currently supports only the default 0.1 second time step";
            return result;
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

        if (version == 2) {
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
            if (cmMagnitude > kChineseBallRadiusCm + 0.000001 || radiusMagnitude > 1.000001) {
                throw std::runtime_error("tip offset must remain within the cue-ball radius");
            }
            for (std::size_t index = 0; index < 2; ++index) {
                if (std::fabs(input.tipOffsetCm[index] /
                    kChineseBallRadiusCm - input.tipOffsetRadius[index]) > 0.0001) {
                    throw std::runtime_error("tip_offset_cm and tip_offset_radius are inconsistent");
                }
            }
            scenario.hasCueImpact = true;
            scenario.cueImpact = input;
        } else if (value.has("cue_impact")) {
            throw std::runtime_error("cue_impact requires schema version 2");
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
    GameState state;
    initializeBalls(state);
    state.balls = scenario.balls;
    runtime.replaceStateForScenario(
        state, scenario.hasCueImpact ? &scenario.cueImpact : nullptr);
    return ActionResult{};
}

}  // namespace billiardgl
