#!/usr/bin/env python3
"""Validate pending Phase-5 rules decision UI/actions and fail-closed authority rules."""

from __future__ import annotations

import argparse
import hashlib
import json
from pathlib import Path
import sys


sys.path.insert(0, str(Path(__file__).resolve().parent))
from generate_phase5_rules_decision_ui_actions import (  # noqa: E402
    CAPABILITY_IDS, FLOW_IDS, FROZEN_COPY, HUD_TOKENS, PROOF_NAME, ROOT,
    SHELL_TOKENS, build_flat_proof, build_manifest,
)


FROZEN_EXPECTED = {
    "docs/art/phase5-rules-shipping-copy-v1.json": "bbc5bd4199d1c876948aa3641957c0b9bbe1748b24bf517670d36d24b49781aa",
    "docs/art/phase5-rules-shipping-copy-v1.schema.json": "b4c056db14ae00e2fdab6ec5374b2bce0da683a8306988482f4adfe836219d68",
    "docs/art/phase5-rules-shipping-copy-flat-proof-v1.svg": "5d2c214c0885a42b1b4c4542c4eaf3a23bb8ce6329784b0f85e0494a6b18bb38",
}


def _localized(value: object) -> bool:
    return isinstance(value, dict) and set(value) == {"en", "zh_cn"} and all(
        isinstance(value[locale], str) and value[locale].strip()
        for locale in ("en", "zh_cn")
    )


def _sha(path: Path) -> str:
    return hashlib.sha256(path.read_bytes()).hexdigest()


def validate_manifest(value: object, manifest_path: Path | None = None) -> list[str]:
    errors: list[str] = []
    if not isinstance(value, dict):
        return ["manifest root must be an object"]
    expected = build_manifest()
    if value.get("schema") != "phase5_rules_decision_ui_actions_v1" or \
            value.get("schema_version") != 1 or \
            value.get("package_id") != "phase5_wpa_heyball_rules_decision_ui_actions_v1" or \
            value.get("ruleset_id") != "wpa_heyball_2025_08_16_v1":
        errors.append("decision UI/action package identity differs")
    if value.get("status") != "PENDING" or value.get("profile_complete") is not False or \
            value.get("product_exposed") is not False:
        errors.append("package must remain PENDING, profile-incomplete, and not product-exposed")

    source_bindings = value.get("source_bindings")
    if source_bindings != expected["source_bindings"]:
        errors.append("frozen copy/audit/token source bindings differ")
    for relative, digest in FROZEN_EXPECTED.items():
        path = ROOT / relative
        if not path.is_file() or _sha(path) != digest:
            errors.append("frozen rules-copy artifact changed: " + relative)

    boundary = value.get("evidence_boundary")
    if boundary != expected["evidence_boundary"]:
        errors.append("evidence boundary differs or claims runtime/profile/policy completion")
    else:
        if boundary.get("changes_cpp") or boundary.get("changes_frozen_rules_copy") or \
                boundary.get("capability_promotion_allowed") or \
                boundary.get("official_off_table_conflict_resolved"):
            errors.append("implementation or official-conflict completion must not be claimed")

    capabilities = value.get("capabilities")
    if not isinstance(capabilities, list):
        errors.append("capabilities must be an array")
        capabilities = []
    capability_by_id = {item.get("capability_id"): item for item in capabilities
                        if isinstance(item, dict)}
    if len(capabilities) != len(capability_by_id) or tuple(item.get("capability_id") for item in capabilities) != CAPABILITY_IDS:
        errors.append("capabilities must exactly cover six ordered M4 interaction capabilities")
    audit = json.loads((ROOT / "docs/rules/phase5-wpa-heyball-completeness-audit.json").read_text(encoding="utf-8"))
    audit_pending = {item["id"]: item for item in audit["pending_capabilities"]}
    audit_rows = {item["id"] for item in audit["matrix"]}
    for capability_id, item in capability_by_id.items():
        if item.get("state") != "pending" or item.get("product_exposed") is not False:
            errors.append("capability must remain pending and not exposed: " + str(capability_id))
        if capability_id not in audit_pending:
            errors.append("capability is not present in official completeness audit: " + str(capability_id))
        if not isinstance(item.get("source_rows"), list) or not item["source_rows"] or \
                not set(item["source_rows"]).issubset(audit_rows):
            errors.append("capability source rows invalid: " + str(capability_id))

    flows = value.get("flows")
    if not isinstance(flows, list):
        errors.append("flows must be an array")
        flows = []
    flow_by_id = {item.get("flow_id"): item for item in flows if isinstance(item, dict)}
    if len(flows) != len(flow_by_id) or tuple(item.get("flow_id") for item in flows) != FLOW_IDS:
        errors.append("flows must exactly cover seven ordered decision/placement/progress flows")
    expected_flows = {item["flow_id"]: item for item in expected["flows"]}
    hud = json.loads(HUD_TOKENS.read_text(encoding="utf-8"))
    shell = json.loads(SHELL_TOKENS.read_text(encoding="utf-8"))
    approved_tokens = set(hud["icon_tokens"]) | set(shell["icon_tokens"])
    frozen = json.loads(FROZEN_COPY.read_text(encoding="utf-8"))
    frozen_pending = {item["capability_id"]: item
                      for item in frozen["pending_ui_capabilities"]}
    all_states: set[str] = set()
    capability_flow_refs: dict[str, list[str]] = {capability_id: [] for capability_id in capability_by_id}
    for flow_id, item in flow_by_id.items():
        capability_id = item.get("capability_id")
        if capability_id in capability_flow_refs:
            capability_flow_refs[capability_id].append(flow_id)
        if item.get("capability_state") != "pending" or item.get("product_exposed") is not False:
            errors.append("flow must remain pending and not exposed: " + str(flow_id))
        if not _localized(item.get("copy", {}).get("title")) or \
                not _localized(item.get("copy", {}).get("body")):
            errors.append("flow bilingual title/body missing: " + str(flow_id))
        if not isinstance(item.get("preconditions"), list) or not item["preconditions"] or \
                not isinstance(item.get("postconditions"), list) or not item["postconditions"]:
            errors.append("flow preconditions/postconditions missing: " + str(flow_id))
        tokens = item.get("shape_tokens")
        if not isinstance(tokens, list) or not tokens or not set(tokens).issubset(approved_tokens):
            errors.append("flow uses unknown or missing existing shape token: " + str(flow_id))
        states = item.get("states")
        if not isinstance(states, list):
            errors.append("flow states must be an array: " + str(flow_id))
            states = []
        state_ids = [state.get("state_id") for state in states if isinstance(state, dict)]
        if len(state_ids) != len(states) or len(state_ids) != len(set(state_ids)) or \
                any(not isinstance(state_id, str) or not state_id.startswith(flow_id + ".")
                    for state_id in state_ids):
            errors.append("flow state IDs missing, duplicate, or outside namespace: " + str(flow_id))
        if all_states.intersection(state_ids):
            errors.append("state ID reused across flows: " + str(flow_id))
        all_states.update(state_ids)
        if item.get("initial_state") not in state_ids or item.get("completed_state") not in state_ids:
            errors.append("flow initial/completed state is not declared: " + str(flow_id))
        actions = item.get("actions")
        if not isinstance(actions, list) or not actions:
            errors.append("flow actions missing: " + str(flow_id))
            actions = []
        action_ids = [entry.get("action_id") for entry in actions if isinstance(entry, dict)]
        if len(action_ids) != len(actions) or len(action_ids) != len(set(action_ids)):
            errors.append("action IDs missing or duplicate within flow: " + str(flow_id))
        for entry in actions:
            if not isinstance(entry, dict):
                continue
            action_id = str(entry.get("action_id"))
            if not _localized(entry.get("label")):
                errors.append("action bilingual label missing: " + action_id)
            if entry.get("allowed_from_state") not in state_ids or \
                    entry.get("target_state_before_ack") not in state_ids:
                errors.append("action state transition references undeclared state: " + action_id)
            if entry.get("optimistic_authoritative_mutation") is not False:
                errors.append("optimistic authoritative mutation forbidden: " + action_id)
            if entry.get("mutates_authoritative_state") is True and \
                    entry.get("requires_authority_ack") is not True:
                errors.append("authoritative mutation requires ACK: " + action_id)
            if not isinstance(entry.get("authority_command"), str) or not entry["authority_command"]:
                errors.append("action authority command missing: " + action_id)
            legacy_id = entry.get("frozen_copy_action_id")
            if legacy_id is not None:
                frozen_item = frozen_pending.get(capability_id)
                legacy_ids = {button["id"] for button in frozen_item["actions"]} if frozen_item else set()
                if legacy_id not in legacy_ids:
                    errors.append("action references unknown frozen-copy action ID: " + action_id)
        failure = item.get("failure_policy", {})
        if failure.get("default_action_on_timeout") is not None or \
                failure.get("dismiss_on_escape") is not False or \
                failure.get("advance_turn_without_ack") is not False or \
                failure.get("clear_entitlement_without_ack") is not False or \
                failure.get("retry_reuses_idempotency_key") is not True:
            errors.append("flow fail-closed timeout/cancel/ACK policy differs: " + str(flow_id))
        frozen_code = item.get("frozen_copy_machine_code")
        frozen_key = item.get("frozen_copy_text_key")
        if frozen_code is not None or frozen_key is not None:
            frozen_item = frozen_pending.get(capability_id)
            if not frozen_item or frozen_item["machine_code"] != frozen_code or \
                    frozen_item["text_key"] != frozen_key:
                errors.append("flow frozen-copy machine/text binding differs: " + str(flow_id))
        if flow_id in expected_flows:
            expected_action_ids = [entry["action_id"] for entry in expected_flows[flow_id]["actions"]]
            if action_ids != expected_action_ids:
                errors.append("exact action ID/order differs: " + str(flow_id))

    for capability_id, item in capability_by_id.items():
        if item.get("flow_ids") != capability_flow_refs.get(capability_id, []):
            errors.append("capability-to-flow mapping differs: " + capability_id)

    interaction = value.get("interaction_policy")
    if interaction != expected["interaction_policy"]:
        errors.append("HUD/modal hierarchy, focus, input, cancel, timeout, or resume policy differs")
    authority = value.get("authority_contract")
    if authority != expected["authority_contract"]:
        errors.append("authority request/ACK/error contract differs")
    accessibility = value.get("accessibility")
    if accessibility != expected["accessibility"]:
        errors.append("screen-reader/CVD/subtitle/reduced-motion policy differs")

    policies = value.get("policy_decision_placeholders")
    if not isinstance(policies, list) or len(policies) != 3:
        errors.append("three unresolved policy placeholders are required")
        policies = []
    policy_by_id = {item.get("policy_id"): item for item in policies if isinstance(item, dict)}
    for policy_id, item in policy_by_id.items():
        if item.get("status") != "UNRESOLVED" or item.get("product_exposed") is not False or \
                item.get("assumed_answer") is not None:
            errors.append("policy placeholder must remain unresolved and answer-free: " + str(policy_id))
    if set(policy_by_id) != {"stalemate_declaration_authority_policy",
                            "stalemate_continuation_window_policy",
                            "off_table_12_3_vs_17_e_interpretation"}:
        errors.append("policy placeholder inventory differs")
    for item in flows:
        if not isinstance(item, dict):
            continue
        for policy_id in item.get("policy_dependencies", []):
            policy = policy_by_id.get(policy_id)
            if not policy or item.get("capability_id") not in policy.get("blocks_capabilities", []):
                errors.append("flow policy dependency does not block capability: " + str(policy_id))

    proof_contract = value.get("flat_proof")
    if proof_contract != expected["flat_proof"]:
        errors.append("flat proof contract differs or claims GPU/runtime evidence")
    else:
        proof_path = (Path(manifest_path).parent / PROOF_NAME) if manifest_path else ROOT / proof_contract["path"]
        if not proof_path.is_file():
            errors.append("flat proof file missing")
        else:
            actual = proof_path.read_bytes()
            generated = build_flat_proof(value).encode("utf-8")
            if actual != generated:
                errors.append("flat proof differs from exact manifest copy/layout")
            for marker in (b"NOT GPU", b"NOT RUNTIME", b"PENDING", b"PRODUCT_EXPOSED=FALSE"):
                if marker not in actual:
                    errors.append("flat proof evidence boundary marker missing: " + marker.decode())

    return errors


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("manifest", type=Path)
    args = parser.parse_args()
    try:
        value = json.loads(args.manifest.read_text(encoding="utf-8"))
        errors = validate_manifest(value, args.manifest)
    except (OSError, json.JSONDecodeError, ValueError, KeyError) as error:
        print("Phase5 rules decision UI/action validation failed: " + str(error), file=sys.stderr)
        return 1
    if errors:
        for error in errors:
            print("ERROR: " + error, file=sys.stderr)
        return 1
    print("Phase5 rules decision UI/actions PASS: capabilities=6 flows=7 status=PENDING product_exposed=false profile_complete=false proof=FLAT_NOT_GPU_NOT_RUNTIME")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
