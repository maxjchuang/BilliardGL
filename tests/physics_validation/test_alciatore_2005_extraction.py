import csv
import hashlib
import json
import unittest
from pathlib import Path

from tools.physics_validation.data_lifecycle import load_data_lifecycle
from tools.physics_validation.extract_alciatore_2005_tp_a15 import (
    PAIRS,
    generated_files,
    verify_package,
)
from tools.physics_validation.reference_package import load_reference_package
from tools.physics_validation.reference_point import read_reference_points


ROOT = Path(__file__).resolve().parents[2]
PACKAGE = (
    ROOT / "tests/physics_validation/reference_data/alciatore_2005_tp_a15"
)
STATUS = ROOT / "tests/physics_validation/validation_data_status.json"
PUBLISHED_PAIRS = (
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


class Alciatore2005ExtractionTests(unittest.TestCase):
    def test_all_published_phi_to_theta_pairs_are_preserved(self):
        self.assertEqual(PAIRS, PUBLISHED_PAIRS)
        with (PACKAGE / "raw_extracted.csv").open(
                encoding="utf-8", newline="") as stream:
            rows = list(csv.DictReader(stream))
        self.assertEqual(
            tuple((int(row["cut_angle_phi_degrees"]),
                   int(row["target_angle_theta_degrees"])) for row in rows),
            PUBLISHED_PAIRS,
        )

    def test_source_semantics_are_cue_ball_target_line_not_object_ball(self):
        extraction = json.loads(
            (PACKAGE / "extraction.json").read_text(encoding="utf-8"))
        transform = extraction["transformations"][0]
        self.assertEqual(transform["formula"],
                         "phi_exper (cut angle) -> theta_exper (cue-ball target-line angle)")
        points = read_reference_points(
            PACKAGE / "normalized.csv", "alciatore_2005_tp_a15")
        self.assertEqual({point.metric for point in points},
                         {"cue_ball_target_line_angle"})
        self.assertNotIn("target_ball_angle",
                         (PACKAGE / "normalized.csv").read_text(encoding="utf-8"))

    def test_package_is_spent_after_v5_regression(self):
        entry = load_data_lifecycle(STATUS).entry(
            "alciatore_2005_tp_a15", "1.0.0")
        self.assertEqual(
            (entry.calibration_status, entry.holdout_status),
            ("spent", "spent"),
        )
        package = load_reference_package(PACKAGE)
        self.assertFalse(package.manifest["evidence"]["candidate_selection_input"])
        self.assertTrue(package.manifest["evidence"]["confirmation_only"])

    def test_generated_package_is_byte_reproducible(self):
        self.assertEqual(verify_package(PACKAGE), [])
        expected = generated_files()
        self.assertEqual(set(expected), {
            "raw_extracted.csv", "normalized.csv", "scalars.csv",
            "split.json", "extraction.json", "scenario_template.json",
            "expected_model_mismatches.json",
            "expected_reference_limitations.json", "source_access_audit.json",
            "manifest.json",
        })

    def test_acceptance_contract_and_endpoint_roles_are_preregistered(self):
        scenario = json.loads(
            (PACKAGE / "scenario_template.json").read_text(encoding="utf-8"))
        self.assertEqual(scenario["interior_rmse_degrees_maximum"], 3)
        self.assertEqual(
            scenario["interior_absolute_error_degrees_maximum"], 5)
        self.assertEqual(
            scenario["head_on_lateral_to_incident_speed_ratio_maximum"], 1e-3)
        self.assertEqual(scenario["head_on_direction_error_degrees_maximum"], 1)
        self.assertEqual(
            scenario["grazing_target_to_incident_speed_ratio_maximum"], 1e-3)
        roles = {case["reference_point_id"]: case["evaluation_role"]
                 for case in scenario["cases"].values()}
        self.assertEqual(roles["alciatore_cut_000"], "endpoint_invariant")
        self.assertEqual(roles["alciatore_cut_090"], "endpoint_invariant")
        self.assertEqual(
            {role for point_id, role in roles.items()
             if point_id not in {"alciatore_cut_000", "alciatore_cut_090"}},
            {"interior_angle"},
        )

    def test_source_audit_hashes_canonical_rows_not_unvendored_media(self):
        audit = json.loads(
            (PACKAGE / "source_access_audit.json").read_text(encoding="utf-8"))
        self.assertFalse(audit["source_media_committed"])
        self.assertEqual(audit["audited_on"], "2026-07-15")
        canonical_rows = "".join(
            f"phi_exper={phi},theta_exper={theta}\n"
            for phi, theta in PUBLISHED_PAIRS
        ).encode("utf-8")
        self.assertEqual(
            audit["canonical_extracted_rows_sha256"],
            "sha256:" + hashlib.sha256(canonical_rows).hexdigest(),
        )
        self.assertEqual(audit["independent_mapping_review"], {
            "reviewed_by": "project owner",
            "reviewed_extraction_sha256": audit[
                "canonical_extracted_rows_sha256"],
            "reviewed_on": "2026-07-15",
            "status": "APPROVED",
        })


if __name__ == "__main__":
    unittest.main()
