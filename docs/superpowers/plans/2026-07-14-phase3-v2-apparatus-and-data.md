# Phase 3 v2 Experimental Apparatus and Data Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Commit every usable experimental scalar and correct the scenario/apparatus integrations so v2 calibration and confirmation measure the intended physical event.

**Architecture:** Extend reference packages with source-scalar provenance and explicit lifecycle roles while keeping publications out of Git. Add boundary mode and initial-state validation to the C++ scenario contract, then make adapters select telemetry by motion phase and solver event rather than fixed ticks.

**Tech Stack:** Python 3, CSV/JSON, `unittest`, C++17, CMake/CTest, existing automation JSON and physics trace formats.

## Global Constraints

- Existing v1 HOLDOUT outputs are marked `spent` and are never executed again.
- Sudo 2002 and Derby-Fuller 1999 PDFs, images, and figures are not committed.
- Every used scalar, conversion, uncertainty, locator, audited-source SHA-256, and output hash is committed.
- `production_table` is the default boundary mode; only open-bench packages may select `unbounded`.
- Invalid initial geometry returns `INTEGRATION_MISMATCH` before the first tick.
- Each task below ends in one independently reviewable commit.

---

### Task 1: Record spent v1 evidence and confirmation-only source packages

**Files:**
- Modify: `tests/physics_validation/validation_data_status.json`
- Modify: `tools/physics_validation/data_lifecycle.py`
- Modify: `tests/physics_validation/test_reference_accounting.py`
- Create: `tests/physics_validation/reference_data/sudo_2002/{manifest.json,extraction.json,raw_extracted.csv,normalized.csv,scalars.csv,split.json,scenario_template.json,expected_model_mismatches.json,expected_reference_limitations.json,source_access_audit.json}`
- Create: `tests/physics_validation/reference_data/derby_fuller_1999/{manifest.json,extraction.json,raw_extracted.csv,normalized.csv,scalars.csv,split.json,scenario_template.json,expected_model_mismatches.json,expected_reference_limitations.json,source_access_audit.json}`
- Create: `tests/physics_validation/test_phase3_v2_source_packages.py`

**Interfaces:**
- Consumes: `load_data_lifecycle(path)` and `load_reference_package(path)`.
- Produces: package lifecycle `confirmation`, scalar rows with `point_id, quantity, value, unit, uncertainty, locator, applicability`, and immutable audited-source metadata.

- [ ] **Step 1: Write failing lifecycle and scalar-completeness tests**

```python
EXPECTED_SPENT = {
    "mathavan_2009_high_speed",
    "mathavan_2010_cushion",
    "domenech_2023_ball_collision",
}


def test_v1_experimental_holdouts_are_spent(self):
    registry = load_data_lifecycle(STATUS)
    actual = {item.dataset_id for item in registry.datasets if item.lifecycle == "spent"}
    self.assertTrue(EXPECTED_SPENT <= actual)

def test_confirmation_packages_commit_every_used_scalar(self):
    registry = load_data_lifecycle(STATUS)
    lifecycle_by_id = {item.dataset_id: item.lifecycle for item in registry.datasets}
    for package_id in ("sudo_2002", "derby_fuller_1999"):
        package = load_reference_package(REFERENCE_ROOT / package_id)
        scalar_ids = {row["point_id"] for row in read_csv(package.root / "scalars.csv")}
        normalized_ids = {row["point_id"] for row in read_csv(package.normalized_path)}
        self.assertEqual(scalar_ids, normalized_ids)
        self.assertEqual(lifecycle_by_id[package_id], "confirmation")
```

- [ ] **Step 2: Run tests and verify missing lifecycle/package support**

Run: `python3 -m unittest tests.physics_validation.test_phase3_v2_source_packages tests.physics_validation.test_reference_accounting -v`

Expected: failures because `confirmation` and the two packages are absent.

- [ ] **Step 3: Extend lifecycle validation and add deterministic packages**

```python
VALID_LIFECYCLES = {"calibration", "spent", "validation", "confirmation"}


def _validate_lifecycle(value):
    if value not in VALID_LIFECYCLES:
        raise ValueError(f"unsupported dataset lifecycle: {value}")
    return value
```

Populate Sudo scalar IDs `cushion_e_low_speed`, `cushion_e_all_speed`, `cushion_contact_time_plateau`, `ball_ball_e_head_on`, `separation_angle_mean`, and `transverse_momentum_deficit`. Populate Derby scalar IDs `initial_speed`, both diagnostic accelerations, both slide times, both final speeds, pre/post momentum, and kinetic_energy_loss`. Store SI conversions at full decimal precision and explicit rounding/engineering uncertainty fields. Set both splits to `confirmation` and set `candidate_selection_input` to `false`.

- [ ] **Step 4: Verify hashes and copyright exclusions**

Run: `python3 -m unittest tests.physics_validation.test_phase3_v2_source_packages tests.physics_validation.test_reference_package tests.physics_validation.test_reference_accounting -v && ! find tests/physics_validation/reference_data/sudo_2002 tests/physics_validation/reference_data/derby_fuller_1999 -type f | rg '\.(pdf|png|jpg|jpeg|tif)$'`

Expected: all tests pass and no copyrighted publication/image file is found.

- [ ] **Step 5: Commit lifecycle and complete scalar evidence**

```bash
git add tests/physics_validation/validation_data_status.json tools/physics_validation/data_lifecycle.py tests/physics_validation/test_reference_accounting.py tests/physics_validation/test_phase3_v2_source_packages.py tests/physics_validation/reference_data/sudo_2002 tests/physics_validation/reference_data/derby_fuller_1999
git commit -m "data: admit phase 3 v2 confirmation scalars"
```

### Task 2: Add explicit scenario boundary modes and trace identity

**Files:**
- Modify: `src/Billiards/physics_scenario.h`
- Modify: `src/Billiards/physics_scenario.cpp`
- Modify: `src/Billiards/physics_telemetry.h`
- Modify: `src/Billiards/physics_telemetry.cpp`
- Modify: `src/Billiards/automation_json.cpp`
- Modify: `tests/physics_scenario_tests.cpp`
- Modify: `tests/physics_telemetry_tests.cpp`

**Interfaces:**
- Produces: `enum class PhysicsBoundaryMode { ProductionTable, Unbounded }`, `PhysicsScenario::boundaryMode`, and serialized frame field `boundary_mode`.
- Consumes: `applyPhysicsScenario(GameRuntime&, const PhysicsScenario&)` and `capturePhysicsFrame(...)`.

- [ ] **Step 1: Write failing parser and telemetry tests**

```cpp
const auto unbounded = parsePhysicsScenario(json::parse(R"({
  "schema_version":9,"id":"open_bench","description":"bench",
  "evidence_grade":"A","evidence_source":"source","equipment":"bench",
  "boundary_mode":"unbounded","ticks":3,"time_step_seconds":0.001,
  "balls":[],"expectations":[]
})"));
expect(unbounded.ok, "unbounded mode parses");
expect(unbounded.scenario.boundaryMode == PhysicsBoundaryMode::Unbounded,
       "unbounded mode is retained");

const auto unknown = parsePhysicsScenario(withBoundaryMode("periodic"));
expect(!unknown.ok && unknown.errorCode == "INVALID_BOUNDARY_MODE",
       "unknown boundary mode fails closed");
```

- [ ] **Step 2: Build and run focused C++ tests**

Run: `cmake --build build --target physics-scenario-tests physics-telemetry-tests -j2 && ctest --test-dir build -R 'physics-scenario|physics-telemetry' --output-on-failure`

Expected: compile/test failure because schema 9 and `PhysicsBoundaryMode` do not exist.

- [ ] **Step 3: Implement schema 9 and serialized boundary identity**

```cpp
enum class PhysicsBoundaryMode { ProductionTable, Unbounded };

struct PhysicsScenario {
    int schemaVersion = 9;
    PhysicsBoundaryMode boundaryMode = PhysicsBoundaryMode::ProductionTable;
    // Retain the existing fields unchanged below this member.
};

const char* physicsBoundaryModeName(PhysicsBoundaryMode mode) {
    return mode == PhysicsBoundaryMode::Unbounded ? "unbounded" : "production_table";
}
```

Parser behavior: missing `boundary_mode` maps to `ProductionTable`; exact strings `production_table` and `unbounded` are accepted; all other values return `INVALID_BOUNDARY_MODE`. Store the selected mode in `GameRuntime`, copy it into every `PhysicsFrame`, and emit it as `boundary_mode` in automation JSON.

- [ ] **Step 4: Run parser, telemetry, and automation regressions**

Run: `cmake --build build --target physics-scenario-tests physics-telemetry-tests automation-json-tests -j2 && ctest --test-dir build -R 'physics-scenario|physics-telemetry|automation-json' --output-on-failure`

Expected: all selected tests pass.

- [ ] **Step 5: Commit boundary-mode support**

```bash
git add src/Billiards/physics_scenario.h src/Billiards/physics_scenario.cpp src/Billiards/physics_telemetry.h src/Billiards/physics_telemetry.cpp src/Billiards/automation_json.cpp tests/physics_scenario_tests.cpp tests/physics_telemetry_tests.cpp
git commit -m "feat: add explicit physics apparatus boundaries"
```

### Task 3: Validate initial geometry before applying a scenario

**Files:**
- Create: `src/Billiards/scenario_geometry.h`
- Create: `src/Billiards/scenario_geometry.cpp`
- Modify: `src/Billiards/physics_scenario.cpp`
- Modify: `CMakeLists.txt`
- Create: `tests/scenario_geometry_tests.cpp`
- Modify: `tests/physics_scenario_tests.cpp`

**Interfaces:**
- Produces: `ScenarioGeometryResult validateScenarioGeometry(const PhysicsScenario&)` with stable codes `NONFINITE_BALL`, `BALL_OVERLAP`, and `OUTSIDE_APPARATUS`.
- Consumes: ball radius/table dimensions from `PhysicsProfile`, active/pocketed ball state, `boundaryMode`, and optional `initial_contact_epsilon_cm`.

- [ ] **Step 1: Write failing finite, overlap, and domain tests**

```cpp
PhysicsScenario scenario = validScenario();
scenario.balls[0].position.x = std::numeric_limits<float>::infinity();
expect(validateScenarioGeometry(scenario).code == "NONFINITE_BALL", "finite positions required");

scenario = validScenario();
scenario.balls[1].position = scenario.balls[0].position;
expect(validateScenarioGeometry(scenario).code == "BALL_OVERLAP", "overlap rejected");

scenario = validScenario();
scenario.balls[0].position.x = scenario.physicsProfile.tableBoundary.playfieldWidthCm;
expect(validateScenarioGeometry(scenario).code == "OUTSIDE_APPARATUS", "production bounds enforced");
scenario.boundaryMode = PhysicsBoundaryMode::Unbounded;
expect(validateScenarioGeometry(scenario).ok, "unbounded bench has no rail domain");
```

- [ ] **Step 2: Build and observe the missing validator**

Run: `cmake --build build --target scenario-geometry-tests -j2`

Expected: compile failure for missing `scenario_geometry.h`.

- [ ] **Step 3: Implement deterministic preflight validation**

```cpp
struct ScenarioGeometryResult {
    bool ok = false;
    std::string code;
    int firstBall = -1;
    int secondBall = -1;
};

ScenarioGeometryResult validateScenarioGeometry(const PhysicsScenario& scenario) {
    const double diameter = 2.0 * scenario.physicsProfile.ball.radiusCm;
    for (int i = 0; i < kBallCount; ++i) {
        if (scenario.balls[i].pocketed) continue;
        if (!finiteBall(scenario.balls[i])) return {false, "NONFINITE_BALL", i, -1};
        if (scenario.boundaryMode == PhysicsBoundaryMode::ProductionTable &&
            !insidePlayfield(scenario.balls[i].position, scenario.physicsProfile))
            return {false, "OUTSIDE_APPARATUS", i, -1};
        for (int j = i + 1; j < kBallCount; ++j) {
            if (scenario.balls[j].pocketed) continue;
            if (distance(scenario.balls[i].position, scenario.balls[j].position) <
                diameter - scenario.initialContactEpsilonCm)
                return {false, "BALL_OVERLAP", i, j};
        }
    }
    return {true, "", -1, -1};
}
```

Call the validator from `applyPhysicsScenario` before mutating runtime state. Map any failure to `ActionResult{false, "INTEGRATION_MISMATCH", geometry.code}`.

- [ ] **Step 4: Run geometry and scenario tests**

Run: `cmake --build build --target scenario-geometry-tests physics-scenario-tests -j2 && ctest --test-dir build -R 'scenario-geometry|physics-scenario' --output-on-failure`

Expected: all selected tests pass and invalid scenarios leave runtime unchanged.

- [ ] **Step 5: Commit scenario preflight validation**

```bash
git add src/Billiards/scenario_geometry.h src/Billiards/scenario_geometry.cpp src/Billiards/physics_scenario.cpp CMakeLists.txt tests/scenario_geometry_tests.cpp tests/physics_scenario_tests.cpp
git commit -m "feat: reject invalid physics scenario geometry"
```

### Task 4: Select surface and collision metrics by physical phase/event

**Files:**
- Modify: `tools/physics_validation/adapters/mathavan_2009.py`
- Modify: `tools/physics_validation/adapters/domenech_2023.py`
- Modify: `tools/physics_validation/analyzer.py`
- Modify: `tests/physics_validation/test_mathavan_2009_adapter.py`
- Modify: `tests/physics_validation/test_domenech_2023_adapter.py`
- Modify: `tests/physics_validation/test_analyzer.py`

**Interfaces:**
- Produces: `maximal_phase_segment(frames, ball_index, phase)`, `interpolated_transition_time(frames, ball_index)`, and `contacts_for_solver_event(frames, event_id)`.
- Consumes: `balls[*].motion_state`, `surface_transitions`, and `contacts[*].solver_event_id` from traces.

- [ ] **Step 1: Write failing cross-phase and duplicate-collision tests**

```python
def test_sliding_fit_stops_at_the_transition(self):
    frames = trace_with_phases(["sliding", "sliding", "sliding", "rolling", "rolling"])
    selected = maximal_phase_segment(frames, 0, "sliding")
    self.assertEqual([frame["tick"] for frame in selected], [0, 1, 2])

def test_collision_metric_is_bound_to_one_event(self):
    frames = trace_with_collision_events([17, 29])
    contacts = contacts_for_solver_event(frames, 17)
    self.assertTrue(contacts)
    self.assertEqual({item["solver_event_id"] for item in contacts}, {17})
```

- [ ] **Step 2: Run adapter tests and observe fixed-tick/event mixing**

Run: `python3 -m unittest tests.physics_validation.test_mathavan_2009_adapter tests.physics_validation.test_domenech_2023_adapter tests.physics_validation.test_analyzer -v`

Expected: new tests fail because current adapters fit fixed ticks and do not bind a collision event.

- [ ] **Step 3: Implement contiguous phase and event selectors**

```python
def maximal_phase_segment(frames, ball_index, phase):
    segments, current = [], []
    for frame in frames:
        if frame["balls"][ball_index]["motion_state"] == phase:
            current.append(frame)
        elif current:
            segments.append(current)
            current = []
    if current:
        segments.append(current)
    selected = max(segments, key=len, default=[])
    if len(selected) < 3:
        raise IntegrationMismatch(f"fewer than three {phase} samples")
    return selected


def contacts_for_solver_event(frames, event_id):
    return [contact for frame in frames for contact in frame.get("contacts", [])
            if contact.get("solver_event_id") == event_id]
```

Interpolate transition time from the two samples bracketing `SurfaceMotionStep.transition_timestamp`. Reject a boundary contact in an `unbounded` trace and reject any collision metric that observes more than one solver event ID.

- [ ] **Step 4: Run all affected adapters**

Run: `python3 -m unittest tests.physics_validation.test_mathavan_2009_adapter tests.physics_validation.test_mathavan_2009_metrics tests.physics_validation.test_domenech_2023_adapter tests.physics_validation.test_domenech_2023_metrics tests.physics_validation.test_analyzer -v`

Expected: all tests pass; the fixture sliding acceleration is approximately `196.13296508789062 cm/s^2`.

- [ ] **Step 5: Commit phase/event-aware metrics**

```bash
git add tools/physics_validation/adapters/mathavan_2009.py tools/physics_validation/adapters/domenech_2023.py tools/physics_validation/analyzer.py tests/physics_validation/test_mathavan_2009_adapter.py tests/physics_validation/test_domenech_2023_adapter.py tests/physics_validation/test_analyzer.py
git commit -m "fix: bind validation metrics to physical phases"
```

### Task 5: Generate corrected v2 calibration scenarios without rerunning v1

**Files:**
- Modify: `tools/physics_validation/adapters/mathavan_2010.py`
- Modify: `tools/physics_validation/adapters/domenech_2023.py`
- Modify: `tests/physics_validation/test_mathavan_2010_adapter.py`
- Modify: `tests/physics_validation/test_domenech_2023_adapter.py`
- Create: `tests/physics_validation/scenarios/mathavan_2010_cushion_v2.json`
- Create: `tests/physics_validation/scenarios/domenech_2023_open_bench_v2.json`
- Create: `physics_models/calibration/phase3_v2_apparatus/{reference_points.csv,reference_report.json,reference_report.md,traces/**,provenance/**}`
- Create: `physics_models/calibration/phase3_v2_apparatus_report.json`

**Interfaces:**
- Produces: new scenario IDs suffixed `_v2`, valid initial centers, at least three approach samples, and `unbounded` Doménech apparatus.
- Consumes: committed normalized spent rows only; it does not invoke a v1 HOLDOUT runner.

- [ ] **Step 1: Write failing apparatus-construction tests**

```python
def test_fast_cushion_case_starts_inside_table_with_three_samples(self):
    scenario = build_scenario(fastest_incident_row(), time_step_seconds=0.001)
    self.assertGreaterEqual(scenario["preimpact_samples"], 3)
    self.assertLessEqual(abs(scenario["balls"][0]["position"][0]), scenario["playfield_half_width_cm"])

def test_domenech_v2_uses_open_bench_apparatus(self):
    scenario = build_domenech_scenario(reference_row())
    self.assertEqual(scenario["boundary_mode"], "unbounded")
    self.assertTrue(scenario["id"].endswith("_v2"))
```

- [ ] **Step 2: Run adapter tests and observe invalid v1 construction**

Run: `python3 -m unittest tests.physics_validation.test_mathavan_2010_adapter tests.physics_validation.test_domenech_2023_adapter -v`

Expected: failures because the Mathavan adapter reserves 0.4 seconds and Doménech scenarios retain production rails.

- [ ] **Step 3: Implement bounded approach time and open-bench mode**

```python
def approach_time_seconds(speed_m_s, distance_to_boundary_m, requested_dt):
    maximum = distance_to_boundary_m / speed_m_s
    dt = min(requested_dt, maximum / 3.0)
    samples = max(3, int(maximum / dt))
    return samples * dt, dt, samples


def apply_open_bench(scenario):
    result = dict(scenario)
    result["schema_version"] = 9
    result["boundary_mode"] = "unbounded"
    result["id"] = f"{scenario['id']}_v2"
    return result
```

Build the apparatus report from committed normalized rows and generated scenario JSON hashes. The report records original v1 failure ID, v2 scenario ID, cause code (`CROSS_PHASE_SELECTION`, `INITIAL_STATE_OUTSIDE_DOMAIN`, or `UNINTENDED_SECOND_COLLISION`), and correction. Execute the new `_v2` scenarios through `calibration_run` only, writing complete reference points, reports, traces, and provenance under `physics_models/calibration/phase3_v2_apparatus`; do not call `reference_run` or `validation_run`.

- [ ] **Step 4: Run scenario construction and static no-HOLDOUT audit**

Run: `python3 -m unittest tests.physics_validation.test_mathavan_2010_adapter tests.physics_validation.test_domenech_2023_adapter tests.physics_validation.test_mathavan_2009_adapter -v && python3 -m tools.physics_validation.calibration_run --executable build/physics-scenario --scenario tests/physics_validation/scenarios/mathavan_2010_cushion_v2.json --scenario tests/physics_validation/scenarios/domenech_2023_open_bench_v2.json --output physics_models/calibration/phase3_v2_apparatus && ! git diff --name-only ddc3b82 | rg 'candidates/.+_v1/validation'`

Expected: all adapter tests pass and no v1 validation artifact changed.

- [ ] **Step 5: Commit corrected calibration apparatus**

```bash
git add tools/physics_validation/adapters/mathavan_2010.py tools/physics_validation/adapters/domenech_2023.py tests/physics_validation/test_mathavan_2010_adapter.py tests/physics_validation/test_domenech_2023_adapter.py tests/physics_validation/scenarios/mathavan_2010_cushion_v2.json tests/physics_validation/scenarios/domenech_2023_open_bench_v2.json physics_models/calibration/phase3_v2_apparatus physics_models/calibration/phase3_v2_apparatus_report.json
git commit -m "fix: correct phase 3 v2 validation apparatus"
```

## Plan Verification

- Run `python3 -m unittest tests.physics_validation.test_phase3_v2_source_packages tests.physics_validation.test_reference_accounting tests.physics_validation.test_mathavan_2009_adapter tests.physics_validation.test_mathavan_2010_adapter tests.physics_validation.test_domenech_2023_adapter -v`.
- Run `ctest --test-dir build -R 'physics-scenario|scenario-geometry|physics-telemetry|automation-json' --output-on-failure`.
- Verify no PDF/image is tracked under either confirmation package and no v1 validation artifact differs from commit `ddc3b82`.
