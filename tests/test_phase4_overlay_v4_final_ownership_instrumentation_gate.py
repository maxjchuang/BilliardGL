#!/usr/bin/env python3
"""Positive and fail-closed mutation tests for the Phase 4 v4 design gate."""

from __future__ import annotations

import copy
import hashlib
import importlib.util
import json
import pathlib
import subprocess
import tempfile


ROOT = pathlib.Path(__file__).resolve().parents[1]
GATE = ROOT / "docs/art/phase4-overlay-v4-final-ownership-instrumentation-implementation-design-gate-v1.json"
VALIDATOR = ROOT / "scripts/validate_phase4_overlay_v4_final_ownership_instrumentation_gate.py"
EXPECTED_GATE_SHA = "c3b6f032deaac6bd39d32755d4ab423915e8b70b08b6da18626707b8d7134056"


def sha(path: pathlib.Path) -> str:
    return hashlib.sha256(path.read_bytes()).hexdigest()


def load_validator():
    spec = importlib.util.spec_from_file_location("phase4_v4_gate_validator", VALIDATOR)
    module = importlib.util.module_from_spec(spec)
    assert spec.loader is not None
    spec.loader.exec_module(module)
    return module


def expect_rejected(validator, gate, mutation_id: str) -> None:
    try:
        validator.validate(gate)
    except validator.GateValidationError:
        return
    raise AssertionError(f"fail-closed mutation accepted:{mutation_id}")


def main() -> int:
    validator = load_validator()
    gate = json.loads(GATE.read_text())
    result = validator.validate(gate)
    assert result["implementation_steps"] == [
        "A_FAILURE_TELEMETRY_PRESERVATION",
        "B_INTEGRATED_FINAL_SEMANTIC_OWNERSHIP",
    ]
    assert result["cpu_fixture_count"] == 12
    assert result["exit_stage_count"] == 6
    assert result["gpu_capture_authorized"] is False
    assert result["implementation_performed"] is False
    assert sha(GATE) == EXPECTED_GATE_SHA

    mutations = []

    changed = copy.deepcopy(gate)
    changed["authorization_boundary"]["gpu_capture_authorized"] = True
    mutations.append(("gpu_authorized", changed))

    changed = copy.deepcopy(gate)
    changed["authorization_boundary"]["implementation_performed_by_this_gate"] = True
    mutations.append(("implementation_claimed", changed))

    changed = copy.deepcopy(gate)
    changed["implementation_sequence"].reverse()
    mutations.append(("stage_order_reversed", changed))

    changed = copy.deepcopy(gate)
    changed["implementation_sequence"][0]["required_failure_row_fields"].remove(
        "ownership_candidate_records")
    mutations.append(("stage_a_field_removed", changed))

    changed = copy.deepcopy(gate)
    changed["implementation_sequence"][0]["mandatory_control_flow"][
        "serialize_before_gate"] = "Serialize only after machinePass."
    mutations.append(("serialize_after_machine_gate", changed))

    changed = copy.deepcopy(gate)
    del changed["implementation_sequence"][1]["integration_contract"]["same_msaa"]
    mutations.append(("msaa_contract_removed", changed))

    changed = copy.deepcopy(gate)
    changed["implementation_sequence"][1]["depends_on"] = None
    mutations.append(("stage_b_dependency_removed", changed))

    changed = copy.deepcopy(gate)
    changed["immutable_invariants"]["aggregate_visible_probe_coverage_min"] = 0.89
    mutations.append(("coverage_relaxed", changed))

    changed = copy.deepcopy(gate)
    changed["immutable_invariants"]["ordinal1_source_probe_denominator"] = 94
    mutations.append(("denominator_reduced", changed))

    changed = copy.deepcopy(gate)
    changed["immutable_invariants"]["actual_lateral_offset_px"] = 1.0
    mutations.append(("offset_changed", changed))

    changed = copy.deepcopy(gate)
    changed["required_cpu_negative_and_positive_fixtures"].pop()
    mutations.append(("negative_fixture_removed", changed))

    changed = copy.deepcopy(gate)
    exits = changed["required_exit_sequence_before_any_fresh_targeted_readiness"]
    exits[2], exits[5] = exits[5], exits[2]
    mutations.append(("readiness_before_cpu_fixtures", changed))

    changed = copy.deepcopy(gate)
    changed["required_exit_sequence_before_any_fresh_targeted_readiness"][3][
        "gpu_allowed"] = True
    mutations.append(("gpu_in_exit_stage", changed))

    changed = copy.deepcopy(gate)
    changed["input_bindings"]["offline_diagnostic_manifest"]["sha256"] = "0" * 64
    mutations.append(("diagnostic_binding_changed", changed))

    changed = copy.deepcopy(gate)
    changed["frozen_diagnostic_decision"]["root_cause_isolated"] = True
    mutations.append(("root_cause_overclaimed", changed))

    for mutation_id, mutation in mutations:
        expect_rejected(validator, mutation, mutation_id)

    process = subprocess.run(
        ["python3", str(VALIDATOR), "--gate", str(GATE),
         "--expect-sha", EXPECTED_GATE_SHA],
        cwd=ROOT, capture_output=True, text=True)
    assert process.returncode == 0, process.stderr
    assert "PHASE4_V4_FINAL_OWNERSHIP_INSTRUMENTATION_GATE_VALIDATION_PASS" \
        in process.stdout

    with tempfile.TemporaryDirectory(prefix="phase4-v4-gate-test-") as directory:
        tampered = pathlib.Path(directory) / "gate.json"
        bad = copy.deepcopy(gate)
        bad["future_capture_boundary"][
            "fresh_targeted_readiness_is_not_capture_authorization"] = False
        tampered.write_text(json.dumps(bad))
        process = subprocess.run(
            ["python3", str(VALIDATOR), "--gate", str(tampered)],
            cwd=ROOT, capture_output=True, text=True)
        assert process.returncode != 0
        assert "gate_future_capture_boundary_invalid" in process.stderr

    # The validator binds these implementation inputs but never edits them.
    for _, (path_string, expected) in validator.EXPECTED_BINDINGS.items():
        assert sha(ROOT / path_string) == expected

    print("PHASE4_V4_FINAL_OWNERSHIP_INSTRUMENTATION_GATE_TEST_PASS")
    print(f"fail_closed_mutations={len(mutations) + 1}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
