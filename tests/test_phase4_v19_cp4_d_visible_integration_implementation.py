import copy
import importlib.util
import json
import pathlib
import unittest

ROOT = pathlib.Path(__file__).resolve().parents[1]
SPEC = importlib.util.spec_from_file_location(
    "validator", ROOT / "scripts/validate_phase4_v19_cp4_d_visible_integration_implementation.py")
V = importlib.util.module_from_spec(SPEC)
SPEC.loader.exec_module(V)
BASE = json.loads(V.RECEIPT.read_text()) if V.RECEIPT.exists() else {}


class ReceiptTests(unittest.TestCase):
    def test_canonical(self):
        # The canonical validator is run once separately with fresh O2 replay;
        # receipt mutations intentionally remain read-only and bounded.
        self.assertEqual(V.validate(replay=False), [])

    def test_fail_closed_mutations(self):
        mutations = []

        def add(name, mutate):
            document = copy.deepcopy(BASE)
            mutate(document)
            mutations.append((name, document))

        add("schema", lambda d: d.update(schema="bad"))
        add("schema_version", lambda d: d.update(schema_version=2))
        add("status", lambda d: d.update(status="FINAL_PRODUCT_PASS"))
        add("proof_boundary", lambda d: d.update(proof_boundary="GPU_PRODUCT_PASS"))
        for key in V.EXACT_CONTRACT:
            add("contract_" + key,
                lambda d, k=key: d["verified_contract"].__setitem__(k, "mutated"))
        for role in [
            "core41_h", "core41_cpp", "gpu_h", "gpu_cpp", "runtime_cpp",
            "modern_cpp", "cpu_test", "o0_stdout", "o2_stdout",
            "red_v2_stderr", "red_v2_exit", "validator", "mutation_tests",
            "chrome_correction_gate", "core41_correction_gate",
            "cp1_receipt", "cp2_receipt", "cp3_receipt",
            "cmake_external_accepted_d5_d_lineage",
        ]:
            add("binding_sha_" + role,
                lambda d, r=role: next(x for x in d["bindings"] if x["role"] == r).__setitem__("sha256", "0" * 64))
        add("drop_binding", lambda d: d["bindings"].pop())
        add("duplicate_role", lambda d: d["bindings"].append(copy.deepcopy(d["bindings"][0])))
        add("core41_source_swap", lambda d: next(x for x in d["bindings"] if x["role"] == "core41_cpp").__setitem__("path", "src/Billiards/render/ui/product_ui_gpu_renderer.cpp"))
        add("validator_redirect", lambda d: next(x for x in d["bindings"] if x["role"] == "validator").__setitem__("path", "CMakeLists.txt"))
        add("tests_redirect", lambda d: next(x for x in d["bindings"] if x["role"] == "mutation_tests").__setitem__("path", "CMakeLists.txt"))
        add("typed_receipt_false", lambda d: d["verified_contract"].__setitem__("typed_begin_receipt", False))
        add("successful_uncaptured", lambda d: d["verified_contract"].__setitem__("successful_begin_explicitly_captured_and_restore_required", False))
        add("internal_restore_one", lambda d: d["verified_contract"].__setitem__("captured_setup_failure_core41_internal_restore_attempts", 1))
        add("outer_restore_two", lambda d: d["verified_contract"].__setitem__("captured_setup_failure_outer_restore_attempts", 2))
        add("poison_false", lambda d: d["verified_contract"].__setitem__("persistent_poison_same_context_zero_work", False))
        add("recovery_false", lambda d: d["verified_contract"].__setitem__("context_loss_fresh_core41_runtime_recovery", False))
        add("sample_center_false", lambda d: d["verified_contract"].__setitem__("fractional_cutout_sample_center_audit", False))
        add("o0_o2_drift_claim", lambda d: d["verified_contract"].__setitem__("o2_wall_seconds", 0))
        add("gpu_true", lambda d: d["verified_contract"].__setitem__("gpu_run", True))
        add("auth_true", lambda d: d["verified_contract"].__setitem__("authorization_created", True))
        self.assertGreaterEqual(len(mutations), 70)
        for name, document in mutations:
            with self.subTest(name=name):
                self.assertTrue(V.validate_document(document), name)


if __name__ == "__main__":
    unittest.main()
