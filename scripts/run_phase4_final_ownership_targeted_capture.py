#!/usr/bin/env python3
"""Single-use dispatcher for Phase4 final-ownership targeted11 capture.

The default mode validates readiness only. GPU execution is possible solely
with ``--run`` and a separately issued exact authorization receipt.
"""

from __future__ import annotations

import argparse
import hashlib
import json
import os
from pathlib import Path
import subprocess
import sys
import tempfile

SCRIPT_DIR = Path(__file__).resolve().parent
if str(SCRIPT_DIR) not in sys.path:
    sys.path.insert(0, str(SCRIPT_DIR))

from validate_phase4_final_ownership_capture_dispatch import (  # noqa: E402
    CANONICAL_PLAN_SHA256, FAILURE_SCHEMA, PASS_SCHEMA, PLAN_NAME,
    analyze_output,
)
from validate_phase4_final_ownership_targeted_readiness import (  # noqa: E402
    validate_manifest,
)

AUTH_SCHEMA = "billiardgl.phase4.final_ownership_gpu_authorization.v1"
AUTH_ENV = "BILLIARDGL_PHASE4_FINAL_OWNERSHIP_GPU_AUTH_RECEIPT"
OLD_AUTH_ENV = "BILLIARDGL_PHASE4_V4_GPU_AUTH_RECEIPT"
PASS_RECEIPT_NAME = "phase4-final-ownership-dispatch-pass-v1.json"
FAILURE_RECEIPT_NAME = "phase4-final-ownership-dispatch-failure-receipt-v1.json"
AUTH_FIELDS = {
    "schema", "authorized", "single_use", "targeted_case_count",
    "design_gate_sha256", "implementation_source_aggregate_sha256",
    "source_binding_manifest_sha256", "runner_sha256",
    "billiards_executable_sha256", "cpu_fixture_receipt_sha256",
    "frozen_replay_receipt_sha256", "ready_manifest_sha256",
    "plan_sha256", "output_root", "authorization_nonce",
    "gl1282_minimal_correction_gate_sha256",
    "v4_pairing_correction_gate_sha256",
    "v7_owned_island_correction_gate_sha256",
    "v8_visual_style_correction_gate_sha256",
    "frozen_v8_visual_failure_receipt_sha256",
    "v9_compositing_style_correction_gate_sha256",
    "frozen_v9_compositing_failure_receipt_sha256",
    "v10_machine_contract_correction_gate_sha256",
    "frozen_v10_machine_contract_failure_receipt_sha256",
}


def sha(path: Path) -> str:
    return hashlib.sha256(path.read_bytes()).hexdigest()


def require(condition: bool, message: str) -> None:
    if not condition:
        raise RuntimeError(message)


def load_object(path: Path) -> dict:
    value = json.loads(path.read_text())
    require(isinstance(value, dict), f"JSON object required:{path}")
    return value


def atomic_write_json(path: Path, value: dict) -> None:
    path.parent.mkdir(parents=True, exist_ok=True)
    descriptor, temporary = tempfile.mkstemp(
        prefix=path.name + ".", suffix=".tmp", dir=path.parent)
    try:
        with os.fdopen(descriptor, "w", encoding="utf-8") as stream:
            stream.write(json.dumps(value, indent=2, sort_keys=True) + "\n")
            stream.flush()
            os.fsync(stream.fileno())
        os.replace(temporary, path)
        directory = os.open(path.parent, os.O_RDONLY)
        try:
            os.fsync(directory)
        finally:
            os.close(directory)
    finally:
        if os.path.exists(temporary):
            os.unlink(temporary)


def validate_ready(ready_path: Path, output: Path) -> dict:
    errors = validate_manifest(ready_path)
    require(not errors, "final readiness invalid:" + ",".join(errors))
    ready = load_object(ready_path)
    require(ready.get("schema") ==
            "billiardgl.phase4.final_ownership_targeted11_ready.v11",
            "final ownership v11 readiness required")
    require(ready.get("targeted_output_root") == str(output.resolve()),
            "dispatcher output root differs from readiness")
    require(output.is_dir() and not any(output.iterdir()),
            "dispatcher requires the readiness-bound empty output root")
    return ready


def expected_authorization(ready_path: Path, ready: dict, output: Path) -> dict:
    return {
        "schema": AUTH_SCHEMA,
        "authorized": True,
        "single_use": True,
        "targeted_case_count": 11,
        "design_gate_sha256": ready["gate_bindings"]["design_gate_sha256"],
        "gl1282_minimal_correction_gate_sha256":
            ready["gate_bindings"]["gl1282_minimal_correction_gate_sha256"],
        "v4_pairing_correction_gate_sha256":
            ready["gate_bindings"]["v4_pairing_correction_gate_sha256"],
        "v7_owned_island_correction_gate_sha256":
            ready["gate_bindings"]["v7_owned_island_correction_gate_sha256"],
        "v8_visual_style_correction_gate_sha256":
            ready["gate_bindings"]["v8_visual_style_correction_gate_sha256"],
        "v9_compositing_style_correction_gate_sha256":
            ready["gate_bindings"][
                "v9_compositing_style_correction_gate_sha256"],
        "v10_machine_contract_correction_gate_sha256":
            ready["gate_bindings"][
                "v10_machine_contract_correction_gate_sha256"],
        "implementation_source_aggregate_sha256":
            ready["implementation_source_aggregate_sha256"],
        "source_binding_manifest_sha256": ready["source_binding_manifest_sha256"],
        "runner_sha256": ready["runner_sha256"],
        "billiards_executable_sha256": ready["billiards_executable_sha256"],
        "cpu_fixture_receipt_sha256": ready["cpu_fixture_receipt_sha256"],
        "frozen_replay_receipt_sha256": ready["frozen_replay_receipt_sha256"],
        "frozen_v8_visual_failure_receipt_sha256":
            ready["frozen_v8_visual_failure_receipt_sha256"],
        "frozen_v9_compositing_failure_receipt_sha256":
            ready["frozen_v9_compositing_failure_receipt_sha256"],
        "frozen_v10_machine_contract_failure_receipt_sha256":
            ready["frozen_v10_machine_contract_failure_receipt_sha256"],
        "ready_manifest_sha256": sha(ready_path),
        "plan_sha256": CANONICAL_PLAN_SHA256,
        "output_root": str(output.resolve()),
    }


def validate_authorization(auth_path: Path, ready_path: Path,
                           ready: dict, output: Path) -> tuple[dict, str]:
    receipt_bytes_before = auth_path.read_bytes()
    receipt = load_object(auth_path)
    require(set(receipt) == AUTH_FIELDS, "authorization fields must match exact schema")
    for field, expected in expected_authorization(ready_path, ready, output).items():
        require(receipt.get(field) == expected,
                f"authorization binding drift:{field}")
    nonce = receipt.get("authorization_nonce")
    require(isinstance(nonce, str) and len(nonce) >= 16,
            "authorization nonce missing")
    require(not Path(str(auth_path) + ".consumed").exists(),
            "authorization already consumed")
    return receipt, hashlib.sha256(receipt_bytes_before).hexdigest()


def validate_atomic_consumption(auth_path: Path, nonce: str,
                                auth_sha_before: str) -> None:
    require(sha(auth_path) == auth_sha_before,
            "authorization receipt mutated during dispatch")
    consumed = Path(str(auth_path) + ".consumed")
    require(consumed.is_file() and consumed.read_text() == nonce + "\n",
            "runner did not atomically consume authorization exactly once")


def dispatch(args: argparse.Namespace) -> int:
    ready_path = args.ready.resolve()
    output = args.output_root.resolve()
    ready = validate_ready(ready_path, output)
    if not args.run:
        require(args.authorization_receipt is None,
                "authorization receipt is accepted only with --run")
        print(json.dumps({
            "status": "READY_NOT_RUN_GPU_NOT_AUTHORIZED",
            "capture_count": 11,
            "gpu_or_cocoa_invoked": False,
            "authorization_issued": False,
            "ready_manifest_sha256": sha(ready_path),
        }, indent=2, sort_keys=True))
        return 0

    require(args.authorization_receipt is not None,
            "--run requires separately issued authorization receipt")
    require(args.warmup >= 120 and args.samples >= 300,
            "capture requires >=120 warmup and >=300 samples")
    auth_path = args.authorization_receipt.resolve()
    authorization, auth_sha_before = validate_authorization(
        auth_path, ready_path, ready, output)

    source_plan = Path(ready["plan_path"])
    require(sha(source_plan) == CANONICAL_PLAN_SHA256,
            "readiness plan is not canonical")
    plan = output / PLAN_NAME
    with plan.open("xb") as stream:
        stream.write(source_plan.read_bytes())
        stream.flush()
        os.fsync(stream.fileno())
    require(sha(plan) == CANONICAL_PLAN_SHA256 and
            plan.read_bytes() == source_plan.read_bytes(),
            "written plan differs from exact pinned plan")

    results = output / "results.jsonl"
    environment = os.environ.copy()
    environment.pop(OLD_AUTH_ENV, None)
    environment[AUTH_ENV] = str(auth_path)
    completed = subprocess.run([
        ready["runner_path"], str(plan), str(output), str(results),
        str(args.warmup), str(args.samples),
    ], env=environment, text=True, check=False)
    validate_atomic_consumption(auth_path, authorization["authorization_nonce"],
                                auth_sha_before)
    analysis, errors = analyze_output(ready, output, completed.returncode)

    common = {
        "ready_manifest_path": str(ready_path),
        "ready_manifest_sha256": sha(ready_path),
        "authorization_receipt_path": str(auth_path),
        "authorization_receipt_sha256": auth_sha_before,
        "authorization_nonce": authorization["authorization_nonce"],
        "authorization_consumed_once": True,
        "design_gate_sha256": ready["gate_bindings"]["design_gate_sha256"],
        "implementation_source_aggregate_sha256":
            ready["implementation_source_aggregate_sha256"],
        "source_binding_manifest_sha256": ready["source_binding_manifest_sha256"],
        "runner_sha256": ready["runner_sha256"],
        "billiards_executable_sha256": ready["billiards_executable_sha256"],
        "cpu_fixture_receipt_sha256": ready["cpu_fixture_receipt_sha256"],
        "frozen_replay_receipt_sha256": ready["frozen_replay_receipt_sha256"],
        "plan_path": str(plan),
        "plan_sha256": CANONICAL_PLAN_SHA256,
        "output_root": str(output),
        "runner_exit_code": completed.returncode,
        "capture_count": 11,
        "completed_count": analysis["completed_count"],
        "failed_count": analysis["failed_count"],
        "unrun_count": analysis["unrun_count"],
        "failed_capture_id": analysis["failed_capture_id"],
        "results_path": analysis["results_path"],
        "results_sha256": analysis["results_sha256"],
        "telemetry_path": analysis["telemetry_path"],
        "telemetry_sha256": analysis["telemetry_sha256"],
        "technical_art_review_authorized": False,
        "formal68_authorized": False,
        "validation_errors": errors,
    }
    if completed.returncode == 0 and not errors:
        receipt = {"schema": PASS_SCHEMA,
                   "status": "MACHINE_PASS_PENDING_SEPARATE_TA", **common}
        atomic_write_json(output / PASS_RECEIPT_NAME, receipt)
        print("Phase4 final ownership targeted11 machine capture PASS; TA NOT_RUN")
        return 0

    status = "CAPTURE_FAILED_TELEMETRY_PRESERVED" if \
        analysis["failed_count"] else \
        "CAPTURE_INFRASTRUCTURE_FAILED_NO_ORDINAL_TELEMETRY"
    receipt = {"schema": FAILURE_SCHEMA, "status": status, **common}
    atomic_write_json(output / FAILURE_RECEIPT_NAME, receipt)
    print(json.dumps({"status": status, "runner_exit_code": completed.returncode,
                      "completed_count": analysis["completed_count"],
                      "failed_count": analysis["failed_count"],
                      "unrun_count": analysis["unrun_count"],
                      "validation_errors": errors}, indent=2, sort_keys=True))
    return completed.returncode if completed.returncode != 0 else 1


def main() -> None:
    parser = argparse.ArgumentParser()
    parser.add_argument("--ready", type=Path, required=True)
    parser.add_argument("--output-root", type=Path, required=True)
    parser.add_argument("--authorization-receipt", type=Path)
    parser.add_argument("--warmup", type=int, default=120)
    parser.add_argument("--samples", type=int, default=300)
    parser.add_argument("--run", action="store_true")
    args = parser.parse_args()
    try:
        raise SystemExit(dispatch(args))
    except RuntimeError as error:
        print(f"phase4-final-ownership-dispatch: {error}", file=sys.stderr)
        raise SystemExit(1)


if __name__ == "__main__":
    main()
