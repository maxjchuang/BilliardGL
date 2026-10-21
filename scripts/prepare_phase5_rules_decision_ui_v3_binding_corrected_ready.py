#!/usr/bin/env python3
"""Prepare corrected Phase5 Rules UI v3 exact-28 readiness without GPU."""

from __future__ import annotations

import argparse
import hashlib
import json
from pathlib import Path
import subprocess
import sys


ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT))

from scripts import prepare_phase5_rules_decision_ui_v3_targeted_ready as prior
from scripts import run_phase5_rules_decision_ui_v3_visual_repair_machine as machine
from scripts import (
    run_phase5_rules_decision_ui_v3_binding_corrected_targeted_gpu_capture
    as capture,
)
from scripts import (
    validate_phase5_rules_decision_ui_v3_visual_repair_machine
    as machine_validator,
)


FONT = Path("/System/Library/Fonts/Hiragino Sans GB.ttc")
HUD_TOKEN = (
    ROOT / "assets/modern/hud/"
    "focused_practice_hud_shipping_tokens_v1.json")
SHELL_TOKEN = (
    ROOT / "assets/modern/hud/"
    "focused_practice_shell_shipping_tokens_v1.json")
CORRECTION_GATE = (
    ROOT / "docs/art/phase5-rules-decision-ui-v3-targeted-"
    "preordinal-binding-correction-gate-v1.json")
CORRECTION_GATE_SHA = (
    "2155c60375062c7992d1280a3dfc96929840627d3194a4401a0501eabfad77a3")
SOURCE_FILES = (
    "CMakeLists.txt",
    "src/Billiards/render/ui/rules_decision_ui_draw_list.h",
    "src/Billiards/render/ui/rules_decision_ui_draw_list.cpp",
    "src/Billiards/render/ui/rules_decision_product_draw.cpp",
    "tests/rules_decision_ui_tests.cpp",
    "tests/rules_decision_product_runtime_tests.cpp",
    "tests/phase5_rules_decision_ui_v2_cpu_frame_preflight.cpp",
    "spikes/render_core_profile/phase5_rules_decision_ui_gpu_capture_runner.mm",
    "scripts/run_phase5_rules_decision_ui_v3_visual_repair_machine.py",
    "scripts/validate_phase5_rules_decision_ui_v3_visual_repair_machine.py",
    "scripts/run_phase5_rules_decision_ui_v3_binding_corrected_targeted_gpu_capture.py",
    "scripts/prepare_phase5_rules_decision_ui_v3_binding_corrected_ready.py",
    "scripts/validate_phase5_rules_decision_ui_v3_binding_corrected_ready.py",
    "tests/visual/test_phase5_rules_decision_ui_v3_visual_repair_machine.py",
    "tests/visual/test_phase5_rules_decision_ui_v3_binding_corrected_ready.py",
)


def sha(path: Path) -> str:
    return hashlib.sha256(path.read_bytes()).hexdigest()


def require(value: bool, message: str) -> None:
    if not value:
        raise RuntimeError(message)


def write_json(path: Path, value: object) -> None:
    path.write_text(
        json.dumps(value, ensure_ascii=False, indent=2, sort_keys=True) + "\n",
        encoding="utf-8")


def plan_rows() -> list[str]:
    return prior.plan_rows()


def source_rows() -> list[dict[str, str]]:
    rows = [{
        "path": relative,
        "sha256": sha(ROOT / relative),
        "scope": "allowed_presentation_or_offline_scaffolding",
    } for relative in SOURCE_FILES]
    rows.extend({
        "path": relative,
        "sha256": digest,
        "scope": "forbidden_authority_read_only",
    } for relative, digest in machine.FORBIDDEN_BINDINGS.items())
    rows.extend((
        {
            "path":
                "assets/modern/hud/focused_practice_hud_shipping_tokens_v1.json",
            "sha256": sha(HUD_TOKEN),
            "scope": "read_only_runtime_asset",
        },
        {
            "path":
                "assets/modern/hud/focused_practice_shell_shipping_tokens_v1.json",
            "sha256": sha(SHELL_TOKEN),
            "scope": "read_only_runtime_asset",
        },
        {
            "path": str(CORRECTION_GATE.relative_to(ROOT)),
            "sha256": sha(CORRECTION_GATE),
            "scope": "read_only_design_gate",
        },
    ))
    rows.sort(key=lambda row: row["path"])
    return rows


def aggregate_sha(rows: list[dict[str, str]]) -> str:
    digest = hashlib.sha256()
    for row in rows:
        digest.update(row["path"].encode("utf-8"))
        digest.update(b"\0")
        digest.update(row["sha256"].encode("ascii"))
        digest.update(b"\n")
    return digest.hexdigest()


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--runner", type=Path, required=True)
    parser.add_argument("--billiards", type=Path, required=True)
    parser.add_argument("--cpu-preflight", type=Path, required=True)
    parser.add_argument("--machine-a", type=Path, required=True)
    parser.add_argument("--machine-b", type=Path, required=True)
    parser.add_argument("--output-root", type=Path, required=True)
    args = parser.parse_args()
    required = (
        args.runner, args.billiards, args.cpu_preflight,
        args.machine_a, args.machine_b, FONT, HUD_TOKEN, SHELL_TOKEN,
        CORRECTION_GATE,
    )
    require(all(path.is_file() for path in required),
            "corrected v3 readiness input missing")
    require(not args.output_root.exists(),
            "corrected v3 readiness root must be fresh")
    require(sha(machine.GATE) == machine.GATE_SHA, "v3 gate SHA drift")
    require(sha(CORRECTION_GATE) == CORRECTION_GATE_SHA,
            "correction gate SHA drift")
    require(args.machine_a.read_bytes() == args.machine_b.read_bytes(),
            "corrected v3 540-case reports are not byte reproducible")
    machine_value = json.loads(
        args.machine_a.read_text(encoding="utf-8"))
    machine_validator.validate(machine_value)

    output = args.output_root.resolve()
    output.mkdir(parents=True)
    plan = output / "rules-decision-ui-v3-targeted-plan.tsv"
    plan.write_text("\n".join(plan_rows()) + "\n", encoding="utf-8")
    machine_path = output / "rules-decision-ui-v3-machine.json"
    machine_path.write_bytes(args.machine_a.read_bytes())
    cpu_report = (
        output / "rules-decision-ui-v3-targeted-cpu-preflight.json")
    subprocess.run([
        str(args.cpu_preflight.resolve()), str(plan), str(cpu_report),
    ], cwd=ROOT, check=True)
    cpu = json.loads(cpu_report.read_text(encoding="utf-8"))
    require(
        cpu.get("status") == "CPU_FRAME_PREFLIGHT_PASS_28_OF_28_NO_GPU" and
        cpu.get("passed_count") == 28 and
        cpu.get("bad_input_passed_count") == 5 and
        all(cpu.get(field) is False for field in (
            "actual_gpu_invoked", "ns_application_invoked",
            "opengl_invoked", "gpu_output_created")),
        "corrected v3 28-case CPU preflight failed")

    rows = source_rows()
    bindings = {
        "schema":
            "phase5_rules_decision_ui_v3_binding_corrected_source_bindings_v1",
        "visual_repair_design_gate_sha256": machine.GATE_SHA,
        "correction_gate_sha256": CORRECTION_GATE_SHA,
        "aggregate_sha256": aggregate_sha(rows),
        "file_count": len(rows),
        "files": rows,
    }
    bindings_path = (
        output / "rules-decision-ui-v3-source-binding-manifest.json")
    write_json(bindings_path, bindings)
    case_ids = [line.split("\t", 1)[0] for line in plan_rows()]
    frozen = {
        "visual_repair_design_gate_sha256": machine.GATE_SHA,
        "correction_gate_sha256": CORRECTION_GATE_SHA,
        "plan_sha256": sha(plan),
        "source_binding_manifest_sha256": sha(bindings_path),
        "source_aggregate_sha256": bindings["aggregate_sha256"],
        "hud_token_sha256": sha(HUD_TOKEN),
        "shell_token_sha256": sha(SHELL_TOKEN),
        "runner_source_sha256": sha(
            ROOT / "spikes/render_core_profile/"
            "phase5_rules_decision_ui_gpu_capture_runner.mm"),
        "runner_binary_sha256": sha(args.runner),
        "billiards_binary_sha256": sha(args.billiards),
        "cpu_preflight_source_sha256": sha(
            ROOT / "tests/phase5_rules_decision_ui_v2_cpu_frame_preflight.cpp"),
        "cpu_preflight_binary_sha256": sha(args.cpu_preflight),
        "cpu_preflight_report_sha256": sha(cpu_report),
        "machine_report_sha256": sha(machine_path),
        "font_sha256": sha(FONT),
    }
    claims = {
        "actual_gpu_authorized": False,
        "actual_gpu_completed": 0,
        "golden_promotion": False,
        "m2_promotion": False,
        "product_exposed": False,
        "technical_art_review_completed": 0,
    }
    ready = {
        "schema":
            "phase5_rules_decision_ui_v3_binding_corrected_targeted_gpu_ready_v1",
        "status": capture.READY_STATUS,
        "case_count": 28,
        "repair_case_count": 20,
        "regression_control_count": 8,
        "case_ids_in_order": case_ids,
        "machine_matrix_passed": 540,
        "machine_matrix_byte_reproducible": True,
        "cpu_preflight_passed": 28,
        "case_results": [
            {"capture_id": case_id, "status": "NOT_RUN"}
            for case_id in case_ids
        ],
        "paths": {
            "plan": str(plan),
            "source_binding_manifest": str(bindings_path),
            "runner_binary": str(args.runner.resolve()),
            "billiards_binary": str(args.billiards.resolve()),
            "cpu_preflight_binary": str(args.cpu_preflight.resolve()),
            "cpu_preflight_report": str(cpu_report),
            "machine_report": str(machine_path),
            "font": str(FONT),
            "hud_token": str(HUD_TOKEN),
            "shell_token": str(SHELL_TOKEN),
        },
        "frozen_bindings": frozen,
        "claims": claims,
    }
    ready_path = (
        output / "rules-decision-ui-v3-binding-corrected-targeted-ready.json")
    write_json(ready_path, ready)
    auth = {
        "schema": capture.AUTH_SCHEMA,
        "authorized": False,
        "consumed": False,
        "authorization_id": "",
        "approved_by": "",
        "approved_at": "",
        "expires_unix_seconds": 0,
        "authorized_output_canonical_path": "",
        "case_count": 28,
        "scene_ids_in_order": case_ids,
        "targeted_visual_repair_gpu_authorized": False,
        "product_exposed": False,
        "golden_promotion": False,
        "m2_promotion": False,
        **frozen,
        "ready_manifest_sha256": sha(ready_path),
        "instructions":
            "NOT AN AUTHORIZATION. Root must issue a separate exact single-use receipt.",
    }
    require(capture.RUNNER_REQUIRED_AUTH_FIELDS.issubset(auth),
            "corrected auth template omits runner-required fields")
    auth_path = (
        output /
        "rules-decision-ui-v3-binding-corrected-authorization-template.json")
    write_json(auth_path, auth)
    receipt = {
        "schema":
            "phase5_rules_decision_ui_v3_binding_corrected_offline_readiness_v1",
        "status":
            "CPU_28_AND_MACHINE_540_BINDING_CORRECTED_READY_GPU_NOT_AUTHORIZED_NOT_RUN",
        "ready_manifest_sha256": sha(ready_path),
        "authorization_template_sha256": sha(auth_path),
        "source_binding_manifest_sha256": sha(bindings_path),
        "claims": claims,
    }
    write_json(output / "offline-readiness-receipt.json", receipt)
    print(json.dumps(receipt, sort_keys=True))
    return 0


if __name__ == "__main__":
    raise SystemExit(main())

