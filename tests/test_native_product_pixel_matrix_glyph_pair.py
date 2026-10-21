"""Synthetic negatives for causal glyph pixels, not a native release verdict."""
import pathlib
import sys
import unittest
import tempfile
import hashlib
import json
from unittest.mock import patch

sys.path.insert(0, str(pathlib.Path(__file__).resolve().parents[1] / "scripts"))
from run_native_product_pixel_matrix import (Ppm, paired_glyph_pixel_stats,
    validate_glyph_pair_binding, audit_existing_glyph_pairs, STATES)


class GlyphPairTests(unittest.TestCase):
    def setUp(self):
        self.background = Ppm(20, 12, bytearray(20 * 12 * 3))
        self.normal = self.background.copy()
        self.glyphs = []
        for ordinal, x in enumerate((2, 10)):
            glyph = dict(text_run_index=0, codepoint_ordinal=ordinal,
                         codepoint=65 + ordinal, clip_x=0, clip_y=0,
                         clip_width=20, clip_height=12,
                         x=x-2, y=1, width=7, height=9)
            for prefix in ("ink_", "ink_source_"):
                glyph.update({prefix + "x": x, prefix + "y": 3,
                              prefix + "width": 3, prefix + "height": 5})
            self.glyphs.append(glyph)
            self.paint(self.normal, x, 4, 255)
        self.receipt = dict(product_ui_text_runs=[dict(text="AB")],
                            product_ui_glyph_quads=self.glyphs)

    @staticmethod
    def paint(image, x, y, value):
        offset = (y * image.width + x) * 3
        image.pixels[offset:offset + 3] = bytes((value, value, value))

    def audit(self):
        return paired_glyph_pixel_stats(self.normal, self.background, self.receipt)

    def test_causal_coverage_is_not_frame_or_font_authentication(self):
        result = self.audit()
        self.assertEqual(result["glyph_count"], 2)
        self.assertFalse(result["paired_frame_binding_verified"])
        self.assertFalse(result["font_shape_identity_verified"])

    def test_missing_or_low_contrast_glyph_rejected(self):
        for value in (0, 40):
            with self.subTest(value=value):
                self.paint(self.normal, 10, 4, value)
                with self.assertRaisesRegex(ValueError, "exclusive contrasting"):
                    self.audit()

    def test_all_failing_runs_reported_not_only_first_character(self):
        self.receipt["product_ui_text_runs"] = [dict(text="A"), dict(text="B")]
        self.glyphs[1].update(text_run_index=1, codepoint_ordinal=0)
        self.paint(self.normal, 2, 4, 40)
        self.paint(self.normal, 10, 4, 40)
        with self.assertRaises(ValueError) as caught:
            self.audit()
        report = json.loads(str(caught.exception).split(": ", 1)[1])
        self.assertEqual(set(report), {"0", "1"})
        self.assertTrue(all(item["failed_glyph_count"] == 1 for item in report.values()))

    def test_unrelated_scene_change_rejected(self):
        self.paint(self.normal, 18, 10, 255)
        with self.assertRaisesRegex(ValueError, "outside submitted"):
            self.audit()

    def test_overlapping_glyph_cannot_borrow_other_glyph_pixels(self):
        self.paint(self.normal, 10, 4, 0)
        self.glyphs[1].update(ink_x=2, ink_source_x=2)
        self.glyphs[1]["x"] = 0
        with self.assertRaisesRegex(ValueError, "exclusive contrasting"):
            self.audit()

    def test_sdf_edge_outside_bitmap_is_allowed_only_within_submitted_quad(self):
        self.paint(self.normal, 0, 2, 40)
        self.audit()
        self.paint(self.normal, 0, 0, 40)
        with self.assertRaisesRegex(ValueError, "outside submitted"):
            self.audit()

    def test_codepoint_exchange_without_source_change_rejected(self):
        self.glyphs[0]["codepoint"] = 66
        self.glyphs[1]["codepoint"] = 65
        with self.assertRaisesRegex(ValueError, "coverage differ"):
            self.audit()

    def test_dimension_mismatch_rejected(self):
        self.background.width = 19
        with self.assertRaisesRegex(ValueError, "dimensions"):
            self.audit()

    def test_persisted_pair_binding_rejects_drift_and_inventory_substitution(self):
        with tempfile.TemporaryDirectory() as directory:
            root = pathlib.Path(directory)
            payload = b"P6\n1 1\n255\n\xff\xff\xff"
            for name in ("aim.ppm", "aim-no-glyphs.ppm"):
                (root / name).write_bytes(payload)
            digest = hashlib.sha256(payload).hexdigest()
            receipt = dict(self.receipt,
                paired_no_glyph_image="aim-no-glyphs.ppm",
                paired_restored_user_visible=True,
                glyph_atlas_binding_verified=True,
                font_atlas_cache_key="frozen-test-atlas",
                paired_no_glyph_suppressed_count=2,
                paired_no_glyph_suppressed_icon_labels=0)
            for key in ("paired_normal_ppm_sha256", "paired_no_glyph_ppm_sha256",
                        "product_ui_canonical_sha256", "paired_no_glyph_canonical_sha256",
                        "paired_restored_canonical_sha256", "paired_frozen_state_sha256",
                        "paired_frozen_state_before_sha256", "paired_frozen_state_after_sha256",
                        "paired_normal_rgba_sha256", "paired_restored_rgba_sha256",
                        "paired_no_glyph_rgba_sha256"):
                receipt[key] = digest
            for glyph in self.glyphs:
                glyph["atlas_record_sha256"] = digest
            result = validate_glyph_pair_binding(root, "aim", receipt)
            self.assertFalse(result["independent_font_rasterization_verified"])
            for change in (dict(paired_frozen_state_after_sha256="0" * 64),
                           dict(paired_restored_canonical_sha256="0" * 64),
                           dict(paired_normal_ppm_sha256="0" * 64),
                           dict(paired_no_glyph_image="../wrong.ppm"),
                           dict(paired_no_glyph_suppressed_count=True),
                           dict(paired_no_glyph_suppressed_icon_labels=1),
                           dict(glyph_atlas_binding_verified=False),
                           dict(paired_restored_user_visible=False)):
                with self.subTest(change=change), self.assertRaises(ValueError):
                    validate_glyph_pair_binding(root, "aim", {**receipt, **change})

    def test_offline_pair_audit_reports_every_failure_without_writing_evidence(self):
        with tempfile.TemporaryDirectory() as directory:
            root = pathlib.Path(directory)
            profile = "physical-1280x720"
            path = root / "native-pixel-matrix-receipt.jsonl"
            payload = "\n".join(json.dumps(dict(state=state, profile=profile,
                schema="billiardgl_native_product_pixel_matrix_v1",
                color_vision_mode=0)) for state in STATES)
            path.write_text(payload)
            visited = []
            def reject(directory, state, row):
                visited.append(state)
                raise ValueError(state + " missing pair")
            with patch("run_native_product_pixel_matrix.validate_glyph_pair_binding",
                       side_effect=reject):
                report = audit_existing_glyph_pairs(root, profile)
            self.assertEqual(visited, list(STATES))
            self.assertFalse(report["glyph_pairs_passed"])
            self.assertFalse(report["all_pixel_verdicts_passed"])
            self.assertFalse(report["new_capture_performed"])
            self.assertEqual(path.read_text(), payload)
            self.assertEqual(list(root.iterdir()), [path])
