import copy
import csv
import hashlib
import io
import json
import math
from pathlib import Path

from .confirmation_adapters import (
    ConfirmationAdapter,
    ConfirmationEvaluation,
    base_scenario,
    execute_deterministically,
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
        cue_contact = frame.get("cue_contact", {})
        for microstep in cue_contact.get("microsteps", []):
            for contact in microstep.get("contacts", []):
                if contact.get("kind") == 0 and contact.get("second_ball", -1) >= 0:
                    impulse = contact.get("normal_impulse_n_s", 0.0)
                    return index, {
                        "kind": "ball_ball",
                        "normal": contact.get("normal"),
                        "normal_impulse_ns": impulse,
                        "velocity_impulse_applied": impulse > 0.0,
                    }
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
                microsteps = contact.get("microsteps", [])
                coupled_speeds = []
                for step in microsteps:
                    cue_ball = next((ball for ball in step.get("balls", [])
                                     if ball.get("index") == 0), None)
                    if cue_ball is not None:
                        coupled_speeds.append(_speed(cue_ball["velocity_cm_s"]))
                if coupled_speeds and max(coupled_speeds) > 0.0:
                    return max(coupled_speeds)
                value = _speed(contact["ball_velocity_after_cm_s"])
            except (KeyError, TypeError, ValueError):
                return None
            if math.isfinite(value) and value > 0.0:
                return value
            cue_impact = frame.get("cue_impact", {})
            fallback = cue_impact.get("cue_speed_cm_s")
            return (float(fallback) if isinstance(fallback, (int, float))
                    and math.isfinite(fallback) and fallback > 0.0 else None)
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
        candidates.append(velocity if _finite(velocity) and math.isfinite(speed)
                          else None)
    for first, second in zip(candidates, candidates[1:]):
        if first is None or second is None:
            continue
        first_speed = _speed(first)
        second_speed = _speed(second)
        if first_speed <= 1e-6 and second_speed <= 1e-6:
            if max(abs(a - b) for a, b in zip(first, second)) <= 1e-6:
                return second
            continue
        if first_speed <= 1e-6 or second_speed <= 1e-6:
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
        if "frozen_cue_contact" in profile:
            scenario["schema_version"] = 12
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
    head_on_cue_residual_speed_ratio = None
    head_on_cue_lateral_speed_ratio = None
    grazing_target_speed_ratio = None

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
        error = None
        if (case["evaluation_role"] == "interior_angle"
                and cue_velocity is not None
                and _speed(cue_velocity) > 1e-6):
            observed = math.degrees(math.atan2(cue_velocity[2], cue_velocity[0]))
            if observed < 0.0:
                observed += 360.0
            error = observed - point.expected
        case_passed = (case_finite and case_energy and case_contact
                       and incident is not None)
        if case["evaluation_role"] == "interior_angle":
            if error is not None:
                interior_errors.append(error)
            case_passed = case_passed and error is not None and abs(error) <= \
                template["interior_absolute_error_degrees_maximum"]
        elif phi == 0:
            if cue_velocity is not None and incident:
                head_on_cue_residual_speed_ratio = (
                    _speed(cue_velocity) / incident)
                head_on_cue_lateral_speed_ratio = (
                    abs(cue_velocity[2]) / incident)
            endpoint_limit = template[
                "head_on_lateral_to_incident_speed_ratio_maximum"]
            case_passed = case_passed \
                and head_on_cue_residual_speed_ratio is not None \
                and head_on_cue_lateral_speed_ratio is not None \
                and head_on_cue_lateral_speed_ratio <= endpoint_limit
        else:
            target_velocity = (_stable_velocity(trace, 1, start)
                               if case_finite else None)
            if target_velocity is not None and incident:
                grazing_target_speed_ratio = _speed(target_velocity) / incident
            case_passed = case_passed and grazing_target_speed_ratio is not None \
                and grazing_target_speed_ratio <= template[
                    "grazing_target_to_incident_speed_ratio_maximum"]
        rows.append(_row(
            point, observed, "PASSED" if case_passed else "FAILED",
            ("interior: earliest two-frame-stable separating cue-ball direction; "
             "endpoints: speed ratios only, undefined stopped-ball angle is null"),
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
    grazing_limit = template[
        "grazing_target_to_incident_speed_ratio_maximum"]
    summary = {
        "contact_complete_passed": contact_complete,
        "finite_state_passed": finite_state,
        "grazing_target_speed_ratio": grazing_target_speed_ratio,
        "grazing_target_speed_ratio_maximum": grazing_limit,
        "grazing_target_speed_ratio_passed": (
            grazing_target_speed_ratio is not None
            and grazing_target_speed_ratio <= grazing_limit),
        "head_on_cue_lateral_speed_ratio": head_on_cue_lateral_speed_ratio,
        "head_on_cue_lateral_speed_ratio_maximum": lateral_limit,
        "head_on_cue_lateral_speed_ratio_passed": (
            head_on_cue_lateral_speed_ratio is not None
            and head_on_cue_lateral_speed_ratio <= lateral_limit),
        "head_on_cue_residual_speed_ratio":
            head_on_cue_residual_speed_ratio,
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


def phase3_v5_regression_profile(v4_document, fit_document):
    profile = copy.deepcopy(v4_document["runtime_profile"])
    winner = fit_document["winner"]
    fixed = fit_document["fixed"]
    profile["id"] = "phase3_integrated_v5"
    profile["formula_version"] = "phase3_integrated_v5"
    profile["frozen_cue_contact"] = {
        "enabled": True,
        "normal_stiffness_n_per_m32": winner["stiffness_n_per_m32"],
        "normal_dissipation_s_per_m": winner["dissipation_s_per_m"],
        "tangential_stiffness_n_per_m":
            fixed["tangential_stiffness_n_per_m"],
        "tangential_damping_ns_per_m":
            fixed["tangential_damping_n_s_per_m"],
        "microstep_seconds": 0.0000025,
        "maximum_contact_seconds": 0.006,
        "release_compression_m": 0.00000001,
        "maximum_compression_m": 0.004,
        "maximum_normal_force_n": 10000.0,
    }
    return profile


def _canonical_bytes(document):
    return (json.dumps(document, ensure_ascii=False, indent=2,
                       sort_keys=True, allow_nan=False) + "\n").encode("utf-8")


def _csv_bytes(header, rows):
    stream = io.StringIO(newline="")
    writer = csv.writer(stream, lineterminator="\n")
    writer.writerow(header)
    writer.writerows(rows)
    return stream.getvalue().encode("utf-8")


def _evaluation_passed(evaluation):
    return (all(row["status"] == "PASSED" for row in evaluation.rows)
            and all(value for key, value in evaluation.summary_metrics.items()
                    if key.endswith("_passed")))


def build_alciatore_v5_artifacts(executable, profile, package,
                                  execute_once=None):
    if execute_once is None:
        from .run import _execute_once
        execute_once = _execute_once

    scenarios = build_alciatore_scenarios(profile, package)
    traces = execute_deterministically(executable, scenarios, execute_once)
    evaluation = evaluate_alciatore(traces, profile, package)
    template = _template(package)
    diagnostics = {row["point_id"]: row for row in evaluation.diagnostics}

    input_header = (
        "point_id", "cut_angle_phi_degrees", "cue_speed_cm_s",
        "normal_stiffness_n_per_m32", "normal_dissipation_s_per_m",
        "microstep_seconds", "partition")
    settings = profile["frozen_cue_contact"]
    input_rows = []
    for point_id, case in sorted(template["cases"].items()):
        input_rows.append((
            point_id, case["cut_angle_phi_degrees"], CUE_SPEED_CM_S,
            settings["normal_stiffness_n_per_m32"],
            settings["normal_dissipation_s_per_m"],
            settings["microstep_seconds"], "spent_regression"))

    residual_header = (
        "point_id", "evaluation_role", "expected_angle_degrees",
        "observed_angle_degrees", "signed_error_degrees",
        "normalized_error", "head_on_cue_residual_speed_ratio",
        "head_on_cue_lateral_speed_ratio", "grazing_target_speed_ratio",
        "status")
    residual_rows = []
    for row in evaluation.rows:
        point_id = row["point_id"]
        role = template["cases"][point_id]["evaluation_role"]
        residual_rows.append((
            point_id, role, row["expected"], row["observed"],
            diagnostics[point_id]["signed_error_degrees"],
            row["normalized_error"],
            (evaluation.summary_metrics["head_on_cue_residual_speed_ratio"]
             if point_id == "alciatore_cut_000" else ""),
            (evaluation.summary_metrics["head_on_cue_lateral_speed_ratio"]
             if point_id == "alciatore_cut_000" else ""),
            (evaluation.summary_metrics["grazing_target_speed_ratio"]
             if point_id == "alciatore_cut_090" else ""),
            row["status"],
        ))

    sensitivity_header = (
        "parameter", "scale", "normal_stiffness_n_per_m32",
        "normal_dissipation_s_per_m", "interior_rmse_degrees",
        "interior_maximum_absolute_error_degrees",
        "head_on_cue_residual_speed_ratio",
        "head_on_cue_lateral_speed_ratio", "grazing_target_speed_ratio",
        "passed")
    sensitivity_rows = []
    for parameter in ("normal_stiffness_n_per_m32",
                      "normal_dissipation_s_per_m"):
        for scale in (0.5, 1.0, 2.0):
            varied = copy.deepcopy(profile)
            varied["id"] = f"phase3_integrated_v5_sensitivity_{parameter}_{scale:g}"
            varied["frozen_cue_contact"][parameter] *= scale
            varied_scenarios = build_alciatore_scenarios(varied, package)
            varied_traces = execute_deterministically(
                executable, varied_scenarios, execute_once)
            varied_evaluation = evaluate_alciatore(
                varied_traces, varied, package)
            summary = varied_evaluation.summary_metrics
            sensitivity_rows.append((
                parameter, scale,
                varied["frozen_cue_contact"]["normal_stiffness_n_per_m32"],
                varied["frozen_cue_contact"]["normal_dissipation_s_per_m"],
                summary["interior_rmse_degrees"],
                summary["interior_maximum_absolute_error_degrees"],
                summary["head_on_cue_residual_speed_ratio"],
                summary["head_on_cue_lateral_speed_ratio"],
                summary["grazing_target_speed_ratio"],
                _evaluation_passed(varied_evaluation),
            ))

    trace_hashes = {
        point_id: "sha256:" + hashlib.sha256(
            _canonical_bytes(trace)).hexdigest()
        for point_id, trace in sorted(traces.items())
    }
    report = {
        "dataset_id": DATASET_ID,
        "dataset_version": "1.0.0",
        "deterministic_double_execution": True,
        "evaluation_rows": list(evaluation.rows),
        "fit_role": "spent_regression",
        "profile_id": profile["id"],
        "schema_version": 1,
        "summary_metrics": evaluation.summary_metrics,
        "trace_sha256": trace_hashes,
    }
    return {
        "alciatore_frozen_contact_v5_inputs.csv":
            _csv_bytes(input_header, input_rows),
        "alciatore_frozen_contact_v5_residuals.csv":
            _csv_bytes(residual_header, residual_rows),
        "alciatore_frozen_contact_v5_sensitivity.csv":
            _csv_bytes(sensitivity_header, sensitivity_rows),
        "alciatore_frozen_contact_v5_report.json": _canonical_bytes(report),
    }


def main(argv=None):
    import argparse
    from .reference_package import load_reference_package
    parser = argparse.ArgumentParser()
    parser.add_argument("--executable", type=Path, required=True)
    parser.add_argument("--v4-profile", type=Path, required=True)
    parser.add_argument("--fit", type=Path, required=True)
    parser.add_argument("--package", type=Path, required=True)
    parser.add_argument("--output", type=Path, required=True)
    arguments = parser.parse_args(argv)
    profile = phase3_v5_regression_profile(
        json.loads(arguments.v4_profile.read_text(encoding="utf-8")),
        json.loads(arguments.fit.read_text(encoding="utf-8")))
    package = load_reference_package(arguments.package)
    artifacts = build_alciatore_v5_artifacts(
        arguments.executable, profile, package)
    arguments.output.mkdir(parents=True, exist_ok=True)
    for name, data in artifacts.items():
        (arguments.output / name).write_bytes(data)
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
