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

    def test_table_mapping_preserves_source_geometry_and_point_ball_identity(self):
        cases = default_reference_registry().adapt(
            self.package, self.split, self.points)
        case = next(case for case in cases if case.case_id == "table1_shot_01")
        scenario = json.loads(case.scenario_json)

        self.assertEqual(scenario["evidence"]["source_ball_diameter_cm"], 5.24)
        self.assertAlmostEqual(
            scenario["balls"][1]["position_cm"][2],
            5.24 * math.sin(math.radians(33.83)),
        )
        self.assertEqual(scenario["balls"][0]["velocity_cm_s"], [153.9, 0.0, 0.0])
        expectations = {
            item["value"]["point_id"]: item["value"]
            for item in scenario["expectations"]
        }
        self.assertEqual(expectations["table1_shot_01_cue_speed"]["ball_index"], 0)
        self.assertEqual(expectations["table1_shot_01_object_speed"]["ball_index"], 1)
        self.assertEqual(
            expectations["table1_shot_01_cue_speed"]["selection"]["event_kind"],
            "ball_ball",
        )

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
