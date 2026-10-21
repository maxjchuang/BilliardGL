#!/usr/bin/env python3
"""Generate the WPA II 10(4) narrow product-closure audit successor."""

from __future__ import annotations

from collections import Counter
import hashlib
import json
from pathlib import Path


ROOT = Path(__file__).resolve().parents[1]
V3_PATH = ROOT / "docs/rules/phase5-wpa-heyball-as-built-reconciliation-audit-v3.json"
JSON_PATH = ROOT / "docs/rules/phase5-wpa-heyball-as-built-reconciliation-audit-v4.json"
MD_PATH = ROOT / "docs/rules/phase5-wpa-heyball-as-built-reconciliation-audit-v4.md"


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
    value = json.loads(V3_PATH.read_text(encoding="utf-8"))
    value["schema"] = "phase5_wpa_heyball_as_built_reconciliation_audit_v4"
    value["schema_version"] = 4
    value["audit_id"] = "phase5_wpa_heyball_2025_08_16_as_built_reconciliation_v4"
    value["generated_on"] = "2026-08-12"
    value["audit_scope"] = "targeted_product_closure_wpa_ii_10_4"
    value["frozen_v3_predecessor"] = {
        "path": str(V3_PATH.relative_to(ROOT)),
        "sha256": sha256(V3_PATH),
    }
    value["wpa_ii_10_4_product_closure"] = {
        "row": "WPA-II-10-4",
        "entry_point": "ModernProductSession::submitProductAuthorizedGroupChoice",
        "allowed_actions": [
            "rules.group_choice.choose_solids",
            "rules.group_choice.choose_stripes",
        ],
        "requires_live_offered_action": True,
        "requires_match_screen": True,
        "requires_local_offered_actor": True,
        "ruleset_capability_flow_revision_and_state_hash_bound": True,
        "canonical_payload": "{}",
        "exact_retry_returns_original_ack": True,
        "conflicting_retry_fails_without_mutation": True,
        "foundation_actions_product_exposed": False,
        "other_rules_parent_actions_product_exposed_by_this_increment": False,
        "rules_decision_ui_product_exposed": False,
        "gpu_executed": False,
    }
    changed_paths = [
        "src/Billiards/rules/group_choice_authority.h",
        "src/Billiards/rules/rules_decision_ui_model.h",
        "src/Billiards/rules/rules_decision_ui_model.cpp",
        "src/Billiards/game_runtime.h",
        "src/Billiards/game_runtime.cpp",
        "src/Billiards/automation_controller.cpp",
        "src/Billiards/render/modern/modern_product_session.h",
        "src/Billiards/render/modern/modern_product_session.cpp",
        "tests/rules_runtime_integration_tests.cpp",
        "tests/rules_decision_session_integration_tests.cpp",
    ]
    value["wpa_ii_10_4_implementation_bindings"] = {
        "changed_files": [
            {"path": path, "sha256": sha256(ROOT / path)}
            for path in changed_paths
        ],
        "cpu_build_root": "build-wpa-10-4-product-group-choice-cpu-v1",
        "cpu_results": [
            {"target": "BilliardsRulesRuntimeIntegrationTests",
             "sha256": sha256(ROOT / "build-wpa-10-4-product-group-choice-cpu-v1/BilliardsRulesRuntimeIntegrationTests"),
             "exit_code": 0},
            {"target": "BilliardsRulesDecisionSessionIntegrationTests",
             "sha256": sha256(ROOT / "build-wpa-10-4-product-group-choice-cpu-v1/BilliardsRulesDecisionSessionIntegrationTests"),
             "exit_code": 0},
            {"target": "BilliardsAutomationControllerTests",
             "sha256": sha256(ROOT / "build-wpa-10-4-product-group-choice-cpu-v1/BilliardsAutomationControllerTests"),
             "exit_code": 0},
        ],
        "cmake_changed_by_increment": False,
        "gpu_cocoa_or_opengl_executed": False,
        "readiness_generated": False,
        "authorization_issued": False,
    }

    # Source bytes inherited by evidence may move between targeted successors;
    # refresh hashes without changing any non-target row's status or finding.
    for row in value["matrix"]:
        for item in row["evidence"]:
            path = ROOT / item["path"]
            if path.is_file():
                item["sha256"] = sha256(path)

    rows = {row["id"]: row for row in value["matrix"]}
    target = rows["WPA-II-10-4"]
    target.update({
        "status": "IMPLEMENTED",
        "product_reachable": True,
        "assessment": (
            "Core B stages the simultaneous two-group choice and hardened Core D "
            "validates ruleset/capability/flow, exact decision/revision/actor/action, "
            "canonical payload, frozen authority hash and idempotency. The shipping "
            "session exposes only the two actions in the current GroupChoice parent "
            "offer while the global Rules Decision UI and foundation actions remain hidden."
        ),
        "gaps": [],
        "evidence": [
            evidence("source_positive", "src/Billiards/rules/chinese_eight_ball_rules.cpp",
                     330, 390, "Core B stages a choice only when both legally contacted groups are pocketed simultaneously."),
            evidence("source_positive", "src/Billiards/rules/group_choice_authority.h",
                     8, 76, "Core D freezes the ruleset/capability/flow, actor, action, revision, payload and ACK schema."),
            evidence("source_positive", "src/Billiards/game_runtime.cpp",
                     1336, 1443, "Core D validates exact context and frozen authority hash before one atomic complementary-group commit."),
            evidence("source_positive", "src/Billiards/render/modern/modern_product_session.cpp",
                     241, 312, "The Match-only product seam accepts only a live offered solids/stripes action, verifies the ACK, then refreshes authority and AI."),
            evidence("test_positive", "tests/rules_runtime_integration_tests.cpp",
                     1700, 1885, "Headless Core D tests cover context, actor, action, stale revision/decision, hash drift, payload, retry, conflict and rollback."),
            evidence("test_positive", "tests/rules_decision_session_integration_tests.cpp",
                     266, 388, "Product tests cover outside-Match, wrong actor/action, state drift, exact commit, duplicate/conflict and hidden global UI."),
        ],
    })

    statuses = Counter(row["status"] for row in value["matrix"])
    value["summary"] = {
        "total": len(value["matrix"]),
        "implemented": statuses["IMPLEMENTED"],
        "partial": statuses["PARTIAL"],
        "missing": statuses["MISSING"],
        "procedural_only": statuses["PROCEDURAL_ONLY"],
    }
    value["m4_required_unclosed"] = [
        row["id"] for row in value["matrix"]
        if row["m4_required"] and row["status"] != "IMPLEMENTED"
    ]
    for plan in value["next_deterministic_implementation_plan"]:
        plan["rows"] = [row for row in plan["rows"]
                        if row != "WPA-II-10-4"]
    value["next_deterministic_implementation_plan"] = [
        plan for plan in value["next_deterministic_implementation_plan"]
        if plan["rows"]
    ]
    value["completed_targeted_increments"].append({
        "priority": "P0",
        "rows": ["WPA-II-10-4"],
        "result": (
            "The current offered simultaneous group-choice parent action is product "
            "reachable through a fail-closed narrow seam with CPU evidence."
        ),
    })
    return value


def markdown(value: dict) -> str:
    row = next(item for item in value["matrix"]
               if item["id"] == "WPA-II-10-4")
    return "\n".join([
        "# WPA Heyball as-built audit v4",
        "",
        "Targeted successor for WPA-II-10-4. The v3 audit remains frozen.",
        "",
        f"- Status: `{row['status']}`",
        f"- Product reachable: `{str(row['product_reachable']).lower()}`",
        "- Product surface: current offered solids/stripes GroupChoice parent action only",
        "- Global Rules Decision UI exposed: `false`",
        "- Foundation actions exposed: `false`",
        "- GPU executed: `false`",
        "",
        row["assessment"],
        "",
    ])


def main() -> int:
    value = build()
    JSON_PATH.write_text(json.dumps(value, indent=2, ensure_ascii=False) + "\n",
                         encoding="utf-8")
    MD_PATH.write_text(markdown(value), encoding="utf-8")
    print(json.dumps({"json": str(JSON_PATH), "sha256": sha256(JSON_PATH),
                      "summary": value["summary"]}, sort_keys=True))
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
