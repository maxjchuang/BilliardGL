import csv
import json

from ..reference_adapter import ReferenceCase, ReferenceLimitation, _canonical, _fail, _read_template


_SOURCE_RADIUS_CM = 2.625
_BALL_Y_CM = 86.483976 + _SOURCE_RADIUS_CM
_CONTACT_CENTER_X_CM = 63.5 - _SOURCE_RADIUS_CM


def _raw_by_point(package):
    with package.files["raw_extracted"].open("r", encoding="utf-8", newline="") as source:
        rows = tuple(csv.DictReader(source))
    return {row["point_id"]: row for row in rows}


def adapt_mathavan_2010(package, split, points):
    template = _read_template(package)
    raw = _raw_by_point(package)
    hashes = {item["id"]: item["sha256"] for item in package.manifest["files"]}
    cases = []
    for point in sorted(points, key=lambda item: item.point_id):
        if point.series_id != "perpendicular_rolling_rebound":
            _fail("THEORY_EVIDENCE_REJECTED", f"series {point.series_id} is not Fig. 7 experimental evidence")
        source = raw.get(point.point_id)
        if source is None or source["evidence_kind"] != "experimental_marker":
            _fail("ADAPTER_MAPPING_MISSING", f"point {point.point_id} lacks experimental raw evidence")
        partition = split.partition_for(point)
        incident_at_impact = float(source["incident_speed_m_s"]) * 100.0
        # Reserve four 0.1 s approach samples. The versioned 12.5 cm/s^2
        # rolling resistance removes 5 cm/s before the paired fit reaches contact.
        launch_speed = incident_at_impact + 5.0
        scenario = json.loads(json.dumps(template["base_scenario"], allow_nan=False))
        scenario["id"] = f"{package.manifest['dataset_id']}__{point.case_id}"
        scenario["balls"] = [{
            "angular_velocity_rad_s": [0.0, 0.0, -launch_speed / _SOURCE_RADIUS_CM],
            "index": 0,
            "pocketed": False,
            "position_cm": [
                _CONTACT_CENTER_X_CM - (launch_speed * 0.40 - 1.0),
                _BALL_Y_CM,
                20.0,
            ],
            "velocity_cm_s": [launch_speed, 0.0, 0.0],
        }]
        lower, upper = point.acceptance_interval
        scenario["expectations"] = [{
            "metric": "value_within_interval",
            "operator": "eq",
            "value": {
                "ball_index": 0,
                "expected": point.expected,
                "lower": lower,
                "observed_metric": point.metric,
                "point_id": point.point_id,
                "selection": {
                    "ball_index": 0,
                    "ball_radius_cm": _SOURCE_RADIUS_CM,
                    "event_kind": "rail_collision",
                    "incident_speed_cm_s": incident_at_impact,
                    "incident_speed_tolerance_cm_s": 0.25,
                    "incident_window_ticks": 3,
                    "minimum_window_ticks": 3,
                    "pure_roll_tolerance_cm_s": 0.001,
                    "rebound_window_ticks": 3,
                    "sample_phase": "immediate_post_impact",
                    "sidespin_tolerance_rad_s": 0.001,
                },
                "unit": point.unit,
                "upper": upper,
            },
        }]
        provenance = {
            "adapter_id": package.manifest["adapter_id"],
            "case_id": point.case_id,
            "dataset_id": package.manifest["dataset_id"],
            "dataset_version": package.manifest["dataset_version"],
            "fit_subset": source["fit_subset"] == "true",
            "incident_speed_cm_s": incident_at_impact,
            "package_hashes": hashes,
            "point_ids": [point.point_id],
            "rigid_cushion_domain": source["rigid_cushion_domain"] == "true",
            "source_ball_mass_kg": package.manifest["apparatus"]["ball_mass_kg"],
            "source_ball_radius_cm": _SOURCE_RADIUS_CM,
            "source_locator": point.source_locator,
        }
        cases.append(ReferenceCase(
            package.manifest["dataset_id"], package.manifest["dataset_version"],
            point.case_id, partition, _canonical(scenario), (point,), _canonical(provenance)))
    return tuple(cases)


def mathavan_2010_limitations(package):
    dataset_id = package.manifest["dataset_id"]
    return (
        ReferenceLimitation(dataset_id, "oblique_experimental_rebound_angle_unavailable", "cushion_rebound_angle_degrees", "The source reports no experimental oblique-incidence rebound-angle series.", "Acquire synchronized experimental incident/rebound angles for oblique cushion impacts."),
        ReferenceLimitation(dataset_id, "experimental_spin_change_unavailable", "post_collision_angular_velocity_rad_s", "The source experiment did not measure spin change through cushion impact.", "Acquire synchronized pre/post-impact angular-velocity measurements."),
        ReferenceLimitation(dataset_id, "snooker_cushion_to_pool_material_conversion_missing", "equipment_conversion", "Riley Renaissance snooker cushion, cloth, and 52.5 mm balls are not the production Chinese Pool apparatus.", "Publish a validated cushion/cloth/ball material conversion to production equipment."),
        ReferenceLimitation(dataset_id, "rigid_cushion_domain_warning", "rigid_cushion_domain", "The authors identify incident speeds above 2.5 m/s as outside the reliable rigid-cushion model domain.", "Measure cushion deformation and contact response above 2.5 m/s."),
    )
