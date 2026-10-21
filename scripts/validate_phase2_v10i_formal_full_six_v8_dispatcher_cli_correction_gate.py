#!/usr/bin/env python3
"""CPU-only validator for the Phase2 v8 dispatcher CLI correction gate."""

from __future__ import annotations

import argparse
import hashlib
import json
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
GATE = ROOT / "docs/art/phase2-v10i-formal-full-six-v8-dispatcher-cli-correction-gate-v1.json"


def sha(path: Path) -> str:
    return hashlib.sha256(path.read_bytes()).hexdigest()


def require(value: bool, message: str) -> None:
    if not value:
        raise RuntimeError(message)


def validate(path: Path = GATE) -> dict:
    gate = json.loads(path.read_text())
    require(gate.get("schema") ==
            "phase2_v10i_formal_full_six_v8_dispatcher_cli_correction_gate_v1" and
            gate.get("status") == "APPROVED_MINIMAL_DISPATCHER_ONLY_CORRECTION_NO_GPU",
            "v8 correction gate identity mismatch")
    receipt = gate.get("frozen_v7_failure_receipt", {})
    receipt_path = ROOT / receipt.get("path", "")
    require(receipt_path.is_file() and sha(receipt_path) == receipt.get("sha256") ==
            "a50e19521543162b15e7871b39526dbaa38afae2a36b8e2cf906a3fbf53ad129",
            "v7 failure receipt binding mismatch")
    cause = gate.get("root_cause", {})
    require(cause == {
        "delegated_v6_dispatcher_emitted": "--formal-asset-package",
        "capture_driver_parser_accepts": "--asset-package",
        "failure_stage": "capture_driver_argparse_before_output_or_candidate_process",
    }, "v8 root cause mismatch")
    allowed = gate.get("allowed_changes", [])
    forbidden = gate.get("forbidden_changes", [])
    require(any("replace exactly" in item for item in allowed) and
            "scripts/capture_visual_baselines.py" in forbidden and
            "Billiards binary or production source" in forbidden and
            "v7 authorization or consumed marker" in forbidden,
            "v8 correction scope mismatch")
    required = gate.get("required_test", {})
    require(required.get("kind") == "real_argparse_contract_without_capture_execution" and
            required.get("expected_asset_package_option") == "--asset-package" and
            required.get("forbidden_option") == "--formal-asset-package",
            "v8 real parser test gate mismatch")
    require(gate.get("required_v8_boundary") == {
        "scene_count": 6, "all_scenes_not_run": True,
        "authorization_issued": False, "output_root_absent": True,
        "gpu_executed": False,
    }, "v8 evidence boundary mismatch")
    return {"status": "PASS_PHASE2_V8_DISPATCHER_CLI_CORRECTION_GATE",
            "gate_sha256": sha(path), "gpu_executed": False}


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("gate", type=Path, nargs="?", default=GATE)
    try:
        print(json.dumps(validate(parser.parse_args().gate), sort_keys=True))
    except (OSError, ValueError, json.JSONDecodeError, RuntimeError) as error:
        print("FAIL: " + str(error))
        return 2
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
