#!/usr/bin/env python3
"""Validate frozen CPU evidence that falsifies the v14 cap-only premise."""

from __future__ import annotations

import argparse
import hashlib
import json
from pathlib import Path


ROOT = Path(__file__).resolve().parents[1]
DEFAULT = ROOT / "docs/art/phase4-v14-cap-mask-premise-falsification-v1/premise-falsification-receipt-v1.json"


def sha(path: Path) -> str:
    return hashlib.sha256(path.read_bytes()).hexdigest()


def valid_hash(value: object) -> bool:
    return isinstance(value, str) and len(value) == 64 and all(c in "0123456789abcdef" for c in value)


def validate(path: Path) -> list[str]:
    errors: list[str] = []
    try:
        value = json.loads(path.read_text())
    except Exception as exc:
        return [f"receipt_invalid:{exc}"]
    if value.get("schema") != "billiardgl.phase4.v14.cap_mask_premise_falsification.v1":
        errors.append("schema_invalid")
    if value.get("status") != "PREMISE_FALSIFIED_FAIL_CLOSED_PARTIAL_IMPLEMENTATION_REVERTED":
        errors.append("status_invalid")
    for group in ("frozen_bindings", "required_restored_production_baselines"):
        for role, item in value.get(group, {}).items():
            target = ROOT / item.get("path", "")
            if not target.is_file() or sha(target) != item.get("sha256"):
                errors.append(f"binding_invalid:{group}:{role}")
    candidate = value.get("frozen_partial_candidate", {})
    for field in ("trajectory_renderer_cpp_sha256", "trajectory_renderer_h_sha256", "modern_renderer_cpp_sha256", "modern_renderer_h_sha256", "diagnostic_test_source_sha256", "diagnostic_test_object_sha256"):
        if not valid_hash(candidate.get(field)):
            errors.append(f"candidate_hash_invalid:{field}")
    binary = candidate.get("diagnostic_binary", {})
    binary_path = ROOT / binary.get("path", "")
    if not binary_path.is_file() or sha(binary_path) != binary.get("sha256"):
        errors.append("diagnostic_binary_binding_invalid")
    if candidate.get("exit_code") != 1:
        errors.append("diagnostic_exit_invalid")
    fixture = value.get("exact_fixture", {})
    if fixture != {"capture_id": "camera_broadcast_1080", "source_probe_count": 81, "ordered_source_digest": "33511aa46717a208f3db43acd278416b8fe8393e2e003484da8f44dabffe5645", "blocking_coordinate": [940, 525], "neighborhood_radius_px": 8, "palettes": ["standard", "color_vision_safe"], "sample_route": "single_sample_center_0.5_0.5"}:
        errors.append("fixture_invalid")
    metrics = value.get("observed_metrics_per_palette", {})
    exact_metrics = {"actual_route_body_source_mask_pixels": 88, "tagged_predicted_inner_cap_discards": 59, "final_source_mask_pixels_owned_by_predicted_core": 81, "final_source_mask_pixels_owned_by_untagged_predicted_inner_body": 7, "final_source_mask_pixels_owned_by_tagged_predicted_inner_cap": 0, "final_actual_route_body_pixels": 0, "legacy_helper_actual_core_pixels": 1, "legacy_helper_actual_core_largest8": 1, "final_actual_tick_pixels": 1, "all_actual_sublayer_pixels": 39, "all_actual_sublayer_largest8": 20, "required_actual_route_body_largest8": 2}
    if metrics != exact_metrics or metrics.get("final_source_mask_pixels_owned_by_predicted_core", 0) + metrics.get("final_source_mask_pixels_owned_by_untagged_predicted_inner_body", 0) != metrics.get("actual_route_body_source_mask_pixels"):
        errors.append("metrics_invalid")
    premise = value.get("premise_falsification", {})
    if premise.get("cap_predicate_executed") is not True or premise.get("cap_predicate_was_finally_authoritative") is not False or premise.get("v14_cap_only_strategy_can_meet_required_result") is not False or premise.get("successor_design_gate_required") is not True or "Tick" not in premise.get("legacy_helper_misclassification", ""):
        errors.append("premise_conclusion_invalid")
    revert = value.get("revert_boundary", {})
    if len(revert) != 3 or set(revert.values()) != {True}:
        errors.append("revert_boundary_invalid")
    auth = value.get("authorization", {})
    if len(auth) != 5 or set(auth.values()) != {False}:
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
