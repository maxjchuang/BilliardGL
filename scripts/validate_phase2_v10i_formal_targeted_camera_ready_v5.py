#!/usr/bin/env python3
"""Read-only CPU validator for executable targeted readiness v5."""

from __future__ import annotations

import argparse
import json
from pathlib import Path
import sys
from typing import Any

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT))

from scripts import prepare_phase2_v10i_formal_targeted_camera_ready_v5 as v5
from scripts import validate_phase2_v10i_formal_six_scene_ready_v2 as base


DEFAULT_READY_ROOT = v5.V5_ROOT


def load(path: Path) -> dict[str, Any]:
    value = json.loads(path.read_text(encoding="utf-8"))
    base.require(isinstance(value, dict), f"{path}: object required")
    return value


def validate(ready_root: Path = DEFAULT_READY_ROOT) -> dict[str, Any]:
    ready_root = ready_root.resolve(strict=True)
    paths = {
        "implementation_gate": ready_root / "implementation-gate-v5.json",
        "targeted_plan": ready_root / "targeted-plan-not-run-v5.json",
        "machine_template": ready_root / "machine-template-not-run-v5.json",
        "authorization_template":
            ready_root / "targeted-authorization-template-not-issued-v5.json",
        "readiness": ready_root / "readiness-manifest-v5.json",
        "receipt": ready_root / "readiness-receipt-v5.json",
    }
    base.require(all(path.is_file() for path in paths.values()),
                 "targeted readiness-v5 artifact set incomplete")
    base.require(not any(ready_root.glob("*.ppm")) and
                 not any(ready_root.glob("*.png")) and
                 not any(ready_root.glob("*.consumed.json")),
                 "readiness-v5 may not contain output or consumed marker")
    values = {key: load(path) for key, path in paths.items()}
    readiness = values["readiness"]
    base.require(readiness.get("schema") ==
                 "phase2_v10i_formal_targeted_camera_readiness_v5" and
                 readiness.get("status") ==
                 "READY_FOR_EXACT_BREAK_LOW_AIM_AUTHORIZATION_NO_GPU" and
                 readiness.get("implementation_ready") is True,
                 "targeted readiness-v5 identity/status")
    base.validate_false_flags(readiness, "targeted readiness-v5")
    for field, path, digest in (
            ("frozen_v4_lineage", v5.V4_READINESS,
             v5.V4_READINESS_SHA256),
            ("camera_design_gate_lineage", v5.CAMERA_GATE,
             v5.CAMERA_GATE_SHA256),
            ("frozen_failure_receipt_lineage", v5.FAILURE_RECEIPT,
             v5.FAILURE_RECEIPT_SHA256)):
        binding = readiness.get(field)
        base.require(base.validate_binding(binding, field) == path and
                     binding.get("sha256") == digest,
                     field + " exact")
    base.require(readiness.get("targeted_scene_ids_in_order") ==
                    list(v5.SCENE_IDS) and
                 readiness.get("authorized_capture_count_when_issued") == 2 and
                 readiness.get("full_six_scene_capture_authorized") is False and
                 readiness.get("expected_semantic_camera") ==
                    v5.EXPECTED_CAMERA,
                 "exact targeted scope")
    sources = readiness.get("source_bindings")
    base.require(isinstance(sources, dict) and
                 set(sources) == set(v5.SOURCE_BINDINGS),
                 "targeted source binding inventory")
    for name, binding in sources.items():
        base.require(base.validate_binding(binding, "source " + name) ==
                     v5.ROOT / name, "targeted source path " + name)
    executable = base.validate_binding(
        readiness.get("fresh_integrated_release_executable"),
        "integrated Release executable")
    base.require(executable == v5.DEFAULT_EXECUTABLE.resolve(strict=True),
                 "integrated executable path")
    package_binding = readiness.get("self_contained_asset_package")
    package_path = base.validate_binding(package_binding, "formal package")
    package = load(package_path)
    package_root = package_path.parent.resolve(strict=True)
    base.require(package.get("schema") ==
                    "phase2_v10i_self_contained_formal_asset_package_v2" and
                 package.get("self_contained") is True and
                 package.get("immutable") is True and
                 len(package.get("files", [])) == 24 and
                 package_binding.get("asset_root") == str(package_root),
                 "formal package contract")
    for item in package["files"]:
        path = package_root / item["filename"]
        digest = base.sha256(path).removeprefix("sha256:")
        base.require(path.is_file() and not path.is_symlink() and
                     item.get("source_sha256") == digest and
                     item.get("packaged_sha256") == digest,
                     "formal package SHA " + item["filename"])
    generated = readiness.get("generated_bindings")
    base.require(isinstance(generated, dict) and
                 set(generated) == {
                     "implementation_gate", "targeted_plan",
                     "machine_template", "authorization_template"},
                 "generated targeted binding inventory")
    for key in generated:
        base.require(base.validate_binding(generated[key], key) == paths[key],
                     "generated targeted path " + key)

    gate = values["implementation_gate"]
    base.require(gate.get("status") ==
                 "PASS_EXECUTABLE_TARGETED_PROTOCOL_NO_GPU",
                 "targeted implementation gate")
    base.validate_false_flags(gate, "targeted implementation gate")
    contract = gate.get("targeted_contract", {})
    base.require(contract.get("cli_option") ==
                    "--formal-targeted-scenes break,low_aim" and
                 contract.get("scene_ids_in_order") == list(v5.SCENE_IDS) and
                 contract.get("capture_count") == 2 and
                 contract.get("binds_authorization_id_and_nonce") is True and
                 contract.get("wrong_subset_rejects_before_output_or_process")
                    is True and
                 contract.get("bad_authorization_creates_no_marker_output_or_process")
                    is True and
                 contract.get("full_six_scene_path_preserved") is True and
                 contract.get("full_six_scene_capture_authorized") is False,
                 "targeted executable contract")
    consumption = gate.get("single_use_consumption", {})
    base.require(consumption.get("required") is True and
                 consumption.get("timing") == "BEFORE_OUTPUT_OR_PROCESS_START" and
                 consumption.get("atomic_flags") ==
                    ["O_CREAT", "O_EXCL", "O_WRONLY"] and
                 "authorization_nonce" in consumption.get("marker_binds", []) and
                 consumption.get("failure_permanently_consumes_authorization")
                    is True and
                 consumption.get("durability_before_process") ==
                    "flush_plus_fsync" and
                 consumption.get("fsync_failure_starts_no_output_or_process")
                    is True and
                 consumption.get("incomplete_marker_is_never_deleted_or_reused")
                    is True and
                 consumption.get("existing_marker_rejects_before_output_or_process")
                    is True,
                 "atomic single-use consumption")
    output = gate.get("output_contract", {})
    base.require(output.get("manifest_schema") ==
                    "phase2_v10i_formal_targeted_camera_capture_v1" and
                 output.get("registry_version") == 2 and
                 output.get("expected_semantic_camera") ==
                    v5.EXPECTED_CAMERA and
                 output.get("full_six_scene_evidence") is False,
                 "targeted output contract")
    cpu = gate.get("cpu_tests", {})
    base.require(cpu.get("status") == "PASS" and
                 cpu.get("required_case_count") == 10 and
                 cpu.get("gpu_or_graphics_context_used") is False,
                 "targeted CPU tests")

    plan = values["targeted_plan"]
    base.validate_false_flags(plan, "targeted plan v5")
    base.require(plan.get("status") ==
                    "NOT_RUN_READY_FOR_EXACT_TWO_SCENE_AUTHORIZATION" and
                 plan.get("capture_command", [None])[-1] == "1080",
                 "targeted plan identity")
    rows = plan.get("rows")
    base.require(isinstance(rows, list) and len(rows) == 2 and
                 [item.get("scene_id") for item in rows] == list(v5.SCENE_IDS) and
                 all(item.get("status") == "NOT_RUN" and
                     item.get("process_started") is False and
                     item.get("screenshot_created") is False
                     for item in rows), "targeted rows NOT_RUN")
    machine = values["machine_template"]
    base.validate_false_flags(machine, "targeted machine template")
    base.require(machine.get("status") == "NOT_RUN_READY" and
                 machine.get("required_manifest_schema") ==
                    "phase2_v10i_formal_targeted_camera_capture_v1" and
                 [item.get("scene_id") for item in machine.get("scenes", [])] ==
                    list(v5.SCENE_IDS) and
                 all(item.get("status") == "NOT_RUN"
                     for item in machine["scenes"]),
                 "targeted machine template")
    base.require(values["authorization_template"] == v5.authorization_template(),
                 "targeted authorization template must be non-executable")
    safety = readiness.get("offline_safety", {})
    base.require(all(safety.get(field) is False for field in (
        "can_start_candidate_executable", "can_create_cocoa_context",
        "can_call_opengl", "can_create_screenshot", "can_issue_authorization")),
        "targeted readiness offline safety")
    receipt = values["receipt"]
    base.validate_false_flags(receipt, "targeted readiness receipt")
    base.require(base.validate_binding(receipt.get("readiness_manifest"),
                                       "receipt readiness") == paths["readiness"] and
                 base.validate_binding(
                     receipt.get("fresh_integrated_release_executable"),
                     "receipt executable") == executable,
                 "targeted readiness receipt")
    return {
        "status": "PASS_CPU_ONLY_TARGETED_READINESS_V5",
        "readiness_manifest_sha256":
            base.sha256(paths["readiness"]).removeprefix("sha256:"),
        "release_executable_sha256":
            base.sha256(executable).removeprefix("sha256:"),
        "asset_package_manifest_sha256":
            base.sha256(package_path).removeprefix("sha256:"),
        "targeted_scene_count": 2,
        "not_run_scene_count": 2,
        "authorization_issued": False,
        "gpu_executed": False,
    }


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--ready-root", type=Path, default=DEFAULT_READY_ROOT)
    arguments = parser.parse_args()
    try:
        print(json.dumps(validate(arguments.ready_root), indent=2,
                         sort_keys=True))
    except (OSError, ValueError, json.JSONDecodeError,
            base.ValidationError) as error:
        print("FAIL: " + str(error))
        return 2
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
