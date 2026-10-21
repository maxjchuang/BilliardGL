#!/usr/bin/env python3
"""Prepare a GPU-free BLOCKED readiness package for v10i formal six-scene evidence.

This program is intentionally not a capture runner.  It cannot start Cocoa,
OpenGL, or the candidate executable.  It records the exact implementation
gaps that must be closed before a separate, one-shot runner may be authorized.
"""

from __future__ import annotations

import argparse
import hashlib
import json
from pathlib import Path
from typing import Any


ROOT = Path(__file__).resolve().parents[1]
DEFAULT_READY_ROOT = ROOT / "docs/art/phase2-v10i-formal-six-scene-ready-v1"
DEFAULT_CAPTURE_OUTPUT = ROOT / "docs/art/phase2-v10i-formal-six-scene-candidate-v1"
DEFAULT_EXECUTABLE = ROOT / "build-phase2-v10i-formal-ready/Billiards"

SCENE_IDS = (
    "break", "low_aim", "ball_closeup", "pocket_closeup",
    "tactical_overhead", "post_shot_observe",
)
TA_AXES = (
    "composition", "material_color", "contact_shadow", "geometry_alignment",
    "ball_identity_and_rotation_seam", "venue_and_background_focus",
    "wood_uv_scale_and_rail_joint_continuity",
    "ball_material_number_label_and_highlight_quality",
    "felt_microdetail_saturation_and_full_table_composition",
)
PRODUCT_AXES = (
    "gameplay_readability", "camera_composition", "table_visual_focus",
    "physics_visual_alignment", "material_quality_consistency",
    "phase2_exit_gate_acceptance",
)
EXPECTED_FROZEN_SHA256 = {
    "v10i_gltf": "c25cfedf969deb5bc329b58aad089015f6f2bc841315d39d9e793c8a5786ba88",
    "v10i_asset_manifest": "3cc895416d6bec6483379bd7ab921d921863a6da2372e25624c66b03c8bda48d",
    "single_capture_manifest": "28c5d2221cc52e6c6e4475dfa77756841f1443f6b401d17475567a6aa2f5016c",
    "single_capture_png": "e5f701d6907adf77fd082c40011bf3ef4eedeb8d5d9fd99e09b5de9f0ef13f6e",
    "single_gpu_freeze": "df0f5853097e348b9f6526cac2d91fe9a29c99d6ccf74cf494db11b951343f2f",
    "single_ta_review": "43987deb863e5860752d5ae78c0b8abafa0831955dccce352c663dfcfc55f29a",
    "renderer_source": "f7169523b4ef9a8a84d32a61c901d96911d436f7c7e7d3075733bd43e21bea93",
}

INPUT_PATHS = {
    "implementation_plan": ROOT / "docs/superpowers/specs/2026-07-27-billiardgl-visual-gameplay-optimization-plan.md",
    "target_board": ROOT / "docs/art/visual-target-and-acceptance.md",
    "lookdev_manifest": ROOT / "assets/modern/lookdev/focused_practice_hall_v1.lookdev.json",
    "v10i_gltf": ROOT / "assets/modern/phase2-v10i/focused_practice_hall_modular_v1.gltf",
    "v10i_asset_manifest": ROOT / "assets/modern/phase2-v10i/focused_practice_hall_modular_v1.asset.json",
    "renderer_source": ROOT / "src/Billiards/render/modern/modern_renderer.cpp",
    "application_source": ROOT / "src/Billiards/render/modern/modern_application_mac.mm",
    "asset_root_source": ROOT / "src/Billiards/billiards.cpp",
    "automation_client": ROOT / "tests/e2e/automation_client.py",
    "capture_runner": ROOT / "scripts/capture_visual_baselines.py",
    "scene_registry": ROOT / "tests/visual/visual_scene_registry.py",
    "formal_validator": ROOT / "scripts/validate_phase2_production_visuals.py",
    "single_capture_manifest": ROOT / "docs/art/phase2-pocket-v10i-gpu-preflight-v1/phase2-pocket-v10i-gpu-preflight-manifest-v1.json",
    "single_capture_png": ROOT / "docs/art/phase2-pocket-v10i-gpu-preflight-v1/pocket_closeup.png",
    "single_gpu_freeze": ROOT / "docs/art/phase2-pocket-v10i-gpu-source-freeze-v1/receipt.json",
    "single_ta_review": ROOT / "docs/art/phase2-pocket-v10i-ta-review-v1/phase2-pocket-v10i-ta-review-v1.json",
    "historical_v5_manifest": ROOT / "build/test-output/gltf-six-v5/manifest.json",
    "historical_v5_manual_review": ROOT / "docs/art/phase2-production-modern-v5-manual-review.json",
}


class ReadinessError(RuntimeError):
    """Fail-closed readiness or authorization error."""


def sha256(path: Path) -> str:
    digest = hashlib.sha256()
    with path.open("rb") as source:
        for chunk in iter(lambda: source.read(1024 * 1024), b""):
            digest.update(chunk)
    return "sha256:" + digest.hexdigest()


def canonical(value: Any) -> bytes:
    return (json.dumps(value, indent=2, sort_keys=True) + "\n").encode("utf-8")


def write_immutable(path: Path, value: Any) -> None:
    payload = canonical(value)
    path.parent.mkdir(parents=True, exist_ok=True)
    if path.exists() and path.read_bytes() != payload:
        raise ReadinessError("immutable formal-readiness artifact differs: " + str(path))
    path.write_bytes(payload)


def binding(path: Path) -> dict[str, Any]:
    try:
        display_path = path.relative_to(ROOT).as_posix()
    except ValueError:
        display_path = str(path.resolve())
    result = {
        "exists": path.is_file(),
        "path": display_path,
        "sha256": sha256(path) if path.is_file() else None,
    }
    return result


def validate_frozen_inputs(executable: Path = DEFAULT_EXECUTABLE) -> dict[str, dict[str, Any]]:
    paths = dict(INPUT_PATHS)
    paths["fresh_candidate_executable"] = executable
    missing = [name for name, path in paths.items() if not path.is_file()]
    if missing:
        raise ReadinessError("missing readiness input: " + ",".join(missing))
    bindings = {name: binding(path) for name, path in paths.items()}
    mismatches = [name for name, expected in EXPECTED_FROZEN_SHA256.items()
                  if bindings[name]["sha256"] != "sha256:" + expected]
    if mismatches:
        raise ReadinessError("frozen SHA mismatch: " + ",".join(mismatches))

    capture = json.loads(INPUT_PATHS["single_capture_manifest"].read_text(encoding="utf-8"))
    review = json.loads(INPUT_PATHS["single_ta_review"].read_text(encoding="utf-8"))
    checks = capture.get("machine_checks", {})
    if len(checks) != 50 or sum(value is True for value in checks.values()) != 50:
        raise ReadinessError("v10i single-scene machine prerequisite is not exact 50/50")
    if review.get("axis_summary", {}).get("axis_count") != 7 or \
            review.get("axis_summary", {}).get("pass_count") != 7:
        raise ReadinessError("v10i single-scene TA prerequisite is not exact 7/7")
    return bindings


def exact_authorization_template(readiness_manifest_sha: str = "POPULATE_AFTER_GAPS_CLOSE") -> dict[str, Any]:
    return {
        "schema": "phase2_v10i_formal_six_scene_one_shot_authorization_v1",
        "status": "BLOCKED_NOT_AUTHORIZABLE",
        "actual_gpu_used": False,
        "authorization_id": "NOT_ISSUED",
        "authorized_by": None,
        "gpu_capture_authorized": False,
        "authorized_capture_count": 0,
        "consumed_capture_count": 0,
        "ta_review_performed": False,
        "product_review_performed": False,
        "ta_review_authorized": False,
        "product_review_authorized": False,
        "formal_six_scene_evidence": False,
        "formal_six_scene_evidence_authorized": False,
        "formal_pass": False,
        "formal_pass_authorized": False,
        "golden_promotion": False,
        "golden_promotion_authorized": False,
        "m2_promotion": False,
        "m2_promotion_authorized": False,
        "contract_after_implementation_gaps_close": {
            "capture_count": 6,
            "scene_ids_in_order": list(SCENE_IDS),
            "renderer": "modern",
            "width": 1920,
            "height": 1080,
            "quality": "medium",
            "msaa_samples": 4,
            "fresh_process_per_scene": True,
            "retry_without_new_authorization": False,
            "required_readiness_manifest_sha256": readiness_manifest_sha,
            "required_per_scene_loaded_asset_sha_attestation": True,
        },
        "decision": "TEMPLATE_ONLY; MUST NOT BE CONSUMED WHILE READINESS IS BLOCKED",
    }


def validate_authorization_for_capture(value: dict[str, Any]) -> None:
    """Validate and then fail because this readiness package is blocked.

    The function performs no filesystem mutation and imports no process or UI
    APIs.  A future runner must replace this function only after every recorded
    implementation gap has its own offline test and exact source/binary binding.
    """
    if value.get("schema") != "phase2_v10i_formal_six_scene_one_shot_authorization_v1":
        raise ReadinessError("authorization schema mismatch; pre-Cocoa rejection")
    if value.get("status") != "AUTHORIZED_FOR_EXACTLY_SIX_FRESH_PROCESS_CAPTURES":
        raise ReadinessError("authorization status is not executable; pre-Cocoa rejection")
    required_true = (
        "gpu_capture_authorized", "formal_six_scene_evidence_authorized",
    )
    for field in required_true:
        if value.get(field) is not True:
            raise ReadinessError("authorization field mismatch: " + field)
    if value.get("authorized_capture_count") != 6 or value.get("consumed_capture_count") != 0:
        raise ReadinessError("authorization capture count mismatch")
    if value.get("scene_ids_in_order") != list(SCENE_IDS):
        raise ReadinessError("authorization six-scene order mismatch")
    raise ReadinessError(
        "BLOCKED: exact per-scene v10i loaded-asset attestation and formal performance collection are not implemented; pre-Cocoa rejection")


def pretrigger_capture(authorization: dict[str, Any], capture_output: Path) -> None:
    """Fail before any output directory, Cocoa context, or OpenGL process exists."""
    if capture_output.exists():
        raise ReadinessError("capture output must be absent before one-shot authorization")
    validate_authorization_for_capture(authorization)
    raise AssertionError("unreachable")


def _not_run_axis(axis: str, blocker: str | None = None) -> dict[str, Any]:
    value = {"axis": axis, "evidence": None, "status": "NOT_RUN"}
    if blocker:
        value["blocking_policy"] = blocker
    return value


def prepare(ready_root: Path = DEFAULT_READY_ROOT,
            executable: Path = DEFAULT_EXECUTABLE) -> dict[str, Path]:
    bindings = validate_frozen_inputs(executable)
    status = "BLOCKED_MISSING_EXACT_V10I_PER_SCENE_ASSET_ATTESTATION_AND_FORMAL_PERFORMANCE"
    global_flags = {
        "actual_gpu_used": False,
        "gpu_capture_authorized": False,
        "ta_review_performed": False,
        "product_review_performed": False,
        "formal_six_scene_evidence": False,
        "formal_pass": False,
        "golden_promotion": False,
        "m2_promotion": False,
    }
    plan = {
        "schema": "phase2_v10i_formal_six_scene_plan_v1",
        "status": status,
        **global_flags,
        "capture_contract": {
            "renderer": "modern", "resolution": {"width": 1920, "height": 1080},
            "quality": "medium", "msaa_samples": 4, "fresh_process_per_scene": True,
        },
        "rows": [
            {
                "ordinal": index + 1, "scene_id": scene_id, "status": "NOT_RUN",
                "actual_gpu": False, "loaded_v10i_sha_attested": False,
                "machine_evidence": False, "ta_review": False, "product_review": False,
                "formal_evidence": False, "golden": False, "m2": False,
            }
            for index, scene_id in enumerate(SCENE_IDS)
        ],
    }
    authorization = exact_authorization_template()
    machine = {
        "schema": "phase2_v10i_formal_six_scene_machine_evidence_template_v1",
        "status": "NOT_RUN_BLOCKED", **global_flags,
        "asset_gates": {
            "v10i_modular_gltf_sha256": bindings["v10i_gltf"]["sha256"],
            "v10i_asset_manifest_sha256": bindings["v10i_asset_manifest"]["sha256"],
            "per_scene_ready_handshake_must_report_canonical_path_and_sha_for": [
                "modular_gltf", "asset_manifest", "pbr_params", "lighting_rig", "venue_gltf", "ball_atlas"
            ],
            "self_contained_capture_asset_root_required": True,
            "load_error_gate": "FAIL unless every error names the file and field; silent white/fallback material is prohibited",
        },
        "performance_gates": {
            "profile": "1920x1080_medium", "warmup_frames_min": 120,
            "sample_frames_min": 300, "p95_total_frame_ms_max": 16.7,
            "draw_calls_complete_scene_max": 100, "texture_resident_mib_max": 256,
            "measurement_method_allowed": ["gpu_timer_query_plus_cpu_frame", "platform_frame_capture"],
        },
        "scenes": [
            {
                "scene_id": scene_id, "status": "NOT_RUN", "screenshot_path": None,
                "screenshot_sha256": None, "loaded_asset_attestation": None,
                "roi_checks": [], "load_errors": [], "performance": None,
                "machine_pass": False,
            }
            for scene_id in SCENE_IDS
        ],
    }
    full_table_blocker = (
        "Blocking in formal six-scene review even though the frozen single pocket_closeup passed; "
        "N/A requires scene-specific evidence and cannot waive another scene."
    )
    manual = {
        "schema": "phase2_v10i_formal_six_scene_manual_evidence_template_v1",
        "status": "NOT_RUN_BLOCKED", **global_flags,
        "prerequisite": "machine six-of-six PASS with exact per-scene v10i asset attestation",
        "scenes": [
            {
                "scene_id": scene_id, "status": "NOT_RUN", "screenshot_sha256": None,
                "technical_art": {
                    "reviewer": None, "performed": False, "pass": False,
                    "axes": [_not_run_axis(axis, full_table_blocker if axis in TA_AXES[-3:] else None)
                             for axis in TA_AXES],
                },
                "product": {
                    "reviewer": None, "performed": False, "pass": False,
                    "axes": [_not_run_axis(axis) for axis in PRODUCT_AXES],
                },
            }
            for scene_id in SCENE_IDS
        ],
        "formal_blocking_axes_promoted_from_single_scene_outside_scope": [
            "stretched_or_repetitive_wood_grain_and_sharp_triangular_or_faceted_rail_apron_joints",
            "simplistic_decal_like_ball_highlights_numbers_identity_and_rotation_seams",
            "flat_saturated_felt_and_full_table_composition_clipping_or_edge_geometry",
        ],
    }
    gaps = {
        "schema": "phase2_v10i_formal_six_scene_implementation_gap_v1",
        "status": status, **global_flags,
        "runner_audit_decision": "CURRENT_RUNNER_CANNOT_PROVE_ANY_OF_SIX_PROCESSES_LOADED_V10I",
        "gaps": [
            {
                "id": "CPP_READY_ASSET_ATTESTATION",
                "owner": "C++/graphics",
                "required_change": "Retain canonical resolved paths and exact SHA-256 for glTF, asset manifest, PBR params, lighting, venue and ball atlas; emit them in the rendered-automation ready metadata for every process.",
                "files_expected": ["src/Billiards/billiards.cpp", "src/Billiards/render/modern/modern_application_mac.mm", "modern renderer/loader result interfaces"],
            },
            {
                "id": "CPP_LOAD_ERROR_CONTRACT",
                "owner": "C++/graphics",
                "required_change": "Expose structured load failures naming exact file and field and forbid silent white/plain material fallback for production formal capture.",
            },
            {
                "id": "PYTHON_EXPLICIT_CHILD_ENV",
                "owner": "capture infrastructure",
                "required_change": "AutomationClient must accept an explicit child environment; the six-scene runner must set the exact v10i asset root for each fresh process and reject inherited/default ambiguity.",
                "files_expected": ["tests/e2e/automation_client.py", "scripts/capture_visual_baselines.py"],
            },
            {
                "id": "SELF_CONTAINED_FORMAL_ASSET_PACKAGE",
                "owner": "C++/graphics + technical art",
                "required_change": "Provide an immutable capture-local asset_root containing all six assets required by the formal validator. The current phase2-v10i root contains only modular glTF and its asset manifest.",
            },
            {
                "id": "FORMAL_PERFORMANCE_COLLECTION",
                "owner": "C++/graphics + capture infrastructure",
                "required_change": "Collect 120 warmup and >=300 samples plus p95 total frame ms, complete-scene draw calls, texture residency and accepted measurement method at 1920x1080 Medium.",
            },
            {
                "id": "FORMAL_MANIFEST_CONTRACT",
                "owner": "capture infrastructure",
                "required_change": "Write a manifest accepted by validate_phase2_production_visuals.py, bind every screenshot and exact source asset SHA, then run TA and product review only after machine 6/6.",
            },
        ],
        "candidate_executable_disposition": "FRESH_BUILD_AUDITED_BUT_INELIGIBLE; DO_NOT_RUN",
        "candidate_runner_disposition": "SOURCE_BOUND_BUT_INELIGIBLE; DO_NOT_RUN",
    }

    files = {
        "six_scene_plan": ready_root / "six-scene-plan-not-run.json",
        "authorization_template": ready_root / "one-shot-authorization-template-blocked.json",
        "machine_template": ready_root / "machine-evidence-template-not-run.json",
        "manual_template": ready_root / "manual-evidence-template-not-run.json",
        "implementation_gaps": ready_root / "implementation-gaps-blocking.json",
    }
    for key, document in (
        ("six_scene_plan", plan), ("authorization_template", authorization),
        ("machine_template", machine), ("manual_template", manual),
        ("implementation_gaps", gaps),
    ):
        write_immutable(files[key], document)

    generated_bindings = {name: binding(path) for name, path in files.items()}
    manifest = {
        "schema": "phase2_v10i_formal_six_scene_readiness_manifest_v1",
        "status": status, **global_flags,
        "prerequisites": {
            "single_scene_machine": "PASS_50_OF_50",
            "single_scene_ta": "PASS_7_OF_7",
            "prerequisite_scope": "pocket_closeup_single_scene_non_formal_only",
            "does_not_imply_formal_six_scene_pass": True,
        },
        "approved_scenes": list(SCENE_IDS),
        "manual_axes": {"technical_art": list(TA_AXES), "product": list(PRODUCT_AXES)},
        "formal_gates": machine["performance_gates"] | {
            "machine_scenes_required": 6, "ta_scenes_required": 6,
            "product_scenes_required": 6, "asset_and_load_gates_mandatory": True,
        },
        "input_bindings": bindings,
        "generated_bindings": generated_bindings,
        "baseline_policy": {
            "historical_v5_baseline_bound_for_comparison_only": True,
            "historical_v5_status": "FAIL_NOT_APPROVED_GOLDEN",
            "review_ssim": "UNRESOLVED_NO_APPROVED_BASELINE",
            "self_approval_or_golden_promotion": False,
        },
        "offline_safety": {
            "imports_process_api": False, "can_start_candidate_executable": False,
            "can_create_cocoa_context": False, "can_call_opengl": False,
            "capture_output_path": DEFAULT_CAPTURE_OUTPUT.relative_to(ROOT).as_posix(),
            "capture_output_created": DEFAULT_CAPTURE_OUTPUT.exists(),
        },
        "decision": "BLOCKED; IMPLEMENT GAPS AND REBUILD BEFORE REQUESTING ONE-SHOT AUTHORIZATION",
    }
    files["readiness_manifest"] = ready_root / "readiness-manifest-blocked.json"
    write_immutable(files["readiness_manifest"], manifest)
    receipt = {
        "schema": "phase2_v10i_formal_six_scene_readiness_receipt_v1",
        "status": status, **global_flags,
        "readiness_manifest": binding(files["readiness_manifest"]),
        "artifact_count": 6,
        "all_six_rows_not_run": all(row["status"] == "NOT_RUN" for row in plan["rows"]),
        "candidate_executable": bindings["fresh_candidate_executable"],
        "capture_output_absent": not DEFAULT_CAPTURE_OUTPUT.exists(),
        "decision": "NO_GPU; NO_TA; NO_PRODUCT; NO_FORMAL; NO_GOLDEN; NO_M2",
    }
    files["readiness_receipt"] = ready_root / "readiness-receipt-blocked.json"
    write_immutable(files["readiness_receipt"], receipt)
    return files


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--ready-root", type=Path, default=DEFAULT_READY_ROOT)
    parser.add_argument("--executable", type=Path, default=DEFAULT_EXECUTABLE)
    parser.add_argument("--attempt-capture-authorization", type=Path)
    parser.add_argument("--capture-output", type=Path, default=DEFAULT_CAPTURE_OUTPUT)
    args = parser.parse_args()
    try:
        if args.attempt_capture_authorization is not None:
            value = json.loads(args.attempt_capture_authorization.read_text(encoding="utf-8"))
            pretrigger_capture(value, args.capture_output)
        files = prepare(args.ready_root.resolve(), args.executable.resolve())
    except (OSError, json.JSONDecodeError, ReadinessError) as error:
        print("phase2 v10i formal six-scene readiness rejected: " + str(error))
        return 2
    print("phase2 v10i formal six-scene readiness BLOCKED; GPU=false")
    for name, path in files.items():
        print(name + "=" + str(path))
        print(name + "_sha256=" + sha256(path))
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
