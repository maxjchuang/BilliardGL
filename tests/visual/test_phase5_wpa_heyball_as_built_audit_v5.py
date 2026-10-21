from __future__ import annotations

import copy
import importlib.util
import json
import sys
import unittest
from pathlib import Path


ROOT = Path(__file__).resolve().parents[2]
VALIDATOR_PATH = ROOT / "scripts/validate_phase5_wpa_heyball_as_built_audit_v5.py"
AUDIT_PATH = ROOT / "docs/rules/phase5-wpa-heyball-as-built-reconciliation-audit-v5.json"

SPEC = importlib.util.spec_from_file_location("wpa_audit_v5_validator", VALIDATOR_PATH)
assert SPEC and SPEC.loader
MODULE = importlib.util.module_from_spec(SPEC)
sys.modules[SPEC.name] = MODULE
SPEC.loader.exec_module(MODULE)


class WpaHeyballAsBuiltAuditV5Tests(unittest.TestCase):
    @classmethod
    def setUpClass(cls) -> None:
        cls.value = json.loads(AUDIT_PATH.read_text(encoding="utf-8"))

    def assertRejected(self, mutate) -> None:
        value = copy.deepcopy(self.value)
        mutate(value)
        self.assertTrue(MODULE.validate(value, check_files=False))

    @staticmethod
    def row(value, row_id):
        return next(row for row in value["matrix"] if row["id"] == row_id)

    def test_canonical_audit_passes_live_validation(self) -> None:
        self.assertEqual([], MODULE.validate(self.value, check_files=True))

    def test_rejects_schema_scope_or_predecessor_drift(self) -> None:
        self.assertRejected(lambda value: value.__setitem__("schema_version", 6))
        self.assertRejected(lambda value: value.__setitem__("audit_scope", "complete"))
        self.assertRejected(lambda value: value["frozen_v4_predecessor"]
                            .__setitem__("sha256", "0" * 64))

    def test_rejects_false_full_row_promotion(self) -> None:
        for row_id in ("WPA-II-08", "WPA-II-09"):
            self.assertRejected(lambda value, row_id=row_id:
                                self.row(value, row_id)
                                .__setitem__("status", "IMPLEMENTED"))
            self.assertRejected(lambda value, row_id=row_id:
                                self.row(value, row_id)
                                .__setitem__("product_reachable", True))

    def test_rejects_unresolved_gap_or_semantic_loss(self) -> None:
        self.assertRejected(lambda value: self.row(value, "WPA-II-08")
                            .__setitem__("gaps", []))
        self.assertRejected(lambda value: self.row(value, "WPA-II-09")
                            ["gaps"].pop())
        self.assertRejected(lambda value: value["wpa_ii_08_09_product_placement_subflow"]
                            ["unresolved_clause_semantics"]
                            .__setitem__("WPA-II-09", []))

    def test_rejects_preview_or_confirm_authority_weakening(self) -> None:
        contract = "wpa_ii_08_09_product_placement_subflow"
        for key in ("requires_match_screen", "requires_current_live_offer",
                    "requires_local_offered_actor", "requires_current_legal_preview",
                    "ruleset_capability_flow_decision_revision_state_hash_bound",
                    "position_payload_is_authoritatively_revalidated"):
            self.assertRejected(lambda value, key=key:
                                value[contract].__setitem__(key, False))
        self.assertRejected(lambda value: value[contract]
                            .__setitem__("preview_authority_mutation", True))

    def test_rejects_scope_or_action_expansion(self) -> None:
        contract = self.value["wpa_ii_08_09_product_placement_subflow"]
        self.assertEqual({"BehindHeadString", "Anywhere"},
                         set(contract["allowed_actions"]))
        self.assertRejected(lambda value: value["wpa_ii_08_09_product_placement_subflow"]
                            ["allowed_actions"].__setitem__(
                                "Foundation", "rules.foundation.rerack"))
        self.assertRejected(lambda value: value["wpa_ii_08_09_product_placement_subflow"]
                            .__setitem__("row_status_promotions", ["WPA-II-09"]))

    def test_rejects_retry_or_conflict_contract_loss(self) -> None:
        contract = "wpa_ii_08_09_product_placement_subflow"
        self.assertRejected(lambda value: value[contract]
                            .__setitem__("exact_retry_returns_original_ack", False))
        self.assertRejected(lambda value: value[contract]
                            .__setitem__("conflicting_retry_fails_without_mutation", False))

    def test_rejects_global_ui_foundation_or_complete_claim(self) -> None:
        contract = "wpa_ii_08_09_product_placement_subflow"
        self.assertRejected(lambda value: value[contract]
                            .__setitem__("rules_decision_ui_product_exposed", True))
        self.assertRejected(lambda value: value[contract]
                            .__setitem__("foundation_or_other_rules_actions_product_exposed", True))
        self.assertRejected(lambda value: value[contract]
                            .__setitem__("complete_clause_product_reachable", True))

    def test_rejects_unrelated_row_promotion_or_rewrite(self) -> None:
        self.assertRejected(lambda value: self.row(value, "WPA-II-06-C")
                            .__setitem__("status", "IMPLEMENTED"))
        self.assertRejected(lambda value: self.row(value, "WPA-II-13")
                            .__setitem__("assessment", "rewritten"))

    def test_rejects_summary_or_m4_inventory_promotion(self) -> None:
        self.assertRejected(lambda value: value["summary"]
                            .__setitem__("implemented", 13))
        self.assertRejected(lambda value: value["m4_required_unclosed"]
                            .remove("WPA-II-08"))

    def test_rejects_subincrement_as_complete_rows(self) -> None:
        self.assertRejected(lambda value: value["completed_product_subincrements"][0]
                            .__setitem__("rows_remain_partial", False))
        self.assertRejected(lambda value: value["completed_product_subincrements"][0]
                            ["rows"].remove("WPA-II-09"))

    def test_rejects_missing_semantic_followup_plan(self) -> None:
        self.assertRejected(lambda value: value["next_deterministic_implementation_plan"]
                            .pop())
        self.assertRejected(lambda value: value["next_deterministic_implementation_plan"][-1]
                            ["depends_on"].remove(
                                "cue_tip_touch_placement_classification"))

    def test_rejects_source_cpu_or_exit_binding_drift(self) -> None:
        bindings = "wpa_ii_08_09_implementation_bindings"
        self.assertRejected(lambda value: value[bindings]["bound_files"][0]
                            .__setitem__("sha256", "0" * 64))
        self.assertRejected(lambda value: value[bindings]["cpu_results"][1]
                            .__setitem__("sha256", "0" * 64))
        self.assertRejected(lambda value: value[bindings]["cpu_results"][2]
                            .__setitem__("exit_code", 1))

    def test_rejects_cmake_gpu_readiness_or_authorization_claim(self) -> None:
        bindings = "wpa_ii_08_09_implementation_bindings"
        for key in ("cmake_changed_by_increment", "gpu_cocoa_or_opengl_executed",
                    "readiness_generated", "authorization_issued"):
            self.assertRejected(lambda value, key=key:
                                value[bindings].__setitem__(key, True))

    def test_rejects_evidence_loss_or_unsafe_path(self) -> None:
        self.assertRejected(lambda value: self.row(value, "WPA-II-08")
                            ["evidence"].pop())
        self.assertRejected(lambda value: self.row(value, "WPA-II-09")
                            ["evidence"][0].__setitem__("path", "../../fake.cpp"))

    def test_rejects_non_object_bad_input(self) -> None:
        self.assertTrue(MODULE.validate(None, check_files=False))
        self.assertTrue(MODULE.validate([], check_files=False))
        self.assertTrue(MODULE.validate("audit", check_files=False))


if __name__ == "__main__":
    unittest.main()
