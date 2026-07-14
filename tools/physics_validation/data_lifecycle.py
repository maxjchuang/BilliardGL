import json
import re
from dataclasses import dataclass
from pathlib import Path


_SAFE_ID = re.compile(r"[A-Za-z0-9][A-Za-z0-9_.-]*")
_STATES = {"calibration", "validation", "spent", "confirmation"}
_TOP_LEVEL_KEYS = {"schema_version", "datasets"}
_DATASET_KEYS = {
    "dataset_id", "dataset_version", "calibration_status", "holdout_status"}


def _safe_id(value, field):
    if not isinstance(value, str) or _SAFE_ID.fullmatch(value) is None:
        raise ValueError(f"{field} is not a safe stable ID")
    return value


@dataclass(frozen=True)
class DatasetLifecycle:
    dataset_id: str
    dataset_version: str
    calibration_status: str
    holdout_status: str


@dataclass(frozen=True)
class DataLifecycleRegistry:
    datasets: tuple

    def entry(self, dataset_id, dataset_version):
        matches = [
            item for item in self.datasets
            if item.dataset_id == dataset_id
            and item.dataset_version == dataset_version
        ]
        if len(matches) != 1:
            raise ValueError(
                f"dataset lifecycle is missing {dataset_id} {dataset_version}")
        return matches[0]

    def require_validation_holdout(self, dataset_id, dataset_version):
        entry = self.entry(dataset_id, dataset_version)
        if entry.holdout_status != "validation":
            raise ValueError(
                f"HOLDOUT must be in validation state, not {entry.holdout_status}")
        return entry


def load_data_lifecycle(path):
    try:
        document = json.loads(Path(path).read_text(encoding="utf-8"))
    except (OSError, UnicodeError, json.JSONDecodeError) as error:
        raise ValueError(f"cannot read data lifecycle registry: {error}") from error
    if not isinstance(document, dict) or set(document) != _TOP_LEVEL_KEYS:
        raise ValueError("data lifecycle keys do not match schema version 1")
    if document.get("schema_version") != 1:
        raise ValueError("data lifecycle schema_version must be 1")
    values = document.get("datasets")
    if not isinstance(values, list) or not values:
        raise ValueError("data lifecycle datasets must be a nonempty array")
    datasets = []
    for value in values:
        if not isinstance(value, dict) or set(value) != _DATASET_KEYS:
            raise ValueError("data lifecycle dataset keys do not match schema version 1")
        states = (value.get("calibration_status"), value.get("holdout_status"))
        if any(state not in _STATES for state in states):
            raise ValueError("unknown lifecycle state")
        datasets.append(DatasetLifecycle(
            dataset_id=_safe_id(value.get("dataset_id"), "dataset_id"),
            dataset_version=_safe_id(value.get("dataset_version"), "dataset_version"),
            calibration_status=states[0],
            holdout_status=states[1],
        ))
    identities = [(item.dataset_id, item.dataset_version) for item in datasets]
    if identities != sorted(identities) or len(identities) != len(set(identities)):
        raise ValueError("data lifecycle datasets must be sorted and unique")
    return DataLifecycleRegistry(tuple(datasets))
