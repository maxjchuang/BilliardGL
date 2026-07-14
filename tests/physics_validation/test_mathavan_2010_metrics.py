import unittest

from tools.physics_validation.analyzer import analyze_scenario


def _frame(tick, speed, contact=False):
    rail_contact = {
        "contact_arm_cm": [2.625, 1.05, 0.0],
        "contact_height_cm": 3.675,
        "contact_tangent": [0.0, 0.0, -1.0],
        "contact_velocity_before_cm_s": [105.0, 0.0, 0.0],
        "contact_velocity_after_cm_s": [-80.0, 0.0, 0.0],
        "first_ball": 0,
        "friction_coefficient": 0.14,
        "impulse_on_ball_ns": [-0.3, 0.0, 0.0],
        "incident_speed_cm_s": 105.0,
        "kinetic_energy_after_j": 0.7,
        "kinetic_energy_before_j": 1.0,
        "kind": "rail",
        "maximum_rigid_incident_speed_cm_s": 250.0,
        "normal": [-1.0, 0.0, 0.0],
        "normal_impulse_ns": 0.3,
        "normal_relative_speed_after_cm_s": 80.0,
        "normal_relative_speed_before_cm_s": -105.0,
        "nose_height_ratio": 1.4,
        "position_corrected": False,
        "position_correction_cm": [0.0, 0.0, 0.0],
        "regime": "frictionless",
        "restitution": 0.8,
        "rigid_domain_exceeded": False,
        "second_ball": -1,
        "tangential_impulse_ns": 0.0,
        "time_of_impact_seconds": 0.1,
        "velocity_impulse_applied": True,
    }
    return {
        "balls": [{
            "angular_velocity_rad_s": [0.0, 0.0, -speed / 2.625],
            "index": 0,
            "position_cm": [0.0, 0.0, 0.0],
            "speed_cm_s": speed,
            "velocity_cm_s": [speed, 0.0, 0.0],
        }],
        "contacts": ([rail_contact] if contact else []),
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

    def test_cushion_diagnostics_classify_physics_and_integration_failures(self):
        selection = {
            "ball_index": 0, "ball_radius_cm": 2.625,
            "event_kind": "rail_collision", "incident_speed_cm_s": 105.0,
            "incident_speed_tolerance_cm_s": 0.001, "incident_window_ticks": 3,
            "minimum_window_ticks": 3, "rebound_window_ticks": 3,
            "sample_phase": "immediate_post_impact", "sidespin_tolerance_rad_s": 0.001,
            "pure_roll_tolerance_cm_s": 0.001,
        }
        base = [_frame(1, 108.0), _frame(2, 107.0), _frame(3, 106.0),
                _frame(4, 80.0, True), _frame(5, 79.0), _frame(6, 78.0)]
        cases = [
            ("contact_velocity_before_cm_s", [float("nan"), 0.0, 0.0], "NUMERICAL_FAILURE"),
            ("contact_velocity_before_cm_s", [-105.0, 0.0, 0.0], "INTEGRATION_MISMATCH"),
            ("tangential_impulse_ns", 1.0, "INTEGRATION_MISMATCH"),
            ("kinetic_energy_after_j", 1.1, "NUMERICAL_FAILURE"),
            ("rigid_domain_exceeded", True, "INTEGRATION_MISMATCH"),
        ]
        for field, value, code in cases:
            frames = [_frame(i, speed, i == 4) for i, speed in
                      [(1, 108.0), (2, 107.0), (3, 106.0), (4, 80.0), (5, 79.0), (6, 78.0)]]
            frames[3]["contacts"][0][field] = value
            result = analyze_scenario(_scenario(selection), frames)
            self.assertEqual(result.failures[0].code, code, field)

    def test_incomplete_cushion_diagnostics_are_integration_mismatch(self):
        selection = {
            "ball_index": 0, "ball_radius_cm": 2.625, "event_kind": "rail_collision",
            "incident_speed_cm_s": 105.0, "incident_speed_tolerance_cm_s": 0.001,
            "incident_window_ticks": 3, "minimum_window_ticks": 3,
            "rebound_window_ticks": 3, "sample_phase": "immediate_post_impact",
            "sidespin_tolerance_rad_s": 0.001, "pure_roll_tolerance_cm_s": 0.001,
        }
        frames = [_frame(1, 108.0), _frame(2, 107.0), _frame(3, 106.0),
                  _frame(4, 80.0, True), _frame(5, 79.0), _frame(6, 78.0)]
        del frames[3]["contacts"][0]["contact_arm_cm"]
        result = analyze_scenario(_scenario(selection), frames)
        self.assertEqual(result.failures[0].code, "INTEGRATION_MISMATCH")


if __name__ == "__main__":
    unittest.main()
