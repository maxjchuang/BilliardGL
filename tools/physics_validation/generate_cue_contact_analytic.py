import argparse
import csv
import hashlib
import json
import shutil
import tempfile
from decimal import Decimal, getcontext
from pathlib import Path


getcontext().prec = 60

DATASET_ID = "cue_contact_analytic_contract"
DATASET_VERSION = "1.0.0"
D = Decimal
BALL_MASS = D("0.17")
BALL_RADIUS_M = D("0.028575")
CUE_MASS = D("0.5")
CUE_SPEED = D("1.0")
RESTITUTION = D("0.0")
CHALKED_MU = D("0.6")
INERTIA = D("0.4") * BALL_MASS * BALL_RADIUS_M * BALL_RADIUS_M
NORMALIZED_HEADER = (
    "dataset_id", "series_id", "group_id", "case_id", "point_id",
    "partition", "metric", "expected", "unit", "measurement_uncertainty",
    "digitization_uncertainty", "conversion_uncertainty", "coverage_factor",
    "engineering_absolute_tolerance", "engineering_relative_tolerance",
    "source_locator", "pool_applicability",
)


def _sqrt(value):
    return value.sqrt()


def _dot(first, second):
    return sum((a * b for a, b in zip(first, second)), D(0))


def _cross(first, second):
    return (
        first[1] * second[2] - first[2] * second[1],
        first[2] * second[0] - first[0] * second[2],
        first[0] * second[1] - first[1] * second[0],
    )


def _add(first, second):
    return tuple(a + b for a, b in zip(first, second))


def _scale(value, scale):
    return tuple(component * scale for component in value)


def solve(side, vertical, mu=CHALKED_MU):
    side, vertical = D(side), D(vertical)
    fraction2 = side * side + vertical * vertical
    root = _sqrt(D(1) - fraction2)
    arm = (-root * BALL_RADIUS_M, vertical * BALL_RADIUS_M,
           side * BALL_RADIUS_M)
    normal = (root, -vertical, -side)
    direction = (D(1), D(0), D(0))
    arm_cross_direction = _cross(arm, direction)
    inverse_mass = D(1) / CUE_MASS + D(1) / BALL_MASS + \
        _dot(arm_cross_direction, arm_cross_direction) / INERTIA
    desired_magnitude = (D(1) + RESTITUTION) * CUE_SPEED / inverse_mass
    desired = _scale(direction, desired_magnitude)
    normal_impulse = _dot(desired, normal)
    tangent = _add(desired, _scale(normal, -normal_impulse))
    tangent_magnitude = _sqrt(_dot(tangent, tangent))
    if tangent_magnitude <= mu * normal_impulse:
        regime = "stick"
        impulse = desired
    else:
        regime = "slip"
        impulse = _add(_scale(normal, normal_impulse),
                       _scale(tangent, mu * normal_impulse / tangent_magnitude))
    ball_velocity = _scale(impulse, D(1) / BALL_MASS)
    angular_velocity = _scale(_cross(arm, impulse), D(1) / INERTIA)
    cue_after = _add(_scale(direction, CUE_SPEED),
                     _scale(impulse, -D(1) / CUE_MASS))
    input_energy = D("0.5") * CUE_MASS * CUE_SPEED * CUE_SPEED
    output_energy = D("0.5") * BALL_MASS * _dot(ball_velocity, ball_velocity) + \
        D("0.5") * INERTIA * _dot(angular_velocity, angular_velocity) + \
        D("0.5") * CUE_MASS * _dot(cue_after, cue_after)
    return {
        "regime": regime,
        "normal_impulse": normal_impulse,
        "tangential_impulse": (tangent_magnitude if regime == "stick"
                               else mu * normal_impulse),
        "energy_efficiency": output_energy / input_energy,
        "linear_speed_cm_s": _sqrt(_dot(ball_velocity, ball_velocity)) * D(100),
        "angular_velocity": angular_velocity,
    }


def _decimal(value):
    text = format(value, "f")
    return text.rstrip("0").rstrip(".") if "." in text else text


BOUNDARY_INNER = CHALKED_MU / _sqrt(D(1) + CHALKED_MU * CHALKED_MU) - D("0.00000001")
CASES = (
    ("center_hit", "CALIBRATION", D(0), D(0)),
    ("positive_vertical_stick", "CALIBRATION", D(0), D("0.2")),
    ("negative_vertical_stick", "CALIBRATION", D(0), D("-0.2")),
    ("stick_boundary_inner", "CALIBRATION", BOUNDARY_INNER, D(0)),
    ("left_mirror", "HOLDOUT", D("-0.2"), D(0)),
    ("right_mirror", "HOLDOUT", D("0.2"), D(0)),
    ("horizontal_slip", "HOLDOUT", D("0.7"), D(0)),
)


def _point_rows():
    rows = []
    raw = []
    for case_id, partition, side, vertical in CASES:
        result = solve(side, vertical)
        values = [
            ("normal_impulse", "cue_contact_normal_impulse_ns",
             result["normal_impulse"], "N*s", "0.0000001", "0.00001"),
            ("tangential_impulse", "cue_contact_tangential_impulse_ns",
             result["tangential_impulse"], "N*s", "0.0000001", "0.00001"),
            ("energy_efficiency", "cue_contact_energy_efficiency",
             result["energy_efficiency"], "dimensionless", "0.000001", "0.00001"),
            ("linear_speed", "cue_impact_linear_speed_cm_s",
             result["linear_speed_cm_s"], "cm/s", "0.0001", "0.00001"),
        ]
        axis = "z" if vertical else "y"
        angular = result["angular_velocity"][2 if axis == "z" else 1]
        if side or vertical:
            values.append(("angular_speed", "cue_impact_angular_speed_rad_s",
                           angular, "rad/s", "0.0001", "0.00001"))
        for suffix, metric, expected, unit, absolute, relative in values:
            point_id = f"{case_id}_{suffix}"
            locator = f"analytic:rigid-impulse:{case_id}:{suffix}"
            row = {
                "dataset_id": DATASET_ID,
                "series_id": "rigid_impulse_v1",
                "group_id": case_id,
                "case_id": case_id,
                "point_id": point_id,
                "partition": partition,
                "metric": metric,
                "expected": _decimal(expected),
                "unit": unit,
                "measurement_uncertainty": "0",
                "digitization_uncertainty": "0",
                "conversion_uncertainty": "0",
                "coverage_factor": "2",
                "engineering_absolute_tolerance": absolute,
                "engineering_relative_tolerance": relative,
                "source_locator": locator,
                "pool_applicability": "NOT_APPLICABLE",
            }
            rows.append(row)
            raw.append({
                "case_id": case_id, "partition": partition,
                "side_offset_radius": _decimal(side),
                "vertical_offset_radius": _decimal(vertical),
                "regime": result["regime"], "quantity": metric,
                "exact_decimal_value": _decimal(expected), "unit": unit,
                "source_locator": locator,
            })
    rows.append({
        "dataset_id": DATASET_ID, "series_id": "rigid_impulse_v1",
        "group_id": "miscue", "case_id": "miscue",
        "point_id": "miscue_classification", "partition": "HOLDOUT",
        "metric": "cue_contact_energy_efficiency", "expected": "1",
        "unit": "dimensionless", "measurement_uncertainty": "0",
        "digitization_uncertainty": "0", "conversion_uncertainty": "0",
        "coverage_factor": "2", "engineering_absolute_tolerance": "0",
        "engineering_relative_tolerance": "0",
        "source_locator": "analytic:rigid-impulse:miscue:classification",
        "pool_applicability": "NOT_APPLICABLE",
    })
    raw.append({
        "case_id": "miscue", "partition": "HOLDOUT",
        "side_offset_radius": "0.81", "vertical_offset_radius": "0",
        "regime": "miscue", "quantity": "cue_contact_energy_efficiency",
        "exact_decimal_value": "1", "unit": "dimensionless",
        "source_locator": "analytic:rigid-impulse:miscue:classification",
    })
    return raw, rows


def _json(path, document):
    path.write_text(json.dumps(document, ensure_ascii=False, indent=2,
                               sort_keys=True, allow_nan=False) + "\n", encoding="utf-8")


def _sha(path):
    return "sha256:" + hashlib.sha256(path.read_bytes()).hexdigest()


def _base_scenario():
    return {
        "schema_version": 4,
        "id": "cue_contact_analytic",
        "description": "Independent grade-C analytic rigid-impulse contract; not experimental validation",
        "evidence": {"grade": "C", "source": "independent Decimal rigid-impulse equations; equipment applicability remains NOT_APPLICABLE in package metadata",
                     "equipment": "WPA_POOL"},
        "simulation": {"ticks": 2, "time_step_seconds": 0.1},
        "physics_profile": {
            "id": "cue_contact_analytic_v1", "formula_version": "cue_contact_v1",
            "ball": {"mass_kg": 0.17, "radius_cm": 2.8575,
                     "material": "analytic_phenolic"},
            "surface": {"legacy_friction_acceleration_cm_s2": 0.0,
                        "sliding_friction_coefficient": 0.0,
                        "rolling_resistance_acceleration_cm_s2": 0.0,
                        "torsional_spin_deceleration_rad_s2": 0.0,
                        "slip_speed_epsilon_cm_s": 0.0001,
                        "stop_energy_threshold_j": 0.000000001,
                        "material": "analytic_frictionless_surface"},
            "cue": {"effective_mass_kg": 0.5, "normal_restitution": 0.0,
                    "chalked_friction_coefficient": 0.6,
                    "unchalked_friction_coefficient": 0.1,
                    "maximum_reliable_offset_radius": 0.8,
                    "cue_speed_per_power_unit_cm_s": 1.34},
            "cushion": {"normal_restitution": 1.0, "friction_coefficient": 0.0},
            "solver": {"time_step_seconds": 0.1, "maximum_events_per_tick": 64},
        },
        "balls": [{"index": 0, "position_cm": [0.0, 89.341476, 20.0],
                   "velocity_cm_s": [0.0, 0.0, 0.0],
                   "angular_velocity_rad_s": [0.0, 0.0, 0.0], "pocketed": False}],
        "expectations": [],
    }


def generate(root):
    root.mkdir(parents=True, exist_ok=True)
    raw, rows = _point_rows()
    raw_path = root / "raw_extracted.csv"
    with raw_path.open("w", encoding="utf-8", newline="") as output:
        writer = csv.DictWriter(output, fieldnames=tuple(raw[0]), lineterminator="\n")
        writer.writeheader(); writer.writerows(raw)
    normalized_path = root / "normalized.csv"
    with normalized_path.open("w", encoding="utf-8", newline="") as output:
        writer = csv.DictWriter(output, fieldnames=NORMALIZED_HEADER, lineterminator="\n")
        writer.writeheader(); writer.writerows(rows)

    calibration = [case_id for case_id, partition, _, _ in CASES if partition == "CALIBRATION"]
    holdout = [case_id for case_id, partition, _, _ in CASES if partition == "HOLDOUT"] + ["miscue"]
    _json(root / "split.json", {"schema_version": 1, "dataset_id": DATASET_ID,
          "dataset_version": DATASET_VERSION, "calibration_groups": calibration,
          "holdout_groups": holdout})
    mappings = {}
    for case_id, _, side, vertical in CASES:
        result = solve(side, vertical)
        mappings[case_id] = {
            "expected_regime": result["regime"],
            "tip_offset_radius": [float(side), float(vertical)],
        }
    mappings["miscue"] = {"expected_regime": "miscue", "tip_offset_radius": [0.81, 0.0]}
    _json(root / "scenario_template.json", {"schema_version": 1,
          "base_scenario": _base_scenario(), "cases": mappings})
    _json(root / "expected_model_mismatches.json", {"schema_version": 1, "failures": []})
    _json(root / "expected_reference_limitations.json", {"schema_version": 1,
          "failures": []})
    extraction = {
        "schema_version": 2, "method": "independent Decimal evaluation of printed rigid-impulse equations",
        "tool": {"name": "cue contact analytic generator", "version": "1.0.0"},
        "date": "2026-07-14", "operator": "Codex",
        "review": {"reviewed_by": "machine-verifiable independent equation generator",
                   "date": "2026-07-14", "method": "byte reproduction plus production-free import audit"},
        "inputs": [{"file_id": "raw_extracted", "sha256": _sha(raw_path)}],
        "transformations": [{"id": "rigid_impulse_decimal",
            "formula": "r=-n*sqrt(R^2-|o|^2)+o; P=(1+e)*u/(1/mc+1/mb+|r cross n|^2/I); Coulomb cone clamp; dv=J/mb; dw=(r cross J)/I",
            "input_unit": "SI Decimal inputs", "output_unit": "SI analytic expectations"}],
        "rounding_policy": "Decimal precision 60; canonical decimal strings retain all generated digits; float conversion occurs only in scenario JSON.",
        "uncertainty_interpretation": "Grade-C deterministic contract tolerances cover binary float transport only; they are not experimental uncertainty.",
        "source_sha256": _sha(raw_path), "output_sha256": _sha(normalized_path),
        "script": {"module": "tools.physics_validation.generate_cue_contact_analytic",
                   "version": "1.0.0"},
    }
    _json(root / "extraction.json", extraction)
    file_entries = [(name.removesuffix(".json").removesuffix(".csv"), name) for name in (
        "raw_extracted.csv", "normalized.csv", "split.json", "extraction.json",
        "scenario_template.json", "expected_model_mismatches.json",
        "expected_reference_limitations.json")]
    manifest = {
        "schema_version": 1, "dataset_id": DATASET_ID, "dataset_version": DATASET_VERSION,
        "adapter_id": "cue_contact_analytic_v1",
        "source": {"kind": "analytic_contract", "title": "Rigid cue-ball impulse equations",
                   "locator": "repository generator; no publication or experiment claimed"},
        "acquisition": {"method": "independent derivation", "date": "2026-07-14",
                        "license_status": "repository-authored", "source_media_committed": False},
        "evidence": {"grade": "C", "experimental_validation": False,
                     "claim": "analytic invariants and reproducibility only"},
        "apparatus": {"pool_applicability": "NOT_APPLICABLE",
                      "reason": "equation contract is not measured equipment data"},
        "extraction_review": {"operator": "Codex", "reviewed_by": "deterministic --check",
                              "date": "2026-07-14", "method": "independent Decimal regeneration"},
        "files": [{"id": file_id, "path": name, "sha256": _sha(root / name)}
                  for file_id, name in file_entries],
    }
    _json(root / "manifest.json", manifest)


def check(root):
    with tempfile.TemporaryDirectory() as directory:
        generated = Path(directory) / "package"
        generate(generated)
        expected = sorted(path.name for path in generated.iterdir())
        actual = sorted(path.name for path in root.iterdir())
        if actual != expected:
            raise SystemExit(f"package file set differs: expected {expected}, received {actual}")
        for name in expected:
            if (root / name).read_bytes() != (generated / name).read_bytes():
                raise SystemExit(f"generated bytes differ: {name}")


def main(argv=None):
    parser = argparse.ArgumentParser()
    parser.add_argument("--package", type=Path, required=True)
    parser.add_argument("--check", action="store_true")
    args = parser.parse_args(argv)
    if args.check:
        check(args.package)
    else:
        if args.package.exists():
            shutil.rmtree(args.package)
        generate(args.package)
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
