import hashlib
import json
from pathlib import Path


REQUIRED_EMPTY_RECEIPT_FIELDS = (
    "unallowlistable_failures",
    "new_model_mismatches",
    "new_limitations",
    "missing_expected_failures",
)


def _validate_receipt(path):
    path = Path(path)
    receipt = json.loads(path.read_text(encoding="utf-8"))
    failures = []
    if receipt.get("result") != "PASSED_OR_ACCOUNTED":
        failures.append(f"receipt did not pass: {path.as_posix()}")
    for field in REQUIRED_EMPTY_RECEIPT_FIELDS:
        if receipt.get(field) != []:
            failures.append(
                f"receipt missing or non-empty accounting field {field}: "
                f"{path.as_posix()}"
            )
    return failures


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
    if document.get("component_integration_base") not in ids:
        failures.append("component integration base is not inventoried")
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
        for receipt in candidate.get("receipts", []):
            target = root / receipt["path"]
            if target.is_file():
                failures.extend(_validate_receipt(target))
        if candidate.get("validation_disposition") != "passed":
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


def validate_golden_registry(path, matrix_path, root):
    path, matrix_path, root = Path(path), Path(matrix_path), Path(root)
    registry = json.loads(path.read_text(encoding="utf-8"))
    matrix = json.loads(matrix_path.read_text(encoding="utf-8"))
    failures = []
    expected = {case["id"]: case["evidence_label"] for case in matrix["cases"]}
    entries = registry.get("entries", [])
    if registry.get("schema_version") != 1 or len(entries) != len(expected):
        failures.append("golden registry does not match matrix size")
    actual = {entry.get("case_id"): entry for entry in entries}
    if set(actual) != set(expected):
        failures.append("golden registry case set differs from matrix")
    for case_id, label in expected.items():
        entry = actual.get(case_id, {})
        if entry.get("label") != label:
            failures.append(f"golden evidence label changed: {case_id}")
        artifact = root / entry.get("artifact", "")
        if not artifact.is_file():
            failures.append(f"golden artifact missing: {case_id}")
            continue
        if hashlib.sha256(artifact.read_bytes()).hexdigest() != entry.get("artifact_sha256"):
            failures.append(f"golden artifact hash mismatch: {case_id}")
        if label == "reality_golden":
            point_id = entry.get("validated_point_id")
            if not point_id or '"status": "PASSED"' not in artifact.read_text(encoding="utf-8") \
                    or point_id not in artifact.read_text(encoding="utf-8"):
                failures.append(f"reality golden lacks a passed validation point: {case_id}")
        if label == "behavior_snapshot" and entry.get("validated_point_id") is not None:
            failures.append(f"behavior snapshot claims validation: {case_id}")
    return failures


def validate_release_manifest(
        path, root, executable=None, head_revision=None,
        is_ancestor=None, executable_profile_id=None):
    path, root = Path(path), Path(root)
    release = json.loads(path.read_text(encoding="utf-8"))
    failures = []
    schema_version = release.get("schema_version")
    if schema_version == 1:
        if release.get("status") != "PASSED_WITH_DECLARED_LIMITATIONS":
            failures.append("release status or schema is invalid")
    elif schema_version == 2:
        if release.get("status") != "PASSED":
            failures.append("v2 release status must be PASSED")
    else:
        failures.append("release status or schema is invalid")
    source_revision = release.get("source_revision", "")
    if (len(source_revision) != 40 or
            any(character not in "0123456789abcdef" for character in source_revision)):
        failures.append("release source revision is not immutable")
    artifacts = list(release.get("inputs", [])) + [release.get("profile", {})]
    for artifact in artifacts:
        target = root / artifact.get("path", "")
        if not target.is_file() or hashlib.sha256(target.read_bytes()).hexdigest() != \
                artifact.get("sha256"):
            failures.append(f"release artifact mismatch: {artifact.get('path')}")
    if executable is not None and hashlib.sha256(Path(executable).read_bytes()).hexdigest() != \
            release.get("executable_sha256"):
        failures.append("release executable hash mismatch")
    if schema_version == 2:
        if head_revision is None or is_ancestor is None:
            failures.append("release source ancestry was not verified")
        elif not is_ancestor(source_revision, head_revision):
            failures.append("release source revision is not an ancestor of HEAD")
        frozen_profile_id = release.get("profile", {}).get("id")
        if executable_profile_id is None:
            failures.append("production default profile was not verified")
        elif executable_profile_id != frozen_profile_id:
            failures.append("production default profile differs from frozen profile")
    return failures
