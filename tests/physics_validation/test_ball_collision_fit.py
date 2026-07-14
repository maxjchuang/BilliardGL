import json
import math
import unittest
from pathlib import Path

from tools.physics_validation.fit_ball_collision import (
    build_fit_report,
    fit_material_parameters,
    read_impact_inputs,
)
from tools.physics_validation.reference_package import load_reference_package
from tools.physics_validation.reference_point import read_reference_points
from tools.physics_validation.reference_split import load_reference_split


PACKAGE_PATH = Path(__file__).parent / "reference_data/domenech_2023_ball_collision"
FIT_PATH = Path(__file__).parents[2] / (
    "physics_models/calibration/ball_collision_material_fit_v1.json")


class BallCollisionFitTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.package = load_reference_package(PACKAGE_PATH)
        cls.points = read_reference_points(
            cls.package.files["normalized"], cls.package.manifest["dataset_id"])
        cls.split = load_reference_split(
            cls.package.files["split"], cls.points,
            cls.package.manifest["dataset_id"],
            cls.package.manifest["dataset_version"])
        cls.impacts = read_impact_inputs(cls.package.files["raw_extracted"])

    def test_fit_is_bounded_deterministic_and_calibration_only(self):
        first = fit_material_parameters(self.points, self.split, self.impacts)
        second = fit_material_parameters(
            tuple(reversed(self.points)), self.split, self.impacts)
        self.assertEqual(first, second)
        self.assertEqual(set(first), {"billiard", "brass", "rubber", "steel"})
        for material, fit in first.items():
            with self.subTest(material=material):
                self.assertGreaterEqual(fit["normal_restitution"], 0.0)
                self.assertLessEqual(fit["normal_restitution"], 1.0)
                self.assertGreaterEqual(fit["friction_coefficient"], 0.0)
                self.assertLessEqual(fit["friction_coefficient"], 1.0)
                self.assertTrue(math.isfinite(fit["objective_mean_squared_degrees"])
                                and fit["objective_mean_squared_degrees"] >= 0.0)
                self.assertTrue(fit["calibration_point_ids"])
                self.assertTrue(all(
                    self.split.partition_for(point) == "CALIBRATION"
                    for point in self.points
                    if point.point_id in fit["calibration_point_ids"]))

    def test_report_preserves_full_objective_sensitivity_and_exclusions(self):
        report = build_fit_report(self.points, self.split, self.impacts)
        self.assertEqual(report["schema_version"], 1)
        self.assertEqual(report["fit_partition"], "CALIBRATION")
        self.assertEqual(report["parameter_bounds"], {
            "friction_coefficient": [0.0, 1.0],
            "normal_restitution": [0.0, 1.0],
        })
        self.assertEqual(
            report["algorithm"]["surface_sliding_friction_hypothesis"], 0.002)
        self.assertEqual(len(report["materials"]), 4)
        calibration_ids = {
            point.point_id for point in self.points
            if self.split.partition_for(point) == "CALIBRATION"
        }
        holdout_ids = {
            point.point_id for point in self.points
            if self.split.partition_for(point) == "HOLDOUT"
        }
        reported_calibration = {
            point_id for fit in report["materials"].values()
            for point_id in fit["calibration_point_ids"]
        }
        reported_excluded = {
            point_id for fit in report["materials"].values()
            for point_id in fit["excluded_holdout_point_ids"]
        }
        self.assertEqual(reported_calibration, calibration_ids)
        self.assertEqual(reported_excluded, holdout_ids)
        self.assertTrue(all(
            fit["sensitivity"] for fit in report["materials"].values()))
        self.assertEqual(
            json.loads(FIT_PATH.read_text(encoding="utf-8")), report,
            "the committed full-precision fit artifact must match the adapter inputs")


if __name__ == "__main__":
    unittest.main()
