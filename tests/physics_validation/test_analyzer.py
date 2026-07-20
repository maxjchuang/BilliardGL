import json
import unittest

from tools.physics_validation.analyzer import (
    Failure,
    ScenarioResult,
    analyze_scenario,
    compare_integration_traces,
    compare_traces,
    contacts_for_solver_event,
    interpolated_transition_time,
    match_known_failures,
    maximal_phase_segment,
)


def scenario(metric, value=True, operator="eq", absolute_tolerance=0.0, grade="C"):
    return {
        "id": "case",
        "evidence": {"grade": grade, "source": "analytic", "equipment": "WPA_POOL"},
        "expectations": [{
            "metric": metric,
            "operator": operator,
            "value": value,
            "absolute_tolerance": absolute_tolerance,
        }],
    }


def frame(tick, energy=1.0, x=0.0, speed=1.0):
    return {
        "tick": tick,
        "time_seconds": tick * 0.1,
        "translational_kinetic_energy_j": energy,
        "maximum_penetration_cm": 0.0,
        "balls": [{
            "index": 0,
            "position_cm": {"x": x, "y": 0.0, "z": 0.0},
            "velocity_cm_s": {"x": speed, "y": 0.0, "z": 0.0},
            "acceleration_cm_s2": {"x": 0.0, "y": 0.0, "z": 0.0},
            "angular_velocity_rad_s": {"x": 0.0, "y": 0.0, "z": 0.0},
            "speed_cm_s": speed,
            "pocketed": False,
        }],
        "contacts": [],
    }


def interval_scenario(lower=4.5, upper=5.5, observed_metric="stopping_distance_cm"):
    return {
        "id": "reference_case",
        "balls": [{"index": 0, "position_cm": [0.0, 0.0, 0.0]}],
        "evidence": {"grade": "A", "source": "experiment", "equipment": "WPA_POOL"},
        "expectations": [{
            "metric": "value_within_interval",
            "value": {
                "point_id": "distance_01",
                "observed_metric": observed_metric,
                "ball_index": 0,
                "expected": 5.0,
                "lower": lower,
                "upper": upper,
                "unit": "cm",
            },
        }],
    }


class AnalyzerTests(unittest.TestCase):
    def _selection_interval(self, metric, expected, selection, unit):
        case = interval_scenario(
            lower=expected - 1e-9,
            upper=expected + 1e-9,
            observed_metric=metric,
        )
        case["expectations"][0]["value"].update({
            "expected": expected,
            "selection": selection,
            "unit": unit,
        })
        return case

    def test_phase_event_and_transition_selectors_use_physical_identity(self):
        frames = [frame(index) for index in range(1, 7)]
        phases = ["sliding", "sliding", "rolling", "sliding", "sliding", "sliding"]
        for item, phase in zip(frames, phases):
            item["balls"][0]["motion_state"] = phase
        selected = maximal_phase_segment(frames, 0, "sliding")
        self.assertEqual([item["tick"] for item in selected], [4, 5, 6])

        frames[1]["contacts"] = [
            {"solver_event_id": 17, "kind": "ball_ball"},
            {"solver_event_id": 29, "kind": "ball_ball"},
        ]
        frames[2]["contacts"] = [
            {"solver_event_id": 17, "kind": "ball_ball"},
        ]
        self.assertEqual(
            contacts_for_solver_event(frames, 17),
            [frames[1]["contacts"][0], frames[2]["contacts"][0]],
        )

        transition_frames = [frame(1), frame(2)]
        transition_frames[0]["balls"][0]["motion_state"] = "sliding"
        transition_frames[1]["balls"][0]["motion_state"] = "rolling"
        transition_frames[1]["delta_seconds"] = 0.1
        transition_frames[1]["surface_transitions"] = [{
            "after": "rolling",
            "ball_index": 0,
            "before": "sliding",
            "transition_time_seconds": 0.025,
        }]
        self.assertAlmostEqual(
            interpolated_transition_time(transition_frames, 0), 0.125)

    def test_phase_fit_excludes_other_motion_and_transition_uses_substep_time(self):
        deceleration_frames = [
            frame(1, speed=10.0), frame(2, speed=8.0), frame(3, speed=6.0),
            frame(4, speed=100.0), frame(5, speed=100.0),
        ]
        for item in deceleration_frames[:3]:
            item["balls"][0]["motion_state"] = "sliding"
        for item in deceleration_frames[3:]:
            item["balls"][0]["motion_state"] = "rolling"
        deceleration = self._selection_interval(
            "sliding_deceleration_cm_s2", 20.0,
            {
                "ball_index": 0,
                "minimum_window_ticks": 3,
                "motion_state": "sliding",
                "sample_phase": "maximal_motion_phase",
            },
            "cm/s^2",
        )
        self.assertTrue(analyze_scenario(
            deceleration, deceleration_frames).passed)

        transition_frames = [frame(1, speed=10.0), frame(2, speed=10.0),
                             frame(3, speed=10.0)]
        transition_frames[0]["balls"][0]["motion_state"] = "sliding"
        for item in transition_frames[1:]:
            item["balls"][0].update({
                "motion_state": "rolling",
                "angular_velocity_rad_s": {
                    "x": 0.0, "y": 0.0, "z": -5.0},
            })
        transition_frames[1]["delta_seconds"] = 0.1
        transition_frames[1]["surface_transitions"] = [{
            "after": "rolling", "ball_index": 0, "before": "sliding",
            "transition_time_seconds": 0.025,
        }]
        transition = self._selection_interval(
            "transition_to_rolling_time_seconds", 0.025,
            {
                "ball_index": 0,
                "ball_radius_cm": 2.0,
                "minimum_window_ticks": 2,
                "pure_roll_tolerance_cm_s": 0.001,
                "sample_phase": "first_stable_pure_roll",
                "time_origin_seconds": 0.1,
            },
            "s",
        )
        self.assertTrue(analyze_scenario(transition, transition_frames).passed)

    def test_unbounded_trace_rejects_boundary_contact(self):
        case = self._selection_interval(
            "cushion_rebound_speed_cm_s", 1.0,
            {
                "ball_index": 0,
                "event_kind": "rail_collision",
                "minimum_window_ticks": 1,
                "sample_phase": "first_sample_after_event",
            },
            "cm/s",
        )
        observed = frame(1)
        observed["boundary_mode"] = "unbounded"
        observed["contacts"] = [{
            "first_ball": 0, "kind": "rail", "second_ball": -1}]
        result = analyze_scenario(case, [observed])
        self.assertEqual(result.failures[0].code, "INTEGRATION_MISMATCH")
        self.assertIn("unbounded", result.failures[0].message)

    def test_trajectory_rmse_reconstructs_declared_tick_positions_in_mm(self):
        frames = [frame(1, x=1.0), frame(2, x=2.2), frame(3, x=3.0)]
        case = self._selection_interval(
            "trajectory_position_rmse_mm",
            2.0 / (3.0 ** 0.5),
            {
                "ball_index": 0,
                "minimum_window_ticks": 3,
                "reference_positions_cm": [
                    {"tick": 1, "position_cm": [1.0, 0.0, 0.0]},
                    {"tick": 2, "position_cm": [2.0, 0.0, 0.0]},
                    {"tick": 3, "position_cm": [3.0, 0.0, 0.0]},
                ],
                "sample_phase": "declared_trajectory_ticks",
            },
            "mm",
        )

        result = analyze_scenario(case, frames)

        self.assertTrue(result.passed)

    def test_stopping_and_rolling_transition_times_use_first_stable_window(self):
        frames = [frame(1, speed=10.0), frame(2, speed=0.05), frame(3, speed=0.04)]
        stop = self._selection_interval(
            "stopping_time_seconds",
            0.1,
            {
                "ball_index": 0,
                "minimum_window_ticks": 2,
                "sample_phase": "first_stable_stop",
                "speed_threshold_cm_s": 0.1,
                "time_origin_seconds": 0.1,
            },
            "s",
        )
        rolling_frames = [frame(1, speed=10.0), frame(2, speed=10.0), frame(3, speed=10.0)]
        for item in rolling_frames:
            item["balls"][0]["angular_velocity_rad_s"] = {
                "x": 0.0, "y": 0.0, "z": -item["balls"][0]["speed_cm_s"] / 2.0}
        transition = self._selection_interval(
            "transition_to_rolling_time_seconds",
            0.0,
            {
                "ball_index": 0,
                "ball_radius_cm": 2.0,
                "minimum_window_ticks": 3,
                "pure_roll_tolerance_cm_s": 0.001,
                "sample_phase": "first_stable_pure_roll",
                "time_origin_seconds": 0.1,
            },
            "s",
        )

        self.assertTrue(analyze_scenario(stop, frames).passed)
        self.assertTrue(analyze_scenario(transition, rolling_frames).passed)

        nonfinite = [frame(1, speed=float("nan")), frame(2, speed=0.0)]
        self.assertEqual(
            analyze_scenario(stop, nonfinite).failures[0].code,
            "NUMERICAL_FAILURE",
        )

    def test_explicit_surface_state_is_cross_checked_against_kinematics(self):
        selection = {
            "ball_index": 0,
            "ball_radius_cm": 2.0,
            "minimum_window_ticks": 1,
            "pure_roll_tolerance_cm_s": 0.001,
            "sample_phase": "first_stable_pure_roll",
            "time_origin_seconds": 0.0,
        }
        case = self._selection_interval(
            "transition_to_rolling_time_seconds", 0.1, selection, "s")
        consistent = frame(1, speed=10.0)
        consistent["balls"][0].update({
            "motion_state": "rolling",
            "contact_slip_speed_cm_s": 0.0,
            "rotational_kinetic_energy_j": 0.01,
            "angular_velocity_rad_s": {"x": 0.0, "y": 0.0, "z": -5.0},
        })
        self.assertTrue(analyze_scenario(case, [consistent]).passed)

        inconsistent = frame(1, speed=10.0)
        inconsistent["balls"][0].update({
            "motion_state": "rolling",
            "contact_slip_speed_cm_s": 10.0,
            "rotational_kinetic_energy_j": 0.0,
        })
        self.assertEqual(
            analyze_scenario(case, [inconsistent]).failures[0].code,
            "INTEGRATION_MISMATCH",
        )

        for field in ("contact_slip_speed_cm_s", "rotational_kinetic_energy_j"):
            with self.subTest(field=field):
                nonfinite_surface = json.loads(json.dumps(consistent))
                nonfinite_surface["balls"][0][field] = float("nan")
                self.assertEqual(
                    analyze_scenario(case, [nonfinite_surface]).failures[0].code,
                    "NUMERICAL_FAILURE",
                )

    def test_reference_value_inside_interval_comes_from_trace(self):
        result_y = frame(1, x=3.0)
        result_y["balls"][0]["position_cm"]["y"] = 4.0
        result = analyze_scenario(interval_scenario(), [result_y])

        self.assertTrue(result.passed)
        self.assertEqual(result.metrics["stopping_distance_cm"], 5.0)

    def test_reference_interval_includes_both_boundaries(self):
        for position in (4.5, 5.5):
            with self.subTest(position=position):
                result = analyze_scenario(interval_scenario(), [frame(1, x=position)])
                self.assertTrue(result.passed)

    def test_reference_value_outside_interval_is_model_mismatch(self):
        for position in (4.49, 5.51):
            with self.subTest(position=position):
                result = analyze_scenario(interval_scenario(), [frame(1, x=position)])
                self.assertEqual(result.failures[0].code, "MODEL_MISMATCH")
                self.assertEqual(result.failures[0].metric, "stopping_distance_cm")

    def test_reference_points_sharing_a_metric_keep_distinct_predictions(self):
        case = interval_scenario(lower=4.5, upper=5.5)
        second = json.loads(json.dumps(case["expectations"][0]))
        second["value"].update({
            "point_id": "distance_02",
            "ball_index": 1,
            "expected": 8.0,
            "lower": 7.5,
            "upper": 8.5,
        })
        case["balls"].append({"index": 1, "position_cm": [0.0, 0.0, 0.0]})
        case["expectations"].append(second)
        observed = frame(1, x=5.0)
        other = dict(observed["balls"][0], index=1)
        other["position_cm"] = {"x": 8.0, "y": 0.0, "z": 0.0}
        observed["balls"].append(other)

        result = analyze_scenario(case, [observed])

        self.assertEqual(result.metrics["distance_01"], 5.0)
        self.assertEqual(result.metrics["distance_02"], 8.0)

    def test_malformed_reference_interval_is_limitation(self):
        malformed = interval_scenario(lower=6.0, upper=5.0)
        missing = interval_scenario()
        del missing["expectations"][0]["value"]["lower"]

        for case in (malformed, missing):
            with self.subTest(case=case):
                result = analyze_scenario(case, [frame(1, x=5.0)])
                self.assertEqual(result.failures[0].code, "REFERENCE_LIMITATION")

    def test_unknown_trace_metric_is_integration_mismatch(self):
        result = analyze_scenario(
            interval_scenario(observed_metric="unimplemented_metric"),
            [frame(1, x=5.0)])

        self.assertEqual(result.failures[0].code, "INTEGRATION_MISMATCH")

    def test_nonfinite_reference_observation_is_numerical_failure(self):
        for position in (float("nan"), float("inf")):
            with self.subTest(position=position):
                result = analyze_scenario(interval_scenario(), [frame(1, x=position)])
                self.assertEqual(result.failures[0].code, "NUMERICAL_FAILURE")

    def test_energy_increase_is_numerical_failure(self):
        result = analyze_scenario(
            scenario("nonincreasing_translational_energy"),
            [frame(1, energy=1.0), frame(2, energy=1.01)])
        self.assertFalse(result.passed)
        self.assertEqual(result.failures[0].code, "NUMERICAL_FAILURE")

    def test_nonfinite_state_is_numerical_failure(self):
        result = analyze_scenario(scenario("finite_state"), [frame(1, x=float("nan"))])
        self.assertEqual(result.failures[0].metric, "finite_state")

    def test_experimental_tolerance_is_model_mismatch(self):
        result = analyze_scenario(
            scenario("final_speed_cm_s", value={"ball_index": 0, "value": 90.0},
                     absolute_tolerance=2.0, grade="A"),
            [frame(1, speed=95.0)])
        self.assertEqual(result.failures[0].code, "MODEL_MISMATCH")

    def test_missing_reference_data_is_explicit_limitation(self):
        result = analyze_scenario(scenario("final_speed_cm_s", grade="A"), [frame(1)])
        self.assertEqual(result.failures[0].code, "REFERENCE_LIMITATION")

    def test_trace_comparison_detects_nondeterminism(self):
        self.assertIsNone(compare_traces("case", [frame(1)], [frame(1)]))
        changed = frame(1)
        changed["balls"][0]["position_cm"]["x"] = 1.0
        failure = compare_traces("case", [frame(1)], [changed])
        self.assertEqual(failure.code, "NON_DETERMINISTIC")

    def test_core_process_disagreement_is_integration_mismatch(self):
        self.assertIsNone(compare_integration_traces("case", [frame(1)], [frame(1)]))
        process = frame(1)
        process["balls"][0]["position_cm"]["x"] = 1.0
        failure = compare_integration_traces("case", [frame(1)], [process])
        self.assertEqual(failure.code, "INTEGRATION_MISMATCH")
        self.assertEqual(failure.metric, "core_process_trace_equal")

    def test_permutation_invariance_compares_two_runs_by_identity_map(self):
        case = scenario(
            "permutation_invariance",
            value={"index_map": {"0": 2, "1": 1, "2": 0}},
            absolute_tolerance=0.001)
        baseline = frame(1, speed=10.0)
        baseline["balls"] = [
            dict(frame(1, speed=value)["balls"][0], index=index)
            for index, value in enumerate((0.0, 0.0, -10.0))
        ]
        permuted = frame(1, speed=10.0)
        permuted["balls"] = [
            dict(frame(1, speed=value)["balls"][0], index=index)
            for index, value in enumerate((-10.0, 0.0, 0.0))
        ]
        result = analyze_scenario(case, [baseline], [permuted])
        self.assertTrue(result.passed)

        permuted["balls"][0]["velocity_cm_s"]["x"] = -9.0
        result = analyze_scenario(case, [baseline], [permuted])
        self.assertEqual(result.failures[0].code, "NON_DETERMINISTIC")

    def test_permutation_invariance_requires_comparison_trace(self):
        case = scenario("permutation_invariance", value={"index_map": {"0": 1}})
        result = analyze_scenario(case, [frame(1)])
        self.assertEqual(result.failures[0].code, "INTEGRATION_MISMATCH")

    def test_known_failure_matching_is_strict(self):
        result = ScenarioResult(
            "case", False, "C", {},
            (Failure("NUMERICAL_FAILURE", "missed_collision", "missed", False, True),))
        manifest = {("case", "NUMERICAL_FAILURE", "missed_collision")}
        matched = match_known_failures([result], manifest)
        self.assertEqual(matched.known, manifest)
        self.assertEqual(matched.new, set())
        self.assertEqual(matched.missing, set())

        missing = match_known_failures([], manifest)
        self.assertEqual(missing.missing, manifest)

    def test_reference_limitation_is_a_new_failure_not_a_success(self):
        result = ScenarioResult(
            "case", False, "A", {},
            (Failure("REFERENCE_LIMITATION", "reference_data", "missing", True, None),))
        matched = match_known_failures([result], set())
        self.assertEqual(
            matched.new,
            {("case", "REFERENCE_LIMITATION", "reference_data")})


if __name__ == "__main__":
    unittest.main()
