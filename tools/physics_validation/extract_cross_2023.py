import argparse
import csv
from pathlib import Path


_HEADER = "dataset_id,series_id,group_id,case_id,point_id,partition,metric,expected,unit,measurement_uncertainty,digitization_uncertainty,conversion_uncertainty,coverage_factor,engineering_absolute_tolerance,engineering_relative_tolerance,source_locator,pool_applicability\n"


def normalize_rows(raw_path: Path, digitization_path: Path, extraction_path: Path):
    del extraction_path
    with raw_path.open(encoding="utf-8", newline="") as source:
        rows = tuple(csv.DictReader(source))
    with digitization_path.open(encoding="utf-8", newline="") as source:
        digitized = tuple(csv.DictReader(source))
    if len(rows) != 1 or rows[0].get("status") != "ADMISSION_BLOCKED":
        raise ValueError("Cross raw inventory must remain admission-blocked until full text is audited")
    if digitized:
        raise ValueError("numeric digitization cannot precede lawful full-text admission")
    return ()


def write_normalized(package_path: Path):
    normalize_rows(
        package_path / "raw_extracted.csv",
        package_path / "digitization.csv",
        package_path / "extraction.json",
    )
    return _HEADER.encode("utf-8")


def main(argv=None):
    parser = argparse.ArgumentParser()
    parser.add_argument("--package", type=Path, required=True)
    parser.add_argument("--check", action="store_true")
    arguments = parser.parse_args(argv)
    generated = write_normalized(arguments.package)
    target = arguments.package / "normalized.csv"
    if arguments.check:
        if target.read_bytes() != generated:
            raise SystemExit("normalized.csv is not byte-identical to the admission-gated extraction")
    else:
        target.write_bytes(generated)
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
