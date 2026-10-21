#!/usr/bin/env python3
"""Validate the NO-GPU Phase5 v4 exact16 capture-producer correction gate."""

from __future__ import annotations

import argparse
import hashlib
import json
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
GATE = ROOT / "docs/art/phase5-rules-decision-ui-v4-exact16-capture-producer-contract-correction-gate-v1.json"
GATE_SHA256 = "dbab785638be91550e25380141a62819c28ea72da9082eb23c02c5e4869866ad"

EXACT16 = [
    "group_choice_armed_confirm--p1--zh_cn",
    "group_choice_armed_confirm--p2--en",
    "group_choice_armed_confirm--p3--zh_cn",
    "group_choice_armed_confirm--p4--en",
    "group_choice_open_unselected--p1--en",
    "group_choice_open_unselected--p2--zh_cn",
    "group_choice_open_unselected--p3--en",
    "group_choice_open_unselected--p4--zh_cn",
    "eight_legal_open_unselected--p1--en",
    "break_illegal_open_unselected--p2--zh_cn",
    "submitting_progress--p3--en",
    "authority_error--p4--en",
    "full_table_preview_valid--p1--en",
    "full_table_preview_invalid--p2--en",
    "behind_head_string_preview_valid--p3--en",
    "behind_head_string_preview_invalid--p4--en",
]

EXPECTED_BINDINGS = {
    "v4_exact_shape_visual_authority": (
        "docs/art/phase5-rules-decision-ui-v4-exact-shape-correction-design-gate-v1.json",
        "f93ca59634875ed5256f5c22678a55817e0f3e8bf8df582b3b04ac527b886051"),
    "dispatcher_ready_but_producer_blocked_manifest": (
        "docs/evidence/phase5/rules-decision-ui-v4-exact16-ready-v2/rules-decision-ui-v4-exact16-ready.json",
        "796e895b0917027b3774dde1f223adda067716ad4c4589607f60c354c299741a"),
    "dispatcher_ready_but_producer_blocked_receipt": (
        "docs/evidence/phase5/rules-decision-ui-v4-exact16-ready-v2/offline-readiness-receipt.json",
        "9902fdf790a08a097aa7ae5d00455f15bdfc1b4704c21e097a955cf0b6afd23e"),
    "non_executable_authorization_template": (
        "docs/evidence/phase5/rules-decision-ui-v4-exact16-ready-v2/rules-decision-ui-v4-authorization-template-not-issued.json",
        "12a130bd994a1b4b4924294ebff8b8371f4f99833d4a5d9d9367bd8bde2fcdb6"),
    "exact16_ordered_plan": (
        "docs/evidence/phase5/rules-decision-ui-v4-exact16-ready-v2/rules-decision-ui-v4-exact16-plan.tsv",
        "426de40ba87b8cfdb5eefc88464720507e53ced2190ffb61564e7cd848eb593f"),
    "v2_source_binding_manifest": (
        "docs/evidence/phase5/rules-decision-ui-v4-exact16-ready-v2/rules-decision-ui-v4-source-bindings.json",
        "533f82cf339df809023e98e0836a982946d91b700320872580c9eed29fb012a7"),
    "single_use_dispatcher_source": (
        "scripts/run_phase5_rules_decision_ui_v4_exact16_capture_dispatch.py",
        "428668d38ece54d71cd23617df1b50477716c983d267f6ffad6d33fd297fcdd5"),
    "single_use_dispatch_validator_source": (
        "scripts/validate_phase5_rules_decision_ui_v4_exact16_capture_dispatch.py",
        "1b5c96f758c0497fd501b9281b8ec3575a5a0a4d8b0807d5ed7d809057987cba"),
    "incompatible_capture_runner_source_baseline": (
        "spikes/render_core_profile/phase5_rules_decision_ui_gpu_capture_runner.mm",
        "a5a2764fe526cd4670b90f5dce5e44c1a0fda1610d5d4a9a3e3a3292509bec3a"),
    "capture_target_build_wiring_baseline": (
        "CMakeLists.txt",
        "acf6abcb7f6ae9bc177f872a4ec11d052f5fa81757d9b58f86c406d52a628835"),
}

FALSE_BOUNDARY = {
    "production_modified_by_this_gate", "capture_runner_modified_by_this_gate",
    "capture_producer_compiled", "successor_readiness_created",
    "authorization_prepared", "authorization_issued", "authorization_consumed",
    "gpu_executed", "capture_performed", "machine_capture_pass",
    "technical_art_review_pass", "real_input_pass", "screen_reader_pass",
    "product_exposed", "product_pass", "golden_promotion", "m2_promotion",
    "beta_pass",
}


def sha(path: Path) -> str:
    return hashlib.sha256(path.read_bytes()).hexdigest()


def require(condition: bool, message: str) -> None:
    if not condition:
        raise RuntimeError(message)


def load(path: Path) -> dict:
    value = json.loads(path.read_text(encoding="utf-8"))
    require(isinstance(value, dict), "gate must be a JSON object")
    return value


def validate(path: Path = GATE, enforce_frozen_gate_sha: bool = True,
             enforce_live_baselines: bool = True) -> dict:
    path = path.resolve(strict=True)
    if enforce_frozen_gate_sha:
        require(path == GATE.resolve() and sha(path) == GATE_SHA256,
                "frozen correction gate SHA mismatch")
    gate = load(path)
    require(gate.get("schema") ==
            "phase5_rules_decision_ui_v4_exact16_capture_producer_contract_correction_gate_v1" and
            gate.get("schema_version") == 1 and gate.get("status") ==
            "APPROVED_FOR_CAPTURE_ONLY_PRODUCER_CONTRACT_IMPLEMENTATION_NO_GPU_NO_AUTH",
            "gate identity/status mismatch")

    bindings = gate.get("baseline_bindings")
    require(isinstance(bindings, list) and len(bindings) == 10,
            "baseline binding inventory mismatch")
    actual = {row.get("role"): (row.get("path"), row.get("sha256"))
              for row in bindings if isinstance(row, dict)}
    require(actual == EXPECTED_BINDINGS, "baseline path/SHA binding mismatch")
    if enforce_live_baselines:
        for role, (relative, digest) in EXPECTED_BINDINGS.items():
            live = ROOT / relative
            require(live.is_file() and sha(live) == digest,
                    "live baseline drift: " + role)

    policy = gate.get("baseline_binding_policy", {})
    immutable = {
        "v4_exact_shape_visual_authority",
        "dispatcher_ready_but_producer_blocked_manifest",
        "dispatcher_ready_but_producer_blocked_receipt",
        "non_executable_authorization_template", "exact16_ordered_plan",
        "v2_source_binding_manifest",
    }
    mutable = set(EXPECTED_BINDINGS) - immutable
    require(set(policy.get("immutable_evidence_roles", [])) == immutable and
            set(policy.get("implementation_baseline_roles", [])) == mutable,
            "immutable/mutable baseline classification mismatch")

    require(gate.get("exact16_case_ids_in_order") == EXACT16 and
            len(set(EXACT16)) == 16, "exact16 case inventory/order mismatch")
    problem = gate.get("frozen_problem_statement", {})
    require(problem.get("current_capture_producer_modes") == [
        {"compile_mode": "default", "required_case_count": 60},
        {"compile_mode": "BILLIARDGL_PHASE5_RULES_UI_V3_TARGETED",
         "required_case_count": 28}] and
        problem.get("missing_mode") == "BILLIARDGL_PHASE5_RULES_UI_V4_EXACT16" and
        problem.get("correction_class") == "capture_producer_contract_only",
        "frozen producer gap mismatch")

    allowed = gate.get("allowed_modification_surface", [])
    require([item.get("category") for item in allowed] == [
        "capture_target_compile_time_wiring",
        "capture_runner_compile_time_contract",
        "offline_readiness_and_dispatch_binding",
        "offline_fail_closed_tests"], "allowed modification surface mismatch")
    require(allowed[0].get("allowed_path") == "CMakeLists.txt" and
            allowed[1].get("allowed_path") ==
            "spikes/render_core_profile/phase5_rules_decision_ui_gpu_capture_runner.mm",
            "capture-only source paths mismatch")

    producer = gate.get("capture_producer_contract", {})
    plan = producer.get("plan_contract", {})
    auth = producer.get("authorization_contract", {})
    output = producer.get("output_contract", {})
    require(producer.get("compile_definition") ==
            "BILLIARDGL_PHASE5_RULES_UI_V4_EXACT16=1" and
            producer.get("required_distinct_binary_target") ==
            "BilliardsPhase5RulesDecisionUiV4Exact16GpuCaptureRunner" and
            plan.get("required_case_count") == 16 and
            plan.get("required_field_count_per_row") == 12 and
            plan.get("required_ids_and_order") == "exact16_case_ids_in_order" and
            all(plan.get(field) is True for field in (
                "duplicate_ids_forbidden", "empty_or_extra_rows_forbidden",
                "plan_sha256_must_equal_authorization_and_readiness")),
            "compile/loadPlan contract mismatch")
    required_auth_bindings = {
        "ready_manifest_sha256", "dispatcher_sha256", "runner_source_sha256",
        "runner_binary_sha256", "plan_sha256", "source_binding_manifest_sha256",
        "source_aggregate_sha256", "billiards_binary_sha256", "font_sha256",
        "hud_token_sha256", "shell_token_sha256",
        "authorized_output_canonical_path", "scene_ids_in_order",
    }
    require(auth.get("required_schema") ==
            "phase5_rules_decision_ui_v4_exact16_gpu_authorization_v2" and
            auth.get("required_case_count") == 16 and
            auth.get("required_flag") == {"exact16_capture_producer_contract": True} and
            set(auth.get("required_bindings", [])) == required_auth_bindings and
            auth.get("identity_and_expiry_required") is True and
            auth.get("single_use_required") is True and
            auth.get("pre_process_rejection_required") is True,
            "exact16 authorization contract mismatch")
    require(output == {"results_rows": 16, "telemetry_rows": 16,
                       "ppm_files": 16, "capture_order_must_equal_plan": True,
                       "partial_or_extra_output_is_failure": True,
                       "failure_bytes_and_telemetry_must_be_preserved": True},
            "exact16 output contract mismatch")

    dispatcher = gate.get("single_use_dispatcher_binding_gate", {})
    requirements = dispatcher.get("required_before_authorization_may_be_signed", [])
    require(dispatcher.get("status") ==
            "CURRENT_DISPATCHER_IMPLEMENTED_PRODUCER_BINDING_PENDING" and
            len(requirements) == 6 and
            dispatcher.get("authorization_may_be_signed_now") is False and
            dispatcher.get("gpu_execution_authorized") is False,
            "dispatcher/authorization gate mismatch")
    tests = gate.get("required_offline_tests", [])
    require(len(tests) == 8 and any("15, 17" in item for item in tests) and
            any("v3 exact28 and default60" in item for item in tests),
            "offline fail-closed test contract mismatch")

    forbidden = gate.get("forbidden_changes", {})
    require(set(forbidden) == {"production_and_visual", "authority_and_rules",
                              "thresholds", "authorization_and_evidence"} and
            all(forbidden[key] for key in forbidden),
            "forbidden-change boundary mismatch")
    boundary = gate.get("non_execution_boundary", {})
    require(set(boundary) == FALSE_BOUNDARY and
            all(boundary[field] is False for field in FALSE_BOUNDARY),
            "non-execution boundary escalation")
    acceptance = gate.get("acceptance", {})
    require(acceptance.get("implementation_gate_result") ==
            "PASS_CAPTURE_PRODUCER_CONTRACT_IMPLEMENTATION_ONLY" and
            "separate root" in acceptance.get("authorization_signing_precondition", "") and
            all(term in acceptance.get("proof_boundary", "") for term in
                ("does not authorize GPU", "TA 16/16", "Golden", "Beta")),
            "acceptance/proof boundary mismatch")

    # Validate the diagnosed immutable evidence semantics, not only its hash.
    ready = load(ROOT / EXPECTED_BINDINGS[
        "dispatcher_ready_but_producer_blocked_manifest"][0])
    authorization = load(ROOT / EXPECTED_BINDINGS[
        "non_executable_authorization_template"][0])
    require(ready.get("case_ids_in_order") == EXACT16 and
            ready.get("dispatch_gate", {}).get("compatible_capture_producer_bound") is False and
            ready.get("dispatch_gate", {}).get("authorization_may_not_be_issued") is True,
            "frozen readiness no longer demonstrates producer blocker")
    require(authorization.get("status") ==
            "NOT_ISSUED_INCOMPATIBLE_CAPTURE_PRODUCER_NOT_BOUND" and
            authorization.get("authorized") is False and
            authorization.get("runner_binary_sha256") is None and
            authorization.get("exact16_capture_producer_contract") is False,
            "frozen authorization template is unexpectedly executable")
    plan_rows = (ROOT / EXPECTED_BINDINGS["exact16_ordered_plan"][0]).read_text(
        encoding="utf-8").splitlines()
    require(len(plan_rows) == 16 and
            [row.split("\t")[0] for row in plan_rows] == EXACT16 and
            all(len(row.split("\t")) == 12 for row in plan_rows),
            "frozen exact16 plan content mismatch")
    return {
        "status": "PASS_CAPTURE_ONLY_PRODUCER_CORRECTION_GATE_NO_GPU_NO_AUTH",
        "gate_sha256": sha(path), "case_count": 16,
        "authorization_may_be_signed_now": False,
        "gpu_execution_authorized": False,
        "production_modified_by_this_gate": False,
    }


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("gate", type=Path, nargs="?", default=GATE)
    args = parser.parse_args()
    try:
        print(json.dumps(validate(args.gate,
                                  enforce_frozen_gate_sha=args.gate.resolve() == GATE.resolve()),
                         sort_keys=True))
    except (OSError, ValueError, json.JSONDecodeError, RuntimeError) as error:
        print("FAIL: " + str(error))
        return 2
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
