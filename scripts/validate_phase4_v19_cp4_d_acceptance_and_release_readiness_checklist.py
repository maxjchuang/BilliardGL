#!/usr/bin/env python3
"""Validate CP4-D independent acceptance and release/readiness successor."""

from __future__ import annotations

import hashlib
import json
from pathlib import Path
import sys

ROOT = Path(__file__).resolve().parents[1]
ADDENDUM = ROOT / "docs/art/phase4-v19-cp4-d-visible-integration-independent-ta-acceptance-addendum-v1.json"
CHECKLIST = ROOT / "docs/art/phase4-v19-combined-release-readiness-successor-checklist-v1.json"

ADDENDUM_SCHEMA = "billiardgl.phase4.v19.cp4_d_visible_integration_independent_ta_acceptance_addendum.v1"
CHECKLIST_SCHEMA = "billiardgl.phase4.v19.combined_release_readiness_successor_checklist.v1"

ADDENDUM_BINDINGS = {
    "docs/art/phase4-v19-cp1-trajectory-inset-foundation-independent-ta-acceptance-addendum-v1.json": "e89e7febf1266efbea751ff0d50ac1d4a6bf358317974f701202d02a5396d02e",
    "docs/art/phase4-v19-cp2-layout-draw-list-independent-ta-acceptance-addendum-v1.json": "6e7724df1a977af914b427538728f476d865db54c6cf95a3ae112e9de6172dd6",
    "docs/art/phase4-v19-cp3-offscreen-atomic-independent-ta-acceptance-addendum-v1.json": "46dd31775ce501969a8079171d8ff106c8de4bf9698b04ba0c09cfed21f6007d",
    "docs/art/phase4-v19-cp4-visible-integration-implementation-acceptance-checklist-v1.json": "f43bf8dba7b99673fb9a2db4a4cc9ddb686816c92de60ddc203721de80d5bc69",
    "docs/art/phase4-v19-cp4-d-product-ui-chrome-submission-correction-design-gate-v1.json": "dab3bd181ecff6f631249fc629b377b6e3e3a327bdc61e72358dde7467be54f7",
    "docs/art/phase4-v19-cp4-d-core41-restore-ownership-allowlist-correction-gate-v1.json": "9dc0516153fe16439699c80a47436971435e8f72c9ea7ac88a6ef9df29fe0c6c",
    "docs/evidence/phase4-v19-cp4-d-visible-integration-implementation-v1/receipt.json": "fe341d320050e5cba3d67655631e0dfb314b4ccb39d880300ef3278a49dbc9c0",
    "scripts/validate_phase4_v19_cp4_d_visible_integration_implementation.py": "690bd7786f5e562cf9ec9d09c498d07998c323acb9b0c02df53bbaae7f586fe0",
    "tests/test_phase4_v19_cp4_d_visible_integration_implementation.py": "785dcffcb3baf45950094ed5f8c302d08bc9a8d8a8a9afd8146c2ee59f2b2aec",
}

SOURCES = {
    "src/Billiards/training/trajectory_inset_model.h": "3a6e0550d1f85b9168528454b74c5768c5d6c9b9ba6db0dd8fd4fdac1c66a8c6",
    "src/Billiards/training/trajectory_inset_model.cpp": "58a9a3dfe922c235314884d36bd8cea47df8cfe400adcf5a4a716e64e093e2b4",
    "src/Billiards/training/trajectory_inset_renderer.h": "2e7191c1b260dc257a07f5868d0dc1a9dc0012e69696229f3fd0118c02af3947",
    "src/Billiards/training/trajectory_inset_renderer.cpp": "9969c1736fb89329e25dfa1d0252f18e5475d18a9027ddebf740cab4e8804201",
    "src/Billiards/render/modern/trajectory_inset_atomic_transaction.h": "69be6cb1e61385e0b70743bc80bb8ab01e9a483b1c79358ab1fc03d578a5ed64",
    "src/Billiards/render/modern/trajectory_inset_atomic_transaction.cpp": "183f630052f7676b37f0b66da6f6b2725faf6da930f6dc3cea79c6b160a4c6b2",
    "src/Billiards/render/modern/modern_product_ui_runtime.h": "8d8497e5c1c2edd0f7a3bfe19c234a777bb27fd4dcf5a09b019c5d3ce1f230b7",
    "src/Billiards/render/modern/modern_product_ui_runtime.cpp": "601c508facf5d83c09fd3fc603970e9caa7e22e5dbde10a8eae049b7081206b3",
    "src/Billiards/render/modern/modern_renderer_product_ui.h": "8b7b0cdb1b238502fdd970c88e1dfc99e833a39f60e1ae58fd7e7cdb93d72106",
    "src/Billiards/render/modern/modern_renderer_product_ui.cpp": "8a3e8ad0b280d5a9a951fd9be4941661718fd0b6099ef99ad697dead03d042c0",
    "src/Billiards/render/ui/product_ui_gpu_renderer.h": "45313df247fadb59c1aba0c8350398940de82451e99c3869784494c170d0861d",
    "src/Billiards/render/ui/product_ui_gpu_renderer.cpp": "df444cc870658cc2d8d770ec4439bfb51a1f1fb7273cf0a8717537e726c0b540",
    "src/Billiards/render/modern/product_ui_gl_core41.h": "65465946d36dee6b49687568847a187cf6fbc4b50c9da5fcfaee14d2b982e571",
    "src/Billiards/render/modern/product_ui_gl_core41.cpp": "10918cd69a19ac52714eec71f3b2e4f277f3dcf151374898840b12add7cd363f",
    "src/Billiards/render/modern/trajectory_inset_visible_integration.h": "823f9d3560cdc930e6012b56446307d26314e6d7ac4786a8c8d2448dc8fcec74",
    "src/Billiards/render/modern/trajectory_inset_visible_integration.cpp": "0fbd08d194e3626f562fd54476d2be3a53d5f2edb08c5b0505d0b3afdff85580",
    "src/Billiards/render/modern/modern_renderer.h": "f86f93a6ba05df4b1273b20c1b1c57db69017aa961e53f06d2663c251a776b99",
    "src/Billiards/render/modern/modern_renderer.cpp": "8ab8ba8b10a473924d5294e69b8bb008d670391fd60c6294d8cfd5549dfdb1e3",
    "tests/phase4_v19_trajectory_inset_visible_integration_cpu_tests.cpp": "cc00e8b5f5d8fab6e0cc955378aa8e0aa04d735fb7ec623bda3a502e4a830b74",
}

ZERO_REFERENCE_SOURCES = [
    "src/Billiards/training/trajectory_inset_model.cpp",
    "src/Billiards/training/trajectory_inset_renderer.cpp",
    "src/Billiards/render/modern/trajectory_inset_atomic_transaction.cpp",
    "src/Billiards/render/modern/trajectory_inset_visible_integration.cpp",
]

TARGET_CLOSURE_REQUIREMENTS = [
    "Each of the four zero-reference V19 implementation sources has exactly one owning library or executable source list; duplicate compilation into multiple final-link paths is forbidden.",
    "The owner exposes all required CP1, CP3 and CP4 symbols to ModernRenderer without relying on unresolved static archive members.",
    "A dedicated CPU test target compiles and runs tests/phase4_v19_trajectory_inset_visible_integration_cpu_tests.cpp with the actual Core41 link shim.",
    "A final-link closure target directly instantiates and calls the ModernRenderer CP4 visible path so modern_renderer.o and all four V19 implementation objects are pulled; an archive-only build or unrelated smoke target is insufficient.",
    "The stale ModernGpuResources production-link comment is corrected or removed without changing runtime behavior.",
    "The exact target names, source ownership, dependency graph, binary paths and expected symbols are frozen before any CMake edit or build.",
]

MINIMUM_LIBRARY_TARGETS = [
    "BilliardsProductUiGpuCore",
    "BilliardsProductUiOpenGL41",
    "BilliardsModernProductUiRuntime",
    "BilliardsModernGpuResources",
    "FUTURE_EXACT_V19_IMPLEMENTATION_OWNER_TARGET_NOT_YET_DEFINED",
]

MINIMUM_CPU_TARGETS = [
    "BilliardsProductUiGpuRendererTests",
    "BilliardsModernProductUiRuntimeTests",
    "BilliardsModernRendererProductUiTests",
    "BilliardsProductShellTests",
    "BilliardsModernRenderFoundationTests",
    "FUTURE_EXACT_V19_CPU_TRANSACTION_TEST_TARGET_NOT_YET_DEFINED",
    "FUTURE_EXACT_MODERN_RENDERER_CP4_FINAL_LINK_TARGET_NOT_YET_DEFINED",
]

GPU_CASES = [
    "player_standard_control",
    "elevated_standard_control",
    "overhead_standard_control",
    "broadcast_standard_inset",
    "broadcast_cvd_inset",
]


def sha(path: Path) -> str:
    return hashlib.sha256(path.read_bytes()).hexdigest()


def read(path: Path) -> dict:
    return json.loads(path.read_text(encoding="utf-8"))


def validate(addendum_path: Path = ADDENDUM, checklist_path: Path = CHECKLIST) -> list[str]:
    errors: list[str] = []
    try:
        addendum, checklist = read(addendum_path), read(checklist_path)
    except Exception as error:
        return [f"json_invalid:{error}"]

    def require(value: bool, message: str) -> None:
        if not value:
            errors.append(message)

    require(addendum.get("schema") == ADDENDUM_SCHEMA and addendum.get("schema_version") == 1, "addendum_schema")
    require(addendum.get("status") == "CP4_D_INDEPENDENT_TA_ACCEPTED_CPU_STATIC_ONLY_CP1_CP2_CP3_CP4_TRUE_GPU_PRODUCT_FALSE", "addendum_status")
    rows = addendum.get("frozen_bindings", [])
    bindings = {row.get("path"): row.get("sha256") for row in rows}
    require(len(rows) == len(bindings) and bindings == ADDENDUM_BINDINGS, "addendum_bindings")
    for relative, digest in bindings.items():
        target = ROOT / relative
        require(target.is_file() and sha(target) == digest, "addendum_binding:" + relative)
    reproduction = addendum.get("independent_reproduction", {})
    require(reproduction.get("post_freeze_receipt_validator") == "PASS_ERRORS_EMPTY_FRESH_O2_ACTUAL_CORE41_CHAIN", "addendum_validator")
    require(reproduction.get("post_freeze_mutation_unittest") == "PASS_CANONICAL_AND_MINIMUM_70_MUTATIONS", "addendum_mutations")
    require(reproduction.get("source_and_self_bindings_unique_exact") == "PASS_28_OF_28", "addendum_source_bindings")
    require(reproduction.get("gpu_or_window_context_created") is False and reproduction.get("billiards_executed") is False and reproduction.get("readiness_or_capture_created") is False, "addendum_execution_boundary")
    axes = addendum.get("accepted_cp4_d_axes", {})
    require(len(axes) == 13 and all(value is True or value == 96 for value in axes.values()), "addendum_axes")
    boundary = addendum.get("acceptance_boundary", {})
    for key in ("cp1_cpu_static_accepted", "cp2_cpu_static_accepted", "cp3_cpu_static_accepted", "cp4_cpu_static_accepted", "combined_cp1_through_cp4_cpu_static_accepted"):
        require(boundary.get(key) is True, "addendum_cpu_boundary:" + key)
    for key, value in boundary.items():
        if key not in {"cp1_cpu_static_accepted", "cp2_cpu_static_accepted", "cp3_cpu_static_accepted", "cp4_cpu_static_accepted", "combined_cp1_through_cp4_cpu_static_accepted"}:
            require(value is False, "addendum_false_claim:" + key)

    require(checklist.get("schema") == CHECKLIST_SCHEMA and checklist.get("schema_version") == 1, "checklist_schema")
    require(checklist.get("status") == "DESIGN_ONLY_FRESH_RELEASE_CPU_CLOSURE_AND_FUTURE_ONE_SHOT_GPU_PREREQUISITES_NOT_RUN_NOT_AUTHORIZED", "checklist_status")
    frozen = {row.get("role"): (row.get("path"), row.get("sha256")) for row in checklist.get("frozen_bindings", [])}
    expected_frozen = {
        "rules_audit_v10": ("docs/rules/phase5-wpa-heyball-as-built-reconciliation-audit-v10.json", "8d2e45aa18f7084b6252592e3ca7a57f5f97747ea6ec978d0d701b65cf60a5ab"),
        "cp4_d_receipt": ("docs/evidence/phase4-v19-cp4-d-visible-integration-implementation-v1/receipt.json", "fe341d320050e5cba3d67655631e0dfb314b4ccb39d880300ef3278a49dbc9c0"),
        "cp4_d_independent_acceptance": ("docs/art/phase4-v19-cp4-d-visible-integration-independent-ta-acceptance-addendum-v1.json", sha(ADDENDUM)),
        "cmake_external_accepted_d5_d_lineage": ("CMakeLists.txt", "871c7916e2d1033073ca880c1c5e61263dff5f97bb28405fe9fd9a95c40d1dc2"),
    }
    require(frozen == expected_frozen, "checklist_frozen_bindings")
    for relative, digest in (value for value in frozen.values()):
        target = ROOT / relative
        require(target.is_file() and sha(target) == digest, "checklist_binding:" + relative)
    require(checklist.get("current_source_bindings") == SOURCES, "checklist_sources")
    for relative, digest in SOURCES.items():
        require((ROOT / relative).is_file() and sha(ROOT / relative) == digest, "checklist_source:" + relative)
    audit = checklist.get("rules_audit_boundary", {})
    require(audit.get("audit_v10_summary") == {"total": 40, "implemented": 19, "partial": 7, "missing": 7, "procedural_only": 7}, "audit_summary")
    require(audit.get("profile_complete") is False and audit.get("product_exposed") is False and audit.get("audit_does_not_authorize_release_or_gpu") is True and audit.get("rules_authority_or_threshold_change_allowed") is False, "audit_boundary")
    cpu = checklist.get("future_fresh_release_cpu_build_closure_not_run", {})
    gap = cpu.get("current_cmake_gap", {})
    require(gap.get("status") == "BLOCKS_FRESH_RELEASE_CLOSURE_UNTIL_SEPARATE_TARGET_GATE_AND_IMPLEMENTATION" and gap.get("zero_reference_sources_exact") == ZERO_REFERENCE_SOURCES, "cmake_gap")
    require(gap.get("static_archive_success_is_not_link_closure") is True and gap.get("modern_gpu_resources_comment_not_linked_to_production_is_stale_on_apple") is True and gap.get("existing_targets_do_not_prove_modern_renderer_cp4_object_is_pulled") is True, "cmake_gap_semantics")
    target_gate = cpu.get("required_separate_cmake_target_closure_gate_before_configure", {})
    require(target_gate.get("gate_frozen") is False and target_gate.get("cmake_edit_authorized") is False and target_gate.get("requirements") == TARGET_CLOSURE_REQUIREMENTS, "target_gate")
    require(cpu.get("build_type") == "Release" and cpu.get("minimum_library_targets_after_target_closure") == MINIMUM_LIBRARY_TARGETS and cpu.get("minimum_cpu_or_compile_targets_after_target_closure") == MINIMUM_CPU_TARGETS, "cpu_target_closure")
    require(len(cpu.get("required_standalone_cp4_validation", [])) == 2 and cpu.get("no_stale_binary_or_prior_build_reuse") is True, "cpu_validation")
    for key in ("cmake_or_source_edit_authorized", "configure_executed", "build_executed", "tests_executed", "cpu_closure_pass"):
        require(cpu.get(key) is False, "cpu_false_claim:" + key)
    producer = checklist.get("capture_producer_and_readiness_preconditions", {})
    require(producer.get("existing_old_phase4_world_overlay_runner_is_not_accepted_as_v19_inset_producer_by_name_only") is True and len(producer.get("required_before_readiness", [])) == 6, "producer_prerequisites")
    require("historical only" in producer.get("historical_evidence_not_inheritable", "") and "None proves a V19 inset producer" in producer.get("historical_evidence_not_inheritable", ""), "historical_evidence_boundary")
    require(producer.get("capture_producer_contract_frozen") is False and producer.get("readiness_created") is False and producer.get("single_use_gpu_authorization_created") is False, "producer_false_claim")
    gpu = checklist.get("future_one_shot_gpu_minimum_plan_not_authorized", {})
    require(gpu.get("case_count") == 5 and gpu.get("cases_exact") == GPU_CASES and len(gpu.get("required_machine_axes", [])) == 7 and gpu.get("machine_5_of_5_required_before_ta") is True, "gpu_plan")
    require(gpu.get("gpu_authorized") is False and gpu.get("gpu_executed") is False and gpu.get("machine_pass_claimed") is False, "gpu_false_claim")
    ta = checklist.get("future_ta_and_product_boundary", {})
    require(len(ta.get("ta_axes", [])) == 8 and ta.get("ta_requires_original_resolution_images_and_per_image_sha_dimensions") is True, "ta_plan")
    require(ta.get("ta_visual_pass_claimed") is False and ta.get("human_pass_claimed") is False and ta.get("product_exposed") is False and ta.get("m3_golden_beta_pass_claimed") is False, "ta_false_claim")
    auth = checklist.get("authorization_boundary", {})
    require(auth.get("design_checklist_complete") is True and auth.get("cp1_cp2_cp3_cp4_cpu_static_accepted") is True, "checklist_design_boundary")
    require(all(value is False for key, value in auth.items() if key not in {"design_checklist_complete", "cp1_cp2_cp3_cp4_cpu_static_accepted"}), "checklist_false_authority")
    return sorted(set(errors))


if __name__ == "__main__":
    result = validate(*(Path(arg).resolve() for arg in sys.argv[1:3])) if len(sys.argv) == 3 else validate()
    print(json.dumps({"valid": not result, "errors": result}, sort_keys=True))
    raise SystemExit(1 if result else 0)
