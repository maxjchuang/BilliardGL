#!/usr/bin/env python3
"""Generate the NOT_RUN combined 27-case official-rack TA review template."""

from __future__ import annotations

import argparse
import hashlib
import json
from pathlib import Path
import sys

ROOT = Path(__file__).resolve().parents[1]
GATE = ROOT / "docs/art/wpa-official-fixed-seed-rack-visual-rebaseline-design-gate-v1.json"
GATE_SHA = "5c8460672bcf10d3abac12649cbf427102d764d0aacf60f0ccd9371d62a270c9"
SOURCE_TEMPLATES = {
    "phase2_full_six": (
        ROOT / "docs/art/phase2-v10i-formal-full-six-ta-review-template-v1.json",
        "6adc89b2b9030426287f432edfa3c33a6431af545cbaedae80a446c2b9e81f49",
    ),
    "phase4_targeted5": (
        ROOT / "docs/art/phase4-v12-targeted5-independent-ta-review-template-v1.json",
        "e4ef1fbfb92536d6a02b6c3287b4a453f84e7e21bfed8c594414401d937e98df",
    ),
    "phase5_exact16": (
        ROOT / "docs/art/phase5-rules-decision-ui-v5-exact16-independent-ta-review-template-v1.json",
        "917ef40bdf96ccca631117363c2d7d92eadade3bc9629d3d7559067e53164e2a",
    ),
}


def sha(path: Path) -> str:
    return hashlib.sha256(path.read_bytes()).hexdigest()


def pending() -> dict:
    return {"status": "PENDING", "observation": None,
            "physical_coordinates": []}


def unavailable_case_evidence() -> dict:
    return {
        "ppm_path": None,
        "ppm_sha256": None,
        "ppm_physical_dimensions": None,
        "png_path": None,
        "png_sha256": None,
        "png_physical_dimensions": None,
    }


def unavailable_batch_evidence(required: int) -> dict:
    return {
        "readiness_path": None,
        "readiness_sha256": None,
        "machine_receipt_path": None,
        "machine_receipt_sha256": None,
        "required_machine_pass_count": required,
        "machine_pass_count": None,
        "machine_failure_count": None,
        "machine_not_run_count": None,
        "stable_png_manifest_path": None,
        "stable_png_manifest_sha256": None,
        "ppm_aggregate_sha256": None,
        "png_aggregate_sha256": None,
        "case_evidence_aggregate_sha256": None,
    }


def build_template() -> dict:
    if sha(GATE) != GATE_SHA:
        raise RuntimeError("official-rack visual rebaseline gate SHA drift")
    gate = json.loads(GATE.read_text(encoding="utf-8"))
    sources: dict[str, dict] = {}
    for batch, (path, digest) in SOURCE_TEMPLATES.items():
        if sha(path) != digest:
            raise RuntimeError(f"source TA template SHA drift: {batch}")
        sources[batch] = json.loads(path.read_text(encoding="utf-8"))

    inventory = gate["affected_case_inventory"]
    rack_axes = gate["original_resolution_ta_contract"][
        "canonical_rack_axes_in_order"]
    isolation_axes = gate["original_resolution_ta_contract"]["phase4"][
        "additional_isolation_axes"]
    phase_axes = {
        "phase2_full_six": sources["phase2_full_six"]["review_scope"]
            ["canonical_axes_in_order"],
        "phase4_targeted5": sources["phase4_targeted5"]["review_scope"]
            ["canonical_axes_in_order"],
        "phase5_exact16": sources["phase5_exact16"]["review_scope"]
            ["canonical_axes_in_order"],
    }
    case_ids = {
        "phase2_full_six": inventory["phase2_full_six"]["case_ids_in_order"],
        "phase4_targeted5": inventory["phase4_targeted5"]["case_ids_in_order"],
        "phase5_exact16": inventory["phase5_exact16"]["case_ids_in_order"],
    }
    phase5_dimensions = {
        row["case_id"]: row["expected_physical_dimensions"]
        for row in sources["phase5_exact16"]["cases"]
    }
    cases: list[dict] = []
    ordinal = 0
    for batch in ("phase2_full_six", "phase4_targeted5", "phase5_exact16"):
        for batch_ordinal, case_id in enumerate(case_ids[batch], 1):
            ordinal += 1
            applicable_axes = list(phase_axes[batch])
            row: dict = {
                "ordinal": ordinal,
                "batch_id": batch,
                "batch_ordinal": batch_ordinal,
                "case_id": case_id,
                "expected_physical_dimensions":
                    phase5_dimensions[case_id] if batch == "phase5_exact16"
                    else [1920, 1080],
                "prior_canonical_axes_in_order": list(phase_axes[batch]),
                "applicable_rack_axes_in_order": [],
                "not_applicable_rack_axes_in_order": [],
                "isolation_axes_in_order": [],
                "applicable_axis_findings": {},
                "not_applicable_axis_findings": {},
                "future_evidence": unavailable_case_evidence(),
                "manual_case_result": "NOT_RUN",
                "blocking_axes": [],
            }
            if batch == "phase4_targeted5":
                row["not_applicable_rack_axes_in_order"] = list(rack_axes)
                row["isolation_axes_in_order"] = list(isolation_axes)
                applicable_axes.extend(isolation_axes)
                row["not_applicable_axis_findings"] = {
                    axis: {
                        "status": "NOT_APPLICABLE",
                        "reason": "The bound launch_stop_shot_center_v1 fixture contains only balls 0 and 1 and must not fabricate product-rack evidence.",
                    }
                    for axis in rack_axes
                }
            else:
                row["applicable_rack_axes_in_order"] = list(rack_axes)
                applicable_axes.extend(rack_axes)
            row["applicable_axis_findings"] = {
                axis: pending() for axis in applicable_axes
            }
            cases.append(row)

    return {
        "schema": "wpa_official_fixed_seed_rack_combined27_independent_ta_review_v1",
        "schema_version": 1,
        "status": "TEMPLATE_NOT_RUN_WAITING_FOR_FRESH_SHARED_BUILD_EVIDENCE",
        "overall_result": "NOT_RUN",
        "review_performed": False,
        "frozen_bindings": {
            "official_rack_visual_rebaseline_gate": {
                "path": str(GATE.relative_to(ROOT)), "sha256": GATE_SHA},
            **{
                batch + "_source_template": {
                    "path": str(path.relative_to(ROOT)), "sha256": digest}
                for batch, (path, digest) in SOURCE_TEMPLATES.items()
            },
        },
        "scope": {
            "batch_order": ["phase2_full_six", "phase4_targeted5",
                            "phase5_exact16"],
            "batch_case_counts": {"phase2_full_six": 6,
                                  "phase4_targeted5": 5,
                                  "phase5_exact16": 16},
            "case_count": 27,
            "case_ids_in_exact_order": [row["case_id"] for row in cases],
            "rack_axes_in_order": list(rack_axes),
            "phase4_isolation_axes_in_order": list(isolation_axes),
            "applicable_pending_axis_count": 406,
            "phase4_not_applicable_rack_axis_count": 35,
            "total_axis_record_count": 441,
            "original_resolution_review_required": True,
        },
        "batch_future_evidence": {
            "phase2_full_six": unavailable_batch_evidence(6),
            "phase4_targeted5": unavailable_batch_evidence(5),
            "phase5_exact16": unavailable_batch_evidence(16),
        },
        "combined_future_evidence": {
            "fresh_shared_build_manifest_path": None,
            "fresh_shared_build_manifest_sha256": None,
            "fresh_shared_binary_sha256": None,
            "ordered_27_ppm_aggregate_sha256": None,
            "ordered_27_png_aggregate_sha256": None,
            "ordered_27_case_evidence_aggregate_sha256": None,
        },
        "aggregate_contract": {
            "algorithm": "sha256_canonical_json_utf8_v1",
            "canonical_json": {
                "sort_keys": True,
                "separators": [",", ":"],
                "ensure_ascii": False,
            },
            "batch_ppm_rows_in_case_order": [
                "case_id", "ppm_path", "ppm_sha256",
                "ppm_physical_dimensions"],
            "batch_png_rows_in_case_order": [
                "case_id", "png_path", "png_sha256",
                "png_physical_dimensions"],
            "batch_case_evidence_rows_in_case_order": [
                "case_id", "ppm_path", "ppm_sha256",
                "ppm_physical_dimensions", "png_path", "png_sha256",
                "png_physical_dimensions"],
            "combined_rows_are_the_exact_27_case_order": True,
        },
        "completion_preconditions": [
            "A fresh shared build manifest binds the official-rack gate production SHAs and one fresh binary SHA.",
            "Phase2, Phase4, and Phase5 readiness and machine receipts are independently frozen; machine results are exactly 6/6, 5/5, and 16/16 PASS with zero failure or NOT_RUN.",
            "Every ordered case binds repository-relative PPM and PNG path, SHA-256, and independently decoded physical dimensions equal to expected_physical_dimensions.",
            "Each batch stable PNG manifest and ordered PPM, PNG, and case-evidence aggregate is present; combined ordered-27 aggregates are present and recomputable.",
            "Only after every evidence precondition passes may all 406 applicable axes atomically change from PENDING to PASS or FAIL, all 35 Phase4 rack axes remain NOT_APPLICABLE, and all 27 case results change from NOT_RUN to PASS or FAIL.",
            "Overall PASS requires every applicable axis and every case PASS. Any applicable FAIL produces overall FAIL and an exact blocking_axes list; no partial or inherited review is accepted."
        ],
        "allowed_status_transitions": {
            "template_applicable_axis": ["PENDING"],
            "completed_applicable_axis": ["PASS", "FAIL"],
            "phase4_rack_axis": ["NOT_APPLICABLE"],
            "template_case": ["NOT_RUN"],
            "completed_case": ["PASS", "FAIL"],
            "template_overall": ["NOT_RUN"],
            "completed_overall": ["PASS", "FAIL"],
        },
        "cases": cases,
        "evidence_boundary": {
            "production_modified": False,
            "gpu_executed": False,
            "readiness_created": False,
            "authorization_created": False,
            "machine_acceptance": False,
            "review_performed": False,
            "ta_acceptance": False,
            "m2": False,
            "m3": False,
            "golden": False,
            "beta": False,
            "product_acceptance": False,
        },
    }


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--output", type=Path)
    parser.add_argument("--stdout", action="store_true")
    args = parser.parse_args()
    value = build_template()
    payload = json.dumps(value, ensure_ascii=False, indent=2,
                         sort_keys=True) + "\n"
    if args.stdout:
        sys.stdout.write(payload)
    elif args.output:
        args.output.write_text(payload, encoding="utf-8")
    else:
        parser.error("one of --output or --stdout is required")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
