from __future__ import annotations

import copy
import json
from pathlib import Path
import unittest

from scripts.validate_m4_wpa_policy_referee_input_decision_register import validate


ROOT = Path(__file__).resolve().parents[2]
REGISTER = ROOT / "docs/rules/m4-wpa-policy-referee-input-decision-register-v1.json"


class M4WpaPolicyRefereeInputDecisionRegisterTests(unittest.TestCase):
    def setUp(self) -> None:
        self.value = json.loads(REGISTER.read_text(encoding="utf-8"))

    def assertRejected(self, mutate) -> None:
        value = copy.deepcopy(self.value)
        mutate(value)
        self.assertTrue(validate(value, check_files=False))

    def entry(self, value, row_id):
        return next(item for item in value["entries"] if item["row_id"] == row_id)

    def test_canonical_register_and_live_bindings_pass(self) -> None:
        self.assertEqual([], validate(self.value, check_files=True))

    def test_rejects_schema_or_status_drift(self) -> None:
        self.assertRejected(lambda v: v.__setitem__("schema", "v2"))
        self.assertRejected(lambda v: v.__setitem__("status", "POLICIES_SELECTED"))

    def test_rejects_pdf_or_audit_binding_drift(self) -> None:
        self.assertRejected(lambda v: v["official_source"].__setitem__("sha256", "0" * 64))
        self.assertRejected(lambda v: v["audit_binding"].__setitem__("sha256", "0" * 64))
        self.assertRejected(lambda v: v["audit_binding"]["m4_required_unclosed_rows"].pop())

    def test_rejects_silent_wpa_19_audit_priority_rewrite(self) -> None:
        self.assertRejected(lambda v: v["audit_binding"].__setitem__(
            "program_added_row_audit_m4_required", True))
        self.assertRejected(lambda v: v["audit_binding"].__setitem__(
            "audit_mutation_or_promotion_authorized", True))

    def test_rejects_gate_or_successor_binding_drift(self) -> None:
        self.assertRejected(lambda v: v["gate_bindings"]["wpa_ii_06_c"].__setitem__("sha256", "0" * 64))
        self.assertRejected(lambda v: v["gate_bindings"].pop("cue_contact_cluster"))
        self.assertRejected(lambda v: v["successor_receipt_bindings"]["wpa_ii_17_a_cue_scratch"].__setitem__(
            "sha256", "0" * 64))

    def test_rejects_recommendation_becoming_selection(self) -> None:
        self.assertRejected(lambda v: v["decision_rules"].__setitem__("recommendation_is_selection", True))
        self.assertRejected(lambda v: v["decision_bundle"].__setitem__("all_unselected", False))
        self.assertRejected(lambda v: v["entries"][0].__setitem__("selection_status", "SELECTED"))

    def test_rejects_global_ui_or_unsupported_evidence_policy(self) -> None:
        self.assertRejected(lambda v: v["decision_rules"].__setitem__(
            "global_rules_decision_ui_product_exposed", True))
        self.assertRejected(lambda v: v["decision_rules"].__setitem__(
            "no_referee_fact_may_be_inferred_from_geometry", False))
        self.assertRejected(lambda v: v["decision_rules"].__setitem__(
            "no_unsupported_physics_may_be_simulated_by_injected_coordinates", False))

    def test_rejects_entry_inventory_duplicate_or_loss(self) -> None:
        self.assertRejected(lambda v: v["entries"].pop())
        self.assertRejected(lambda v: v["entries"].append(copy.deepcopy(v["entries"][0])))

    def test_rejects_entry_order_or_stable_id_drift(self) -> None:
        self.assertRejected(lambda v: v["entries"].reverse())
        self.assertRejected(lambda v: v["entries"][0].__setitem__("row_id", "WPA-II-06-C-RENAMED"))
        self.assertRejected(lambda v: v["entries"][0].__setitem__("decision_id", "M4-POL-RENAMED"))

    def test_rejects_06c_policy_or_physics_drift(self) -> None:
        self.assertRejected(lambda v: self.entry(v, "WPA-II-06-C")["recommended_shipping_policy"].__setitem__(
            "id", "INCLUDE_POCKET_JAWS"))
        self.assertRejected(lambda v: self.entry(v, "WPA-II-06-C").__setitem__("new_physics_required", True))

    def test_rejects_06d_policy_or_referee_drift(self) -> None:
        self.assertRejected(lambda v: self.entry(v, "WPA-II-06-D")["recommended_shipping_policy"].__setitem__(
            "id", "RESET_PER_RACK"))
        self.assertRejected(lambda v: self.entry(v, "WPA-II-06-D").__setitem__("external_referee_required", True))

    def test_rejects_06e_or_06f_product_choice_drift(self) -> None:
        self.assertRejected(lambda v: self.entry(v, "WPA-II-06-E")["recommended_shipping_policy"].__setitem__(
            "id", "GLOBAL_UI"))
        self.assertRejected(lambda v: self.entry(v, "WPA-II-06-F")["recommended_shipping_policy"].__setitem__(
            "id", "KEEP_SPOT_ONLY"))

    def test_rejects_08_or_09_geometry_lifecycle_drift(self) -> None:
        self.assertRejected(lambda v: self.entry(v, "WPA-II-08")["recommended_shipping_policy"].__setitem__(
            "id", "LEADING_EDGE_CROSSING"))
        self.assertRejected(lambda v: self.entry(v, "WPA-II-09")["recommended_shipping_policy"].__setitem__(
            "id", "VALIDATED_LAUNCH_CONSUMES"))

    def test_rejects_off_table_policy_or_physics_false_claim(self) -> None:
        self.assertRejected(lambda v: self.entry(v, "WPA-II-12-3").__setitem__("external_referee_required", False))
        self.assertRejected(lambda v: self.entry(v, "WPA-II-12-3").__setitem__("new_physics_required", False))
        self.assertRejected(lambda v: self.entry(v, "WPA-II-12-3")["recommended_shipping_policy"].__setitem__(
            "id", "SILENT_17E"))

    def test_rejects_double_hit_or_push_completion_without_dependencies(self) -> None:
        self.assertRejected(lambda v: self.entry(v, "WPA-II-12-5-6").__setitem__("new_physics_required", False))
        self.assertRejected(lambda v: self.entry(v, "WPA-II-12-7").__setitem__("external_referee_required", False))

    def test_rejects_frozen_declaration_without_referee(self) -> None:
        self.assertRejected(lambda v: self.entry(v, "WPA-II-15-1").__setitem__("external_referee_required", False))
        self.assertRejected(lambda v: self.entry(v, "WPA-II-15-2").__setitem__("external_referee_required", False))

    def test_rejects_jump_enablement_or_false_no_physics_claim(self) -> None:
        self.assertRejected(lambda v: self.entry(v, "WPA-II-16")["recommended_shipping_policy"].__setitem__(
            "id", "ENABLE_JUMP_NOW"))
        self.assertRejected(lambda v: self.entry(v, "WPA-II-16").__setitem__("new_physics_required", False))

    def test_rejects_general_foul_false_completion(self) -> None:
        self.assertRejected(lambda v: self.entry(v, "WPA-II-17")["recommended_shipping_policy"].__setitem__(
            "id", "ALL_AUTOMATIC"))
        self.assertRejected(lambda v: self.entry(v, "WPA-II-17").__setitem__("external_referee_required", False))

    def test_rejects_19_wrong_first_or_21_mandatory_three_policy(self) -> None:
        self.assertRejected(lambda v: self.entry(v, "WPA-II-19")["recommended_shipping_policy"].__setitem__(
            "id", "EVERY_WRONG_FIRST_TRIGGERS_RERACK"))
        self.assertRejected(lambda v: self.entry(v, "WPA-II-21")["recommended_shipping_policy"].__setitem__(
            "id", "MANDATORY_THREE_EACH"))

    def test_rejects_missing_allowlist_or_fixture_family(self) -> None:
        self.assertRejected(lambda v: self.entry(v, "WPA-II-08")["minimal_implementation_allowlist"].pop())
        self.assertRejected(lambda v: self.entry(v, "WPA-II-09")["required_fixtures"]["positive"].pop())
        self.assertRejected(lambda v: self.entry(v, "WPA-II-09")["required_fixtures"]["negative"].pop())
        self.assertRejected(lambda v: self.entry(v, "WPA-II-17")["minimal_implementation_allowlist"].append(
            "CMakeLists.txt"))

    def test_rejects_per_row_capability_gap_or_evidence_contract_loss(self) -> None:
        self.assertRejected(lambda v: self.entry(v, "WPA-II-06-C").__setitem__("current_capability", ""))
        self.assertRejected(lambda v: self.entry(v, "WPA-II-06-C").__setitem__(
            "current_capability", "different but nonempty capability"))
        self.assertRejected(lambda v: self.entry(v, "WPA-II-06-D").__setitem__(
            "remaining_policy_referee_physics_gap", ""))
        self.assertRejected(lambda v: self.entry(v, "WPA-II-06-D").__setitem__(
            "remaining_policy_referee_physics_gap", "different but nonempty gap"))
        self.assertRejected(lambda v: self.entry(v, "WPA-II-16").__setitem__("evidence_invalidation", ""))
        self.assertRejected(lambda v: self.entry(v, "WPA-II-16").__setitem__(
            "evidence_invalidation", "different but nonempty evidence rule"))
        self.assertRejected(lambda v: self.entry(v, "WPA-II-21").__setitem__("officially_determined", []))
        self.assertRejected(lambda v: self.entry(v, "WPA-II-21")["officially_determined"].__setitem__(
            0, "different official determination"))

    def test_rejects_nonempty_allowlist_fixture_or_alternative_drift(self) -> None:
        self.assertRejected(lambda v: self.entry(v, "WPA-II-08")["minimal_implementation_allowlist"].__setitem__(
            0, "src/Billiards/game.cpp"))
        self.assertRejected(lambda v: self.entry(v, "WPA-II-09")["required_fixtures"]["positive"].__setitem__(
            0, "different positive fixture"))
        self.assertRejected(lambda v: self.entry(v, "WPA-II-17")["required_fixtures"]["negative"].__setitem__(
            0, "different negative fixture"))
        self.assertRejected(lambda v: self.entry(v, "WPA-II-19")["mutually_exclusive_alternatives"].__setitem__(
            0, "DIFFERENT_FULL_PROFILE"))

    def test_rejects_recommended_or_alternative_policy_selection(self) -> None:
        self.assertRejected(lambda v: self.entry(v, "WPA-II-06-C")["recommended_shipping_policy"].__setitem__(
            "selection_status", "SELECTED"))
        self.assertRejected(lambda v: self.entry(v, "WPA-II-12-3").__setitem__(
            "alternatives_selection_status", "SELECTED"))
        self.assertRejected(lambda v: self.entry(v, "WPA-II-19")["mutually_exclusive_alternatives"].pop())

    def test_rejects_four_way_closure_classification_drift(self) -> None:
        self.assertRejected(lambda v: v["closure_classification"]["policy_independent_telemetry_already_implemented"].pop())
        self.assertRejected(lambda v: v["closure_classification"]["existing_narrow_product_subflows"].pop())
        self.assertRejected(lambda v: v["closure_classification"]["still_requires_root_policy_or_external_referee_decision"].pop())
        self.assertRejected(lambda v: v["closure_classification"]["requires_new_or_materially_extended_physics_before_automatic_claim"].pop())
        self.assertRejected(lambda v: v["closure_classification"].__setitem__("existing_subflow_is_full_row_completion", True))

    def test_rejects_decision_id_order_or_implementation_authorization(self) -> None:
        self.assertRejected(lambda v: v["decision_bundle"]["all_decision_ids"].reverse())
        self.assertRejected(lambda v: v["decision_bundle"].__setitem__(
            "implementation_may_start_from_this_register_alone", True))

    def test_rejects_forbidden_scope_loss(self) -> None:
        for item in ("treating recommendations as selected policy",
                     "editing production source or CMake", "editing or promoting audit v5",
                     "claiming referee input exists where none exists",
                     "claiming off-table or jump physics exists where none exists",
                     "Phase4 or Phase5 renderer changes", "GPU, Cocoa, or OpenGL execution"):
            self.assertRejected(lambda v, item=item: v["forbidden_scope"].remove(item))

    def test_rejects_false_completion_claims(self) -> None:
        for key in ("any_policy_selected", "any_production_implementation_authorized",
                    "audit_modified_or_promoted", "product_exposure_changed", "global_ui_exposed",
                    "cmake_modified", "gpu_executed", "readiness_or_authorization_created"):
            self.assertRejected(lambda v, key=key: v["claims"].__setitem__(key, True))

    def test_rejects_non_object_bad_input(self) -> None:
        for value in (None, [], "register", 1):
            self.assertTrue(validate(value, check_files=False))

    def test_rejects_malformed_nested_types_without_exception(self) -> None:
        self.assertRejected(lambda v: self.entry(v, "WPA-II-06-C").__setitem__(
            "recommended_shipping_policy", "UNSELECTED"))
        self.assertRejected(lambda v: self.entry(v, "WPA-II-06-C").__setitem__(
            "mutually_exclusive_alternatives", [{"not": "hashable"}]))
        self.assertRejected(lambda v: self.entry(v, "WPA-II-06-C").__setitem__(
            "required_fixtures", "fixtures"))


if __name__ == "__main__":
    unittest.main()
