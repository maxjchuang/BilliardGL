#!/usr/bin/env python3
"""Read-only validator for Phase2 full-six dispatcher-corrected v8 readiness."""

from __future__ import annotations

import argparse
import json
from pathlib import Path
import sys

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT))
from scripts import prepare_phase2_v10i_formal_full_six_reentry_ready_v8 as prep
from scripts import validate_phase2_v10i_formal_six_scene_ready_v2 as base


def load(path: Path) -> dict:
    value = json.loads(path.read_text())
    base.require(isinstance(value, dict), str(path) + ": object required")
    return value


def validate(root: Path = prep.READY_ROOT) -> dict:
    root = root.resolve(strict=True)
    paths = {
        "implementation_gate": root / "implementation-gate-v8.json",
        "six_scene_plan": root / "six-scene-plan-not-run-v8.json",
        "machine_template": root / "machine-evidence-template-not-run-v8.json",
        "authorization_template": root / "one-shot-authorization-template-not-issued-v8.json",
        "dispatch_contract": root / "single-use-dispatch-contract-v8.json",
        "readiness": root / "readiness-manifest-v8.json",
        "receipt": root / "readiness-receipt-v8.json",
    }
    base.require(all(path.is_file() for path in paths.values()),
                 "v8 readiness artifact set incomplete")
    base.require(not any(root.rglob("*.ppm")) and not any(root.rglob("*.png")) and
                 not any(root.rglob("*.consumed.json")),
                 "v8 readiness contains capture/consumption evidence")
    values = {key: load(path) for key, path in paths.items()}
    ready = values["readiness"]
    base.require(ready.get("schema") ==
                 "phase2_v10i_formal_full_six_reentry_readiness_v8" and
                 ready.get("status") ==
                 "READY_FOR_EXACT_FULL_SIX_ONE_SHOT_AUTHORIZATION_NO_GPU" and
                 ready.get("implementation_ready") is True,
                 "v8 readiness identity/status mismatch")
    base.validate_false_flags(ready, "v8 readiness")
    base.require(ready.get("approved_scenes") == list(prep.SCENE_IDS) and
                 ready.get("formal_registry_version") == 2 and
                 ready.get("expected_semantic_camera") == prep.EXPECTED_CAMERA and
                 ready.get("integrated_phase4_source_state") ==
                    "PHASE4_V8_SOURCE_STABLE" and
                 ready.get("future_capture_output_root") ==
                    str(prep.FUTURE_OUTPUT_ROOT.resolve()) and
                 not prep.FUTURE_OUTPUT_ROOT.exists(),
                 "v8 scene/source/output boundary mismatch")
    for field, expected_path, digest in (
            ("reentry_gate", prep.REENTRY_GATE, prep.REENTRY_GATE_SHA256),
            ("targeted_machine_prerequisite", prep.TARGETED_MACHINE,
             prep.TARGETED_MACHINE_SHA256),
            ("targeted_ta_prerequisite", prep.TARGETED_TA,
             prep.TARGETED_TA_SHA256),
            ("full_six_ta_template", prep.TA_TEMPLATE, prep.TA_TEMPLATE_SHA256),
            ("dispatcher_cli_correction_gate", prep.CORRECTION_GATE,
             prep.CORRECTION_GATE_SHA256),
            ("frozen_v7_failure", prep.V7_FAILURE, prep.V7_FAILURE_SHA256)):
        binding = ready.get(field)
        base.require(base.validate_binding(binding, field) ==
                     expected_path.resolve(strict=True) and
                     binding.get("sha256") == digest, field + " exact binding mismatch")
    executable = base.validate_binding(ready.get("fresh_release_executable"),
                                       "v8 executable")
    base.require(executable == prep.DEFAULT_EXECUTABLE.resolve(strict=True) and
                 base.sha256(executable) ==
                 "042a64496195d453f11e0a19770fa207ec448862d0a8396ce98cdaa0f0c017c1",
                 "unchanged Billiards binary binding mismatch")
    package = base.validate_binding(ready.get("self_contained_asset_package"),
                                    "v8 asset package")
    base.require(base.sha256(package) == prep.PACKAGE_MANIFEST_SHA256 and
                 ready["self_contained_asset_package"].get("required_asset_count") == 6 and
                 ready["self_contained_asset_package"].get("runtime_dependency_count") == 18,
                 "unchanged asset package mismatch")
    prep.validate_frozen_package(package.parent)
    sources = ready.get("source_bindings")
    base.require(isinstance(sources, dict) and set(sources) == set(prep.SOURCE_BINDINGS),
                 "v8 source inventory mismatch")
    for name, binding in sources.items():
        base.require(base.validate_binding(binding, "v8 source " + name) ==
                     (ROOT / name).resolve(strict=True),
                     "v8 source path/SHA drift: " + name)
    base.require(sources["scripts/capture_visual_baselines.py"]["sha256"] ==
                 "772ecc72a9ade0234413fbd87425fe8ddd810a85255b3f8274556eb8ecae3b89",
                 "capture driver source changed")
    generated = ready.get("generated_bindings")
    base.require(isinstance(generated, dict) and set(generated) == {
        "implementation_gate", "six_scene_plan", "machine_template",
        "authorization_template", "dispatch_contract"},
        "v8 generated inventory mismatch")
    for key in generated:
        base.require(base.validate_binding(generated[key], "v8 generated " + key) ==
                     paths[key], "v8 generated binding drift: " + key)

    plan = values["six_scene_plan"]
    base.validate_false_flags(plan, "v8 plan")
    base.require(plan.get("schema") == "phase2_v10i_formal_full_six_plan_v8" and
                 plan.get("capture_contract") == prep.capture_contract(),
                 "v8 plan threshold contract mismatch")
    rows = plan.get("rows", [])
    base.require(len(rows) == 6 and
                 [row.get("scene_id") for row in rows] == list(prep.SCENE_IDS),
                 "v8 exact six order mismatch")
    for ordinal, (row, scene) in enumerate(zip(rows,
                                               prep.FORMAL_MODERN_VISUAL_SCENES), 1):
        state, preset = prep.REGISTRY_CAMERA[scene.scene_id]
        base.require(row.get("ordinal") == ordinal and row.get("status") == "NOT_RUN" and
                     row.get("recipe") == prep.recipe(scene) and
                     any(item["command"] == "set_camera_preset" and
                         item["params"].get("instant") is True
                         for item in row["recipe"]) and
                     row.get("presentation_query") == "get_presentation_state" and
                     row.get("expected_presentation") == {
                         "state": state, "preset": preset, "transitioning": False,
                         "finite_pose_and_projection": True} and
                     row.get("interest_region") == prep.region(scene),
                     scene.scene_id + ": v8 semantic row mismatch")
        base.require(all(row.get(field) is False for field in (
            "process_started", "screenshot_created", "semantic_camera_attested",
            "loaded_asset_attestation", "performance_measured", "machine_pass",
            "ta_review", "formal_evidence")), scene.scene_id + ": v8 evidence promoted")
    machine = values["machine_template"]
    base.validate_false_flags(machine, "v8 machine")
    base.require(machine.get("schema") ==
                 "phase2_v10i_formal_full_six_machine_template_v8" and
                 machine.get("required_capture_count") == 6 and
                 machine.get("required_fresh_process_count") == 6 and
                 all(item.get("status") == "NOT_RUN" and
                     item.get("machine_pass") is False
                     for item in machine.get("scenes", [])),
                 "v8 machine template mismatch")
    base.require(values["authorization_template"] == prep.authorization_template(),
                 "v8 authorization template must remain NOT_ISSUED")
    dispatch = values["dispatch_contract"]
    base.validate_false_flags(dispatch, "v8 dispatch")
    base.require(dispatch.get("schema") ==
                 "phase2_v10i_formal_full_six_single_use_dispatch_contract_v8" and
                 dispatch.get("authorized_scene_ids_in_order") == list(prep.SCENE_IDS) and
                 dispatch.get("authorized_capture_count") == 6 and
                 dispatch.get("capture_driver_asset_option") == "--asset-package" and
                 dispatch.get("real_parser_contract_test_required") is True and
                 dispatch.get("future_output_canonical_path") ==
                    str(prep.FUTURE_OUTPUT_ROOT.resolve()) and
                 dispatch.get("consumption_timing") ==
                    "BEFORE_OUTPUT_OR_PROCESS_START" and
                 dispatch.get("durability") == "flush_plus_fsync_before_close" and
                 dispatch.get("failure_permanently_consumes_authorization") is True,
                 "v8 corrected dispatch contract mismatch")
    base.require(base.validate_binding(dispatch.get("wrapper"), "v8 wrapper") ==
                 (ROOT / "scripts/run_phase2_v10i_formal_full_six_capture_v8.py").resolve(strict=True) and
                 base.validate_binding(dispatch.get("delegated_implementation"),
                                       "v8 delegated implementation") ==
                 (ROOT / "scripts/run_phase2_v10i_formal_full_six_capture_v7.py").resolve(strict=True),
                 "v8 wrapper/delegation binding mismatch")
    implementation = values["implementation_gate"]
    base.validate_false_flags(implementation, "v8 implementation")
    base.require(implementation.get("single_use_dispatch") == dispatch and
                 implementation.get("production_capture_binary_assets_unchanged") is True and
                 base.validate_binding(implementation.get("dispatcher_cli_correction_gate"),
                                       "implementation correction gate") ==
                    prep.CORRECTION_GATE.resolve(strict=True) and
                 base.validate_binding(implementation.get("frozen_v7_failure"),
                                       "implementation v7 failure") ==
                    prep.V7_FAILURE.resolve(strict=True),
                 "v8 implementation correction lineage mismatch")
    safety = ready.get("offline_safety", {})
    base.require(all(safety.get(field) is False for field in (
        "can_start_candidate_executable", "can_create_cocoa_context",
        "can_call_opengl", "can_create_screenshot", "can_issue_authorization")),
        "v8 offline safety mismatch")
    receipt = values["receipt"]
    base.validate_false_flags(receipt, "v8 receipt")
    base.require(base.validate_binding(receipt.get("readiness_manifest"),
                                       "v8 receipt readiness") == paths["readiness"] and
                 base.validate_binding(receipt.get("fresh_release_executable"),
                                       "v8 receipt binary") == executable,
                 "v8 receipt binding mismatch")
    return {"status": "PASS_CPU_ONLY_FULL_SIX_REENTRY_READINESS_V8",
            "readiness_manifest_sha256": base.sha256(paths["readiness"]),
            "release_executable_sha256": base.sha256(executable),
            "scene_count": 6, "not_run_scene_count": 6,
            "authorization_issued": False, "gpu_executed": False}


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--ready-root", type=Path, default=prep.READY_ROOT)
    try:
        print(json.dumps(validate(parser.parse_args().ready_root), sort_keys=True))
    except (OSError, ValueError, json.JSONDecodeError,
            base.ValidationError, prep.ReadinessError) as error:
        print("FAIL: " + str(error))
        return 2
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
