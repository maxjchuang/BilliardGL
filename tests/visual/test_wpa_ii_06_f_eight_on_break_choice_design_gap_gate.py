from __future__ import annotations

import copy
import importlib.util
import json
import sys
import unittest
from pathlib import Path


ROOT = Path(__file__).resolve().parents[2]
VALIDATOR_PATH = ROOT / "scripts/validate_wpa_ii_06_f_eight_on_break_choice_design_gap_gate.py"
GATE_PATH = ROOT / "docs/rules/wpa-ii-06-f-eight-on-break-choice-design-gap-gate-v1.json"

SPEC = importlib.util.spec_from_file_location("wpa_ii_06_f_validator", VALIDATOR_PATH)
assert SPEC and SPEC.loader
MODULE = importlib.util.module_from_spec(SPEC)
sys.modules[SPEC.name] = MODULE
SPEC.loader.exec_module(MODULE)


class WpaIi06FEightOnBreakChoiceDesignGapGateTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls) -> None:
        cls.value = json.loads(GATE_PATH.read_text(encoding="utf-8"))

    def assertRejected(self, mutate) -> None:
        value = copy.deepcopy(self.value)
        mutate(value)
        self.assertTrue(MODULE.validate(value, check_files=False))

    def test_canonical_gate_passes_with_live_bindings(self) -> None:
        self.assertEqual([], MODULE.validate(self.value, check_files=True))

    def test_contract_digest_is_static_and_recomputed(self) -> None:
        self.assertEqual(
            MODULE.EXPECTED_CONTRACT_SHA,
            MODULE.canonical_sha(self.value["frozen_contract"]),
        )
        self.assertEqual(
            MODULE.EXPECTED_CONTRACT_SHA,
            self.value["frozen_contract_sha256"],
        )

    def test_rejects_schema_status_or_scope_drift(self) -> None:
        self.assertRejected(lambda value: value.__setitem__("schema", "v2"))
        self.assertRejected(lambda value: value.__setitem__("status", "READY"))
        self.assertRejected(lambda value: value["scope"].append("WPA-II-06-E"))

    def test_rejects_legal_actor_or_action_drift(self) -> None:
        self.assertRejected(lambda value: value["frozen_contract"]
                            ["official_interpretation"]
                            ["resolved_actor_and_action_semantics"]
                            .__setitem__("legal_branch_actor", "opponent"))
        self.assertRejected(lambda value: value["frozen_contract"]
                            ["as_built_parent_routing"]["legal_branch"]
                            ["offered_actions"].reverse())

    def test_rejects_foul_actor_or_action_drift(self) -> None:
        self.assertRejected(lambda value: value["frozen_contract"]
                            ["official_interpretation"]
                            ["resolved_actor_and_action_semantics"]
                            .__setitem__("foul_branch_actor", "original breaker"))
        self.assertRejected(lambda value: value["frozen_contract"]
                            ["as_built_parent_routing"]["foul_branch"]
                            ["offered_actions"].pop())

    def test_rejects_opponent_name_reference_drift(self) -> None:
        self.assertRejected(lambda value: value["frozen_contract"]
                            ["exact_action_semantics"]
                            ["rules.eight_on_break.rerack_opponent_breaks"]
                            .__setitem__("selected_breaker", "original breaker"))

    def test_rejects_early_bih_defect_concealment(self) -> None:
        self.assertRejected(lambda value: value["frozen_contract"]
                            ["observed_authority_gap"]["current_precommit_state"]
                            .__setitem__("match_state_ball_in_hand", False))
        self.assertRejected(lambda value: value["frozen_contract"]
                            ["observed_authority_gap"]
                            .__setitem__("audit_or_product_promotion_blocked_until_corrected_and_tested", False))

    def test_rejects_precommit_target_entitlement(self) -> None:
        self.assertRejected(lambda value: value["frozen_contract"]
                            ["observed_authority_gap"]["required_corrected_precommit_state"]
                            .__setitem__("ball_in_hand_placement_scope", "BehindHeadString"))

    def test_rejects_foul_classification_promotion(self) -> None:
        self.assertRejected(lambda value: value["frozen_contract"]
                            ["classification_and_sibling_boundary"]
                            ["legal_vs_foul_branch"]
                            .__setitem__("status", "COMPLETE"))
        self.assertRejected(lambda value: value["frozen_contract"]
                            ["classification_and_sibling_boundary"]
                            ["legal_vs_foul_branch"]
                            .__setitem__("product_may_reclassify_raw_facts", True))

    def test_rejects_core_f_core_h_coexistence(self) -> None:
        self.assertRejected(lambda value: value["frozen_contract"]
                            ["classification_and_sibling_boundary"]
                            .__setitem__("simultaneous_core_f_and_core_h_parent_allowed", True))

    def test_rejects_parent_child_transaction_collapse(self) -> None:
        self.assertRejected(lambda value: value["frozen_contract"]
                            ["exact_action_semantics"]
                            ["rules.eight_on_break.spot_ball_in_hand_head_string"]
                            .__setitem__("physical_cue_ball_placement_occurs_in_parent", True))

    def test_rejects_authority_identity_or_hash_loss(self) -> None:
        self.assertRejected(lambda value: value["frozen_contract"]
                            ["authority_integrity_contract"]["request_requires"]
                            .remove("unchanged_authority_state_hash"))
        self.assertRejected(lambda value: value["frozen_contract"]
                            ["authority_integrity_contract"]
                            .__setitem__("foundation_action_direct_submission_allowed", True))

    def test_rejects_partial_commit_or_rollback_weakening(self) -> None:
        self.assertRejected(lambda value: value["frozen_contract"]
                            ["transaction_and_full_rollback_contract"]
                            .__setitem__("partial_authority_commit_allowed", True))
        self.assertRejected(lambda value: value["frozen_contract"]
                            ["transaction_and_full_rollback_contract"]
                            .__setitem__("pre_ack_failure", "best effort"))

    def test_rejects_false_full_product_reachability(self) -> None:
        self.assertRejected(lambda value: value["frozen_contract"]
                            ["as_built_product_reachability"]
                            .__setitem__("either_rerack_action_product_reachable", True))
        self.assertRejected(lambda value: value["frozen_contract"]
                            ["as_built_product_reachability"]
                            .__setitem__("rules_decision_ui_product_exposed", True))

    def test_rejects_broadened_future_product_seam(self) -> None:
        self.assertRejected(lambda value: value["frozen_contract"]
                            ["future_minimal_closure"]
                            .__setitem__("foundation_actions_remain_product_unreachable", False))
        self.assertRejected(lambda value: value["frozen_contract"]
                            ["future_minimal_closure"]["preconditions"].pop())

    def test_rejects_missing_positive_or_negative_fixture(self) -> None:
        self.assertRejected(lambda value: value["frozen_contract"]
                            ["required_future_fixtures"]["positive"].pop())
        self.assertRejected(lambda value: value["frozen_contract"]
                            ["required_future_fixtures"]["negative"].pop())

    def test_rejects_allowlist_expansion_or_cmake_scope(self) -> None:
        self.assertRejected(lambda value: value["frozen_contract"]
                            ["future_implementation_allowlist"]["production_files"]
                            .append("src/Billiards/render/modern/modern_renderer.cpp"))
        self.assertRejected(lambda value: value["frozen_contract"]
                            ["future_implementation_allowlist"]
                            .__setitem__("core_f_header_or_core_e_changes_allowed", True))
        self.assertRejected(lambda value: value["frozen_contract"]
                            ["forbidden_scope"].remove("CMake changes"))

    def test_rejects_audit_gpu_or_behavior_claim_promotion(self) -> None:
        self.assertRejected(lambda value: value["frozen_contract"]
                            ["claims"].__setitem__("audit_promoted", True))
        self.assertRejected(lambda value: value["frozen_contract"]
                            ["claims"].__setitem__("gpu_executed", True))
        self.assertRejected(lambda value: value["frozen_contract"]
                            ["claims"].__setitem__("product_behavior_changed", True))

    def test_rejects_source_sha_or_path_traversal(self) -> None:
        self.assertRejected(lambda value: value["source_bindings"]
                            ["core_f_parent"].__setitem__("sha256", "0" * 64))
        self.assertRejected(lambda value: value["source_bindings"]
                            ["official_pdf"].__setitem__("path", "../../rules.pdf"))

    def test_rejects_extra_top_level_or_missing_source(self) -> None:
        self.assertRejected(lambda value: value.__setitem__("authorized", True))
        self.assertRejected(lambda value: value["source_bindings"].pop("runtime"))

    def test_rejects_non_object_bad_input(self) -> None:
        self.assertTrue(MODULE.validate(None, check_files=False))
        self.assertTrue(MODULE.validate([], check_files=False))


if __name__ == "__main__":
    unittest.main()
