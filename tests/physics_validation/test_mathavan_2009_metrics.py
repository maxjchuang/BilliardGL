import math
import unittest

from tools.physics_validation.analyzer import analyze_scenario


def _ball(index, speed, velocity=None, angular=None):
    velocity = velocity or [speed, 0.0, 0.0]
    angular = angular or [0.0, 0.0, -speed / 2.0]
    return {
        "index": index,
        "position_cm": [0.0, 0.0, 0.0],
        "velocity_cm_s": velocity,
        "acceleration_cm_s2": [0.0, 0.0, 0.0],
        "angular_velocity_rad_s": angular,
        "speed_cm_s": speed,
        "pocketed": False,
    }


def _frame(tick, time_seconds, balls, contacts=()):
    return {
        "tick": tick,
        "time_seconds": time_seconds,
        "balls": list(balls),
        "contacts": list(contacts),
    }


def _scenario(metric, selection, expected, lower=None, upper=None):
    return {
        "id": "metric_case",
        "evidence": {"grade": "B", "source": "experiment"},
        "expectations": [{
            "metric": "value_within_interval",
            "value": {
                "point_id": "point_01",
                "observed_metric": metric,
                "ball_index": selection.get("ball_index", 0),
                "expected": expected,
                "lower": expected if lower is None else lower,
                "upper": expected if upper is None else upper,
                "unit": "cm/s",
                "selection": selection,
            },
        }],
    }


class Mathavan2009MetricTests(unittest.TestCase):
    def test_rolling_and_sliding_deceleration_fit_speed_over_declared_window(self):
        frames = [
            _frame(1, 0.0, [_ball(0, 10.0)]),
            _frame(2, 1.0, [_ball(0, 8.0)]),
            _frame(3, 2.0, [_ball(0, 6.0)]),
        ]
        selection = {
            "sample_phase": "declared_tick_window",
            "ball_index": 0,
            "first_tick": 1,
            "last_tick": 3,
            "minimum_window_ticks": 3,
        }
        for metric in (
                "rolling_deceleration_cm_s2", "sliding_deceleration_cm_s2"):
            with self.subTest(metric=metric):
                result = analyze_scenario(_scenario(metric, selection, 2.0), frames)
                self.assertTrue(result.passed)
                self.assertAlmostEqual(result.metrics[metric], 2.0)

    def test_post_collision_speed_selects_first_declared_pure_roll_sample(self):
        contact = {"kind": "ball_ball", "first_ball": 0, "second_ball": 1}
        frames = [
            _frame(1, 0.0, [_ball(0, 6.0, angular=[0.0, 0.0, 0.0]), _ball(1, 1.0)], [contact]),
            _frame(2, 0.1, [_ball(0, 5.0), _ball(1, 1.0)]),
            _frame(3, 0.2, [_ball(0, 5.0), _ball(1, 1.0)]),
            _frame(4, 0.3, [_ball(0, 5.0), _ball(1, 1.0)]),
        ]
        selection = {
            "event_kind": "ball_ball",
            "sample_phase": "first_pure_roll_after_event",
            "ball_index": 0,
            "minimum_window_ticks": 3,
            "ball_radius_cm": 2.0,
            "pure_roll_tolerance_cm_s": 0.001,
        }

        result = analyze_scenario(
            _scenario("post_collision_linear_velocity_cm_s", selection, 5.0),
            frames,
        )

        self.assertTrue(result.passed)
        self.assertEqual(result.metrics["post_collision_linear_velocity_cm_s"], 5.0)

    def test_separation_angle_uses_two_post_collision_velocity_vectors(self):
        contact = {"kind": "ball_ball", "first_ball": 0, "second_ball": 1}
        rolling_x = _ball(0, 4.0, [4.0, 0.0, 0.0], [0.0, 0.0, -2.0])
        rolling_z = _ball(1, 3.0, [0.0, 0.0, 3.0], [1.5, 0.0, 0.0])
        frames = [
            _frame(1, 0.0, [rolling_x, rolling_z], [contact]),
            _frame(2, 0.1, [rolling_x, rolling_z]),
            _frame(3, 0.2, [rolling_x, rolling_z]),
        ]
        selection = {
            "event_kind": "ball_ball",
            "sample_phase": "first_pure_roll_after_event",
            "ball_index": 0,
            "other_ball_index": 1,
            "minimum_window_ticks": 3,
            "ball_radius_cm": 2.0,
            "pure_roll_tolerance_cm_s": 0.001,
        }

        result = analyze_scenario(
            _scenario("separation_angle_degrees", selection, 90.0), frames)

        self.assertTrue(result.passed)
        self.assertAlmostEqual(result.metrics["separation_angle_degrees"], 90.0)

    def test_cushion_rebound_speed_and_signed_tangent_angle_use_rail_event(self):
        rail = {
            "kind": "rail",
            "first_ball": 0,
            "second_ball": -1,
            "normal": [-1.0, 0.0, 0.0],
        }
        frames = [
            _frame(1, 0.0, [_ball(0, 5.0, [-3.0, 0.0, 4.0])], [rail]),
        ]
        selection = {
            "event_kind": "rail_collision",
            "sample_phase": "first_sample_after_event",
            "ball_index": 0,
            "minimum_window_ticks": 1,
        }
        expectations = (
            ("cushion_rebound_speed_cm_s", 5.0),
            ("cushion_rebound_angle_degrees", math.degrees(math.atan2(3.0, 4.0))),
        )
        for metric, expected in expectations:
            with self.subTest(metric=metric):
                result = analyze_scenario(_scenario(metric, selection, expected), frames)
                self.assertTrue(result.passed)
                self.assertAlmostEqual(result.metrics[metric], expected)

    def test_selection_and_trace_failures_have_distinct_codes(self):
        valid_selection = {
            "sample_phase": "declared_tick_window",
            "ball_index": 0,
            "first_tick": 1,
            "last_tick": 3,
            "minimum_window_ticks": 3,
        }
        cases = (
            ({"ball_index": 0}, [
                _frame(1, 0.0, [_ball(0, 10.0)]),
            ], "REFERENCE_LIMITATION"),
            (valid_selection, [
                _frame(1, 0.0, [_ball(0, 10.0)]),
                _frame(3, 2.0, [_ball(0, 6.0)]),
            ], "INTEGRATION_MISMATCH"),
            (valid_selection, [
                _frame(1, 0.0, [_ball(0, 10.0)]),
                _frame(2, 1.0, [_ball(0, float("nan"))]),
                _frame(3, 2.0, [_ball(0, 6.0)]),
            ], "NUMERICAL_FAILURE"),
        )
        for selection, frames, expected_code in cases:
            with self.subTest(expected_code=expected_code):
                result = analyze_scenario(
                    _scenario("rolling_deceleration_cm_s2", selection, 2.0),
                    frames,
                )
                self.assertEqual(result.failures[0].code, expected_code)


if __name__ == "__main__":
    unittest.main()
