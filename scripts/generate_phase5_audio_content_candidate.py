#!/usr/bin/env python3
"""Generate ten deterministic, repository-authored Phase-5 PCM24 candidates."""

from __future__ import annotations

import argparse
import hashlib
import json
import math
from pathlib import Path
import sys
import wave


ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(Path(__file__).resolve().parent))
from validate_phase5_audio_content_candidate import (  # noqa: E402
    analyze_wav,
    validate_manifest,
)


SAMPLE_RATE = 48000
MAX_PCM24 = 8388607
DESIGN_GATE = "docs/art/phase5-audio-content-remediation-design-gate-v1.json"
VALIDATOR = "scripts/validate_phase5_audio_content_candidate.py"
GENERATOR = "scripts/generate_phase5_audio_content_candidate.py"
DEFAULT_OUTPUT = "assets/audio/phase5-v1"


ONE_SHOTS = {
    "cue_impact": {
        "duration": 0.24, "frequency": 410.0, "chirp": -520.0,
        "decay": 0.050, "noise": 0.18, "seed": 0x41A0C001,
        "target_peak_dbfs": -3.0, "asset_type": "one_shot_transient",
        "bus": "effects",
    },
    "ball_ball": {
        "duration": 0.20, "frequency": 1380.0, "chirp": -900.0,
        "decay": 0.036, "noise": 0.10, "seed": 0xBA11BA11,
        "target_peak_dbfs": -4.0, "asset_type": "one_shot_transient",
        "bus": "effects",
    },
    "rail": {
        "duration": 0.38, "frequency": 245.0, "chirp": -140.0,
        "decay": 0.090, "noise": 0.26, "seed": 0x0A11CE55,
        "target_peak_dbfs": -5.0, "asset_type": "one_shot_transient",
        "bus": "effects",
    },
    "pocket_jaw": {
        "duration": 0.44, "frequency": 620.0, "chirp": -260.0,
        "decay": 0.100, "noise": 0.32, "seed": 0x0C0FFEE1,
        "target_peak_dbfs": -5.5, "asset_type": "one_shot_transient",
        "bus": "effects",
    },
    "pocket_drop": {
        "duration": 0.72, "frequency": 155.0, "chirp": -80.0,
        "decay": 0.180, "noise": 0.38, "seed": 0xD20F2026,
        "target_peak_dbfs": -6.0, "asset_type": "one_shot_transient",
        "bus": "effects",
    },
    "ui_feedback": {
        "duration": 0.22, "frequency": 880.0, "chirp": 240.0,
        "decay": 0.070, "noise": 0.02, "seed": 0x001FEE11,
        "target_peak_dbfs": -8.0, "asset_type": "one_shot_notification",
        "bus": "ui",
    },
    "foul": {
        "duration": 0.90, "frequency": 196.0, "chirp": -70.0,
        "decay": 0.260, "noise": 0.06, "seed": 0xF0012026,
        "target_peak_dbfs": -6.0, "asset_type": "one_shot_notification",
        "bus": "ui",
    },
    "match_finish": {
        "duration": 2.60, "frequency": 262.0, "chirp": 38.0,
        "decay": 0.780, "noise": 0.025, "seed": 0xF1A15EED,
        "target_peak_dbfs": -5.0, "asset_type": "one_shot_notification",
        "bus": "ui",
    },
}


LOOPS = {
    "music_loop": {
        "duration": 32.0, "target_peak_dbfs": -8.0,
        "asset_type": "stereo_loop", "bus": "music",
    },
    "venue_ambience": {
        "duration": 32.0, "target_peak_dbfs": -12.0,
        "asset_type": "stereo_loop", "bus": "ambience",
    },
}


class RepositoryNoise:
    """Tiny deterministic noise state; no sample library or external input."""

    def __init__(self, seed: int):
        self.state = seed & 0xFFFFFFFF

    def next(self) -> float:
        self.state = (self.state * 1664525 + 1013904223) & 0xFFFFFFFF
        return (self.state / 2147483648.0) - 1.0


def _sha256(path: Path) -> str:
    return "sha256:" + hashlib.sha256(path.read_bytes()).hexdigest()


def _smoothstep(value: float) -> float:
    value = max(0.0, min(1.0, value))
    return value * value * (3.0 - 2.0 * value)


def _pack_pcm24(value: int, output: bytearray) -> None:
    if value < 0:
        value += 1 << 24
    output.extend((value & 0xFF, (value >> 8) & 0xFF, (value >> 16) & 0xFF))


def _write_pcm24(path: Path, channels: int, frames) -> None:
    with wave.open(str(path), "wb") as target:
        target.setnchannels(channels)
        target.setsampwidth(3)
        target.setframerate(SAMPLE_RATE)
        block = bytearray()
        frame_in_block = 0
        for frame in frames:
            values = (frame,) if channels == 1 else frame
            for value in values:
                _pack_pcm24(value, block)
            frame_in_block += 1
            if frame_in_block == 4096:
                target.writeframesraw(bytes(block))
                block.clear()
                frame_in_block = 0
        if block:
            target.writeframesraw(bytes(block))


def _one_shot_floats(spec: dict) -> list[float]:
    frame_count = round(spec["duration"] * SAMPLE_RATE)
    noise = RepositoryNoise(spec["seed"])
    filtered_noise = 0.0
    values = []
    for index in range(frame_count):
        t = index / SAMPLE_RATE
        attack = 1.0 - math.exp(-t / 0.0012)
        decay = math.exp(-t / spec["decay"])
        remaining = spec["duration"] - t
        tail = min(1.0, max(0.0, remaining / 0.080)) ** 5
        phase = 2.0 * math.pi * (
            spec["frequency"] * t + 0.5 * spec["chirp"] * t * t)
        tone = math.sin(phase) + 0.34 * math.sin(2.07 * phase) + \
            0.15 * math.sin(3.91 * phase)
        filtered_noise = 0.72 * filtered_noise + 0.28 * noise.next()
        body = tone + spec["noise"] * filtered_noise
        initial = 0.55 * math.sin(2.0 * math.pi *
                                  (spec["frequency"] * 1.73) * t) * \
            math.exp(-t / 0.014)
        values.append(attack * decay * tail * (body + initial))
    values[0] = 0.0
    values[-1] = 0.0
    return values


def _quantize_normalized(values: list[float], target_peak_dbfs: float) -> list[int]:
    maximum = max(abs(value) for value in values)
    target = 10.0 ** (target_peak_dbfs / 20.0)
    scale = target / maximum if maximum else 0.0
    result = [max(-MAX_PCM24, min(MAX_PCM24, round(value * scale * MAX_PCM24)))
              for value in values]
    result[0] = 0
    result[-1] = 0
    return result


def _seam_envelope(t: float, duration: float) -> float:
    fade_seconds = 0.35
    if t < fade_seconds:
        return _smoothstep(t / fade_seconds)
    if t > duration - fade_seconds:
        return _smoothstep((duration - t) / fade_seconds)
    return 1.0


def _loop_float(semantic_id: str, channel: int, t: float, duration: float) -> float:
    seam = _seam_envelope(t, duration)
    slow = 0.78 + 0.22 * math.cos(2.0 * math.pi * t / 4.0)
    if semantic_id == "music_loop":
        frequencies = (110.0, 220.0, 330.0, 440.0, 660.0)
        weights = (0.44, 0.24, 0.15, 0.10, 0.05)
        if channel == 0:
            body = sum(weight * math.sin(2.0 * math.pi * frequency * t)
                       for frequency, weight in zip(frequencies, weights))
        else:
            body = sum(weight * math.cos(2.0 * math.pi * frequency * t)
                       for frequency, weight in zip(frequencies, weights))
        pulse = 0.08 * math.sin(2.0 * math.pi * 55.0 * t) * \
            (0.5 + 0.5 * math.cos(2.0 * math.pi * t / 2.0))
        return seam * slow * (body + pulse)
    frequencies = (30.0, 50.0, 80.0, 130.0, 210.0, 340.0, 550.0, 890.0)
    weights = (0.28, 0.23, 0.18, 0.14, 0.10, 0.075, 0.05, 0.035)
    if channel == 0:
        body = sum(weight * math.sin(2.0 * math.pi * frequency * t)
                   for frequency, weight in zip(frequencies, weights))
    else:
        body = sum(weight * math.cos(2.0 * math.pi * frequency * t)
                   for frequency, weight in zip(frequencies, weights))
    air = 0.035 * math.sin(2.0 * math.pi * (1230.0 if channel == 0 else 1370.0) * t)
    return seam * (0.88 + 0.12 * math.cos(2.0 * math.pi * t / 8.0)) * (body + air)


def _loop_frames(semantic_id: str, spec: dict):
    frame_count = round(spec["duration"] * SAMPLE_RATE)
    maximum = 0.0
    for index in range(frame_count):
        t = index / SAMPLE_RATE
        maximum = max(maximum,
                      abs(_loop_float(semantic_id, 0, t, spec["duration"])),
                      abs(_loop_float(semantic_id, 1, t, spec["duration"])))
    target = 10.0 ** (spec["target_peak_dbfs"] / 20.0)
    scale = target / maximum
    for index in range(frame_count):
        if index == 0 or index == frame_count - 1:
            yield (0, 0)
            continue
        t = index / SAMPLE_RATE
        yield tuple(max(-MAX_PCM24, min(MAX_PCM24,
            round(_loop_float(semantic_id, channel, t, spec["duration"]) *
                  scale * MAX_PCM24))) for channel in range(2))


def _asset_entry(semantic_id: str, spec: dict, output_dir: Path) -> dict:
    filename = semantic_id + ".wav"
    path = output_dir / filename
    measurements = analyze_wav(path, spec["asset_type"])
    return {
        "semantic_id": semantic_id,
        "filename": filename,
        "asset_type": spec["asset_type"],
        "bus": spec["bus"],
        "channel_policy": "mono_point_source" if spec["asset_type"].startswith("one_shot")
            else "stereo_bed",
        "loop_contract": "explicit_full_file_loop" if spec["asset_type"] == "stereo_loop"
            else "not_looped",
        "origin": {
            "method": "repository_authored_deterministic_procedural_synthesis",
            "external_samples_or_downloads": False,
            "parameter_set": semantic_id + "_phase5_v1",
        },
        "measurements": measurements,
        "unmeasured": {
            "true_peak_dbtp": "NOT_MEASURED",
            "integrated_lufs": "NOT_MEASURED",
            "perceptual_quality": "NOT_MEASURED",
            "semantic_identification": "NOT_MEASURED",
            "actual_device_playback": "NOT_MEASURED",
            "external_human_review": "NOT_MEASURED",
            "authorship_or_rights_acceptance": "NOT_MEASURED",
        },
        "machine_gate_status": "PASS",
        "content_acceptance_status": "NOT_CONTENT_ACCEPTED",
    }


def generate(output_dir: Path) -> Path:
    output_dir.mkdir(parents=True, exist_ok=True)
    for semantic_id, spec in ONE_SHOTS.items():
        floats = _one_shot_floats(spec)
        samples = _quantize_normalized(floats, spec["target_peak_dbfs"])
        _write_pcm24(output_dir / (semantic_id + ".wav"), 1, iter(samples))
    for semantic_id, spec in LOOPS.items():
        _write_pcm24(output_dir / (semantic_id + ".wav"), 2,
                     _loop_frames(semantic_id, spec))

    assets = [_asset_entry(semantic_id, spec, output_dir)
              for semantic_id, spec in {**ONE_SHOTS, **LOOPS}.items()]
    manifest = {
        "schema": "phase5_audio_content_candidate_manifest_v1",
        "schema_version": 1,
        "package_id": "phase5_audio_content_candidate_phase5_v1",
        "status": "MACHINE_CANDIDATE_NOT_CONTENT_ACCEPTED",
        "contract_bindings": [
            {"role": "audio_remediation_design_gate", "path": DESIGN_GATE,
             "sha256": _sha256(ROOT / DESIGN_GATE)},
            {"role": "deterministic_generator", "path": GENERATOR,
             "sha256": _sha256(ROOT / GENERATOR)},
            {"role": "fail_closed_validator", "path": VALIDATOR,
             "sha256": _sha256(ROOT / VALIDATOR)},
        ],
        "generation_contract": {
            "sample_rate_hz": SAMPLE_RATE,
            "bits_per_sample": 24,
            "encoding": "uncompressed_signed_little_endian_PCM",
            "deterministic": True,
            "repository_authored_synthesis_graph_and_parameters": True,
            "external_audio_input_count": 0,
            "third_party_sample_count": 0,
            "randomness": "fixed per-cue integer state where noise is used",
        },
        "evidence_boundary": {
            "procedural_repository_generation": True,
            "third_party_samples_or_downloads_used": False,
            "true_peak_measured": False,
            "integrated_lufs_measured": False,
            "listening_review_completed": False,
            "actual_device_playback_completed": False,
            "external_human_review_completed": False,
            "authorship_or_rights_accepted": False,
            "content_accepted": False,
            "beta_gate_completed": False,
        },
        "assets": assets,
        "machine_summary": {
            "asset_count": 10,
            "machine_pass_count": 10,
            "machine_fail_count": 0,
            "unique_file_sha256_count": 10,
            "unique_decoded_pcm_sha256_count": 10,
            "result": "PASS_MACHINE_CANDIDATE_ONLY",
        },
        "remaining_acceptance": [
            "version-recorded true-peak measurement",
            "version-recorded ITU-R BS.1770 integrated loudness measurement",
            "independent original-resolution listening and semantic blind test",
            "actual reference-headphone and representative-speaker playback",
            "external-human content disposition for all required cues",
            "authorship and chain-of-title review/signature",
            "AudioAssetPolicy and Phase-5 Beta contract migration after content acceptance",
        ],
        "acceptance": {
            "machine_candidate": True,
            "content_accepted": False,
            "product_default_switch_authorized": False,
            "result": "MACHINE_CANDIDATE_NOT_CONTENT_ACCEPTED",
        },
    }
    manifest_path = output_dir / "phase5-audio-content-candidate-manifest-v1.json"
    manifest_path.write_text(json.dumps(manifest, indent=2, sort_keys=True) + "\n")
    errors = validate_manifest(manifest, manifest_path)
    if errors:
        raise RuntimeError("generated candidate failed validation:\n" + "\n".join(errors))
    return manifest_path


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--output-dir", type=Path, default=ROOT / DEFAULT_OUTPUT)
    args = parser.parse_args()
    path = generate(args.output_dir)
    print("Generated " + str(path))
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
