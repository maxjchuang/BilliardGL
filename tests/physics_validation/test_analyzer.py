import json
import unittest

from tools.physics_validation.analyzer import (
    Failure,
    ScenarioResult,
    analyze_scenario,
    compare_integration_traces,
    compare_traces,
    match_known_failures,
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
