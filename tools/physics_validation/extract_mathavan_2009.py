import argparse
import csv
import io
import json
from decimal import Decimal
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
_DATASET_ID = "mathavan_2009_high_speed"
_PARTITIONS = {
    "rolling_summary": "CALIBRATION",
    "table1_mid": "CALIBRATION",
    "sliding_summary": "HOLDOUT",
    "fig9_visible": "HOLDOUT",
    "table1_extreme": "HOLDOUT",
}


def _read_csv(path):
    with Path(path).open("r", encoding="utf-8", newline="") as source:
        return tuple(csv.DictReader(source))


def _decimal(value):
    return Decimal(str(value))


def _format(value):
    value = _decimal(value)
    text = format(value, "f")
    if "." in text:
        text = text.rstrip("0").rstrip(".")
    return text or "0"


_SIX_PLACES = Decimal("0.000001")


def _reconstruct_figure_observation(row, axis):
    pixel = _decimal(row[f"pixel_{axis}"])
    if axis == "x":
        low_pixel = _decimal(row["axis_x0_pixel"])
        high_pixel = _decimal(row["axis_x4_pixel"])
        value = (pixel - low_pixel) * Decimal("4") / (high_pixel - low_pixel)
    else:
        low_pixel = _decimal(row["axis_y0_pixel"])
        high_pixel = _decimal(row["axis_y3_5_pixel"])
        value = (low_pixel - pixel) * Decimal("3.5") / (low_pixel - high_pixel)
    reconstructed = value.quantize(_SIX_PLACES)
    field = f"converted_{axis}_m_s"
    if _decimal(row[field]) != reconstructed:
        raise ValueError(
            f"digitized point {row['point_id']} {field} does not match pixel reconstruction")
    return value, reconstructed


def _base(row, *, point_id, metric, expected, unit, measurement, digitization="0"):
    return {
        "dataset_id": _DATASET_ID,
        "series_id": row["series_id"],
        "group_id": row["group_id"],
        "case_id": row["case_id"],
        "point_id": point_id,
        "partition": _PARTITIONS[row["group_id"]],
        "metric": metric,
        "expected": _format(expected),
        "unit": unit,
        "measurement_uncertainty": _format(measurement),
        "digitization_uncertainty": _format(digitization),
        "conversion_uncertainty": "0",
        "coverage_factor": "1",
        "engineering_absolute_tolerance": "0",
        "engineering_relative_tolerance": "0",
        "source_locator": row["source_locator"],
        "pool_applicability": "TREND_ONLY",
    }


def _digitization_values(rows):
    values = {}
    for row in rows:
        _reconstruct_figure_observation(row, "x")
        value, rounded = _reconstruct_figure_observation(row, "y")
        values.setdefault(row["point_id"], []).append((value, rounded))
    result = {}
    for point_id, observations in values.items():
        if len(observations) != 2:
            raise ValueError(f"digitized point {point_id} requires exactly two extraction passes")
        result[point_id] = (
            ((observations[0][0] + observations[1][0]) / 2).quantize(_SIX_PLACES),
            abs(observations[0][1] - observations[1][1]) * Decimal("50"),
        )
    return result


def normalize_rows(raw_path, digitization_path, extraction_path):
    raw_rows = _read_csv(raw_path)
    digitization_rows = _read_csv(digitization_path)
    extraction = json.loads(Path(extraction_path).read_text(encoding="utf-8"))
    if extraction.get("uncertainty_interpretation") != "reported_bounded_range":
        raise ValueError("extraction metadata must declare reported bounded range semantics")

    figure_rows = [row for row in raw_rows if row["record_type"] == "figure_marker"]
    if len(figure_rows) != 31:
        raise ValueError("Fig. 9 inventory must preserve all 31 reported shots")
    digitization_values = _digitization_values(digitization_rows)

    normalized = []
    for row in raw_rows:
        if row["status"] != "ADMITTED":
            continue
        if row["record_type"] == "reported_range":
            lower = _decimal(row["y_lower"]) * Decimal("100")
            upper = _decimal(row["y_upper"]) * Decimal("100")
            normalized.append(_base(
                row,
                point_id=row["point_id"],
                metric=row["y_name"] + "_cm_s2",
                expected=(lower + upper) / 2,
                unit="cm/s^2",
                measurement=(upper - lower) / 2,
            ))
        elif row["record_type"] == "figure_marker":
            expected, digitization_uncertainty = digitization_values[row["point_id"]]
            if abs(_decimal(row["y_value"]) - expected) > _SIX_PLACES:
                raise ValueError(
                    f"raw point {row['point_id']} y_value disagrees with pixel reconstruction")
            normalized.append(_base(
                row,
                point_id=row["point_id"],
                metric="cushion_rebound_speed_cm_s",
                expected=expected * Decimal("100"),
                unit="cm/s",
                measurement=Decimal("21.213203"),
                digitization=digitization_uncertainty,
            ))
        elif row["record_type"] == "table_shot":
            normalized.append(_base(
                row,
                point_id=row["point_id"] + "_cue_speed",
                metric="post_collision_linear_velocity_cm_s",
                expected=_decimal(row["y_value"]) * Decimal("100"),
                unit="cm/s",
                measurement=Decimal("6.363961"),
            ))
            normalized.append(_base(
                row,
                point_id=row["point_id"] + "_object_speed",
                metric="post_collision_linear_velocity_cm_s",
                expected=_decimal(row["secondary_value"]) * Decimal("100"),
                unit="cm/s",
                measurement=Decimal("6.363961"),
            ))
        else:
            raise ValueError(f"unsupported raw record type: {row['record_type']}")

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
    parser = argparse.ArgumentParser(description="Rebuild Mathavan 2009 normalized data")
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
