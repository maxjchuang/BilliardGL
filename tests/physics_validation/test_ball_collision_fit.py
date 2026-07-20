import hashlib
import json
import math
import unittest
from pathlib import Path

from tools.physics_validation.fit_ball_collision import (
    Fit,
    ResidualRow,
    build_fit_report,
    build_v2_fit_report,
    fit_ball_collision_parameters,
    fit_material_parameters,
    read_impact_inputs,
    select_fit,
    series_balanced_objective,
)
from tools.physics_validation.reference_package import load_reference_package
from tools.physics_validation.reference_point import read_reference_points
from tools.physics_validation.reference_split import load_reference_split


PACKAGE_PATH = Path(__file__).parent / "reference_data/domenech_2023_ball_collision"
FIT_PATH = Path(__file__).parents[2] / (
    "physics_models/calibration/ball_collision_material_fit_v1.json")
V2_INPUTS = Path(__file__).parents[2] / (
    "physics_models/calibration/ball_collision_fit_v2_inputs.csv")
V2_FIT = Path(__file__).parents[2] / (
    "physics_models/calibration/ball_collision_fit_v2.json")


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

    def test_dense_series_does_not_gain_point_count_weight(self):
        sparse = [ResidualRow("sparse", 1.0), ResidualRow("sparse", 1.0)]
        dense = [ResidualRow("dense", 3.0) for _ in range(100)]

        objective, by_series = series_balanced_objective(sparse + dense)

        self.assertEqual(objective, 5.0)
        self.assertEqual(by_series, {"dense": 9.0, "sparse": 1.0})

    def test_tie_break_is_objective_then_e_then_mu(self):
        fits = [
            Fit(1.0, 0.90, 0.05),
            Fit(1.0, 0.89, 0.06),
            Fit(1.0, 0.89, 0.04),
        ]

        self.assertEqual(select_fit(fits), Fit(1.0, 0.89, 0.04))

    def test_v2_fit_inputs_are_spent_pool_ball_data_without_confirmation(self):
        rows = read_impact_inputs(V2_INPUTS)
        dataset_ids = {row.dataset_id for row in rows}

        self.assertEqual(
            dataset_ids,
            {"domenech_2023_ball_collision", "mathavan_2009_high_speed"},
        )
        self.assertEqual({row.lifecycle for row in rows}, {"spent"})
        self.assertFalse(dataset_ids & {"sudo_2002", "derby_fuller_1999"})
        self.assertEqual(
            {row.series_id for row in rows},
            {"billiard_alpha1", "billiard_delta2", "mathavan_velocity"},
        )
        self.assertEqual(len(rows), 109)
        self.assertEqual(
            {row.metric for row in rows if row.series_id == "billiard_delta2"},
            {"object_normal_deflection_angle_degrees"},
        )

    def test_v2_inputs_are_bound_to_the_committed_numeric_sources(self):
        root = Path(__file__).parents[2]
        for row in read_impact_inputs(V2_INPUTS):
            for path_value, expected in (
                    (row.normalized_path, row.normalized_sha256),
                    (row.raw_extracted_path, row.raw_extracted_sha256)):
                path = root / path_value
                actual = "sha256:" + hashlib.sha256(path.read_bytes()).hexdigest()
                self.assertEqual(actual, expected)

    def test_v2_fit_is_deterministic_series_balanced_and_energy_passive(self):
        points = read_impact_inputs(V2_INPUTS)
        first = fit_ball_collision_parameters(points)
        second = fit_ball_collision_parameters(tuple(reversed(points)))

        self.assertEqual(first, second)
        self.assertEqual(first["normal_restitution"], 0.97)
        self.assertEqual(first["friction_coefficient"], 0.1)
        self.assertEqual(len(first["residuals"]), len(points))
        self.assertTrue(all(
            row["kinetic_energy_after_j"] <= row["kinetic_energy_before_j"]
            for row in first["residuals"]
        ))
        self.assertEqual(
            json.loads(V2_FIT.read_text(encoding="utf-8")),
            build_v2_fit_report(points)[0],
        )


if __name__ == "__main__":
    unittest.main()
