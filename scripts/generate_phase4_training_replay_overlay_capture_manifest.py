#!/usr/bin/env python3
"""Generate the pending, hash-bound 68-capture Phase 4 matrix.

This generator creates selectors only. It intentionally cannot mark a capture
as GPU/TA/manual PASS and contains no human-study record fields.
"""

import argparse
import hashlib
import json
from pathlib import Path

GATE_SHA256 = "55b9caa8924ef8d9f3c722e90c8189064a320c8ef53c9199c191b4a0f2722d22"
CORRECTION_GATE_SHA256 = \
    "9cba251af89c6a0b6724966f983be8ad68b5c94ad0d3d902c840e2275bcf28bb"
CORRECTION_GATE_PATH = Path(__file__).resolve().parents[1] / \
    "docs/art/phase4-overlay-v3-correction-design-gate-v1.json"


def digest(path: Path) -> str:
    return hashlib.sha256(path.read_bytes()).hexdigest()


def normalized_sha(value: str, field: str) -> str:
    if not isinstance(value, str):
        raise RuntimeError(f"{field} SHA-256 is missing")
    result = value.removeprefix("sha256:")
    if len(result) != 64 or any(c not in "0123456789abcdef" for c in result):
        raise RuntimeError(f"{field} SHA-256 is invalid")
    return result


def pending_capture(capture_id, scenario, camera, profile, state):
    return {
        "capture_id": capture_id,
        "scenario_id": scenario["id"],
        "scenario_sha256": scenario["sha256"],
        "camera_preset": camera,
        "physical_viewport_px": profile["physical_viewport_px"],
        "logical_viewport_px": profile.get("logical_viewport_px"),
        "device_scale": profile["device_scale"],
        "palette": profile["palette"],
        "ui_scale": profile["ui_scale"],
        "overlay_state": state,
        "actual_gpu_required": True,
        "replay_fixture_path": None,
        "replay_fixture_sha256": None,
        "build_sha256": None,
        "executable_sha256": None,
        "asset_manifest_sha256": None,
        "physics_profile_sha256": None,
        "png_path": None,
        "png_sha256": None,
        "status": "NOT_RUN",
    }


def generate(gate):
    matrix = gate["actual_gpu_evidence_matrix"]
    scenarios = gate["scenario_contract"]
    core = matrix["core_minimum"]
    records = []
    for scenario in scenarios:
        for camera in core["camera_presets"]:
            records.append(pending_capture(
                f"core__{scenario['id']}__{camera}", scenario, camera,
                core["common_profile"], "comparison_ready_collapsed"))
    by_id = {scenario["id"]: scenario for scenario in scenarios}
    extension = matrix["representative_extension"]
    for scenario_id in extension["category_representatives"]:
        for profile in extension["profiles_per_representative"]:
            records.append(pending_capture(
                f"extension__{scenario_id}__{profile['id']}",
                by_id[scenario_id], profile["camera"], profile,
                profile["overlay_state"]))
    assert len(records) == 68
    assert len({record["capture_id"] for record in records}) == 68
    return {
        "schema": "billiardgl.phase4.training_replay_overlay_gpu_capture_manifest.v1",
        "status": "IMPLEMENTATION_PREFLIGHT_ONLY",
        "design_gate_sha256": GATE_SHA256,
        "capture_count": len(records),
        "actual_gpu_capture_count": 0,
        "ta_pass_count": 0,
        "manual_pass_count": 0,
        "external_real_human_records": [],
        "captures": records,
    }


def validate_fixture_manifest(gate, fixture_path: Path):
    fixtures = json.loads(fixture_path.read_text(encoding="utf-8"))
    if fixtures.get("fixture_count") != 12 or len(fixtures.get("fixtures", [])) != 12:
        raise RuntimeError("exact 12-fixture manifest required")
    if fixtures.get("status") != "FIXTURE_VALIDATED_NOT_GPU_EVIDENCE":
        raise RuntimeError("fixture manifest status is not validation-only")
    pack_path = Path(fixtures.get("training_pack_path", ""))
    if not pack_path.is_file() or digest(pack_path) != normalized_sha(
            fixtures.get("training_pack_sha256"), "training pack"):
        raise RuntimeError("training pack binding drift")
    executable_sha = normalized_sha(fixtures.get("executable_sha256"),
                                    "executable")
    source_sha = normalized_sha(fixtures.get("source_sha256"), "source")
    expected = {row["id"]: normalized_sha(row["sha256"], row["id"])
                for row in gate["scenario_contract"]}
    pack = json.loads(pack_path.read_text(encoding="utf-8"))
    pack_entries = {row["id"]: row for row in pack.get("scenarios", [])}
    if len(pack_entries) != 12 or set(pack_entries) != set(expected):
        raise RuntimeError("training pack scenario coverage drift")
    by_scenario = {}
    for fixture in fixtures["fixtures"]:
        scenario_id = fixture.get("scenario_id")
        if scenario_id in by_scenario or scenario_id not in expected:
            raise RuntimeError(f"fixture scenario set invalid: {scenario_id}")
        replay_path = Path(fixture.get("replay_path", ""))
        if not replay_path.is_file() or digest(replay_path) != normalized_sha(
                fixture.get("replay_sha256"), f"{scenario_id} replay"):
            raise RuntimeError(f"replay fixture drift: {scenario_id}")
        if normalized_sha(fixture.get("scenario_sha256"), scenario_id) != \
                expected[scenario_id] or normalized_sha(
                    fixture.get("content_sha256"), scenario_id) != expected[scenario_id]:
            raise RuntimeError(f"fixture scenario/content drift: {scenario_id}")
        scenario_path = Path(fixture.get("scenario_path", ""))
        if not scenario_path.is_file() or digest(scenario_path) != \
                expected[scenario_id] or normalized_sha(
                    pack_entries[scenario_id].get("sha256"), scenario_id) != \
                expected[scenario_id] or scenario_path.resolve() != \
                (pack_path.parent / pack_entries[scenario_id]["file"]).resolve():
            raise RuntimeError(f"scenario file binding drift: {scenario_id}")
        if normalized_sha(fixture.get("executable_sha256"), scenario_id) != \
                executable_sha:
            raise RuntimeError(f"fixture executable drift: {scenario_id}")
        normalized_sha(fixture.get("physics_profile_sha256"),
                       f"{scenario_id} physics profile")
        if not fixture.get("executable_version") or not fixture.get(
                "physics_profile_id"):
            raise RuntimeError(f"fixture metadata missing: {scenario_id}")
        by_scenario[scenario_id] = fixture
    if set(by_scenario) != set(expected):
        raise RuntimeError("fixture scenario coverage is not exact launch 12")
    return fixtures, by_scenario, source_sha, executable_sha


def terminal_outcome(gate, row):
    state = row["overlay_state"]
    if state in {"predicted_only_active_collapsed",
                 "actual_replay_paused_collapsed"}:
        return "not_terminal"
    representatives = gate["actual_gpu_evidence_matrix"][
        "representative_extension"]["category_representatives"]
    if row["scenario_id"] in representatives:
        # Across the five representative scenarios: three success and two
        # failure terminal fixtures, satisfying the design-gate minimum.
        return "failure" if representatives.index(row["scenario_id"]) in (2, 3) \
            else "success"
    scenario_ids = [value["id"] for value in gate["scenario_contract"]]
    return "success" if scenario_ids.index(row["scenario_id"]) % 2 == 0 \
        else "failure"


def bind(gate, fixtures_path: Path, runner_path: Path,
         capture_output_dir: Path):
    if not CORRECTION_GATE_PATH.is_file() or \
            digest(CORRECTION_GATE_PATH) != CORRECTION_GATE_SHA256:
        raise RuntimeError("v3 correction gate binding drift")
    if not runner_path.is_file():
        raise RuntimeError("production GPU runner is missing")
    fixtures, by_scenario, source_sha, executable_sha = \
        validate_fixture_manifest(gate, fixtures_path)
    result = generate(gate)
    capture_output_dir = capture_output_dir.resolve()
    for row in result["captures"]:
        fixture = by_scenario[row["scenario_id"]]
        row["scenario_sha256"] = normalized_sha(
            row["scenario_sha256"], row["scenario_id"])
        row["replay_fixture_path"] = str(Path(
            fixture["replay_path"]).resolve())
        row["replay_fixture_sha256"] = normalized_sha(
            fixture["replay_sha256"], row["scenario_id"])
        row["build_sha256"] = source_sha
        row["executable_version"] = fixture["executable_version"]
        row["executable_sha256"] = executable_sha
        row["content_sha256"] = normalized_sha(
            fixture["content_sha256"], row["scenario_id"])
        row["asset_manifest_sha256"] = normalized_sha(
            fixtures["training_pack_sha256"], "training pack")
        row["physics_profile_id"] = fixture["physics_profile_id"]
        row["physics_profile_sha256"] = normalized_sha(
            fixture["physics_profile_sha256"], row["scenario_id"])
        row["runner_sha256"] = digest(runner_path)
        row["terminal_outcome"] = terminal_outcome(gate, row)
        row["png_path"] = str(capture_output_dir /
                              f"{row['capture_id']}.png")
        row["status"] = "READY_FOR_ACTUAL_GPU_CAPTURE_NOT_RUN"
    result.update({
        "schema": "billiardgl.phase4.training_replay_overlay_gpu_capture_manifest.v3",
        "status": "HASH_BOUND_READY_NOT_RUN",
        "fixture_manifest_path": str(fixtures_path.resolve()),
        "fixture_manifest_sha256": digest(fixtures_path),
        "runner_path": str(runner_path.resolve()),
        "runner_sha256": digest(runner_path),
        "build_sha256": source_sha,
        "executable_sha256": executable_sha,
        "asset_manifest_sha256": normalized_sha(
            fixtures["training_pack_sha256"], "training pack"),
        "correction_design_gate_path": str(CORRECTION_GATE_PATH),
        "correction_design_gate_sha256": CORRECTION_GATE_SHA256,
    })
    return result


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--gate", type=Path, required=True)
    parser.add_argument("--output", type=Path, required=True)
    parser.add_argument("--fixtures", type=Path)
    parser.add_argument("--runner", type=Path)
    parser.add_argument("--capture-output-dir", type=Path)
    args = parser.parse_args()
    if digest(args.gate) != GATE_SHA256:
        raise SystemExit("design gate SHA-256 mismatch; refusing generation")
    gate = json.loads(args.gate.read_text(encoding="utf-8"))
    binding_args = (args.fixtures, args.runner, args.capture_output_dir)
    if any(binding_args) and not all(binding_args):
        raise SystemExit("--fixtures, --runner and --capture-output-dir are required together")
    result = bind(gate, args.fixtures, args.runner,
                  args.capture_output_dir) if args.fixtures else generate(gate)
    args.output.parent.mkdir(parents=True, exist_ok=True)
    args.output.write_text(json.dumps(result, indent=2) + "\n", encoding="utf-8")
    print(f"generated {len(result['captures'])} " +
          ("hash-bound selectors; actual GPU evidence remains NOT_RUN" if
           args.fixtures else "pending selectors; no GPU evidence claimed"))


if __name__ == "__main__":
    main()
