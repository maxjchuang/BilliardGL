import argparse
from pathlib import Path

from .model_candidate import load_candidate_freeze, write_candidate_freeze


def main(argv=None):
    parser = argparse.ArgumentParser(
        description="Create or verify a canonical frozen physics candidate")
    parser.add_argument("--verify", type=Path)
    parser.add_argument("--candidate-id")
    parser.add_argument("--formula-version")
    parser.add_argument("--source-revision")
    parser.add_argument("--profile", required=True, type=Path)
    parser.add_argument("--executable", required=True, type=Path)
    parser.add_argument("--calibration-report", required=True, type=Path)
    parser.add_argument("--dataset-manifest", action="append", type=Path)
    parser.add_argument("--created-at")
    parser.add_argument("--output", type=Path)
    arguments = parser.parse_args(argv)

    if arguments.verify is not None:
        creation_values = (
            arguments.candidate_id,
            arguments.formula_version,
            arguments.source_revision,
            arguments.created_at,
            arguments.output,
        )
        if any(value is not None for value in creation_values):
            parser.error("--verify is mutually exclusive with candidate creation")
        freeze = load_candidate_freeze(arguments.verify)
        freeze.verify(
            profile=arguments.profile,
            executable=arguments.executable,
            calibration_report=arguments.calibration_report,
            dataset_manifests=arguments.dataset_manifest,
        )
        return 0

    required = {
        "--candidate-id": arguments.candidate_id,
        "--formula-version": arguments.formula_version,
        "--source-revision": arguments.source_revision,
        "--dataset-manifest": arguments.dataset_manifest,
        "--created-at": arguments.created_at,
        "--output": arguments.output,
    }
    missing = [name for name, value in required.items() if not value]
    if missing:
        parser.error("candidate creation requires " + ", ".join(missing))
    write_candidate_freeze(
        candidate_id=arguments.candidate_id,
        formula_version=arguments.formula_version,
        source_revision=arguments.source_revision,
        profile=arguments.profile,
        executable=arguments.executable,
        calibration_report=arguments.calibration_report,
        dataset_manifests=arguments.dataset_manifest,
        created_at=arguments.created_at,
        output=arguments.output,
    )
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
