#!/usr/bin/env python3
"""Read-only CPU validator for Phase2 exact full-six reentry readiness v7."""

from __future__ import annotations

import argparse
import json
from pathlib import Path
import sys

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT))
from scripts import prepare_phase2_v10i_formal_full_six_reentry_ready_v7 as prep
from scripts import validate_phase2_v10i_formal_six_scene_ready_v2 as base


def load(path: Path) -> dict:
    value = json.loads(path.read_text(encoding="utf-8"))
    base.require(isinstance(value, dict), str(path) + ": object required")
    return value


def validate(ready_root: Path = prep.READY_ROOT) -> dict:
    root = ready_root.resolve(strict=True)
    paths = {
        "implementation_gate": root / "implementation-gate-v7.json",
        "six_scene_plan": root / "six-scene-plan-not-run-v7.json",
        "machine_template": root / "machine-evidence-template-not-run-v7.json",
        "authorization_template": root / "one-shot-authorization-template-not-issued-v7.json",
        "dispatch_contract": root / "single-use-dispatch-contract-v7.json",
        "readiness": root / "readiness-manifest-v7.json",
        "receipt": root / "readiness-receipt-v7.json",
    }
    base.require(all(path.is_file() for path in paths.values()),
                 "readiness-v7 artifact set incomplete")
    base.require(not any(root.rglob("*.ppm")) and not any(root.rglob("*.png")) and
                 not any(root.rglob("*.consumed.json")),
                 "readiness-v7 contains capture or consumption evidence")
    values = {key: load(path) for key, path in paths.items()}
    ready = values["readiness"]
    base.require(ready.get("schema") ==
                 "phase2_v10i_formal_full_six_reentry_readiness_v7" and
                 ready.get("status") ==
                 "READY_FOR_EXACT_FULL_SIX_ONE_SHOT_AUTHORIZATION_NO_GPU" and
                 ready.get("implementation_ready") is True,
                 "readiness-v7 identity/status mismatch")
    base.validate_false_flags(ready, "readiness-v7")
    base.require(ready.get("approved_scenes") == list(prep.SCENE_IDS) and
                 ready.get("formal_registry_version") == 2 and
                 ready.get("expected_semantic_camera") == prep.EXPECTED_CAMERA and
                 ready.get("integrated_phase4_source_state") ==
                    "PHASE4_V8_SOURCE_STABLE",
                 "readiness-v7 semantic/source-stable scope mismatch")
    base.require(ready.get("future_capture_output_root") ==
                 str(prep.FUTURE_OUTPUT_ROOT.resolve()) and
                 not prep.FUTURE_OUTPUT_ROOT.exists(),
                 "v7 fresh future output root boundary mismatch")
    for field, expected_path, digest in (
            ("reentry_gate", prep.REENTRY_GATE, prep.REENTRY_GATE_SHA256),
            ("targeted_machine_prerequisite", prep.TARGETED_MACHINE,
             prep.TARGETED_MACHINE_SHA256),
            ("targeted_ta_prerequisite", prep.TARGETED_TA,
             prep.TARGETED_TA_SHA256),
            ("full_six_ta_template", prep.TA_TEMPLATE,
             prep.TA_TEMPLATE_SHA256)):
        binding = ready.get(field)
        base.require(base.validate_binding(binding, field) ==
                     expected_path.resolve(strict=True) and
                     binding.get("sha256") == digest, field + " exact binding")
    executable = base.validate_binding(ready.get("fresh_release_executable"),
                                       "fresh Release executable")
    base.require(executable == prep.DEFAULT_EXECUTABLE.resolve(strict=True) and
                 executable.name == "Billiards",
                 "fresh v7 Release executable mismatch")
    sources = ready.get("source_bindings")
    base.require(isinstance(sources, dict) and
                 set(sources) == set(prep.SOURCE_BINDINGS),
                 "v7 source binding inventory mismatch")
    for name, binding in sources.items():
        base.require(base.validate_binding(binding, "source " + name) ==
                     (ROOT / name).resolve(strict=True),
                     "v7 source path/SHA mismatch: " + name)
    package_binding = ready.get("self_contained_asset_package")
    package_path = base.validate_binding(package_binding, "formal asset package")
    base.require(package_path == (prep.base.PACKAGE_ROOT /
                 "formal-asset-package-v2.json").resolve(strict=True) and
                 package_binding.get("sha256") == prep.PACKAGE_MANIFEST_SHA256 and
                 package_binding.get("required_asset_count") == 6 and
                 package_binding.get("runtime_dependency_count") == 18,
                 "formal asset package binding mismatch")
    prep.validate_frozen_package(package_path.parent)
    generated = ready.get("generated_bindings")
    base.require(isinstance(generated, dict) and set(generated) == {
        "implementation_gate", "six_scene_plan", "machine_template",
        "authorization_template", "dispatch_contract"},
        "v7 generated binding inventory mismatch")
    for key in generated:
        base.require(base.validate_binding(generated[key], "generated " + key) ==
                     paths[key], "v7 generated path/SHA mismatch: " + key)

    plan = values["six_scene_plan"]
    base.validate_false_flags(plan, "v7 six-scene plan")
    base.require(plan.get("schema") == "phase2_v10i_formal_full_six_plan_v7" and
                 plan.get("status") ==
                 "NOT_RUN_READY_FOR_FUTURE_EXACT_FULL_SIX_AUTHORIZATION" and
                 plan.get("capture_contract") == prep.capture_contract(),
                 "v7 plan contract/threshold mismatch")
    rows = plan.get("rows", [])
    base.require(len(rows) == 6 and
                 [row.get("scene_id") for row in rows] == list(prep.SCENE_IDS),
                 "v7 six-scene row scope/order mismatch")
    for ordinal, (row, scene) in enumerate(
            zip(rows, prep.FORMAL_MODERN_VISUAL_SCENES), 1):
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
                     scene.scene_id + ": v7 semantic recipe/state/ROI mismatch")
        for field in ("process_started", "screenshot_created",
                      "semantic_camera_attested", "loaded_asset_attestation",
                      "performance_measured", "machine_pass", "ta_review",
                      "formal_evidence"):
            base.require(row.get(field) is False,
                         scene.scene_id + ": v7 evidence must be false")
    machine = values["machine_template"]
    base.validate_false_flags(machine, "v7 machine template")
    base.require(machine.get("schema") ==
                 "phase2_v10i_formal_full_six_machine_template_v7" and
                 machine.get("required_capture_count") == 6 and
                 machine.get("required_fresh_process_count") == 6 and
                 machine.get("required_scene_ids_in_order") == list(prep.SCENE_IDS) and
                 all(item.get("status") == "NOT_RUN" and
                     item.get("machine_pass") is False
                     for item in machine.get("scenes", [])),
                 "v7 machine template mismatch")
    base.require(values["authorization_template"] == prep.authorization_template(),
                 "v7 authorization template must remain non-executable")
    dispatch = values["dispatch_contract"]
    base.validate_false_flags(dispatch, "v7 dispatch contract")
    wrapper_binding = dispatch.get("wrapper")
    implementation_binding = dispatch.get("delegated_implementation")
    base.require(dispatch.get("schema") ==
                 "phase2_v10i_formal_full_six_single_use_dispatch_contract_v7" and
                 dispatch.get("authorized_scene_ids_in_order") == list(prep.SCENE_IDS) and
                 dispatch.get("authorized_capture_count") == 6 and
                 dispatch.get("consumption_timing") ==
                    "BEFORE_OUTPUT_OR_PROCESS_START" and
                 dispatch.get("atomic_flags") == ["O_CREAT", "O_EXCL", "O_WRONLY"] and
                 dispatch.get("durability") == "flush_plus_fsync_before_close" and
                 dispatch.get("failure_permanently_consumes_authorization") is True and
                 dispatch.get("bad_input_creates_no_marker_output_or_process") is True and
                 dispatch.get("future_output_canonical_path") ==
                    str(prep.FUTURE_OUTPUT_ROOT.resolve()) and
                 base.validate_binding(wrapper_binding, "v7 wrapper") ==
                    (ROOT / "scripts/run_phase2_v10i_formal_full_six_capture_v7.py").resolve(strict=True) and
                 base.validate_binding(implementation_binding,
                                       "v7 delegated implementation") ==
                    (ROOT / "scripts/run_phase2_v10i_formal_full_six_capture_v6.py").resolve(strict=True),
                 "v7 single-use dispatch contract mismatch")
    implementation = values["implementation_gate"]
    base.validate_false_flags(implementation, "v7 implementation")
    base.require(implementation.get("schema") ==
                 "phase2_v10i_formal_full_six_reentry_implementation_gate_v7" and
                 implementation.get("integrated_phase4_source_state") ==
                    "PHASE4_V8_SOURCE_STABLE" and
                 implementation.get("single_use_dispatch") == dispatch,
                 "v7 implementation gate mismatch")
    safety = ready.get("offline_safety", {})
    base.require(all(safety.get(field) is False for field in (
        "can_start_candidate_executable", "can_create_cocoa_context",
        "can_call_opengl", "can_create_screenshot", "can_issue_authorization")),
        "v7 offline safety mismatch")
    receipt = values["receipt"]
    base.validate_false_flags(receipt, "v7 readiness receipt")
    base.require(receipt.get("schema") ==
                 "phase2_v10i_formal_full_six_reentry_readiness_receipt_v7" and
                 base.validate_binding(receipt.get("readiness_manifest"),
                                       "receipt readiness") == paths["readiness"] and
                 base.validate_binding(receipt.get("fresh_release_executable"),
                                       "receipt executable") == executable,
                 "v7 receipt binding mismatch")
    return {
        "status": "PASS_CPU_ONLY_FULL_SIX_REENTRY_READINESS_V7",
        "readiness_manifest_sha256": base.sha256(paths["readiness"]),
        "release_executable_sha256": base.sha256(executable),
        "formal_asset_package_sha256": base.sha256(package_path),
        "scene_count": 6, "not_run_scene_count": 6,
        "authorization_issued": False, "gpu_executed": False,
    }


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--ready-root", type=Path, default=prep.READY_ROOT)
    try:
        print(json.dumps(validate(parser.parse_args().ready_root),
                         indent=2, sort_keys=True))
    except (OSError, ValueError, json.JSONDecodeError,
            base.ValidationError, prep.ReadinessError) as error:
        print("FAIL: " + str(error))
        return 2
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
