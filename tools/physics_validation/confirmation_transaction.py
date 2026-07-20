import hashlib
import json
import os
import re
import shutil
import tempfile
from dataclasses import dataclass, replace
from pathlib import Path

from .data_lifecycle import load_data_lifecycle
from .reference_package import load_reference_package


_SHA256 = re.compile(r"(?:sha256:)?[0-9a-f]{64}")


class ConfirmationAccessError(RuntimeError):
    pass


@dataclass(frozen=True)
class Declaration:
    root: Path
    freeze_path: Path
    candidate_id: str
    freeze_sha256: str
    package_key: str
    dataset_version: str
    manifest_path: Path
    manifest_relative_path: str
    manifest_sha256: str


@dataclass(frozen=True)
class Attempt:
    declaration: Declaration
    ledger_path: Path
    output_path: Path
    attempt_id: str

    def document(self):
        declaration = self.declaration
        return {
            "attempt_id": self.attempt_id,
            "candidate_id": declaration.candidate_id,
            "freeze_sha256": declaration.freeze_sha256,
            "dataset_id": declaration.package_key,
            "dataset_version": declaration.dataset_version,
            "package_key": declaration.package_key,
            "package_manifest_path": declaration.manifest_relative_path,
            "package_manifest_sha256": declaration.manifest_sha256,
            "partition": "CONFIRMATION",
            "schema_version": 2,
            "state": "STARTED",
        }


def _canonical(document):
    return (json.dumps(document, ensure_ascii=False, indent=2, sort_keys=True,
                       allow_nan=False) + "\n").encode("utf-8")


def _sha256(path):
    return hashlib.sha256(Path(path).read_bytes()).hexdigest()


def _fsync_directory(path):
    descriptor = os.open(Path(path), os.O_RDONLY)
    try:
        os.fsync(descriptor)
    finally:
        os.close(descriptor)


def _write_atomic(path, data):
    path = Path(path)
    path.parent.mkdir(parents=True, exist_ok=True)
    descriptor, temporary_name = tempfile.mkstemp(
        prefix=path.name + ".tmp-", dir=path.parent)
    temporary = Path(temporary_name)
    try:
        with os.fdopen(descriptor, "wb") as stream:
            stream.write(data)
            stream.flush()
            os.fsync(stream.fileno())
        os.replace(temporary, path)
        _fsync_directory(path.parent)
    finally:
        temporary.unlink(missing_ok=True)


def _read_ledger(path):
    path = Path(path)
    if not path.exists():
        return {"attempts": [], "records": [], "schema_version": 1}
    try:
        ledger = json.loads(path.read_text(encoding="utf-8"))
    except (OSError, UnicodeError, json.JSONDecodeError) as error:
        raise ConfirmationAccessError("confirmation ledger is unreadable") from error
    if set(ledger) != {"attempts", "records", "schema_version"} or \
            ledger["schema_version"] != 1 or \
            not isinstance(ledger["attempts"], list) or \
            not isinstance(ledger["records"], list):
        raise ConfirmationAccessError("confirmation ledger is invalid")
    return ledger


def _consumes(record, package_key):
    return record.get("partition") == "CONFIRMATION" and (
        record.get("package_key") == package_key or
        record.get("dataset_id") == package_key)


def _lock(path):
    lock_path = Path(str(path) + ".lock")
    try:
        descriptor = os.open(
            lock_path, os.O_CREAT | os.O_EXCL | os.O_WRONLY, 0o600)
    except FileExistsError as error:
        raise ConfirmationAccessError("confirmation ledger is locked") from error
    os.close(descriptor)
    return lock_path


def confirmation_declaration(root, freeze, package_key):
    root = Path(root).resolve()
    freeze_path = Path(freeze).resolve()
    try:
        freeze_path.relative_to(root)
    except ValueError as error:
        raise ConfirmationAccessError(
            "confirmation freeze must be inside the repository") from error
    try:
        document = json.loads(freeze_path.read_text(encoding="utf-8"))
    except (OSError, UnicodeError, json.JSONDecodeError) as error:
        raise ConfirmationAccessError("confirmation freeze is unreadable") from error
    if document.get("schema_version") not in {2, 3} or not isinstance(
            document.get("candidate_id"), str):
        raise ConfirmationAccessError("confirmation freeze schema is invalid")
    matches = []
    for artifact in document.get("artifacts", []):
        if not isinstance(artifact, dict) or \
                artifact.get("role") != "confirmation_package_manifest":
            continue
        relative = artifact.get("path")
        digest = artifact.get("sha256")
        if not isinstance(relative, str) or not isinstance(digest, str):
            continue
        if _SHA256.fullmatch(digest) is None:
            continue
        if Path(relative).name == "manifest.json" and \
                Path(relative).parent.name == package_key:
            matches.append((relative, digest))
    if len(matches) != 1:
        raise ConfirmationAccessError(
            "freeze must declare exactly one confirmation package manifest")
    relative, manifest_sha256 = matches[0]
    manifest_path = (root / relative).resolve()
    try:
        manifest_path.relative_to(root)
    except ValueError as error:
        raise ConfirmationAccessError(
            "confirmation manifest path is unsafe") from error
    freeze_sha256 = _sha256(freeze_path)
    return Declaration(
        root=root,
        freeze_path=freeze_path,
        candidate_id=document["candidate_id"],
        freeze_sha256=freeze_sha256,
        package_key=package_key,
        dataset_version="",
        manifest_path=manifest_path,
        manifest_relative_path=relative,
        manifest_sha256=manifest_sha256.removeprefix("sha256:"),
    )


def validate_confirmation_access_from_freeze(
        root, freeze, package_key, ledger,
        lifecycle_path=None):
    failures = []
    try:
        declaration = confirmation_declaration(root, freeze, package_key)
    except ConfirmationAccessError as error:
        return [str(error)]
    lifecycle_path = lifecycle_path or (
        declaration.root / "tests/physics_validation/validation_data_status.json")
    try:
        registry = load_data_lifecycle(lifecycle_path)
        matches = [entry for entry in registry.datasets
                   if entry.dataset_id == package_key]
        if len(matches) != 1 or matches[0].holdout_status != "confirmation":
            failures.append("reference partition is not in confirmation state")
    except (OSError, UnicodeError, ValueError) as error:
        failures.append(f"confirmation lifecycle is invalid: {error}")
    try:
        ledger_document = _read_ledger(ledger)
        if any(_consumes(record, package_key) for record in
               ledger_document["attempts"] + ledger_document["records"]):
            failures.append("confirmation partition is already consumed")
    except ConfirmationAccessError as error:
        failures.append(str(error))
    return failures


def reserve_from_freeze(root, freeze, package_key, ledger, output=None,
                        lifecycle_path=None):
    declaration = confirmation_declaration(root, freeze, package_key)
    failures = validate_confirmation_access_from_freeze(
        root, freeze, package_key, ledger, lifecycle_path)
    if failures:
        raise ConfirmationAccessError("; ".join(failures))
    lifecycle_path = lifecycle_path or (
        declaration.root / "tests/physics_validation/validation_data_status.json")
    registry = load_data_lifecycle(lifecycle_path)
    lifecycle_entries = [entry for entry in registry.datasets
                         if entry.dataset_id == package_key]
    if len(lifecycle_entries) != 1:
        raise ConfirmationAccessError(
            "confirmation lifecycle identity is ambiguous")
    declaration = replace(
        declaration, dataset_version=lifecycle_entries[0].dataset_version)
    output_path = Path(
        output or (Path(ledger).parent / "confirmation" / package_key)).resolve()
    if output_path.exists():
        raise ConfirmationAccessError("output path already exists")
    ledger_path = Path(ledger).resolve()
    ledger_path.parent.mkdir(parents=True, exist_ok=True)
    attempt_id = hashlib.sha256("\0".join((
        declaration.candidate_id,
        declaration.freeze_sha256,
        package_key,
        declaration.manifest_sha256,
    )).encode("utf-8")).hexdigest()
    attempt = Attempt(
        declaration, ledger_path, output_path, attempt_id)
    lock_path = _lock(ledger_path)
    try:
        document = _read_ledger(ledger_path)
        if any(_consumes(record, package_key) for record in
               document["attempts"] + document["records"]):
            raise ConfirmationAccessError(
                "confirmation partition is already consumed")
        document["attempts"].append(attempt.document())
        _write_atomic(ledger_path, _canonical(document))
    finally:
        lock_path.unlink(missing_ok=True)
        _fsync_directory(lock_path.parent)
    return attempt


def _open_reserved_package(attempt):
    declaration = attempt.declaration
    if not declaration.manifest_path.is_file() or \
            _sha256(declaration.manifest_path) != declaration.manifest_sha256:
        raise ConfirmationAccessError(
            "reserved confirmation manifest does not match the freeze")
    package = load_reference_package(declaration.manifest_path.parent)
    if package.manifest.get("dataset_id") != declaration.package_key:
        raise ConfirmationAccessError(
            "reserved confirmation package identity is invalid")
    if package.manifest.get("dataset_version") != declaration.dataset_version:
        raise ConfirmationAccessError(
            "reserved confirmation package version is invalid")
    return package


def _write_directory_atomically(output, files):
    output = Path(output)
    if output.exists():
        raise ConfirmationAccessError("output path already exists")
    output.parent.mkdir(parents=True, exist_ok=True)
    temporary = Path(tempfile.mkdtemp(
        prefix=output.name + ".tmp-", dir=output.parent))
    try:
        for relative, content in sorted(files.items()):
            relative_path = Path(relative)
            if relative_path.is_absolute() or ".." in relative_path.parts:
                raise ConfirmationAccessError("unsafe confirmation output path")
            target = temporary / relative_path
            target.parent.mkdir(parents=True, exist_ok=True)
            data = content.encode("utf-8") if isinstance(content, str) else content
            if not isinstance(data, bytes):
                raise ConfirmationAccessError("confirmation output is not bytes")
            with target.open("xb") as stream:
                stream.write(data)
                stream.flush()
                os.fsync(stream.fileno())
            _fsync_directory(target.parent)
        os.replace(temporary, output)
        _fsync_directory(output.parent)
    except BaseException:
        shutil.rmtree(temporary, ignore_errors=True)
        raise


def complete_attempt(attempt, result):
    if not isinstance(result, dict) or set(result) - {
            "result", "files", "failure_code"} or \
            set(result) < {"result", "files"}:
        raise ConfirmationAccessError("confirmation evaluator result is invalid")
    if result["result"] not in {"PASSED", "PASSED_OR_ACCOUNTED", "FAILED"} or \
            not isinstance(result["files"], dict) or \
            "validation_receipt.json" in result["files"]:
        raise ConfirmationAccessError("confirmation evaluator result is invalid")
    declaration = attempt.declaration
    files = dict(result["files"])
    _write_directory_atomically(attempt.output_path, files)
    receipt = {
        "attempt_id": attempt.attempt_id,
        "candidate_id": declaration.candidate_id,
        "dataset_id": declaration.package_key,
        "dataset_version": declaration.dataset_version,
        "freeze_sha256": declaration.freeze_sha256,
        "package_key": declaration.package_key,
        "package_manifest_sha256": declaration.manifest_sha256,
        "partition": "CONFIRMATION",
        "result": result["result"],
        "schema_version": 3,
        "files": {
            relative: _sha256(attempt.output_path / relative)
            for relative in sorted(files)
        },
    }
    if result.get("failure_code"):
        receipt["failure_code"] = result["failure_code"]
    receipt_path = attempt.output_path / "validation_receipt.json"
    with receipt_path.open("xb") as stream:
        stream.write(_canonical(receipt))
        stream.flush()
        os.fsync(stream.fileno())
    _fsync_directory(receipt_path.parent)
    lock_path = _lock(attempt.ledger_path)
    try:
        ledger = _read_ledger(attempt.ledger_path)
        if any(record.get("attempt_id") == attempt.attempt_id
               for record in ledger["records"]):
            raise ConfirmationAccessError("confirmation attempt is already finalized")
        ledger["records"].append(receipt)
        _write_atomic(attempt.ledger_path, _canonical(ledger))
    finally:
        lock_path.unlink(missing_ok=True)
        _fsync_directory(lock_path.parent)
    return receipt


def finalize_interrupted(attempt, output=None):
    if output is not None:
        attempt = replace(attempt, output_path=Path(output).resolve())
    return complete_attempt(attempt, {
        "failure_code": "FAILED_INTERRUPTED",
        "files": {"failure.json": _canonical({
            "attempt_id": attempt.attempt_id,
            "failure_code": "FAILED_INTERRUPTED",
            "policy": "fail_closed_without_replay",
            "schema_version": 1,
        })},
        "result": "FAILED",
    })


def consume_confirmation(freeze, package_key, output, ledger, evaluator,
                         repository_root=None, lifecycle_path=None):
    root = Path(repository_root or Path.cwd()).resolve()
    attempt = reserve_from_freeze(
        root, freeze, package_key, ledger, output, lifecycle_path)
    try:
        package = _open_reserved_package(attempt)
        result = evaluator(package)
        if not isinstance(result, dict) or set(result) - {
                "result", "files", "failure_code"} or \
                set(result) < {"result", "files"}:
            raise ConfirmationAccessError(
                "confirmation evaluator result is invalid")
        if not isinstance(result["files"], dict) or \
                "validation_receipt.json" in result["files"]:
            raise ConfirmationAccessError(
                "confirmation evaluator cannot provide its own receipt")
        if result["result"] not in {
                "PASSED", "PASSED_OR_ACCOUNTED", "FAILED"}:
            raise ConfirmationAccessError(
                "confirmation evaluator result is invalid")
        for relative, content in result["files"].items():
            relative_path = Path(relative)
            if relative_path.is_absolute() or ".." in relative_path.parts or \
                    not isinstance(content, (bytes, str)):
                raise ConfirmationAccessError(
                    "confirmation evaluator files are invalid")
    except Exception as error:
        result = {
            "failure_code": "FAILED_EVALUATOR_EXCEPTION",
            "files": {"failure.json": _canonical({
                "exception_type": type(error).__name__,
                "failure_code": "FAILED_EVALUATOR_EXCEPTION",
                "message": str(error),
                "policy": "fail_closed_without_replay",
                "schema_version": 1,
            })},
            "result": "FAILED",
        }
    return complete_attempt(attempt, result)
