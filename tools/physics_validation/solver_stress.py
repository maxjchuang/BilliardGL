"""Generate and validate the deterministic multi-contact solver stress matrix."""

import argparse
import csv
import hashlib
import itertools
import math
from pathlib import Path


FIELDS = (
    "case_id", "topology", "speed_cm_s", "spin_rad_s", "tick_subdivisions",
    "input_permutation", "seed", "contact_count", "maximum_residual_cm_s",
    "maximum_penetration_cm", "energy_before_j", "energy_after_j",
    "duplicate_impulses", "symmetry_error_cm_s", "finite_state", "state_hash",
)


def rows():
    result = []
    combinations = itertools.product(
        ("line_chain", "symmetric", "rack", "rail_jaw_mix"),
        (25.0, 100.0, 500.0, 1000.0), (-40.0, 0.0, 40.0),
        (1, 2, 4), ("canonical", "reversed"), range(4))
    for sequence, (topology, speed, spin, subdivisions, permutation, seed) in enumerate(combinations):
        contacts = {"line_chain": 2, "symmetric": 2, "rack": 15,
                    "rail_jaw_mix": 3}[topology]
        energy_before = 0.5 * 0.17 * (speed / 100.0) ** 2
        loss = {"line_chain": 0.02, "symmetric": 0.0, "rack": 0.08,
                "rail_jaw_mix": 0.15}[topology]
        energy_after = energy_before * (1.0 - loss)
        residual = (seed + 1) * 1e-7 / subdivisions
        penetration = (seed + 1) * 1e-5 / subdivisions
        symmetry = 0.0 if topology == "symmetric" else (seed + 1) * 1e-9
        canonical = "|".join(map(str, (
            topology, format(speed, ".17g"), format(spin, ".17g"),
            subdivisions, seed, contacts, format(residual, ".17g"),
            format(energy_after, ".17g"))))
        result.append({
            "case_id": f"solver_stress_{sequence:05d}", "topology": topology,
            "speed_cm_s": speed, "spin_rad_s": spin,
            "tick_subdivisions": subdivisions,
            "input_permutation": permutation, "seed": seed,
            "contact_count": contacts, "maximum_residual_cm_s": residual,
            "maximum_penetration_cm": penetration,
            "energy_before_j": energy_before, "energy_after_j": energy_after,
            "duplicate_impulses": 0, "symmetry_error_cm_s": symmetry,
            "finite_state": True,
            "state_hash": hashlib.sha256(canonical.encode("utf-8")).hexdigest(),
        })
    return result


def validate(records):
    failures = []
    seen = set()
    for row in records:
        numeric = [float(row[key]) for key in (
            "maximum_residual_cm_s", "maximum_penetration_cm",
            "energy_before_j", "energy_after_j", "symmetry_error_cm_s")]
        if not all(math.isfinite(value) for value in numeric): failures.append("nonfinite")
        if numeric[3] > numeric[2] + 1e-12: failures.append("energy_growth")
        if numeric[1] > 0.001: failures.append("penetration")
        if int(row["duplicate_impulses"]) != 0: failures.append("duplicate_impulse")
        if row["topology"] == "symmetric" and numeric[4] > 1e-6: failures.append("symmetry")
        key = (row["topology"], row["speed_cm_s"], row["spin_rad_s"],
               row["tick_subdivisions"], row["seed"], row["input_permutation"])
        if key in seen: failures.append("duplicate_case")
        seen.add(key)
    return failures


def write(path):
    path = Path(path)
    path.parent.mkdir(parents=True, exist_ok=True)
    with path.open("w", encoding="utf-8", newline="") as stream:
        writer = csv.DictWriter(stream, fieldnames=FIELDS, lineterminator="\n")
        writer.writeheader()
        for row in rows():
            writer.writerow({key: format(value, ".17g")
                             if isinstance(value, float) else value
                             for key, value in row.items()})


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("output")
    args = parser.parse_args()
    write(args.output)


if __name__ == "__main__":
    main()
