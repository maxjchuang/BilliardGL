import argparse
import hashlib
import json
from pathlib import Path

from .data_lifecycle import load_data_lifecycle
from .reference_package import load_reference_package


ROOT = Path(__file__).resolve().parents[2]
DEFAULT_LIFECYCLE = ROOT / "tests/physics_validation/validation_data_status.json"


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


def validate_confirmation_access(root, freeze_path, package_path, ledger_path,
                                 lifecycle_path=DEFAULT_LIFECYCLE):
    root = Path(root).resolve()
    freeze_path = Path(freeze_path).resolve()
    package_path = Path(package_path).resolve()
    ledger_path = Path(ledger_path).resolve()
    failures = []
    try:
        relative_freeze = freeze_path.relative_to(root).as_posix()
    except ValueError:
        return ["confirmation freeze must be inside the repository"]
    if not freeze_path.is_file():
        return ["confirmation freeze does not exist"]
    try:
        freeze = json.loads(freeze_path.read_text(encoding="utf-8"))
    except (OSError, UnicodeError, json.JSONDecodeError):
        return ["confirmation freeze is unreadable"]
    if freeze.get("schema_version") != 2 or \
            freeze.get("candidate_id") != "phase3_integrated_v2":
        failures.append("confirmation requires the immutable phase 3 v2 freeze")
    build_digests = freeze.get("clean_build_sha256")
    profile_digests = freeze.get("clean_profile_sha256")
    if not isinstance(build_digests, list) or len(build_digests) != 2 or \
            len(set(build_digests)) != 1 or \
            freeze.get("executable_sha256") != build_digests[0]:
        failures.append("confirmation freeze has invalid clean-build evidence")
    if not isinstance(profile_digests, list) or len(profile_digests) != 2 or \
            len(set(profile_digests)) != 1 or \
            freeze.get("canonical_profile_sha256") != profile_digests[0]:
        failures.append("confirmation freeze has invalid profile evidence")
    if relative_freeze in {
            item.get("path") for item in freeze.get("artifacts", [])
            if isinstance(item, dict)}:
        failures.append("confirmation freeze must not hash itself")
    for artifact in freeze.get("artifacts", []):
        if not isinstance(artifact, dict) or not isinstance(
                artifact.get("path"), str):
            failures.append("confirmation freeze artifact is invalid")
            continue
        target = root / artifact["path"]
        try:
            target.resolve().relative_to(root)
        except ValueError:
            failures.append("confirmation freeze artifact path is unsafe")
            continue
        if not target.is_file() or hashlib.sha256(
                target.read_bytes()).hexdigest() != artifact.get("sha256"):
            failures.append(
                f"confirmation freeze artifact mismatch: {artifact['path']}")

    try:
        relative_manifest = (package_path / "manifest.json").relative_to(
            root).as_posix()
    except ValueError:
        failures.append("confirmation package must be inside the repository")
        relative_manifest = ""
    manifest_path = package_path / "manifest.json"
    if not manifest_path.is_file():
        failures.append("confirmation package manifest does not exist")
        return failures
    try:
        package = load_reference_package(package_path)
        manifest = package.manifest
    except (OSError, UnicodeError, ValueError, KeyError) as error:
        failures.append(f"confirmation package is invalid: {error}")
        return failures
    expected_hash = hashlib.sha256(manifest_path.read_bytes()).hexdigest()
    matches = [
        item for item in freeze.get("artifacts", [])
        if isinstance(item, dict) and
        item.get("role") == "confirmation_package_manifest" and
        item.get("path") == relative_manifest and
        item.get("sha256") == expected_hash
    ]
    if len(matches) != 1:
        failures.append("confirmation package is not hash-bound by the freeze")

    try:
        registry = load_data_lifecycle(lifecycle_path)
        entry = registry.entry(
            manifest.get("dataset_id"), manifest.get("dataset_version"))
        if entry.holdout_status != "confirmation":
            failures.append("reference partition is not in confirmation state")
    except (OSError, UnicodeError, ValueError, KeyError) as error:
        failures.append(f"confirmation lifecycle is invalid: {error}")

    if ledger_path.exists():
        try:
            ledger = json.loads(ledger_path.read_text(encoding="utf-8"))
        except (OSError, UnicodeError, json.JSONDecodeError):
            failures.append("confirmation ledger is unreadable")
            return failures
        if ledger.get("schema_version") != 1 or \
                not isinstance(ledger.get("records"), list):
            failures.append("confirmation ledger is invalid")
            return failures
        identity = (manifest.get("dataset_id"), manifest.get("dataset_version"))
        for record in ledger["records"]:
            if not isinstance(record, dict):
                failures.append("confirmation ledger record is invalid")
                continue
            if (record.get("dataset_id"), record.get("dataset_version")) == identity \
                    and record.get("partition") == "CONFIRMATION":
                failures.append("confirmation partition is already consumed")
                break
    return failures


def main(argv=None):
    parser = argparse.ArgumentParser(
        description="Check one-time Phase 3 confirmation access")
    parser.add_argument("--freeze", required=True, type=Path)
    parser.add_argument("--package", required=True, type=Path)
    parser.add_argument("--ledger", required=True, type=Path)
    arguments = parser.parse_args(argv)
    failures = validate_confirmation_access(
        Path.cwd(), arguments.freeze, arguments.package, arguments.ledger)
    if failures:
        for failure in failures:
            print(failure)
        return 1
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
