#!/usr/bin/env python3
"""Fail-closed validator for the V19 combined Release target-closure gate."""

from __future__ import annotations

import hashlib
import json
from pathlib import Path
import sys

ROOT = Path(__file__).resolve().parents[1]
DEFAULT = ROOT / "docs/art/phase4-v19-combined-release-target-closure-design-gate-v1.json"
SCHEMA = "billiardgl.phase4.v19.combined_release_target_closure_design_gate.v1"
STATUS = "FINAL_DESIGN_ONLY_WAIT_D6_A_CMAKE_SOURCE_STABLE_REBIND_NO_IMPLEMENTATION_NO_BUILD_NO_GPU"

CHECKLIST = "docs/art/phase4-v19-combined-release-readiness-successor-checklist-v1.json"
CHECKLIST_SHA = "73e72e0e0f0833543c7d80d7525dcb1813f34f5f7cb31c8f41fac2ea1af4d9fe"
CP4_ACCEPTANCE = "docs/art/phase4-v19-cp4-d-visible-integration-independent-ta-acceptance-addendum-v1.json"
CP4_ACCEPTANCE_SHA = "c7b80d14a3e6d260da5594d347de24a3bb31eb07c83d22c765c848e339d1fcef"
CP4_RECEIPT = "docs/evidence/phase4-v19-cp4-d-visible-integration-implementation-v1/receipt.json"
CP4_RECEIPT_SHA = "fe341d320050e5cba3d67655631e0dfb314b4ccb39d880300ef3278a49dbc9c0"
CMAKE_SHA = "871c7916e2d1033073ca880c1c5e61263dff5f97bb28405fe9fd9a95c40d1dc2"
D6_A_GATE = "docs/rules/m4-d6-a-3d-table-departure-physics-foundation-implementation-design-gate-v1.json"
D6_A_GATE_SHA = "676b70795fc45e4e3cc4f5c8ffa3a236c1e1e83aea9371e422ff9b54e87264d1"

V19_SOURCES = [
    "src/Billiards/training/trajectory_inset_model.cpp",
    "src/Billiards/training/trajectory_inset_renderer.cpp",
    "src/Billiards/render/modern/trajectory_inset_atomic_transaction.cpp",
    "src/Billiards/render/modern/trajectory_inset_visible_integration.cpp",
]
SOURCE_BINDINGS = {
    V19_SOURCES[0]: "58a9a3dfe922c235314884d36bd8cea47df8cfe400adcf5a4a716e64e093e2b4",
    V19_SOURCES[1]: "9969c1736fb89329e25dfa1d0252f18e5475d18a9027ddebf740cab4e8804201",
    V19_SOURCES[2]: "183f630052f7676b37f0b66da6f6b2725faf6da930f6dc3cea79c6b160a4c6b2",
    V19_SOURCES[3]: "0fbd08d194e3626f562fd54476d2be3a53d5f2edb08c5b0505d0b3afdff85580",
    "src/Billiards/render/modern/modern_renderer.cpp": "8ab8ba8b10a473924d5294e69b8bb008d670391fd60c6294d8cfd5549dfdb1e3",
    "tests/phase4_v19_trajectory_inset_visible_integration_cpu_tests.cpp": "cc00e8b5f5d8fab6e0cc955378aa8e0aa04d735fb7ec623bda3a502e4a830b74",
}
OWNER = "BilliardsPhase4V19TrajectoryInsetImplementation"
CPU_TARGET = "BilliardsPhase4V19TrajectoryInsetVisibleIntegrationCpuTests"
FINAL_TARGET = "BilliardsPhase4V19ModernRendererCp4FinalLinkTests"
FINAL_SOURCE = "tests/phase4_v19_modern_renderer_cp4_final_link_tests.cpp"
ANCHORS = [
    "billiardgl::render::ModernRenderer::prepareTrajectoryInsetVisibleFinalStaging",
    "billiardgl::training::trajectoryInsetRoleName",
    "billiardgl::training::trajectoryInsetSampleProfile1x",
    "billiardgl::render::modern::validateTrajectoryInsetAtomicIdentity",
    "billiardgl::render::modern::orderedTrajectoryInsetVisibleAtomicFinalStages",
]
OLD_COMMENT = "Modern renderer foundation. This library is intentionally not linked into the production executable until the dual-backend migration reaches integration."
NEW_COMMENT = "Modern renderer foundation. The Apple production executable links this library; V19 inset implementation objects are supplied by the dedicated target-closure dependency."


def sha(path: Path) -> str:
    return hashlib.sha256(path.read_bytes()).hexdigest()


def validate(path: Path = DEFAULT) -> list[str]:
    try:
        value = json.loads(path.read_text(encoding="utf-8"))
    except Exception as error:
        return [f"gate_invalid:{error}"]
    errors: list[str] = []

    def require(condition: bool, message: str) -> None:
        if not condition:
            errors.append(message)

    require(value.get("schema") == SCHEMA and value.get("schema_version") == 1 and value.get("status") == STATUS, "schema_or_status_invalid")
    expected_bindings = [
        {"role": "combined_release_successor_checklist", "path": CHECKLIST, "sha256": CHECKLIST_SHA},
        {"role": "cp4_d_independent_acceptance", "path": CP4_ACCEPTANCE, "sha256": CP4_ACCEPTANCE_SHA},
        {"role": "cp4_d_implementation_receipt", "path": CP4_RECEIPT, "sha256": CP4_RECEIPT_SHA},
        {"role": "current_cmake_external_accepted_d5_d_lineage", "path": "CMakeLists.txt", "sha256": CMAKE_SHA},
        {"role": "d6_a_approved_successor_lineage_definition", "path": D6_A_GATE, "sha256": D6_A_GATE_SHA},
    ]
    require(value.get("frozen_bindings") == expected_bindings, "frozen_bindings_invalid")
    for binding in expected_bindings:
        target = ROOT / binding["path"]
        require(target.is_file() and sha(target) == binding["sha256"], "binding_missing_or_drift:" + binding["role"])

    require(value.get("current_source_bindings") == SOURCE_BINDINGS, "source_bindings_invalid")
    for source, expected in SOURCE_BINDINGS.items():
        target = ROOT / source
        require(target.is_file() and sha(target) == expected, "source_missing_or_drift:" + source)

    cmake = (ROOT / "CMakeLists.txt").read_text(encoding="utf-8")
    gap = value.get("observed_current_cmake_gap", {})
    require(gap.get("zero_reference_sources_exact") == V19_SOURCES, "gap_source_inventory_invalid")
    require(gap.get("four_v19_cpp_reference_count_each") == 0, "gap_reference_count_claim_invalid")
    require(all(cmake.count(source) == 0 for source in V19_SOURCES), "current_cmake_gap_no_longer_exact")
    require(gap.get("modern_renderer_consumes_cp1_cp3_cp4_symbols") is True, "modern_renderer_consumption_missing")
    require(gap.get("static_archive_creation_allows_unresolved_symbols") is True and gap.get("archive_only_or_unrelated_smoke_success_is_final_link_closure") is False, "archive_boundary_invalid")
    require(gap.get("existing_smoke_may_not_pull_modern_renderer_object") is True and gap.get("fresh_release_closure_currently_proven") is False, "smoke_or_closure_false_claim")
    stale_comment_present = (
        "Modern renderer foundation. This library is intentionally not linked into the" in cmake
        and "production executable until the dual-backend migration reaches integration." in cmake
    )
    require(gap.get("stale_comment_exact") == OLD_COMMENT and stale_comment_present, "stale_comment_observation_invalid")
    apple_link = "target_link_libraries(Billiards PRIVATE\n        BilliardsModernGpuResources"
    require(gap.get("apple_branch_actually_links_modern_gpu_resources_to_billiards") is True and apple_link in cmake, "apple_production_link_observation_invalid")

    graph = value.get("root_selected_future_target_graph", {})
    owner = graph.get("implementation_owner", {})
    require(owner.get("target") == OWNER and owner.get("type") == "STATIC", "owner_target_invalid")
    require(owner.get("sources_exact") == V19_SOURCES and owner.get("source_occurrences_across_all_cmake_target_source_lists_each") == 1, "owner_source_contract_invalid")
    require(owner.get("direct_link_dependencies_exact") == ["BilliardsModernProductUiRuntime"], "owner_dependency_invalid")
    require(owner.get("consumer_edge") == f"BilliardsModernGpuResources PRIVATE {OWNER}", "owner_consumer_edge_invalid")
    require(owner.get("duplicate_compilation_into_executables_or_other_libraries_forbidden") is True, "duplicate_compilation_allowed")

    cpu = graph.get("formal_v19_cpu_test", {})
    require(cpu.get("target") == CPU_TARGET and cpu.get("source") == "tests/phase4_v19_trajectory_inset_visible_integration_cpu_tests.cpp", "cpu_target_invalid")
    require(cpu.get("direct_link_dependencies_exact") == [OWNER, "BilliardsModernProductUiRuntime", "BilliardsProductUiOpenGL41"], "cpu_dependencies_invalid")
    require(cpu.get("actual_core41_link_shim_required") is True and cpu.get("registered_with_ctest") is True and cpu.get("release_run_required") is True, "cpu_execution_contract_invalid")

    final = graph.get("modern_renderer_cp4_final_link_test", {})
    require(final.get("target") == FINAL_TARGET and final.get("new_source") == FINAL_SOURCE, "final_target_invalid")
    require(final.get("direct_link_dependencies_exact") == ["BilliardsModernGpuResources", "BilliardsGltfGpuBridge"], "final_dependencies_invalid")
    require(final.get("must_not_link_implementation_owner_directly") is True and final.get("must_instantiate_modern_renderer") is True, "final_pull_contract_invalid")
    require(final.get("must_call_preinitialized_cp4_visible_method_and_require_fail_closed_result") == "ModernRenderer::prepareTrajectoryInsetVisibleFinalStaging returns false with trajectory_inset_final_missing_cp3_staging", "final_runtime_anchor_invalid")
    require(final.get("required_symbol_anchors_exact") == ANCHORS and final.get("link_map_or_demangled_symbol_inventory_must_contain_all_anchors") is True, "final_symbol_inventory_invalid")
    require(final.get("whole_archive_or_force_load_substitution_for_real_references_forbidden") is True, "force_load_allowed")
    require(final.get("registered_with_ctest") is True and final.get("release_run_required") is True and final.get("creates_gl_context_or_executes_gpu") is False, "final_execution_boundary_invalid")

    comment = graph.get("stale_comment_correction", {})
    require(comment.get("old_comment_must_be_absent") is True and comment.get("replacement_exact") == NEW_COMMENT and comment.get("runtime_or_link_behavior_change_from_comment_edit") is False, "comment_correction_invalid")
    require(value.get("future_implementation_allowlist_after_d6_a_rebind_only") == ["CMakeLists.txt", FINAL_SOURCE], "implementation_allowlist_invalid")

    future = value.get("future_release_closure_acceptance_not_run", {})
    require(future.get("fresh_build_directory") is True and future.get("cmake_build_type") == "Release", "fresh_release_contract_invalid")
    required_targets = ["BilliardsProductUiGpuCore", "BilliardsProductUiOpenGL41", "BilliardsModernProductUiRuntime", OWNER, "BilliardsModernGpuResources", CPU_TARGET, FINAL_TARGET]
    require(future.get("required_targets_exact") == required_targets, "required_target_inventory_invalid")
    require(future.get("archive_only_build_is_acceptance") is False and future.get("smoke_only_build_is_acceptance") is False, "archive_or_smoke_false_acceptance")
    require(future.get("final_link_binary_sha_and_link_map_or_symbol_inventory_required") is True and future.get("cp4_d_standalone_validator_replay_required") is True and future.get("all_object_source_cmake_command_stdout_stderr_hashes_required") is True, "future_receipt_contract_invalid")
    require(all(future.get(key) is False for key in ("configure_run", "release_build_run", "cpu_tests_run", "final_link_pass", "closure_receipt_created")), "false_future_execution_claim")

    barrier = value.get("d6_a_successor_lineage_barrier", {})
    require(barrier.get("current_cmake_baseline_sha256") == CMAKE_SHA, "d6_a_baseline_invalid")
    require(barrier.get("d6_a_is_the_only_preimplementation_cmake_successor_lineage_allowed") is True and barrier.get("unknown_future_cmake_sha_is_accepted_by_this_gate") is False, "d6_a_successor_scope_invalid")
    prerequisites = barrier.get("required_before_v19_target_closure_implementation", [])
    require(len(prerequisites) == 4 and "SOURCE_STABLE" in prerequisites[0] and "separate V19 target-closure rebind addendum" in prerequisites[1] and "no V19 target-closure implementation yet" in prerequisites[2] and "Root and TA" in prerequisites[3], "d6_a_rebind_prerequisites_invalid")
    require(barrier.get("automatic_rebind_or_hash_wildcard_forbidden") is True, "automatic_rebind_allowed")
    require(all(barrier.get(key) is False for key in ("d6_a_cmake_source_stable_received", "rebind_addendum_frozen", "implementation_may_start")), "false_d6_a_or_implementation_claim")

    auth = value.get("authorization_boundary", {})
    require(auth.get("design_gate_complete") is True and auth.get("target_names_and_graph_selected") is True, "design_boundary_invalid")
    require(all(flag is False for key, flag in auth.items() if key not in {"design_gate_complete", "target_names_and_graph_selected"}), "false_authorization")
    return errors


def main() -> int:
    target = Path(sys.argv[1]).resolve() if len(sys.argv) > 1 else DEFAULT
    errors = validate(target)
    if errors:
        for error in errors:
            print("FAIL: " + error, file=sys.stderr)
        return 1
    print("PASS_PHASE4_V19_COMBINED_RELEASE_TARGET_CLOSURE_DESIGN_GATE")
    print(f"owner={OWNER} cpu={CPU_TARGET} final={FINAL_TARGET} sources=4 implementation=false build=false gpu=false")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
