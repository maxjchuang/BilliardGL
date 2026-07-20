# Cue–Ball Impact Implementation Plan

**Execution status: Completed and archived.** Unchecked boxes below preserve the original execution script; current disposition and evidence are indexed in [Phase 3 current status](../../phase3-physics-current-status.md).

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Replace direct shot-power-to-ball-velocity assignment with one governed cue-contact model that produces linear velocity, three-dimensional spin, stick/slip/miscue classification, and complete telemetry for both player and automation inputs.

**Architecture:** Add a versioned cue section to `PhysicsProfile` without mutating the frozen surface-motion v1 artifacts. `ShotPowerMapping` converts UI power to a physical `CueImpactInput`; `resolveCueContact` applies one analytic rigid impulse at the declared tip offset; `GameRuntime` and the interactive loop share that path. A committed analytic contract package freezes conservation/friction invariants honestly as non-experimental evidence, while Cross 2023 remains admission-blocked until lawful full text and experimental numbers are available.

**Tech Stack:** C++17 core physics and tests, CMake/CTest, Python 3 reference validation, canonical JSON/CSV/Markdown candidate artifacts.

## Global Constraints

- Do not modify or regenerate `physics_models/candidates/surface_motion_v1/`; it is immutable historical evidence.
- Scenario v2/v3 and profile-manifest schema v1 remain readable and retain their old defaults; extended cue fields require scenario v4 and profile-manifest schema v2.
- The production model is 2.5D: horizontal cue direction and planar ball velocity with full 3D angular velocity. A result requiring non-negligible vertical ball impulse is unsupported, not silently projected.
- Cross 2023 has zero admitted numerical points. Do not remove `full_text_not_acquired`, `experimental_markers_not_admitted`, or claim experimental calibration/validation.
- `cue_speed_to_power_mapping_missing` remains until an independently validated mapping exists; a versioned compatibility mapping does not satisfy that evidence condition.
- Remove `cue_contact_regime_telemetry_missing` only after normal/tangential relative velocity, impulses, regime, and result are serialized from the production path.
- Analytic contract results are evidence grade C and may prove invariants/reproducibility only; they do not authorize a real-world accuracy claim.
- Perform impulse, inertia, momentum, and energy calculations in SI units (`m`, `s`, `kg`, `N·s`, `J`) inside `cue_contact.cpp`; convert only at the `BallState`/telemetry boundary.
- Every generated calibration/validation JSON, full-precision CSV, Markdown report, freeze, and receipt is committed.
- Until the cue candidate freeze commit, run only its analytic CALIBRATION partition. Run analytic HOLDOUT exactly once after the freeze commit.
- Each task ends in one commit; do not push unless the user explicitly requests it.

---

### Task 1: Version the Extended Cue Profile Without Rewriting Frozen v1

**Files:**
- Modify: `src/Billiards/physics_profile.h`
- Modify: `src/Billiards/physics_profile.cpp`
- Modify: `src/Billiards/physics_scenario.h`
- Modify: `src/Billiards/physics_scenario.cpp`
- Modify: `tools/physics_validation/model_candidate.py`
- Modify: `tests/physics_profile_tests.cpp`
- Modify: `tests/physics_scenario_tests.cpp`
- Modify: `tests/physics_validation/test_model_candidate.py`

**Interfaces:**
- Produces: `CueProperties::{effectiveMassKg, normalRestitution, chalkedFrictionCoefficient, unchalkedFrictionCoefficient, maximumReliableOffsetRadius, cueSpeedPerPowerUnitCmS}`.
- Produces: scenario schema v4 and profile-manifest schema v2; old schemas keep compatibility defaults.

- [ ] **Step 1: Add failing version-compatibility tests**

Require schema v4 to parse an exact cue object:

```json
{
  "effective_mass_kg": 0.5,
  "normal_restitution": 0.0,
  "chalked_friction_coefficient": 0.6,
  "unchalked_friction_coefficient": 0.1,
  "maximum_reliable_offset_radius": 0.8,
  "cue_speed_per_power_unit_cm_s": 1.34
}
```

Require v3 to accept only `effective_mass_kg` and populate the other fields from compatibility defaults. Require profile-manifest schema v2 to cover every new numeric leaf, while the committed surface profile schema v1 still loads byte-for-byte.

- [ ] **Step 2: Run RED tests**

```bash
cmake --build /tmp/billiardgl-phase3 --target BilliardsPhysicsProfileTests BilliardsPhysicsScenarioTests
ctest --test-dir /tmp/billiardgl-phase3 -R 'Billiards(PhysicsProfile|PhysicsScenario)Tests' --output-on-failure
python3 -m unittest tests.physics_validation.test_model_candidate -v
```

Expected: v4 and manifest-v2 assertions fail because only v3/v1 exist.

- [ ] **Step 3: Implement additive schema versioning**

Add the six fields with the exact defaults above. Set `kPhysicsScenarioVersion = 4`. In `parsePhysicsProfile`, select the cue key set from the enclosing scenario version: v3 reads only effective mass; v4 requires all six fields. Update validation to require finite `0 <= restitution <= 1`, nonnegative friction, `0 < maximumReliableOffsetRadius < 1`, and positive mapping scale.

In Python, keep `_RUNTIME_SECTION_KEYS_V1` unchanged and add `_RUNTIME_SECTION_KEYS_V2`; select by profile manifest `schema_version`. Do not rewrite the old profile or freeze.

- [ ] **Step 4: Run GREEN compatibility tests**

Run the Step 2 commands. Expected: all pass, including loading the old frozen profile.

- [ ] **Step 5: Commit**

```bash
git add src/Billiards/physics_profile.h src/Billiards/physics_profile.cpp src/Billiards/physics_scenario.h src/Billiards/physics_scenario.cpp tools/physics_validation/model_candidate.py tests/physics_profile_tests.cpp tests/physics_scenario_tests.cpp tests/physics_validation/test_model_candidate.py
git diff --cached --check
git commit -m "feat: version extended cue physics profiles"
```

### Task 2: Add a Versioned Shot-Power Mapping

**Files:**
- Modify: `src/Billiards/cue_impact.h`
- Modify: `src/Billiards/shot.h`
- Modify: `src/Billiards/shot.cpp`
- Modify: `tests/shot_tests.cpp`

**Interfaces:**
- Produces: `CueImpactInput cueImpactFromShotControls(float yaw, float shotPower, const PhysicsProfile&)`.
- Preserves: rendering helpers and `shotVelocityFromAim` for temporary source compatibility only.

- [ ] **Step 1: Add failing mapping tests**

For yaw zero, power `40`, and scale `1.34`, require cue speed `53.6 cm/s`, mass from the profile, unit direction `[1,0,0]`, zero elevation/offset, and chalk state `CHALKED`. Require powers `0` and `200` to map monotonically and finite negative/nonfinite power to be rejected by the caller rather than clamped here.

- [ ] **Step 2: Run RED**

```bash
cmake --build /tmp/billiardgl-phase3 --target BilliardsShotTests
ctest --test-dir /tmp/billiardgl-phase3 -R BilliardsShotTests --output-on-failure
```

Expected: missing mapping API.

- [ ] **Step 3: Implement the pure mapping**

Use `aimDirectionOnTable(yaw)` and:

```cpp
input.cueSpeedCmS = shotPower * profile.cue.cueSpeedPerPowerUnitCmS;
input.cueMassKg = profile.cue.effectiveMassKg;
input.direction = {{direction.x, 0.0, direction.z}};
input.tipOffsetCm = {{0.0, 0.0}};
input.tipOffsetRadius = {{0.0, 0.0}};
input.chalkState = "CHALKED";
```

Do not read expected ball speed or Cross outputs.

- [ ] **Step 4: Run GREEN and commit**

```bash
cmake --build /tmp/billiardgl-phase3 --target BilliardsShotTests
ctest --test-dir /tmp/billiardgl-phase3 -R BilliardsShotTests --output-on-failure
git add src/Billiards/cue_impact.h src/Billiards/shot.h src/Billiards/shot.cpp tests/shot_tests.cpp
git diff --cached --check
git commit -m "feat: map shot power to physical cue input"
```

### Task 3: Implement the Analytic Cue Contact Model

**Files:**
- Create: `src/Billiards/cue_contact.h`
- Create: `src/Billiards/cue_contact.cpp`
- Create: `tests/cue_contact_tests.cpp`
- Modify: `CMakeLists.txt`

**Interfaces:**
- Produces: `enum class CueContactRegime { Unsupported, Stick, Slip, Miscue }`.
- Produces: `CueContactResult resolveCueContact(BallState&, const CueImpactInput&, const BallProperties&, const CueProperties&)`.

- [ ] **Step 1: Add failing analytic tests**

Cover center hit, positive/negative vertical offset, left/right mirror, stick/slip boundary, unchalked friction, miscue at offset fraction `> 0.8`, moving/spinning cue ball, zero speed, nonapproaching input, and nonzero elevation. Assert finite results, `|J_t| <= mu J_n`, center-hit zero added spin, mirrored spin signs, and total cue-plus-ball kinetic energy does not increase for restitution in `[0,1]`.

- [ ] **Step 2: Run RED**

```bash
cmake --build /tmp/billiardgl-phase3 --target BilliardsCueContactTests
ctest --test-dir /tmp/billiardgl-phase3 -R BilliardsCueContactTests --output-on-failure
```

Expected: target/API absent.

- [ ] **Step 3: Implement contact geometry and impulse**

For horizontal unit cue direction `n`, define `side=(-n.z,0,n.x)`, offset `o=side*x+up*y`, and contact arm:

```text
r = -n * sqrt(R^2 - |o|^2) + o
q = -r / R
I = 0.4 * m_ball * (R/100)^2
k_axis = 1/m_cue + 1/m_ball + |r_m × n|^2/I
P = (1+e) * max(0, (v_cue-v_contact)·n) / k_axis
J_desired = P*n
J_n = J_desired·q
J_t_desired = J_desired - J_n*q
```

Use chalk state to choose `mu`. Stick applies `J_desired` when `|J_t_desired| <= mu*J_n`. Slip clamps the tangent to `mu*J_n`. Offset above the reliable fraction is `Miscue` with no applied impulse. Apply `Δv=J/m_ball` and `Δω=I^-1(r_m×J)`. If slip produces `|J.y| > 1e-9 Ns`, return `Unsupported` with `vertical_ball_impulse_requires_3d` and do not mutate the ball.

- [ ] **Step 4: Run analytic and split-sign tests**

Run Step 2. Expected: all invariants pass.

- [ ] **Step 5: Commit**

```bash
git add CMakeLists.txt src/Billiards/cue_contact.h src/Billiards/cue_contact.cpp tests/cue_contact_tests.cpp
git diff --cached --check
git commit -m "feat: resolve cue ball contact impulses"
```

### Task 4: Route Player, Automation, and Scenario Shots Through One Contact Path

**Files:**
- Modify: `src/Billiards/game_runtime.h`
- Modify: `src/Billiards/game_runtime.cpp`
- Modify: `src/Billiards/billiards.cpp`
- Modify: `src/Billiards/physics_scenario.cpp`
- Modify: `tests/game_runtime_tests.cpp`
- Modify: `tests/physics_scenario_tests.cpp`
- Modify: `tests/automation_controller_tests.cpp`

**Interfaces:**
- Produces: `ActionResult GameRuntime::applyCueImpact(const CueImpactInput&)`.
- Produces: `CueShotApplication applyCueShot(GameState&, const CueImpactInput&, const PhysicsProfile&)`, containing the contact result and stable action error.
- Produces: the same `CueContactResult` for UI `Shoot` and scenario cue input.

- [ ] **Step 1: Add failing production-path tests**

Require a normal player shot to use `cueImpactFromShotControls`; require a v4 scenario with a supported horizontal impact to execute exactly once; require vertical slip/elevation/miscue to return stable errors without changing the ball. Require UI and explicit equivalent `CueImpactInput` to produce byte-equal ball velocity/angular velocity.

- [ ] **Step 2: Run RED focused tests**

```bash
cmake --build /tmp/billiardgl-phase3 --target BilliardsGameRuntimeTests BilliardsPhysicsScenarioTests BilliardsAutomationControllerTests
ctest --test-dir /tmp/billiardgl-phase3 -R 'Billiards(GameRuntime|PhysicsScenario|AutomationController)Tests' --output-on-failure
```

- [ ] **Step 3: Integrate one authoritative path**

Make `applyShot()` construct input then call `applyCueImpact`, which delegates physical mutation and common shot-state transitions to `applyCueShot`. `applyPhysicsScenario` applies the scenario state/profile and then the declared cue impact; it must not preload output velocity. Replace the interactive direct `setBallVelocity` block with `applyCueShot(Game, input, ProductionPhysicsProfile)`; only sound playback remains interactive-only.

Set `CueImpactSupport.shotExecuted` from the actual result. Exactly consumed fields include speed, mass, horizontal direction, horizontal/vertical offset, and explicit chalk state only when used.

- [ ] **Step 4: Run GREEN plus headless E2E**

```bash
cmake --build /tmp/billiardgl-phase3 --target BilliardsGameRuntimeTests BilliardsPhysicsScenarioTests BilliardsAutomationControllerTests Billiards
ctest --test-dir /tmp/billiardgl-phase3 -R 'Billiards(GameRuntime|PhysicsScenario|AutomationController|HeadlessAutomationE2E)Tests' --output-on-failure
```

- [ ] **Step 5: Commit**

```bash
git add src/Billiards/game_runtime.h src/Billiards/game_runtime.cpp src/Billiards/billiards.cpp src/Billiards/physics_scenario.cpp tests/game_runtime_tests.cpp tests/physics_scenario_tests.cpp tests/automation_controller_tests.cpp
git diff --cached --check
git commit -m "feat: use cue contact for every shot path"
```

### Task 5: Serialize Cue Contact Telemetry and Analyze It

**Files:**
- Modify: `src/Billiards/cue_impact.h`
- Modify: `src/Billiards/physics_telemetry.h`
- Modify: `src/Billiards/physics_telemetry.cpp`
- Modify: `src/Billiards/automation_protocol.cpp`
- Modify: `tests/physics_telemetry_tests.cpp`
- Modify: `tests/automation_protocol_tests.cpp`
- Modify: `tools/physics_validation/analyzer.py`
- Modify: `tests/physics_validation/test_cross_2023_metrics.py`

**Interfaces:**
- Trace fields: cue velocities before/after, contact offset/arm, normal/tangential relative speed, normal/tangential impulse, `stick|slip|miscue|unsupported`, input/output energy, applied flag, error code.
- Analyzer metrics: `cue_contact_normal_impulse_ns`, `cue_contact_tangential_impulse_ns`, `cue_contact_energy_efficiency`, plus the existing signed linear/angular post-impact metrics. Selection metadata includes `expected_regime` and is cross-checked against the trace.

- [ ] **Step 1: Add failing serialization/analyzer tests**

Require finite full-precision fields and lowercase stable regime names. Analyzer must reject a trace claiming stick outside the friction cone as `INTEGRATION_MISMATCH`, nonfinite energy as `NUMERICAL_FAILURE`, and an unsupported 3D result as `REFERENCE_LIMITATION`.

- [ ] **Step 2: Run RED**

```bash
cmake --build /tmp/billiardgl-phase3 --target BilliardsPhysicsTelemetryTests BilliardsAutomationProtocolTests
ctest --test-dir /tmp/billiardgl-phase3 -R 'Billiards(PhysicsTelemetry|AutomationProtocol)Tests' --output-on-failure
python3 -m unittest tests.physics_validation.test_cross_2023_metrics -v
```

- [ ] **Step 3: Capture the event independently of physics ticks**

Store the latest cue result in `GameRuntime`; attach it to the first trace frame after the shot and clear the pending marker after serialization. Serialize vectors with explicit `*_ns`, `*_cm_s`, `*_rad_s`, and `*_j` suffixes. Cross-check regime and friction cone in the analyzer rather than trusting the label.

- [ ] **Step 4: Run GREEN and commit**

Run Step 2, then:

```bash
git add src/Billiards/cue_impact.h src/Billiards/physics_telemetry.h src/Billiards/physics_telemetry.cpp src/Billiards/automation_protocol.cpp tests/physics_telemetry_tests.cpp tests/automation_protocol_tests.cpp tools/physics_validation/analyzer.py tests/physics_validation/test_cross_2023_metrics.py
git diff --cached --check
git commit -m "feat: expose cue contact physics telemetry"
```

### Task 6: Add an Honest Analytic Cue Contract Package

**Files:**
- Create: `tests/physics_validation/reference_data/cue_contact_analytic_contract/*`
- Create: `tools/physics_validation/adapters/cue_contact_analytic.py`
- Create: `tools/physics_validation/generate_cue_contact_analytic.py`
- Create: `tests/physics_validation/test_cue_contact_analytic_adapter.py`
- Modify: `tools/physics_validation/reference_adapter.py`
- Modify: `tests/physics_validation/validation_data_status.json`
- Modify: `docs/reference-data-packages.md`

**Interfaces:**
- Produces: deterministic calibration/holdout cases for conservation, mirror symmetry, center hit, stick cone, slip clamp, miscue, and 2.5D rejection.
- Does not produce: experimental accuracy evidence.

- [ ] **Step 1: Commit exact analytic points and failing adapter tests**

Use a Python `decimal.Decimal` generator that independently evaluates the equations printed in the package extraction metadata; it must not import or execute the production cue-contact implementation. Commit its full-precision raw and normalized outputs with source locators such as `analytic:rigid-impulse:center-hit`, evidence grade C, and `pool_applicability=NOT_APPLICABLE`. Calibration cases are center hit, positive/negative offset, and stick boundary. HOLDOUT cases are left/right mirror, horizontal slip clamp, and miscue. Unsupported vertical-slip/elevation behavior remains a C++ hard-constraint test rather than an executable reference case. No expected model mismatch is allowed.

- [ ] **Step 2: Run RED package tests**

```bash
python3 -m unittest tests.physics_validation.test_cue_contact_analytic_adapter -v
```

- [ ] **Step 3: Implement canonical v4 adaptation**

The adapter emits `cue_impact` input and a complete schema-v4 profile; it never writes post-impact ball velocity/spin into initial state. Register `cue_contact_analytic_v1` and add lifecycle state `calibration/validation`.

- [ ] **Step 4: Run calibration only**

```bash
python3 -m tools.physics_validation.calibration_run --executable /tmp/billiardgl-phase3/Billiards --package tests/physics_validation/reference_data/cue_contact_analytic_contract --output /tmp/billiardgl-cue-calibration
```

Expected: CALIBRATION points pass, HOLDOUT contains zero rows, and accounting has no failures.

- [ ] **Step 5: Commit**

```bash
git add tests/physics_validation/reference_data/cue_contact_analytic_contract tools/physics_validation/adapters/cue_contact_analytic.py tools/physics_validation/generate_cue_contact_analytic.py tests/physics_validation/test_cue_contact_analytic_adapter.py tools/physics_validation/reference_adapter.py tests/physics_validation/validation_data_status.json docs/reference-data-packages.md
git diff --cached --check
git commit -m "test: add analytic cue contact contract"
```

### Task 7: Freeze the Cue Contact Candidate

**Files:**
- Create: `physics_models/profiles/chinese_pool_cue_contact_v1.json`
- Create: `physics_models/candidates/cue_contact_v1/calibration/reference_report.json`
- Create: `physics_models/candidates/cue_contact_v1/calibration/reference_points.csv`
- Create: `physics_models/candidates/cue_contact_v1/calibration/reference_report.md`
- Create: `physics_models/candidates/cue_contact_v1/freeze.json`
- Modify: `src/Billiards/physics_profile.cpp`
- Modify: `tests/physics_profile_tests.cpp`

**Interfaces:**
- Produces: production profile `chinese_pool_cue_contact_v1`, formula `cue_contact_v1`, with every numeric source classified.

- [ ] **Step 1: Add failing production-profile assertions**

Require the six cue values from Task 1 and unchanged surface-motion v1 values. Require profile schema v2 and explicit provenance: `0.6/0.1/0.8` are preregistered hypotheses or Cross-2008-supported mechanism values, `1.34` is a compatibility mapping with `experimental_validation=false`, and Cross-2023 validation is blocked.

- [ ] **Step 2: Run RED then install the profile**

```bash
cmake --build /tmp/billiardgl-phase3 --target BilliardsPhysicsProfileTests
ctest --test-dir /tmp/billiardgl-phase3 -R BilliardsPhysicsProfileTests --output-on-failure
```

Change the default ID/formula and write canonical manifest-v2 provenance without editing the old surface candidate.

- [ ] **Step 3: Rebuild and regenerate CALIBRATION once**

```bash
cmake --build /tmp/billiardgl-phase3 --target Billiards
rm -rf /tmp/billiardgl-cue-calibration
python3 -m tools.physics_validation.calibration_run --executable /tmp/billiardgl-phase3/Billiards --package tests/physics_validation/reference_data/cue_contact_analytic_contract --output /tmp/billiardgl-cue-calibration
```

Require exit zero; copy JSON, `reference_points.csv`, and Markdown byte-for-byte.

- [ ] **Step 4: Freeze with an explicit analytic-only applicability**

```bash
python3 -m tools.physics_validation.freeze_candidate --candidate-id chinese_pool_cue_contact_v1 --formula-version cue_contact_v1 --source-revision "$(git rev-parse HEAD)" --profile physics_models/profiles/chinese_pool_cue_contact_v1.json --executable /tmp/billiardgl-phase3/Billiards --calibration-report physics_models/candidates/cue_contact_v1/calibration/reference_report.json --dataset-manifest tests/physics_validation/reference_data/cue_contact_analytic_contract/manifest.json --created-at 2026-07-14T00:00:00Z --output physics_models/candidates/cue_contact_v1/freeze.json
```

Profile applicability must say `analytic_contract_passed` and `experimental_validation_blocked`; no wording may say real-world validated.

- [ ] **Step 5: Commit before HOLDOUT**

```bash
git add src/Billiards/physics_profile.cpp tests/physics_profile_tests.cpp physics_models/profiles/chinese_pool_cue_contact_v1.json physics_models/candidates/cue_contact_v1
python3 scripts/check_text_encoding.py --root .
git diff --cached --check
git commit -m "feat: freeze analytic cue contact candidate v1"
```

### Task 8: Execute Analytic HOLDOUT Once and Preserve Theme 2 Evidence

**Files:**
- Create: `physics_models/candidates/cue_contact_v1/validation/reference_report.json`
- Create: `physics_models/candidates/cue_contact_v1/validation/reference_points.csv`
- Create: `physics_models/candidates/cue_contact_v1/validation/reference_report.md`
- Create: `physics_models/candidates/cue_contact_v1/validation/validation_receipt.json`
- Modify: `tools/physics_validation/adapters/cross_2023.py`
- Modify: `tests/physics_validation/reference_data/cross_2023_cue_impact/expected_reference_limitations.json`
- Modify: `tests/physics_validation/reference_data/cross_2023_cue_impact/manifest.json`
- Modify: `tests/physics_validation/test_cross_2023_adapter.py`
- Modify: `docs/reference-data-packages.md`

**Interfaces:**
- Preserves: immutable first analytic HOLDOUT and honest Cross evidence blockers.
- Removes only: `cue_contact_regime_telemetry_missing` after production telemetry proves its resolution condition.

- [ ] **Step 1: Verify clean freeze and execute HOLDOUT exactly once**

```bash
test -z "$(git status --short)"
python3 -m tools.physics_validation.freeze_candidate --verify physics_models/candidates/cue_contact_v1/freeze.json --profile physics_models/profiles/chinese_pool_cue_contact_v1.json --executable /tmp/billiardgl-phase3/Billiards --calibration-report physics_models/candidates/cue_contact_v1/calibration/reference_report.json
rm -rf /tmp/billiardgl-cue-validation
python3 -m tools.physics_validation.validation_run --freeze physics_models/candidates/cue_contact_v1/freeze.json --profile physics_models/profiles/chinese_pool_cue_contact_v1.json --executable /tmp/billiardgl-phase3/Billiards --package tests/physics_validation/reference_data/cue_contact_analytic_contract --output /tmp/billiardgl-cue-validation
```

- [ ] **Step 2: Preserve first artifacts without rerunning**

Copy all four files byte-for-byte. If accounting fails, change only exact diagnostic manifests; never change formula, parameters, expectations, partitions, or the first receipt.

- [ ] **Step 3: Reconcile Cross limitations honestly**

Remove `cue_contact_regime_telemetry_missing`. Retain `full_text_not_acquired`, `experimental_markers_not_admitted`, and `cue_speed_to_power_mapping_missing`. Update package hashes and tests. Do not invent Cross points.

- [ ] **Step 4: Run full verification**

```bash
BILLIARDGL_BUILD_DIR=/tmp/billiardgl-phase3 ./scripts/check.sh
python3 -m tools.physics_validation.reference_run --executable /tmp/billiardgl-phase3/Billiards --package tests/physics_validation/reference_data/cue_contact_analytic_contract --output /tmp/billiardgl-cue-full
python3 -m tools.physics_validation.reference_run --executable /tmp/billiardgl-phase3/Billiards --package tests/physics_validation/reference_data/cross_2023_cue_impact --output /tmp/billiardgl-cross-full
python3 scripts/check_text_encoding.py --root .
git diff --check
```

Expected: all analytic points pass with no integration/numerical failures; Cross reports exactly three known limitations and zero numerical claims.

- [ ] **Step 5: Document and commit**

Document formula, parameter evidence, analytic calibration/validation, remaining Cross blockers, candidate/freeze/receipt paths, and replay commands.

```bash
git add physics_models/candidates/cue_contact_v1/validation tools/physics_validation/adapters/cross_2023.py tests/physics_validation/reference_data/cross_2023_cue_impact tests/physics_validation/test_cross_2023_adapter.py docs/reference-data-packages.md
git diff --cached --check
git commit -m "test: preserve cue contact validation v1"
```

## Theme 2 Acceptance

- UI, automation, and direct scenario shots use one `CueContactModel`.
- Center, offset, mirror, friction-cone, energy, miscue, and unsupported-3D hard constraints pass.
- Cue contact telemetry is authoritative and analyzer-cross-checked.
- The formula/profile and complete analytic reports are frozen before analytic HOLDOUT.
- Cross contact-telemetry limitation is resolved; missing lawful full text, experimental markers, and independent UI-power mapping remain explicit.
- Documentation calls the candidate analytic-only and does not claim real-world cue validation.
