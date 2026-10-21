from __future__ import annotations

import copy
import json
from pathlib import Path
import unittest

from scripts.validate_wpa_ii_21_stalemate_state_machine_design_gap_gate import validate


ROOT = Path(__file__).resolve().parents[2]
GATE = ROOT / "docs/rules/wpa-ii-21-stalemate-state-machine-design-gap-gate-v1.json"


class WpaIi21StalemateStateMachineDesignGapGateTests(unittest.TestCase):
    def setUp(self) -> None:
        self.value = json.loads(GATE.read_text(encoding="utf-8"))

    def assertRejected(self, mutate) -> None:
        value = copy.deepcopy(self.value)
        mutate(value)
        self.assertTrue(validate(value, check_files=False))

    def test_canonical_gate_and_live_bindings_pass(self) -> None:
        self.assertEqual([], validate(self.value, check_files=True))

    def test_rejects_identity_status_or_scope_drift(self) -> None:
        self.assertRejected(lambda v: v.__setitem__("status", "IMPLEMENTATION_APPROVED"))
        self.assertRejected(lambda v: v.__setitem__("scope", ["WPA-II-20", "WPA-II-21"]))

    def test_rejects_official_pdf_or_section_drift(self) -> None:
        self.assertRejected(lambda v: v["official_source"].__setitem__("sha256", "0" * 64))
        self.assertRejected(lambda v: v["official_source"]["section"].__setitem__("clause", "22"))
        self.assertRejected(lambda v: v["official_source"].__setitem__("visually_reviewed_pdf_pages", [11]))

    def test_rejects_referee_judgment_or_refusal_rule_loss(self) -> None:
        self.assertRejected(lambda v: v["official_source"]["uniquely_determined_rules"].remove(
            "the initial proposal is triggered by referee judgment, not by an automatic shot predicate"))
        self.assertRejected(lambda v: v["official_source"]["uniquely_determined_rules"].remove(
            "a refusal by any player causes play to continue for a referee-controlled period"))

    def test_rejects_original_breaker_or_order_rule_loss(self) -> None:
        self.assertRejected(lambda v: v["official_source"]["uniquely_determined_rules"].remove(
            "the final rerack assigns the break to the original breaking player"))
        self.assertRejected(lambda v: v["official_source"]["uniquely_determined_rules"].remove(
            "the original shooting order remains unchanged"))

    def test_rejects_generally_three_each_policy_guess(self) -> None:
        self.assertRejected(lambda v: v["official_source"].__setitem__(
            "fixed_three_each_machine_policy_authorized", True))
        self.assertRejected(lambda v: v["official_source"]["text_does_not_uniquely_determine"].remove(
            "whether generally three more shots each is a default, recommendation, maximum, or replaceable allocation"))
        self.assertRejected(lambda v: v["pending_policy_contract"]["continuation_window"].__setitem__(
            "fixed_three_shots_each", 3))

    def test_rejects_referee_or_acceptance_policy_guess(self) -> None:
        self.assertRejected(lambda v: v["pending_policy_contract"]["referee_authority"].__setitem__(
            "initial_proposal_issuer", "local_client"))
        self.assertRejected(lambda v: v["pending_policy_contract"]["player_response"].__setitem__(
            "acceptance_quorum_or_timeout", "first_response_wins"))
        self.assertRejected(lambda v: v["pending_policy_contract"]["continuation_window"].__setitem__(
            "stalemate_broken_predicate", "any_pocket"))

    def test_rejects_audit_promotion(self) -> None:
        self.assertRejected(lambda v: v["frozen_audit"]["row"].__setitem__("status", "PARTIAL"))
        self.assertRejected(lambda v: v["frozen_audit"].__setitem__("promotion_authorized", True))

    def test_rejects_pending_ui_exposure_or_policy_resolution(self) -> None:
        self.assertRejected(lambda v: v["existing_pending_ui_lineage"].__setitem__("product_exposed", True))
        self.assertRejected(lambda v: v["existing_pending_ui_lineage"]["unresolved_policy_ids"].pop())
        self.assertRejected(lambda v: v["existing_pending_ui_lineage"].__setitem__(
            "placeholder_is_not_authority_or_implementation", False))

    def test_rejects_source_binding_drift(self) -> None:
        self.assertRejected(lambda v: v["source_bindings"]["runtime"].__setitem__("sha256", "0" * 64))
        self.assertRejected(lambda v: v["source_bindings"].pop("rack_executor"))

    def test_rejects_false_existing_stalemate_authority_claims(self) -> None:
        for key in ("match_state_has_stalemate_phase", "match_state_has_rack_identity",
                    "match_state_persists_original_breaker", "match_state_persists_original_shooting_order",
                    "referee_proposal_input_exists", "referee_final_declaration_input_exists",
                    "player_refusal_authority_exists", "continuation_window_policy_exists",
                    "per_player_stalemate_shot_counters_exist", "stalemate_broken_predicate_exists",
                    "dedicated_stalemate_product_seam_exists"):
            self.assertRejected(lambda v, key=key: v["current_system_mapping"].__setitem__(key, True))

    def test_rejects_foundation_or_replay_as_parent_authority(self) -> None:
        self.assertRejected(lambda v: v["current_system_mapping"].__setitem__(
            "rack_foundation_by_itself_proves_section_21_parent_authority", True))
        self.assertRejected(lambda v: v["current_system_mapping"].__setitem__(
            "replay_global_shot_count_is_rules_authority", True))
        self.assertRejected(lambda v: v["current_system_mapping"].__setitem__(
            "applied_shoot_journal_is_authoritative_per_rack_per_player_stalemate_counter", True))

    def test_rejects_unsafe_production_extension(self) -> None:
        self.assertRejected(lambda v: v["why_implementation_is_blocked"].__setitem__(
            "production_extension_safe_now", True))
        self.assertRejected(lambda v: v["why_implementation_is_blocked"].__setitem__(
            "continuation_policy", "fixed at three"))

    def test_rejects_incomplete_future_state_or_hash(self) -> None:
        self.assertRejected(lambda v: v["required_future_state"]["original_order"].pop())
        self.assertRejected(lambda v: v["required_future_state"]["proposal"].pop())
        self.assertRejected(lambda v: v["required_future_state"].__setitem__(
            "all_fields_in_rules_authority_hash", False))

    def test_rejects_non_authoritative_shot_counting(self) -> None:
        self.assertRejected(lambda v: v["shot_counting_boundary"]["never_count"].remove(
            "a rejected Shoot action"))
        self.assertRejected(lambda v: v["shot_counting_boundary"]["never_count"].remove(
            "a replay seek, replay playback action or imported replay shot count"))
        self.assertRejected(lambda v: v["shot_counting_boundary"].__setitem__(
            "hardcoded_three_each_forbidden", False))

    def test_rejects_auto_declare_or_one_accept_override(self) -> None:
        self.assertRejected(lambda v: v["future_state_machine"].__setitem__(
            "progress_transition", "three each then automatically declare and rerack"))
        self.assertRejected(lambda v: v["future_state_machine"].__setitem__(
            "accept_transition", "first acceptance reracks immediately"))
        self.assertRejected(lambda v: v["future_state_machine"].__setitem__("product_exposed", True))

    def test_rejects_wrong_reset_semantics(self) -> None:
        self.assertRejected(lambda v: v["reset_contract"].__setitem__("turn_change_alone_resets_window", True))
        self.assertRejected(lambda v: v["reset_contract"].__setitem__("legal_pocket_alone_resets_window", True))
        self.assertRejected(lambda v: v["reset_contract"]["clear_all_stalemate_state_on"].pop())

    def test_rejects_nonatomic_or_current_player_rerack(self) -> None:
        self.assertRejected(lambda v: v["future_atomic_idempotency_contract"].__setitem__(
            "original_breaker_and_order", "use current player"))
        self.assertRejected(lambda v: v["future_atomic_idempotency_contract"].__setitem__(
            "failure_rollback", "partial"))
        self.assertRejected(lambda v: v["future_atomic_idempotency_contract"].__setitem__(
            "partial_commit_allowed", True))

    def test_rejects_missing_future_fixture(self) -> None:
        self.assertRejected(lambda v: v["required_future_fixtures"]["positive_after_policy"].pop())
        self.assertRejected(lambda v: v["required_future_fixtures"]["negative"].remove(
            "NUMERICAL_WINDOW_END_CANNOT_AUTO_DECLARE_STALEMATE"))

    def test_rejects_active_or_product_allowlist(self) -> None:
        self.assertRejected(lambda v: v["future_implementation_allowlist"].__setitem__(
            "allowlist_is_dormant_until_referee_input_and_continuation_policy_are_approved", False))
        self.assertRejected(lambda v: v["future_implementation_allowlist"].__setitem__(
            "product_session_files_allowed", True))
        self.assertRejected(lambda v: v["future_implementation_allowlist"]["production_files"].append(
            "src/Billiards/render/modern/modern_product_session.cpp"))

    def test_rejects_forbidden_scope_loss(self) -> None:
        for item in ("hardcoding generally three shots each as mandatory policy",
                     "automatically detecting or finally declaring stalemate without referee input",
                     "using global replay shotCount as a rules-authoritative per-player continuation counter",
                     "exposing the pending stalemate UI or product seam", "CMake changes",
                     "Phase4 or Phase5 renderer changes", "GPU, Cocoa, or OpenGL execution"):
            self.assertRejected(lambda v, item=item: v["forbidden_scope"].remove(item))

    def test_rejects_false_completion_claims(self) -> None:
        for key in ("referee_input_available", "continuation_policy_resolved",
                    "fixed_three_each_authorized", "stalemate_authority_implemented",
                    "dedicated_product_seam_available", "production_implementation_complete",
                    "audit_row_promoted", "global_ui_exposed", "cmake_modified",
                    "gpu_executed", "readiness_or_authorization_created"):
            self.assertRejected(lambda v, key=key: v["claims"].__setitem__(key, True))

    def test_rejects_non_object_bad_input(self) -> None:
        for value in (None, [], "gate", 1):
            self.assertTrue(validate(value, check_files=False))


if __name__ == "__main__":
    unittest.main()
