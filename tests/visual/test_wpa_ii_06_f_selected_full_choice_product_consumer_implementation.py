from __future__ import annotations

import copy
import importlib.util
import json
import sys
import unittest
from pathlib import Path


ROOT = Path(__file__).resolve().parents[2]
VALIDATOR_PATH = ROOT / "scripts/validate_wpa_ii_06_f_selected_full_choice_product_consumer_implementation.py"
RECEIPT_PATH = ROOT / "docs/rules/wpa-ii-06-f-selected-full-choice-product-consumer-implementation-receipt-v1.json"

SPEC = importlib.util.spec_from_file_location("wpa_ii_06_f_full_consumer_validator", VALIDATOR_PATH)
assert SPEC and SPEC.loader
MODULE = importlib.util.module_from_spec(SPEC)
sys.modules[SPEC.name] = MODULE
SPEC.loader.exec_module(MODULE)


class WpaIi06FSelectedFullChoiceProductConsumerImplementationTests(unittest.TestCase):
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
        self.assertRejected(lambda value: value.__setitem__("status", "COMPLETE"))
        self.assertRejected(lambda value: value.__setitem__("authorization", True))

    def test_rejects_selection_or_stage_drift(self) -> None:
        self.assertRejected(lambda value: value["selection_binding"].__setitem__("root_selected", False))
        self.assertRejected(lambda value: value["selection_binding"].__setitem__(
            "selected_option_id", "SPOT_ONLY"))
        self.assertRejected(lambda value: value["selection_binding"].__setitem__(
            "authorized_stage", "B2_TESTS_FIRST_CPU_REFEREE_PROTOCOL_FOUNDATION_ONLY"))
        self.assertRejected(lambda value: value["selection_binding"].__setitem__(
            "selection_alone_closes_row", True))

    def test_rejects_predecessor_rewrite_or_early_bih_regression(self) -> None:
        self.assertRejected(lambda value: value["predecessor_bindings"]["design_gate"].__setitem__(
            "historical_source_bindings_are_not_rewritten", False))
        self.assertRejected(lambda value: value["predecessor_bindings"][
            "entitlement_timing_implementation"].__setitem__("early_bih_defect_remains_fixed", False))

    def test_rejects_non_match_or_lane_count_drift(self) -> None:
        self.assertRejected(lambda value: value["exact_core_f_product_contract"].__setitem__(
            "product_screen", "TrainingSession"))
        self.assertRejected(lambda value: value["exact_core_f_product_contract"].__setitem__(
            "branch_action_lane_count", 4))

    def test_rejects_legal_actor_next_or_continue_drift(self) -> None:
        legal = "legal_branch"
        self.assertRejected(lambda value: value["exact_core_f_product_contract"][legal].__setitem__(
            "actor_relation", "OPPONENT_OF_FROZEN_ORIGINAL_BREAKER"))
        self.assertRejected(lambda value: value["exact_core_f_product_contract"][legal].__setitem__(
            "frozen_next_player_relation", "OPPONENT"))
        self.assertRejected(lambda value: value["exact_core_f_product_contract"][legal].__setitem__(
            "frozen_continue_turn", False))

    def test_rejects_legal_action_order_or_sibling(self) -> None:
        self.assertRejected(lambda value: value["exact_core_f_product_contract"]["legal_branch"][
            "ordered_action_ids"].reverse())
        self.assertRejected(lambda value: value["exact_core_f_product_contract"]["legal_branch"][
            "ordered_action_ids"].append("rules.break_foul.rerack_original_breaker"))

    def test_rejects_foul_actor_next_or_continue_drift(self) -> None:
        foul = "foul_branch"
        self.assertRejected(lambda value: value["exact_core_f_product_contract"][foul].__setitem__(
            "actor_relation", "FROZEN_ORIGINAL_BREAKER"))
        self.assertRejected(lambda value: value["exact_core_f_product_contract"][foul].__setitem__(
            "frozen_next_player_relation", "ORIGINAL_BREAKER"))
        self.assertRejected(lambda value: value["exact_core_f_product_contract"][foul].__setitem__(
            "frozen_continue_turn", True))

    def test_rejects_foul_action_order_loss_or_core_h_substitution(self) -> None:
        self.assertRejected(lambda value: value["exact_core_f_product_contract"]["foul_branch"][
            "ordered_action_ids"].pop())
        self.assertRejected(lambda value: value["exact_core_f_product_contract"]["foul_branch"][
            "ordered_action_ids"].reverse())
        self.assertRejected(lambda value: value["exact_core_f_product_contract"]["foul_branch"][
            "ordered_action_ids"].__setitem__(0, "rules.break_foul.ball_in_hand_behind_head_string"))

    def test_rejects_payload_or_idempotency_weakening(self) -> None:
        self.assertRejected(lambda value: value["exact_core_f_product_contract"].__setitem__(
            "canonical_payload_text", "{\"rerack\":true}"))
        self.assertRejected(lambda value: value["exact_core_f_product_contract"].__setitem__(
            "idempotency_key_length", {"minimum": 0, "maximum": 1024}))

    def test_rejects_revision_hash_ack_or_refresh_weakening(self) -> None:
        for key in ("request_binds_current_decision_revision", "request_binds_current_authority_state_hash",
                    "ack_verified_before_request_retention",
                    "authority_sync_and_ai_refresh_only_after_verified_ack"):
            self.assertRejected(lambda value, key=key: value["exact_core_f_product_contract"].__setitem__(key, False))

    def test_rejects_reclassification_sibling_or_global_exposure(self) -> None:
        self.assertRejected(lambda value: value["exact_core_f_product_contract"].__setitem__(
            "product_reclassifies_break_facts_or_branch", True))
        self.assertRejected(lambda value: value["exact_core_f_product_contract"].__setitem__(
            "core_h_or_direct_core_e_action_ids_product_reachable", True))
        self.assertRejected(lambda value: value["exact_core_f_product_contract"].__setitem__(
            "global_rules_decision_ui_exposed", True))

    def test_rejects_legal_spot_or_rerack_bih_semantic_drift(self) -> None:
        self.assertRejected(lambda value: value["effects_and_atomicity_contract"].__setitem__(
            "legal_spot_continues_original_breaker_turn_without_bih", False))
        self.assertRejected(lambda value: value["effects_and_atomicity_contract"].__setitem__(
            "legal_rerack_selects_original_breaker_without_bih", False))
        self.assertRejected(lambda value: value["effects_and_atomicity_contract"].__setitem__(
            "any_rerack_grants_or_preserves_bih", True))

    def test_rejects_foul_spot_early_or_non_atomic_bih(self) -> None:
        for key in ("foul_parent_stages_without_bih",
                    "foul_spot_parent_commit_grants_behind_head_string_bih",
                    "foul_spot_parent_commit_stages_exactly_one_core_g_child",
                    "foul_spot_child_requires_separate_authoritative_confirm"):
            self.assertRejected(lambda value, key=key: value["effects_and_atomicity_contract"].__setitem__(key, False))

    def test_rejects_foul_rerack_breaker_swap(self) -> None:
        self.assertRejected(lambda value: value["effects_and_atomicity_contract"].__setitem__(
            "foul_rerack_opponent_action_selects_decision_actor", False))
        self.assertRejected(lambda value: value["effects_and_atomicity_contract"].__setitem__(
            "foul_rerack_original_action_selects_frozen_original_breaker", False))

    def test_rejects_retry_conflict_reset_or_failure_mutation(self) -> None:
        self.assertRejected(lambda value: value["effects_and_atomicity_contract"].__setitem__(
            "exact_retry_returns_original_ack_without_duplicate_effect", False))
        for key in ("same_key_different_action_or_payload_mutates_authority",
                    "reset_stale_wrong_actor_wrong_action_wrong_payload_or_hash_drift_mutates_authority",
                    "failed_core_e_respot_mutates_parent_bih_child_revision_or_action",
                    "rejected_child_preview_weakens_committed_parent_entitlement",
                    "partial_parent_child_or_rerack_commit_allowed"):
            self.assertRejected(lambda value, key=key: value["effects_and_atomicity_contract"].__setitem__(key, True))

    def test_rejects_source_path_hash_or_binding_loss(self) -> None:
        self.assertRejected(lambda value: value["source_bindings"]["product_session_cpp"].__setitem__(
            "sha256", "0" * 64))
        self.assertRejected(lambda value: value["source_bindings"]["runtime_cpp"].__setitem__(
            "path", "../game_runtime.cpp"))
        self.assertRejected(lambda value: value["source_bindings"].pop("product_integration_tests"))

    def test_rejects_cpu_failure_count_or_forbidden_execution(self) -> None:
        self.assertRejected(lambda value: value["cpu_verification"]["binaries"][0].__setitem__("exit_code", 1))
        self.assertRejected(lambda value: value["cpu_verification"]["binaries"].pop())
        self.assertRejected(lambda value: value["cpu_verification"].__setitem__(
            "ctest_subset", {"passed": 9, "failed": 1}))
        self.assertRejected(lambda value: value["cpu_verification"].__setitem__(
            "gpu_cocoa_or_opengl_executed", True))
        self.assertRejected(lambda value: value["cpu_verification"].__setitem__(
            "cmake_source_modified_in_task", True))

    def test_rejects_evidence_inheritance_or_audit_rewrite(self) -> None:
        self.assertRejected(lambda value: value["evidence_invalidation"].__setitem__(
            "prior_spot_only_product_session_evidence", "CURRENT"))
        self.assertRejected(lambda value: value["evidence_invalidation"].__setitem__(
            "fresh_release_binary_bindings_required_before_future_readiness", False))
        self.assertRejected(lambda value: value["evidence_invalidation"].__setitem__(
            "audit_v5_is_not_rewritten_or_promoted_here", False))
        self.assertRejected(lambda value: value["evidence_invalidation"].__setitem__(
            "gpu_or_readiness_authorized_here", True))

    def test_rejects_false_completion_or_scope_expansion(self) -> None:
        for key in ("wpa_ii_06_f_row_closed", "upstream_break_foul_classification_complete",
                    "audit_v5_modified_or_promoted", "global_rules_decision_ui_exposed",
                    "core_h_or_direct_core_e_product_reachable", "product_visual_evidence_complete",
                    "profile_complete", "m4_complete", "ui_visual_modified", "cmake_modified",
                    "phase4_or_phase5_renderer_modified", "gpu_executed",
                    "readiness_or_authorization_created"):
            self.assertRejected(lambda value, key=key: value["claims"].__setitem__(key, True))

    def test_rejects_loss_of_positive_cpu_claims(self) -> None:
        for key in ("selected_full_core_f_consumer_cpu_implemented",
                    "all_five_branch_action_lanes_cpu_verified", "current_frozen_core_f_offer_only",
                    "ack_then_sync_and_ai_refresh_cpu_verified", "parent_child_and_rerack_atomicity_cpu_verified",
                    "wpa_ii_06_f_current_frozen_offer_product_seam_reachable"):
            self.assertRejected(lambda value, key=key: value["claims"].__setitem__(key, False))

    def test_malformed_inputs_fail_closed(self) -> None:
        for value in (None, [], "receipt", 1):
            self.assertTrue(MODULE.validate(value, check_files=False))
        self.assertRejected(lambda value: value.__setitem__("selection_binding", []))
        self.assertRejected(lambda value: value.__setitem__("exact_core_f_product_contract", None))
        self.assertRejected(lambda value: value.__setitem__("effects_and_atomicity_contract", "atomic"))
        self.assertRejected(lambda value: value.__setitem__("source_bindings", []))
        self.assertRejected(lambda value: value.__setitem__("cpu_verification", None))
        self.assertRejected(lambda value: value.__setitem__("claims", []))


if __name__ == "__main__":
    unittest.main()
