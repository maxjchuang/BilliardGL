import argparse
import csv
import io
import json
from decimal import Decimal
from pathlib import Path


_DATASET_ID = "domenech_2023_ball_collision"
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


def _read_csv(path):
    with Path(path).open("r", encoding="utf-8", newline="") as source:
        return tuple(csv.DictReader(source))


def _format(value):
    text = format(Decimal(str(value)), "f")
    if "." in text:
        text = text.rstrip("0").rstrip(".")
    return text or "0"


def _digitization_by_point(rows):
    grouped = {}
    for row in rows:
        grouped.setdefault(row["point_id"], []).append(row)
    result = {}
    for point_id, observations in grouped.items():
        if len(observations) != 2 or {item["pass_id"] for item in observations} != {"1", "2"}:
            raise ValueError(f"digitized point {point_id} requires two independent passes")
        values = [Decimal(item["converted_y_degrees"]) for item in observations]
        residuals = [Decimal(item["axis_residual_degrees"]) for item in observations]
        result[point_id] = (abs(values[0] - values[1]) / Decimal("2"), max(residuals))
    return result


def _partition(group_id):
    return "CALIBRATION" if group_id.endswith("_low") else "HOLDOUT"


def normalize_rows(raw_path, digitization_path, extraction_path):
    raw_rows = _read_csv(raw_path)
    digitization = _digitization_by_point(_read_csv(digitization_path))
    extraction = json.loads(Path(extraction_path).read_text(encoding="utf-8"))
    if extraction.get("uncertainty_interpretation") != "independent_bounded_components":
        raise ValueError("extraction must preserve independent bounded uncertainty components")
    normalized = []
    for row in raw_rows:
        if row["status"] != "ADMITTED":
            continue
        point_id = row["point_id"]
        digitization_uncertainty, calibration_residual = digitization[point_id]
        normalized.append({
            "dataset_id": _DATASET_ID,
            "series_id": row["series_id"],
            "group_id": row["group_id"],
            "case_id": row["case_id"],
            "point_id": point_id,
            "partition": _partition(row["group_id"]),
            "metric": row["metric"],
            "expected": _format(row["expected_degrees"]),
            "unit": "degree",
            "measurement_uncertainty": "0",
            "digitization_uncertainty": _format(digitization_uncertainty),
            "conversion_uncertainty": _format(calibration_residual),
            "coverage_factor": "1",
            "engineering_absolute_tolerance": "0",
            "engineering_relative_tolerance": "0",
            "source_locator": row["source_locator"],
            "pool_applicability": (
                "CONVERTED" if row["material"] == "billiard" else "TREND_ONLY"
            ),
        })
    if set(digitization) != {row["point_id"] for row in raw_rows if row["status"] == "ADMITTED"}:
        raise ValueError("digitization and admitted raw point inventories differ")
    normalized.sort(key=lambda row: (
        row["series_id"], row["group_id"], row["case_id"], row["point_id"]))
    return tuple(normalized)


def write_normalized(package_path):
    package = Path(package_path)
    rows = normalize_rows(
        package / "raw_extracted.csv",
        package / "digitization.csv",
        package / "extraction.json",
    )
    output = io.StringIO(newline="")
    writer = csv.DictWriter(output, fieldnames=_HEADER, lineterminator="\n")
    writer.writeheader()
    writer.writerows(rows)
    return output.getvalue().encode("utf-8")


def main(argv=None):
    parser = argparse.ArgumentParser(description="Rebuild Doménech 2023 normalized data")
    parser.add_argument("--package", required=True, type=Path)
    parser.add_argument("--check", action="store_true")
    arguments = parser.parse_args(argv)
    generated = write_normalized(arguments.package)
    normalized_path = arguments.package / "normalized.csv"
    if arguments.check:
        if not normalized_path.is_file() or normalized_path.read_bytes() != generated:
            raise SystemExit("normalized.csv is not byte-reproducible")
    else:
        normalized_path.write_bytes(generated)
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
