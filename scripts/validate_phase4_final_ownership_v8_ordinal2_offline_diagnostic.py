#!/usr/bin/env python3
"""Fail-closed validator for the Phase4 v8 ordinal-2 offline diagnosis."""

from __future__ import annotations

import argparse
import hashlib
import json
from pathlib import Path
import stat


ROOT = Path(__file__).resolve().parents[1]
CANONICAL = ROOT / "docs/art/phase4-final-ownership-v8-ordinal2-offline-diagnostic-v1"
CANONICAL_SCRIPT = ROOT / \
    "scripts/diagnose_phase4_final_ownership_v8_ordinal2_offline.py"

EXPECTED_ARTIFACT_SHA256 = {
    "failure-evidence-receipt-v1.json":
        "7994824c30ef85e41bb4a80d0372e8fee8b011b832507ac18c09f09e7adb07b7",
    "minimal-visual-style-correction-design-gate-v1.json":
        "0fe967c8ca0e855c6cb0cd29f745aefd7dfcf8a9ae387eb1522d254753196526",
}
EXPECTED_SCRIPT_SHA256 = \
    "5e22ea8ee3458f8abe78c2163a9668445e922c625e607a979cc3c413ea02de75"
EXPECTED_FROZEN = {
    "docs/art/phase4-training-replay-overlay-v4-final-ownership-targeted11-ready-v8/root-one-shot-gpu-authorization-v8-run1.json":
        "58e6e901dc1ef435cd3f0aa072267cece4aa278c6d9fa111bc38cd9bd623b1a4",
    "docs/art/phase4-training-replay-overlay-v4-final-ownership-targeted11-ready-v8/root-one-shot-gpu-authorization-v8-run1.json.consumed":
        "74b135e0f7054827cb7d81bbf117fe9fa096bdfe3fee9ea9d0e7ced73046a23e",
    "docs/art/phase4-training-replay-overlay-v4-final-ownership-targeted11-output-v8/phase4-final-ownership-targeted11-plan.tsv":
        "3b969b34e52fe2e01e31e34b148c4ce7c211682db65b3c609a43274366c8717d",
    "docs/art/phase4-training-replay-overlay-v4-final-ownership-targeted11-output-v8/results.jsonl":
        "bbe1082a330f111421e8767f88c20ac9b6dda3ba945a746b6723a199c4fd84e1",
    "docs/art/phase4-training-replay-overlay-v4-final-ownership-targeted11-output-v8/results.jsonl.final-ownership.jsonl":
        "48d34fb11a342ffb7ff00f0729396ad48544bcfec4db43470bedd3d92148bd99",
    "docs/art/phase4-training-replay-overlay-v4-final-ownership-targeted11-output-v8/camera_player_low_1080.ppm":
        "f3626f44ead6cfe4729e6ff3b76168747154b2452d0ca2afd70e1520b0e09744",
    "docs/art/phase4-training-replay-overlay-v4-final-ownership-targeted11-output-v8/camera_elevated_1080.ppm":
        "1761d04326782eebaef2f92043d422d32f1140297bc64515a8a114d637c51da7",
    "docs/art/phase4-training-replay-overlay-v4-final-ownership-targeted11-output-v8/phase4-final-ownership-dispatch-failure-receipt-v1.json":
        "1b4cac8f1df51e8b0d04a3e06770cddbf815b8a64377326f424c58e3901cfd89",
}
EXPECTED_BINDINGS = {
    "docs/art/phase4-training-replay-overlay-v4-final-ownership-targeted11-ready-v8/targeted11-readiness-v8.json":
        "9a9d44ff3cd388e85a1e4f603b74e89d3d934f4f132ec25191dbcbc0eb5e44af",
    "docs/art/phase4-training-replay-overlay-v4-final-ownership-targeted11-ready-v8/source-bindings-v1.json":
        "331ab39540ab49e8f44a5a8dc98a2d8e39dfdaa9af474a7446cdf3f5eeb5df3f",
    "docs/art/phase4-final-ownership-v7-ordinal1-offline-diagnostic-v1/minimal-owned-island-correction-design-gate-v1.json":
        "9f6c5febd88a06b22a8d05bf621f1310ddc8c854950dc02990cd857b21002cf2",
}


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
    for relative, expected in EXPECTED_BINDINGS.items():
        path = ROOT / relative
        if not path.is_file() or sha(path) != expected:
            errors.append(f"binding_hash_drift:{relative}")

    receipt = values.get("failure-evidence-receipt-v1.json", {})
    gate = values.get("minimal-visual-style-correction-design-gate-v1.json", {})
    add(receipt.get("schema") ==
        "billiardgl.phase4.final_ownership_v8_ordinal2_offline_diagnostic.v1" and
        receipt.get("status") ==
        "VISUAL_STYLE_CONTRAST_DEFECT_ISOLATED_ALGORITHM_CLEAN_NO_CAPTURE_AUTHORIZED",
        "receipt_status_invalid", errors)
    add(receipt.get("gpu_cocoa_opengl_invoked") is False and
        receipt.get("capture_retry_authorized") is False and
        receipt.get("new_readiness_or_authorization_created") is False,
        "receipt_execution_boundary_invalid", errors)
    dispatch = receipt.get("dispatch_outcome", {})
    add(dispatch.get("status") == "CAPTURE_FAILED_TELEMETRY_PRESERVED" and
        [dispatch.get(key) for key in ("completed", "failed", "unrun")] ==
        [1, 1, 9] and
        dispatch.get("failed_capture_id") == "camera_elevated_1080" and
        dispatch.get("validation_errors") == [] and
        dispatch.get("authorization_consumed_once") is True,
        "dispatch_outcome_invalid", errors)

    authority = receipt.get("ordinal2_authority_separation", {})
    corrected = authority.get("corrected_integrated", {})
    legacy = authority.get("legacy_ppm_diagnostic_only", {})
    add(authority.get("authoritative_gate") ==
        "corrected_integrated_final_semantic_ownership" and
        [corrected.get(key) for key in ("source", "visible", "hidden")] ==
        [83, 72, 11] and corrected.get("coverage") == 72 / 83 and
        corrected.get("machine_pass") is False,
        "corrected_authority_invalid", errors)
    add([legacy.get(key) for key in ("source", "visible", "hidden")] ==
        [83, 40, 43] and legacy.get("coverage") == 40 / 83 and
        legacy.get("authoritative") is False,
        "legacy_authority_invalid", errors)

    tracks = receipt.get("ordinal2_track_counts", {})
    predicted = tracks.get("predicted", {})
    actual = tracks.get("actual", {})
    add([predicted.get(key) for key in ("source", "visible", "hidden")] ==
        [71, 61, 10] and predicted.get("coverage") == 61 / 71 and
        [actual.get(key) for key in ("source", "visible", "hidden")] ==
        [12, 11, 1] and actual.get("coverage") == 11 / 12,
        "track_counts_invalid", errors)
    components = receipt.get("ordinal2_component_counts", {})
    add(components.get("source") == {
        "core": 38, "endpoint": 2, "outer_halo_edge": 37,
        "solid_in_predicted_dash_gap": 3, "tick": 3,
    } and components.get("visible") == {
        "core": 37, "endpoint": 2, "outer_halo_edge": 28,
        "solid_in_predicted_dash_gap": 2, "tick": 3,
    } and components.get("hidden_reason") == {
        "core|insufficient_owned_pixel_area": 1,
        "outer_halo_edge|final_edge_contrast_below_4": 8,
        "outer_halo_edge|insufficient_owned_pixel_area": 1,
        "solid_in_predicted_dash_gap|final_edge_contrast_below_4": 1,
    }, "component_counts_invalid", errors)

    ownership = receipt.get("ownership_and_arbitration", {})
    add(ownership.get("candidate_count") == 83 and
        ownership.get("arbitration_record_count") == 3586 and
        ownership.get("contested_arbitration_record_count") == 2750 and
        ownership.get("winner_source_probe_count") == 83 and
        [ownership.get(key) for key in (
            "foreign_track_evidence_count", "unowned_evidence_count",
            "double_assignment_count", "association_or_owner_failure_count")]
        == [0, 0, 0, 0], "ownership_arbitration_invalid", errors)
    association = receipt.get("selected_boundary_association", {})
    add(association.get("selected_count") == 83 and
        association.get("selected_unique_source_probe_count") == 83 and
        association.get("selected_all_role_eligible") is True and
        association.get("association_mode_counts") == {
            "exact_primitive_stem": 2,
            "exclusive_area_owner": 51,
            "owned_island_component": 30,
        } and association.get("boundary_class_counts") == {
            "compound_outer_silhouette": 39,
            "same_track_compound_inner_edge": 44,
        }, "selected_association_invalid", errors)

    contrast = receipt.get("selected_boundary_final_ldr_contrast", {})
    add(contrast.get("record_count") == 83 and
        contrast.get("unique_source_probe_count") == 83 and
        contrast.get("selected_below_4_count") == 10 and
        contrast.get("serialized_contrast_failure_count") == 9,
        "contrast_inventory_invalid", errors)
    groups = contrast.get("by_track_component_boundary_class", {})
    predicted_outer = groups.get(
        "predicted|outer_halo_edge|compound_outer_silhouette", {})
    actual_outer = groups.get(
        "actual|outer_halo_edge|compound_outer_silhouette", {})
    add([predicted_outer.get("count"), predicted_outer.get("below_4_count")] ==
        [35, 9] and
        abs(predicted_outer.get("minimum", 0) - 1.0973756636727583) < 1e-12 and
        [actual_outer.get("count"), actual_outer.get("below_4_count")] ==
        [4, 1] and
        abs(actual_outer.get("minimum", 0) - 1.2513367545787983) < 1e-12,
        "outer_silhouette_contrast_invalid", errors)
    low = contrast.get("below_4_records", [])
    add(isinstance(low, list) and len(low) == 10 and
        all(item.get("boundary_class") == "compound_outer_silhouette"
            for item in low) and
        sum(item.get("serialized_hidden_reason") ==
            "final_edge_contrast_below_4" for item in low) == 9 and
        sum(item.get("serialized_hidden_reason") ==
            "insufficient_owned_pixel_area" for item in low) == 1,
        "low_contrast_records_invalid", errors)
    hidden = receipt.get("hidden_probe_details", [])
    add(isinstance(hidden, list) and len(hidden) == 11 and
        all(item.get("selected_boundary_class") in {
            "compound_outer_silhouette", "same_track_compound_inner_edge"}
            for item in hidden) and
        all(item.get("selected_association_mode") in {
            "exclusive_area_owner", "exact_primitive_stem",
            "owned_island_component"} for item in hidden),
        "hidden_probe_provenance_invalid", errors)

    stress = receipt.get("screen_space_coaxial_stress", {})
    p_to_a = stress.get("predicted_to_actual_nearest", {})
    a_to_p = stress.get("actual_to_predicted_nearest", {})
    add(stress.get("evidence_kind") ==
        "TELEMETRY_PHYSICAL_PIXEL_PROXIMITY_NOT_SCREENSHOT_INFERENCE" and
        stress.get("predicted_candidate_bounds") == {
            "x_min": 956, "x_max": 973, "y_min": 184, "y_max": 521} and
        stress.get("actual_candidate_bounds") == {
            "x_min": 955, "x_max": 965, "y_min": 264, "y_max": 525} and
        [a_to_p.get(key) for key in (
            "source_count", "within_4px_count", "within_8px_count",
            "within_12px_count")] == [12, 5, 11, 12] and
        [p_to_a.get(key) for key in (
            "source_count", "within_4px_count", "within_8px_count",
            "within_12px_count")] == [71, 6, 19, 35],
        "coaxial_stress_evidence_invalid", errors)

    root_cause = receipt.get("root_cause", {})
    add(root_cause.get("isolated") is True and
        root_cause.get("primary_classification") ==
        "PRODUCT_VISUAL_STYLE_FINAL_LDR_SILHOUETTE_CONTRAST_FAILURE" and
        root_cause.get("algorithm_classification") ==
        "NO_ASSOCIATION_OR_OWNERSHIP_DEFECT_DETECTED_IN_FROZEN_PAYLOAD" and
        root_cause.get("direct_gate_reasons") == {
            "final_edge_contrast_below_4": 9,
            "insufficient_owned_pixel_area": 2,
        }, "root_cause_invalid", errors)
    analytic = receipt.get("analytic_visual_only_counterfactual_non_final", {})
    add(analytic.get("label") ==
        "ANALYTIC_ONLY_NOT_CAPTURE_EVIDENCE_NOT_A_MACHINE_PASS" and
        [analytic.get(key) for key in ("source", "visible", "hidden")] ==
        [83, 81, 2] and analytic.get("coverage") == 81 / 83 and
        analytic.get("authoritative") is False,
        "analytic_counterfactual_boundary_invalid", errors)

    evidence_path = artifact_dir / "failure-evidence-receipt-v1.json"
    evidence_sha = sha(evidence_path) if evidence_path.is_file() else ""
    add(gate.get("schema") ==
        "billiardgl.phase4.final_ownership_v8_visual_style_correction_design_gate.v1" and
        gate.get("status") ==
        "DESIGN_ONLY_NO_IMPLEMENTATION_NO_CAPTURE_AUTHORIZATION" and
        gate.get("bound_failure_evidence_receipt_sha256") == evidence_sha and
        gate.get("bound_v8_telemetry_sha256") ==
        EXPECTED_FROZEN[
            "docs/art/phase4-training-replay-overlay-v4-final-ownership-targeted11-output-v8/results.jsonl.final-ownership.jsonl"] and
        gate.get("bound_prior_algorithm_gate_sha256") ==
        EXPECTED_BINDINGS[
            "docs/art/phase4-final-ownership-v7-ordinal1-offline-diagnostic-v1/minimal-owned-island-correction-design-gate-v1.json"],
        "gate_binding_invalid", errors)
    add(gate.get("implementation_authorized") is False and
        gate.get("capture_authorized") is False and
        gate.get("technical_art_review_authorized") is False and
        gate.get("formal68_authorized") is False,
        "gate_authorization_invalid", errors)
    invariants = gate.get("immutable_invariants", {})
    add(invariants.get("ordinal1_source_probe_count") == 95 and
        invariants.get("ordinal2_source_probe_count") == 83 and
        len(invariants.get("ordinal2_source_probe_ids", [])) == 83 and
        invariants.get("minimum_visible_coverage") == 0.9 and
        invariants.get("minimum_edge_contrast_ratio") == 4.0 and
        invariants.get("canonical_plan_sha256") ==
        "3b969b34e52fe2e01e31e34b148c4ce7c211682db65b3c609a43274366c8717d" and
        invariants.get("probe_ids_denominator_route_offsets_z_order") ==
        "UNCHANGED" and
        invariants.get("layout_assets_scene_geometry_and_ui") == "UNCHANGED",
        "gate_invariants_invalid", errors)
    fixtures = gate.get("required_cpu_static_fixtures_before_any_new_readiness", [])
    forbidden = gate.get("forbidden", [])
    allowed = gate.get("allowed_next_implementation_scope", [])
    add(len(fixtures) == 7 and
        any("counterfactual" in item for item in fixtures) and
        any("No boundary-association" in item for item in forbidden) and
        any("visual styling only" in item for item in allowed),
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
            "billiardgl.phase4.final_ownership_v8_offline_diagnostic_validation.v1",
        "status": "PASS" if not errors else "FAIL",
        "errors": errors,
        "gpu_cocoa_opengl_invoked": False,
        "capture_retry_authorized": False,
    }, indent=2, sort_keys=True))
    raise SystemExit(0 if not errors else 1)


if __name__ == "__main__":
    main()
