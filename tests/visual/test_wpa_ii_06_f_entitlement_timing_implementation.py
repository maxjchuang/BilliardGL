from __future__ import annotations

import copy
import importlib.util
import json
import sys
import unittest
from pathlib import Path


ROOT = Path(__file__).resolve().parents[2]
VALIDATOR_PATH = ROOT / "scripts/validate_wpa_ii_06_f_entitlement_timing_implementation.py"
RECEIPT_PATH = ROOT / "docs/rules/wpa-ii-06-f-entitlement-timing-implementation-receipt-v1.json"

SPEC = importlib.util.spec_from_file_location("wpa_ii_06_f_implementation_validator", VALIDATOR_PATH)
assert SPEC and SPEC.loader
MODULE = importlib.util.module_from_spec(SPEC)
sys.modules[SPEC.name] = MODULE
SPEC.loader.exec_module(MODULE)


class WpaIi06FEntitlementTimingImplementationTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls) -> None:
        cls.value = json.loads(RECEIPT_PATH.read_text(encoding="utf-8"))

    def assertRejected(self, mutate) -> None:
        value = copy.deepcopy(self.value)
        mutate(value)
        self.assertTrue(MODULE.validate(value, check_files=False))

    def test_canonical_receipt_passes_live_validation(self) -> None:
        self.assertEqual([], MODULE.validate(self.value, check_files=True))

    def test_contract_digest_is_frozen(self) -> None:
        self.assertEqual(MODULE.EXPECTED_CONTRACT_SHA,
                         self.value["implementation_contract_sha256"])
        self.assertEqual(MODULE.EXPECTED_CONTRACT_SHA,
                         MODULE.canonical_sha(self.value["implementation_contract"]))

    def test_rejects_schema_status_or_gate_drift(self) -> None:
        self.assertRejected(lambda value: value.__setitem__("schema", "v2"))
        self.assertRejected(lambda value: value.__setitem__("status", "COMPLETE"))
        self.assertRejected(lambda value: value["design_gate"].__setitem__("sha256", "0" * 64))

    def test_rejects_production_or_test_scope_expansion(self) -> None:
        self.assertRejected(lambda value: value["implementation_contract"]
                            ["change_scope"]["production_files_changed"]
                            .append("src/Billiards/render/modern/modern_product_session.cpp"))
        self.assertRejected(lambda value: value["implementation_contract"]
                            ["change_scope"].__setitem__("cmake_changed", True))

    def test_rejects_false_tests_first_claim(self) -> None:
        self.assertRejected(lambda value: value["implementation_contract"]
                            ["tests_first_red_result"]
                            .__setitem__("observed_before_production_fix", False))

    def test_rejects_hidden_or_broadened_predecessor_drift(self) -> None:
        self.assertRejected(lambda value: value["implementation_contract"]
                            ["predecessor_design_gate_live_validation"]
                            ["exact_errors"].append("source_product_session_drift"))
        self.assertRejected(lambda value: value["implementation_contract"]
                            ["predecessor_design_gate_live_validation"]
                            .__setitem__("predecessor_gate_or_validator_modified_to_hide_drift", True))

    def test_rejects_precommit_bih_or_scope(self) -> None:
        self.assertRejected(lambda value: value["implementation_contract"]
                            ["production_correction"]["foul_parent_staging"]
                            .__setitem__("match_state_ball_in_hand", True))
        self.assertRejected(lambda value: value["implementation_contract"]
                            ["production_correction"]["foul_parent_staging"]
                            .__setitem__("ball_in_hand_placement_scope", "BehindHeadString"))

    def test_rejects_precommit_pending_or_child(self) -> None:
        self.assertRejected(lambda value: value["implementation_contract"]
                            ["production_correction"]["foul_parent_staging"]
                            .__setitem__("ball_in_hand_placement_pending", True))
        self.assertRejected(lambda value: value["implementation_contract"]
                            ["production_correction"]["foul_parent_staging"]
                            .__setitem__("ball_in_hand_placement_offer_exists", True))

    def test_rejects_parent_physical_placement_or_duplicate_child(self) -> None:
        self.assertRejected(lambda value: value["implementation_contract"]
                            ["production_correction"]["spot_plus_bih_commit"]
                            .__setitem__("physical_cue_ball_placement_in_parent", True))
        self.assertRejected(lambda value: value["implementation_contract"]
                            ["production_correction"]["spot_plus_bih_commit"]
                            .__setitem__("one_core_g_child_created", False))

    def test_rejects_last_adjudication_commit_mismatch(self) -> None:
        self.assertRejected(lambda value: value["implementation_contract"]
                            ["production_correction"]["spot_plus_bih_commit"]
                            .__setitem__("last_adjudication_ball_in_hand_after_commit", False))
        self.assertRejected(lambda value: value["implementation_contract"]
                            ["production_correction"]["consumer_consistency"]
                            .__setitem__("accepted_parent_now_keeps_ack_match_state_last_adjudication_and_child_scope_consistent", False))

    def test_rejects_rerack_bih_or_legal_regression(self) -> None:
        self.assertRejected(lambda value: value["implementation_contract"]
                            ["production_correction"]["rerack_commit"]
                            .__setitem__("ball_in_hand", True))
        self.assertRejected(lambda value: value["implementation_contract"]
                            ["production_correction"]
                            .__setitem__("legal_branch_behavior_changed", True))

    def test_rejects_fail_closed_matrix_loss(self) -> None:
        self.assertRejected(lambda value: value["implementation_contract"]
                            ["fail_closed_matrix"].pop("authority_hash_drift"))
        self.assertRejected(lambda value: value["implementation_contract"]
                            ["fail_closed_matrix"].pop("respot_plan_failure"))
        self.assertRejected(lambda value: value["implementation_contract"]
                            ["fail_closed_matrix"].pop("reset"))

    def test_rejects_retry_or_conflict_semantic_drift(self) -> None:
        self.assertRejected(lambda value: value["implementation_contract"]
                            ["fail_closed_matrix"]
                            .__setitem__("exact_spot_retry", "creates another child"))
        self.assertRejected(lambda value: value["implementation_contract"]
                            ["fail_closed_matrix"]
                            .__setitem__("conflicting_retry_after_rerack", "grants BIH"))

    def test_rejects_cpu_failure_or_binary_substitution(self) -> None:
        self.assertRejected(lambda value: value["implementation_contract"]
                            ["cpu_results"][0].__setitem__("exit_code", 1))
        self.assertRejected(lambda value: value["implementation_contract"]
                            ["cpu_results"][1].__setitem__("sha256", "0" * 64))

    def test_rejects_false_fresh_cmake_claim(self) -> None:
        self.assertRejected(lambda value: value["implementation_contract"]
                            ["cpu_build"]
                            .__setitem__("cmake_reconfigure_used_for_acceptance", True))

    def test_rejects_product_seam_or_ui_exposure_promotion(self) -> None:
        self.assertRejected(lambda value: value["implementation_contract"]
                            ["audit_and_product_boundary"]
                            .__setitem__("complete_clause_6f_product_reachable", True))
        self.assertRejected(lambda value: value["implementation_contract"]
                            ["audit_and_product_boundary"]
                            .__setitem__("rules_decision_ui_product_exposed", True))
        self.assertRejected(lambda value: value["implementation_contract"]
                            ["audit_and_product_boundary"]
                            .__setitem__("existing_narrow_spot_seam_broadened", True))

    def test_rejects_classification_or_audit_promotion(self) -> None:
        self.assertRejected(lambda value: value["implementation_contract"]
                            ["audit_and_product_boundary"]
                            .__setitem__("audit_status", "IMPLEMENTED"))
        self.assertRejected(lambda value: value["implementation_contract"]
                            ["claims"].__setitem__("classification_complete", True))
        self.assertRejected(lambda value: value["implementation_contract"]
                            ["claims"].__setitem__("audit_promoted", True))

    def test_rejects_gpu_readiness_or_renderer_claim(self) -> None:
        self.assertRejected(lambda value: value["implementation_contract"]
                            ["forbidden_and_absent"]
                            .__setitem__("gpu_cocoa_or_opengl_executed", True))
        self.assertRejected(lambda value: value["implementation_contract"]
                            ["forbidden_and_absent"]
                            .__setitem__("readiness_generated", True))
        self.assertRejected(lambda value: value["implementation_contract"]
                            ["forbidden_and_absent"]
                            .__setitem__("phase4_or_phase5_renderer_changed", True))

    def test_rejects_source_binding_drift_or_unsafe_path(self) -> None:
        self.assertRejected(lambda value: value["source_bindings"]
                            ["production_after"].__setitem__("sha256", "0" * 64))
        self.assertRejected(lambda value: value["source_bindings"]
                            ["audit_v3_unchanged"].__setitem__("path", "../../audit.json"))

    def test_rejects_extra_top_level_or_missing_binding(self) -> None:
        self.assertRejected(lambda value: value.__setitem__("authorized", True))
        self.assertRejected(lambda value: value["source_bindings"].pop("product_session_unchanged"))

    def test_rejects_non_object_bad_input(self) -> None:
        self.assertTrue(MODULE.validate(None, check_files=False))
        self.assertTrue(MODULE.validate([], check_files=False))


if __name__ == "__main__":
    unittest.main()
