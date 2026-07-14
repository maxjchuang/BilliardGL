import unittest
import json
import tempfile
from dataclasses import replace
from pathlib import Path

from tools.physics_validation.adapters.domenech_2023 import adapt_domenech_2023
from tools.physics_validation.reference_adapter import default_reference_registry
from tools.physics_validation.reference_accounting import ReferenceAccounting, ReferenceFailureKey
from tools.physics_validation.reference_package import ReferencePackageError, load_reference_package
from tools.physics_validation.reference_point import read_reference_points
from tools.physics_validation.reference_report import write_reference_reports
from tools.physics_validation.reference_split import load_reference_split


PACKAGE_PATH = Path(__file__).parent / "reference_data/domenech_2023_ball_collision"


class DomenechAdapterTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.package = load_reference_package(PACKAGE_PATH)
        cls.points = read_reference_points(cls.package.files["normalized"], cls.package.manifest["dataset_id"])
        cls.split = load_reference_split(
            cls.package.files["split"], cls.points,
            cls.package.manifest["dataset_id"], cls.package.manifest["dataset_version"])

    def test_all_material_series_are_accounted_as_structured_limitations(self):
        first = adapt_domenech_2023(self.package, self.split, self.points)
        second = adapt_domenech_2023(self.package, self.split, self.points)

        self.assertEqual(first, second)
        self.assertEqual(first.cases, ())
        self.assertEqual(
            {item.case_id for item in first.limitations},
            {
                "author_data_request_pending",
                "version_record_pdf_audit_pending",
                "billiard_alpha1_source_geometry_not_expressible",
                "billiard_delta2_source_geometry_not_expressible",
                "brass_alpha1_source_geometry_not_expressible",
                "rubber_delta2_source_geometry_not_expressible",
                "rubber_lambda2_source_geometry_not_expressible",
                "steel_alpha1_source_geometry_not_expressible",
                "steel_beta1_source_geometry_not_expressible",
            },
        )
        covered = {
            point_id
            for limitation in first.limitations
            for point_id in limitation.point_ids
        }
        self.assertEqual(covered, {point.point_id for point in self.points})

    def test_limitation_only_points_remain_visible_in_reports(self):
        adaptation = adapt_domenech_2023(self.package, self.split, self.points)
        known = frozenset(
            ReferenceFailureKey(
                item.dataset_id, item.case_id, "REFERENCE_LIMITATION", item.metric)
            for item in adaptation.limitations
        )
        empty = frozenset()
        accounting = ReferenceAccounting(
            known_model_mismatches=empty,
            new_model_mismatches=empty,
            missing_model_mismatches=empty,
            known_limitations=known,
            new_limitations=empty,
            missing_limitations=empty,
            unallowlistable_failures=empty,
        )
        with tempfile.TemporaryDirectory() as temporary:
            json_path, csv_path, markdown_path = write_reference_reports(
                adaptation.cases,
                (),
                accounting,
                Path(temporary),
                {"build_id": "test", "scenarios": {}},
                points=self.points,
                limitations=adaptation.limitations,
            )
            payload = json.loads(json_path.read_text(encoding="utf-8"))
            csv_lines = csv_path.read_text(encoding="utf-8").splitlines()
            markdown = markdown_path.read_text(encoding="utf-8")

        rows = [
            row
            for partition in ("CALIBRATION", "HOLDOUT")
            for row in payload["partitions"][partition]["points"]
        ]
        self.assertEqual(len(rows), 214)
        self.assertEqual(len(csv_lines), 215)
        self.assertEqual({row["point_id"] for row in rows}, {point.point_id for point in self.points})
        self.assertTrue(all(row["status"] == "REFERENCE_LIMITATION_KNOWN" for row in rows))
        self.assertTrue(all(row["experimental_value"] is not None for row in rows))
        self.assertTrue(all(row["source_locator"] for row in rows))
        self.assertTrue(all(row["missing_evidence"] for row in rows))
        self.assertTrue(all(row["resolution_condition"] for row in rows))
        self.assertEqual(len(payload["reference_limitations"]), 9)
        self.assertTrue(all(item["missing_evidence"] for item in payload["reference_limitations"]))
        self.assertTrue(all(item["resolution_condition"] for item in payload["reference_limitations"]))
        self.assertIn("version-of-record PDF", markdown)

    def test_registry_exposes_domenech_adapter_without_regressing_prior_adapter(self):
        registry = default_reference_registry()
        adaptation = registry.adapt_with_limitations(self.package, self.split, self.points)

        self.assertEqual(adaptation, adapt_domenech_2023(self.package, self.split, self.points))

    def test_mixed_material_case_is_rejected(self):
        billiard = next(point for point in self.points if point.series_id == "billiard_alpha1")
        brass = next(point for point in self.points if point.series_id == "brass_alpha1")
        mixed = (billiard, replace(brass, case_id=billiard.case_id))

        with self.assertRaisesRegex(ReferencePackageError, "MIXED_MATERIAL_CASE"):
            adapt_domenech_2023(self.package, self.split, mixed)


if __name__ == "__main__":
    unittest.main()
