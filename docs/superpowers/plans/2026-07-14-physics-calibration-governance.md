# Physics Calibration Governance Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Build the versioned physics-parameter, calibration-only, candidate-freeze, and validation-receipt foundation required before production physics can be tuned.

**Architecture:** Add an immutable C++ `PhysicsProfile` owned by `GameRuntime`, with scenario v3 overrides applied atomically. Add Python entry points that select committed calibration or validation cases by their existing partition labels, plus canonical freeze/receipt schemas that bind a candidate to the executable, profile, data packages, and reports.

**Tech Stack:** C++11, CMake/CTest, project JSON value type, Python 3 standard library and `unittest`.

## Global Constraints

- All source and data files are UTF-8; run `scripts/check_text_encoding.py` before each commit.
- Production state remains in `cm`, `s`, and `rad/s`; every new numeric field carries an explicit unit suffix.
- All numerical data used by calibration or validation remains committed and runnable offline.
- Calibration tools may consume only committed `CALIBRATION` cases and expose no split override.
- Validation requires a previously committed candidate freeze record and never changes model parameters.
- Existing scenario versions 1 and 2 and automation clients remain compatible.
- Theme 0 changes parameter plumbing and governance only; it must preserve current production physics outputs.
- Do not run `scripts/check.sh`, the full reference workflow, or any command that executes holdout cases before Task 5 freezes the candidate interface.

---

## File Structure

- Create `src/Billiards/physics_profile.h`: parameter value types, validation result, defaults, and canonical profile identity interface.
- Create `src/Billiards/physics_profile.cpp`: default Chinese Pool profile and strict validation.
- Create `tests/physics_profile_tests.cpp`: C++ parameter/default/validation tests.
- Modify `src/Billiards/game_runtime.h` and `.cpp`: runtime profile ownership and atomic scenario replacement.
- Modify `src/Billiards/physics.h` and `.cpp`: accept a profile without changing legacy calculations.
- Modify `src/Billiards/physics_scenario.h` and `.cpp`: scenario schema v3 profile parsing.
- Modify `CMakeLists.txt`: compile the profile source and test target.
- Create `tools/physics_validation/partition_run.py`: common committed-partition selection.
- Create `tools/physics_validation/calibration_run.py`: calibration-only CLI.
- Create `tests/physics_validation/test_calibration_run.py`: isolation tests.
- Create `tools/physics_validation/model_candidate.py`: freeze record and validation receipt schemas/hashes.
- Create `tools/physics_validation/data_lifecycle.py`: committed dataset lifecycle registry validation.
- Create `tools/physics_validation/freeze_candidate.py`: canonical freeze CLI.
- Create `tools/physics_validation/validation_run.py`: frozen-candidate validation CLI.
- Create `tests/physics_validation/test_model_candidate.py` and `test_validation_run.py`: schema, hash, and partition tests.
- Create `tests/physics_validation/validation_data_status.json`: reviewed calibration/validation/spent/confirmation status registry.
- Create `tests/physics_validation/fixtures/model_candidate_v1/`: minimal deterministic candidate fixture.
- Modify `docs/reference-data-packages.md`: document calibration, freeze, validation, and spent semantics.

### Task 1: Add the Versioned PhysicsProfile Value Model

**Files:**
- Create: `src/Billiards/physics_profile.h`
- Create: `src/Billiards/physics_profile.cpp`
- Create: `tests/physics_profile_tests.cpp`
- Modify: `CMakeLists.txt`

**Interfaces:**
- Produces: `PhysicsProfile defaultChinesePoolPhysicsProfile()`.
- Produces: `PhysicsProfileValidation validatePhysicsProfile(const PhysicsProfile&)`.
- Produces: `std::string canonicalPhysicsProfileText(const PhysicsProfile&)` for hashing outside the runtime.
- Consumed later by: `GameRuntime`, scenario v3, trace provenance, and surface motion.

- [ ] **Step 1: Write the failing profile tests**

Create `tests/physics_profile_tests.cpp` with assertions for finite positive mass/radius, nonnegative friction/resistance, unique safe ID, current compatibility defaults, and deterministic identity:

```cpp
#include "physics_profile.h"

#include <cstdlib>
#include <iostream>
#include <limits>

namespace {
void expect(bool condition, const char* message)
{
    if (!condition) { std::cerr << message << '\n'; std::exit(1); }
}
}

int main()
{
    const billiardgl::PhysicsProfile profile =
        billiardgl::defaultChinesePoolPhysicsProfile();
    expect(billiardgl::validatePhysicsProfile(profile).ok, "default profile is valid");
    expect(profile.id == "chinese_pool_legacy_v1", "stable default profile ID");
    expect(profile.ball.radiusCm == billiardgl::kChineseBallRadiusCm, "ball radius unit");
    expect(profile.ball.massKg == 0.17f, "legacy telemetry mass");
    expect(profile.surface.legacyFrictionAccelerationCmS2 == 4.0f,
        "theme zero preserves legacy friction");
    expect(billiardgl::canonicalPhysicsProfileText(profile) ==
        billiardgl::canonicalPhysicsProfileText(profile), "deterministic serialization");

    billiardgl::PhysicsProfile invalid = profile;
    invalid.ball.massKg = std::numeric_limits<float>::quiet_NaN();
    expect(!billiardgl::validatePhysicsProfile(invalid).ok, "nonfinite mass rejected");
    invalid = profile;
    invalid.id = "../unsafe";
    expect(!billiardgl::validatePhysicsProfile(invalid).ok, "unsafe ID rejected");
    invalid = profile;
    invalid.surface.slidingFrictionCoefficient = -0.1f;
    expect(!billiardgl::validatePhysicsProfile(invalid).ok, "negative friction rejected");
    return 0;
}
```

- [ ] **Step 2: Register and run the missing test target**

Add `src/Billiards/physics_profile.cpp` to `BILLIARDGL_CORE_SOURCES` and:

```cmake
billiardgl_add_core_test(BilliardsPhysicsProfileTests tests/physics_profile_tests.cpp)
```

Run:

```bash
cmake -S . -B /tmp/billiardgl-phase3 -DCMAKE_BUILD_TYPE=Debug
cmake --build /tmp/billiardgl-phase3 --target BilliardsPhysicsProfileTests
```

Expected: build fails because `physics_profile.h` and its functions do not exist.

- [ ] **Step 3: Implement the minimal profile types and validation**

Define focused value types in `physics_profile.h`:

```cpp
#pragma once

#include "table_specs.h"

#include <string>

namespace billiardgl {

struct BallProperties {
    float radiusCm = kChineseBallRadiusCm;
    float massKg = 0.17f;
    std::string material = "phenolic_resin";
};

struct SurfaceProperties {
    float legacyFrictionAccelerationCmS2 = 4.0f;
    float slidingFrictionCoefficient = 0.0f;
    float rollingResistanceAccelerationCmS2 = 4.0f;
    float torsionalSpinDecelerationRadS2 = 0.0f;
    float slipSpeedEpsilonCmS = 0.0001f;
    float stopEnergyThresholdJ = 0.000000001f;
    std::string material = "production_cloth_legacy";
};

struct CueProperties { float effectiveMassKg = 0.5f; };
struct CushionProperties { float normalRestitution = 1.0f; float frictionCoefficient = 0.0f; };
struct SolverSettings { float timeStepSeconds = 0.1f; int maximumEventsPerTick = 64; };

struct PhysicsProfile {
    std::string id;
    std::string formulaVersion;
    BallProperties ball;
    SurfaceProperties surface;
    CueProperties cue;
    CushionProperties cushion;
    SolverSettings solver;
};

struct PhysicsProfileValidation { bool ok = false; std::string error; };

PhysicsProfile defaultChinesePoolPhysicsProfile();
PhysicsProfileValidation validatePhysicsProfile(const PhysicsProfile& profile);
std::string canonicalPhysicsProfileText(const PhysicsProfile& profile);

}  // namespace billiardgl
```

In `physics_profile.cpp`, validate every finite/range condition and construct the canonical identity from every field with `std::setprecision(std::numeric_limits<float>::max_digits10)`. Do not use `std::hash`; return the canonical text itself at this task so identity is stable across processes.

- [ ] **Step 4: Run the focused tests**

```bash
cmake --build /tmp/billiardgl-phase3 --target BilliardsPhysicsProfileTests
ctest --test-dir /tmp/billiardgl-phase3 -R BilliardsPhysicsProfileTests --output-on-failure
```

Expected: one test passes.

- [ ] **Step 5: Commit the profile model**

```bash
git add CMakeLists.txt src/Billiards/physics_profile.h src/Billiards/physics_profile.cpp tests/physics_profile_tests.cpp
python3 scripts/check_text_encoding.py --root .
git diff --cached --check
git commit -m "feat: add versioned physics profiles"
```

### Task 2: Make GameRuntime Own and Apply PhysicsProfile Atomically

**Files:**
- Modify: `src/Billiards/game_runtime.h`
- Modify: `src/Billiards/game_runtime.cpp`
- Modify: `src/Billiards/physics.h`
- Modify: `src/Billiards/physics.cpp`
- Modify: `src/Billiards/physics_telemetry.h`
- Modify: `src/Billiards/physics_telemetry.cpp`
- Modify: `src/Billiards/automation_protocol.cpp`
- Modify: `tests/game_runtime_tests.cpp`
- Modify: `tests/physics_instrumentation_tests.cpp`
- Modify: `tests/physics_telemetry_tests.cpp`
- Modify: `tests/automation_protocol_tests.cpp`

**Interfaces:**
- Consumes: `defaultChinesePoolPhysicsProfile()` and `validatePhysicsProfile()`.
- Produces: `const PhysicsProfile& GameRuntime::physicsProfile() const`.
- Produces: `ActionResult GameRuntime::replaceStateForScenario(const GameState&, const PhysicsProfile&, const CueImpactInput*)`.
- Produces: `PhysicsStepTelemetry updatePhysics(GameState&, float, const PhysicsProfile&)`.
- Produces: `PhysicsFrame capturePhysicsFrame(..., const PhysicsProfile&)` with profile ID and profile mass.

- [ ] **Step 1: Write failing runtime ownership tests**

Add to `tests/game_runtime_tests.cpp`:

```cpp
billiardgl::GameRuntime profiled;
expect(profiled.physicsProfile().id == "chinese_pool_legacy_v1", "runtime default profile");
billiardgl::PhysicsProfile experiment = billiardgl::defaultChinesePoolPhysicsProfile();
experiment.id = "experiment_surface_v1";
experiment.ball.massKg = 0.205f;
const billiardgl::GameState replacement = profiled.state();
expect(profiled.replaceStateForScenario(replacement, experiment).ok,
    "valid profile applies atomically");
expect(profiled.physicsProfile().id == "experiment_surface_v1", "profile retained");
billiardgl::PhysicsProfile invalid = experiment;
invalid.ball.radiusCm = 0.0f;
expect(!profiled.replaceStateForScenario(replacement, invalid).ok,
    "invalid profile rejected");
expect(profiled.physicsProfile().id == "experiment_surface_v1",
    "failed replacement preserves old profile");

billiardgl::GameState before = profiled.state();
billiardgl::GameState after = before;
after.balls[0].velocity.x = 100.0f;
const billiardgl::PhysicsFrame frame = billiardgl::capturePhysicsFrame(
    1, 0.1, 0.1f, before, after, billiardgl::PhysicsStepTelemetry{}, experiment);
expect(frame.physicsProfileId == "experiment_surface_v1", "trace profile ID");
```

- [ ] **Step 2: Run the focused test and confirm interface failure**

```bash
cmake --build /tmp/billiardgl-phase3 --target BilliardsGameRuntimeTests
```

Expected: compile fails because `physicsProfile` and the profile overload do not exist.

- [ ] **Step 3: Thread the profile through runtime and physics**

Add `PhysicsProfile physicsProfile_;` to `GameRuntime`, reset it to the default in `reset()`, and pass it from `step()`:

```cpp
const PhysicsStepTelemetry telemetry =
    updatePhysics(state_, physicsProfile_.solver.timeStepSeconds, physicsProfile_);
```

Add the profile overload:

```cpp
ActionResult GameRuntime::replaceStateForScenario(
    const GameState& state, const PhysicsProfile& profile, const CueImpactInput* cueImpact)
{
    const PhysicsProfileValidation validation = validatePhysicsProfile(profile);
    if (!validation.ok) return ActionResult{false, validation.error};
    replaceState(state);
    physicsProfile_ = profile;
    tick_ = 0;
    nextSequence_ = 1;
    events_.clear();
    clearGameplayEvents(state_);
    physicsTrace_.clear();
    hasCueImpactInput_ = cueImpact != nullptr;
    cueImpactInput_ = cueImpact ? *cueImpact : CueImpactInput{};
    return ActionResult{};
}
```

Keep the old overload and `updatePhysics(GameState&, float)` as wrappers using the default profile. Inside theme 0, `physics.cpp` must continue using `profile.surface.legacyFrictionAccelerationCmS2` so every existing golden result is unchanged.

Add a profile-aware `capturePhysicsFrame` overload. It calculates momentum and energy with `profile.ball.massKg`, stores `physicsProfileId`, and is called by `GameRuntime::step`. Keep the old capture overload as a default-profile wrapper for existing callers. Serialize the profile ID at frame level; the freeze/report metadata carries the SHA-256 of the complete profile file without repeating it in every frame:

```cpp
value["physics_profile_id"] = json::Value(frame.physicsProfileId);
```

- [ ] **Step 4: Prove legacy output and new ownership both pass**

```bash
cmake --build /tmp/billiardgl-phase3 --target BilliardsGameRuntimeTests BilliardsPhysicsInstrumentationTests BilliardsPhysicsTelemetryTests BilliardsAutomationProtocolTests
ctest --test-dir /tmp/billiardgl-phase3 -R 'Billiards(GameRuntime|PhysicsInstrumentation|PhysicsTelemetry|AutomationProtocol)Tests' --output-on-failure
```

Expected: both targets pass, including the existing five-tick legacy golden.

- [ ] **Step 5: Commit runtime profile ownership**

```bash
git add src/Billiards/game_runtime.h src/Billiards/game_runtime.cpp src/Billiards/physics.h src/Billiards/physics.cpp src/Billiards/physics_telemetry.h src/Billiards/physics_telemetry.cpp src/Billiards/automation_protocol.cpp tests/game_runtime_tests.cpp tests/physics_instrumentation_tests.cpp tests/physics_telemetry_tests.cpp tests/automation_protocol_tests.cpp
git diff --cached --check
git commit -m "refactor: route physics through runtime profiles"
```

### Task 3: Add Scenario Schema v3 Physics Profile Overrides

**Files:**
- Modify: `src/Billiards/physics_scenario.h`
- Modify: `src/Billiards/physics_scenario.cpp`
- Modify: `tests/physics_scenario_tests.cpp`
- Create: `tests/physics_validation/scenarios/profile_override_v3.json`

**Interfaces:**
- Consumes: C++ `PhysicsProfile` and runtime atomic replacement.
- Produces: schema v3 `physics_profile` with exact ball, surface, cue, cushion, and solver objects.

- [ ] **Step 1: Write failing v3 parser and atomic-apply tests**

Extend `tests/physics_scenario_tests.cpp` with a v3 document derived from `validDocument()` whose `physics_profile` contains every profile field. Assert that `ball.mass_kg=0.205`, `ball.radius_cm=3.05`, and a surface material survive parsing; reject missing keys, negative coefficients, unknown extra keys, nonfinite values, and unsupported solver time step. Apply it and assert the runtime owns the override while a fresh runtime still owns the production default.

Use this exact JSON shape:

```json
"physics_profile": {
  "id": "domenech_billiard_pvc_v1",
  "formula_version": "legacy_v1",
  "ball": {"mass_kg": 0.205, "radius_cm": 3.05, "material": "billiard_resin"},
  "surface": {
    "legacy_friction_acceleration_cm_s2": 4.0,
    "sliding_friction_coefficient": 0.0,
    "rolling_resistance_acceleration_cm_s2": 4.0,
    "torsional_spin_deceleration_rad_s2": 0.0,
    "slip_speed_epsilon_cm_s": 0.0001,
    "stop_energy_threshold_j": 0.000000001,
    "material": "pvc"
  },
  "cue": {"effective_mass_kg": 0.5},
  "cushion": {"normal_restitution": 1.0, "friction_coefficient": 0.0},
  "solver": {"time_step_seconds": 0.1, "maximum_events_per_tick": 64}
}
```

- [ ] **Step 2: Run the parser test and confirm v3 is rejected**

```bash
cmake --build /tmp/billiardgl-phase3 --target BilliardsPhysicsScenarioTests
ctest --test-dir /tmp/billiardgl-phase3 -R BilliardsPhysicsScenarioTests --output-on-failure
```

Expected: fails with unsupported scenario version 3.

- [ ] **Step 3: Implement exact schema parsing**

Set `kPhysicsScenarioVersion = 3`, retain versions 1 and 2, add `PhysicsProfile physicsProfile` to `PhysicsScenario`, and initialize it from `defaultChinesePoolPhysicsProfile()`. Parse v3 with exact-key helper functions; reject any unknown key. For versions 1 and 2, reject `physics_profile` and use the default. Apply with:

```cpp
const CueImpactInput* cue = scenario.hasCueImpact ? &scenario.cueImpact : nullptr;
return runtime.replaceStateForScenario(scenarioState, scenario.physicsProfile, cue);
```

- [ ] **Step 4: Run parser and E2E compatibility tests**

```bash
cmake --build /tmp/billiardgl-phase3 --target BilliardsPhysicsScenarioTests Billiards
ctest --test-dir /tmp/billiardgl-phase3 -R 'Billiards(PhysicsScenarioTests|HeadlessAutomationE2E)' --output-on-failure
```

Expected: v1/v2/v3 tests and existing headless automation pass.

- [ ] **Step 5: Commit scenario profile support**

```bash
git add src/Billiards/physics_scenario.h src/Billiards/physics_scenario.cpp tests/physics_scenario_tests.cpp tests/physics_validation/scenarios/profile_override_v3.json
git diff --cached --check
git commit -m "feat: add scenario physics profile overrides"
```

### Task 4: Add a Calibration-Only Reference Runner

**Files:**
- Create: `tools/physics_validation/partition_run.py`
- Create: `tools/physics_validation/calibration_run.py`
- Create: `tests/physics_validation/test_calibration_run.py`
- Modify: `tools/physics_validation/reference_run.py`

**Interfaces:**
- Produces: `case_ids_for_partition(adaptation, partition) -> tuple[str, ...]`.
- Produces: `run_calibration(executable, package, output, execute_once=None) -> int`.
- Must not expose arbitrary case IDs or split override in calibration CLI.

- [ ] **Step 1: Write isolation tests**

Create tests that use `reference_package_v1` and a fake executor. Record scenario IDs and assert only `synthetic_reference__free_roll_calibration` executes twice. Inspect both function and CLI signatures to assert that `case`, `partition`, `split`, and `holdout` are absent. Assert a package with no executable calibration case fails before starting the process.

```python
def test_executes_only_committed_calibration_cases(self):
    seen = []
    def execute_once(executable, scenario):
        seen.append(scenario["id"])
        return trace_for_scenario(scenario)
    self.assertEqual(run_calibration(self.executable, FIXTURE_ROOT, self.output,
                                     execute_once=execute_once), 0)
    self.assertEqual(seen, ["synthetic_reference__free_roll_calibration"] * 2)
```

- [ ] **Step 2: Run the missing runner test**

```bash
python3 -m unittest tests.physics_validation.test_calibration_run -v
```

Expected: import failure for `calibration_run`.

- [ ] **Step 3: Extract committed-partition selection and implement the CLI**

Implement:

```python
def case_ids_for_partition(adaptation, partition):
    if partition not in {"CALIBRATION", "HOLDOUT"}:
        raise ValueError("partition must be a committed reference partition")
    result = tuple(case.case_id for case in adaptation.cases
                   if case.partition == partition)
    if not result:
        raise ValueError(f"no executable {partition} cases")
    return result
```

Refactor package loading/adaptation into a read-only helper shared with `reference_run`. `run_calibration` derives calibration case IDs itself and calls the existing execution/report path. Its parser accepts only `--executable`, `--package`, and `--output`.

- [ ] **Step 4: Run calibration and existing reference tests**

```bash
python3 -m unittest tests.physics_validation.test_calibration_run tests.physics_validation.test_reference_run tests.physics_validation.test_reference_split -v
```

Expected: all tests pass and no holdout scenario appears in the calibration output.

- [ ] **Step 5: Commit the calibration boundary**

```bash
git add tools/physics_validation/partition_run.py tools/physics_validation/calibration_run.py tools/physics_validation/reference_run.py tests/physics_validation/test_calibration_run.py
git diff --cached --check
git commit -m "feat: isolate reference calibration runs"
```

### Task 5: Add Canonical Candidate Freeze Records

**Files:**
- Create: `tools/physics_validation/model_candidate.py`
- Create: `tools/physics_validation/freeze_candidate.py`
- Create: `tests/physics_validation/test_model_candidate.py`
- Create: `tests/physics_validation/fixtures/model_candidate_v1/profile.json`
- Create: `tests/physics_validation/fixtures/model_candidate_v1/calibration_report.json`

**Interfaces:**
- Produces: `load_candidate_freeze(path) -> ModelCandidateFreeze`.
- Produces: `load_profile_manifest(path) -> PhysicsProfileManifest` with complete parameter provenance.
- Produces: `write_candidate_freeze(...) -> Path`.
- Freeze schema version 1 binds candidate ID, formula version, profile SHA-256, executable SHA-256, calibration report SHA-256, dataset/version/package hashes, metric targets, and creation timestamp supplied by the caller.

- [ ] **Step 1: Write freeze schema/hash tests**

Tests require exact schema keys, safe IDs, lowercase 64-character SHA-256, a 40-character lowercase source revision, sorted unique datasets, finite metric thresholds, canonical bytes, and rejection after changing one profile or report byte. A profile manifest must contain exactly `schema_version`, `runtime_profile`, `parameter_sources`, and `applicability`; every numeric runtime leaf must have a corresponding source record containing `kind`, `unit`, and a nonempty evidence or limitation statement. The timestamp is an explicit CLI argument so repeated runs with the same inputs are byte-identical.

```python
freeze = load_candidate_freeze(path)
self.assertEqual(freeze.candidate_id, "surface_motion_v1")
self.assertEqual(path.read_bytes(), regenerated.read_bytes())
with profile.open("ab") as output:
    output.write(b" ")
with self.assertRaisesRegex(ValueError, "profile_sha256"):
    freeze.verify(profile=profile, executable=executable,
                  calibration_report=report)
```

- [ ] **Step 2: Run the missing candidate tests**

```bash
python3 -m unittest tests.physics_validation.test_model_candidate -v
```

Expected: import failure for `model_candidate`.

- [ ] **Step 3: Implement strict canonical freeze I/O**

Use `hashlib.sha256(path.read_bytes()).hexdigest()`, `json.dumps(..., ensure_ascii=False, indent=2, sort_keys=True, allow_nan=False) + "\n"`, exact-key validation, and frozen dataclasses. The CLI must require:

```text
--candidate-id --formula-version --source-revision --profile --executable
--calibration-report --dataset-manifest --created-at --output
```

It also supports a mutually exclusive verification mode:

```text
--verify FREEZE_JSON --profile PROFILE_JSON --executable BINARY
--calibration-report REFERENCE_REPORT_JSON
```

Verification returns zero only when every recorded hash and exact schema field matches. Neither mode reads validation reports.

- [ ] **Step 4: Run candidate and encoding tests**

```bash
python3 -m unittest tests.physics_validation.test_model_candidate tests.check_text_encoding_tests -v
```

Expected: all tests pass.

- [ ] **Step 5: Commit candidate freezing**

```bash
git add tools/physics_validation/model_candidate.py tools/physics_validation/freeze_candidate.py tests/physics_validation/test_model_candidate.py tests/physics_validation/fixtures/model_candidate_v1
git diff --cached --check
git commit -m "feat: freeze calibrated physics candidates"
```

### Task 6: Require a Frozen Candidate for Validation Runs

**Files:**
- Create: `tools/physics_validation/validation_run.py`
- Create: `tools/physics_validation/data_lifecycle.py`
- Create: `tests/physics_validation/test_validation_run.py`
- Create: `tests/physics_validation/validation_data_status.json`
- Modify: `tools/physics_validation/model_candidate.py`
- Modify: `tools/physics_validation/reference_report.py`
- Modify: `tests/physics_validation/test_reference_report.py`

**Interfaces:**
- Produces: `run_candidate_validation(freeze, executable, package, profile, output, execute_once=None) -> int`.
- Produces: canonical `validation_receipt.json` with candidate/freeze hashes and unchanged result summary.
- Produces: `load_data_lifecycle(path)` with only `calibration`, `validation`, `spent`, and `confirmation` states.
- Uses only committed `HOLDOUT` cases; no arbitrary case or split option.

- [ ] **Step 1: Write frozen-validation tests**

Assert that validation refuses a mismatched executable/profile/package before invoking the executor; refuses a partition marked `spent` or `calibration` in the lifecycle registry; executes only the synthetic holdout case twice for a valid freeze and `validation` state; labels report metadata with candidate and freeze hashes; and writes a receipt even when the physical result is a known mismatch. Assert CLI has no calibration, case, split, or parameter flag.

- [ ] **Step 2: Run the missing validation tests**

```bash
python3 -m unittest tests.physics_validation.test_validation_run -v
```

Expected: import failure for `validation_run`.

- [ ] **Step 3: Implement validation and receipts**

Validate exact registry keys, dataset/version identity, and the four allowed lifecycle states. Verify every freeze hash first, require the package's committed holdout partition to be in `validation` state, derive only `HOLDOUT` IDs with `case_ids_for_partition`, execute through the shared reference path, and write:

```json
{
  "schema_version": 1,
  "datasets": [
    {
      "dataset_id": "mathavan_2009_high_speed",
      "dataset_version": "1.0.0",
      "calibration_status": "calibration",
      "holdout_status": "validation"
    }
  ]
}
```

The committed registry contains one sorted entry for each of the four reference packages; the snippet shows the exact entry shape.

```json
{
  "schema_version": 1,
  "candidate_id": "surface_motion_v1",
  "freeze_sha256": "<sha256>",
  "dataset_id": "synthetic_reference",
  "dataset_version": "1.0.0",
  "partition": "HOLDOUT",
  "report_sha256": "<sha256>",
  "result": "PASSED_OR_ACCOUNTED"
}
```

The receipt records results only; it never edits a profile, freeze record, or lifecycle registry. If a result is later used to choose a new model, changing that dataset entry to `spent` is an explicit reviewed commit.

- [ ] **Step 4: Run all governance tests**

```bash
python3 -m unittest \
  tests.physics_validation.test_calibration_run \
  tests.physics_validation.test_model_candidate \
  tests.physics_validation.test_validation_run \
  tests.physics_validation.test_reference_report -v
```

Expected: all tests pass.

- [ ] **Step 5: Commit frozen validation**

```bash
git add tools/physics_validation/validation_run.py tools/physics_validation/data_lifecycle.py tools/physics_validation/model_candidate.py tools/physics_validation/reference_report.py tests/physics_validation/test_validation_run.py tests/physics_validation/test_reference_report.py tests/physics_validation/validation_data_status.json
git diff --cached --check
git commit -m "feat: validate only frozen physics candidates"
```

### Task 7: Document and Verify Theme 0 End to End

**Files:**
- Modify: `docs/reference-data-packages.md`
- Modify: `.github/workflows/physics-reference-full.yml`
- Create: `tests/physics_validation/test_candidate_workflow.py`

**Interfaces:**
- Documents exact calibration, freeze, validation, receipt, and spent-data commands.
- CI continues full regression, while candidate validation remains an explicit frozen-candidate action.

- [ ] **Step 1: Write a workflow contract test**

Create a test that reads the workflow and docs, requiring calibration and validation command examples, artifact upload of freeze/receipt files, and the statement that committed values provide process isolation rather than secrecy.

- [ ] **Step 2: Run it and confirm documentation is missing**

```bash
python3 -m unittest tests.physics_validation.test_candidate_workflow -v
```

Expected: fails because the new workflow contract is undocumented.

- [ ] **Step 3: Add the documented manual workflow path**

Document the exact commands and add manual workflow inputs for a committed freeze/profile path. The workflow must verify hashes before running validation and upload calibration reports, freeze record, validation report, and receipt. Keep the weekly full regression unchanged and never pass a split override.

- [ ] **Step 4: Run the complete theme 0 verification**

```bash
cmake --build /tmp/billiardgl-phase3
ctest --test-dir /tmp/billiardgl-phase3 --output-on-failure
python3 -m unittest discover -s tests -p 'test_*.py' -v
python3 scripts/check_text_encoding.py --root .
git diff --check
```

Expected: all C++/E2E/Python tests pass; this is the first point in the plan where full reference checks may run because governance exists, but no tuned candidate has yet been selected.

- [ ] **Step 5: Commit theme 0 documentation and CI**

```bash
git add docs/reference-data-packages.md .github/workflows/physics-reference-full.yml tests/physics_validation/test_candidate_workflow.py
git diff --cached --check
git commit -m "docs: define physics candidate validation workflow"
```

## Theme 0 Acceptance

- Runtime owns a validated profile and preserves legacy outputs.
- Scenario v3 can override experimental geometry/material inputs without changing production defaults.
- Calibration and validation tools expose no split override and execute only their committed partitions.
- Validation cannot start until executable, profile, package, and calibration hashes match a freeze record.
- Freeze records and receipts are deterministic, offline, UTF-8, and auditable.
- Existing v1/v2 scenarios and automation remain compatible.
