from __future__ import annotations

import hashlib
import json
import tempfile
import unittest
from pathlib import Path

from scripts.validate_phase2_production_visuals import (
    CAPTURE_ASSET_HASHES,
    MANUAL_AXES,
    ReviewError,
    SCENE_IDS,
    edge_density,
    evaluate_performance,
    evaluate_check,
    load_capture_manifest,
    read_ppm,
    scalar_metrics,
    sha256,
    validate_manual_review,
)


def ppm(width: int, height: int, pixels: bytes) -> bytes:
    return f"P6\n{width} {height}\n255\n".encode() + pixels


class Phase2ProductionVisualTests(unittest.TestCase):
    def test_ppm_reader_accepts_rgb8(self) -> None:
        with tempfile.TemporaryDirectory() as directory:
            path = Path(directory) / "image.ppm"
            path.write_bytes(ppm(2, 1, bytes([255, 0, 0, 0, 255, 0])))
            self.assertEqual(read_ppm(path), (2, 1, bytes([255, 0, 0, 0, 255, 0])))

    def test_ppm_reader_rejects_incomplete_pixels(self) -> None:
        with tempfile.TemporaryDirectory() as directory:
            path = Path(directory) / "image.ppm"
            path.write_bytes(ppm(2, 1, bytes([255, 0, 0])))
            with self.assertRaises(ReviewError):
                read_ppm(path)

    def test_scalar_metrics_report_black_and_white_clipping(self) -> None:
        pixels = bytes([0, 0, 0, 255, 255, 255])
        actual = scalar_metrics(pixels, 2, 1, (0, 0, 2, 1))
        self.assertEqual(actual["non_black_fraction"], 0.5)
        self.assertEqual(actual["highlight_clip_fraction"], 0.5)
        self.assertEqual(actual["shadow_clip_fraction"], 0.5)

    def test_sobel_edge_density_detects_checkerboard(self) -> None:
        pixels = bytearray()
        for y in range(10):
            for x in range(10):
                value = 255 if (x // 2 + y // 2) % 2 else 0
                pixels.extend((value, value, value))
        self.assertGreater(edge_density(bytes(pixels), 10, (0, 0, 10, 10)), 0.4)

    def test_threshold_evaluation_does_not_accept_a_value_below_minimum(self) -> None:
        actual = {"edge_density": 0.019}
        result = evaluate_check(actual, {"minimum_edge_density": 0.02})
        self.assertFalse(result[0]["passed"])
        self.assertEqual(result[0]["threshold"], 0.02)

    def capture_manifest(self) -> dict:
        value = {
            "renderer": "modern", "production_gltf": True,
            "resolution": {"width": 1920, "height": 1080},
            "quality": "medium",
            "performance": {
                "draw_calls_complete_scene": 100,
                "measurement_method": "gpu_timer_query_plus_cpu_frame",
                "p95_total_frame_ms": 16.7,
                "sample_frames": 300,
                "texture_resident_mib": 256,
                "warmup_frames": 120,
            },
            "scenes": [{"scene_id": scene, "file": f"{scene}.ppm"} for scene in SCENE_IDS],
        }
        value.update({
            field: sha256(asset_path).removeprefix("sha256:")
            for field, asset_path in CAPTURE_ASSET_HASHES.items()
        })
        return value

    def self_contained_manifest(self, directory: Path) -> tuple[dict, Path]:
        value = self.capture_manifest()
        value["asset_root"] = "asset_root"
        asset_root = directory / "asset_root"
        asset_root.mkdir()
        for field, source in CAPTURE_ASSET_HASHES.items():
            target = asset_root / source.name
            target.write_bytes(source.read_bytes())
            value[field] = sha256(target).removeprefix("sha256:")
        return value, asset_root

    def test_capture_manifest_requires_production_gltf_attestation(self) -> None:
        value = self.capture_manifest(); value["production_gltf"] = False
        with tempfile.TemporaryDirectory() as directory:
            path = Path(directory) / "manifest.json"; path.write_text(json.dumps(value))
            with self.assertRaises(ReviewError):
                load_capture_manifest(path)

    def test_capture_manifest_requires_exact_six_scenes(self) -> None:
        value = self.capture_manifest(); value["scenes"].pop()
        with tempfile.TemporaryDirectory() as directory:
            path = Path(directory) / "manifest.json"; path.write_text(json.dumps(value))
            with self.assertRaises(ReviewError):
                load_capture_manifest(path)

    def test_capture_manifest_requires_ball_atlas_hash_binding(self) -> None:
        value = self.capture_manifest(); del value["ball_atlas_sha256"]
        with tempfile.TemporaryDirectory() as directory:
            path = Path(directory) / "manifest.json"; path.write_text(json.dumps(value))
            with self.assertRaisesRegex(ReviewError, "ball_atlas_sha256"):
                load_capture_manifest(path)

    def test_capture_manifest_requires_asset_manifest_hash_binding(self) -> None:
        value = self.capture_manifest(); del value["asset_manifest_sha256"]
        with tempfile.TemporaryDirectory() as directory:
            path = Path(directory) / "manifest.json"; path.write_text(json.dumps(value))
            with self.assertRaisesRegex(ReviewError, "asset_manifest_sha256"):
                load_capture_manifest(path)

    def test_capture_manifest_rejects_hash_that_does_not_match_asset_file(self) -> None:
        value = self.capture_manifest(); value["ball_atlas_sha256"] = "0" * 64
        with tempfile.TemporaryDirectory() as directory:
            path = Path(directory) / "manifest.json"; path.write_text(json.dumps(value))
            with self.assertRaisesRegex(ReviewError, "does not match"):
                load_capture_manifest(path)

    def test_capture_manifest_accepts_self_contained_asset_root(self) -> None:
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            value, asset_root = self.self_contained_manifest(root)
            path = root / "manifest.json"; path.write_text(json.dumps(value))
            _, _, resolved = load_capture_manifest(path)
            self.assertEqual(
                resolved["modular_gltf_sha256"],
                asset_root.resolve() / "focused_practice_hall_modular_v1.gltf")

    def test_capture_manifest_rejects_asset_root_path_escape(self) -> None:
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            value, _ = self.self_contained_manifest(root)
            value["asset_root"] = "../asset_root"
            path = root / "manifest.json"; path.write_text(json.dumps(value))
            with self.assertRaisesRegex(ReviewError, "constrained relative path"):
                load_capture_manifest(path)

    def test_capture_manifest_rejects_missing_self_contained_asset(self) -> None:
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            value, asset_root = self.self_contained_manifest(root)
            (asset_root / "focused_practice_hall_venue_v1.gltf").unlink()
            path = root / "manifest.json"; path.write_text(json.dumps(value))
            with self.assertRaisesRegex(ReviewError, "attested asset is missing"):
                load_capture_manifest(path)

    def test_capture_manifest_rejects_tampered_self_contained_asset(self) -> None:
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            value, asset_root = self.self_contained_manifest(root)
            target = asset_root / "focused_practice_hall_modular_v1.gltf"
            target.write_bytes(target.read_bytes() + b"tamper")
            path = root / "manifest.json"; path.write_text(json.dumps(value))
            with self.assertRaisesRegex(ReviewError, "does not match"):
                load_capture_manifest(path)

    def test_performance_reports_over_budget_p95_without_changing_threshold(self) -> None:
        performance = self.capture_manifest()["performance"]
        performance["p95_total_frame_ms"] = 16.71
        result = {item["metric"]: item for item in evaluate_performance(performance)}
        self.assertFalse(result["p95_total_frame_ms"]["passed"])
        self.assertEqual(result["p95_total_frame_ms"]["threshold"], 16.7)

    def test_performance_reports_over_budget_draw_calls(self) -> None:
        performance = self.capture_manifest()["performance"]
        performance["draw_calls_complete_scene"] = 101
        result = {item["metric"]: item for item in evaluate_performance(performance)}
        self.assertFalse(result["draw_calls_complete_scene"]["passed"])
        self.assertEqual(result["draw_calls_complete_scene"]["threshold"], 100)

    def test_manual_review_requires_screenshot_hash_binding(self) -> None:
        hashes = {scene: "sha256:" + hashlib.sha256(scene.encode()).hexdigest() for scene in SCENE_IDS}
        value = {
            "schema": "phase2_ta_manual_review_v1", "reviewer_role": "independent_technical_artist",
            "scenes": {scene: {"screenshot_sha256": hashes[scene], "axes": {
                axis: {"status": "PASS", "evidence": "inspected at native resolution"} for axis in MANUAL_AXES
            }} for scene in SCENE_IDS},
        }
        validate_manual_review(value, hashes)
        value["scenes"]["break"]["screenshot_sha256"] = "sha256:wrong"
        with self.assertRaises(ReviewError):
            validate_manual_review(value, hashes)


if __name__ == "__main__":
    unittest.main()
