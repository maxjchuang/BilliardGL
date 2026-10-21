#!/usr/bin/env python3
"""Validate the targeted WPA §4/§7 as-built audit successor."""

from __future__ import annotations

import argparse
from collections import Counter
import hashlib
import json
from pathlib import Path
import re

try:
    from validate_phase5_wpa_heyball_as_built_audit_v2 import validate_audit as validate_v2
except ModuleNotFoundError:
    from scripts.validate_phase5_wpa_heyball_as_built_audit_v2 import validate_audit as validate_v2


ROOT = Path(__file__).resolve().parents[1]
DEFAULT_AUDIT = ROOT / "docs/rules/phase5-wpa-heyball-as-built-reconciliation-audit-v3.json"
V2_PATH = ROOT / "docs/rules/phase5-wpa-heyball-as-built-reconciliation-audit-v2.json"
V2_SHA = "a1e675cc32057880a4805ddafaf93c2192b6f127ae6bc2f28c71f5390d444809"
GATE_PATH = ROOT / "docs/rules/wpa-ii-04-official-rack-product-migration-gate-v1.json"
GATE_SHA = "bf02b866e6017278c60366718535053049738555d72a573c8afc1f675d5c5545"
EXPECTED_COUNTS = {"total": 40, "implemented": 10, "partial": 10, "missing": 13, "procedural_only": 7}


def sha256(path: Path) -> str:
    return hashlib.sha256(path.read_bytes()).hexdigest()


def validate(value: object, *, check_files: bool = True) -> list[str]:
    errors: list[str] = []
    if not isinstance(value, dict):
        return ["audit root must be an object"]
    if value.get("schema") != "phase5_wpa_heyball_as_built_reconciliation_audit_v3" or \
            value.get("schema_version") != 3 or \
            value.get("audit_id") != "phase5_wpa_heyball_2025_08_16_as_built_reconciliation_v3" or \
            value.get("audit_scope") != "targeted_product_closure_wpa_ii_04_and_07":
        errors.append("v3 identity/scope differs")
    if value.get("profile_complete") is not False or \
            value.get("product_exposed") is not False or \
            value.get("complete_profile_product_exposed") is not False:
        errors.append("complete profile/global product exposure must remain false")

    expected_v2 = {"path": str(V2_PATH.relative_to(ROOT)), "sha256": V2_SHA}
    if value.get("frozen_v2_predecessor") != expected_v2:
        errors.append("frozen v2 binding differs")
    if check_files and sha256(V2_PATH) != V2_SHA:
        errors.append("frozen v2 bytes changed")
    else:
        predecessor = json.loads(V2_PATH.read_text(encoding="utf-8"))
        if validate_v2(predecessor, check_files=False):
            errors.append("frozen v2 semantic validation failed")

    expected_gate = {
        "path": str(GATE_PATH.relative_to(ROOT)),
        "sha256": GATE_SHA,
        "status": "APPROVED_NO_GPU_PRODUCT_LAYOUT_MIGRATION_REQUIRED",
    }
    if value.get("official_rack_migration_gate") != expected_gate:
        errors.append("official-rack migration gate binding differs")
    if check_files and sha256(GATE_PATH) != GATE_SHA:
        errors.append("official-rack migration gate bytes changed")

    boundary = value.get("targeted_product_closure")
    if boundary != {
        "rows": ["WPA-II-04", "WPA-II-07"],
        "foundation_actions_product_exposed": False,
        "rules_decision_ui_product_exposed": False,
        "seeded_numbered_ball_order_normative": False,
        "historical_phase2_phase4_phase5_visual_evidence_inheritable": False,
    }:
        errors.append("targeted product boundary differs")
    impact = value.get("downstream_binding_impact")
    if impact != {
        "phase2_visual_readiness": "FRESH_SOURCE_BINARY_BINDINGS_REQUIRED",
        "phase4_probe_digest": "NOT_RECOMPUTED_NO_INHERITANCE_CLAIM",
        "phase4_readiness": "FRESH_SOURCE_BINARY_BINDINGS_REQUIRED_IF_PRODUCT_LIFECYCLE_IS_LINKED",
        "phase5_machine_inputs": "SOURCE_BINDING_DRIFT_FRESH_MACHINE_AND_READINESS_REQUIRED",
        "phase5_prior_modern_product_session_sha256":
            "f5493e5f9b610312180f57bc60a624167c55e82627c08ad572539e6f6d91d74b",
        "phase5_successor_modern_product_session_sha256":
            "8e962dfc76149a4461e011941231e5e72860aed9f8cb6f006169e269455cd174",
        "successor_product_shell_controller_sha256":
            "eb1a754778808e23232ff14ab1a411be9189da876070910961ec6f4cbdf69a6c",
        "old_visual_or_machine_evidence_modified": False,
        "gpu_executed": False,
    }:
        errors.append("downstream source/probe/readiness impact differs")

    matrix = value.get("matrix")
    if not isinstance(matrix, list):
        return errors + ["matrix must be an array"]
    rows = {row.get("id"): row for row in matrix if isinstance(row, dict)}
    predecessor_rows = {row["id"]: row for row in
        json.loads(V2_PATH.read_text(encoding="utf-8"))["matrix"]}
    if set(rows) != set(predecessor_rows) or len(rows) != 40:
        errors.append("official 40-row inventory differs")
        return errors

    for row_id, row in rows.items():
        if row_id in {"WPA-II-04", "WPA-II-07"}:
            if row.get("status") != "IMPLEMENTED" or \
                    row.get("product_action_required") is not True or \
                    row.get("product_reachable") is not True or row.get("gaps") != []:
                errors.append(f"target row not honestly closed: {row_id}")
        else:
            prior = predecessor_rows[row_id]
            for key in ("status", "product_action_required", "product_reachable",
                        "policy_decision_required", "m4_required", "gaps"):
                if row.get(key) != prior.get(key):
                    errors.append(f"non-target row conclusion changed: {row_id}/{key}")
        evidence = row.get("evidence")
        if not isinstance(evidence, list) or not evidence:
            errors.append(f"evidence missing: {row_id}")
            continue
        kinds = {item.get("kind") for item in evidence if isinstance(item, dict)}
        if row.get("status") == "IMPLEMENTED" and not {
                "source_positive", "test_positive"}.issubset(kinds):
            errors.append(f"IMPLEMENTED row lacks source/test evidence: {row_id}")
        for item in evidence:
            if not isinstance(item, dict) or \
                    item.get("kind") not in {"source_positive", "source_boundary", "test_positive", "test_boundary", "product_gate", "migration_gate"} or \
                    not re.fullmatch(r"[0-9a-f]{64}", str(item.get("sha256", ""))) or \
                    not isinstance(item.get("path"), str) or Path(item["path"]).is_absolute():
                errors.append(f"evidence metadata invalid: {row_id}")
                continue
            path = (ROOT / item["path"]).resolve()
            try:
                path.relative_to(ROOT.resolve())
            except ValueError:
                errors.append(f"evidence escapes root: {row_id}")
                continue
            lines = item.get("lines")
            if not isinstance(lines, dict) or not isinstance(lines.get("start"), int) or \
                    not isinstance(lines.get("end"), int) or lines["start"] < 1 or \
                    lines["end"] < lines["start"]:
                errors.append(f"evidence line range invalid: {row_id}")
                continue
            if check_files and (not path.is_file() or sha256(path) != item["sha256"] or
                    lines["end"] > len(path.read_text(encoding="utf-8").splitlines())):
                errors.append(f"evidence path/SHA/range mismatch: {row_id}/{item['path']}")

    for row_id, required in {
        "WPA-II-04": {"src/Billiards/product_shell_controller.cpp", "tests/product_shell_tests.cpp"},
        "WPA-II-07": {"src/Billiards/render/modern/modern_product_session.cpp", "tests/rules_decision_session_integration_tests.cpp"},
    }.items():
        paths = {item["path"] for item in rows[row_id]["evidence"]}
        if not required.issubset(paths):
            errors.append(f"target product evidence missing: {row_id}")

    counts = Counter(row.get("status") for row in rows.values())
    computed = {"total": len(rows), **{status.lower(): counts[status] for status in
        ("IMPLEMENTED", "PARTIAL", "MISSING", "PROCEDURAL_ONLY")}}
    if value.get("summary") != EXPECTED_COUNTS or computed != EXPECTED_COUNTS:
        errors.append("v3 status counts must be exactly 10/10/13/7")
    expected_unclosed = [row_id for row_id in predecessor_rows
        if rows[row_id].get("m4_required") and rows[row_id].get("status") != "IMPLEMENTED"]
    if value.get("m4_required_unclosed") != expected_unclosed or \
            "WPA-II-04" in expected_unclosed or "WPA-II-07" in expected_unclosed:
        errors.append("M4 unclosed inventory differs")
    if any(item.get("rows") == ["WPA-II-04", "WPA-II-07"] for item in
            value.get("next_deterministic_implementation_plan", [])):
        errors.append("completed target batch remains in next plan")
    completed = value.get("completed_targeted_increments")
    if not isinstance(completed, list) or len(completed) != 1 or \
            completed[0].get("rows") != ["WPA-II-04", "WPA-II-07"]:
        errors.append("completed targeted increment missing")
    return errors


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--audit", type=Path, default=DEFAULT_AUDIT)
    parser.add_argument("--no-file-checks", action="store_true")
    args = parser.parse_args()
    try:
        value = json.loads(args.audit.read_text(encoding="utf-8"))
    except (OSError, json.JSONDecodeError) as error:
        print(json.dumps({"status": "FAIL", "errors": [str(error)]}, sort_keys=True))
        return 1
    errors = validate(value, check_files=not args.no_file_checks)
    print(json.dumps({
        "status": "PASS_PHASE5_WPA_HEYBALL_AS_BUILT_AUDIT_V3" if not errors else "FAIL",
        "errors": errors,
        "gpu_executed": False,
    }, sort_keys=True))
    return 0 if not errors else 1


if __name__ == "__main__":
    raise SystemExit(main())
