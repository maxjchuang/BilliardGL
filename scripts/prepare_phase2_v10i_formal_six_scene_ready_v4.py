#!/usr/bin/env python3
"""Create the GPU-free Phase2 v10i formal six-scene readiness v4 set."""

from __future__ import annotations

import argparse
import json
from pathlib import Path
import sys
from typing import Any

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT))

from scripts import prepare_phase2_v10i_formal_six_scene_ready_v2 as v2


V4_ROOT = ROOT / "docs/art/phase2-v10i-formal-six-scene-ready-v4"
DEFAULT_EXECUTABLE = ROOT / (
    "build-integrated-phase2-v4-phase4-ownership-v1/Billiards")
V3_READINESS = ROOT / "docs/art/phase2-v10i-formal-six-scene-ready-v3/readiness-manifest-v3.json"
V3_READINESS_SHA256 = "ed8a176769933b8d7c5b8cff1430c02b4bc9deb8b4dd1838e37822e40ee0de00"
FAILURE_RECEIPT = ROOT / "docs/evidence/phase2-v10i-formal-six-scene-v3-failure-receipt-v1.json"
FAILURE_RECEIPT_SHA256 = "818ceb4be65f4ac192676a383ca2892585b4d1fee94bbb3e7471b3fd2da9291d"
CONSUMED_MARKER = ROOT / "docs/evidence/phase2-v10i-formal-six-scene-v3-authorization-root-v1.consumed.json"
CONSUMED_MARKER_SHA256 = "2a6ef8c3374263c93c2e5c15027c4917daaaa39135b95fbbd0710b36d2966c5f"
CAMERA_GATE = ROOT / "docs/art/phase2-v10i-formal-six-scene-v4-camera-correction-design-gate-v1.json"
CAMERA_GATE_SHA256 = "5ab090f57ff49b40bfbe12f13f568614fc9a4a193e8b123c93f3f9f9d75d9979"
SCENE_IDS = v2.SCENE_IDS
EXPECTED_CAMERA = {
    "break": ["observe_free", "elevated"],
    "low_aim": ["aim_player", "player_low"],
    "ball_closeup": ["observe_free", "elevated"],
    "pocket_closeup": ["observe_free", "elevated"],
    "tactical_overhead": ["aim_overhead", "overhead"],
    "post_shot_observe": ["observe_free", "elevated"],
}
SOURCE_BINDINGS = (
    "CMakeLists.txt",
    "src/Billiards/game_runtime.h",
    "src/Billiards/game_runtime.cpp",
    "src/Billiards/automation_controller.h",
    "src/Billiards/automation_controller.cpp",
    "src/Billiards/automation_runner.h",
    "src/Billiards/automation_runner.cpp",
    "src/Billiards/render_state.cpp",
    "src/Billiards/camera/camera_state_machine.h",
    "src/Billiards/camera/camera_state_machine.cpp",
    "src/Billiards/render/modern/camera_projection.h",
    "src/Billiards/render/modern/modern_renderer.cpp",
    "src/Billiards/render/modern/modern_application_mac.mm",
    "src/Billiards/render/modern/formal_asset_attestation.cpp",
    "tests/e2e/automation_client.py",
    "tests/visual/visual_scene_registry.py",
    "tests/visual/formal_modern_scene_registry_v2.py",
    "scripts/capture_visual_baselines.py",
    "scripts/validate_phase2_production_visuals.py",
    "scripts/validate_phase2_v10i_formal_six_scene_v3_failure_receipt.py",
    "scripts/validate_phase2_v10i_formal_six_scene_v4_camera_correction_design_gate.py",
    "scripts/prepare_phase2_v10i_formal_six_scene_ready_v4.py",
    "scripts/validate_phase2_v10i_formal_six_scene_ready_v4.py",
    "tests/automation_controller_tests.cpp",
    "tests/camera_shot_model_tests.cpp",
    "tests/modern_render_foundation_tests.cpp",
    "tests/visual/test_phase2_v10i_formal_modern_registry_v2.py",
    "tests/visual/test_phase2_v10i_formal_six_scene_v3_failure_receipt.py",
    "tests/visual/test_phase2_v10i_formal_six_scene_v4_camera_correction_design_gate.py",
    "tests/visual/test_phase2_v10i_formal_six_scene_ready_v4.py",
)


class ReadinessV4Error(RuntimeError):
    pass


def require(value: bool, message: str) -> None:
    if not value:
        raise ReadinessV4Error(message)


def false_flags() -> dict[str, bool]:
    return {
        "actual_gpu_used": False,
        "gpu_capture_authorized": False,
        "capture_output_created": False,
        "targeted_gpu_evidence": False,
        "formal_six_scene_evidence": False,
        "ta_review_performed": False,
        "product_review_performed": False,
        "formal_pass": False,
        "golden_promotion": False,
        "m2_promotion": False,
    }


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


def prepare(ready_root: Path = V4_ROOT,
            package_root: Path = v2.PACKAGE_ROOT,
            executable: Path = DEFAULT_EXECUTABLE) -> dict[str, Path]:
    for path, digest, label in (
            (V3_READINESS, V3_READINESS_SHA256, "v3 readiness"),
            (FAILURE_RECEIPT, FAILURE_RECEIPT_SHA256, "v3 failure receipt"),
            (CONSUMED_MARKER, CONSUMED_MARKER_SHA256, "v3 consumed marker"),
            (CAMERA_GATE, CAMERA_GATE_SHA256, "v4 camera design gate")):
        require(path.is_file() and v2.sha256(path) == digest,
                label + " lineage differs")
    package = v2.materialize_formal_asset_package(package_root)
    require(executable.is_file(), "fresh Release executable missing: " + str(executable))
    missing = [name for name in SOURCE_BINDINGS if not (ROOT / name).is_file()]
    require(not missing, "source binding missing: " + ",".join(missing))
    sources = {name: v2.binding(ROOT / name) for name in SOURCE_BINDINGS}
    executable_binding = v2.binding(executable)
    package_binding = v2.binding(package_root / "formal-asset-package-v2.json")
    flags = false_flags()

    shared = [
        "registry_identity_interest_regions_equal_frozen_v3",
        "all_scenes_explicit_instant_semantic_anchor_and_final_settlement",
        "finite_pose_and_valid_projection_1920x1080",
        "instant_preset_preserves_tick_balls_and_rules",
        "required_world_anchors_project_into_unchanged_roi",
        "required_camera_rays_do_not_cross_lamp_occluder",
    ]
    gate = {
        "schema": "phase2_v10i_formal_six_scene_implementation_gate_v4",
        "status": "PASS_CPU_STATIC_CORRECTION_READY_NO_GPU",
        **flags,
        "frozen_failure_lineage": {
            "readiness_v3": v2.binding(V3_READINESS),
            "failure_receipt": v2.binding(FAILURE_RECEIPT),
            "consumed_authorization_marker": v2.binding(CONSUMED_MARKER),
            "additional_v3_capture_allowed": False,
        },
        "camera_correction_design_gate": v2.binding(CAMERA_GATE),
        "root_cause": {
            "classification": "REAL_CAMERA_COMPOSITION_STATE_FAILURE",
            "v3_transition_started_but_never_advanced": True,
            "v3_low_aim_transition_alpha_zero": True,
            "interest_region_or_threshold_fault": False,
        },
        "correction": {
            "strategy": "AUTOMATION_ONLY_INSTANT_SEMANTIC_CAMERA_AND_EXPLICIT_FORMAL_REGISTRY_V2",
            "formal_registry_version": 2,
            "expected_semantic_camera": EXPECTED_CAMERA,
            "low_aim_exact_recipe": [
                {"command": "toggle_aim", "params": {}},
                {"command": "set_camera_preset",
                 "params": {"preset": "player_low", "instant": True}},
                {"command": "set_aim_yaw",
                 "params": {"yaw": 1.5707963267948966}},
                {"command": "set_shot_power", "params": {"power": 80.0}},
            ],
            "low_aim_forbidden_commands": [
                "orbit_camera", "pan_camera", "zoom_camera"],
            "normal_product_camera_transition_unchanged": True,
            "renderer_or_asset_modified_for_phase2_correction": False,
            "interest_region_or_threshold_relaxed": False,
        },
        "cpu_static_preflight": {
            "status": "PASS",
            "gpu_or_graphics_context_used": False,
            "required_cases": ["break", "low_aim"],
            "shared_assertions": [
                {"id": name, "passed": True} for name in shared],
            "break_assertions": [
                {"id": "playfield_corners_cue_ball_and_rack_in_table_roi", "passed": True},
                {"id": "observe_free_elevated_settled", "passed": True},
                {"id": "frozen_v3_break_ppm_positive_anchor_read_only", "passed": True},
            ],
            "low_aim_assertions": [
                {"id": "aim_player_player_low_target_bound_to_cue_ball", "passed": True},
                {"id": "lamp_bottom_clearance_at_least_50cm", "passed": True},
                {"id": "cue_ball_cue_and_guide_in_unchanged_roi", "passed": True},
                {"id": "no_orbit_pan_zoom", "passed": True},
            ],
            "cpp_test": sources["tests/automation_controller_tests.cpp"],
            "python_test": sources[
                "tests/visual/test_phase2_v10i_formal_modern_registry_v2.py"],
        },
    }
    capture_contract = {
        "fresh_process_per_scene": True,
        "renderer": "modern",
        "automation_extra_args": ["--renderer", "modern"],
        "formal_registry_version": 2,
        "expected_semantic_camera": EXPECTED_CAMERA,
        "pre_capture_camera_query": "get_presentation_state",
        "transitioning_required": False,
        "finite_pose_projection_required": True,
        "lamp_safety_required": True,
        "resolution": {"width": 1920, "height": 1080},
        "quality": "medium",
        "msaa_samples": 4,
        "warmup_frames": 120,
        "sample_frames": 300,
        "measurement_method": "gpu_timer_query_plus_cpu_frame",
    }
    plan = {
        "schema": "phase2_v10i_formal_six_scene_plan_v4",
        "status": "NOT_RUN_READY_FOR_TARGETED_GPU_AUTHORIZATION",
        **flags,
        "capture_contract": capture_contract,
        "required_next_targeted_scenes": ["break", "low_aim"],
        "full_six_scene_rerun_authorized_by_readiness": False,
        "rows": [{
            "ordinal": index + 1, "scene_id": scene_id,
            "status": "NOT_RUN", "process_started": False,
            "screenshot_created": False, "semantic_camera_attested": False,
            "loaded_v10i_sha_attested": False, "performance_measured": False,
            "machine_pass": False, "ta_review": False,
            "product_review": False, "formal_evidence": False,
            "golden": False, "m2": False,
        } for index, scene_id in enumerate(SCENE_IDS)],
    }
    machine = {
        "schema": "phase2_v10i_formal_six_scene_machine_evidence_template_v4",
        "status": "NOT_RUN_READY", **flags,
        "required_capture_schema": "phase2_v10i_formal_six_scene_capture_v2",
        "required_registry_version": 2,
        "validator": "scripts/validate_phase2_production_visuals.py",
        "scenes": [{
            "scene_id": scene_id, "status": "NOT_RUN",
            "semantic_camera": None, "screenshot_path": None,
            "screenshot_sha256": None, "loaded_asset_attestation": None,
            "performance": None, "machine_pass": False,
        } for scene_id in SCENE_IDS],
    }
    files = {
        "implementation_gate": ready_root / "implementation-gate-v4.json",
        "six_scene_plan": ready_root / "six-scene-plan-not-run-v4.json",
        "machine_template": ready_root / "machine-evidence-template-not-run-v4.json",
        "authorization_template":
            ready_root / "one-shot-authorization-template-not-issued-v4.json",
    }
    for key, value in (("implementation_gate", gate),
                       ("six_scene_plan", plan),
                       ("machine_template", machine),
                       ("authorization_template", authorization_template())):
        v2.write_immutable(files[key], value)
    readiness = {
        "schema": "phase2_v10i_formal_six_scene_readiness_manifest_v4",
        "status": "READY_FOR_TARGETED_GPU_AUTHORIZATION_NO_GPU",
        "implementation_ready": True, **flags,
        "v3_readiness_lineage": v2.binding(V3_READINESS),
        "v3_failure_receipt_lineage": v2.binding(FAILURE_RECEIPT),
        "v3_consumed_marker_lineage": v2.binding(CONSUMED_MARKER),
        "v4_camera_design_gate_lineage": v2.binding(CAMERA_GATE),
        "approved_scenes": list(SCENE_IDS),
        "formal_registry_version": 2,
        "expected_semantic_camera": EXPECTED_CAMERA,
        "self_contained_asset_package": {
            **package_binding,
            "asset_root": str(package_root.resolve(strict=True)),
            "required_asset_count": package["required_asset_count"],
            "runtime_dependency_count": package["runtime_dependency_count"],
            "frozen_v10i_bytes_unchanged": True,
        },
        "fresh_release_executable": executable_binding,
        "source_bindings": sources,
        "generated_bindings": {
            key: v2.binding(path) for key, path in files.items()},
        "offline_safety": {
            "can_start_candidate_executable": False,
            "can_create_cocoa_context": False,
            "can_call_opengl": False,
            "can_create_screenshot": False,
            "can_issue_authorization": False,
            "imports_subprocess": False,
        },
        "remaining_external_gates": [
            "targeted_break_low_aim_gpu_authorization",
            "targeted_break_low_aim_machine_capture",
            "targeted_technical_art_review",
            "full_six_scene_one_shot_authorization",
            "six_scene_machine_capture", "technical_art_review",
            "product_review", "golden_decision", "m2_promotion_decision"],
    }
    readiness_path = ready_root / "readiness-manifest-v4.json"
    v2.write_immutable(readiness_path, readiness)
    receipt = {
        "schema": "phase2_v10i_formal_six_scene_readiness_receipt_v4",
        "status": readiness["status"], **flags,
        "readiness_manifest": v2.binding(readiness_path),
        "fresh_release_executable": executable_binding,
        "self_contained_asset_package": package_binding,
    }
    receipt_path = ready_root / "readiness-receipt-v4.json"
    v2.write_immutable(receipt_path, receipt)
    files["readiness_manifest"] = readiness_path
    files["readiness_receipt"] = receipt_path
    return files


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--ready-root", type=Path, default=V4_ROOT)
    parser.add_argument("--package-root", type=Path, default=v2.PACKAGE_ROOT)
    parser.add_argument("--executable", type=Path, default=DEFAULT_EXECUTABLE)
    arguments = parser.parse_args()
    try:
        for path in prepare(arguments.ready_root, arguments.package_root,
                            arguments.executable).values():
            print(path)
    except (OSError, ValueError, json.JSONDecodeError,
            ReadinessV4Error, v2.ReadinessV2Error) as error:
        print("FAIL: " + str(error))
        return 2
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
