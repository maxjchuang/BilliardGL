from __future__ import annotations

import copy
import json
import sys
import tempfile
import unittest
from pathlib import Path


ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT))

from scripts.validate_phase4_v13_broadcast_predicted_dash_cap_release_design_gate import (  # noqa: E402
    ALLOWED_PRODUCTION_FILES,
    BINDINGS,
    DEFAULT_GATE,
    FALSE_FIELDS,
    REPAIR_IDS,
    load_json,
    validate,
    validate_bindings,
    validate_boundary,
    validate_cap_contract,
    validate_exhaustion_and_repairs,
    validate_preflight,
    validate_scope,
)


class Phase4V13PredictedDashCapReleaseDesignGateTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls) -> None:
        cls.gate = load_json(DEFAULT_GATE)

    def test_committed_gate_is_design_only_and_passes(self) -> None:
        result = validate(DEFAULT_GATE)
        self.assertEqual(result["status"],
                         "PASS_PHASE4_V13_PREDICTED_DASH_CAP_RELEASE_DESIGN_GATE")
        self.assertEqual(result["binding_count"], 6)
        self.assertEqual(result["candidate_count"], 2)
        self.assertEqual(result["repair_id_count"], 3)
        self.assertEqual(result["control_count"], 3)
        self.assertEqual(result["cpu_static_preflight"], "NOT_RUN")
        self.assertFalse(result["production_modified"])
        self.assertFalse(result["gpu_authorized"])
        self.assertFalse(result["authorization_created"])

    def test_json_round_trip(self) -> None:
        with tempfile.TemporaryDirectory() as directory:
            path = Path(directory) / "gate.json"
            path.write_text(json.dumps(self.gate, indent=2) + "\n")
            self.assertEqual(json.loads(path.read_text()), self.gate)

    def test_any_binding_drift_fails_closed(self) -> None:
        for index in range(len(BINDINGS)):
            gate = copy.deepcopy(self.gate)
            gate["frozen_bindings"][index]["sha256"] = "0" * 64
            with self.assertRaises(RuntimeError):
                validate_bindings(gate)

    def test_exhausted_actual_only_space_cannot_be_reopened(self) -> None:
        gate = copy.deepcopy(self.gate)
        gate["bound_exhaustion_facts"]["exhausted_actual_route_body_width_px"] = [5.0]
        with self.assertRaises(RuntimeError):
            validate_exhaustion_and_repairs(gate)
        gate = copy.deepcopy(self.gate)
        gate["bound_exhaustion_facts"]["actual_halo_pixels_may_not_substitute_for_actual_core"] = False
        with self.assertRaises(RuntimeError):
            validate_exhaustion_and_repairs(gate)

    def test_exact_78_plus_three_ids_cannot_be_weakened(self) -> None:
        self.assertEqual(self.gate["exact_repair_ids"]["predicted_contrast_repair_probe_ids"] +
                         [self.gate["exact_repair_ids"]["actual_connectivity_repair_probe_id"]],
                         REPAIR_IDS)
        gate = copy.deepcopy(self.gate)
        gate["exact_repair_ids"]["algorithm_replay_inherited_visible_count"] = 77
        with self.assertRaises(RuntimeError):
            validate_exhaustion_and_repairs(gate)
        gate = copy.deepcopy(self.gate)
        gate["exact_repair_ids"]["required_union_count"] = 80
        with self.assertRaises(RuntimeError):
            validate_exhaustion_and_repairs(gate)

    def test_equal_contraction_candidates_and_annulus_are_exact(self) -> None:
        candidates = self.gate["primary_equal_cap_contraction_sweep"]["ordered_candidates"]
        self.assertEqual([row["predicted_core_cap_extension_px"] for row in candidates],
                         [1.25, 1.0])
        for index in range(2):
            gate = copy.deepcopy(self.gate)
            gate["primary_equal_cap_contraction_sweep"]["ordered_candidates"][index][
                "core_to_inner_annulus_px"] = 1.75
            with self.assertRaises(RuntimeError):
                validate_cap_contract(gate)

    def test_dash_center_rhythm_cannot_change(self) -> None:
        for field, value in (("dash_on_px", 17.5), ("dash_off_px", 11.5),
                             ("period_px", 29.0)):
            gate = copy.deepcopy(self.gate)
            gate["primary_equal_cap_contraction_sweep"]["unchanged_center_pattern"][field] = value
            with self.assertRaises(RuntimeError):
                validate_cap_contract(gate)

    def test_fallback_cannot_mix_with_primary_or_change_zorder(self) -> None:
        for field in ("cannot_be_combined_with_equal_cap_contraction",
                      "must_not_change_draw_order",
                      "requires_renderer_tagged_prior_actual_core_coverage"):
            gate = copy.deepcopy(self.gate)
            gate["fallback_equivalent_cap_policy"][field] = False
            with self.assertRaises(RuntimeError):
                validate_cap_contract(gate)

    def test_allowlist_and_forbidden_boundaries_are_fail_closed(self) -> None:
        self.assertEqual([row["path"] for row in self.gate["allowed_production_files"]],
                         ALLOWED_PRODUCTION_FILES)
        gate = copy.deepcopy(self.gate)
        gate["allowed_production_files"].append({
            "path": "src/Billiards/training/final_semantic_ownership.cpp",
            "allowed_symbols_or_regions": "threshold",
        })
        with self.assertRaises(RuntimeError):
            validate_scope(gate)
        gate = copy.deepcopy(self.gate)
        gate["forbidden_changes"] = ["No product exposure"]
        with self.assertRaises(RuntimeError):
            validate_scope(gate)

    def test_all_three_gap_coordinates_and_control_proof_are_required(self) -> None:
        for coordinate in ("[783,413]", "[892,491]", "[940,525]"):
            gate = copy.deepcopy(self.gate)
            gate["required_cpu_and_static_preflight"]["required_checks"] = [
                row.replace(coordinate, "[0,0]") for row in
                gate["required_cpu_and_static_preflight"]["required_checks"]]
            with self.assertRaises(RuntimeError):
                validate_preflight(gate)
        gate = copy.deepcopy(self.gate)
        gate["required_cpu_and_static_preflight"]["required_checks"] = [
            row.replace("player 95 source/at least 89", "player inventory") for row in
            gate["required_cpu_and_static_preflight"]["required_checks"]]
        with self.assertRaises(RuntimeError):
            validate_preflight(gate)

    def test_no_design_gate_boundary_can_claim_execution_or_acceptance(self) -> None:
        for field in FALSE_FIELDS:
            gate = copy.deepcopy(self.gate)
            gate["proof_boundary"][field] = True
            with self.assertRaises(RuntimeError):
                validate_boundary(gate)


if __name__ == "__main__":
    unittest.main()
