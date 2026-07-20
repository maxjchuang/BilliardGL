import json

from ..reference_adapter import ReferenceAdaptation, ReferenceCase, _canonical, _fail


def adapt_multi_contact_solver_analytic(package, split, points):
    try:
        template = json.loads(package.files["scenario_template"].read_text(encoding="utf-8"))
    except (OSError, UnicodeError, json.JSONDecodeError) as error:
        _fail("INVALID_ADAPTER_TEMPLATE", str(error))
    points_by_case = {}
    for point in points:
        points_by_case.setdefault(point.case_id, []).append(point)
    hashes = {item["id"]: item["sha256"] for item in package.manifest["files"]}
    cases = []
    for case_id in sorted(points_by_case):
        case_points = tuple(sorted(points_by_case[case_id], key=lambda value: value.point_id))
        partitions = {split.partition_for(point) for point in case_points}
        if len(partitions) != 1:
            _fail("MIXED_PARTITION_CASE", case_id)
        mapping = template["cases"].get(case_id)
        if not isinstance(mapping, dict):
            _fail("ADAPTER_MAPPING_MISSING", case_id)
        scenario = json.loads(json.dumps(template["base_scenario"], allow_nan=False))
        scenario["id"] = f"{package.manifest['dataset_id']}__{case_id}"
        scenario["balls"] = mapping["balls"]
        scenario["expectations"] = []
        for point in case_points:
            lower, upper = point.acceptance_interval
            scenario["expectations"].append({
                "metric": "value_within_interval", "operator": "eq",
                "value": {"point_id": point.point_id,
                          "observed_metric": point.metric,
                          "ball_index": 0,
                          "expected": point.expected, "lower": lower,
                          "upper": upper, "unit": point.unit,
                          "selection": {"sample_phase": "complete_trace"}},
            })
        cases.append(ReferenceCase(
            dataset_id=package.manifest["dataset_id"],
            dataset_version=package.manifest["dataset_version"],
            case_id=case_id, partition=next(iter(partitions)),
            scenario_json=_canonical(scenario), points=case_points,
            provenance_json=_canonical({
                "adapter_id": package.manifest["adapter_id"], "case_id": case_id,
                "dataset_id": package.manifest["dataset_id"],
                "dataset_version": package.manifest["dataset_version"],
                "package_hashes": hashes,
                "point_ids": [point.point_id for point in case_points],
                "source_locators": [point.source_locator for point in case_points],
                "evidence_grade": "C", "experimental_validation": False,
            })))
    return ReferenceAdaptation(tuple(cases), ())
