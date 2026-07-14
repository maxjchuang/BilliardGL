import json
import math
import unittest
from dataclasses import replace
from pathlib import Path

from tools.physics_validation.adapters.mathavan_2010 import adapt_mathavan_2010
from tools.physics_validation.fit_cushion import (
    build_fit_report, fit_cushion_parameters, read_incident_inputs)
from tools.physics_validation.reference_package import load_reference_package
from tools.physics_validation.reference_point import read_reference_points
from tools.physics_validation.reference_split import load_reference_split


PACKAGE_PATH = Path(__file__).parent / "reference_data/mathavan_2010_cushion"
FIT_PATH = Path(__file__).parents[2] / "physics_models/calibration/cushion_fit_v1.json"


class CushionFitTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.package = load_reference_package(PACKAGE_PATH)
        cls.points = read_reference_points(
            cls.package.files["normalized"], cls.package.manifest["dataset_id"])
        cls.split = load_reference_split(
            cls.package.files["split"], cls.points,
            cls.package.manifest["dataset_id"], cls.package.manifest["dataset_version"])
        cls.inputs = read_incident_inputs(cls.package.files["raw_extracted"])

    def test_fit_is_bounded_deterministic_and_calibration_only(self):
        first = fit_cushion_parameters(self.points, self.split, self.inputs)
        second = fit_cushion_parameters(tuple(reversed(self.points)), self.split, self.inputs)
        self.assertEqual(first, second)
        self.assertTrue(0.0 <= first["normal_restitution"] <= 1.0)
        self.assertTrue(0.0 <= first["friction_coefficient"] <= 1.0)
        self.assertTrue(math.isfinite(first["objective_mean_squared_cm_s2"]))
        self.assertTrue(all(
            self.split.partition_for(point) == "CALIBRATION"
            for point in self.points
            if point.point_id in first["calibration_point_ids"]))

    def test_holdout_values_cannot_change_fit_or_calibration_scenarios(self):
        baseline_fit = fit_cushion_parameters(self.points, self.split, self.inputs)
        baseline_cases = {
            case.case_id: case.scenario_json for case in
            adapt_mathavan_2010(self.package, self.split, self.points)
            if case.partition == "CALIBRATION"
        }
        changed = tuple(
            replace(point, expected=point.expected + 10000.0)
            if self.split.partition_for(point) == "HOLDOUT" else point
            for point in self.points)
        self.assertEqual(
            baseline_fit, fit_cushion_parameters(changed, self.split, self.inputs))
        changed_cases = {
            case.case_id: case.scenario_json for case in
            adapt_mathavan_2010(self.package, self.split, changed)
            if case.partition == "CALIBRATION"
        }
        self.assertEqual(baseline_cases, changed_cases)

    def test_report_preserves_objective_sensitivity_and_source_hypothesis(self):
        report = build_fit_report(self.points, self.split, self.inputs)
        self.assertEqual(report["fit_partition"], "CALIBRATION")
        self.assertEqual(report["physical_constants"], {
            "maximum_rigid_incident_speed_cm_s": 250.0,
            "nose_height_ratio": 1.4,
        })
        self.assertFalse(report["source_reported_sensitivity_center"]
                         ["used_as_experimental_expected_values"])
        self.assertEqual(json.loads(FIT_PATH.read_text(encoding="utf-8")), report)


if __name__ == "__main__":
    unittest.main()
