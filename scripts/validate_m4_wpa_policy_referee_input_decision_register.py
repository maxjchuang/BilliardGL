#!/usr/bin/env python3
"""Validate the non-selecting M4 WPA policy/referee-input register."""

from __future__ import annotations

import argparse
import hashlib
import json
from pathlib import Path
from typing import Any


ROOT = Path(__file__).resolve().parents[1]
DEFAULT_REGISTER = ROOT / "docs/rules/m4-wpa-policy-referee-input-decision-register-v1.json"
PDF_SHA = "2b92dd5088ea630337326e26b7a2a7a4499a9801f2c5da7e5cc40421d5424a24"
AUDIT_SHA = "2811958f1205d9998b40060b83c577f91359d2164e1ac0eaa9ccc4cac2ad787a"
ENTRY_CONTRACT_SHA = "f1668e1281a0530d57fa390ec4feb408298928da9fd0485ba76577d57ff5ed5b"
AUDIT_ROWS = [
    "WPA-II-06-C", "WPA-II-06-D", "WPA-II-06-E", "WPA-II-06-F",
    "WPA-II-08", "WPA-II-09", "WPA-II-12-3", "WPA-II-12-5-6",
    "WPA-II-12-7", "WPA-II-15-1", "WPA-II-15-2", "WPA-II-16",
    "WPA-II-17", "WPA-II-21",
]
GATES = {
    "docs/rules/wpa-ii-06-c-legal-break-product-choice-design-gap-gate-v1.json": "b5826bcf9eaf2f1c4f7110a89392885a5b269e2e8644309b42efbac4c2ba89e0",
    "docs/rules/wpa-ii-06-d-head-string-half-ball-classification-design-gate-v1.json": "32f41fa0e7dbe36b7be01a2a16c7e4eebb90932aa8b99322f99ef31585acad79",
    "docs/rules/wpa-ii-06-e-break-foul-opponent-choice-design-gap-gate-v1.json": "3dcb6bb12c10256c988863711bb9b9624beba0053c7b6cb271ab89c34db4c8b0",
    "docs/rules/wpa-ii-06-f-eight-on-break-choice-design-gap-gate-v1.json": "f008d99e222289ba8279518a4aa21577d238d793140ae4071598684cc3939ae9",
    "docs/rules/wpa-ii-08-all-behind-respot-first-contact-design-gap-gate-v1.json": "2219bab50689979b77e6ba92630c4afae3276396993a73ac81b12e9e968acefe",
    "docs/rules/wpa-ii-09-tip-touch-until-shot-lifecycle-design-gap-gate-v1.json": "848d45fc1d8edd8daec8f55c8a4857a9f9f57d148f64bb6db28a0caf052b615e",
    "docs/rules/wpa-cue-contact-cluster-design-gap-gate-v1.json": "1442e4204c4acb0718ca0b87c858c8222e03ac634f5b5b7def3d9a6bdae64033",
    "docs/rules/wpa-ii-16-jump-shot-legality-design-gap-gate-v1.json": "520143e2eeb8f27656d041a867d93bd38fd202dd6a40ec09494d7c3eefcc9dc2",
    "docs/rules/wpa-ii-17-a-e-off-table-physical-telemetry-design-gap-gate-v1.json": "3fc468502ab09e7ba0122d84b6c20386b0646d1f0db76a3f8ab3e8a1d30a9dbe",
    "docs/rules/wpa-ii-19-ball-group-confusion-discovery-rerack-design-gap-gate-v1.json": "8bce41f79ba5c690d9846dc0d36c015c5fec1ae580a5ef8c91855029d2f74788",
    "docs/rules/wpa-ii-21-stalemate-state-machine-design-gap-gate-v1.json": "a7156415057caf733e2a6c7317d470ac13e01715b3f9c77b3f960b7d414b4e55",
}
RECEIPTS = {
    "docs/rules/wpa-ii-06-c-policy-independent-break-telemetry-implementation-receipt-v1.json": "c62c54f45d5b9cdfefced52aebf89ff991266f14a51d798511ab7419cb256597",
    "docs/rules/wpa-ii-06-f-entitlement-timing-implementation-receipt-v1.json": "c4ca0ff5aa9534cc5bdd0c285344a79470aaea0e51e3c5f677803b049ebec744",
    "docs/rules/wpa-ii-17-a-cue-scratch-policy-independent-telemetry-implementation-receipt-v1.json": "38d4c067d0f3c77603ef5149ea5160e859f8fc8d02c52d04a116761b8106aef5",
}
ENTRY = {
    "WPA-II-06-C": ("PARTIAL", "M4-POL-06C-BREAK-THRESHOLD", "STRAIGHT_CUSHION_VELOCITY_IMPULSE_OBJECT_POCKET", False, False, 3, 4),
    "WPA-II-06-D": ("PARTIAL", "M4-POL-06D-HALF-BALL-HISTORY", "FROZEN_POSITION_AT_ACCEPTED_BREAK_RELEASE_MATCH_SCOPE", False, False, 4, 4),
    "WPA-II-06-E": ("PARTIAL", "M4-PROD-06E-BREAK-FOUL-CHOICES", "NARROW_FROZEN_OFFER_CONSUMER", False, False, 3, 4),
    "WPA-II-06-F": ("PARTIAL", "M4-PROD-06F-EIGHT-ON-BREAK-FULL-CHOICES", "EXPAND_NARROW_CORE_F_TO_EXACT_RERACK_ACTIONS", False, False, 3, 4),
    "WPA-II-08": ("PARTIAL", "M4-POL-08-HEAD-STRING-CROSSING", "CENTER_CROSSING_EXACT_CONTINUOUS_ORDER", False, False, 4, 4),
    "WPA-II-09": ("PARTIAL", "M4-POL-09-TIP-TOUCH-SHOT-BOUNDARY", "COORDINATE_PLACEMENT_ONLY_POSITIVE_IMPULSE_CONSUMES", False, False, 3, 4),
    "WPA-II-12-3": ("PARTIAL", "M4-POL-12-3-OBJECT-OFF-TABLE-CONFLICT", "SIGNED_TOURNAMENT_PROFILE_DEFAULT_17E_FOUL", True, True, 3, 4),
    "WPA-II-12-5-6": ("MISSING", "M4-POL-12-5-6-DOUBLE-HIT", "AUTO_ONLY_COMPLETE_MULTI_INTERVAL_REFEREE_CLOSE_CASES", True, True, 3, 4),
    "WPA-II-12-7": ("MISSING", "M4-POL-12-7-PUSH", "MEASUREMENTS_PLUS_SIGNED_REFEREE_PUSH_DECLARATION", True, True, 3, 4),
    "WPA-II-15-1": ("MISSING", "M4-POL-15-1-FROZEN-CUE-OBJECT", "SIGNED_PRE_SHOT_FROZEN_DECLARATION", True, False, 3, 4),
    "WPA-II-15-2": ("MISSING", "M4-POL-15-2-FROZEN-CUSHION", "SIGNED_DECLARATION_STRAIGHT_CUSHION_CAPTURE_ONLY", True, False, 3, 4),
    "WPA-II-16": ("MISSING", "M4-PROD-16-JUMP-CAPABILITY", "KEEP_JUMP_DISABLED_FOR_M4", False, True, 3, 4),
    "WPA-II-17": ("PARTIAL", "M4-PROD-17-GENERAL-FOUL-COVERAGE", "EVIDENCE_BACKED_SUBSET_PLUS_SIGNED_REFEREE_EVENTS", True, True, 3, 4),
    "WPA-II-19": ("MISSING_PROGRAM_PRIORITY_OVERRIDE", "M4-POL-19-GROUP-CONFUSION", "SIGNED_CONFUSION_DECLARATION_ASSIGNMENT_ERROR_ONLY", True, False, 3, 4),
    "WPA-II-21": ("MISSING", "M4-POL-21-STALEMATE", "SIGNED_REFEREE_DIRECTIVE_DEFAULT_THREE_EACH", True, False, 4, 4),
}


def sha256(path: Path) -> str:
    return hashlib.sha256(path.read_bytes()).hexdigest()


def resolve(raw: str) -> Path | None:
    path = Path(raw)
    if path.is_absolute() or ".." in path.parts:
        return None
    result = (ROOT / path).resolve()
    return result if result.is_relative_to(ROOT.resolve()) else None


def exact_list(value: Any, expected: set[str]) -> bool:
    return isinstance(value, list) and len(value) == len(expected) and set(value) == expected


def validate(value: Any, check_files: bool = True) -> list[str]:
    errors: list[str] = []
    if not isinstance(value, dict):
        return ["root_not_object"]
    if value.get("schema") != "m4_wpa_policy_referee_input_decision_register_v1":
        errors.append("schema")
    if value.get("status") != "DECISION_REGISTER_READY_ALL_SELECTIONS_UNSELECTED_NO_PRODUCTION_MUTATION":
        errors.append("status")
    official = value.get("official_source", {})
    if official != {"path": "tmp/pdfs/250816-Rules-of-Heyball.pdf", "sha256": PDF_SHA,
                    "pages_covered_by_bound_gates": [3, 6, 7, 8, 9, 10, 11, 12]}:
        errors.append("official_binding")
    elif check_files:
        path = resolve(official["path"])
        if path is None or not path.is_file() or sha256(path) != PDF_SHA:
            errors.append("official_pdf_drift")

    audit = value.get("audit_binding", {})
    if audit != {
            "path": "docs/rules/phase5-wpa-heyball-as-built-reconciliation-audit-v5.json",
            "sha256": AUDIT_SHA, "m4_required_unclosed_rows": AUDIT_ROWS,
            "program_added_priority_row": "WPA-II-19", "program_added_row_audit_m4_required": False,
            "audit_mutation_or_promotion_authorized": False}:
        errors.append("audit_binding")
    elif check_files:
        path = resolve(audit["path"])
        if path is None or not path.is_file() or sha256(path) != AUDIT_SHA:
            errors.append("audit_drift")
        else:
            parsed = json.loads(path.read_text(encoding="utf-8"))
            actual = [row["id"] for row in parsed.get("matrix", [])
                      if row.get("m4_required") and row.get("status") != "IMPLEMENTED"]
            row19 = [row for row in parsed.get("matrix", []) if row.get("id") == "WPA-II-19"]
            if actual != AUDIT_ROWS or len(row19) != 1 or row19[0].get("m4_required") is not False:
                errors.append("audit_row_inventory")

    for field, expected in (("gate_bindings", GATES), ("successor_receipt_bindings", RECEIPTS)):
        bindings = value.get(field, {})
        actual = {entry.get("path"): entry.get("sha256") for entry in bindings.values()
                  if isinstance(entry, dict)} if isinstance(bindings, dict) else {}
        if actual != expected or len(bindings) != len(expected):
            errors.append(field)
        elif check_files:
            for raw, digest in expected.items():
                path = resolve(raw)
                if path is None or not path.is_file() or sha256(path) != digest:
                    errors.append(f"binding_drift:{raw}")

    rules = value.get("decision_rules", {})
    expected_rules = {
        "selection_authority": "root product-policy decision only",
        "selection_status_required_before_implementation": "SELECTED_AND_VERSIONED",
        "recommendation_is_selection": False,
        "no_referee_fact_may_be_inferred_from_geometry": True,
        "no_policy_may_be_inferred_from_telemetry": True,
        "no_unsupported_physics_may_be_simulated_by_injected_coordinates": True,
        "narrow_product_seams_preferred_over_global_rules_ui": True,
        "global_rules_decision_ui_product_exposed": False,
        "all_existing_evidence_remains_historical_only_after_linked_source_or_binary_change": True,
    }
    if rules != expected_rules:
        errors.append("decision_rules")

    entries = value.get("entries")
    if not isinstance(entries, list) or len(entries) != len(ENTRY):
        errors.append("entry_inventory")
    else:
        canonical_entries = json.dumps(
            entries, sort_keys=True, separators=(",", ":"), ensure_ascii=False).encode("utf-8")
        if hashlib.sha256(canonical_entries).hexdigest() != ENTRY_CONTRACT_SHA:
            errors.append("entry_contract_digest")
        if [entry.get("row_id") for entry in entries if isinstance(entry, dict)] != list(ENTRY):
            errors.append("entry_order")
        by_row = {entry.get("row_id"): entry for entry in entries if isinstance(entry, dict)}
        if len(by_row) != len(ENTRY) or set(by_row) != set(ENTRY):
            errors.append("entry_ids")
        else:
            for row, expected in ENTRY.items():
                entry = by_row[row]
                status, decision_id, recommendation, referee, physics, alt_count, fixture_count = expected
                rec = entry.get("recommended_shipping_policy", {})
                alternatives = entry.get("mutually_exclusive_alternatives", [])
                fixtures = entry.get("required_fixtures", {})
                if entry.get("audit_status") != status or entry.get("decision_id") != decision_id or \
                        not entry.get("current_capability") or \
                        not isinstance(entry.get("officially_determined"), list) or len(entry["officially_determined"]) < 2 or \
                        not isinstance(entry.get("gap_types"), list) or not entry["gap_types"] or \
                        not entry.get("remaining_policy_referee_physics_gap") or \
                        not isinstance(rec, dict) or rec.get("id") != recommendation or not rec.get("summary") or not rec.get("rationale") or rec.get("selection_status") != "UNSELECTED" or \
                        not isinstance(alternatives, list) or len(alternatives) != alt_count or \
                        not all(isinstance(item, str) and item for item in alternatives) or \
                        len(set(alternatives)) != alt_count or \
                        entry.get("alternatives_selection_status") != "UNSELECTED" or \
                        entry.get("external_referee_required") is not referee or \
                        entry.get("new_physics_required") is not physics or \
                        not isinstance(entry.get("minimal_implementation_allowlist"), list) or len(entry["minimal_implementation_allowlist"]) < 4 or \
                        not isinstance(fixtures, dict) or len(fixtures.get("positive", [])) != fixture_count // 2 or \
                        len(fixtures.get("negative", [])) != fixture_count // 2 or \
                        not entry.get("product_exposure_effect") or not entry.get("fresh_evidence_effect") or \
                        not entry.get("evidence_invalidation") or \
                        entry.get("selection_status") != "UNSELECTED":
                    errors.append(f"entry_contract:{row}")
                if "CMakeLists.txt" in entry.get("minimal_implementation_allowlist", []) or \
                        "src/Billiards/render/modern/modern_renderer.cpp" in entry.get("minimal_implementation_allowlist", []):
                    errors.append(f"entry_allowlist:{row}")

    classes = value.get("closure_classification", {})
    if classes != {
            "policy_independent_telemetry_already_implemented": [
                "WPA-II-06-C canonical break object-cushion contacts with provenance/completeness",
                "WPA-II-17(a) successful cue-ball pocket capture evidence for scratch"],
            "existing_narrow_product_subflows": [
                "WPA-II-06-F current offered legal/foul spot parent actions only",
                "WPA-II-08 current BehindHeadString placement preview/confirm only",
                "WPA-II-09 current Anywhere placement preview/confirm only",
                "WPA-II-17 classified non-break foul to current BIH placement only"],
            "still_requires_root_policy_or_external_referee_decision": [
                "WPA-II-06-C", "WPA-II-06-D", "WPA-II-08", "WPA-II-09",
                "WPA-II-12-3", "WPA-II-12-5-6", "WPA-II-12-7", "WPA-II-15-1",
                "WPA-II-15-2", "WPA-II-16", "WPA-II-17", "WPA-II-19", "WPA-II-21"],
            "requires_new_or_materially_extended_physics_before_automatic_claim": [
                "WPA-II-12-3 true off-table lifecycle",
                "WPA-II-12-5-6 complete full-stroke cue-tip interval lifecycle",
                "WPA-II-12-7 complete push distance/duration lifecycle",
                "WPA-II-16 three-dimensional jump and obstacle-overflight lifecycle",
                "WPA-II-17 cue/object off-table lifecycle"],
            "existing_subflow_is_full_row_completion": False,
            "implemented_telemetry_selects_policy": False}:
        errors.append("closure_classification")

    bundle = value.get("decision_bundle", {})
    expected_ids = [expected[1] for expected in ENTRY.values()]
    if not isinstance(bundle, dict) or bundle.get("all_decision_ids") != expected_ids or \
            bundle.get("all_unselected") is not True or \
            bundle.get("implementation_may_start_from_this_register_alone") is not False or \
            "versioned selection for every decision ID" not in bundle.get("required_root_output", ""):
        errors.append("decision_bundle")

    forbidden = {
        "treating recommendations as selected policy", "editing production source or CMake",
        "editing or promoting audit v5", "rewriting any bound frozen gate or receipt",
        "enabling global rulesDecisionUiProductExposed",
        "adding product exposure before a selected implementation gate",
        "claiming referee input exists where none exists",
        "claiming off-table or jump physics exists where none exists",
        "inheriting old source-bound evidence after implementation changes",
        "Phase4 or Phase5 renderer changes", "GPU, Cocoa, or OpenGL execution",
        "readiness generation or authorization issuance",
    }
    if not exact_list(value.get("forbidden_scope"), forbidden):
        errors.append("forbidden_scope")
    expected_claims = {
        "decision_register_complete": True, "all_audit_v5_m4_unclosed_rows_listed": True,
        "program_priority_wpa_ii_19_listed": True, "recommendations_are_nonbinding": True,
        "any_policy_selected": False, "any_production_implementation_authorized": False,
        "audit_modified_or_promoted": False, "product_exposure_changed": False,
        "global_ui_exposed": False, "cmake_modified": False, "gpu_executed": False,
        "readiness_or_authorization_created": False,
    }
    if value.get("claims") != expected_claims:
        errors.append("claims")
    return errors


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("register", nargs="?", default=str(DEFAULT_REGISTER))
    parser.add_argument("--no-file-checks", action="store_true")
    args = parser.parse_args()
    try:
        value = json.loads(Path(args.register).read_text(encoding="utf-8"))
    except (OSError, json.JSONDecodeError) as exc:
        print(f"INVALID: {exc}")
        return 1
    errors = validate(value, check_files=not args.no_file_checks)
    if errors:
        print("INVALID: " + ", ".join(errors))
        return 1
    print("PASS_M4_WPA_POLICY_REFEREE_INPUT_DECISION_REGISTER")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
