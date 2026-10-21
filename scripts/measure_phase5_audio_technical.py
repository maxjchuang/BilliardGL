#!/usr/bin/env python3
"""Deterministic offline technical measurement for Phase-5 audio candidates.

This tool reads repository WAV bytes only.  It never opens an audio device and
does not make listening, authorship, rights, content-acceptance, integration or
Beta claims.
"""

from __future__ import annotations

import argparse
from array import array
import hashlib
import json
import math
from pathlib import Path
import platform
import sys
import wave


ROOT = Path(__file__).resolve().parents[1]
DESIGN_GATE = ROOT / "docs/art/phase5-audio-content-remediation-design-gate-v1.json"
REQUESTED_MANIFEST = ROOT / "assets/audio/phase5-v1/manifest.json"
RESOLVED_MANIFEST = ROOT / "assets/audio/phase5-v1/phase5-audio-content-candidate-manifest-v1.json"
VALIDATOR = ROOT / "scripts/validate_phase5_audio_technical.py"
EXPECTED_GATE_SHA256 = "c9bdbe929afa868e94e991835f3bd52972d67d5ef9aedb80605c21ec489d1e00"
EXPECTED_MANIFEST_SHA256 = "d213424dd81940a51bde8e886e640297afcb0648bf7c5d712670e405cd29a7cc"
PCM24_SCALE = 8388608.0
FLOOR_DB = -300.0
TRUE_PEAK_OVERSAMPLE = 8

# ITU-R BS.1770 K-weighting coefficients for 48 kHz.  Stage 1 is the
# pre-filter/high shelf and stage 2 is the RLB high-pass.  Versioned here so a
# report is reproducible without a network package or host audio stack.
K_WEIGHTING_48K = (
    ((1.53512485958697, -2.69169618940638, 1.19839281085285),
     (1.0, -1.69065929318241, 0.73248077421585)),
    ((1.0, -2.0, 1.0),
     (1.0, -1.99004745483398, 0.99007225036621)),
)


def sha256(path: Path) -> str:
    return hashlib.sha256(path.read_bytes()).hexdigest()


def db20(value: float) -> float:
    return 20.0 * math.log10(value) if value > 0.0 else FLOOR_DB


def rounded(value: float) -> float:
    return round(value, 6)


def rms(values) -> float:
    total = 0.0
    count = 0
    for value in values:
        total += value * value
        count += 1
    return math.sqrt(total / count) if count else 0.0


def decode_pcm24(path: Path) -> tuple[int, int, int, list[array]]:
    try:
        with wave.open(str(path), "rb") as source:
            channels = source.getnchannels()
            sample_width = source.getsampwidth()
            sample_rate = source.getframerate()
            frame_count = source.getnframes()
            compression = source.getcomptype()
            raw = source.readframes(frame_count)
    except (OSError, wave.Error) as exc:
        raise ValueError(f"{path}: invalid RIFF/WAVE: {exc}") from exc
    if channels not in (1, 2):
        raise ValueError(f"{path}: only mono/stereo measurement is supported")
    if sample_width != 3 or sample_rate != 48000 or compression != "NONE":
        raise ValueError(f"{path}: expected uncompressed 48 kHz PCM24")
    expected = frame_count * channels * sample_width
    if len(raw) != expected:
        raise ValueError(f"{path}: truncated PCM data")
    decoded = [array("d") for _ in range(channels)]
    sample_index = 0
    for offset in range(0, len(raw), 3):
        value = raw[offset] | (raw[offset + 1] << 8) | (raw[offset + 2] << 16)
        if value & 0x800000:
            value -= 1 << 24
        decoded[sample_index % channels].append(value / PCM24_SCALE)
        sample_index += 1
    return channels, sample_rate, frame_count, decoded


def biquad(values: array, coefficients) -> array:
    b, a = coefficients
    x1 = x2 = y1 = y2 = 0.0
    output = array("d")
    append = output.append
    for x0 in values:
        y0 = b[0] * x0 + b[1] * x1 + b[2] * x2 - a[1] * y1 - a[2] * y2
        append(y0)
        x2, x1 = x1, x0
        y2, y1 = y1, y0
    return output


def k_weight(values: array, minimum_frames: int) -> array:
    padded = array("d", values)
    if len(padded) < minimum_frames:
        padded.extend([0.0] * (minimum_frames - len(padded)))
    for stage in K_WEIGHTING_48K:
        padded = biquad(padded, stage)
    return padded


def bs1770_integrated_lufs(channels: list[array], sample_rate: int) -> dict:
    block_frames = round(0.400 * sample_rate)
    step_frames = round(0.100 * sample_rate)
    channel_block_energy: list[list[float]] = []
    block_count = None
    for values in channels:
        filtered = k_weight(values, block_frames)
        starts = list(range(0, len(filtered) - block_frames + 1, step_frames))
        energies = []
        prefix = array("d", [0.0])
        running = 0.0
        for value in filtered:
            running += value * value
            prefix.append(running)
        for start in starts:
            energies.append((prefix[start + block_frames] - prefix[start]) /
                            block_frames)
        channel_block_energy.append(energies)
        block_count = len(energies) if block_count is None else block_count
        if len(energies) != block_count:
            raise ValueError("BS.1770 channel frame-count mismatch")
    combined = [sum(values[index] for values in channel_block_energy)
                for index in range(block_count or 0)]
    loudness = [(-0.691 + 10.0 * math.log10(value)) if value > 0.0 else FLOOR_DB
                for value in combined]
    absolute = [energy for energy, level in zip(combined, loudness)
                if level > -70.0]
    if not absolute:
        return {"integrated_lufs": FLOOR_DB, "block_count": len(combined),
                "absolute_gated_block_count": 0, "relative_gated_block_count": 0,
                "relative_gate_lufs": FLOOR_DB,
                "short_program_zero_padded_to_400ms":
                    len(channels[0]) < block_frames}
    absolute_mean = sum(absolute) / len(absolute)
    relative_gate = -0.691 + 10.0 * math.log10(absolute_mean) - 10.0
    relative = [energy for energy, level in zip(combined, loudness)
                if level > -70.0 and level > relative_gate]
    integrated = (-0.691 + 10.0 * math.log10(sum(relative) / len(relative))) \
        if relative else FLOOR_DB
    return {"integrated_lufs": rounded(integrated),
            "block_count": len(combined),
            "absolute_gated_block_count": len(absolute),
            "relative_gated_block_count": len(relative),
            "relative_gate_lufs": rounded(relative_gate),
            "short_program_zero_padded_to_400ms":
                len(channels[0]) < block_frames}


def cubic_true_peak(values: array) -> float:
    """Return an 8x cubic-Hermite reconstructed inter-sample peak estimate."""
    if not values:
        return 0.0
    peak = max(abs(value) for value in values)
    last = len(values) - 1
    for index in range(last):
        p0 = values[index - 1] if index > 0 else 0.0
        p1 = values[index]
        p2 = values[index + 1]
        p3 = values[index + 2] if index + 2 <= last else 0.0
        a = -p0 + 3.0 * p1 - 3.0 * p2 + p3
        b = 2.0 * p0 - 5.0 * p1 + 4.0 * p2 - p3
        c = -p0 + p2
        for phase in range(1, TRUE_PEAK_OVERSAMPLE):
            t = phase / TRUE_PEAK_OVERSAMPLE
            value = 0.5 * (((a * t + b) * t + c) * t + 2.0 * p1)
            peak = max(peak, abs(value))
    return peak


def zero_crossing_rate(values: array) -> float:
    if len(values) < 2:
        return 0.0
    return sum((a < 0.0 <= b) or (a >= 0.0 > b)
               for a, b in zip(values, values[1:])) / (len(values) - 1)


def difference_rms(values: array) -> float:
    return rms(b - a for a, b in zip(values, values[1:]))


def flat_top_run_count(channels: list[array]) -> int:
    count = 0
    for values in channels:
        channel_peak = max((abs(value) for value in values), default=0.0)
        run = 0
        previous = None
        for value in values:
            if channel_peak > 0.0 and abs(value) == channel_peak and value == previous:
                run += 1
                if run == 1:
                    count += 1
            else:
                run = 0
            previous = value
    return count


def one_shot_metrics(values: array, sample_rate: int) -> dict:
    threshold = 10.0 ** (-60.0 / 20.0)
    above = [index for index, value in enumerate(values) if abs(value) >= threshold]
    onset = above[0] if above else 0
    last_active = above[-1] if above else 0
    silence = round(0.010 * sample_rate)
    active_start = onset if onset >= silence else 0
    active_end = last_active + 1 if len(values) - 1 - last_active >= silence else len(values)
    active = values[active_start:active_end]
    peak_index = max(range(len(values)), key=lambda index: abs(values[index])) \
        if values else 0
    peak = max((abs(value) for value in values), default=0.0)
    active_rms = rms(active)
    tail = values[-silence:] if values else array("d")
    return {
        "onset_sample_index_minus60_dbfs": onset,
        "onset_to_peak_ms": rounded(max(0, peak_index - onset) * 1000.0 /
                                     sample_rate),
        "active_region_start_frame": active_start,
        "active_region_end_frame_exclusive": active_end,
        "active_region_rms_dbfs": rounded(db20(active_rms)),
        "active_crest_factor_db": rounded(db20(peak) - db20(active_rms)),
        "last_10ms_rms_dbfs": rounded(db20(rms(tail))),
        "first_sample_linear": rounded(abs(values[0]) if values else 0.0),
        "last_sample_linear": rounded(abs(values[-1]) if values else 0.0),
    }


def loop_metrics(channels: list[array], sample_rate: int) -> dict:
    window = round(0.100 * sample_rate)
    rows = []
    for index, values in enumerate(channels):
        head = values[:window]
        tail = values[-window:]
        rows.append({
            "channel_index": index,
            "boundary_sample_jump_dbfs": rounded(db20(abs(values[0] - values[-1]))),
            "head_tail_100ms_rms_delta_db": rounded(abs(db20(rms(head)) -
                                                         db20(rms(tail)))),
            "head_tail_100ms_zero_crossing_rate_delta": rounded(abs(
                zero_crossing_rate(head) - zero_crossing_rate(tail))),
            "head_tail_100ms_first_difference_rms_delta_db": rounded(abs(
                db20(difference_rms(head)) - db20(difference_rms(tail)))),
        })
    return {
        "boundary_sample_jump_dbfs_max": max(
            row["boundary_sample_jump_dbfs"] for row in rows),
        "head_tail_100ms_rms_delta_db_max": max(
            row["head_tail_100ms_rms_delta_db"] for row in rows),
        "head_tail_100ms_zero_crossing_rate_delta_max": max(
            row["head_tail_100ms_zero_crossing_rate_delta"] for row in rows),
        "head_tail_100ms_first_difference_rms_delta_db_max": max(
            row["head_tail_100ms_first_difference_rms_delta_db"] for row in rows),
        "per_channel": rows,
    }


def measure_wav(path: Path, asset_type: str) -> dict:
    channels_count, sample_rate, frame_count, channels = decode_pcm24(path)
    sample_peak = max((abs(value) for values in channels for value in values),
                      default=0.0)
    true_peaks = [cubic_true_peak(values) for values in channels]
    full_rms = rms(value for values in channels for value in values)
    result = {
        "container": "RIFF_WAVE",
        "encoding": "PCM_SIGNED_LE",
        "sample_rate_hz": sample_rate,
        "bits_per_sample": 24,
        "channels": channels_count,
        "frame_count": frame_count,
        "duration_seconds": round(frame_count / sample_rate, 9),
        "sample_peak_dbfs": rounded(db20(sample_peak)),
        "true_peak_dbtp": rounded(db20(max(true_peaks, default=0.0))),
        "true_peak_per_channel_dbtp": [rounded(db20(value)) for value in true_peaks],
        "unweighted_full_file_rms_dbfs": rounded(db20(full_rms)),
        "full_scale_sample_count": sum(abs(value) >= (8388607.0 / PCM24_SCALE)
                                       for values in channels for value in values),
        "flat_top_run_count": flat_top_run_count(channels),
        "bs1770": bs1770_integrated_lufs(channels, sample_rate),
    }
    if asset_type.startswith("one_shot"):
        if channels_count != 1:
            raise ValueError(f"{path}: one-shot must be mono")
        result["one_shot"] = one_shot_metrics(channels[0], sample_rate)
    elif asset_type == "stereo_loop":
        if channels_count != 2:
            raise ValueError(f"{path}: loop must be stereo")
        result["loop"] = loop_metrics(channels, sample_rate)
    else:
        raise ValueError(f"unknown asset type: {asset_type}")
    return result


def threshold_checks(asset: dict, measurement: dict, gate: dict) -> list[dict]:
    future = gate["future_content_acceptance_gates"]
    peak_gate = future["loudness_and_peak"]
    transient = future["transient_integrity"]
    loop_gate = future["loop_integrity"]
    clipping = future["no_clipping"]
    checks = []

    def add(name: str, passed: bool, value, threshold) -> None:
        checks.append({"check": name, "result": "PASS" if passed else "FAIL",
                       "measured": value, "threshold": threshold})

    add("delivery_sample_rate_hz", measurement["sample_rate_hz"] == 48000,
        measurement["sample_rate_hz"], 48000)
    add("delivery_bits_per_sample", measurement["bits_per_sample"] == 24,
        measurement["bits_per_sample"], 24)
    expected_channels = 1 if asset["asset_type"].startswith("one_shot") else 2
    add("delivery_channel_count", measurement["channels"] == expected_channels,
        measurement["channels"], expected_channels)
    add("sample_peak_dbfs", measurement["sample_peak_dbfs"] <=
        peak_gate["all_assets_sample_peak_dbfs_lte"],
        measurement["sample_peak_dbfs"],
        {"lte": peak_gate["all_assets_sample_peak_dbfs_lte"]})
    add("true_peak_dbtp", measurement["true_peak_dbtp"] <=
        peak_gate["all_assets_true_peak_dbtp_lte"],
        measurement["true_peak_dbtp"],
        {"lte": peak_gate["all_assets_true_peak_dbtp_lte"]})
    add("full_scale_sample_count", measurement["full_scale_sample_count"] ==
        clipping["full_scale_sample_count"], measurement["full_scale_sample_count"],
        clipping["full_scale_sample_count"])
    add("flat_top_run_count", measurement["flat_top_run_count"] ==
        clipping["flat_top_run_count"], measurement["flat_top_run_count"],
        clipping["flat_top_run_count"])
    if asset["asset_type"].startswith("one_shot"):
        values = measurement["one_shot"]
        onset_limit = transient["impact_onset_to_peak_ms_lte"] if \
            asset["asset_type"] == "one_shot_transient" else \
            transient["notification_onset_to_peak_ms_lte"]
        add("onset_to_peak_ms", values["onset_to_peak_ms"] <= onset_limit,
            values["onset_to_peak_ms"], {"lte": onset_limit})
        crest_range = transient["one_shot_active_crest_factor_db_range"]
        add("active_crest_factor_db", crest_range[0] <=
            values["active_crest_factor_db"] <= crest_range[1],
            values["active_crest_factor_db"], {"range_inclusive": crest_range})
        rms_range = peak_gate["effects_and_ui_active_region_rms_dbfs_range"]
        add("active_region_rms_dbfs", rms_range[0] <=
            values["active_region_rms_dbfs"] <= rms_range[1],
            values["active_region_rms_dbfs"], {"range_inclusive": rms_range})
        add("last_10ms_rms_dbfs", values["last_10ms_rms_dbfs"] <=
            transient["one_shot_last_10ms_rms_dbfs_lte"],
            values["last_10ms_rms_dbfs"],
            {"lte": transient["one_shot_last_10ms_rms_dbfs_lte"]})
        add("endpoint_zero_samples", values["first_sample_linear"] == 0.0 and
            values["last_sample_linear"] == 0.0,
            [values["first_sample_linear"], values["last_sample_linear"]],
            [0.0, 0.0])
    else:
        values = measurement["loop"]
        add("loop_minimum_duration_seconds", measurement["duration_seconds"] >=
            loop_gate["minimum_duration_seconds"], measurement["duration_seconds"],
            {"gte": loop_gate["minimum_duration_seconds"]})
        add("explicit_full_file_loop_contract",
            asset.get("loop_contract") == "explicit_full_file_loop",
            asset.get("loop_contract"), "explicit_full_file_loop")
        add("loop_boundary_sample_jump_dbfs",
            values["boundary_sample_jump_dbfs_max"] <=
            loop_gate["boundary_sample_jump_dbfs_lte"],
            values["boundary_sample_jump_dbfs_max"],
            {"lte": loop_gate["boundary_sample_jump_dbfs_lte"]})
        add("loop_head_tail_100ms_rms_delta_db",
            values["head_tail_100ms_rms_delta_db_max"] <=
            loop_gate["head_tail_100ms_rms_delta_db_lte"],
            values["head_tail_100ms_rms_delta_db_max"],
            {"lte": loop_gate["head_tail_100ms_rms_delta_db_lte"]})
        # The design gate requires a machine spectral/rhythmic result but does
        # not define a scalar.  Preserve the candidate validator's frozen proxy
        # thresholds and do not present them as a listening-seam judgment.
        proxy_pass = values["head_tail_100ms_zero_crossing_rate_delta_max"] <= 0.02 and \
            values["head_tail_100ms_first_difference_rms_delta_db_max"] <= 1.0
        add("spectral_or_rhythmic_discontinuity_machine_proxy", proxy_pass,
            {"zcr_delta": values["head_tail_100ms_zero_crossing_rate_delta_max"],
             "first_difference_rms_delta_db":
                values["head_tail_100ms_first_difference_rms_delta_db_max"]},
            {"zcr_delta_lte": 0.02,
             "first_difference_rms_delta_db_lte": 1.0})
        target, tolerance = ((peak_gate["music_integrated_lufs_target"],
                              peak_gate["music_integrated_lufs_tolerance"])
                             if asset["semantic_id"] == "music_loop" else
                             (peak_gate["ambience_integrated_lufs_target"],
                              peak_gate["ambience_integrated_lufs_tolerance"]))
        lufs = measurement["bs1770"]["integrated_lufs"]
        add("bs1770_integrated_lufs_target", target - tolerance <= lufs <=
            target + tolerance, lufs,
            {"target": target, "tolerance": tolerance})
    return checks


def build_report() -> dict:
    if sha256(DESIGN_GATE) != EXPECTED_GATE_SHA256:
        raise ValueError("Phase5 audio design-gate SHA drift")
    if REQUESTED_MANIFEST.exists():
        raise ValueError("requested manifest alias unexpectedly exists; re-freeze identity")
    if sha256(RESOLVED_MANIFEST) != EXPECTED_MANIFEST_SHA256:
        raise ValueError("Phase5 audio candidate-manifest SHA drift")
    gate = json.loads(DESIGN_GATE.read_text(encoding="utf-8"))
    manifest = json.loads(RESOLVED_MANIFEST.read_text(encoding="utf-8"))
    assets = []
    for source in manifest["assets"]:
        wav = RESOLVED_MANIFEST.parent / source["filename"]
        expected_file_sha = source["measurements"]["file_sha256"].removeprefix("sha256:")
        if sha256(wav) != expected_file_sha:
            raise ValueError(f"WAV binding drift: {source['semantic_id']}")
        measurement = measure_wav(wav, source["asset_type"])
        checks = threshold_checks(source, measurement, gate)
        assets.append({
            "semantic_id": source["semantic_id"],
            "asset_type": source["asset_type"],
            "bus": source["bus"],
            "filename": source["filename"],
            "file_sha256": expected_file_sha,
            "measurements": measurement,
            "threshold_checks": checks,
            "technical_threshold_result": "PASS" if all(
                item["result"] == "PASS" for item in checks) else "FAIL",
            "content_acceptance_status": "NOT_CONTENT_ACCEPTED",
        })
    failed = [asset["semantic_id"] for asset in assets
              if asset["technical_threshold_result"] != "PASS"]
    return {
        "schema": "billiardgl.phase5.audio_technical_machine_measurement.v1",
        "status": "TECHNICAL_MACHINE_MEASUREMENT",
        "bindings": {
            "design_gate_path": str(DESIGN_GATE.relative_to(ROOT)),
            "design_gate_sha256": EXPECTED_GATE_SHA256,
            "requested_manifest_path": str(REQUESTED_MANIFEST.relative_to(ROOT)),
            "requested_manifest_path_missing": True,
            "resolved_manifest_path": str(RESOLVED_MANIFEST.relative_to(ROOT)),
            "resolved_manifest_sha256": EXPECTED_MANIFEST_SHA256,
            "resolution_reason": "requested path absent; exact instructed SHA resolves uniquely to existing candidate manifest",
        },
        "implementation": {
            "measurement_script": str(Path(__file__).resolve().relative_to(ROOT)),
            "measurement_script_sha256": sha256(Path(__file__).resolve()),
            "validator_script": str(VALIDATOR.relative_to(ROOT)),
            "validator_script_sha256": sha256(VALIDATOR),
            "python_version": platform.python_version(),
            "standard_library_only": True,
            "bs1770_revision": "ITU-R BS.1770 block gating and 48kHz K-weighting coefficients; mono/stereo channel weights 1.0",
            "bs1770_block_seconds": 0.400,
            "bs1770_block_overlap_fraction": 0.75,
            "bs1770_absolute_gate_lufs": -70.0,
            "bs1770_relative_gate_lu": -10.0,
            "short_program_policy": "zero-pad to one 400ms measurement block; recorded per asset",
            "true_peak_method": "8x cubic-Hermite reconstructed inter-sample peak estimate",
            "true_peak_oversample_factor": TRUE_PEAK_OVERSAMPLE,
            "audio_device_open_count": 0,
        },
        "assets": assets,
        "technical_summary": {
            "asset_count": len(assets),
            "technical_threshold_pass_count": len(assets) - len(failed),
            "technical_threshold_fail_count": len(failed),
            "failed_asset_ids": failed,
            "result": "PASS_TECHNICAL_THRESHOLDS_ONLY" if not failed else
                "FAIL_TECHNICAL_THRESHOLDS",
        },
        "evidence_boundary": {
            "offline_machine_measurement_completed": True,
            "audio_device_playback_completed": False,
            "human_listening_completed": False,
            "external_semantic_blind_test_completed": False,
            "authorship_or_rights_verified": False,
            "content_accepted": False,
            "product_integration_authorized": False,
            "beta_promotion_authorized": False,
        },
        "remaining_gates": [
            "independent original-resolution human listening and three-cycle loop review",
            "actual reference-headphone and representative-integrated-speaker playback",
            "five-reviewer randomized semantic blind test",
            "authorship/source/creation-date and chain-of-title or compatible-license review",
            "content disposition and acceptance",
            "AudioAssetPolicy/product integration and Beta reconciliation after acceptance",
        ],
    }


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--output", type=Path, required=True)
    args = parser.parse_args()
    try:
        report = build_report()
    except (OSError, ValueError, KeyError, json.JSONDecodeError) as exc:
        print(f"Phase5 audio technical measurement FAIL: {exc}", file=sys.stderr)
        return 1
    args.output.parent.mkdir(parents=True, exist_ok=True)
    args.output.write_text(json.dumps(report, indent=2, sort_keys=True) + "\n",
                           encoding="utf-8")
    print("Phase5 audio technical measurement complete: " +
          report["technical_summary"]["result"] +
          " content=NOT_ACCEPTED device=NOT_RUN")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
