#!/usr/bin/env python3
"""Create and verify the image-free byte-identical v10i offline freeze."""

import argparse
import hashlib
import json
from pathlib import Path


ROOT=Path(__file__).resolve().parents[1]
DEFAULT_OUTPUT=ROOT/"docs/art/phase2-pocket-v10i-offline-source-freeze-v1"
REPORT="docs/art/phase2-pocket-v10i-offline-preflight-v1/phase2-pocket-v10i-offline-report-v1.json"
SOURCES=(
 ("generator","scripts/generate_phase2_modular_gltf_v10i.py"),
 ("validator","scripts/validate_phase2_modular_gltf_v10i.py"),
 ("calibrator","scripts/calibrate_phase2_pocket_v10i.py"),
 ("freeze_tool","scripts/freeze_phase2_pocket_v10i_offline.py"),
 ("asset_tests","tests/visual/test_phase2_modular_gltf_v10i.py"),
 ("calibration_tests","tests/visual/test_phase2_pocket_v10i_calibration.py"),
 ("gltf","assets/modern/phase2-v10i/focused_practice_hall_modular_v1.gltf"),
 ("asset_manifest","assets/modern/phase2-v10i/focused_practice_hall_modular_v1.asset.json"),
 ("design_gate","docs/art/phase2-pocket-v10i-offline-correction-design-gate-v1.json"))


def digest(payload):return "sha256:"+hashlib.sha256(payload).hexdigest()


def freeze(output_root=DEFAULT_OUTPUT,report_path=None):
    report_relative=REPORT if report_path is None else Path(report_path).resolve().relative_to(ROOT).as_posix()
    report=json.loads((ROOT/report_relative).read_text())
    if report.get("status")!="OFFLINE_PREFLIGHT_PASS_GPU_NOT_AUTHORIZED" or \
       report.get("machine_failures")!=[] or report.get("actual_gpu_used") is not False or \
       report.get("GPU_CAPTURE_AUTHORIZED") is not False:
        raise ValueError("only fail-free GPU=false v10i offline PASS may be frozen")
    entries=list(SOURCES)+[("offline_report",report_relative)]
    if any(Path(path).suffix.lower() in (".png",".ppm",".jpg",".jpeg") for _,path in entries):
        raise ValueError("v10i offline freeze must be image-free")
    bindings=[]
    for identifier,relative in entries:
        payload=(ROOT/relative).read_bytes(); snapshot_rel=Path("snapshot")/relative
        snapshot=output_root/snapshot_rel; snapshot.parent.mkdir(parents=True,exist_ok=True)
        if snapshot.exists() and snapshot.read_bytes()!=payload:
            raise ValueError("immutable snapshot differs: "+relative)
        snapshot.write_bytes(payload); copied=snapshot.read_bytes()
        bindings.append({"id":identifier,"original_path":relative,"original_sha256":digest(payload),
                         "snapshot_path":snapshot_rel.as_posix(),"snapshot_sha256":digest(copied),
                         "byte_identical":payload==copied})
    receipt={"schema":"phase2_pocket_v10i_offline_source_freeze_receipt_v1",
      "status":"BYTE_IDENTICAL_OFFLINE_PREFLIGHT_PASS_GPU_NOT_AUTHORIZED",
      "evidence_scope":"phase2_pocket_v10i_offline_only","visual_review_status":"NOT_RUN",
      "actual_gpu_used":False,"gpu_capture_authorized":False,"ta_review_authorized":False,
      "formal_six_scene_evidence_authorized":False,"formal_pass_claimed":False,
      "golden_promotion_claimed":False,"image_files_in_freeze":0,
      "bindings":bindings,"binding_count":len(bindings),
      "all_original_and_snapshot_sha256_match":all(i["byte_identical"] and i["original_sha256"]==i["snapshot_sha256"] for i in bindings),
      "decision":"WAIT_FOR_ROOT_INDEPENDENT_REVIEW; GPU_AND_TA_NOT_AUTHORIZED"}
    payload=(json.dumps(receipt,indent=2,sort_keys=True)+"\n").encode(); path=output_root/"receipt.json"
    if path.exists() and path.read_bytes()!=payload:raise ValueError("immutable receipt differs")
    path.write_bytes(payload);return path,receipt


def verify(output_root=DEFAULT_OUTPUT):
    receipt=json.loads((output_root/"receipt.json").read_text());errors=[]
    for item in receipt.get("bindings",[]):
        original=ROOT/item["original_path"];snapshot=output_root/item["snapshot_path"]
        if original.read_bytes()!=snapshot.read_bytes() or digest(original.read_bytes())!=item["original_sha256"] or digest(snapshot.read_bytes())!=item["snapshot_sha256"]:
            errors.append(item["id"]+": drift")
    return errors,receipt


def main():
    parser=argparse.ArgumentParser(description=__doc__);parser.add_argument("--output-root",type=Path,default=DEFAULT_OUTPUT);parser.add_argument("--verify",action="store_true");args=parser.parse_args()
    if args.verify:
        errors,receipt=verify(args.output_root.resolve())
        if errors:raise ValueError("; ".join(errors))
        print("v10i offline freeze verify PASS: %d bindings"%receipt["binding_count"])
    else:
        path,receipt=freeze(args.output_root.resolve());print("v10i offline source freeze: bindings=%d receipt=%s GPU=false"%(receipt["binding_count"],path))


if __name__=="__main__":
    try:main()
    except (OSError,ValueError,KeyError,json.JSONDecodeError) as error:
        print("phase2 v10i offline freeze failed: "+str(error));raise SystemExit(2)
