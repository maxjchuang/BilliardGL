#!/usr/bin/env python3
"""Validate the CPU-only WPA-II-06-C selected threshold implementation receipt."""

from __future__ import annotations

import argparse
import hashlib
import json
from pathlib import Path
from typing import Any


ROOT = Path(__file__).resolve().parents[1]
DEFAULT_RECEIPT = ROOT / "docs/rules/wpa-ii-06-c-selected-break-threshold-implementation-receipt-v1.json"
DIGESTS = {
    "selection_binding": "1eb82e88108fa7fd36019634e0714fb8040eb5003c223aeff32b0342c51b6c24",
    "implementation_scope": "37af3c73ed1f36ed1ca0bb9625f2338b9a0fad6d66ea9cb07a4bb7249f9b32a1",
    "atomic_and_failure_contract": "c92a293930adccdb7b48f1a50df52b03bb5611ef1a2973f9a70bf438b967531b",
    "source_bindings": "37ccfe787ed82ceb91aaa0315ead06efe0a2032eab448b27edbbfefef35a80f9",
    "cpu_build": "75df3f3703ce1f1ec246e5c3a5198f92f65101b000affe3d33a9e4c51df2fca9",
    "evidence_invalidation": "496b46333e0a34bb6dd297a214028545783dee98d737d0d550abddc0a8831e52",
    "claims": "9abcbc7bb168b15bcda3e6178320ce695616ac4096eba7d77889264c7b749e8d",
}


def sha256(data: bytes) -> str:
    return hashlib.sha256(data).hexdigest()


def canonical_digest(value: Any) -> str:
    raw = json.dumps(value, sort_keys=True, separators=(",", ":"), ensure_ascii=False).encode("utf-8")
    return sha256(raw)


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
    if value.get("schema") != "wpa_ii_06_c_selected_break_threshold_implementation_receipt_v1":
        errors.append("schema")
    if value.get("status") != "SELECTED_THRESHOLD_IMPLEMENTED_CPU_VERIFIED_ROW_REMAINS_OPEN":
        errors.append("status")
    for key, digest in DIGESTS.items():
        if key not in value or canonical_digest(value.get(key)) != digest:
            errors.append(f"contract_digest:{key}")

    selection = value.get("selection_binding")
    if not isinstance(selection, dict) or selection.get("decision_id") != "M4-POL-06C-BREAK-THRESHOLD" or \
            selection.get("selected_option_id") != "STRAIGHT_CUSHION_VELOCITY_IMPULSE_OBJECT_POCKET":
        errors.append("selection_contract")

    scope = value.get("implementation_scope")
    if not isinstance(scope, dict) or scope.get("row_id") != "WPA-II-06-C" or \
            scope.get("authorized_stage") != "B1_TESTS_FIRST_CPU_DIRECT_CODE":
        errors.append("implementation_scope")
    facts = scope.get("fact_contract", {}) if isinstance(scope, dict) else {}
    if not isinstance(facts, dict) or facts.get("canonical_contact_rows_consumed_by_rules_adjudicator") is not True or \
            facts.get("product_reclassifies_contacts") is not False or \
            facts.get("direct_or_synthetic_scalar_may_open_runtime_product_offer") is not False:
        errors.append("fact_authority_boundary")

    sources = value.get("source_bindings")
    if not isinstance(sources, dict):
        errors.append("source_bindings")
    elif check_files:
        for key, binding in sources.items():
            if not isinstance(binding, dict):
                errors.append(f"source_binding:{key}")
                continue
            path = resolve(binding.get("path", ""))
            if path is None or not path.is_file() or sha256(path.read_bytes()) != binding.get("sha256"):
                errors.append(f"source_drift:{key}")

    cpu = value.get("cpu_build")
    binaries = cpu.get("binaries", []) if isinstance(cpu, dict) else []
    if not isinstance(binaries, list) or len(binaries) != 8 or \
            any(not isinstance(item, dict) or item.get("exit_code") != 0 for item in binaries):
        errors.append("cpu_results")
    if not isinstance(cpu, dict) or cpu.get("gpu_cocoa_or_opengl_executed") is not False or \
            cpu.get("cmake_source_modified") is not False or \
            cpu.get("ctest_rules_subset") != {"passed": 7, "failed": 0}:
        errors.append("cpu_boundary")

    claims = value.get("claims")
    if not isinstance(claims, dict) or claims.get("selected_threshold_policy_implemented") is not True or \
            claims.get("canonical_authoritative_product_classification_enforced") is not True or \
            claims.get("cpu_verified") is not True:
        errors.append("positive_claims")
    for key in ("wpa_ii_06_c_row_closed", "product_choice_seam_complete",
                "audit_v5_modified_or_promoted", "profile_complete", "m4_complete", "ui_modified",
                "cmake_modified", "phase4_or_phase5_renderer_modified", "gpu_executed",
                "readiness_or_authorization_created"):
        if not isinstance(claims, dict) or claims.get(key) is not False:
            errors.append(f"false_completion:{key}")

    if check_files and isinstance(selection, dict):
        try:
            selected_path = resolve(selection["path"])
            if selected_path is None or sha256(selected_path.read_bytes()) != selection["sha256"]:
                errors.append("selection_drift")
            else:
                selected = json.loads(selected_path.read_text(encoding="utf-8"))
                row = next(item for item in selected["decisions"] if item["row_id"] == "WPA-II-06-C")
                if row["selected_option_id"] != selection["selected_option_id"] or \
                        row["root_selected"] is not True or row["authorized_stage"] != "B1_TESTS_FIRST_CPU_DIRECT_CODE":
                    errors.append("selection_row_drift")
        except (OSError, KeyError, StopIteration, TypeError, json.JSONDecodeError):
            errors.append("selection_parse")
    return errors


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("receipt", nargs="?", default=str(DEFAULT_RECEIPT))
    parser.add_argument("--no-file-checks", action="store_true")
    args = parser.parse_args()
    try:
        value = json.loads(Path(args.receipt).read_text(encoding="utf-8"))
    except (OSError, json.JSONDecodeError) as exc:
        print(f"INVALID: {exc}")
        return 1
    errors = validate(value, check_files=not args.no_file_checks)
    if errors:
        print("INVALID: " + ", ".join(errors))
        return 1
    print("PASS_WPA_II_06_C_SELECTED_BREAK_THRESHOLD_IMPLEMENTATION")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
