import argparse
from pathlib import Path

from .partition_run import load_reference_inputs
from .reference_run import _run_loaded_reference_validation


def _execution_is_forbidden(executable, scenario):
    raise RuntimeError("admission reports cannot execute scenarios")


def write_admission_report(executable, package, output):
    """Write limitations for a package that has admitted no numeric data."""
    executable = Path(executable).resolve()
    package_path = Path(package).resolve()
    output = Path(output).resolve()
    loaded = load_reference_inputs(package_path)
    if loaded.points:
        raise ValueError("admission report requires zero normalized points")
    if loaded.adaptation.cases:
        raise ValueError("admission report requires zero executable cases")
    if not loaded.adaptation.limitations:
        raise ValueError("admission report requires explicit limitations")
    return _run_loaded_reference_validation(
        executable,
        package_path,
        output,
        loaded,
        execute_once=_execution_is_forbidden,
    )


def main(argv=None):
    parser = argparse.ArgumentParser(
        description="Write a report for a zero-data admission-gated package")
    parser.add_argument("--executable", required=True, type=Path)
    parser.add_argument("--package", required=True, type=Path)
    parser.add_argument("--output", required=True, type=Path)
    arguments = parser.parse_args(argv)
    return write_admission_report(
        arguments.executable,
        arguments.package,
        arguments.output,
    )


if __name__ == "__main__":
    raise SystemExit(main())
