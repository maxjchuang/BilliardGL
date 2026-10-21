#!/usr/bin/env python3
"""Validate Phase-5 rules decision UI machine evidence without promoting it."""

from __future__ import annotations

import argparse
import hashlib
import json
from pathlib import Path


ROOT = Path(__file__).resolve().parents[1]
GATE = ROOT / "docs/art/phase5-rules-decision-ui-design-gate-v1.json"
GATE_SHA = "26567d0547163561ed77e6e41b32753c4cc55efcd693c9ce22faf847d43f5a08"
STATES = (
    "group_choice_open_unselected", "group_choice_armed_confirm",
    "full_table_preview_valid", "full_table_preview_invalid",
    "behind_head_string_preview_valid", "behind_head_string_preview_invalid",
    "eight_legal_open_unselected", "eight_foul_open_unselected",
    "break_illegal_open_unselected", "break_shot_foul_open_unselected",
    "submitting_progress", "authority_error", "state_drift_refreshing",
    "ai_or_remote_pending", "section_6g_classification_pending",
)
LOCALES = ("en", "zh_cn")
PALETTES = ("standard", "color_vision_safe")
VIEWPORTS = (
    "1280x720_density1", "1920x1080_density1",
    "1280x720_retina_density2",
)
SCALES = (0.85, 1.0, 1.35)
READ_ONLY_BINDINGS = {
    "docs/art/phase5-rules-decision-ui-design-gate-v1.json": GATE_SHA,
    "docs/art/phase5-rules-decision-ui-actions-v1.pending.json":
        "7b3de569f57d1a8e589a6d053bf3959f04a5b68bbfb204046bfe8b63428383f1",
    "src/Billiards/rules/group_choice_authority.h":
        "95f6f76bdaddc5627723fe47fd22fa13bc17254a576af63073cba1496dabdf36",
    "src/Billiards/rules/ball_in_hand_authority.h":
        "026fd6196263b1c6dab180fa7f94d29baf277ff56bdff450ca189e7d8ba795d6",
    "src/Billiards/rules/eight_on_break_authority.h":
        "dacb83140b687b285f1dc67bcbb38d571349a60caa01a45664f0bbdcc2981b2e",
    "src/Billiards/rules/break_foul_authority.h":
        "5b4bf09f9950fd61d3d4add4fd70a9304d5c82ab05254f5878c208a6dd92ad9e",
    "src/Billiards/rules/rack_executor.h":
        "d42dc8ae486ad9df8911e4f82cb3d657c0e51e76b35082edb3fd909ee37ef242",
    "assets/modern/hud/focused_practice_hud_shipping_tokens_v1.json":
        "abf73aa283279c28677b5c97a9b02b3130b0d0125048c3124c85a9c0e72df860",
    "assets/modern/hud/focused_practice_shell_shipping_tokens_v1.json":
        "c144f1a25d3e7ced14e385bd6f76ca7e0f05b06597dcc8a4bacb4c570fc6bad3",
}
IMPLEMENTATION_BINDINGS = {
    "src/Billiards/rules/rules_decision_ui_model.h",
    "src/Billiards/rules/rules_decision_ui_model.cpp",
    "src/Billiards/render/ui/rules_decision_ui_draw_list.h",
    "src/Billiards/render/ui/rules_decision_ui_draw_list.cpp",
    "src/Billiards/render/ui/rules_decision_product_draw.h",
    "src/Billiards/render/ui/rules_decision_product_draw.cpp",
    "src/Billiards/render/modern/modern_product_session.h",
    "src/Billiards/render/modern/modern_product_session.cpp",
    "src/Billiards/render/ui/product_ui_frame_coordinator.h",
    "src/Billiards/render/ui/product_ui_frame_coordinator.cpp",
    "src/Billiards/render/modern/modern_product_ui_runtime.h",
    "src/Billiards/render/modern/modern_product_ui_runtime.cpp",
    "src/Billiards/render/modern/modern_renderer.h",
    "src/Billiards/render/modern/modern_renderer.cpp",
    "tests/rules_decision_ui_tests.cpp",
    "tests/rules_decision_product_runtime_tests.cpp",
    "tests/rules_decision_session_integration_tests.cpp",
    "spikes/render_core_profile/phase5_rules_decision_ui_gpu_capture_runner.mm",
    "scripts/prepare_phase5_rules_decision_ui_gpu_capture.py",
    "scripts/run_phase5_rules_decision_ui_gpu_capture.py",
}


def validate(value: object) -> list[str]:
    errors: list[str] = []
    if not isinstance(value, dict):
        return ["report root must be an object"]
    if hashlib.sha256(GATE.read_bytes()).hexdigest() != GATE_SHA:
        errors.append("bound design gate hash changed")
    if value.get("read_only_bindings") != READ_ONLY_BINDINGS:
        errors.append("read-only binding inventory differs")
    else:
        for relative, expected in READ_ONLY_BINDINGS.items():
            if hashlib.sha256((ROOT / relative).read_bytes()).hexdigest() != expected:
                errors.append("read-only binding changed: " + relative)
    implementation = value.get("implementation_bindings")
    if not isinstance(implementation, dict) or \
            set(implementation) != IMPLEMENTATION_BINDINGS:
        errors.append("implementation binding inventory differs")
    else:
        for relative, expected in implementation.items():
            if not isinstance(expected, str) or len(expected) != 64 or \
                    hashlib.sha256((ROOT / relative).read_bytes()).hexdigest() != expected:
                errors.append("implementation binding changed: " + relative)
    if value.get("schema") != "phase5_rules_decision_ui_machine_evidence_v1" or \
            value.get("schema_version") != 1 or \
            value.get("design_gate_sha256") != GATE_SHA:
        errors.append("machine report identity or gate binding differs")
    if value.get("status") != \
            "MACHINE_PASS_ONLY_NOT_GPU_NOT_HUMAN_NOT_PRODUCT_PASS" or \
            value.get("product_exposed") is not False or \
            value.get("profile_complete") is not False or \
            value.get("rules_owner_signature_present") is not False or \
            value.get("off_table_12_3_vs_17_e_status") != "UNRESOLVED":
        errors.append("evidence boundary or unresolved policy status differs")
    totals = value.get("totals", {})
    expected_totals = {
        "machine_required": 540, "machine_executed": 540,
        "machine_passed": 540, "actual_gpu_required": 60,
        "actual_gpu_completed": 0, "manual_required": 60,
        "manual_completed": 0, "beta_required": 12, "beta_completed": 0,
    }
    if totals != expected_totals:
        errors.append("machine/GPU/manual/Beta totals differ")
    boundary = value.get("evidence_boundaries", {})
    if boundary != {"actual_gpu_claimed": False, "manual_review_claimed": False,
                     "beta_claimed": False, "product_pass_claimed": False}:
        errors.append("forbidden GPU/human/Beta/product evidence is claimed")
    font = value.get("font_backend", {})
    if font.get("backend") != "existing_freetype_sdf_atlas" or \
            font.get("actual_glyph_advance_measurement") is not True or \
            not isinstance(font.get("glyph_rasterizations"), int) or \
            font.get("glyph_rasterizations", 0) <= 0 or \
            not isinstance(font.get("font_sha256"), str) or \
            len(font.get("font_sha256", "")) != 64:
        errors.append("actual FreeType/SDF glyph-measurement binding is incomplete")
    cases = value.get("cases")
    if not isinstance(cases, list) or len(cases) != 540:
        errors.append("case inventory must contain exactly 540 rows")
        cases = []
    expected = {
        (state, locale, palette, viewport, scale)
        for state in STATES for locale in LOCALES for palette in PALETTES
        for viewport in VIEWPORTS for scale in SCALES
    }
    observed: set[tuple[object, ...]] = set()
    ids: set[str] = set()
    for case in cases:
        if not isinstance(case, dict):
            errors.append("case row must be an object")
            continue
        case_id = case.get("case_id")
        if not isinstance(case_id, str) or case_id in ids:
            errors.append("case IDs must be non-empty and unique")
        else:
            ids.add(case_id)
        if case.get("passed") is not True:
            errors.append("every machine case must pass")
        observed.add((case.get("state"), case.get("locale"),
                      case.get("palette"), case.get("viewport_density"),
                      case.get("ui_scale")))
    if observed != expected:
        errors.append("540-case exhaustive cross product differs")
    checks = value.get("per_case_checks")
    if not isinstance(checks, list) or len(checks) != 11 or \
            len(set(checks)) != 11:
        errors.append("eleven per-case hard checks are not recorded")
    return errors


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("report", type=Path)
    args = parser.parse_args()
    try:
        value = json.loads(args.report.read_text(encoding="utf-8"))
    except (OSError, json.JSONDecodeError) as error:
        print(f"Phase5 rules decision UI machine validation failed: {error}")
        return 1
    errors = validate(value)
    if errors:
        for error in errors:
            print("ERROR: " + error)
        return 1
    print("Phase5 rules decision UI machine PASS: 540/540; actual GPU 0/60; "
          "manual 0/60; Beta 0/12; product_exposed=false; policy=UNRESOLVED")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
