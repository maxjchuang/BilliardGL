#!/usr/bin/env python3
"""Prepare Phase5 v5 pixel-visible exact16 NOT_RUN readiness (NO GPU/auth)."""

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
from scripts import prepare_phase5_rules_decision_ui_v4_exact16_producer_bound_ready_v3 as v3
from scripts import validate_phase5_rules_decision_ui_v5_pixel_visible_shape_repair_gate as gate_validator

OUTPUT = ROOT / "docs/evidence/phase5/rules-decision-ui-v5-pixel-visible-exact16-ready-v1"
BUILD = ROOT / "build-integrated-final-shared-phase4-v10-phase5-v5-phase2-v9"
BILLIARDS = BUILD / "Billiards"
RUNNER = BUILD / "BilliardsPhase5RulesDecisionUiV4Exact16GpuCaptureRunner"
CPU_PREFLIGHT = BUILD / "BilliardsPhase5RulesDecisionUiV4Exact16CpuFramePreflight"
CPU_RASTER = BUILD / "BilliardsPhase5RulesDecisionUiV5CpuRasterTests"
PRODUCT = BUILD / "BilliardsRulesDecisionProductRuntimeTests"
CPU_ROOT = ROOT / "docs/evidence/phase5/rules-decision-ui-v5-final-cpu-raster-ready-v1"
CPU_REPORT = CPU_ROOT / "final-cpu-raster-report.json"
GATE = ROOT / "docs/art/phase5-rules-decision-ui-v5-pixel-visible-shape-repair-design-gate-v1.json"
GATE_SHA = "657f3ab6f4ccac4db23fb0ab72889bc49d091f27fd5f63bd5610b736b1aa2a10"
RUNNER_SOURCE = ROOT / "spikes/render_core_profile/phase5_rules_decision_ui_gpu_capture_runner.mm"
DISPATCHER = ROOT / "scripts/run_phase5_rules_decision_ui_v4_exact16_capture_dispatch.py"
DISPATCH_VALIDATOR = ROOT / "scripts/validate_phase5_rules_decision_ui_v4_exact16_capture_dispatch.py"
ALLOWLIST = {
    "src/Billiards/render/ui/rules_decision_ui_draw_list.cpp":
        ("fe5522e17c1c39ae57fd87aa87a0f9881dc95ad6cf4d35880694f8d6a8560231",
         "4c383c1d842ec338a2d0a2dc482e235448269b3a91df43e753824273c6015a21"),
    "src/Billiards/render/ui/rules_decision_product_draw.cpp":
        ("68f0b8e65d7220a2c362e66c00b198d89e1b3ac44333f2682cf95cd8d30a3f80",
         "fd33c09048dc5eb4d88f781df4ed33ab00bc340746f8a8140facc18feccc57cd"),
}
ADDITIONAL_SOURCES = (
    "docs/art/phase5-rules-decision-ui-v5-pixel-visible-shape-repair-design-gate-v1.json",
    "scripts/validate_phase5_rules_decision_ui_v5_pixel_visible_shape_repair_gate.py",
    "scripts/validate_phase5_rules_decision_ui_v5_pixel_visible_implementation.py",
    "tests/visual/test_phase5_rules_decision_ui_v5_pixel_visible_implementation.py",
    "tests/phase5_rules_decision_ui_v5_cpu_raster_tests.cpp",
    "scripts/prepare_phase5_rules_decision_ui_v5_pixel_visible_exact16_ready.py",
    "scripts/validate_phase5_rules_decision_ui_v5_pixel_visible_exact16_ready.py",
    "tests/visual/test_phase5_rules_decision_ui_v5_pixel_visible_exact16_ready.py",
)


def require(value: bool, message: str) -> None:
    if not value:
        raise RuntimeError(message)


def write(path: Path, value: object) -> None:
    path.write_text(json.dumps(value, indent=2, sort_keys=True) + "\n",
                    encoding="utf-8")


def snapshot_manifest() -> dict:
    report = json.loads(CPU_REPORT.read_text(encoding="utf-8"))
    require(report.get("status") ==
            "PASS_16_SNAPSHOTS_8_PALETTE_PROFILES_BASE_PDT_NO_GPU" and
            report.get("snapshot_case_count") == 16 and
            report.get("repair_case_count") == 4 and
            report.get("control_case_count") == 12 and
            report.get("bad_input_count") ==
            report.get("bad_input_passed_count") == 12 and
            report.get("actual_gpu_invoked") is False,
            "final CPU raster report invalid")
    files = []
    for path in sorted((CPU_ROOT / "snapshots").rglob("*")):
        if path.is_file():
            files.append({"path": str(path.relative_to(ROOT)),
                          "sha256": v1.sha(path), "bytes": path.stat().st_size})
    require(len(files) == 32 and
            len([row for row in files if row["path"].endswith(".ppm")]) == 16 and
            len([row for row in files if row["path"].endswith(".pgm")]) == 16,
            "final CPU raster snapshot inventory invalid")
    return {
        "schema": "phase5_rules_decision_ui_v5_cpu_raster_manifest_v1",
        "report_path": str(CPU_REPORT.relative_to(ROOT)),
        "report_sha256": v1.sha(CPU_REPORT),
        "snapshot_aggregate_sha256": report["snapshot_aggregate_sha256"],
        "file_count": len(files), "files": files,
        "file_manifest_aggregate_sha256": v1.aggregate(files),
        "actual_gpu_invoked": False, "authorization_issued": False,
    }


def lineage() -> dict:
    expected = gate_validator.SOURCE_BINDINGS
    drift = []
    for relative, old_sha in expected.items():
        current = v1.sha(ROOT / relative)
        if current != old_sha:
            drift.append(relative)
        if relative not in ALLOWLIST:
            require(current == old_sha,
                    "non-allowlisted v5 gate diagnosis drift: " + relative)
    require(set(drift) == set(ALLOWLIST),
            "superseded v5 gate drift must be exact production allowlist")
    for relative, pair in ALLOWLIST.items():
        require((expected[relative], v1.sha(ROOT / relative)) == pair,
                "approved source lineage mismatch: " + relative)
    return {
        "supersedes_pre_implementation_live_binding_check": True,
        "old_gate_validator_expected_to_fail_after_authorized_implementation": True,
        "old_gate_and_failure_receipt_bytes_unchanged": True,
        "approved_production_drift_only": [
            {"path": relative, "pre_implementation_sha256": pair[0],
             "post_implementation_sha256": pair[1]}
            for relative, pair in sorted(ALLOWLIST.items())],
        "unapproved_drift_count": 0,
    }


def prepare(output: Path = OUTPUT) -> dict[str, Path]:
    require(not output.exists(), "fresh Phase5 v5 readiness root required")
    required = (BILLIARDS, RUNNER, CPU_PREFLIGHT, CPU_RASTER, PRODUCT,
                CPU_REPORT, GATE, RUNNER_SOURCE, DISPATCHER, DISPATCH_VALIDATOR)
    require(all(path.is_file() for path in required) and v1.sha(GATE) == GATE_SHA,
            "Phase5 v5 readiness input missing/drifted")
    require(all((ROOT / path).is_file() for path in ADDITIONAL_SOURCES),
            "Phase5 v5 readiness source inventory incomplete")
    successor = lineage()
    output.mkdir(parents=True)
    plan = output / "rules-decision-ui-v5-exact16-plan.tsv"
    shutil.copyfile(v3.OUTPUT / "rules-decision-ui-v4-exact16-plan.tsv", plan)
    require(v1.sha(plan) ==
            "426de40ba87b8cfdb5eefc88464720507e53ced2190ffb61564e7cd848eb593f",
            "exact16 frozen plan drift")
    machine = output / "rules-decision-ui-v5-machine-540.json"
    subprocess.run([sys.executable,
                    str(ROOT / "scripts/run_phase5_rules_decision_ui_v4_exact_shape_machine.py"),
                    "--output", str(machine)], cwd=ROOT, check=True)
    cpu_preflight = output / "rules-decision-ui-v5-exact16-cpu-preflight.json"
    subprocess.run([str(CPU_PREFLIGHT), str(plan), str(cpu_preflight)],
                   cwd=ROOT, check=True)
    raster_manifest = output / "rules-decision-ui-v5-final-cpu-raster-manifest.json"
    write(raster_manifest, snapshot_manifest())

    old_sources = json.loads((v3.OUTPUT /
        "rules-decision-ui-v4-source-bindings.json").read_text(encoding="utf-8"))
    rows = {row["path"]: row for row in old_sources["files"]}
    for relative in list(rows):
        require((ROOT / relative).is_file(), "source missing: " + relative)
        rows[relative] = {**rows[relative], "sha256": v1.sha(ROOT / relative)}
    for relative in ADDITIONAL_SOURCES:
        rows[relative] = {"path": relative, "sha256": v1.sha(ROOT / relative),
                          "scope": "v5_pixel_visible_offline_or_capture_contract"}
    ordered = [rows[key] for key in sorted(rows)]
    sources = {
        "schema": "phase5_rules_decision_ui_v5_exact16_source_bindings_v1",
        "gate_sha256": GATE_SHA,
        "capture_target": "BilliardsPhase5RulesDecisionUiV4Exact16GpuCaptureRunner",
        "compile_definition": "BILLIARDGL_PHASE5_RULES_UI_V4_EXACT16=1",
        "runner_source_sha256": v1.sha(RUNNER_SOURCE),
        "runner_binary_sha256": v1.sha(RUNNER),
        "file_count": len(ordered), "files": ordered,
        "aggregate_sha256": v1.aggregate(ordered),
    }
    source_path = output / "rules-decision-ui-v5-source-bindings.json"
    write(source_path, sources)
    ids = v1.case_ids()
    claims = {"gpu_authorized": False, "gpu_executed": False,
              "technical_art_review": False, "product_exposed": False,
              "golden_promotion": False, "m2_promotion": False}
    paths = {
        "plan": str(plan.resolve()), "machine_report": str(machine.resolve()),
        "cpu_preflight_report": str(cpu_preflight.resolve()),
        "cpu_raster_report": str(CPU_REPORT.resolve()),
        "cpu_raster_manifest": str(raster_manifest.resolve()),
        "source_binding_manifest": str(source_path.resolve()),
        "billiards_binary": str(BILLIARDS.resolve()), "font": str(v1.FONT),
        "hud_token": str(v1.HUD_TOKEN), "shell_token": str(v1.SHELL_TOKEN),
        "dispatcher": str(DISPATCHER), "dispatch_validator": str(DISPATCH_VALIDATOR),
        "runner_source": str(RUNNER_SOURCE), "runner_binary": str(RUNNER.resolve()),
        "cmake": str(ROOT / "CMakeLists.txt"), "gate": str(GATE),
    }
    frozen = {
        "gate_sha256": GATE_SHA, "plan_sha256": v1.sha(plan),
        "machine_report_sha256": v1.sha(machine),
        "cpu_preflight_report_sha256": v1.sha(cpu_preflight),
        "cpu_raster_report_sha256": v1.sha(CPU_REPORT),
        "cpu_raster_manifest_sha256": v1.sha(raster_manifest),
        "cpu_raster_snapshot_aggregate_sha256":
            json.loads(CPU_REPORT.read_text())["snapshot_aggregate_sha256"],
        "source_binding_manifest_sha256": v1.sha(source_path),
        "source_aggregate_sha256": sources["aggregate_sha256"],
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
        "schema": "phase5_rules_decision_ui_v5_exact16_readiness_v1",
        "status": "V5_PIXEL_VISIBLE_EXACT16_PRODUCER_BOUND_NOT_RUN_NO_AUTH",
        "case_count": 16, "repair_case_count": 4,
        "regression_control_count": 12, "case_ids_in_order": ids,
        "case_results": [{"ordinal": index + 1, "capture_id": item,
                          "status": "NOT_RUN"}
                         for index, item in enumerate(ids)],
        "capture_producer": {
            "target": "BilliardsPhase5RulesDecisionUiV4Exact16GpuCaptureRunner",
            "compile_definition": "BILLIARDGL_PHASE5_RULES_UI_V4_EXACT16=1",
            "mode_exclusive": True},
        "dispatch_gate": {
            "single_use_dispatcher_implemented": True,
            "compatible_capture_producer_bound": True,
            "authorization_may_not_be_issued": False,
            "authorization_requires_separate_root_review": True,
            "preprocess_o_excl_flush_fsync_consumption": True,
            "failure_permanently_consumed_and_bytes_preserved": True,
            "exact16_result_telemetry_summary_validation": True},
        "successor_lineage": successor, "paths": paths,
        "frozen_bindings": frozen, "claims": claims,
    }
    ready_path = output / "rules-decision-ui-v5-exact16-ready.json"
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
    auth_path = output / "rules-decision-ui-v5-authorization-template-not-issued.json"
    write(auth_path, auth)
    receipt = {
        "schema": "phase5_rules_decision_ui_v5_exact16_offline_readiness_v1",
        "status": "PIXEL_VISIBLE_PRODUCER_BOUND_EXACT16_NOT_RUN_NO_AUTH",
        "readiness_sha256": v1.sha(ready_path),
        "authorization_template_sha256": v1.sha(auth_path),
        "source_binding_manifest_sha256": v1.sha(source_path),
        "cpu_raster_manifest_sha256": v1.sha(raster_manifest),
        "runner_binary_sha256": v1.sha(RUNNER), "claims": claims,
    }
    receipt_path = output / "offline-readiness-receipt.json"
    write(receipt_path, receipt)
    return {"readiness": ready_path, "authorization": auth_path,
            "sources": source_path, "cpu_raster": raster_manifest,
            "receipt": receipt_path}


if __name__ == "__main__":
    parser = argparse.ArgumentParser()
    parser.add_argument("--output", type=Path, default=OUTPUT)
    try:
        print(json.dumps({key: str(value) for key, value in
                          prepare(parser.parse_args().output).items()}, sort_keys=True))
    except (OSError, ValueError, json.JSONDecodeError, RuntimeError,
            subprocess.CalledProcessError) as error:
        print("FAIL: " + str(error))
        raise SystemExit(2)
