#!/usr/bin/env python3
"""Validate the WPA-II-08 selected exact-crossing feasibility blocker."""

from __future__ import annotations

import argparse
import hashlib
import json
from pathlib import Path
from typing import Any


ROOT = Path(__file__).resolve().parents[1]
DEFAULT_GATE = ROOT / "docs/rules/wpa-ii-08-selected-center-crossing-exact-continuous-order-feasibility-blocker-v1.json"
TOP_LEVEL_KEYS = {
    "schema", "status", "selection_binding", "predecessor_bindings",
    "selected_policy_contract", "feasibility_findings", "all_behind_respot_boundary",
    "required_successor_design_gate", "source_bindings", "cpu_read_only_verification", "claims",
}
DIGESTS = {
    "selection_binding": "bee7257d3a6586c16f57c5316c8ef4cc6eebe7ab95a4041046cefd3c938e2ffb",
    "predecessor_bindings": "09289d898037e2fa3a9e33a5fdc6c8104054da5fa5c0bf21c76ef27a3dafe5b8",
    "selected_policy_contract": "95983ad9897ddeb2f6cfaf4d11264195c1e9c6eddd3314146fbc4e53999aa03b",
    "feasibility_findings": "6b6102b0fddb97357b1a5547c06c62f6c4021592dfa38401b9aba258d9897567",
    "all_behind_respot_boundary": "a51317de9f92f79377604e56d8dec5726ed13c31421ef45abfb4013c1c88f262",
    "required_successor_design_gate": "db840304e3c7378ecf523e9fe3d2bb4fe5626413b2d6722aadd98c485f4d2433",
    "source_bindings": "619f54656a29bef74dccf35b39c557a8136832cce7919c1bdb42f983cdc9e7f4",
    "cpu_read_only_verification": "65b9d954d45c6e531500bac41d9fded3374069ce7d6f57fa646054b6375a8a97",
    "claims": "4e87e01a9afc03f8b2f68d907b815c193aaeadd859827beee7eda55074523961",
}


def sha256(data: bytes) -> str:
    return hashlib.sha256(data).hexdigest()


def canonical_digest(value: Any) -> str:
    return sha256(json.dumps(value, sort_keys=True, separators=(",", ":"),
        ensure_ascii=False).encode("utf-8"))


def resolve(raw: Any) -> Path | None:
    if not isinstance(raw, str):
        return None
    path = Path(raw)
    if path.is_absolute() or ".." in path.parts:
        return None
    result = (ROOT / path).resolve()
    return result if result.is_relative_to(ROOT.resolve()) else None


def binding_ok(binding: Any) -> bool:
    if not isinstance(binding, dict):
        return False
    path = resolve(binding.get("path"))
    return path is not None and path.is_file() and \
        sha256(path.read_bytes()) == binding.get("sha256")


def validate(value: Any, check_files: bool = True) -> list[str]:
    if not isinstance(value, dict):
        return ["root_not_object"]
    errors: list[str] = []
    if set(value) != TOP_LEVEL_KEYS:
        errors.append("top_level_keys")
    if value.get("schema") != "wpa_ii_08_selected_center_crossing_exact_continuous_order_feasibility_blocker_v1":
        errors.append("schema")
    if value.get("status") != "BLOCKED_NO_CANONICAL_CONTINUOUS_CROSSING_EVENT_OR_TOTAL_ORDER_NO_PRODUCTION_MUTATION":
        errors.append("status")
    for key, digest in DIGESTS.items():
        if key not in value or canonical_digest(value.get(key)) != digest:
            errors.append(f"contract_digest:{key}")

    selection = value.get("selection_binding")
    if not isinstance(selection, dict) or selection.get("row_id") != "WPA-II-08" or \
            selection.get("decision_id") != "M4-POL-08-HEAD-STRING-CROSSING" or \
            selection.get("selected_option_id") != "CENTER_CROSSING_EXACT_CONTINUOUS_ORDER" or \
            selection.get("authorized_stage") != "B1_TESTS_FIRST_CPU_DIRECT_CODE" or \
            selection.get("root_selected") is not True or \
            selection.get("selection_resolves_geometry_but_does_not_create_missing_telemetry") is not True or \
            selection.get("selection_alone_closes_row") is not False:
        errors.append("selection_contract")

    policy = value.get("selected_policy_contract")
    if not isinstance(policy, dict) or policy.get("crossing_reference_point") != "CUE_BALL_CENTER" or \
            policy.get("crossing_relation") != "CENTER_STRICTLY_CROSSES_HEAD_STRING" or \
            policy.get("required_order") != "CONTINUOUS_CROSSING_EVENT_STRICTLY_BEFORE_FIRST_OBJECT_CONTACT" or \
            policy.get("same_time_or_unordered_result") != "FAIL_CLOSED" or \
            policy.get("incomplete_evidence_result") != "FAIL_CLOSED" or \
            policy.get("legal_object_on_head_string_direct_contact_legal") is not True or \
            policy.get("legal_object_foot_side_direct_contact_legal") is not True or \
            policy.get("leading_edge_inference_forbidden") is not True or \
            policy.get("sampled_endpoint_approximation_forbidden") is not True or \
            policy.get("product_reclassification_forbidden") is not True:
        errors.append("selected_policy")

    findings = value.get("feasibility_findings")
    solver = findings.get("continuous_solver") if isinstance(findings, dict) else None
    telemetry = findings.get("physics_step_telemetry") if isinstance(findings, dict) else None
    collector = findings.get("shot_fact_collector") if isinstance(findings, dict) else None
    facts = findings.get("shot_facts") if isinstance(findings, dict) else None
    if not isinstance(findings, dict) or findings.get("same_step_exact_order_provable_from_current_contract") is not False or \
            findings.get("implementation_may_infer_from_endpoints_or_contact_tick") is not False or \
            findings.get("blocker_class") != "NEW_PHYSICS_TELEMETRY_AND_COLLECTOR_PROVENANCE_REQUIRED":
        errors.append("feasibility_result")
    if not isinstance(solver, dict) or solver.get("ball_ball_and_boundary_contact_toi_available") is not True or \
            solver.get("head_string_plane_crossing_candidate_exists") is not False or \
            solver.get("head_string_plane_crossing_telemetry_record_exists") is not False or \
            solver.get("tail_contact_toi_rebased_to_whole_tick_absolute_time") is not False or \
            solver.get("solver_event_id_orders_contact_batches_but_cannot_order_an_absent_crossing") is not True:
        errors.append("solver_findings")
    if not isinstance(telemetry, dict) or telemetry.get("contact_records_have_time_of_impact") is not True or \
            telemetry.get("surface_motion_steps_have_segment_start_end_positions") is not False or \
            telemetry.get("surface_motion_steps_have_segment_duration_and_absolute_offset") is not False or \
            telemetry.get("before_after_cue_center_endpoints_are_sufficient_for_strict_same_step_order") is not False:
        errors.append("telemetry_findings")
    if not isinstance(collector, dict) or collector.get("begin_receives_behind_head_string_scope_or_plane") is not False or \
            collector.get("consume_records_first_contact_tick_object_set") is not True or \
            collector.get("consume_preserves_first_contact_within_tick_toi") is not False or \
            collector.get("consume_records_crossing_event") is not False or \
            collector.get("finish_attests_crossing_origin_completeness_and_order") is not False:
        errors.append("collector_findings")
    if not isinstance(facts, dict) or any(facts.get(key) is not False for key in facts):
        errors.append("shot_fact_findings")

    boundary = value.get("all_behind_respot_boundary")
    if not isinstance(boundary, dict) or boundary.get("official_candidate_actor_and_sample_time_logic_already_resolved") is not True or \
            boundary.get("production_work_stopped_at_selected_crossing_feasibility_gate") is not True:
        errors.append("all_behind_boundary")
    for key in ("dedicated_parent_authority_exists", "request_and_commit_schema_exists",
                "frozen_state_hash_revalidation_exists", "tie_choice_authority_exists",
                "replacement_core_g_child_transaction_exists", "narrow_product_action_exists",
                "partial_product_exposure_before_complete_facts_allowed"):
        if not isinstance(boundary, dict) or boundary.get(key) is not False:
            errors.append(f"all_behind_false:{key}")

    successor = value.get("required_successor_design_gate")
    if not isinstance(successor, dict) or \
            successor.get("gate_id") != "WPA-II-08-EXACT-CONTINUOUS-CROSSING-TELEMETRY-IMPLEMENTATION-GATE" or \
            successor.get("root_approval_required_before_production") is not True or \
            not isinstance(successor.get("must_define"), list) or len(successor["must_define"]) != 8 or \
            not isinstance(successor.get("potential_production_allowlist"), list) or len(successor["potential_production_allowlist"]) != 14 or \
            not isinstance(successor.get("required_positive_fixtures"), list) or len(successor["required_positive_fixtures"]) != 8 or \
            not isinstance(successor.get("required_negative_fixtures"), list) or len(successor["required_negative_fixtures"]) != 9:
        errors.append("successor_gate")

    sources = value.get("source_bindings")
    if not isinstance(sources, dict) or len(sources) != 17:
        errors.append("source_bindings")
    elif check_files:
        for key, binding in sources.items():
            if not binding_ok(binding):
                errors.append(f"source_drift:{key}")

    cpu = value.get("cpu_read_only_verification")
    binaries = cpu.get("binaries") if isinstance(cpu, dict) else None
    if not isinstance(cpu, dict) or cpu.get("build_root") != "build-product-session" or \
            cpu.get("ctest_subset") != {"passed": 3, "failed": 0} or \
            cpu.get("production_or_cmake_source_modified_in_task") is not False or \
            cpu.get("gpu_cocoa_or_opengl_executed") is not False or \
            not isinstance(binaries, list) or len(binaries) != 3 or \
            any(not isinstance(item, dict) or item.get("exit_code") != 0 for item in (binaries or [])):
        errors.append("cpu_boundary")
    elif check_files:
        build_root = resolve(cpu.get("build_root"))
        for item in binaries:
            binary = build_root / str(item.get("target", "")) if build_root else None
            if binary is None or not binary.is_file() or sha256(binary.read_bytes()) != item.get("sha256"):
                errors.append(f"binary_drift:{item.get('target', 'unknown')}")

    claims = value.get("claims")
    if not isinstance(claims, dict) or claims.get("root_selected_policy_bound") is not True:
        errors.append("positive_claim")
    for key in ("same_step_exact_order_currently_provable", "crossing_telemetry_implemented",
                "all_behind_respot_authority_implemented", "all_behind_product_action_reachable",
                "wpa_ii_08_row_closed", "audit_v5_modified_or_promoted",
                "global_rules_decision_ui_exposed", "product_behavior_changed",
                "production_source_modified", "cmake_modified", "gpu_executed",
                "readiness_or_authorization_created"):
        if not isinstance(claims, dict) or claims.get(key) is not False:
            errors.append(f"false_completion:{key}")

    if check_files:
        try:
            if not binding_ok(selection):
                errors.append("selection_drift")
            else:
                selected = json.loads(resolve(selection["path"]).read_text(encoding="utf-8"))
                row = next(item for item in selected["decisions"] if item["row_id"] == "WPA-II-08")
                if row["selected_option_id"] != selection["selected_option_id"] or row["root_selected"] is not True or \
                        row["authorized_stage"] != selection["authorized_stage"] or row["row_closed_by_selection"] is not False:
                    errors.append("selection_row_drift")
        except (OSError, KeyError, StopIteration, TypeError, json.JSONDecodeError):
            errors.append("selection_parse")

        predecessors = value.get("predecessor_bindings")
        try:
            if not binding_ok(predecessors["candidate"]):
                errors.append("candidate_drift")
            if not binding_ok(predecessors["design_gap_gate"]):
                errors.append("design_gap_gate_drift")
        except (KeyError, TypeError):
            errors.append("predecessor_parse")

        physics_telemetry = (ROOT / "src/Billiards/physics_telemetry.h").read_text(encoding="utf-8")
        surface_motion = (ROOT / "src/Billiards/surface_motion.h").read_text(encoding="utf-8")
        collector_text = (ROOT / "src/Billiards/rules/shot_fact_collector.cpp").read_text(encoding="utf-8")
        facts_text = (ROOT / "src/Billiards/rules/chinese_eight_ball_rules.h").read_text(encoding="utf-8")
        product_text = (ROOT / "src/Billiards/render/modern/modern_product_session.cpp").read_text(encoding="utf-8")
        if "std::vector<PhysicsContactRecord> contacts" not in physics_telemetry or \
                "HeadStringCrossing" in physics_telemetry or "PlaneCrossing" in physics_telemetry:
            errors.append("physics_telemetry_static")
        if "struct SurfaceMotionStep" not in surface_motion or "startPosition" in surface_motion or \
                "endPosition" in surface_motion or "absoluteTime" in surface_motion:
            errors.append("surface_motion_static")
        if "firstTickObjectBalls" not in collector_text or "headString" in collector_text or \
                "HeadString" in collector_text:
            errors.append("collector_static")
        if "struct ShotFacts" not in facts_text or "cueBallCrossedHeadString" in facts_text or \
                "headStringCrossing" in facts_text:
            errors.append("shot_facts_static")
        if "rules.ball_in_hand.request_nearest_legal_respot" in product_text or \
                "rules.ball_in_hand.choose_equidistant_respot_ball" in product_text:
            errors.append("product_action_unexpected")
        for relative in ("src/Billiards/rules/behind_head_string_respot_authority.h",
                         "src/Billiards/rules/behind_head_string_respot_authority.cpp"):
            if (ROOT / relative).exists():
                errors.append("future_authority_unexpected")
    return errors


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("gate", nargs="?", default=str(DEFAULT_GATE))
    parser.add_argument("--no-file-checks", action="store_true")
    args = parser.parse_args()
    try:
        value = json.loads(Path(args.gate).read_text(encoding="utf-8"))
    except (OSError, json.JSONDecodeError) as exc:
        print(f"INVALID: {exc}")
        return 1
    errors = validate(value, check_files=not args.no_file_checks)
    if errors:
        print("INVALID: " + ", ".join(errors))
        return 1
    print("PASS_WPA_II_08_SELECTED_CENTER_CROSSING_FEASIBILITY_BLOCKER")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
