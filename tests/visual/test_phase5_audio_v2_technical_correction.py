#!/usr/bin/env python3
"""Bad-input and byte-determinism tests for Phase-5 v2 audio correction."""

from __future__ import annotations

import copy
import json
from pathlib import Path
import shutil
import sys
import tempfile
import unittest


ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT / "scripts"))
import generate_phase5_audio_v2_technical_correction as generator  # noqa: E402
from validate_phase5_audio_v2_technical_correction import (  # noqa: E402
    structural_errors,
    validate_package,
)


ASSET_ROOT = ROOT / "assets/audio/phase5-v2-technical-correction"
REPORT = ROOT / "docs/art/phase5-audio-v2-technical-correction-machine-measurement-v1/report.json"


class Phase5AudioV2TechnicalCorrectionTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.manifest = json.loads((ASSET_ROOT /
            generator.CANDIDATE_MANIFEST_NAME).read_text(encoding="utf-8"))
        cls.report = json.loads(REPORT.read_text(encoding="utf-8"))

    def test_checked_in_package_recomputes_and_passes(self):
        self.assertEqual(validate_package(ASSET_ROOT, REPORT), [])

    def test_wrong_binding_and_acceptance_promotion_fail_closed(self):
        manifest = copy.deepcopy(self.manifest)
        report = copy.deepcopy(self.report)
        manifest["bindings"]["technical_correction_design_gate_sha256"] = "0" * 64
        report["evidence_boundary"]["audio_playback_completed"] = True
        report["evidence_boundary"]["content_accepted"] = True
        errors = structural_errors(manifest, report)
        self.assertTrue(any("immutable binding" in error for error in errors))
        self.assertTrue(any("promotes unperformed" in error for error in errors))

    def test_preexisting_output_or_report_refuses_overwrite(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            output = root / "already_exists"
            output.mkdir()
            with self.assertRaisesRegex(ValueError, "already exists"):
                generator.generate(output, root / "report.json")
            report = root / "report_exists.json"
            report.write_text("do not overwrite", encoding="utf-8")
            with self.assertRaisesRegex(ValueError, "report already exists"):
                generator.generate(root / "new_output", report)

    def test_one_shot_byte_drift_fails_closed(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            output = root / "assets"
            shutil.copytree(ASSET_ROOT, output)
            report = root / "report.json"
            shutil.copyfile(REPORT, report)
            path = output / "ball_ball.wav"
            raw = bytearray(path.read_bytes())
            raw[-4] ^= 1
            path.write_bytes(raw)
            self.assertTrue(any("one-shot byte identity" in error
                                for error in validate_package(output, report)))

    def test_loop_sample_or_non_data_riff_drift_fails_closed(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            output = root / "assets"
            shutil.copytree(ASSET_ROOT, output)
            report = root / "report.json"
            shutil.copyfile(REPORT, report)
            path = output / "music_loop.wav"
            raw = bytearray(path.read_bytes())
            data_start, _ = generator.riff_data_range(raw)
            raw[data_start + 30] ^= 1
            path.write_bytes(raw)
            self.assertTrue(any("sample mapping differs" in error
                                for error in validate_package(output, report)))

            shutil.copyfile(ASSET_ROOT / "music_loop.wav", path)
            raw = bytearray(path.read_bytes())
            raw[24] ^= 1
            path.write_bytes(raw)
            self.assertTrue(any("non-data RIFF bytes changed" in error or
                                "sample mapping differs" in error
                                for error in validate_package(output, report)))

    def test_threshold_or_transform_summary_tamper_fails_closed(self):
        manifest = copy.deepcopy(self.manifest)
        report = copy.deepcopy(self.report)
        manifest["technical_summary"]["pass_count"] = 9
        manifest["assets"][0]["transformation"]["operation"] = "gain"
        errors = structural_errors(manifest, report)
        self.assertTrue(any("transform inventory" in error for error in errors))
        self.assertTrue(any("technical summary" in error for error in errors))

    def test_two_independent_roots_are_byte_identical(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            first_assets, second_assets = root / "a", root / "b"
            first_report, second_report = root / "a-report.json", root / "b-report.json"
            generator.generate(first_assets, first_report)
            generator.generate(second_assets, second_report)
            first_names = sorted(path.name for path in first_assets.iterdir())
            second_names = sorted(path.name for path in second_assets.iterdir())
            self.assertEqual(first_names, second_names)
            self.assertEqual(len(first_names), 11)
            for name in first_names:
                self.assertEqual((first_assets / name).read_bytes(),
                                 (second_assets / name).read_bytes(), name)
            self.assertEqual(first_report.read_bytes(), second_report.read_bytes())
            for name in ("cue_impact.wav", "ball_ball.wav", "rail.wav",
                         "pocket_jaw.wav", "pocket_drop.wav", "ui_feedback.wav",
                         "foul.wav", "match_finish.wav"):
                self.assertEqual((first_assets / name).read_bytes(),
                                 (generator.V1_ROOT / name).read_bytes(), name)


if __name__ == "__main__":
    unittest.main()
