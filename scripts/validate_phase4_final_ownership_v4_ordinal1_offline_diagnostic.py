#!/usr/bin/env python3
"""Fail-closed CPU-only validator for the consumed v4 ordinal-1 diagnosis."""

from __future__ import annotations

import argparse
import hashlib
import json
from pathlib import Path
import stat

ROOT = Path(__file__).resolve().parents[1]
DEFAULT_ROOT = ROOT / "docs/art/phase4-final-ownership-v4-ordinal1-offline-diagnostic-v1"
EVIDENCE_SHA256 = "b25865928621e56a667302ebf9175d94ad512265d9e4cbf3174eebe259227f35"
GATE_SHA256 = "103c6b20ba5647fbb6cc82a7748cd362d378de34bffa2cd01f7f610a8469e269"
FROZEN = {
    "authorization": "629f8309a9393de01bd69a1b252f3b6023398ba8eb9a98c69bf839fa6ef0b9eb",
    "authorization_consumed": "b1a6ca55763c3998bfa85eb72f32b6351cd7ae7bf198161b3826d811c066dfde",
    "dispatch_failure": "32ddc5988864fbeebef094749d14a98728c384d4063789d41fd89bd8e395615e",
    "executed_plan": "3b969b34e52fe2e01e31e34b148c4ce7c211682db65b3c609a43274366c8717d",
    "empty_results": "e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855",
    "integrated_telemetry": "f090e8f2740f44b505b62137c88ace4813e3ad1a9cb5e25f542a48027feb35e4",
    "captured_ppm": "f3626f44ead6cfe4729e6ff3b76168747154b2452d0ca2afd70e1520b0e09744",
}
EXPECTED_COMPONENT_REASON = {
    '"core"|"final_edge_contrast_below_4"': 40,
    '"endpoint"|"final_edge_contrast_below_4"': 1,
    '"outer_halo_edge"|"eligible"': 49,
    '"tick"|"eligible"': 1,
    '"tick"|"final_edge_contrast_below_4"': 4,
}


def sha(path: Path) -> str:
    return hashlib.sha256(path.read_bytes()).hexdigest()


def validate(root: Path, require_canonical_hash: bool = True) -> list[str]:
    errors: list[str] = []
    evidence_path = root / "failure-evidence-receipt-v1.json"
    gate_path = root / "minimal-pairing-correction-design-gate-v1.json"
    try:
        evidence = json.loads(evidence_path.read_text())
        gate = json.loads(gate_path.read_text())
    except (OSError, ValueError, TypeError) as error:
        return [f"diagnostic_json_invalid:{type(error).__name__}"]
    if require_canonical_hash and sha(evidence_path) != EVIDENCE_SHA256:
        errors.append("canonical_evidence_hash_drift")
    if require_canonical_hash and sha(gate_path) != GATE_SHA256:
        errors.append("canonical_gate_hash_drift")
    if evidence.get("schema") != \
            "billiardgl.phase4.final_ownership_v4_ordinal1_offline_evidence.v1" or \
            evidence.get("status") != "ROOT_CAUSE_ISOLATED_OFFLINE_NO_GPU" or \
            evidence.get("gpu_cocoa_opengl_invoked_by_diagnostic") is not False or \
            evidence.get("retry_authorized") is not False or \
            evidence.get("authorization_reusable") is not False or \
            evidence.get("production_or_runner_modified") is not False:
        errors.append("evidence_boundary_invalid")
    files = evidence.get("frozen_files", {})
    if set(files) != set(FROZEN):
        errors.append("frozen_inventory_invalid")
    else:
        for name, digest in FROZEN.items():
            try:
                item = files[name]
                path = Path(item["path"])
                if item.get("sha256") != digest or not path.is_file() or \
                        sha(path) != digest or item.get("mode_octal") != "0o444" or \
                        stat.S_IMODE(path.stat().st_mode) != 0o444 or \
                        item.get("size_bytes") != path.stat().st_size:
                    errors.append(f"frozen_binding_invalid:{name}")
            except (KeyError, OSError, TypeError):
                errors.append(f"frozen_binding_invalid:{name}")
    decision = evidence.get("dispatch_decision", {})
    if decision != {"status": "CAPTURE_FAILED_TELEMETRY_PRESERVED",
                     "runner_exit_code": 1, "completed_count": 0,
                     "failed_count": 1, "unrun_count": 10,
                     "failed_capture_id": "camera_player_low_1080"}:
        errors.append("dispatch_decision_invalid")
    integrated = evidence.get("integrated_telemetry_decision", {})
    if integrated.get("integrated_final_ownership") is not True or \
            integrated.get("machine_status") != "FAILED" or \
            integrated.get("machine_error_code") != "compound_visibility_below_contract" or \
            integrated.get("source_visible_hidden") != [95, 50, 45] or \
            integrated.get("unowned_evidence_count") != 0 or \
            integrated.get("foreign_track_evidence_count") != 0 or \
            integrated.get("double_assignment_count") != 0:
        errors.append("integrated_decision_invalid")
    candidate = evidence.get("candidate_audit", {})
    if candidate.get("candidate_count") != 95 or \
            candidate.get("eligible_count") != 50 or \
            candidate.get("contrast_below_4_count") != 45 or \
            candidate.get("all_candidates_have_same_track_owner") is not True or \
            candidate.get("component_by_reason") != EXPECTED_COMPONENT_REASON:
        errors.append("candidate_matrix_invalid")
    arbitration = evidence.get("arbitration_audit", {})
    if arbitration.get("record_count") != 5286 or \
            arbitration.get("unique_physical_sample_or_pixel_ids") != 5286 or \
            arbitration.get("winner_source_probe_count") != 95 or \
            arbitration.get("pixel_already_assigned_record_count") != 3878:
        errors.append("arbitration_matrix_invalid")
    comparison = evidence.get("frozen_comparison", {})
    if comparison.get("same_ppm_as_pre_instrumentation_failure") is not True or \
            comparison.get("legacy_ppm_gate_counts") != \
            {"source": 95, "visible": 46, "hidden": 49, "coverage": 0.484211} or \
            comparison.get("new_integrated_row_source_visible_hidden") != [95, 50, 45]:
        errors.append("frozen_comparison_invalid")
    root_cause = evidence.get("root_cause", {})
    if root_cause.get("isolated") is not True or root_cause.get("id") != \
            "DUAL_LEGACY_PPM_GATE_AND_COMPONENT_UNAWARE_FIRST_DIFFERENT_TOKEN_LDR_PAIRING":
        errors.append("root_cause_invalid")
    hypotheses = {item.get("id"): item.get("decision")
                  for item in evidence.get("hypotheses", [])}
    if hypotheses != {
        "H1_SEMANTIC_OWNERSHIP_NOT_CONSUMED": "FALSIFIED",
        "H2_LEGACY_CLASSIFIER_STILL_AUTHORITATIVE": "CONFIRMED",
        "H3_COMPONENT_UNAWARE_LDR_PAIR_SELECTION": "CONFIRMED",
        "H4_EXCLUSIVE_ARBITRATION_LOST_OWNERS": "FALSIFIED",
        "H5_FULLY_INVISIBLE_OR_UNBLENDED_OVERLAY": "FALSIFIED_AS_GLOBAL_CAUSE_INCONCLUSIVE_FOR_45",
        "H6_GL1282_OR_CAPTURE_INFRASTRUCTURE_RECURRED": "FALSIFIED",
    }:
        errors.append("hypothesis_decisions_invalid")
    renderer = (ROOT / "src/Billiards/render/modern/modern_renderer.cpp").read_text()
    runner = (ROOT / "build-integrated-phase2-v4-phase4-ownership-gl1282-fix-v1/generated/phase4_overlay_v4_gpu_capture_runner.mm").read_text()
    for token in ("semanticDiffers(x,glY,", "pixel.adjacentNonSelfLdrSrgb8=colorAt(", "break;"):
        if token not in renderer:
            errors.append(f"renderer_source_evidence_missing:{token}")
    for token in ("ownershipCaptured && ownership.ok &&",
                  "contrast.ok && contrast.minimumLocalContrastRatio >= 4.0"):
        if token not in runner:
            errors.append(f"runner_source_evidence_missing:{token}")
    if gate.get("schema") != \
            "billiardgl.phase4.final_ownership_v4_pairing_correction_design_gate.v1" or \
            gate.get("status") != "DESIGN_ONLY_NO_IMPLEMENTATION_NO_CAPTURE_AUTHORIZATION" or \
            gate.get("bound_evidence_receipt_sha256") != sha(evidence_path) or \
            gate.get("capture_authorized") is not False or \
            gate.get("formal68_authorized") is not False or \
            gate.get("technical_art_review_authorized") is not False:
        errors.append("design_gate_boundary_invalid")
    invariants = gate.get("immutable_invariants", {})
    if invariants.get("source_probe_count") != 95 or \
            invariants.get("minimum_visible_coverage") != 0.90 or \
            invariants.get("minimum_edge_contrast_ratio") != 4.0 or \
            any(invariants.get(key) is not True for key in (
                "probe_route_offset_z_order_unchanged", "palette_assets_ui_unchanged",
                "production_color_bytes_unchanged")):
        errors.append("design_gate_invariants_invalid")
    steps = gate.get("required_implementation_sequence", [])
    if [step.get("id") for step in steps] != [
            "A_SERIALIZE_COMPLETE_COMPONENT_AWARE_BOUNDARY_SET",
            "B_DETERMINISTIC_ROLE_AWARE_PAIR_SELECTION",
            "C_SINGLE_AUTHORITATIVE_INTEGRATED_GATE"]:
        errors.append("design_gate_sequence_invalid")
    fixtures = gate.get("required_cpu_negative_positive_fixtures", [])
    if len(fixtures) != 9 or not any("different-token seam" in item for item in fixtures) or \
            not any("duplicate legacy contrast.ok" in item for item in fixtures):
        errors.append("design_gate_fixtures_invalid")
    return errors


def main() -> None:
    parser = argparse.ArgumentParser()
    parser.add_argument("diagnostic_root", type=Path, nargs="?", default=DEFAULT_ROOT)
    args = parser.parse_args()
    errors = validate(args.diagnostic_root)
    print(json.dumps({"status": "PASS" if not errors else "FAIL",
                      "gpu_cocoa_opengl_invoked": False, "errors": errors},
                     indent=2, sort_keys=True))
    if errors:
        raise SystemExit(1)


if __name__ == "__main__":
    main()
