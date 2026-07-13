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
    point_id: object = None


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


_EXPERIMENTAL_METRICS = {
    "rolling_deceleration_cm_s2",
    "sliding_deceleration_cm_s2",
    "post_collision_linear_velocity_cm_s",
    "post_collision_angular_velocity_rad_s",
    "separation_angle_degrees",
    "cue_scattering_angle_degrees",
    "object_scattering_angle_degrees",
    "stick_slip_classification",
    "cushion_rebound_speed_cm_s",
    "cushion_rebound_angle_degrees",
}


def _selection(reference, required):
    selection = reference.get("selection")
    if not isinstance(selection, dict) or not required <= set(selection):
        missing = sorted(required - set(selection or {}))
        return None, REFERENCE_LIMITATION, (
            f"experimental selection metadata is incomplete: {missing}")
    ball_index = selection.get("ball_index")
    minimum = selection.get("minimum_window_ticks")
    if not isinstance(ball_index, int) or isinstance(ball_index, bool) \
            or not isinstance(minimum, int) or isinstance(minimum, bool) or minimum < 1:
        return None, REFERENCE_LIMITATION, "experimental selection values are invalid"
    return selection, None, None


def _contiguous(frames):
    for before, after in zip(frames, frames[1:]):
        if after.get("tick") != before.get("tick") + 1:
            return False
    return True


def _finite_number(value):
    return isinstance(value, (int, float)) and not isinstance(value, bool) \
        and math.isfinite(value)


def _deceleration_observation(reference, frames):
    required = {
        "sample_phase", "ball_index", "first_tick", "last_tick",
        "minimum_window_ticks",
    }
    selection, code, message = _selection(reference, required)
    if code:
        return None, code, message
    if selection["sample_phase"] != "declared_tick_window" \
            or not isinstance(selection["first_tick"], int) \
            or not isinstance(selection["last_tick"], int):
        return None, REFERENCE_LIMITATION, "deceleration tick window is invalid"
    window = [
        frame for frame in frames
        if selection["first_tick"] <= frame.get("tick", -1) <= selection["last_tick"]
    ]
    expected_ticks = selection["last_tick"] - selection["first_tick"] + 1
    if expected_ticks < selection["minimum_window_ticks"] or len(window) != expected_ticks \
            or not _contiguous(window):
        return None, INTEGRATION_MISMATCH, "deceleration trace window is incomplete"
    try:
        samples = [
            (frame["time_seconds"], _ball(frame, selection["ball_index"])["speed_cm_s"])
            for frame in window
        ]
    except (KeyError, TypeError, ValueError) as error:
        return None, INTEGRATION_MISMATCH, str(error)
    if not all(_finite_number(value) for sample in samples for value in sample):
        return None, NUMERICAL_FAILURE, "deceleration samples are not finite"
    mean_time = sum(time for time, _ in samples) / len(samples)
    denominator = sum((time - mean_time) ** 2 for time, _ in samples)
    if denominator <= 0.0:
        return None, INTEGRATION_MISMATCH, "deceleration sample times do not advance"
    mean_speed = sum(speed for _, speed in samples) / len(samples)
    slope = sum(
        (time - mean_time) * (speed - mean_speed)
        for time, speed in samples
    ) / denominator
    actual = max(0.0, -slope)
    return actual, None, None


def _contact_index(frames, event_kind, ball_index):
    contact_kind = {"rail_collision": "rail", "ball_ball": "ball_ball"}.get(event_kind)
    if contact_kind is None:
        return None
    for index, frame in enumerate(frames):
        for contact in frame.get("contacts", []):
            if contact.get("kind") != contact_kind:
                continue
            involved = ball_index in (contact.get("first_ball"), contact.get("second_ball"))
            if involved:
                return index, contact
    return None


def _is_pure_roll(ball, radius, tolerance):
    velocity = _vector(ball["velocity_cm_s"])
    angular = _vector(ball["angular_velocity_rad_s"])
    slip_x = velocity[0] + radius * angular[2]
    slip_z = velocity[2] - radius * angular[0]
    return math.hypot(slip_x, slip_z) <= tolerance


def _event_observation(metric, reference, frames):
    required = {
        "event_kind", "sample_phase", "ball_index", "minimum_window_ticks",
    }
    selection, code, message = _selection(reference, required)
    if code:
        return None, code, message
    event = _contact_index(frames, selection["event_kind"], selection["ball_index"])
    if event is None:
        return None, INTEGRATION_MISMATCH, "declared contact event is absent"
    event_index, contact = event
    # A telemetry frame is captured after its physics step, so the frame carrying
    # the contact already contains the first post-event state.
    candidates = frames[event_index:]
    minimum = selection["minimum_window_ticks"]
    if not _contiguous(frames):
        return None, INTEGRATION_MISMATCH, "trace ticks are not contiguous"

    if selection["sample_phase"] in {"first_sample_after_event", "immediate_post_impact"}:
        selected_index = 0 if candidates else None
    elif selection["sample_phase"] == "first_pure_roll_after_event":
        rolling_required = {
            "ball_radius_cm", "pure_roll_tolerance_cm_s",
        }
        if not rolling_required <= set(selection):
            return None, REFERENCE_LIMITATION, "pure-roll selection metadata is incomplete"
        radius = selection["ball_radius_cm"]
        tolerance = selection["pure_roll_tolerance_cm_s"]
        if not _finite_number(radius) or radius <= 0.0 \
                or not _finite_number(tolerance) or tolerance < 0.0:
            return None, REFERENCE_LIMITATION, "pure-roll selection values are invalid"
        indices = [selection["ball_index"]]
        if metric == "separation_angle_degrees":
            other = selection.get("other_ball_index")
            if not isinstance(other, int) or isinstance(other, bool):
                return None, REFERENCE_LIMITATION, "other ball index is missing"
            indices.append(other)
        selected_index = None
        try:
            for index in range(0, len(candidates) - minimum + 1):
                window = candidates[index:index + minimum]
                if all(
                    _is_pure_roll(_ball(frame, ball_index), radius, tolerance)
                    for frame in window for ball_index in indices
                ):
                    selected_index = index
                    break
        except (KeyError, TypeError, ValueError, IndexError) as error:
            return None, INTEGRATION_MISMATCH, str(error)
    else:
        return None, REFERENCE_LIMITATION, "sample phase is unsupported"
    if selected_index is None or len(candidates) - selected_index < minimum:
        return None, INTEGRATION_MISMATCH, "declared post-event sample window is absent"

    try:
        ball = _ball(candidates[selected_index], selection["ball_index"])
        if metric in {
                "post_collision_linear_velocity_cm_s",
                "cushion_rebound_speed_cm_s"}:
            actual = ball["speed_cm_s"]
        elif metric == "post_collision_angular_velocity_rad_s":
            angular = _vector(ball["angular_velocity_rad_s"])
            actual = math.sqrt(sum(component * component for component in angular))
        elif metric == "separation_angle_degrees":
            other = _ball(candidates[selected_index], selection["other_ball_index"])
            first_velocity = _vector(ball["velocity_cm_s"])
            second_velocity = _vector(other["velocity_cm_s"])
            first = (first_velocity[0], first_velocity[2])
            second = (second_velocity[0], second_velocity[2])
            denominator = math.hypot(*first) * math.hypot(*second)
            if denominator == 0.0:
                return None, INTEGRATION_MISMATCH, "separation angle velocity is zero"
            cosine = max(-1.0, min(1.0, sum(a * b for a, b in zip(first, second)) / denominator))
            actual = math.degrees(math.acos(cosine))
        elif metric in {"cue_scattering_angle_degrees", "object_scattering_angle_degrees"}:
            axis = selection.get("angle_reference_axis")
            orientation = selection.get("positive_orientation")
            if not isinstance(axis, (list, tuple)) or len(axis) != 2 \
                    or orientation not in {"clockwise", "counterclockwise"}:
                return None, REFERENCE_LIMITATION, "scattering-angle axis/orientation is missing"
            velocity = _vector(ball["velocity_cm_s"])
            axis_length = math.hypot(axis[0], axis[1])
            velocity_length = math.hypot(velocity[0], velocity[2])
            if axis_length == 0.0 or velocity_length == 0.0:
                return None, INTEGRATION_MISMATCH, "scattering-angle vector is zero"
            dot = axis[0] * velocity[0] + axis[1] * velocity[2]
            cross = axis[0] * velocity[2] - axis[1] * velocity[0]
            actual = math.degrees(math.atan2(cross, dot))
            if orientation == "clockwise":
                actual = -actual
        elif metric == "stick_slip_classification":
            epsilon = selection.get("stick_slip_epsilon_cm_s")
            if not _finite_number(epsilon) or epsilon < 0.0:
                return None, REFERENCE_LIMITATION, "stick/slip epsilon is missing or invalid"
            relative = _vector(contact["relative_contact_velocity_cm_s"])
            if not all(_finite_number(component) for component in relative):
                return None, NUMERICAL_FAILURE, "relative contact velocity is not finite"
            actual = "stick" if math.sqrt(sum(component * component for component in relative)) <= epsilon else "slip"
        elif metric == "cushion_rebound_angle_degrees":
            velocity = _vector(ball["velocity_cm_s"])
            normal = _vector(contact["normal"])
            normal_component = velocity[0] * normal[0] + velocity[2] * normal[2]
            tangent_component = -velocity[0] * normal[2] + velocity[2] * normal[0]
            # Positive means the rebound travels inward; zero is parallel to the cushion.
            actual = math.degrees(math.atan2(normal_component, abs(tangent_component)))
        else:
            return None, INTEGRATION_MISMATCH, f"experimental metric {metric} is unavailable"
    except (KeyError, TypeError, ValueError, IndexError) as error:
        return None, INTEGRATION_MISMATCH, str(error)
    if metric != "stick_slip_classification" and not _finite_number(actual):
        return actual, NUMERICAL_FAILURE, "experimental observation is not finite"
    return actual, None, None


def _linear_value_at(samples, target_time):
    if len(samples) < 2 or not all(
            _finite_number(value) for sample in samples for value in sample):
        return None
    mean_time = sum(time for time, _ in samples) / len(samples)
    mean_value = sum(value for _, value in samples) / len(samples)
    denominator = sum((time - mean_time) ** 2 for time, _ in samples)
    if denominator <= 0.0:
        return None
    slope = sum(
        (time - mean_time) * (value - mean_value)
        for time, value in samples
    ) / denominator
    return mean_value + slope * (target_time - mean_time)


def _paired_cushion_observation(reference, scenario, frames):
    required = {
        "event_kind", "sample_phase", "ball_index", "minimum_window_ticks",
        "incident_window_ticks", "rebound_window_ticks", "incident_speed_cm_s",
        "incident_speed_tolerance_cm_s", "ball_radius_cm",
        "sidespin_tolerance_rad_s", "pure_roll_tolerance_cm_s",
    }
    selection, code, message = _selection(reference, required)
    if code:
        return None, code, message
    if selection["sample_phase"] != "immediate_post_impact":
        return None, REFERENCE_LIMITATION, "paired cushion source phase is unsupported"
    incident_count = selection["incident_window_ticks"]
    rebound_count = selection["rebound_window_ticks"]
    if not isinstance(incident_count, int) or isinstance(incident_count, bool) \
            or not isinstance(rebound_count, int) or isinstance(rebound_count, bool) \
            or incident_count < selection["minimum_window_ticks"] \
            or rebound_count < selection["minimum_window_ticks"]:
        return None, REFERENCE_LIMITATION, "paired cushion window sizes are invalid"
    event = _contact_index(frames, selection["event_kind"], selection["ball_index"])
    if event is None:
        return None, INTEGRATION_MISMATCH, "declared rail event is absent"
    event_index, contact = event
    if event_index < incident_count or len(frames) - event_index < rebound_count \
            or not _contiguous(frames):
        return None, INTEGRATION_MISMATCH, "paired cushion fit window is incomplete"
    window = frames[event_index - incident_count:event_index + rebound_count]
    rail_contacts = [
        item for frame in window for item in frame.get("contacts", [])
        if item.get("kind") == "rail"
        and selection["ball_index"] in (item.get("first_ball"), item.get("second_ball"))
    ]
    if len(rail_contacts) != 1:
        return None, INTEGRATION_MISMATCH, "paired cushion fit window contains an ambiguous rail event"
    try:
        scenario_ball = _scenario_ball(scenario, selection["ball_index"])
        velocity = _vector(scenario_ball["velocity_cm_s"])
        angular = _vector(scenario_ball["angular_velocity_rad_s"])
        normal = _vector(contact["normal"])
        radius = selection["ball_radius_cm"]
        sidespin_tolerance = selection["sidespin_tolerance_rad_s"]
        roll_tolerance = selection["pure_roll_tolerance_cm_s"]
        if not all(_finite_number(value) for value in velocity + angular + normal) \
                or not _finite_number(radius) or radius <= 0.0 \
                or not _finite_number(sidespin_tolerance) or sidespin_tolerance < 0.0 \
                or not _finite_number(roll_tolerance) or roll_tolerance < 0.0:
            return None, REFERENCE_LIMITATION, "cushion initial-condition metadata is invalid"
        tangent_speed = abs(-velocity[0] * normal[2] + velocity[2] * normal[0])
        slip_speed = math.hypot(
            velocity[0] + radius * angular[2],
            velocity[2] - radius * angular[0],
        )
        if tangent_speed > roll_tolerance or abs(angular[1]) > sidespin_tolerance \
                or slip_speed > roll_tolerance:
            return None, INTEGRATION_MISMATCH, "scenario is not perpendicular pure roll with zero sidespin"
        event_time = frames[event_index]["time_seconds"]
        incident_samples = [
            (frame["time_seconds"], _ball(frame, selection["ball_index"])["speed_cm_s"])
            for frame in frames[event_index - incident_count:event_index]
        ]
        rebound_samples = [
            (frame["time_seconds"], _ball(frame, selection["ball_index"])["speed_cm_s"])
            for frame in frames[event_index:event_index + rebound_count]
        ]
    except (KeyError, TypeError, ValueError, IndexError) as error:
        return None, INTEGRATION_MISMATCH, str(error)
    incident = _linear_value_at(incident_samples, event_time)
    rebound = _linear_value_at(rebound_samples, event_time)
    if incident is None or rebound is None or not _finite_number(incident) or not _finite_number(rebound):
        return None, NUMERICAL_FAILURE, "paired cushion speed fit is non-finite or degenerate"
    expected_incident = selection["incident_speed_cm_s"]
    incident_tolerance = selection["incident_speed_tolerance_cm_s"]
    if not _finite_number(expected_incident) or not _finite_number(incident_tolerance) \
            or incident_tolerance < 0.0:
        return None, REFERENCE_LIMITATION, "declared incident speed is invalid"
    if abs(incident - expected_incident) > incident_tolerance:
        return None, INTEGRATION_MISMATCH, "fitted incident speed disagrees with the declared source case"
    return rebound, None, None


def _reference_observation(observed_metric, reference, scenario, frames):
    if observed_metric in {"rolling_deceleration_cm_s2", "sliding_deceleration_cm_s2"}:
        return _deceleration_observation(reference, frames)
    if observed_metric == "cushion_rebound_speed_cm_s" \
            and isinstance(reference.get("selection"), dict) \
            and "incident_window_ticks" in reference["selection"]:
        return _paired_cushion_observation(reference, scenario, frames)
    if observed_metric in _EXPERIMENTAL_METRICS:
        return _event_observation(observed_metric, reference, frames)
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
    elif metric == "stick_slip_classification":
        if not isinstance(expected, dict) or expected.get("expected") not in {"stick", "slip"}:
            return False, None, REFERENCE_LIMITATION, "stick/slip expected class is missing"
        actual, failure_code, message = _reference_observation(
            metric, expected, scenario or {}, frames)
        if failure_code is not None:
            return False, actual, failure_code, message
        return actual == expected["expected"], actual, MODEL_MISMATCH, "stick/slip class differs"
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
        point_id = None
        if metric == "value_within_interval" and isinstance(expectation.get("value"), dict):
            point_id = expectation["value"].get("point_id")
            if isinstance(point_id, str) and point_id:
                metrics[point_id] = actual
        if not passed:
            failures.append(Failure(
                code, result_metric, message, expectation.get("value"), actual,
                point_id))
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
