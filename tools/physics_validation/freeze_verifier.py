import hashlib
import json
import re
from pathlib import Path


REQUIRED_ARTIFACT_ROLES = {
    "profile", "executable", "calibration_report", "source_manifest",
    "source_numeric_input", "split", "metric_contract", "receipt",
    "trace", "provenance", "full_game_matrix", "performance_budget",
}
_SHA256 = re.compile(r"[0-9a-f]{64}")
_REVISION = re.compile(r"[0-9a-f]{40}")


def _sha256(path):
    return hashlib.sha256(Path(path).read_bytes()).hexdigest()


def _safe_relative_path(root, value):
    if not isinstance(value, str) or not value or Path(value).is_absolute():
        return None
    candidate = root / value
    try:
        candidate.resolve().relative_to(root.resolve())
    except (OSError, ValueError):
        return None
    if candidate.is_symlink():
        return None
    return candidate


def validate_freeze(freeze_path, root, executable=None):
    root = Path(root)
    try:
        freeze = json.loads(Path(freeze_path).read_text(encoding="utf-8"))
    except (OSError, UnicodeError, json.JSONDecodeError) as error:
        return [f"freeze is unreadable: {error}"]
    failures = []
    if freeze.get("schema_version") != 2:
        failures.append("freeze schema version must be 2")
    revision = freeze.get("source_revision")
    if not isinstance(revision, str) or _REVISION.fullmatch(revision) is None:
        failures.append("freeze source revision is invalid")
    executable_digest = freeze.get("executable_sha256")
    if not isinstance(executable_digest, str) or _SHA256.fullmatch(executable_digest) is None:
        failures.append("freeze executable SHA-256 is invalid")
    elif executable is not None:
        try:
            if _sha256(executable) != executable_digest:
                failures.append("freeze executable hash mismatch")
        except OSError:
            failures.append("freeze executable is unreadable")

    artifacts = freeze.get("artifacts")
    if not isinstance(artifacts, list):
        artifacts = []
        failures.append("freeze artifacts must be a list")
    roles = {value.get("role") for value in artifacts if isinstance(value, dict)}
    missing_roles = REQUIRED_ARTIFACT_ROLES - roles
    if missing_roles:
        failures.append(f"freeze roles missing: {sorted(missing_roles)}")

    declared_paths = []
    for artifact in artifacts:
        if not isinstance(artifact, dict) or set(artifact) != {"path", "role", "sha256"}:
            failures.append("freeze artifact entry has invalid keys")
            continue
        logical_path = artifact["path"]
        declared_paths.append(logical_path)
        target = _safe_relative_path(root, logical_path)
        if target is None:
            failures.append(f"unsafe artifact path: {logical_path}")
            continue
        digest = artifact["sha256"]
        if not isinstance(digest, str) or _SHA256.fullmatch(digest) is None:
            failures.append(f"invalid artifact SHA-256: {logical_path}")
        elif not target.is_file() or _sha256(target) != digest:
            failures.append(f"freeze artifact mismatch: {logical_path}")
    duplicate_paths = sorted({
        value for value in declared_paths if declared_paths.count(value) > 1
    })
    for value in duplicate_paths:
        failures.append(f"duplicate freeze artifact path: {value}")

    declared = set(declared_paths)
    roots = freeze.get("artifact_roots")
    if not isinstance(roots, list) or not roots:
        failures.append("freeze artifact_roots must be a nonempty list")
        roots = []
    for logical_root in roots:
        artifact_root = _safe_relative_path(root, logical_root)
        if artifact_root is None or not artifact_root.is_dir():
            failures.append(f"unsafe artifact root: {logical_root}")
            continue
        for path in sorted(artifact_root.rglob("*")):
            if path.is_symlink():
                failures.append(f"artifact symlink is forbidden: {path.relative_to(root)}")
            elif path.is_file():
                logical_path = path.relative_to(root).as_posix()
                if logical_path not in declared:
                    failures.append(f"undeclared artifact: {logical_path}")
    return failures
