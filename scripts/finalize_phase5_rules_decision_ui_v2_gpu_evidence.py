#!/usr/bin/env python3
"""Losslessly finalize frozen Rules UI v2 PPM captures into PNG evidence."""

from __future__ import annotations

import argparse
import hashlib
import json
from pathlib import Path

from run_phase5_rules_decision_ui_gpu_capture import convert_png
from validate_phase5_rules_decision_ui_v2_gpu_evidence import (
    DEFAULT_RECEIPT,
    ROOT,
    load_inputs,
    validate,
)


DEFAULT_OUTPUT = ROOT / "docs/evidence/phase5/actual-gpu-v2-evidence"


def sha(path: Path) -> str:
    return hashlib.sha256(path.read_bytes()).hexdigest()


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--receipt", type=Path, default=DEFAULT_RECEIPT)
    parser.add_argument("--output-root", type=Path, default=DEFAULT_OUTPUT)
    args = parser.parse_args()
    receipt_path = args.receipt.resolve()
    output = args.output_root.resolve()
    if output.exists():
        raise RuntimeError("final PNG evidence root must not already exist")
    validate(receipt_path)
    receipt, plan, rows = load_inputs(receipt_path)
    png_root = output / "png"
    png_root.mkdir(parents=True)
    cases = []
    for selector, row in zip(plan, rows):
        source = ROOT / row["ppm_path"]
        destination = png_root / f"{row['capture_id']}.png"
        width, height = convert_png(source, destination)
        if (width, height) != (int(selector[7]), int(selector[8])):
            raise RuntimeError(f"converted PNG dimensions drift: {row['capture_id']}")
        cases.append({
            "capture_id": row["capture_id"],
            "state": selector[1],
            "locale": selector[2],
            "palette": selector[3],
            "profile_id": selector[4],
            "logical_dimensions": [int(selector[5]), int(selector[6])],
            "physical_dimensions": [width, height],
            "density": float(selector[9]),
            "ui_scale": float(selector[10]),
            "ppm_path": row["ppm_path"],
            "ppm_sha256": row["ppm_sha256"],
            "png_path": str(destination.relative_to(ROOT)),
            "png_sha256": sha(destination),
            "canonical_sha256": row["canonical_sha256"],
            "gpu_canonical_sha256": row["gpu_canonical_sha256"],
            "gpu_draw_calls": row["gpu_draw_calls"],
            "rules_ui_after_tone_map": row["rules_ui_after_tone_map"],
            "render_passes": row["render_passes"],
            "actual_gpu_capture": row["actual_gpu_capture"],
            "hit_rectangles": row["hit_rectangles"],
            "gl_vendor": row["gl_vendor"],
            "gl_device": row["gl_device"],
            "gl_version": row["gl_version"],
        })
    png_aggregate = hashlib.sha256()
    for case in cases:
        png_aggregate.update(case["png_path"].encode("utf-8"))
        png_aggregate.update(b"\0")
        png_aggregate.update(case["png_sha256"].encode("ascii"))
        png_aggregate.update(b"\n")
    authorization = receipt["authorization"]
    frozen = receipt["frozen_bindings"]
    evidence = {
        "schema": "phase5_rules_decision_ui_actual_gpu_evidence_v2",
        "status": "ACTUAL_GPU_MACHINE_PASS_60_OF_60_MANUAL_REVIEW_PENDING",
        "case_count": 60,
        "cases": cases,
        "frozen_bindings": {
            "machine_pass_receipt_path": str(receipt_path.relative_to(ROOT)),
            "machine_pass_receipt_sha256": sha(receipt_path),
            "authorization_path": authorization["authorization_path"],
            "authorization_sha256": authorization["authorization_sha256"],
            "consumed_marker_path": authorization["consumed_marker_path"],
            "consumed_marker_sha256": authorization["consumed_marker_sha256"],
            "ready_manifest_sha256": frozen["ready_manifest_sha256"],
            "gpu_plan_sha256": frozen["gpu_plan_sha256"],
            "gpu_results_jsonl_sha256": frozen["gpu_results_jsonl_sha256"],
            "source_bindings_sha256": frozen["source_bindings_sha256"],
            "source_aggregate_sha256": frozen["source_aggregate_sha256"],
            "runner_source_sha256": frozen["runner_source_sha256"],
            "runner_binary_sha256": frozen["runner_binary_sha256"],
            "billiards_binary_sha256": frozen["billiards_binary_sha256"],
            "ppm_aggregate_sha256": frozen["ppm_aggregate_sha256"],
            "png_aggregate_algorithm":
                "sha256(png_path_utf8 + NUL + png_sha256_ascii + LF), plan order",
            "png_aggregate_sha256": png_aggregate.hexdigest(),
        },
        "claims": {
            "actual_gpu_capture_completed": 60,
            "machine_validation_completed": 60,
            "manual_review_completed": 0,
            "product_exposed": False,
            "true_input_session_completed": False,
            "screen_reader_session_completed": False,
            "external_beta_qa_completed": 0,
            "profile_complete": False,
        },
        "conversion": {
            "source_format": "P6 PPM RGB8",
            "destination_format": "PNG RGB8 lossless",
            "pixel_transform": "none",
            "gpu_rerun": False,
        },
    }
    manifest = output / "rules-decision-ui-actual-gpu-evidence-v2.json"
    manifest.write_text(
        json.dumps(evidence, ensure_ascii=False, indent=2, sort_keys=True) + "\n",
        encoding="utf-8")
    print(f"Phase5 Rules UI v2 PNG evidence finalized: {manifest}")
    print(f"PNG aggregate: {png_aggregate.hexdigest()}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
