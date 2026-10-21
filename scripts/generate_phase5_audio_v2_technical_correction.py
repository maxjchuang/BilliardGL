#!/usr/bin/env python3
"""Generate the authorized Phase-5 v2 machine-only technical correction.

Only the two loop data chunks receive their design-gate constant gains.  The
eight one-shots are byte copies.  This tool never opens an audio device and
does not modify v1 assets, policy, product source, or acceptance state.
"""

from __future__ import annotations

import argparse
import hashlib
import json
import math
from pathlib import Path
import shutil
import sys

sys.path.insert(0, str(Path(__file__).resolve().parent))
from measure_phase5_audio_technical import measure_wav, threshold_checks  # noqa: E402


ROOT = Path(__file__).resolve().parents[1]
DESIGN_GATE = ROOT / "docs/art/phase5-audio-v2-technical-correction-design-gate-v1.json"
V1_DESIGN_GATE = ROOT / "docs/art/phase5-audio-content-remediation-design-gate-v1.json"
V1_MANIFEST = ROOT / "assets/audio/phase5-v1/phase5-audio-content-candidate-manifest-v1.json"
V1_REPORT = ROOT / "docs/art/phase5-audio-technical-machine-measurement-v1/report.json"
V1_ROOT = V1_MANIFEST.parent
DEFAULT_OUTPUT_ROOT = ROOT / "assets/audio/phase5-v2-technical-correction"
DEFAULT_REPORT = ROOT / "docs/art/phase5-audio-v2-technical-correction-machine-measurement-v1/report.json"
CANDIDATE_MANIFEST_NAME = "phase5-audio-v2-technical-correction-candidate-manifest-v1.json"
VALIDATOR = ROOT / "scripts/validate_phase5_audio_v2_technical_correction.py"
TESTS = ROOT / "tests/visual/test_phase5_audio_v2_technical_correction.py"
MEASUREMENT = ROOT / "scripts/measure_phase5_audio_technical.py"
GATE_SHA256 = "276fb64e2d9484070784745d1bff1b637ad2bff6689161b585a4ab004e507b1f"
V1_DESIGN_GATE_SHA256 = "c9bdbe929afa868e94e991835f3bd52972d67d5ef9aedb80605c21ec489d1e00"
V1_MANIFEST_SHA256 = "d213424dd81940a51bde8e886e640297afcb0648bf7c5d712670e405cd29a7cc"
V1_REPORT_SHA256 = "28d15d3c7cceded3cd7be5832cb439d93edbbf9a912832e1ca74d7fe6859e2bc"
LOOP_GAIN_DB = {"music_loop": -7.36, "venue_ambience": -4.98}
STRICT_LUFS = {"music_loop": (-25.95, -22.05),
               "venue_ambience": (-32.95, -27.05)}


def sha256(path: Path) -> str:
    return hashlib.sha256(path.read_bytes()).hexdigest()


def verify_bindings() -> tuple[dict, dict, dict]:
    expected = ((DESIGN_GATE, GATE_SHA256),
                (V1_DESIGN_GATE, V1_DESIGN_GATE_SHA256),
                (V1_MANIFEST, V1_MANIFEST_SHA256),
                (V1_REPORT, V1_REPORT_SHA256))
    for path, digest in expected:
        if not path.is_file() or sha256(path) != digest:
            raise ValueError(f"immutable binding drift: {path}")
    gate = json.loads(DESIGN_GATE.read_text(encoding="utf-8"))
    v1_gate = json.loads(V1_DESIGN_GATE.read_text(encoding="utf-8"))
    manifest = json.loads(V1_MANIFEST.read_text(encoding="utf-8"))
    return gate, v1_gate, manifest


def riff_data_range(raw: bytes) -> tuple[int, int]:
    if len(raw) < 12 or raw[:4] != b"RIFF" or raw[8:12] != b"WAVE":
        raise ValueError("invalid RIFF/WAVE identity")
    if int.from_bytes(raw[4:8], "little") + 8 != len(raw):
        raise ValueError("RIFF byte size differs")
    offset = 12
    found = []
    while offset + 8 <= len(raw):
        size = int.from_bytes(raw[offset + 4:offset + 8], "little")
        start = offset + 8
        end = start + size
        if end > len(raw):
            raise ValueError("truncated RIFF chunk")
        if raw[offset:offset + 4] == b"data":
            found.append((start, size))
        offset = end + (size & 1)
    if offset != len(raw) or len(found) != 1:
        raise ValueError("exactly one complete data chunk required")
    return found[0]


def decode_pcm24(sample: bytes) -> int:
    value = sample[0] | (sample[1] << 8) | (sample[2] << 16)
    return value - (1 << 24) if value & 0x800000 else value


def encode_pcm24(value: int) -> bytes:
    encoded = value + (1 << 24) if value < 0 else value
    return bytes((encoded & 255, (encoded >> 8) & 255,
                  (encoded >> 16) & 255))


def quantize_ties_away(value: int, gain: float) -> tuple[int, bool]:
    magnitude = math.floor(abs(value * gain) + 0.5)
    result = -magnitude if value < 0 else magnitude
    saturated = result < -8388608 or result > 8388607
    return max(-8388608, min(8388607, result)), saturated


def transform_loop_bytes(source: bytes, gain_db: float) -> tuple[bytes, dict]:
    start, size = riff_data_range(source)
    if size % 6:
        raise ValueError("stereo PCM24 data size must be divisible by six")
    gain = 10.0 ** (gain_db / 20.0)
    output = bytearray(source)
    saturation_count = 0
    maximum_input = 0
    maximum_output = 0
    sample_count = 0
    for offset in range(start, start + size, 3):
        input_value = decode_pcm24(source[offset:offset + 3])
        output_value, saturated = quantize_ties_away(input_value, gain)
        output[offset:offset + 3] = encode_pcm24(output_value)
        maximum_input = max(maximum_input, abs(input_value))
        maximum_output = max(maximum_output, abs(output_value))
        saturation_count += saturated
        sample_count += 1
    result = bytes(output)
    output_start, output_size = riff_data_range(result)
    if (output_start, output_size) != (start, size) or \
            source[:start] != result[:start] or \
            source[start + size:] != result[start + size:]:
        raise ValueError("non-data RIFF bytes changed")
    channels = 2
    first = [decode_pcm24(result[start + index * 3:start + index * 3 + 3])
             for index in range(channels)]
    last_start = start + size - channels * 3
    last = [decode_pcm24(result[last_start + index * 3:
                                   last_start + index * 3 + 3])
            for index in range(channels)]
    if first != [0, 0] or last != [0, 0]:
        raise ValueError("loop endpoint zero contract changed")
    if saturation_count != 0:
        raise ValueError("PCM24 saturation unexpectedly triggered")
    return result, {
        "gain_db": gain_db,
        "linear_gain": gain,
        "quantizer": "round_to_nearest_ties_away_from_zero_then_saturate_signed24",
        "interleaved_sample_count": sample_count,
        "saturated_sample_count": saturation_count,
        "input_maximum_absolute_pcm24_integer": maximum_input,
        "output_maximum_absolute_pcm24_integer": maximum_output,
        "first_stereo_frame": first,
        "last_stereo_frame": last,
        "non_data_riff_bytes_identical": True,
        "data_chunk_byte_count": size,
    }


def audit_loop_mapping(source_path: Path, output_path: Path,
                       gain_db: float) -> dict:
    expected, metrics = transform_loop_bytes(source_path.read_bytes(), gain_db)
    if output_path.read_bytes() != expected:
        raise ValueError(f"output sample mapping differs: {output_path.name}")
    return metrics


def asset_entries(output_root: Path) -> list[dict]:
    gate, v1_gate, v1_manifest = verify_bindings()
    gate_one_shots = {row["semantic_id"]: row for row in
                      gate["unchanged_one_shot_byte_contract"]}
    rows = []
    for source in v1_manifest["assets"]:
        semantic_id = source["semantic_id"]
        output_path = output_root / source["filename"]
        if not output_path.is_file():
            raise ValueError(f"v2 WAV missing: {semantic_id}")
        if semantic_id in LOOP_GAIN_DB:
            transform = {"operation": "constant_linear_gain_full_file_all_channels",
                         **audit_loop_mapping(V1_ROOT / source["filename"],
                                              output_path,
                                              LOOP_GAIN_DB[semantic_id])}
        else:
            expected = gate_one_shots.get(semantic_id)
            if expected is None or sha256(V1_ROOT / source["filename"]) != \
                    expected["sha256"] or \
                    output_path.read_bytes() != (V1_ROOT /
                                                   source["filename"]).read_bytes():
                raise ValueError(f"one-shot byte identity differs: {semantic_id}")
            transform = {"operation": "byte_identical_copy",
                         "source_bytes_equal_output_bytes": True}
        measurement = measure_wav(output_path, source["asset_type"])
        checks = threshold_checks(source, measurement, v1_gate)
        if semantic_id in STRICT_LUFS:
            low, high = STRICT_LUFS[semantic_id]
            actual = measurement["bs1770"]["integrated_lufs"]
            checks.append({"check": "reserved_integrated_lufs_interval",
                           "measured": actual,
                           "threshold": {"inclusive": [low, high]},
                           "result": "PASS" if low <= actual <= high else "FAIL"})
        rows.append({
            "semantic_id": semantic_id,
            "filename": source["filename"],
            "asset_type": source["asset_type"],
            "bus": source["bus"],
            "source_file_sha256": sha256(V1_ROOT / source["filename"]),
            "output_file_sha256": sha256(output_path),
            "transformation": transform,
            "measurements": measurement,
            "threshold_checks": checks,
            "technical_threshold_result": "PASS" if all(
                check["result"] == "PASS" for check in checks) else "FAIL",
            "content_acceptance_status": "NOT_CONTENT_ACCEPTED",
        })
    return rows


def build_manifest(output_root: Path) -> dict:
    assets = asset_entries(output_root)
    failed = [row["semantic_id"] for row in assets
              if row["technical_threshold_result"] != "PASS"]
    return {
        "schema": "billiardgl.phase5.audio_v2_technical_correction_candidate.v1",
        "status": "MACHINE_TECHNICAL_CANDIDATE_NOT_CONTENT_ACCEPTED",
        "bindings": {
            "technical_correction_design_gate_sha256": GATE_SHA256,
            "v1_design_gate_sha256": V1_DESIGN_GATE_SHA256,
            "v1_candidate_manifest_sha256": V1_MANIFEST_SHA256,
            "v1_machine_report_sha256": V1_REPORT_SHA256,
            "generator_sha256": sha256(Path(__file__).resolve()),
            "validator_sha256": sha256(VALIDATOR),
            "bad_input_and_determinism_tests_sha256": sha256(TESTS),
            "measurement_implementation_sha256": sha256(MEASUREMENT),
        },
        "asset_root_identity": "assets/audio/phase5-v2-technical-correction",
        "asset_count": len(assets),
        "assets": assets,
        "technical_summary": {
            "pass_count": len(assets) - len(failed),
            "fail_count": len(failed),
            "failed_asset_ids": failed,
            "result": "PASS_TECHNICAL_THRESHOLDS_ONLY" if not failed else
                "FAIL_TECHNICAL_THRESHOLDS",
        },
        "evidence_boundary": {
            "offline_machine_measurement_completed": True,
            "audio_playback_completed": False,
            "actual_device_playback_completed": False,
            "human_listening_completed": False,
            "semantic_blind_test_completed": False,
            "authorship_or_rights_verified": False,
            "content_accepted": False,
            "product_integration_authorized": False,
            "beta_promotion_authorized": False,
        },
    }


def build_report(manifest: dict, manifest_sha256: str) -> dict:
    loop_rows = [row for row in manifest["assets"]
                 if row["semantic_id"] in LOOP_GAIN_DB]
    return {
        "schema": "billiardgl.phase5.audio_v2_technical_correction_machine_measurement.v1",
        "status": "TECHNICAL_MACHINE_MEASUREMENT",
        "candidate_manifest_sha256": manifest_sha256,
        "bindings": manifest["bindings"],
        "technical_summary": manifest["technical_summary"],
        "corrected_loop_measurements": loop_rows,
        "one_shot_byte_identical_count": sum(
            row["transformation"]["operation"] == "byte_identical_copy"
            for row in manifest["assets"]),
        "audio_device_open_count": 0,
        "evidence_boundary": manifest["evidence_boundary"],
        "remaining_gates": [
            "independent original-resolution human listening and three-cycle loop review",
            "actual reference-headphone and representative-integrated-speaker playback",
            "five-reviewer randomized semantic blind test",
            "authorship/source/date and chain-of-title or compatible-license review",
            "content disposition and acceptance",
            "AudioAssetPolicy/product integration and Beta reconciliation after acceptance",
        ],
    }


def write_json(path: Path, value: dict) -> None:
    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_text(json.dumps(value, indent=2, sort_keys=True) + "\n",
                    encoding="utf-8")


def generate(output_root: Path, report_path: Path) -> tuple[Path, Path]:
    gate, _, v1_manifest = verify_bindings()
    if output_root.exists():
        raise ValueError("v2 output root already exists; overwrite forbidden")
    if report_path.exists():
        raise ValueError("v2 report already exists; overwrite forbidden")
    if output_root.resolve() == V1_ROOT.resolve():
        raise ValueError("v1 root mutation forbidden")
    output_root.mkdir(parents=True)
    gate_one_shots = {row["semantic_id"]: row for row in
                      gate["unchanged_one_shot_byte_contract"]}
    for asset in v1_manifest["assets"]:
        semantic_id = asset["semantic_id"]
        source = V1_ROOT / asset["filename"]
        target = output_root / asset["filename"]
        if semantic_id in LOOP_GAIN_DB:
            transformed, _ = transform_loop_bytes(
                source.read_bytes(), LOOP_GAIN_DB[semantic_id])
            target.write_bytes(transformed)
        else:
            if sha256(source) != gate_one_shots[semantic_id]["sha256"]:
                raise ValueError(f"one-shot source SHA drift: {semantic_id}")
            shutil.copyfile(source, target)
    manifest = build_manifest(output_root)
    manifest_path = output_root / CANDIDATE_MANIFEST_NAME
    write_json(manifest_path, manifest)
    report = build_report(manifest, sha256(manifest_path))
    write_json(report_path, report)
    return manifest_path, report_path


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--output-root", type=Path, default=DEFAULT_OUTPUT_ROOT)
    parser.add_argument("--report", type=Path, default=DEFAULT_REPORT)
    args = parser.parse_args()
    try:
        manifest, report = generate(args.output_root.resolve(),
                                    args.report.resolve())
    except (OSError, ValueError, KeyError, json.JSONDecodeError) as exc:
        print("Phase5 audio v2 generation FAIL: " + str(exc), file=sys.stderr)
        return 1
    print(f"Phase5 audio v2 generated: {manifest} report={report} "
          "content=NOT_ACCEPTED device=NOT_RUN")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
