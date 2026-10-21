from __future__ import annotations

import copy
import json
import sys
import tempfile
import unittest
from pathlib import Path


ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT))

from scripts.validate_phase5_rules_decision_ui_v4_real_input_accessibility_external_gate import (  # noqa: E402
    CATEGORY_COUNTS,
    DEFAULT_GATE,
    DEFAULT_TEMPLATE,
    DEVICE_IDS,
    GATE_SHA256,
    SCREEN_READERS,
    SUBJECT_IDS,
    expected_ta_prerequisite,
    load_json,
    validate,
    validate_completed,
    validate_gate,
    validate_template_common,
    validate_template_not_run,
)


class Phase5RulesDecisionUiV4ExternalGateTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls) -> None:
        cls.gate = load_json(DEFAULT_GATE)
        cls.template = load_json(DEFAULT_TEMPLATE)
        cls.catalog = cls.gate["protocol_catalog"]

    def assert_template_not_run_rejected(self, template) -> None:
        validate_template_common(template, self.catalog)
        with self.assertRaises(RuntimeError):
            validate_template_not_run(template)

    def test_committed_gate_and_template_validate_not_run(self) -> None:
        result = validate(DEFAULT_GATE, DEFAULT_TEMPLATE)
        self.assertEqual(result["status"],
                         "PASS_EXTERNAL_SESSION_TEMPLATE_NOT_RUN")
        self.assertEqual(result["gate_sha256"], GATE_SHA256)
        self.assertEqual(result["participants"], 12)
        self.assertEqual(result["devices"], 4)
        self.assertEqual(result["screen_reader_families"], 2)
        self.assertEqual(result["protocol_items"], 30)
        self.assertEqual(result["items_not_run"], 30)
        self.assertFalse(result["targeted16_ta_pass_bound"])
        self.assertFalse(result["external_human_review_pass"])

    def test_gate_binds_v3_manual_and_v4_exact_shape_gate(self) -> None:
        catalog = validate_gate(self.gate)
        self.assertEqual(len(catalog), 30)
        bindings = self.gate["read_only_bindings"]
        self.assertEqual(len(bindings), 2)
        self.assertIn("f044e39c", bindings[0]["sha256"])
        self.assertIn("f93ca596", bindings[1]["sha256"])
        gate = copy.deepcopy(self.gate)
        gate["read_only_bindings"][0]["sha256"] = "0" * 64
        with self.assertRaises(RuntimeError):
            validate_gate(gate)

    def test_future_targeted16_ta_sha_remains_nonexistent_and_unbound(self) -> None:
        expected = expected_ta_prerequisite()
        self.assertEqual(self.template[
            "future_targeted16_ta_pass_binding"], expected)
        self.assertIsNone(expected["manual_path"])
        self.assertIsNone(expected["manual_sha256"])
        template = copy.deepcopy(self.template)
        template["future_targeted16_ta_pass_binding"]["manual_sha256"] = (
            "0" * 64)
        self.assert_template_not_run_rejected(template)

    def test_exact_12_subject_locale_cvd_and_screen_reader_slots(self) -> None:
        participants = self.template["participant_slots"]
        self.assertEqual([item["slot_id"] for item in participants],
                         SUBJECT_IDS)
        self.assertEqual([item["locale"] for item in participants].count("en"),
                         6)
        self.assertEqual([item["locale"] for item in participants].count(
            "zh_cn"), 6)
        self.assertEqual([item["vision_profile"] for item in participants].count(
            "cvd_protan"), 2)
        self.assertEqual([item["vision_profile"] for item in participants].count(
            "cvd_deutan"), 2)
        template = copy.deepcopy(self.template)
        template["participant_slots"][2]["vision_profile"] = "standard"
        with self.assertRaises(RuntimeError):
            validate_template_common(template, self.catalog)

    def test_exact_four_physical_devices_and_two_screen_reader_families(self) -> None:
        self.assertEqual([item["slot_id"] for item in
                          self.template["device_slots"]], DEVICE_IDS)
        self.assertEqual([item["family"] for item in self.template[
            "screen_reader_configuration_slots"]], SCREEN_READERS)
        template = copy.deepcopy(self.template)
        template["device_slots"].pop()
        with self.assertRaises(RuntimeError):
            validate_template_common(template, self.catalog)
        template = copy.deepcopy(self.template)
        template["screen_reader_configuration_slots"][1]["family"] = (
            "VoiceOver")
        with self.assertRaises(RuntimeError):
            validate_template_common(template, self.catalog)

    def test_exact_30_item_category_inventory(self) -> None:
        counts = {}
        for item in self.catalog:
            counts[item["category"]] = counts.get(item["category"], 0) + 1
        self.assertEqual(counts, CATEGORY_COUNTS)
        self.assertEqual(len(self.template["protocol_results"]), 30)
        template = copy.deepcopy(self.template)
        template["protocol_results"][0], template["protocol_results"][1] = (
            template["protocol_results"][1],
            template["protocol_results"][0])
        with self.assertRaises(RuntimeError):
            validate_template_common(template, self.catalog)

    def test_every_committed_item_is_individually_not_run(self) -> None:
        for item in self.template["protocol_results"]:
            self.assertEqual(item["status"], "NOT_RUN")
            self.assertEqual(item["future_per_subject_results"], [])
            self.assertEqual(item["evidence_refs"], [])
            self.assertIsNone(item["moderator_observation"])
        template = copy.deepcopy(self.template)
        template["protocol_results"][5]["status"] = "PASS"
        self.assert_template_not_run_rejected(template)
        template = copy.deepcopy(self.template)
        template["protocol_results"][5]["status"] = "N/A"
        self.assert_template_not_run_rejected(template)

    def test_partial_subject_recruitment_or_consent_cannot_claim_progress(self) -> None:
        template = copy.deepcopy(self.template)
        participant = template["participant_slots"][0]
        participant["future_subject_code"] = "EXT-001"
        participant["future_consent_receipt_sha256"] = "0" * 64
        participant["status"] = "COMPLETED"
        self.assert_template_not_run_rejected(template)

    def test_partial_device_or_screen_reader_configuration_rejects(self) -> None:
        template = copy.deepcopy(self.template)
        template["device_slots"][0]["future_os_build"] = "future-build"
        self.assert_template_not_run_rejected(template)
        template = copy.deepcopy(self.template)
        template["screen_reader_configuration_slots"][0][
            "future_version_build"] = "future-version"
        self.assert_template_not_run_rejected(template)

    def test_pass_or_fail_cannot_start_before_real_targeted16_ta_binding(self) -> None:
        for status, overall in (
            ("EXTERNAL_SESSION_REVIEW_PASS", "PASS"),
            ("EXTERNAL_SESSION_REVIEW_FAIL", "FAIL"),
        ):
            template = copy.deepcopy(self.template)
            template["status"] = status
            template["overall_result"] = overall
            with self.assertRaises(RuntimeError):
                validate_completed(template, self.catalog)

    def test_fake_targeted16_ta_path_or_sha_fails_closed(self) -> None:
        template = copy.deepcopy(self.template)
        template["status"] = "EXTERNAL_SESSION_REVIEW_PASS"
        template["overall_result"] = "PASS"
        binding = template["future_targeted16_ta_pass_binding"]
        binding["status"] = "BOUND_TARGETED16_TA_PASS"
        binding["manual_path"] = "docs/evidence/not-created.json"
        binding["manual_sha256"] = "0" * 64
        with self.assertRaises(RuntimeError):
            validate_completed(template, self.catalog)

    def test_gate_rejects_protocol_count_acceptance_or_pass_policy_drift(self) -> None:
        gate = copy.deepcopy(self.gate)
        gate["protocol_counts"]["total_items"] = 29
        with self.assertRaises(RuntimeError):
            validate_gate(gate)
        gate = copy.deepcopy(self.gate)
        gate["protocol_catalog"][0]["acceptance"] = "too short"
        with self.assertRaises(RuntimeError):
            validate_gate(gate)
        gate = copy.deepcopy(self.gate)
        gate["item_result_contract"]["pass_policy"] = "majority vote"
        with self.assertRaises(RuntimeError):
            validate_gate(gate)

    def test_any_unexecuted_count_or_future_evidence_claim_rejects(self) -> None:
        template = copy.deepcopy(self.template)
        template["session_counts"]["protocol_items_not_run"] = 29
        self.assert_template_not_run_rejected(template)
        template = copy.deepcopy(self.template)
        template["future_session_evidence_manifest"]["sha256"] = "0" * 64
        self.assert_template_not_run_rejected(template)

    def test_product_gpu_authority_threshold_and_promotion_remain_false(self) -> None:
        for field in (
            "external_human_review_pass", "product_source_modified",
            "gpu_executed", "authority_or_rules_modified",
            "threshold_modified", "product_exposed", "product_pass",
            "golden_promotion", "m2_promotion", "beta_pass",
        ):
            template = copy.deepcopy(self.template)
            template["evidence_boundary"][field] = True
            self.assert_template_not_run_rejected(template)

    def test_frozen_gate_sha_detects_any_gate_byte_change(self) -> None:
        with tempfile.TemporaryDirectory() as directory:
            path = Path(directory) / "gate.json"
            gate = copy.deepcopy(self.gate)
            gate["scope"] += " drift"
            path.write_text(json.dumps(gate))
            with self.assertRaises(RuntimeError):
                validate(path, DEFAULT_TEMPLATE)


if __name__ == "__main__":
    unittest.main()
