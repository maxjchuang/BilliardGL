#!/usr/bin/env python3
"""Prepare GPU-free Phase2 v10i exact full-six reentry readiness v6."""

from __future__ import annotations

import argparse
import json
from pathlib import Path
import sys
from typing import Any

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT))

from scripts import prepare_phase2_v10i_formal_six_scene_ready_v2 as base
from tests.visual.formal_modern_scene_registry_v2 import (
    EXPECTED_CAMERA as REGISTRY_CAMERA,
    FORMAL_MODERN_VISUAL_SCENES,
    REGISTRY_VERSION,
)


READY_ROOT = ROOT / "docs/art/phase2-v10i-formal-full-six-reentry-ready-v6"
DEFAULT_EXECUTABLE = ROOT / "build-integrated-phase2-full-six-reentry-v6-phase4-v7/Billiards"
REENTRY_GATE = ROOT / "docs/art/phase2-v10i-formal-full-six-reentry-acceptance-design-gate-v1.json"
REENTRY_GATE_SHA256 = "40fcca249cf1a307833a75972428bbcdae5bb642cef1f5d7637b96deb9bc3fa4"
TARGETED_MACHINE = ROOT / "docs/evidence/phase2-v10i-formal-targeted-camera-v5-machine-pass-receipt-v1.json"
TARGETED_MACHINE_SHA256 = "f26c82883704b01165dbb325509aa1cc6bf72e4b655a9229860751c036dc7db9"
TARGETED_TA = ROOT / "docs/evidence/phase2-v10i-formal-targeted-camera-v5-independent-ta-manual-review-v1.json"
TARGETED_TA_SHA256 = "79f0efcf430362c41abc4abbec1dd075c9d52a551bc4969d6be44e7764cf2931"
TA_TEMPLATE = ROOT / "docs/art/phase2-v10i-formal-full-six-ta-review-template-v1.json"
TA_TEMPLATE_SHA256 = "6adc89b2b9030426287f432edfa3c33a6431af545cbaedae80a446c2b9e81f49"
PACKAGE_MANIFEST_SHA256 = "610b1c49dcc5f798a1a40ee90f9312e5a1a15f4c5c648276c172cfd87591e37d"
SCENE_IDS = tuple(scene.scene_id for scene in FORMAL_MODERN_VISUAL_SCENES)
EXPECTED_CAMERA = {key: [value[0], value[1]] for key, value in REGISTRY_CAMERA.items()}

SOURCE_BINDINGS = (
    "CMakeLists.txt",
    "src/Billiards/game_runtime.h",
    "src/Billiards/game_runtime.cpp",
    "src/Billiards/automation_controller.h",
    "src/Billiards/automation_controller.cpp",
    "src/Billiards/automation_runner.h",
    "src/Billiards/automation_runner.cpp",
    "src/Billiards/camera/camera_state_machine.h",
    "src/Billiards/camera/camera_state_machine.cpp",
    "src/Billiards/render/modern/camera_projection.h",
    "src/Billiards/render/modern/formal_asset_attestation.cpp",
    "src/Billiards/render/modern/modern_application_mac.mm",
    "src/Billiards/render/modern/modern_renderer.cpp",
    "src/Billiards/training/phase4_final_ownership_instrumentation.h",
    "src/Billiards/training/phase4_final_ownership_instrumentation.cpp",
    "src/Billiards/training/trajectory_overlay_renderer.cpp",
    "tests/e2e/automation_client.py",
    "tests/visual/visual_scene_registry.py",
    "tests/visual/formal_modern_scene_registry_v2.py",
    "scripts/capture_visual_baselines.py",
    "scripts/validate_phase2_production_visuals.py",
    "scripts/run_phase2_v10i_formal_full_six_capture_v6.py",
    "scripts/prepare_phase2_v10i_formal_full_six_reentry_ready_v6.py",
    "scripts/validate_phase2_v10i_formal_full_six_reentry_ready_v6.py",
    "tests/automation_controller_tests.cpp",
    "tests/camera_shot_model_tests.cpp",
    "tests/modern_render_foundation_tests.cpp",
    "tests/visual/test_phase2_v10i_formal_modern_registry_v2.py",
    "tests/visual/test_phase2_v10i_formal_full_six_wrapper_v6.py",
    "tests/visual/test_phase2_v10i_formal_full_six_reentry_ready_v6.py",
)


class ReadinessError(RuntimeError):
    pass


def require(condition: bool, message: str) -> None:
    if not condition:
        raise ReadinessError(message)


def false_flags() -> dict[str, bool]:
    return {
        "actual_gpu_used": False,
        "gpu_capture_authorized": False,
        "capture_output_created": False,
        "formal_six_scene_evidence": False,
        "formal_pass": False,
        "ta_review_performed": False,
        "product_review_performed": False,
        "golden_promotion": False,
        "m2_promotion": False,
    }


def recipe(scene: Any) -> list[dict[str, Any]]:
    return [{"command": item.name, "params": dict(item.params)}
            for item in scene.commands]


def region(scene: Any) -> dict[str, Any]:
    item = scene.interest_regions[0]
    return {"name": item[0], "rect_normalized": list(item[1:])}


def validate_frozen_package(package_root: Path) -> dict[str, Any]:
    manifest_path = package_root / "formal-asset-package-v2.json"
    require(manifest_path.is_file() and base.sha256(manifest_path) ==
            PACKAGE_MANIFEST_SHA256, "formal package manifest SHA differs")
    manifest = json.loads(manifest_path.read_text(encoding="utf-8"))
    require(manifest.get("schema") ==
            "phase2_v10i_self_contained_formal_asset_package_v2" and
            manifest.get("immutable") is True and
            manifest.get("self_contained") is True and
            manifest.get("symlinks_allowed") is False and
            manifest.get("required_asset_count") == 6 and
            manifest.get("runtime_dependency_count") == 18 and
            len(manifest.get("files", [])) == 24,
            "formal package contract differs")
    for item in manifest["files"]:
        path = package_root / item["filename"]
        require(path.is_file() and not path.is_symlink() and
                base.sha256(path) == item.get("source_sha256") ==
                item.get("packaged_sha256"),
                "formal package file differs: " + item["filename"])
    return manifest


def authorization_template() -> dict[str, Any]:
    return {
        "schema": "phase2_v10i_formal_six_scene_one_shot_authorization_v3",
        "status": "NOT_ISSUED",
        "authorization_id": None,
        "authorization_nonce": None,
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


def capture_contract() -> dict[str, Any]:
    return {
        "fresh_process_per_scene": True,
        "renderer": "modern",
        "quality": "medium",
        "msaa_samples": 4,
        "resolution": {"width": 1920, "height": 1080},
        "pixel_format": "PPM P6 RGB8",
        "formal_registry_version": REGISTRY_VERSION,
        "expected_semantic_camera": EXPECTED_CAMERA,
        "pre_capture_camera_query": "get_presentation_state",
        "query_timing": "AFTER_COMPLETE_RECIPE_BEFORE_PERFORMANCE_AND_SCREENSHOT",
        "transitioning_required": False,
        "finite_pose_projection_required": True,
        "lamp_safety_required_except_tactical_overhead": True,
        "visible_pixel_channel_rule": "max_rgb_channel_gt_8",
        "minimum_whole_frame_visible_pixels": 1,
        "minimum_interest_region_visible_pixels": 1,
        "performance": {
            "measurement_method": "gpu_timer_query_plus_cpu_frame",
            "gpu_timer_queries_observed": True,
            "warmup_frames_min": 120,
            "sample_frames_min": 300,
            "p95_total_frame_ms_max": 16.7,
            "draw_calls_complete_scene_max": 100,
            "texture_resident_mib_max": 256,
        },
        "asset_attestation": {
            "self_contained_capture_asset_root_required": True,
            "required_asset_binding_count": 6,
            "runtime_dependency_sha_verification_required": True,
            "silent_material_fallback_allowed": False,
            "load_errors_allowed": False,
            "renderer_load_checks_required": [
                "pbr_loaded", "pbr_texture_count_22", "environment_enabled",
                "gltf_scene_loaded", "venue_scene_loaded", "medium_quality",
                "msaa_4x"],
        },
    }


def prepare(ready_root: Path = READY_ROOT,
            package_root: Path = base.PACKAGE_ROOT,
            executable: Path = DEFAULT_EXECUTABLE) -> dict[str, Path]:
    for path, digest, label in (
            (REENTRY_GATE, REENTRY_GATE_SHA256, "reentry gate"),
            (TARGETED_MACHINE, TARGETED_MACHINE_SHA256, "targeted machine receipt"),
            (TARGETED_TA, TARGETED_TA_SHA256, "targeted TA review"),
            (TA_TEMPLATE, TA_TEMPLATE_SHA256, "full-six TA template")):
        require(path.is_file() and base.sha256(path) == digest,
                label + " SHA differs")
    package = validate_frozen_package(package_root.resolve(strict=True))
    require(executable.is_file(), "fresh integrated Release executable missing")
    missing = [name for name in SOURCE_BINDINGS if not (ROOT / name).is_file()]
    require(not missing, "source binding missing: " + ",".join(missing))
    sources = {name: base.binding(ROOT / name) for name in SOURCE_BINDINGS}
    flags = false_flags()
    rows = []
    for ordinal, scene in enumerate(FORMAL_MODERN_VISUAL_SCENES, 1):
        state, preset = REGISTRY_CAMERA[scene.scene_id]
        rows.append({
            "ordinal": ordinal, "scene_id": scene.scene_id,
            "status": "NOT_RUN", "recipe": recipe(scene),
            "explicit_instant_anchor_required": True,
            "presentation_query": "get_presentation_state",
            "presentation_query_timing":
                "AFTER_COMPLETE_RECIPE_BEFORE_PERFORMANCE_AND_SCREENSHOT",
            "expected_presentation": {
                "state": state, "preset": preset, "transitioning": False,
                "finite_pose_and_projection": True,
            },
            "interest_region": region(scene),
            "process_started": False, "screenshot_created": False,
            "semantic_camera_attested": False,
            "loaded_asset_attestation": False,
            "performance_measured": False, "machine_pass": False,
            "ta_review": False, "formal_evidence": False,
        })
    plan = {
        "schema": "phase2_v10i_formal_full_six_plan_v6",
        "status": "NOT_RUN_READY_FOR_FUTURE_EXACT_FULL_SIX_AUTHORIZATION",
        **flags, "capture_contract": capture_contract(), "rows": rows,
    }
    machine = {
        "schema": "phase2_v10i_formal_full_six_machine_template_v6",
        "status": "NOT_RUN_READY", **flags,
        "required_capture_schema": "phase2_v10i_formal_six_scene_capture_v2",
        "required_capture_count": 6,
        "required_fresh_process_count": 6,
        "required_scene_ids_in_order": list(SCENE_IDS),
        "validator": "scripts/validate_phase2_production_visuals.py",
        "scenes": [{"ordinal": i + 1, "scene_id": scene_id,
                    "status": "NOT_RUN", "ppm_path": None,
                    "ppm_sha256": None, "semantic_camera": None,
                    "loaded_asset_attestation": None, "performance": None,
                    "machine_pass": False}
                   for i, scene_id in enumerate(SCENE_IDS)],
    }
    dispatch = {
        "schema": "phase2_v10i_formal_full_six_single_use_dispatch_contract_v6",
        "status": "PASS_CPU_ONLY_NOT_AUTHORIZED",
        **flags,
        "wrapper": sources["scripts/run_phase2_v10i_formal_full_six_capture_v6.py"],
        "authorization_schema":
            "phase2_v10i_formal_six_scene_one_shot_authorization_v3",
        "authorized_scene_ids_in_order": list(SCENE_IDS),
        "authorized_capture_count": 6,
        "consumption_timing": "BEFORE_OUTPUT_OR_PROCESS_START",
        "atomic_flags": ["O_CREAT", "O_EXCL", "O_WRONLY"],
        "durability": "flush_plus_fsync_before_close",
        "failure_permanently_consumes_authorization": True,
        "incomplete_marker_is_never_deleted_or_reused": True,
        "bad_input_creates_no_marker_output_or_process": True,
        "existing_marker_rejects_before_output_or_process": True,
        "capture_source_unchanged": True,
    }
    implementation = {
        "schema": "phase2_v10i_formal_full_six_reentry_implementation_gate_v6",
        "status": "PASS_CPU_STATIC_READY_NO_GPU", **flags,
        "reentry_gate": base.binding(REENTRY_GATE),
        "targeted_machine_prerequisite": base.binding(TARGETED_MACHINE),
        "targeted_ta_prerequisite": base.binding(TARGETED_TA),
        "full_six_ta_template": base.binding(TA_TEMPLATE),
        "semantic_scene_contract": {
            "registry_version": REGISTRY_VERSION,
            "scene_ids_in_order": list(SCENE_IDS),
            "expected_semantic_camera": EXPECTED_CAMERA,
            "all_scenes_have_explicit_instant_anchor": True,
            "all_scenes_query_exact_settled_presentation": True,
        },
        "immutable_acceptance_thresholds": capture_contract(),
        "single_use_dispatch": dispatch,
    }
    files = {
        "implementation_gate": ready_root / "implementation-gate-v6.json",
        "six_scene_plan": ready_root / "six-scene-plan-not-run-v6.json",
        "machine_template": ready_root / "machine-evidence-template-not-run-v6.json",
        "authorization_template": ready_root / "one-shot-authorization-template-not-issued-v6.json",
        "dispatch_contract": ready_root / "single-use-dispatch-contract-v6.json",
    }
    for key, value in (("implementation_gate", implementation),
                       ("six_scene_plan", plan),
                       ("machine_template", machine),
                       ("authorization_template", authorization_template()),
                       ("dispatch_contract", dispatch)):
        base.write_immutable(files[key], value)
    package_binding = base.binding(package_root / "formal-asset-package-v2.json")
    readiness = {
        "schema": "phase2_v10i_formal_full_six_reentry_readiness_v6",
        "status": "READY_FOR_EXACT_FULL_SIX_ONE_SHOT_AUTHORIZATION_NO_GPU",
        "implementation_ready": True, **flags,
        "approved_scenes": list(SCENE_IDS),
        "formal_registry_version": REGISTRY_VERSION,
        "expected_semantic_camera": EXPECTED_CAMERA,
        "reentry_gate": base.binding(REENTRY_GATE),
        "targeted_machine_prerequisite": base.binding(TARGETED_MACHINE),
        "targeted_ta_prerequisite": base.binding(TARGETED_TA),
        "full_six_ta_template": base.binding(TA_TEMPLATE),
        "fresh_release_executable": base.binding(executable),
        "frozen_gate_integrated_release": json.loads(
            REENTRY_GATE.read_text(encoding="utf-8"))["frozen_bindings"][7],
        "self_contained_asset_package": {
            **package_binding, "asset_root": str(package_root.resolve(strict=True)),
            "required_asset_count": package["required_asset_count"],
            "runtime_dependency_count": package["runtime_dependency_count"],
            "frozen_bytes_unchanged": True,
        },
        "source_bindings": sources,
        "generated_bindings": {key: base.binding(path) for key, path in files.items()},
        "offline_safety": {
            "can_start_candidate_executable": False,
            "can_create_cocoa_context": False, "can_call_opengl": False,
            "can_create_screenshot": False, "can_issue_authorization": False,
        },
        "remaining_external_gates": [
            "separate_exact_full_six_one_shot_authorization",
            "single_use_authorization_consumption", "six_scene_machine_capture",
            "six_scene_machine_validator", "independent_six_of_six_ta_review",
            "formal_decision", "golden_decision", "m2_decision"],
    }
    readiness_path = ready_root / "readiness-manifest-v6.json"
    base.write_immutable(readiness_path, readiness)
    receipt = {
        "schema": "phase2_v10i_formal_full_six_reentry_readiness_receipt_v6",
        "status": readiness["status"], **flags,
        "readiness_manifest": base.binding(readiness_path),
        "fresh_release_executable": base.binding(executable),
        "self_contained_asset_package": package_binding,
    }
    receipt_path = ready_root / "readiness-receipt-v6.json"
    base.write_immutable(receipt_path, receipt)
    files["readiness_manifest"] = readiness_path
    files["readiness_receipt"] = receipt_path
    return files


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--ready-root", type=Path, default=READY_ROOT)
    parser.add_argument("--package-root", type=Path, default=base.PACKAGE_ROOT)
    parser.add_argument("--executable", type=Path, default=DEFAULT_EXECUTABLE)
    arguments = parser.parse_args()
    try:
        result = prepare(arguments.ready_root, arguments.package_root,
                         arguments.executable)
        print(json.dumps({key: str(value) for key, value in result.items()},
                         indent=2, sort_keys=True))
    except (OSError, ValueError, json.JSONDecodeError, ReadinessError,
            base.ReadinessV2Error) as error:
        print("FAIL: " + str(error))
        return 2
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
