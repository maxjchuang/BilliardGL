import csv
import json
import math
import unittest
from pathlib import Path

from tools.physics_validation.data_lifecycle import load_data_lifecycle
from tools.physics_validation.extract_han_2005 import han_restitution
from tools.physics_validation.reference_package import load_reference_package
from tools.physics_validation.reference_point import read_reference_points


ROOT = Path(__file__).resolve().parents[2]
HAN = ROOT / "tests/physics_validation/reference_data/han_2005"
STATUS = ROOT / "tests/physics_validation/validation_data_status.json"
SOURCE_SHA256 = (
    "sha256:22bfcd09368da94ce90c5f0f953d0fadf8f15a00163b6c7384109927546c5f3d"
)
SPEEDS_M_S = (0.5, 1.0, 1.5, 2.0, 2.5)


def csv_rows(path):
    with Path(path).open(encoding="utf-8", newline="") as stream:
        return list(csv.DictReader(stream))


class Han2005ExtractionTests(unittest.TestCase):
    def test_empirical_relation_reproduces_full_precision_points(self):
        rows = csv_rows(HAN / "scalars.csv")
        targets = [row for row in rows if row["role"] == "confirmation_target"]
        self.assertEqual(
            [float(row["normalized_value"]) for row in targets],
            [han_restitution(speed) for speed in SPEEDS_M_S],
        )
        self.assertTrue(all(math.isfinite(float(row["normalized_value"]))
                            for row in targets))

    def test_han_is_confirmation_and_absolute_values_are_transfer_limited(self):
        entry = load_data_lifecycle(STATUS).entry("han_2005", "1.0.0")
        self.assertEqual(
            (entry.calibration_status, entry.holdout_status),
            ("confirmation", "confirmation"),
        )
        points = read_reference_points(HAN / "normalized.csv", "han_2005")
        self.assertEqual(
            {point.pool_applicability for point in points},
            {"TRANSFER_LIMITED"},
        )

    def test_hard_contract_was_fixed_without_candidate_predictions(self):
        contract = json.loads(
            (HAN / "scenario_template.json").read_text(encoding="utf-8"))
        self.assertEqual(contract["normalized_curve_rmse_maximum"], 0.15)
        self.assertEqual(
            contract["hard_metrics"],
            [
                "normalized_curve_rmse",
                "finite_bounded_response",
                "continuous_response",
                "source_domain_response",
                "nonincreasing_total_energy",
            ],
        )
        self.assertNotIn("observed", json.dumps(contract, sort_keys=True))

    def test_source_audit_binds_the_locally_audited_primary_pdf(self):
        package = load_reference_package(HAN)
        self.assertEqual(package.manifest["acquisition"]["source_sha256"],
                         SOURCE_SHA256)
        audit = json.loads(
            package.files["source_access_audit"].read_text(encoding="utf-8"))
        self.assertEqual(audit["source_sha256"], SOURCE_SHA256)
        self.assertEqual(audit["wayback_timestamp"], "20250131133605")
        self.assertFalse(audit["source_media_committed"])


if __name__ == "__main__":
    unittest.main()
