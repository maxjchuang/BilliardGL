#!/usr/bin/env python3
"""Validate manifest/runner contract correction gate."""

from __future__ import annotations

import argparse
import hashlib
import json
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
DEFAULT = ROOT / "docs/art/phase5-rules-decision-ui-v5v2-exact16-manifest-runner-contract-correction-gate-v1.json"
ALLOWED = {
    "scripts/phase5_rules_decision_ui_v5_exact16_runner_json_contract.py",
    "tests/visual/test_phase5_rules_decision_ui_v5_exact16_runner_json_contract.py",
    "scripts/prepare_phase5_rules_decision_ui_v5_pixel_visible_exact16_ready_v3.py",
    "scripts/validate_phase5_rules_decision_ui_v5_pixel_visible_exact16_ready_v3.py",
    "tests/visual/test_phase5_rules_decision_ui_v5_pixel_visible_exact16_ready_v3.py",
    "scripts/validate_phase5_rules_decision_ui_v5v2_exact16_manifest_runner_contract_correction_gate.py",
    "tests/visual/test_phase5_rules_decision_ui_v5v2_exact16_manifest_runner_contract_correction_gate.py",
}


def require(value: bool, message: str) -> None:
    if not value:
        raise RuntimeError(message)


def sha(path: Path) -> str:
    return hashlib.sha256(path.read_bytes()).hexdigest()


def validate(path: Path = DEFAULT) -> dict:
    value = json.loads(path.read_text())
    require(value.get("schema") ==
            "phase5_rules_decision_ui_v5v2_exact16_manifest_runner_contract_correction_gate_v1" and
            value.get("status") ==
            "APPROVED_MANIFEST_ONLY_FIELD_PARITY_CORRECTION_NO_GPU_NO_AUTH",
            "gate identity/status")
    receipt = value["failure_receipt"]
    require(sha(ROOT / receipt["path"]) == receipt["sha256"] and
            receipt["missing_field"] == "gate_sha256", "failure binding")
    diagnosis = value["diagnosis"]
    require(diagnosis == {
        "manifest_schema": "phase5_rules_decision_ui_v5_exact16_source_bindings_v2",
        "runner_required_missing_field": "gate_sha256",
        "existing_semantic_value_field": "visual_design_gate_sha256",
        "required_value":
            "657f3ab6f4ccac4db23fb0ab72889bc49d091f27fd5f63bd5610b736b1aa2a10",
        "runner_source_change_required": False, "cmake_change_required": False,
        "dispatcher_protocol_change_required": False,
        "product_visual_or_rules_change_required": False}, "diagnosis exactness")
    expected_bindings = {
        "CMakeLists.txt": "f41563c35224dd809f64acedba6455355e70082697c41305c2d1b05426f71d41",
        "spikes/render_core_profile/phase5_rules_decision_ui_gpu_capture_runner.mm":
            "99a9624c5e95229ac42619bfb1824559224fa259a6b166bb26bd95682c9a1086",
        "scripts/run_phase5_rules_decision_ui_v4_exact16_capture_dispatch.py":
            "2156eac151f845e1b264f93a89464c92634d72fbe27d63e6624d6eb719f7c5d2",
        "scripts/prepare_phase5_rules_decision_ui_v5_pixel_visible_exact16_ready_v2.py":
            "1713a93fb00ae376e8048dcc980ec8a0c7efc0c57691cbe81aea8ecc3d391b67",
        "scripts/validate_phase5_rules_decision_ui_v5_pixel_visible_exact16_ready_v2.py":
            "ed573a3c5e0faa228da8f37c25ec8b44eaa6764e7635a620d7ae4b032c7dc4aa"}
    require(value["frozen_implementation_bindings"] == expected_bindings and
            all(sha(ROOT / relative) == digest
                for relative, digest in expected_bindings.items()),
            "frozen implementation drift")
    require(set(value["allowed_new_files"]) == ALLOWED,
            "allowed file inventory")
    correction = value["required_correction"]
    require(correction.get("source_manifest_add_top_level_field") == "gate_sha256" and
            correction.get("source_manifest_gate_sha256") == diagnosis["required_value"] and
            correction.get("retain_visual_design_gate_sha256") is True and
            correction.get("retain_producer_correction_lineage") is True and
            correction.get("readiness_schema_for_unchanged_runner") ==
            "phase5_rules_decision_ui_v5_exact16_readiness_v2" and
            correction.get("authorization_schema") ==
            "phase5_rules_decision_ui_v4_exact16_gpu_authorization_v2" and
            correction.get("case_count") == 16 and
            correction.get("all_case_rows_not_run") is True,
            "required correction")
    parity = value["parity_test_requirements"]
    require(all(parity.values()), "all parity requirements must be true")
    forbidden = value["forbidden_changes"]
    require("spikes/render_core_profile/phase5_rules_decision_ui_gpu_capture_runner.mm" in forbidden and
            "CMakeLists.txt" in forbidden and
            "scripts/run_phase5_rules_decision_ui_v4_exact16_capture_dispatch.py" in forbidden and
            "authorization issuance or GPU retry" in forbidden,
            "forbidden changes")
    acceptance = value["acceptance"]
    require(acceptance.get("old_failure_artifacts_byte_and_permission_exact") is True and
            acceptance.get("faithful_full_contract_positive_pass") is True and
            acceptance.get("all_required_field_negative_mutations_rejected") is True and
            acceptance.get("fresh_successor_package_not_run") is True and
            acceptance.get("authorization_template_not_issued") is True and
            acceptance.get("final_build_and_readiness_wait_for_phase4") is True and
            acceptance.get("gpu_executed") is False, "acceptance")
    return {"status": "PASS_PHASE5_V5V2_MANIFEST_RUNNER_CONTRACT_CORRECTION_GATE",
            "allowed_file_count": len(ALLOWED), "gpu_executed": False}


if __name__ == "__main__":
    parser = argparse.ArgumentParser()
    parser.add_argument("gate", type=Path, nargs="?", default=DEFAULT)
    try:
        print(json.dumps(validate(parser.parse_args().gate), sort_keys=True))
    except (OSError, ValueError, json.JSONDecodeError, RuntimeError) as error:
        print("FAIL: " + str(error))
        raise SystemExit(2)
