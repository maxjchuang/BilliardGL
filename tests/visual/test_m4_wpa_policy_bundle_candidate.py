from __future__ import annotations

import copy
import json
from pathlib import Path
import unittest

from scripts.validate_m4_wpa_policy_bundle_candidate import validate


ROOT = Path(__file__).resolve().parents[2]
CANDIDATE = ROOT / "docs/rules/m4-wpa-policy-bundle-candidate-v1.json"


class M4WpaPolicyBundleCandidateTests(unittest.TestCase):
    def setUp(self) -> None:
        self.value = json.loads(CANDIDATE.read_text(encoding="utf-8"))

    def assertRejected(self, mutate) -> None:
        value = copy.deepcopy(self.value)
        mutate(value)
        self.assertTrue(validate(value, check_files=False))

    def entry(self, value, row_id):
        return next(item for item in value["decisions"] if item["row_id"] == row_id)

    def test_canonical_candidate_and_live_bindings_pass(self) -> None:
        self.assertEqual([], validate(self.value, check_files=True))

    def test_rejects_schema_id_or_version_drift(self) -> None:
        self.assertRejected(lambda v: v.__setitem__("schema", "v2"))
        self.assertRejected(lambda v: v.__setitem__("bundle_id", "approved-bundle"))
        self.assertRejected(lambda v: v.__setitem__("candidate_version", 2))

    def test_rejects_selected_approved_or_authorized_top_level(self) -> None:
        self.assertRejected(lambda v: v.__setitem__("status", "SELECTED"))
        self.assertRejected(lambda v: v.__setitem__("approval_state", "APPROVED"))
        self.assertRejected(lambda v: v.__setitem__("root_selected", True))
        self.assertRejected(lambda v: v.__setitem__("implementation_authorized", True))

    def test_rejects_input_binding_drift(self) -> None:
        self.assertRejected(lambda v: v["input_bindings"]["policy_register"].__setitem__("sha256", "0" * 64))
        self.assertRejected(lambda v: v["input_bindings"]["dependency_register"].__setitem__("sha256", "0" * 64))
        self.assertRejected(lambda v: v["input_bindings"].pop("dependency_register"))

    def test_rejects_decision_loss_duplicate_or_order_drift(self) -> None:
        self.assertRejected(lambda v: v["decisions"].pop())
        self.assertRejected(lambda v: v["decisions"].append(copy.deepcopy(v["decisions"][0])))
        self.assertRejected(lambda v: v["decisions"].reverse())

    def test_rejects_row_or_decision_id_drift(self) -> None:
        self.assertRejected(lambda v: v["decisions"][0].__setitem__("row_id", "WPA-II-06-C-RENAMED"))
        self.assertRejected(lambda v: v["decisions"][0].__setitem__("decision_id", "M4-POL-RENAMED"))

    def test_rejects_any_item_becoming_selected(self) -> None:
        self.assertRejected(lambda v: self.entry(v, "WPA-II-06-C").__setitem__("candidate_state", "SELECTED"))
        self.assertRejected(lambda v: v["claims"].__setitem__("any_item_root_selected", True))
        self.assertRejected(lambda v: v["claims"].__setitem__("root_review_complete", True))

    def test_rejects_nonrecommended_option_substitution(self) -> None:
        self.assertRejected(lambda v: self.entry(v, "WPA-II-06-C").__setitem__(
            "proposed_option_id", "LITERAL_BROAD_JAWS_POSITIVE_IMPULSE_ANY_CAPTURE_PROFILE"))
        self.assertRejected(lambda v: self.entry(v, "WPA-II-16").__setitem__(
            "proposed_option_id", "BUILD_FULL_3D_THEN_ENABLE"))

    def test_rejects_machine_constant_drift(self) -> None:
        self.assertRejected(lambda v: self.entry(v, "WPA-II-06-C")["machine_policy_constants"].__setitem__(
            "jaw_contacts_count", True))
        self.assertRejected(lambda v: self.entry(v, "WPA-II-06-D")["machine_policy_constants"].__setitem__(
            "history_scope", "RACK"))
        self.assertRejected(lambda v: self.entry(v, "WPA-II-09")["machine_policy_constants"].__setitem__(
            "rejected_zero_or_failed_attempt_consumes", True))
        self.assertRejected(lambda v: self.entry(v, "WPA-II-21")["machine_policy_constants"].__setitem__(
            "automatic_stalemate_on_counter_expiry", True))

    def test_rejects_authority_boundary_drift(self) -> None:
        self.assertRejected(lambda v: self.entry(v, "WPA-II-06-E")["authority_boundary"].__setitem__(
            "product_owner", "global Rules UI"))
        self.assertRejected(lambda v: self.entry(v, "WPA-II-12-7")["authority_boundary"].pop("forbidden"))
        self.assertRejected(lambda v: self.entry(v, "WPA-II-19")["authority_boundary"].__setitem__(
            "classification_owner", "ordinary WrongFirstContact"))

    def test_rejects_code_lane_drift(self) -> None:
        self.assertRejected(lambda v: self.entry(v, "WPA-II-12-3")["expected_code_lanes"].pop())
        self.assertRejected(lambda v: self.entry(v, "WPA-II-16")["expected_code_lanes"].append(
            "D7-JUMP-3D-PHYSICS_PROMOTED"))

    def test_rejects_evidence_invalidation_drift(self) -> None:
        self.assertRejected(lambda v: self.entry(v, "WPA-II-06-F").__setitem__(
            "evidence_invalidation", "old evidence remains current"))
        self.assertRejected(lambda v: self.entry(v, "WPA-II-17").__setitem__(
            "evidence_invalidation", ""))

    def test_rejects_global_ui_or_contract_authorization(self) -> None:
        self.assertRejected(lambda v: v["candidate_contract"].__setitem__(
            "global_rules_decision_ui_product_exposed", True))
        self.assertRejected(lambda v: v["candidate_contract"].__setitem__(
            "proposal_does_not_authorize_implementation", False))
        self.assertRejected(lambda v: v["candidate_contract"].__setitem__(
            "deferred_or_dependency_blocked_rows_remain_open", False))

    def test_rejects_review_contract_pretending_selection(self) -> None:
        self.assertRejected(lambda v: v["root_review_contract"].__setitem__(
            "selection_artifact_must_be_separate_and_versioned", False))
        self.assertRejected(lambda v: v["root_review_contract"].__setitem__(
            "implementation_requires_selected_artifact_and_new_allowlist_gate", False))

    def test_rejects_false_claims(self) -> None:
        for key in ("root_review_complete", "any_item_root_selected", "approved", "implementation_authorized",
                    "production_modified", "cmake_modified", "readiness_or_authorization_created",
                    "gpu_or_cocoa_or_opengl_executed"):
            self.assertRejected(lambda v, key=key: v["claims"].__setitem__(key, True))
        self.assertRejected(lambda v: v["claims"].__setitem__(
            "all_options_equal_bound_register_recommendations", False))

    def test_malformed_input_fails_closed(self) -> None:
        for value in (None, [], "candidate", 1):
            self.assertTrue(validate(value, check_files=False))
        self.assertRejected(lambda v: v.__setitem__("decisions", "decisions"))
        self.assertRejected(lambda v: v["decisions"][0].__setitem__("machine_policy_constants", []))
        self.assertRejected(lambda v: v["decisions"][0].__setitem__("authority_boundary", "authority"))
        self.assertRejected(lambda v: v.__setitem__("claims", "claims"))


if __name__ == "__main__":
    unittest.main()
