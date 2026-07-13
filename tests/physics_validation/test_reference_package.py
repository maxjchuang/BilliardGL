import hashlib
import json
import os
import shutil
import tempfile
import unittest
from pathlib import Path

from tools.physics_validation.reference_package import (
    ReferencePackageError,
    load_reference_package,
    update_reference_hashes,
)


FIXTURE = Path(__file__).parent / "fixtures/reference_package_v1"


def _digest(path):
    digest = hashlib.sha256()
    with path.open("rb") as source:
        for block in iter(lambda: source.read(1024 * 1024), b""):
            digest.update(block)
    return "sha256:" + digest.hexdigest()


class ReferencePackageTests(unittest.TestCase):
    def _copy_fixture(self, parent):
        destination = Path(parent) / "package"
        shutil.copytree(FIXTURE, destination)
        return destination

    def _manifest(self, package):
        return json.loads((package / "manifest.json").read_text(encoding="utf-8"))

    def _write_manifest(self, package, manifest):
        (package / "manifest.json").write_text(
            json.dumps(manifest, ensure_ascii=False, indent=2, sort_keys=True,
                       allow_nan=False) + "\n",
            encoding="utf-8")

    def _set_file_hash(self, package, manifest, file_id):
        item = next(item for item in manifest["files"] if item["id"] == file_id)
        item["sha256"] = _digest(package / item["path"])

    def _upgrade_extraction_v2(self, package, manifest):
        extraction_path = package / "extraction.json"
        extraction = json.loads(extraction_path.read_text(encoding="utf-8"))
        extraction.update({
            "schema_version": 2,
            "uncertainty_interpretation": "reported_bounded_range",
            "source_sha256": "sha256:" + "1" * 64,
            "output_sha256": next(
                item["sha256"] for item in manifest["files"]
                if item["id"] == "normalized"),
            "script": {
                "module": "tools.physics_validation.extract_fixture",
                "version": "1.0.0",
            },
        })
        extraction_path.write_text(
            json.dumps(extraction, ensure_ascii=False, indent=2, sort_keys=True,
                       allow_nan=False) + "\n",
            encoding="utf-8")
        self._set_file_hash(package, manifest, "extraction")

    def _assert_error_code(self, package, code):
        with self.assertRaises(ReferencePackageError) as raised:
            load_reference_package(package)
        self.assertEqual(raised.exception.code, code)

    def test_loads_verified_package_with_absolute_file_paths(self):
        package = load_reference_package(FIXTURE)

        self.assertEqual(package.manifest["dataset_id"], "synthetic_reference")
        self.assertEqual(package.root, FIXTURE.resolve())
        self.assertEqual(set(package.files), {
            "raw_extracted", "normalized", "split", "extraction",
            "scenario_template", "expected_model_mismatches",
            "expected_reference_limitations",
        })
        self.assertTrue(all(path.is_absolute() for path in package.files.values()))

    def test_rejects_unsupported_schema_before_returning_package(self):
        with tempfile.TemporaryDirectory() as directory:
            package = self._copy_fixture(directory)
            manifest = self._manifest(package)
            manifest["schema_version"] = 2
            self._write_manifest(package, manifest)

            self._assert_error_code(package, "UNSUPPORTED_SCHEMA")

    def test_rejects_unsafe_manifest_and_file_ids(self):
        mutations = (
            ("dataset_id", lambda manifest: manifest.__setitem__("dataset_id", "../data")),
            ("dataset_version", lambda manifest: manifest.__setitem__("dataset_version", "v 1")),
            ("adapter_id", lambda manifest: manifest.__setitem__("adapter_id", "adapter/name")),
            ("file_id", lambda manifest: manifest["files"][0].__setitem__("id", "../raw")),
        )
        for label, mutate in mutations:
            with self.subTest(label=label), tempfile.TemporaryDirectory() as directory:
                package = self._copy_fixture(directory)
                manifest = self._manifest(package)
                mutate(manifest)
                self._write_manifest(package, manifest)
                self._assert_error_code(package, "UNSAFE_ID")

    def test_rejects_duplicate_logical_and_resolved_files(self):
        for duplicate_kind in ("logical", "resolved"):
            with self.subTest(duplicate_kind=duplicate_kind), \
                    tempfile.TemporaryDirectory() as directory:
                package = self._copy_fixture(directory)
                manifest = self._manifest(package)
                if duplicate_kind == "logical":
                    manifest["files"][1]["id"] = manifest["files"][0]["id"]
                else:
                    manifest["files"][1]["path"] = manifest["files"][0]["path"]
                    manifest["files"][1]["sha256"] = manifest["files"][0]["sha256"]
                self._write_manifest(package, manifest)
                self._assert_error_code(package, "DUPLICATE_FILE")

    def test_rejects_absolute_traversal_and_symlink_escape_paths(self):
        path_values = ("/tmp/outside.csv", "../outside.csv")
        for path_value in path_values:
            with self.subTest(path=path_value), tempfile.TemporaryDirectory() as directory:
                package = self._copy_fixture(directory)
                manifest = self._manifest(package)
                manifest["files"][0]["path"] = path_value
                self._write_manifest(package, manifest)
                self._assert_error_code(package, "UNSAFE_PATH")

        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            package = self._copy_fixture(root)
            outside = root / "outside.csv"
            outside.write_text("outside\n", encoding="utf-8")
            os.symlink(outside, package / "escape.csv")
            manifest = self._manifest(package)
            manifest["files"][0]["path"] = "escape.csv"
            manifest["files"][0]["sha256"] = _digest(outside)
            self._write_manifest(package, manifest)
            self._assert_error_code(package, "UNSAFE_PATH")

    def test_rejects_missing_declared_or_required_file(self):
        for missing_kind in ("declared", "required"):
            with self.subTest(missing_kind=missing_kind), \
                    tempfile.TemporaryDirectory() as directory:
                package = self._copy_fixture(directory)
                manifest = self._manifest(package)
                if missing_kind == "declared":
                    manifest["files"][0]["path"] = "absent.csv"
                else:
                    manifest["files"] = [
                        item for item in manifest["files"] if item["id"] != "normalized"]
                self._write_manifest(package, manifest)
                self._assert_error_code(package, "MISSING_FILE")

    def test_rejects_malformed_or_mismatched_digest(self):
        for digest_kind in ("malformed", "mismatch"):
            with self.subTest(digest_kind=digest_kind), \
                    tempfile.TemporaryDirectory() as directory:
                package = self._copy_fixture(directory)
                manifest = self._manifest(package)
                if digest_kind == "malformed":
                    manifest["files"][0]["sha256"] = "0" * 64
                else:
                    with (package / "raw_extracted.csv").open("a", encoding="utf-8") as output:
                        output.write("changed\n")
                self._write_manifest(package, manifest)
                self._assert_error_code(
                    package, "INVALID_HASH" if digest_kind == "malformed" else "HASH_MISMATCH")

    def test_rejects_incomplete_extraction_metadata_after_hash_verification(self):
        required_fields = ("inputs", "transformations", "operator", "review")
        for field in required_fields:
            with self.subTest(field=field), tempfile.TemporaryDirectory() as directory:
                package = self._copy_fixture(directory)
                extraction_path = package / "extraction.json"
                extraction = json.loads(extraction_path.read_text(encoding="utf-8"))
                del extraction[field]
                extraction_path.write_text(
                    json.dumps(extraction, ensure_ascii=False, indent=2, sort_keys=True,
                               allow_nan=False) + "\n",
                    encoding="utf-8")
                manifest = self._manifest(package)
                self._set_file_hash(package, manifest, "extraction")
                self._write_manifest(package, manifest)
                self._assert_error_code(package, "INVALID_EXTRACTION_METADATA")

    def test_accepts_extraction_schema_v2_with_immutable_source_and_output_hashes(self):
        with tempfile.TemporaryDirectory() as directory:
            package = self._copy_fixture(directory)
            manifest = self._manifest(package)
            self._upgrade_extraction_v2(package, manifest)
            self._write_manifest(package, manifest)

            loaded = load_reference_package(package)

            self.assertEqual(loaded.manifest["dataset_id"], "synthetic_reference")

    def test_rejects_extraction_schema_v2_output_hash_disagreement(self):
        with tempfile.TemporaryDirectory() as directory:
            package = self._copy_fixture(directory)
            manifest = self._manifest(package)
            self._upgrade_extraction_v2(package, manifest)
            extraction_path = package / "extraction.json"
            extraction = json.loads(extraction_path.read_text(encoding="utf-8"))
            extraction["output_sha256"] = "sha256:" + "2" * 64
            extraction_path.write_text(
                json.dumps(extraction, ensure_ascii=False, indent=2, sort_keys=True,
                           allow_nan=False) + "\n",
                encoding="utf-8")
            self._set_file_hash(package, manifest, "extraction")
            self._write_manifest(package, manifest)

            self._assert_error_code(package, "INVALID_EXTRACTION_METADATA")

    def test_hash_updater_rewrites_only_manifest(self):
        with tempfile.TemporaryDirectory() as directory:
            package = self._copy_fixture(directory)
            original_files = {
                path.name: path.read_bytes()
                for path in package.iterdir() if path.name != "manifest.json"
            }
            with (package / "normalized.csv").open("a", encoding="utf-8") as output:
                output.write("\n")

            result = update_reference_hashes(package)
            loaded = load_reference_package(package)

            self.assertEqual(result, package / "manifest.json")
            self.assertEqual(loaded.root, package.resolve())
            for name, content in original_files.items():
                if name == "normalized.csv":
                    self.assertEqual((package / name).read_bytes(), content + b"\n")
                else:
                    self.assertEqual((package / name).read_bytes(), content)


if __name__ == "__main__":
    unittest.main()
