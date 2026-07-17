# Surface Motion and Spin Implementation Plan

**Execution status: Completed and archived.** Unchecked boxes below preserve the original execution script; current disposition and evidence are indexed in [Phase 3 current status](../../phase3-physics-current-status.md).

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Replace legacy constant translational deceleration with event-aware sliding, pure-rolling, stopping, and three-dimensional angular-velocity dynamics, then freeze and validate the first production physics candidate.

**Architecture:** Introduce a focused `SurfaceMotionModel` that advances one ball from its contact-point slip velocity using exact constant-acceleration segments. `PhysicsStepper` continues to orchestrate the existing game, while runtime profiles provide material and numerical parameters and telemetry exposes the authoritative motion state.

**Tech Stack:** C++11, CMake/CTest, Python 3 reference analyzer/runners, committed Mathavan 2009 reference package.

## Global Constraints

- This plan starts only after `2026-07-14-physics-calibration-governance.md` is fully implemented and verified.
- Use only committed `CALIBRATION` points while choosing formulas or parameters.
- Do not run holdout, the full reference workflow, or `scripts/check.sh` until the candidate freeze commit in Task 7 exists.
- Keep position and linear speed in `cm`/`cm/s`, angular speed in `rad/s`, mass in `kg`, and energy in `J`.
- Sliding uses contact-point relative velocity; pure rolling is not manufactured by truncating angular velocity.
- Material parameters may fit calibration; numerical epsilons come only from convergence/stability tests.
- Mathavan Table I calibration points may verify that a pure-roll window exists, but their post-collision speed must not tune surface parameters because the ball-ball model belongs to theme 3.
- Torsional sidespin decay remains zero and explicitly unevidenced in this candidate; it is not fitted from holdout.
- Existing ball-ball, rail, pocket, and gameplay behavior remains unchanged except for the downstream effect of the new surface motion.
- Every commit passes focused tests, UTF-8 validation, and `git diff --check`.

---

## File Structure

- Create `src/Billiards/surface_motion.h`: motion-state enum, result record, slip/energy helpers, and one-ball advance API.
- Create `src/Billiards/surface_motion.cpp`: sliding, rolling, stop, and display-rotation integration.
- Create `tests/surface_motion_tests.cpp`: analytic, energy, state, mirror, and time-step tests.
- Modify `src/Billiards/game_state.h` and `.cpp`: authoritative motion state initialization/reset.
- Modify `src/Billiards/physics.h` and `.cpp`: call `advanceSurfaceMotion` through the runtime profile.
- Modify `src/Billiards/physics_telemetry.h` and `.cpp`: rotational energy, slip speed, state, and transition records.
- Modify `src/Billiards/automation_protocol.cpp`: serialize new trace fields without changing protocol request shapes.
- Modify C++ telemetry/protocol tests and `CMakeLists.txt`.
- Modify `tools/physics_validation/analyzer.py` and tests: prefer explicit motion state and cross-check kinematics.
- Modify `tools/physics_validation/adapters/mathavan_2009.py` and its tests: make Table I executable after a real pure-roll window exists.
- Create `physics_models/profiles/chinese_pool_surface_motion_v1.json`: runtime profile plus per-parameter provenance.
- Create `physics_models/candidates/surface_motion_v1/`: committed calibration report, freeze record, and later validation receipt/report.
- Modify Mathavan mismatch/limitation manifests only after the corresponding result exists.
- Modify `docs/reference-data-packages.md`: candidate parameters, evidence, validation result, and remaining limitations.

### Task 1: Add Authoritative Motion State and Surface Telemetry Contracts

**Files:**
- Create: `src/Billiards/surface_motion.h`
- Modify: `src/Billiards/game_state.h`
- Modify: `src/Billiards/game_state.cpp`
- Modify: `src/Billiards/physics_telemetry.h`
- Modify: `tests/game_state_tests.cpp`
- Modify: `tests/physics_telemetry_tests.cpp`
- Modify: `CMakeLists.txt`

**Interfaces:**
- Produces: `enum class BallMotionState { Stationary, Sliding, Rolling }`.
- Produces: `const char* ballMotionStateName(BallMotionState)`.
- Produces: `Point3 surfaceContactSlipVelocity(const BallState&, float radiusCm)`.
- Produces: `double rotationalKineticEnergyJ(const BallState&, const BallProperties&)`.
- Produces: `SurfaceMotionStep advanceSurfaceMotion(BallState&, float, const BallProperties&, const SurfaceProperties&)`.

- [ ] **Step 1: Write failing state and telemetry tests**

Add tests that require reset balls to be `Stationary`, a moving zero-spin ball to classify as `Sliding`, and a ball with `v=(20,0,0)` and `omega=(0,0,-20/r)` to classify as `Rolling`. Extend telemetry expectations:

```cpp
expect(frame.balls[0].motionState == billiardgl::BallMotionState::Sliding,
    "explicit motion state is traced");
expect(close(frame.balls[0].contactSlipSpeedCmS, 100.0),
    "contact slip speed is traced");
expect(frame.rotationalKineticEnergyJ > 0.0,
    "rotational kinetic energy is included");
expect(frame.totalKineticEnergyJ ==
    frame.translationalKineticEnergyJ + frame.rotationalKineticEnergyJ,
    "total energy has one definition");
```

- [ ] **Step 2: Register and run the missing surface target**

Add `src/Billiards/surface_motion.cpp` to core sources and:

```cmake
billiardgl_add_core_test(BilliardsSurfaceMotionTests tests/surface_motion_tests.cpp)
```

Create an initially empty `tests/surface_motion_tests.cpp` that includes `surface_motion.h`, then run:

```bash
cmake --build /tmp/billiardgl-phase3 --target BilliardsGameStateTests BilliardsPhysicsTelemetryTests BilliardsSurfaceMotionTests
```

Expected: compilation fails because the motion state and telemetry fields do not exist.

- [ ] **Step 3: Define the contract without changing motion behavior**

In `surface_motion.h` define:

```cpp
enum class BallMotionState { Stationary, Sliding, Rolling };

struct SurfaceMotionStep {
    int ballIndex = -1;
    BallMotionState before = BallMotionState::Stationary;
    BallMotionState after = BallMotionState::Stationary;
    float initialSlipSpeedCmS = 0.0f;
    float finalSlipSpeedCmS = 0.0f;
    float transitionTimeSeconds = -1.0f;
    Point3 frictionAccelerationCmS2;
    Point3 angularAccelerationRadS2;
};

Point3 surfaceContactSlipVelocity(const BallState& ball, float radiusCm);
BallMotionState classifySurfaceMotion(
    const BallState& ball, const BallProperties& ballProperties,
    const SurfaceProperties& surface);
double rotationalKineticEnergyJ(
    const BallState& ball, const BallProperties& ballProperties);
SurfaceMotionStep advanceSurfaceMotion(
    BallState& ball, float deltaSeconds, const BallProperties& ballProperties,
    const SurfaceProperties& surface);
```

Add `motionState` to `BallState`, initialize/reset it to stationary, and add state/slip/rotational energy fields to telemetry. At this task `advanceSurfaceMotion` may delegate to legacy movement; physical replacement starts in Task 2.

- [ ] **Step 4: Run contract tests**

```bash
cmake --build /tmp/billiardgl-phase3 --target BilliardsGameStateTests BilliardsPhysicsTelemetryTests BilliardsSurfaceMotionTests
ctest --test-dir /tmp/billiardgl-phase3 -R 'Billiards(GameState|PhysicsTelemetry|SurfaceMotion)Tests' --output-on-failure
```

Expected: all three targets pass and legacy golden tests remain untouched.

- [ ] **Step 5: Commit the state contract**

```bash
git add CMakeLists.txt src/Billiards/surface_motion.h src/Billiards/surface_motion.cpp src/Billiards/game_state.h src/Billiards/game_state.cpp src/Billiards/physics_telemetry.h tests/game_state_tests.cpp tests/physics_telemetry_tests.cpp tests/surface_motion_tests.cpp
python3 scripts/check_text_encoding.py --root .
git diff --cached --check
git commit -m "feat: add surface motion state telemetry"
```

### Task 2: Implement Exact Pure-Rolling and Stop Integration

**Files:**
- Modify: `src/Billiards/surface_motion.cpp`
- Modify: `tests/surface_motion_tests.cpp`

**Interfaces:**
- Consumes: `advanceSurfaceMotion` contract and rolling resistance profile field.
- Produces: exact position/velocity/angular velocity for an initially pure-rolling ball.

- [ ] **Step 1: Add failing rolling tests**

For `radius=2.8575 cm`, `rollingResistance=12.5 cm/s^2`, `v0=20 cm/s`, and pure-roll `omega_z=-v0/radius`, test `dt=0.1 s`:

```cpp
expect(close(ball.position.x, 1.9375f, 1e-5f), "x = v0*t - 0.5*a*t^2");
expect(close(ball.velocity.x, 18.75f, 1e-5f), "rolling speed after one step");
expect(close(ball.angularVelocity.z, -18.75f / radius, 1e-5f),
    "rolling constraint remains exact");
expect(step.after == billiardgl::BallMotionState::Rolling, "still rolling");
```

Also test a ball at `0.5 cm/s` stops after `0.04 s`, travels `0.01 cm`, ends with zero horizontal velocity and roll-coupled angular velocity, and never reverses. Compare one `0.1 s` step with ten `0.01 s` steps.

- [ ] **Step 2: Run and observe legacy integration failure**

```bash
cmake --build /tmp/billiardgl-phase3 --target BilliardsSurfaceMotionTests
ctest --test-dir /tmp/billiardgl-phase3 -R BilliardsSurfaceMotionTests --output-on-failure
```

Expected: position and rolling-state assertions fail.

- [ ] **Step 3: Implement the exact rolling segment**

Use horizontal speed `s`, unit direction `d`, resistance `a`, and:

```cpp
const float segment = std::min(deltaSeconds, s / a);
const float distance = s * segment - 0.5f * a * segment * segment;
ball.position.x += direction.x * distance;
ball.position.z += direction.z * distance;
const float finalSpeed = std::max(0.0f, s - a * segment);
ball.velocity.x = direction.x * finalSpeed;
ball.velocity.z = direction.z * finalSpeed;
ball.angularVelocity.x = ball.velocity.z / radiusCm;
ball.angularVelocity.z = -ball.velocity.x / radiusCm;
```

If `segment < deltaSeconds`, set horizontal velocity/roll angular components to zero and state to stationary. Do not zero `omega_y`; torsional decay is a separate parameter.

- [ ] **Step 4: Run rolling and time-step tests**

```bash
cmake --build /tmp/billiardgl-phase3 --target BilliardsSurfaceMotionTests
ctest --test-dir /tmp/billiardgl-phase3 -R BilliardsSurfaceMotionTests --output-on-failure
```

Expected: exact rolling, stop, non-reversal, and split-step tests pass.

- [ ] **Step 5: Commit rolling integration**

```bash
git add src/Billiards/surface_motion.cpp tests/surface_motion_tests.cpp
git diff --cached --check
git commit -m "feat: integrate pure rolling and stopping"
```

### Task 3: Implement Sliding Friction and In-Tick Transition to Rolling

**Files:**
- Modify: `src/Billiards/surface_motion.cpp`
- Modify: `tests/surface_motion_tests.cpp`

**Interfaces:**
- Consumes: ball mass/radius, `slidingFrictionCoefficient`, and exact rolling segment.
- Produces: coupled linear/angular sliding integration and transition timestamp.

- [ ] **Step 1: Add failing analytic sliding tests**

For `v=(100,0,0) cm/s`, zero spin, `mu=0.20`, and `g=980.665 cm/s^2`, require:

```text
sliding acceleration x = -196.133 cm/s^2
angular acceleration z = -5 * 196.133 / (2 * radius) rad/s^2
transition time = 100 / (3.5 * 196.133) s
```

Test the state immediately before and after transition, exact zero contact slip after transition, monotonic total kinetic energy, negative-velocity mirror symmetry, a z-axis shot, initial topspin that accelerates the center without increasing total energy, and equality between one event-crossing step and split steps.

- [ ] **Step 2: Run and confirm sliding coupling is absent**

```bash
cmake --build /tmp/billiardgl-phase3 --target BilliardsSurfaceMotionTests
ctest --test-dir /tmp/billiardgl-phase3 -R BilliardsSurfaceMotionTests --output-on-failure
```

Expected: angular acceleration and transition tests fail.

- [ ] **Step 3: Implement coupled Coulomb sliding**

For contact vector `(0,-r,0)`, compute:

```cpp
Point3 slip{
    ball.velocity.x + radiusCm * ball.angularVelocity.z,
    0.0f,
    ball.velocity.z - radiusCm * ball.angularVelocity.x};
const float slipSpeed = horizontalLength(slip);
const Point3 acceleration{
    -mu * kStandardGravityCmS2 * slip.x / slipSpeed,
    0.0f,
    -mu * kStandardGravityCmS2 * slip.z / slipSpeed};
const Point3 angularAcceleration{
    -2.5f * acceleration.z / radiusCm,
    0.0f,
    2.5f * acceleration.x / radiusCm};
const float transition = slipSpeed /
    (3.5f * mu * kStandardGravityCmS2);
```

Integrate position with `v*t + 0.5*a*t^2`, update velocity and angular velocity over `min(dt, transition)`, snap only the floating-point residual contact slip at the analytic transition, then advance the remaining time through the rolling segment. `PhysicsStepper` supplies a finite nonnegative duration from the already validated solver profile.

- [ ] **Step 4: Run all analytic surface tests**

```bash
cmake --build /tmp/billiardgl-phase3 --target BilliardsSurfaceMotionTests
ctest --test-dir /tmp/billiardgl-phase3 -R BilliardsSurfaceMotionTests --output-on-failure
```

Expected: sliding, rotation, energy, transition, mirror, and split-step tests pass.

- [ ] **Step 5: Commit sliding integration**

```bash
git add src/Billiards/surface_motion.cpp tests/surface_motion_tests.cpp
git diff --cached --check
git commit -m "feat: couple sliding friction and ball spin"
```

### Task 4: Replace Legacy Movement in the Production Physics Path

**Files:**
- Modify: `src/Billiards/physics.h`
- Modify: `src/Billiards/physics.cpp`
- Modify: `src/Billiards/game_runtime.cpp`
- Modify: `src/Billiards/billiards.cpp`
- Modify: `src/Billiards/surface_motion.cpp`
- Modify: `tests/physics_tests.cpp`
- Modify: `tests/physics_instrumentation_tests.cpp`
- Modify: `tests/game_runtime_tests.cpp`

**Interfaces:**
- Replaces: production calls to `applyFrictionAndMove`.
- Preserves: `updatePhysics(GameState&, float)` wrapper for source compatibility, using the production profile.
- Produces: `SurfaceMotionStep` entries inside `PhysicsStepTelemetry`.

- [ ] **Step 1: Rewrite legacy behavior tests as profile-based physical tests**

Remove expectations tied to the five-tick `4 cm/s^2` snapshot. Require `updatePhysics(state, dt, profile)` to match direct `advanceSurfaceMotion`, preserve all collision/rail/pocket contact records at `dt=0`, and use the runtime-owned profile. Add a test that two runtimes with rolling resistance `4` and `12.5 cm/s^2` diverge while a new runtime retains the production candidate.

- [ ] **Step 2: Run and confirm production still calls legacy friction**

```bash
cmake --build /tmp/billiardgl-phase3 --target BilliardsPhysicsTests BilliardsPhysicsInstrumentationTests BilliardsGameRuntimeTests
ctest --test-dir /tmp/billiardgl-phase3 -R 'Billiards(Physics|PhysicsInstrumentation|GameRuntime)Tests' --output-on-failure
```

Expected: profile-dependent motion tests fail.

- [ ] **Step 3: Route every production entry through SurfaceMotionModel**

Replace the loop call with:

```cpp
SurfaceMotionStep surface = advanceSurfaceMotion(
    ball, timeStep, profile.ball, profile.surface);
surface.ballIndex = i;
telemetry.surfaceMotion.push_back(surface);
```

The interactive legacy entry points in `billiards.cpp` must call `updatePhysics` with the same production profile used by `GameRuntime`, held in one shared immutable value rather than reconstructing constants per frame. Remove the old implementation after all call sites are migrated; keep only a deprecated test wrapper if Windows project compatibility still requires the symbol.

Update display orientation from authoritative angular velocity:

```cpp
const float omega = length(ball.angularVelocity);
if (omega > 0.0f) {
    ball.rotationAxis.x = ball.angularVelocity.x / omega;
    ball.rotationAxis.y = ball.angularVelocity.y / omega;
    ball.rotationAxis.z = ball.angularVelocity.z / omega;
    ball.rotationAngle += omega * deltaSeconds * 180.0f / kPi;
}
```

- [ ] **Step 4: Run focused production and E2E tests without reference holdout**

```bash
cmake --build /tmp/billiardgl-phase3 --target BilliardsPhysicsTests BilliardsPhysicsInstrumentationTests BilliardsGameRuntimeTests Billiards
ctest --test-dir /tmp/billiardgl-phase3 -R 'Billiards(Physics|PhysicsInstrumentation|GameRuntime|HeadlessAutomationE2E)' --output-on-failure
```

Expected: focused production tests pass. Do not run reference E2E yet.

- [ ] **Step 5: Commit production integration**

```bash
git add src/Billiards/physics.h src/Billiards/physics.cpp src/Billiards/game_runtime.cpp src/Billiards/billiards.cpp src/Billiards/surface_motion.cpp tests/physics_tests.cpp tests/physics_instrumentation_tests.cpp tests/game_runtime_tests.cpp
git diff --cached --check
git commit -m "feat: use surface motion in production physics"
```

### Task 5: Serialize and Analyze Explicit Surface State

**Files:**
- Modify: `src/Billiards/physics_telemetry.cpp`
- Modify: `src/Billiards/automation_protocol.cpp`
- Modify: `tests/physics_telemetry_tests.cpp`
- Modify: `tests/automation_protocol_tests.cpp`
- Modify: `tools/physics_validation/analyzer.py`
- Modify: `tests/physics_validation/test_analyzer.py`

**Interfaces:**
- Trace ball fields: `motion_state`, `contact_slip_speed_cm_s`, `rotational_kinetic_energy_j`.
- Trace frame fields: `rotational_kinetic_energy_j`, `total_kinetic_energy_j`, `surface_transitions`.
- Analyzer cross-checks explicit state against contact-slip kinematics.

- [ ] **Step 1: Add failing serialization and analyzer tests**

Require exact lowercase names `stationary`, `sliding`, `rolling`. Feed analyzer a frame labeled rolling with nonzero slip and require `INTEGRATION_MISMATCH`; feed a consistent explicit rolling state and require the transition metric to pass. Require nonfinite slip/rotational energy to be `NUMERICAL_FAILURE`.

- [ ] **Step 2: Run and observe missing trace fields**

```bash
cmake --build /tmp/billiardgl-phase3 --target BilliardsPhysicsTelemetryTests BilliardsAutomationProtocolTests
ctest --test-dir /tmp/billiardgl-phase3 -R 'Billiards(PhysicsTelemetry|AutomationProtocol)Tests' --output-on-failure
python3 -m unittest tests.physics_validation.test_analyzer -v
```

Expected: new field assertions fail.

- [ ] **Step 3: Serialize authoritative fields and cross-check them**

Add to each serialized ball:

```cpp
ball["motion_state"] = json::Value(ballMotionStateName(sample.motionState));
ball["contact_slip_speed_cm_s"] = json::Value(sample.contactSlipSpeedCmS);
ball["rotational_kinetic_energy_j"] = json::Value(sample.rotationalKineticEnergyJ);
```

Change `_is_pure_roll` to require `motion_state == "rolling"` when present and always verify computed slip is within tolerance. Keep the kinematic fallback for old committed traces. Explicit-state disagreement is integration failure, not reference limitation.

- [ ] **Step 4: Run telemetry and analyzer tests**

```bash
cmake --build /tmp/billiardgl-phase3 --target BilliardsPhysicsTelemetryTests BilliardsAutomationProtocolTests
ctest --test-dir /tmp/billiardgl-phase3 -R 'Billiards(PhysicsTelemetry|AutomationProtocol)Tests' --output-on-failure
python3 -m unittest tests.physics_validation.test_analyzer -v
```

Expected: all pass.

- [ ] **Step 5: Commit trace integration**

```bash
git add src/Billiards/physics_telemetry.cpp src/Billiards/automation_protocol.cpp tests/physics_telemetry_tests.cpp tests/automation_protocol_tests.cpp tools/physics_validation/analyzer.py tests/physics_validation/test_analyzer.py
git diff --cached --check
git commit -m "feat: expose surface motion in physics traces"
```

### Task 6: Make Mathavan Surface and Post-Collision Windows Executable

**Files:**
- Modify: `tools/physics_validation/adapters/mathavan_2009.py`
- Modify: `tests/physics_validation/test_mathavan_2009_adapter.py`
- Modify: `tests/physics_validation/reference_data/mathavan_2009_high_speed/expected_reference_limitations.json`
- Modify after calibration execution: `tests/physics_validation/reference_data/mathavan_2009_high_speed/expected_model_mismatches.json`

**Interfaces:**
- Produces executable Table I cases with `first_pure_roll_after_event` selection.
- Removes only `table1_post_collision_roll_transition_unmodeled`; source/equipment/spin limitations remain.

- [ ] **Step 1: Add failing adapter tests**

Require all five Table I cases and ten points to be adapted, with the existing pure-roll selection metadata, and require no point IDs to be claimed by the transition limitation. Require source geometry, unmeasured spin, equipment conversion, and unresolved marker limitations to remain.

- [ ] **Step 2: Run and confirm Table I is still skipped**

```bash
python3 -m unittest tests.physics_validation.test_mathavan_2009_adapter -v
```

Expected: fails because oblique cases are skipped and ten points are limitation-only.

- [ ] **Step 3: Remove only the resolved runtime limitation**

Delete the `if mapping.get("kind") == "oblique_ball_collision": continue` branch and remove the transition limitation from `mathavan_2009_limitations`. Do not alter normalized values, split membership, uncertainty, source locators, or the other three limitations.

- [ ] **Step 4: Run calibration only and classify calibration diagnostics**

```bash
python3 -m unittest tests.physics_validation.test_mathavan_2009_adapter -v
python3 -m tools.physics_validation.calibration_run \
  --executable /tmp/billiardgl-phase3/Billiards \
  --package tests/physics_validation/reference_data/mathavan_2009_high_speed \
  --output /tmp/billiardgl-surface-calibration
```

Expected: rolling and Table I calibration cases execute. Rolling deceleration is used for surface fitting; Table I speed errors are recorded as theme 3 collision diagnostics and are not inputs to the surface objective. Add exact calibration-only mismatch keys produced by the frozen implementation; do not inspect or modify holdout entries.

- [ ] **Step 5: Commit executable calibration coverage**

```bash
git add tools/physics_validation/adapters/mathavan_2009.py tests/physics_validation/test_mathavan_2009_adapter.py tests/physics_validation/reference_data/mathavan_2009_high_speed/expected_reference_limitations.json tests/physics_validation/reference_data/mathavan_2009_high_speed/expected_model_mismatches.json
git diff --cached --check
git commit -m "test: execute Mathavan post-collision roll windows"
```

### Task 7: Fit Calibration, Commit the Profile, and Freeze Candidate v1

**Files:**
- Create: `physics_models/profiles/chinese_pool_surface_motion_v1.json`
- Create: `physics_models/candidates/surface_motion_v1/calibration/reference_report.json`
- Create: `physics_models/candidates/surface_motion_v1/calibration/reference_points.csv`
- Create: `physics_models/candidates/surface_motion_v1/calibration/reference_report.md`
- Create: `physics_models/candidates/surface_motion_v1/freeze.json`
- Modify: `src/Billiards/physics_profile.cpp`
- Modify: `tests/physics_profile_tests.cpp`

**Interfaces:**
- Produces production candidate `chinese_pool_surface_motion_v1` / formula `surface_motion_v1`.
- Freezes the exact profile, calibration artifacts, source revision, and executable used for validation.

- [ ] **Step 1: Add a failing production-profile test**

Require these preregistered candidate values:

```cpp
expect(profile.id == "chinese_pool_surface_motion_v1", "candidate ID");
expect(profile.formulaVersion == "surface_motion_v1", "formula version");
expect(close(profile.surface.rollingResistanceAccelerationCmS2, 12.5f),
    "Mathavan rolling calibration midpoint");
expect(close(profile.surface.slidingFrictionCoefficient, 0.20f),
    "preregistered independent sliding hypothesis");
expect(profile.surface.torsionalSpinDecelerationRadS2 == 0.0f,
    "sidespin decay remains unevidenced");
```

The `0.20` sliding coefficient is frozen before holdout as a physical prior, not selected from Mathavan's holdout sliding result. Record that distinction in profile provenance.

- [ ] **Step 2: Run the production-profile test and confirm legacy defaults remain**

```bash
cmake --build /tmp/billiardgl-phase3 --target BilliardsPhysicsProfileTests
ctest --test-dir /tmp/billiardgl-phase3 -R BilliardsPhysicsProfileTests --output-on-failure
```

Expected: candidate-ID/value assertions fail.

- [ ] **Step 3: Install candidate defaults and profile provenance**

Change the production default to the candidate. Write a profile manifest whose `runtime_profile` exactly matches scenario v3 fields. Its `parameter_sources` contains one entry for every numeric runtime leaf, including unchanged ball, cue, cushion, solver, epsilon, energy, and legacy compatibility fields. The following entries distinguish calibrated, hypothesized, and unmodeled surface values:

```json
{
  "rolling_resistance_acceleration_cm_s2": {
    "kind": "calibration",
    "dataset_id": "mathavan_2009_high_speed",
    "point_id": "rolling_deceleration_range"
  },
  "sliding_friction_coefficient": {
    "kind": "preregistered_physical_hypothesis",
    "value": 0.2,
    "validation_data_used": false
  },
  "torsional_spin_deceleration_rad_s2": {
    "kind": "unmodeled",
    "value": 0.0,
    "limitation": "no admitted sidespin-decay experiment"
  }
}
```

- [ ] **Step 4: Regenerate calibration artifacts and create the freeze record**

```bash
cmake --build /tmp/billiardgl-phase3 --target Billiards
rm -rf /tmp/billiardgl-surface-calibration
python3 -m tools.physics_validation.calibration_run \
  --executable /tmp/billiardgl-phase3/Billiards \
  --package tests/physics_validation/reference_data/mathavan_2009_high_speed \
  --output /tmp/billiardgl-surface-calibration
```

Inspect only the calibration report. Remove the obsolete rolling-deceleration mismatch and reconcile any Table I calibration-only collision mismatches without changing model values or intervals, then rerun the same calibration command and require exit 0. After reconciliation, preserve the successful report and freeze it:

```bash
mkdir -p physics_models/candidates/surface_motion_v1/calibration
cp /tmp/billiardgl-surface-calibration/reference_report.json physics_models/candidates/surface_motion_v1/calibration/
cp /tmp/billiardgl-surface-calibration/reference_points.csv physics_models/candidates/surface_motion_v1/calibration/
cp /tmp/billiardgl-surface-calibration/reference_report.md physics_models/candidates/surface_motion_v1/calibration/
python3 -m tools.physics_validation.freeze_candidate \
  --candidate-id chinese_pool_surface_motion_v1 \
  --formula-version surface_motion_v1 \
  --source-revision "$(git rev-parse HEAD)" \
  --profile physics_models/profiles/chinese_pool_surface_motion_v1.json \
  --executable /tmp/billiardgl-phase3/Billiards \
  --calibration-report physics_models/candidates/surface_motion_v1/calibration/reference_report.json \
  --dataset-manifest tests/physics_validation/reference_data/mathavan_2009_high_speed/manifest.json \
  --created-at 2026-07-14T00:00:00Z \
  --output physics_models/candidates/surface_motion_v1/freeze.json
```

Expected: calibration report contains only `CALIBRATION`, accounting is exact, freeze verification succeeds, and no validation/holdout command has run.

- [ ] **Step 5: Commit the frozen candidate before validation**

```bash
git add src/Billiards/physics_profile.cpp tests/physics_profile_tests.cpp physics_models/profiles/chinese_pool_surface_motion_v1.json physics_models/candidates/surface_motion_v1 tests/physics_validation/reference_data/mathavan_2009_high_speed/expected_model_mismatches.json
python3 scripts/check_text_encoding.py --root .
git diff --cached --check
git commit -m "feat: freeze surface motion candidate v1"
```

### Task 8: Run Validation Once, Preserve the Result, and Complete Regression

**Files:**
- Create: `physics_models/candidates/surface_motion_v1/validation/reference_report.json`
- Create: `physics_models/candidates/surface_motion_v1/validation/reference_points.csv`
- Create: `physics_models/candidates/surface_motion_v1/validation/reference_report.md`
- Create: `physics_models/candidates/surface_motion_v1/validation/validation_receipt.json`
- Modify only from observed output: `tests/physics_validation/reference_data/mathavan_2009_high_speed/expected_model_mismatches.json`
- Modify: `docs/reference-data-packages.md`

**Interfaces:**
- Consumes: committed `surface_motion_v1/freeze.json` without changing its formula or parameters.
- Produces: immutable first validation receipt and full regression accounting.

- [ ] **Step 1: Verify the tree is clean and the freeze matches**

```bash
test -z "$(git status --short)"
python3 -m tools.physics_validation.freeze_candidate \
  --verify physics_models/candidates/surface_motion_v1/freeze.json \
  --profile physics_models/profiles/chinese_pool_surface_motion_v1.json \
  --executable /tmp/billiardgl-phase3/Billiards \
  --calibration-report physics_models/candidates/surface_motion_v1/calibration/reference_report.json
```

Expected: verification passes. If the executable changed after freezing, rebuild from the frozen source revision and generate a new freeze commit before proceeding.

- [ ] **Step 2: Execute the committed validation partition exactly once**

```bash
rm -rf /tmp/billiardgl-surface-validation
python3 -m tools.physics_validation.validation_run \
  --freeze physics_models/candidates/surface_motion_v1/freeze.json \
  --profile physics_models/profiles/chinese_pool_surface_motion_v1.json \
  --executable /tmp/billiardgl-phase3/Billiards \
  --package tests/physics_validation/reference_data/mathavan_2009_high_speed \
  --output /tmp/billiardgl-surface-validation
```

Expected: only committed holdout cases execute. Exit may be nonzero when an old expected mismatch has disappeared or a new one is discovered; numeric reports and the receipt must still be complete and are copied without editing.

- [ ] **Step 3: Reconcile results without tuning the model**

If sliding deceleration passes, remove its old mismatch entry. Register exact remaining Table I or unrelated cushion mismatch keys produced by the run. If sliding fails, retain/add the exact mismatch and document that candidate v1 failed validation; do not change `0.20`, `12.5`, formula branches, tolerances, or sampling windows in this plan.

```bash
mkdir -p physics_models/candidates/surface_motion_v1/validation
cp /tmp/billiardgl-surface-validation/reference_report.* physics_models/candidates/surface_motion_v1/validation/
cp /tmp/billiardgl-surface-validation/reference_points.csv physics_models/candidates/surface_motion_v1/validation/
cp /tmp/billiardgl-surface-validation/validation_receipt.json physics_models/candidates/surface_motion_v1/validation/
```

- [ ] **Step 4: Run full verification after validation is preserved**

```bash
BILLIARDGL_BUILD_DIR=/tmp/billiardgl-phase3 ./scripts/check.sh
python3 -m tools.physics_validation.reference_run \
  --executable /tmp/billiardgl-phase3/Billiards \
  --package tests/physics_validation/reference_data/mathavan_2009_high_speed \
  --output /tmp/billiardgl-surface-full
python3 scripts/check_text_encoding.py --root .
git diff --check
```

Expected: all integration/numerical tests pass; known physical mismatches reconcile exactly; reports contain no unregistered failure. A failed experimental interval remains a reported model mismatch, not a test harness failure.

- [ ] **Step 5: Document observed results and commit immutable validation**

Document formula, parameters, calibration result, validation result, resolved transition limitation, remaining evidence limitations, candidate/freeze/receipt paths, and replay commands.

```bash
git add physics_models/candidates/surface_motion_v1/validation tests/physics_validation/reference_data/mathavan_2009_high_speed/expected_model_mismatches.json docs/reference-data-packages.md
git diff --cached --check
git commit -m "test: preserve surface motion validation v1"
```

## Theme 1 Acceptance

- Linear and angular motion are coupled through contact-point slip.
- Sliding-to-rolling and rolling-to-stop events are resolved inside a tick without reversal.
- Energy is nonincreasing under surface friction, and results converge under time-step subdivision.
- Runtime, direct scenario, automation, and telemetry share the same production model.
- Mathavan rolling calibration and Table I transition windows are executable and fully reported.
- Candidate parameters are frozen before validation; validation artifacts are complete numeric files committed without post-result tuning.
- Unmeasured sidespin decay, source equipment conversion, and theme 3 collision errors remain explicit rather than being absorbed into theme 1.
