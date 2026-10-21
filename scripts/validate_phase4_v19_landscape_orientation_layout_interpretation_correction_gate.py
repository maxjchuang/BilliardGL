#!/usr/bin/env python3
"""Validate the Phase4 V19 landscape orientation correction gate."""

from __future__ import annotations

import hashlib
import json
from pathlib import Path
import sys


ROOT = Path(__file__).resolve().parents[1]
DEFAULT = ROOT / "docs/art/phase4-v19-landscape-orientation-layout-interpretation-correction-gate-v1.json"
SCHEMA = "billiardgl.phase4.v19.landscape_orientation_layout_interpretation_correction_gate.v1"
STATUS = "ROOT_SELECTED_LANDSCAPE_HEAD_LEFT_FOOT_RIGHT_DESIGN_CORRECTION_ONLY_NO_PRODUCTION_NO_CMAKE_NO_GPU_NO_AUTH"
ORIGINAL = ROOT / "docs/art/phase4-v19-dedicated-trajectory-inset-minimap-product-decision-and-implementation-design-gate-v1.json"
ORIGINAL_SHA = "c7d86d9dcdd4891fac9dd9cbfffe3931561b5098db1df5a4aaec9b50e79af4c7"
CONTRADICTION_SHA = "3f8b41f9d7fa8d24bca45d68b21b328a2552b7029e35c4bf3ae909dc768b48e0"
DECISION_SHA = "0472f50e9c4b4c8e39b2a83f79204dab9f615415eaa48281a273b800edb94337"


def sha(path: Path) -> str:
    return hashlib.sha256(path.read_bytes()).hexdigest()


def validate(path: Path = DEFAULT) -> list[str]:
    errors: list[str] = []
    try:
        value = json.loads(path.read_text(encoding="utf-8"))
    except Exception as error:
        return [f"correction_gate_invalid:{error}"]

    def require(condition: bool, message: str) -> None:
        if not condition:
            errors.append(message)

    require(value.get("schema") == SCHEMA and value.get("schema_version") == 1 and value.get("status") == STATUS, "schema_or_status_invalid")
    original = value.get("frozen_original_gate", {})
    require(original.get("path") == str(ORIGINAL.relative_to(ROOT)) and original.get("sha256") == ORIGINAL_SHA, "original_binding_invalid")
    require(ORIGINAL.is_file() and sha(ORIGINAL) == ORIGINAL_SHA, "original_gate_missing_or_drift")
    contradiction = value.get("nonfinal_contradiction_binding", {})
    require(hashlib.sha256(contradiction.get("statement", "").encode()).hexdigest() == contradiction.get("statement_sha256") == CONTRADICTION_SHA, "contradiction_statement_invalid")
    require(contradiction.get("evidence_kind") == "NONFINAL_CP1_FAIL_CLOSED_STATIC_FINDING_NO_RECEIPT_NO_PASS" and contradiction.get("source_or_binary_frozen") is False and contradiction.get("implementation_result_inferred") is False, "contradiction_boundary_invalid")
    decision = value.get("root_orientation_decision_binding", {})
    require(hashlib.sha256(decision.get("statement", "").encode()).hexdigest() == decision.get("statement_sha256") == DECISION_SHA, "root_decision_statement_invalid")
    require(decision.get("option") == "A_LANDSCAPE_CANONICAL_TWO_TO_ONE" and decision.get("selected") is True and decision.get("portrait_letterbox_not_selected") is True and decision.get("anisotropic_distortion_forbidden") is True, "root_selection_invalid")
    supersession = value.get("supersession_contract", {})
    require(supersession.get("superseded_original_field") == "default_layout_and_ui_contract.plot_aspect_ratio" and "head rail at bottom" in supersession.get("superseded_text", ""), "superseded_field_invalid")
    require("head rail is LEFT" in supersession.get("replacement", "") and "foot rail is RIGHT" in supersession.get("replacement", ""), "replacement_orientation_invalid")
    require(supersession.get("all_other_v19_product_semantic_render_resource_gl_cpu_gpu_ta_human_and_authorization_contracts_unchanged") is True and supersession.get("original_gate_may_be_used_without_this_correction") is False, "supersession_boundary_invalid")

    orientation = value.get("corrected_orientation_contract", {})
    require(orientation.get("canonical_table_dimensions_cm") == [254.0, 127.0], "canonical_dimensions_invalid")
    require(orientation.get("head_rail_canonical_z_cm") == -127.0 and orientation.get("head_rail_screen_side") == "LEFT", "head_orientation_invalid")
    require(orientation.get("foot_rail_canonical_z_cm") == 127.0 and orientation.get("foot_rail_screen_side") == "RIGHT", "foot_orientation_invalid")
    require(orientation.get("canonical_x_negative_screen_side") == "TOP" and orientation.get("canonical_x_positive_screen_side") == "BOTTOM", "short_axis_orientation_invalid")
    require("screen_x" in orientation.get("mapping_formula", "") and "screen_y" in orientation.get("mapping_formula", ""), "mapping_formula_invalid")
    require("absolute epsilon 1e-6" in orientation.get("uniform_scale_required", "") and orientation.get("camera_matrix_name_or_scene_content_may_affect_orientation") is False and orientation.get("mirror_rotate_or_auto_flip_by_player_side_allowed") is False and orientation.get("anisotropic_stretch_allowed") is False, "uniform_scale_or_flip_invalid")

    gutter = value.get("reference_gutter_contract", {})
    require(gutter.get("plot_rect_within_panel_px") == [16, 48, 416, 208] and gutter.get("minimum_silhouette_clearance_px_at_reference") == 12, "plot_or_clearance_invalid")
    require(gutter.get("reference_canonical_table_rect_within_plot_px") == [24, 12, 368, 184] and gutter.get("reference_plot_margins_left_top_right_bottom_px") == [24, 12, 24, 12], "reference_gutter_invalid")
    require(368 / 184 == 2.0 and gutter.get("reference_table_aspect_ratio") == "368:184 == 2:1", "reference_aspect_invalid")
    require("largest exact 2:1" in gutter.get("responsive_algorithm", "") and "split equally" in gutter.get("responsive_algorithm", ""), "responsive_algorithm_invalid")
    require("must remain inside the plot rectangle" in gutter.get("complete_silhouette_must_remain_inside_plot", "") and gutter.get("shrinking_below_original_minimum_outer_panel_allowed") is False and gutter.get("scene_content_driven_relocation_allowed") is False, "silhouette_or_layout_boundary_invalid")

    labels = value.get("orientation_labels_contract", {})
    require(labels.get("left_label") == "HEAD / 开球端" and labels.get("right_label") == "FOOT / 底端", "bilingual_labels_invalid")
    require(labels.get("labels_visible_when_inset_visible") is True and labels.get("labels_are_outside_trajectory_denominator") is True and labels.get("labels_do_not_cover_canonical_table_rect") is True and labels.get("standard_and_cvd_geometry_identical") is True, "label_semantics_invalid")
    require(labels.get("screen_reader_names_in_order") == ["Head rail, 开球端", "Foot rail, 底端"], "screen_reader_names_invalid")
    cpu = value.get("cp1_cpu_acceptance_delta_not_executed", {})
    require(cpu.get("existing_exact_96_case_matrix_unchanged") is True and len(cpu.get("required_new_positive_fixtures", [])) == 5 and len(cpu.get("required_new_fail_closed_fixtures", [])) == 6, "cpu_fixture_inventory_invalid")
    require(any("different physical scale" in item for item in cpu.get("required_new_fail_closed_fixtures", [])) and any("392x184" in item for item in cpu.get("required_new_fail_closed_fixtures", [])), "distortion_negative_missing")
    require(cpu.get("cpu_checkpoint_pass") is False and cpu.get("implementation_pass") is False, "cpu_false_pass")
    auth = value.get("authorization_boundary", {})
    require(len(auth) == 13 and auth.get("orientation_product_decision_selected") is True and auth.get("interpretation_correction_gate_complete") is True and all(flag is False for key, flag in auth.items() if key not in {"orientation_product_decision_selected", "interpretation_correction_gate_complete"}), "authorization_boundary_invalid")
    proof = value.get("proof_boundary", {})
    require(len(proof) == 8 and proof.get("design_interpretation_only") is True and all(flag is False for key, flag in proof.items() if key != "design_interpretation_only"), "proof_boundary_invalid")
    return errors


def main() -> int:
    target = Path(sys.argv[1]).resolve() if len(sys.argv) > 1 else DEFAULT
    errors = validate(target)
    if errors:
        for error in errors:
            print("FAIL: " + error, file=sys.stderr)
        return 1
    print("PASS_PHASE4_V19_LANDSCAPE_ORIENTATION_LAYOUT_INTERPRETATION_CORRECTION_GATE")
    print("orientation=head_left_foot_right table=368x184 gutter=24,12,24,12 cpu=false production=false gpu=false")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
