#!/usr/bin/env python3
"""Validate the Phase4 V19 CP4-D Product UI chrome correction gate."""

from __future__ import annotations

import hashlib
import json
from pathlib import Path
import sys


ROOT = Path(__file__).resolve().parents[1]
DEFAULT = ROOT / "docs/art/phase4-v19-cp4-d-product-ui-chrome-submission-correction-design-gate-v1.json"
SCHEMA = "billiardgl.phase4.v19.cp4_d_product_ui_chrome_submission_correction_design_gate.v1"
STATUS = "ROOT_SELECTED_PRODUCT_UI_CURRENT_LDR_CHROME_SUBMISSION_DESIGN_CORRECTION_ONLY_NO_PRODUCTION_NO_CMAKE_NO_GPU_NO_AUTH"
BINDINGS = {
    "docs/art/phase4-v19-cp4-visible-integration-implementation-acceptance-checklist-v1.json": "f43bf8dba7b99673fb9a2db4a4cc9ddb686816c92de60ddc203721de80d5bc69",
    "docs/art/phase4-v19-cp1-trajectory-inset-foundation-independent-ta-acceptance-addendum-v1.json": "e89e7febf1266efbea751ff0d50ac1d4a6bf358317974f701202d02a5396d02e",
    "docs/art/phase4-v19-cp2-layout-draw-list-independent-ta-acceptance-addendum-v1.json": "6e7724df1a977af914b427538728f476d865db54c6cf95a3ae112e9de6172dd6",
    "docs/evidence/phase4-v19-cp2-layout-draw-list-implementation-v1/receipt.json": "2524223aa320ff5aecb70a164bfeac471d9fc36f09a67b30a1bb7486333bbbc5",
    "docs/art/phase4-v19-cp3-offscreen-atomic-independent-ta-acceptance-addendum-v1.json": "46dd31775ce501969a8079171d8ff106c8de4bf9698b04ba0c09cfed21f6007d",
    "docs/evidence/phase4-v19-cp3-offscreen-atomic-implementation-v1/receipt.json": "9848e3f869db91f547e6c2cce1e4f6067538702a6dd04cbb18a585c57007133a",
    "docs/evidence/phase4-v19-cp4-checkpoint-d-content-op-receipt-red-v8/baselines.txt": "a0cf724c8dd0fbd53a7d7757c6eedb9fe685314cf52a20342955fe75235aafa3",
    "docs/evidence/phase4-v19-cp4-checkpoint-d-content-op-receipt-red-v8/command.txt": "773c6ad07b6c9887b043b6cc2226c1772448c2cf7333a814e0f04a46331275e1",
    "docs/evidence/phase4-v19-cp4-checkpoint-d-content-op-receipt-red-v8/stderr.txt": "f9683410dcbb57bca350d209a498a104483512bfceb03be850b2d37de786bea5",
    "docs/evidence/phase4-v19-cp4-checkpoint-d-content-op-receipt-red-v8/exit-code.txt": "4355a46b19d348dc2f57c046f8ef63d4538ebb936000f3c9ee954a27460dd865",
}
BLOCKER_SHA = "57aebbd960fad492e068f1daff7c56c7614aa242e7d4beb7f57f35ce009dc930"
DECISION_SHA = "4fcb3a459dd5efd2aef911396f14150e75fd58f5d5cf703c8432dd241bac84d1"
ALLOWLIST = [
    "src/Billiards/render/modern/modern_product_ui_runtime.h",
    "src/Billiards/render/modern/modern_product_ui_runtime.cpp",
    "src/Billiards/render/modern/modern_renderer_product_ui.h",
    "src/Billiards/render/modern/modern_renderer_product_ui.cpp",
    "src/Billiards/render/ui/product_ui_gpu_renderer.h",
    "src/Billiards/render/ui/product_ui_gpu_renderer.cpp",
    "src/Billiards/render/modern/trajectory_inset_visible_integration.h",
    "src/Billiards/render/modern/trajectory_inset_visible_integration.cpp",
    "src/Billiards/render/modern/modern_renderer.h",
    "src/Billiards/render/modern/modern_renderer.cpp",
    "tests/phase4_v19_trajectory_inset_visible_integration_cpu_tests.cpp",
]
FROZEN = [
    "src/Billiards/training/trajectory_inset_model.h",
    "src/Billiards/training/trajectory_inset_model.cpp",
    "src/Billiards/training/trajectory_inset_renderer.h",
    "src/Billiards/training/trajectory_inset_renderer.cpp",
    "src/Billiards/render/ui/product_shell_layout.h",
    "src/Billiards/render/ui/product_shell_layout.cpp",
    "src/Billiards/render/ui/product_shell_draw_list.h",
    "src/Billiards/render/ui/product_shell_draw_list.cpp",
    "src/Billiards/render/modern/trajectory_inset_atomic_transaction.h",
    "src/Billiards/render/modern/trajectory_inset_atomic_transaction.cpp",
    "CMakeLists.txt",
]
TEXT = [
    "TRAJECTORY / 轨迹",
    "HEAD / 开球端",
    "FOOT / 底端",
    "ACTUAL / 实际 — paired continuous rails",
    "PREDICTED / 预测 — centerline dashes",
    "END / 终点 — endpoint marker",
    "Trajectory inset / 轨迹小窗",
]
RECEIPT_FIELDS = [
    "sessionSerial", "frameSerial", "layoutSerial", "cp1GenerationId",
    "cp2LayoutDigest", "cp2ExclusionDigest", "chromeSemanticDigest",
    "atlasCacheKey", "targetCurrentLdrFramebuffer",
    "targetCurrentLdrColorTexture", "physicalWidth", "physicalHeight",
    "palette", "semanticPrimitiveCount", "orderedPrimitiveDigest",
    "submittedSolidVertexCount", "submittedSdfVertexCount",
    "submittedBatchCount", "submittedDrawCallCount", "orderedBatchDigest",
    "cutoutZeroCoverageProofDigest", "glError", "stateRestored",
]


def sha(path: Path) -> str:
    return hashlib.sha256(path.read_bytes()).hexdigest()


def validate(path: Path = DEFAULT) -> list[str]:
    errors: list[str] = []
    try:
        value = json.loads(path.read_text(encoding="utf-8"))
    except Exception as error:
        return [f"gate_invalid:{error}"]

    def require(condition: bool, message: str) -> None:
        if not condition:
            errors.append(message)

    require(value.get("schema") == SCHEMA and value.get("schema_version") == 1 and value.get("status") == STATUS, "schema_or_status_invalid")
    bindings = {item.get("path"): item.get("sha256") for item in value.get("frozen_bindings", [])}
    require(bindings == BINDINGS, "binding_inventory_or_sha_invalid")
    for relative, digest in BINDINGS.items():
        target = ROOT / relative
        require(target.is_file() and sha(target) == digest, f"binding_missing_or_drift:{relative}")

    blocker = value.get("nonfinal_blocker_binding", {})
    require(hashlib.sha256(blocker.get("statement", "").encode()).hexdigest() == blocker.get("statement_sha256") == BLOCKER_SHA, "blocker_statement_invalid")
    require(blocker.get("candidate_is_frozen_or_accepted") is False and blocker.get("coordinate_mapping_fix_is_not_chrome_submission_proof") is True, "blocker_authority_invalid")
    require(set(blocker.get("candidate_source_sha256", {})) == {
        "src/Billiards/render/modern/trajectory_inset_visible_integration.h",
        "src/Billiards/render/modern/trajectory_inset_visible_integration.cpp",
        "src/Billiards/render/modern/modern_renderer.h",
        "src/Billiards/render/modern/modern_renderer.cpp",
        "tests/phase4_v19_trajectory_inset_visible_integration_cpu_tests.cpp",
    }, "candidate_source_binding_invalid")
    decision = value.get("root_selected_correction_binding", {})
    require(hashlib.sha256(decision.get("statement", "").encode()).hexdigest() == decision.get("statement_sha256") == DECISION_SHA, "root_decision_statement_invalid")
    require(decision.get("option") == "A_PRODUCT_UI_CURRENT_LDR_CHROME_SUBMISSION" and decision.get("selected") is True and decision.get("final_staging_duplicate_ui_option_selected") is False, "root_option_invalid")

    supersession = value.get("supersession_contract", {})
    require("productChromeAlreadyInCurrentLdr" in supersession.get("superseded_nonfinal_assumption", ""), "superseded_assumption_invalid")
    require("actual converted vertex/batch streams" in supersession.get("replacement", "").lower(), "replacement_receipt_invalid")
    require(len(supersession.get("corrected_order", [])) == 6 and "current product LDR" in supersession.get("corrected_order", [""])[1], "corrected_order_invalid")
    require(supersession.get("cp3_plot_publish_before_chrome_wording_in_original_checklist_is_replaced_for_option_a") is True and supersession.get("final_staging_may_draw_tone_or_submit_ordinary_ui_again") is False, "supersession_boundary_invalid")
    require(supersession.get("all_cp1_geometry_semantics_cp2_layout_chrome_geometry_cp3_resource_and_other_cp4_identity_contracts_unchanged") is True and supersession.get("original_cp4_checklist_may_be_used_without_this_correction") is False, "supersession_scope_invalid")
    require(value.get("future_candidate_exact_allowlist_not_authorized") == ALLOWLIST, "allowlist_invalid")
    require(value.get("frozen_files_forbidden_to_edit") == FROZEN, "frozen_file_inventory_invalid")

    submission = value.get("product_ui_submission_contract", {})
    require("exact current" in submission.get("input", "") and "no regenerated" in submission.get("input", ""), "current_input_invalid")
    require("Absent" in submission.get("runtime_seam", "") and "zero supplemental" in submission.get("runtime_seam", ""), "default_absent_invalid")
    require("existing Product UI SDF/solid renderer" in submission.get("gpu_conversion", "") and "No new final-staging UI shader" in submission.get("gpu_conversion", ""), "product_ui_path_invalid")
    require("every required English, Chinese" in submission.get("bilingual_atlas", "") and "clipped-to-empty text fails closed" in submission.get("bilingual_atlas", ""), "bilingual_atlas_invalid")
    require(len(submission.get("current_identity_fields", [])) == 14 and "chromeSemanticDigest" in submission.get("current_identity_fields", []) and "atlasCacheKey" in submission.get("current_identity_fields", []), "current_identity_invalid")
    require("consumed exactly once" in submission.get("cache_policy", "") and "never included" in submission.get("cache_policy", ""), "cache_policy_invalid")

    inventory = value.get("exact_current_chrome_semantic_inventory", {})
    require(inventory.get("ordered_text_exact") == TEXT, "ordered_text_invalid")
    require(inventory.get("unfocused_semantic_primitive_count") == 16 and inventory.get("focused_semantic_primitive_count") == 17, "primitive_count_invalid")
    require("four nonempty" in inventory.get("panel", "") and "opacity 0.94" in inventory.get("panel", ""), "panel_inventory_invalid")
    require("_0 through _3" in inventory.get("borders", "") and "_0 through _6" in inventory.get("text", ""), "border_or_text_inventory_invalid")
    require("iff" in inventory.get("focus", "") and inventory.get("all_primitives_excluded_from_trajectory_denominator") is True and inventory.get("all_semantic_primitive_rectangles_have_zero_area_overlap_with_current_cutout") is True, "focus_or_cutout_invalid")
    require(inventory.get("semantic_member_probe_and_denominator_contribution") == [0, 0, 0] and inventory.get("foreign_missing_duplicate_reordered_relocated_resized_rekinded_retokened_retexted_reopacity_or_focus_mismatch_fails_closed") is True, "denominator_or_mutation_invalid")

    receipt = value.get("resource_bound_submission_receipt_contract", {})
    require(receipt.get("required_receipt_fields_in_order") == RECEIPT_FIELDS, "receipt_field_order_invalid")
    require("actual target resource keys" in receipt.get("actual_not_echo", "") and "Echoing request fields" in receipt.get("actual_not_echo", ""), "receipt_echo_invalid")
    require("one-to-many" in receipt.get("mapping", "") and "union equals" in receipt.get("mapping", ""), "receipt_mapping_invalid")
    require("only after" in receipt.get("success_condition", "") and "GL error zero" in receipt.get("success_condition", ""), "receipt_success_invalid")
    require("Instrumentation OFF and ON" in receipt.get("instrumentation_parity", "") and "writes no trajectory semantic token" in receipt.get("instrumentation_parity", ""), "instrumentation_invalid")

    cutout = value.get("cutout_and_final_staging_contract", {})
    require("same current product LDR" in cutout.get("product_ui_target", ""), "target_invalid")
    require("SDF fringe" in cutout.get("cutout_transparency", "") and "geometry-rectangle-only proof is insufficient" in cutout.get("cutout_transparency", ""), "physical_cutout_invalid")
    require("one full-size sample-1 color copy" in cutout.get("final_copy", "") and "does not assert or redraw" in cutout.get("final_copy", ""), "final_copy_invalid")
    require("zero physical sample intersection" in cutout.get("plot_composite", "") and cutout.get("main_world_trajectory_tokens") == 0 and cutout.get("world_overlay_draw_calls") == 0, "plot_or_world_invalid")
    require("zero tone application" in cutout.get("tone_contract", ""), "tone_invalid")

    failure = value.get("atomic_failure_and_noncurrent_contract", {})
    require("forbids terminal publication and product_present" in failure.get("draw_failure", "") and "no failed inset or partial chrome reaches" in failure.get("draw_failure", ""), "draw_failure_invalid")
    require("poisons the adapter" in failure.get("restore_failure", "") and "new dispatch/resource namespace" in failure.get("restore_failure", ""), "restore_poison_invalid")
    require("Chrome from N" in failure.get("noncurrent_chrome", "") and "silently rebuild" in failure.get("noncurrent_chrome", ""), "noncurrent_invalid")
    require("N+2 requires fresh" in failure.get("valid_fail_valid", "") and "ordinary LDR framebuffer" in failure.get("no_fallback", ""), "valid_fail_valid_or_fallback_invalid")
    require(failure.get("resize_context_loss_screen_modal_toggle_focus_palette_or_atlas_change_invalidates") is True, "invalidation_invalid")

    future = value.get("future_cpu_static_gl_mock_acceptance_not_run", {})
    matrix = future.get("positive_matrix", {})
    require(matrix.get("exact_case_count") == 96 and len(matrix.get("layout_profiles", [])) == 4 and matrix.get("sample_counts") == [1, 2, 4] and matrix.get("focus_states") == ["unfocused", "focused"], "matrix_invalid")
    require(len(future.get("required_static_checks", [])) == 5 and len(future.get("required_mock_checks", [])) == 6 and len(future.get("required_fail_closed_fixtures", [])) == 12, "future_fixture_inventory_invalid")
    require(future.get("tests_first_red_required") is True and future.get("cpu_static_pass") is False and future.get("gl_mock_pass") is False and future.get("implementation_pass") is False, "future_false_pass")

    auth = value.get("authorization_boundary", {})
    require(auth.get("root_option_a_selected") is True and auth.get("chrome_correction_design_gate_complete") is True, "design_selection_invalid")
    require(all(flag is False for key, flag in auth.items() if key not in {"root_option_a_selected", "chrome_correction_design_gate_complete"}), "false_authorization")
    proof = value.get("proof_boundary", {})
    require(proof.get("design_correction_only") is True and proof.get("nonfinal_d_candidate_accepted") is False, "proof_design_boundary_invalid")
    require(all(flag is False for key, flag in proof.items() if key not in {"design_correction_only", "nonfinal_d_candidate_accepted"}), "proof_false_claim")
    return errors


def main() -> int:
    target = Path(sys.argv[1]).resolve() if len(sys.argv) > 1 else DEFAULT
    errors = validate(target)
    if errors:
        for error in errors:
            print("FAIL: " + error, file=sys.stderr)
        return 1
    print("PASS_PHASE4_V19_CP4_D_PRODUCT_UI_CHROME_SUBMISSION_CORRECTION_GATE")
    print("option=A current_ldr_chrome=true final_ui_redraw=false cases=96 production=false gpu=false")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
