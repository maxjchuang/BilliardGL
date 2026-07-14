import json
import unittest
from dataclasses import replace
from pathlib import Path

from tools.physics_validation.adapters.mathavan_2010 import adapt_mathavan_2010
from tools.physics_validation.reference_adapter import default_reference_registry
from tools.physics_validation.reference_package import ReferencePackageError, load_reference_package
from tools.physics_validation.reference_point import read_reference_points
from tools.physics_validation.reference_split import load_reference_split


PACKAGE_PATH = Path(__file__).parent / "reference_data/mathavan_2010_cushion"


class Mathavan2010AdapterTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.package = load_reference_package(PACKAGE_PATH)
        cls.points = read_reference_points(cls.package.files["normalized"], cls.package.manifest["dataset_id"])
        cls.split = load_reference_split(cls.package.files["split"], cls.points, cls.package.manifest["dataset_id"], cls.package.manifest["dataset_version"])

    def test_adaptation_is_order_independent_and_preserves_partition(self):
        first = adapt_mathavan_2010(self.package, self.split, self.points)
        second = adapt_mathavan_2010(self.package, self.split, tuple(reversed(self.points)))
        self.assertEqual(first, second)
        self.assertEqual(len(first), 19)
        for case in first:
            self.assertEqual(case.partition, case.points[0].partition)

    def test_scenario_is_perpendicular_pure_roll_and_has_paired_windows(self):
        case = adapt_mathavan_2010(self.package, self.split, self.points)[0]
        scenario = json.loads(case.scenario_json)
        ball = scenario["balls"][0]
        speed = ball["velocity_cm_s"][0]
        self.assertEqual(ball["velocity_cm_s"][2], 0.0)
        self.assertAlmostEqual(ball["angular_velocity_rad_s"][2], -speed / 2.625)
        self.assertEqual(ball["angular_velocity_rad_s"][1], 0.0)
        selection = scenario["expectations"][0]["value"]["selection"]
        self.assertEqual(selection["incident_window_ticks"], 3)
        self.assertEqual(selection["rebound_window_ticks"], 3)
        provenance = json.loads(case.provenance_json)
        self.assertIn("fit_subset", provenance)
        self.assertIn("rigid_cushion_domain", provenance)
        self.assertNotIn("coefficient_of_restitution", scenario)
        self.assertEqual(scenario["schema_version"], 6)
        profile = scenario["physics_profile"]
        self.assertEqual(profile["ball"]["mass_kg"], 0.1406)
        self.assertEqual(profile["ball"]["radius_cm"], 2.625)
        self.assertEqual(profile["cushion"]["nose_height_ratio"], 1.4)
        self.assertEqual(
            profile["cushion"]["maximum_rigid_incident_speed_cm_s"], 250.0)

    def test_theory_only_series_is_rejected(self):
        theory = replace(self.points[0], series_id="fig8_theory_curve")
        with self.assertRaisesRegex(ReferencePackageError, "THEORY_EVIDENCE_REJECTED"):
            adapt_mathavan_2010(self.package, self.split, (theory,))

    def test_registry_preserves_source_limitations(self):
        adaptation = default_reference_registry().adapt_with_limitations(self.package, self.split, self.points)
        self.assertEqual(len(adaptation.cases), 19)
        self.assertEqual(len(adaptation.limitations), 4)


if __name__ == "__main__":
    unittest.main()
