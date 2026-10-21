#!/usr/bin/env python3
"""Build the GPU-free v10i formal six-scene implementation-ready package v2."""

from __future__ import annotations

import argparse
import hashlib
import json
from pathlib import Path
import shutil
from typing import Any


ROOT = Path(__file__).resolve().parents[1]
V1_ROOT = ROOT / "docs/art/phase2-v10i-formal-six-scene-ready-v1"
V2_ROOT = ROOT / "docs/art/phase2-v10i-formal-six-scene-ready-v2"
PACKAGE_ROOT = ROOT / "assets/modern/phase2-v10i-formal"
DEFAULT_EXECUTABLE = ROOT / "build-phase2-v10i-formal-ready-v2/Billiards"
BLOCKED_MANIFEST = V1_ROOT / "readiness-manifest-blocked.json"
BLOCKED_MANIFEST_SHA256 = "7cdfc1975b42c3e17a17d4f4aa940d08888faeb5b94e6e8702302c4b01632dcf"
SCENE_IDS = (
    "break", "low_aim", "ball_closeup", "pocket_closeup",
    "tactical_overhead", "post_shot_observe",
)
GAP_IDS = (
    "CPP_READY_ASSET_ATTESTATION", "CPP_LOAD_ERROR_CONTRACT",
    "PYTHON_EXPLICIT_CHILD_ENV", "SELF_CONTAINED_FORMAL_ASSET_PACKAGE",
    "FORMAL_PERFORMANCE_COLLECTION", "FORMAL_MANIFEST_CONTRACT",
)
REQUIRED_ASSETS = {
    "modular_gltf": "focused_practice_hall_modular_v1.gltf",
    "asset_manifest": "focused_practice_hall_modular_v1.asset.json",
    "pbr_params": "focused_practice_hall_pbr_v1.params.json",
    "lighting_rig": "focused_practice_hall_lighting_rig_v1.json",
    "venue_gltf": "focused_practice_hall_venue_v1.gltf",
    "ball_atlas": "focused_practice_hall_ball_identity_atlas_v1.png",
}
V10I_FROZEN = {
    "focused_practice_hall_modular_v1.gltf":
        "c25cfedf969deb5bc329b58aad089015f6f2bc841315d39d9e793c8a5786ba88",
    "focused_practice_hall_modular_v1.asset.json":
        "3cc895416d6bec6483379bd7ab921d921863a6da2372e25624c66b03c8bda48d",
}
SOURCE_BINDINGS = (
    "CMakeLists.txt",
    "src/Billiards/billiards.cpp",
    "src/Billiards/render/modern/formal_asset_attestation.h",
    "src/Billiards/render/modern/formal_asset_attestation.cpp",
    "src/Billiards/render/modern/modern_application_mac.h",
    "src/Billiards/render/modern/modern_application_mac.mm",
    "src/Billiards/automation_controller.h",
    "src/Billiards/automation_controller.cpp",
    "src/Billiards/automation_runner.h",
    "src/Billiards/automation_runner.cpp",
    "tests/e2e/automation_client.py",
    "scripts/capture_visual_baselines.py",
    "scripts/validate_phase2_production_visuals.py",
    "scripts/prepare_phase2_v10i_formal_six_scene_ready_v2.py",
    "scripts/validate_phase2_v10i_formal_six_scene_ready_v2.py",
    "tests/modern_render_foundation_tests.cpp",
    "tests/automation_controller_tests.cpp",
    "tests/visual/test_phase2_v10i_formal_six_scene_ready_v2.py",
    "tests/visual/visual_scene_registry.py",
)


class ReadinessV2Error(RuntimeError):
    pass


def sha256(path: Path) -> str:
    digest = hashlib.sha256()
    with path.open("rb") as source:
        for chunk in iter(lambda: source.read(1024 * 1024), b""):
            digest.update(chunk)
    return digest.hexdigest()


def canonical(value: Any) -> bytes:
    return (json.dumps(value, indent=2, sort_keys=True) + "\n").encode("utf-8")


def write_immutable(path: Path, value: Any) -> None:
    payload = canonical(value)
    path.parent.mkdir(parents=True, exist_ok=True)
    if path.exists() and path.read_bytes() != payload:
        raise ReadinessV2Error("immutable readiness v2 artifact differs: " + str(path))
    path.write_bytes(payload)


def copy_immutable(source: Path, destination: Path) -> None:
    if not source.is_file():
        raise ReadinessV2Error("formal package source missing: " + str(source))
    destination.parent.mkdir(parents=True, exist_ok=True)
    if destination.exists() and destination.read_bytes() != source.read_bytes():
        raise ReadinessV2Error("immutable formal package file differs: " + str(destination))
    if not destination.exists():
        shutil.copyfile(source, destination)


def _local_image_names(path: Path) -> set[str]:
    value = json.loads(path.read_text(encoding="utf-8"))
    return {item["uri"] for item in value.get("images", [])
            if isinstance(item, dict) and isinstance(item.get("uri"), str)
            and not item["uri"].startswith("data:")}


def package_sources() -> dict[str, Path]:
    phase2 = ROOT / "assets/modern/phase2"
    v10i = ROOT / "assets/modern/phase2-v10i"
    sources = {
        REQUIRED_ASSETS["modular_gltf"]: v10i / REQUIRED_ASSETS["modular_gltf"],
        REQUIRED_ASSETS["asset_manifest"]: v10i / REQUIRED_ASSETS["asset_manifest"],
        REQUIRED_ASSETS["pbr_params"]: phase2 / REQUIRED_ASSETS["pbr_params"],
        REQUIRED_ASSETS["lighting_rig"]: phase2 / REQUIRED_ASSETS["lighting_rig"],
        REQUIRED_ASSETS["venue_gltf"]: phase2 / REQUIRED_ASSETS["venue_gltf"],
        REQUIRED_ASSETS["ball_atlas"]: phase2 / REQUIRED_ASSETS["ball_atlas"],
    }
    for gltf in (sources[REQUIRED_ASSETS["modular_gltf"]],
                 sources[REQUIRED_ASSETS["venue_gltf"]]):
        for filename in _local_image_names(gltf):
            sources[filename] = phase2 / filename
    return sources


def materialize_formal_asset_package(
        package_root: Path = PACKAGE_ROOT) -> dict[str, Any]:
    if sha256(BLOCKED_MANIFEST) != BLOCKED_MANIFEST_SHA256:
        raise ReadinessV2Error("blocked manifest SHA differs")
    sources = package_sources()
    for filename, source in sources.items():
        copy_immutable(source, package_root / filename)
    for filename, expected in V10I_FROZEN.items():
        if sha256(package_root / filename) != expected:
            raise ReadinessV2Error("frozen v10i bytes changed: " + filename)
    files = []
    roles = {filename: role for role, filename in REQUIRED_ASSETS.items()}
    for filename in sorted(sources):
        source = sources[filename]
        destination = package_root / filename
        if sha256(source) != sha256(destination):
            raise ReadinessV2Error("formal package copy SHA mismatch: " + filename)
        files.append({
            "filename": filename,
            "role": roles.get(filename, "runtime_texture_dependency"),
            "source_path": source.relative_to(ROOT).as_posix(),
            "source_sha256": sha256(source),
            "packaged_sha256": sha256(destination),
        })
    manifest = {
        "schema": "phase2_v10i_self_contained_formal_asset_package_v2",
        "blocked_manifest_sha256": BLOCKED_MANIFEST_SHA256,
        "immutable": True,
        "self_contained": True,
        "symlinks_allowed": False,
        "required_asset_ids": list(REQUIRED_ASSETS),
        "required_asset_count": 6,
        "runtime_dependency_count": sum(
            item["role"] == "runtime_texture_dependency" for item in files),
        "files": files,
    }
    write_immutable(package_root / "formal-asset-package-v2.json", manifest)
    return manifest


def binding(path: Path) -> dict[str, Any]:
    return {
        "canonical_path": str(path.resolve(strict=True)),
        "sha256": sha256(path),
    }


def authorization_template(readiness_sha: str = "POPULATE_FROM_FINAL_READINESS_MANIFEST") -> dict[str, Any]:
    return {
        "schema": "phase2_v10i_formal_six_scene_one_shot_authorization_v2",
        "status": "NOT_ISSUED",
        "authorization_id": None,
        "authorized_by": None,
        "gpu_capture_authorized": False,
        "formal_six_scene_evidence_authorized": False,
        "authorized_capture_count": 0,
        "consumed_capture_count": 0,
        "scene_ids_in_order": list(SCENE_IDS),
        "required_readiness_manifest_sha256": readiness_sha,
        "formal_pass": False,
        "golden_promotion": False,
        "m2_promotion": False,
    }


def validate_authorization_before_process(value: dict[str, Any],
                                          readiness_sha: str,
                                          capture_output: Path) -> None:
    if capture_output.exists():
        raise ReadinessV2Error("capture output must be absent before authorization")
    if value.get("schema") != "phase2_v10i_formal_six_scene_one_shot_authorization_v2":
        raise ReadinessV2Error("authorization schema mismatch; pre-process rejection")
    if value.get("status") != "AUTHORIZED_FOR_EXACTLY_SIX_FRESH_PROCESS_CAPTURES":
        raise ReadinessV2Error("authorization status is not executable; pre-process rejection")
    if value.get("required_readiness_manifest_sha256") != readiness_sha:
        raise ReadinessV2Error("authorization readiness SHA mismatch; pre-process rejection")
    if value.get("gpu_capture_authorized") is not True or \
            value.get("formal_six_scene_evidence_authorized") is not True:
        raise ReadinessV2Error("authorization flags are false; pre-process rejection")
    if value.get("authorized_capture_count") != 6 or \
            value.get("consumed_capture_count") != 0 or \
            value.get("scene_ids_in_order") != list(SCENE_IDS):
        raise ReadinessV2Error("authorization count/order mismatch; pre-process rejection")


def prepare(ready_root: Path = V2_ROOT, package_root: Path = PACKAGE_ROOT,
            executable: Path = DEFAULT_EXECUTABLE) -> dict[str, Path]:
    package_manifest = materialize_formal_asset_package(package_root)
    if not executable.is_file():
        raise ReadinessV2Error("fresh Release executable missing: " + str(executable))
    missing_sources = [path for path in SOURCE_BINDINGS if not (ROOT / path).is_file()]
    if missing_sources:
        raise ReadinessV2Error("source binding missing: " + ",".join(missing_sources))
    source_bindings = {path: binding(ROOT / path) for path in SOURCE_BINDINGS}
    package_binding = binding(package_root / "formal-asset-package-v2.json")
    executable_binding = binding(executable)
    false_flags = {
        "actual_gpu_used": False, "gpu_capture_authorized": False,
        "capture_output_created": False, "ta_review_performed": False,
        "product_review_performed": False, "formal_six_scene_evidence": False,
        "formal_pass": False, "golden_promotion": False,
        "m2_promotion": False,
    }
    gate_evidence = {
        "CPP_READY_ASSET_ATTESTATION": [
            "src/Billiards/render/modern/formal_asset_attestation.cpp",
            "src/Billiards/render/modern/modern_application_mac.mm"],
        "CPP_LOAD_ERROR_CONTRACT": [
            "src/Billiards/render/modern/formal_asset_attestation.cpp",
            "tests/modern_render_foundation_tests.cpp"],
        "PYTHON_EXPLICIT_CHILD_ENV": [
            "tests/e2e/automation_client.py", "scripts/capture_visual_baselines.py"],
        "SELF_CONTAINED_FORMAL_ASSET_PACKAGE": [
            "assets/modern/phase2-v10i-formal/formal-asset-package-v2.json"],
        "FORMAL_PERFORMANCE_COLLECTION": [
            "src/Billiards/automation_controller.cpp",
            "src/Billiards/render/modern/modern_application_mac.mm",
            "scripts/capture_visual_baselines.py"],
        "FORMAL_MANIFEST_CONTRACT": [
            "scripts/capture_visual_baselines.py",
            "scripts/validate_phase2_production_visuals.py"],
    }
    gate = {
        "schema": "phase2_v10i_formal_six_scene_implementation_gate_v2",
        "status": "PASS_IMPLEMENTATION_READY_NO_GPU",
        **false_flags,
        "blocked_manifest_sha256": BLOCKED_MANIFEST_SHA256,
        "gaps": [{
            "id": gap_id, "status": "CLOSED_BY_SOURCE_AND_CPU_TEST",
            "evidence": [binding(ROOT / path) if not path.startswith("assets/")
                         else binding(ROOT / path)
                         for path in gate_evidence[gap_id]],
        } for gap_id in GAP_IDS],
    }
    plan = {
        "schema": "phase2_v10i_formal_six_scene_plan_v2",
        "status": "NOT_RUN_READY_FOR_ONE_SHOT_AUTHORIZATION",
        **false_flags,
        "capture_contract": {
            "fresh_process_per_scene": True, "renderer": "modern",
            "resolution": {"width": 1920, "height": 1080},
            "quality": "medium", "msaa_samples": 4,
            "warmup_frames": 120, "sample_frames": 300,
            "measurement_method": "gpu_timer_query_plus_cpu_frame",
            "gpu_timer_queries_observed_required": True,
            "explicit_child_env": {
                "BILLIARDGL_MODERN_PHASE2_ROOT": "<capture_output>/asset_root",
                "BILLIARDGL_FORMAL_CAPTURE": "1",
            },
        },
        "rows": [{
            "ordinal": index + 1, "scene_id": scene_id, "status": "NOT_RUN",
            "actual_gpu": False, "process_started": False,
            "screenshot_created": False, "loaded_v10i_sha_attested": False,
            "performance_measured": False, "machine_evidence": False,
            "ta_review": False, "product_review": False,
            "formal_evidence": False, "golden": False, "m2": False,
        } for index, scene_id in enumerate(SCENE_IDS)],
    }
    machine = {
        "schema": "phase2_v10i_formal_six_scene_machine_evidence_template_v2",
        "status": "NOT_RUN_READY", **false_flags,
        "formal_manifest_contract": {
            "schema": "phase2_v10i_formal_six_scene_capture_v2",
            "validator": "scripts/validate_phase2_production_visuals.py",
            "asset_root": "asset_root", "production_gltf": True,
            "renderer": "modern", "quality": "medium",
            "resolution": {"width": 1920, "height": 1080},
            "required_per_scene_attestation": True,
            "required_asset_ids": list(REQUIRED_ASSETS),
            "warmup_frames_min": 120, "sample_frames_min": 300,
            "p95_total_frame_ms_max": 16.7,
            "draw_calls_complete_scene_max": 100,
            "texture_resident_mib_max": 256,
            "measurement_method": "gpu_timer_query_plus_cpu_frame",
        },
        "scenes": [{
            "scene_id": scene_id, "status": "NOT_RUN",
            "screenshot_path": None, "screenshot_sha256": None,
            "loaded_asset_attestation": None, "performance": None,
            "machine_pass": False,
        } for scene_id in SCENE_IDS],
    }
    authorization = authorization_template()
    files = {
        "implementation_gate": ready_root / "implementation-gate-v2.json",
        "six_scene_plan": ready_root / "six-scene-plan-not-run-v2.json",
        "machine_template": ready_root / "machine-evidence-template-not-run-v2.json",
        "authorization_template": ready_root / "one-shot-authorization-template-not-issued-v2.json",
    }
    for key, value in (("implementation_gate", gate), ("six_scene_plan", plan),
                       ("machine_template", machine),
                       ("authorization_template", authorization)):
        write_immutable(files[key], value)
    generated = {key: binding(path) for key, path in files.items()}
    readiness = {
        "schema": "phase2_v10i_formal_six_scene_readiness_manifest_v2",
        "status": "READY_FOR_ONE_SHOT_AUTHORIZATION_NO_GPU",
        "implementation_ready": True,
        **false_flags,
        "blocked_v1_lineage": binding(BLOCKED_MANIFEST),
        "blocked_manifest_sha256": BLOCKED_MANIFEST_SHA256,
        "closed_gap_ids": list(GAP_IDS),
        "approved_scenes": list(SCENE_IDS),
        "self_contained_asset_package": {
            **package_binding,
            "asset_root": str(package_root.resolve(strict=True)),
            "required_asset_count": package_manifest["required_asset_count"],
            "runtime_dependency_count": package_manifest["runtime_dependency_count"],
            "frozen_v10i_bytes_unchanged": True,
        },
        "fresh_release_executable": executable_binding,
        "source_bindings": source_bindings,
        "generated_bindings": generated,
        "offline_safety": {
            "can_start_candidate_executable": False,
            "can_create_cocoa_context": False,
            "can_call_opengl": False,
            "can_create_screenshot": False,
            "imports_subprocess": False,
        },
        "remaining_external_gates": [
            "one_shot_gpu_authorization", "six_scene_machine_capture",
            "technical_art_review", "product_review", "golden_decision",
            "m2_promotion_decision"],
    }
    readiness_path = ready_root / "readiness-manifest-v2.json"
    write_immutable(readiness_path, readiness)
    receipt = {
        "schema": "phase2_v10i_formal_six_scene_readiness_receipt_v2",
        "status": readiness["status"], **false_flags,
        "readiness_manifest": binding(readiness_path),
        "fresh_release_executable": executable_binding,
        "self_contained_asset_package": package_binding,
    }
    receipt_path = ready_root / "readiness-receipt-v2.json"
    write_immutable(receipt_path, receipt)
    files["readiness_manifest"] = readiness_path
    files["readiness_receipt"] = receipt_path
    return files


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--package-only", action="store_true")
    parser.add_argument("--package-root", type=Path, default=PACKAGE_ROOT)
    parser.add_argument("--ready-root", type=Path, default=V2_ROOT)
    parser.add_argument("--executable", type=Path, default=DEFAULT_EXECUTABLE)
    args = parser.parse_args()
    try:
        if args.package_only:
            manifest = materialize_formal_asset_package(args.package_root)
            print("formal asset package ready: files=%d" % len(manifest["files"]))
        else:
            files = prepare(args.ready_root, args.package_root, args.executable)
            for path in files.values():
                print(path)
    except (OSError, ValueError, json.JSONDecodeError, ReadinessV2Error) as error:
        print("FAIL: " + str(error))
        return 2
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
