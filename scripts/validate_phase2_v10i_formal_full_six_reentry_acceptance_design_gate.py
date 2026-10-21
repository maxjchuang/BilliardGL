#!/usr/bin/env python3
"""CPU-only validator for the Phase2 full-six reentry acceptance gate."""

from __future__ import annotations

import argparse
import hashlib
import json
from pathlib import Path
import sys
from typing import Any


ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT))

from tests.visual.formal_modern_scene_registry_v2 import (  # noqa: E402
    EXPECTED_CAMERA as REGISTRY_EXPECTED_CAMERA,
    FORMAL_MODERN_VISUAL_SCENES,
    REGISTRY_VERSION,
)


DEFAULT_GATE = ROOT / (
    "docs/art/phase2-v10i-formal-full-six-reentry-"
    "acceptance-design-gate-v1.json"
)
EXPECTED_BINDINGS = {
    "v3_frozen_failure_receipt": (
        "docs/evidence/phase2-v10i-formal-six-scene-v3-failure-receipt-v1.json",
        "818ceb4be65f4ac192676a383ca2892585b4d1fee94bbb3e7471b3fd2da9291d",
    ),
    "v4_camera_correction_design_gate": (
        "docs/art/phase2-v10i-formal-six-scene-v4-camera-correction-design-gate-v1.json",
        "5ab090f57ff49b40bfbe12f13f568614fc9a4a193e8b123c93f3f9f9d75d9979",
    ),
    "v5_targeted_readiness_manifest": (
        "docs/art/phase2-v10i-formal-targeted-camera-ready-v5/readiness-manifest-v5.json",
        "687fce04e0521ab26465b77ba35434353d6b210bfa703f3b9564633c9a28e01f",
    ),
    "v5_targeted_machine_pass_receipt": (
        "docs/evidence/phase2-v10i-formal-targeted-camera-v5-machine-pass-receipt-v1.json",
        "f26c82883704b01165dbb325509aa1cc6bf72e4b655a9229860751c036dc7db9",
    ),
    "v5_independent_ta_manual_review": (
        "docs/evidence/phase2-v10i-formal-targeted-camera-v5-independent-ta-manual-review-v1.json",
        "79f0efcf430362c41abc4abbec1dd075c9d52a551bc4969d6be44e7764cf2931",
    ),
    "current_formal_semantic_scene_registry": (
        "tests/visual/formal_modern_scene_registry_v2.py",
        "1e2d526f1e015435d3c67b7a4eac7e9fdda4086ee23c2e640de7d4eb17dc75e3",
    ),
    "current_formal_capture_source": (
        "scripts/capture_visual_baselines.py",
        "772ecc72a9ade0234413fbd87425fe8ddd810a85255b3f8274556eb8ecae3b89",
    ),
    "current_integrated_release_binary": (
        "build-integrated-phase2-v4-phase4-ownership-v1/Billiards",
        "854e86b9a7f995f7abd2417cbfc8b00610ed2e5f97525163d17d165e9ae427e9",
    ),
    "current_formal_production_visual_validator": (
        "scripts/validate_phase2_production_visuals.py",
        "3cdce7fb7fa131b54fffbcb419e24699af7c55c4af0a0e24673689ee03dad8c2",
    ),
    "frozen_formal_asset_package": (
        "assets/modern/phase2-v10i-formal/formal-asset-package-v2.json",
        "610b1c49dcc5f798a1a40ee90f9312e5a1a15f4c5c648276c172cfd87591e37d",
    ),
}
SCENES = [
    "break", "low_aim", "ball_closeup", "pocket_closeup",
    "tactical_overhead", "post_shot_observe",
]
EXPECTED_CAMERA = {
    scene: {"state": state, "preset": preset}
    for scene, (state, preset) in REGISTRY_EXPECTED_CAMERA.items()
}
EXPECTED_REGIONS = {
    "break": {"name": "table", "rect_normalized": [0.10, 0.16, 0.80, 0.68]},
    "low_aim": {"name": "cue_and_ball", "rect_normalized": [0.18, 0.36, 0.64, 0.42]},
    "ball_closeup": {"name": "ball", "rect_normalized": [0.25, 0.24, 0.50, 0.55]},
    "pocket_closeup": {"name": "pocket", "rect_normalized": [0.18, 0.18, 0.64, 0.64]},
    "tactical_overhead": {"name": "playfield", "rect_normalized": [0.20, 0.06, 0.60, 0.88]},
    "post_shot_observe": {"name": "moving_balls", "rect_normalized": [0.12, 0.15, 0.76, 0.68]},
}
EXPECTED_BOUNDARY = {
    "design_gate_only": True,
    "production_modified": False,
    "readiness_created": False,
    "authorization_prepared": False,
    "authorization_issued": False,
    "gpu_executed": False,
    "capture_performed": False,
    "full_six_machine_pass": False,
    "full_six_ta_review_performed": False,
    "full_six_formal_pass": False,
    "m2_promotion": False,
    "golden_promotion": False,
    "product_review_performed": False,
    "product_pass": False,
    "beta_pass": False,
}


def require(condition: bool, message: str) -> None:
    if not condition:
        raise RuntimeError(message)


def sha256(path: Path) -> str:
    digest = hashlib.sha256()
    with path.open("rb") as source:
        for chunk in iter(lambda: source.read(1024 * 1024), b""):
            digest.update(chunk)
    return digest.hexdigest()


def load_json(path: Path) -> dict[str, Any]:
    value = json.loads(path.read_text(encoding="utf-8"))
    require(isinstance(value, dict), f"{path}: object required")
    return value


def validate_bindings(gate: dict[str, Any]) -> dict[str, dict[str, Any]]:
    bindings = gate.get("frozen_bindings", [])
    require(isinstance(bindings, list) and len(bindings) == len(EXPECTED_BINDINGS),
            "binding count")
    by_role = {item.get("role"): item for item in bindings
               if isinstance(item, dict)}
    require(set(by_role) == set(EXPECTED_BINDINGS) and
            len(by_role) == len(bindings), "binding role inventory")
    loaded: dict[str, dict[str, Any]] = {}
    for role, (relative, digest) in EXPECTED_BINDINGS.items():
        item = by_role[role]
        require(item == {"role": role, "path": relative, "sha256": digest},
                f"{role}: gate binding drift")
        path = (ROOT / relative).resolve(strict=True)
        require(path.is_file() and sha256(path) == digest,
                f"{role}: byte drift")
        if path.suffix == ".json":
            loaded[role] = load_json(path)
    return loaded


def validate_lineage(loaded: dict[str, dict[str, Any]]) -> None:
    failure = loaded["v3_frozen_failure_receipt"]
    require(failure.get("status") == "FORMAL_SIX_SCENE_FAIL_FROZEN_NO_RERUN"
            and failure.get("failure", {}).get("failed_scene_id") == "low_aim"
            and failure.get("evidence_boundary", {}).get("formal_pass") is False,
            "v3 frozen failure lineage")
    v4 = loaded["v4_camera_correction_design_gate"]
    require(v4.get("schema") ==
            "phase2_v10i_formal_six_scene_v4_camera_correction_design_gate_v1"
            and v4.get("evidence_boundary", {}).get("gpu_executed") is False,
            "v4 correction gate lineage")
    readiness = loaded["v5_targeted_readiness_manifest"]
    require(readiness.get("schema") ==
            "phase2_v10i_formal_targeted_camera_readiness_v5" and
            readiness.get("targeted_scene_ids_in_order") == ["break", "low_aim"]
            and readiness.get("full_six_scene_capture_authorized") is False
            and readiness.get("formal_pass") is False,
            "v5 targeted readiness boundary")
    machine = loaded["v5_targeted_machine_pass_receipt"]
    require(machine.get("status") == "TARGETED_MACHINE_PASS" and
            machine.get("counts") == {
                "actual_targeted_gpu_captures": 2,
                "failed_targeted_machine_checks": 0,
                "full_six_scene_captures": 0,
                "machine_passed_targeted_scenes": 2,
                "planned_targeted_scenes": 2,
                "technical_art_reviews_completed": 0,
            } and machine.get("evidence_boundary", {}).get(
                "full_six_scene_evidence") is False,
            "v5 targeted machine receipt boundary")
    ta = loaded["v5_independent_ta_manual_review"]
    require(ta.get("status") == "PASS_TARGETED_BREAK_LOW_AIM_TA_REVIEW"
            and ta.get("overall_result") == "PASS"
            and ta.get("review_counts", {}).get("cases_pass") == 2
            and ta.get("evidence_boundary", {}).get(
                "full_six_scene_formal_pass") is False
            and ta.get("evidence_boundary", {}).get("m2_promotion") is False
            and ta.get("evidence_boundary", {}).get("golden_promotion") is False
            and ta.get("evidence_boundary", {}).get("product_pass") is False,
            "v5 targeted TA review boundary")


def validate_current_registry_and_capture() -> None:
    require(REGISTRY_VERSION == 2 and
            [scene.scene_id for scene in FORMAL_MODERN_VISUAL_SCENES] == SCENES,
            "current semantic registry identity/order")
    transition_starters = {"toggle_aim", "shoot", "replay_load",
                           "replay_seek_shot"}
    relative = {"orbit_camera", "pan_camera", "zoom_camera"}
    for scene in FORMAL_MODERN_VISUAL_SCENES:
        commands = list(scene.commands)
        presets = [(index, command) for index, command in enumerate(commands)
                   if command.name == "set_camera_preset"]
        require(presets and all(command.params.get("instant") is True
                                for _, command in presets),
                f"{scene.scene_id}: explicit instant semantic anchor")
        first_preset = presets[0][0]
        require(all(index > first_preset for index, command in enumerate(commands)
                    if command.name in relative),
                f"{scene.scene_id}: relative camera before semantic anchor")
        last_transition = max(
            (index for index, command in enumerate(commands)
             if command.name in transition_starters), default=-1)
        require(last_transition < 0 or any(index > last_transition
                                           for index, _ in presets),
                f"{scene.scene_id}: later transition lacks instant settlement")
        region = scene.interest_regions
        require(len(region) == 1 and {
            "name": region[0][0], "rect_normalized": list(region[0][1:])
        } == EXPECTED_REGIONS[scene.scene_id],
                f"{scene.scene_id}: ROI drift")
    require({key: {"state": value[0], "preset": value[1]}
             for key, value in REGISTRY_EXPECTED_CAMERA.items()} ==
            EXPECTED_CAMERA, "current semantic camera mapping")

    source = (ROOT / EXPECTED_BINDINGS[
        "current_formal_capture_source"][0]).read_text(encoding="utf-8")
    for token in (
        'presentation = client.command("get_presentation_state")',
        "validate_formal_presentation_camera(",
        "client.collect_render_performance(",
        "client.screenshot(",
        '"phase2_v10i_formal_six_scene_capture_v2"',
        '"formal_six_scene"',
        '"full_six_scene_evidence": not targeted',
        '"AUTHORIZED_FOR_EXACTLY_SIX_FRESH_PROCESS_CAPTURES"',
    ):
        require(token in source, "capture source contract missing: " + token)
    query = source.index('presentation = client.command("get_presentation_state")')
    performance = source.index("client.collect_render_performance(", query)
    screenshot = source.index("client.screenshot(", performance)
    require(query < performance < screenshot,
            "presentation query must precede performance and screenshot")


def validate_gate_structure(gate: dict[str, Any]) -> None:
    require(gate.get("schema") ==
            "phase2_v10i_formal_full_six_reentry_acceptance_design_gate_v1"
            and gate.get("status") ==
            "PASS_DESIGN_ONLY_READY_TO_PREPARE_FRESH_FULL_SIX_READINESS_NO_GPU",
            "gate identity/status")
    prerequisite = gate.get("prerequisite_disposition", {})
    require(prerequisite.get("v3_failure_remains_frozen") is True
            and prerequisite.get("v3_failed_scene") == "low_aim"
            and prerequisite.get("v5_targeted_machine_result") == "PASS"
            and prerequisite.get("v5_targeted_machine_scene_count") == 2
            and prerequisite.get("v5_targeted_ta_result") == "PASS"
            and prerequisite.get("v5_targeted_ta_scene_count") == 2
            and len(prerequisite.get("targeted_pass_proves_only", [])) == 2
            and prerequisite.get("targeted_pass_does_not_prove") == [
                "ball_closeup", "pocket_closeup", "tactical_overhead",
                "post_shot_observe", "full-six formal pass", "M2 promotion",
                "golden promotion", "product pass"],
            "targeted prerequisite/narrowness")

    contract = gate.get("full_six_scene_contract", {})
    require(contract.get("registry_version") == 2
            and contract.get("scene_ids_in_order") == SCENES
            and contract.get("expected_semantic_camera") == EXPECTED_CAMERA
            and contract.get("interest_regions") == EXPECTED_REGIONS
            and len(contract.get("camera_requirements_each_scene", [])) == 8
            and contract.get("fresh_process_per_scene") is True,
            "full-six semantic scene contract")
    camera_requirements = contract["camera_requirements_each_scene"]
    for token in ("instant=true", "get_presentation_state", "transitioning equals false",
                  "state and preset", "lamp-safety"):
        require(any(token in item for item in camera_requirements),
                "camera requirement missing: " + token)

    thresholds = gate.get("immutable_acceptance_thresholds", {})
    require(thresholds.get("resolution") == [1920, 1080]
            and thresholds.get("renderer") == "modern"
            and thresholds.get("quality") == "medium"
            and thresholds.get("msaa_samples") == 4
            and thresholds.get("pixel_format") == "PPM P6 RGB8"
            and thresholds.get("visible_pixel_channel_rule") ==
                "max_rgb_channel_gt_8"
            and thresholds.get("minimum_whole_frame_visible_pixels") == 1
            and thresholds.get("minimum_interest_region_visible_pixels") == 1
            and thresholds.get("performance") == {
                "measurement_method": "gpu_timer_query_plus_cpu_frame",
                "gpu_timer_queries_observed": True,
                "warmup_frames_min": 120, "sample_frames_min": 300,
                "p95_total_frame_ms_max": 16.7,
                "draw_calls_complete_scene_max": 100,
                "texture_resident_mib_max": 256,
            } and thresholds.get("threshold_or_region_relaxation_allowed")
            is False, "immutable render/performance thresholds")
    asset = thresholds.get("asset_attestation", {})
    require(asset.get("formal_asset_package_sha256") ==
            EXPECTED_BINDINGS["frozen_formal_asset_package"][1]
            and asset.get("self_contained_capture_asset_root_required") is True
            and asset.get("required_asset_binding_count") == 6
            and asset.get("runtime_dependency_sha_verification_required") is True
            and asset.get("silent_material_fallback_allowed") is False
            and asset.get("load_errors_allowed") is False
            and len(asset.get("renderer_load_checks_required", [])) == 7,
            "immutable asset-attestation thresholds")

    readiness = gate.get("fresh_reentry_readiness_gate", {})
    expected_rows = [{"ordinal": index + 1, "scene_id": scene,
                      "status": "NOT_RUN"}
                     for index, scene in enumerate(SCENES)]
    require(readiness.get("status") == "REQUIRED_NOT_CREATED_BY_THIS_GATE"
            and readiness.get("new_readiness_root_required") is True
            and readiness.get("must_bind_this_gate_sha256") is True
            and readiness.get(
                "must_bind_all_current_source_binary_asset_hashes_above") is True
            and readiness.get("must_run_cpu_static_tests_without_gpu") is True
            and readiness.get("must_create_exact_six_row_plan") is True
            and readiness.get("required_initial_row_status") == "NOT_RUN"
            and readiness.get("required_initial_rows") == expected_rows
            and readiness.get("all_initial_evidence_flags_false") is True
            and readiness.get("capture_output_must_not_exist") is True
            and readiness.get("authorization_must_not_be_issued") is True
            and readiness.get("readiness_may_not_claim_machine_or_ta_pass") is True,
            "fresh six-NOT_RUN readiness gate")

    authorization = gate.get("future_one_shot_full_six_authorization_gate", {})
    require(authorization.get("status") == "NOT_ISSUED_NOT_AUTHORIZED"
            and authorization.get("may_be_issued_only_after_fresh_readiness_pass")
            is True
            and authorization.get("schema") ==
                "phase2_v10i_formal_six_scene_one_shot_authorization_v3"
            and authorization.get("required_status") ==
                "AUTHORIZED_FOR_EXACTLY_SIX_FRESH_PROCESS_CAPTURES"
            and authorization.get("authorized_capture_count") == 6
            and authorization.get("consumed_capture_count_before_dispatch") == 0
            and authorization.get("scene_ids_in_order_must_equal_full_contract")
            is True
            and authorization.get("gpu_capture_authorized") is True
            and authorization.get("formal_six_scene_evidence_authorized") is True
            and authorization.get("formal_pass_at_issue") is False
            and authorization.get("golden_promotion_at_issue") is False
            and authorization.get("m2_promotion_at_issue") is False
            and authorization.get("single_use") is True
            and authorization.get("failure_consumes_authorization_and_freezes_partial_output")
            is True
            and authorization.get("authorization_artifact_created_by_this_gate")
            is False, "future one-shot authorization boundary")

    machine = gate.get("future_full_six_machine_gate", {})
    require(machine.get("status") == "NOT_RUN"
            and machine.get("manifest_schema") ==
                "phase2_v10i_formal_six_scene_capture_v2"
            and machine.get("required_capture_count") == 6
            and machine.get("required_fresh_process_count") == 6
            and machine.get("capture_scope_kind") == "formal_six_scene"
            and machine.get("capture_scope_full_six_scene_evidence") is True
            and len(machine.get("each_scene_requires", [])) == 6
            and len(machine.get("aggregate_requires", [])) == 4
            and machine.get("any_failure_result") ==
                "FAIL_FREEZE_NO_SAME_AUTH_RERUN"
            and machine.get("machine_pass_does_not_equal_ta_pass") is True
            and machine.get("machine_pass_does_not_equal_m2_golden_or_product_pass")
            is True, "future full-six machine gate")

    ta = gate.get("future_independent_ta_six_of_six_gate", {})
    require(ta.get("status") == "NOT_RUN"
            and ta.get("may_start_only_after_all_six_machine_pass") is True
            and ta.get("review_scene_count") == 6
            and ta.get("review_each_ppm_at_original_resolution") is True
            and ta.get("allowed_axis_statuses") == ["PASS", "FAIL"]
            and len(ta.get("required_axes_each_scene", [])) == 7
            and ta.get("fail_requires_exact_scene_and_blocking_axes") is True
            and ta.get("pass_requires_all_six_cases_and_all_42_axes_pass") is True
            and ta.get("ta_six_of_six_pass_does_not_equal_m2_golden_or_product_pass")
            is True, "future independent TA 6/6 gate")

    allowed = gate.get("allowed_changes_after_this_gate", [])
    require(len(allowed) == 3 and all("new" in item or "future" in item
                                     for item in allowed),
            "allowed reentry changes")
    forbidden = gate.get("forbidden_changes", [])
    for token in ("production C++", "semantic registry", "renderer, shaders",
                  "shipping assets", "scene IDs", "visible-pixel",
                  "rules, physics", "v3/v5 frozen", "authorization", "M2"):
        require(any(token in item for item in forbidden),
                "forbidden scope missing: " + token)
    require(gate.get("evidence_boundary") == EXPECTED_BOUNDARY,
            "design-gate evidence boundary promoted")


def validate(path: Path = DEFAULT_GATE) -> dict[str, Any]:
    path = path.resolve(strict=True)
    gate = load_json(path)
    loaded = validate_bindings(gate)
    validate_lineage(loaded)
    validate_current_registry_and_capture()
    validate_gate_structure(gate)
    return {
        "status": "PASS_FULL_SIX_REENTRY_ACCEPTANCE_DESIGN_GATE_NO_GPU",
        "gate_sha256": sha256(path),
        "bound_scene_count": 6,
        "required_initial_status": "6_NOT_RUN",
        "targeted_pass_is_full_six_pass": False,
        "readiness_created": False,
        "authorization_prepared": False,
        "authorization_issued": False,
        "gpu_executed": False,
        "production_modified": False,
        "full_six_machine_pass": False,
        "full_six_ta_pass": False,
        "m2_promotion": False,
        "golden_promotion": False,
        "product_pass": False,
    }


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--gate", type=Path, default=DEFAULT_GATE)
    arguments = parser.parse_args()
    try:
        print(json.dumps(validate(arguments.gate), indent=2, sort_keys=True))
    except (OSError, ValueError, json.JSONDecodeError, RuntimeError) as error:
        print("FAIL: " + str(error))
        return 2
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
