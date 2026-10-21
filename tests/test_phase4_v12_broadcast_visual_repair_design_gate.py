from __future__ import annotations

import copy
import json
import sys
import tempfile
import unittest
from pathlib import Path


ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT))

from scripts.validate_phase4_v12_broadcast_visual_repair_design_gate import (  # noqa: E402
    ALLOWED_PRODUCTION_FILES,
    AXES,
    BINDINGS,
    CASES,
    DEFAULT_GATE,
    FALSE_BOUNDARY_FIELDS,
    OUTCOMES,
    load_json,
    validate,
    validate_bindings,
    validate_boundary,
    validate_failure_facts,
    validate_future_acceptance,
    validate_preflight,
    validate_scope,
)


class Phase4V12BroadcastVisualRepairDesignGateTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls) -> None:
        cls.gate = load_json(DEFAULT_GATE)

    def test_committed_design_gate_passes_without_authorizing_work(self) -> None:
        result = validate(DEFAULT_GATE)
        self.assertEqual(result["status"],
                         "PASS_PHASE4_V12_BROADCAST_VISUAL_REPAIR_DESIGN_GATE")
        self.assertEqual(result["frozen_binding_count"], 10)
        self.assertEqual(result["visual_outcome_count"], 6)
        self.assertEqual(result["allowed_production_file_count"], 2)
        self.assertEqual(result["cpu_static_preflight"], "NOT_RUN")
        self.assertEqual(result["future_targeted_case_count"], 5)
        self.assertEqual(result["future_ta_axis_count"], 35)
        self.assertFalse(result["production_modified"])
        self.assertFalse(result["gpu_authorized"])
        self.assertFalse(result["authorization_created"])

    def test_json_round_trip_preserves_gate(self) -> None:
        with tempfile.TemporaryDirectory() as directory:
            path = Path(directory) / "gate.json"
            path.write_text(json.dumps(self.gate, indent=2) + "\n")
            self.assertEqual(json.loads(path.read_text()), self.gate)

    def test_any_frozen_binding_drift_fails_closed(self) -> None:
        for index in range(len(BINDINGS)):
            gate = copy.deepcopy(self.gate)
            gate["frozen_bindings"][index]["sha256"] = "0" * 64
            with self.assertRaises(RuntimeError):
                validate_bindings(gate)

    def test_algorithm_gate_cannot_be_removed_or_folded_into_visual_gate(self) -> None:
        gate = copy.deepcopy(self.gate)
        gate["coexistence_with_algorithm_gate"][
            "algorithm_gate_remains_independently_required"] = False
        with self.assertRaises(RuntimeError):
            validate_scope(gate)
        gate = copy.deepcopy(self.gate)
        gate["coexistence_with_algorithm_gate"][
            "separate_change_sets_and_separate_validator_receipts_required"] = False
        with self.assertRaises(RuntimeError):
            validate_scope(gate)

    def test_failure_cannot_be_relabelled_machine_or_ta_pass(self) -> None:
        for field in ("targeted11_machine_pass", "targeted11_ta_pass",
                      "formal68_pass"):
            gate = copy.deepcopy(self.gate)
            gate["bound_failure_facts"][field] = True
            with self.assertRaises(RuntimeError):
                validate_failure_facts(gate)
        gate = copy.deepcopy(self.gate)
        gate["bound_failure_facts"]["broadcast_actual_source_visible_hidden"] = [12, 12, 0]
        with self.assertRaises(RuntimeError):
            validate_failure_facts(gate)

    def test_visual_outcomes_and_production_allowlist_are_exact(self) -> None:
        self.assertEqual([item["outcome_id"] for item in
                          self.gate["required_visual_outcomes"]], OUTCOMES)
        self.assertEqual([item["path"] for item in
                          self.gate["allowed_production_files"]],
                         ALLOWED_PRODUCTION_FILES)
        gate = copy.deepcopy(self.gate)
        gate["allowed_production_files"].append({
            "path": "src/Billiards/training/trajectory_overlay_model.cpp",
            "allowed_symbols_or_regions": "route",
        })
        with self.assertRaises(RuntimeError):
            validate_scope(gate)

    def test_threshold_offset_zorder_camera_workarounds_remain_forbidden(self) -> None:
        gate = copy.deepcopy(self.gate)
        gate["forbidden_changes"] = ["No product exposure"]
        with self.assertRaises(RuntimeError):
            validate_scope(gate)

    def test_cpu_snapshot_requires_all_actual_and_predicted_sources(self) -> None:
        checks = "\n".join(
            self.gate["required_cpu_and_static_preflight"]["required_checks"])
        self.assertIn("81/81", checks)
        self.assertIn("12 broadcast actual source roles", checks)
        self.assertIn("69 predicted source roles", checks)
        gate = copy.deepcopy(self.gate)
        gate["required_cpu_and_static_preflight"]["required_checks"] = [
            item.replace("81/81", "78/81") for item in
            gate["required_cpu_and_static_preflight"]["required_checks"]]
        with self.assertRaises(RuntimeError):
            validate_preflight(gate)

    def test_all_three_predicted_gap_neighborhoods_are_required(self) -> None:
        for coordinate in ("[783,413]", "[892,491]", "[940,525]"):
            gate = copy.deepcopy(self.gate)
            gate["required_cpu_and_static_preflight"]["required_checks"] = [
                item.replace(coordinate, "[0,0]") for item in
                gate["required_cpu_and_static_preflight"]["required_checks"]]
            with self.assertRaises(RuntimeError):
                validate_preflight(gate)

    def test_future_targeted5_and_35_ta_axes_are_exact(self) -> None:
        future = self.gate["future_targeted_capture_gate"]
        self.assertEqual([item["capture_id"] for item in
                          future["exact_minimum_cases"]], CASES)
        ta = self.gate["future_independent_ta_acceptance"]
        self.assertEqual(ta["canonical_axes"], AXES)
        self.assertEqual(ta["image_axis_count"], 35)
        gate = copy.deepcopy(self.gate)
        gate["future_independent_ta_acceptance"]["required_result"] = \
            "34_PASS_1_FAIL"
        with self.assertRaises(RuntimeError):
            validate_future_acceptance(gate)

    def test_targeted5_cannot_claim_targeted11_or_formal68(self) -> None:
        gate = copy.deepcopy(self.gate)
        gate["future_targeted_capture_gate"][
            "targeted5_pass_does_not_equal_targeted11_or_formal68"] = False
        with self.assertRaises(RuntimeError):
            validate_future_acceptance(gate)

    def test_design_gate_mutation_and_acceptance_boundaries_remain_false(self) -> None:
        for field in FALSE_BOUNDARY_FIELDS:
            gate = copy.deepcopy(self.gate)
            gate["proof_boundary"][field] = True
            with self.assertRaises(RuntimeError):
                validate_boundary(gate)


if __name__ == "__main__":
    unittest.main()
