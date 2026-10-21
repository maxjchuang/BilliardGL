#!/usr/bin/env python3
"""Validate the V19 route-wide dash-event semantic correction gate."""

from __future__ import annotations

import hashlib
import json
from pathlib import Path
import sys


ROOT = Path(__file__).resolve().parents[1]
DEFAULT = ROOT / "docs/art/phase4-v19-route-wide-dash-event-semantic-interpretation-correction-gate-v1.json"
SCHEMA = "billiardgl.phase4.v19.route_wide_dash_event_semantic_interpretation_correction_gate.v1"
STATUS = "ROOT_SELECTED_ROUTE_WIDE_DASH_EVENT_SEMANTICS_DESIGN_ONLY_NO_PRODUCTION_NO_CMAKE_NO_GPU_NO_AUTH"
BINDINGS = {
    "docs/art/phase4-v19-dedicated-trajectory-inset-minimap-product-decision-and-implementation-design-gate-v1.json": "c7d86d9dcdd4891fac9dd9cbfffe3931561b5098db1df5a4aaec9b50e79af4c7",
    "docs/art/phase4-v19-landscape-orientation-layout-interpretation-correction-gate-v1.json": "cffc93f29bb24f7f9378dee3c17fffd1da1c139745eec17bdb380cf6bda04d13",
}
OBS_SHA = "bd5ebb32071fb351407995650ed40b2010e343d6d46703a96e20c3a210e735f5"
ROOT_SHA = "97257421080faa7332ef97c39d7a787f73327036306cf9466f32d57c9cd3c133"
ROLES = ["predicted_outer_rim", "predicted_inner_halo", "predicted_dash_core"]
CONTRIBUTOR_FIELDS = ["parentSourceKey", "arcSegmentId", "routeOrderOrdinal", "sourceLocalTStart", "sourceLocalTEnd", "eventRouteDistanceStartPx", "eventRouteDistanceEndPx"]


def sha(path: Path) -> str:
    return hashlib.sha256(path.read_bytes()).hexdigest()


def validate(path: Path = DEFAULT) -> list[str]:
    errors: list[str] = []
    try:
        value = json.loads(path.read_text(encoding="utf-8"))
    except Exception as error:
        return [f"gate_invalid:{error}"]

    def require(condition: bool, message: str) -> None:
        if not condition:
            errors.append(message)

    require(value.get("schema") == SCHEMA and value.get("schema_version") == 1 and value.get("status") == STATUS, "schema_or_status_invalid")
    bindings = {item.get("path"): item.get("sha256") for item in value.get("frozen_bindings", [])}
    require(bindings == BINDINGS, "binding_inventory_or_sha_invalid")
    for relative, digest in BINDINGS.items():
        require((ROOT / relative).is_file() and sha(ROOT / relative) == digest, f"binding_missing_or_drift:{relative}")
    finding = value.get("nonfinal_unsatisfiable_example_binding", {})
    require(hashlib.sha256(finding.get("statement", "").encode()).hexdigest() == finding.get("statement_sha256") == OBS_SHA, "finding_statement_invalid")
    require(finding.get("evidence_kind") == "NONFINAL_CP1_FAIL_CLOSED_REPRODUCIBLE_FINDING_NO_FROZEN_RECEIPT_NO_PASS" and finding.get("layout_profile") == "1920x1080_ui135", "finding_boundary_invalid")
    require(abs(finding.get("projected_interval_px", 0) - 123.22204724409448) < 1e-12 and abs(finding.get("next_arc_residual_dash_on_px", 0) - 0.7779527559055168) < 1e-12, "finding_numeric_invalid")
    require(finding.get("source_or_binary_frozen") is False and finding.get("checkpoint_pass_inferred") is False, "finding_false_authority")
    decision = value.get("root_semantic_correction_decision_binding", {})
    require(hashlib.sha256(decision.get("statement", "").encode()).hexdigest() == decision.get("statement_sha256") == ROOT_SHA and decision.get("selected") is True and decision.get("design_only") is True, "root_decision_invalid")

    supersession = value.get("supersession_contract", {})
    require("per inset core or marker semantic member" in supersession.get("superseded_original_text", "") and "route-wide predicted dash-event core" in supersession.get("replacement", ""), "supersession_text_invalid")
    require(supersession.get("predicted_per_parent_dash_fragment_member_identity_allowed") is False and supersession.get("silent_probe_omission_for_emitted_event_member_allowed") is False, "old_fragment_or_probe_skip_allowed")
    require(supersession.get("all_actual_rail_endpoint_orientation_layout_offscreen_gl_and_future_acceptance_contracts_unchanged") is True and supersession.get("original_gate_may_be_used_without_both_v19_corrections") is False, "supersession_scope_invalid")
    event = value.get("route_wide_dash_event_contract", {})
    require("complete canonical predicted route first" in event.get("generation_order", "") and "without reset" in event.get("generation_order", "") and "maximal connected dash-on interval" in event.get("generation_order", ""), "event_generation_invalid")
    require(event.get("event_roles_in_order") == ROLES and event.get("one_uniform_nonzero_inset_token_per_event_role") is True, "event_roles_or_token_invalid")
    require(event.get("centerline_and_cadence_unchanged") is True and event.get("dash_event_may_reset_at_analytic_source_boundary") is False and event.get("event_may_be_split_only_for_source_identity_or_probe_convenience") is False, "event_cadence_or_split_invalid")
    require(event.get("subpixel_cap_or_extension_across_event_bounds_allowed") is False and event.get("coordinate_probe_or_case_id_may_affect_event_generation") is False, "event_geometry_scope_invalid")

    lineage = value.get("ordered_contributor_interval_lineage_contract", {})
    require(lineage.get("contributor_record_fields_in_order") == CONTRIBUTOR_FIELDS and lineage.get("event_contributors_are_ordered_by_route_then_interval") is True, "contributor_schema_or_order_invalid")
    require(lineage.get("intervals_are_finite_half_open_and_nonoverlapping") is True and lineage.get("event_interval_union_exactly_equals_dash_on_event") is True, "interval_partition_invalid")
    require(lineage.get("every_event_contributor_back_references_one_existing_source_group") is True and lineage.get("every_source_mapping_row_lists_all_ordered_event_references_and_local_intervals") is True, "lineage_backreference_invalid")
    require(lineage.get("foreign_duplicate_missing_reordered_overlapping_or_mixed_generation_contributor_fails_closed") is True and lineage.get("world_to_source_group_mapping_remains_exact") is True and lineage.get("world_visibility_or_camera_projection_may_change_event_eligibility") is False, "lineage_fail_closed_invalid")

    source = value.get("source_group_and_gap_denominator_contract", {})
    require(source.get("source_group_denominator_unchanged") is True and source.get("source_parent_keys_count_and_digest_unchanged_by_this_correction") is True and source.get("every_source_group_has_complete_route_interval_partition") is True, "source_denominator_invalid")
    require("predicted_dash_gap_by_route_wide_cadence" in source.get("dash_gap_interval_record", "") and source.get("gap_only_source_group_remains_in_source_denominator") is True and source.get("gap_only_source_group_may_have_zero_rendered_event_members") is True, "gap_only_lineage_invalid")
    require(source.get("gap_only_is_not_missing_or_clipped") is True and source.get("gap_records_have_no_token_and_no_probe") is True and source.get("source_rows_may_be_deleted_or_merged") is False and source.get("historical_world_81_denominator_or_pass_inherited") is False, "gap_or_source_false_authority")
    probe = value.get("event_semantic_and_probe_denominator_contract", {})
    require("route-wide predicted event role members" in probe.get("semantic_member_denominator", "") and "Exactly one probe" in probe.get("probe_denominator", ""), "event_denominator_invalid")
    require(probe.get("predicted_event_probe_fields") == ["eventKey", "predictedCoreMemberKey", "predictedInnerHaloCompanionMemberKey", "corePhysicalPixel", "haloPhysicalPixel", "fixedPanelBackgroundPhysicalPixel"], "probe_schema_invalid")
    require("1x/2x/4x" in probe.get("probe_resolution", "") and probe.get("one_common_candidate_must_pass_all_required_sample_profiles") is True, "probe_sample_resolution_invalid")
    require("do not split the event" in probe.get("no_candidate_behavior", "") and "omit the probe" in probe.get("no_candidate_behavior", ""), "probe_failure_behavior_invalid")
    require(probe.get("standard_cvd_event_topology_lineage_and_probe_ids_identical") is True and probe.get("ordered_event_count_digest_contributor_digest_semantic_member_count_and_probe_digest_must_be_frozen_by_future_cp1_receipt") is True, "probe_future_freeze_invalid")
    cpu = value.get("cp1_acceptance_delta_not_executed", {})
    require(cpu.get("existing_exact_96_case_order_and_count_unchanged") is True and len(cpu.get("required_positive_fixtures", [])) == 6 and len(cpu.get("required_fail_closed_fixtures", [])) == 8, "cpu_fixture_inventory_invalid")
    require(any("0.778-pixel" in item for item in cpu.get("required_positive_fixtures", [])) and any("gap-only" in item for item in cpu.get("required_fail_closed_fixtures", [])), "cpu_key_fixture_missing")
    require(cpu.get("cpu_checkpoint_pass") is False and cpu.get("implementation_pass") is False, "cpu_false_pass")
    auth = value.get("authorization_boundary", {})
    require(len(auth) == 13 and auth.get("root_semantic_correction_selected") is True and auth.get("semantic_correction_gate_complete") is True and all(flag is False for key, flag in auth.items() if key not in {"root_semantic_correction_selected", "semantic_correction_gate_complete"}), "authorization_boundary_invalid")
    proof = value.get("proof_boundary", {})
    require(len(proof) == 9 and proof.get("design_semantic_interpretation_only") is True and proof.get("nonfinal_example_is_not_a_frozen_receipt") is True and all(flag is False for key, flag in proof.items() if key not in {"design_semantic_interpretation_only", "nonfinal_example_is_not_a_frozen_receipt"}), "proof_boundary_invalid")
    return errors


def main() -> int:
    target = Path(sys.argv[1]).resolve() if len(sys.argv) > 1 else DEFAULT
    errors = validate(target)
    if errors:
        for error in errors:
            print("FAIL: " + error, file=sys.stderr)
        return 1
    print("PASS_PHASE4_V19_ROUTE_WIDE_DASH_EVENT_SEMANTIC_CORRECTION_GATE")
    print("source_denominator=retained dash_event=many_to_one probe=per_event cpu=false production=false gpu=false")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
