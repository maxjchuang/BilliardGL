#!/usr/bin/env python3
"""Validate Phase5 v5 corrected exact16 source contract and v2 readiness."""

from __future__ import annotations

import argparse
import hashlib
import json
from pathlib import Path
import sys

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT))
from scripts import prepare_phase5_rules_decision_ui_v4_exact16_ready as base
from scripts import prepare_phase5_rules_decision_ui_v5_pixel_visible_exact16_ready_v2 as prep
from scripts import run_phase5_rules_decision_ui_v4_exact16_capture_dispatch as dispatch


def require(value: bool, message: str) -> None:
    if not value:
        raise RuntimeError(message)


def sha(path: Path) -> str:
    return hashlib.sha256(path.read_bytes()).hexdigest()


def validate_contract_text(cmake: str, runner: str, dispatcher: str) -> None:
    require("add_executable(BilliardsPhase5RulesDecisionUiV5Exact16GpuCaptureRunner" in cmake and
            cmake.count("BILLIARDGL_PHASE5_RULES_UI_V5_EXACT16=1") == 1 and
            cmake.count("BILLIARDGL_PHASE5_RULES_UI_V4_EXACT16=1") >= 2,
            "isolated v5 capture target/definitions missing")
    target = cmake.split(
        "add_executable(BilliardsPhase5RulesDecisionUiV5Exact16GpuCaptureRunner", 1)[1]
    target = target.split("option(BILLIARDGL_PHASE4_FINAL_OWNERSHIP_RUNNER", 1)[0]
    require("${BILLIARDGL_PHASE5_RULES_UI_RUNNER_SOURCE}" in target and
            "BILLIARDGL_PHASE5_RULES_UI_V4_EXACT16=1" in target and
            "BILLIARDGL_PHASE5_RULES_UI_V5_EXACT16=1" in target,
            "v5 target does not reuse exact16 runner contract")
    require("#ifdef BILLIARDGL_PHASE5_RULES_UI_V5_EXACT16" in runner and
            prep.v1.GATE_SHA in runner and
            '"phase5_rules_decision_ui_v5_exact16_readiness_v2"' in runner and
            runner.index(prep.v1.GATE_SHA) < runner.index(
                '"f93ca59634875ed5256f5c22678a55817e0f3e8bf8df582b3b04ac527b886051"'),
            "v5 runner gate/readiness branch missing")
    require("validateAndConsumeAuthorization(receipt, plan" in runner and
            "std::filesystem::create_directories(output);" in runner and
            "[NSApplication sharedApplication]" in runner,
            "pre-output validation sequence missing")
    validate_pos = runner.index("validateAndConsumeAuthorization(receipt, plan")
    output_pos = runner.index("std::filesystem::create_directories(output);")
    cocoa_pos = runner.index("[NSApplication sharedApplication]")
    require(validate_pos < output_pos < cocoa_pos,
            "authorization contract must remain before output/Cocoa/OpenGL")
    require(dispatcher.count("phase5_rules_decision_ui_v5_exact16_readiness_v2") >= 2 and
            "V5_PIXEL_VISIBLE_EXACT16_CORRECTED_PRODUCER_NOT_RUN_NO_AUTH" in dispatcher and
            "BilliardsPhase5RulesDecisionUiV5Exact16GpuCaptureRunner" in dispatcher and
            "os.O_EXCL" in dispatcher and "os.fsync" in dispatcher,
            "dispatcher v5/v2 identity or durable single-use contract missing")


def validate_source_contract() -> dict:
    validate_contract_text((ROOT / "CMakeLists.txt").read_text(),
        prep.RUNNER_SOURCE.read_text(), prep.DISPATCHER.read_text())
    require(sha(prep.GATE) == prep.GATE_SHA and sha(prep.FAILURE) == prep.FAILURE_SHA,
            "correction gate/failure lineage drift")
    frozen_product = {
        "src/Billiards/render/ui/rules_decision_product_draw.cpp":
            "fd33c09048dc5eb4d88f781df4ed33ab00bc340746f8a8140facc18feccc57cd",
        "src/Billiards/render/ui/rules_decision_ui_draw_list.cpp":
            "4c383c1d842ec338a2d0a2dc482e235448269b3a91df43e753824273c6015a21",
        "tests/phase5_rules_decision_ui_v5_cpu_raster_tests.cpp":
            "4f1ca4ac0bb818da1babf944fa43b8e9583fc125fb5b308ee2e11aa3ce852df6",
    }
    for relative, digest in frozen_product.items():
        require(sha(ROOT / relative) == digest,
                "forbidden product/visual source drift: " + relative)
    return {"status": "PASS_PHASE5_V5_CORRECTED_PRODUCER_SOURCE_CONTRACT",
            "target": prep.TARGET, "gpu_executed": False}


def validate(root: Path) -> dict:
    validate_source_contract()
    root = root.resolve(strict=True)
    files = {name: root / filename for name, filename in {
        "ready": "rules-decision-ui-v5-exact16-ready-v2.json",
        "auth": "rules-decision-ui-v5-authorization-template-not-issued-v2.json",
        "sources": "rules-decision-ui-v5-source-bindings-v2.json",
        "receipt": "offline-readiness-receipt-v2.json",
        "plan": "rules-decision-ui-v5-exact16-plan.tsv",
        "machine": "rules-decision-ui-v5-machine-540.json",
        "cpu": "rules-decision-ui-v5-exact16-cpu-preflight.json",
        "raster": "rules-decision-ui-v5-final-cpu-raster-manifest.json",
    }.items()}
    require(all(path.is_file() for path in files.values()), "v2 artifact set incomplete")
    require(not list(root.rglob("*.ppm")) and not list(root.rglob("*.png")) and
            not list(root.rglob("*.consumed*")), "v2 contains capture/consumption evidence")
    ready = json.loads(files["ready"].read_text())
    auth = json.loads(files["auth"].read_text())
    sources = json.loads(files["sources"].read_text())
    receipt = json.loads(files["receipt"].read_text())
    ids = base.case_ids()
    claims = {"gpu_authorized": False, "gpu_executed": False,
              "technical_art_review": False, "product_exposed": False,
              "golden_promotion": False, "m2_promotion": False}
    require(ready.get("schema") ==
            "phase5_rules_decision_ui_v5_exact16_readiness_v2" and
            ready.get("status") ==
            "V5_PIXEL_VISIBLE_EXACT16_CORRECTED_PRODUCER_NOT_RUN_NO_AUTH" and
            ready.get("case_count") == 16 and ready.get("case_ids_in_order") == ids and
            ready.get("case_results") == [{"ordinal": index + 1,
                "capture_id": item, "status": "NOT_RUN"}
                for index, item in enumerate(ids)] and ready.get("claims") == claims,
            "v2 identity/scope/claims mismatch")
    require(ready.get("capture_producer") == {"target": prep.TARGET,
            "compile_definitions": ["BILLIARDGL_PHASE5_RULES_UI_V4_EXACT16=1",
                                    "BILLIARDGL_PHASE5_RULES_UI_V5_EXACT16=1"],
            "mode_exclusive": True}, "corrected producer identity mismatch")
    require(ready.get("correction_lineage") == {
        "old_authorization_reuse_forbidden": True,
        "old_failure_receipt_sha256": prep.FAILURE_SHA,
        "contract_only_correction": True,
        "visual_product_authority_rules_unchanged": True},
        "v2 correction lineage mismatch")
    frozen = ready.get("frozen_bindings", {})
    mapping = {"gate_sha256": "gate",
        "preordinal_failure_receipt_sha256": "failure_receipt",
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
        "runner_binary_sha256": "runner_binary", "cmake_sha256": "cmake"}
    for field, key in mapping.items():
        live = Path(ready["paths"][key])
        require(live.is_file() and sha(live) == frozen.get(field),
                "v2 live binding drift: " + field)
    binary = Path(ready["paths"]["runner_binary"]).read_bytes()
    require(prep.v1.GATE_SHA.encode() in binary and
            b"phase5_rules_decision_ui_v5_exact16_readiness_v2" in binary and
            prep.GATE_SHA.encode() not in binary,
            "corrected runner embedded contract missing")
    require(sources.get("capture_target") == prep.TARGET and
            sources.get("producer_correction_gate_sha256") == prep.GATE_SHA and
            sources.get("preordinal_failure_receipt_sha256") == prep.FAILURE_SHA and
            base.aggregate(sources["files"]) == sources.get("aggregate_sha256") ==
            frozen.get("source_aggregate_sha256"), "v2 source manifest mismatch")
    for row in sources["files"]:
        require(sha(ROOT / row["path"]) == row["sha256"],
                "v2 source drift: " + row["path"])
    require(auth.get("status") == "NOT_ISSUED" and auth.get("authorized") is False and
            auth.get("consumed") is False and auth.get("case_count") == 16 and
            auth.get("scene_ids_in_order") == ids and
            auth.get("ready_manifest_sha256") == sha(files["ready"]) and
            auth.get("runner_binary_sha256") == frozen["runner_binary_sha256"] and
            all(auth.get(field) is False for field in claims),
            "v2 auth template promoted or drifted")
    require(receipt.get("status") == "CORRECTED_PRODUCER_EXACT16_NOT_RUN_NO_AUTH" and
            receipt.get("readiness_sha256") == sha(files["ready"]) and
            receipt.get("authorization_template_sha256") == sha(files["auth"]) and
            receipt.get("claims") == claims, "v2 receipt mismatch")
    dispatch.validate_ready(files["ready"])
    return {"status": "PASS_PHASE5_V5_CORRECTED_EXACT16_READY_NO_GPU_NO_AUTH",
            "readiness_sha256": sha(files["ready"]), "case_count": 16,
            "authorization_issued": False, "gpu_executed": False}


if __name__ == "__main__":
    parser = argparse.ArgumentParser()
    parser.add_argument("root", type=Path, nargs="?")
    parser.add_argument("--source-only", action="store_true")
    args = parser.parse_args()
    try:
        result = validate_source_contract() if args.source_only else validate(
            args.root or prep.DEFAULT_OUTPUT)
        print(json.dumps(result, sort_keys=True))
    except (OSError, ValueError, json.JSONDecodeError, RuntimeError) as error:
        print("FAIL: " + str(error))
        raise SystemExit(2)
