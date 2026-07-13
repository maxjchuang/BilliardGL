import argparse
import csv
import io
import json
from decimal import Decimal
from pathlib import Path


_DATASET_ID = "mathavan_2010_cushion"
_HEADER = (
    "dataset_id", "series_id", "group_id", "case_id", "point_id", "partition",
    "metric", "expected", "unit", "measurement_uncertainty", "digitization_uncertainty",
    "conversion_uncertainty", "coverage_factor", "engineering_absolute_tolerance",
    "engineering_relative_tolerance", "source_locator", "pool_applicability",
)


def _read_csv(path):
    with Path(path).open("r", encoding="utf-8", newline="") as source:
        return tuple(csv.DictReader(source))


def _format(value):
    text = format(Decimal(str(value)), "f")
    if "." in text:
        text = text.rstrip("0").rstrip(".")
    return text or "0"


def normalize_rows(raw_path, digitization_path, extraction_path):
    raw = _read_csv(raw_path)
    digitization = _read_csv(digitization_path)
    extraction = json.loads(Path(extraction_path).read_text(encoding="utf-8"))
    if extraction.get("uncertainty_interpretation") != "independent_bounded_components":
        raise ValueError("Mathavan 2010 uncertainty components must remain independent")
    by_point = {}
    for row in digitization:
        by_point.setdefault(row["point_id"], []).append(row)
    rows = []
    for source in raw:
        if source["status"] != "ADMITTED":
            continue
        passes = by_point.get(source["point_id"], [])
        if len(passes) != 2 or {item["pass_id"] for item in passes} != {"1", "2"}:
            raise ValueError(f"point {source['point_id']} requires two digitization passes")
        rebound = [Decimal(item["rebound_speed_m_s"]) for item in passes]
        residual = max(Decimal(item["y_axis_residual_m_s"]) for item in passes)
        rows.append({
            "dataset_id": _DATASET_ID,
            "series_id": source["series_id"],
            "group_id": source["group_id"],
            "case_id": source["case_id"],
            "point_id": source["point_id"],
            "partition": "CALIBRATION" if source["group_id"] == "incident_low" else "HOLDOUT",
            "metric": "cushion_rebound_speed_cm_s",
            "expected": _format(Decimal(source["rebound_speed_m_s"]) * 100),
            "unit": "cm/s",
            "measurement_uncertainty": "0",
            "digitization_uncertainty": _format(abs(rebound[0] - rebound[1]) * 50),
            "conversion_uncertainty": _format(residual * 100),
            "coverage_factor": "1",
            "engineering_absolute_tolerance": "0",
            "engineering_relative_tolerance": "0",
            "source_locator": source["source_locator"],
            "pool_applicability": "TREND_ONLY",
        })
    if len(rows) != 19 or set(by_point) != {row["point_id"] for row in raw}:
        raise ValueError("Fig. 7 experimental inventory must contain exactly 19 points")
    rows.sort(key=lambda row: row["point_id"])
    return tuple(rows)


def write_normalized(package_path):
    package = Path(package_path)
    rows = normalize_rows(package / "raw_extracted.csv", package / "digitization.csv", package / "extraction.json")
    output = io.StringIO(newline="")
    writer = csv.DictWriter(output, fieldnames=_HEADER, lineterminator="\n")
    writer.writeheader()
    writer.writerows(rows)
    return output.getvalue().encode("utf-8")


def main(argv=None):
    parser = argparse.ArgumentParser(description="Rebuild Mathavan 2010 Fig. 7 data")
    parser.add_argument("--package", required=True, type=Path)
    parser.add_argument("--check", action="store_true")
    arguments = parser.parse_args(argv)
    generated = write_normalized(arguments.package)
    path = arguments.package / "normalized.csv"
    if arguments.check:
        if not path.is_file() or path.read_bytes() != generated:
            raise SystemExit("normalized.csv is not byte-reproducible")
    else:
        path.write_bytes(generated)
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
