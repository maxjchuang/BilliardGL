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
    parser.add_argument(
        "--calibration-report", required=True, action="append", type=Path)
    parser.add_argument("--dataset-manifest", action="append", type=Path)
    parser.add_argument("--supplemental-artifact", action="append", type=Path)
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
        if freeze.schema_version == 1:
            if len(arguments.calibration_report) != 1 \
                    or arguments.supplemental_artifact:
                parser.error(
                    "schema v1 verification requires one calibration report "
                    "and no supplemental artifacts")
            verify_arguments = {
                "calibration_report": arguments.calibration_report[0],
            }
        else:
            verify_arguments = {
                "calibration_reports": arguments.calibration_report,
                "supplemental_artifacts": arguments.supplemental_artifact or (),
                "repository_root": Path.cwd(),
            }
        freeze.verify(
            profile=arguments.profile,
            executable=arguments.executable,
            dataset_manifests=arguments.dataset_manifest,
            **verify_arguments,
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
    use_v2 = len(arguments.calibration_report) > 1 \
        or bool(arguments.supplemental_artifact)
    write_candidate_freeze(
        candidate_id=arguments.candidate_id,
        formula_version=arguments.formula_version,
        source_revision=arguments.source_revision,
        profile=arguments.profile,
        executable=arguments.executable,
        calibration_report=(
            None if use_v2 else arguments.calibration_report[0]),
        calibration_reports=(
            arguments.calibration_report if use_v2 else None),
        dataset_manifests=arguments.dataset_manifest,
        supplemental_artifacts=arguments.supplemental_artifact or (),
        repository_root=Path.cwd(),
        created_at=arguments.created_at,
        output=arguments.output,
    )
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
