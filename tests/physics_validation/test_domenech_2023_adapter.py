import json
import math
import tempfile
import unittest
from dataclasses import replace
from pathlib import Path

from tools.physics_validation.adapters.domenech_2023 import adapt_domenech_2023
from tools.physics_validation.fit_ball_collision import (
    fit_material_parameters,
    read_impact_inputs,
)
from tools.physics_validation.reference_adapter import default_reference_registry
from tools.physics_validation.reference_package import (
    ReferencePackageError,
    load_reference_package,
)
from tools.physics_validation.reference_point import read_reference_points
from tools.physics_validation.reference_split import load_reference_split


PACKAGE_PATH = Path(__file__).parent / "reference_data/domenech_2023_ball_collision"


class DomenechAdapterTests(unittest.TestCase):
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

    def test_all_214_material_points_are_executable_with_only_source_audit_limitations(self):
        first = adapt_domenech_2023(self.package, self.split, self.points)
        second = adapt_domenech_2023(
            self.package, self.split, tuple(reversed(self.points)))

        self.assertEqual(first, second)
        self.assertEqual(len(first.cases), 214)
        self.assertEqual(
            {item.case_id for item in first.limitations},
            {"author_data_request_pending", "version_record_pdf_audit_pending"},
        )
        self.assertTrue(all(not item.point_ids for item in first.limitations))
        self.assertEqual(
            {point.point_id for case in first.cases for point in case.points},
            {point.point_id for point in self.points},
        )
        self.assertEqual(
            sum(case.partition == "CALIBRATION" for case in first.cases), 75)
        self.assertEqual(
            sum(case.partition == "HOLDOUT" for case in first.cases), 139)

    def test_v5_scenarios_install_exact_source_geometry_material_and_impact_angle(self):
        adaptation = adapt_domenech_2023(self.package, self.split, self.points)
        cases = {case.case_id: case for case in adaptation.cases}
        for case_id in (
                "billiard_alpha1_001", "brass_alpha1_001",
                "rubber_delta2_001", "steel_alpha1_001"):
            scenario = json.loads(cases[case_id].scenario_json)
            point = cases[case_id].points[0]
            material = point.series_id.split("_")[0]
            source = self.package.manifest["apparatus"]["materials"][material]
            ball = scenario["physics_profile"]["ball"]
            self.assertEqual(scenario["schema_version"], 5)
            self.assertEqual(scenario["evidence"]["equipment"],
                             "SOURCE_LABORATORY_APPARATUS")
            self.assertEqual(ball["radius_cm"], source["diameter_cm"] / 2.0)
            self.assertEqual(ball["mass_kg"], source["mass_g"] / 1000.0)
            self.assertEqual(ball["material"], material)
            self.assertIn("inertia_factor", ball)
            self.assertIn("normal_restitution", ball)
            self.assertIn("friction_coefficient", ball)
            cue, object_ball = scenario["balls"]
            self.assertEqual(cue["velocity_cm_s"], [80.0, 0.0, 0.0])
            self.assertAlmostEqual(
                cue["angular_velocity_rad_s"][2], -80.0 / ball["radius_cm"])
            distance = math.dist(cue["position_cm"], object_ball["position_cm"])
            self.assertAlmostEqual(distance, source["diameter_cm"] - 1e-6, places=7)
            normal = [
                (object_ball["position_cm"][axis] - cue["position_cm"][axis]) /
                distance for axis in range(3)
            ]
            impact = math.degrees(math.acos(normal[0]))
            self.assertAlmostEqual(impact, self.impacts[point.point_id]["impact_angle_degrees"])
            self.assertEqual(
                scenario["expectations"][0]["value"]["selection"]
                ["solver_event_scope"],
                "single",
            )
        post = json.loads(cases["rubber_lambda2_001"].scenario_json)
        self.assertEqual(post["simulation"]["ticks"], 400)
        self.assertEqual(
            post["physics_profile"]["surface"]["sliding_friction_coefficient"],
            0.002)

    def test_holdout_expected_mutation_cannot_change_fit_or_calibration_scenarios(self):
        mutated = tuple(
            replace(point, expected=point.expected + 1000.0)
            if self.split.partition_for(point) == "HOLDOUT" else point
            for point in self.points
        )
        original_fit = fit_material_parameters(self.points, self.split, self.impacts)
        mutated_fit = fit_material_parameters(mutated, self.split, self.impacts)
        self.assertEqual(original_fit, mutated_fit)

        original = adapt_domenech_2023(self.package, self.split, self.points)
        changed = adapt_domenech_2023(self.package, self.split, mutated)
        original_calibration = {
            case.case_id: case.scenario_json for case in original.cases
            if case.partition == "CALIBRATION"
        }
        changed_calibration = {
            case.case_id: case.scenario_json for case in changed.cases
            if case.partition == "CALIBRATION"
        }
        self.assertEqual(original_calibration, changed_calibration)

    def test_registry_exposes_domenech_adapter(self):
        registry = default_reference_registry()
        self.assertEqual(
            registry.adapt_with_limitations(
                self.package, self.split, self.points),
            adapt_domenech_2023(self.package, self.split, self.points),
        )

    def test_mixed_material_case_is_rejected(self):
        billiard = next(
            point for point in self.points if point.series_id == "billiard_alpha1")
        brass = next(
            point for point in self.points if point.series_id == "brass_alpha1")
        mixed = (billiard, replace(brass, case_id=billiard.case_id))

        with self.assertRaisesRegex(ReferencePackageError, "MIXED_MATERIAL_CASE"):
            adapt_domenech_2023(self.package, self.split, mixed)


if __name__ == "__main__":
    unittest.main()
