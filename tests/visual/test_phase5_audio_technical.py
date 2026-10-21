#!/usr/bin/env python3
"""Positive, determinism and bad-input checks for audio technical measurement."""

from __future__ import annotations

import copy
import json
from pathlib import Path
import sys
import tempfile
import unittest
import wave


ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT / "scripts"))
from measure_phase5_audio_technical import measure_wav  # noqa: E402
from validate_phase5_audio_technical import validate_report  # noqa: E402


REPORT = ROOT / "docs/art/phase5-audio-technical-machine-measurement-v1/report.json"
MANIFEST = ROOT / "assets/audio/phase5-v1/phase5-audio-content-candidate-manifest-v1.json"


class Phase5AudioTechnicalTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.report = json.loads(REPORT.read_text(encoding="utf-8"))

    def test_checked_in_report_passes_structural_validator(self):
        self.assertEqual(validate_report(self.report, recompute=False), [])

    def test_human_device_rights_or_acceptance_promotion_fails_closed(self):
        value = copy.deepcopy(self.report)
        boundary = value["evidence_boundary"]
        boundary["audio_device_playback_completed"] = True
        boundary["human_listening_completed"] = True
        boundary["authorship_or_rights_verified"] = True
        boundary["content_accepted"] = True
        self.assertTrue(any("promotes unperformed" in error
                            for error in validate_report(value, recompute=False)))

    def test_gate_and_manifest_hash_drift_fails_closed(self):
        value = copy.deepcopy(self.report)
        value["bindings"]["design_gate_sha256"] = "0" * 64
        value["bindings"]["resolved_manifest_sha256"] = "1" * 64
        self.assertTrue(any("frozen gate/manifest" in error
                            for error in validate_report(value, recompute=False)))

    def test_threshold_result_tamper_fails_closed(self):
        value = copy.deepcopy(self.report)
        value["assets"][0]["threshold_checks"][0]["result"] = "FAIL"
        self.assertTrue(any("threshold summary" in error
                            for error in validate_report(value, recompute=False)))

    def test_bad_pcm16_input_is_rejected(self):
        with tempfile.TemporaryDirectory() as directory:
            path = Path(directory) / "bad.wav"
            with wave.open(str(path), "wb") as target:
                target.setnchannels(1)
                target.setsampwidth(2)
                target.setframerate(48000)
                target.writeframes(b"\0\0" * 480)
            with self.assertRaisesRegex(ValueError, "PCM24"):
                measure_wav(path, "one_shot_transient")

    def test_same_bound_wav_measurement_is_deterministic(self):
        manifest = json.loads(MANIFEST.read_text(encoding="utf-8"))
        asset = manifest["assets"][1]
        path = MANIFEST.parent / asset["filename"]
        first = measure_wav(path, asset["asset_type"])
        second = measure_wav(path, asset["asset_type"])
        self.assertEqual(first, second)


if __name__ == "__main__":
    unittest.main()
