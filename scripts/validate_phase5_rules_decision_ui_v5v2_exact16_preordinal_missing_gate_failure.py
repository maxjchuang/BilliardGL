#!/usr/bin/env python3
"""Validate frozen v5/v2 pre-ordinal missing gate_sha256 failure."""

from __future__ import annotations

import argparse
import hashlib
import json
from pathlib import Path
import stat

ROOT = Path(__file__).resolve().parents[1]
DEFAULT = ROOT / "docs/evidence/phase5/rules-decision-ui-v5v2-exact16-preordinal-missing-gate-field-failure-receipt-v1.json"


def require(value: bool, message: str) -> None:
    if not value:
        raise RuntimeError(message)


def sha(path: Path) -> str:
    return hashlib.sha256(path.read_bytes()).hexdigest()


def mode(path: Path) -> str:
    return f"0{stat.S_IMODE(path.stat().st_mode):03o}"


def validate(path: Path = DEFAULT) -> dict:
    value = json.loads(path.read_text(encoding="utf-8"))
    require(value.get("schema") ==
            "phase5_rules_decision_ui_v5v2_exact16_preordinal_missing_gate_field_failure_receipt_v1" and
            value.get("status") ==
            "FROZEN_PREORDINAL_MISSING_MANIFEST_FIELD_NO_GPU_NO_REUSE",
            "receipt identity/status")
    frozen = value["frozen_artifacts"]
    for key in ("authorization", "consumed_marker", "dispatcher_failure_receipt",
                "readiness", "source_binding_manifest", "runner_binary"):
        row = frozen[key]
        live = ROOT / row["path"]
        require(live.is_file() and sha(live) == row["sha256"],
                "frozen artifact drift: " + key)
        if "mode" in row:
            require(mode(live) == row["mode"], "frozen mode drift: " + key)
    output = ROOT / frozen["output_root"]["path"]
    files = [item for item in output.rglob("*") if item.is_file()]
    require(output.is_dir() and mode(output) == "0555" and
            frozen["output_root"]["file_count"] == len(files) == 1 and
            files[0] == ROOT / frozen["dispatcher_failure_receipt"]["path"] and
            not list(output.rglob("*.ppm")) and not list(output.rglob("*.png")) and
            not list(output.rglob("*.jsonl")), "frozen output inventory")
    auth = json.loads((ROOT / frozen["authorization"]["path"]).read_text())
    marker = json.loads((ROOT / frozen["consumed_marker"]["path"]).read_text())
    failure = json.loads((ROOT / frozen["dispatcher_failure_receipt"]["path"]).read_text())
    require(auth["authorization_id"] == marker["authorization_id"] and
            auth["authorization_nonce"] == marker["authorization_nonce"] and
            marker["authorization_sha256"] == frozen["authorization"]["sha256"] and
            marker["consumed_before_output_or_process"] is True,
            "durable consumption identity")
    require(failure.get("runner_exit_code") == 1 and
            failure.get("completed_count") == failure.get("failed_count") == 0 and
            failure.get("unrun_count") == 16 and
            failure.get("results_sha256") == failure.get("telemetry_sha256") == "" and
            failure.get("validation_errors") == ["results_missing", "telemetry_missing"],
            "dispatcher failure prefix")
    manifest = json.loads((ROOT / frozen["source_binding_manifest"]["path"]).read_text())
    require(manifest.get("schema") == frozen["source_binding_manifest"]["schema"] and
            manifest.get("aggregate_sha256") ==
            frozen["source_binding_manifest"]["aggregate_sha256"] and
            manifest.get("visual_design_gate_sha256") ==
            frozen["source_binding_manifest"]["visual_design_gate_sha256"] and
            "gate_sha256" not in manifest and
            frozen["source_binding_manifest"]["gate_sha256_present"] is False,
            "missing manifest field evidence")
    binary = (ROOT / frozen["runner_binary"]["path"]).read_bytes()
    require(frozen["runner_binary"]["expected_manifest_gate_field"].encode() in binary and
            frozen["runner_binary"]["embedded_gate_sha256"].encode() in binary and
            frozen["runner_binary"]["embedded_readiness_schema"].encode() in binary,
            "runner embedded field/gate/schema evidence")
    claim = Path(frozen["consumed_marker"]["path"] + ".exact16-runner-claimed")
    require(not (ROOT / claim).exists(), "runner claim marker must be absent")
    scope = value["attempt_scope"]
    require(scope.get("completed_count") == scope.get("failed_case_count") == 0 and
            scope.get("unrun_count") == 16 and scope.get("runner_process_started") is True and
            scope.get("runner_claim_marker_created") is False and
            scope.get("cocoa_started") is False and
            scope.get("opengl_context_created") is False and
            scope.get("gpu_work_submitted") is False and
            scope.get("ppm_count") == scope.get("png_count") == 0,
            "attempt scope")
    failure_detail = value["failure"]
    require(failure_detail.get("runner_stdout_error") ==
            "phase5-rules-ui-gpu-capture: gate_sha256" and
            failure_detail.get("missing_field") == "gate_sha256" and
            failure_detail.get("before_runner_claim_marker") is True and
            failure_detail.get("before_nsapplication") is True,
            "exact failure stage")
    cause = value["root_cause"]
    require(cause.get("classification") ==
            "CAPTURE_ONLY_MANIFEST_SCHEMA_RUNNER_FIELD_PARITY_GAP" and
            cause.get("exact_missing_field") == "gate_sha256" and
            cause.get("manifest_provided_semantic_equivalent_field") ==
            "visual_design_gate_sha256" and
            cause.get("runner_required_manifest_fields") ==
            ["gate_sha256", "aggregate_sha256"] and
            cause.get("product_visual_source_fault") is False and
            cause.get("authority_rules_physics_fault") is False and
            cause.get("single_use_dispatcher_fault") is False,
            "root cause")
    boundary = value["single_use_boundary"]
    require(boundary.get("authorization_consumed_durably") is True and
            boundary.get("authorization_reuse_forbidden") is True and
            boundary.get("consumed_marker_removal_forbidden") is True and
            boundary.get("runner_claim_marker_absent") is True and
            boundary.get("gpu_retry_authorized") is False,
            "single-use boundary")
    require(not any(value["claims"].values()), "claims must remain false")
    return {"status": "PASS_FROZEN_PHASE5_V5V2_MISSING_GATE_FIELD_FAILURE",
            "completed_count": 0, "unrun_count": 16,
            "missing_field": "gate_sha256", "gpu_executed": False,
            "authorization_reuse_forbidden": True}


if __name__ == "__main__":
    parser = argparse.ArgumentParser()
    parser.add_argument("receipt", type=Path, nargs="?", default=DEFAULT)
    try:
        print(json.dumps(validate(parser.parse_args().receipt), sort_keys=True))
    except (OSError, ValueError, json.JSONDecodeError, RuntimeError) as error:
        print("FAIL: " + str(error))
        raise SystemExit(2)
