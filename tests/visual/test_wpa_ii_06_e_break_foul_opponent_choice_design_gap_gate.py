from __future__ import annotations

import copy
import json
from pathlib import Path
import unittest

from scripts.validate_wpa_ii_06_e_break_foul_opponent_choice_design_gap_gate import validate


ROOT = Path(__file__).resolve().parents[2]
GATE = ROOT / "docs/rules/wpa-ii-06-e-break-foul-opponent-choice-design-gap-gate-v1.json"


class BreakFoulOpponentChoiceDesignGapGateTests(unittest.TestCase):
    def setUp(self) -> None:
        self.value = json.loads(GATE.read_text(encoding="utf-8"))

    def assertRejected(self, mutate) -> None:  # noqa: N802
        bad = copy.deepcopy(self.value)
        mutate(bad)
        self.assertTrue(validate(bad, check_files=False))

    def test_frozen_gate_and_live_bindings_pass(self) -> None:
        self.assertEqual([], validate(self.value))

    def test_rejects_actor_or_breaker_assignment_drift(self) -> None:
        self.assertRejected(lambda value: value["as_built_parent_offer"]
                            .__setitem__("actor_player_id", "original_breaker"))
        self.assertRejected(lambda value: value["classification_and_policy"]
                            ["rerack_breaker_assignment_policy"]
                            .__setitem__("self_break", "original breaker"))

    def test_rejects_parent_action_set_drift(self) -> None:
        self.assertRejected(lambda value: value["as_built_parent_offer"]["offered_actions"].pop())
        self.assertRejected(lambda value: value["as_built_parent_offer"]
                            .__setitem__("accept_table_action_offered", True))

    def test_rejects_early_ball_in_hand_or_child(self) -> None:
        self.assertRejected(lambda value: value["as_built_parent_offer"]
                            .__setitem__("ball_in_hand_before_parent_commit", True))
        self.assertRejected(lambda value: value["as_built_parent_offer"]
                            .__setitem__("placement_child_before_parent_commit", True))

    def test_rejects_physical_placement_in_parent(self) -> None:
        self.assertRejected(lambda value: value["exact_action_semantics"]
                            ["rules.break_foul.ball_in_hand_behind_head_string"]
                            .__setitem__("physical_cue_ball_placement_occurs_in_parent", True))

    def test_rejects_child_actor_or_flow_drift(self) -> None:
        self.assertRejected(lambda value: value["exact_action_semantics"]
                            ["rules.break_foul.ball_in_hand_behind_head_string"]
                            .__setitem__("child_actor", "original breaker"))
        self.assertRejected(lambda value: value["exact_action_semantics"]
                            ["rules.break_foul.ball_in_hand_behind_head_string"]
                            .__setitem__("child_flow", "ball_in_hand_full_table"))

    def test_rejects_rerack_breaker_swap_or_bih(self) -> None:
        self.assertRejected(lambda value: value["exact_action_semantics"]
                            ["rules.break_foul.rerack_opponent_breaks"]
                            .__setitem__("selected_breaker", "the frozen original breaker"))
        self.assertRejected(lambda value: value["exact_action_semantics"]
                            ["rules.break_foul.rerack_original_breaker"]
                            .__setitem__("ball_in_hand", True))

    def test_rejects_raw_product_routing_or_core_f_merge(self) -> None:
        self.assertRejected(lambda value: value["sibling_and_exception_routing"]
                            .__setitem__("raw_product_input_may_select_route", True))
        self.assertRejected(lambda value: value["sibling_and_exception_routing"]
                            .__setitem__("eight_ball_pocketed_on_break", "use WPA-II-06-E"))

    def test_rejects_false_policy_or_classification_promotion(self) -> None:
        self.assertRejected(lambda value: value["classification_and_policy"]
                            ["actor_policy"].__setitem__("status", "POLICY_PENDING"))
        self.assertRejected(lambda value: value["classification_and_policy"]
                            ["break_shot_foul_classification_coverage"]
                            .__setitem__("status", "COMPLETE"))
        self.assertRejected(lambda value: value["claims"]
                            .__setitem__("upstream_break_foul_classification_complete", True))

    def test_rejects_product_side_reclassification(self) -> None:
        self.assertRejected(lambda value: value["classification_and_policy"]
                            ["break_shot_foul_classification_coverage"]
                            .__setitem__("product_may_reclassify_raw_facts", True))

    def test_rejects_authority_identity_or_hash_loss(self) -> None:
        self.assertRejected(lambda value: value["authority_integrity_contract"]
                            ["request_requires"].remove("unchanged_authority_state_hash"))
        self.assertRejected(lambda value: value["authority_integrity_contract"]
                            .__setitem__("foundation_action_direct_submission_allowed", True))

    def test_rejects_partial_commit_or_rollback_loss(self) -> None:
        self.assertRejected(lambda value: value["transaction_and_rollback_contract"]
                            .__setitem__("partial_authority_commit_allowed", True))
        self.assertRejected(lambda value: value["transaction_and_rollback_contract"]
                            .__setitem__("pre_ack_failure", "best effort"))
        self.assertRejected(lambda value: value["transaction_and_rollback_contract"]
                            .__setitem__("post_ack_refresh_boundary", "reverse runtime ACK"))

    def test_rejects_product_exposure_or_false_reachability(self) -> None:
        self.assertRejected(lambda value: value["as_built_product_reachability"]
                            .__setitem__("rules_decision_ui_product_exposed", True))
        self.assertRejected(lambda value: value["as_built_product_reachability"]
                            .__setitem__("product_reachable", True))
        self.assertRejected(lambda value: value["claims"].__setitem__("product_reachable", True))

    def test_rejects_product_seam_premature_authorization(self) -> None:
        self.assertRejected(lambda value: value["classification_and_policy"]
                            .__setitem__("product_parent_seam_authorized", True))
        self.assertRejected(lambda value: value["as_built_product_reachability"]
                            .__setitem__("wpa_ii_06_e_narrow_product_parent_seam_exists", True))

    def test_rejects_future_seam_precondition_substitution(self) -> None:
        self.assertRejected(lambda value: value["future_minimal_product_parent_seam"]
                            ["preconditions"].__setitem__(
                                2, "offer branch may be IllegalBreak"))
        self.assertRejected(lambda value: value["future_minimal_product_parent_seam"]
                            .__setitem__("core_f_or_core_c_offer_rejected", False))

    def test_rejects_missing_positive_or_negative_fixture(self) -> None:
        self.assertRejected(lambda value: value["required_future_fixtures"]["positive"].pop())
        self.assertRejected(lambda value: value["required_future_fixtures"]["negative"].pop())

    def test_rejects_source_sha_or_path_traversal(self) -> None:
        self.assertRejected(lambda value: value["source_bindings"]["core_h_parent"]
                            .__setitem__("sha256", "0" * 64))
        self.assertRejected(lambda value: value["source_bindings"]["core_h_parent"]
                            .__setitem__("path", "../break_foul_authority.h"))
        self.assertRejected(lambda value: value["official_source"]
                            .__setitem__("path", "../../250816-Rules-of-Heyball.pdf"))

    def test_rejects_runtime_core_or_renderer_allowlist_expansion(self) -> None:
        self.assertRejected(lambda value: value["future_implementation_allowlist"]
                            .__setitem__("runtime_or_rule_changes_allowed", True))
        self.assertRejected(lambda value: value["future_implementation_allowlist"]
                            .__setitem__("core_e_or_core_f_changes_allowed", True))
        self.assertRejected(lambda value: value["future_implementation_allowlist"]
                            ["production_files"].append(
                                "src/Billiards/render/modern/modern_renderer.cpp"))

    def test_rejects_cmake_gpu_readiness_or_audit_promotion(self) -> None:
        self.assertRejected(lambda value: value["forbidden_scope"].remove("CMake changes"))
        self.assertRejected(lambda value: value["forbidden_scope"].remove(
            "GPU, Cocoa, or OpenGL execution"))
        self.assertRejected(lambda value: value["forbidden_scope"].remove("readiness generation"))
        self.assertRejected(lambda value: value["classification_and_policy"]
                            .__setitem__("audit_row_may_be_promoted", True))

    def test_rejects_non_object_bad_input(self) -> None:
        self.assertTrue(validate(None, check_files=False))
        self.assertTrue(validate([], check_files=False))


if __name__ == "__main__":
    unittest.main()
