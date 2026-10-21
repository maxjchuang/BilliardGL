#!/usr/bin/env python3
"""CPU-only validation for Phase5 Rules UI v4 exact16 dispatch output."""

from __future__ import annotations

import hashlib
import json
from pathlib import Path
from typing import Any

PASS_SCHEMA = "phase5_rules_decision_ui_v4_exact16_dispatch_pass_v1"
FAILURE_SCHEMA = "phase5_rules_decision_ui_v4_exact16_dispatch_failure_v1"
RESULTS_NAME = "rules-decision-ui-v4-exact16-results.jsonl"
TELEMETRY_NAME = "rules-decision-ui-v4-exact16-telemetry.jsonl"
PASS_RECEIPT_NAME = "rules-decision-ui-v4-exact16-dispatch-pass.json"
FAILURE_RECEIPT_NAME = "rules-decision-ui-v4-exact16-dispatch-failure.json"


def sha(path: Path) -> str:
    return hashlib.sha256(path.read_bytes()).hexdigest()


def read_jsonl(path: Path, errors: list[str], label: str) -> list[dict[str, Any]]:
    if not path.is_file():
        errors.append(label + "_missing")
        return []
    rows: list[dict[str, Any]] = []
    try:
        for line in path.read_text(encoding="utf-8").splitlines():
            if line:
                value = json.loads(line)
                if not isinstance(value, dict):
                    raise ValueError("object required")
                rows.append(value)
    except (OSError, ValueError, json.JSONDecodeError):
        errors.append(label + "_invalid_jsonl")
        return []
    return rows


def analyze_output(ready: dict[str, Any], output: Path,
                   runner_exit_code: int) -> tuple[dict[str, Any], list[str]]:
    output = output.resolve()
    errors: list[str] = []
    expected = ready.get("case_ids_in_order", [])
    results_path = output / RESULTS_NAME
    telemetry_path = output / TELEMETRY_NAME
    results = read_jsonl(results_path, errors, "results")
    telemetry = read_jsonl(telemetry_path, errors, "telemetry")
    result_ids = [row.get("capture_id") for row in results]
    telemetry_ids = [row.get("capture_id") for row in telemetry]
    if result_ids != expected[:len(result_ids)]:
        errors.append("result_order_not_exact16_prefix")
    if telemetry_ids != expected[:len(telemetry_ids)]:
        errors.append("telemetry_order_not_exact16_prefix")
    if len(results) > 16 or len(telemetry) > 16:
        errors.append("row_count_exceeds_exact16")

    def valid_common(row: dict[str, Any]) -> bool:
        passes = row.get("render_passes", [])
        return row.get("actual_gpu_capture") is True and \
            row.get("rules_ui_after_tone_map") is True and \
            isinstance(passes, list) and "product_ui" in passes and \
            "product_present" in passes and \
            row.get("authorization_consumed_before_process") is True

    if any(not valid_common(row) for row in results):
        errors.append("result_actual_gpu_or_after_tonemap_invalid")
    for index, row in enumerate(results):
        try:
            ppm = Path(row["ppm_path"])
            if ppm.parent.resolve() != (output / "ppm").resolve() or \
                    ppm.name != expected[index] + ".ppm" or \
                    not ppm.is_file() or sha(ppm) != row.get("ppm_sha256") or \
                    row.get("physical_width", 0) <= 0 or \
                    row.get("physical_height", 0) <= 0:
                errors.append("result_ppm_binding_invalid:" + expected[index])
        except (KeyError, OSError, TypeError):
            errors.append("result_ppm_binding_invalid:" + str(row.get("capture_id")))

    failed_rows = [row for row in telemetry
                   if row.get("machine_status") == "FAILED"]
    pass_rows = [row for row in telemetry
                 if row.get("machine_status") == "PASS"]
    if any(row.get("actual_gpu_capture") is not True or
           row.get("rules_ui_after_tone_map") is not True
           for row in telemetry):
        errors.append("telemetry_gpu_contract_invalid")
    if runner_exit_code == 0:
        if len(results) != 16 or len(telemetry) != 16 or len(pass_rows) != 16:
            errors.append("pass_requires_exact16_results_and_telemetry")
        if failed_rows:
            errors.append("pass_contains_failed_telemetry")
        outcome = "PASS"
        completed, failed, unrun, failed_id = 16, 0, 0, ""
    else:
        if len(failed_rows) > 1 or (failed_rows and telemetry[-1] is not failed_rows[-1]):
            errors.append("failure_must_have_at_most_one_terminal_failed_row")
        if any(row.get("machine_status") != "PASS" for row in telemetry[:-1]):
            errors.append("failure_prefix_telemetry_not_pass")
        failed = 1 if failed_rows else 0
        completed = len(results)
        if len(telemetry) != completed + failed:
            errors.append("failure_result_telemetry_count_incoherent")
        unrun = max(0, 16 - completed - failed)
        failed_id = failed_rows[-1].get("capture_id", "") if failed_rows else ""
        outcome = "FAILURE"
    analysis = {
        "outcome": outcome, "completed_count": completed,
        "failed_count": failed, "unrun_count": unrun,
        "failed_capture_id": failed_id,
        "results_path": str(results_path),
        "results_sha256": sha(results_path) if results_path.is_file() else "",
        "telemetry_path": str(telemetry_path),
        "telemetry_sha256": sha(telemetry_path) if telemetry_path.is_file() else "",
    }
    return analysis, errors


def validate_receipt(receipt: dict[str, Any], ready: dict[str, Any],
                     output: Path, runner_exit_code: int,
                     analysis: dict[str, Any]) -> list[str]:
    errors: list[str] = []
    expected_schema = PASS_SCHEMA if runner_exit_code == 0 else FAILURE_SCHEMA
    if receipt.get("schema") != expected_schema:
        errors.append("receipt_schema_invalid")
    expected = {
        "runner_exit_code": runner_exit_code,
        "case_count": 16,
        "completed_count": analysis["completed_count"],
        "failed_count": analysis["failed_count"],
        "unrun_count": analysis["unrun_count"],
        "failed_capture_id": analysis["failed_capture_id"],
        "results_path": analysis["results_path"],
        "results_sha256": analysis["results_sha256"],
        "telemetry_path": analysis["telemetry_path"],
        "telemetry_sha256": analysis["telemetry_sha256"],
        "output_root": str(output.resolve()),
        "authorization_consumed_once": True,
        "technical_art_review": False,
        "product_exposed": False,
        "golden_promotion": False,
        "m2_promotion": False,
    }
    for field, value in expected.items():
        if receipt.get(field) != value:
            errors.append("receipt_binding_invalid:" + field)
    if receipt.get("case_ids_in_order") != ready.get("case_ids_in_order"):
        errors.append("receipt_case_order_invalid")
    if receipt.get("validation_errors") != []:
        errors.append("receipt_validation_errors_not_empty")
    try:
        marker = Path(receipt["consumed_marker_path"])
        auth = Path(receipt["authorization_path"])
        if not marker.is_file() or sha(marker) != receipt.get("consumed_marker_sha256") or \
                not auth.is_file() or sha(auth) != receipt.get("authorization_sha256"):
            errors.append("receipt_auth_or_marker_binding_invalid")
    except (KeyError, OSError, TypeError):
        errors.append("receipt_auth_or_marker_binding_invalid")
    return errors
