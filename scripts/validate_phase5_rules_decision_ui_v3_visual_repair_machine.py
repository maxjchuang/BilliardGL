#!/usr/bin/env python3
"""Validate the no-GPU Phase5 Rules UI v3 540-case repair report."""

from __future__ import annotations

import argparse
import hashlib
import json
from pathlib import Path
import sys
from typing import Any

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT))

from scripts import run_phase5_rules_decision_ui_v3_visual_repair_machine as runner
EXPECTED_BOUNDARY = {
    "actual_gpu_authorized": False,
    "actual_gpu_claimed": False,
    "full_v2_evidence_overwritten": False,
    "golden_promotion": False,
    "m2_promotion": False,
    "product_exposed": False,
    "technical_art_review_claimed": False,
}


def sha(path: Path) -> str:
    return hashlib.sha256(path.read_bytes()).hexdigest()


def require(value: bool, message: str) -> None:
    if not value:
        raise RuntimeError(message)


def validate(value: Any) -> dict[str, Any]:
    require(isinstance(value, dict), "report root must be object")
    require(value.get("schema") ==
            "phase5_rules_decision_ui_v3_visual_repair_machine_evidence_v1" and
            value.get("schema_version") == 1 and
            value.get("visual_repair_design_gate_sha256") == runner.GATE_SHA and
            value.get("status") ==
            "V3_VISUAL_REPAIR_MACHINE_PASS_540_OF_540_GPU_NOT_AUTHORIZED_NOT_RUN",
            "v3 machine identity")
    require(sha(runner.GATE) == runner.GATE_SHA, "v3 gate bytes changed")
    gate = json.loads(runner.GATE.read_text(encoding="utf-8"))
    expected_evidence = {row["path"]: row["sha256"]
                         for row in gate["read_only_bindings"]}
    require(value.get("v3_read_only_evidence_bindings") == expected_evidence,
            "v3 frozen evidence binding inventory")
    for relative, digest in expected_evidence.items():
        require(sha(ROOT / relative) == digest,
                "v3 frozen evidence bytes changed: " + relative)
    require(value.get("v3_forbidden_authority_bindings") ==
            runner.FORBIDDEN_BINDINGS,
            "forbidden authority binding inventory")
    for relative, digest in runner.FORBIDDEN_BINDINGS.items():
        require(sha(ROOT / relative) == digest,
                "forbidden authority/rules source changed: " + relative)
    implementation = value.get("v3_implementation_bindings")
    require(isinstance(implementation, dict) and
            set(implementation) == set(runner.IMPLEMENTATION_FILES),
            "v3 implementation binding inventory")
    for relative, digest in implementation.items():
        require(isinstance(digest, str) and len(digest) == 64 and
                sha(ROOT / relative) == digest,
                "v3 implementation source drift: " + relative)
    require(value.get("v3_totals") == {
        "machine_required": 540,
        "machine_passed": 540,
        "repair_state_cases": 180,
        "targeted_gpu_required": 28,
        "targeted_gpu_completed": 0,
        "technical_art_review_completed": 0,
    }, "v3 machine/targeted totals")
    require(value.get("v3_evidence_boundary") == EXPECTED_BOUNDARY,
            "v3 no-GPU/no-product boundary")
    require(value.get("v3_visual_repair_checks") ==
            gate["machine_validation_gate"]["per_case_hard_checks"],
            "v3 hard-check inventory")
    cases = value.get("cases")
    require(isinstance(cases, list) and len(cases) == 540,
            "v3 exact 540-case inventory")
    require(all(case.get("passed") is True and
                case.get("visual_repair_contract_passed") is True
                for case in cases), "v3 case failure")
    affected = [case for case in cases
                if case.get("visual_repair_affected_state") is True]
    require(len(affected) == 180 and
            {case.get("state") for case in affected} == runner.REPAIR_STATES,
            "v3 repair-state cross-product coverage")
    return {
        "status": "PASS_PHASE5_RULES_UI_V3_MACHINE_540_OF_540",
        "case_count": 540,
        "repair_state_case_count": 180,
        "targeted_gpu_authorized": False,
        "targeted_gpu_completed": 0,
        "product_exposed": False,
    }


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("report", type=Path)
    args = parser.parse_args()
    try:
        value = json.loads(args.report.read_text(encoding="utf-8"))
        print(json.dumps(validate(value), sort_keys=True))
    except (OSError, ValueError, json.JSONDecodeError, RuntimeError) as error:
        print("FAIL: " + str(error))
        return 2
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
