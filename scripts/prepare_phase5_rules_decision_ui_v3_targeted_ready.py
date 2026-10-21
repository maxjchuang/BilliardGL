#!/usr/bin/env python3
"""Prepare exact 28-case Phase5 Rules UI v3 readiness without GPU."""

from __future__ import annotations

import argparse
import hashlib
import json
from pathlib import Path
import subprocess
import sys


ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT))

from scripts import run_phase5_rules_decision_ui_v3_visual_repair_machine as machine
from scripts import validate_phase5_rules_decision_ui_v3_visual_repair_machine as machine_validator


FONT = Path("/System/Library/Fonts/Hiragino Sans GB.ttc")
ORIGINAL_GATE = ROOT / "docs/art/phase5-rules-decision-ui-design-gate-v1.json"
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
    "scripts/run_phase5_rules_decision_ui_v3_targeted_gpu_capture.py",
    "scripts/prepare_phase5_rules_decision_ui_v3_targeted_ready.py",
    "scripts/validate_phase5_rules_decision_ui_v3_targeted_ready.py",
    "tests/visual/test_phase5_rules_decision_ui_v3_visual_repair_machine.py",
    "tests/visual/test_phase5_rules_decision_ui_v3_targeted_ready.py",
)


def sha(path: Path) -> str:
    return hashlib.sha256(path.read_bytes()).hexdigest()


def require(value: bool, message: str) -> None:
    if not value:
        raise RuntimeError(message)


def write_json(path: Path, value: object) -> None:
    path.write_text(json.dumps(value, ensure_ascii=False, indent=2,
                               sort_keys=True) + "\n", encoding="utf-8")


def plan_rows() -> list[str]:
    visual = json.loads(machine.GATE.read_text(encoding="utf-8"))[
        "targeted_actual_gpu_gate"]
    ids = visual["repair_case_ids"] + visual["regression_control_case_ids"]
    profiles = {row["profile_id"]: row for row in
        json.loads(ORIGINAL_GATE.read_text(encoding="utf-8"))[
            "evidence_gate"]["minimum_actual_gpu_matrix"]["display_profiles"]}
    rows = []
    for case_id in ids:
        state, profile_id, locale = case_id.split("--")
        profile = profiles[profile_id]
        logical = profile["logical_viewport"].split("x")
        physical = profile["physical_viewport"].split("x")
        fields = (case_id, state, locale, profile["palette"], profile_id,
                  int(logical[0]), int(logical[1]), int(physical[0]),
                  int(physical[1]), profile["density"], profile["ui_scale"],
                  96.0 * profile["density"])
        rows.append("\t".join(str(value) for value in fields))
    require(len(rows) == 28 and len(set(rows)) == 28,
            "v3 targeted plan must be exact 20 repair + 8 control")
    return rows


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--runner", type=Path, required=True)
    parser.add_argument("--billiards", type=Path, required=True)
    parser.add_argument("--cpu-preflight", type=Path, required=True)
    parser.add_argument("--machine-a", type=Path, required=True)
    parser.add_argument("--machine-b", type=Path, required=True)
    parser.add_argument("--output-root", type=Path, required=True)
    args = parser.parse_args()
    required = (args.runner, args.billiards, args.cpu_preflight,
                args.machine_a, args.machine_b, FONT)
    require(all(path.is_file() for path in required), "v3 readiness input missing")
    require(not args.output_root.exists(), "v3 readiness root must be fresh")
    require(sha(machine.GATE) == machine.GATE_SHA, "v3 gate SHA drift")
    require(args.machine_a.read_bytes() == args.machine_b.read_bytes(),
            "v3 540-case reports are not byte reproducible")
    machine_value = json.loads(args.machine_a.read_text(encoding="utf-8"))
    machine_validator.validate(machine_value)

    output = args.output_root.resolve()
    output.mkdir(parents=True)
    plan = output / "rules-decision-ui-v3-targeted-plan.tsv"
    plan.write_text("\n".join(plan_rows()) + "\n", encoding="utf-8")
    machine_path = output / "rules-decision-ui-v3-machine.json"
    machine_path.write_bytes(args.machine_a.read_bytes())
    cpu_report = output / "rules-decision-ui-v3-targeted-cpu-preflight.json"
    subprocess.run([str(args.cpu_preflight.resolve()), str(plan),
                    str(cpu_report)], cwd=ROOT, check=True)
    cpu = json.loads(cpu_report.read_text(encoding="utf-8"))
    require(cpu.get("status") == "CPU_FRAME_PREFLIGHT_PASS_28_OF_28_NO_GPU" and
            cpu.get("passed_count") == 28 and
            cpu.get("bad_input_passed_count") == 5 and
            all(cpu.get(field) is False for field in
                ("actual_gpu_invoked", "ns_application_invoked",
                 "opengl_invoked", "gpu_output_created")),
            "v3 28-case CPU preflight failed")

    files = [{"path": relative, "sha256": sha(ROOT / relative),
              "scope": "allowed_presentation_or_offline_scaffolding"}
             for relative in SOURCE_FILES]
    files.extend({"path": relative, "sha256": digest,
                  "scope": "forbidden_authority_read_only"}
                 for relative, digest in machine.FORBIDDEN_BINDINGS.items())
    files.sort(key=lambda row: row["path"])
    aggregate = hashlib.sha256()
    for row in files:
        aggregate.update(row["path"].encode("utf-8")); aggregate.update(b"\0")
        aggregate.update(row["sha256"].encode("ascii")); aggregate.update(b"\n")
    bindings = {
        "schema": "phase5_rules_decision_ui_v3_source_bindings_v1",
        "visual_repair_design_gate_sha256": machine.GATE_SHA,
        "aggregate_sha256": aggregate.hexdigest(),
        "file_count": len(files), "files": files,
    }
    bindings_path = output / "rules-decision-ui-v3-source-bindings.json"
    write_json(bindings_path, bindings)
    case_ids = [line.split("\t", 1)[0] for line in plan_rows()]
    frozen = {
        "visual_repair_design_gate_sha256": machine.GATE_SHA,
        "plan_sha256": sha(plan),
        "source_bindings_sha256": sha(bindings_path),
        "source_aggregate_sha256": bindings["aggregate_sha256"],
        "runner_source_sha256": sha(ROOT /
            "spikes/render_core_profile/phase5_rules_decision_ui_gpu_capture_runner.mm"),
        "runner_binary_sha256": sha(args.runner),
        "billiards_binary_sha256": sha(args.billiards),
        "cpu_preflight_source_sha256": sha(ROOT /
            "tests/phase5_rules_decision_ui_v2_cpu_frame_preflight.cpp"),
        "cpu_preflight_binary_sha256": sha(args.cpu_preflight),
        "cpu_preflight_report_sha256": sha(cpu_report),
        "machine_report_sha256": sha(machine_path),
        "font_sha256": sha(FONT),
    }
    claims = {
        "actual_gpu_authorized": False, "actual_gpu_completed": 0,
        "golden_promotion": False, "m2_promotion": False,
        "product_exposed": False, "technical_art_review_completed": 0,
    }
    ready = {
        "schema": "phase5_rules_decision_ui_v3_targeted_gpu_ready_v1",
        "status": "V3_TARGETED_28_READY_EXPLICIT_AUTHORIZATION_REQUIRED_NOT_RUN",
        "case_count": 28, "repair_case_count": 20,
        "regression_control_count": 8, "case_ids_in_order": case_ids,
        "machine_matrix_passed": 540,
        "machine_matrix_byte_reproducible": True,
        "cpu_preflight_passed": 28,
        "case_results": [{"capture_id": case_id, "status": "NOT_RUN"}
                         for case_id in case_ids],
        "paths": {
            "plan": str(plan), "source_bindings": str(bindings_path),
            "runner_binary": str(args.runner.resolve()),
            "billiards_binary": str(args.billiards.resolve()),
            "cpu_preflight_binary": str(args.cpu_preflight.resolve()),
            "cpu_preflight_report": str(cpu_report),
            "machine_report": str(machine_path), "font": str(FONT),
        },
        "frozen_bindings": frozen, "claims": claims,
    }
    ready_path = output / "rules-decision-ui-v3-targeted-ready.json"
    write_json(ready_path, ready)
    auth = {
        "schema": "phase5_rules_decision_ui_v3_targeted_gpu_authorization_v1",
        "authorized": False, "consumed": False, "authorization_id": "",
        "approved_by": "", "approved_at": "", "expires_unix_seconds": 0,
        "authorized_output_canonical_path": "", "case_count": 28,
        "scene_ids_in_order": case_ids,
        "targeted_visual_repair_gpu_authorized": False,
        "product_exposed": False, "golden_promotion": False,
        "m2_promotion": False, **frozen,
        "ready_manifest_sha256": sha(ready_path),
        "instructions": "NOT AN AUTHORIZATION. Root must issue a separate exact single-use receipt.",
    }
    auth_path = output / "rules-decision-ui-v3-targeted-authorization-template.json"
    write_json(auth_path, auth)
    receipt = {
        "schema": "phase5_rules_decision_ui_v3_targeted_offline_readiness_v1",
        "status": "CPU_28_AND_MACHINE_540_READY_GPU_NOT_AUTHORIZED_NOT_RUN",
        "ready_manifest_sha256": sha(ready_path),
        "authorization_template_sha256": sha(auth_path),
        "claims": claims,
    }
    write_json(output / "offline-readiness-receipt.json", receipt)
    print(json.dumps(receipt, sort_keys=True))
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
