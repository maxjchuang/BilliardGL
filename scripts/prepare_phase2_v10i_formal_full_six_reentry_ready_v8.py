#!/usr/bin/env python3
"""Prepare Phase2 full-six v8 readiness after dispatcher-only CLI correction."""

from __future__ import annotations

import argparse
import json
from pathlib import Path
import sys
import tempfile

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT))
from scripts import prepare_phase2_v10i_formal_full_six_reentry_ready_v7 as v7
from scripts import prepare_phase2_v10i_formal_six_scene_ready_v2 as base

READY_ROOT = ROOT / "docs/art/phase2-v10i-formal-full-six-reentry-ready-v8"
DEFAULT_EXECUTABLE = v7.DEFAULT_EXECUTABLE
FUTURE_OUTPUT_ROOT = ROOT / "docs/evidence/phase2-v10i-formal-full-six-capture-v8"
CORRECTION_GATE = ROOT / "docs/art/phase2-v10i-formal-full-six-v8-dispatcher-cli-correction-gate-v1.json"
CORRECTION_GATE_SHA256 = "982b41a37ed04131e801787eb219549b041592d0af25382e5ae11d909b60d199"
V7_FAILURE = ROOT / "docs/evidence/phase2-v10i-formal-full-six-v7-preprocess-dispatch-failure-receipt-v1.json"
V7_FAILURE_SHA256 = "a50e19521543162b15e7871b39526dbaa38afae2a36b8e2cf906a3fbf53ad129"
REENTRY_GATE = v7.REENTRY_GATE
REENTRY_GATE_SHA256 = v7.REENTRY_GATE_SHA256
TARGETED_MACHINE = v7.TARGETED_MACHINE
TARGETED_MACHINE_SHA256 = v7.TARGETED_MACHINE_SHA256
TARGETED_TA = v7.TARGETED_TA
TARGETED_TA_SHA256 = v7.TARGETED_TA_SHA256
TA_TEMPLATE = v7.TA_TEMPLATE
TA_TEMPLATE_SHA256 = v7.TA_TEMPLATE_SHA256
PACKAGE_MANIFEST_SHA256 = v7.PACKAGE_MANIFEST_SHA256
SCENE_IDS = v7.SCENE_IDS
EXPECTED_CAMERA = v7.EXPECTED_CAMERA
FORMAL_MODERN_VISUAL_SCENES = v7.FORMAL_MODERN_VISUAL_SCENES
REGISTRY_CAMERA = v7.REGISTRY_CAMERA
REGISTRY_VERSION = v7.REGISTRY_VERSION
ReadinessError = v7.ReadinessError
false_flags = v7.false_flags
recipe = v7.recipe
region = v7.region
capture_contract = v7.capture_contract
authorization_template = v7.authorization_template
validate_frozen_package = v7.validate_frozen_package

V7_SCAFFOLDING = {
    "scripts/prepare_phase2_v10i_formal_full_six_reentry_ready_v7.py",
    "scripts/validate_phase2_v10i_formal_full_six_reentry_ready_v7.py",
    "tests/visual/test_phase2_v10i_formal_full_six_wrapper_v7.py",
    "tests/visual/test_phase2_v10i_formal_full_six_reentry_ready_v7.py",
}
V8_SOURCES = (
    "scripts/run_phase2_v10i_formal_full_six_capture_v8.py",
    "scripts/prepare_phase2_v10i_formal_full_six_reentry_ready_v8.py",
    "scripts/validate_phase2_v10i_formal_full_six_reentry_ready_v8.py",
    "scripts/validate_phase2_v10i_formal_full_six_v7_preprocess_failure_receipt.py",
    "scripts/validate_phase2_v10i_formal_full_six_v8_dispatcher_cli_correction_gate.py",
    "tests/visual/test_phase2_v10i_formal_full_six_wrapper_v8.py",
    "tests/visual/test_phase2_v10i_formal_full_six_reentry_ready_v8.py",
    "tests/visual/test_phase2_v10i_formal_full_six_v7_preprocess_failure_receipt.py",
    "tests/visual/test_phase2_v10i_formal_full_six_v8_dispatcher_cli_correction_gate.py",
)
SOURCE_BINDINGS = tuple(name for name in v7.SOURCE_BINDINGS
                        if name not in V7_SCAFFOLDING) + V8_SOURCES


def require(value: bool, message: str) -> None:
    if not value:
        raise ReadinessError(message)


def version(value: object) -> object:
    if isinstance(value, dict):
        return {key: version(item) for key, item in value.items()}
    if isinstance(value, list):
        return [version(item) for item in value]
    if isinstance(value, str):
        return value.replace("_v7", "_v8").replace("-v7.json", "-v8.json")
    return value


def prepare(ready_root: Path = READY_ROOT,
            package_root: Path = base.PACKAGE_ROOT,
            executable: Path = DEFAULT_EXECUTABLE) -> dict[str, Path]:
    require(not ready_root.exists(), "fresh v8 readiness root required")
    require(not FUTURE_OUTPUT_ROOT.exists(), "fresh v8 output root must be absent")
    for path, digest, label in ((CORRECTION_GATE, CORRECTION_GATE_SHA256,
                                 "v8 correction gate"),
                                (V7_FAILURE, V7_FAILURE_SHA256,
                                 "frozen v7 failure")):
        require(path.is_file() and base.sha256(path) == digest, label + " SHA drift")
    require(executable.is_file() and all((ROOT / name).is_file()
                                         for name in SOURCE_BINDINGS),
            "v8 executable/source inventory incomplete")
    with tempfile.TemporaryDirectory(prefix="phase2-v8-derive-v7-") as directory:
        v7_files = v7.prepare(Path(directory) / "v7", package_root, executable)
        values = {key: version(json.loads(path.read_text()))
                  for key, path in v7_files.items()
                  if key not in ("readiness_manifest", "readiness_receipt")}
    sources = {name: base.binding(ROOT / name) for name in SOURCE_BINDINGS}
    dispatch = values["dispatch_contract"]
    dispatch["wrapper"] = sources[
        "scripts/run_phase2_v10i_formal_full_six_capture_v8.py"]
    dispatch["delegated_implementation"] = sources[
        "scripts/run_phase2_v10i_formal_full_six_capture_v7.py"]
    dispatch["future_output_canonical_path"] = str(FUTURE_OUTPUT_ROOT.resolve())
    dispatch["capture_driver_asset_option"] = "--asset-package"
    dispatch["real_parser_contract_test_required"] = True
    implementation = values["implementation_gate"]
    implementation["single_use_dispatch"] = dispatch
    implementation["dispatcher_cli_correction_gate"] = base.binding(CORRECTION_GATE)
    implementation["frozen_v7_failure"] = base.binding(V7_FAILURE)
    implementation["production_capture_binary_assets_unchanged"] = True
    files = {
        "implementation_gate": ready_root / "implementation-gate-v8.json",
        "six_scene_plan": ready_root / "six-scene-plan-not-run-v8.json",
        "machine_template": ready_root / "machine-evidence-template-not-run-v8.json",
        "authorization_template": ready_root / "one-shot-authorization-template-not-issued-v8.json",
        "dispatch_contract": ready_root / "single-use-dispatch-contract-v8.json",
    }
    ready_root.mkdir(parents=True)
    for key, value in (("implementation_gate", implementation),
                       ("six_scene_plan", values["six_scene_plan"]),
                       ("machine_template", values["machine_template"]),
                       ("authorization_template", authorization_template()),
                       ("dispatch_contract", dispatch)):
        base.write_immutable(files[key], value)
    package = validate_frozen_package(package_root.resolve(strict=True))
    readiness = {
        "schema": "phase2_v10i_formal_full_six_reentry_readiness_v8",
        "status": "READY_FOR_EXACT_FULL_SIX_ONE_SHOT_AUTHORIZATION_NO_GPU",
        "implementation_ready": True, **false_flags(),
        "approved_scenes": list(SCENE_IDS),
        "formal_registry_version": REGISTRY_VERSION,
        "expected_semantic_camera": EXPECTED_CAMERA,
        "reentry_gate": base.binding(REENTRY_GATE),
        "targeted_machine_prerequisite": base.binding(TARGETED_MACHINE),
        "targeted_ta_prerequisite": base.binding(TARGETED_TA),
        "full_six_ta_template": base.binding(TA_TEMPLATE),
        "dispatcher_cli_correction_gate": base.binding(CORRECTION_GATE),
        "frozen_v7_failure": base.binding(V7_FAILURE),
        "fresh_release_executable": base.binding(executable),
        "integrated_phase4_source_state": "PHASE4_V8_SOURCE_STABLE",
        "future_capture_output_root": str(FUTURE_OUTPUT_ROOT.resolve()),
        "self_contained_asset_package": {
            **base.binding(package_root / "formal-asset-package-v2.json"),
            "asset_root": str(package_root.resolve(strict=True)),
            "required_asset_count": package["required_asset_count"],
            "runtime_dependency_count": package["runtime_dependency_count"],
            "frozen_bytes_unchanged": True,
        },
        "source_bindings": sources,
        "generated_bindings": {key: base.binding(path)
                               for key, path in files.items()},
        "offline_safety": {
            "can_start_candidate_executable": False,
            "can_create_cocoa_context": False, "can_call_opengl": False,
            "can_create_screenshot": False, "can_issue_authorization": False,
        },
        "remaining_external_gates": [
            "separate_exact_full_six_one_shot_authorization",
            "single_use_authorization_consumption", "six_scene_machine_capture",
            "six_scene_machine_validator", "independent_six_of_six_ta_review",
            "formal_decision", "golden_decision", "m2_decision"],
    }
    readiness_path = ready_root / "readiness-manifest-v8.json"
    base.write_immutable(readiness_path, readiness)
    receipt = {
        "schema": "phase2_v10i_formal_full_six_reentry_readiness_receipt_v8",
        "status": readiness["status"], **false_flags(),
        "readiness_manifest": base.binding(readiness_path),
        "fresh_release_executable": base.binding(executable),
        "dispatcher_cli_correction_gate": base.binding(CORRECTION_GATE),
        "frozen_v7_failure": base.binding(V7_FAILURE),
        "self_contained_asset_package": base.binding(
            package_root / "formal-asset-package-v2.json"),
    }
    receipt_path = ready_root / "readiness-receipt-v8.json"
    base.write_immutable(receipt_path, receipt)
    files["readiness_manifest"] = readiness_path
    files["readiness_receipt"] = receipt_path
    return files


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--ready-root", type=Path, default=READY_ROOT)
    parser.add_argument("--package-root", type=Path, default=base.PACKAGE_ROOT)
    parser.add_argument("--executable", type=Path, default=DEFAULT_EXECUTABLE)
    args = parser.parse_args()
    try:
        print(json.dumps({key: str(path) for key, path in
                          prepare(args.ready_root, args.package_root,
                                  args.executable).items()}, indent=2, sort_keys=True))
    except (OSError, ValueError, json.JSONDecodeError, ReadinessError) as error:
        print("FAIL: " + str(error))
        return 2
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
