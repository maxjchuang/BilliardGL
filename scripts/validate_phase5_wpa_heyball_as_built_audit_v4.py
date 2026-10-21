#!/usr/bin/env python3
"""Validate the WPA II 10(4)-only product-reachability audit successor."""

from __future__ import annotations

from collections import Counter
import argparse
import hashlib
import json
from pathlib import Path
from typing import Any


ROOT = Path(__file__).resolve().parents[1]
DEFAULT_AUDIT = ROOT / "docs/rules/phase5-wpa-heyball-as-built-reconciliation-audit-v4.json"
V3_PATH = ROOT / "docs/rules/phase5-wpa-heyball-as-built-reconciliation-audit-v3.json"
EXPECTED_V3_SHA = "d7ab2104fac9fd85fe8d7336ae658cc9baaaa6b4fa866cd4e7815c51404ad033"
EXPECTED_SUMMARY = {"total": 40, "implemented": 11, "partial": 9,
                    "missing": 13, "procedural_only": 7}
EXPECTED_SOURCES = {
    "src/Billiards/rules/group_choice_authority.h": "9ebf1e938b7248e34245f3ce40205d5438e06d6d3463a2d7cf986063e55101d3",
    "src/Billiards/rules/rules_decision_ui_model.h": "6d66dbde51464dcd6e18ffb062aff55ff5ec659c1e43806423d67d79ce45c747",
    "src/Billiards/rules/rules_decision_ui_model.cpp": "9e3248eb2280612e9b65ada92452f0e8c64b92aaf495c01a38d3455d4a769113",
    "src/Billiards/game_runtime.h": "61453e15d88d5581fa439fa77eb6b834fc0a22a5366b470b182b17b8d4d9f725",
    "src/Billiards/game_runtime.cpp": "fca6d4f8ffc54b5d68f3aa24928f71416b003ac8ae37ef7ab4d376aa78684ca4",
    "src/Billiards/automation_controller.cpp": "dd8c141565aae8ac05a60c8fa0ce392729255d6f030d1f79d79d666ddeda4357",
    "src/Billiards/render/modern/modern_product_session.h": "c581524cbb7114d54b6fe7f9e6614edece61972adb1667ef5e7a7622c0268d5e",
    "src/Billiards/render/modern/modern_product_session.cpp": "506a134777fa172dcee52a43a7453e7ba20d1a1a441dacab471d34e951495859",
    "tests/rules_runtime_integration_tests.cpp": "aab289bcb7e01d037e6022d41676145e09027d2d90317b9cf9e2e017335f0698",
    "tests/rules_decision_session_integration_tests.cpp": "37053978067e97b897cad19d8f503e9e7fef62d93d0ebb041a508075d0de712b",
}
EXPECTED_CPU = {
    "BilliardsRulesRuntimeIntegrationTests": "b774003b6b96676eb3c79b79892cb5deecd9e78e093f74f716c448f8cdc27cd1",
    "BilliardsRulesDecisionSessionIntegrationTests": "18320838be66101420ba402028554ed6c7ea6ce29039d0d82e9aa63d7efea164",
    "BilliardsAutomationControllerTests": "aadf738383eeae2d76d602dd877e3eb494600e213c2fe01f52a70bfd15b6986b",
}


def file_sha(path: Path) -> str:
    return hashlib.sha256(path.read_bytes()).hexdigest()


def safe_path(raw: Any) -> Path | None:
    if not isinstance(raw, str) or not raw or Path(raw).is_absolute() or \
            ".." in Path(raw).parts:
        return None
    result = (ROOT / raw).resolve()
    try:
        result.relative_to(ROOT.resolve())
    except ValueError:
        return None
    return result


def validate(value: Any, *, check_files: bool = True) -> list[str]:
    errors: list[str] = []
    if not isinstance(value, dict):
        return ["audit_not_object"]
    if value.get("schema") != "phase5_wpa_heyball_as_built_reconciliation_audit_v4" or \
            value.get("schema_version") != 4 or \
            value.get("audit_scope") != "targeted_product_closure_wpa_ii_10_4":
        errors.append("schema_or_scope")
    if value.get("frozen_v3_predecessor") != {
        "path": "docs/rules/phase5-wpa-heyball-as-built-reconciliation-audit-v3.json",
        "sha256": EXPECTED_V3_SHA,
    }:
        errors.append("frozen_v3_predecessor")

    matrix = value.get("matrix")
    if not isinstance(matrix, list) or len(matrix) != 40:
        return errors + ["matrix"]
    rows = {row.get("id"): row for row in matrix if isinstance(row, dict)}
    if len(rows) != 40 or "WPA-II-10-4" not in rows:
        return errors + ["matrix_ids"]
    target = rows["WPA-II-10-4"]
    if target.get("status") != "IMPLEMENTED" or \
            target.get("product_reachable") is not True or \
            target.get("gaps") != [] or \
            target.get("m4_required") is not True or \
            target.get("product_action_required") is not True:
        errors.append("target_row")
    target_paths = {item.get("path") for item in target.get("evidence", [])
                    if isinstance(item, dict)}
    required_target_paths = {
        "src/Billiards/rules/chinese_eight_ball_rules.cpp",
        "src/Billiards/rules/group_choice_authority.h",
        "src/Billiards/game_runtime.cpp",
        "src/Billiards/render/modern/modern_product_session.cpp",
        "tests/rules_runtime_integration_tests.cpp",
        "tests/rules_decision_session_integration_tests.cpp",
    }
    if target_paths != required_target_paths:
        errors.append("target_evidence")

    closure = value.get("wpa_ii_10_4_product_closure", {})
    if closure.get("row") != "WPA-II-10-4" or \
            closure.get("allowed_actions") != [
                "rules.group_choice.choose_solids",
                "rules.group_choice.choose_stripes"] or \
            closure.get("requires_live_offered_action") is not True or \
            closure.get("requires_match_screen") is not True or \
            closure.get("requires_local_offered_actor") is not True or \
            closure.get("ruleset_capability_flow_revision_and_state_hash_bound") is not True or \
            closure.get("exact_retry_returns_original_ack") is not True or \
            closure.get("conflicting_retry_fails_without_mutation") is not True or \
            closure.get("foundation_actions_product_exposed") is not False or \
            closure.get("other_rules_parent_actions_product_exposed_by_this_increment") is not False or \
            closure.get("rules_decision_ui_product_exposed") is not False or \
            closure.get("gpu_executed") is not False:
        errors.append("closure_contract")

    try:
        predecessor = json.loads(V3_PATH.read_text(encoding="utf-8"))
    except (OSError, json.JSONDecodeError):
        predecessor = None
        errors.append("predecessor_load")
    if isinstance(predecessor, dict):
        previous_rows = {row["id"]: row for row in predecessor["matrix"]}
        for row_id, row in rows.items():
            if row_id == "WPA-II-10-4":
                continue
            old = previous_rows[row_id]
            for field in ("status", "product_reachable", "assessment", "gaps",
                          "m4_required", "policy_decision_required",
                          "product_action_required", "core_increments"):
                if row.get(field) != old.get(field):
                    errors.append(f"non_target_{row_id}_{field}")

    counts = Counter(row.get("status") for row in rows.values())
    computed = {"total": len(rows), "implemented": counts["IMPLEMENTED"],
                "partial": counts["PARTIAL"], "missing": counts["MISSING"],
                "procedural_only": counts["PROCEDURAL_ONLY"]}
    if value.get("summary") != EXPECTED_SUMMARY or computed != EXPECTED_SUMMARY:
        errors.append("summary")
    expected_unclosed = [row["id"] for row in matrix
                         if row.get("m4_required") and
                         row.get("status") != "IMPLEMENTED"]
    if value.get("m4_required_unclosed") != expected_unclosed or \
            "WPA-II-10-4" in expected_unclosed:
        errors.append("m4_unclosed")
    if any("WPA-II-10-4" in plan.get("rows", []) for plan in
           value.get("next_deterministic_implementation_plan", [])):
        errors.append("target_still_in_plan")
    increments = value.get("completed_targeted_increments", [])
    if not any(item.get("rows") == ["WPA-II-10-4"] for item in increments
               if isinstance(item, dict)):
        errors.append("completed_increment")

    bindings = value.get("wpa_ii_10_4_implementation_bindings", {})
    source_rows = bindings.get("changed_files")
    expected_source_rows = [{"path": path, "sha256": digest}
                            for path, digest in EXPECTED_SOURCES.items()]
    if source_rows != expected_source_rows:
        errors.append("source_bindings")
    cpu_rows = bindings.get("cpu_results")
    expected_cpu_rows = [{"target": target_name, "sha256": digest,
                          "exit_code": 0}
                         for target_name, digest in EXPECTED_CPU.items()]
    if cpu_rows != expected_cpu_rows or \
            bindings.get("cmake_changed_by_increment") is not False or \
            bindings.get("gpu_cocoa_or_opengl_executed") is not False or \
            bindings.get("readiness_generated") is not False or \
            bindings.get("authorization_issued") is not False:
        errors.append("cpu_or_forbidden_bindings")

    if check_files:
        if not V3_PATH.is_file() or file_sha(V3_PATH) != EXPECTED_V3_SHA:
            errors.append("predecessor_drift")
        for path_text, digest in EXPECTED_SOURCES.items():
            path = safe_path(path_text)
            if path is None or not path.is_file() or file_sha(path) != digest:
                errors.append(f"source_drift:{path_text}")
        build_root = ROOT / "build-wpa-10-4-product-group-choice-cpu-v1"
        for target_name, digest in EXPECTED_CPU.items():
            path = build_root / target_name
            if not path.is_file() or file_sha(path) != digest:
                errors.append(f"cpu_drift:{target_name}")
        for row in matrix:
            for item in row.get("evidence", []):
                path = safe_path(item.get("path"))
                if path is None or not path.is_file() or \
                        file_sha(path) != item.get("sha256"):
                    errors.append(f"evidence_drift:{row.get('id')}:{item.get('path')}")
    return errors


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--audit", type=Path, default=DEFAULT_AUDIT)
    parser.add_argument("--no-file-checks", action="store_true")
    args = parser.parse_args()
    try:
        value = json.loads(args.audit.read_text(encoding="utf-8"))
    except (OSError, json.JSONDecodeError) as error:
        print(json.dumps({"status": "FAIL", "errors": [f"load:{error}"]}))
        return 1
    errors = validate(value, check_files=not args.no_file_checks)
    print(json.dumps({"status": "PASS" if not errors else "FAIL",
                      "errors": errors, "audit": str(args.audit)},
                     sort_keys=True))
    if errors:
        return 1
    print("PASS_PHASE5_WPA_HEYBALL_AS_BUILT_AUDIT_V4_WPA_II_10_4")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
