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


class AnalyzerTests(unittest.TestCase):
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
