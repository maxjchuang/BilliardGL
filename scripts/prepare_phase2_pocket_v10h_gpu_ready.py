#!/usr/bin/env python3
"""Create deterministic v10h one-shot GPU authorization-readiness evidence."""

import argparse
import json
import os
from pathlib import Path

from generate_phase2_modular_gltf_v10h import ASSET_NAME, MANIFEST_NAME
from run_phase2_pocket_v10h_gpu_preflight import (
    ASSET_ROOT, DEFAULT_AUTHORIZATION, DEFAULT_BUILD, DEFAULT_GPU_FREEZE,
    DEFAULT_OUTPUT, EVIDENCE_SCOPE, EXPECTED, GATE, OFFLINE_FREEZE,
    OFFLINE_REPORT, ROOT, sha256,
)
from validate_phase2_modular_gltf_v10h import validate


DEFAULT_READY_ROOT = ROOT / "docs/art/phase2-pocket-v10h-gpu-ready-v1"
READY_MANIFEST_NAME = "phase2-pocket-v10h-gpu-readiness-manifest-v1.json"
READY_RECEIPT_NAME = "authorization-ready-receipt-v1.json"


def _canonical(value):
    return (json.dumps(value, indent=2, sort_keys=True) + "\n").encode("utf-8")


def _write_immutable(path, payload):
    path.parent.mkdir(parents=True, exist_ok=True)
    if path.exists() and path.read_bytes() != payload:
        raise ValueError("immutable readiness artifact already exists with different bytes: " + str(path))
    path.write_bytes(payload)


def prepare(ready_root=DEFAULT_READY_ROOT, executable=None,
            authorization_path=DEFAULT_AUTHORIZATION, output=DEFAULT_OUTPUT,
            gpu_freeze=DEFAULT_GPU_FREEZE):
    executable = executable or DEFAULT_BUILD / "BilliardsModernRendererSliceSmoke"
    fixed_paths = {
        "gate": GATE, "offline_report": OFFLINE_REPORT, "offline_freeze": OFFLINE_FREEZE,
        "gltf": ASSET_ROOT / ASSET_NAME, "asset_manifest": ASSET_ROOT / MANIFEST_NAME,
        "renderer_source": ROOT / "src/Billiards/render/modern/modern_renderer.cpp",
        "smoke_source": ROOT / "spikes/render_core_profile/modern_renderer_slice_smoke.mm",
    }
    fixed_checks = {name + "_sha_exact": path.is_file() and
                    sha256(path) == "sha256:" + EXPECTED[name]
                    for name, path in fixed_paths.items()}
    if not all(fixed_checks.values()):
        raise ValueError("frozen v10h readiness input SHA mismatch")
    report = json.loads(OFFLINE_REPORT.read_text(encoding="utf-8"))
    receipt = json.loads(OFFLINE_FREEZE.read_text(encoding="utf-8"))
    document = json.loads((ASSET_ROOT / ASSET_NAME).read_text(encoding="utf-8"))
    asset_manifest = json.loads((ASSET_ROOT / MANIFEST_NAME).read_text(encoding="utf-8"))
    validator_errors, validator_report = validate(document, asset_manifest, ASSET_ROOT)
    source_paths = {
        "generator": ROOT / "scripts/generate_phase2_modular_gltf_v10h.py",
        "validator": ROOT / "scripts/validate_phase2_modular_gltf_v10h.py",
        "calibrator": ROOT / "scripts/calibrate_phase2_pocket_v10h.py",
        "offline_freeze_tool": ROOT / "scripts/freeze_phase2_pocket_v10h_offline.py",
        "gpu_runner": ROOT / "scripts/run_phase2_pocket_v10h_gpu_preflight.py",
        "readiness_orchestrator": Path(__file__).resolve(),
        "gpu_runner_tests": ROOT / "tests/visual/test_phase2_pocket_v10h_gpu_runner.py",
        "renderer_executable": executable,
    }
    if not executable.is_file() or not os.access(executable, os.X_OK):
        raise ValueError("fresh v10h renderer executable is missing")
    newest_source = max(fixed_paths["renderer_source"].stat().st_mtime_ns,
                        fixed_paths["smoke_source"].stat().st_mtime_ns)
    checks = dict(fixed_checks)
    checks.update({
        "offline_report_status_pass_gpu_not_authorized":
            report.get("status") == "OFFLINE_PREFLIGHT_PASS_GPU_NOT_AUTHORIZED",
        "offline_report_89_of_89": report.get("machine_check_count") == 89 and
            sum(report.get("machine_checks", {}).values()) == 89 and
            report.get("machine_failures") == [],
        "offline_report_actual_gpu_false": report.get("actual_gpu_used") is False,
        "offline_freeze_10_of_10": receipt.get("binding_count") == 10 and
            receipt.get("all_original_and_snapshot_sha256_match") is True,
        "asset_validator_pass": not validator_errors,
        "fresh_executable_exists_and_executable": executable.is_file() and os.access(executable, os.X_OK),
        "fresh_executable_newer_than_renderer_sources": executable.stat().st_mtime_ns >= newest_source,
        "root_authorization_file_absent": not authorization_path.exists(),
        "gpu_output_single_use_absent": not output.exists(),
        "gpu_freeze_single_use_absent": not gpu_freeze.exists(),
        "actual_gpu_used_false": True,
        "ta_review_authorized_false": True,
        "six_scene_formal_golden_false": True,
    })
    failures = [name for name, passed in checks.items() if not passed]
    if failures:
        raise ValueError("v10h readiness failed: " + "; ".join(failures))
    all_paths = {**fixed_paths, **source_paths}
    bindings = {name: {"path": path.relative_to(ROOT).as_posix(), "sha256": sha256(path)}
                for name, path in all_paths.items()}
    required_auth_bindings = {
        "ready_manifest": "POPULATED_FROM_READY_MANIFEST_SHA256",
        "ready_receipt": "POPULATED_FROM_READY_RECEIPT_SHA256",
        "runner_source": bindings["gpu_runner"]["sha256"],
        "renderer_executable": bindings["renderer_executable"]["sha256"],
        "gate": "sha256:" + EXPECTED["gate"],
        "offline_report": "sha256:" + EXPECTED["offline_report"],
        "offline_freeze": "sha256:" + EXPECTED["offline_freeze"],
        "gltf": "sha256:" + EXPECTED["gltf"],
        "asset_manifest": "sha256:" + EXPECTED["asset_manifest"],
    }
    manifest = {
        "schema": "phase2_pocket_v10h_gpu_readiness_manifest_v1",
        "status": "READY_FOR_ROOT_ONE_SHOT_AUTHORIZATION_GPU_NOT_AUTHORIZED",
        "evidence_scope": "phase2_pocket_v10h_capture_readiness_only",
        "actual_gpu_used": False, "gpu_capture_authorized": False,
        "authorization_file_present": False, "visual_review_status": "NOT_RUN",
        "ta_review_authorized": False, "formal_six_scene_evidence_authorized": False,
        "formal_pass_claimed": False, "golden_promotion_claimed": False,
        "capture_contract_if_separately_authorized": {
            "capture_count": 1, "scene_id": "pocket_closeup", "width": 1920,
            "height": 1080, "quality": "medium", "msaa_samples": 4,
            "evidence_scope": EVIDENCE_SCOPE, "retry_without_new_root_authorization": False,
        },
        "actual_gpu_hard_gate_contract": {
            "rim_luma": [40, 52], "upper_luma": [27, 31], "lower_luma": [8, 11],
            "void_median_luma": [1, 3], "void_p95_max": 4,
            "rim_minus_upper_min": 10, "upper_minus_lower_min": 12,
            "lower_minus_void_min": 5, "strict_ordering": True,
            "neutral_channel_spread_max": 12,
            "inherit_v10g_geometry_visibility_ui_performance_gates": True,
        },
        "fresh_metadata_contract": {
            "machine_status_before_execution": "NOT_RUN",
            "visual_review_status": "NOT_RUN", "ta_review_authorized": False,
            "formal_six_scene_evidence_authorized": False,
            "formal_pass_claimed": False, "golden_promotion_claimed": False,
        },
        "single_use_paths": {"output": output.relative_to(ROOT).as_posix(),
                             "gpu_freeze": gpu_freeze.relative_to(ROOT).as_posix(),
                             "both_absent": True},
        "required_root_authorization_path": authorization_path.relative_to(ROOT).as_posix(),
        "required_root_authorization_schema": {
            "schema": "phase2_pocket_v10h_root_one_shot_gpu_authorization_v1",
            "status": "AUTHORIZED_FOR_EXACTLY_ONE_GPU_CAPTURE",
            "authorized_by": "root", "gpu_capture_authorized": True,
            "authorized_capture_count": 1, "consumed_capture_count": 0,
            "additional_capture_authorized": False,
            "bindings": required_auth_bindings,
            "note": "No authorization file is generated by this readiness package.",
        },
        "bindings": bindings, "validator_report": validator_report,
        "readiness_checks": checks, "readiness_check_count": len(checks),
        "readiness_failures": [],
        "decision": "WAIT_FOR_ROOT_EXPLICIT_ONE_SHOT_AUTHORIZATION; DO_NOT_RUN_GPU",
    }
    manifest_path = ready_root / READY_MANIFEST_NAME
    _write_immutable(manifest_path, _canonical(manifest))
    ready_receipt = {
        "schema": "phase2_pocket_v10h_authorization_ready_receipt_v1",
        "status": "AUTHORIZATION_READY_GPU_NOT_AUTHORIZED",
        "actual_gpu_used": False, "gpu_capture_authorized": False,
        "authorization_file_present": False, "visual_review_status": "NOT_RUN",
        "ta_review_authorized": False, "formal_six_scene_evidence_authorized": False,
        "formal_pass_claimed": False, "golden_promotion_claimed": False,
        "ready_manifest_path": manifest_path.relative_to(ROOT).as_posix(),
        "ready_manifest_sha256": sha256(manifest_path),
        "runner_source_sha256": bindings["gpu_runner"]["sha256"],
        "renderer_executable_sha256": bindings["renderer_executable"]["sha256"],
        "offline_freeze_sha256": "sha256:" + EXPECTED["offline_freeze"],
        "single_use_output_absent": True, "single_use_gpu_freeze_absent": True,
        "decision": "WAIT_FOR_ROOT; MISSING AUTHORIZATION MUST FAIL CLOSED",
    }
    receipt_path = ready_root / READY_RECEIPT_NAME
    _write_immutable(receipt_path, _canonical(ready_receipt))
    return manifest_path, receipt_path, manifest, ready_receipt


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--ready-root", type=Path, default=DEFAULT_READY_ROOT)
    parser.add_argument("--build-dir", type=Path, default=DEFAULT_BUILD)
    args = parser.parse_args()
    executable = args.build_dir.resolve() / "BilliardsModernRendererSliceSmoke"
    manifest, receipt, document, _ = prepare(args.ready_root.resolve(), executable)
    print("v10h GPU authorization readiness PASS; GPU=false")
    print("manifest=" + str(manifest)); print("manifest_sha256=" + sha256(manifest))
    print("receipt=" + str(receipt)); print("receipt_sha256=" + sha256(receipt))
    print("checks=%d/%d" % (sum(document["readiness_checks"].values()),
                            document["readiness_check_count"]))


if __name__ == "__main__":
    try:
        main()
    except (OSError, ValueError, KeyError, json.JSONDecodeError) as error:
        print("phase2 v10h GPU readiness failed: " + str(error))
        raise SystemExit(1)
