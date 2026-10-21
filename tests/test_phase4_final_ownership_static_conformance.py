import copy
import importlib.util
from pathlib import Path
import unittest


ROOT = Path(__file__).resolve().parents[1]
SPEC = importlib.util.spec_from_file_location(
    "phase4_static",
    ROOT / "scripts/validate_phase4_final_ownership_implementation.py")
MODULE = importlib.util.module_from_spec(SPEC)
assert SPEC.loader is not None
SPEC.loader.exec_module(MODULE)


class Phase4FinalOwnershipStaticConformanceTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.texts = {
            key: (ROOT / relative).read_text()
            for key, relative in MODULE.SOURCE_PATHS.items()
        }

    def mutation_fails(self, key: str, before: str, after: str = ""):
        texts = copy.deepcopy(self.texts)
        self.assertIn(before, texts[key])
        texts[key] = texts[key].replace(before, after, 1)
        self.assertTrue(MODULE.validate_texts(texts))

    def test_live_tree_passes(self):
        result = MODULE.validate(ROOT)
        self.assertEqual(result["status"], "PASS", result["errors"])

    def test_default_off_is_fail_closed(self):
        self.mutation_fails("renderer_header",
            "bool enableFinalOwnershipInstrumentation = false;",
            "bool enableFinalOwnershipInstrumentation = true;")

    def test_sample_payload_is_required(self):
        self.mutation_fails("renderer", "GL_RGBA32UI", "GL_RGBA8")

    def test_ui_coverage_is_same_draw(self):
        self.mutation_fails("ui_shader",
            "layout(location = 1) out vec4 outFinalOwnershipUiCoverage;")

    def test_pass_order_is_required(self):
        texts = copy.deepcopy(self.texts)
        texts["renderer"] = texts["renderer"].replace(
            '"integrated_semantic_msaa_resolve"', '"late_resolve"', 1)
        self.assertIn("production_pass_graph_order_not_integrated",
            MODULE.validate_texts(texts))

    def test_denominator_is_immutable(self):
        self.mutation_fails("resolver_header",
            "std::size_t expectedSourceProbeCount = 95;",
            "std::size_t expectedSourceProbeCount = 94;")

    def test_threshold_is_immutable(self):
        self.mutation_fails("resolver_header",
            "double minimumEdgeContrastRatio = 4.0;",
            "double minimumEdgeContrastRatio = 3.9;")

    def test_machine_gate_remains_after_durable_append(self):
        self.mutation_fails("telemetry", "if (!input.machinePass)")

    def test_cross_type_sampler_same_program_is_rejected(self):
        texts = copy.deepcopy(self.texts)
        anchor = "uniform usampler2D uSemanticSingle;"
        self.assertIn(anchor, texts["renderer"])
        texts["renderer"] = texts["renderer"].replace(
            anchor, "uniform usampler2DMS uForbiddenAlias;\n" + anchor, 1)
        self.assertIn("cross_type_active_sampler_program_alias_risk",
                      MODULE.validate_texts(texts))

    def test_runtime_branch_combined_sampler_program_is_rejected(self):
        texts = copy.deepcopy(self.texts)
        anchor = "uniform usampler2D uSemanticSingle;"
        texts["renderer"] = texts["renderer"].replace(
            anchor, anchor + "\nuniform int uMultisample;", 1)
        self.assertIn("runtime_branch_combined_sampler_program_forbidden",
                      MODULE.validate_texts(texts))

    def test_single_sample_lane_mapping_is_immutable(self):
        self.mutation_fails("renderer",
            "uvec4(texelFetch(uSemanticSingle,px,0).r,0u,0u,0u)",
            "uvec4(0u,texelFetch(uSemanticSingle,px,0).r,0u,0u)")

    def test_resolve_program_route_selection_is_required(self):
        self.mutation_fails("renderer",
            "impl_->finalOwnershipResolveMsaaProgram:\n"
            "                    impl_->finalOwnershipResolveSingleProgram",
            "impl_->finalOwnershipResolveSingleProgram:\n"
            "                    impl_->finalOwnershipResolveMsaaProgram")

    def test_legacy_ppm_gate_cannot_be_authoritative(self):
        self.mutation_fails("runner_generator",
            '        "")',
            '        "                    contrast.ok &&\\n")')

    def test_component_boundary_serialization_is_required(self):
        self.mutation_fails("telemetry",
            'row["component_aware_boundary_records"] = json::Value::array();')

    def test_boundary_sort_contract_includes_probe_local_distance(self):
        self.mutation_fails("resolver", '"normal_distance="')

    def test_owned_island_association_is_required(self):
        self.mutation_fails("resolver", "class OwnedIslandUnionFind")

    def test_exact_primitive_stem_association_is_required(self):
        self.mutation_fails("resolver",
            'associationMode = "exact_primitive_stem";')

    def test_area_owner_provenance_is_serialized(self):
        self.mutation_fails("telemetry",
            'value["boundary_area_owner_source_probe_id"]')

    def test_no_eligible_boundary_reason_does_not_collapse(self):
        self.mutation_fails("visibility",
            'return "no_eligible_component_aware_boundary";')

    def test_adaptive_black_final_ldr_outer_rim_is_pinned(self):
        self.mutation_fails("trajectory_header",
            "adaptiveOuterBlackFinalSrgb8{{0, 0, 0}}",
            "adaptiveOuterBlackFinalSrgb8{{32, 32, 32}}")

    def test_adaptive_white_final_ldr_outer_rim_is_pinned(self):
        self.mutation_fails("trajectory_header",
            "adaptiveOuterWhiteFinalSrgb8{{255, 255, 255}}",
            "adaptiveOuterWhiteFinalSrgb8{{220, 220, 220}}")

    def test_outer_band_cannot_regress_to_full_fill(self):
        self.mutation_fails("trajectory",
            "void appendNonOverwritingOuterBand(",
            "void appendFullOuterFill(")

    def test_visual_sublayer_provenance_is_required(self):
        self.mutation_fails("trajectory_header",
            "TrajectoryVisualSublayer visualSublayer")

    def test_two_pixel_inner_cap_annulus_is_required(self):
        self.mutation_fails("trajectory",
            "style.predictedCoreCapExtensionPx + 2.0",
            "style.predictedCoreCapExtensionPx + 0.5")

    def test_scene_and_actual_composited_backdrops_are_both_required(self):
        texts = copy.deepcopy(self.texts)
        token = "trajectoryCompositedBackdropFramebuffer"
        self.assertIn(token, texts["renderer"])
        texts["renderer"] = texts["renderer"].replace(
            token, "missingCompositedBackdropFramebuffer")
        self.assertTrue(MODULE.validate_texts(texts))

    def test_prior_actual_disables_scene_only_suppression(self):
        self.mutation_fails("renderer",
            "!priorActualCoverage&&contrastRatio(innerFinal,backdropFinal)>=4.0",
            "contrastRatio(innerFinal,backdropFinal)>=4.0")

    def test_physical_texel_fetch_is_required(self):
        self.mutation_fails("renderer", "ivec2(gl_FragCoord.xy)",
            "ivec2(vColor.xy)")

    def test_visual_sublayer_is_serialized(self):
        texts = copy.deepcopy(self.texts)
        token = 'value["owner_visual_sublayer"]'
        self.assertIn(token, texts["telemetry"])
        texts["telemetry"] = texts["telemetry"].replace(token, "removed")
        self.assertTrue(MODULE.validate_texts(texts))

    def test_visual_unique_area_cap_is_required(self):
        self.mutation_fails("trajectory",
            "style.predictedCoreCapExtensionPx,", "0.0,")

    def test_exact_two_draw_submission_gate_is_required(self):
        self.mutation_fails("runner_generator",
            "metrics.trajectoryOverlayDrawCalls == 2",
            "metrics.trajectoryOverlayDrawCalls == 1")

    def test_machine_predicate_ledger_is_required(self):
        self.mutation_fails("runner_generator",
            "telemetry.machinePredicateLedgerRequired = true;",
            "telemetry.machinePredicateLedgerRequired = false;")

    def test_machine_predicate_ledger_is_serialized(self):
        self.mutation_fails("telemetry",
            'row["machine_predicate_ledger"].asArray().push_back(value);',
            'row["removed_machine_predicate_ledger"].asArray().push_back(value);')

    def test_midpoint_freeze_observation_is_required(self):
        self.mutation_fails("renderer",
            "trajectoryOverlayActualBackdropFrozen = true",
            "trajectoryOverlayActualBackdropFrozen = false")

    def test_ordered_submission_observation_is_required(self):
        self.mutation_fails("renderer",
            "trajectoryOverlaySubmissionOrderValid =",
            "removedSubmissionOrderValid =")

    def test_probe_coordinate_function_is_immutable(self):
        texts = copy.deepcopy(self.texts)
        anchor = "style.haloWidthPx + 24.0;"
        self.assertIn(anchor, texts["trajectory"])
        texts["trajectory"] = texts["trajectory"].replace(
            anchor, "style.haloWidthPx + 23.0;", 1)
        self.assertIn(
            "immutable_probe_function_drift:appendContrastProbe",
            MODULE.validate_texts(texts))


if __name__ == "__main__":
    unittest.main()
