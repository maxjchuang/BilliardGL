#!/usr/bin/env python3
"""Fail-closed validator for the CPU-only Phase4 V16 premise failure."""

from __future__ import annotations

import argparse
import difflib
import hashlib
import json
import shutil
import subprocess
import tempfile
from pathlib import Path


ROOT = Path(__file__).resolve().parents[1]
EVIDENCE = ROOT / "docs/art/phase4-v16-local-cross-track-actual-continuity-premise-falsification-v1"
DEFAULT = EVIDENCE / "premise-failure-receipt-v1.json"
SCHEMA = "billiardgl.phase4.v16.local_cross_track_actual_continuity_premise_failure.v1"
STATUS = "GATE_PREMISE_FALSIFIED_IMPLEMENTATION_REVERTED_NO_GPU_NO_AUTHORIZATION"

BASELINES = {
    "trajectory_cpp": ("src/Billiards/training/trajectory_overlay_renderer.cpp", "0c108c506641ab4828290a1a4c7b6b5ebbd51ab4fc7c12845bde0f5deb18a840"),
    "trajectory_h": ("src/Billiards/training/trajectory_overlay_renderer.h", "9805e6a5f56c17d7645200576505425cddf0d642970e5dc5cec186e003bd564e"),
    "modern_cpp": ("src/Billiards/render/modern/modern_renderer.cpp", "b00cc6b370122275a42f106967f935dbb34bd5d3f81f2a750ca9ba6e503af191"),
    "modern_h": ("src/Billiards/render/modern/modern_renderer.h", "936de5b77c9e3485f2e51f10e9de26c59c8ec94674865e0b50ae125725183c9d"),
    "cmake": ("CMakeLists.txt", "f41563c35224dd809f64acedba6455355e70082697c41305c2d1b05426f71d41"),
}
CANDIDATES = {
    "candidate_trajectory_cpp": ("candidate-sources/src/Billiards/training/trajectory_overlay_renderer.cpp", "bbcced5b0d1ff056495a93ff2388961b2c833fb748f7487b29967d4bd7fa2de9", "trajectory_cpp"),
    "candidate_trajectory_h": ("candidate-sources/src/Billiards/training/trajectory_overlay_renderer.h", "66905d2bb955b7f278778c8d4a8f0c0a65c05ef01473429fab61da2a46bbf6f8", "trajectory_h"),
    "candidate_modern_cpp": ("candidate-sources/src/Billiards/render/modern/modern_renderer.cpp", "523aeb64a9a2805deb00eaea177fdc317d1ad170e86ba0a07377b6704172aba4", "modern_cpp"),
    "candidate_modern_h": ("candidate-sources/src/Billiards/render/modern/modern_renderer.h", "4ae0be93c453b097f6402b54801783ea41ee1bd804f80b1a1ac8f12b6c3c59b4", "modern_h"),
    "candidate_cmake": ("candidate-sources/CMakeLists.txt", "f8345dabb96752e8954afa32bfeaac149f2e6f5b70f6a288a8fc92a49af3ee25", "cmake"),
}
FIXED = {
    "diagnostic_source": ("phase4_v16_premise_falsification_cpu_tests.cpp", "8ac7409db89b7637860110f5a010769aeae017563c6be06fdd22b896b1da0f3b"),
    "frozen_stderr": ("frozen-stderr-v1.txt", "8dfe2be1696e82c143f42ba3ccbfd4a0a269457721f5c42b2d3fc52dd8e6fba0"),
    "metrics": ("premise-failure-metrics-v1.json", "126809799cb42a6529244e300ac4067480ad338074d5f926bc1280003aa7217b"),
    "link_inputs": ("inherited-link-inputs-v1.sha256", "21cf034173b9c33cd9155ed49a8fe7cfd922dc70601e28511f9e183fdf2b830a"),
}
BUILD_ARTIFACTS = {
    "diagnostic_test_object": ("build-phase4-v16-premise-falsification-cpu/test.o", "9dac9d96d905dacfb5c623207e879c9f8c6ad5438c5fa758b4c52339ab93279c"),
    "candidate_renderer_object": ("build-phase4-v16-premise-falsification-cpu/renderer.o", "9a35364342e0d4c0231256341b1dd04cd4cd8fd65b98141eef773242fa472737"),
    "diagnostic_binary": ("build-phase4-v16-premise-falsification-cpu/phase4_v16_premise_falsification_cpu_tests", "143edad6896d9e6b9d8f2a6e42edb20f069dea84ce8acef42aba787baede6b07"),
}
COMPILE_CONTRACT = {
    "compiler": "/usr/bin/clang++",
    "architecture": "arm64",
    "language": "c++17",
    "optimization": ["-O3", "-DNDEBUG"],
    "diagnostic_define": "BILLIARDGL_PHASE4_V16_CONTINUITY_CPU_STATIC=1",
    "candidate_include_precedes_workspace_include": True,
    "expected_exit_code": 1,
    "expected_stdout_sha256": hashlib.sha256(b"").hexdigest(),
}


def sha(path: Path) -> str:
    return hashlib.sha256(path.read_bytes()).hexdigest()


def binding(value: object, path: str, digest: str) -> bool:
    return isinstance(value, dict) and value.get("path") == path and value.get("sha256") == digest


def bound_file(value: object, root: Path, path: str, digest: str) -> bool:
    if not binding(value, path, digest):
        return False
    target = root / path
    return target.is_file() and sha(target) == digest


def validate_patch_roundtrip(errors: list[str]) -> None:
    with tempfile.TemporaryDirectory() as directory:
        temp = Path(directory)
        for role, (candidate_rel, candidate_sha, baseline_role) in CANDIDATES.items():
            baseline_rel, baseline_sha = BASELINES[baseline_role]
            baseline = ROOT / baseline_rel
            candidate = EVIDENCE / candidate_rel
            target = temp / role
            shutil.copyfile(baseline, target)
            patch = "".join(difflib.unified_diff(
                baseline.read_text().splitlines(keepends=True),
                candidate.read_text().splitlines(keepends=True),
                fromfile="baseline", tofile="candidate"))
            forward = subprocess.run(["/usr/bin/patch", "--silent", str(target)], input=patch, text=True, capture_output=True)
            if forward.returncode or sha(target) != candidate_sha:
                errors.append(f"candidate_forward_roundtrip_invalid:{role}")
                continue
            reverse = subprocess.run(["/usr/bin/patch", "--silent", "-R", str(target)], input=patch, text=True, capture_output=True)
            if reverse.returncode or sha(target) != baseline_sha:
                errors.append(f"candidate_reverse_roundtrip_invalid:{role}")


def read_link_inputs(errors: list[str]) -> list[Path]:
    result: list[Path] = []
    lines = (EVIDENCE / FIXED["link_inputs"][0]).read_text().splitlines()
    if len(lines) != 47:
        errors.append("link_input_count_invalid")
        return result
    for index, line in enumerate(lines):
        digest, separator, relative = line.partition("  ")
        path = ROOT / relative
        if not separator or not path.is_file() or sha(path) != digest:
            errors.append(f"link_input_invalid:{index}")
        result.append(path)
    return result


def recompile_relink_replay(link_inputs: list[Path], frozen_stderr: str, errors: list[str]) -> None:
    with tempfile.TemporaryDirectory() as directory:
        temp = Path(directory)
        candidate_root = EVIDENCE / "candidate-sources"
        common = ["-O3", "-DNDEBUG", "-std=c++17", "-arch", "arm64",
            f'-DBILLIARDGL_ASSET_ROOT="{ROOT / "src/Billiards"}"',
            "-DBILLIARDGL_PHASE4_V16_CONTINUITY_CPU_STATIC=1",
            f'-DBILLIARDGL_SOURCE_ROOT="{ROOT}"',
            # The embedded V14 source includes "training/...".  Its candidate
            # header must precede the restored workspace header.
            f'-I{candidate_root / "src/Billiards"}', f'-I{candidate_root}',
            f'-I{ROOT / "src/Billiards"}', f'-I{ROOT}']
        test_object = temp / "test.o"
        renderer_object = temp / "renderer.o"
        test_command = ["/usr/bin/clang++", *common,
            f'-DBILLIARDGL_PHASE4_V16_CANDIDATE_SOURCE_ROOT="{candidate_root}"',
            "-c", str(EVIDENCE / FIXED["diagnostic_source"][0]), "-o", str(test_object)]
        renderer_command = ["/usr/bin/clang++", *common, "-c",
            str(candidate_root / "src/Billiards/training/trajectory_overlay_renderer.cpp"),
            "-o", str(renderer_object)]
        for command, output, expected, role in (
            (test_command, test_object, "9dac9d96d905dacfb5c623207e879c9f8c6ad5438c5fa758b4c52339ab93279c", "test"),
            (renderer_command, renderer_object, "9a35364342e0d4c0231256341b1dd04cd4cd8fd65b98141eef773242fa472737", "renderer")):
            run = subprocess.run(command, cwd=ROOT, capture_output=True, text=True)
            if run.returncode or not output.is_file() or sha(output) != expected:
                errors.append(f"candidate_recompile_invalid:{role}")
                return
        binary = temp / "diagnostic"
        link = subprocess.run(["/usr/bin/clang++", "-arch", "arm64",
            str(test_object), str(renderer_object), *map(str, link_inputs),
            "-o", str(binary)], cwd=ROOT, capture_output=True, text=True)
        if link.returncode:
            errors.append("candidate_relink_invalid")
            return
        replay = subprocess.run([str(binary)], cwd=ROOT, capture_output=True, text=True)
        if replay.returncode != 1 or replay.stdout != "" or replay.stderr != frozen_stderr:
            errors.append("candidate_replay_invalid")


def validate(path: Path, recompile: bool = True) -> list[str]:
    errors: list[str] = []
    try:
        value = json.loads(path.read_text())
    except Exception as exc:
        return [f"receipt_invalid:{exc}"]
    if value.get("schema") != SCHEMA or value.get("status") != STATUS or value.get("scope") != "CPU_ONLY_NO_GPU_NO_AUTHORIZATION":
        errors.append("schema_status_scope_invalid")
    gate_path = "docs/art/phase4-v16-local-cross-track-actual-continuity-implementation-design-gate-v1.json"
    gate_sha = "f281949f298cb0e07c7f8e427f419455555709277e137f34389ffdb3789c2191"
    if not bound_file(value.get("gate_binding"), ROOT, gate_path, gate_sha):
        errors.append("gate_binding_invalid")
    lineage_expected = {
        "v15_diagnostic_receipt": ("docs/art/phase4-v15-broadcast-overwrite-provenance-diagnostic-v1/diagnostic-receipt-v1.json", "05e0bc2de8fd9c0978bf66547f2ce444b9bee864ca27dab871f2482b4df80dcf"),
        "v15_provenance_ledger": ("docs/art/phase4-v15-broadcast-overwrite-provenance-diagnostic-v1/provenance-ledger-v1.json", "bb2c3ecafb01d42cfddb8eec89a22619bf646ddb64773846747c5c76d2f5538b"),
        "v14_reproducible_receipt": ("docs/art/phase4-v14-cap-mask-premise-falsification-v2/premise-falsification-receipt-v2.json", "bf320f08453a1b55f48235193ecd92ed3a88ccf671b506ee0cea36276e9f97f8"),
    }
    for role, (relative, digest) in lineage_expected.items():
        if not bound_file(value.get("lineage", {}).get(role), ROOT, relative, digest):
            errors.append(f"lineage_invalid:{role}")
    for role, (relative, digest) in BASELINES.items():
        if not bound_file(value.get("restored_baselines", {}).get(role), ROOT, relative, digest):
            errors.append(f"baseline_not_restored:{role}")
    evidence = value.get("reproducible_evidence", {})
    for role, (relative, digest, _) in CANDIDATES.items():
        full = "docs/art/phase4-v16-local-cross-track-actual-continuity-premise-falsification-v1/" + relative
        if not binding(evidence.get(role), full, digest) or not (EVIDENCE / relative).is_file() or sha(EVIDENCE / relative) != digest:
            errors.append(f"candidate_binding_invalid:{role}")
    for role, (relative, digest) in FIXED.items():
        full = "docs/art/phase4-v16-local-cross-track-actual-continuity-premise-falsification-v1/" + relative
        if not binding(evidence.get(role), full, digest) or not (EVIDENCE / relative).is_file() or sha(EVIDENCE / relative) != digest:
            errors.append(f"evidence_binding_invalid:{role}")
    for role, (relative, digest) in BUILD_ARTIFACTS.items():
        if not bound_file(evidence.get(role), ROOT, relative, digest):
            errors.append(f"build_artifact_binding_invalid:{role}")
    if evidence.get("link_inputs", {}).get("count") != 47 or value.get("compile_contract") != COMPILE_CONTRACT:
        errors.append("replay_contract_invalid")
    metrics = json.loads((EVIDENCE / FIXED["metrics"][0]).read_text())
    if metrics.get("single_sample_center", {}).get("standard_unsatisfiable_crossing_islands") != 27 or metrics.get("single_sample_center", {}).get("cvd_unsatisfiable_crossing_islands") != 27:
        errors.append("single_sample_failure_counts_invalid")
    four = metrics.get("asymmetric_four_sample", {})
    if four.get("standard_unsatisfiable_crossing_islands") != 76 or four.get("cvd_unsatisfiable_crossing_islands") != 76 or four.get("conversion") != "cpu_y=1.0-gl_y; sample index remains gl_SampleID":
        errors.append("asymmetric_four_sample_failure_invalid")
    first = metrics.get("first_single_sample_failure", {})
    if first.get("physical_pixel") != [840, 451] or first.get("maximum_largest8_after_any_allowed_subset_of_size_at_most_2") != 1 or first.get("required_largest8") != 2:
        errors.append("constructive_counterexample_invalid")
    stderr = (EVIDENCE / FIXED["frozen_stderr"][0]).read_text()
    required_lines = ["V16_FAILED_ISLANDS palette=standard count=27", "V16_FAILED_ISLANDS palette=cvd count=27", "V16_ASYMMETRIC_4X standard_ok=0 standard_failed=76 cvd_ok=0 cvd_failed=76", "x=840 y=451 actual_arc=actual_arc_1 keys=2", "PHASE4_V16_LOCAL_CROSS_TRACK_CPU_STATIC_FAIL"]
    for token in required_lines:
        if token not in stderr:
            errors.append(f"frozen_output_semantics_missing:{token}")
    modern = (EVIDENCE / CANDIDATES["candidate_modern_cpp"][0]).read_text()
    if "{{position[0], 1.0f - position[1]}}" not in modern or "glTexSubImage3D" not in modern:
        errors.append("candidate_coordinate_or_upload_path_missing")
    upload = modern[modern.find("bool uploadTrajectoryContinuityMask"):modern.find("std::uint32_t drawTrajectoryOverlay")]
    if "checkGl" in upload or "glGetError" in upload:
        errors.append("latent_upload_failure_characterization_drift")
    observed = value.get("observed", {})
    if observed != {
        "single_sample_unsatisfiable_islands_standard_cvd": [27, 27],
        "asymmetric_four_sample_unsatisfiable_islands_standard_cvd": [76, 76],
        "fail_closed_selected_count": 0,
        "standard_cvd_topology_counts_identical": True,
        "sample_coordinate_conversion": "cpu_y=1.0-gl_y_with_same_gl_SampleID_layer",
        "gate_expected_0_1_2_is_not_achievable_for_generic_full_frame": True,
        "coordinate_or_repair_id_filter_would_violate_gate": True,
        "latent_gl_upload_error_fail_open_remains": True,
    }:
        errors.append("observed_contract_invalid")
    claims = value.get("claims", {})
    if set(claims) != {"v16_implementation_complete", "source_stable_for_build", "production_change_retained", "final_build_authorized", "readiness_authorized", "gpu_authorized", "capture_authorized", "machine_or_ta_pass_claimed", "successor_design_gate_required"} or any(claims[k] for k in claims if k != "successor_design_gate_required") or claims.get("successor_design_gate_required") is not True:
        errors.append("authorization_or_claims_invalid")
    validate_patch_roundtrip(errors)
    link_inputs = read_link_inputs(errors)
    binary = ROOT / BUILD_ARTIFACTS["diagnostic_binary"][0]
    if binary.is_file():
        replay = subprocess.run([str(binary)], cwd=ROOT, capture_output=True, text=True)
        if replay.returncode != 1 or replay.stdout != "" or replay.stderr != stderr:
            errors.append("frozen_binary_replay_invalid")
    if recompile and not errors:
        recompile_relink_replay(link_inputs, stderr, errors)
    return errors


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--receipt", type=Path, default=DEFAULT)
    parser.add_argument("--skip-recompile", action="store_true")
    args = parser.parse_args()
    errors = validate(args.receipt, not args.skip_recompile)
    print(json.dumps({"ok": not errors, "errors": errors}, sort_keys=True))
    return 0 if not errors else 1


if __name__ == "__main__":
    raise SystemExit(main())
