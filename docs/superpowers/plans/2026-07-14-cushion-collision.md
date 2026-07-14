# Cushion Collision Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: use `superpowers:executing-plans` and `superpowers:test-driven-development`. Execute inline, in order, with one commit per task.

**Goal:** Replace the lossless axis reflection with one versioned rigid-cushion contact model that accounts for restitution, friction, cushion-nose height, spin coupling, source equipment, speed-domain limits, authoritative telemetry, calibration-only fitting, and governed validation.

**Architecture:** `CushionContactModel` resolves one approaching ball against an inward rail normal in SI units and returns the complete impulse, energy, spin, penetration, and domain diagnostics. The production stepper locates the earliest straight-rail impact in the tick, advances surface motion to the event, applies the same standalone contact model used by scenarios, and advances the remainder. Scenario v6/profile-manifest v4 carry cushion material and geometry without changing v1–v5 compatibility. A deterministic fitter reads only Mathavan 2010 `incident_low` CALIBRATION rows. Freeze schema v2 binds Mathavan 2010 and Mathavan 2009 calibration reports plus the full-precision fit before either cushion HOLDOUT is executed.

**Tech Stack:** C++11 core physics and CTest, Python 3 offline reference pipeline, canonical JSON/CSV/Markdown artifacts.

## Global Constraints

- Preserve every frozen candidate under `physics_models/candidates/{surface_motion_v1,cue_contact_v1,ball_collision_v1}/` byte-for-byte.
- Keep scenario v1–v5 and profile-manifest v1–v3 readable with their historical cushion defaults.
- Use SI units inside the contact solver; convert only at `BallState`, profile, and telemetry boundaries.
- Use inward rail normals. Apply velocity impulse only when contact-point normal velocity is approaching. Position correction is separate and cannot add energy.
- Model only straight rails. Curved pocket jaws, throats, and capture belong to Theme 5; simultaneous contact islands and general CCD belong to Theme 6.
- The contact arm is `r = -R n + (h-R) y`, where `h/R` is the versioned cushion-nose height ratio. Tangential friction acts along the horizontal rail tangent and may change translation plus three-dimensional spin without introducing vertical ball velocity.
- Candidate v1 uses a constant restitution/friction pair. Do not introduce a speed function unless CALIBRATION evidence and the source model independently require it.
- `maximum_rigid_incident_speed_cm_s` is an applicability boundary, not a clamp or tuning parameter. Above-domain experimental points still execute and remain visibly domain-limited.
- Mathavan 2009 cushion rows are already committed HOLDOUT and must not be used for fitting. Bind its CALIBRATION report as a combined-profile regression, then retain its cushion rows for post-freeze validation.
- Do not widen experimental intervals, change partitions, or predeclare new candidate HOLDOUT mismatches after seeing results.
- Until Task 7 is committed, run no Mathavan 2010 or Mathavan 2009 candidate HOLDOUT. Task 8 runs each frozen HOLDOUT partition exactly once.
- Commit every fit, calibration, freeze, validation, receipt, full-precision CSV, and Markdown artifact. Each task ends in one commit. Do not push unless explicitly requested.

---

### Task 1: Version Cushion Geometry, Material, and Domain

**Files:**
- Modify: `src/Billiards/physics_profile.h`
- Modify: `src/Billiards/physics_profile.cpp`
- Modify: `src/Billiards/physics_scenario.h`
- Modify: `src/Billiards/physics_scenario.cpp`
- Modify: `tools/physics_validation/model_candidate.py`
- Modify: `tests/physics_profile_tests.cpp`
- Modify: `tests/physics_scenario_tests.cpp`
- Modify: `tests/physics_validation/test_model_candidate.py`

**Interfaces:** Add `CushionProperties::{noseHeightRatio, maximumRigidIncidentSpeedCmS, material}`. Produce scenario v6 and profile-manifest v4. v1–v5 use `noseHeightRatio=1`, an effectively unbounded finite speed domain, and `legacy_rigid_rail` without requiring new keys.

- [ ] Add RED parse/validation tests for finite `normal_restitution`, `friction_coefficient`, `nose_height_ratio`, `maximum_rigid_incident_speed_cm_s`, and safe `material`; require `0<=e<=1`, `mu>=0`, `0<h/R<2`, and positive maximum speed.
- [ ] Require manifest v4 provenance for every new numeric leaf and prove all committed v1/v2/v3 profiles and freezes still load unchanged.
- [ ] Implement additive versioning only; do not change production defaults in this task.
- [ ] Run focused C++/Python tests and commit `feat: version cushion collision physics profiles`.

### Task 2: Implement the Standalone Rigid-Cushion Contact Model

**Files:**
- Create: `src/Billiards/cushion_contact.h`
- Create: `src/Billiards/cushion_contact.cpp`
- Create: `tests/cushion_contact_tests.cpp`
- Modify: `CMakeLists.txt`

**Interfaces:** Produce `CushionContactResult resolveCushionContact(BallState&, const Point3& inwardNormal, double penetrationM, const BallProperties&, const CushionProperties&)` and regimes `NoContact`, `Separating`, `Frictionless`, `Stick`, `Slip`.

- [ ] RED-test head-on restitution, oblique friction, topspin/backspin/sidespin direction, left/right and top/bottom mirrors, nose-height torque, receding overlap, stationary penetration, friction cone, energy, finite outputs, and speed-domain flagging.
- [ ] Compute contact velocity `v + omega x r`, normal impulse with rigid-wall effective mass, and a horizontal rail-tangent impulse clamped by `|Jt|<=mu*Jn`. Apply impulse once; correct penetration separately along the inward normal with declared slop.
- [ ] Return contact arm/tangent, before/after contact velocity, impulses, linear/angular state changes, energy, correction, incident speed, domain flag, and rejected-impulse diagnostics.
- [ ] Run the new hard-gate tests and commit `feat: resolve rigid cushion contact impulses`.

### Task 3: Use Swept Straight-Rail Contacts in Production

**Files:**
- Modify: `src/Billiards/physics.h`
- Modify: `src/Billiards/physics.cpp`
- Modify: `tests/physics_tests.cpp`
- Modify: `tests/physics_instrumentation_tests.cpp`
- Modify: `tests/automation_physics_scenarios_tests.cpp`

**Interfaces:** Replace `collideWithTableEdge` reflection with a profile-aware wrapper and earliest straight-rail event result. Preserve the current pocket-mouth exclusion until Theme 5.

- [ ] RED-test profile-dependent rebound/spin, no impulse for a receding overlap, no adjacent-tick repeat, high-speed straight-rail impact, mirrored boundaries, corner event tie-break, and direct-model/runtime equivalence.
- [ ] Find the earliest x/z rail crossing during the tick, advance Theme 1 surface motion to that event, apply one cushion impulse, then advance the remainder. Keep stable x-before-z tie-breaking and do not implement pocket jaws or general contact islands.
- [ ] Use profile ball radius and table dimensions; remove the lossless sign flip and post-hoc impulse reconstruction.
- [ ] Run production/headless E2E tests and commit `feat: use swept cushion contacts in production`.

### Task 4: Expose and Cross-Check Cushion Telemetry

**Files:**
- Modify: `src/Billiards/physics_telemetry.h`
- Modify: `src/Billiards/automation_protocol.cpp`
- Modify: `tests/physics_telemetry_tests.cpp`
- Modify: `tests/automation_protocol_tests.cpp`
- Modify: `tools/physics_validation/analyzer.py`
- Modify: `tests/physics_validation/test_mathavan_2010_metrics.py`

**Interfaces:** Rail records expose contact arm/height/tangent, before/after contact velocity, normal/tangential impulses, friction, regime, energy, correction, incident speed, rigid-domain maximum, domain-exceeded, time-of-impact, and `velocity_impulse_applied` with explicit units.

- [ ] RED-test serialization and analyzer checks for approach/separation, friction cone, energy, duplicate rail impulse, domain labeling, and exact selection of the first rail event.
- [ ] Classify nonfinite/energy creation as `NUMERICAL_FAILURE`, incomplete/cone/duplicate/phase errors as `INTEGRATION_MISMATCH`, finite experimental disagreement as `MODEL_MISMATCH`, and above-domain results with the existing source limitation still visible.
- [ ] Run telemetry/analyzer tests and commit `feat: expose cushion collision telemetry`.

### Task 5: Fit the Mathavan 2010 Rigid-Cushion Candidate From CALIBRATION Only

**Files:**
- Create: `tools/physics_validation/fit_cushion.py`
- Create: `tests/physics_validation/test_cushion_fit.py`
- Modify: `tools/physics_validation/adapters/mathavan_2010.py`
- Modify: `tests/physics_validation/test_mathavan_2010_adapter.py`
- Modify: `tests/physics_validation/reference_data/mathavan_2010_cushion/scenario_template.json`
- Modify: `tests/physics_validation/reference_data/mathavan_2010_cushion/expected_model_mismatches.json`
- Modify: `tests/physics_validation/reference_data/mathavan_2010_cushion/manifest.json`
- Create: `physics_models/calibration/cushion_fit_v1.json`
- Modify: `docs/reference-data-packages.md`

- [ ] RED-test deterministic bounded fitting under point reordering and prove that changing every HOLDOUT expected value cannot change fitted parameters or CALIBRATION scenario bytes.
- [ ] Fit only a constant `(e,mu)` against Mathavan 2010 `incident_low`; use the committed source `h/R=7/5`, source mass/radius/material, and `250 cm/s` rigid-domain maximum. Preserve the paper's `(0.98,0.14)` report as provenance/sensitivity, not experimental expected values.
- [ ] Make every Fig. 7 point executable through scenario v6; retain all four angle/spin/equipment/domain limitations and never promote `TREND_ONLY` evidence.
- [ ] Run only Mathavan 2010 CALIBRATION, commit the full-precision objective/sensitivity artifact and reports, and commit `test: calibrate rigid cushion collision`.

### Task 6: Preserve Both Pre-Freeze Calibration Reports

**Files:**
- Create: `physics_models/candidates/cushion_collision_v1/calibration/mathavan_2010/*`
- Create: `physics_models/candidates/cushion_collision_v1/calibration/mathavan_2009/*`
- Modify: `docs/reference-data-packages.md`

- [ ] Rebuild the executable and run Mathavan 2010 CALIBRATION plus Mathavan 2009 CALIBRATION only; confirm both reports contain zero HOLDOUT points.
- [ ] Copy JSON/CSV/Markdown byte-for-byte, record hashes/counts, and verify no Mathavan 2009 result enters `fit_cushion.py`.
- [ ] Run package reconstruction and commit `test: preserve cushion calibration reports`.

### Task 7: Freeze Cushion Candidate v1 Before HOLDOUT

**Files:**
- Create: `physics_models/profiles/chinese_pool_cushion_collision_v1.json`
- Create: `physics_models/candidates/cushion_collision_v1/freeze.json`
- Modify: `src/Billiards/physics_profile.cpp`
- Modify: production-profile assertions and protocol/runtime ID tests

- [ ] RED-test production ID `chinese_pool_cushion_collision_v1`, formula `cushion_collision_v1`, unchanged Theme 1–3 values, fitted cushion parameters, full provenance, domain boundary, and explicit snooker-to-pool/angle/spin limitations.
- [ ] Rebuild and rerun both CALIBRATION reports if the executable hash changes. Freeze both reports/manifests and `cushion_fit_v1.json` with schema v2; make no real-world validation claim.
- [ ] Require clean full checks and freeze verification, then commit `feat: freeze cushion collision candidate v1` before any HOLDOUT.

### Task 8: Execute Each Frozen Cushion HOLDOUT Once and Preserve the Result

**Files:**
- Create: `physics_models/candidates/cushion_collision_v1/validation/mathavan_2010/*`
- Create: `physics_models/candidates/cushion_collision_v1/validation/mathavan_2009/*`
- Modify only objectively resolved limitations/accounting files
- Modify: `docs/reference-data-packages.md`

- [ ] Require a clean worktree and verify the committed freeze. Execute Mathavan 2010 HOLDOUT once and Mathavan 2009 HOLDOUT once into distinct empty directories; never rerun either first event.
- [ ] Immediately copy JSON/CSV/Markdown/receipt byte-for-byte. Preserve failures without changing formula, parameters, intervals, partitions, or preregistered mismatch entries.
- [ ] Reconcile only limitations whose written resolution condition was actually met. Keep angle, spin, equipment conversion, unresolved markers, initial spin, and above-domain limitations as applicable.
- [ ] Run `scripts/check.sh`, both package reconstructions, encoding, and freeze verification without invoking candidate HOLDOUT again.
- [ ] Document calibration/validation counts, valid domain, applicability, immutable paths, and replay governance; commit `test: preserve cushion collision validation v1`.

## Theme 4 Acceptance

- Production and scenarios use one straight-rail contact model with versioned restitution, friction, nose height, material, and speed domain.
- Energy, friction cone, separation, mirror, spin direction, swept-hit, no-repeat, and direct/runtime gates pass.
- Cushion telemetry is authoritative and independently cross-checked.
- Mathavan 2010 fitting is demonstrably CALIBRATION-only; Mathavan 2009 HOLDOUT never enters optimization.
- Source snooker geometry executes without silently becoming Chinese Pool evidence, and `>250 cm/s` remains outside the rigid-model domain.
- Both calibration reports and the fit are frozen before either candidate HOLDOUT event.
- First validation reports/receipts are immutable and honestly accounted; angle/spin/equipment evidence gaps remain explicit.
