import argparse
import csv
import hashlib
import io
import json
import math
import tempfile
from pathlib import Path

from .extract_confirmation_scalars import NORMALIZED_HEADER, RAW_HEADER
from .extract_confirmation_scalars import normalized_bytes as _normalized_bytes


DATASET_ID = "han_2005"
DATASET_VERSION = "1.0.0"
COEFFICIENTS = (0.39, 0.257, -0.044)
COEFFICIENT_HALF_UNITS = (0.005, 0.0005, 0.0005)
SPEEDS_M_S = (0.5, 1.0, 1.5, 2.0, 2.5)
SOURCE_SHA256 = (
    "sha256:22bfcd09368da94ce90c5f0f953d0fadf8f15a00163b6c7384109927546c5f3d"
)
ORIGINAL_URL = (
    "https://www.electronicsandbooks.com/edt/manual/Magazine/J/"
    "Journal%20of%20Mechanical%20Science%20and%20Technology/"
    "2005%20Volume%2019/4/976-984.pdf"
)
ARCHIVE_URL = (
    "https://web.archive.org/web/20250131133605id_/" + ORIGINAL_URL
)
LOCATOR = "PDF p.7 / article p.982, Eq. (26)"
JSON_FILES = (
    "split.json",
    "extraction.json",
    "scenario_template.json",
    "expected_model_mismatches.json",
    "expected_reference_limitations.json",
    "source_access_audit.json",
)


def han_restitution(speed_m_s):
    speed = float(speed_m_s)
    a, b, c = COEFFICIENTS
    value = a + b * speed + c * speed * speed
    if not math.isfinite(value):
        raise ValueError("Han restitution must be finite")
    return value


def _canonical_json(document):
    return (json.dumps(
        document, ensure_ascii=False, indent=2, sort_keys=True,
        allow_nan=False) + "\n").encode("utf-8")


def _sha256(data):
    return "sha256:" + hashlib.sha256(data).hexdigest()


def _decimal(value):
    return format(value, ".17g")


def _coefficient_uncertainty(speed):
    # Printed coefficient resolution is treated as an independent rectangular
    # distribution. This is metadata, never a fitted candidate tolerance.
    components = (
        COEFFICIENT_HALF_UNITS[0],
        speed * COEFFICIENT_HALF_UNITS[1],
        speed * speed * COEFFICIENT_HALF_UNITS[2],
    )
    return math.sqrt(sum((value / math.sqrt(3.0)) ** 2
                         for value in components))


def _csv_bytes(header, rows):
    output = io.StringIO(newline="")
    writer = csv.DictWriter(output, fieldnames=header, lineterminator="\n")
    writer.writeheader()
    writer.writerows(rows)
    return output.getvalue().encode("utf-8")


def _raw_rows():
    rows = []
    coefficient_rows = (
        ("coefficient_a", "0.39", COEFFICIENT_HALF_UNITS[0]),
        ("coefficient_b", "0.257", COEFFICIENT_HALF_UNITS[1]),
        ("coefficient_c", "-0.044", COEFFICIENT_HALF_UNITS[2]),
    )
    for point_id, value, half_unit in coefficient_rows:
        rows.append({
            "point_id": point_id,
            "role": "published_coefficient",
            "series_id": "han_restitution_relation",
            "group_id": "cushion_restitution_coefficients",
            "case_id": point_id,
            "metric": point_id,
            "source_value": value,
            "source_unit": "dimensionless",
            "normalized_value": value,
            "normalized_unit": "dimensionless",
            "measurement_uncertainty": "0",
            "digitization_uncertainty": "0",
            "conversion_uncertainty": _decimal(half_unit / math.sqrt(3.0)),
            "coverage_factor": "2",
            "engineering_absolute_tolerance": "0",
            "engineering_relative_tolerance": "0",
            "source_locator": LOCATOR,
            "pool_applicability": "NOT_APPLICABLE",
        })
    for speed in SPEEDS_M_S:
        suffix = int(round(speed * 100.0))
        point_id = f"han_speed_{suffix:03d}"
        value = han_restitution(speed)
        rows.append({
            "point_id": point_id,
            "role": "confirmation_target",
            "series_id": "han_restitution_curve",
            "group_id": "cushion_restitution",
            "case_id": point_id,
            "metric": "cushion_normal_restitution",
            "source_value": _decimal(value),
            "source_unit": "dimensionless",
            "normalized_value": _decimal(value),
            "normalized_unit": "dimensionless",
            "measurement_uncertainty": "0",
            "digitization_uncertainty": "0",
            "conversion_uncertainty": _decimal(
                _coefficient_uncertainty(speed)),
            "coverage_factor": "2",
            "engineering_absolute_tolerance": "0",
            "engineering_relative_tolerance": "0",
            "source_locator": LOCATOR,
            "pool_applicability": "TRANSFER_LIMITED",
        })
    return rows


def normalized_bytes(raw_path):
    return _normalized_bytes(raw_path, DATASET_ID)


def _documents(raw_bytes, normalized):
    cases = {}
    for speed in SPEEDS_M_S:
        suffix = int(round(speed * 100.0))
        point_id = f"han_speed_{suffix:03d}"
        cases[point_id] = {
            "incident_normal_speed_m_s": speed,
            "reference_point_id": point_id,
            "status": "confirmation_only_transfer_limited_absolute",
        }
    return {
        "split.json": {
            "calibration_groups": [],
            "dataset_id": DATASET_ID,
            "dataset_version": DATASET_VERSION,
            "holdout_groups": ["cushion_restitution"],
            "schema_version": 1,
        },
        "extraction.json": {
            "date": "2026-07-15",
            "inputs": [{
                "file_id": "raw_extracted",
                "sha256": _sha256(raw_bytes),
            }],
            "method": (
                "Deterministic evaluation of the empirical restitution "
                "polynomial printed in Eq. (26) of the locally audited "
                "primary publication"
            ),
            "operator": "Codex source-evidence audit",
            "output_sha256": _sha256(normalized),
            "review": {
                "date": "2026-07-15",
                "method": (
                    "Second-pass comparison of coefficients, apparatus, "
                    "page, and equation locator against all nine PDF pages"
                ),
                "reviewed_by": "Codex independent transcription review",
            },
            "rounding_policy": (
                "Preserve printed coefficients exactly; derived points use "
                "IEEE-754 double evaluation serialized with 17 significant "
                "digits; coefficient last-place half-units are independent "
                "rectangular uncertainties."
            ),
            "schema_version": 2,
            "script": {
                "module": "tools.physics_validation.extract_han_2005",
                "version": DATASET_VERSION,
            },
            "source_sha256": SOURCE_SHA256,
            "tool": {
                "name": "Python standard-library polynomial evaluator",
                "version": "2026-07-15",
            },
            "transformations": [{
                "formula": "e=0.39+0.257*v-0.044*v^2",
                "id": "han_equation_26",
                "input_unit": "m/s",
                "output_unit": "dimensionless",
            }],
            "uncertainty_interpretation": (
                "Independent standard uncertainties propagated from the "
                "printed coefficient resolutions; the 0.15 normalized-curve "
                "engineering threshold is preregistered separately."
            ),
        },
        "scenario_template.json": {
            "base_scenario": {
                "balls": [],
                "description": (
                    "Han 2005 cross-equipment cushion restitution trend "
                    "confirmation"
                ),
                "evidence": {
                    "grade": "B",
                    "pool_applicability": "TRANSFER_LIMITED",
                    "source": DATASET_ID,
                },
                "expectations": [],
                "schema_version": 6,
            },
            "cases": cases,
            "hard_metrics": [
                "normalized_curve_rmse",
                "finite_bounded_response",
                "continuous_response",
                "source_domain_response",
                "nonincreasing_total_energy",
            ],
            "normalization": {
                "equation": "curve_value=e(v)/e(0.5 m/s)",
                "reference_speed_m_s": 0.5,
                "source_evaluation_domain_m_s": [0.5, 2.5],
            },
            "normalized_curve_rmse_maximum": 0.15,
            "schema_version": 1,
        },
        "expected_model_mismatches.json": {
            "failures": [],
            "schema_version": 1,
        },
        "expected_reference_limitations.json": {
            "failures": [{
                "code": "EQUIPMENT_TRANSFER_LIMITATION",
                "dataset_id": DATASET_ID,
                "rationale": (
                    "Han used 65.5 mm, 230 g carom balls and a pocketless "
                    "three-cushion table; absolute restitution values cannot "
                    "directly validate Chinese-pool cushion parameters."
                ),
                "scope": "absolute_cushion_restitution",
            }, {
                "code": "SOURCE_DOMAIN_NOT_PRINTED",
                "dataset_id": DATASET_ID,
                "rationale": (
                    "The paper prints Eq. (26) and a 0.6-0.95 measured range "
                    "but no explicit velocity endpoints; 0.5-2.5 m/s is the "
                    "preregistered evaluation domain, not a claimed apparatus "
                    "sampling range."
                ),
                "scope": "velocity_domain",
            }],
            "schema_version": 1,
        },
        "source_access_audit.json": {
            "acquired_copy_url": ARCHIVE_URL,
            "acquisition_method": (
                "Internet Archive raw snapshot of the public primary-PDF "
                "mirror, audited locally as a nine-page PDF"
            ),
            "audited_on": "2026-07-15",
            "copyright_policy": (
                "Commit extracted numeric facts and provenance only; do not "
                "redistribute the publication PDF or rendered pages."
            ),
            "official_landing_url": (
                "https://link.springer.com/article/10.1007/BF02919180"
            ),
            "original_mirror_url": ORIGINAL_URL,
            "source_media_committed": False,
            "source_sha256": SOURCE_SHA256,
            "wayback_cdx_digest": "MZCDJNZ4FNSSUOUH5V5UFZVRP34UV4PZ",
            "wayback_timestamp": "20250131133605",
        },
    }


def package_bytes():
    raw = _csv_bytes(RAW_HEADER, _raw_rows())
    with tempfile.TemporaryDirectory() as directory:
        raw_path = Path(directory) / "raw_extracted.csv"
        raw_path.write_bytes(raw)
        normalized = normalized_bytes(raw_path)
    files = {
        "raw_extracted.csv": raw,
        "normalized.csv": normalized,
        "scalars.csv": raw,
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
    manifest = {
        "acquisition": {
            "acquired_copy_url": ARCHIVE_URL,
            "license_status": "publisher-copyright-no-redistribution-grant-recorded",
            "original_mirror_url": ORIGINAL_URL,
            "retrieved_on": "2026-07-15",
            "source_media_committed": False,
            "source_sha256": SOURCE_SHA256,
        },
        "adapter_id": "han_2005_confirmation_v1",
        "apparatus": {
            "ball_diameter_mm": 65.5,
            "ball_mass_g": 230,
            "game": "carom_three_cushion",
            "long_rail_length_mm": 2540,
            "short_rail_length_mm": 1270,
        },
        "dataset_id": DATASET_ID,
        "dataset_version": DATASET_VERSION,
        "evidence": {
            "absolute_values": "diagnostic_transfer_limited",
            "candidate_selection_input": False,
            "confirmation_only": True,
            "grade": "B",
            "hard_contract": (
                "normalized curve plus finite, bounded, continuous, source-"
                "domain, and non-increasing-energy invariants"
            ),
            "method": "published empirical cushion restitution relation",
            "target_count": len(SPEEDS_M_S),
        },
        "extraction_review": {
            "date": "2026-07-15",
            "method": "page-and-equation second-pass comparison",
            "reviewed_by": "Codex independent transcription review",
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
            "authors": ["Inhwan Han"],
            "doi": "10.1007/BF02919180",
            "journal": "Journal of Mechanical Science and Technology",
            "pages": "19(4):976-984",
            "title": "Dynamics in Carom and Three Cushion Billiards",
            "year": 2005,
        },
    }
    files["manifest.json"] = _canonical_json(manifest)
    return files


def write_or_check(package, check=False):
    package = Path(package)
    expected = package_bytes()
    differences = []
    for name, content in expected.items():
        path = package / name
        if check:
            if not path.is_file() or path.read_bytes() != content:
                differences.append(name)
        else:
            package.mkdir(parents=True, exist_ok=True)
            path.write_bytes(content)
    if check and differences:
        raise SystemExit("Han package differs: " + ", ".join(differences))
    return expected


def main(argv=None):
    parser = argparse.ArgumentParser(
        description="Generate or verify the Han 2005 confirmation package")
    parser.add_argument("--package", required=True, type=Path)
    parser.add_argument("--check", action="store_true")
    arguments = parser.parse_args(argv)
    write_or_check(arguments.package, arguments.check)
    print(f"{DATASET_ID} {DATASET_VERSION} " +
          ("verified" if arguments.check else "written"))
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
