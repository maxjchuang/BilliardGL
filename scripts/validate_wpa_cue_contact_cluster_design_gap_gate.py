#!/usr/bin/env python3
"""Validate the fail-closed WPA cue-contact cluster design gap gate."""

from __future__ import annotations

import argparse
import hashlib
import json
from pathlib import Path
from typing import Any


ROOT = Path(__file__).resolve().parents[1]
DEFAULT_GATE = ROOT / "docs/rules/wpa-cue-contact-cluster-design-gap-gate-v1.json"
PDF_SHA = "2b92dd5088ea630337326e26b7a2a7a4499a9801f2c5da7e5cc40421d5424a24"
AUDIT_SHA = "2811958f1205d9998b40060b83c577f91359d2164e1ac0eaa9ccc4cac2ad787a"
SCOPE = ["WPA-II-12-5-6", "WPA-II-12-7", "WPA-II-15-1", "WPA-II-15-2"]
SOURCE_SHA = {
    "src/Billiards/cue_impact.h": "b8d34ee8204596366eb55ade70bb2db1177212adeff6d5aa25ba77b1b52c0000",
    "src/Billiards/cue_contact.h": "8a04660ae3927f76544a46a2676fe30e17c88856f4b2f9f53c274dd849be9a9c",
    "src/Billiards/cue_contact.cpp": "371eeea9353fe2759c0edf25101cdaa2a1298d6688b537690a65957c47565d27",
    "src/Billiards/coupled_cue_contact.h": "c4e24dbfcf3b5f3a3eb4ab4ef4d639bc31a56fb194ae046b44aea5ea2c408664",
    "src/Billiards/coupled_cue_contact.cpp": "e5034d0697ba86e3ed2341d2555d9da6fefbd6f084b0fb74bdf45b386b75d278",
    "src/Billiards/frozen_cue_topology.h": "4c1be21a868120275e7bc12f4fa95dcf99447fc833932a831183faf3c37547cf",
    "src/Billiards/frozen_cue_topology.cpp": "031271b5984ad354b5d4c41256aba3083ec80243a663ffbb1ec3cb00d2cf021d",
    "src/Billiards/physics_telemetry.h": "d1fa08cb8d1c2878d7a662dd267959ffb8b505fb6afcc22351b1056a5a941178",
    "src/Billiards/physics_telemetry.cpp": "b2e781bbc461e28688d8bb9eeca9b2bfb067317099adf0d431c3fe2761592612",
    "src/Billiards/rules/chinese_eight_ball_rules.h": "86b6bf332fe5ca182580c57f82c74ddda075fda28b659019c709bc459e785793",
    "src/Billiards/rules/chinese_eight_ball_rules.cpp": "b984639bdf239814e0a1e0a14a64d6b73de56841273f71d7671c20e86bf475a4",
    "src/Billiards/rules/shot_fact_collector.h": "88dfc28370b4b56936ac88ac560623a8600d89a295b1f98efff65471de17d615",
    "src/Billiards/rules/shot_fact_collector.cpp": "c901ba8591ebd301ccbd4a2ee4fe2ab02f390436cf80aed0596074f65e0ec4c4",
    "src/Billiards/game_runtime.h": "61453e15d88d5581fa439fa77eb6b834fc0a22a5366b470b182b17b8d4d9f725",
    "src/Billiards/game_runtime.cpp": "fca6d4f8ffc54b5d68f3aa24928f71416b003ac8ae37ef7ab4d376aa78684ca4",
    "tests/cue_contact_tests.cpp": "93a6d368f985d6c990537d12eef5ea89ea9166e0029b6836b425a0328139755d",
    "tests/coupled_cue_contact_tests.cpp": "b439f5a698ac7ee665f16cc1ab0c35df96fe577882d8c57fbb17adc1ffbfd777",
    "tests/physics_telemetry_tests.cpp": "cbbb2eb4e5f1c1eb22c5525d0c1859e19aebf2a0d975b94321f6daf741ced0c8",
    "tests/shot_fact_collector_tests.cpp": "4a5a4fee924cf62c29551844af718a4b73e6b5917d1b1323dcac29b20d43683c",
    "tests/chinese_eight_ball_rules_tests.cpp": "4c4079159e1db19545a82c1ad43b1cdfbee8ea4462b9c66c92a8be13322f3b8e",
    "tests/rules_runtime_integration_tests.cpp": "aab289bcb7e01d037e6022d41676145e09027d2d90317b9cf9e2e017335f0698",
}


def sha256(path: Path) -> str:
    return hashlib.sha256(path.read_bytes()).hexdigest()


def workspace_path(raw: Any) -> Path | None:
    if not isinstance(raw, str) or not raw or Path(raw).is_absolute() or ".." in Path(raw).parts:
        return None
    path = (ROOT / raw).resolve()
    try:
        path.relative_to(ROOT.resolve())
    except ValueError:
        return None
    return path


def exact_list(value: Any, expected: set[str]) -> bool:
    return isinstance(value, list) and len(value) == len(expected) and set(value) == expected


def validate(value: Any, *, check_files: bool = True) -> list[str]:
    errors: list[str] = []
    if not isinstance(value, dict):
        return ["gate_not_object"]
    if value.get("schema") != "wpa_cue_contact_cluster_design_gap_gate_v1" or \
            value.get("status") != "POLICY_REFEREE_INPUT_AND_TELEMETRY_PENDING_NO_PRODUCTION_MUTATION" or \
            value.get("scope") != SCOPE:
        errors.append("identity_status_scope")

    official = value.get("official_source")
    expected_sections = [("I", "5", 3), ("II", "12.5-12.6", 9),
                         ("II", "12.7", 10), ("II", "15.1", 10),
                         ("II", "15.2", 10)]
    if not isinstance(official, dict) or official.get("path") != "tmp/pdfs/250816-Rules-of-Heyball.pdf" or \
            official.get("sha256") != PDF_SHA or official.get("visually_reviewed_pdf_pages") != [3, 9, 10]:
        errors.append("official_binding")
    else:
        sections = official.get("sections")
        actual_sections = [(x.get("chapter"), x.get("clause"), x.get("pdf_page"))
                           for x in sections] if isinstance(sections, list) and all(isinstance(x, dict) for x in sections) else []
        if actual_sections != expected_sections or any(not x.get("controlled_paraphrase") for x in sections):
            errors.append("official_sections")
        required_rules = {
            "more than one cue-tip contact with the cue ball during one stroke is a foul",
            "cue-ball contact with a close object ball before the cue tip leaves the cue ball is within the double-hit rule",
            "a push is distinct from an instantaneous strike and remains prohibited for a declared frozen cue/object arrangement",
            "frozen-ball rule treatment requires a referee announcement and absence of an announcement means not frozen",
            "a declared frozen cue/object arrangement changes the double-hit presumption but does not legalize an obvious push",
            "a declared frozen object/cushion arrangement has leave-and-recontact, other-ball-cushion, or pocket alternatives",
        }
        pending = {
            "the separation threshold for very close",
            "the trajectory, angle, speed, impulse, or duration threshold for lightly grazes",
            "the contact-distance or contact-duration threshold separating an instantaneous strike from an obvious push",
            "the kinematic threshold for an obvious stroke action or obvious push",
            "the geometric tolerance and authoritative sample time used by a referee to declare a ball frozen",
            "whether pocket jaws satisfy the cushion alternatives in clause 15.2",
            "the exact machine predicate represented by the prerequisite phrase about a completed legal cushion contact",
        }
        if not exact_list(official.get("uniquely_determined_rules"), required_rules):
            errors.append("official_resolved_rules")
        if not exact_list(official.get("not_numerically_or_operationally_defined"), pending):
            errors.append("official_pending_rules")
        if check_files:
            path = workspace_path(official.get("path"))
            if path is None or not path.is_file() or sha256(path) != PDF_SHA:
                errors.append("official_pdf_drift")

    audit = value.get("frozen_audit")
    expected_rows = [{"id": row, "status": "MISSING", "product_reachable": True} for row in SCOPE]
    if not isinstance(audit, dict) or audit.get("path") != "docs/rules/phase5-wpa-heyball-as-built-reconciliation-audit-v5.json" or \
            audit.get("sha256") != AUDIT_SHA or audit.get("rows") != expected_rows or \
            audit.get("promotion_authorized") is not False:
        errors.append("audit_binding")
    elif check_files:
        path = workspace_path(audit.get("path"))
        if path is None or not path.is_file() or sha256(path) != AUDIT_SHA:
            errors.append("audit_drift")
        else:
            audit_value = json.loads(path.read_text(encoding="utf-8"))
            rows = [x for x in audit_value.get("matrix", []) if x.get("id") in SCOPE]
            if [{"id": x.get("id"), "status": x.get("status"),
                 "product_reachable": x.get("product_reachable")} for x in rows] != expected_rows:
                errors.append("audit_rows_drift")

    bindings = value.get("source_bindings")
    actual_bindings = {x.get("path"): x.get("sha256") for x in bindings.values()
                       if isinstance(x, dict)} if isinstance(bindings, dict) else {}
    if actual_bindings != SOURCE_SHA or len(bindings or {}) != len(SOURCE_SHA):
        errors.append("source_bindings")
    elif check_files:
        for raw, digest in SOURCE_SHA.items():
            path = workspace_path(raw)
            if path is None or not path.is_file() or sha256(path) != digest:
                errors.append(f"source_drift:{raw}")

    mapping = value.get("current_system_mapping")
    expected_mapping = {
        "regular_cue_contact_is_single_analytic_resolution": True,
        "regular_cue_contact_reports_contact_duration": False,
        "coupled_contact_reports_contiguous_microsteps_until_release_or_failure": True,
        "coupled_contact_reports_microstep_time_force_impulse_and_constraint_contacts": True,
        "coupled_contact_simulates_cue_tip_after_initial_release": False,
        "full_shot_second_tip_contact_observable": False,
        "full_shot_tip_contact_count_complete": False,
        "frozen_topology_is_geometry_detected_with_solver_slop": True,
        "frozen_topology_is_referee_declared": False,
        "shot_fact_collector_consumes_cue_contact_result": False,
        "shot_fact_collector_tracks_declared_frozen_ball_identity": False,
        "shot_fact_collector_tracks_frozen_object_departure_and_recontact": False,
        "shot_fact_collector_tracks_other_ball_cushion_and_pocket_raw_events": True,
        "rules_model_has_double_hit_push_or_frozen_foul_reasons": False,
    }
    if mapping != expected_mapping:
        errors.append("current_mapping")

    observations = value.get("policy_independent_observations")
    if not isinstance(observations, dict) or len(observations.get("safe_existing_raw_observations", [])) != 5 or \
            len(observations.get("forbidden_inferences_from_existing_data", [])) != 6 or \
            observations.get("production_extension_safe_now") is not False or \
            "stop simulating the cue tip after release" not in observations.get("reason", ""):
        errors.append("observation_boundary")

    telemetry = value.get("required_future_telemetry_contract")
    expected_lengths = {"stroke_identity": 6, "cue_tip_intervals": 9,
                        "completeness": 5, "cue_object_overlap": 7,
                        "frozen_declaration": 9, "frozen_cushion_lifecycle": 9}
    if not isinstance(telemetry, dict) or telemetry.get("status") != "TELEMETRY_PENDING" or \
            telemetry.get("facts_are_measurements_not_fouls") is not True or \
            any(len(telemetry.get(key, [])) != count for key, count in expected_lengths.items()):
        errors.append("future_telemetry_contract")

    classification = value.get("pending_classification_contract")
    double_hit = classification.get("double_hit", {}) if isinstance(classification, dict) else {}
    push = classification.get("push", {}) if isinstance(classification, dict) else {}
    cue_frozen = classification.get("frozen_cue_object", {}) if isinstance(classification, dict) else {}
    rail_frozen = classification.get("frozen_object_cushion", {}) if isinstance(classification, dict) else {}
    if not isinstance(classification, dict) or classification.get("status") != "POLICY_AND_REFEREE_INPUT_PENDING" or \
            double_hit.get("resolved_machine_predicate") != "complete full-stroke cue-tip interval count greater than one" or \
            double_hit.get("close_ball_presumption") != "POLICY_PENDING" or double_hit.get("forbidden_guess") is not True or \
            push.get("resolved_machine_predicate") is not None or push.get("status") != "POLICY_OR_REFEREE_INPUT_PENDING" or push.get("forbidden_guess") is not True or \
            cue_frozen.get("geometry_is_not_declaration") is not True or cue_frozen.get("referee_declaration_required") is not True or cue_frozen.get("forbidden_guess") is not True or \
            rail_frozen.get("referee_declaration_required") is not True or rail_frozen.get("jaw_and_prerequisite_legal_cushion_policy") != "POLICY_PENDING" or rail_frozen.get("forbidden_guess") is not True:
        errors.append("pending_classification")

    atomic = value.get("future_atomic_contract")
    if not isinstance(atomic, dict) or len(atomic.get("declaration_commit", [])) != 3 or \
            len(atomic.get("shot_telemetry_commit", [])) != 4 or len(atomic.get("adjudication_commit", [])) != 4 or \
            "byte-equivalent" not in atomic.get("failure_rollback", "") or \
            "without duplicating" not in atomic.get("exact_retry", "") or \
            "zero mutation" not in atomic.get("conflicting_retry", "") or \
            atomic.get("partial_commit_allowed") is not False:
        errors.append("future_atomic_contract")

    fixtures = value.get("required_future_fixtures")
    if not isinstance(fixtures, dict) or len(fixtures.get("telemetry_positive", [])) != 8 or \
            len(fixtures.get("telemetry_negative", [])) != 12 or \
            len(fixtures.get("classification_after_policy", [])) != 8 or \
            any(len(items) != len(set(items)) for items in fixtures.values() if isinstance(items, list)):
        errors.append("future_fixtures")

    allowlist = value.get("future_implementation_allowlist")
    production = allowlist.get("production_files", []) if isinstance(allowlist, dict) else []
    if not isinstance(allowlist, dict) or \
            allowlist.get("allowlist_is_dormant_until_policy_referee_input_and_telemetry_design_is_approved") is not True or \
            allowlist.get("cmake_change_requires_separate_coordination") is not True or \
            "src/Billiards/render/modern/modern_renderer.cpp" in production or "CMakeLists.txt" in production:
        errors.append("future_allowlist")

    required_forbidden = {
        "production source edits in this design-gate task",
        "treating coupled microsteps as a complete full-stroke tip-contact trace",
        "treating geometry-detected contact as a referee frozen-ball declaration",
        "guessing very-close, light-graze, obvious-push, duration, distance, jaw or sampling tolerances",
        "adjudicating from incomplete cue-tip or frozen-cushion telemetry",
        "promoting any scoped audit row above MISSING",
        "enabling global rulesDecisionUiProductExposed",
        "CMake changes", "Phase4 or Phase5 renderer changes",
        "GPU, Cocoa, or OpenGL execution",
        "readiness generation or authorization issuance",
    }
    if not exact_list(value.get("forbidden_scope"), required_forbidden):
        errors.append("forbidden_scope")

    expected_claims = {
        "design_gap_gate_complete": True,
        "official_pages_visually_reviewed": True,
        "existing_solver_provenance_mapped": True,
        "complete_full_stroke_tip_telemetry_available": False,
        "referee_frozen_declaration_available": False,
        "double_hit_adjudication_implemented": False,
        "push_adjudication_implemented": False,
        "frozen_ball_adjudication_implemented": False,
        "production_implementation_complete": False,
        "audit_rows_promoted": False,
        "gpu_executed": False,
    }
    if value.get("claims") != expected_claims:
        errors.append("claims")
    return errors


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--gate", type=Path, default=DEFAULT_GATE)
    args = parser.parse_args()
    try:
        value = json.loads(args.gate.read_text(encoding="utf-8"))
    except (OSError, json.JSONDecodeError) as error:
        print(json.dumps({"status": "FAIL", "errors": [f"gate_read:{error}"]}, sort_keys=True))
        return 1
    errors = validate(value, check_files=True)
    print(json.dumps({"status": "PASS" if not errors else "FAIL",
                      "gate": str(args.gate.resolve()), "errors": errors}, sort_keys=True))
    if errors:
        return 1
    print("PASS_WPA_CUE_CONTACT_CLUSTER_POLICY_REFEREE_INPUT_TELEMETRY_PENDING_GATE")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
