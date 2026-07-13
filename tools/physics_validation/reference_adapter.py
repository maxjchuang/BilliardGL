import json
import re
from dataclasses import dataclass

from .reference_package import ReferencePackageError


_SAFE_ID = re.compile(r"[A-Za-z0-9][A-Za-z0-9_.-]*")


@dataclass(frozen=True)
class ReferenceCase:
    dataset_id: str
    dataset_version: str
    case_id: str
    partition: str
    scenario_json: str
    points: tuple
    provenance_json: str


def _fail(code, message):
    raise ReferencePackageError(code, message)


def _canonical(document):
    return json.dumps(
        document,
        ensure_ascii=False,
        indent=2,
        sort_keys=True,
        allow_nan=False,
    ) + "\n"


class ReferenceAdapterRegistry:
    def __init__(self):
        self._factories = {}

    def register(self, adapter_id, factory):
        if not isinstance(adapter_id, str) or _SAFE_ID.fullmatch(adapter_id) is None:
            _fail("UNSAFE_ID", "adapter ID is not a safe stable ID")
        if not callable(factory):
            _fail("INVALID_ADAPTER", f"adapter {adapter_id} is not callable")
        if adapter_id in self._factories:
            _fail("DUPLICATE_ADAPTER", f"adapter {adapter_id} is already registered")
        self._factories[adapter_id] = factory

    def adapt(self, package, split, points):
        dataset_id = package.manifest["dataset_id"]
        points = tuple(points)
        for point in points:
            if point.dataset_id != dataset_id:
                _fail(
                    "DATASET_MISMATCH",
                    f"point {point.point_id} belongs to {point.dataset_id}, not {dataset_id}",
                )
        adapter_id = package.manifest["adapter_id"]
        factory = self._factories.get(adapter_id)
        if factory is None:
            _fail("UNKNOWN_ADAPTER", f"adapter {adapter_id} is not registered")
        cases = tuple(factory(package, split, points))
        if not all(isinstance(case, ReferenceCase) for case in cases):
            _fail("INVALID_ADAPTER", f"adapter {adapter_id} returned an invalid case")
        return cases


def _read_template(package):
    try:
        document = json.loads(
            package.files["scenario_template"].read_text(encoding="utf-8"))
    except (OSError, UnicodeError, json.JSONDecodeError) as error:
        _fail("INVALID_ADAPTER_TEMPLATE", f"cannot read scenario template: {error}")
    if not isinstance(document, dict) or document.get("schema_version") != 1:
        _fail("INVALID_ADAPTER_TEMPLATE", "scenario template must use schema version 1")
    if not isinstance(document.get("base_scenario"), dict) \
            or not isinstance(document.get("cases"), dict):
        _fail("INVALID_ADAPTER_TEMPLATE", "scenario template requires base_scenario and cases")
    return document


def _adapt_synthetic(package, split, points):
    template = _read_template(package)
    points_by_case = {}
    for point in points:
        points_by_case.setdefault(point.case_id, []).append(point)

    package_hashes = {
        item["id"]: item["sha256"]
        for item in package.manifest["files"]
    }
    cases = []
    for case_id in sorted(points_by_case):
        case_points = tuple(sorted(points_by_case[case_id], key=lambda point: point.point_id))
        partitions = {split.partition_for(point) for point in case_points}
        if len(partitions) != 1:
            _fail(
                "MIXED_PARTITION_CASE",
                f"case {case_id} contains calibration and holdout points",
            )
        mapping = template["cases"].get(case_id)
        if not isinstance(mapping, dict):
            _fail("ADAPTER_MAPPING_MISSING", f"case {case_id} has no declared mapping")
        if set(mapping) != {"initial_velocity_cm_s", "ticks"}:
            _fail("INVALID_ADAPTER_TEMPLATE", f"case {case_id} mapping keys are invalid")

        scenario = json.loads(json.dumps(template["base_scenario"], allow_nan=False))
        try:
            scenario["balls"][0]["velocity_cm_s"] = mapping["initial_velocity_cm_s"]
            scenario["simulation"]["ticks"] = mapping["ticks"]
        except (KeyError, IndexError, TypeError) as error:
            _fail("INVALID_ADAPTER_TEMPLATE", f"base scenario cannot accept mapping: {error}")
        scenario["id"] = f"{package.manifest['dataset_id']}__{case_id}"
        scenario["expectations"] = []
        for point in case_points:
            lower, upper = point.acceptance_interval
            scenario["expectations"].append({
                "metric": "value_within_interval",
                "value": {
                    "ball_index": 0,
                    "expected": point.expected,
                    "lower": lower,
                    "observed_metric": point.metric,
                    "point_id": point.point_id,
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
        }
        cases.append(ReferenceCase(
            dataset_id=package.manifest["dataset_id"],
            dataset_version=package.manifest["dataset_version"],
            case_id=case_id,
            partition=next(iter(partitions)),
            scenario_json=_canonical(scenario),
            points=case_points,
            provenance_json=_canonical(provenance),
        ))
    return tuple(cases)


def default_reference_registry():
    registry = ReferenceAdapterRegistry()
    registry.register("synthetic_free_roll_v1", _adapt_synthetic)
    return registry
