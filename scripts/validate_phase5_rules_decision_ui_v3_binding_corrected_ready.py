#!/usr/bin/env python3
"""Read-only validator for corrected Phase5 Rules UI v3 exact-28 readiness."""

from __future__ import annotations

import argparse
import hashlib
import json
from pathlib import Path
import sys
from typing import Any


ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT))

from scripts import (
    prepare_phase5_rules_decision_ui_v3_binding_corrected_ready as prepare,
)
from scripts import (
    run_phase5_rules_decision_ui_v3_binding_corrected_targeted_gpu_capture
    as capture,
)
from scripts import (
    validate_phase5_rules_decision_ui_v3_visual_repair_machine
    as machine_validator,
)


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
    ready_path = (
        root / "rules-decision-ui-v3-binding-corrected-targeted-ready.json")
    auth_path = (
        root /
        "rules-decision-ui-v3-binding-corrected-authorization-template.json")
    receipt_path = root / "offline-readiness-receipt.json"
    ready = load(ready_path)
    auth = load(auth_path)
    receipt = load(receipt_path)
    expected_ids = [row.split("\t", 1)[0] for row in prepare.plan_rows()]
    claims = {
        "actual_gpu_authorized": False,
        "actual_gpu_completed": 0,
        "golden_promotion": False,
        "m2_promotion": False,
        "product_exposed": False,
        "technical_art_review_completed": 0,
    }
    require(
        ready.get("schema") ==
            "phase5_rules_decision_ui_v3_binding_corrected_targeted_gpu_ready_v1" and
        ready.get("status") == capture.READY_STATUS and
        ready.get("case_count") == 28 and
        ready.get("repair_case_count") == 20 and
        ready.get("regression_control_count") == 8 and
        ready.get("case_ids_in_order") == expected_ids and
        ready.get("machine_matrix_passed") == 540 and
        ready.get("machine_matrix_byte_reproducible") is True and
        ready.get("cpu_preflight_passed") == 28 and
        ready.get("claims") == claims,
        "corrected readiness identity/count/boundary")
    require(
        ready.get("case_results") == [
            {"capture_id": case_id, "status": "NOT_RUN"}
            for case_id in expected_ids
        ], "corrected exact 28 NOT_RUN rows")

    expected_path_keys = {
        "plan", "source_binding_manifest", "runner_binary",
        "billiards_binary", "cpu_preflight_binary", "cpu_preflight_report",
        "machine_report", "font", "hud_token", "shell_token",
    }
    paths = {key: Path(value)
             for key, value in ready.get("paths", {}).items()}
    require(set(paths) == expected_path_keys and
            all(path.is_file() for path in paths.values()),
            "corrected readiness paths")
    frozen = ready.get("frozen_bindings", {})
    checks = {
        "plan_sha256": "plan",
        "source_binding_manifest_sha256": "source_binding_manifest",
        "runner_binary_sha256": "runner_binary",
        "billiards_binary_sha256": "billiards_binary",
        "cpu_preflight_binary_sha256": "cpu_preflight_binary",
        "cpu_preflight_report_sha256": "cpu_preflight_report",
        "machine_report_sha256": "machine_report",
        "font_sha256": "font",
        "hud_token_sha256": "hud_token",
        "shell_token_sha256": "shell_token",
    }
    for field, key in checks.items():
        require(frozen.get(field) == sha(paths[key]),
                "corrected binding mismatch: " + field)
    require(
        frozen.get("visual_repair_design_gate_sha256") == prepare.machine.GATE_SHA and
        frozen.get("correction_gate_sha256") == prepare.CORRECTION_GATE_SHA,
        "corrected design gate bindings")
    require(paths["plan"].read_text(encoding="utf-8").splitlines() ==
            prepare.plan_rows(), "corrected plan bytes/rows")

    machine_validator.validate(load(paths["machine_report"]))
    cpu = load(paths["cpu_preflight_report"])
    require(
        cpu.get("schema") ==
            "phase5_rules_decision_ui_v3_targeted_cpu_frame_preflight_v1" and
        cpu.get("status") == "CPU_FRAME_PREFLIGHT_PASS_28_OF_28_NO_GPU" and
        cpu.get("case_count") == cpu.get("passed_count") == 28 and
        cpu.get("bad_input_count") == cpu.get("bad_input_passed_count") == 5 and
        all(cpu.get(field) is False for field in (
            "actual_gpu_invoked", "ns_application_invoked",
            "opengl_invoked", "gpu_output_created")),
        "corrected CPU preflight boundary")

    bindings = load(paths["source_binding_manifest"])
    require(
        bindings.get("schema") ==
            "phase5_rules_decision_ui_v3_binding_corrected_source_bindings_v1" and
        bindings.get("visual_repair_design_gate_sha256") ==
            prepare.machine.GATE_SHA and
        bindings.get("correction_gate_sha256") ==
            prepare.CORRECTION_GATE_SHA and
        bindings.get("aggregate_sha256") ==
            frozen.get("source_aggregate_sha256") and
        bindings.get("file_count") == len(bindings.get("files", [])),
        "corrected source binding manifest")
    rows = {row.get("path"): row for row in bindings.get("files", [])}
    require(len(rows) == bindings.get("file_count"),
            "corrected source binding duplicate path")
    for row in bindings.get("files", []):
        path = ROOT / row.get("path", "")
        require(path.is_file() and sha(path) == row.get("sha256"),
                "corrected source binding drift: " + row.get("path", ""))
    for relative, field in capture.TOKEN_ROWS.items():
        row = rows.get(relative, {})
        require(
            row.get("scope") == "read_only_runtime_asset" and
            row.get("sha256") == frozen.get(field) and
            Path(ready["paths"][
                "hud_token" if field == "hud_token_sha256"
                else "shell_token"]).resolve() == (ROOT / relative).resolve(),
            "corrected canonical token path/SHA: " + relative)

    require(
        auth.get("schema") == capture.AUTH_SCHEMA and
        capture.RUNNER_REQUIRED_AUTH_FIELDS.issubset(auth) and
        auth.get("authorized") is False and
        auth.get("consumed") is False and
        auth.get("targeted_visual_repair_gpu_authorized") is False and
        auth.get("case_count") == 28 and
        auth.get("scene_ids_in_order") == expected_ids and
        auth.get("ready_manifest_sha256") == sha(ready_path) and
        auth.get("product_exposed") is False and
        auth.get("golden_promotion") is False and
        auth.get("m2_promotion") is False,
        "corrected unsigned authorization template")
    require("source_bindings_sha256" not in auth,
            "legacy source_bindings_sha256 field forbidden")
    for field, digest in frozen.items():
        require(auth.get(field) == digest,
                "corrected authorization frozen binding: " + field)

    require(receipt == {
        "schema":
            "phase5_rules_decision_ui_v3_binding_corrected_offline_readiness_v1",
        "status":
            "CPU_28_AND_MACHINE_540_BINDING_CORRECTED_READY_GPU_NOT_AUTHORIZED_NOT_RUN",
        "ready_manifest_sha256": sha(ready_path),
        "authorization_template_sha256": sha(auth_path),
        "source_binding_manifest_sha256": sha(paths["source_binding_manifest"]),
        "claims": claims,
    }, "corrected offline readiness receipt")
    return {
        "status":
            "PASS_PHASE5_RULES_UI_V3_BINDING_CORRECTED_TARGETED_READY_NO_GPU",
        "ready_manifest_sha256": sha(ready_path),
        "source_binding_manifest_sha256":
            sha(paths["source_binding_manifest"]),
        "case_count": 28,
        "repair_case_count": 20,
        "regression_control_count": 8,
        "runner_required_auth_field_count":
            len(capture.RUNNER_REQUIRED_AUTH_FIELDS),
        "gpu_authorized": False,
        "gpu_executed": False,
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
