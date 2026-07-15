import copy
import math
import unittest
from pathlib import Path

from tools.physics_validation.alciatore_confirmation import (
    build_alciatore_scenarios,
    evaluate_alciatore,
)
from tools.physics_validation.confirmation_adapters import (
    confirmation_adapter,
    execute_deterministically,
)
from tools.physics_validation.reference_package import load_reference_package


ROOT = Path(__file__).resolve().parents[2]
PACKAGE = load_reference_package(
    ROOT / "tests/physics_validation/reference_data/alciatore_2005_tp_a15")
PROFILE = __import__("json").loads(
    (ROOT / "physics_models/profiles/chinese_pool_full_game_v3.json").read_text(
        encoding="utf-8"))["runtime_profile"]
INTERIOR = (8, 20, 34, 46, 57, 67, 77)
EXPECTED = {0: 0, 8: 13, 20: 34, 34: 50, 46: 61,
            57: 70, 67: 78, 77: 87, 90: 90}


def _vector(speed, angle_degrees):
    angle = math.radians(angle_degrees)
    return {
        "x": speed * math.cos(angle),
        "y": 0.0,
        "z": speed * math.sin(angle),
    }


def traces_with_interior_errors(errors):
    errors_by_phi = dict(zip(INTERIOR, errors))
    traces = {}
    for phi, expected in EXPECTED.items():
        observed = expected + errors_by_phi.get(phi, 0.0)
        cue_velocity = _vector(50.0, observed)
        object_velocity = _vector(40.0, 0.0) if phi < 90 else _vector(0.0, 0.0)
        contact = {
            "kind": "ball_ball",
            "normal": {"x": 1.0, "y": 0.0, "z": 0.0},
            "normal_impulse_ns": 0.1,
            "velocity_impulse_applied": True,
        }
        frames = []
        for tick, energy in ((1, 1.0), (2, 0.9)):
            frame = {
                "balls": [
                    {"index": 0, "velocity_cm_s": copy.deepcopy(cue_velocity)},
                    {"index": 1, "velocity_cm_s": copy.deepcopy(object_velocity)},
                ],
                "contacts": ([contact] if tick == 1 and phi < 90 else []),
                "tick": tick,
                "total_kinetic_energy_j": energy,
            }
            if tick == 1:
                frame["cue_contact"] = {
                    "applied": True,
                    "ball_velocity_after_cm_s": _vector(100.0, phi),
                }
            frames.append(frame)
        traces[f"alciatore_cut_{phi:03d}"] = frames
    return traces


class AlciatoreConfirmationTests(unittest.TestCase):
    def test_adapter_is_registered(self):
        self.assertEqual(
            confirmation_adapter("alciatore_2005_tp_a15").dataset_id,
            "alciatore_2005_tp_a15")

    def test_scenarios_are_touching_centered_horizontal_schema_v11(self):
        scenarios = build_alciatore_scenarios(PROFILE, PACKAGE)
        self.assertEqual(len(scenarios), 9)
        for scenario in scenarios.values():
            self.assertEqual(scenario["schema_version"], 11)
            self.assertEqual(scenario["boundary_mode"], "unbounded")
            self.assertTrue(scenario["expectations"])
            self.assertLessEqual(scenario["initial_contact_epsilon_cm"], 1e-5)
            cue = scenario["cue_impact"]
            self.assertEqual(cue["elevation_degrees"], 0)
            self.assertEqual(cue["tip_offset_radius"], [0, 0])
            first, second = scenario["balls"]
            distance = math.dist(first["position_cm"], second["position_cm"])
            self.assertAlmostEqual(distance, 2 * PROFILE["ball"]["radius_cm"])

    def test_threshold_boundaries_are_inclusive(self):
        evaluation = evaluate_alciatore(
            traces_with_interior_errors([3, -3, 3, -3, 3, -3, 3]),
            PROFILE, PACKAGE)
        self.assertEqual(evaluation.summary_metrics["interior_rmse_degrees"], 3)
        self.assertTrue(evaluation.summary_metrics["interior_rmse_passed"])
        self.assertTrue(evaluation.summary_metrics["interior_maximum_passed"])

    def test_maximum_error_rejects_above_five(self):
        evaluation = evaluate_alciatore(
            traces_with_interior_errors([0, 0, 0, 0, 0, 0, 5.0001]),
            PROFILE, PACKAGE)
        self.assertFalse(
            evaluation.summary_metrics["interior_maximum_passed"])

    def test_endpoint_contracts_are_inclusive_and_use_correct_ball(self):
        traces = traces_with_interior_errors([0] * 7)
        head_on = traces["alciatore_cut_000"]
        head_on[0]["balls"][0]["velocity_cm_s"]["z"] = 0.1
        head_on[1]["balls"][0]["velocity_cm_s"]["z"] = 0.1
        grazing = traces["alciatore_cut_090"]
        grazing[0]["balls"][1]["velocity_cm_s"]["x"] = 0.1
        grazing[1]["balls"][1]["velocity_cm_s"]["x"] = 0.1
        evaluation = evaluate_alciatore(traces, PROFILE, PACKAGE)
        summary = evaluation.summary_metrics
        self.assertTrue(summary["head_on_lateral_ratio_passed"])
        self.assertTrue(summary["grazing_object_speed_ratio_passed"])
        grazing_row = next(row for row in evaluation.rows
                           if row["point_id"] == "alciatore_cut_090")
        self.assertEqual(grazing_row["observed"], 90)

    def test_endpoint_ratios_and_direction_fail_above_limits(self):
        traces = traces_with_interior_errors([0] * 7)
        for frame in traces["alciatore_cut_000"]:
            frame["balls"][0]["velocity_cm_s"] = _vector(50, 1.0001)
        for frame in traces["alciatore_cut_090"]:
            frame["balls"][1]["velocity_cm_s"] = _vector(0.10001, 0)
        evaluation = evaluate_alciatore(traces, PROFILE, PACKAGE)
        summary = evaluation.summary_metrics
        self.assertFalse(summary["head_on_direction_passed"])
        self.assertFalse(summary["head_on_lateral_ratio_passed"])
        self.assertFalse(summary["grazing_object_speed_ratio_passed"])

    def test_missing_contact_nonfinite_state_and_energy_gain_fail(self):
        traces = traces_with_interior_errors([0] * 7)
        traces["alciatore_cut_020"][0]["contacts"] = []
        traces["alciatore_cut_034"][0]["balls"][0]["velocity_cm_s"]["x"] = (
            float("nan"))
        traces["alciatore_cut_046"][1]["total_kinetic_energy_j"] = 1.1
        evaluation = evaluate_alciatore(traces, PROFILE, PACKAGE)
        summary = evaluation.summary_metrics
        self.assertFalse(summary["contact_complete_passed"])
        self.assertFalse(summary["finite_state_passed"])
        self.assertFalse(summary["nonincreasing_total_energy_passed"])
        self.assertTrue(any(row["status"] == "FAILED"
                            for row in evaluation.rows))

    def test_nondeterministic_execution_fails_before_evaluation(self):
        calls = 0

        def alternating(_executable, _scenario):
            nonlocal calls
            calls += 1
            return [{"tick": 1, "value": calls}]

        with self.assertRaisesRegex(RuntimeError, "nondeterministic"):
            execute_deterministically(
                Path("fixture"),
                {"case": {"simulation": {"ticks": 1}}},
                alternating,
            )


if __name__ == "__main__":
    unittest.main()
