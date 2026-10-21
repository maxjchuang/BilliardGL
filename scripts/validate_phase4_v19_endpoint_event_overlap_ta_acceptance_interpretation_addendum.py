#!/usr/bin/env python3
"""Validate the Phase4 V19 endpoint/event TA acceptance interpretation."""

from __future__ import annotations

import hashlib
import json
from pathlib import Path
import sys


ROOT = Path(__file__).resolve().parents[1]
DEFAULT = ROOT / "docs/art/phase4-v19-endpoint-event-overlap-ta-acceptance-interpretation-addendum-v1.json"
SCHEMA = "billiardgl.phase4.v19.endpoint_event_overlap_ta_acceptance_interpretation_addendum.v1"
STATUS = "TA_INTERPRETATION_FROZEN_CP1_NOT_ACCEPTED_NO_PRODUCTION_NO_GPU_NO_AUTH"
BINDINGS = {
    "docs/art/phase4-v19-dedicated-trajectory-inset-minimap-product-decision-and-implementation-design-gate-v1.json": "c7d86d9dcdd4891fac9dd9cbfffe3931561b5098db1df5a4aaec9b50e79af4c7",
    "docs/art/phase4-v19-landscape-orientation-layout-interpretation-correction-gate-v1.json": "cffc93f29bb24f7f9378dee3c17fffd1da1c139745eec17bdb380cf6bda04d13",
    "docs/art/phase4-v19-route-wide-dash-event-semantic-interpretation-correction-gate-v1.json": "de1d3dd818050d42e03025080939002af82395fd42ea9a22d3413aab0efe3441",
}


def sha(path: Path) -> str:
    return hashlib.sha256(path.read_bytes()).hexdigest()


def validate(path: Path = DEFAULT) -> list[str]:
    errors: list[str] = []
    try:
        value = json.loads(path.read_text(encoding="utf-8"))
    except Exception as error:
        return [f"addendum_invalid:{error}"]

    def require(condition: bool, message: str) -> None:
        if not condition:
            errors.append(message)

    require(value.get("schema") == SCHEMA and value.get("status") == STATUS,
            "schema_or_status_invalid")
    require("acceptance interpretation only" in value.get("scope", "") and
            "neither changes production nor accepts CP1" in value.get("scope", ""),
            "scope_invalid")
    observed = {item.get("path"): item.get("sha256") for item in
                value.get("frozen_gate_bindings", [])}
    require(observed == BINDINGS, "gate_binding_inventory_or_sha_invalid")
    for relative, digest in BINDINGS.items():
        target = ROOT / relative
        require(target.is_file() and sha(target) == digest,
                f"gate_missing_or_drift:{relative}")

    decision = value.get("root_decision_binding", {})
    require(decision.get("selected_interpretation") ==
            "BOUNDED_ANALYTIC_CROSSING_WITH_DETERMINISTIC_FINAL_SAMPLE_OWNERSHIP",
            "root_interpretation_invalid")
    statement = decision.get("statement", "")
    require("necessarily crosses" in statement and "two-pixel tail event" in statement and
            "No notch" in statement and "no cadence truncation" in statement,
            "root_statement_incomplete")
    require(decision.get("new_endpoint_geometry_or_product_grammar_selected") is False,
            "false_product_selection")

    reason = value.get("why_zero_geometry_overlap_is_impossible", {})
    require("must cross" in reason.get("topology", "") and
            reason.get("not_a_relaxation_of_identity_or_visibility") is True,
            "topology_reason_invalid")
    require(reason.get("invalid_test_removed_from_acceptance", "").startswith(
            "isolated tail-event core geometry"), "invalid_test_boundary_missing")

    geometry = value.get("locked_endpoint_geometry", {})
    require(geometry.get("predicted_endpoint_halo_l1_band_outer_to_inner_px") == [4.5, 3.5] and
            geometry.get("predicted_endpoint_marker_l1_band_outer_to_inner_px") == [3.5, 2.5],
            "diamond_band_geometry_invalid")
    require(geometry.get("actual_capsule_tangent_by_normal_extent_px") == [7.0, 2.5] and
            geometry.get("actual_capsule_center_offsets_from_canonical_endpoint_px") == [-7.75, 7.75] and
            geometry.get("paired_capsule_midpoint_is_canonical_endpoint") is True,
            "capsule_geometry_invalid")
    require(geometry.get("edge_corner_reference_tail_event_interval_px") == [550.0, 552.0] and
            geometry.get("full_cadence_tail_event_interval_is_retained") is True,
            "tail_event_interval_invalid")
    require(all(geometry.get(key) is False for key in (
        "open_notch_in_closed_diamond_allowed",
        "tail_event_truncation_or_suppression_allowed",
        "endpoint_or_route_relocation_allowed")), "forbidden_geometry_escape_enabled")

    ownership = value.get("deterministic_overlap_ownership_contract", {})
    require(ownership.get("draw_order") == [
        "route-wide predicted dash outer rim",
        "route-wide predicted dash inner halo",
        "route-wide predicted dash core",
        "predicted endpoint halo closed band",
        "predicted endpoint marker closed band",
    ], "draw_order_invalid")
    require(ownership.get("analytic_overlap_samples_are_owned_by_later_endpoint_band") is True and
            ownership.get("hollow_interior_samples_remain_available_to_tail_event_core") is True and
            ownership.get("one_final_nonzero_semantic_owner_per_sample") is True and
            ownership.get("ownership_must_be_top_left_half_open_and_repeatable") is True and
            ownership.get("palette_may_change_color_but_not_topology_or_ownership") is True,
            "ownership_contract_invalid")

    acceptance = value.get("cp1_endpoint_acceptance_delta_not_yet_executed", {})
    require(acceptance.get("palettes") == ["standard", "color_vision_safe"] and
            acceptance.get("sample_profiles") == ["center_1x", "asymmetric_2x", "asymmetric_4x"],
            "palette_or_sample_matrix_invalid")
    require(len(acceptance.get("required_geometry_assertions", [])) == 4 and
            len(acceptance.get("required_resolved_semantic_assertions_per_palette_profile_and_sample", [])) == 5 and
            len(acceptance.get("required_fail_closed_mutations", [])) == 7,
            "acceptance_inventory_invalid")
    resolved = " ".join(acceptance.get(
        "required_resolved_semantic_assertions_per_palette_profile_and_sample", []))
    require("tail dash-event core has nonzero independent final exact-token area" in resolved and
            "endpoint marker band and endpoint halo band each have nonzero" in resolved and
            "not fully consumed" in resolved and "byte-identical" in resolved,
            "resolved_semantic_acceptance_incomplete")
    mutations = " ".join(acceptance.get("required_fail_closed_mutations", []))
    require("notch" in mutations and "truncate" in mutations and
            "consume every tail-event" in mutations and "standard and CVD" in mutations,
            "fail_closed_mutation_inventory_incomplete")
    require(acceptance.get("cp1_accepted") is False and
            acceptance.get("implementation_receipt_bound") is False,
            "premature_cp1_acceptance")

    unchanged = value.get("unchanged_contracts", {})
    require(len(unchanged) == 6 and all(flag is True for flag in unchanged.values()),
            "unchanged_contract_invalid")
    authorization = value.get("authorization_boundary", {})
    require(len(authorization) == 12 and all(flag is False for flag in authorization.values()),
            "authorization_boundary_must_be_all_false")
    return errors


def main() -> int:
    target = Path(sys.argv[1]).resolve() if len(sys.argv) > 1 else DEFAULT
    errors = validate(target)
    if errors:
        for error in errors:
            print("FAIL: " + error, file=sys.stderr)
        return 1
    print("PASS_PHASE4_V19_ENDPOINT_EVENT_OVERLAP_TA_INTERPRETATION")
    print("analytic_overlap=bounded final_ownership=deterministic cp1=false gpu=false")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
