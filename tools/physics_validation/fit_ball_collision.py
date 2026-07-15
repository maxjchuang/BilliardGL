import argparse
import csv
import json
import math
from dataclasses import dataclass
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
_V2_DATASETS = {"domenech_2023_ball_collision", "mathavan_2009_high_speed"}
_V2_SERIES = {"billiard_alpha1", "billiard_delta2", "mathavan_velocity"}
_V3_DATASETS = _V2_DATASETS | {"sudo_2002"}
_V3_SERIES = _V2_SERIES | {"sudo_ball_collision"}
_V2_INPUT_FIELDS = (
    "point_id", "dataset_id", "dataset_version", "lifecycle", "series_id",
    "case_id", "metric", "sample_phase", "impact_angle_degrees",
    "incident_speed_m_s", "expected", "unit", "standard_uncertainty",
    "source_locator", "normalized_path", "normalized_sha256",
    "raw_extracted_path", "raw_extracted_sha256",
)


@dataclass(frozen=True)
class ResidualRow:
    series_id: str
    normalized_residual: float


@dataclass(frozen=True)
class Fit:
    objective: float
    normal_restitution: float
    friction_coefficient: float


@dataclass(frozen=True)
class CollisionFitPoint:
    point_id: str
    dataset_id: str
    dataset_version: str
    lifecycle: str
    series_id: str
    case_id: str
    metric: str
    sample_phase: str
    impact_angle_degrees: float
    incident_speed_m_s: float
    expected: float
    unit: str
    standard_uncertainty: float
    source_locator: str
    normalized_path: str
    normalized_sha256: str
    raw_extracted_path: str
    raw_extracted_sha256: str


def series_balanced_objective(residual_rows):
    grouped = {}
    for row in residual_rows:
        if not math.isfinite(row.normalized_residual):
            raise ValueError("collision residual must be finite")
        grouped.setdefault(row.series_id, []).append(
            row.normalized_residual ** 2)
    if not grouped:
        raise ValueError("collision objective requires residual rows")
    by_series = {
        series_id: sum(values) / len(values)
        for series_id, values in sorted(grouped.items())
    }
    return sum(by_series.values()) / len(by_series), by_series


def select_fit(fits):
    fits = tuple(fits)
    if not fits:
        raise ValueError("collision fit selection requires candidates")
    return min(fits, key=lambda value: (
        value.objective,
        value.normal_restitution,
        value.friction_coefficient,
    ))


def _canonical(document):
    return json.dumps(
        document, ensure_ascii=False, indent=2, sort_keys=True,
        allow_nan=False) + "\n"


def _finite(row, field, line_number):
    try:
        value = float(row[field])
    except ValueError as error:
        raise ValueError(f"line {line_number}: {field} must be numeric") from error
    if not math.isfinite(value):
        raise ValueError(f"line {line_number}: {field} must be finite")
    return value


def _read_v2_impact_inputs(path):
    rows = []
    seen = set()
    with Path(path).open("r", encoding="utf-8", newline="") as source:
        reader = csv.DictReader(source)
        if tuple(reader.fieldnames or ()) != _V2_INPUT_FIELDS:
            raise ValueError("ball collision v2 input header does not match schema version 1")
        for line_number, row in enumerate(reader, start=2):
            point_id = row["point_id"]
            if not point_id or point_id in seen:
                raise ValueError(f"line {line_number}: point_id must be nonempty and unique")
            seen.add(point_id)
            if row["dataset_id"] not in _V3_DATASETS:
                raise ValueError(f"line {line_number}: confirmation or unknown dataset")
            if row["lifecycle"] != "spent":
                raise ValueError(f"line {line_number}: collision fit input must be spent")
            if row["series_id"] not in _V3_SERIES:
                raise ValueError(f"line {line_number}: unsupported collision series")
            impact = _finite(row, "impact_angle_degrees", line_number)
            speed = _finite(row, "incident_speed_m_s", line_number)
            expected = _finite(row, "expected", line_number)
            uncertainty = _finite(row, "standard_uncertainty", line_number)
            if impact < 0.0 or impact >= 90.0 or speed <= 0.0 or uncertainty <= 0.0:
                raise ValueError(f"line {line_number}: collision numeric domain is invalid")
            metric = row["metric"]
            if metric not in {
                    "cue_scattering_angle_degrees",
                    "object_scattering_angle_degrees",
                    "object_normal_deflection_angle_degrees",
                    "separation_angle_degrees",
                    "post_collision_cue_speed_cm_s",
                    "post_collision_object_speed_cm_s",
                    "ball_ball_normal_restitution",
                    "post_collision_separation_angle_degrees",
                    "transverse_momentum_deficit_fraction"}:
                raise ValueError(f"line {line_number}: unsupported collision metric")
            if row["sample_phase"] not in {
                    "immediate_post_impact", "first_pure_roll_after_event"}:
                raise ValueError(f"line {line_number}: unsupported collision sample phase")
            digests = (row["normalized_sha256"], row["raw_extracted_sha256"])
            if any(not digest.startswith("sha256:") or len(digest) != 71
                   for digest in digests):
                raise ValueError(f"line {line_number}: evidence digest is invalid")
            if not all((row["dataset_version"], row["case_id"], row["unit"],
                        row["source_locator"], row["normalized_path"],
                        row["raw_extracted_path"])):
                raise ValueError(f"line {line_number}: collision provenance is incomplete")
            rows.append(CollisionFitPoint(
                point_id=point_id,
                dataset_id=row["dataset_id"],
                dataset_version=row["dataset_version"],
                lifecycle=row["lifecycle"],
                series_id=row["series_id"],
                case_id=row["case_id"],
                metric=metric,
                sample_phase=row["sample_phase"],
                impact_angle_degrees=impact,
                incident_speed_m_s=speed,
                expected=expected,
                unit=row["unit"],
                standard_uncertainty=uncertainty,
                source_locator=row["source_locator"],
                normalized_path=row["normalized_path"],
                normalized_sha256=row["normalized_sha256"],
                raw_extracted_path=row["raw_extracted_path"],
                raw_extracted_sha256=row["raw_extracted_sha256"],
            ))
    if not rows or [row.point_id for row in rows] != sorted(seen):
        raise ValueError("ball collision v2 inputs must be nonempty and sorted")
    series = frozenset(row.series_id for row in rows)
    datasets = frozenset(row.dataset_id for row in rows)
    if (series, datasets) not in {
            (frozenset(_V2_SERIES), frozenset(_V2_DATASETS)),
            (frozenset(_V3_SERIES), frozenset(_V3_DATASETS))}:
        raise ValueError(
            "ball collision inputs do not cover one complete versioned series set")
    return tuple(rows)


def read_impact_inputs(path):
    with Path(path).open("r", encoding="utf-8", newline="") as source:
        fieldnames = tuple(csv.DictReader(source).fieldnames or ())
    if fieldnames == _V2_INPUT_FIELDS:
        return _read_v2_impact_inputs(path)
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


def _collision_state(impact_degrees, restitution, friction, speed=0.8,
                     return_diagnostics=False):
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
    regime = "frictionless"
    tangential_impulse = 0.0
    if tangent_speed > 1e-15 and friction > 0.0:
        tangent = _multiply(tangent_velocity, 1.0 / tangent_speed)
        denominator = 2.0 + (
            _dot(_cross(first_arm, tangent), _cross(first_arm, tangent)) +
            _dot(_cross(second_arm, tangent), _cross(second_arm, tangent))) / inertia
        desired = -_dot(relative, tangent) / denominator
        limit = friction * normal_impulse
        applied = max(-limit, min(limit, desired))
        tangential_impulse = applied
        regime = "sticking" if abs(desired) <= limit else "sliding"
        impulse = _add(impulse, _multiply(tangent, applied))
    first_velocity = _subtract(first_velocity, _multiply(impulse, 1.0 / mass))
    second_velocity = _add(second_velocity, _multiply(impulse, 1.0 / mass))
    first_angular = _add(
        first_angular,
        _multiply(_cross(first_arm, _multiply(impulse, -1.0)), 1.0 / inertia))
    second_angular = _add(
        second_angular,
        _multiply(_cross(second_arm, impulse), 1.0 / inertia))
    states = ((first_velocity, first_angular), (second_velocity, second_angular))
    if not return_diagnostics:
        return states
    initial_energy = 0.5 * mass * speed ** 2 + 0.5 * inertia * (speed / radius) ** 2
    final_energy = sum(
        0.5 * mass * _dot(velocity, velocity)
        + 0.5 * inertia * _dot(angular, angular)
        for velocity, angular in states
    )
    if final_energy > initial_energy + 1e-12:
        raise ValueError("collision candidate increases total kinetic energy")
    return states, {
        "kinetic_energy_after_j": final_energy,
        "kinetic_energy_before_j": initial_energy,
        "normal_impulse_ns": normal_impulse,
        "regime": regime,
        "tangential_impulse_ns": tangential_impulse,
    }


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


def _v2_prediction(point, restitution, friction):
    states, diagnostics = _collision_state(
        point.impact_angle_degrees,
        restitution,
        friction,
        speed=point.incident_speed_m_s,
        return_diagnostics=True,
    )
    first, second = states
    if point.sample_phase == "first_pure_roll_after_event":
        first = _at_pure_roll(first)
        second = _at_pure_roll(second)
    first_velocity = first[0]
    second_velocity = second[0]
    if point.metric == "cue_scattering_angle_degrees":
        prediction = _signed_angle(first_velocity)
    elif point.metric == "object_scattering_angle_degrees":
        prediction = _signed_angle(second_velocity, clockwise=True)
    elif point.metric == "separation_angle_degrees":
        first_planar = (first_velocity[0], first_velocity[2])
        second_planar = (second_velocity[0], second_velocity[2])
        denominator = math.hypot(*first_planar) * math.hypot(*second_planar)
        if denominator <= 1e-15:
            prediction = 0.0
        else:
            cosine = max(-1.0, min(1.0,
                (first_planar[0] * second_planar[0]
                 + first_planar[1] * second_planar[1]) / denominator))
            prediction = math.degrees(math.acos(cosine))
    elif point.metric == "object_normal_deflection_angle_degrees":
        angle = math.radians(point.impact_angle_degrees)
        normal = (math.cos(angle), -math.sin(angle))
        object_planar = (second_velocity[0], second_velocity[2])
        speed = math.hypot(*object_planar)
        if speed <= 1e-15:
            prediction = 0.0
        else:
            cosine = max(-1.0, min(1.0,
                (object_planar[0] * normal[0]
                 + object_planar[1] * normal[1]) / speed))
            prediction = math.degrees(math.acos(cosine))
    elif point.metric == "post_collision_cue_speed_cm_s":
        prediction = 100.0 * math.hypot(first_velocity[0], first_velocity[2])
    elif point.metric == "post_collision_object_speed_cm_s":
        prediction = 100.0 * math.hypot(second_velocity[0], second_velocity[2])
    elif point.metric == "ball_ball_normal_restitution":
        prediction = restitution
    elif point.metric == "post_collision_separation_angle_degrees":
        first_planar = (first_velocity[0], first_velocity[2])
        second_planar = (second_velocity[0], second_velocity[2])
        denominator = math.hypot(*first_planar) * math.hypot(*second_planar)
        if denominator <= 1e-15:
            prediction = 0.0
        else:
            cosine = max(-1.0, min(1.0,
                (first_planar[0] * second_planar[0]
                 + first_planar[1] * second_planar[1]) / denominator))
            prediction = math.degrees(math.acos(cosine))
    elif point.metric == "transverse_momentum_deficit_fraction":
        prediction = abs(first_velocity[2] + second_velocity[2]) / \
            point.incident_speed_m_s
    else:
        raise ValueError(f"unsupported collision metric {point.metric}")
    return prediction, diagnostics


def _v2_residuals(points, restitution, friction):
    rows = []
    detailed = []
    for point in points:
        predicted, diagnostics = _v2_prediction(point, restitution, friction)
        normalized = (predicted - point.expected) / point.standard_uncertainty
        rows.append(ResidualRow(point.series_id, normalized))
        detailed.append({
            "case_id": point.case_id,
            "dataset_id": point.dataset_id,
            "dataset_version": point.dataset_version,
            "expected": point.expected,
            "friction_coefficient": friction,
            "incident_speed_m_s": point.incident_speed_m_s,
            "impact_angle_degrees": point.impact_angle_degrees,
            "kinetic_energy_after_j": diagnostics["kinetic_energy_after_j"],
            "kinetic_energy_before_j": diagnostics["kinetic_energy_before_j"],
            "lifecycle": point.lifecycle,
            "metric": point.metric,
            "normal_restitution": restitution,
            "normal_impulse_ns": diagnostics["normal_impulse_ns"],
            "normalized_path": point.normalized_path,
            "normalized_sha256": point.normalized_sha256,
            "normalized_residual": normalized,
            "point_id": point.point_id,
            "predicted": predicted,
            "raw_extracted_path": point.raw_extracted_path,
            "raw_extracted_sha256": point.raw_extracted_sha256,
            "regime": diagnostics["regime"],
            "sample_phase": point.sample_phase,
            "series_id": point.series_id,
            "source_locator": point.source_locator,
            "standard_uncertainty": point.standard_uncertainty,
            "tangential_impulse_ns": diagnostics["tangential_impulse_ns"],
            "unit": point.unit,
        })
    objective, by_series = series_balanced_objective(rows)
    for row in detailed:
        row["series_mean_squared_normalized_residual"] = by_series[row["series_id"]]
    return objective, by_series, tuple(detailed)


def fit_ball_collision_parameters(points):
    points = tuple(sorted(points, key=lambda point: point.point_id))
    required_series = (
        _V3_SERIES if any(point.dataset_id == "sudo_2002" for point in points)
        else _V2_SERIES
    )
    required_datasets = (
        _V3_DATASETS if required_series == _V3_SERIES else _V2_DATASETS)
    if not points or {point.series_id for point in points} != required_series \
            or {point.dataset_id for point in points} != required_datasets:
        raise ValueError("collision fit requires one complete preregistered series set")
    coarse = []
    for restitution in _grid(0.0, 1.0, 0.05):
        for friction in _grid(0.0, 1.0, 0.05):
            objective, _, _ = _v2_residuals(
                points, restitution, friction)
            coarse.append(Fit(objective, restitution, friction))
    coarse_best = select_fit(coarse)
    fine = []
    for restitution in _grid(
            max(0.0, coarse_best.normal_restitution - 0.05),
            min(1.0, coarse_best.normal_restitution + 0.05), 0.01):
        for friction in _grid(
                max(0.0, coarse_best.friction_coefficient - 0.05),
                min(1.0, coarse_best.friction_coefficient + 0.05), 0.01):
            objective, _, _ = _v2_residuals(
                points, restitution, friction)
            fine.append(Fit(objective, restitution, friction))
    selected = select_fit(tuple(coarse) + tuple(fine))
    objective, by_series, residuals = _v2_residuals(
        points, selected.normal_restitution, selected.friction_coefficient)
    regimes = sorted({row["regime"] for row in residuals})
    return {
        "friction_coefficient": selected.friction_coefficient,
        "normal_restitution": selected.normal_restitution,
        "objective": objective,
        "objective_by_series": by_series,
        "point_count_by_series": {
            series_id: sum(point.series_id == series_id for point in points)
            for series_id in sorted(required_series)
        },
        "residuals": residuals,
        "selected_regimes": regimes,
    }


def build_v2_fit_report(points):
    fit = fit_ball_collision_parameters(points)
    return {
        "algorithm": {
            "coarse_grid_step": 0.05,
            "fine_grid_step": 0.01,
            "objective": (
                "mean_across_series_of_mean_squared_uncertainty_normalized_residual"
            ),
            "delta2_observation": (
                "object_direction_deflection_from_contact_normal_in_table_plane"
            ),
            "stable_tie_break": [
                "objective", "normal_restitution", "friction_coefficient"],
        },
        "dataset_lifecycle": "spent",
        "fit": {key: value for key, value in fit.items() if key != "residuals"},
        "model": {
            "contact_velocity": "translational_plus_angular",
            "energy_constraint": "non_increasing_total_kinetic_energy",
            "friction": "Coulomb_with_explicit_stick_slip",
            "inertia_factor": 0.4,
            "shape": "uniform_solid_sphere",
        },
        "parameter_bounds": {
            "friction_coefficient": [0.0, 1.0],
            "normal_restitution": [0.0, 1.0],
        },
        "schema_version": 2,
    }, fit["residuals"]


def build_v3_fit_report(points):
    fit = fit_ball_collision_v3(points)
    if set(fit["objective_by_series"]) != _V3_SERIES:
        raise ValueError("collision v3 fit requires Sudo spent evidence")
    return {
        "algorithm": {
            "coarse_grid_step": 0.05,
            "fine_grid_step": 0.01,
            "objective": (
                "mean_across_series_of_mean_squared_uncertainty_normalized_residual"
            ),
            "stable_tie_break": [
                "objective", "normal_restitution", "friction_coefficient"],
            "sudo_adapters": {
                "ball_ball_normal_restitution": "selected normal restitution",
                "post_collision_separation_angle_degrees": (
                    "physical 30-degree, 0.98 m/s post-contact velocity angle"
                ),
                "transverse_momentum_deficit_fraction": (
                    "absolute post-contact transverse momentum over incident momentum"
                ),
            },
        },
        "dataset_lifecycle": "spent",
        "fit": {key: value for key, value in fit.items() if key != "residuals"},
        "model": {
            "contact_velocity": "translational_plus_angular",
            "energy_constraint": "non_increasing_total_kinetic_energy",
            "friction": "Coulomb_with_explicit_stick_slip",
            "inertia_factor": 0.4,
            "shape": "uniform_solid_sphere",
        },
        "parameter_bounds": {
            "friction_coefficient": [0.0, 1.0],
            "normal_restitution": [0.0, 1.0],
        },
        "schema_version": 3,
    }, fit["residuals"]


def fit_ball_collision_v3(points):
    fit = fit_ball_collision_parameters(points)
    if set(fit["objective_by_series"]) != _V3_SERIES:
        raise ValueError("collision v3 fit requires Sudo spent evidence")
    return fit


def write_v2_fit_artifacts(points, output_path, residuals_path):
    report, residuals = build_v2_fit_report(points)
    output = Path(output_path)
    residual_output = Path(residuals_path)
    output.parent.mkdir(parents=True, exist_ok=True)
    residual_output.parent.mkdir(parents=True, exist_ok=True)
    output.write_text(_canonical(report), encoding="utf-8")
    fields = tuple(residuals[0])
    with residual_output.open("w", encoding="utf-8", newline="") as target:
        writer = csv.DictWriter(target, fieldnames=fields, lineterminator="\n")
        writer.writeheader()
        writer.writerows(residuals)
    return report


def write_fit_artifacts(points, output_path, residuals_path):
    points = tuple(points)
    if any(point.dataset_id == "sudo_2002" for point in points):
        report, residuals = build_v3_fit_report(points)
    else:
        report, residuals = build_v2_fit_report(points)
    output = Path(output_path)
    residual_output = Path(residuals_path)
    output.parent.mkdir(parents=True, exist_ok=True)
    residual_output.parent.mkdir(parents=True, exist_ok=True)
    output.write_text(_canonical(report), encoding="utf-8")
    fields = tuple(residuals[0])
    with residual_output.open("w", encoding="utf-8", newline="") as target:
        writer = csv.DictWriter(target, fieldnames=fields, lineterminator="\n")
        writer.writeheader()
        writer.writerows(residuals)
    return report


def main(argv=None):
    parser = argparse.ArgumentParser()
    source = parser.add_mutually_exclusive_group(required=True)
    source.add_argument("--package")
    source.add_argument("--inputs")
    parser.add_argument("--output", required=True)
    parser.add_argument("--residuals")
    arguments = parser.parse_args(argv)
    if arguments.inputs:
        if not arguments.residuals:
            parser.error("--residuals is required with --inputs")
        write_fit_artifacts(
            read_impact_inputs(arguments.inputs),
            arguments.output,
            arguments.residuals,
        )
        return 0
    if arguments.residuals:
        parser.error("--residuals is only valid with --inputs")
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
