from __future__ import annotations

import copy
import importlib.util
import json
import sys
import unittest
from pathlib import Path


ROOT = Path(__file__).resolve().parents[2]
VALIDATOR_PATH = ROOT / "scripts/validate_wpa_ii_08_exact_continuous_crossing_telemetry_implementation_design_gate.py"
GATE_PATH = ROOT / "docs/rules/wpa-ii-08-exact-continuous-crossing-telemetry-implementation-design-gate-v1.json"

SPEC = importlib.util.spec_from_file_location("wpa_ii_08_crossing_telemetry_gate_validator", VALIDATOR_PATH)
assert SPEC and SPEC.loader
MODULE = importlib.util.module_from_spec(SPEC)
sys.modules[SPEC.name] = MODULE
SPEC.loader.exec_module(MODULE)


class WpaIi08CrossingTelemetryImplementationDesignGateTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls) -> None:
        cls.value = json.loads(GATE_PATH.read_text(encoding="utf-8"))

    def assertRejected(self, mutate) -> None:
        value = copy.deepcopy(self.value)
        mutate(value)
        self.assertTrue(MODULE.validate(value, check_files=False))

    def test_canonical_gate_passes_live_validation(self) -> None:
        self.assertEqual([], MODULE.validate(self.value, check_files=True))

    def test_rejects_schema_status_or_top_level_expansion(self) -> None:
        self.assertRejected(lambda value: value.__setitem__("schema", "v2"))
        self.assertRejected(lambda value: value.__setitem__("status", "IMPLEMENTED"))
        self.assertRejected(lambda value: value.__setitem__("authorization", True))

    def test_rejects_root_policy_binding_drift(self) -> None:
        self.assertRejected(lambda value: value["authority_bindings"]["root_selected_policy"].__setitem__(
            "selected_option_id", "LEADING_EDGE_SAMPLED"))
        self.assertRejected(lambda value: value["authority_bindings"]["root_selected_policy"].__setitem__(
            "root_selected", False))
        self.assertRejected(lambda value: value["authority_bindings"]["root_selected_policy"].__setitem__(
            "sha256", "0" * 64))

    def test_rejects_blocker_rewrite_or_false_supersession(self) -> None:
        self.assertRejected(lambda value: value["authority_bindings"]["feasibility_blocker"].__setitem__(
            "sha256", "0" * 64))
        self.assertRejected(lambda value: value["authority_bindings"]["feasibility_blocker"].__setitem__(
            "superseded_by_design_only", True))
        self.assertRejected(lambda value: value["authority_bindings"]["feasibility_blocker"].__setitem__(
            "superseded_only_after_verified_implementation", False))

    def test_rejects_authority_scope_expansion_or_sampled_inference(self) -> None:
        self.assertRejected(lambda value: value["scope_and_authority_boundary"].__setitem__(
            "solver_decides_rule_legality", True))
        self.assertRejected(lambda value: value["scope_and_authority_boundary"].__setitem__(
            "collector_or_product_infers_from_sampled_endpoints", True))
        self.assertRejected(lambda value: value["scope_and_authority_boundary"].__setitem__(
            "all_behind_respot_or_product_action_in_scope", True))

    def test_rejects_plane_request_reference_axis_source_or_direction_drift(self) -> None:
        runtime = "wpa_runtime_values"
        self.assertRejected(lambda value: value["canonical_physics_contract"]["observation_request"][runtime].__setitem__(
            "reference_point", "LEADING_EDGE"))
        self.assertRejected(lambda value: value["canonical_physics_contract"]["observation_request"][runtime].__setitem__(
            "axis", "X"))
        self.assertRejected(lambda value: value["canonical_physics_contract"]["observation_request"][runtime].__setitem__(
            "plane_coordinate_source", "LIVE_TABLE_SAMPLE"))
        self.assertRejected(lambda value: value["canonical_physics_contract"]["observation_request"][runtime].__setitem__(
            "direction", "EITHER"))

    def test_rejects_surface_segment_field_loss_or_endpoint_interpolation(self) -> None:
        segment = "surface_motion_analytic_segment"
        self.assertRejected(lambda value: value["canonical_physics_contract"][segment]["fields"].remove(
            "whole_tick_start_offset_seconds"))
        self.assertRejected(lambda value: value["canonical_physics_contract"][segment].__setitem__(
            "piecewise_split_required_at_sliding_to_rolling_or_stop_transition", False))
        self.assertRejected(lambda value: value["canonical_physics_contract"][segment].__setitem__(
            "segments_are_emitted_by_the_same_integrator_that_mutates_ball_state", False))
        self.assertRejected(lambda value: value["canonical_physics_contract"][segment].__setitem__(
            "endpoint_linear_interpolation_used_for_crossing", True))

    def test_rejects_crossing_root_equation_or_sign_direction_drift(self) -> None:
        candidate = "crossing_candidate"
        self.assertRejected(lambda value: value["canonical_physics_contract"][candidate].__setitem__(
            "equation", "linear-endpoint-lerp"))
        self.assertRejected(lambda value: value["canonical_physics_contract"][candidate].__setitem__(
            "required_sign_change", "touches plane"))
        self.assertRejected(lambda value: value["canonical_physics_contract"][candidate].__setitem__(
            "required_derivative_direction", "any"))
        self.assertRejected(lambda value: value["canonical_physics_contract"][candidate].__setitem__(
            "linear_fallback_only_when_axis_acceleration_is_exactly_zero", False))

    def test_rejects_tangent_reverse_late_root_or_nonfinite_acceptance(self) -> None:
        candidate = "crossing_candidate"
        for key in ("touch_tangent_or_reverse_crossing_is_not_candidate", "earliest_valid_root_only",
                    "nonfinite_or_ambiguous_root_invalidates_tick_attestation"):
            self.assertRejected(lambda value, key=key: value["canonical_physics_contract"][candidate].__setitem__(key, False))

    def test_rejects_crossing_record_origin_key_or_conflict_policy_drift(self) -> None:
        record = "crossing_record"
        self.assertRejected(lambda value: value["canonical_physics_contract"][record].__setitem__(
            "origin", "SAMPLED_ENDPOINT"))
        self.assertRejected(lambda value: value["canonical_physics_contract"][record]["stable_dedupe_key"].pop())
        self.assertRejected(lambda value: value["canonical_physics_contract"][record].__setitem__(
            "segment_boundary_duplicate_policy", "keep both"))

    def test_rejects_missing_attestation_or_failed_step_publication(self) -> None:
        attestation = "per_tick_attestation"
        self.assertRejected(lambda value: value["canonical_physics_contract"][attestation].__setitem__(
            "exactly_one_per_valid_request_per_successful_tick", False))
        self.assertRejected(lambda value: value["canonical_physics_contract"][attestation].__setitem__(
            "no_crossing_with_complete_coverage_is_distinguishable_from_missing_telemetry", False))
        self.assertRejected(lambda value: value["canonical_physics_contract"][attestation].__setitem__(
            "failed_step_publishes_authoritative_attestation", True))

    def test_rejects_recursive_tail_or_segment_non_rebase(self) -> None:
        self.assertRejected(lambda value: value["whole_tick_time_contract"].__setitem__(
            "recursive_tail_offset", "local_toi_seconds"))
        self.assertRejected(lambda value: value["whole_tick_time_contract"].__setitem__(
            "tail_contact_time_is_rebased_before_telemetry_merge", False))
        self.assertRejected(lambda value: value["whole_tick_time_contract"].__setitem__(
            "surface_motion_segment_offsets_are_rebased_before_telemetry_merge", False))

    def test_rejects_nonmonotonic_or_segment_relative_canonical_order(self) -> None:
        self.assertRejected(lambda value: value["whole_tick_time_contract"].__setitem__(
            "event_and_segment_ordinals_are_strictly_monotonic_across_recursion", False))
        self.assertRejected(lambda value: value["whole_tick_time_contract"].__setitem__(
            "canonical_order_uses_whole_tick_time_only", False))
        self.assertRejected(lambda value: value["whole_tick_time_contract"].__setitem__(
            "all_times_must_be_finite_and_within_closed_whole_tick_interval", False))

    def test_rejects_first_contact_key_or_complete_event_set_drift(self) -> None:
        contact = "first_contact_evidence"
        self.assertRejected(lambda value: value["collector_and_shot_facts_contract"][contact]["canonical_key"].remove(
            "whole_tick_time_of_impact_seconds"))
        self.assertRejected(lambda value: value["collector_and_shot_facts_contract"][contact].__setitem__(
            "first_event_object_set", "first array element"))
        self.assertRejected(lambda value: value["collector_and_shot_facts_contract"][contact].__setitem__(
            "contact_completeness_required", False))

    def test_rejects_crossing_provenance_or_tick_coverage_drift(self) -> None:
        crossing = "crossing_evidence"
        self.assertRejected(lambda value: value["collector_and_shot_facts_contract"][crossing].__setitem__(
            "origin_required", "RUNTIME_ESTIMATE"))
        self.assertRejected(lambda value: value["collector_and_shot_facts_contract"][crossing].__setitem__(
            "every_consumed_successful_tick_requires_complete_matching_attestation_until_first_contact", False))
        self.assertRejected(lambda value: value["collector_and_shot_facts_contract"][crossing]["canonical_key"].pop())

    def test_rejects_equal_time_acceptance_epsilon_or_array_override(self) -> None:
        order = "total_order"
        self.assertRejected(lambda value: value["collector_and_shot_facts_contract"][order].__setitem__(
            "exact_equal_time", "BEFORE"))
        self.assertRejected(lambda value: value["collector_and_shot_facts_contract"][order].__setitem__(
            "epsilon_or_quantized_tie_breaker", True))
        self.assertRejected(lambda value: value["collector_and_shot_facts_contract"][order].__setitem__(
            "solver_event_or_array_order_may_override_equal_time", True))

    def test_rejects_shot_facts_loss_or_synthetic_claim(self) -> None:
        self.assertRejected(lambda value: value["collector_and_shot_facts_contract"]["shot_facts_fields"].remove(
            "headStringCrossingEvidenceComplete"))
        self.assertRejected(lambda value: value["collector_and_shot_facts_contract"].__setitem__(
            "direct_or_synthetic_facts_may_claim_complete_crossing", True))
        self.assertRejected(lambda value: value["collector_and_shot_facts_contract"].__setitem__(
            "behind_object_direct_contact_without_complete_before_order", "ALLOW"))

    def test_rejects_runtime_arming_consumption_or_reset_drift(self) -> None:
        self.assertRejected(lambda value: value["runtime_lifecycle_contract"].__setitem__(
            "arm_point", "drag preview"))
        self.assertRejected(lambda value: value["runtime_lifecycle_contract"].__setitem__(
            "rack_or_rerack_without_match_reset_clears_context", True))
        self.assertRejected(lambda value: value["runtime_lifecycle_contract"].__setitem__(
            "other_player_shot_may_consume_context", True))

    def test_rejects_failed_step_leak_or_context_consumption(self) -> None:
        self.assertRejected(lambda value: value["runtime_lifecycle_contract"].__setitem__(
            "failed_physics_step", "publish partial telemetry and clear context"))
        self.assertRejected(lambda value: value["rollback_dedupe_idempotency_contract"].__setitem__(
            "failed_step_changes_collector_or_runtime_restriction", True))
        self.assertRejected(lambda value: value["rollback_dedupe_idempotency_contract"].__setitem__(
            "adjudication_failure_consumes_restriction", True))

    def test_rejects_dedupe_conflict_or_partial_commit_regression(self) -> None:
        self.assertRejected(lambda value: value["rollback_dedupe_idempotency_contract"].__setitem__(
            "collector_exact_duplicate_successful_tick_digest_is_no_op", False))
        self.assertRejected(lambda value: value["rollback_dedupe_idempotency_contract"].__setitem__(
            "collector_same_tick_conflicting_digest_is_rejected_without_partial_delta", False))
        self.assertRejected(lambda value: value["rollback_dedupe_idempotency_contract"].__setitem__(
            "partial_origin_completeness_or_order_commit_allowed", True))

    def test_rejects_allowlist_loss_reorder_or_scope_expansion(self) -> None:
        self.assertRejected(lambda value: value["implementation_allowlist"]["production_files"].pop())
        self.assertRejected(lambda value: value["implementation_allowlist"]["test_files"].reverse())
        self.assertRejected(lambda value: value["implementation_allowlist"].__setitem__(
            "cmake_change_allowed", True))
        self.assertRejected(lambda value: value["implementation_allowlist"].__setitem__(
            "modern_product_session_or_ui_allowed", True))

    def test_rejects_fixture_count_order_or_mock_only_proof(self) -> None:
        self.assertRejected(lambda value: value["required_fixtures"]["positive"].pop())
        self.assertRejected(lambda value: value["required_fixtures"]["negative"].reverse())
        self.assertRejected(lambda value: value["required_fixtures"].__setitem__("positive_count", 7))
        self.assertRejected(lambda value: value["required_fixtures"].__setitem__("mock_only_proof_sufficient", True))

    def test_rejects_deferred_all_behind_promotion_or_fixture_loss(self) -> None:
        self.assertRejected(lambda value: value["deferred_all_behind_lineage"][
            "blocker_all_behind_positive_fixtures_deferred"].pop())
        self.assertRejected(lambda value: value["deferred_all_behind_lineage"].__setitem__(
            "requires_separate_post_telemetry_implementation_gate", False))
        self.assertRejected(lambda value: value["deferred_all_behind_lineage"].__setitem__(
            "product_reachability_remains_false", False))

    def test_rejects_source_binding_loss_path_or_hash_drift(self) -> None:
        self.assertRejected(lambda value: value["source_bindings"].pop("physics_cpp"))
        self.assertRejected(lambda value: value["source_bindings"]["physics_cpp"].__setitem__(
            "path", "../physics.cpp"))
        self.assertRejected(lambda value: value["source_bindings"]["cmake_unchanged"].__setitem__(
            "sha256", "0" * 64))

    def test_rejects_implementation_authorization_or_false_completion(self) -> None:
        self.assertRejected(lambda value: value["evidence_and_completion_boundary"].__setitem__(
            "implementation_authorized_by_this_gate", True))
        self.assertRejected(lambda value: value["evidence_and_completion_boundary"].__setitem__(
            "root_must_revalidate_gate_before_authorizing_tests_first_patch", False))
        for key in ("implementation_approved", "continuous_crossing_telemetry_implemented",
                    "all_behind_respot_implemented", "product_action_reachable", "wpa_ii_08_row_closed",
                    "cmake_modified", "gpu_executed", "readiness_or_authorization_created"):
            self.assertRejected(lambda value, key=key: value["claims"].__setitem__(key, True))

    def test_malformed_inputs_fail_closed(self) -> None:
        for value in (None, [], "gate", 1):
            self.assertTrue(MODULE.validate(value, check_files=False))
        for key, replacement in (("authority_bindings", []), ("canonical_physics_contract", None),
                                 ("collector_and_shot_facts_contract", "facts"),
                                 ("source_bindings", []), ("claims", None)):
            self.assertRejected(lambda value, key=key, replacement=replacement: value.__setitem__(key, replacement))


if __name__ == "__main__":
    unittest.main()
