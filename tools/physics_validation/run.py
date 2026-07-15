import argparse
import hashlib
import json
import re
import sys
from dataclasses import dataclass
from pathlib import Path

from .analyzer import (
    Failure,
    INTEGRATION_MISMATCH,
    ScenarioResult,
    analyze_scenario,
    compare_traces,
    match_known_failures,
)
from .report import write_reports


REPO_ROOT = Path(__file__).resolve().parents[2]
E2E_ROOT = REPO_ROOT / "tests/e2e"
if str(E2E_ROOT) not in sys.path:
    sys.path.insert(0, str(E2E_ROOT))

from automation_client import AutomationClient


@dataclass(frozen=True)
class ExecutionEvidence:
    frames: tuple
    protocol_transcript: tuple
    stderr: str
    return_code: int


def _read_json(path):
    return json.loads(Path(path).read_text(encoding="utf-8"))


def _known_failure_set(path, scenario_ids=None):
    manifest = _read_json(path)
    if manifest.get("schema_version") != 1:
        raise ValueError("known-failure manifest must use schema version 1")
    failures = {
        (item["scenario_id"], item["code"], item["metric"])
        for item in manifest.get("failures", [])
    }
    if scenario_ids is not None:
        failures = {item for item in failures if item[0] in scenario_ids}
    return failures


def _scenario_paths(path):
    path = Path(path)
    if path.is_file():
        if path.suffix.lower() != ".json":
            raise ValueError("scenario file must use the .json extension")
        return [path]
    if path.is_dir():
        return sorted(path.glob("*.json"))
    raise ValueError(f"scenario path does not exist: {path}")


def _validated_scenario_id(value):
    if not isinstance(value, str) or re.fullmatch(r"[A-Za-z0-9][A-Za-z0-9_.-]*", value) is None:
        raise ValueError("scenario id must be a safe nonempty filename component")
    return value


def _build_id(executable):
    digest = hashlib.sha256()
    with Path(executable).open("rb") as input_file:
        for block in iter(lambda: input_file.read(1024 * 1024), b""):
            digest.update(block)
    return "sha256:" + digest.hexdigest()


def _permutation_expectation(scenario):
    for expectation in scenario.get("expectations", []):
        if expectation.get("metric") == "permutation_invariance":
            return expectation
    return None


def _permuted_scenario(scenario):
    expectation = _permutation_expectation(scenario)
    if expectation is None:
        return None
    index_map = expectation.get("value", {}).get("index_map")
    if not isinstance(index_map, dict) or not index_map:
        raise ValueError("permutation_invariance requires a nonempty index_map")
    normalized = {int(source): int(target) for source, target in index_map.items()}
    if len(set(normalized.values())) != len(normalized):
        raise ValueError("permutation index_map targets must be unique")
    result = json.loads(json.dumps(scenario))
    result["id"] = scenario["id"] + "__permuted"
    for ball in result.get("balls", []):
        source = ball["index"]
        if source not in normalized:
            raise ValueError(f"permutation index_map is missing ball {source}")
        ball["index"] = normalized[source]
    return result


def _fetch_trace(client):
    frames = []
    after_tick = 0
    while True:
        page = client.physics_trace(after_tick=after_tick, limit=1000)
        if page["dropped_frames"] != 0:
            raise RuntimeError(f"physics trace dropped {page['dropped_frames']} frames")
        page_frames = page["frames"]
        if page_frames:
            expected_tick = after_tick + 1
            for frame in page_frames:
                if frame["tick"] != expected_tick:
                    raise RuntimeError(
                        f"trace skipped tick {expected_tick}; received {frame['tick']}")
                expected_tick += 1
            frames.extend(page_frames)
            after_tick = page_frames[-1]["tick"]
        if not page["has_more"]:
            return frames
        if not page_frames:
            raise RuntimeError("trace pagination reported more data without returning a frame")


def _execute_once_with_evidence(executable, scenario):
    with AutomationClient(str(executable), mode="headless") as client:
        if client.ready.get("event") != "ready":
            raise RuntimeError("automation process did not emit ready")
        if "physics_scenario_v1" not in client.ready.get("capabilities", []):
            raise RuntimeError("automation process does not advertise physics_scenario_v1")
        client.load_physics_scenario(scenario)
        client.start_physics_trace()
        client.step(scenario["simulation"]["ticks"])
        frames = _fetch_trace(client)
        expected_count = scenario["simulation"]["ticks"]
        if len(frames) != expected_count:
            raise RuntimeError(
                f"expected {expected_count} trace frames, received {len(frames)}")
    return ExecutionEvidence(
        tuple(frames), tuple(client.transcript), client.stderr_text,
        client.return_code)


def _execute_once(executable, scenario):
    return list(_execute_once_with_evidence(executable, scenario).frames)


def _integration_failure(scenario, error):
    failure = Failure(
        INTEGRATION_MISMATCH,
        "process_execution",
        str(error),
        "complete contiguous trace",
        None,
    )
    return ScenarioResult(
        scenario.get("id", "unknown"),
        False,
        scenario.get("evidence", {}).get("grade", "C"),
        {},
        (failure,),
    )


def run_validation(executable, scenarios, known_failures, output):
    executable = Path(executable).resolve()
    scenarios = Path(scenarios).resolve()
    output = Path(output).resolve()
    trace_directory = output / "traces"
    trace_directory.mkdir(parents=True, exist_ok=True)

    results = []
    scenario_metadata = {}
    scenario_documents = []
    seen_ids = set()
    for scenario_path in _scenario_paths(scenarios):
        scenario = _read_json(scenario_path)
        scenario_id = _validated_scenario_id(scenario.get("id"))
        if scenario_id in seen_ids:
            raise ValueError(f"duplicate scenario id: {scenario_id}")
        seen_ids.add(scenario_id)
        scenario_documents.append((scenario_path, scenario))

    for scenario_path, scenario in scenario_documents:
        scenario_id = scenario["id"]
        try:
            first = _execute_once(executable, scenario)
            second = _execute_once(executable, scenario)
            comparison = None
            comparison_trace_path = None
            permuted_scenario = _permuted_scenario(scenario)
            if permuted_scenario is not None:
                comparison = _execute_once(executable, permuted_scenario)
                repeated_comparison = _execute_once(executable, permuted_scenario)
                comparison_determinism_failure = compare_traces(
                    permuted_scenario["id"], comparison, repeated_comparison)
            else:
                comparison_determinism_failure = None
            result = analyze_scenario(scenario, first, comparison)
            determinism_failure = compare_traces(scenario["id"], first, second)
            if determinism_failure is not None:
                result = ScenarioResult(
                    result.scenario_id,
                    False,
                    result.evidence_grade,
                    result.metrics,
                    result.failures + (determinism_failure,),
                )
            if comparison_determinism_failure is not None:
                result = ScenarioResult(
                    result.scenario_id,
                    False,
                    result.evidence_grade,
                    result.metrics,
                    result.failures + (comparison_determinism_failure,),
                )
            trace_path = trace_directory / f"{scenario_id}.json"
            trace_path.write_text(
                json.dumps(first, ensure_ascii=False, indent=2, sort_keys=True,
                           allow_nan=False) + "\n",
                encoding="utf-8")
            if comparison is not None:
                comparison_trace_path = trace_directory / f"{scenario_id}__permuted.json"
                comparison_trace_path.write_text(
                    json.dumps(comparison, ensure_ascii=False, indent=2, sort_keys=True,
                               allow_nan=False) + "\n",
                    encoding="utf-8")
            scenario_metadata[scenario_id] = {
                "source_path": str(scenario_path.resolve()),
                "trace_path": str(trace_path),
                "comparison_trace_path": (
                    str(comparison_trace_path) if comparison_trace_path else None),
                "expectations": scenario.get("expectations", []),
                "evidence": scenario.get("evidence", {}),
            }
        except Exception as error:
            result = _integration_failure(scenario, error)
            scenario_metadata[scenario_id] = {
                "source_path": str(scenario_path.resolve()),
                "trace_path": None,
                "comparison_trace_path": None,
                "expectations": scenario.get("expectations", []),
                "evidence": scenario.get("evidence", {}),
            }
        results.append(result)

    expected = _known_failure_set(known_failures, seen_ids)
    matching = match_known_failures(results, expected)
    write_reports(
        results,
        matching,
        output,
        {
            "build_id": _build_id(executable),
            "executable": str(executable),
            "scenario_input": str(scenarios),
            "scenarios": scenario_metadata,
        },
    )
    return 0 if not matching.new and not matching.missing else 1


def main(argv=None):
    parser = argparse.ArgumentParser(description="Run BilliardGL physics validation scenarios")
    parser.add_argument("--executable", required=True, type=Path)
    parser.add_argument("--scenarios", required=True, type=Path)
    parser.add_argument("--known-failures", required=True, type=Path)
    parser.add_argument("--output", required=True, type=Path)
    arguments = parser.parse_args(argv)
    return run_validation(
        arguments.executable,
        arguments.scenarios,
        arguments.known_failures,
        arguments.output,
    )


if __name__ == "__main__":
    raise SystemExit(main())
