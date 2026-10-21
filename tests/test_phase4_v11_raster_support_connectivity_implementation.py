#!/usr/bin/env python3
"""CPU/static fail-closed tests for Phase4 v11 topology correction."""

from __future__ import annotations

import json
from pathlib import Path
import shutil
import subprocess
import tempfile
import unittest

ROOT = Path(__file__).resolve().parents[1]
VALIDATOR = ROOT / "scripts/validate_phase4_v11_raster_support_connectivity_implementation.py"
SOURCES = [
    "src/Billiards/training/final_semantic_ownership.cpp",
    "src/Billiards/training/final_semantic_ownership.h",
    "src/Billiards/training/trajectory_compound_visibility.cpp",
    "src/Billiards/training/trajectory_compound_visibility.h",
    "src/Billiards/training/phase4_final_ownership_instrumentation.cpp",
    "src/Billiards/training/phase4_final_ownership_instrumentation.h",
    "src/Billiards/render/modern/modern_renderer.cpp",
    "src/Billiards/training/trajectory_overlay_renderer.cpp",
    "src/Billiards/training/trajectory_overlay_renderer.h",
    "scripts/generate_phase4_overlay_v4_gpu_runner.py",
    "tests/phase4_final_semantic_ownership_tests.cpp",
    "tests/phase4_final_ownership_telemetry_tests.cpp",
]


def largest_eight_connected(pixels: set[tuple[int, int]]) -> int:
    remaining = set(pixels)
    largest = 0
    while remaining:
        stack = [remaining.pop()]
        size = 0
        while stack:
            x, y = stack.pop()
            size += 1
            for dy in (-1, 0, 1):
                for dx in (-1, 0, 1):
                    neighbor = x + dx, y + dy
                    if neighbor in remaining:
                        remaining.remove(neighbor)
                        stack.append(neighbor)
        largest = max(largest, size)
    return largest


def topology_visible(exclusive_area: float,
                     support: set[tuple[int, int]]) -> bool:
    return exclusive_area >= 1.5 and largest_eight_connected(support) >= 2


class Phase4V11RasterSupportImplementationTests(unittest.TestCase):
    def run_validator(self, root: Path = ROOT) -> subprocess.CompletedProcess:
        return subprocess.run([
            "python3", str(VALIDATOR), "--source-root", str(root)],
            cwd=ROOT, text=True, stdout=subprocess.PIPE,
            stderr=subprocess.STDOUT, check=False)

    def mutate_source(self, relative: str, old: str, new: str) -> str:
        with tempfile.TemporaryDirectory(prefix="phase4-v11-topology-") as temp:
            root = Path(temp)
            for source in SOURCES:
                target = root / source
                target.parent.mkdir(parents=True, exist_ok=True)
                shutil.copy2(ROOT / source, target)
            path = root / relative
            text = path.read_text()
            self.assertIn(old, text)
            path.write_text(text.replace(old, new, 1))
            result = self.run_validator(root)
            self.assertNotEqual(result.returncode, 0, result.stdout)
            return result.stdout

    def test_canonical_passes(self) -> None:
        result = self.run_validator()
        self.assertEqual(result.returncode, 0, result.stdout)
        value = json.loads(result.stdout)
        self.assertEqual(value["status"], "PASS")
        self.assertFalse(value["capture_or_readiness_authorized"])

    def test_diagonal_pair_is_raster_continuous(self) -> None:
        self.assertTrue(topology_visible(2.0, {(10, 10), (11, 11)}))

    def test_horizontal_and_vertical_pairs_are_continuous(self) -> None:
        self.assertTrue(topology_visible(2.0, {(10, 10), (11, 10)}))
        self.assertTrue(topology_visible(2.0, {(10, 10), (10, 11)}))

    def test_isolated_scatter_is_not_continuous(self) -> None:
        self.assertFalse(topology_visible(2.0, {(10, 10), (12, 12)}))

    def test_connected_support_cannot_rescue_low_area(self) -> None:
        self.assertFalse(topology_visible(1.49, {(10, 10), (11, 11)}))

    def test_eight_neighbor_implementation_mutation_fails(self) -> None:
        output = self.mutate_source(
            "src/Billiards/training/final_semantic_ownership.cpp",
            "largestEightConnectedComponent", "largestDisabledComponent")
        self.assertIn("source_drift:src/Billiards/training/final_semantic_ownership.cpp", output)

    def test_area_threshold_mutation_fails(self) -> None:
        output = self.mutate_source(
            "src/Billiards/training/final_semantic_ownership.h",
            "minimumResolvedPixelEquivalentArea = 1.5",
            "minimumResolvedPixelEquivalentArea = 1.0")
        self.assertIn("source_drift:src/Billiards/training/final_semantic_ownership.h", output)

    def test_reason_enum_mutation_fails(self) -> None:
        output = self.mutate_source(
            "src/Billiards/training/trajectory_compound_visibility.h",
            "InsufficientRasterContinuity", "CollapsedAreaReason")
        self.assertIn("source_drift:src/Billiards/training/trajectory_compound_visibility.h", output)

    def test_topology_telemetry_mutation_fails(self) -> None:
        output = self.mutate_source(
            "src/Billiards/training/phase4_final_ownership_instrumentation.cpp",
            'value["pre_arbitration_support_pixel_count"]',
            'value["removed_support_pixel_count"]')
        self.assertIn("source_drift:src/Billiards/training/phase4_final_ownership_instrumentation.cpp", output)

    def test_required_fixture_inventory_mutation_fails(self) -> None:
        output = self.mutate_source(
            "tests/phase4_final_semantic_ownership_tests.cpp",
            "TOPO_N6_REASON_SPLIT", "REMOVED_REASON_FIXTURE")
        self.assertIn("source_drift:tests/phase4_final_semantic_ownership_tests.cpp", output)

    def test_visual_source_mutation_fails(self) -> None:
        output = self.mutate_source(
            "src/Billiards/training/trajectory_overlay_renderer.cpp",
            "namespace billiardgl::training", "namespace mutated::training")
        self.assertIn("source_drift:src/Billiards/training/trajectory_overlay_renderer.cpp", output)

    def test_frozen_replay_remains_non_final(self) -> None:
        result = self.run_validator()
        value = json.loads(result.stdout)
        self.assertFalse(value["frozen_counterfactual_is_machine_pass"])
        self.assertEqual(value["cpu_fixture_count"], 34)


if __name__ == "__main__":
    unittest.main()
