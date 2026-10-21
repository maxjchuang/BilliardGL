#!/usr/bin/env python3
"""Validate the versioned ROOT_SELECTED M4 WPA policy bundle without implying completion."""

from __future__ import annotations

import argparse
import hashlib
import json
from pathlib import Path
from typing import Any


ROOT = Path(__file__).resolve().parents[1]
DEFAULT_BUNDLE = ROOT / "docs/rules/m4-wpa-policy-bundle-root-selected-v1.json"
INPUTS = {
    "candidate": ("docs/rules/m4-wpa-policy-bundle-candidate-v1.json", "0cc6a5f319eaed7f7b46fb6c5579982592c504e89d57b04183bd749be82f7da9"),
    "policy_register": ("docs/rules/m4-wpa-policy-referee-input-decision-register-v1.json", "66e105e968fc49f46cfe7fd117e42b545c6789a87d401f1d657ee263103a31ce"),
    "critical_path_register": ("docs/rules/m4-remaining-gap-dependency-critical-path-register-v1.json", "a981b12424ddea9387250b884c53f058e0f987ae5ce88a0e7362df0c0015a684"),
}
DIGESTS = {
    "root_pm_selection_directive": "9b18a437b78b508034330db05fca52052c6fb9664d4943aeb11f8c67e166b31c",
    "implementation_permissions": "2beb4cd843f97abafb683f4b1de26f6e3283fb6853c96ff7f9e9471a06bb0aa7",
    "selection_semantics": "c4a10c286d6c90f4860cdfc81d7780c24c1ed5540930d7d6dcfedd3805639d3d",
    "decisions": "9656dcb5ea99c7c0f3c2bbf53c98fff98603ba161649ad15ad0b750cc80e2630",
    "claims": "969081fb4fb1afcf5cd9dde8587dcbbc34ff079731c13e60bbd4894f87cb87db",
}


def sha256_bytes(data: bytes) -> str:
    return hashlib.sha256(data).hexdigest()


def canonical_digest(value: Any) -> str:
    raw = json.dumps(value, sort_keys=True, separators=(",", ":"), ensure_ascii=False).encode("utf-8")
    return sha256_bytes(raw)


def resolve(raw: str) -> Path | None:
    path = Path(raw)
    if path.is_absolute() or ".." in path.parts:
        return None
    result = (ROOT / path).resolve()
    return result if result.is_relative_to(ROOT.resolve()) else None


def validate(value: Any, check_files: bool = True) -> list[str]:
    if not isinstance(value, dict):
        return ["root_not_object"]
    errors: list[str] = []
    if value.get("schema") != "m4_wpa_policy_bundle_root_selected_v1":
        errors.append("schema")
    if value.get("bundle_id") != "m4-wpa-root-selected-2026-08-12-v1":
        errors.append("bundle_id")
    if value.get("selection_version") != 1:
        errors.append("selection_version")
    if value.get("status") != "ROOT_SELECTED" or value.get("selected_by") != "ROOT_PM" or \
            value.get("selected_on") != "2026-08-12":
        errors.append("selection_identity")

    bindings = value.get("input_bindings")
    if not isinstance(bindings, dict) or set(bindings) != set(INPUTS):
        errors.append("input_binding_inventory")
    else:
        for key, (raw, digest) in INPUTS.items():
            if bindings.get(key) != {"path": raw, "sha256": digest}:
                errors.append(f"input_binding:{key}")
                continue
            if check_files:
                path = resolve(raw)
                if path is None or not path.is_file() or sha256_bytes(path.read_bytes()) != digest:
                    errors.append(f"input_drift:{key}")

    for key, digest in DIGESTS.items():
        if key not in value or canonical_digest(value.get(key)) != digest:
            errors.append(f"contract_digest:{key}")

    directive = value.get("root_pm_selection_directive")
    if not isinstance(directive, dict) or not isinstance(directive.get("semantic_text"), str) or \
            sha256_bytes(directive.get("semantic_text", "").encode("utf-8")) != directive.get("semantic_sha256") or \
            directive.get("semantic_sha256") != "a95efb1b7467fd2b5c01ed02d05bdcd97ac1a2021aa82b1e6254322abf58663c":
        errors.append("root_directive_semantics")

    decisions = value.get("decisions")
    if not isinstance(decisions, list) or len(decisions) != 15:
        errors.append("decision_inventory")
    else:
        pairs = [(item.get("row_id"), item.get("decision_id")) for item in decisions if isinstance(item, dict)]
        if len(pairs) != 15 or len(set(pairs)) != 15:
            errors.append("decision_pair_uniqueness")
        b1 = 0
        b2 = 0
        guard = 0
        for item in decisions:
            if not isinstance(item, dict):
                continue
            row = item.get("row_id")
            if item.get("root_selected") is not True:
                errors.append(f"not_root_selected:{row}")
            if item.get("row_closed_by_selection") is not False:
                errors.append(f"false_row_close:{row}")
            stage = item.get("authorized_stage")
            if stage == "B1_TESTS_FIRST_CPU_DIRECT_CODE":
                b1 += 1
                if item.get("row_implementation_allowed") is not True:
                    errors.append(f"b1_not_allowed:{row}")
            elif stage == "B2_TESTS_FIRST_CPU_REFEREE_PROTOCOL_FOUNDATION_ONLY":
                b2 += 1
                if item.get("row_implementation_allowed") is not False:
                    errors.append(f"b2_row_overauthorized:{row}")
            elif stage == "NO_PRODUCTION_IMPLEMENTATION_CURRENT_GUARD_REMAINS":
                guard += 1
                if row != "WPA-II-16" or item.get("row_implementation_allowed") is not False:
                    errors.append("jump_guard_scope")
            else:
                errors.append(f"authorized_stage:{row}")
        if (b1, b2, guard) != (6, 8, 1):
            errors.append("stage_counts")

    if check_files and isinstance(bindings, dict):
        try:
            candidate = json.loads((ROOT / bindings["candidate"]["path"]).read_text(encoding="utf-8"))
            policy = json.loads((ROOT / bindings["policy_register"]["path"]).read_text(encoding="utf-8"))
            critical = json.loads((ROOT / bindings["critical_path_register"]["path"]).read_text(encoding="utf-8"))
            candidate_items = candidate["decisions"]
            policy_items = policy["entries"]
            critical_items = critical["entries"]
            selected_pairs = [(item["row_id"], item["decision_id"]) for item in decisions]
            if selected_pairs != [(item["row_id"], item["decision_id"]) for item in candidate_items] or \
                    selected_pairs != [(item["row_id"], item["decision_id"]) for item in policy_items] or \
                    selected_pairs != [(item["row_id"], item["decision_id"]) for item in critical_items]:
                errors.append("bound_order")
            for selected, proposed, source in zip(decisions, candidate_items, policy_items):
                if selected.get("selected_option_id") != proposed.get("proposed_option_id") or \
                        selected.get("selected_option_id") != source["recommended_shipping_policy"]["id"]:
                    errors.append(f"selected_option:{selected.get('row_id')}")
                if selected.get("candidate_decision_sha256") != canonical_digest(proposed):
                    errors.append(f"candidate_decision_binding:{selected.get('row_id')}")
        except (OSError, KeyError, TypeError, json.JSONDecodeError):
            errors.append("bound_artifact_parse")

    permissions = value.get("implementation_permissions")
    if not isinstance(permissions, dict):
        errors.append("permissions")
    else:
        for key in ("B1_direct_code_tests_first_cpu_implementation_allowed",
                    "B2_authenticated_referee_authority_protocol_tests_first_cpu_implementation_allowed",
                    "B3_requires_independent_versioned_implementation_design_gate_per_lane",
                    "production_work_may_start_only_after_this_bundle_validator_and_mutations_pass"):
            if permissions.get(key) is not True:
                errors.append(f"permission_missing:{key}")
        for key in ("B3_D5_cue_contact_implementation_allowed", "B3_D6_off_table_physics_implementation_allowed",
                    "B3_D7_jump_3d_physics_implementation_allowed", "gpu_execution_allowed",
                    "readiness_generation_allowed", "authorization_issuance_allowed", "cmake_change_allowed",
                    "phase4_or_phase5_renderer_change_allowed"):
            if permissions.get(key) is not False:
                errors.append(f"overauthorization:{key}")

    claims = value.get("claims")
    if not isinstance(claims, dict) or claims.get("decision_count") != 15 or \
            claims.get("all_15_recommended_options_root_selected") is not True or \
            claims.get("b0_policy_selection_complete") is not True:
        errors.append("claims_selection")
    for key in ("any_row_closed_by_selection", "profile_complete", "m4_complete", "wpa_ii_16_implemented",
                "wpa_ii_17_full_row_closed", "d5_d6_d7_implementation_authorized", "gpu_execution_authorized",
                "readiness_generation_authorized", "capture_authorization_issuance_authorized",
                "production_modified_by_selection_freeze", "cmake_modified"):
        if not isinstance(claims, dict) or claims.get(key) is not False:
            errors.append(f"false_completion_or_authorization:{key}")
    return errors


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("bundle", nargs="?", default=str(DEFAULT_BUNDLE))
    parser.add_argument("--no-file-checks", action="store_true")
    args = parser.parse_args()
    try:
        value = json.loads(Path(args.bundle).read_text(encoding="utf-8"))
    except (OSError, json.JSONDecodeError) as exc:
        print(f"INVALID: {exc}")
        return 1
    errors = validate(value, check_files=not args.no_file_checks)
    if errors:
        print("INVALID: " + ", ".join(errors))
        return 1
    print("PASS_M4_WPA_POLICY_BUNDLE_ROOT_SELECTED_V1")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
