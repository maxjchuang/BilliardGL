#!/usr/bin/env python3
"""Fail-closed validator for the Phase4 v9 ordinal-1 offline diagnosis."""

from __future__ import annotations

import argparse
import hashlib
import json
from pathlib import Path
import stat


ROOT = Path(__file__).resolve().parents[1]
CANONICAL = ROOT / "docs/art/phase4-final-ownership-v9-ordinal1-offline-diagnostic-v1"
CANONICAL_SCRIPT = ROOT / \
    "scripts/diagnose_phase4_final_ownership_v9_ordinal1_offline.py"

EXPECTED_ARTIFACT_SHA256 = {
    "failure-evidence-receipt-v1.json":
        "6bfcaf2c5ac5602deb557ccca176ebfd478c4183cf9bad6a054b6642f13a39eb",
    "minimal-compositing-style-correction-design-gate-v1.json":
        "e80baf05f0240872ba24b9ca9a6c7daa0d5a51496dde99902014d8ea3391a4f6",
}
EXPECTED_SCRIPT_SHA256 = \
    "810fc328504915b08afac136cdf5ee1e21f442e1816c7fc10b1595d6fa4cd010"
EXPECTED_FROZEN = {
    "docs/art/phase4-training-replay-overlay-v4-final-ownership-targeted11-ready-v9/root-one-shot-gpu-authorization-v9-run1.json":
        "67d06a6f7a8cd25fb38367beb0e8b975eeaf8dd6905dc250ec5ebb511b2d08c0",
    "docs/art/phase4-training-replay-overlay-v4-final-ownership-targeted11-ready-v9/root-one-shot-gpu-authorization-v9-run1.json.consumed":
        "2b20f90a76309fa425264580582f020b94120f0cb102af0949f18516e983e66e",
    "docs/art/phase4-training-replay-overlay-v4-final-ownership-targeted11-output-v9/phase4-final-ownership-targeted11-plan.tsv":
        "3b969b34e52fe2e01e31e34b148c4ce7c211682db65b3c609a43274366c8717d",
    "docs/art/phase4-training-replay-overlay-v4-final-ownership-targeted11-output-v9/results.jsonl":
        "e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855",
    "docs/art/phase4-training-replay-overlay-v4-final-ownership-targeted11-output-v9/results.jsonl.final-ownership.jsonl":
        "3f40581fc4f66861a3a5f0953d6350619ca3195ad0c560f1202c5458dd66f62a",
    "docs/art/phase4-training-replay-overlay-v4-final-ownership-targeted11-output-v9/camera_player_low_1080.ppm":
        "90db92505afaf6d6c2d74193dee646afb73515a42acb3d25fa32b8034d84686c",
    "docs/art/phase4-training-replay-overlay-v4-final-ownership-targeted11-output-v9/phase4-final-ownership-dispatch-failure-receipt-v1.json":
        "597bbf92be859cf0d4e37cbf677140bdafa286cde59412077a75416fcf035f0a",
}
EXPECTED_BINDINGS = {
    "docs/art/phase4-training-replay-overlay-v4-final-ownership-targeted11-ready-v9/targeted11-readiness-v9.json":
        "c3057b9147b232935c29a0a1f046511e53b8e5c97b426822c9c0325d45f196ab",
    "docs/art/phase4-training-replay-overlay-v4-final-ownership-targeted11-ready-v9/source-bindings-v1.json":
        "8718bf638c96be25cc0a54aa204086436352fd7af6ce9b9a7cae6bd41a5ddb12",
    "docs/art/phase4-training-replay-overlay-v4-final-ownership-targeted11-output-v8/results.jsonl.final-ownership.jsonl":
        "48d34fb11a342ffb7ff00f0729396ad48544bcfec4db43470bedd3d92148bd99",
    "docs/art/phase4-final-ownership-v8-ordinal2-offline-diagnostic-v1/minimal-visual-style-correction-design-gate-v1.json":
        "0fe967c8ca0e855c6cb0cd29f745aefd7dfcf8a9ae387eb1522d254753196526",
    "src/Billiards/training/trajectory_overlay_renderer.cpp":
        "3e652402ab8cd5603defb0c54b9ff0951b7d5a292bcc81f9b4429f3877e0aa25",
    "src/Billiards/training/trajectory_overlay_renderer.h":
        "3918037812b177d4713bb4f99ee6ae96d598e023ff8e2f40afceea749d81cec1",
}
OUTPUT = ROOT / \
    "docs/art/phase4-training-replay-overlay-v4-final-ownership-targeted11-output-v9"


def sha(path: Path) -> str:
    return hashlib.sha256(path.read_bytes()).hexdigest()


def add(condition: bool, error: str, errors: list[str]) -> None:
    if not condition:
        errors.append(error)


def validate(artifact_dir: Path, diagnostic_script: Path) -> list[str]:
    errors: list[str] = []
    values: dict[str, dict] = {}
    for name, expected in EXPECTED_ARTIFACT_SHA256.items():
        path = artifact_dir / name
        if not path.is_file():
            errors.append(f"artifact_missing:{name}")
            continue
        if sha(path) != expected:
            errors.append(f"artifact_hash_drift:{name}")
        try:
            values[name] = json.loads(path.read_text())
        except (OSError, ValueError, TypeError):
            errors.append(f"artifact_json_invalid:{name}")

    if not diagnostic_script.is_file() or \
            sha(diagnostic_script) != EXPECTED_SCRIPT_SHA256:
        errors.append("diagnostic_script_hash_drift")
    else:
        source = diagnostic_script.read_text()
        for forbidden in ("import subprocess", "subprocess.run", "os.system",
                          "NSOpenGLContext", "glDraw", "--run"):
            if forbidden in source:
                errors.append(
                    f"diagnostic_script_execution_boundary_invalid:{forbidden}")

    for relative, expected in EXPECTED_FROZEN.items():
        path = ROOT / relative
        if not path.is_file() or sha(path) != expected:
            errors.append(f"frozen_hash_drift:{relative}")
        elif stat.S_IMODE(path.stat().st_mode) != 0o444:
            errors.append(f"frozen_mode_not_0444:{relative}")
    add(OUTPUT.is_dir() and stat.S_IMODE(OUTPUT.stat().st_mode) == 0o555,
        "frozen_output_mode_not_0555", errors)
    for relative, expected in EXPECTED_BINDINGS.items():
        path = ROOT / relative
        if not path.is_file() or sha(path) != expected:
            errors.append(f"binding_hash_drift:{relative}")

    receipt = values.get("failure-evidence-receipt-v1.json", {})
    gate = values.get("minimal-compositing-style-correction-design-gate-v1.json", {})
    add(receipt.get("schema") ==
        "billiardgl.phase4.final_ownership_v9_ordinal1_offline_diagnostic.v1" and
        receipt.get("status") ==
        "COMPOSITING_STYLE_LAYER_ALIAS_AND_FIXED_RIM_CONTRAST_DEFECT_ISOLATED_ALGORITHM_CLEAN_NO_CAPTURE_AUTHORIZED",
        "receipt_status_invalid", errors)
    add(receipt.get("gpu_cocoa_opengl_invoked") is False and
        receipt.get("capture_retry_authorized") is False and
        receipt.get("new_readiness_or_authorization_created") is False,
        "receipt_execution_boundary_invalid", errors)
    frozen = receipt.get("frozen_evidence", {})
    add(len(frozen) == len(EXPECTED_FROZEN) and
        all(frozen.get(relative) == {"sha256": expected, "mode": "0444"}
            for relative, expected in EXPECTED_FROZEN.items()) and
        receipt.get("frozen_output_directory_mode") == "0555",
        "receipt_frozen_binding_invalid", errors)

    dispatch = receipt.get("dispatch_outcome", {})
    add(dispatch.get("status") == "CAPTURE_FAILED_TELEMETRY_PRESERVED" and
        [dispatch.get(key) for key in ("completed", "failed", "unrun")] ==
        [0, 1, 10] and
        dispatch.get("failed_capture_id") == "camera_player_low_1080" and
        dispatch.get("authorization_consumed_once") is True and
        dispatch.get("validation_errors") == [],
        "dispatch_outcome_invalid", errors)

    authority = receipt.get("authority_separation", {})
    corrected = authority.get("corrected_integrated", {})
    legacy = authority.get("legacy_ppm_diagnostic_only", {})
    add(authority.get("authoritative_gate") ==
        "corrected_integrated_final_semantic_ownership" and
        [corrected.get(key) for key in ("source", "visible", "hidden")] ==
        [95, 66, 29] and corrected.get("coverage") == 66 / 95 and
        corrected.get("machine_pass") is False,
        "corrected_authority_invalid", errors)
    add([legacy.get(key) for key in ("source", "visible", "hidden")] ==
        [95, 47, 48] and legacy.get("coverage") == 47 / 95 and
        legacy.get("authoritative") is False,
        "legacy_authority_invalid", errors)

    tracks = receipt.get("track_counts", {})
    predicted = tracks.get("predicted", {})
    actual = tracks.get("actual", {})
    add([predicted.get(key) for key in ("source", "visible", "hidden")] ==
        [76, 52, 24] and predicted.get("coverage") == 52 / 76 and
        [actual.get(key) for key in ("source", "visible", "hidden")] ==
        [19, 14, 5] and actual.get("coverage") == 14 / 19,
        "track_counts_invalid", errors)
    components = receipt.get("component_counts", {})
    add(components.get("source") == {
        "core": 43, "endpoint": 2, "outer_halo_edge": 41,
        "solid_in_predicted_dash_gap": 4, "tick": 5,
    } and components.get("visible") == {
        "core": 30, "endpoint": 2, "outer_halo_edge": 27,
        "solid_in_predicted_dash_gap": 4, "tick": 3,
    } and components.get("hidden_reason") == {
        "core|final_edge_contrast_below_4": 13,
        "outer_halo_edge|final_edge_contrast_below_4": 14,
        "tick|final_edge_contrast_below_4": 2,
    }, "component_counts_invalid", errors)

    ownership = receipt.get("ownership_and_association", {})
    add(ownership.get("candidate_count") == 95 and
        ownership.get("selected_boundary_count") == 95 and
        ownership.get("arbitration_record_count") == 6552 and
        ownership.get("contested_arbitration_record_count") == 4662 and
        ownership.get("winner_source_probe_count") == 95 and
        ownership.get("foreign_unowned_double_counts") == [0, 0, 0] and
        ownership.get("candidate_reason_counts") == {
            "eligible": 66, "final_edge_contrast_below_4": 29} and
        ownership.get("association_mode_counts") == {
            "exact_primitive_stem": 1, "exclusive_area_owner": 71,
            "owned_island_component": 23} and
        ownership.get("boundary_class_counts") == {
            "compound_outer_silhouette": 46,
            "same_track_compound_inner_edge": 49},
        "ownership_association_invalid", errors)

    partition = receipt.get("contrast_failure_partition", {})
    add(partition == {
        "total": 29,
        "same_track_inner_edge": 14,
        "compound_outer_silhouette": 15,
        "predicted_core_adjacent_white_outer_rim": 12,
        "actual_tick_adjacent_black_outer_rim": 2,
        "same_primitive_stem_inner_failures": 4,
        "cross_primitive_stem_inner_failures": 10,
        "predicted_white_rim_vs_pale_background": 12,
        "actual_black_rim_vs_dark_or_teal_background": 3,
        "area_or_association_failure_count": 0,
    }, "contrast_partition_invalid", errors)
    hidden = receipt.get("hidden_probe_details", [])
    add(isinstance(hidden, list) and len(hidden) == 29 and
        all(value.get("reason") == "final_edge_contrast_below_4"
            for value in hidden) and
        sum(value.get("same_primitive_stem") is True for value in hidden) >= 4,
        "hidden_probe_details_invalid", errors)

    comparison = receipt.get("v8_to_v9_comparison", {})
    area = comparison.get("candidate_area", {})
    add(comparison.get("source_probe_ids_and_ordered_digest_equal") is True and
        comparison.get("v8_authoritative_visible_hidden") == [92, 3] and
        comparison.get("v9_authoritative_visible_hidden") == [66, 29] and
        comparison.get("visible_delta") == -26 and
        area.get("v8_min_median_max") == [4, 56, 215] and
        area.get("v9_min_median_max") == [4, 66, 247] and
        area.get("delta_increased_equal_decreased") == [77, 8, 10] and
        area.get("v9_below_area_floor_count") == 0 and
        area.get("v9_hidden_min_area") == 12 and
        comparison.get("selected_association_mode_changed_count") == 9 and
        comparison.get("selected_boundary_class_changed_count") == 0 and
        comparison.get("selected_owner_track_or_component_changed_count") == 0,
        "v8_v9_comparison_invalid", errors)

    layering = receipt.get("static_layering_evidence", {})
    add(layering.get("renderer_sha256") == EXPECTED_BINDINGS[
        "src/Billiards/training/trajectory_overlay_renderer.cpp"] and
        layering.get("renderer_header_sha256") == EXPECTED_BINDINGS[
        "src/Billiards/training/trajectory_overlay_renderer.h"] and
        layering.get("outer_and_inner_share_one_semantic_halo_component") is True and
        layering.get("per_primitive_geometry_is_full_outer_fill_then_full_inner_fill") is True and
        layering.get("outer_fill_can_overwrite_prior_inner_fill_at_overlap") is True and
        layering.get("predicted_core_cap_extension_px_at_1080") == 1.5 and
        layering.get("predicted_inner_halo_extension_px_at_1080") == 2.0 and
        layering.get("same_primitive_remaining_cap_annulus_px") == 0.5,
        "static_layering_evidence_invalid", errors)
    root_cause = receipt.get("root_cause", {})
    add(root_cause.get("isolated") is True and
        root_cause.get("primary") ==
        "VISUAL_COMPOSITING_LAYER_ORDER_AND_BACKGROUND_ADAPTIVITY_DEFECT" and
        root_cause.get("not_ownership_or_association_algorithm") is True and
        root_cause.get("direct_gate_reason") == {
            "final_edge_contrast_below_4": 29} and
        root_cause.get("cap_expansion_area_result") ==
        "AREA_RECOVERED_BUT_FOUR_SAME_PRIMITIVE_CORE_INNER_EDGES_LOST_CONTRAST" and
        root_cause.get("three_layers_same_semantic_result") ==
        "SEMANTIC_IDENTITY_STABLE_BUT_COLOR_LAYERS_ALIAS_IN_BOUNDARY_TELEMETRY_AND_FULL_OUTER_FILLS_OVERWRITE_INNER_COLOR",
        "root_cause_invalid", errors)

    observation = receipt.get("root_original_resolution_observation_crosscheck", {})
    telemetry_check = observation.get("telemetry_crosscheck", {})
    add(observation.get("observation_is_not_machine_authority") is True and
        "white/black/yellow triple-line" in observation.get("observation", "") and
        telemetry_check.get("yellow_to_white_selected_inner_edge_count") == 12 and
        telemetry_check.get("yellow_to_white_contrast_ratio") ==
        1.0749340470546902 and
        telemetry_check.get("white_to_pale_outer_silhouette_failure_count") == 12 and
        telemetry_check.get("white_to_pale_contrast_range") ==
        [1.0834217233705468, 1.9309778443054044] and
        observation.get("product_clarity_conclusion") ==
        "Do not widen the three-band treatment; retain a dominant track stroke and use only a narrow, locally necessary contrast boundary.",
        "original_resolution_crosscheck_invalid", errors)

    evidence_path = artifact_dir / "failure-evidence-receipt-v1.json"
    evidence_sha = sha(evidence_path) if evidence_path.is_file() else ""
    add(gate.get("schema") ==
        "billiardgl.phase4.final_ownership_v9_compositing_style_correction_design_gate.v1" and
        gate.get("status") ==
        "DESIGN_ONLY_NO_IMPLEMENTATION_NO_CAPTURE_AUTHORIZATION" and
        gate.get("bound_failure_evidence_receipt_sha256") == evidence_sha and
        gate.get("bound_v9_telemetry_sha256") == EXPECTED_FROZEN[
            "docs/art/phase4-training-replay-overlay-v4-final-ownership-targeted11-output-v9/results.jsonl.final-ownership.jsonl"] and
        gate.get("bound_v9_dispatch_failure_receipt_sha256") == EXPECTED_FROZEN[
            "docs/art/phase4-training-replay-overlay-v4-final-ownership-targeted11-output-v9/phase4-final-ownership-dispatch-failure-receipt-v1.json"] and
        gate.get("bound_v9_authorization_sha256") == EXPECTED_FROZEN[
            "docs/art/phase4-training-replay-overlay-v4-final-ownership-targeted11-ready-v9/root-one-shot-gpu-authorization-v9-run1.json"] and
        gate.get("bound_v9_consumed_sha256") == EXPECTED_FROZEN[
            "docs/art/phase4-training-replay-overlay-v4-final-ownership-targeted11-ready-v9/root-one-shot-gpu-authorization-v9-run1.json.consumed"] and
        gate.get("bound_v8_visual_gate_sha256") == EXPECTED_BINDINGS[
            "docs/art/phase4-final-ownership-v8-ordinal2-offline-diagnostic-v1/minimal-visual-style-correction-design-gate-v1.json"],
        "gate_binding_invalid", errors)
    add(gate.get("capture_authorized") is False and
        gate.get("formal68_authorized") is False and
        gate.get("technical_art_review_authorized") is False,
        "gate_authorization_invalid", errors)
    invariants = gate.get("immutable_invariants", {})
    add(invariants.get("minimum_visible_coverage") == 0.9 and
        invariants.get("minimum_edge_contrast_ratio") == 4.0 and
        invariants.get("minimum_owned_area_pixel_equivalents") == 1.5 and
        invariants.get("ordinal1_source_probe_count") == 95 and
        invariants.get("ordinal1_ordered_source_probe_digest") ==
        "d533924980efc85aeb94e4f1d31831d93e2bfce744be3b66481d08c56d10289a" and
        invariants.get("probe_ids_denominator_route_offsets_z_order") ==
        "UNCHANGED" and
        invariants.get("ownership_arbitration_association_and_hidden_reason_algorithm") ==
        "UNCHANGED" and
        invariants.get("camera_layout_assets_scene_ui") == "UNCHANGED",
        "gate_invariants_invalid", errors)
    allowed = gate.get("allowed_next_implementation_scope", [])
    fixtures = gate.get("required_cpu_static_negative_fixtures", [])
    forbidden = gate.get("forbidden", [])
    add(len(allowed) == 4 and
        any("visual-sublayer provenance" in value for value in allowed) and
        any("non-overwriting narrow band" in value for value in allowed) and
        any("deterministic local pre-overlay backdrop" in value
            for value in allowed) and
        len(fixtures) == 7 and
        any("high-frequency" in value and "track color must remain dominant"
            in value for value in fixtures) and
        any("fixed predicted-white/actual-black" in value for value in fixtures) and
        any("Frozen v9 replay remains 66/29 FAILED" in value
            for value in fixtures) and
        any("No further outer-envelope widening" in value and
            "no fixed track-wide rim polarity" in value for value in forbidden),
        "gate_scope_or_fixtures_invalid", errors)
    return errors


def main() -> None:
    parser = argparse.ArgumentParser()
    parser.add_argument("artifact_dir", nargs="?", type=Path,
                        default=CANONICAL)
    parser.add_argument("--diagnostic-script", type=Path,
                        default=CANONICAL_SCRIPT)
    args = parser.parse_args()
    errors = validate(args.artifact_dir.resolve(),
                      args.diagnostic_script.resolve())
    print(json.dumps({
        "schema":
            "billiardgl.phase4.final_ownership_v9_offline_diagnostic_validation.v1",
        "status": "PASS" if not errors else "FAIL",
        "errors": errors,
        "gpu_cocoa_opengl_invoked": False,
        "capture_retry_authorized": False,
    }, indent=2, sort_keys=True))
    raise SystemExit(0 if not errors else 1)


if __name__ == "__main__":
    main()
