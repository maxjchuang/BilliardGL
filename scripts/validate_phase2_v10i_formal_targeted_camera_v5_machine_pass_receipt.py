#!/usr/bin/env python3
"""Read-only validator for the frozen Phase2 v5 targeted machine pass."""

from __future__ import annotations

import argparse
import hashlib
import json
from pathlib import Path
import stat
import sys
from typing import Any


ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT))

from scripts import validate_phase2_v10i_formal_targeted_camera_capture as targeted


DEFAULT_RECEIPT = ROOT / (
    "docs/evidence/phase2-v10i-formal-targeted-camera-v5-machine-pass-"
    "receipt-v1.json")
EXPECTED_RECEIPT_SHA256 = (
    "f26c82883704b01165dbb325509aa1cc6bf72e4b655a9229860751c036dc7db9")
EXPECTED_READINESS_SHA256 = (
    "687fce04e0521ab26465b77ba35434353d6b210bfa703f3b9564633c9a28e01f")
EXPECTED_MANIFEST_SHA256 = (
    "807e2371c5e3bfd245cf67dc4209f4ddd4e4fab0c21c273218cf131e49aa85d8")
EXPECTED_CONSUMED_SHA256 = (
    "3302a1028845ee2d1d072c4bdf871d81c590ffdae3b704b1063aaa9ef892f4ed")
EXPECTED_AUTHORIZATION_SHA256 = (
    "449b7376e8bbdb36205baa7b58c0ddbc3c70b2e3663b2859120d61cb5f140dcf")
EXPECTED_EXECUTABLE_SHA256 = (
    "854e86b9a7f995f7abd2417cbfc8b00610ed2e5f97525163d17d165e9ae427e9")
EXPECTED_OUTPUT_INVENTORY_SHA256 = (
    "f11a3cc382f8ff2470d48a43664278787bd46138dc0bd0d3e7bd539d0a2b53e8")
EXPECTED_SCENES = (
    ("break", "93fba7595b291b64500091f0e0a3c69d644caf827499df0c88ed8cf2127afc60",
     2065987, "table", 1121348, "observe_free", "elevated"),
    ("low_aim", "c8beb0ab0a4724c1ef79a7aae52f4fd7621a478138aef702c1248c7e6fc8bade",
     1996633, "cue_and_ball", 555841, "aim_player", "player_low"),
)


class MachinePassReceiptError(RuntimeError):
    pass


def require(value: bool, message: str) -> None:
    if not value:
        raise MachinePassReceiptError(message)


def sha256(path: Path) -> str:
    digest = hashlib.sha256()
    with path.open("rb") as source:
        for chunk in iter(lambda: source.read(1024 * 1024), b""):
            digest.update(chunk)
    return digest.hexdigest()


def load(path: Path) -> dict[str, Any]:
    value = json.loads(path.read_text(encoding="utf-8"))
    require(isinstance(value, dict), f"{path}: JSON object required")
    return value


def bound_file(relative: Any, digest: Any, label: str) -> Path:
    require(isinstance(relative, str) and isinstance(digest, str),
            label + ": binding missing")
    path = (ROOT / relative).resolve(strict=True)
    require(path.is_file() and not path.is_symlink(), label + ": regular file required")
    require(sha256(path) == digest, label + ": SHA mismatch")
    return path


def permission(path: Path) -> dict[str, Any]:
    info = path.stat()
    return {"gid": info.st_gid, "mode": f"{stat.S_IMODE(info.st_mode):04o}",
            "uid": info.st_uid}


def output_inventory(root: Path) -> dict[str, Any]:
    digest = hashlib.sha256()
    count = 0
    total = 0
    files = sorted(path for path in root.rglob("*") if path.is_file())
    require(all(not path.is_symlink() for path in files),
            "output inventory may not contain symlinks")
    for path in files:
        relative = path.relative_to(root).as_posix()
        file_sha = sha256(path)
        mode = f"{stat.S_IMODE(path.stat().st_mode):04o}"
        size = path.stat().st_size
        digest.update(relative.encode("utf-8"))
        digest.update(b"\0")
        digest.update(file_sha.encode("ascii"))
        digest.update(b"\0")
        digest.update(mode.encode("ascii"))
        digest.update(b"\0")
        digest.update(str(size).encode("ascii"))
        digest.update(b"\n")
        count += 1
        total += size
    return {"count": count, "sha256": digest.hexdigest(), "total_bytes": total,
            "files": files}


def validate_structure(value: dict[str, Any]) -> None:
    require(value.get("schema") ==
            "phase2_v10i_formal_targeted_camera_v5_machine_pass_receipt_v1",
            "receipt schema")
    require(value.get("status") == "TARGETED_MACHINE_PASS", "receipt status")
    require(value.get("counts") == {
        "actual_targeted_gpu_captures": 2,
        "failed_targeted_machine_checks": 0,
        "full_six_scene_captures": 0,
        "machine_passed_targeted_scenes": 2,
        "planned_targeted_scenes": 2,
        "technical_art_reviews_completed": 0,
    }, "targeted counts")
    require(value.get("evidence_boundary") == {
        "formal_pass": False,
        "full_six_scene_authorized": False,
        "full_six_scene_evidence": False,
        "golden_promotion": False,
        "m2_promotion": False,
        "product_review_performed": False,
        "technical_art_review_performed": False,
    }, "targeted-only promotion boundary")
    frozen = value.get("frozen_bindings", {})
    require(frozen.get("readiness_manifest_sha256") == EXPECTED_READINESS_SHA256 and
            frozen.get("manifest_sha256") == EXPECTED_MANIFEST_SHA256 and
            frozen.get("billiards_executable_sha256") == EXPECTED_EXECUTABLE_SHA256 and
            frozen.get("output_inventory_sha256") == EXPECTED_OUTPUT_INVENTORY_SHA256 and
            frozen.get("output_file_count") == 28 and
            frozen.get("output_total_bytes") == 14798297,
            "frozen machine evidence bindings")
    authorization = value.get("authorization", {})
    require(authorization.get("authorization_sha256") ==
                EXPECTED_AUTHORIZATION_SHA256 and
            authorization.get("consumed_marker_sha256") ==
                EXPECTED_CONSUMED_SHA256 and
            authorization.get("consumed_before_process_start") is True,
            "authorization/consumption binding")
    require(value.get("permission_audit") == {
        "authorization": {"gid": 20, "mode": "0644", "uid": 501},
        "consumed_marker": {"gid": 20, "mode": "0600", "uid": 501},
        "output_asset_files": {
            "count": 25, "gid": 20, "mode": "0644", "uid": 501},
        "output_root": {"gid": 20, "mode": "0755", "uid": 501},
        "output_top_level_files": {
            "count": 3, "gid": 20, "mode": "0644", "uid": 501},
    }, "frozen permission audit")
    scenes = value.get("scenes_in_order")
    require(isinstance(scenes, list) and len(scenes) == 2, "targeted scene count")
    for item, expected in zip(scenes, EXPECTED_SCENES):
        scene_id, ppm_sha, visible, region, region_visible, state, preset = expected
        require(item == {
            "interest_region": region,
            "interest_region_visible_pixels": region_visible,
            "machine_result": "PASS",
            "ppm_path": ("docs/evidence/phase2-v10i-formal-targeted-camera-v5-"
                         f"run-1/{scene_id}.ppm"),
            "ppm_sha256": ppm_sha,
            "scene_id": scene_id,
            "semantic_camera_preset": preset,
            "semantic_camera_state": state,
            "semantic_camera_transitioning": False,
            "visible_pixels": visible,
        }, scene_id + ": frozen machine result")


def validate_receipt(path: Path = DEFAULT_RECEIPT,
                     enforce_frozen_sha: bool = True) -> dict[str, Any]:
    path = path.resolve(strict=True)
    if enforce_frozen_sha:
        require(sha256(path) == EXPECTED_RECEIPT_SHA256,
                "frozen machine-pass receipt SHA mismatch")
    value = load(path)
    validate_structure(value)
    frozen = value["frozen_bindings"]
    readiness = bound_file(frozen["readiness_manifest_path"],
                           EXPECTED_READINESS_SHA256, "readiness")
    manifest = bound_file(frozen["manifest_path"], EXPECTED_MANIFEST_SHA256,
                          "targeted manifest")
    output_root = (ROOT / frozen["output_root"]).resolve(strict=True)
    require(output_root == manifest.parent and output_root.is_dir() and
            not output_root.is_symlink(), "output root binding")
    executable = Path(load(manifest)["executable"])
    require(executable.is_file() and sha256(executable) == EXPECTED_EXECUTABLE_SHA256,
            "integrated executable binding")
    targeted_result = targeted.validate(manifest, executable,
                                         EXPECTED_READINESS_SHA256)
    require(targeted_result["status"] ==
            "PASS_TARGETED_BREAK_LOW_AIM_MACHINE_CAPTURE" and
            targeted_result["formal_six_scene_evidence"] is False and
            targeted_result["formal_pass"] is False,
            "underlying targeted machine validator")

    authorization = value["authorization"]
    auth_path = bound_file(authorization["authorization_path"],
                           EXPECTED_AUTHORIZATION_SHA256, "authorization")
    marker_path = bound_file(authorization["consumed_marker_path"],
                             EXPECTED_CONSUMED_SHA256, "consumed marker")
    auth = load(auth_path)
    marker = load(marker_path)
    require(marker.get("authorization_id") == auth.get("authorization_id") ==
                authorization["authorization_id"] and
            marker.get("authorization_nonce") == auth.get("authorization_nonce") ==
                authorization["authorization_nonce"] and
            marker.get("authorization_sha256") == EXPECTED_AUTHORIZATION_SHA256 and
            marker.get("status") == "CONSUMED_BEFORE_PROCESS_START" and
            marker.get("authorized_capture_count") == 2 and
            marker.get("consumed_capture_count") == 2,
            "consumed authorization identity/count")

    audit = value.get("permission_audit", {})
    require(permission(auth_path) == audit.get("authorization") ==
                {"gid": 20, "mode": "0644", "uid": 501} and
            permission(marker_path) == audit.get("consumed_marker") ==
                {"gid": 20, "mode": "0600", "uid": 501} and
            permission(output_root) == audit.get("output_root") ==
                {"gid": 20, "mode": "0755", "uid": 501},
            "authorization/marker/output-root permissions")
    inventory = output_inventory(output_root)
    require(inventory["sha256"] == EXPECTED_OUTPUT_INVENTORY_SHA256 and
            inventory["count"] == 28 and inventory["total_bytes"] == 14798297,
            "output evidence inventory")
    top_files = [path for path in inventory["files"] if path.parent == output_root]
    asset_files = [path for path in inventory["files"] if
                   path.parent == output_root / "asset_root"]
    expected_top = audit.get("output_top_level_files", {})
    expected_assets = audit.get("output_asset_files", {})
    require(len(top_files) == expected_top.get("count") == 3 and
            len(asset_files) == expected_assets.get("count") == 25 and
            all(permission(path) == {k: expected_top[k] for k in
                                     ("gid", "mode", "uid")} for path in top_files) and
            all(permission(path) == {k: expected_assets[k] for k in
                                     ("gid", "mode", "uid")} for path in asset_files),
            "output evidence file permissions")
    require(sha256(readiness) == EXPECTED_READINESS_SHA256,
            "readiness changed during validation")
    return {
        "status": "PASS_FROZEN_TARGETED_MACHINE_PASS_V1",
        "receipt_sha256": sha256(path),
        "readiness_sha256": EXPECTED_READINESS_SHA256,
        "manifest_sha256": EXPECTED_MANIFEST_SHA256,
        "consumed_marker_sha256": EXPECTED_CONSUMED_SHA256,
        "targeted_scene_count": 2,
        "formal_six_scene_evidence": False,
        "formal_pass": False,
        "golden_promotion": False,
        "m2_promotion": False,
        "product_review_performed": False,
    }


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--receipt", type=Path, default=DEFAULT_RECEIPT)
    arguments = parser.parse_args()
    try:
        print(json.dumps(validate_receipt(arguments.receipt), indent=2,
                         sort_keys=True))
    except (OSError, ValueError, json.JSONDecodeError,
            MachinePassReceiptError, targeted.TargetedCaptureError) as error:
        print("FAIL: " + str(error))
        return 2
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
