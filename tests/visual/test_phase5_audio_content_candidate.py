#!/usr/bin/env python3
"""Positive and fail-closed negative checks for Phase-5 audio candidates."""

from __future__ import annotations

import copy
import json
from pathlib import Path
import sys
import unittest


ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT / "scripts"))
from validate_phase5_audio_content_candidate import validate_manifest  # noqa: E402


MANIFEST = ROOT / "assets/audio/phase5-v1/phase5-audio-content-candidate-manifest-v1.json"


class Phase5AudioContentCandidateTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.value = json.loads(MANIFEST.read_text())

    def errors(self, value, verify_files=False):
        return validate_manifest(value, MANIFEST, verify_files=verify_files)

    def test_checked_in_machine_candidate_passes_with_real_wavs(self):
        self.assertEqual(self.errors(self.value, verify_files=True), [])

    def test_content_acceptance_or_human_claim_fails_closed(self):
        value = copy.deepcopy(self.value)
        value["evidence_boundary"]["content_accepted"] = True
        value["evidence_boundary"]["external_human_review_completed"] = True
        self.assertTrue(any("evidence boundary" in item for item in self.errors(value)))

    def test_missing_semantic_asset_fails_closed(self):
        value = copy.deepcopy(self.value)
        value["assets"].pop()
        self.assertTrue(any("exactly cover ten" in item for item in self.errors(value)))

    def test_decoded_pcm_alias_fails_closed(self):
        value = copy.deepcopy(self.value)
        value["assets"][1]["measurements"]["decoded_pcm_sha256"] = \
            value["assets"][0]["measurements"]["decoded_pcm_sha256"]
        self.assertTrue(any("decoded PCM SHA" in item for item in self.errors(value)))

    def test_format_peak_and_full_scale_fail_closed(self):
        value = copy.deepcopy(self.value)
        measurements = value["assets"][0]["measurements"]
        measurements["sample_rate_hz"] = 44100
        measurements["sample_peak_dbfs"] = -0.5
        measurements["full_scale_sample_count"] = 1
        errors = self.errors(value)
        self.assertTrue(any("48kHz/24-bit" in item for item in errors))
        self.assertTrue(any("sample peak" in item for item in errors))
        self.assertTrue(any("full-scale" in item for item in errors))

    def test_transient_and_tail_fail_closed(self):
        value = copy.deepcopy(self.value)
        one_shot = value["assets"][0]["measurements"]["one_shot"]
        one_shot["onset_to_peak_ms"] = 21.0
        one_shot["active_crest_factor_db"] = 5.0
        one_shot["last_10ms_rms_dbfs"] = -59.0
        errors = self.errors(value)
        self.assertTrue(any("onset-to-peak" in item for item in errors))
        self.assertTrue(any("crest-factor" in item for item in errors))
        self.assertTrue(any("tail-silence" in item for item in errors))

    def test_loop_duration_boundary_and_continuity_fail_closed(self):
        value = copy.deepcopy(self.value)
        loop_asset = next(asset for asset in value["assets"]
                          if asset["semantic_id"] == "music_loop")
        loop_asset["measurements"]["duration_seconds"] = 29.0
        loop_asset["measurements"]["loop"]["boundary_sample_jump_dbfs_max"] = -59.0
        loop_asset["measurements"]["loop"]["head_tail_100ms_rms_delta_db_max"] = 1.1
        loop_asset["measurements"]["loop"][
            "head_tail_100ms_first_difference_rms_delta_db_max"] = 1.1
        errors = self.errors(value)
        self.assertTrue(any("loop duration" in item for item in errors))
        self.assertTrue(any("loop boundary" in item for item in errors))
        self.assertTrue(any("head/tail RMS" in item for item in errors))
        self.assertTrue(any("spectral continuity proxy" in item for item in errors))


if __name__ == "__main__":
    unittest.main()
