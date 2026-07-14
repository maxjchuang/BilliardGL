import copy
import csv
import hashlib
import io
import json
import math
from pathlib import Path

from .reference_package import load_reference_package
from .reference_point import read_reference_points
from .run import _execute_once


METRIC_FIELDS = (
    "point_id", "metric", "expected", "observed", "unit", "lower",
    "upper", "normalized_error", "status", "source_locator",
    "pool_applicability", "scenario_id", "evaluation_contract",
)


def _canonical(document):
    return json.dumps(
        document, ensure_ascii=False, indent=2, sort_keys=True,
        allow_nan=False) + "\n"


def _sha256(path):
    return hashlib.sha256(Path(path).read_bytes()).hexdigest()


def _scenario(profile, scenario_id, balls, boundary_mode, ticks, description):
    return {
        "schema_version": 11,
        "id": scenario_id,
        "description": description,
        "boundary_mode": boundary_mode,
        "physics_profile": copy.deepcopy(profile),
        "balls": balls,
        "simulation": {
            "ticks": ticks,
            "time_step_seconds": profile["solver"]["time_step_seconds"],
        },
        "expectations": [],
        "evidence": {
            "equipment": "WPA_POOL",
            "grade": "B",
            "pool_applicability": "DIRECT",
            "source": "phase3_v2_confirmation_contract",
        },
    }


def _ball(index, position, velocity, radius, rolling=True):
    speed_x = velocity[0]
    spin_z = -speed_x / radius if rolling else 0.0
    return {
        "index": index,
        "position_cm": position,
        "velocity_cm_s": velocity,
        "angular_velocity_rad_s": [0.0, 0.0, spin_z],
        "pocketed": False,
    }


def _confirmation_scenarios(dataset_id, profile, scalars):
    radius = profile["ball"]["radius_cm"]
    center_y = 89.34147644042969
    if dataset_id == "sudo_2002":
        incident = 98.0
        offset = 2.0 * radius * math.sin(math.radians(30.0))
        return {
            "sudo_cushion_low_speed": _scenario(
                profile, "sudo_2002__cushion_low_speed",
                [_ball(0, [45.0, center_y, 20.0], [180.0, 0.0, 0.0], radius)],
                "production_table", 3,
                "Sudo low-speed boundary query at 1.8 m/s"),
            "sudo_cushion_all_speed": _scenario(
                profile, "sudo_2002__cushion_all_speed",
                [_ball(0, [40.0, center_y, 20.0], [250.0, 0.0, 0.0], radius)],
                "production_table", 3,
                "Sudo all-speed query at the frozen rigid-law limit 2.5 m/s"),
            "sudo_oblique_ball_collision": _scenario(
                profile, "sudo_2002__oblique_ball_collision",
                [
                    _ball(0, [-20.0, center_y, offset],
                          [incident, 0.0, 0.0], radius),
                    _ball(1, [0.0, center_y, 0.0],
                          [0.0, 0.0, 0.0], radius),
                ],
                "unbounded", 4,
                "Sudo ball-contact query at 98 cm/s and 30 degree impact geometry"),
        }
    if dataset_id == "derby_fuller_1999":
        incident = scalars["initial_speed"]
        return {
            "derby_head_on_collision": _scenario(
                profile, "derby_fuller_1999__head_on_collision",
                [
                    _ball(0, [-20.0, center_y, 0.0],
                          [incident, 0.0, 0.0], radius),
                    _ball(1, [0.0, center_y, 0.0],
                          [0.0, 0.0, 0.0], radius),
                ],
                "unbounded", 8,
                "Derby-Fuller equal-mass head-on reconstruction at 98 cm/s"),
        }
    raise ValueError(f"unsupported confirmation package: {dataset_id}")


def _execute_deterministically(executable, scenarios, execute_once):
    traces = {}
    for key, scenario in scenarios.items():
        first = execute_once(executable, scenario)
        second = execute_once(executable, scenario)
        if first != second:
            raise RuntimeError(f"confirmation trace is nondeterministic: {key}")
        if len(first) != scenario["simulation"]["ticks"]:
            raise RuntimeError(f"confirmation trace is incomplete: {key}")
        traces[key] = first
    return traces


def _first_contact(trace, kind):
    for frame_index, frame in enumerate(trace):
        for contact in frame.get("contacts", []):
            if contact.get("kind") == kind:
                return frame_index, frame, contact
    raise RuntimeError(f"confirmation trace has no {kind} contact")


def _speed(ball):
    velocity = ball["velocity_cm_s"]
    return math.sqrt(velocity["x"] ** 2 + velocity["z"] ** 2)


def _sudo_predictions(traces, profile):
    _, _, low = _first_contact(traces["sudo_cushion_low_speed"], "rail")
    _, _, all_speed = _first_contact(traces["sudo_cushion_all_speed"], "rail")
    _, collision_frame, collision = _first_contact(
        traces["sudo_oblique_ball_collision"], "ball_ball")
    balls = {ball["index"]: ball for ball in collision_frame["balls"]}
    first = balls[0]["velocity_cm_s"]
    second = balls[1]["velocity_cm_s"]
    first_vector = (first["x"], first["z"])
    second_vector = (second["x"], second["z"])
    denominator = math.hypot(*first_vector) * math.hypot(*second_vector)
    cosine = max(-1.0, min(1.0, (
        first_vector[0] * second_vector[0] +
        first_vector[1] * second_vector[1]) / denominator))
    separation = math.degrees(math.acos(cosine))
    transverse_deficit = abs(first["z"] + second["z"]) / 98.0
    return {
        "cushion_e_low_speed": (low["restitution"],
            "first rail contact from 1.8 m/s rolling incidence"),
        "cushion_e_all_speed": (all_speed["restitution"],
            "first rail contact at frozen 2.5 m/s rigid-law boundary"),
        "cushion_contact_time_plateau": (0.0,
            "instantaneous rigid impulse reports zero modeled contact duration"),
        "ball_ball_e_head_on": (collision["restitution"],
            "frozen ball-ball contact restitution from executable telemetry"),
        "separation_angle_mean": (separation,
            "post-contact velocity separation in the 30 degree query"),
        "transverse_momentum_deficit": (transverse_deficit,
            "absolute post-contact transverse momentum divided by incident momentum"),
    }


def _rolling_transition(trace, ball_index, after_frame):
    for frame in trace[after_frame:]:
        for transition in frame.get("surface_transitions", []):
            if transition["ball_index"] == ball_index and \
                    transition["after"] == "rolling":
                absolute = ((frame["tick"] - 1) * frame["delta_seconds"] +
                            transition["transition_time_seconds"])
                ball = next(ball for ball in frame["balls"]
                            if ball["index"] == ball_index)
                return absolute, _speed(ball)
    frame = trace[after_frame]
    ball = next(ball for ball in frame["balls"] if ball["index"] == ball_index)
    if ball["motion_state"] == "rolling":
        return frame["time_seconds"], _speed(ball)
    raise RuntimeError(f"ball {ball_index} never reaches rolling")


def _derby_predictions(traces, profile, scalars):
    trace = traces["derby_head_on_collision"]
    frame_index, frame, contact = _first_contact(trace, "ball_ball")
    collision_time = ((frame["tick"] - 1) * frame["delta_seconds"] +
                      contact["time_of_impact_seconds"])
    cue_time, cue_speed = _rolling_transition(trace, 0, frame_index)
    target_time, target_speed = _rolling_transition(trace, 1, frame_index)
    balls = {ball["index"]: ball for ball in frame["balls"]}
    mass = profile["ball"]["mass_kg"]
    initial_speed = scalars["initial_speed"]
    momentum_before = mass * initial_speed / 100.0
    momentum_after = mass * (
        _speed(balls[0]) + _speed(balls[1])) / 100.0
    energy_before = 0.5 * mass * (initial_speed / 100.0) ** 2
    energy_after = 0.5 * mass * (
        (_speed(balls[0]) / 100.0) ** 2 +
        (_speed(balls[1]) / 100.0) ** 2)
    return {
        "initial_speed": (initial_speed,
            "source incident speed used as the sole scenario input"),
        "cue_sliding_time": (max(0.0, cue_time - collision_time),
            "time from first ball contact to cue-ball rolling transition"),
        "target_sliding_time": (max(0.0, target_time - collision_time),
            "time from first ball contact to target-ball rolling transition"),
        "cue_final_speed": (cue_speed,
            "cue-ball speed at first rolling transition"),
        "target_final_speed": (target_speed,
            "target-ball speed at first rolling transition"),
        "momentum_before": (momentum_before,
            "incident scalar momentum using frozen production ball mass"),
        "momentum_after": (momentum_after,
            "sum of ball momentum magnitudes in the first post-contact frame"),
        "kinetic_energy_loss": (
            (energy_before - energy_after) / energy_before,
            "fractional translational energy loss in the first post-contact frame"),
    }


def _known_mismatches(package):
    document = json.loads(package.files["expected_model_mismatches"].read_text(
        encoding="utf-8"))
    return {(row["case_id"], row["metric"]) for row in document["failures"]}


def _metric_rows(points, predictions, known):
    rows = []
    for point in points:
        observed, contract = predictions[point.point_id]
        lower, upper = point.acceptance_interval
        normalized_error = ((observed - point.expected) /
                            point.acceptance_half_width)
        if lower <= observed <= upper:
            status = "PASSED"
        elif (point.case_id, point.metric) in known:
            status = "MODEL_MISMATCH_KNOWN"
        else:
            status = "FAILED"
        rows.append({
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
            "scenario_id": point.dataset_id,
            "evaluation_contract": contract,
        })
    return rows


def _csv_bytes(fields, rows):
    stream = io.StringIO(newline="")
    writer = csv.DictWriter(stream, fieldnames=fields, lineterminator="\n")
    writer.writeheader()
    writer.writerows(rows)
    return stream.getvalue().encode("utf-8")


def build_confirmation_result(executable, freeze_path, package_path,
                              repository_root, execute_once=None):
    root = Path(repository_root).resolve()
    freeze = json.loads(Path(freeze_path).read_text(encoding="utf-8"))
    executable = Path(executable).resolve()
    if _sha256(executable) != freeze["executable_sha256"]:
        raise ValueError("confirmation executable does not match frozen SHA-256")
    profile_document = json.loads(
        (root / freeze["profile"]["path"]).read_text(encoding="utf-8"))
    profile = profile_document["runtime_profile"]
    package = load_reference_package(package_path)
    dataset_id = package.manifest["dataset_id"]
    points = read_reference_points(package.files["normalized"], dataset_id)
    with package.files["scalars"].open(encoding="utf-8", newline="") as stream:
        scalar_rows = list(csv.DictReader(stream))
    scalars = {row["point_id"]: float(row["normalized_value"])
               for row in scalar_rows}
    scenarios = _confirmation_scenarios(dataset_id, profile, scalars)
    traces = _execute_deterministically(
        executable, scenarios, execute_once or _execute_once)
    if dataset_id == "sudo_2002":
        predictions = _sudo_predictions(traces, profile)
    else:
        predictions = _derby_predictions(traces, profile, scalars)
    rows = _metric_rows(points, predictions, _known_mismatches(package))
    failed = [row for row in rows if row["status"] == "FAILED"]
    files = {
        "metrics.csv": _csv_bytes(METRIC_FIELDS, rows),
        "source_scalars.csv": package.files["scalars"].read_bytes(),
    }
    for key, scenario in scenarios.items():
        files[f"scenarios/{key}.json"] = _canonical(scenario).encode("utf-8")
        files[f"traces/{key}.json"] = _canonical(traces[key]).encode("utf-8")
        files[f"provenance/{key}.json"] = _canonical({
            "schema_version": 1,
            "candidate_id": freeze["candidate_id"],
            "dataset_id": dataset_id,
            "executable_sha256": freeze["executable_sha256"],
            "freeze_sha256": _sha256(freeze_path),
            "package_manifest_sha256": _sha256(Path(package_path) / "manifest.json"),
            "scenario_id": scenario["id"],
            "source_revision": freeze["source_revision"],
        }).encode("utf-8")
    report = {
        "schema_version": 2,
        "candidate_id": freeze["candidate_id"],
        "dataset_id": dataset_id,
        "dataset_version": package.manifest["dataset_version"],
        "partition": "CONFIRMATION",
        "result": "FAILED" if failed else "PASSED_OR_ACCOUNTED",
        "summary": {
            "points": len(rows),
            "passed": sum(row["status"] == "PASSED" for row in rows),
            "known_model_mismatches": sum(
                row["status"] == "MODEL_MISMATCH_KNOWN" for row in rows),
            "failed": len(failed),
        },
        "points": rows,
        "diagnostics": [row for row in scalar_rows if row["role"] == "diagnostic"],
        "apparatus": [row for row in scalar_rows if row["role"] == "apparatus"],
    }
    files["reference_report.json"] = _canonical(report).encode("utf-8")
    files["diagnostics.csv"] = _csv_bytes(
        tuple(scalar_rows[0]) if scalar_rows else (),
        [row for row in scalar_rows if row["role"] != "confirmation_target"])
    return {"result": report["result"], "files": files}
