#!/usr/bin/env python3
"""Generate the Phase-5 WPA Heyball as-built reconciliation audit v2."""

from __future__ import annotations

from collections import Counter
from datetime import date
import hashlib
import json
from pathlib import Path


ROOT = Path(__file__).resolve().parents[1]
V1_PATH = ROOT / "docs/rules/phase5-wpa-heyball-completeness-audit.json"
JSON_PATH = ROOT / "docs/rules/phase5-wpa-heyball-as-built-reconciliation-audit-v2.json"
MD_PATH = ROOT / "docs/rules/phase5-wpa-heyball-as-built-reconciliation-audit-v2.md"

V1_FILES = {
    "docs/rules/phase5-wpa-heyball-completeness-audit.json":
        "c70e8662dd523b04eefab5745c4cd9023c2ae9dfa374f1b289d684d532ea8555",
    "docs/rules/phase5-wpa-heyball-completeness-audit.md":
        "f4384c54a245184c281697d6e18562cf1d8a656856f07174d590e351f2b1fd04",
    "docs/rules/phase5-wpa-heyball-completeness-audit.schema.json":
        "6694e31bed405fa8c3258b9103110f9b4ff6aa7bbb5da1d9946183510471529d",
    "scripts/validate_phase5_wpa_heyball_completeness_audit.py":
        "735c70bea8adf3529707cf994d52a4a8b74495492b7244fe93baf9dfae660837",
    "tests/visual/test_phase5_wpa_heyball_completeness_audit.py":
        "ee2db00a4580084fc9466574b2e5e8a495062bf6a3a846abecfa1c27eb3f9184",
}

STATUS = {
    "WPA-I-05": "PROCEDURAL_ONLY",
    "WPA-II-01": "PROCEDURAL_ONLY",
    "WPA-II-02": "IMPLEMENTED",
    "WPA-II-04": "PARTIAL",
    "WPA-II-05": "MISSING",
    "WPA-II-06-A-B": "MISSING",
    "WPA-II-06-C": "PARTIAL",
    "WPA-II-06-D": "PARTIAL",
    "WPA-II-06-E": "PARTIAL",
    "WPA-II-06-F": "PARTIAL",
    "WPA-II-06-G": "MISSING",
    "WPA-II-07": "PARTIAL",
    "WPA-II-08": "PARTIAL",
    "WPA-II-09": "PARTIAL",
    "WPA-II-10-1": "IMPLEMENTED",
    "WPA-II-10-2": "PARTIAL",
    "WPA-II-10-3": "IMPLEMENTED",
    "WPA-II-10-4": "PARTIAL",
    "WPA-II-11": "IMPLEMENTED",
    "WPA-II-12-1": "IMPLEMENTED",
    "WPA-II-12-2": "IMPLEMENTED",
    "WPA-II-12-3": "PARTIAL",
    "WPA-II-12-4": "MISSING",
    "WPA-II-12-5-6": "MISSING",
    "WPA-II-12-7": "MISSING",
    "WPA-II-12-8": "MISSING",
    "WPA-II-12-9": "PROCEDURAL_ONLY",
    "WPA-II-13": "IMPLEMENTED",
    "WPA-II-14": "PROCEDURAL_ONLY",
    "WPA-II-15-1": "MISSING",
    "WPA-II-15-2": "MISSING",
    "WPA-II-16": "MISSING",
    "WPA-II-17": "PARTIAL",
    "WPA-II-18": "MISSING",
    "WPA-II-19": "MISSING",
    "WPA-II-20": "IMPLEMENTED",
    "WPA-II-21": "MISSING",
    "WPA-II-22": "PROCEDURAL_ONLY",
    "WPA-II-23-24": "PROCEDURAL_ONLY",
    "WPA-III-01-08": "PROCEDURAL_ONLY",
}

CORE = {
    "WPA-II-02": ["A"], "WPA-II-04": ["E"],
    "WPA-II-06-C": ["H"], "WPA-II-06-E": ["H", "G"],
    "WPA-II-06-F": ["F", "G", "E"], "WPA-II-06-G": ["H"],
    "WPA-II-07": ["E"], "WPA-II-08": ["G", "E"],
    "WPA-II-09": ["D"], "WPA-II-10-1": ["A", "B"],
    "WPA-II-10-2": ["A"], "WPA-II-10-3": ["A"],
    "WPA-II-10-4": ["B", "C"], "WPA-II-12-1": ["A", "B"],
    "WPA-II-12-2": ["A", "B"], "WPA-II-12-3": ["A"],
    "WPA-II-13": ["B"], "WPA-II-17": ["A", "D", "G"],
    "WPA-II-20": ["A"],
}

ASSESSMENT = {
    "WPA-I-05": "Frozen-ball declaration remains a referee observation; neither ShotFacts nor the foul model carries a declaration state.",
    "WPA-II-01": "Player knowledge and schedule responsibility is event procedure, not gameplay automation.",
    "WPA-II-02": "Core A ends the rack only after assigned-group clearance and a legal 8-ball; early, foul, same-shot and off-table 8 outcomes lose.",
    "WPA-II-04": "Core E builds and commits official rack geometry, but seeded number order is explicitly non-official and the foundation is not the product rack lifecycle.",
    "WPA-II-05": "No lag facts, bad-lag/tie state, re-lag action or test exists.",
    "WPA-II-06-A-B": "No soft-break judgment, warning history, intentional no-break/miscue classification or escalating rack/match penalty exists.",
    "WPA-II-06-C": "Objective legal-break adjudication and opponent accept/rerack authority exist, but the product decision gate remains closed.",
    "WPA-II-06-D": "Break cue-ball geometry is seeded behind the head string, but no half-ball crossing measurement or reminder/offense escalation exists.",
    "WPA-II-06-E": "Core H offers the three official opponent choices and Core G handles head-string placement, but product actions remain gated off.",
    "WPA-II-06-F": "Core F implements spot-or-rerack authority and Core E commits effects, but the choice is not product exposed.",
    "WPA-II-06-G": "Generic no-contact cannot distinguish an unintentional miscue from other causes; authority rejects with classification_pending and mutates nothing.",
    "WPA-II-07": "Core E searches the nominal spot then the longitudinal axis and commits atomically, but it is a non-product foundation surface.",
    "WPA-II-08": "Core G enforces behind-head-string placement and authority freshness; the full first-contact restriction/re-spot workflow is not product complete.",
    "WPA-II-09": "Core D validates and commits full-table cue-ball placement, but product placement actions remain gated off.",
    "WPA-II-10-1": "Open-table first-contact logic accepts any non-8 contact, rejects 8-only contact, and retains open-table state after the break.",
    "WPA-II-10-2": "Runtime preserves pocketed non-8 balls, but off-table treatment is entangled with the unresolved section 12.3 versus 17(e) policy conflict.",
    "WPA-II-10-3": "The break never assigns groups; the first legal post-break group pocket assigns shooter and opponent groups.",
    "WPA-II-10-4": "Core B stages simultaneous two-group choice and Core C commits a revision-bound selection, but no product action is exposed.",
    "WPA-II-11": "The default ruleset requires no called ball or pocket, including for the 8-ball.",
    "WPA-II-12-1": "Open-table assignment and assigned-group/cleared-group first-contact rules are enforced by the adjudicator.",
    "WPA-II-12-2": "Collector records cushion contact only from the first object-contact tick onward; adjudicator requires a pocket or such a cushion contact.",
    "WPA-II-12-3": "Non-8 off-table currently emits a foul, while official 12.3 and 17(e) conflict; this remains policy_decision_required.",
    "WPA-II-12-4": "ShotFacts has no body, clothing, hair or equipment contact signal.",
    "WPA-II-12-5-6": "ShotFacts has no cue-tip contact count, close-ball separation or double-hit classification.",
    "WPA-II-12-7": "ShotFacts has no cue-tip contact-duration or push-shot signal.",
    "WPA-II-12-8": "No cushion-top trajectory/contact classification exists; referee policy remains required.",
    "WPA-II-12-9": "Handicapped-ball removal and player reminder are event/referee procedure.",
    "WPA-II-13": "Core B captures the same-tick first-contact set and presumes the shot legal whenever that set contains a legal ball.",
    "WPA-II-14": "Self-settling, three-second pocket validity, restoration and replay remain referee procedure.",
    "WPA-II-15-1": "No frozen cue/object declaration or frozen-contact double-hit/push presumption is represented.",
    "WPA-II-15-2": "No frozen-object leave/re-contact or alternative-cushion predicate is represented.",
    "WPA-II-16": "No upper/lower cue-ball strike, scoop, obstacle-overflight or miscue jump classification exists.",
    "WPA-II-17": "Scratch/off-table, wrong-first and no-contact/no-rail subsets award ball in hand; the remaining general-foul inventory and 17(e) policy are incomplete.",
    "WPA-II-18": "No intentional-foul classifier or rack/match escalation history exists.",
    "WPA-II-19": "No wrong-group discovery window, stop-rack action or corrective rerack transaction exists.",
    "WPA-II-20": "Core A covers foul with 8, same-shot last group plus 8, 8 off table, early 8 and legal 8 win as mutually exclusive outcomes.",
    "WPA-II-21": "No stalemate proposal/refusal state, three-shot counter or original-breaker rerack workflow exists.",
    "WPA-II-22": "Ball restoration/replay and unrecoverable-position judgment remain referee procedure.",
    "WPA-II-23-24": "Concession and conduct penalties remain referee/event discretion.",
    "WPA-III-01-08": "Optional clocks, shootout, timeout, dispute and conduct variant are outside the current gameplay product profile.",
}

GAPS = {
    "WPA-II-04": ["Integrate Core E through the standard product rack lifecycle.", "Document seeded number order as non-normative while enforcing every official rack constraint."],
    "WPA-II-06-C": ["Enable the verified Rules Decision product capability for accept/rerack choices."],
    "WPA-II-06-D": ["Add half-ball head-string crossing telemetry and per-player reminder/offense history."],
    "WPA-II-06-E": ["Enable the verified product choice and head-string placement flows."],
    "WPA-II-06-F": ["Enable the verified product spot/rerack flow."],
    "WPA-II-06-G": ["Add authoritative intentional/no-intentional and miscue classification before offering a re-break."],
    "WPA-II-07": ["Route respot through a product-authorized parent decision rather than exposing the foundation executor."],
    "WPA-II-08": ["Complete the first-contact restriction and all-behind-string re-spot path; expose the placement flow."],
    "WPA-II-09": ["Enable the verified full-table placement flow in product."],
    "WPA-II-10-2": ["Resolve the section 12.3 versus 17(e) off-table policy before claiming complete behavior."],
    "WPA-II-10-4": ["Enable the verified group-choice action in product."],
    "WPA-II-12-3": ["Keep policy_decision_required until the official conflict receives an explicit ruling."],
    "WPA-II-17": ["Add the unrepresented general-foul facts/actions and preserve the 17(e) policy gate."],
}

for row_id, status in STATUS.items():
    if row_id not in GAPS and status in {"MISSING", "PROCEDURAL_ONLY"}:
        GAPS[row_id] = [ASSESSMENT[row_id]]


def sha256(path: Path) -> str:
    return hashlib.sha256(path.read_bytes()).hexdigest()


def evidence(key: str) -> dict:
    specs = {
        "fact_model": ("src/Billiards/rules/chinese_eight_ball_rules.h", 22, 71, "source_boundary", "The authoritative foul and ShotFacts model enumerates the currently representable signals."),
        "ruleset": ("src/Billiards/rules/chinese_eight_ball_rules.h", 34, 71, "source_positive", "Ruleset defaults and state/fact fields define the no-call profile."),
        "adjudicator": ("src/Billiards/rules/chinese_eight_ball_rules.cpp", 175, 351, "source_positive", "The authoritative adjudicator implements break, contact, 8-ball, assignment, continuation and ball-in-hand outcomes."),
        "collector": ("src/Billiards/rules/shot_fact_collector.cpp", 19, 93, "source_positive", "The runtime collector derives canonical first-contact, rail, pocket and off-table facts from physics telemetry."),
        "rack_source": ("src/Billiards/rules/rack_executor.cpp", 153, 355, "source_positive", "Core E builds official rack geometry and deterministic rerack/respot foundation plans."),
        "group_authority": ("src/Billiards/game_runtime.cpp", 1309, 1490, "source_positive", "Core C validates and atomically commits revision-bound group-choice authority."),
        "bih_authority": ("src/Billiards/game_runtime.cpp", 1491, 1612, "source_positive", "Core D/G validates and atomically commits full-table or head-string cue-ball placement."),
        "break_authority": ("src/Billiards/game_runtime.cpp", 1613, 1754, "source_positive", "Core H validates and commits the break-foul opponent choice transaction."),
        "eight_authority": ("src/Billiards/game_runtime.cpp", 1755, 1912, "source_positive", "Core F validates and commits 8-on-break spot/rerack authority through Core E."),
        "product_gate": ("src/Billiards/render/modern/modern_product_session.cpp", 655, 674, "product_gate", "The production session snapshot explicitly keeps rulesDecisionUiProductExposed false."),
        "rules_tests": ("tests/chinese_eight_ball_rules_tests.cpp", 43, 190, "test_positive", "Scenario tests cover break, groups, contact, rail, scratch and legal/illegal 8 outcomes."),
        "config_tests": ("tests/chinese_eight_ball_rules_tests.cpp", 391, 418, "test_positive", "Configuration tests prove the default no-call behavior and explicit called-pocket variant boundary."),
        "simultaneous_tests": ("tests/chinese_eight_ball_rules_tests.cpp", 307, 389, "test_positive", "Rules tests cover legal-first presumption and simultaneous two-group choice staging."),
        "collector_tests": ("tests/shot_fact_collector_tests.cpp", 52, 165, "test_positive", "Collector tests prove after-contact rail semantics and canonical same-tick first-contact sets."),
        "rack_tests": ("tests/rules_runtime_integration_tests.cpp", 192, 568, "test_positive", "Runtime tests cover official rack constraints and atomic rerack/respot foundation transactions."),
        "eight_tests": ("tests/rules_runtime_integration_tests.cpp", 569, 680, "test_positive", "Runtime tests cover 8-on-break authority and explicitly assert Product HUD remains hidden."),
        "break_tests": ("tests/rules_runtime_integration_tests.cpp", 1027, 1130, "test_positive", "Runtime tests cover break-foul choices, blocking and the 6(g) classification-pending rejection."),
        "core_a_tests": ("tests/rules_runtime_integration_tests.cpp", 1284, 1392, "test_positive", "Core A integration tests cover corrected 8-ball, no-rail and off-table outcomes."),
        "group_tests": ("tests/rules_runtime_integration_tests.cpp", 1393, 1699, "test_positive", "Core B/C tests cover simultaneous contact and revision-bound group choice while product remains hidden."),
        "full_bih_tests": ("tests/rules_runtime_integration_tests.cpp", 1700, 1912, "test_positive", "Core D tests cover full-table placement authority and fail-closed transaction behavior."),
        "head_bih_tests": ("tests/rules_runtime_integration_tests.cpp", 2040, 2072, "test_positive", "Core G tests cover head-string placement and explicitly assert Product HUD remains hidden."),
        "test_boundary": ("tests/chinese_eight_ball_rules_tests.cpp", 423, 430, "test_boundary", "The rule-core test entry point contains no clause-specific scenario for this missing/procedural requirement."),
    }
    path, start, end, kind, finding = specs[key]
    absolute = ROOT / path
    return {"kind": kind, "path": path, "sha256": sha256(absolute),
            "lines": {"start": start, "end": end}, "finding": finding}


EVIDENCE_KEYS = {
    "WPA-I-05": ["fact_model", "test_boundary"],
    "WPA-II-01": ["fact_model", "test_boundary"],
    "WPA-II-02": ["adjudicator", "rules_tests", "core_a_tests"],
    "WPA-II-04": ["rack_source", "rack_tests", "product_gate"],
    "WPA-II-05": ["fact_model", "test_boundary"],
    "WPA-II-06-A-B": ["fact_model", "test_boundary"],
    "WPA-II-06-C": ["adjudicator", "break_authority", "break_tests", "product_gate"],
    "WPA-II-06-D": ["rack_source", "fact_model", "test_boundary"],
    "WPA-II-06-E": ["adjudicator", "break_authority", "break_tests", "head_bih_tests", "product_gate"],
    "WPA-II-06-F": ["adjudicator", "eight_authority", "eight_tests", "product_gate"],
    "WPA-II-06-G": ["fact_model", "break_authority", "break_tests", "product_gate"],
    "WPA-II-07": ["rack_source", "rack_tests", "product_gate"],
    "WPA-II-08": ["adjudicator", "bih_authority", "head_bih_tests", "product_gate"],
    "WPA-II-09": ["bih_authority", "full_bih_tests", "product_gate"],
    "WPA-II-10-1": ["adjudicator", "rules_tests", "simultaneous_tests"],
    "WPA-II-10-2": ["adjudicator", "core_a_tests"],
    "WPA-II-10-3": ["adjudicator", "rules_tests"],
    "WPA-II-10-4": ["adjudicator", "group_authority", "group_tests", "product_gate"],
    "WPA-II-11": ["ruleset", "config_tests"],
    "WPA-II-12-1": ["adjudicator", "rules_tests", "simultaneous_tests"],
    "WPA-II-12-2": ["collector", "adjudicator", "collector_tests", "rules_tests"],
    "WPA-II-12-3": ["adjudicator", "core_a_tests"],
    "WPA-II-12-4": ["fact_model", "test_boundary"],
    "WPA-II-12-5-6": ["fact_model", "test_boundary"],
    "WPA-II-12-7": ["fact_model", "test_boundary"],
    "WPA-II-12-8": ["fact_model", "test_boundary"],
    "WPA-II-12-9": ["fact_model", "test_boundary"],
    "WPA-II-13": ["collector", "adjudicator", "collector_tests", "simultaneous_tests"],
    "WPA-II-14": ["fact_model", "test_boundary"],
    "WPA-II-15-1": ["fact_model", "test_boundary"],
    "WPA-II-15-2": ["fact_model", "test_boundary"],
    "WPA-II-16": ["fact_model", "test_boundary"],
    "WPA-II-17": ["fact_model", "adjudicator", "rules_tests", "core_a_tests"],
    "WPA-II-18": ["fact_model", "test_boundary"],
    "WPA-II-19": ["fact_model", "test_boundary"],
    "WPA-II-20": ["adjudicator", "rules_tests", "core_a_tests"],
    "WPA-II-21": ["fact_model", "test_boundary"],
    "WPA-II-22": ["fact_model", "test_boundary"],
    "WPA-II-23-24": ["fact_model", "test_boundary"],
    "WPA-III-01-08": ["fact_model", "test_boundary"],
}


def build() -> dict:
    v1 = json.loads(V1_PATH.read_text(encoding="utf-8"))
    v1_rows = {row["id"]: row for row in v1["matrix"]}
    rows = []
    for row_id in STATUS:
        prior = v1_rows[row_id]
        row = {
            "id": row_id,
            "source": prior["source"],
            "requirement": prior["requirement"],
            "status": STATUS[row_id],
            "m4_required": prior["m4_required"],
            "policy_decision_required": prior["policy_decision_required"],
            "product_action_required": row_id in {
                "WPA-II-04", "WPA-II-06-C", "WPA-II-06-E", "WPA-II-06-F",
                "WPA-II-07", "WPA-II-08", "WPA-II-09", "WPA-II-10-4"},
            "product_reachable": row_id not in {
                "WPA-II-04", "WPA-II-06-C", "WPA-II-06-E", "WPA-II-06-F",
                "WPA-II-07", "WPA-II-08", "WPA-II-09", "WPA-II-10-4"},
            "core_increments": CORE.get(row_id, []),
            "assessment": ASSESSMENT[row_id],
            "evidence": [evidence(key) for key in EVIDENCE_KEYS[row_id]],
            "gaps": GAPS.get(row_id, []),
        }
        if row_id == "WPA-II-06-G":
            row["classification"] = "pending"
            row["classification_error"] = "rules_break_no_rack_contact_classification_pending"
        rows.append(row)

    counts = Counter(STATUS.values())
    m4_unclosed = [row["id"] for row in rows
                   if row["m4_required"] and row["status"] != "IMPLEMENTED"]
    return {
        "schema": "phase5_wpa_heyball_as_built_reconciliation_audit_v2",
        "schema_version": 2,
        "audit_id": "phase5_wpa_heyball_2025_08_16_as_built_reconciliation_v2",
        "generated_on": date.today().isoformat(),
        "ruleset_id": "wpa_heyball_2025_08_16_v1",
        "audit_scope": "read_only_as_built_core_a_through_h",
        "profile_complete": False,
        "product_exposed": False,
        "complete_profile_product_exposed": False,
        "normative_source": v1["normative_source"],
        "frozen_predecessor_files": V1_FILES,
        "status_semantics": {
            "IMPLEMENTED": "Authoritative product-reachable automatic behavior has complete source and test evidence for the row.",
            "PARTIAL": "A proper subset or non-product foundation/authority exists; product-action rows cannot be IMPLEMENTED while gated off.",
            "MISSING": "Required fact, adjudication, action, state or test surface is absent.",
            "PROCEDURAL_ONLY": "The row requires referee/event procedure and is not fabricated as gameplay automation.",
        },
        "summary": {"total": len(rows), **{key.lower(): counts[key] for key in
            ("IMPLEMENTED", "PARTIAL", "MISSING", "PROCEDURAL_ONLY")}},
        "policy_conflicts": [{
            "id": "off_table_12_3_vs_17_e",
            "rows": ["WPA-II-12-3", "WPA-II-17"],
            "status": "policy_decision_required",
            "resolved": False,
            "decision_owner": "external_rules_policy",
        }],
        "classification_pending": [{
            "id": "section_6_g_no_rack_contact",
            "row": "WPA-II-06-G",
            "status": "classification_pending",
            "error_code": "rules_break_no_rack_contact_classification_pending",
            "mutation_allowed": False,
        }],
        "m4_required_unclosed": m4_unclosed,
        "next_deterministic_implementation_plan": [
            {"priority": "P0", "rows": ["WPA-II-06-C", "WPA-II-06-E", "WPA-II-06-F", "WPA-II-08", "WPA-II-09", "WPA-II-10-4"], "depends_on": ["rules_decision_ui_v2_verification", "production_capability_enablement"], "deliverable": "Promote existing Core C/F/G/H/D authority transactions through the verified product decision surface."},
            {"priority": "P0", "rows": ["WPA-II-04", "WPA-II-07"], "depends_on": ["product_rack_lifecycle_parent_actions", "official_rack_contract_product_acceptance"], "deliverable": "Integrate Core E rack/respot foundations only behind authoritative product parent actions."},
            {"priority": "P1", "rows": ["WPA-II-06-G"], "depends_on": ["authoritative_miscue_and_intent_telemetry"], "deliverable": "Replace classification_pending with a tested no-rack-contact classifier and re-break authority path."},
            {"priority": "P1", "rows": ["WPA-II-06-D"], "depends_on": ["head_string_crossing_telemetry", "per_player_break_offense_history"], "deliverable": "Implement deterministic reminder then foul escalation."},
            {"priority": "P1", "rows": ["WPA-II-12-5-6", "WPA-II-12-7", "WPA-II-15-1", "WPA-II-15-2", "WPA-II-16"], "depends_on": ["cue_tip_contact_telemetry", "frozen_ball_declaration_input", "jump_trajectory_classification"], "deliverable": "Add measurable stroke/contact predicates, adjudication and bad-input tests without inventing referee judgments."},
            {"priority": "P2", "rows": ["WPA-II-21"], "depends_on": ["referee_stalemate_proposal_input", "shot_counter_and_original_breaker_state"], "deliverable": "Implement the official stalemate proposal/refusal and rerack state machine."},
        ],
        "matrix": rows,
    }


def render_markdown(value: dict, json_sha: str) -> str:
    summary = value["summary"]
    lines = [
        "# Phase 5 WPA Heyball as-built reconciliation audit v2", "",
        f"Generated `{value['generated_on']}` from the frozen official 2025-08-16 profile. JSON SHA-256: `{json_sha}`.", "",
        "This is a read-only Core A-H reconciliation. It does not rewrite the frozen v1 audit, promote the complete profile, resolve policy, or enable Product Rules UI.", "",
        "## Current result", "",
        f"- Total: {summary['total']}",
        f"- IMPLEMENTED: {summary['implemented']}",
        f"- PARTIAL: {summary['partial']}",
        f"- MISSING: {summary['missing']}",
        f"- PROCEDURAL_ONLY: {summary['procedural_only']}",
        f"- Profile complete: `{str(value['profile_complete']).lower()}`",
        f"- Product exposed: `{str(value['product_exposed']).lower()}`",
        f"- Complete-profile product exposed: `{str(value['complete_profile_product_exposed']).lower()}`", "",
        "Foundation executors and authority transactions do not count as IMPLEMENTED when their required Product action is gated off. This is why §6(c), §6(e), §6(f), §7, §8, §9 and §10.4 remain PARTIAL despite substantive Core C-F/G/H code.", "",
        "The §12.3/§17(e) conflict remains `policy_decision_required`; §6(g) remains `classification_pending` and fail-closed.", "",
        "## M4-required rows still unclosed", "",
        ", ".join(f"`{row}`" for row in value["m4_required_unclosed"]), "",
        "## Row-by-row evidence", "",
        "| Row | Status | Core | Product | Assessment | Exact evidence |", "|---|---|---|---|---|---|",
    ]
    for row in value["matrix"]:
        refs = "<br>".join(f"`{item['path']}:{item['lines']['start']}` `{item['sha256'][:12]}…` ({item['kind']})" for item in row["evidence"])
        assessment = row["assessment"].replace("|", "\\|")
        product = "gated" if row["product_action_required"] and not row["product_reachable"] else "automatic / n.a."
        lines.append(f"| `{row['id']}` | {row['status']} | {','.join(row['core_increments']) or '—'} | {product} | {assessment} | {refs} |")
    lines.extend(["", "## Next deterministic implementation batches", ""])
    for item in value["next_deterministic_implementation_plan"]:
        rows = ", ".join(f"`{row}`" for row in item["rows"])
        deps = ", ".join(f"`{dep}`" for dep in item["depends_on"])
        lines.append(f"- **{item['priority']} — {rows}:** {item['deliverable']} Dependencies: {deps}.")
    lines.extend(["", "Policy-gated rows are deliberately excluded from deterministic closure claims. Full path, SHA, line range, finding and gap data lives in the JSON audit.", ""])
    return "\n".join(lines)


def main() -> None:
    value = build()
    encoded = (json.dumps(value, indent=2, ensure_ascii=False) + "\n").encode("utf-8")
    JSON_PATH.write_bytes(encoded)
    MD_PATH.write_text(render_markdown(value, hashlib.sha256(encoded).hexdigest()), encoding="utf-8")
    print(JSON_PATH.relative_to(ROOT))
    print(MD_PATH.relative_to(ROOT))


if __name__ == "__main__":
    main()
