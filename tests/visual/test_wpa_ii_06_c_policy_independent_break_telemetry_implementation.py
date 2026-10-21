from __future__ import annotations

import copy
import importlib.util
import json
import sys
import unittest
from pathlib import Path


ROOT = Path(__file__).resolve().parents[2]
VALIDATOR_PATH = ROOT / "scripts/validate_wpa_ii_06_c_policy_independent_break_telemetry_implementation.py"
RECEIPT_PATH = ROOT / "docs/rules/wpa-ii-06-c-policy-independent-break-telemetry-implementation-receipt-v1.json"

SPEC = importlib.util.spec_from_file_location("wpa_ii_06_c_telemetry_validator", VALIDATOR_PATH)
assert SPEC and SPEC.loader
MODULE = importlib.util.module_from_spec(SPEC)
sys.modules[SPEC.name] = MODULE
SPEC.loader.exec_module(MODULE)


class WpaIi06CPolicyIndependentBreakTelemetryTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls) -> None:
        cls.value = json.loads(RECEIPT_PATH.read_text(encoding="utf-8"))

    def assertRejected(self, mutate) -> None:
        value = copy.deepcopy(self.value)
        mutate(value)
        self.assertTrue(MODULE.validate(value, check_files=False))

    def test_canonical_receipt_passes_live_validation(self) -> None:
        self.assertEqual([], MODULE.validate(self.value, check_files=True))

    def test_contract_digest_is_frozen(self) -> None:
        self.assertEqual(MODULE.EXPECTED_CONTRACT_SHA,
                         self.value["implementation_contract_sha256"])
        self.assertEqual(MODULE.EXPECTED_CONTRACT_SHA,
                         MODULE.canonical_sha(self.value["implementation_contract"]))

    def test_rejects_schema_status_or_gate_drift(self) -> None:
        self.assertRejected(lambda value: value.__setitem__("schema", "v2"))
        self.assertRejected(lambda value: value.__setitem__("status", "IMPLEMENTED"))
        self.assertRejected(lambda value: value["design_gate"].__setitem__("sha256", "0" * 64))

    def test_rejects_scope_expansion_or_cmake_claim(self) -> None:
        self.assertRejected(lambda value: value["implementation_contract"]
                            ["change_scope"]["production_files_changed"]
                            .append("src/Billiards/physics.cpp"))
        self.assertRejected(lambda value: value["implementation_contract"]
                            ["change_scope"].__setitem__("cmake_changed_by_task", True))

    def test_rejects_physics_or_product_mutation_claim(self) -> None:
        self.assertRejected(lambda value: value["implementation_contract"]
                            ["change_scope"].__setitem__("physics_producer_changed", True))
        self.assertRejected(lambda value: value["implementation_contract"]
                            ["change_scope"].__setitem__("product_session_changed", True))

    def test_rejects_hidden_or_broadened_predecessor_drift(self) -> None:
        self.assertRejected(lambda value: value["implementation_contract"]
                            ["predecessor_design_gate_live_validation"]
                            ["exact_errors"].append("source binding bytes/path changed: product_session"))
        self.assertRejected(lambda value: value["implementation_contract"]
                            ["predecessor_design_gate_live_validation"]
                            .__setitem__("predecessor_gate_or_validator_modified_to_hide_drift", True))

    def test_rejects_loss_of_identity_or_canonical_dedup(self) -> None:
        self.assertRejected(lambda value: value["implementation_contract"]
                            ["canonical_fact"].__setitem__("exact_duplicate_rows_removed", False))
        self.assertRejected(lambda value: value["implementation_contract"]
                            ["canonical_fact"].__setitem__("legacy_distinct_scalar_is_unique_object_id_projection", False))

    def test_rejects_failed_step_or_zero_step_false_completeness(self) -> None:
        self.assertRejected(lambda value: value["implementation_contract"]
                            ["origin_and_completeness"].__setitem__("failed_step_advances_tick_or_mutates_facts", True))
        self.assertRejected(lambda value: value["implementation_contract"]
                            ["origin_and_completeness"].__setitem__("zero_succeeded_steps_complete", True))

    def test_rejects_non_authoritative_origin_or_incomplete_acceptance(self) -> None:
        self.assertRejected(lambda value: value["implementation_contract"]
                            ["origin_and_completeness"].__setitem__("authoritative_origin", "direct"))
        self.assertRejected(lambda value: value["implementation_contract"]
                            ["origin_and_completeness"].__setitem__("incomplete_authoritative_break_fails_rule_validation", False))

    def test_rejects_jaw_policy_guess(self) -> None:
        self.assertRejected(lambda value: value["implementation_contract"]
                            ["policy_independence"].__setitem__("cushion_surface_policy", "RESOLVED"))
        self.assertRejected(lambda value: value["implementation_contract"]
                            ["policy_independence"].__setitem__("selected_cushion_surface_policy", "straight_and_jaw"))

    def test_rejects_solver_predicate_guess(self) -> None:
        self.assertRejected(lambda value: value["implementation_contract"]
                            ["policy_independence"].__setitem__("physics_contact_validity_policy", "RESOLVED"))
        self.assertRejected(lambda value: value["implementation_contract"]
                            ["policy_independence"].__setitem__("selected_physics_contact_predicate", "positive_impulse"))

    def test_rejects_cue_ball_pocket_policy_guess(self) -> None:
        self.assertRejected(lambda value: value["implementation_contract"]
                            ["policy_independence"].__setitem__("pocketed_ball_scope_policy", "RESOLVED"))
        self.assertRejected(lambda value: value["implementation_contract"]
                            ["policy_independence"].__setitem__("selected_pocketed_ball_scope", "any_ball"))

    def test_rejects_scalar_reinterpretation_or_product_promotion(self) -> None:
        self.assertRejected(lambda value: value["implementation_contract"]
                            ["policy_independence"].__setitem__("adjudicator_still_uses_existing_distinct_scalar", False))
        self.assertRejected(lambda value: value["implementation_contract"]
                            ["policy_independence"].__setitem__("telemetry_closure_promotes_product_claim", True))

    def test_rejects_audit_or_ui_exposure_promotion(self) -> None:
        self.assertRejected(lambda value: value["implementation_contract"]
                            ["audit_and_product_boundary"].__setitem__("audit_status", "IMPLEMENTED"))
        self.assertRejected(lambda value: value["implementation_contract"]
                            ["audit_and_product_boundary"].__setitem__("rules_decision_ui_product_exposed", True))
        self.assertRejected(lambda value: value["implementation_contract"]
                            ["audit_and_product_boundary"].__setitem__("product_reachable", True))

    def test_rejects_gpu_readiness_or_policy_claim(self) -> None:
        self.assertRejected(lambda value: value["implementation_contract"]
                            ["forbidden_and_absent"].__setitem__("gpu_cocoa_or_opengl_executed", True))
        self.assertRejected(lambda value: value["implementation_contract"]
                            ["forbidden_and_absent"].__setitem__("readiness_generated", True))
        self.assertRejected(lambda value: value["implementation_contract"]
                            ["forbidden_and_absent"].__setitem__("official_policy_invented", True))

    def test_rejects_cpu_failure_or_binary_substitution(self) -> None:
        self.assertRejected(lambda value: value["implementation_contract"]
                            ["cpu_results"][0].__setitem__("exit_code", 1))
        self.assertRejected(lambda value: value["implementation_contract"]
                            ["cpu_results"][1].__setitem__("sha256", "0" * 64))

    def test_rejects_source_binding_drift_or_unsafe_path(self) -> None:
        self.assertRejected(lambda value: value["source_bindings"]
                            ["collector_after"].__setitem__("sha256", "0" * 64))
        self.assertRejected(lambda value: value["source_bindings"]
                            ["audit_v3_unchanged"].__setitem__("path", "../../audit.json"))

    def test_rejects_extra_top_level_or_missing_binding(self) -> None:
        self.assertRejected(lambda value: value.__setitem__("authorized", True))
        self.assertRejected(lambda value: value["source_bindings"].pop("physics_producer_unchanged"))

    def test_rejects_non_object_bad_input(self) -> None:
        self.assertTrue(MODULE.validate(None, check_files=False))
        self.assertTrue(MODULE.validate([], check_files=False))


if __name__ == "__main__":
    unittest.main()
