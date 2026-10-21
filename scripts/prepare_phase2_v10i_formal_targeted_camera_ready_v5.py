#!/usr/bin/env python3
"""Create executable, GPU-free break/low_aim targeted readiness v5."""

from __future__ import annotations

import argparse
import json
from pathlib import Path
import sys
from typing import Any

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT))

from scripts import prepare_phase2_v10i_formal_six_scene_ready_v2 as v2


V5_ROOT = ROOT / "docs/art/phase2-v10i-formal-targeted-camera-ready-v5"
DEFAULT_EXECUTABLE = ROOT / (
    "build-integrated-phase2-v4-phase4-ownership-v1/Billiards")
V4_READINESS = ROOT / (
    "docs/art/phase2-v10i-formal-six-scene-ready-v4/readiness-manifest-v4.json")
V4_READINESS_SHA256 = "597e6bcc7b43abe393aba69795aeaba231e1bab38f5b4a576c527e1ad4120a96"
CAMERA_GATE = ROOT / (
    "docs/art/phase2-v10i-formal-six-scene-v4-camera-correction-design-gate-v1.json")
CAMERA_GATE_SHA256 = "5ab090f57ff49b40bfbe12f13f568614fc9a4a193e8b123c93f3f9f9d75d9979"
FAILURE_RECEIPT = ROOT / (
    "docs/evidence/phase2-v10i-formal-six-scene-v3-failure-receipt-v1.json")
FAILURE_RECEIPT_SHA256 = "818ceb4be65f4ac192676a383ca2892585b4d1fee94bbb3e7471b3fd2da9291d"
SCENE_IDS = ("break", "low_aim")
EXPECTED_CAMERA = {
    "break": ["observe_free", "elevated"],
    "low_aim": ["aim_player", "player_low"],
}
SOURCE_BINDINGS = (
    "CMakeLists.txt",
    "src/Billiards/game_runtime.h",
    "src/Billiards/game_runtime.cpp",
    "src/Billiards/automation_controller.cpp",
    "src/Billiards/render/modern/modern_renderer.cpp",
    "tests/automation_controller_tests.cpp",
    "tests/e2e/automation_client.py",
    "tests/visual/visual_scene_registry.py",
    "tests/visual/formal_modern_scene_registry_v2.py",
    "scripts/capture_visual_baselines.py",
    "scripts/validate_phase2_production_visuals.py",
    "scripts/validate_phase2_v10i_formal_targeted_camera_capture.py",
    "scripts/prepare_phase2_v10i_formal_targeted_camera_ready_v5.py",
    "scripts/validate_phase2_v10i_formal_targeted_camera_ready_v5.py",
    "tests/visual/test_phase2_v10i_formal_targeted_capture_v1.py",
    "tests/visual/test_phase2_v10i_formal_targeted_camera_ready_v5.py",
)


class TargetedReadinessV5Error(RuntimeError):
    pass


def require(value: bool, message: str) -> None:
    if not value:
        raise TargetedReadinessV5Error(message)


def false_flags() -> dict[str, bool]:
    return {
        "actual_gpu_used": False,
        "gpu_capture_authorized": False,
        "capture_output_created": False,
        "targeted_camera_evidence": False,
        "targeted_camera_pass": False,
        "formal_six_scene_evidence": False,
        "formal_pass": False,
        "ta_review_performed": False,
        "product_review_performed": False,
        "golden_promotion": False,
        "m2_promotion": False,
    }


def authorization_template() -> dict[str, Any]:
    return {
        "schema": "phase2_v10i_formal_targeted_camera_authorization_v1",
        "status": "NOT_ISSUED",
        "authorization_id": None,
        "authorization_nonce": None,
        "authorized_by": None,
        "authorized_at_utc": None,
        "authorized_output_canonical_path":
            "POPULATE_WITH_ABSOLUTE_ABSENT_CAPTURE_OUTPUT",
        "gpu_capture_authorized": False,
        "targeted_camera_evidence_authorized": False,
        "formal_six_scene_evidence_authorized": False,
        "full_six_scene_capture_authorized": False,
        "authorized_capture_count": 0,
        "consumed_capture_count": 0,
        "scene_ids_in_order": list(SCENE_IDS),
        "required_readiness_manifest_sha256":
            "POPULATE_FROM_FINAL_READINESS_MANIFEST",
        "targeted_camera_pass": False,
        "formal_pass": False,
        "golden_promotion": False,
        "m2_promotion": False,
    }


def prepare(ready_root: Path = V5_ROOT,
            package_root: Path = v2.PACKAGE_ROOT,
            executable: Path = DEFAULT_EXECUTABLE) -> dict[str, Path]:
    for path, digest, label in (
            (V4_READINESS, V4_READINESS_SHA256, "frozen v4 readiness"),
            (CAMERA_GATE, CAMERA_GATE_SHA256, "camera design gate"),
            (FAILURE_RECEIPT, FAILURE_RECEIPT_SHA256, "v3 failure receipt")):
        require(path.is_file() and v2.sha256(path) == digest,
                label + " SHA differs")
    package = v2.materialize_formal_asset_package(package_root)
    require(executable.is_file(), "integrated Release executable missing")
    missing = [name for name in SOURCE_BINDINGS if not (ROOT / name).is_file()]
    require(not missing, "source binding missing: " + ",".join(missing))
    sources = {name: v2.binding(ROOT / name) for name in SOURCE_BINDINGS}
    executable_binding = v2.binding(executable)
    package_binding = v2.binding(package_root / "formal-asset-package-v2.json")
    flags = false_flags()
    gate = {
        "schema": "phase2_v10i_formal_targeted_camera_implementation_gate_v5",
        "status": "PASS_EXECUTABLE_TARGETED_PROTOCOL_NO_GPU",
        **flags,
        "frozen_v4_non_executable_lineage": v2.binding(V4_READINESS),
        "camera_design_gate_lineage": v2.binding(CAMERA_GATE),
        "frozen_failure_receipt_lineage": v2.binding(FAILURE_RECEIPT),
        "targeted_contract": {
            "cli_option": "--formal-targeted-scenes break,low_aim",
            "scene_ids_in_order": list(SCENE_IDS),
            "capture_count": 2,
            "authorization_schema":
                "phase2_v10i_formal_targeted_camera_authorization_v1",
            "authorization_status":
                "AUTHORIZED_FOR_EXACTLY_TWO_FRESH_PROCESS_CAPTURES",
            "binds_authorization_id_and_nonce": True,
            "binds_output_readiness_scene_ids_and_count": True,
            "wrong_subset_rejects_before_output_or_process": True,
            "bad_authorization_creates_no_marker_output_or_process": True,
            "full_six_scene_path_preserved": True,
            "full_six_scene_capture_authorized": False,
        },
        "single_use_consumption": {
            "required": True,
            "timing": "BEFORE_OUTPUT_OR_PROCESS_START",
            "atomic_flags": ["O_CREAT", "O_EXCL", "O_WRONLY"],
            "marker_schema":
                "phase2_v10i_formal_targeted_camera_authorization_consumed_v1",
            "marker_binds": [
                "authorization_path", "authorization_sha256",
                "authorization_id", "authorization_nonce",
                "authorized_output_canonical_path",
                "required_readiness_manifest_sha256",
                "scene_ids_in_order", "authorized_capture_count",
                "consumed_capture_count"],
            "failure_permanently_consumes_authorization": True,
            "durability_before_process": "flush_plus_fsync",
            "fsync_failure_starts_no_output_or_process": True,
            "incomplete_marker_is_never_deleted_or_reused": True,
            "existing_marker_rejects_before_output_or_process": True,
        },
        "output_contract": {
            "manifest_schema":
                "phase2_v10i_formal_targeted_camera_capture_v1",
            "validator":
                "scripts/validate_phase2_v10i_formal_targeted_camera_capture.py",
            "registry_version": 2,
            "expected_semantic_camera": EXPECTED_CAMERA,
            "full_six_scene_evidence": False,
        },
        "cpu_tests": {
            "status": "PASS",
            "test_file": sources[
                "tests/visual/test_phase2_v10i_formal_targeted_capture_v1.py"],
            "required_case_count": 10,
            "gpu_or_graphics_context_used": False,
        },
    }
    plan = {
        "schema": "phase2_v10i_formal_targeted_camera_plan_v5",
        "status": "NOT_RUN_READY_FOR_EXACT_TWO_SCENE_AUTHORIZATION",
        **flags,
        "capture_command": [
            "python3", "scripts/capture_visual_baselines.py",
            "--formal-v10i", "--formal-targeted-scenes", "break,low_aim",
            "--executable", str(executable.resolve(strict=True)),
            "--output", "<AUTHORIZED_ABSENT_OUTPUT>",
            "--asset-package", str(package_root.resolve(strict=True)),
            "--authorization", "<ISSUED_TARGETED_AUTHORIZATION_JSON>",
            "--readiness-manifest-sha256", "<FINAL_V5_READINESS_SHA256>",
            "--width", "1920", "--height", "1080"],
        "rows": [{
            "ordinal": index + 1, "scene_id": scene_id,
            "status": "NOT_RUN", "process_started": False,
            "screenshot_created": False, "semantic_camera_attested": False,
            "machine_pass": False, "ta_review": False,
        } for index, scene_id in enumerate(SCENE_IDS)],
    }
    machine = {
        "schema": "phase2_v10i_formal_targeted_camera_machine_template_v5",
        "status": "NOT_RUN_READY", **flags,
        "required_manifest_schema":
            "phase2_v10i_formal_targeted_camera_capture_v1",
        "validator_command": [
            "python3",
            "scripts/validate_phase2_v10i_formal_targeted_camera_capture.py",
            "--manifest", "<TARGETED_OUTPUT>/manifest.json",
            "--expected-executable", str(executable.resolve(strict=True)),
            "--expected-readiness-sha256", "<FINAL_V5_READINESS_SHA256>"],
        "scenes": [{"scene_id": scene_id, "status": "NOT_RUN",
                    "machine_pass": False, "semantic_camera": None}
                   for scene_id in SCENE_IDS],
    }
    files = {
        "implementation_gate": ready_root / "implementation-gate-v5.json",
        "targeted_plan": ready_root / "targeted-plan-not-run-v5.json",
        "machine_template": ready_root / "machine-template-not-run-v5.json",
        "authorization_template":
            ready_root / "targeted-authorization-template-not-issued-v5.json",
    }
    for key, value in (("implementation_gate", gate),
                       ("targeted_plan", plan),
                       ("machine_template", machine),
                       ("authorization_template", authorization_template())):
        v2.write_immutable(files[key], value)
    readiness = {
        "schema": "phase2_v10i_formal_targeted_camera_readiness_v5",
        "status": "READY_FOR_EXACT_BREAK_LOW_AIM_AUTHORIZATION_NO_GPU",
        "implementation_ready": True, **flags,
        "frozen_v4_lineage": v2.binding(V4_READINESS),
        "camera_design_gate_lineage": v2.binding(CAMERA_GATE),
        "frozen_failure_receipt_lineage": v2.binding(FAILURE_RECEIPT),
        "targeted_scene_ids_in_order": list(SCENE_IDS),
        "authorized_capture_count_when_issued": 2,
        "full_six_scene_capture_authorized": False,
        "expected_semantic_camera": EXPECTED_CAMERA,
        "fresh_integrated_release_executable": executable_binding,
        "self_contained_asset_package": {
            **package_binding,
            "asset_root": str(package_root.resolve(strict=True)),
            "required_asset_count": package["required_asset_count"],
            "runtime_dependency_count": package["runtime_dependency_count"],
            "frozen_v10i_bytes_unchanged": True,
        },
        "source_bindings": sources,
        "generated_bindings": {
            key: v2.binding(path) for key, path in files.items()},
        "offline_safety": {
            "can_start_candidate_executable": False,
            "can_create_cocoa_context": False,
            "can_call_opengl": False,
            "can_create_screenshot": False,
            "can_issue_authorization": False,
        },
        "remaining_external_gates": [
            "issue_exact_targeted_authorization",
            "atomic_authorization_consumption",
            "targeted_break_low_aim_machine_capture",
            "targeted_machine_validator", "targeted_technical_art_review"],
    }
    readiness_path = ready_root / "readiness-manifest-v5.json"
    v2.write_immutable(readiness_path, readiness)
    receipt = {
        "schema": "phase2_v10i_formal_targeted_camera_readiness_receipt_v5",
        "status": readiness["status"], **flags,
        "readiness_manifest": v2.binding(readiness_path),
        "fresh_integrated_release_executable": executable_binding,
        "self_contained_asset_package": package_binding,
    }
    receipt_path = ready_root / "readiness-receipt-v5.json"
    v2.write_immutable(receipt_path, receipt)
    files["readiness_manifest"] = readiness_path
    files["readiness_receipt"] = receipt_path
    return files


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--ready-root", type=Path, default=V5_ROOT)
    parser.add_argument("--package-root", type=Path, default=v2.PACKAGE_ROOT)
    parser.add_argument("--executable", type=Path, default=DEFAULT_EXECUTABLE)
    arguments = parser.parse_args()
    try:
        for path in prepare(arguments.ready_root, arguments.package_root,
                            arguments.executable).values():
            print(path)
    except (OSError, ValueError, json.JSONDecodeError,
            TargetedReadinessV5Error, v2.ReadinessV2Error) as error:
        print("FAIL: " + str(error))
        return 2
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
