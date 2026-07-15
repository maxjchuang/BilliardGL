import json
import math

from .confirmation_adapters import (
    ConfirmationAdapter,
    ConfirmationEvaluation,
    base_scenario,
    register_confirmation_adapter,
    scenario_ball,
)
from .reference_point import read_reference_points


DATASET_ID = "alciatore_2005_tp_a15"
INITIAL_CONTACT_EPSILON_CM = 1e-6
CUE_SPEED_CM_S = 100.0
TICKS = 8


def _template(package):
    return json.loads(
        package.files["scenario_template"].read_text(encoding="utf-8"))


def _components(value):
    if isinstance(value, dict):
        return float(value["x"]), float(value.get("y", 0.0)), float(value["z"])
    return tuple(float(component) for component in value)


def _speed(value):
    x, y, z = _components(value)
    return math.sqrt(x * x + y * y + z * z)


def _finite(value):
    if isinstance(value, bool) or value is None:
        return True
    if isinstance(value, (int, float)):
        return math.isfinite(float(value))
    if isinstance(value, dict):
        return all(_finite(item) for item in value.values())
    if isinstance(value, (list, tuple)):
        return all(_finite(item) for item in value)
    return True


def _nonincreasing_energy(trace, tolerance):
    values = [frame.get("total_kinetic_energy_j") for frame in trace]
    if not values or any(not isinstance(value, (int, float))
                         or not math.isfinite(value) for value in values):
        return False
    return all(after <= before + tolerance
               for before, after in zip(values, values[1:]))


def _ball(frame, index):
    return next((ball for ball in frame.get("balls", [])
                 if ball.get("index") == index), None)


def _first_contact_index(trace):
    for index, frame in enumerate(trace):
        for contact in frame.get("contacts", []):
            if contact.get("kind") == "ball_ball":
                return index, contact
    return None, None


def _valid_contact(contact):
    if not isinstance(contact, dict):
        return False
    try:
        normal = _components(contact["normal"])
        impulse = float(contact["normal_impulse_ns"])
    except (KeyError, TypeError, ValueError):
        return False
    return (_finite(normal) and math.isfinite(impulse) and impulse >= 0.0
            and contact.get("velocity_impulse_applied") is True)


def _incident_speed(trace):
    for frame in trace:
        contact = frame.get("cue_contact")
        if isinstance(contact, dict) and contact.get("applied") is True:
            try:
                value = _speed(contact["ball_velocity_after_cm_s"])
            except (KeyError, TypeError, ValueError):
                return None
            return value if math.isfinite(value) and value > 0.0 else None
    return None


def _stable_velocity(trace, ball_index, start_index=0):
    candidates = []
    for frame in trace[start_index:]:
        ball = _ball(frame, ball_index)
        if ball is None:
            candidates.append(None)
            continue
        try:
            velocity = _components(ball["velocity_cm_s"])
            speed = _speed(velocity)
        except (KeyError, TypeError, ValueError):
            candidates.append(None)
            continue
        candidates.append(velocity if _finite(velocity) and speed > 1e-6 else None)
    for first, second in zip(candidates, candidates[1:]):
        if first is None or second is None:
            continue
        first_angle = math.degrees(math.atan2(first[2], first[0]))
        second_angle = math.degrees(math.atan2(second[2], second[0]))
        if abs(first_angle - second_angle) <= 0.1:
            return first
    return None


def _row(point, observed, status, contract):
    lower, upper = point.acceptance_interval
    normalized_error = None
    if observed is not None and point.acceptance_half_width > 0.0:
        normalized_error = (
            (observed - point.expected) / point.acceptance_half_width)
    return {
        "point_id": point.point_id,
        "metric": point.metric,
        "expected": point.expected,
        "observed": observed,
        "unit": point.unit,
        "lower": lower,
        "upper": upper,
        "normalized_error": normalized_error,
        "status": status,
        "source_locator": point.source_locator,
        "pool_applicability": point.pool_applicability,
        "scenario_id": point.point_id,
        "evaluation_contract": contract,
    }


def build_alciatore_scenarios(profile, package):
    template = _template(package)
    radius = profile["ball"]["radius_cm"]
    center_y = 89.34147644042969
    scenarios = {}
    for point_id, case in sorted(template["cases"].items()):
        phi = math.radians(case["cut_angle_phi_degrees"])
        scenario = base_scenario(
            profile,
            f"{DATASET_ID}__{point_id}",
            [
                scenario_ball(0, [-radius, center_y, 0.0], [0.0, 0.0, 0.0],
                              radius, rolling=False),
                scenario_ball(1, [radius, center_y, 0.0], [0.0, 0.0, 0.0],
                              radius, rolling=False),
            ],
            "unbounded",
            TICKS,
            ("Alciatore frozen cue-ball shot at "
             f"phi={case['cut_angle_phi_degrees']} degrees"),
            evidence_source=DATASET_ID,
        )
        scenario["initial_contact_epsilon_cm"] = INITIAL_CONTACT_EPSILON_CM
        scenario["cue_impact"] = {
            "cue_ball_index": 0,
            "cue_speed_cm_s": CUE_SPEED_CM_S,
            "cue_mass_kg": profile["cue"]["effective_mass_kg"],
            "direction": [math.cos(phi), 0.0, math.sin(phi)],
            "elevation_degrees": 0,
            "tip_offset_cm": [0, 0],
            "tip_offset_radius": [0, 0],
            "chalk_state": "CHALKED",
        }
        scenarios[point_id] = scenario
    return scenarios


def evaluate_alciatore(traces, profile, package):
    template = _template(package)
    points = read_reference_points(package.files["normalized"], DATASET_ID)
    points_by_id = {point.point_id: point for point in points}
    energy_tolerance = profile["solver"]["passive_energy_tolerance_j"]
    finite_state = True
    energy_ok = True
    contact_complete = True
    rows = []
    diagnostics = []
    interior_errors = []
    head_on_lateral_ratio = None
    head_on_direction_error = None
    grazing_object_speed_ratio = None

    for point_id, case in sorted(template["cases"].items()):
        point = points_by_id[point_id]
        trace = traces.get(point_id)
        phi = case["cut_angle_phi_degrees"]
        case_finite = isinstance(trace, (list, tuple)) and bool(trace) \
            and _finite(trace)
        finite_state = finite_state and case_finite
        case_energy = case_finite and _nonincreasing_energy(
            trace, energy_tolerance)
        energy_ok = energy_ok and case_energy
        contact_index, contact = (
            _first_contact_index(trace) if case_finite else (None, None))
        contact_required = phi < 90
        case_contact = (not contact_required) or _valid_contact(contact)
        contact_complete = contact_complete and case_contact
        incident = _incident_speed(trace) if case_finite else None
        start = contact_index if contact_index is not None else 0
        cue_velocity = (_stable_velocity(trace, 0, start)
                        if case_finite else None)
        observed = None
        if cue_velocity is not None:
            observed = math.degrees(math.atan2(cue_velocity[2], cue_velocity[0]))
            if observed < 0.0:
                observed += 360.0
        error = observed - point.expected if observed is not None else None
        case_passed = (case_finite and case_energy and case_contact
                       and incident is not None and error is not None)
        if case["evaluation_role"] == "interior_angle":
            if error is not None:
                interior_errors.append(error)
            case_passed = case_passed and abs(error) <= template[
                "interior_absolute_error_degrees_maximum"]
        elif phi == 0:
            if cue_velocity is not None and incident:
                head_on_lateral_ratio = abs(cue_velocity[2]) / incident
                head_on_direction_error = abs(error)
            case_passed = case_passed and head_on_lateral_ratio is not None \
                and head_on_lateral_ratio <= template[
                    "head_on_lateral_to_incident_speed_ratio_maximum"] \
                and head_on_direction_error <= template[
                    "head_on_direction_error_degrees_maximum"]
        else:
            object_ball = _ball(trace[0], 1) if case_finite else None
            if object_ball is not None and incident:
                try:
                    grazing_object_speed_ratio = (
                        _speed(object_ball["velocity_cm_s"]) / incident)
                except (KeyError, TypeError, ValueError):
                    grazing_object_speed_ratio = None
            case_passed = case_passed and grazing_object_speed_ratio is not None \
                and grazing_object_speed_ratio <= template[
                    "grazing_target_to_incident_speed_ratio_maximum"]
        rows.append(_row(
            point, observed, "PASSED" if case_passed else "FAILED",
            ("earliest two-frame-stable separating cue-ball direction; "
             "endpoint speed ratios use post-step ball velocities"),
        ))
        diagnostics.append({
            "contact_required": contact_required,
            "contact_valid": case_contact,
            "cut_angle_phi_degrees": phi,
            "incident_speed_cm_s": incident,
            "point_id": point_id,
            "signed_error_degrees": error,
        })

    if len(interior_errors) == 7:
        rmse = math.sqrt(sum(error * error for error in interior_errors) / 7)
        maximum = max(abs(error) for error in interior_errors)
    else:
        rmse = None
        maximum = None
    rmse_limit = template["interior_rmse_degrees_maximum"]
    maximum_limit = template["interior_absolute_error_degrees_maximum"]
    lateral_limit = template[
        "head_on_lateral_to_incident_speed_ratio_maximum"]
    direction_limit = template["head_on_direction_error_degrees_maximum"]
    grazing_limit = template[
        "grazing_target_to_incident_speed_ratio_maximum"]
    summary = {
        "contact_complete_passed": contact_complete,
        "finite_state_passed": finite_state,
        "grazing_object_speed_ratio": grazing_object_speed_ratio,
        "grazing_object_speed_ratio_maximum": grazing_limit,
        "grazing_object_speed_ratio_passed": (
            grazing_object_speed_ratio is not None
            and grazing_object_speed_ratio <= grazing_limit),
        "head_on_direction_error_degrees": head_on_direction_error,
        "head_on_direction_error_degrees_maximum": direction_limit,
        "head_on_direction_passed": (
            head_on_direction_error is not None
            and head_on_direction_error <= direction_limit),
        "head_on_lateral_ratio": head_on_lateral_ratio,
        "head_on_lateral_ratio_maximum": lateral_limit,
        "head_on_lateral_ratio_passed": (
            head_on_lateral_ratio is not None
            and head_on_lateral_ratio <= lateral_limit),
        "interior_maximum_absolute_error_degrees": maximum,
        "interior_maximum_absolute_error_degrees_maximum": maximum_limit,
        "interior_maximum_passed": (
            maximum is not None and maximum <= maximum_limit),
        "interior_rmse_degrees": rmse,
        "interior_rmse_degrees_maximum": rmse_limit,
        "interior_rmse_passed": rmse is not None and rmse <= rmse_limit,
        "nonincreasing_total_energy_passed": energy_ok,
    }
    return ConfirmationEvaluation(tuple(rows), summary, tuple(diagnostics))


register_confirmation_adapter(ConfirmationAdapter(
    DATASET_ID, build_alciatore_scenarios, evaluate_alciatore))
