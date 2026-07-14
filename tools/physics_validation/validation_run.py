import argparse
import hashlib
import json
import os
import shutil
import tempfile
from pathlib import Path

from .data_lifecycle import load_data_lifecycle
from .holdout_access import validate_confirmation_access
from .model_candidate import load_candidate_freeze, sha256_file
from .partition_run import case_ids_for_partition, load_reference_inputs
from .reference_run import _run_loaded_reference_validation
from .confirmation_run import build_confirmation_result


DEFAULT_LIFECYCLE_PATH = (
    Path(__file__).resolve().parents[2]
    / "tests/physics_validation/validation_data_status.json")


def _canonical(document):
    return json.dumps(
        document,
        ensure_ascii=False,
        indent=2,
        sort_keys=True,
        allow_nan=False,
    ) + "\n"


class ConfirmationAccessError(RuntimeError):
    pass


def _safe_output_path(root, value):
    value = Path(value)
    if value.is_absolute() or not value.parts or ".." in value.parts:
        raise ConfirmationAccessError(f"unsafe confirmation output path: {value}")
    target = root / value
    try:
        target.resolve().relative_to(root.resolve())
    except ValueError as error:
        raise ConfirmationAccessError(
            f"unsafe confirmation output path: {value}") from error
    return target


def write_directory_atomically(output_path, files):
    output_path = Path(output_path).resolve()
    if output_path.exists():
        raise ConfirmationAccessError("output path already exists")
    output_path.parent.mkdir(parents=True, exist_ok=True)
    temporary = Path(tempfile.mkdtemp(
        prefix=output_path.name + ".tmp-", dir=output_path.parent))
    try:
        for relative, content in sorted(files.items()):
            target = _safe_output_path(temporary, relative)
            target.parent.mkdir(parents=True, exist_ok=True)
            data = content.encode("utf-8") if isinstance(content, str) else content
            if not isinstance(data, bytes):
                raise ConfirmationAccessError(
                    f"confirmation output is not bytes: {relative}")
            with target.open("xb") as stream:
                stream.write(data)
                stream.flush()
                os.fsync(stream.fileno())
        temporary.rename(output_path)
    except BaseException:
        if temporary.exists():
            shutil.rmtree(temporary)
        raise


def write_json_exclusive(path, document):
    path = Path(path)
    path.parent.mkdir(parents=True, exist_ok=True)
    try:
        with path.open("x", encoding="utf-8", newline="") as stream:
            stream.write(_canonical(document))
            stream.flush()
            os.fsync(stream.fileno())
    except FileExistsError as error:
        raise ConfirmationAccessError(
            f"exclusive confirmation file already exists: {path}") from error


def reserve_confirmation_attempt_exclusive(
        ledger_path, freeze_path, package_path):
    ledger_path = Path(ledger_path).resolve()
    ledger_path.parent.mkdir(parents=True, exist_ok=True)
    manifest_path = Path(package_path).resolve() / "manifest.json"
    manifest = json.loads(manifest_path.read_text(encoding="utf-8"))
    attempt = {
        "schema_version": 1,
        "candidate_id": "phase3_integrated_v2",
        "freeze_sha256": hashlib.sha256(
            Path(freeze_path).read_bytes()).hexdigest(),
        "dataset_id": manifest["dataset_id"],
        "dataset_version": manifest["dataset_version"],
        "partition": "CONFIRMATION",
        "package_manifest_sha256": hashlib.sha256(
            manifest_path.read_bytes()).hexdigest(),
        "state": "STARTED",
    }
    lock = ledger_path.with_name(ledger_path.name + ".lock")
    try:
        descriptor = os.open(lock, os.O_CREAT | os.O_EXCL | os.O_WRONLY, 0o600)
    except FileExistsError as error:
        raise ConfirmationAccessError("confirmation ledger is locked") from error
    os.close(descriptor)
    temporary = ledger_path.with_name(ledger_path.name + ".tmp")
    try:
        if ledger_path.exists():
            ledger = json.loads(ledger_path.read_text(encoding="utf-8"))
            if ledger.get("schema_version") != 1 or \
                    not isinstance(ledger.get("records"), list) or \
                    not isinstance(ledger.get("attempts", []), list):
                raise ConfirmationAccessError("confirmation ledger is invalid")
        else:
            ledger = {"schema_version": 1, "attempts": [], "records": []}
        identity = (attempt["dataset_id"], attempt["dataset_version"])
        consumed = ledger.get("attempts", []) + ledger["records"]
        if any((record.get("dataset_id"), record.get("dataset_version")) == identity
               and record.get("partition") == "CONFIRMATION"
               for record in consumed):
            raise ConfirmationAccessError(
                "confirmation partition is already consumed")
        ledger.setdefault("attempts", []).append(attempt)
        with temporary.open("x", encoding="utf-8", newline="") as stream:
            stream.write(_canonical(ledger))
            stream.flush()
            os.fsync(stream.fileno())
        os.replace(temporary, ledger_path)
    finally:
        if temporary.exists():
            temporary.unlink()
        lock.unlink(missing_ok=True)
    return attempt


def append_consumption_record_exclusive(ledger_path, receipt):
    ledger_path = Path(ledger_path).resolve()
    ledger_path.parent.mkdir(parents=True, exist_ok=True)
    lock = ledger_path.with_name(ledger_path.name + ".lock")
    try:
        lock_descriptor = os.open(lock, os.O_CREAT | os.O_EXCL | os.O_WRONLY, 0o600)
    except FileExistsError as error:
        raise ConfirmationAccessError("confirmation ledger is locked") from error
    os.close(lock_descriptor)
    temporary = ledger_path.with_name(ledger_path.name + ".tmp")
    try:
        if ledger_path.exists():
            ledger = json.loads(ledger_path.read_text(encoding="utf-8"))
            if ledger.get("schema_version") != 1 or \
                    not isinstance(ledger.get("records"), list):
                raise ConfirmationAccessError("confirmation ledger is invalid")
        else:
            ledger = {"schema_version": 1, "records": []}
        identity = (receipt["dataset_id"], receipt["dataset_version"])
        if any((record.get("dataset_id"), record.get("dataset_version")) == identity
               and record.get("partition") == "CONFIRMATION"
               for record in ledger["records"]):
            raise ConfirmationAccessError(
                "confirmation partition is already consumed")
        ledger["records"].append(receipt)
        with temporary.open("x", encoding="utf-8", newline="") as stream:
            stream.write(_canonical(ledger))
            stream.flush()
            os.fsync(stream.fileno())
        os.replace(temporary, ledger_path)
    finally:
        if temporary.exists():
            temporary.unlink()
        lock.unlink(missing_ok=True)


def consume_confirmation(freeze_path, package_path, output_path, ledger_path,
                         runner, repository_root=None):
    root = Path(repository_root or Path.cwd()).resolve()
    output_path = Path(output_path).resolve()
    if output_path.exists():
        raise ConfirmationAccessError("output path already exists")
    failures = validate_confirmation_access(
        root, freeze_path, package_path, ledger_path)
    if failures:
        raise ConfirmationAccessError("; ".join(failures))
    reserve_confirmation_attempt_exclusive(
        ledger_path, freeze_path, package_path)
    try:
        result = runner()
    except Exception as error:
        result = {
            "result": "FAILED",
            "files": {
                "failure.json": _canonical({
                    "schema_version": 1,
                    "failure_stage": "confirmation_runner",
                    "exception_type": type(error).__name__,
                    "message": str(error),
                    "policy": "fail_closed_without_replay",
                }).encode("utf-8"),
            },
        }
    if not isinstance(result, dict) or set(result) != {"result", "files"}:
        raise ConfirmationAccessError("confirmation runner result is invalid")
    if result["result"] not in {"PASSED_OR_ACCOUNTED", "FAILED"} or \
            not isinstance(result["files"], dict):
        raise ConfirmationAccessError("confirmation runner result is invalid")
    if "validation_receipt.json" in result["files"]:
        raise ConfirmationAccessError(
            "confirmation runner cannot provide its own receipt")
    write_directory_atomically(output_path, result["files"])
    manifest_path = Path(package_path).resolve() / "manifest.json"
    manifest = json.loads(manifest_path.read_text(encoding="utf-8"))
    receipt = {
        "schema_version": 2,
        "candidate_id": "phase3_integrated_v2",
        "freeze_sha256": hashlib.sha256(
            Path(freeze_path).read_bytes()).hexdigest(),
        "dataset_id": manifest["dataset_id"],
        "dataset_version": manifest["dataset_version"],
        "partition": "CONFIRMATION",
        "package_manifest_sha256": hashlib.sha256(
            manifest_path.read_bytes()).hexdigest(),
        "result": result["result"],
        "files": {
            relative: hashlib.sha256(
                (output_path / relative).read_bytes()).hexdigest()
            for relative in sorted(result["files"])
        },
    }
    write_json_exclusive(output_path / "validation_receipt.json", receipt)
    append_consumption_record_exclusive(ledger_path, receipt)
    return receipt


def run_candidate_validation(
        freeze, executable, package, profile, output, execute_once=None,
        repository_root=None):
    freeze_path = Path(freeze).resolve()
    executable = Path(executable).resolve()
    package_path = Path(package).resolve()
    profile = Path(profile).resolve()
    output = Path(output).resolve()

    candidate = load_candidate_freeze(freeze_path)
    if candidate.schema_version == 1:
        candidate.verify(
            profile=profile,
            executable=executable,
            calibration_report=(
                freeze_path.parent / "calibration/reference_report.json"),
        )
    else:
        root = Path(repository_root or Path.cwd()).resolve()
        candidate.verify(
            profile=profile,
            executable=executable,
            calibration_reports=tuple(
                root / item["path"] for item in candidate.calibration_reports),
            dataset_manifests=tuple(
                root / item["manifest_path"]
                for item in candidate.calibration_reports),
            supplemental_artifacts=tuple(
                root / item["path"] for item in candidate.supplemental_artifacts),
            repository_root=root,
        )

    loaded = load_reference_inputs(package_path)
    candidate.verify_dataset_manifest(package_path / "manifest.json")
    lifecycle = load_data_lifecycle(DEFAULT_LIFECYCLE_PATH)
    lifecycle.require_validation_holdout(
        loaded.dataset_id, loaded.dataset_version)
    holdout_case_ids = case_ids_for_partition(loaded.adaptation, "HOLDOUT")

    freeze_hash = sha256_file(freeze_path)
    exit_code = _run_loaded_reference_validation(
        executable,
        package_path,
        output,
        loaded,
        execute_once=execute_once,
        case_ids=holdout_case_ids,
        metadata_labels={
            "candidate_id": candidate.candidate_id,
            "freeze_sha256": freeze_hash,
        },
    )
    report_path = output / "reference_report.json"
    receipt = {
        "schema_version": 1,
        "candidate_id": candidate.candidate_id,
        "freeze_sha256": freeze_hash,
        "dataset_id": loaded.dataset_id,
        "dataset_version": loaded.dataset_version,
        "partition": "HOLDOUT",
        "report_sha256": sha256_file(report_path),
        "result": "PASSED_OR_ACCOUNTED" if exit_code == 0 else "FAILED",
    }
    output.mkdir(parents=True, exist_ok=True)
    (output / "validation_receipt.json").write_text(
        _canonical(receipt), encoding="utf-8")
    return exit_code


def main(argv=None):
    parser = argparse.ArgumentParser(
        description="Validate a frozen physics candidate on committed holdout data")
    parser.add_argument("--freeze", required=True, type=Path)
    parser.add_argument("--executable", required=True, type=Path)
    parser.add_argument("--package", required=True, type=Path)
    parser.add_argument("--profile", type=Path)
    parser.add_argument("--output", required=True, type=Path)
    parser.add_argument("--ledger", type=Path)
    arguments = parser.parse_args(argv)
    if arguments.ledger is not None:
        receipt = consume_confirmation(
            arguments.freeze,
            arguments.package,
            arguments.output,
            arguments.ledger,
            lambda: build_confirmation_result(
                arguments.executable,
                arguments.freeze,
                arguments.package,
                Path.cwd(),
            ),
            repository_root=Path.cwd(),
        )
        return 0 if receipt["result"] == "PASSED_OR_ACCOUNTED" else 1
    if arguments.profile is None:
        parser.error("legacy validation requires --profile")
    return run_candidate_validation(
        arguments.freeze,
        arguments.executable,
        arguments.package,
        arguments.profile,
        arguments.output,
    )


if __name__ == "__main__":
    raise SystemExit(main())
