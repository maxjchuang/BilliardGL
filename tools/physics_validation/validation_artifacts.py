import hashlib
import json
from pathlib import Path


def _sha256(path):
    return hashlib.sha256(Path(path).read_bytes()).hexdigest()


def build_validation_artifact_manifest(root, inventory_path):
    root = Path(root).resolve()
    inventory = json.loads(Path(inventory_path).read_text(encoding="utf-8"))
    files = []
    for candidate in inventory["candidates"]:
        for receipt in candidate["receipts"]:
            validation = (root / receipt["path"]).parent
            report = json.loads((validation / "reference_report.json").read_text(
                encoding="utf-8"))
            dataset_id = report["metadata"]["dataset_id"]
            for kind in ("provenance", "traces"):
                for path in sorted((validation / kind).glob("*.json")):
                    files.append({
                        "candidate_id": candidate["id"],
                        "dataset_id": dataset_id,
                        "kind": kind,
                        "path": path.relative_to(root).as_posix(),
                        "sha256": _sha256(path),
                    })
    return {"schema_version": 1, "files": files}


def validate_validation_artifact_manifest(path, root, inventory_path):
    try:
        committed = json.loads(Path(path).read_text(encoding="utf-8"))
        expected = build_validation_artifact_manifest(root, inventory_path)
    except (OSError, KeyError, TypeError, ValueError, json.JSONDecodeError) as error:
        return [f"validation artifact manifest is unreadable: {error}"]
    if committed != expected:
        return ["validation artifact manifest is stale or incomplete"]
    return []


def _repository_file(root, logical_path):
    root = Path(root).resolve()
    if not isinstance(logical_path, str) or not logical_path or Path(logical_path).is_absolute():
        raise ValueError(f"unsafe artifact path: {logical_path}")
    target = root / logical_path
    if target.is_symlink():
        raise ValueError(f"artifact symlink is forbidden: {logical_path}")
    try:
        target.resolve().relative_to(root)
    except (OSError, ValueError) as error:
        raise ValueError(f"unsafe artifact path: {logical_path}") from error
    if not target.is_file():
        raise ValueError(f"artifact is missing: {logical_path}")
    return target


def build_declared_artifact_manifest(root, freeze_path):
    root = Path(root).resolve()
    freeze_path = Path(freeze_path)
    freeze = json.loads(freeze_path.read_text(encoding="utf-8"))
    if freeze.get("schema_version") != 2 or not isinstance(freeze.get("artifacts"), list):
        raise ValueError("declared artifact inventory requires freeze schema version 2")
    artifacts = []
    paths = []
    for entry in freeze["artifacts"]:
        if not isinstance(entry, dict) or set(entry) != {"path", "role", "sha256"}:
            raise ValueError("freeze artifact entry has invalid keys")
        paths.append(entry["path"])
        target = _repository_file(root, entry["path"])
        artifacts.append({
            "path": entry["path"],
            "role": entry["role"],
            "sha256": _sha256(target),
        })
    if len(paths) != len(set(paths)):
        raise ValueError("freeze artifact paths must be unique")
    artifacts.sort(key=lambda item: (item["role"], item["path"]))
    return {
        "schema_version": 2,
        "freeze_sha256": _sha256(freeze_path),
        "artifacts": artifacts,
    }


def validate_declared_artifact_manifest(path, root, freeze_path):
    try:
        committed = json.loads(Path(path).read_text(encoding="utf-8"))
        expected = build_declared_artifact_manifest(root, freeze_path)
    except (OSError, KeyError, TypeError, ValueError, json.JSONDecodeError) as error:
        return [f"declared artifact manifest is unreadable: {error}"]
    if committed != expected:
        return ["declared artifact manifest is stale or incomplete"]
    return []
