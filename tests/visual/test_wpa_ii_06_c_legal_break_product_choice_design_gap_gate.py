from __future__ import annotations

import copy
import json
from pathlib import Path
import unittest

from scripts.validate_wpa_ii_06_c_legal_break_product_choice_design_gap_gate import validate


ROOT = Path(__file__).resolve().parents[2]
GATE = ROOT / "docs/rules/wpa-ii-06-c-legal-break-product-choice-design-gap-gate-v1.json"


class LegalBreakProductChoiceDesignGapGateTests(unittest.TestCase):
    def setUp(self) -> None:
        self.value = json.loads(GATE.read_text(encoding="utf-8"))

    def assertRejected(self, mutate) -> None:  # noqa: N802
        bad = copy.deepcopy(self.value)
        mutate(bad)
        self.assertTrue(validate(bad, check_files=False))

    def test_frozen_gate_and_live_bindings_pass(self) -> None:
        self.assertEqual([], validate(self.value))

    def test_rejects_four_ball_threshold_drift(self) -> None:
        self.assertRejected(lambda value: value["as_built_rule_core"]
                            .__setitem__("minimum_distinct_object_balls_to_rail", 3))

    def test_rejects_and_instead_of_or_legal_threshold(self) -> None:
        self.assertRejected(lambda value: value["official_source"]["resolved_semantics"]
                            .__setitem__("legal_break_threshold", "rail_branch AND pocket_branch"))

    def test_rejects_pocket_jaw_policy_guess(self) -> None:
        def mutate(value):
            policy = value["pending_classification"]["cushion_surface_policy"]
            policy["selected"] = "straight_cushion_and_pocket_jaw"
            policy["status"] = "RESOLVED"
        self.assertRejected(mutate)

    def test_rejects_solver_contact_predicate_guess(self) -> None:
        def mutate(value):
            policy = value["pending_classification"]["physics_contact_validity_policy"]
            policy["selected"] = "velocity_impulse_applied"
            policy["status"] = "RESOLVED"
        self.assertRejected(mutate)

    def test_rejects_pocketed_ball_scope_guess(self) -> None:
        def mutate(value):
            policy = value["pending_classification"]["pocketed_ball_scope_policy"]
            policy["selected"] = "object_ball_only"
            policy["status"] = "RESOLVED"
        self.assertRejected(mutate)

    def test_rejects_false_telemetry_completeness(self) -> None:
        self.assertRejected(lambda value: value["as_built_telemetry"]
                            .__setitem__("telemetry_sufficient_for_product_four_cushion_claim", True))
        self.assertRejected(lambda value: value["claims"]
                            .__setitem__("authoritative_cushion_telemetry_complete", True))

    def test_rejects_missing_authoritative_provenance(self) -> None:
        self.assertRejected(lambda value: value["missing_authoritative_fields"]
                            .__setitem__("required_origin_field", ""))
        self.assertRejected(lambda value: value["missing_authoritative_fields"]
                            .__setitem__("synthetic_or_direct_fact_origin_may_open_product_choice", True))

    def test_rejects_lost_pocket_capture_mapping(self) -> None:
        self.assertRejected(lambda value: value["as_built_telemetry"]
                            .__setitem__("pocket_evidence_sufficient_for_product_object_pocket_branch", False))

    def test_rejects_ball_in_hand_on_illegal_break(self) -> None:
        self.assertRejected(lambda value: value["as_built_parent_authority"]
                            .__setitem__("ball_in_hand_action_offered", True))
        self.assertRejected(lambda value: value["official_source"]["resolved_semantics"]
                            .__setitem__("ball_in_hand_awarded", True))

    def test_rejects_parent_action_set_drift(self) -> None:
        self.assertRejected(lambda value: value["as_built_parent_authority"]
                            ["offered_actions"].pop())

    def test_rejects_parent_idempotency_or_state_hash_loss(self) -> None:
        self.assertRejected(lambda value: value["as_built_parent_authority"]
                            .__setitem__("exact_retry_returns_original_ack_without_mutation", False))
        self.assertRejected(lambda value: value["as_built_parent_authority"]
                            .__setitem__("request_requires_frozen_authority_state_hash", False))

    def test_rejects_product_exposure_or_false_reachability(self) -> None:
        self.assertRejected(lambda value: value["as_built_product_reachability"]
                            .__setitem__("rules_decision_ui_product_exposed", True))
        self.assertRejected(lambda value: value["as_built_product_reachability"]
                            .__setitem__("product_reachable", True))
        self.assertRejected(lambda value: value["claims"]
                            .__setitem__("product_reachable", True))

    def test_rejects_unblocked_future_product_seam(self) -> None:
        self.assertRejected(lambda value: value["future_minimal_parent_product_contract"]
                            .__setitem__("blocked_until_telemetry_and_policy_close", False))
        self.assertRejected(lambda value: value["pending_classification"]
                            .__setitem__("product_choice_exposure_authorized", True))

    def test_rejects_product_contract_precondition_loss(self) -> None:
        self.assertRejected(lambda value: value["future_minimal_parent_product_contract"]
                            ["preconditions"].pop())
        self.assertRejected(lambda value: value["future_minimal_parent_product_contract"]
                            ["request_bindings"].pop())

    def test_rejects_same_length_product_contract_substitution(self) -> None:
        self.assertRejected(lambda value: value["future_minimal_parent_product_contract"]
                            ["preconditions"].__setitem__(
                                5, "synthetic scalar is sufficient"))
        self.assertRejected(lambda value: value["future_minimal_parent_product_contract"]
                            ["request_bindings"].__setitem__(
                                4, "unbound_revision"))

    def test_rejects_partial_commit_or_rollback_loss(self) -> None:
        self.assertRejected(lambda value: value["atomic_mutation_contract"]
                            .__setitem__("partial_commit_allowed", True))
        self.assertRejected(lambda value: value["atomic_mutation_contract"]
                            .__setitem__("rollback", "best effort"))

    def test_rejects_missing_positive_or_negative_fixture(self) -> None:
        self.assertRejected(lambda value: value["required_future_fixtures"]["positive"].pop())
        self.assertRejected(lambda value: value["required_future_fixtures"]["negative"].pop())

    def test_rejects_source_sha_or_path_traversal(self) -> None:
        self.assertRejected(lambda value: value["source_bindings"]["shot_collector"]
                            .__setitem__("sha256", "0" * 64))
        self.assertRejected(lambda value: value["source_bindings"]["shot_collector"]
                            .__setitem__("path", "../shot_fact_collector.cpp"))
        self.assertRejected(lambda value: value["official_source"]
                            .__setitem__("path", "../../250816-Rules-of-Heyball.pdf"))

    def test_rejects_physics_renderer_or_cmake_scope_expansion(self) -> None:
        self.assertRejected(lambda value: value["future_implementation_allowlist"]
                            .__setitem__("physics_cpp_change_allowed", True))
        self.assertRejected(lambda value: value["future_implementation_allowlist"]
                            ["production_files"].append(
                                "src/Billiards/render/modern/modern_renderer.cpp"))
        self.assertRejected(lambda value: value["forbidden_scope"].remove("CMake changes"))

    def test_rejects_gpu_readiness_or_audit_promotion(self) -> None:
        self.assertRejected(lambda value: value["forbidden_scope"].remove(
            "GPU, Cocoa, or OpenGL execution"))
        self.assertRejected(lambda value: value["forbidden_scope"].remove(
            "readiness generation"))
        self.assertRejected(lambda value: value["pending_classification"]
                            .__setitem__("audit_row_may_be_promoted", True))

    def test_rejects_non_object_bad_input(self) -> None:
        self.assertTrue(validate(None, check_files=False))
        self.assertTrue(validate([], check_files=False))


if __name__ == "__main__":
    unittest.main()
