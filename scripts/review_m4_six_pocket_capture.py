#!/usr/bin/env python3
"""Summarize real GPU captures for TA review; never auto-approve art.

Annotates shipping-table static mouth anchors, not dynamic collision contacts.
All readability statistics are in display RGB and are descriptive only.
"""
import argparse
import hashlib
import json
from pathlib import Path

from PIL import Image, ImageDraw


def review(directory):
    binding = json.loads((directory / "capture_binding.json").read_text())
    for filename, key in (("pocket_closeup.ppm", "beauty_ppm_sha256"),
                          ("pocket_object_ids.ppm", "object_id_ppm_sha256")):
        if hashlib.sha256((directory / filename).read_bytes()).hexdigest() != binding[key]:
            raise ValueError(f"capture binding mismatch: {directory / filename}")
    image = Image.open(directory / "pocket_closeup.ppm").convert("RGB")
    ids = Image.open(directory / "pocket_object_ids.ppm").convert("RGB")
    before = Image.open(directory / "pocket_indirect_off.ppm").convert("RGB")
    if image.size != (1920, 1080) or ids.size != image.size or before.size != image.size:
        raise ValueError("capture dimensions differ from frozen 1080p")
    overlay = image.copy()
    drawing = ImageDraw.Draw(overlay)
    selected = binding["review_pocket_id"]
    selected = 3 if selected < 0 else selected
    mouths = binding["physical_mouth_anchors"]
    chosen = mouths[selected]
    if not all(lip["projectable"] for lip in chosen["lips"]):
        raise ValueError(f"selected mouth {selected} is not projectable")
    points = [tuple(lip["screen_pixels"]) for lip in chosen["lips"]]
    if not all(48 <= x <= image.width - 48 and 48 <= y <= image.height - 48 for x, y in points):
        raise ValueError(f"selected mouth {selected} is clipped or lacks frame margin")
    drawing.line(points, fill=(255, 225, 70), width=2)
    for x, y in points:
        drawing.ellipse((x - 4, y - 4, x + 4, y + 4), outline=(255, 225, 70), width=2)
    drawing.text((points[0][0] + 8, points[0][1] + 8),
                 f"P{selected}: static physical mouth {chosen['mouth_width_cm']:.1f} cm", fill=(255, 225, 70))
    drawing.text((20, 20), "Static mouth anchors only; not dynamic collision proof", fill=(255, 225, 70))
    overlay.save(directory / "physical_mouth_overlay.png")
    image.save(directory / "pocket_closeup.png")
    ids.save(directory / "pocket_object_ids.png")
    pixel_image, pixel_ids, pixel_before = image.load(), ids.load(), before.load()
    # Bound the selected pocket to the physical projected mouth, avoiding other
    # visible pockets. The cavity extends outward from the mouth in the view.
    x0 = max(0, int(min(p[0] for p in points)) - 150)
    x1 = min(image.width, int(max(p[0] for p in points)) + 150)
    y0 = max(0, int(min(p[1] for p in points)) - 100)
    y1 = min(image.height, int(max(p[1] for p in points)) + 350)
    image.crop((x0, y0, x1, y1)).save(directory / "pocket_review_crop.png")
    before.crop((x0, y0, x1, y1)).save(directory / "pocket_indirect_off_crop.png")
    records = {name: {"samples": [], "changed_pixels": 0}
               for name in ("leather", "inner", "deep")}
    for y in range(y0, y1):
        for x in range(x0, x1):
            r, g, b = pixel_ids[x, y]
            name = "leather" if r > 220 and g < 3 and b < 3 else (
                "inner" if g > 220 and r < 3 and b < 3 else (
                    "deep" if b > 220 and r < 3 and g < 3 else None))
            if name is None:
                continue
            rgb = pixel_image[x, y]
            records[name]["samples"].append(sum(a * w for a, w in zip(rgb, (.2126, .7152, .0722))))
            records[name]["changed_pixels"] += rgb != pixel_before[x, y]
    result = {"directory": str(directory), "pocket_id": selected,
              "camera": binding["camera"], "crop_pixels": [x0, y0, x1, y1],
              "scope": "descriptive_real_capture_static_alignment",
              "formal_pass_claimed": False, "visual_review_status": "PENDING_TA"}
    for name, record in records.items():
        samples = sorted(record.pop("samples"))
        result[name] = {**record, "owned_pixels": len(samples),
                        "display_luma_p10": samples[len(samples) // 10] if samples else None,
                        "display_luma_p50": samples[len(samples) // 2] if samples else None,
                        "display_luma_p90": samples[len(samples) * 9 // 10] if samples else None}
    descent_path = directory / "descent_capture_binding.json"
    if descent_path.exists():
        descent = json.loads(descent_path.read_text())
        frames = descent["rendered_checkpoint_receipts"]
        if len(frames) != 5 or [frame["visible"] for frame in frames] != [True] * 4 + [False]:
            raise ValueError("real descent checkpoints must preserve four visible and one captured state")
        sheet = Image.new("RGB", ((x1 - x0) * 5, y1 - y0 + 48), (25, 25, 25))
        labels = ImageDraw.Draw(sheet)
        for index, frame in enumerate(frames):
            path = directory / frame["file"]
            if hashlib.sha256(path.read_bytes()).hexdigest() != frame["beauty_ppm_sha256"]:
                raise ValueError("descent frame receipt hash mismatch")
            crop = Image.open(path).convert("RGB").crop((x0, y0, x1, y1))
            crop.save(directory / f"descent_frame_{index}_crop.png")
            sheet.paste(crop, ((x1 - x0) * index, 48))
            labels.text(((x1 - x0) * index + 8, 8),
                        f"tick {frame['tick']} / {frame['time_seconds'] * 1000:.1f} ms", fill=(235, 235, 235))
            labels.text(((x1 - x0) * index + 8, 25),
                        frame["label"], fill=(235, 235, 235))
        sheet.save(directory / "real_descent_sequence.png")
        result["real_descent"] = {"trace_sha256": descent["trace_sha256"],
                                   "scope": descent["trajectory_scope"],
                                   "checkpoint_count": 5,
                                   "manual_collision_review": "PENDING_TA"}
    (directory / "readability_measurements.json").write_text(json.dumps(result, indent=2) + "\n")
    return result


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("directories", nargs="+", type=Path)
    args = parser.parse_args()
    print(json.dumps([review(directory) for directory in args.directories], indent=2))


if __name__ == "__main__":
    main()
