#!/usr/bin/env python3
"""Validate the design-only WPA-II-08 continuous crossing telemetry gate."""

from __future__ import annotations

import argparse
import hashlib
import json
from pathlib import Path
from typing import Any


ROOT = Path(__file__).resolve().parents[1]
DEFAULT_GATE = ROOT / "docs/rules/wpa-ii-08-exact-continuous-crossing-telemetry-implementation-design-gate-v1.json"
TOP_LEVEL_KEYS = {
    "schema", "status", "authority_bindings", "scope_and_authority_boundary",
    "canonical_physics_contract", "whole_tick_time_contract",
    "collector_and_shot_facts_contract", "runtime_lifecycle_contract",
    "rollback_dedupe_idempotency_contract", "implementation_allowlist",
    "required_fixtures", "deferred_all_behind_lineage", "source_bindings",
    "evidence_and_completion_boundary", "claims",
}
DIGESTS = {
    "authority_bindings": "95b6d6d890d39dbe1ecc5f74b7eef53590b7d2ed8d642676f6cc10b86cc56468",
    "scope_and_authority_boundary": "4da8b784e8af0cb529bc1952b875e6cbf5ea4acf943ac1ab3d468c0b40043bac",
    "canonical_physics_contract": "e9acac4fb30eb441dd4fb3c8e31f34df697c45431384e7c472526adaca7e4af6",
    "whole_tick_time_contract": "054464c4f524eef31254a7cd0bebdcc9309202d51f8f4ada03091095c796ef9f",
    "collector_and_shot_facts_contract": "2e7dfc40ad761df3ea5a70e37c0126e9faedb951815b92adc40b56f8b7a7b8d6",
    "runtime_lifecycle_contract": "24fa1433c567c4e6b5abcf78cc430232bb5094a4de2a7d1cc7d6b0c30fd3ebde",
    "rollback_dedupe_idempotency_contract": "fa3110ace3014bd53bd2c8ea4fdfe5b27c7e43c9933ca7bf6d3f8a33604de8fb",
    "implementation_allowlist": "1ea9d351879db384eef163fff1eda05e52b9fe53d21264301b17c9d82976850e",
    "required_fixtures": "2ecfa67cebb1dacdd174638b52017116bb4312f6f8031fec6f5b76ca07eca5a4",
    "deferred_all_behind_lineage": "1f8712c436fa1f4886c623deb608b2bb368017360c77bd50400bd844ee57bc32",
    "source_bindings": "2b29106df4c7b208b7a370d22d175d9f57a278d44bc457cfcb21c9d13cc59de3",
    "evidence_and_completion_boundary": "6c7f0748d5594b8f092105c7cc577daa249c137d900fc806a5a2bdcbb2e68bc9",
    "claims": "8a72aec9f4cf811386b0c235d7993732bb765ee2f36353db758491b5cb70c07a",
}
PRODUCTION_ALLOWLIST = [
    "src/Billiards/surface_motion.h", "src/Billiards/surface_motion.cpp",
    "src/Billiards/physics.h", "src/Billiards/physics_telemetry.h",
    "src/Billiards/physics.cpp", "src/Billiards/rules/chinese_eight_ball_rules.h",
    "src/Billiards/rules/chinese_eight_ball_rules.cpp",
    "src/Billiards/rules/shot_fact_collector.h",
    "src/Billiards/rules/shot_fact_collector.cpp", "src/Billiards/game_runtime.h",
    "src/Billiards/game_runtime.cpp",
]
TEST_ALLOWLIST = [
    "tests/surface_motion_tests.cpp", "tests/physics_tests.cpp",
    "tests/physics_telemetry_tests.cpp", "tests/shot_fact_collector_tests.cpp",
    "tests/chinese_eight_ball_rules_tests.cpp", "tests/rules_runtime_integration_tests.cpp",
]
POSITIVE_FIXTURES = [
    "CROSSING_IN_EARLIER_TICK_THAN_CONTACT",
    "SAME_TICK_CROSSING_STRICTLY_BEFORE_CONTACT",
    "CROSSING_BEFORE_CUSHION_THEN_RETURN_CONTACT",
    "SLIDING_QUADRATIC_CROSSING_BEFORE_CONTACT",
    "SLIDING_TO_ROLLING_PIECEWISE_CROSSING_BEFORE_CONTACT",
    "ON_LINE_OBJECT_DIRECT_CONTACT_WITHOUT_CROSSING",
    "FOOT_SIDE_OBJECT_DIRECT_CONTACT_WITHOUT_CROSSING",
    "EXACT_DUPLICATE_SUCCESSFUL_TICK_AND_RECORD_ZERO_DELTA",
]
NEGATIVE_FIXTURES = [
    "SAME_TICK_CONTACT_BEFORE_CROSSING", "SAME_TIME_CROSSING_AND_CONTACT_UNORDERED",
    "ENDPOINTS_CROSS_BUT_INTERMEDIATE_CUSHION_ORDER_UNATTESTED",
    "FAILED_PHYSICS_STEP_CROSSING_DISCARDED_CONTEXT_PRESERVED",
    "MISSING_OR_INCOMPLETE_TICK_ATTESTATION_FAILS_CLOSED",
    "NONFINITE_PLANE_OR_CROSSING_RECORD_FAILS_CLOSED",
    "CONFLICTING_DUPLICATE_TICK_OR_RECORD_ZERO_PARTIAL_DELTA",
    "DIRECT_OR_SYNTHETIC_FACTS_CANNOT_CLAIM_CROSSING",
    "REJECTED_LAUNCH_WRONG_ACTOR_OR_RESET_CANNOT_LEAK_STALE_EVIDENCE",
]


def sha256(data: bytes) -> str:
    return hashlib.sha256(data).hexdigest()


def canonical_digest(value: Any) -> str:
    return sha256(json.dumps(value, sort_keys=True, separators=(",", ":"),
        ensure_ascii=False).encode("utf-8"))


def resolve(raw: Any) -> Path | None:
    if not isinstance(raw, str):
        return None
    path = Path(raw)
    if path.is_absolute() or ".." in path.parts:
        return None
    result = (ROOT / path).resolve()
    return result if result.is_relative_to(ROOT.resolve()) else None


def binding_ok(binding: Any) -> bool:
    if not isinstance(binding, dict) or set(binding) != {"path", "sha256"}:
        return False
    path = resolve(binding.get("path"))
    return path is not None and path.is_file() and sha256(path.read_bytes()) == binding.get("sha256")


def validate(value: Any, check_files: bool = True) -> list[str]:
    if not isinstance(value, dict):
        return ["root_not_object"]
    errors: list[str] = []
    if set(value) != TOP_LEVEL_KEYS:
        errors.append("top_level_keys")
    if value.get("schema") != "wpa_ii_08_exact_continuous_crossing_telemetry_implementation_design_gate_v1":
        errors.append("schema")
    if value.get("status") != "DESIGN_COMPLETE_AWAITING_ROOT_IMPLEMENTATION_APPROVAL_NO_PRODUCTION_MUTATION":
        errors.append("status")
    for key, digest in DIGESTS.items():
        if key not in value or canonical_digest(value.get(key)) != digest:
            errors.append(f"contract_digest:{key}")

    authority = value.get("authority_bindings")
    selected = authority.get("root_selected_policy") if isinstance(authority, dict) else None
    blocker = authority.get("feasibility_blocker") if isinstance(authority, dict) else None
    if not isinstance(selected, dict) or selected.get("sha256") != \
            "fff40b72776b4381eae82044421432c139606d300f7214fb87e63e36eeaa770f" or \
            selected.get("row_id") != "WPA-II-08" or \
            selected.get("decision_id") != "M4-POL-08-HEAD-STRING-CROSSING" or \
            selected.get("selected_option_id") != "CENTER_CROSSING_EXACT_CONTINUOUS_ORDER" or \
            selected.get("authorized_stage") != "B1_TESTS_FIRST_CPU_DIRECT_CODE" or \
            selected.get("root_selected") is not True:
        errors.append("root_selection_binding")
    if not isinstance(blocker, dict) or blocker.get("sha256") != \
            "f10525b49619c2a67e6ae0530da1c0eb8b80a54a0d64e134e42158a64ea3095d" or \
            blocker.get("blocker_class") != "NEW_PHYSICS_TELEMETRY_AND_COLLECTOR_PROVENANCE_REQUIRED" or \
            blocker.get("superseded_by_design_only") is not False or \
            blocker.get("superseded_only_after_verified_implementation") is not True:
        errors.append("blocker_binding")

    scope = value.get("scope_and_authority_boundary")
    if not isinstance(scope, dict) or scope.get("implementation_lane") != \
            "POLICY_SELECTED_TELEMETRY_FOUNDATION_ONLY" or scope.get("product_owner") != "none in this implementation gate":
        errors.append("scope_owner")
    for key in ("solver_decides_rule_legality", "collector_or_product_infers_from_sampled_endpoints",
                "all_behind_respot_or_product_action_in_scope", "global_rules_ui_in_scope"):
        if not isinstance(scope, dict) or scope.get(key) is not False:
            errors.append(f"scope_false:{key}")

    physics = value.get("canonical_physics_contract")
    request = physics.get("observation_request") if isinstance(physics, dict) else None
    segment = physics.get("surface_motion_analytic_segment") if isinstance(physics, dict) else None
    candidate = physics.get("crossing_candidate") if isinstance(physics, dict) else None
    record = physics.get("crossing_record") if isinstance(physics, dict) else None
    attestation = physics.get("per_tick_attestation") if isinstance(physics, dict) else None
    request_fields = ["observation_id", "ball_index", "axis", "plane_coordinate_cm", "direction"]
    if not isinstance(request, dict) or request.get("type") != "PhysicsPlaneObservationRequest" or \
            request.get("fields") != request_fields or request.get("rule_meaning_in_physics_layer") is not False:
        errors.append("observation_request")
    runtime_values = request.get("wpa_runtime_values") if isinstance(request, dict) else None
    expected_runtime = {"observation_id": "wpa_ii_08_head_string_cue_center", "ball_index": 0,
        "axis": "Z", "plane_coordinate_source": "FROZEN_COMMITTED_BEHIND_HEAD_STRING_OFFER",
        "direction": "NEGATIVE_TO_POSITIVE", "reference_point": "BALL_CENTER"}
    if runtime_values != expected_runtime:
        errors.append("observation_runtime_values")
    segment_fields = ["ball_index", "segment_ordinal", "motion_state",
        "whole_tick_start_offset_seconds", "duration_seconds", "start_position_cm",
        "end_position_cm", "start_velocity_cm_s", "constant_acceleration_cm_s2",
        "analytic_path_complete"]
    if not isinstance(segment, dict) or segment.get("type") != "SurfaceMotionAnalyticSegment" or \
            segment.get("fields") != segment_fields or \
            segment.get("piecewise_split_required_at_sliding_to_rolling_or_stop_transition") is not True or \
            segment.get("segments_are_emitted_by_the_same_integrator_that_mutates_ball_state") is not True or \
            segment.get("endpoint_linear_interpolation_used_for_crossing") is not False:
        errors.append("surface_motion_segment")
    if not isinstance(candidate, dict) or \
            candidate.get("equation") != "axis_position(t)=start_position+start_velocity*t+0.5*constant_acceleration*t^2" or \
            candidate.get("linear_fallback_only_when_axis_acceleration_is_exactly_zero") is not True or \
            candidate.get("required_sign_change") != "signed_distance_before < 0 and signed_distance_after > 0" or \
            candidate.get("required_derivative_direction") != "axis_velocity_at_root > 0" or \
            candidate.get("touch_tangent_or_reverse_crossing_is_not_candidate") is not True or \
            candidate.get("earliest_valid_root_only") is not True or \
            candidate.get("nonfinite_or_ambiguous_root_invalidates_tick_attestation") is not True:
        errors.append("crossing_candidate")
    record_fields = ["observation_id", "ball_index", "axis", "plane_coordinate_cm", "direction",
        "whole_tick_time_of_impact_seconds", "segment_ordinal", "signed_distance_before_cm",
        "signed_distance_after_cm", "axis_velocity_at_crossing_cm_s", "origin", "complete"]
    if not isinstance(record, dict) or record.get("type") != "PhysicsPlaneCrossingRecord" or \
            record.get("fields") != record_fields or \
            record.get("origin") != "AUTHORITATIVE_CONTINUOUS_SURFACE_INTEGRATOR" or \
            record.get("stable_dedupe_key") != ["observation_id", "ball_index",
                "whole_tick_time_of_impact_seconds", "segment_ordinal", "direction"] or \
            "conflicting roots invalidate completeness" not in str(record.get("segment_boundary_duplicate_policy")):
        errors.append("crossing_record")
    if not isinstance(attestation, dict) or attestation.get("type") != "PhysicsPlaneObservationAttestation" or \
            attestation.get("exactly_one_per_valid_request_per_successful_tick") is not True or \
            attestation.get("no_crossing_with_complete_coverage_is_distinguishable_from_missing_telemetry") is not True or \
            attestation.get("failed_step_publishes_authoritative_attestation") is not False:
        errors.append("per_tick_attestation")

    whole = value.get("whole_tick_time_contract")
    expected_whole = {
        "internal_recursive_parameter": "whole_tick_offset_seconds", "top_level_offset_seconds": 0.0,
        "prefix_segment_start_offset": "whole_tick_offset_seconds",
        "prefix_contact_time": "whole_tick_offset_seconds + local_toi_seconds",
        "recursive_tail_offset": "whole_tick_offset_seconds + local_toi_seconds",
        "tail_contact_time_is_rebased_before_telemetry_merge": True,
        "surface_motion_segment_offsets_are_rebased_before_telemetry_merge": True,
        "event_and_segment_ordinals_are_strictly_monotonic_across_recursion": True,
        "all_times_must_be_finite_and_within_closed_whole_tick_interval": True,
        "legacy_segment_relative_contact_toi_may_remain_as_separate_noncanonical_field": True,
        "canonical_order_uses_whole_tick_time_only": True,
    }
    if whole != expected_whole:
        errors.append("whole_tick_rebase")

    collector = value.get("collector_and_shot_facts_contract")
    begin = collector.get("begin_context") if isinstance(collector, dict) else None
    contact = collector.get("first_contact_evidence") if isinstance(collector, dict) else None
    crossing = collector.get("crossing_evidence") if isinstance(collector, dict) else None
    order = collector.get("total_order") if isinstance(collector, dict) else None
    if not isinstance(begin, dict) or begin.get("only_successful_behind_head_string_placement_commit_may_arm") is not True or \
            "head_string_world_z_cm" not in begin.get("fields", []):
        errors.append("collector_begin_context")
    if not isinstance(contact, dict) or contact.get("canonical_key") != ["physics_tick",
            "whole_tick_time_of_impact_seconds", "solver_event_id", "physics_contact_ordinal",
            "object_ball_id"] or contact.get("contact_completeness_required") is not True:
        errors.append("first_contact_evidence")
    if not isinstance(crossing, dict) or crossing.get("canonical_key") != ["physics_tick",
            "whole_tick_time_of_impact_seconds", "segment_ordinal", "observation_id"] or \
            crossing.get("origin_required") != "AUTHORITATIVE_CONTINUOUS_SURFACE_INTEGRATOR" or \
            crossing.get("every_consumed_successful_tick_requires_complete_matching_attestation_until_first_contact") is not True:
        errors.append("crossing_evidence")
    if not isinstance(order, dict) or order.get("same_tick_comparator") != \
            "crossing.whole_tick_time_of_impact_seconds < first_contact.whole_tick_time_of_impact_seconds" or \
            order.get("exact_equal_time") != "UNORDERED_FAIL_CLOSED" or \
            order.get("missing_or_incomplete_side") != "UNORDERED_FAIL_CLOSED" or \
            order.get("epsilon_or_quantized_tie_breaker") is not False or \
            order.get("solver_event_or_array_order_may_override_equal_time") is not False:
        errors.append("total_order")
    expected_facts = ["behindHeadStringFirstContactRestrictionActive", "headStringWorldZCm",
        "cueCenterPlaneCrossingEvents", "firstCueObjectContactEvidence",
        "cueBallCrossedHeadStringBeforeFirstContact", "headStringCrossingEvidenceOrigin",
        "headStringCrossingEvidenceComplete"]
    if not isinstance(collector, dict) or collector.get("shot_facts_fields") != expected_facts or \
            collector.get("direct_or_synthetic_facts_may_claim_complete_crossing") is not False or \
            collector.get("legal_object_on_line_or_foot_side_direct_contact_needs_crossing") is not False or \
            collector.get("behind_object_direct_contact_without_complete_before_order") != "FAIL_CLOSED_WRONG_FIRST_CONTACT":
        errors.append("shot_facts_contract")

    runtime = value.get("runtime_lifecycle_contract")
    if not isinstance(runtime, dict) or runtime.get("arm_point") != \
            "successful authoritative BehindHeadString placement ACK only" or \
            runtime.get("accepted_launch_begins_collector_with_frozen_context") is not True or \
            runtime.get("successful_physics_ticks_only_feed_crossing_evidence") is not True or \
            runtime.get("rack_or_rerack_without_match_reset_clears_context") is not False or \
            runtime.get("other_player_shot_may_consume_context") is not False or \
            runtime.get("direct_or_synthetic_adjudication_while_context_active") != \
                "reject incomplete evidence without consuming context":
        errors.append("runtime_lifecycle")
    if not isinstance(runtime, dict) or "discard tick telemetry/collector delta" not in str(runtime.get("failed_physics_step")) or \
            "preserve armed restriction for retry" not in str(runtime.get("failed_physics_step")):
        errors.append("failed_step_rollback")

    rollback = value.get("rollback_dedupe_idempotency_contract")
    required_true = ("physics_update_same_state_controls_context_produces_byte_identical_records",
        "collector_exact_duplicate_successful_tick_digest_is_no_op",
        "collector_same_tick_conflicting_digest_is_rejected_without_partial_delta",
        "duplicate_crossing_record_with_same_stable_key_and_payload_is_collapsed",
        "duplicate_crossing_record_with_same_key_different_payload_marks_incomplete")
    required_false = ("failed_step_changes_collector_or_runtime_restriction",
        "malformed_attestation_changes_authoritative_match_or_ball_state",
        "adjudication_failure_consumes_restriction", "partial_origin_completeness_or_order_commit_allowed")
    for key in required_true:
        if not isinstance(rollback, dict) or rollback.get(key) is not True:
            errors.append(f"rollback_true:{key}")
    for key in required_false:
        if not isinstance(rollback, dict) or rollback.get(key) is not False:
            errors.append(f"rollback_false:{key}")

    allowlist = value.get("implementation_allowlist")
    if not isinstance(allowlist, dict) or allowlist.get("production_files") != PRODUCTION_ALLOWLIST or \
            allowlist.get("test_files") != TEST_ALLOWLIST:
        errors.append("implementation_allowlist")
    for key in ("new_production_file_allowed", "cmake_change_allowed",
                "modern_product_session_or_ui_allowed", "all_behind_respot_authority_allowed"):
        if not isinstance(allowlist, dict) or allowlist.get(key) is not False:
            errors.append(f"allowlist_false:{key}")

    fixtures = value.get("required_fixtures")
    if not isinstance(fixtures, dict) or fixtures.get("positive") != POSITIVE_FIXTURES or \
            fixtures.get("negative") != NEGATIVE_FIXTURES or fixtures.get("positive_count") != 8 or \
            fixtures.get("negative_count") != 9 or fixtures.get("all_fixtures_are_headless_cpu") is not True or \
            fixtures.get("mock_only_proof_sufficient") is not False:
        errors.append("required_fixtures")

    deferred = value.get("deferred_all_behind_lineage")
    if not isinstance(deferred, dict) or len(deferred.get("blocker_all_behind_positive_fixtures_deferred", [])) != 3 or \
            len(deferred.get("blocker_all_behind_negative_fixtures_deferred", [])) != 3 or \
            deferred.get("requires_separate_post_telemetry_implementation_gate") is not True or \
            deferred.get("product_reachability_remains_false") is not True:
        errors.append("deferred_all_behind")

    sources = value.get("source_bindings")
    if not isinstance(sources, dict) or len(sources) != 18:
        errors.append("source_bindings")
    elif check_files:
        for key, binding in sources.items():
            if not binding_ok(binding):
                errors.append(f"source_drift:{key}")

    boundary = value.get("evidence_and_completion_boundary")
    for key in ("implementation_authorized_by_this_gate", "audit_v5_modified_or_promoted_here",
                "wpa_ii_08_row_closed_here", "gpu_or_readiness_authorized_here"):
        if not isinstance(boundary, dict) or boundary.get(key) is not False:
            errors.append(f"boundary_false:{key}")
    if not isinstance(boundary, dict) or boundary.get("root_must_revalidate_gate_before_authorizing_tests_first_patch") is not True:
        errors.append("root_revalidation_required")

    claims = value.get("claims")
    for key in ("selected_policy_bound", "successor_telemetry_design_complete"):
        if not isinstance(claims, dict) or claims.get(key) is not True:
            errors.append(f"claim_true:{key}")
    for key in ("implementation_approved", "production_or_test_source_modified",
                "continuous_crossing_telemetry_implemented", "all_behind_respot_implemented",
                "product_action_reachable", "audit_v5_modified_or_promoted", "wpa_ii_08_row_closed",
                "cmake_modified", "gpu_executed", "readiness_or_authorization_created"):
        if not isinstance(claims, dict) or claims.get(key) is not False:
            errors.append(f"false_completion:{key}")

    if check_files:
        for binding, label in ((selected, "root_selection"), (blocker, "blocker")):
            if not binding_ok({"path": binding.get("path"), "sha256": binding.get("sha256")}
                    if isinstance(binding, dict) else binding):
                errors.append(f"{label}_drift")
        try:
            selected_value = json.loads(resolve(selected["path"]).read_text(encoding="utf-8"))
            row = next(item for item in selected_value["decisions"] if item["row_id"] == "WPA-II-08")
            if row.get("selected_option_id") != "CENTER_CROSSING_EXACT_CONTINUOUS_ORDER" or \
                    row.get("root_selected") is not True or row.get("row_closed_by_selection") is not False:
                errors.append("root_selection_row_drift")
        except (OSError, KeyError, StopIteration, TypeError, json.JSONDecodeError, AttributeError):
            errors.append("root_selection_parse")
        future_tokens = ("PhysicsPlaneObservationRequest", "PhysicsPlaneCrossingRecord",
            "cueCenterPlaneCrossingEvents", "cueBallCrossedHeadStringBeforeFirstContact",
            "headStringCrossingEvidenceComplete")
        for relative in PRODUCTION_ALLOWLIST:
            text = (ROOT / relative).read_text(encoding="utf-8")
            if any(token in text for token in future_tokens):
                errors.append(f"implementation_unexpected:{relative}")
        product_path = ROOT / "src/Billiards/render/modern/modern_product_session.cpp"
        if product_path.is_file():
            product = product_path.read_text(encoding="utf-8")
            for token in ("rules.ball_in_hand.request_nearest_legal_respot",
                          "rules.ball_in_hand.choose_equidistant_respot_ball"):
                if token in product:
                    errors.append("all_behind_product_action_unexpected")
                    break
    return errors


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("gate", nargs="?", default=str(DEFAULT_GATE))
    parser.add_argument("--no-file-checks", action="store_true")
    args = parser.parse_args()
    try:
        value = json.loads(Path(args.gate).read_text(encoding="utf-8"))
    except (OSError, json.JSONDecodeError) as exc:
        print(f"INVALID: {exc}")
        return 1
    errors = validate(value, check_files=not args.no_file_checks)
    if errors:
        print("INVALID: " + ", ".join(errors))
        return 1
    print("PASS_WPA_II_08_EXACT_CONTINUOUS_CROSSING_TELEMETRY_IMPLEMENTATION_DESIGN_GATE")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
