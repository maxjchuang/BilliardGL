from __future__ import annotations

import copy
import importlib.util
import json
import sys
import unittest
from pathlib import Path


ROOT = Path(__file__).resolve().parents[2]
VALIDATOR_PATH = ROOT / "scripts/validate_wpa_ii_06_d_selected_break_release_history_implementation.py"
RECEIPT_PATH = ROOT / "docs/rules/wpa-ii-06-d-selected-break-release-history-implementation-receipt-v1.json"

SPEC = importlib.util.spec_from_file_location("wpa_ii_06_d_selected_release_validator", VALIDATOR_PATH)
assert SPEC and SPEC.loader
MODULE = importlib.util.module_from_spec(SPEC)
sys.modules[SPEC.name] = MODULE
SPEC.loader.exec_module(MODULE)


class WpaIi06DSelectedBreakReleaseHistoryImplementationTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls) -> None:
        cls.value = json.loads(RECEIPT_PATH.read_text(encoding="utf-8"))

    def assertRejected(self, mutate) -> None:
        value = copy.deepcopy(self.value)
        mutate(value)
        self.assertTrue(MODULE.validate(value, check_files=False))

    def test_canonical_receipt_passes_live_validation(self) -> None:
        self.assertEqual([], MODULE.validate(self.value, check_files=True))

    def test_rejects_schema_status_or_extra_top_level(self) -> None:
        self.assertRejected(lambda value: value.__setitem__("schema", "v2"))
        self.assertRejected(lambda value: value.__setitem__("status", "IMPLEMENTED"))
        self.assertRejected(lambda value: value.__setitem__("authorized", True))

    def test_rejects_selection_or_authorized_stage_drift(self) -> None:
        self.assertRejected(lambda value: value["selection_binding"].__setitem__("sha256", "0" * 64))
        self.assertRejected(lambda value: value["selection_binding"].__setitem__(
            "selected_option_id", "PLACEMENT_COMMIT_RACK_SCOPE"))
        self.assertRejected(lambda value: value["selection_binding"].__setitem__(
            "authorized_stage", "B2_TESTS_FIRST_CPU_REFEREE_PROTOCOL_FOUNDATION_ONLY"))

    def test_rejects_predecessor_rewrite_or_unselected_policy(self) -> None:
        self.assertRejected(lambda value: value["predecessor_bindings"]["design_gate"].__setitem__(
            "historical_source_bindings_are_not_rewritten", False))
        self.assertRejected(lambda value: value["predecessor_bindings"]["design_gate"].__setitem__(
            "policy_pending_fields_resolved_only_by_root_selection", False))

    def test_rejects_sample_event_or_position_source_drift(self) -> None:
        self.assertRejected(lambda value: value["implementation_contract"].__setitem__(
            "sample_event", "PLACEMENT_COMMIT"))
        self.assertRejected(lambda value: value["implementation_contract"].__setitem__(
            "position_source", "LIVE_POST_SHOT_CUE_BALL"))

    def test_rejects_boundary_or_quantization_drift(self) -> None:
        self.assertRejected(lambda value: value["implementation_contract"].__setitem__(
            "foot_side_relation", "GREATER_THAN_OR_EQUAL"))
        self.assertRejected(lambda value: value["implementation_contract"].__setitem__(
            "quantization_grid_world_units", 0.00001))
        self.assertRejected(lambda value: value["implementation_contract"].__setitem__(
            "exact_half_is_offense", True))

    def test_rejects_reminder_or_foul_order_drift(self) -> None:
        self.assertRejected(lambda value: value["implementation_contract"].__setitem__(
            "first_offense_result", "FOUL"))
        self.assertRejected(lambda value: value["implementation_contract"].__setitem__(
            "subsequent_same_player_result", "REMINDER"))

    def test_rejects_history_partition_or_reset_scope_drift(self) -> None:
        self.assertRejected(lambda value: value["implementation_contract"].__setitem__(
            "history_partition", "GLOBAL"))
        self.assertRejected(lambda value: value["implementation_contract"].__setitem__(
            "history_scope", "RACK"))
        self.assertRejected(lambda value: value["implementation_contract"].__setitem__(
            "official_rerack_clears_history", True))

    def test_rejects_product_reclassification_or_direct_sample(self) -> None:
        self.assertRejected(lambda value: value["implementation_contract"].__setitem__(
            "product_or_automation_reclassifies_geometry", True))
        self.assertRejected(lambda value: value["implementation_contract"].__setitem__(
            "direct_or_synthetic_adjudication_creates_release_sample", True))

    def test_rejects_failed_physics_preview_or_rejected_launch_counting(self) -> None:
        for key in ("failed_physics_commits_history",
                    "drag_preview_or_repeated_state_edit_commits_history",
                    "rejected_contact_launch_commits_history",
                    "balls_moving_rejected_launch_commits_history"):
            self.assertRejected(lambda value, key=key: value["atomic_and_failure_contract"].__setitem__(key, True))

    def test_rejects_idempotency_or_atomicity_weakening(self) -> None:
        for key in ("history_commit_waits_for_completed_authoritative_physics_shot",
                    "other_player_history_is_independent",
                    "exact_shot_identity_retry_is_idempotent",
                    "conflicting_shot_identity_retry_is_rejected_without_mutation",
                    "invalid_actor_or_nonfinite_position_is_rejected_without_mutation",
                    "successful_history_and_adjudication_commit_share_one_authority_revision"):
            self.assertRejected(lambda value, key=key: value["atomic_and_failure_contract"].__setitem__(key, False))

    def test_rejects_visible_contract_or_ui_promotion(self) -> None:
        self.assertRejected(lambda value: value["presentation_and_readback_contract"].__setitem__(
            "visible_only_for_reminder_or_foul", False))
        self.assertRejected(lambda value: value["presentation_and_readback_contract"].__setitem__(
            "ui_draw_code_modified", True))
        self.assertRejected(lambda value: value["presentation_and_readback_contract"].__setitem__(
            "fresh_gpu_ta_required_before_visual_claim", False))

    def test_rejects_source_path_hash_or_binding_loss(self) -> None:
        self.assertRejected(lambda value: value["source_bindings"]["authority_header"].__setitem__(
            "sha256", "0" * 64))
        self.assertRejected(lambda value: value["source_bindings"]["runtime_cpp"].__setitem__(
            "path", "../../game_runtime.cpp"))
        self.assertRejected(lambda value: value["source_bindings"].pop("automation_cpp"))

    def test_rejects_cpu_failure_count_or_gpu_cmake_claim(self) -> None:
        self.assertRejected(lambda value: value["cpu_verification"]["binaries"][0].__setitem__("exit_code", 1))
        self.assertRejected(lambda value: value["cpu_verification"]["binaries"].pop())
        self.assertRejected(lambda value: value["cpu_verification"].__setitem__(
            "ctest_subset", {"passed": 7, "failed": 1}))
        self.assertRejected(lambda value: value["cpu_verification"].__setitem__(
            "gpu_cocoa_or_opengl_executed", True))
        self.assertRejected(lambda value: value["cpu_verification"].__setitem__(
            "cmake_source_modified", True))

    def test_rejects_evidence_inheritance_or_visual_completion(self) -> None:
        self.assertRejected(lambda value: value["evidence_invalidation"].__setitem__(
            "prior_source_bound_rules_runtime_automation_evidence", "CURRENT"))
        self.assertRejected(lambda value: value["evidence_invalidation"].__setitem__(
            "fresh_release_binary_bindings_required_before_future_readiness", False))
        self.assertRejected(lambda value: value["evidence_invalidation"].__setitem__(
            "gpu_or_readiness_authorized_here", True))

    def test_rejects_false_completion_or_scope_claims(self) -> None:
        for key in ("wpa_ii_06_d_row_closed", "audit_v5_modified_or_promoted", "profile_complete",
                    "m4_complete", "product_visual_evidence_complete", "ui_modified", "cmake_modified",
                    "phase4_or_phase5_renderer_modified", "gpu_executed",
                    "readiness_or_authorization_created"):
            self.assertRejected(lambda value, key=key: value["claims"].__setitem__(key, True))

    def test_rejects_loss_of_positive_cpu_claims(self) -> None:
        for key in ("selected_policy_cpu_implemented", "accepted_release_sample_authoritative",
                    "match_scope_history_cpu_verified", "automation_readback_cpu_verified"):
            self.assertRejected(lambda value, key=key: value["claims"].__setitem__(key, False))

    def test_malformed_inputs_fail_closed(self) -> None:
        for value in (None, [], "receipt", 1):
            self.assertTrue(MODULE.validate(value, check_files=False))
        self.assertRejected(lambda value: value.__setitem__("implementation_contract", []))
        self.assertRejected(lambda value: value.__setitem__("atomic_and_failure_contract", None))
        self.assertRejected(lambda value: value.__setitem__("source_bindings", "bindings"))
        self.assertRejected(lambda value: value.__setitem__("cpu_verification", []))
        self.assertRejected(lambda value: value.__setitem__("claims", None))


if __name__ == "__main__":
    unittest.main()
