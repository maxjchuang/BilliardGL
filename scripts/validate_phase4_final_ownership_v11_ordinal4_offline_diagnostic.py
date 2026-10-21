#!/usr/bin/env python3
"""Fail-closed validator for the frozen Phase4 v11 ordinal-4 diagnosis."""

from __future__ import annotations

import argparse
import hashlib
import importlib.util
import json
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
DIAGNOSTIC = ROOT / "scripts/diagnose_phase4_final_ownership_v11_ordinal4_offline.py"
DEFAULT_RECEIPT = ROOT / "docs/art/phase4-final-ownership-v11-ordinal4-offline-diagnostic-v1/failure-evidence-receipt-v1.json"
DEFAULT_GATE = ROOT / "docs/art/phase4-final-ownership-v11-ordinal4-offline-diagnostic-v1/minimal-raster-support-connectivity-correction-design-gate-v1.json"
RECEIPT_SHA256 = "07f251d466b0dcad550b4951e8242403bcaaaeb97dd800cbde99cdf23c2f3f85"
GATE_SHA256 = "59bbabf437e9195392b2a41f4e48eea0513afb222706115a07e9c0965cbad059"
DIAGNOSTIC_SHA256 = "d406151148e7120363699e4f50760e0341bd7d69038d657c0d7b3fead8610ce6"
SOURCE_SHA256 = {
    "src/Billiards/training/final_semantic_ownership.cpp":
        "7d182d918e00920ad178606fda6371319c4f5cb10d5bf9ca0688434ac9a2f704",
    "src/Billiards/training/final_semantic_ownership.h":
        "6b66fd2f6a186ace953c24a5214fda16e56314209fffe413d2f27fc4665bc1ac",
    "src/Billiards/render/modern/modern_renderer.cpp":
        "b00cc6b370122275a42f106967f935dbb34bd5d3f81f2a750ca9ba6e503af191",
    "scripts/generate_phase4_overlay_v4_gpu_runner.py":
        "bd084842a10065e6f20b5826c8d38407230be2e82729fe29769dd13b4004c5a1",
}


def sha(path: Path) -> str:
    return hashlib.sha256(path.read_bytes()).hexdigest()


def load_diagnostic_module():
    specification = importlib.util.spec_from_file_location(
        "phase4_v11_offline_diagnostic", DIAGNOSTIC)
    if specification is None or specification.loader is None:
        raise RuntimeError("diagnostic_module_unloadable")
    module = importlib.util.module_from_spec(specification)
    specification.loader.exec_module(module)
    return module


def validate(receipt_path: Path, gate_path: Path) -> list[str]:
    errors: list[str] = []
    if sha(DIAGNOSTIC) != DIAGNOSTIC_SHA256:
        errors.append("diagnostic_script_drift")
    for relative, expected in SOURCE_SHA256.items():
        path = ROOT / relative
        if not path.is_file() or sha(path) != expected:
            errors.append(f"production_or_runner_source_drift:{relative}")
    try:
        receipt = json.loads(receipt_path.read_text())
        gate = json.loads(gate_path.read_text())
    except (OSError, ValueError, TypeError) as error:
        return errors + [f"diagnostic_artifact_invalid:{type(error).__name__}"]
    if receipt_path == DEFAULT_RECEIPT and sha(receipt_path) != RECEIPT_SHA256:
        errors.append("canonical_receipt_hash_drift")
    if gate_path == DEFAULT_GATE and sha(gate_path) != GATE_SHA256:
        errors.append("canonical_gate_hash_drift")

    try:
        summary = load_diagnostic_module().summarize()
    except Exception as error:  # fail closed on any frozen evidence mismatch
        return errors + [f"frozen_diagnostic_failed:{error}"]

    ordinals = summary["ordinals"]
    if [(item["ordinal"], item["capture_id"], item["machine_status"])
            for item in ordinals] != [
                (1, "camera_player_low_1080", "PASS"),
                (2, "camera_elevated_1080", "PASS"),
                (3, "camera_overhead_1080", "PASS"),
                (4, "camera_broadcast_1080", "FAILED")]:
        errors.append("ordinal_inventory_or_status_invalid")
    if any(item["predicate_count"] != 41 for item in ordinals) or \
            [item["failed_predicates"] for item in ordinals] != \
            [[], [], [], ["ownership_ok"]]:
        errors.append("predicate_ledger_semantics_invalid")
    if summary["dispatch"] != {
            "status": "CAPTURE_FAILED_TELEMETRY_PRESERVED",
            "completed_failed_unrun": [3, 1, 7],
            "failed_capture_id": "camera_broadcast_1080",
            "authorization_consumed_once": True,
            "validation_errors": []}:
        errors.append("dispatch_freeze_semantics_invalid")

    hidden = summary["ordinal4_hidden_details"]
    area = [item for item in hidden
            if item["reason"] == "insufficient_owned_pixel_area"]
    contrast = [item for item in hidden
                if item["reason"] == "final_edge_contrast_below_4"]
    if len(hidden) != 13 or len(area) != 11 or len(contrast) != 2 or \
            any(item["resolved_pixel_equivalent_area"] < 1.5 for item in area) or \
            any(item["exclusive_largest_four_connected"] != 1 for item in area):
        errors.append("ordinal4_hidden_partition_invalid")
    recovered = [item for item in area
                 if item["analytic_counterfactual_recovered"]]
    remaining = [item for item in area
                 if not item["analytic_counterfactual_recovered"]]
    if len(recovered) != 10 or \
            sum(item["track"] == "predicted" for item in recovered) != 5 or \
            sum(item["track"] == "actual" for item in recovered) != 5 or \
            [item["source_probe_id"] for item in remaining] != [
                "1bf60bcd841a0d754c82876b55a4d3d6c29ebad582e069d4f4a608a4778c33a9"]:
        errors.append("topology_counterfactual_partition_invalid")
    counterfactual = summary["ordinal4_counterfactual"]
    if counterfactual["aggregate_source_visible_hidden"] != [81, 78, 3] or \
            counterfactual["predicted_source_visible_hidden"] != [69, 67, 2] or \
            counterfactual["actual_source_visible_hidden"] != [12, 11, 1] or \
            min(counterfactual["aggregate_coverage"],
                counterfactual["predicted_coverage"],
                counterfactual["actual_coverage"]) < 0.90 or \
            counterfactual["analytic_non_final_not_a_machine_pass"] is not True:
        errors.append("counterfactual_semantics_invalid")

    if receipt.get("schema") != \
            "billiardgl.phase4.final_ownership_v11_ordinal4_offline_diagnostic.v1" or \
            receipt.get("status") != \
            "EXCLUSIVE_PARTITION_TOPOLOGY_FRAGMENTATION_AND_DIAGONAL_RASTER_CONNECTIVITY_DEFECT_ISOLATED_NO_RETRY_AUTHORIZED" or \
            receipt.get("gpu_cocoa_opengl_invoked") is not False or \
            receipt.get("capture_retry_authorized") is not False or \
            receipt.get("new_readiness_or_authorization_created") is not False:
        errors.append("receipt_status_or_authorization_invalid")
    expected_frozen = {
        "readiness_sha256": summary["frozen_file_sha256"][
            "docs/art/phase4-training-replay-overlay-v4-final-ownership-targeted11-ready-v11/targeted11-readiness-v11.json"],
        "source_bindings_sha256":
            "37c00af328c38db9fcecb58f540714efed228b51bb979cbdf399c7b81cf5221a",
        "authorization_sha256": summary["frozen_file_sha256"][
            "docs/art/phase4-training-replay-overlay-v4-final-ownership-targeted11-ready-v11/root-one-shot-gpu-authorization-v11-run1.json"],
        "consumed_marker_sha256": summary["frozen_file_sha256"][
            "docs/art/phase4-training-replay-overlay-v4-final-ownership-targeted11-ready-v11/root-one-shot-gpu-authorization-v11-run1.json.consumed"],
        "failure_receipt_sha256": summary["frozen_file_sha256"][
            "docs/art/phase4-training-replay-overlay-v4-final-ownership-targeted11-output-v11/phase4-final-ownership-dispatch-failure-receipt-v1.json"],
        "results_sha256": summary["frozen_file_sha256"][
            "docs/art/phase4-training-replay-overlay-v4-final-ownership-targeted11-output-v11/results.jsonl"],
        "telemetry_sha256": summary["frozen_file_sha256"][
            "docs/art/phase4-training-replay-overlay-v4-final-ownership-targeted11-output-v11/results.jsonl.final-ownership.jsonl"],
        "broadcast_ppm_sha256": summary["frozen_file_sha256"][
            "docs/art/phase4-training-replay-overlay-v4-final-ownership-targeted11-output-v11/camera_broadcast_1080.ppm"],
    }
    if receipt.get("frozen_evidence") != expected_frozen or \
            receipt.get("frozen_output_directory_mode") != "0555" or \
            receipt.get("frozen_file_mode") != "0444":
        errors.append("receipt_frozen_bindings_invalid")
    root_cause = receipt.get("root_cause", {})
    if root_cause.get("primary") != \
            "CONNECTIVITY_WAS_MEASURED_ON_PER_PROBE_EXCLUSIVE_WINNERS_INSTEAD_OF_PRE_ARBITRATION_SAME_TRACK_RASTER_SUPPORT" or \
            root_cause.get("secondary") != \
            "FOUR_CONNECTED_ONLY_REJECTS_VISUALLY_CONTIGUOUS_DIAGONAL_BROADCAST_RASTER" or \
            any(root_cause.get(key) is not True for key in (
                "not_machine_contract", "not_missing_telemetry",
                "not_ppm_legacy_gate", "not_ui_or_capture_infrastructure",
                "not_exclusive_area_count_or_double_assignment",
                "not_threshold_or_denominator")):
        errors.append("root_cause_isolation_invalid")
    receipt_cf = receipt.get("frozen_counterfactual", {})
    if receipt_cf.get("status") != "ANALYTIC_NON_FINAL_NOT_A_MACHINE_PASS" or \
            receipt_cf.get("aggregate_source_visible_hidden") != [81, 78, 3] or \
            receipt_cf.get("predicted_source_visible_hidden") != [69, 67, 2] or \
            receipt_cf.get("actual_source_visible_hidden") != [12, 11, 1] or \
            receipt_cf.get("fresh_capture_still_required") is not True:
        errors.append("receipt_counterfactual_invalid")

    required = gate.get("required_algorithm", {})
    forbidden = gate.get("forbidden_changes", [])
    fixtures = gate.get("required_cpu_fixtures", [])
    if gate.get("schema") != \
            "billiardgl.phase4.final_ownership_v11_raster_support_connectivity_correction_gate.v1" or \
            gate.get("status") != "DESIGN_ONLY_NOT_IMPLEMENTED_NO_CAPTURE_AUTHORIZED" or \
            gate.get("bound_failure_evidence_receipt_sha256") != RECEIPT_SHA256 or \
            gate.get("gpu_cocoa_opengl_authorized") is not False or \
            gate.get("capture_retry_authorized") is not False or \
            gate.get("fresh_readiness_authorized") is not False or \
            required.get("minimum_resolved_pixel_equivalent_area_remains") != 1.5 or \
            required.get("minimum_connected_raster_pixels_remains") != 2 or \
            required.get("raster_continuity_uses_deterministic_eight_neighbor_physical_pixel_adjacency") is not True or \
            required.get("scalar_area_and_topology_support_must_both_pass") is not True or \
            len(forbidden) != 9 or len(fixtures) != 9 or \
            gate.get("frozen_counterfactual_acceptance", {}).get(
                "cannot_satisfy_machine_pass") is not True:
        errors.append("minimal_design_gate_invalid")
    return errors


def main() -> None:
    parser = argparse.ArgumentParser()
    parser.add_argument("--receipt", type=Path, default=DEFAULT_RECEIPT)
    parser.add_argument("--gate", type=Path, default=DEFAULT_GATE)
    args = parser.parse_args()
    errors = validate(args.receipt.resolve(), args.gate.resolve())
    print(json.dumps({
        "schema": "billiardgl.phase4.final_ownership_v11_ordinal4_offline_diagnostic_validation.v1",
        "status": "PASS" if not errors else "FAIL",
        "gpu_cocoa_opengl_invoked": False,
        "capture_retry_authorized": False,
        "errors": errors,
    }, indent=2, sort_keys=True))
    if errors:
        raise SystemExit(1)


if __name__ == "__main__":
    main()
