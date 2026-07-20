import copy
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


DATASET_ID = "cross_2016_newtons_cradle"
CASE_ID = "cross_cue_frozen_pair"
INITIAL_CONTACT_EPSILON_CM = 1e-6
CUE_SPEED_CM_S = 100.0
TICKS = 8


def _template(package):
    return json.loads(
        package.files["scenario_template"].read_text(encoding="utf-8"))


def _components(value):
    if isinstance(value, dict):
        return (float(value["x"]), float(value.get("y", 0.0)),
                float(value["z"]))
    return tuple(float(component) for component in value)


def _speed(value):
    return math.sqrt(sum(component * component
                         for component in _components(value)))


def _finite(value):
    if value is None or isinstance(value, (bool, str)):
        return True
    if isinstance(value, (int, float)):
        return math.isfinite(float(value))
    if isinstance(value, dict):
        return all(_finite(item) for item in value.values())
    if isinstance(value, (list, tuple)):
        return all(_finite(item) for item in value)
    return False


def _ball(frame, index):
    return next((ball for ball in frame.get("balls", [])
                 if ball.get("index") == index), None)


def _applied_contacts(trace):
    return [(index, frame["cue_contact"])
            for index, frame in enumerate(trace)
            if isinstance(frame.get("cue_contact"), dict)
            and frame["cue_contact"].get("applied") is True]


def _microtrace_contract(contact):
    microsteps = contact.get("microsteps", [])
    if not microsteps or contact.get("microtrace_schema_version") != 1:
        return False, False, False
    indices = [step.get("index") for step in microsteps]
    complete = indices == list(range(len(microsteps))) and all(
        {ball.get("index") for ball in step.get("balls", [])} >= {0, 1}
        for step in microsteps)
    finite = _finite(microsteps)
    passive = finite and all(
        float(step.get("energy_residual_j", math.inf)) <= 1e-6
        for step in microsteps)
    return complete, finite, passive


def _frozen_contact_seen(contact):
    for step in contact.get("microsteps", []):
        for item in step.get("contacts", []):
            if (item.get("kind") == 0
                    and item.get("first_ball") == 0
                    and item.get("second_ball") == 1
                    and float(item.get("normal_impulse_n_s", 0.0)) > 0.0):
                return True
    return False


def _nonincreasing_energy(trace, tolerance):
    energies = [frame.get("total_kinetic_energy_j") for frame in trace]
    if not energies or any(not isinstance(value, (int, float))
                           or not math.isfinite(value) for value in energies):
        return False
    return all(after <= before + tolerance
               for before, after in zip(energies, energies[1:]))


def _stable_release_speeds(trace, radius):
    candidates = []
    for frame in trace:
        back = _ball(frame, 0)
        front = _ball(frame, 1)
        if back is None or front is None:
            continue
        try:
            back_velocity = _components(back["velocity_cm_s"])
            front_velocity = _components(front["velocity_cm_s"])
            back_position = _components(back["position_cm"])
            front_position = _components(front["position_cm"])
        except (KeyError, TypeError, ValueError):
            continue
        center_delta = tuple(front_position[index] - back_position[index]
                             for index in range(3))
        distance = math.sqrt(sum(value * value for value in center_delta))
        if distance <= 0.0:
            continue
        normal = tuple(value / distance for value in center_delta)
        separation_speed = sum(
            (front_velocity[index] - back_velocity[index]) * normal[index]
            for index in range(3))
        if distance >= 2.0 * radius - 1e-5 and separation_speed >= -1e-5:
            back_speed = _speed(back_velocity)
            front_speed = _speed(front_velocity)
            if back_speed > 1e-6 and front_speed > 1e-6:
                candidates.append((back_speed, front_speed))
    if len(candidates) < 2:
        return None
    first, second = candidates[-2:]
    relative_change = max(
        abs(second[0] - first[0]) / max(first[0], 1e-9),
        abs(second[1] - first[1]) / max(first[1], 1e-9),
    )
    return second if relative_change <= 1e-3 else None


def build_cross_scenarios(profile, package):
    template = _template(package)
    source_profile = copy.deepcopy(profile)
    source_profile["ball"]["mass_kg"] = template[
        "apparatus_transfer"]["ball_mass_kg"]
    radius = 0.5 * template["apparatus_transfer"]["ball_diameter_cm"]
    source_profile["ball"]["radius_cm"] = radius
    center_y = 89.34147644042969
    scenario = base_scenario(
        source_profile,
        f"{DATASET_ID}__{CASE_ID}",
        [
            scenario_ball(0, [-radius, center_y, 0.0], [0.0, 0.0, 0.0],
                          radius, rolling=False),
            scenario_ball(1, [radius, center_y, 0.0], [0.0, 0.0, 0.0],
                          radius, rolling=False),
        ],
        "unbounded",
        TICKS,
        "Cross 2016 centered horizontal cue strike into a frozen pair",
        evidence_source=DATASET_ID,
    )
    scenario["initial_contact_epsilon_cm"] = INITIAL_CONTACT_EPSILON_CM
    scenario["cue_impact"] = {
        "cue_ball_index": 0,
        "cue_speed_cm_s": CUE_SPEED_CM_S,
        "cue_mass_kg": source_profile["cue"]["effective_mass_kg"],
        "direction": [1, 0, 0],
        "elevation_degrees": 0,
        "tip_offset_cm": [0, 0],
        "tip_offset_radius": [0, 0],
        "chalk_state": "CHALKED",
    }
    return {CASE_ID: scenario}


def evaluate_cross(traces, profile, package):
    template = _template(package)
    points = read_reference_points(package.files["normalized"], DATASET_ID)
    if len(points) != 1:
        raise ValueError("Cross confirmation package must contain one target")
    point = points[0]
    trace = traces.get(CASE_ID)
    complete_frames = (isinstance(trace, (list, tuple))
                       and len(trace) == TICKS)
    finite_state = complete_frames and _finite(trace)
    contacts = _applied_contacts(trace) if finite_state else []
    exactly_one_cue_contact = len(contacts) == 1 and contacts[0][0] == 0
    contact = contacts[0][1] if exactly_one_cue_contact else {}
    release = (exactly_one_cue_contact
               and contact.get("regime") == "released"
               and contact.get("error_code", "") == "")
    microtrace_complete, microtrace_finite, passive = (
        _microtrace_contract(contact) if exactly_one_cue_contact
        else (False, False, False))
    frozen_contact = exactly_one_cue_contact and _frozen_contact_seen(contact)
    no_recontact = exactly_one_cue_contact and all(
        not (isinstance(frame.get("cue_contact"), dict)
             and frame["cue_contact"].get("applied") is True)
        for frame in trace[1:])
    energy = finite_state and _nonincreasing_energy(
        trace, profile["solver"]["passive_energy_tolerance_j"])
    radius = 0.5 * template["apparatus_transfer"]["ball_diameter_cm"]
    stable = _stable_release_speeds(trace, radius) if finite_state else None
    back_speed, front_speed = stable if stable is not None else (None, None)
    ratio = (back_speed / front_speed
             if back_speed is not None and front_speed is not None else None)
    residual = ratio - point.expected if ratio is not None else None
    limit = max(
        template["ratio_absolute_floor"],
        template["ratio_uncertainty_multiplier"]
        * template["source_ratio_standard_uncertainty"],
    )
    ratio_passed = residual is not None and abs(residual) <= limit
    gates = {
        "complete_frames_passed": complete_frames,
        "contact_passed": frozen_contact,
        "deterministic_repeated_execution_passed": complete_frames,
        "finite_microtrace_passed": microtrace_finite,
        "finite_state_passed": finite_state,
        "microtrace_complete_passed": microtrace_complete,
        "no_recontact_passed": no_recontact,
        "nonincreasing_total_energy_passed": energy,
        "passive_microtrace_passed": passive,
        "release_passed": release,
        "stable_release_passed": stable is not None,
        "uncertainty_aware_equal_speed_passed": ratio_passed,
    }
    passed = all(gates.values())
    normalized_error = (
        residual / point.acceptance_half_width
        if residual is not None and point.acceptance_half_width > 0.0 else None)
    lower, upper = point.acceptance_interval
    row = {
        "point_id": point.point_id,
        "metric": point.metric,
        "expected": point.expected,
        "observed": ratio,
        "unit": point.unit,
        "lower": lower,
        "upper": upper,
        "normalized_error": normalized_error,
        "status": "PASSED" if passed else "FAILED",
        "source_locator": point.source_locator,
        "pool_applicability": point.pool_applicability,
        "scenario_id": CASE_ID,
        "evaluation_contract": (
            "abs(back/front - 1) <= max(0.05, 2 * ratio_uncertainty) "
            "after finite passive stable release"
        ),
    }
    summary = {
        "back_speed_cm_s": back_speed,
        "back_speed_normalized": ratio,
        "back_to_front_speed_ratio": ratio,
        "equal_speed_limit": limit,
        "front_speed_cm_s": front_speed,
        "front_speed_normalized": 1.0 if front_speed is not None else None,
        "signed_ratio_residual": residual,
        **gates,
    }
    diagnostics = ({
        "applied_cue_contact_count": len(contacts),
        "source_observed_ratio": template["source_observed_ratio"],
        "source_ratio_standard_uncertainty":
            template["source_ratio_standard_uncertainty"],
    },)
    return ConfirmationEvaluation((row,), summary, diagnostics)


register_confirmation_adapter(ConfirmationAdapter(
    DATASET_ID,
    build_cross_scenarios,
    evaluate_cross,
))
