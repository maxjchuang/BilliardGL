from __future__ import annotations

import copy
import sys
import unittest
from pathlib import Path


ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT))

from scripts.validate_phase2_v10i_formal_full_six_reentry_acceptance_design_gate import (  # noqa: E402
    DEFAULT_GATE,
    EXPECTED_BINDINGS,
    load_json,
    validate,
    validate_bindings,
    validate_current_registry_and_capture,
    validate_gate_structure,
)


class Phase2V10iFormalFullSixReentryAcceptanceDesignGateTests(
        unittest.TestCase):
    @classmethod
    def setUpClass(cls) -> None:
        cls.gate = load_json(DEFAULT_GATE)

    def assert_rejected(self, gate) -> None:
        with self.assertRaises(RuntimeError):
            validate_gate_structure(gate)

    def test_committed_gate_passes_without_readiness_auth_gpu_or_edit(self) -> None:
        result = validate(DEFAULT_GATE)
        self.assertEqual(
            result["status"],
            "PASS_FULL_SIX_REENTRY_ACCEPTANCE_DESIGN_GATE_NO_GPU")
        self.assertEqual(result["required_initial_status"], "6_NOT_RUN")
        for field in (
            "targeted_pass_is_full_six_pass", "readiness_created",
            "authorization_prepared", "authorization_issued", "gpu_executed",
            "production_modified", "full_six_machine_pass",
            "full_six_ta_pass", "m2_promotion", "golden_promotion",
            "product_pass",
        ):
            self.assertFalse(result[field], field)

    def test_every_frozen_source_binary_and_lineage_binding_is_exact(self) -> None:
        self.assertEqual(
            {item["role"] for item in self.gate["frozen_bindings"]},
            set(EXPECTED_BINDINGS))
        for role in EXPECTED_BINDINGS:
            gate = copy.deepcopy(self.gate)
            item = next(binding for binding in gate["frozen_bindings"]
                        if binding["role"] == role)
            item["sha256"] = "0" * 64
            with self.assertRaises(RuntimeError, msg=role):
                validate_bindings(gate)

    def test_current_registry_and_capture_static_contract_passes(self) -> None:
        # Includes six instant semantic anchors and query-before-performance/
        # screenshot ordering; it performs no capture.
        validate_current_registry_and_capture()

    def test_targeted_pass_cannot_be_expanded_to_full_six_or_promotions(self) -> None:
        gate = copy.deepcopy(self.gate)
        gate["prerequisite_disposition"]["targeted_pass_does_not_prove"].remove(
            "full-six formal pass")
        self.assert_rejected(gate)
        gate = copy.deepcopy(self.gate)
        gate["prerequisite_disposition"]["v5_targeted_ta_scene_count"] = 6
        self.assert_rejected(gate)

    def test_scene_camera_or_original_roi_drift_rejects(self) -> None:
        gate = copy.deepcopy(self.gate)
        gate["full_six_scene_contract"]["expected_semantic_camera"][
            "pocket_closeup"]["preset"] = "player_low"
        self.assert_rejected(gate)
        gate = copy.deepcopy(self.gate)
        gate["full_six_scene_contract"]["interest_regions"][
            "ball_closeup"]["rect_normalized"] = [0, 0, 1, 1]
        self.assert_rejected(gate)

    def test_visible_performance_or_asset_threshold_relaxation_rejects(self) -> None:
        gate = copy.deepcopy(self.gate)
        gate["immutable_acceptance_thresholds"][
            "visible_pixel_channel_rule"] = "max_rgb_channel_gt_1"
        self.assert_rejected(gate)
        gate = copy.deepcopy(self.gate)
        gate["immutable_acceptance_thresholds"]["performance"][
            "p95_total_frame_ms_max"] = 20.0
        self.assert_rejected(gate)
        gate = copy.deepcopy(self.gate)
        gate["immutable_acceptance_thresholds"]["asset_attestation"][
            "silent_material_fallback_allowed"] = True
        self.assert_rejected(gate)

    def test_fresh_readiness_must_begin_with_exact_six_not_run(self) -> None:
        gate = copy.deepcopy(self.gate)
        gate["fresh_reentry_readiness_gate"]["required_initial_rows"][2][
            "status"] = "PASS"
        self.assert_rejected(gate)
        gate = copy.deepcopy(self.gate)
        gate["fresh_reentry_readiness_gate"]["required_initial_rows"].pop()
        self.assert_rejected(gate)
        gate = copy.deepcopy(self.gate)
        gate["fresh_reentry_readiness_gate"][
            "authorization_must_not_be_issued"] = False
        self.assert_rejected(gate)

    def test_one_shot_authorization_cannot_be_preissued_or_prepromote(self) -> None:
        gate = copy.deepcopy(self.gate)
        gate["future_one_shot_full_six_authorization_gate"]["status"] = (
            "AUTHORIZED_FOR_EXACTLY_SIX_FRESH_PROCESS_CAPTURES")
        self.assert_rejected(gate)
        gate = copy.deepcopy(self.gate)
        gate["future_one_shot_full_six_authorization_gate"][
            "formal_pass_at_issue"] = True
        self.assert_rejected(gate)
        gate = copy.deepcopy(self.gate)
        gate["future_one_shot_full_six_authorization_gate"][
            "authorization_artifact_created_by_this_gate"] = True
        self.assert_rejected(gate)

    def test_machine_gate_requires_all_six_and_freezes_any_failure(self) -> None:
        gate = copy.deepcopy(self.gate)
        gate["future_full_six_machine_gate"]["required_capture_count"] = 2
        self.assert_rejected(gate)
        gate = copy.deepcopy(self.gate)
        gate["future_full_six_machine_gate"]["any_failure_result"] = "RERUN"
        self.assert_rejected(gate)
        gate = copy.deepcopy(self.gate)
        gate["future_full_six_machine_gate"][
            "machine_pass_does_not_equal_m2_golden_or_product_pass"] = False
        self.assert_rejected(gate)

    def test_ta_gate_requires_machine_first_and_six_of_six_42_axes(self) -> None:
        gate = copy.deepcopy(self.gate)
        gate["future_independent_ta_six_of_six_gate"][
            "may_start_only_after_all_six_machine_pass"] = False
        self.assert_rejected(gate)
        gate = copy.deepcopy(self.gate)
        gate["future_independent_ta_six_of_six_gate"][
            "review_scene_count"] = 2
        self.assert_rejected(gate)
        gate = copy.deepcopy(self.gate)
        gate["future_independent_ta_six_of_six_gate"][
            "pass_requires_all_six_cases_and_all_42_axes_pass"] = False
        self.assert_rejected(gate)

    def test_production_change_or_evidence_promotion_rejects(self) -> None:
        gate = copy.deepcopy(self.gate)
        gate["allowed_changes_after_this_gate"].append(
            "production C++ camera changes")
        self.assert_rejected(gate)
        for field in (
            "production_modified", "readiness_created", "authorization_prepared",
            "authorization_issued", "gpu_executed", "capture_performed",
            "full_six_machine_pass", "full_six_ta_review_performed",
            "full_six_formal_pass", "m2_promotion", "golden_promotion",
            "product_pass",
        ):
            gate = copy.deepcopy(self.gate)
            gate["evidence_boundary"][field] = True
            self.assert_rejected(gate)


if __name__ == "__main__":
    unittest.main()
