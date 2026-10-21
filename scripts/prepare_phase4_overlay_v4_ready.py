#!/usr/bin/env python3
"""Prepare hash-bound Phase 4 v4 NOT_RUN templates only.

This orchestrator has deliberately no GPU execution mode.  It verifies the
telemetry runner's offline contract self-test, deterministically qualifies the
two non-overlap controls, and emits a new v4 fixture index, 11-case targeted
preflight ready manifest, and 68-selector formal ready template.  It never
authorizes or performs GPU capture, TA review, formal68, M3, or human claims.
"""

from __future__ import annotations

import argparse
import hashlib
import json
from pathlib import Path
import re
import subprocess

ROOT = Path(__file__).resolve().parents[1]
GATE = ROOT / "docs/art/phase4-overlay-v4-compound-visibility-correction-design-gate-v1.json"
GATE_SHA256 = "1901a825b092c08ce79425ec936ec150fada9ff4e3b4f9817e818d481013a39f"
DESIGN_GATE = ROOT / "docs/art/phase4-training-replay-overlay-design-gate-v1.json"
V3_FIXTURES = ROOT / "docs/art/phase4-training-replay-overlay-fixtures-v3/fixtures.json"
FROZEN_FAILURE_RECEIPT = ROOT / "docs/art/phase4-training-replay-overlay-gpu-capture-v3/failure-receipt.json"
FROZEN_FAILURE_RECEIPT_SHA256 = "a6c900f7e96e523a4f45bfd153430857b3114997648a31dd8c8aa145bf6615ed"

SOURCE_PATHS = [
    "src/Billiards/training/trajectory_overlay_renderer.h",
    "src/Billiards/training/trajectory_overlay_renderer.cpp",
    "src/Billiards/training/trajectory_compound_visibility.h",
    "src/Billiards/training/trajectory_compound_visibility.cpp",
    "spikes/render_core_profile/phase4_overlay_v4_compound_audit_runner.cpp",
    "spikes/render_core_profile/phase4_overlay_v4_topology_audit.cpp",
    "spikes/render_core_profile/phase4_overlay_v4_gpu_capture_support.inc",
    "scripts/generate_phase4_overlay_v4_gpu_runner.py",
    "scripts/run_phase4_overlay_v4_gpu_capture.py",
    "tests/trajectory_compound_visibility_tests.cpp",
    "scripts/prepare_phase4_overlay_v4_ready.py",
]

LINKED_TARGETS = (
    "Billiards",
    "BilliardsPhase4OverlayV4GpuCaptureRunner",
    "BilliardsModernGpuResources",
    "BilliardsGltfGpuBridge",
    "BilliardsModernProductUiRuntime",
    "BilliardsProductUiOpenGL41",
    "BilliardsProductUiGpuCore",
)


def sha(path: Path) -> str:
    return hashlib.sha256(path.read_bytes()).hexdigest()


def source_binding_manifest(build: Path, runner: Path,
                            billiards: Path) -> dict:
    """Bind transitive in-repo compile inputs and final link descriptions."""
    compile_commands = build / "compile_commands.json"
    if not compile_commands.is_file():
        raise RuntimeError("fresh build must export compile_commands.json")
    repository_inputs = {Path(relative) for relative in SOURCE_PATHS}
    repository_inputs.add(Path("CMakeLists.txt"))
    root_prefix = str(ROOT.resolve()) + "/"
    for target in LINKED_TARGETS:
        dependency_root = build / "CMakeFiles" / f"{target}.dir"
        if not dependency_root.is_dir():
            raise RuntimeError(f"linked target dependency tree missing: {target}")
        for dependency in dependency_root.rglob("*.d"):
            text = dependency.read_text(encoding="utf-8", errors="replace")
            for absolute in re.findall(re.escape(root_prefix) + r"[^\s\\]+", text):
                path = Path(absolute)
                if path.is_file():
                    repository_inputs.add(path.relative_to(ROOT.resolve()))
    files = []
    for relative in sorted(repository_inputs, key=lambda value: value.as_posix()):
        path = ROOT / relative
        if not path.is_file():
            raise RuntimeError(f"bound repository source missing: {relative}")
        files.append({"path": relative.as_posix(), "scope": "repository",
                      "sha256": sha(path)})

    generated_runner = build / "generated/phase4_overlay_v4_gpu_capture_runner.mm"
    if not generated_runner.is_file():
        raise RuntimeError("generated v4 runner source missing")
    files.append({
        "path": "@build/generated/phase4_overlay_v4_gpu_capture_runner.mm",
        "scope": "generated_runner_source",
        "sha256": sha(generated_runner),
    })
    files.append({
        "path": "@build/compile_commands.json",
        "scope": "fresh_release_compile_graph",
        "sha256": sha(compile_commands),
    })
    for target in LINKED_TARGETS:
        link = build / "CMakeFiles" / f"{target}.dir/link.txt"
        if not link.is_file():
            raise RuntimeError(f"linked target command missing: {target}")
        files.append({
            "path": f"@build/CMakeFiles/{target}.dir/link.txt",
            "scope": "final_link_command",
            "sha256": sha(link),
        })
    files.sort(key=lambda value: value["path"])
    aggregate = hashlib.sha256()
    for row in files:
        aggregate.update(row["path"].encode("utf-8"))
        aggregate.update(b"\0")
        aggregate.update(row["sha256"].encode("ascii"))
        aggregate.update(b"\n")
    required = {
        "src/Billiards/render/modern/modern_renderer.cpp",
        "src/Billiards/render/modern/modern_product_ui_runtime.cpp",
        "src/Billiards/render/modern/modern_product_session.cpp",
        "src/Billiards/render/ui/product_ui_frame_coordinator.cpp",
        "src/Billiards/render/ui/rules_decision_product_draw.cpp",
        "src/Billiards/render/ui/rules_decision_ui_draw_list.cpp",
        "src/Billiards/training/trajectory_overlay_renderer.cpp",
        "src/Billiards/training/trajectory_compound_visibility.cpp",
    }
    observed = {row["path"] for row in files}
    if not required.issubset(observed):
        raise RuntimeError("final link source binding lacks required Phase4/5 inputs")
    return {
        "schema": "billiardgl.phase4.overlay_v4_source_bindings.v3",
        "status": "FRESH_RELEASE_FINAL_LINK_INPUTS_BOUND_NOT_GPU_EVIDENCE",
        "aggregate_algorithm": "sha256(path_utf8 + NUL + file_sha256_ascii + LF), sorted by path",
        "aggregate_sha256": aggregate.hexdigest(),
        "file_count": len(files),
        "linked_targets": list(LINKED_TARGETS),
        "build_type": "Release",
        "build_root": str(build.resolve()),
        "runner_source_path": str(generated_runner.resolve()),
        "runner_source_sha256": sha(generated_runner),
        "runner_binary_path": str(runner.resolve()),
        "runner_binary_sha256": sha(runner),
        "billiards_binary_path": str(billiards.resolve()),
        "billiards_binary_sha256": sha(billiards),
        "files": files,
    }


def run_json(command: list[str]) -> dict:
    completed = subprocess.run(command, check=True, text=True,
                               capture_output=True)
    return json.loads(completed.stdout)


def terminal_outcome(design: dict, scenario_id: str, state: str) -> str:
    if state in {"predicted_only_active_collapsed",
                 "actual_replay_paused_collapsed"}:
        return "not_terminal"
    representatives = design["actual_gpu_evidence_matrix"][
        "representative_extension"]["category_representatives"]
    if scenario_id in representatives:
        return "failure" if representatives.index(scenario_id) in (2, 3) \
            else "success"
    scenarios = [value["id"] for value in design["scenario_contract"]]
    return "success" if scenarios.index(scenario_id) % 2 == 0 else "failure"


def fixture_index(v3: dict, billiards: Path, source_sha: str,
                  source_bindings_path: Path,
                  source_bindings_sha: str) -> dict:
    executable_sha = sha(billiards)
    rows = []
    for value in v3["fixtures"]:
        row = dict(value)
        row["executable_path"] = str(billiards.resolve())
        row["executable_sha256"] = executable_sha
        row["status"] = "V4_HASH_REBOUND_REPLAY_CONTENT_UNCHANGED_NOT_GPU_EVIDENCE"
        row["source_binding_manifest_path"] = str(
            source_bindings_path.resolve())
        row["source_binding_manifest_sha256"] = source_bindings_sha
        rows.append(row)
    return {
        "schema": "billiardgl.phase4.overlay_replay_fixtures.v4",
        "status": "V4_HASH_REBOUND_REPLAY_CONTENT_UNCHANGED_NOT_GPU_EVIDENCE",
        "v3_fixture_manifest_reused_as_v4_identity": False,
        "replay_content_intentionally_unchanged": True,
        "source_sha256": source_sha,
        "source_binding_manifest_path": str(source_bindings_path.resolve()),
        "source_binding_manifest_sha256": source_bindings_sha,
        "training_pack_path": v3["training_pack_path"],
        "training_pack_sha256": v3["training_pack_sha256"],
        "executable_path": str(billiards.resolve()),
        "executable_sha256": executable_sha,
        "fixture_count": 12,
        "actual_gpu_capture_count": 0,
        "evidence_claims": {
            "actual_gpu_capture": False,
            "machine_capture": False,
            "technical_art_review": False,
            "manual_review": False,
            "m3": False,
            "formal68": False,
            "golden": False,
        },
        "fixtures": rows,
    }


def base_capture(capture_id: str, fixture: dict, camera: str, profile: dict,
                 group: str, source_sha: str, runner: Path,
                 billiards_sha: str) -> dict:
    return {
        "capture_id": capture_id,
        "group": group,
        "scenario_id": fixture["scenario_id"],
        "scenario_sha256": fixture["scenario_sha256"],
        "camera_preset": camera,
        "physical_viewport_px": profile["physical"],
        "logical_viewport_px": profile.get("logical"),
        "device_scale": profile.get("device", 1.0),
        "palette": profile.get("palette", "standard"),
        "ui_scale": profile.get("ui", 1.0),
        "overlay_state": profile.get("state", "comparison_ready_collapsed"),
        "replay_fixture_path": fixture["replay_path"],
        "replay_fixture_sha256": fixture["replay_sha256"],
        "source_sha256": source_sha,
        "runner_sha256": sha(runner),
        "billiards_executable_sha256": billiards_sha,
        "executable_sha256": billiards_sha,
        "actual_gpu_required": True,
        "actual_gpu_capture_count": 0,
        "machine_status": "NOT_RUN",
        "ta_status": "NOT_RUN",
        "gpu_capture_authorized": False,
        "formal68_authorized": False,
        "evidence_claims": {
            "actual_gpu_capture": False,
            "machine_capture": False,
            "technical_art_review": False,
            "manual_review": False,
            "m3": False,
            "formal68": False,
            "golden": False,
        },
    }


def bind_capture_dependencies(captures: list[dict], runner: Path,
                              billiards: Path,
                              fixture_manifest_path: Path,
                              source_binding_manifest_path: Path,
                              runner_source: Path) -> None:
    """Bind every executable/configuration dependency at capture-row scope."""
    for row in captures:
        row["runner_path"] = str(runner.resolve())
        row["runner_sha256"] = sha(runner)
        row["billiards_executable_path"] = str(billiards.resolve())
        row["billiards_executable_sha256"] = sha(billiards)
        row["executable_sha256"] = sha(billiards)
        row["fixture_manifest_path"] = str(fixture_manifest_path.resolve())
        row["fixture_manifest_sha256"] = sha(fixture_manifest_path)
        row["correction_design_gate_path"] = str(GATE.resolve())
        row["correction_design_gate_sha256"] = GATE_SHA256
        row["runner_source_path"] = str(runner_source.resolve())
        row["runner_source_sha256"] = sha(runner_source)
        row["source_binding_manifest_path"] = str(
            source_binding_manifest_path.resolve())
        row["source_binding_manifest_sha256"] = sha(
            source_binding_manifest_path)


def targeted_manifest(fixtures: dict, source_sha: str, runner: Path,
                      topology: Path, fixture_manifest_path: Path,
                      billiards: Path, source_binding_manifest_path: Path,
                      runner_source: Path, frozen_constraints: dict) -> dict:
    by_id = {value["scenario_id"]: value for value in fixtures["fixtures"]}
    first = by_id["launch_stop_shot_center_v1"]
    common = {"physical": [1920, 1080]}
    captures = []
    billiards_sha = sha(billiards)
    for camera in ("player_low", "elevated", "overhead", "broadcast"):
        captures.append(base_capture(f"camera_{camera}_1080", first, camera,
            common, "original_v3_seven", source_sha, runner,
            billiards_sha))
        captures[-1]["terminal_outcome"] = "failure"
    captures.append(base_capture("comparison_720_ui135_cvd", first,
        "overhead", {"physical": [1280, 720], "palette":
        "color_vision_safe", "ui": 1.35,
        "state": "comparison_terminal_collapsed"},
        "original_v3_seven", source_sha, runner, billiards_sha))
    captures[-1]["terminal_outcome"] = "failure"
    captures.append(base_capture("actual_retina_ui135_cvd", first,
        "broadcast", {"physical": [1920, 1080], "logical": [1280, 720],
        "device": 1.5, "palette": "color_vision_safe", "ui": 1.35,
        "state": "actual_replay_paused_collapsed"},
        "original_v3_seven", source_sha, runner, billiards_sha))
    captures[-1]["terminal_outcome"] = "not_terminal"
    captures.append(base_capture("advanced_1080_ui100_standard", first,
        "elevated", {"physical": [1920, 1080],
        "state": "comparison_terminal_explicit_advanced_expanded"},
        "original_v3_seven", source_sha, runner, billiards_sha))
    captures[-1]["terminal_outcome"] = "success"
    for scenario, group in (("launch_right_english_one_rail_v1",
                             "exact_frozen_failure_case"),
                            ("launch_left_english_one_rail_v1",
                             "mirrored_case")):
        captures.append(base_capture(f"core__{scenario}__broadcast",
            by_id[scenario], "broadcast", common, group, source_sha, runner,
            billiards_sha))
    captures[-2]["terminal_outcome"] = "success"
    captures[-1]["terminal_outcome"] = "success"
    captures[-2]["minimum_source_probe_inventory"] = 159
    captures[-2]["frozen_v3_source_probe_inventory"] = 159
    captures[-2]["source_probe_inventory_may_not_decrease"] = True

    controls = []
    for scenario, camera in (("launch_corner_pocket_angle_v1", "broadcast"),
                             ("launch_side_pocket_angle_v1", "overhead")):
        fixture = by_id[scenario]
        metric = run_json([str(topology.resolve()), scenario,
                           fixture["replay_path"], camera])
        if metric["overlap_fraction_of_smaller_core"] > 0.10 or not \
                metric["both_tracks_have_source_probes"]:
            raise RuntimeError("recommended non-overlap control did not qualify")
        controls.append(metric)
        row = base_capture(metric["capture_id"], fixture, camera, common,
                           "deterministic_non_overlap_control", source_sha,
                           runner, billiards_sha)
        row["terminal_outcome"] = "failure"
        row["topology_qualification"] = metric
        captures.append(row)
    selection_bytes = "\n".join(
        f"{row['capture_id']}|{row['overlap_fraction_of_smaller_core']:.17g}|"
        f"{row['predicted_source_probe_inventory']}|"
        f"{row['actual_source_probe_inventory']}" for row in controls)
    selection_digest = hashlib.sha256(selection_bytes.encode()).hexdigest()
    bind_capture_dependencies(captures, runner, billiards,
                              fixture_manifest_path,
                              source_binding_manifest_path, runner_source)
    return {
        "schema": "billiardgl.phase4.overlay_v4_targeted_preflight_ready.v1",
        "status": "READY_NOT_RUN_GPU_NOT_AUTHORIZED",
        "correction_design_gate_path": str(GATE.resolve()),
        "correction_design_gate_sha256": GATE_SHA256,
        "source_sha256": source_sha,
        "source_binding_manifest_path": str(
            source_binding_manifest_path.resolve()),
        "source_binding_manifest_sha256": sha(source_binding_manifest_path),
        "source_aggregate_algorithm":
            "sha256(path_utf8 + NUL + file_sha256_ascii + LF), sorted by path",
        "runner_path": str(runner.resolve()),
        "runner_sha256": sha(runner),
        "runner_source_path": str(runner_source.resolve()),
        "runner_source_sha256": sha(runner_source),
        "runner_capability": "ACTUAL_GPU_CAPABLE_AUTHORIZATION_GATED_V4_COMPOUND_ATTACHMENT",
        "billiards_executable_path": str(billiards.resolve()),
        "billiards_executable_sha256": billiards_sha,
        "fixture_manifest_path": str(fixture_manifest_path.resolve()),
        "fixture_manifest_sha256": sha(fixture_manifest_path),
        "capture_count": len(captures),
        "actual_gpu_capture_count": 0,
        "ta_review_count": 0,
        "formal68_capture_count": 0,
        "gpu_capture_authorized": False,
        "technical_art_review_authorized": False,
        "formal68_authorized": False,
        "m3_authorized": False,
        "frozen_gate_constraints": frozen_constraints,
        "evidence_claims": {
            "actual_gpu_capture": False,
            "machine_capture": False,
            "technical_art_review": False,
            "manual_review": False,
            "m3": False,
            "formal68": False,
            "golden": False,
        },
        "non_overlap_control_selection_digest": selection_digest,
        "non_overlap_controls": controls,
        "captures": captures,
    }


def formal_manifest(design: dict, fixtures: dict, source_sha: str,
                    runner: Path, billiards: Path,
                    fixture_manifest_path: Path,
                    source_binding_manifest_path: Path,
                    runner_source: Path, frozen_constraints: dict) -> dict:
    by_id = {value["scenario_id"]: value for value in fixtures["fixtures"]}
    captures = []
    core = design["actual_gpu_evidence_matrix"]["core_minimum"]
    for scenario in design["scenario_contract"]:
        for camera in core["camera_presets"]:
            row = base_capture(f"core__{scenario['id']}__{camera}",
                by_id[scenario["id"]], camera,
                {"physical": core["common_profile"]["physical_viewport_px"]},
                "formal68_core", source_sha, runner, sha(billiards))
            row["terminal_outcome"] = terminal_outcome(design,
                scenario["id"], row["overlay_state"])
            captures.append(row)
    extension = design["actual_gpu_evidence_matrix"]["representative_extension"]
    for scenario_id in extension["category_representatives"]:
        for profile in extension["profiles_per_representative"]:
            row = base_capture(
                f"extension__{scenario_id}__{profile['id']}",
                by_id[scenario_id], profile["camera"], {
                    "physical": profile["physical_viewport_px"],
                    "logical": profile.get("logical_viewport_px"),
                    "device": profile["device_scale"],
                    "palette": profile["palette"],
                    "ui": profile["ui_scale"],
                    "state": profile["overlay_state"]},
                "formal68_extension", source_sha, runner, sha(billiards))
            row["terminal_outcome"] = terminal_outcome(design,
                scenario_id, row["overlay_state"])
            captures.append(row)
    if len(captures) != 68 or len({row["capture_id"] for row in captures}) != 68:
        raise RuntimeError("formal selector derivation is not exactly 68 unique rows")
    bind_capture_dependencies(captures, runner, billiards,
                              fixture_manifest_path,
                              source_binding_manifest_path, runner_source)
    return {
        "schema": "billiardgl.phase4.overlay_v4_formal68_ready_template.v1",
        "status": "HASH_BOUND_READY_TEMPLATE_NOT_AUTHORIZED_NOT_RUN",
        "selector_source": str(DESIGN_GATE.resolve()),
        "v3_ready_manifest_reused": False,
        "v3_runner_reused": False,
        "v3_source_digest_reused_as_v4_identity": False,
        "correction_design_gate_sha256": GATE_SHA256,
        "source_sha256": source_sha,
        "source_binding_manifest_path": str(
            source_binding_manifest_path.resolve()),
        "source_binding_manifest_sha256": sha(source_binding_manifest_path),
        "source_aggregate_algorithm":
            "sha256(path_utf8 + NUL + file_sha256_ascii + LF), sorted by path",
        "runner_path": str(runner.resolve()),
        "runner_sha256": sha(runner),
        "runner_source_path": str(runner_source.resolve()),
        "runner_source_sha256": sha(runner_source),
        "runner_capability": "ACTUAL_GPU_CAPABLE_AUTHORIZATION_GATED_V4_COMPOUND_ATTACHMENT",
        "billiards_executable_path": str(billiards.resolve()),
        "billiards_executable_sha256": sha(billiards),
        "fixture_manifest_path": str(fixture_manifest_path.resolve()),
        "fixture_manifest_sha256": sha(fixture_manifest_path),
        "capture_count": 68,
        "actual_gpu_capture_count": 0,
        "ta_review_count": 0,
        "formal68_authorized": False,
        "gpu_capture_authorized": False,
        "requires_separate_root_authorization": True,
        "frozen_gate_constraints": frozen_constraints,
        "evidence_claims": {
            "actual_gpu_capture": False,
            "machine_capture": False,
            "technical_art_review": False,
            "manual_review": False,
            "m3": False,
            "formal68": False,
            "golden": False,
        },
        "captures": captures,
    }


def write_json(path: Path, value: dict) -> None:
    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_text(json.dumps(value, indent=2) + "\n", encoding="utf-8")


def main() -> None:
    parser = argparse.ArgumentParser()
    parser.add_argument("--compound-runner", type=Path, required=True)
    parser.add_argument("--gpu-runner", type=Path, required=True)
    parser.add_argument("--topology-runner", type=Path, required=True)
    parser.add_argument("--billiards", type=Path, required=True)
    parser.add_argument("--build-dir", type=Path, required=True)
    parser.add_argument("--output-root", type=Path, required=True)
    args = parser.parse_args()
    if sha(GATE) != GATE_SHA256:
        raise SystemExit("v4 correction design gate drift")
    if sha(FROZEN_FAILURE_RECEIPT) != FROZEN_FAILURE_RECEIPT_SHA256:
        raise SystemExit("frozen v3 failure receipt drift")
    for path in (args.compound_runner, args.gpu_runner, args.topology_runner,
                 args.billiards):
        if not path.is_file():
            raise SystemExit(f"required binary missing: {path}")
    self_test = run_json([str(args.compound_runner.resolve()),
                          "--contract-self-test"])
    if self_test.get("status") != "OFFLINE_CONTRACT_SELF_TEST_PASS" or \
            self_test.get("actual_gpu_capture_count") != 0:
        raise SystemExit("compound runner offline contract self-test failed")
    build = args.build_dir.resolve()
    bindings = source_binding_manifest(build, args.gpu_runner.resolve(),
                                       args.billiards.resolve())
    source_sha = bindings["aggregate_sha256"]
    if source_sha == "9854b1605733c042e158ce51fc9bf9113684a738b8fc253960e9398736b5a397":
        raise SystemExit("v4 source identity illegally equals v3")
    design = json.loads(DESIGN_GATE.read_text(encoding="utf-8"))
    v3 = json.loads(V3_FIXTURES.read_text(encoding="utf-8"))
    output = args.output_root.resolve()
    if output.exists():
        raise SystemExit("v3 ready root must be new and immutable")
    output.mkdir(parents=True)
    source_bindings_path = output / "source-bindings-v3.json"
    write_json(source_bindings_path, bindings)
    runner_source = build / "generated/phase4_overlay_v4_gpu_capture_runner.mm"
    fixture_path = output / "fixtures-v4.json"
    fixtures = fixture_index(v3, args.billiards.resolve(), source_sha,
                             source_bindings_path,
                             sha(source_bindings_path))
    write_json(fixture_path, fixtures)
    gate = json.loads(GATE.read_text(encoding="utf-8"))
    frozen_constraints = {
        "correction_design_gate_sha256": GATE_SHA256,
        "source_probe_count_floor": 159,
        "aggregate_visible_probe_coverage_min": 0.9,
        "predicted_visible_probe_coverage_min": 0.9,
        "actual_visible_probe_coverage_min": 0.9,
        "minimum_local_contrast_ratio": 4.0,
        "contrast_4_5_target_coverage_min": 0.99,
        "predicted_screen_offset_px": 0.0,
        "actual_screen_offset_px": 0.0,
        "maximum_screen_offset_px": 0.0,
        "double_assignment_count_required": 0,
        "foreign_track_evidence_count_required": 0,
        "unowned_evidence_count_required": 0,
    }
    separation = gate["v4_authorized_change_scope"][
        "screen_space_separation"]
    machine = gate["machine_gates_unchanged_or_strengthened"]
    denominator = gate["compound_glyph_visibility_machine_semantics"][
        "denominator_contract"]
    if separation["authorized"] is not False or \
            separation["predicted_offset_px"] != 0.0 or \
            separation["actual_offset_px"] != 0.0 or \
            separation["maximum_allowed_by_this_gate_px"] != 0.0:
        raise SystemExit("v4 screen-space offset gate drift")
    if denominator["exact_failure_case_source_probe_count_min"] != 159 or \
            machine["aggregate_visible_probe_coverage_min"] != 0.9 or \
            machine["predicted_visible_probe_coverage_min"] != 0.9 or \
            machine["actual_visible_probe_coverage_min"] != 0.9 or \
            machine["minimum_local_contrast_ratio"] != 4.0 or \
            machine["contrast_4_5_target_coverage_min"] != 0.99:
        raise SystemExit("v4 frozen machine threshold drift")
    targeted = targeted_manifest(fixtures, source_sha,
        args.gpu_runner.resolve(), args.topology_runner.resolve(),
        fixture_path, args.billiards.resolve(), source_bindings_path,
        runner_source, frozen_constraints)
    write_json(output / "targeted-preflight-ready.json", targeted)
    formal = formal_manifest(design, fixtures, source_sha,
        args.gpu_runner.resolve(), args.billiards.resolve(), fixture_path,
        source_bindings_path, runner_source, frozen_constraints)
    write_json(output / "formal68-ready-template.json", formal)
    receipt = {
        "schema": "billiardgl.phase4.overlay_v4_offline_preparation_receipt.v1",
        "status": "OFFLINE_IMPLEMENTATION_VALIDATED_READY_TEMPLATES_NOT_RUN",
        "source_sha256": source_sha,
        "source_binding_manifest_path": str(source_bindings_path.resolve()),
        "source_binding_manifest_sha256": sha(source_bindings_path),
        "source_aggregate_algorithm": bindings["aggregate_algorithm"],
        "source_binding_file_count": bindings["file_count"],
        "billiards_executable_path": str(args.billiards.resolve()),
        "billiards_executable_sha256": sha(args.billiards),
        "runner_source_path": str(runner_source.resolve()),
        "runner_source_sha256": sha(runner_source),
        "compound_runner_self_test": self_test,
        "gpu_runner_path": str(args.gpu_runner.resolve()),
        "gpu_runner_sha256": sha(args.gpu_runner),
        "targeted_ready_sha256": sha(output / "targeted-preflight-ready.json"),
        "formal68_ready_template_sha256": sha(output / "formal68-ready-template.json"),
        "fixture_manifest_sha256": sha(fixture_path),
        "actual_gpu_capture_count": 0,
        "gpu_capture_authorized": False,
        "formal68_authorized": False,
        "technical_art_review_authorized": False,
        "m3_authorized": False,
        "frozen_gate_constraints": frozen_constraints,
        "evidence_claims": {
            "actual_gpu_capture": False,
            "machine_capture": False,
            "technical_art_review": False,
            "manual_review": False,
            "m3": False,
            "formal68": False,
            "golden": False,
        },
    }
    write_json(output / "offline-preparation-receipt.json", receipt)
    print(json.dumps(receipt, sort_keys=True))


if __name__ == "__main__":
    main()
