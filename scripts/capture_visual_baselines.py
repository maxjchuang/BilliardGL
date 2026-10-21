#!/usr/bin/env python3
"""Capture the six deterministic Phase-0 scenes through rendered automation."""

import argparse
import datetime
import hashlib
import json
import math
import os
from pathlib import Path
import shutil
import subprocess
import sys
from typing import Any, Dict, Optional, Tuple


REPOSITORY_ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(REPOSITORY_ROOT))

from tests.e2e.automation_client import AutomationClient  # noqa: E402
from tests.visual.formal_modern_scene_registry_v2 import (  # noqa: E402
    EXPECTED_CAMERA as FORMAL_EXPECTED_CAMERA,
    FORMAL_MODERN_VISUAL_SCENES,
    REGISTRY_VERSION as FORMAL_REGISTRY_VERSION,
)
from tests.visual.visual_scene_registry import (  # noqa: E402
    CAPTURE_HEIGHT,
    CAPTURE_PROFILES,
    CAPTURE_WIDTH,
    PHYSICS_PROFILE_ID,
    RANDOM_SEED,
    REGISTRY_VERSION,
    VISUAL_SCENES,
    apply_scene,
)


def _parse_arguments() -> argparse.Namespace:
    parser = argparse.ArgumentParser(
        description="Capture deterministic BilliardGL visual baselines as PPM files.")
    parser.add_argument("--executable", required=True, type=Path)
    parser.add_argument(
        "--output", type=Path, default=Path("/tmp/billiardgl-visual-baselines"),
        help="artifact directory (default: /tmp/billiardgl-visual-baselines)")
    parser.add_argument("--width", type=int, default=CAPTURE_WIDTH)
    parser.add_argument("--height", type=int, default=CAPTURE_HEIGHT)
    parser.add_argument(
        "--verify-repeatability", action="store_true",
        help="capture every scene twice and require byte-identical images and states")
    parser.add_argument("--formal-v10i", action="store_true")
    parser.add_argument(
        "--formal-targeted-scenes",
        help="exact targeted formal scope; only break,low_aim is accepted")
    parser.add_argument("--asset-package", type=Path)
    parser.add_argument("--authorization", type=Path)
    parser.add_argument("--readiness-manifest-sha256")
    parser.add_argument("--warmup-frames", type=int, default=120)
    parser.add_argument("--sample-frames", type=int, default=300)
    return parser.parse_args()


def read_ppm(path: Path) -> Tuple[int, int, bytes]:
    with path.open("rb") as image:
        if image.readline().strip() != b"P6":
            raise ValueError(str(path) + " is not a binary PPM")
        dimensions = image.readline().split()
        if len(dimensions) != 2:
            raise ValueError(str(path) + " has invalid dimensions")
        width, height = map(int, dimensions)
        if image.readline().strip() != b"255":
            raise ValueError(str(path) + " is not RGB8")
        pixels = image.read()
    if len(pixels) != width * height * 3:
        raise ValueError(str(path) + " has incomplete pixel data")
    return width, height, pixels


def _visible_pixels(pixels: bytes) -> int:
    return sum(
        1 for index in range(0, len(pixels), 3)
        if max(pixels[index:index + 3]) > 8)


def _visible_pixels_in_region(
    pixels: bytes, width: int, height: int,
    region: Tuple[str, float, float, float, float],
) -> int:
    _, normalized_x, normalized_y, normalized_width, normalized_height = region
    x0 = int(normalized_x * width)
    y0 = int(normalized_y * height)
    x1 = int((normalized_x + normalized_width) * width)
    y1 = int((normalized_y + normalized_height) * height)
    count = 0
    for y in range(y0, y1):
        row = y * width * 3
        for x in range(x0, x1):
            offset = row + x * 3
            if max(pixels[offset:offset + 3]) > 8:
                count += 1
    return count


def _sha256_bytes(value: bytes) -> str:
    return "sha256:" + hashlib.sha256(value).hexdigest()


def _sha256_file(path: Path) -> str:
    digest = hashlib.sha256()
    with path.open("rb") as source:
        for chunk in iter(lambda: source.read(1024 * 1024), b""):
            digest.update(chunk)
    return "sha256:" + digest.hexdigest()


def _revision() -> str:
    try:
        return subprocess.check_output(
            ["git", "rev-parse", "HEAD"], cwd=str(REPOSITORY_ROOT), text=True,
            stderr=subprocess.DEVNULL).strip()
    except (OSError, subprocess.CalledProcessError):
        return "unknown"


def _stable_state(state: Dict[str, Any]) -> Dict[str, Any]:
    return {
        "tick": state["tick"],
        "balls_moving": state["balls_moving"],
        "balls": state["balls"],
        "aim": state["aim"],
        "camera": state["camera"],
        "input": {"shot_power": state["input"]["shot_power"]},
    }


FORMAL_SCENE_IDS = tuple(
    scene.scene_id for scene in FORMAL_MODERN_VISUAL_SCENES)
FORMAL_TARGETED_SCENE_IDS = ("break", "low_aim")
FORMAL_ASSET_FILES = {
    "modular_gltf": "focused_practice_hall_modular_v1.gltf",
    "asset_manifest": "focused_practice_hall_modular_v1.asset.json",
    "pbr_params": "focused_practice_hall_pbr_v1.params.json",
    "lighting_rig": "focused_practice_hall_lighting_rig_v1.json",
    "venue_gltf": "focused_practice_hall_venue_v1.gltf",
    "ball_atlas": "focused_practice_hall_ball_identity_atlas_v1.png",
}
FORMAL_MANIFEST_HASH_FIELDS = {
    "modular_gltf": "modular_gltf_sha256",
    "asset_manifest": "asset_manifest_sha256",
    "pbr_params": "pbr_params_sha256",
    "lighting_rig": "lighting_sha256",
    "venue_gltf": "venue_gltf_sha256",
    "ball_atlas": "ball_atlas_sha256",
}


def validate_formal_authorization(value: Dict[str, Any],
                                  readiness_manifest_sha256: str,
                                  capture_output: Optional[Path] = None) -> None:
    schema = value.get("schema")
    if schema not in {
            "phase2_v10i_formal_six_scene_one_shot_authorization_v2",
            "phase2_v10i_formal_six_scene_one_shot_authorization_v3"}:
        raise RuntimeError("formal authorization schema mismatch; pre-process rejection")
    if value.get("status") != "AUTHORIZED_FOR_EXACTLY_SIX_FRESH_PROCESS_CAPTURES":
        raise RuntimeError("formal authorization status is not executable; pre-process rejection")
    if value.get("gpu_capture_authorized") is not True or \
            value.get("formal_six_scene_evidence_authorized") is not True:
        raise RuntimeError("formal authorization flags are false; pre-process rejection")
    if value.get("authorized_capture_count") != 6 or \
            value.get("consumed_capture_count") != 0:
        raise RuntimeError("formal authorization capture count mismatch; pre-process rejection")
    if value.get("scene_ids_in_order") != list(FORMAL_SCENE_IDS):
        raise RuntimeError("formal authorization scene order mismatch; pre-process rejection")
    if value.get("required_readiness_manifest_sha256") != readiness_manifest_sha256:
        raise RuntimeError("formal authorization readiness SHA mismatch; pre-process rejection")
    if schema == "phase2_v10i_formal_six_scene_one_shot_authorization_v3":
        for field in ("authorization_id", "authorized_by", "authorized_at_utc"):
            if not isinstance(value.get(field), str) or not value[field].strip():
                raise RuntimeError(
                    "formal authorization identity/time is incomplete; pre-process rejection")
        authorized_output = value.get("authorized_output_canonical_path")
        if not isinstance(authorized_output, str) or \
                not Path(authorized_output).is_absolute():
            raise RuntimeError(
                "formal authorization output path is incomplete; pre-process rejection")
        if capture_output is not None and \
                authorized_output != str(capture_output.resolve()):
            raise RuntimeError(
                "formal authorization output path mismatch; pre-process rejection")
        if value.get("formal_pass") is not False or \
                value.get("golden_promotion") is not False or \
                value.get("m2_promotion") is not False:
            raise RuntimeError(
                "formal authorization cannot pre-promote evidence; pre-process rejection")


def parse_formal_targeted_scenes(value: Optional[str]) -> Optional[Tuple[str, ...]]:
    if value is None:
        return None
    if value != ",".join(FORMAL_TARGETED_SCENE_IDS):
        raise RuntimeError(
            "formal targeted scenes must be exactly break,low_aim; pre-process rejection")
    return FORMAL_TARGETED_SCENE_IDS


def validate_formal_targeted_authorization(
        value: Dict[str, Any], readiness_manifest_sha256: str,
        capture_output: Path) -> None:
    if value.get("schema") != \
            "phase2_v10i_formal_targeted_camera_authorization_v1":
        raise RuntimeError(
            "formal targeted authorization schema mismatch; pre-process rejection")
    if value.get("status") != \
            "AUTHORIZED_FOR_EXACTLY_TWO_FRESH_PROCESS_CAPTURES":
        raise RuntimeError(
            "formal targeted authorization status mismatch; pre-process rejection")
    if value.get("gpu_capture_authorized") is not True or \
            value.get("targeted_camera_evidence_authorized") is not True or \
            value.get("formal_six_scene_evidence_authorized") is not False or \
            value.get("full_six_scene_capture_authorized") is not False:
        raise RuntimeError(
            "formal targeted authorization scope flags mismatch; pre-process rejection")
    if value.get("authorized_capture_count") != 2 or \
            value.get("consumed_capture_count") != 0 or \
            value.get("scene_ids_in_order") != list(FORMAL_TARGETED_SCENE_IDS):
        raise RuntimeError(
            "formal targeted authorization count/scenes mismatch; pre-process rejection")
    if value.get("required_readiness_manifest_sha256") != \
            readiness_manifest_sha256:
        raise RuntimeError(
            "formal targeted authorization readiness SHA mismatch; pre-process rejection")
    for field in ("authorization_id", "authorization_nonce",
                  "authorized_by", "authorized_at_utc"):
        if not isinstance(value.get(field), str) or not value[field].strip():
            raise RuntimeError(
                "formal targeted authorization identity/time incomplete; pre-process rejection")
    authorized_output = value.get("authorized_output_canonical_path")
    if not isinstance(authorized_output, str) or \
            not Path(authorized_output).is_absolute() or \
            authorized_output != str(capture_output.resolve()):
        raise RuntimeError(
            "formal targeted authorization output path mismatch; pre-process rejection")
    if value.get("targeted_camera_pass") is not False or \
            value.get("formal_pass") is not False or \
            value.get("golden_promotion") is not False or \
            value.get("m2_promotion") is not False:
        raise RuntimeError(
            "formal targeted authorization cannot pre-promote evidence; pre-process rejection")


def consume_formal_targeted_authorization(
        authorization_path: Path, value: Dict[str, Any],
        readiness_manifest_sha256: str, capture_output: Path) -> Dict[str, Any]:
    path = authorization_path.resolve(strict=True)
    marker = path.with_name(path.name + ".consumed.json")
    if marker.exists():
        raise RuntimeError(
            "formal targeted authorization already consumed; pre-process rejection")
    payload = {
        "schema": "phase2_v10i_formal_targeted_camera_authorization_consumed_v1",
        "status": "CONSUMED_BEFORE_PROCESS_START",
        "authorization_path": str(path),
        "authorization_sha256": _sha256_file(path).removeprefix("sha256:"),
        "authorization_id": value["authorization_id"],
        "authorization_nonce": value["authorization_nonce"],
        "authorized_output_canonical_path": str(capture_output.resolve()),
        "required_readiness_manifest_sha256": readiness_manifest_sha256,
        "scene_ids_in_order": list(FORMAL_TARGETED_SCENE_IDS),
        "authorized_capture_count": 2,
        "consumed_capture_count": 2,
        "formal_six_scene_evidence": False,
        "formal_pass": False,
        "golden_promotion": False,
        "m2_promotion": False,
    }
    try:
        descriptor = os.open(
            marker, os.O_WRONLY | os.O_CREAT | os.O_EXCL, 0o600)
        with os.fdopen(descriptor, "w", encoding="utf-8") as destination:
            destination.write(json.dumps(payload, indent=2, sort_keys=True) + "\n")
            destination.flush()
            os.fsync(destination.fileno())
    except FileExistsError as error:
        raise RuntimeError(
            "formal targeted authorization already consumed; pre-process rejection") from error
    return {
        "canonical_path": str(marker.resolve(strict=True)),
        "sha256": _sha256_file(marker).removeprefix("sha256:"),
        "authorization_sha256": payload["authorization_sha256"],
        "authorization_id": value["authorization_id"],
        "authorization_nonce": value["authorization_nonce"],
    }


def explicit_formal_child_environment(asset_root: Path,
                                      inherited: Optional[Dict[str, str]] = None) -> Dict[str, str]:
    root = asset_root.resolve(strict=True)
    if not root.is_dir():
        raise RuntimeError("formal child asset root is not a directory")
    environment = dict(os.environ if inherited is None else inherited)
    environment.pop("BILLIARDGL_MODERN_PHASE2_ROOT", None)
    environment.pop("BILLIARDGL_FORMAL_CAPTURE", None)
    environment["BILLIARDGL_MODERN_PHASE2_ROOT"] = str(root)
    environment["BILLIARDGL_FORMAL_CAPTURE"] = "1"
    return environment


def materialize_capture_asset_root(package: Path, destination: Path) -> None:
    source = package.resolve(strict=True)
    if not source.is_dir():
        raise RuntimeError("formal asset package is not a directory")
    if destination.exists():
        raise RuntimeError("capture-local asset_root must not pre-exist")
    for path in source.rglob("*"):
        if path.is_symlink():
            raise RuntimeError("formal asset package must not contain symlinks")
    shutil.copytree(source, destination, copy_function=shutil.copy2)
    for filename in FORMAL_ASSET_FILES.values():
        if not (destination / filename).is_file():
            raise RuntimeError("formal asset package missing: " + filename)


def validate_ready_asset_attestation(ready: Dict[str, Any],
                                     asset_root: Path) -> Dict[str, Any]:
    if ready.get("formal_capture") is not True:
        raise RuntimeError("rendered process did not enter formal capture mode")
    renderer_load_checks = {
        "pbr_loaded": ready.get("pbr_loaded") is True,
        "pbr_texture_count_22": ready.get("pbr_texture_count") == 22,
        "environment_enabled": ready.get("environment_enabled") is True,
        "gltf_scene_loaded": ready.get("gltf_scene_loaded") is True,
        "venue_scene_loaded": ready.get("venue_scene_loaded") is True,
        "medium_quality": ready.get("render_quality") == "medium",
        "msaa_4x": ready.get("msaa_samples") == 4,
    }
    if not all(renderer_load_checks.values()):
        raise RuntimeError("formal process renderer load/quality checks are incomplete")
    attestation = ready.get("formal_asset_attestation")
    if not isinstance(attestation, dict) or \
            attestation.get("schema") != "billiardgl_formal_asset_attestation_v1" or \
            attestation.get("load_state") != "loaded" or \
            attestation.get("silent_material_fallback_allowed") is not False or \
            attestation.get("load_errors") != []:
        raise RuntimeError("formal asset attestation/load contract is incomplete")
    expected_root = str(asset_root.resolve(strict=True))
    if attestation.get("asset_root") != expected_root:
        raise RuntimeError("formal process attested a different canonical asset root")
    assets = attestation.get("required_assets")
    if not isinstance(assets, list) or len(assets) != len(FORMAL_ASSET_FILES):
        raise RuntimeError("formal process did not attest exact six assets")
    by_id = {item.get("id"): item for item in assets if isinstance(item, dict)}
    if set(by_id) != set(FORMAL_ASSET_FILES):
        raise RuntimeError("formal process asset attestation ids differ")
    for asset_id, filename in FORMAL_ASSET_FILES.items():
        path = (asset_root / filename).resolve(strict=True)
        item = by_id[asset_id]
        if item.get("canonical_path") != str(path) or \
                item.get("sha256") != _sha256_file(path).removeprefix("sha256:"):
            raise RuntimeError("formal process asset path/SHA mismatch: " + asset_id)
    dependencies = attestation.get("runtime_dependencies")
    if not isinstance(dependencies, list) or not dependencies:
        raise RuntimeError("formal process runtime dependency attestation missing")
    for item in dependencies:
        if not isinstance(item, dict) or not isinstance(item.get("canonical_path"), str):
            raise RuntimeError("formal runtime dependency attestation invalid")
        path = Path(item["canonical_path"]).resolve(strict=True)
        if path.parent != asset_root.resolve(strict=True) or \
                item.get("sha256") != _sha256_file(path).removeprefix("sha256:"):
            raise RuntimeError("formal runtime dependency path/SHA mismatch")
    attestation = dict(attestation)
    attestation["renderer_load_checks"] = renderer_load_checks
    return attestation


def validate_formal_presentation_camera(
        scene_id: str, presentation: Dict[str, Any]) -> Dict[str, Any]:
    camera = presentation.get("camera")
    if not isinstance(camera, dict):
        raise RuntimeError(scene_id + ": semantic presentation camera missing")
    expected_state, expected_preset = FORMAL_EXPECTED_CAMERA[scene_id]
    if camera.get("state") != expected_state or \
            camera.get("preset") != expected_preset or \
            camera.get("transitioning") is not False:
        raise RuntimeError(
            scene_id + ": semantic presentation camera was not applied/settled")
    for point_name in ("eye_cm", "target_cm"):
        point = camera.get(point_name)
        if not isinstance(point, dict) or any(
                not isinstance(point.get(axis), (int, float)) or
                not math.isfinite(point[axis]) for axis in ("x", "y", "z")):
            raise RuntimeError(scene_id + ": semantic camera point is invalid")
    for field in ("distance_cm", "fov_degrees"):
        if not isinstance(camera.get(field), (int, float)) or \
                not math.isfinite(camera[field]) or camera[field] <= 0.0:
            raise RuntimeError(scene_id + ": semantic camera scalar is invalid")
    eye = camera["eye_cm"]
    target = camera["target_cm"]
    projected_distance = math.sqrt(sum(
        (eye[axis] - target[axis]) ** 2 for axis in ("x", "y", "z")))
    if not math.isclose(projected_distance, camera["distance_cm"],
                        rel_tol=1.0e-4, abs_tol=1.0e-3):
        raise RuntimeError(scene_id + ": semantic camera projection differs")
    if not 20.0 <= camera["fov_degrees"] <= 90.0:
        raise RuntimeError(scene_id + ": semantic camera projection is unsafe")
    # The overhead preset intentionally observes from above the fixture plane;
    # all perspective evidence views must remain below the authored underside.
    if scene_id != "tactical_overhead" and eye["y"] >= 221.0:
        raise RuntimeError(scene_id + ": semantic camera is not lamp-safe")
    return camera


def capture_scene(
    executable: Path, output: Path, scene: Any, width: int, height: int,
    suffix: str = "", child_env: Optional[Dict[str, str]] = None,
    formal_asset_root: Optional[Path] = None, warmup_frames: int = 120,
    sample_frames: int = 300,
) -> Dict[str, Any]:
    expected_width = width
    expected_height = height
    image_path = output / (scene.scene_id + suffix + ".ppm")
    automation_extra_args = (["--renderer", "modern"]
                             if formal_asset_root is not None else None)
    with AutomationClient(str(executable), mode="rendered",
                          extra_args=automation_extra_args,
                          child_env=child_env) as client:
        if "screenshot" not in client.ready.get("capabilities", []):
            raise RuntimeError("rendered automation did not advertise screenshot support")
        state = apply_scene(client, scene, width, height)
        attestation = None
        performance = None
        semantic_camera = None
        if formal_asset_root is not None:
            presentation = client.command("get_presentation_state")
            semantic_camera = validate_formal_presentation_camera(
                scene.scene_id, presentation)
            attestation = validate_ready_asset_attestation(
                client.ready, formal_asset_root)
            performance = client.collect_render_performance(
                warmup_frames, sample_frames)
            if performance.get("measurement_method") != \
                    "gpu_timer_query_plus_cpu_frame" or \
                    performance.get("warmup_frames", 0) < 120 or \
                    performance.get("sample_frames", 0) < 300 or \
                    performance.get("gpu_timer_queries_observed") is not True:
                raise RuntimeError("formal performance measurement contract differs")
        result = client.screenshot(str(image_path))
        if result["tick"] != state["tick"]:
            raise RuntimeError(scene.scene_id + " screenshot tick differs from configured state")

    captured_width, captured_height, pixels = read_ppm(image_path)
    if (captured_width, captured_height) != (expected_width, expected_height):
        raise RuntimeError(
            "unexpected drawable size for %s: %dx%d" %
            (scene.scene_id, captured_width, captured_height))
    visible = _visible_pixels(pixels)
    if visible == 0:
        raise RuntimeError(scene.scene_id + " produced an all-black frame")
    regions = {}
    for region in scene.interest_regions:
        region_visible = _visible_pixels_in_region(
            pixels, captured_width, captured_height, region)
        if region_visible == 0:
            raise RuntimeError(scene.scene_id + " has no visible pixels in region " + region[0])
        regions[region[0]] = {"visible_pixels": region_visible}

    stable_state = _stable_state(state)
    state_bytes = json.dumps(
        stable_state, sort_keys=True, separators=(",", ":")).encode("utf-8")
    value = {
        "scene_id": scene.scene_id,
        "title": scene.title,
        "purpose": scene.purpose,
        "file": image_path.name,
        "image_sha256": _sha256_bytes(pixels),
        "ppm_sha256": _sha256_file(image_path),
        "state_sha256": _sha256_bytes(state_bytes),
        "tick": state["tick"],
        "visible_pixels": visible,
        "interest_regions": regions,
        "camera": state["camera"],
    }
    if attestation is not None:
        value["loaded_asset_attestation"] = attestation
        value["performance"] = performance
        value["semantic_camera"] = semantic_camera
    return value


def capture_all(
    executable: Path, output: Path, width: int = CAPTURE_WIDTH,
    height: int = CAPTURE_HEIGHT, verify_repeatability: bool = False,
    formal_asset_package: Optional[Path] = None,
    authorization: Optional[Dict[str, Any]] = None,
    readiness_manifest_sha256: Optional[str] = None,
    warmup_frames: int = 120, sample_frames: int = 300,
    formal_targeted_scene_ids: Optional[Tuple[str, ...]] = None,
    formal_authorization_path: Optional[Path] = None,
) -> Dict[str, Any]:
    expected_width = width
    expected_height = height
    formal = formal_asset_package is not None
    targeted = formal_targeted_scene_ids is not None
    if targeted and tuple(formal_targeted_scene_ids) != \
            FORMAL_TARGETED_SCENE_IDS:
        raise RuntimeError(
            "formal targeted scene scope must be exact; pre-process rejection")
    if targeted and not formal:
        raise RuntimeError(
            "formal targeted capture requires formal asset package; pre-process rejection")
    if formal:
        if output.exists():
            raise RuntimeError("formal capture output must be absent before authorization")
        if authorization is None or not readiness_manifest_sha256:
            raise RuntimeError("formal authorization is required; pre-process rejection")
        if targeted:
            validate_formal_targeted_authorization(
                authorization, readiness_manifest_sha256, output)
            if formal_authorization_path is None:
                raise RuntimeError(
                    "formal targeted authorization path required; pre-process rejection")
            targeted_consumption = consume_formal_targeted_authorization(
                formal_authorization_path, authorization,
                readiness_manifest_sha256, output)
        else:
            validate_formal_authorization(
                authorization, readiness_manifest_sha256, output)
        output.mkdir(parents=True)
        formal_asset_root = output / "asset_root"
        materialize_capture_asset_root(formal_asset_package, formal_asset_root)
        child_env = explicit_formal_child_environment(formal_asset_root)
    else:
        output.mkdir(parents=True, exist_ok=True)
        formal_asset_root = None
        child_env = None
        targeted_consumption = None
    scenes = []
    if targeted:
        capture_scenes = tuple(
            scene for scene in FORMAL_MODERN_VISUAL_SCENES
            if scene.scene_id in FORMAL_TARGETED_SCENE_IDS)
    else:
        capture_scenes = FORMAL_MODERN_VISUAL_SCENES if formal else VISUAL_SCENES
    for scene in capture_scenes:
        captured = capture_scene(
            executable, output, scene, expected_width, expected_height,
            child_env=child_env, formal_asset_root=formal_asset_root,
            warmup_frames=warmup_frames, sample_frames=sample_frames)
        if verify_repeatability:
            repeated = capture_scene(
                executable, output, scene, expected_width, expected_height,
                ".repeat", child_env=child_env,
                formal_asset_root=formal_asset_root,
                warmup_frames=warmup_frames, sample_frames=sample_frames)
            for key in ("image_sha256", "ppm_sha256", "state_sha256", "tick"):
                if captured[key] != repeated[key]:
                    raise RuntimeError(
                        "%s is not repeatable (%s differs)" % (scene.scene_id, key))
            (output / repeated["file"]).unlink()
        scenes.append(captured)

    manifest = {
        "schema_version": 1,
        "registry_version": FORMAL_REGISTRY_VERSION if formal else REGISTRY_VERSION,
        "generated_at_utc": datetime.datetime.now(
            datetime.timezone.utc).isoformat().replace("+00:00", "Z"),
        "revision": _revision(),
        "executable": str(executable.resolve()),
        "executable_sha256": _sha256_file(executable),
        "resolution": {"width": expected_width, "height": expected_height},
        "pixel_format": "PPM P6 RGB8",
        "color_space": "legacy framebuffer (unmanaged; Phase 0 baseline)",
        "physics_profile_id": PHYSICS_PROFILE_ID,
        "random_seed": RANDOM_SEED,
        "repeatability_verified": verify_repeatability,
        "scenes": scenes,
    }
    if formal and formal_asset_root is not None:
        performances = [scene["performance"] for scene in scenes]
        methods = {item["measurement_method"] for item in performances}
        if len(methods) != 1:
            raise RuntimeError("formal per-scene measurement methods differ")
        manifest.update({
            "schema": ("phase2_v10i_formal_targeted_camera_capture_v1"
                       if targeted else
                       "phase2_v10i_formal_six_scene_capture_v2"),
            "capture_scope": {
                "kind": ("targeted_camera_break_low_aim"
                         if targeted else "formal_six_scene"),
                "scene_ids_in_order": [
                    scene["scene_id"] for scene in scenes],
                "capture_count": len(scenes),
                "full_six_scene_evidence": not targeted,
            },
            "renderer": "modern",
            "production_gltf": True,
            "quality": "medium",
            "asset_root": "asset_root",
            "formal_asset_attestation_required": True,
            "performance": {
                "warmup_frames": min(item["warmup_frames"] for item in performances),
                "sample_frames": min(item["sample_frames"] for item in performances),
                "p95_total_frame_ms": max(item["p95_total_frame_ms"] for item in performances),
                "draw_calls_complete_scene": max(item["draw_calls_complete_scene"] for item in performances),
                "texture_resident_mib": max(item["texture_resident_mib"] for item in performances),
                "measurement_method": methods.pop(),
                "gpu_timer_queries_observed": all(
                    item.get("gpu_timer_queries_observed") is True
                    for item in performances),
                "aggregation": "worst_scene_max_with_minimum_sample_contract",
            },
        })
        if targeted:
            manifest["targeted_authorization_consumption"] = \
                targeted_consumption
        for asset_id, filename in FORMAL_ASSET_FILES.items():
            field = FORMAL_MANIFEST_HASH_FIELDS[asset_id]
            manifest[field] = _sha256_file(
                formal_asset_root / filename).removeprefix("sha256:")
    manifest_path = output / "manifest.json"
    manifest_path.write_text(
        json.dumps(manifest, indent=2, sort_keys=True) + "\n", encoding="utf-8")
    return manifest


def main() -> int:
    arguments = _parse_arguments()
    try:
        targeted_scene_ids = parse_formal_targeted_scenes(
            arguments.formal_targeted_scenes)
    except RuntimeError as error:
        print("error: " + str(error), file=sys.stderr)
        return 2
    if targeted_scene_ids is not None and not arguments.formal_v10i:
        print("error: formal targeted scenes require --formal-v10i; pre-process rejection",
              file=sys.stderr)
        return 2
    if (arguments.width, arguments.height) not in CAPTURE_PROFILES:
        print(
            "error: supported capture profiles are " +
            ", ".join("%dx%d" % profile for profile in CAPTURE_PROFILES),
            file=sys.stderr)
        return 2
    if not arguments.executable.is_file():
        print("error: executable does not exist: " + str(arguments.executable), file=sys.stderr)
        return 2
    authorization = None
    if arguments.formal_v10i:
        if arguments.authorization is None or arguments.asset_package is None or \
                not arguments.readiness_manifest_sha256:
            print("error: formal-v10i requires --authorization, --asset-package, and --readiness-manifest-sha256", file=sys.stderr)
            return 2
        try:
            authorization = json.loads(
                arguments.authorization.read_text(encoding="utf-8"))
        except (OSError, json.JSONDecodeError) as error:
            print("error: formal authorization read failed: " + str(error), file=sys.stderr)
            return 2
    try:
        manifest = capture_all(
            arguments.executable, arguments.output,
            arguments.width, arguments.height, arguments.verify_repeatability,
            arguments.asset_package if arguments.formal_v10i else None,
            authorization, arguments.readiness_manifest_sha256,
            arguments.warmup_frames, arguments.sample_frames,
            targeted_scene_ids,
            arguments.authorization if targeted_scene_ids is not None else None)
    except (OSError, RuntimeError, ValueError) as error:
        print("error: " + str(error), file=sys.stderr)
        return 2
    print(
        "captured %d visual scenes in %s" %
        (len(manifest["scenes"]), arguments.output.resolve()))
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
