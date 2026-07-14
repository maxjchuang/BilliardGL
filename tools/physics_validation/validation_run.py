import argparse
import json
from pathlib import Path

from .data_lifecycle import load_data_lifecycle
from .model_candidate import load_candidate_freeze, sha256_file
from .partition_run import case_ids_for_partition, load_reference_inputs
from .reference_run import _run_loaded_reference_validation


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


def run_candidate_validation(
        freeze, executable, package, profile, output, execute_once=None):
    freeze_path = Path(freeze).resolve()
    executable = Path(executable).resolve()
    package_path = Path(package).resolve()
    profile = Path(profile).resolve()
    output = Path(output).resolve()

    candidate = load_candidate_freeze(freeze_path)
    calibration_report = freeze_path.parent / "calibration/reference_report.json"
    candidate.verify(
        profile=profile,
        executable=executable,
        calibration_report=calibration_report,
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
    parser.add_argument("--profile", required=True, type=Path)
    parser.add_argument("--output", required=True, type=Path)
    arguments = parser.parse_args(argv)
    return run_candidate_validation(
        arguments.freeze,
        arguments.executable,
        arguments.package,
        arguments.profile,
        arguments.output,
    )


if __name__ == "__main__":
    raise SystemExit(main())
