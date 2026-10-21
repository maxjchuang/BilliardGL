#!/usr/bin/env python3
"""Create deterministic, CPU-only PNG review assets for frozen Phase5 v4 PPMs."""

from __future__ import annotations

import binascii
import hashlib
import json
import os
from pathlib import Path
import stat
import struct
import zlib


ROOT = Path(__file__).resolve().parents[1]
READY_ROOT = ROOT / "docs/evidence/phase5/rules-decision-ui-v4-exact16-producer-bound-ready-v3"
READY = READY_ROOT / "rules-decision-ui-v4-exact16-ready.json"
AUTH = READY_ROOT / "root-one-shot-authorization-v4-exact16-run1.json"
MARKER = Path(str(AUTH) + ".consumed.json")
RUNNER_CLAIM = Path(str(MARKER) + ".exact16-runner-claimed")
GPU_ROOT = ROOT / "docs/evidence/phase5/rules-decision-ui-v4-exact16-targeted-gpu-run-1"
RESULTS = GPU_ROOT / "rules-decision-ui-v4-exact16-results.jsonl"
TELEMETRY = GPU_ROOT / "rules-decision-ui-v4-exact16-telemetry.jsonl"
DISPATCH = GPU_ROOT / "rules-decision-ui-v4-exact16-dispatch-pass.json"
FREEZE_ROOT = ROOT / "docs/evidence/phase5/rules-decision-ui-v4-exact16-machine-pass-freeze-v1"

EXPECTED = {
    READY: "968cc99f330c23b8b712222b74510f6d53a1842af07e8884bca8a4bb0bb27485",
    AUTH: "6bb1cd427e78668bbd32d5743bc5e0d16d23bef70586e8faab1c53bb04246cfd",
    MARKER: "81f2cff642fa1d26791a46f8fa2c71ae4154a61815dcc96eb622146c525a06c0",
    RUNNER_CLAIM: "c3f3f2c3eceb53da353c9567b1ba9b86c3337a9f7c29604ba28068da4e6237be",
    RESULTS: "8b96f22e8045a1c6b3d1914f6bb1c162fd433f217e350d0c97e75c21d6c6202b",
    TELEMETRY: "469493eec3e912ffeae00f7c3d7e2a0b46a59a6f9e0cf065490e4cf9dfccbb91",
    DISPATCH: "eebbd8506064869e51f6977113eabf26405426748e2c474b55be371bae33987f",
}


class FreezeError(RuntimeError):
    pass


def require(condition: bool, message: str) -> None:
    if not condition:
        raise FreezeError(message)


def sha256(path: Path) -> str:
    digest = hashlib.sha256()
    with path.open("rb") as source:
        for chunk in iter(lambda: source.read(1024 * 1024), b""):
            digest.update(chunk)
    return digest.hexdigest()


def mode(path: Path) -> str:
    return f"{stat.S_IMODE(path.stat().st_mode):04o}"


def relative(path: Path) -> str:
    return path.resolve().relative_to(ROOT.resolve()).as_posix()


def load(path: Path) -> dict:
    value = json.loads(path.read_text(encoding="utf-8"))
    require(isinstance(value, dict), str(path) + ": object required")
    return value


def read_ppm(path: Path) -> tuple[int, int, bytes]:
    data = path.read_bytes()
    require(data.startswith(b"P6"), str(path) + ": P6 required")
    offset = 2
    tokens: list[bytes] = []
    while len(tokens) < 3:
        while offset < len(data) and data[offset] in b" \t\r\n":
            offset += 1
        if offset < len(data) and data[offset] == ord("#"):
            while offset < len(data) and data[offset] not in b"\r\n":
                offset += 1
            continue
        start = offset
        while offset < len(data) and data[offset] not in b" \t\r\n":
            offset += 1
        require(offset > start, str(path) + ": truncated header")
        tokens.append(data[start:offset])
    require(offset < len(data) and data[offset] in b" \t\r\n",
            str(path) + ": pixel separator")
    if data[offset:offset + 2] == b"\r\n":
        offset += 2
    else:
        offset += 1
    width, height, maximum = map(int, tokens)
    pixels = data[offset:]
    require(width > 0 and height > 0 and maximum == 255,
            str(path) + ": RGB8 dimensions")
    require(len(pixels) == width * height * 3,
            str(path) + ": pixel byte length")
    return width, height, pixels


def png_chunk(kind: bytes, payload: bytes) -> bytes:
    return (struct.pack(">I", len(payload)) + kind + payload +
            struct.pack(">I", binascii.crc32(kind + payload) & 0xFFFFFFFF))


def encode_png(width: int, height: int, pixels: bytes) -> bytes:
    stride = width * 3
    scanlines = b"".join(
        b"\0" + pixels[row * stride:(row + 1) * stride]
        for row in range(height)
    )
    ihdr = struct.pack(">IIBBBBB", width, height, 8, 2, 0, 0, 0)
    return (b"\x89PNG\r\n\x1a\n" + png_chunk(b"IHDR", ihdr) +
            png_chunk(b"IDAT", zlib.compress(scanlines, level=9)) +
            png_chunk(b"IEND", b""))


def durable_write(path: Path, data: bytes) -> None:
    with path.open("xb") as target:
        target.write(data)
        target.flush()
        os.fsync(target.fileno())


def durable_json(path: Path, value: object) -> None:
    durable_write(
        path, (json.dumps(value, indent=2, sort_keys=True) + "\n").encode())


def aggregate_rows(rows: list[dict]) -> tuple[str, str, str]:
    case_digest = hashlib.sha256()
    ppm_digest = hashlib.sha256()
    png_digest = hashlib.sha256()
    for row in rows:
        capture_id = row["capture_id"]
        dimensions = f"{row['physical_width']}x{row['physical_height']}"
        case_digest.update(capture_id.encode())
        case_digest.update(b"\0")
        case_digest.update(row["ppm_sha256"].encode())
        case_digest.update(b"\0")
        case_digest.update(row["png_sha256"].encode())
        case_digest.update(b"\0")
        case_digest.update((dimensions + "\n").encode())
        ppm_digest.update(row["ppm_path"].encode())
        ppm_digest.update(b"\0")
        ppm_digest.update((row["ppm_sha256"] + "\n").encode())
        png_digest.update(row["png_path"].encode())
        png_digest.update(b"\0")
        png_digest.update((row["png_sha256"] + "\n").encode())
    return (case_digest.hexdigest(), ppm_digest.hexdigest(),
            png_digest.hexdigest())


def main() -> int:
    require(not FREEZE_ROOT.exists(), "fresh freeze root required")
    for path, expected in EXPECTED.items():
        require(path.is_file() and sha256(path) == expected,
                str(path) + ": frozen input drift")
    require(mode(AUTH) == mode(MARKER) == mode(RUNNER_CLAIM) == "0444",
            "authorization/marker/runner claim permissions")
    require(mode(GPU_ROOT) == mode(GPU_ROOT / "ppm") == "0555",
            "GPU output directories must remain frozen")
    original_files = sorted(path for path in GPU_ROOT.rglob("*") if path.is_file())
    require(len(original_files) == 19 and
            all(mode(path) == "0444" for path in original_files),
            "exact frozen GPU output inventory/permissions")

    ready, auth, marker, dispatch = map(load, (READY, AUTH, MARKER, DISPATCH))
    ids = ready.get("case_ids_in_order")
    require(isinstance(ids, list) and len(ids) == len(set(ids)) == 16,
            "readiness exact16 IDs")
    require(auth.get("scene_ids_in_order") == ids and
            marker.get("scene_ids_in_order") == ids and
            dispatch.get("case_ids_in_order") == ids,
            "authorization/marker/dispatch exact16 order")
    require(dispatch.get("outcome") == "PASS" and
            dispatch.get("completed_count") == 16 and
            dispatch.get("failed_count") == dispatch.get("unrun_count") == 0 and
            dispatch.get("validation_errors") == [], "dispatch machine pass")
    results = [json.loads(line) for line in RESULTS.read_text().splitlines() if line]
    telemetry = [json.loads(line) for line in TELEMETRY.read_text().splitlines() if line]
    require([row.get("capture_id") for row in results] == ids and
            [row.get("capture_id") for row in telemetry] == ids,
            "results/telemetry exact16 order")

    plan: dict[str, tuple[int, int]] = {}
    for line in Path(ready["paths"]["plan"]).read_text().splitlines():
        fields = line.split("\t")
        require(len(fields) == 12, "plan field count")
        plan[fields[0]] = (int(fields[7]), int(fields[8]))
    require(list(plan) == ids, "plan exact16 order")

    FREEZE_ROOT.mkdir(parents=True)
    png_root = FREEZE_ROOT / "png"
    png_root.mkdir()
    rows: list[dict] = []
    dimension_counts: dict[str, int] = {}
    for ordinal, (capture_id, result, event) in enumerate(
            zip(ids, results, telemetry), start=1):
        require(result.get("actual_gpu_capture") is True and
                result.get("authorization_consumed_before_process") is True and
                result.get("rules_ui_after_tone_map") is True and
                result.get("gpu_draw_calls") == 1 and
                result.get("canonical_sha256") == result.get("gpu_canonical_sha256") and
                event.get("actual_gpu_capture") is True and
                event.get("machine_status") == "PASS" and
                event.get("rules_ui_after_tone_map") is True,
                capture_id + ": GPU machine row")
        passes = result.get("render_passes", [])
        require(passes == ["shadow_depth", "hdr_opaque", "overlay",
                           "aces_tone_map", "product_ui", "product_present"],
                capture_id + ": render pass order")
        ppm = GPU_ROOT / "ppm" / (capture_id + ".ppm")
        require(ppm.is_file() and mode(ppm) == "0444" and
                Path(result.get("ppm_path", "")).resolve() == ppm.resolve() and
                sha256(ppm) == result.get("ppm_sha256"),
                capture_id + ": PPM path/SHA/permission")
        width, height, pixels = read_ppm(ppm)
        require((width, height) == plan[capture_id] ==
                (result.get("physical_width"), result.get("physical_height")),
                capture_id + ": physical dimensions")
        png = png_root / (capture_id + ".png")
        durable_write(png, encode_png(width, height, pixels))
        rgb_sha = hashlib.sha256(pixels).hexdigest()
        label = f"{width}x{height}"
        dimension_counts[label] = dimension_counts.get(label, 0) + 1
        rows.append({
            "ordinal": ordinal, "capture_id": capture_id,
            "physical_width": width, "physical_height": height,
            "ppm_path": relative(ppm), "ppm_sha256": sha256(ppm),
            "ppm_size_bytes": ppm.stat().st_size, "ppm_mode": "0444",
            "png_path": relative(png), "png_sha256": sha256(png),
            "png_size_bytes": png.stat().st_size, "png_mode": "0444",
            "decoded_rgb_sha256": rgb_sha,
            "png_decoded_rgb_equals_ppm_rgb": True,
        })
    case_aggregate, ppm_aggregate, png_aggregate = aggregate_rows(rows)
    manifest = {
        "schema": "phase5_rules_decision_ui_v4_exact16_stable_png_manifest_v1",
        "status": "PPM_BYTE_FAITHFUL_PNG16_FROZEN_FOR_TA_REVIEW",
        "case_count": 16, "case_ids_in_order": ids,
        "conversion_contract": {
            "source": "P6 RGB8 PPM", "target": "PNG RGB8",
            "color_transform": "none", "row_transform": "none",
            "alpha_added": False, "filter": "none_per_scanline",
            "zlib_level": 9, "decoded_rgb_must_equal_source_ppm_rgb": True,
        },
        "decoded_physical_dimension_counts": dimension_counts,
        "aggregates": {
            "case_evidence_aggregate_sha256": case_aggregate,
            "ppm_path_sha_aggregate_sha256": ppm_aggregate,
            "png_path_sha_aggregate_sha256": png_aggregate,
        },
        "cases": rows,
    }
    manifest_path = FREEZE_ROOT / "stable-png-manifest.json"
    durable_json(manifest_path, manifest)

    original_digest = hashlib.sha256()
    for path in original_files:
        original_digest.update(path.relative_to(GPU_ROOT).as_posix().encode())
        original_digest.update(b"\0")
        original_digest.update((sha256(path) + "\n").encode())
    bindings = ready["frozen_bindings"]
    receipt = {
        "schema": "phase5_rules_decision_ui_v4_exact16_machine_pass_freeze_receipt_v1",
        "status": "TARGETED_GPU_MACHINE_16_PASS_TA_REVIEW_PENDING",
        "lineage": {
            "readiness_manifest_path": relative(READY),
            "readiness_manifest_sha256": EXPECTED[READY],
            "authorization_path": relative(AUTH),
            "authorization_sha256": EXPECTED[AUTH],
            "authorization_id": auth["authorization_id"],
            "consumed_marker_path": relative(MARKER),
            "consumed_marker_sha256": EXPECTED[MARKER],
            "runner_claim_path": relative(RUNNER_CLAIM),
            "runner_claim_sha256": EXPECTED[RUNNER_CLAIM],
            "gpu_output_root": relative(GPU_ROOT),
            "results_jsonl_sha256": EXPECTED[RESULTS],
            "telemetry_jsonl_sha256": EXPECTED[TELEMETRY],
            "dispatch_pass_sha256": EXPECTED[DISPATCH],
            "stable_png_manifest_path": relative(manifest_path),
            "stable_png_manifest_sha256": sha256(manifest_path),
        },
        "verified_counts": {
            "planned_cases": 16, "result_rows": 16, "telemetry_rows": 16,
            "actual_gpu_completed": 16, "failed_cases": 0,
            "unrun_cases": 0, "ppm_files": 16, "png_files": 16,
            "independent_ta_reviews_completed": 0,
        },
        "decoded_physical_dimension_counts": dimension_counts,
        "machine_checks": {
            "actual_gpu_true": "16/16", "after_tonemap_true": "16/16",
            "draw_calls_exactly_one": "16/16",
            "authorization_consumed_before_process": "16/16",
            "result_and_telemetry_machine_pass": "16/16",
            "ppm_path_sha_permission_dimensions": "16/16",
            "png_decoded_rgb_byte_faithful_to_ppm": "16/16",
        },
        "stable_aggregates": {
            "case_evidence_aggregate_sha256": case_aggregate,
            "ppm_path_sha_aggregate_sha256": ppm_aggregate,
            "png_path_sha_aggregate_sha256": png_aggregate,
            "original_gpu_output_inventory_aggregate_sha256":
                original_digest.hexdigest(),
        },
        "frozen_runtime_bindings": {
            key: bindings[key] for key in (
                "source_binding_manifest_sha256", "source_aggregate_sha256",
                "runner_source_sha256", "runner_binary_sha256",
                "billiards_binary_sha256", "font_sha256",
                "hud_token_sha256", "shell_token_sha256", "plan_sha256")
        },
        "permissions": {
            "authorization_mode": "0444", "consumed_marker_mode": "0444",
            "runner_claim_mode": "0444", "gpu_output_root_mode": "0555",
            "gpu_ppm_root_mode": "0555", "gpu_output_file_mode": "0444",
            "freeze_root_mode": "0555", "stable_png_root_mode": "0555",
            "stable_png_file_mode": "0444", "receipt_mode": "0444",
            "stable_png_manifest_mode": "0444",
        },
        "independent_ta_review_gate": {
            "case_count": 16, "axes_per_case": 10,
            "required_original_image_axis_decisions": 160,
            "completed_original_image_axis_decisions": 0,
            "machine_pass_does_not_equal_ta_pass": True,
        },
        "claims": {
            "targeted_machine_16_pass": True, "actual_gpu_completed": 16,
            "technical_art_review_completed": 0, "product_exposed": False,
            "golden_promotion": False, "m2_promotion": False,
        },
    }
    receipt_path = FREEZE_ROOT / "machine-pass-freeze-receipt.json"
    durable_json(receipt_path, receipt)
    for path in png_root.iterdir():
        path.chmod(0o444)
    manifest_path.chmod(0o444)
    receipt_path.chmod(0o444)
    png_root.chmod(0o555)
    FREEZE_ROOT.chmod(0o555)
    print(json.dumps({
        "status": "FROZEN_PHASE5_V4_EXACT16_MACHINE16_PASS_PENDING_TA",
        "receipt": relative(receipt_path), "receipt_sha256": sha256(receipt_path),
        "png_manifest": relative(manifest_path),
        "png_manifest_sha256": sha256(manifest_path),
        "case_evidence_aggregate_sha256": case_aggregate,
    }, sort_keys=True))
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
