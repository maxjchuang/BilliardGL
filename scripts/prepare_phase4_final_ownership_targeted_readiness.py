#!/usr/bin/env python3
"""Prepare targeted11 readiness after CPU gates; never invokes a GPU runner."""

from __future__ import annotations

import argparse
import hashlib
import json
from pathlib import Path
import shutil
import subprocess

ROOT = Path(__file__).resolve().parents[1]
OLD_READY = ROOT / "docs/art/phase4-training-replay-overlay-v4-ready-v3/targeted-preflight-ready.json"
FROZEN_PLAN = ROOT / "docs/art/phase4-training-replay-overlay-v4-targeted-gpu-v1/phase4-overlay-v4-plan.tsv"
D1 = ROOT / "docs/art/phase4-overlay-v4-ordinal1-offline-diagnostic-v1/d1-frozen-cpu-package-probe-dump-v1.json"
PPM = ROOT / "docs/art/phase4-training-replay-overlay-v4-targeted-gpu-v1/camera_player_low_1080.ppm"
REPLAY = ROOT / "docs/art/phase4-training-replay-overlay-fixtures-v3/launch_stop_shot_center_v1.overlay.replay.json"
FAILURE = ROOT / "docs/art/phase4-training-replay-overlay-v4-targeted-failure-receipt-v1.json"
CANONICAL_PLAN_SHA256 = "3b969b34e52fe2e01e31e34b148c4ce7c211682db65b3c609a43274366c8717d"
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
    "frozen_failure_sha256": FAILURE,
    "frozen_ppm_sha256": PPM,
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


def write_json(path: Path, value: object) -> None:
    path.write_text(json.dumps(value, indent=2, sort_keys=True) + "\n")


def run(command: list[str]) -> str:
    result = subprocess.run(command, cwd=ROOT, text=True,
                            stdout=subprocess.PIPE,
                            stderr=subprocess.STDOUT, check=True)
    return result.stdout


def run_expected_failure(command: list[str]) -> tuple[int, str]:
    result = subprocess.run(command, cwd=ROOT, text=True,
                            stdout=subprocess.PIPE,
                            stderr=subprocess.STDOUT, check=False)
    return result.returncode, result.stdout


def require_sha256(value: str, label: str) -> str:
    if len(value) != 64 or any(character not in "0123456789abcdef" for character in value):
        raise RuntimeError(f"{label} must be a lowercase SHA-256")
    return value


def main() -> None:
    parser = argparse.ArgumentParser()
    parser.add_argument("--build-dir", type=Path, required=True)
    parser.add_argument("--cpu-build-dir", type=Path, required=True)
    parser.add_argument("--ready-dir", type=Path, required=True)
    parser.add_argument("--output-root", type=Path, required=True)
    parser.add_argument("--expected-billiards-sha256", required=True)
    parser.add_argument("--expected-runner-sha256", required=True)
    parser.add_argument("--expected-runner-source-sha256", required=True)
    parser.add_argument("--lineage-ready-manifest", type=Path, required=True)
    parser.add_argument("--superseded-v2-ready-manifest", type=Path, required=True)
    parser.add_argument("--failed-v3-ready-manifest", type=Path, required=True)
    parser.add_argument("--failed-v4-ready-manifest", type=Path, required=True)
    parser.add_argument("--superseded-v5-ready-manifest", type=Path, required=True)
    parser.add_argument("--superseded-v6-ready-manifest", type=Path, required=True)
    parser.add_argument("--failed-v7-ready-manifest", type=Path, required=True)
    parser.add_argument("--failed-v8-ready-manifest", type=Path, required=True)
    parser.add_argument("--failed-v9-ready-manifest", type=Path, required=True)
    parser.add_argument("--failed-v10-ready-manifest", type=Path, required=True)
    args = parser.parse_args()
    build = args.build_dir.resolve()
    cpu_build = args.cpu_build_dir.resolve()
    ready = args.ready_dir.resolve()
    output_root = args.output_root.resolve()
    lineage = args.lineage_ready_manifest.resolve()
    superseded_v2 = args.superseded_v2_ready_manifest.resolve()
    failed_v3_ready = args.failed_v3_ready_manifest.resolve()
    failed_v4_ready = args.failed_v4_ready_manifest.resolve()
    superseded_v5_ready = args.superseded_v5_ready_manifest.resolve()
    superseded_v6_ready = args.superseded_v6_ready_manifest.resolve()
    failed_v7_ready = args.failed_v7_ready_manifest.resolve()
    failed_v8_ready = args.failed_v8_ready_manifest.resolve()
    failed_v9_ready = args.failed_v9_ready_manifest.resolve()
    failed_v10_ready = args.failed_v10_ready_manifest.resolve()
    if ready.exists() or output_root.exists():
        raise RuntimeError("fresh readiness and output roots must not exist")
    if not lineage.is_file() or sha(lineage) != LINEAGE_V1_SHA256:
        raise RuntimeError("frozen v1 readiness lineage binding drift")
    lineage_value = json.loads(lineage.read_text())
    if lineage_value.get("schema") != \
            "billiardgl.phase4.final_ownership_targeted11_ready.v1" or \
            lineage_value.get("status") != "READY_NOT_RUN_GPU_NOT_AUTHORIZED" or \
            lineage_value.get("not_run_count") != 11 or \
            lineage_value.get("gpu_capture_authorized") is not False:
        raise RuntimeError("frozen v1 readiness lineage semantics invalid")
    if not superseded_v2.is_file() or sha(superseded_v2) != SUPERSEDED_V2_SHA256:
        raise RuntimeError("superseded v2 readiness binding drift")
    superseded_value = json.loads(superseded_v2.read_text())
    if superseded_value.get("schema") != \
            "billiardgl.phase4.final_ownership_targeted11_ready.v2" or \
            superseded_value.get("gpu_capture_authorized") is not False or \
            superseded_value.get("authorization_issued") is not False:
        raise RuntimeError("superseded v2 readiness authorization boundary invalid")
    if not failed_v3_ready.is_file() or sha(failed_v3_ready) != FAILED_V3_READY_SHA256:
        raise RuntimeError("failed v3 readiness lineage binding drift")
    failed_v3_value = json.loads(failed_v3_ready.read_text())
    if failed_v3_value.get("schema") != \
            "billiardgl.phase4.final_ownership_targeted11_ready.v3" or \
            failed_v3_value.get("capture_count") != 11 or \
            failed_v3_value.get("authorization_issued") is not False:
        raise RuntimeError("failed v3 readiness lineage semantics invalid")
    if not failed_v4_ready.is_file() or sha(failed_v4_ready) != FAILED_V4_READY_SHA256:
        raise RuntimeError("failed v4 readiness lineage binding drift")
    failed_v4_value = json.loads(failed_v4_ready.read_text())
    if failed_v4_value.get("schema") != \
            "billiardgl.phase4.final_ownership_targeted11_ready.v4" or \
            failed_v4_value.get("capture_count") != 11 or \
            failed_v4_value.get("authorization_issued") is not False:
        raise RuntimeError("failed v4 readiness lineage semantics invalid")
    if not superseded_v5_ready.is_file() or \
            sha(superseded_v5_ready) != SUPERSEDED_V5_READY_SHA256:
        raise RuntimeError("superseded v5 readiness binding drift")
    superseded_v5_value = json.loads(superseded_v5_ready.read_text())
    if superseded_v5_value.get("schema") != \
            "billiardgl.phase4.final_ownership_targeted11_ready.v5" or \
            superseded_v5_value.get("gpu_capture_authorized") is not False or \
            superseded_v5_value.get("authorization_issued") is not False:
        raise RuntimeError("superseded v5 readiness authorization boundary invalid")
    if not superseded_v6_ready.is_file() or \
            sha(superseded_v6_ready) != SUPERSEDED_V6_READY_SHA256:
        raise RuntimeError("superseded v6 readiness binding drift")
    superseded_v6_value = json.loads(superseded_v6_ready.read_text())
    if superseded_v6_value.get("schema") != \
            "billiardgl.phase4.final_ownership_targeted11_ready.v6" or \
            superseded_v6_value.get("gpu_capture_authorized") is not False or \
            superseded_v6_value.get("authorization_issued") is not False:
        raise RuntimeError("superseded v6 readiness authorization boundary invalid")
    if not failed_v7_ready.is_file() or \
            sha(failed_v7_ready) != FAILED_V7_READY_SHA256:
        raise RuntimeError("failed v7 readiness lineage binding drift")
    failed_v7_value = json.loads(failed_v7_ready.read_text())
    if failed_v7_value.get("schema") != \
            "billiardgl.phase4.final_ownership_targeted11_ready.v7" or \
            failed_v7_value.get("capture_count") != 11 or \
            failed_v7_value.get("authorization_issued") is not False:
        raise RuntimeError("failed v7 readiness lineage semantics invalid")
    if not failed_v8_ready.is_file() or \
            sha(failed_v8_ready) != FAILED_V8_READY_SHA256:
        raise RuntimeError("failed v8 readiness lineage binding drift")
    failed_v8_value = json.loads(failed_v8_ready.read_text())
    if failed_v8_value.get("schema") != \
            "billiardgl.phase4.final_ownership_targeted11_ready.v8" or \
            failed_v8_value.get("capture_count") != 11 or \
            failed_v8_value.get("authorization_issued") is not False:
        raise RuntimeError("failed v8 readiness lineage semantics invalid")
    if not failed_v9_ready.is_file() or \
            sha(failed_v9_ready) != FAILED_V9_READY_SHA256:
        raise RuntimeError("failed v9 readiness lineage binding drift")
    failed_v9_value = json.loads(failed_v9_ready.read_text())
    if failed_v9_value.get("schema") != \
            "billiardgl.phase4.final_ownership_targeted11_ready.v9" or \
            failed_v9_value.get("capture_count") != 11 or \
            failed_v9_value.get("authorization_issued") is not False:
        raise RuntimeError("failed v9 readiness lineage semantics invalid")
    if not failed_v10_ready.is_file() or \
            sha(failed_v10_ready) != FAILED_V10_READY_SHA256:
        raise RuntimeError("failed v10 readiness lineage binding drift")
    failed_v10_value = json.loads(failed_v10_ready.read_text())
    if failed_v10_value.get("schema") != \
            "billiardgl.phase4.final_ownership_targeted11_ready.v10" or \
            failed_v10_value.get("capture_count") != 11 or \
            failed_v10_value.get("authorization_issued") is not False:
        raise RuntimeError("failed v10 readiness lineage semantics invalid")
    for key, path in GATE_FILES.items():
        if not path.is_file() or sha(path) != GATE_BINDINGS[key]:
            raise RuntimeError(f"pinned gate binding drift:{key}")
    if not FROZEN_PLAN.is_file() or sha(FROZEN_PLAN) != CANONICAL_PLAN_SHA256:
        raise RuntimeError("canonical targeted11 plan drift")
    runner = build / "BilliardsPhase4OverlayV4GpuCaptureRunner"
    runner_source = build / "generated/phase4_overlay_v4_gpu_capture_runner.mm"
    billiards = build / "Billiards"
    for path in (runner, runner_source, billiards):
        if not path.is_file():
            raise RuntimeError(f"fresh release artifact missing:{path}")
    expected_artifact_sha256 = {
        "billiards": require_sha256(args.expected_billiards_sha256,
                                     "expected Billiards SHA"),
        "runner": require_sha256(args.expected_runner_sha256,
                                  "expected runner SHA"),
        "runner_source": require_sha256(args.expected_runner_source_sha256,
                                         "expected runner source SHA"),
    }
    actual_artifact_sha256 = {
        "billiards": sha(billiards),
        "runner": sha(runner),
        "runner_source": sha(runner_source),
    }
    if actual_artifact_sha256 != expected_artifact_sha256:
        raise RuntimeError("fresh release artifact SHA does not match required expected SHA")
    cache = (build / "CMakeCache.txt").read_text()
    if "CMAKE_BUILD_TYPE:STRING=Release" not in cache or \
            "BILLIARDGL_PHASE4_FINAL_OWNERSHIP_RUNNER:BOOL=ON" not in cache:
        raise RuntimeError("fresh final-ownership Release build not proven")

    ready.mkdir(parents=True)
    output_root.mkdir(parents=True)
    plan = ready / "phase4-final-ownership-targeted11-plan.tsv"
    shutil.copyfile(FROZEN_PLAN, plan)
    if sha(plan) != CANONICAL_PLAN_SHA256 or len(plan.read_text().splitlines()) != 11:
        raise RuntimeError("targeted plan must contain exactly 11 cases")

    telemetry_test = cpu_build / "phase4_final_ownership_telemetry_tests"
    semantic_test = cpu_build / "phase4_final_semantic_ownership_tests"
    frozen_test = cpu_build / "phase4_frozen_ordinal1_failure_replay"
    visual_test = cpu_build / "BilliardsTrajectoryOverlayRendererTests"
    telemetry_output = run([str(telemetry_test)])
    semantic_output = run([str(semantic_test)])
    visual_output = run([str(visual_test)])
    if "26/26 PASS" not in semantic_output:
        raise RuntimeError("26/26 semantic fixtures did not pass")
    if "trajectory overlay renderer tests passed" not in visual_output:
        raise RuntimeError("visual style/color/area fixtures did not pass")
    static_output = run([
        "python3", "scripts/validate_phase4_final_ownership_implementation.py"])
    static_report = json.loads(static_output)
    if static_report["status"] != "PASS":
        raise RuntimeError("static conformance did not pass")
    unittest_output = run(["python3", "-m", "unittest",
        "tests.test_phase4_final_ownership_static_conformance"])
    frozen_v7_counterfactual_output = run(["python3",
        "scripts/validate_phase4_final_ownership_v7_ordinal1_offline_diagnostic.py"])
    frozen_v7_counterfactual_validation = json.loads(
        frozen_v7_counterfactual_output)
    if frozen_v7_counterfactual_validation.get("status") != "PASS":
        raise RuntimeError("frozen v7 association counterfactual invalid")
    frozen_v8_validation = json.loads(run(["python3",
        "scripts/validate_phase4_final_ownership_v8_ordinal2_offline_diagnostic.py"]))
    if frozen_v8_validation.get("status") != "PASS":
        raise RuntimeError("frozen v8 visual failure evidence invalid")
    frozen_v9_validator_returncode, frozen_v9_validator_output = \
        run_expected_failure(["python3",
            "scripts/validate_phase4_final_ownership_v9_ordinal1_offline_diagnostic.py"])
    frozen_v9_validation = json.loads(frozen_v9_validator_output)
    expected_v9_source_drift = [
        "binding_hash_drift:src/Billiards/training/trajectory_overlay_renderer.cpp",
        "binding_hash_drift:src/Billiards/training/trajectory_overlay_renderer.h",
    ]
    if frozen_v9_validator_returncode != 1 or \
            frozen_v9_validation.get("status") != "FAIL" or \
            frozen_v9_validation.get("gpu_cocoa_opengl_invoked") is not False or \
            frozen_v9_validation.get("capture_retry_authorized") is not False or \
            frozen_v9_validation.get("errors") != expected_v9_source_drift:
        raise RuntimeError(
            "frozen v9 validator must fail only for expected superseded source drift")
    frozen_v10_validator_returncode, frozen_v10_validator_output = \
        run_expected_failure(["python3",
            "scripts/validate_phase4_final_ownership_v10_ordinal1_offline_diagnostic.py"])
    frozen_v10_validation = json.loads(frozen_v10_validator_output)
    expected_v10_source_drift = [
        "binding_hash_drift:src/Billiards/render/modern/modern_renderer.cpp",
    ]
    if frozen_v10_validator_returncode != 1 or \
            frozen_v10_validation.get("status") != "FAIL" or \
            frozen_v10_validation.get("gpu_cocoa_opengl_invoked") is not False or \
            frozen_v10_validation.get("capture_retry_authorized") is not False or \
            frozen_v10_validation.get("errors") != expected_v10_source_drift:
        raise RuntimeError(
            "frozen v10 validator must fail only for expected superseded machine-contract source drift")
    machine_contract_validation = json.loads(run(["python3",
        "scripts/validate_phase4_v10_machine_contract_implementation.py"]))
    if machine_contract_validation.get("status") != "PASS" or \
            machine_contract_validation.get("predicate_count") != 41 or \
            machine_contract_validation.get("ordered_exact_two_fixture_count") != 7:
        raise RuntimeError("v10 machine-contract implementation validation failed")
    machine_contract_unittest_output = run(["python3", "-m", "unittest",
        "tests.test_phase4_v10_machine_contract_implementation"])

    frozen_row = ready / "frozen-ordinal1-read-only-failure-row.jsonl"
    frozen_output = run([str(frozen_test), str(D1), str(PPM), str(REPLAY),
                         str(FAILURE), str(frozen_row)])
    cpu_receipt = {
        "schema": "billiardgl.phase4.final_ownership_cpu_fixture_receipt.v1",
        "status": "PASS",
        "gpu_or_cocoa_invoked": False,
        "fixture_count": 26,
        "fixture_status": "26/26_PASS",
        "telemetry_test_binary": str(telemetry_test),
        "telemetry_test_binary_sha256": sha(telemetry_test),
        "semantic_test_binary": str(semantic_test),
        "semantic_test_binary_sha256": sha(semantic_test),
        "telemetry_test_output": telemetry_output.strip(),
        "semantic_test_output": semantic_output.strip().splitlines(),
        "visual_test_binary": str(visual_test),
        "visual_test_binary_sha256": sha(visual_test),
        "visual_test_output": visual_output.strip(),
        "visual_fixture_status":
            "ADAPTIVE_FINAL_LDR_SUBLAYERS_CROSS_TRACK_AND_CAP_CASES_PASS",
        "machine_contract_fixture_status":
            "EXACT_TWO_ORDERED_ACTUAL_FREEZE_PREDICTED_AND_NEGATIVES_PASS",
        "machine_contract_predicate_count": 41,
        "machine_contract_order_fixture_count": 7,
        "machine_contract_validator_output": machine_contract_validation,
        "machine_contract_unittest_output":
            machine_contract_unittest_output.strip().splitlines(),
    }
    cpu_receipt_path = ready / "cpu-fixture-receipt-v1.json"
    write_json(cpu_receipt_path, cpu_receipt)
    frozen_receipt = {
        "schema": "billiardgl.phase4.frozen_ordinal1_read_only_replay_receipt.v1",
        "status": "PASS",
        "gpu_or_cocoa_invoked": False,
        "inputs_read_only": True,
        "source_probe_count": 95,
        "ordered_source_probe_digest":
            "d533924980efc85aeb94e4f1d31831d93e2bfce744be3b66481d08c56d10289a",
        "frozen_counts_bound_not_redistributed": {"source": 95,
            "visible": 46, "hidden": 49},
        "integrated_ownership_claimed": False,
        "analytic_evidence_label": "ANALYTIC_NON_FINAL",
        "failure_row_path": str(frozen_row),
        "failure_row_sha256": sha(frozen_row),
        "test_binary": str(frozen_test),
        "test_binary_sha256": sha(frozen_test),
        "test_output": frozen_output.strip(),
    }
    frozen_receipt_path = ready / "frozen-replay-receipt-v1.json"
    write_json(frozen_receipt_path, frozen_receipt)
    frozen_v7_counterfactual_receipt = {
        "schema": "billiardgl.phase4.final_ownership_v7_association_counterfactual_receipt.v1",
        "status": "ANALYTIC_NON_FINAL_NOT_A_MACHINE_PASS",
        "gpu_or_cocoa_invoked": False,
        "corrected_capture_claimed": False,
        "frozen_v7_failure_evidence_path": str(
            GATE_FILES["v7_association_failure_evidence_sha256"]),
        "frozen_v7_failure_evidence_sha256":
            GATE_BINDINGS["v7_association_failure_evidence_sha256"],
        "lost_cross_partition_boundary_count": 6,
        "selected_outer_silhouette_below_4_count": 3,
        "analytic_reassociation_visible_hidden": [92, 3],
        "analytic_reassociation_predicted_visible_hidden": [73, 3],
        "warning": "Counterfactual association discovery is not a capture result and cannot satisfy machinePass.",
        "validator_output": frozen_v7_counterfactual_validation,
    }
    frozen_v7_counterfactual_receipt_path = ready / \
        "frozen-v7-association-counterfactual-receipt-v1.json"
    write_json(frozen_v7_counterfactual_receipt_path,
               frozen_v7_counterfactual_receipt)
    frozen_v8_visual_receipt = {
        "schema": "billiardgl.phase4.final_ownership_v8_visual_failure_replay_receipt.v1",
        "status": "FROZEN_FAILURE_NOT_A_MACHINE_PASS",
        "gpu_or_cocoa_invoked": False,
        "frozen_failure_evidence_sha256":
            GATE_BINDINGS["v8_visual_failure_evidence_sha256"],
        "source_probe_count": 83,
        "ordered_source_probe_digest":
            "a3314c4ad04a72bae1718b290129604067954c16685d1d7573726caeb6d5b3f9",
        "authoritative_visible_hidden": [72, 11],
        "machine_pass": False,
        "analytic_81_2_counterfactual_claimed_as_capture": False,
        "validator_output": frozen_v8_validation,
    }
    frozen_v8_visual_receipt_path = ready / \
        "frozen-v8-visual-failure-receipt-v1.json"
    write_json(frozen_v8_visual_receipt_path, frozen_v8_visual_receipt)
    static_report["unittest_output"] = unittest_output.strip().splitlines()
    static_receipt_path = ready / "static-conformance-receipt-v1.json"
    write_json(static_receipt_path, static_report)

    source_rows = [{"path": relative, "sha256": sha(ROOT / relative)}
                   for relative in ALL_BOUND_SOURCES]
    source_manifest = {
        "schema": "billiardgl.phase4.final_ownership_source_bindings.v1",
        "implementation_source_aggregate_sha256":
            aggregate(IMPLEMENTATION_SOURCES),
        "all_modified_source_test_tooling_aggregate_sha256":
            aggregate(ALL_BOUND_SOURCES),
        "algorithm": "sha256(path_utf8 + NUL + file_sha256_ascii + LF), listed order",
        "sources": source_rows,
        "gate_bindings": GATE_BINDINGS,
    }
    source_manifest_path = ready / "source-bindings-v1.json"
    write_json(source_manifest_path, source_manifest)

    frozen_v9_superseded_receipt = {
        "schema":
            "billiardgl.phase4.final_ownership_v9_superseded_validator_lineage.v1",
        "status": "EXPECTED_SUPERSEDED_SOURCE_DRIFT_NOT_A_MACHINE_PASS",
        "gpu_or_cocoa_invoked": False,
        "capture_retry_authorized": False,
        "frozen_v9_failure_evidence_sha256":
            GATE_BINDINGS["v9_compositing_failure_evidence_sha256"],
        "successor_compositing_style_gate_sha256":
            GATE_BINDINGS["v9_compositing_style_correction_gate_sha256"],
        "superseded_validator_path": str(ROOT /
            "scripts/validate_phase4_final_ownership_v9_ordinal1_offline_diagnostic.py"),
        "superseded_validator_returncode": frozen_v9_validator_returncode,
        "superseded_validator_output": frozen_v9_validation,
        "expected_source_drift_errors": expected_v9_source_drift,
        "successor_implementation_source_aggregate_sha256":
            source_manifest["implementation_source_aggregate_sha256"],
        "successor_all_source_test_tooling_aggregate_sha256":
            source_manifest["all_modified_source_test_tooling_aggregate_sha256"],
    }
    frozen_v9_superseded_receipt_path = ready / \
        "frozen-v9-superseded-validator-lineage-receipt-v1.json"
    write_json(frozen_v9_superseded_receipt_path,
               frozen_v9_superseded_receipt)
    frozen_v10_superseded_receipt = {
        "schema":
            "billiardgl.phase4.final_ownership_v10_superseded_validator_lineage.v1",
        "status":
            "EXPECTED_SUPERSEDED_MACHINE_CONTRACT_SOURCE_DRIFT_NOT_A_MACHINE_PASS",
        "gpu_or_cocoa_invoked": False,
        "capture_retry_authorized": False,
        "frozen_v10_failure_evidence_sha256":
            GATE_BINDINGS["v10_machine_contract_failure_evidence_sha256"],
        "successor_machine_contract_gate_sha256":
            GATE_BINDINGS["v10_machine_contract_correction_gate_sha256"],
        "superseded_validator_path": str(ROOT /
            "scripts/validate_phase4_final_ownership_v10_ordinal1_offline_diagnostic.py"),
        "superseded_validator_returncode": frozen_v10_validator_returncode,
        "superseded_validator_output": frozen_v10_validation,
        "expected_source_drift_errors": expected_v10_source_drift,
        "successor_implementation_source_aggregate_sha256":
            source_manifest["implementation_source_aggregate_sha256"],
        "successor_all_source_test_tooling_aggregate_sha256":
            source_manifest["all_modified_source_test_tooling_aggregate_sha256"],
    }
    frozen_v10_superseded_receipt_path = ready / \
        "frozen-v10-superseded-validator-lineage-receipt-v1.json"
    write_json(frozen_v10_superseded_receipt_path,
               frozen_v10_superseded_receipt)

    old = json.loads(OLD_READY.read_text())
    captures = []
    for ordinal, capture in enumerate(old["captures"], 1):
        captures.append({
            "ordinal_one_based": ordinal,
            "capture_id": capture["capture_id"],
            "scenario_id": capture["scenario_id"],
            "camera_preset": capture["camera_preset"],
            "physical_viewport_px": capture["physical_viewport_px"],
            "device_scale": capture["device_scale"],
            "ui_scale": capture["ui_scale"],
            "palette": capture["palette"],
            "overlay_state": capture["overlay_state"],
            "machine_status": "NOT_RUN",
            "ta_status": "NOT_RUN",
            "actual_gpu_capture": False,
            "integrated_final_ownership_evidence": False,
            "gpu_capture_authorized": False,
            "formal68_authorized": False,
        })
    manifest = {
        "schema": "billiardgl.phase4.final_ownership_targeted11_ready.v11",
        "status": "READY_NOT_RUN_GPU_NOT_AUTHORIZED",
        "scope": "Fresh targeted11 readiness only; no capture or evidence claim.",
        "gate_bindings": GATE_BINDINGS,
        "lineage_v1_ready_manifest_path": str(lineage),
        "lineage_v1_ready_manifest_sha256": LINEAGE_V1_SHA256,
        "supersedes_v2_ready_manifest_path": str(superseded_v2),
        "supersedes_v2_ready_manifest_sha256": SUPERSEDED_V2_SHA256,
        "superseded_v2_capture_authorized": False,
        "failed_v3_ready_manifest_path": str(failed_v3_ready),
        "failed_v3_ready_manifest_sha256": FAILED_V3_READY_SHA256,
        "failed_v3_dispatch_receipt_path": str(
            GATE_FILES["failed_v3_dispatch_receipt_sha256"]),
        "failed_v3_dispatch_receipt_sha256": FAILED_V3_DISPATCH_SHA256,
        "failed_v3_authorization_consumed_not_reusable": True,
        "failed_v3_retry_authorized": False,
        "failed_v4_ready_manifest_path": str(failed_v4_ready),
        "failed_v4_ready_manifest_sha256": FAILED_V4_READY_SHA256,
        "failed_v4_dispatch_receipt_path": str(
            GATE_FILES["failed_v4_dispatch_receipt_sha256"]),
        "failed_v4_dispatch_receipt_sha256": FAILED_V4_DISPATCH_SHA256,
        "failed_v4_authorization_consumed_not_reusable": True,
        "failed_v4_retry_authorized": False,
        "superseded_v5_ready_manifest_path": str(superseded_v5_ready),
        "superseded_v5_ready_manifest_sha256": SUPERSEDED_V5_READY_SHA256,
        "superseded_v5_capture_authorized": False,
        "superseded_v6_ready_manifest_path": str(superseded_v6_ready),
        "superseded_v6_ready_manifest_sha256": SUPERSEDED_V6_READY_SHA256,
        "superseded_v6_capture_authorized": False,
        "failed_v7_ready_manifest_path": str(failed_v7_ready),
        "failed_v7_ready_manifest_sha256": FAILED_V7_READY_SHA256,
        "failed_v7_dispatch_receipt_path": str(
            GATE_FILES["failed_v7_dispatch_receipt_sha256"]),
        "failed_v7_dispatch_receipt_sha256": FAILED_V7_DISPATCH_SHA256,
        "failed_v7_authorization_consumed_not_reusable": True,
        "failed_v7_retry_authorized": False,
        "failed_v8_ready_manifest_path": str(failed_v8_ready),
        "failed_v8_ready_manifest_sha256": FAILED_V8_READY_SHA256,
        "failed_v8_dispatch_receipt_path": str(
            GATE_FILES["failed_v8_dispatch_receipt_sha256"]),
        "failed_v8_dispatch_receipt_sha256": FAILED_V8_DISPATCH_SHA256,
        "failed_v8_authorization_consumed_not_reusable": True,
        "failed_v8_retry_authorized": False,
        "failed_v9_ready_manifest_path": str(failed_v9_ready),
        "failed_v9_ready_manifest_sha256": FAILED_V9_READY_SHA256,
        "failed_v9_dispatch_receipt_path": str(
            GATE_FILES["failed_v9_dispatch_receipt_sha256"]),
        "failed_v9_dispatch_receipt_sha256": FAILED_V9_DISPATCH_SHA256,
        "failed_v9_authorization_consumed_not_reusable": True,
        "failed_v9_retry_authorized": False,
        "failed_v10_ready_manifest_path": str(failed_v10_ready),
        "failed_v10_ready_manifest_sha256": FAILED_V10_READY_SHA256,
        "failed_v10_dispatch_receipt_path": str(
            GATE_FILES["failed_v10_dispatch_receipt_sha256"]),
        "failed_v10_dispatch_receipt_sha256": FAILED_V10_DISPATCH_SHA256,
        "failed_v10_authorization_consumed_not_reusable": True,
        "failed_v10_retry_authorized": False,
        "implementation_source_aggregate_sha256":
            source_manifest["implementation_source_aggregate_sha256"],
        "all_source_test_tooling_aggregate_sha256":
            source_manifest["all_modified_source_test_tooling_aggregate_sha256"],
        "source_binding_manifest_path": str(source_manifest_path),
        "source_binding_manifest_sha256": sha(source_manifest_path),
        "runner_source_path": str(runner_source),
        "runner_source_sha256": actual_artifact_sha256["runner_source"],
        "runner_path": str(runner),
        "runner_sha256": actual_artifact_sha256["runner"],
        "billiards_executable_path": str(billiards),
        "billiards_executable_sha256": actual_artifact_sha256["billiards"],
        "required_expected_release_artifact_sha256": expected_artifact_sha256,
        "release_build": True,
        "capture_runner_built_not_executed": True,
        "plan_path": str(plan),
        "plan_sha256": sha(plan),
        "targeted_output_root": str(output_root),
        "targeted_output_root_initial_entry_count": 0,
        "results_path": str(output_root / "results.jsonl"),
        "results_file_exists": False,
        "capture_count": 11,
        "not_run_count": 11,
        "actual_gpu_capture_count": 0,
        "cpu_fixture_receipt_path": str(cpu_receipt_path),
        "cpu_fixture_receipt_sha256": sha(cpu_receipt_path),
        "frozen_replay_receipt_path": str(frozen_receipt_path),
        "frozen_replay_receipt_sha256": sha(frozen_receipt_path),
        "frozen_v7_association_counterfactual_receipt_path": str(
            frozen_v7_counterfactual_receipt_path),
        "frozen_v7_association_counterfactual_receipt_sha256": sha(
            frozen_v7_counterfactual_receipt_path),
        "frozen_v8_visual_failure_receipt_path": str(
            frozen_v8_visual_receipt_path),
        "frozen_v8_visual_failure_receipt_sha256": sha(
            frozen_v8_visual_receipt_path),
        "frozen_v9_compositing_failure_receipt_path": str(
            GATE_FILES["v9_compositing_failure_evidence_sha256"]),
        "frozen_v9_compositing_failure_receipt_sha256":
            GATE_BINDINGS["v9_compositing_failure_evidence_sha256"],
        "frozen_v9_superseded_validator_lineage_receipt_path": str(
            frozen_v9_superseded_receipt_path),
        "frozen_v9_superseded_validator_lineage_receipt_sha256": sha(
            frozen_v9_superseded_receipt_path),
        "frozen_v10_machine_contract_failure_receipt_path": str(
            GATE_FILES["v10_machine_contract_failure_evidence_sha256"]),
        "frozen_v10_machine_contract_failure_receipt_sha256":
            GATE_BINDINGS["v10_machine_contract_failure_evidence_sha256"],
        "frozen_v10_superseded_validator_lineage_receipt_path": str(
            frozen_v10_superseded_receipt_path),
        "frozen_v10_superseded_validator_lineage_receipt_sha256": sha(
            frozen_v10_superseded_receipt_path),
        "static_conformance_receipt_path": str(static_receipt_path),
        "static_conformance_receipt_sha256": sha(static_receipt_path),
        "gpu_capture_authorized": False,
        "technical_art_review_authorized": False,
        "formal68_authorized": False,
        "fresh_targeted_readiness_is_not_capture_authorization": True,
        "authorization_issued": False,
        "capture_dispatcher_path": str(ROOT /
            "scripts/run_phase4_final_ownership_targeted_capture.py"),
        "capture_dispatch_validator_path": str(ROOT /
            "scripts/validate_phase4_final_ownership_capture_dispatch.py"),
        "evidence_claims": {name: False for name in (
            "actual_gpu_capture", "machine_capture", "technical_art_review",
            "manual_review", "m3", "formal68", "golden", "beta")},
        "immutable_invariants": {
            "ordinal1_source_probe_denominator": 95,
            "ordinal2_source_probe_denominator": 83,
            "ordinal2_ordered_source_probe_digest":
                "a3314c4ad04a72bae1718b290129604067954c16685d1d7573726caeb6d5b3f9",
            "ordinal2_source_probe_ids_bound_by_visual_gate_sha256":
                GATE_BINDINGS["v8_visual_style_correction_gate_sha256"],
            "aggregate_visible_probe_coverage_min": 0.90,
            "predicted_visible_probe_coverage_min": 0.90,
            "actual_visible_probe_coverage_min": 0.90,
            "minimum_local_contrast_ratio": 4.0,
            "minimum_owned_area_pixel_equivalents": 1.5,
            "predicted_lateral_offset_px": 0.0,
            "actual_lateral_offset_px": 0.0,
            "maximum_screen_space_offset_px": 0.0,
            "compositing_order": "actual_then_predicted",
        },
        "captures": captures,
    }
    manifest_path = ready / "targeted11-readiness-v11.json"
    write_json(manifest_path, manifest)
    print(manifest_path)


if __name__ == "__main__":
    main()
