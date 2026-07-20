from dataclasses import dataclass
from pathlib import Path

from .reference_adapter import default_reference_registry
from .reference_package import load_reference_package
from .reference_point import read_reference_points
from .reference_split import load_reference_split


@dataclass(frozen=True)
class LoadedReferenceInputs:
    reference_package: object
    dataset_id: str
    dataset_version: str
    points: tuple
    split: object
    adaptation: object


def load_reference_inputs(package):
    package_path = Path(package).resolve()
    reference_package = load_reference_package(package_path)
    dataset_id = reference_package.manifest["dataset_id"]
    dataset_version = reference_package.manifest["dataset_version"]
    points = tuple(read_reference_points(
        reference_package.files["normalized"], dataset_id))
    split = load_reference_split(
        reference_package.files["split"], points, dataset_id, dataset_version)
    adaptation = default_reference_registry().adapt_with_limitations(
        reference_package, split, points)
    return LoadedReferenceInputs(
        reference_package=reference_package,
        dataset_id=dataset_id,
        dataset_version=dataset_version,
        points=points,
        split=split,
        adaptation=adaptation,
    )


def case_ids_for_partition(adaptation, partition):
    if partition not in {"CALIBRATION", "HOLDOUT"}:
        raise ValueError("partition must be a committed reference partition")
    result = tuple(
        case.case_id for case in adaptation.cases
        if case.partition == partition
    )
    if not result:
        raise ValueError(f"no executable {partition} cases")
    return result
