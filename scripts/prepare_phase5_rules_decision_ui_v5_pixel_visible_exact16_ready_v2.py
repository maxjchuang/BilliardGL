#!/usr/bin/env python3
"""Prepare corrected-producer Phase5 v5 exact16 v2 readiness (NO GPU/auth)."""

from __future__ import annotations

import argparse
import json
from pathlib import Path
import shutil
import sys

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT))
from scripts import prepare_phase5_rules_decision_ui_v4_exact16_ready as base
from scripts import prepare_phase5_rules_decision_ui_v5_pixel_visible_exact16_ready as v1

DEFAULT_OUTPUT = ROOT / "docs/evidence/phase5/rules-decision-ui-v5-pixel-visible-exact16-corrected-ready-v2"
GATE = ROOT / "docs/art/phase5-rules-decision-ui-v5-exact16-producer-contract-correction-gate-v1.json"
GATE_SHA = "b0bf1808f381b2dc5bc7a89bd8001ee08565f95227d0194db771722187df336a"
FAILURE = ROOT / "docs/evidence/phase5/rules-decision-ui-v5-exact16-preordinal-contract-failure-receipt-v1.json"
FAILURE_SHA = "4d261779bdcb4c9efa938147267425bfd3fb20685cd57b5482ca91cb79ddd588"
RUNNER_SOURCE = ROOT / "spikes/render_core_profile/phase5_rules_decision_ui_gpu_capture_runner.mm"
DISPATCHER = ROOT / "scripts/run_phase5_rules_decision_ui_v4_exact16_capture_dispatch.py"
DISPATCH_VALIDATOR = ROOT / "scripts/validate_phase5_rules_decision_ui_v4_exact16_capture_dispatch.py"
TARGET = "BilliardsPhase5RulesDecisionUiV5Exact16GpuCaptureRunner"
EXTRA_SOURCES = (
    "docs/art/phase5-rules-decision-ui-v5-exact16-producer-contract-correction-gate-v1.json",
    "docs/evidence/phase5/rules-decision-ui-v5-exact16-preordinal-contract-failure-receipt-v1.json",
    "scripts/validate_phase5_rules_decision_ui_v5_exact16_preordinal_contract_failure.py",
    "tests/visual/test_phase5_rules_decision_ui_v5_exact16_preordinal_contract_failure.py",
    "scripts/validate_phase5_rules_decision_ui_v5_exact16_producer_contract_correction_gate.py",
    "tests/visual/test_phase5_rules_decision_ui_v5_exact16_producer_contract_correction_gate.py",
    "scripts/prepare_phase5_rules_decision_ui_v5_pixel_visible_exact16_ready_v2.py",
    "scripts/validate_phase5_rules_decision_ui_v5_pixel_visible_exact16_ready_v2.py",
    "tests/visual/test_phase5_rules_decision_ui_v5_pixel_visible_exact16_ready_v2.py",
)


def require(value: bool, message: str) -> None:
    if not value:
        raise RuntimeError(message)


def write(path: Path, value: object) -> None:
    path.write_text(json.dumps(value, indent=2, sort_keys=True) + "\n",
                    encoding="utf-8")


def prepare(build: Path, output: Path = DEFAULT_OUTPUT) -> dict[str, Path]:
    build = build.resolve(strict=True)
    require(not output.exists(), "fresh corrected v2 readiness root required")
    binaries = {name: build / name for name in (
        "Billiards", TARGET,
        "BilliardsPhase5RulesDecisionUiV4Exact16CpuFramePreflight",
        "BilliardsPhase5RulesDecisionUiV5CpuRasterTests",
        "BilliardsRulesDecisionProductRuntimeTests")}
    require(all(path.is_file() for path in binaries.values()),
            "corrected shared build incomplete")
    require(base.sha(GATE) == GATE_SHA and base.sha(FAILURE) == FAILURE_SHA and
            all((ROOT / path).is_file() for path in EXTRA_SOURCES),
            "correction lineage/source input drift")
    output.mkdir(parents=True)
    copies = {
        "plan": "rules-decision-ui-v5-exact16-plan.tsv",
        "machine": "rules-decision-ui-v5-machine-540.json",
        "cpu": "rules-decision-ui-v5-exact16-cpu-preflight.json",
        "raster": "rules-decision-ui-v5-final-cpu-raster-manifest.json",
    }
    for name, filename in copies.items():
        shutil.copyfile(v1.OUTPUT / filename, output / filename)
    plan = output / copies["plan"]
    require(base.sha(plan) ==
            "426de40ba87b8cfdb5eefc88464720507e53ced2190ffb61564e7cd848eb593f",
            "exact16 plan drift")
    old = json.loads((v1.OUTPUT / "rules-decision-ui-v5-source-bindings.json").read_text())
    rows = {row["path"]: row for row in old["files"]}
    for relative in list(rows):
        require((ROOT / relative).is_file(), "source missing: " + relative)
        rows[relative] = {**rows[relative], "sha256": base.sha(ROOT / relative)}
    for relative in EXTRA_SOURCES:
        rows[relative] = {"path": relative, "sha256": base.sha(ROOT / relative),
                          "scope": "v5_capture_only_contract_correction_or_validation"}
    ordered = [rows[key] for key in sorted(rows)]
    sources = {
        "schema": "phase5_rules_decision_ui_v5_exact16_source_bindings_v2",
        "visual_design_gate_sha256": v1.GATE_SHA,
        "producer_correction_gate_sha256": GATE_SHA,
        "preordinal_failure_receipt_sha256": FAILURE_SHA,
        "capture_target": TARGET,
        "compile_definitions": ["BILLIARDGL_PHASE5_RULES_UI_V4_EXACT16=1",
                                "BILLIARDGL_PHASE5_RULES_UI_V5_EXACT16=1"],
        "runner_source_sha256": base.sha(RUNNER_SOURCE),
        "runner_binary_sha256": base.sha(binaries[TARGET]),
        "file_count": len(ordered), "files": ordered,
        "aggregate_sha256": base.aggregate(ordered),
    }
    source_path = output / "rules-decision-ui-v5-source-bindings-v2.json"
    write(source_path, sources)
    ids = base.case_ids()
    claims = {"gpu_authorized": False, "gpu_executed": False,
              "technical_art_review": False, "product_exposed": False,
              "golden_promotion": False, "m2_promotion": False}
    paths = {
        "plan": str(plan.resolve()),
        "machine_report": str((output / copies["machine"]).resolve()),
        "cpu_preflight_report": str((output / copies["cpu"]).resolve()),
        "cpu_raster_report": str(v1.CPU_REPORT.resolve()),
        "cpu_raster_manifest": str((output / copies["raster"]).resolve()),
        "source_binding_manifest": str(source_path.resolve()),
        "billiards_binary": str(binaries["Billiards"].resolve()),
        "font": str(base.FONT), "hud_token": str(base.HUD_TOKEN),
        "shell_token": str(base.SHELL_TOKEN), "dispatcher": str(DISPATCHER),
        "dispatch_validator": str(DISPATCH_VALIDATOR),
        "runner_source": str(RUNNER_SOURCE),
        "runner_binary": str(binaries[TARGET].resolve()),
        "cmake": str(ROOT / "CMakeLists.txt"), "gate": str(GATE),
        "failure_receipt": str(FAILURE),
    }
    frozen = {key: base.sha(Path(paths[path_key])) for key, path_key in {
        "gate_sha256": "gate", "preordinal_failure_receipt_sha256": "failure_receipt",
        "plan_sha256": "plan", "machine_report_sha256": "machine_report",
        "cpu_preflight_report_sha256": "cpu_preflight_report",
        "cpu_raster_report_sha256": "cpu_raster_report",
        "cpu_raster_manifest_sha256": "cpu_raster_manifest",
        "source_binding_manifest_sha256": "source_binding_manifest",
        "billiards_binary_sha256": "billiards_binary", "font_sha256": "font",
        "hud_token_sha256": "hud_token", "shell_token_sha256": "shell_token",
        "dispatcher_sha256": "dispatcher",
        "dispatch_validator_sha256": "dispatch_validator",
        "runner_source_sha256": "runner_source",
        "runner_binary_sha256": "runner_binary", "cmake_sha256": "cmake",
    }.items()}
    frozen["source_aggregate_sha256"] = sources["aggregate_sha256"]
    ready = {
        "schema": "phase5_rules_decision_ui_v5_exact16_readiness_v2",
        "status": "V5_PIXEL_VISIBLE_EXACT16_CORRECTED_PRODUCER_NOT_RUN_NO_AUTH",
        "case_count": 16, "repair_case_count": 4,
        "regression_control_count": 12, "case_ids_in_order": ids,
        "case_results": [{"ordinal": index + 1, "capture_id": item,
                          "status": "NOT_RUN"}
                         for index, item in enumerate(ids)],
        "capture_producer": {"target": TARGET,
            "compile_definitions": sources["compile_definitions"],
            "mode_exclusive": True},
        "dispatch_gate": {"single_use_dispatcher_implemented": True,
            "compatible_capture_producer_bound": True,
            "authorization_may_not_be_issued": False,
            "authorization_requires_separate_root_review": True,
            "preprocess_o_excl_flush_fsync_consumption": True,
            "failure_permanently_consumed_and_bytes_preserved": True,
            "exact16_result_telemetry_summary_validation": True},
        "correction_lineage": {"old_authorization_reuse_forbidden": True,
            "old_failure_receipt_sha256": FAILURE_SHA,
            "contract_only_correction": True,
            "visual_product_authority_rules_unchanged": True},
        "paths": paths, "frozen_bindings": frozen, "claims": claims,
    }
    ready_path = output / "rules-decision-ui-v5-exact16-ready-v2.json"
    write(ready_path, ready)
    auth = {
        "schema": "phase5_rules_decision_ui_v4_exact16_gpu_authorization_v2",
        "status": "NOT_ISSUED", "authorized": False, "consumed": False,
        "single_use": True, "authorization_id": None, "authorization_nonce": None,
        "approved_by": None, "approved_at": None, "expires_unix_seconds": None,
        "authorized_output_canonical_path": None, "case_count": 16,
        "scene_ids_in_order": ids, "ready_manifest_sha256": base.sha(ready_path),
        "dispatcher_sha256": frozen["dispatcher_sha256"],
        "runner_binary_path": paths["runner_binary"],
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
    auth_path = output / "rules-decision-ui-v5-authorization-template-not-issued-v2.json"
    write(auth_path, auth)
    receipt = {"schema": "phase5_rules_decision_ui_v5_exact16_offline_readiness_v2",
        "status": "CORRECTED_PRODUCER_EXACT16_NOT_RUN_NO_AUTH",
        "readiness_sha256": base.sha(ready_path),
        "authorization_template_sha256": base.sha(auth_path),
        "source_binding_manifest_sha256": base.sha(source_path),
        "runner_binary_sha256": frozen["runner_binary_sha256"], "claims": claims}
    receipt_path = output / "offline-readiness-receipt-v2.json"
    write(receipt_path, receipt)
    return {"readiness": ready_path, "authorization": auth_path,
            "sources": source_path, "receipt": receipt_path}


if __name__ == "__main__":
    parser = argparse.ArgumentParser()
    parser.add_argument("--build", type=Path, required=True)
    parser.add_argument("--output", type=Path, default=DEFAULT_OUTPUT)
    args = parser.parse_args()
    try:
        print(json.dumps({key: str(path) for key, path in
                          prepare(args.build, args.output).items()}, sort_keys=True))
    except (OSError, ValueError, json.JSONDecodeError, RuntimeError) as error:
        print("FAIL: " + str(error))
        raise SystemExit(2)
