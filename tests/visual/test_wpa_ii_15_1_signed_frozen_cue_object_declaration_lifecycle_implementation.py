import copy
import importlib.util
import json
import pathlib
import unittest

ROOT = pathlib.Path(__file__).resolve().parents[2]
VALIDATOR = ROOT / "scripts/validate_wpa_ii_15_1_signed_frozen_cue_object_declaration_lifecycle_implementation.py"
RECEIPT = ROOT / "docs/rules/wpa-ii-15-1-signed-frozen-cue-object-declaration-lifecycle-implementation-receipt-v1.json"
SPEC = importlib.util.spec_from_file_location("wpa_15_1_impl", VALIDATOR)
MODULE = importlib.util.module_from_spec(SPEC); SPEC.loader.exec_module(MODULE)

class ReceiptTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls): cls.value = json.loads(RECEIPT.read_text())
    def assert_bad(self, mutate):
        value = copy.deepcopy(self.value); mutate(value)
        self.assertTrue(MODULE.validate(value, False))
    def test_valid(self): self.assertEqual(MODULE.validate(self.value, True), [])
    def test_status(self): self.assert_bad(lambda v: v.update(status="IMPLEMENTED"))
    def test_gate(self): self.assert_bad(lambda v: v["authority_bindings"]["implementation_design_gate"].update(sha256="0" * 64))
    def test_policy(self): self.assert_bad(lambda v: v["authority_bindings"]["root_selected_policy"].update(selected_option_id="OTHER"))
    def test_red(self): self.assert_bad(lambda v: v["tests_first_record"].update(red_observed=False))
    def test_states(self): self.assert_bad(lambda v: v["implemented_contract"]["states"].pop())
    def test_retry(self): self.assert_bad(lambda v: v["implemented_contract"].update(exact_proposal_final_cancel_retry_zero_delta=False))
    def test_commands(self): self.assert_bad(lambda v: v["automation_headless"]["commands"].pop())
    def test_positive(self): self.assert_bad(lambda v: v["fixture_matrix"]["positive"].pop())
    def test_negative(self): self.assert_bad(lambda v: v["fixture_matrix"]["negative"].pop())
    def test_sources(self): self.assert_bad(lambda v: v["source_bindings"].pop())
    def test_tests(self): self.assert_bad(lambda v: v["test_bindings"].pop())
    def test_cpu_count(self): self.assert_bad(lambda v: v["cpu_verification"].update(passed_count=10))
    def test_external_blocker(self): self.assert_bad(lambda v: v["cpu_verification"]["blocked_target"].update(status="PASS"))
    def test_no_d5_completion(self): self.assert_bad(lambda v: v["completion_boundary"].update(d5_frozen_contact_classification_complete=True))
    def test_no_audit_promotion(self): self.assert_bad(lambda v: v["completion_boundary"].update(wpa_ii_15_1_audit_row_promoted=True))

if __name__ == "__main__": unittest.main()
