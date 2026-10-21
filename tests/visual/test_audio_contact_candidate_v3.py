"""Focused v3 contact-synthesis behavior; no subjective realism assertion."""
import importlib.util
import os
import shutil
from pathlib import Path
import sys
import subprocess
import tempfile
import unittest
import wave

ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT / "scripts"))
import generate_phase5_audio_content_candidate as previous


def contact_samples(name):
    path = ROOT / "scripts/generate_phase5_audio_contact_candidate_v3.py"
    if not path.exists():
        # RED exercises the actual previous synthesis, not a mock. Its long
        # pitched tail does not implement the approved short-contact change.
        spec = previous.ONE_SHOTS[name]
        return previous._quantize_normalized(previous._one_shot_floats(spec), spec["target_peak_dbfs"])
    spec = importlib.util.spec_from_file_location("audio_contact_v3", path)
    module = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(module)
    return module.contact_wave(name)


def energy95_ms(samples):
    total = sum(value * value for value in samples)
    if total <= 0:
        raise AssertionError("contact wave must be audible rather than silence")
    accumulated = 0
    for index, value in enumerate(samples):
        accumulated += value * value
        if accumulated >= total * .95:
            return index / 48
    raise AssertionError("contact wave has no energy")


class ContactCandidateV3Tests(unittest.TestCase):
    def test_ball_contact_finishes_its_energy_early(self):
        # Wrong long decay/body or a sustained oscillator must fail this
        # approved style behavior. This is not an audio-content acceptance gate.
        samples = contact_samples("ball_ball")
        self.assertLessEqual(energy95_ms(samples), 18.0)
        self.assertEqual((samples[0], samples[-1]), (0, 0))

    def test_five_contacts_are_deterministic_and_distinct(self):
        names = ("cue_impact", "ball_ball", "rail", "pocket_jaw", "pocket_drop")
        waves = [contact_samples(name) for name in names]
        self.assertEqual(waves, [contact_samples(name) for name in names])
        self.assertEqual(len({tuple(wave) for wave in waves}), 5)
        for wave in waves:
            self.assertGreater(sum(value * value for value in wave), 0)
            self.assertEqual((wave[0], wave[-1]), (0, 0))
            self.assertLess(max(abs(value) for value in wave), 8388607)

    @unittest.skipUnless(os.environ.get("BILLIARDGL_AUDIO_COMPARE_BIN"), "explicit independent SDL comparison binary required")
    def test_actual_sdl_comparison_writes_ten_queued_contact_preview(self):
        with tempfile.TemporaryDirectory(prefix="billiardgl-audio-compare-") as directory:
            destination = Path(directory) / "comparison.wav"
            result = subprocess.run([os.environ["BILLIARDGL_AUDIO_COMPARE_BIN"],
                "--compare-v3", str(destination)], capture_output=True, text=True, timeout=30)
            self.assertEqual(result.returncode, 0, result.stderr)
            self.assertTrue(destination.is_file(), "missing actual SDL postmix comparison output")
            self.assertIn("comparison_actual_queued_cues=10", result.stdout)
            with wave.open(str(destination)) as audio:
                self.assertEqual((audio.getframerate(),audio.getnchannels(),audio.getsampwidth()),(48000,2,2))
                self.assertGreater(audio.getnframes(),48000)
                self.assertLess(audio.getnframes(),48000*8)

    @unittest.skipUnless(os.environ.get("BILLIARDGL_AUDIO_COMPARE_BIN"), "explicit independent SDL comparison binary required")
    def test_existing_comparison_evidence_cannot_be_overwritten(self):
        with tempfile.TemporaryDirectory(prefix="billiardgl-audio-preserve-") as directory:
            destination = Path(directory) / "existing.wav"
            sentinel = b"preserve-existing-evidence"
            destination.write_bytes(sentinel)
            result = subprocess.run([os.environ["BILLIARDGL_AUDIO_COMPARE_BIN"],
                "--compare-v3", str(destination)], capture_output=True, text=True, timeout=30)
            self.assertNotEqual(result.returncode, 0, "existing output must fail rather than truncate")
            self.assertEqual(destination.read_bytes(), sentinel)

    def test_generator_refuses_output_below_parent_frozen_tree(self):
        # Real filesystem clone: reproducing the rejected path never writes
        # into the repository's actual frozen v2 directory.
        path = ROOT / "scripts/generate_phase5_audio_contact_candidate_v3.py"
        spec = importlib.util.spec_from_file_location("audio_contact_v3_guard", path)
        module = importlib.util.module_from_spec(spec)
        spec.loader.exec_module(module)
        with tempfile.TemporaryDirectory(prefix="billiardgl-audio-parent-") as directory:
            parent = Path(directory) / "frozen-v2"
            shutil.copytree(ROOT / "assets/audio/phase5-v2-technical-correction",parent)
            parent.chmod(0o700)  # Only this owned temporary clone, never v2 itself.
            module.PARENT = parent
            module.PARENT_MANIFEST = parent / "phase5-audio-v2-technical-correction-candidate-manifest-v1.json"
            destination = parent / "nested-candidate"
            with self.assertRaisesRegex(ValueError,"new empty output"):
                module.generate(destination)
            self.assertFalse(destination.exists())


if __name__ == "__main__":
    unittest.main()
