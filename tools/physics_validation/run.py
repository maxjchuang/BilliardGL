import argparse
import json
import sys
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


def _read_json(path):
    return json.loads(Path(path).read_text(encoding="utf-8"))


def _known_failure_set(path):
    manifest = _read_json(path)
    if manifest.get("schema_version") != 1:
        raise ValueError("known-failure manifest must use schema version 1")
    return {
        (item["scenario_id"], item["code"], item["metric"])
        for item in manifest.get("failures", [])
    }


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


def _execute_once(executable, scenario):
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
        return frames


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
    trace_paths = {}
    for scenario_path in sorted(scenarios.glob("*.json")):
        scenario = _read_json(scenario_path)
        try:
            first = _execute_once(executable, scenario)
            second = _execute_once(executable, scenario)
            result = analyze_scenario(scenario, first)
            determinism_failure = compare_traces(scenario["id"], first, second)
            if determinism_failure is not None:
                result = ScenarioResult(
                    result.scenario_id,
                    False,
                    result.evidence_grade,
                    result.metrics,
                    result.failures + (determinism_failure,),
                )
            trace_path = trace_directory / f"{scenario['id']}.json"
            trace_path.write_text(
                json.dumps(first, ensure_ascii=False, indent=2, sort_keys=True,
                           allow_nan=False) + "\n",
                encoding="utf-8")
            trace_paths[scenario["id"]] = str(trace_path)
        except Exception as error:
            result = _integration_failure(scenario, error)
        results.append(result)

    expected = _known_failure_set(known_failures)
    matching = match_known_failures(results, expected)
    write_reports(
        results,
        matching,
        output,
        {"executable": str(executable), "trace_paths": trace_paths},
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
