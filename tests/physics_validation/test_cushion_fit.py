import hashlib
import json
import math
import unittest
from dataclasses import replace
from pathlib import Path

from tools.physics_validation.adapters.mathavan_2010 import adapt_mathavan_2010
from tools.physics_validation.fit_cushion import (
    build_fit_report, build_v2_fit_report, fit_cushion_parameters,
    read_incident_inputs)
from tools.physics_validation.reference_package import load_reference_package
from tools.physics_validation.reference_point import read_reference_points
from tools.physics_validation.reference_split import load_reference_split


PACKAGE_PATH = Path(__file__).parent / "reference_data/mathavan_2010_cushion"
FIT_PATH = Path(__file__).parents[2] / "physics_models/calibration/cushion_fit_v1.json"
V2_INPUTS = Path(__file__).parents[2] / (
    "physics_models/calibration/cushion_fit_v2_inputs.csv")
V2_FIT = Path(__file__).parents[2] / "physics_models/calibration/cushion_fit_v2.json"


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

    def test_fitted_cushion_law_is_finite_bounded_and_nonincreasing(self):
        fit = fit_cushion_parameters(read_incident_inputs(V2_INPUTS))

        self.assertGreaterEqual(fit.e_slope, 0.0)
        self.assertTrue(0.0 <= fit.e_min <= fit.e_max <= 1.0)
        values = [fit.restitution(speed) for speed in (0.0, 0.5, 1.8, 4.0)]
        self.assertEqual(values, sorted(values, reverse=True))
        self.assertTrue(math.isfinite(fit.objective))
        self.assertEqual(
            (fit.e_intercept, fit.e_slope, fit.e_min, fit.e_max),
            (1.0, 0.056, 0.0, 0.93),
        )
        self.assertEqual(len(fit.residuals), len(read_incident_inputs(V2_INPUTS)))
        self.assertEqual(
            json.loads(V2_FIT.read_text(encoding="utf-8")),
            build_v2_fit_report(read_incident_inputs(V2_INPUTS))[0],
        )

    def test_v2_inputs_are_spent_and_exclude_confirmation(self):
        rows = read_incident_inputs(V2_INPUTS)
        dataset_ids = {row.dataset_id for row in rows}

        self.assertEqual(
            dataset_ids,
            {"mathavan_2009_high_speed", "mathavan_2010_cushion"},
        )
        self.assertEqual({row.lifecycle for row in rows}, {"spent"})
        self.assertNotIn("sudo_2002", dataset_ids)

    def test_v2_inputs_are_bound_to_committed_numeric_sources(self):
        root = Path(__file__).parents[2]
        for row in read_incident_inputs(V2_INPUTS):
            for path_value, expected in (
                    (row.normalized_path, row.normalized_sha256),
                    (row.raw_extracted_path, row.raw_extracted_sha256)):
                actual = "sha256:" + hashlib.sha256(
                    (root / path_value).read_bytes()).hexdigest()
                self.assertEqual(actual, expected)


if __name__ == "__main__":
    unittest.main()
