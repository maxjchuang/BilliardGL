#!/usr/bin/env python3
"""Run the frozen Phase-5 GPU matrix only with an explicit one-time receipt."""

from __future__ import annotations

import argparse
import binascii
import hashlib
import json
from pathlib import Path
import struct
import subprocess
import time
import zlib


ROOT = Path(__file__).resolve().parents[1]
DEFAULT_READY = (
    ROOT / "docs/evidence/phase5/rules-decision-ui-gpu-capture-ready-v1.json")
DEFAULT_OUTPUT = ROOT / "docs/evidence/phase5/actual-gpu"


def sha(path: Path) -> str:
    return hashlib.sha256(path.read_bytes()).hexdigest()


def resolve_bound_path(value: str) -> Path:
    path = Path(value)
    return path if path.is_absolute() else ROOT / path


def validate_receipt(receipt: dict, ready: dict, receipt_path: Path) -> None:
    if receipt.get("schema") != "phase5_rules_decision_ui_gpu_authorization_v1":
        raise RuntimeError("authorization schema mismatch")
    if receipt.get("authorized") is not True or receipt.get("consumed") is not False:
        raise RuntimeError("explicit unconsumed authorization is required")
    if receipt.get("case_count") != 60:
        raise RuntimeError("authorization must bind exactly 60 cases")
    for field in ("authorization_id", "approved_by", "approved_at"):
        if not isinstance(receipt.get(field), str) or not receipt[field]:
            raise RuntimeError(f"authorization field is missing: {field}")
    if receipt.get("expires_unix_seconds", 0) <= time.time():
        raise RuntimeError("authorization receipt is expired")
    for field, expected in ready["frozen_bindings"].items():
        if receipt.get(field) != expected:
            raise RuntimeError(f"authorization frozen binding mismatch: {field}")
    marker = Path(str(receipt_path) + ".consumed-" + sha(receipt_path))
    if marker.exists():
        raise RuntimeError("authorization receipt was already consumed")


def ppm(path: Path) -> tuple[int, int, bytes]:
    data = path.read_bytes()
    position = 0

    def token() -> bytes:
        nonlocal position
        while position < len(data):
            if data[position] == 35:
                position = data.find(b"\n", position) + 1
            elif data[position] in b" \t\r\n":
                position += 1
            else:
                break
        end = position
        while end < len(data) and data[end] not in b" \t\r\n":
            end += 1
        value = data[position:end]
        position = end
        return value

    if token() != b"P6":
        raise RuntimeError(f"not a binary PPM: {path}")
    width, height, maximum = int(token()), int(token()), int(token())
    if maximum != 255:
        raise RuntimeError(f"unsupported PPM maximum: {path}")
    while position < len(data) and data[position] in b" \t\r\n":
        position += 1
    pixels = data[position:]
    if len(pixels) != width * height * 3:
        raise RuntimeError(f"PPM byte count mismatch: {path}")
    return width, height, pixels


def png_chunk(kind: bytes, payload: bytes) -> bytes:
    return (struct.pack(">I", len(payload)) + kind + payload +
        struct.pack(">I", binascii.crc32(kind + payload) & 0xFFFFFFFF))


def convert_png(source: Path, destination: Path) -> tuple[int, int]:
    width, height, pixels = ppm(source)
    stride = width * 3
    scanlines = b"".join(
        b"\0" + pixels[row * stride:(row + 1) * stride]
        for row in range(height))
    value = (b"\x89PNG\r\n\x1a\n" +
        png_chunk(b"IHDR", struct.pack(">IIBBBBB", width, height,
            8, 2, 0, 0, 0)) +
        png_chunk(b"IDAT", zlib.compress(scanlines, 9)) +
        png_chunk(b"IEND", b""))
    destination.write_bytes(value)
    return width, height


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--ready", type=Path, default=DEFAULT_READY)
    parser.add_argument("--authorization", type=Path, required=True)
    parser.add_argument("--output-dir", type=Path, default=DEFAULT_OUTPUT)
    parser.add_argument("--execute-authorized-capture", action="store_true")
    args = parser.parse_args()
    if not args.execute_authorized_capture:
        raise RuntimeError("refusing GPU work without --execute-authorized-capture")

    ready = json.loads(args.ready.read_text(encoding="utf-8"))
    receipt = json.loads(args.authorization.read_text(encoding="utf-8"))
    if ready.get("status") != "READY_REQUIRES_EXPLICIT_ONE_TIME_AUTH_NOT_RUN":
        raise RuntimeError("capture package is not in ready/not-run state")
    validate_receipt(receipt, ready, args.authorization)
    paths = {key: resolve_bound_path(value)
        for key, value in ready["paths"].items()}
    frozen = ready["frozen_bindings"]
    checks = {
        "plan_sha256": paths["plan"],
        "source_binding_manifest_sha256": paths["source_bindings"],
        "runner_binary_sha256": paths["runner_binary"],
        "billiards_binary_sha256": paths["billiards_binary"],
        "font_sha256": paths["font"],
    }
    for field, path in checks.items():
        if not path.is_file() or sha(path) != frozen[field]:
            raise RuntimeError(f"ready package binding mismatch: {field}")

    if args.output_dir.exists():
        raise RuntimeError("capture output directory must not already exist")
    ppm_dir = args.output_dir / "ppm"
    png_dir = args.output_dir / "png"
    raw_results = args.output_dir / "rules-decision-ui-gpu-results-v1.jsonl"
    subprocess.run([
        str(paths["runner_binary"]), str(args.authorization),
        str(paths["plan"]), str(ppm_dir), str(raw_results),
        str(paths["billiards_binary"]), str(paths["font"]),
        str(paths["source_bindings"]),
    ], cwd=ROOT, check=True)

    rows = [json.loads(line) for line in
        raw_results.read_text(encoding="utf-8").splitlines() if line]
    if len(rows) != 60 or not all(row.get("actual_gpu_capture") for row in rows):
        raise RuntimeError("actual-GPU runner did not produce 60/60 records")
    png_dir.mkdir(exist_ok=False)
    for row in rows:
        source = Path(row["ppm_path"])
        destination = png_dir / (row["capture_id"] + ".png")
        width, height = convert_png(source, destination)
        row["png_path"] = str(destination)
        row["png_sha256"] = sha(destination)
        row["png_physical_width"] = width
        row["png_physical_height"] = height
    evidence = {
        "schema": "phase5_rules_decision_ui_actual_gpu_evidence_v1",
        "status": "ACTUAL_GPU_CAPTURED_MANUAL_REVIEW_PENDING",
        "authorization_id": receipt["authorization_id"],
        "ready_manifest_sha256": sha(args.ready),
        "case_count": len(rows),
        "cases": rows,
        "claims": {
            "actual_gpu_capture_completed": 60,
            "manual_review_completed": 0,
            "beta_executed": False,
            "product_exposed": False,
            "profile_complete": False,
            "section_12_3_vs_17_e": "UNRESOLVED",
        },
    }
    evidence_path = (args.output_dir /
        "rules-decision-ui-actual-gpu-evidence-v1.json")
    evidence_path.write_text(
        json.dumps(evidence, ensure_ascii=False, indent=2, sort_keys=True) + "\n",
        encoding="utf-8")
    print(f"Phase5 actual-GPU capture complete: {evidence_path}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
