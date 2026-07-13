import unittest

from tools.physics_validation.analyzer import analyze_scenario


def _ball(index, velocity, angular=(0.0, 0.0, 0.0)):
    return {
        "angular_velocity_rad_s": list(angular),
        "index": index,
        "position_cm": [0.0, 0.0, 0.0],
        "speed_cm_s": (velocity[0] ** 2 + velocity[2] ** 2) ** 0.5,
        "velocity_cm_s": list(velocity),
    }


def _frame(tick, first, second, *, contact=False, relative=(0.0, 0.0, 0.0)):
    return {
        "balls": [first, second],
        "contacts": ([{
            "first_ball": 0,
            "kind": "ball_ball",
            "relative_contact_velocity_cm_s": list(relative),
            "second_ball": 1,
        }] if contact else []),
        "maximum_penetration_cm": 0.0,
        "tick": tick,
        "translational_kinetic_energy_j": 1.0,
    }


def _scenario(metric, expected, selection):
    if isinstance(expected, str):
        expectation = {
            "metric": metric,
            "operator": "eq",
            "value": {"expected": expected, "selection": selection},
        }
    else:
        expectation = {
            "metric": "value_within_interval",
            "value": {
                "ball_index": 0,
                "expected": expected,
                "lower": expected - 0.001,
                "observed_metric": metric,
                "point_id": "point",
                "selection": selection,
                "unit": "degrees" if "angle" in metric else "rad/s",
                "upper": expected + 0.001,
            },
        }
    return {
        "evidence": {"equipment": "SOURCE_LABORATORY_APPARATUS", "grade": "B", "source": "experiment"},
        "expectations": [expectation],
        "id": "domenech_metric",
    }


class DomenechMetricTests(unittest.TestCase):
    def test_immediate_scattering_angle_uses_declared_axis_and_orientation(self):
        selection = {
            "angle_reference_axis": [1.0, 0.0],
            "ball_index": 0,
            "event_kind": "ball_ball",
            "minimum_window_ticks": 1,
            "positive_orientation": "counterclockwise",
            "sample_phase": "immediate_post_impact",
        }
        frames = [_frame(1, _ball(0, (1.0, 0.0, 1.0)), _ball(1, (1.0, 0.0, 0.0)), contact=True)]

        result = analyze_scenario(_scenario("cue_scattering_angle_degrees", 45.0, selection), frames)

        self.assertTrue(result.passed)
        self.assertAlmostEqual(result.metrics["cue_scattering_angle_degrees"], 45.0)

    def test_immediate_angular_velocity_selects_declared_ball(self):
        selection = {
            "ball_index": 0,
            "event_kind": "ball_ball",
            "minimum_window_ticks": 1,
            "sample_phase": "immediate_post_impact",
        }
        frames = [_frame(1, _ball(0, (1.0, 0.0, 0.0), (0.0, 3.0, 4.0)), _ball(1, (1.0, 0.0, 0.0)), contact=True)]

        result = analyze_scenario(_scenario("post_collision_angular_velocity_rad_s", 5.0, selection), frames)

        self.assertTrue(result.passed)

    def test_post_transition_separation_requires_stable_pure_roll_window(self):
        selection = {
            "ball_index": 0,
            "ball_radius_cm": 2.5,
            "event_kind": "ball_ball",
            "minimum_window_ticks": 2,
            "other_ball_index": 1,
            "pure_roll_tolerance_cm_s": 0.001,
            "sample_phase": "first_pure_roll_after_event",
        }
        sliding = _frame(1, _ball(0, (1.0, 0.0, 0.0)), _ball(1, (0.0, 0.0, 1.0)), contact=True)
        rolling_a = _frame(2, _ball(0, (1.0, 0.0, 0.0), (0.0, 0.0, -0.4)), _ball(1, (0.0, 0.0, 1.0), (0.4, 0.0, 0.0)))
        rolling_b = _frame(3, _ball(0, (1.0, 0.0, 0.0), (0.0, 0.0, -0.4)), _ball(1, (0.0, 0.0, 1.0), (0.4, 0.0, 0.0)))

        result = analyze_scenario(_scenario("separation_angle_degrees", 90.0, selection), [sliding, rolling_a, rolling_b])

        self.assertTrue(result.passed)

    def test_stick_slip_uses_finite_declared_relative_contact_velocity(self):
        selection = {
            "ball_index": 0,
            "event_kind": "ball_ball",
            "minimum_window_ticks": 1,
            "sample_phase": "immediate_post_impact",
            "stick_slip_epsilon_cm_s": 0.01,
        }
        frames = [_frame(1, _ball(0, (1.0, 0.0, 0.0)), _ball(1, (1.0, 0.0, 0.0)), contact=True, relative=(0.005, 0.0, 0.0))]

        result = analyze_scenario(_scenario("stick_slip_classification", "stick", selection), frames)

        self.assertTrue(result.passed)


if __name__ == "__main__":
    unittest.main()
