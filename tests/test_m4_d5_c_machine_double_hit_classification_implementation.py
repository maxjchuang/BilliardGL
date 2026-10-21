import copy
import importlib.util
import json
import pathlib
import unittest

ROOT = pathlib.Path(__file__).resolve().parents[1]
SPEC = importlib.util.spec_from_file_location("validator", ROOT / "scripts/validate_m4_d5_c_machine_double_hit_classification_implementation.py")
V = importlib.util.module_from_spec(SPEC)
SPEC.loader.exec_module(V)
BASE = json.loads(V.RECEIPT.read_text())

class Tests(unittest.TestCase):
    def reject(self, mutate):
        data = copy.deepcopy(BASE)
        mutate(data)
        with self.assertRaises(Exception):
            V.validate(data)

    def test_valid(self): V.validate(copy.deepcopy(BASE))
    def test_schema(self): self.reject(lambda d: d.update(schema="bad"))
    def test_status(self): self.reject(lambda d: d.update(status="complete"))
    def test_policy_sha(self): self.reject(lambda d: d["authority_bindings"]["root_selected_policy"].update(sha256="0" * 64))
    def test_policy_bundle(self): self.reject(lambda d: d["authority_bindings"]["root_selected_policy"].update(bundle="D5C-B"))
    def test_gate_sha(self): self.reject(lambda d: d["authority_bindings"]["implementation_design_gate"].update(sha256="0" * 64))
    def test_red(self): self.reject(lambda d: d["tests_first"].update(red_observed=False))
    def test_green(self): self.reject(lambda d: d["tests_first"].update(green=False))
    def test_gap_threshold(self): self.reject(lambda d: d["contract"].update(very_close_surface_gap_cm=0.1))
    def test_graze_threshold(self): self.reject(lambda d: d["contract"].update(light_graze_normal_impulse_ratio=0.1))
    def test_tolerance(self): self.reject(lambda d: d["contract"].update(tip_separation_tie_tolerance_seconds=1e-6))
    def test_precedence(self): self.reject(lambda d: d["contract"].update(interval_count_precedes_signed_review=False))
    def test_review(self): self.reject(lambda d: d["contract"].update(signed_close_case_otherwise_review_only=False))
    def test_fixture(self): self.reject(lambda d: d["fixtures"]["positive"].pop())
    def test_production_allowlist(self): self.reject(lambda d: d["source_bindings"].pop())
    def test_test_allowlist(self): self.reject(lambda d: d["test_bindings"].pop())
    def test_source_sha(self): self.reject(lambda d: d["source_bindings"][0].update(sha256="0" * 64))
    def test_test_sha(self): self.reject(lambda d: d["test_bindings"][0].update(sha256="0" * 64))
    def test_cmake_sha(self): self.reject(lambda d: d["build_binding"].update(sha256="0" * 64))
    def test_cpu_count(self): self.reject(lambda d: d["cpu"].update(passed_count=16))
    def test_binary_sha(self): self.reject(lambda d: d["cpu"]["binaries"][0].update(sha256="0" * 64))
    def test_gpu(self): self.reject(lambda d: d["cpu"].update(gpu=True))
    def test_false_d5_complete(self): self.reject(lambda d: d["completion"].update(d5_complete=True))
    def test_false_m4_complete(self): self.reject(lambda d: d["completion"].update(m4_complete=True))

if __name__ == "__main__": unittest.main()
