#!/usr/bin/env python3
"""Fail-closed validator for the CPU-only Phase4 V15 provenance receipt."""

from __future__ import annotations

import hashlib
import json
import os
from pathlib import Path
import shutil
import subprocess
import tempfile
from typing import Any

ROOT = Path(__file__).resolve().parents[1]
EVIDENCE = ROOT / "docs/art/phase4-v15-broadcast-overwrite-provenance-diagnostic-v1"
RECEIPT = EVIDENCE / "diagnostic-receipt-v1.json"


def sha256(path: Path) -> str:
    digest = hashlib.sha256()
    with path.open("rb") as stream:
        for block in iter(lambda: stream.read(1024 * 1024), b""):
            digest.update(block)
    return digest.hexdigest()


def load_json(path: Path) -> Any:
    with path.open("r", encoding="utf-8") as stream:
        return json.load(stream)


def canonical_topology(palette: dict[str, Any]) -> list[tuple[Any, ...]]:
    return [
        (
            tuple(row["physical_pixel"]),
            row.get("prior_actual_source_probe_id"),
            row.get("prior_actual_primitive_id"),
            row.get("final_predicted_source_probe_id"),
            row.get("final_predicted_primitive_id"),
            row.get("predicted_fragment_class"),
            row.get("semantic_interval_class"),
            row.get("dash_ordinal_and_local_tangent_parameter", {}).get("ordinal"),
            row.get("dash_ordinal_and_local_tangent_parameter", {}).get("t"),
            row.get("is_protected_predicted_body_or_lateral"),
            row.get("would_be_candidate_for_future_gap_only_non_overwrite"),
        )
        for row in palette["samples"]
    ]


def largest8(points: set[tuple[int, int]]) -> int:
    largest = 0
    remaining = set(points)
    while remaining:
        stack = [remaining.pop()]
        size = 0
        while stack:
            x, y = stack.pop()
            size += 1
            for dy in (-1, 0, 1):
                for dx in (-1, 0, 1):
                    if dx == 0 and dy == 0:
                        continue
                    candidate = (x + dx, y + dy)
                    if candidate in remaining:
                        remaining.remove(candidate)
                        stack.append(candidate)
        largest = max(largest, size)
    return largest


def semantic_errors(receipt: dict[str, Any], ledger: dict[str, Any]) -> list[str]:
    errors: list[str] = []
    required_fields = {
        "palette", "physical_pixel", "sample_index_and_position",
        "prior_actual_source_probe_id", "prior_actual_primitive_id",
        "prior_actual_route_body_boolean",
        "final_predicted_source_probe_id", "final_predicted_primitive_id",
        "predicted_fragment_class", "semantic_interval_class",
        "dash_ordinal_and_local_tangent_parameter",
        "is_protected_predicted_body_or_lateral",
        "would_be_candidate_for_future_gap_only_non_overwrite",
    }
    fragments = {
        "core_base_body", "core_start_cap_extension",
        "core_end_cap_extension", "inner_central_body",
        "inner_lateral_annulus", "inner_start_cap_annulus",
        "inner_end_cap_annulus", "outer_side_band",
        "endpoint_or_tick_or_marker",
    }
    intervals = {
        "dash_on_interior", "dash_gap_intrusion",
        "route_endpoint_envelope",
        "cross_segment_or_cross_source_overlap",
    }
    if ledger.get("schema") != \
            "billiardgl.phase4.v15.broadcast_overwrite_provenance.v1":
        errors.append("ledger_schema_invalid")
    if ledger.get("capture_id") != "camera_broadcast_1080":
        errors.append("capture_id_invalid")
    if ledger.get("ordered_source_count") != 81 or \
            ledger.get("ordered_source_digest") != \
            "33511aa46717a208f3db43acd278416b8fe8393e2e003484da8f44dabffe5645":
        errors.append("source_inventory_invalid")
    palettes = ledger.get("palettes", [])
    if [item.get("palette") for item in palettes] != \
            ["standard", "color_vision_safe"]:
        errors.append("palette_inventory_invalid")
        return errors
    for palette in palettes:
        if palette.get("blocking_predicted_core") != 81 or \
                palette.get("blocking_inner_body") != 7:
            errors.append(f"blocking_partition_invalid:{palette.get('palette')}")
        if any(palette.get(key) != 0 for key in (
                "foreign_evidence_count", "double_assignment_count",
                "unowned_evidence_count")):
            errors.append(f"ownership_error_nonzero:{palette.get('palette')}")
        rows = palette.get("samples", [])
        for index, row in enumerate(rows):
            if not required_fields.issubset(row):
                errors.append(f"sample_fields_missing:{palette.get('palette')}:{index}")
                break
            if row.get("palette") != palette.get("palette"):
                errors.append(f"sample_palette_drift:{palette.get('palette')}:{index}")
                break
            if row.get("sample_index_and_position") != \
                    {"index": 0, "position": [0.5, 0.5]}:
                errors.append(f"sample_position_invalid:{palette.get('palette')}:{index}")
                break
            if row.get("prior_actual_route_body_boolean") is not True:
                errors.append(f"route_body_boolean_invalid:{palette.get('palette')}:{index}")
                break
            if row.get("predicted_fragment_class") not in fragments or \
                    row.get("semantic_interval_class") not in intervals:
                errors.append(f"classification_invalid:{palette.get('palette')}:{index}")
                break
        repairs = palette.get("repair_probe_owner_chains", [])
        expected_repairs = [
            "d6d850a1a10c7c7ce403e6de4afd4330e4c6e89b9100c72f4c23fbab1f6f978b",
            "1890fd0652c8f74a8b03a35a6cee1ad2cab43062987cab73efb1f6b455da468a",
            "1bf60bcd841a0d754c82876b55a4d3d6c29ebad582e069d4f4a608a4778c33a9",
        ]
        if [row.get("repair_probe_id") for row in repairs] != expected_repairs:
            errors.append(f"repair_chain_inventory_invalid:{palette.get('palette')}")
        elif any(not row.get("final_owner_kind") for row in repairs):
            errors.append(f"repair_chain_owner_missing:{palette.get('palette')}")

        exact_gap = {(783, 413): (84, 18, 18),
                     (892, 491): (96, 1, 1),
                     (940, 525): (88, 0, 0)}
        for (cx, cy), (region_count, eligible_count, expected_largest) in \
                exact_gap.items():
            region = [row for row in rows
                      if abs(row["physical_pixel"][0] - cx) <= 8 and
                      abs(row["physical_pixel"][1] - cy) <= 8]
            eligible = {
                tuple(row["physical_pixel"]) for row in region
                if row["would_be_candidate_for_future_gap_only_non_overwrite"]
            }
            if len(region) != region_count or len(eligible) != eligible_count \
                    or largest8(eligible) != expected_largest:
                errors.append(f"gap_partition_invalid:{palette.get('palette')}:{cx},{cy}")
        blocking = [row for row in rows
                    if abs(row["physical_pixel"][0] - 940) <= 8 and
                    abs(row["physical_pixel"][1] - 525) <= 8]
        fragment_counts: dict[str, int] = {}
        interval_counts: dict[str, int] = {}
        for row in blocking:
            fragment_counts[row["predicted_fragment_class"]] = \
                fragment_counts.get(row["predicted_fragment_class"], 0) + 1
            interval_counts[row["semantic_interval_class"]] = \
                interval_counts.get(row["semantic_interval_class"], 0) + 1
        expected_fragments = {"core_base_body": 48,
            "core_start_cap_extension": 28, "core_end_cap_extension": 5,
            "inner_start_cap_annulus": 4, "inner_end_cap_annulus": 2,
            "inner_lateral_annulus": 1}
        if fragment_counts != expected_fragments:
            errors.append(f"blocking_fragment_partition_invalid:{palette.get('palette')}")
        if interval_counts != {
                "cross_segment_or_cross_source_overlap": 83,
                "dash_on_interior": 5}:
            errors.append(f"blocking_interval_partition_invalid:{palette.get('palette')}")
    if canonical_topology(palettes[0]) != canonical_topology(palettes[1]):
        errors.append("standard_cvd_topology_not_identical")
    conclusion = receipt.get("diagnostic_conclusion", {})
    if conclusion.get("classification") != "INFEASIBLE_UNDER_FROZEN_SEMANTICS" \
            or conclusion.get("visual_fix_authorized") is not False:
        errors.append("conclusion_not_fail_closed")
    return errors


def patch_round_trip(errors: list[str]) -> None:
    pairs = [
        (ROOT / "src/Billiards/training/trajectory_overlay_renderer.cpp",
         EVIDENCE / "candidate-sources/src/Billiards/training/trajectory_overlay_renderer.cpp",
         EVIDENCE / "trajectory-renderer-cpp-v15.patch"),
        (ROOT / "src/Billiards/training/trajectory_overlay_renderer.h",
         EVIDENCE / "candidate-sources/src/Billiards/training/trajectory_overlay_renderer.h",
         EVIDENCE / "trajectory-renderer-h-v15.patch"),
        (ROOT / "CMakeLists.txt", EVIDENCE / "candidate-sources/CMakeLists.txt",
         EVIDENCE / "CMakeLists-v15.patch"),
    ]
    with tempfile.TemporaryDirectory(prefix="phase4-v15-patch-") as raw:
        temp = Path(raw)
        for index, (baseline, candidate, patch_path) in enumerate(pairs):
            work = temp / f"source-{index}"
            shutil.copyfile(baseline, work)
            forward = subprocess.run(["patch", "--silent", work, patch_path],
                stdout=subprocess.PIPE, stderr=subprocess.PIPE)
            if forward.returncode != 0 or work.read_bytes() != candidate.read_bytes():
                errors.append(f"patch_forward_invalid:{patch_path.name}")
                continue
            reverse = subprocess.run(["patch", "--silent", "-R", work, patch_path],
                stdout=subprocess.PIPE, stderr=subprocess.PIPE)
            if reverse.returncode != 0 or work.read_bytes() != baseline.read_bytes():
                errors.append(f"patch_reverse_invalid:{patch_path.name}")


def replay_reproducible_binary(errors: list[str]) -> None:
    contract = load_json(EVIDENCE / "compile-contract-v1.json")
    with tempfile.TemporaryDirectory(prefix="phase4-v15-replay-") as raw:
        temp = Path(raw)
        common = ["/usr/bin/c++",
            '-DBILLIARDGL_ASSET_ROOT="' + str(ROOT / "src/Billiards") + '"',
            "-DBILLIARDGL_PHASE4_V15_PROVENANCE_DIAGNOSTIC=1",
            '-DBILLIARDGL_SOURCE_ROOT="' + str(ROOT) + '"',
            "-I" + str(ROOT),
            "-I" + str(EVIDENCE / "candidate-sources/src/Billiards"),
            "-I" + str(ROOT / "src/Billiards"),
            "-O3", "-DNDEBUG", "-std=c++17", "-arch", "arm64", "-c"]
        test_object = temp / "test.o"
        renderer_object = temp / "renderer.o"
        compile_jobs = [
            (EVIDENCE / "phase4_v15_broadcast_overwrite_provenance_cpu_tests.cpp",
             test_object, contract["diagnostic_test_source"]["expected_object_sha256"]),
            (EVIDENCE / "candidate-sources/src/Billiards/training/trajectory_overlay_renderer.cpp",
             renderer_object, contract["diagnostic_renderer_source"]["expected_object_sha256"]),
        ]
        for source, output, expected in compile_jobs:
            result = subprocess.run(common + [str(source), "-o", str(output)],
                cwd=ROOT, stdout=subprocess.PIPE, stderr=subprocess.PIPE)
            if result.returncode != 0:
                errors.append(f"recompile_failed:{source.name}")
                return
            if sha256(output) != expected:
                errors.append(f"recompiled_object_hash_mismatch:{source.name}")
                return
        manifest = []
        for line in (EVIDENCE / "link-inputs-v1.sha256").read_text().splitlines():
            expected, relative = line.split("  ", 1)
            path = ROOT / relative
            if not path.is_file() or sha256(path) != expected:
                errors.append(f"link_input_invalid:{relative}")
                return
            if path.name not in {"phase4_v15_test.o", "trajectory_overlay_renderer_v15.o"}:
                manifest.append(path)
        binary = temp / "diagnostic"
        link = subprocess.run(["/usr/bin/c++", "-O3", "-DNDEBUG", "-arch", "arm64",
            str(test_object), str(renderer_object), *map(str, manifest), "-o", str(binary)],
            cwd=ROOT, stdout=subprocess.PIPE, stderr=subprocess.PIPE)
        if link.returncode != 0:
            errors.append("relink_failed")
            return
        run = subprocess.run([str(binary)], cwd=ROOT,
            stdout=subprocess.PIPE, stderr=subprocess.PIPE)
        if run.returncode != contract["expected_replay"]["exit_code"]:
            errors.append("replay_exit_invalid")
        if hashlib.sha256(run.stdout).hexdigest() != \
                contract["expected_replay"]["stdout_sha256"]:
            errors.append("replay_stdout_invalid")
        if hashlib.sha256(run.stderr).hexdigest() != \
                contract["expected_replay"]["stderr_sha256"]:
            errors.append("replay_stderr_invalid")


def validate(run_replay: bool = True) -> list[str]:
    errors: list[str] = []
    receipt = load_json(RECEIPT)
    if receipt.get("schema") != \
            "billiardgl.phase4.v15.broadcast_overwrite_provenance_diagnostic_receipt.v1":
        errors.append("receipt_schema_invalid")
    for binding in receipt.get("frozen_bindings", []):
        path = ROOT / binding["path"]
        if not path.is_file() or sha256(path) != binding["sha256"]:
            errors.append(f"frozen_binding_invalid:{binding.get('role')}")
    for artifact in receipt.get("artifacts", []):
        path = ROOT / artifact["path"]
        if not path.is_file() or sha256(path) != artifact["sha256"]:
            errors.append(f"artifact_invalid:{artifact.get('path')}")
    shipping = receipt.get("restored_shipping_bindings", {})
    source_checks = {
        ROOT / "src/Billiards/training/trajectory_overlay_renderer.cpp":
            shipping.get("trajectory_renderer_cpp_sha256"),
        ROOT / "src/Billiards/training/trajectory_overlay_renderer.h":
            shipping.get("trajectory_renderer_h_sha256"),
        ROOT / "src/Billiards/render/modern/modern_renderer.cpp":
            shipping.get("modern_renderer_cpp_sha256"),
        ROOT / "src/Billiards/render/modern/modern_renderer.h":
            shipping.get("modern_renderer_h_sha256"),
        ROOT / "CMakeLists.txt": shipping.get("cmake_sha256"),
    }
    for path, expected in source_checks.items():
        if not path.is_file() or sha256(path) != expected:
            errors.append(f"restored_source_invalid:{path.relative_to(ROOT)}")
    errors.extend(semantic_errors(receipt,
        load_json(EVIDENCE / "provenance-ledger-v1.json")))
    patch_round_trip(errors)
    test_source = (EVIDENCE /
        "phase4_v15_broadcast_overwrite_provenance_cpu_tests.cpp").read_text()
    for required in ("same_stem_core_halo_misclassified_cross",
            "different_stem_must_be_cross", "different_arc_must_be_cross"):
        if required not in test_source:
            errors.append(f"static_fixture_missing:{required}")
    candidate_header = (EVIDENCE /
        "candidate-sources/src/Billiards/training/trajectory_overlay_renderer.h").read_text()
    if "BILLIARDGL_PHASE4_V15_PROVENANCE_DIAGNOSTIC" not in candidate_header:
        errors.append("static_fixture_missing:diagnostic_compile_guard")
    if run_replay and not errors:
        replay_reproducible_binary(errors)
    return errors


def main() -> int:
    errors = validate(run_replay=os.environ.get("PHASE4_V15_SKIP_REPLAY") != "1")
    print(json.dumps({"errors": errors}, sort_keys=True))
    return 0 if not errors else 1


if __name__ == "__main__":
    raise SystemExit(main())
