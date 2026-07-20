import json
import subprocess
import sys
import unittest
from pathlib import Path

from tools.physics_validation.reference_adapter import default_reference_registry
from tools.physics_validation.reference_package import load_reference_package
from tools.physics_validation.reference_point import read_reference_points
from tools.physics_validation.reference_split import load_reference_split


PACKAGE = Path(__file__).parent / "reference_data/cue_contact_analytic_contract"


class CueContactAnalyticAdapterTests(unittest.TestCase):
    def _adapt(self):
        package = load_reference_package(PACKAGE)
        points = read_reference_points(
            package.files["normalized"], package.manifest["dataset_id"])
        split = load_reference_split(
            package.files["split"], points, package.manifest["dataset_id"],
            package.manifest["dataset_version"])
        return package, points, default_reference_registry().adapt_with_limitations(
            package, split, points)

    def test_package_is_complete_grade_c_and_reproducible(self):
        package, points, adaptation = self._adapt()
        self.assertEqual(package.manifest["evidence"]["grade"], "C")
        self.assertTrue(points)
        self.assertTrue(all(
            point.source_locator.startswith("analytic:rigid-impulse:") and
            point.pool_applicability == "NOT_APPLICABLE"
            for point in points))
        completed = subprocess.run([
            sys.executable, "-m", "tools.physics_validation.generate_cue_contact_analytic",
            "--package", str(PACKAGE), "--check",
        ], capture_output=True, text=True, check=False)
        self.assertEqual(completed.returncode, 0, completed.stderr)
        self.assertEqual(sum(len(case.points) for case in adaptation.cases) +
                         sum(len(item.point_ids) for item in adaptation.limitations),
                         len(points))

    def test_adapter_emits_only_v4_inputs_and_committed_partitions(self):
        _, _, adaptation = self._adapt()
        self.assertEqual({case.case_id for case in adaptation.cases}, {
            "center_hit", "positive_vertical_stick", "negative_vertical_stick",
            "stick_boundary_inner", "left_mirror", "right_mirror",
            "horizontal_slip", "miscue",
        })
        self.assertEqual(adaptation.limitations, ())
        for case in adaptation.cases:
            scenario = json.loads(case.scenario_json)
            self.assertEqual(scenario["schema_version"], 4)
            self.assertIn("cue_impact", scenario)
            self.assertEqual(scenario["balls"][0]["velocity_cm_s"], [0.0, 0.0, 0.0])
            self.assertEqual(
                scenario["balls"][0]["angular_velocity_rad_s"], [0.0, 0.0, 0.0])
            expected_partition = "CALIBRATION" if case.case_id in {
                "center_hit", "positive_vertical_stick", "negative_vertical_stick",
                "stick_boundary_inner",
            } else "HOLDOUT"
            self.assertEqual(case.partition, expected_partition)
            for expectation in scenario["expectations"]:
                selection = expectation["value"]["selection"]
                self.assertIn(selection["expected_regime"], {"stick", "slip", "miscue"})


if __name__ == "__main__":
    unittest.main()
