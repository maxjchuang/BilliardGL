#!/usr/bin/env python3
"""Validate the design-only M4 B2 referee-authority foundation gate."""
from __future__ import annotations

import argparse
import hashlib
import json
from pathlib import Path
from typing import Any

ROOT = Path(__file__).resolve().parents[1]
DEFAULT = ROOT / "docs/rules/m4-b2-authenticated-referee-authority-protocol-foundation-implementation-design-gate-v1.json"
KEYS = {
    "schema", "status", "authority_bindings", "foundation_scope",
    "identity_and_authentication_contract", "request_schema",
    "consumer_policy_registry", "proposal_final_state_machine",
    "ack_journal_and_readback", "implementation_allowlist",
    "required_fixtures", "source_bindings", "build_source_bindings",
    "test_source_bindings", "completion_boundary",
}
DIGESTS = {
    "authority_bindings": "1e8c826800dd63e2901472054da052362113e23dd65257649ce6d6d26cdf2f2c",
    "foundation_scope": "084d95161427c3acf11f80cf3e552f95fcfe5b2458d00c69602cc0b1345434a0",
    "identity_and_authentication_contract": "ff24aa44e92df61c5cbc475235f0efd9fab1f6f9469610107cb98476abbead5e",
    "request_schema": "83359e58858f12c6bc7a8c1c5708c9005936daa84d8116b3c7a55818efdd14f3",
    "consumer_policy_registry": "3850cc38dcf9a47b063f7d7d1542891623eca0acd6e9faec6ff8185152fd194e",
    "proposal_final_state_machine": "a333175c883e0b8106297b27e4a3ba052bd5725182acec8e6349e3382f46fdbc",
    "ack_journal_and_readback": "12ba2323ea411e80f29cc221bf15a49cf9d520a245b7860ad17aaf17fb219a07",
    "implementation_allowlist": "45dc5a97eab540a62b838baf07c565ea4b6e64ffd8bf1f26301840af99521c29",
    "required_fixtures": "926da2ad02c1f12fe8a6a0a98b01d188824f439fb141fdfcf7f2d9d88cebbcc5",
    "source_bindings": "572fb99c13c5ec6d40901ada0e69d85910aa12062f6b809f70a81e4ace4b2021",
    "build_source_bindings": "f1fc18923c9ebe4c270794cb8a38fff5bdb7c6174a842f1adc238db169f065dd",
    "test_source_bindings": "f658ffd28886ec478e15c0a44cb0f8b19449d8931a1fde00e67d6a7566e83524",
    "completion_boundary": "e8d11206cf4ae54297048aa71cd9006d10facea27b1a762809abb516b7164b7a",
}
PROD = [
    "src/Billiards/rules/referee_authority_protocol.h",
    "src/Billiards/rules/referee_authority_protocol.cpp",
    "src/Billiards/game_runtime.h",
    "src/Billiards/game_runtime.cpp",
    "src/Billiards/automation_controller.cpp",
]
TEST = [
    "tests/referee_authority_protocol_tests.cpp",
    "tests/rules_runtime_integration_tests.cpp",
    "tests/automation_controller_tests.cpp",
]


def sha(data: bytes) -> str:
    return hashlib.sha256(data).hexdigest()


def digest(value: Any) -> str:
    return sha(json.dumps(value, sort_keys=True, separators=(",", ":")).encode())


def resolve(raw: Any) -> Path | None:
    if not isinstance(raw, str):
        return None
    path = Path(raw)
    if path.is_absolute() or ".." in path.parts:
        return None
    resolved = (ROOT / path).resolve()
    return resolved if resolved.is_relative_to(ROOT.resolve()) else None


def binding_ok(value: Any) -> bool:
    if not isinstance(value, dict) or set(value) != {"path", "sha256"}:
        return False
    path = resolve(value.get("path"))
    return path is not None and path.is_file() and sha(path.read_bytes()) == value.get("sha256")


def validate(value: Any, check_files: bool = True) -> list[str]:
    if not isinstance(value, dict):
        return ["root_not_object"]
    errors: list[str] = []
    if set(value) != KEYS:
        errors.append("top_level_keys")
    if value.get("schema") != "m4_b2_authenticated_referee_authority_protocol_foundation_implementation_design_gate_v1":
        errors.append("schema")
    if value.get("status") != "DESIGN_COMPLETE_AWAITING_ROOT_EXACT_ALLOWLIST_APPROVAL_NO_PRODUCTION_MUTATION":
        errors.append("status")
    for key, expected in DIGESTS.items():
        if digest(value.get(key)) != expected:
            errors.append("contract_digest:" + key)

    authority = value.get("authority_bindings", {})
    authority = authority if isinstance(authority, dict) else {}
    selected = authority.get("root_selected_policy", {})
    b1 = authority.get("b1_completion_receipt", {})
    critical = authority.get("critical_path_successor", {})
    if selected.get("sha256") != "fff40b72776b4381eae82044421432c139606d300f7214fb87e63e36eeaa770f" or selected.get("authorized_stage") != "B2_TESTS_FIRST_CPU_REFEREE_PROTOCOL_FOUNDATION_ONLY":
        errors.append("root_authority")
    if b1.get("sha256") != "a66ca9b31fc75607cc82638a3ddd96075793f748e708aaa361086b5901fce2de":
        errors.append("b1_lineage")
    if critical.get("sha256") != "f2cc41f2c00ea5da3eb01cb12c2d1f76acea265eeed688eabc89b0302a98ea53":
        errors.append("critical_lineage")
    if check_files:
        for binding, label in ((selected, "root"), (b1, "b1"), (critical, "critical")):
            if not binding_ok({"path": binding.get("path"), "sha256": binding.get("sha256")}):
                errors.append(label + "_drift")

    scope = value.get("foundation_scope", {})
    scope = scope if isinstance(scope, dict) else {}
    if scope.get("protocol_id") != "authenticated_referee_authority_v1":
        errors.append("protocol")
    for key in (
        "specific_wpa_rule_row_consumer_implemented", "specific_foul_or_rerack_semantics_implemented",
        "visual_or_player_ui_implemented", "global_player_rules_ui_exposed",
        "referee_declaration_is_physics_fact", "physics_fact_may_authenticate_referee",
    ):
        if scope.get(key) is not False:
            errors.append("scope_false:" + key)

    identity = value.get("identity_and_authentication_contract", {})
    identity = identity if isinstance(identity, dict) else {}
    if identity.get("authority_roles") != ["REFEREE", "PLAYER_0", "PLAYER_1"]:
        errors.append("roles")
    if identity.get("no_embedded_secret_or_test_bypass_in_production") is not True:
        errors.append("auth_bypass")
    if "trusted host" not in str(identity.get("authenticator_boundary")) or "proof covers protocol/schema" not in str(identity.get("signature_binding")):
        errors.append("authentication_boundary")

    request = value.get("request_schema", {})
    request = request if isinstance(request, dict) else {}
    required = request.get("required_fields", [])
    if not isinstance(required, list) or len(required) != 17 or len(set(required)) != 17:
        errors.append("request_fields")
    if request.get("operation_values") != ["PROPOSE", "FINAL_DECLARE", "CANCEL_PROPOSAL"]:
        errors.append("operations")
    if "duplicate keys" not in str(request.get("payload")) or "current_authority_tick <= expires_at_authority_tick" not in str(request.get("expiry")):
        errors.append("schema_fail_closed")

    registry = value.get("consumer_policy_registry", {})
    registry = registry if isinstance(registry, dict) else {}
    if len(registry.get("required_policy_fields", [])) != 7 or registry.get("specific_wpa_kinds_registered_by_this_gate") is not False:
        errors.append("consumer_registry")
    if registry.get("foundation_test_kinds_only") != ["TEST_REFEREE_ONLY_DECLARATION", "TEST_PLAYER_ALLOWED_DECLARATION"]:
        errors.append("test_kinds")

    machine = value.get("proposal_final_state_machine", {})
    machine = machine if isinstance(machine, dict) else {}
    if machine.get("partial_commit_allowed") is not False or "only FINAL_DECLARE ACK" not in str(machine.get("separation")):
        errors.append("proposal_final")
    if "no GameRuntime rule" not in str(machine.get("consumer_boundary")) or "all roll back" not in str(machine.get("atomicity")):
        errors.append("atomic_boundary")

    readback = value.get("ack_journal_and_readback", {})
    readback = readback if isinstance(readback, dict) else {}
    if len(readback.get("ack_fields", [])) != 18 or "zero additional" not in str(readback.get("exact_retry")):
        errors.append("ack")
    if "never credentials" not in str(readback.get("automation_readback")) or "no secret/proof" not in str(readback.get("audit_journal")):
        errors.append("readback_secret")

    allow = value.get("implementation_allowlist", {})
    allow = allow if isinstance(allow, dict) else {}
    if allow.get("production_files") != PROD or allow.get("test_files") != TEST:
        errors.append("allowlist")
    if allow.get("build_files") != ["CMakeLists.txt"] or allow.get("cmake_change_allowed") is not True:
        errors.append("build_allowlist")
    if allow.get("visual_ui_renderer_product_session_edit_allowed") is not False or allow.get("physics_shotfacts_adjudicator_rack_executor_edit_allowed") is not False:
        errors.append("forbidden_files")

    fixtures = value.get("required_fixtures", {})
    fixtures = fixtures if isinstance(fixtures, dict) else {}
    if len(fixtures.get("positive", [])) != 7 or len(fixtures.get("negative", [])) != 11 or fixtures.get("headless_cpu_only") is not True:
        errors.append("fixtures")

    sources = value.get("source_bindings")
    builds = value.get("build_source_bindings")
    tests = value.get("test_source_bindings")
    if not isinstance(sources, list) or [r.get("path") for r in sources if isinstance(r, dict)] != PROD[2:]:
        errors.append("source_bindings")
    if not isinstance(builds, list) or [r.get("path") for r in builds if isinstance(r, dict)] != ["CMakeLists.txt"]:
        errors.append("build_bindings")
    if not isinstance(tests, list) or [r.get("path") for r in tests if isinstance(r, dict)] != TEST[1:]:
        errors.append("test_bindings")
    if check_files:
        for rows, label in ((sources, "source"), (builds, "build"), (tests, "test")):
            if isinstance(rows, list):
                for row in rows:
                    if not binding_ok(row):
                        errors.append(label + "_drift")

    boundary = value.get("completion_boundary", {})
    boundary = boundary if isinstance(boundary, dict) else {}
    if boundary.get("implementation_authorized_by_this_gate") is not False or boundary.get("root_revalidation_required_before_tests_first_patch") is not True:
        errors.append("authorization_boundary")
    for key in ("specific_rule_consumers_authorized", "b2_complete_from_foundation", "m4_complete", "production_modified", "cmake_modified", "gpu_ui_readiness_or_authorization"):
        if boundary.get(key) is not False:
            errors.append("false_completion:" + key)
    return errors


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("gate", nargs="?", default=str(DEFAULT))
    parser.add_argument("--no-file-checks", action="store_true")
    args = parser.parse_args()
    try:
        value = json.loads(Path(args.gate).read_text())
    except (OSError, json.JSONDecodeError) as exc:
        print("INVALID:", exc)
        return 1
    errors = validate(value, not args.no_file_checks)
    if errors:
        print("INVALID:", ", ".join(errors))
        return 1
    print("PASS_M4_B2_AUTHENTICATED_REFEREE_AUTHORITY_PROTOCOL_FOUNDATION_IMPLEMENTATION_DESIGN_GATE")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
