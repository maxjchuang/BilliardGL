"""Generate the deterministic full-precision pocket-boundary scan matrix."""

import argparse
import csv
import itertools
import math
from pathlib import Path


GEOMETRY = {
    "ball_radius_cm": 2.8575,
    "corner_mouth_width_cm": 13.2,
    "corner_throat_width_cm": 11.0,
    "side_mouth_width_cm": 8.6,
    "side_throat_width_cm": 7.0,
    "jaw_radius_cm": 1.2,
    "throat_depth_cm": 4.0,
    "capture_depth_cm": 6.0,
}

OFFSETS = (-1.2, -0.8, -0.4, 0.0, 0.4, 0.8, 1.2)
ANGLES_DEG = (-30.0, -15.0, 0.0, 15.0, 30.0)
SPEEDS_CM_S = (25.0, 100.0, 250.0, 500.0)
SPINS_RAD_S = ((0.0, 0.0), (20.0, 0.0), (-20.0, 0.0), (0.0, 20.0), (0.0, -20.0))
JAW_SIDES = (-1, 1)

FIELDS = (
    "case_id", "pocket_kind", "normalized_offset", "offset_cm",
    "angle_deg", "speed_cm_s", "topspin_rad_s", "sidespin_rad_s",
    "jaw_side", "mouth_width_cm", "throat_width_cm", "jaw_radius_cm",
    "throat_depth_cm", "capture_depth_cm", "ball_radius_cm",
    "mouth_passable", "throat_passable", "expected_outcome", "mirror_key",
)


def rows():
    result = []
    sequence = 0
    for kind, offset, angle, speed, spin, jaw_side in itertools.product(
            ("corner", "side"), OFFSETS, ANGLES_DEG, SPEEDS_CM_S,
            SPINS_RAD_S, JAW_SIDES):
        mouth = GEOMETRY[f"{kind}_mouth_width_cm"]
        throat = GEOMETRY[f"{kind}_throat_width_cm"]
        available_mouth = mouth - 2.0 * GEOMETRY["ball_radius_cm"]
        available_throat = throat - 2.0 * GEOMETRY["ball_radius_cm"]
        offset_cm = offset * available_mouth * 0.5
        mouth_passable = available_mouth >= 0.0 and abs(offset_cm) <= available_mouth * 0.5
        throat_passable = available_throat >= 0.0 and abs(offset_cm) <= available_throat * 0.5
        expected = "capture" if mouth_passable and throat_passable else (
            "jaw_or_rail_rejection" if not throat_passable else "solid")
        mirror_key = (
            f"{kind}|{abs(offset):.17g}|{abs(angle):.17g}|{speed:.17g}|"
            f"{spin[0]:.17g}|{abs(spin[1]):.17g}|{abs(jaw_side)}")
        result.append({
            "case_id": f"pocket_scan_{sequence:05d}",
            "pocket_kind": kind,
            "normalized_offset": offset,
            "offset_cm": offset_cm,
            "angle_deg": angle,
            "speed_cm_s": speed,
            "topspin_rad_s": spin[0],
            "sidespin_rad_s": spin[1],
            "jaw_side": jaw_side,
            "mouth_width_cm": mouth,
            "throat_width_cm": throat,
            "jaw_radius_cm": GEOMETRY["jaw_radius_cm"],
            "throat_depth_cm": GEOMETRY["throat_depth_cm"],
            "capture_depth_cm": GEOMETRY["capture_depth_cm"],
            "ball_radius_cm": GEOMETRY["ball_radius_cm"],
            "mouth_passable": mouth_passable,
            "throat_passable": throat_passable,
            "expected_outcome": expected,
            "mirror_key": mirror_key,
        })
        sequence += 1
    return result


def write(path):
    path = Path(path)
    path.parent.mkdir(parents=True, exist_ok=True)
    with path.open("w", encoding="utf-8", newline="") as stream:
        writer = csv.DictWriter(stream, fieldnames=FIELDS, lineterminator="\n")
        writer.writeheader()
        for row in rows():
            writer.writerow({key: format(value, ".17g")
                             if isinstance(value, float) and math.isfinite(value)
                             else value for key, value in row.items()})


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("output")
    args = parser.parse_args()
    write(args.output)


if __name__ == "__main__":
    main()
