from __future__ import annotations

import copy
import json
import subprocess
import sys
import tempfile
import unittest
from pathlib import Path

from scripts.generate_phase5_rules_decision_ui_actions import (
    CAPABILITY_IDS,
    FLOW_IDS,
    MANIFEST_NAME,
    PROOF_NAME,
    SCHEMA_NAME,
    build_flat_proof,
    build_manifest,
    write_package,
)
from scripts.validate_phase5_rules_decision_ui_actions import validate_manifest


ROOT = Path(__file__).resolve().parents[2]
MANIFEST = ROOT / "docs/art" / MANIFEST_NAME


class Phase5RulesDecisionUiActionsTests(unittest.TestCase):
    def validate_value(self, value: dict) -> list[str]:
        with tempfile.TemporaryDirectory() as directory:
            output = Path(directory)
            write_package(output)
            manifest_path = output / MANIFEST_NAME
            manifest_path.write_text(
                json.dumps(value, indent=2, sort_keys=True) + "\n", encoding="utf-8")
            (output / PROOF_NAME).write_text(build_flat_proof(value), encoding="utf-8")
            return validate_manifest(value, manifest_path)

    def test_committed_package_is_valid_pending_and_exactly_scoped(self):
        value = json.loads(MANIFEST.read_text(encoding="utf-8"))
        self.assertEqual(validate_manifest(value, MANIFEST), [])
        self.assertEqual(tuple(item["capability_id"] for item in value["capabilities"]),
                         CAPABILITY_IDS)
        self.assertEqual(tuple(item["flow_id"] for item in value["flows"]), FLOW_IDS)
        self.assertEqual(value["status"], "PENDING")
        self.assertFalse(value["profile_complete"])
        self.assertFalse(value["product_exposed"])

    def test_generator_is_byte_deterministic_for_all_three_outputs(self):
        with tempfile.TemporaryDirectory() as first, tempfile.TemporaryDirectory() as second:
            write_package(Path(first))
            write_package(Path(second))
            for name in (MANIFEST_NAME, SCHEMA_NAME, PROOF_NAME):
                self.assertEqual((Path(first) / name).read_bytes(),
                                 (Path(second) / name).read_bytes())

    def test_pending_package_or_flow_cannot_be_promoted_or_exposed(self):
        value = build_manifest()
        value["status"] = "READY"
        value["profile_complete"] = True
        value["product_exposed"] = True
        value["flows"][0]["capability_state"] = "implemented"
        value["flows"][0]["product_exposed"] = True
        errors = self.validate_value(value)
        self.assertTrue(any("must remain PENDING" in error for error in errors))
        self.assertTrue(any("flow must remain pending" in error for error in errors))

    def test_missing_or_duplicate_action_id_is_rejected(self):
        missing = build_manifest()
        missing["flows"][0]["actions"].pop()
        self.assertTrue(any("exact action ID/order differs" in error
                            for error in self.validate_value(missing)))
        duplicate = build_manifest()
        duplicate["flows"][0]["actions"][1]["action_id"] = \
            duplicate["flows"][0]["actions"][0]["action_id"]
        self.assertTrue(any("action IDs missing or duplicate" in error
                            for error in self.validate_value(duplicate)))

    def test_authoritative_mutation_without_ack_is_rejected(self):
        value = build_manifest()
        value["flows"][0]["actions"][0]["requires_authority_ack"] = False
        errors = self.validate_value(value)
        self.assertTrue(any("authoritative mutation requires ACK" in error
                            for error in errors))

    def test_optimistic_authoritative_mutation_is_rejected(self):
        value = build_manifest()
        value["flows"][1]["actions"][0]["optimistic_authoritative_mutation"] = True
        errors = self.validate_value(value)
        self.assertTrue(any("optimistic authoritative mutation forbidden" in error
                            for error in errors))

    def test_missing_bilingual_copy_or_action_label_is_rejected(self):
        value = build_manifest()
        value["flows"][0]["copy"]["body"]["zh_cn"] = ""
        value["flows"][0]["actions"][0]["label"]["en"] = ""
        errors = self.validate_value(value)
        self.assertTrue(any("bilingual title/body missing" in error for error in errors))
        self.assertTrue(any("action bilingual label missing" in error for error in errors))

    def test_unknown_shape_token_is_rejected(self):
        value = build_manifest()
        value["flows"][0]["shape_tokens"] = ["new_unreviewed_warning_icon"]
        errors = self.validate_value(value)
        self.assertTrue(any("unknown or missing existing shape token" in error
                            for error in errors))

    def test_hierarchy_input_focus_cancel_timeout_and_resume_drift_is_rejected(self):
        value = build_manifest()
        value["interaction_policy"]["focus"]["modal_traps_focus"] = False
        value["interaction_policy"]["input"]["escape_on_required_decision"] = "dismiss"
        value["interaction_policy"]["timeout_and_resume"]["auto_select_action"] = True
        errors = self.validate_value(value)
        self.assertTrue(any("hierarchy, focus, input, cancel, timeout, or resume" in error
                            for error in errors))

    def test_timeout_cancel_and_ack_must_remain_fail_closed(self):
        value = build_manifest()
        failure = value["flows"][0]["failure_policy"]
        failure["default_action_on_timeout"] = "accept_table"
        failure["dismiss_on_escape"] = True
        failure["advance_turn_without_ack"] = True
        errors = self.validate_value(value)
        self.assertTrue(any("fail-closed timeout/cancel/ACK policy differs" in error
                            for error in errors))

    def test_stalemate_policy_placeholder_cannot_guess_an_answer(self):
        value = build_manifest()
        policy = value["policy_decision_placeholders"][0]
        policy["status"] = "RESOLVED"
        policy["assumed_answer"] = "local client decides"
        policy["product_exposed"] = True
        errors = self.validate_value(value)
        self.assertTrue(any("must remain unresolved and answer-free" in error
                            for error in errors))

    def test_off_table_official_conflict_cannot_be_claimed_resolved(self):
        value = build_manifest()
        value["evidence_boundary"]["official_off_table_conflict_resolved"] = True
        value["policy_decision_placeholders"][2]["status"] = "RESOLVED"
        value["policy_decision_placeholders"][2]["assumed_answer"] = "prefer 17(e)"
        errors = self.validate_value(value)
        self.assertTrue(any("evidence boundary differs" in error for error in errors))
        self.assertTrue(any("must remain unresolved and answer-free" in error
                            for error in errors))

    def test_frozen_copy_action_machine_and_text_binding_drift_is_rejected(self):
        value = build_manifest()
        value["flows"][0]["actions"][0]["frozen_copy_action_id"] = "invented_action"
        value["flows"][1]["frozen_copy_machine_code"] = "ui.invented"
        errors = self.validate_value(value)
        self.assertTrue(any("unknown frozen-copy action ID" in error for error in errors))
        self.assertTrue(any("machine/text binding differs" in error for error in errors))

    def test_schema_freezes_pending_and_not_exposed_constants(self):
        schema = json.loads((ROOT / "docs/art" / SCHEMA_NAME).read_text(encoding="utf-8"))
        self.assertEqual(schema["properties"]["status"]["const"], "PENDING")
        self.assertEqual(schema["properties"]["profile_complete"]["const"], False)
        self.assertEqual(schema["properties"]["product_exposed"]["const"], False)
        flow_properties = schema["properties"]["flows"]["items"]["properties"]
        self.assertEqual(flow_properties["capability_state"]["const"], "pending")
        self.assertEqual(flow_properties["product_exposed"]["const"], False)

    def test_flat_proof_is_exact_and_explicitly_not_gpu_or_runtime(self):
        value = build_manifest()
        with tempfile.TemporaryDirectory() as directory:
            output = Path(directory)
            write_package(output)
            proof_path = output / PROOF_NAME
            proof = proof_path.read_text(encoding="utf-8")
            self.assertIn("NOT GPU", proof)
            self.assertIn("NOT RUNTIME", proof)
            self.assertIn("PRODUCT_EXPOSED=FALSE", proof)
            proof_path.write_text(proof.replace("NOT GPU", "GPU CAPTURE"), encoding="utf-8")
            errors = validate_manifest(value, output / MANIFEST_NAME)
            self.assertTrue(any("flat proof" in error for error in errors))

    def test_validator_cli_reports_fail_closed_evidence_boundary(self):
        result = subprocess.run(
            [sys.executable,
             str(ROOT / "scripts/validate_phase5_rules_decision_ui_actions.py"),
             str(MANIFEST)], cwd=ROOT, text=True, capture_output=True, check=False)
        self.assertEqual(result.returncode, 0, result.stderr)
        self.assertIn("capabilities=6 flows=7", result.stdout)
        self.assertIn("status=PENDING product_exposed=false profile_complete=false", result.stdout)
        self.assertIn("proof=FLAT_NOT_GPU_NOT_RUNTIME", result.stdout)


if __name__ == "__main__":
    unittest.main()
