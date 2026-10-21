from __future__ import annotations

import copy
import json
from pathlib import Path
import unittest

from scripts.validate_wpa_ii_19_ball_group_confusion_discovery_rerack_design_gap_gate import validate


ROOT = Path(__file__).resolve().parents[2]
GATE = ROOT / "docs/rules/wpa-ii-19-ball-group-confusion-discovery-rerack-design-gap-gate-v1.json"


class WpaIi19BallGroupConfusionDiscoveryRerackDesignGapGateTests(unittest.TestCase):
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
        self.assertRejected(lambda v: v.__setitem__("scope", ["WPA-II-17", "WPA-II-19"]))

    def test_rejects_priority_or_frozen_audit_rewrite(self) -> None:
        self.assertRejected(lambda v: v["program_priority"].__setitem__("current_task", "P2"))
        self.assertRejected(lambda v: v["program_priority"].__setitem__("frozen_audit_v5_m4_required", True))
        self.assertRejected(lambda v: v["frozen_audit"]["row"].__setitem__("m4_required", True))

    def test_rejects_pdf_section_or_page_drift(self) -> None:
        self.assertRejected(lambda v: v["official_source"].__setitem__("sha256", "0" * 64))
        self.assertRejected(lambda v: v["official_source"]["section"].__setitem__("clause", "17(b)"))
        self.assertRejected(lambda v: v["official_source"].__setitem__("visually_reviewed_pdf_pages", [10]))

    def test_rejects_deadline_actor_or_rerack_rule_loss(self) -> None:
        self.assertRejected(lambda v: v["official_source"]["uniquely_determined_rules"].remove(
            "the foul-pronouncement deadline is before the next shot"))
        self.assertRejected(lambda v: v["official_source"]["uniquely_determined_rules"].remove(
            "a player or referee may be the party who realizes that groups were wrongly played"))
        self.assertRejected(lambda v: v["official_source"]["uniquely_determined_rules"].remove(
            "the prescribed rack-level remedy is rerack"))

    def test_rejects_wrong_first_to_rerack_guess(self) -> None:
        self.assertRejected(lambda v: v["official_source"].__setitem__(
            "automatic_wrong_first_to_rerack_mapping_authorized", True))
        self.assertRejected(lambda v: v["official_source"]["text_does_not_uniquely_determine"].remove(
            "whether every ordinary wrong-first-ball foul is also a group-confusion rerack event"))
        self.assertRejected(lambda v: v["pending_policy_and_input_contract"]["classification"].__setitem__(
            "ordinary_wrong_first_always_section_19", True))

    def test_rejects_group_confusion_meaning_guess(self) -> None:
        self.assertRejected(lambda v: v["official_source"]["text_does_not_uniquely_determine"].remove(
            "whether groups wrongly played means one wrong-group stroke, a mistaken group assignment, or a longer sequence of play under confused assignments"))
        self.assertRejected(lambda v: v["pending_policy_and_input_contract"]["classification"].__setitem__(
            "mistaken_assignment_or_sequence_required", False))

    def test_rejects_deadline_or_late_remedy_guess(self) -> None:
        self.assertRejected(lambda v: v["pending_policy_and_input_contract"]["deadline"].__setitem__(
            "next_shot_machine_boundary", "mouse_down"))
        self.assertRejected(lambda v: v["pending_policy_and_input_contract"]["deadline"].__setitem__(
            "late_discovery_effect", "ignore"))

    def test_rejects_breaker_order_or_entitlement_guess(self) -> None:
        self.assertRejected(lambda v: v["pending_policy_and_input_contract"]["rerack"].__setitem__(
            "breaker_player_id", "current_player"))
        self.assertRejected(lambda v: v["pending_policy_and_input_contract"]["rerack"].__setitem__(
            "shooting_order", "reset"))
        self.assertRejected(lambda v: v["pending_policy_and_input_contract"]["rerack"].__setitem__(
            "ordinary_ball_in_hand_disposition", "grant_before_rerack"))

    def test_rejects_audit_promotion(self) -> None:
        self.assertRejected(lambda v: v["frozen_audit"]["row"].__setitem__("status", "PARTIAL"))
        self.assertRejected(lambda v: v["frozen_audit"].__setitem__("promotion_authorized", True))

    def test_rejects_source_binding_drift(self) -> None:
        self.assertRejected(lambda v: v["source_bindings"]["runtime"].__setitem__("sha256", "0" * 64))
        self.assertRejected(lambda v: v["source_bindings"].pop("group_choice_authority"))

    def test_rejects_false_current_authority_claims(self) -> None:
        for key in ("wrong_first_contact_is_section_19_discovery_event",
                    "group_confusion_discovery_actor_or_input_exists",
                    "offending_shot_identity_persists_for_discovery_window",
                    "before_next_shot_window_state_exists", "rack_stop_action_exists",
                    "section_19_parent_authority_exists", "section_19_breaker_or_order_policy_exists",
                    "applied_shoot_journal_is_section_19_discovery_authority",
                    "replay_shot_count_is_section_19_deadline_authority",
                    "dedicated_group_confusion_product_seam_exists"):
            self.assertRejected(lambda v, key=key: v["current_system_mapping"].__setitem__(key, True))

    def test_rejects_section_17_conflation_or_mutation(self) -> None:
        self.assertRejected(lambda v: v["ordinary_foul_boundary"]["section_17_wrong_first_contact"].__setitem__(
            "remains_independent", False))
        self.assertRejected(lambda v: v["ordinary_foul_boundary"]["section_19_group_confusion"].__setitem__(
            "may_not_be_inferred_from_wrong_first_contact_alone", False))
        self.assertRejected(lambda v: v["ordinary_foul_boundary"].__setitem__(
            "no_optimistic_entitlement_or_rerack_mutation", False))

    def test_rejects_unsafe_production_extension(self) -> None:
        self.assertRejected(lambda v: v["why_implementation_is_blocked"].__setitem__(
            "production_extension_safe_now", True))
        self.assertRejected(lambda v: v["why_implementation_is_blocked"].__setitem__(
            "classification", "all wrong first is confusion"))

    def test_rejects_invented_actor_input(self) -> None:
        self.assertRejected(lambda v: v["pending_policy_and_input_contract"]["actor"].__setitem__(
            "authenticated_player_request_schema", "trust_local"))
        self.assertRejected(lambda v: v["pending_policy_and_input_contract"]["actor"].__setitem__(
            "authenticated_referee_request_schema", "trust_host"))
        self.assertRejected(lambda v: v["pending_policy_and_input_contract"]["actor"].__setitem__(
            "allowed_textual_classes", ["player"]))

    def test_rejects_incomplete_future_evidence(self) -> None:
        self.assertRejected(lambda v: v["required_future_evidence"]["offending_shot"].pop())
        self.assertRejected(lambda v: v["required_future_evidence"]["discovery"].pop())
        self.assertRejected(lambda v: v["required_future_evidence"].__setitem__(
            "all_fields_in_rules_authority_hash", False))

    def test_rejects_preview_as_window_close_or_product_exposure(self) -> None:
        self.assertRejected(lambda v: v["future_state_machine"].__setitem__(
            "next_shot_close", "aim preview closes window"))
        self.assertRejected(lambda v: v["future_state_machine"].__setitem__("product_exposed", True))

    def test_rejects_rerack_without_policy_or_atomic_parent(self) -> None:
        self.assertRejected(lambda v: v["future_state_machine"].__setitem__(
            "rerack_commit", "call foundation with current player"))
        self.assertRejected(lambda v: v["future_atomic_idempotency_contract"].__setitem__(
            "stop_and_rerack", "stop now, rerack later"))
        self.assertRejected(lambda v: v["future_atomic_idempotency_contract"].__setitem__(
            "partial_commit_allowed", True))

    def test_rejects_failure_mutating_ordinary_foul_or_state(self) -> None:
        self.assertRejected(lambda v: v["future_atomic_idempotency_contract"].__setitem__(
            "ordinary_foul_isolation", "clear BIH on request"))
        self.assertRejected(lambda v: v["future_atomic_idempotency_contract"].__setitem__(
            "failure_rollback", "partial"))

    def test_rejects_missing_future_fixture(self) -> None:
        self.assertRejected(lambda v: v["required_future_fixtures"]["positive_after_policy"].pop())
        self.assertRejected(lambda v: v["required_future_fixtures"]["negative"].remove(
            "WRONG_FIRST_CONTACT_ALONE_CANNOT_AUTO_RERACK"))

    def test_rejects_active_or_product_allowlist(self) -> None:
        self.assertRejected(lambda v: v["future_implementation_allowlist"].__setitem__(
            "allowlist_is_dormant_until_discovery_input_classification_deadline_and_rerack_policy_are_approved", False))
        self.assertRejected(lambda v: v["future_implementation_allowlist"].__setitem__(
            "product_session_files_allowed", True))
        self.assertRejected(lambda v: v["future_implementation_allowlist"]["production_files"].append(
            "src/Billiards/render/modern/modern_product_session.cpp"))

    def test_rejects_forbidden_scope_loss(self) -> None:
        for item in ("mapping every wrong-first-ball foul directly to a Section 19 rerack",
                     "inventing a player or referee discovery event from physics facts alone",
                     "guessing the rerack breaker or shooting order",
                     "changing Section 17 wrong-first-contact foul semantics",
                     "CMake changes", "Phase4 or Phase5 renderer changes",
                     "GPU, Cocoa, or OpenGL execution"):
            self.assertRejected(lambda v, item=item: v["forbidden_scope"].remove(item))

    def test_rejects_false_completion_claims(self) -> None:
        for key in ("section_19_discovery_input_available", "section_19_classification_resolved",
                    "next_shot_deadline_policy_resolved", "rerack_breaker_and_order_policy_resolved",
                    "section_19_internal_authority_implemented", "dedicated_product_seam_available",
                    "production_implementation_complete", "audit_row_promoted", "global_ui_exposed",
                    "cmake_modified", "gpu_executed", "readiness_or_authorization_created"):
            self.assertRejected(lambda v, key=key: v["claims"].__setitem__(key, True))

    def test_rejects_non_object_bad_input(self) -> None:
        for value in (None, [], "gate", 1):
            self.assertTrue(validate(value, check_files=False))


if __name__ == "__main__":
    unittest.main()
