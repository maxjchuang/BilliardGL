#!/usr/bin/env python3
"""Fail-closed validator for the Phase4 v12 CPU-only exhaustion receipt."""

from __future__ import annotations

import argparse
import hashlib
import json
from pathlib import Path


ROOT = Path(__file__).resolve().parents[1]
DEFAULT_RECEIPT = ROOT / "docs/art/phase4-v12-visual-local-alternatives-exhaustion-v1/exhaustion-evidence-receipt-v1.json"


def sha256(path: Path) -> str:
    return hashlib.sha256(path.read_bytes()).hexdigest()


def variant_bytes(source: str, width: float) -> bytes:
    width_text = f"{width:.1f}"
    replacements = (
        ("appendNonOverwritingOuterBand(outerRimVertices, segment.first,\n            segment.second, style.lineWidthPx, style.haloWidthPx,",
         f"appendNonOverwritingOuterBand(outerRimVertices, segment.first,\n            segment.second, {width_text}, style.haloWidthPx,"),
        ("appendHaloSegment(innerHaloVertices, segment.first, segment.second,\n            style.lineWidthPx, style.haloWidthPx, innerHalo,",
         f"appendHaloSegment(innerHaloVertices, segment.first, segment.second,\n            {width_text}, style.haloWidthPx, innerHalo,"),
        ("appendInternalJoinOverlappedSegment(colorVertices, segment.first,\n            segment.second, style.lineWidthPx,",
         f"appendInternalJoinOverlappedSegment(colorVertices, segment.first,\n            segment.second, {width_text},"),
    )
    for old, new in replacements:
        if source.count(old) != 1:
            raise ValueError("variant_transform_anchor_invalid")
        source = source.replace(old, new)
    return source.encode()


def validate(receipt_path: Path) -> list[str]:
    errors: list[str] = []
    try:
        receipt = json.loads(receipt_path.read_text())
    except Exception as exc:
        return [f"receipt_invalid:{exc}"]
    if receipt.get("schema") != "billiardgl.phase4_v12_visual_local_alternatives_exhaustion.v1":
        errors.append("schema_invalid")
    if receipt.get("status") != "FAIL_CLOSED_LOCAL_ALTERNATIVES_EXHAUSTED":
        errors.append("status_invalid")
    bindings = receipt.get("bindings", {})
    for name, expected in bindings.items():
        path = ROOT / expected.get("path", "")
        if not path.is_file() or sha256(path) != expected.get("sha256"):
            errors.append(f"binding_invalid:{name}")
    contract = receipt.get("immutable_contract", {})
    exact_contract = {
        "frozen_gap_coordinates": [[783, 413], [892, 491], [940, 525]],
        "palettes": ["standard", "color_vision_safe"],
        "required_actual_core_largest_8_connected": 2,
        "forbid_halo_or_all_sublayer_substitution": True,
        "predicted_core_width_px": 4.5,
        "actual_internal_overlap_px": 0.75,
        "actual_width_hard_max_px": 6.0,
        "vertex_budget": 8192,
        "source_identity_and_probe_contract_immutable": True,
        "thresholds_and_ownership_immutable": True,
    }
    if contract != exact_contract:
        errors.append("immutable_contract_invalid")
    baseline = receipt.get("restored_baseline_result", {})
    for key in ("standard", "color_vision_safe"):
        values = baseline.get(key, {})
        if values.get("actual_core_pixels") != 1 or values.get("actual_core_largest_8_connected") != 1:
            errors.append(f"baseline_core_metric_invalid:{key}")
        if values.get("actual_all_sublayer_pixels") != 39 or values.get("actual_all_sublayer_largest_8_connected") != 20:
            errors.append(f"baseline_all_sublayer_metric_invalid:{key}")
    source_path = ROOT / bindings.get("restored_renderer_cpp", {}).get("path", "")
    source = source_path.read_text() if source_path.is_file() else ""
    sweep = receipt.get("actual_route_body_width_sweep", [])
    if [row.get("width_px") for row in sweep] != [5.0, 5.5, 6.0]:
        errors.append("width_sweep_order_or_boundary_invalid")
    for row in sweep:
        width = row.get("width_px")
        try:
            computed_variant = hashlib.sha256(variant_bytes(source, width)).hexdigest()
        except Exception as exc:
            errors.append(f"variant_transform_invalid:{width}:{exc}")
            continue
        if computed_variant != row.get("variant_source_sha256"):
            errors.append(f"variant_source_sha_invalid:{width}")
        for field in ("standard_actual_core_pixels", "standard_actual_core_largest_8_connected", "cvd_actual_core_pixels", "cvd_actual_core_largest_8_connected"):
            if row.get(field) != 1:
                errors.append(f"sweep_metric_invalid:{width}:{field}")
        if row.get("exit_code") != 1:
            errors.append(f"sweep_exit_invalid:{width}")
    joins = receipt.get("join_trials", [])
    if len(joins) != 2 or joins[0].get("error_code") != "trajectory_overlay_actual_vertex_budget" or joins[1].get("standard_actual_core_largest_8_connected") != 1 or joins[1].get("cvd_actual_core_largest_8_connected") != 1:
        errors.append("join_trials_invalid")
    boundary = receipt.get("root_cause_boundary", {})
    if boundary.get("isolated") is not True or boundary.get("classification") != "tangent_gap_last_draw_competition_not_normal_width_or_internal_join_crack" or boundary.get("next_action") != "NEW_PREDICTED_CAP_OR_GAP_SCOPED_DESIGN_GATE_REQUIRED":
        errors.append("root_cause_boundary_invalid")
    auth = receipt.get("authorization", {})
    if set(auth.values()) != {False} or len(auth) != 4:
        errors.append("authorization_boundary_invalid")
    return errors


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--receipt", type=Path, default=DEFAULT_RECEIPT)
    args = parser.parse_args()
    errors = validate(args.receipt)
    if errors:
        print(json.dumps({"status": "FAIL", "errors": errors}, sort_keys=True))
        return 1
    print(json.dumps({"status": "PASS", "sweep": "4.5,5.0,5.5,6.0 exhausted", "gpu": "NOT_RUN"}, sort_keys=True))
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
