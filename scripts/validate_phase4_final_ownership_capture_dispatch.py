#!/usr/bin/env python3
"""Validate final-ownership targeted11 dispatcher pass/failure artifacts."""

from __future__ import annotations

import argparse
import hashlib
import json
from pathlib import Path

CANONICAL_PLAN_SHA256 = "3b969b34e52fe2e01e31e34b148c4ce7c211682db65b3c609a43274366c8717d"
PASS_SCHEMA = "billiardgl.phase4.final_ownership_capture_dispatch_pass.v1"
FAILURE_SCHEMA = "billiardgl.phase4.final_ownership_capture_dispatch_failure.v1"
PLAN_NAME = "phase4-final-ownership-targeted11-plan.tsv"
RESULTS_NAME = "results.jsonl"
TELEMETRY_NAME = RESULTS_NAME + ".final-ownership.jsonl"


def sha(path: Path) -> str:
    return hashlib.sha256(path.read_bytes()).hexdigest()


def read_jsonl(path: Path, errors: list[str], label: str) -> list[dict]:
    if not path.is_file():
        return []
    rows: list[dict] = []
    try:
        for ordinal, line in enumerate(path.read_text().splitlines(), 1):
            if not line:
                continue
            value = json.loads(line)
            if not isinstance(value, dict):
                errors.append(f"{label}_row_not_object:{ordinal}")
            else:
                rows.append(value)
    except (OSError, ValueError) as error:
        errors.append(f"{label}_invalid:{type(error).__name__}")
    return rows


def analyze_output(ready: dict, output: Path, runner_exit_code: int) -> tuple[dict, list[str]]:
    errors: list[str] = []
    output = output.resolve()
    expected_ids = [row.get("capture_id") for row in ready.get("captures", [])]
    plan = output / PLAN_NAME
    results = output / RESULTS_NAME
    telemetry = output / TELEMETRY_NAME
    if not plan.is_file() or sha(plan) != CANONICAL_PLAN_SHA256 or \
            plan.read_bytes() != Path(ready.get("plan_path", "")).read_bytes():
        errors.append("executed_plan_not_exact_canonical_bytes")
    result_rows = read_jsonl(results, errors, "results")
    capture_rows = [row for row in result_rows if row.get("capture_id")]
    summaries = [row for row in result_rows if row.get("record_type") == "summary"]
    telemetry_rows = read_jsonl(telemetry, errors, "telemetry")
    completed_ids = [row.get("capture_id") for row in capture_rows]
    telemetry_ids = [row.get("capture_id") for row in telemetry_rows]
    if completed_ids != expected_ids[:len(completed_ids)] or \
            telemetry_ids != expected_ids[:len(telemetry_ids)]:
        errors.append("capture_or_telemetry_order_not_plan_prefix")
    if len(capture_rows) > 11 or len(telemetry_rows) > 11:
        errors.append("capture_or_telemetry_count_exceeds_targeted11")

    if runner_exit_code == 0:
        if len(capture_rows) != 11 or len(summaries) != 1 or len(telemetry_rows) != 11:
            errors.append("pass_requires_11_rows_11_telemetry_and_one_summary")
        if any(not str(row.get("machine_status", "")).startswith("V4_") or
               row.get("authorization_receipt_consumed") is not True or
               row.get("gpu_capture_authorized_for_this_invocation") is not True
               for row in capture_rows):
            errors.append("pass_capture_machine_or_authorization_invalid")
        if any(row.get("machine_status") != "PASS" or
               row.get("integrated_final_ownership") is not True
               for row in telemetry_rows):
            errors.append("pass_telemetry_not_integrated_machine_pass")
        if summaries:
            summary = summaries[0]
            if summary.get("capture_count") != 11 or \
                    not str(summary.get("machine_status", "")).startswith("V4_") or \
                    summary.get("ta_status") != "NOT_RUN" or \
                    summary.get("manual_status") != "NOT_RUN" or \
                    summary.get("m3_status") != "NOT_CLAIMED":
                errors.append("pass_summary_invalid")
        return {
            "outcome": "PASS",
            "completed_count": len(capture_rows),
            "failed_count": 0,
            "unrun_count": 0,
            "failed_capture_id": "",
            "results_path": str(results),
            "results_sha256": sha(results) if results.is_file() else "",
            "telemetry_path": str(telemetry),
            "telemetry_sha256": sha(telemetry) if telemetry.is_file() else "",
        }, errors

    if summaries:
        errors.append("failed_run_must_not_claim_pass_summary")
    failed_rows = [row for row in telemetry_rows if row.get("machine_status") == "FAILED"]
    if len(failed_rows) > 1 or (failed_rows and telemetry_rows[-1] is not failed_rows[-1]):
        errors.append("failed_telemetry_must_be_single_terminal_row")
    if any(row.get("machine_status") != "PASS" for row in telemetry_rows[:-1]):
        errors.append("pre_failure_telemetry_not_pass")
    failed_count = 1 if failed_rows else 0
    if len(telemetry_rows) not in (len(capture_rows), len(capture_rows) + failed_count):
        errors.append("failure_result_telemetry_count_incoherent")
    if any(row.get("authorization_receipt_consumed") is not True or
           row.get("gpu_capture_authorized_for_this_invocation") is not True
           for row in capture_rows):
        errors.append("completed_failure_prefix_authorization_invalid")
    unrun = 11 - len(capture_rows) - failed_count
    if unrun < 0:
        errors.append("failure_counts_exceed_targeted11")
        unrun = 0
    failed_capture = failed_rows[-1].get("capture_id", "") if failed_rows else ""
    return {
        "outcome": "FAILURE",
        "completed_count": len(capture_rows),
        "failed_count": failed_count,
        "unrun_count": unrun,
        "failed_capture_id": failed_capture,
        "results_path": str(results),
        "results_sha256": sha(results) if results.is_file() else "",
        "telemetry_path": str(telemetry),
        "telemetry_sha256": sha(telemetry) if telemetry.is_file() else "",
    }, errors


def validate_receipt(receipt: dict, ready: dict, output: Path,
                     runner_exit_code: int, analysis: dict) -> list[str]:
    errors: list[str] = []
    output = output.resolve()
    try:
        ready_path = Path(receipt["ready_manifest_path"])
        ready_sha = sha(ready_path)
    except (KeyError, OSError, TypeError):
        ready_sha = ""
        errors.append("dispatch_ready_manifest_binding_invalid")
    expected_common = {
        "ready_manifest_sha256": ready_sha,
        "design_gate_sha256": ready.get("gate_bindings", {}).get(
            "design_gate_sha256"),
        "implementation_source_aggregate_sha256":
            ready.get("implementation_source_aggregate_sha256"),
        "source_binding_manifest_sha256":
            ready.get("source_binding_manifest_sha256"),
        "runner_sha256": ready.get("runner_sha256"),
        "billiards_executable_sha256":
            ready.get("billiards_executable_sha256"),
        "cpu_fixture_receipt_sha256": ready.get("cpu_fixture_receipt_sha256"),
        "frozen_replay_receipt_sha256":
            ready.get("frozen_replay_receipt_sha256"),
        "plan_sha256": CANONICAL_PLAN_SHA256,
        "plan_path": str(output / PLAN_NAME),
        "output_root": str(output.resolve()),
        "runner_exit_code": runner_exit_code,
        "completed_count": analysis["completed_count"],
        "failed_count": analysis["failed_count"],
        "unrun_count": analysis["unrun_count"],
        "failed_capture_id": analysis["failed_capture_id"],
        "results_path": analysis["results_path"],
        "results_sha256": analysis["results_sha256"],
        "telemetry_path": analysis["telemetry_path"],
        "telemetry_sha256": analysis["telemetry_sha256"],
        "authorization_consumed_once": True,
        "technical_art_review_authorized": False,
        "formal68_authorized": False,
    }
    expected_schema = PASS_SCHEMA if runner_exit_code == 0 else FAILURE_SCHEMA
    if receipt.get("schema") != expected_schema:
        errors.append("dispatch_receipt_schema_invalid")
    for key, value in expected_common.items():
        if receipt.get(key) != value:
            errors.append(f"dispatch_receipt_binding_invalid:{key}")
    if runner_exit_code == 0 and receipt.get("status") != "MACHINE_PASS_PENDING_SEPARATE_TA":
        errors.append("dispatch_pass_status_invalid")
    if runner_exit_code != 0 and receipt.get("status") not in (
            "CAPTURE_FAILED_TELEMETRY_PRESERVED",
            "CAPTURE_INFRASTRUCTURE_FAILED_NO_ORDINAL_TELEMETRY"):
        errors.append("dispatch_failure_status_invalid")
    if receipt.get("capture_count") != 11 or \
            receipt.get("completed_count", 0) + receipt.get("failed_count", 0) + \
            receipt.get("unrun_count", 0) != 11:
        errors.append("dispatch_receipt_counts_invalid")
    try:
        auth_path = Path(receipt["authorization_receipt_path"])
        consumed = Path(str(auth_path) + ".consumed")
        if not auth_path.is_file() or sha(auth_path) != \
                receipt.get("authorization_receipt_sha256") or \
                not consumed.is_file() or consumed.read_text() != \
                receipt.get("authorization_nonce", "") + "\n":
            errors.append("dispatch_authorization_consumption_binding_invalid")
    except (KeyError, OSError, TypeError):
        errors.append("dispatch_authorization_consumption_binding_invalid")
    if receipt.get("validation_errors") != []:
        errors.append("dispatch_receipt_contains_validation_errors")
    return errors


def main() -> None:
    parser = argparse.ArgumentParser()
    parser.add_argument("--ready", type=Path, required=True)
    parser.add_argument("--output-root", type=Path, required=True)
    parser.add_argument("--runner-exit-code", type=int, required=True)
    parser.add_argument("--receipt", type=Path, required=True)
    args = parser.parse_args()
    errors: list[str] = []
    try:
        ready = json.loads(args.ready.read_text())
        analysis, analysis_errors = analyze_output(
            ready, args.output_root.resolve(), args.runner_exit_code)
        errors.extend(analysis_errors)
        receipt = json.loads(args.receipt.read_text())
        errors.extend(validate_receipt(receipt, ready, args.output_root.resolve(),
                                       args.runner_exit_code, analysis))
    except (OSError, ValueError, TypeError) as error:
        errors.append(f"dispatch_validation_input_invalid:{type(error).__name__}")
    print(json.dumps({"status": "PASS" if not errors else "FAIL",
                      "gpu_or_cocoa_invoked": False, "errors": errors},
                     indent=2, sort_keys=True))
    if errors:
        raise SystemExit(1)


if __name__ == "__main__":
    main()
