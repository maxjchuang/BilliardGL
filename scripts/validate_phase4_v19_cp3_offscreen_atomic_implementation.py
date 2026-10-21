#!/usr/bin/env python3
import hashlib
import json
import pathlib
import subprocess
import sys
import tempfile

ROOT = pathlib.Path(__file__).resolve().parents[1]
EVIDENCE = ROOT / "docs/evidence/phase4-v19-cp3-offscreen-atomic-implementation-v1"
RECEIPT = EVIDENCE / "receipt.json"
STDOUT = EVIDENCE / "cpu-test.stdout"
ADAPTER_RED = EVIDENCE / "adapter-red.stdout"


def sha(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


PINNED = {
    "cp3_checklist": ("docs/art/phase4-v19-cp3-offscreen-atomic-implementation-acceptance-checklist-v1.json", "c232427c702bc25b07f2f836cb7b6f4dc22d872b69dd4de24e77e590deff3a9d"),
    "cp1_receipt": ("docs/evidence/phase4-v19-cp1-trajectory-inset-foundation-implementation-v1/receipt.json", "3208c819bd81ddc2143921a3b3ef2136127b275317a6ae0f5549de18188de758"),
    "cp2_receipt": ("docs/evidence/phase4-v19-cp2-layout-draw-list-implementation-v1/receipt.json", "2524223aa320ff5aecb70a164bfeac471d9fc36f09a67b30a1bb7486333bbbc5"),
    "modern_h": ("src/Billiards/render/modern/modern_renderer.h", "ad611b763397df677606d236e4f33d401676cc2f96042cf0a7e355c64989fe67"),
    "modern_cpp": ("src/Billiards/render/modern/modern_renderer.cpp", "bfe8565324a020b7f3171d332f7d10bdd9c61f0b0b3b4b62a8b5c10d6f0dd561"),
    "transaction_h": ("src/Billiards/render/modern/trajectory_inset_atomic_transaction.h", "69be6cb1e61385e0b70743bc80bb8ab01e9a483b1c79358ab1fc03d578a5ed64"),
    "transaction_cpp": ("src/Billiards/render/modern/trajectory_inset_atomic_transaction.cpp", "183f630052f7676b37f0b66da6f6b2725faf6da930f6dc3cea79c6b160a4c6b2"),
    "cpu_test": ("tests/phase4_v19_trajectory_inset_offscreen_atomic_cpu_tests.cpp", "0c75c1da8d67764cb0700cbd6daad099d38445955cfc8945c5c4dc95511255f8"),
    "cmake_external_b2_lineage": ("CMakeLists.txt", "5f396126815f2e536cc3ba52a16edf03353648890bbf36b576f22bd3f5424b93"),
}


def expected_dynamic(doc):
    return {
        "cpu_stdout": (str(STDOUT.relative_to(ROOT)), doc.get("cpu_stdout_sha256")),
        "adapter_red_stdout": (str(ADAPTER_RED.relative_to(ROOT)), doc.get("adapter_red_stdout_sha256")),
        "validator": ("scripts/validate_phase4_v19_cp3_offscreen_atomic_implementation.py", doc.get("validator_sha256")),
        "mutation_tests": ("tests/test_phase4_v19_cp3_offscreen_atomic_implementation.py", doc.get("mutation_tests_sha256")),
    }


def validate_document(doc):
    errors = []
    if doc.get("schema") != "billiardgl.phase4.v19.cp3_offscreen_atomic_implementation_receipt.v1": errors.append("schema")
    if doc.get("status") != "CP3_CPU_STATIC_SOURCE_CANDIDATE_NO_CP4_NO_GPU_NO_BILLIARDS_NO_READINESS_NO_AUTH": errors.append("status")
    bindings = {x.get("role"): (x.get("path"), x.get("sha256")) for x in doc.get("bindings", [])}
    expected = PINNED | expected_dynamic(doc)
    if bindings != expected: errors.append("bindings")
    for role, (path, digest) in bindings.items():
        target = ROOT / path
        if not target.is_file() or not isinstance(digest, str) or sha(target) != digest:
            errors.append("binding:" + role)
    contract = doc.get("verified_contract", {})
    exact = {
        "sample_profiles": [1, 2, 4],
        "failure_stages": 15,
        "success_cases": 6,
        "invoke_false_failures": 15,
        "invoke_true_gl_error_failures": 15,
        "helper_throw_failures": 14,
        "publish_failures": 2,
        "transaction_red_status": "RED_UNVERIFIABLE",
        "adapter_red_status": "VERIFIED_MISSING_EXPLICIT_INPUT_API",
        "typed_live_main_resource_alias_check": True,
        "exact_fbo_attachment_mapping_check": True,
        "frozen_role_palette_validation": True,
        "instrumentation_color_semantic_current_token_pairing": True,
        "render_frame_prepare_call_count": 0,
        "visible_composite_deferred_to_cp4": True,
        "cmake_modified_by_cp3": False,
        "cmake_external_lineage": "M4_B2_AUTHORIZED_FOUNDATION_DRIFT",
        "gpu_run": False,
        "billiards_executed": False,
        "readiness_created": False,
        "authorization_created": False,
        "cp4_started": False,
    }
    for key, value in exact.items():
        if contract.get(key) != value: errors.append("contract:" + key)
    try:
        cp1 = json.loads((ROOT / PINNED["cp1_receipt"][0]).read_text())
        cp2 = json.loads((ROOT / PINNED["cp2_receipt"][0]).read_text())
        inventory = cp1["canonical_layout_inventory"]
        if doc.get("cp1_canonical_inventory") != inventory: errors.append("cp1_inventory")
        if cp2.get("cp1_canonical_inventory") != inventory: errors.append("cp2_cp1_inventory_lineage")
        if doc.get("cp2_verified_contract") != cp2.get("verified_contract"): errors.append("cp2_contract_lineage")
    except Exception as exc:
        errors.append("lineage_parse:" + str(exc))
    if ADAPTER_RED.is_file() and ADAPTER_RED.read_text() != "FAIL production adapter contract token TrajectoryInsetAtomicFrameInput\n":
        errors.append("adapter_red_stdout")
    return sorted(set(errors))


def compile_flags():
    return [
        "-DGL_SILENCE_DEPRECATION", "-Isrc/Billiards",
        "-isystem", "/opt/homebrew/include/freetype2", "-iframework",
        "/Applications/Xcode.app/Contents/Developer/Platforms/MacOSX.platform/Developer/SDKs/MacOSX.sdk/System/Library/Frameworks",
        "-std=c++17", "-arch", "arm64",
    ]


def fresh_replay():
    with tempfile.TemporaryDirectory(prefix="phase4-v19-cp3-") as directory:
        temporary = pathlib.Path(directory)
        exe = temporary / "cp3"
        command = ["/usr/bin/c++", "-std=c++17", "-O2", "-Isrc/Billiards",
                   PINNED["cpu_test"][0], PINNED["transaction_cpp"][0], "-o", str(exe)]
        build = subprocess.run(command, cwd=ROOT, text=True, capture_output=True)
        if build.returncode: return ["fresh_cpu_compile:" + build.stderr[-600:]]
        run = subprocess.run([str(exe)], cwd=ROOT, text=True, capture_output=True)
        if run.returncode or run.stdout != STDOUT.read_text(): return ["fresh_cpu_replay"]
        for optimization in ("-O0", "-O2"):
            modern_object = temporary / ("modern_" + optimization[2:] + ".o")
            modern = subprocess.run(["/usr/bin/c++", *compile_flags(), optimization,
                                     "-DNDEBUG", "-c", PINNED["modern_cpp"][0],
                                     "-o", str(modern_object)], cwd=ROOT,
                                    text=True, capture_output=True)
            if modern.returncode: return ["modern_compile_" + optimization + ":" + modern.stderr[-600:]]
    return []


def static_contract():
    errors = []
    source = (ROOT / PINNED["modern_cpp"][0]).read_text()
    header = (ROOT / PINNED["modern_h"][0]).read_text()
    transaction = (ROOT / PINNED["transaction_cpp"][0]).read_text()
    render_begin = source.find("void ModernRenderer::renderFrame")
    capture_begin = source.find("bool ModernRenderer::capture(", render_begin)
    if render_begin < 0 or capture_begin < 0 or "prepareTrajectoryInsetAtomicFrame" in source[render_begin:capture_begin]:
        errors.append("render_frame_auto_prepare")
    for token in [
        "forbiddenMainResources", "externalTextureKey", "externalRenderbufferKey",
        "glGetFramebufferAttachmentParameteriv", "framebufferAttachmentMappingVerified",
        "trajectoryInsetGeometryDigest", "expectedTrajectoryInsetColor",
        "trajectory_inset_instrumentation_member_color_mismatch",
        "GL_COLOR_WRITEMASK", "GL_DRAW_BUFFER0", "glDrawBuffers(",
        "glDisable(GL_SCISSOR_TEST)", "glDisable(GL_CULL_FACE)",
        "trajectory_inset_visible_composite_deferred_to_cp4",
    ]:
        if token not in source: errors.append("modern_token:" + token)
    for token in ["TrajectoryInsetAtomicFrameInput", "cp1GeometryDigest", "cp2LayoutDigest"]:
        if token not in header: errors.append("header_token:" + token)
    for token in ["validateTrajectoryInsetAtomicResourceIsolation", "trajectory_inset_attachment_alias",
                  "trajectory_inset_framebuffer_feedback", "trajectory_inset_main_resource_kind_invalid",
                  "trajectory_inset_framebuffer_attachment_mapping_invalid"]:
        if token not in transaction: errors.append("transaction_token:" + token)
    return errors


def validate(path=RECEIPT, replay=True):
    try:
        doc = json.loads(path.read_text())
    except Exception as exc:
        return ["receipt_parse:" + str(exc)]
    errors = validate_document(doc)
    if replay and not errors:
        errors += fresh_replay()
        if not errors: errors += static_contract()
    return errors


if __name__ == "__main__":
    errors = validate()
    print(json.dumps({"valid": not errors, "errors": errors}, sort_keys=True))
    sys.exit(1 if errors else 0)
