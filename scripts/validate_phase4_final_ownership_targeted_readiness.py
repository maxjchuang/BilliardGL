#!/usr/bin/env python3
"""Fail-closed validator for final-ownership targeted11 NOT_RUN readiness."""

from __future__ import annotations

import argparse
import hashlib
import json
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
CANONICAL_PLAN_SHA256 = "3b969b34e52fe2e01e31e34b148c4ce7c211682db65b3c609a43274366c8717d"
ORDERED_PROBE_DIGEST = "d533924980efc85aeb94e4f1d31831d93e2bfce744be3b66481d08c56d10289a"
LINEAGE_V1_SHA256 = "c0b7fac77dd098b489290d8aa561d81b1346d624bc87c323e8ed47d985bd89b7"
SUPERSEDED_V2_SHA256 = "258d9de17427e42839ad71f3339121dff3eb7c08dd354c6e54f0aa2cec6bf41e"
FAILED_V3_READY_SHA256 = "943d9bc17383de817d97fc5984fd9d5590e0afde234ae7d2b5807add64caeeb9"
FAILED_V3_DISPATCH_SHA256 = "8bf47205bddc1957822241bc9a9a66ee34aa2afb8a1f3b08c43f0379dae41804"
FAILED_V4_READY_SHA256 = "a823fbae2f291404673ec3ec55c50c48851e9c32e9d0ad15d50551163716d33b"
FAILED_V4_DISPATCH_SHA256 = "32ddc5988864fbeebef094749d14a98728c384d4063789d41fd89bd8e395615e"
SUPERSEDED_V5_READY_SHA256 = "a0878668a236d82551afbab624ce5dc425894abb9609fed09fee686d839d06ac"
SUPERSEDED_V6_READY_SHA256 = "5cdc06c944f5045839013e21a643842ae81e3f1d2c205faf11f46498a0030e23"
FAILED_V7_READY_SHA256 = "7a2bd592f7d1e6766af51658c73053c7a44e1ec875ecc184215b420ec4278486"
FAILED_V7_DISPATCH_SHA256 = "e14979e634c03d676d3ddd99e1dc714352a9c2c6e9ecd1c01b9af8fdeadba66e"
FAILED_V8_READY_SHA256 = "9a9d44ff3cd388e85a1e4f603b74e89d3d934f4f132ec25191dbcbc0eb5e44af"
FAILED_V8_DISPATCH_SHA256 = "1b4cac8f1df51e8b0d04a3e06770cddbf815b8a64377326f424c58e3901cfd89"
FAILED_V9_READY_SHA256 = "c3057b9147b232935c29a0a1f046511e53b8e5c97b426822c9c0325d45f196ab"
FAILED_V9_DISPATCH_SHA256 = "597bbf92be859cf0d4e37cbf677140bdafa286cde59412077a75416fcf035f0a"
FAILED_V10_READY_SHA256 = "878d69d0833b1a8becd6322fbb29e3cdb20f57723a8d1d3ff43c985c10750f37"
FAILED_V10_DISPATCH_SHA256 = "5b5923a8efbce6f293af4d2fd71ce2d4c016d2faaabc5dbd4d19600ea10772ae"

GATE_BINDINGS = {
    "design_gate_sha256": "c3b6f032deaac6bd39d32755d4ab423915e8b70b08b6da18626707b8d7134056",
    "diagnostic_manifest_sha256": "fb041c6e96c83f30df78742ead8a59251e3425f8b62999dd16390b349bdf7349",
    "root_report_sha256": "54d0bef6af28085f6e87456645802d4eb3e2537c6ca1869a57fece27141ab50b",
    "ta_gate_sha256": "ad95db70125384a1de289792002aba05c3557d587a70ea849d5d168f415e57fa",
    "frozen_failure_sha256": "2dbd499249df1e44631669165a891f94d232f4fa436c9fd0910716de35d2717f",
    "frozen_ppm_sha256": "f3626f44ead6cfe4729e6ff3b76168747154b2452d0ca2afd70e1520b0e09744",
    "gl1282_failure_evidence_sha256": "8dfd743ae367f1ed083df9dcff97e337cbd130848ace4fa256835127bea05f04",
    "gl1282_minimal_correction_gate_sha256": "22a9cb6f6393ecb70df148ca42f830a640162e80cc4810a3f9ac04656a692a6d",
    "failed_v3_dispatch_receipt_sha256": FAILED_V3_DISPATCH_SHA256,
    "v4_pairing_failure_evidence_sha256": "b25865928621e56a667302ebf9175d94ad512265d9e4cbf3174eebe259227f35",
    "v4_pairing_correction_gate_sha256": "103c6b20ba5647fbb6cc82a7748cd362d378de34bffa2cd01f7f610a8469e269",
    "failed_v4_dispatch_receipt_sha256": FAILED_V4_DISPATCH_SHA256,
    "v7_association_failure_evidence_sha256": "3db8ccde3034d6dc6f59d546b75bbc5986d6692c13e6647c9fb5d8f9ed1d31ec",
    "v7_owned_island_correction_gate_sha256": "9f6c5febd88a06b22a8d05bf621f1310ddc8c854950dc02990cd857b21002cf2",
    "failed_v7_dispatch_receipt_sha256": FAILED_V7_DISPATCH_SHA256,
    "v8_visual_failure_evidence_sha256": "7994824c30ef85e41bb4a80d0372e8fee8b011b832507ac18c09f09e7adb07b7",
    "v8_visual_style_correction_gate_sha256": "0fe967c8ca0e855c6cb0cd29f745aefd7dfcf8a9ae387eb1522d254753196526",
    "failed_v8_dispatch_receipt_sha256": FAILED_V8_DISPATCH_SHA256,
    "failed_v8_authorization_sha256": "58e6e901dc1ef435cd3f0aa072267cece4aa278c6d9fa111bc38cd9bd623b1a4",
    "failed_v8_consumed_sha256": "74b135e0f7054827cb7d81bbf117fe9fa096bdfe3fee9ea9d0e7ced73046a23e",
    "failed_v8_telemetry_sha256": "48d34fb11a342ffb7ff00f0729396ad48544bcfec4db43470bedd3d92148bd99",
    "failed_v8_ppm_sha256": "1761d04326782eebaef2f92043d422d32f1140297bc64515a8a114d637c51da7",
    "v9_compositing_failure_evidence_sha256": "6bfcaf2c5ac5602deb557ccca176ebfd478c4183cf9bad6a054b6642f13a39eb",
    "v9_compositing_style_correction_gate_sha256": "e80baf05f0240872ba24b9ca9a6c7daa0d5a51496dde99902014d8ea3391a4f6",
    "failed_v9_dispatch_receipt_sha256": FAILED_V9_DISPATCH_SHA256,
    "failed_v9_authorization_sha256": "67d06a6f7a8cd25fb38367beb0e8b975eeaf8dd6905dc250ec5ebb511b2d08c0",
    "failed_v9_consumed_sha256": "2b20f90a76309fa425264580582f020b94120f0cb102af0949f18516e983e66e",
    "failed_v9_telemetry_sha256": "3f40581fc4f66861a3a5f0953d6350619ca3195ad0c560f1202c5458dd66f62a",
    "failed_v9_ppm_sha256": "90db92505afaf6d6c2d74193dee646afb73515a42acb3d25fa32b8034d84686c",
    "v10_machine_contract_failure_evidence_sha256": "c78fa8df1e74dbe377b97bdf3f28949bb8a778abc743952abcdb308f3693ffdc",
    "v10_machine_contract_correction_gate_sha256": "98575c3e3c48e99e9a056c047ba59e268a23c038d2685ed7252edd5d0cbe525a",
    "failed_v10_dispatch_receipt_sha256": FAILED_V10_DISPATCH_SHA256,
    "failed_v10_authorization_sha256": "dc0c60ddba2bef6a86ee4c1e328727543e24de5153260af905afd5f8d383fa8f",
    "failed_v10_consumed_sha256": "6cd0c9ec5d07b4ecd637aca6bff25048897e51541bb13e0d29571c3635c0a33c",
    "failed_v10_telemetry_sha256": "d8e06f0aa0e77d0e677af72ce2250a563d2595b70b790b05ad5728c9cc59c94d",
    "failed_v10_ppm_sha256": "82d6f9e5aba1821b3b27c00f2b76977fff035cf8541c768bd5361c1ee4ee744a",
}
GATE_FILES = {
    "design_gate_sha256": ROOT / "docs/art/phase4-overlay-v4-final-ownership-instrumentation-implementation-design-gate-v1.json",
    "diagnostic_manifest_sha256": ROOT / "docs/art/phase4-overlay-v4-ordinal1-offline-diagnostic-v1/diagnostic-evidence-manifest-v1.json",
    "root_report_sha256": ROOT / "docs/art/phase4-overlay-v4-ordinal1-offline-diagnostic-v1/root-cause-diagnostic-report-v1.json",
    "ta_gate_sha256": ROOT / "docs/art/phase4-overlay-v4-targeted-ordinal1-root-cause-diagnostic-gate-v1.json",
    "frozen_failure_sha256": ROOT / "docs/art/phase4-training-replay-overlay-v4-targeted-failure-receipt-v1.json",
    "frozen_ppm_sha256": ROOT / "docs/art/phase4-training-replay-overlay-v4-targeted-gpu-v1/camera_player_low_1080.ppm",
    "gl1282_failure_evidence_sha256": ROOT / "docs/art/phase4-final-ownership-v3-gl1282-offline-diagnostic-v1/failure-evidence-receipt-v1.json",
    "gl1282_minimal_correction_gate_sha256": ROOT / "docs/art/phase4-final-ownership-v3-gl1282-offline-diagnostic-v1/minimal-correction-design-gate-v1.json",
    "failed_v3_dispatch_receipt_sha256": ROOT / "docs/art/phase4-training-replay-overlay-v4-final-ownership-targeted11-output-v3/phase4-final-ownership-dispatch-failure-receipt-v1.json",
    "v4_pairing_failure_evidence_sha256": ROOT / "docs/art/phase4-final-ownership-v4-ordinal1-offline-diagnostic-v1/failure-evidence-receipt-v1.json",
    "v4_pairing_correction_gate_sha256": ROOT / "docs/art/phase4-final-ownership-v4-ordinal1-offline-diagnostic-v1/minimal-pairing-correction-design-gate-v1.json",
    "failed_v4_dispatch_receipt_sha256": ROOT / "docs/art/phase4-training-replay-overlay-v4-final-ownership-targeted11-output-v4/phase4-final-ownership-dispatch-failure-receipt-v1.json",
    "v7_association_failure_evidence_sha256": ROOT / "docs/art/phase4-final-ownership-v7-ordinal1-offline-diagnostic-v1/failure-evidence-receipt-v1.json",
    "v7_owned_island_correction_gate_sha256": ROOT / "docs/art/phase4-final-ownership-v7-ordinal1-offline-diagnostic-v1/minimal-owned-island-correction-design-gate-v1.json",
    "failed_v7_dispatch_receipt_sha256": ROOT / "docs/art/phase4-training-replay-overlay-v4-final-ownership-targeted11-output-v7/phase4-final-ownership-dispatch-failure-receipt-v1.json",
    "v8_visual_failure_evidence_sha256": ROOT / "docs/art/phase4-final-ownership-v8-ordinal2-offline-diagnostic-v1/failure-evidence-receipt-v1.json",
    "v8_visual_style_correction_gate_sha256": ROOT / "docs/art/phase4-final-ownership-v8-ordinal2-offline-diagnostic-v1/minimal-visual-style-correction-design-gate-v1.json",
    "failed_v8_dispatch_receipt_sha256": ROOT / "docs/art/phase4-training-replay-overlay-v4-final-ownership-targeted11-output-v8/phase4-final-ownership-dispatch-failure-receipt-v1.json",
    "failed_v8_authorization_sha256": ROOT / "docs/art/phase4-training-replay-overlay-v4-final-ownership-targeted11-ready-v8/root-one-shot-gpu-authorization-v8-run1.json",
    "failed_v8_consumed_sha256": ROOT / "docs/art/phase4-training-replay-overlay-v4-final-ownership-targeted11-ready-v8/root-one-shot-gpu-authorization-v8-run1.json.consumed",
    "failed_v8_telemetry_sha256": ROOT / "docs/art/phase4-training-replay-overlay-v4-final-ownership-targeted11-output-v8/results.jsonl.final-ownership.jsonl",
    "failed_v8_ppm_sha256": ROOT / "docs/art/phase4-training-replay-overlay-v4-final-ownership-targeted11-output-v8/camera_elevated_1080.ppm",
    "v9_compositing_failure_evidence_sha256": ROOT / "docs/art/phase4-final-ownership-v9-ordinal1-offline-diagnostic-v1/failure-evidence-receipt-v1.json",
    "v9_compositing_style_correction_gate_sha256": ROOT / "docs/art/phase4-final-ownership-v9-ordinal1-offline-diagnostic-v1/minimal-compositing-style-correction-design-gate-v1.json",
    "failed_v9_dispatch_receipt_sha256": ROOT / "docs/art/phase4-training-replay-overlay-v4-final-ownership-targeted11-output-v9/phase4-final-ownership-dispatch-failure-receipt-v1.json",
    "failed_v9_authorization_sha256": ROOT / "docs/art/phase4-training-replay-overlay-v4-final-ownership-targeted11-ready-v9/root-one-shot-gpu-authorization-v9-run1.json",
    "failed_v9_consumed_sha256": ROOT / "docs/art/phase4-training-replay-overlay-v4-final-ownership-targeted11-ready-v9/root-one-shot-gpu-authorization-v9-run1.json.consumed",
    "failed_v9_telemetry_sha256": ROOT / "docs/art/phase4-training-replay-overlay-v4-final-ownership-targeted11-output-v9/results.jsonl.final-ownership.jsonl",
    "failed_v9_ppm_sha256": ROOT / "docs/art/phase4-training-replay-overlay-v4-final-ownership-targeted11-output-v9/camera_player_low_1080.ppm",
    "v10_machine_contract_failure_evidence_sha256": ROOT / "docs/art/phase4-final-ownership-v10-ordinal1-offline-diagnostic-v1/failure-evidence-receipt-v1.json",
    "v10_machine_contract_correction_gate_sha256": ROOT / "docs/art/phase4-final-ownership-v10-ordinal1-offline-diagnostic-v1/minimal-machine-contract-correction-design-gate-v1.json",
    "failed_v10_dispatch_receipt_sha256": ROOT / "docs/art/phase4-training-replay-overlay-v4-final-ownership-targeted11-output-v10/phase4-final-ownership-dispatch-failure-receipt-v1.json",
    "failed_v10_authorization_sha256": ROOT / "docs/art/phase4-training-replay-overlay-v4-final-ownership-targeted11-ready-v10/root-one-shot-gpu-authorization-v10-run1.json",
    "failed_v10_consumed_sha256": ROOT / "docs/art/phase4-training-replay-overlay-v4-final-ownership-targeted11-ready-v10/root-one-shot-gpu-authorization-v10-run1.json.consumed",
    "failed_v10_telemetry_sha256": ROOT / "docs/art/phase4-training-replay-overlay-v4-final-ownership-targeted11-output-v10/results.jsonl.final-ownership.jsonl",
    "failed_v10_ppm_sha256": ROOT / "docs/art/phase4-training-replay-overlay-v4-final-ownership-targeted11-output-v10/camera_player_low_1080.ppm",
}
IMPLEMENTATION_SOURCES = [
    "src/Billiards/render/modern/modern_renderer.cpp",
    "src/Billiards/render/modern/modern_renderer.h",
    "src/Billiards/render/ui/product_ui_gpu_renderer.cpp",
    "src/Billiards/training/trajectory_overlay_renderer.cpp",
    "src/Billiards/training/trajectory_overlay_renderer.h",
    "src/Billiards/training/final_semantic_ownership.cpp",
    "src/Billiards/training/final_semantic_ownership.h",
    "src/Billiards/training/phase4_final_ownership_instrumentation.cpp",
    "src/Billiards/training/phase4_final_ownership_instrumentation.h",
    "src/Billiards/training/trajectory_compound_visibility.cpp",
    "src/Billiards/training/trajectory_compound_visibility.h",
]
ALL_BOUND_SOURCES = sorted(IMPLEMENTATION_SOURCES + [
    "CMakeLists.txt",
    "scripts/generate_phase4_overlay_v4_gpu_runner.py",
    "scripts/validate_phase4_final_ownership_implementation.py",
    "scripts/prepare_phase4_final_ownership_targeted_readiness.py",
    "scripts/validate_phase4_final_ownership_targeted_readiness.py",
    "scripts/run_phase4_final_ownership_targeted_capture.py",
    "scripts/validate_phase4_final_ownership_capture_dispatch.py",
    "scripts/validate_phase4_final_ownership_v3_gl1282_diagnostic.py",
    "scripts/diagnose_phase4_final_ownership_v4_ordinal1_offline.py",
    "scripts/validate_phase4_final_ownership_v4_ordinal1_offline_diagnostic.py",
    "scripts/diagnose_phase4_final_ownership_v7_ordinal1_offline.py",
    "scripts/validate_phase4_final_ownership_v7_ordinal1_offline_diagnostic.py",
    "scripts/diagnose_phase4_final_ownership_v8_ordinal2_offline.py",
    "scripts/validate_phase4_final_ownership_v8_ordinal2_offline_diagnostic.py",
    "scripts/diagnose_phase4_final_ownership_v9_ordinal1_offline.py",
    "scripts/validate_phase4_final_ownership_v9_ordinal1_offline_diagnostic.py",
    "scripts/diagnose_phase4_final_ownership_v10_ordinal1_offline.py",
    "scripts/validate_phase4_final_ownership_v10_ordinal1_offline_diagnostic.py",
    "scripts/validate_phase4_v10_machine_contract_implementation.py",
    "tests/trajectory_overlay_renderer_tests.cpp",
    "tests/phase4_final_ownership_telemetry_tests.cpp",
    "tests/phase4_final_semantic_ownership_tests.cpp",
    "tests/phase4_frozen_ordinal1_failure_replay.cpp",
    "tests/test_phase4_final_ownership_static_conformance.py",
    "tests/test_phase4_final_ownership_targeted_readiness.py",
    "tests/test_phase4_final_ownership_capture_dispatch.py",
    "tests/test_phase4_final_ownership_v3_gl1282_diagnostic.py",
    "tests/test_phase4_final_ownership_v4_ordinal1_offline_diagnostic.py",
    "tests/test_phase4_final_ownership_v7_ordinal1_offline_diagnostic.py",
    "tests/test_phase4_final_ownership_v8_ordinal2_offline_diagnostic.py",
    "tests/test_phase4_final_ownership_v9_ordinal1_offline_diagnostic.py",
    "tests/test_phase4_final_ownership_v10_ordinal1_offline_diagnostic.py",
    "tests/test_phase4_v10_machine_contract_implementation.py",
])
STATIC_CHECKS = {
    "blend_resolve_tone_ui_clip_pairing": True,
    "coverage_contrast_denominator_route_contract_unchanged": True,
    "default_off_and_color_attachment_zero_invariance": True,
    "msaa_sample_payload_and_positions": True,
    "visual_sublayers_adaptive_backdrops_and_final_ldr_colors": True,
    "probe_functions_and_algorithm_sources_immutable": True,
    "same_production_vertex_stream_and_ordered_track_ranges": True,
    "v8_v9_visual_gates_and_frozen_failures_bound": True,
    "v10_exact_two_machine_predicate_ledger": True,
}
IMMUTABLE_INVARIANTS = {
    "actual_lateral_offset_px": 0.0,
    "actual_visible_probe_coverage_min": 0.9,
    "aggregate_visible_probe_coverage_min": 0.9,
    "compositing_order": "actual_then_predicted",
    "maximum_screen_space_offset_px": 0.0,
    "minimum_local_contrast_ratio": 4.0,
    "minimum_owned_area_pixel_equivalents": 1.5,
    "ordinal1_source_probe_denominator": 95,
    "ordinal2_source_probe_denominator": 83,
    "ordinal2_ordered_source_probe_digest":
        "a3314c4ad04a72bae1718b290129604067954c16685d1d7573726caeb6d5b3f9",
    "ordinal2_source_probe_ids_bound_by_visual_gate_sha256":
        GATE_BINDINGS["v8_visual_style_correction_gate_sha256"],
    "predicted_lateral_offset_px": 0.0,
    "predicted_visible_probe_coverage_min": 0.9,
}


def sha(path: Path) -> str:
    return hashlib.sha256(path.read_bytes()).hexdigest()


def aggregate(paths: list[str]) -> str:
    digest = hashlib.sha256()
    for relative in paths:
        digest.update(relative.encode())
        digest.update(b"\0")
        digest.update(sha(ROOT / relative).encode())
        digest.update(b"\n")
    return digest.hexdigest()


def load_bound_json(manifest: dict, path_key: str, sha_key: str,
                    errors: list[str]) -> dict | None:
    try:
        path = Path(manifest[path_key])
        if not path.is_file() or sha(path) != manifest[sha_key]:
            errors.append(f"binding_drift:{path_key}")
            return None
        return json.loads(path.read_text())
    except (KeyError, OSError, ValueError, TypeError) as error:
        errors.append(f"binding_invalid:{path_key}:{type(error).__name__}")
        return None


def expected_plan_inventory(plan: Path, errors: list[str]) -> list[dict]:
    try:
        if not plan.is_file() or sha(plan) != CANONICAL_PLAN_SHA256:
            errors.append("canonical_plan_binding_drift")
            return []
        rows = [line.split("\t") for line in plan.read_text().splitlines()]
        if len(rows) != 11 or any(len(row) < 18 for row in rows):
            errors.append("canonical_plan_inventory_invalid")
            return []
        return [{
            "ordinal_one_based": ordinal,
            "capture_id": row[0],
            "scenario_id": row[1],
            "camera_preset": row[7],
            "physical_viewport_px": [int(row[10]), int(row[11])],
            "device_scale": float(row[13]),
            "ui_scale": float(row[15]),
            "palette": row[14],
            "overlay_state": row[17],
        } for ordinal, row in enumerate(rows, 1)]
    except (OSError, ValueError) as error:
        errors.append(f"canonical_plan_invalid:{type(error).__name__}")
        return []


def validate_manifest(path: Path) -> list[str]:
    errors: list[str] = []
    try:
        manifest = json.loads(path.read_text())
    except (OSError, ValueError, TypeError) as error:
        return [f"manifest_invalid:{type(error).__name__}"]

    for key, gate_path in GATE_FILES.items():
        if not gate_path.is_file() or sha(gate_path) != GATE_BINDINGS[key]:
            errors.append(f"pinned_gate_file_drift:{key}")
    try:
        visual_gate = json.loads(GATE_FILES[
            "v8_visual_style_correction_gate_sha256"].read_text())
        visual_invariants = visual_gate["immutable_invariants"]
        ordinal2_ids = visual_invariants["ordinal2_source_probe_ids"]
        if visual_invariants.get("ordinal2_source_probe_count") != 83 or \
                visual_invariants.get("ordinal2_ordered_source_probe_digest") != \
                "a3314c4ad04a72bae1718b290129604067954c16685d1d7573726caeb6d5b3f9" or \
                len(ordinal2_ids) != 83 or len(set(ordinal2_ids)) != 83:
            errors.append("ordinal2_visual_gate_probe_inventory_invalid")
    except (KeyError, OSError, ValueError, TypeError):
        errors.append("ordinal2_visual_gate_probe_inventory_invalid")
    if manifest.get("gate_bindings") != GATE_BINDINGS:
        errors.append("gate_bindings_invalid")
    if manifest.get("schema") != "billiardgl.phase4.final_ownership_targeted11_ready.v11" or \
            manifest.get("status") != "READY_NOT_RUN_GPU_NOT_AUTHORIZED":
        errors.append("readiness_status_invalid")
    try:
        lineage_path = Path(manifest["lineage_v1_ready_manifest_path"])
        lineage = json.loads(lineage_path.read_text())
        if sha(lineage_path) != LINEAGE_V1_SHA256 or \
                manifest.get("lineage_v1_ready_manifest_sha256") != LINEAGE_V1_SHA256 or \
                lineage.get("schema") != \
                "billiardgl.phase4.final_ownership_targeted11_ready.v1" or \
                lineage.get("status") != "READY_NOT_RUN_GPU_NOT_AUTHORIZED" or \
                lineage.get("not_run_count") != 11 or \
                lineage.get("gpu_capture_authorized") is not False:
            errors.append("lineage_v1_readiness_invalid")
    except (KeyError, OSError, ValueError, TypeError):
        errors.append("lineage_v1_readiness_invalid")
    try:
        superseded_path = Path(manifest["supersedes_v2_ready_manifest_path"])
        superseded = json.loads(superseded_path.read_text())
        if sha(superseded_path) != SUPERSEDED_V2_SHA256 or \
                manifest.get("supersedes_v2_ready_manifest_sha256") != \
                SUPERSEDED_V2_SHA256 or \
                manifest.get("superseded_v2_capture_authorized") is not False or \
                superseded.get("schema") != \
                "billiardgl.phase4.final_ownership_targeted11_ready.v2" or \
                superseded.get("gpu_capture_authorized") is not False or \
                superseded.get("authorization_issued") is not False:
            errors.append("superseded_v2_readiness_invalid")
    except (KeyError, OSError, ValueError, TypeError):
        errors.append("superseded_v2_readiness_invalid")
    try:
        failed_v3_path = Path(manifest["failed_v3_ready_manifest_path"])
        failed_v3 = json.loads(failed_v3_path.read_text())
        failed_receipt_path = Path(manifest["failed_v3_dispatch_receipt_path"])
        failed_receipt = json.loads(failed_receipt_path.read_text())
        if sha(failed_v3_path) != FAILED_V3_READY_SHA256 or \
                manifest.get("failed_v3_ready_manifest_sha256") != FAILED_V3_READY_SHA256 or \
                sha(failed_receipt_path) != FAILED_V3_DISPATCH_SHA256 or \
                manifest.get("failed_v3_dispatch_receipt_sha256") != FAILED_V3_DISPATCH_SHA256 or \
                failed_v3.get("schema") != \
                "billiardgl.phase4.final_ownership_targeted11_ready.v3" or \
                failed_receipt.get("status") != \
                "CAPTURE_INFRASTRUCTURE_FAILED_NO_ORDINAL_TELEMETRY" or \
                [failed_receipt.get(key) for key in
                 ("completed_count", "failed_count", "unrun_count")] != [0, 0, 11] or \
                manifest.get("failed_v3_authorization_consumed_not_reusable") is not True or \
                manifest.get("failed_v3_retry_authorized") is not False:
            errors.append("failed_v3_lineage_invalid")
    except (KeyError, OSError, ValueError, TypeError):
        errors.append("failed_v3_lineage_invalid")
    try:
        failed_v4_path = Path(manifest["failed_v4_ready_manifest_path"])
        failed_v4 = json.loads(failed_v4_path.read_text())
        failed_receipt_path = Path(manifest["failed_v4_dispatch_receipt_path"])
        failed_receipt = json.loads(failed_receipt_path.read_text())
        if sha(failed_v4_path) != FAILED_V4_READY_SHA256 or \
                manifest.get("failed_v4_ready_manifest_sha256") != FAILED_V4_READY_SHA256 or \
                sha(failed_receipt_path) != FAILED_V4_DISPATCH_SHA256 or \
                manifest.get("failed_v4_dispatch_receipt_sha256") != FAILED_V4_DISPATCH_SHA256 or \
                failed_v4.get("schema") != \
                "billiardgl.phase4.final_ownership_targeted11_ready.v4" or \
                failed_receipt.get("status") != "CAPTURE_FAILED_TELEMETRY_PRESERVED" or \
                [failed_receipt.get(key) for key in
                 ("completed_count", "failed_count", "unrun_count")] != [0, 1, 10] or \
                manifest.get("failed_v4_authorization_consumed_not_reusable") is not True or \
                manifest.get("failed_v4_retry_authorized") is not False:
            errors.append("failed_v4_lineage_invalid")
    except (KeyError, OSError, ValueError, TypeError):
        errors.append("failed_v4_lineage_invalid")
    try:
        superseded_v5_path = Path(manifest["superseded_v5_ready_manifest_path"])
        superseded_v5 = json.loads(superseded_v5_path.read_text())
        if sha(superseded_v5_path) != SUPERSEDED_V5_READY_SHA256 or \
                manifest.get("superseded_v5_ready_manifest_sha256") != \
                SUPERSEDED_V5_READY_SHA256 or \
                manifest.get("superseded_v5_capture_authorized") is not False or \
                superseded_v5.get("schema") != \
                "billiardgl.phase4.final_ownership_targeted11_ready.v5" or \
                superseded_v5.get("gpu_capture_authorized") is not False or \
                superseded_v5.get("authorization_issued") is not False:
            errors.append("superseded_v5_readiness_invalid")
    except (KeyError, OSError, ValueError, TypeError):
        errors.append("superseded_v5_readiness_invalid")
    try:
        superseded_v6_path = Path(manifest["superseded_v6_ready_manifest_path"])
        superseded_v6 = json.loads(superseded_v6_path.read_text())
        if sha(superseded_v6_path) != SUPERSEDED_V6_READY_SHA256 or \
                manifest.get("superseded_v6_ready_manifest_sha256") != \
                SUPERSEDED_V6_READY_SHA256 or \
                manifest.get("superseded_v6_capture_authorized") is not False or \
                superseded_v6.get("schema") != \
                "billiardgl.phase4.final_ownership_targeted11_ready.v6" or \
                superseded_v6.get("gpu_capture_authorized") is not False or \
                superseded_v6.get("authorization_issued") is not False:
            errors.append("superseded_v6_readiness_invalid")
    except (KeyError, OSError, ValueError, TypeError):
        errors.append("superseded_v6_readiness_invalid")
    try:
        failed_v7_path = Path(manifest["failed_v7_ready_manifest_path"])
        failed_v7 = json.loads(failed_v7_path.read_text())
        failed_receipt_path = Path(manifest["failed_v7_dispatch_receipt_path"])
        failed_receipt = json.loads(failed_receipt_path.read_text())
        if sha(failed_v7_path) != FAILED_V7_READY_SHA256 or \
                manifest.get("failed_v7_ready_manifest_sha256") != \
                FAILED_V7_READY_SHA256 or \
                sha(failed_receipt_path) != FAILED_V7_DISPATCH_SHA256 or \
                manifest.get("failed_v7_dispatch_receipt_sha256") != \
                FAILED_V7_DISPATCH_SHA256 or \
                failed_v7.get("schema") != \
                "billiardgl.phase4.final_ownership_targeted11_ready.v7" or \
                failed_receipt.get("status") != \
                "CAPTURE_FAILED_TELEMETRY_PRESERVED" or \
                [failed_receipt.get(key) for key in
                 ("completed_count", "failed_count", "unrun_count")] != \
                [0, 1, 10] or \
                failed_receipt.get("authorization_consumed_once") is not True or \
                manifest.get("failed_v7_authorization_consumed_not_reusable") is not True or \
                manifest.get("failed_v7_retry_authorized") is not False:
            errors.append("failed_v7_lineage_invalid")
    except (KeyError, OSError, ValueError, TypeError):
        errors.append("failed_v7_lineage_invalid")
    try:
        failed_v8_path = Path(manifest["failed_v8_ready_manifest_path"])
        failed_v8 = json.loads(failed_v8_path.read_text())
        failed_receipt_path = Path(manifest["failed_v8_dispatch_receipt_path"])
        failed_receipt = json.loads(failed_receipt_path.read_text())
        if sha(failed_v8_path) != FAILED_V8_READY_SHA256 or \
                manifest.get("failed_v8_ready_manifest_sha256") != \
                FAILED_V8_READY_SHA256 or \
                sha(failed_receipt_path) != FAILED_V8_DISPATCH_SHA256 or \
                manifest.get("failed_v8_dispatch_receipt_sha256") != \
                FAILED_V8_DISPATCH_SHA256 or \
                failed_v8.get("schema") != \
                "billiardgl.phase4.final_ownership_targeted11_ready.v8" or \
                failed_receipt.get("status") != \
                "CAPTURE_FAILED_TELEMETRY_PRESERVED" or \
                [failed_receipt.get(key) for key in
                 ("completed_count", "failed_count", "unrun_count")] != \
                [1, 1, 9] or \
                failed_receipt.get("authorization_consumed_once") is not True or \
                manifest.get("failed_v8_authorization_consumed_not_reusable") is not True or \
                manifest.get("failed_v8_retry_authorized") is not False:
            errors.append("failed_v8_lineage_invalid")
    except (KeyError, OSError, ValueError, TypeError):
        errors.append("failed_v8_lineage_invalid")
    try:
        failed_v9_path = Path(manifest["failed_v9_ready_manifest_path"])
        failed_v9 = json.loads(failed_v9_path.read_text())
        failed_receipt_path = Path(manifest["failed_v9_dispatch_receipt_path"])
        failed_receipt = json.loads(failed_receipt_path.read_text())
        if sha(failed_v9_path) != FAILED_V9_READY_SHA256 or \
                manifest.get("failed_v9_ready_manifest_sha256") != \
                FAILED_V9_READY_SHA256 or \
                sha(failed_receipt_path) != FAILED_V9_DISPATCH_SHA256 or \
                manifest.get("failed_v9_dispatch_receipt_sha256") != \
                FAILED_V9_DISPATCH_SHA256 or \
                failed_v9.get("schema") != \
                "billiardgl.phase4.final_ownership_targeted11_ready.v9" or \
                failed_receipt.get("status") != \
                "CAPTURE_FAILED_TELEMETRY_PRESERVED" or \
                [failed_receipt.get(key) for key in
                 ("completed_count", "failed_count", "unrun_count")] != \
                [0, 1, 10] or \
                failed_receipt.get("authorization_consumed_once") is not True or \
                manifest.get("failed_v9_authorization_consumed_not_reusable") is not True or \
                manifest.get("failed_v9_retry_authorized") is not False:
            errors.append("failed_v9_lineage_invalid")
    except (KeyError, OSError, ValueError, TypeError):
        errors.append("failed_v9_lineage_invalid")
    try:
        failed_v10_path = Path(manifest["failed_v10_ready_manifest_path"])
        failed_v10 = json.loads(failed_v10_path.read_text())
        failed_receipt_path = Path(manifest["failed_v10_dispatch_receipt_path"])
        failed_receipt = json.loads(failed_receipt_path.read_text())
        if sha(failed_v10_path) != FAILED_V10_READY_SHA256 or \
                manifest.get("failed_v10_ready_manifest_sha256") != \
                FAILED_V10_READY_SHA256 or \
                sha(failed_receipt_path) != FAILED_V10_DISPATCH_SHA256 or \
                manifest.get("failed_v10_dispatch_receipt_sha256") != \
                FAILED_V10_DISPATCH_SHA256 or \
                failed_v10.get("schema") != \
                "billiardgl.phase4.final_ownership_targeted11_ready.v10" or \
                failed_receipt.get("status") != \
                "CAPTURE_FAILED_TELEMETRY_PRESERVED" or \
                [failed_receipt.get(key) for key in
                 ("completed_count", "failed_count", "unrun_count")] != \
                [0, 1, 10] or \
                failed_receipt.get("authorization_consumed_once") is not True or \
                manifest.get("failed_v10_authorization_consumed_not_reusable") is not True or \
                manifest.get("failed_v10_retry_authorized") is not False:
            errors.append("failed_v10_lineage_invalid")
    except (KeyError, OSError, ValueError, TypeError):
        errors.append("failed_v10_lineage_invalid")

    captures = manifest.get("captures", [])
    if not isinstance(captures, list) or len(captures) != 11 or \
            manifest.get("capture_count") != 11 or manifest.get("not_run_count") != 11 or \
            manifest.get("actual_gpu_capture_count") != 0:
        errors.append("targeted11_inventory_invalid")
        captures = []
    if captures and any(row.get("machine_status") != "NOT_RUN" or
                        row.get("ta_status") != "NOT_RUN" or
                        row.get("actual_gpu_capture") is not False or
                        row.get("integrated_final_ownership_evidence") is not False or
                        row.get("gpu_capture_authorized") is not False or
                        row.get("formal68_authorized") is not False
                        for row in captures):
        errors.append("capture_status_or_claim_not_false")
    if manifest.get("gpu_capture_authorized") is not False or \
            manifest.get("formal68_authorized") is not False or \
            manifest.get("technical_art_review_authorized") is not False or \
            manifest.get("fresh_targeted_readiness_is_not_capture_authorization") is not True or \
            manifest.get("authorization_issued") is not False or \
            any(value is not False for value in manifest.get("evidence_claims", {}).values()) or \
            len(manifest.get("evidence_claims", {})) != 8:
        errors.append("authorization_or_evidence_claim_present")
    expected_dispatcher = ROOT / "scripts/run_phase4_final_ownership_targeted_capture.py"
    expected_dispatch_validator = ROOT / \
        "scripts/validate_phase4_final_ownership_capture_dispatch.py"
    if manifest.get("capture_dispatcher_path") != str(expected_dispatcher) or \
            manifest.get("capture_dispatch_validator_path") != \
            str(expected_dispatch_validator) or \
            not expected_dispatcher.is_file() or not expected_dispatch_validator.is_file():
        errors.append("capture_dispatch_tool_binding_invalid")
    if manifest.get("release_build") is not True or \
            manifest.get("capture_runner_built_not_executed") is not True or \
            manifest.get("results_file_exists") is not False or \
            manifest.get("targeted_output_root_initial_entry_count") != 0:
        errors.append("build_or_not_run_contract_invalid")

    try:
        output_root = Path(manifest["targeted_output_root"])
        expected_results = output_root / "results.jsonl"
        if not output_root.is_dir() or any(output_root.iterdir()) or \
                Path(manifest["results_path"]) != expected_results or expected_results.exists():
            errors.append("fresh_targeted_output_root_not_empty")
    except (KeyError, OSError, TypeError):
        errors.append("fresh_targeted_output_root_invalid")

    plan = Path(manifest.get("plan_path", ""))
    inventory = expected_plan_inventory(plan, errors)
    if manifest.get("plan_sha256") != CANONICAL_PLAN_SHA256:
        errors.append("canonical_plan_sha_not_pinned")
    if captures:
        selected = [{key: row.get(key) for key in expected}
                    for row, expected in zip(captures, inventory)] if inventory else []
        if selected != inventory:
            errors.append("capture_inventory_does_not_match_plan")
        ordinals = [row.get("ordinal_one_based") for row in captures]
        capture_ids = [row.get("capture_id") for row in captures]
        if ordinals != list(range(1, 12)) or len(set(capture_ids)) != 11:
            errors.append("capture_ordinal_or_id_not_unique")

    for path_key, sha_key in (
        ("runner_source_path", "runner_source_sha256"),
        ("runner_path", "runner_sha256"),
        ("billiards_executable_path", "billiards_executable_sha256"),
        ("plan_path", "plan_sha256"),
    ):
        try:
            bound_path = Path(manifest[path_key])
            if not bound_path.is_file() or sha(bound_path) != manifest[sha_key]:
                errors.append(f"binding_drift:{path_key}")
        except (KeyError, OSError, TypeError):
            errors.append(f"binding_invalid:{path_key}")

    actual_artifacts = {
        "billiards": manifest.get("billiards_executable_sha256"),
        "runner": manifest.get("runner_sha256"),
        "runner_source": manifest.get("runner_source_sha256"),
    }
    if manifest.get("required_expected_release_artifact_sha256") != actual_artifacts:
        errors.append("required_expected_release_artifact_sha_mismatch")

    source_manifest = load_bound_json(
        manifest, "source_binding_manifest_path", "source_binding_manifest_sha256", errors)
    if source_manifest is not None:
        expected_rows = [{"path": relative, "sha256": sha(ROOT / relative)}
                         for relative in ALL_BOUND_SOURCES]
        expected_impl = aggregate(IMPLEMENTATION_SOURCES)
        expected_all = aggregate(ALL_BOUND_SOURCES)
        if source_manifest.get("schema") != "billiardgl.phase4.final_ownership_source_bindings.v1" or \
                source_manifest.get("algorithm") != \
                "sha256(path_utf8 + NUL + file_sha256_ascii + LF), listed order" or \
                source_manifest.get("gate_bindings") != GATE_BINDINGS or \
                source_manifest.get("sources") != expected_rows or \
                source_manifest.get("implementation_source_aggregate_sha256") != expected_impl or \
                source_manifest.get("all_modified_source_test_tooling_aggregate_sha256") != expected_all or \
                manifest.get("implementation_source_aggregate_sha256") != expected_impl or \
                manifest.get("all_source_test_tooling_aggregate_sha256") != expected_all:
            errors.append("source_binding_semantics_invalid")

    cpu = load_bound_json(manifest, "cpu_fixture_receipt_path",
                          "cpu_fixture_receipt_sha256", errors)
    if cpu is not None:
        cpu_ok = cpu.get("schema") == "billiardgl.phase4.final_ownership_cpu_fixture_receipt.v1" and \
            cpu.get("status") == "PASS" and cpu.get("gpu_or_cocoa_invoked") is False and \
            cpu.get("fixture_count") == 26 and cpu.get("fixture_status") == "26/26_PASS" and \
            bool(cpu.get("telemetry_test_output")) and \
            "26/26 PASS" in "\n".join(cpu.get("semantic_test_output", []))
        for binary_key, binary_sha_key in (
                ("telemetry_test_binary", "telemetry_test_binary_sha256"),
                ("semantic_test_binary", "semantic_test_binary_sha256"),
                ("visual_test_binary", "visual_test_binary_sha256")):
            try:
                binary = Path(cpu[binary_key])
                cpu_ok = cpu_ok and binary.is_file() and sha(binary) == cpu[binary_sha_key]
            except (KeyError, OSError, TypeError):
                cpu_ok = False
        cpu_ok = cpu_ok and cpu.get("visual_fixture_status") == \
            "ADAPTIVE_FINAL_LDR_SUBLAYERS_CROSS_TRACK_AND_CAP_CASES_PASS" and \
            "trajectory overlay renderer tests passed" in \
            cpu.get("visual_test_output", "") and \
            cpu.get("machine_contract_fixture_status") == \
            "EXACT_TWO_ORDERED_ACTUAL_FREEZE_PREDICTED_AND_NEGATIVES_PASS" and \
            cpu.get("machine_contract_predicate_count") == 41 and \
            cpu.get("machine_contract_order_fixture_count") == 7 and \
            cpu.get("machine_contract_validator_output", {}).get("status") == "PASS" and \
            "OK" in "\n".join(cpu.get("machine_contract_unittest_output", []))
        if not cpu_ok:
            errors.append("cpu_fixture_receipt_semantics_invalid")

    frozen = load_bound_json(manifest, "frozen_replay_receipt_path",
                             "frozen_replay_receipt_sha256", errors)
    if frozen is not None:
        frozen_ok = frozen.get("schema") == \
            "billiardgl.phase4.frozen_ordinal1_read_only_replay_receipt.v1" and \
            frozen.get("status") == "PASS" and frozen.get("gpu_or_cocoa_invoked") is False and \
            frozen.get("inputs_read_only") is True and frozen.get("source_probe_count") == 95 and \
            frozen.get("ordered_source_probe_digest") == ORDERED_PROBE_DIGEST and \
            frozen.get("frozen_counts_bound_not_redistributed") == \
            {"source": 95, "visible": 46, "hidden": 49} and \
            frozen.get("integrated_ownership_claimed") is False and \
            frozen.get("analytic_evidence_label") == "ANALYTIC_NON_FINAL" and \
            "PASS" in frozen.get("test_output", "")
        for file_key, file_sha_key in (
                ("failure_row_path", "failure_row_sha256"),
                ("test_binary", "test_binary_sha256")):
            try:
                bound = Path(frozen[file_key])
                frozen_ok = frozen_ok and bound.is_file() and sha(bound) == frozen[file_sha_key]
            except (KeyError, OSError, TypeError):
                frozen_ok = False
        try:
            rows = Path(frozen["failure_row_path"]).read_text().splitlines()
            row = json.loads(rows[0]) if len(rows) == 1 else {}
            ids = row.get("ordered_source_probe_ids", [])
            frozen_ok = frozen_ok and row.get("schema") == \
                "billiardgl.phase4.final_ownership_capture_telemetry.v1" and \
                row.get("machine_status") == "FAILED" and \
                row.get("integrated_final_ownership") is False and \
                row.get("source_probe_count") == 95 and \
                row.get("ordered_source_probe_digest") == ORDERED_PROBE_DIGEST and \
                len(ids) == len(set(ids)) == 95 and \
                len(row.get("hidden_probe_id_track_role_reason_coordinates", [])) == 95 and \
                len(row.get("ownership_candidate_records", [])) == 95 and \
                len(row.get("exclusive_arbitration_records", [])) == 95
        except (KeyError, OSError, ValueError, TypeError):
            frozen_ok = False
        if not frozen_ok:
            errors.append("frozen_replay_receipt_semantics_invalid")

    frozen_v7 = load_bound_json(manifest,
        "frozen_v7_association_counterfactual_receipt_path",
        "frozen_v7_association_counterfactual_receipt_sha256", errors)
    if frozen_v7 is not None:
        frozen_v7_ok = frozen_v7.get("schema") == \
            "billiardgl.phase4.final_ownership_v7_association_counterfactual_receipt.v1" and \
            frozen_v7.get("status") == \
            "ANALYTIC_NON_FINAL_NOT_A_MACHINE_PASS" and \
            frozen_v7.get("gpu_or_cocoa_invoked") is False and \
            frozen_v7.get("corrected_capture_claimed") is False and \
            frozen_v7.get("lost_cross_partition_boundary_count") == 6 and \
            frozen_v7.get("selected_outer_silhouette_below_4_count") == 3 and \
            frozen_v7.get("analytic_reassociation_visible_hidden") == [92, 3] and \
            frozen_v7.get("analytic_reassociation_predicted_visible_hidden") == [73, 3] and \
            frozen_v7.get("validator_output", {}).get("status") == "PASS"
        if not frozen_v7_ok:
            errors.append("frozen_v7_counterfactual_semantics_invalid")

    frozen_v8 = load_bound_json(manifest,
        "frozen_v8_visual_failure_receipt_path",
        "frozen_v8_visual_failure_receipt_sha256", errors)
    if frozen_v8 is not None:
        frozen_v8_ok = frozen_v8.get("schema") == \
            "billiardgl.phase4.final_ownership_v8_visual_failure_replay_receipt.v1" and \
            frozen_v8.get("status") == "FROZEN_FAILURE_NOT_A_MACHINE_PASS" and \
            frozen_v8.get("gpu_or_cocoa_invoked") is False and \
            frozen_v8.get("frozen_failure_evidence_sha256") == \
            GATE_BINDINGS["v8_visual_failure_evidence_sha256"] and \
            frozen_v8.get("source_probe_count") == 83 and \
            frozen_v8.get("ordered_source_probe_digest") == \
            "a3314c4ad04a72bae1718b290129604067954c16685d1d7573726caeb6d5b3f9" and \
            frozen_v8.get("authoritative_visible_hidden") == [72, 11] and \
            frozen_v8.get("machine_pass") is False and \
            frozen_v8.get("analytic_81_2_counterfactual_claimed_as_capture") is False and \
            frozen_v8.get("validator_output", {}).get("status") == "PASS"
        if not frozen_v8_ok:
            errors.append("frozen_v8_visual_failure_semantics_invalid")

    frozen_v9 = load_bound_json(manifest,
        "frozen_v9_compositing_failure_receipt_path",
        "frozen_v9_compositing_failure_receipt_sha256", errors)
    if frozen_v9 is not None:
        frozen_v9_ok = frozen_v9.get("schema") == \
            "billiardgl.phase4.final_ownership_v9_ordinal1_offline_diagnostic.v1" and \
            frozen_v9.get("status") == \
            "COMPOSITING_STYLE_LAYER_ALIAS_AND_FIXED_RIM_CONTRAST_DEFECT_ISOLATED_ALGORITHM_CLEAN_NO_CAPTURE_AUTHORIZED" and \
            frozen_v9.get("gpu_cocoa_opengl_invoked") is False and \
            frozen_v9.get("capture_retry_authorized") is False and \
            frozen_v9.get("authority_separation", {}).get(
                "corrected_integrated", {}).get("visible") == 66 and \
            frozen_v9.get("authority_separation", {}).get(
                "corrected_integrated", {}).get("hidden") == 29 and \
            frozen_v9.get("contrast_failure_partition", {}).get(
                "area_or_association_failure_count") == 0
        if not frozen_v9_ok:
            errors.append("frozen_v9_compositing_failure_semantics_invalid")

    frozen_v9_superseded = load_bound_json(manifest,
        "frozen_v9_superseded_validator_lineage_receipt_path",
        "frozen_v9_superseded_validator_lineage_receipt_sha256", errors)
    if frozen_v9_superseded is not None:
        expected_source_drift = [
            "binding_hash_drift:src/Billiards/training/trajectory_overlay_renderer.cpp",
            "binding_hash_drift:src/Billiards/training/trajectory_overlay_renderer.h",
        ]
        frozen_v9_superseded_ok = frozen_v9_superseded.get("schema") == \
            "billiardgl.phase4.final_ownership_v9_superseded_validator_lineage.v1" and \
            frozen_v9_superseded.get("status") == \
            "EXPECTED_SUPERSEDED_SOURCE_DRIFT_NOT_A_MACHINE_PASS" and \
            frozen_v9_superseded.get("gpu_or_cocoa_invoked") is False and \
            frozen_v9_superseded.get("capture_retry_authorized") is False and \
            frozen_v9_superseded.get("frozen_v9_failure_evidence_sha256") == \
            GATE_BINDINGS["v9_compositing_failure_evidence_sha256"] and \
            frozen_v9_superseded.get("successor_compositing_style_gate_sha256") == \
            GATE_BINDINGS["v9_compositing_style_correction_gate_sha256"] and \
            frozen_v9_superseded.get("superseded_validator_returncode") == 1 and \
            frozen_v9_superseded.get("expected_source_drift_errors") == \
            expected_source_drift and \
            frozen_v9_superseded.get("superseded_validator_output", {}).get(
                "status") == "FAIL" and \
            frozen_v9_superseded.get("superseded_validator_output", {}).get(
                "errors") == expected_source_drift and \
            frozen_v9_superseded.get(
                "successor_implementation_source_aggregate_sha256") == \
            manifest.get("implementation_source_aggregate_sha256") and \
            frozen_v9_superseded.get(
                "successor_all_source_test_tooling_aggregate_sha256") == \
            manifest.get("all_source_test_tooling_aggregate_sha256")
        if not frozen_v9_superseded_ok:
            errors.append("frozen_v9_superseded_lineage_semantics_invalid")

    frozen_v10 = load_bound_json(manifest,
        "frozen_v10_machine_contract_failure_receipt_path",
        "frozen_v10_machine_contract_failure_receipt_sha256", errors)
    if frozen_v10 is not None:
        root_cause = frozen_v10.get("root_cause", {})
        authority = frozen_v10.get("authority_separation", {}).get(
            "corrected_integrated", {})
        predicate = frozen_v10.get("static_machine_predicate_evidence", {})
        frozen_v10_ok = frozen_v10.get("schema") == \
            "billiardgl.phase4.final_ownership_v10_ordinal1_offline_diagnostic.v1" and \
            frozen_v10.get("status") == \
            "STALE_EXACT_ONE_DRAW_MACHINE_CONTRACT_SUFFICIENT_FALSE_CONJUNCT_ISOLATED_NO_RETRY_AUTHORIZED" and \
            frozen_v10.get("gpu_cocoa_opengl_invoked") is False and \
            frozen_v10.get("capture_retry_authorized") is False and \
            authority.get("visible") == 89 and authority.get("hidden") == 6 and \
            frozen_v10.get("track_counts", {}).get("predicted", {}).get("visible") == 70 and \
            frozen_v10.get("track_counts", {}).get("predicted", {}).get("hidden") == 6 and \
            frozen_v10.get("track_counts", {}).get("actual", {}).get("visible") == 19 and \
            frozen_v10.get("track_counts", {}).get("actual", {}).get("hidden") == 0 and \
            root_cause.get("primary") == "STALE_EXACT_ONE_DRAW_MACHINE_ACCEPTANCE_CONTRACT" and \
            root_cause.get("sufficient_false_conjunct_isolated") is True and \
            root_cause.get("exclusivity_of_this_single_failure_proven") is False and \
            root_cause.get("other_runtime_predicates_individually_serialized") is False and \
            predicate.get("frozen_runner_required_exact_draw_submissions") == 1 and \
            predicate.get("package_reported_draw_submissions_by_definition") == 2 and \
            predicate.get("draw_predicate_evaluates") is False
        if not frozen_v10_ok:
            errors.append("frozen_v10_machine_contract_failure_semantics_invalid")

    frozen_v10_superseded = load_bound_json(manifest,
        "frozen_v10_superseded_validator_lineage_receipt_path",
        "frozen_v10_superseded_validator_lineage_receipt_sha256", errors)
    if frozen_v10_superseded is not None:
        expected_source_drift = [
            "binding_hash_drift:src/Billiards/render/modern/modern_renderer.cpp",
        ]
        frozen_v10_superseded_ok = frozen_v10_superseded.get("schema") == \
            "billiardgl.phase4.final_ownership_v10_superseded_validator_lineage.v1" and \
            frozen_v10_superseded.get("status") == \
            "EXPECTED_SUPERSEDED_MACHINE_CONTRACT_SOURCE_DRIFT_NOT_A_MACHINE_PASS" and \
            frozen_v10_superseded.get("gpu_or_cocoa_invoked") is False and \
            frozen_v10_superseded.get("capture_retry_authorized") is False and \
            frozen_v10_superseded.get("frozen_v10_failure_evidence_sha256") == \
            GATE_BINDINGS["v10_machine_contract_failure_evidence_sha256"] and \
            frozen_v10_superseded.get("successor_machine_contract_gate_sha256") == \
            GATE_BINDINGS["v10_machine_contract_correction_gate_sha256"] and \
            frozen_v10_superseded.get("superseded_validator_returncode") == 1 and \
            frozen_v10_superseded.get("expected_source_drift_errors") == \
            expected_source_drift and \
            frozen_v10_superseded.get("superseded_validator_output", {}).get(
                "status") == "FAIL" and \
            frozen_v10_superseded.get("superseded_validator_output", {}).get(
                "errors") == expected_source_drift and \
            frozen_v10_superseded.get(
                "successor_implementation_source_aggregate_sha256") == \
            manifest.get("implementation_source_aggregate_sha256") and \
            frozen_v10_superseded.get(
                "successor_all_source_test_tooling_aggregate_sha256") == \
            manifest.get("all_source_test_tooling_aggregate_sha256")
        if not frozen_v10_superseded_ok:
            errors.append("frozen_v10_superseded_lineage_semantics_invalid")

    static = load_bound_json(manifest, "static_conformance_receipt_path",
                             "static_conformance_receipt_sha256", errors)
    if static is not None:
        source_hashes = static.get("source_sha256", {})
        static_ok = static.get("schema") == \
            "billiardgl.phase4.final_ownership_static_conformance.v1" and \
            static.get("status") == "PASS" and static.get("gpu_or_cocoa_invoked") is False and \
            static.get("capture_authorized") is False and static.get("errors") == [] and \
            static.get("checks") == STATIC_CHECKS and \
            bool(source_hashes) and all((ROOT / relative).is_file() and
                sha(ROOT / relative) == digest for relative, digest in source_hashes.items()) and \
            "OK" in "\n".join(static.get("unittest_output", []))
        bound_inputs = static.get("bound_inputs", {})
        static_ok = static_ok and bool(bound_inputs) and all(
            (ROOT / relative).is_file() and sha(ROOT / relative) == digest
            for relative, digest in bound_inputs.items())
        if not static_ok:
            errors.append("static_conformance_receipt_semantics_invalid")

    try:
        runner_source = Path(manifest["runner_source_path"]).read_text()
        for token in ("enableFinalOwnershipInstrumentation = true",
                      "captureIntegratedFinalOwnership",
                      "preservePhase4TelemetryThenEnforceMachineGate",
                      "componentAwareBoundaryRecords = ownership.boundaryTelemetry",
                      "legacyPpmDiagnosticOnly = true",
                      "machinePredicateLedgerRequired = true",
                      "metrics.trajectoryOverlayDrawCalls == 2",
                      "BILLIARDGL_PHASE4_FINAL_OWNERSHIP_GPU_AUTH_RECEIPT"):
            if token not in runner_source:
                errors.append(f"runner_capability_missing:{token}")
    except (KeyError, OSError, TypeError):
        errors.append("runner_source_unreadable")
    if manifest.get("immutable_invariants") != IMMUTABLE_INVARIANTS:
        errors.append("immutable_invariants_drift")
    return errors


def main() -> None:
    parser = argparse.ArgumentParser()
    parser.add_argument("manifest", type=Path)
    args = parser.parse_args()
    errors = validate_manifest(args.manifest)
    result = {"status": "PASS" if not errors else "FAIL",
              "gpu_or_cocoa_invoked": False, "errors": errors}
    print(json.dumps(result, indent=2, sort_keys=True))
    if errors:
        raise SystemExit(1)


if __name__ == "__main__":
    main()
