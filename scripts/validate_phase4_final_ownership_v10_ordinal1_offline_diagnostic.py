#!/usr/bin/env python3
"""Fail-closed validator for the Phase4 v10 ordinal-1 offline diagnosis."""

from __future__ import annotations

import argparse
import hashlib
import json
from pathlib import Path
import stat


ROOT = Path(__file__).resolve().parents[1]
CANONICAL = ROOT / "docs/art/phase4-final-ownership-v10-ordinal1-offline-diagnostic-v1"
DIAGNOSTIC = ROOT / "scripts/diagnose_phase4_final_ownership_v10_ordinal1_offline.py"
OUTPUT = ROOT / ("docs/art/phase4-training-replay-overlay-v4-final-ownership-"
                 "targeted11-output-v10")
EXPECTED_ARTIFACTS = {
    "failure-evidence-receipt-v1.json":
        "c78fa8df1e74dbe377b97bdf3f28949bb8a778abc743952abcdb308f3693ffdc",
    "minimal-machine-contract-correction-design-gate-v1.json":
        "98575c3e3c48e99e9a056c047ba59e268a23c038d2685ed7252edd5d0cbe525a",
}
EXPECTED_DIAGNOSTIC_SHA256 = \
    "71536534e1dd0875bef53d5cd2037b351a03a05b78086e0c4442e95d4fbdf00d"
EXPECTED_FROZEN = {
    "docs/art/phase4-training-replay-overlay-v4-final-ownership-targeted11-ready-v10/root-one-shot-gpu-authorization-v10-run1.json":
        "dc0c60ddba2bef6a86ee4c1e328727543e24de5153260af905afd5f8d383fa8f",
    "docs/art/phase4-training-replay-overlay-v4-final-ownership-targeted11-ready-v10/root-one-shot-gpu-authorization-v10-run1.json.consumed":
        "6cd0c9ec5d07b4ecd637aca6bff25048897e51541bb13e0d29571c3635c0a33c",
    "docs/art/phase4-training-replay-overlay-v4-final-ownership-targeted11-output-v10/phase4-final-ownership-targeted11-plan.tsv":
        "3b969b34e52fe2e01e31e34b148c4ce7c211682db65b3c609a43274366c8717d",
    "docs/art/phase4-training-replay-overlay-v4-final-ownership-targeted11-output-v10/results.jsonl":
        "e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855",
    "docs/art/phase4-training-replay-overlay-v4-final-ownership-targeted11-output-v10/results.jsonl.final-ownership.jsonl":
        "d8e06f0aa0e77d0e677af72ce2250a563d2595b70b790b05ad5728c9cc59c94d",
    "docs/art/phase4-training-replay-overlay-v4-final-ownership-targeted11-output-v10/camera_player_low_1080.ppm":
        "82d6f9e5aba1821b3b27c00f2b76977fff035cf8541c768bd5361c1ee4ee744a",
    "docs/art/phase4-training-replay-overlay-v4-final-ownership-targeted11-output-v10/phase4-final-ownership-dispatch-failure-receipt-v1.json":
        "5b5923a8efbce6f293af4d2fd71ce2d4c016d2faaabc5dbd4d19600ea10772ae",
}
EXPECTED_BINDINGS = {
    "docs/art/phase4-training-replay-overlay-v4-final-ownership-targeted11-ready-v10/targeted11-readiness-v10.json":
        "878d69d0833b1a8becd6322fbb29e3cdb20f57723a8d1d3ff43c985c10750f37",
    "docs/art/phase4-training-replay-overlay-v4-final-ownership-targeted11-ready-v10/source-bindings-v1.json":
        "66a34080e3934e60c5c7a021d14846dd6d54b3f1a1d19e2c579b63174a5e2b6a",
    "docs/art/phase4-training-replay-overlay-v4-final-ownership-targeted11-output-v9/results.jsonl.final-ownership.jsonl":
        "3f40581fc4f66861a3a5f0953d6350619ca3195ad0c560f1202c5458dd66f62a",
    "src/Billiards/training/trajectory_overlay_renderer.cpp":
        "09852cd4ad0a9c8aebeeac5b026d84f2d3c24624deddf7aab46330cfb1afd09e",
    "src/Billiards/render/modern/modern_renderer.cpp":
        "634d0b5605134c578f93c0a13efcc3fbba26fe6e5ce567dd11d21925992a8390",
    "build-integrated-final-shared-phase4-v10-phase5-v5-phase2-v9/generated/phase4_overlay_v4_gpu_capture_runner.mm":
        "9528af34f96dbf90916115770ccfad6009010d2c7e006dc55eb1997e65bfa286",
}


def sha(path: Path) -> str:
    return hashlib.sha256(path.read_bytes()).hexdigest()


def add(condition: bool, error: str, errors: list[str]) -> None:
    if not condition:
        errors.append(error)


def validate(artifact_dir: Path, diagnostic: Path) -> list[str]:
    errors: list[str] = []
    values: dict[str, dict] = {}
    for name, expected in EXPECTED_ARTIFACTS.items():
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

    if not diagnostic.is_file() or sha(diagnostic) != EXPECTED_DIAGNOSTIC_SHA256:
        errors.append("diagnostic_script_hash_drift")
    else:
        source = diagnostic.read_text()
        for forbidden in ("import subprocess", "subprocess.run", "os.system",
                          "NSOpenGLContext", "glDrawArrays(", "--run"):
            if forbidden in source:
                errors.append(f"diagnostic_execution_boundary_invalid:{forbidden}")

    for relative, expected in EXPECTED_FROZEN.items():
        path = ROOT / relative
        add(path.is_file() and sha(path) == expected,
            f"frozen_hash_drift:{relative}", errors)
        if path.is_file():
            add(stat.S_IMODE(path.stat().st_mode) == 0o444,
                f"frozen_mode_not_0444:{relative}", errors)
    add(OUTPUT.is_dir() and stat.S_IMODE(OUTPUT.stat().st_mode) == 0o555,
        "frozen_output_mode_not_0555", errors)
    for relative, expected in EXPECTED_BINDINGS.items():
        path = ROOT / relative
        add(path.is_file() and sha(path) == expected,
            f"binding_hash_drift:{relative}", errors)

    receipt = values.get("failure-evidence-receipt-v1.json", {})
    gate = values.get("minimal-machine-contract-correction-design-gate-v1.json", {})
    add(receipt.get("schema") ==
        "billiardgl.phase4.final_ownership_v10_ordinal1_offline_diagnostic.v1" and
        receipt.get("status") ==
        "STALE_EXACT_ONE_DRAW_MACHINE_CONTRACT_SUFFICIENT_FALSE_CONJUNCT_ISOLATED_NO_RETRY_AUTHORIZED",
        "receipt_status_invalid", errors)
    add(receipt.get("gpu_cocoa_opengl_invoked") is False and
        receipt.get("capture_retry_authorized") is False and
        receipt.get("new_readiness_or_authorization_created") is False,
        "receipt_execution_boundary_invalid", errors)
    add(receipt.get("frozen_output_directory_mode") == "0555" and
        receipt.get("frozen_evidence") == {
            relative: {"sha256": expected, "mode": "0444"}
            for relative, expected in EXPECTED_FROZEN.items()},
        "receipt_frozen_binding_invalid", errors)

    dispatch = receipt.get("dispatch_outcome", {})
    add([dispatch.get(k) for k in ("completed", "failed", "unrun")] ==
        [0, 1, 10] and dispatch.get("failed_capture_id") ==
        "camera_player_low_1080" and
        dispatch.get("authorization_consumed_once") is True and
        dispatch.get("validation_errors") == [], "dispatch_outcome_invalid", errors)
    authority = receipt.get("authority_separation", {})
    corrected = authority.get("corrected_integrated", {})
    legacy = authority.get("legacy_ppm_diagnostic_only", {})
    add(authority.get("authoritative_gate") ==
        "corrected_integrated_final_semantic_ownership" and
        [corrected.get(k) for k in ("source", "visible", "hidden")] ==
        [95, 89, 6] and corrected.get("coverage") == 89 / 95 and
        all(corrected.get(k) is True for k in
            ("aggregate_threshold_pass", "predicted_threshold_pass",
             "actual_threshold_pass")), "corrected_authority_invalid", errors)
    add([legacy.get(k) for k in ("source", "visible", "hidden")] ==
        [95, 50, 45] and legacy.get("coverage") == 50 / 95 and
        legacy.get("authoritative") is False, "legacy_authority_invalid", errors)
    tracks = receipt.get("track_counts", {})
    add(tracks.get("predicted") == {"source": 76, "visible": 70,
        "hidden": 6, "coverage": 70 / 76} and
        tracks.get("actual") == {"source": 19, "visible": 19,
        "hidden": 0, "coverage": 1}, "track_counts_invalid", errors)

    components = receipt.get("component_counts", {})
    add(components.get("source") == {"core": 43, "endpoint": 2,
        "outer_halo_edge": 41, "solid_in_predicted_dash_gap": 4, "tick": 5} and
        components.get("visible") == {"core": 43, "endpoint": 2,
        "outer_halo_edge": 35, "solid_in_predicted_dash_gap": 4, "tick": 5} and
        components.get("hidden_reason") == {
            "outer_halo_edge|final_edge_contrast_below_4": 6},
        "component_counts_invalid", errors)
    ownership = receipt.get("ownership_and_association", {})
    add(ownership == {
        "candidate_count": 95, "selected_boundary_count": 95,
        "arbitration_record_count": 5808,
        "contested_arbitration_record_count": 4215,
        "foreign_unowned_double_counts": [0, 0, 0],
        "candidate_reason_counts": {"eligible": 89,
            "final_edge_contrast_below_4": 6},
        "selected_association_mode_counts": {"exact_primitive_stem": 1,
            "exclusive_area_owner": 72, "owned_island_component": 22},
        "selected_boundary_class_counts": {"compound_outer_silhouette": 46,
            "same_track_compound_inner_edge": 49},
        "selected_sublayer_pair_counts": {"core|inner_halo": 49,
            "inner_halo|scene_or_background": 40,
            "outer_rim|scene_or_background": 6},
        "area_failure_count": 0}, "ownership_association_invalid", errors)
    hidden = receipt.get("hidden_probe_details", [])
    add(len(hidden) == 6 and
        all(value.get("track") == "predicted" and
            value.get("component_role") == "outer_halo_edge" and
            value.get("reason") == "final_edge_contrast_below_4" and
            value.get("selected_boundary_class") == "compound_outer_silhouette" and
            value.get("selected_association_mode") == "exclusive_area_owner"
            for value in hidden) and
        sorted(value.get("selected_owner_visual_sublayer") for value in hidden) ==
        ["inner_halo"] * 3 + ["outer_rim"] * 3 and
        min(value.get("resolved_pixel_equivalent_area") for value in hidden) == 49,
        "hidden_details_invalid", errors)
    comparison = receipt.get("v9_to_v10_comparison", {})
    add(comparison == {"source_probe_ids_and_digest_equal": True,
        "v9_visible_hidden": [66, 29], "v10_visible_hidden": [89, 6],
        "visible_delta": 23, "common_hidden": 4, "resolved_hidden": 25,
        "new_hidden": 2}, "v9_v10_comparison_invalid", errors)

    predicate = receipt.get("static_machine_predicate_evidence", {})
    add(predicate.get("package_reported_draw_submissions_by_definition") == 2 and
        predicate.get("frozen_runner_required_exact_draw_submissions") == 1 and
        predicate.get("actual_track_nonempty") is True and
        predicate.get("predicted_track_nonempty") is True and
        predicate.get("frame_metric_copies_package_additional_draw_calls") is True and
        predicate.get("draw_predicate_evaluates") is False and
        predicate.get("draw_predicate_is_a_sufficient_false_conjunct") is True,
        "draw_predicate_evidence_invalid", errors)
    root_cause = receipt.get("root_cause", {})
    add(root_cause.get("sufficient_false_conjunct_isolated") is True and
        root_cause.get("primary") ==
        "STALE_EXACT_ONE_DRAW_MACHINE_ACCEPTANCE_CONTRACT" and
        root_cause.get("authoritative_ownership_gate_passed") is True and
        root_cause.get("not_visibility_threshold_or_ownership_algorithm") is True and
        root_cause.get("not_legacy_ppm_gate") is True and
        root_cause.get("other_runtime_predicates_individually_serialized") is False and
        root_cause.get("exclusivity_of_this_single_failure_proven") is False,
        "root_cause_invalid", errors)

    add(gate.get("schema") ==
        "billiardgl.phase4.final_ownership_v10_machine_contract_correction_design_gate.v1" and
        gate.get("status") == "DESIGN_ONLY_NO_IMPLEMENTATION_NO_CAPTURE_AUTHORIZATION" and
        gate.get("bound_failure_evidence_receipt_sha256") ==
        EXPECTED_ARTIFACTS["failure-evidence-receipt-v1.json"],
        "gate_status_binding_invalid", errors)
    add(gate.get("capture_authorized") is False and
        gate.get("formal68_authorized") is False and
        gate.get("technical_art_review_authorized") is False,
        "gate_authorization_invalid", errors)
    allowed = gate.get("allowed_next_implementation_scope", [])
    fixtures = gate.get("required_cpu_static_negative_fixtures", [])
    forbidden = gate.get("forbidden", [])
    add(len(allowed) == 3 and any("exact-two" in item for item in allowed) and
        any("complete named machine-predicate ledger" in item for item in allowed) and
        len(fixtures) == 4 and any("rejects 0, 1, 3" in item for item in fixtures) and
        len(forbidden) == 4 and any("exclusive runtime failure" in item
                                   for item in forbidden),
        "gate_scope_invalid", errors)
    invariants = gate.get("immutable_invariants", {})
    add(invariants.get("minimum_visible_coverage") == .90 and
        invariants.get("minimum_local_contrast_ratio") == 4.0 and
        invariants.get("minimum_owned_area_pixel_equivalents") == 1.5 and
        invariants.get("ordinal1_source_probe_count") == 95 and
        invariants.get("probe_ids_denominator_route_offsets_z_order") ==
        "UNCHANGED" and
        invariants.get("ownership_arbitration_association_hidden_reason_algorithm") ==
        "UNCHANGED" and
        invariants.get("visual_colors_geometry_sublayers_backdrops_assets_scene_ui") ==
        "UNCHANGED", "gate_invariants_invalid", errors)
    return errors


def main() -> None:
    parser = argparse.ArgumentParser()
    parser.add_argument("artifact_dir", type=Path, nargs="?", default=CANONICAL)
    parser.add_argument("--diagnostic-script", type=Path, default=DIAGNOSTIC)
    args = parser.parse_args()
    errors = validate(args.artifact_dir.resolve(), args.diagnostic_script.resolve())
    print(json.dumps({"status": "PASS" if not errors else "FAIL",
                      "gpu_cocoa_opengl_invoked": False,
                      "capture_retry_authorized": False,
                      "errors": errors}, indent=2, sort_keys=True))
    raise SystemExit(0 if not errors else 1)


if __name__ == "__main__":
    main()
