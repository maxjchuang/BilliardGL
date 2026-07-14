import json
import math

from ..fit_ball_collision import (
    _MATERIAL_BY_SERIES,
    fit_material_parameters,
    read_impact_inputs,
)
from ..reference_adapter import (
    ReferenceAdaptation,
    ReferenceCase,
    ReferenceLimitation,
    _canonical,
    _fail,
    _read_template,
)


_BALL_Y_BASE_CM = 90.0
_CONTACT_OVERLAP_CM = 0.000001
_LAUNCH_SPEED_CM_S = 80.0


def _admission_limitations(dataset_id):
    return (
        ReferenceLimitation(
            dataset_id,
            "author_data_request_pending",
            "source_table_availability",
            "The article states that data are available on request, but no author request has been sent without user authorization.",
            "Send and archive an authorized author-data request outside the repository, then version any received table separately.",
        ),
        ReferenceLimitation(
            dataset_id,
            "version_record_pdf_audit_pending",
            "source_version_audit",
            "Publisher automation challenge prevented a complete version-of-record PDF audit; official high-resolution figures and article metadata were audited instead.",
            "Lawfully obtain and manually inspect the version-of-record PDF, retaining its digest without redistributing it.",
        ),
    )


def _profile(material, source, fit, post_transition):
    return {
        "id": f"domenech_2023_{material}_{'post_transition' if post_transition else 'immediate'}_v1",
        "formula_version": "ball_collision_v1",
        "ball": {
            "friction_coefficient": fit["friction_coefficient"],
            "inertia_factor": 0.4,
            "mass_kg": source["mass_g"] / 1000.0,
            "material": material,
            "normal_restitution": fit["normal_restitution"],
            "radius_cm": source["diameter_cm"] / 2.0,
        },
        "surface": {
            "legacy_friction_acceleration_cm_s2": 4.0,
            "material": "pvc_source_hypothesis",
            "rolling_resistance_acceleration_cm_s2": 0.0,
            "sliding_friction_coefficient": 0.002 if post_transition else 0.0,
            "slip_speed_epsilon_cm_s": 0.0001,
            "stop_energy_threshold_j": 0.000000001,
            "torsional_spin_deceleration_rad_s2": 0.0,
        },
        "cue": {
            "chalked_friction_coefficient": 0.6,
            "cue_speed_per_power_unit_cm_s": 1.34,
            "effective_mass_kg": 0.5,
            "maximum_reliable_offset_radius": 0.8,
            "normal_restitution": 0.0,
            "unchalked_friction_coefficient": 0.1,
        },
        "cushion": {
            "friction_coefficient": 0.0,
            "normal_restitution": 1.0,
        },
        "solver": {
            "maximum_events_per_tick": 64,
            "time_step_seconds": 0.1,
        },
    }


def _balls(source, impact_degrees):
    radius = source["diameter_cm"] / 2.0
    contact_distance = source["diameter_cm"] - _CONTACT_OVERLAP_CM
    angle = math.radians(impact_degrees)
    y = _BALL_Y_BASE_CM + radius
    return [
        {
            "angular_velocity_rad_s": [0.0, 0.0, -_LAUNCH_SPEED_CM_S / radius],
            "index": 0,
            "pocketed": False,
            "position_cm": [
                -contact_distance * math.cos(angle), y,
                contact_distance * math.sin(angle),
            ],
            "velocity_cm_s": [_LAUNCH_SPEED_CM_S, 0.0, 0.0],
        },
        {
            "angular_velocity_rad_s": [0.0, 0.0, 0.0],
            "index": 1,
            "pocketed": False,
            "position_cm": [0.0, y, 0.0],
            "velocity_cm_s": [0.0, 0.0, 0.0],
        },
    ]


def _selection(point, source):
    post_transition = point.series_id in {"steel_beta1", "rubber_lambda2"}
    selection = {
        "ball_index": 1 if point.metric == "object_scattering_angle_degrees" else 0,
        "event_kind": "ball_ball",
        "minimum_window_ticks": 2 if post_transition else 1,
        "sample_phase": "first_pure_roll_after_event" if post_transition
                        else "immediate_post_impact",
    }
    if post_transition:
        selection.update({
            "ball_radius_cm": source["diameter_cm"] / 2.0,
            "pure_roll_tolerance_cm_s": 0.001,
        })
    if point.metric == "separation_angle_degrees":
        selection["other_ball_index"] = 1
    if point.metric in {
            "cue_scattering_angle_degrees", "object_scattering_angle_degrees"}:
        selection["angle_reference_axis"] = [1.0, 0.0]
        selection["positive_orientation"] = (
            "clockwise" if point.metric == "object_scattering_angle_degrees"
            else "counterclockwise")
    return selection


def adapt_domenech_2023(package, split, points):
    template = _read_template(package)
    dataset_id = package.manifest["dataset_id"]
    materials = package.manifest["apparatus"]["materials"]
    points = tuple(sorted(points, key=lambda point: point.point_id))
    impacts = read_impact_inputs(package.files["raw_extracted"])
    series_by_case = {}
    for point in points:
        material = _MATERIAL_BY_SERIES.get(point.series_id)
        if material is None:
            _fail("UNKNOWN_SOURCE_MATERIAL",
                  f"series {point.series_id} has no material mapping")
        series_by_case.setdefault(point.case_id, set()).add(material)
    mixed = [case_id for case_id, case_materials in series_by_case.items()
             if len(case_materials) != 1]
    if mixed:
        _fail("MIXED_MATERIAL_CASE",
              f"cases mix source materials: {sorted(mixed)}")
    fits = fit_material_parameters(points, split, impacts)

    package_hashes = {
        item["id"]: item["sha256"] for item in package.manifest["files"]
    }
    cases = []
    for point in points:
        impact = impacts.get(point.point_id)
        if impact is None or impact["case_id"] != point.case_id \
                or impact["series_id"] != point.series_id:
            _fail("SOURCE_INPUT_MISMATCH",
                  f"point {point.point_id} does not match raw collision input")
        material = _MATERIAL_BY_SERIES[point.series_id]
        source = materials[material]
        post_transition = point.series_id in {"steel_beta1", "rubber_lambda2"}
        scenario = json.loads(json.dumps(
            template["base_scenario"], allow_nan=False))
        scenario["schema_version"] = 5
        scenario["id"] = f"{dataset_id}__{point.case_id}"
        scenario["physics_profile"] = _profile(
            material, source, fits[material], post_transition)
        scenario["balls"] = _balls(source, impact["impact_angle_degrees"])
        scenario["simulation"]["ticks"] = 400 if post_transition else 40
        lower, upper = point.acceptance_interval
        scenario["expectations"] = [{
            "metric": "value_within_interval",
            "operator": "eq",
            "value": {
                "ball_index": 1 if point.metric ==
                    "object_scattering_angle_degrees" else 0,
                "expected": point.expected,
                "lower": lower,
                "observed_metric": point.metric,
                "point_id": point.point_id,
                "selection": _selection(point, source),
                "unit": point.unit,
                "upper": upper,
            },
        }]
        scenario["evidence"]["pool_applicability"] = point.pool_applicability
        provenance = {
            "adapter_id": package.manifest["adapter_id"],
            "case_id": point.case_id,
            "dataset_id": dataset_id,
            "dataset_version": package.manifest["dataset_version"],
            "fit_calibration_point_ids": fits[material]["calibration_point_ids"],
            "fit_parameters": {
                "friction_coefficient": fits[material]["friction_coefficient"],
                "normal_restitution": fits[material]["normal_restitution"],
            },
            "impact_angle_degrees": impact["impact_angle_degrees"],
            "package_hashes": package_hashes,
            "point_ids": [point.point_id],
            "source_locators": [point.source_locator],
            "source_material": material,
        }
        cases.append(ReferenceCase(
            dataset_id, package.manifest["dataset_version"], point.case_id,
            split.partition_for(point), _canonical(scenario), (point,),
            _canonical(provenance)))
    return ReferenceAdaptation(tuple(cases), _admission_limitations(dataset_id))
