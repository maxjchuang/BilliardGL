#!/usr/bin/env python3
"""Validate the Phase4 V19 dedicated trajectory inset design gate."""

from __future__ import annotations

import hashlib
import json
from pathlib import Path
import sys


ROOT = Path(__file__).resolve().parents[1]
DEFAULT = ROOT / "docs/art/phase4-v19-dedicated-trajectory-inset-minimap-product-decision-and-implementation-design-gate-v1.json"
SCHEMA = "billiardgl.phase4.v19.dedicated_trajectory_inset_minimap_product_decision_and_implementation_design_gate.v1"
STATUS = "ROOT_SELECTED_D_DEDICATED_INSET_DESIGN_ONLY_NO_PRODUCTION_NO_CMAKE_NO_GPU_NO_AUTH"
DECISION = "ROOT PRODUCT DECISION: SELECT D DEDICATED_TRAJECTORY_INSET_MINIMAP. A/B/C are not selected. Main-world overlay becomes secondary/diagnostic and no prior machine pass transfers. Design-only; no production/CMake/GPU/readiness/authorization."
DECISION_SHA = "a5c627db2ee53f299135ebd361784a228d2d026269321b4117f7288f38296c70"
BINDINGS = {
    "docs/art/phase4-v18-projected-near-coaxial-grammar-independent-ta-feasibility-failure-addendum-v1.json": "ec4f929a8c212ce8e25cbd7b040c00f511a5546918597ef0a3240cec1793b310",
    "docs/art/phase4-v18-projected-near-coaxial-grammar-feasibility-diagnostic-v1/feasibility-receipt-v1.json": "664d7b10875202571ad869fc477b5fd35b27994280217ad02ebff08e0ffe0398",
    "docs/art/phase4-v17-explicit-crossing-grammar-independent-ta-feasibility-failure-addendum-v1.json": "150d1d8f1cb3cf0b43935a01a795d56ceadc51a6df330d0a68e4d01f51a36b3c",
    "docs/art/phase4-v16-local-cross-track-actual-continuity-independent-ta-implementation-failure-addendum-v1.json": "6ccbb8fc7a149c146b755709a41558bd98a2592cf353f7a639660f1c910ce8ef",
    "docs/art/phase4-v12-broadcast-projected-trajectory-visual-repair-design-gate-v1.json": "0fe6d893cd8fa1130e93c975be8e297802749b6515eb4f4e65b7d9af2006b8d2",
    "docs/art/phase4-v15-independent-ta-diagnostic-acceptance-decision-gate-v1.json": "2628104e6d219a98fb38fa599058ec3dd22f11c658d7fe3aa1ffc82850bcf23b",
    "docs/art/phase4-v17-explicit-non-occluding-crossing-grammar-product-decision-gate-v1.json": "c08b7f2c15fa9f53c0ecc76c7c44f07265500b520dfa377097568296d56d8329",
}
SOURCES = {
    "src/Billiards/training/trajectory_overlay_renderer.cpp": "0c108c506641ab4828290a1a4c7b6b5ebbd51ab4fc7c12845bde0f5deb18a840",
    "src/Billiards/training/trajectory_overlay_renderer.h": "9805e6a5f56c17d7645200576505425cddf0d642970e5dc5cec186e003bd564e",
    "src/Billiards/training/trajectory_overlay_model.cpp": "9307cfd2be139d14f6df12842f31eca6a714cede506c053046b64d047411cfc3",
    "src/Billiards/training/trajectory_overlay_model.h": "176049fc73e2558e3ea96ed1e2cc4fe67c11c1f771b19752b2a9cf32f8d24869",
    "src/Billiards/render/modern/modern_renderer.cpp": "b00cc6b370122275a42f106967f935dbb34bd5d3f81f2a750ca9ba6e503af191",
    "src/Billiards/render/modern/modern_renderer.h": "936de5b77c9e3485f2e51f10e9de26c59c8ec94674865e0b50ae125725183c9d",
    "src/Billiards/render/modern/modern_product_session.cpp": "d7a3d4e88e3ec8723d78b7ed37808463fbfe9f0e005e71b409c82d4a3e7cb5e9",
    "src/Billiards/render/modern/modern_product_session.h": "bc7862aac74640bd9fb2aa5929eddc3601b89d5e17b5cd047c8fd8e5578f003d",
    "src/Billiards/render/modern/modern_renderer_product_ui.cpp": "0b9a5a77191488769c9a5f83674dca7a5f9fad657e5fc0ac6f0c5964c4c8bdbb",
    "src/Billiards/render/modern/modern_renderer_product_ui.h": "bdbd209193535ce08f9e7503dc4f6c6e4e2224be71e169222d61e7885672a213",
    "src/Billiards/product_hud.cpp": "10d23219274a8c7dfbfcd96b54c4f8fd972e1a46df29fde41633c1a97d76004c",
    "src/Billiards/product_hud.h": "3e00ba459451b16141a488672285de54ffdf92eeb19f72aed12d31c5566470f5",
    "src/Billiards/render/ui/product_shell_layout.cpp": "1cd65911f93f72f1fb4adf353c401047832b2b46b24372cf14fb1ebdbfa250fc",
    "src/Billiards/render/ui/product_shell_layout.h": "5a9b8836a133baf42c788b92bcd2a3f212081995a7eb2af3c100f78667a7312e",
    "src/Billiards/render/ui/product_shell_draw_list.cpp": "c0ce953eb5f8c72368540dd2128ca5f60304371812213894d947adf1f8b4f07b",
    "src/Billiards/render/ui/product_shell_draw_list.h": "daf31b57263a81b0e041be890ead7fed1e44b95d2dc0336b7e6c9017ed79da2c",
    "CMakeLists.txt": "f41563c35224dd809f64acedba6455355e70082697c41305c2d1b05426f71d41",
}
PASS_ORDER = [
    "main_hdr_opaque", "optional_secondary_world_overlay", "main_color_and_world_semantic_resolve",
    "main_aces_tone_map_to_ldr", "inset_hdr_and_inset_semantic_clear", "inset_table_and_trajectory_draw",
    "inset_color_and_per_sample_semantic_resolve", "inset_fixed_palette_tone_encode",
    "product_ui_base_hud_with_reserved_inset_rect", "inset_panel_composite_to_main_ldr",
    "inset_legend_toggle_and_focus_ui", "present",
]
CAMERAS = ["player_low", "elevated", "overhead", "broadcast"]
PALETTES = ["standard", "color_vision_safe"]
SAMPLES = ["center_1x", "asymmetric_2x", "asymmetric_4x"]
LAYOUTS = ["1920x1080_ui100", "1920x1080_ui135", "1280x720_ui100", "1280x720_ui135"]


def sha(path: Path) -> str:
    return hashlib.sha256(path.read_bytes()).hexdigest()


def validate(path: Path = DEFAULT) -> list[str]:
    errors: list[str] = []
    try:
        value = json.loads(path.read_text(encoding="utf-8"))
    except Exception as error:
        return [f"gate_invalid:{error}"]

    def require(condition: bool, message: str) -> None:
        if not condition:
            errors.append(message)

    require(value.get("schema") == SCHEMA and value.get("schema_version") == 1 and value.get("status") == STATUS, "schema_or_status_invalid")
    require("design-only" in value.get("scope", "") and "does not authorize or perform production" in value.get("scope", ""), "scope_invalid")
    decision = value.get("root_product_decision_binding", {})
    require(decision.get("decision_id") == "DEDICATED_TRAJECTORY_INSET_MINIMAP" and decision.get("option") == "D", "root_decision_invalid")
    require(decision.get("statement") == DECISION and decision.get("statement_sha256") == DECISION_SHA and hashlib.sha256(DECISION.encode()).hexdigest() == DECISION_SHA, "root_statement_binding_invalid")
    require(decision.get("selection_authorizes_design_only") is True, "decision_scope_invalid")
    options = value.get("option_disposition", {})
    require(options == {
        "D_DEDICATED_TRAJECTORY_INSET_MINIMAP": "SELECTED",
        "A_SCREEN_SPACE_LANE_SEPARATION": "NOT_SELECTED",
        "B_TEMPORAL_PHASE_ALTERNATION": "NOT_SELECTED",
        "C_TRUNCATION_OR_PRIORITY_DISCLOSURE": "NOT_SELECTED",
        "unlisted_automatic_fallback_allowed": False,
    }, "option_disposition_invalid")

    bindings = {item.get("path"): item.get("sha256") for item in value.get("frozen_bindings", [])}
    require(bindings == BINDINGS, "binding_inventory_or_sha_invalid")
    for relative, digest in BINDINGS.items():
        require((ROOT / relative).is_file() and sha(ROOT / relative) == digest, f"binding_missing_or_drift:{relative}")
    observed_sources = {item.get("path"): item.get("sha256") for item in value.get("current_source_hashes_no_edit", {}).values()}
    require(observed_sources == SOURCES, "source_inventory_or_sha_invalid")
    for relative, digest in SOURCES.items():
        require((ROOT / relative).is_file() and sha(ROOT / relative) == digest, f"source_missing_or_drift:{relative}")

    product = value.get("product_semantics", {})
    require(product.get("default_inset_enabled") is True and product.get("default_world_overlay_enabled") is False, "default_toggle_state_invalid")
    require(product.get("world_overlay_role") == "SECONDARY_DIAGNOSTIC_OPTIONAL" and product.get("world_and_inset_toggles_are_independent") is True, "world_secondary_contract_invalid")
    require(product.get("no_automatic_world_fallback_when_inset_fails") is True, "world_fallback_invalid")
    require(product.get("prior_world_machine_ta_or_visual_pass_inherited_by_inset") is False and product.get("prior_world_machine_pass_remains_product_authority") is False, "prior_world_pass_inherited")
    require(product.get("rules_or_physics_authority_changed") is False and product.get("camera_state_changed") is False and product.get("route_information_deleted_or_temporally_hidden") is False, "authority_or_route_boundary_invalid")

    layout = value.get("default_layout_and_ui_contract", {})
    require(layout.get("reference_viewport_px") == [1920, 1080] and layout.get("reference_outer_panel_rect_px") == [1440, 680, 448, 288], "reference_layout_invalid")
    require(layout.get("reference_plot_rect_within_panel_px") == [16, 48, 416, 208] and layout.get("minimum_outer_panel_px") == [336, 216], "plot_or_minimum_layout_invalid")
    require(layout.get("default_anchor") == "bottom_right_above_product_footer" and layout.get("legend_always_visible_when_inset_visible") is True, "anchor_or_legend_invalid")
    require(layout.get("legend_entries_in_order") == ["ACTUAL / 实际 — paired continuous rails", "PREDICTED / 预测 — centerline dashes", "END / 终点 — endpoint marker"], "legend_entries_invalid")
    require(layout.get("minimum_legend_text_px_at_reference") == 16 and layout.get("minimum_toggle_target_px") == [44, 44], "legend_or_toggle_size_invalid")
    require("suppress" in layout.get("modal_behavior", "").lower() and "do not overlap" in layout.get("layout_collision_behavior", ""), "layout_fail_closed_invalid")

    grammar = value.get("inset_visual_grammar", {})
    require("Camera-independent top-down" in grammar.get("projection", "") and "never inputs" in grammar.get("projection", ""), "camera_independence_invalid")
    require("Two continuous symmetric" in grammar.get("actual_track", "") and "midpoint is the unmodified route" in grammar.get("actual_track", ""), "actual_twin_rail_invalid")
    require("exact canonical centerline" in grammar.get("predicted_track", "") and "dashed core" in grammar.get("predicted_track", ""), "predicted_centerline_invalid")
    require("no source route is erased, clipped, recolored or time-sliced" in grammar.get("overlap_behavior", ""), "overlap_non_occluding_invalid")
    require(grammar.get("non_color_redundancy_is_primary") is True and grammar.get("palette_color_is_not_semantic_identity") is True and grammar.get("main_world_threshold_offset_width_cap_z_order_or_camera_workaround_allowed") is False, "visual_identity_or_workaround_invalid")

    semantic = value.get("independent_semantic_plane_contract", {})
    require(semantic.get("plane_id") == "trajectory_inset_semantic_v1" and "never reused" in semantic.get("token_namespace", ""), "semantic_plane_or_namespace_invalid")
    require("common parent source key" in semantic.get("mapping_ledger_row", "") and semantic.get("world_visibility_is_not_inset_eligibility") is True and semantic.get("camera_or_world_clip_cannot_remove_inset_denominator") is True, "mapping_or_denominator_invalid")
    require("Every canonical actual or predicted" in semantic.get("inset_source_group_denominator", "") and "All required inset roles" in semantic.get("inset_semantic_member_denominator", ""), "denominator_definition_invalid")
    require("InsetTrajectoryProbe" in semantic.get("inset_probe_denominator", "") and "future CPU implementation receipt" in semantic.get("ordered_probe_count_and_digest", ""), "probe_inventory_invalid")
    require(semantic.get("main_world_and_inset_denominators_may_be_combined") is False and semantic.get("historical_81_probe_pass_inherited") is False, "denominator_combined_or_81_inherited")

    rendering = value.get("render_resource_and_pass_contract", {})
    require(rendering.get("architecture_decision") == "OFFSCREEN_ATOMIC_INSET_COLOR_AND_SEMANTIC_PLANE", "architecture_invalid")
    require("partially contaminate" in rendering.get("direct_hdr_scene_fbo_rejected_reason", "") and "independent inset plane" in rendering.get("direct_hdr_scene_fbo_rejected_reason", ""), "direct_hdr_rejection_invalid")
    resources = rendering.get("required_independent_resources", [])
    require(len(resources) == 6 and any("R32UI" in item for item in resources) and any("tone-encoded" in item for item in resources), "independent_resource_inventory_invalid")
    require(rendering.get("exact_pass_order") == PASS_ORDER, "pass_order_invalid")
    require(rendering.get("no_framebuffer_feedback") is True and rendering.get("no_main_depth_or_world_semantic_attachment_reuse") is True, "attachment_isolation_invalid")
    require(rendering.get("single_sample_and_msaa_resolve_paths_are_distinct") is True and rendering.get("sample_counts_supported_exactly") == [1, 2, 4], "sample_resolve_contract_invalid")

    gl = value.get("gl_fail_closed_contract", {})
    checks = gl.get("required_checks", [])
    require(len(checks) == 8 and any("glGetError" in item for item in checks) and any("framebuffer completeness" in item for item in checks), "gl_check_inventory_invalid")
    require("zero inset pixels/tokens" in gl.get("required_consecutive_frame_negative", "") and "N mappings or textures" in gl.get("required_consecutive_frame_negative", ""), "consecutive_frame_negative_invalid")
    require(gl.get("silent_sample_count_downgrade_allowed") is False and gl.get("stale_texture_or_semantic_fail_open_allowed") is False and gl.get("world_overlay_fallback_allowed") is False, "gl_fail_open_invalid")
    require("Abort both inset trajectory and inset legend composite" in gl.get("on_any_failure", ""), "atomic_failure_behavior_invalid")

    cpu = value.get("future_cpu_static_acceptance_not_executed", {})
    require(cpu.get("camera_presets_in_order") == CAMERAS and cpu.get("palettes_in_order") == PALETTES and cpu.get("sample_profiles_in_order") == SAMPLES, "cpu_semantic_matrix_invalid")
    require(cpu.get("layout_profiles_in_order") == LAYOUTS and cpu.get("exact_semantic_case_count_per_layout_profile") == 24 and cpu.get("exact_total_case_count") == 96, "cpu_layout_matrix_invalid")
    require(cpu.get("case_order") == "layout profile major, then camera, palette and sample profile", "cpu_case_order_invalid")
    invariants = cpu.get("required_invariants", [])
    negatives = cpu.get("required_fail_closed_fixtures", [])
    require(len(invariants) == 8 and any("camera-independent" in item for item in invariants) and any("continuously traceable" in item for item in invariants), "cpu_invariants_invalid")
    require(len(negatives) == 14 and any("historical 81" in item for item in negatives) and any("stale texture" in item for item in negatives), "cpu_negative_inventory_invalid")
    require(cpu.get("cpu_machine_pass") is False and cpu.get("static_scope_pass") is False, "cpu_false_pass")

    future = value.get("future_targeted_gpu_ta_and_human_gates_not_authorized", {})
    require(future.get("gpu_requires_separate_readiness_and_single_use_authorization") is True and len(future.get("independent_ta_axes", [])) == 8 and len(future.get("human_comprehension_questions", [])) == 3, "future_gate_contract_invalid")
    require(future.get("gpu_pass") is False and future.get("ta_pass") is False and future.get("human_comprehension_pass") is False, "future_false_pass")
    boundary = value.get("candidate_future_implementation_file_boundary_not_authorized", {})
    require(len(boundary.get("new_files", [])) == 5 and len(boundary.get("existing_file_categories", [])) == 5 and len(boundary.get("explicitly_forbidden_categories", [])) == 4, "future_file_boundary_invalid")
    require(all("only" in item or item.startswith("CMake") for item in boundary.get("existing_file_categories", [])), "future_existing_scope_not_bounded")

    auth = value.get("authorization_boundary", {})
    require(auth.get("root_product_decision_selected") is True and auth.get("design_gate_complete") is True, "design_decision_not_frozen")
    require(len(auth) == 19 and all(flag is False for key, flag in auth.items() if key not in {"root_product_decision_selected", "design_gate_complete"}), "authorization_boundary_invalid")
    proof = value.get("proof_boundary", {})
    require(proof.get("product_decision_and_implementation_design_gate_only") is True and len(proof) == 11 and all(flag is False for key, flag in proof.items() if key != "product_decision_and_implementation_design_gate_only"), "proof_boundary_invalid")
    return errors


def main() -> int:
    target = Path(sys.argv[1]).resolve() if len(sys.argv) > 1 else DEFAULT
    errors = validate(target)
    if errors:
        for error in errors:
            print("FAIL: " + error, file=sys.stderr)
        return 1
    print("PASS_PHASE4_V19_DEDICATED_TRAJECTORY_INSET_PRODUCT_DECISION_AND_DESIGN_GATE")
    print("architecture=offscreen_atomic cases=96 production=false cmake=false gpu=false readiness=false auth=false")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
