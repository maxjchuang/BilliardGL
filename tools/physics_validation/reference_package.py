import argparse
import hashlib
import json
import os
import re
import tempfile
from dataclasses import dataclass
from pathlib import Path


_SAFE_ID = re.compile(r"[A-Za-z0-9][A-Za-z0-9_.-]*")
_HASH = re.compile(r"sha256:[0-9a-f]{64}")
_MANIFEST_KEYS = {
    "schema_version",
    "dataset_id",
    "dataset_version",
    "adapter_id",
    "source",
    "acquisition",
    "evidence",
    "apparatus",
    "extraction_review",
    "files",
}
_REQUIRED_FILES = {
    "raw_extracted",
    "normalized",
    "split",
    "extraction",
    "scenario_template",
    "expected_model_mismatches",
    "expected_reference_limitations",
}
_EXTRACTION_KEYS = {
    "schema_version",
    "method",
    "tool",
    "date",
    "operator",
    "review",
    "inputs",
    "transformations",
    "rounding_policy",
}


class ReferencePackageError(ValueError):
    def __init__(self, code, message):
        super().__init__(f"{code}: {message}")
        self.code = code


@dataclass(frozen=True)
class ReferencePackage:
    root: Path
    manifest: dict
    files: dict


def _error(code, message):
    raise ReferencePackageError(code, message)


def _read_json(path, code):
    try:
        return json.loads(path.read_text(encoding="utf-8"))
    except (OSError, UnicodeError, json.JSONDecodeError) as error:
        _error(code, f"cannot read {path.name}: {error}")


def _valid_id(value):
    return isinstance(value, str) and _SAFE_ID.fullmatch(value) is not None


def _digest(path):
    digest = hashlib.sha256()
    try:
        with path.open("rb") as source:
            for block in iter(lambda: source.read(1024 * 1024), b""):
                digest.update(block)
    except OSError as error:
        _error("MISSING_FILE", f"cannot read declared file {path}: {error}")
    return "sha256:" + digest.hexdigest()


def _read_manifest(root):
    manifest_path = root / "manifest.json"
    if not manifest_path.is_file():
        _error("MISSING_FILE", "manifest.json is missing")
    manifest = _read_json(manifest_path, "INVALID_MANIFEST")
    if not isinstance(manifest, dict):
        _error("INVALID_MANIFEST", "manifest root must be an object")
    if manifest.get("schema_version") != 1:
        _error("UNSUPPORTED_SCHEMA", "manifest schema_version must be 1")
    if set(manifest) != _MANIFEST_KEYS:
        _error("INVALID_MANIFEST", "manifest keys do not match schema version 1")
    for field in ("dataset_id", "dataset_version", "adapter_id"):
        if not _valid_id(manifest.get(field)):
            _error("UNSAFE_ID", f"{field} is not a safe stable ID")
    for field in ("source", "acquisition", "evidence", "apparatus", "extraction_review"):
        if not isinstance(manifest.get(field), dict) or not manifest[field]:
            _error("INVALID_MANIFEST", f"{field} must be a nonempty object")
    if not isinstance(manifest.get("files"), list):
        _error("INVALID_MANIFEST", "files must be an array")
    return manifest


def _resolve_files(root, manifest, verify_hashes):
    files = {}
    resolved_paths = set()
    for item in manifest["files"]:
        if not isinstance(item, dict) or set(item) != {"id", "path", "sha256"}:
            _error("INVALID_MANIFEST", "each file entry requires id, path, and sha256")
        file_id = item["id"]
        if not _valid_id(file_id):
            _error("UNSAFE_ID", "file id is not a safe stable ID")
        if file_id in files:
            _error("DUPLICATE_FILE", f"duplicate logical file id: {file_id}")
        path_text = item["path"]
        if not isinstance(path_text, str) or not path_text:
            _error("UNSAFE_PATH", f"file {file_id} has an invalid path")
        relative_path = Path(path_text)
        if relative_path.is_absolute():
            _error("UNSAFE_PATH", f"file {file_id} uses an absolute path")
        resolved = (root / relative_path).resolve()
        try:
            resolved.relative_to(root)
        except ValueError:
            _error("UNSAFE_PATH", f"file {file_id} escapes the package root")
        if resolved in resolved_paths:
            _error("DUPLICATE_FILE", f"multiple file IDs resolve to {resolved}")
        if not resolved.is_file():
            _error("MISSING_FILE", f"declared file is missing: {path_text}")
        expected_hash = item["sha256"]
        if not isinstance(expected_hash, str) or _HASH.fullmatch(expected_hash) is None:
            _error("INVALID_HASH", f"file {file_id} has a malformed SHA-256")
        if verify_hashes:
            actual_hash = _digest(resolved)
            if actual_hash != expected_hash:
                _error(
                    "HASH_MISMATCH",
                    f"file {file_id} expected {expected_hash}, received {actual_hash}",
                )
        files[file_id] = resolved
        resolved_paths.add(resolved)
    missing = _REQUIRED_FILES - set(files)
    if missing:
        _error("MISSING_FILE", f"required logical files are absent: {sorted(missing)}")
    return files


def _validate_extraction(path, manifest):
    extraction = _read_json(path, "INVALID_EXTRACTION_METADATA")
    if not isinstance(extraction, dict) or set(extraction) != _EXTRACTION_KEYS:
        _error("INVALID_EXTRACTION_METADATA", "extraction metadata keys are incomplete")
    if extraction.get("schema_version") != 1:
        _error("INVALID_EXTRACTION_METADATA", "extraction schema_version must be 1")
    for field in ("method", "date", "operator", "rounding_policy"):
        if not isinstance(extraction.get(field), str) or not extraction[field].strip():
            _error("INVALID_EXTRACTION_METADATA", f"extraction {field} is required")
    tool = extraction.get("tool")
    if not isinstance(tool, dict) or set(tool) != {"name", "version"} \
            or not all(isinstance(value, str) and value.strip() for value in tool.values()):
        _error("INVALID_EXTRACTION_METADATA", "extraction tool name and version are required")
    review = extraction.get("review")
    if not isinstance(review, dict) or set(review) != {"reviewed_by", "date", "method"} \
            or not all(isinstance(value, str) and value.strip() for value in review.values()):
        _error("INVALID_EXTRACTION_METADATA", "independent extraction review is required")
    inputs = extraction.get("inputs")
    if not isinstance(inputs, list) or not inputs:
        _error("INVALID_EXTRACTION_METADATA", "at least one extraction input is required")
    manifest_hashes = {item["id"]: item["sha256"] for item in manifest["files"]}
    seen_inputs = set()
    for item in inputs:
        if not isinstance(item, dict) or set(item) != {"file_id", "sha256"}:
            _error("INVALID_EXTRACTION_METADATA", "extraction input keys are invalid")
        file_id = item["file_id"]
        digest = item["sha256"]
        if file_id in seen_inputs or file_id not in manifest_hashes:
            _error("INVALID_EXTRACTION_METADATA", "extraction input file ID is invalid")
        if not isinstance(digest, str) or _HASH.fullmatch(digest) is None:
            _error("INVALID_EXTRACTION_METADATA", "extraction input hash is invalid")
        if digest != manifest_hashes[file_id]:
            _error("INVALID_EXTRACTION_METADATA", "extraction input hash disagrees with manifest")
        seen_inputs.add(file_id)
    transformations = extraction.get("transformations")
    if not isinstance(transformations, list) or not transformations:
        _error("INVALID_EXTRACTION_METADATA", "at least one transformation is required")
    required_transformation = {"id", "formula", "input_unit", "output_unit"}
    for item in transformations:
        if not isinstance(item, dict) or set(item) != required_transformation:
            _error("INVALID_EXTRACTION_METADATA", "transformation keys are invalid")
        if not all(isinstance(value, str) and value.strip() for value in item.values()):
            _error("INVALID_EXTRACTION_METADATA", "transformation values must be nonempty")


def load_reference_package(path):
    root = Path(path).resolve()
    if not root.is_dir():
        _error("MISSING_FILE", f"package directory does not exist: {root}")
    manifest = _read_manifest(root)
    files = _resolve_files(root, manifest, verify_hashes=True)
    _validate_extraction(files["extraction"], manifest)
    return ReferencePackage(root, manifest, files)


def update_reference_hashes(path):
    requested_root = Path(path)
    root = Path(path).resolve()
    if not root.is_dir():
        _error("MISSING_FILE", f"package directory does not exist: {root}")
    manifest = _read_manifest(root)
    files = _resolve_files(root, manifest, verify_hashes=False)
    for item in manifest["files"]:
        item["sha256"] = _digest(files[item["id"]])
    _validate_extraction(files["extraction"], manifest)
    manifest_path = root / "manifest.json"
    serialized = json.dumps(
        manifest, ensure_ascii=False, indent=2, sort_keys=True, allow_nan=False) + "\n"
    descriptor, temporary_name = tempfile.mkstemp(
        prefix=".manifest.", suffix=".tmp", dir=root)
    try:
        with os.fdopen(descriptor, "w", encoding="utf-8") as output:
            output.write(serialized)
            output.flush()
            os.fsync(output.fileno())
        os.replace(temporary_name, manifest_path)
    except BaseException:
        try:
            os.unlink(temporary_name)
        except FileNotFoundError:
            pass
        raise
    return requested_root / "manifest.json"


def main(argv=None):
    parser = argparse.ArgumentParser(description="Verify an offline reference package")
    parser.add_argument("package", type=Path)
    parser.add_argument("--update-hashes", action="store_true")
    arguments = parser.parse_args(argv)
    if arguments.update_hashes:
        update_reference_hashes(arguments.package)
    package = load_reference_package(arguments.package)
    print(
        f"{package.manifest['dataset_id']} "
        f"{package.manifest['dataset_version']} verified"
    )
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
