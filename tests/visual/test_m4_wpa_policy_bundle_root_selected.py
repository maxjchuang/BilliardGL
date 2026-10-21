from __future__ import annotations

import copy
import json
from pathlib import Path
import unittest

from scripts.validate_m4_wpa_policy_bundle_root_selected import validate


ROOT = Path(__file__).resolve().parents[2]
BUNDLE = ROOT / "docs/rules/m4-wpa-policy-bundle-root-selected-v1.json"


class M4WpaPolicyBundleRootSelectedTests(unittest.TestCase):
    def setUp(self) -> None:
        self.value = json.loads(BUNDLE.read_text(encoding="utf-8"))

    def assertRejected(self, mutate) -> None:
        value = copy.deepcopy(self.value)
        mutate(value)
        self.assertTrue(validate(value, check_files=False))

    def entry(self, value, row_id):
        return next(item for item in value["decisions"] if item["row_id"] == row_id)

    def test_canonical_selection_and_live_bindings_pass(self) -> None:
        self.assertEqual([], validate(self.value, check_files=True))

    def test_rejects_schema_identity_or_version_drift(self) -> None:
        self.assertRejected(lambda v: v.__setitem__("schema", "v2"))
        self.assertRejected(lambda v: v.__setitem__("bundle_id", "selection-v2"))
        self.assertRejected(lambda v: v.__setitem__("selection_version", 2))
        self.assertRejected(lambda v: v.__setitem__("selected_by", "AGENT"))
        self.assertRejected(lambda v: v.__setitem__("selected_on", "2026-08-13"))

    def test_rejects_input_binding_drift(self) -> None:
        for key in self.value["input_bindings"]:
            self.assertRejected(lambda v, key=key: v["input_bindings"][key].__setitem__("sha256", "0" * 64))
        self.assertRejected(lambda v: v["input_bindings"].pop("candidate"))

    def test_rejects_root_directive_semantic_drift(self) -> None:
        self.assertRejected(lambda v: v["root_pm_selection_directive"].__setitem__(
            "semantic_text", "approve everything including GPU"))
        self.assertRejected(lambda v: v["root_pm_selection_directive"].__setitem__(
            "semantic_sha256", "0" * 64))

    def test_rejects_decision_loss_duplicate_or_order_drift(self) -> None:
        self.assertRejected(lambda v: v["decisions"].pop())
        self.assertRejected(lambda v: v["decisions"].append(copy.deepcopy(v["decisions"][0])))
        self.assertRejected(lambda v: v["decisions"].reverse())

    def test_rejects_any_item_not_root_selected(self) -> None:
        self.assertRejected(lambda v: self.entry(v, "WPA-II-06-C").__setitem__("root_selected", False))
        self.assertRejected(lambda v: self.entry(v, "WPA-II-21").__setitem__("root_selected", False))

    def test_rejects_nonrecommended_selected_option(self) -> None:
        self.assertRejected(lambda v: self.entry(v, "WPA-II-06-C").__setitem__(
            "selected_option_id", "KEEP_PRODUCT_CHOICE_CLOSED"))
        self.assertRejected(lambda v: self.entry(v, "WPA-II-16").__setitem__(
            "selected_option_id", "BUILD_FULL_3D_THEN_ENABLE"))

    def test_rejects_candidate_decision_binding_drift(self) -> None:
        self.assertRejected(lambda v: self.entry(v, "WPA-II-09").__setitem__(
            "candidate_decision_sha256", "0" * 64))

    def test_rejects_selection_as_row_completion(self) -> None:
        self.assertRejected(lambda v: self.entry(v, "WPA-II-06-C").__setitem__(
            "row_closed_by_selection", True))
        self.assertRejected(lambda v: v["selection_semantics"].__setitem__(
            "policy_selection_is_row_completion", True))
        self.assertRejected(lambda v: v["selection_semantics"].__setitem__(
            "dependency_blocked_or_deferred_rows_remain_open", False))

    def test_rejects_b1_scope_loss_or_b2_row_overauthorization(self) -> None:
        self.assertRejected(lambda v: self.entry(v, "WPA-II-06-D").__setitem__(
            "authorized_stage", "B2_TESTS_FIRST_CPU_REFEREE_PROTOCOL_FOUNDATION_ONLY"))
        self.assertRejected(lambda v: self.entry(v, "WPA-II-12-7").__setitem__(
            "row_implementation_allowed", True))
        self.assertRejected(lambda v: self.entry(v, "WPA-II-19").__setitem__(
            "authorized_stage", "B1_TESTS_FIRST_CPU_DIRECT_CODE"))

    def test_rejects_d5_d6_or_d7_implementation_authorization(self) -> None:
        for key in ("B3_D5_cue_contact_implementation_allowed", "B3_D6_off_table_physics_implementation_allowed",
                    "B3_D7_jump_3d_physics_implementation_allowed"):
            self.assertRejected(lambda v, key=key: v["implementation_permissions"].__setitem__(key, True))
        self.assertRejected(lambda v: v["implementation_permissions"].__setitem__(
            "B3_requires_independent_versioned_implementation_design_gate_per_lane", False))

    def test_rejects_jump_as_implemented_or_enabled(self) -> None:
        self.assertRejected(lambda v: self.entry(v, "WPA-II-16").__setitem__(
            "authorized_stage", "B1_TESTS_FIRST_CPU_DIRECT_CODE"))
        self.assertRejected(lambda v: self.entry(v, "WPA-II-16").__setitem__(
            "row_implementation_allowed", True))
        self.assertRejected(lambda v: v["claims"].__setitem__("wpa_ii_16_implemented", True))

    def test_rejects_off_table_defer_or_section17_false_close(self) -> None:
        self.assertRejected(lambda v: self.entry(v, "WPA-II-17")["remaining_independent_gates"].pop())
        self.assertRejected(lambda v: self.entry(v, "WPA-II-12-3")["remaining_independent_gates"].clear())
        self.assertRejected(lambda v: v["claims"].__setitem__("wpa_ii_17_full_row_closed", True))

    def test_rejects_gpu_readiness_or_cmake_authorization(self) -> None:
        for key in ("gpu_execution_allowed", "readiness_generation_allowed", "authorization_issuance_allowed",
                    "cmake_change_allowed", "phase4_or_phase5_renderer_change_allowed"):
            self.assertRejected(lambda v, key=key: v["implementation_permissions"].__setitem__(key, True))

    def test_rejects_profile_or_m4_completion_claims(self) -> None:
        for key in ("any_row_closed_by_selection", "profile_complete", "m4_complete",
                    "d5_d6_d7_implementation_authorized", "gpu_execution_authorized",
                    "readiness_generation_authorized", "capture_authorization_issuance_authorized",
                    "production_modified_by_selection_freeze", "cmake_modified"):
            self.assertRejected(lambda v, key=key: v["claims"].__setitem__(key, True))

    def test_rejects_audit_v5_or_global_ui_semantic_drift(self) -> None:
        self.assertRejected(lambda v: v["selection_semantics"].__setitem__(
            "wpa_ii_19_remains_program_priority_without_mutating_audit_v5", False))
        self.assertRejected(lambda v: v["selection_semantics"].__setitem__(
            "global_rules_decision_ui_product_exposed", True))

    def test_malformed_inputs_fail_closed(self) -> None:
        for value in (None, [], "selection", 1):
            self.assertTrue(validate(value, check_files=False))
        self.assertRejected(lambda v: v.__setitem__("decisions", "decisions"))
        self.assertRejected(lambda v: v.__setitem__("claims", "claims"))
        self.assertRejected(lambda v: v.__setitem__("implementation_permissions", "permissions"))


if __name__ == "__main__":
    unittest.main()
