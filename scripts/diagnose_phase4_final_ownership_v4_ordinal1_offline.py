#!/usr/bin/env python3
"""Freeze and diagnose the consumed Phase4 v4 ordinal-1 failure, CPU-only."""

from __future__ import annotations

from collections import Counter
import hashlib
import json
from pathlib import Path
import stat

ROOT = Path(__file__).resolve().parents[1]
OUT = ROOT / "docs/art/phase4-final-ownership-v4-ordinal1-offline-diagnostic-v1"
READY = ROOT / "docs/art/phase4-training-replay-overlay-v4-final-ownership-targeted11-ready-v4/targeted11-readiness-v4.json"
OUTPUT = ROOT / "docs/art/phase4-training-replay-overlay-v4-final-ownership-targeted11-output-v4"
AUTH = ROOT / "docs/evidence/phase4-final-ownership-targeted11-v4-authorization-root-v1.json"
CONSUMED = Path(str(AUTH) + ".consumed")
FAILURE = OUTPUT / "phase4-final-ownership-dispatch-failure-receipt-v1.json"
PLAN = OUTPUT / "phase4-final-ownership-targeted11-plan.tsv"
RESULTS = OUTPUT / "results.jsonl"
TELEMETRY = OUTPUT / "results.jsonl.final-ownership.jsonl"
PPM = OUTPUT / "camera_player_low_1080.ppm"
OLD_ROW = ROOT / "docs/art/phase4-training-replay-overlay-v4-final-ownership-targeted11-ready-v4/frozen-ordinal1-read-only-failure-row.jsonl"
OLD_GATE = ROOT / "docs/art/phase4-overlay-v4-final-ownership-instrumentation-implementation-design-gate-v1.json"
RENDERER = ROOT / "src/Billiards/render/modern/modern_renderer.cpp"
RUNNER = ROOT / "build-integrated-phase2-v4-phase4-ownership-gl1282-fix-v1/generated/phase4_overlay_v4_gpu_capture_runner.mm"

PINNED = {
    "authorization": "629f8309a9393de01bd69a1b252f3b6023398ba8eb9a98c69bf839fa6ef0b9eb",
    "authorization_consumed": "b1a6ca55763c3998bfa85eb72f32b6351cd7ae7bf198161b3826d811c066dfde",
    "readiness": "a823fbae2f291404673ec3ec55c50c48851e9c32e9d0ad15d50551163716d33b",
    "dispatch_failure": "32ddc5988864fbeebef094749d14a98728c384d4063789d41fd89bd8e395615e",
    "plan": "3b969b34e52fe2e01e31e34b148c4ce7c211682db65b3c609a43274366c8717d",
    "results": "e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855",
    "telemetry": "f090e8f2740f44b505b62137c88ace4813e3ad1a9cb5e25f542a48027feb35e4",
    "ppm": "f3626f44ead6cfe4729e6ff3b76168747154b2452d0ca2afd70e1520b0e09744",
}


def sha(path: Path) -> str:
    return hashlib.sha256(path.read_bytes()).hexdigest()


def frozen(path: Path, expected: str) -> dict:
    if not path.is_file() or sha(path) != expected:
        raise RuntimeError(f"frozen binding drift:{path}")
    mode = stat.S_IMODE(path.stat().st_mode)
    if mode != 0o444:
        raise RuntimeError(f"frozen mode must be 0444:{path}:{oct(mode)}")
    return {"path": str(path), "sha256": expected,
            "mode_octal": "0o444", "size_bytes": path.stat().st_size}


def count_pairs(rows: list[dict], *fields: str) -> dict[str, int]:
    counts = Counter("|".join(json.dumps(row[field], separators=(",", ":"))
                              for field in fields) for row in rows)
    return dict(sorted(counts.items()))


def main() -> None:
    if OUT.exists():
        raise RuntimeError("fresh diagnostic root must not exist")
    files = {
        "authorization": frozen(AUTH, PINNED["authorization"]),
        "authorization_consumed": frozen(CONSUMED, PINNED["authorization_consumed"]),
        "dispatch_failure": frozen(FAILURE, PINNED["dispatch_failure"]),
        "executed_plan": frozen(PLAN, PINNED["plan"]),
        "empty_results": frozen(RESULTS, PINNED["results"]),
        "integrated_telemetry": frozen(TELEMETRY, PINNED["telemetry"]),
        "captured_ppm": frozen(PPM, PINNED["ppm"]),
    }
    if sha(READY) != PINNED["readiness"]:
        raise RuntimeError("readiness drift")
    lines = TELEMETRY.read_text().splitlines()
    if len(lines) != 1:
        raise RuntimeError("exactly one telemetry row required")
    row = json.loads(lines[0])
    old = json.loads(OLD_ROW.read_text())
    old_gate = json.loads(OLD_GATE.read_text())
    candidates = row["ownership_candidate_records"]
    arbitration = row["exclusive_arbitration_records"]
    if len(candidates) != 95 or len(row["ordered_source_probe_ids"]) != 95:
        raise RuntimeError("95 immutable source records required")

    component_reason = count_pairs(candidates, "owner_component",
                                   "eligibility_or_rejection_reason")
    color_reason = count_pairs(candidates, "final_rgb_srgb8",
                               "adjacent_non_self_rgb_srgb8",
                               "eligibility_or_rejection_reason")
    claims = Counter(len(item["claiming_source_probe_ids"])
                     for item in arbitration)
    failure = json.loads(FAILURE.read_text())
    receipt = {
        "schema": "billiardgl.phase4.final_ownership_v4_ordinal1_offline_evidence.v1",
        "status": "ROOT_CAUSE_ISOLATED_OFFLINE_NO_GPU",
        "scope": "Read-only diagnosis of the consumed v4 ordinal-1 failure; no retry, capture, production change, threshold change, readiness or authorization.",
        "gpu_cocoa_opengl_invoked_by_diagnostic": False,
        "authorization_reusable": False,
        "retry_authorized": False,
        "production_or_runner_modified": False,
        "frozen_files": files,
        "readiness_binding": {"path": str(READY),
                              "sha256": PINNED["readiness"]},
        "dispatch_decision": {
            "status": failure["status"], "runner_exit_code": failure["runner_exit_code"],
            "completed_count": failure["completed_count"],
            "failed_count": failure["failed_count"],
            "unrun_count": failure["unrun_count"],
            "failed_capture_id": failure["failed_capture_id"],
        },
        "integrated_telemetry_decision": {
            "integrated_final_ownership": row["integrated_final_ownership"],
            "machine_status": row["machine_status"],
            "machine_error_code": row["machine_error_code"],
            "source_visible_hidden": [row["source_probe_count"],
                                      row["visible_probe_count"],
                                      row["hidden_probe_count"]],
            "aggregate_coverage": row["aggregate_visible_probe_coverage"],
            "predicted": row["predicted_source_visible_hidden_and_coverage"],
            "actual": row["actual_source_visible_hidden_and_coverage"],
            "unowned_evidence_count": row["unowned_evidence_count"],
            "foreign_track_evidence_count": row["foreign_track_evidence_count"],
            "double_assignment_count": row["double_assignment_count"],
            "ordered_source_probe_digest": row["ordered_source_probe_digest"],
        },
        "candidate_audit": {
            "candidate_count": len(candidates),
            "component_by_reason": component_reason,
            "final_adjacent_color_by_reason": color_reason,
            "eligible_count": sum(item["eligibility_or_rejection_reason"] == "eligible"
                                  for item in candidates),
            "contrast_below_4_count": sum(item["eligibility_or_rejection_reason"] ==
                                          "final_edge_contrast_below_4"
                                          for item in candidates),
            "all_candidates_have_same_track_owner": all(item["owner_track"] in
                ("predicted", "actual") for item in candidates),
        },
        "arbitration_audit": {
            "record_count": len(arbitration),
            "unique_physical_sample_or_pixel_ids": len({item[
                "physical_sample_or_pixel_id"] for item in arbitration}),
            "winner_source_probe_count": len({item["winner_source_probe_id"]
                                               for item in arbitration}),
            "claim_count_histogram": {str(key): value for key, value in sorted(claims.items())},
            "pixel_already_assigned_record_count": sum(item["loser_reason"] ==
                "pixel_already_assigned" for item in arbitration),
        },
        "frozen_comparison": {
            "same_ppm_as_pre_instrumentation_failure": sha(PPM) ==
                old_gate["input_bindings"]["frozen_ordinal1_ppm"]["sha256"],
            "legacy_ppm_gate_counts": old_gate["frozen_diagnostic_decision"]["frozen_counts"],
            "old_read_only_row": {"integrated_final_ownership": old[
                "integrated_final_ownership"], "source_visible_hidden": [old[
                "source_probe_count"], old["visible_probe_count"], old["hidden_probe_count"]]},
            "new_integrated_row_source_visible_hidden": [row["source_probe_count"],
                row["visible_probe_count"], row["hidden_probe_count"]],
        },
        "root_cause": {
            "isolated": True,
            "id": "DUAL_LEGACY_PPM_GATE_AND_COMPONENT_UNAWARE_FIRST_DIFFERENT_TOKEN_LDR_PAIRING",
            "confidence": "HIGH_FROZEN_TELEMETRY_AND_SOURCE_MATCH",
            "mechanism": [
                "machinePass still requires legacy contrast.ok in addition to ownership.ok, so the frozen 46/49 PPM classifier remains authoritative after integrated ownership was added",
                "paired LDR selection stops at the first four-neighbor whose raw semantic token differs, without distinguishing same compound, component, primitive, inner edge or outer silhouette",
                "the resulting pattern marks all 40 core candidates hidden while all 49 halo candidates are eligible; 39 predicted halo records use black-to-yellow inner edges and 37 predicted core records use yellow-to-yellow token boundaries",
            ],
            "actual_overlay_color_or_blend_invisibility": "NOT_ISOLATED: the unchanged PPM contains at least 50 semantically owned >=4 contrast records, but current adjacency records cannot decide whether the remaining 45 represent true silhouette failure or pairing error",
        },
        "hypotheses": [
            {"id": "H1_SEMANTIC_OWNERSHIP_NOT_CONSUMED", "decision": "FALSIFIED",
             "evidence": "integrated=true; 95 candidates have same-track owners; unowned/foreign/double are zero; machinePass explicitly requires ownership.ok"},
            {"id": "H2_LEGACY_CLASSIFIER_STILL_AUTHORITATIVE", "decision": "CONFIRMED",
             "evidence": "runner machinePass explicitly requires contrast.ok; the PPM is byte-identical to the frozen 46/49 input"},
            {"id": "H3_COMPONENT_UNAWARE_LDR_PAIR_SELECTION", "decision": "CONFIRMED",
             "evidence": "renderer breaks on first semanticDiffers neighbor; frozen candidate color/component matrix shows systematic core/halo inversion"},
            {"id": "H4_EXCLUSIVE_ARBITRATION_LOST_OWNERS", "decision": "FALSIFIED",
             "evidence": "5286 unique arbitration records cover 95 winner IDs; double assignment is zero"},
            {"id": "H5_FULLY_INVISIBLE_OR_UNBLENDED_OVERLAY", "decision": "FALSIFIED_AS_GLOBAL_CAUSE_INCONCLUSIVE_FOR_45",
             "evidence": "50 records have exact final LDR contrast >=4; current pairing is insufficient to adjudicate the other 45"},
            {"id": "H6_GL1282_OR_CAPTURE_INFRASTRUCTURE_RECURRED", "decision": "FALSIFIED",
             "evidence": "one complete integrated telemetry row was durably preserved and dispatch classified one ordinal failure"},
        ],
        "source_evidence": {
            "renderer_sha256": sha(RENDERER), "runner_source_sha256": sha(RUNNER),
            "renderer_first_different_neighbor_tokens": [
                "semanticDiffers(x,glY,", "pixel.adjacentNonSelfLdrSrgb8=colorAt(", "break;"],
            "runner_dual_gate_tokens": ["ownershipCaptured && ownership.ok &&",
                                        "contrast.ok && contrast.minimumLocalContrastRatio >= 4.0"],
        },
    }
    OUT.mkdir(parents=True)
    evidence = OUT / "failure-evidence-receipt-v1.json"
    evidence.write_text(json.dumps(receipt, indent=2, sort_keys=True) + "\n")
    gate = {
        "schema": "billiardgl.phase4.final_ownership_v4_pairing_correction_design_gate.v1",
        "status": "DESIGN_ONLY_NO_IMPLEMENTATION_NO_CAPTURE_AUTHORIZATION",
        "scope": "Authorize only a future correction of final-LDR boundary pairing and retirement of the duplicate legacy PPM pass/fail decision after equivalence fixtures pass.",
        "bound_evidence_receipt_path": str(evidence),
        "bound_evidence_receipt_sha256": sha(evidence),
        "immutable_invariants": {
            "source_probe_count": 95, "minimum_visible_coverage": 0.90,
            "minimum_edge_contrast_ratio": 4.0, "probe_route_offset_z_order_unchanged": True,
            "palette_assets_ui_unchanged": True, "production_color_bytes_unchanged": True,
        },
        "required_implementation_sequence": [
            {"ordinal": 1, "id": "A_SERIALIZE_COMPLETE_COMPONENT_AWARE_BOUNDARY_SET",
             "requirements": [
                 "For every selected owned island serialize every four-connected boundary pair, both coordinates, both per-sample semantic owners, UI state, exact final LDR colors, contrast, boundary class and deterministic sort key",
                 "Classify boundaries as same-component token seam, same-track compound inner edge, compound outer silhouette, foreign-track edge, UI edge or background/scene edge",
                 "A missing or truncated boundary set fails closed before any machine decision",
             ]},
            {"ordinal": 2, "id": "B_DETERMINISTIC_ROLE_AWARE_PAIR_SELECTION",
             "requirements": [
                 "Never treat a raw token difference alone as an eligible non-self edge",
                 "Core, tick and endpoint evidence may use only gate-approved same-track compound inner edges or true outer silhouettes; outer_halo_edge evidence may use only the outer silhouette, never its inner core edge",
                 "Choose deterministically from the complete eligible boundary set by contract-defined role, distance, coordinate and direction; record all alternatives and the selected pair",
                 "Use exact paired final LDR bytes and the unchanged >=4.0 threshold",
             ]},
            {"ordinal": 3, "id": "C_SINGLE_AUTHORITATIVE_INTEGRATED_GATE",
             "requirements": [
                 "Keep legacy PPM contrast telemetry for comparison but remove it from machinePass only after CPU replay proves the integrated result is complete and fail-closed",
                 "machinePass must have exactly one authoritative final visibility decision: corrected integrated final semantic ownership",
                 "Any discrepancy between legacy telemetry and integrated telemetry is serialized, never silently reconciled",
             ]},
        ],
        "required_cpu_negative_positive_fixtures": [
            "N1 same-color different-token seam is not an eligible edge",
            "N2 halo-to-core inner edge cannot satisfy an outer_halo_edge probe",
            "P1 core-to-approved-halo inner edge with final LDR contrast >=4 is eligible",
            "P2 true compound outer silhouette is eligible for outer_halo_edge",
            "N3 changing four-neighbor iteration order cannot change classification",
            "N4 foreign-track and later-UI boundaries remain ineligible",
            "N5 missing boundary alternatives or selected-pair provenance fails closed",
            "N6 duplicate legacy contrast.ok in machinePass is statically rejected",
            "P3 frozen v4 telemetry is replayed read-only and preserves 95 IDs without claiming a corrected count",
        ],
        "exit_conditions_before_any_fresh_readiness": [
            "All CPU fixtures and mutation tests pass",
            "Frozen v4 auth, consumed marker, PPM, telemetry, results, plan and failure receipt hashes/modes remain unchanged",
            "Static graph proves one integrated authoritative gate and legacy contrast is diagnostic-only",
            "No corrected ordinal-1 coverage is claimed without a separately authorized fresh capture",
        ],
        "forbidden": [
            "GPU/Cocoa/OpenGL execution or retry", "new readiness or authorization",
            "production palette, asset, geometry, route, offset, z-order or UI change",
            "threshold, denominator or probe change", "rewriting any frozen v4 artifact",
        ],
        "capture_authorized": False, "formal68_authorized": False,
        "technical_art_review_authorized": False,
    }
    gate_path = OUT / "minimal-pairing-correction-design-gate-v1.json"
    gate_path.write_text(json.dumps(gate, indent=2, sort_keys=True) + "\n")
    print(json.dumps({"evidence": str(evidence), "evidence_sha256": sha(evidence),
                      "gate": str(gate_path), "gate_sha256": sha(gate_path),
                      "gpu_cocoa_opengl_invoked": False}, indent=2, sort_keys=True))


if __name__ == "__main__":
    main()
