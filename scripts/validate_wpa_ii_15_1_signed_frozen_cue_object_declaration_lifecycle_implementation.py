#!/usr/bin/env python3
from __future__ import annotations
import argparse, hashlib, json
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
DEFAULT = ROOT / "docs/rules/wpa-ii-15-1-signed-frozen-cue-object-declaration-lifecycle-implementation-receipt-v1.json"
PROD = ["CMakeLists.txt", "src/Billiards/rules/frozen_ball_declaration.h", "src/Billiards/rules/frozen_ball_declaration.cpp", "src/Billiards/game_runtime.h", "src/Billiards/game_runtime.cpp", "src/Billiards/automation_controller.cpp"]
TEST = ["tests/frozen_ball_declaration_tests.cpp", "tests/rules_runtime_integration_tests.cpp", "tests/automation_controller_tests.cpp"]
COMMANDS = ["rules.frozen_cue_object_propose", "rules.frozen_cue_object_final", "rules.frozen_cue_object_cancel", "get_frozen_cue_object_declaration_state"]

def sha(data: bytes) -> str: return hashlib.sha256(data).hexdigest()
def resolve(path: object):
    if not isinstance(path, str): return None
    candidate = Path(path)
    if candidate.is_absolute() or ".." in candidate.parts: return None
    resolved = (ROOT / candidate).resolve()
    return resolved if resolved.is_relative_to(ROOT.resolve()) else None
def binding(row: object) -> bool:
    if not isinstance(row, dict) or set(row) != {"path", "sha256"}: return False
    path = resolve(row.get("path"))
    return path is not None and path.is_file() and sha(path.read_bytes()) == row.get("sha256")

def validate(value: object, check_files: bool = True) -> list[str]:
    if not isinstance(value, dict): return ["root"]
    errors: list[str] = []
    required = {"schema", "status", "authority_bindings", "tests_first_record", "implemented_contract", "automation_headless", "fixture_matrix", "source_bindings", "test_bindings", "cpu_verification", "completion_boundary"}
    if set(value) != required: errors.append("keys")
    if value.get("schema") != "wpa_ii_15_1_signed_frozen_cue_object_declaration_lifecycle_implementation_receipt_v1": errors.append("schema")
    if value.get("status") != "PARTIAL_LIFECYCLE_CPU_VERIFIED_HEADLESS_D5_AND_AUDIT_REMAIN_OPEN": errors.append("status")
    auth = value.get("authority_bindings", {})
    gate = auth.get("implementation_design_gate", {}) if isinstance(auth, dict) else {}
    policy = auth.get("root_selected_policy", {}) if isinstance(auth, dict) else {}
    foundation = auth.get("b2_foundation_receipt", {}) if isinstance(auth, dict) else {}
    if gate.get("sha256") != "7b307f3a703e53a2cb9512a26053a7d038e8321e0a518114cae4ce165183e035": errors.append("gate")
    if policy.get("selected_option_id") != "SIGNED_PRE_SHOT_FROZEN_DECLARATION" or policy.get("row_id") != "WPA-II-15-1": errors.append("policy")
    if foundation.get("sha256") != "1f654b7e407d34b362430fb83b53f00dfd3598742c5553c815231517727f914b": errors.append("foundation")
    if check_files:
        for label, row in (("gate", gate), ("policy", policy), ("foundation", foundation)):
            if not binding({"path": row.get("path"), "sha256": row.get("sha256")}): errors.append(label + "_drift")
    red = value.get("tests_first_record", {})
    if red.get("red_observed") is not True or red.get("red_exit_code") != 1 or "no such file" not in str(red.get("red_failure")) or red.get("green_after_minimal_production_patch") is not True: errors.append("red_green")
    contract = value.get("implemented_contract", {})
    if contract.get("states") != ["INACTIVE", "PROPOSAL_OPEN", "DECLARATION_ACTIVE", "DECLARATION_CONSUMED", "CANCELLED", "INVALIDATED"]: errors.append("states")
    for key in ("exact_proposal_final_cancel_retry_zero_delta", "authoritative_parent_drift_invalidates_before_shot", "reset_rules_reset_and_rerack_invalidate"):
        if contract.get(key) is not True: errors.append("contract_true:" + key)
    for key in ("zero_rejected_unsupported_or_failed_launch_consumes", "consumed_declaration_reactivates", "second_shot_reuses_declaration", "signed_payload_synthesizes_physics_shotfacts_or_foul", "visible_or_global_rules_ui_exposed"):
        if contract.get(key) is not False: errors.append("contract_false:" + key)
    automation = value.get("automation_headless", {})
    if automation.get("capability") != "wpa_ii_15_1_signed_frozen_declaration_v1" or automation.get("commands") != COMMANDS or automation.get("request_exact_field_count") != 17 or automation.get("readback_omits") != ["credential_id", "authentication_proof", "payload_canonical"] or automation.get("headless_only") is not True: errors.append("automation")
    fixtures = value.get("fixture_matrix", {})
    positive, negative = fixtures.get("positive", []), fixtures.get("negative", [])
    if len(positive) != 10 or len(negative) != 17 or len(set(positive + negative)) != 27: errors.append("fixtures")
    sources, tests = value.get("source_bindings"), value.get("test_bindings")
    if not isinstance(sources, list) or [x.get("path") for x in sources if isinstance(x, dict)] != PROD: errors.append("sources")
    if not isinstance(tests, list) or [x.get("path") for x in tests if isinstance(x, dict)] != TEST: errors.append("tests")
    if check_files:
        for rows, label in ((sources, "source"), (tests, "test")):
            if isinstance(rows, list):
                for row in rows:
                    if not binding(row): errors.append(label + "_drift")
    cpu = value.get("cpu_verification", {})
    blocked = cpu.get("blocked_target", {}) if isinstance(cpu, dict) else {}
    binaries = cpu.get("binary_bindings", []) if isinstance(cpu, dict) else []
    if cpu.get("passed_count") != 9 or cpu.get("planned_count") != 10 or len(cpu.get("passed_targets", [])) != 9 or len(binaries) != 9 or any(not isinstance(row, dict) or row.get("exit_code") != 0 for row in binaries) or cpu.get("all_executed_targets_exit_code") != 0 or cpu.get("gpu_cocoa_opengl_executed") is not False or blocked.get("status") != "NOT_RUN_LINK_BLOCKED_EXTERNAL" or blocked.get("in_allowlist_to_fix") is not False: errors.append("cpu")
    elif check_files:
        for row in binaries:
            if not binding({"path": row.get("path"), "sha256": row.get("sha256")}): errors.append("binary_drift")
    boundary = value.get("completion_boundary", {})
    if boundary.get("signed_declaration_partial_lifecycle_implemented") is not True: errors.append("partial")
    for key in ("d5_frozen_contact_classification_complete", "wpa_ii_15_1_audit_row_promoted", "b2_complete", "m4_complete", "shotfacts_rules_physics_topology_or_cue_contact_modified", "visible_product_ui_implemented", "global_rules_ui_exposed", "gpu_executed", "readiness_or_authorization_created"):
        if boundary.get(key) is not False: errors.append("false_claim:" + key)
    return errors

def main() -> int:
    parser = argparse.ArgumentParser(); parser.add_argument("receipt", nargs="?", default=str(DEFAULT)); parser.add_argument("--no-file-checks", action="store_true"); args = parser.parse_args()
    try: value = json.loads(Path(args.receipt).read_text())
    except Exception as exc: print("INVALID:", exc); return 1
    errors = validate(value, not args.no_file_checks)
    if errors: print("INVALID:", ", ".join(errors)); return 1
    print("PASS_WPA_II_15_1_SIGNED_FROZEN_DECLARATION_PARTIAL_LIFECYCLE_IMPLEMENTATION"); return 0

if __name__ == "__main__": raise SystemExit(main())
