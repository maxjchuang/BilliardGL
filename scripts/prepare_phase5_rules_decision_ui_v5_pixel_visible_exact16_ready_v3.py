#!/usr/bin/env python3
"""Prepare manifest-field-corrected Phase5 v5 exact16 successor package."""

from __future__ import annotations

import argparse
import json
from pathlib import Path
import sys

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT))
from scripts import prepare_phase5_rules_decision_ui_v4_exact16_ready as base
from scripts import prepare_phase5_rules_decision_ui_v5_pixel_visible_exact16_ready_v2 as v2

DEFAULT_OUTPUT = ROOT / "docs/evidence/phase5/rules-decision-ui-v5-pixel-visible-exact16-manifest-corrected-ready-v3"
GATE = ROOT / "docs/art/phase5-rules-decision-ui-v5v2-exact16-manifest-runner-contract-correction-gate-v1.json"
GATE_SHA = "6b003450d49db533713c95833e6f4d9348225439de1c27582be4ba8b6ac56ea5"
FAILURE = ROOT / "docs/evidence/phase5/rules-decision-ui-v5v2-exact16-preordinal-missing-gate-field-failure-receipt-v1.json"
FAILURE_SHA = "fd721e3c8dff6f1b526abb2c51ce66a4f81e749108b301737451ad83efc759c5"
EXTRA = (
    "docs/art/phase5-rules-decision-ui-v5v2-exact16-manifest-runner-contract-correction-gate-v1.json",
    "docs/evidence/phase5/rules-decision-ui-v5v2-exact16-preordinal-missing-gate-field-failure-receipt-v1.json",
    "scripts/validate_phase5_rules_decision_ui_v5v2_exact16_preordinal_missing_gate_failure.py",
    "tests/visual/test_phase5_rules_decision_ui_v5v2_exact16_preordinal_missing_gate_failure.py",
    "scripts/validate_phase5_rules_decision_ui_v5v2_exact16_manifest_runner_contract_correction_gate.py",
    "tests/visual/test_phase5_rules_decision_ui_v5v2_exact16_manifest_runner_contract_correction_gate.py",
    "scripts/phase5_rules_decision_ui_v5_exact16_runner_json_contract.py",
    "tests/visual/test_phase5_rules_decision_ui_v5_exact16_runner_json_contract.py",
    "scripts/prepare_phase5_rules_decision_ui_v5_pixel_visible_exact16_ready_v3.py",
    "scripts/validate_phase5_rules_decision_ui_v5_pixel_visible_exact16_ready_v3.py",
    "tests/visual/test_phase5_rules_decision_ui_v5_pixel_visible_exact16_ready_v3.py",
)


def require(value: bool, message: str) -> None:
    if not value:
        raise RuntimeError(message)


def write(path: Path, value: object) -> None:
    path.write_text(json.dumps(value, indent=2, sort_keys=True) + "\n",
                    encoding="utf-8")


def prepare(build: Path, output: Path = DEFAULT_OUTPUT) -> dict[str, Path]:
    require(base.sha(GATE) == GATE_SHA and base.sha(FAILURE) == FAILURE_SHA and
            all((ROOT / relative).is_file() for relative in EXTRA),
            "manifest correction lineage/source drift")
    generated = v2.prepare(build, output)
    old_source = generated["sources"]
    source_path = output / "rules-decision-ui-v5-source-bindings-v3.json"
    old_source.rename(source_path)
    sources = json.loads(source_path.read_text())
    rows = {row["path"]: row for row in sources["files"]}
    for relative in EXTRA:
        rows[relative] = {"path": relative, "sha256": base.sha(ROOT / relative),
                          "scope": "v5_manifest_runner_field_parity_correction"}
    ordered = [rows[key] for key in sorted(rows)]
    sources.update({
        "schema": "phase5_rules_decision_ui_v5_exact16_source_bindings_v3",
        "gate_sha256": v2.v1.GATE_SHA,
        "manifest_contract_correction_gate_sha256": GATE_SHA,
        "missing_gate_field_failure_receipt_sha256": FAILURE_SHA,
        "file_count": len(ordered), "files": ordered,
        "aggregate_sha256": base.aggregate(ordered),
    })
    write(source_path, sources)

    old_ready = generated["readiness"]
    ready_path = output / "rules-decision-ui-v5-exact16-ready-v3.json"
    old_ready.rename(ready_path)
    ready = json.loads(ready_path.read_text())
    ready["package_revision"] = "v3_manifest_runner_field_parity_corrected"
    ready["paths"]["source_binding_manifest"] = str(source_path.resolve())
    ready["paths"]["manifest_contract_correction_gate"] = str(GATE)
    ready["paths"]["missing_gate_field_failure_receipt"] = str(FAILURE)
    ready["frozen_bindings"]["source_binding_manifest_sha256"] = base.sha(source_path)
    ready["frozen_bindings"]["source_aggregate_sha256"] = sources["aggregate_sha256"]
    ready["frozen_bindings"]["manifest_contract_correction_gate_sha256"] = GATE_SHA
    ready["frozen_bindings"]["missing_gate_field_failure_receipt_sha256"] = FAILURE_SHA
    ready["manifest_contract_correction"] = {
        "top_level_gate_sha256_present": True,
        "gate_sha256": v2.v1.GATE_SHA,
        "full_runner_json_contract_parity_tested": True,
        "runner_source_and_binary_unchanged": True,
        "old_v2_authorization_reuse_forbidden": True,
    }
    write(ready_path, ready)

    old_auth = generated["authorization"]
    auth_path = output / "rules-decision-ui-v5-authorization-template-not-issued-v3.json"
    old_auth.rename(auth_path)
    auth = json.loads(auth_path.read_text())
    auth["ready_manifest_sha256"] = base.sha(ready_path)
    auth["source_binding_manifest_sha256"] = base.sha(source_path)
    auth["source_aggregate_sha256"] = sources["aggregate_sha256"]
    write(auth_path, auth)

    old_receipt = generated["receipt"]
    receipt_path = output / "offline-readiness-receipt-v3.json"
    old_receipt.rename(receipt_path)
    receipt = json.loads(receipt_path.read_text())
    receipt.update({
        "schema": "phase5_rules_decision_ui_v5_exact16_offline_readiness_v3",
        "status": "MANIFEST_FIELD_PARITY_CORRECTED_EXACT16_NOT_RUN_NO_AUTH",
        "readiness_sha256": base.sha(ready_path),
        "authorization_template_sha256": base.sha(auth_path),
        "source_binding_manifest_sha256": base.sha(source_path),
        "manifest_contract_correction_gate_sha256": GATE_SHA,
        "missing_gate_field_failure_receipt_sha256": FAILURE_SHA,
    })
    write(receipt_path, receipt)
    return {"readiness": ready_path, "authorization": auth_path,
            "sources": source_path, "receipt": receipt_path}


if __name__ == "__main__":
    parser = argparse.ArgumentParser()
    parser.add_argument("--build", type=Path, required=True)
    parser.add_argument("--output", type=Path, default=DEFAULT_OUTPUT)
    args = parser.parse_args()
    try:
        print(json.dumps({key: str(path) for key, path in
                          prepare(args.build, args.output).items()}, sort_keys=True))
    except (OSError, ValueError, json.JSONDecodeError, RuntimeError) as error:
        print("FAIL: " + str(error))
        raise SystemExit(2)
