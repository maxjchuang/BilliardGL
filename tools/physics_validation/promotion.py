import hashlib
import json
from pathlib import Path


def validate_promotion_manifest(path, root):
    path, root = Path(path), Path(root)
    document = json.loads(path.read_text(encoding="utf-8"))
    failures = []
    candidates = document.get("candidates", [])
    if document.get("schema_version") != 1 or len(candidates) != 6:
        failures.append("promotion inventory must contain six version-1 candidates")
    if [item.get("theme") for item in candidates] != list(range(1, 7)):
        failures.append("candidate themes must be ordered 1 through 6")
    ids = {item.get("id") for item in candidates}
    if document.get("production_default") not in ids:
        failures.append("production default is not inventoried")
    for candidate in candidates:
        if candidate.get("evidence_grade") not in {"A", "B", "C", "mixed"}:
            failures.append(f"invalid evidence grade: {candidate.get('id')}")
        if candidate.get("real_world_claim") != "bounded" or not candidate.get("limitations"):
            failures.append(f"overstated or missing limitations: {candidate.get('id')}")
        artifacts = [candidate.get("profile", {}), candidate.get("freeze", {})]
        artifacts += candidate.get("receipts", [])
        if not candidate.get("receipts"):
            failures.append(f"unvalidated candidate: {candidate.get('id')}")
        for artifact in artifacts:
            target = root / artifact.get("path", "")
            if not target.is_file():
                failures.append(f"missing artifact: {artifact.get('path')}")
                continue
            actual = hashlib.sha256(target.read_bytes()).hexdigest()
            if actual != artifact.get("sha256"):
                failures.append(f"artifact hash mismatch: {artifact.get('path')}")
        receipt_results = []
        for receipt in candidate.get("receipts", []):
            target = root / receipt["path"]
            if target.is_file():
                result = json.loads(target.read_text(encoding="utf-8")).get("result")
                receipt_results.append(result)
                if result not in {"PASSED_OR_ACCOUNTED", "FAILED"}:
                    failures.append(f"receipt has invalid result: {receipt['path']}")
        disposition = candidate.get("validation_disposition")
        expected_disposition = "limitations_preserved" \
            if "FAILED" in receipt_results else "passed"
        if disposition != expected_disposition:
            failures.append(f"validation disposition mismatch: {candidate.get('id')}")
    return failures


def validate_full_game_matrix(path, root):
    path, root = Path(path), Path(root)
    document = json.loads(path.read_text(encoding="utf-8"))
    failures = []
    cases = document.get("cases", [])
    required = set(document.get("required_coverage", []))
    covered = {value for case in cases for value in case.get("coverage", [])}
    if document.get("schema_version") != 1 or not cases:
        failures.append("full-game matrix must use schema version 1")
    if covered != required:
        failures.append(f"coverage mismatch: missing={sorted(required-covered)} extra={sorted(covered-required)}")
    ids = [case.get("id") for case in cases]
    if len(ids) != len(set(ids)):
        failures.append("case IDs must be unique")
    for case in cases:
        label = case.get("evidence_label")
        if label not in {"reality_golden", "analytic_golden", "behavior_snapshot"}:
            failures.append(f"invalid evidence label: {case.get('id')}")
        replay = case.get("replay", "")
        if label in {"reality_golden", "analytic_golden"} and " --case " not in replay:
            target = root / replay
            if not target.is_file():
                failures.append(f"golden replay artifact is missing: {case.get('id')}")
        if not isinstance(case.get("seed"), int):
            failures.append(f"case seed is not fixed: {case.get('id')}")
    return failures
