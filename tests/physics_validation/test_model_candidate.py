import json
import math
import shutil
import tempfile
import unittest
from pathlib import Path

from tools.physics_validation.model_candidate import (
    load_candidate_freeze,
    load_profile_manifest,
    write_candidate_freeze,
)


FIXTURE_ROOT = Path(__file__).parent / "fixtures/model_candidate_v1"
DATASET_MANIFEST = (
    Path(__file__).parent / "fixtures/reference_package_v1/manifest.json")
PRODUCTION_CUE_PROFILE = Path(__file__).parents[2] / (
    "physics_models/profiles/chinese_pool_cue_contact_v1.json")


class ModelCandidateTests(unittest.TestCase):
    def setUp(self):
        self.temporary = tempfile.TemporaryDirectory()
        self.addCleanup(self.temporary.cleanup)
        self.root = Path(self.temporary.name)
        self.profile = self.root / "profile.json"
        self.report = self.root / "calibration_report.json"
        shutil.copyfile(FIXTURE_ROOT / "profile.json", self.profile)
        shutil.copyfile(FIXTURE_ROOT / "calibration_report.json", self.report)
        self.executable = self.root / "Billiards"
        self.executable.write_bytes(b"synthetic executable")

    def _write(self, output=None, **overrides):
        arguments = {
            "candidate_id": "surface_motion_v1",
            "formula_version": "legacy_v1",
            "source_revision": "0123456789abcdef0123456789abcdef01234567",
            "profile": self.profile,
            "executable": self.executable,
            "calibration_report": self.report,
            "dataset_manifests": (DATASET_MANIFEST,),
            "created_at": "2026-07-14T00:00:00Z",
            "output": output or self.root / "freeze.json",
        }
        arguments.update(overrides)
        return write_candidate_freeze(**arguments)

    def test_profile_manifest_requires_provenance_for_every_numeric_leaf(self):
        manifest = load_profile_manifest(self.profile)
        self.assertEqual(manifest.runtime_profile["id"], "surface_motion_v1")
        self.assertIn("surface.sliding_friction_coefficient", manifest.parameter_sources)

        document = json.loads(self.profile.read_text(encoding="utf-8"))
        del document["parameter_sources"]["ball.mass_kg"]
        self.profile.write_text(
            json.dumps(document, indent=2, sort_keys=True) + "\n",
            encoding="utf-8",
        )
        with self.assertRaisesRegex(ValueError, "ball.mass_kg"):
            load_profile_manifest(self.profile)

    def test_profile_manifest_v2_accepts_extended_cue_numeric_provenance(self):
        document = json.loads(self.profile.read_text(encoding="utf-8"))
        document["schema_version"] = 2
        extended = {
            "normal_restitution": 0.0,
            "chalked_friction_coefficient": 0.6,
            "unchalked_friction_coefficient": 0.1,
            "maximum_reliable_offset_radius": 0.8,
            "cue_speed_per_power_unit_cm_s": 1.34,
        }
        for key, value in extended.items():
            document["runtime_profile"]["cue"][key] = value
            document["parameter_sources"][f"cue.{key}"] = {
                "evidence": "analytic cue profile v2 test fixture",
                "kind": "analytic_contract",
                "unit": "1",
            }
        self.profile.write_text(
            json.dumps(document, ensure_ascii=False, indent=2, sort_keys=True) + "\n",
            encoding="utf-8",
        )

        manifest = load_profile_manifest(self.profile)
        self.assertEqual(manifest.runtime_profile["cue"], {
            "effective_mass_kg": 0.5,
            **extended,
        })

    def test_production_cue_profile_is_analytic_only_and_fully_sourced(self):
        document = json.loads(PRODUCTION_CUE_PROFILE.read_text(encoding="utf-8"))
        manifest = load_profile_manifest(PRODUCTION_CUE_PROFILE)
        self.assertEqual(document["schema_version"], 2)
        self.assertEqual(manifest.runtime_profile["id"],
                         "chinese_pool_cue_contact_v1")
        self.assertEqual(manifest.runtime_profile["formula_version"],
                         "cue_contact_v1")
        self.assertTrue(manifest.applicability["analytic_contract_passed"])
        self.assertTrue(manifest.applicability["experimental_validation_blocked"])
        mapping = manifest.parameter_sources["cue.cue_speed_per_power_unit_cm_s"]
        self.assertFalse(mapping["experimental_validation"])
        for key in (
                "cue.chalked_friction_coefficient",
                "cue.unchalked_friction_coefficient",
                "cue.maximum_reliable_offset_radius"):
            self.assertEqual(manifest.parameter_sources[key]["kind"],
                             "preregistered_physical_hypothesis")

    def test_freeze_is_canonical_deterministic_and_verifiable(self):
        first = self._write()
        second = self._write(output=self.root / "regenerated.json")
        self.assertEqual(first.read_bytes(), second.read_bytes())

        freeze = load_candidate_freeze(first)
        self.assertEqual(freeze.candidate_id, "surface_motion_v1")
        self.assertEqual(
            [item["dataset_id"] for item in freeze.datasets],
            ["synthetic_reference"],
        )
        self.assertEqual(
            freeze.metric_targets,
            ({
                "lower": 17.55000305175782,
                "metric": "stopping_distance_cm",
                "point_id": "stop_distance_cal_01",
                "upper": 18.05000305175782,
            },),
        )
        freeze.verify(
            profile=self.profile,
            executable=self.executable,
            calibration_report=self.report,
            dataset_manifests=(DATASET_MANIFEST,),
        )

    def test_verification_rejects_changed_profile_or_report_bytes(self):
        freeze = load_candidate_freeze(self._write())
        with self.profile.open("ab") as output:
            output.write(b" ")
        with self.assertRaisesRegex(ValueError, "profile_sha256"):
            freeze.verify(
                profile=self.profile,
                executable=self.executable,
                calibration_report=self.report,
            )

        shutil.copyfile(FIXTURE_ROOT / "profile.json", self.profile)
        with self.report.open("ab") as output:
            output.write(b" ")
        with self.assertRaisesRegex(ValueError, "calibration_report_sha256"):
            freeze.verify(
                profile=self.profile,
                executable=self.executable,
                calibration_report=self.report,
            )

    def test_rejects_invalid_identity_schema_and_metric_thresholds(self):
        with self.assertRaisesRegex(ValueError, "source_revision"):
            self._write(source_revision="main")

        path = self._write()
        document = json.loads(path.read_text(encoding="utf-8"))
        document["unexpected"] = True
        path.write_text(json.dumps(document), encoding="utf-8")
        with self.assertRaisesRegex(ValueError, "keys"):
            load_candidate_freeze(path)

        report = json.loads(self.report.read_text(encoding="utf-8"))
        report["partitions"]["CALIBRATION"]["points"][0][
            "acceptance_interval"][0] = math.nan
        self.report.write_text(json.dumps(report), encoding="utf-8")
        with self.assertRaisesRegex(ValueError, "finite"):
            self._write(output=self.root / "nonfinite.json")

    def test_rejects_duplicate_dataset_manifests(self):
        with self.assertRaisesRegex(ValueError, "sorted and unique"):
            self._write(dataset_manifests=(DATASET_MANIFEST, DATASET_MANIFEST))


if __name__ == "__main__":
    unittest.main()
