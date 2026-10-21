#!/usr/bin/env python3
"""Offline validator for the Phase2 v4 camera-correction design gate.

This script reads frozen JSON/PPM evidence and performs CPU math only.  It does
not create a graphics context, run the product, authorize capture, or edit any
production source or asset.
"""

from __future__ import annotations

import argparse
import hashlib
import json
import math
from pathlib import Path
from typing import Any


ROOT = Path(__file__).resolve().parents[1]
DEFAULT_GATE = ROOT / (
    "docs/art/phase2-v10i-formal-six-scene-v4-camera-"
    "correction-design-gate-v1.json"
)
EXPECTED_RECEIPT_PATH = ROOT / (
    "docs/evidence/phase2-v10i-formal-six-scene-v3-failure-receipt-v1.json"
)
EXPECTED_RECEIPT_SHA256 = (
    "818ceb4be65f4ac192676a383ca2892585b4d1fee94bbb3e7471b3fd2da9291d"
)
EXPECTED_SCENES = [
    "break", "low_aim", "ball_closeup", "pocket_closeup",
    "tactical_overhead", "post_shot_observe",
]
EXPECTED_PRESETS = {
    "break": {"preset": "elevated", "state": "observe_free"},
    "low_aim": {"preset": "player_low", "state": "aim_player"},
    "ball_closeup": {"preset": "elevated", "state": "observe_free"},
    "pocket_closeup": {"preset": "elevated", "state": "observe_free"},
    "tactical_overhead": {"preset": "overhead", "state": "aim_overhead"},
    "post_shot_observe": {"preset": "elevated", "state": "observe_free"},
}
EXPECTED_LOW_RECIPE = [
    {"command": "toggle_aim", "params": {}},
    {"command": "set_camera_preset",
     "params": {"preset": "player_low", "instant": True}},
    {"command": "set_aim_yaw",
     "params": {"yaw": 1.5707963267948966}},
    {"command": "set_shot_power", "params": {"power": 80.0}},
]
EXPECTED_BOUNDARY = {
    "design_only": True,
    "production_files_modified_by_this_gate": False,
    "gpu_executed": False,
    "capture_authorized": False,
    "capture_performed": False,
    "formal_pass": False,
    "golden_promotion": False,
    "m2_promotion": False,
    "product_review_performed": False,
    "technical_art_review_performed": False,
    "product_exposed": False,
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
    require(isinstance(value, dict), f"{path}: JSON object required")
    return value


def bound_path(relative: Any) -> Path:
    require(isinstance(relative, str) and relative, "bound path missing")
    path = (ROOT / relative).resolve(strict=True)
    require(path.is_file(), f"bound file missing: {relative}")
    return path


def read_ppm(path: Path) -> tuple[int, int, bytes]:
    with path.open("rb") as image:
        require(image.readline().strip() == b"P6", f"{path}: P6 required")
        width, height = map(int, image.readline().split())
        require(image.readline().strip() == b"255", f"{path}: RGB8 required")
        pixels = image.read()
    require(len(pixels) == width * height * 3, f"{path}: pixel length")
    return width, height, pixels


def pixel_diagnostics(
    pixels: bytes, width: int, height: int,
    bounds: tuple[int, int, int, int] | None = None,
) -> tuple[int, int, list[int] | None]:
    x0, y0, x1, y1 = bounds or (0, 0, width, height)
    visible = 0
    maximum = 0
    min_x, min_y, max_x, max_y = width, height, -1, -1
    for y in range(y0, y1):
        row = y * width * 3
        for x in range(x0, x1):
            channel = max(pixels[row + x * 3:row + x * 3 + 3])
            maximum = max(maximum, channel)
            if channel > 8:
                visible += 1
                min_x, min_y = min(min_x, x), min(min_y, y)
                max_x, max_y = max(max_x, x), max(max_y, y)
    bbox = None if visible == 0 else [min_x, min_y, max_x, max_y]
    return visible, maximum, bbox


def validate_frozen_failure(gate: dict[str, Any]) -> dict[str, Any]:
    frozen = gate.get("frozen_v3_failure", {})
    require(frozen.get("receipt_path") == str(
        EXPECTED_RECEIPT_PATH.relative_to(ROOT)), "failure receipt path drift")
    require(frozen.get("receipt_sha256") == EXPECTED_RECEIPT_SHA256,
            "failure receipt gate SHA drift")
    require(sha256(EXPECTED_RECEIPT_PATH) == EXPECTED_RECEIPT_SHA256,
            "frozen failure receipt byte drift")
    receipt = load_json(EXPECTED_RECEIPT_PATH)
    require(receipt.get("schema") ==
            "phase2_v10i_formal_six_scene_v3_failure_receipt_v1" and
            receipt.get("status") == "FORMAL_SIX_SCENE_FAIL_FROZEN_NO_RERUN",
            "failure receipt identity")
    require(receipt.get("failure", {}).get("failed_scene_id") == "low_aim",
            "frozen failed scene")
    require(frozen.get("status") == receipt["status"] and
            frozen.get("failed_scene_id") == "low_aim" and
            frozen.get("authorization_consumed") is True and
            frozen.get("additional_v3_capture_allowed") is False,
            "frozen v3 disposition")

    bindings = receipt.get("frozen_bindings", {})
    for gate_key, receipt_prefix in (("break_ppm", "break"),
                                     ("low_aim_ppm", "low_aim")):
        item = frozen.get(gate_key, {})
        require(item.get("path") == bindings.get(receipt_prefix + "_ppm_path")
                and item.get("sha256") ==
                bindings.get(receipt_prefix + "_ppm_sha256"),
                f"{gate_key}: receipt binding drift")
        path = bound_path(item.get("path"))
        require(sha256(path) == item.get("sha256"), f"{gate_key}: SHA drift")
        width, height, pixels = read_ppm(path)
        require(item.get("dimensions") == [width, height] == [1920, 1080]
                and item.get("reviewed_at_original_resolution") is True,
                f"{gate_key}: original-resolution review binding")
        if gate_key == "break_ppm":
            visible, _, _ = pixel_diagnostics(pixels, width, height)
            roi = (192, 172, 1728, 907)
            region_visible, _, _ = pixel_diagnostics(
                pixels, width, height, roi)
            require(visible > 3450 and region_visible > 0,
                    "break positive visual anchor")
        else:
            visible, _, bbox = pixel_diagnostics(pixels, width, height)
            region_visible, region_max, _ = pixel_diagnostics(
                pixels, width, height, (345, 388, 1574, 842))
            require(visible == item.get("overall_visible_pixels") == 3450
                    and bbox == item.get("overall_visible_pixel_bbox") ==
                    [240, 1029, 469, 1043]
                    and region_visible ==
                    item.get("interest_region_visible_pixels") == 0
                    and region_max == item.get("interest_region_max_channel") == 2,
                    "low_aim frozen pixel diagnostics")
    return receipt


def _close(actual: float, expected: float, tolerance: float = 0.00001) -> bool:
    return math.isfinite(actual) and abs(actual - expected) <= tolerance


def validate_attribution(gate: dict[str, Any]) -> None:
    finding = gate.get("independent_technical_art_attribution", {})
    require(finding.get("classification") ==
            "REAL_CAMERA_COMPOSITION_STATE_FAILURE",
            "low_aim attribution must remain camera/composition")
    require(finding.get("asset_or_lighting_failure_primary") is False and
            finding.get("interest_region_misconfiguration") is False,
            "asset/lighting or ROI misclassification")
    chain = finding.get("causal_chain", [])
    require(len(chain) == 6 and any("alpha zero" in item for item in chain)
            and any("No camera-time step" in item for item in chain),
            "causal chain incomplete")

    surface_y = 86.483976
    default_target_y = surface_y - 10.0
    lamp_reference_y = surface_y + 135.0
    lamp_bottom_y = lamp_reference_y - 0.6
    break_eye_y = default_target_y + math.sin(math.radians(42.0) - 0.10) * 220.0
    v3_eye_y = default_target_y + math.sin(math.radians(42.0) + 0.40) * 182.0
    cue_target_y = surface_y + 5.715 / 2.0
    v4_eye_y = cue_target_y + math.sin(math.radians(20.0)) * 145.0
    expected = {
        "play_surface_y": surface_y,
        "lamp_reference_y": lamp_reference_y,
        "lamp_bottom_y": lamp_bottom_y,
        "v3_break_eye_y": break_eye_y,
        "v3_low_aim_transition_start_eye_y": v3_eye_y,
        "v4_low_aim_player_low_eye_y": v4_eye_y,
        "v4_low_aim_clearance_below_lamp_bottom": lamp_bottom_y - v4_eye_y,
    }
    actual = finding.get("static_geometry_cm", {})
    require(set(actual) == set(expected), "static camera/geometry fields")
    for key, value in expected.items():
        require(_close(float(actual[key]), value, 0.000002),
                f"static camera/geometry mismatch: {key}")
    require(v3_eye_y > lamp_reference_y and break_eye_y < lamp_bottom_y
            and v4_eye_y + 50.0 < lamp_bottom_y,
            "occluder height classification")

    # At the lamp bottom plane, the v3 ray from centered elevated eye to the
    # centered target crosses the 65x185 cm lamp footprint.
    pitch = math.radians(42.0) + 0.40
    horizontal = math.cos(pitch) * 182.0
    eye = (math.cos(-math.pi / 2) * horizontal, v3_eye_y,
           math.sin(-math.pi / 2) * horizontal)
    t = (lamp_bottom_y - eye[1]) / (default_target_y - eye[1])
    cross_x = eye[0] * (1.0 - t)
    cross_z = eye[2] * (1.0 - t)
    require(0.0 < t < 1.0 and abs(cross_x) <= 32.5
            and abs(cross_z) <= 92.5,
            "v3 view ray must cross the authored lamp footprint")


def validate_gate_structure(gate: dict[str, Any]) -> None:
    require(gate.get("schema") ==
            "phase2_v10i_formal_six_scene_v4_camera_correction_design_gate_v1"
            and gate.get("status") ==
            "PASS_DESIGN_ONLY_NO_GPU_NO_PRODUCTION_EDIT",
            "gate identity")
    validate_attribution(gate)

    correction = gate.get("v4_minimum_correction_contract", {})
    require(correction.get("strategy") ==
            "AUTOMATION_ONLY_INSTANT_SEMANTIC_CAMERA_AND_EXPLICIT_FORMAL_REGISTRY_V2"
            and correction.get("formal_modern_registry_version") == 2,
            "v4 correction strategy")
    require(correction.get("required_scene_ids_in_order") == EXPECTED_SCENES,
            "registry v2 scene inventory/order")
    every = correction.get("every_scene_camera_rule", {})
    require(every == {
        "required_explicit_camera_anchor": "set_camera_preset",
        "required_parameter": {"instant": True},
        "relative_delta_requires_preceding_semantic_anchor": True,
        "later_semantic_transition_requires_explicit_settlement_and_final_query": True,
        "low_aim_anchor_must_follow_toggle_aim": True,
        "reason": "Every scene has a named semantic pose; low_aim snaps after toggle_aim, while a later intentional shot/replay transition in another scene must settle and be verified before capture.",
        "legacy_relative_delta_may_be_sole_final_camera_contract": False,
        "pre_capture_transitioning_must_equal": False,
    }, "every-scene semantic camera rule")
    require(correction.get("semantic_preset_map") == EXPECTED_PRESETS,
            "semantic preset map")
    require(correction.get("low_aim_required_recipe") == EXPECTED_LOW_RECIPE,
            "low_aim exact recipe")
    require(correction.get("low_aim_forbidden_commands") ==
            ["orbit_camera", "pan_camera", "zoom_camera"],
            "low_aim relative camera command ban")
    low_state = correction.get("low_aim_pre_capture_presentation_state", {})
    require(low_state == {
        "state": "aim_player", "preset": "player_low",
        "transitioning": False, "finite_eye_target_distance_fov": True,
        "cue_ball_target_binding": True,
    }, "low_aim pre-capture state")
    instant = correction.get("instant_semantics", {})
    require(instant == {
        "automation_only": True,
        "must_resolve_to_exact_named_preset_pose": True,
        "must_not_advance_physics_tick": True,
        "must_not_change_ball_or_rules_state": True,
        "must_return_presentation_state_or_be_immediately_queryable": True,
        "interactive_product_transition_behavior_unchanged": True,
    }, "instant automation semantics")

    allowed = gate.get("allowed_implementation_layers", [])
    require(len(allowed) == 6 and
            any("automation command schema" in item for item in allowed) and
            any("immediate preset snap seam" in item for item in allowed) and
            any("registry v2" in item for item in allowed),
            "allowed implementation layers")
    require(not any("renderer implementation" in item or
                    "shipping asset" in item for item in allowed),
            "renderer/assets may not enter allowed scope")
    forbidden = gate.get("forbidden_changes", [])
    for token in ("renderer implementation", "shipping glTF", "product camera",
                  "rules, physics", "interest-region", "visible-pixel",
                  "consumed v3", "formal/GPU/golden"):
        require(any(token in item for item in forbidden),
                f"forbidden scope missing: {token}")

    visual = gate.get("immutable_visual_acceptance", {})
    require(visual == {
        "resolution": [1920, 1080], "renderer": "modern",
        "quality": "medium", "msaa_samples": 4,
        "fresh_process_per_scene": True,
        "visible_pixel_channel_rule": "max_rgb_channel_gt_8",
        "minimum_whole_frame_visible_pixels": 1,
        "minimum_interest_region_visible_pixels": 1,
        "break_interest_region": {
            "name": "table", "rect_normalized": [0.10, 0.16, 0.80, 0.68]},
        "low_aim_interest_region": {
            "name": "cue_and_ball",
            "rect_normalized": [0.18, 0.36, 0.64, 0.42]},
        "threshold_or_region_relaxation_allowed": False,
    }, "immutable visual acceptance drift")

    cpu = gate.get("cpu_static_preflight_gate", {})
    require(cpu.get("status") == "REQUIRED_NOT_RUN_BY_THIS_DESIGN_GATE"
            and cpu.get("gpu_or_graphics_context_allowed") is False
            and cpu.get("required_cases") == ["break", "low_aim"]
            and len(cpu.get("shared_assertions", [])) == 6
            and len(cpu.get("break_assertions", [])) == 3
            and len(cpu.get("low_aim_assertions", [])) == 4
            and cpu.get("report_must_fail_closed_if_any_assertion_missing_or_false")
            is True,
            "CPU/static preflight contract")
    require(any("50 cm" in item for item in cpu["low_aim_assertions"])
            and any("unchanged cue_and_ball ROI" in item
                    for item in cpu["low_aim_assertions"]),
            "low_aim static clearance/projection checks")

    gpu = gate.get("future_targeted_actual_gpu_gate", {})
    require(gpu.get("status") == "NOT_RUN_NOT_AUTHORIZED"
            and gpu.get("required_scene_ids_in_order") == ["break", "low_aim"]
            and gpu.get("minimum_capture_count") == 2
            and gpu.get("new_output_root_required") is True
            and gpu.get("fresh_process_per_scene") is True
            and gpu.get("same_frozen_v10i_formal_asset_package_required") is True
            and len(gpu.get("preconditions", [])) == 4
            and len(gpu.get("machine_checks", [])) == 6
            and gpu.get("technical_art_review", {}).get("required") is True
            and gpu.get("technical_art_review", {}).get(
                "original_resolution_required") is True
            and gpu.get("full_six_scene_formal_rerun_authorized_by_this_gate")
            is False
            and gpu.get("capture_authorized_by_this_gate") is False,
            "future targeted GPU boundary")
    require(gate.get("evidence_boundary") == EXPECTED_BOUNDARY,
            "evidence boundary promoted")


def validate(path: Path = DEFAULT_GATE) -> dict[str, Any]:
    gate = load_json(path.resolve(strict=True))
    receipt = validate_frozen_failure(gate)
    validate_gate_structure(gate)
    return {
        "status": "PASS_V4_CAMERA_CORRECTION_DESIGN_GATE_NO_GPU",
        "gate_sha256": sha256(path),
        "failure_receipt_sha256": sha256(EXPECTED_RECEIPT_PATH),
        "classification": gate[
            "independent_technical_art_attribution"]["classification"],
        "frozen_actual_gpu_ppm_count":
            receipt["counts"]["actual_gpu_ppm_files"],
        "cpu_static_required_cases": ["break", "low_aim"],
        "future_targeted_gpu_required_cases": ["break", "low_aim"],
        "gpu_executed": False,
        "production_modified": False,
        "capture_authorized": False,
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
