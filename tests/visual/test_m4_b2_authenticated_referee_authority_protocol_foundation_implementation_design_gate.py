from __future__ import annotations

import copy
import importlib.util
import json
import sys
import unittest
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
VALIDATOR = ROOT / "scripts/validate_m4_b2_authenticated_referee_authority_protocol_foundation_implementation_design_gate.py"
GATE = ROOT / "docs/rules/m4-b2-authenticated-referee-authority-protocol-foundation-implementation-design-gate-v1.json"
SPEC = importlib.util.spec_from_file_location("m4_b2_referee_gate", VALIDATOR)
assert SPEC and SPEC.loader
MODULE = importlib.util.module_from_spec(SPEC)
sys.modules[SPEC.name] = MODULE
SPEC.loader.exec_module(MODULE)


class RefereeAuthorityFoundationGateTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls) -> None:
        cls.value = json.loads(GATE.read_text())

    def reject(self, mutation) -> None:
        value = copy.deepcopy(self.value)
        mutation(value)
        self.assertTrue(MODULE.validate(value, False))

    def test_frozen_gate_contract(self) -> None:
        self.assertEqual([], MODULE.validate(self.value, False))

    def test_live_sources_have_authorized_successor_drift(self) -> None:
        errors = MODULE.validate(self.value, True)
        self.assertEqual(
            ["source_drift", "source_drift", "source_drift",
             "build_drift", "test_drift", "test_drift"], errors)

    def test_schema_status_and_extra_key(self) -> None:
        self.reject(lambda v: v.__setitem__("schema", "v2"))
        self.reject(lambda v: v.__setitem__("status", "IMPLEMENTED"))
        self.reject(lambda v: v.__setitem__("approved", True))

    def test_authority_and_lineage_are_bound(self) -> None:
        self.reject(lambda v: v["authority_bindings"]["root_selected_policy"].__setitem__("authorized_stage", "B2_ALL_ROWS"))
        self.reject(lambda v: v["authority_bindings"]["b1_completion_receipt"].__setitem__("sha256", "0" * 64))
        self.reject(lambda v: v["authority_bindings"]["critical_path_successor"].__setitem__("sha256", "1" * 64))

    def test_foundation_never_becomes_specific_rule_or_physics(self) -> None:
        for key in (
            "specific_wpa_rule_row_consumer_implemented", "specific_foul_or_rerack_semantics_implemented",
            "visual_or_player_ui_implemented", "global_player_rules_ui_exposed",
            "referee_declaration_is_physics_fact", "physics_fact_may_authenticate_referee",
        ):
            self.reject(lambda v, key=key: v["foundation_scope"].__setitem__(key, True))

    def test_authentication_role_and_no_bypass(self) -> None:
        self.reject(lambda v: v["identity_and_authentication_contract"]["authority_roles"].pop())
        self.reject(lambda v: v["identity_and_authentication_contract"].__setitem__("no_embedded_secret_or_test_bypass_in_production", False))
        self.reject(lambda v: v["identity_and_authentication_contract"].__setitem__("authenticator_boundary", "accept test token"))
        self.reject(lambda v: v["identity_and_authentication_contract"].__setitem__("signature_binding", "proof covers payload"))

    def test_request_schema_and_operations_are_exact(self) -> None:
        self.reject(lambda v: v["request_schema"]["required_fields"].pop())
        self.reject(lambda v: v["request_schema"]["required_fields"].append("rule_result"))
        self.reject(lambda v: v["request_schema"]["operation_values"].reverse())
        self.reject(lambda v: v["request_schema"].__setitem__("payload", "any json"))

    def test_consumer_registry_cannot_register_wpa_row(self) -> None:
        self.reject(lambda v: v["consumer_policy_registry"].__setitem__("specific_wpa_kinds_registered_by_this_gate", True))
        self.reject(lambda v: v["consumer_policy_registry"]["required_policy_fields"].pop())
        self.reject(lambda v: v["consumer_policy_registry"]["foundation_test_kinds_only"].append("STALEMATE"))

    def test_proposal_final_separation_and_atomicity(self) -> None:
        self.reject(lambda v: v["proposal_final_state_machine"].__setitem__("separation", "proposal is final"))
        self.reject(lambda v: v["proposal_final_state_machine"].__setitem__("consumer_boundary", "write ShotFacts"))
        self.reject(lambda v: v["proposal_final_state_machine"].__setitem__("partial_commit_allowed", True))
        self.reject(lambda v: v["proposal_final_state_machine"].__setitem__("atomicity", "best effort"))

    def test_ack_retry_journal_and_readback_do_not_leak(self) -> None:
        self.reject(lambda v: v["ack_journal_and_readback"]["ack_fields"].pop())
        self.reject(lambda v: v["ack_journal_and_readback"].__setitem__("exact_retry", "append again"))
        self.reject(lambda v: v["ack_journal_and_readback"].__setitem__("automation_readback", "include credentials"))
        self.reject(lambda v: v["ack_journal_and_readback"].__setitem__("audit_journal", "record proof"))

    def test_allowlist_and_cmake_scope_are_exact(self) -> None:
        self.reject(lambda v: v["implementation_allowlist"]["production_files"].pop())
        self.reject(lambda v: v["implementation_allowlist"]["test_files"].reverse())
        self.reject(lambda v: v["implementation_allowlist"]["build_files"].append("src/Billiards/CMakeLists.txt"))
        self.reject(lambda v: v["implementation_allowlist"].__setitem__("cmake_change_allowed", False))
        self.reject(lambda v: v["implementation_allowlist"].__setitem__("visual_ui_renderer_product_session_edit_allowed", True))
        self.reject(lambda v: v["implementation_allowlist"].__setitem__("physics_shotfacts_adjudicator_rack_executor_edit_allowed", True))

    def test_fixture_families_are_exact(self) -> None:
        self.reject(lambda v: v["required_fixtures"]["positive"].pop())
        self.reject(lambda v: v["required_fixtures"]["negative"].reverse())
        self.reject(lambda v: v["required_fixtures"].__setitem__("headless_cpu_only", False))

    def test_live_source_build_and_test_bindings(self) -> None:
        self.reject(lambda v: v["source_bindings"][0].__setitem__("sha256", "0" * 64))
        self.reject(lambda v: v["build_source_bindings"].pop())
        self.reject(lambda v: v["test_source_bindings"].reverse())

    def test_gate_does_not_authorize_implementation_or_completion(self) -> None:
        self.reject(lambda v: v["completion_boundary"].__setitem__("implementation_authorized_by_this_gate", True))
        self.reject(lambda v: v["completion_boundary"].__setitem__("root_revalidation_required_before_tests_first_patch", False))
        for key in ("specific_rule_consumers_authorized", "b2_complete_from_foundation", "m4_complete", "production_modified", "cmake_modified", "gpu_ui_readiness_or_authorization"):
            self.reject(lambda v, key=key: v["completion_boundary"].__setitem__(key, True))

    def test_malformed_types_fail_closed(self) -> None:
        for value in (None, [], 1, "x"):
            self.assertTrue(MODULE.validate(value, False))
        self.reject(lambda v: v.__setitem__("authority_bindings", []))
        self.reject(lambda v: v.__setitem__("request_schema", None))
        self.reject(lambda v: v.__setitem__("completion_boundary", "done"))


if __name__ == "__main__":
    unittest.main()
