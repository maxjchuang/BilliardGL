#!/usr/bin/env python3
"""Validate the fail-closed v13 ordered Primary CPU sweep receipt."""

from __future__ import annotations

import argparse
import hashlib
import json
from pathlib import Path


ROOT = Path(__file__).resolve().parents[1]
DEFAULT = ROOT / "docs/art/phase4-v13-primary-equal-cap-contraction-exhaustion-v1/primary-exhaustion-receipt-v1.json"


def digest(path: Path) -> str:
    return hashlib.sha256(path.read_bytes()).hexdigest()


def variant(source: str, cap: float) -> bytes:
    if cap == 1.5:
        return source.encode()
    old = "12.0, 2.0, 2.0, 1.5, 0.0, 0.0};"
    new = f"12.0, 2.0, 2.0, {cap}, 0.0, 0.0}};"
    if source.count(old) != 1:
        raise ValueError("full_hd_cap_anchor_invalid")
    return source.replace(old, new).encode()


def validate(path: Path) -> list[str]:
    errors: list[str] = []
    try:
        receipt = json.loads(path.read_text())
    except Exception as exc:
        return [f"receipt_invalid:{exc}"]
    if receipt.get("schema") != "billiardgl.phase4.v13.primary_equal_cap_contraction_exhaustion.v1":
        errors.append("schema_invalid")
    if receipt.get("status") != "PRIMARY_EXHAUSTED_FAIL_CLOSED_FALLBACK_NOT_IMPLEMENTED":
        errors.append("status_invalid")
    bindings = receipt.get("bindings", {})
    for role, item in bindings.items():
        target = ROOT / item.get("path", "")
        if not target.is_file() or digest(target) != item.get("sha256"):
            errors.append(f"binding_invalid:{role}")
    renderer = ROOT / bindings.get("restored_renderer_cpp", {}).get("path", "")
    source = renderer.read_text() if renderer.is_file() else ""
    sweep = receipt.get("ordered_primary_sweep", [])
    expected = [(1, 1.5, 3.5), (2, 1.25, 3.25), (3, 1.0, 3.0)]
    if [(x.get("ordinal"), x.get("core_cap_px"), x.get("inner_cap_px")) for x in sweep] != expected:
        errors.append("ordered_sweep_invalid")
    for row in sweep:
        cap = row.get("core_cap_px")
        if row.get("outer_cap_px") != row.get("inner_cap_px") or row.get("inner_cap_px") - cap != 2.0:
            errors.append(f"equal_cap_or_annulus_invalid:{cap}")
        try:
            source_sha = hashlib.sha256(variant(source, cap)).hexdigest()
        except Exception as exc:
            errors.append(f"variant_transform_invalid:{cap}:{exc}")
            continue
        if source_sha != row.get("variant_source_sha256"):
            errors.append(f"variant_source_invalid:{cap}")
        if row.get("exit_code") != 1 or row.get("result") != "FAIL_ACTUAL_CORE_1_PIXEL_AT_940_525":
            errors.append(f"candidate_result_invalid:{cap}")
        for field in ("variant_object_sha256", "execution_binary_instance_sha256", "deterministic_codesign_stripped_binary_sha256"):
            value = row.get(field, "")
            if len(value) != 64 or any(c not in "0123456789abcdef" for c in value):
                errors.append(f"candidate_hash_invalid:{cap}:{field}")
    metrics = receipt.get("shared_failure_metrics", {})
    if metrics.get("coordinate") != [940, 525] or metrics.get("required_actual_core_largest_8_connected") != 2 or metrics.get("halo_substitution_forbidden") is not True:
        errors.append("failure_metric_contract_invalid")
    for palette in ("standard", "color_vision_safe"):
        value = metrics.get(palette, {})
        if value != {"actual_core_pixels": 1, "actual_core_largest_8_connected": 1, "actual_all_sublayer_pixels": 39, "actual_all_sublayer_largest_8_connected": 20}:
            errors.append(f"failure_metrics_invalid:{palette}")
    unchanged = receipt.get("unchanged_contract", {})
    if unchanged != {"dash_on_px": 18.0, "dash_off_px": 12.0, "period_px": 30.0, "core_to_inner_annulus_px": 2.0, "outer_equals_inner_cap": True, "source_ids_counts_order_coordinates_unchanged": True, "threshold_ownership_route_phase_z_order_unchanged": True, "algorithm_receipt_six_sources_unchanged": True}:
        errors.append("unchanged_contract_invalid")
    fallback = receipt.get("fallback_boundary", {})
    if fallback != {"equal_cap_primary_exhausted": True, "fallback_implemented": False, "fallback_authorized_by_this_receipt": False, "separate_root_approval_required": True}:
        errors.append("fallback_boundary_invalid")
    auth = receipt.get("authorization", {})
    if len(auth) != 4 or set(auth.values()) != {False}:
        errors.append("authorization_invalid")
    return errors


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--receipt", type=Path, default=DEFAULT)
    args = parser.parse_args()
    errors = validate(args.receipt)
    print(json.dumps({"status": "FAIL" if errors else "PASS", "errors": errors, "gpu": "NOT_RUN"}, sort_keys=True))
    return 1 if errors else 0


if __name__ == "__main__":
    raise SystemExit(main())
