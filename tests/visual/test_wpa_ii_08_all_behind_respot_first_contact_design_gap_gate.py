from __future__ import annotations

import copy
import json
from pathlib import Path
import unittest

from scripts.validate_wpa_ii_08_all_behind_respot_first_contact_design_gap_gate import validate


ROOT = Path(__file__).resolve().parents[2]
GATE = ROOT / "docs/rules/wpa-ii-08-all-behind-respot-first-contact-design-gap-gate-v1.json"


class WpaIi08RespotFirstContactDesignGateTests(unittest.TestCase):
    def setUp(self) -> None:
        self.value = json.loads(GATE.read_text(encoding="utf-8"))

    def assertRejected(self, mutate) -> None:
        value = copy.deepcopy(self.value)
        mutate(value)
        self.assertTrue(validate(value, check_files=False))

    def test_canonical_gate_and_live_bindings_pass(self) -> None:
        self.assertEqual([], validate(self.value, check_files=True))

    def test_rejects_identity_status_or_scope_promotion(self) -> None:
        self.assertRejected(lambda value: value.__setitem__("status", "IMPLEMENTATION_APPROVED"))
        self.assertRejected(lambda value: value.__setitem__("scope", ["WPA-II-08", "WPA-II-09"]))

    def test_rejects_pdf_path_sha_or_visual_page_drift(self) -> None:
        self.assertRejected(lambda value: value["official_source"]
                            .__setitem__("path", "../../rules.pdf"))
        self.assertRejected(lambda value: value["official_source"]
                            .__setitem__("sha256", "0" * 64))
        self.assertRejected(lambda value: value["official_source"]
                            ["visually_reviewed_pdf_pages"].append(9))

    def test_rejects_candidate_or_tie_rule_loss(self) -> None:
        self.assertRejected(lambda value: value["official_source"]
                            ["text_uniquely_determines"].remove(
                                "the candidate is the legal object ball with the smallest center-to-line distance"))
        self.assertRejected(lambda value: value["official_source"]
                            ["text_uniquely_determines"].remove(
                                "the entitled player chooses only when two or more eligible candidates are exactly equidistant"))

    def test_rejects_crossing_ambiguity_erasure(self) -> None:
        self.assertRejected(lambda value: value["official_source"]
                            ["text_does_not_specify"].pop())

    def test_rejects_audit_promotion_or_subflow_demotion(self) -> None:
        self.assertRejected(lambda value: value["frozen_audit"]
                            .__setitem__("row_status", "IMPLEMENTED"))
        self.assertRejected(lambda value: value["frozen_audit"]
                            .__setitem__("product_placement_subflow_reachable", False))

    def test_rejects_source_sha_or_inventory_drift(self) -> None:
        self.assertRejected(lambda value: value["source_bindings"]["runtime"]
                            .__setitem__("sha256", "0" * 64))
        self.assertRejected(lambda value: value["source_bindings"].pop("shot_collector"))

    def test_rejects_non_current_offer_or_wrong_actor_contract(self) -> None:
        self.assertRejected(lambda value: value["resolved_respot_contract"]
                            .__setitem__("authority_context", "any placement offer"))
        self.assertRejected(lambda value: value["resolved_respot_contract"]
                            .__setitem__("actor", "either player"))

    def test_rejects_preview_as_sample_or_missing_commit_revalidation(self) -> None:
        self.assertRejected(lambda value: value["resolved_respot_contract"]
                            ["sample_time"].__setitem__("selected", "preview"))
        self.assertRejected(lambda value: value["resolved_respot_contract"]
                            .__setitem__("preview_mutates_authority", True))

    def test_rejects_vacuous_or_on_line_all_behind_predicate(self) -> None:
        self.assertRejected(lambda value: value["resolved_respot_contract"]
                            .__setitem__("trigger", "every eligible ball is behind"))
        self.assertRejected(lambda value: value["resolved_respot_contract"]
                            .__setitem__("behind_predicate",
                                         "object world_z_cm <= head string"))

    def test_rejects_legal_object_predicate_divergence(self) -> None:
        self.assertRejected(lambda value: value["resolved_respot_contract"]
                            ["legal_object_predicate"]
                            .__setitem__("must_match_existing_first_contact_predicate", False))

    def test_rejects_crossing_geometry_policy_guess(self) -> None:
        def mutate(value):
            policy = value["first_contact_classification"]["policy_decisions"]
            policy["cue_ball_past_line_geometry"]["selected"] = "center_crosses_line"
            policy["cue_ball_past_line_geometry"]["status"] = "RESOLVED"
        self.assertRejected(mutate)

    def test_rejects_crossing_numeric_policy_guess(self) -> None:
        self.assertRejected(lambda value: value["first_contact_classification"]
                            ["policy_decisions"]["crossing_numeric_policy"]
                            .__setitem__("selected", "exact_continuous_event"))

    def test_rejects_missing_crossing_provenance_or_completeness(self) -> None:
        self.assertRejected(lambda value: value["first_contact_classification"]
                            ["missing_shot_facts"].remove(
                                "crossing_evidence_origin_and_completeness"))
        self.assertRejected(lambda value: value["first_contact_classification"]
                            .__setitem__("production_implementation_authorized", True))

    def test_rejects_foundation_or_global_ui_exposure(self) -> None:
        transaction = "future_parent_child_transaction"
        self.assertRejected(lambda value: value[transaction]
                            .__setitem__("foundation_action_product_exposed", True))
        self.assertRejected(lambda value: value[transaction]
                            .__setitem__("global_rules_decision_ui_product_exposed", True))

    def test_rejects_action_scope_expansion(self) -> None:
        self.assertRejected(lambda value: value["future_parent_child_transaction"]
                            ["allowed_product_actions"].append("rules.foundation.respot"))

    def test_rejects_atomicity_retry_conflict_or_rollback_weakening(self) -> None:
        transaction = "future_parent_child_transaction"
        self.assertRejected(lambda value: value[transaction]
                            .__setitem__("partial_commit_allowed", True))
        self.assertRejected(lambda value: value[transaction]
                            .__setitem__("idempotent_retry", "commit again"))
        self.assertRejected(lambda value: value[transaction]
                            .__setitem__("conflicting_retry", "best effort"))
        self.assertRejected(lambda value: value[transaction]
                            .__setitem__("failure_rollback", "partial rollback"))

    def test_rejects_missing_future_positive_negative_or_contact_fixture(self) -> None:
        self.assertRejected(lambda value: value["required_future_fixtures"]
                            ["respot_positive"].pop())
        self.assertRejected(lambda value: value["required_future_fixtures"]
                            ["respot_negative"].pop())
        self.assertRejected(lambda value: value["required_future_fixtures"]
                            ["first_contact_after_policy"].pop())

    def test_rejects_active_allowlist_or_renderer_cmake_expansion(self) -> None:
        self.assertRejected(lambda value: value["future_implementation_allowlist"]
                            .__setitem__("allowlist_is_dormant_until_crossing_policy_resolved", False))
        self.assertRejected(lambda value: value["future_implementation_allowlist"]
                            ["production_files"].append(
                                "src/Billiards/render/modern/modern_renderer.cpp"))
        self.assertRejected(lambda value: value["future_implementation_allowlist"]
                            ["production_files"].append("CMakeLists.txt"))

    def test_rejects_gpu_cmake_section9_or_audit_scope_loss(self) -> None:
        for item in ("GPU, Cocoa, or OpenGL execution", "CMake changes",
                     "changes to WPA-II-09 tip-touch or until-legal-shot semantics",
                     "promotion of WPA-II-08 above PARTIAL"):
            self.assertRejected(lambda value, item=item:
                                value["forbidden_scope"].remove(item))

    def test_rejects_false_completion_claims(self) -> None:
        self.assertRejected(lambda value: value["claims"]
                            .__setitem__("first_contact_crossing_policy_resolved", True))
        self.assertRejected(lambda value: value["claims"]
                            .__setitem__("production_implementation_complete", True))
        self.assertRejected(lambda value: value["claims"]
                            .__setitem__("audit_row_promoted", True))

    def test_rejects_non_object_bad_input(self) -> None:
        self.assertTrue(validate(None, check_files=False))
        self.assertTrue(validate([], check_files=False))
        self.assertTrue(validate("gate", check_files=False))


if __name__ == "__main__":
    unittest.main()
