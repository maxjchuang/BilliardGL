#!/usr/bin/env python3
"""Summarize one retained playtest session; never certify a performance gate."""
import argparse
import hashlib
import json
import math
from pathlib import Path


def percentiles(values):
    ordered = sorted(values)
    return {f"p{p}_ms": ordered[math.ceil(p / 100 * len(ordered)) - 1]
            for p in (50, 95, 99)}


def summarize(records, warmup_frames=0):
    if type(warmup_frames) is not int or warmup_frames < 0:
        raise ValueError("warmup_frames must be a nonnegative integer")
    def frame_id(record):
        value = record["details"]["frame_id"]
        if not isinstance(value, str) or not value.isascii() or not value.isdecimal():
            raise ValueError("frame_id must be a canonical positive integer string")
        number = int(value)
        if number < 1 or str(number) != value:
            raise ValueError("frame_id must be a canonical positive integer string")
        return number

    frames = sorted((r for r in records if r.get("kind") == "swap_submitted"),
                    key=frame_id)
    if not frames:
        raise ValueError("no presented frames")
    ids = [frame_id(r) for r in frames]
    if ids != list(range(1, len(ids) + 1)):
        raise ValueError("incomplete or duplicated frame sequence; logs may have rotated")
    samples = {"loop_through_swap": [], "swap_to_swap": [], "swap": []}
    configurations = {}
    for frame_id, record in zip(ids, frames):
        d = record["details"]
        if (d.get("timing_scope") != "event_loop_through_swap_completion"
                or d.get("frame_timing_valid") is not True
                or d.get("includes_simulation") is not True
                or d.get("includes_display_swap") is not True
                or d.get("measures_scanout") is not False):
            raise ValueError("missing or incompatible full-frame timing contract")
        for key in ("loop_through_swap_ms", "swap_ms"):
            if (type(d.get(key)) not in (int, float)
                    or not math.isfinite(d[key]) or d[key] < 0):
                raise ValueError(f"invalid {key}")
        if d["loop_through_swap_ms"] < d["swap_ms"]:
            raise ValueError("swap exceeds loop")
        cadence = d.get("swap_to_swap_ms")
        if frame_id == 1:
            if cadence is not None:
                raise ValueError("first frame cannot have a measured cadence")
        elif (type(cadence) not in (int, float) or not math.isfinite(cadence)
              or cadence < d["loop_through_swap_ms"]):
            raise ValueError("invalid or missing swap cadence")
        # Frame one has no cadence; all reported series use the same frame set.
        if frame_id > max(1, warmup_frames):
            samples["loop_through_swap"].append(d["loop_through_swap_ms"])
            samples["swap_to_swap"].append(cadence)
            samples["swap"].append(d["swap_ms"])
            config = tuple(d.get(k) for k in ("drawable_width_px", "drawable_height_px",
                                              "render_quality_enum", "requested_msaa_samples"))
            if any(type(v) is not int for v in config) or config[0] <= 0 or config[1] <= 0:
                config = (None, None, None, None)
            configurations[config] = configurations.get(config, 0) + 1
    if not samples["swap"]:
        raise ValueError("no complete samples after warmup")
    return {"schema": "billiardgl_presented_frame_summary_v1",
            "qualifies_as_product_frame_budget": False,
            "limitation": "Requires independently bound machine, scene, drawable size, quality and uncontaminated sampling window.",
            "warmup_frames": warmup_frames, "retained_frames": len(frames),
            "sample_count": len(samples["swap"]),
            "measurement_scope": "cpu_wall_time_through_swap_completion_not_scanout",
            "render_configurations": [dict(drawable_width_px=c[0], drawable_height_px=c[1],
                render_quality_enum=c[2], requested_msaa_samples=c[3], sample_count=count)
                for c, count in configurations.items()],
            "series": {name: percentiles(values) for name, values in samples.items()}}


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("session", type=Path)
    parser.add_argument("--warmup-frames", type=int, default=0)
    args = parser.parse_args()
    records, bindings = [], []
    try:
        if not args.session.is_dir():
            raise ValueError("session must be an existing directory")
        for path in sorted(args.session.glob("events-*.jsonl")):
            data = path.read_bytes()
            bindings.append({"name": path.name, "sha256": hashlib.sha256(data).hexdigest()})
            records.extend(json.loads(line) for line in data.splitlines() if line.strip())
        report = summarize(records, args.warmup_frames)
    except (OSError, ValueError, KeyError, TypeError, AttributeError) as error:
        parser.exit(2, f"timing summary rejected: {error}\n")
    report["source_session"] = str(args.session.resolve())
    report["source_files"] = bindings
    print(json.dumps(report, indent=2, allow_nan=False))


if __name__ == "__main__":
    main()
