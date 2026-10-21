#!/usr/bin/env python3
"""Validate the design-only M4 remaining-gap dependency/critical-path register."""

from __future__ import annotations

import argparse
import hashlib
import json
from pathlib import Path
from typing import Any


ROOT = Path(__file__).resolve().parents[1]
DEFAULT_REGISTER = ROOT / "docs/rules/m4-remaining-gap-dependency-critical-path-register-v1.json"
INPUTS = {
    "policy_register": ("docs/rules/m4-wpa-policy-referee-input-decision-register-v1.json", "66e105e968fc49f46cfe7fd117e42b545c6789a87d401f1d657ee263103a31ce"),
    "audit_v5": ("docs/rules/phase5-wpa-heyball-as-built-reconciliation-audit-v5.json", "2811958f1205d9998b40060b83c577f91359d2164e1ac0eaa9ccc4cac2ad787a"),
    "m4_beta_verification_matrix": ("docs/m4-beta-verification-matrix.md", "0f1f480ace3356fabc84ea9d70919fbd6dcc9714d3c53fba0861323d04815783"),
    "program_status": ("docs/visual-gameplay-program-status.md", "2f76b1a96681e8b6b0e95bb4fd97e0fcd6d030534bf5961c6df7c9000bfa0c08"),
}
ROWS = [
    "WPA-II-06-C", "WPA-II-06-D", "WPA-II-06-E", "WPA-II-06-F",
    "WPA-II-08", "WPA-II-09", "WPA-II-12-3", "WPA-II-12-5-6",
    "WPA-II-12-7", "WPA-II-15-1", "WPA-II-15-2", "WPA-II-16",
    "WPA-II-17", "WPA-II-19", "WPA-II-21",
]
DECISIONS = [
    "M4-POL-06C-BREAK-THRESHOLD", "M4-POL-06D-HALF-BALL-HISTORY",
    "M4-PROD-06E-BREAK-FOUL-CHOICES", "M4-PROD-06F-EIGHT-ON-BREAK-FULL-CHOICES",
    "M4-POL-08-HEAD-STRING-CROSSING", "M4-POL-09-TIP-TOUCH-SHOT-BOUNDARY",
    "M4-POL-12-3-OBJECT-OFF-TABLE-CONFLICT", "M4-POL-12-5-6-DOUBLE-HIT",
    "M4-POL-12-7-PUSH", "M4-POL-15-1-FROZEN-CUE-OBJECT",
    "M4-POL-15-2-FROZEN-CUSHION", "M4-PROD-16-JUMP-CAPABILITY",
    "M4-PROD-17-GENERAL-FOUL-COVERAGE", "M4-POL-19-GROUP-CONFUSION",
    "M4-POL-21-STALEMATE",
]
DIGESTS = {
    "classification_contract": "2f421cf6f813fcf194f8843a62453ec01dc73b9fccda97357cecd3e3797da675",
    "dependency_nodes": "5058106bdd73a2837cbe0878a40977e7c5cfbdae8c5fd7fc7d01c9b371bde9ed",
    "minimum_execution_batches": "d141ab8599e14cef4d9f0db6030a76ff12337192c8057efc03aeb30d5aac1c0a",
    "entries": "e0684d57c9a93a66d2bf403ddd33a607b17d9ed1f49b161553a529d32c3d322f",
    "critical_path_summary": "bdb0d10699c459ea717e2e391f81e4c7bb6e0981932e08166c92286053f32478",
    "claims": "4cad112388f83322ebcb7f8c71650c426c500317ff0c252aa0f0b781e1260156",
}


def sha256_bytes(data: bytes) -> str:
    return hashlib.sha256(data).hexdigest()


def sha256_file(path: Path) -> str:
    return sha256_bytes(path.read_bytes())


def canonical_digest(value: Any) -> str:
    data = json.dumps(value, sort_keys=True, separators=(",", ":"), ensure_ascii=False).encode("utf-8")
    return sha256_bytes(data)


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
    if value.get("schema") != "m4_remaining_gap_dependency_critical_path_register_v1":
        errors.append("schema")
    if value.get("status") != "FINAL_DEPENDENCY_REGISTER_ALL_POLICY_SELECTIONS_UNSELECTED":
        errors.append("status")
    if value.get("scope") != "Design-only dependency and execution ordering for the 15 frozen M4 WPA remaining-gap rows.":
        errors.append("scope")

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
                if path is None or not path.is_file() or sha256_file(path) != digest:
                    errors.append(f"input_drift:{key}")

    for key, digest in DIGESTS.items():
        if key not in value or canonical_digest(value.get(key)) != digest:
            errors.append(f"contract_digest:{key}")

    entries = value.get("entries")
    if not isinstance(entries, list) or len(entries) != 15:
        errors.append("entry_inventory")
    else:
        if [entry.get("row_id") for entry in entries if isinstance(entry, dict)] != ROWS:
            errors.append("row_order")
        if [entry.get("decision_id") for entry in entries if isinstance(entry, dict)] != DECISIONS:
            errors.append("decision_order")
        if len({entry.get("row_id") for entry in entries if isinstance(entry, dict)}) != 15:
            errors.append("row_uniqueness")
        if len({entry.get("decision_id") for entry in entries if isinstance(entry, dict)}) != 15:
            errors.append("decision_uniqueness")
        for entry in entries:
            if not isinstance(entry, dict):
                continue
            if entry.get("root_policy_selection") != "REQUIRED_UNSELECTED":
                errors.append(f"policy_selected:{entry.get('row_id')}")
            if not isinstance(entry.get("depends_on"), list) or "D1-ROOT-POLICY-BUNDLE" not in entry["depends_on"]:
                errors.append(f"policy_dependency:{entry.get('row_id')}")
            for gate in ("gpu", "ta", "human"):
                if entry.get(gate) not in value.get("classification_contract", {}).get("evidence_gate_values", []):
                    errors.append(f"evidence_gate:{entry.get('row_id')}:{gate}")

    batches = value.get("minimum_execution_batches")
    if isinstance(batches, list):
        for batch in batches:
            if not isinstance(batch, dict) or batch.get("may_start_now") is not False:
                errors.append("batch_authorization")
    claims = value.get("claims")
    if not isinstance(claims, dict) or claims.get("row_count") != 15 or \
            claims.get("all_policy_selections_unselected") is not True or \
            claims.get("deferred_rows_are_not_closed") is not True:
        errors.append("claims_summary")
    for key in ("production_modified", "cmake_modified", "readiness_or_authorization_created",
                "gpu_or_cocoa_or_opengl_executed", "audit_v5_modified_or_promoted",
                "implementation_authorized"):
        if not isinstance(claims, dict) or claims.get(key) is not False:
            errors.append(f"false_claim:{key}")

    if check_files and isinstance(bindings, dict) and "policy_register" in bindings:
        try:
            source = json.loads((ROOT / bindings["policy_register"]["path"]).read_text(encoding="utf-8"))
            source_pairs = [(item["row_id"], item["decision_id"]) for item in source["entries"]]
            if source_pairs != list(zip(ROWS, DECISIONS)):
                errors.append("policy_register_pair_drift")
        except (OSError, KeyError, TypeError, json.JSONDecodeError):
            errors.append("policy_register_parse")
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
    print("PASS_M4_REMAINING_GAP_DEPENDENCY_CRITICAL_PATH_REGISTER_FINAL")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
