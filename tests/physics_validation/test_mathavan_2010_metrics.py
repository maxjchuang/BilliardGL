import unittest

from tools.physics_validation.analyzer import analyze_scenario


def _frame(tick, speed, contact=False):
    return {
        "balls": [{
            "angular_velocity_rad_s": [0.0, 0.0, -speed / 2.625],
            "index": 0,
            "position_cm": [0.0, 0.0, 0.0],
            "speed_cm_s": speed,
            "velocity_cm_s": [speed, 0.0, 0.0],
        }],
        "contacts": ([{"first_ball": 0, "kind": "rail", "normal": [-1.0, 0.0, 0.0], "second_ball": -1}] if contact else []),
        "maximum_penetration_cm": 0.0,
        "tick": tick,
        "time_seconds": tick * 0.1,
        "translational_kinetic_energy_j": 1.0,
    }


def _scenario(selection):
    return {
        "balls": [{"angular_velocity_rad_s": [0.0, 0.0, -40.0], "index": 0, "velocity_cm_s": [105.0, 0.0, 0.0]}],
        "evidence": {"equipment": "SOURCE_SNOOKER", "grade": "B", "source": "experiment"},
        "expectations": [{
            "metric": "value_within_interval",
            "value": {
                "ball_index": 0,
                "expected": 80.0,
                "lower": 79.999,
                "observed_metric": "cushion_rebound_speed_cm_s",
                "point_id": "fig7",
                "selection": selection,
                "unit": "cm/s",
                "upper": 80.001,
            },
        }],
        "id": "paired_cushion",
    }


class Mathavan2010MetricTests(unittest.TestCase):
    def test_paired_windows_fit_incident_and_immediate_rebound_at_event_time(self):
        selection = {
            "ball_index": 0,
            "ball_radius_cm": 2.625,
            "event_kind": "rail_collision",
            "incident_speed_cm_s": 105.0,
            "incident_speed_tolerance_cm_s": 0.001,
            "incident_window_ticks": 3,
            "minimum_window_ticks": 3,
            "rebound_window_ticks": 3,
            "sample_phase": "immediate_post_impact",
            "sidespin_tolerance_rad_s": 0.001,
            "pure_roll_tolerance_cm_s": 0.001,
        }
        frames = [_frame(1, 108.0), _frame(2, 107.0), _frame(3, 106.0),
                  _frame(4, 80.0, True), _frame(5, 79.0), _frame(6, 78.0)]

        result = analyze_scenario(_scenario(selection), frames)

        self.assertTrue(result.passed)
        self.assertAlmostEqual(result.metrics["cushion_rebound_speed_cm_s"], 80.0)

    def test_second_rail_event_in_fit_window_is_integration_mismatch(self):
        selection = {
            "ball_index": 0, "ball_radius_cm": 2.625, "event_kind": "rail_collision", "incident_speed_cm_s": 105.0,
            "incident_speed_tolerance_cm_s": 0.001, "incident_window_ticks": 3,
            "minimum_window_ticks": 3, "rebound_window_ticks": 3, "sample_phase": "immediate_post_impact",
            "sidespin_tolerance_rad_s": 0.001, "pure_roll_tolerance_cm_s": 0.001,
        }
        frames = [_frame(1, 108.0), _frame(2, 107.0), _frame(3, 106.0),
                  _frame(4, 80.0, True), _frame(5, 79.0, True), _frame(6, 78.0)]

        result = analyze_scenario(_scenario(selection), frames)

        self.assertEqual(result.failures[0].code, "INTEGRATION_MISMATCH")

    def test_missing_phase_is_limitation_and_nonfinite_fit_is_numerical_failure(self):
        selection = {
            "ball_index": 0, "ball_radius_cm": 2.625, "event_kind": "rail_collision", "incident_speed_cm_s": 105.0,
            "incident_speed_tolerance_cm_s": 0.001, "incident_window_ticks": 3,
            "minimum_window_ticks": 3, "rebound_window_ticks": 3,
            "sidespin_tolerance_rad_s": 0.001, "pure_roll_tolerance_cm_s": 0.001,
        }
        frames = [_frame(1, 108.0), _frame(2, 107.0), _frame(3, 106.0),
                  _frame(4, 80.0, True), _frame(5, 79.0), _frame(6, 78.0)]
        limitation = analyze_scenario(_scenario(selection), frames)
        self.assertEqual(limitation.failures[0].code, "REFERENCE_LIMITATION")
        selection["sample_phase"] = "immediate_post_impact"
        frames[5]["balls"][0]["speed_cm_s"] = float("nan")
        numerical = analyze_scenario(_scenario(selection), frames)
        self.assertEqual(numerical.failures[0].code, "NUMERICAL_FAILURE")


if __name__ == "__main__":
    unittest.main()
