from __future__ import annotations

import copy
import inspect
import json
from pathlib import Path
import re
import tempfile
import unittest
from unittest import mock

from scripts import (
    prepare_phase5_rules_decision_ui_v3_binding_corrected_ready as prepare,
)
from scripts import (
    run_phase5_rules_decision_ui_v3_binding_corrected_targeted_gpu_capture
    as capture,
)


READY_ROOT = (
    Path(__file__).resolve().parents[2] / "docs/evidence/phase5/"
    "rules-decision-ui-v3-binding-corrected-targeted-ready-v1")
READY_PATH = (
    READY_ROOT /
    "rules-decision-ui-v3-binding-corrected-targeted-ready.json")
AUTH_TEMPLATE_PATH = (
    READY_ROOT /
    "rules-decision-ui-v3-binding-corrected-authorization-template.json")


class Phase5RulesUiV3BindingCorrectedReadyTests(unittest.TestCase):
    def test_plan_remains_exact_20_repair_plus_8_controls(self) -> None:
        rows = prepare.plan_rows()
        self.assertEqual(len(rows), 28)
        self.assertEqual(len(set(rows)), 28)

    def test_capture_refuses_without_execute_flag_before_process(self) -> None:
        with mock.patch.object(capture.subprocess, "run") as process:
            with mock.patch("sys.argv", [
                "capture", "--ready", "missing",
                "--authorization", "missing",
                "--output-dir", "missing",
            ]):
                with self.assertRaisesRegex(RuntimeError, "refusing GPU"):
                    capture.main()
        process.assert_not_called()

    def test_wrapper_validation_precedes_process_and_output(self) -> None:
        source = inspect.getsource(capture.main)
        self.assertLess(source.index("validate_pre_process"),
                        source.index("subprocess.run"))
        self.assertNotIn("mkdir", source[:source.index("subprocess.run")])

    def test_template_covers_every_runner_required_field(self) -> None:
        if not AUTH_TEMPLATE_PATH.is_file():
            self.skipTest("fresh corrected readiness not generated")
        template = json.loads(AUTH_TEMPLATE_PATH.read_text(encoding="utf-8"))
        runner = (
            Path(__file__).resolve().parents[2] /
            "spikes/render_core_profile/"
            "phase5_rules_decision_ui_gpu_capture_runner.mm"
        ).read_text(encoding="utf-8")
        parsed = set(re.findall(r'receipt\.at\("([^"]+)"\)', runner))
        parsed.add("visual_repair_design_gate_sha256")
        self.assertTrue(capture.RUNNER_REQUIRED_AUTH_FIELDS.issubset(template))
        self.assertTrue(parsed.issubset(capture.RUNNER_REQUIRED_AUTH_FIELDS))
        self.assertNotIn("source_bindings_sha256", template)
        self.assertIn("source_binding_manifest_sha256", template)

    def signed_fixture(self, directory: Path) -> tuple[Path, Path, Path, dict, dict]:
        if not READY_PATH.is_file() or not AUTH_TEMPLATE_PATH.is_file():
            self.skipTest("fresh corrected readiness not generated")
        ready = json.loads(READY_PATH.read_text(encoding="utf-8"))
        auth = json.loads(AUTH_TEMPLATE_PATH.read_text(encoding="utf-8"))
        ready_path = directory / "ready.json"
        output = directory / "fresh-output"
        ready_path.write_text(json.dumps(ready), encoding="utf-8")
        auth.update({
            "authorized": True,
            "authorization_id": "offline-corrected-contract-test",
            "approved_by": "unit-test",
            "approved_at": "2099-01-01T00:00:00Z",
            "expires_unix_seconds": 4070908800,
            "authorized_output_canonical_path": str(output.resolve()),
            "targeted_visual_repair_gpu_authorized": True,
            "ready_manifest_sha256": capture.sha(ready_path),
        })
        auth_path = directory / "authorization.json"
        auth_path.write_text(json.dumps(auth), encoding="utf-8")
        return ready_path, auth_path, output, ready, auth

    def test_exact_corrected_authorization_passes_offline(self) -> None:
        with tempfile.TemporaryDirectory() as directory:
            ready_path, auth_path, output, ready, _ = self.signed_fixture(
                Path(directory))
            observed, _, paths = capture.validate_pre_process(
                ready_path, auth_path, output)
            self.assertEqual(observed["case_ids_in_order"],
                             ready["case_ids_in_order"])
            self.assertTrue(paths["hud_token"].is_file())
            self.assertTrue(paths["shell_token"].is_file())
            self.assertFalse(output.exists())

    def test_missing_runner_field_fails_closed(self) -> None:
        with tempfile.TemporaryDirectory() as directory:
            ready_path, auth_path, output, _, auth = self.signed_fixture(
                Path(directory))
            auth.pop("hud_token_sha256")
            auth_path.write_text(json.dumps(auth), encoding="utf-8")
            with mock.patch.object(capture.subprocess, "run") as process:
                with self.assertRaisesRegex(RuntimeError,
                                            "runner fields missing"):
                    capture.validate_pre_process(ready_path, auth_path, output)
            process.assert_not_called()
            self.assertFalse(output.exists())

    def test_live_token_sha_drift_fails_closed(self) -> None:
        for path_key, field in (("hud_token", "hud_token_sha256"),
                                ("shell_token", "shell_token_sha256")):
            with self.subTest(field=field), tempfile.TemporaryDirectory() as directory:
                ready_path, auth_path, output, ready, _ = self.signed_fixture(
                    Path(directory))
                token = Path(ready["paths"][path_key]).resolve()
                real_sha = capture.sha

                def observed_sha(path: Path) -> str:
                    return ("0" * 64 if Path(path).resolve() == token
                            else real_sha(path))

                with mock.patch.object(capture, "sha", side_effect=observed_sha):
                    with mock.patch.object(capture.subprocess, "run") as process:
                        with self.assertRaisesRegex(
                                RuntimeError,
                                "runtime binding mismatch: " + field):
                            capture.validate_pre_process(
                                ready_path, auth_path, output)
                process.assert_not_called()
                self.assertFalse(output.exists())

    def test_token_path_drift_fails_closed(self) -> None:
        for path_key, field in (("hud_token", "hud_token_sha256"),
                                ("shell_token", "shell_token_sha256")):
            with self.subTest(field=field), tempfile.TemporaryDirectory() as directory:
                temp = Path(directory)
                ready_path, auth_path, output, ready, auth = self.signed_fixture(temp)
                ready["paths"][path_key] = str(temp / ("missing-" + path_key))
                ready_path.write_text(json.dumps(ready), encoding="utf-8")
                auth["ready_manifest_sha256"] = capture.sha(ready_path)
                auth_path.write_text(json.dumps(auth), encoding="utf-8")
                with mock.patch.object(capture.subprocess, "run") as process:
                    with self.assertRaisesRegex(
                            RuntimeError,
                            "runtime binding mismatch: " + field):
                        capture.validate_pre_process(ready_path, auth_path, output)
                process.assert_not_called()
                self.assertFalse(output.exists())

    def test_source_manifest_token_row_drift_fails_closed(self) -> None:
        with tempfile.TemporaryDirectory() as directory:
            temp = Path(directory)
            ready_path, auth_path, output, ready, auth = self.signed_fixture(temp)
            original = Path(ready["paths"]["source_binding_manifest"])
            bindings = json.loads(original.read_text(encoding="utf-8"))
            hud_relative = next(iter(capture.TOKEN_ROWS))
            bindings["files"] = [
                row for row in bindings["files"]
                if row.get("path") != hud_relative
            ]
            manifest = temp / "source-bindings.json"
            manifest.write_text(json.dumps(bindings), encoding="utf-8")
            ready["paths"]["source_binding_manifest"] = str(manifest)
            digest = capture.sha(manifest)
            ready["frozen_bindings"]["source_binding_manifest_sha256"] = digest
            ready_path.write_text(json.dumps(ready), encoding="utf-8")
            auth["source_binding_manifest_sha256"] = digest
            auth["ready_manifest_sha256"] = capture.sha(ready_path)
            auth_path.write_text(json.dumps(auth), encoding="utf-8")
            with mock.patch.object(capture.subprocess, "run") as process:
                with self.assertRaisesRegex(RuntimeError,
                                            "token binding mismatch"):
                    capture.validate_pre_process(ready_path, auth_path, output)
            process.assert_not_called()
            self.assertFalse(output.exists())


if __name__ == "__main__":
    unittest.main()
