#!/usr/bin/env python3
"""Read-only validator for Phase5 Rules UI v3 exact-28 readiness."""

from __future__ import annotations

import argparse
import hashlib
import json
from pathlib import Path
import sys
from typing import Any


ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT))

from scripts import prepare_phase5_rules_decision_ui_v3_targeted_ready as prepare
from scripts import run_phase5_rules_decision_ui_v3_visual_repair_machine as machine
from scripts import validate_phase5_rules_decision_ui_v3_visual_repair_machine as machine_validator


def sha(path: Path) -> str:
    return hashlib.sha256(path.read_bytes()).hexdigest()


def require(value: bool, message: str) -> None:
    if not value:
        raise RuntimeError(message)


def load(path: Path) -> dict[str, Any]:
    value = json.loads(path.read_text(encoding="utf-8"))
    require(isinstance(value, dict), str(path) + ": object required")
    return value


def validate(root: Path) -> dict[str, Any]:
    root = root.resolve(strict=True)
    ready_path = root / "rules-decision-ui-v3-targeted-ready.json"
    auth_path = root / "rules-decision-ui-v3-targeted-authorization-template.json"
    receipt_path = root / "offline-readiness-receipt.json"
    ready, auth, receipt = load(ready_path), load(auth_path), load(receipt_path)
    expected_ids = [row.split("\t", 1)[0] for row in prepare.plan_rows()]
    require(ready.get("schema") ==
            "phase5_rules_decision_ui_v3_targeted_gpu_ready_v1" and
            ready.get("status") ==
            "V3_TARGETED_28_READY_EXPLICIT_AUTHORIZATION_REQUIRED_NOT_RUN" and
            ready.get("case_count") == 28 and
            ready.get("repair_case_count") == 20 and
            ready.get("regression_control_count") == 8 and
            ready.get("case_ids_in_order") == expected_ids and
            ready.get("machine_matrix_passed") == 540 and
            ready.get("machine_matrix_byte_reproducible") is True and
            ready.get("cpu_preflight_passed") == 28,
            "v3 readiness identity/count")
    require(ready.get("case_results") ==
            [{"capture_id": case_id, "status": "NOT_RUN"}
             for case_id in expected_ids], "v3 exact 28 NOT_RUN rows")
    claims = {
        "actual_gpu_authorized": False, "actual_gpu_completed": 0,
        "golden_promotion": False, "m2_promotion": False,
        "product_exposed": False, "technical_art_review_completed": 0,
    }
    require(ready.get("claims") == claims, "v3 readiness claims")
    paths = {key: Path(value) for key, value in ready.get("paths", {}).items()}
    require(set(paths) == {"plan", "source_bindings", "runner_binary",
            "billiards_binary", "cpu_preflight_binary",
            "cpu_preflight_report", "machine_report", "font"} and
            all(path.is_file() for path in paths.values()),
            "v3 readiness paths")
    frozen = ready.get("frozen_bindings", {})
    checks = {
        "plan_sha256": paths["plan"],
        "source_bindings_sha256": paths["source_bindings"],
        "runner_binary_sha256": paths["runner_binary"],
        "billiards_binary_sha256": paths["billiards_binary"],
        "cpu_preflight_binary_sha256": paths["cpu_preflight_binary"],
        "cpu_preflight_report_sha256": paths["cpu_preflight_report"],
        "machine_report_sha256": paths["machine_report"],
        "font_sha256": paths["font"],
    }
    require(frozen.get("visual_repair_design_gate_sha256") == machine.GATE_SHA,
            "v3 gate binding")
    for field, path in checks.items():
        require(frozen.get(field) == sha(path), "v3 binding mismatch: " + field)
    require(paths["plan"].read_text(encoding="utf-8").splitlines() ==
            prepare.plan_rows(), "v3 plan bytes/rows")
    machine_validator.validate(load(paths["machine_report"]))
    cpu = load(paths["cpu_preflight_report"])
    require(cpu.get("schema") ==
            "phase5_rules_decision_ui_v3_targeted_cpu_frame_preflight_v1" and
            cpu.get("status") == "CPU_FRAME_PREFLIGHT_PASS_28_OF_28_NO_GPU" and
            cpu.get("case_count") == cpu.get("passed_count") == 28 and
            cpu.get("bad_input_count") == cpu.get("bad_input_passed_count") == 5 and
            all(cpu.get(field) is False for field in
                ("actual_gpu_invoked", "ns_application_invoked",
                 "opengl_invoked", "gpu_output_created")),
            "v3 CPU preflight boundary")
    bindings = load(paths["source_bindings"])
    require(bindings.get("schema") ==
            "phase5_rules_decision_ui_v3_source_bindings_v1" and
            bindings.get("visual_repair_design_gate_sha256") == machine.GATE_SHA and
            bindings.get("aggregate_sha256") ==
                frozen.get("source_aggregate_sha256"),
            "v3 source binding manifest")
    for row in bindings.get("files", []):
        path = ROOT / row.get("path", "")
        require(path.is_file() and sha(path) == row.get("sha256"),
                "v3 source binding drift: " + row.get("path", ""))
    require(auth.get("schema") ==
            "phase5_rules_decision_ui_v3_targeted_gpu_authorization_v1" and
            auth.get("authorized") is False and auth.get("consumed") is False and
            auth.get("targeted_visual_repair_gpu_authorized") is False and
            auth.get("case_count") == 28 and
            auth.get("scene_ids_in_order") == expected_ids and
            auth.get("ready_manifest_sha256") == sha(ready_path) and
            auth.get("product_exposed") is False and
            auth.get("golden_promotion") is False and
            auth.get("m2_promotion") is False,
            "v3 unsigned authorization template")
    for field, digest in frozen.items():
        require(auth.get(field) == digest,
                "v3 authorization template frozen binding: " + field)
    require(receipt == {
        "schema": "phase5_rules_decision_ui_v3_targeted_offline_readiness_v1",
        "status": "CPU_28_AND_MACHINE_540_READY_GPU_NOT_AUTHORIZED_NOT_RUN",
        "ready_manifest_sha256": sha(ready_path),
        "authorization_template_sha256": sha(auth_path),
        "claims": claims,
    }, "v3 offline readiness receipt")
    return {
        "status": "PASS_PHASE5_RULES_UI_V3_TARGETED_READY_NO_GPU",
        "ready_manifest_sha256": sha(ready_path),
        "case_count": 28, "repair_case_count": 20,
        "regression_control_count": 8,
        "gpu_authorized": False, "gpu_executed": False,
        "product_exposed": False,
    }


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("root", type=Path)
    args = parser.parse_args()
    try:
        print(json.dumps(validate(args.root), sort_keys=True))
    except (OSError, ValueError, json.JSONDecodeError, RuntimeError) as error:
        print("FAIL: " + str(error))
        return 2
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
