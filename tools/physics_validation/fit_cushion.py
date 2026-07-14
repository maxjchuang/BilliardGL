import argparse
import csv
import json
import math
from pathlib import Path


_SOURCE_REPORTED_FRICTION = 0.14
_SOURCE_REPORTED_RESTITUTION = 0.98
_NOSE_HEIGHT_RATIO = 1.4
_MAXIMUM_RIGID_INCIDENT_SPEED_CM_S = 250.0


def _canonical(document):
    return json.dumps(document, ensure_ascii=False, indent=2, sort_keys=True,
                      allow_nan=False) + "\n"


def read_incident_inputs(path):
    required = {"point_id", "incident_speed_m_s", "fit_subset",
                "rigid_cushion_domain", "status"}
    rows = {}
    with Path(path).open("r", encoding="utf-8", newline="") as source:
        reader = csv.DictReader(source)
        if not required <= set(reader.fieldnames or ()):
            raise ValueError("Mathavan raw extraction lacks cushion fit columns")
        for line_number, row in enumerate(reader, start=2):
            if row["point_id"] in rows:
                raise ValueError(f"duplicate cushion input {row['point_id']}")
            if row["status"] != "ADMITTED":
                raise ValueError(f"line {line_number}: cushion point is not admitted")
            speed = float(row["incident_speed_m_s"]) * 100.0
            if not math.isfinite(speed) or speed <= 0.0:
                raise ValueError(f"line {line_number}: incident speed is invalid")
            rows[row["point_id"]] = {
                "fit_subset": row["fit_subset"] == "true",
                "incident_speed_cm_s": speed,
                "rigid_cushion_domain": row["rigid_cushion_domain"] == "true",
            }
    return dict(sorted(rows.items()))


def _prediction(incident_speed_cm_s, restitution):
    # For perpendicular pure roll at h/R=7/5, the raised-nose normal effective
    # mass reduces the standalone solver exactly to |v_after| = e |v_before|.
    return restitution * incident_speed_cm_s


def _objective(points, inputs, restitution):
    residuals = [
        _prediction(inputs[point.point_id]["incident_speed_cm_s"], restitution)
        - point.expected
        for point in points
    ]
    return sum(value * value for value in residuals) / len(residuals)


def fit_cushion_parameters(points, split, inputs):
    points = tuple(sorted(points, key=lambda point: point.point_id))
    if {point.point_id for point in points} != set(inputs):
        raise ValueError("Mathavan normalized points and cushion inputs differ")
    calibration = tuple(
        point for point in points
        if split.partition_for(point) == "CALIBRATION")
    holdout = tuple(
        point for point in points if split.partition_for(point) == "HOLDOUT")
    if not calibration or any(not inputs[p.point_id]["fit_subset"] for p in calibration):
        raise ValueError("cushion calibration partition is not inside the source fit subset")
    numerator = sum(
        inputs[p.point_id]["incident_speed_cm_s"] * p.expected
        for p in calibration)
    denominator = sum(
        inputs[p.point_id]["incident_speed_cm_s"] ** 2
        for p in calibration)
    restitution = min(1.0, max(0.0, numerator / denominator))
    friction = _SOURCE_REPORTED_FRICTION
    objective = _objective(calibration, inputs, restitution)
    sensitivity = []
    for candidate_e, candidate_mu in (
            (max(0.0, restitution - 0.01), friction),
            (min(1.0, restitution + 0.01), friction),
            (restitution, max(0.0, friction - 0.01)),
            (restitution, min(1.0, friction + 0.01))):
        sensitivity.append({
            "friction_coefficient": candidate_mu,
            "normal_restitution": candidate_e,
            "objective_mean_squared_cm_s2":
                _objective(calibration, inputs, candidate_e),
        })
    return {
        "calibration_point_ids": [p.point_id for p in calibration],
        "excluded_holdout_point_ids": [p.point_id for p in holdout],
        "friction_coefficient": friction,
        "friction_identifiability":
            "not identifiable from perpendicular zero-sidespin markers; source-reported value retained",
        "normal_restitution": restitution,
        "objective_mean_squared_cm_s2": objective,
        "sensitivity": sensitivity,
    }


def build_fit_report(points, split, inputs):
    return {
        "algorithm": {
            "model": "constant rigid-cushion response at h/R=7/5",
            "normal_restitution_solution": "bounded least-squares closed form",
            "objective": "unweighted_mean_squared_rebound_speed_residual_cm_s2",
            "friction_tie_break": "source_reported_value_due_to_experimental_nonidentifiability",
        },
        "dataset_id": "mathavan_2010_cushion",
        "fit_partition": "CALIBRATION",
        "fit": fit_cushion_parameters(points, split, inputs),
        "parameter_bounds": {
            "friction_coefficient": [0.0, 1.0],
            "normal_restitution": [0.0, 1.0],
        },
        "physical_constants": {
            "maximum_rigid_incident_speed_cm_s": _MAXIMUM_RIGID_INCIDENT_SPEED_CM_S,
            "nose_height_ratio": _NOSE_HEIGHT_RATIO,
        },
        "source_reported_sensitivity_center": {
            "friction_coefficient": _SOURCE_REPORTED_FRICTION,
            "normal_restitution": _SOURCE_REPORTED_RESTITUTION,
            "used_as_experimental_expected_values": False,
        },
        "schema_version": 1,
    }


def main(argv=None):
    parser = argparse.ArgumentParser()
    parser.add_argument("--package", required=True)
    parser.add_argument("--output", required=True)
    arguments = parser.parse_args(argv)
    from .reference_package import load_reference_package
    from .reference_point import read_reference_points
    from .reference_split import load_reference_split
    package = load_reference_package(arguments.package)
    points = read_reference_points(
        package.files["normalized"], package.manifest["dataset_id"])
    split = load_reference_split(
        package.files["split"], points, package.manifest["dataset_id"],
        package.manifest["dataset_version"])
    inputs = read_incident_inputs(package.files["raw_extracted"])
    output = Path(arguments.output)
    output.parent.mkdir(parents=True, exist_ok=True)
    output.write_text(_canonical(build_fit_report(points, split, inputs)),
                      encoding="utf-8")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
