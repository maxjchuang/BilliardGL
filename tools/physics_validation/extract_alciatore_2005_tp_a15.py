import argparse
import csv
import hashlib
import io
import json
import math
import tempfile
from pathlib import Path

from .extract_confirmation_scalars import NORMALIZED_HEADER, RAW_HEADER


DATASET_ID = "alciatore_2005_tp_a15"
DATASET_VERSION = "1.0.0"
SOURCE_URL = "https://drdavepoolinfo.com/technical_proofs/new/TP_A-15.pdf"
SOURCE_TITLE = "TP A.15: Controlling the cue ball direction in a frozen cue ball shot"
SOURCE_AUTHOR = "David G. Alciatore"
SOURCE_LOCATOR = (
    "PDF p.2, M_exper: column <0> theta_exper; column <1> phi_exper"
)
PAIRS = (
    (0, 0),
    (8, 13),
    (20, 34),
    (34, 50),
    (46, 61),
    (57, 70),
    (67, 78),
    (77, 87),
    (90, 90),
)
RAW_EXTRACTED_HEADER = (
    "source_row_index",
    "source_column_0_label",
    "source_column_0_value_degrees",
    "source_column_1_label",
    "source_column_1_value_degrees",
    "cut_angle_phi_degrees",
    "target_angle_theta_degrees",
    "source_numeric_resolution_degrees",
    "source_locator",
)
JSON_NAMES = (
    "split.json",
    "extraction.json",
    "scenario_template.json",
    "expected_model_mismatches.json",
    "expected_reference_limitations.json",
    "source_access_audit.json",
)


def _canonical_json(document):
    return (json.dumps(
        document, ensure_ascii=False, indent=2, sort_keys=True,
        allow_nan=False) + "\n").encode("utf-8")


def _sha256(content):
    return "sha256:" + hashlib.sha256(content).hexdigest()


def _csv_bytes(header, rows):
    output = io.StringIO(newline="")
    writer = csv.DictWriter(output, fieldnames=header, lineterminator="\n")
    writer.writeheader()
    writer.writerows(rows)
    return output.getvalue().encode("utf-8")


def _point_id(phi):
    return f"alciatore_cut_{phi:03d}"


def _canonical_extracted_rows():
    return "".join(
        f"phi_exper={phi},theta_exper={theta}\n" for phi, theta in PAIRS
    ).encode("utf-8")


def _raw_rows():
    return [{
        "source_row_index": index,
        "source_column_0_label": "theta_exper",
        "source_column_0_value_degrees": theta,
        "source_column_1_label": "phi_exper",
        "source_column_1_value_degrees": phi,
        "cut_angle_phi_degrees": phi,
        "target_angle_theta_degrees": theta,
        "source_numeric_resolution_degrees": "0.5",
        "source_locator": SOURCE_LOCATOR,
    } for index, (phi, theta) in enumerate(PAIRS)]


def _scalar_row(point_id, role, group_id, metric, value, unit,
                *, tolerance="0", locator=SOURCE_LOCATOR,
                applicability="DIRECT"):
    return {
        "point_id": point_id,
        "role": role,
        "series_id": "alciatore_frozen_cue_ball",
        "group_id": group_id,
        "case_id": point_id,
        "metric": metric,
        "source_value": str(value),
        "source_unit": unit,
        "normalized_value": str(value),
        "normalized_unit": unit,
        "measurement_uncertainty": "0",
        "digitization_uncertainty": "0",
        "conversion_uncertainty": "0",
        "coverage_factor": "2",
        "engineering_absolute_tolerance": tolerance,
        "engineering_relative_tolerance": "0",
        "source_locator": locator,
        "pool_applicability": applicability,
    }


def _scalar_rows():
    # Half of the one-degree printed resolution, modeled as a rectangular
    # distribution. This is source-resolution metadata, not a confidence band.
    resolution_standard_uncertainty = format(0.5 / math.sqrt(3.0), ".17g")
    rows = []
    for phi, theta in PAIRS:
        row = _scalar_row(
            _point_id(phi),
            "confirmation_target",
            "frozen_cue_ball_angle_relation",
            "cue_ball_target_line_angle",
            theta,
            "degree",
            tolerance="3",
        )
        row["conversion_uncertainty"] = resolution_standard_uncertainty
        rows.append(row)
    rows.extend((
        _scalar_row(
            "source_frozen_ball_contact", "source_apparatus_fact",
            "source_apparatus", "frozen_ball_contact", 1, "dimensionless",
            applicability="NOT_APPLICABLE"),
        _scalar_row(
            "source_high_speed_video", "source_apparatus_fact",
            "source_apparatus", "high_speed_video_method", 1,
            "dimensionless", applicability="NOT_APPLICABLE"),
        _scalar_row(
            "contract_initial_ball_gap_cm", "scenario_contract",
            "confirmation_scenario_contract", "initial_ball_gap", 0, "cm",
            locator="Repository preregistered scenario contract",
            applicability="NOT_APPLICABLE"),
        _scalar_row(
            "contract_cue_elevation_degrees", "scenario_contract",
            "confirmation_scenario_contract", "cue_elevation", 0, "degree",
            locator="Repository preregistered scenario contract",
            applicability="NOT_APPLICABLE"),
        _scalar_row(
            "contract_centered_tip_lateral_fraction", "scenario_contract",
            "confirmation_scenario_contract", "cue_tip_lateral_offset", 0,
            "dimensionless", locator="Repository preregistered scenario contract",
            applicability="NOT_APPLICABLE"),
        _scalar_row(
            "contract_centered_tip_vertical_fraction", "scenario_contract",
            "confirmation_scenario_contract", "cue_tip_vertical_offset", 0,
            "dimensionless", locator="Repository preregistered scenario contract",
            applicability="NOT_APPLICABLE"),
    ))
    return rows


def _normalized_rows(scalars):
    rows = []
    for row in scalars:
        if row["role"] != "confirmation_target":
            continue
        rows.append({
            "dataset_id": DATASET_ID,
            "series_id": row["series_id"],
            "group_id": row["group_id"],
            "case_id": row["case_id"],
            "point_id": row["point_id"],
            # The shared reference schema calls every withheld row HOLDOUT;
            # confirmation-only state is enforced by the lifecycle registry.
            "partition": "HOLDOUT",
            "metric": row["metric"],
            "expected": row["normalized_value"],
            "unit": row["normalized_unit"],
            "measurement_uncertainty": row["measurement_uncertainty"],
            "digitization_uncertainty": row["digitization_uncertainty"],
            "conversion_uncertainty": row["conversion_uncertainty"],
            "coverage_factor": row["coverage_factor"],
            "engineering_absolute_tolerance": row[
                "engineering_absolute_tolerance"],
            "engineering_relative_tolerance": row[
                "engineering_relative_tolerance"],
            "source_locator": row["source_locator"],
            "pool_applicability": row["pool_applicability"],
        })
    return rows


def normalized_bytes(raw_path):
    # The raw table is deliberately source-shaped so the original Mathcad
    # column orientation remains auditable. Verify it before normalizing.
    with Path(raw_path).open(encoding="utf-8", newline="") as stream:
        reader = csv.DictReader(stream)
        if tuple(reader.fieldnames or ()) != RAW_EXTRACTED_HEADER:
            raise ValueError("Alciatore raw CSV header is invalid")
        rows = list(reader)
    observed = tuple(
        (int(row["cut_angle_phi_degrees"]),
         int(row["target_angle_theta_degrees"])) for row in rows)
    if observed != PAIRS:
        raise ValueError("Alciatore raw rows do not match the published pairs")
    return _csv_bytes(NORMALIZED_HEADER, _normalized_rows(_scalar_rows()))


def _documents(raw, normalized):
    rows_digest = _sha256(_canonical_extracted_rows())
    cases = {}
    for phi, theta in PAIRS:
        point_id = _point_id(phi)
        cases[point_id] = {
            "cut_angle_phi_degrees": phi,
            "evaluation_role": (
                "endpoint_invariant" if phi in {0, 90} else "interior_angle"),
            "reference_point_id": point_id,
            "target_angle_theta_degrees": theta,
        }
    return {
        "split.json": {
            "calibration_groups": [],
            "dataset_id": DATASET_ID,
            "dataset_version": DATASET_VERSION,
            "holdout_groups": ["frozen_cue_ball_angle_relation"],
            "schema_version": 1,
        },
        "extraction.json": {
            "date": "2026-07-15",
            "inputs": [{"file_id": "raw_extracted", "sha256": _sha256(raw)}],
            "method": (
                "Direct transcription of all nine rows in M_exper after "
                "visually checking the rendered primary PDF; Mathcad column "
                "<1> phi_exper is the input cut angle and column <0> "
                "theta_exper is the resulting cue-ball target-line angle"
            ),
            "operator": "Codex primary-source extraction",
            "output_sha256": _sha256(normalized),
            "review": {
                "date": "2026-07-15",
                "method": (
                    "Text extraction plus visual review of PDF pages 1-3, "
                    "including the diagram, M_exper assignments, and plot axes"
                ),
                "reviewed_by": "Codex primary-source visual cross-check",
            },
            "rounding_policy": (
                "Preserve every published integer degree exactly; interpret "
                "the printed last-place half-unit as rectangular source "
                "resolution, not as an experimental confidence interval"
            ),
            "schema_version": 2,
            "script": {
                "module": (
                    "tools.physics_validation."
                    "extract_alciatore_2005_tp_a15"
                ),
                "version": DATASET_VERSION,
            },
            "source_sha256": rows_digest,
            "tool": {
                "name": "Python standard-library deterministic transcriber",
                "version": "2026-07-15",
            },
            "transformations": [{
                "formula": (
                    "phi_exper (cut angle) -> theta_exper "
                    "(cue-ball target-line angle)"
                ),
                "id": "mathcad_column_orientation",
                "input_unit": "degree",
                "output_unit": "degree",
            }],
            "uncertainty_interpretation": (
                "The source prints integer degrees without confidence "
                "intervals. A 0.5-degree half-unit divided by sqrt(3) records "
                "only rectangular numeric resolution; the 3/5-degree "
                "engineering gates are preregistered separately."
            ),
        },
        "scenario_template.json": {
            "base_scenario": {
                "description": (
                    "Frozen cue-ball target-line confirmation from "
                    "Alciatore TP A.15"
                ),
                "evidence": {
                    "grade": "B",
                    "pool_applicability": "DIRECT",
                    "source": DATASET_ID,
                },
                "expectations": [
                    {"metric": "finite_state", "operator": "eq", "value": True},
                    {"metric": "nonincreasing_translational_energy",
                     "operator": "eq", "value": True},
                ],
                "schema_version": 6,
            },
            "cases": cases,
            "coordinate_contract": {
                "engine_table_plane": "x-z",
                "engine_vertical_axis": "+y",
                "impact_line_unit_vector": [1, 0, 0],
                "source_cut_angle_definition": (
                    "phi_exper is the angle from the impact line (line of "
                    "centers) toward the cue aiming direction"
                ),
                "source_target_angle_definition": (
                    "theta_exper is the angle from the impact line to the "
                    "resulting cue-ball target-line direction"
                ),
                "cue_direction_formula": "[cos(phi), 0, sin(phi)]",
                "observed_theta_formula": "atan2(cue_ball_vz, cue_ball_vx)",
                "positive_tangent_unit_vector": [0, 0, 1],
            },
            "grazing_target_to_incident_speed_ratio_maximum": 1e-3,
            "head_on_direction_error_degrees_maximum": 1,
            "head_on_lateral_to_incident_speed_ratio_maximum": 1e-3,
            "interior_absolute_error_degrees_maximum": 5,
            "interior_rmse_degrees_maximum": 3,
            "schema_version": 1,
        },
        "expected_model_mismatches.json": {
            "failures": [],
            "schema_version": 1,
        },
        "expected_reference_limitations.json": {
            "failures": [{
                "code": "SOURCE_APPARATUS_UNDERSPECIFIED",
                "dataset_id": DATASET_ID,
                "rationale": (
                    "TP A.15 does not publish ball dimensions, masses, cloth, "
                    "cue speed, or cue-tip properties for HSV A.97; the "
                    "comparison validates the angular relation, not those "
                    "absolute apparatus parameters."
                ),
                "scope": "absolute_contact_parameters",
            }, {
                "code": "NO_PUBLISHED_CONFIDENCE_INTERVAL",
                "dataset_id": DATASET_ID,
                "rationale": (
                    "The report prints integer-degree pairs but no repeat "
                    "count, dispersion, or confidence interval."
                ),
                "scope": "experimental_uncertainty",
            }],
            "schema_version": 1,
        },
        "source_access_audit.json": {
            "acquisition_method": (
                "Direct read-only download of the public primary PDF followed "
                "by text extraction and 160-DPI visual page review"
            ),
            "audited_on": "2026-07-15",
            "author": SOURCE_AUTHOR,
            "canonical_extracted_rows_sha256": rows_digest,
            "copyright_policy": (
                "Commit extracted numerical facts and provenance only; do not "
                "redistribute the PDF or HSV A.97 video."
            ),
            "document_title": SOURCE_TITLE,
            "independent_mapping_review": {
                "reviewed_extraction_sha256": rows_digest,
                "reviewed_on": "2026-07-15",
                "reviewed_by": "project owner",
                "status": "APPROVED",
            },
            "method": (
                "High-speed-camera super-slow-motion video HSV A.97 with "
                "gradually increased cut angle"
            ),
            "official_source_url": SOURCE_URL,
            "source_hash_scope": (
                "canonical extracted phi_exper/theta_exper rows; the "
                "third-party source media is intentionally uncommitted"
            ),
            "source_media_committed": False,
        },
    }


def generated_files():
    raw = _csv_bytes(RAW_EXTRACTED_HEADER, _raw_rows())
    scalars = _csv_bytes(RAW_HEADER, _scalar_rows())
    with tempfile.TemporaryDirectory() as directory:
        raw_path = Path(directory) / "raw_extracted.csv"
        raw_path.write_bytes(raw)
        normalized = normalized_bytes(raw_path)
    files = {
        "raw_extracted.csv": raw,
        "normalized.csv": normalized,
        "scalars.csv": scalars,
    }
    files.update({name: _canonical_json(document)
                  for name, document in _documents(raw, normalized).items()})
    logical_ids = {
        "raw_extracted.csv": "raw_extracted",
        "normalized.csv": "normalized",
        "scalars.csv": "scalars",
        "split.json": "split",
        "extraction.json": "extraction",
        "scenario_template.json": "scenario_template",
        "expected_model_mismatches.json": "expected_model_mismatches",
        "expected_reference_limitations.json": "expected_reference_limitations",
        "source_access_audit.json": "source_access_audit",
    }
    rows_digest = _sha256(_canonical_extracted_rows())
    manifest = {
        "acquisition": {
            "license_status": "author-publication-no-redistribution-grant-recorded",
            "retrieved_on": "2026-07-15",
            "source_hash_scope": "canonical_extracted_rows",
            "source_media_committed": False,
            "source_sha256": rows_digest,
            "source_url": SOURCE_URL,
        },
        "adapter_id": "alciatore_2005_tp_a15_confirmation_v1",
        "apparatus": {
            "balls_initially_frozen": True,
            "camera_method": "high_speed_video_HSV_A.97",
            "cue_contact": "centered_horizontal_repository_contract",
            "unpublished_properties": [
                "ball_diameter", "ball_mass", "cloth", "cue_speed",
                "cue_tip_properties",
            ],
        },
        "dataset_id": DATASET_ID,
        "dataset_version": DATASET_VERSION,
        "evidence": {
            "candidate_selection_input": False,
            "confirmation_only": True,
            "grade": "B",
            "hard_contract": (
                "seven-point angular RMSE/maximum error plus two endpoint "
                "invariant cases"
            ),
            "method": "published high-speed-video integer-degree pairs",
            "target_count": len(PAIRS),
        },
        "extraction_review": {
            "date": "2026-07-15",
            "method": "rendered primary-PDF diagram/table/axis cross-check",
            "reviewed_by": "Codex primary-source visual cross-check",
        },
        "files": [{
            "id": logical_ids[name],
            "path": name,
            "sha256": _sha256(files[name]),
        } for name in (
            "raw_extracted.csv", "normalized.csv", "scalars.csv",
            "split.json", "extraction.json", "scenario_template.json",
            "expected_model_mismatches.json",
            "expected_reference_limitations.json", "source_access_audit.json",
        )],
        "schema_version": 1,
        "source": {
            "authors": [SOURCE_AUTHOR],
            "document_id": "TP_A-15",
            "originally_posted": "2005-07-29",
            "title": SOURCE_TITLE,
            "year": 2005,
        },
    }
    files["manifest.json"] = _canonical_json(manifest)
    return files


def write_package(path):
    package = Path(path)
    package.mkdir(parents=True, exist_ok=True)
    files = generated_files()
    for name, content in files.items():
        (package / name).write_bytes(content)
    return files


def verify_package(path):
    package = Path(path)
    expected = generated_files()
    return [name for name, content in expected.items()
            if not (package / name).is_file()
            or (package / name).read_bytes() != content]


def main(argv=None):
    parser = argparse.ArgumentParser(
        description="Generate or verify Alciatore TP A.15 numeric evidence")
    mode = parser.add_mutually_exclusive_group(required=True)
    mode.add_argument("--output", type=Path)
    mode.add_argument("--verify", type=Path)
    arguments = parser.parse_args(argv)
    if arguments.output is not None:
        write_package(arguments.output)
        print(f"{DATASET_ID} {DATASET_VERSION} written")
        return 0
    differences = verify_package(arguments.verify)
    if differences:
        raise SystemExit("Alciatore package differs: " + ", ".join(differences))
    print(f"{DATASET_ID} {DATASET_VERSION} verified")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
