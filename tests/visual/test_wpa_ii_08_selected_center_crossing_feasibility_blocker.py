from __future__ import annotations

import copy
import importlib.util
import json
import sys
import unittest
from pathlib import Path


ROOT = Path(__file__).resolve().parents[2]
VALIDATOR_PATH = ROOT / "scripts/validate_wpa_ii_08_selected_center_crossing_feasibility_blocker.py"
GATE_PATH = ROOT / "docs/rules/wpa-ii-08-selected-center-crossing-exact-continuous-order-feasibility-blocker-v1.json"

SPEC = importlib.util.spec_from_file_location("wpa_ii_08_crossing_blocker_validator", VALIDATOR_PATH)
assert SPEC and SPEC.loader
MODULE = importlib.util.module_from_spec(SPEC)
sys.modules[SPEC.name] = MODULE
SPEC.loader.exec_module(MODULE)


class WpaIi08SelectedCenterCrossingFeasibilityBlockerTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls) -> None:
        cls.value = json.loads(GATE_PATH.read_text(encoding="utf-8"))

    def assertRejected(self, mutate) -> None:
        value = copy.deepcopy(self.value)
        mutate(value)
        self.assertTrue(MODULE.validate(value, check_files=False))

    def test_canonical_gate_passes_live_validation(self) -> None:
        self.assertEqual([], MODULE.validate(self.value, check_files=True))

    def test_rejects_schema_status_or_extra_top_level(self) -> None:
        self.assertRejected(lambda value: value.__setitem__("schema", "v2"))
        self.assertRejected(lambda value: value.__setitem__("status", "IMPLEMENTED"))
        self.assertRejected(lambda value: value.__setitem__("authorization", True))

    def test_rejects_selection_or_false_closure(self) -> None:
        self.assertRejected(lambda value: value["selection_binding"].__setitem__("root_selected", False))
        self.assertRejected(lambda value: value["selection_binding"].__setitem__(
            "selected_option_id", "LEADING_EDGE_SAMPLED"))
        self.assertRejected(lambda value: value["selection_binding"].__setitem__(
            "selection_resolves_geometry_but_does_not_create_missing_telemetry", False))
        self.assertRejected(lambda value: value["selection_binding"].__setitem__(
            "selection_alone_closes_row", True))

    def test_rejects_predecessor_rewrite_or_fake_telemetry_resolution(self) -> None:
        self.assertRejected(lambda value: value["predecessor_bindings"]["design_gap_gate"].__setitem__(
            "historical_source_bindings_are_not_rewritten", False))
        self.assertRejected(lambda value: value["predecessor_bindings"]["design_gap_gate"].__setitem__(
            "previous_telemetry_pending_remains_open", False))

    def test_rejects_reference_point_or_strict_order_drift(self) -> None:
        self.assertRejected(lambda value: value["selected_policy_contract"].__setitem__(
            "crossing_reference_point", "LEADING_EDGE"))
        self.assertRejected(lambda value: value["selected_policy_contract"].__setitem__(
            "crossing_relation", "TOUCHES_OR_CROSSES"))
        self.assertRejected(lambda value: value["selected_policy_contract"].__setitem__(
            "required_order", "SAME_TICK"))

    def test_rejects_unordered_or_incomplete_acceptance(self) -> None:
        self.assertRejected(lambda value: value["selected_policy_contract"].__setitem__(
            "same_time_or_unordered_result", "ALLOW"))
        self.assertRejected(lambda value: value["selected_policy_contract"].__setitem__(
            "incomplete_evidence_result", "ALLOW"))

    def test_rejects_on_line_or_foot_side_direct_contact_regression(self) -> None:
        self.assertRejected(lambda value: value["selected_policy_contract"].__setitem__(
            "legal_object_on_head_string_direct_contact_legal", False))
        self.assertRejected(lambda value: value["selected_policy_contract"].__setitem__(
            "legal_object_foot_side_direct_contact_legal", False))

    def test_rejects_leading_edge_endpoint_or_product_reclassification(self) -> None:
        for key in ("leading_edge_inference_forbidden", "sampled_endpoint_approximation_forbidden",
                    "product_reclassification_forbidden"):
            self.assertRejected(lambda value, key=key: value["selected_policy_contract"].__setitem__(key, False))

    def test_rejects_invented_solver_crossing_event(self) -> None:
        solver = "continuous_solver"
        self.assertRejected(lambda value: value["feasibility_findings"][solver].__setitem__(
            "head_string_plane_crossing_candidate_exists", True))
        self.assertRejected(lambda value: value["feasibility_findings"][solver].__setitem__(
            "head_string_plane_crossing_telemetry_record_exists", True))
        self.assertRejected(lambda value: value["feasibility_findings"][solver].__setitem__(
            "cue_center_crossing_origin_or_completeness_attestation_exists", True))

    def test_rejects_false_whole_tick_rebase_or_order_claim(self) -> None:
        solver = "continuous_solver"
        self.assertRejected(lambda value: value["feasibility_findings"][solver].__setitem__(
            "tail_contact_toi_rebased_to_whole_tick_absolute_time", True))
        self.assertRejected(lambda value: value["feasibility_findings"][solver].__setitem__(
            "solver_event_id_orders_contact_batches_but_cannot_order_an_absent_crossing", False))

    def test_rejects_invented_surface_motion_provenance(self) -> None:
        telemetry = "physics_step_telemetry"
        self.assertRejected(lambda value: value["feasibility_findings"][telemetry].__setitem__(
            "surface_motion_steps_have_segment_start_end_positions", True))
        self.assertRejected(lambda value: value["feasibility_findings"][telemetry].__setitem__(
            "surface_motion_steps_have_segment_duration_and_absolute_offset", True))
        self.assertRejected(lambda value: value["feasibility_findings"][telemetry].__setitem__(
            "before_after_cue_center_endpoints_are_sufficient_for_strict_same_step_order", True))

    def test_rejects_invented_collector_order_or_crossing(self) -> None:
        collector = "shot_fact_collector"
        for key in ("begin_receives_behind_head_string_scope_or_plane",
                    "consume_preserves_first_contact_within_tick_toi",
                    "consume_records_crossing_event",
                    "finish_attests_crossing_origin_completeness_and_order"):
            self.assertRejected(lambda value, key=key: value["feasibility_findings"][collector].__setitem__(key, True))

    def test_rejects_invented_shot_fact_fields(self) -> None:
        for key in self.value["feasibility_findings"]["shot_facts"]:
            self.assertRejected(lambda value, key=key: value["feasibility_findings"]["shot_facts"].__setitem__(key, True))

    def test_rejects_false_feasibility_or_inference(self) -> None:
        self.assertRejected(lambda value: value["feasibility_findings"].__setitem__(
            "same_step_exact_order_provable_from_current_contract", True))
        self.assertRejected(lambda value: value["feasibility_findings"].__setitem__(
            "implementation_may_infer_from_endpoints_or_contact_tick", True))
        self.assertRejected(lambda value: value["feasibility_findings"].__setitem__(
            "blocker_class", "NONE"))

    def test_rejects_partial_all_behind_authority_or_product_exposure(self) -> None:
        for key in ("dedicated_parent_authority_exists", "request_and_commit_schema_exists",
                    "frozen_state_hash_revalidation_exists", "tie_choice_authority_exists",
                    "replacement_core_g_child_transaction_exists", "narrow_product_action_exists",
                    "partial_product_exposure_before_complete_facts_allowed"):
            self.assertRejected(lambda value, key=key: value["all_behind_respot_boundary"].__setitem__(key, True))
        self.assertRejected(lambda value: value["all_behind_respot_boundary"].__setitem__(
            "production_work_stopped_at_selected_crossing_feasibility_gate", False))

    def test_rejects_successor_gate_scope_or_fixture_loss(self) -> None:
        self.assertRejected(lambda value: value["required_successor_design_gate"].__setitem__(
            "root_approval_required_before_production", False))
        self.assertRejected(lambda value: value["required_successor_design_gate"]["must_define"].pop())
        self.assertRejected(lambda value: value["required_successor_design_gate"]["potential_production_allowlist"].pop())
        self.assertRejected(lambda value: value["required_successor_design_gate"]["required_positive_fixtures"].pop())
        self.assertRejected(lambda value: value["required_successor_design_gate"]["required_negative_fixtures"].pop())

    def test_rejects_source_path_hash_or_binding_loss(self) -> None:
        self.assertRejected(lambda value: value["source_bindings"]["physics"].__setitem__("sha256", "0" * 64))
        self.assertRejected(lambda value: value["source_bindings"]["collector_cpp"].__setitem__(
            "path", "../shot_fact_collector.cpp"))
        self.assertRejected(lambda value: value["source_bindings"].pop("surface_motion_contract"))

    def test_rejects_cpu_failure_or_forbidden_mutation(self) -> None:
        self.assertRejected(lambda value: value["cpu_read_only_verification"]["binaries"][0].__setitem__(
            "exit_code", 1))
        self.assertRejected(lambda value: value["cpu_read_only_verification"]["binaries"].pop())
        self.assertRejected(lambda value: value["cpu_read_only_verification"].__setitem__(
            "ctest_subset", {"passed": 2, "failed": 1}))
        self.assertRejected(lambda value: value["cpu_read_only_verification"].__setitem__(
            "production_or_cmake_source_modified_in_task", True))
        self.assertRejected(lambda value: value["cpu_read_only_verification"].__setitem__(
            "gpu_cocoa_or_opengl_executed", True))

    def test_rejects_false_completion_or_behavior_claims(self) -> None:
        for key in ("same_step_exact_order_currently_provable", "crossing_telemetry_implemented",
                    "all_behind_respot_authority_implemented", "all_behind_product_action_reachable",
                    "wpa_ii_08_row_closed", "audit_v5_modified_or_promoted",
                    "global_rules_decision_ui_exposed", "product_behavior_changed",
                    "production_source_modified", "cmake_modified", "gpu_executed",
                    "readiness_or_authorization_created"):
            self.assertRejected(lambda value, key=key: value["claims"].__setitem__(key, True))

    def test_malformed_inputs_fail_closed(self) -> None:
        for value in (None, [], "gate", 1):
            self.assertTrue(MODULE.validate(value, check_files=False))
        self.assertRejected(lambda value: value.__setitem__("selection_binding", []))
        self.assertRejected(lambda value: value.__setitem__("selected_policy_contract", None))
        self.assertRejected(lambda value: value.__setitem__("feasibility_findings", "findings"))
        self.assertRejected(lambda value: value.__setitem__("all_behind_respot_boundary", []))
        self.assertRejected(lambda value: value.__setitem__("source_bindings", None))
        self.assertRejected(lambda value: value.__setitem__("claims", []))


if __name__ == "__main__":
    unittest.main()
