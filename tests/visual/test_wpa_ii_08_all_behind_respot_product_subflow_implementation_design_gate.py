from __future__ import annotations

import copy
import importlib.util
import json
import sys
import unittest
from pathlib import Path


ROOT = Path(__file__).resolve().parents[2]
VALIDATOR = ROOT / "scripts/validate_wpa_ii_08_all_behind_respot_product_subflow_implementation_design_gate.py"
GATE = ROOT / "docs/rules/wpa-ii-08-all-behind-respot-product-subflow-implementation-design-gate-v1.json"
SPEC = importlib.util.spec_from_file_location("wpa_ii_08_all_behind_gate_validator", VALIDATOR)
assert SPEC and SPEC.loader
MODULE = importlib.util.module_from_spec(SPEC)
sys.modules[SPEC.name] = MODULE
SPEC.loader.exec_module(MODULE)


class WpaIi08AllBehindRespotProductSubflowGateTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls) -> None:
        cls.value = json.loads(GATE.read_text())

    def assertRejected(self, mutate) -> None:
        value = copy.deepcopy(self.value); mutate(value)
        self.assertTrue(MODULE.validate(value, check_files=False))

    def test_canonical_gate_passes_live(self) -> None:
        self.assertEqual([], MODULE.validate(self.value, check_files=True))

    def test_rejects_schema_status_or_extra_key(self) -> None:
        self.assertRejected(lambda v: v.__setitem__("schema", "v2"))
        self.assertRejected(lambda v: v.__setitem__("status", "IMPLEMENTED"))
        self.assertRejected(lambda v: v.__setitem__("authorization", True))

    def test_rejects_policy_original_gate_or_telemetry_drift(self) -> None:
        self.assertRejected(lambda v: v["authority_bindings"]["root_selected_policy"].__setitem__("selected_option_id", "LEADING_EDGE"))
        self.assertRejected(lambda v: v["authority_bindings"]["original_design_gap_gate"].__setitem__("sha256", "0" * 64))
        self.assertRejected(lambda v: v["authority_bindings"]["telemetry_implementation_receipt"].__setitem__("root_independently_verified", False))

    def test_rejects_foundation_global_ui_or_product_reclassification(self) -> None:
        for key in ("foundation_action_product_exposed", "global_rules_decision_ui_product_exposed", "product_may_reclassify_legal_balls"):
            self.assertRejected(lambda v, key=key: v["scope_and_authority"].__setitem__(key, True))

    def test_rejects_action_or_payload_scope_drift(self) -> None:
        self.assertRejected(lambda v: v["scope_and_authority"].__setitem__("request_action", "rules.foundation.respot"))
        self.assertRejected(lambda v: v["scope_and_authority"].__setitem__("tie_choice_action", "choose_any_ball"))

    def test_rejects_legal_predicate_or_on_line_broadening(self) -> None:
        self.assertRejected(lambda v: v["candidate_contract"].__setitem__("legal_object_predicate", "all active balls"))
        self.assertRejected(lambda v: v["candidate_contract"].__setitem__("on_line", "counts as behind"))

    def test_rejects_closest_tie_or_nonfinite_policy_drift(self) -> None:
        self.assertRejected(lambda v: v["candidate_contract"].__setitem__("closest", "minimum ball id"))
        self.assertRejected(lambda v: v["candidate_contract"].__setitem__("tie", "random selection"))
        self.assertRejected(lambda v: v["candidate_contract"].__setitem__("nonfinite_or_incomplete_layout", "ignore ball"))

    def test_rejects_core_e_revalidation_or_child_identity_loss(self) -> None:
        self.assertRejected(lambda v: v["transaction_contract"].__setitem__("unique_request", "move ball directly"))
        self.assertRejected(lambda v: v["transaction_contract"].__setitem__("successful_child", "reuse old child"))
        self.assertRejected(lambda v: v["transaction_contract"].__setitem__("partial_commit_allowed", True))

    def test_rejects_failure_retry_or_reset_rollback_loss(self) -> None:
        self.assertRejected(lambda v: v["transaction_contract"].__setitem__("no_solution", "drop old child"))
        self.assertRejected(lambda v: v["transaction_contract"].__setitem__("conflicting_retry", "return old ACK"))
        self.assertRejected(lambda v: v["transaction_contract"].__setitem__("reset", "retain product request"))

    def test_rejects_telemetry_regression_or_early_restriction(self) -> None:
        self.assertRejected(lambda v: v["telemetry_successor_contract"].__setitem__("unchanged_implementation_receipt_bound", False))
        self.assertRejected(lambda v: v["telemetry_successor_contract"].__setitem__("restriction_arms_only_after_subsequent_Core_G_placement_commit", False))
        self.assertRejected(lambda v: v["telemetry_successor_contract"].__setitem__("required_regression", "smoke only"))

    def test_rejects_production_or_test_allowlist_drift(self) -> None:
        self.assertRejected(lambda v: v["implementation_allowlist"]["production_files"].pop())
        self.assertRejected(lambda v: v["implementation_allowlist"]["test_files"].reverse())

    def test_rejects_new_file_rack_cmake_or_visual_scope(self) -> None:
        for key in ("new_production_file_allowed", "rack_executor_edit_allowed", "cmake_change_allowed", "visual_ui_or_renderer_allowed"):
            self.assertRejected(lambda v, key=key: v["implementation_allowlist"].__setitem__(key, True))

    def test_rejects_fixture_loss_order_or_telemetry_counts(self) -> None:
        self.assertRejected(lambda v: v["required_fixtures"]["positive"].pop())
        self.assertRejected(lambda v: v["required_fixtures"]["negative"].reverse())
        self.assertRejected(lambda v: v["required_fixtures"].__setitem__("telemetry_regression_positive_count", 7))
        self.assertRejected(lambda v: v["required_fixtures"].__setitem__("headless_cpu_only", False))

    def test_rejects_source_test_or_cmake_binding_drift(self) -> None:
        self.assertRejected(lambda v: v["source_bindings"][0].__setitem__("sha256", "0" * 64))
        self.assertRejected(lambda v: v["test_source_bindings"].pop())
        self.assertRejected(lambda v: v["cmake_binding"].__setitem__("modified_in_gate", True))
        self.assertRejected(lambda v: v["cmake_binding"].__setitem__("collector_target_link_gap_must_remain_honestly_recorded", False))

    def test_rejects_implementation_authorization_or_false_completion(self) -> None:
        self.assertRejected(lambda v: v["completion_boundary"].__setitem__("implementation_authorized_by_this_gate", True))
        self.assertRejected(lambda v: v["completion_boundary"].__setitem__("root_revalidation_required_before_tests_first_patch", False))
        for key in ("implementation_complete", "all_behind_product_reachable", "wpa_ii_08_complete", "production_modified", "cmake_modified", "visual_ui_modified", "gpu_executed"):
            self.assertRejected(lambda v, key=key: v["claims"].__setitem__(key, True))

    def test_malformed_inputs_fail_closed(self) -> None:
        for value in (None, [], "gate", 1): self.assertTrue(MODULE.validate(value, check_files=False))
        self.assertRejected(lambda v: v.__setitem__("authority_bindings", []))
        self.assertRejected(lambda v: v.__setitem__("required_fixtures", None))
        self.assertRejected(lambda v: v.__setitem__("claims", "complete"))


if __name__ == "__main__": unittest.main()
