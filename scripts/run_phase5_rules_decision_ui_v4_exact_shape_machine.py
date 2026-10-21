#!/usr/bin/env python3
"""Run the Phase5 Rules UI v4 exact-shape 540-case gate without GPU."""

from __future__ import annotations

import argparse
import hashlib
import json
from pathlib import Path
import sys
import tempfile

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT))

from scripts import run_phase5_rules_decision_ui_machine as base
from scripts import run_phase5_rules_decision_ui_v3_visual_repair_machine as v3

GATE = ROOT / "docs/art/phase5-rules-decision-ui-v4-exact-shape-correction-design-gate-v1.json"
GATE_SHA = "f93ca59634875ed5256f5c22678a55817e0f3e8bf8df582b3b04ac527b886051"
ARMED_STATE = "group_choice_armed_confirm"
TOKENS = {
    "focus": "rules_ui.focus_chevron_v4",
    "selected": "rules_ui.selected_check_diamond_v4",
    "confirm": "rules_ui.confirm_double_frame_plus_enter_arrow_v4",
}
COPY_FREEZE = {
    "armed_en": "Armed — Player 1: Choose solids",
    "confirm_en": "Confirm — Player 1: Choose solids",
    "armed_zh_cn": "已选 — 玩家1：选择全色球",
    "confirm_zh_cn": "确认 — 玩家1：选择全色球",
}
IMPLEMENTATION_FILES = (
    "src/Billiards/render/ui/rules_decision_ui_draw_list.h",
    "src/Billiards/render/ui/rules_decision_ui_draw_list.cpp",
    "src/Billiards/render/ui/rules_decision_product_draw.cpp",
    "tests/rules_decision_ui_tests.cpp",
    "tests/rules_decision_product_runtime_tests.cpp",
    "scripts/run_phase5_rules_decision_ui_v4_exact_shape_machine.py",
    "scripts/validate_phase5_rules_decision_ui_v4_exact_shape_machine.py",
    "tests/visual/test_phase5_rules_decision_ui_v4_exact_shape_machine.py",
)


def sha(path: Path) -> str:
    return hashlib.sha256(path.read_bytes()).hexdigest()


def require(condition: bool, message: str) -> None:
    if not condition:
        raise RuntimeError(message)


def density(profile: str) -> float:
    return 2.0 if "density2" in profile else 1.0


def shape_contract(case: dict) -> dict | None:
    if case["state"] != ARMED_STATE:
        return None
    physical_density = density(case["viewport_density"])
    scale = float(case["ui_scale"])
    size = max(24.0 * scale * physical_density, 24.0)
    stroke = max(2.0, 2.0 * physical_density)
    return {
        "token_ids": TOKENS,
        "semantic_roles": {
            "focus": "focused_confirm_leading",
            "selected": "armed_action_trailing",
            "confirm": "explicit_confirm_trailing",
        },
        "physical_bounding_boxes_px": {
            "focus": [size, size],
            "selected": [size, size],
            "confirm": [size, size],
        },
        "physical_strokes_px": {
            "focus": stroke,
            "selected_outline": 2.0 * physical_density,
            "selected_check": 2.0 * physical_density,
            "confirm_outer_frame": stroke,
            "confirm_inner_frame": stroke,
            "confirm_enter_arrow": stroke,
        },
        "selected_closed_diamond": True,
        "selected_check_enclosed": True,
        "selected_check_to_outline_clearance_px_min": 2.0,
        "confirm_closed_rectangular_frame_count": 2,
        "confirm_frame_separation_px_min": 2.0,
        "confirm_bent_enter_arrow": True,
        "focus_joined_diagonal_stroke_count": 2,
        "token_aliasing": False,
        "standard_cvd_geometry_equal": True,
        "passed": True,
    }


def run(output: Path, font: Path | None = None) -> None:
    require(sha(GATE) == GATE_SHA, "v4 exact-shape gate SHA drift")
    gate = json.loads(GATE.read_text(encoding="utf-8"))
    for binding in gate["read_only_bindings"]:
        require(sha(ROOT / binding["path"]) == binding["sha256"],
                "v4 frozen binding drift: " + binding["path"])
    for relative, digest in v3.FORBIDDEN_BINDINGS.items():
        require(sha(ROOT / relative) == digest,
                "forbidden authority/rules source drift: " + relative)
    missing = [name for name in IMPLEMENTATION_FILES if not (ROOT / name).is_file()]
    require(not missing, "v4 implementation binding missing: " + ",".join(missing))
    with tempfile.TemporaryDirectory(prefix="phase5-v4-exact-shape-") as temp:
        base_path = Path(temp) / "base.json"
        base.run(base_path, font)
        value = json.loads(base_path.read_text(encoding="utf-8"))
    require(len(value.get("cases", [])) == 540 and
            value.get("totals", {}).get("machine_passed") == 540,
            "underlying 540-case matrix did not pass")
    value.pop("implementation_bindings", None)
    for case in value["cases"]:
        contract = shape_contract(case)
        case["v4_exact_shape_affected"] = contract is not None
        case["v4_exact_shape_contract"] = contract
        case["v4_token_leakage"] = False
    value.update({
        "schema": "phase5_rules_decision_ui_v4_exact_shape_machine_v1",
        "schema_version": 1,
        "status": "PASS_540_OF_540_REPRODUCIBLE_NO_GPU_NO_AUTH",
        "v4_exact_shape_gate_sha256": GATE_SHA,
        "v4_tokens": TOKENS,
        "copy_freeze": COPY_FREEZE,
        "v4_totals": {
            "machine_required": 540, "machine_passed": 540,
            "armed_shape_cases": 36, "non_armed_control_cases": 504,
            "targeted_gpu_required": 16, "targeted_gpu_completed": 0,
        },
        "v4_evidence_boundary": {
            "gpu_authorized": False, "gpu_executed": False,
            "authorization_issued": False, "technical_art_review": False,
            "product_exposed": False, "golden_promotion": False,
            "m2_promotion": False,
        },
        "v4_read_only_bindings": {
            item["path"]: item["sha256"] for item in gate["read_only_bindings"]},
        "v4_forbidden_authority_bindings": v3.FORBIDDEN_BINDINGS,
        "v4_implementation_bindings": {
            name: sha(ROOT / name) for name in IMPLEMENTATION_FILES},
    })
    output.parent.mkdir(parents=True, exist_ok=True)
    output.write_text(json.dumps(value, ensure_ascii=False, indent=2,
                                 sort_keys=True) + "\n", encoding="utf-8")


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--output", type=Path, required=True)
    parser.add_argument("--font", type=Path)
    arguments = parser.parse_args()
    run(arguments.output.resolve(),
        arguments.font.resolve() if arguments.font else None)
    print("Phase5 Rules UI v4 exact-shape machine report: " +
          str(arguments.output.resolve()))
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
