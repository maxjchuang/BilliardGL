#!/usr/bin/env python3
"""Validate manifest-field-corrected Phase5 v5 exact16 successor package."""

from __future__ import annotations

import argparse
import copy
import json
from pathlib import Path
import sys

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT))
from scripts import phase5_rules_decision_ui_v5_exact16_runner_json_contract as contract
from scripts import prepare_phase5_rules_decision_ui_v4_exact16_ready as base
from scripts import prepare_phase5_rules_decision_ui_v5_pixel_visible_exact16_ready_v3 as prep
from scripts import run_phase5_rules_decision_ui_v4_exact16_capture_dispatch as dispatch


def require(value: bool, message: str) -> None:
    if not value:
        raise RuntimeError(message)


def validate_source_contract() -> dict:
    contract.validate_runner_source_access_inventory()
    require(base.sha(prep.GATE) == prep.GATE_SHA and
            base.sha(prep.FAILURE) == prep.FAILURE_SHA,
            "manifest correction lineage drift")
    for relative, digest in {
        "CMakeLists.txt":
            "f41563c35224dd809f64acedba6455355e70082697c41305c2d1b05426f71d41",
        "spikes/render_core_profile/phase5_rules_decision_ui_gpu_capture_runner.mm":
            "99a9624c5e95229ac42619bfb1824559224fa259a6b166bb26bd95682c9a1086",
        "scripts/run_phase5_rules_decision_ui_v4_exact16_capture_dispatch.py":
            "2156eac151f845e1b264f93a89464c92634d72fbe27d63e6624d6eb719f7c5d2",
        "src/Billiards/render/ui/rules_decision_product_draw.cpp":
            "fd33c09048dc5eb4d88f781df4ed33ab00bc340746f8a8140facc18feccc57cd",
        "src/Billiards/render/ui/rules_decision_ui_draw_list.cpp":
            "4c383c1d842ec338a2d0a2dc482e235448269b3a91df43e753824273c6015a21",
    }.items():
        require(base.sha(ROOT / relative) == digest,
                "forbidden source drift: " + relative)
    return {"status": "PASS_PHASE5_V5_MANIFEST_CORRECTION_SOURCE_CONTRACT",
            "gpu_executed": False}


def in_memory_signed_contract(auth_template: dict, manifest: dict,
                              readiness: dict) -> dict:
    auth = copy.deepcopy(auth_template)
    output = "/no-gpu/offline-contract-only-output"
    auth.update({"status": "AUTHORIZED_FOR_EXACTLY_16_SINGLE_USE_CAPTURES",
        "authorized": True, "authorization_id": "offline-contract-id",
        "authorization_nonce": "offline-contract-nonce",
        "approved_by": "offline-contract-validator",
        "approved_at": "2099-01-01T00:00:00Z",
        "expires_unix_seconds": 4070908800,
        "authorized_output_canonical_path": output, "gpu_authorized": True})
    marker = {"authorization_id": auth["authorization_id"],
        "authorization_nonce": auth["authorization_nonce"],
        "ready_manifest_sha256": auth["ready_manifest_sha256"],
        "output_root": output,
        "runner_binary_sha256": auth["runner_binary_sha256"],
        "consumed_before_output_or_process": True}
    artifacts = {"plan": auth["plan_sha256"],
        "billiards": auth["billiards_binary_sha256"],
        "font": auth["font_sha256"],
        "manifest": auth["source_binding_manifest_sha256"],
        "runner": auth["runner_binary_sha256"],
        "runner_source": auth["runner_source_sha256"],
        "readiness": auth["ready_manifest_sha256"],
        "dispatcher": auth["dispatcher_sha256"]}
    return contract.validate_contract(auth, manifest, readiness, marker,
        artifacts, {"output": output, "runner": auth["runner_binary_path"]},
        now_unix=4070908799)


def validate(root: Path = prep.DEFAULT_OUTPUT) -> dict:
    validate_source_contract()
    root = root.resolve(strict=True)
    paths = {name: root / filename for name, filename in {
        "ready": "rules-decision-ui-v5-exact16-ready-v3.json",
        "auth": "rules-decision-ui-v5-authorization-template-not-issued-v3.json",
        "sources": "rules-decision-ui-v5-source-bindings-v3.json",
        "receipt": "offline-readiness-receipt-v3.json",
        "plan": "rules-decision-ui-v5-exact16-plan.tsv",
        "machine": "rules-decision-ui-v5-machine-540.json",
        "cpu": "rules-decision-ui-v5-exact16-cpu-preflight.json",
        "raster": "rules-decision-ui-v5-final-cpu-raster-manifest.json",
    }.items()}
    require(all(path.is_file() for path in paths.values()), "v3 artifact set")
    require(not list(root.rglob("*.ppm")) and not list(root.rglob("*.png")) and
            not list(root.rglob("*.consumed*")), "v3 capture/consumption evidence")
    ready = json.loads(paths["ready"].read_text())
    auth = json.loads(paths["auth"].read_text())
    sources = json.loads(paths["sources"].read_text())
    receipt = json.loads(paths["receipt"].read_text())
    require(ready.get("schema") == contract.READY_SCHEMA and
            ready.get("package_revision") ==
            "v3_manifest_runner_field_parity_corrected" and
            ready.get("case_count") == 16 and
            ready.get("case_ids_in_order") == contract.IDS and
            all(row.get("status") == "NOT_RUN" for row in ready["case_results"]) and
            not any(ready["claims"].values()), "v3 scope/claims")
    correction = ready.get("manifest_contract_correction", {})
    require(correction == {"top_level_gate_sha256_present": True,
        "gate_sha256": contract.GATE_SHA,
        "full_runner_json_contract_parity_tested": True,
        "runner_source_and_binary_unchanged": True,
        "old_v2_authorization_reuse_forbidden": True}, "v3 correction declaration")
    frozen = ready["frozen_bindings"]
    mapping = {"plan_sha256": "plan", "machine_report_sha256": "machine_report",
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
        "manifest_contract_correction_gate_sha256": "manifest_contract_correction_gate",
        "missing_gate_field_failure_receipt_sha256":
            "missing_gate_field_failure_receipt"}
    for field, key in mapping.items():
        live = Path(ready["paths"][key])
        require(live.is_file() and base.sha(live) == frozen[field],
                "v3 live binding: " + field)
    require(sources.get("schema") ==
            "phase5_rules_decision_ui_v5_exact16_source_bindings_v3" and
            sources.get("gate_sha256") == contract.GATE_SHA and
            sources.get("visual_design_gate_sha256") == contract.GATE_SHA and
            sources.get("manifest_contract_correction_gate_sha256") == prep.GATE_SHA and
            sources.get("missing_gate_field_failure_receipt_sha256") == prep.FAILURE_SHA and
            base.aggregate(sources["files"]) == sources.get("aggregate_sha256") ==
            frozen["source_aggregate_sha256"], "v3 source manifest")
    for row in sources["files"]:
        require(base.sha(ROOT / row["path"]) == row["sha256"],
                "v3 source drift: " + row["path"])
    require(auth.get("status") == "NOT_ISSUED" and auth.get("authorized") is False and
            auth.get("consumed") is False and
            auth.get("ready_manifest_sha256") == base.sha(paths["ready"]) and
            auth.get("source_binding_manifest_sha256") == base.sha(paths["sources"]) and
            not any(auth.get(field) for field in (
                "gpu_executed", "technical_art_review", "product_exposed",
                "golden_promotion", "m2_promotion")), "v3 auth template")
    parity = in_memory_signed_contract(auth, sources, ready)
    require(parity["case_count"] == 16 and parity["gpu_executed"] is False,
            "v3 full contract parity")
    require(receipt.get("status") ==
            "MANIFEST_FIELD_PARITY_CORRECTED_EXACT16_NOT_RUN_NO_AUTH" and
            receipt.get("readiness_sha256") == base.sha(paths["ready"]) and
            receipt.get("authorization_template_sha256") == base.sha(paths["auth"]) and
            receipt.get("source_binding_manifest_sha256") == base.sha(paths["sources"]) and
            not any(receipt["claims"].values()), "v3 receipt")
    dispatch.validate_ready(paths["ready"])
    return {"status": "PASS_PHASE5_V5_MANIFEST_CORRECTED_READY_NO_GPU_NO_AUTH",
            "readiness_sha256": base.sha(paths["ready"]), "case_count": 16,
            "full_runner_json_contract_parity": True,
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
