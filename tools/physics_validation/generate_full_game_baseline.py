import argparse
import csv
import hashlib
import io
import json
from pathlib import Path


CSV_FIELDS = (
    "case_id", "seed", "passed", "frame_count", "wall_seconds",
    "peak_rss_bytes", "maximum_penetration_cm", "maximum_residual_cm_s",
    "dropped_trace_frames", "step_failures", "deterministic_hash",
    "physics_profile_id", "summary_sha256", "trace_sha256", "index_sha256",
)


def sha256(path):
    return hashlib.sha256(Path(path).read_bytes()).hexdigest()


def generate_documents(root, freeze_path, matrix_path, budget_path,
                       output_root, runner_path=None, runner_sha256=None):
    root = Path(root).resolve()
    freeze = json.loads(Path(freeze_path).read_text(encoding="utf-8"))
    matrix = json.loads(Path(matrix_path).read_text(encoding="utf-8"))
    expected_profile_id = matrix.get(
        "physics_profile_id", "chinese_pool_full_game_v2")
    cases = {}
    rows = []
    for specification in matrix["cases"]:
        case_id = specification["id"]
        directory = Path(output_root) / case_id
        summary_path = directory / "summary.json"
        trace_path = directory / "trace.json"
        index_path = directory / "index.csv"
        summary = json.loads(summary_path.read_text(encoding="utf-8"))
        trace = json.loads(trace_path.read_text(encoding="utf-8"))
        profile_ids = {frame["physics_profile_id"] for frame in trace["frames"]}
        if profile_ids != {expected_profile_id}:
            raise ValueError(f"unexpected profile IDs for {case_id}: {profile_ids}")
        evidence = {
            "seed": summary["seed"],
            "passed": summary["passed"],
            "frame_count": summary["frame_count"],
            "wall_seconds": summary["wall_seconds"],
            "peak_rss_bytes": summary["peak_rss_bytes"],
            "maximum_penetration_cm": summary["maximum_penetration_cm"],
            "maximum_residual_cm_s": summary["maximum_residual_cm_s"],
            "dropped_trace_frames": summary["dropped_trace_frames"],
            "step_failures": summary["step_failures"],
            "deterministic_hash": summary["deterministic_hash"],
            "physics_profile_id": expected_profile_id,
            "summary_sha256": sha256(summary_path),
            "trace_sha256": sha256(trace_path),
            "index_sha256": sha256(index_path),
        }
        cases[case_id] = evidence
        rows.append({"case_id": case_id, **evidence})
    if runner_sha256 is None:
        if runner_path is None:
            raise ValueError("runner_path or runner_sha256 is required")
        runner_sha256 = sha256(runner_path)
    baseline = {
        "schema_version": 2,
        "candidate_id": freeze["candidate_id"],
        "executable_sha256": freeze["executable_sha256"],
        "runner_source_revision": freeze["source_revision"],
        "runner_sha256": runner_sha256,
        "matrix_sha256": sha256(matrix_path),
        "performance_budget_sha256": sha256(budget_path),
        "matrix_summary_sha256": sha256(Path(output_root) / "matrix_summary.json"),
        "matrix_index_sha256": sha256(Path(output_root) / "index.csv"),
        "total_wall_seconds": sum(value["wall_seconds"] for value in cases.values()),
        "cases": cases,
    }
    stream = io.StringIO(newline="")
    writer = csv.DictWriter(stream, fieldnames=CSV_FIELDS, lineterminator="\n")
    writer.writeheader()
    for row in rows:
        writer.writerow({key: row[key] for key in CSV_FIELDS})
    return (
        json.dumps(baseline, ensure_ascii=False, indent=2, sort_keys=True) + "\n",
        stream.getvalue(),
    )


def main(argv=None):
    parser = argparse.ArgumentParser(
        description="Derive hash-bound Phase 3 full-game evidence")
    parser.add_argument("--root", type=Path, default=Path.cwd())
    parser.add_argument("--freeze", required=True, type=Path)
    parser.add_argument("--matrix", required=True, type=Path)
    parser.add_argument("--budget", required=True, type=Path)
    parser.add_argument("--output-root", required=True, type=Path)
    parser.add_argument("--runner", required=True, type=Path)
    parser.add_argument("--baseline", required=True, type=Path)
    parser.add_argument("--csv", required=True, type=Path)
    arguments = parser.parse_args(argv)
    baseline, csv_text = generate_documents(
        arguments.root, arguments.freeze, arguments.matrix, arguments.budget,
        arguments.output_root, arguments.runner)
    arguments.baseline.write_text(baseline, encoding="utf-8")
    arguments.csv.write_text(csv_text, encoding="utf-8", newline="")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
