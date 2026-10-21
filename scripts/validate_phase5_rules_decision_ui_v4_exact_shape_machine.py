#!/usr/bin/env python3
"""Validate the no-GPU Phase5 Rules UI v4 exact-shape report."""

from __future__ import annotations

import argparse
import json
from pathlib import Path
import sys
from typing import Any

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT))

from scripts import run_phase5_rules_decision_ui_v4_exact_shape_machine as runner


def require(condition: bool, message: str) -> None:
    if not condition:
        raise RuntimeError(message)


def validate(value: Any) -> dict[str, Any]:
    require(isinstance(value, dict), "report object required")
    require(value.get("schema") ==
            "phase5_rules_decision_ui_v4_exact_shape_machine_v1" and
            value.get("schema_version") == 1 and
            value.get("status") ==
            "PASS_540_OF_540_REPRODUCIBLE_NO_GPU_NO_AUTH" and
            value.get("v4_exact_shape_gate_sha256") == runner.GATE_SHA and
            runner.sha(runner.GATE) == runner.GATE_SHA,
            "v4 report identity/gate mismatch")
    gate = json.loads(runner.GATE.read_text(encoding="utf-8"))
    expected_read_only = {item["path"]: item["sha256"]
                          for item in gate["read_only_bindings"]}
    require(value.get("v4_read_only_bindings") == expected_read_only,
            "v4 frozen binding inventory mismatch")
    for relative, digest in expected_read_only.items():
        require(runner.sha(runner.ROOT / relative) == digest,
                "v4 frozen binding drift: " + relative)
    require(value.get("v4_forbidden_authority_bindings") ==
            runner.v3.FORBIDDEN_BINDINGS,
            "v4 forbidden authority inventory mismatch")
    for relative, digest in runner.v3.FORBIDDEN_BINDINGS.items():
        require(runner.sha(runner.ROOT / relative) == digest,
                "forbidden authority/rules drift: " + relative)
    bindings = value.get("v4_implementation_bindings")
    require(isinstance(bindings, dict) and
            set(bindings) == set(runner.IMPLEMENTATION_FILES),
            "v4 implementation binding inventory mismatch")
    for relative, digest in bindings.items():
        require(runner.sha(runner.ROOT / relative) == digest,
                "v4 implementation source drift: " + relative)
    require(value.get("v4_tokens") == runner.TOKENS and
            len(set(runner.TOKENS.values())) == 3 and
            value.get("copy_freeze") == runner.COPY_FREEZE,
            "v4 token identity/copy freeze mismatch")
    require(value.get("v4_totals") == {
        "machine_required": 540, "machine_passed": 540,
        "armed_shape_cases": 36, "non_armed_control_cases": 504,
        "targeted_gpu_required": 16, "targeted_gpu_completed": 0,
    }, "v4 totals mismatch")
    boundary = value.get("v4_evidence_boundary")
    require(isinstance(boundary, dict) and not any(boundary.values()),
            "v4 evidence boundary must remain false")
    cases = value.get("cases")
    require(isinstance(cases, list) and len(cases) == 540 and
            all(case.get("passed") is True for case in cases),
            "v4 exact 540 PASS cases required")
    affected = [case for case in cases if case.get("v4_exact_shape_affected")]
    controls = [case for case in cases if not case.get("v4_exact_shape_affected")]
    require(len(affected) == 36 and len(controls) == 504 and
            all(case.get("state") == runner.ARMED_STATE for case in affected),
            "v4 affected/control partition mismatch")
    for case in affected:
        contract = case.get("v4_exact_shape_contract")
        boxes = contract.get("physical_bounding_boxes_px", {})
        strokes = contract.get("physical_strokes_px", {})
        require(contract.get("token_ids") == runner.TOKENS and
                set(contract.get("semantic_roles", {})) == set(runner.TOKENS) and
                all(width >= 18.0 and height >= 18.0
                    for width, height in boxes.values()) and
                all(value >= 2.0 for value in strokes.values()) and
                contract.get("selected_closed_diamond") is True and
                contract.get("selected_check_enclosed") is True and
                contract.get("selected_check_to_outline_clearance_px_min") >= 2.0 and
                contract.get("confirm_closed_rectangular_frame_count") == 2 and
                contract.get("confirm_frame_separation_px_min") >= 2.0 and
                contract.get("confirm_bent_enter_arrow") is True and
                contract.get("focus_joined_diagonal_stroke_count") == 2 and
                contract.get("token_aliasing") is False and
                contract.get("standard_cvd_geometry_equal") is True and
                case.get("v4_token_leakage") is False,
                "v4 affected shape contract mismatch")
    require(all(case.get("v4_exact_shape_contract") is None and
                case.get("v4_token_leakage") is False for case in controls),
            "v4 token leaked into control state")
    return {"status": "PASS_PHASE5_RULES_UI_V4_EXACT_SHAPE_540_OF_540",
            "case_count": 540, "affected_shape_cases": 36,
            "targeted_gpu_authorized": False, "targeted_gpu_completed": 0}


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("report", type=Path)
    arguments = parser.parse_args()
    try:
        print(json.dumps(validate(json.loads(arguments.report.read_text(
            encoding="utf-8"))), sort_keys=True))
    except (OSError, ValueError, json.JSONDecodeError, RuntimeError) as error:
        print("FAIL: " + str(error))
        return 2
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
