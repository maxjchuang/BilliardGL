import csv
import json
import math
from pathlib import Path

from .analyzer import (
    INTEGRATION_MISMATCH,
    MODEL_MISMATCH,
    NON_DETERMINISTIC,
    NUMERICAL_FAILURE,
    REFERENCE_LIMITATION,
)
from .reference_accounting import ReferenceFailureKey


_PARTITIONS = ("CALIBRATION", "HOLDOUT")
_CSV_FIELDS = (
    "dataset_id",
    "dataset_version",
    "series_id",
    "group_id",
    "case_id",
    "scenario_id",
    "point_id",
    "partition",
    "metric",
    "unit",
    "prediction",
    "prediction_nonfinite",
    "experimental_value",
    "signed_error",
    "acceptance_lower",
    "acceptance_upper",
    "measurement_uncertainty",
    "digitization_uncertainty",
    "conversion_uncertainty",
    "combined_standard_uncertainty",
    "coverage_factor",
    "engineering_absolute_tolerance",
    "engineering_relative_tolerance",
    "source_locator",
    "pool_applicability",
    "status",
    "trace_path",
    "build_id",
    "replay_command",
    "package_hashes",
)
_FAILURE_PRIORITY = {
    NUMERICAL_FAILURE: 0,
    INTEGRATION_MISMATCH: 1,
    NON_DETERMINISTIC: 2,
    MODEL_MISMATCH: 3,
    REFERENCE_LIMITATION: 4,
}


def _json(document):
    return json.dumps(
        document, ensure_ascii=False, indent=2, sort_keys=True, allow_nan=False) + "\n"


def _scenario_id(case):
    scenario = json.loads(case.scenario_json)
    scenario_id = scenario.get("id")
    if not isinstance(scenario_id, str) or not scenario_id:
        raise ValueError(f"reference case {case.case_id} has no scenario ID")
    return scenario_id


def _failure_key(case, code, metric):
    return ReferenceFailureKey(case.dataset_id, case.case_id, code, metric)


def _failure_status(case, point, failure, accounting):
    code = failure.code
    key = _failure_key(case, code, point.metric)
    if code == MODEL_MISMATCH:
        if key in accounting.known_model_mismatches:
            return "MODEL_MISMATCH_KNOWN"
        return "MODEL_MISMATCH_NEW"
    if code == REFERENCE_LIMITATION:
        if key in accounting.known_limitations:
            return "REFERENCE_LIMITATION_KNOWN"
        return "REFERENCE_LIMITATION_NEW"
    if code == INTEGRATION_MISMATCH:
        return "INTEGRATION_MISMATCH"
    if code == NUMERICAL_FAILURE:
        return "NUMERICAL_FAILURE"
    if code == NON_DETERMINISTIC:
        return "NON_DETERMINISTIC"
    return "NUMERICAL_FAILURE"


def _point_failure(result, metric):
    if result is None:
        return None
    exact = [failure for failure in result.failures if failure.metric == metric]
    global_failures = [
        failure for failure in result.failures
        if failure.code in {NUMERICAL_FAILURE, INTEGRATION_MISMATCH, NON_DETERMINISTIC}
    ]
    candidates = exact or global_failures
    if not candidates:
        return None
    return sorted(
        candidates,
        key=lambda failure: (_FAILURE_PRIORITY.get(failure.code, 99), failure.metric),
    )[0]


def _reference_point_failure(result, point):
    if result is None:
        return None
    point_failures = [
        failure for failure in result.failures
        if failure.point_id == point.point_id
    ]
    if point_failures:
        return sorted(
            point_failures,
            key=lambda failure: (_FAILURE_PRIORITY.get(failure.code, 99), failure.metric),
        )[0]
    untagged = [
        failure for failure in result.failures
        if failure.point_id is None and (
            failure.metric == point.metric
            or failure.code in {
                NUMERICAL_FAILURE, INTEGRATION_MISMATCH, NON_DETERMINISTIC,
            }
        )
    ]
    if not untagged:
        return None
    return sorted(
        untagged,
        key=lambda failure: (_FAILURE_PRIORITY.get(failure.code, 99), failure.metric),
    )[0]


def _point_row(case, point, result, accounting, metadata):
    scenario_id = _scenario_id(case)
    scenario_metadata = metadata.get("scenarios", {}).get(scenario_id, {})
    provenance = json.loads(case.provenance_json)
    prediction = None if result is None else result.metrics.get(
        point.point_id, result.metrics.get(point.metric))
    prediction_nonfinite = None
    if isinstance(prediction, float) and not math.isfinite(prediction):
        if math.isnan(prediction):
            prediction_nonfinite = "NaN"
        elif prediction > 0.0:
            prediction_nonfinite = "+Infinity"
        else:
            prediction_nonfinite = "-Infinity"
        prediction = None
    failure = _reference_point_failure(result, point)
    if result is None or (prediction is None and failure is None):
        status = "INTEGRATION_MISMATCH"
    elif failure is None:
        status = "PASSED"
    else:
        status = _failure_status(case, point, failure, accounting)
    signed_error = None if prediction is None else prediction - point.expected
    lower, upper = point.acceptance_interval
    return {
        "acceptance_interval": [lower, upper],
        "build_id": metadata.get("build_id"),
        "case_id": case.case_id,
        "combined_standard_uncertainty": point.combined_standard_uncertainty,
        "conversion_uncertainty": point.conversion_uncertainty,
        "coverage_factor": point.coverage_factor,
        "dataset_id": case.dataset_id,
        "dataset_version": case.dataset_version,
        "digitization_uncertainty": point.digitization_uncertainty,
        "engineering_absolute_tolerance": point.engineering_absolute_tolerance,
        "engineering_relative_tolerance": point.engineering_relative_tolerance,
        "experimental_value": point.expected,
        "group_id": point.group_id,
        "measurement_uncertainty": point.measurement_uncertainty,
        "metric": point.metric,
        "package_hashes": provenance.get("package_hashes", {}),
        "partition": case.partition,
        "point_id": point.point_id,
        "pool_applicability": point.pool_applicability,
        "prediction": prediction,
        "prediction_nonfinite": prediction_nonfinite,
        "replay_command": scenario_metadata.get("replay_command"),
        "scenario_id": scenario_id,
        "series_id": point.series_id,
        "signed_error": signed_error,
        "source_locator": point.source_locator,
        "status": status,
        "trace_path": scenario_metadata.get("trace_path"),
        "unit": point.unit,
    }


def _series(rows):
    grouped = {}
    for row in rows:
        grouped.setdefault(row["series_id"], []).append(row)
    summaries = []
    for series_id in sorted(grouped):
        series_rows = grouped[series_id]
        errors = [
            row["signed_error"] for row in series_rows
            if row["signed_error"] is not None
        ]
        summaries.append({
            "count": len(series_rows),
            "maximum_absolute_error": (
                max(abs(error) for error in errors) if errors else None),
            "pass_rate": (
                sum(row["status"] == "PASSED" for row in series_rows)
                / len(series_rows)),
            "rmse": (
                math.sqrt(sum(error * error for error in errors) / len(errors))
                if errors else None),
            "series_id": series_id,
        })
    return summaries


def _partition(rows):
    statuses = {}
    for row in rows:
        statuses[row["status"]] = statuses.get(row["status"], 0) + 1
    return {
        "points": rows,
        "series": _series(rows),
        "summary": {
            "passed": statuses.get("PASSED", 0),
            "points": len(rows),
            "statuses": dict(sorted(statuses.items())),
        },
    }


def _key_document(key):
    return {
        "case_id": key.case_id,
        "code": key.code,
        "dataset_id": key.dataset_id,
        "metric": key.metric,
    }


def _accounting_document(accounting):
    fields = (
        "known_model_mismatches",
        "new_model_mismatches",
        "missing_model_mismatches",
        "known_limitations",
        "new_limitations",
        "missing_limitations",
        "unallowlistable_failures",
    )
    return {
        field: [_key_document(key) for key in sorted(getattr(accounting, field))]
        for field in fields
    }


def _csv_value(value):
    if value is None:
        return ""
    if isinstance(value, (dict, list)):
        value = json.dumps(value, ensure_ascii=False, sort_keys=True, allow_nan=False)
    if isinstance(value, str) and value.startswith(("=", "+", "-", "@")):
        return "'" + value
    return value


def _csv_row(row):
    lower, upper = row["acceptance_interval"]
    flattened = dict(row)
    del flattened["acceptance_interval"]
    flattened["acceptance_lower"] = lower
    flattened["acceptance_upper"] = upper
    return {field: _csv_value(flattened.get(field)) for field in _CSV_FIELDS}


def _markdown_table(partition, rows):
    lines = [
        f"## {partition}",
        "",
        "| Point | Case | Metric | Prediction | Experiment | Interval | Status |",
        "|---|---|---|---:|---:|---|---|",
    ]
    for row in rows:
        prediction = "-" if row["prediction"] is None else str(row["prediction"])
        lower, upper = row["acceptance_interval"]
        lines.append(
            f"| {row['point_id']} | {row['case_id']} | {row['metric']} | "
            f"{prediction} | {row['experimental_value']} | [{lower}, {upper}] | "
            f"{row['status']} |")
    if not rows:
        lines.append("| - | - | - | - | - | - | - |")
    lines.append("")
    return lines


def _markdown_accounting(accounting_document):
    labels = (
        ("known_model_mismatches", "Known model mismatches"),
        ("new_model_mismatches", "New model mismatches"),
        ("missing_model_mismatches", "Missing model mismatches"),
        ("known_limitations", "Known reference limitations"),
        ("new_limitations", "New reference limitations"),
        ("missing_limitations", "Missing reference limitations"),
        ("unallowlistable_failures", "Unallowlistable failures"),
    )
    lines = ["## Failure accounting", ""]
    for field, label in labels:
        values = accounting_document[field]
        rendered = ", ".join(
            f"{item['dataset_id']}:{item['case_id']}:{item['code']}:{item['metric']}"
            for item in values) or "none"
        lines.append(f"- {label}: {rendered}")
    lines.append("")
    return lines


def write_reference_reports(cases, results, accounting, output_directory, metadata):
    output_directory = Path(output_directory)
    output_directory.mkdir(parents=True, exist_ok=True)
    result_map = {}
    for result in results:
        if result.scenario_id in result_map:
            raise ValueError(f"duplicate scenario result: {result.scenario_id}")
        result_map[result.scenario_id] = result

    rows = []
    for case in sorted(cases, key=lambda item: (item.partition, item.case_id)):
        scenario_id = _scenario_id(case)
        result = result_map.get(scenario_id)
        for point in sorted(case.points, key=lambda item: item.point_id):
            rows.append(_point_row(case, point, result, accounting, metadata))
    rows.sort(key=lambda row: (
        row["partition"], row["dataset_id"], row["case_id"], row["point_id"]))
    partition_rows = {
        partition: [row for row in rows if row["partition"] == partition]
        for partition in _PARTITIONS
    }
    accounting_payload = _accounting_document(accounting)
    payload = {
        "accounting": accounting_payload,
        "metadata": metadata,
        "partitions": {
            partition: _partition(partition_rows[partition])
            for partition in _PARTITIONS
        },
    }

    json_path = output_directory / "reference_report.json"
    json_path.write_text(_json(payload), encoding="utf-8")

    csv_path = output_directory / "reference_points.csv"
    with csv_path.open("w", encoding="utf-8", newline="") as output:
        writer = csv.DictWriter(output, fieldnames=_CSV_FIELDS)
        writer.writeheader()
        writer.writerows(_csv_row(row) for row in rows)

    markdown_lines = [
        "# Reference Physics Validation Report",
        "",
        f"Build: {metadata.get('build_id', '-')}",
        "",
    ]
    for partition in _PARTITIONS:
        markdown_lines.extend(_markdown_table(partition, partition_rows[partition]))
    markdown_lines.extend(_markdown_accounting(accounting_payload))
    markdown_path = output_directory / "reference_report.md"
    markdown_path.write_text("\n".join(markdown_lines), encoding="utf-8")
    return json_path, csv_path, markdown_path
