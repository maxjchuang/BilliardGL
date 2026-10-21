#!/usr/bin/env python3
"""Create the GPU-free v10i formal six-scene readiness-v3 package."""

from __future__ import annotations

import argparse
import json
from pathlib import Path
import sys
from typing import Any

REPOSITORY_ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(REPOSITORY_ROOT))

from scripts import prepare_phase2_v10i_formal_six_scene_ready_v2 as v2


ROOT = v2.ROOT
V3_ROOT = ROOT / "docs/art/phase2-v10i-formal-six-scene-ready-v3"
DEFAULT_EXECUTABLE = ROOT / "build-phase2-v10i-formal-ready-v2/Billiards"
V2_READINESS = v2.V2_ROOT / "readiness-manifest-v2.json"
V2_READINESS_SHA256 = "8e3bd17da6aba3d3fb56678cf220dcabe0f394cc5b358c6d934a9d6dfadb5a61"
SCENE_IDS = v2.SCENE_IDS
GAP_IDS = v2.GAP_IDS
SOURCE_BINDINGS = tuple(dict.fromkeys(v2.SOURCE_BINDINGS + (
    "scripts/prepare_phase2_v10i_formal_six_scene_ready_v3.py",
    "scripts/validate_phase2_v10i_formal_six_scene_ready_v3.py",
    "tests/visual/test_phase2_v10i_formal_six_scene_ready_v3.py",
)))


class ReadinessV3Error(RuntimeError):
    pass


def require(condition: bool, message: str) -> None:
    if not condition:
        raise ReadinessV3Error(message)


def authorization_template() -> dict[str, Any]:
    return {
        "schema": "phase2_v10i_formal_six_scene_one_shot_authorization_v3",
        "status": "NOT_ISSUED",
        "authorization_id": None,
        "authorized_by": None,
        "authorized_at_utc": None,
        "authorized_output_canonical_path":
            "POPULATE_WITH_ABSOLUTE_ABSENT_CAPTURE_OUTPUT",
        "gpu_capture_authorized": False,
        "formal_six_scene_evidence_authorized": False,
        "authorized_capture_count": 0,
        "consumed_capture_count": 0,
        "scene_ids_in_order": list(SCENE_IDS),
        "required_readiness_manifest_sha256":
            "POPULATE_FROM_FINAL_READINESS_MANIFEST",
        "formal_pass": False,
        "golden_promotion": False,
        "m2_promotion": False,
    }


def false_flags() -> dict[str, bool]:
    return {
        "actual_gpu_used": False,
        "gpu_capture_authorized": False,
        "capture_output_created": False,
        "ta_review_performed": False,
        "product_review_performed": False,
        "formal_six_scene_evidence": False,
        "formal_pass": False,
        "golden_promotion": False,
        "m2_promotion": False,
    }


def prepare(ready_root: Path = V3_ROOT,
            package_root: Path = v2.PACKAGE_ROOT,
            executable: Path = DEFAULT_EXECUTABLE) -> dict[str, Path]:
    require(V2_READINESS.is_file() and
            v2.sha256(V2_READINESS) == V2_READINESS_SHA256,
            "v2 readiness lineage differs")
    package = v2.materialize_formal_asset_package(package_root)
    require(executable.is_file(), "Release executable missing: " + str(executable))
    missing = [path for path in SOURCE_BINDINGS if not (ROOT / path).is_file()]
    require(not missing, "source binding missing: " + ",".join(missing))
    source_bindings = {path: v2.binding(ROOT / path) for path in SOURCE_BINDINGS}
    executable_binding = v2.binding(executable)
    package_binding = v2.binding(package_root / "formal-asset-package-v2.json")
    flags = false_flags()

    gap_evidence = {
        "CPP_READY_ASSET_ATTESTATION": (
            "src/Billiards/render/modern/formal_asset_attestation.cpp",
            "src/Billiards/render/modern/modern_application_mac.mm"),
        "CPP_LOAD_ERROR_CONTRACT": (
            "src/Billiards/render/modern/formal_asset_attestation.cpp",
            "tests/modern_render_foundation_tests.cpp"),
        "PYTHON_EXPLICIT_CHILD_ENV": (
            "tests/e2e/automation_client.py",
            "scripts/capture_visual_baselines.py"),
        "SELF_CONTAINED_FORMAL_ASSET_PACKAGE": (
            "assets/modern/phase2-v10i-formal/formal-asset-package-v2.json",),
        "FORMAL_PERFORMANCE_COLLECTION": (
            "src/Billiards/automation_controller.cpp",
            "src/Billiards/render/modern/modern_application_mac.mm",
            "scripts/capture_visual_baselines.py"),
        "FORMAL_MANIFEST_CONTRACT": (
            "scripts/capture_visual_baselines.py",
            "scripts/validate_phase2_production_visuals.py"),
    }
    gate = {
        "schema": "phase2_v10i_formal_six_scene_implementation_gate_v3",
        "status": "PASS_IMPLEMENTATION_READY_NO_GPU",
        **flags,
        "blocked_manifest_sha256": v2.BLOCKED_MANIFEST_SHA256,
        "v2_drift_finding": {
            "id": "FORMAL_RUNNER_EXPLICIT_MODERN_RENDERER",
            "status": "CLOSED_BY_SOURCE_AND_CPU_TEST",
            "description":
                "formal child process now receives --renderer modern explicitly",
            "evidence": v2.binding(ROOT / "scripts/capture_visual_baselines.py"),
        },
        "gaps": [{
            "id": gap_id,
            "status": "CLOSED_BY_SOURCE_AND_CPU_TEST",
            "evidence": [v2.binding(ROOT / path)
                         for path in gap_evidence[gap_id]],
        } for gap_id in GAP_IDS],
    }
    capture_contract = {
        "fresh_process_per_scene": True,
        "renderer": "modern",
        "automation_extra_args": ["--renderer", "modern"],
        "resolution": {"width": 1920, "height": 1080},
        "quality": "medium",
        "msaa_samples": 4,
        "warmup_frames": 120,
        "sample_frames": 300,
        "measurement_method": "gpu_timer_query_plus_cpu_frame",
        "gpu_timer_queries_observed_required": True,
        "explicit_child_env": {
            "BILLIARDGL_MODERN_PHASE2_ROOT": "<capture_output>/asset_root",
            "BILLIARDGL_FORMAL_CAPTURE": "1",
        },
    }
    plan = {
        "schema": "phase2_v10i_formal_six_scene_plan_v3",
        "status": "NOT_RUN_READY_FOR_ONE_SHOT_AUTHORIZATION",
        **flags,
        "capture_contract": capture_contract,
        "rows": [{
            "ordinal": index + 1,
            "scene_id": scene_id,
            "status": "NOT_RUN",
            "actual_gpu": False,
            "process_started": False,
            "screenshot_created": False,
            "loaded_v10i_sha_attested": False,
            "performance_measured": False,
            "machine_evidence": False,
            "ta_review": False,
            "product_review": False,
            "formal_evidence": False,
            "golden": False,
            "m2": False,
        } for index, scene_id in enumerate(SCENE_IDS)],
    }
    machine = {
        "schema": "phase2_v10i_formal_six_scene_machine_evidence_template_v3",
        "status": "NOT_RUN_READY",
        **flags,
        "formal_manifest_contract": {
            "schema": "phase2_v10i_formal_six_scene_capture_v2",
            "validator": "scripts/validate_phase2_production_visuals.py",
            "asset_root": "asset_root",
            "production_gltf": True,
            "renderer": "modern",
            "quality": "medium",
            "resolution": {"width": 1920, "height": 1080},
            "required_per_scene_attestation": True,
            "required_asset_ids": list(v2.REQUIRED_ASSETS),
            "warmup_frames_min": 120,
            "sample_frames_min": 300,
            "p95_total_frame_ms_max": 16.7,
            "draw_calls_complete_scene_max": 100,
            "texture_resident_mib_max": 256,
            "measurement_method": "gpu_timer_query_plus_cpu_frame",
            "gpu_timer_queries_observed_required": True,
        },
        "scenes": [{
            "scene_id": scene_id,
            "status": "NOT_RUN",
            "screenshot_path": None,
            "screenshot_sha256": None,
            "loaded_asset_attestation": None,
            "performance": None,
            "machine_pass": False,
        } for scene_id in SCENE_IDS],
    }
    authorization = authorization_template()
    files = {
        "implementation_gate": ready_root / "implementation-gate-v3.json",
        "six_scene_plan": ready_root / "six-scene-plan-not-run-v3.json",
        "machine_template": ready_root / "machine-evidence-template-not-run-v3.json",
        "authorization_template":
            ready_root / "one-shot-authorization-template-not-issued-v3.json",
    }
    for key, value in (
            ("implementation_gate", gate), ("six_scene_plan", plan),
            ("machine_template", machine),
            ("authorization_template", authorization)):
        v2.write_immutable(files[key], value)
    generated = {key: v2.binding(path) for key, path in files.items()}
    readiness = {
        "schema": "phase2_v10i_formal_six_scene_readiness_manifest_v3",
        "status": "READY_FOR_ONE_SHOT_AUTHORIZATION_NO_GPU",
        "implementation_ready": True,
        **flags,
        "v2_lineage": v2.binding(V2_READINESS),
        "v2_readiness_manifest_sha256": V2_READINESS_SHA256,
        "blocked_v1_lineage": v2.binding(v2.BLOCKED_MANIFEST),
        "blocked_manifest_sha256": v2.BLOCKED_MANIFEST_SHA256,
        "closed_gap_ids": list(GAP_IDS),
        "closed_v2_drift_ids": ["FORMAL_RUNNER_EXPLICIT_MODERN_RENDERER"],
        "approved_scenes": list(SCENE_IDS),
        "self_contained_asset_package": {
            **package_binding,
            "asset_root": str(package_root.resolve(strict=True)),
            "required_asset_count": package["required_asset_count"],
            "runtime_dependency_count": package["runtime_dependency_count"],
            "frozen_v10i_bytes_unchanged": True,
        },
        "fresh_release_executable": executable_binding,
        "source_bindings": source_bindings,
        "generated_bindings": generated,
        "offline_safety": {
            "can_start_candidate_executable": False,
            "can_create_cocoa_context": False,
            "can_call_opengl": False,
            "can_create_screenshot": False,
            "imports_subprocess": False,
        },
        "remaining_external_gates": [
            "one_shot_gpu_authorization", "six_scene_machine_capture",
            "technical_art_review", "product_review", "golden_decision",
            "m2_promotion_decision"],
    }
    readiness_path = ready_root / "readiness-manifest-v3.json"
    v2.write_immutable(readiness_path, readiness)
    receipt = {
        "schema": "phase2_v10i_formal_six_scene_readiness_receipt_v3",
        "status": readiness["status"],
        **flags,
        "readiness_manifest": v2.binding(readiness_path),
        "fresh_release_executable": executable_binding,
        "self_contained_asset_package": package_binding,
    }
    receipt_path = ready_root / "readiness-receipt-v3.json"
    v2.write_immutable(receipt_path, receipt)
    files["readiness_manifest"] = readiness_path
    files["readiness_receipt"] = receipt_path
    return files


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--ready-root", type=Path, default=V3_ROOT)
    parser.add_argument("--package-root", type=Path, default=v2.PACKAGE_ROOT)
    parser.add_argument("--executable", type=Path, default=DEFAULT_EXECUTABLE)
    arguments = parser.parse_args()
    try:
        files = prepare(arguments.ready_root, arguments.package_root,
                        arguments.executable)
        for path in files.values():
            print(path)
    except (OSError, ValueError, json.JSONDecodeError, ReadinessV3Error,
            v2.ReadinessV2Error) as error:
        print("FAIL: " + str(error))
        return 2
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
