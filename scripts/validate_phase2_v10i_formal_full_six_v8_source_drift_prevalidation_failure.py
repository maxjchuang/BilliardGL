#!/usr/bin/env python3
"""Validate frozen v8 no-marker source-drift prevalidation rejection."""

from __future__ import annotations
import hashlib, json, stat
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
RECEIPT = ROOT / "docs/evidence/phase2-v10i-formal-full-six-v8-source-drift-prevalidation-failure-receipt-v1.json"
SCENES = ["break", "low_aim", "ball_closeup", "pocket_closeup",
          "tactical_overhead", "post_shot_observe"]

def sha(path: Path) -> str: return hashlib.sha256(path.read_bytes()).hexdigest()
def require(value: bool, message: str) -> None:
    if not value: raise RuntimeError(message)

def validate(path: Path = RECEIPT) -> dict:
    value = json.loads(path.read_text())
    require(value.get("schema") ==
            "phase2_v10i_formal_full_six_v8_source_drift_prevalidation_failure_receipt_v1" and
            value.get("status") ==
            "AUTHORIZATION_REVOKED_PREVALIDATION_SOURCE_DRIFT_NO_CONSUMPTION_NO_PROCESS",
            "v8 prevalidation receipt identity")
    auth = value["authorization"]; auth_path = Path(auth["path"])
    require(auth_path.is_file() and sha(auth_path) == auth["sha256"] ==
            "14be0745f017c67b7187e89efdfe9aafdbcd32697f5bd8776b014033637ddc9d" and
            stat.S_IMODE(auth_path.stat().st_mode) == 0o444 and auth["mode"] == "0444" and
            auth["root_policy_revoked"] is True and
            auth["authorization_reuse_forbidden"] is True,
            "v8 revoked auth binding")
    readiness = value["readiness"]; ready_path = Path(readiness["path"])
    require(ready_path.is_file() and sha(ready_path) == readiness["sha256"] ==
            "99076bad210a31f64a3bf96d227e1c2dcf528a9b80bd2f0b6275e90819417fc3",
            "v8 readiness binding")
    ready = json.loads(ready_path.read_text()); drift = value["source_drift"]
    source = Path(drift["path"])
    require(ready["source_bindings"]["CMakeLists.txt"]["sha256"] ==
            drift["readiness_expected_sha256"] ==
            "acf6abcb7f6ae9bc177f872a4ec11d052f5fa81757d9b58f86c406d52a628835" and
            source.is_file() and sha(source) == drift["prevalidation_live_sha256"] and
            drift["prevalidation_live_sha256"] != drift["readiness_expected_sha256"],
            "v8 CMake source drift proof")
    boundary = value["execution_boundary"]; output = Path(boundary["output_root"])
    marker = Path(str(auth_path) + ".consumed.json")
    require(boundary["prevalidation_rejected"] is True and
            all(boundary[field] is False for field in (
                "wrapper_executed", "consumed_marker_created",
                "candidate_process_started", "output_root_exists", "cocoa_invoked",
                "opengl_invoked", "gpu_invoked", "screenshot_created")) and
            not marker.exists() and not output.exists(), "v8 zero-side-effect boundary")
    require(value["scene_results"] == [
        {"ordinal": i + 1, "scene_id": scene, "status": "NOT_RUN"}
        for i, scene in enumerate(SCENES)], "v8 six NOT_RUN")
    require(all(flag is False for flag in value["claims"].values()),
            "v8 failure claims promoted")
    return {"status": "PASS_FROZEN_PHASE2_V8_SOURCE_DRIFT_PREVALIDATION_FAILURE",
            "authorization_sha256": sha(auth_path), "marker_created": False,
            "process_started": False, "gpu_invoked": False, "not_run_count": 6}

if __name__ == "__main__":
    try: print(json.dumps(validate(), sort_keys=True))
    except (OSError, ValueError, KeyError, json.JSONDecodeError, RuntimeError) as error:
        print("FAIL: " + str(error)); raise SystemExit(2)
