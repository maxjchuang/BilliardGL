import json
import re
from dataclasses import dataclass
from pathlib import Path

from .analyzer import MODEL_MISMATCH, REFERENCE_LIMITATION


_SAFE_ID = re.compile(r"[A-Za-z0-9][A-Za-z0-9_.-]*")
_MANIFEST_KEYS = {"schema_version", "failures"}
_MODEL_KEYS = {"dataset_id", "case_id", "code", "metric", "rationale"}
_LIMITATION_KEYS = {
    "dataset_id",
    "case_id",
    "code",
    "metric",
    "missing_evidence",
    "resolution_condition",
}


@dataclass(frozen=True, order=True)
class ReferenceFailureKey:
    dataset_id: str
    case_id: str
    code: str
    metric: str


@dataclass(frozen=True)
class ReferenceAccounting:
    known_model_mismatches: frozenset
    new_model_mismatches: frozenset
    missing_model_mismatches: frozenset
    known_limitations: frozenset
    new_limitations: frozenset
    missing_limitations: frozenset
    unallowlistable_failures: frozenset

    @property
    def ci_passed(self):
        return not (
            self.new_model_mismatches
            or self.missing_model_mismatches
            or self.new_limitations
            or self.missing_limitations
            or self.unallowlistable_failures
        )


def _safe_id(value, field):
    if not isinstance(value, str) or _SAFE_ID.fullmatch(value) is None:
        raise ValueError(f"{field} is not a safe stable ID")
    return value


def _read_document(source):
    if isinstance(source, dict):
        return source
    try:
        return json.loads(Path(source).read_text(encoding="utf-8"))
    except (OSError, UnicodeError, json.JSONDecodeError) as error:
        raise ValueError(f"cannot read expected failure manifest: {error}") from error


def _expected_failures(source, dataset_id, expected_code):
    document = _read_document(source)
    if not isinstance(document, dict) or set(document) != _MANIFEST_KEYS:
        raise ValueError("expected failure manifest schema keys are invalid")
    if document.get("schema_version") != 1:
        raise ValueError("expected failure manifest schema_version must be 1")
    items = document.get("failures")
    if not isinstance(items, list):
        raise ValueError("expected failure manifest failures must be an array")
    required_keys = _MODEL_KEYS if expected_code == MODEL_MISMATCH else _LIMITATION_KEYS
    audit_keys = (
        ("rationale",)
        if expected_code == MODEL_MISMATCH
        else ("missing_evidence", "resolution_condition")
    )
    failures = set()
    for item in items:
        if not isinstance(item, dict) or set(item) != required_keys:
            raise ValueError("expected failure entry schema keys are invalid")
        item_dataset = _safe_id(item.get("dataset_id"), "dataset_id")
        if item_dataset != dataset_id:
            raise ValueError(
                f"manifest dataset_id {item_dataset} does not match {dataset_id}")
        case_id = _safe_id(item.get("case_id"), "case_id")
        metric = _safe_id(item.get("metric"), "metric")
        code = item.get("code")
        if code != expected_code:
            raise ValueError(
                f"manifest code {code} is invalid; expected only {expected_code}")
        for field in audit_keys:
            if not isinstance(item.get(field), str) or not item[field].strip():
                raise ValueError(f"expected failure entry schema requires {field}")
        key = ReferenceFailureKey(item_dataset, case_id, code, metric)
        if key in failures:
            raise ValueError(f"duplicate expected failure entry: {key}")
        failures.add(key)
    return frozenset(failures)


def _case_id(scenario_id, dataset_id):
    prefix = dataset_id + "__"
    case_id = scenario_id[len(prefix):] if scenario_id.startswith(prefix) else scenario_id
    return _safe_id(case_id, "case_id")


def reconcile_reference_failures(results, model_manifest, limitation_manifest, dataset_id):
    _safe_id(dataset_id, "dataset_id")
    expected_model = _expected_failures(
        model_manifest, dataset_id, MODEL_MISMATCH)
    expected_limitations = _expected_failures(
        limitation_manifest, dataset_id, REFERENCE_LIMITATION)

    actual_model = set()
    actual_limitations = set()
    unallowlistable = set()
    for result in results:
        case_id = _case_id(result.scenario_id, dataset_id)
        for failure in result.failures:
            metric = _safe_id(failure.metric, "metric")
            key = ReferenceFailureKey(dataset_id, case_id, failure.code, metric)
            if failure.code == MODEL_MISMATCH:
                actual_model.add(key)
            elif failure.code == REFERENCE_LIMITATION:
                actual_limitations.add(key)
            else:
                unallowlistable.add(key)

    actual_model = frozenset(actual_model)
    actual_limitations = frozenset(actual_limitations)
    return ReferenceAccounting(
        known_model_mismatches=actual_model & expected_model,
        new_model_mismatches=actual_model - expected_model,
        missing_model_mismatches=expected_model - actual_model,
        known_limitations=actual_limitations & expected_limitations,
        new_limitations=actual_limitations - expected_limitations,
        missing_limitations=expected_limitations - actual_limitations,
        unallowlistable_failures=frozenset(unallowlistable),
    )
