#!/usr/bin/env python3
"""Generate fail-closed Phase-3 GPU capture and manual-review templates."""

from __future__ import annotations

import argparse
import json
from pathlib import Path


CAPTURE_TEMPLATE_NAME = "phase3-gpu-capture-matrix-v1.pending.json"
MANUAL_TEMPLATE_NAME = "phase3-gpu-manual-review-v1.pending.json"
CAPTURE_SCHEMA_NAME = "phase3-gpu-capture-manifest-v1.schema.json"
MANUAL_SCHEMA_NAME = "phase3-gpu-manual-review-v1.schema.json"

HUD_STATES = ("default", "aiming", "post_shot", "foul", "help")
CAMERA_PRESETS = ("player_low", "elevated", "overhead", "broadcast")
PALETTES = ("standard", "color_vision_safe")
UI_SCALES = (0.85, 1.0, 1.35)
VIEWPORTS = {
    "1280x720": {"logical_px": [1280, 720], "physical_px": [1280, 720],
                 "dpi": 96, "device_scale": 1.0,
                 "safe_area_physical_px": {"left": 43, "top": 24, "right": 43, "bottom": 36}},
    "1920x1080": {"logical_px": [1920, 1080], "physical_px": [1920, 1080],
                   "dpi": 96, "device_scale": 1.0,
                   "safe_area_physical_px": {"left": 64, "top": 36, "right": 64, "bottom": 54}},
    "retina_1280x720": {"logical_px": [1280, 720], "physical_px": [1920, 1080],
                         "dpi": 144, "device_scale": 1.5,
                         "safe_area_physical_px": {"left": 64, "top": 36, "right": 64, "bottom": 54}},
}

SHELL_SCENARIOS = {
    "main_menu_training_unavailable": {
        "screen": "main_menu", "variant": "training_unavailable",
        "setting_category": None, "scroll_state": "not_applicable",
        "focus_state": "single_player_focused", "minimum_icons": 7,
        "minimum_focus": 1, "minimum_selected": 0, "minimum_error": 0,
        "minimum_disabled": 1,
    },
    "single_player_setup_club_selected": {
        "screen": "single_player_setup", "variant": "club_selected",
        "setting_category": None, "scroll_state": "not_applicable",
        "focus_state": "rookie_focused", "minimum_icons": 7,
        "minimum_focus": 1, "minimum_selected": 1, "minimum_error": 0,
        "minimum_disabled": 1,
    },
    "main_menu_training_available": {
        "screen": "main_menu", "variant": "training_pack_loaded",
        "setting_category": None, "scroll_state": "not_applicable",
        "focus_state": "training_focused", "minimum_icons": 7,
        "minimum_focus": 1, "minimum_selected": 0, "minimum_error": 0,
        "minimum_disabled": 0,
    },
    "training_catalog_invalid_pack_error": {
        "screen": "training_catalog", "variant": "invalid_pack_error",
        "setting_category": None, "scroll_state": "not_applicable",
        "focus_state": "none_no_enabled_item", "minimum_icons": 2,
        "minimum_focus": 0, "minimum_selected": 0, "minimum_error": 1,
        "minimum_disabled": 1,
    },
    "training_session_active": {
        "screen": "training_session", "variant": "attempt_active",
        "setting_category": None, "scroll_state": "not_applicable",
        "focus_state": "reset_training_focused", "minimum_icons": 4,
        "minimum_focus": 1, "minimum_selected": 0, "minimum_error": 0,
        "minimum_disabled": 1,
    },
    "replay_browser_invalid_path_error": {
        "screen": "replay_browser", "variant": "invalid_path_error",
        "setting_category": None, "scroll_state": "not_applicable",
        "focus_state": "choose_replay_focused", "minimum_icons": 4,
        "minimum_focus": 1, "minimum_selected": 0, "minimum_error": 1,
        "minimum_disabled": 1,
    },
    "replay_playback_paused": {
        "screen": "replay_playback", "variant": "paused",
        "setting_category": None, "scroll_state": "not_applicable",
        "focus_state": "play_focused", "minimum_icons": 8,
        "minimum_focus": 1, "minimum_selected": 0, "minimum_error": 0,
        "minimum_disabled": 0,
    },
    "settings_gameplay": {
        "screen": "settings", "variant": "category_gameplay",
        "setting_category": "gameplay", "scroll_state": "top_all_rows_fit",
        "focus_state": "assist_level_focused", "minimum_icons": 2,
        "minimum_focus": 1, "minimum_selected": 0, "minimum_error": 0,
        "minimum_disabled": 0,
    },
    "settings_input": {
        "screen": "settings", "variant": "category_input_rebind_idle",
        "setting_category": "input", "scroll_state": "top_all_rows_fit",
        "focus_state": "mouse_sensitivity_focused", "minimum_icons": 4,
        "minimum_focus": 1, "minimum_selected": 0, "minimum_error": 0,
        "minimum_disabled": 0,
    },
    "settings_accessibility": {
        "screen": "settings", "variant": "category_accessibility",
        "setting_category": "accessibility", "scroll_state": "top_all_rows_fit",
        "focus_state": "ui_scale_focused", "minimum_icons": 4,
        "minimum_focus": 1, "minimum_selected": 0, "minimum_error": 0,
        "minimum_disabled": 0,
    },
    "settings_audio": {
        "screen": "settings", "variant": "category_audio",
        "setting_category": "audio", "scroll_state": "top_all_rows_fit_or_reflow_columns",
        "focus_state": "music_volume_focused", "minimum_icons": 9,
        "minimum_focus": 1, "minimum_selected": 0, "minimum_error": 0,
        "minimum_disabled": 0,
    },
    "settings_display": {
        "screen": "settings", "variant": "category_display",
        "setting_category": "display", "scroll_state": "top_all_rows_fit_or_reflow_columns",
        "focus_state": "field_of_view_focused", "minimum_icons": 7,
        "minimum_focus": 1, "minimum_selected": 0, "minimum_error": 0,
        "minimum_disabled": 0,
    },
    "help_back_focused": {
        "screen": "help", "variant": "reference_rows_disabled",
        "setting_category": None, "scroll_state": "not_applicable",
        "focus_state": "help_back_focused", "minimum_icons": 6,
        "minimum_focus": 1, "minimum_selected": 0, "minimum_error": 0,
        "minimum_disabled": 5,
    },
}

ASSET_BINDINGS = {
    "hud_manifest": {"path": "assets/modern/hud/focused_practice_hud_v1.hud.json",
                     "sha256": "sha256:969d3b8bc7de46600d03f77a10e577fe560e2106a612e708739a96a73c27a6e1"},
    "hud_tokens": {"path": "assets/modern/hud/focused_practice_hud_shipping_tokens_v1.json",
                   "sha256": "sha256:abf73aa283279c28677b5c97a9b02b3130b0d0125048c3124c85a9c0e72df860"},
    "shell_manifest": {"path": "assets/modern/hud/focused_practice_shell_shipping_tokens_v1.manifest.json",
                       "sha256": "sha256:cb937f7c590a7ab0a2b7666e1fd6b522eb032c9393909558e31872fc4faf743a"},
    "shell_tokens": {"path": "assets/modern/hud/focused_practice_shell_shipping_tokens_v1.json",
                     "sha256": "sha256:c144f1a25d3e7ced14e385bd6f76ca7e0f05b06597dcc8a4bacb4c570fc6bad3"},
}

PERFORMANCE_THRESHOLDS = {
    "warmup_frames_min": 120, "sample_frames_min": 300,
    "p95_total_frame_ms_max": 16.7, "draw_calls_complete_scene_max": 100,
    "texture_resident_mib_max": 256,
}

MANUAL_AXES = (
    "composition_and_gameplay_occlusion", "text_readability_and_hierarchy",
    "icon_semantics_and_state_redundancy", "palette_and_color_vision_readability",
    "focus_selected_error_disabled_clarity", "camera_preset_consistency",
    "debug_and_shipping_separation",
)


def _scale_id(value):
    return {0.85: "085", 1.0: "100", 1.35: "135"}[value]


def _hud_minimum_icons(state):
    return {"default": 1, "aiming": 3, "post_shot": 1, "foul": 2, "help": 0}[state]


def expected_capture_entries():
    entries = []
    for state in HUD_STATES:
        for viewport_name, viewport in VIEWPORTS.items():
            for camera in CAMERA_PRESETS:
                for palette in PALETTES:
                    for scale in UI_SCALES:
                        capture_id = "hud__%s__%s__%s__%s__ui%s" % (
                            state, viewport_name, camera, palette, _scale_id(scale))
                        entries.append({
                            "capture_id": capture_id, "surface": "hud", "state": state,
                            "screen": None, "variant": None, "camera_preset": camera,
                            "viewport": viewport_name, "palette": palette, "ui_scale": scale,
                            "expected_physical_px": viewport["physical_px"],
                            "safe_area_physical_px": viewport["safe_area_physical_px"],
                            "minimum_text_physical_px": round(14 * viewport["device_scale"] * scale, 2),
                            "minimum_expanded_icons": _hud_minimum_icons(state),
                            "minimum_focus": 0, "minimum_selected": 0,
                            "minimum_error": 1 if state == "foul" else 0,
                            "minimum_disabled": 0,
                        })
    for scenario_name, scenario in SHELL_SCENARIOS.items():
        for viewport_name, viewport in VIEWPORTS.items():
            for palette in PALETTES:
                for scale in UI_SCALES:
                    capture_id = "shell__%s__%s__%s__ui%s" % (
                        scenario_name, viewport_name, palette, _scale_id(scale))
                    entries.append({
                        "capture_id": capture_id, "surface": "shell", "state": None,
                        "screen": scenario["screen"], "variant": scenario["variant"],
                        "scenario": scenario_name, "camera_preset": None,
                        "viewport": viewport_name, "palette": palette, "ui_scale": scale,
                        "setting_category": scenario["setting_category"],
                        "scroll_state": scenario["scroll_state"],
                        "focus_state": scenario["focus_state"],
                        "expected_physical_px": viewport["physical_px"],
                        "safe_area_physical_px": viewport["safe_area_physical_px"],
                        "minimum_text_physical_px": round(14 * viewport["device_scale"] * scale, 2),
                        "minimum_expanded_icons": scenario["minimum_icons"],
                        "minimum_focus": scenario["minimum_focus"],
                        "minimum_selected": scenario["minimum_selected"],
                        "minimum_error": scenario["minimum_error"],
                        "minimum_disabled": scenario["minimum_disabled"],
                    })
    return entries


def _pending_capture(entry):
    return {
        **entry,
        "artifact": {"path": "PENDING", "sha256": "PENDING",
                     "actual_gpu_capture": False, "capture_method": "PENDING"},
        "machine_status": "PENDING",
        "machine_metrics": {
            "safe_area_violations": "PENDING", "truncated_text_runs": "PENDING",
            "overlap_pairs": "PENDING", "minimum_rendered_text_px": "PENDING",
            "expanded_icon_count": "PENDING", "opaque_icon_count": "PENDING",
            "unexpanded_icon_count": "PENDING", "focus_indicator_count": "PENDING",
            "selected_indicator_count": "PENDING", "error_indicator_count": "PENDING",
            "disabled_shape_count": "PENDING", "debug_node_count": "PENDING",
            "glyph_rasterizations_during_frame": "PENDING",
        },
        "performance": {
            "warmup_frames": "PENDING", "sample_frames": "PENDING",
            "p95_total_frame_ms": "PENDING", "draw_calls_complete_scene": "PENDING",
            "texture_resident_mib": "PENDING",
            "measurement_method": "PENDING",
        },
    }


def build_capture_template():
    entries = [_pending_capture(entry) for entry in expected_capture_entries()]
    return {
        "schema": "phase3_gpu_capture_manifest_v1", "status": "PENDING",
        "evidence_class": "actual_gpu_framebuffer_capture_required",
        "asset_bindings": ASSET_BINDINGS,
        "runtime_attestation": {
            "git_revision": "PENDING", "executable_sha256": "PENDING",
            "renderer": "PENDING", "gpu_vendor": "PENDING", "gpu_device": "PENDING",
            "driver": "PENDING", "operating_system": "PENDING",
            "capture_utc": "PENDING",
        },
        "matrix_contract": {
            "hud_states": list(HUD_STATES), "camera_presets": list(CAMERA_PRESETS),
            "viewports": VIEWPORTS, "palettes": list(PALETTES),
            "ui_scales": list(UI_SCALES), "shell_scenarios": SHELL_SCENARIOS,
            "hud_capture_count": 360, "shell_capture_count": 234,
            "total_capture_count": 594,
            "pre_capture_contract_corrections": [
                {
                    "id": "replace_unroutable_match_shell_scenario_v1",
                    "classification": "pre_capture_stale_contract_correction",
                    "removed_scenario": "match_default",
                    "replacement_scenario": "main_menu_training_available",
                    "capture_count_delta": 0,
                    "machine_thresholds_changed": False,
                    "evidence_status_advanced": False,
                },
                {
                    "id": "replace_unreachable_empty_training_catalog_v1",
                    "classification": "pre_capture_reachability_correction",
                    "removed_scenario": "training_catalog_empty",
                    "replacement_scenario": "training_catalog_invalid_pack_error",
                    "capture_count_delta": 0,
                    "machine_thresholds_changed": False,
                    "evidence_status_advanced": False,
                },
                {
                    "id": "correct_help_back_focus_v1",
                    "classification": "pre_capture_reachability_correction",
                    "removed_scenario": "help_disabled_reference",
                    "replacement_scenario": "help_back_focused",
                    "capture_count_delta": 0,
                    "machine_thresholds_changed": False,
                    "evidence_status_advanced": False,
                },
            ],
        },
        "machine_thresholds": PERFORMANCE_THRESHOLDS,
        "evidence_boundary": {
            "flat_proof_is_capture": False, "flat_proof_is_user_study": False,
            "desktop_screenshot_allowed": False,
            "allowed_capture_methods": ["rendered_automation_gpu_readback", "gpu_framebuffer_readback"],
            "forbidden_proof_files": [
                "focused_practice_hud_shipping_tokens_flat_proof_v1.svg",
                "focused_practice_shell_shipping_tokens_flat_proof_v1.svg",
            ],
        },
        "captures": entries,
    }


def build_manual_template():
    reviews = []
    for entry in expected_capture_entries():
        reviews.append({
            "capture_id": entry["capture_id"], "screenshot_sha256": "PENDING",
            "axes": {axis: {"status": "PENDING", "evidence": "PENDING_NATIVE_RESOLUTION_REVIEW"}
                     for axis in MANUAL_AXES},
        })
    return {
        "schema": "phase3_gpu_manual_review_v1", "status": "PENDING",
        "reviewer_role": "independent_technical_artist",
        "capture_binding": {"manifest_path": "PENDING", "manifest_sha256": "PENDING",
                            "machine_status_required": "PASS_ALL_594"},
        "evidence_boundary": {"flat_proof_is_capture": False,
                              "flat_proof_is_user_study": False,
                              "user_study_completed": False},
        "review_axes": list(MANUAL_AXES),
        "capture_reviews": reviews,
        "template_policy": {
            "approval_prohibited_while_pending": True,
            "fill_only_after_actual_gpu_manifest_hash_binding": True,
            "fill_only_after_machine_pass": True,
            "golden_creation_authorized": False,
        },
    }


def build_capture_schema():
    return {
        "$schema": "https://json-schema.org/draft/2020-12/schema",
        "$id": "billiardgl://schemas/phase3-gpu-capture-manifest-v1",
        "title": "BilliardGL Phase 3 GPU Capture Manifest",
        "type": "object", "additionalProperties": False,
        "required": ["schema", "status", "evidence_class", "asset_bindings",
                     "runtime_attestation", "matrix_contract", "machine_thresholds",
                     "evidence_boundary", "captures"],
        "properties": {
            "schema": {"const": "phase3_gpu_capture_manifest_v1"},
            "status": {"enum": ["PENDING", "PASS"]},
            "evidence_class": {"const": "actual_gpu_framebuffer_capture_required"},
            "asset_bindings": {"type": "object"},
            "runtime_attestation": {"type": "object"},
            "matrix_contract": {
                "type": "object",
                "required": ["hud_states", "camera_presets", "viewports",
                             "palettes", "ui_scales", "shell_scenarios",
                             "hud_capture_count", "shell_capture_count",
                             "total_capture_count", "pre_capture_contract_corrections"],
                "properties": {
                    "pre_capture_contract_corrections": {
                        "type": "array", "minItems": 3, "maxItems": 3,
                        "items": {
                            "type": "object",
                            "required": ["id", "classification", "removed_scenario",
                                         "replacement_scenario", "capture_count_delta",
                                         "machine_thresholds_changed",
                                         "evidence_status_advanced"],
                            "properties": {
                                "classification": {"enum": [
                                    "pre_capture_stale_contract_correction",
                                    "pre_capture_reachability_correction"]},
                                "capture_count_delta": {"const": 0},
                                "machine_thresholds_changed": {"const": False},
                                "evidence_status_advanced": {"const": False},
                            },
                        },
                    },
                },
            },
            "machine_thresholds": {"type": "object"},
            "evidence_boundary": {"type": "object"},
            "captures": {"type": "array", "minItems": 594, "maxItems": 594,
                         "items": {"type": "object", "required": ["capture_id", "surface",
                                   "viewport", "palette", "ui_scale", "artifact",
                                   "machine_status", "machine_metrics", "performance"]}},
        },
    }


def build_manual_schema():
    return {
        "$schema": "https://json-schema.org/draft/2020-12/schema",
        "$id": "billiardgl://schemas/phase3-gpu-manual-review-v1",
        "title": "BilliardGL Phase 3 GPU Manual Review",
        "type": "object", "additionalProperties": False,
        "required": ["schema", "status", "reviewer_role", "capture_binding",
                     "evidence_boundary", "review_axes", "capture_reviews", "template_policy"],
        "properties": {
            "schema": {"const": "phase3_gpu_manual_review_v1"},
            "status": {"enum": ["PENDING", "PASS", "FAIL"]},
            "reviewer_role": {"const": "independent_technical_artist"},
            "capture_binding": {"type": "object"},
            "evidence_boundary": {"type": "object"},
            "review_axes": {"type": "array", "minItems": 7, "maxItems": 7},
            "capture_reviews": {"type": "array", "minItems": 594, "maxItems": 594},
            "template_policy": {"type": "object"},
        },
    }


def write_templates(output_dir):
    output_dir.mkdir(parents=True, exist_ok=True)
    values = {
        CAPTURE_TEMPLATE_NAME: build_capture_template(),
        MANUAL_TEMPLATE_NAME: build_manual_template(),
        CAPTURE_SCHEMA_NAME: build_capture_schema(),
        MANUAL_SCHEMA_NAME: build_manual_schema(),
    }
    for name, value in values.items():
        (output_dir / name).write_text(json.dumps(value, indent=2, sort_keys=True) + "\n")


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--output-dir", type=Path, default=Path("docs/art"))
    args = parser.parse_args()
    write_templates(args.output_dir)
    print("Generated Phase3 GPU evidence templates: hud=360 shell=234 total=594 status=PENDING")


if __name__ == "__main__":
    main()
