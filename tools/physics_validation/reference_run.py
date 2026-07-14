import argparse
import json
import shlex
from pathlib import Path

from .analyzer import (
    Failure,
    INTEGRATION_MISMATCH,
    NUMERICAL_FAILURE,
    ScenarioResult,
    analyze_scenario,
    compare_traces,
)
from .reference_accounting import reconcile_reference_failures
from .partition_run import load_reference_inputs
from .reference_report import write_reference_reports
from .run import _build_id, _execute_once


def _write_json(path, document):
    path.write_text(
        json.dumps(
            document, ensure_ascii=False, indent=2, sort_keys=True, allow_nan=False) + "\n",
        encoding="utf-8")


def _validate_trace(scenario, frames):
    if not isinstance(frames, list):
        raise RuntimeError("process execution did not return a trace array")
    expected_count = scenario["simulation"]["ticks"]
    if len(frames) != expected_count:
        raise RuntimeError(
            f"expected {expected_count} trace frames, received {len(frames)}")
    for expected_tick, frame in enumerate(frames, start=1):
        if not isinstance(frame, dict) or frame.get("tick") != expected_tick:
            received = frame.get("tick") if isinstance(frame, dict) else None
            raise RuntimeError(
                f"trace skipped tick {expected_tick}; received {received}")


def _integration_result(scenario, error):
    return ScenarioResult(
        scenario.get("id", "unknown"),
        False,
        scenario.get("evidence", {}).get("grade", "C"),
        {},
        (Failure(
            INTEGRATION_MISMATCH,
            "process_execution",
            str(error),
            "complete contiguous trace",
            None,
        ),),
    )


def _limitation_result(dataset_id, limitation):
    return ScenarioResult(
        f"{dataset_id}__{limitation.case_id}",
        False,
        "B",
        {},
        (Failure(
            "REFERENCE_LIMITATION",
            limitation.metric,
            limitation.missing_evidence,
            limitation.resolution_condition,
            None,
        ),),
    )


def _append_failure(result, failure):
    return ScenarioResult(
        result.scenario_id,
        False,
        result.evidence_grade,
        result.metrics,
        result.failures + (failure,),
    )


def _integration_failure(metric, error):
    return Failure(
        INTEGRATION_MISMATCH,
        metric,
        str(error),
        "successful reference validation integration",
        None,
    )


def _selected_manifest(path, selected_case_ids):
    document = json.loads(Path(path).read_text(encoding="utf-8"))
    if selected_case_ids is None:
        return document
    result = dict(document)
    result["failures"] = [
        item for item in document.get("failures", [])
        if item.get("case_id") in selected_case_ids
    ]
    return result


def _select_cases(cases, case_ids):
    if case_ids is None:
        return tuple(cases), None
    requested = tuple(case_ids)
    if len(requested) != len(set(requested)):
        raise ValueError("duplicate case filter")
    available = {case.case_id for case in cases}
    unknown = set(requested) - available
    if unknown:
        raise ValueError(f"unknown case filter: {sorted(unknown)}")
    requested_set = set(requested)
    return tuple(case for case in cases if case.case_id in requested_set), requested_set


def _replay_command(executable, package, output, case_id):
    arguments = (
        "python3",
        "-m",
        "tools.physics_validation.reference_run",
        "--executable",
        str(executable),
        "--package",
        str(package),
        "--output",
        str(output),
        "--case",
        case_id,
    )
    return " ".join(shlex.quote(argument) for argument in arguments)


def _run_loaded_reference_validation(
        executable, package_path, output, loaded, execute_once=None, case_ids=None,
        metadata_labels=None):
    executor = execute_once or _execute_once
    reference_package = loaded.reference_package
    dataset_id = loaded.dataset_id
    dataset_version = loaded.dataset_version
    points = loaded.points
    adaptation = loaded.adaptation
    cases, selected_case_ids = _select_cases(adaptation.cases, case_ids)

    trace_directory = output / "traces"
    provenance_directory = output / "provenance"
    trace_directory.mkdir(parents=True, exist_ok=True)
    provenance_directory.mkdir(parents=True, exist_ok=True)

    results = []
    scenario_metadata = {}
    for case in cases:
        scenario = json.loads(case.scenario_json)
        scenario_id = scenario["id"]
        provenance_path = provenance_directory / f"{scenario_id}.json"
        provenance_path.write_text(case.provenance_json, encoding="utf-8")
        trace_path = None
        try:
            first = executor(executable, scenario)
            _validate_trace(scenario, first)
            second = executor(executable, scenario)
            _validate_trace(scenario, second)
        except Exception as error:
            result = _integration_result(scenario, error)
        else:
            result = analyze_scenario(scenario, first)
            has_numerical_failure = any(
                failure.code == NUMERICAL_FAILURE for failure in result.failures)
            try:
                determinism_failure = compare_traces(scenario_id, first, second)
            except Exception as error:
                if not has_numerical_failure:
                    result = _append_failure(
                        result, _integration_failure("trace_comparison", error))
            else:
                if determinism_failure is not None:
                    result = _append_failure(result, determinism_failure)
            candidate_trace_path = trace_directory / f"{scenario_id}.json"
            try:
                _write_json(candidate_trace_path, first)
            except Exception as error:
                if not has_numerical_failure:
                    result = _append_failure(
                        result, _integration_failure("trace_artifact", error))
            else:
                trace_path = candidate_trace_path
        results.append(result)
        scenario_metadata[scenario_id] = {
            "provenance_path": str(provenance_path),
            "replay_command": _replay_command(
                executable, package_path, output, case.case_id),
            "trace_path": str(trace_path) if trace_path else None,
        }

    if selected_case_ids is None:
        results.extend(
            _limitation_result(dataset_id, limitation)
            for limitation in adaptation.limitations
        )

    model_manifest = _selected_manifest(
        reference_package.files["expected_model_mismatches"], selected_case_ids)
    limitation_manifest = _selected_manifest(
        reference_package.files["expected_reference_limitations"], selected_case_ids)
    accounting = reconcile_reference_failures(
        results, model_manifest, limitation_manifest, dataset_id)
    metadata = {
        "build_id": _build_id(executable),
        "dataset_id": dataset_id,
        "dataset_version": dataset_version,
        "executable": str(executable),
        "package": str(package_path),
        "package_hashes": {
            item["id"]: item["sha256"]
            for item in reference_package.manifest["files"]
        },
        "scenarios": scenario_metadata,
    }
    if metadata_labels is not None:
        overlap = set(metadata) & set(metadata_labels)
        if overlap:
            raise ValueError(f"metadata labels cannot replace fields: {sorted(overlap)}")
        metadata.update(metadata_labels)
    write_reference_reports(
        cases,
        results,
        accounting,
        output,
        metadata,
        points=points,
        limitations=adaptation.limitations,
    )
    return 0 if accounting.ci_passed else 1


def run_reference_validation(executable, package, output, execute_once=None, case_ids=None):
    executable = Path(executable).resolve()
    package_path = Path(package).resolve()
    output = Path(output).resolve()
    loaded = load_reference_inputs(package_path)
    return _run_loaded_reference_validation(
        executable,
        package_path,
        output,
        loaded,
        execute_once=execute_once,
        case_ids=case_ids,
    )


def main(argv=None):
    parser = argparse.ArgumentParser(
        description="Run offline BilliardGL reference validation")
    parser.add_argument("--executable", required=True, type=Path)
    parser.add_argument("--package", required=True, type=Path)
    parser.add_argument("--output", required=True, type=Path)
    parser.add_argument("--case", action="append", dest="case_ids")
    arguments = parser.parse_args(argv)
    return run_reference_validation(
        arguments.executable,
        arguments.package,
        arguments.output,
        case_ids=arguments.case_ids,
    )


if __name__ == "__main__":
    raise SystemExit(main())
