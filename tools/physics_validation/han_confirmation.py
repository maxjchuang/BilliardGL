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


DATASET_ID = "han_2005"
TICKS = 3
RAIL_MARGIN_CM = 0.01


def _template(package):
    return json.loads(
        package.files["scenario_template"].read_text(encoding="utf-8"))


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


def _energy_ok(trace, tolerance):
    values = [frame.get("total_kinetic_energy_j") for frame in trace]
    if not values or any(not isinstance(value, (int, float))
                         or not math.isfinite(value) for value in values):
        return False
    return all(after <= before + tolerance
               for before, after in zip(values, values[1:]))


def _first_rail_contact(trace):
    for frame in trace:
        for contact in frame.get("contacts", []):
            if contact.get("kind") == "rail":
                return contact
    return None


def _observed_restitution(contact):
    if not isinstance(contact, dict):
        return None
    try:
        before = float(contact["normal_relative_speed_before_cm_s"])
        after = float(contact["normal_relative_speed_after_cm_s"])
    except (KeyError, TypeError, ValueError):
        return None
    if not math.isfinite(before) or not math.isfinite(after) or before == 0.0:
        return None
    value = abs(after / before)
    return value if math.isfinite(value) else None


def _row(point, observed, normalized_error, status):
    lower, upper = point.acceptance_interval
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
        "evaluation_contract": (
            "absolute restitution is diagnostic and transfer-limited; "
            "acceptance uses the normalized five-speed curve and hard invariants"
        ),
    }


def build_han_scenarios(profile, package):
    template = _template(package)
    radius = profile["ball"]["radius_cm"]
    x_limit = (0.5 * profile["table_boundary"]["playfield_width_cm"]
               - radius)
    center_y = 89.34147644042969
    scenarios = {}
    for point_id, case in sorted(template["cases"].items()):
        speed_cm_s = case["incident_normal_speed_m_s"] * 100.0
        scenario = base_scenario(
            profile,
            f"{DATASET_ID}__{point_id}",
            [scenario_ball(
                0, [x_limit - RAIL_MARGIN_CM, center_y, 20.0],
                [speed_cm_s, 0.0, 0.0], radius)],
            "production_table",
            TICKS,
            ("Han normalized cushion restitution at "
             f"{case['incident_normal_speed_m_s']} m/s"),
            evidence_source=DATASET_ID,
        )
        scenario["evidence"]["pool_applicability"] = "TRANSFER_LIMITED"
        scenarios[point_id] = scenario
    return scenarios


def evaluate_han(traces, profile, package):
    template = _template(package)
    points = read_reference_points(package.files["normalized"], DATASET_ID)
    points_by_id = {point.point_id: point for point in points}
    tolerance = profile["solver"]["passive_energy_tolerance_j"]
    incident_relative_tolerance = template[
        "source_domain"]["incident_speed_relative_tolerance"]
    observations = {}
    case_validity = {}
    diagnostics = []
    finite_bounded = True
    source_domain = True
    energy_passed = True

    for point_id, case in sorted(template["cases"].items()):
        trace = traces.get(point_id)
        case_finite = isinstance(trace, (list, tuple)) and bool(trace) \
            and _finite(trace)
        contact = _first_rail_contact(trace) if case_finite else None
        observed = _observed_restitution(contact)
        bounded = observed is not None and 0.0 <= observed <= 1.0
        commanded = case["incident_normal_speed_m_s"] * 100.0
        try:
            incident = float(contact["incident_speed_cm_s"])
            domain_ok = (math.isfinite(incident)
                         and abs(incident - commanded) <=
                         incident_relative_tolerance * commanded
                         and contact.get("rigid_domain_exceeded") is False)
        except (TypeError, KeyError, ValueError):
            incident = None
            domain_ok = False
        case_energy = case_finite and _energy_ok(trace, tolerance)
        finite_bounded = finite_bounded and case_finite and bounded
        source_domain = source_domain and domain_ok
        energy_passed = energy_passed and case_energy
        observations[point_id] = observed
        case_validity[point_id] = (
            case_finite and bounded and domain_ok and case_energy)
        diagnostics.append({
            "commanded_incident_speed_cm_s": commanded,
            "contact_incident_speed_cm_s": incident,
            "observed_absolute_restitution": observed,
            "point_id": point_id,
        })

    base_id = "han_speed_050"
    observed_base = observations.get(base_id)
    expected_base = points_by_id[base_id].expected
    complete = (set(traces) == set(template["cases"])
                and observed_base is not None and observed_base > 0.0)
    observed_normalized = {}
    expected_normalized = {}
    if complete and all(value is not None for value in observations.values()):
        for point_id, observed in observations.items():
            observed_normalized[point_id] = observed / observed_base
            expected_normalized[point_id] = (
                points_by_id[point_id].expected / expected_base)
        errors = [observed_normalized[point_id]
                  - expected_normalized[point_id]
                  for point_id in sorted(template["cases"])]
        rmse = math.sqrt(sum(error * error for error in errors) / len(errors))
        adjacent_changes = [
            abs(observed_normalized[after] - observed_normalized[before])
            for before, after in zip(
                sorted(template["cases"]), sorted(template["cases"])[1:])
        ]
        maximum_adjacent_change = max(adjacent_changes)
    else:
        rmse = None
        maximum_adjacent_change = None

    rmse_limit = template["normalized_curve_rmse_maximum"]
    continuity_limit = template["continuity"][
        "maximum_adjacent_normalized_change"]
    continuity_passed = (maximum_adjacent_change is not None
                         and maximum_adjacent_change <= continuity_limit)
    rows = []
    for point_id in sorted(template["cases"]):
        point = points_by_id[point_id]
        normalized_error = None
        if point_id in observed_normalized:
            normalized_error = (
                observed_normalized[point_id] - expected_normalized[point_id])
        rows.append(_row(
            point,
            observations[point_id],
            normalized_error,
            "PASSED" if case_validity[point_id] else "FAILED",
        ))
    hard_metrics = [metric for metric in template["hard_metrics"]
                    if metric != "normalized_curve_rmse"]
    summary = {
        "continuous_response_maximum_adjacent_change": maximum_adjacent_change,
        "continuous_response_maximum_adjacent_change_limit": continuity_limit,
        "continuous_response_passed": continuity_passed,
        "finite_bounded_response_passed": finite_bounded,
        "hard_metrics": hard_metrics,
        "nonincreasing_total_energy_passed": energy_passed,
        "normalized_curve_rmse": rmse,
        "normalized_curve_rmse_maximum": rmse_limit,
        "normalized_curve_rmse_passed": rmse is not None and rmse <= rmse_limit,
        "source_domain_response_passed": source_domain and complete,
    }
    return ConfirmationEvaluation(tuple(rows), summary, tuple(diagnostics))


register_confirmation_adapter(ConfirmationAdapter(
    DATASET_ID, build_han_scenarios, evaluate_han))
