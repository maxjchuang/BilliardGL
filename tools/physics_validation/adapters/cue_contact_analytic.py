import json

from ..reference_adapter import (
    ReferenceAdaptation,
    ReferenceCase,
    _canonical,
    _fail,
)


def adapt_cue_contact_analytic(package, split, points):
    try:
        template = json.loads(package.files["scenario_template"].read_text(encoding="utf-8"))
    except (OSError, UnicodeError, json.JSONDecodeError) as error:
        _fail("INVALID_ADAPTER_TEMPLATE", str(error))
    if template.get("schema_version") != 1 or not isinstance(template.get("cases"), dict):
        _fail("INVALID_ADAPTER_TEMPLATE", "analytic template schema is invalid")
    points_by_case = {}
    for point in points:
        points_by_case.setdefault(point.case_id, []).append(point)
    package_hashes = {item["id"]: item["sha256"] for item in package.manifest["files"]}
    cases = []
    for case_id in sorted(points_by_case):
        case_points = tuple(sorted(points_by_case[case_id], key=lambda point: point.point_id))
        partitions = {split.partition_for(point) for point in case_points}
        if len(partitions) != 1:
            _fail("MIXED_PARTITION_CASE", f"case {case_id} crosses partitions")
        mapping = template["cases"].get(case_id)
        if not isinstance(mapping, dict) or set(mapping) != {
                "expected_regime", "tip_offset_radius"}:
            _fail("ADAPTER_MAPPING_MISSING", f"case {case_id} mapping is invalid")
        scenario = json.loads(json.dumps(template["base_scenario"], allow_nan=False))
        radius = scenario["physics_profile"]["ball"]["radius_cm"]
        offset = mapping["tip_offset_radius"]
        scenario["id"] = f"{package.manifest['dataset_id']}__{case_id}"
        scenario["cue_impact"] = {
            "cue_ball_index": 0,
            "cue_speed_cm_s": 100.0,
            "cue_mass_kg": 0.5,
            "direction": [1.0, 0.0, 0.0],
            "elevation_degrees": 0.0,
            "tip_offset_cm": [offset[0] * radius, offset[1] * radius],
            "tip_offset_radius": offset,
            "chalk_state": "CHALKED",
        }
        scenario["expectations"] = []
        for point in case_points:
            lower, upper = point.acceptance_interval
            selection = {
                "sample_phase": "first_stable_frames_after_cue_impact",
                "source_phase": "analytic_instant_after_impulse",
                "ball_index": 0,
                "minimum_window_ticks": 2,
                "input_support": True,
                "stability_tolerance": 0.0001,
                "expected_regime": mapping["expected_regime"],
            }
            if point.metric == "cue_impact_angular_speed_rad_s":
                selection["angular_axis"] = "z" if "vertical" in case_id else "y"
                selection["angular_sign"] = 1
            scenario["expectations"].append({
                "metric": "value_within_interval", "operator": "eq",
                "value": {"point_id": point.point_id,
                          "observed_metric": point.metric, "ball_index": 0,
                          "expected": point.expected, "lower": lower, "upper": upper,
                          "unit": point.unit, "selection": selection},
            })
        provenance = {
            "adapter_id": package.manifest["adapter_id"], "case_id": case_id,
            "dataset_id": package.manifest["dataset_id"],
            "dataset_version": package.manifest["dataset_version"],
            "package_hashes": package_hashes,
            "point_ids": [point.point_id for point in case_points],
            "source_locators": [point.source_locator for point in case_points],
            "evidence_grade": "C", "experimental_validation": False,
        }
        cases.append(ReferenceCase(
            dataset_id=package.manifest["dataset_id"],
            dataset_version=package.manifest["dataset_version"],
            case_id=case_id, partition=next(iter(partitions)),
            scenario_json=_canonical(scenario), points=case_points,
            provenance_json=_canonical(provenance)))
    return ReferenceAdaptation(tuple(cases), ())
