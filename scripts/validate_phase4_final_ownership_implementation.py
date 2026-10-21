#!/usr/bin/env python3
"""CPU-only static conformance for Phase 4 integrated ownership instrumentation."""

from __future__ import annotations

import hashlib
import json
from pathlib import Path
import sys

ROOT = Path(__file__).resolve().parents[1]

BOUND = {
    "docs/art/phase4-final-ownership-v3-gl1282-offline-diagnostic-v1/minimal-correction-design-gate-v1.json":
        "22a9cb6f6393ecb70df148ca42f830a640162e80cc4810a3f9ac04656a692a6d",
    "docs/art/phase4-overlay-v4-final-ownership-instrumentation-implementation-design-gate-v1.json":
        "c3b6f032deaac6bd39d32755d4ab423915e8b70b08b6da18626707b8d7134056",
    "docs/art/phase4-overlay-v4-ordinal1-offline-diagnostic-v1/diagnostic-evidence-manifest-v1.json":
        "fb041c6e96c83f30df78742ead8a59251e3425f8b62999dd16390b349bdf7349",
    "docs/art/phase4-overlay-v4-ordinal1-offline-diagnostic-v1/root-cause-diagnostic-report-v1.json":
        "54d0bef6af28085f6e87456645802d4eb3e2537c6ca1869a57fece27141ab50b",
    "docs/art/phase4-overlay-v4-targeted-ordinal1-root-cause-diagnostic-gate-v1.json":
        "ad95db70125384a1de289792002aba05c3557d587a70ea849d5d168f415e57fa",
    "docs/art/phase4-training-replay-overlay-v4-targeted-failure-receipt-v1.json":
        "2dbd499249df1e44631669165a891f94d232f4fa436c9fd0910716de35d2717f",
    "docs/art/phase4-training-replay-overlay-v4-targeted-gpu-v1/camera_player_low_1080.ppm":
        "f3626f44ead6cfe4729e6ff3b76168747154b2452d0ca2afd70e1520b0e09744",
    "build-phase4-overlay-v4-ready-v3/generated/phase4_overlay_v4_gpu_capture_runner.mm":
        "35fa98ad44e438f11a6a5b50a80c06de00adb518de4d345d51e741283b195d43",
    "docs/art/phase4-final-ownership-v7-ordinal1-offline-diagnostic-v1/failure-evidence-receipt-v1.json":
        "3db8ccde3034d6dc6f59d546b75bbc5986d6692c13e6647c9fb5d8f9ed1d31ec",
    "docs/art/phase4-final-ownership-v7-ordinal1-offline-diagnostic-v1/minimal-owned-island-correction-design-gate-v1.json":
        "9f6c5febd88a06b22a8d05bf621f1310ddc8c854950dc02990cd857b21002cf2",
    "docs/art/phase4-final-ownership-v8-ordinal2-offline-diagnostic-v1/failure-evidence-receipt-v1.json":
        "7994824c30ef85e41bb4a80d0372e8fee8b011b832507ac18c09f09e7adb07b7",
    "docs/art/phase4-final-ownership-v8-ordinal2-offline-diagnostic-v1/minimal-visual-style-correction-design-gate-v1.json":
        "0fe967c8ca0e855c6cb0cd29f745aefd7dfcf8a9ae387eb1522d254753196526",
    "docs/art/phase4-training-replay-overlay-v4-final-ownership-targeted11-ready-v8/root-one-shot-gpu-authorization-v8-run1.json":
        "58e6e901dc1ef435cd3f0aa072267cece4aa278c6d9fa111bc38cd9bd623b1a4",
    "docs/art/phase4-training-replay-overlay-v4-final-ownership-targeted11-ready-v8/root-one-shot-gpu-authorization-v8-run1.json.consumed":
        "74b135e0f7054827cb7d81bbf117fe9fa096bdfe3fee9ea9d0e7ced73046a23e",
    "docs/art/phase4-training-replay-overlay-v4-final-ownership-targeted11-output-v8/phase4-final-ownership-dispatch-failure-receipt-v1.json":
        "1b4cac8f1df51e8b0d04a3e06770cddbf815b8a64377326f424c58e3901cfd89",
    "docs/art/phase4-training-replay-overlay-v4-final-ownership-targeted11-output-v8/results.jsonl.final-ownership.jsonl":
        "48d34fb11a342ffb7ff00f0729396ad48544bcfec4db43470bedd3d92148bd99",
    "docs/art/phase4-training-replay-overlay-v4-final-ownership-targeted11-output-v8/camera_elevated_1080.ppm":
        "1761d04326782eebaef2f92043d422d32f1140297bc64515a8a114d637c51da7",
    "docs/art/phase4-final-ownership-v9-ordinal1-offline-diagnostic-v1/failure-evidence-receipt-v1.json":
        "6bfcaf2c5ac5602deb557ccca176ebfd478c4183cf9bad6a054b6642f13a39eb",
    "docs/art/phase4-final-ownership-v9-ordinal1-offline-diagnostic-v1/minimal-compositing-style-correction-design-gate-v1.json":
        "e80baf05f0240872ba24b9ca9a6c7daa0d5a51496dde99902014d8ea3391a4f6",
    "docs/art/phase4-training-replay-overlay-v4-final-ownership-targeted11-ready-v9/root-one-shot-gpu-authorization-v9-run1.json":
        "67d06a6f7a8cd25fb38367beb0e8b975eeaf8dd6905dc250ec5ebb511b2d08c0",
    "docs/art/phase4-training-replay-overlay-v4-final-ownership-targeted11-ready-v9/root-one-shot-gpu-authorization-v9-run1.json.consumed":
        "2b20f90a76309fa425264580582f020b94120f0cb102af0949f18516e983e66e",
    "docs/art/phase4-training-replay-overlay-v4-final-ownership-targeted11-output-v9/results.jsonl.final-ownership.jsonl":
        "3f40581fc4f66861a3a5f0953d6350619ca3195ad0c560f1202c5458dd66f62a",
    "docs/art/phase4-training-replay-overlay-v4-final-ownership-targeted11-output-v9/phase4-final-ownership-dispatch-failure-receipt-v1.json":
        "597bbf92be859cf0d4e37cbf677140bdafa286cde59412077a75416fcf035f0a",
    "docs/art/phase4-final-ownership-v10-ordinal1-offline-diagnostic-v1/failure-evidence-receipt-v1.json":
        "c78fa8df1e74dbe377b97bdf3f28949bb8a778abc743952abcdb308f3693ffdc",
    "docs/art/phase4-final-ownership-v10-ordinal1-offline-diagnostic-v1/minimal-machine-contract-correction-design-gate-v1.json":
        "98575c3e3c48e99e9a056c047ba59e268a23c038d2685ed7252edd5d0cbe525a",
    "docs/art/phase4-training-replay-overlay-v4-final-ownership-targeted11-ready-v10/root-one-shot-gpu-authorization-v10-run1.json":
        "dc0c60ddba2bef6a86ee4c1e328727543e24de5153260af905afd5f8d383fa8f",
    "docs/art/phase4-training-replay-overlay-v4-final-ownership-targeted11-ready-v10/root-one-shot-gpu-authorization-v10-run1.json.consumed":
        "6cd0c9ec5d07b4ecd637aca6bff25048897e51541bb13e0d29571c3635c0a33c",
    "docs/art/phase4-training-replay-overlay-v4-final-ownership-targeted11-output-v10/results.jsonl.final-ownership.jsonl":
        "d8e06f0aa0e77d0e677af72ce2250a563d2595b70b790b05ad5728c9cc59c94d",
    "docs/art/phase4-training-replay-overlay-v4-final-ownership-targeted11-output-v10/phase4-final-ownership-dispatch-failure-receipt-v1.json":
        "5b5923a8efbce6f293af4d2fd71ce2d4c016d2faaabc5dbd4d19600ea10772ae",
    "docs/art/phase4-training-replay-overlay-v4-final-ownership-targeted11-output-v10/camera_player_low_1080.ppm":
        "82d6f9e5aba1821b3b27c00f2b76977fff035cf8541c768bd5361c1ee4ee744a",
    "docs/art/phase4-final-ownership-v11-ordinal4-offline-diagnostic-v1/failure-evidence-receipt-v1.json":
        "07f251d466b0dcad550b4951e8242403bcaaaeb97dd800cbde99cdf23c2f3f85",
    "docs/art/phase4-final-ownership-v11-ordinal4-offline-diagnostic-v1/minimal-raster-support-connectivity-correction-design-gate-v1.json":
        "59bbabf437e9195392b2a41f4e48eea0513afb222706115a07e9c0965cbad059",
}

IMMUTABLE_SOURCE_SHA256 = {
    "src/Billiards/render/modern/modern_renderer.cpp":
        "b00cc6b370122275a42f106967f935dbb34bd5d3f81f2a750ca9ba6e503af191",
    "src/Billiards/training/final_semantic_ownership.cpp":
        "c4c91683b2de8bfdd6a4d256c6cd105023a4a9bd4113467a1bba012d3fe9aeba",
    "src/Billiards/training/final_semantic_ownership.h":
        "3f21ce72ee1946182d431588464831d364003e91fbf02e3ce2b7852ca4266057",
    "src/Billiards/training/trajectory_compound_visibility.cpp":
        "4b3defeed60bf931b8c7f19e8a0da73c82ab983830af23792a8ef1e6a29b2921",
    "src/Billiards/training/trajectory_compound_visibility.h":
        "4babc8a79e9b69978153dda1230de6855cfaa2f58b3a38d7e3c6ae90330e5890",
    "src/Billiards/training/phase4_final_ownership_instrumentation.cpp":
        "378d5373cb215634bea4a6a62e6c2c72d85b8a2cfd04c1055fae2896c4d0898b",
    "src/Billiards/training/phase4_final_ownership_instrumentation.h":
        "d75d5bebf95eea48ae6bf97b94f97976101b1fbac8a96d24fd9abf1d11beac61",
}

PROBE_FUNCTION_SHA256 = {
    "appendContrastProbe":
        "e4fe2ff3696b17133818bf2c7a0af34c9ca51331fa62fae3147fba88e9598746",
    "appendTickContrastProbe":
        "0359351e0ef7088cff7fd81f842d61e16cf5568df0d639c45b2a791c9025ee01",
}

SOURCE_PATHS = {
    "renderer": "src/Billiards/render/modern/modern_renderer.cpp",
    "renderer_header": "src/Billiards/render/modern/modern_renderer.h",
    "ui_shader": "src/Billiards/render/ui/product_ui_gpu_renderer.cpp",
    "trajectory_header": "src/Billiards/training/trajectory_overlay_renderer.h",
    "trajectory": "src/Billiards/training/trajectory_overlay_renderer.cpp",
    "resolver_header": "src/Billiards/training/final_semantic_ownership.h",
    "resolver": "src/Billiards/training/final_semantic_ownership.cpp",
    "telemetry": "src/Billiards/training/phase4_final_ownership_instrumentation.cpp",
    "telemetry_header": "src/Billiards/training/phase4_final_ownership_instrumentation.h",
    "visibility": "src/Billiards/training/trajectory_compound_visibility.cpp",
    "visibility_header": "src/Billiards/training/trajectory_compound_visibility.h",
    "runner_generator": "scripts/generate_phase4_overlay_v4_gpu_runner.py",
}


def sha(path: Path) -> str:
    return hashlib.sha256(path.read_bytes()).hexdigest()


def validate_texts(texts: dict[str, str]) -> list[str]:
    errors: list[str] = []

    def require(key: str, token: str, error: str) -> None:
        if token not in texts[key]:
            errors.append(error)

    require("renderer_header", "bool enableFinalOwnershipInstrumentation = false;",
            "capture_only_option_not_default_false")
    require("renderer", "GL_COLOR_ATTACHMENT1", "semantic_mrt_missing")
    require("renderer", "GL_R32UI", "per_sample_integer_owner_target_missing")
    require("renderer", "GL_RGBA32UI", "four_sample_resolve_payload_missing")
    require("renderer", "glTexImage2DMultisample", "production_msaa_attachment_missing")
    require("renderer", "glGetMultisamplefv(GL_SAMPLE_POSITION",
            "production_sample_positions_missing")
    require("renderer", "finalOwnershipToken=vFinalOwnershipToken",
            "same_draw_semantic_fragment_output_missing")
    require("renderer", "captureIntegratedFinalOwnership",
            "paired_integrated_readback_missing")
    require("renderer", "frame.colorFrameId=", "color_frame_identity_missing")
    require("renderer", "frame.semanticFrameId=frame.colorFrameId",
            "semantic_color_frame_pairing_missing")
    require("renderer", "integrated_semantic_msaa_resolve",
            "semantic_resolve_pass_missing")
    require("renderer", "kFinalOwnershipResolveSingleFragment",
            "single_sample_resolve_variant_missing")
    require("renderer", "kFinalOwnershipResolveMsaaFragment",
            "msaa_resolve_variant_missing")
    require("renderer", "finalOwnershipResolveSingleProgram",
            "single_sample_linked_program_missing")
    require("renderer", "finalOwnershipResolveMsaaProgram",
            "msaa_linked_program_missing")
    require("renderer", "aces_tone_map", "tone_map_pass_missing")
    require("renderer", "product_ui", "later_product_ui_pass_missing")
    require("ui_shader",
            "layout(location = 1) out vec4 outFinalOwnershipUiCoverage;",
            "same_ui_draw_coverage_output_missing")
    require("ui_shader", "vClipPixels", "ui_clip_not_shared")
    require("ui_shader", "vColorLinear.a * coverage",
            "ui_alpha_sdf_coverage_not_shared")
    require("trajectory_header", "finalOwnershipToken = 0",
            "semantic_token_not_on_production_vertex_stream")
    require("trajectory_header", "double localTangentX = 1.0;",
            "true_probe_tangent_payload_missing")
    require("trajectory", "result.vertices[vertex].finalOwnershipToken = token",
            "semantic_token_ranges_not_bound_to_exact_vertices")
    require("trajectory", "Actual is the continuous base; predicted dashes are composited last.",
            "actual_then_predicted_order_anchor_missing")
    require("trajectory_header", "double outerContrastRimWidthPx = 0.0;",
            "visual_outer_rim_width_contract_missing")
    require("trajectory_header", "double predictedCoreCapExtensionPx = 0.0;",
            "visual_unique_area_cap_contract_missing")
    require("trajectory_header",
            "adaptiveOuterBlackFinalSrgb8{{0, 0, 0}}",
            "adaptive_black_rim_final_ldr_contract_changed")
    require("trajectory_header",
            "adaptiveOuterWhiteFinalSrgb8{{255, 255, 255}}",
            "adaptive_white_rim_final_ldr_contract_changed")
    require("trajectory_header",
            "predictedInnerHaloFinalSrgb8{{0, 0, 0}}",
            "predicted_inner_halo_final_ldr_contract_changed")
    require("trajectory_header",
            "actualInnerHaloFinalSrgb8{{255, 255, 255}}",
            "actual_inner_halo_final_ldr_contract_changed")
    require("trajectory_header", "TrajectoryVisualSublayer visualSublayer",
            "visual_sublayer_provenance_missing")
    require("trajectory", "appendNonOverwritingOuterBand(",
            "non_overwriting_outer_band_missing")
    require("trajectory", "style.predictedCoreCapExtensionPx + 2.0",
            "predicted_inner_cap_annulus_below_2px")
    require("trajectory", "predictedOuterRimVertices.begin()",
            "predicted_outer_range_missing")
    require("trajectory", "predictedInnerHaloVertices.begin()",
            "predicted_inner_range_missing")
    require("trajectory", "actualOuterRimVertices.begin()",
            "actual_outer_range_missing")
    require("trajectory", "actualInnerHaloVertices.begin()",
            "actual_inner_range_missing")
    require("trajectory", "selectTrajectoryAdaptiveOuterRim(",
            "cpu_adaptive_polarity_contract_missing")
    require("trajectory", "style.predictedCoreCapExtensionPx,",
            "predicted_unique_area_cap_not_applied")
    require("trajectory", "style.outerContrastRimWidthPx = 1.0 * scale;",
            "scaled_outer_rim_contract_changed")
    require("trajectory", "style.predictedCoreCapExtensionPx = 1.0 * scale;",
            "scaled_core_cap_contract_changed")

    trajectory = texts["trajectory"]
    band_start = trajectory.find("void appendNonOverwritingOuterBand")
    band_end = trajectory.find("void appendExtendedThickSegment", band_start)
    band = trajectory[band_start:band_end]
    if band_start < 0 or band_end < 0 or \
            "appendThickSegment" in band or \
            band.count("appendQuad(vertices") < 2 or \
            "outerCapExtension > innerCapExtension" not in band:
        errors.append("outer_band_is_not_narrow_non_overwriting_geometry")
    probe_boundaries = {
        "appendContrastProbe": "appendTickContrastProbe",
        "appendTickContrastProbe": "appendDisc",
    }
    for function, next_function in probe_boundaries.items():
        start = trajectory.find("void " + function)
        end = trajectory.find("void " + next_function, start)
        digest = hashlib.sha256(trajectory[start:end].encode()).hexdigest() \
            if start >= 0 and end >= 0 else "MISSING"
        if digest != PROBE_FUNCTION_SHA256[function]:
            errors.append(f"immutable_probe_function_drift:{function}")
    require("resolver", "request.minimumEdgeContrastRatio",
            "contrast_threshold_not_enforced")
    require("resolver_header", "double minimumVisibleCoverage = 0.90;",
            "coverage_threshold_changed")
    require("resolver_header", "std::size_t expectedSourceProbeCount = 95;",
            "ordinal1_denominator_changed")
    require("resolver_header", "double minimumEdgeContrastRatio = 4.0;",
            "contrast_floor_changed")
    require("resolver_header", "double minimumResolvedPixelEquivalentArea = 1.5;",
            "owned_area_floor_changed")
    require("resolver_header", "std::size_t minimumRasterConnectedPhysicalPixels = 2;",
            "raster_connected_pixel_floor_changed")
    require("resolver", "std::min(12.0, probe.sourceProbeSpacingPx * 0.5)",
            "tangent_bound_changed")
    require("resolver", "dx * probe.tangentX +",
            "true_tangent_projection_missing")
    require("resolver", "dx * probe.normalX +",
            "true_normal_projection_missing")
    require("telemetry", "appendPhase4CaptureTelemetryRowDurable(path, input)",
            "pre_gate_durable_append_missing")
    require("telemetry", "if (!input.machinePass)",
            "unchanged_machine_gate_missing")
    require("telemetry", "candidateSourceIds != sourceIds",
            "complete_candidate_inventory_check_missing")
    require("telemetry", "arbitratedSourceIds != sourceIds",
            "complete_arbitration_inventory_check_missing")
    require("telemetry",
            'row["component_aware_boundary_records"] = json::Value::array();',
            "complete_component_boundary_serialization_missing")
    require("resolver", "same_component_token_seam",
            "same_component_token_seam_classification_missing")
    require("resolver", "same_track_compound_inner_edge",
            "compound_inner_edge_classification_missing")
    require("resolver", "compound_outer_silhouette",
            "compound_outer_silhouette_classification_missing")
    require("resolver", "ownerRole == CompoundComponentRole::OuterHaloEdge",
            "outer_halo_role_aware_pairing_missing")
    require("resolver", "boundary.deterministicSortKey",
            "deterministic_boundary_selection_missing")
    require("resolver", '"role_rank="',
            "role_rank_missing_from_boundary_sort_contract")
    require("resolver", '"normal_distance="',
            "normal_distance_missing_from_boundary_sort_contract")
    require("resolver", '"tangent_distance="',
            "tangent_distance_missing_from_boundary_sort_contract")
    require("resolver", '"direction="',
            "direction_missing_from_boundary_sort_contract")
    require("resolver", "class OwnedIslandUnionFind",
            "pre_arbitration_owned_island_graph_missing")
    require("resolver", 'associationMode = "exact_primitive_stem";',
            "exact_primitive_stem_association_missing")
    require("resolver", 'associationMode = "owned_island_component";',
            "owned_island_component_association_missing")
    require("resolver", "exclusiveAreaOwnerSourceIds",
            "exclusive_area_owner_provenance_missing")
    require("telemetry", 'value["association_mode"]',
            "boundary_association_mode_not_serialized")
    require("telemetry", 'value["boundary_area_owner_source_probe_id"]',
            "boundary_area_owner_provenance_not_serialized")
    require("telemetry", 'value["owner_visual_sublayer"]',
            "owner_visual_sublayer_not_serialized")
    require("telemetry", 'value["adjacent_visual_sublayer"]',
            "adjacent_visual_sublayer_not_serialized")
    require("visibility", 'return "no_eligible_component_aware_boundary";',
            "owned_without_boundary_reason_collapsed")
    require("visibility_header", "NoEligibleComponentAwareBoundary",
            "owned_without_boundary_reason_enum_missing")
    require("runner_generator", "telemetry.legacyPpmDiagnosticOnly = true;",
            "legacy_ppm_diagnostic_serialization_missing")
    require("runner_generator",
            "telemetry.componentAwareBoundaryRecords = ownership.boundaryTelemetry;",
            "runner_boundary_telemetry_binding_missing")
    require("runner_generator",
            'metrics.trajectoryOverlayDrawCalls == 2',
            "strict_exact_two_draw_submission_gate_missing")
    require("runner_generator",
            "telemetry.machinePredicateLedgerRequired = true;",
            "capture_machine_predicate_ledger_not_required")
    require("runner_generator",
            "telemetry.machinePredicateLedger = machinePredicateLedger;",
            "capture_machine_predicate_ledger_not_preserved")
    require("telemetry_header", "struct Phase4MachinePredicateTelemetry",
            "machine_predicate_telemetry_type_missing")
    require("telemetry",
            'row["machine_predicate_ledger"].asArray().push_back(value);',
            "machine_predicate_ledger_not_serialized")
    require("telemetry", 'row["failed_machine_predicate_names"]',
            "failed_machine_predicate_names_not_serialized")
    require("resolver", "preArbitrationSupportPixels",
            "pre_arbitration_topology_support_missing")
    require("resolver", "largestEightConnectedComponent",
            "deterministic_eight_neighbor_raster_continuity_missing")
    require("resolver", "telemetry.resolvedPixelEquivalentArea <",
            "exclusive_scalar_area_gate_missing")
    require("resolver", "telemetry.preArbitrationLargestEightConnectedPixelCount <",
            "pre_arbitration_topology_gate_missing")
    require("resolver", '"insufficient_exclusive_owned_area"',
            "exclusive_area_reason_split_missing")
    require("resolver", '"insufficient_raster_continuity"',
            "raster_continuity_reason_split_missing")
    require("telemetry", 'value["exclusive_owned_pixel_count"]',
            "exclusive_area_topology_provenance_missing")
    require("telemetry", 'value["pre_arbitration_support_pixel_count"]',
            "pre_arbitration_support_provenance_missing")
    require("telemetry",
            'value["pre_arbitration_largest_eight_connected_pixel_count"]',
            "eight_connected_topology_provenance_missing")
    generator = texts["runner_generator"]
    legacy_gate_removal = (
        '        "                    contrast.designTargetCoverage >= 0.99 &&\\n",\n'
        '        "")')
    if legacy_gate_removal not in generator:
        errors.append("legacy_ppm_still_authoritative_in_machine_gate")

    renderer = texts["renderer"]
    for token, error in (
        ("trajectoryBackdropFramebuffer", "scene_backdrop_target_missing"),
        ("trajectoryCompositedBackdropFramebuffer",
         "actual_composited_backdrop_target_missing"),
        ("uPreOverlayBackdropHdr", "adaptive_current_backdrop_sampler_missing"),
        ("uSceneBackdropHdr", "adaptive_scene_backdrop_sampler_missing"),
        ("ivec2(gl_FragCoord.xy)", "physical_texel_backdrop_coordinate_missing"),
        ("texelFetch(uPreOverlayBackdropHdr,px,0)",
         "current_backdrop_texel_fetch_missing"),
        ("texelFetch(uSceneBackdropHdr,px,0)",
         "scene_backdrop_texel_fetch_missing"),
        ("priorActualCoverage", "coaxial_prior_actual_detection_missing"),
        ("!priorActualCoverage&&contrastRatio(innerFinal,backdropFinal)>=4.0",
         "conditional_outer_suppression_missing"),
        ("actualCount > 0 && predictedCount > 0",
         "actual_then_predicted_midpoint_freeze_missing"),
        ("glDrawArrays(GL_TRIANGLES,0,actualCount)",
         "actual_range_submission_missing"),
        ("glDrawArrays(GL_TRIANGLES,actualCount,predictedCount)",
         "predicted_range_submission_missing"),
        ("trajectoryOverlayActualDrawSubmitted = true",
         "actual_submission_observation_missing"),
        ("trajectoryOverlayActualBackdropFrozen = true",
         "midpoint_freeze_observation_missing"),
        ("trajectoryOverlayPredictedDrawSubmitted = true",
         "predicted_submission_observation_missing"),
        ("trajectoryOverlaySubmissionOrderValid =",
         "ordered_submission_observation_missing")):
        require("renderer", token, error)
    single_start = renderer.find("const char* kFinalOwnershipResolveSingleFragment")
    msaa_start = renderer.find("const char* kFinalOwnershipResolveMsaaFragment")
    shader_end = renderer.find("void checkGl", msaa_start)
    single_shader = renderer[single_start:msaa_start]
    msaa_shader = renderer[msaa_start:shader_end]
    if single_start < 0 or msaa_start < 0 or shader_end < 0 or \
            "uniform usampler2D uSemanticSingle;" not in single_shader or \
            "usampler2DMS" in single_shader or \
            "uniform usampler2DMS uSemanticMsaa;" not in msaa_shader or \
            "usampler2D uSemanticSingle" in msaa_shader:
        errors.append("cross_type_active_sampler_program_alias_risk")
    if "uMultisample" in single_shader or "uMultisample" in msaa_shader or \
            "kFinalOwnershipResolveFragment" in renderer:
        errors.append("runtime_branch_combined_sampler_program_forbidden")
    if "uvec4(texelFetch(uSemanticSingle,px,0).r,0u,0u,0u)" not in \
            single_shader or not all(
                f"resolvedOwnerTokens.{lane}=texelFetch(uSemanticMsaa,px,{index}).r"
                in msaa_shader for index, lane in enumerate("xyzw")):
        errors.append("semantic_rgba32ui_lane_mapping_changed")
    if "impl_->options.msaaSamples?\n                    impl_->finalOwnershipResolveMsaaProgram:\n                    impl_->finalOwnershipResolveSingleProgram" not in renderer:
        errors.append("resolve_program_route_selection_missing")
    render_frame_start = renderer.find("void ModernRenderer::renderFrame")
    render_frame_end = renderer.find("bool ModernRenderer::capture(",
                                     render_frame_start)
    render_frame = renderer[render_frame_start:render_frame_end]
    positions = {
        name: render_frame.find(name) for name in (
            '"overlay"', '"integrated_semantic_msaa_resolve"',
            '"aces_tone_map"', '"product_ui"')
    }
    capture_position = renderer.find("captureIntegratedFinalOwnership")
    if not all(value >= 0 for value in positions.values()) or \
            capture_position < 0 or not (
        positions['"overlay"'] < positions['"integrated_semantic_msaa_resolve"'] <
        positions['"aces_tone_map"'] < positions['"product_ui"']
    ):
        errors.append("production_pass_graph_order_not_integrated")

    # Location 0 remains the original color equation; capture metadata writes
    # only location 1 and a distinct UI mask target.
    require("renderer", "}else color=vColor;",
            "trajectory_non_outer_color_equation_changed")
    require("renderer", "finalOwnershipToken=vFinalOwnershipToken;",
            "trajectory_semantic_output_missing")
    require("ui_shader", "outColor = vec4(vColorLinear.rgb, vColorLinear.a * coverage);",
            "product_ui_color_equation_changed")
    require("renderer", "instrumentationColorWritesDisabled=true",
            "color_invariance_readback_contract_missing")
    return errors


def validate(root: Path = ROOT) -> dict[str, object]:
    errors: list[str] = []
    bindings: dict[str, str] = {}
    for relative, expected in BOUND.items():
        path = root / relative
        actual = sha(path) if path.exists() else "MISSING"
        bindings[relative] = actual
        if actual != expected:
            errors.append(f"bound_input_drift:{relative}")
    for relative, expected in IMMUTABLE_SOURCE_SHA256.items():
        path = root / relative
        actual = sha(path) if path.exists() else "MISSING"
        if actual != expected:
            errors.append(f"immutable_algorithm_or_graph_source_drift:{relative}")
    texts: dict[str, str] = {}
    for key, relative in SOURCE_PATHS.items():
        path = root / relative
        if not path.exists():
            errors.append(f"implementation_source_missing:{relative}")
            texts[key] = ""
        else:
            texts[key] = path.read_text()
    errors.extend(validate_texts(texts))
    return {
        "schema": "billiardgl.phase4.final_ownership_static_conformance.v1",
        "status": "PASS" if not errors else "FAIL",
        "gpu_or_cocoa_invoked": False,
        "capture_authorized": False,
        "bound_inputs": bindings,
        "source_sha256": {
            relative: sha(root / relative)
            for relative in SOURCE_PATHS.values()
            if (root / relative).exists()
        },
        "checks": {
            "same_production_vertex_stream_and_ordered_track_ranges": not errors,
            "msaa_sample_payload_and_positions": not errors,
            "blend_resolve_tone_ui_clip_pairing": not errors,
            "default_off_and_color_attachment_zero_invariance": not errors,
            "coverage_contrast_denominator_route_contract_unchanged": not errors,
            "v8_v9_visual_gates_and_frozen_failures_bound": not errors,
            "visual_sublayers_adaptive_backdrops_and_final_ldr_colors": not errors,
            "probe_functions_and_algorithm_sources_immutable": not errors,
            "v10_exact_two_machine_predicate_ledger": not errors,
            "v11_pre_arbitration_eight_neighbor_topology": not errors,
        },
        "errors": errors,
    }


def main() -> int:
    result = validate()
    print(json.dumps(result, indent=2, sort_keys=True))
    return 0 if result["status"] == "PASS" else 1


if __name__ == "__main__":
    sys.exit(main())
