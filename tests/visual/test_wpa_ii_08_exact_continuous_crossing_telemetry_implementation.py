from __future__ import annotations

import copy
import importlib.util
import json
import sys
import unittest
from pathlib import Path


ROOT = Path(__file__).resolve().parents[2]
VALIDATOR_PATH = ROOT / "scripts/validate_wpa_ii_08_exact_continuous_crossing_telemetry_implementation.py"
RECEIPT_PATH = ROOT / "docs/rules/wpa-ii-08-exact-continuous-crossing-telemetry-implementation-receipt-v1.json"

SPEC = importlib.util.spec_from_file_location("wpa_ii_08_crossing_implementation_validator", VALIDATOR_PATH)
assert SPEC and SPEC.loader
MODULE = importlib.util.module_from_spec(SPEC)
sys.modules[SPEC.name] = MODULE
SPEC.loader.exec_module(MODULE)


class WpaIi08ExactContinuousCrossingTelemetryImplementationTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls) -> None:
        cls.value = json.loads(RECEIPT_PATH.read_text(encoding="utf-8"))

    def assertRejected(self, mutate) -> None:
        value = copy.deepcopy(self.value)
        mutate(value)
        self.assertTrue(MODULE.validate(value, check_files=False))

    def test_canonical_receipt_passes_live_validation(self) -> None:
        self.assertEqual([], MODULE.validate(self.value, check_files=True))

    def test_rejects_schema_status_or_top_level_drift(self) -> None:
        self.assertRejected(lambda value: value.__setitem__("schema", "v2"))
        self.assertRejected(lambda value: value.__setitem__("status", "WPA_II_08_COMPLETE"))
        self.assertRejected(lambda value: value.__setitem__("extra", True))

    def test_rejects_design_gate_authority_drift(self) -> None:
        self.assertRejected(lambda value: value["authority_bindings"]["implementation_design_gate"].__setitem__(
            "sha256", "0" * 64))
        self.assertRejected(lambda value: value["authority_bindings"]["implementation_design_gate"].__setitem__(
            "root_authorized_exact_allowlist_tests_first_implementation", False))

    def test_rejects_root_selected_policy_drift(self) -> None:
        self.assertRejected(lambda value: value["authority_bindings"]["root_selected_policy"].__setitem__(
            "selected_option_id", "LEADING_EDGE_SAMPLED_APPROXIMATION"))
        self.assertRejected(lambda value: value["authority_bindings"]["root_selected_policy"].__setitem__(
            "sha256", "0" * 64))

    def test_rejects_blocker_lineage_or_false_all_behind_resolution(self) -> None:
        self.assertRejected(lambda value: value["authority_bindings"]["feasibility_blocker"].__setitem__(
            "telemetry_blocker_resolved_by_this_receipt", False))
        self.assertRejected(lambda value: value["authority_bindings"]["feasibility_blocker"].__setitem__(
            "all_behind_parent_product_blocker_resolved", True))

    def test_rejects_non_generic_plane_observation(self) -> None:
        self.assertRejected(lambda value: value["implementation_contract"].__setitem__(
            "generic_plane_observation", "head_string_special_case"))

    def test_rejects_surface_segment_provenance_or_split_loss(self) -> None:
        self.assertRejected(lambda value: value["implementation_contract"].__setitem__(
            "surface_motion_provenance", "endpoint_samples"))
        self.assertRejected(lambda value: value["implementation_contract"].__setitem__(
            "segment_boundary_crossing", "ignored"))

    def test_rejects_linear_quadratic_candidate_or_nonfinite_broadening(self) -> None:
        self.assertRejected(lambda value: value["implementation_contract"].__setitem__(
            "continuous_candidate", "linear interpolation only"))

    def test_rejects_recursive_whole_tick_rebasing_loss(self) -> None:
        self.assertRejected(lambda value: value["implementation_contract"].__setitem__(
            "whole_tick_time", "recursive tail local time"))

    def test_rejects_epsilon_tie_break_or_nonstrict_order(self) -> None:
        self.assertRejected(lambda value: value["implementation_contract"].__setitem__(
            "order_policy", "crossing_time <= contact_time with epsilon"))
        self.assertRejected(lambda value: value["implementation_contract"].__setitem__(
            "coverage_tolerance_is_not_an_order_tie_breaker", False))

    def test_rejects_attestation_or_completeness_erosion(self) -> None:
        self.assertRejected(lambda value: value["implementation_contract"].__setitem__(
            "per_tick_attestation", "optional"))
        self.assertRejected(lambda value: value["implementation_contract"].__setitem__(
            "coverage_contiguity_tolerance_seconds", 0.001))

    def test_rejects_collector_origin_dedupe_or_rollback_loss(self) -> None:
        self.assertRejected(lambda value: value["implementation_contract"].__setitem__(
            "collector_origin", "DirectOrSynthetic"))
        self.assertRejected(lambda value: value["implementation_contract"].__setitem__(
            "collector_idempotency", "conflicts overwrite"))
        self.assertRejected(lambda value: value["implementation_contract"].__setitem__(
            "failed_step_rollback", "retain partial telemetry"))

    def test_rejects_runtime_lifecycle_or_rules_bypass(self) -> None:
        self.assertRejected(lambda value: value["implementation_contract"].__setitem__(
            "runtime_lifecycle", "armed by preview"))
        self.assertRejected(lambda value: value["implementation_contract"].__setitem__(
            "direct_or_synthetic_complete_claim_allowed", True))

    def test_rejects_product_reclassification(self) -> None:
        self.assertRejected(lambda value: value["implementation_contract"].__setitem__(
            "product_reclassification_allowed", True))

    def test_rejects_positive_fixture_ids_order_or_count(self) -> None:
        self.assertRejected(lambda value: value["fixture_matrix"]["positive"].reverse())
        self.assertRejected(lambda value: value["fixture_matrix"]["positive"].pop())
        self.assertRejected(lambda value: value["fixture_matrix"].__setitem__("positive_count", 9))

    def test_rejects_negative_fixture_ids_order_or_count(self) -> None:
        self.assertRejected(lambda value: value["fixture_matrix"]["negative"].reverse())
        self.assertRejected(lambda value: value["fixture_matrix"]["negative"].pop())
        self.assertRejected(lambda value: value["fixture_matrix"].__setitem__("negative_count", 8))

    def test_rejects_fixture_evidence_or_headless_claim_drift(self) -> None:
        self.assertRejected(lambda value: value["fixture_matrix"]["positive"][0].__setitem__(
            "evidence", "docs/unexecutable.md"))
        self.assertRejected(lambda value: value["fixture_matrix"].__setitem__("all_headless_cpu", False))

    def test_rejects_source_allowlist_hash_or_aggregate_drift(self) -> None:
        self.assertRejected(lambda value: value["source_bindings"].reverse())
        self.assertRejected(lambda value: value["source_bindings"][0].__setitem__("sha256", "0" * 64))
        self.assertRejected(lambda value: value.__setitem__("implementation_aggregate_sha256", "0" * 64))

    def test_rejects_test_allowlist_or_hash_drift(self) -> None:
        self.assertRejected(lambda value: value["test_source_bindings"].pop())
        self.assertRejected(lambda value: value["test_source_bindings"][0].__setitem__("path", "../test.cpp"))

    def test_rejects_cmake_boundary_or_hash_drift(self) -> None:
        self.assertRejected(lambda value: value["cmake_binding"].__setitem__("modified_in_task", True))
        self.assertRejected(lambda value: value["cmake_binding"].__setitem__("sha256", "0" * 64))

    def test_rejects_required_or_wide_cpu_failure(self) -> None:
        self.assertRejected(lambda value: value["cpu_verification"]["required_six"][0].__setitem__(
            "exit_code", 1))
        self.assertRejected(lambda value: value["cpu_verification"]["wide_regression"].pop())
        self.assertRejected(lambda value: value["cpu_verification"].__setitem__("failed", 1))

    def test_rejects_hidden_cmake_workaround_or_gpu_execution(self) -> None:
        self.assertRejected(lambda value: value["cpu_verification"]["shot_fact_collector_compile_workaround"].__setitem__(
            "cmake_modified_to_hide_gap", True))
        self.assertRejected(lambda value: value["cpu_verification"].__setitem__(
            "gpu_cocoa_opengl_executed", True))

    def test_rejects_all_behind_or_product_scope_expansion(self) -> None:
        for key in ("all_behind_parent_authority_implemented",
                    "all_behind_respot_request_or_commit_implemented",
                    "all_behind_product_action_reachable", "global_rules_ui_exposed",
                    "audit_v5_modified_or_promoted", "wpa_ii_08_row_closed",
                    "fresh_gpu_ta_or_readiness_created"):
            self.assertRejected(lambda value, key=key: value["deferred_boundary"].__setitem__(key, True))
        self.assertRejected(lambda value: value["deferred_boundary"].__setitem__(
            "requires_separate_post_telemetry_gate", False))

    def test_rejects_false_completion_or_loss_of_positive_claims(self) -> None:
        for key in ("all_behind_respot_implemented", "product_action_reachable", "wpa_ii_08_complete",
                    "cmake_modified", "ui_modified", "gpu_executed", "readiness_or_authorization_created"):
            self.assertRejected(lambda value, key=key: value["claims"].__setitem__(key, True))
        for key in ("root_selected_telemetry_foundation_implemented",
                    "exact_continuous_crossing_order_cpu_verified",
                    "failed_step_and_duplicate_boundaries_cpu_verified"):
            self.assertRejected(lambda value, key=key: value["claims"].__setitem__(key, False))

    def test_malformed_inputs_fail_closed(self) -> None:
        for value in (None, [], "receipt", 1):
            self.assertTrue(MODULE.validate(value, check_files=False))
        self.assertRejected(lambda value: value.__setitem__("authority_bindings", []))
        self.assertRejected(lambda value: value.__setitem__("fixture_matrix", None))
        self.assertRejected(lambda value: value.__setitem__("cpu_verification", "pass"))
        self.assertRejected(lambda value: value.__setitem__("claims", []))


if __name__ == "__main__":
    unittest.main()
