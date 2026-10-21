#!/usr/bin/env python3
"""Generate the WPA II §§8/9 product-placement subflow audit successor."""

from __future__ import annotations

from collections import Counter
import hashlib
import json
from pathlib import Path


ROOT = Path(__file__).resolve().parents[1]
V4_PATH = ROOT / "docs/rules/phase5-wpa-heyball-as-built-reconciliation-audit-v4.json"
JSON_PATH = ROOT / "docs/rules/phase5-wpa-heyball-as-built-reconciliation-audit-v5.json"
MD_PATH = ROOT / "docs/rules/phase5-wpa-heyball-as-built-reconciliation-audit-v5.md"


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
    value = json.loads(V4_PATH.read_text(encoding="utf-8"))
    value["schema"] = "phase5_wpa_heyball_as_built_reconciliation_audit_v5"
    value["schema_version"] = 5
    value["audit_id"] = "phase5_wpa_heyball_2025_08_16_as_built_reconciliation_v5"
    value["generated_on"] = "2026-08-12"
    value["audit_scope"] = "targeted_product_subflow_wpa_ii_08_09"
    value["frozen_v4_predecessor"] = {
        "path": str(V4_PATH.relative_to(ROOT)),
        "sha256": sha256(V4_PATH),
    }
    value["wpa_ii_08_09_product_placement_subflow"] = {
        "rows": ["WPA-II-08", "WPA-II-09"],
        "entry_points": [
            "ModernProductSession::previewProductAuthorizedBallInHandPlacement",
            "ModernProductSession::confirmProductAuthorizedBallInHandPlacement",
        ],
        "allowed_actions": {
            "BehindHeadString": "rules.ball_in_hand.confirm_behind_head_string",
            "Anywhere": "rules.ball_in_hand.confirm_full_table_placement",
        },
        "requires_match_screen": True,
        "requires_current_live_offer": True,
        "requires_local_offered_actor": True,
        "requires_current_legal_preview": True,
        "preview_authority_mutation": False,
        "ruleset_capability_flow_decision_revision_state_hash_bound": True,
        "position_payload_is_authoritatively_revalidated": True,
        "exact_retry_returns_original_ack": True,
        "conflicting_retry_fails_without_mutation": True,
        "foundation_or_other_rules_actions_product_exposed": False,
        "rules_decision_ui_product_exposed": False,
        "product_subflow_reachable": True,
        "complete_clause_product_reachable": False,
        "row_status_promotions": [],
        "unresolved_clause_semantics": {
            "WPA-II-08": [
                "all_legal_object_balls_behind_string_respot_product_transaction",
                "post_placement_first_contact_product_lifecycle",
            ],
            "WPA-II-09": [
                "cue_tip_touch_during_placement_classification",
                "continued_move_until_legal_shot_product_lifecycle",
            ],
        },
        "gpu_executed": False,
    }

    bound_paths = [
        "src/Billiards/rules/ball_in_hand_authority.h",
        "src/Billiards/game_runtime.h",
        "src/Billiards/game_runtime.cpp",
        "src/Billiards/render/modern/modern_product_session.h",
        "src/Billiards/render/modern/modern_product_session.cpp",
        "tests/rules_runtime_integration_tests.cpp",
        "tests/rules_decision_session_integration_tests.cpp",
    ]
    build_root = ROOT / "build-wpa-08-09-product-placement-cpu-v1/fresh"
    cpu_targets = [
        "BilliardsRulesRuntimeIntegrationTests",
        "BilliardsRulesDecisionSessionIntegrationTests",
        "BilliardsAutomationControllerTests",
    ]
    value["wpa_ii_08_09_implementation_bindings"] = {
        "bound_files": [
            {"path": path, "sha256": sha256(ROOT / path)}
            for path in bound_paths
        ],
        "production_files_changed_by_increment": [
            "src/Billiards/render/modern/modern_product_session.h",
            "src/Billiards/render/modern/modern_product_session.cpp",
        ],
        "test_files_changed_by_increment": [
            "tests/rules_decision_session_integration_tests.cpp",
        ],
        "cpu_build_root": str(build_root.relative_to(ROOT)),
        "cpu_results": [
            {"target": target, "sha256": sha256(build_root / target),
             "exit_code": 0}
            for target in cpu_targets
        ],
        "cmake_changed_by_increment": False,
        "gpu_cocoa_or_opengl_executed": False,
        "readiness_generated": False,
        "authorization_issued": False,
    }

    # Evidence source bytes can move between targeted successors. Refresh only
    # hashes here; semantic fields remain predecessor-locked below.
    for row in value["matrix"]:
        for item in row["evidence"]:
            path = ROOT / item["path"]
            if path.is_file():
                item["sha256"] = sha256(path)

    rows = {row["id"]: row for row in value["matrix"]}
    rows["WPA-II-08"].update({
        "status": "PARTIAL",
        "product_reachable": False,
        "assessment": (
            "Core G and the shipping session now provide a narrow, current-offer-only "
            "BehindHeadString preview/confirm transaction. Preview is read-only and "
            "confirm is actor/revision/state-hash/idempotency bound. The complete clause "
            "remains non-product-reachable because the all-legal-balls-behind re-spot "
            "transaction and the post-placement first-contact lifecycle are not closed."
        ),
        "gaps": [
            "Implement and product-authorize the all-legal-object-balls-behind-string re-spot transaction.",
            "Close the post-placement first-contact restriction lifecycle without inferring missing policy.",
        ],
        "evidence": [
            evidence("source_positive", "src/Billiards/rules/ball_in_hand_authority.h",
                     8, 76, "Core G defines the exact BehindHeadString action, flow, scope, actor, revision and position request schema."),
            evidence("source_positive", "src/Billiards/game_runtime.cpp",
                     1449, 1649, "Core G validates head-string geometry and atomically commits one frozen-hash placement request."),
            evidence("source_positive", "src/Billiards/render/modern/modern_product_session.cpp",
                     334, 465, "The shipping seam accepts only the current legal preview and exact offered scoped action while the global Rules UI stays hidden."),
            evidence("test_positive", "tests/rules_runtime_integration_tests.cpp",
                     1920, 2475, "Core G CPU tests cover full-table and BehindHeadString context, geometry, stale state, retry, conflict and rollback."),
            evidence("test_positive", "tests/rules_decision_session_integration_tests.cpp",
                     567, 652, "Product CPU tests cover wrong actor/half/action, state drift and one exact BehindHeadString commit."),
        ],
    })
    rows["WPA-II-09"].update({
        "status": "PARTIAL",
        "product_reachable": False,
        "assessment": (
            "Core G and the shipping session now provide a narrow, current-offer-only "
            "Anywhere preview/confirm transaction for ordinary ball in hand. Preview is "
            "read-only and confirm is actor/revision/state-hash/idempotency bound. The "
            "complete clause remains non-product-reachable because cue-tip contact while "
            "placing and continued movement until a legal shot are not classified end to end."
        ),
        "gaps": [
            "Define and implement the authoritative cue-tip-touch classification while placement remains active.",
            "Close the product lifecycle for continued cue-ball adjustment until the legal shot boundary.",
        ],
        "evidence": [
            evidence("source_positive", "src/Billiards/rules/ball_in_hand_authority.h",
                     8, 76, "Core G defines the exact full-table action, flow, actor, revision and position request schema."),
            evidence("source_positive", "src/Billiards/game_runtime.cpp",
                     1449, 1649, "Core G validates table, pocket, jaw and overlap geometry and atomically commits one frozen-hash placement request."),
            evidence("source_positive", "src/Billiards/render/modern/modern_product_session.cpp",
                     334, 465, "The shipping seam requires an exact legal preview before the offered Anywhere confirm can enter Core G."),
            evidence("test_positive", "tests/rules_runtime_integration_tests.cpp",
                     1920, 2140, "Core G CPU tests cover full-table placement context, invalid geometry, atomic commit, retry and conflict."),
            evidence("test_positive", "tests/rules_decision_session_integration_tests.cpp",
                     444, 564, "Product CPU tests prove preview read-only behavior, Match/actor/action/idempotency binding, exact ACK and hidden global UI."),
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
    value["completed_product_subincrements"] = [
        {
            "priority": "P0",
            "rows": ["WPA-II-08", "WPA-II-09"],
            "result": "Core-G BehindHeadString/Anywhere placement preview and confirm are product reachable through a fail-closed narrow seam.",
            "rows_remain_partial": True,
        }
    ]
    for plan in value["next_deterministic_implementation_plan"]:
        plan["rows"] = [row for row in plan["rows"] if row not in {
            "WPA-II-08", "WPA-II-09"}]
    value["next_deterministic_implementation_plan"] = [
        plan for plan in value["next_deterministic_implementation_plan"]
        if plan["rows"]
    ]
    value["next_deterministic_implementation_plan"].append({
        "priority": "P1",
        "rows": ["WPA-II-08", "WPA-II-09"],
        "depends_on": [
            "all_behind_string_respot_authority",
            "post_placement_first_contact_lifecycle",
            "cue_tip_touch_placement_classification",
            "until_legal_shot_adjustment_lifecycle",
        ],
        "deliverable": (
            "Close the remaining official clause semantics; do not infer them "
            "from the completed Core-G placement subflow."
        ),
    })
    return value


def markdown(value: dict) -> str:
    return "\n".join([
        "# WPA Heyball as-built audit v5",
        "",
        "Targeted product-placement subflow successor for WPA-II-08/09. The v4 audit remains frozen.",
        "",
        "- Core-G placement subflow product reachable: `true`",
        "- WPA-II-08 complete row: `PARTIAL` / product reachable `false`",
        "- WPA-II-09 complete row: `PARTIAL` / product reachable `false`",
        "- Global Rules Decision UI exposed: `false`",
        "- GPU executed: `false`",
        "",
        "The remaining all-behind re-spot, post-placement first-contact, cue-tip-touch and until-legal-shot semantics are not inferred or promoted.",
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
