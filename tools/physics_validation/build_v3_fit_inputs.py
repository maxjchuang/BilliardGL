import argparse
import csv
import hashlib
import io
import math
from pathlib import Path

from .data_lifecycle import load_data_lifecycle


BALL_FIELDS = (
    "point_id", "dataset_id", "dataset_version", "lifecycle", "series_id",
    "case_id", "metric", "sample_phase", "impact_angle_degrees",
    "incident_speed_m_s", "expected", "unit", "standard_uncertainty",
    "source_locator", "normalized_path", "normalized_sha256",
    "raw_extracted_path", "raw_extracted_sha256",
)
CUSHION_FIELDS = (
    "point_id", "dataset_id", "dataset_version", "lifecycle", "series_id",
    "metric", "incident_speed_m_s", "expected", "unit",
    "standard_uncertainty", "rigid_cushion_domain", "source_locator",
    "normalized_path", "normalized_sha256", "raw_extracted_path",
    "raw_extracted_sha256",
)
STRUCTURAL_FIELDS = (
    "point_id", "dataset_id", "dataset_version", "lifecycle", "metric",
    "expected", "unit", "observed", "status", "source_locator", "rationale",
)


def _read_csv(path):
    with Path(path).open(encoding="utf-8", newline="") as stream:
        return list(csv.DictReader(stream))


def _sha256(path):
    return "sha256:" + hashlib.sha256(Path(path).read_bytes()).hexdigest()


def _relative(root, path):
    return Path(path).resolve().relative_to(Path(root).resolve()).as_posix()


def _effective_uncertainty(row):
    combined = math.sqrt(sum(float(row[field]) ** 2 for field in (
        "measurement_uncertainty",
        "digitization_uncertainty",
        "conversion_uncertainty",
    )))
    expected = abs(float(row["expected"]))
    # Engineering limits are half-width contracts. Converting them to a k=2
    # standard uncertainty prevents printed rounding from pretending to be
    # experimental precision while preserving the committed numeric contract.
    return max(
        combined,
        float(row["engineering_absolute_tolerance"]) / 2.0,
        expected * float(row["engineering_relative_tolerance"]) / 2.0,
    )


def _sudo_context(root):
    package = Path(root) / "tests/physics_validation/reference_data/sudo_2002"
    normalized = package / "normalized.csv"
    raw = package / "raw_extracted.csv"
    rows = {row["point_id"]: row for row in _read_csv(normalized)}
    provenance = {
        "dataset_id": "sudo_2002",
        "dataset_version": "1.0.0",
        "lifecycle": "spent",
        "normalized_path": _relative(root, normalized),
        "normalized_sha256": _sha256(normalized),
        "raw_extracted_path": _relative(root, raw),
        "raw_extracted_sha256": _sha256(raw),
    }
    return rows, provenance


def _require_spent_rows(root, rows):
    registry = load_data_lifecycle(
        Path(root) / "tests/physics_validation/validation_data_status.json")
    for row in rows:
        if row["lifecycle"] != "spent":
            raise ValueError("v3 fitting accepts only spent input rows")
        entry = registry.entry(row["dataset_id"], row["dataset_version"])
        if entry.holdout_status != "spent":
            raise ValueError(
                f"{row['dataset_id']} is not spent in the lifecycle registry")
        if row["dataset_id"] in {"derby_fuller_1999", "han_2005"}:
            raise ValueError("confirmation evidence cannot enter v3 fitting")
    return rows


def build_ball_inputs(root):
    root = Path(root)
    base = _read_csv(
        root / "physics_models/calibration/ball_collision_fit_v2_inputs.csv")
    rows, provenance = _sudo_context(root)
    adapters = {
        "ball_ball_e_head_on": {
            "impact_angle_degrees": "30",
            "sample_phase": "immediate_post_impact",
        },
        "separation_angle_mean": {
            "impact_angle_degrees": "30",
            "sample_phase": "immediate_post_impact",
        },
        "transverse_momentum_deficit": {
            "impact_angle_degrees": "30",
            "sample_phase": "immediate_post_impact",
        },
    }
    for point_id, adapter in adapters.items():
        source = rows[point_id]
        base.append({
            **provenance,
            "point_id": point_id,
            "series_id": "sudo_ball_collision",
            "case_id": source["case_id"],
            "metric": source["metric"],
            "sample_phase": adapter["sample_phase"],
            "impact_angle_degrees": adapter["impact_angle_degrees"],
            "incident_speed_m_s": "0.98",
            "expected": source["expected"],
            "unit": source["unit"],
            "standard_uncertainty": format(
                _effective_uncertainty(source), ".17g"),
            "source_locator": source["source_locator"],
        })
    return _require_spent_rows(
        root, sorted(base, key=lambda row: row["point_id"]))


def build_cushion_inputs(root):
    root = Path(root)
    base = []
    for source in _read_csv(
            root / "physics_models/calibration/cushion_fit_v2_inputs.csv"):
        base.append({
            "point_id": source["point_id"],
            "dataset_id": source["dataset_id"],
            "dataset_version": source["dataset_version"],
            "lifecycle": source["lifecycle"],
            "series_id": source["series_id"],
            "metric": "rebound_speed_cm_s",
            "incident_speed_m_s": source["incident_speed_m_s"],
            "expected": source["expected_rebound_cm_s"],
            "unit": "cm/s",
            "standard_uncertainty": source["standard_uncertainty_cm_s"],
            "rigid_cushion_domain": source["rigid_cushion_domain"],
            "source_locator": source["source_locator"],
            "normalized_path": source["normalized_path"],
            "normalized_sha256": source["normalized_sha256"],
            "raw_extracted_path": source["raw_extracted_path"],
            "raw_extracted_sha256": source["raw_extracted_sha256"],
        })
    rows, provenance = _sudo_context(root)
    for point_id, speed in (
            ("cushion_e_low_speed", 1.8),
            ("cushion_e_all_speed", 2.5)):
        source = rows[point_id]
        base.append({
            **provenance,
            "point_id": point_id,
            "series_id": "sudo_cushion_restitution",
            "metric": source["metric"],
            "incident_speed_m_s": format(speed, ".17g"),
            "expected": source["expected"],
            "unit": source["unit"],
            "standard_uncertainty": format(
                _effective_uncertainty(source), ".17g"),
            "rigid_cushion_domain": "true",
            "source_locator": source["source_locator"],
        })
    return _require_spent_rows(
        root, sorted(base, key=lambda row: row["point_id"]))


def build_structural_rows(root):
    rows, provenance = _sudo_context(root)
    source = rows["cushion_contact_time_plateau"]
    return [{
        "point_id": source["point_id"],
        "dataset_id": provenance["dataset_id"],
        "dataset_version": provenance["dataset_version"],
        "lifecycle": provenance["lifecycle"],
        "metric": source["metric"],
        "expected": source["expected"],
        "unit": source["unit"],
        "observed": "",
        "status": "OUT_OF_MODEL_SPENT",
        "source_locator": source["source_locator"],
        "rationale": (
            "The production rigid impulse solver has no finite-duration "
            "contact state; no telemetry-only duration is fabricated."
        ),
    }]


def _csv_bytes(fields, rows):
    output = io.StringIO(newline="")
    writer = csv.DictWriter(output, fieldnames=fields, lineterminator="\n")
    writer.writeheader()
    writer.writerows(rows)
    return output.getvalue().encode("utf-8")


def write_v3_fit_inputs(root, output):
    output = Path(output)
    output.mkdir(parents=True, exist_ok=True)
    files = {
        "ball_collision_fit_v3_inputs.csv": _csv_bytes(
            BALL_FIELDS, build_ball_inputs(root)),
        "cushion_fit_v3_inputs.csv": _csv_bytes(
            CUSHION_FIELDS, build_cushion_inputs(root)),
        "sudo_2002_structural_residuals.csv": _csv_bytes(
            STRUCTURAL_FIELDS, build_structural_rows(root)),
    }
    for name, content in files.items():
        (output / name).write_bytes(content)
    return files


def main(argv=None):
    parser = argparse.ArgumentParser(
        description="Build phase 3 v3 spent-only fit inputs")
    parser.add_argument("--root", default=".", type=Path)
    parser.add_argument("--write", required=True, type=Path)
    arguments = parser.parse_args(argv)
    write_v3_fit_inputs(arguments.root, arguments.write)
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
