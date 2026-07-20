import unittest

from tools.physics_validation.analyzer import analyze_scenario


def _frame(tick, speed=12.0, spin=3.0, traced=True):
    frame = {
        "tick": tick,
        "time_seconds": tick * 0.1,
        "balls": [{
            "index": 0,
            "speed_cm_s": speed,
            "velocity_cm_s": [speed, 0.0, 0.0],
            "angular_velocity_rad_s": [0.0, 0.0, spin],
        }],
        "contacts": [],
    }
    if traced:
        frame["cue_impact"] = {"cue_speed_cm_s": 100.0}
    return frame


def _contact_frame(regime="stick", normal=0.1, tangent=0.02,
                   friction=0.6, input_energy=1.0, output_energy=0.8,
                   applied=True, error=""):
    frame = _frame(1)
    frame["cue_contact"] = {
        "regime": regime, "applied": applied, "error_code": error,
        "friction_coefficient": friction,
        "normal_impulse_ns": normal,
        "tangential_impulse_ns": tangent,
        "input_kinetic_energy_j": input_energy,
        "output_kinetic_energy_j": output_energy,
    }
    return frame


def _scenario(metric, expected, selection):
    return {
        "id": "cross_metric",
        "evidence": {"grade": "B"},
        "expectations": [{
            "metric": "value_within_interval",
            "value": {
                "point_id": "p1", "observed_metric": metric,
                "ball_index": 0, "expected": expected,
                "lower": expected, "upper": expected, "unit": "cm/s",
                "selection": selection,
            },
        }],
    }


class Cross2023MetricTests(unittest.TestCase):
    def setUp(self):
        self.selection = {
            "sample_phase": "first_stable_frames_after_cue_impact",
            "source_phase": "first_stable_post_impact_frames",
            "ball_index": 0,
            "minimum_window_ticks": 2,
            "input_support": True,
            "stability_tolerance": 0.001,
            "angular_axis": "z",
            "angular_sign": -1,
        }

    def test_linear_and_signed_angular_outputs_use_first_stable_window(self):
        frames = [_frame(1), _frame(2), _frame(3)]
        linear = analyze_scenario(
            _scenario("cue_impact_linear_speed_cm_s", 12.0, self.selection), frames)
        angular = analyze_scenario(
            _scenario("cue_impact_angular_speed_rad_s", -3.0, self.selection), frames)
        self.assertTrue(linear.passed)
        self.assertTrue(angular.passed)

    def test_absent_input_trace_and_unsupported_input_are_distinct(self):
        absent = analyze_scenario(
            _scenario("cue_impact_linear_speed_cm_s", 12.0, self.selection),
            [_frame(1, traced=False), _frame(2, traced=False)],
        )
        unsupported = dict(self.selection, input_support=False)
        limited = analyze_scenario(
            _scenario("cue_impact_linear_speed_cm_s", 12.0, unsupported),
            [_frame(1), _frame(2)],
        )
        self.assertEqual(absent.failures[0].code, "INTEGRATION_MISMATCH")
        self.assertEqual(limited.failures[0].code, "REFERENCE_LIMITATION")

    def test_finite_zero_spin_is_a_model_mismatch(self):
        result = analyze_scenario(
            _scenario("cue_impact_angular_speed_rad_s", -3.0, self.selection),
            [_frame(1, spin=0.0), _frame(2, spin=0.0)],
        )
        self.assertEqual(result.failures[0].code, "MODEL_MISMATCH")

    def test_contact_metrics_cross_check_regime_and_friction_cone(self):
        selection = dict(self.selection, expected_regime="stick")
        scenario = _scenario("cue_contact_normal_impulse_ns", 0.1, selection)
        self.assertTrue(analyze_scenario(scenario, [_contact_frame()]).passed)
        invalid = analyze_scenario(
            scenario, [_contact_frame(regime="stick", tangent=0.07)])
        self.assertEqual(invalid.failures[0].code, "INTEGRATION_MISMATCH")

    def test_contact_nonfinite_energy_is_numerical_failure(self):
        selection = dict(self.selection, expected_regime="stick")
        scenario = _scenario("cue_contact_energy_efficiency", 0.8, selection)
        result = analyze_scenario(
            scenario, [_contact_frame(output_energy=float("nan"))])
        self.assertEqual(result.failures[0].code, "NUMERICAL_FAILURE")

    def test_unsupported_3d_contact_is_reference_limitation(self):
        selection = dict(self.selection, expected_regime="unsupported")
        scenario = _scenario("cue_contact_normal_impulse_ns", 0.0, selection)
        result = analyze_scenario(scenario, [_contact_frame(
            regime="unsupported", normal=0.0, tangent=0.0, applied=False,
            error="vertical_ball_impulse_requires_3d")])
        self.assertEqual(result.failures[0].code, "REFERENCE_LIMITATION")


if __name__ == "__main__":
    unittest.main()
