#!/usr/bin/env python3
"""Fail-closed validator for the Phase4 v7 ordinal-1 offline diagnosis."""

from __future__ import annotations

import argparse
import hashlib
import json
from pathlib import Path
import stat


ROOT = Path(__file__).resolve().parents[1]
CANONICAL = ROOT / "docs/art/phase4-final-ownership-v7-ordinal1-offline-diagnostic-v1"
CANONICAL_SCRIPT = ROOT / "scripts/diagnose_phase4_final_ownership_v7_ordinal1_offline.py"

EXPECTED_ARTIFACT_SHA256 = {
    "failure-evidence-receipt-v1.json":
        "3db8ccde3034d6dc6f59d546b75bbc5986d6692c13e6647c9fb5d8f9ed1d31ec",
    "root-cause-diagnostic-report-v1.json":
        "2b530368f0a88a398803eacfd4749cfbf60c6e5f4987926e9e0b15275d00788a",
    "minimal-owned-island-correction-design-gate-v1.json":
        "9f6c5febd88a06b22a8d05bf621f1310ddc8c854950dc02990cd857b21002cf2",
}
EXPECTED_SCRIPT_SHA256 = \
    "f2dfff72873bae796084e9352ab3253bfb4f71af03d8bc15f9cf8703d99a2ebf"
EXPECTED_FROZEN = {
    "docs/evidence/phase4-final-ownership-targeted11-v7-authorization-root-v1.json":
        "1cd2a1483929b863e5ac9914eec94dfb28643578a23d79c08a155ac4c94ebe05",
    "docs/evidence/phase4-final-ownership-targeted11-v7-authorization-root-v1.json.consumed":
        "9bd6ef6688af125fbc73ada51cb0da13d824f423c52d5ae3a62000283128c592",
    "docs/art/phase4-training-replay-overlay-v4-final-ownership-targeted11-output-v7/phase4-final-ownership-dispatch-failure-receipt-v1.json":
        "e14979e634c03d676d3ddd99e1dc714352a9c2c6e9ecd1c01b9af8fdeadba66e",
    "docs/art/phase4-training-replay-overlay-v4-final-ownership-targeted11-output-v7/results.jsonl.final-ownership.jsonl":
        "ecfecdcc506adbebf21fa327e4ad50c08ec8a947b43359557c1318971bf6570a",
    "docs/art/phase4-training-replay-overlay-v4-final-ownership-targeted11-output-v7/camera_player_low_1080.ppm":
        "f3626f44ead6cfe4729e6ff3b76168747154b2452d0ca2afd70e1520b0e09744",
    "docs/art/phase4-training-replay-overlay-v4-final-ownership-targeted11-output-v7/results.jsonl":
        "e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855",
    "docs/art/phase4-training-replay-overlay-v4-final-ownership-targeted11-output-v7/phase4-final-ownership-targeted11-plan.tsv":
        "3b969b34e52fe2e01e31e34b148c4ce7c211682db65b3c609a43274366c8717d",
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
        for forbidden in ("import subprocess",
                          "BilliardsPhase4OverlayV4GpuCaptureRunner",
                          "NSOpenGLContext", "#include <Cocoa"):
            if forbidden in source:
                errors.append(f"diagnostic_script_execution_boundary_invalid:{forbidden}")

    for relative, expected in EXPECTED_FROZEN.items():
        path = ROOT / relative
        if not path.is_file() or sha(path) != expected:
            errors.append(f"frozen_hash_drift:{relative}")
        elif stat.S_IMODE(path.stat().st_mode) != 0o444:
            errors.append(f"frozen_mode_not_0444:{relative}")

    receipt = values.get("failure-evidence-receipt-v1.json", {})
    report = values.get("root-cause-diagnostic-report-v1.json", {})
    gate = values.get("minimal-owned-island-correction-design-gate-v1.json", {})
    add(receipt.get("schema") ==
        "billiardgl.phase4.final_ownership_v7_ordinal1_offline_diagnostic.v1" and
        receipt.get("status") ==
        "ALGORITHM_DEFECT_ISOLATED_NO_VISUAL_REPAIR_AUTHORIZED",
        "receipt_status_invalid", errors)
    add(receipt.get("gpu_cocoa_opengl_invoked") is False and
        receipt.get("capture_retry_authorized") is False and
        receipt.get("new_readiness_or_authorization_created") is False,
        "receipt_execution_boundary_invalid", errors)
    corrected = receipt.get("authoritative_corrected_capture", {})
    add([corrected.get(key) for key in ("source", "visible", "hidden")] ==
        [95, 86, 9] and corrected.get("coverage") == 86 / 95,
        "corrected_aggregate_invalid", errors)
    predicted = corrected.get("predicted", {})
    actual = corrected.get("actual", {})
    add([predicted.get(key) for key in ("source", "visible", "hidden")] ==
        [76, 67, 9] and predicted.get("coverage") == 67 / 76 and
        [actual.get(key) for key in ("source", "visible", "hidden")] ==
        [19, 19, 0] and actual.get("coverage") == 1,
        "track_counts_invalid", errors)
    add(corrected.get("component_hidden_reason_counts") == {
        "core|no_same_track_final_owner": 5,
        "outer_halo_edge|final_edge_contrast_below_4": 3,
        "outer_halo_edge|no_same_track_final_owner": 1,
    }, "component_failure_counts_invalid", errors)
    legacy = receipt.get("legacy_ppm_diagnostic_only", {})
    add([legacy.get("visible"), legacy.get("hidden")] == [46, 49] and
        legacy.get("authoritative") is False,
        "legacy_authority_invalid", errors)

    inventory = receipt.get("boundary_inventory", {})
    add(inventory.get("record_count") == 5268 and
        inventory.get("eligible_count") == 1596 and
        inventory.get("selected_count") == 89 and
        inventory.get("selected_below_4_count") == 3 and
        inventory.get("selected_boundary_class_counts") == {
            "compound_outer_silhouette": 45,
            "same_track_compound_inner_edge": 44,
        }, "boundary_inventory_invalid", errors)
    lost = receipt.get("lost_boundary_cross_partition_proof", [])
    add(isinstance(lost, list) and len(lost) == 6,
        "lost_boundary_proof_count_invalid", errors)
    if isinstance(lost, list):
        for item in lost:
            other = item.get("nearest_qualifying_boundary_owned_by_other_probe", {})
            add(item.get("resolved_sample_count", 0) > 0 and
                item.get("local_eligible_boundary_count") == 0 and
                other.get("other_source_probe_id") != item.get("source_probe_id") and
                0 < other.get("distance_from_source_probe_px", 999) <= 2 and
                other.get("edge_contrast_ratio", 0) >= 4 and
                other.get("boundary_class") in {
                    "same_track_compound_inner_edge",
                    "compound_outer_silhouette"},
                f"lost_boundary_proof_invalid:{item.get('source_probe_id')}",
                errors)
    low = receipt.get("genuine_low_contrast_selected_boundaries", [])
    add(isinstance(low, list) and len(low) == 3 and
        sorted(round(item.get("edge_contrast_ratio", 0), 6) for item in low) ==
        [1.287806, 3.892312, 3.902846] and
        all(item.get("boundary_class") == "compound_outer_silhouette"
            for item in low), "low_contrast_proof_invalid", errors)
    root_cause = receipt.get("root_cause", {})
    add(root_cause.get("isolated") is True and
        root_cause.get("classification") ==
        "ALGORITHM_DEFECT_PER_PROBE_ARBITRATION_PARTITIONS_BOUNDARY_SET",
        "root_cause_classification_invalid", errors)
    analytic = receipt.get("analytic_non_final_reassociation", {})
    add(analytic.get("label") == "COUNTERFACTUAL_ONLY_NOT_CORRECTED_EVIDENCE" and
        [analytic.get("visible"), analytic.get("hidden")] == [92, 3] and
        [analytic.get("predicted_visible"), analytic.get("predicted_hidden")] ==
        [73, 3], "analytic_non_final_boundary_invalid", errors)

    add(report.get("status") == receipt.get("status") and
        report.get("legacy_ppm_is_diagnostic_only") is True and
        report.get("gpu_cocoa_opengl_invoked") is False,
        "report_semantics_invalid", errors)
    evidence_path = artifact_dir / "failure-evidence-receipt-v1.json"
    evidence_sha = sha(evidence_path) if evidence_path.is_file() else ""
    add(gate.get("schema") ==
        "billiardgl.phase4.final_ownership_v7_owned_island_boundary_association_design_gate.v1" and
        gate.get("status") == "DESIGN_ONLY_NO_IMPLEMENTATION_NO_CAPTURE_AUTHORIZATION" and
        gate.get("bound_failure_evidence_receipt_sha256") == evidence_sha and
        gate.get("capture_authorized") is False and
        gate.get("technical_art_review_authorized") is False and
        gate.get("formal68_authorized") is False,
        "design_gate_boundary_invalid", errors)
    forbidden = gate.get("forbidden", [])
    add(any("product overlay palette/color/halo/stroke" in item
            for item in forbidden) and
        gate.get("immutable_invariants", {}).get("source_probe_count") == 95 and
        gate.get("immutable_invariants", {}).get(
            "minimum_visible_coverage") == 0.9 and
        gate.get("immutable_invariants", {}).get(
            "minimum_edge_contrast_ratio") == 4.0,
        "design_gate_invariants_invalid", errors)
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
            "billiardgl.phase4.final_ownership_v7_offline_diagnostic_validation.v1",
        "status": "PASS" if not errors else "FAIL",
        "errors": errors,
        "gpu_cocoa_opengl_invoked": False,
    }, indent=2, sort_keys=True))
    raise SystemExit(0 if not errors else 1)


if __name__ == "__main__":
    main()
