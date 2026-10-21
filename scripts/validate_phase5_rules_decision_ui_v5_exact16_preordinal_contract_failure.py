#!/usr/bin/env python3
"""Validate frozen Phase5 v5 exact16 pre-ordinal contract failure."""

from __future__ import annotations

import argparse
import hashlib
import json
from pathlib import Path
import stat

ROOT = Path(__file__).resolve().parents[1]
DEFAULT = ROOT / "docs/evidence/phase5/rules-decision-ui-v5-exact16-preordinal-contract-failure-receipt-v1.json"


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
            "phase5_rules_decision_ui_v5_exact16_preordinal_contract_failure_receipt_v1" and
            value.get("status") ==
            "FROZEN_PREORDINAL_PRODUCER_CONTRACT_FAILURE_NO_GPU_NO_REUSE",
            "failure receipt identity/status")
    frozen = value.get("frozen_artifacts", {})
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
            frozen["output_root"].get("file_count") == len(files) == 1 and
            files[0] == ROOT / frozen["dispatcher_failure_receipt"]["path"] and
            not list(output.rglob("*.ppm")) and not list(output.rglob("*.png")) and
            not list(output.rglob("*.jsonl")), "frozen output inventory drift")
    auth = json.loads((ROOT / frozen["authorization"]["path"]).read_text())
    marker = json.loads((ROOT / frozen["consumed_marker"]["path"]).read_text())
    failure = json.loads((ROOT / frozen["dispatcher_failure_receipt"]["path"]).read_text())
    require(auth.get("authorization_id") == marker.get("authorization_id") and
            auth.get("authorization_nonce") == marker.get("authorization_nonce") and
            marker.get("authorization_sha256") == frozen["authorization"]["sha256"] and
            marker.get("consumed_before_output_or_process") is True,
            "durable marker identity/binding mismatch")
    require(failure.get("runner_exit_code") == 1 and
            failure.get("completed_count") == failure.get("failed_count") == 0 and
            failure.get("unrun_count") == 16 and
            failure.get("results_sha256") == failure.get("telemetry_sha256") == "" and
            failure.get("validation_errors") == ["results_missing", "telemetry_missing"],
            "dispatcher failure prefix mismatch")
    manifest = json.loads((ROOT / frozen["source_binding_manifest"]["path"]).read_text())
    require(manifest.get("gate_sha256") ==
            frozen["source_binding_manifest"]["gate_sha256"] and
            manifest.get("aggregate_sha256") ==
            frozen["source_binding_manifest"]["aggregate_sha256"],
            "v5 manifest gate/aggregate mismatch")
    binary = (ROOT / frozen["runner_binary"]["path"]).read_bytes()
    require(frozen["runner_binary"]["embedded_gate_sha256"].encode() in binary and
            frozen["runner_binary"]["embedded_readiness_schema"].encode() in binary and
            frozen["source_binding_manifest"]["gate_sha256"].encode() not in binary and
            b"phase5_rules_decision_ui_v5_exact16_readiness_v1" not in binary,
            "runner embedded contract mismatch evidence missing")
    scope = value.get("attempt_scope", {})
    require(scope == {"case_count": 16, "completed_count": 0,
            "failed_case_count": 0, "unrun_count": 16,
            "failed_capture_id": "", "runner_process_started": True,
            "output_created_by_dispatcher_after_runner_failure": True,
            "cocoa_started": False, "opengl_context_created": False,
            "gpu_work_submitted": False, "ppm_count": 0, "png_count": 0,
            "results_row_count": 0, "telemetry_row_count": 0},
            "attempt scope mismatch")
    cause = value.get("root_cause", {})
    require(cause.get("classification") ==
            "CAPTURE_ONLY_PRODUCER_CONTRACT_VERSION_MISMATCH" and
            cause.get("first_mismatch") ==
            "runner_embedded_gate_sha256_vs_v5_source_binding_manifest_gate_sha256" and
            cause.get("latent_next_mismatch") ==
            "runner_embedded_v4_readiness_schema_vs_v5_readiness_schema" and
            cause.get("product_visual_source_fault") is False and
            cause.get("authority_or_rules_fault") is False and
            cause.get("dispatcher_binding_fault") is False,
            "root cause classification mismatch")
    boundary = value.get("single_use_boundary", {})
    require(boundary.get("authorization_consumed_durably") is True and
            boundary.get("authorization_reuse_forbidden") is True and
            boundary.get("consumed_marker_removal_forbidden") is True and
            boundary.get("gpu_retry_authorized") is False and
            boundary.get("root_policy_revoked_after_attempt") is True,
            "single-use boundary mismatch")
    require(not any(value.get("claims", {}).values()), "failure claims must be false")
    return {"status": "PASS_FROZEN_PHASE5_V5_PREORDINAL_CONTRACT_FAILURE",
            "completed_count": 0, "unrun_count": 16,
            "gpu_work_submitted": False, "authorization_reuse_forbidden": True}


if __name__ == "__main__":
    parser = argparse.ArgumentParser()
    parser.add_argument("receipt", type=Path, nargs="?", default=DEFAULT)
    try:
        print(json.dumps(validate(parser.parse_args().receipt), sort_keys=True))
    except (OSError, ValueError, json.JSONDecodeError, RuntimeError) as error:
        print("FAIL: " + str(error))
        raise SystemExit(2)
