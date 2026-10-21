#!/usr/bin/env python3
"""Read-only validator for producer-bound Phase5 exact16 readiness v3."""

from __future__ import annotations

import argparse
import json
from pathlib import Path
import sys

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT))
from scripts import prepare_phase5_rules_decision_ui_v4_exact16_producer_bound_ready_v3 as prep
from scripts import prepare_phase5_rules_decision_ui_v4_exact16_ready as v1
from scripts import run_phase5_rules_decision_ui_v4_exact16_capture_dispatch as dispatch


def require(value: bool, message: str) -> None:
    if not value:
        raise RuntimeError(message)


def validate(root: Path = prep.OUTPUT) -> dict:
    root = root.resolve(strict=True)
    paths = {name: root / file for name, file in {
        "ready": "rules-decision-ui-v4-exact16-ready.json",
        "auth": "rules-decision-ui-v4-authorization-template-not-issued.json",
        "sources": "rules-decision-ui-v4-source-bindings.json",
        "receipt": "offline-readiness-receipt.json",
        "plan": "rules-decision-ui-v4-exact16-plan.tsv",
        "machine": "rules-decision-ui-v4-machine.json",
        "cpu": "rules-decision-ui-v4-exact16-cpu-preflight.json"}.items()}
    require(all(path.is_file() for path in paths.values()), "v3 artifact set incomplete")
    require(not any(root.rglob("*.ppm")) and not any(root.rglob("*.png")) and
            not any(root.rglob("*.consumed*")), "v3 contains capture/consumption evidence")
    ready = json.loads(paths["ready"].read_text())
    auth = json.loads(paths["auth"].read_text())
    sources = json.loads(paths["sources"].read_text())
    receipt = json.loads(paths["receipt"].read_text())
    ids = v1.case_ids()
    claims = {"gpu_authorized": False, "gpu_executed": False,
              "technical_art_review": False, "product_exposed": False,
              "golden_promotion": False, "m2_promotion": False}
    require(ready.get("schema") == "phase5_rules_decision_ui_v4_exact16_readiness_v3" and
            ready.get("status") == "V4_EXACT16_PRODUCER_BOUND_NOT_RUN_NO_AUTH" and
            ready.get("case_count") == 16 and ready.get("case_ids_in_order") == ids and
            ready.get("claims") == claims and ready.get("case_results") == [
                {"ordinal": i + 1, "capture_id": item, "status": "NOT_RUN"}
                for i, item in enumerate(ids)], "v3 identity/scope/claims mismatch")
    require(ready.get("capture_producer") == {
        "target": "BilliardsPhase5RulesDecisionUiV4Exact16GpuCaptureRunner",
        "compile_definition": "BILLIARDGL_PHASE5_RULES_UI_V4_EXACT16=1",
        "mode_exclusive": True}, "v3 producer identity mismatch")
    gate = ready.get("dispatch_gate", {})
    require(gate.get("compatible_capture_producer_bound") is True and
            gate.get("authorization_may_not_be_issued") is False and
            gate.get("authorization_requires_separate_root_review") is True and
            gate.get("preprocess_o_excl_flush_fsync_consumption") is True and
            gate.get("failure_permanently_consumed_and_bytes_preserved") is True,
            "v3 dispatch/signing boundary mismatch")
    frozen = ready["frozen_bindings"]
    mapping = {"plan_sha256": "plan", "machine_report_sha256": "machine_report",
               "cpu_preflight_report_sha256": "cpu_preflight_report",
               "source_binding_manifest_sha256": "source_binding_manifest",
               "billiards_binary_sha256": "billiards_binary", "font_sha256": "font",
               "hud_token_sha256": "hud_token", "shell_token_sha256": "shell_token",
               "dispatcher_sha256": "dispatcher",
               "dispatch_validator_sha256": "dispatch_validator",
               "runner_source_sha256": "runner_source",
               "runner_binary_sha256": "runner_binary", "cmake_sha256": "cmake"}
    for field, key in mapping.items():
        live = Path(ready["paths"][key])
        require(live.is_file() and v1.sha(live) == frozen[field],
                "v3 live binding drift: " + field)
    require(frozen.get("producer_correction_gate_sha256") == prep.GATE_SHA and
            v1.sha(prep.GATE) == prep.GATE_SHA, "producer gate binding drift")
    require(paths["plan"].read_text().splitlines() == v1.plan_rows(),
            "v3 exact plan drift")
    cpu = json.loads(paths["cpu"].read_text())
    require(cpu.get("case_count") == cpu.get("passed_count") == 16 and
            cpu.get("bad_input_count") == cpu.get("bad_input_passed_count") == 5 and
            cpu.get("actual_gpu_invoked") is False, "v3 CPU preflight mismatch")
    require(sources.get("schema") ==
            "phase5_rules_decision_ui_v4_exact16_source_bindings_v3" and
            sources.get("capture_target") == ready["capture_producer"]["target"] and
            sources.get("compile_definition") == ready["capture_producer"]["compile_definition"] and
            sources.get("aggregate_sha256") == frozen["source_aggregate_sha256"] and
            v1.aggregate(sources["files"]) == sources["aggregate_sha256"],
            "v3 source manifest contract mismatch")
    for row in sources["files"]:
        path = ROOT / row["path"]
        require(path.is_file() and v1.sha(path) == row["sha256"],
                "v3 source drift: " + row["path"])
    require(auth.get("schema") == dispatch.AUTH_SCHEMA and
            auth.get("status") == "NOT_ISSUED" and auth.get("authorized") is False and
            auth.get("consumed") is False and auth.get("case_count") == 16 and
            auth.get("scene_ids_in_order") == ids and
            auth.get("runner_binary_path") == ready["paths"]["runner_binary"] and
            auth.get("runner_binary_sha256") == frozen["runner_binary_sha256"] and
            auth.get("exact16_capture_producer_contract") is True and
            all(auth.get(field) is False for field in claims),
            "v3 auth template must remain NOT_ISSUED but producer-bound")
    require(receipt.get("status") == "PRODUCER_BOUND_EXACT16_NOT_RUN_NO_AUTH" and
            receipt.get("readiness_sha256") == v1.sha(paths["ready"]) and
            receipt.get("authorization_template_sha256") == v1.sha(paths["auth"]) and
            receipt.get("claims") == claims, "v3 receipt mismatch")
    dispatch.validate_ready(paths["ready"])
    return {"status": "PASS_PHASE5_V4_EXACT16_PRODUCER_BOUND_READY_NO_GPU_NO_AUTH",
            "readiness_sha256": v1.sha(paths["ready"]),
            "runner_binary_sha256": frozen["runner_binary_sha256"],
            "case_count": 16, "authorization_issued": False,
            "gpu_executed": False}


if __name__ == "__main__":
    parser = argparse.ArgumentParser(); parser.add_argument("root", type=Path, nargs="?", default=prep.OUTPUT)
    try:
        print(json.dumps(validate(parser.parse_args().root), sort_keys=True))
    except (OSError, ValueError, json.JSONDecodeError, RuntimeError) as error:
        print("FAIL: " + str(error)); raise SystemExit(2)
