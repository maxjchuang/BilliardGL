import argparse
import csv
import json
import math
from pathlib import Path


_MATERIAL_BY_SERIES = {
    "billiard_alpha1": "billiard",
    "billiard_delta2": "billiard",
    "brass_alpha1": "brass",
    "rubber_delta2": "rubber",
    "rubber_lambda2": "rubber",
    "steel_alpha1": "steel",
    "steel_beta1": "steel",
}
_SURFACE_SLIDING_FRICTION = 0.002
_GRAVITY_M_S2 = 9.80665


def _canonical(document):
    return json.dumps(
        document, ensure_ascii=False, indent=2, sort_keys=True,
        allow_nan=False) + "\n"


def read_impact_inputs(path):
    required = {
        "series_id", "group_id", "case_id", "point_id", "status",
        "material", "metric", "sample_phase", "impact_angle_degrees",
    }
    rows = {}
    with Path(path).open("r", encoding="utf-8", newline="") as source:
        reader = csv.DictReader(source)
        if not required <= set(reader.fieldnames or ()):
            raise ValueError("Domenech raw extraction lacks collision input columns")
        for line_number, row in enumerate(reader, start=2):
            point_id = row["point_id"]
            if point_id in rows:
                raise ValueError(f"duplicate Domenech collision input {point_id}")
            if row["status"] != "ADMITTED":
                raise ValueError(
                    f"line {line_number}: collision input {point_id} is not admitted")
            try:
                impact = float(row["impact_angle_degrees"])
            except ValueError as error:
                raise ValueError(
                    f"line {line_number}: impact angle must be numeric") from error
            if not math.isfinite(impact) or impact < 0.0 or impact >= 90.0:
                raise ValueError(
                    f"line {line_number}: impact angle is outside [0,90)")
            material = _MATERIAL_BY_SERIES.get(row["series_id"])
            if material is None or material != row["material"]:
                raise ValueError(
                    f"line {line_number}: source material mapping is inconsistent")
            rows[point_id] = {
                "case_id": row["case_id"],
                "group_id": row["group_id"],
                "impact_angle_degrees": impact,
                "material": material,
                "metric": row["metric"],
                "sample_phase": row["sample_phase"],
                "series_id": row["series_id"],
            }
    return dict(sorted(rows.items()))


def _add(first, second):
    return tuple(a + b for a, b in zip(first, second))


def _subtract(first, second):
    return tuple(a - b for a, b in zip(first, second))


def _multiply(vector, scale):
    return tuple(value * scale for value in vector)


def _dot(first, second):
    return sum(a * b for a, b in zip(first, second))


def _cross(first, second):
    return (
        first[1] * second[2] - first[2] * second[1],
        first[2] * second[0] - first[0] * second[2],
        first[0] * second[1] - first[1] * second[0],
    )


def _length(vector):
    return math.sqrt(_dot(vector, vector))


def _collision_state(impact_degrees, restitution, friction):
    speed = 0.8
    radius = 1.0
    mass = 1.0
    inertia = 0.4 * mass * radius * radius
    angle = math.radians(impact_degrees)
    normal = (math.cos(angle), 0.0, -math.sin(angle))
    first_arm = _multiply(normal, radius)
    second_arm = _multiply(normal, -radius)
    first_velocity = (speed, 0.0, 0.0)
    second_velocity = (0.0, 0.0, 0.0)
    first_angular = (0.0, 0.0, -speed / radius)
    second_angular = (0.0, 0.0, 0.0)
    first_contact = _add(first_velocity, _cross(first_angular, first_arm))
    second_contact = _add(second_velocity, _cross(second_angular, second_arm))
    relative = _subtract(second_contact, first_contact)
    normal_speed = _dot(relative, normal)
    normal_impulse = -(1.0 + restitution) * normal_speed / 2.0
    impulse = _multiply(normal, normal_impulse)
    tangent_velocity = _subtract(relative, _multiply(normal, normal_speed))
    tangent_speed = _length(tangent_velocity)
    if tangent_speed > 1e-15 and friction > 0.0:
        tangent = _multiply(tangent_velocity, 1.0 / tangent_speed)
        denominator = 2.0 + (
            _dot(_cross(first_arm, tangent), _cross(first_arm, tangent)) +
            _dot(_cross(second_arm, tangent), _cross(second_arm, tangent))) / inertia
        desired = -_dot(relative, tangent) / denominator
        limit = friction * normal_impulse
        applied = max(-limit, min(limit, desired))
        impulse = _add(impulse, _multiply(tangent, applied))
    first_velocity = _subtract(first_velocity, _multiply(impulse, 1.0 / mass))
    second_velocity = _add(second_velocity, _multiply(impulse, 1.0 / mass))
    first_angular = _add(
        first_angular,
        _multiply(_cross(first_arm, _multiply(impulse, -1.0)), 1.0 / inertia))
    second_angular = _add(
        second_angular,
        _multiply(_cross(second_arm, impulse), 1.0 / inertia))
    return ((first_velocity, first_angular), (second_velocity, second_angular))


def _at_pure_roll(state):
    velocity, angular = state
    slip = (velocity[0] + angular[2], 0.0,
            velocity[2] - angular[0])
    slip_speed = math.hypot(slip[0], slip[2])
    if slip_speed <= 1e-15:
        return state
    acceleration = (
        -_SURFACE_SLIDING_FRICTION * _GRAVITY_M_S2 * slip[0] / slip_speed,
        0.0,
        -_SURFACE_SLIDING_FRICTION * _GRAVITY_M_S2 * slip[2] / slip_speed,
    )
    transition = slip_speed / (
        3.5 * _SURFACE_SLIDING_FRICTION * _GRAVITY_M_S2)
    final_velocity = _add(velocity, _multiply(acceleration, transition))
    final_angular = (
        final_velocity[2], angular[1], -final_velocity[0])
    return final_velocity, final_angular


def _signed_angle(velocity, clockwise=False):
    value = math.degrees(math.atan2(velocity[2], velocity[0]))
    return -value if clockwise else value


def _prediction(point, impact, restitution, friction):
    first, second = _collision_state(
        impact["impact_angle_degrees"], restitution, friction)
    if impact["sample_phase"] == "first_pure_roll_after_event":
        first = _at_pure_roll(first)
        second = _at_pure_roll(second)
    first_velocity = first[0]
    second_velocity = second[0]
    if point.metric == "cue_scattering_angle_degrees":
        return _signed_angle(first_velocity)
    if point.metric == "object_scattering_angle_degrees":
        return _signed_angle(second_velocity, clockwise=True)
    if point.metric == "separation_angle_degrees":
        denominator = math.hypot(first_velocity[0], first_velocity[2]) * \
            math.hypot(second_velocity[0], second_velocity[2])
        if denominator <= 1e-15:
            return 0.0
        cosine = max(-1.0, min(1.0, (
            first_velocity[0] * second_velocity[0] +
            first_velocity[2] * second_velocity[2]) / denominator))
        return math.degrees(math.acos(cosine))
    raise ValueError(f"unsupported Domenech metric {point.metric}")


def _objective(points, impacts, restitution, friction):
    residuals = [
        _prediction(point, impacts[point.point_id], restitution, friction) -
        point.expected
        for point in points
    ]
    return sum(value * value for value in residuals) / len(residuals)


def _grid(start, stop, step):
    count = int(round((stop - start) / step))
    return tuple(round(start + index * step, 12) for index in range(count + 1))


def _fit_one(points, impacts):
    best = None
    for restitution in _grid(0.0, 1.0, 0.05):
        for friction in _grid(0.0, 1.0, 0.05):
            objective = _objective(points, impacts, restitution, friction)
            candidate = (objective, restitution, friction)
            if best is None or candidate < best:
                best = candidate
    _, coarse_restitution, coarse_friction = best
    restitution_values = _grid(
        max(0.0, coarse_restitution - 0.05),
        min(1.0, coarse_restitution + 0.05), 0.01)
    friction_values = _grid(
        max(0.0, coarse_friction - 0.05),
        min(1.0, coarse_friction + 0.05), 0.01)
    for restitution in restitution_values:
        for friction in friction_values:
            objective = _objective(points, impacts, restitution, friction)
            candidate = (objective, restitution, friction)
            if candidate < best:
                best = candidate
    objective, restitution, friction = best
    sensitivity = []
    for delta_restitution, delta_friction in (
            (-0.01, 0.0), (0.01, 0.0), (0.0, -0.01), (0.0, 0.01)):
        candidate_restitution = min(1.0, max(0.0, restitution + delta_restitution))
        candidate_friction = min(1.0, max(0.0, friction + delta_friction))
        sensitivity.append({
            "friction_coefficient": candidate_friction,
            "normal_restitution": candidate_restitution,
            "objective_mean_squared_degrees": _objective(
                points, impacts, candidate_restitution, candidate_friction),
        })
    by_series = {}
    for series_id in sorted({point.series_id for point in points}):
        series = tuple(point for point in points if point.series_id == series_id)
        by_series[series_id] = _objective(
            series, impacts, restitution, friction)
    return {
        "calibration_point_ids": [point.point_id for point in points],
        "friction_coefficient": friction,
        "normal_restitution": restitution,
        "objective_by_series_mean_squared_degrees": by_series,
        "objective_mean_squared_degrees": objective,
        "sensitivity": sensitivity,
    }


def fit_material_parameters(points, split, impacts):
    points = tuple(sorted(points, key=lambda point: point.point_id))
    if {point.point_id for point in points} != set(impacts):
        raise ValueError("Domenech normalized points and collision inputs differ")
    result = {}
    for material in sorted(set(_MATERIAL_BY_SERIES.values())):
        calibration = tuple(
            point for point in points
            if _MATERIAL_BY_SERIES.get(point.series_id) == material
            and split.partition_for(point) == "CALIBRATION")
        holdout = tuple(
            point for point in points
            if _MATERIAL_BY_SERIES.get(point.series_id) == material
            and split.partition_for(point) == "HOLDOUT")
        if not calibration:
            raise ValueError(f"material {material} has no calibration points")
        fit = _fit_one(calibration, impacts)
        fit["excluded_holdout_point_ids"] = [point.point_id for point in holdout]
        result[material] = fit
    return result


def build_fit_report(points, split, impacts):
    return {
        "algorithm": {
            "coarse_grid_step": 0.05,
            "fine_grid_step": 0.01,
            "objective": "unweighted_mean_squared_angular_residual_degrees",
            "stable_tie_break": ["objective", "normal_restitution",
                                 "friction_coefficient"],
            "surface_sliding_friction_hypothesis":
                _SURFACE_SLIDING_FRICTION,
        },
        "dataset_id": "domenech_2023_ball_collision",
        "fit_partition": "CALIBRATION",
        "materials": fit_material_parameters(points, split, impacts),
        "parameter_bounds": {
            "friction_coefficient": [0.0, 1.0],
            "normal_restitution": [0.0, 1.0],
        },
        "schema_version": 1,
    }


def main(argv=None):
    parser = argparse.ArgumentParser()
    parser.add_argument("--package", required=True)
    parser.add_argument("--output", required=True)
    arguments = parser.parse_args(argv)
    from .reference_package import load_reference_package
    from .reference_point import read_reference_points
    from .reference_split import load_reference_split
    package = load_reference_package(arguments.package)
    points = read_reference_points(
        package.files["normalized"], package.manifest["dataset_id"])
    split = load_reference_split(
        package.files["split"], points, package.manifest["dataset_id"],
        package.manifest["dataset_version"])
    impacts = read_impact_inputs(package.files["raw_extracted"])
    output = Path(arguments.output)
    output.parent.mkdir(parents=True, exist_ok=True)
    output.write_text(
        _canonical(build_fit_report(points, split, impacts)), encoding="utf-8")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
