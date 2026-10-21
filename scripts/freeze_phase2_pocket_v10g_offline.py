#!/usr/bin/env python3
"""Create a byte-identical, image-free v10g offline source freeze."""

import argparse
import hashlib
import json
from pathlib import Path


ROOT = Path(__file__).resolve().parents[1]
DEFAULT_OUTPUT = ROOT / "docs/art/phase2-pocket-v10g-offline-source-freeze-v2"
REPORT = "docs/art/phase2-pocket-v10g-offline-preflight-v2/phase2-pocket-v10g-offline-report-v2.json"
SOURCES = (
    ("generator", "scripts/generate_phase2_modular_gltf_v10g.py"),
    ("validator", "scripts/validate_phase2_modular_gltf_v10g.py"),
    ("calibrator", "scripts/calibrate_phase2_pocket_v10g.py"),
    ("freeze_tool", "scripts/freeze_phase2_pocket_v10g_offline.py"),
    ("asset_tests", "tests/visual/test_phase2_modular_gltf_v10g.py"),
    ("calibration_tests", "tests/visual/test_phase2_pocket_v10g_calibration.py"),
    ("gltf", "assets/modern/phase2-v10g/focused_practice_hall_modular_v1.gltf"),
    ("asset_manifest", "assets/modern/phase2-v10g/focused_practice_hall_modular_v1.asset.json"),
    ("design_gate_v2", "docs/art/phase2-pocket-v10g-offline-correction-design-gate-v2.json"),
    ("diagnostic", "docs/art/phase2-pocket-v10f-gpu-preflight-v1/root-cause-diagnostic-v1.json"),
)


def digest(payload):
    return "sha256:" + hashlib.sha256(payload).hexdigest()


def freeze(output_root=DEFAULT_OUTPUT):
    report_path = ROOT / REPORT
    report_document = json.loads(report_path.read_text(encoding="utf-8"))
    if report_document.get("status") != "OFFLINE_PREFLIGHT_PASS_GPU_NOT_AUTHORIZED" or \
            report_document.get("machine_failures") != [] or \
            report_document.get("gpu_capture_authorized") is not False:
        raise ValueError("only a fail-free offline PASS may be frozen")
    entries = list(SOURCES) + [("offline_report", REPORT)]
    if any(Path(path).suffix.lower() in (".png", ".ppm") for _, path in entries):
        raise ValueError("v10g offline freeze must not contain images")
    bindings = []
    for identifier, relative in entries:
        original = ROOT / relative
        payload = original.read_bytes()
        snapshot_relative = Path("snapshot") / relative
        snapshot = output_root / snapshot_relative
        snapshot.parent.mkdir(parents=True, exist_ok=True)
        snapshot.write_bytes(payload)
        snapshot_payload = snapshot.read_bytes()
        bindings.append({
            "id": identifier,
            "original_path": relative,
            "original_sha256": digest(payload),
            "snapshot_path": snapshot_relative.as_posix(),
            "snapshot_sha256": digest(snapshot_payload),
            "byte_identical": payload == snapshot_payload,
        })
    receipt = {
        "schema": "phase2_pocket_v10g_offline_source_freeze_receipt_v2",
        "status": "BYTE_IDENTICAL_OFFLINE_PREFLIGHT_PASS_GPU_NOT_AUTHORIZED",
        "evidence_scope": "phase2_pocket_v10g_offline_only",
        "visual_review_status": "NOT_RUN",
        "actual_gpu_used": False,
        "gpu_capture_authorized": False,
        "ta_review_authorized": False,
        "formal_six_scene_evidence_authorized": False,
        "image_files_in_freeze": 0,
        "bindings": bindings,
        "binding_count": len(bindings),
        "all_original_and_snapshot_sha256_match":
            all(item["byte_identical"] and
                item["original_sha256"] == item["snapshot_sha256"]
                for item in bindings),
        "decision": "WAIT_FOR_ROOT_INDEPENDENT_REVIEW; GPU_AND_TA_NOT_AUTHORIZED",
    }
    if not receipt["all_original_and_snapshot_sha256_match"]:
        raise ValueError("source freeze is not byte-identical")
    receipt_path = output_root / "receipt.json"
    receipt_path.write_text(json.dumps(receipt, indent=2, sort_keys=True) + "\n",
                            encoding="utf-8")
    return receipt_path, receipt


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--output-root", type=Path, default=DEFAULT_OUTPUT)
    args = parser.parse_args()
    path, receipt = freeze(args.output_root.resolve())
    print("v10g offline source freeze: bindings=%d receipt=%s" %
          (receipt["binding_count"], path))
    return 0


if __name__ == "__main__":
    try:
        raise SystemExit(main())
    except (OSError, ValueError, KeyError, json.JSONDecodeError) as error:
        print("phase2 v10g offline freeze failed: " + str(error))
        raise SystemExit(2)
