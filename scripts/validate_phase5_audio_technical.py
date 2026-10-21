#!/usr/bin/env python3
"""Fail-closed validator for Phase-5 offline audio technical reports."""

from __future__ import annotations

import argparse
import json
from pathlib import Path
import sys

sys.path.insert(0, str(Path(__file__).resolve().parent))
from measure_phase5_audio_technical import build_report  # noqa: E402


def validate_report(value: dict, recompute: bool = True) -> list[str]:
    errors = []
    if value.get("schema") != \
            "billiardgl.phase5.audio_technical_machine_measurement.v1" or \
            value.get("status") != "TECHNICAL_MACHINE_MEASUREMENT":
        errors.append("report identity/status invalid")
    boundary = value.get("evidence_boundary", {})
    if boundary.get("offline_machine_measurement_completed") is not True:
        errors.append("offline measurement completion missing")
    forbidden_true = (
        "audio_device_playback_completed", "human_listening_completed",
        "external_semantic_blind_test_completed", "authorship_or_rights_verified",
        "content_accepted", "product_integration_authorized",
        "beta_promotion_authorized",
    )
    if any(boundary.get(name) is not False for name in forbidden_true):
        errors.append("report promotes unperformed human/device/rights/content gate")
    bindings = value.get("bindings", {})
    if bindings.get("requested_manifest_path_missing") is not True or \
            bindings.get("resolved_manifest_sha256") != \
            "d213424dd81940a51bde8e886e640297afcb0648bf7c5d712670e405cd29a7cc" or \
            bindings.get("design_gate_sha256") != \
            "c9bdbe929afa868e94e991835f3bd52972d67d5ef9aedb80605c21ec489d1e00":
        errors.append("frozen gate/manifest binding invalid")
    assets = value.get("assets")
    if not isinstance(assets, list) or len(assets) != 10 or \
            len({row.get("semantic_id") for row in assets
                 if isinstance(row, dict)}) != 10:
        errors.append("technical asset inventory must be ten unique rows")
    else:
        for row in assets:
            if row.get("content_acceptance_status") != "NOT_CONTENT_ACCEPTED":
                errors.append(row.get("semantic_id", "UNKNOWN") +
                              ": content acceptance boundary invalid")
            checks = row.get("threshold_checks", [])
            actual_result = "PASS" if checks and all(
                item.get("result") == "PASS" for item in checks) else "FAIL"
            if row.get("technical_threshold_result") != actual_result:
                errors.append(row.get("semantic_id", "UNKNOWN") +
                              ": threshold summary inconsistent")
    summary = value.get("technical_summary", {})
    if summary.get("asset_count") != 10 or \
            summary.get("technical_threshold_pass_count", 0) + \
            summary.get("technical_threshold_fail_count", 0) != 10 or \
            summary.get("result") not in (
                "PASS_TECHNICAL_THRESHOLDS_ONLY", "FAIL_TECHNICAL_THRESHOLDS"):
        errors.append("technical summary invalid")
    if recompute:
        try:
            expected = build_report()
        except (OSError, ValueError, KeyError, json.JSONDecodeError) as exc:
            errors.append("recompute failed: " + str(exc))
        else:
            if value != expected:
                errors.append("report differs from deterministic WAV recomputation")
    return errors


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("report", type=Path)
    parser.add_argument("--no-recompute", action="store_true")
    args = parser.parse_args()
    try:
        value = json.loads(args.report.read_text(encoding="utf-8"))
    except (OSError, json.JSONDecodeError) as exc:
        print("Phase5 audio technical report FAIL: " + str(exc))
        return 1
    errors = validate_report(value, recompute=not args.no_recompute)
    if errors:
        for error in errors:
            print("FAIL: " + error)
        return 1
    print("Phase5 audio technical report PASS: machine-only content=NOT_ACCEPTED device=NOT_RUN")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
