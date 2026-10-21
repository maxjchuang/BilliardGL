from __future__ import annotations

import copy
import json
from pathlib import Path
import unittest

from scripts.validate_wpa_ii_17_a_e_off_table_physical_telemetry_design_gap_gate import validate


ROOT = Path(__file__).resolve().parents[2]
GATE = ROOT / "docs/rules/wpa-ii-17-a-e-off-table-physical-telemetry-design-gap-gate-v1.json"


class WpaIi17AeOffTablePhysicalTelemetryDesignGapGateTests(unittest.TestCase):
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
        self.assertRejected(lambda v: v.__setitem__("scope", ["WPA-II-17(b)", "WPA-II-17(e)"]))

    def test_rejects_silent_official_locator_relabel(self) -> None:
        self.assertRejected(lambda v: v["requested_scope_locator_correction"].__setitem__(
            "official_cue_off_table_locator", "17(b)"))
        self.assertRejected(lambda v: v["requested_scope_locator_correction"].__setitem__(
            "silent_relabel_forbidden", False))

    def test_rejects_official_pdf_clause_or_page_drift(self) -> None:
        self.assertRejected(lambda v: v["official_source"].__setitem__("sha256", "0" * 64))
        self.assertRejected(lambda v: v["official_source"]["clauses"][1].__setitem__("clause", "17(b)"))
        self.assertRejected(lambda v: v["official_source"].__setitem__("visually_reviewed_pdf_pages", [11]))

    def test_rejects_object_policy_conflict_resolution(self) -> None:
        self.assertRejected(lambda v: v["official_source"]["object_ball_policy_conflict"].__setitem__(
            "status", "RESOLVED_AS_FOUL"))
        self.assertRejected(lambda v: v["official_source"]["object_ball_policy_conflict"].__setitem__(
            "classification_or_adjudication_authorized", True))
        self.assertRejected(lambda v: v["pending_classification_contract"]["object_ball"].__setitem__(
            "classification_predicate", "always_foul"))

    def test_rejects_audit_promotion(self) -> None:
        self.assertRejected(lambda v: v["frozen_audit"]["rows"][0].__setitem__("status", "IMPLEMENTED"))
        self.assertRejected(lambda v: v["frozen_audit"].__setitem__("promotion_authorized", True))

    def test_rejects_cue_scratch_receipt_conflation_or_drift(self) -> None:
        self.assertRejected(lambda v: v["independent_cue_scratch_receipt"].__setitem__(
            "off_table_extension_claimed", True))
        self.assertRejected(lambda v: v["independent_cue_scratch_receipt"].__setitem__("sha256", "0" * 64))

    def test_rejects_source_binding_drift(self) -> None:
        self.assertRejected(lambda v: v["source_bindings"]["physics"].__setitem__("sha256", "0" * 64))
        self.assertRejected(lambda v: v["source_bindings"].pop("surface_motion"))

    def test_rejects_false_vertical_or_departure_physics_claim(self) -> None:
        for key in ("position_y_integrated", "velocity_y_integrated", "gravity_or_airborne_phase_exists",
                    "leave_table_event_exists", "outside_table_landing_surface_exists",
                    "off_table_rest_event_exists", "rail_top_return_event_exists"):
            self.assertRejected(lambda v, key=key: v["current_physics_mapping"].__setitem__(key, True))

    def test_rejects_unbounded_or_xyz_storage_as_physical_evidence(self) -> None:
        self.assertRejected(lambda v: v["current_physics_mapping"].__setitem__(
            "unbounded_mode_is_product_off_table_physics", True))
        self.assertRejected(lambda v: v["current_physics_mapping"].__setitem__(
            "physics_frame_xyz_storage_proves_vertical_dynamics", True))
        self.assertRejected(lambda v: v["capture_and_exclusion_boundary"]["forbidden_substitutes"].remove(
            "using PhysicsBoundaryMode::Unbounded to let a planar ball wander beyond a boundary"))

    def test_rejects_pocket_or_storage_as_off_table_evidence(self) -> None:
        self.assertRejected(lambda v: v["current_physics_mapping"].__setitem__(
            "object_capture_storage_coordinates_are_physical_off_table_evidence", True))
        self.assertRejected(lambda v: v["current_physics_mapping"].__setitem__(
            "pocket_capture_is_distinct_from_off_table", False))
        self.assertRejected(lambda v: v["capture_and_exclusion_boundary"]["forbidden_substitutes"].remove(
            "treating an object's post-pocket storage coordinates as an off-table rest"))

    def test_rejects_injected_coordinate_or_shotfacts_substitute(self) -> None:
        self.assertRejected(lambda v: v["capture_and_exclusion_boundary"]["forbidden_substitutes"].remove(
            "manually placing a ball outside the x/z playfield rectangle"))
        self.assertRejected(lambda v: v["capture_and_exclusion_boundary"]["forbidden_substitutes"].remove(
            "injecting cueBallOffTable or objectBallsOffTable through automation or a unit-test ShotFacts object"))
        self.assertRejected(lambda v: v["capture_and_exclusion_boundary"].__setitem__(
            "production_extension_safe_now", True))

    def test_rejects_false_collector_completion(self) -> None:
        self.assertRejected(lambda v: v["current_physics_mapping"].__setitem__(
            "shot_fact_collector_derives_cue_off_table", True))
        self.assertRejected(lambda v: v["current_physics_mapping"].__setitem__(
            "shot_fact_collector_derives_object_off_table", True))

    def test_rejects_incomplete_future_physics_or_telemetry_contract(self) -> None:
        self.assertRejected(lambda v: v["required_future_physics_contract"]["capabilities"].pop())
        self.assertRejected(lambda v: v["required_future_physics_contract"].__setitem__(
            "no_coordinate_only_shortcut", False))
        self.assertRejected(lambda v: v["required_future_telemetry_contract"]["outside_rest"].pop())
        self.assertRejected(lambda v: v["required_future_telemetry_contract"]["exclusions"].pop())

    def test_rejects_nonatomic_or_failed_step_evidence(self) -> None:
        self.assertRejected(lambda v: v["future_atomic_contract"].__setitem__(
            "append_only_on_succeeded_physics_steps", False))
        self.assertRejected(lambda v: v["future_atomic_contract"].__setitem__("partial_commit_allowed", True))
        self.assertRejected(lambda v: v["future_atomic_contract"].__setitem__("failed_step", "keep departure"))

    def test_rejects_missing_future_fixtures(self) -> None:
        self.assertRejected(lambda v: v["required_future_fixtures"]["physics_positive"].pop())
        self.assertRejected(lambda v: v["required_future_fixtures"]["physics_negative"].remove(
            "RAIL_TOP_RETURN_IS_EXCLUDED_FROM_OFF_TABLE_REST"))
        self.assertRejected(lambda v: v["required_future_fixtures"]["classification_after_policy"].pop())

    def test_rejects_active_or_expanded_allowlist(self) -> None:
        self.assertRejected(lambda v: v["future_implementation_allowlist"].__setitem__(
            "allowlist_is_dormant_until_physics_telemetry_and_object_policy_gates_are_approved", False))
        self.assertRejected(lambda v: v["future_implementation_allowlist"]["production_files"].append(
            "src/Billiards/render/modern/modern_renderer.cpp"))
        self.assertRejected(lambda v: v["future_implementation_allowlist"]["production_files"].append(
            "CMakeLists.txt"))

    def test_rejects_forbidden_scope_loss(self) -> None:
        for item in ("production source edits in this design-gap task",
                     "resolving the 12.3 versus 17(e) object-ball policy conflict",
                     "changing cue-scratch receipt or pocket-capture semantics",
                     "CMake changes", "Phase4 or Phase5 renderer changes",
                     "GPU, Cocoa, or OpenGL execution"):
            self.assertRejected(lambda v, item=item: v["forbidden_scope"].remove(item))

    def test_rejects_false_completion_claims(self) -> None:
        for key in ("physical_off_table_telemetry_available", "cue_off_table_fact_collector_complete",
                    "object_off_table_fact_collector_complete", "rail_top_return_observable",
                    "object_policy_conflict_resolved", "production_implementation_complete",
                    "audit_row_promoted", "cue_scratch_receipt_modified", "cmake_modified",
                    "gpu_executed", "readiness_or_authorization_created"):
            self.assertRejected(lambda v, key=key: v["claims"].__setitem__(key, True))

    def test_rejects_non_object_bad_input(self) -> None:
        for value in (None, [], "gate", 1):
            self.assertTrue(validate(value, check_files=False))


if __name__ == "__main__":
    unittest.main()
