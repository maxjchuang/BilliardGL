#!/usr/bin/env python3
"""CPU-only validator for the consumed v3 GL_INVALID_OPERATION diagnostic."""

from __future__ import annotations

import argparse
import hashlib
import json
from pathlib import Path
import stat

ROOT = Path(__file__).resolve().parents[1]
DIAGNOSTIC_ROOT = ROOT / \
    "docs/art/phase4-final-ownership-v3-gl1282-offline-diagnostic-v1"
DEFAULT_EVIDENCE = DIAGNOSTIC_ROOT / "failure-evidence-receipt-v1.json"
DEFAULT_GATE = DIAGNOSTIC_ROOT / "minimal-correction-design-gate-v1.json"
DEFAULT_RENDERER = ROOT / "src/Billiards/render/modern/modern_renderer.cpp"
DEFAULT_RUNNER_SOURCE = ROOT / \
    "build-integrated-phase2-v4-phase4-ownership-v1/generated/phase4_overlay_v4_gpu_capture_runner.mm"
VALIDATOR_PATH = ROOT / \
    "scripts/validate_phase4_final_ownership_v3_gl1282_diagnostic.py"
TEST_PATH = ROOT / "tests/test_phase4_final_ownership_v3_gl1282_diagnostic.py"

EXPECTED_FILES = {
    "dispatch_failure_receipt": (
        ROOT / "docs/art/phase4-training-replay-overlay-v4-final-ownership-targeted11-output-v3/phase4-final-ownership-dispatch-failure-receipt-v1.json",
        "8bf47205bddc1957822241bc9a9a66ee34aa2afb8a1f3b08c43f0379dae41804", 0o444, 2623),
    "authorization_receipt": (
        ROOT / "docs/evidence/phase4-final-ownership-targeted11-v3-authorization-root-v1.json",
        "b31a86313bda80f99b542efdc69133d22c2bdc2a3affc0b1d706839a049529c9", 0o444, 1260),
    "authorization_consumed_marker": (
        ROOT / "docs/evidence/phase4-final-ownership-targeted11-v3-authorization-root-v1.json.consumed",
        "82635d6c386aba2307c17e536b1a4cc9bde95efce6f9bc6e05dbc64bd9a8c44e", 0o444, 64),
    "executed_plan": (
        ROOT / "docs/art/phase4-training-replay-overlay-v4-final-ownership-targeted11-output-v3/phase4-final-ownership-targeted11-plan.tsv",
        "3b969b34e52fe2e01e31e34b148c4ce7c211682db65b3c609a43274366c8717d", 0o444, 5755),
    "empty_results": (
        ROOT / "docs/art/phase4-training-replay-overlay-v4-final-ownership-targeted11-output-v3/results.jsonl",
        "e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855", 0o444, 0),
    "v3_readiness": (
        ROOT / "docs/art/phase4-training-replay-overlay-v4-final-ownership-targeted11-ready-v3/targeted11-readiness-v3.json",
        "943d9bc17383de817d97fc5984fd9d5590e0afde234ae7d2b5807add64caeeb9", 0o644, 12688),
}
EXPECTED_SOURCE_SHA = {
    "modern_renderer.cpp": "8b160626dc541393bd885f86fb78e5daf974a0d6372cbbe304771c2aadc78115",
    "generated_runner_source": "85ad2e67d6303b7ed63891151b8ecb3e01f75f34f784bec51633f3838754edb9",
    "runner_binary": "ef2633ea5dbdaa825d7710635f4292d0f16ff79813a0c719b43414453b251bcc",
    "billiards_binary": "854e86b9a7f995f7abd2417cbfc8b00610ed2e5f97525163d17d165e9ae427e9",
}
OUTPUT_ROOT = ROOT / \
    "docs/art/phase4-training-replay-overlay-v4-final-ownership-targeted11-output-v3"
TELEMETRY = OUTPUT_ROOT / "results.jsonl.final-ownership.jsonl"
AUTH_NONCE = "phase4-final-ownership-targeted11-root-20260811T081500-8f1b7c3a"


def sha(path: Path) -> str:
    return hashlib.sha256(path.read_bytes()).hexdigest()


def validate(evidence_path: Path = DEFAULT_EVIDENCE,
             gate_path: Path = DEFAULT_GATE,
             renderer_path: Path = DEFAULT_RENDERER,
             runner_source_path: Path = DEFAULT_RUNNER_SOURCE) -> list[str]:
    errors: list[str] = []
    try:
        evidence = json.loads(evidence_path.read_text())
        gate = json.loads(gate_path.read_text())
    except (OSError, ValueError, TypeError) as error:
        return [f"diagnostic_document_invalid:{type(error).__name__}"]

    observed_rows = evidence.get("frozen_files", {})
    for name, (path, digest, mode, size) in EXPECTED_FILES.items():
        try:
            actual_mode = stat.S_IMODE(path.stat().st_mode)
            row = observed_rows.get(name, {})
            if not path.is_file() or sha(path) != digest or \
                    actual_mode != mode or path.stat().st_size != size or \
                    row != {"path": str(path), "sha256": digest,
                            "mode_octal": oct(mode), "size_bytes": size}:
                errors.append(f"frozen_file_binding_invalid:{name}")
        except OSError:
            errors.append(f"frozen_file_binding_invalid:{name}")
    if TELEMETRY.exists() or evidence.get("telemetry_file_exists") is not False:
        errors.append("telemetry_absence_binding_invalid")
    expected_entries = sorted(path.name for path in OUTPUT_ROOT.iterdir())
    if expected_entries != [
            "phase4-final-ownership-dispatch-failure-receipt-v1.json",
            "phase4-final-ownership-targeted11-plan.tsv", "results.jsonl"] or \
            evidence.get("output_root_entries") != expected_entries:
        errors.append("output_root_inventory_invalid")

    failure = json.loads(EXPECTED_FILES["dispatch_failure_receipt"][0].read_text())
    if failure.get("schema") != \
            "billiardgl.phase4.final_ownership_capture_dispatch_failure.v1" or \
            failure.get("status") != \
            "CAPTURE_INFRASTRUCTURE_FAILED_NO_ORDINAL_TELEMETRY" or \
            failure.get("runner_exit_code") != 1 or \
            (failure.get("completed_count"), failure.get("failed_count"),
             failure.get("unrun_count")) != (0, 0, 11) or \
            failure.get("authorization_consumed_once") is not True or \
            failure.get("validation_errors") != []:
        errors.append("existing_dispatch_failure_receipt_semantics_invalid")
    authorization = json.loads(
        EXPECTED_FILES["authorization_receipt"][0].read_text())
    consumed = EXPECTED_FILES["authorization_consumed_marker"][0].read_text()
    if authorization.get("schema") != \
            "billiardgl.phase4.final_ownership_gpu_authorization.v1" or \
            authorization.get("single_use") is not True or \
            authorization.get("authorization_nonce") != AUTH_NONCE or \
            consumed != AUTH_NONCE + "\n" or \
            evidence.get("authorization_reusable") is not False:
        errors.append("authorization_consumption_semantics_invalid")

    try:
        renderer = renderer_path.read_text()
        runner_source = runner_source_path.read_text()
    except OSError as error:
        return errors + [f"source_read_failed:{type(error).__name__}"]
    runner_binary = ROOT / \
        "build-integrated-phase2-v4-phase4-ownership-v1/BilliardsPhase4OverlayV4GpuCaptureRunner"
    billiards = ROOT / "build-integrated-phase2-v4-phase4-ownership-v1/Billiards"
    if sha(renderer_path) != EXPECTED_SOURCE_SHA["modern_renderer.cpp"] or \
            sha(runner_source_path) != EXPECTED_SOURCE_SHA["generated_runner_source"] or \
            sha(runner_binary) != EXPECTED_SOURCE_SHA["runner_binary"] or \
            sha(billiards) != EXPECTED_SOURCE_SHA["billiards_binary"]:
        errors.append("frozen_source_or_binary_binding_invalid")

    required_renderer_tokens = (
        "uniform usampler2DMS uSemanticMsaa;",
        "uniform usampler2D uSemanticSingle;",
        "textureSize(uSemanticMsaa)", "textureSize(uSemanticSingle,0)",
        "texelFetch(uSemanticMsaa", "texelFetch(uSemanticSingle",
        '"uSemanticMsaa"),0);', '"uSemanticSingle"),0);',
        "GL_RGBA32UI", "layout(location=0)out uvec4 resolvedOwnerTokens;",
        'requireComplete("resolved final ownership")',
        '"integrated_semantic_msaa_resolve"', "glDisable(GL_BLEND);",
        "glDrawArrays(GL_TRIANGLES,0,6);",
    )
    if any(token not in renderer for token in required_renderer_tokens):
        errors.append("root_cause_static_chain_missing")
    if "options.msaaSamples = 0" not in runner_source or \
            "options.enableFinalOwnershipInstrumentation = true" not in runner_source:
        errors.append("failed_runner_mode_binding_invalid")
    if evidence.get("root_cause", {}).get("id") != \
            "ACTIVE_DIFFERENT_SAMPLER_TYPES_ALIAS_TEXTURE_UNIT_ZERO" or \
            evidence.get("root_cause", {}).get("isolated") is not True or \
            evidence.get("root_cause", {}).get("gl_error") != 1282 or \
            evidence.get("root_cause", {}).get("trigger_command") != "glDrawArrays":
        errors.append("root_cause_decision_invalid")
    hypotheses = evidence.get("falsifiable_hypotheses", [])
    if len(hypotheses) != 6 or sum(row.get("decision") == "CONFIRMED"
                                   for row in hypotheses) != 1:
        errors.append("hypothesis_matrix_invalid")
    if evidence.get("gpu_or_cocoa_invoked_by_diagnostic") is not False or \
            evidence.get("retry_authorized") is not False or \
            evidence.get("new_readiness_or_authorization_created") is not False:
        errors.append("diagnostic_authorization_boundary_invalid")

    if gate.get("schema") != \
            "billiardgl.phase4.final_ownership_gl1282_minimal_correction_design_gate.v1" or \
            gate.get("status") != "DESIGN_ONLY_NO_IMPLEMENTATION_OR_GPU_AUTH" or \
            gate.get("failure_evidence_receipt_sha256") != sha(evidence_path) or \
            gate.get("validator_sha256") != sha(VALIDATOR_PATH) or \
            gate.get("tests_sha256") != sha(TEST_PATH):
        errors.append("minimal_correction_gate_binding_invalid")
    boundary = gate.get("authorization_boundary", {})
    if any(boundary.get(key) is not False for key in (
            "production_change_implemented", "runner_change_implemented",
            "gpu_capture_authorized", "retry_authorized",
            "new_readiness_authorized", "formal68_authorized")):
        errors.append("minimal_correction_gate_authorization_invalid")
    correction = gate.get("required_minimal_correction", {})
    if correction.get("strategy") != \
            "SEPARATE_SINGLE_SAMPLE_AND_MULTISAMPLE_RESOLVE_PROGRAM_VARIANTS" or \
            correction.get("active_sampler_types_per_linked_program_max") != 1 or \
            correction.get("preserve_output_format") != "GL_RGBA32UI" or \
            correction.get("preserve_pass_order") is not True:
        errors.append("minimal_correction_contract_invalid")
    return errors


def main() -> None:
    parser = argparse.ArgumentParser()
    parser.add_argument("--evidence-receipt", type=Path, default=DEFAULT_EVIDENCE)
    parser.add_argument("--design-gate", type=Path, default=DEFAULT_GATE)
    parser.add_argument("--renderer-source", type=Path, default=DEFAULT_RENDERER)
    parser.add_argument("--runner-source", type=Path, default=DEFAULT_RUNNER_SOURCE)
    args = parser.parse_args()
    errors = validate(args.evidence_receipt, args.design_gate,
                      args.renderer_source, args.runner_source)
    print(json.dumps({"status": "PASS" if not errors else "FAIL",
                      "gpu_or_cocoa_invoked": False, "errors": errors},
                     indent=2, sort_keys=True))
    if errors:
        raise SystemExit(1)


if __name__ == "__main__":
    main()
