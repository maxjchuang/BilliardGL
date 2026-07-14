import hashlib
import json
import math
import re
from dataclasses import dataclass
from pathlib import Path


_SAFE_ID = re.compile(r"[A-Za-z0-9][A-Za-z0-9_.-]*")
_SHA256 = re.compile(r"[0-9a-f]{64}")
_REVISION = re.compile(r"[0-9a-f]{40}")
_TIMESTAMP = re.compile(r"\d{4}-\d{2}-\d{2}T\d{2}:\d{2}:\d{2}Z")
_PROFILE_KEYS = {
    "schema_version", "runtime_profile", "parameter_sources", "applicability"}
_RUNTIME_KEYS = {
    "id", "formula_version", "ball", "surface", "cue", "cushion", "solver"}
_RUNTIME_SECTION_KEYS_V1 = {
    "ball": {"mass_kg", "radius_cm", "material"},
    "surface": {
        "legacy_friction_acceleration_cm_s2",
        "sliding_friction_coefficient",
        "rolling_resistance_acceleration_cm_s2",
        "torsional_spin_deceleration_rad_s2",
        "slip_speed_epsilon_cm_s",
        "stop_energy_threshold_j",
        "material",
    },
    "cue": {"effective_mass_kg"},
    "cushion": {"normal_restitution", "friction_coefficient"},
    "solver": {"time_step_seconds", "maximum_events_per_tick"},
}
_RUNTIME_SECTION_KEYS_V2 = dict(_RUNTIME_SECTION_KEYS_V1)
_RUNTIME_SECTION_KEYS_V2["cue"] = {
    "effective_mass_kg", "normal_restitution",
    "chalked_friction_coefficient", "unchalked_friction_coefficient",
    "maximum_reliable_offset_radius", "cue_speed_per_power_unit_cm_s",
}
_RUNTIME_SECTION_KEYS_V3 = dict(_RUNTIME_SECTION_KEYS_V2)
_RUNTIME_SECTION_KEYS_V3["ball"] = {
    "mass_kg", "radius_cm", "inertia_factor", "normal_restitution",
    "friction_coefficient", "material",
}
_FREEZE_KEYS_V1 = {
    "schema_version",
    "candidate_id",
    "formula_version",
    "source_revision",
    "profile_sha256",
    "executable_sha256",
    "calibration_report_sha256",
    "datasets",
    "metric_targets",
    "created_at",
}
_FREEZE_KEYS_V2 = {
    "schema_version",
    "candidate_id",
    "formula_version",
    "source_revision",
    "profile_sha256",
    "executable_sha256",
    "calibration_reports",
    "supplemental_artifacts",
    "created_at",
}
_DATASET_KEYS = {
    "dataset_id", "dataset_version", "manifest_sha256", "package_hashes"}
_TARGET_KEYS = {"point_id", "metric", "lower", "upper"}
_CALIBRATION_REPORT_KEYS = {
    "dataset_id", "dataset_version", "manifest_sha256", "package_hashes",
    "manifest_path", "path", "report_sha256", "metric_targets",
}
_SUPPLEMENTAL_ARTIFACT_KEYS = {"path", "sha256"}


def _reject_constant(value):
    raise ValueError(f"nonfinite JSON number {value}")


def _read_json(path, name):
    path = Path(path)
    try:
        raw = path.read_bytes()
        document = json.loads(raw.decode("utf-8"), parse_constant=_reject_constant)
    except (OSError, UnicodeError, json.JSONDecodeError) as error:
        raise ValueError(f"cannot read {name}: {error}") from error
    return raw, document


def _canonical(document):
    try:
        return (json.dumps(
            document,
            ensure_ascii=False,
            indent=2,
            sort_keys=True,
            allow_nan=False,
        ) + "\n").encode("utf-8")
    except (TypeError, ValueError) as error:
        raise ValueError(f"document is not finite canonical JSON: {error}") from error


def _safe_id(value, field):
    if not isinstance(value, str) or _SAFE_ID.fullmatch(value) is None:
        raise ValueError(f"{field} is not a safe stable ID")
    return value


def _sha256_bytes(raw):
    return hashlib.sha256(raw).hexdigest()


def sha256_file(path):
    try:
        return _sha256_bytes(Path(path).read_bytes())
    except OSError as error:
        raise ValueError(f"cannot hash {path}: {error}") from error


def _sha256(value, field):
    if not isinstance(value, str) or _SHA256.fullmatch(value) is None:
        raise ValueError(f"{field} must be a lowercase 64-character SHA-256")
    return value


def _numeric_leaves(value, prefix=""):
    result = {}
    if isinstance(value, dict):
        for key, item in value.items():
            path = f"{prefix}.{key}" if prefix else key
            result.update(_numeric_leaves(item, path))
    elif isinstance(value, (int, float)) and not isinstance(value, bool):
        if not math.isfinite(value):
            raise ValueError(f"runtime profile value {prefix} must be finite")
        result[prefix] = value
    return result


@dataclass(frozen=True)
class PhysicsProfileManifest:
    runtime_profile: dict
    parameter_sources: dict
    applicability: dict


def load_profile_manifest(path):
    raw, document = _read_json(path, "profile manifest")
    if not isinstance(document, dict) or set(document) != _PROFILE_KEYS:
        raise ValueError("profile manifest keys do not match schema version 1")
    schema_version = document.get("schema_version")
    if schema_version not in {1, 2, 3}:
        raise ValueError("profile manifest schema_version must be 1, 2, or 3")
    if raw != _canonical(document):
        raise ValueError("profile manifest must use canonical JSON bytes")

    runtime = document.get("runtime_profile")
    if not isinstance(runtime, dict) or set(runtime) != _RUNTIME_KEYS:
        raise ValueError("runtime_profile keys do not match scenario v3")
    _safe_id(runtime.get("id"), "runtime_profile.id")
    _safe_id(runtime.get("formula_version"), "runtime_profile.formula_version")
    section_keys = {
        1: _RUNTIME_SECTION_KEYS_V1,
        2: _RUNTIME_SECTION_KEYS_V2,
        3: _RUNTIME_SECTION_KEYS_V3,
    }[schema_version]
    for section, keys in section_keys.items():
        value = runtime.get(section)
        if not isinstance(value, dict) or set(value) != keys:
            raise ValueError(f"runtime_profile.{section} keys do not match scenario v3")
    numeric = _numeric_leaves(runtime)

    sources = document.get("parameter_sources")
    if not isinstance(sources, dict) or set(sources) != set(numeric):
        missing = sorted(set(numeric) - set(sources) if isinstance(sources, dict) else numeric)
        extra = sorted(set(sources) - set(numeric) if isinstance(sources, dict) else ())
        raise ValueError(
            f"parameter_sources must cover every numeric leaf; missing={missing}, extra={extra}")
    for path, source in sources.items():
        if not isinstance(source, dict):
            raise ValueError(f"parameter source {path} must be an object")
        _safe_id(source.get("kind"), f"parameter_sources.{path}.kind")
        if not isinstance(source.get("unit"), str) or not source["unit"].strip():
            raise ValueError(f"parameter source {path} requires a nonempty unit")
        statements = [source.get("evidence"), source.get("limitation")]
        if not any(isinstance(item, str) and item.strip() for item in statements):
            raise ValueError(
                f"parameter source {path} requires evidence or limitation")

    applicability = document.get("applicability")
    if not isinstance(applicability, dict) or not applicability:
        raise ValueError("applicability must be a nonempty object")
    return PhysicsProfileManifest(runtime, sources, applicability)


def _manifest_dataset(path):
    raw, document = _read_json(path, "dataset manifest")
    if not isinstance(document, dict) or document.get("schema_version") != 1:
        raise ValueError("dataset manifest must use schema version 1")
    dataset_id = _safe_id(document.get("dataset_id"), "dataset_id")
    dataset_version = _safe_id(document.get("dataset_version"), "dataset_version")
    files = document.get("files")
    if not isinstance(files, list) or not files:
        raise ValueError("dataset manifest requires files")
    hashes = {}
    for item in files:
        if not isinstance(item, dict):
            raise ValueError("dataset manifest file entry must be an object")
        file_id = _safe_id(item.get("id"), "dataset file id")
        value = item.get("sha256")
        if not isinstance(value, str) or not value.startswith("sha256:"):
            raise ValueError(f"dataset file {file_id} requires sha256")
        hashes[file_id] = _sha256(value[7:], f"dataset file {file_id} sha256")
    if len(hashes) != len(files):
        raise ValueError("dataset manifest file IDs must be unique")
    return {
        "dataset_id": dataset_id,
        "dataset_version": dataset_version,
        "manifest_sha256": _sha256_bytes(raw),
        "package_hashes": dict(sorted(hashes.items())),
    }


def dataset_records(dataset_manifests):
    records = [_manifest_dataset(path) for path in dataset_manifests]
    identities = [(item["dataset_id"], item["dataset_version"]) for item in records]
    if len(identities) != len(set(identities)):
        raise ValueError("dataset manifests must be sorted and unique")
    return tuple(sorted(records, key=lambda item: (
        item["dataset_id"], item["dataset_version"])))


def _calibration_targets(report, executable_sha256, datasets):
    if not isinstance(report, dict):
        raise ValueError("calibration report must be an object")
    metadata = report.get("metadata")
    partitions = report.get("partitions")
    if not isinstance(metadata, dict) or not isinstance(partitions, dict):
        raise ValueError("calibration report requires metadata and partitions")
    if metadata.get("build_id") != "sha256:" + executable_sha256:
        raise ValueError("calibration report build_id does not match executable_sha256")
    identity = (metadata.get("dataset_id"), metadata.get("dataset_version"))
    matching = [
        item for item in datasets
        if (item["dataset_id"], item["dataset_version"]) == identity
    ]
    if len(matching) != 1:
        raise ValueError("calibration report dataset is not bound by dataset manifests")
    expected_hashes = {
        key: "sha256:" + value
        for key, value in matching[0]["package_hashes"].items()
    }
    if metadata.get("package_hashes") != expected_hashes:
        raise ValueError("calibration report package hashes do not match dataset manifest")
    holdout = partitions.get("HOLDOUT")
    if not isinstance(holdout, dict) or holdout.get("points") != []:
        raise ValueError("calibration report must not contain HOLDOUT points")
    calibration = partitions.get("CALIBRATION")
    points = calibration.get("points") if isinstance(calibration, dict) else None
    if not isinstance(points, list) or not points:
        raise ValueError("calibration report has no metric targets")
    targets = []
    for point in points:
        if not isinstance(point, dict):
            raise ValueError("calibration metric target must be an object")
        point_id = _safe_id(point.get("point_id"), "metric target point_id")
        metric = _safe_id(point.get("metric"), "metric target metric")
        interval = point.get("acceptance_interval")
        if not isinstance(interval, list) or len(interval) != 2:
            raise ValueError(f"metric target {point_id} requires an acceptance interval")
        lower, upper = interval
        if any(isinstance(value, bool) or not isinstance(value, (int, float))
               or not math.isfinite(value) for value in interval):
            raise ValueError(f"metric target {point_id} thresholds must be finite")
        if lower > upper:
            raise ValueError(f"metric target {point_id} interval is reversed")
        targets.append({
            "point_id": point_id,
            "metric": metric,
            "lower": lower,
            "upper": upper,
        })
    targets.sort(key=lambda item: item["point_id"])
    if len({item["point_id"] for item in targets}) != len(targets):
        raise ValueError("metric target point IDs must be unique")
    return tuple(targets)


def _repository_relative(path, repository_root, name):
    root = Path(repository_root or Path.cwd()).resolve()
    resolved = Path(path).resolve()
    try:
        relative = resolved.relative_to(root)
    except ValueError as error:
        raise ValueError(f"{name} must be inside repository_root") from error
    if not relative.parts:
        raise ValueError(f"{name} must name a file")
    return relative.as_posix()


def _calibration_records(
        calibration_reports, executable_sha256, datasets, dataset_manifests,
        repository_root):
    manifests_by_identity = {}
    for path in dataset_manifests:
        record = _manifest_dataset(path)
        identity = (record["dataset_id"], record["dataset_version"])
        manifests_by_identity[identity] = path
    records = []
    for path in calibration_reports:
        _, report = _read_json(path, "calibration report")
        targets = _calibration_targets(report, executable_sha256, datasets)
        metadata = report["metadata"]
        identity = (metadata["dataset_id"], metadata["dataset_version"])
        dataset = next(item for item in datasets if (
            item["dataset_id"], item["dataset_version"]) == identity)
        records.append({
            "dataset_id": identity[0],
            "dataset_version": identity[1],
            "manifest_sha256": dataset["manifest_sha256"],
            "manifest_path": _repository_relative(
                manifests_by_identity[identity], repository_root,
                "dataset manifest"),
            "package_hashes": dataset["package_hashes"],
            "path": _repository_relative(
                path, repository_root, "calibration report"),
            "report_sha256": sha256_file(path),
            "metric_targets": list(targets),
        })
    identities = [
        (item["dataset_id"], item["dataset_version"]) for item in records]
    if len(identities) != len(set(identities)):
        raise ValueError("calibration reports must be sorted and unique")
    if set(identities) != {
            (item["dataset_id"], item["dataset_version"])
            for item in datasets}:
        raise ValueError(
            "calibration reports must bind every dataset manifest exactly once")
    return tuple(sorted(records, key=lambda item: (
        item["dataset_id"], item["dataset_version"])))


def _supplemental_records(paths, repository_root):
    records = [{
        "path": _repository_relative(path, repository_root, "supplemental artifact"),
        "sha256": sha256_file(path),
    } for path in paths]
    records.sort(key=lambda item: item["path"])
    names = [item["path"] for item in records]
    if len(names) != len(set(names)):
        raise ValueError("supplemental artifacts must be sorted and unique")
    return tuple(records)


def _validate_identity(document):
    _safe_id(document.get("candidate_id"), "candidate_id")
    _safe_id(document.get("formula_version"), "formula_version")
    revision = document.get("source_revision")
    if not isinstance(revision, str) or _REVISION.fullmatch(revision) is None:
        raise ValueError("source_revision must be a 40-character lowercase revision")
    created_at = document.get("created_at")
    if not isinstance(created_at, str) or _TIMESTAMP.fullmatch(created_at) is None:
        raise ValueError("created_at must be an explicit UTC timestamp")
    for field in ("profile_sha256", "executable_sha256"):
        _sha256(document.get(field), field)


def _validate_dataset(dataset, schema_version):
    if not isinstance(dataset, dict) or set(dataset) != _DATASET_KEYS:
        raise ValueError(
            f"dataset freeze keys do not match schema version {schema_version}")
    identity = (
        _safe_id(dataset.get("dataset_id"), "dataset_id"),
        _safe_id(dataset.get("dataset_version"), "dataset_version"),
    )
    _sha256(dataset.get("manifest_sha256"), "manifest_sha256")
    package_hashes = dataset.get("package_hashes")
    if not isinstance(package_hashes, dict) or not package_hashes:
        raise ValueError("package_hashes must be a nonempty object")
    if list(package_hashes) != sorted(package_hashes):
        raise ValueError("package_hashes must be sorted")
    for key, value in package_hashes.items():
        _safe_id(key, "package hash id")
        _sha256(value, f"package_hashes.{key}")
    return identity


def _validate_targets(targets, schema_version):
    if not isinstance(targets, list) or not targets:
        raise ValueError("metric_targets must be a nonempty sorted array")
    point_ids = []
    for target in targets:
        if not isinstance(target, dict) or set(target) != _TARGET_KEYS:
            raise ValueError(
                f"metric target keys do not match schema version {schema_version}")
        point_ids.append(_safe_id(target.get("point_id"), "metric target point_id"))
        _safe_id(target.get("metric"), "metric target metric")
        for field in ("lower", "upper"):
            value = target.get(field)
            if isinstance(value, bool) or not isinstance(value, (int, float)) \
                    or not math.isfinite(value):
                raise ValueError(f"metric target {field} must be finite")
        if target["lower"] > target["upper"]:
            raise ValueError("metric target interval is reversed")
    if point_ids != sorted(point_ids) or len(point_ids) != len(set(point_ids)):
        raise ValueError("metric targets must be sorted and unique")


def _validate_freeze_document(document):
    if not isinstance(document, dict):
        raise ValueError("candidate freeze must be an object")
    schema_version = document.get("schema_version")
    expected_keys = {1: _FREEZE_KEYS_V1, 2: _FREEZE_KEYS_V2}.get(schema_version)
    if expected_keys is None:
        raise ValueError("candidate freeze schema_version must be 1 or 2")
    if set(document) != expected_keys:
        raise ValueError(
            f"candidate freeze keys do not match schema version {schema_version}")
    _validate_identity(document)

    if schema_version == 2:
        reports = document.get("calibration_reports")
        if not isinstance(reports, list) or not reports:
            raise ValueError("calibration_reports must be a nonempty sorted array")
        identities = []
        for report in reports:
            if not isinstance(report, dict) or set(report) != _CALIBRATION_REPORT_KEYS:
                raise ValueError("calibration report keys do not match schema version 2")
            dataset = {key: report[key] for key in _DATASET_KEYS}
            identities.append(_validate_dataset(dataset, 2))
            path = report.get("path")
            if not isinstance(path, str) or not path or Path(path).is_absolute():
                raise ValueError("calibration report path must be repository-relative")
            manifest_path = report.get("manifest_path")
            if not isinstance(manifest_path, str) or not manifest_path \
                    or Path(manifest_path).is_absolute():
                raise ValueError("dataset manifest path must be repository-relative")
            _sha256(report.get("report_sha256"), "report_sha256")
            _validate_targets(report.get("metric_targets"), 2)
        if identities != sorted(identities) or len(identities) != len(set(identities)):
            raise ValueError("calibration reports must be sorted and unique")

        artifacts = document.get("supplemental_artifacts")
        if not isinstance(artifacts, list) or not artifacts:
            raise ValueError("supplemental_artifacts must be a nonempty sorted array")
        paths = []
        for artifact in artifacts:
            if not isinstance(artifact, dict) or set(artifact) != _SUPPLEMENTAL_ARTIFACT_KEYS:
                raise ValueError("supplemental artifact keys do not match schema version 2")
            path = artifact.get("path")
            if not isinstance(path, str) or not path or Path(path).is_absolute():
                raise ValueError("supplemental artifact path must be repository-relative")
            paths.append(path)
            _sha256(artifact.get("sha256"), "supplemental artifact sha256")
        if paths != sorted(paths) or len(paths) != len(set(paths)):
            raise ValueError("supplemental artifacts must be sorted and unique")
        return

    _sha256(document.get("calibration_report_sha256"),
            "calibration_report_sha256")

    datasets = document.get("datasets")
    if not isinstance(datasets, list) or not datasets:
        raise ValueError("datasets must be a nonempty sorted array")
    identities = []
    for dataset in datasets:
        identities.append(_validate_dataset(dataset, 1))
    if identities != sorted(identities) or len(identities) != len(set(identities)):
        raise ValueError("datasets must be sorted and unique")

    _validate_targets(document.get("metric_targets"), 1)


@dataclass(frozen=True)
class ModelCandidateFreeze:
    schema_version: int
    candidate_id: str
    formula_version: str
    source_revision: str
    profile_sha256: str
    executable_sha256: str
    calibration_report_sha256: object
    calibration_reports: tuple
    datasets: tuple
    metric_targets: tuple
    supplemental_artifacts: tuple
    created_at: str

    def verify(
            self, profile, executable, calibration_report=None,
            dataset_manifests=None, calibration_reports=None,
            supplemental_artifacts=None, repository_root=None):
        actual = {
            "profile_sha256": sha256_file(profile),
            "executable_sha256": sha256_file(executable),
        }
        for field, value in actual.items():
            if value != getattr(self, field):
                raise ValueError(f"{field} does not match candidate freeze")
        if self.schema_version == 1:
            value = sha256_file(calibration_report)
            if value != self.calibration_report_sha256:
                raise ValueError(
                    "calibration_report_sha256 does not match candidate freeze")
        else:
            if calibration_reports is None or dataset_manifests is None \
                    or supplemental_artifacts is None:
                raise ValueError(
                    "schema v2 verification requires all calibration reports, "
                    "dataset manifests, and supplemental artifacts")
            datasets = dataset_records(tuple(dataset_manifests))
            reports = _calibration_records(
                tuple(calibration_reports), self.executable_sha256, datasets,
                tuple(dataset_manifests), repository_root)
            if reports != self.calibration_reports:
                raise ValueError("calibration reports do not match candidate freeze")
            supplemental = _supplemental_records(
                tuple(supplemental_artifacts), repository_root)
            if supplemental != self.supplemental_artifacts:
                raise ValueError("supplemental artifacts do not match candidate freeze")
        manifest = load_profile_manifest(profile)
        if manifest.runtime_profile["id"] != self.candidate_id:
            raise ValueError("candidate_id does not match profile")
        if manifest.runtime_profile["formula_version"] != self.formula_version:
            raise ValueError("formula_version does not match profile")
        if self.schema_version == 1 and dataset_manifests is not None:
            if dataset_records(dataset_manifests) != self.datasets:
                raise ValueError("dataset manifests do not match candidate freeze")
        return True

    def verify_dataset_manifest(self, dataset_manifest):
        record = _manifest_dataset(dataset_manifest)
        identity = (record["dataset_id"], record["dataset_version"])
        matches = [
            item for item in self.datasets
            if (item["dataset_id"], item["dataset_version"]) == identity
        ]
        if len(matches) != 1 or matches[0] != record:
            raise ValueError("dataset manifest does not match candidate freeze")
        return record


def load_candidate_freeze(path):
    raw, document = _read_json(path, "candidate freeze")
    _validate_freeze_document(document)
    if raw != _canonical(document):
        raise ValueError("candidate freeze must use canonical JSON bytes")
    return ModelCandidateFreeze(
        schema_version=document["schema_version"],
        candidate_id=document["candidate_id"],
        formula_version=document["formula_version"],
        source_revision=document["source_revision"],
        profile_sha256=document["profile_sha256"],
        executable_sha256=document["executable_sha256"],
        calibration_report_sha256=document.get("calibration_report_sha256"),
        calibration_reports=tuple(document.get("calibration_reports", ())),
        datasets=tuple(document.get("datasets", ({
            key: report[key] for key in _DATASET_KEYS
        } for report in document.get("calibration_reports", ())))),
        metric_targets=tuple(document.get("metric_targets", ())),
        supplemental_artifacts=tuple(document.get("supplemental_artifacts", ())),
        created_at=document["created_at"],
    )


def write_candidate_freeze(
        candidate_id, formula_version, source_revision, profile, executable,
        calibration_report, dataset_manifests, created_at, output,
        calibration_reports=None, supplemental_artifacts=(),
        repository_root=None):
    profile_manifest = load_profile_manifest(profile)
    candidate_id = _safe_id(candidate_id, "candidate_id")
    formula_version = _safe_id(formula_version, "formula_version")
    if profile_manifest.runtime_profile["id"] != candidate_id:
        raise ValueError("candidate_id does not match profile")
    if profile_manifest.runtime_profile["formula_version"] != formula_version:
        raise ValueError("formula_version does not match profile")
    if not isinstance(source_revision, str) or _REVISION.fullmatch(source_revision) is None:
        raise ValueError("source_revision must be a 40-character lowercase revision")
    if not isinstance(created_at, str) or _TIMESTAMP.fullmatch(created_at) is None:
        raise ValueError("created_at must be an explicit UTC timestamp")

    datasets = dataset_records(tuple(dataset_manifests))
    executable_hash = sha256_file(executable)
    common = {
        "candidate_id": candidate_id,
        "formula_version": formula_version,
        "source_revision": source_revision,
        "profile_sha256": sha256_file(profile),
        "executable_sha256": executable_hash,
        "created_at": created_at,
    }
    if calibration_reports is None:
        _, report_document = _read_json(calibration_report, "calibration report")
        targets = _calibration_targets(report_document, executable_hash, datasets)
        document = {
            "schema_version": 1,
            **common,
            "calibration_report_sha256": sha256_file(calibration_report),
            "datasets": list(datasets),
            "metric_targets": list(targets),
        }
    else:
        if calibration_report is not None:
            raise ValueError(
                "calibration_report and calibration_reports are mutually exclusive")
        reports = _calibration_records(
            tuple(calibration_reports), executable_hash, datasets,
            tuple(dataset_manifests), repository_root)
        artifacts = _supplemental_records(
            tuple(supplemental_artifacts), repository_root)
        document = {
            "schema_version": 2,
            **common,
            "calibration_reports": list(reports),
            "supplemental_artifacts": list(artifacts),
        }
    _validate_freeze_document(document)
    output = Path(output)
    output.parent.mkdir(parents=True, exist_ok=True)
    output.write_bytes(_canonical(document))
    return output
