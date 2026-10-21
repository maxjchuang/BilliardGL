import copy
import importlib.util
import json
import pathlib
import unittest

ROOT = pathlib.Path(__file__).resolve().parents[1]
SPEC = importlib.util.spec_from_file_location(
    "validator", ROOT / "scripts/validate_phase4_v19_cp3_offscreen_atomic_implementation.py")
V = importlib.util.module_from_spec(SPEC)
SPEC.loader.exec_module(V)
BASE = json.loads(V.RECEIPT.read_text()) if V.RECEIPT.exists() else {}


class ReceiptTests(unittest.TestCase):
    def test_canonical(self):
        self.assertEqual(V.validate(replay=True), [])

    def test_fail_closed_mutations(self):
        mutations = []

        def add(name, mutate):
            document = copy.deepcopy(BASE)
            mutate(document)
            mutations.append((name, document))

        add("schema", lambda d: d.update(schema="bad"))
        add("status", lambda d: d.update(status="bad"))
        for key in [
            "sample_profiles", "failure_stages", "success_cases",
            "invoke_false_failures", "invoke_true_gl_error_failures",
            "helper_throw_failures", "publish_failures", "transaction_red_status",
            "adapter_red_status", "typed_live_main_resource_alias_check",
            "exact_fbo_attachment_mapping_check", "frozen_role_palette_validation",
            "instrumentation_color_semantic_current_token_pairing",
            "render_frame_prepare_call_count", "visible_composite_deferred_to_cp4",
            "cmake_modified_by_cp3", "cmake_external_lineage", "gpu_run",
            "billiards_executed", "readiness_created", "authorization_created",
            "cp4_started",
        ]:
            add("contract_" + key,
                lambda d, k=key: d["verified_contract"].__setitem__(k, "mutated"))
        for role in V.PINNED:
            add("binding_" + role,
                lambda d, r=role: next(x for x in d["bindings"] if x["role"] == r).__setitem__("sha256", "0" * 64))
        add("drop_binding", lambda d: d["bindings"].pop())
        add("typed_alias_source", lambda d: next(x for x in d["bindings"] if x["role"] == "transaction_cpp").__setitem__("path", "CMakeLists.txt"))
        add("fbo_mapping_source", lambda d: next(x for x in d["bindings"] if x["role"] == "modern_cpp").__setitem__("sha256", "f" * 64))
        add("palette_geometry_source", lambda d: next(x for x in d["bindings"] if x["role"] == "modern_cpp").__setitem__("path", "src/Billiards/render/modern/modern_renderer.h"))
        add("rgb_token_test", lambda d: next(x for x in d["bindings"] if x["role"] == "cpu_test").__setitem__("sha256", "1" * 64))
        add("state_throw_publish_source", lambda d: next(x for x in d["bindings"] if x["role"] == "transaction_h").__setitem__("sha256", "2" * 64))
        add("resize_context_source", lambda d: next(x for x in d["bindings"] if x["role"] == "modern_cpp").__setitem__("sha256", "3" * 64))
        add("stale_identity_test", lambda d: next(x for x in d["bindings"] if x["role"] == "cpu_test").__setitem__("path", "CMakeLists.txt"))
        add("cp1_inventory", lambda d: d["cp1_canonical_inventory"][0].__setitem__("probeDenominator", 0))
        add("cp2_contract", lambda d: d["cp2_verified_contract"].__setitem__("default_inset_on", False))
        self.assertGreaterEqual(len(mutations), 40)
        for name, document in mutations:
            with self.subTest(name=name):
                self.assertTrue(V.validate_document(document))


if __name__ == "__main__":
    unittest.main()
