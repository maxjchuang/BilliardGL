import argparse
from pathlib import Path

from .partition_run import case_ids_for_partition, load_reference_inputs
from .reference_run import _run_loaded_reference_validation


def run_calibration(executable, package, output, execute_once=None):
    executable = Path(executable).resolve()
    package_path = Path(package).resolve()
    output = Path(output).resolve()
    loaded = load_reference_inputs(package_path)
    calibration_case_ids = case_ids_for_partition(
        loaded.adaptation, "CALIBRATION")
    return _run_loaded_reference_validation(
        executable,
        package_path,
        output,
        loaded,
        execute_once=execute_once,
        case_ids=calibration_case_ids,
    )


def main(argv=None):
    parser = argparse.ArgumentParser(
        description="Run only committed calibration reference cases")
    parser.add_argument("--executable", required=True, type=Path)
    parser.add_argument("--package", required=True, type=Path)
    parser.add_argument("--output", required=True, type=Path)
    arguments = parser.parse_args(argv)
    return run_calibration(
        arguments.executable,
        arguments.package,
        arguments.output,
    )


if __name__ == "__main__":
    raise SystemExit(main())
