import unittest
from pathlib import Path

from tools.physics_validation.reference_adapter import default_reference_registry
from tools.physics_validation.reference_package import load_reference_package
from tools.physics_validation.reference_point import read_reference_points
from tools.physics_validation.reference_split import load_reference_split


PACKAGE = Path(__file__).parent / "reference_data/cross_2023_cue_impact"


class Cross2023AdapterTests(unittest.TestCase):
    def test_every_admitted_point_is_accounted_and_blockers_are_exact(self):
        package = load_reference_package(PACKAGE)
        points = read_reference_points(package.files["normalized"], package.manifest["dataset_id"])
        split = load_reference_split(
            package.files["split"], points, package.manifest["dataset_id"],
            package.manifest["dataset_version"])
        adaptation = default_reference_registry().adapt_with_limitations(package, split, points)
        self.assertEqual(adaptation.cases, ())
        self.assertEqual({item.case_id for item in adaptation.limitations}, {
            "full_text_not_acquired", "experimental_markers_not_admitted",
            "cue_speed_to_power_mapping_missing", "cue_contact_regime_telemetry_missing",
        })
        regime = next(
            item for item in adaptation.limitations
            if item.case_id == "cue_contact_regime_telemetry_missing")
        self.assertIn("tangential", regime.missing_evidence)
        self.assertIn("stick/slip", regime.resolution_condition)
        self.assertEqual(sum(len(case.points) for case in adaptation.cases), len(points))


if __name__ == "__main__":
    unittest.main()
