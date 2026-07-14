import json
import unittest
from pathlib import Path

from tools.physics_validation.reference_adapter import default_reference_registry
from tools.physics_validation.reference_package import load_reference_package
from tools.physics_validation.reference_point import read_reference_points
from tools.physics_validation.reference_split import load_reference_split


PACKAGE = Path(__file__).parent / "reference_data/multi_contact_solver_analytic_contract"


class MultiContactSolverAnalyticAdapterTests(unittest.TestCase):
    def test_grade_c_package_has_immutable_balanced_partitions(self):
        package = load_reference_package(PACKAGE)
        points = read_reference_points(package.files["normalized"], package.manifest["dataset_id"])
        split = load_reference_split(package.files["split"], points,
                                     package.manifest["dataset_id"], package.manifest["dataset_version"])
        adaptation = default_reference_registry().adapt(package, split, points)
        self.assertEqual(package.manifest["evidence"]["grade"], "C")
        self.assertEqual(sum(case.partition == "CALIBRATION" for case in adaptation), 4)
        self.assertEqual(sum(case.partition == "HOLDOUT" for case in adaptation), 4)
        self.assertTrue(all(json.loads(case.scenario_json)["schema_version"] == 8 for case in adaptation))

    def test_holdout_mutation_cannot_change_calibration_scenarios(self):
        package = load_reference_package(PACKAGE)
        points = read_reference_points(package.files["normalized"], package.manifest["dataset_id"])
        split = load_reference_split(package.files["split"], points,
                                     package.manifest["dataset_id"], package.manifest["dataset_version"])
        registry = default_reference_registry()
        baseline = {case.case_id: case.scenario_json for case in registry.adapt(package, split, points)
                    if case.partition == "CALIBRATION"}
        changed = tuple(point if split.partition_for(point) == "CALIBRATION" else
                        point.__class__(**{**point.__dict__, "expected": point.expected + 999})
                        for point in points)
        modified = {case.case_id: case.scenario_json for case in registry.adapt(package, split, changed)
                    if case.partition == "CALIBRATION"}
        self.assertEqual(baseline, modified)


if __name__ == "__main__":
    unittest.main()
