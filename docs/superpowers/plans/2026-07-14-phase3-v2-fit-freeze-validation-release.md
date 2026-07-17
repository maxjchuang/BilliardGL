# Phase 3 v2 Candidate Fit, Freeze, Validation, and Release Implementation Plan

**Execution status: Completed and archived.** Unchecked boxes below preserve the original execution script; current disposition and evidence are indexed in [Phase 3 current status](../../phase3-physics-current-status.md).

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Fit and select one integrated v2 candidate, freeze it reproducibly, execute independent confirmation exactly once, compare all v1/v2 evidence, and publish only if every gate passes.

**Architecture:** Fit surface, ball, and cushion parameters only from committed calibration/spent rows using deterministic objectives. Assemble cue/pocket bounded v1 components with solver v2 into a single production profile, freeze two clean-build digests, then open confirmation once and write immutable receipts/artifacts before generating comparison and release manifests.

**Tech Stack:** Python 3 numerical grid/least-squares code without new dependencies, C++17 profile runtime, CMake, JSON/CSV/Markdown, SHA-256, Git.

## Global Constraints

- Confirmation sources never enter candidate fit or formula selection.
- Candidate formula, metrics, intervals, splits, inputs, and budgets are committed before freeze.
- Each confirmation partition executes once for the frozen candidate; first results are immutable even on failure.
- A failure rejects the candidate ID; it does not trigger tolerance/formula edits under that ID.
- All numeric inputs/outputs and full traces are committed; source publications/images are not.
- Successful release status is exactly `PASSED`.
- Each task below ends in one independently reviewable commit.

---

### Task 1: Fit surface v2 with phase-correct spent data

**Files:**
- Create: `tools/physics_validation/fit_surface.py`
- Create: `tests/physics_validation/test_surface_fit_v2.py`
- Create: `physics_models/calibration/surface_fit_v2_inputs.csv`
- Create: `physics_models/calibration/surface_fit_v2.json`
- Create: `physics_models/calibration/surface_fit_v2_residuals.csv`

**Interfaces:**
- Produces: `fit_surface_parameters(points) -> SurfaceFit`, with `sliding_friction_coefficient`, `rolling_resistance_acceleration_cm_s2`, covariance/bounds, objective, and per-row normalized residual.
- Consumes: committed spent surface rows selected by contiguous `sliding` or `rolling` telemetry phase.

- [ ] **Step 1: Write failing deterministic fit and source-isolation tests**

```python
def test_surface_fit_uses_only_spent_calibration_rows(self):
    points = read_surface_inputs(INPUTS)
    self.assertTrue(points)
    self.assertEqual({point.lifecycle for point in points}, {"spent"})
    self.assertFalse(any("derby" in point.dataset_id for point in points))

def test_surface_fit_is_deterministic_and_reports_every_residual(self):
    first = fit_surface_parameters(read_surface_inputs(INPUTS))
    second = fit_surface_parameters(read_surface_inputs(INPUTS))
    self.assertEqual(first, second)
    self.assertEqual(len(first.residuals), len(read_surface_inputs(INPUTS)))
    self.assertAlmostEqual(first.sliding_acceleration_cm_s2, 196.13296508789062)
```

- [ ] **Step 2: Run tests and observe missing fitter/artifacts**

Run: `python3 -m unittest tests.physics_validation.test_surface_fit_v2 -v`

Expected: import/file failure because surface v2 fitter and inputs do not exist.

- [ ] **Step 3: Implement the exact two-parameter objective**

```python
@dataclass(frozen=True)
class SurfaceFit:
    sliding_friction_coefficient: float
    sliding_acceleration_cm_s2: float
    rolling_resistance_acceleration_cm_s2: float
    objective: float
    residuals: tuple
    parameter_bounds: dict


def fit_surface_parameters(points):
    sliding = [point for point in points if point.phase == "sliding"]
    rolling = [point for point in points if point.phase == "rolling"]
    sliding_a = weighted_mean(sliding)
    rolling_a = weighted_mean(rolling)
    mu = sliding_a / 980.665
    residuals = tuple(normalized_residual(point, sliding_a, rolling_a)
                      for point in points)
    objective = sum(value * value for value in residuals) / len(residuals)
    return SurfaceFit(mu, sliding_a, rolling_a, objective, residuals,
                      bounded_intervals(points, sliding_a, rolling_a))
```

Write canonical JSON with exact objective definition and bounded intervals; write one residual CSV row for every input ID. The equations remain rigid-sphere Coulomb sliding plus constant rolling resistance.

- [ ] **Step 4: Run fit tests and reproduce artifacts byte-for-byte**

Run: `python3 -m unittest tests.physics_validation.test_surface_fit_v2 tests.physics_validation.test_mathavan_2009_metrics -v && python3 -m tools.physics_validation.fit_surface --inputs physics_models/calibration/surface_fit_v2_inputs.csv --json /tmp/surface-v2.json --residuals /tmp/surface-v2.csv && cmp /tmp/surface-v2.json physics_models/calibration/surface_fit_v2.json && cmp /tmp/surface-v2.csv physics_models/calibration/surface_fit_v2_residuals.csv`

Expected: all tests pass and generated artifacts match committed bytes.

- [ ] **Step 5: Commit surface candidate fit**

```bash
git add tools/physics_validation/fit_surface.py tests/physics_validation/test_surface_fit_v2.py physics_models/calibration/surface_fit_v2_inputs.csv physics_models/calibration/surface_fit_v2.json physics_models/calibration/surface_fit_v2_residuals.csv
git commit -m "feat: fit phase-correct surface candidate v2"
```

### Task 2: Fit series-balanced ball-collision v2

**Files:**
- Modify: `tools/physics_validation/fit_ball_collision.py`
- Modify: `tests/physics_validation/test_ball_collision_fit.py`
- Create: `physics_models/calibration/ball_collision_fit_v2_inputs.csv`
- Create: `physics_models/calibration/ball_collision_fit_v2.json`
- Create: `physics_models/calibration/ball_collision_fit_v2_residuals.csv`

**Interfaces:**
- Produces: rigid impulse parameters `(normal_restitution, friction_coefficient)` selected by `(objective, e, mu)`.
- Consumes: spent Mathavan direct post-impact velocities and spent Doménech billiard series only; no confirmation rows.

- [ ] **Step 1: Write failing series-balance and tie-break tests**

```python
def test_dense_series_does_not_gain_point_count_weight(self):
    sparse = series("mathavan_velocity", [residual(1.0), residual(1.0)])
    dense = series("domenech_angle", [residual(3.0)] * 100)
    self.assertEqual(series_balanced_objective([sparse, dense]), 5.0)

def test_tie_break_is_objective_then_e_then_mu(self):
    fits = [Fit(1.0, 0.90, 0.05), Fit(1.0, 0.89, 0.06), Fit(1.0, 0.89, 0.04)]
    self.assertEqual(select_fit(fits), Fit(1.0, 0.89, 0.04))

def test_confirmation_sources_are_not_fit_inputs(self):
    ids = {row.dataset_id for row in read_impact_inputs(INPUTS)}
    self.assertFalse(ids & {"sudo_2002", "derby_fuller_1999"})
```

- [ ] **Step 2: Run fit tests and observe point-weighted objective**

Run: `python3 -m unittest tests.physics_validation.test_ball_collision_fit -v`

Expected: series-balance test fails because current `_objective` averages individual points.

- [ ] **Step 3: Implement uncertainty-normalized series means**

```python
def series_balanced_objective(residual_rows):
    grouped = {}
    for row in residual_rows:
        grouped.setdefault(row.series_id, []).append(row.normalized_residual ** 2)
    series_mse = {key: sum(values) / len(values)
                  for key, values in grouped.items()}
    return sum(series_mse.values()) / len(series_mse), series_mse


def select_fit(fits):
    return min(fits, key=lambda value: (
        value.objective, value.normal_restitution,
        value.friction_coefficient))
```

Retain translational/angular contact velocity, uniform sphere inertia, explicit stick/slip, and non-increasing total energy in `_collision_state`. Report each normalized residual, each series MSE, the balanced objective, grid bounds/steps, and selected regime.

- [ ] **Step 4: Run fit tests and reproduce committed reports**

Run: `python3 -m unittest tests.physics_validation.test_ball_collision_fit tests.physics_validation.test_domenech_2023_metrics tests.physics_validation.test_mathavan_2009_metrics -v && python3 -m tools.physics_validation.fit_ball_collision --inputs physics_models/calibration/ball_collision_fit_v2_inputs.csv --output /tmp/ball-v2.json --residuals /tmp/ball-v2.csv && cmp /tmp/ball-v2.json physics_models/calibration/ball_collision_fit_v2.json && cmp /tmp/ball-v2.csv physics_models/calibration/ball_collision_fit_v2_residuals.csv`

Expected: all tests pass and generated artifacts match committed bytes.

- [ ] **Step 5: Commit ball-collision candidate fit**

```bash
git add tools/physics_validation/fit_ball_collision.py tests/physics_validation/test_ball_collision_fit.py physics_models/calibration/ball_collision_fit_v2_inputs.csv physics_models/calibration/ball_collision_fit_v2.json physics_models/calibration/ball_collision_fit_v2_residuals.csv
git commit -m "feat: fit series-balanced ball collision v2"
```

### Task 3: Fit the minimal speed-dependent cushion v2 law

**Files:**
- Modify: `src/Billiards/physics_profile.h`
- Modify: `src/Billiards/physics_profile.cpp`
- Modify: `src/Billiards/cushion_contact.cpp`
- Modify: `tools/physics_validation/fit_cushion.py`
- Modify: `tests/physics_validation/test_cushion_fit.py`
- Modify: `tests/cushion_contact_tests.cpp`
- Create: `physics_models/calibration/cushion_fit_v2_inputs.csv`
- Create: `physics_models/calibration/cushion_fit_v2.json`
- Create: `physics_models/calibration/cushion_fit_v2_residuals.csv`

**Interfaces:**
- Produces: `e(v_n) = clamp(e_intercept - e_slope*v_n, e_min, e_max)` with deterministic fit key `(objective, intercept, slope, min, max)`.
- Consumes: spent Mathavan 2009/2010 cushion rows only; preserves nose height, contact arm, friction, rotational coupling, and rigid-domain warning.

- [ ] **Step 1: Write failing law constraint and source-isolation tests**

```python
def test_fitted_cushion_law_is_finite_bounded_and_nonincreasing(self):
    fit = fit_cushion_parameters(read_incident_inputs(INPUTS))
    self.assertGreaterEqual(fit.e_slope, 0.0)
    self.assertTrue(0.0 <= fit.e_min <= fit.e_max <= 1.0)
    values = [fit.restitution(speed) for speed in (0.0, 0.5, 1.8, 4.0)]
    self.assertEqual(values, sorted(values, reverse=True))

def test_sudo_confirmation_is_not_a_fit_input(self):
    self.assertNotIn("sudo_2002", {row.dataset_id for row in read_incident_inputs(INPUTS)})
```

```cpp
expectNear(cushionRestitution(profile.cushion, 50.0f),
           clamp(profile.cushion.restitutionIntercept -
                 profile.cushion.restitutionSlopePerMps * 0.5f,
                 profile.cushion.minimumRestitution,
                 profile.cushion.maximumRestitution), 1e-7f);
```

- [ ] **Step 2: Run Python/C++ tests and observe constant restitution**

Run: `python3 -m unittest tests.physics_validation.test_cushion_fit -v && cmake --build build --target cushion-contact-tests -j2 && ctest --test-dir build -R cushion-contact --output-on-failure`

Expected: failures because the fit/runtime expose one constant normal restitution.

- [ ] **Step 3: Implement bounded affine restitution and deterministic fit**

```python
def restitution(speed_m_s, intercept, slope, minimum, maximum):
    return min(maximum, max(minimum, intercept - slope * speed_m_s))


def fit_key(value):
    return (value.objective, value.e_intercept, value.e_slope,
            value.e_min, value.e_max)
```

Add four finite profile fields and validate `slope >= 0` plus `0 <= min <= max <= 1`. Search the committed grid using uncertainty-normalized residuals and stable `fit_key`. Runtime converts incident normal speed from cm/s to m/s before evaluation. Keep `maximumRigidIncidentSpeedCmS`; exceeding it sets a warning and does not silently extrapolate confidence. Record instantaneous contact time as an explicit limitation, not an 8 ms prediction.

- [ ] **Step 4: Run fit/profile/contact regressions and reproduce artifacts**

Run: `python3 -m unittest tests.physics_validation.test_cushion_fit -v && cmake --build build --target cushion-contact-tests physics-profile-tests -j2 && ctest --test-dir build -R 'cushion-contact|physics-profile' --output-on-failure && python3 -m tools.physics_validation.fit_cushion --inputs physics_models/calibration/cushion_fit_v2_inputs.csv --output /tmp/cushion-v2.json --residuals /tmp/cushion-v2.csv && cmp /tmp/cushion-v2.json physics_models/calibration/cushion_fit_v2.json && cmp /tmp/cushion-v2.csv physics_models/calibration/cushion_fit_v2_residuals.csv`

Expected: all tests pass and committed fit artifacts reproduce byte-for-byte.

- [ ] **Step 5: Commit cushion candidate fit and runtime law**

```bash
git add src/Billiards/physics_profile.h src/Billiards/physics_profile.cpp src/Billiards/cushion_contact.cpp tools/physics_validation/fit_cushion.py tests/physics_validation/test_cushion_fit.py tests/cushion_contact_tests.cpp physics_models/calibration/cushion_fit_v2_inputs.csv physics_models/calibration/cushion_fit_v2.json physics_models/calibration/cushion_fit_v2_residuals.csv
git commit -m "feat: fit speed-dependent cushion candidate v2"
```

### Task 4: Assemble and commit the integrated pre-freeze candidate

**Files:**
- Create: `physics_models/profiles/chinese_pool_full_game_v2.json`
- Modify: `src/Billiards/physics_profile.cpp`
- Modify: `tests/physics_profile_tests.cpp`
- Modify: `tests/physics_validation/test_candidate_workflow.py`
- Create: `physics_models/promotion/phase3_candidates_v2.json`

**Interfaces:**
- Produces: production default profile `chinese_pool_full_game_v2` and a complete pre-freeze inventory.
- Consumes: cue/pocket v1 bounded parameters and surface/ball/cushion/solver v2 outputs.

- [ ] **Step 1: Write a failing profile-default and provenance test**

```python
def test_integrated_profile_is_the_runtime_default(self):
    completed = subprocess.run([str(EXECUTABLE), "--print-physics-profile"],
                               check=True, capture_output=True, text=True)
    self.assertEqual(json.loads(completed.stdout)["id"], "chinese_pool_full_game_v2")

def test_integrated_profile_uses_committed_fit_outputs(self):
    profile = json.loads(PROFILE.read_text(encoding="utf-8"))
    self.assertEqual(profile["surface"], selected_surface_parameters(SURFACE_FIT))
    self.assertEqual(profile["ball"], selected_ball_parameters(BALL_FIT))
    self.assertEqual(profile["cushion"], selected_cushion_parameters(CUSHION_FIT))
```

- [ ] **Step 2: Run profile tests before changing the production default**

Run: `python3 -m unittest tests.physics_validation.test_candidate_workflow -v && cmake --build build --target physics-profile-tests -j2 && ctest --test-dir build -R physics-profile --output-on-failure`

Expected: failures because profile v2 does not exist and runtime still selects v1.

- [ ] **Step 3: Assemble the profile and preregister every release input**

```python
candidate = {
    "schema_version": 2,
    "candidate_id": "phase3_integrated_v2",
    "profile": hashed_artifact(PROFILE, "profile"),
    "calibration_reports": calibration_artifacts(),
    "confirmation_packages": confirmation_package_artifacts(),
    "full_game_matrix": hashed_artifact(MATRIX, "full_game_matrix"),
    "performance_budget": hashed_artifact(BUDGET, "performance_budget"),
    "metric_contracts": metric_contract_artifacts(),
}
```

Populate the integrated profile only from committed fit JSON values. Carry cue/pocket limitations verbatim; add the cushion instantaneous-contact and supported-speed-domain limitations. Update `defaultChinesePoolPhysicsProfile()` to return v2. The inventory contains splits, metrics, acceptance intervals, supplemental scalars, matrix, and budget, but no result artifact.

- [ ] **Step 4: Run pre-freeze verification without confirmation**

Run: `python3 -m unittest tests.physics_validation.test_candidate_workflow tests.physics_validation.test_phase3_v2_source_packages -v && cmake --build build -j2 && ctest --test-dir build --output-on-failure`

Expected: all tests pass and no Sudo/Derby confirmation output exists.

- [ ] **Step 5: Commit the source revision that will be frozen**

```bash
git add physics_models/profiles/chinese_pool_full_game_v2.json src/Billiards/physics_profile.cpp tests/physics_profile_tests.cpp tests/physics_validation/test_candidate_workflow.py physics_models/promotion/phase3_candidates_v2.json
git commit -m "feat: select integrated phase 3 candidate v2"
```

### Task 5: Reproducibly freeze the committed source and executable

**Files:**
- Modify: `tools/physics_validation/freeze_candidate.py`
- Create: `tests/physics_validation/test_phase3_v2_freeze.py`
- Create: `physics_models/candidates/phase3_integrated_v2/freeze.json`

**Interfaces:**
- Produces: immutable freeze binding the Task 4 commit, two identical clean-build digests, profile, calibration, package, matrix, budget, metrics, and supplemental inputs.
- Consumes: clean `HEAD`, pre-freeze inventory, and strict freeze verifier.

- [ ] **Step 1: Write failing source-revision and two-build tests**

```python
def test_freeze_binds_committed_profile_source(self):
    freeze = json.loads(FREEZE.read_text(encoding="utf-8"))
    self.assertEqual(profile_at_revision(freeze["source_revision"])["id"],
                     "chinese_pool_full_game_v2")

def test_two_clean_build_digests_match(self):
    freeze = json.loads(FREEZE.read_text(encoding="utf-8"))
    self.assertEqual(len(set(freeze["clean_build_sha256"])), 1)
    self.assertEqual(freeze["executable_sha256"], freeze["clean_build_sha256"][0])
```

- [ ] **Step 2: Run freeze tests before the freeze exists**

Run: `python3 -m unittest tests.physics_validation.test_phase3_v2_freeze -v`

Expected: missing `freeze.json` failure.

- [ ] **Step 3: Build twice from committed HEAD and write the freeze**

```python
def freeze_document(source_revision, build_digests, profile, artifacts):
    if len(set(build_digests)) != 1:
        raise ValueError("clean build executable digests differ")
    return {
        "schema_version": 2,
        "candidate_id": "phase3_integrated_v2",
        "source_revision": source_revision,
        "executable_sha256": build_digests[0],
        "clean_build_sha256": build_digests,
        "profile": hashed_artifact(profile, "profile"),
        "artifacts": sorted(artifacts, key=lambda item: (item["role"], item["path"])),
    }
```

Require a clean tree, build from two detached temporary worktrees at Task 4's commit, and compare executable plus canonical profile output. The freeze does not hash itself and is never modified later.

- [ ] **Step 4: Verify freeze without running confirmation**

Run: `python3 -m unittest tests.physics_validation.test_phase3_v2_freeze tests.physics_validation.test_freeze_verifier -v`

Expected: all tests pass and no confirmation ledger/output exists.

- [ ] **Step 5: Commit the immutable freeze**

```bash
git add tools/physics_validation/freeze_candidate.py tests/physics_validation/test_phase3_v2_freeze.py physics_models/candidates/phase3_integrated_v2/freeze.json
git commit -m "release: freeze integrated phase 3 candidate v2"
```

### Task 6: Run the full-game matrix against the frozen executable

**Files:**
- Create: `tests/physics_validation/test_phase3_v2_full_game_run.py`
- Create: `physics_models/candidates/phase3_integrated_v2/full_game/**`
- Create: `physics_models/promotion/full_game_performance_baseline_v2.json`
- Create: `physics_models/promotion/full_game_stress_v2.csv`

**Interfaces:**
- Produces: twelve complete summary/trace pairs and performance baseline bound to the frozen executable.
- Consumes: frozen executable/profile, matrix v2, and preregistered budget v2.

- [ ] **Step 1: Write failing frozen-output completeness tests**

```python
def test_full_game_outputs_bind_the_frozen_executable(self):
    freeze = json.loads(FREEZE.read_text(encoding="utf-8"))
    baseline = json.loads(BASELINE.read_text(encoding="utf-8"))
    self.assertEqual(baseline["executable_sha256"], freeze["executable_sha256"])
    self.assertEqual(set(baseline["cases"]), REQUIRED_CASES)
    self.assertTrue(all((OUTPUT / case / "trace.json").is_file()
                        for case in REQUIRED_CASES))
```

- [ ] **Step 2: Run before frozen outputs exist**

Run: `python3 -m unittest tests.physics_validation.test_phase3_v2_full_game_run -v`

Expected: missing baseline/output failures.

- [ ] **Step 3: Execute the frozen matrix and preserve complete outputs**

Run: `build/full-game-stress --matrix physics_models/promotion/full_game_matrix_v2.json --write physics_models/candidates/phase3_integrated_v2/full_game`

Expected: all twelve cases pass with complete traces and budgets.

- [ ] **Step 4: Validate traces, hashes, derived CSV, and baseline**

Run: `python3 -m unittest tests.physics_validation.test_phase3_v2_full_game_run tests.physics_validation.test_full_game_stress_artifact tests.physics_validation.test_full_game_performance -v`

Expected: all tests pass.

- [ ] **Step 5: Commit frozen full-game evidence**

```bash
git add tests/physics_validation/test_phase3_v2_full_game_run.py physics_models/candidates/phase3_integrated_v2/full_game physics_models/promotion/full_game_performance_baseline_v2.json physics_models/promotion/full_game_stress_v2.csv
git commit -m "data: preserve frozen phase 3 full-game results"
```

### Task 7: Guard independent confirmation before opening it

**Files:**
- Modify: `tools/physics_validation/validation_run.py`
- Modify: `tools/physics_validation/holdout_access.py`
- Create: `tests/physics_validation/test_phase3_v2_confirmation.py`

**Interfaces:**
- Produces: `validate_confirmation_access(root, freeze, package, ledger)`, exclusive output creation, and append-only consumption-record support.
- Consumes: immutable freeze and synthetic ledger fixtures; fitter imports remain forbidden.

- [ ] **Step 1: Write failing one-time/access/source-isolation tests before opening data**

```python
def test_confirmation_runner_requires_frozen_unopened_candidate(self):
    self.assertEqual(validate_confirmation_access(ROOT, FREEZE, SUDO, EMPTY_LEDGER), [])

def test_fitters_do_not_import_confirmation_packages(self):
    for path in (FIT_SURFACE, FIT_BALL, FIT_CUSHION):
        text = path.read_text(encoding="utf-8")
        self.assertNotIn("sudo_2002", text)
        self.assertNotIn("derby_fuller_1999", text)

def test_second_confirmation_execution_is_rejected(self):
    consumed = confirmation_fixture(consumed=True)
    self.assertIn("confirmation partition is already consumed",
                  validate_confirmation_access(ROOT, FREEZE, SUDO, consumed))
```

- [ ] **Step 2: Run access tests while confirmation is still unopened**

Run: `python3 -m unittest tests.physics_validation.test_phase3_v2_confirmation -v`

Expected: tests fail only for missing confirmation-specific access support; do not invoke the executable yet.

- [ ] **Step 3: Implement fail-closed confirmation opening and atomic receipt writing**

```python
def consume_confirmation(freeze_path, package_path, output_path, ledger_path, runner):
    failures = validate_confirmation_access(ROOT, freeze_path, package_path, ledger_path)
    if failures:
        raise ConfirmationAccessError("; ".join(failures))
    result = runner()
    write_directory_atomically(output_path, result.files)
    receipt = build_receipt(result, freeze_path, package_path)
    write_json_exclusive(output_path / "validation_receipt.json", receipt)
    append_consumption_record_exclusive(ledger_path, freeze_path, package_path, receipt)
    return receipt
```

Use exclusive creation for first receipt, output to a sibling temporary directory, hash every file before atomic rename, and append the exact package partition to a ledger that hash-binds—but never modifies—the freeze. Record consumption even when result is `FAILED`. Confirmation metrics include Sudo restitution/separation/momentum/contact-time limitation and Derby slide-time/final-speed/momentum/energy; both unequal Derby accelerations appear as diagnostics only.

- [ ] **Step 4: Commit runner/access code before opening confirmation**

Run: `python3 -m unittest tests.physics_validation.test_phase3_v2_confirmation tests.physics_validation.test_validation_run -v`

Expected: all tests pass against synthetic fixtures and the real freeze still reports unopened.

```bash
git add tools/physics_validation/validation_run.py tools/physics_validation/holdout_access.py tests/physics_validation/test_phase3_v2_confirmation.py
git commit -m "feat: guard one-time phase 3 confirmation"
```

### Task 8: Execute and preserve first-result confirmation evidence

**Files:**
- Create: `physics_models/candidates/phase3_integrated_v2/confirmation/sudo_2002/**`
- Create: `physics_models/candidates/phase3_integrated_v2/confirmation/derby_fuller_1999/**`
- Create: `physics_models/candidates/phase3_integrated_v2/confirmation_consumption.json`

**Interfaces:**
- Produces: immutable first-result receipts, reports, CSVs, traces, provenance, and two consumption records.
- Consumes: the frozen executable/profile and each confirmation package exactly once.

- [ ] **Step 1: Verify both real partitions are open without creating files**

Run: `python3 -m tools.physics_validation.holdout_access --freeze physics_models/candidates/phase3_integrated_v2/freeze.json --package tests/physics_validation/reference_data/sudo_2002 --ledger physics_models/candidates/phase3_integrated_v2/confirmation_consumption.json && python3 -m tools.physics_validation.holdout_access --freeze physics_models/candidates/phase3_integrated_v2/freeze.json --package tests/physics_validation/reference_data/derby_fuller_1999 --ledger physics_models/candidates/phase3_integrated_v2/confirmation_consumption.json`

Expected: both checks pass and do not create the ledger.

- [ ] **Step 2: Execute each frozen confirmation partition once**

Run exactly once: `python3 -m tools.physics_validation.validation_run --freeze physics_models/candidates/phase3_integrated_v2/freeze.json --package tests/physics_validation/reference_data/sudo_2002 --ledger physics_models/candidates/phase3_integrated_v2/confirmation_consumption.json --executable build/physics-scenario --output physics_models/candidates/phase3_integrated_v2/confirmation/sudo_2002`

Run exactly once: `python3 -m tools.physics_validation.validation_run --freeze physics_models/candidates/phase3_integrated_v2/freeze.json --package tests/physics_validation/reference_data/derby_fuller_1999 --ledger physics_models/candidates/phase3_integrated_v2/confirmation_consumption.json --executable build/physics-scenario --output physics_models/candidates/phase3_integrated_v2/confirmation/derby_fuller_1999`

Expected: each command creates one immutable receipt and complete numeric output. If either receipt is `FAILED`, stop this plan, commit the failure evidence, and mark candidate v2 rejected; do not continue to release.

- [ ] **Step 3: Validate first-result evidence and one-time denial**

Run: `python3 -m unittest tests.physics_validation.test_phase3_v2_confirmation tests.physics_validation.test_complete_validation_artifacts -v && python3 -m tools.physics_validation.holdout_access --freeze physics_models/candidates/phase3_integrated_v2/freeze.json --package tests/physics_validation/reference_data/sudo_2002 --ledger physics_models/candidates/phase3_integrated_v2/confirmation_consumption.json; test $? -ne 0`

Expected: tests pass and the attempted second Sudo execution exits non-zero with `confirmation partition is already consumed` without changing any file.

- [ ] **Step 4: Verify receipts fail closed**

Run: `python3 -c 'import json; from pathlib import Path; paths=Path("physics_models/candidates/phase3_integrated_v2/confirmation").glob("*/validation_receipt.json"); assert all(json.loads(p.read_text())["result"] == "PASSED_OR_ACCOUNTED" for p in paths)'`

Expected: zero exit status. If a receipt is `FAILED`, stop release work and commit the unchanged failure evidence as a rejected v2 candidate.

- [ ] **Step 5: Commit first-result evidence**

```bash
git add physics_models/candidates/phase3_integrated_v2/confirmation physics_models/candidates/phase3_integrated_v2/confirmation_consumption.json
git commit -m "data: preserve phase 3 v2 confirmation results"
```

### Task 9: Generate complete old/new comparison and strict final release

**Files:**
- Create: `tools/physics_validation/compare_phase3.py`
- Create: `tests/physics_validation/test_phase3_comparison.py`
- Create: `physics_models/promotion/phase3_v1_v2_comparison.json`
- Create: `docs/phase3-v1-v2-comparison.md`
- Create: `physics_models/promotion/phase3_validation_artifacts_v2.json`
- Create: `physics_models/promotion/phase3_release_v2.json`
- Modify: `tests/physics_validation/test_phase3_release_gate.py`
- Modify: `scripts/check_phase3_physics_release.py`

**Interfaces:**
- Produces: exhaustive comparison keyed by source point/case and schema-v2 release manifest with status exactly `PASSED`.
- Consumes: preserved v1 rows including failures, v2 calibration/confirmation receipts, solver/full-game traces, budgets/baselines, artifact inventory, source/executable/profile checks.

- [ ] **Step 1: Write failing omission and final-gate tests**

```python
def test_comparison_contains_every_v1_row(self):
    comparison = build_comparison(ROOT)
    expected = all_v1_experimental_point_ids(ROOT)
    self.assertEqual(set(comparison["experimental_points"]), expected)

def test_corrected_integrations_are_cause_paired(self):
    comparison = build_comparison(ROOT)
    causes = {row["cause_code"] for row in comparison["corrected_integrations"]}
    self.assertEqual(causes, {
        "CROSS_PHASE_SELECTION", "INITIAL_STATE_OUTSIDE_DOMAIN",
        "UNINTENDED_SECOND_COLLISION",
    })

def test_release_gate_accepts_only_complete_passed_v2(self):
    self.assertEqual(validate_phase3_release(ROOT, RELEASE, EXECUTABLE), [])
```

- [ ] **Step 2: Run comparison/release tests before artifacts exist**

Run: `python3 -m unittest tests.physics_validation.test_phase3_comparison tests.physics_validation.test_phase3_release_gate -v`

Expected: failures because comparison, inventory v2, and release v2 do not exist.

- [ ] **Step 3: Implement exhaustive comparison with stable aggregates**

```python
def series_statistics(rows):
    normalized = [row["normalized_error"] for row in rows]
    return {
        "count": len(rows),
        "rmse": math.sqrt(sum(value * value for value in normalized) / len(normalized)),
        "maximum_absolute_normalized_error": max(abs(value) for value in normalized),
        "passed": sum(row["status"] == "PASSED" for row in rows),
        "failed": sum(row["status"] != "PASSED" for row in rows),
    }
```

Emit every common experimental row/status, per-series statistics, solver residual/penetration/energy/iteration/event distributions, all twelve gameplay outcomes/hashes, wall/RSS comparison, profile parameter provenance, and limitations gained/resolved/retained. Pair each repaired scenario with its original failure ID and cause code. Generate Markdown strictly from comparison JSON so the machine-readable file is authoritative.

- [ ] **Step 4: Build complete inventory and release manifest**

```python
release = {
    "schema_version": 2,
    "status": "PASSED",
    "source_revision": git_head(ROOT),
    "executable_sha256": sha256(EXECUTABLE),
    "profile": hashed_artifact(PROFILE, "profile"),
    "inputs": release_inputs_with_hashes(),
    "unexplained_regressions": 0,
}
```

Generate `phase3_validation_artifacts_v2.json` from every declared calibration/confirmation/full-game/comparison path. Refuse to write a `PASSED` release unless every candidate receipt is `PASSED_OR_ACCOUNTED`, every confirmation/full-game/performance/inventory/source/profile check passes, and the production default is the frozen v2 profile.

- [ ] **Step 5: Run complete final verification from a clean build**

Run: `git diff --check && cmake --build build -j2 && ctest --test-dir build --output-on-failure && python3 -m unittest discover -s tests/physics_validation -p 'test_*.py' -v && python3 scripts/check_phase3_physics_release.py --release physics_models/promotion/phase3_release_v2.json --executable build/physics-scenario`

Expected: all C++/Python tests pass, release checker prints `phase3_release_v2: PASSED`, and no confirmation runner is called by CI.

- [ ] **Step 6: Commit comparison and final release evidence**

```bash
git add tools/physics_validation/compare_phase3.py tests/physics_validation/test_phase3_comparison.py tests/physics_validation/test_phase3_release_gate.py scripts/check_phase3_physics_release.py physics_models/promotion/phase3_v1_v2_comparison.json docs/phase3-v1-v2-comparison.md physics_models/promotion/phase3_validation_artifacts_v2.json physics_models/promotion/phase3_release_v2.json
git commit -m "release: accept strict phase 3 physics v2"
```

## Plan Verification

- Confirm every fitter input is `calibration` or `spent`, never `confirmation`.
- Confirm both confirmation receipts were created once, are hash-inventoried, and a replay attempt fails without filesystem changes.
- Run the complete clean-build C++/Python/release command in Task 6.
- Verify `phase3_release_v2.json` says exactly `PASSED`, points at an ancestor source revision, binds the built executable/profile, and comparison JSON contains every failed v1 row.
- Request a final independent code review; any blocking finding prevents Phase 3 completion.
