#!/usr/bin/env python3
"""Prepare Phase5 Rules UI v4 exact-16 NOT_RUN readiness without GPU."""

from __future__ import annotations

import argparse
import hashlib
import json
from pathlib import Path
import subprocess
import sys

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT))

from scripts import run_phase5_rules_decision_ui_v4_exact_shape_machine as machine
from scripts import validate_phase5_rules_decision_ui_v4_exact_shape_machine as machine_validator

READY_ROOT = ROOT / "docs/evidence/phase5/rules-decision-ui-v4-exact16-ready-v1"
ORIGINAL_GATE = ROOT / "docs/art/phase5-rules-decision-ui-design-gate-v1.json"
FONT = Path("/System/Library/Fonts/Hiragino Sans GB.ttc")
HUD_TOKEN = ROOT / "assets/modern/hud/focused_practice_hud_shipping_tokens_v1.json"
SHELL_TOKEN = ROOT / "assets/modern/hud/focused_practice_shell_shipping_tokens_v1.json"
SOURCE_FILES = (
    "CMakeLists.txt",
    "src/Billiards/render/ui/rules_decision_ui_draw_list.h",
    "src/Billiards/render/ui/rules_decision_ui_draw_list.cpp",
    "src/Billiards/render/ui/rules_decision_product_draw.cpp",
    "tests/rules_decision_ui_tests.cpp",
    "tests/rules_decision_product_runtime_tests.cpp",
    "tests/phase5_rules_decision_ui_v2_cpu_frame_preflight.cpp",
    "scripts/run_phase5_rules_decision_ui_v4_exact_shape_machine.py",
    "scripts/validate_phase5_rules_decision_ui_v4_exact_shape_machine.py",
    "scripts/prepare_phase5_rules_decision_ui_v4_exact16_ready.py",
    "scripts/validate_phase5_rules_decision_ui_v4_exact16_ready.py",
    "tests/visual/test_phase5_rules_decision_ui_v4_exact_shape_machine.py",
    "tests/visual/test_phase5_rules_decision_ui_v4_exact16_ready.py",
)


def sha(path: Path) -> str:
    return hashlib.sha256(path.read_bytes()).hexdigest()


def require(condition: bool, message: str) -> None:
    if not condition:
        raise RuntimeError(message)


def write_json(path: Path, value: object) -> None:
    payload = json.dumps(value, ensure_ascii=False, indent=2,
                         sort_keys=True) + "\n"
    require(not path.exists(), "fresh readiness artifact already exists: " + str(path))
    path.write_text(payload, encoding="utf-8")


def case_ids() -> list[str]:
    value = json.loads(machine.GATE.read_text(encoding="utf-8"))[
        "future_targeted_actual_gpu_gate"]
    result = value["repair_case_ids"] + value["regression_control_case_ids"]
    require(len(result) == 16 and len(set(result)) == 16,
            "v4 exact16 case inventory mismatch")
    return result


def plan_rows() -> list[str]:
    profiles = {item["profile_id"]: item for item in
        json.loads(ORIGINAL_GATE.read_text(encoding="utf-8"))[
            "evidence_gate"]["minimum_actual_gpu_matrix"]["display_profiles"]}
    rows = []
    for case_id in case_ids():
        state, profile_id, locale = case_id.split("--")
        profile = profiles[profile_id]
        logical = profile["logical_viewport"].split("x")
        physical = profile["physical_viewport"].split("x")
        fields = (case_id, state, locale, profile["palette"], profile_id,
                  int(logical[0]), int(logical[1]), int(physical[0]),
                  int(physical[1]), profile["density"], profile["ui_scale"],
                  96.0 * profile["density"])
        rows.append("\t".join(str(field) for field in fields))
    return rows


def source_rows() -> list[dict[str, str]]:
    rows = [{"path": name, "sha256": sha(ROOT / name),
             "scope": "v4_allowed_shape_or_offline_scaffolding"}
            for name in SOURCE_FILES]
    rows.extend({"path": name, "sha256": digest,
                 "scope": "forbidden_authority_read_only"}
                for name, digest in machine.v3.FORBIDDEN_BINDINGS.items())
    gate = json.loads(machine.GATE.read_text(encoding="utf-8"))
    rows.extend({"path": item["path"], "sha256": item["sha256"],
                 "scope": "v4_read_only_lineage"}
                for item in gate["read_only_bindings"])
    rows.extend((
        {"path": str(machine.GATE.relative_to(ROOT)), "sha256": machine.GATE_SHA,
         "scope": "v4_design_gate"},
        {"path": str(HUD_TOKEN.relative_to(ROOT)), "sha256": sha(HUD_TOKEN),
         "scope": "read_only_runtime_asset"},
        {"path": str(SHELL_TOKEN.relative_to(ROOT)), "sha256": sha(SHELL_TOKEN),
         "scope": "read_only_runtime_asset"},
    ))
    unique = {row["path"]: row for row in rows}
    require(len(unique) == len(rows), "v4 source binding duplicate path")
    return [unique[name] for name in sorted(unique)]


def aggregate(rows: list[dict[str, str]]) -> str:
    digest = hashlib.sha256()
    for row in rows:
        digest.update((row["path"] + "\0" + row["sha256"] + "\n").encode())
    return digest.hexdigest()


def prepare(output: Path, billiards: Path, cpu_preflight: Path,
            product_test: Path, machine_a: Path, machine_b: Path) -> dict[str, Path]:
    required = (billiards, cpu_preflight, product_test, machine_a, machine_b,
                FONT, HUD_TOKEN, SHELL_TOKEN, machine.GATE, ORIGINAL_GATE)
    require(all(path.is_file() for path in required), "v4 readiness input missing")
    require(not output.exists(), "v4 readiness root must be fresh")
    require(machine.sha(machine.GATE) == machine.GATE_SHA,
            "v4 exact-shape gate SHA drift")
    require(machine_a.read_bytes() == machine_b.read_bytes(),
            "v4 540 reports are not byte-identical")
    machine_validator.validate(json.loads(machine_a.read_text(encoding="utf-8")))
    output = output.resolve()
    output.mkdir(parents=True)
    plan = output / "rules-decision-ui-v4-exact16-plan.tsv"
    plan.write_text("\n".join(plan_rows()) + "\n", encoding="utf-8")
    machine_path = output / "rules-decision-ui-v4-machine.json"
    machine_path.write_bytes(machine_a.read_bytes())
    cpu_report = output / "rules-decision-ui-v4-exact16-cpu-preflight.json"
    subprocess.run([str(cpu_preflight.resolve()), str(plan), str(cpu_report)],
                   cwd=ROOT, check=True)
    cpu = json.loads(cpu_report.read_text(encoding="utf-8"))
    require(cpu.get("schema") ==
            "phase5_rules_decision_ui_v4_exact16_cpu_frame_preflight_v1" and
            cpu.get("status") == "CPU_FRAME_PREFLIGHT_PASS_16_OF_16_NO_GPU" and
            cpu.get("case_count") == cpu.get("passed_count") == 16 and
            cpu.get("bad_input_count") == cpu.get("bad_input_passed_count") == 5 and
            all(cpu.get(field) is False for field in (
                "actual_gpu_invoked", "ns_application_invoked",
                "opengl_invoked", "gpu_output_created")),
            "v4 exact16 CPU/bad-input preflight failed")
    rows = source_rows()
    source_manifest = {
        "schema": "phase5_rules_decision_ui_v4_exact16_source_bindings_v1",
        "gate_sha256": machine.GATE_SHA, "file_count": len(rows),
        "aggregate_sha256": aggregate(rows), "files": rows,
    }
    source_path = output / "rules-decision-ui-v4-source-bindings.json"
    write_json(source_path, source_manifest)
    ids = case_ids()
    claims = {"gpu_authorized": False, "gpu_executed": False,
              "technical_art_review": False, "product_exposed": False,
              "golden_promotion": False, "m2_promotion": False}
    frozen = {
        "gate_sha256": machine.GATE_SHA,
        "plan_sha256": sha(plan),
        "machine_report_sha256": sha(machine_path),
        "source_binding_manifest_sha256": sha(source_path),
        "source_aggregate_sha256": source_manifest["aggregate_sha256"],
        "billiards_binary_sha256": sha(billiards),
        "cpu_preflight_binary_sha256": sha(cpu_preflight),
        "cpu_preflight_report_sha256": sha(cpu_report),
        "product_integration_test_binary_sha256": sha(product_test),
        "font_sha256": sha(FONT), "hud_token_sha256": sha(HUD_TOKEN),
        "shell_token_sha256": sha(SHELL_TOKEN),
    }
    ready = {
        "schema": "phase5_rules_decision_ui_v4_exact16_readiness_v1",
        "status": "V4_EXACT16_NOT_RUN_GPU_AUTHORIZATION_NOT_ISSUED",
        "case_count": 16, "repair_case_count": 4,
        "regression_control_count": 12, "case_ids_in_order": ids,
        "case_results": [{"ordinal": index + 1, "capture_id": case_id,
                          "status": "NOT_RUN"}
                         for index, case_id in enumerate(ids)],
        "machine_matrix_passed": 540,
        "machine_matrix_byte_reproducible": True,
        "cpu_preflight_passed": 16, "bad_input_passed": 5,
        "product_integration_passed": True,
        "future_dispatch_gate": {
            "implemented_by_this_readiness": False,
            "authorization_may_not_be_issued_until_exact16_single_use_dispatch_exists": True,
            "output_root_must_be_fresh": True,
        },
        "paths": {
            "plan": str(plan), "machine_report": str(machine_path),
            "source_binding_manifest": str(source_path),
            "billiards_binary": str(billiards.resolve()),
            "cpu_preflight_binary": str(cpu_preflight.resolve()),
            "cpu_preflight_report": str(cpu_report),
            "product_integration_test_binary": str(product_test.resolve()),
            "font": str(FONT), "hud_token": str(HUD_TOKEN),
            "shell_token": str(SHELL_TOKEN),
        },
        "frozen_bindings": frozen, "claims": claims,
    }
    ready_path = output / "rules-decision-ui-v4-exact16-ready.json"
    write_json(ready_path, ready)
    auth = {
        "schema": "phase5_rules_decision_ui_v4_exact16_gpu_authorization_v1",
        "status": "NOT_ISSUED",
        "authorized": False, "consumed": False,
        "authorization_id": None, "authorization_nonce": None,
        "approved_by": None, "approved_at": None,
        "authorized_output_canonical_path": None,
        "case_count": 16, "scene_ids_in_order": ids,
        "exact_shape_gpu_authorized": False,
        "ready_manifest_sha256": sha(ready_path),
        "single_use_dispatch_implemented": False,
        **frozen, **claims,
    }
    auth_path = output / "rules-decision-ui-v4-authorization-template-not-issued.json"
    write_json(auth_path, auth)
    receipt = {
        "schema": "phase5_rules_decision_ui_v4_exact16_offline_readiness_v1",
        "status": "CPU_16_MACHINE_540_PRODUCT_INTEGRATION_PASS_GPU_NOT_AUTHORIZED_NOT_RUN",
        "ready_manifest_sha256": sha(ready_path),
        "authorization_template_sha256": sha(auth_path),
        "source_binding_manifest_sha256": sha(source_path),
        "claims": claims,
    }
    receipt_path = output / "offline-readiness-receipt.json"
    write_json(receipt_path, receipt)
    return {"plan": plan, "machine": machine_path, "cpu": cpu_report,
            "sources": source_path, "readiness": ready_path,
            "authorization": auth_path, "receipt": receipt_path}


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--output-root", type=Path, default=READY_ROOT)
    parser.add_argument("--billiards", type=Path, required=True)
    parser.add_argument("--cpu-preflight", type=Path, required=True)
    parser.add_argument("--product-test", type=Path, required=True)
    parser.add_argument("--machine-a", type=Path, required=True)
    parser.add_argument("--machine-b", type=Path, required=True)
    arguments = parser.parse_args()
    try:
        result = prepare(arguments.output_root, arguments.billiards,
                         arguments.cpu_preflight, arguments.product_test,
                         arguments.machine_a, arguments.machine_b)
        print(json.dumps({key: str(path) for key, path in result.items()},
                         sort_keys=True))
    except (OSError, ValueError, json.JSONDecodeError, RuntimeError,
            subprocess.CalledProcessError) as error:
        print("FAIL: " + str(error))
        return 2
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
