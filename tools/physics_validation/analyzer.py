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
    "trajectory_position_rmse_mm",
    "stopping_time_seconds",
    "transition_to_rolling_time_seconds",
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
    "cue_impact_linear_speed_cm_s",
    "cue_impact_angular_speed_rad_s",
}


def _trajectory_observation(reference, frames):
    required = {
        "sample_phase", "ball_index", "minimum_window_ticks",
        "reference_positions_cm",
    }
    selection, code, message = _selection(reference, required)
    if code:
        return None, code, message
    samples = selection["reference_positions_cm"]
    if selection["sample_phase"] != "declared_trajectory_ticks" \
            or not isinstance(samples, list) \
            or len(samples) < selection["minimum_window_ticks"]:
        return None, REFERENCE_LIMITATION, "trajectory reference samples are invalid"
    by_tick = {frame.get("tick"): frame for frame in frames}
    squared_errors = []
    try:
        for sample in samples:
            if not isinstance(sample, dict) or set(sample) != {"tick", "position_cm"}:
                return None, REFERENCE_LIMITATION, "trajectory sample fields are invalid"
            tick = sample["tick"]
            expected = _vector(sample["position_cm"])
            if not isinstance(tick, int) or isinstance(tick, bool) or len(expected) != 3 \
                    or not all(_finite_number(value) for value in expected):
                return None, REFERENCE_LIMITATION, "trajectory sample values are invalid"
            actual = _vector(_ball(by_tick[tick], selection["ball_index"])["position_cm"])
            if len(actual) != 3:
                return None, INTEGRATION_MISMATCH, "trajectory position dimension is invalid"
            if not all(_finite_number(value) for value in actual):
                return None, NUMERICAL_FAILURE, "trajectory position is not finite"
            squared_errors.append(sum((a - b) ** 2 for a, b in zip(actual, expected)))
    except (KeyError, TypeError, ValueError) as error:
        return None, INTEGRATION_MISMATCH, str(error)
    return math.sqrt(sum(squared_errors) / len(squared_errors)) * 10.0, None, None


def _transition_time_observation(metric, reference, frames):
    required = {
        "sample_phase", "ball_index", "minimum_window_ticks", "time_origin_seconds",
    }
    selection, code, message = _selection(reference, required)
    if code:
        return None, code, message
    origin = selection["time_origin_seconds"]
    if not _finite_number(origin):
        return None, REFERENCE_LIMITATION, "transition time origin is invalid"
    if not _contiguous(frames):
        return None, INTEGRATION_MISMATCH, "transition trace timing is invalid"
    if metric == "stopping_time_seconds":
        threshold = selection.get("speed_threshold_cm_s")
        if selection["sample_phase"] != "first_stable_stop" \
                or not _finite_number(threshold) or threshold < 0.0:
            return None, REFERENCE_LIMITATION, "stable-stop selection is invalid"
        criterion = lambda ball: _finite_number(ball.get("speed_cm_s")) \
            and ball["speed_cm_s"] <= threshold
    else:
        radius = selection.get("ball_radius_cm")
        tolerance = selection.get("pure_roll_tolerance_cm_s")
        if selection["sample_phase"] != "first_stable_pure_roll" \
                or not _finite_number(radius) or radius <= 0.0 \
                or not _finite_number(tolerance) or tolerance < 0.0:
            return None, REFERENCE_LIMITATION, "pure-roll transition selection is invalid"
        criterion = lambda ball: _is_pure_roll(ball, radius, tolerance)
    minimum = selection["minimum_window_ticks"]
    try:
        for frame in frames:
            if not _finite_number(frame.get("time_seconds")):
                return None, NUMERICAL_FAILURE, "transition sample time is not finite"
            ball = _ball(frame, selection["ball_index"])
            if metric == "stopping_time_seconds":
                if not _finite_number(ball.get("speed_cm_s")):
                    return None, NUMERICAL_FAILURE, "stopping speed is not finite"
            else:
                velocity = _vector(ball["velocity_cm_s"])
                angular = _vector(ball["angular_velocity_rad_s"])
                if not all(_finite_number(value) for value in velocity + angular):
                    return None, NUMERICAL_FAILURE, "pure-roll state is not finite"
                _is_pure_roll(ball, radius, tolerance)
        for index in range(len(frames) - minimum + 1):
            window = frames[index:index + minimum]
            if all(criterion(_ball(frame, selection["ball_index"])) for frame in window):
                actual = window[0]["time_seconds"] - origin
                if not _finite_number(actual):
                    return actual, NUMERICAL_FAILURE, "transition time is not finite"
                return actual, None, None
    except _NonfiniteSurfaceState as error:
        return None, NUMERICAL_FAILURE, str(error)
    except _SurfaceStateMismatch as error:
        return None, INTEGRATION_MISMATCH, str(error)
    except (KeyError, TypeError, ValueError) as error:
        return None, INTEGRATION_MISMATCH, str(error)
    return None, INTEGRATION_MISMATCH, "declared stable transition window is absent"


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


def _validate_ball_contact(frames, contact):
    required = {
        "first_ball", "second_ball", "normal",
        "relative_contact_velocity_before_cm_s",
        "relative_contact_velocity_after_cm_s",
        "normal_relative_speed_before_cm_s",
        "normal_relative_speed_after_cm_s", "normal_impulse_ns",
        "tangential_impulse_ns", "friction_coefficient", "regime",
        "velocity_impulse_applied", "kinetic_energy_before_j",
        "kinetic_energy_after_j",
    }
    if not required <= set(contact):
        return INTEGRATION_MISMATCH, "ball contact diagnostics are incomplete"
    if not _finite(contact):
        return NUMERICAL_FAILURE, "ball contact diagnostics are not finite"
    try:
        normal = _vector(contact["normal"])
        before = _vector(contact["relative_contact_velocity_before_cm_s"])
        after = _vector(contact["relative_contact_velocity_after_cm_s"])
    except (KeyError, TypeError, ValueError) as error:
        return INTEGRATION_MISMATCH, str(error)
    if len(normal) != 3 or len(before) != 3 or len(after) != 3:
        return INTEGRATION_MISMATCH, "ball contact vectors must have three components"
    normal_length = math.sqrt(sum(component * component for component in normal))
    if abs(normal_length - 1.0) > 1e-5:
        return INTEGRATION_MISMATCH, "ball contact normal is not unit length"
    normal_before = sum(value * axis for value, axis in zip(before, normal))
    normal_after = sum(value * axis for value, axis in zip(after, normal))
    reported_before = contact["normal_relative_speed_before_cm_s"]
    reported_after = contact["normal_relative_speed_after_cm_s"]
    if abs(normal_before - reported_before) > 1e-4 \
            or abs(normal_after - reported_after) > 1e-4:
        return INTEGRATION_MISMATCH, \
            "reported normal relative speed disagrees with contact velocity"

    normal_impulse = contact["normal_impulse_ns"]
    tangent_impulse = contact["tangential_impulse_ns"]
    friction = contact["friction_coefficient"]
    if normal_impulse < 0.0 or tangent_impulse < 0.0 or friction < 0.0:
        return INTEGRATION_MISMATCH, "ball contact impulse or friction is negative"
    if tangent_impulse > friction * normal_impulse + 1e-10:
        return INTEGRATION_MISMATCH, "ball contact violates the friction cone"

    applied = contact["velocity_impulse_applied"]
    if not isinstance(applied, bool):
        return INTEGRATION_MISMATCH, "velocity_impulse_applied must be boolean"
    if applied and (normal_before >= -1e-6 or normal_after < -1e-5):
        return INTEGRATION_MISMATCH, \
            "ball contact does not transition from approach to separation"
    if not applied and (normal_impulse > 1e-12 or tangent_impulse > 1e-12):
        return INTEGRATION_MISMATCH, \
            "ball contact reports impulse without applying it"

    energy_before = contact["kinetic_energy_before_j"]
    energy_after = contact["kinetic_energy_after_j"]
    if energy_before < 0.0 or energy_after < 0.0 \
            or energy_after > energy_before + max(1e-9, abs(energy_before) * 1e-7):
        return NUMERICAL_FAILURE, "ball contact creates kinetic energy"

    regime = contact["regime"]
    if regime not in {"no_contact", "separating", "frictionless", "stick", "slip"}:
        return INTEGRATION_MISMATCH, "ball contact regime is invalid"
    tangent_after = math.sqrt(sum(
        (value - normal_after * axis) ** 2
        for value, axis in zip(after, normal)))
    if regime == "stick" and tangent_after > 1e-3:
        return INTEGRATION_MISMATCH, "stick regime retains tangential contact motion"
    if regime == "frictionless" and tangent_impulse > 1e-12:
        return INTEGRATION_MISMATCH, "frictionless regime has tangential impulse"

    pair = frozenset((contact["first_ball"], contact["second_ball"]))
    applied_count = sum(
        1 for frame in frames for item in frame.get("contacts", [])
        if item.get("kind") == "ball_ball"
        and frozenset((item.get("first_ball"), item.get("second_ball"))) == pair
        and item.get("velocity_impulse_applied") is True)
    if applied_count > 1:
        return INTEGRATION_MISMATCH, "duplicate ball velocity impulse is present"
    return None, None


def _validate_cushion_contact(contact):
    required = {
        "first_ball", "second_ball", "normal", "contact_tangent",
        "contact_arm_cm", "contact_height_cm",
        "contact_velocity_before_cm_s", "contact_velocity_after_cm_s",
        "normal_relative_speed_before_cm_s", "normal_relative_speed_after_cm_s",
        "normal_impulse_ns", "tangential_impulse_ns", "impulse_on_ball_ns",
        "friction_coefficient", "restitution", "nose_height_ratio", "regime",
        "velocity_impulse_applied", "kinetic_energy_before_j",
        "kinetic_energy_after_j", "position_correction_cm", "position_corrected",
        "incident_speed_cm_s", "maximum_rigid_incident_speed_cm_s",
        "rigid_domain_exceeded", "time_of_impact_seconds",
    }
    if not required <= set(contact):
        return INTEGRATION_MISMATCH, "cushion contact diagnostics are incomplete"
    if not _finite(contact):
        return NUMERICAL_FAILURE, "cushion contact diagnostics are not finite"
    try:
        normal = _vector(contact["normal"])
        tangent = _vector(contact["contact_tangent"])
        arm = _vector(contact["contact_arm_cm"])
        before = _vector(contact["contact_velocity_before_cm_s"])
        after = _vector(contact["contact_velocity_after_cm_s"])
        impulse = _vector(contact["impulse_on_ball_ns"])
        correction = _vector(contact["position_correction_cm"])
    except (KeyError, TypeError, ValueError) as error:
        return INTEGRATION_MISMATCH, str(error)
    if any(len(value) != 3 for value in
           (normal, tangent, arm, before, after, impulse, correction)):
        return INTEGRATION_MISMATCH, "cushion contact vectors must have three components"
    if abs(math.sqrt(sum(value * value for value in normal)) - 1.0) > 1e-5 \
            or abs(sum(a * b for a, b in zip(normal, tangent))) > 1e-5:
        return INTEGRATION_MISMATCH, "cushion contact basis is invalid"
    normal_before = sum(value * axis for value, axis in zip(before, normal))
    normal_after = sum(value * axis for value, axis in zip(after, normal))
    if abs(normal_before - contact["normal_relative_speed_before_cm_s"]) > 1e-4 \
            or abs(normal_after - contact["normal_relative_speed_after_cm_s"]) > 1e-4:
        return INTEGRATION_MISMATCH, \
            "reported cushion normal speed disagrees with contact velocity"
    applied = contact["velocity_impulse_applied"]
    corrected = contact["position_corrected"]
    exceeded = contact["rigid_domain_exceeded"]
    if not isinstance(applied, bool) or not isinstance(corrected, bool) \
            or not isinstance(exceeded, bool):
        return INTEGRATION_MISMATCH, "cushion diagnostic flags must be boolean"
    normal_impulse = contact["normal_impulse_ns"]
    tangent_impulse = contact["tangential_impulse_ns"]
    friction = contact["friction_coefficient"]
    if min(normal_impulse, tangent_impulse, friction) < 0.0:
        return INTEGRATION_MISMATCH, "cushion impulse or friction is negative"
    if tangent_impulse > friction * normal_impulse + 1e-10:
        return INTEGRATION_MISMATCH, "cushion contact violates the friction cone"
    if applied and (normal_before >= -1e-6 or normal_after < -1e-5):
        return INTEGRATION_MISMATCH, \
            "cushion contact does not transition from approach to separation"
    if not applied and (normal_impulse > 1e-12 or tangent_impulse > 1e-12):
        return INTEGRATION_MISMATCH, "cushion reports an unapplied impulse"
    incident = contact["incident_speed_cm_s"]
    maximum = contact["maximum_rigid_incident_speed_cm_s"]
    if incident < 0.0 or maximum <= 0.0 \
            or abs(incident - max(0.0, -normal_before)) > 1e-4:
        return INTEGRATION_MISMATCH, "cushion incident-speed diagnostics disagree"
    if exceeded != (incident > maximum + 1e-10):
        return INTEGRATION_MISMATCH, "cushion rigid-domain label is inconsistent"
    if contact["time_of_impact_seconds"] < 0.0:
        return INTEGRATION_MISMATCH, "cushion time of impact is negative"
    energy_before = contact["kinetic_energy_before_j"]
    energy_after = contact["kinetic_energy_after_j"]
    if energy_before < 0.0 or energy_after < 0.0 \
            or energy_after > energy_before + max(1e-9, abs(energy_before) * 1e-7):
        return NUMERICAL_FAILURE, "cushion contact creates kinetic energy"
    if contact["regime"] not in {
            "no_contact", "separating", "frictionless", "stick", "slip"}:
        return INTEGRATION_MISMATCH, "cushion contact regime is invalid"
    return None, None


class _NonfiniteSurfaceState(Exception):
    pass


class _SurfaceStateMismatch(Exception):
    pass


def _is_pure_roll(ball, radius, tolerance):
    velocity = _vector(ball["velocity_cm_s"])
    angular = _vector(ball["angular_velocity_rad_s"])
    slip_x = velocity[0] + radius * angular[2]
    slip_z = velocity[2] - radius * angular[0]
    slip = math.hypot(slip_x, slip_z)
    if not all(_finite_number(value) for value in velocity + angular) \
            or not _finite_number(slip):
        raise _NonfiniteSurfaceState("surface kinematics are not finite")
    reported_slip = ball.get("contact_slip_speed_cm_s")
    if reported_slip is not None:
        if not _finite_number(reported_slip):
            raise _NonfiniteSurfaceState("contact slip speed is not finite")
        if abs(reported_slip - slip) > max(tolerance, 1e-6):
            raise _SurfaceStateMismatch(
                "reported contact slip disagrees with velocity and spin")
    rotational_energy = ball.get("rotational_kinetic_energy_j")
    if rotational_energy is not None and not _finite_number(rotational_energy):
        raise _NonfiniteSurfaceState("rotational kinetic energy is not finite")
    state = ball.get("motion_state")
    if state is None:
        return slip <= tolerance
    if state not in {"stationary", "sliding", "rolling"}:
        raise _SurfaceStateMismatch("motion state is invalid")
    if state == "rolling":
        if slip > tolerance:
            raise _SurfaceStateMismatch(
                "rolling motion state has nonzero contact slip")
        return True
    if state == "sliding" and slip <= tolerance:
        raise _SurfaceStateMismatch(
            "sliding motion state has rolling contact kinematics")
    return False


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
    if contact.get("kind") == "rail":
        code, message = _validate_cushion_contact(contact)
        if code:
            return None, code, message
    elif contact.get("kind") == "ball_ball":
        code, message = _validate_ball_contact(frames, contact)
        if code:
            return None, code, message
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
        except _NonfiniteSurfaceState as error:
            return None, NUMERICAL_FAILURE, str(error)
        except _SurfaceStateMismatch as error:
            return None, INTEGRATION_MISMATCH, str(error)
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
            relative = _vector(contact["relative_contact_velocity_after_cm_s"])
            if not all(_finite_number(component) for component in relative):
                return None, NUMERICAL_FAILURE, "relative contact velocity is not finite"
            actual = contact["regime"]
            if actual not in {"stick", "slip"}:
                return None, INTEGRATION_MISMATCH, \
                    "stick/slip metric has incompatible contact regime"
            normal = _vector(contact["normal"])
            normal_component = sum(
                component * axis for component, axis in zip(relative, normal))
            tangent_speed = math.sqrt(sum(
                (component - normal_component * axis) ** 2
                for component, axis in zip(relative, normal)))
            derived = "stick" if tangent_speed <= epsilon else "slip"
            if derived != actual:
                return None, INTEGRATION_MISMATCH, \
                    "contact regime disagrees with post-impact tangential speed"
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


def _cue_impact_observation(metric, reference, frames):
    required = {
        "sample_phase", "source_phase", "ball_index", "minimum_window_ticks",
        "input_support", "stability_tolerance",
    }
    selection, code, message = _selection(reference, required)
    if code:
        return None, code, message
    if selection["sample_phase"] != "first_stable_frames_after_cue_impact" \
            or not isinstance(selection["source_phase"], str) \
            or not selection["source_phase"]:
        return None, REFERENCE_LIMITATION, "cue-impact source/sample phase is invalid"
    if selection["input_support"] is not True:
        return None, REFERENCE_LIMITATION, "requested physical cue input is unsupported"
    tolerance = selection["stability_tolerance"]
    if not _finite_number(tolerance) or tolerance < 0.0:
        return None, REFERENCE_LIMITATION, "cue-impact stability tolerance is invalid"
    if not frames or not all(isinstance(frame.get("cue_impact"), dict) for frame in frames):
        return None, INTEGRATION_MISMATCH, "requested cue-impact input trace is absent"
    minimum = selection["minimum_window_ticks"]
    if not _contiguous(frames):
        return None, INTEGRATION_MISMATCH, "trace ticks are not contiguous"
    try:
        values = []
        for frame in frames:
            ball = _ball(frame, selection["ball_index"])
            if metric == "cue_impact_linear_speed_cm_s":
                value = ball["speed_cm_s"]
            else:
                axis = selection.get("angular_axis")
                sign = selection.get("angular_sign")
                if axis not in {"x", "y", "z"} or sign not in {-1, 1}:
                    return None, REFERENCE_LIMITATION, "angular axis/sign is missing or invalid"
                value = sign * _vector(ball["angular_velocity_rad_s"])[
                    {"x": 0, "y": 1, "z": 2}[axis]]
            if not _finite_number(value):
                return value, NUMERICAL_FAILURE, "cue-impact output is not finite"
            values.append(value)
    except (KeyError, TypeError, ValueError) as error:
        return None, INTEGRATION_MISMATCH, str(error)
    for index in range(len(values) - minimum + 1):
        window = values[index:index + minimum]
        if max(window) - min(window) <= tolerance:
            return sum(window) / len(window), None, None
    return None, INTEGRATION_MISMATCH, "stable cue-impact output window is absent"


def _cue_contact_observation(metric, reference, frames):
    selection = reference.get("selection")
    if not isinstance(selection, dict):
        return None, REFERENCE_LIMITATION, "cue-contact selection metadata is absent"
    expected_regime = selection.get("expected_regime")
    if expected_regime not in {"stick", "slip", "miscue", "unsupported"}:
        return None, REFERENCE_LIMITATION, "expected cue-contact regime is invalid"
    contacts = [frame.get("cue_contact") for frame in frames
                if isinstance(frame.get("cue_contact"), dict)]
    if len(contacts) != 1:
        return None, INTEGRATION_MISMATCH, (
            "cue contact must appear on exactly one trace frame")
    contact = contacts[0]
    regime = contact.get("regime")
    if regime not in {"stick", "slip", "miscue", "unsupported"}:
        return None, INTEGRATION_MISMATCH, "cue-contact regime is invalid"
    if regime == "unsupported":
        return None, REFERENCE_LIMITATION, (
            contact.get("error_code") or "requested cue contact was not applied")
    required = {
        "friction_coefficient", "normal_impulse_ns", "tangential_impulse_ns",
        "input_kinetic_energy_j", "output_kinetic_energy_j",
    }
    if not required <= set(contact):
        return None, INTEGRATION_MISMATCH, "cue-contact telemetry fields are incomplete"
    friction = contact["friction_coefficient"]
    normal = contact["normal_impulse_ns"]
    tangent = contact["tangential_impulse_ns"]
    input_energy = contact["input_kinetic_energy_j"]
    output_energy = contact["output_kinetic_energy_j"]
    if not all(_finite_number(value) for value in
               (friction, normal, tangent, input_energy, output_energy)):
        return None, NUMERICAL_FAILURE, "cue-contact impulse or energy is not finite"
    if friction < 0.0 or normal < 0.0 or tangent < 0.0:
        return None, INTEGRATION_MISMATCH, "cue-contact impulse signs are invalid"
    if regime == "miscue":
        if contact.get("applied", False) or normal != 0.0 or tangent != 0.0:
            return None, INTEGRATION_MISMATCH, "miscue must apply no ball impulse"
        if regime != expected_regime:
            return regime, MODEL_MISMATCH, "cue-contact regime differs from reference"
        if metric == "cue_contact_energy_efficiency":
            if input_energy <= 0.0:
                return None, INTEGRATION_MISMATCH, "cue-contact input energy is not positive"
            return output_energy / input_energy, None, None
        return None, INTEGRATION_MISMATCH, f"miscue metric {metric} is unavailable"
    cone_limit = friction * normal
    cone_tolerance = max(1e-12, abs(cone_limit) * 1e-9)
    if tangent > cone_limit + cone_tolerance:
        return None, INTEGRATION_MISMATCH, "cue-contact impulse exceeds friction cone"
    if regime == "slip" and abs(tangent - cone_limit) > cone_tolerance:
        return None, INTEGRATION_MISMATCH, "slip label disagrees with friction-cone clamp"
    if regime != expected_regime:
        return regime, MODEL_MISMATCH, "cue-contact regime differs from reference"
    if metric == "cue_contact_normal_impulse_ns":
        return normal, None, None
    if metric == "cue_contact_tangential_impulse_ns":
        return tangent, None, None
    if metric == "cue_contact_energy_efficiency":
        if input_energy <= 0.0:
            return None, INTEGRATION_MISMATCH, "cue-contact input energy is not positive"
        return output_energy / input_energy, None, None
    return None, INTEGRATION_MISMATCH, f"cue-contact metric {metric} is unavailable"


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
    code, message = _validate_cushion_contact(contact)
    if code:
        return None, code, message
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
        step_start_time = frames[event_index - 1]["time_seconds"]
        event_time = step_start_time + contact["time_of_impact_seconds"]
        if event_time < step_start_time - 1e-12 \
                or event_time > frames[event_index]["time_seconds"] + 1e-12:
            return None, INTEGRATION_MISMATCH, \
                "cushion time of impact lies outside its telemetry step"
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
    if observed_metric == "trajectory_position_rmse_mm":
        return _trajectory_observation(reference, frames)
    if observed_metric in {"stopping_time_seconds", "transition_to_rolling_time_seconds"}:
        return _transition_time_observation(observed_metric, reference, frames)
    if observed_metric in {"rolling_deceleration_cm_s2", "sliding_deceleration_cm_s2"}:
        return _deceleration_observation(reference, frames)
    if observed_metric == "cushion_rebound_speed_cm_s" \
            and isinstance(reference.get("selection"), dict) \
            and "incident_window_ticks" in reference["selection"]:
        return _paired_cushion_observation(reference, scenario, frames)
    if observed_metric in {
            "cue_impact_linear_speed_cm_s", "cue_impact_angular_speed_rad_s"}:
        return _cue_impact_observation(observed_metric, reference, frames)
    if observed_metric in {
            "cue_contact_normal_impulse_ns",
            "cue_contact_tangential_impulse_ns",
            "cue_contact_energy_efficiency"}:
        return _cue_contact_observation(observed_metric, reference, frames)
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
    elif metric == "cue_impact_input_round_trip":
        requested = (scenario or {}).get("cue_impact")
        if not isinstance(requested, dict):
            return False, None, REFERENCE_LIMITATION, "scenario cue-impact input is missing"
        expected_trace = dict(requested)
        direction = requested.get("direction")
        if not isinstance(direction, list) or len(direction) != 3:
            return False, None, REFERENCE_LIMITATION, "scenario cue direction is invalid"
        expected_trace["direction"] = {
            "x": direction[0], "y": direction[1], "z": direction[2]}
        actual = all(frame.get("cue_impact") == expected_trace for frame in frames)
        passed = actual == expected
        return passed, actual, INTEGRATION_MISMATCH, "cue-impact input changed across state/trace"
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
