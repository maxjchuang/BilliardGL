import json
from dataclasses import asdict
from pathlib import Path

from .analyzer import REFERENCE_LIMITATION


def _failure_tuples(result):
    return {(result.scenario_id, failure.code, failure.metric) for failure in result.failures}


def _status(result, matching):
    if result.passed:
        return "PASSED"
    if result.failures and all(failure.code == REFERENCE_LIMITATION for failure in result.failures):
        return "REFERENCE LIMITED"
    if _failure_tuples(result) and _failure_tuples(result) <= matching.known:
        return "FAILED (KNOWN)"
    return "FAILED (NEW)"


def write_reports(results, matching, output_directory, metadata):
    output_directory = Path(output_directory)
    output_directory.mkdir(parents=True, exist_ok=True)
    ordered = sorted(results, key=lambda result: result.scenario_id)
    statuses = {result.scenario_id: _status(result, matching) for result in ordered}
    summary = {
        "passed": sum(status == "PASSED" for status in statuses.values()),
        "failed_known": sum(status == "FAILED (KNOWN)" for status in statuses.values()),
        "failed_new": sum(status == "FAILED (NEW)" for status in statuses.values()),
        "reference_limited": sum(status == "REFERENCE LIMITED" for status in statuses.values()),
    }
    payload = {
        "metadata": metadata,
        "summary": summary,
        "known_failure_match": {
            "known": sorted(list(item) for item in matching.known),
            "new": sorted(list(item) for item in matching.new),
            "missing": sorted(list(item) for item in matching.missing),
        },
        "scenarios": [dict(
            asdict(result),
            status=statuses[result.scenario_id],
            **metadata.get("scenarios", {}).get(result.scenario_id, {}),
        ) for result in ordered],
    }

    json_path = output_directory / "report.json"
    json_path.write_text(
        json.dumps(payload, ensure_ascii=False, indent=2, sort_keys=True) + "\n",
        encoding="utf-8")

    lines = [
        "# Physics Validation Report",
        "",
        f"Passed: {summary['passed']} | Failed known: {summary['failed_known']} | "
        f"Failed new: {summary['failed_new']} | Reference limited: {summary['reference_limited']}",
        "",
        "| Scenario | Status | Evidence | Failures |",
        "|---|---|---|---|",
    ]
    for result in ordered:
        failures = ", ".join(f"{failure.code}:{failure.metric}" for failure in result.failures) or "-"
        lines.append(
            f"| {result.scenario_id} | {statuses[result.scenario_id]} | "
            f"{result.evidence_grade} | {failures} |")
    markdown_path = output_directory / "report.md"
    markdown_path.write_text("\n".join(lines) + "\n", encoding="utf-8")
    return json_path, markdown_path
