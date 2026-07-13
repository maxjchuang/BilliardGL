# Cross 2023 Cue-Impact Reference Implementation Plan

> **Required subskill:** Use `superpowers:test-driven-development` for every implementation task and `superpowers:verification-before-completion` before committing the implementation.

**Goal:** Preserve the complete experimental speed/spin-versus-vertical-impact dataset from Cross and connect every legally admitted point either to a production cue-shot replay or to a precise, strictly accounted input-capability limitation.

**Architecture:** Build the source package independently of runtime support, then introduce canonical scenario v2 as an input contract for cue-impact conditions. Scenario v2 validates and traces cue speed, mass, direction, elevation, and tip offset, but this phase does not add a cue-contact physics formula. Points whose physical inputs cannot be applied by the current production shot path remain `REFERENCE_LIMITATION`; accepted inputs that yield missing/zero spin become `MODEL_MISMATCH`.

**Tech Stack:** Python 3 standard library, existing reference framework, C++ canonical scenario/automation protocol, `GameRuntime`, JSONL traces, `unittest`, CTest.

## Global Constraints

- Implement after the other three source plans so common package, adapter, uncertainty, and angular-velocity metrics are available.
- Do not derive expected values from the paper's simple impact model. Only experimental markers/tables enter normalized acceptance data.
- Do not add or tune cue-ball impulse, normal/tangential force, friction, spin, miscues, or stick-slip formulas in phase 2.
- Do not convert physical cue speed to the game's power control by fitting against the same experimental output. A mapping needs independent mechanical evidence; otherwise it is a limitation.
- Full-text access is currently restricted on the publisher page. No numeric point may be admitted from the abstract, a citation snippet, or an earlier Cross paper. Obtain the 2023/2025 article lawfully and audit its exact license before extraction.
- Commit all extracted experiment values, digitization coordinates, input conditions, uncertainties, split assignments, and hashes even when every runtime point is limitation-only.
- Keep this article distinct from Cross 2008 “Cue and ball deflection.” The older work may be a supplemental package but cannot silently fill missing 2023 conditions.
- Freeze complete impact-offset series into approximately one-third calibration and two-thirds holdout before running the game. Center/low offsets may calibrate; maximum topspin, maximum backspin, and grip/slip boundary offsets stay holdout.

## Source Admission Record

- Citation: Rod Cross, “Impact of a cue with a billiard ball,” *Proceedings of the Institution of Mechanical Engineers, Part P: Journal of Sports Engineering and Technology* 239(4), 647–651, first published online 29 June 2023, issue published December 2025, DOI `10.1177/17543371231184011`.
- Publisher URL: `https://journals.sagepub.com/doi/abs/10.1177/17543371231184011`; the current page offers purchase/institutional access and permission requests, not open full text.
- Abstract-supported scope only: experimental ball speed and spin as cue impact point varies above/below ball center; comparison with a simple impact model to estimate normal/tangential forces.
- Publisher keywords: center of percussion, coefficient of friction, grip, slip, backspin, topspin, impact model.
- Admission gate: secure the version-of-record or an author manuscript through lawful institutional/author access; record source SHA-256, exact pages/figures/tables, apparatus, cue/ball dimensions and mass, cue speed/control, impact offsets, camera/sensor method, uncertainty, and license. If any required field is absent, commit extracted outputs but block scenario adaptation with a field-specific limitation.

## File Structure

**Create:**

- `tools/physics_validation/adapters/cross_2023.py`
- `tools/physics_validation/extract_cross_2023.py`
- `tests/physics_validation/test_cross_2023_extraction.py`
- `tests/physics_validation/test_cross_2023_adapter.py`
- `tests/physics_validation/test_cross_2023_metrics.py`
- `tests/physics_validation/reference_data/cross_2023_cue_impact/manifest.json`
- `tests/physics_validation/reference_data/cross_2023_cue_impact/raw_extracted.csv`
- `tests/physics_validation/reference_data/cross_2023_cue_impact/digitization.csv`
- `tests/physics_validation/reference_data/cross_2023_cue_impact/normalized.csv`
- `tests/physics_validation/reference_data/cross_2023_cue_impact/extraction.json`
- `tests/physics_validation/reference_data/cross_2023_cue_impact/split.json`
- `tests/physics_validation/reference_data/cross_2023_cue_impact/scenario_template.json`
- `tests/physics_validation/reference_data/cross_2023_cue_impact/expected_model_mismatches.json`
- `tests/physics_validation/reference_data/cross_2023_cue_impact/expected_reference_limitations.json`
- `tests/physics_validation/scenarios/cue_impact_v2_contract.json`

**Modify:**

- `src/Billiards/physics_scenario.h`
- `src/Billiards/physics_scenario.cpp`
- `src/Billiards/automation_protocol.cpp`
- `src/Billiards/game_runtime.h`
- `src/Billiards/game_runtime.cpp`
- `tests/physics_scenario_tests.cpp`
- `tests/automation_e2e_tests.cpp`
- `tools/physics_validation/reference_adapter.py`
- `tools/physics_validation/analyzer.py`
- `tests/physics_validation/test_analyzer.py`
- `docs/automation-protocol.md`
- `docs/physics-validation.md`
- `docs/reference-data-packages.md`
- `.github/workflows/ci.yml`

## Task 1: Acquire and Inventory the Actual Experiment

**Files:** Create manifest/raw/digitization/limitation files and extraction tests.

1. Write a failing test requiring dataset `cross_2023_cue_impact`, version `1.0.0`, adapter `cross_2023_v1`, exact citation dates/pages, source/version/license status, acquisition URL, SHA-256, and reviewer record.
2. Require an evidence inventory for every experimental figure/table: page/panel, axes, marker series, counted markers, input variables, observed variables, apparatus, and experiment-versus-model classification.
3. Obtain and inspect the lawful full text. Record all fields named in the admission gate; do not enter values until experiment markers and simple-model lines can be distinguished unambiguously.
4. Directly transcribe tables. Digitize experimental markers twice with axis anchors and residuals. Store model curves only as excluded-evidence metadata.
5. Lock the full experimental marker count per series in the manifest. Reject raw count mismatch and any normalized row whose evidence ID is classified as model output.
6. Register one limitation per missing input field, using codes such as `cue_speed_unavailable`, `cue_mass_unavailable`, `tip_offset_definition_unavailable`, `ball_radius_unavailable`, or `measurement_phase_unavailable`; each entry names affected point IDs and release evidence.
7. Run `python3 -m unittest tests.physics_validation.test_cross_2023_extraction -v`; expect admission assertions to pass only after the full-text inventory is complete.

## Task 2: Normalize and Pre-Register the Full Numeric Dataset

**Files:** Create extractor, normalized/extraction/split files; extend tests.

1. Add exact-byte tests for:

```python
def normalize_rows(raw_path: Path, digitization_path: Path,
                   extraction_path: Path) -> tuple[dict, ...]: ...

def write_normalized(package_path: Path) -> bytes: ...
```

2. Normalize tip offset to both physical cm and dimensionless `offset/radius`, ball speed to cm/s, angular speed to rad/s, cue speed to cm/s, cue/ball mass to kg, and angles to degrees. Preserve source sign: above center is positive topspin and below center negative backspin unless the article explicitly defines otherwise.
3. Propagate stated measurement, digitization, and conversion uncertainty separately. Do not use model residual as experimental measurement error.
4. Group complete offset sweeps by cue condition. Assign center and representative moderate positive/negative sweeps to calibration; assign largest positive/negative offsets and any grip/slip-transition sweep to holdout. Do not split adjacent points from one sweep.
5. Include all numerical points even if inputs are currently unexpressible; applicability/limitation is metadata, not a reason to drop data.
6. Generate sorted bytes and extraction hashes, then run:

```bash
python3 -m tools.physics_validation.extract_cross_2023 \
  --package tests/physics_validation/reference_data/cross_2023_cue_impact --check
```

Expected: exit 0 and byte-identical full numeric data.

## Task 3: Define Canonical Cue-Impact Scenario v2

**Files:** Modify scenario headers/parser/tests; create the contract scenario and update protocol docs.

1. Write failing C++ parser tests for schema v2 with a required `cue_impact` object:

```json
{
  "cue_ball_index": 0,
  "cue_speed_cm_s": 100.0,
  "cue_mass_kg": 0.5,
  "direction": [1.0, 0.0, 0.0],
  "elevation_degrees": 0.0,
  "tip_offset_cm": [0.0, 1.0],
  "tip_offset_radius": [0.0, 0.35],
  "chalk_state": "SOURCE_DECLARED"
}
```

2. Validate finite/ranged values, unit direction, one cue ball, mutually consistent physical/dimensionless offsets, and contact within ball radius. Schema v1 behavior must remain byte-compatible.
3. Add a `CueImpactInput` value object to `PhysicsScenario` and runtime state. Loading a scenario stores the exact requested input and exposes it in state/trace; it does not synthesize an outcome.
4. Advertise `physics_scenario_v2_cue_input` only when parsing/state tracing is available. Do not advertise physical offset support merely because the input can be stored.
5. Atomically reject malformed v2 scenarios without changing runtime state. Test headless process load/state/trace round-trip through the automation protocol.
6. Run `cmake --build build/check --target physics_scenario_tests automation_e2e_tests && ctest --test-dir build/check -R 'physics_scenario|automation_e2e' --output-on-failure`; expect all tests to pass.

## Task 4: Route Supported Inputs Through the Existing Shot Path

**Files:** Modify runtime/E2E tests and docs without adding contact physics.

1. Write tests enumerating which `CueImpactInput` fields the current production shot path can consume exactly. Return structured unsupported-field results for the rest.
2. If source cue speed has an independently documented exact mapping to existing shot power, invoke the normal user `shoot` path and trace both requested physical input and applied user controls. If no independent mapping exists, report `cue_speed_to_power_mapping_missing` and do not fire a surrogate shot.
3. If vertical tip offset is not consumed by current production physics, report `vertical_tip_offset_not_modeled`; do not silently center-hit the ball. If a zero-offset point is otherwise fully expressible, allow that case through the normal shot path.
4. Keep user-visible shot controls and automation behavior unchanged for schema v1.
5. Test that unsupported inputs cause reference limitations, while an accepted input that executes and yields finite but incorrect spin is eligible for model-mismatch classification.

## Task 5: Add Cue-Impact Output Metrics

**Files:** Modify analyzer/tests and create `test_cross_2023_metrics.py`.

1. Add synthetic traces for `cue_impact_linear_speed_cm_s`, `cue_impact_angular_speed_rad_s`, and `stick_slip_classification` sampled at the first stable frames after `cue_impact`/shot event.
2. Require expectation metadata to declare cue ball, angular axis/sign, sampling window, and source phase. Compute speed magnitude and signed angular component without fitting to expected values.
3. Classify absent requested-input trace as `INTEGRATION_MISMATCH`, unsupported physical input as `REFERENCE_LIMITATION`, finite missing modeled response such as zero spin as `MODEL_MISMATCH`, and non-finite state as `NUMERICAL_FAILURE`.
4. Run `python3 -m unittest tests.physics_validation.test_cross_2023_metrics tests.physics_validation.test_analyzer -v`; expect all tests to pass.

## Task 6: Adapt Every Point or Account for It

**Files:** Create adapter/template/tests; modify registry and strict manifests.

1. Write failing tests requiring every normalized point ID to appear exactly once in an emitted `ReferenceCase` or registered limitation; duplicates and silent omissions fail.
2. Implement:

```python
def adapt_cross_2023(package: ReferencePackage,
                     split: ReferenceSplit,
                     points: tuple[ReferencePoint, ...]) -> tuple[ReferenceCase, ...]: ...
```

3. Build schema v2 cases only when all source input fields and exact runtime mappings exist. Preserve cue/ball apparatus, offset, phase, partition, and source location in canonical provenance.
4. Emit field-specific limitations for remaining points. Do not replace them with preloaded ball velocity/angular velocity because that bypasses cue impact.
5. Register `cross_2023_v1`, verify deterministic output, and prove all earlier adapters remain byte-identical.
6. Run the package through production; lock exact finite failures in `expected_model_mismatches.json` and exact unexpressible inputs in `expected_reference_limitations.json` without widening data intervals.

## Task 7: Report, Replay, Document, and Verify

**Files:** Modify docs and CI.

1. Reports must show tip offset (cm and radius fraction), cue condition, requested/applied input, predicted/experimental speed and spin, partition, source panel, limitation/mismatch, and replay command.
2. Add offline extraction/package/all-point-accounting tests to PR-fast CI. Run one executable zero-offset case only if independently expressible; otherwise assert its exact limitation tuple.
3. Document full extraction, scenario v2 contract, capability distinction, full regression, and single-point replay.
4. Run:

```bash
python3 -m unittest discover -s tests/physics_validation -p 'test_*.py' -v
ctest --test-dir build/check --output-on-failure
git diff --exit-code -- tests/physics_validation/reference_data/cross_2023_cue_impact/normalized.csv
```

Expected: all data reproduce offline, every point is adapted or strictly limited, no undeclared failure exists, and no cue-contact physics formula changed.

## Plan Self-Review Checklist

- No number is admitted from the abstract or an earlier paper.
- The complete experimental dataset is committed even if runtime mapping is unavailable.
- Scenario v2 separates input representation/tracing from physical support.
- Unsupported offset/speed inputs cannot be silently converted into center-hit/preloaded-ball cases.
- Experimental markers and simple-model curves remain structurally separate.
- Every normalized point is adapted or explicitly limited; extremes remain holdout.
- No production cue-impact formula or parameter change is planned.
- A scan for unfinished drafting markers returns no matches.
