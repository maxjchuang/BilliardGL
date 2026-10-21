#!/usr/bin/env python3
"""Validate the Phase-5 procedural audio machine-candidate package."""

from __future__ import annotations

import argparse
import hashlib
import json
import math
from pathlib import Path
import wave


ROOT = Path(__file__).resolve().parents[1]
FULL_SCALE = 8388608.0
MEASUREMENT_FLOOR_DBFS = -300.0
EXPECTED = {
    "cue_impact": ("cue_impact.wav", "one_shot_transient", 1, 20.0),
    "ball_ball": ("ball_ball.wav", "one_shot_transient", 1, 20.0),
    "rail": ("rail.wav", "one_shot_transient", 1, 20.0),
    "pocket_jaw": ("pocket_jaw.wav", "one_shot_transient", 1, 20.0),
    "pocket_drop": ("pocket_drop.wav", "one_shot_transient", 1, 20.0),
    "ui_feedback": ("ui_feedback.wav", "one_shot_notification", 1, 50.0),
    "foul": ("foul.wav", "one_shot_notification", 1, 50.0),
    "match_finish": ("match_finish.wav", "one_shot_notification", 1, 50.0),
    "music_loop": ("music_loop.wav", "stereo_loop", 2, None),
    "venue_ambience": ("venue_ambience.wav", "stereo_loop", 2, None),
}


def _sha256(path: Path) -> str:
    return "sha256:" + hashlib.sha256(path.read_bytes()).hexdigest()


def _db(value: float) -> float:
    return round(20.0 * math.log10(value), 6) if value > 0.0 else MEASUREMENT_FLOOR_DBFS


def _rms(values) -> float:
    values = list(values)
    return math.sqrt(sum(value * value for value in values) / len(values)) \
        if values else 0.0


def _decode_pcm24(raw: bytes) -> list[int]:
    if len(raw) % 3:
        raise ValueError("PCM24 data byte count is not divisible by three")
    values = []
    append = values.append
    for offset in range(0, len(raw), 3):
        value = raw[offset] | (raw[offset + 1] << 8) | (raw[offset + 2] << 16)
        if value & 0x800000:
            value -= 1 << 24
        append(value)
    return values


def _zero_crossing_rate(values: list[int]) -> float:
    if len(values) < 2:
        return 0.0
    crossings = sum((a < 0 <= b) or (a >= 0 > b)
                    for a, b in zip(values, values[1:]))
    return crossings / (len(values) - 1)


def _first_difference_rms(values: list[int]) -> float:
    return _rms(b - a for a, b in zip(values, values[1:])) / FULL_SCALE


def analyze_wav(path: Path, asset_type: str) -> dict:
    with wave.open(str(path), "rb") as source:
        channels = source.getnchannels()
        sample_width = source.getsampwidth()
        sample_rate = source.getframerate()
        frame_count = source.getnframes()
        compression = source.getcomptype()
        raw = source.readframes(frame_count)
    if sample_width != 3 or compression != "NONE":
        raise ValueError(str(path) + ": expected uncompressed PCM24")
    values = _decode_pcm24(raw)
    peak = max((abs(value) for value in values), default=0) / FULL_SCALE
    rms = _rms(values) / FULL_SCALE
    common = {
        "byte_size": path.stat().st_size,
        "channels": channels,
        "sample_rate_hz": sample_rate,
        "bits_per_sample": 24,
        "frame_count": frame_count,
        "duration_seconds": round(frame_count / sample_rate, 9),
        "file_sha256": _sha256(path),
        "decoded_pcm_sha256": "sha256:" + hashlib.sha256(raw).hexdigest(),
        "sample_peak_linear": round(peak, 9),
        "sample_peak_dbfs": _db(peak),
        "unweighted_full_file_rms_dbfs": _db(rms),
        "full_scale_sample_count": sum(
            value in (-8388608, 8388607) for value in values),
    }
    if asset_type.startswith("one_shot"):
        if channels != 1:
            raise ValueError(str(path) + ": one-shot analysis requires mono")
        threshold = round(FULL_SCALE * 0.001)
        active_indices = [index for index, value in enumerate(values)
                          if abs(value) >= threshold]
        active = values[active_indices[0]:active_indices[-1] + 1] \
            if active_indices else []
        active_rms = _rms(active) / FULL_SCALE
        last_10ms = values[-max(1, round(sample_rate * 0.010)):]
        last_rms = _rms(last_10ms) / FULL_SCALE
        peak_index = max(range(len(values)), key=lambda index: abs(values[index])) \
            if values else 0
        common["one_shot"] = {
            "onset_to_peak_ms": round(peak_index * 1000.0 / sample_rate, 6),
            "active_region_rms_dbfs": _db(active_rms),
            "active_crest_factor_db": round(_db(peak) - _db(active_rms), 6),
            "last_10ms_rms_dbfs": _db(last_rms),
            "first_sample_linear": round(
                abs(values[0]) / FULL_SCALE if values else 0.0, 9),
            "last_sample_linear": round(
                abs(values[-1]) / FULL_SCALE if values else 0.0, 9),
        }
    elif asset_type == "stereo_loop":
        if channels != 2:
            raise ValueError(str(path) + ": loop analysis requires stereo")
        window_frames = round(sample_rate * 0.100)
        per_channel = []
        for channel in range(channels):
            channel_values = values[channel::channels]
            head = channel_values[:window_frames]
            tail = channel_values[-window_frames:]
            head_rms = _rms(head) / FULL_SCALE
            tail_rms = _rms(tail) / FULL_SCALE
            rms_delta = abs(_db(head_rms) - _db(tail_rms))
            zcr_delta = abs(_zero_crossing_rate(head) - _zero_crossing_rate(tail))
            diff_head = _first_difference_rms(head)
            diff_tail = _first_difference_rms(tail)
            diff_delta = abs(_db(diff_head) - _db(diff_tail))
            boundary_jump = abs(channel_values[0] - channel_values[-1]) / FULL_SCALE
            per_channel.append({
                "channel_index": channel,
                "boundary_sample_jump_dbfs": _db(boundary_jump),
                "head_tail_100ms_rms_delta_db": round(rms_delta, 6),
                "head_tail_100ms_zero_crossing_rate_delta": round(zcr_delta, 9),
                "head_tail_100ms_first_difference_rms_delta_db": round(diff_delta, 6),
            })
        common["loop"] = {
            "boundary_sample_jump_dbfs_max": max(
                item["boundary_sample_jump_dbfs"] for item in per_channel),
            "head_tail_100ms_rms_delta_db_max": max(
                item["head_tail_100ms_rms_delta_db"] for item in per_channel),
            "head_tail_100ms_zero_crossing_rate_delta_max": max(
                item["head_tail_100ms_zero_crossing_rate_delta"] for item in per_channel),
            "head_tail_100ms_first_difference_rms_delta_db_max": max(
                item["head_tail_100ms_first_difference_rms_delta_db"] for item in per_channel),
            "per_channel": per_channel,
        }
    else:
        raise ValueError("unknown asset type: " + asset_type)
    return common


def _asset_gate_errors(asset: dict) -> list[str]:
    errors = []
    semantic_id = asset.get("semantic_id", "UNKNOWN")
    expected = EXPECTED.get(semantic_id)
    if expected is None:
        return [semantic_id + ": unexpected semantic asset"]
    filename, asset_type, channels, onset_limit = expected
    if asset.get("filename") != filename or asset.get("asset_type") != asset_type:
        errors.append(semantic_id + ": filename/type differs")
    measurements = asset.get("measurements", {})
    if measurements.get("sample_rate_hz") != 48000 or \
            measurements.get("bits_per_sample") != 24 or \
            measurements.get("channels") != channels:
        errors.append(semantic_id + ": 48kHz/24-bit/channel policy failed")
    if not isinstance(measurements.get("sample_peak_dbfs"), (int, float)) or \
            measurements.get("sample_peak_dbfs") > -1.0:
        errors.append(semantic_id + ": sample peak gate failed")
    if measurements.get("full_scale_sample_count") != 0:
        errors.append(semantic_id + ": full-scale sample gate failed")
    if asset_type.startswith("one_shot"):
        one_shot = measurements.get("one_shot", {})
        if not isinstance(one_shot.get("onset_to_peak_ms"), (int, float)) or \
                one_shot.get("onset_to_peak_ms") > onset_limit:
            errors.append(semantic_id + ": onset-to-peak gate failed")
        active_rms = one_shot.get("active_region_rms_dbfs")
        if not isinstance(active_rms, (int, float)) or not -30.0 <= active_rms <= -14.0:
            errors.append(semantic_id + ": active RMS gate failed")
        crest = one_shot.get("active_crest_factor_db")
        if not isinstance(crest, (int, float)) or not 6.0 <= crest <= 24.0:
            errors.append(semantic_id + ": active crest-factor gate failed")
        if not isinstance(one_shot.get("last_10ms_rms_dbfs"), (int, float)) or \
                one_shot.get("last_10ms_rms_dbfs") > -60.0:
            errors.append(semantic_id + ": tail-silence gate failed")
        if one_shot.get("first_sample_linear") != 0.0 or \
                one_shot.get("last_sample_linear") != 0.0:
            errors.append(semantic_id + ": endpoint click gate failed")
    else:
        loop = measurements.get("loop", {})
        if not isinstance(measurements.get("duration_seconds"), (int, float)) or \
                measurements.get("duration_seconds") < 30.0:
            errors.append(semantic_id + ": loop duration gate failed")
        if asset.get("loop_contract") != "explicit_full_file_loop":
            errors.append(semantic_id + ": explicit loop contract missing")
        if not isinstance(loop.get("boundary_sample_jump_dbfs_max"), (int, float)) or \
                loop.get("boundary_sample_jump_dbfs_max") > -60.0:
            errors.append(semantic_id + ": loop boundary gate failed")
        if not isinstance(loop.get("head_tail_100ms_rms_delta_db_max"), (int, float)) or \
                loop.get("head_tail_100ms_rms_delta_db_max") > 1.0:
            errors.append(semantic_id + ": loop head/tail RMS gate failed")
        if not isinstance(loop.get("head_tail_100ms_zero_crossing_rate_delta_max"), (int, float)) or \
                loop.get("head_tail_100ms_zero_crossing_rate_delta_max") > 0.02:
            errors.append(semantic_id + ": loop zero-crossing continuity proxy failed")
        if not isinstance(loop.get("head_tail_100ms_first_difference_rms_delta_db_max"), (int, float)) or \
                loop.get("head_tail_100ms_first_difference_rms_delta_db_max") > 1.0:
            errors.append(semantic_id + ": loop spectral continuity proxy failed")
    if asset.get("machine_gate_status") != "PASS":
        errors.append(semantic_id + ": machine gate status is not PASS")
    return errors


def validate_manifest(manifest: dict, manifest_path: Path,
                      verify_files: bool = True) -> list[str]:
    errors = []
    if manifest.get("schema") != "phase5_audio_content_candidate_manifest_v1" or \
            manifest.get("status") != "MACHINE_CANDIDATE_NOT_CONTENT_ACCEPTED":
        errors.append("manifest identity/status invalid")
    boundary = manifest.get("evidence_boundary", {})
    required_false = (
        "true_peak_measured", "integrated_lufs_measured", "listening_review_completed",
        "actual_device_playback_completed", "external_human_review_completed",
        "authorship_or_rights_accepted", "content_accepted", "beta_gate_completed",
    )
    if any(boundary.get(key) is not False for key in required_false):
        errors.append("evidence boundary promotes unperformed content evidence")
    if boundary.get("procedural_repository_generation") is not True or \
            boundary.get("third_party_samples_or_downloads_used") is not False:
        errors.append("procedural origin disclosure invalid")
    for binding in manifest.get("contract_bindings", []):
        path = ROOT / binding.get("path", "")
        if not path.is_file() or _sha256(path) != binding.get("sha256"):
            errors.append("contract/tool binding differs: " + binding.get("path", "UNKNOWN"))
    assets = manifest.get("assets")
    if not isinstance(assets, list):
        return errors + ["assets must be an array"]
    by_id = {asset.get("semantic_id"): asset for asset in assets if isinstance(asset, dict)}
    if len(assets) != len(by_id) or set(by_id) != set(EXPECTED):
        errors.append("semantic asset inventory must exactly cover ten unique cues")
    filenames = [asset.get("filename") for asset in assets]
    file_hashes = [asset.get("measurements", {}).get("file_sha256") for asset in assets]
    pcm_hashes = [asset.get("measurements", {}).get("decoded_pcm_sha256") for asset in assets]
    if len(filenames) != len(set(filenames)):
        errors.append("asset filenames are not unique")
    if len(file_hashes) != len(set(file_hashes)):
        errors.append("asset file SHA values are not unique")
    if len(pcm_hashes) != len(set(pcm_hashes)):
        errors.append("decoded PCM SHA values are not unique")
    for asset in assets:
        errors += _asset_gate_errors(asset)
        if verify_files and asset.get("semantic_id") in EXPECTED:
            asset_path = manifest_path.parent / asset.get("filename", "")
            if not asset_path.is_file():
                errors.append(asset.get("semantic_id") + ": WAV missing")
                continue
            try:
                actual = analyze_wav(asset_path, asset["asset_type"])
            except (OSError, ValueError, wave.Error) as exc:
                errors.append(asset.get("semantic_id") + ": WAV analysis failed: " + str(exc))
                continue
            if actual != asset.get("measurements"):
                errors.append(asset.get("semantic_id") + ": measurements/hash differ from WAV")
    if manifest.get("machine_summary") != {
        "asset_count": 10,
        "machine_pass_count": 10,
        "machine_fail_count": 0,
        "unique_file_sha256_count": 10,
        "unique_decoded_pcm_sha256_count": 10,
        "result": "PASS_MACHINE_CANDIDATE_ONLY",
    }:
        errors.append("machine summary differs")
    if manifest.get("acceptance", {}).get("result") != \
            "MACHINE_CANDIDATE_NOT_CONTENT_ACCEPTED":
        errors.append("acceptance boundary differs")
    return errors


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("manifest", type=Path)
    args = parser.parse_args()
    try:
        manifest = json.loads(args.manifest.read_text())
    except (OSError, json.JSONDecodeError) as exc:
        print("Phase5 audio candidate FAIL: " + str(exc))
        return 1
    errors = validate_manifest(manifest, args.manifest)
    if errors:
        for error in errors:
            print("FAIL: " + error)
        return 1
    print("Phase5 audio candidate PASS: assets=10 machine=10 content=NOT_ACCEPTED")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
