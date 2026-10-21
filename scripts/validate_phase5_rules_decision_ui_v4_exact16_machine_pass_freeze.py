#!/usr/bin/env python3
"""Read-only validator for the frozen Phase5 v4 exact16 machine pass."""

from __future__ import annotations

import argparse
import binascii
import hashlib
import json
from pathlib import Path
import stat
import struct
from typing import Any
import zlib


ROOT = Path(__file__).resolve().parents[1]
DEFAULT_RECEIPT = (
    ROOT / "docs/evidence/phase5/rules-decision-ui-v4-exact16-"
    "machine-pass-freeze-v1/machine-pass-freeze-receipt.json")
EXPECTED_RECEIPT_SHA256 = (
    "db59bc6b5e7bfd0b2038043b147deac13cde20fa6ebaafef7085bcef94d85fd5")
EXPECTED_READY_SHA256 = (
    "968cc99f330c23b8b712222b74510f6d53a1842af07e8884bca8a4bb0bb27485")
EXPECTED_AUTH_SHA256 = (
    "6bb1cd427e78668bbd32d5743bc5e0d16d23bef70586e8faab1c53bb04246cfd")
EXPECTED_MARKER_SHA256 = (
    "81f2cff642fa1d26791a46f8fa2c71ae4154a61815dcc96eb622146c525a06c0")
EXPECTED_RUNNER_CLAIM_SHA256 = (
    "c3f3f2c3eceb53da353c9567b1ba9b86c3337a9f7c29604ba28068da4e6237be")
EXPECTED_RESULTS_SHA256 = (
    "8b96f22e8045a1c6b3d1914f6bb1c162fd433f217e350d0c97e75c21d6c6202b")
EXPECTED_TELEMETRY_SHA256 = (
    "469493eec3e912ffeae00f7c3d7e2a0b46a59a6f9e0cf065490e4cf9dfccbb91")
EXPECTED_DISPATCH_SHA256 = (
    "eebbd8506064869e51f6977113eabf26405426748e2c474b55be371bae33987f")
EXPECTED_PNG_MANIFEST_SHA256 = (
    "f1242dba2c5fdde4728fbf474ae8163475f0108df16a074e0f80bc0a63076bec")
EXPECTED_CASE_AGGREGATE = (
    "602dd26c0a2b3bbfbb99b08f543614c420c2165dd2db5d3c14345ed6fedc2fff")
EXPECTED_PPM_AGGREGATE = (
    "4437b3eb1e97ad4221f6ff82de4e5ee8f529dbd39450cbb374527d104f006ef3")
EXPECTED_PNG_AGGREGATE = (
    "f9db1e2b883d05a3507a98fa7ec6edf0df88f787ecd16a9e2420dbcc3d0e5de2")
EXPECTED_OUTPUT_AGGREGATE = (
    "1fd702a74751db0e51f57f7a5c94e2e84002c5347edd3ad45418d84b078ee670")


class MachinePassFreezeError(RuntimeError):
    pass


def require(condition: bool, message: str) -> None:
    if not condition:
        raise MachinePassFreezeError(message)


def sha256(path: Path) -> str:
    digest = hashlib.sha256()
    with path.open("rb") as source:
        for chunk in iter(lambda: source.read(1024 * 1024), b""):
            digest.update(chunk)
    return digest.hexdigest()


def mode(path: Path) -> str:
    return f"{stat.S_IMODE(path.stat().st_mode):04o}"


def load(path: Path) -> dict[str, Any]:
    value = json.loads(path.read_text(encoding="utf-8"))
    require(isinstance(value, dict), str(path) + ": JSON object required")
    return value


def bound_file(relative: Any, digest: Any, label: str) -> Path:
    require(isinstance(relative, str) and isinstance(digest, str) and
            len(digest) == 64, label + ": path/SHA required")
    path = (ROOT / relative).resolve(strict=True)
    try:
        path.relative_to(ROOT.resolve())
    except ValueError as error:
        raise MachinePassFreezeError(label + ": outside repository") from error
    require(path.is_file() and sha256(path) == digest,
            label + ": missing or SHA mismatch")
    return path


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
        require(offset > start, str(path) + ": truncated PPM header")
        tokens.append(data[start:offset])
    require(offset < len(data) and data[offset] in b" \t\r\n",
            str(path) + ": PPM pixel separator")
    offset += 2 if data[offset:offset + 2] == b"\r\n" else 1
    width, height, maximum = map(int, tokens)
    pixels = data[offset:]
    require(width > 0 and height > 0 and maximum == 255 and
            len(pixels) == width * height * 3,
            str(path) + ": PPM RGB8 dimensions/byte length")
    return width, height, pixels


def decode_png(path: Path) -> tuple[int, int, bytes]:
    data = path.read_bytes()
    require(data.startswith(b"\x89PNG\r\n\x1a\n"),
            str(path) + ": PNG signature")
    offset = 8
    chunks: list[tuple[bytes, bytes]] = []
    while offset < len(data):
        require(offset + 12 <= len(data), str(path) + ": truncated PNG chunk")
        length = struct.unpack(">I", data[offset:offset + 4])[0]
        kind = data[offset + 4:offset + 8]
        end = offset + 12 + length
        require(end <= len(data), str(path) + ": PNG chunk length")
        payload = data[offset + 8:offset + 8 + length]
        expected_crc = struct.unpack(">I", data[offset + 8 + length:end])[0]
        require((binascii.crc32(kind + payload) & 0xFFFFFFFF) == expected_crc,
                str(path) + ": PNG CRC")
        chunks.append((kind, payload))
        offset = end
        if kind == b"IEND":
            break
    require(offset == len(data) and [kind for kind, _ in chunks] ==
            [b"IHDR", b"IDAT", b"IEND"],
            str(path) + ": canonical PNG chunk sequence")
    ihdr = chunks[0][1]
    require(len(ihdr) == 13, str(path) + ": PNG IHDR")
    width, height, depth, color, compression, filtering, interlace = \
        struct.unpack(">IIBBBBB", ihdr)
    require(width > 0 and height > 0 and (depth, color, compression,
            filtering, interlace) == (8, 2, 0, 0, 0),
            str(path) + ": PNG RGB8 non-interlaced contract")
    try:
        scanlines = zlib.decompress(chunks[1][1])
    except zlib.error as error:
        raise MachinePassFreezeError(str(path) + ": PNG zlib") from error
    stride = width * 3
    require(len(scanlines) == height * (stride + 1),
            str(path) + ": PNG decoded byte length")
    rows: list[bytes] = []
    for row in range(height):
        start = row * (stride + 1)
        require(scanlines[start] == 0,
                str(path) + ": PNG filter must be none")
        rows.append(scanlines[start + 1:start + 1 + stride])
    return width, height, b"".join(rows)


def row_aggregates(rows: list[dict[str, Any]]) -> tuple[str, str, str]:
    case_digest, ppm_digest, png_digest = (hashlib.sha256() for _ in range(3))
    for row in rows:
        dimensions = f"{row['physical_width']}x{row['physical_height']}"
        case_digest.update(row["capture_id"].encode())
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
    return case_digest.hexdigest(), ppm_digest.hexdigest(), png_digest.hexdigest()


def inventory_aggregate(root: Path, files: list[Path]) -> str:
    digest = hashlib.sha256()
    for path in sorted(files):
        digest.update(path.relative_to(root).as_posix().encode())
        digest.update(b"\0")
        digest.update((sha256(path) + "\n").encode())
    return digest.hexdigest()


def validate_receipt(path: Path = DEFAULT_RECEIPT,
                     enforce_frozen_sha: bool = True) -> dict[str, Any]:
    path = path.resolve(strict=True)
    if enforce_frozen_sha:
        require(sha256(path) == EXPECTED_RECEIPT_SHA256,
                "frozen machine pass receipt SHA mismatch")
    receipt = load(path)
    require(receipt.get("schema") ==
            "phase5_rules_decision_ui_v4_exact16_machine_pass_freeze_receipt_v1" and
            receipt.get("status") ==
            "TARGETED_GPU_MACHINE_16_PASS_TA_REVIEW_PENDING",
            "machine pass receipt identity")
    lineage = receipt.get("lineage", {})
    ready_path = bound_file(lineage.get("readiness_manifest_path"),
                            lineage.get("readiness_manifest_sha256"), "readiness")
    require(lineage.get("readiness_manifest_sha256") == EXPECTED_READY_SHA256,
            "readiness frozen SHA")
    ready = load(ready_path)
    ids = ready.get("case_ids_in_order")
    require(isinstance(ids, list) and len(ids) == len(set(ids)) == 16,
            "readiness exact16 IDs")

    auth_path = bound_file(lineage.get("authorization_path"),
                           lineage.get("authorization_sha256"), "authorization")
    marker_path = bound_file(lineage.get("consumed_marker_path"),
                             lineage.get("consumed_marker_sha256"), "consumed marker")
    claim_path = bound_file(lineage.get("runner_claim_path"),
                            lineage.get("runner_claim_sha256"), "runner claim")
    require(lineage.get("authorization_sha256") == EXPECTED_AUTH_SHA256 and
            lineage.get("consumed_marker_sha256") == EXPECTED_MARKER_SHA256 and
            lineage.get("runner_claim_sha256") == EXPECTED_RUNNER_CLAIM_SHA256 and
            mode(auth_path) == mode(marker_path) == mode(claim_path) == "0444",
            "authorization consumption SHA/permissions")
    auth, marker = load(auth_path), load(marker_path)
    require(auth.get("authorized") is True and auth.get("single_use") is True and
            auth.get("case_count") == 16 and auth.get("scene_ids_in_order") == ids and
            auth.get("ready_manifest_sha256") == EXPECTED_READY_SHA256 and
            auth.get("golden_promotion") is False and
            auth.get("m2_promotion") is False and
            auth.get("product_exposed") is False and
            marker.get("authorization_id") == auth.get("authorization_id") ==
                lineage.get("authorization_id") and
            marker.get("authorization_nonce") == auth.get("authorization_nonce") and
            marker.get("authorization_sha256") == EXPECTED_AUTH_SHA256 and
            marker.get("consumed_before_output_or_process") is True and
            marker.get("consumed_capture_count") == 16 and
            marker.get("scene_ids_in_order") == ids and
            claim_path.read_text() == auth["authorization_id"] + "\n",
            "authorization scope/consumption identity")

    gpu_root = (ROOT / lineage.get("gpu_output_root", "")).resolve(strict=True)
    require(gpu_root.is_dir() and mode(gpu_root) == "0555" and
            mode(gpu_root / "ppm") == "0555" and
            auth.get("authorized_output_canonical_path") == str(gpu_root),
            "frozen GPU output root/path/permission")
    results_path = gpu_root / "rules-decision-ui-v4-exact16-results.jsonl"
    telemetry_path = gpu_root / "rules-decision-ui-v4-exact16-telemetry.jsonl"
    dispatch_path = gpu_root / "rules-decision-ui-v4-exact16-dispatch-pass.json"
    require(sha256(results_path) == lineage.get("results_jsonl_sha256") ==
                EXPECTED_RESULTS_SHA256 and
            sha256(telemetry_path) == lineage.get("telemetry_jsonl_sha256") ==
                EXPECTED_TELEMETRY_SHA256 and
            sha256(dispatch_path) == lineage.get("dispatch_pass_sha256") ==
                EXPECTED_DISPATCH_SHA256,
            "results/telemetry/dispatch frozen SHA")
    results = [json.loads(line) for line in results_path.read_text().splitlines() if line]
    telemetry = [json.loads(line) for line in telemetry_path.read_text().splitlines() if line]
    dispatch = load(dispatch_path)
    require([row.get("capture_id") for row in results] == ids and
            [row.get("capture_id") for row in telemetry] == ids and
            dispatch.get("case_ids_in_order") == ids and
            dispatch.get("completed_count") == 16 and
            dispatch.get("failed_count") == dispatch.get("unrun_count") == 0 and
            dispatch.get("validation_errors") == [] and
            dispatch.get("outcome") == "PASS",
            "exact16 result/telemetry/dispatch rows")

    manifest_path = bound_file(lineage.get("stable_png_manifest_path"),
                               lineage.get("stable_png_manifest_sha256"),
                               "stable PNG manifest")
    require(lineage.get("stable_png_manifest_sha256") ==
            EXPECTED_PNG_MANIFEST_SHA256, "stable PNG manifest frozen SHA")
    manifest = load(manifest_path)
    rows = manifest.get("cases")
    require(manifest.get("schema") ==
            "phase5_rules_decision_ui_v4_exact16_stable_png_manifest_v1" and
            manifest.get("status") ==
            "PPM_BYTE_FAITHFUL_PNG16_FROZEN_FOR_TA_REVIEW" and
            manifest.get("case_ids_in_order") == ids and
            isinstance(rows, list) and len(rows) == 16 and
            [row.get("capture_id") for row in rows] == ids,
            "stable PNG manifest identity/exact16 order")
    conversion = manifest.get("conversion_contract", {})
    require(conversion == {
        "source": "P6 RGB8 PPM", "target": "PNG RGB8",
        "color_transform": "none", "row_transform": "none",
        "alpha_added": False, "filter": "none_per_scanline",
        "zlib_level": 9, "decoded_rgb_must_equal_source_ppm_rgb": True,
    }, "stable PNG byte-faithful conversion contract")

    plan: dict[str, tuple[int, int]] = {}
    for line in Path(ready["paths"]["plan"]).read_text().splitlines():
        fields = line.split("\t")
        require(len(fields) == 12, "plan field count")
        plan[fields[0]] = (int(fields[7]), int(fields[8]))
    require(list(plan) == ids, "plan exact16 order")
    expected_gpu_inventory = {results_path, telemetry_path, dispatch_path}
    freeze_root = manifest_path.parent
    frozen_receipt_path = freeze_root / "machine-pass-freeze-receipt.json"
    require(frozen_receipt_path.is_file() and
            sha256(frozen_receipt_path) == EXPECTED_RECEIPT_SHA256 and
            mode(frozen_receipt_path) == "0444",
            "canonical frozen receipt path/SHA/permission")
    expected_freeze_inventory = {frozen_receipt_path, manifest_path}
    dimensions: dict[str, int] = {}
    for ordinal, (capture_id, result, event, row) in enumerate(
            zip(ids, results, telemetry, rows), start=1):
        require(row.get("ordinal") == ordinal and
                result.get("actual_gpu_capture") is True and
                result.get("authorization_consumed_before_process") is True and
                result.get("rules_ui_after_tone_map") is True and
                result.get("gpu_draw_calls") == 1 and
                result.get("canonical_sha256") == result.get("gpu_canonical_sha256") and
                result.get("render_passes") == ["shadow_depth", "hdr_opaque", "overlay",
                                                  "aces_tone_map", "product_ui",
                                                  "product_present"] and
                event.get("actual_gpu_capture") is True and
                event.get("machine_status") == "PASS" and
                event.get("rules_ui_after_tone_map") is True,
                capture_id + ": machine GPU/after-tonemap/draw contract")
        ppm = bound_file(row.get("ppm_path"), row.get("ppm_sha256"),
                         capture_id + ": PPM")
        png = bound_file(row.get("png_path"), row.get("png_sha256"),
                         capture_id + ": PNG")
        require(mode(ppm) == row.get("ppm_mode") == "0444" and
                mode(png) == row.get("png_mode") == "0444" and
                ppm.stat().st_size == row.get("ppm_size_bytes") and
                png.stat().st_size == row.get("png_size_bytes") and
                Path(result.get("ppm_path", "")).resolve() == ppm.resolve() and
                result.get("ppm_sha256") == row.get("ppm_sha256"),
                capture_id + ": image path/SHA/size/permission")
        ppm_width, ppm_height, ppm_pixels = read_ppm(ppm)
        png_width, png_height, png_pixels = decode_png(png)
        expected_dimensions = plan[capture_id]
        require((ppm_width, ppm_height) == (png_width, png_height) ==
                expected_dimensions == (row.get("physical_width"),
                                         row.get("physical_height")) ==
                (result.get("physical_width"), result.get("physical_height")),
                capture_id + ": decoded physical dimensions")
        require(png_pixels == ppm_pixels and
                hashlib.sha256(ppm_pixels).hexdigest() ==
                    row.get("decoded_rgb_sha256") and
                row.get("png_decoded_rgb_equals_ppm_rgb") is True,
                capture_id + ": PNG must be byte-faithful to PPM RGB")
        label = f"{ppm_width}x{ppm_height}"
        dimensions[label] = dimensions.get(label, 0) + 1
        expected_gpu_inventory.add(ppm)
        expected_freeze_inventory.add(png)

    gpu_files = [item for item in gpu_root.rglob("*") if item.is_file()]
    freeze_files = [item for item in freeze_root.rglob("*") if item.is_file()]
    require(set(gpu_files) == expected_gpu_inventory and len(gpu_files) == 19 and
            all(mode(item) == "0444" for item in gpu_files),
            "exact frozen GPU output inventory")
    require(set(freeze_files) == expected_freeze_inventory and
            len(freeze_files) == 18 and mode(freeze_root) == "0555" and
            mode(manifest_path.parent / "png") == "0555" and
            all(mode(item) == "0444" for item in freeze_files),
            "exact stable PNG freeze inventory/permissions")
    require(dimensions == manifest.get("decoded_physical_dimension_counts") ==
            receipt.get("decoded_physical_dimension_counts") ==
            {"1280x720": 8, "1920x1080": 4, "2560x1440": 4},
            "decoded physical dimension counts")
    case_aggregate, ppm_aggregate, png_aggregate = row_aggregates(rows)
    aggregates = receipt.get("stable_aggregates", {})
    require(case_aggregate == aggregates.get("case_evidence_aggregate_sha256") ==
                manifest.get("aggregates", {}).get("case_evidence_aggregate_sha256") ==
                EXPECTED_CASE_AGGREGATE and
            ppm_aggregate == aggregates.get("ppm_path_sha_aggregate_sha256") ==
                manifest.get("aggregates", {}).get("ppm_path_sha_aggregate_sha256") ==
                EXPECTED_PPM_AGGREGATE and
            png_aggregate == aggregates.get("png_path_sha_aggregate_sha256") ==
                manifest.get("aggregates", {}).get("png_path_sha_aggregate_sha256") ==
                EXPECTED_PNG_AGGREGATE and
            inventory_aggregate(gpu_root, gpu_files) ==
                aggregates.get("original_gpu_output_inventory_aggregate_sha256") ==
                EXPECTED_OUTPUT_AGGREGATE,
            "stable evidence aggregates")

    counts = receipt.get("verified_counts")
    require(counts == {
        "planned_cases": 16, "result_rows": 16, "telemetry_rows": 16,
        "actual_gpu_completed": 16, "failed_cases": 0, "unrun_cases": 0,
        "ppm_files": 16, "png_files": 16,
        "independent_ta_reviews_completed": 0,
    }, "verified machine counts")
    bindings = receipt.get("frozen_runtime_bindings", {})
    require(all(ready.get("frozen_bindings", {}).get(key) == value and
                auth.get(key) == value for key, value in bindings.items()),
            "readiness/auth/runtime binding equality")
    source_manifest = Path(ready["paths"]["source_binding_manifest"])
    require(sha256(source_manifest) == bindings.get("source_binding_manifest_sha256") and
            load(source_manifest).get("aggregate_sha256") ==
                bindings.get("source_aggregate_sha256") and
            sha256(Path(ready["paths"]["runner_binary"])) ==
                bindings.get("runner_binary_sha256") and
            sha256(Path(ready["paths"]["billiards_binary"])) ==
                bindings.get("billiards_binary_sha256") and
            sha256(Path(ready["paths"]["font"])) == bindings.get("font_sha256") and
            sha256(Path(ready["paths"]["hud_token"])) ==
                bindings.get("hud_token_sha256") and
            sha256(Path(ready["paths"]["shell_token"])) ==
                bindings.get("shell_token_sha256") and
            sha256(Path(ready["paths"]["runner_source"])) ==
                bindings.get("runner_source_sha256") and
            sha256(Path(ready["paths"]["plan"])) == bindings.get("plan_sha256"),
            "live frozen runtime/source/binary/font/token bindings")
    require(receipt.get("independent_ta_review_gate") == {
        "case_count": 16, "axes_per_case": 10,
        "required_original_image_axis_decisions": 160,
        "completed_original_image_axis_decisions": 0,
        "machine_pass_does_not_equal_ta_pass": True,
    }, "independent TA 16x10 review gate")
    require(receipt.get("claims") == {
        "targeted_machine_16_pass": True, "actual_gpu_completed": 16,
        "technical_art_review_completed": 0, "product_exposed": False,
        "golden_promotion": False, "m2_promotion": False,
    }, "machine-only claims boundary")
    return {
        "status": "PASS_FROZEN_PHASE5_V4_EXACT16_MACHINE_16",
        "machine_pass_receipt_sha256": sha256(path),
        "stable_png_manifest_sha256": sha256(manifest_path),
        "case_count": 16, "ppm_count": 16, "png_count": 16,
        "byte_faithful_png_count": 16,
        "required_independent_ta_axis_decisions": 160,
        "technical_art_review_completed": 0, "product_exposed": False,
        "golden_promotion": False, "m2_promotion": False,
    }


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--receipt", type=Path, default=DEFAULT_RECEIPT)
    args = parser.parse_args()
    try:
        print(json.dumps(validate_receipt(args.receipt), indent=2, sort_keys=True))
    except (OSError, ValueError, IndexError, KeyError, struct.error,
            json.JSONDecodeError, MachinePassFreezeError) as error:
        print("FAIL: " + str(error))
        return 2
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
