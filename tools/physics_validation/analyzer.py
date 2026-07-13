import json
import math
from dataclasses import dataclass


NUMERICAL_FAILURE = "NUMERICAL_FAILURE"
MODEL_MISMATCH = "MODEL_MISMATCH"
NON_DETERMINISTIC = "NON_DETERMINISTIC"
INTEGRATION_MISMATCH = "INTEGRATION_MISMATCH"
REFERENCE_LIMITATION = "REFERENCE_LIMITATION"


@dataclass(frozen=True)
class Failure:
    code: str
    metric: str
    message: str
    expected: object
    actual: object


@dataclass(frozen=True)
class ScenarioResult:
    scenario_id: str
    passed: bool
    evidence_grade: str
    metrics: dict
    failures: tuple


@dataclass(frozen=True)
class KnownFailureMatch:
    known: set
    new: set
    missing: set


def _finite(value):
    if isinstance(value, bool) or value is None or isinstance(value, str):
        return True
    if isinstance(value, (int, float)):
        return math.isfinite(value)
    if isinstance(value, dict):
        return all(_finite(item) for item in value.values())
    if isinstance(value, (list, tuple)):
        return all(_finite(item) for item in value)
    return False


def _tolerance(expectation, expected):
    absolute = float(expectation.get("absolute_tolerance", 0.0))
    relative = float(expectation.get("relative_tolerance", 0.0))
    if isinstance(expected, (int, float)) and not isinstance(expected, bool):
        return max(absolute, abs(expected) * relative)
    return absolute


def _compare(actual, expected, operator, tolerance):
    if isinstance(actual, (int, float)) and isinstance(expected, (int, float)) \
            and not isinstance(actual, bool) and not isinstance(expected, bool):
        if operator == "eq":
            return abs(actual - expected) <= tolerance
        if operator == "lte":
            return actual <= expected + tolerance
        if operator == "gte":
            return actual >= expected - tolerance
    if operator == "eq":
        return actual == expected
    if operator == "lte":
        return actual <= expected
    if operator == "gte":
        return actual >= expected
    return False


def _ball(frame, index):
    for ball in frame.get("balls", []):
        if ball.get("index") == index:
            return ball
    raise KeyError(f"ball {index} is absent from trace")


def _vector(value):
    if isinstance(value, dict):
        return [value["x"], value["y"], value["z"]]
    return list(value)


def _scenario_ball(scenario, index):
    for ball in scenario.get("balls", []):
        if ball.get("index") == index:
            return ball
    raise KeyError(f"ball {index} is absent from scenario")


def _reference_observation(observed_metric, reference, scenario, frames):
    if observed_metric != "stopping_distance_cm":
        return None, INTEGRATION_MISMATCH, f"observed metric {observed_metric} is unavailable"
    ball_index = reference.get("ball_index")
    if not isinstance(ball_index, int) or isinstance(ball_index, bool):
        return None, REFERENCE_LIMITATION, "reference ball_index is missing"
    try:
        start = _vector(_scenario_ball(scenario, ball_index)["position_cm"])
        end = _vector(_ball(frames[-1], ball_index)["position_cm"])
        actual = math.hypot(end[0] - start[0], end[1] - start[1])
    except (IndexError, KeyError, TypeError, ValueError) as error:
        return None, INTEGRATION_MISMATCH, str(error)
    if not math.isfinite(actual):
        return actual, NUMERICAL_FAILURE, "observed reference metric is not finite"
    return actual, None, None


def _failure_code(metric):
    if metric == "permutation_invariance":
        return NON_DETERMINISTIC
    if metric in {"final_speed_cm_s", "final_velocity_cm_s"}:
        return MODEL_MISMATCH
    return NUMERICAL_FAILURE


def _evaluate(expectation, frames, comparison_frames=None, scenario=None):
    metric = expectation["metric"]
    expected = expectation.get("value")
    operator = expectation.get("operator", "eq")
    tolerance = _tolerance(expectation, expected)

    if not frames:
        return False, None, INTEGRATION_MISMATCH, "trace contains no frames"
    if metric == "finite_state":
        actual = _finite(frames)
    elif metric == "nonincreasing_translational_energy":
        energies = [frame["translational_kinetic_energy_j"] for frame in frames]
        actual = all(after <= before + tolerance
                     for before, after in zip(energies, energies[1:]))
    elif metric == "maximum_penetration_cm":
        actual = max(frame["maximum_penetration_cm"] for frame in frames)
    elif metric == "contact_count":
        actual = sum(len(frame.get("contacts", [])) for frame in frames)
    elif metric == "unexpected_ball_ball_impulse":
        actual = any(
            contact.get("kind") == "ball_ball" and contact.get("normal_impulse_ns", 0.0) > tolerance
            for frame in frames for contact in frame.get("contacts", []))
    elif metric == "missed_collision":
        actual = not any(
            contact.get("kind") == "ball_ball"
            for frame in frames for contact in frame.get("contacts", []))
    elif metric == "final_speed_cm_s":
        if not isinstance(expected, dict) or "ball_index" not in expected or "value" not in expected:
            return False, None, REFERENCE_LIMITATION, "reference speed and ball index are missing"
        actual = _ball(frames[-1], expected["ball_index"])["speed_cm_s"]
        expected = expected["value"]
        tolerance = _tolerance(expectation, expected)
    elif metric == "final_velocity_cm_s":
        if not isinstance(expected, dict) or "ball_index" not in expected or "velocity_cm_s" not in expected:
            return False, None, REFERENCE_LIMITATION, "reference velocity and ball index are missing"
        actual = _vector(_ball(frames[-1], expected["ball_index"])["velocity_cm_s"])
        target = _vector(expected["velocity_cm_s"])
        passed = all(abs(a - b) <= tolerance for a, b in zip(actual, target))
        return passed, actual, MODEL_MISMATCH, "final velocity differs from reference"
    elif metric == "value_within_interval":
        if not isinstance(expected, dict):
            return False, None, REFERENCE_LIMITATION, "reference interval is missing"
        required = {
            "point_id", "observed_metric", "ball_index", "expected",
            "lower", "upper", "unit",
        }
        if not required <= set(expected):
            return False, None, REFERENCE_LIMITATION, "reference interval fields are missing"
        numbers = (expected["expected"], expected["lower"], expected["upper"])
        if any(isinstance(value, bool) or not isinstance(value, (int, float))
               or not math.isfinite(value) for value in numbers):
            return False, None, REFERENCE_LIMITATION, "reference interval must be finite"
        if expected["lower"] > expected["upper"]:
            return False, None, REFERENCE_LIMITATION, "reference interval is inverted"
        observed_metric = expected["observed_metric"]
        if not isinstance(observed_metric, str) or not observed_metric:
            return False, None, REFERENCE_LIMITATION, "observed metric is missing"
        actual, failure_code, message = _reference_observation(
            observed_metric, expected, scenario or {}, frames)
        if failure_code is not None:
            return False, actual, failure_code, message
        passed = expected["lower"] <= actual <= expected["upper"]
        return passed, actual, MODEL_MISMATCH, "observed value is outside reference interval"
    elif metric == "permutation_invariance":
        if comparison_frames is None or not comparison_frames:
            return False, None, INTEGRATION_MISMATCH, "permutation comparison trace is missing"
        if not isinstance(expected, dict) or not isinstance(expected.get("index_map"), dict):
            return False, None, REFERENCE_LIMITATION, "permutation index map is missing"
        differences = []
        for source_text, target_index in sorted(expected["index_map"].items()):
            source_index = int(source_text)
            source_velocity = _vector(_ball(frames[-1], source_index)["velocity_cm_s"])
            target_velocity = _vector(_ball(comparison_frames[-1], target_index)["velocity_cm_s"])
            differences.append({
                "source_index": source_index,
                "target_index": target_index,
                "source_velocity_cm_s": source_velocity,
                "target_velocity_cm_s": target_velocity,
            })
        passed = all(
            all(abs(a - b) <= tolerance for a, b in zip(
                item["source_velocity_cm_s"], item["target_velocity_cm_s"]))
            for item in differences)
        return passed, differences, NON_DETERMINISTIC, "result depends on ball iteration order"
    else:
        return False, None, REFERENCE_LIMITATION, f"metric {metric} is not implemented"

    passed = _compare(actual, expected, operator, tolerance)
    return passed, actual, _failure_code(metric), f"{metric} is outside its acceptance rule"


def analyze_scenario(scenario, frames, comparison_frames=None):
    scenario_id = scenario["id"]
    grade = scenario.get("evidence", {}).get("grade", "C")
    metrics = {}
    failures = []
    for expectation in scenario.get("expectations", []):
        metric = expectation["metric"]
        result_metric = metric
        if metric == "value_within_interval" and isinstance(expectation.get("value"), dict):
            observed_metric = expectation["value"].get("observed_metric")
            if isinstance(observed_metric, str) and observed_metric:
                result_metric = observed_metric
        try:
            passed, actual, code, message = _evaluate(
                expectation, frames, comparison_frames, scenario)
        except (KeyError, TypeError, ValueError) as error:
            passed, actual, code, message = False, None, INTEGRATION_MISMATCH, str(error)
        metrics[result_metric] = actual
        if not passed:
            failures.append(Failure(
                code, result_metric, message, expectation.get("value"), actual))
    return ScenarioResult(
        scenario_id, not failures, grade, metrics, tuple(failures))


def compare_traces(scenario_id, first, second):
    canonical_first = json.dumps(first, sort_keys=True, separators=(",", ":"), allow_nan=False)
    canonical_second = json.dumps(second, sort_keys=True, separators=(",", ":"), allow_nan=False)
    if canonical_first == canonical_second:
        return None
    return Failure(
        NON_DETERMINISTIC, "trace_equal",
        f"{scenario_id} produced different traces for identical fresh runs",
        canonical_first, canonical_second)


def compare_integration_traces(scenario_id, core, process):
    canonical_core = json.dumps(core, sort_keys=True, separators=(",", ":"), allow_nan=False)
    canonical_process = json.dumps(
        process, sort_keys=True, separators=(",", ":"), allow_nan=False)
    if canonical_core == canonical_process:
        return None
    return Failure(
        INTEGRATION_MISMATCH,
        "core_process_trace_equal",
        f"{scenario_id} produced different direct-core and process traces",
        canonical_core,
        canonical_process)


def match_known_failures(results, expected):
    actual = {
        (result.scenario_id, failure.code, failure.metric)
        for result in results
        for failure in result.failures
    }
    return KnownFailureMatch(actual & expected, actual - expected, expected - actual)
