from __future__ import annotations

import copy
import json
from pathlib import Path
import unittest

from scripts.validate_wpa_ii_17_general_foul_product_lifecycle_subflow_audit_successor import validate


ROOT = Path(__file__).resolve().parents[2]
AUDIT = ROOT / "docs/rules/wpa-ii-17-general-foul-product-lifecycle-subflow-audit-successor-v1.json"


class WpaIi17GeneralFoulProductLifecycleSubflowAuditTests(unittest.TestCase):
    def setUp(self) -> None:
        self.value = json.loads(AUDIT.read_text(encoding="utf-8"))

    def assertRejected(self, mutate) -> None:
        value = copy.deepcopy(self.value)
        mutate(value)
        self.assertTrue(validate(value, check_files=False))

    def test_canonical_audit_and_live_bindings_pass(self) -> None:
        self.assertEqual([], validate(self.value, check_files=True))

    def test_rejects_identity_status_or_scope_drift(self) -> None:
        self.assertRejected(lambda v: v.__setitem__("status", "IMPLEMENTED"))
        self.assertRejected(lambda v: v.__setitem__("scope", ["WPA-II-17"]))

    def test_rejects_pdf_or_clause_binding_drift(self) -> None:
        self.assertRejected(lambda v: v["official_source"].__setitem__("sha256", "0" * 64))
        self.assertRejected(lambda v: v["official_source"]["section_17"].__setitem__("pdf_page", 10))
        self.assertRejected(lambda v: v["official_source"]["section_12_3"].__setitem__("clause", "12.4"))

    def test_rejects_object_off_table_policy_guess(self) -> None:
        self.assertRejected(lambda v: v["official_source"]["policy_conflict"].__setitem__("machine_resolution", "17e_wins"))
        self.assertRejected(lambda v: v["official_source"]["policy_conflict"].__setitem__("status", "RESOLVED"))
        self.assertRejected(lambda v: v["claims"].__setitem__("object_ball_off_table_policy_resolved", True))

    def test_rejects_frozen_audit_promotion(self) -> None:
        self.assertRejected(lambda v: v["frozen_audit"]["rows"][1].__setitem__("status", "IMPLEMENTED"))
        self.assertRejected(lambda v: v["frozen_audit"].__setitem__("row_promotion_authorized", True))

    def test_rejects_source_binding_drift_or_loss(self) -> None:
        self.assertRejected(lambda v: v["source_bindings"]["runtime"].__setitem__("sha256", "0" * 64))
        self.assertRejected(lambda v: v["source_bindings"].pop("product_session"))

    def test_rejects_schema_as_physical_scratch_claim(self) -> None:
        self.assertRejected(lambda v: v["current_fact_authority"]["cue_ball_scratch_gap"].__setitem__("production_physics_sets_cue_ball_pocketed_state", True))
        self.assertRejected(lambda v: v["current_fact_authority"]["cue_ball_scratch_gap"].__setitem__("physical_authoritative_collection_complete", True))
        self.assertRejected(lambda v: v["claims"].__setitem__("cue_ball_scratch_physical_collection_complete", True))

    def test_rejects_general_origin_or_injected_fact_overclaim(self) -> None:
        self.assertRejected(lambda v: v["current_fact_authority"].__setitem__("general_foul_origin_and_completeness_attestation_exists", True))
        self.assertRejected(lambda v: v["current_fact_authority"].__setitem__("direct_or_automation_injected_shot_facts_are_not_physical_telemetry_proof", False))

    def test_rejects_clause_inventory_or_order_drift(self) -> None:
        self.assertRejected(lambda v: v["clause_matrix"].pop())
        self.assertRejected(lambda v: v["clause_matrix"].reverse())

    def test_rejects_false_moving_ball_or_off_table_classification(self) -> None:
        self.assertRejected(lambda v: v["clause_matrix"][2].__setitem__("classification", "IMPLEMENTED"))
        self.assertRejected(lambda v: v["clause_matrix"][4].__setitem__("classification", "OFFICIALLY_RESOLVED_FOUL"))

    def test_rejects_double_hit_push_or_bad_head_string_overclaim(self) -> None:
        self.assertRejected(lambda v: v["clause_matrix"][6].__setitem__("classification", "IMPLEMENTED"))
        self.assertRejected(lambda v: v["clause_matrix"][7].__setitem__("classification", "IMPLEMENTED"))
        self.assertRejected(lambda v: v["clause_matrix"][8].__setitem__("telemetry", "COMPLETE"))

    def test_rejects_mobile_repeat_lifecycle_erasure(self) -> None:
        self.assertRejected(lambda v: v["clause_matrix"][11].__setitem__("pending", "NONE"))

    def test_rejects_product_lifecycle_contract_drift(self) -> None:
        self.assertRejected(lambda v: v["classified_foul_bih_product_subflow"].__setitem__("presentation_reads_current_match_state_ball_in_hand", False))
        self.assertRejected(lambda v: v["classified_foul_bih_product_subflow"].__setitem__("last_adjudication_ball_in_hand_is_historical_verdict_not_live_entitlement", False))
        self.assertRejected(lambda v: v["classified_foul_bih_product_subflow"].__setitem__("production_fix_required", True))

    def test_rejects_global_ui_exposure(self) -> None:
        self.assertRejected(lambda v: v["classified_foul_bih_product_subflow"].__setitem__("global_rules_decision_ui_product_exposed", True))
        self.assertRejected(lambda v: v["claims"].__setitem__("global_rules_decision_ui_product_exposed", True))

    def test_rejects_non_atomic_future_contract(self) -> None:
        self.assertRejected(lambda v: v["future_closure_contract"]["atomic_classification"].pop())
        self.assertRejected(lambda v: v["future_closure_contract"].__setitem__("partial_commit_allowed", True))

    def test_rejects_complete_section_or_production_claim(self) -> None:
        for key in ("full_wpa_ii_17_telemetry_complete", "full_wpa_ii_17_classification_complete",
                    "all_listed_fouls_product_reachable", "production_source_changed",
                    "audit_row_promoted", "gpu_executed"):
            self.assertRejected(lambda v, key=key: v["claims"].__setitem__(key, True))

    def test_rejects_forbidden_scope_loss(self) -> None:
        for item in ("promoting WPA-II-17 above PARTIAL",
                     "treating ShotFacts schema presence or automation injection as physical telemetry",
                     "resolving WPA-II-12-3 versus WPA-II-17(e) without an approved policy ruling",
                     "enabling global rulesDecisionUiProductExposed",
                     "production or CMake changes",
                     "GPU, Cocoa, OpenGL, readiness, or authorization execution"):
            self.assertRejected(lambda v, item=item: v["forbidden_scope"].remove(item))

    def test_rejects_non_object_bad_input(self) -> None:
        for value in (None, [], "audit", 1):
            self.assertTrue(validate(value, check_files=False))


if __name__ == "__main__":
    unittest.main()
