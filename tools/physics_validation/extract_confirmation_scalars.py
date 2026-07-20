import argparse
import csv
import io
from pathlib import Path


RAW_HEADER = (
    "point_id", "role", "series_id", "group_id", "case_id", "metric",
    "source_value", "source_unit", "normalized_value", "normalized_unit",
    "measurement_uncertainty", "digitization_uncertainty",
    "conversion_uncertainty", "coverage_factor",
    "engineering_absolute_tolerance", "engineering_relative_tolerance",
    "source_locator", "pool_applicability",
)
NORMALIZED_HEADER = (
    "dataset_id", "series_id", "group_id", "case_id", "point_id",
    "partition", "metric", "expected", "unit", "measurement_uncertainty",
    "digitization_uncertainty", "conversion_uncertainty", "coverage_factor",
    "engineering_absolute_tolerance", "engineering_relative_tolerance",
    "source_locator", "pool_applicability",
)


def normalized_bytes(raw_path, dataset_id):
    output = io.StringIO(newline="")
    writer = csv.DictWriter(output, fieldnames=NORMALIZED_HEADER, lineterminator="\n")
    writer.writeheader()
    with Path(raw_path).open(encoding="utf-8", newline="") as stream:
        reader = csv.DictReader(stream)
        if tuple(reader.fieldnames or ()) != RAW_HEADER:
            raise ValueError("confirmation scalar CSV header is invalid")
        for row in reader:
            if row["role"] != "confirmation_target":
                continue
            writer.writerow({
                "dataset_id": dataset_id,
                "series_id": row["series_id"],
                "group_id": row["group_id"],
                "case_id": row["case_id"],
                "point_id": row["point_id"],
                "partition": "HOLDOUT",
                "metric": row["metric"],
                "expected": row["normalized_value"],
                "unit": row["normalized_unit"],
                "measurement_uncertainty": row["measurement_uncertainty"],
                "digitization_uncertainty": row["digitization_uncertainty"],
                "conversion_uncertainty": row["conversion_uncertainty"],
                "coverage_factor": row["coverage_factor"],
                "engineering_absolute_tolerance": row["engineering_absolute_tolerance"],
                "engineering_relative_tolerance": row["engineering_relative_tolerance"],
                "source_locator": row["source_locator"],
                "pool_applicability": row["pool_applicability"],
            })
    return output.getvalue().encode("utf-8")


def main_for_dataset(dataset_id, argv=None):
    parser = argparse.ArgumentParser(description=f"Normalize {dataset_id} scalars")
    parser.add_argument("raw", type=Path)
    parser.add_argument("output", type=Path)
    arguments = parser.parse_args(argv)
    arguments.output.write_bytes(normalized_bytes(arguments.raw, dataset_id))

