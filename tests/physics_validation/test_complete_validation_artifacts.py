import json
import hashlib
import tempfile
import unittest
from pathlib import Path

import tools.physics_validation.validation_artifacts as validation_artifacts
from tools.physics_validation.validation_artifacts import (
    build_validation_artifact_manifest,
    validate_validation_artifact_manifest,
)


ROOT = Path(__file__).resolve().parents[2]
INVENTORY = ROOT / "physics_models/promotion/phase3_candidates_v1.json"
MANIFEST = ROOT / "physics_models/promotion/phase3_validation_artifacts_v1.json"


class CompleteValidationArtifactTests(unittest.TestCase):
    def test_schema_v2_manifest_inventories_every_declared_artifact(self):
        build = getattr(validation_artifacts, "build_declared_artifact_manifest", None)
        validate = getattr(validation_artifacts, "validate_declared_artifact_manifest", None)
        self.assertIsNotNone(build, "schema-v2 artifact inventory is not implemented")
        self.assertIsNotNone(validate, "schema-v2 artifact validation is not implemented")
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            artifacts = root / "artifacts"
            artifacts.mkdir()
            first = artifacts / "first.json"
            second = artifacts / "second.csv"
            first.write_text("{}", encoding="utf-8")
            second.write_text("x,y\n1,2\n", encoding="utf-8")
            freeze = root / "freeze.json"
            document = {
                "artifacts": [
                    {"path": "artifacts/first.json", "role": "trace",
                     "sha256": hashlib.sha256(first.read_bytes()).hexdigest()},
                    {"path": "artifacts/second.csv", "role": "source_numeric_input",
                     "sha256": hashlib.sha256(second.read_bytes()).hexdigest()},
                ],
                "schema_version": 2,
            }
            freeze.write_text(json.dumps(document), encoding="utf-8")
            manifest = root / "manifest.json"
            expected = build(root, freeze)
            manifest.write_text(json.dumps(expected), encoding="utf-8")
            self.assertEqual(validate(manifest, root, freeze), [])
            second.write_text("changed", encoding="utf-8")
            self.assertEqual(
                validate(manifest, root, freeze),
                ["declared artifact manifest is stale or incomplete"],
            )

    def test_complete_artifact_manifest_reconstructs_and_hashes_every_file(self):
        expected = build_validation_artifact_manifest(ROOT, INVENTORY)
        committed = json.loads(MANIFEST.read_text(encoding="utf-8"))
        self.assertEqual(committed, expected)
        self.assertEqual(validate_validation_artifact_manifest(
            MANIFEST, ROOT, INVENTORY), [])

    def test_every_executed_holdout_case_preserves_trace_and_provenance(self):
        inventory = json.loads(INVENTORY.read_text(encoding="utf-8"))
        for candidate in inventory["candidates"]:
            for receipt_entry in candidate["receipts"]:
                validation = (ROOT / receipt_entry["path"]).parent
                report = json.loads((validation / "reference_report.json").read_text(
                    encoding="utf-8"))
                points = report["partitions"]["HOLDOUT"]["points"]
                expected = {
                    Path(point["trace_path"]).stem
                    for point in points if point.get("trace_path")
                }
                traces = {path.stem for path in (validation / "traces").glob("*.json")}
                provenance = {
                    path.stem for path in (validation / "provenance").glob("*.json")
                }
                with self.subTest(candidate=candidate["id"], validation=validation):
                    self.assertTrue(expected, "validation receipt has no executed trace")
                    self.assertEqual(traces, expected)
                    self.assertEqual(provenance, expected)
                    for stem in expected:
                        frames = json.loads((validation / "traces" / f"{stem}.json").read_text(
                            encoding="utf-8"))
                        source = json.loads((
                            validation / "provenance" / f"{stem}.json"
                        ).read_text(encoding="utf-8"))
                        self.assertIsInstance(frames, list)
                        self.assertTrue(frames)
                        self.assertEqual(source["dataset_id"], report["metadata"]["dataset_id"])
                        self.assertEqual(source["case_id"], stem.split("__", 1)[1])


if __name__ == "__main__":
    unittest.main()
