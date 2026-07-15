# Phase 3 Integrated v3 Successor Release Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Build, freeze, independently confirm, and strictly release one `phase3_integrated_v3` successor without modifying v2 evidence or replaying Sudo confirmation.

**Architecture:** Move Sudo to spent through an external lifecycle transition while preserving its package and v2 hashes, admit Han 2005 as an equipment-transfer-limited confirmation package, and refit only representable ball/cushion observations. Harden confirmation into a reserve-before-read transaction, assemble and freeze one v3 production profile, preserve complete full-game evidence, execute Derby and then Han exactly once behind explicit checkpoints, and publish only when every receipt and release gate passes.

**Tech Stack:** Python 3 standard library, C++17, CMake/CTest, canonical JSON, CSV, Markdown, SHA-256, Git.

## Global Constraints

- Preserve every v2 freeze, trace, receipt, ledger, package, and failure byte unchanged.
- Never execute the v2 Sudo confirmation again.
- Commit every numeric input/output and complete trace used by fitting, diagnosis, confirmation, comparison, or release; do not commit copyrighted publication media.
- Derby and Han never influence fitting, formula selection, candidate ranking, or post-result tolerances.
- Reserve confirmation before the evaluator reads package content or starts the executable.
- Freeze exactly one v3 candidate; a failed receipt rejects that candidate and stops the plan.
- Execute Derby before Han. Do not reserve Han unless Derby is `PASSED_OR_ACCOUNTED`.
- Successful release status is exactly `PASSED`.
- Each task produces one independently reviewable commit.

---

### Task 1: Preserve v2 rejection and transition Sudo to spent

**Files:**
- Create: `tools/physics_validation/successor_lifecycle.py`
- Create: `tests/physics_validation/test_phase3_v3_lifecycle.py`
- Create: `physics_models/promotion/phase3_integrated_v2_rejection.json`
- Create: `physics_models/promotion/sudo_2002_spent_transition.json`
- Modify: `tests/physics_validation/validation_data_status.json`
- Modify: `tests/physics_validation/test_phase3_v2_source_packages.py`

**Interfaces:**
- Consumes: immutable v2 freeze, Sudo failure receipt, v2 confirmation ledger, and Sudo package manifest.
- Produces: `build_rejection(root) -> dict`, `build_spent_transition(root, dataset_id, dataset_version, rejected_candidate) -> dict`, and a lifecycle registry where Sudo `1.0.0` has both statuses `spent`.

- [ ] **Step 1: Write failing immutability and transition tests**

```python
def test_v2_rejection_binds_existing_failure_without_mutation(self):
    rejection = build_rejection(ROOT)
    self.assertEqual(rejection["candidate_id"], "phase3_integrated_v2")
    self.assertEqual(rejection["status"], "REJECTED")
    self.assertEqual(rejection["receipt_sha256"], digest(V2_RECEIPT))
    self.assertEqual(rejection["ledger_sha256"], digest(V2_LEDGER))

def test_sudo_is_spent_but_original_package_still_matches_v2_freeze(self):
    entry = load_data_lifecycle(STATUS).entry("sudo_2002", "1.0.0")
    self.assertEqual((entry.calibration_status, entry.holdout_status),
                     ("spent", "spent"))
    self.assertEqual(digest(SUDO / "manifest.json"), V2_SUDO_MANIFEST_SHA256)
```

- [ ] **Step 2: Run the lifecycle test and observe Sudo is still confirmation**

Run: `python3 -m unittest tests.physics_validation.test_phase3_v3_lifecycle -v`

Expected: FAIL because the transition builder/artifacts do not exist and Sudo remains `confirmation`.

- [ ] **Step 3: Implement canonical transition and rejection records**

```python
def build_spent_transition(root, dataset_id, dataset_version,
                           rejected_candidate):
    receipt = root / ("physics_models/candidates/phase3_integrated_v2/"
                      "confirmation/sudo_2002/validation_receipt.json")
    ledger = root / ("physics_models/candidates/phase3_integrated_v2/"
                     "confirmation_consumption.json")
    return {
        "schema_version": 1,
        "dataset_id": dataset_id,
        "dataset_version": dataset_version,
        "from": "confirmation",
        "to": "spent",
        "rejected_candidate": rejected_candidate,
        "receipt_sha256": sha256(receipt),
        "ledger_sha256": sha256(ledger),
        "reason": "sole confirmation transaction failed closed",
    }
```

Write canonical JSON records. Change only the external lifecycle registry; do not edit any file under `tests/physics_validation/reference_data/sudo_2002` or `physics_models/candidates/phase3_integrated_v2`.

- [ ] **Step 4: Prove v2 bytes are unchanged and lifecycle tests pass**

Run: `git diff --exit-code 948c936 -- physics_models/candidates/phase3_integrated_v2 tests/physics_validation/reference_data/sudo_2002 && python3 -m unittest tests.physics_validation.test_phase3_v3_lifecycle tests.physics_validation.test_phase3_v2_source_packages tests.physics_validation.test_validation_run -v`

Expected: no v2/Sudo-package diff and all tests pass.

- [ ] **Step 5: Commit the lifecycle transition**

```bash
git add tools/physics_validation/successor_lifecycle.py tests/physics_validation/test_phase3_v3_lifecycle.py tests/physics_validation/test_phase3_v2_source_packages.py tests/physics_validation/validation_data_status.json physics_models/promotion/phase3_integrated_v2_rejection.json physics_models/promotion/sudo_2002_spent_transition.json
git commit -m "data: retire v2 confirmation into spent evidence"
```

### Task 2: Admit and preregister Han 2005 confirmation evidence

**Files:**
- Create: `tools/physics_validation/extract_han_2005.py`
- Create: `tests/physics_validation/test_han_2005_extraction.py`
- Create: `tests/physics_validation/reference_data/han_2005/{manifest.json,raw_extracted.csv,normalized.csv,scalars.csv,split.json,extraction.json,scenario_template.json,expected_model_mismatches.json,expected_reference_limitations.json,source_access_audit.json}`
- Modify: `tests/physics_validation/validation_data_status.json`
- Modify: `tests/physics_validation/test_phase3_v2_source_packages.py`

**Interfaces:**
- Produces: `han_restitution(speed_m_s) -> float`, canonical package `han_2005@1.0.0`, and preregistered metrics `normalized_curve_rmse`, `finite_bounded_response`, `continuous_response`, `source_domain_response`, and `nonincreasing_total_energy`.
- Consumes: printed empirical relation `e(v)=0.39+0.257v-0.044v^2`, source domain declared by the paper/package, bibliographic metadata, and source audit hash; no candidate output.

- [ ] **Step 1: Write failing extraction, lifecycle, and applicability tests**

```python
def test_empirical_relation_reproduces_full_precision_points(self):
    expected = [han_restitution(v) for v in (0.5, 1.0, 1.5, 2.0, 2.5)]
    rows = read_scalars(HAN / "scalars.csv")
    self.assertEqual([row.normalized_value for row in rows], expected)

def test_han_is_confirmation_and_absolute_values_are_transfer_limited(self):
    entry = load_data_lifecycle(STATUS).entry("han_2005", "1.0.0")
    self.assertEqual(entry.holdout_status, "confirmation")
    points = read_reference_points(HAN / "normalized.csv", "han_2005")
    self.assertEqual({point.pool_applicability for point in points},
                     {"TRANSFER_LIMITED"})

def test_hard_contract_was_fixed_without_candidate_predictions(self):
    contract = json.loads((HAN / "scenario_template.json").read_text())
    self.assertEqual(contract["normalized_curve_rmse_maximum"], 0.15)
    self.assertNotIn("observed", json.dumps(contract))
```

- [ ] **Step 2: Run extraction tests and observe the package is absent**

Run: `python3 -m unittest tests.physics_validation.test_han_2005_extraction -v`

Expected: FAIL because the extractor and package do not exist.

- [ ] **Step 3: Implement deterministic extraction and package generation**

```python
COEFFICIENTS = (0.39, 0.257, -0.044)
SPEEDS_M_S = (0.5, 1.0, 1.5, 2.0, 2.5)

def han_restitution(speed_m_s):
    a, b, c = COEFFICIENTS
    value = a + b * speed_m_s + c * speed_m_s * speed_m_s
    if not math.isfinite(value):
        raise ValueError("Han restitution must be finite")
    return value
```

Generate every derived point from the printed coefficients. Record coefficient rounding uncertainty, engineering curve tolerance `0.15`, apparatus `carom_three_cushion`, applicability `TRANSFER_LIMITED`, DOI, page/equation locator, acquisition date/hash, and copyright status. Keep absolute values diagnostic; hard gates use the normalized curve and constitutive invariants.

- [ ] **Step 4: Reproduce every package byte and validate the generic schema**

Run: `python3 -m tools.physics_validation.extract_han_2005 --package tests/physics_validation/reference_data/han_2005 --check && python3 -m unittest tests.physics_validation.test_han_2005_extraction tests.physics_validation.test_reference_package tests.physics_validation.test_reference_accounting tests.physics_validation.test_phase3_v2_source_packages -v`

Expected: extractor reports no byte difference and all tests pass.

- [ ] **Step 5: Commit the Han package and preregistration**

```bash
git add tools/physics_validation/extract_han_2005.py tests/physics_validation/test_han_2005_extraction.py tests/physics_validation/reference_data/han_2005 tests/physics_validation/validation_data_status.json tests/physics_validation/test_phase3_v2_source_packages.py
git commit -m "data: admit Han 2005 cushion confirmation"
```

### Task 3: Fit v3 ball and cushion models from spent evidence

**Files:**
- Create: `tools/physics_validation/build_v3_fit_inputs.py`
- Modify: `tools/physics_validation/fit_ball_collision.py`
- Modify: `tools/physics_validation/fit_cushion.py`
- Modify: `tests/physics_validation/test_ball_collision_fit.py`
- Modify: `tests/physics_validation/test_cushion_fit.py`
- Create: `tests/physics_validation/test_phase3_v3_fit.py`
- Create: `physics_models/calibration/{ball_collision_fit_v3_inputs.csv,ball_collision_fit_v3.json,ball_collision_fit_v3_residuals.csv,cushion_fit_v3_inputs.csv,cushion_fit_v3.json,cushion_fit_v3_residuals.csv,sudo_2002_structural_residuals.csv}`

**Interfaces:**
- Produces: deterministic v3 fit documents plus `build_ball_inputs(root)`, `build_cushion_inputs(root)`, `fit_ball_collision_v3(points)`, and `fit_cushion_v3(points)`.
- Consumes: existing spent Mathavan/Doménech inputs and Sudo scalars labeled `spent`; explicitly excludes Derby and Han.

- [ ] **Step 1: Write failing spent-source, series-balance, and structural-gap tests**

```python
def test_v3_inputs_include_sudo_as_spent_and_no_confirmation(self):
    rows = build_ball_inputs(ROOT) + build_cushion_inputs(ROOT)
    self.assertIn("sudo_2002", {row.dataset_id for row in rows})
    self.assertEqual({row.lifecycle for row in rows}, {"spent"})
    self.assertFalse({"derby_fuller_1999", "han_2005"} &
                     {row.dataset_id for row in rows})

def test_every_series_has_equal_objective_weight(self):
    fit = fit_ball_collision_v3(read_v3_ball_inputs(BALL_INPUTS))
    self.assertEqual(set(fit.series_mse), {
        "billiard_alpha1", "billiard_delta2", "mathavan_velocity",
        "sudo_ball_collision",
    })

def test_contact_time_is_visible_but_not_faked(self):
    rows = read_csv(STRUCTURAL)
    self.assertEqual(rows[0]["point_id"], "cushion_contact_time_plateau")
    self.assertEqual(rows[0]["status"], "OUT_OF_MODEL_SPENT")
    self.assertEqual(rows[0]["observed"], "")
```

- [ ] **Step 2: Run v3 fit tests and observe unsupported Sudo metrics**

Run: `python3 -m unittest tests.physics_validation.test_phase3_v3_fit tests.physics_validation.test_ball_collision_fit tests.physics_validation.test_cushion_fit -v`

Expected: FAIL because v3 input builders/artifacts and Sudo metric adapters do not exist.

- [ ] **Step 3: Extend fit schemas without weakening v2 reproduction**

```python
V3_BALL_DATASETS = {
    "domenech_2023_ball_collision", "mathavan_2009_high_speed", "sudo_2002",
}
V3_BALL_METRICS = {
    "cue_scattering_angle_degrees",
    "object_scattering_angle_degrees",
    "object_normal_deflection_angle_degrees",
    "separation_angle_degrees",
    "post_collision_cue_speed_cm_s",
    "post_collision_object_speed_cm_s",
    "ball_ball_normal_restitution",
    "post_collision_separation_angle_degrees",
    "transverse_momentum_deficit_fraction",
}

def balanced_objective(rows):
    grouped = group_by_series(rows)
    by_series = {
        key: sum(item.normalized_residual ** 2 for item in values) / len(values)
        for key, values in sorted(grouped.items())
    }
    return sum(by_series.values()) / len(by_series), by_series
```

Keep the v2 CLI and artifacts byte-reproducible. Add schema-version dispatch for v3. Treat Sudo `8 ms` contact time only in the structural residual file. Reject any fit row whose lifecycle is not `spent` or whose dataset is Derby/Han.

- [ ] **Step 4: Generate and reproduce complete v3 fit artifacts**

Run: `python3 -m tools.physics_validation.build_v3_fit_inputs --root . --write physics_models/calibration && python3 -m tools.physics_validation.fit_ball_collision --inputs physics_models/calibration/ball_collision_fit_v3_inputs.csv --output physics_models/calibration/ball_collision_fit_v3.json --residuals physics_models/calibration/ball_collision_fit_v3_residuals.csv && python3 -m tools.physics_validation.fit_cushion --inputs physics_models/calibration/cushion_fit_v3_inputs.csv --output physics_models/calibration/cushion_fit_v3.json --residuals physics_models/calibration/cushion_fit_v3_residuals.csv && python3 -m unittest tests.physics_validation.test_phase3_v3_fit tests.physics_validation.test_ball_collision_fit tests.physics_validation.test_cushion_fit -v`

Expected: all tests pass; a second generation into a temporary directory compares byte-for-byte with the committed outputs.

- [ ] **Step 5: Commit the v3 spent-data fits**

```bash
git add tools/physics_validation/build_v3_fit_inputs.py tools/physics_validation/fit_ball_collision.py tools/physics_validation/fit_cushion.py tests/physics_validation/test_ball_collision_fit.py tests/physics_validation/test_cushion_fit.py tests/physics_validation/test_phase3_v3_fit.py physics_models/calibration/ball_collision_fit_v3_inputs.csv physics_models/calibration/ball_collision_fit_v3.json physics_models/calibration/ball_collision_fit_v3_residuals.csv physics_models/calibration/cushion_fit_v3_inputs.csv physics_models/calibration/cushion_fit_v3.json physics_models/calibration/cushion_fit_v3_residuals.csv physics_models/calibration/sudo_2002_structural_residuals.csv
git commit -m "feat: fit phase 3 successor from spent evidence"
```

### Task 4: Make confirmation reserve-before-read and crash-finalizable

**Files:**
- Create: `tools/physics_validation/confirmation_transaction.py`
- Modify: `tools/physics_validation/holdout_access.py`
- Modify: `tools/physics_validation/validation_run.py`
- Modify: `tools/physics_validation/confirmation_run.py`
- Create: `tests/physics_validation/test_confirmation_transaction.py`
- Create: `tests/physics_validation/fixtures/confirmation_transaction_v1/{manifest.json,normalized.csv,raw_extracted.csv,scalars.csv,split.json,extraction.json,scenario_template.json,expected_model_mismatches.json,expected_reference_limitations.json,source_access_audit.json}`
- Modify: `tests/physics_validation/test_phase3_v2_confirmation.py`

**Interfaces:**
- Produces: `confirmation_declaration(root, freeze, package_key) -> Declaration`, `validate_confirmation_access_from_freeze(root, freeze, package_key, ledger) -> list[str]`, `reserve_from_freeze(root, freeze, package_key, ledger) -> Attempt`, `complete_attempt(attempt, result) -> Receipt`, `finalize_interrupted(attempt, output) -> Receipt`, and generic `consume_confirmation(freeze, package_key, output, ledger, evaluator) -> Receipt` with no hard-coded candidate ID.
- Consumes: freeze-declared confirmation manifest path/digest before reservation; package content only after reservation.

- [ ] **Step 1: Write failing ordering, concurrency, and fault-injection tests**

```python
def test_reservation_precedes_package_read_and_runner_launch(self):
    events = []
    consume_confirmation(
        freeze=self.freeze,
        package_key="fixture_confirmation",
        output=self.output,
        ledger=self.ledger,
        evaluator=recording_fixture_evaluator(events, self.ledger),
        repository_root=self.root,
    )
    self.assertEqual(events, [("open", True), ("run", True)])

def test_concurrent_consumers_launch_exactly_one_runner(self):
    launches = run_two_consumers_with_barrier()
    self.assertEqual(launches, 1)

def test_interrupted_attempt_can_only_be_finalized_failed(self):
    attempt = reserve_fixture_attempt()
    receipt = finalize_interrupted(attempt, output)
    self.assertEqual(receipt["result"], "FAILED")
    self.assertEqual(receipt["failure_code"], "FAILED_INTERRUPTED")
    self.assertRaises(ConfirmationAccessError, replay_fixture_attempt)
```

- [ ] **Step 2: Run transaction tests and observe package validation occurs first**

Run: `python3 -m unittest tests.physics_validation.test_confirmation_transaction -v`

Expected: FAIL because current `validate_confirmation_access` opens the package before `STARTED` is persisted and no interrupted finalizer exists.

- [ ] **Step 3: Implement the generic transaction state machine**

```python
def consume_confirmation(freeze, package_key, output, ledger, evaluator):
    declaration = confirmation_declaration(freeze, package_key)
    attempt = reserve_from_freeze(declaration, ledger)
    try:
        package = open_and_verify_package(declaration)
        result = evaluator(package)
        return complete_attempt(attempt, output, result)
    except Exception as error:
        return complete_attempt(
            attempt, output, failed_result(type(error).__name__, str(error)))
```

Use `O_CREAT|O_EXCL`, lock files, atomic sibling directories, canonical JSON, file and parent-directory `fsync`, stable attempt IDs derived from frozen identities, and append-only semantic records. Access checking reads freeze declarations and ledger only. Preserve the committed v2 failure evidence exactly.

- [ ] **Step 4: Pass all fault boundaries using synthetic fixtures only**

Run: `python3 -m unittest tests.physics_validation.test_confirmation_transaction tests.physics_validation.test_phase3_v2_confirmation tests.physics_validation.test_validation_run -v && ! rg -n "reference_data/(derby_fuller_1999|han_2005)" tests/physics_validation/test_confirmation_transaction.py tests/physics_validation/fixtures/confirmation_transaction_v1`

Expected: tests pass, only one concurrent launch occurs, every post-reservation fault blocks replay, and transaction tests contain no real confirmation path.

- [ ] **Step 5: Commit transaction hardening before selecting v3**

```bash
git add tools/physics_validation/confirmation_transaction.py tools/physics_validation/holdout_access.py tools/physics_validation/validation_run.py tools/physics_validation/confirmation_run.py tests/physics_validation/test_confirmation_transaction.py tests/physics_validation/fixtures/confirmation_transaction_v1 tests/physics_validation/test_phase3_v2_confirmation.py
git commit -m "feat: reserve confirmation before package access"
```

### Task 5: Select the integrated v3 production candidate

**Files:**
- Create: `physics_models/profiles/chinese_pool_full_game_v3.json`
- Create: `physics_models/promotion/phase3_candidates_v3.json`
- Create: `physics_models/promotion/full_game_matrix_v3.json`
- Create: `physics_models/promotion/full_game_performance_budget_v3.json`
- Create: `tools/physics_validation/build_v3_profile.py`
- Create: `src/Billiards/generated/phase3_v3_profile.inc`
- Modify: `src/Billiards/physics_profile.cpp`
- Modify: `tools/physics_validation/freeze_candidate.py`
- Modify: `tools/physics_validation/generate_full_game_baseline.py`
- Create: `tests/physics_validation/test_phase3_v3_candidate.py`

**Interfaces:**
- Produces: runtime profile ID `chinese_pool_full_game_v3`, inventory candidate ID `phase3_integrated_v3`, and generic freeze/full-game generators parameterized by inventory/profile IDs.
- Consumes: v3 fit artifacts, unchanged cue/pocket/solver evidence, Derby/Han manifest/metric contracts, and unchanged 12-case acceptance semantics.

- [ ] **Step 1: Write failing profile, provenance, and inventory-completeness tests**

```python
def test_runtime_default_is_exact_v3_profile(self):
    emitted = json.loads(run_billiards("--print-physics-profile"))
    committed = json.loads(PROFILE.read_text())["runtime_profile"]
    self.assertEqual(emitted, committed)

def test_inventory_contains_every_pre_freeze_artifact(self):
    inventory = load_inventory(INVENTORY)
    self.assertEqual(inventory["candidate_id"], "phase3_integrated_v3")
    self.assertEqual({p["package_id"] for p in inventory["confirmation_packages"]},
                     {"derby_fuller_1999", "han_2005"})
    assert_inventory_hashes(ROOT, inventory)
```

- [ ] **Step 2: Build and observe the default still emits v2**

Run: `cmake --build build -j2 && python3 -m unittest tests.physics_validation.test_phase3_v3_candidate -v`

Expected: FAIL because the v3 profile/inventory do not exist and the executable emits `chinese_pool_full_game_v2`.

- [ ] **Step 3: Assemble v3 profile and remove v2 hard-coding from generators**

```python
def build_v3_profile(v2_profile, ball_fit, cushion_fit):
    result = copy.deepcopy(v2_profile)
    runtime = result["runtime_profile"]
    runtime["id"] = "chinese_pool_full_game_v3"
    runtime["ball"]["normal_restitution"] = ball_fit["normal_restitution"]
    runtime["ball"]["friction_coefficient"] = ball_fit["friction_coefficient"]
    runtime["cushion"]["restitution_intercept"] = cushion_fit["e_intercept"]
    runtime["cushion"]["restitution_slope_per_mps"] = cushion_fit["e_slope"]
    runtime["cushion"]["minimum_restitution"] = cushion_fit["e_min"]
    runtime["cushion"]["maximum_restitution"] = cushion_fit["e_max"]
    return result
```

`build_v3_profile.py` writes both canonical profile JSON and a generated C++ include from the same numeric document. `physics_profile.cpp` includes `phase3_v3_profile.inc`, eliminating hand-copied constants. Generate the inventory from an explicit artifact list; do not glob confirmation output directories. Copy v2 matrix/budget semantics under v3 IDs and hashes without loosening limits.

- [ ] **Step 4: Run complete pre-freeze verification with no confirmation output**

Run: `cmake --build build -j2 && ctest --test-dir build --output-on-failure && python3 -m unittest tests.physics_validation.test_phase3_v3_candidate tests.physics_validation.test_phase3_v3_fit tests.physics_validation.test_confirmation_transaction -v && test ! -e physics_models/candidates/phase3_integrated_v3/confirmation_consumption.json`

Expected: all tests pass; executable/profile match; no v3 confirmation ledger/output exists.

- [ ] **Step 5: Commit the sole selected source revision**

```bash
git add physics_models/profiles/chinese_pool_full_game_v3.json physics_models/promotion/phase3_candidates_v3.json physics_models/promotion/full_game_matrix_v3.json physics_models/promotion/full_game_performance_budget_v3.json tools/physics_validation/build_v3_profile.py src/Billiards/generated/phase3_v3_profile.inc src/Billiards/physics_profile.cpp tools/physics_validation/freeze_candidate.py tools/physics_validation/generate_full_game_baseline.py tests/physics_validation/test_phase3_v3_candidate.py
git commit -m "feat: select integrated phase 3 candidate v3"
```

### Task 6: Freeze v3 with two reproducible clean builds

**Files:**
- Create: `tools/physics_validation/rebuild_frozen.py`
- Create: `physics_models/candidates/phase3_integrated_v3/freeze.json`
- Create: `tests/physics_validation/test_phase3_v3_freeze.py`

**Interfaces:**
- Produces: schema-v2 freeze bound to the exact Task 5 source revision, two identical clean executable hashes, two identical emitted-profile hashes, a canonical stable-path build recipe, and the complete pre-freeze inventory. `rebuild_frozen(freeze) -> FrozenBuild` recreates that exact path/hash without reading confirmation data.
- Consumes: `freeze_phase3_candidate(root, source_revision, inventory, output, jobs=2)` using one stable temporary checkout path.

- [ ] **Step 1: Write failing v3 freeze tests**

```python
def test_freeze_binds_selected_revision_and_two_clean_builds(self):
    freeze = json.loads(FREEZE.read_text())
    self.assertEqual(freeze["candidate_id"], "phase3_integrated_v3")
    self.assertEqual(len(set(freeze["clean_build_sha256"])), 1)
    self.assertEqual(len(set(freeze["clean_profile_sha256"])), 1)
    self.assertEqual(freeze["source_revision"], SELECTED_REVISION)
```

- [ ] **Step 2: Run freeze tests before the artifact exists**

Run: `python3 -m unittest tests.physics_validation.test_phase3_v3_freeze -v`

Expected: FAIL because `freeze.json` does not exist.

- [ ] **Step 3: Generate the freeze from the exact Task 5 commit**

Run: `selected=$(git rev-parse HEAD) && python3 -m tools.physics_validation.freeze_candidate --phase3-inventory physics_models/promotion/phase3_candidates_v3.json --two-clean-builds --source-revision "$selected" --jobs 2 --output physics_models/candidates/phase3_integrated_v3/freeze.json`

Expected: two clean builds and canonical profile outputs have identical SHA-256 values; freeze records the temporary-root-relative stable checkout leaf, CMake generator, Release configuration, and executable relative path; stable temporary worktree is removed.

- [ ] **Step 4: Verify freeze inventory and unopened confirmation state**

Run: `python3 -m unittest tests.physics_validation.test_phase3_v3_freeze tests.physics_validation.test_phase3_v3_candidate -v && test ! -e physics_models/candidates/phase3_integrated_v3/confirmation && test ! -e physics_models/candidates/phase3_integrated_v3/confirmation_consumption.json`

Expected: all tests pass and no confirmation state exists.

- [ ] **Step 5: Commit the immutable freeze**

```bash
git add tools/physics_validation/rebuild_frozen.py physics_models/candidates/phase3_integrated_v3/freeze.json tests/physics_validation/test_phase3_v3_freeze.py
git commit -m "release: freeze integrated phase 3 candidate v3"
```

### Task 7: Preserve complete frozen v3 full-game evidence

**Files:**
- Create: `physics_models/candidates/phase3_integrated_v3/full_game/{cadence_equivalence,continuous_scoring,cue_ball_scratch,cue_center_hit,cue_near_miscue,host_load_equivalence,oblique_ball_collision,rail_rebound,randomized_legal_sequence,seeded_break,side_pocket_capture,sliding_to_rolling}/{index.csv,summary.json,trace.json}`
- Create: `physics_models/candidates/phase3_integrated_v3/full_game/{index.csv,matrix_summary.json}`
- Create: `physics_models/promotion/full_game_performance_baseline_v3.json`
- Create: `physics_models/promotion/full_game_stress_v3.csv`
- Create: `tests/physics_validation/test_phase3_v3_full_game_run.py`

**Interfaces:**
- Produces: complete 12-case scenario/trace/index/summary outputs and hash-bound baseline/CSV for the frozen executable.
- Consumes: frozen source revision/executable, `full_game_matrix_v3.json`, and unchanged v3 performance budget.

- [ ] **Step 1: Write failing completeness and frozen-source tests**

```python
def test_all_twelve_cases_preserve_complete_v3_traces(self):
    matrix = json.loads(MATRIX.read_text())
    self.assertEqual(len(matrix["cases"]), 12)
    for case in matrix["cases"]:
        trace = load_trace(OUTPUT / case["id"] / "trace.json")
        self.assertTrue(trace["frames"])
        self.assertEqual({f["physics_profile_id"] for f in trace["frames"]},
                         {"chinese_pool_full_game_v3"})

def test_baseline_binds_frozen_executable_and_source(self):
    self.assertEqual(BASELINE["executable_sha256"], FREEZE["executable_sha256"])
    self.assertEqual(BASELINE["runner_source_revision"], FREEZE["source_revision"])
```

- [ ] **Step 2: Run tests before v3 full-game evidence exists**

Run: `python3 -m unittest tests.physics_validation.test_phase3_v3_full_game_run -v`

Expected: FAIL because the output tree and baseline do not exist.

- [ ] **Step 3: Build the frozen revision and execute the matrix twice**

Run: `frozen=$(python3 -m tools.physics_validation.rebuild_frozen --freeze physics_models/candidates/phase3_integrated_v3/freeze.json --print-build-dir) && "$frozen/full-game-stress" --matrix physics_models/promotion/full_game_matrix_v3.json --write physics_models/candidates/phase3_integrated_v3/full_game && "$frozen/full-game-stress" --matrix physics_models/promotion/full_game_matrix_v3.json --write build/phase3-v3-repeat`

Expected: both matrices pass, deterministic hashes match case-for-case, and the executable hash equals the freeze.

- [ ] **Step 4: Generate baseline/CSV and verify every budget**

Run: `frozen=$(python3 -m tools.physics_validation.rebuild_frozen --freeze physics_models/candidates/phase3_integrated_v3/freeze.json --print-build-dir) && python3 -m tools.physics_validation.generate_full_game_baseline --freeze physics_models/candidates/phase3_integrated_v3/freeze.json --matrix physics_models/promotion/full_game_matrix_v3.json --budget physics_models/promotion/full_game_performance_budget_v3.json --output-root physics_models/candidates/phase3_integrated_v3/full_game --runner "$frozen/full-game-stress" --baseline physics_models/promotion/full_game_performance_baseline_v3.json --csv physics_models/promotion/full_game_stress_v3.csv && python3 -m unittest tests.physics_validation.test_phase3_v3_full_game_run tests.physics_validation.test_full_game_performance -v`

Expected: all tests and budgets pass; all numeric traces remain committed at full precision.

- [ ] **Step 5: Commit full-game evidence**

```bash
git add physics_models/candidates/phase3_integrated_v3/full_game physics_models/promotion/full_game_performance_baseline_v3.json physics_models/promotion/full_game_stress_v3.csv tests/physics_validation/test_phase3_v3_full_game_run.py
git commit -m "data: preserve frozen phase 3 v3 full-game results"
```

### Task 8: Check readiness and execute Derby exactly once

**Files:**
- Create: `tools/physics_validation/confirmation_readiness.py`
- Create: `physics_models/candidates/phase3_integrated_v3/confirmation_readiness.json`
- Create conditionally: `physics_models/candidates/phase3_integrated_v3/confirmation/derby_fuller_1999/{metrics.csv,source_scalars.csv,reference_report.json,validation_receipt.json}`
- Create conditionally: `physics_models/candidates/phase3_integrated_v3/confirmation/derby_fuller_1999/{scenarios,traces,provenance}/derby_head_on_collision.json`
- Create: `physics_models/candidates/phase3_integrated_v3/confirmation_consumption.json`
- Create: `tests/physics_validation/test_phase3_v3_confirmation.py`

**Interfaces:**
- Produces: `build_readiness(root, freeze, inventory, full_game) -> dict`, immutable first Derby attempt, complete output, receipt, and consumption record.
- Consumes: exact frozen executable and Derby package exactly once.

- [ ] **Step 1: Generate and review the non-consuming readiness report**

Run: `python3 -m tools.physics_validation.confirmation_readiness --freeze physics_models/candidates/phase3_integrated_v3/freeze.json --inventory physics_models/promotion/phase3_candidates_v3.json --full-game physics_models/candidates/phase3_integrated_v3/full_game --output physics_models/candidates/phase3_integrated_v3/confirmation_readiness.json && python3 -m unittest tests.physics_validation.test_phase3_v3_confirmation -v`

Expected: readiness says `READY`, proves both confirmation partitions have no attempt, and does not read package content or create a ledger.

- [ ] **Step 2: Commit a temporary pre-execution checkpoint and request explicit approval**

```bash
git add tools/physics_validation/confirmation_readiness.py physics_models/candidates/phase3_integrated_v3/confirmation_readiness.json tests/physics_validation/test_phase3_v3_confirmation.py
git commit -m "data: preserve phase 3 v3 Derby confirmation"
```

Expected: stop here until the user explicitly approves the sole Derby transaction.

- [ ] **Step 3: Execute the frozen Derby partition exactly once**

Run exactly once: `frozen=$(python3 -m tools.physics_validation.rebuild_frozen --freeze physics_models/candidates/phase3_integrated_v3/freeze.json --print-build-dir) && python3 -m tools.physics_validation.validation_run --freeze physics_models/candidates/phase3_integrated_v3/freeze.json --package-key derby_fuller_1999 --ledger physics_models/candidates/phase3_integrated_v3/confirmation_consumption.json --executable "$frozen/Billiards" --output physics_models/candidates/phase3_integrated_v3/confirmation/derby_fuller_1999`

Expected: one `STARTED`, one final record, one hash-bound receipt, and complete scenarios/traces/metrics/provenance. Never rerun this command.

- [ ] **Step 4: Validate and amend immutable Derby evidence into the task commit**

Run: `python3 -m unittest tests.physics_validation.test_phase3_v3_confirmation -v && python3 -m tools.physics_validation.holdout_access --freeze physics_models/candidates/phase3_integrated_v3/freeze.json --package-key derby_fuller_1999 --ledger physics_models/candidates/phase3_integrated_v3/confirmation_consumption.json; test $? -ne 0`

If receipt result is `FAILED`, add the unchanged evidence, amend the temporary commit, stop the plan, and mark v3 rejected. If it is `PASSED_OR_ACCOUNTED`, amend the evidence and continue.

```bash
git add physics_models/candidates/phase3_integrated_v3/confirmation/derby_fuller_1999 physics_models/candidates/phase3_integrated_v3/confirmation_consumption.json
git commit --amend --no-edit
```

### Task 9: Execute Han exactly once after Derby passes

**Files:**
- Create conditionally: `physics_models/candidates/phase3_integrated_v3/confirmation/han_2005/{metrics.csv,source_scalars.csv,reference_report.json,validation_receipt.json}`
- Create conditionally: `physics_models/candidates/phase3_integrated_v3/confirmation/han_2005/{scenarios,traces,provenance}/{han_speed_050,han_speed_100,han_speed_150,han_speed_200,han_speed_250}.json`
- Modify: `physics_models/candidates/phase3_integrated_v3/confirmation_consumption.json`
- Modify: `tests/physics_validation/test_phase3_v3_confirmation.py`

**Interfaces:**
- Produces: immutable Han trace/metric/provenance output and second receipt; test helpers `result_for(ledger, dataset_id)` and `attempt_for(ledger, dataset_id)` read committed state without opening packages.
- Consumes: frozen executable and Han confirmation package exactly once, only after a passing Derby receipt.

- [ ] **Step 1: Add a pre-execution test that requires passing Derby and unopened Han**

```python
def test_han_can_open_only_after_derby_passed(self):
    ledger = load_ledger(LEDGER)
    self.assertEqual(result_for(ledger, "derby_fuller_1999"),
                     "PASSED_OR_ACCOUNTED")
    self.assertIsNone(attempt_for(ledger, "han_2005"))
    self.assertEqual(validate_confirmation_access_from_freeze(
        ROOT, FREEZE, "han_2005", LEDGER), [])
```

- [ ] **Step 2: Run the precondition test without evaluating Han**

Run: `python3 -m unittest tests.physics_validation.test_phase3_v3_confirmation.Phase3V3ConfirmationTests.test_han_can_open_only_after_derby_passed -v`

Expected: PASS only when Derby is passing and Han has no attempt.

- [ ] **Step 3: Execute the frozen Han partition exactly once**

Run exactly once: `frozen=$(python3 -m tools.physics_validation.rebuild_frozen --freeze physics_models/candidates/phase3_integrated_v3/freeze.json --print-build-dir) && python3 -m tools.physics_validation.validation_run --freeze physics_models/candidates/phase3_integrated_v3/freeze.json --package-key han_2005 --ledger physics_models/candidates/phase3_integrated_v3/confirmation_consumption.json --executable "$frozen/Billiards" --output physics_models/candidates/phase3_integrated_v3/confirmation/han_2005`

Expected: one Han attempt/final record and complete absolute diagnostics plus hard normalized-curve/constitutive metrics. Never rerun this command.

- [ ] **Step 4: Validate replay denial and commit the first result**

Run: `python3 -m unittest tests.physics_validation.test_phase3_v3_confirmation -v && python3 -m tools.physics_validation.holdout_access --freeze physics_models/candidates/phase3_integrated_v3/freeze.json --package-key han_2005 --ledger physics_models/candidates/phase3_integrated_v3/confirmation_consumption.json; test $? -ne 0`

If receipt is `FAILED`, commit unchanged evidence and stop release. Otherwise commit the passing evidence.

```bash
git add tests/physics_validation/test_phase3_v3_confirmation.py physics_models/candidates/phase3_integrated_v3/confirmation/han_2005 physics_models/candidates/phase3_integrated_v3/confirmation_consumption.json
git commit -m "data: preserve phase 3 v3 Han confirmation"
```

### Task 10: Generate exhaustive comparison and strict v3 release

**Files:**
- Create: `tools/physics_validation/compare_phase3.py`
- Modify: `tools/physics_validation/validation_artifacts.py`
- Modify: `tools/physics_validation/phase3_release_gate.py`
- Modify: `scripts/check_phase3_physics_release.py`
- Create: `tests/physics_validation/test_phase3_v3_comparison.py`
- Modify: `tests/physics_validation/test_phase3_release_gate.py`
- Create: `physics_models/promotion/phase3_v1_v2_v3_comparison.json`
- Create: `docs/phase3-v1-v2-v3-comparison.md`
- Create: `physics_models/promotion/phase3_validation_artifacts_v3.json`
- Create: `physics_models/promotion/phase3_release_v3.json`

**Interfaces:**
- Produces: `build_comparison(root, candidate) -> dict`, `all_expected_point_ids(root) -> set[str]`, exhaustive point/case comparison, complete artifact inventory, and the only acceptable release status `PASSED`.
- Consumes: immutable v1/v2 evidence, v3 fit/full-game/confirmation evidence, freeze, budgets, source ancestry, executable, and profile.

- [ ] **Step 1: Write failing comparison completeness and release-gate tests**

```python
def test_comparison_contains_every_historical_and_v3_point(self):
    comparison = build_comparison(ROOT, candidate="phase3_integrated_v3")
    self.assertEqual(set(comparison["experimental_points"]),
                     all_expected_point_ids(ROOT))
    self.assertEqual(comparison["candidates"]["phase3_integrated_v2"]["status"],
                     "REJECTED")

def test_release_accepts_only_two_passing_confirmation_receipts(self):
    failures = validate_phase3_release(ROOT, RELEASE, executable=EXECUTABLE)
    self.assertEqual(failures, [])
```

- [ ] **Step 2: Run release tests before v3 artifacts exist**

Run: `python3 -m unittest tests.physics_validation.test_phase3_v3_comparison tests.physics_validation.test_phase3_release_gate -v`

Expected: FAIL because comparison, inventory, and release manifest do not exist.

- [ ] **Step 3: Generate comparison, inventory, and release from committed evidence**

```python
release = {
    "schema_version": 3,
    "candidate_id": "phase3_integrated_v3",
    "status": "PASSED",
    "source_revision": freeze["source_revision"],
    "executable_sha256": freeze["executable_sha256"],
    "confirmation_receipts": hashed_receipts(expected={
        "derby_fuller_1999", "han_2005"}),
    "unexplained_regressions": 0,
}
```

Inventory every fit input/output, structural residual, profile, freeze, full-game file, confirmation file, comparison file, transition/rejection record, and budget. Refuse generation unless both receipts pass, all 12 full-game cases pass, hashes match, source revision is an ancestor, and executable emits `chinese_pool_full_game_v3`.

- [ ] **Step 4: Run full clean-build and release verification**

Run: `git diff --check && cmake --build build -j2 && ctest --test-dir build --output-on-failure && python3 -m unittest discover -s tests/physics_validation -p 'test_*.py' -v && python3 scripts/check_phase3_physics_release.py --release physics_models/promotion/phase3_release_v3.json --executable build/Billiards`

Expected: every C++/Python test passes, checker prints `phase3_release_v3: PASSED`, and no real confirmation runner is invoked by tests or CI.

- [ ] **Step 5: Commit final Phase 3 release evidence**

```bash
git add tools/physics_validation/compare_phase3.py tools/physics_validation/validation_artifacts.py tools/physics_validation/phase3_release_gate.py scripts/check_phase3_physics_release.py tests/physics_validation/test_phase3_v3_comparison.py tests/physics_validation/test_phase3_release_gate.py physics_models/promotion/phase3_v1_v2_v3_comparison.json docs/phase3-v1-v2-v3-comparison.md physics_models/promotion/phase3_validation_artifacts_v3.json physics_models/promotion/phase3_release_v3.json
git commit -m "release: accept strict phase 3 physics v3"
```

## Plan Verification

- Confirm Task 1 changes no v2 candidate or Sudo package byte.
- Confirm every v3 fitter input is `spent` and neither Derby nor Han is imported.
- Confirm Task 4 reserves before evaluator package access under every injected fault and concurrent launch.
- Confirm Task 6 freezes the exact Task 5 revision with two matching clean builds/profile outputs.
- Confirm Task 7 commits complete full-precision output for all 12 game cases.
- Stop for explicit approval immediately before the sole Derby execution.
- Never execute Han unless the committed Derby receipt passes.
- Stop release work on either failed/interrupted receipt and commit unchanged failure evidence.
- Confirm final release says exactly `PASSED`, inventories every artifact, binds the frozen executable/profile/source, and can be verified without rerunning confirmation.
- Request a final independent code review; any blocking finding prevents Phase 3 completion.
