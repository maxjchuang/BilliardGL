#!/usr/bin/env python3
"""Fail-closed validator for Phase-5 v2 technical-correction candidates."""

from __future__ import annotations

import argparse
import json
from pathlib import Path
import sys

sys.path.insert(0, str(Path(__file__).resolve().parent))
import generate_phase5_audio_v2_technical_correction as generator  # noqa: E402


FORBIDDEN_TRUE = (
    "audio_playback_completed", "actual_device_playback_completed",
    "human_listening_completed", "semantic_blind_test_completed",
    "authorship_or_rights_verified", "content_accepted",
    "product_integration_authorized", "beta_promotion_authorized",
)


def structural_errors(manifest: dict, report: dict) -> list[str]:
    errors = []
    if manifest.get("schema") != \
            "billiardgl.phase5.audio_v2_technical_correction_candidate.v1" or \
            manifest.get("status") != \
            "MACHINE_TECHNICAL_CANDIDATE_NOT_CONTENT_ACCEPTED":
        errors.append("candidate identity/status invalid")
    if report.get("schema") != \
            "billiardgl.phase5.audio_v2_technical_correction_machine_measurement.v1" or \
            report.get("status") != "TECHNICAL_MACHINE_MEASUREMENT":
        errors.append("machine report identity/status invalid")
    expected_bindings = {
        "technical_correction_design_gate_sha256": generator.GATE_SHA256,
        "v1_design_gate_sha256": generator.V1_DESIGN_GATE_SHA256,
        "v1_candidate_manifest_sha256": generator.V1_MANIFEST_SHA256,
        "v1_machine_report_sha256": generator.V1_REPORT_SHA256,
    }
    for name, value in expected_bindings.items():
        if manifest.get("bindings", {}).get(name) != value or \
                report.get("bindings", {}).get(name) != value:
            errors.append("immutable binding differs: " + name)
    for document_name, document in (("candidate", manifest),
                                    ("report", report)):
        boundary = document.get("evidence_boundary", {})
        if boundary.get("offline_machine_measurement_completed") is not True or \
                any(boundary.get(name) is not False for name in FORBIDDEN_TRUE):
            errors.append(document_name +
                          " promotes unperformed human/device/rights/content gate")
    assets = manifest.get("assets")
    if not isinstance(assets, list) or len(assets) != 10 or \
            len({row.get("semantic_id") for row in assets
                 if isinstance(row, dict)}) != 10:
        errors.append("asset inventory must be ten unique rows")
    elif sum(row.get("transformation", {}).get("operation") ==
             "byte_identical_copy" for row in assets) != 8 or \
            sum(row.get("transformation", {}).get("operation") ==
                "constant_linear_gain_full_file_all_channels"
                for row in assets) != 2:
        errors.append("transform inventory must be eight copies and two gains")
    summary = manifest.get("technical_summary", {})
    if summary != {"pass_count": 10, "fail_count": 0,
                   "failed_asset_ids": [],
                   "result": "PASS_TECHNICAL_THRESHOLDS_ONLY"} or \
            report.get("technical_summary") != summary:
        errors.append("technical summary must be machine-only 10/10")
    if report.get("one_shot_byte_identical_count") != 8 or \
            report.get("audio_device_open_count") != 0:
        errors.append("report copy/device boundary invalid")
    return errors


def validate_package(output_root: Path, report_path: Path) -> list[str]:
    errors = []
    try:
        generator.verify_bindings()
        manifest_path = output_root / generator.CANDIDATE_MANIFEST_NAME
        manifest = json.loads(manifest_path.read_text(encoding="utf-8"))
        report = json.loads(report_path.read_text(encoding="utf-8"))
    except (OSError, ValueError, KeyError, json.JSONDecodeError) as exc:
        return ["package read/binding failed: " + str(exc)]
    errors += structural_errors(manifest, report)
    expected_names = {row["filename"] for row in manifest.get("assets", [])}
    expected_names.add(generator.CANDIDATE_MANIFEST_NAME)
    actual_names = {path.name for path in output_root.iterdir() if path.is_file()}
    if actual_names != expected_names:
        errors.append("v2 asset root file inventory differs")
    try:
        expected_manifest = generator.build_manifest(output_root)
    except (OSError, ValueError, KeyError, json.JSONDecodeError) as exc:
        errors.append("WAV/sample recomputation failed: " + str(exc))
        return errors
    if manifest != expected_manifest:
        errors.append("candidate manifest differs from deterministic recomputation")
    expected_report = generator.build_report(expected_manifest,
        generator.sha256(output_root / generator.CANDIDATE_MANIFEST_NAME))
    if report != expected_report:
        errors.append("machine report differs from deterministic recomputation")
    if report.get("candidate_manifest_sha256") != generator.sha256(
            output_root / generator.CANDIDATE_MANIFEST_NAME):
        errors.append("machine report candidate-manifest SHA differs")
    return errors


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--asset-root", type=Path,
                        default=generator.DEFAULT_OUTPUT_ROOT)
    parser.add_argument("--report", type=Path, default=generator.DEFAULT_REPORT)
    args = parser.parse_args()
    errors = validate_package(args.asset_root.resolve(), args.report.resolve())
    if errors:
        for error in errors:
            print("FAIL: " + error)
        return 1
    print("Phase5 audio v2 technical correction PASS: machine-only "
          "content=NOT_ACCEPTED device=NOT_RUN")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
