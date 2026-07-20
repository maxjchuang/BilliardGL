import copy
import json
import unittest
from pathlib import Path

from tools.physics_validation.extract_han_2005 import han_restitution
from tools.physics_validation.confirmation_adapters import confirmation_adapter
from tools.physics_validation.han_confirmation import (
    build_han_scenarios,
    evaluate_han,
)
from tools.physics_validation.reference_package import load_reference_package


ROOT = Path(__file__).resolve().parents[2]
PACKAGE = load_reference_package(
    ROOT / "tests/physics_validation/reference_data/han_2005")
PROFILE = json.loads(
    (ROOT / "physics_models/profiles/chinese_pool_full_game_v3.json").read_text(
        encoding="utf-8"))["runtime_profile"]
SPEEDS = (0.5, 1.0, 1.5, 2.0, 2.5)


def passing_traces():
    traces = {}
    for speed in SPEEDS:
        point_id = f"han_speed_{int(speed * 100):03d}"
        restitution = han_restitution(speed)
        contact = {
            "incident_speed_cm_s": speed * 100,
            "kind": "rail",
            "normal_relative_speed_after_cm_s": restitution * speed * 100,
            "normal_relative_speed_before_cm_s": -speed * 100,
            "rigid_domain_exceeded": False,
        }
        traces[point_id] = [{
            "balls": [{"index": 0, "velocity_cm_s": {
                "x": -restitution * speed * 100, "y": 0.0, "z": 0.0}}],
            "contacts": [contact],
            "tick": 1,
            "total_kinetic_energy_j": 1.0,
        }, {
            "balls": [{"index": 0, "velocity_cm_s": {
                "x": -restitution * speed * 99, "y": 0.0, "z": 0.0}}],
            "contacts": [],
            "tick": 2,
            "total_kinetic_energy_j": 0.9,
        }]
    return traces


class HanConfirmationTests(unittest.TestCase):
    def test_adapter_is_registered(self):
        self.assertEqual(confirmation_adapter("han_2005").dataset_id, "han_2005")

    def test_scenarios_cover_committed_domain_on_production_table(self):
        scenarios = build_han_scenarios(PROFILE, PACKAGE)
        self.assertEqual(tuple(scenarios), tuple(
            f"han_speed_{int(speed * 100):03d}" for speed in SPEEDS))
        for speed, scenario in zip(SPEEDS, scenarios.values()):
            self.assertEqual(scenario["schema_version"], 11)
            self.assertEqual(scenario["boundary_mode"], "production_table")
            self.assertTrue(scenario["expectations"])
            self.assertAlmostEqual(
                scenario["balls"][0]["velocity_cm_s"][0], speed * 100)

    def test_han_contract_is_unchanged(self):
        evaluation = evaluate_han(passing_traces(), PROFILE, PACKAGE)
        summary = evaluation.summary_metrics
        self.assertEqual(summary["normalized_curve_rmse_maximum"], 0.15)
        self.assertEqual(set(summary["hard_metrics"]), {
            "finite_bounded_response", "continuous_response",
            "source_domain_response", "nonincreasing_total_energy",
        })
        self.assertTrue(summary["normalized_curve_rmse_passed"])
        self.assertTrue(all(summary[name + "_passed"] for name in (
            "finite_bounded_response", "continuous_response",
            "source_domain_response", "nonincreasing_total_energy")))

    def test_absolute_restitution_is_diagnostic_not_direct_gate(self):
        traces = passing_traces()
        for trace in traces.values():
            contact = trace[0]["contacts"][0]
            contact["normal_relative_speed_after_cm_s"] *= 0.8
        evaluation = evaluate_han(traces, PROFILE, PACKAGE)
        self.assertTrue(
            evaluation.summary_metrics["normalized_curve_rmse_passed"])
        self.assertTrue(all(row["pool_applicability"] == "TRANSFER_LIMITED"
                            for row in evaluation.rows))

    def test_missing_contact_nonfinite_response_and_energy_gain_fail(self):
        traces = passing_traces()
        traces["han_speed_100"][0]["contacts"] = []
        traces["han_speed_150"][0]["contacts"][0][
            "normal_relative_speed_after_cm_s"] = float("nan")
        traces["han_speed_200"][1]["total_kinetic_energy_j"] = 1.1
        evaluation = evaluate_han(traces, PROFILE, PACKAGE)
        summary = evaluation.summary_metrics
        self.assertFalse(summary["finite_bounded_response_passed"])
        self.assertFalse(summary["source_domain_response_passed"])
        self.assertFalse(summary["nonincreasing_total_energy_passed"])
        self.assertTrue(any(row["status"] == "FAILED"
                            for row in evaluation.rows))

    def test_discontinuous_normalized_response_fails(self):
        traces = passing_traces()
        contact = traces["han_speed_150"][0]["contacts"][0]
        contact["normal_relative_speed_after_cm_s"] = 0.01
        evaluation = evaluate_han(traces, PROFILE, PACKAGE)
        self.assertFalse(
            evaluation.summary_metrics["continuous_response_passed"])


if __name__ == "__main__":
    unittest.main()
