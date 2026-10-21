#!/usr/bin/env python3
"""Validate the Phase5 v5 exact16 producer contract correction gate."""

from __future__ import annotations

import argparse
import hashlib
import json
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
DEFAULT = ROOT / "docs/art/phase5-rules-decision-ui-v5-exact16-producer-contract-correction-gate-v1.json"
ALLOWED = {
    "CMakeLists.txt",
    "spikes/render_core_profile/phase5_rules_decision_ui_gpu_capture_runner.mm",
    "scripts/run_phase5_rules_decision_ui_v4_exact16_capture_dispatch.py",
    "scripts/prepare_phase5_rules_decision_ui_v5_pixel_visible_exact16_ready_v2.py",
    "scripts/validate_phase5_rules_decision_ui_v5_pixel_visible_exact16_ready_v2.py",
    "tests/visual/test_phase5_rules_decision_ui_v5_pixel_visible_exact16_ready_v2.py",
    "scripts/validate_phase5_rules_decision_ui_v5_exact16_producer_contract_correction_gate.py",
    "tests/visual/test_phase5_rules_decision_ui_v5_exact16_producer_contract_correction_gate.py",
}


def require(value: bool, message: str) -> None:
    if not value:
        raise RuntimeError(message)


def sha(path: Path) -> str:
    return hashlib.sha256(path.read_bytes()).hexdigest()


def validate(path: Path = DEFAULT) -> dict:
    value = json.loads(path.read_text(encoding="utf-8"))
    require(value.get("schema") ==
            "phase5_rules_decision_ui_v5_exact16_producer_contract_correction_gate_v1" and
            value.get("status") ==
            "APPROVED_MINIMAL_CAPTURE_ONLY_CONTRACT_CORRECTION_NO_GPU_NO_AUTH",
            "gate identity/status")
    receipt = value.get("failure_receipt", {})
    receipt_path = ROOT / receipt.get("path", "")
    require(receipt_path.is_file() and sha(receipt_path) == receipt.get("sha256") and
            receipt.get("result") ==
            "FROZEN_PREORDINAL_PRODUCER_CONTRACT_FAILURE_NO_GPU_NO_REUSE",
            "frozen failure receipt binding")
    diagnosis = value.get("diagnosis", {})
    require(diagnosis == {
        "first_mismatch": "runner_v4_gate_vs_v5_source_manifest_gate",
        "latent_next_mismatch": "runner_v4_readiness_schema_vs_v5_readiness_schema",
        "correction_layer": "capture_only_runner_and_build_target",
        "production_visual_bytes_need_change": False,
        "authority_rules_physics_need_change": False,
        "dispatcher_single_use_protocol_need_change": False},
        "exact root cause/correction layer")
    bindings = value.get("pre_correction_bindings", {})
    require(set(bindings) == {
        "CMakeLists.txt",
        "spikes/render_core_profile/phase5_rules_decision_ui_gpu_capture_runner.mm",
        "scripts/run_phase5_rules_decision_ui_v4_exact16_capture_dispatch.py"},
        "pre-correction binding inventory")
    require(bindings == {
        "CMakeLists.txt":
            "917291bff5a23f6986d496fd677ad65f0f6921eebad2a34c6db8acc880ae2f93",
        "spikes/render_core_profile/phase5_rules_decision_ui_gpu_capture_runner.mm":
            "9f72fb8c412bab48dbce9cfea2f5de7eaad1ab7538ebdc87b796072c07a61ae4",
        "scripts/run_phase5_rules_decision_ui_v4_exact16_capture_dispatch.py":
            "6891c18e7df9f9a6b1dd41d4838fae2b3b43119c8fec8fd8b010bd38102fbc59"},
        "pre-correction binding values")
    allowed = value.get("allowed_source_changes", [])
    require({row.get("path") for row in allowed} == ALLOWED and
            all(isinstance(row.get("scope"), str) and row["scope"] for row in allowed),
            "allowed source scope")
    contract = value.get("required_corrected_contract", {})
    require(contract == {
        "new_target": "BilliardsPhase5RulesDecisionUiV5Exact16GpuCaptureRunner",
        "base_exact16_definition": "BILLIARDGL_PHASE5_RULES_UI_V4_EXACT16=1",
        "v5_contract_definition": "BILLIARDGL_PHASE5_RULES_UI_V5_EXACT16=1",
        "authorization_schema_unchanged":
            "phase5_rules_decision_ui_v4_exact16_gpu_authorization_v2",
        "manifest_gate_sha256":
            "657f3ab6f4ccac4db23fb0ab72889bc49d091f27fd5f63bd5610b736b1aa2a10",
        "readiness_schema": "phase5_rules_decision_ui_v5_exact16_readiness_v2",
        "case_count": 16, "case_ids_and_order_unchanged": True,
        "renderer_and_visual_source_unchanged": True,
        "single_use_dispatcher_unchanged": True,
        "dispatcher_protocol_unchanged_except_v5_v2_readiness_identity": True,
        "validate_before_output_cocoa_opengl": True},
        "corrected contract exactness")
    forbidden = value.get("forbidden_changes", [])
    require("src/Billiards/render/modern/modern_renderer.cpp" in forbidden and
            "any Phase4 source or readiness" in forbidden and
            "authorization issuance or GPU execution" in forbidden,
            "forbidden scope incomplete")
    acceptance = value.get("acceptance", {})
    require(acceptance.get("old_failure_artifacts_remain_byte_and_permission_exact") is True and
            acceptance.get("static_binary_strings_bind_v5_gate_and_v5_readiness_schema") is True and
            acceptance.get("corrected_runner_rejects_v4_gate_or_v4_readiness_offline") is True and
            acceptance.get("fresh_release_build") is True and
            acceptance.get("fresh_v2_readiness_16_not_run") is True and
            acceptance.get("authorization_template_not_issued") is True and
            acceptance.get("gpu_executed") is False,
            "acceptance criteria")
    return {"status": "PASS_PHASE5_V5_EXACT16_PRODUCER_CONTRACT_CORRECTION_GATE",
            "allowed_file_count": len(ALLOWED), "gpu_authorized": False,
            "gpu_executed": False}


if __name__ == "__main__":
    parser = argparse.ArgumentParser()
    parser.add_argument("gate", type=Path, nargs="?", default=DEFAULT)
    try:
        print(json.dumps(validate(parser.parse_args().gate), sort_keys=True))
    except (OSError, ValueError, json.JSONDecodeError, RuntimeError) as error:
        print("FAIL: " + str(error))
        raise SystemExit(2)
