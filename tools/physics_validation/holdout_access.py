import hashlib
import json
from pathlib import Path


def _dataset_versions(document):
    result = set()
    if isinstance(document, dict):
        if "dataset_id" in document and "dataset_version" in document:
            result.add((document["dataset_id"], document["dataset_version"]))
        for value in document.values():
            result.update(_dataset_versions(value))
    elif isinstance(document, list):
        for value in document:
            result.update(_dataset_versions(value))
    return result


def validate_candidate_holdout_access(root, freeze_path, package_path=None):
    root = Path(root).resolve()
    freeze_path = Path(freeze_path).resolve()
    failures = []
    try:
        freeze_path.relative_to(root)
    except ValueError:
        return ["candidate freeze must be committed inside the repository"]
    if not freeze_path.is_file():
        return ["candidate freeze does not exist"]

    inventory_path = root / "physics_models/promotion/phase3_candidates_v1.json"
    inventory = json.loads(inventory_path.read_text(encoding="utf-8"))
    relative = freeze_path.relative_to(root).as_posix()
    digest = hashlib.sha256(freeze_path.read_bytes()).hexdigest()
    spent_datasets = set()
    for candidate in inventory.get("candidates", []):
        freeze = candidate.get("freeze", {})
        candidate_freeze = root / freeze.get("path", "")
        if candidate_freeze.is_file():
            spent_datasets.update(_dataset_versions(json.loads(
                candidate_freeze.read_text(encoding="utf-8"))))
        if freeze.get("path") == relative or freeze.get("sha256") == digest:
            failures.append(
                f"candidate HOLDOUT is already consumed and promoted: {candidate.get('id')}")
    if (freeze_path.parent / "validation/validation_receipt.json").exists():
        failures.append("candidate already has a committed validation receipt")
    if package_path is not None:
        manifest = Path(package_path).resolve() / "manifest.json"
        if not manifest.is_file():
            failures.append("candidate reference package manifest does not exist")
        else:
            package = json.loads(manifest.read_text(encoding="utf-8"))
            identity = (package.get("dataset_id"), package.get("dataset_version"))
            if identity in spent_datasets:
                failures.append(
                    f"reference package HOLDOUT is already consumed: {identity[0]} {identity[1]}")
    return failures
