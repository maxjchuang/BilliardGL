#include "training/training_scenario.h"

#include "deterministic_hash.h"

#include <algorithm>
#include <charconv>
#include <cmath>
#include <fstream>
#include <iomanip>
#include <iterator>
#include <sstream>
#include <stdexcept>

namespace billiardgl::training {
namespace {

const json::Value& required(const json::Value& object, const char* key)
{
    if (!object.isObject() || !object.has(key))
        throw std::runtime_error(std::string("missing ") + key);
    return object.at(key);
}

std::string requiredString(const json::Value& object, const char* key)
{
    const json::Value& value = required(object, key);
    if (!value.isString()) throw std::runtime_error(std::string(key) + " must be string");
    return value.asString();
}

int requiredInt(const json::Value& object, const char* key)
{
    return required(object, key).asInt();
}

bool validSha256(const std::string& value)
{
    return value.size() == 64 && std::all_of(value.begin(), value.end(),
        [](unsigned char c) { return (c >= '0' && c <= '9') || (c >= 'a' && c <= 'f'); });
}

std::string canonicalNumber(double value, bool integerField)
{
    char buffer[64];
    if (integerField) {
        const auto result = std::to_chars(buffer, buffer + sizeof(buffer),
            static_cast<long long>(value));
        return std::string(buffer, result.ptr);
    }
    const auto result = std::to_chars(buffer, buffer + sizeof(buffer), value,
        std::chars_format::general);
    std::string text(buffer, result.ptr);
    if (text.find_first_of(".eE") == std::string::npos) text += ".0";
    return text;
}

std::string canonicalScenarioValue(const json::Value& value,
    const std::string& parentKey = std::string())
{
    if (value.isNull()) return "null";
    if (value.isBool()) return value.asBool() ? "true" : "false";
    if (value.isString()) return json::stringify(value);
    if (value.isNumber()) {
        const bool integerField = parentKey == "index" ||
            parentKey == "schema_version" || parentKey == "ticks";
        return canonicalNumber(value.asNumber(), integerField);
    }
    if (value.isArray()) {
        std::string result = "[";
        bool first = true;
        for (const json::Value& item : value.asArray()) {
            if (!first) result += ',';
            first = false;
            result += canonicalScenarioValue(item, parentKey);
        }
        return result + ']';
    }
    std::string result = "{";
    bool first = true;
    for (const auto& item : value.asObject()) {
        if (!first) result += ',';
        first = false;
        result += json::stringify(json::Value(item.first)) + ':' +
            canonicalScenarioValue(item.second, item.first);
    }
    return result + '}';
}

bool orderedSubsequence(const std::vector<int>& expected,
    const std::vector<int>& actual)
{
    std::size_t cursor = 0;
    for (int value : actual) {
        if (cursor < expected.size() && value == expected[cursor]) ++cursor;
    }
    return cursor == expected.size();
}

int observedPocketId(const AttemptFacts& facts, int ballIndex)
{
    if (ballIndex < 0 || ballIndex >= kBallCount) return -1;
    if (facts.pocketIds[ballIndex] >= 0) return facts.pocketIds[ballIndex];
    const BallState& ball = facts.state.balls[ballIndex];
    return ball.pocketed &&
        ball.pocketInteraction.phase == PocketInteractionPhase::Captured ?
        ball.pocketInteraction.pocketId : -1;
}

bool predicateMatches(const json::Value& condition, const AttemptFacts& facts)
{
    const std::string name = requiredString(condition, "predicate");
    if (name == "ball_pocketed" || name == "ball_not_pocketed") {
        const int index = requiredInt(condition, "ball_index");
        if (index < 0 || index >= kBallCount) return false;
        return facts.state.balls[index].pocketed == (name == "ball_pocketed");
    }
    if (name == "ball_pocketed_in_pocket") {
        const int index = requiredInt(condition, "ball_index");
        const int pocketId = requiredInt(condition, "pocket_id");
        return index >= 0 && index < kBallCount &&
            pocketId >= 0 && pocketId < 6 &&
            facts.state.balls[index].pocketed &&
            observedPocketId(facts, index) == pocketId;
    }
    if (name == "ball_final_position_in_zone") {
        const int index = requiredInt(condition, "ball_index");
        const json::Value::Array& center = required(condition, "center_cm").asArray();
        if (index < 0 || index >= kBallCount || center.size() != 2) return false;
        const double dx = facts.state.balls[index].position.x - center[0].asNumber();
        const double dz = facts.state.balls[index].position.z - center[1].asNumber();
        const double radius = required(condition, "radius_cm").asNumber();
        return dx * dx + dz * dz <= radius * radius;
    }
    if (name == "cue_ball_final_near_first_contact_point") {
        if (!facts.hasCueBallPositionAtFirstContact) return false;
        const double dx = facts.state.balls[0].position.x -
            facts.cueBallPositionAtFirstContactCm.x;
        const double dz = facts.state.balls[0].position.z -
            facts.cueBallPositionAtFirstContactCm.z;
        const double radius = required(condition, "radius_cm").asNumber();
        return radius > 0.0 && dx * dx + dz * dz <= radius * radius;
    }
    if (name == "shot_count_lte")
        return facts.shotCount <= requiredInt(condition, "maximum");
    if (name == "first_contact_ball")
        return facts.firstContactBall == requiredInt(condition, "ball_index");
    if (name == "rail_contact_count_gte") {
        const int index = requiredInt(condition, "ball_index");
        return index >= 0 && index < kBallCount &&
            facts.railContacts[index] >= requiredInt(condition, "minimum");
    }
    if (name == "rail_contact_count_lte") {
        const int index = requiredInt(condition, "ball_index");
        const int maximum = requiredInt(condition, "maximum");
        return index >= 0 && index < kBallCount && maximum >= 0 &&
            facts.railContacts[index] <= maximum;
    }
    if (name == "distinct_object_balls_with_rail_contact_gte")
        return facts.distinctObjectBallsWithRailContact >= requiredInt(condition, "minimum");
    if (name == "ordered_ball_pockets") {
        std::vector<int> expected;
        for (const json::Value& value : required(condition, "ball_indices").asArray())
            expected.push_back(value.asInt());
        return orderedSubsequence(expected, facts.pocketOrder);
    }
    if (name == "numerical_failure") return facts.numericalFailure;
    if (name == "cue_ball_pocketed") return facts.state.balls[0].pocketed;
    if (name == "cue_ball_pocketed_during_attempt")
        return facts.state.balls[0].pocketed ||
            std::find(facts.pocketOrder.begin(), facts.pocketOrder.end(), 0) != facts.pocketOrder.end();
    if (name == "shot_limit_exceeded")
        return facts.shotCount > requiredInt(condition, "maximum");
    if (name == "timeout_ticks")
        return facts.elapsedTicks > static_cast<std::uint64_t>(requiredInt(condition, "maximum"));
    throw std::runtime_error("unsupported predicate " + name);
}

double cueBallTargetError(const Scenario& scenario, const AttemptFacts& facts)
{
    for (const json::Value& condition : scenario.success.asArray()) {
        const std::string predicate = requiredString(condition, "predicate");
        if (predicate == "cue_ball_final_near_first_contact_point") {
            if (!facts.hasCueBallPositionAtFirstContact) return 0.0;
            return std::hypot(
                facts.state.balls[0].position.x -
                    facts.cueBallPositionAtFirstContactCm.x,
                facts.state.balls[0].position.z -
                    facts.cueBallPositionAtFirstContactCm.z);
        }
        if (predicate != "ball_final_position_in_zone") continue;
        const json::Value::Array& center = required(condition, "center_cm").asArray();
        const double dx = facts.state.balls[0].position.x - center[0].asNumber();
        const double dz = facts.state.balls[0].position.z - center[1].asNumber();
        return std::hypot(dx, dz);
    }
    return 0.0;
}

double quantize(double value, double quantum)
{
    return quantum > 0.0 ? std::round(value / quantum) * quantum : value;
}

}  // namespace

LoadResult parseScenario(const json::Value& value,
    const std::set<std::string>& capabilities)
{
    LoadResult result;
    try {
        if (requiredString(value, "schema") != "training_scenario_v1" ||
            requiredInt(value, "schema_version") != kTrainingScenarioVersion) {
            result.errorCode = "E_TRAINING_SCHEMA";
            result.errorMessage = "unsupported training scenario schema";
            return result;
        }
        Scenario scenario;
        scenario.schemaVersion = kTrainingScenarioVersion;
        scenario.id = requiredString(value, "id");
        scenario.version = requiredInt(value, "version");
        scenario.category = requiredString(value, "category");
        if (scenario.id.empty() ||
            (scenario.version != 1 && scenario.version != 2))
            throw std::runtime_error("invalid scenario identity");

        const json::Value& initial = required(value, "initial_scenario");
        scenario.scenarioSha256 = requiredString(initial, "canonical_sha256");
        if (!validSha256(scenario.scenarioSha256))
            throw std::runtime_error("invalid initial scenario sha256");
        const json::Value& boundScenario = required(initial, "scenario");
        const std::string computedScenarioSha256 = sha256Hex(
            canonicalScenarioValue(boundScenario) + "\n");
        if (computedScenarioSha256 != scenario.scenarioSha256) {
            result.errorCode = "E_TRAINING_HASH_MISMATCH";
            result.errorMessage = "initial scenario content hash mismatch";
            return result;
        }
        // The launch pack uses the product-facing equipment name while the
        // legacy physics-scenario v1 parser predates it and accepts WPA_POOL.
        // Normalize only the parser adapter copy; the content and its bound
        // canonical hash remain untouched.
        json::Value normalizedScenario = boundScenario;
        if (normalizedScenario.has("evidence") &&
            normalizedScenario.at("evidence").has("equipment") &&
            normalizedScenario.at("evidence").at("equipment").asString() ==
                "CHINESE_EIGHT_BALL") {
            normalizedScenario["evidence"]["equipment"] = json::Value("WPA_POOL");
        }
        const PhysicsScenarioResult parsed = parsePhysicsScenario(normalizedScenario);
        if (!parsed.ok) {
            result.errorCode = parsed.errorCode;
            result.errorMessage = parsed.errorMessage;
            return result;
        }
        scenario.initialScenario = parsed.scenario;
        const json::Value& success = required(value, "success");
        const json::Value& failure = required(value, "failure");
        scenario.success = required(success, "all");
        scenario.failure = required(failure, "any");
        scenario.scoring = required(value, "scoring");
        if (!scenario.success.isArray() || scenario.success.asArray().empty()) {
            result.errorCode = "E_REQUIRED_SUCCESS";
            result.errorMessage = "success predicates are required";
            return result;
        }
        if (!scenario.failure.isArray() || !scenario.scoring.isObject() ||
            !required(scenario.scoring, "deterministic").asBool()) {
            result.errorCode = "E_SCORING_DETERMINISM";
            result.errorMessage = "deterministic scoring is required";
            return result;
        }
        const json::Value& gate = required(value, "runtime_gate");
        for (const json::Value& capability : required(gate, "required_capabilities").asArray()) {
            const std::string name = capability.asString();
            scenario.requiredCapabilities.push_back(name);
            if (capabilities.count(name) == 0) result.missingCapabilities.push_back(name);
        }
        scenario.scoreEnabled = result.missingCapabilities.empty();
        result.scenario = scenario;
        result.ok = true;
    } catch (const std::exception& error) {
        result.errorCode = "E_TRAINING_FORMAT";
        result.errorMessage = error.what();
    }
    return result;
}

LoadResult loadScenarioFile(const std::string& path,
    const std::set<std::string>& capabilities)
{
    std::ifstream input(path, std::ios::binary);
    if (!input) return LoadResult{false, Scenario{}, "E_TRAINING_IO", "cannot open scenario", {}};
    const std::string bytes((std::istreambuf_iterator<char>(input)),
        std::istreambuf_iterator<char>());
    const json::ParseResult parsed = json::parse(bytes);
    if (!parsed.ok)
        return LoadResult{false, Scenario{}, "E_TRAINING_JSON", parsed.error, {}};
    return parseScenario(parsed.value, capabilities);
}

Evaluation evaluate(const Scenario& scenario, const AttemptFacts& facts)
{
    Evaluation result;
    try {
        for (const json::Value& condition : scenario.failure.asArray()) {
            if (predicateMatches(condition, facts)) {
                result.failed = true;
                result.reason = requiredString(condition, "predicate");
                if (result.reason == "cue_ball_pocketed" || result.reason == "cue_ball_pocketed_during_attempt")
                    result.feedback = "Cue ball pocketed - restart the drill";
                else if (result.reason == "shot_limit_exceeded") result.feedback = "Shot limit exceeded - restart the drill";
                else if (result.reason == "timeout_ticks") result.feedback = "Time limit reached - restart the drill";
                else if (result.reason == "numerical_failure") result.feedback = "Simulation error - inspect diagnostics";
                return result;
            }
        }
        result.succeeded = std::all_of(scenario.success.asArray().begin(),
            scenario.success.asArray().end(),
            [&](const json::Value& condition) { return predicateMatches(condition, facts); });
        if (!result.succeeded) {
            result.reason = "success_conditions_not_met";
            for (const auto& condition : scenario.success.asArray()) {
                if (predicateMatches(condition, facts)) continue;
                const auto name = requiredString(condition, "predicate");
                if (!result.feedback.empty()) result.feedback += "; ";
                if (name == "ball_final_position_in_zone") {
                    const int ball = requiredInt(condition, "ball_index");
                    const auto& center = required(condition, "center_cm").asArray();
                    const double distance = std::hypot(facts.state.balls[ball].position.x - center[0].asNumber(),
                        facts.state.balls[ball].position.z - center[1].asNumber());
                    result.feedback += "Ball " + std::to_string(ball) + " outside target: " +
                        std::to_string(static_cast<int>(std::lround(distance))) + "cm / max " +
                        std::to_string(static_cast<int>(std::lround(required(condition, "radius_cm").asNumber()))) + "cm";
                } else if (name == "cue_ball_final_near_first_contact_point") {
                    if (!facts.hasCueBallPositionAtFirstContact) {
                        result.feedback += "First cue-object contact point unavailable";
                    } else {
                        const double distance = std::hypot(
                            facts.state.balls[0].position.x -
                                facts.cueBallPositionAtFirstContactCm.x,
                            facts.state.balls[0].position.z -
                                facts.cueBallPositionAtFirstContactCm.z);
                        if (facts.state.balls[1].pocketed)
                            result.feedback += "Ball 1 pocketed; ";
                        std::ostringstream detail;
                        detail << std::fixed << std::setprecision(1)
                               << "cue moved " << distance <<
                            "cm / max " <<
                            required(condition, "radius_cm").asNumber() << "cm";
                        result.feedback += detail.str();
                    }
                } else if (name == "ball_pocketed") {
                    result.feedback += "Ball " + std::to_string(requiredInt(condition, "ball_index")) + " not pocketed";
                } else if (name == "ball_pocketed_in_pocket") {
                    const int ball = requiredInt(condition, "ball_index");
                    const int expectedPocket = requiredInt(
                        condition, "pocket_id");
                    if (ball < 0 || ball >= kBallCount || expectedPocket < 0 ||
                        expectedPocket >= 6) {
                        result.feedback += "Invalid target pocket";
                    } else {
                        const int actualPocket = observedPocketId(facts, ball);
                        result.feedback += "Ball " + std::to_string(ball) +
                            " pocket: " + (actualPocket >= 0 ?
                                std::to_string(actualPocket) : "none") +
                            " / target " + std::to_string(expectedPocket);
                    }
                } else if (name == "rail_contact_count_lte") {
                    const int ball = requiredInt(condition, "ball_index");
                    const int maximum = requiredInt(condition, "maximum");
                    if (ball < 0 || ball >= kBallCount || maximum < 0) {
                        result.feedback += "Invalid rail contact limit";
                    } else {
                        result.feedback += "Ball " + std::to_string(ball) +
                            " rail contacts: " +
                            std::to_string(facts.railContacts[ball]) +
                            " / max " + std::to_string(maximum);
                    }
                } else if (name == "rail_contact_count_gte") {
                    const int ball = requiredInt(condition, "ball_index");
                    if (ball >= 0 && ball < kBallCount)
                        result.feedback += "Ball " + std::to_string(ball) + " rail contacts: " +
                            std::to_string(facts.railContacts[ball]) + " / need " +
                            std::to_string(requiredInt(condition, "minimum"));
                } else if (name == "distinct_object_balls_with_rail_contact_gte") {
                    result.feedback += "Object balls reaching rails: " +
                        std::to_string(facts.distinctObjectBallsWithRailContact) + " / need " +
                        std::to_string(requiredInt(condition, "minimum"));
                } else if (name == "first_contact_ball") {
                    result.feedback += "First hit: " + std::to_string(facts.firstContactBall) +
                        " / aim for ball " + std::to_string(requiredInt(condition, "ball_index"));
                } else if (name == "ordered_ball_pockets") {
                    result.feedback += "Pocket the highlighted balls in the shown order";
                } else if (name == "ball_not_pocketed") {
                    result.feedback += "Ball " + std::to_string(requiredInt(condition, "ball_index")) + " must stay on table";
                } else if (name == "shot_count_lte") {
                    result.feedback += "Use at most " + std::to_string(requiredInt(condition, "maximum")) + " shots";
                } else result.feedback += "Unmet: " + name;
            }
            return result;
        }
        result.scoreEligible = scenario.scoreEnabled;
        if (!result.scoreEligible) {
            result.reason = "missing_capability";
            return result;
        }
        const int minimum = requiredInt(scenario.scoring, "minimum");
        const int maximum = requiredInt(scenario.scoring, "maximum");
        double score = required(scenario.scoring, "base_points").asNumber();
        const int par = requiredInt(scenario.scoring, "par_shots");
        for (const json::Value& deduction : required(scenario.scoring, "deductions").asArray()) {
            const std::string metric = requiredString(deduction, "metric");
            double units = 0.0;
            if (metric == "cue_ball_target_error_cm") units = cueBallTargetError(scenario, facts);
            else if (metric == "cue_ball_bonus_zone_missed")
                units = predicateMatches(required(scenario.scoring, "optional_bonus_zone"), facts) ? 0.0 : 1.0;
            else if (metric == "shot_count_over_par") units = std::max(0, facts.shotCount - par);
            const double measured = quantize(units, required(deduction, "quantization").asNumber());
            score -= std::min(required(deduction, "cap_points").asNumber(),
                measured * required(deduction, "points_per_unit").asNumber());
        }
        result.score = std::max(minimum, std::min(maximum,
            static_cast<int>(std::round(score))));
        result.reason = "success";
    } catch (const std::exception& error) {
        result.failed = true;
        result.reason = std::string("evaluation_error:") + error.what();
    }
    return result;
}

}  // namespace billiardgl::training
