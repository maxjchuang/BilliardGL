#!/usr/bin/env python3
"""Read-only CPU validator for Phase2 v10i formal readiness v4."""

from __future__ import annotations

import argparse
import json
from pathlib import Path
import sys
from typing import Any

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT))

from scripts import prepare_phase2_v10i_formal_six_scene_ready_v4 as v4
from scripts import validate_phase2_v10i_formal_six_scene_ready_v2 as base


DEFAULT_READY_ROOT = v4.V4_ROOT


def load(path: Path) -> dict[str, Any]:
    value = json.loads(path.read_text(encoding="utf-8"))
    base.require(isinstance(value, dict), f"{path}: object required")
    return value


def validate(ready_root: Path = DEFAULT_READY_ROOT) -> dict[str, Any]:
    ready_root = ready_root.resolve(strict=True)
    paths = {
        "gate": ready_root / "implementation-gate-v4.json",
        "plan": ready_root / "six-scene-plan-not-run-v4.json",
        "machine": ready_root / "machine-evidence-template-not-run-v4.json",
        "authorization":
            ready_root / "one-shot-authorization-template-not-issued-v4.json",
        "readiness": ready_root / "readiness-manifest-v4.json",
        "receipt": ready_root / "readiness-receipt-v4.json",
    }
    base.require(all(path.is_file() for path in paths.values()),
                 "readiness-v4 artifact set incomplete")
    base.require(not any(ready_root.glob("*.ppm")) and
                 not any(ready_root.glob("*.png")),
                 "readiness-v4 may not contain capture output")
    values = {key: load(path) for key, path in paths.items()}
    readiness = values["readiness"]
    base.require(readiness.get("schema") ==
                 "phase2_v10i_formal_six_scene_readiness_manifest_v4" and
                 readiness.get("status") ==
                 "READY_FOR_TARGETED_GPU_AUTHORIZATION_NO_GPU" and
                 readiness.get("implementation_ready") is True,
                 "readiness-v4 identity/status")
    base.validate_false_flags(readiness, "readiness-v4")
    lineage = (
        ("v3_readiness_lineage", v4.V3_READINESS,
         v4.V3_READINESS_SHA256),
        ("v3_failure_receipt_lineage", v4.FAILURE_RECEIPT,
         v4.FAILURE_RECEIPT_SHA256),
        ("v3_consumed_marker_lineage", v4.CONSUMED_MARKER,
         v4.CONSUMED_MARKER_SHA256),
        ("v4_camera_design_gate_lineage", v4.CAMERA_GATE,
         v4.CAMERA_GATE_SHA256),
    )
    for field, expected_path, expected_sha in lineage:
        binding = readiness.get(field)
        base.require(base.validate_binding(binding, field) == expected_path and
                     binding.get("sha256") == expected_sha,
                     field + " exact lineage")
    base.require(tuple(readiness.get("approved_scenes", ())) == v4.SCENE_IDS and
                 readiness.get("formal_registry_version") == 2 and
                 readiness.get("expected_semantic_camera") == v4.EXPECTED_CAMERA,
                 "formal registry/camera map")

    sources = readiness.get("source_bindings")
    base.require(isinstance(sources, dict) and
                 set(sources) == set(v4.SOURCE_BINDINGS),
                 "source binding inventory")
    for name, binding in sources.items():
        base.require(base.validate_binding(binding, "source " + name) ==
                     v4.ROOT / name, "source path " + name)
    executable = base.validate_binding(
        readiness.get("fresh_release_executable"), "fresh Release executable")
    base.require(executable == v4.DEFAULT_EXECUTABLE.resolve(strict=True),
                 "integrated Release executable path")

    package_binding = readiness.get("self_contained_asset_package")
    package_path = base.validate_binding(package_binding, "asset package")
    package = load(package_path)
    package_root = package_path.parent.resolve(strict=True)
    base.require(package.get("schema") ==
                 "phase2_v10i_self_contained_formal_asset_package_v2" and
                 package.get("self_contained") is True and
                 package.get("immutable") is True and
                 package.get("symlinks_allowed") is False,
                 "formal package identity")
    files = package.get("files")
    base.require(isinstance(files, list) and len(files) == 24 and
                 package.get("required_asset_count") == 6 and
                 package.get("runtime_dependency_count") == 18 and
                 package_binding.get("asset_root") == str(package_root),
                 "formal package counts/root")
    for item in files:
        file_path = package_root / item["filename"]
        digest = base.sha256(file_path).removeprefix("sha256:")
        base.require(file_path.is_file() and not file_path.is_symlink() and
                     item.get("source_sha256") == digest and
                     item.get("packaged_sha256") == digest,
                     "formal package file/SHA " + item["filename"])

    generated = readiness.get("generated_bindings")
    expected_generated = {key: paths[key] for key in (
        "gate", "plan", "machine", "authorization")}
    key_map = {
        "implementation_gate": "gate", "six_scene_plan": "plan",
        "machine_template": "machine",
        "authorization_template": "authorization",
    }
    base.require(isinstance(generated, dict) and
                 set(generated) == set(key_map), "generated binding inventory")
    for generated_key, path_key in key_map.items():
        base.require(base.validate_binding(
            generated[generated_key], generated_key) == paths[path_key],
            "generated path " + generated_key)

    gate = values["gate"]
    base.require(gate.get("status") ==
                 "PASS_CPU_STATIC_CORRECTION_READY_NO_GPU",
                 "implementation gate status")
    base.validate_false_flags(gate, "implementation gate v4")
    frozen = gate.get("frozen_failure_lineage", {})
    base.require(frozen.get("additional_v3_capture_allowed") is False,
                 "v3 failure must remain consumed")
    correction = gate.get("correction", {})
    base.require(correction.get("formal_registry_version") == 2 and
                 correction.get("expected_semantic_camera") ==
                    v4.EXPECTED_CAMERA and
                 correction.get("low_aim_forbidden_commands") ==
                    ["orbit_camera", "pan_camera", "zoom_camera"] and
                 len(correction.get("low_aim_exact_recipe", [])) == 4 and
                 correction.get("normal_product_camera_transition_unchanged")
                    is True and
                 correction.get("renderer_or_asset_modified_for_phase2_correction")
                    is False and
                 correction.get("interest_region_or_threshold_relaxed") is False,
                 "v4 camera correction contract")
    cpu = gate.get("cpu_static_preflight", {})
    base.require(cpu.get("status") == "PASS" and
                 cpu.get("gpu_or_graphics_context_used") is False and
                 cpu.get("required_cases") == ["break", "low_aim"] and
                 len(cpu.get("shared_assertions", [])) == 6 and
                 len(cpu.get("break_assertions", [])) == 3 and
                 len(cpu.get("low_aim_assertions", [])) == 4 and
                 all(item.get("passed") is True for section in (
                     "shared_assertions", "break_assertions",
                     "low_aim_assertions") for item in cpu[section]),
                 "CPU/static preflight report")

    plan = values["plan"]
    base.require(plan.get("status") ==
                 "NOT_RUN_READY_FOR_TARGETED_GPU_AUTHORIZATION" and
                 plan.get("required_next_targeted_scenes") ==
                    ["break", "low_aim"] and
                 plan.get("full_six_scene_rerun_authorized_by_readiness")
                    is False,
                 "targeted next gate")
    base.validate_false_flags(plan, "six-scene plan v4")
    contract = plan.get("capture_contract", {})
    base.require(contract.get("renderer") == "modern" and
                 contract.get("automation_extra_args") ==
                    ["--renderer", "modern"] and
                 contract.get("formal_registry_version") == 2 and
                 contract.get("expected_semantic_camera") ==
                    v4.EXPECTED_CAMERA and
                 contract.get("transitioning_required") is False and
                 contract.get("resolution") ==
                    {"width": 1920, "height": 1080} and
                 contract.get("quality") == "medium" and
                 contract.get("msaa_samples") == 4 and
                 contract.get("warmup_frames") == 120 and
                 contract.get("sample_frames") == 300,
                 "capture contract")
    rows = plan.get("rows")
    base.require(isinstance(rows, list) and
                 tuple(item.get("scene_id") for item in rows) == v4.SCENE_IDS and
                 all(item.get("status") == "NOT_RUN" and
                     item.get("process_started") is False and
                     item.get("screenshot_created") is False
                     for item in rows), "six scenes must remain NOT_RUN")

    machine = values["machine"]
    base.validate_false_flags(machine, "machine template v4")
    base.require(machine.get("status") == "NOT_RUN_READY" and
                 machine.get("required_registry_version") == 2 and
                 tuple(item.get("scene_id")
                       for item in machine.get("scenes", ())) == v4.SCENE_IDS and
                 all(item.get("status") == "NOT_RUN" and
                     item.get("semantic_camera") is None
                     for item in machine["scenes"]),
                 "machine evidence remains NOT_RUN")
    authorization = values["authorization"]
    base.require(authorization == v4.authorization_template(),
                 "authorization template must remain exact and non-executable")
    safety = readiness.get("offline_safety", {})
    base.require(all(safety.get(field) is False for field in (
        "can_start_candidate_executable", "can_create_cocoa_context",
        "can_call_opengl", "can_create_screenshot", "can_issue_authorization",
        "imports_subprocess")), "offline safety")
    receipt = values["receipt"]
    base.validate_false_flags(receipt, "readiness receipt v4")
    base.require(base.validate_binding(receipt.get("readiness_manifest"),
                                       "receipt readiness") == paths["readiness"] and
                 base.validate_binding(receipt.get("fresh_release_executable"),
                                       "receipt executable") == executable,
                 "readiness receipt bindings")
    return {
        "status": "PASS_CPU_ONLY_READINESS_V4",
        "readiness_manifest_sha256":
            base.sha256(paths["readiness"]).removeprefix("sha256:"),
        "release_executable_sha256":
            base.sha256(executable).removeprefix("sha256:"),
        "asset_package_manifest_sha256":
            base.sha256(package_path).removeprefix("sha256:"),
        "camera_design_gate_sha256": v4.CAMERA_GATE_SHA256,
        "frozen_failure_receipt_sha256": v4.FAILURE_RECEIPT_SHA256,
        "not_run_scene_count": len(v4.SCENE_IDS),
        "next_targeted_scene_count": 2,
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
