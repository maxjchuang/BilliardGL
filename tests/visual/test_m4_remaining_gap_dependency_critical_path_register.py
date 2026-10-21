from __future__ import annotations

import copy
import json
from pathlib import Path
import unittest

from scripts.validate_m4_remaining_gap_dependency_critical_path_register import validate


ROOT = Path(__file__).resolve().parents[2]
REGISTER = ROOT / "docs/rules/m4-remaining-gap-dependency-critical-path-register-v1.json"


class M4RemainingGapDependencyCriticalPathRegisterTests(unittest.TestCase):
    def setUp(self) -> None:
        self.value = json.loads(REGISTER.read_text(encoding="utf-8"))

    def assertRejected(self, mutate) -> None:
        value = copy.deepcopy(self.value)
        mutate(value)
        self.assertTrue(validate(value, check_files=False))

    def entry(self, value, row_id):
        return next(item for item in value["entries"] if item["row_id"] == row_id)

    def test_canonical_final_and_live_inputs_pass(self) -> None:
        self.assertEqual([], validate(self.value, check_files=True))

    def test_rejects_schema_status_or_scope_drift(self) -> None:
        self.assertRejected(lambda v: v.__setitem__("schema", "v2"))
        self.assertRejected(lambda v: v.__setitem__("status", "FINAL_POLICY_SELECTED"))
        self.assertRejected(lambda v: v.__setitem__("scope", "implementation"))

    def test_rejects_each_input_binding_drift(self) -> None:
        for key in self.value["input_bindings"]:
            self.assertRejected(lambda v, key=key: v["input_bindings"][key].__setitem__("sha256", "0" * 64))
        self.assertRejected(lambda v: v["input_bindings"].pop("audit_v5"))

    def test_rejects_row_loss_duplicate_or_order_drift(self) -> None:
        self.assertRejected(lambda v: v["entries"].pop())
        self.assertRejected(lambda v: v["entries"].append(copy.deepcopy(v["entries"][0])))
        self.assertRejected(lambda v: v["entries"].reverse())

    def test_rejects_stable_row_or_decision_id_drift(self) -> None:
        self.assertRejected(lambda v: v["entries"][0].__setitem__("row_id", "WPA-II-06-C-RENAMED"))
        self.assertRejected(lambda v: v["entries"][0].__setitem__("decision_id", "M4-POL-RENAMED"))

    def test_rejects_any_policy_becoming_selected(self) -> None:
        self.assertRejected(lambda v: self.entry(v, "WPA-II-06-C").__setitem__(
            "root_policy_selection", "SELECTED"))
        self.assertRejected(lambda v: v["classification_contract"].__setitem__("policy_selection_made", True))
        self.assertRejected(lambda v: v["classification_contract"].__setitem__(
            "external_referee_protocol_authorized", True))
        self.assertRejected(lambda v: v["classification_contract"].__setitem__("new_physics_authorized", True))

    def test_rejects_code_closure_classification_drift(self) -> None:
        self.assertRejected(lambda v: self.entry(v, "WPA-II-06-C").__setitem__(
            "direct_code_closure", "NO_REQUIRES_NEW_PHYSICS_OR_EXTERNAL_AUTHORITY"))
        self.assertRejected(lambda v: self.entry(v, "WPA-II-16").__setitem__(
            "direct_code_closure", "YES_AFTER_LISTED_PREREQUISITES"))

    def test_rejects_referee_or_physics_classification_drift(self) -> None:
        self.assertRejected(lambda v: self.entry(v, "WPA-II-12-7").__setitem__(
            "external_referee_input_protocol", "NOT_REQUIRED"))
        self.assertRejected(lambda v: self.entry(v, "WPA-II-12-3").__setitem__(
            "new_physics", "NOT_REQUIRED"))
        self.assertRejected(lambda v: self.entry(v, "WPA-II-15-1").__setitem__(
            "new_physics", "REQUIRED_UNIMPLEMENTED"))

    def test_rejects_gpu_ta_or_human_gate_drift(self) -> None:
        self.assertRejected(lambda v: self.entry(v, "WPA-II-16").__setitem__(
            "gpu", "NOT_REQUIRED_FOR_INTERNAL_CLOSURE"))
        self.assertRejected(lambda v: self.entry(v, "WPA-II-19").__setitem__(
            "ta", "NOT_REQUIRED_FOR_INTERNAL_CLOSURE"))
        self.assertRejected(lambda v: self.entry(v, "WPA-II-21").__setitem__(
            "human", "NOT_REQUIRED_FOR_INTERNAL_CLOSURE"))

    def test_rejects_dependency_or_batch_drift(self) -> None:
        self.assertRejected(lambda v: self.entry(v, "WPA-II-17")["depends_on"].pop())
        self.assertRejected(lambda v: self.entry(v, "WPA-II-06-C").__setitem__(
            "minimum_execution_batch", "B3-PHYSICS-LANES"))
        self.assertRejected(lambda v: v["minimum_execution_batches"][0].__setitem__("may_start_now", True))
        self.assertRejected(lambda v: v["minimum_execution_batches"].reverse())

    def test_rejects_dependency_node_state_promotion(self) -> None:
        self.assertRejected(lambda v: v["dependency_nodes"][1].__setitem__("state", "SATISFIED"))
        self.assertRejected(lambda v: v["dependency_nodes"].pop())

    def test_rejects_critical_path_reclassification(self) -> None:
        self.assertRejected(lambda v: v["critical_path_summary"]["new_physics_long_paths"].pop())
        self.assertRejected(lambda v: v["critical_path_summary"].__setitem__(
            "convergence_row", "WPA-II-06-C"))
        self.assertRejected(lambda v: self.entry(v, "WPA-II-16").__setitem__(
            "critical_path_class", "FAST_LANE_POLICY_BLOCKED"))

    def test_rejects_wpa_19_audit_override_promotion(self) -> None:
        self.assertRejected(lambda v: self.entry(v, "WPA-II-19").__setitem__(
            "closure_note", "audit v5 is now m4_required=true"))

    def test_rejects_false_completion_or_authorization_claims(self) -> None:
        for key in ("production_modified", "cmake_modified", "readiness_or_authorization_created",
                    "gpu_or_cocoa_or_opengl_executed", "audit_v5_modified_or_promoted",
                    "implementation_authorized"):
            self.assertRejected(lambda v, key=key: v["claims"].__setitem__(key, True))
        self.assertRejected(lambda v: v["claims"].__setitem__("all_policy_selections_unselected", False))
        self.assertRejected(lambda v: v["claims"].__setitem__("deferred_rows_are_not_closed", False))

    def test_rejects_nonempty_contract_text_drift(self) -> None:
        self.assertRejected(lambda v: self.entry(v, "WPA-II-06-D").__setitem__(
            "closure_note", "different but nonempty"))
        self.assertRejected(lambda v: v["minimum_execution_batches"][1].__setitem__(
            "reason", "different but nonempty"))

    def test_malformed_values_fail_closed(self) -> None:
        for value in (None, [], "register", 1):
            self.assertTrue(validate(value, check_files=False))
        self.assertRejected(lambda v: v.__setitem__("entries", "rows"))
        self.assertRejected(lambda v: v.__setitem__("minimum_execution_batches", "batches"))
        self.assertRejected(lambda v: v.__setitem__("claims", "claims"))


if __name__ == "__main__":
    unittest.main()
