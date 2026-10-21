from __future__ import annotations

import copy
import sys
import unittest
from pathlib import Path


ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT))

from scripts.validate_phase5_rules_decision_ui_v3_visual_repair_design_gate import (  # noqa: E402
    DEFAULT_GATE,
    EXPECTED_BINDINGS,
    load_json,
    validate,
    validate_bindings,
    validate_gate_structure,
)


class Phase5RulesDecisionUiV3VisualRepairDesignGateTests(
        unittest.TestCase):
    @classmethod
    def setUpClass(cls) -> None:
        cls.gate = load_json(DEFAULT_GATE)
        cls.loaded = validate_bindings(cls.gate)
        cls.manual = cls.loaded["v2_final_manual_failure_authority"]
        cls.manifest = cls.loaded["v2_stable_png_manifest"]
        cls.receipt = cls.loaded["v2_actual_gpu_machine_pass_receipt"]
        cls.original = cls.loaded["original_rules_ui_design_gate"]

    def assert_rejected(self, gate) -> None:
        with self.assertRaises(RuntimeError):
            validate_gate_structure(
                gate, self.manual, self.manifest,
                self.receipt, self.original)

    def test_committed_gate_is_fully_valid(self) -> None:
        result = validate(DEFAULT_GATE)
        self.assertEqual(result["status"], "PASS")
        self.assertFalse(result["gpu_executed"])
        self.assertFalse(result["production_modified"])
        self.assertFalse(result["product_exposed"])
        self.assertFalse(result["beta_pass"])

    def test_exact_frozen_bindings_are_required(self) -> None:
        gate = copy.deepcopy(self.gate)
        gate["read_only_bindings"][0]["sha256"] = "0" * 64
        with self.assertRaises(RuntimeError):
            validate_bindings(gate)
        self.assertEqual(
            set(EXPECTED_BINDINGS),
            {binding["role"] for binding in self.gate["read_only_bindings"]},
        )

    def test_manual_failure_inventory_drift_rejects(self) -> None:
        gate = copy.deepcopy(self.gate)
        gate["bound_v2_failure_set"]["failed_case_ids"].pop()
        self.assert_rejected(gate)
        gate = copy.deepcopy(self.gate)
        failures = gate["bound_v2_failure_set"]["failed_axis_case_ids"]
        failures["placement_scope_and_legality_readability"].pop()
        self.assert_rejected(gate)

    def test_scope_widening_or_authority_change_rejects(self) -> None:
        gate = copy.deepcopy(self.gate)
        gate["repair_contract"]["allowed_state_ids"].append(
            "eight_legal_open_unselected")
        self.assert_rejected(gate)
        gate = copy.deepcopy(self.gate)
        gate["forbidden_changes"]["authority_and_rules"][0] = (
            "Authority changes are allowed.")
        self.assert_rejected(gate)

    def test_armed_copy_or_independent_shape_token_omission_rejects(self) -> None:
        gate = copy.deepcopy(self.gate)
        gate["repair_contract"]["armed_confirm"]["confirm_copy_templates"][
            "en"] = "Confirm armed action"
        self.assert_rejected(gate)
        gate = copy.deepcopy(self.gate)
        gate["repair_contract"]["armed_confirm"][
            "non_color_shape_tokens"]["armed"] = "use fill"
        self.assert_rejected(gate)

    def test_placement_shape_or_bilingual_world_copy_omission_rejects(
            self) -> None:
        gate = copy.deepcopy(self.gate)
        gate["repair_contract"]["placement_world_overlay"][
            "invalid_candidate"]["required_shape"] = "red_square"
        self.assert_rejected(gate)
        gate = copy.deepcopy(self.gate)
        gate["repair_contract"]["placement_world_overlay"][
            "behind_head_string"]["in_world_copy"].pop("zh_cn")
        self.assert_rejected(gate)

    def test_machine_matrix_or_targeted_gpu_coverage_drift_rejects(
            self) -> None:
        gate = copy.deepcopy(self.gate)
        gate["machine_validation_gate"]["required_case_count"] = 539
        self.assert_rejected(gate)
        gate = copy.deepcopy(self.gate)
        gate["targeted_actual_gpu_gate"]["repair_case_ids"].pop()
        self.assert_rejected(gate)
        gate = copy.deepcopy(self.gate)
        gate["targeted_actual_gpu_gate"][
            "regression_control_case_ids"].pop()
        self.assert_rejected(gate)

    def test_threshold_or_exposure_promotion_rejects(self) -> None:
        gate = copy.deepcopy(self.gate)
        gate["inherited_thresholds_immutable"]["contrast"][
            "normal_text_minimum"] = 4.4
        self.assert_rejected(gate)
        gate = copy.deepcopy(self.gate)
        gate["evidence_boundary"]["gpu_execution_authorized"] = True
        self.assert_rejected(gate)
        gate = copy.deepcopy(self.gate)
        gate["product_exposed"] = True
        self.assert_rejected(gate)

    def test_ta_and_external_sessions_cannot_be_preclaimed(self) -> None:
        gate = copy.deepcopy(self.gate)
        gate["technical_art_re_review_gate"]["status"] = "PASS"
        self.assert_rejected(gate)
        gate = copy.deepcopy(self.gate)
        gate["required_external_sessions"][0]["status"] = "PASS"
        self.assert_rejected(gate)


if __name__ == "__main__":
    unittest.main()
