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


def _contact(relative=(0.0, 0.0, 0.0), **overrides):
    contact = {
        "first_ball": 0,
        "friction_coefficient": 0.2,
        "impulse_on_second_ns": [0.1, 0.0, 0.0],
        "kinetic_energy_after_j": 0.9,
        "kinetic_energy_before_j": 1.0,
        "kind": "ball_ball",
        "normal": [1.0, 0.0, 0.0],
        "normal_impulse_ns": 0.1,
        "normal_relative_speed_after_cm_s": relative[0],
        "normal_relative_speed_before_cm_s": -100.0,
        "regime": "stick",
        "relative_contact_velocity_after_cm_s": list(relative),
        "relative_contact_velocity_before_cm_s": [-100.0, 0.0, 20.0],
        "second_ball": 1,
        "solver_event_id": 17,
        "tangential_impulse_ns": 0.01,
        "velocity_impulse_applied": True,
    }
    contact.update(overrides)
    return contact


def _frame(tick, first, second, *, contact=False, relative=(0.0, 0.0, 0.0),
           contact_overrides=None):
    return {
        "balls": [first, second],
        "contacts": ([_contact(relative, **(contact_overrides or {}))]
                     if contact else []),
        "maximum_penetration_cm": 0.0,
        "tick": tick,
        "translational_kinetic_energy_j": 1.0,
    }


def _scenario(metric, expected, selection):
    selection = dict(selection)
    if selection.get("event_kind") == "ball_ball":
        selection.setdefault("solver_event_scope", "single")
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

    def test_ball_contact_friction_cone_violation_is_integration_mismatch(self):
        selection = {
            "ball_index": 0,
            "event_kind": "ball_ball",
            "minimum_window_ticks": 1,
            "sample_phase": "immediate_post_impact",
        }
        frames = [_frame(
            1, _ball(0, (1.0, 0.0, 0.0)), _ball(1, (1.0, 0.0, 0.0)),
            contact=True, contact_overrides={"tangential_impulse_ns": 0.03})]

        result = analyze_scenario(_scenario(
            "post_collision_angular_velocity_rad_s", 0.0, selection), frames)

        self.assertEqual(result.failures[0].code, "INTEGRATION_MISMATCH")

    def test_ball_contact_energy_creation_is_numerical_failure(self):
        selection = {
            "ball_index": 0,
            "event_kind": "ball_ball",
            "minimum_window_ticks": 1,
            "sample_phase": "immediate_post_impact",
        }
        frames = [_frame(
            1, _ball(0, (1.0, 0.0, 0.0)), _ball(1, (1.0, 0.0, 0.0)),
            contact=True, contact_overrides={"kinetic_energy_after_j": 1.1})]

        result = analyze_scenario(_scenario(
            "post_collision_angular_velocity_rad_s", 0.0, selection), frames)

        self.assertEqual(result.failures[0].code, "NUMERICAL_FAILURE")

    def test_nonfinite_ball_contact_is_numerical_failure(self):
        selection = {
            "ball_index": 0,
            "event_kind": "ball_ball",
            "minimum_window_ticks": 1,
            "sample_phase": "immediate_post_impact",
        }
        frames = [_frame(
            1, _ball(0, (1.0, 0.0, 0.0)), _ball(1, (1.0, 0.0, 0.0)),
            contact=True, contact_overrides={"normal_impulse_ns": float("nan")})]

        result = analyze_scenario(_scenario(
            "post_collision_angular_velocity_rad_s", 0.0, selection), frames)

        self.assertEqual(result.failures[0].code, "NUMERICAL_FAILURE")

    def test_nonseparating_ball_contact_is_integration_mismatch(self):
        selection = {
            "ball_index": 0,
            "event_kind": "ball_ball",
            "minimum_window_ticks": 1,
            "sample_phase": "immediate_post_impact",
        }
        frames = [_frame(
            1, _ball(0, (1.0, 0.0, 0.0)), _ball(1, (1.0, 0.0, 0.0)),
            contact=True,
            contact_overrides={
                "normal_relative_speed_after_cm_s": -1.0,
                "relative_contact_velocity_after_cm_s": [-1.0, 0.0, 0.0],
            })]

        result = analyze_scenario(_scenario(
            "post_collision_angular_velocity_rad_s", 0.0, selection), frames)

        self.assertEqual(result.failures[0].code, "INTEGRATION_MISMATCH")

    def test_duplicate_ball_velocity_impulse_is_integration_mismatch(self):
        selection = {
            "ball_index": 0,
            "event_kind": "ball_ball",
            "minimum_window_ticks": 1,
            "sample_phase": "immediate_post_impact",
        }
        frames = [
            _frame(1, _ball(0, (1.0, 0.0, 0.0)),
                   _ball(1, (1.0, 0.0, 0.0)), contact=True),
            _frame(2, _ball(0, (1.0, 0.0, 0.0)),
                   _ball(1, (1.0, 0.0, 0.0)), contact=True),
        ]

        result = analyze_scenario(_scenario(
            "post_collision_angular_velocity_rad_s", 0.0, selection), frames)

        self.assertEqual(result.failures[0].code, "INTEGRATION_MISMATCH")

    def test_multiple_solver_events_are_not_mixed_into_one_observation(self):
        selection = {
            "ball_index": 0,
            "event_kind": "ball_ball",
            "minimum_window_ticks": 1,
            "sample_phase": "immediate_post_impact",
        }
        frames = [
            _frame(1, _ball(0, (1.0, 0.0, 0.0)),
                   _ball(1, (1.0, 0.0, 0.0)), contact=True),
            _frame(2, _ball(0, (1.0, 0.0, 0.0)),
                   _ball(1, (1.0, 0.0, 0.0)), contact=True,
                   contact_overrides={"solver_event_id": 29}),
        ]
        result = analyze_scenario(_scenario(
            "post_collision_angular_velocity_rad_s", 0.0, selection), frames)
        self.assertEqual(result.failures[0].code, "INTEGRATION_MISMATCH")
        self.assertIn("one solver event", result.failures[0].message)


if __name__ == "__main__":
    unittest.main()
