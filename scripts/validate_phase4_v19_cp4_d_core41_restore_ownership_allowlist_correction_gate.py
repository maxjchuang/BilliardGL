#!/usr/bin/env python3
"""Fail-closed validator for the Phase4 V19 CP4-D Core41 restore gate."""

from __future__ import annotations

import hashlib
import json
from pathlib import Path
import sys

ROOT = Path(__file__).resolve().parents[1]
DEFAULT = ROOT / "docs/art/phase4-v19-cp4-d-core41-restore-ownership-allowlist-correction-gate-v1.json"
SCHEMA = "billiardgl.phase4.v19.cp4_d_core41_restore_ownership_allowlist_correction_gate.v1"
STATUS = "ROOT_SELECTED_SINGLE_OUTER_RESTORE_OWNER_DESIGN_ONLY_NO_PRODUCTION_NO_CMAKE_NO_GPU_NO_AUTH"
PRIOR = "docs/art/phase4-v19-cp4-d-product-ui-chrome-submission-correction-design-gate-v1.json"
PRIOR_SHA = "dab3bd181ecff6f631249fc629b377b6e3e3a327bdc61e72358dde7467be54f7"
DELTA = [
    "src/Billiards/render/modern/product_ui_gl_core41.h",
    "src/Billiards/render/modern/product_ui_gl_core41.cpp",
]
ORIGINAL = [
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
SOURCE_BINDINGS = {
    "src/Billiards/render/modern/product_ui_gl_core41.h": "6d81f709b48c00108233765c6835163edff381a92f1c5c1b4dff7dc46d79b8fc",
    "src/Billiards/render/modern/product_ui_gl_core41.cpp": "2b54ae4b765d5faaccbaff4438e086d1818b3f26b5b6dfa5fd058b8d28197fd4",
    "src/Billiards/render/ui/product_ui_gpu_renderer.h": "7efed99e830c9ddf5b33a6dd58fbb64a576ac36d02aa5bfff69291a95387ef12",
    "src/Billiards/render/ui/product_ui_gpu_renderer.cpp": "50543bce8356d6cac8000b1f9d84bf4f6d1ea798055dadaa3e7ea9ac4c4aedeb",
    "src/Billiards/render/modern/modern_renderer.cpp": "8ab8ba8b10a473924d5294e69b8bb008d670391fd60c6294d8cfd5549dfdb1e3",
    "tests/phase4_v19_trajectory_inset_visible_integration_cpu_tests.cpp": "1a598373423f64876a2ab84c055f4f97a633f9985e5ad531120548431841da09",
}


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
    require(value.get("frozen_bindings") == [{"path": PRIOR, "sha256": PRIOR_SHA}], "prior_binding_invalid")
    require((ROOT / PRIOR).is_file() and sha(ROOT / PRIOR) == PRIOR_SHA, "prior_gate_missing_or_drift")
    failure = value.get("nonfinal_failure_binding", {})
    require(failure.get("classification") == "DOUBLE_RESTORE_AND_UNOBSERVED_INTERNAL_RESTORE_RESULT_FAIL_OPEN", "failure_classification_invalid")
    require(failure.get("source_sha256") == SOURCE_BINDINGS, "failure_source_bindings_invalid")
    require("calls endFrame internally" in failure.get("control_flow_finding", "") and "two restore attempts" in failure.get("control_flow_finding", ""), "control_flow_finding_invalid")
    require(failure.get("candidate_accepted") is False and failure.get("compile_only_is_not_acceptance") is True, "nonfinal_authority_invalid")
    decision = value.get("root_decision_binding", {})
    require(decision.get("selected_owner") == "PRODUCT_UI_GPU_RENDERER_OUTER_TRANSACTION", "owner_selection_invalid")
    require(decision.get("core41_internal_silent_restore_allowed") is False and decision.get("outer_exact_restore_attempts_after_captured_begin_failure") == 1, "root_restore_decision_invalid")
    require("仅新增product_ui_gl_core41" in decision.get("statement", "") and "production-style tests必须忠实Core41" in decision.get("statement", ""), "root_statement_invalid")
    require(value.get("corrected_exact_allowlist_not_implementation_authority") == ORIGINAL + DELTA, "corrected_allowlist_invalid")
    require(value.get("allowlist_delta_exact") == DELTA, "allowlist_delta_invalid")
    forbidden = value.get("still_forbidden", {})
    require(forbidden.get("cmake") == "CMakeLists.txt" and forbidden.get("authority_rules_threshold_camera_layout_copy_palette_product_exposure") is True, "forbidden_scope_invalid")
    typed = value.get("typed_begin_receipt_contract", {})
    require(typed.get("required_fields") == ["setupOk", "stateCaptured", "restoreRequired", "errorCode"], "typed_receipt_fields_invalid")
    require("restoreRequired=false" in typed.get("invalid_precondition_before_capture", "") and "restoreRequired=true" in typed.get("setup_failure_after_capture", ""), "typed_receipt_state_invalid")
    require(typed.get("no_boolean_or_configured_claim_may_substitute_for_actual_receipt") is True and typed.get("no_core41_end_frame_call_on_begin_failure") is True, "typed_receipt_fail_open")
    owner = value.get("single_restore_owner_contract", {})
    require(owner.get("owner") == "ProductUiGpuRenderer outer frame transaction", "outer_owner_invalid")
    require("exactly once" in owner.get("restore_attempt_rule", "") and "pre-capture" in owner.get("restore_attempt_rule", ""), "restore_attempt_rule_invalid")
    require("never silently restores" in owner.get("core41_rule", "") and "actual exact-one endFrame result" in owner.get("result_rule", ""), "core_or_result_rule_invalid")
    require("poisons" in owner.get("restore_failure", "") and "zero target queries" in owner.get("poison_retry", "") and "fresh dispatch/resource namespace" in owner.get("recovery", ""), "poison_or_recovery_invalid")
    future = value.get("future_cpu_static_gl_mock_acceptance_not_run", {})
    require(future.get("actual_core41_required") is True and "actual product_ui_gl_core41.cpp" in future.get("production_style_fixture", "") and "handwritten dispatcher" in future.get("production_style_fixture", ""), "actual_core41_fixture_invalid")
    require(len(future.get("required_positive_and_negative_cases", [])) == 9 and len(future.get("required_static_checks", [])) == 4 and len(future.get("tests_must_reject", [])) == 8, "fixture_inventory_invalid")
    require(all(future.get(key) is False for key in ("cpu_static_pass", "actual_core41_mock_pass", "implementation_pass")), "false_future_pass")
    auth = value.get("authorization_boundary", {})
    require(auth.get("design_gate_complete") is True and auth.get("allowlist_correction_defined") is True, "design_boundary_invalid")
    require(all(flag is False for key, flag in auth.items() if key not in {"design_gate_complete", "allowlist_correction_defined"}), "false_authorization")
    return errors


def main() -> int:
    target = Path(sys.argv[1]).resolve() if len(sys.argv) > 1 else DEFAULT
    errors = validate(target)
    if errors:
        for error in errors:
            print("FAIL: " + error, file=sys.stderr)
        return 1
    print("PASS_PHASE4_V19_CP4_D_CORE41_RESTORE_OWNERSHIP_ALLOWLIST_CORRECTION_GATE")
    print("owner=outer exact_restore=1 core41_delta=2 production=false gpu=false")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
