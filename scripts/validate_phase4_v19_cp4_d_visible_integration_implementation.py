#!/usr/bin/env python3
"""Validate the Phase4 V19 CP4-D visible-integration CPU/static candidate."""

from __future__ import annotations

import hashlib
import json
import pathlib
import subprocess
import sys
import tempfile

ROOT = pathlib.Path(__file__).resolve().parents[1]
EVIDENCE = ROOT / "docs/evidence/phase4-v19-cp4-d-visible-integration-implementation-v1"
RECEIPT = EVIDENCE / "receipt.json"
O0_STDOUT = EVIDENCE / "cpu-test-o0.stdout"
O2_STDOUT = EVIDENCE / "cpu-test-o2.stdout"


def sha(path: pathlib.Path) -> str:
    return hashlib.sha256(path.read_bytes()).hexdigest()


PINNED = {
    "cp4_checklist": ("docs/art/phase4-v19-cp4-visible-integration-implementation-acceptance-checklist-v1.json", "f43bf8dba7b99673fb9a2db4a4cc9ddb686816c92de60ddc203721de80d5bc69"),
    "chrome_correction_gate": ("docs/art/phase4-v19-cp4-d-product-ui-chrome-submission-correction-design-gate-v1.json", "dab3bd181ecff6f631249fc629b377b6e3e3a327bdc61e72358dde7467be54f7"),
    "core41_correction_gate": ("docs/art/phase4-v19-cp4-d-core41-restore-ownership-allowlist-correction-gate-v1.json", "9dc0516153fe16439699c80a47436971435e8f72c9ea7ac88a6ef9df29fe0c6c"),
    "cp1_receipt": ("docs/evidence/phase4-v19-cp1-trajectory-inset-foundation-implementation-v1/receipt.json", "3208c819bd81ddc2143921a3b3ef2136127b275317a6ae0f5549de18188de758"),
    "cp2_receipt": ("docs/evidence/phase4-v19-cp2-layout-draw-list-implementation-v1/receipt.json", "2524223aa320ff5aecb70a164bfeac471d9fc36f09a67b30a1bb7486333bbbc5"),
    "cp3_receipt": ("docs/evidence/phase4-v19-cp3-offscreen-atomic-implementation-v1/receipt.json", "9848e3f869db91f547e6c2cce1e4f6067538702a6dd04cbb18a585c57007133a"),
    "runtime_h": ("src/Billiards/render/modern/modern_product_ui_runtime.h", "8d8497e5c1c2edd0f7a3bfe19c234a777bb27fd4dcf5a09b019c5d3ce1f230b7"),
    "runtime_cpp": ("src/Billiards/render/modern/modern_product_ui_runtime.cpp", "601c508facf5d83c09fd3fc603970e9caa7e22e5dbde10a8eae049b7081206b3"),
    "product_ui_pass_h": ("src/Billiards/render/modern/modern_renderer_product_ui.h", "8b7b0cdb1b238502fdd970c88e1dfc99e833a39f60e1ae58fd7e7cdb93d72106"),
    "product_ui_pass_cpp": ("src/Billiards/render/modern/modern_renderer_product_ui.cpp", "8a3e8ad0b280d5a9a951fd9be4941661718fd0b6099ef99ad697dead03d042c0"),
    "gpu_h": ("src/Billiards/render/ui/product_ui_gpu_renderer.h", "45313df247fadb59c1aba0c8350398940de82451e99c3869784494c170d0861d"),
    "gpu_cpp": ("src/Billiards/render/ui/product_ui_gpu_renderer.cpp", "df444cc870658cc2d8d770ec4439bfb51a1f1fb7273cf0a8717537e726c0b540"),
    "integration_h": ("src/Billiards/render/modern/trajectory_inset_visible_integration.h", "823f9d3560cdc930e6012b56446307d26314e6d7ac4786a8c8d2448dc8fcec74"),
    "integration_cpp": ("src/Billiards/render/modern/trajectory_inset_visible_integration.cpp", "0fbd08d194e3626f562fd54476d2be3a53d5f2edb08c5b0505d0b3afdff85580"),
    "modern_h": ("src/Billiards/render/modern/modern_renderer.h", "f86f93a6ba05df4b1273b20c1b1c57db69017aa961e53f06d2663c251a776b99"),
    "modern_cpp": ("src/Billiards/render/modern/modern_renderer.cpp", "8ab8ba8b10a473924d5294e69b8bb008d670391fd60c6294d8cfd5549dfdb1e3"),
    "core41_h": ("src/Billiards/render/modern/product_ui_gl_core41.h", "65465946d36dee6b49687568847a187cf6fbc4b50c9da5fcfaee14d2b982e571"),
    "core41_cpp": ("src/Billiards/render/modern/product_ui_gl_core41.cpp", "10918cd69a19ac52714eec71f3b2e4f277f3dcf151374898840b12add7cd363f"),
    "cpu_test": ("tests/phase4_v19_trajectory_inset_visible_integration_cpu_tests.cpp", "cc00e8b5f5d8fab6e0cc955378aa8e0aa04d735fb7ec623bda3a502e4a830b74"),
    "cmake_external_accepted_d5_d_lineage": ("CMakeLists.txt", "871c7916e2d1033073ca880c1c5e61263dff5f97bb28405fe9fd9a95c40d1dc2"),
    "red_v2_command": ("docs/evidence/phase4-v19-cp4-d-visible-integration-implementation-v1/red-v2-command.txt", "38e909196a3d8c172e5336f526b4c79954dff44084ef9882c288363cb3e93854"),
    "red_v2_stderr": ("docs/evidence/phase4-v19-cp4-d-visible-integration-implementation-v1/red-v2.stderr", "346b2340587fa5f9cd8402b5150385129747c436f6e84ac09b29c83791948e50"),
    "red_v2_exit": ("docs/evidence/phase4-v19-cp4-d-visible-integration-implementation-v1/red-v2-exit.txt", "4355a46b19d348dc2f57c046f8ef63d4538ebb936000f3c9ee954a27460dd865"),
    "o0_stdout": ("docs/evidence/phase4-v19-cp4-d-visible-integration-implementation-v1/cpu-test-o0.stdout", "50dd4caf8349bda693178611348cf8c61e0c3246bbda5c59f723620a59903369"),
    "o2_stdout": ("docs/evidence/phase4-v19-cp4-d-visible-integration-implementation-v1/cpu-test-o2.stdout", "50dd4caf8349bda693178611348cf8c61e0c3246bbda5c59f723620a59903369"),
    "fresh_o2_command": ("docs/evidence/phase4-v19-cp4-d-visible-integration-implementation-v1/fresh-o2-command.txt", "209e6da3349728c265e9594056f02ec76022876737570132b2b09c4ac6d2dbcf"),
}


def dynamic_bindings(doc: dict) -> dict:
    return {
        "validator": ("scripts/validate_phase4_v19_cp4_d_visible_integration_implementation.py", doc.get("validator_sha256")),
        "mutation_tests": ("tests/test_phase4_v19_cp4_d_visible_integration_implementation.py", doc.get("mutation_tests_sha256")),
    }


EXACT_CONTRACT = {
    "red_status": "VERIFIED_ACTUAL_CORE41_RUNTIME_GPU_CHAIN_MISSING_TYPED_BEGIN_RECEIPT",
    "layout_profiles": 4,
    "screens": ["TrainingSession", "ReplayPlayback"],
    "palettes": ["Standard", "ColorVisionSafe"],
    "sample_profiles": [1, 2, 4],
    "focus_states": [False, True],
    "chrome_exact_cases": 96,
    "unfocused_primitive_count": 16,
    "focused_primitive_count": 17,
    "typed_begin_receipt": True,
    "successful_begin_explicitly_captured_and_restore_required": True,
    "pre_capture_rejection_restore_attempts": 0,
    "captured_setup_failure_core41_internal_restore_attempts": 0,
    "captured_setup_failure_outer_restore_attempts": 1,
    "restore_false_and_throw_modes": 2,
    "persistent_poison_same_context_zero_work": True,
    "context_loss_fresh_core41_runtime_recovery": True,
    "fractional_cutout_sample_center_audit": True,
    "sample_digest_preserves_integer_pixel_and_sample_index": True,
    "current_ldr_target_query_real": True,
    "adjacent_gl_error_query_real": True,
    "actual_core41_link_shim_chain_executed": True,
    "o0_pass": True,
    "o0_wall_seconds": 152.60,
    "o2_pass": True,
    "o2_wall_seconds": 44.16,
    "cmake_modified_by_cp4": False,
    "cmake_external_lineage": "EXTERNAL_ACCEPTED_D5_D_LINEAGE",
    "world_overlay_draw_calls": 0,
    "gpu_run": False,
    "billiards_executed": False,
    "readiness_created": False,
    "capture_created": False,
    "authorization_created": False,
    "product_pass_claimed": False,
}


def validate_document(doc: dict) -> list[str]:
    errors: list[str] = []
    if doc.get("schema") != "billiardgl.phase4.v19.cp4_d_visible_integration_implementation_receipt.v1": errors.append("schema")
    if doc.get("schema_version") != 1: errors.append("schema_version")
    if doc.get("status") != "CP4_D_CPU_STATIC_SOURCE_CANDIDATE_NO_GPU_NO_BILLIARDS_NO_READINESS_NO_CAPTURE_NO_AUTH_NO_PRODUCT_PASS": errors.append("status")
    rows = doc.get("bindings", [])
    bindings = {row.get("role"): (row.get("path"), row.get("sha256")) for row in rows}
    expected = PINNED | dynamic_bindings(doc)
    if len(rows) != len(bindings) or bindings != expected: errors.append("bindings")
    for role, (relative, digest) in bindings.items():
        target = ROOT / relative
        if not target.is_file() or not isinstance(digest, str) or sha(target) != digest:
            errors.append("binding:" + role)
    if doc.get("verified_contract") != EXACT_CONTRACT: errors.append("verified_contract")
    if doc.get("proof_boundary") != "CPU_STATIC_GL_LINK_SHIM_ONLY_NONFINAL_UNTIL_ROOT_AND_TA_SHA_BOUND_ACCEPTANCE": errors.append("proof_boundary")
    if O0_STDOUT.read_bytes() != O2_STDOUT.read_bytes(): errors.append("o0_o2_stdout_mismatch")
    if (EVIDENCE / "red-v2-exit.txt").read_text() != "1\n": errors.append("red_exit")
    red = (EVIDENCE / "red-v2.stderr").read_text()
    if "stateCaptured" not in red or "restoreRequired" not in red or "6 errors generated" not in red: errors.append("red_semantics")
    return sorted(set(errors))


SOURCES = [
    "tests/phase4_v19_trajectory_inset_visible_integration_cpu_tests.cpp",
    "src/Billiards/render/modern/product_ui_gl_core41.cpp",
    "src/Billiards/render/modern/trajectory_inset_visible_integration.cpp",
    "src/Billiards/render/modern/modern_product_ui_runtime.cpp",
    "src/Billiards/render/modern/modern_renderer_product_ui.cpp",
    "src/Billiards/render/ui/product_ui_gpu_renderer.cpp",
    "src/Billiards/render/ui/product_shell_layout.cpp",
    "src/Billiards/render/ui/product_shell_draw_list.cpp",
    "src/Billiards/render/ui/product_ui_frame_coordinator.cpp",
    "src/Billiards/product_hud.cpp", "src/Billiards/product_settings.cpp",
    "src/Billiards/render/ui/hud_draw_list.cpp",
    "src/Billiards/render/ui/hud_shipping_tokens.cpp",
    "src/Billiards/render/ui/rules_decision_product_draw.cpp",
    "src/Billiards/render/ui/rules_decision_ui_draw_list.cpp",
    "src/Billiards/render/ui/sdf_font_atlas.cpp",
    "src/Billiards/render/ui/sdf_generator.cpp",
    "src/Billiards/training/trajectory_inset_model.cpp",
    "src/Billiards/training/trajectory_inset_renderer.cpp",
    "src/Billiards/deterministic_hash.cpp", "src/Billiards/automation_json.cpp",
]


def pkg_config(option: str) -> list[str]:
    result = subprocess.run(["pkg-config", option, "freetype2"], cwd=ROOT, text=True, capture_output=True)
    if result.returncode: raise RuntimeError(result.stderr)
    return result.stdout.split()


def fresh_replay() -> list[str]:
    with tempfile.TemporaryDirectory(prefix="phase4-v19-cp4-d-") as directory:
        temp = pathlib.Path(directory)
        executable = temp / "actual_chain"
        command = ["/usr/bin/c++", "-std=c++17", "-O2", "-Wno-deprecated-declarations", "-Isrc/Billiards", *pkg_config("--cflags"), *SOURCES, *pkg_config("--libs"), "-o", str(executable)]
        built = subprocess.run(command, cwd=ROOT, text=True, capture_output=True)
        if built.returncode: return ["fresh_o2_compile:" + built.stderr[-1200:]]
        run = subprocess.run([str(executable)], cwd=ROOT, text=True, capture_output=True)
        if run.returncode or run.stdout != O2_STDOUT.read_text(): return ["fresh_o2_replay:" + run.stderr[-1200:]]
        modern = subprocess.run(["/usr/bin/c++", "-std=c++17", "-O2", "-Wno-deprecated-declarations", "-Isrc/Billiards", *pkg_config("--cflags"), "-c", PINNED["modern_cpp"][0], "-o", str(temp / "modern.o")], cwd=ROOT, text=True, capture_output=True)
        if modern.returncode: return ["modern_compile:" + modern.stderr[-1200:]]
    return []


def static_contract() -> list[str]:
    errors: list[str] = []
    header = (ROOT / PINNED["gpu_h"][0]).read_text()
    gpu = (ROOT / PINNED["gpu_cpp"][0]).read_text()
    core = (ROOT / PINNED["core41_cpp"][0]).read_text()
    runtime = (ROOT / PINNED["runtime_cpp"][0]).read_text()
    modern = (ROOT / PINNED["modern_cpp"][0]).read_text()
    test = (ROOT / PINNED["cpu_test"][0]).read_text()
    for token in ["class UiGpuBeginReceipt", "preCaptureFailure", "capturedFailure", "capturedSuccess", "beginFrameReceipt"]:
        if token not in header: errors.append("typed_receipt:" + token)
    begin = core.find("UiGpuBeginReceipt Core41ProductUiGlDispatch::beginFrameReceipt")
    upload = core.find("UiGpuResult Core41ProductUiGlDispatch::uploadVertices", begin)
    if begin < 0 or upload < 0 or "endFrame()" in core[begin:upload] or "state_ = {}" in core[begin:upload]: errors.append("core41_internal_restore")
    for token in ["beginFrameReceipt(", "restoreExactlyOnce", "trajectory_inset_chrome_restore_failed"]:
        if token not in gpu: errors.append("outer_owner:" + token)
    for token in ["trajectory_inset_chrome_context_poisoned", "trajectoryInsetChromeContextPoisoned_ = true", "chromeSamplePosition"]:
        if token not in runtime: errors.append("runtime:" + token)
    for token in ["configureTrajectoryInsetChromeQueries", "glGetFramebufferAttachmentParameteriv", "glGetError"]:
        if token not in modern: errors.append("target_query:" + token)
    for token in ["actualCore41SingleRestoreOwnershipRed", "ModernProductUiRuntime actualRuntime(actualCore41)", "production-style begin fail plus restore fail/throw", "fractionalCutoutSampleRegression", "CP4_CORE41_ONLY"]:
        if token not in test: errors.append("test_fixture:" + token)
    if "beginFrameFailureRestoresStateInternally" in header + gpu + modern: errors.append("boolean_escape_hatch")
    if runtime.count("gpuRenderer_->renderTrajectoryInsetChrome(") != 1 or "trajectory_inset_chrome_echo_receipt_forbidden" not in runtime: errors.append("runtime_real_echo_separation")
    return errors


def validate(path: pathlib.Path = RECEIPT, replay: bool = True) -> list[str]:
    try: doc = json.loads(path.read_text())
    except Exception as exc: return ["receipt_parse:" + str(exc)]
    errors = validate_document(doc)
    if replay and not errors:
        errors += fresh_replay()
        if not errors: errors += static_contract()
        if not errors:
            for script in ["scripts/validate_phase4_v19_cp4_d_product_ui_chrome_submission_correction_gate.py", "scripts/validate_phase4_v19_cp4_d_core41_restore_ownership_allowlist_correction_gate.py"]:
                result = subprocess.run([sys.executable, script], cwd=ROOT, text=True, capture_output=True)
                if result.returncode: errors.append("gate_replay:" + script)
    return sorted(set(errors))


if __name__ == "__main__":
    result = validate()
    print(json.dumps({"valid": not result, "errors": result}, sort_keys=True))
    sys.exit(1 if result else 0)
