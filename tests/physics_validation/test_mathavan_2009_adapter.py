import json
import math
import unittest
from pathlib import Path

from tools.physics_validation.reference_adapter import default_reference_registry
from tools.physics_validation.reference_package import load_reference_package
from tools.physics_validation.reference_point import read_reference_points
from tools.physics_validation.reference_split import load_reference_split


PACKAGE_ROOT = (
    Path(__file__).parent
    / "reference_data/mathavan_2009_high_speed"
)


class Mathavan2009AdapterTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.package = load_reference_package(PACKAGE_ROOT)
        profile_path = (
            Path(__file__).parents[2]
            / "physics_models/profiles/chinese_pool_surface_motion_v1.json"
        )
        cls.candidate_profile = json.loads(
            profile_path.read_text(encoding="utf-8"))["runtime_profile"]
        cls.points = read_reference_points(
            cls.package.files["normalized"], cls.package.manifest["dataset_id"])
        cls.split = load_reference_split(
            cls.package.files["split"], cls.points,
            cls.package.manifest["dataset_id"],
            cls.package.manifest["dataset_version"],
        )

    def test_adapter_is_byte_deterministic_and_independent_of_point_order(self):
        from tools.physics_validation.adapters.mathavan_2009 import adapt_mathavan_2009

        first = adapt_mathavan_2009(self.package, self.split, self.points)
        second = adapt_mathavan_2009(
            self.package, self.split, tuple(reversed(self.points)))

        self.assertEqual(first, second)
        self.assertEqual(len(first), 15)
        self.assertEqual(sum(len(case.points) for case in first), 20)
        self.assertEqual(
            [case.case_id for case in first],
            sorted(case.case_id for case in first),
        )

    def test_table_points_execute_at_the_first_pure_roll_window(self):
        adaptation = default_reference_registry().adapt_with_limitations(
            self.package, self.split, self.points)
        table_points = {
            point.point_id for point in self.points
            if point.series_id == "oblique_ball_collision"
        }
        self.assertEqual(len(table_points), 10)
        table_cases = [
            case for case in adaptation.cases
            if case.case_id.startswith("table1_shot_")
        ]
        self.assertEqual(len(table_cases), 5)
        self.assertEqual(
            {point.point_id for case in table_cases for point in case.points},
            table_points,
        )
        for case in table_cases:
            scenario = json.loads(case.scenario_json)
            self.assertEqual(scenario["schema_version"], 3)
            self.assertEqual(
                scenario["physics_profile"], self.candidate_profile)
            self.assertEqual(
                scenario["physics_profile"]["id"],
                "chinese_pool_surface_motion_v1",
            )
            self.assertEqual(
                scenario["physics_profile"]["surface"]
                ["sliding_friction_coefficient"],
                0.20,
            )
            self.assertEqual(
                scenario["physics_profile"]["surface"]
                ["rolling_resistance_acceleration_cm_s2"],
                12.5,
            )
            self.assertTrue(all(
                item["value"]["selection"]["sample_phase"] ==
                "first_pure_roll_after_event"
                for item in scenario["expectations"]
            ))
        claimed = {
            point_id
            for limitation in adaptation.limitations
            for point_id in limitation.point_ids
        }
        self.assertTrue(table_points.isdisjoint(claimed))

    def test_cushion_and_deceleration_cases_encode_explicit_selection(self):
        cases = default_reference_registry().adapt(
            self.package, self.split, self.points)
        by_id = {case.case_id: json.loads(case.scenario_json) for case in cases}

        cushion = by_id["fig9_visible_01"]
        self.assertEqual(cushion["balls"][0]["velocity_cm_s"], [27.2697, 0.0, 0.0])
        self.assertEqual(
            cushion["expectations"][0]["value"]["selection"]["sample_phase"],
            "first_sample_after_event",
        )
        rolling = by_id["rolling_deceleration"]
        self.assertAlmostEqual(
            rolling["balls"][0]["angular_velocity_rad_s"][2],
            -100.0 / 2.8575,
        )
        self.assertEqual(
            rolling["expectations"][0]["value"]["selection"],
            {
                "ball_index": 0,
                "first_tick": 1,
                "last_tick": 5,
                "minimum_window_ticks": 3,
                "sample_phase": "declared_tick_window",
            },
        )

    def test_registry_exposes_non_executable_source_limitations(self):
        adaptation = default_reference_registry().adapt_with_limitations(
            self.package, self.split, self.points)

        self.assertEqual(len(adaptation.cases), 15)
        self.assertEqual(
            {limitation.case_id for limitation in adaptation.limitations},
            {
                "fig9_unresolved_markers",
                "snooker_to_pool_material_conversion_missing",
                "unmeasured_initial_spin",
            },
        )
        self.assertTrue(all(limitation.resolution_condition for limitation in adaptation.limitations))


if __name__ == "__main__":
    unittest.main()
