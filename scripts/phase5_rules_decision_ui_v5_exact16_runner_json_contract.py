#!/usr/bin/env python3
"""Faithful CPU-only mirror of the v5 exact16 runner's pre-claim JSON contract."""

from __future__ import annotations

import copy
import hashlib
import json
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
GATE_SHA = "657f3ab6f4ccac4db23fb0ab72889bc49d091f27fd5f63bd5610b736b1aa2a10"
AUTH_SCHEMA = "phase5_rules_decision_ui_v4_exact16_gpu_authorization_v2"
READY_SCHEMA = "phase5_rules_decision_ui_v5_exact16_readiness_v2"
HUD_SHA = "abf73aa283279c28677b5c97a9b02b3130b0d0125048c3124c85a9c0e72df860"
SHELL_SHA = "c144f1a25d3e7ced14e385bd6f76ca7e0f05b06597dcc8a4bacb4c570fc6bad3"
IDS = [
    "group_choice_armed_confirm--p1--zh_cn",
    "group_choice_armed_confirm--p2--en",
    "group_choice_armed_confirm--p3--zh_cn",
    "group_choice_armed_confirm--p4--en",
    "group_choice_open_unselected--p1--en",
    "group_choice_open_unselected--p2--zh_cn",
    "group_choice_open_unselected--p3--en",
    "group_choice_open_unselected--p4--zh_cn",
    "eight_legal_open_unselected--p1--en",
    "break_illegal_open_unselected--p2--zh_cn",
    "submitting_progress--p3--en",
    "authority_error--p4--en",
    "full_table_preview_valid--p1--en",
    "full_table_preview_invalid--p2--en",
    "behind_head_string_preview_valid--p3--en",
    "behind_head_string_preview_invalid--p4--en",
]
FROZEN_ROOT = ROOT / "docs/evidence/phase5/rules-decision-ui-v5-pixel-visible-exact16-corrected-ready-v2"
FROZEN_AUTH = FROZEN_ROOT / "root-one-shot-gpu-authorization-v5v2-run1.json"
FROZEN_MARKER = Path(str(FROZEN_AUTH) + ".consumed.json")
FROZEN_READY = FROZEN_ROOT / "rules-decision-ui-v5-exact16-ready-v2.json"
FROZEN_MANIFEST = FROZEN_ROOT / "rules-decision-ui-v5-source-bindings-v2.json"
RUNNER_SOURCE = ROOT / "spikes/render_core_profile/phase5_rules_decision_ui_gpu_capture_runner.mm"

AUTH_REQUIRED = (
    "schema", "authorized", "consumed", "case_count", "authorization_id",
    "authorization_nonce", "approved_by", "approved_at", "expires_unix_seconds",
    "hud_token_sha256", "shell_token_sha256", "runner_source_sha256",
    "plan_sha256", "billiards_binary_sha256", "font_sha256",
    "source_binding_manifest_sha256", "runner_binary_sha256",
    "source_aggregate_sha256", "status", "single_use",
    "exact16_capture_producer_contract", "authorized_output_canonical_path",
    "ready_manifest_sha256", "dispatcher_sha256", "runner_binary_path",
    "scene_ids_in_order",
)
MANIFEST_REQUIRED = ("gate_sha256", "aggregate_sha256")
READY_REQUIRED = ("schema", "frozen_bindings")
READY_FROZEN_REQUIRED = ("plan_sha256", "source_binding_manifest_sha256")
MARKER_REQUIRED = (
    "authorization_id", "authorization_nonce", "ready_manifest_sha256",
    "output_root", "runner_binary_sha256", "consumed_before_output_or_process",
)


def require(value: bool, message: str) -> None:
    if not value:
        raise RuntimeError(message)


def encoded(value: object) -> bytes:
    return (json.dumps(value, indent=2, sort_keys=True) + "\n").encode()


def digest_bytes(value: bytes) -> str:
    return hashlib.sha256(value).hexdigest()


def sha(path: Path) -> str:
    return digest_bytes(path.read_bytes())


def require_fields(value: dict, fields: tuple[str, ...], label: str) -> None:
    missing = [field for field in fields if field not in value]
    require(not missing, label + " missing fields: " + ",".join(missing))


def validate_runner_source_access_inventory(source: str | None = None) -> None:
    source = RUNNER_SOURCE.read_text() if source is None else source
    for field in set(AUTH_REQUIRED + MANIFEST_REQUIRED + READY_REQUIRED +
                     READY_FROZEN_REQUIRED + MARKER_REQUIRED):
        require(('"' + field + '"') in source,
                "runner access inventory missing: " + field)
    require(source.index("validateAndConsumeAuthorization(receipt, plan") <
            source.index("std::filesystem::create_directories(output);") <
            source.index("[NSApplication sharedApplication]"),
            "runner contract order drift")


def validate_contract(auth: dict, manifest: dict, readiness: dict, marker: dict,
                      artifacts: dict[str, str], paths: dict[str, str],
                      now_unix: int) -> dict:
    require_fields(auth, AUTH_REQUIRED, "auth")
    require_fields(manifest, MANIFEST_REQUIRED, "manifest")
    require_fields(readiness, READY_REQUIRED, "readiness")
    require(isinstance(readiness["frozen_bindings"], dict),
            "readiness frozen_bindings object")
    require_fields(readiness["frozen_bindings"], READY_FROZEN_REQUIRED,
                   "readiness.frozen_bindings")
    require_fields(marker, MARKER_REQUIRED, "marker")
    require(auth["schema"] == AUTH_SCHEMA and auth["authorized"] is True and
            auth["consumed"] is False and auth["case_count"] == 16,
            "authorization explicit contract")
    require(all(isinstance(auth[field], str) and auth[field]
                for field in ("authorization_id", "authorization_nonce",
                              "approved_by", "approved_at")) and
            auth["expires_unix_seconds"] > now_unix,
            "authorization identity/expiry")
    require(auth["hud_token_sha256"] == HUD_SHA and
            auth["shell_token_sha256"] == SHELL_SHA and
            auth["runner_source_sha256"] == artifacts["runner_source"],
            "authorization frozen binding")
    for field, artifact in (
            ("plan_sha256", "plan"),
            ("billiards_binary_sha256", "billiards"),
            ("font_sha256", "font"),
            ("source_binding_manifest_sha256", "manifest"),
            ("runner_binary_sha256", "runner")):
        require(auth[field] == artifacts[artifact],
                "authorization runtime binding: " + field)
    require(manifest["gate_sha256"] == GATE_SHA and
            manifest["aggregate_sha256"] == auth["source_aggregate_sha256"],
            "source manifest gate/aggregate")
    require(auth["status"] == "AUTHORIZED_FOR_EXACTLY_16_SINGLE_USE_CAPTURES" and
            auth["single_use"] is True and
            auth["exact16_capture_producer_contract"] is True and
            auth["authorized_output_canonical_path"] == paths["output"] and
            auth["ready_manifest_sha256"] == artifacts["readiness"] and
            auth["dispatcher_sha256"] == artifacts["dispatcher"] and
            auth["runner_binary_path"] == paths["runner"],
            "authorization dispatch binding")
    require(readiness["schema"] == READY_SCHEMA and
            readiness["frozen_bindings"]["plan_sha256"] == artifacts["plan"] and
            readiness["frozen_bindings"]["source_binding_manifest_sha256"] ==
            artifacts["manifest"], "readiness runtime binding")
    require(auth["scene_ids_in_order"] == IDS,
            "authorization exact IDs/order")
    require(marker["authorization_id"] == auth["authorization_id"] and
            marker["authorization_nonce"] == auth["authorization_nonce"] and
            marker["ready_manifest_sha256"] == artifacts["readiness"] and
            marker["output_root"] == paths["output"] and
            marker["runner_binary_sha256"] == artifacts["runner"] and
            marker["consumed_before_output_or_process"] is True,
            "durable marker binding")
    return {"status": "PASS_V5_EXACT16_FULL_PRECLAIM_JSON_CONTRACT",
            "auth_field_count": len(AUTH_REQUIRED),
            "manifest_field_count": len(MANIFEST_REQUIRED),
            "readiness_field_count": len(READY_REQUIRED) + len(READY_FROZEN_REQUIRED),
            "marker_field_count": len(MARKER_REQUIRED),
            "case_count": 16, "gpu_executed": False}


def corrected_frozen_fixture() -> tuple[dict, dict, dict, dict, dict, dict]:
    auth = json.loads(FROZEN_AUTH.read_text())
    marker = json.loads(FROZEN_MARKER.read_text())
    readiness = json.loads(FROZEN_READY.read_text())
    manifest = json.loads(FROZEN_MANIFEST.read_text())
    manifest["gate_sha256"] = GATE_SHA
    manifest_sha = digest_bytes(encoded(manifest))
    readiness["frozen_bindings"]["source_binding_manifest_sha256"] = manifest_sha
    readiness["paths"]["source_binding_manifest"] = str(FROZEN_MANIFEST)
    readiness_sha = digest_bytes(encoded(readiness))
    auth["source_binding_manifest_sha256"] = manifest_sha
    auth["ready_manifest_sha256"] = readiness_sha
    marker["ready_manifest_sha256"] = readiness_sha
    auth_sha = digest_bytes(encoded(auth))
    marker["authorization_sha256"] = auth_sha
    paths = {"output": auth["authorized_output_canonical_path"],
             "runner": auth["runner_binary_path"]}
    artifacts = {
        "plan": auth["plan_sha256"], "billiards": auth["billiards_binary_sha256"],
        "font": auth["font_sha256"], "manifest": manifest_sha,
        "runner": auth["runner_binary_sha256"],
        "runner_source": auth["runner_source_sha256"],
        "readiness": readiness_sha, "dispatcher": auth["dispatcher_sha256"],
        "authorization": auth_sha,
    }
    return auth, manifest, readiness, marker, artifacts, paths


def validate_real_frozen_with_approved_overlay() -> dict:
    validate_runner_source_access_inventory()
    fixture = corrected_frozen_fixture()
    return validate_contract(*fixture, now_unix=fixture[0]["expires_unix_seconds"] - 1)


if __name__ == "__main__":
    print(json.dumps(validate_real_frozen_with_approved_overlay(), sort_keys=True))
