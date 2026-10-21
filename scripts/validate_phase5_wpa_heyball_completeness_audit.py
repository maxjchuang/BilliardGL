#!/usr/bin/env python3
"""Validate the fail-closed Phase-5 WPA Heyball completeness audit."""

from __future__ import annotations

import argparse
from collections import Counter, defaultdict
import json
from pathlib import Path
import re
import sys


OFFICIAL_URL = "https://wpapool.com/wp-content/uploads/2025/08/250816-Rules-of-Heyball.pdf"
OFFICIAL_SHA256 = "2b92dd5088ea630337326e26b7a2a7a4499a9801f2c5da7e5cc40421d5424a24"
VALID_STATUSES = {"IMPLEMENTED", "PARTIAL", "MISSING", "PROCEDURAL_ONLY"}
VALID_COVERAGE = {"present", "partial", "absent", "not_applicable", "procedural"}
PROTECTED_FILES = {
    "docs/art/phase5-rules-shipping-copy-v1.json",
    "docs/art/phase5-rules-shipping-copy-v1.schema.json",
    "docs/art/phase5-rules-shipping-copy-flat-proof-v1.svg",
}
EXPECTED_STATUS = {
    "WPA-I-05": "PROCEDURAL_ONLY",
    "WPA-II-01": "PROCEDURAL_ONLY",
    "WPA-II-02": "PARTIAL",
    "WPA-II-04": "MISSING",
    "WPA-II-05": "MISSING",
    "WPA-II-06-A-B": "MISSING",
    "WPA-II-06-C": "IMPLEMENTED",
    "WPA-II-06-D": "PARTIAL",
    "WPA-II-06-E": "PARTIAL",
    "WPA-II-06-F": "PARTIAL",
    "WPA-II-06-G": "MISSING",
    "WPA-II-07": "MISSING",
    "WPA-II-08": "PARTIAL",
    "WPA-II-09": "PARTIAL",
    "WPA-II-10-1": "PARTIAL",
    "WPA-II-10-2": "PARTIAL",
    "WPA-II-10-3": "IMPLEMENTED",
    "WPA-II-10-4": "MISSING",
    "WPA-II-11": "IMPLEMENTED",
    "WPA-II-12-1": "IMPLEMENTED",
    "WPA-II-12-2": "PARTIAL",
    "WPA-II-12-3": "PARTIAL",
    "WPA-II-12-4": "MISSING",
    "WPA-II-12-5-6": "MISSING",
    "WPA-II-12-7": "MISSING",
    "WPA-II-12-8": "MISSING",
    "WPA-II-12-9": "PROCEDURAL_ONLY",
    "WPA-II-13": "MISSING",
    "WPA-II-14": "PROCEDURAL_ONLY",
    "WPA-II-15-1": "MISSING",
    "WPA-II-15-2": "MISSING",
    "WPA-II-16": "MISSING",
    "WPA-II-17": "PARTIAL",
    "WPA-II-18": "MISSING",
    "WPA-II-19": "MISSING",
    "WPA-II-20": "PARTIAL",
    "WPA-II-21": "MISSING",
    "WPA-II-22": "PROCEDURAL_ONLY",
    "WPA-II-23-24": "PROCEDURAL_ONLY",
    "WPA-III-01-08": "PROCEDURAL_ONLY",
}
EXPECTED_M4_ROWS = {
    "WPA-II-02", "WPA-II-04", "WPA-II-06-C", "WPA-II-06-D",
    "WPA-II-06-E", "WPA-II-06-F", "WPA-II-07", "WPA-II-08",
    "WPA-II-09", "WPA-II-10-1", "WPA-II-10-3", "WPA-II-10-4",
    "WPA-II-12-1", "WPA-II-12-2", "WPA-II-12-3", "WPA-II-12-5-6",
    "WPA-II-12-7", "WPA-II-13", "WPA-II-15-1", "WPA-II-15-2",
    "WPA-II-16", "WPA-II-17", "WPA-II-20", "WPA-II-21",
}
REQUIRED_MAPPING_KEYS = {
    "shot_facts", "rules_logic", "runtime_action", "reason_text",
    "ui_action", "tests",
}


def _valid_string_list(value: object) -> bool:
    return isinstance(value, list) and all(
        isinstance(item, str) and bool(item.strip()) for item in value
    ) and len(value) == len(set(value))


def validate_audit(value: object) -> list[str]:
    errors: list[str] = []
    if not isinstance(value, dict):
        return ["audit root must be an object"]

    if value.get("schema") != "phase5_wpa_heyball_completeness_audit_v1" or \
            value.get("schema_version") != 1 or \
            value.get("audit_id") != "phase5_wpa_heyball_2025_08_16_product_completeness_v1" or \
            value.get("ruleset_id") != "wpa_heyball_2025_08_16_v1":
        errors.append("audit identity differs")
    if value.get("profile_complete") is not False or \
            value.get("complete_profile_product_exposed") is not False:
        errors.append("rules profile and complete-profile product exposure must remain false")

    source = value.get("normative_source", {})
    if not isinstance(source, dict) or source.get("url") != OFFICIAL_URL or \
            source.get("effective_date") != "2025-08-16" or \
            source.get("retrieval_status") != "VERIFIED" or \
            source.get("page_count") != 21 or source.get("sha256") != OFFICIAL_SHA256:
        errors.append("official PDF identity, retrieval status, page count, or SHA-256 differs")
    if not re.fullmatch(r"\d{4}-\d{2}-\d{2}", str(source.get("retrieved_on", ""))):
        errors.append("official PDF retrieval date must be explicit")

    if set(value.get("protected_files", [])) != PROTECTED_FILES:
        errors.append("three frozen rules-copy artifacts must be explicitly protected")

    matrix = value.get("matrix")
    if not isinstance(matrix, list):
        return errors + ["matrix must be an array"]
    rows: dict[str, dict] = {}
    for index, row in enumerate(matrix):
        if not isinstance(row, dict):
            errors.append(f"matrix row {index} must be an object")
            continue
        row_id = row.get("id")
        if not isinstance(row_id, str) or row_id in rows:
            errors.append(f"matrix row id missing or duplicate at index {index}")
            continue
        rows[row_id] = row
    if set(rows) != set(EXPECTED_STATUS):
        errors.append("official gameplay/procedural clause inventory differs")

    pending_refs: dict[str, set[str]] = defaultdict(set)
    for row_id, row in rows.items():
        status = row.get("status")
        if status not in VALID_STATUSES:
            errors.append(f"invalid status: {row_id}")
        if row_id in EXPECTED_STATUS and status != EXPECTED_STATUS[row_id]:
            errors.append(f"audited status differs: {row_id}")
        source_ref = row.get("source", {})
        pages = source_ref.get("pages") if isinstance(source_ref, dict) else None
        if not isinstance(pages, list) or not pages or any(
                not isinstance(page, int) or page < 1 or page > 21 for page in pages):
            errors.append(f"official page citation invalid: {row_id}")
        if not isinstance(source_ref, dict) or not all(
                isinstance(source_ref.get(key), str) and source_ref[key].strip()
                for key in ("chapter", "clause", "locator")):
            errors.append(f"official chapter/clause locator missing: {row_id}")
        if not isinstance(row.get("requirement"), str) or not row["requirement"].strip():
            errors.append(f"official requirement summary missing: {row_id}")
        if not REQUIRED_MAPPING_KEYS.issubset(row):
            errors.append(f"ShotFacts/rules/runtime/reason/UI/test mapping incomplete: {row_id}")
            continue
        for key in ("shot_facts", "rules_logic", "runtime_action", "tests"):
            mapping = row.get(key, {})
            if not isinstance(mapping, dict) or mapping.get("coverage") not in VALID_COVERAGE or \
                    not _valid_string_list(mapping.get("bindings", [])):
                errors.append(f"invalid {key} mapping: {row_id}")
        reason = row.get("reason_text", {})
        if not isinstance(reason, dict) or reason.get("coverage") not in VALID_COVERAGE or \
                not _valid_string_list(reason.get("reason_codes", [])) or \
                not _valid_string_list(reason.get("text_keys", [])):
            errors.append(f"invalid reason/text-key mapping: {row_id}")
        ui = row.get("ui_action", {})
        if not isinstance(ui, dict) or ui.get("coverage") not in VALID_COVERAGE or \
                not _valid_string_list(ui.get("bindings", [])) or \
                not isinstance(ui.get("interactive_actions_product_exposed"), bool) or \
                not _valid_string_list(ui.get("pending_capabilities", [])):
            errors.append(f"invalid product UI/action mapping: {row_id}")
        else:
            for capability in ui["pending_capabilities"]:
                pending_refs[capability].add(row_id)
            if ui["pending_capabilities"] and ui["interactive_actions_product_exposed"]:
                errors.append(f"pending interactive UI/action must fail closed: {row_id}")
        if not isinstance(row.get("gaps"), list) or not all(
                isinstance(gap, str) and gap.strip() for gap in row["gaps"]):
            errors.append(f"gap list invalid: {row_id}")
        if not isinstance(row.get("policy_decision_required"), bool) or \
                not isinstance(row.get("m4_required"), bool):
            errors.append(f"policy/M4 booleans invalid: {row_id}")

    actual_counts = Counter(row.get("status") for row in rows.values())
    expected_summary = {status: actual_counts[status] for status in VALID_STATUSES}
    expected_summary["total"] = len(rows)
    if value.get("status_summary") != expected_summary:
        errors.append("status summary is not validator-computed from the matrix")

    pending = value.get("pending_capabilities")
    pending_by_id: dict[str, dict] = {}
    if not isinstance(pending, list):
        errors.append("pending_capabilities must be an array")
        pending = []
    for item in pending:
        if not isinstance(item, dict) or not isinstance(item.get("id"), str) or \
                item["id"] in pending_by_id:
            errors.append("pending capability id missing or duplicate")
            continue
        pending_by_id[item["id"]] = item
        if item.get("state") != "pending" or item.get("product_exposed") is not False:
            errors.append(f"pending capability must remain fail closed: {item['id']}")
        if not item.get("source_rows") or not _valid_string_list(item.get("source_rows", [])):
            errors.append(f"pending capability source rows invalid: {item['id']}")
        elif set(item["source_rows"]) != pending_refs.get(item["id"], set()):
            errors.append(f"pending capability row references differ: {item['id']}")
    if set(pending_by_id) != set(pending_refs):
        errors.append("pending capability catalog differs from matrix references")

    m4 = value.get("m4_minimum_closure", {})
    m4_rows = set(m4.get("required_rows", [])) if isinstance(m4, dict) else set()
    flagged_rows = {row_id for row_id, row in rows.items() if row.get("m4_required") is True}
    if not isinstance(m4, dict) or m4.get("gate") != "M4_RULES_PRODUCT_MINIMUM" or \
            m4.get("profile_must_remain_incomplete_until_gate_passes") is not True or \
            m4_rows != EXPECTED_M4_ROWS or flagged_rows != EXPECTED_M4_ROWS:
        errors.append("M4 minimum closure row set or fail-closed gate differs")
    if not _valid_string_list(m4.get("acceptance", []) if isinstance(m4, dict) else []):
        errors.append("M4 minimum closure requires explicit acceptance criteria")

    conflict = rows.get("WPA-II-12-3", {})
    conflict_source = conflict.get("source", {}) if isinstance(conflict, dict) else {}
    if conflict.get("status") != "PARTIAL" or \
            conflict.get("policy_decision_required") is not True or \
            set(conflict_source.get("pages", [])) != {9, 11} or \
            not any("contradiction" in gap.lower() for gap in conflict.get("gaps", [])):
        errors.append("official 12.3/17(e) off-table contradiction must remain explicit")

    return errors


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("audit", type=Path)
    args = parser.parse_args()
    try:
        value = json.loads(args.audit.read_text(encoding="utf-8"))
    except (OSError, json.JSONDecodeError) as error:
        print(f"Phase5 WPA Heyball audit validation failed: {error}", file=sys.stderr)
        return 1
    errors = validate_audit(value)
    if errors:
        for error in errors:
            print("ERROR: " + error, file=sys.stderr)
        return 1
    summary = value["status_summary"]
    print(
        "Phase5 WPA Heyball audit PASS: "
        f"rows={summary['total']} implemented={summary['IMPLEMENTED']} "
        f"partial={summary['PARTIAL']} missing={summary['MISSING']} "
        f"procedural={summary['PROCEDURAL_ONLY']} "
        f"m4_required={len(value['m4_minimum_closure']['required_rows'])} "
        "profile_complete=false pending_fail_closed=true"
    )
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
