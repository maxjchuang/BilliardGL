import csv
import io
import json
import math
from pathlib import Path


DEFAULT_FIXED = {
    "ball_mass_kg": 0.17,
    "cue_mass_kg": 0.5,
    "cue_speed_m_s": 3.0,
    "normal_exponent": 1.5,
    "tangential_stiffness_n_per_m": 400000.0,
    "tangential_damping_n_s_per_m": 25.0,
    "chalked_friction_coefficient": 0.62,
    "simulation_step_s": 0.0000005,
    "target_duration_s": 0.001,
    "timing_uncertainty_s": 0.000025,
}

DEFAULT_BOUNDS = {
    "stiffness_min_n_per_m32": 1.0e5,
    "stiffness_max_n_per_m32": 1.0e9,
    "stiffness_grid_points": 161,
    "dissipation_s_per_m": (0.0, 0.025, 0.05, 0.075, 0.1),
    "refinement_iterations": 80,
}


def hunt_crossley_force(compression_m, compression_rate_m_s,
                        stiffness_n_per_m32, dissipation_s_per_m):
    if compression_m <= 0.0:
        return 0.0
    return max(0.0, stiffness_n_per_m32 * compression_m ** 1.5
               * (1.0 + dissipation_s_per_m * compression_rate_m_s))


def _simulate(stiffness, dissipation, fixed, sample_times):
    reduced_mass = 1.0 / (1.0 / fixed["cue_mass_kg"]
                          + 1.0 / fixed["ball_mass_kg"])
    dt = fixed["simulation_step_s"]
    compression = 0.0
    rate = fixed["cue_speed_m_s"]
    time_s = 0.0
    history = [(time_s, compression)]
    maximum_energy = 0.5 * reduced_mass * rate * rate
    passive = True
    released = False
    duration = math.nan
    for _ in range(100000):
        previous_time = time_s
        previous_compression = compression
        force = hunt_crossley_force(
            compression, rate, stiffness, dissipation)
        half_rate = rate - 0.5 * force / reduced_mass * dt
        compression += half_rate * dt
        next_force = hunt_crossley_force(
            max(compression, 0.0), half_rate, stiffness, dissipation)
        rate = half_rate - 0.5 * next_force / reduced_mass * dt
        time_s += dt
        kinetic = 0.5 * reduced_mass * rate * rate
        elastic = stiffness * max(compression, 0.0) ** 2.5 / 2.5
        passive = passive and math.isfinite(kinetic + elastic)
        if dissipation >= 0.0:
            passive = passive and kinetic + elastic <= maximum_energy + 5e-6
        history.append((time_s, max(0.0, compression)))
        if previous_compression > 0.0 and compression <= 0.0 and rate < 0.0:
            fraction = previous_compression / (previous_compression - compression)
            duration = previous_time + fraction * dt
            history[-1] = (duration, 0.0)
            released = True
            break
    if not released:
        return {"duration_s": math.nan, "trace": [], "passive": False}

    trace = []
    cursor = 0
    for target in sample_times:
        while cursor + 1 < len(history) and history[cursor + 1][0] < target:
            cursor += 1
        if target >= duration or cursor + 1 >= len(history):
            trace.append(0.0)
            continue
        first_t, first_x = history[cursor]
        second_t, second_x = history[cursor + 1]
        weight = (target - first_t) / (second_t - first_t)
        trace.append(first_x + (second_x - first_x) * weight)
    peak = max(trace) if trace else 0.0
    normalized = [value / peak if peak > 0.0 else 0.0 for value in trace]
    return {"duration_s": duration, "trace": normalized, "passive": passive}


def _evaluate(stiffness, dissipation, points, fixed, stage):
    times = [point["time_s"] for point in points]
    observed = [point["normalized_strain"] for point in points]
    simulated = _simulate(stiffness, dissipation, fixed, times)
    if not math.isfinite(simulated["duration_s"]):
        return {"objective": math.inf, "passive": False}
    duration_residual = simulated["duration_s"] - fixed["target_duration_s"]
    trace_rmse = math.sqrt(sum(
        (actual - expected) ** 2
        for actual, expected in zip(simulated["trace"], observed)) / len(points))
    objective = abs(duration_residual) / fixed["timing_uncertainty_s"] + trace_rmse
    return {
        "stage": stage,
        "stiffness_n_per_m32": stiffness,
        "dissipation_s_per_m": dissipation,
        "duration_s": simulated["duration_s"],
        "duration_residual_s": duration_residual,
        "trace_rmse": trace_rmse,
        "objective": objective,
        "passive": simulated["passive"],
    }


def fit_frozen_contact(points, fixed=None, bounds=None):
    fixed = dict(DEFAULT_FIXED if fixed is None else fixed)
    bounds = dict(DEFAULT_BOUNDS if bounds is None else bounds)
    if fixed["normal_exponent"] != 1.5:
        raise ValueError("the Hunt-Crossley exponent is fixed at 1.5")
    count = bounds["stiffness_grid_points"]
    lower_log = math.log10(bounds["stiffness_min_n_per_m32"])
    upper_log = math.log10(bounds["stiffness_max_n_per_m32"])
    stiffness_grid = [10.0 ** (lower_log + (upper_log - lower_log)
                      * index / (count - 1)) for index in range(count)]
    residuals = []
    refined = []
    for dissipation in bounds["dissipation_s_per_m"]:
        grid_rows = [_evaluate(stiffness, dissipation, points, fixed, "grid")
                     for stiffness in stiffness_grid]
        residuals.extend(grid_rows)
        left = bounds["stiffness_min_n_per_m32"]
        right = bounds["stiffness_max_n_per_m32"]
        for _ in range(bounds["refinement_iterations"]):
            middle = math.sqrt(left * right)
            observation = _evaluate(
                middle, dissipation, points, fixed, "refinement_probe")
            if observation["duration_s"] > fixed["target_duration_s"]:
                left = middle
            else:
                right = middle
        row = _evaluate(math.sqrt(left * right), dissipation,
                        points, fixed, "refined")
        refined.append(row)
        residuals.append(row)
    winner = min(refined, key=lambda row: (
        row["objective"], row["stiffness_n_per_m32"],
        row["dissipation_s_per_m"]))

    sensitivity = []
    for parameter in ("stiffness_n_per_m32", "dissipation_s_per_m"):
        for scale in (0.5, 1.0, 2.0):
            stiffness = winner["stiffness_n_per_m32"]
            dissipation = winner["dissipation_s_per_m"]
            if parameter == "stiffness_n_per_m32":
                stiffness *= scale
            else:
                dissipation *= scale
            row = _evaluate(stiffness, dissipation, points, fixed, "sensitivity")
            row["parameter"] = parameter
            row["scale"] = scale
            sensitivity.append(row)
    return {
        "fixed": fixed,
        "bounds": bounds,
        "winner": winner,
        "residuals": residuals,
        "sensitivity": sensitivity,
    }


def _csv_bytes(header, rows):
    stream = io.StringIO(newline="")
    writer = csv.writer(stream, lineterminator="\n")
    writer.writerow(header)
    writer.writerows(rows)
    return stream.getvalue().encode("utf-8")


def _json_bytes(value):
    return (json.dumps(value, indent=2, sort_keys=True) + "\n").encode("utf-8")


def load_points(package):
    with (package / "normalized.csv").open(encoding="utf-8", newline="") as source:
        return [{"time_s": float(row["time_s"]),
                 "normalized_strain": float(row["expected"]),
                 "point_id": row["point_id"]}
                for row in csv.DictReader(source)]


def build_fit_artifacts(package):
    points = load_points(package)
    fit = fit_frozen_contact(points)
    inputs = _csv_bytes(
        ("point_id", "time_s", "normalized_strain", "partition"),
        ((point["point_id"], f'{point["time_s"]:.6f}',
          f'{point["normalized_strain"]:.9f}', "calibration")
         for point in points),
    )
    residual_header = (
        "stage", "stiffness_n_per_m32", "dissipation_s_per_m",
        "duration_s", "duration_residual_s", "trace_rmse", "objective",
        "passive")
    residual_rows = ((row.get(key, "") for key in residual_header)
                     for row in fit["residuals"])
    sensitivity_header = (
        "parameter", "scale", "stiffness_n_per_m32", "dissipation_s_per_m",
        "duration_s", "duration_residual_s", "trace_rmse", "objective",
        "passive")
    sensitivity_rows = ((row.get(key, "") for key in sensitivity_header)
                        for row in fit["sensitivity"])
    fit_report = {
        "bounds": fit["bounds"],
        "cpp_formula_cross_check": {
            "compression_m": 0.001,
            "compression_rate_m_s": 0.2,
            "force_n": hunt_crossley_force(0.001, 0.2, 1.25e7, 0.05),
            "formula": "max(0,k*delta^1.5*(1+alpha*delta_dot))",
        },
        "dataset_id": "shimamura_2006_cue_contact",
        "dataset_version": "1.0.0",
        "fixed": fit["fixed"],
        "fit_version": "frozen_cue_contact_v1",
        "identifiability": {
            "normal_dissipation": (
                "sensitivity-only; selected grid boundary does not establish an interior estimate"
            ),
            "normal_stiffness": "identified from registered contact duration and trace shape",
        },
        "winner": fit["winner"],
    }
    return {
        "frozen_cue_contact_v1_inputs.csv": inputs,
        "frozen_cue_contact_v1_fit.json": _json_bytes(fit_report),
        "frozen_cue_contact_v1_residuals.csv":
            _csv_bytes(residual_header, residual_rows),
        "frozen_cue_contact_v1_sensitivity.csv":
            _csv_bytes(sensitivity_header, sensitivity_rows),
    }


def main():
    import argparse
    parser = argparse.ArgumentParser()
    parser.add_argument("--package", type=Path, required=True)
    parser.add_argument("--output", type=Path, required=True)
    parser.add_argument("--check", action="store_true")
    args = parser.parse_args()
    generated = build_fit_artifacts(args.package)
    if args.check:
        for name, data in generated.items():
            if (args.output / name).read_bytes() != data:
                raise SystemExit(f"{name} is not byte-identical")
    else:
        args.output.mkdir(parents=True, exist_ok=True)
        for name, data in generated.items():
            (args.output / name).write_bytes(data)
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
