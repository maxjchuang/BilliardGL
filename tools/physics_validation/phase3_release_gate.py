import csv
import json
from collections import defaultdict
from pathlib import Path

from .promotion import (
    validate_full_game_matrix,
    validate_golden_registry,
    validate_promotion_manifest,
    validate_release_manifest,
)
from .promotion_report import build_report, markdown
from .validation_artifacts import validate_validation_artifact_manifest


def _json(path):
    return json.loads(Path(path).read_text(encoding="utf-8"))


def _validate_stress(path):
    failures = []
    with Path(path).open(encoding="utf-8", newline="") as stream:
        rows = list(csv.DictReader(stream))
    if len(rows) != 12:
        failures.append("stress evidence must contain the complete 12-row matrix")
    replay_hashes = defaultdict(set)
    for row in rows:
        seed = row.get("seed", "unknown")
        try:
            if row["finite_state"] != "true":
                failures.append(f"stress state is non-finite for seed {seed}")
            if float(row["maximum_penetration_cm"]) > 0.5:
                failures.append(f"stress penetration exceeds its gate for seed {seed}")
            if float(row["maximum_residual_cm_s"]) > 0.001:
                failures.append(f"stress residual exceeds its gate for seed {seed}")
            if int(row["duplicate_contacts"]) != 0:
                failures.append(f"stress has duplicate contacts for seed {seed}")
            if int(row["repeated_breaks"]) != 3:
                failures.append(f"stress repeat count changed for seed {seed}")
            replay_hashes[int(seed)].add(row["replay_hash"])
        except (KeyError, TypeError, ValueError) as error:
            failures.append(f"invalid stress row for seed {seed}: {error}")
    if set(replay_hashes) != {101, 211, 307} or any(
            len(values) != 1 for values in replay_hashes.values()):
        failures.append("stress replay hashes are incomplete or nondeterministic")
    return failures


def _validate_performance(budget_path, baseline_path):
    failures = []
    budget, baseline = _json(budget_path), _json(baseline_path)
    if budget.get("schema_version") != 1 or baseline.get("schema_version") != 1:
        failures.append("performance evidence must use schema version 1")
    if budget.get("ticks") != baseline.get("ticks"):
        failures.append("performance baseline workload differs from its budget")
    for metric in ("mean_step_ms", "p95_step_ms", "p99_step_ms",
                   "peak_rss_bytes", "artifact_bytes_per_tick"):
        try:
            if baseline[metric] > budget[f"{metric}_max"]:
                failures.append(f"performance budget exceeded: {metric}")
        except (KeyError, TypeError):
            failures.append(f"invalid performance evidence: {metric}")
    return failures


def validate_phase3_release(root, release_path=None, executable=None):
    """Validate frozen Phase 3 evidence without executing any reference partition."""
    root = Path(root).resolve()
    promotion = root / "physics_models/promotion"
    release_path = Path(release_path) if release_path else promotion / "phase3_release_v1.json"
    failures = []
    try:
        release = _json(release_path)
        candidates = promotion / "phase3_candidates_v1.json"
        matrix = promotion / "full_game_matrix_v1.json"
        goldens = promotion / "full_game_goldens_v1.json"
        stress = promotion / "full_game_stress_v1.csv"
        budget = promotion / "full_game_performance_budget_v1.json"
        baseline = promotion / "full_game_performance_baseline_v1.json"
        validation_artifacts = promotion / "phase3_validation_artifacts_v1.json"

        failures.extend(validate_promotion_manifest(candidates, root))
        failures.extend(validate_full_game_matrix(matrix, root))
        failures.extend(validate_golden_registry(goldens, matrix, root))
        failures.extend(validate_release_manifest(release_path, root, executable))
        failures.extend(_validate_stress(stress))
        failures.extend(_validate_performance(budget, baseline))
        failures.extend(validate_validation_artifact_manifest(
            validation_artifacts, root, candidates))

        if release.get("unexplained_regressions") != 0:
            failures.append("release has unexplained regressions")
        report = build_report(release_path, root)
        expected_json = json.dumps(
            report, ensure_ascii=False, indent=2, sort_keys=True) + "\n"
        if expected_json.encode("utf-8") != (
                promotion / "phase3_promotion_report_v1.json").read_bytes():
            failures.append("committed JSON promotion report is stale")
        if markdown(report).encode("utf-8") != (
                root / "docs/phase3-physics-promotion-report.md").read_bytes():
            failures.append("committed Markdown promotion report is stale")
    except (OSError, ValueError, KeyError, TypeError, json.JSONDecodeError) as error:
        failures.append(f"release evidence is unreadable: {error}")
    return failures
