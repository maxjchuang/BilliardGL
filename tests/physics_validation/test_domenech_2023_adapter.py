import unittest
from dataclasses import replace
from pathlib import Path

from tools.physics_validation.adapters.domenech_2023 import adapt_domenech_2023
from tools.physics_validation.reference_adapter import default_reference_registry
from tools.physics_validation.reference_package import ReferencePackageError, load_reference_package
from tools.physics_validation.reference_point import read_reference_points
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
