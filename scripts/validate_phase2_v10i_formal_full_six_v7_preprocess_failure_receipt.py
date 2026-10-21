#!/usr/bin/env python3
"""Read-only validator for the consumed Phase2 v7 argparse failure."""

from __future__ import annotations

import argparse
import hashlib
import json
import os
from pathlib import Path
import stat

ROOT = Path(__file__).resolve().parents[1]
DEFAULT_RECEIPT = ROOT / "docs/evidence/phase2-v10i-formal-full-six-v7-preprocess-dispatch-failure-receipt-v1.json"
SCENES = ["break", "low_aim", "ball_closeup", "pocket_closeup",
          "tactical_overhead", "post_shot_observe"]


def sha(path: Path) -> str:
    return hashlib.sha256(path.read_bytes()).hexdigest()


def require(condition: bool, message: str) -> None:
    if not condition:
        raise RuntimeError(message)


def bound_file(value: dict, label: str, relative: bool = False) -> Path:
    path = Path(value.get("path", ""))
    if relative:
        path = ROOT / path
    require(path.is_file() and sha(path) == value.get("sha256"),
            label + " path/SHA mismatch")
    return path


def validate(receipt_path: Path = DEFAULT_RECEIPT) -> dict:
    receipt = json.loads(receipt_path.read_text(encoding="utf-8"))
    require(receipt.get("schema") ==
            "phase2_v10i_formal_full_six_v7_preprocess_dispatch_failure_receipt_v1" and
            receipt.get("status") ==
            "AUTHORIZATION_CONSUMED_CAPTURE_DRIVER_ARGPARSE_FAILED_BEFORE_OUTPUT_OR_CANDIDATE_PROCESS" and
            receipt.get("failure_class") ==
            "DISPATCHER_TO_CAPTURE_DRIVER_CLI_CONTRACT_MISMATCH",
            "failure receipt identity mismatch")
    auth_value = receipt.get("authorization", {})
    marker_value = receipt.get("consumed_marker", {})
    auth = bound_file(auth_value, "authorization")
    marker = bound_file(marker_value, "consumed marker")
    require(auth_value.get("sha256") ==
            "8f1f828899ff172d23e1ff88d6e3b492ca2e1685ccfa49d1608b5c00a85a6f12" and
            marker_value.get("sha256") ==
            "5fbddc9ee74765712d3587a5a8b6ee539bcc5d1262cd154e4757f0ba84cdfbb1",
            "frozen auth/marker SHA mismatch")
    for path, value, label in ((auth, auth_value, "authorization"),
                               (marker, marker_value, "marker")):
        info = path.stat()
        require(stat.S_IMODE(info.st_mode) == 0o444 and value.get("mode") == "0444" and
                info.st_uid == value.get("uid") == 501 and
                info.st_gid == value.get("gid") == 20,
                label + " permission/owner mismatch")
    auth_json = json.loads(auth.read_text())
    marker_json = json.loads(marker.read_text())
    require(auth_json.get("authorization_id") == marker_json.get("authorization_id") ==
            auth_value.get("authorization_id") and
            auth_json.get("authorization_nonce") == marker_json.get("authorization_nonce") ==
            auth_value.get("authorization_nonce") and
            marker_json.get("authorization_sha256") == sha(auth) and
            marker_json.get("consumed_capture_count") == 6 and
            marker_value.get("durable_consumption") is True and
            marker_value.get("must_never_be_deleted_or_reused") is True and
            auth_value.get("authorization_reuse_forbidden") is True,
            "consumption identity/permanence mismatch")
    readiness = bound_file(receipt.get("readiness", {}), "v7 readiness")
    require(sha(readiness) ==
            "669dbf75734e231a24b8d702f28c6e80b2ce925331d3134e2c7a20302ad6f383",
            "frozen v7 readiness mismatch")
    runtime = receipt.get("runtime_contract", {})
    wrapper = bound_file({"path": runtime.get("v7_wrapper_path"),
                          "sha256": runtime.get("v7_wrapper_sha256")},
                         "v7 wrapper", True)
    delegated = bound_file({"path": runtime.get("delegated_v6_dispatcher_path"),
                            "sha256": runtime.get("delegated_v6_dispatcher_sha256")},
                           "delegated v6 dispatcher", True)
    capture = bound_file({"path": runtime.get("capture_driver_path"),
                          "sha256": runtime.get("capture_driver_sha256")},
                         "capture driver", True)
    delegated_source = delegated.read_text(encoding="utf-8")
    capture_source = capture.read_text(encoding="utf-8")
    require(runtime.get("dispatcher_emitted_option") == "--formal-asset-package" and
            runtime.get("capture_driver_required_option") == "--asset-package" and
            runtime.get("argparse_exit_code") == 2 and
            '"--formal-asset-package"' in delegated_source and
            'add_argument("--asset-package"' in capture_source and
            'add_argument("--formal-asset-package"' not in capture_source,
            "frozen CLI mismatch root cause invalid")
    boundary = receipt.get("execution_boundary", {})
    output = Path(boundary.get("output_root", ""))
    require(boundary == {
        "wrapper_process_started": True,
        "capture_driver_python_process_started": True,
        "candidate_billiards_process_started": False,
        "output_root": str(output), "output_root_exists": False,
        "cocoa_invoked": False, "opengl_invoked": False,
        "gpu_invoked": False, "screenshot_created": False,
    } and output.is_absolute() and not output.exists(),
            "pre-candidate execution boundary mismatch")
    require(receipt.get("scene_results") == [
        {"ordinal": i + 1, "scene_id": scene, "status": "NOT_RUN"}
        for i, scene in enumerate(SCENES)], "six NOT_RUN rows mismatch")
    require(receipt.get("claims") == {
        "machine_pass": False, "formal_six_scene_evidence": False,
        "technical_art_review": False, "formal_pass": False,
        "golden_promotion": False, "m2_promotion": False,
    }, "failure receipt claim boundary mismatch")
    return {"status": "PASS_FROZEN_PHASE2_V7_PREPROCESS_DISPATCH_FAILURE",
            "authorization_sha256": sha(auth), "consumed_marker_sha256": sha(marker),
            "not_run_count": 6, "candidate_process_started": False,
            "gpu_invoked": False}


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("receipt", type=Path, nargs="?", default=DEFAULT_RECEIPT)
    try:
        print(json.dumps(validate(parser.parse_args().receipt), sort_keys=True))
    except (OSError, ValueError, json.JSONDecodeError, RuntimeError) as error:
        print("FAIL: " + str(error))
        return 2
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
