#!/usr/bin/env python3
"""Fail-closed single-use dispatcher for Phase5 Rules UI v4 exact16.

Without --execute-authorized-capture this performs only a NO-GPU readiness
check.  Authorization is consumed durably before output or subprocess work.
"""

from __future__ import annotations

import argparse
import hashlib
import json
import os
from pathlib import Path
import subprocess
import sys
import time

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / "scripts"))
from validate_phase5_rules_decision_ui_v4_exact16_capture_dispatch import (  # noqa:E402
    FAILURE_RECEIPT_NAME, FAILURE_SCHEMA, PASS_RECEIPT_NAME, PASS_SCHEMA,
    RESULTS_NAME, TELEMETRY_NAME, analyze_output,
)

READY_SCHEMA = "phase5_rules_decision_ui_v4_exact16_readiness_v2"
AUTH_SCHEMA = "phase5_rules_decision_ui_v4_exact16_gpu_authorization_v2"
AUTH_FIELDS = {
    "schema", "status", "authorized", "consumed", "single_use",
    "authorization_id", "authorization_nonce", "approved_by", "approved_at",
    "expires_unix_seconds", "authorized_output_canonical_path", "case_count",
    "scene_ids_in_order", "ready_manifest_sha256", "dispatcher_sha256",
    "runner_binary_path", "runner_binary_sha256", "runner_source_sha256",
    "plan_sha256", "source_binding_manifest_sha256", "source_aggregate_sha256",
    "billiards_binary_sha256", "font_sha256", "hud_token_sha256",
    "shell_token_sha256", "exact16_capture_producer_contract",
    "gpu_authorized", "gpu_executed", "technical_art_review",
    "product_exposed", "golden_promotion", "m2_promotion",
}


def sha(path: Path) -> str:
    return hashlib.sha256(path.read_bytes()).hexdigest()


def require(condition: bool, message: str) -> None:
    if not condition:
        raise RuntimeError(message)


def load(path: Path) -> dict:
    value = json.loads(path.read_text(encoding="utf-8"))
    require(isinstance(value, dict), "JSON object required: " + str(path))
    return value


def validate_ready(path: Path) -> dict:
    value = load(path)
    identity = (value.get("schema"), value.get("status"))
    require(identity in (
                (READY_SCHEMA, "V4_EXACT16_DISPATCH_READY_NOT_RUN_NO_AUTH"),
                ("phase5_rules_decision_ui_v4_exact16_readiness_v3",
                 "V4_EXACT16_PRODUCER_BOUND_NOT_RUN_NO_AUTH"),
                ("phase5_rules_decision_ui_v5_exact16_readiness_v1",
                 "V5_PIXEL_VISIBLE_EXACT16_PRODUCER_BOUND_NOT_RUN_NO_AUTH"),
                ("phase5_rules_decision_ui_v5_exact16_readiness_v2",
                 "V5_PIXEL_VISIBLE_EXACT16_CORRECTED_PRODUCER_NOT_RUN_NO_AUTH")) and
            value.get("case_count") == 16 and
            value.get("repair_case_count") == 4 and
            value.get("regression_control_count") == 12,
            "exact16 readiness identity invalid")
    ids = value.get("case_ids_in_order")
    require(isinstance(ids, list) and len(ids) == len(set(ids)) == 16,
            "exact16 readiness scope invalid")
    require(value.get("case_results") == [
        {"ordinal": index + 1, "capture_id": item, "status": "NOT_RUN"}
        for index, item in enumerate(ids)], "exact16 readiness rows invalid")
    paths = value.get("paths", {})
    bindings = value.get("frozen_bindings", {})
    mapping = {
        "plan_sha256": "plan", "source_binding_manifest_sha256": "source_binding_manifest",
        "billiards_binary_sha256": "billiards_binary", "font_sha256": "font",
        "hud_token_sha256": "hud_token", "shell_token_sha256": "shell_token",
        "dispatcher_sha256": "dispatcher", "dispatch_validator_sha256": "dispatch_validator",
    }
    if value.get("schema") in {
            "phase5_rules_decision_ui_v4_exact16_readiness_v3",
            "phase5_rules_decision_ui_v5_exact16_readiness_v1",
            "phase5_rules_decision_ui_v5_exact16_readiness_v2"}:
        mapping.update({"runner_source_sha256": "runner_source",
                        "runner_binary_sha256": "runner_binary",
                        "cmake_sha256": "cmake"})
        expected_producer = ({
            "target": "BilliardsPhase5RulesDecisionUiV5Exact16GpuCaptureRunner",
            "compile_definitions": [
                "BILLIARDGL_PHASE5_RULES_UI_V4_EXACT16=1",
                "BILLIARDGL_PHASE5_RULES_UI_V5_EXACT16=1"],
            "mode_exclusive": True,
        } if value.get("schema") ==
            "phase5_rules_decision_ui_v5_exact16_readiness_v2" else {
            "target": "BilliardsPhase5RulesDecisionUiV4Exact16GpuCaptureRunner",
            "compile_definition": "BILLIARDGL_PHASE5_RULES_UI_V4_EXACT16=1",
            "mode_exclusive": True,
        })
        require(value.get("capture_producer", {}) == expected_producer,
                "exact16 capture producer identity invalid")
    for field, key in mapping.items():
        live = Path(paths.get(key, ""))
        require(live.is_file() and sha(live) == bindings.get(field),
                "readiness live binding drift: " + field)
    require(sha(Path(paths["plan"])) == value.get("frozen_bindings", {}).get("plan_sha256"),
            "plan binding invalid")
    return value


def expected_auth(ready_path: Path, ready: dict, output: Path,
                  runner: Path) -> dict:
    frozen = ready["frozen_bindings"]
    claims = {"gpu_authorized": True, "gpu_executed": False,
              "technical_art_review": False, "product_exposed": False,
              "golden_promotion": False, "m2_promotion": False}
    return {
        "schema": AUTH_SCHEMA,
        "status": "AUTHORIZED_FOR_EXACTLY_16_SINGLE_USE_CAPTURES",
        "authorized": True, "consumed": False, "single_use": True,
        "authorized_output_canonical_path": str(output.resolve()),
        "case_count": 16, "scene_ids_in_order": ready["case_ids_in_order"],
        "ready_manifest_sha256": sha(ready_path),
        "dispatcher_sha256": sha(Path(__file__)),
        "runner_binary_path": str(runner.resolve()),
        "runner_binary_sha256": sha(runner),
        "runner_source_sha256": frozen["runner_source_sha256"],
        "plan_sha256": frozen["plan_sha256"],
        "source_binding_manifest_sha256": frozen["source_binding_manifest_sha256"],
        "source_aggregate_sha256": frozen["source_aggregate_sha256"],
        "billiards_binary_sha256": frozen["billiards_binary_sha256"],
        "font_sha256": frozen["font_sha256"],
        "hud_token_sha256": frozen["hud_token_sha256"],
        "shell_token_sha256": frozen["shell_token_sha256"],
        "exact16_capture_producer_contract": True,
        **claims,
    }


def validate_auth(path: Path, ready_path: Path, ready: dict,
                  output: Path, runner: Path) -> tuple[dict, str]:
    require(path.is_file() and runner.is_file(), "authorization or runner missing")
    value = load(path)
    require(set(value) == AUTH_FIELDS, "authorization exact fields mismatch")
    expected = expected_auth(ready_path, ready, output, runner)
    for field, expected_value in expected.items():
        require(value.get(field) == expected_value, "authorization binding drift: " + field)
    for field in ("authorization_id", "authorization_nonce", "approved_by", "approved_at"):
        require(isinstance(value.get(field), str) and len(value[field]) >= 8,
                "authorization identity invalid: " + field)
    require(isinstance(value.get("expires_unix_seconds"), (int, float)) and
            value["expires_unix_seconds"] > time.time(),
            "authorization expiry invalid")
    marker = Path(str(path) + ".consumed.json")
    require(not marker.exists(), "authorization already consumed")
    require(not output.exists(), "authorized output root must not exist before process")
    return value, sha(path)


def consume(path: Path, auth: dict, auth_sha: str, ready_path: Path,
            output: Path, runner: Path) -> Path:
    marker = Path(str(path) + ".consumed.json")
    payload = {
        "schema": "phase5_rules_decision_ui_v4_exact16_authorization_consumed_v1",
        "authorization_path": str(path), "authorization_sha256": auth_sha,
        "authorization_id": auth["authorization_id"],
        "authorization_nonce": auth["authorization_nonce"],
        "ready_manifest_path": str(ready_path),
        "ready_manifest_sha256": sha(ready_path),
        "output_root": str(output), "runner_binary_path": str(runner),
        "runner_binary_sha256": sha(runner), "consumed_capture_count": 16,
        "scene_ids_in_order": auth["scene_ids_in_order"],
        "consumed_before_output_or_process": True,
    }
    descriptor = os.open(marker, os.O_WRONLY | os.O_CREAT | os.O_EXCL, 0o600)
    with os.fdopen(descriptor, "w", encoding="utf-8") as stream:
        stream.write(json.dumps(payload, indent=2, sort_keys=True) + "\n")
        stream.flush()
        os.fsync(stream.fileno())
    require(marker.is_file(), "consumed marker missing after durable write")
    return marker


def dispatch(args: argparse.Namespace) -> int:
    ready_path = args.ready.resolve(strict=True)
    ready = validate_ready(ready_path)
    if not args.execute_authorized_capture:
        require(args.authorization is None and args.runner_binary is None and
                args.output_root is None,
                "authorization/runner/output accepted only with --execute-authorized-capture")
        print(json.dumps({"status": "DRY_RUN_READY_NO_GPU_NO_AUTH",
                          "case_count": 16, "gpu_or_cocoa_invoked": False,
                          "ready_manifest_sha256": sha(ready_path)}, sort_keys=True))
        return 0
    require(args.authorization and args.runner_binary and args.output_root,
            "--execute-authorized-capture requires authorization, runner and output")
    require(ready.get("dispatch_gate", {}).get("compatible_capture_producer_bound") is True,
            "readiness does not bind an exact16-compatible capture producer")
    auth_path = args.authorization.resolve(strict=True)
    runner = args.runner_binary.resolve(strict=True)
    output = args.output_root.resolve()
    auth, auth_sha = validate_auth(auth_path, ready_path, ready, output, runner)
    marker = consume(auth_path, auth, auth_sha, ready_path, output, runner)
    command = [str(runner), str(auth_path), ready["paths"]["plan"], str(output),
               str(output / RESULTS_NAME), ready["paths"]["billiards_binary"],
               ready["paths"]["font"], ready["paths"]["source_binding_manifest"],
               str(ready_path), str(Path(__file__).resolve())]
    completed = subprocess.run(command, check=False)
    analysis, errors = analyze_output(ready, output, completed.returncode)
    output.mkdir(parents=True, exist_ok=True)
    common = {
        "status": "TARGETED_MACHINE16_PASS_PENDING_TA" if completed.returncode == 0 and not errors
                  else "CAPTURE_FAILURE_BYTES_AND_TELEMETRY_PRESERVED",
        "ready_manifest_path": str(ready_path), "ready_manifest_sha256": sha(ready_path),
        "authorization_path": str(auth_path), "authorization_sha256": auth_sha,
        "consumed_marker_path": str(marker), "consumed_marker_sha256": sha(marker),
        "authorization_consumed_once": True, "runner_binary_path": str(runner),
        "runner_binary_sha256": sha(runner), "runner_exit_code": completed.returncode,
        "output_root": str(output),
        "case_count": 16, "case_ids_in_order": ready["case_ids_in_order"],
        **analysis, "validation_errors": errors,
        "technical_art_review": False, "product_exposed": False,
        "golden_promotion": False, "m2_promotion": False,
    }
    passed = completed.returncode == 0 and not errors
    receipt = {"schema": PASS_SCHEMA if passed else FAILURE_SCHEMA, **common}
    receipt_path = output / (PASS_RECEIPT_NAME if passed else FAILURE_RECEIPT_NAME)
    with receipt_path.open("x", encoding="utf-8") as stream:
        stream.write(json.dumps(receipt, indent=2, sort_keys=True) + "\n")
        stream.flush()
        os.fsync(stream.fileno())
    return 0 if passed else (completed.returncode or 1)


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--ready", type=Path, required=True)
    parser.add_argument("--authorization", type=Path)
    parser.add_argument("--runner-binary", type=Path)
    parser.add_argument("--output-root", type=Path)
    parser.add_argument("--execute-authorized-capture", action="store_true")
    try:
        return dispatch(parser.parse_args())
    except (OSError, ValueError, json.JSONDecodeError, RuntimeError) as error:
        print("phase5-v4-exact16-dispatch: " + str(error), file=sys.stderr)
        return 2


if __name__ == "__main__":
    raise SystemExit(main())
