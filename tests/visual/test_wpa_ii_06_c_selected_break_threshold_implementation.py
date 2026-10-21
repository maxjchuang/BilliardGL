from __future__ import annotations

import copy
import importlib.util
import json
import sys
import unittest
from pathlib import Path


ROOT = Path(__file__).resolve().parents[2]
VALIDATOR_PATH = ROOT / "scripts/validate_wpa_ii_06_c_selected_break_threshold_implementation.py"
RECEIPT_PATH = ROOT / "docs/rules/wpa-ii-06-c-selected-break-threshold-implementation-receipt-v1.json"

SPEC = importlib.util.spec_from_file_location("wpa_ii_06_c_selected_threshold_validator", VALIDATOR_PATH)
assert SPEC and SPEC.loader
MODULE = importlib.util.module_from_spec(SPEC)
sys.modules[SPEC.name] = MODULE
SPEC.loader.exec_module(MODULE)


class WpaIi06CSelectedBreakThresholdImplementationTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls) -> None:
        cls.value = json.loads(RECEIPT_PATH.read_text(encoding="utf-8"))

    def assertRejected(self, mutate) -> None:
        value = copy.deepcopy(self.value)
        mutate(value)
        self.assertTrue(MODULE.validate(value, check_files=False))

    def test_canonical_receipt_passes_live_validation(self) -> None:
        self.assertEqual([], MODULE.validate(self.value, check_files=True))

    def test_rejects_schema_status_or_selection_drift(self) -> None:
        self.assertRejected(lambda value: value.__setitem__("schema", "v2"))
        self.assertRejected(lambda value: value.__setitem__("status", "IMPLEMENTED"))
        self.assertRejected(lambda value: value["selection_binding"].__setitem__("sha256", "0" * 64))
        self.assertRejected(lambda value: value["selection_binding"].__setitem__(
            "selected_option_id", "KEEP_PRODUCT_CHOICE_CLOSED"))

    def test_rejects_scope_or_authorized_stage_drift(self) -> None:
        self.assertRejected(lambda value: value["implementation_scope"].__setitem__(
            "row_id", "WPA-II-06-D"))
        self.assertRejected(lambda value: value["implementation_scope"].__setitem__(
            "authorized_stage", "B2_TESTS_FIRST_CPU_REFEREE_PROTOCOL_FOUNDATION_ONLY"))

    def test_rejects_non_straight_or_non_impulse_contact_policy(self) -> None:
        self.assertRejected(lambda value: value["implementation_scope"]["qualifying_contact_contract"].__setitem__(
            "boundary_kind", "POCKET_JAW"))
        self.assertRejected(lambda value: value["implementation_scope"]["qualifying_contact_contract"].__setitem__(
            "velocity_impulse_applied", False))
        self.assertRejected(lambda value: value["implementation_scope"]["qualifying_contact_contract"].__setitem__(
            "position_correction_only_counts", True))

    def test_rejects_count_identity_or_jaw_broadening(self) -> None:
        self.assertRejected(lambda value: value["implementation_scope"]["qualifying_contact_contract"].__setitem__(
            "minimum_distinct_count", 3))
        self.assertRejected(lambda value: value["implementation_scope"]["qualifying_contact_contract"].__setitem__(
            "repeat_contact_by_same_object_counts_again", True))
        self.assertRejected(lambda value: value["implementation_scope"]["qualifying_contact_contract"].__setitem__(
            "pocket_jaw_counts", True))

    def test_rejects_object_pocket_or_scratch_semantic_drift(self) -> None:
        self.assertRejected(lambda value: value["implementation_scope"]["pocket_alternative_contract"].__setitem__(
            "object_ball_pocket_satisfies_legal_break", False))
        self.assertRejected(lambda value: value["implementation_scope"]["pocket_alternative_contract"].__setitem__(
            "cue_ball_scratch_satisfies_pocket_alternative", True))
        self.assertRejected(lambda value: value["implementation_scope"]["pocket_alternative_contract"].__setitem__(
            "cue_ball_scratch_remains_independent_foul", False))

    def test_rejects_authority_or_completeness_broadening(self) -> None:
        self.assertRejected(lambda value: value["implementation_scope"]["fact_contract"].__setitem__(
            "authoritative_origin", "DirectOrSynthetic"))
        self.assertRejected(lambda value: value["implementation_scope"]["fact_contract"].__setitem__(
            "completeness_required_for_product_illegal_break_offer", False))
        self.assertRejected(lambda value: value["implementation_scope"]["fact_contract"].__setitem__(
            "canonical_contact_rows_consumed_by_rules_adjudicator", False))

    def test_rejects_product_reclassification_or_direct_offer(self) -> None:
        self.assertRejected(lambda value: value["implementation_scope"]["fact_contract"].__setitem__(
            "product_reclassifies_contacts", True))
        self.assertRejected(lambda value: value["implementation_scope"]["fact_contract"].__setitem__(
            "direct_or_synthetic_scalar_may_open_runtime_product_offer", True))
        self.assertRejected(lambda value: value["implementation_scope"]["fact_contract"].__setitem__(
            "direct_offer_rejection_code", ""))

    def test_rejects_failed_step_or_pre_mutation_regression(self) -> None:
        self.assertRejected(lambda value: value["atomic_and_failure_contract"].__setitem__(
            "failed_physics_step_contributes_contact", True))
        self.assertRejected(lambda value: value["atomic_and_failure_contract"].__setitem__(
            "failed_step_fourth_ball_crosses_threshold", True))
        self.assertRejected(lambda value: value["atomic_and_failure_contract"].__setitem__(
            "malformed_or_incomplete_authoritative_evidence_is_rejected_before_runtime_mutation", False))
        self.assertRejected(lambda value: value["atomic_and_failure_contract"].__setitem__(
            "direct_or_synthetic_illegal_break_offer_attempt_is_rejected_before_runtime_mutation", False))

    def test_rejects_source_hash_or_path_drift(self) -> None:
        self.assertRejected(lambda value: value["source_bindings"]["rules_cpp"].__setitem__("sha256", "0" * 64))
        self.assertRejected(lambda value: value["source_bindings"]["runtime_cpp"].__setitem__(
            "path", "../../game_runtime.cpp"))
        self.assertRejected(lambda value: value["source_bindings"].pop("collector_tests"))

    def test_rejects_cpu_failure_or_test_count_drift(self) -> None:
        self.assertRejected(lambda value: value["cpu_build"]["binaries"][0].__setitem__("exit_code", 1))
        self.assertRejected(lambda value: value["cpu_build"]["binaries"].pop())
        self.assertRejected(lambda value: value["cpu_build"].__setitem__(
            "ctest_rules_subset", {"passed": 6, "failed": 1}))

    def test_rejects_cmake_gap_or_gpu_boundary_falsification(self) -> None:
        self.assertRejected(lambda value: value["cpu_build"].__setitem__("cmake_source_modified", True))
        self.assertRejected(lambda value: value["cpu_build"].__setitem__("gpu_cocoa_or_opengl_executed", True))
        self.assertRejected(lambda value: value["cpu_build"]["collector_target_link_gap"].__setitem__(
            "cmake_fix_authorized", True))

    def test_rejects_evidence_inheritance_or_gpu_authorization(self) -> None:
        self.assertRejected(lambda value: value["evidence_invalidation"].__setitem__(
            "prior_source_bound_rules_and_runtime_evidence", "CURRENT"))
        self.assertRejected(lambda value: value["evidence_invalidation"].__setitem__(
            "fresh_release_binary_bindings_required_before_future_readiness", False))
        self.assertRejected(lambda value: value["evidence_invalidation"].__setitem__(
            "gpu_or_readiness_authorized_here", True))

    def test_rejects_false_completion_or_scope_claims(self) -> None:
        for key in ("wpa_ii_06_c_row_closed", "product_choice_seam_complete",
                    "audit_v5_modified_or_promoted", "profile_complete", "m4_complete", "ui_modified",
                    "cmake_modified", "phase4_or_phase5_renderer_modified", "gpu_executed",
                    "readiness_or_authorization_created"):
            self.assertRejected(lambda value, key=key: value["claims"].__setitem__(key, True))

    def test_rejects_loss_of_positive_implementation_claims(self) -> None:
        for key in ("selected_threshold_policy_implemented",
                    "canonical_authoritative_product_classification_enforced", "cpu_verified"):
            self.assertRejected(lambda value, key=key: value["claims"].__setitem__(key, False))

    def test_malformed_inputs_fail_closed(self) -> None:
        for value in (None, [], "receipt", 1):
            self.assertTrue(MODULE.validate(value, check_files=False))
        self.assertRejected(lambda value: value.__setitem__("implementation_scope", "scope"))
        self.assertRejected(lambda value: value.__setitem__("source_bindings", []))
        self.assertRejected(lambda value: value.__setitem__("cpu_build", None))
        self.assertRejected(lambda value: value.__setitem__("claims", []))


if __name__ == "__main__":
    unittest.main()
