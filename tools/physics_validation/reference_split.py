import json
import re
from dataclasses import dataclass
from pathlib import Path

from .reference_point import ReferencePoint


_SAFE_ID = re.compile(r"[A-Za-z0-9][A-Za-z0-9_.-]*")
_SCHEMA_KEYS = {
    "schema_version",
    "dataset_id",
    "dataset_version",
    "calibration_groups",
    "holdout_groups",
}


@dataclass(frozen=True)
class ReferenceSplit:
    dataset_id: str
    dataset_version: str
    calibration_groups: frozenset
    holdout_groups: frozenset

    def partition_for(self, point: ReferencePoint):
        in_calibration = point.group_id in self.calibration_groups
        in_holdout = point.group_id in self.holdout_groups
        if in_calibration == in_holdout:
            raise ValueError(
                f"point {point.point_id} must belong to exactly one committed partition")
        return "CALIBRATION" if in_calibration else "HOLDOUT"


def _safe_id(value, field):
    if not isinstance(value, str) or _SAFE_ID.fullmatch(value) is None:
        raise ValueError(f"{field} is not a safe stable ID")
    return value


def _groups(document, field):
    values = document.get(field)
    if not isinstance(values, list):
        raise ValueError(f"{field} must be an array")
    for value in values:
        _safe_id(value, field)
    if len(values) != len(set(values)):
        raise ValueError(f"{field} contains duplicate group IDs")
    return frozenset(values)


def load_reference_split(path, points, dataset_id, dataset_version):
    _safe_id(dataset_id, "dataset_id")
    _safe_id(dataset_version, "dataset_version")
    try:
        document = json.loads(Path(path).read_text(encoding="utf-8"))
    except (OSError, UnicodeError, json.JSONDecodeError) as error:
        raise ValueError(f"cannot read split.json: {error}") from error
    if not isinstance(document, dict) or set(document) != _SCHEMA_KEYS:
        raise ValueError("split.json keys do not match schema version 1")
    if document.get("schema_version") != 1:
        raise ValueError("split.json schema_version must be 1")
    document_dataset = _safe_id(document.get("dataset_id"), "dataset_id")
    document_version = _safe_id(document.get("dataset_version"), "dataset_version")
    if document_dataset != dataset_id:
        raise ValueError(
            f"split dataset_id {document_dataset} does not match {dataset_id}")
    if document_version != dataset_version:
        raise ValueError(
            f"split dataset_version {document_version} does not match {dataset_version}")
    calibration_groups = _groups(document, "calibration_groups")
    holdout_groups = _groups(document, "holdout_groups")
    overlap = calibration_groups & holdout_groups
    if overlap:
        raise ValueError(f"split groups overlap: {sorted(overlap)}")

    points = tuple(points)
    actual_groups = {point.group_id for point in points}
    declared_groups = calibration_groups | holdout_groups
    missing = actual_groups - declared_groups
    unknown = declared_groups - actual_groups
    if missing or unknown:
        raise ValueError(
            f"split groups must be exhaustive; missing={sorted(missing)}, "
            f"unknown={sorted(unknown)}")
    for point in points:
        if point.dataset_id != dataset_id:
            raise ValueError(
                f"point {point.point_id} dataset_id does not match split dataset_id")

    groups_by_case = {}
    partitions_by_group = {}
    for point in points:
        groups_by_case.setdefault(point.case_id, set()).add(point.group_id)
        partitions_by_group.setdefault(point.group_id, set()).add(point.partition)
    for case_id, groups in groups_by_case.items():
        if len(groups) != 1:
            raise ValueError(f"case_id {case_id} spans multiple group_id values")
    for group_id, partitions in partitions_by_group.items():
        if len(partitions) != 1:
            raise ValueError(f"group_id {group_id} spans normalized partitions")

    result = ReferenceSplit(
        document_dataset,
        document_version,
        calibration_groups,
        holdout_groups,
    )
    for point in points:
        committed_partition = result.partition_for(point)
        if point.partition != committed_partition:
            raise ValueError(
                f"point {point.point_id} normalized partition {point.partition} "
                f"does not match committed partition {committed_partition}")
    return result
