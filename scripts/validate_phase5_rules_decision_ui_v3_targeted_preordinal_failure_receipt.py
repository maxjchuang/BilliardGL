#!/usr/bin/env python3
"""Read-only validator for the frozen Phase5 v3 pre-ordinal failure."""

from __future__ import annotations

import argparse
import hashlib
import json
from pathlib import Path
import stat
from typing import Any


ROOT = Path(__file__).resolve().parents[1]
DEFAULT_RECEIPT = (
    ROOT / "docs/evidence/phase5-rules-decision-ui-v3-targeted-"
    "preordinal-failure-receipt-v1.json")
EXPECTED_RECEIPT_SHA256 = "f29fe3ee3ca206eed63a8a9f7924f499f99e531b874c04b8dcdcdf61f9e93a8a"
EXPECTED_AUTH_SHA256 = (
    "f052852c6a77817c808a347760f910ef1a97d7581f4d54420e5605d6a70b30bc")
EXPECTED_READY_SHA256 = (
    "f13625b6d0da47583c0ec78f723e530b62d2b6ea07b9c7278f3f860b16817e5f")
EXPECTED_BINDINGS_SHA256 = (
    "139db69951c9b4fe9eb9b9502df86ccb66302427b1371174563c4ae0b9dbcdfa")
EXPECTED_HUD_SHA256 = (
    "abf73aa283279c28677b5c97a9b02b3130b0d0125048c3124c85a9c0e72df860")
EXPECTED_SHELL_SHA256 = (
    "c144f1a25d3e7ced14e385bd6f76ca7e0f05b06597dcc8a4bacb4c570fc6bad3")
EXPECTED_CORRECTION_GATE_SHA256 = (
    "2155c60375062c7992d1280a3dfc96929840627d3194a4401a0501eabfad77a3")
MISSING_RUNNER_FIELDS = (
    "hud_token_sha256", "shell_token_sha256",
    "source_binding_manifest_sha256")


class FailureReceiptError(RuntimeError):
    pass


def require(condition: bool, message: str) -> None:
    if not condition:
        raise FailureReceiptError(message)


def sha256(path: Path) -> str:
    digest = hashlib.sha256()
    with path.open("rb") as source:
        for chunk in iter(lambda: source.read(1024 * 1024), b""):
            digest.update(chunk)
    return digest.hexdigest()


def load(path: Path) -> dict[str, Any]:
    value = json.loads(path.read_text(encoding="utf-8"))
    require(isinstance(value, dict), f"{path}: JSON object required")
    return value


def bound_file(relative: Any, digest: Any, label: str) -> Path:
    require(isinstance(relative, str), label + ": path missing")
    require(isinstance(digest, str) and len(digest) == 64,
            label + ": SHA missing")
    path = (ROOT / relative).resolve(strict=True)
    require(path.is_file(), label + ": file missing")
    require(sha256(path) == digest, label + ": SHA mismatch")
    return path


def validate_receipt(path: Path = DEFAULT_RECEIPT,
                     enforce_frozen_sha: bool = True) -> dict[str, Any]:
    path = path.resolve(strict=True)
    if enforce_frozen_sha:
        require(sha256(path) == EXPECTED_RECEIPT_SHA256,
                "frozen failure receipt SHA mismatch")
    value = load(path)
    require(value.get("schema") ==
            "phase5_rules_decision_ui_v3_targeted_preordinal_failure_receipt_v1",
            "failure receipt schema")
    require(value.get("status") ==
            "TARGETED28_PREORDINAL_BINDING_FAILURE_FROZEN_NO_RETRY",
            "failure receipt status")

    authorization = value.get("authorization", {})
    auth_path = bound_file(authorization.get("path"),
                           authorization.get("sha256"), "authorization")
    require(authorization.get("sha256") == EXPECTED_AUTH_SHA256,
            "authorization frozen SHA")
    mode = f"{stat.S_IMODE(auth_path.stat().st_mode):04o}"
    require(authorization.get("mode_octal") == mode == "0444" and
            authorization.get("uid") == auth_path.stat().st_uid and
            authorization.get("gid") == auth_path.stat().st_gid and
            authorization.get("size_bytes") == auth_path.stat().st_size,
            "authorization permissions/metadata")
    require(authorization.get("runner_physical_consumption") is False and
            authorization.get("root_policy_revoked_after_single_attempt") is True and
            authorization.get("authorization_reuse_forbidden") is True and
            authorization.get("json_consumed_field_at_issue") is False,
            "authorization consumption/revocation semantics")
    auth = load(auth_path)
    require(auth.get("authorization_id") ==
            authorization.get("authorization_id") and
            auth.get("authorized") is True and
            auth.get("consumed") is False and
            auth.get("case_count") == 28,
            "signed authorization identity")
    require(tuple(field for field in MISSING_RUNNER_FIELDS if field not in auth) ==
            MISSING_RUNNER_FIELDS,
            "signed authorization missing-field identity")
    require(auth.get("source_bindings_sha256") == EXPECTED_BINDINGS_SHA256,
            "signed authorization misaligned binding field")

    marker = value.get("consumed_marker_audit", {})
    marker_path = ROOT / marker.get("expected_path", "")
    require(marker.get("exists") is False and not marker_path.exists() and
            marker.get("sha256") is None and
            marker.get("mode_octal") is None,
            "consumed marker must remain physically absent")
    output = value.get("output_audit", {})
    output_path = ROOT / output.get("authorized_path", "")
    require(output.get("exists") is False and not output_path.exists() and
            output.get("sha256") is None and
            output.get("mode_octal") is None and
            output.get("ppm_files") == 0 and output.get("png_files") == 0 and
            output.get("result_rows") == 0 and
            output.get("evidence_manifest_created") is False,
            "output absence/zero evidence")

    failure = value.get("failure", {})
    require(failure == {
        "wrapper_exit_code": 1,
        "runner_error": "phase5-rules-ui-gpu-capture: hud_token_sha256",
        "failed_before_capture_ordinal": 0,
        "runner_process_started": True,
        "output_created": False,
        "cocoa_application_created": False,
        "opengl_context_created": False,
    }, "pre-ordinal failure identity")
    require(value.get("counts") == {
        "planned_cases": 28, "completed_cases": 0,
        "failed_capture_cases": 0, "unrun_cases": 28,
    }, "failure counts")

    frozen = value.get("frozen_bindings", {})
    ready_path = bound_file(frozen.get("readiness_manifest_path"),
                            frozen.get("readiness_manifest_sha256"), "readiness")
    require(frozen.get("readiness_manifest_sha256") == EXPECTED_READY_SHA256,
            "readiness frozen SHA")
    ready = load(ready_path)
    expected_ids = ready.get("case_ids_in_order")
    require(isinstance(expected_ids, list) and len(expected_ids) == 28 and
            ready.get("case_results") ==
                [{"capture_id": case_id, "status": "NOT_RUN"}
                 for case_id in expected_ids],
            "readiness exact 28 NOT_RUN")
    case_contract = value.get("case_result_contract", {})
    require(case_contract.get("case_ids_in_order_count") == 28 and
            case_contract.get("all_case_status") == "NOT_RUN",
            "receipt exact 28 NOT_RUN")

    root_cause = value.get("root_cause", {})
    bindings_path = bound_file(root_cause.get("source_binding_manifest_path"),
                               root_cause.get("source_binding_manifest_sha256"),
                               "source bindings")
    require(root_cause.get("source_binding_manifest_sha256") ==
            EXPECTED_BINDINGS_SHA256,
            "source binding frozen SHA")
    bindings = load(bindings_path)
    paths = {row.get("path") for row in bindings.get("files", [])}
    hud_relative = root_cause.get("hud_token", {}).get("path")
    shell_relative = root_cause.get("shell_token", {}).get("path")
    require(hud_relative not in paths and shell_relative not in paths and
            root_cause.get("source_binding_manifest_contains_hud_token_asset")
                is False and
            root_cause.get("source_binding_manifest_contains_shell_token_asset")
                is False,
            "source binding token asset omission")
    require(root_cause.get("signed_authorization_missing_fields_in_runner_check_order")
            == list(MISSING_RUNNER_FIELDS),
            "runner-required missing fields")
    require(root_cause.get("signed_authorization_present_misaligned_field") == {
        "field": "source_bindings_sha256",
        "sha256": EXPECTED_BINDINGS_SHA256,
    }, "misaligned source-binding field")

    for key, expected in (("hud_token", EXPECTED_HUD_SHA256),
                          ("shell_token", EXPECTED_SHELL_SHA256)):
        token = root_cause.get(key, {})
        token_path = bound_file(token.get("path"), token.get("current_sha256"), key)
        require(sha256(token_path) == expected and
                token.get("current_sha256") == expected and
                token.get("runner_expected_sha256") == expected and
                token.get("content_drift") is False,
                key + " live/runner SHA identity")

    runner_source = bound_file(frozen.get("runner_source_path"),
                               frozen.get("runner_source_sha256"),
                               "runner source")
    bound_file(frozen.get("runner_binary_path"),
               frozen.get("runner_binary_sha256"), "runner binary")
    runner_text = runner_source.read_text(encoding="utf-8")
    hud_check = runner_text.index('receipt.at("hud_token_sha256")')
    marker_open = runner_text.index("O_WRONLY | O_CREAT | O_EXCL")
    output_create = runner_text.index("std::filesystem::create_directories(output)")
    cocoa_create = runner_text.index("[NSApplication sharedApplication]")
    require(hud_check < marker_open < output_create < cocoa_create,
            "runner pre-ordinal ordering evidence")

    gate_path = bound_file(frozen.get("correction_gate_path"),
                           frozen.get("correction_gate_sha256"),
                           "correction gate")
    require(frozen.get("correction_gate_sha256") ==
            EXPECTED_CORRECTION_GATE_SHA256,
            "correction gate frozen SHA")
    gate = load(gate_path)
    require(gate.get("status") ==
            "OFFLINE_ROOT_CAUSE_CONFIRMED_CORRECTION_NOT_IMPLEMENTED_GPU_FORBIDDEN" and
            gate.get("root_cause", {}).get("category") ==
            root_cause.get("category") and
            gate.get("minimum_correction_contract", {}).get(
                "gpu_execution_allowed_by_this_gate") is False,
            "correction gate boundary")
    require(value.get("evidence_boundary") == {
        "targeted_machine_pass": False,
        "technical_art_review_completed": 0,
        "golden_promotion": False,
        "m2_promotion": False,
        "product_exposed": False,
        "rerun_authorized": False,
    }, "evidence boundary")
    return {
        "status": "PASS_FROZEN_PHASE5_V3_TARGETED_PREORDINAL_FAILURE",
        "failure_receipt_sha256": sha256(path),
        "authorization_sha256": sha256(auth_path),
        "runner_physical_consumption": False,
        "authorization_reuse_forbidden": True,
        "completed_cases": 0,
        "failed_capture_cases": 0,
        "unrun_cases": 28,
        "hud_token_content_drift": False,
        "shell_token_content_drift": False,
        "gpu_rerun_authorized": False,
    }


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--receipt", type=Path, default=DEFAULT_RECEIPT)
    args = parser.parse_args()
    try:
        print(json.dumps(validate_receipt(args.receipt), indent=2,
                         sort_keys=True))
    except (OSError, ValueError, json.JSONDecodeError,
            FailureReceiptError) as error:
        print("FAIL: " + str(error))
        return 2
    return 0


if __name__ == "__main__":
    raise SystemExit(main())

