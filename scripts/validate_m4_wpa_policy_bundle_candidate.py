#!/usr/bin/env python3
"""Validate the inert, non-selected M4 WPA recommended policy bundle candidate."""

from __future__ import annotations

import argparse
import hashlib
import json
from pathlib import Path
from typing import Any


ROOT = Path(__file__).resolve().parents[1]
DEFAULT_CANDIDATE = ROOT / "docs/rules/m4-wpa-policy-bundle-candidate-v1.json"
INPUTS = {
    "policy_register": ("docs/rules/m4-wpa-policy-referee-input-decision-register-v1.json", "66e105e968fc49f46cfe7fd117e42b545c6789a87d401f1d657ee263103a31ce"),
    "dependency_register": ("docs/rules/m4-remaining-gap-dependency-critical-path-register-v1.json", "a981b12424ddea9387250b884c53f058e0f987ae5ce88a0e7362df0c0015a684"),
}
DIGESTS = {
    "candidate_contract": "b884f6b903aae33f12cfd9a44546a9af8a2f55eca7cfcbce978b8058702bd3e6",
    "decisions": "4daa3430d7a510f01be835206ab8887e1892593bcceed719ae5116c2b7323b62",
    "root_review_contract": "43310a001118b1ff65f95f9427dad902c6913574d4849019c94380cc388bdec9",
    "claims": "0b6ff3f8842c707506ab1f63fbe56c1f4342f18f2f7b4b1eeb95f90c875fd5e0",
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
    if value.get("schema") != "m4_wpa_policy_bundle_candidate_v1":
        errors.append("schema")
    if value.get("bundle_id") != "m4-wpa-recommended-candidate-2026-08-12-v1":
        errors.append("bundle_id")
    if value.get("candidate_version") != 1:
        errors.append("candidate_version")
    if value.get("status") != "PROPOSED_RECOMMENDED_NOT_ROOT_SELECTED":
        errors.append("status")
    if value.get("approval_state") != "NOT_APPROVED":
        errors.append("approval_state")
    if value.get("root_selected") is not False:
        errors.append("root_selected")
    if value.get("implementation_authorized") is not False:
        errors.append("implementation_authorized")

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

    decisions = value.get("decisions")
    if not isinstance(decisions, list) or len(decisions) != 15:
        errors.append("decision_inventory")
    else:
        pairs = [(item.get("row_id"), item.get("decision_id")) for item in decisions if isinstance(item, dict)]
        if len(pairs) != 15 or len(set(pairs)) != 15:
            errors.append("decision_pair_uniqueness")
        for item in decisions:
            if not isinstance(item, dict):
                continue
            row = item.get("row_id")
            if item.get("candidate_state") != "PROPOSED_RECOMMENDED_NOT_ROOT_SELECTED":
                errors.append(f"item_selected:{row}")
            if not isinstance(item.get("machine_policy_constants"), dict) or not item["machine_policy_constants"]:
                errors.append(f"machine_constants:{row}")
            boundary = item.get("authority_boundary")
            if not isinstance(boundary, dict) or set(boundary) != {"fact_owner", "classification_owner", "product_owner", "forbidden"} or \
                    not all(isinstance(field, str) and field for field in boundary.values()):
                errors.append(f"authority_boundary:{row}")
            lanes = item.get("expected_code_lanes")
            if not isinstance(lanes, list) or not lanes or not all(isinstance(lane, str) and lane for lane in lanes):
                errors.append(f"code_lanes:{row}")
            if not isinstance(item.get("evidence_invalidation"), str) or not item["evidence_invalidation"]:
                errors.append(f"evidence_invalidation:{row}")

    if check_files and isinstance(bindings, dict):
        try:
            policy = json.loads((ROOT / bindings["policy_register"]["path"]).read_text(encoding="utf-8"))
            dependency = json.loads((ROOT / bindings["dependency_register"]["path"]).read_text(encoding="utf-8"))
            policy_entries = policy["entries"]
            dependency_entries = dependency["entries"]
            expected_pairs = [(item["row_id"], item["decision_id"]) for item in policy_entries]
            actual_pairs = [(item["row_id"], item["decision_id"]) for item in decisions]
            if actual_pairs != expected_pairs:
                errors.append("policy_pair_order")
            dependency_pairs = [(item["row_id"], item["decision_id"]) for item in dependency_entries]
            if actual_pairs != dependency_pairs:
                errors.append("dependency_pair_order")
            for candidate, source in zip(decisions, policy_entries):
                if candidate.get("proposed_option_id") != source["recommended_shipping_policy"]["id"]:
                    errors.append(f"not_recommended:{candidate.get('row_id')}")
                if source["recommended_shipping_policy"].get("selection_status") != "UNSELECTED":
                    errors.append(f"source_policy_selected:{candidate.get('row_id')}")
                if candidate.get("evidence_invalidation") != source.get("evidence_invalidation"):
                    errors.append(f"evidence_binding:{candidate.get('row_id')}")
        except (OSError, KeyError, TypeError, json.JSONDecodeError):
            errors.append("bound_register_parse")

    claims = value.get("claims")
    if not isinstance(claims, dict) or claims.get("decision_count") != 15 or \
            claims.get("all_options_equal_bound_register_recommendations") is not True:
        errors.append("claims_summary")
    for key in ("root_review_complete", "any_item_root_selected", "approved", "implementation_authorized",
                "production_modified", "cmake_modified", "readiness_or_authorization_created",
                "gpu_or_cocoa_or_opengl_executed"):
        if not isinstance(claims, dict) or claims.get(key) is not False:
            errors.append(f"false_claim:{key}")
    return errors


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("candidate", nargs="?", default=str(DEFAULT_CANDIDATE))
    parser.add_argument("--no-file-checks", action="store_true")
    args = parser.parse_args()
    try:
        value = json.loads(Path(args.candidate).read_text(encoding="utf-8"))
    except (OSError, json.JSONDecodeError) as exc:
        print(f"INVALID: {exc}")
        return 1
    errors = validate(value, check_files=not args.no_file_checks)
    if errors:
        print("INVALID: " + ", ".join(errors))
        return 1
    print("PASS_M4_WPA_POLICY_BUNDLE_CANDIDATE_PROPOSED_NOT_SELECTED")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
