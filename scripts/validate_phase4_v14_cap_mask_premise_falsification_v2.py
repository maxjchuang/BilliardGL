#!/usr/bin/env python3
"""Fail-closed validator for reproducible v14 premise-falsification evidence."""

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
DEFAULT = ROOT / "docs/art/phase4-v14-cap-mask-premise-falsification-v2/premise-falsification-receipt-v2.json"
SCHEMA = "billiardgl.phase4.v14.cap_mask_premise_falsification.v2"
STATUS = "REPRODUCIBILITY_GAP_CLOSED_SOURCE_PRESERVED_BINARY_REPLAYABLE"
EXPECTED_SOURCE_SHA = "2b889dbf151e6fd52a5284877373e21702a8484b24b4b54ae1642757b5df51fa"
EXPECTED_FIXTURE = {
    "capture_id": "camera_broadcast_1080",
    "source_probe_count": 81,
    "ordered_source_digest": "33511aa46717a208f3db43acd278416b8fe8393e2e003484da8f44dabffe5645",
    "blocking_coordinate": [940, 525],
    "neighborhood_radius_px": 8,
    "palettes": ["standard", "color_vision_safe"],
    "sample_route": "single_sample_center_0.5_0.5",
}
EXPECTED_PREMISE = {
    "actual_route_body_source_mask_pixels": 88,
    "final_source_mask_pixels_owned_by_predicted_core": 81,
    "final_source_mask_pixels_owned_by_untagged_predicted_inner_body": 7,
    "final_source_mask_pixels_owned_by_tagged_predicted_inner_cap": 0,
    "tagged_predicted_inner_cap_discards": 59,
    "final_actual_route_body_pixels": 0,
    "legacy_helper_final_actual_tick_pixels": 1,
    "cap_predicate_executed_but_not_authoritative": True,
    "successor_design_gate_required": True,
}
SOURCE_PAIRS = {
    "trajectory_renderer_cpp": (
        "src/Billiards/training/trajectory_overlay_renderer.cpp",
        "docs/art/phase4-v14-cap-mask-premise-falsification-v2/candidate-sources/src/Billiards/training/trajectory_overlay_renderer.cpp",
        "0c108c506641ab4828290a1a4c7b6b5ebbd51ab4fc7c12845bde0f5deb18a840",
        "d534b7a04502345dfac3da4b948be2dc4f8c82c48adb0cc00c07aa5bfea28f74",
    ),
    "trajectory_renderer_h": (
        "src/Billiards/training/trajectory_overlay_renderer.h",
        "docs/art/phase4-v14-cap-mask-premise-falsification-v2/candidate-sources/src/Billiards/training/trajectory_overlay_renderer.h",
        "9805e6a5f56c17d7645200576505425cddf0d642970e5dc5cec186e003bd564e",
        "c9b2a14e6e0024f52d3675c04fcc5d709c7d999810e3a7effb7ca62ebffd8003",
    ),
    "modern_renderer_cpp": (
        "src/Billiards/render/modern/modern_renderer.cpp",
        "docs/art/phase4-v14-cap-mask-premise-falsification-v2/candidate-sources/src/Billiards/render/modern/modern_renderer.cpp",
        "b00cc6b370122275a42f106967f935dbb34bd5d3f81f2a750ca9ba6e503af191",
        "4f83c2eaf6219297df43a66a2c1b79c8d86496f85b272934f85e4b7f45a21015",
    ),
    "modern_renderer_h": (
        "src/Billiards/render/modern/modern_renderer.h",
        "docs/art/phase4-v14-cap-mask-premise-falsification-v2/candidate-sources/src/Billiards/render/modern/modern_renderer.h",
        "936de5b77c9e3485f2e51f10e9de26c59c8ec94674865e0b50ae125725183c9d",
        "9543ed6aab151cdc08722640d83a256175a3988e6ace08474b142656abe996b6",
    ),
}


def sha(path: Path) -> str:
    return hashlib.sha256(path.read_bytes()).hexdigest()


def check_binding(item: object, expected_path: str, expected_sha: str) -> bool:
    if not isinstance(item, dict) or item != {"path": expected_path, "sha256": expected_sha}:
        return False
    path = ROOT / expected_path
    return path.is_file() and sha(path) == expected_sha


def parse_process_output(text: str) -> dict[str, int]:
    prefix = "gap_actual_connected_or_predicted_gap_closed:940,525:"
    if not text.startswith(prefix) or not text.endswith("\n") or text.count("\n") != 1:
        raise ValueError("diagnostic_output_shape")
    result: dict[str, int] = {}
    for item in text[len(prefix):-1].split(":"):
        key, separator, raw = item.partition("=")
        if not separator or key in result:
            raise ValueError("diagnostic_output_field")
        result[key] = int(raw)
    return result


def expected_fields(metrics: dict) -> dict[str, int]:
    mapping = {
        "pixels": "legacy_actual_core_pixels",
        "owned": "all_actual_sublayer_pixels",
        "core_largest": "legacy_actual_core_largest8",
        "source_core_mask": "actual_route_body_source_mask_pixels",
        "source_to_pred_body": "source_mask_ending_predicted_core_or_body",
        "source_to_pred_cap": "source_mask_ending_tagged_predicted_cap",
        "source_to_pred_core": "source_mask_ending_predicted_core",
        "source_to_pred_inner_body": "source_mask_ending_untagged_predicted_inner_body",
        "discarded_pred_cap": "tagged_predicted_inner_cap_discards",
        "final_actual_body": "final_actual_route_body_pixels",
        "final_actual_tick": "final_actual_tick_pixels",
        "largest": "all_actual_sublayer_largest8",
    }
    result: dict[str, int] = {}
    for prefix, palette in (("std", "standard"), ("cvd", "color_vision_safe")):
        for suffix, metric in mapping.items():
            result[f"{prefix}_{suffix}"] = metrics["palettes"][palette][metric]
    return result


def validate_patch_roundtrip(errors: list[str]) -> None:
    with tempfile.TemporaryDirectory() as directory:
        temp = Path(directory)
        for role, (baseline_rel, candidate_rel, baseline_sha, candidate_sha) in SOURCE_PAIRS.items():
            baseline = ROOT / baseline_rel
            candidate = ROOT / candidate_rel
            target = temp / role
            shutil.copyfile(baseline, target)
            patch_text = "".join(difflib.unified_diff(
                baseline.read_text().splitlines(keepends=True),
                candidate.read_text().splitlines(keepends=True),
                fromfile="a", tofile="b"))
            forward = subprocess.run(["/usr/bin/patch", "--silent", str(target)],
                input=patch_text, text=True, capture_output=True)
            if forward.returncode or sha(target) != candidate_sha:
                errors.append(f"candidate_forward_patch_invalid:{role}")
                continue
            reverse = subprocess.run(["/usr/bin/patch", "--silent", "-R", str(target)],
                input=patch_text, text=True, capture_output=True)
            if reverse.returncode or sha(target) != baseline_sha:
                errors.append(f"candidate_reverse_patch_invalid:{role}")


def read_link_manifest(path: Path, errors: list[str]) -> list[Path]:
    result: list[Path] = []
    lines = path.read_text().splitlines() if path.is_file() else []
    if len(lines) != 47:
        errors.append("inherited_link_input_count_invalid")
        return result
    for index, line in enumerate(lines):
        expected_hash, separator, relative = line.partition("  ")
        target = ROOT / relative
        if not separator or len(expected_hash) != 64 or not target.is_file() or sha(target) != expected_hash:
            errors.append(f"inherited_link_input_invalid:{index}")
        result.append(target)
    return result


def recompile_and_replay(contract: dict, frozen_output: str,
        inherited: list[Path], errors: list[str]) -> None:
    with tempfile.TemporaryDirectory() as directory:
        temp = Path(directory)
        built: list[Path] = []
        for role in ("diagnostic_source_compile", "candidate_renderer_compile"):
            item = contract[role]
            output = temp / ("diagnostic.o" if role.startswith("diagnostic") else "renderer.o")
            command = ["clang++", *item["arguments"], item["source"], "-o", str(output)]
            result = subprocess.run(command, cwd=ROOT, capture_output=True, text=True)
            if result.returncode or not output.is_file() or sha(output) != item["expected_object_sha256"]:
                errors.append(f"recompile_invalid:{role}")
                return
            built.append(output)
        direct = [ROOT / item["path"] for item in contract["direct_link_inputs"]]
        output = temp / "diagnostic"
        link = subprocess.run(["clang++", *map(str, built), *map(str, direct),
            *map(str, inherited), "-o", str(output)], cwd=ROOT,
            capture_output=True, text=True)
        if link.returncode:
            errors.append("relink_invalid")
            return
        replay = subprocess.run([str(output)], cwd=ROOT, capture_output=True, text=True)
        if replay.returncode != 1 or replay.stdout != "" or replay.stderr != frozen_output:
            errors.append("relinked_replay_invalid")


def validate(path: Path, *, recompile: bool = False) -> list[str]:
    errors: list[str] = []
    try:
        value = json.loads(path.read_text())
    except Exception as exc:
        return [f"receipt_invalid:{exc}"]
    if value.get("schema") != SCHEMA:
        errors.append("schema_invalid")
    if value.get("status") != STATUS or value.get("scope") != "CPU_ONLY_NO_GPU_NO_AUTHORIZATION":
        errors.append("status_or_scope_invalid")

    lineage = value.get("successor_lineage", {})
    if not check_binding(lineage.get("v1_receipt"),
            "docs/art/phase4-v14-cap-mask-premise-falsification-v1/premise-falsification-receipt-v1.json",
            "a7ee84eae3622b4e9dc472ed1dccdd8c2d6604d0218501e9cafc019473baa76e"):
        errors.append("v1_lineage_invalid")
    if lineage.get("v1_immutable") is not True or lineage.get("v1_claimed_diagnostic_source_sha256") != EXPECTED_SOURCE_SHA:
        errors.append("v1_source_claim_invalid")

    evidence = value.get("reproducible_evidence", {})
    candidates = evidence.get("candidate_sources", {})
    baselines = value.get("required_restored_production_baselines", {})
    if set(candidates) != set(SOURCE_PAIRS) or set(baselines) != set(SOURCE_PAIRS):
        errors.append("candidate_or_baseline_roles_invalid")
    for role, (baseline_rel, candidate_rel, baseline_sha, candidate_sha) in SOURCE_PAIRS.items():
        if not check_binding(baselines.get(role), baseline_rel, baseline_sha):
            errors.append(f"baseline_binding_invalid:{role}")
        if not check_binding(candidates.get(role), candidate_rel, candidate_sha):
            errors.append(f"candidate_source_binding_invalid:{role}")

    fixed = {
        "diagnostic_source": ("docs/art/phase4-v14-cap-mask-premise-falsification-v2/phase4_v14_cap_mask_cpu_diagnostic.cpp", EXPECTED_SOURCE_SHA),
        "diagnostic_object": ("build-phase4-v14-mask-cpu/v12_test.o", "4a969fa48341d83aaf2bc641a36c90d3905253fad0286b658c24a3890c57a57e"),
        "candidate_renderer_object": ("build-phase4-v14-mask-cpu/renderer.o", "d4b222e6bc9149d38a03cdf96e79cd135eb1f6b8cd42b6b484c5cdbaf669cffe"),
        "diagnostic_binary": ("build-phase4-v14-mask-cpu/phase4_v14_actual_core_mask_broadcast_cpu_tests", "7eb7309f2588984c7cec7fd1aa734faff55d49063363572cae070350e445247f"),
        "structured_metrics": ("docs/art/phase4-v14-cap-mask-premise-falsification-v2/diagnostic-metrics-v1.json", "bff7abdf904f1baef9f39e6637ae135b54e4b821d12e50dc0f6a03fc1a935821"),
        "compile_contract": ("docs/art/phase4-v14-cap-mask-premise-falsification-v2/compile-contract-v1.json", "13afbfc26a27e392bea49b5c5f2bb8f1754c30c577f552b4bd470e2afe66e013"),
    }
    for role, (relative, digest) in fixed.items():
        if not check_binding(evidence.get(role), relative, digest):
            errors.append(f"evidence_binding_invalid:{role}")
    output_item = evidence.get("frozen_process_output", {})
    output_expected = {"path": "docs/art/phase4-v14-cap-mask-premise-falsification-v2/diagnostic-stderr-v1.txt", "sha256": "651e015d31f48e85d0d0f037578c33339cd105bb9a87cd4342970f9115f71a21", "stream": "stderr"}
    if output_item != output_expected or sha(ROOT / output_expected["path"]) != output_expected["sha256"]:
        errors.append("frozen_process_output_binding_invalid")
    if evidence.get("expected_exit_code") != 1 or evidence.get("binary_replay_recomputes_metrics") is not True or evidence.get("bit_reproducible_relink_claimed") is not False:
        errors.append("replay_contract_invalid")

    if value.get("exact_fixture") != EXPECTED_FIXTURE:
        errors.append("fixture_invalid")
    if value.get("premise_falsification") != EXPECTED_PREMISE:
        errors.append("premise_invalid")
    auth = value.get("authorization", {})
    if set(auth) != {"production_edit_authorized", "final_build_authorized", "readiness_authorized", "gpu_authorized", "machine_or_ta_acceptance"} or set(auth.values()) != {False}:
        errors.append("authorization_invalid")

    metrics_path = ROOT / "docs/art/phase4-v14-cap-mask-premise-falsification-v2/diagnostic-metrics-v1.json"
    output_path = ROOT / "docs/art/phase4-v14-cap-mask-premise-falsification-v2/diagnostic-stderr-v1.txt"
    try:
        metrics = json.loads(metrics_path.read_text())
        fields = parse_process_output(output_path.read_text())
        if metrics.get("schema") != "billiardgl.phase4.v14.cap_mask_cpu_diagnostic_metrics.v1" or metrics.get("exit_code") != 1 or metrics.get("blocking_coordinate") != [940, 525] or metrics.get("neighborhood_radius_px") != 8 or metrics.get("sample_route") != "single_sample_center_0.5_0.5" or fields != expected_fields(metrics):
            errors.append("structured_metrics_semantics_invalid")
    except Exception as exc:
        errors.append(f"structured_metrics_parse_invalid:{exc}")

    validate_patch_roundtrip(errors)
    contract_path = ROOT / "docs/art/phase4-v14-cap-mask-premise-falsification-v2/compile-contract-v1.json"
    try:
        contract = json.loads(contract_path.read_text())
        manifest_item = contract["inherited_link_inputs"]
        manifest = ROOT / manifest_item["path"]
        if sha(manifest) != manifest_item["sha256"] or manifest_item["ordered_count"] != 47:
            errors.append("link_manifest_binding_invalid")
        inherited = read_link_manifest(manifest, errors)
        for index, item in enumerate(contract["direct_link_inputs"]):
            target = ROOT / item["path"]
            if not target.is_file() or sha(target) != item["sha256"]:
                errors.append(f"direct_link_input_invalid:{index}")
        if contract.get("compiler", {}).get("version_first_line") != "Apple clang version 17.0.0 (clang-1700.6.4.2)" or contract.get("reproduction_contract", {}).get("relinked_binary_sha_is_not_required") is not True:
            errors.append("compile_contract_semantics_invalid")
    except Exception as exc:
        errors.append(f"compile_contract_parse_invalid:{exc}")
        inherited = []

    binary = ROOT / evidence.get("diagnostic_binary", {}).get("path", "")
    if binary.is_file() and output_path.is_file():
        replay = subprocess.run([str(binary)], cwd=ROOT, capture_output=True, text=True)
        if replay.returncode != 1 or replay.stdout != "" or replay.stderr != output_path.read_text():
            errors.append("frozen_binary_replay_invalid")
    if recompile and not errors:
        recompile_and_replay(contract, output_path.read_text(), inherited, errors)
    return errors


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--receipt", type=Path, default=DEFAULT)
    parser.add_argument("--skip-recompile", action="store_true")
    args = parser.parse_args()
    errors = validate(args.receipt, recompile=not args.skip_recompile)
    print(json.dumps({"status": "FAIL" if errors else "PASS", "errors": errors,
        "cpu_recompile": "SKIPPED" if args.skip_recompile else "RUN",
        "gpu": "NOT_RUN"}, sort_keys=True))
    return 1 if errors else 0


if __name__ == "__main__":
    raise SystemExit(main())
