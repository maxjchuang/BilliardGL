from __future__ import annotations

import copy
import json
import subprocess
import sys
import tempfile
import unittest
from pathlib import Path

from scripts.generate_phase5_rules_copy_package import (
    MANIFEST_NAME, NOTICE_DATA, PROOF_NAME, build_manifest, write_package,
)
from scripts.validate_phase5_rules_copy_package import validate_manifest


ROOT = Path(__file__).resolve().parents[2]
MANIFEST = ROOT / "docs/art" / MANIFEST_NAME


class Phase5RulesCopyPackageTests(unittest.TestCase):
    def validate_value(self, value: dict):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            write_package(root)
            path = root / MANIFEST_NAME
            path.write_text(json.dumps(value, indent=2, sort_keys=True) + "\n", encoding="utf-8")
            return validate_manifest(value, path)

    def test_committed_package_covers_exact_eight_codes_and_four_pending_ui(self):
        value = json.loads(MANIFEST.read_text(encoding="utf-8"))
        self.assertEqual(validate_manifest(value, MANIFEST), [])
        self.assertEqual([item["reason_code"] for item in value["reason_notices"]],
                         [item[0] for item in NOTICE_DATA])
        self.assertEqual(len(value["pending_ui_capabilities"]), 4)
        self.assertFalse(value["profile_complete"])

    def test_generator_is_byte_deterministic(self):
        with tempfile.TemporaryDirectory() as first, tempfile.TemporaryDirectory() as second:
            write_package(Path(first)); write_package(Path(second))
            for name in (MANIFEST_NAME, "phase5-rules-shipping-copy-v1.schema.json", PROOF_NAME):
                self.assertEqual((Path(first) / name).read_bytes(), (Path(second) / name).read_bytes())

    def test_copy_has_bilingual_title_explanation_action_and_semantics(self):
        value = build_manifest()
        for notice in value["reason_notices"]:
            for locale in ("en", "zh_cn"):
                self.assertTrue(all(notice["copy"][locale][field]
                                    for field in ("title", "explanation", "next_action")))
            self.assertEqual(set(notice["semantic_effect"]),
                             {"ball_in_hand", "turn", "handoff", "win", "loss"})

    def test_missing_machine_code_is_rejected(self):
        value = build_manifest()
        value["reason_notices"].pop()
        errors = self.validate_value(value)
        self.assertTrue(any("eight unique machine codes" in error for error in errors))

    def test_duplicate_text_key_is_rejected(self):
        value = build_manifest()
        value["reason_notices"][1]["text_key"] = value["reason_notices"][0]["text_key"]
        errors = self.validate_value(value)
        self.assertTrue(any("text keys must be unique" in error for error in errors))

    def test_pending_capability_cannot_be_product_exposed(self):
        value = build_manifest()
        value["pending_ui_capabilities"][0]["product_exposed"] = True
        errors = self.validate_value(value)
        self.assertTrue(any("not product-exposed" in error for error in errors))

    def test_turn_ball_in_hand_handoff_win_loss_semantics_are_exact(self):
        value = build_manifest()
        value["reason_notices"][2]["semantic_effect"]["turn"] = "continues"
        errors = self.validate_value(value)
        self.assertTrue(any("semantics differ" in error for error in errors))

    def test_color_only_encoding_is_rejected(self):
        value = build_manifest()
        value["reason_notices"][0]["redundant_channels"] = ["color"]
        value["presentation_policy"]["accessibility"]["never_color_only"] = False
        errors = self.validate_value(value)
        self.assertTrue(any("color only" in error for error in errors))
        self.assertTrue(any("shape plus text" in error for error in errors))

    def test_pending_decision_without_action_is_rejected(self):
        value = build_manifest()
        value["pending_ui_capabilities"][0]["actions"] = []
        errors = self.validate_value(value)
        self.assertTrue(any("at least one action" in error for error in errors))

    def test_missing_chinese_or_english_localization_is_rejected(self):
        value = build_manifest()
        value["reason_notices"][0]["copy"]["zh_cn"]["explanation"] = ""
        value["pending_ui_capabilities"][0]["title"]["en"] = ""
        errors = self.validate_value(value)
        self.assertTrue(any("English/Chinese" in error for error in errors))
        self.assertTrue(any("pending UI localization" in error for error in errors))

    def test_missing_next_action_is_rejected(self):
        value = build_manifest()
        value["reason_notices"][3]["copy"]["en"]["next_action"] = ""
        errors = self.validate_value(value)
        self.assertTrue(any("English/Chinese" in error or "next action" in error for error in errors))

    def test_unreviewed_shape_alias_is_rejected(self):
        value = build_manifest()
        value["reason_notices"][0]["runtime_shape_alias"] = "new_unreviewed_alias"
        errors = self.validate_value(value)
        self.assertTrue(any("unreviewed" in error for error in errors))

    def test_outcome_copy_requires_all_five_bilingual_states(self):
        value = build_manifest()
        del value["outcome_copy"]["turn_handoff"]
        errors = self.validate_value(value)
        self.assertTrue(any("outcome copy coverage" in error for error in errors))

    def test_profile_completion_or_runtime_change_claim_is_rejected(self):
        value = build_manifest()
        value["profile_complete"] = True
        value["evidence_boundary"]["changes_runtime"] = True
        errors = self.validate_value(value)
        self.assertTrue(any("explicitly incomplete" in error for error in errors))
        self.assertTrue(any("evidence boundary" in error for error in errors))

    def test_flat_proof_is_exact_and_explicitly_not_gpu(self):
        value = build_manifest()
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            write_package(root)
            proof = (root / PROOF_NAME).read_text(encoding="utf-8")
            self.assertIn("FLAT TEXT/LAYOUT PROOF", proof)
            self.assertIn("NOT GPU", proof)
            (root / PROOF_NAME).write_text(proof.replace("NOT GPU", "GPU CAPTURE"), encoding="utf-8")
            errors = validate_manifest(value, root / MANIFEST_NAME)
            self.assertTrue(any("flat proof" in error for error in errors))

    def test_validator_cli_reports_non_gpu_and_incomplete_profile(self):
        result = subprocess.run(
            [sys.executable, str(ROOT / "scripts/validate_phase5_rules_copy_package.py"),
             str(MANIFEST)], cwd=ROOT, text=True, capture_output=True, check=False)
        self.assertEqual(result.returncode, 0, result.stderr)
        self.assertIn("reason_codes=8 pending_ui=4", result.stdout)
        self.assertIn("profile_complete=false proof=FLAT_NOT_GPU", result.stdout)


if __name__ == "__main__":
    unittest.main()
