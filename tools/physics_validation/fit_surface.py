import argparse
import csv
import json
import math
from dataclasses import asdict, dataclass
from pathlib import Path


_GRAVITY_CM_S2 = 980.665
_PHASES = {"rolling", "sliding"}
_INPUT_FIELDS = (
    "point_id",
    "dataset_id",
    "dataset_version",
    "lifecycle",
    "phase",
    "selection",
    "contiguous_sample_count",
    "observed_acceleration_cm_s2",
    "standard_uncertainty_cm_s2",
    "source_expected_cm_s2",
    "source_lower_cm_s2",
    "source_upper_cm_s2",
    "source_locator",
    "evidence_path",
    "evidence_sha256",
)
_RESIDUAL_FIELDS = (
    "point_id",
    "dataset_id",
    "dataset_version",
    "lifecycle",
    "phase",
    "contiguous_sample_count",
    "observed_acceleration_cm_s2",
    "fitted_acceleration_cm_s2",
    "standard_uncertainty_cm_s2",
    "normalized_residual",
    "source_expected_cm_s2",
    "source_lower_cm_s2",
    "source_upper_cm_s2",
    "source_locator",
    "evidence_path",
    "evidence_sha256",
)


@dataclass(frozen=True)
class SurfacePoint:
    point_id: str
    dataset_id: str
    dataset_version: str
    lifecycle: str
    phase: str
    selection: str
    contiguous_sample_count: int
    observed_acceleration_cm_s2: float
    standard_uncertainty_cm_s2: float
    source_expected_cm_s2: float
    source_lower_cm_s2: float
    source_upper_cm_s2: float
    source_locator: str
    evidence_path: str
    evidence_sha256: str


@dataclass(frozen=True)
class SurfaceResidual:
    point_id: str
    dataset_id: str
    dataset_version: str
    lifecycle: str
    phase: str
    contiguous_sample_count: int
    observed_acceleration_cm_s2: float
    fitted_acceleration_cm_s2: float
    standard_uncertainty_cm_s2: float
    normalized_residual: float
    source_expected_cm_s2: float
    source_lower_cm_s2: float
    source_upper_cm_s2: float
    source_locator: str
    evidence_path: str
    evidence_sha256: str


@dataclass(frozen=True)
class SurfaceFit:
    sliding_friction_coefficient: float
    sliding_acceleration_cm_s2: float
    rolling_resistance_acceleration_cm_s2: float
    covariance: tuple
    objective: float
    residuals: tuple
    parameter_bounds: tuple


def _canonical(document):
    return json.dumps(
        document,
        ensure_ascii=False,
        indent=2,
        sort_keys=True,
        allow_nan=False,
    ) + "\n"


def _finite_number(row, field, line_number):
    try:
        value = float(row[field])
    except ValueError as error:
        raise ValueError(f"line {line_number}: {field} must be numeric") from error
    if not math.isfinite(value):
        raise ValueError(f"line {line_number}: {field} must be finite")
    return value


def read_surface_inputs(path):
    points = []
    seen = set()
    with Path(path).open("r", encoding="utf-8", newline="") as source:
        reader = csv.DictReader(source)
        if tuple(reader.fieldnames or ()) != _INPUT_FIELDS:
            raise ValueError("surface fit input header does not match schema version 1")
        for line_number, row in enumerate(reader, start=2):
            point_id = row["point_id"]
            if not point_id or point_id in seen:
                raise ValueError(f"line {line_number}: point_id must be nonempty and unique")
            seen.add(point_id)
            if row["lifecycle"] != "spent":
                raise ValueError(f"line {line_number}: surface fit input must be spent")
            if row["phase"] not in _PHASES:
                raise ValueError(f"line {line_number}: unknown surface phase")
            if row["selection"] != "maximal_contiguous_phase":
                raise ValueError(f"line {line_number}: surface input is not phase-correct")
            try:
                sample_count = int(row["contiguous_sample_count"])
            except ValueError as error:
                raise ValueError(
                    f"line {line_number}: contiguous sample count must be an integer"
                ) from error
            if sample_count < 1:
                raise ValueError(
                    f"line {line_number}: contiguous sample count must be positive"
                )
            observed = _finite_number(
                row, "observed_acceleration_cm_s2", line_number)
            uncertainty = _finite_number(
                row, "standard_uncertainty_cm_s2", line_number)
            expected = _finite_number(row, "source_expected_cm_s2", line_number)
            lower = _finite_number(row, "source_lower_cm_s2", line_number)
            upper = _finite_number(row, "source_upper_cm_s2", line_number)
            if observed < 0.0 or uncertainty <= 0.0:
                raise ValueError(
                    f"line {line_number}: acceleration and uncertainty are invalid"
                )
            if lower < 0.0 or not lower <= expected <= upper:
                raise ValueError(f"line {line_number}: source interval is invalid")
            if (not row["dataset_version"] or not row["source_locator"]
                    or not row["evidence_path"]
                    or not row["evidence_sha256"].startswith("sha256:")
                    or len(row["evidence_sha256"]) != 71):
                raise ValueError(f"line {line_number}: provenance fields must be nonempty")
            points.append(SurfacePoint(
                point_id=point_id,
                dataset_id=row["dataset_id"],
                dataset_version=row["dataset_version"],
                lifecycle=row["lifecycle"],
                phase=row["phase"],
                selection=row["selection"],
                contiguous_sample_count=sample_count,
                observed_acceleration_cm_s2=observed,
                standard_uncertainty_cm_s2=uncertainty,
                source_expected_cm_s2=expected,
                source_lower_cm_s2=lower,
                source_upper_cm_s2=upper,
                source_locator=row["source_locator"],
                evidence_path=row["evidence_path"],
                evidence_sha256=row["evidence_sha256"],
            ))
    if not points:
        raise ValueError("surface fit input is empty")
    if [point.point_id for point in points] != sorted(seen):
        raise ValueError("surface fit inputs must be sorted by point_id")
    if {point.phase for point in points} != _PHASES:
        raise ValueError("surface fit requires both sliding and rolling rows")
    return tuple(points)


def _weighted_mean(points):
    weights = tuple(1.0 / point.standard_uncertainty_cm_s2 ** 2 for point in points)
    return (
        sum(weight * point.observed_acceleration_cm_s2
            for weight, point in zip(weights, points)) / sum(weights),
        1.0 / sum(weights),
    )


def _phase_interval(points):
    return (
        max(0.0, min(point.source_lower_cm_s2 for point in points)),
        max(point.source_upper_cm_s2 for point in points),
    )


def fit_surface_parameters(points):
    points = tuple(points)
    sliding = tuple(point for point in points if point.phase == "sliding")
    rolling = tuple(point for point in points if point.phase == "rolling")
    if not sliding or not rolling:
        raise ValueError("surface fit requires both sliding and rolling rows")
    sliding_acceleration, sliding_variance = _weighted_mean(sliding)
    rolling_acceleration, rolling_variance = _weighted_mean(rolling)
    residuals = tuple(
        SurfaceResidual(
            point_id=point.point_id,
            dataset_id=point.dataset_id,
            dataset_version=point.dataset_version,
            lifecycle=point.lifecycle,
            phase=point.phase,
            contiguous_sample_count=point.contiguous_sample_count,
            observed_acceleration_cm_s2=point.observed_acceleration_cm_s2,
            fitted_acceleration_cm_s2=(
                sliding_acceleration if point.phase == "sliding"
                else rolling_acceleration
            ),
            standard_uncertainty_cm_s2=point.standard_uncertainty_cm_s2,
            normalized_residual=(
                point.observed_acceleration_cm_s2 - (
                    sliding_acceleration if point.phase == "sliding"
                    else rolling_acceleration
                )
            ) / point.standard_uncertainty_cm_s2,
            source_expected_cm_s2=point.source_expected_cm_s2,
            source_lower_cm_s2=point.source_lower_cm_s2,
            source_upper_cm_s2=point.source_upper_cm_s2,
            source_locator=point.source_locator,
            evidence_path=point.evidence_path,
            evidence_sha256=point.evidence_sha256,
        )
        for point in points
    )
    objective = sum(
        residual.normalized_residual ** 2 for residual in residuals
    ) / len(residuals)
    sliding_interval = _phase_interval(sliding)
    rolling_interval = _phase_interval(rolling)
    return SurfaceFit(
        sliding_friction_coefficient=sliding_acceleration / _GRAVITY_CM_S2,
        sliding_acceleration_cm_s2=sliding_acceleration,
        rolling_resistance_acceleration_cm_s2=rolling_acceleration,
        covariance=(
            ("rolling_resistance_acceleration_cm_s2", rolling_variance),
            ("sliding_acceleration_cm_s2", sliding_variance),
            ("sliding_friction_coefficient", sliding_variance / _GRAVITY_CM_S2 ** 2),
        ),
        objective=objective,
        residuals=residuals,
        parameter_bounds=(
            ("rolling_resistance_acceleration_cm_s2", rolling_interval),
            ("sliding_acceleration_cm_s2", sliding_interval),
            ("sliding_friction_coefficient", (
                sliding_interval[0] / _GRAVITY_CM_S2,
                sliding_interval[1] / _GRAVITY_CM_S2,
            )),
        ),
    )


def _fit_document(points, fit):
    return {
        "algorithm": {
            "equations": (
                "rigid-sphere Coulomb sliding plus constant rolling resistance"
            ),
            "objective": "mean_squared_uncertainty_normalized_residual",
            "phase_estimator": "inverse_variance_weighted_mean",
            "selection": "maximal_contiguous_telemetry_phase",
        },
        "dataset_lifecycle": "spent",
        "fit": {
            "covariance": dict(fit.covariance),
            "objective": fit.objective,
            "parameter_bounds": dict(fit.parameter_bounds),
            "rolling_resistance_acceleration_cm_s2": (
                fit.rolling_resistance_acceleration_cm_s2
            ),
            "sliding_acceleration_cm_s2": fit.sliding_acceleration_cm_s2,
            "sliding_friction_coefficient": fit.sliding_friction_coefficient,
        },
        "gravity_cm_s2": _GRAVITY_CM_S2,
        "input_rows": [asdict(point) for point in points],
        "residual_count": len(fit.residuals),
        "schema_version": 1,
    }


def write_fit_artifacts(points, json_path, residuals_path):
    points = tuple(points)
    fit = fit_surface_parameters(points)
    json_output = Path(json_path)
    residual_output = Path(residuals_path)
    json_output.parent.mkdir(parents=True, exist_ok=True)
    residual_output.parent.mkdir(parents=True, exist_ok=True)
    json_output.write_text(
        _canonical(_fit_document(points, fit)), encoding="utf-8")
    with residual_output.open("w", encoding="utf-8", newline="") as output:
        writer = csv.DictWriter(output, fieldnames=_RESIDUAL_FIELDS, lineterminator="\n")
        writer.writeheader()
        for residual in fit.residuals:
            writer.writerow(asdict(residual))
    return fit


def main(argv=None):
    parser = argparse.ArgumentParser()
    parser.add_argument("--inputs", required=True)
    parser.add_argument("--json", required=True)
    parser.add_argument("--residuals", required=True)
    arguments = parser.parse_args(argv)
    write_fit_artifacts(
        read_surface_inputs(arguments.inputs), arguments.json, arguments.residuals)
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
