#!/usr/bin/env python3
"""Prepare fresh GPU-free Phase2 exact full-six reentry readiness v7."""

from __future__ import annotations

import argparse
import json
from pathlib import Path
import sys
import tempfile

_ROOT = Path(__file__).resolve().parents[1]
if str(_ROOT) not in sys.path:
    sys.path.insert(0, str(_ROOT))

from scripts import prepare_phase2_v10i_formal_full_six_reentry_ready_v6 as v6
from scripts import prepare_phase2_v10i_formal_six_scene_ready_v2 as base

ROOT = v6.ROOT
READY_ROOT = ROOT / "docs/art/phase2-v10i-formal-full-six-reentry-ready-v7"
DEFAULT_EXECUTABLE = ROOT / "build-integrated-phase2-full-six-reentry-v7-phase4-v8/Billiards"
FUTURE_OUTPUT_ROOT = ROOT / "docs/evidence/phase2-v10i-formal-full-six-capture-v7"
REENTRY_GATE = v6.REENTRY_GATE
REENTRY_GATE_SHA256 = v6.REENTRY_GATE_SHA256
TARGETED_MACHINE = v6.TARGETED_MACHINE
TARGETED_MACHINE_SHA256 = v6.TARGETED_MACHINE_SHA256
TARGETED_TA = v6.TARGETED_TA
TARGETED_TA_SHA256 = v6.TARGETED_TA_SHA256
TA_TEMPLATE = v6.TA_TEMPLATE
TA_TEMPLATE_SHA256 = v6.TA_TEMPLATE_SHA256
PACKAGE_MANIFEST_SHA256 = v6.PACKAGE_MANIFEST_SHA256
SCENE_IDS = v6.SCENE_IDS
EXPECTED_CAMERA = v6.EXPECTED_CAMERA
FORMAL_MODERN_VISUAL_SCENES = v6.FORMAL_MODERN_VISUAL_SCENES
REGISTRY_CAMERA = v6.REGISTRY_CAMERA
REGISTRY_VERSION = v6.REGISTRY_VERSION
ReadinessError = v6.ReadinessError
false_flags = v6.false_flags
recipe = v6.recipe
region = v6.region
capture_contract = v6.capture_contract
authorization_template = v6.authorization_template
validate_frozen_package = v6.validate_frozen_package

V6_ONLY = {
    "scripts/prepare_phase2_v10i_formal_full_six_reentry_ready_v6.py",
    "scripts/validate_phase2_v10i_formal_full_six_reentry_ready_v6.py",
    "tests/visual/test_phase2_v10i_formal_full_six_wrapper_v6.py",
    "tests/visual/test_phase2_v10i_formal_full_six_reentry_ready_v6.py",
}
V7_SOURCES = (
    "scripts/run_phase2_v10i_formal_full_six_capture_v7.py",
    "scripts/prepare_phase2_v10i_formal_full_six_reentry_ready_v7.py",
    "scripts/validate_phase2_v10i_formal_full_six_reentry_ready_v7.py",
    "tests/visual/test_phase2_v10i_formal_full_six_wrapper_v7.py",
    "tests/visual/test_phase2_v10i_formal_full_six_reentry_ready_v7.py",
)
SOURCE_BINDINGS = tuple(name for name in v6.SOURCE_BINDINGS
                        if name not in V6_ONLY) + V7_SOURCES


def require(condition: bool, message: str) -> None:
    if not condition:
        raise ReadinessError(message)


def version(value: object) -> object:
    if isinstance(value, dict):
        return {key: version(item) for key, item in value.items()}
    if isinstance(value, list):
        return [version(item) for item in value]
    if isinstance(value, str):
        return value.replace("_v6", "_v7").replace("-v6.json", "-v7.json")
    return value


def prepare(ready_root: Path = READY_ROOT,
            package_root: Path = base.PACKAGE_ROOT,
            executable: Path = DEFAULT_EXECUTABLE) -> dict[str, Path]:
    require(not ready_root.exists(), "fresh v7 readiness root required")
    require(not FUTURE_OUTPUT_ROOT.exists(), "fresh v7 future output root must be absent")
    require(executable.is_file(), "fresh integrated Release executable missing")
    require(all((ROOT / item).is_file() for item in SOURCE_BINDINGS),
            "v7 source binding inventory incomplete")
    with tempfile.TemporaryDirectory(prefix="phase2-v7-derive-v6-") as directory:
        interim = Path(directory) / "v6"
        v6_files = v6.prepare(interim, package_root, executable)
        values = {key: version(json.loads(path.read_text(encoding="utf-8")))
                  for key, path in v6_files.items()
                  if key not in ("readiness_manifest", "readiness_receipt")}
    sources = {name: base.binding(ROOT / name) for name in SOURCE_BINDINGS}
    dispatch = values["dispatch_contract"]
    dispatch["wrapper"] = sources[
        "scripts/run_phase2_v10i_formal_full_six_capture_v7.py"]
    dispatch["delegated_implementation"] = sources[
        "scripts/run_phase2_v10i_formal_full_six_capture_v6.py"]
    dispatch["future_output_canonical_path"] = str(FUTURE_OUTPUT_ROOT.resolve())
    implementation = values["implementation_gate"]
    implementation["single_use_dispatch"] = dispatch
    implementation["integrated_phase4_source_state"] = "PHASE4_V8_SOURCE_STABLE"
    files = {
        "implementation_gate": ready_root / "implementation-gate-v7.json",
        "six_scene_plan": ready_root / "six-scene-plan-not-run-v7.json",
        "machine_template": ready_root / "machine-evidence-template-not-run-v7.json",
        "authorization_template": ready_root / "one-shot-authorization-template-not-issued-v7.json",
        "dispatch_contract": ready_root / "single-use-dispatch-contract-v7.json",
    }
    ready_root.mkdir(parents=True)
    for key, value in (("implementation_gate", implementation),
                       ("six_scene_plan", values["six_scene_plan"]),
                       ("machine_template", values["machine_template"]),
                       ("authorization_template", authorization_template()),
                       ("dispatch_contract", dispatch)):
        base.write_immutable(files[key], value)
    package = validate_frozen_package(package_root.resolve(strict=True))
    gate_value = json.loads(REENTRY_GATE.read_text(encoding="utf-8"))
    readiness = {
        "schema": "phase2_v10i_formal_full_six_reentry_readiness_v7",
        "status": "READY_FOR_EXACT_FULL_SIX_ONE_SHOT_AUTHORIZATION_NO_GPU",
        "implementation_ready": True, **false_flags(),
        "approved_scenes": list(SCENE_IDS),
        "formal_registry_version": REGISTRY_VERSION,
        "expected_semantic_camera": EXPECTED_CAMERA,
        "reentry_gate": base.binding(REENTRY_GATE),
        "targeted_machine_prerequisite": base.binding(TARGETED_MACHINE),
        "targeted_ta_prerequisite": base.binding(TARGETED_TA),
        "full_six_ta_template": base.binding(TA_TEMPLATE),
        "fresh_release_executable": base.binding(executable),
        "frozen_gate_integrated_release": gate_value["frozen_bindings"][7],
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
    readiness_path = ready_root / "readiness-manifest-v7.json"
    base.write_immutable(readiness_path, readiness)
    receipt = {
        "schema": "phase2_v10i_formal_full_six_reentry_readiness_receipt_v7",
        "status": readiness["status"], **false_flags(),
        "readiness_manifest": base.binding(readiness_path),
        "fresh_release_executable": base.binding(executable),
        "self_contained_asset_package": base.binding(
            package_root / "formal-asset-package-v2.json"),
    }
    receipt_path = ready_root / "readiness-receipt-v7.json"
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
