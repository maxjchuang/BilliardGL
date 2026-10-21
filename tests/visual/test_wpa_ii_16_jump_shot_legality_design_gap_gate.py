from __future__ import annotations

import copy
import json
from pathlib import Path
import unittest

from scripts.validate_wpa_ii_16_jump_shot_legality_design_gap_gate import validate


ROOT = Path(__file__).resolve().parents[2]
GATE = ROOT / "docs/rules/wpa-ii-16-jump-shot-legality-design-gap-gate-v1.json"


class WpaIi16JumpShotLegalityDesignGapGateTests(unittest.TestCase):
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
        self.assertRejected(lambda v: v.__setitem__("scope", ["WPA-II-15-2", "WPA-II-16"]))

    def test_rejects_pdf_path_sha_or_page_drift(self) -> None:
        self.assertRejected(lambda v: v["official_source"].__setitem__("path", "../../rules.pdf"))
        self.assertRejected(lambda v: v["official_source"].__setitem__("sha256", "0" * 64))
        self.assertRejected(lambda v: v["official_source"]["visually_reviewed_pdf_pages"].pop())

    def test_rejects_official_section_drift(self) -> None:
        self.assertRejected(lambda v: v["official_source"]["section"].__setitem__("clause", "16.1"))
        self.assertRejected(lambda v: v["official_source"]["section"].__setitem__("pdf_pages", [11]))

    def test_rejects_upper_half_rule_loss(self) -> None:
        self.assertRejected(lambda v: v["official_source"]["uniquely_determined_rules"].remove(
            "a jump intended to pass over an obstacle must use cue-tip contact on the upper half of the cue ball"))

    def test_rejects_no_overflight_exception_loss(self) -> None:
        self.assertRejected(lambda v: v["official_source"]["uniquely_determined_rules"].remove(
            "a lower-half strike, miscue, or other cause is not automatically an illegal jump when the cue ball does not pass over an obstacle and the shot otherwise legally hits the player's object ball"))

    def test_rejects_equator_or_clearance_ambiguity_erasure(self) -> None:
        self.assertRejected(lambda v: v["official_source"]["text_does_not_operationally_define"].remove(
            "whether a contact point exactly on the equator belongs to the upper or lower half"))
        self.assertRejected(lambda v: v["official_source"]["text_does_not_operationally_define"].remove(
            "the swept-volume clearance and tolerance that constitutes passing over an obstacle ball"))

    def test_rejects_scoop_or_intent_guess(self) -> None:
        self.assertRejected(lambda v: v["official_source"]["text_does_not_operationally_define"].remove(
            "a standalone scoop predicate distinct from lower-half impact, miscue, and other reasons"))
        self.assertRejected(lambda v: v["official_source"]["text_does_not_operationally_define"].remove(
            "how intent is established when classifying an attempted jump"))

    def test_rejects_audit_promotion_or_reachability_drift(self) -> None:
        self.assertRejected(lambda v: v["frozen_audit"]["row"].__setitem__("status", "PARTIAL"))
        self.assertRejected(lambda v: v["frozen_audit"]["row"].__setitem__("product_reachable", False))
        self.assertRejected(lambda v: v["frozen_audit"].__setitem__("promotion_authorized", True))

    def test_rejects_source_binding_sha_or_inventory_drift(self) -> None:
        self.assertRejected(lambda v: v["source_bindings"]["physics"].__setitem__("sha256", "0" * 64))
        self.assertRejected(lambda v: v["source_bindings"].pop("surface_motion"))

    def test_rejects_false_elevation_capability_claim(self) -> None:
        self.assertRejected(lambda v: v["current_system_mapping"].__setitem__(
            "runtime_cue_elevation_capability_enabled", True))
        self.assertRejected(lambda v: v["current_system_mapping"].__setitem__(
            "regular_cue_contact_accepts_nonzero_elevation", True))

    def test_rejects_false_vertical_physics_claim(self) -> None:
        self.assertRejected(lambda v: v["current_system_mapping"].__setitem__(
            "surface_motion_integrates_vertical_position_or_velocity", True))
        self.assertRejected(lambda v: v["current_system_mapping"].__setitem__(
            "surface_motion_applies_airborne_gravity", True))

    def test_rejects_xyz_storage_as_ballistic_claim(self) -> None:
        self.assertRejected(lambda v: v["current_system_mapping"].__setitem__(
            "xyz_storage_is_authoritative_ballistic_trajectory", True))
        self.assertRejected(lambda v: v["current_system_mapping"].__setitem__(
            "table_plane_departure_and_landing_events_exist", True))

    def test_rejects_false_miscue_or_overflight_observability(self) -> None:
        self.assertRejected(lambda v: v["current_system_mapping"].__setitem__(
            "miscue_induced_jump_observable", True))
        self.assertRejected(lambda v: v["current_system_mapping"].__setitem__(
            "obstacle_overflight_or_clearance_fact_exists", True))

    def test_rejects_requested_offset_as_actual_impact(self) -> None:
        self.assertRejected(lambda v: v["policy_independent_fact_boundary"]
                            ["forbidden_inferences"].remove(
                                "requested vertical tip offset proves the actual impact hemisphere"))
        self.assertRejected(lambda v: v["policy_independent_fact_boundary"].__setitem__(
            "production_extension_safe_now", True))

    def test_rejects_missing_impact_or_airborne_telemetry(self) -> None:
        self.assertRejected(lambda v: v["required_future_telemetry_contract"]["impact"].pop())
        self.assertRejected(lambda v: v["required_future_telemetry_contract"]["airborne_interval"].pop())

    def test_rejects_missing_obstacle_or_lifecycle_completeness(self) -> None:
        self.assertRejected(lambda v: v["required_future_telemetry_contract"]["obstacle_overflight"].pop())
        self.assertRejected(lambda v: v["required_future_telemetry_contract"]["shot_lifecycle"].pop())
        self.assertRejected(lambda v: v["required_future_telemetry_contract"].__setitem__(
            "facts_are_measurements_not_legality_decisions", False))

    def test_rejects_hemisphere_policy_guess(self) -> None:
        self.assertRejected(lambda v: v["pending_classification_contract"]["impact_hemisphere"].__setitem__(
            "equator_and_numeric_tolerance", "equator_is_upper"))
        self.assertRejected(lambda v: v["pending_classification_contract"]["impact_hemisphere"].__setitem__(
            "requested_offset_may_not_substitute_for_actual_contact", False))

    def test_rejects_jump_or_overflight_predicate_guess(self) -> None:
        self.assertRejected(lambda v: v["pending_classification_contract"]["jump_interval"].__setitem__(
            "machine_predicate", "height_gt_zero"))
        self.assertRejected(lambda v: v["pending_classification_contract"]["passes_over_obstacle"].__setitem__(
            "machine_predicate", "no_contact"))

    def test_rejects_miscue_scoop_or_referee_boundary_loss(self) -> None:
        self.assertRejected(lambda v: v["pending_classification_contract"]["miscue_and_other_reason"].__setitem__(
            "intent_or_referee_input_may_be_required", False))
        self.assertRejected(lambda v: v["pending_classification_contract"]["miscue_and_other_reason"].__setitem__(
            "scoop_is_not_independently_defined_by_clause_16", False))

    def test_rejects_nonatomic_contract(self) -> None:
        self.assertRejected(lambda v: v["future_atomic_contract"]["launch_commit"].pop())
        self.assertRejected(lambda v: v["future_atomic_contract"].__setitem__("partial_commit_allowed", True))
        self.assertRejected(lambda v: v["future_atomic_contract"].__setitem__("failure_rollback", "partial"))

    def test_rejects_missing_future_fixture(self) -> None:
        self.assertRejected(lambda v: v["required_future_fixtures"]["telemetry_positive"].pop())
        self.assertRejected(lambda v: v["required_future_fixtures"]["telemetry_negative"].pop())
        self.assertRejected(lambda v: v["required_future_fixtures"]["classification_after_policy"].pop())

    def test_rejects_active_or_expanded_allowlist(self) -> None:
        self.assertRejected(lambda v: v["future_implementation_allowlist"].__setitem__(
            "allowlist_is_dormant_until_3d_physics_and_policy_gate_is_approved", False))
        self.assertRejected(lambda v: v["future_implementation_allowlist"]["production_files"].append(
            "src/Billiards/render/modern/modern_renderer.cpp"))
        self.assertRejected(lambda v: v["future_implementation_allowlist"]["production_files"].append("CMakeLists.txt"))

    def test_rejects_forbidden_scope_loss(self) -> None:
        for item in (
            "enabling cue elevation before a supported three-dimensional contact and trajectory model exists",
            "treating requested tip offset as actual impact hemisphere",
            "promoting WPA-II-16 above MISSING",
            "CMake changes",
            "Phase4 or Phase5 renderer changes",
            "GPU, Cocoa, or OpenGL execution",
        ):
            self.assertRejected(lambda v, item=item: v["forbidden_scope"].remove(item))

    def test_rejects_false_completion_claims(self) -> None:
        for key in (
            "product_reachable_jump_shot_available",
            "actual_impact_hemisphere_telemetry_complete",
            "airborne_trajectory_telemetry_complete",
            "obstacle_overflight_telemetry_complete",
            "jump_legality_implemented",
            "production_implementation_complete",
            "audit_row_promoted",
            "gpu_executed",
        ):
            self.assertRejected(lambda v, key=key: v["claims"].__setitem__(key, True))

    def test_rejects_non_object_bad_input(self) -> None:
        for value in (None, [], "gate", 1):
            self.assertTrue(validate(value, check_files=False))


if __name__ == "__main__":
    unittest.main()
