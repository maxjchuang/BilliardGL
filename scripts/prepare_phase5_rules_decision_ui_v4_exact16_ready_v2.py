#!/usr/bin/env python3
"""Prepare Phase5 v4 exact16 dispatcher-aware NOT_RUN readiness v2."""

from __future__ import annotations

import argparse
import json
from pathlib import Path
import shutil
import sys

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT))
from scripts import prepare_phase5_rules_decision_ui_v4_exact16_ready as v1
from scripts import validate_phase5_rules_decision_ui_v4_exact16_ready as v1_validator

DEFAULT_V1 = ROOT / "docs/evidence/phase5/rules-decision-ui-v4-exact16-ready-v1"
DEFAULT_OUTPUT = ROOT / "docs/evidence/phase5/rules-decision-ui-v4-exact16-ready-v2"
DISPATCHER = ROOT / "scripts/run_phase5_rules_decision_ui_v4_exact16_capture_dispatch.py"
DISPATCH_VALIDATOR = ROOT / "scripts/validate_phase5_rules_decision_ui_v4_exact16_capture_dispatch.py"
RUNNER_SOURCE = ROOT / "spikes/render_core_profile/phase5_rules_decision_ui_gpu_capture_runner.mm"
NEW_SOURCES = (
    "scripts/run_phase5_rules_decision_ui_v4_exact16_capture_dispatch.py",
    "scripts/validate_phase5_rules_decision_ui_v4_exact16_capture_dispatch.py",
    "scripts/prepare_phase5_rules_decision_ui_v4_exact16_ready_v2.py",
    "scripts/validate_phase5_rules_decision_ui_v4_exact16_ready_v2.py",
    "tests/visual/test_phase5_rules_decision_ui_v4_exact16_capture_dispatch.py",
    "tests/visual/test_phase5_rules_decision_ui_v4_exact16_ready_v2.py",
)


def require(condition: bool, message: str) -> None:
    if not condition:
        raise RuntimeError(message)


def write_json(path: Path, value: object) -> None:
    path.write_text(json.dumps(value, ensure_ascii=False, indent=2,
                              sort_keys=True) + "\n", encoding="utf-8")


def prepare(v1_root: Path, output: Path) -> dict[str, Path]:
    v1_root = v1_root.resolve(strict=True)
    require(not output.exists(), "fresh v2 readiness root required")
    v1_result = v1_validator.validate(v1_root)
    require(v1_result["case_count"] == 16, "v1 lineage validation failed")
    require(all((ROOT / path).is_file() for path in NEW_SOURCES),
            "v2 dispatcher source set incomplete")
    output.mkdir(parents=True)
    copied = {
        "plan": "rules-decision-ui-v4-exact16-plan.tsv",
        "machine_report": "rules-decision-ui-v4-machine.json",
        "cpu_preflight_report": "rules-decision-ui-v4-exact16-cpu-preflight.json",
    }
    for name in copied.values():
        shutil.copyfile(v1_root / name, output / name)
    old_sources = json.loads((v1_root /
        "rules-decision-ui-v4-source-bindings.json").read_text())
    by_path = {row["path"]: row for row in old_sources["files"]}
    for relative in NEW_SOURCES:
        by_path[relative] = {"path": relative, "sha256": v1.sha(ROOT / relative),
                             "scope": "v4_offline_dispatch_or_validation"}
    rows = [by_path[key] for key in sorted(by_path)]
    sources = {
        "schema": "phase5_rules_decision_ui_v4_exact16_source_bindings_v2",
        "gate_sha256": v1.machine.GATE_SHA, "file_count": len(rows),
        "aggregate_sha256": v1.aggregate(rows), "files": rows,
    }
    sources_path = output / "rules-decision-ui-v4-source-bindings.json"
    write_json(sources_path, sources)
    v1_ready_path = v1_root / "rules-decision-ui-v4-exact16-ready.json"
    paths = {
        "plan": str((output / copied["plan"]).resolve()),
        "machine_report": str((output / copied["machine_report"]).resolve()),
        "cpu_preflight_report": str((output / copied["cpu_preflight_report"]).resolve()),
        "source_binding_manifest": str(sources_path.resolve()),
        "billiards_binary": str(Path(json.loads(v1_ready_path.read_text())["paths"]["billiards_binary"])),
        "font": str(v1.FONT), "hud_token": str(v1.HUD_TOKEN),
        "shell_token": str(v1.SHELL_TOKEN),
        "dispatcher": str(DISPATCHER), "dispatch_validator": str(DISPATCH_VALIDATOR),
        "runner_source_read_only": str(RUNNER_SOURCE),
    }
    frozen = {
        "gate_sha256": v1.machine.GATE_SHA,
        "v1_readiness_sha256": v1.sha(v1_ready_path),
        "plan_sha256": v1.sha(Path(paths["plan"])),
        "machine_report_sha256": v1.sha(Path(paths["machine_report"])),
        "cpu_preflight_report_sha256": v1.sha(Path(paths["cpu_preflight_report"])),
        "source_binding_manifest_sha256": v1.sha(sources_path),
        "source_aggregate_sha256": sources["aggregate_sha256"],
        "billiards_binary_sha256": v1.sha(Path(paths["billiards_binary"])),
        "font_sha256": v1.sha(v1.FONT), "hud_token_sha256": v1.sha(v1.HUD_TOKEN),
        "shell_token_sha256": v1.sha(v1.SHELL_TOKEN),
        "dispatcher_sha256": v1.sha(DISPATCHER),
        "dispatch_validator_sha256": v1.sha(DISPATCH_VALIDATOR),
        "runner_source_sha256": v1.sha(RUNNER_SOURCE),
    }
    ids = v1.case_ids()
    claims = {"gpu_authorized": False, "gpu_executed": False,
              "technical_art_review": False, "product_exposed": False,
              "golden_promotion": False, "m2_promotion": False}
    ready = {
        "schema": "phase5_rules_decision_ui_v4_exact16_readiness_v2",
        "status": "V4_EXACT16_DISPATCH_READY_NOT_RUN_NO_AUTH",
        "case_count": 16, "repair_case_count": 4,
        "regression_control_count": 12, "case_ids_in_order": ids,
        "case_results": [{"ordinal": index + 1, "capture_id": case_id,
                          "status": "NOT_RUN"}
                         for index, case_id in enumerate(ids)],
        "machine_matrix_passed": 540, "cpu_preflight_passed": 16,
        "bad_input_passed": 5, "product_integration_passed": True,
        "dispatch_gate": {
            "single_use_dispatcher_implemented": True,
            "preprocess_o_excl_flush_fsync_consumption": True,
            "failure_permanently_consumed_and_bytes_preserved": True,
            "exact16_result_telemetry_summary_validation": True,
            "compatible_capture_producer_bound": False,
            "authorization_may_not_be_issued": True,
            "reason": "current capture-only runner accepts exact28 or 60, not exact16",
        },
        "paths": paths, "frozen_bindings": frozen, "claims": claims,
    }
    ready_path = output / "rules-decision-ui-v4-exact16-ready.json"
    write_json(ready_path, ready)
    auth = {
        "schema": "phase5_rules_decision_ui_v4_exact16_gpu_authorization_v2",
        "status": "NOT_ISSUED_INCOMPATIBLE_CAPTURE_PRODUCER_NOT_BOUND",
        "authorized": False, "consumed": False, "single_use": True,
        "authorization_id": None, "authorization_nonce": None,
        "approved_by": None, "approved_at": None, "expires_unix_seconds": None,
        "authorized_output_canonical_path": None,
        "case_count": 16, "scene_ids_in_order": ids,
        "ready_manifest_sha256": v1.sha(ready_path),
        "dispatcher_sha256": frozen["dispatcher_sha256"],
        "runner_binary_path": None, "runner_binary_sha256": None,
        "runner_source_sha256": frozen["runner_source_sha256"],
        "plan_sha256": frozen["plan_sha256"],
        "source_binding_manifest_sha256": frozen["source_binding_manifest_sha256"],
        "source_aggregate_sha256": frozen["source_aggregate_sha256"],
        "billiards_binary_sha256": frozen["billiards_binary_sha256"],
        "font_sha256": frozen["font_sha256"],
        "hud_token_sha256": frozen["hud_token_sha256"],
        "shell_token_sha256": frozen["shell_token_sha256"],
        "exact16_capture_producer_contract": False, **claims,
    }
    auth_path = output / "rules-decision-ui-v4-authorization-template-not-issued.json"
    write_json(auth_path, auth)
    receipt = {
        "schema": "phase5_rules_decision_ui_v4_exact16_offline_readiness_v2",
        "status": "DISPATCHER_PASS_EXACT16_NOT_RUN_PRODUCER_NOT_BOUND_NO_AUTH",
        "readiness_sha256": v1.sha(ready_path),
        "authorization_template_sha256": v1.sha(auth_path),
        "source_binding_manifest_sha256": v1.sha(sources_path),
        "v1_readiness_sha256": frozen["v1_readiness_sha256"], "claims": claims,
    }
    receipt_path = output / "offline-readiness-receipt.json"
    write_json(receipt_path, receipt)
    return {"readiness": ready_path, "authorization": auth_path,
            "receipt": receipt_path, "sources": sources_path}


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--v1-root", type=Path, default=DEFAULT_V1)
    parser.add_argument("--output-root", type=Path, default=DEFAULT_OUTPUT)
    try:
        arguments = parser.parse_args()
        print(json.dumps({key: str(path) for key, path in
                          prepare(arguments.v1_root,
                                  arguments.output_root).items()}, sort_keys=True))
    except (OSError, ValueError, json.JSONDecodeError, RuntimeError) as error:
        print("FAIL: " + str(error))
        return 2
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
