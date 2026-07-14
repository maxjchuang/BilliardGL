import argparse
import csv
import json
import math
from dataclasses import dataclass
from pathlib import Path


_SOURCE_REPORTED_FRICTION = 0.14
_SOURCE_REPORTED_RESTITUTION = 0.98
_NOSE_HEIGHT_RATIO = 1.4
_MAXIMUM_RIGID_INCIDENT_SPEED_CM_S = 250.0
_V2_DATASETS = {"mathavan_2009_high_speed", "mathavan_2010_cushion"}
_V2_INPUT_FIELDS = (
    "point_id", "dataset_id", "dataset_version", "lifecycle", "series_id",
    "incident_speed_m_s", "expected_rebound_cm_s",
    "standard_uncertainty_cm_s", "rigid_cushion_domain", "source_locator",
    "normalized_path", "normalized_sha256", "raw_extracted_path",
    "raw_extracted_sha256",
)


@dataclass(frozen=True)
class CushionFitPoint:
    point_id: str
    dataset_id: str
    dataset_version: str
    lifecycle: str
    series_id: str
    incident_speed_m_s: float
    expected_rebound_cm_s: float
    standard_uncertainty_cm_s: float
    rigid_cushion_domain: bool
    source_locator: str
    normalized_path: str
    normalized_sha256: str
    raw_extracted_path: str
    raw_extracted_sha256: str


@dataclass(frozen=True)
class CushionFit:
    objective: float
    e_intercept: float
    e_slope: float
    e_min: float
    e_max: float
    residuals: tuple

    def restitution(self, speed_m_s):
        return restitution(
            speed_m_s, self.e_intercept, self.e_slope, self.e_min, self.e_max)


def restitution(speed_m_s, intercept, slope, minimum, maximum):
    return min(maximum, max(minimum, intercept - slope * speed_m_s))


def fit_key(value):
    return (value.objective, value.e_intercept, value.e_slope,
            value.e_min, value.e_max)


def _canonical(document):
    return json.dumps(document, ensure_ascii=False, indent=2, sort_keys=True,
                      allow_nan=False) + "\n"


def _finite(row, field, line_number):
    try:
        value = float(row[field])
    except ValueError as error:
        raise ValueError(f"line {line_number}: {field} must be numeric") from error
    if not math.isfinite(value):
        raise ValueError(f"line {line_number}: {field} must be finite")
    return value


def _read_v2_incident_inputs(path):
    rows = []
    seen = set()
    with Path(path).open("r", encoding="utf-8", newline="") as source:
        reader = csv.DictReader(source)
        if tuple(reader.fieldnames or ()) != _V2_INPUT_FIELDS:
            raise ValueError("cushion v2 input header does not match schema version 1")
        for line_number, row in enumerate(reader, start=2):
            point_id = row["point_id"]
            if not point_id or point_id in seen:
                raise ValueError(f"line {line_number}: point_id must be nonempty and unique")
            seen.add(point_id)
            if row["dataset_id"] not in _V2_DATASETS:
                raise ValueError(f"line {line_number}: confirmation or unknown dataset")
            if row["lifecycle"] != "spent":
                raise ValueError(f"line {line_number}: cushion fit input must be spent")
            speed = _finite(row, "incident_speed_m_s", line_number)
            expected = _finite(row, "expected_rebound_cm_s", line_number)
            uncertainty = _finite(row, "standard_uncertainty_cm_s", line_number)
            if speed <= 0.0 or expected < 0.0 or uncertainty <= 0.0:
                raise ValueError(f"line {line_number}: cushion numeric domain is invalid")
            if row["rigid_cushion_domain"] not in {"true", "false"}:
                raise ValueError(f"line {line_number}: rigid domain flag is invalid")
            digests = (row["normalized_sha256"], row["raw_extracted_sha256"])
            if any(not digest.startswith("sha256:") or len(digest) != 71
                   for digest in digests):
                raise ValueError(f"line {line_number}: evidence digest is invalid")
            if not all((row["dataset_version"], row["series_id"],
                        row["source_locator"], row["normalized_path"],
                        row["raw_extracted_path"])):
                raise ValueError(f"line {line_number}: cushion provenance is incomplete")
            rows.append(CushionFitPoint(
                point_id=point_id,
                dataset_id=row["dataset_id"],
                dataset_version=row["dataset_version"],
                lifecycle=row["lifecycle"],
                series_id=row["series_id"],
                incident_speed_m_s=speed,
                expected_rebound_cm_s=expected,
                standard_uncertainty_cm_s=uncertainty,
                rigid_cushion_domain=row["rigid_cushion_domain"] == "true",
                source_locator=row["source_locator"],
                normalized_path=row["normalized_path"],
                normalized_sha256=row["normalized_sha256"],
                raw_extracted_path=row["raw_extracted_path"],
                raw_extracted_sha256=row["raw_extracted_sha256"],
            ))
    if not rows or [row.point_id for row in rows] != sorted(seen):
        raise ValueError("cushion v2 inputs must be nonempty and sorted")
    if {row.dataset_id for row in rows} != _V2_DATASETS:
        raise ValueError("cushion v2 inputs do not cover both spent datasets")
    return tuple(rows)


def read_incident_inputs(path):
    with Path(path).open("r", encoding="utf-8", newline="") as source:
        fieldnames = tuple(csv.DictReader(source).fieldnames or ())
    if fieldnames == _V2_INPUT_FIELDS:
        return _read_v2_incident_inputs(path)
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


def _fit_cushion_parameters_v1(points, split, inputs):
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


def _grid(start, stop, step):
    count = int(round((stop - start) / step))
    return tuple(round(start + index * step, 12) for index in range(count + 1))


def _v2_fit(points, intercept, slope, minimum, maximum):
    residuals = []
    for point in points:
        selected_e = restitution(
            point.incident_speed_m_s, intercept, slope, minimum, maximum)
        predicted = selected_e * point.incident_speed_m_s * 100.0
        normalized = (
            predicted - point.expected_rebound_cm_s
        ) / point.standard_uncertainty_cm_s
        residuals.append({
            "dataset_id": point.dataset_id,
            "dataset_version": point.dataset_version,
            "e_max": maximum,
            "e_min": minimum,
            "e_slope": slope,
            "e_intercept": intercept,
            "expected_rebound_cm_s": point.expected_rebound_cm_s,
            "incident_speed_m_s": point.incident_speed_m_s,
            "lifecycle": point.lifecycle,
            "normalized_path": point.normalized_path,
            "normalized_sha256": point.normalized_sha256,
            "normalized_residual": normalized,
            "point_id": point.point_id,
            "predicted_rebound_cm_s": predicted,
            "raw_extracted_path": point.raw_extracted_path,
            "raw_extracted_sha256": point.raw_extracted_sha256,
            "restitution": selected_e,
            "rigid_cushion_domain": point.rigid_cushion_domain,
            "series_id": point.series_id,
            "source_locator": point.source_locator,
            "standard_uncertainty_cm_s": point.standard_uncertainty_cm_s,
        })
    objective = sum(
        row["normalized_residual"] ** 2 for row in residuals
    ) / len(residuals)
    return CushionFit(
        objective, intercept, slope, minimum, maximum, tuple(residuals))


def _fit_cushion_parameters_v2(points):
    points = tuple(sorted(points, key=lambda point: point.point_id))
    if not points or {point.dataset_id for point in points} != _V2_DATASETS:
        raise ValueError("cushion v2 fit requires both spent datasets")
    coarse = []
    for intercept in _grid(0.7, 1.0, 0.05):
        for slope in _grid(0.0, 0.1, 0.01):
            for minimum in _grid(0.0, 1.0, 0.1):
                for maximum in _grid(minimum, 1.0, 0.1):
                    coarse.append(_v2_fit(
                        points, intercept, slope, minimum, maximum))
    coarse_best = min(coarse, key=fit_key)
    fine = []
    for intercept in _grid(
            max(0.0, coarse_best.e_intercept - 0.05),
            min(1.0, coarse_best.e_intercept + 0.05), 0.01):
        for slope in _grid(
                max(0.0, coarse_best.e_slope - 0.01),
                coarse_best.e_slope + 0.01, 0.002):
            for minimum in _grid(
                    0.0,
                    min(1.0, coarse_best.e_min + 0.1), 0.01):
                for maximum in _grid(
                        max(minimum, coarse_best.e_max - 0.1),
                        min(1.0, coarse_best.e_max + 0.1), 0.01):
                    fine.append(_v2_fit(
                        points, intercept, slope, minimum, maximum))
    return min(tuple(coarse) + tuple(fine), key=fit_key)


def fit_cushion_parameters(*arguments):
    if len(arguments) == 1:
        return _fit_cushion_parameters_v2(arguments[0])
    if len(arguments) == 3:
        return _fit_cushion_parameters_v1(*arguments)
    raise TypeError("fit_cushion_parameters expects v2 points or v1 points/split/inputs")


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


def build_v2_fit_report(points):
    fit = fit_cushion_parameters(points)
    return {
        "algorithm": {
            "coarse_grid": {
                "e_intercept": [0.7, 1.0, 0.05],
                "e_max": [0.0, 1.0, 0.1],
                "e_min": [0.0, 1.0, 0.1],
                "e_slope_per_mps": [0.0, 0.1, 0.01],
            },
            "fine_grid_steps": {
                "e_intercept": 0.01,
                "e_max": 0.01,
                "e_min": 0.01,
                "e_slope_per_mps": 0.002,
            },
            "objective": "mean_squared_uncertainty_normalized_rebound_speed_residual",
            "stable_tie_break": [
                "objective", "e_intercept", "e_slope", "e_min", "e_max"],
        },
        "dataset_lifecycle": "spent",
        "fit": {
            "e_intercept": fit.e_intercept,
            "e_max": fit.e_max,
            "e_min": fit.e_min,
            "e_slope_per_mps": fit.e_slope,
            "objective": fit.objective,
            "residual_count": len(fit.residuals),
        },
        "formula": "clamp(e_intercept - e_slope_per_mps * incident_speed_m_s, e_min, e_max)",
        "limitations": [{
            "code": "INSTANTANEOUS_CONTACT_DURATION",
            "description": (
                "The rigid impulse law does not predict finite cushion contact time; "
                "the approximately 8 ms confirmation fact remains unmodeled."
            ),
        }],
        "physical_constants": {
            "friction_coefficient": _SOURCE_REPORTED_FRICTION,
            "maximum_rigid_incident_speed_cm_s": _MAXIMUM_RIGID_INCIDENT_SPEED_CM_S,
            "nose_height_ratio": _NOSE_HEIGHT_RATIO,
            "outside_domain_behavior": "execute_with_rigid_domain_exceeded_warning",
        },
        "schema_version": 2,
    }, fit.residuals


def write_v2_fit_artifacts(points, output_path, residuals_path):
    report, residuals = build_v2_fit_report(points)
    output = Path(output_path)
    residual_output = Path(residuals_path)
    output.parent.mkdir(parents=True, exist_ok=True)
    residual_output.parent.mkdir(parents=True, exist_ok=True)
    output.write_text(_canonical(report), encoding="utf-8")
    fields = tuple(residuals[0])
    with residual_output.open("w", encoding="utf-8", newline="") as target:
        writer = csv.DictWriter(target, fieldnames=fields, lineterminator="\n")
        writer.writeheader()
        writer.writerows(residuals)
    return report


def main(argv=None):
    parser = argparse.ArgumentParser()
    source = parser.add_mutually_exclusive_group(required=True)
    source.add_argument("--package")
    source.add_argument("--inputs")
    parser.add_argument("--output", required=True)
    parser.add_argument("--residuals")
    arguments = parser.parse_args(argv)
    if arguments.inputs:
        if not arguments.residuals:
            parser.error("--residuals is required with --inputs")
        write_v2_fit_artifacts(
            read_incident_inputs(arguments.inputs),
            arguments.output,
            arguments.residuals,
        )
        return 0
    if arguments.residuals:
        parser.error("--residuals is only valid with --inputs")
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
