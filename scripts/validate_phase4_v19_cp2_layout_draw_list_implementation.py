#!/usr/bin/env python3
import hashlib, json, os, pathlib, subprocess, sys, tempfile

ROOT = pathlib.Path(__file__).resolve().parents[1]
RECEIPT = ROOT / "docs/evidence/phase4-v19-cp2-layout-draw-list-implementation-v1/receipt.json"
STDOUT = ROOT / "docs/evidence/phase4-v19-cp2-layout-draw-list-implementation-v1/cpu-test.stdout"

def sha(path): return hashlib.sha256(path.read_bytes()).hexdigest()

PINNED = {
 "cp2_checklist": ("docs/art/phase4-v19-cp2-layout-draw-list-implementation-acceptance-checklist-v1.json", "c20c65c8e240f2c29a56fac793f6f8d424be2bbc21b7dee2d6db2dd505c46d90"),
 "cp1_receipt": ("docs/evidence/phase4-v19-cp1-trajectory-inset-foundation-implementation-v1/receipt.json", "3208c819bd81ddc2143921a3b3ef2136127b275317a6ae0f5549de18188de758"),
 "layout_h": ("src/Billiards/render/ui/product_shell_layout.h", "8d70d3355bf40ac78bc212988a4205fa08c2c2caac95f816dfbdf5d6140dc5ec"),
 "layout_cpp": ("src/Billiards/render/ui/product_shell_layout.cpp", "50d99d388dfa72c81cbec516e6661349b5238fc5aea5b832db9af4e87eb02189"),
 "draw_h": ("src/Billiards/render/ui/product_shell_draw_list.h", "57fbd8ce714274ef4957debe462aa36d91c48e4297486fb9609eec61a061eac2"),
 "draw_cpp": ("src/Billiards/render/ui/product_shell_draw_list.cpp", "f6b705a100d18cc0218b69e36a74a403fb65987fd9060a047722ca78d391b45c"),
 "cpu_test": ("tests/phase4_v19_trajectory_inset_layout_draw_list_cpu_tests.cpp", "810e129aedfef913d4947ebdb0de65944ef0df82029d9bd27e6d26016bac07af"),
 "cmake_untouched": ("CMakeLists.txt", "f41563c35224dd809f64acedba6455355e70082697c41305c2d1b05426f71d41"),
 "modern_cpp_untouched": ("src/Billiards/render/modern/modern_renderer.cpp", "b00cc6b370122275a42f106967f935dbb34bd5d3f81f2a750ca9ba6e503af191"),
 "modern_h_untouched": ("src/Billiards/render/modern/modern_renderer.h", "936de5b77c9e3485f2e51f10e9de26c59c8ec94674865e0b50ae125725183c9d"),
 "cp1_model_h": ("src/Billiards/training/trajectory_inset_model.h", "3a6e0550d1f85b9168528454b74c5768c5d6c9b9ba6db0dd8fd4fdac1c66a8c6"),
 "cp1_model_cpp": ("src/Billiards/training/trajectory_inset_model.cpp", "58a9a3dfe922c235314884d36bd8cea47df8cfe400adcf5a4a716e64e093e2b4"),
 "cp1_renderer_h": ("src/Billiards/training/trajectory_inset_renderer.h", "2e7191c1b260dc257a07f5868d0dc1a9dc0012e69696229f3fd0118c02af3947"),
 "cp1_renderer_cpp": ("src/Billiards/training/trajectory_inset_renderer.cpp", "9969c1736fb89329e25dfa1d0252f18e5475d18a9027ddebf740cab4e8804201"),
 "cp1_test": ("tests/phase4_v19_trajectory_inset_cpu_tests.cpp", "56c1c8e21bcd87b00cc11cdb5e21012a2acb0b4c3dcd4e08b98b9dfb254eef6b"),
}

EXPECTED_PROFILES = ["1920x1080_ui100","1920x1080_ui135","1280x720_ui100","1280x720_ui135"]

def validate_document(doc):
    errors=[]
    if doc.get("schema") != "billiardgl.phase4.v19.cp2_layout_draw_list_implementation_receipt.v1": errors.append("schema")
    if doc.get("status") != "CP2_CPU_STATIC_SOURCE_CANDIDATE_NO_CP3_NO_GPU_NO_READINESS_NO_AUTH": errors.append("status")
    bindings={x.get("role"):(x.get("path"),x.get("sha256")) for x in doc.get("bindings",[])}
    if bindings != PINNED | {
        "cpu_stdout": (str(STDOUT.relative_to(ROOT)), doc.get("cpu_stdout_sha256")),
        "validator": ("scripts/validate_phase4_v19_cp2_layout_draw_list_implementation.py", doc.get("validator_sha256")),
        "mutation_tests": ("tests/test_phase4_v19_cp2_layout_draw_list_implementation.py", doc.get("mutation_tests_sha256"))}: errors.append("bindings")
    for role,(path,digest) in bindings.items():
        target=ROOT/path
        if not target.is_file() or sha(target)!=digest: errors.append("binding:"+role)
    contract=doc.get("verified_contract",{})
    exact={"profiles_in_order":EXPECTED_PROFILES,"transparent_cutout_profiles":4,"named_exclusions":5,"fail_closed_mutations":27,"cp1_inventory_rows":4,"default_inset_on":True,"default_world_off":True,"bilingual_toggle_label":"Trajectory inset / 轨迹小窗","focused_only_focus_primitive":True,"trajectory_semantic_members":0,"trajectory_probes":0,"trajectory_denominator_contribution":0,"cp3_started":False,"gpu_run":False,"readiness_created":False,"authorization_created":False}
    for key,value in exact.items():
        if contract.get(key)!=value: errors.append("contract:"+key)
    if doc.get("cp1_canonical_inventory") != json.loads((ROOT/PINNED["cp1_receipt"][0]).read_text())["canonical_layout_inventory"]: errors.append("cp1_inventory")
    return sorted(set(errors))

def fresh_replay():
    with tempfile.TemporaryDirectory(prefix="phase4-v19-cp2-") as td:
        exe=pathlib.Path(td)/"cp2"
        cflags=subprocess.check_output(["pkg-config","--cflags","freetype2"],text=True).split()
        libs=subprocess.check_output(["pkg-config","--libs","freetype2"],text=True).split()
        cmd=["/usr/bin/clang++","-std=c++17","-O2","-Isrc/Billiards",*cflags,
          "tests/phase4_v19_trajectory_inset_layout_draw_list_cpu_tests.cpp",
          "src/Billiards/render/ui/product_shell_layout.cpp","src/Billiards/render/ui/product_shell_draw_list.cpp",
          "src/Billiards/render/ui/sdf_font_atlas.cpp","src/Billiards/render/ui/sdf_generator.cpp",
          "src/Billiards/deterministic_hash.cpp","src/Billiards/automation_json.cpp",*libs,"-o",str(exe)]
        build=subprocess.run(cmd,cwd=ROOT,text=True,capture_output=True)
        if build.returncode: return ["fresh_compile:"+build.stderr[-500:]]
        run=subprocess.run([str(exe)],cwd=ROOT,text=True,capture_output=True)
        if run.returncode or run.stdout != STDOUT.read_text(): return ["fresh_replay"]
    return []

def existing_product_shell_replay():
    build=ROOT/"build-integrated-final-shared-phase4-v11-phase5-v5v2-phase2-successor-v1"
    binary=build/"BilliardsProductShellTests"
    if not (build/"CMakeCache.txt").is_file(): return ["product_shell_build_missing"]
    rebuilt=subprocess.run(["cmake","--build",str(build),"--target",
        "BilliardsProductShellTests","-j","6"],cwd=ROOT,text=True,capture_output=True)
    if rebuilt.returncode: return ["product_shell_fresh_build"]
    run=subprocess.run([str(binary)],cwd=ROOT,text=True,capture_output=True)
    if run.returncode or run.stdout != "Product shell controller and shipping draw-list checks passed\n":
        return ["product_shell_fresh_replay"]
    return []

def validate(path=RECEIPT, replay=True):
    try: doc=json.loads(path.read_text())
    except Exception as exc: return ["receipt_parse:"+str(exc)]
    errors=validate_document(doc)
    if replay and not errors:
        errors += fresh_replay()
        if not errors: errors += existing_product_shell_replay()
    return errors

if __name__=="__main__":
    errors=validate()
    print(json.dumps({"valid":not errors,"errors":errors},sort_keys=True))
    sys.exit(1 if errors else 0)
