#!/usr/bin/env python3
"""Create and verify the immutable, image-free v10h offline source freeze."""

import argparse
import hashlib
import json
from pathlib import Path


ROOT = Path(__file__).resolve().parents[1]
DEFAULT_OUTPUT = ROOT / "docs/art/phase2-pocket-v10h-offline-source-freeze-v1"
REPORT = "docs/art/phase2-pocket-v10h-offline-preflight-v1/phase2-pocket-v10h-offline-report-v1.json"
SOURCES = (
    ("generator", "scripts/generate_phase2_modular_gltf_v10h.py"),
    ("validator", "scripts/validate_phase2_modular_gltf_v10h.py"),
    ("calibrator", "scripts/calibrate_phase2_pocket_v10h.py"),
    ("freeze_tool", "scripts/freeze_phase2_pocket_v10h_offline.py"),
    ("asset_tests", "tests/visual/test_phase2_modular_gltf_v10h.py"),
    ("calibration_tests", "tests/visual/test_phase2_pocket_v10h_calibration.py"),
    ("gltf", "assets/modern/phase2-v10h/focused_practice_hall_modular_v1.gltf"),
    ("asset_manifest", "assets/modern/phase2-v10h/focused_practice_hall_modular_v1.asset.json"),
    ("design_gate", "docs/art/phase2-pocket-v10h-offline-correction-design-gate-v1.json"),
)


def digest(payload):
    return "sha256:" + hashlib.sha256(payload).hexdigest()


def freeze(output_root=DEFAULT_OUTPUT, report_path=None):
    report_relative = REPORT if report_path is None else \
        Path(report_path).resolve().relative_to(ROOT).as_posix()
    report_document = json.loads((ROOT / report_relative).read_text(encoding="utf-8"))
    if report_document.get("status") != "OFFLINE_PREFLIGHT_PASS_GPU_NOT_AUTHORIZED" or \
            report_document.get("machine_failures") != [] or \
            report_document.get("actual_gpu_used") is not False or \
            report_document.get("GPU_CAPTURE_AUTHORIZED") is not False:
        raise ValueError("only a fail-free GPU=false offline PASS may be frozen")
    entries = list(SOURCES) + [("offline_report", report_relative)]
    if any(Path(path).suffix.lower() in (".png", ".ppm", ".jpg", ".jpeg")
           for _, path in entries):
        raise ValueError("v10h offline freeze must not contain images")
    bindings = []
    for identifier, relative in entries:
        payload = (ROOT / relative).read_bytes()
        snapshot_relative = Path("snapshot") / relative
        snapshot = output_root / snapshot_relative
        snapshot.parent.mkdir(parents=True, exist_ok=True)
        if snapshot.exists() and snapshot.read_bytes() != payload:
            raise ValueError("immutable snapshot already exists with different bytes: " + relative)
        snapshot.write_bytes(payload)
        copied = snapshot.read_bytes()
        bindings.append({"id": identifier, "original_path": relative,
                         "original_sha256": digest(payload),
                         "snapshot_path": snapshot_relative.as_posix(),
                         "snapshot_sha256": digest(copied),
                         "byte_identical": payload == copied})
    receipt = {
        "schema": "phase2_pocket_v10h_offline_source_freeze_receipt_v1",
        "status": "BYTE_IDENTICAL_OFFLINE_PREFLIGHT_PASS_GPU_NOT_AUTHORIZED",
        "evidence_scope": "phase2_pocket_v10h_offline_only",
        "visual_review_status": "NOT_RUN", "actual_gpu_used": False,
        "gpu_capture_authorized": False, "ta_review_authorized": False,
        "formal_six_scene_evidence_authorized": False,
        "formal_pass_claimed": False, "golden_promotion_claimed": False,
        "image_files_in_freeze": 0, "bindings": bindings,
        "binding_count": len(bindings),
        "all_original_and_snapshot_sha256_match": all(
            item["byte_identical"] and item["original_sha256"] == item["snapshot_sha256"]
            for item in bindings),
        "decision": "WAIT_FOR_ROOT_INDEPENDENT_REVIEW; GPU_AND_TA_NOT_AUTHORIZED",
    }
    if not receipt["all_original_and_snapshot_sha256_match"]:
        raise ValueError("source freeze is not byte-identical")
    receipt_bytes = (json.dumps(receipt, indent=2, sort_keys=True) + "\n").encode()
    receipt_path = output_root / "receipt.json"
    if receipt_path.exists() and receipt_path.read_bytes() != receipt_bytes:
        raise ValueError("immutable receipt already exists with different bytes")
    receipt_path.write_bytes(receipt_bytes)
    return receipt_path, receipt


def verify(output_root=DEFAULT_OUTPUT):
    receipt = json.loads((output_root / "receipt.json").read_text(encoding="utf-8"))
    errors = []
    for item in receipt.get("bindings", []):
        original = ROOT / item["original_path"]
        snapshot = output_root / item["snapshot_path"]
        if digest(original.read_bytes()) != item["original_sha256"]:
            errors.append(item["id"] + ": original drift")
        if digest(snapshot.read_bytes()) != item["snapshot_sha256"]:
            errors.append(item["id"] + ": snapshot drift")
        if original.read_bytes() != snapshot.read_bytes():
            errors.append(item["id"] + ": not byte-identical")
    return errors, receipt


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--output-root", type=Path, default=DEFAULT_OUTPUT)
    parser.add_argument("--verify", action="store_true")
    args = parser.parse_args()
    if args.verify:
        errors, receipt = verify(args.output_root.resolve())
        if errors:
            raise ValueError("; ".join(errors))
        print("v10h offline freeze verify PASS: %d bindings" % receipt["binding_count"])
    else:
        path, receipt = freeze(args.output_root.resolve())
        print("v10h offline source freeze: bindings=%d receipt=%s GPU=false" %
              (receipt["binding_count"], path))


if __name__ == "__main__":
    try:
        main()
    except (OSError, ValueError, KeyError, json.JSONDecodeError) as error:
        print("phase2 v10h offline freeze failed: " + str(error))
        raise SystemExit(2)
