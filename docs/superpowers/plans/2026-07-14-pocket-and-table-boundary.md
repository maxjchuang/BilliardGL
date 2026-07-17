# Pocket and Table Boundary Implementation Plan

**Execution status: Completed and archived.** Unchecked boxes below preserve the original execution script; current disposition and evidence are indexed in [Phase 3 current status](../../phase3-physics-current-status.md).

> **For agentic workers:** REQUIRED SUB-SKILL: use `superpowers:executing-plans` and `superpowers:test-driven-development`. Execute inline, in order, with one commit per task.

**Goal:** Replace rectangular pocket-mouth exemptions and fixed-depth pocket tests with one authoritative, ball-radius-offset pocket boundary that resolves straight rails and curved jaws continuously, classifies throat crossing/capture/rejection, and emits exactly one auditable pocket event.

**Architecture:** `PocketBoundaryModel` builds six immutable local pocket frames from a versioned `TableBoundaryProfile`. Each frame contains two jaw arcs, a throat segment, a capture plane, and straight-rail termini. Standalone geometry queries classify a ball center and return swept jaw/throat events; production chooses the earliest pocket-boundary or Theme 4 straight-rail event, reuses `resolveCushionContact` for jaw impulses, and advances the remaining Theme 1 motion. A per-ball interaction state records `Outside`, `Approaching`, `JawContact`, `ThroatCrossed`, `Captured`, or `Rejected`; rules consume only the one-shot `Captured` event. Evidence is an explicit grade-C analytic geometry contract, not an experimental trajectory claim.

**Tech Stack:** C++11 deterministic geometry/physics and CTest, automation JSON telemetry, Python 3 analytic-contract adapter/report/freeze pipeline, canonical UTF-8 JSON/CSV/Markdown artifacts.

## Global Constraints

- Preserve every Theme 1–4 candidate freeze and first validation artifact byte-for-byte. Never rerun their candidate HOLDOUT.
- Keep scenario v1–v6 and profile-manifest v1–v4 readable. Additive schema versions only.
- Use the physical playfield and versioned pocket parameters as collision truth; render meshes never define contacts.
- Offset every solid boundary and traversable throat by the active ball radius. A channel narrower than one diameter is impassable.
- Curved jaws call the Theme 4 cushion constitutive law with their local inward normal. Do not duplicate restitution/friction/spin formulas.
- Velocity impulse is approach-only; position correction cannot capture a ball or add energy.
- `Captured` means the center crossed the irreversible capture plane inside the valid throat corridor. Mouth entry or throat crossing alone is not a score.
- Captured balls leave 2.5D physics exactly once. Visual falling, return tracks, fouls, turns, and win/loss rules are out of scope.
- Resolve one earliest boundary event per segment with stable keys. Simultaneous dual-jaw/contact-island solving belongs to Theme 6 and must be reported, not order-hidden.
- The analytic contract uses committed geometry and generated exact expectations. It is grade C and cannot support a real-world trajectory-accuracy claim.
- Commit full scan matrices and traces. Each task ends in one commit. Do not push unless explicitly requested.

---

### Task 1: Version Authoritative Pocket Geometry

**Files:**
- Modify: `src/Billiards/physics_profile.h`
- Modify: `src/Billiards/physics_profile.cpp`
- Modify: `src/Billiards/physics_scenario.cpp`
- Modify: `tools/physics_validation/model_candidate.py`
- Modify: `tests/physics_profile_tests.cpp`
- Modify: `tests/physics_scenario_tests.cpp`
- Modify: `tests/physics_validation/test_model_candidate.py`

**Interfaces:** Add `TableBoundaryProperties` with playfield dimensions, distinct corner/side mouth and throat widths, jaw radius, throat depth, capture depth, and geometry/material ID. Produce scenario v7 and profile-manifest v5; v1–v6 reconstruct the historical opening-band defaults.

- [ ] RED-test finite safe geometry, ordering constraints, ball-diameter passability, complete numeric provenance, and v1–v6 byte compatibility.
- [ ] Keep existing production behavior until the standalone model is tested.
- [ ] Run focused schema/profile tests and commit `feat: version pocket boundary geometry`.

### Task 2: Build the Standalone Pocket Boundary Model

**Files:**
- Create: `src/Billiards/pocket_boundary.h`
- Create: `src/Billiards/pocket_boundary.cpp`
- Create: `tests/pocket_boundary_tests.cpp`
- Modify: `CMakeLists.txt`

**Interfaces:** Build local frames and expose point classification plus swept event queries for `StraightRail`, `Jaw`, `Throat`, `Capture`, `Ambiguous`, and `None`.

- [ ] RED-test four corner/two side frames, continuity at rail/jaw joins, ball-radius offsets, closed narrow throats, mirror equivalence, grazing roots, high-speed sweep, and finite outputs.
- [ ] Use analytic segment/circle/plane intersections with stable pocket/event keys and bounded tolerances; do not sample collision truth on a grid.
- [ ] Prove an outside point cannot jump directly to capture without a swept throat/capture event.
- [ ] Commit `feat: model authoritative pocket boundaries`.

### Task 3: Add the Pocket Interaction State Machine

**Files:**
- Modify: `src/Billiards/game_state.h`
- Modify: `src/Billiards/game_state.cpp`
- Modify: `src/Billiards/pocket_boundary.h`
- Modify: `src/Billiards/pocket_boundary.cpp`
- Create: `tests/pocket_state_machine_tests.cpp`
- Modify: `CMakeLists.txt`

**Interfaces:** Per-ball states are `Outside`, `Approaching`, `JawContact`, `ThroatCrossed`, `Captured`, `Rejected`, with pocket identity and one-shot capture sequence.

- [ ] RED-test legal transitions, rejection after jaw/throat exit, capture irreversibility, no cross-pocket teleport, reset semantics, and exactly-once capture emission.
- [ ] Make classification deterministic from authoritative state plus swept event; rendering state is not an input.
- [ ] Commit `feat: track pocket interaction states`.

### Task 4: Integrate Swept Jaws, Throats, and Capture in Production

**Files:**
- Modify: `src/Billiards/physics.cpp`
- Modify: `src/Billiards/physics.h`
- Modify: `src/Billiards/rules.cpp`
- Modify: `tests/physics_tests.cpp`
- Modify: `tests/game_runtime_tests.cpp`
- Modify: `tests/automation_physics_scenarios_tests.cpp`

- [ ] RED-test center capture, off-center rejection, jaw spin/rebound, high-speed no-tunnel, no invisible straight rail, no duplicate pocket event, capture removal, and direct/runtime equivalence.
- [ ] Compete the earliest straight-rail, jaw, throat, and capture times; advance surface motion to the event, apply the shared cushion model for jaws, update state, then advance the remainder.
- [ ] Remove `isInPocketMouth`/fixed drop-zone truth after equivalence tests; rules consume only `Captured`.
- [ ] Commit `feat: use swept pocket boundaries in production`.

### Task 5: Expose Pocket Telemetry and Deterministic Scan Matrices

**Files:**
- Modify: `src/Billiards/physics_telemetry.h`
- Modify: `src/Billiards/automation_protocol.cpp`
- Modify: `tools/physics_validation/analyzer.py`
- Create: `tools/physics_validation/pocket_scan.py`
- Modify: `tests/physics_telemetry_tests.cpp`
- Modify: `tests/automation_protocol_tests.cpp`
- Create: `tests/physics_validation/test_pocket_scan.py`

- [ ] Serialize pocket ID/kind/state transition, boundary event kind, local coordinates, jaw center/radius/normal, TOI, throat/capture signed distances, passability, impulse, and one-shot event sequence with units.
- [ ] Scan corner/side pockets over offset, angle, speed, top/back/side spin, and jaw side; commit every full-precision row.
- [ ] Cross-check no solid-boundary crossing, legal phase order, mirror pairs, duplicate capture, nonfinite values, and classification continuity.
- [ ] Commit `feat: expose and scan pocket boundary physics`.

### Task 6: Create and Execute the Grade-C Analytic Contract Calibration

**Files:**
- Create: `tests/physics_validation/reference_data/pocket_geometry_analytic_contract/*`
- Create: `tools/physics_validation/adapters/pocket_geometry_analytic.py`
- Modify: `tools/physics_validation/reference_adapter.py`
- Create: `tests/physics_validation/test_pocket_geometry_analytic_adapter.py`
- Modify: `docs/reference-data-packages.md`
- Create: `physics_models/candidates/pocket_boundary_v1/calibration/*`

- [ ] Partition exact geometry/mirror/passability/jaw/capture cases into CALIBRATION and HOLDOUT before candidate execution; expectations are analytic invariants, not synthetic experiments.
- [ ] Prove changing all HOLDOUT values cannot change profile bytes or CALIBRATION scenarios.
- [ ] Run CALIBRATION only and preserve full reports with zero HOLDOUT rows.
- [ ] Commit `test: calibrate analytic pocket boundaries`.

### Task 7: Freeze Pocket Boundary Candidate v1

**Files:**
- Create: `physics_models/profiles/chinese_pool_pocket_boundary_v1.json`
- Create: `physics_models/candidates/pocket_boundary_v1/freeze.json`
- Modify: `src/Billiards/physics_profile.cpp`
- Modify: production profile/runtime/protocol assertions

- [ ] Freeze exact geometry, analytic calibration report, contract manifest, scan matrix, profile, executable, and source revision; retain Theme 1–4 values unchanged.
- [ ] Switch production ID/formula only after clean full checks and freeze verification.
- [ ] State `WPA_POOL_GEOMETRY`/grade-C applicability and all missing trajectory/material evidence explicitly.
- [ ] Commit `feat: freeze pocket boundary candidate v1` before HOLDOUT.

### Task 8: Execute the Frozen Analytic HOLDOUT Once and Preserve It

**Files:**
- Create: `physics_models/candidates/pocket_boundary_v1/validation/*`
- Modify: `docs/reference-data-packages.md`

- [ ] Require a clean worktree and valid freeze; execute the analytic HOLDOUT exactly once into an empty immutable directory.
- [ ] Preserve all rows, traces, provenance, reports, and receipt even on failure; never tune geometry/tolerances or rewrite the split afterward.
- [ ] Run full checks, package reconstruction, encoding, and freeze verification without invoking candidate HOLDOUT again.
- [ ] Document counts, failures, evidence grade, valid geometry/domain, immutable hashes, and replay governance.
- [ ] Commit `test: preserve pocket boundary validation v1`.

## Theme 5 Acceptance

- One authoritative ball-radius-offset geometry drives straight rails, jaws, throats, capture, telemetry, scenarios, and full-game E2E.
- Corner and side pockets differ explicitly; narrow channels reject balls analytically.
- High-speed balls cannot cross solid jaws; jaw response reuses Theme 4; capture is irreversible and exactly once.
- Mirror, ball-index, tick-resolution, finite-state, legal-transition, and direct/runtime gates pass.
- Complete scan matrices and grade-C analytic calibration/validation artifacts are committed without experimental overclaim.
- Theme 1–4 freezes and first HOLDOUT reports remain unchanged and are never replayed.
