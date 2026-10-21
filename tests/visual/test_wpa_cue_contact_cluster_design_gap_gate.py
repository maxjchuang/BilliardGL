from __future__ import annotations

import copy
import json
from pathlib import Path
import unittest

from scripts.validate_wpa_cue_contact_cluster_design_gap_gate import validate


ROOT = Path(__file__).resolve().parents[2]
GATE = ROOT / "docs/rules/wpa-cue-contact-cluster-design-gap-gate-v1.json"


class WpaCueContactClusterDesignGapGateTests(unittest.TestCase):
    def setUp(self) -> None:
        self.value = json.loads(GATE.read_text(encoding="utf-8"))

    def assertRejected(self, mutate) -> None:
        value = copy.deepcopy(self.value)
        mutate(value)
        self.assertTrue(validate(value, check_files=False))

    def test_canonical_gate_and_live_bindings_pass(self) -> None:
        self.assertEqual([], validate(self.value, check_files=True))

    def test_rejects_identity_status_or_scope_drift(self) -> None:
        self.assertRejected(lambda v: v.__setitem__("status", "IMPLEMENTATION_APPROVED"))
        self.assertRejected(lambda v: v["scope"].pop())
        self.assertRejected(lambda v: v["scope"].reverse())

    def test_rejects_pdf_path_sha_or_page_drift(self) -> None:
        self.assertRejected(lambda v: v["official_source"].__setitem__("path", "../../rules.pdf"))
        self.assertRejected(lambda v: v["official_source"].__setitem__("sha256", "0" * 64))
        self.assertRejected(lambda v: v["official_source"]["visually_reviewed_pdf_pages"].remove(3))

    def test_rejects_official_section_drift(self) -> None:
        self.assertRejected(lambda v: v["official_source"]["sections"][1].__setitem__("pdf_page", 10))
        self.assertRejected(lambda v: v["official_source"]["sections"][3].__setitem__("clause", "15.2"))

    def test_rejects_resolved_double_hit_rule_loss(self) -> None:
        self.assertRejected(lambda v: v["official_source"]["uniquely_determined_rules"].remove(
            "more than one cue-tip contact with the cue ball during one stroke is a foul"))

    def test_rejects_referee_declaration_rule_loss(self) -> None:
        self.assertRejected(lambda v: v["official_source"]["uniquely_determined_rules"].remove(
            "frozen-ball rule treatment requires a referee announcement and absence of an announcement means not frozen"))

    def test_rejects_policy_ambiguity_erasure(self) -> None:
        self.assertRejected(lambda v: v["official_source"]["not_numerically_or_operationally_defined"].remove(
            "the separation threshold for very close"))
        self.assertRejected(lambda v: v["official_source"]["not_numerically_or_operationally_defined"].remove(
            "whether pocket jaws satisfy the cushion alternatives in clause 15.2"))

    def test_rejects_audit_promotion_or_product_reachability_drift(self) -> None:
        self.assertRejected(lambda v: v["frozen_audit"]["rows"][0].__setitem__("status", "PARTIAL"))
        self.assertRejected(lambda v: v["frozen_audit"]["rows"][1].__setitem__("product_reachable", False))
        self.assertRejected(lambda v: v["frozen_audit"].__setitem__("promotion_authorized", True))

    def test_rejects_source_binding_sha_or_inventory_drift(self) -> None:
        self.assertRejected(lambda v: v["source_bindings"]["coupled_contact"].__setitem__("sha256", "0" * 64))
        self.assertRejected(lambda v: v["source_bindings"].pop("collector"))

    def test_rejects_false_regular_contact_duration_claim(self) -> None:
        self.assertRejected(lambda v: v["current_system_mapping"].__setitem__(
            "regular_cue_contact_reports_contact_duration", True))

    def test_rejects_false_full_stroke_completeness_claim(self) -> None:
        self.assertRejected(lambda v: v["current_system_mapping"].__setitem__(
            "full_shot_second_tip_contact_observable", True))
        self.assertRejected(lambda v: v["current_system_mapping"].__setitem__(
            "full_shot_tip_contact_count_complete", True))

    def test_rejects_geometry_as_referee_declaration(self) -> None:
        self.assertRejected(lambda v: v["current_system_mapping"].__setitem__(
            "frozen_topology_is_referee_declared", True))
        self.assertRejected(lambda v: v["policy_independent_observations"].__setitem__(
            "production_extension_safe_now", True))

    def test_rejects_missing_forbidden_inference(self) -> None:
        self.assertRejected(lambda v: v["policy_independent_observations"]
                            ["forbidden_inferences_from_existing_data"].pop())

    def test_rejects_missing_stroke_or_interval_telemetry(self) -> None:
        self.assertRejected(lambda v: v["required_future_telemetry_contract"]["stroke_identity"].pop())
        self.assertRejected(lambda v: v["required_future_telemetry_contract"]["cue_tip_intervals"].pop())

    def test_rejects_missing_completeness_attestation(self) -> None:
        self.assertRejected(lambda v: v["required_future_telemetry_contract"]["completeness"].remove(
            "full_stroke_complete"))
        self.assertRejected(lambda v: v["required_future_telemetry_contract"].__setitem__(
            "facts_are_measurements_not_fouls", False))

    def test_rejects_missing_frozen_declaration_or_lifecycle_telemetry(self) -> None:
        self.assertRejected(lambda v: v["required_future_telemetry_contract"]["frozen_declaration"].pop())
        self.assertRejected(lambda v: v["required_future_telemetry_contract"]["frozen_cushion_lifecycle"].pop())

    def test_rejects_close_ball_policy_guess(self) -> None:
        self.assertRejected(lambda v: v["pending_classification_contract"]["double_hit"].__setitem__(
            "close_ball_presumption", "distance_under_1mm"))

    def test_rejects_push_machine_predicate_guess(self) -> None:
        self.assertRejected(lambda v: v["pending_classification_contract"]["push"].__setitem__(
            "resolved_machine_predicate", "duration_over_2ms"))
        self.assertRejected(lambda v: v["pending_classification_contract"]["push"].__setitem__(
            "forbidden_guess", False))

    def test_rejects_frozen_declaration_or_jaw_policy_guess(self) -> None:
        self.assertRejected(lambda v: v["pending_classification_contract"]["frozen_cue_object"].__setitem__(
            "referee_declaration_required", False))
        self.assertRejected(lambda v: v["pending_classification_contract"]["frozen_object_cushion"].__setitem__(
            "jaw_and_prerequisite_legal_cushion_policy", "JAWS_COUNT"))

    def test_rejects_nonatomic_contract(self) -> None:
        self.assertRejected(lambda v: v["future_atomic_contract"]["declaration_commit"].pop())
        self.assertRejected(lambda v: v["future_atomic_contract"].__setitem__("partial_commit_allowed", True))
        self.assertRejected(lambda v: v["future_atomic_contract"].__setitem__("failure_rollback", "partial"))

    def test_rejects_missing_future_fixture(self) -> None:
        self.assertRejected(lambda v: v["required_future_fixtures"]["telemetry_positive"].pop())
        self.assertRejected(lambda v: v["required_future_fixtures"]["telemetry_negative"].pop())
        self.assertRejected(lambda v: v["required_future_fixtures"]["classification_after_policy"].pop())

    def test_rejects_active_or_expanded_allowlist(self) -> None:
        self.assertRejected(lambda v: v["future_implementation_allowlist"].__setitem__(
            "allowlist_is_dormant_until_policy_referee_input_and_telemetry_design_is_approved", False))
        self.assertRejected(lambda v: v["future_implementation_allowlist"]["production_files"].append(
            "src/Billiards/render/modern/modern_renderer.cpp"))
        self.assertRejected(lambda v: v["future_implementation_allowlist"]["production_files"].append(
            "CMakeLists.txt"))

    def test_rejects_forbidden_scope_loss(self) -> None:
        for item in (
            "treating coupled microsteps as a complete full-stroke tip-contact trace",
            "treating geometry-detected contact as a referee frozen-ball declaration",
            "promoting any scoped audit row above MISSING",
            "CMake changes",
            "Phase4 or Phase5 renderer changes",
            "GPU, Cocoa, or OpenGL execution",
        ):
            self.assertRejected(lambda v, item=item: v["forbidden_scope"].remove(item))

    def test_rejects_false_completion_claims(self) -> None:
        for key in (
            "complete_full_stroke_tip_telemetry_available",
            "referee_frozen_declaration_available",
            "double_hit_adjudication_implemented",
            "push_adjudication_implemented",
            "frozen_ball_adjudication_implemented",
            "production_implementation_complete",
            "audit_rows_promoted",
            "gpu_executed",
        ):
            self.assertRejected(lambda v, key=key: v["claims"].__setitem__(key, True))

    def test_rejects_non_object_bad_input(self) -> None:
        for value in (None, [], "gate", 1):
            self.assertTrue(validate(value, check_files=False))


if __name__ == "__main__":
    unittest.main()
