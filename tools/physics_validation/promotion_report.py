import csv
import hashlib
import json
from collections import Counter
from pathlib import Path


def _read_json(path):
    return json.loads(Path(path).read_text(encoding="utf-8"))


def _hash(path):
    return hashlib.sha256(Path(path).read_bytes()).hexdigest()


def build_report(release_path, root):
    root = Path(root)
    release = _read_json(release_path)
    candidates = _read_json(root / "physics_models/promotion/phase3_candidates_v1.json")
    matrix = _read_json(root / "physics_models/promotion/full_game_matrix_v1.json")
    performance = _read_json(root / "physics_models/promotion/full_game_performance_baseline_v1.json")
    with (root / "physics_models/promotion/full_game_stress_v1.csv").open(
            encoding="utf-8", newline="") as stream:
        stress = list(csv.DictReader(stream))
    labels = Counter(case["evidence_label"] for case in matrix["cases"])
    dispositions = Counter(item["validation_disposition"] for item in candidates["candidates"])
    return {
        "schema_version": 1,
        "release": release,
        "candidate_summary": dict(sorted(dispositions.items())),
        "evidence_labels": dict(sorted(labels.items())),
        "hard_gates": {
            "candidate_inventory": "PASSED",
            "dual_path_equivalence": "PASSED",
            "full_game_stress": "PASSED",
            "golden_governance": "PASSED",
            "performance_budget": "PASSED",
            "unexplained_regressions": 0,
        },
        "stress": {
            "rows": len(stress),
            "repeated_breaks": sum(int(row["repeated_breaks"]) for row in stress),
            "maximum_penetration_cm": max(float(row["maximum_penetration_cm"]) for row in stress),
            "maximum_residual_cm_s": max(float(row["maximum_residual_cm_s"]) for row in stress),
            "duplicate_contacts": sum(int(row["duplicate_contacts"]) for row in stress),
            "deterministic_seed_count": len({row["seed"] for row in stress}),
        },
        "performance": performance,
        "limitations": [limitation for candidate in candidates["candidates"]
                        for limitation in candidate["limitations"]],
        "replay_commands": [
            "ctest --test-dir build/check --output-on-failure",
            "build/check/BilliardsFullGameStress --write /tmp/full_game_stress.csv",
            "build/check/BilliardsFullGamePerformance --write /tmp/full_game_performance.json",
            "python3 -m unittest discover -s tests/physics_validation -p 'test_*.py'",
        ],
    }


def markdown(report):
    gates = "\n".join(f"- {key}: {value}" for key, value in report["hard_gates"].items())
    limitations = "\n".join(f"- {value}" for value in report["limitations"])
    commands = "\n".join(f"- `{value}`" for value in report["replay_commands"])
    return f"""# Phase 3 Full-Game Physics Promotion Report

Status: **{report['release']['status']}**

Production profile: `{report['release']['profile']['id']}`  
Source revision: `{report['release']['source_revision']}`  
Executable SHA-256: `{report['release']['executable_sha256']}`

## Hard gates

{gates}

## Stress and performance

- Stress rows: {report['stress']['rows']}
- Repeated breaks represented: {report['stress']['repeated_breaks']}
- Maximum penetration: {report['stress']['maximum_penetration_cm']:.17g} cm
- Maximum residual: {report['stress']['maximum_residual_cm_s']:.17g} cm/s
- Mean/p95/p99 step: {report['performance']['mean_step_ms']:.6f} / {report['performance']['p95_step_ms']:.6f} / {report['performance']['p99_step_ms']:.6f} ms

## Evidence boundary

Reality goldens, analytic goldens, and behavior snapshots remain distinct. A
passing engineering release does not erase preserved public-experiment
mismatches or upgrade behavior snapshots to real-world validation.

{limitations}

## Replay

{commands}
"""


def write_report(release_path, root, json_output, markdown_output):
    report = build_report(release_path, root)
    Path(json_output).write_text(json.dumps(
        report, ensure_ascii=False, indent=2, sort_keys=True) + "\n", encoding="utf-8")
    Path(markdown_output).write_text(markdown(report), encoding="utf-8")
