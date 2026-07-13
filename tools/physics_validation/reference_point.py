import csv
import math
import re
from dataclasses import dataclass
from pathlib import Path


_HEADER = (
    "dataset_id",
    "series_id",
    "group_id",
    "case_id",
    "point_id",
    "partition",
    "metric",
    "expected",
    "unit",
    "measurement_uncertainty",
    "digitization_uncertainty",
    "conversion_uncertainty",
    "coverage_factor",
    "engineering_absolute_tolerance",
    "engineering_relative_tolerance",
    "source_locator",
    "pool_applicability",
)
_SAFE_ID = re.compile(r"[A-Za-z0-9][A-Za-z0-9_.-]*")
_UNITS = {"cm", "cm/s", "cm/s^2", "s", "degree", "rad/s", "dimensionless"}
_PARTITIONS = {"CALIBRATION", "HOLDOUT"}
_POOL_APPLICABILITY = {"DIRECT", "CONVERTED", "TREND_ONLY", "NOT_APPLICABLE"}


@dataclass(frozen=True)
class ReferencePoint:
    dataset_id: str
    series_id: str
    group_id: str
    case_id: str
    point_id: str
    partition: str
    metric: str
    expected: float
    unit: str
    measurement_uncertainty: float
    digitization_uncertainty: float
    conversion_uncertainty: float
    coverage_factor: float
    engineering_absolute_tolerance: float
    engineering_relative_tolerance: float
    source_locator: str
    pool_applicability: str

    @property
    def combined_standard_uncertainty(self):
        return math.sqrt(
            self.measurement_uncertainty ** 2
            + self.digitization_uncertainty ** 2
            + self.conversion_uncertainty ** 2
        )

    @property
    def acceptance_half_width(self):
        return max(
            self.engineering_absolute_tolerance,
            self.engineering_relative_tolerance * abs(self.expected),
            self.coverage_factor * self.combined_standard_uncertainty,
        )

    @property
    def acceptance_interval(self):
        width = self.acceptance_half_width
        return self.expected - width, self.expected + width


def _safe_id(value, field, line_number):
    if not isinstance(value, str) or _SAFE_ID.fullmatch(value) is None:
        raise ValueError(f"line {line_number}: {field} is not a safe stable ID")
    return value


def _number(value, field, line_number, nonnegative=False, default=None):
    if value == "" and default is not None:
        return default
    try:
        parsed = float(value)
    except (TypeError, ValueError) as error:
        raise ValueError(f"line {line_number}: {field} must be numeric") from error
    if not math.isfinite(parsed):
        raise ValueError(f"line {line_number}: {field} must be finite")
    if nonnegative and parsed < 0.0:
        raise ValueError(f"line {line_number}: {field} must be nonnegative")
    return parsed


def read_reference_points(path, dataset_id):
    _safe_id(dataset_id, "dataset_id", 0)
    points = []
    seen_point_ids = set()
    path = Path(path)
    try:
        source = path.open("r", encoding="utf-8", newline="")
    except (OSError, UnicodeError) as error:
        raise ValueError(f"cannot read normalized CSV {path}: {error}") from error
    with source:
        reader = csv.DictReader(source)
        if tuple(reader.fieldnames or ()) != _HEADER:
            raise ValueError("normalized CSV header does not match schema version 1")
        for line_number, row in enumerate(reader, start=2):
            row_dataset_id = _safe_id(row["dataset_id"], "dataset_id", line_number)
            if row_dataset_id != dataset_id:
                raise ValueError(
                    f"line {line_number}: dataset_id {row_dataset_id} does not match {dataset_id}")
            identifiers = {
                field: _safe_id(row[field], field, line_number)
                for field in ("series_id", "group_id", "case_id", "point_id", "metric")
            }
            if identifiers["point_id"] in seen_point_ids:
                raise ValueError(
                    f"line {line_number}: duplicate point_id {identifiers['point_id']}")
            partition = row["partition"]
            if partition not in _PARTITIONS:
                raise ValueError(f"line {line_number}: invalid partition {partition}")
            unit = row["unit"]
            if unit not in _UNITS:
                raise ValueError(f"line {line_number}: unsupported unit {unit}")
            pool_applicability = row["pool_applicability"]
            if pool_applicability not in _POOL_APPLICABILITY:
                raise ValueError(
                    f"line {line_number}: invalid pool_applicability {pool_applicability}")
            source_locator = row["source_locator"]
            if not isinstance(source_locator, str) or not source_locator.strip():
                raise ValueError(f"line {line_number}: source_locator must be nonempty")
            reference = ReferencePoint(
                dataset_id=row_dataset_id,
                series_id=identifiers["series_id"],
                group_id=identifiers["group_id"],
                case_id=identifiers["case_id"],
                point_id=identifiers["point_id"],
                partition=partition,
                metric=identifiers["metric"],
                expected=_number(row["expected"], "expected", line_number),
                unit=unit,
                measurement_uncertainty=_number(
                    row["measurement_uncertainty"], "measurement_uncertainty",
                    line_number, nonnegative=True),
                digitization_uncertainty=_number(
                    row["digitization_uncertainty"], "digitization_uncertainty",
                    line_number, nonnegative=True),
                conversion_uncertainty=_number(
                    row["conversion_uncertainty"], "conversion_uncertainty",
                    line_number, nonnegative=True),
                coverage_factor=_number(
                    row["coverage_factor"], "coverage_factor", line_number,
                    nonnegative=True, default=2.0),
                engineering_absolute_tolerance=_number(
                    row["engineering_absolute_tolerance"],
                    "engineering_absolute_tolerance", line_number, nonnegative=True),
                engineering_relative_tolerance=_number(
                    row["engineering_relative_tolerance"],
                    "engineering_relative_tolerance", line_number, nonnegative=True),
                source_locator=source_locator,
                pool_applicability=pool_applicability,
            )
            points.append(reference)
            seen_point_ids.add(reference.point_id)
    return tuple(points)
