from __future__ import annotations

import copy
import importlib.util
import json
import sys
import unittest
from pathlib import Path


ROOT = Path(__file__).resolve().parents[2]
VALIDATOR_PATH = ROOT / "scripts/validate_phase5_wpa_heyball_as_built_audit_v4.py"
AUDIT_PATH = ROOT / "docs/rules/phase5-wpa-heyball-as-built-reconciliation-audit-v4.json"

SPEC = importlib.util.spec_from_file_location("wpa_audit_v4_validator", VALIDATOR_PATH)
assert SPEC and SPEC.loader
MODULE = importlib.util.module_from_spec(SPEC)
sys.modules[SPEC.name] = MODULE
SPEC.loader.exec_module(MODULE)


class WpaHeyballAsBuiltAuditV4Tests(unittest.TestCase):
    @classmethod
    def setUpClass(cls) -> None:
        cls.value = json.loads(AUDIT_PATH.read_text(encoding="utf-8"))

    def assertRejected(self, mutate) -> None:
        value = copy.deepcopy(self.value)
        mutate(value)
        self.assertTrue(MODULE.validate(value, check_files=False))

    def row(self, value, row_id):
        return next(row for row in value["matrix"] if row["id"] == row_id)

    def test_canonical_audit_passes_live_validation(self) -> None:
        self.assertEqual([], MODULE.validate(self.value, check_files=True))

    def test_rejects_schema_scope_or_predecessor_drift(self) -> None:
        self.assertRejected(lambda value: value.__setitem__("schema_version", 5))
        self.assertRejected(lambda value: value.__setitem__("audit_scope", "all_rows"))
        self.assertRejected(lambda value: value["frozen_v3_predecessor"]
                            .__setitem__("sha256", "0" * 64))

    def test_rejects_target_demotion_or_false_product_gate(self) -> None:
        self.assertRejected(lambda value: self.row(value, "WPA-II-10-4")
                            .__setitem__("status", "PARTIAL"))
        self.assertRejected(lambda value: self.row(value, "WPA-II-10-4")
                            .__setitem__("product_reachable", False))
        self.assertRejected(lambda value: self.row(value, "WPA-II-10-4")
                            ["gaps"].append("still hidden"))

    def test_rejects_any_non_target_status_or_product_promotion(self) -> None:
        self.assertRejected(lambda value: self.row(value, "WPA-II-06-C")
                            .__setitem__("status", "IMPLEMENTED"))
        self.assertRejected(lambda value: self.row(value, "WPA-II-06-F")
                            .__setitem__("product_reachable", True))

    def test_rejects_non_target_assessment_or_gap_rewrite(self) -> None:
        self.assertRejected(lambda value: self.row(value, "WPA-II-13")
                            .__setitem__("assessment", "also complete"))
        self.assertRejected(lambda value: self.row(value, "WPA-II-08")
                            .__setitem__("gaps", []))

    def test_rejects_action_scope_expansion(self) -> None:
        self.assertRejected(lambda value: value["wpa_ii_10_4_product_closure"]
                            ["allowed_actions"].append("rules.foundation.rerack"))
        self.assertRejected(lambda value: value["wpa_ii_10_4_product_closure"]
                            .__setitem__("requires_live_offered_action", False))

    def test_rejects_actor_match_or_context_weakening(self) -> None:
        self.assertRejected(lambda value: value["wpa_ii_10_4_product_closure"]
                            .__setitem__("requires_match_screen", False))
        self.assertRejected(lambda value: value["wpa_ii_10_4_product_closure"]
                            .__setitem__("requires_local_offered_actor", False))
        self.assertRejected(lambda value: value["wpa_ii_10_4_product_closure"]
                            .__setitem__("ruleset_capability_flow_revision_and_state_hash_bound", False))

    def test_rejects_retry_or_conflict_contract_loss(self) -> None:
        self.assertRejected(lambda value: value["wpa_ii_10_4_product_closure"]
                            .__setitem__("exact_retry_returns_original_ack", False))
        self.assertRejected(lambda value: value["wpa_ii_10_4_product_closure"]
                            .__setitem__("conflicting_retry_fails_without_mutation", False))

    def test_rejects_global_ui_or_foundation_exposure(self) -> None:
        self.assertRejected(lambda value: value["wpa_ii_10_4_product_closure"]
                            .__setitem__("rules_decision_ui_product_exposed", True))
        self.assertRejected(lambda value: value["wpa_ii_10_4_product_closure"]
                            .__setitem__("foundation_actions_product_exposed", True))
        self.assertRejected(lambda value: value["wpa_ii_10_4_product_closure"]
                            .__setitem__("other_rules_parent_actions_product_exposed_by_this_increment", True))

    def test_rejects_summary_or_unclosed_inventory_drift(self) -> None:
        self.assertRejected(lambda value: value["summary"].__setitem__("implemented", 12))
        self.assertRejected(lambda value: value["m4_required_unclosed"]
                            .append("WPA-II-10-4"))

    def test_rejects_target_remaining_in_plan_or_missing_completion(self) -> None:
        self.assertRejected(lambda value: value["next_deterministic_implementation_plan"][0]
                            ["rows"].append("WPA-II-10-4"))
        self.assertRejected(lambda value: value.__setitem__(
            "completed_targeted_increments",
            [item for item in value["completed_targeted_increments"]
             if item.get("rows") != ["WPA-II-10-4"]]))

    def test_rejects_source_or_cpu_binding_substitution(self) -> None:
        self.assertRejected(lambda value: value["wpa_ii_10_4_implementation_bindings"]
                            ["changed_files"][0].__setitem__("sha256", "0" * 64))
        self.assertRejected(lambda value: value["wpa_ii_10_4_implementation_bindings"]
                            ["cpu_results"][1].__setitem__("sha256", "0" * 64))
        self.assertRejected(lambda value: value["wpa_ii_10_4_implementation_bindings"]
                            ["cpu_results"][2].__setitem__("exit_code", 1))

    def test_rejects_cmake_gpu_readiness_or_authorization_claim(self) -> None:
        self.assertRejected(lambda value: value["wpa_ii_10_4_implementation_bindings"]
                            .__setitem__("cmake_changed_by_increment", True))
        self.assertRejected(lambda value: value["wpa_ii_10_4_implementation_bindings"]
                            .__setitem__("gpu_cocoa_or_opengl_executed", True))
        self.assertRejected(lambda value: value["wpa_ii_10_4_implementation_bindings"]
                            .__setitem__("readiness_generated", True))
        self.assertRejected(lambda value: value["wpa_ii_10_4_implementation_bindings"]
                            .__setitem__("authorization_issued", True))

    def test_rejects_target_evidence_loss_or_path_substitution(self) -> None:
        self.assertRejected(lambda value: self.row(value, "WPA-II-10-4")
                            ["evidence"].pop())
        self.assertRejected(lambda value: self.row(value, "WPA-II-10-4")
                            ["evidence"][0].__setitem__("path", "../../fake.cpp"))

    def test_rejects_non_object_bad_input(self) -> None:
        self.assertTrue(MODULE.validate(None, check_files=False))
        self.assertTrue(MODULE.validate([], check_files=False))


if __name__ == "__main__":
    unittest.main()
