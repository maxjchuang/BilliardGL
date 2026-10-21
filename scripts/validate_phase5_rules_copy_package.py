#!/usr/bin/env python3
"""Validate Phase-5 bilingual rules copy, approved tokens, and pending UI gates."""

from __future__ import annotations

import argparse
import json
from pathlib import Path
import sys


sys.path.insert(0, str(Path(__file__).resolve().parent))
from generate_phase5_rules_copy_package import (  # noqa: E402
    HUD_TOKENS, NOTICE_DATA, PENDING_UI, PROOF_NAME, ROOT, SHELL_TOKENS,
    build_flat_proof, build_manifest,
)


def _tokens():
    hud = json.loads(HUD_TOKENS.read_text(encoding="utf-8"))
    shell = json.loads(SHELL_TOKENS.read_text(encoding="utf-8"))
    return set(hud["icon_tokens"]) | set(shell["icon_tokens"]), hud["aliases"]


def _localized(value):
    return isinstance(value, dict) and all(
        isinstance(value.get(locale), str) and value[locale].strip()
        for locale in ("en", "zh_cn"))


def _copy_localized(value):
    return isinstance(value, dict) and all(
        isinstance(value.get(locale), dict) and all(
            isinstance(value[locale].get(field), str) and value[locale][field].strip()
            for field in ("title", "explanation", "next_action"))
        for locale in ("en", "zh_cn"))


def validate_manifest(value, manifest_path=None):
    errors = []
    expected = build_manifest()
    if value.get("schema") != "phase5_rules_shipping_copy_v1" or value.get("schema_version") != 1 or \
            value.get("package_id") != expected["package_id"] or \
            value.get("ruleset_id") != "wpa_heyball_2025_08_16_v1":
        errors.append("rules-copy package identity differs")
    if value.get("profile_complete") is not False:
        errors.append("rules profile must remain explicitly incomplete")
    if value.get("source_audit") != expected["source_audit"]:
        errors.append("rules/HUD/profile/token source SHA audit differs")
    boundary = value.get("evidence_boundary", {})
    if boundary != expected["evidence_boundary"]:
        errors.append("evidence boundary differs or claims runtime/GPU/profile completion")
    policy = value.get("presentation_policy", {})
    if policy != expected["presentation_policy"]:
        errors.append("HUD/banner/modal duration, focus, screen-reader, CVD, or subtitle policy differs")
    accessibility = policy.get("accessibility", {}) if isinstance(policy, dict) else {}
    if accessibility.get("never_color_only") is not True or \
            accessibility.get("shape_plus_text_required") is not True:
        errors.append("rules meaning cannot rely on color only")
    approved_tokens, approved_aliases = _tokens()
    outcome = value.get("outcome_copy")
    if not isinstance(outcome, dict) or set(outcome) != set(expected["outcome_copy"]):
        errors.append("ball-in-hand/turn/handoff/win/loss outcome copy coverage differs")
    else:
        for outcome_id, item in outcome.items():
            if item.get("shape_token") not in approved_tokens:
                errors.append("outcome uses unapproved shape token: " + outcome_id)
            if not all(isinstance(item.get(locale), str) and item[locale].strip()
                       for locale in ("en", "zh_cn")):
                errors.append("outcome localization missing: " + outcome_id)
    notices = value.get("reason_notices")
    expected_by_code = {item["reason_code"]: item for item in expected["reason_notices"]}
    if not isinstance(notices, list):
        errors.append("reason_notices must be an array")
        notices = []
    codes = [item.get("reason_code") for item in notices if isinstance(item, dict)]
    if len(codes) != 8 or len(set(codes)) != 8 or set(codes) != set(expected_by_code):
        errors.append("reason notices must exactly cover all eight unique machine codes")
    keys = [item.get("text_key") for item in notices if isinstance(item, dict)]
    if len(keys) != len(set(keys)):
        errors.append("reason text keys must be unique")
    for item in notices:
        if not isinstance(item, dict) or item.get("reason_code") not in expected_by_code:
            continue
        code = item["reason_code"]
        contract = expected_by_code[code]
        if item.get("text_key") != contract["text_key"]:
            errors.append("machine code to text-key mapping differs: " + code)
        if item.get("capability") != "implemented" or item.get("product_exposed") is not True:
            errors.append("existing reason-code capability/exposure differs: " + code)
        token, alias = item.get("shape_token"), item.get("runtime_shape_alias")
        if token not in approved_tokens:
            errors.append("reason uses unapproved canonical shape token: " + code)
        if alias != token and approved_aliases.get(alias) != token:
            errors.append("reason introduces unreviewed or wrong shape alias: " + code)
        redundant = item.get("redundant_channels")
        if not isinstance(redundant, list) or "shape" not in redundant or \
                not {"title_text", "explanation_text", "next_action_text"}.issubset(set(redundant)):
            errors.append("reason lacks shape plus text redundancy: " + code)
        if not _copy_localized(item.get("copy")):
            errors.append("reason requires complete English/Chinese title, explanation, and action: " + code)
        elif any(not item["copy"][locale]["next_action"].strip() for locale in ("en", "zh_cn")):
            errors.append("reason next action missing: " + code)
        if item.get("semantic_effect") != contract["semantic_effect"]:
            errors.append("reason turn/ball-in-hand/handoff/win/loss semantics differ: " + code)
    pending = value.get("pending_ui_capabilities")
    expected_pending = {item["capability_id"]: item for item in expected["pending_ui_capabilities"]}
    if not isinstance(pending, list):
        errors.append("pending_ui_capabilities must be an array")
        pending = []
    ids = [item.get("capability_id") for item in pending if isinstance(item, dict)]
    if len(ids) != len(expected_pending) or len(set(ids)) != len(ids) or set(ids) != set(expected_pending):
        errors.append("pending UI capability coverage differs")
    for item in pending:
        if not isinstance(item, dict) or item.get("capability_id") not in expected_pending:
            continue
        capability_id = item["capability_id"]
        if item.get("capability") != "pending" or item.get("product_exposed") is not False:
            errors.append("pending UI must remain pending and not product-exposed: " + capability_id)
        if item.get("shape_token") not in approved_tokens:
            errors.append("pending UI uses unapproved shape token: " + capability_id)
        if not _localized(item.get("title")) or not _localized(item.get("explanation")):
            errors.append("pending UI localization missing: " + capability_id)
        actions = item.get("actions")
        if not isinstance(actions, list) or not actions:
            errors.append("pending decision UI requires at least one action: " + capability_id)
        elif any(not isinstance(action.get("id"), str) or not action["id"].strip() or
                 not all(isinstance(action.get(locale), str) and action[locale].strip()
                         for locale in ("en", "zh_cn")) for action in actions):
            errors.append("pending UI action/localization invalid: " + capability_id)
        if item.get("machine_code") != expected_pending[capability_id]["machine_code"] or \
                item.get("text_key") != expected_pending[capability_id]["text_key"]:
            errors.append("pending UI reserved machine/text key differs: " + capability_id)
    proof_contract = value.get("flat_proof")
    if proof_contract != expected["flat_proof"]:
        errors.append("flat proof contract differs or claims GPU evidence")
    else:
        path = Path(manifest_path).parent / PROOF_NAME if manifest_path else ROOT / proof_contract["path"]
        if not path.is_file():
            errors.append("flat text/layout proof file missing")
        else:
            expected_bytes = build_flat_proof(value).encode("utf-8")
            actual_bytes = path.read_bytes()
            if actual_bytes != expected_bytes:
                errors.append("flat proof differs from exact manifest copy/layout")
            if b"FLAT TEXT/LAYOUT PROOF" not in actual_bytes or b"NOT GPU" not in actual_bytes:
                errors.append("flat proof does not disclose its non-GPU evidence boundary")
    return errors


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("manifest", type=Path)
    args = parser.parse_args()
    try:
        value = json.loads(args.manifest.read_text(encoding="utf-8"))
        errors = validate_manifest(value, args.manifest)
    except (OSError, json.JSONDecodeError, ValueError) as error:
        print("Phase5 rules-copy validation failed: " + str(error), file=sys.stderr)
        return 1
    if errors:
        for error in errors:
            print("ERROR: " + error, file=sys.stderr)
        return 1
    print("Phase5 rules-copy PASS: reason_codes=8 pending_ui=4 profile_complete=false proof=FLAT_NOT_GPU")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
