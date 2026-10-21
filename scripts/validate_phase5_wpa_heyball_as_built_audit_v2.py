#!/usr/bin/env python3
"""Validate the fail-closed Phase-5 WPA Heyball as-built audit v2."""

from __future__ import annotations

import argparse
from collections import Counter
from datetime import date
import hashlib
import json
from pathlib import Path
import re
import sys


ROOT = Path(__file__).resolve().parents[1]
DEFAULT_AUDIT = ROOT / "docs/rules/phase5-wpa-heyball-as-built-reconciliation-audit-v2.json"
DEFAULT_MARKDOWN = ROOT / "docs/rules/phase5-wpa-heyball-as-built-reconciliation-audit-v2.md"
OFFICIAL_URL = "https://wpapool.com/wp-content/uploads/2025/08/250816-Rules-of-Heyball.pdf"
OFFICIAL_SHA256 = "2b92dd5088ea630337326e26b7a2a7a4499a9801f2c5da7e5cc40421d5424a24"
VALID_STATUS = {"IMPLEMENTED", "PARTIAL", "MISSING", "PROCEDURAL_ONLY"}
VALID_EVIDENCE_KIND = {"source_positive", "source_boundary", "test_positive", "test_boundary", "product_gate"}
EXPECTED_COUNTS = {"total": 40, "implemented": 8, "partial": 12, "missing": 13, "procedural_only": 7}
EXPECTED_STATUS = {
    "WPA-I-05": "PROCEDURAL_ONLY", "WPA-II-01": "PROCEDURAL_ONLY",
    "WPA-II-02": "IMPLEMENTED", "WPA-II-04": "PARTIAL",
    "WPA-II-05": "MISSING", "WPA-II-06-A-B": "MISSING",
    "WPA-II-06-C": "PARTIAL", "WPA-II-06-D": "PARTIAL",
    "WPA-II-06-E": "PARTIAL", "WPA-II-06-F": "PARTIAL",
    "WPA-II-06-G": "MISSING", "WPA-II-07": "PARTIAL",
    "WPA-II-08": "PARTIAL", "WPA-II-09": "PARTIAL",
    "WPA-II-10-1": "IMPLEMENTED", "WPA-II-10-2": "PARTIAL",
    "WPA-II-10-3": "IMPLEMENTED", "WPA-II-10-4": "PARTIAL",
    "WPA-II-11": "IMPLEMENTED", "WPA-II-12-1": "IMPLEMENTED",
    "WPA-II-12-2": "IMPLEMENTED", "WPA-II-12-3": "PARTIAL",
    "WPA-II-12-4": "MISSING", "WPA-II-12-5-6": "MISSING",
    "WPA-II-12-7": "MISSING", "WPA-II-12-8": "MISSING",
    "WPA-II-12-9": "PROCEDURAL_ONLY", "WPA-II-13": "IMPLEMENTED",
    "WPA-II-14": "PROCEDURAL_ONLY", "WPA-II-15-1": "MISSING",
    "WPA-II-15-2": "MISSING", "WPA-II-16": "MISSING",
    "WPA-II-17": "PARTIAL", "WPA-II-18": "MISSING",
    "WPA-II-19": "MISSING", "WPA-II-20": "IMPLEMENTED",
    "WPA-II-21": "MISSING", "WPA-II-22": "PROCEDURAL_ONLY",
    "WPA-II-23-24": "PROCEDURAL_ONLY", "WPA-III-01-08": "PROCEDURAL_ONLY",
}
EXPECTED_PREDECESSOR = {
    "docs/rules/phase5-wpa-heyball-completeness-audit.json": "c70e8662dd523b04eefab5745c4cd9023c2ae9dfa374f1b289d684d532ea8555",
    "docs/rules/phase5-wpa-heyball-completeness-audit.md": "f4384c54a245184c281697d6e18562cf1d8a656856f07174d590e351f2b1fd04",
    "docs/rules/phase5-wpa-heyball-completeness-audit.schema.json": "6694e31bed405fa8c3258b9103110f9b4ff6aa7bbb5da1d9946183510471529d",
    "scripts/validate_phase5_wpa_heyball_completeness_audit.py": "735c70bea8adf3529707cf994d52a4a8b74495492b7244fe93baf9dfae660837",
    "tests/visual/test_phase5_wpa_heyball_completeness_audit.py": "ee2db00a4580084fc9466574b2e5e8a495062bf6a3a846abecfa1c27eb3f9184",
}


def _sha(path: Path) -> str:
    return hashlib.sha256(path.read_bytes()).hexdigest()


def _inside_root(path_text: str) -> Path | None:
    if not isinstance(path_text, str) or not path_text or Path(path_text).is_absolute():
        return None
    candidate = (ROOT / path_text).resolve()
    try:
        candidate.relative_to(ROOT.resolve())
    except ValueError:
        return None
    return candidate


def validate_audit(value: object, *, check_files: bool = True) -> list[str]:
    errors: list[str] = []
    if not isinstance(value, dict):
        return ["audit root must be an object"]
    if value.get("schema") != "phase5_wpa_heyball_as_built_reconciliation_audit_v2" or \
            value.get("schema_version") != 2 or \
            value.get("audit_id") != "phase5_wpa_heyball_2025_08_16_as_built_reconciliation_v2" or \
            value.get("ruleset_id") != "wpa_heyball_2025_08_16_v1" or \
            value.get("audit_scope") != "read_only_as_built_core_a_through_h":
        errors.append("v2 audit identity or scope differs")
    if value.get("profile_complete") is not False or value.get("product_exposed") is not False or \
            value.get("complete_profile_product_exposed") is not False:
        errors.append("profile_complete, product_exposed and complete-profile product exposure must remain false")
    try:
        date.fromisoformat(str(value.get("generated_on", "")))
    except ValueError:
        errors.append("generated_on must be an ISO date")

    source = value.get("normative_source")
    if not isinstance(source, dict) or source.get("url") != OFFICIAL_URL or \
            source.get("sha256") != OFFICIAL_SHA256 or source.get("page_count") != 21 or \
            source.get("effective_date") != "2025-08-16" or source.get("retrieval_status") != "VERIFIED":
        errors.append("official PDF identity differs")

    frozen = value.get("frozen_predecessor_files")
    if frozen != EXPECTED_PREDECESSOR:
        errors.append("frozen v1 predecessor path/SHA inventory differs")
    elif check_files:
        for path_text, expected in EXPECTED_PREDECESSOR.items():
            path = ROOT / path_text
            if not path.is_file() or _sha(path) != expected:
                errors.append(f"frozen v1 predecessor changed: {path_text}")

    matrix = value.get("matrix")
    if not isinstance(matrix, list):
        return errors + ["matrix must be an array"]
    rows: dict[str, dict] = {}
    for index, row in enumerate(matrix):
        if not isinstance(row, dict) or not isinstance(row.get("id"), str) or row["id"] in rows:
            errors.append(f"matrix row {index} is invalid or duplicate")
            continue
        rows[row["id"]] = row
    if set(rows) != set(EXPECTED_STATUS):
        errors.append("official 40-row inventory differs")

    for row_id, row in rows.items():
        status = row.get("status")
        if status not in VALID_STATUS or EXPECTED_STATUS.get(row_id) != status:
            errors.append(f"as-built status differs: {row_id}")
        source_ref = row.get("source")
        if not isinstance(source_ref, dict) or not isinstance(source_ref.get("pages"), list) or \
                not source_ref["pages"] or any(not isinstance(page, int) or page < 1 or page > 21 for page in source_ref["pages"]) or \
                not all(isinstance(source_ref.get(key), str) and source_ref[key].strip() for key in ("chapter", "clause", "locator")):
            errors.append(f"official source locator invalid: {row_id}")
        if not isinstance(row.get("requirement"), str) or not row["requirement"].strip() or \
                not isinstance(row.get("assessment"), str) or not row["assessment"].strip():
            errors.append(f"requirement/assessment missing: {row_id}")
        if not isinstance(row.get("policy_decision_required"), bool) or \
                not isinstance(row.get("m4_required"), bool) or \
                not isinstance(row.get("product_action_required"), bool) or \
                not isinstance(row.get("product_reachable"), bool):
            errors.append(f"row booleans invalid: {row_id}")
        if row.get("product_action_required") and not row.get("product_reachable") and status == "IMPLEMENTED":
            errors.append(f"unproductized executor/authority cannot be IMPLEMENTED: {row_id}")
        if not isinstance(row.get("core_increments"), list) or any(core not in set("ABCDEFGH") for core in row.get("core_increments", [])):
            errors.append(f"Core A-H mapping invalid: {row_id}")
        if not isinstance(row.get("gaps"), list) or any(not isinstance(gap, str) or not gap.strip() for gap in row.get("gaps", [])):
            errors.append(f"gap inventory invalid: {row_id}")

        evidence = row.get("evidence")
        if not isinstance(evidence, list) or not evidence:
            errors.append(f"evidence missing: {row_id}")
            continue
        kinds: set[str] = set()
        for index, item in enumerate(evidence):
            if not isinstance(item, dict):
                errors.append(f"evidence object invalid: {row_id}/{index}")
                continue
            kind = item.get("kind")
            kinds.add(kind) if isinstance(kind, str) else None
            path = _inside_root(item.get("path"))
            lines = item.get("lines")
            if kind not in VALID_EVIDENCE_KIND or path is None or \
                    not re.fullmatch(r"[0-9a-f]{64}", str(item.get("sha256", ""))) or \
                    not isinstance(lines, dict) or not isinstance(lines.get("start"), int) or \
                    not isinstance(lines.get("end"), int) or lines["start"] < 1 or lines["end"] < lines["start"] or \
                    not isinstance(item.get("finding"), str) or not item["finding"].strip():
                errors.append(f"evidence metadata invalid: {row_id}/{index}")
                continue
            if check_files:
                if not path.is_file() or _sha(path) != item["sha256"]:
                    errors.append(f"evidence path/SHA mismatch: {row_id}/{item['path']}")
                    continue
                line_count = len(path.read_text(encoding="utf-8").splitlines())
                if lines["end"] > line_count:
                    errors.append(f"evidence line range invalid: {row_id}/{item['path']}")
        if status == "IMPLEMENTED" and not {"source_positive", "test_positive"}.issubset(kinds):
            errors.append(f"IMPLEMENTED row lacks positive source and test evidence: {row_id}")

    observed = Counter(row.get("status") for row in rows.values())
    computed_summary = {"total": len(rows), **{name.lower(): observed[name] for name in
        ("IMPLEMENTED", "PARTIAL", "MISSING", "PROCEDURAL_ONLY")}}
    if value.get("summary") != EXPECTED_COUNTS or computed_summary != EXPECTED_COUNTS:
        errors.append("v2 status counts must be exactly 8/12/13/7 over 40 rows")

    expected_unclosed = [row_id for row_id in EXPECTED_STATUS
                         if rows.get(row_id, {}).get("m4_required") and
                         rows[row_id].get("status") != "IMPLEMENTED"]
    if value.get("m4_required_unclosed") != expected_unclosed or len(expected_unclosed) != 17:
        errors.append("M4-required unclosed inventory differs")

    conflicts = value.get("policy_conflicts")
    expected_conflict = {"id": "off_table_12_3_vs_17_e", "rows": ["WPA-II-12-3", "WPA-II-17"],
                         "status": "policy_decision_required", "resolved": False,
                         "decision_owner": "external_rules_policy"}
    if conflicts != [expected_conflict] or not rows.get("WPA-II-12-3", {}).get("policy_decision_required") or \
            not rows.get("WPA-II-17", {}).get("policy_decision_required"):
        errors.append("section 12.3/17(e) policy conflict must remain unresolved and required")

    pending = value.get("classification_pending")
    expected_pending = [{"id": "section_6_g_no_rack_contact", "row": "WPA-II-06-G",
                         "status": "classification_pending",
                         "error_code": "rules_break_no_rack_contact_classification_pending",
                         "mutation_allowed": False}]
    row_6g = rows.get("WPA-II-06-G", {})
    if pending != expected_pending or row_6g.get("classification") != "pending" or \
            row_6g.get("classification_error") != "rules_break_no_rack_contact_classification_pending":
        errors.append("section 6(g) must remain classification_pending and fail closed")

    plan = value.get("next_deterministic_implementation_plan")
    if not isinstance(plan, list) or not plan:
        errors.append("next deterministic implementation plan missing")
    else:
        for index, item in enumerate(plan):
            if not isinstance(item, dict) or item.get("priority") not in {"P0", "P1", "P2"} or \
                    not isinstance(item.get("rows"), list) or not item["rows"] or \
                    not isinstance(item.get("depends_on"), list) or not item["depends_on"] or \
                    not isinstance(item.get("deliverable"), str) or not item["deliverable"].strip():
                errors.append(f"deterministic plan item invalid: {index}")
                continue
            for row_id in item["rows"]:
                if row_id not in rows or rows[row_id].get("policy_decision_required"):
                    errors.append(f"deterministic plan includes unknown/policy-required row: {row_id}")
    return errors


def validate_markdown(path: Path, value: dict) -> list[str]:
    if not path.is_file():
        return ["v2 markdown report missing"]
    text = path.read_text(encoding="utf-8")
    expected_sha = _sha(DEFAULT_AUDIT)
    errors = []
    if expected_sha not in text or "IMPLEMENTED: 8" not in text or "PARTIAL: 12" not in text or \
            "MISSING: 13" not in text or "PROCEDURAL_ONLY: 7" not in text:
        errors.append("markdown JSON SHA or counts differ")
    for row_id in EXPECTED_STATUS:
        if f"`{row_id}`" not in text:
            errors.append(f"markdown row missing: {row_id}")
    if "policy_decision_required" not in text or "classification_pending" not in text:
        errors.append("markdown policy/classification gates missing")
    return errors


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("audit", nargs="?", type=Path, default=DEFAULT_AUDIT)
    parser.add_argument("--skip-files", action="store_true")
    args = parser.parse_args()
    try:
        value = json.loads(args.audit.read_text(encoding="utf-8"))
    except (OSError, json.JSONDecodeError) as error:
        print(f"audit read failed: {error}", file=sys.stderr)
        return 2
    errors = validate_audit(value, check_files=not args.skip_files)
    if args.audit.resolve() == DEFAULT_AUDIT.resolve() and not args.skip_files:
        errors.extend(validate_markdown(DEFAULT_MARKDOWN, value))
    if errors:
        for error in errors:
            print(error, file=sys.stderr)
        return 1
    print("Phase-5 WPA Heyball as-built reconciliation audit v2: PASS")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
