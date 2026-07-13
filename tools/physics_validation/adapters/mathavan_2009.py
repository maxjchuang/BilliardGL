import json
import math

from ..reference_adapter import (
    ReferenceCase,
    ReferenceLimitation,
    _canonical,
    _fail,
    _read_template,
)


_SOURCE_DIAMETER_CM = 5.24
_SOURCE_RADIUS_CM = _SOURCE_DIAMETER_CM / 2.0
_BALL_Y_CM = 89.341476


def _ball(index, position, velocity, angular_velocity):
    return {
        "angular_velocity_rad_s": angular_velocity,
        "index": index,
        "pocketed": False,
        "position_cm": position,
        "velocity_cm_s": velocity,
    }


def _scenario_balls(mapping):
    kind = mapping.get("kind")
    if kind == "free_motion":
        speed = mapping["initial_speed_cm_s"]
        angular = (
            [0.0, 0.0, -speed / _SOURCE_RADIUS_CM]
            if mapping["motion_phase"] == "rolling"
            else [0.0, 0.0, 0.0]
        )
        return [_ball(0, [0.0, _BALL_Y_CM, 0.0], [speed, 0.0, 0.0], angular)]
    if kind == "normal_cushion":
        speed = mapping["incident_speed_cm_s"]
        return [_ball(
            0,
            [55.0, _BALL_Y_CM, 20.0],
            [speed, 0.0, 0.0],
            [0.0, 0.0, -speed / _SOURCE_RADIUS_CM],
        )]
    if kind == "oblique_ball_collision":
        speed = mapping["incoming_speed_cm_s"]
        offset = _SOURCE_DIAMETER_CM * math.sin(
            math.radians(mapping["cut_angle_degrees"]))
        # The production solver checks contacts only after each 0.1 s move. Place
        # the cue ball so its first moved state is inside the contact threshold;
        # otherwise the high-speed experimental shots tunnel past the object ball.
        contact_distance = 5.0
        contact_x = math.sqrt(contact_distance ** 2 - offset ** 2)
        first_step_distance = max(0.0, speed - 0.4) * 0.1
        cue_start_x = -contact_x - first_step_distance
        return [
            _ball(
                0,
                [cue_start_x, _BALL_Y_CM, 0.0],
                [speed, 0.0, 0.0],
                [0.0, 0.0, -speed / _SOURCE_RADIUS_CM],
            ),
            _ball(1, [0.0, _BALL_Y_CM, offset], [0.0, 0.0, 0.0], [0.0, 0.0, 0.0]),
        ]
    _fail("INVALID_ADAPTER_TEMPLATE", f"unsupported Mathavan case kind: {kind}")


def _selection(mapping, point):
    kind = mapping["kind"]
    if kind == "free_motion":
        return {
            "ball_index": 0,
            "first_tick": 1,
            "last_tick": mapping["ticks"],
            "minimum_window_ticks": 3,
            "sample_phase": "declared_tick_window",
        }, 0
    if kind == "normal_cushion":
        return {
            "ball_index": 0,
            "event_kind": "rail_collision",
            "minimum_window_ticks": 1,
            "sample_phase": "first_sample_after_event",
        }, 0
    if kind == "oblique_ball_collision":
        ball_index = 1 if point.point_id.endswith("_object_speed") else 0
        return {
            "ball_index": ball_index,
            "event_kind": "ball_ball",
            "minimum_window_ticks": 1,
            "sample_phase": "first_sample_after_event",
        }, ball_index
    _fail("INVALID_ADAPTER_TEMPLATE", "Mathavan selection kind is invalid")


def adapt_mathavan_2009(package, split, points):
    template = _read_template(package)
    points_by_case = {}
    for point in points:
        points_by_case.setdefault(point.case_id, []).append(point)
    package_hashes = {
        item["id"]: item["sha256"] for item in package.manifest["files"]
    }
    cases = []
    for case_id in sorted(points_by_case):
        case_points = tuple(sorted(points_by_case[case_id], key=lambda item: item.point_id))
        partitions = {split.partition_for(point) for point in case_points}
        if len(partitions) != 1:
            _fail("MIXED_PARTITION_CASE", f"case {case_id} spans partitions")
        mapping = template["cases"].get(case_id)
        if not isinstance(mapping, dict):
            _fail("ADAPTER_MAPPING_MISSING", f"case {case_id} has no declared mapping")
        scenario = json.loads(json.dumps(template["base_scenario"], allow_nan=False))
        scenario["id"] = f"{package.manifest['dataset_id']}__{case_id}"
        scenario["simulation"]["ticks"] = mapping["ticks"]
        scenario["balls"] = _scenario_balls(mapping)
        scenario["evidence"]["source_ball_diameter_cm"] = _SOURCE_DIAMETER_CM
        scenario["expectations"] = []
        for point in case_points:
            selection, ball_index = _selection(mapping, point)
            lower, upper = point.acceptance_interval
            scenario["expectations"].append({
                "metric": "value_within_interval",
                "operator": "eq",
                "value": {
                    "ball_index": ball_index,
                    "expected": point.expected,
                    "lower": lower,
                    "observed_metric": point.metric,
                    "point_id": point.point_id,
                    "selection": selection,
                    "unit": point.unit,
                    "upper": upper,
                },
            })
        provenance = {
            "adapter_id": package.manifest["adapter_id"],
            "case_id": case_id,
            "dataset_id": package.manifest["dataset_id"],
            "dataset_version": package.manifest["dataset_version"],
            "package_hashes": package_hashes,
            "point_ids": [point.point_id for point in case_points],
            "source_locators": [point.source_locator for point in case_points],
            "source_ball_diameter_cm": _SOURCE_DIAMETER_CM,
        }
        cases.append(ReferenceCase(
            package.manifest["dataset_id"],
            package.manifest["dataset_version"],
            case_id,
            next(iter(partitions)),
            _canonical(scenario),
            case_points,
            _canonical(provenance),
        ))
    return tuple(cases)


def mathavan_2009_limitations(package):
    dataset_id = package.manifest["dataset_id"]
    return (
        ReferenceLimitation(
            dataset_id,
            "fig9_unresolved_markers",
            "cushion_rebound_speed_cm_s",
            "Twenty-three reported Fig. 9 shots overlap and lack independent coordinates.",
            "Obtain an author-verified per-shot numerical table.",
        ),
        ReferenceLimitation(
            dataset_id,
            "snooker_to_pool_material_conversion_missing",
            "equipment_conversion",
            "The source used 52.4 mm snooker balls and a Riley snooker table, not WPA Pool equipment.",
            "Measure or publish a validated material and geometry conversion to WPA Pool.",
        ),
        ReferenceLimitation(
            dataset_id,
            "unmeasured_initial_spin",
            "initial_spin_rad_s",
            "The experiment did not measure initial ball spin for the collision series.",
            "Acquire synchronized translational and rotational measurements for each shot.",
        ),
    )
