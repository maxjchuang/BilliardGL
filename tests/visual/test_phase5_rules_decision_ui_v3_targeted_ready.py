from __future__ import annotations

import copy
import inspect
import json
from pathlib import Path
import tempfile
import unittest
from unittest import mock

from scripts import prepare_phase5_rules_decision_ui_v3_targeted_ready as prepare
from scripts import run_phase5_rules_decision_ui_v3_targeted_gpu_capture as capture


class Phase5RulesDecisionUiV3TargetedReadyTests(unittest.TestCase):
    def test_plan_is_exact_20_repair_plus_8_controls(self) -> None:
        rows = prepare.plan_rows()
        ids = [row.split("\t", 1)[0] for row in rows]
        self.assertEqual(len(ids), 28)
        self.assertEqual(len(set(ids)), 28)
        self.assertEqual(ids[:20], [
            "group_choice_armed_confirm--p1--zh_cn",
            "group_choice_armed_confirm--p2--en",
            "group_choice_armed_confirm--p3--zh_cn",
            "group_choice_armed_confirm--p4--en",
            "full_table_preview_valid--p1--en",
            "full_table_preview_valid--p2--zh_cn",
            "full_table_preview_valid--p3--en",
            "full_table_preview_valid--p4--zh_cn",
            "full_table_preview_invalid--p1--zh_cn",
            "full_table_preview_invalid--p2--en",
            "full_table_preview_invalid--p3--zh_cn",
            "full_table_preview_invalid--p4--en",
            "behind_head_string_preview_valid--p1--en",
            "behind_head_string_preview_valid--p2--zh_cn",
            "behind_head_string_preview_valid--p3--en",
            "behind_head_string_preview_valid--p4--zh_cn",
            "behind_head_string_preview_invalid--p1--zh_cn",
            "behind_head_string_preview_invalid--p2--en",
            "behind_head_string_preview_invalid--p3--zh_cn",
            "behind_head_string_preview_invalid--p4--en",
        ])

    def test_capture_refuses_without_execute_flag_before_process(self) -> None:
        with mock.patch.object(capture.subprocess, "run") as process:
            with mock.patch("sys.argv", ["capture", "--ready", "missing",
                    "--authorization", "missing", "--output-dir", "missing"]):
                with self.assertRaisesRegex(RuntimeError, "refusing GPU"):
                    capture.main()
        process.assert_not_called()

    def test_bad_authorization_rejects_without_output_or_process(self) -> None:
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            ready_path = root / "ready.json"
            auth_path = root / "auth.json"
            output = root / "output"
            ready_path.write_text("{}", encoding="utf-8")
            auth_path.write_text("{}", encoding="utf-8")
            with mock.patch.object(capture.subprocess, "run") as process:
                with self.assertRaisesRegex(RuntimeError, "readiness"):
                    capture.validate_pre_process(ready_path, auth_path, output)
            process.assert_not_called()
            self.assertFalse(output.exists())

    def test_capture_validation_precedes_process_and_output(self) -> None:
        source = inspect.getsource(capture.main)
        self.assertLess(source.index("validate_pre_process"),
                        source.index("subprocess.run"))
        self.assertNotIn("mkdir", source[:source.index("subprocess.run")])

    def test_exact_authorization_contract_accepts_only_bound_28(self) -> None:
        root = (Path(__file__).resolve().parents[2] / "docs/evidence/phase5/"
                "rules-decision-ui-v3-targeted-ready-v1")
        ready_path = root / "rules-decision-ui-v3-targeted-ready.json"
        template_path = (
            root / "rules-decision-ui-v3-targeted-authorization-template.json")
        if not ready_path.is_file() or not template_path.is_file():
            self.skipTest("final readiness is generated after this source freezes")
        ready = json.loads(ready_path.read_text(encoding="utf-8"))
        auth = json.loads(template_path.read_text(encoding="utf-8"))
        with tempfile.TemporaryDirectory() as directory:
            temp = Path(directory)
            output = temp / "fresh-output"
            auth.update({
                "authorized": True,
                "authorization_id": "test-v3-exact-28",
                "approved_by": "unit-test",
                "approved_at": "2099-01-01T00:00:00Z",
                "expires_unix_seconds": 4070908800,
                "authorized_output_canonical_path": str(output.resolve()),
                "targeted_visual_repair_gpu_authorized": True,
                "ready_manifest_sha256": capture.sha(ready_path),
            })
            auth_path = temp / "authorization.json"
            auth_path.write_text(json.dumps(auth), encoding="utf-8")
            observed_ready, _, _ = capture.validate_pre_process(
                ready_path, auth_path, output)
            self.assertEqual(observed_ready["case_ids_in_order"],
                             ready["case_ids_in_order"])
            changed = copy.deepcopy(auth)
            changed["scene_ids_in_order"] = changed["scene_ids_in_order"][:-1]
            auth_path.write_text(json.dumps(changed), encoding="utf-8")
            with self.assertRaisesRegex(RuntimeError, "exact scope"):
                capture.validate_pre_process(ready_path, auth_path, output)


if __name__ == "__main__":
    unittest.main()
