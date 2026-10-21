"""Reject malformed evidence before native pixel preflight can report success."""

import pathlib
import sys
import json
import tempfile
import subprocess
import unittest
from unittest.mock import patch

sys.path.insert(0, str(pathlib.Path(__file__).resolve().parents[1] / "scripts"))
from run_native_product_pixel_matrix import (
    Ppm, text_run_stats, trajectory_probe_stats, glyph_geometry_stats,
    audit_existing_profile, STATES,
    run_profile,
    validate_profile_receipts,
    validate_image,
    validate_tick_inventory,
)


class NativePixelGeometryTests(unittest.TestCase):
    def setUp(self):
        # Deliberately high-edge background: geometry failures must not be
        # rescued by unrelated scene edges. This is not real glyph evidence.
        pixels = bytearray()
        for _y in range(100):
            for x in range(100):
                pixels.extend((255, 255, 255) if x % 4 < 2 else (0, 0, 0))
        self.image = Ppm(100, 100, pixels)
        self.run = dict(text="sample", requested_px=16, measured_width_px=30,
                        origin_x=10, baseline_y=40, clip_x=10, clip_y=10,
                        clip_width=40, clip_height=50)
        self.probe = dict(track="predicted", component_role=0, x=30, y=30,
                          halo_x=30, halo_y=30, background_x=31, background_y=31)

    def text(self, **changes):
        return text_run_stats(self.image, {"product_ui_text_runs": [
            {**self.run, **changes}]})

    def test_valid_preflight_is_not_glyph_acceptance(self):
        result = self.text()
        self.assertEqual(result["truncated_run_count"], 0)
        self.assertFalse(result["glyph_ink_pixel_contract_verified"])

    def test_offset_text_cannot_escape_either_clip_edge(self):
        for origin in (9, 45):
            with self.subTest(origin=origin), self.assertRaises(ValueError):
                self.text(origin_x=origin)

    def test_invalid_and_offscreen_geometry_rejected(self):
        for field in self.run.keys() - {"text"}:
            with self.subTest(field=field), self.assertRaises(ValueError):
                self.text(**{field: float("nan")})
        for changes in (dict(clip_width=0), dict(clip_height=-1),
                        dict(clip_x=-2), dict(clip_y=90),
                        dict(baseline_y=61), dict(baseline_y=9),
                        dict(measured_width_px=-1), dict(requested_px=12.9)):
            with self.subTest(changes=changes), self.assertRaises(ValueError):
                self.text(**changes)

    def test_outside_probes_cannot_shrink_denominator(self):
        for coordinate in ("x", "y", "halo_x", "halo_y",
                           "background_x", "background_y"):
            for outside in (-1, 100):
                probes = [self.probe] + [
                    {**self.probe, coordinate: outside} for _ in range(3)]
                with self.subTest(coordinate=coordinate, outside=outside), \
                        self.assertRaises(ValueError):
                    trajectory_probe_stats(self.image,
                        {"trajectory_probes": probes}, "predicted")

    def test_all_valid_probes_remain_in_denominator(self):
        result = trajectory_probe_stats(self.image,
            {"trajectory_probes": [self.probe] * 4}, "predicted")
        self.assertEqual(result["source_probe_count"], 4)
        self.assertEqual(result["in_bounds_probe_count"], 4)

    def test_one_bad_probe_cannot_be_diluted_by_good_probes(self):
        receipt = dict(width_px=100, height_px=100, product_ui_draw_calls=1,
                       product_ui_sdf_vertices=100, product_ui_clipped_items=0,
                       actual_tick_count=3)
        stats = dict(source_probe_count=24, visible_fraction=23 / 24,
                     median_contrast=4.642)
        with patch("run_native_product_pixel_matrix.quantized_colors", return_value=200), \
             patch("run_native_product_pixel_matrix.edge_count", return_value=200), \
             patch("run_native_product_pixel_matrix.text_run_stats", return_value={}), \
             patch("run_native_product_pixel_matrix.glyph_geometry_stats", return_value={}), \
             patch("run_native_product_pixel_matrix.trajectory_probe_stats", return_value=stats), \
             self.assertRaises(ValueError):
            validate_image(self.image, receipt, "replay", (100, 100))

    def test_tick_probes_are_measured_separately_from_segment_bodies(self):
        receipt = {"trajectory_probes": [
            {**self.probe, "track": "actual"},
            {**self.probe, "track": "actual", "component_role": 3}]}
        bodies = trajectory_probe_stats(self.image, receipt, "actual")
        ticks = trajectory_probe_stats(self.image, receipt, "actual", component_role=3)
        self.assertEqual(bodies["source_probe_count"], 1)
        self.assertEqual(ticks["source_probe_count"], 1)
        self.assertEqual(ticks["component_role"], 3)

    def test_tick_inventory_cannot_replace_missing_tick_with_duplicate(self):
        probes = [dict(track="actual", component_role=3, halo_valid=True,
                       primitive_id=f"actual_tick_{index}_core") for index in range(3)]
        receipt = dict(actual_tick_count=3, trajectory_probes=probes)
        validate_tick_inventory(receipt)
        for changes in (dict(primitive_id="actual_tick_0_core"),
                        dict(primitive_id="actual_segment_2_core"),
                        dict(halo_valid=False)):
            with self.subTest(changes=changes), self.assertRaises(ValueError):
                validate_tick_inventory({**receipt, "trajectory_probes":
                    probes[:2] + [{**probes[2], **changes}]})
        for count in (True, 3.5, 0):
            with self.subTest(count=count), self.assertRaises(ValueError):
                validate_tick_inventory({**receipt, "actual_tick_count": count})

    def test_visible_bodies_do_not_rescue_missing_or_failed_ticks(self):
        receipt = dict(width_px=100, height_px=100, product_ui_draw_calls=1,
                       product_ui_sdf_vertices=100, product_ui_clipped_items=0,
                       actual_tick_count=3)
        good = dict(source_probe_count=24, visible_fraction=1.0, median_contrast=5.0)
        for bad in (dict(source_probe_count=0, visible_fraction=0, median_contrast=0),
                    dict(source_probe_count=2, visible_fraction=1, median_contrast=5),
                    dict(source_probe_count=24, visible_fraction=23/24, median_contrast=5)):
            with self.subTest(ticks=bad), \
                 patch("run_native_product_pixel_matrix.quantized_colors", return_value=200), \
                 patch("run_native_product_pixel_matrix.edge_count", return_value=200), \
                 patch("run_native_product_pixel_matrix.text_run_stats", return_value={}), \
                 patch("run_native_product_pixel_matrix.glyph_geometry_stats", return_value={}), \
                 patch("run_native_product_pixel_matrix.trajectory_probe_stats", side_effect=[good, bad]), \
                 self.assertRaisesRegex(ValueError, "actual ticks"):
                validate_image(self.image, receipt, "replay", (100, 100))

    def glyph(self, **changes):
        glyph = dict(codepoint=65, text_run_index=0, codepoint_ordinal=0,
                     ink_source_x=12, ink_source_y=20,
                     ink_source_width=6, ink_source_height=9,
                     ink_x=12, ink_y=20, ink_width=6, ink_height=9,
                     clip_x=10, clip_y=10, clip_width=40, clip_height=50)
        return glyph_geometry_stats(self.image,
            {"product_ui_glyph_quads": [{**glyph, **changes}],
             "product_ui_text_runs": [{"text": "A"}]})

    def test_glyph_geometry_does_not_pretend_to_prove_pixel_attribution(self):
        result = self.glyph()
        self.assertEqual(result["clipped_ink_count"], 0)
        self.assertFalse(result["final_pixel_attribution_verified"])
        # A two-pixel full stop is not an undersized font.
        self.glyph(ink_height=2, ink_source_height=2)

    def test_glyph_clipping_before_submission_is_rejected(self):
        for changes in (dict(ink_height=8), dict(ink_width=5),
                        dict(ink_x=13), dict(ink_y=21)):
            with self.subTest(changes=changes), self.assertRaises(ValueError):
                self.glyph(**changes)

    def test_glyph_vertical_clip_and_drawable_limits(self):
        for changes in (dict(clip_y=21), dict(clip_height=18),
                        dict(ink_y=95, ink_source_y=95),
                        dict(ink_x=-1, ink_source_x=-1),
                        dict(ink_width=float("inf")),
                        dict(clip_width=0)):
            with self.subTest(changes=changes), self.assertRaises(ValueError):
                self.glyph(**changes)
        with self.assertRaises(ValueError):
            glyph_geometry_stats(self.image, {})

    def test_missing_or_replaced_whole_glyph_rejected(self):
        with self.assertRaises(ValueError):
            self.glyph(codepoint=66)
        with self.assertRaises(ValueError):
            glyph_geometry_stats(self.image, {
                "product_ui_text_runs": [{"text": "AA"}],
                "product_ui_glyph_quads": [{"codepoint": 65,
                    "text_run_index": 0, "codepoint_ordinal": 0}],
            })

    def test_same_character_cannot_be_credited_to_a_different_run_or_ordinal(self):
        for changes in (dict(text_run_index=1), dict(codepoint_ordinal=1),
                        dict(text_run_index=0.5), dict(codepoint=True)):
            with self.subTest(changes=changes), self.assertRaises(ValueError):
                self.glyph(**changes)

    def test_offline_audit_keeps_all_failures_and_does_not_rewrite_capture(self):
        with tempfile.TemporaryDirectory() as directory:
            root = pathlib.Path(directory)
            receipt = root / "native-pixel-matrix-receipt.jsonl"
            original = "\n".join(json.dumps({"state": state,
                "profile": "physical-1280x720", "color_vision_mode": 0,
                "schema": "billiardgl_native_product_pixel_matrix_v1"}) for state in STATES)
            receipt.write_text(original, encoding="utf-8")
            outcomes = [ValueError("clipped"), {}, KeyError("glyphs"), {}, {}, {}, {}]
            with patch("run_native_product_pixel_matrix.sha256", return_value="f" * 64), \
                 patch("run_native_product_pixel_matrix.read_ppm", return_value=self.image), \
                 patch("run_native_product_pixel_matrix.validate_image", side_effect=outcomes) as validate:
                report = audit_existing_profile(root, "physical-1280x720")
            self.assertEqual(validate.call_count, 7)
            self.assertFalse(report["states"]["default"]["preflight_passed"])
            self.assertFalse(report["states"]["post_shot"]["preflight_passed"])
            self.assertTrue(report["states"]["replay"]["preflight_passed"])
            self.assertFalse(report["new_capture_performed"])
            self.assertFalse(report["capture_binary_binding_verified"])
            self.assertFalse(report["all_pixel_verdicts_passed"])
            self.assertEqual(receipt.read_text(encoding="utf-8"), original)
            self.assertEqual(list(root.iterdir()), [receipt])

    def test_failed_capture_retains_binary_and_process_provenance(self):
        with tempfile.TemporaryDirectory() as directory:
            root = pathlib.Path(directory)
            binary = root / "fake-binary"
            binary.write_bytes(b"not executed")
            failed = subprocess.CompletedProcess([], 7, "partial progress", "capture failed")
            with patch("run_native_product_pixel_matrix.subprocess.run", return_value=failed), \
                    self.assertRaises(RuntimeError):
                run_profile(binary, root, "physical-1280x720")
            evidence = root / "physical-1280x720"
            invocation = json.loads((evidence / "capture-invocation.json").read_text())
            self.assertEqual(invocation["returncode"], 7)
            self.assertEqual(invocation["binary_sha256_before"], invocation["binary_sha256_after"])
            self.assertFalse(invocation["all_pixel_verdicts_passed"])
            self.assertEqual((evidence / "stderr.log").read_text(), "capture failed")

    def test_capture_cannot_claim_or_overwrite_old_images(self):
        with tempfile.TemporaryDirectory() as directory:
            root = pathlib.Path(directory)
            prior = root / "physical-1280x720"
            prior.mkdir()
            image = prior / "aim.ppm"
            image.write_bytes(b"old evidence")
            with patch("run_native_product_pixel_matrix.subprocess.run") as launch, \
                    self.assertRaises(ValueError):
                run_profile(root / "unused-binary", root, "physical-1280x720")
            launch.assert_not_called()
            self.assertEqual(image.read_bytes(), b"old evidence")

    def test_live_capture_records_all_state_failures_before_rejecting(self):
        with tempfile.TemporaryDirectory() as directory:
            root = pathlib.Path(directory)
            binary = root / "fake-binary"
            binary.write_bytes(b"not executed")
            profile = "physical-1280x720"
            rows = [{"state": state, "profile": profile,
                     "schema": "billiardgl_native_product_pixel_matrix_v1",
                     "color_vision_mode": 0} for state in STATES]

            def capture(*args, **kwargs):
                (root / profile / "native-pixel-matrix-receipt.jsonl").write_text(
                    "\n".join(json.dumps(row) for row in rows))
                return subprocess.CompletedProcess([], 0, "", "")

            visited = []
            def validate(image, receipt, state, expected):
                visited.append(state)
                if state in ("training", "replay"):
                    raise ValueError(state + " pixel failure")
                return {"checked": True}

            with patch("run_native_product_pixel_matrix.subprocess.run", side_effect=capture), \
                 patch("run_native_product_pixel_matrix.read_ppm", return_value=self.image), \
                 patch("run_native_product_pixel_matrix.validate_image", side_effect=validate), \
                 self.assertRaises(RuntimeError):
                run_profile(binary, root, profile)
            self.assertEqual(visited, list(STATES))
            report = json.loads((root / profile / "state-preflight.json").read_text())
            self.assertEqual(set(report["state_errors"]), {"training", "replay"})
            self.assertEqual(len(report["state_metrics"]), 5)
            self.assertFalse(report["all_pixel_verdicts_passed"])
            paired = json.loads((root / profile / "glyph-pair-audit.json").read_text())
            self.assertEqual(set(paired["states"]), set(STATES))
            self.assertFalse(paired["glyph_pairs_passed"])
            self.assertFalse(report["glyph_pairs_passed"])
            self.assertTrue(paired["capture_binary_binding_verified"])
            self.assertFalse(paired["all_pixel_verdicts_passed"])

    def test_live_capture_rejects_missing_pairs_even_if_other_preflight_passes(self):
        with tempfile.TemporaryDirectory() as directory:
            root = pathlib.Path(directory)
            binary = root / "fake-binary"
            binary.write_bytes(b"not executed")
            profile = "physical-1280x720"
            rows = [{"state": state, "profile": profile,
                     "schema": "billiardgl_native_product_pixel_matrix_v1",
                     "color_vision_mode": 0} for state in STATES]

            def capture(*args, **kwargs):
                (root / profile / "native-pixel-matrix-receipt.jsonl").write_text(
                    "\n".join(json.dumps(row) for row in rows))
                return subprocess.CompletedProcess([], 0, "", "")

            with patch("run_native_product_pixel_matrix.subprocess.run", side_effect=capture), \
                 patch("run_native_product_pixel_matrix.read_ppm", return_value=self.image), \
                 patch("run_native_product_pixel_matrix.validate_image", return_value={}), \
                 self.assertRaisesRegex(RuntimeError, "paired glyph audit failed"):
                run_profile(binary, root, profile)
            report = json.loads((root / profile / "state-preflight.json").read_text())
            self.assertEqual(report["state_errors"], {})
            self.assertFalse(report["glyph_pairs_passed"])

    def test_cvd_profile_requires_exact_mode_and_profile_identity(self):
        rows = [{"state": state, "profile": "cvd-1920x1080",
                 "schema": "billiardgl_native_product_pixel_matrix_v1",
                 "color_vision_mode": 1} for state in STATES]
        validate_profile_receipts(rows, "cvd-1920x1080")
        for changes in (dict(color_vision_mode=2), dict(color_vision_mode=4),
                        dict(color_vision_mode=True), dict(color_vision_mode=1.5),
                        dict(profile="physical-1920x1080"), dict(schema="unknown")):
            mutated = [{**row, **changes} for row in rows]
            with self.subTest(changes=changes), self.assertRaises(ValueError):
                validate_profile_receipts(mutated, "cvd-1920x1080")


if __name__ == "__main__":
    unittest.main()
