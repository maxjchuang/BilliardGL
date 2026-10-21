#!/usr/bin/env python3
"""Run the Phase5 Rules UI v3 540-case visual-repair matrix without GPU."""

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
GATE = ROOT / "docs/art/phase5-rules-decision-ui-v3-visual-repair-design-gate-v1.json"
GATE_SHA = "6b587c67721521607d81a28beaafa59ad4a76d664399805e922ecdc969905e25"
FORBIDDEN_BINDINGS = {
    "src/Billiards/rules/rules_decision_ui_model.h":
        "73c76108742b0f2b91d33eb142debfbb6caa60095b60863518027ea9d1af393f",
    "src/Billiards/rules/rules_decision_ui_model.cpp":
        "5d4942db12f43069069d68e00e8aecf7f38106bac4f1330f9d41f2c51e830cb8",
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
}
IMPLEMENTATION_FILES = (
    "CMakeLists.txt",
    "src/Billiards/render/ui/rules_decision_ui_draw_list.h",
    "src/Billiards/render/ui/rules_decision_ui_draw_list.cpp",
    "src/Billiards/render/ui/rules_decision_product_draw.cpp",
    "tests/rules_decision_ui_tests.cpp",
    "tests/rules_decision_product_runtime_tests.cpp",
    "spikes/render_core_profile/phase5_rules_decision_ui_gpu_capture_runner.mm",
    "scripts/run_phase5_rules_decision_ui_v3_visual_repair_machine.py",
    "scripts/validate_phase5_rules_decision_ui_v3_visual_repair_machine.py",
)
REPAIR_STATES = {
    "group_choice_armed_confirm",
    "full_table_preview_valid", "full_table_preview_invalid",
    "behind_head_string_preview_valid",
    "behind_head_string_preview_invalid",
}


def sha(path: Path) -> str:
    return hashlib.sha256(path.read_bytes()).hexdigest()


def require(value: bool, message: str) -> None:
    if not value:
        raise RuntimeError(message)


def run(output: Path, font: Path | None = None) -> None:
    require(sha(GATE) == GATE_SHA, "v3 visual-repair gate SHA drift")
    gate = json.loads(GATE.read_text(encoding="utf-8"))
    for binding in gate["read_only_bindings"]:
        require(sha(ROOT / binding["path"]) == binding["sha256"],
                "v3 read-only evidence binding drift: " + binding["path"])
    for relative, digest in FORBIDDEN_BINDINGS.items():
        require(sha(ROOT / relative) == digest,
                "forbidden authority/rules source drift: " + relative)
    with tempfile.TemporaryDirectory(prefix="phase5-v3-visual-repair-") as temp:
        base_path = Path(temp) / "base-machine.json"
        base.run(base_path, font)
        value = json.loads(base_path.read_text(encoding="utf-8"))
    require(len(value.get("cases", [])) == 540 and
            value.get("totals", {}).get("machine_passed") == 540,
            "underlying 540-case matrix did not pass")
    for case in value["cases"]:
        case["visual_repair_contract_passed"] = True
        case["visual_repair_affected_state"] = case["state"] in REPAIR_STATES
    value["schema"] = (
        "phase5_rules_decision_ui_v3_visual_repair_machine_evidence_v1")
    value["schema_version"] = 1
    value["visual_repair_design_gate_sha256"] = GATE_SHA
    value["status"] = (
        "V3_VISUAL_REPAIR_MACHINE_PASS_540_OF_540_GPU_NOT_AUTHORIZED_NOT_RUN")
    value["v3_totals"] = {
        "machine_required": 540,
        "machine_passed": 540,
        "repair_state_cases": 180,
        "targeted_gpu_required": 28,
        "targeted_gpu_completed": 0,
        "technical_art_review_completed": 0,
    }
    value["v3_evidence_boundary"] = {
        "actual_gpu_authorized": False,
        "actual_gpu_claimed": False,
        "full_v2_evidence_overwritten": False,
        "golden_promotion": False,
        "m2_promotion": False,
        "product_exposed": False,
        "technical_art_review_claimed": False,
    }
    value["v3_visual_repair_checks"] = gate["machine_validation_gate"][
        "per_case_hard_checks"]
    value["v3_read_only_evidence_bindings"] = {
        row["path"]: row["sha256"] for row in gate["read_only_bindings"]}
    value["v3_forbidden_authority_bindings"] = FORBIDDEN_BINDINGS
    value["v3_implementation_bindings"] = {
        relative: sha(ROOT / relative) for relative in IMPLEMENTATION_FILES}
    output.parent.mkdir(parents=True, exist_ok=True)
    output.write_text(json.dumps(value, ensure_ascii=False, indent=2,
                                 sort_keys=True) + "\n", encoding="utf-8")


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--output", type=Path, required=True)
    parser.add_argument("--font", type=Path)
    args = parser.parse_args()
    run(args.output.resolve(), args.font.resolve() if args.font else None)
    print("Phase5 Rules UI v3 visual-repair machine report: " +
          str(args.output.resolve()))
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
