#!/usr/bin/env python3
"""Freeze the Phase-5 60-case actual-GPU capture package without running it."""

from __future__ import annotations

import argparse
import hashlib
import json
from pathlib import Path


ROOT = Path(__file__).resolve().parents[1]
GATE = ROOT / "docs/art/phase5-rules-decision-ui-design-gate-v1.json"
MACHINE = ROOT / "docs/evidence/phase5/rules-decision-ui-machine-v1.json"
DEFAULT_PLAN = ROOT / "docs/evidence/phase5/rules-decision-ui-gpu-plan-v1.tsv"
DEFAULT_BINDINGS = (
    ROOT / "docs/evidence/phase5/rules-decision-ui-source-bindings-v1.json")
DEFAULT_READY = (
    ROOT / "docs/evidence/phase5/rules-decision-ui-gpu-capture-ready-v1.json")
DEFAULT_AUTH = (
    ROOT / "docs/evidence/phase5/rules-decision-ui-gpu-authorization-template-v1.json")
DEFAULT_FONT = Path("/System/Library/Fonts/Hiragino Sans GB.ttc")
GATE_SHA = "26567d0547163561ed77e6e41b32753c4cc55efcd693c9ce22faf847d43f5a08"
HUD_SHA = "abf73aa283279c28677b5c97a9b02b3130b0d0125048c3124c85a9c0e72df860"
SHELL_SHA = "c144f1a25d3e7ced14e385bd6f76ca7e0f05b06597dcc8a4bacb4c570fc6bad3"

SOURCE_PATHS = (
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
    "scripts/run_phase5_rules_decision_ui_machine.py",
    "scripts/validate_phase5_rules_decision_ui_machine.py",
    "scripts/generate_phase5_rules_decision_ui_evidence_template.py",
    "scripts/prepare_phase5_rules_decision_ui_gpu_capture.py",
    "scripts/run_phase5_rules_decision_ui_gpu_capture.py",
)


def sha(path: Path) -> str:
    return hashlib.sha256(path.read_bytes()).hexdigest()


def write_json(path: Path, value: object) -> None:
    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_text(
        json.dumps(value, ensure_ascii=False, indent=2, sort_keys=True) + "\n",
        encoding="utf-8")


def relative_or_absolute(path: Path) -> str:
    try:
        return path.resolve().relative_to(ROOT).as_posix()
    except ValueError:
        return str(path.resolve())


def build_plan(gate: dict) -> list[str]:
    matrix = gate["evidence_gate"]
    states = matrix["minimum_machine_matrix"]["canonical_states"]
    profiles = matrix["minimum_actual_gpu_matrix"]["display_profiles"]
    if len(states) != 15 or len(profiles) != 4:
        raise RuntimeError("Phase5 design gate no longer declares 15 x 4 GPU cases")
    lines: list[str] = []
    for state_index, state in enumerate(states):
        for profile_index, profile in enumerate(profiles):
            logical_width, logical_height = map(
                int, profile["logical_viewport"].split("x"))
            physical_width, physical_height = map(
                int, profile["physical_viewport"].split("x"))
            locale = "en" if (state_index + profile_index) % 2 == 0 else "zh_cn"
            capture_id = f"{state}--{profile['profile_id']}--{locale}"
            fields = (
                capture_id, state, locale, profile["palette"],
                profile["profile_id"], logical_width, logical_height,
                physical_width, physical_height, profile["density"],
                profile["ui_scale"], 96.0 * profile["density"])
            lines.append("\t".join(str(item) for item in fields))
    if len(lines) != 60:
        raise RuntimeError("Phase5 GPU plan is not exactly 60 cases")
    return lines


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--billiards", type=Path,
        default=ROOT / "build-phase5-rules-ui/Billiards")
    parser.add_argument("--runner", type=Path, default=ROOT /
        "build-phase5-rules-ui/BilliardsPhase5RulesDecisionUiGpuCaptureRunner")
    parser.add_argument("--font", type=Path, default=DEFAULT_FONT)
    parser.add_argument("--plan", type=Path, default=DEFAULT_PLAN)
    parser.add_argument("--bindings", type=Path, default=DEFAULT_BINDINGS)
    parser.add_argument("--ready", type=Path, default=DEFAULT_READY)
    parser.add_argument("--authorization-template", type=Path,
        default=DEFAULT_AUTH)
    args = parser.parse_args()

    required = (GATE, MACHINE, args.billiards, args.runner, args.font)
    for path in required:
        if not path.is_file():
            raise FileNotFoundError(path)
    if sha(GATE) != GATE_SHA:
        raise RuntimeError("Phase5 design gate hash changed")
    machine = json.loads(MACHINE.read_text(encoding="utf-8"))
    if machine.get("totals", {}).get("machine_passed") != 540 or \
            machine.get("totals", {}).get("machine_executed") != 540:
        raise RuntimeError("Phase5 540/540 machine evidence is not current")

    gate = json.loads(GATE.read_text(encoding="utf-8"))
    lines = build_plan(gate)
    args.plan.parent.mkdir(parents=True, exist_ok=True)
    args.plan.write_text("\n".join(lines) + "\n", encoding="utf-8")

    source_files = []
    aggregate = hashlib.sha256()
    for relative in SOURCE_PATHS:
        path = ROOT / relative
        if not path.is_file():
            raise FileNotFoundError(path)
        digest = sha(path)
        source_files.append({"path": relative, "sha256": digest})
        aggregate.update(relative.encode("utf-8"))
        aggregate.update(b"\0")
        aggregate.update(digest.encode("ascii"))
        aggregate.update(b"\n")
    bindings = {
        "schema": "phase5_rules_decision_ui_source_bindings_v1",
        "design_gate_sha256": GATE_SHA,
        "aggregate_algorithm": "sha256(path_utf8 + NUL + sha256_ascii + LF)",
        "aggregate_sha256": aggregate.hexdigest(),
        "files": source_files,
    }
    write_json(args.bindings, bindings)

    frozen = {
        "design_gate_sha256": GATE_SHA,
        "hud_token_sha256": HUD_SHA,
        "shell_token_sha256": SHELL_SHA,
        "machine_evidence_sha256": sha(MACHINE),
        "plan_sha256": sha(args.plan),
        "source_binding_manifest_sha256": sha(args.bindings),
        "source_aggregate_sha256": bindings["aggregate_sha256"],
        "runner_source_sha256": sha(ROOT /
            "spikes/render_core_profile/phase5_rules_decision_ui_gpu_capture_runner.mm"),
        "runner_binary_sha256": sha(args.runner),
        "billiards_binary_sha256": sha(args.billiards),
        "font_sha256": sha(args.font),
    }
    ready = {
        "schema": "phase5_rules_decision_ui_gpu_capture_ready_v1",
        "status": "READY_REQUIRES_EXPLICIT_ONE_TIME_AUTH_NOT_RUN",
        "case_count": 60,
        "paths": {
            "plan": relative_or_absolute(args.plan),
            "source_bindings": relative_or_absolute(args.bindings),
            "runner_binary": relative_or_absolute(args.runner),
            "billiards_binary": relative_or_absolute(args.billiards),
            "font": str(args.font.resolve()),
        },
        "frozen_bindings": frozen,
        "claims": {
            "actual_gpu_capture_completed": 0,
            "manual_review_completed": 0,
            "beta_executed": False,
            "product_exposed": False,
            "profile_complete": False,
            "section_12_3_vs_17_e": "UNRESOLVED",
        },
        "execution_guard": {
            "authorization_schema":
                "phase5_rules_decision_ui_gpu_authorization_v1",
            "single_use": True,
            "consumed_marker_created_before_cocoa_or_opengl": True,
            "template_is_not_authorization": True,
        },
    }
    write_json(args.ready, ready)

    authorization = {
        "schema": "phase5_rules_decision_ui_gpu_authorization_v1",
        "authorized": False,
        "consumed": False,
        "authorization_id": "",
        "approved_by": "",
        "approved_at": "",
        "expires_unix_seconds": 0,
        "case_count": 60,
        **frozen,
        "instructions": (
            "This template cannot run capture. An authorized owner must copy it, "
            "set authorized=true, supply identity/timestamp/future expiry, and pass "
            "that explicit receipt with --execute-authorized-capture. The receipt "
            "is consumed atomically before Cocoa/OpenGL starts."),
    }
    write_json(args.authorization_template, authorization)
    print(f"Phase5 GPU capture package ready (not executed): {args.ready}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
