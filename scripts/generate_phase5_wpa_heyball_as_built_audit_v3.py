#!/usr/bin/env python3
"""Generate the targeted WPA II §4/§7 product-closure audit successor."""

from __future__ import annotations

from collections import Counter
import hashlib
import json
from pathlib import Path


ROOT = Path(__file__).resolve().parents[1]
V2_PATH = ROOT / "docs/rules/phase5-wpa-heyball-as-built-reconciliation-audit-v2.json"
GATE_PATH = ROOT / "docs/rules/wpa-ii-04-official-rack-product-migration-gate-v1.json"
JSON_PATH = ROOT / "docs/rules/phase5-wpa-heyball-as-built-reconciliation-audit-v3.json"
MD_PATH = ROOT / "docs/rules/phase5-wpa-heyball-as-built-reconciliation-audit-v3.md"


def sha256(path: Path) -> str:
    return hashlib.sha256(path.read_bytes()).hexdigest()


def evidence(kind: str, path_text: str, start: int, end: int,
             finding: str) -> dict:
    return {
        "kind": kind,
        "path": path_text,
        "sha256": sha256(ROOT / path_text),
        "lines": {"start": start, "end": end},
        "finding": finding,
    }


def build() -> dict:
    value = json.loads(V2_PATH.read_text(encoding="utf-8"))
    value["schema"] = "phase5_wpa_heyball_as_built_reconciliation_audit_v3"
    value["schema_version"] = 3
    value["audit_id"] = "phase5_wpa_heyball_2025_08_16_as_built_reconciliation_v3"
    value["audit_scope"] = "targeted_product_closure_wpa_ii_04_and_07"
    value["frozen_v2_predecessor"] = {
        "path": str(V2_PATH.relative_to(ROOT)),
        "sha256": sha256(V2_PATH),
    }
    value["official_rack_migration_gate"] = {
        "path": str(GATE_PATH.relative_to(ROOT)),
        "sha256": sha256(GATE_PATH),
        "status": "APPROVED_NO_GPU_PRODUCT_LAYOUT_MIGRATION_REQUIRED",
    }
    value["targeted_product_closure"] = {
        "rows": ["WPA-II-04", "WPA-II-07"],
        "foundation_actions_product_exposed": False,
        "rules_decision_ui_product_exposed": False,
        "seeded_numbered_ball_order_normative": False,
        "historical_phase2_phase4_phase5_visual_evidence_inheritable": False,
    }
    value["downstream_binding_impact"] = {
        "phase2_visual_readiness": "FRESH_SOURCE_BINARY_BINDINGS_REQUIRED",
        "phase4_probe_digest": "NOT_RECOMPUTED_NO_INHERITANCE_CLAIM",
        "phase4_readiness": "FRESH_SOURCE_BINARY_BINDINGS_REQUIRED_IF_PRODUCT_LIFECYCLE_IS_LINKED",
        "phase5_machine_inputs": "SOURCE_BINDING_DRIFT_FRESH_MACHINE_AND_READINESS_REQUIRED",
        "phase5_prior_modern_product_session_sha256":
            "f5493e5f9b610312180f57bc60a624167c55e82627c08ad572539e6f6d91d74b",
        "phase5_successor_modern_product_session_sha256":
            sha256(ROOT / "src/Billiards/render/modern/modern_product_session.cpp"),
        "successor_product_shell_controller_sha256":
            sha256(ROOT / "src/Billiards/product_shell_controller.cpp"),
        "old_visual_or_machine_evidence_modified": False,
        "gpu_executed": False,
    }

    # Refresh inherited evidence SHA bindings without changing any other row's
    # status/product conclusion. The targeted product integration necessarily
    # changes ModernProductSession bytes referenced by several frozen v2 rows.
    for row in value["matrix"]:
        for item in row["evidence"]:
            path = ROOT / item["path"]
            item["sha256"] = sha256(path)
            if item["path"] == "src/Billiards/render/modern/modern_product_session.cpp" and \
                    item["kind"] == "product_gate":
                item["lines"] = {"start": 691, "end": 718}
                item["finding"] = (
                    "The production session still explicitly keeps the global "
                    "Rules Decision UI product exposure false.")

    rows = {row["id"]: row for row in value["matrix"]}
    row04 = rows["WPA-II-04"]
    row04.update({
        "status": "IMPLEMENTED",
        "product_reachable": True,
        "assessment": (
            "The standard ProductShell Match lifecycle commits the fixed-seed "
            "Core E official rack exactly once before AI configuration and Match "
            "visibility, with atomic cleanup on failure. WPA geometry/group "
            "constraints are normative; seeded numbered-ball order remains "
            "explicitly non-normative."),
        "gaps": [],
        "evidence": [
            evidence("source_positive", "src/Billiards/rules/rack_executor.cpp",
                     153, 355, "Core E enforces the official rack geometry/group contract while marking seeded number order non-normative."),
            evidence("source_positive", "src/Billiards/product_shell_controller.cpp",
                     218, 280, "The standard product Match start stages and commits one fixed official rack before AI configuration and Match visibility, with busy preflight and cleanup on every failure."),
            evidence("test_positive", "tests/rules_runtime_integration_tests.cpp",
                     192, 568, "Core E tests independently verify official geometry, group constraints, transaction integrity and seeded-order semantics."),
            evidence("test_positive", "tests/product_shell_tests.cpp",
                     262, 402, "Product tests require exact ball-ID/position contract bytes, one commit, restart determinism, busy preflight and AI/runtime/shell rollback."),
            evidence("migration_gate", str(GATE_PATH.relative_to(ROOT)), 1, 83,
                     "The approved migration records the noncompliant predecessor layout and forbids inheritance of affected historical visual evidence."),
        ],
    })
    row07 = rows["WPA-II-07"]
    row07.update({
        "status": "IMPLEMENTED",
        "product_reachable": True,
        "assessment": (
            "ModernProductSession exposes a narrow product-authorized seam for "
            "only the spot actions in the current 8-on-break parent offer. It "
            "binds local actor, authority revision, action and canonical payload "
            "before GameRuntime performs the Core E respot; foundation and rerack "
            "IDs remain unreachable and the global Rules Decision UI stays hidden."),
        "gaps": [],
        "evidence": [
            evidence("source_positive", "src/Billiards/game_runtime.cpp", 1818, 1911,
                     "The authoritative 8-on-break parent decision builds and applies Core E respot before recording the exact parent action ACK."),
            evidence("source_positive", "src/Billiards/render/modern/modern_product_session.h",
                     100, 122, "The shipping session declares only a narrow 8-on-break respot parent seam, not a foundation executor surface."),
            evidence("source_positive", "src/Billiards/render/modern/modern_product_session.cpp",
                     220, 263, "The product seam accepts only offered spot actions and binds exact actor, revision, context and canonical payload."),
            evidence("test_positive", "tests/rules_runtime_integration_tests.cpp",
                     460, 568, "Core E tests cover nominal, longitudinal fallback, no-solution and stale-state respot behavior."),
            evidence("test_positive", "tests/rules_decision_session_integration_tests.cpp",
                     155, 264, "Product tests cover exact parent commit plus outside-Match, foundation, rerack, malformed-key and actor mismatch rejection with zero mutation."),
            evidence("migration_gate", str(GATE_PATH.relative_to(ROOT)), 1, 83,
                     "The approved boundary keeps foundation actions and global Rules UI product exposure disabled."),
        ],
    })

    counts = Counter(row["status"] for row in value["matrix"])
    value["summary"] = {"total": len(value["matrix"]), **{
        status.lower(): counts[status] for status in
        ("IMPLEMENTED", "PARTIAL", "MISSING", "PROCEDURAL_ONLY")}}
    value["m4_required_unclosed"] = [
        row["id"] for row in value["matrix"]
        if row["m4_required"] and row["status"] != "IMPLEMENTED"]
    value["next_deterministic_implementation_plan"] = [
        item for item in value["next_deterministic_implementation_plan"]
        if item.get("rows") != ["WPA-II-04", "WPA-II-07"]]
    value["completed_targeted_increments"] = [{
        "priority": "P0",
        "rows": ["WPA-II-04", "WPA-II-07"],
        "result": "Official product rack lifecycle and narrow parent-authorized respot are product reachable with CPU evidence.",
    }]
    return value


def markdown(value: dict, digest: str) -> str:
    rows = {row["id"]: row for row in value["matrix"]}
    summary = value["summary"]
    return "\n".join([
        "# Phase 5 WPA Heyball as-built reconciliation audit v3", "",
        f"JSON SHA-256: `{digest}`.", "",
        "This targeted successor closes only WPA-II-04 and WPA-II-07. The complete profile and global Rules Decision UI remain unexposed.", "",
        "## Result", "",
        f"- IMPLEMENTED: {summary['implemented']}",
        f"- PARTIAL: {summary['partial']}",
        f"- MISSING: {summary['missing']}",
        f"- PROCEDURAL_ONLY: {summary['procedural_only']}",
        "- Profile complete: `false`", "- Product exposed: `false`", "",
        "## Targeted rows", "",
        f"- `WPA-II-04` — **{rows['WPA-II-04']['status']}**: {rows['WPA-II-04']['assessment']}",
        f"- `WPA-II-07` — **{rows['WPA-II-07']['status']}**: {rows['WPA-II-07']['assessment']}", "",
        "The prior Phase2/4/5 visual evidence remains historical only after the approved official-rack layout migration. Fresh readiness must bind successor sources/binaries.", "",
    ])


def main() -> None:
    value = build()
    encoded = (json.dumps(value, indent=2, ensure_ascii=False) + "\n").encode("utf-8")
    JSON_PATH.write_bytes(encoded)
    MD_PATH.write_text(markdown(value, hashlib.sha256(encoded).hexdigest()), encoding="utf-8")
    print(JSON_PATH.relative_to(ROOT))
    print(MD_PATH.relative_to(ROOT))


if __name__ == "__main__":
    main()
