#!/usr/bin/env python3
"""Prepare Phase-5 rules UI v2 float-boundary readiness without GPU work."""

from __future__ import annotations

import argparse
import hashlib
import json
from pathlib import Path
import subprocess


ROOT = Path(__file__).resolve().parents[1]
DESIGN_GATE = ROOT / "docs/art/phase5-rules-decision-ui-design-gate-v1.json"
DESIGN_GATE_SHA = "26567d0547163561ed77e6e41b32753c4cc55efcd693c9ce22faf847d43f5a08"
CORRECTION_GATE = (ROOT /
    "docs/art/phase5-rules-decision-ui-v2-float-boundary-correction-design-gate-v1.json")
CORRECTION_GATE_SHA = "d325c99395178a19a9e3952e86f3d7bb2c6465cd052213261fc7c7a727f23a4c"
V1_FAILURE = ROOT / "docs/evidence/phase5/rules-decision-ui-gpu-failure-receipt-v1.json"
V1_FAILURE_SHA = "003bfe13e7c8a122ace4a0153d083f886bbd451c3015971a33f14ade3847351e"
V1_READY = ROOT / "docs/evidence/phase5/rules-decision-ui-gpu-capture-ready-v1.json"
V1_READY_SHA = "9321c0216224a9ce7c96cbfdb4ac85f491b70831017758e2e7f78a0afed89387"
V1_PLAN = ROOT / "docs/evidence/phase5/rules-decision-ui-gpu-plan-v1.tsv"
V1_PLAN_SHA = "1fd67271bfa8ea774f603ecc4fd575c643fc12303e62e9af1e1471cf7dbaf9d4"
V1_BINDINGS = ROOT / "docs/evidence/phase5/rules-decision-ui-source-bindings-v1.json"
V1_BINDINGS_SHA = "5f683887de0a7789dd54b230a013c9d74952b6b86388271c0b7dc648fe61b2e4"
V1_AUTH = ROOT / "docs/evidence/phase5/rules-decision-ui-gpu-authorization-root-v1.json"
V1_AUTH_SHA = "607da103e842ae51f317644ad82ad0d0215a7ae0b6c61a6e3dc474c504d6c663"
V1_RUNNER_SOURCE_SHA = "ed6b7ed793d2ea40c083305e0fe6155a452065d6ee965ca017347e5b92a93f8e"
HUD_SHA = "abf73aa283279c28677b5c97a9b02b3130b0d0125048c3124c85a9c0e72df860"
SHELL_SHA = "c144f1a25d3e7ced14e385bd6f76ca7e0f05b06597dcc8a4bacb4c570fc6bad3"
FONT = Path("/System/Library/Fonts/Hiragino Sans GB.ttc")

NEW_SOURCE_PATHS = (
    "spikes/render_core_profile/phase5_rules_decision_ui_gpu_capture_runner.mm",
    "tests/phase5_rules_decision_ui_v2_cpu_frame_preflight.cpp",
    "scripts/prepare_phase5_rules_decision_ui_v2_ready.py",
    "scripts/run_phase5_rules_decision_ui_machine.py",
    "scripts/validate_phase5_rules_decision_ui_machine.py",
    "CMakeLists.txt",
)


def sha(path: Path) -> str:
    return hashlib.sha256(path.read_bytes()).hexdigest()


def write_json(path: Path, value: object) -> None:
    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_text(
        json.dumps(value, ensure_ascii=False, indent=2, sort_keys=True) + "\n",
        encoding="utf-8")


def require(condition: bool, message: str) -> None:
    if not condition:
        raise RuntimeError(message)


def verify_v1_immutable() -> dict:
    expected = {
        DESIGN_GATE: DESIGN_GATE_SHA,
        CORRECTION_GATE: CORRECTION_GATE_SHA,
        V1_FAILURE: V1_FAILURE_SHA,
        V1_READY: V1_READY_SHA,
        V1_PLAN: V1_PLAN_SHA,
        V1_BINDINGS: V1_BINDINGS_SHA,
        V1_AUTH: V1_AUTH_SHA,
    }
    for path, digest in expected.items():
        require(path.is_file() and sha(path) == digest,
                f"immutable v1 binding drift: {path}")
    failure = json.loads(V1_FAILURE.read_text(encoding="utf-8"))
    require(failure.get("status") == "ACTUAL_GPU_FAIL_FROZEN_AT_ORDINAL_2" and
            failure["counts"] == {
                "actual_gpu_captures_completed": 1,
                "failure_triggered_cases": 1,
                "manual_reviews_completed": 0,
                "planned_cases": 60,
                "unrun_cases": 58,
            }, "v1 failure receipt boundary drift")
    return failure


def verify_exact_runner_delta(runner_source: Path) -> None:
    text = runner_source.read_text(encoding="utf-8")
    require(text.count("double uiScale = 1.0;") == 1 and
            text.count("std::stod(fields[10])") == 1,
            "v2 runner double/stod correction is not exact")
    reconstructed = text.replace("double uiScale = 1.0;",
        "float uiScale = 1.0f;").replace("std::stod(fields[10])",
        "std::stof(fields[10])")
    require(hashlib.sha256(reconstructed.encode("utf-8")).hexdigest() ==
            V1_RUNNER_SOURCE_SHA,
            "runner differs from v1 by more than the approved two replacements")


def verify_production_unchanged() -> list[dict]:
    v1 = json.loads(V1_BINDINGS.read_text(encoding="utf-8"))
    result = []
    for row in v1["files"]:
        relative = row["path"]
        if not relative.startswith("src/Billiards/"):
            continue
        path = ROOT / relative
        require(path.is_file() and sha(path) == row["sha256"],
                f"production source changed outside v2 gate: {relative}")
        result.append({"path": relative, "sha256": row["sha256"],
                       "scope": "v1_production_source_unchanged"})
    return result


def plan_lines() -> list[str]:
    gate = json.loads(DESIGN_GATE.read_text(encoding="utf-8"))
    matrix = gate["evidence_gate"]
    states = matrix["minimum_machine_matrix"]["canonical_states"]
    profiles = matrix["minimum_actual_gpu_matrix"]["display_profiles"]
    require(len(states) == 15 and len(profiles) == 4,
            "design gate no longer declares 15 x 4")
    result = []
    for state_index, state in enumerate(states):
        for profile_index, profile in enumerate(profiles):
            logical = profile["logical_viewport"].split("x")
            physical = profile["physical_viewport"].split("x")
            locale = "en" if (state_index + profile_index) % 2 == 0 else "zh_cn"
            capture_id = f"{state}--{profile['profile_id']}--{locale}"
            fields = (capture_id, state, locale, profile["palette"],
                profile["profile_id"], int(logical[0]), int(logical[1]),
                int(physical[0]), int(physical[1]), profile["density"],
                profile["ui_scale"], 96.0 * profile["density"])
            result.append("\t".join(str(value) for value in fields))
    require(len(result) == 60 and len(set(result)) == 60,
            "v2 plan is not exactly 60 unique rows")
    return result


def source_bindings(production: list[dict]) -> dict:
    files = list(production)
    for relative in NEW_SOURCE_PATHS:
        path = ROOT / relative
        require(path.is_file(), f"v2 source input missing: {relative}")
        files.append({"path": relative, "sha256": sha(path),
                      "scope": "v2_runner_test_or_readiness"})
    files.sort(key=lambda row: row["path"])
    aggregate = hashlib.sha256()
    for row in files:
        aggregate.update(row["path"].encode("utf-8"))
        aggregate.update(b"\0")
        aggregate.update(row["sha256"].encode("ascii"))
        aggregate.update(b"\n")
    return {
        "schema": "phase5_rules_decision_ui_source_bindings_v2",
        "status": "RUNNER_ONLY_CORRECTION_PRODUCTION_SOURCE_UNCHANGED",
        "design_gate_sha256": DESIGN_GATE_SHA,
        "v2_correction_gate_sha256": CORRECTION_GATE_SHA,
        "v1_failure_receipt_sha256": V1_FAILURE_SHA,
        "aggregate_algorithm":
            "sha256(path_utf8 + NUL + file_sha256_ascii + LF), sorted by path",
        "aggregate_sha256": aggregate.hexdigest(),
        "file_count": len(files),
        "files": files,
    }


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--runner", type=Path, required=True)
    parser.add_argument("--billiards", type=Path, required=True)
    parser.add_argument("--cpu-preflight", type=Path, required=True)
    parser.add_argument("--machine-a", type=Path, required=True)
    parser.add_argument("--machine-b", type=Path, required=True)
    parser.add_argument("--output-root", type=Path, required=True)
    args = parser.parse_args()
    for path in (args.runner, args.billiards, args.cpu_preflight,
                 args.machine_a, args.machine_b, FONT):
        require(path.is_file(), f"v2 required input missing: {path}")
    require(not args.output_root.exists(),
            "v2 readiness root must be new and immutable")
    verify_v1_immutable()
    runner_source = (ROOT /
        "spikes/render_core_profile/phase5_rules_decision_ui_gpu_capture_runner.mm")
    verify_exact_runner_delta(runner_source)
    production = verify_production_unchanged()
    require(args.machine_a.read_bytes() == args.machine_b.read_bytes(),
            "540-case machine reports are not byte reproducible")
    machine = json.loads(args.machine_a.read_text(encoding="utf-8"))
    require(machine["totals"]["machine_executed"] == 540 and
            machine["totals"]["machine_passed"] == 540 and
            len(machine["cases"]) == 540,
            "machine report is not 540/540")

    output = args.output_root.resolve()
    output.mkdir(parents=True)
    plan_path = output / "rules-decision-ui-gpu-plan-v2.tsv"
    plan_path.write_text("\n".join(plan_lines()) + "\n", encoding="utf-8")
    require(sha(plan_path) == V1_PLAN_SHA,
            "v2 selector plan content drifted from the frozen 60-row selector")
    machine_path = output / "rules-decision-ui-machine-v2.json"
    machine_path.write_bytes(args.machine_a.read_bytes())
    cpu_path = output / "rules-decision-ui-v2-cpu-frame-preflight.json"
    subprocess.run([str(args.cpu_preflight.resolve()), str(plan_path),
                    str(cpu_path)], cwd=ROOT, check=True)
    cpu = json.loads(cpu_path.read_text(encoding="utf-8"))
    require(cpu.get("status") == "CPU_FRAME_PREFLIGHT_PASS_60_OF_60_NO_GPU" and
            cpu.get("passed_count") == 60 and
            cpu.get("bad_input_passed_count") == 5 and
            cpu.get("actual_gpu_invoked") is False and
            cpu.get("opengl_invoked") is False and
            cpu.get("ns_application_invoked") is False and
            cpu.get("gpu_output_created") is False,
            "v2 CPU-frame preflight did not pass its no-GPU boundary")

    binding_value = source_bindings(production)
    binding_path = output / "rules-decision-ui-source-bindings-v2.json"
    write_json(binding_path, binding_value)
    frozen = {
        "design_gate_sha256": DESIGN_GATE_SHA,
        "v2_correction_gate_sha256": CORRECTION_GATE_SHA,
        "v1_failure_receipt_sha256": V1_FAILURE_SHA,
        "v1_failure_status": "ACTUAL_GPU_FAIL_FROZEN_AT_ORDINAL_2",
        "v1_ready_manifest_sha256": V1_READY_SHA,
        "v1_plan_sha256": V1_PLAN_SHA,
        "v1_source_bindings_sha256": V1_BINDINGS_SHA,
        "v1_consumed_authorization_sha256": V1_AUTH_SHA,
        "plan_sha256": sha(plan_path),
        "source_binding_manifest_sha256": sha(binding_path),
        "source_aggregate_sha256": binding_value["aggregate_sha256"],
        "runner_source_sha256": sha(runner_source),
        "runner_binary_sha256": sha(args.runner),
        "billiards_binary_sha256": sha(args.billiards),
        "cpu_preflight_source_sha256": sha(ROOT /
            "tests/phase5_rules_decision_ui_v2_cpu_frame_preflight.cpp"),
        "cpu_preflight_binary_sha256": sha(args.cpu_preflight),
        "cpu_preflight_report_sha256": sha(cpu_path),
        "machine_report_sha256": sha(machine_path),
        "font_sha256": sha(FONT),
        "hud_token_sha256": HUD_SHA,
        "shell_token_sha256": SHELL_SHA,
    }
    claims = {
        "v2_actual_gpu_capture": False,
        "v2_manual_review": False,
        "beta": False,
        "product_exposed": False,
        "profile_complete": False,
        "v2_rerun_authorized": False,
    }
    ready = {
        "schema": "phase5_rules_decision_ui_gpu_capture_ready_v2",
        "status": "V2_READY_NEW_AUTHORIZATION_REQUIRED_RERUN_NOT_AUTHORIZED",
        "case_count": 60,
        "cpu_frame_preflight_passed": 60,
        "machine_matrix_passed": 540,
        "machine_matrix_byte_reproducible": True,
        "paths": {
            "plan": str(plan_path),
            "source_bindings": str(binding_path),
            "runner_binary": str(args.runner.resolve()),
            "billiards_binary": str(args.billiards.resolve()),
            "cpu_preflight_binary": str(args.cpu_preflight.resolve()),
            "cpu_preflight_report": str(cpu_path),
            "machine_report": str(machine_path),
            "font": str(FONT),
        },
        "frozen_bindings": frozen,
        "claims": claims,
        "v1_frozen_outcome": {
            "actual_gpu_completed": 1,
            "failed": 1,
            "unrun": 58,
            "resume_or_overwrite_allowed": False,
        },
    }
    ready_path = output / "rules-decision-ui-gpu-capture-ready-v2.json"
    write_json(ready_path, ready)
    authorization = {
        "schema": "phase5_rules_decision_ui_gpu_authorization_v1",
        "template_version": 2,
        "authorized": False,
        "consumed": False,
        "authorization_id": "",
        "approved_by": "",
        "approved_at": "",
        "expires_unix_seconds": 0,
        "case_count": 60,
        **frozen,
        "ready_manifest_sha256": sha(ready_path),
        "instructions": (
            "NOT AN AUTHORIZATION. The correction gate does not authorize a v2 "
            "GPU rerun. A future explicit root decision must create a separate "
            "single-use receipt preserving all bindings; never edit this template."),
    }
    auth_path = output / "rules-decision-ui-gpu-authorization-template-v2.json"
    write_json(auth_path, authorization)
    receipt = {
        "schema": "phase5_rules_decision_ui_v2_offline_readiness_receipt_v1",
        "status": "V2_CPU_AND_MACHINE_READY_GPU_NOT_AUTHORIZED_NOT_RUN",
        "ready_manifest_sha256": sha(ready_path),
        "authorization_template_sha256": sha(auth_path),
        "plan_sha256": sha(plan_path),
        "source_bindings_sha256": sha(binding_path),
        "cpu_preflight_report_sha256": sha(cpu_path),
        "machine_report_sha256": sha(machine_path),
        "claims": claims,
        "v1_failure_receipt_sha256": V1_FAILURE_SHA,
        "v1_failure_immutable": True,
    }
    receipt_path = output / "offline-readiness-receipt-v2.json"
    write_json(receipt_path, receipt)
    print(json.dumps(receipt, sort_keys=True))
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
