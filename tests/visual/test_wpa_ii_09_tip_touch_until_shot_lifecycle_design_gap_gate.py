from __future__ import annotations

import copy
import json
from pathlib import Path
import unittest

from scripts.validate_wpa_ii_09_tip_touch_until_shot_lifecycle_design_gap_gate import validate


ROOT = Path(__file__).resolve().parents[2]
GATE = ROOT / "docs/rules/wpa-ii-09-tip-touch-until-shot-lifecycle-design-gap-gate-v1.json"


class WpaIi09TipTouchUntilShotLifecycleGateTests(unittest.TestCase):
    def setUp(self) -> None:
        self.value = json.loads(GATE.read_text(encoding="utf-8"))

    def assertRejected(self, mutate) -> None:
        value = copy.deepcopy(self.value)
        mutate(value)
        self.assertTrue(validate(value, check_files=False))

    def test_canonical_gate_and_live_bindings_pass(self) -> None:
        self.assertEqual([], validate(self.value, check_files=True))

    def test_rejects_identity_status_or_scope_expansion(self) -> None:
        self.assertRejected(lambda value: value.__setitem__("status", "IMPLEMENTATION_APPROVED"))
        self.assertRejected(lambda value: value.__setitem__("scope", ["WPA-II-08", "WPA-II-09"]))

    def test_rejects_pdf_path_sha_or_page_drift(self) -> None:
        self.assertRejected(lambda value: value["official_source"]
                            .__setitem__("path", "../../rules.pdf"))
        self.assertRejected(lambda value: value["official_source"]
                            .__setitem__("sha256", "0" * 64))
        self.assertRejected(lambda value: value["official_source"]
                            ["visually_reviewed_pdf_pages"].append(9))

    def test_rejects_entitlement_persistence_text_loss(self) -> None:
        self.assertRejected(lambda value: value["official_source"]
                            ["text_uniquely_determines"].remove(
                                "the ball-in-hand entitlement persists through repeated cue-ball movement until a shot is executed"))
        self.assertRejected(lambda value: value["official_source"]
                            ["text_uniquely_determines"].remove(
                                "a rejected preview, rejected placement commit, or rejected technical shot request cannot consume the entitlement"))

    def test_rejects_forward_stroke_ambiguity_erasure(self) -> None:
        self.assertRejected(lambda value: value["official_source"]
                            ["text_does_not_specify"].pop())
        self.assertRejected(lambda value: value["official_source"]
                            ["contextual_tip_reading"]
                            .__setitem__("forward_stroke_definition_available", True))

    def test_rejects_nonbreak_specific_tip_rule_loss(self) -> None:
        self.assertRejected(lambda value: value["official_source"]
                            ["contextual_tip_reading"]
                            .__setitem__("specific_non_break_sentence_overrides_general_tip_permission", False))

    def test_rejects_audit_promotion_or_subflow_demotion(self) -> None:
        self.assertRejected(lambda value: value["frozen_audit"]
                            .__setitem__("row_status", "IMPLEMENTED"))
        self.assertRejected(lambda value: value["frozen_audit"]
                            .__setitem__("product_placement_subflow_reachable", False))

    def test_rejects_source_sha_or_inventory_drift(self) -> None:
        self.assertRejected(lambda value: value["source_bindings"]["runtime"]
                            .__setitem__("sha256", "0" * 64))
        self.assertRejected(lambda value: value["source_bindings"].pop("cue_impact"))

    def test_rejects_false_existing_tip_or_stroke_telemetry_claim(self) -> None:
        self.assertRejected(lambda value: value["existing_mapping"]
                            .__setitem__("cue_impact_input_has_cue_part_identity", True))
        self.assertRejected(lambda value: value["existing_mapping"]
                            .__setitem__("physics_telemetry_has_placement_tip_touch_event", True))

    def test_rejects_false_current_repeated_move_claim(self) -> None:
        self.assertRejected(lambda value: value["existing_mapping"]
                            .__setitem__("repeated_move_after_confirm_currently_product_reachable", True))
        self.assertRejected(lambda value: value["existing_mapping"]
                            .__setitem__("core_g_confirm_currently_clears_ball_in_hand", False))

    def test_rejects_preview_confirm_or_failed_input_consumption(self) -> None:
        self.assertRejected(lambda value: value["resolved_lifecycle_contract"]
                            .__setitem__("placement_commit_may_end_entitlement", True))
        self.assertRejected(lambda value: value["resolved_lifecycle_contract"]
                            ["must_not_consume"].remove("read-only preview"))
        self.assertRejected(lambda value: value["resolved_lifecycle_contract"]
                            ["must_not_consume"].remove(
                                "failed cue-impact validation or rejected dispatch"))

    def test_rejects_missing_reset_or_turn_change_boundary(self) -> None:
        self.assertRejected(lambda value: value["resolved_lifecycle_contract"]
                            ["reset_boundaries"].pop())
        self.assertRejected(lambda value: value["resolved_lifecycle_contract"]
                            .__setitem__("unexpected_turn_change_while_entitlement_active",
                                         "transfer to the next actor"))

    def test_rejects_miscue_consumption_guess(self) -> None:
        self.assertRejected(lambda value: value["resolved_lifecycle_contract"]
                            .__setitem__("failed_or_miscue_shot_consumes_entitlement", True))

    def test_rejects_forward_stroke_policy_guess(self) -> None:
        def mutate(value):
            policy = value["tip_touch_and_shot_classification"]
            policy["policy_decisions"]["forward_stroke_predicate"]["selected"] = "positive_velocity"
            policy["policy_decisions"]["forward_stroke_predicate"]["status"] = "RESOLVED"
        self.assertRejected(mutate)

    def test_rejects_shot_execution_boundary_guess(self) -> None:
        self.assertRejected(lambda value: value["tip_touch_and_shot_classification"]
                            ["policy_decisions"]["shot_execution_boundary"]
                            .__setitem__("selected", "positive_authoritative_cue_ball_impulse"))

    def test_rejects_tip_contact_numeric_policy_guess(self) -> None:
        self.assertRejected(lambda value: value["tip_touch_and_shot_classification"]
                            ["policy_decisions"]["tip_contact_numeric_policy"]
                            .__setitem__("selected", "exact_geometry"))

    def test_rejects_missing_tip_or_consumption_telemetry(self) -> None:
        self.assertRejected(lambda value: value["tip_touch_and_shot_classification"]
                            ["required_future_telemetry"].remove(
                                "cue_part_identity_tip_or_non_tip"))
        self.assertRejected(lambda value: value["tip_touch_and_shot_classification"]
                            ["required_future_telemetry"].remove(
                                "entitlement_consumption_reason"))
        self.assertRejected(lambda value: value["tip_touch_and_shot_classification"]
                            .__setitem__("production_implementation_authorized", True))

    def test_rejects_coordinate_drag_as_tip_touch(self) -> None:
        self.assertRejected(lambda value: value["tip_touch_and_shot_classification"]
                            .__setitem__("coordinate_drag_policy",
                                         "coordinate drag is a tip touch"))

    def test_rejects_nonatomic_release_or_rollback_weakening(self) -> None:
        atomic = "future_atomic_product_contract"
        self.assertRejected(lambda value: value[atomic]
                            ["shot_release_commit_after_policy"].pop())
        self.assertRejected(lambda value: value[atomic]
                            .__setitem__("partial_commit_allowed", True))
        self.assertRejected(lambda value: value[atomic]
                            .__setitem__("failure_rollback", "partial rollback"))
        self.assertRejected(lambda value: value[atomic]
                            .__setitem__("exact_retry", "consume again"))

    def test_rejects_global_ui_foundation_or_other_action_exposure(self) -> None:
        atomic = "future_atomic_product_contract"
        self.assertRejected(lambda value: value[atomic]
                            .__setitem__("global_rules_decision_ui_product_exposed", True))
        self.assertRejected(lambda value: value[atomic]
                            .__setitem__("foundation_actions_product_exposed", True))
        self.assertRejected(lambda value: value[atomic]
                            .__setitem__("other_rules_actions_product_exposed", True))

    def test_rejects_missing_future_fixture(self) -> None:
        self.assertRejected(lambda value: value["required_future_fixtures"]
                            ["lifecycle_positive"].pop())
        self.assertRejected(lambda value: value["required_future_fixtures"]
                            ["lifecycle_negative"].pop())
        self.assertRejected(lambda value: value["required_future_fixtures"]
                            ["tip_and_shot_after_policy"].pop())

    def test_rejects_active_allowlist_or_renderer_cmake_expansion(self) -> None:
        self.assertRejected(lambda value: value["future_implementation_allowlist"]
                            .__setitem__("allowlist_is_dormant_until_tip_and_shot_policies_resolved", False))
        self.assertRejected(lambda value: value["future_implementation_allowlist"]
                            ["production_files"].append(
                                "src/Billiards/render/modern/modern_renderer.cpp"))
        self.assertRejected(lambda value: value["future_implementation_allowlist"]
                            ["production_files"].append("CMakeLists.txt"))

    def test_rejects_forbidden_scope_loss(self) -> None:
        for item in ("GPU, Cocoa, or OpenGL execution", "CMake changes",
                     "changes to WPA-II-08 re-spot or first-contact semantics",
                     "promotion of WPA-II-09 above PARTIAL",
                     "treating coordinate preview or drag as a cue-tip touch"):
            self.assertRejected(lambda value, item=item:
                                value["forbidden_scope"].remove(item))

    def test_rejects_false_completion_claims(self) -> None:
        for key in ("forward_stroke_policy_resolved", "shot_execution_boundary_resolved",
                    "tip_touch_telemetry_complete", "production_implementation_complete",
                    "audit_row_promoted"):
            self.assertRejected(lambda value, key=key:
                                value["claims"].__setitem__(key, True))

    def test_rejects_non_object_bad_input(self) -> None:
        self.assertTrue(validate(None, check_files=False))
        self.assertTrue(validate([], check_files=False))
        self.assertTrue(validate("gate", check_files=False))


if __name__ == "__main__":
    unittest.main()
