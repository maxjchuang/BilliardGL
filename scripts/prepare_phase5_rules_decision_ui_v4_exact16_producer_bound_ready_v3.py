#!/usr/bin/env python3
"""Prepare fresh producer-bound Phase5 v4 exact16 NOT_RUN readiness v3."""

from __future__ import annotations

import argparse
import json
from pathlib import Path
import shutil
import subprocess
import sys

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT))
from scripts import prepare_phase5_rules_decision_ui_v4_exact16_ready as v1

OUTPUT = ROOT / "docs/evidence/phase5/rules-decision-ui-v4-exact16-producer-bound-ready-v3"
V2 = ROOT / "docs/evidence/phase5/rules-decision-ui-v4-exact16-ready-v2"
BUILD = ROOT / "build-integrated-final-shared-phase2-v9-phase4-v9-phase5-v3"
RUNNER = BUILD / "BilliardsPhase5RulesDecisionUiV4Exact16GpuCaptureRunner"
CPU = BUILD / "BilliardsPhase5RulesDecisionUiV4Exact16CpuFramePreflight"
PRODUCT = BUILD / "BilliardsRulesDecisionProductRuntimeTests"
BILLIARDS = BUILD / "Billiards"
GATE = ROOT / "docs/art/phase5-rules-decision-ui-v4-exact16-capture-producer-contract-correction-gate-v1.json"
GATE_SHA = "dbab785638be91550e25380141a62819c28ea72da9082eb23c02c5e4869866ad"
RUNNER_SOURCE = ROOT / "spikes/render_core_profile/phase5_rules_decision_ui_gpu_capture_runner.mm"
DISPATCHER = ROOT / "scripts/run_phase5_rules_decision_ui_v4_exact16_capture_dispatch.py"
DISPATCH_VALIDATOR = ROOT / "scripts/validate_phase5_rules_decision_ui_v4_exact16_capture_dispatch.py"
ADDITIONAL_SOURCES = (
    "spikes/render_core_profile/phase5_rules_decision_ui_gpu_capture_runner.mm",
    "docs/art/phase5-rules-decision-ui-v4-exact16-capture-producer-contract-correction-gate-v1.json",
    "scripts/validate_phase5_rules_decision_ui_v4_exact16_capture_producer_contract_correction_gate.py",
    "scripts/validate_phase5_rules_decision_ui_v4_exact16_capture_producer_implementation.py",
    "scripts/prepare_phase5_rules_decision_ui_v4_exact16_producer_bound_ready_v3.py",
    "scripts/validate_phase5_rules_decision_ui_v4_exact16_producer_bound_ready_v3.py",
    "tests/visual/test_phase5_rules_decision_ui_v4_exact16_capture_producer_contract_correction_gate.py",
    "tests/visual/test_phase5_rules_decision_ui_v4_exact16_capture_producer_implementation.py",
    "tests/visual/test_phase5_rules_decision_ui_v4_exact16_producer_bound_ready_v3.py",
)


def require(value: bool, message: str) -> None:
    if not value:
        raise RuntimeError(message)


def write(path: Path, value: object) -> None:
    path.write_text(json.dumps(value, indent=2, sort_keys=True) + "\n")


def prepare(output: Path = OUTPUT) -> dict[str, Path]:
    require(not output.exists(), "fresh producer-bound readiness root required")
    required = (RUNNER, CPU, PRODUCT, BILLIARDS, GATE, RUNNER_SOURCE,
                DISPATCHER, DISPATCH_VALIDATOR)
    require(all(path.is_file() for path in required) and
            v1.sha(GATE) == GATE_SHA, "producer-bound input missing/drifted")
    require(all((ROOT / path).is_file() for path in ADDITIONAL_SOURCES),
            "producer-bound source inventory incomplete")
    output.mkdir(parents=True)
    plan = output / "rules-decision-ui-v4-exact16-plan.tsv"
    shutil.copyfile(V2 / plan.name, plan)
    require(v1.sha(plan) ==
            "426de40ba87b8cfdb5eefc88464720507e53ced2190ffb61564e7cd848eb593f",
            "exact16 frozen plan drift")
    machine = output / "rules-decision-ui-v4-machine.json"
    shutil.copyfile(V2 / machine.name, machine)
    cpu_report = output / "rules-decision-ui-v4-exact16-cpu-preflight.json"
    subprocess.run([str(CPU), str(plan), str(cpu_report)], cwd=ROOT, check=True)
    cpu = json.loads(cpu_report.read_text())
    require(cpu.get("case_count") == cpu.get("passed_count") == 16 and
            cpu.get("bad_input_count") == cpu.get("bad_input_passed_count") == 5 and
            cpu.get("actual_gpu_invoked") is False,
            "exact16 CPU preflight failed")
    old = json.loads((V2 / "rules-decision-ui-v4-source-bindings.json").read_text())
    rows = {item["path"]: item for item in old["files"]}
    for relative in list(rows):
        path = ROOT / relative
        require(path.is_file(), "source missing: " + relative)
        rows[relative] = {**rows[relative], "sha256": v1.sha(path)}
    for relative in ADDITIONAL_SOURCES:
        rows[relative] = {"path": relative, "sha256": v1.sha(ROOT / relative),
                          "scope": "v4_capture_only_producer_or_offline_validation"}
    ordered = [rows[key] for key in sorted(rows)]
    manifest = {
        "schema": "phase5_rules_decision_ui_v4_exact16_source_bindings_v3",
        "gate_sha256": v1.machine.GATE_SHA,
        "producer_correction_gate_sha256": GATE_SHA,
        "capture_target": "BilliardsPhase5RulesDecisionUiV4Exact16GpuCaptureRunner",
        "compile_definition": "BILLIARDGL_PHASE5_RULES_UI_V4_EXACT16=1",
        "runner_source_sha256": v1.sha(RUNNER_SOURCE),
        "runner_binary_sha256": v1.sha(RUNNER),
        "file_count": len(ordered), "files": ordered,
        "aggregate_sha256": v1.aggregate(ordered),
    }
    source_path = output / "rules-decision-ui-v4-source-bindings.json"
    write(source_path, manifest)
    ids = v1.case_ids()
    claims = {"gpu_authorized": False, "gpu_executed": False,
              "technical_art_review": False, "product_exposed": False,
              "golden_promotion": False, "m2_promotion": False}
    paths = {
        "plan": str(plan.resolve()), "machine_report": str(machine.resolve()),
        "cpu_preflight_report": str(cpu_report.resolve()),
        "source_binding_manifest": str(source_path.resolve()),
        "billiards_binary": str(BILLIARDS.resolve()), "font": str(v1.FONT),
        "hud_token": str(v1.HUD_TOKEN), "shell_token": str(v1.SHELL_TOKEN),
        "dispatcher": str(DISPATCHER), "dispatch_validator": str(DISPATCH_VALIDATOR),
        "runner_source": str(RUNNER_SOURCE), "runner_binary": str(RUNNER.resolve()),
        "cmake": str(ROOT / "CMakeLists.txt"),
    }
    frozen = {
        "gate_sha256": v1.machine.GATE_SHA,
        "producer_correction_gate_sha256": GATE_SHA,
        "plan_sha256": v1.sha(plan), "machine_report_sha256": v1.sha(machine),
        "cpu_preflight_report_sha256": v1.sha(cpu_report),
        "source_binding_manifest_sha256": v1.sha(source_path),
        "source_aggregate_sha256": manifest["aggregate_sha256"],
        "billiards_binary_sha256": v1.sha(BILLIARDS),
        "font_sha256": v1.sha(v1.FONT), "hud_token_sha256": v1.sha(v1.HUD_TOKEN),
        "shell_token_sha256": v1.sha(v1.SHELL_TOKEN),
        "dispatcher_sha256": v1.sha(DISPATCHER),
        "dispatch_validator_sha256": v1.sha(DISPATCH_VALIDATOR),
        "runner_source_sha256": v1.sha(RUNNER_SOURCE),
        "runner_binary_sha256": v1.sha(RUNNER),
        "cmake_sha256": v1.sha(ROOT / "CMakeLists.txt"),
    }
    ready = {
        "schema": "phase5_rules_decision_ui_v4_exact16_readiness_v3",
        "status": "V4_EXACT16_PRODUCER_BOUND_NOT_RUN_NO_AUTH",
        "case_count": 16, "repair_case_count": 4,
        "regression_control_count": 12, "case_ids_in_order": ids,
        "case_results": [{"ordinal": i + 1, "capture_id": item, "status": "NOT_RUN"}
                         for i, item in enumerate(ids)],
        "capture_producer": {
            "target": "BilliardsPhase5RulesDecisionUiV4Exact16GpuCaptureRunner",
            "compile_definition": "BILLIARDGL_PHASE5_RULES_UI_V4_EXACT16=1",
            "mode_exclusive": True,
        },
        "dispatch_gate": {
            "single_use_dispatcher_implemented": True,
            "compatible_capture_producer_bound": True,
            "authorization_may_not_be_issued": False,
            "authorization_requires_separate_root_review": True,
            "preprocess_o_excl_flush_fsync_consumption": True,
            "failure_permanently_consumed_and_bytes_preserved": True,
            "exact16_result_telemetry_summary_validation": True,
        },
        "paths": paths, "frozen_bindings": frozen, "claims": claims,
    }
    ready_path = output / "rules-decision-ui-v4-exact16-ready.json"
    write(ready_path, ready)
    auth = {
        "schema": "phase5_rules_decision_ui_v4_exact16_gpu_authorization_v2",
        "status": "NOT_ISSUED", "authorized": False, "consumed": False,
        "single_use": True, "authorization_id": None,
        "authorization_nonce": None, "approved_by": None, "approved_at": None,
        "expires_unix_seconds": None, "authorized_output_canonical_path": None,
        "case_count": 16, "scene_ids_in_order": ids,
        "ready_manifest_sha256": v1.sha(ready_path),
        "dispatcher_sha256": frozen["dispatcher_sha256"],
        "runner_binary_path": str(RUNNER.resolve()),
        "runner_binary_sha256": frozen["runner_binary_sha256"],
        "runner_source_sha256": frozen["runner_source_sha256"],
        "plan_sha256": frozen["plan_sha256"],
        "source_binding_manifest_sha256": frozen["source_binding_manifest_sha256"],
        "source_aggregate_sha256": frozen["source_aggregate_sha256"],
        "billiards_binary_sha256": frozen["billiards_binary_sha256"],
        "font_sha256": frozen["font_sha256"],
        "hud_token_sha256": frozen["hud_token_sha256"],
        "shell_token_sha256": frozen["shell_token_sha256"],
        "exact16_capture_producer_contract": True, **claims,
    }
    auth_path = output / "rules-decision-ui-v4-authorization-template-not-issued.json"
    write(auth_path, auth)
    receipt = {"schema": "phase5_rules_decision_ui_v4_exact16_offline_readiness_v3",
               "status": "PRODUCER_BOUND_EXACT16_NOT_RUN_NO_AUTH",
               "readiness_sha256": v1.sha(ready_path),
               "authorization_template_sha256": v1.sha(auth_path),
               "source_binding_manifest_sha256": v1.sha(source_path),
               "runner_binary_sha256": v1.sha(RUNNER), "claims": claims}
    receipt_path = output / "offline-readiness-receipt.json"
    write(receipt_path, receipt)
    return {"readiness": ready_path, "authorization": auth_path,
            "sources": source_path, "receipt": receipt_path}


if __name__ == "__main__":
    parser = argparse.ArgumentParser(); parser.add_argument("--output", type=Path, default=OUTPUT)
    try:
        print(json.dumps({k: str(v) for k, v in prepare(parser.parse_args().output).items()},
                         sort_keys=True))
    except (OSError, ValueError, json.JSONDecodeError, RuntimeError,
            subprocess.CalledProcessError) as error:
        print("FAIL: " + str(error)); raise SystemExit(2)
