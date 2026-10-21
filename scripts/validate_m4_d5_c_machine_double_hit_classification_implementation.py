#!/usr/bin/env python3
import hashlib
import json
import pathlib
import sys

ROOT = pathlib.Path(__file__).resolve().parents[1]
RECEIPT = ROOT / "docs/rules/m4-d5-c-machine-double-hit-classification-implementation-receipt-v1.json"

def sha(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()

def require(ok, message):
    if not ok:
        raise ValueError(message)

def validate(data):
    require(data["schema"] == "m4_d5_c_machine_double_hit_classification_implementation_receipt_v1", "schema")
    require(data["status"] == "ROOT_SELECTED_D5_C_CPU_VERIFIED_AUDIT_ROWS_OPEN", "status")
    for binding in data["authority_bindings"].values():
        require(sha(ROOT / binding["path"]) == binding["sha256"], "authority binding")
    require(data["authority_bindings"]["root_selected_policy"]["bundle"] == "D5C-A_REFEREE_CLOSE_CASES_EXACT_ORDER", "bundle")
    require(data["tests_first"] == {"red_observed": True, "red_exit_code": 1, "red_failure": "tests/double_hit_classifier_tests.cpp: fatal error: rules/double_hit_classifier.h file not found", "green": True}, "red green")
    contract = data["contract"]
    require(contract["very_close_surface_gap_cm"] is None and contract["light_graze_normal_impulse_ratio"] is None, "null thresholds")
    require(contract["tip_separation_tie_tolerance_seconds"] == 0.0, "exact tie")
    require(contract["interval_count_precedes_signed_review"] and contract["signed_close_case_otherwise_review_only"], "precedence")
    require(not contract["push_or_frozen_exception_classified"] and not contract["visible_or_global_ui_changed"] and not contract["gpu_or_readiness_used"], "forbidden claims")
    gate = json.loads((ROOT / "docs/rules/m4-d5-c-machine-double-hit-classification-implementation-design-gate-v1.json").read_text())
    require(data["fixtures"] == gate["required_fixtures"], "fixtures")
    expected_prod = gate["implementation_allowlist"]["production_files"]
    expected_tests = gate["implementation_allowlist"]["test_files"]
    require([x["path"] for x in data["source_bindings"]] == expected_prod, "production allowlist")
    require([x["path"] for x in data["test_bindings"]] == expected_tests, "test allowlist")
    for binding in data["source_bindings"] + data["test_bindings"] + [data["build_binding"]]:
        require(sha(ROOT / binding["path"]) == binding["sha256"], "live binding " + binding["path"])
    cpu = data["cpu"]
    require(cpu["passed_count"] == 17 and cpu["failed_count"] == 0 and len(cpu["binaries"]) == 17 and cpu["gpu"] is False, "cpu summary")
    for binary in cpu["binaries"]:
        require(binary["exit_code"] == 0 and sha(ROOT / binary["path"]) == binary["sha256"], "binary binding")
    completion = data["completion"]
    require(completion["d5_c_machine_double_hit_classification_implemented"], "implementation")
    require(not completion["d5_complete"] and not completion["wpa_ii_12_5_6_or_15_1_promoted"] and not completion["audit_rows_changed"] and not completion["m4_complete"], "no false complete")

def main():
    try:
        validate(json.loads(RECEIPT.read_text()))
    except Exception as exc:
        print("INVALID_M4_D5_C_IMPLEMENTATION_RECEIPT:", exc)
        return 1
    print("PASS_M4_D5_C_IMPLEMENTATION_RECEIPT")
    return 0

if __name__ == "__main__":
    sys.exit(main())
