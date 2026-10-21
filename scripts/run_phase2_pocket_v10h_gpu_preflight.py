#!/usr/bin/env python3
"""Run one root-authorized v10h GPU close-up; fail closed before GPU otherwise.

This file is capture-capable but is never invoked by the readiness workflow.
It requires an external, exact-hash-bound one-shot root authorization and two
absent single-use output roots before it can start the renderer executable.
"""

import argparse
import hashlib
import json
import math
import os
from pathlib import Path
import shutil
import subprocess
import sys
import tempfile

from generate_phase2_modular_gltf_v10h import ASSET_NAME, MANIFEST_NAME
from generate_phase2_modular_gltf_v10f import (
    BALL_DIAMETER_M, POCKET_BACK_Z_M, POCKET_FRONT_Z_M,
    POCKET_RIM_WIDTH_M, POCKET_SIDEWALL_DROP_M, marker_binding_sha256,
)
from run_phase2_pocket_v10f_gpu_preflight import (
    geometry_measurements, largest_black_component, read_ppm,
)
from run_phase2_pocket_v10g_gpu_preflight import object_id_measurements_v10g
from validate_phase2_modular_gltf_v10h import validate


ROOT = Path(__file__).resolve().parents[1]
ASSET_ROOT = ROOT / "assets/modern/phase2-v10h"
SHARED_ROOT = ROOT / "assets/modern/phase2-v10f"
GATE = ROOT / "docs/art/phase2-pocket-v10h-offline-correction-design-gate-v1.json"
OFFLINE_REPORT = ROOT / "docs/art/phase2-pocket-v10h-offline-preflight-v1/phase2-pocket-v10h-offline-report-v1.json"
OFFLINE_FREEZE = ROOT / "docs/art/phase2-pocket-v10h-offline-source-freeze-v1/receipt.json"
READY_ROOT = ROOT / "docs/art/phase2-pocket-v10h-gpu-ready-v1"
READY_MANIFEST = READY_ROOT / "phase2-pocket-v10h-gpu-readiness-manifest-v1.json"
READY_RECEIPT = READY_ROOT / "authorization-ready-receipt-v1.json"
DEFAULT_AUTHORIZATION = READY_ROOT / "root-one-shot-authorization.json"
DEFAULT_OUTPUT = ROOT / "docs/art/phase2-pocket-v10h-gpu-preflight-v1"
DEFAULT_GPU_FREEZE = ROOT / "docs/art/phase2-pocket-v10h-gpu-source-freeze-v1"
DEFAULT_BUILD = ROOT / "build-phase2-v10h-gpu-ready"
CAPTURE_NAME = "pocket_closeup.png"
METRICS_NAME = "pocket_gpu_metrics.json"
MANIFEST_OUT = "phase2-pocket-v10h-gpu-preflight-manifest-v1.json"
EVIDENCE_SCOPE = "phase2_pocket_v10h_single_scene_non_formal"
EXPECTED = {
    "gate": "fd6c2d501a54dce74c2ceb1117b2364099465010e880781c8049f9cd6a269d08",
    "offline_report": "54ed555358b4664b5817b25576d3bb8d664467e6093fa7a45ddadda0da56bc78",
    "offline_freeze": "3121c61fd2e60961e11fc1fe5d5d113c04fec594c80317c7aff993b33efd53d9",
    "gltf": "99ddceb6771dc1a05e1a21b469940f457f229aa871368c10cd2bc40970c0e136",
    "asset_manifest": "4534acc249d1cf505e0ee6303ea237123536226eb272f2a347d4703049ccc98a",
    "renderer_source": "7626026f5cbbb8df443df3a89e9cb7c4243c97d74113f927ea0705d1bc7e745b",
    "smoke_source": "e820c76e64fccd4bb0d58b531104d708d90182b9e6fcc0332869737831fc7dd5",
}


def sha256(path):
    return "sha256:" + hashlib.sha256(path.read_bytes()).hexdigest()


def _write_json(path, value):
    path.write_text(json.dumps(value, indent=2, sort_keys=True) + "\n", encoding="utf-8")


def canonicalize_metrics(raw):
    required = ("renderer", "backend", "width", "height", "quality", "msaa_samples",
                "camera", "warmup_frames", "sample_frames",
                "pocket_only_ui_pass_draw_calls", "full_frame_ui_delta_nonzero_pixels",
                "p95_total_frame_ms", "draw_calls_max", "triangles_max",
                "texture_resident_mib", "measurement_method")
    missing = [key for key in required if key not in raw]
    if missing:
        raise RuntimeError("raw smoke metrics are incomplete: " + ",".join(missing))
    result = {key: raw[key] for key in required}
    result.update({
        "schema": "phase2_pocket_v10h_actual_gpu_metrics_v1",
        "evidence_scope": EVIDENCE_SCOPE, "visual_review_status": "NOT_RUN",
        "machine_status_before_execution": "NOT_RUN", "actual_gpu": True,
        "actual_gpu_capture_count": 1, "additional_gpu_capture_authorized": False,
        "ta_review_authorized": False, "formal_six_scene_evidence_authorized": False,
        "formal_pass_claimed": False, "golden_promotion_claimed": False,
        "legacy_smoke_metadata_disposition": "discarded_and_not_inherited",
    })
    serialized = json.dumps(result)
    if any(token in serialized for token in ("PENDING_TA", "PENDING_ROOT_AND_TA", "v10c")):
        raise RuntimeError("canonical metrics retained stale review metadata")
    return result


def _verify_offline_freeze():
    receipt = json.loads(OFFLINE_FREEZE.read_text(encoding="utf-8"))
    if receipt.get("status") != "BYTE_IDENTICAL_OFFLINE_PREFLIGHT_PASS_GPU_NOT_AUTHORIZED" or \
            receipt.get("binding_count") != 10 or \
            receipt.get("all_original_and_snapshot_sha256_match") is not True or \
            receipt.get("actual_gpu_used") is not False:
        raise RuntimeError("v10h offline freeze is not the exact 10/10 GPU=false package")
    for binding in receipt["bindings"]:
        original = ROOT / binding["original_path"]
        snapshot = OFFLINE_FREEZE.parent / binding["snapshot_path"]
        if original.read_bytes() != snapshot.read_bytes() or \
                sha256(original) != binding["original_sha256"] or \
                sha256(snapshot) != binding["snapshot_sha256"]:
            raise RuntimeError("offline freeze binding drift: " + binding["id"])
    return receipt


def _verify_ready_package(executable):
    manifest = json.loads(READY_MANIFEST.read_text(encoding="utf-8"))
    receipt = json.loads(READY_RECEIPT.read_text(encoding="utf-8"))
    if manifest.get("status") != "READY_FOR_ROOT_ONE_SHOT_AUTHORIZATION_GPU_NOT_AUTHORIZED" or \
            manifest.get("actual_gpu_used") is not False or \
            manifest.get("gpu_capture_authorized") is not False or \
            manifest.get("authorization_file_present") is not False or \
            not all(manifest.get("readiness_checks", {}).values()):
        raise RuntimeError("v10h GPU readiness manifest is not an un-authorized PASS")
    if receipt.get("status") != "AUTHORIZATION_READY_GPU_NOT_AUTHORIZED" or \
            receipt.get("ready_manifest_sha256") != sha256(READY_MANIFEST) or \
            receipt.get("actual_gpu_used") is not False or \
            receipt.get("gpu_capture_authorized") is not False:
        raise RuntimeError("v10h authorization-ready receipt is invalid")
    bindings = manifest.get("bindings", {})
    for name, binding in bindings.items():
        path = ROOT / binding["path"] if not Path(binding["path"]).is_absolute() else Path(binding["path"])
        if not path.is_file() or sha256(path) != binding["sha256"]:
            raise RuntimeError("readiness binding drift: " + name)
    if bindings.get("renderer_executable", {}).get("sha256") != sha256(executable):
        raise RuntimeError("selected renderer executable differs from ready package")
    return manifest, receipt


def _verify_authorization(path, ready_manifest, ready_receipt, executable):
    if not path.is_file():
        raise RuntimeError("root one-shot authorization file is missing; GPU start forbidden")
    authorization = json.loads(path.read_text(encoding="utf-8"))
    exact = {
        "schema": "phase2_pocket_v10h_root_one_shot_gpu_authorization_v1",
        "status": "AUTHORIZED_FOR_EXACTLY_ONE_GPU_CAPTURE",
        "authorized_by": "root", "gpu_capture_authorized": True,
        "authorized_capture_count": 1, "consumed_capture_count": 0,
        "scene_id": "pocket_closeup", "width": 1920, "height": 1080,
        "quality": "medium", "msaa_samples": 4,
        "evidence_scope": EVIDENCE_SCOPE,
        "additional_capture_authorized": False,
        "ta_review_authorized": False,
        "formal_six_scene_evidence_authorized": False,
        "formal_pass_authorized": False, "golden_promotion_authorized": False,
    }
    for key, expected in exact.items():
        if authorization.get(key) != expected:
            raise RuntimeError("root authorization field mismatch: " + key)
    if not isinstance(authorization.get("authorization_id"), str) or \
            not authorization["authorization_id"].strip():
        raise RuntimeError("root authorization_id is missing")
    expected_bindings = {
        "ready_manifest": sha256(READY_MANIFEST), "ready_receipt": sha256(READY_RECEIPT),
        "runner_source": sha256(Path(__file__).resolve()),
        "renderer_executable": sha256(executable),
        "gate": "sha256:" + EXPECTED["gate"],
        "offline_report": "sha256:" + EXPECTED["offline_report"],
        "offline_freeze": "sha256:" + EXPECTED["offline_freeze"],
        "gltf": "sha256:" + EXPECTED["gltf"],
        "asset_manifest": "sha256:" + EXPECTED["asset_manifest"],
    }
    if authorization.get("bindings") != expected_bindings:
        raise RuntimeError("root authorization exact SHA bindings mismatch")
    return authorization


def pretrigger_checks(asset_root, executable, output, gpu_freeze, authorization_path):
    if output.exists() or gpu_freeze.exists():
        raise RuntimeError("single-use GPU evidence or freeze directory already exists")
    paths = {
        "gate": GATE, "offline_report": OFFLINE_REPORT, "offline_freeze": OFFLINE_FREEZE,
        "gltf": asset_root / ASSET_NAME,
        "asset_manifest": asset_root / MANIFEST_NAME,
        "renderer_source": ROOT / "src/Billiards/render/modern/modern_renderer.cpp",
        "smoke_source": ROOT / "spikes/render_core_profile/modern_renderer_slice_smoke.mm",
    }
    checks = {}
    for name, path in paths.items():
        checks[name + "_sha_exact"] = path.is_file() and sha256(path) == "sha256:" + EXPECTED[name]
    if not all(checks.values()):
        raise RuntimeError("pre-trigger frozen SHA mismatch")
    report = json.loads(OFFLINE_REPORT.read_text(encoding="utf-8"))
    if report.get("status") != "OFFLINE_PREFLIGHT_PASS_GPU_NOT_AUTHORIZED" or \
            report.get("machine_failures") != [] or \
            report.get("machine_check_count") != 89 or \
            sum(report.get("machine_checks", {}).values()) != 89 or \
            report.get("actual_gpu_used") is not False or \
            report.get("GPU_CAPTURE_AUTHORIZED") is not False:
        raise RuntimeError("v10h offline report is not the frozen 89/89 GPU=false PASS")
    _verify_offline_freeze()
    document = json.loads(paths["gltf"].read_text(encoding="utf-8"))
    asset_manifest = json.loads(paths["asset_manifest"].read_text(encoding="utf-8"))
    errors, validator_report = validate(document, asset_manifest, asset_root)
    if errors:
        raise RuntimeError("v10h asset validator failed: " + "; ".join(errors))
    if not executable.is_file() or not os.access(executable, os.X_OK):
        raise RuntimeError("fresh renderer executable is missing or not executable")
    if executable.stat().st_mtime_ns < max(paths["renderer_source"].stat().st_mtime_ns,
                                           paths["smoke_source"].stat().st_mtime_ns):
        raise RuntimeError("renderer executable predates frozen source")
    ready_manifest, ready_receipt = _verify_ready_package(executable)
    authorization = _verify_authorization(authorization_path, ready_manifest,
                                          ready_receipt, executable)
    checks.update({
        "offline_89_of_89": True, "offline_freeze_10_of_10": True,
        "asset_validator_pass": True, "fresh_executable": True,
        "ready_package_exact": True, "root_one_shot_authorization_exact": True,
        "output_single_use_absent": True, "gpu_freeze_single_use_absent": True,
        "metadata_pretrigger_not_run": True,
    })
    return checks, validator_report, document, asset_manifest, paths, authorization


def _runtime_asset_root(directory, asset_root):
    runtime = Path(directory)
    for source in SHARED_ROOT.iterdir():
        if source.is_file() and source.name not in (ASSET_NAME, MANIFEST_NAME):
            (runtime / source.name).symlink_to(source)
    shutil.copyfile(asset_root / ASSET_NAME, runtime / ASSET_NAME)
    shutil.copyfile(asset_root / MANIFEST_NAME, runtime / MANIFEST_NAME)
    return runtime


def evaluate_hard_gates(gpu_metrics, image, geometry, marker_sha,
                        document, asset_manifest, prechecks):
    luma_values = image["luma"]
    bands = image["sidewall_geometric_luma_bands"]
    checks = {"pretrigger_" + name: passed for name, passed in prechecks.items()}
    checks.update({
        "actual_gpu_capture_count_exactly_one": gpu_metrics["actual_gpu_capture_count"] == 1,
        "viewport_1920x1080": [gpu_metrics["width"], gpu_metrics["height"]] == [1920, 1080],
        "medium_4x_actual_gpu": gpu_metrics["quality"] == "medium" and
            gpu_metrics["msaa_samples"] == 4 and
            gpu_metrics["measurement_method"] == "gpu_timer_query_plus_cpu_frame",
        "warmup_120_and_samples_300": gpu_metrics["warmup_frames"] >= 120 and
            gpu_metrics["sample_frames"] >= 300,
        "luma_rim_40_to_52": 40 <= luma_values["rim_median"] <= 52,
        "luma_sidewall_upper_27_to_31": 27 <= luma_values["sidewall_upper_median"] <= 31,
        "luma_sidewall_lower_8_to_11": 8 <= luma_values["sidewall_lower_median"] <= 11,
        "luma_void_1_to_3_p95_at_most_4": 1 <= luma_values["void_median"] <= 3 and
            luma_values["void_p95"] <= 4,
        "ordered_luma_chain": luma_values["rim_median"] > luma_values["sidewall_upper_median"] >
            luma_values["sidewall_lower_median"] > luma_values["void_median"],
        "rim_minus_upper_at_least_10": luma_values["rim_median"]-luma_values["sidewall_upper_median"] >= 10,
        "upper_minus_lower_at_least_12": luma_values["sidewall_upper_median"]-luma_values["sidewall_lower_median"] >= 12,
        "lower_minus_void_at_least_5": luma_values["sidewall_lower_median"]-luma_values["void_median"] >= 5,
        "sidewall_neutral_channel_spread_at_most_12":
            bands["upper"]["maximum_channel_spread"] <= 12 and
            bands["lower"]["maximum_channel_spread"] <= 12,
        "sidewall_bands_each_128px_15percent_and_9_of_11":
            min(bands["upper"]["sample_count"], bands["lower"]["sample_count"]) >= 128 and
            min(bands["upper"]["coverage_fraction"], bands["lower"]["coverage_fraction"]) >= .15 and
            bands["central_columns_sampled"] == 11 and bands["central_column_coverage"] >= 9,
        "void_span_exact_1_20d": abs(geometry["void_minimum_span_m"]-BALL_DIAMETER_M*1.20) <= 1e-6,
        "void_front_exact_1_00d": max(abs(v-POCKET_FRONT_Z_M) for v in geometry["void_front_z_range_m"]) <= 1e-6,
        "void_back_exact_2_20d": max(abs(v-POCKET_BACK_Z_M) for v in geometry["void_back_z_range_m"]) <= 1e-6,
        "rim_sidewall_drop_position_matched_0_25mm": geometry["rim_sidewall_drop_boundary_error_max_m"] <= .00025,
        "rim_width_frozen": abs(geometry["rim_width_m"]-POCKET_RIM_WIDTH_M) <= 1e-9,
        "sidewall_drop_frozen": abs(geometry["sidewall_vertical_drop_m"]-POCKET_SIDEWALL_DROP_M) <= 1e-9,
        "rounded_arc_segments_by_lod": geometry["minimum_arc_segments_per_half_by_lod"] == {"0":24,"1":16,"2":10},
        "drop_has_five_distinct_widths": all(row["distinct_widths_0_5mm"] >= 5 for row in geometry["drop_width_profiles"]),
        "drop_bottom_mouth_ratio_0_55_to_0_85": all(.55 <= row["bottom_to_mouth_width_ratio"] <= .85 for row in geometry["drop_width_profiles"]),
        "walnut_does_not_enter_cavity": geometry["walnut_apron_maximum_cavity_z_m"] <= 1e-6,
        "connected_stage_height_at_least_72px": image["connected_stage_component"]["height_px"] >= 72,
        "ordered_stage_columns_11_of_11": image["ordered_stage_columns"]["sampled"] == 11 and image["ordered_stage_columns"]["passing"] == 11,
        "void_is_not_rectangular_slot": image["void_rectangularity"] < .85,
        "walnut_area_inside_envelope_at_most_10_percent": image["walnut_area_inside_mouth_envelope_fraction"] <= .10,
        "pocket_ui_and_full_frame_delta_zero": gpu_metrics["pocket_only_ui_pass_draw_calls"] == 0 and gpu_metrics["full_frame_ui_delta_nonzero_pixels"] == 0,
        "gold_bar_absent": image["gold_component_pixels_bottom_band"] == 0,
        "physics_marker_binding_frozen": marker_sha ==
            document["asset"]["extras"]["physics_marker_binding_sha256"] ==
            asset_manifest["alignment_binding"]["physics_marker_binding_sha256"],
        "visual_marker_z_mapping_exact": asset_manifest["alignment_binding"]["visual_to_marker_frame"]["z_visual"] == "-z_marker",
        "p95_frame_under_16_667ms": gpu_metrics["p95_total_frame_ms"] < 16.667,
        "fresh_metadata_no_review_claim": gpu_metrics["evidence_scope"] == EVIDENCE_SCOPE and gpu_metrics["visual_review_status"] == "NOT_RUN" and not gpu_metrics["ta_review_authorized"],
    })
    return checks


def _freeze_gpu_evidence(freeze_root, output, binding_files, status):
    entries = list(binding_files.items()) + [
        ("gpu_manifest", output / MANIFEST_OUT), ("gpu_capture", output / CAPTURE_NAME),
        ("gpu_metrics", output / METRICS_NAME),
        ("authorization_consumed", output / "gpu_capture_authorization_consumed.json")]
    bindings = []
    for identifier, original in entries:
        relative = original.resolve().relative_to(ROOT)
        snapshot_rel = Path("snapshot") / relative
        snapshot = freeze_root / snapshot_rel
        snapshot.parent.mkdir(parents=True, exist_ok=True)
        snapshot.write_bytes(original.read_bytes())
        bindings.append({"id": identifier, "original_path": relative.as_posix(),
                         "original_sha256": sha256(original),
                         "snapshot_path": snapshot_rel.as_posix(),
                         "snapshot_sha256": sha256(snapshot), "byte_identical": True})
    receipt = {"schema": "phase2_pocket_v10h_gpu_source_freeze_receipt_v1",
               "status": "BYTE_IDENTICAL_" + status,
               "evidence_scope": EVIDENCE_SCOPE, "actual_gpu_capture_count": 1,
               "additional_gpu_capture_authorized": False,
               "visual_review_status": "NOT_RUN", "ta_review_authorized": False,
               "formal_six_scene_evidence_authorized": False,
               "formal_pass_claimed": False, "golden_promotion_claimed": False,
               "bindings": bindings, "binding_count": len(bindings),
               "all_original_and_snapshot_sha256_match": True,
               "decision": "NO_RERUN; WAIT_FOR_ROOT"}
    _write_json(freeze_root / "receipt.json", receipt)
    return receipt


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--authorization", type=Path, default=DEFAULT_AUTHORIZATION)
    parser.add_argument("--output-dir", type=Path, default=DEFAULT_OUTPUT)
    parser.add_argument("--freeze-dir", type=Path, default=DEFAULT_GPU_FREEZE)
    parser.add_argument("--build-dir", type=Path, default=DEFAULT_BUILD)
    parser.add_argument("--asset-root", type=Path, default=ASSET_ROOT)
    args = parser.parse_args()
    output, freeze_root = args.output_dir.resolve(), args.freeze_dir.resolve()
    executable = args.build_dir.resolve() / "BilliardsModernRendererSliceSmoke"
    prechecks, validator_report, document, asset_manifest, paths, authorization = \
        pretrigger_checks(args.asset_root.resolve(), executable, output, freeze_root,
                          args.authorization.resolve())
    output.mkdir(parents=True, exist_ok=False)
    consumed = dict(authorization)
    consumed.update({"status": "CONSUMED_BEFORE_GPU_START", "consumed_capture_count": 1,
                     "additional_capture_authorized": False,
                     "source_authorization_sha256": sha256(args.authorization.resolve())})
    consumed_path = output / "gpu_capture_authorization_consumed.json"
    with consumed_path.open("x", encoding="utf-8") as destination:
        destination.write(json.dumps(consumed, indent=2, sort_keys=True) + "\n")
    with tempfile.TemporaryDirectory(prefix="billiardgl-v10h-runtime-") as directory:
        runtime_root = _runtime_asset_root(directory, args.asset_root.resolve())
        environment = os.environ.copy()
        environment["BILLIARDGL_POCKET_ONLY"] = "1"
        environment["BILLIARDGL_MODERN_PHASE2_ROOT"] = str(runtime_root)
        completed = subprocess.run([str(executable), str(output)], cwd=ROOT,
                                   env=environment, text=True, capture_output=True)
    if completed.returncode:
        raise RuntimeError("the consumed one-shot GPU process failed: " + completed.stdout + completed.stderr)
    ppm, object_ids = output / "pocket_closeup.ppm", output / "pocket_object_ids.ppm"
    width, height, pixels = read_ppm(ppm)
    id_width, id_height, id_pixels = read_ppm(object_ids)
    if [width, height] != [1920,1080] or [id_width,id_height] != [width,height]:
        raise RuntimeError("actual-GPU output dimensions differ from authorization")
    image = object_id_measurements_v10g(width, height, id_pixels, pixels)
    object_id_sha = sha256(object_ids); object_ids.unlink()
    png = output / CAPTURE_NAME
    subprocess.run(["sips", "-s", "format", "png", str(ppm), "--out", str(png)],
                   check=True, capture_output=True, text=True); ppm.unlink()
    metrics_path = output / METRICS_NAME
    gpu_metrics = canonicalize_metrics(json.loads(metrics_path.read_text(encoding="utf-8")))
    _write_json(metrics_path, gpu_metrics)
    geometry = geometry_measurements(document)
    marker_indices = [i for i,n in enumerate(document["nodes"]) if n.get("name", "").startswith("SOCKET_")]
    marker_sha = marker_binding_sha256(document, marker_indices)
    checks = evaluate_hard_gates(gpu_metrics, image, geometry, marker_sha,
                                 document, asset_manifest, prechecks)
    failures = [name for name, passed in checks.items() if not passed]
    status = "MACHINE_PASS_PENDING_ROOT_REVIEW_TA_NOT_AUTHORIZED" if not failures else "REJECTED_MACHINE_NO_RERUN"
    binding_files = {"production_gltf": args.asset_root.resolve()/ASSET_NAME,
                     "production_asset_manifest": args.asset_root.resolve()/MANIFEST_NAME,
                     "renderer_executable": executable, "runner_source": Path(__file__).resolve(),
                     "ready_manifest": READY_MANIFEST, "ready_receipt": READY_RECEIPT,
                     "offline_report": OFFLINE_REPORT, "offline_freeze": OFFLINE_FREEZE, "design_gate": GATE}
    manifest = {"schema": "phase2_pocket_v10h_actual_gpu_preflight_manifest_v1",
                "status": status, "evidence_scope": EVIDENCE_SCOPE,
                "machine_status_before_execution": "NOT_RUN", "visual_review_status": "NOT_RUN",
                "actual_gpu_capture_count": 1, "additional_gpu_capture_authorized": False,
                "ta_review_authorized": False, "formal_six_scene_evidence_authorized": False,
                "formal_pass_claimed": False, "golden_promotion_claimed": False,
                "capture": {"scene_id":"pocket_closeup","file":CAPTURE_NAME,"sha256":sha256(png),
                            "width":width,"height":height,"quality":"medium","msaa_samples":4,"actual_gpu":True},
                "object_id_diagnostic": {"retained":False,"sha256_before_discard":object_id_sha},
                "gpu_metrics": gpu_metrics, "image_measurements": image,
                "geometry_measurements": geometry, "pretrigger_checks": prechecks,
                "machine_checks": checks, "machine_failures": failures,
                "offline_validator_report": validator_report,
                "production_binding": {name:{"path":str(path.relative_to(ROOT)),"sha256":sha256(path)} for name,path in binding_files.items()},
                "decision": "NO_RERUN; WAIT_FOR_ROOT"}
    _write_json(output / MANIFEST_OUT, manifest)
    freeze_root.mkdir(parents=True, exist_ok=False)
    _freeze_gpu_evidence(freeze_root, output, binding_files, status)
    print(status)
    return 0


if __name__ == "__main__":
    try:
        raise SystemExit(main())
    except (OSError, ValueError, KeyError, RuntimeError,
            subprocess.SubprocessError, json.JSONDecodeError) as error:
        print("phase2 pocket v10h actual-GPU preflight failed: " + str(error), file=sys.stderr)
        raise SystemExit(1)
