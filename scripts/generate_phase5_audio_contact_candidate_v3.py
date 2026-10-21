#!/usr/bin/env python3
"""Dry contact-sound review candidates: internal noise/modal synthesis only.

No recording/sample download, human listening or rights certification. Keep v2
immutable; replace only five contact masters in a new explicit review directory.
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
from generate_phase5_audio_content_candidate import (
    RepositoryNoise, SAMPLE_RATE, _quantize_normalized, _write_pcm24,
)
from measure_phase5_audio_technical import measure_wav, threshold_checks

ROOT = Path(__file__).resolve().parents[1]
PARENT = ROOT / "assets/audio/phase5-v2-technical-correction"
PARENT_MANIFEST = PARENT / "phase5-audio-v2-technical-correction-candidate-manifest-v1.json"
PARENT_SHA = "ca2267ac9ad2d020f0bbb1cbe935ed544c4f6b0875ffd53e101108bffaadcc10"
GATE = ROOT / "docs/art/phase5-audio-content-remediation-design-gate-v1.json"
DEFAULT_OUTPUT = ROOT / "assets/audio/phase5-v3-contact-review"
MANIFEST_NAME = "phase5-audio-v3-contact-review-manifest.json"

# Modal frequencies are design parameters, not measured modes of a real ball.
# (frequency Hz, amplitude decay seconds, weight): deliberately non-harmonic,
# very short resonances driven alongside deterministic band-limited noise.
CONTACTS = {
    "cue_impact": {"duration": .140, "peak_dbfs": -3.0, "seed": 0xC0E30001,
        "low_hz": 350, "high_hz": 4800, "noise_decay": .004, "noise_weight": .85,
        "modes": [(175,.010,.22),(430,.006,.18),(1130,.003,.08)]},
    "ball_ball": {"duration": .080, "peak_dbfs": -4.0, "seed": 0xBA113003,
        "low_hz": 1500, "high_hz": 11000, "noise_decay": .0016, "noise_weight": 1.0,
        "modes": [(2130,.003,.18),(3370,.0025,.13),(6190,.0017,.08),(8870,.0012,.04)]},
    "rail": {"duration": .180, "peak_dbfs": -5.0, "seed": 0x0A113003,
        "low_hz": 60, "high_hz": 1100, "noise_decay": .012, "noise_weight": 1.0,
        "modes": [(127,.014,.28),(273,.010,.15),(587,.005,.05)]},
    "pocket_jaw": {"duration": .160, "peak_dbfs": -5.5, "seed": 0x0C0F3003,
        "low_hz": 450, "high_hz": 5200, "noise_decay": .003, "noise_weight": 1.0,
        "modes": [(437,.006,.24),(1291,.004,.14),(2939,.002,.07)]},
    "pocket_drop": {"duration": .380, "peak_dbfs": -6.0, "seed": 0xD2030003,
        "low_hz": 70, "high_hz": 2600, "noise_decay": .019, "noise_weight": .6,
        "modes": [(139,.019,.36),(383,.008,.12)],
        "events": [(0.0,1.0),(.057,.28),(.119,.12),(.181,.045)]},
}


def sha256(path: Path) -> str:
    return hashlib.sha256(path.read_bytes()).hexdigest()


def contact_wave(name: str) -> list[int]:
    spec = CONTACTS[name]
    count = round(spec["duration"] * SAMPLE_RATE)
    values = [0.0] * count
    # Explicitly time-separated diminishing cup/net contacts only for drop;
    # all other cues have one short impact. No pitch sweep or sustained tone.
    for event_index, (start, amplitude) in enumerate(spec.get("events", [(0.0,1.0)])):
        noise = RepositoryNoise(spec["seed"] + event_index * 1009)
        low = high = 0.0
        fast = 1 - math.exp(-2 * math.pi * spec["high_hz"] / SAMPLE_RATE)
        slow = 1 - math.exp(-2 * math.pi * spec["low_hz"] / SAMPLE_RATE)
        first = round(start * SAMPLE_RATE)
        for index in range(first, count):
            t = (index - first) / SAMPLE_RATE
            excitation = noise.next()
            high += fast * (excitation - high)
            low += slow * (excitation - low)
            burst = (high - low) * math.exp(-t / spec["noise_decay"])
            body = sum(weight * math.sin(2 * math.pi * frequency * t) * math.exp(-t / decay)
                for frequency, decay, weight in spec["modes"])
            attack = 1 - math.exp(-t / .00008)
            # Last 20ms are tapered to exact zero; final 10ms is silent. The
            # existing technical tail requirement is retained without trimming.
            remaining = spec["duration"] - index / SAMPLE_RATE
            tail = max(0.0, min(1.0, (remaining - .010) / .020)) ** 3
            values[index] += amplitude * attack * tail * (spec["noise_weight"] * burst + body)
    values[0] = values[-1] = 0.0
    return _quantize_normalized(values, spec["peak_dbfs"])


def generate(output: Path) -> dict:
    if sha256(PARENT_MANIFEST) != PARENT_SHA:
        raise ValueError("frozen v2 manifest mismatch")
    parent = json.loads(PARENT_MANIFEST.read_text())
    for asset in parent["assets"]:
        if sha256(PARENT / asset["filename"]) != asset["output_file_sha256"]:
            raise ValueError("frozen v2 master mismatch: " + asset["filename"])
    destination = output.resolve()
    source_root = PARENT.resolve()
    if destination == source_root or source_root in destination.parents or \
            (output.exists() and any(output.iterdir())):
        raise ValueError("new empty output directory required; existing candidates must stay intact")
    output.mkdir(parents=True, exist_ok=True)
    gate = json.loads(GATE.read_text())
    rows = []
    for asset in parent["assets"]:
        name = asset["semantic_id"]
        destination = output / asset["filename"]
        if name in CONTACTS:
            _write_pcm24(destination, 1, contact_wave(name))
        else:
            shutil.copyfile(PARENT / asset["filename"], destination)
        row = {"semantic_id":name,"filename":asset["filename"],
            "asset_type":asset["asset_type"],"bus":asset["bus"],
            "loop_contract":"explicit_full_file_loop" if asset["asset_type"]=="stereo_loop" else None,
            "source_v2_sha256":asset["output_file_sha256"],"output_file_sha256":sha256(destination),
            "operation":"internal_noise_modal_resynthesis" if name in CONTACTS else "byte_identical_copy",
            "content_acceptance_status":"NOT_CONTENT_ACCEPTED"}
        measurement = measure_wav(destination, asset["asset_type"])
        checks = threshold_checks(row, measurement, gate)
        row.update(measurements=measurement, threshold_checks=checks,
            technical_threshold_result="PASS" if all(c["result"]=="PASS" for c in checks) else "FAIL")
        if name in CONTACTS:
            row["synthesis_parameters"] = CONTACTS[name]
        rows.append(row)
    manifest = {"schema":"billiardgl.audio_contact_review_v3.v1","asset_count":10,
        "parent_manifest_sha256":PARENT_SHA,"generator_sha256":sha256(Path(__file__)),
        "technical_gate_sha256":sha256(GATE),"assets":rows,
        "provenance":"Internal deterministic seeded-noise and damped non-harmonic modal synthesis. No recordings, downloads or external samples.",
        "evidence_boundary":{"content_accepted":False,"human_listening_completed":False,
            "rights_certified":False,"real_recording_claimed":False,"shipping_default_promoted":False},
        "technical_result":"PASS" if all(r["technical_threshold_result"]=="PASS" for r in rows) else "FAIL"}
    (output / MANIFEST_NAME).write_text(json.dumps(manifest,indent=2)+"\n")
    return manifest


if __name__ == "__main__":
    parser = argparse.ArgumentParser()
    parser.add_argument("--output-root", type=Path, default=DEFAULT_OUTPUT)
    args = parser.parse_args()
    try:
        report = generate(args.output_root)
        print("v3 contact candidate technical="+report["technical_result"]+" content=NOT_ACCEPTED")
        for row in report["assets"]:
            for check in row["threshold_checks"]:
                if check["result"] != "PASS": print(row["semantic_id"], check)
        sys.exit(0 if report["technical_result"]=="PASS" else 1)
    except (OSError,ValueError) as error:
        print("FAIL:",error); sys.exit(1)
