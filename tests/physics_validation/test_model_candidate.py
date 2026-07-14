import json
import math
import shutil
import tempfile
import unittest
from contextlib import chdir
from pathlib import Path

from tools.physics_validation.freeze_candidate import main as freeze_main
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
PRODUCTION_BALL_COLLISION_PROFILE = Path(__file__).parents[2] / (
    "physics_models/profiles/chinese_pool_ball_collision_v1.json")
PRODUCTION_CUSHION_PROFILE = Path(__file__).parents[2] / (
    "physics_models/profiles/chinese_pool_cushion_collision_v1.json")


class ModelCandidateTests(unittest.TestCase):
    def setUp(self):
        self.temporary = tempfile.TemporaryDirectory()
        self.addCleanup(self.temporary.cleanup)
        self.root = Path(self.temporary.name)
        self.profile = self.root / "profile.json"
        self.report = self.root / "calibration_report.json"
        self.manifest = self.root / "manifest.json"
        shutil.copyfile(FIXTURE_ROOT / "profile.json", self.profile)
        shutil.copyfile(FIXTURE_ROOT / "calibration_report.json", self.report)
        shutil.copyfile(DATASET_MANIFEST, self.manifest)
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

    def _second_dataset(self):
        manifest = json.loads(DATASET_MANIFEST.read_text(encoding="utf-8"))
        manifest["dataset_id"] = "synthetic_reference_two"
        manifest_path = self.root / "second_manifest.json"
        manifest_path.write_text(
            json.dumps(manifest, indent=2, sort_keys=True) + "\n",
            encoding="utf-8",
        )
        report = json.loads(self.report.read_text(encoding="utf-8"))
        report["metadata"]["dataset_id"] = "synthetic_reference_two"
        report_path = self.root / "second_report.json"
        report_path.write_text(
            json.dumps(report, indent=2, sort_keys=True) + "\n",
            encoding="utf-8",
        )
        return manifest_path, report_path

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

    def test_profile_manifest_v3_requires_ball_contact_numeric_provenance(self):
        document = json.loads(self.profile.read_text(encoding="utf-8"))
        document["schema_version"] = 3
        extended = {
            "inertia_factor": 0.4,
            "normal_restitution": 0.91,
            "friction_coefficient": 0.08,
        }
        for key, value in extended.items():
            document["runtime_profile"]["ball"][key] = value
            document["parameter_sources"][f"ball.{key}"] = {
                "evidence": "ball collision profile v3 test fixture",
                "kind": "analytic_contract",
                "unit": "1",
            }
        document["runtime_profile"]["cue"].update({
            "normal_restitution": 0.0,
            "chalked_friction_coefficient": 0.6,
            "unchalked_friction_coefficient": 0.1,
            "maximum_reliable_offset_radius": 0.8,
            "cue_speed_per_power_unit_cm_s": 1.34,
        })
        for key in set(document["runtime_profile"]["cue"]) - {"effective_mass_kg"}:
            document["parameter_sources"][f"cue.{key}"] = {
                "evidence": "cue profile v2 compatibility fixture",
                "kind": "compatibility_default",
                "unit": "1",
            }
        self.profile.write_text(
            json.dumps(document, ensure_ascii=False, indent=2, sort_keys=True) + "\n",
            encoding="utf-8",
        )

        manifest = load_profile_manifest(self.profile)
        self.assertEqual(manifest.runtime_profile["ball"], {
            "mass_kg": 0.17,
            "material": "phenolic_resin",
            "radius_cm": 2.8575,
            **extended,
        })

        del document["parameter_sources"]["ball.normal_restitution"]
        self.profile.write_text(
            json.dumps(document, ensure_ascii=False, indent=2, sort_keys=True) + "\n",
            encoding="utf-8",
        )
        with self.assertRaisesRegex(ValueError, "ball.normal_restitution"):
            load_profile_manifest(self.profile)

    def test_profile_manifest_v4_requires_cushion_geometry_and_domain_provenance(self):
        document = json.loads(
            PRODUCTION_BALL_COLLISION_PROFILE.read_text(encoding="utf-8"))
        document["schema_version"] = 4
        extended = {
            "maximum_rigid_incident_speed_cm_s": 250.0,
            "nose_height_ratio": 1.4,
        }
        document["runtime_profile"]["cushion"].update({
            **extended,
            "material": "riley_snooker_cushion",
        })
        for key in extended:
            document["parameter_sources"][f"cushion.{key}"] = {
                "evidence": "Mathavan 2010 cushion profile fixture",
                "kind": "source_apparatus",
                "unit": "cm/s" if "speed" in key else "1",
            }
        path = self.root / "profile_v4.json"
        path.write_text(
            json.dumps(document, ensure_ascii=False, indent=2, sort_keys=True) + "\n",
            encoding="utf-8",
        )

        manifest = load_profile_manifest(path)
        self.assertEqual(manifest.runtime_profile["cushion"], {
            "friction_coefficient": 0.0,
            "material": "riley_snooker_cushion",
            "maximum_rigid_incident_speed_cm_s": 250.0,
            "normal_restitution": 1.0,
            "nose_height_ratio": 1.4,
        })
        previous = load_profile_manifest(PRODUCTION_BALL_COLLISION_PROFILE)
        for section in ("ball", "cue", "surface", "solver"):
            self.assertEqual(manifest.runtime_profile[section],
                             previous.runtime_profile[section])
        del document["parameter_sources"]["cushion.nose_height_ratio"]
        path.write_text(
            json.dumps(document, ensure_ascii=False, indent=2, sort_keys=True) + "\n",
            encoding="utf-8",
        )
        with self.assertRaisesRegex(ValueError, "cushion.nose_height_ratio"):
            load_profile_manifest(path)

    def test_profile_manifest_v5_requires_pocket_boundary_provenance(self):
        document = json.loads(
            PRODUCTION_CUSHION_PROFILE.read_text(encoding="utf-8"))
        document["schema_version"] = 5
        boundary = {
            "capture_depth_cm": 6.0,
            "corner_mouth_width_cm": 13.2,
            "corner_throat_width_cm": 10.0,
            "geometry_id": "wpa_chinese_pool_v1",
            "jaw_radius_cm": 2.5,
            "material": "competition_table_boundary",
            "playfield_length_cm": 254.0,
            "playfield_width_cm": 127.0,
            "side_mouth_width_cm": 8.6,
            "side_throat_width_cm": 7.0,
            "throat_depth_cm": 3.0,
        }
        document["runtime_profile"]["table_boundary"] = boundary
        for key, value in boundary.items():
            if isinstance(value, (int, float)):
                document["parameter_sources"][f"table_boundary.{key}"] = {
                    "evidence": "committed competition geometry fixture",
                    "kind": "equipment_specification",
                    "unit": "cm",
                }
        path = self.root / "profile_v5.json"
        path.write_text(
            json.dumps(document, ensure_ascii=False, indent=2, sort_keys=True) + "\n",
            encoding="utf-8")
        manifest = load_profile_manifest(path)
        self.assertEqual(manifest.runtime_profile["table_boundary"], boundary)
        del document["parameter_sources"]["table_boundary.jaw_radius_cm"]
        path.write_text(
            json.dumps(document, ensure_ascii=False, indent=2, sort_keys=True) + "\n",
            encoding="utf-8")
        with self.assertRaisesRegex(ValueError, "table_boundary.jaw_radius_cm"):
            load_profile_manifest(path)

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

    def test_production_ball_collision_profile_is_calibration_only_and_fully_sourced(self):
        document = json.loads(
            PRODUCTION_BALL_COLLISION_PROFILE.read_text(encoding="utf-8"))
        manifest = load_profile_manifest(PRODUCTION_BALL_COLLISION_PROFILE)
        self.assertEqual(document["schema_version"], 3)
        self.assertEqual(manifest.runtime_profile["id"],
                         "chinese_pool_ball_collision_v1")
        self.assertEqual(manifest.runtime_profile["formula_version"],
                         "ball_collision_v1")
        self.assertEqual(manifest.runtime_profile["ball"], {
            "friction_coefficient": 0.25,
            "inertia_factor": 0.4,
            "mass_kg": 0.17,
            "material": "phenolic_resin",
            "normal_restitution": 0.36,
            "radius_cm": 2.8575,
        })
        self.assertTrue(manifest.applicability["analytic_gates_passed"])
        self.assertTrue(manifest.applicability[
            "experimental_validation_pending"])
        self.assertFalse(manifest.applicability[
            "real_world_validation_claimed"])
        for key in (
                "ball.normal_restitution", "ball.friction_coefficient"):
            source = manifest.parameter_sources[key]
            self.assertEqual(source["kind"], "calibration_transfer")
            self.assertEqual(source["dataset_id"],
                             "domenech_2023_ball_collision")
            self.assertEqual(source["partition"], "CALIBRATION")
            self.assertIn("PVC", source["limitation"])

    def test_production_cushion_profile_is_calibration_only_and_fully_sourced(self):
        document = json.loads(PRODUCTION_CUSHION_PROFILE.read_text(encoding="utf-8"))
        manifest = load_profile_manifest(PRODUCTION_CUSHION_PROFILE)
        self.assertEqual(document["schema_version"], 4)
        self.assertEqual(manifest.runtime_profile["id"],
                         "chinese_pool_cushion_collision_v1")
        self.assertEqual(manifest.runtime_profile["formula_version"],
                         "cushion_collision_v1")
        self.assertEqual(manifest.runtime_profile["cushion"], {
            "friction_coefficient": 0.14,
            "material": "riley_renaissance_snooker_cushion",
            "maximum_rigid_incident_speed_cm_s": 250.0,
            "normal_restitution": 0.9248723120650503,
            "nose_height_ratio": 1.4,
        })
        self.assertFalse(manifest.applicability["real_world_validation_claimed"])
        self.assertTrue(manifest.applicability["experimental_validation_pending"])
        for key in ("cushion.normal_restitution", "cushion.friction_coefficient"):
            source = manifest.parameter_sources[key]
            self.assertEqual(source["dataset_id"], "mathavan_2010_cushion")
            self.assertEqual(source["partition"], "CALIBRATION")
        self.assertEqual(
            manifest.parameter_sources["cushion.maximum_rigid_incident_speed_cm_s"]
            ["kind"], "source_domain_boundary")

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

    def test_schema_v2_binds_two_reports_and_supplemental_artifact_deterministically(self):
        second_manifest, second_report = self._second_dataset()
        fit = self.root / "material_fit.json"
        fit.write_text('{"full_precision":true}\n', encoding="utf-8")
        common = {
            "calibration_report": None,
            "calibration_reports": (second_report, self.report),
            "dataset_manifests": (second_manifest, self.manifest),
            "supplemental_artifacts": (fit,),
            "repository_root": self.root,
        }
        first = self._write(**common)
        second = self._write(
            output=self.root / "regenerated_v2.json",
            **{
                **common,
                "calibration_reports": tuple(reversed(common["calibration_reports"])),
                "dataset_manifests": tuple(reversed(common["dataset_manifests"])),
            },
        )
        self.assertEqual(first.read_bytes(), second.read_bytes())

        freeze = load_candidate_freeze(first)
        self.assertEqual(freeze.schema_version, 2)
        self.assertEqual(
            [item["dataset_id"] for item in freeze.calibration_reports],
            ["synthetic_reference", "synthetic_reference_two"],
        )
        self.assertEqual(
            freeze.supplemental_artifacts[0]["path"], "material_fit.json")
        freeze.verify(
            profile=self.profile,
            executable=self.executable,
            calibration_reports=(self.report, second_report),
            dataset_manifests=(self.manifest, second_manifest),
            supplemental_artifacts=(fit,),
            repository_root=self.root,
        )

    def test_schema_v2_rejects_duplicate_report_identity(self):
        fit = self.root / "material_fit.json"
        fit.write_text('{}\n', encoding="utf-8")
        with self.assertRaisesRegex(ValueError, "calibration reports.*unique"):
            self._write(
                calibration_report=None,
                calibration_reports=(self.report, self.report),
                dataset_manifests=(self.manifest,),
                supplemental_artifacts=(fit,),
                repository_root=self.root,
            )

    def test_schema_v2_rejects_changed_report_or_supplemental_bytes(self):
        second_manifest, second_report = self._second_dataset()
        fit = self.root / "material_fit.json"
        fit.write_text('{}\n', encoding="utf-8")
        path = self._write(
            calibration_report=None,
            calibration_reports=(self.report, second_report),
            dataset_manifests=(self.manifest, second_manifest),
            supplemental_artifacts=(fit,),
            repository_root=self.root,
        )
        freeze = load_candidate_freeze(path)
        second_report.write_text(
            second_report.read_text(encoding="utf-8") + " ", encoding="utf-8")
        with self.assertRaisesRegex(ValueError, "calibration reports"):
            freeze.verify(
                profile=self.profile,
                executable=self.executable,
                calibration_reports=(self.report, second_report),
                dataset_manifests=(self.manifest, second_manifest),
                supplemental_artifacts=(fit,),
                repository_root=self.root,
            )

        _, second_report = self._second_dataset()
        fit.write_text('{"changed":true}\n', encoding="utf-8")
        with self.assertRaisesRegex(ValueError, "supplemental artifacts"):
            freeze.verify(
                profile=self.profile,
                executable=self.executable,
                calibration_reports=(self.report, second_report),
                dataset_manifests=(self.manifest, second_manifest),
                supplemental_artifacts=(fit,),
                repository_root=self.root,
            )

    def test_freeze_cli_accepts_repeatable_v2_inputs_in_create_and_verify_modes(self):
        second_manifest, second_report = self._second_dataset()
        fit = self.root / "material_fit.json"
        fit.write_text('{}\n', encoding="utf-8")
        output = self.root / "cli_freeze.json"
        shared = [
            "--profile", str(self.profile),
            "--executable", str(self.executable),
            "--calibration-report", str(second_report),
            "--calibration-report", str(self.report),
            "--dataset-manifest", str(second_manifest),
            "--dataset-manifest", str(self.manifest),
            "--supplemental-artifact", str(fit),
        ]
        with chdir(self.root):
            self.assertEqual(freeze_main([
                "--candidate-id", "surface_motion_v1",
                "--formula-version", "legacy_v1",
                "--source-revision",
                "0123456789abcdef0123456789abcdef01234567",
                "--created-at", "2026-07-14T00:00:00Z",
                "--output", str(output),
                *shared,
            ]), 0)
            self.assertEqual(freeze_main([
                "--verify", str(output),
                *shared,
            ]), 0)


if __name__ == "__main__":
    unittest.main()
