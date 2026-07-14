# Ball–Ball Collision Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: use `superpowers:executing-plans` and `superpowers:test-driven-development`. Execute inline, in order, with one commit per task.

**Goal:** Replace the equal-mass velocity-swap collision with one SI-unit rigid-sphere contact model that supports versioned mass, radius, inertia, restitution, friction, spin transfer, source-apparatus scenarios, authoritative telemetry, calibration-only fitting, and frozen multi-dataset validation.

**Architecture:** `BallBallContactModel` resolves one overlapping, approaching pair and returns its complete impulse/energy diagnostics; position correction is a separate mass-weighted operation. `PhysicsStepper` uses the same model for production and scenarios. Scenario v5 and profile-manifest v3 add ball contact parameters while retaining v1–v4 compatibility. The Doménech adapter derives source geometry from committed raw impact angles, fits material parameters from CALIBRATION groups only, and emits executable v5 scenarios. Freeze schema v2 binds both Doménech and Mathavan calibration reports plus the supplemental material-fit artifact before either candidate HOLDOUT is executed.

**Tech Stack:** C++11 core physics and CTest, Python 3 offline reference pipeline, canonical JSON/CSV/Markdown artifacts.

## Global Constraints

- Do not alter `physics_models/candidates/surface_motion_v1/` or `physics_models/candidates/cue_contact_v1/`.
- Keep scenario v1–v4 and profile-manifest v1/v2 readable with their historical compatibility defaults.
- Use SI units inside the contact solver; convert only at `BallState` and telemetry boundaries.
- A velocity impulse is allowed only for an approaching contact. A receding overlap may receive position correction but never another velocity impulse.
- Keep velocity impulse, position correction, surface transition, and later multi-contact solving as separate operations.
- Use only committed CALIBRATION groups for material fitting. Add tests proving that changing HOLDOUT expected values cannot change fitted parameters or emitted calibration scenarios.
- Doménech experimental markers remain grade B. IFR curves and paper-fitted coefficients remain excluded evidence and are not copied as expected values.
- Source apparatus profiles never mutate Chinese Pool defaults. Non-billiard materials remain `TREND_ONLY` even when expressible.
- `author_data_request_pending` and `version_record_pdf_audit_pending` remain until their actual resolution conditions are met.
- Do not predeclare new candidate HOLDOUT mismatches by inspecting candidate results. Preserve unexpected validation failures honestly.
- Until Task 7 is committed, run no Doménech or Mathavan HOLDOUT case with the new candidate. Task 8 runs each candidate HOLDOUT partition exactly once.
- Commit every calibration, freeze, validation, receipt, parameter-fit, full-precision CSV, and Markdown artifact.
- Each task ends in one commit. Do not push unless explicitly requested.

---

### Task 1: Version Ball Contact Parameters and Source Geometry

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
- Add `BallProperties::{inertiaFactor, normalRestitution, frictionCoefficient}`.
- Produce scenario schema v5 and profile-manifest schema v3.
- v1–v4 populate `0.4`, `1.0`, and `0.0` compatibility values without requiring new keys.

- [ ] Add failing tests requiring v5 ball keys `radius_cm`, `mass_kg`, `inertia_factor`, `normal_restitution`, `friction_coefficient`, and `material`; reject nonfinite values, `inertia_factor <= 0`, restitution outside `[0,1]`, and negative friction.
- [ ] Require manifest v3 provenance for every new numeric leaf and prove the frozen v1/v2 profiles still load byte-for-byte.
- [ ] Run RED:

```bash
cmake --build /tmp/billiardgl-phase3 --target BilliardsPhysicsProfileTests BilliardsPhysicsScenarioTests
ctest --test-dir /tmp/billiardgl-phase3 -R 'Billiards(PhysicsProfile|PhysicsScenario)Tests' --output-on-failure
python3 -m unittest tests.physics_validation.test_model_candidate -v
```

- [ ] Implement additive versioning only. Use source geometry in collision/surface calculations through the profile; do not change global rendering geometry.
- [ ] Run GREEN and commit:

```bash
git add src/Billiards/physics_profile.h src/Billiards/physics_profile.cpp src/Billiards/physics_scenario.h src/Billiards/physics_scenario.cpp tools/physics_validation/model_candidate.py tests/physics_profile_tests.cpp tests/physics_scenario_tests.cpp tests/physics_validation/test_model_candidate.py
git diff --cached --check
git commit -m "feat: version ball collision physics profiles"
```

### Task 2: Implement the Standalone Rigid-Sphere Contact Model

**Files:**
- Create: `src/Billiards/ball_ball_contact.h`
- Create: `src/Billiards/ball_ball_contact.cpp`
- Create: `tests/ball_ball_contact_tests.cpp`
- Modify: `CMakeLists.txt`

**Interfaces:**
- Produce `BallBallContactResult resolveBallBallContact(BallState&, BallState&, const BallProperties&, const BallProperties&)`.
- Produce `BallBallContactRegime { NoContact, Separating, Frictionless, Stick, Slip }`.
- Return normal/tangent, arms, pre-contact relative velocity, normal/tangential impulse, energy before/after, penetration, and correction without hiding rejected impulses.

- [ ] Add RED tests for equal and unequal mass/radius, head-on restitution, oblique friction, spin-to-translation coupling, left/right mirror, index permutation, stationary overlap, receding overlap, zero-distance degeneracy, and finite outputs.
- [ ] In SI units use contact arms `r1=R1*n`, `r2=-R2*n`, contact velocities `v+omega×r`, and relative velocity `v2_contact-v1_contact`. Apply:

```text
J_n = -(1+e) v_n / (1/m1 + 1/m2)
J_t* = -v_t / (1/m1 + 1/m2 + |r1×t|²/I1 + |r2×t|²/I2)
|J_t| = min(|J_t*|, mu J_n)
I_i = inertia_factor_i m_i R_i²
```

Use the conservative pair rule `e=min(e1,e2)`, `mu=sqrt(mu1*mu2)`. Apply `-J` to ball 1 and `+J` to ball 2. Require momentum conservation, `|J_t|<=mu J_n`, nonincrease of total kinetic energy for `e<=1`, and post-impact nonapproach within tolerance.
- [ ] Correct penetration separately along `n`, weighted by inverse mass, with a fixed numerical slop declared in the result. Never use expected experimental angles in the solver.
- [ ] Run GREEN and commit:

```bash
cmake --build /tmp/billiardgl-phase3 --target BilliardsBallBallContactTests
ctest --test-dir /tmp/billiardgl-phase3 -R BilliardsBallBallContactTests --output-on-failure
git add CMakeLists.txt src/Billiards/ball_ball_contact.h src/Billiards/ball_ball_contact.cpp tests/ball_ball_contact_tests.cpp
git diff --cached --check
git commit -m "feat: resolve ball ball contact impulses"
```

### Task 3: Replace the Legacy Swap in the Production Stepper

**Files:**
- Modify: `src/Billiards/physics.h`
- Modify: `src/Billiards/physics.cpp`
- Modify: `tests/physics_tests.cpp`
- Modify: `tests/physics_instrumentation_tests.cpp`
- Modify: `tests/automation_physics_scenarios_tests.cpp`

**Interfaces:**
- Add a profile-aware compatibility wrapper for `collideBalls`; production `updatePhysics` calls the standalone model exactly once per pair per tick.
- Collision detection uses `R1+R2` from the profile, not `kBallRadius` or the old `-0.5 cm` threshold.

- [ ] Add failing tests proving profile restitution/mass/friction change production motion, receding overlap has zero velocity impulse, a persistent overlap does not receive duplicate impulse, and direct-model versus runtime results are identical.
- [ ] Replace the velocity swap and teleport correction. Set speeds/motion inputs from the resulting authoritative velocities; leave surface evolution to Theme 1.
- [ ] Keep rendering and pocket geometry on production table specs; source radius affects only scenario physics in this theme.
- [ ] Run focused production and headless E2E tests, then commit:

```bash
cmake --build /tmp/billiardgl-phase3 --target BilliardsPhysicsTests BilliardsPhysicsInstrumentationTests BilliardsAutomationPhysicsScenarioTests Billiards
ctest --test-dir /tmp/billiardgl-phase3 -R 'Billiards(Physics|PhysicsInstrumentation|AutomationPhysicsScenario|HeadlessAutomationE2E)Tests' --output-on-failure
git add src/Billiards/physics.h src/Billiards/physics.cpp tests/physics_tests.cpp tests/physics_instrumentation_tests.cpp tests/automation_physics_scenarios_tests.cpp
git diff --cached --check
git commit -m "feat: use rigid impulses for production ball collisions"
```

### Task 4: Expose and Cross-Check Ball Contact Telemetry

**Files:**
- Modify: `src/Billiards/physics_telemetry.h`
- Modify: `src/Billiards/automation_protocol.cpp`
- Modify: `tests/physics_telemetry_tests.cpp`
- Modify: `tests/automation_protocol_tests.cpp`
- Modify: `tools/physics_validation/analyzer.py`
- Modify: `tests/physics_validation/test_domenech_2023_metrics.py`

**Interfaces:**
- Extend each ball-ball contact record with relative contact velocity, normal/tangential impulses, friction coefficient, regime, energy before/after, correction, and `velocity_impulse_applied`.
- Analyzer independently checks approach/separation, friction cone, energy, and phase selection before trusting experimental metrics.

- [ ] Add RED serialization tests with explicit unit suffixes and lowercase regime names.
- [ ] Add analyzer failures: cone violation or duplicate impulse is `INTEGRATION_MISMATCH`; nonfinite/energy creation is `NUMERICAL_FAILURE`; stable interval disagreement remains `MODEL_MISMATCH`.
- [ ] Preserve immediate-post-impact sampling at the first contact frame and post-transition sampling through the existing stable pure-roll window. Never substitute one phase for the other.
- [ ] Run GREEN and commit:

```bash
cmake --build /tmp/billiardgl-phase3 --target BilliardsPhysicsTelemetryTests BilliardsAutomationProtocolTests
ctest --test-dir /tmp/billiardgl-phase3 -R 'Billiards(PhysicsTelemetry|AutomationProtocol)Tests' --output-on-failure
python3 -m unittest tests.physics_validation.test_domenech_2023_metrics tests.physics_validation.test_analyzer -v
git add src/Billiards/physics_telemetry.h src/Billiards/automation_protocol.cpp tests/physics_telemetry_tests.cpp tests/automation_protocol_tests.cpp tools/physics_validation/analyzer.py tests/physics_validation/test_domenech_2023_metrics.py
git diff --cached --check
git commit -m "feat: expose ball collision physics telemetry"
```

### Task 5: Make Doménech Material Cases Executable With Calibration Isolation

**Files:**
- Modify: `tools/physics_validation/adapters/domenech_2023.py`
- Create: `tools/physics_validation/fit_ball_collision.py`
- Modify: `tests/physics_validation/test_domenech_2023_adapter.py`
- Create: `tests/physics_validation/test_ball_collision_fit.py`
- Modify: `tests/physics_validation/reference_data/domenech_2023_ball_collision/scenario_template.json`
- Modify: `tests/physics_validation/reference_data/domenech_2023_ball_collision/expected_reference_limitations.json`
- Modify: `tests/physics_validation/reference_data/domenech_2023_ball_collision/manifest.json`
- Modify: `docs/reference-data-packages.md`

**Interfaces:**
- Fit one constant restitution/friction pair per source material using only its committed CALIBRATION groups.
- Convert each raw `impact_angle_degrees` to two-ball contact geometry and emit a complete scenario-v5 source profile.

- [ ] Add RED tests requiring all 214 points to be either executable cases or one of the two source-audit limitations. Require deterministic fit/scenarios under point reordering.
- [ ] Add a mutation test that changes every HOLDOUT expected value and proves all fitted parameters and CALIBRATION scenario bytes remain identical.
- [ ] Implement a bounded deterministic calibration search with preregistered ranges `0<=e<=1`, `0<=mu<=1`, stable tie-breaking, and a committed full-precision objective/parameter/sensitivity report. It may read raw impact inputs and CALIBRATION expected outputs; it must never read HOLDOUT expected values while fitting.
- [ ] Place object ball at rest and cue ball at `(R1+R2-slop)` along the committed impact normal, with `80 cm/s` source launch and pure-roll spin. Set source mass/radius/material in the scenario profile. Preserve `CONVERTED` versus `TREND_ONLY` labels.
- [ ] Keep both source-audit limitations. Remove a geometry/material limitation only after its points execute. Post-transition cases must use Theme 1's explicit pure-roll window; if the required PVC surface assumption cannot be expressed honestly, retain a point-scoped phase/material limitation rather than sampling the wrong frame.
- [ ] Run only CALIBRATION:

```bash
python3 -m unittest tests.physics_validation.test_domenech_2023_adapter tests.physics_validation.test_ball_collision_fit -v
rm -rf /tmp/billiardgl-ball-domenech-calibration
python3 -m tools.physics_validation.calibration_run --executable /tmp/billiardgl-phase3/Billiards --package tests/physics_validation/reference_data/domenech_2023_ball_collision --output /tmp/billiardgl-ball-domenech-calibration
```

- [ ] Commit code, updated package hashes/limitations, fit report, and docs:

```bash
git add tools/physics_validation/adapters/domenech_2023.py tools/physics_validation/fit_ball_collision.py tests/physics_validation/test_domenech_2023_adapter.py tests/physics_validation/test_ball_collision_fit.py tests/physics_validation/reference_data/domenech_2023_ball_collision docs/reference-data-packages.md physics_models/calibration/ball_collision_material_fit_v1.json
git diff --cached --check
git commit -m "test: execute Domenech ball collision calibration"
```

### Task 6: Bind Multiple Calibration Reports in Freeze Schema v2

**Files:**
- Modify: `tools/physics_validation/model_candidate.py`
- Modify: `tools/physics_validation/freeze_candidate.py`
- Modify: `tools/physics_validation/validation_run.py`
- Modify: `tests/physics_validation/test_model_candidate.py`
- Modify: `tests/physics_validation/test_validation_run.py`
- Modify: `docs/reference-data-packages.md`

**Interfaces:**
- Freeze schema v2 stores a sorted `calibration_reports` array. Each entry binds dataset/version, report SHA-256, package hashes, and metric targets.
- Freeze schema v2 stores a sorted `supplemental_artifacts` array for the full-precision material-fit report; every entry binds its repository-relative path and SHA-256.
- Schema v1 cue/surface freezes remain loadable and verifiable unchanged.

- [ ] Add RED tests for two reports/two manifests, deterministic ordering, duplicate identity rejection, calibration or supplemental-artifact byte mutation rejection, and validation of either bound package.
- [ ] Make CLI `--calibration-report` and `--supplemental-artifact` repeatable in create/verify modes. Validation of one package verifies the complete freeze and then selects only that package's HOLDOUT.
- [ ] Run Mathavan and Doménech CALIBRATION only with the current executable; do not run either HOLDOUT:

```bash
rm -rf /tmp/billiardgl-ball-mathavan-calibration /tmp/billiardgl-ball-domenech-calibration
python3 -m tools.physics_validation.calibration_run --executable /tmp/billiardgl-phase3/Billiards --package tests/physics_validation/reference_data/mathavan_2009_high_speed --output /tmp/billiardgl-ball-mathavan-calibration
python3 -m tools.physics_validation.calibration_run --executable /tmp/billiardgl-phase3/Billiards --package tests/physics_validation/reference_data/domenech_2023_ball_collision --output /tmp/billiardgl-ball-domenech-calibration
```

- [ ] Preserve complete calibration reports under `physics_models/candidates/ball_collision_v1/calibration/{mathavan_2009,domenech_2023}/` and commit:

```bash
git add tools/physics_validation/model_candidate.py tools/physics_validation/freeze_candidate.py tools/physics_validation/validation_run.py tests/physics_validation/test_model_candidate.py tests/physics_validation/test_validation_run.py docs/reference-data-packages.md physics_models/candidates/ball_collision_v1/calibration
git diff --cached --check
git commit -m "feat: freeze multiple calibration datasets"
```

### Task 7: Freeze Ball Collision Candidate v1 Before HOLDOUT

**Files:**
- Create: `physics_models/profiles/chinese_pool_ball_collision_v1.json`
- Create: `physics_models/candidates/ball_collision_v1/freeze.json`
- Modify: `src/Billiards/physics_profile.cpp`
- Modify: `tests/physics_profile_tests.cpp`
- Modify: `tests/physics_validation/test_model_candidate.py`

**Interfaces:**
- Production ID `chinese_pool_ball_collision_v1`, formula `ball_collision_v1`, manifest schema v3.
- Preserve all Theme 1/2 values exactly; classify new contact values and apparatus conversion honestly.

- [ ] Add failing production-profile assertions and provenance checks. Production restitution/friction may use only the preregistered Doménech billiard CALIBRATION fit; mark geometry/PVC-to-Chinese-Pool transfer and lack of confirmation data explicitly.
- [ ] Rebuild and rerun both CALIBRATION reports once if the default profile ID changed their executable hash. Copy JSON/CSV/Markdown byte-for-byte.
- [ ] Freeze both reports/manifests plus the exact executable/profile and declare analytic gates in applicability. No wording may claim real-world validation before Task 8.
- [ ] Verify freeze, encoding, and focused tests, then commit before HOLDOUT:

```bash
python3 -m tools.physics_validation.freeze_candidate --candidate-id chinese_pool_ball_collision_v1 --formula-version ball_collision_v1 --source-revision "$(git rev-parse HEAD)" --profile physics_models/profiles/chinese_pool_ball_collision_v1.json --executable /tmp/billiardgl-phase3/Billiards --calibration-report physics_models/candidates/ball_collision_v1/calibration/domenech_2023/reference_report.json --calibration-report physics_models/candidates/ball_collision_v1/calibration/mathavan_2009/reference_report.json --dataset-manifest tests/physics_validation/reference_data/domenech_2023_ball_collision/manifest.json --dataset-manifest tests/physics_validation/reference_data/mathavan_2009_high_speed/manifest.json --supplemental-artifact physics_models/calibration/ball_collision_material_fit_v1.json --created-at 2026-07-14T00:00:00Z --output physics_models/candidates/ball_collision_v1/freeze.json
python3 scripts/check_text_encoding.py --root .
git add src/Billiards/physics_profile.cpp tests/physics_profile_tests.cpp tests/physics_validation/test_model_candidate.py physics_models/profiles/chinese_pool_ball_collision_v1.json physics_models/candidates/ball_collision_v1
git diff --cached --check
git commit -m "feat: freeze ball collision candidate v1"
```

### Task 8: Execute Each Frozen HOLDOUT Once and Preserve the Result

**Files:**
- Create: `physics_models/candidates/ball_collision_v1/validation/domenech_2023/*`
- Create: `physics_models/candidates/ball_collision_v1/validation/mathavan_2009/*`
- Modify: `tests/physics_validation/reference_data/domenech_2023_ball_collision/expected_reference_limitations.json`
- Modify: `tests/physics_validation/reference_data/domenech_2023_ball_collision/manifest.json`
- Modify: `tests/physics_validation/test_domenech_2023_adapter.py`
- Modify: `docs/reference-data-packages.md`

- [ ] Require a clean worktree and verify the committed freeze. Execute Doménech HOLDOUT once and Mathavan HOLDOUT once, into distinct empty directories. Never rerun either first event:

```bash
python3 -m tools.physics_validation.validation_run --freeze physics_models/candidates/ball_collision_v1/freeze.json --profile physics_models/profiles/chinese_pool_ball_collision_v1.json --executable /tmp/billiardgl-phase3/Billiards --package tests/physics_validation/reference_data/domenech_2023_ball_collision --output /tmp/billiardgl-ball-validation-domenech
python3 -m tools.physics_validation.validation_run --freeze physics_models/candidates/ball_collision_v1/freeze.json --profile physics_models/profiles/chinese_pool_ball_collision_v1.json --executable /tmp/billiardgl-phase3/Billiards --package tests/physics_validation/reference_data/mathavan_2009_high_speed --output /tmp/billiardgl-ball-validation-mathavan
```

- [ ] Immediately copy each JSON/CSV/Markdown/receipt byte-for-byte. If accounting fails, preserve the first receipt and do not alter formula, parameters, intervals, partitions, or expected candidate mismatches.
- [ ] Reconcile only limitations whose resolution condition was objectively met. Retain source-audit, apparatus conversion, and unmeasured-input limitations. Do not convert finite validation disagreement into a limitation.
- [ ] Run `scripts/check.sh`, package reconstruction, encoding, and freeze verification. These checks must not execute either candidate HOLDOUT again.
- [ ] Document calibration fit, all validation pass/mismatch/failure counts, applicability, immutable paths, and replay governance. Commit:

```bash
git add physics_models/candidates/ball_collision_v1/validation tests/physics_validation/reference_data/domenech_2023_ball_collision tests/physics_validation/test_domenech_2023_adapter.py docs/reference-data-packages.md
git diff --cached --check
git commit -m "test: preserve ball collision validation v1"
```

## Theme 3 Acceptance

- Production and scenario collisions use one contact model with versioned source geometry/material parameters.
- Momentum, energy, friction cone, separation, no-repeat, mirror, permutation, and unequal-property gates pass.
- Ball-ball telemetry is authoritative and analyzer-cross-checked.
- Doménech source geometry is executable without changing Chinese Pool defaults; remaining source/apparatus limitations are exact.
- Calibration-only material fitting is demonstrably independent of HOLDOUT expected values.
- Both calibration reports are bound before either candidate HOLDOUT event.
- First Doménech and Mathavan validation reports/receipts are immutable and honestly accounted.
- Documentation distinguishes analytic/numerical success, experimental agreement, `TREND_ONLY` material evidence, and unresolved confirmation needs.
