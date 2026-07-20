# Multi-Contact Solver Implementation Plan

**Execution status: Completed and archived.** Unchecked boxes below preserve the original execution script; current disposition and evidence are indexed in [Phase 3 current status](../../phase3-physics-current-status.md).

> **For agentic workers:** REQUIRED SUB-SKILL: use `superpowers:executing-plans`, `superpowers:test-driven-development`, and `superpowers:systematic-debugging`. Execute inline, in order, with one commit per task.

**Goal:** Replace per-ball, traversal-ordered collision handling with a fixed-tick, event-driven solver that groups simultaneous contacts into deterministic islands, prevents high-speed tunneling, and reports hard numerical failures instead of silently dropping work.

**Architecture:** A standalone continuous-collision layer produces canonical ball-ball and boundary candidates without mutating state. Equal-TOI candidates form a graph whose connected components are stable contact islands. A bounded projected sequential impulse solver operates on a snapshot, alternates canonical forward/reverse sweeps to suppress order bias, separates velocity impulses from positional projection, and reuses Theme 2–5 contact parameters. `updatePhysics` advances surface motion to the earliest event, solves all islands, and repeats over the remaining fixed tick. Complete grade-C analytic and stress artifacts govern the candidate; they do not claim chaotic break-shot trajectory accuracy.

**Tech Stack:** C++11 deterministic CCD/island solver and CTest, automation JSON telemetry, Python 3 stress-matrix/analytic-contract/freeze pipeline, canonical UTF-8 JSON/CSV/Markdown artifacts.

## Global Constraints

- Preserve Theme 1–5 freezes and first HOLDOUT artifacts byte-for-byte; never rerun their candidate HOLDOUT.
- Keep scenario v1–v7 and profile-manifest v1–v5 readable. Additive schema versions only.
- Fixed physics tick is authoritative; render cadence and host load cannot alter step size.
- Contact generation is read-only. Stable keys are `(toi, kind, min_ball, max_ball, feature_id)`.
- Resolve all candidates within the committed TOI tolerance as one simultaneous event; connected contacts form islands.
- Reuse Theme 2–5 material/contact laws. The solver owns orchestration, accumulation, and projection, not duplicate restitution/friction formulas.
- Velocity impulse is approach-only. Positional projection cannot add kinetic energy.
- Every loop, island, iteration, event, penetration, residual, finite-state, and energy limit is explicit. Exceeding one returns a traceable numerical failure.
- Commit full stress matrices and traces. Each task ends in one commit. Do not push unless explicitly requested.

---

### Task 1: Version Deterministic Solver Controls

**Files:** `physics_profile.h/.cpp`, `physics_scenario.cpp`, `model_candidate.py`, profile/scenario/model-candidate tests.

- [ ] Add TOI tolerance, maximum island size, velocity iterations, position iterations, penetration slop, maximum penetration, and residual tolerance.
- [ ] Produce scenario v8/profile-manifest v6; reconstruct historical defaults for v1–v7.
- [ ] RED-test finite ranges, ordering, complete numeric provenance, and old-schema compatibility.
- [ ] Commit `feat: version multi-contact solver controls`.

### Task 2: Build Standalone Continuous Contact Candidates

**Files:** create `continuous_collision.h/.cpp`, tests, CMake.

- [ ] Analytic swept-sphere ball-ball TOI plus canonical boundary candidate wrapper.
- [ ] RED-test high-speed crossing, initial overlap, grazing discriminant, separating pairs, mirror/index permutation, and finite output.
- [ ] Candidate generation must not mutate input state.
- [ ] Commit `feat: generate continuous contact candidates`.

### Task 3: Build Canonical Contact Islands

**Files:** create `contact_island.h/.cpp`, tests, CMake.

- [ ] Group equal-TOI candidates by graph connectivity with canonical keys.
- [ ] RED-test disjoint components, chains, simultaneous symmetric contacts, duplicate suppression, shuffled input, ball-index permutation, and limit failures.
- [ ] Commit `feat: build deterministic contact islands`.

### Task 4: Implement Bounded Island Impulse and Projection Solving

**Files:** create `contact_solver.h/.cpp`, tests, CMake.

- [ ] Snapshot an island, accumulate impulses, alternate canonical sweep direction, and project penetration separately.
- [ ] RED-test three-ball transfer, Newton-cradle symmetry, two-sided simultaneous impact, resting contact, energy nonincrease, no duplicate impulse, convergence residual, and hard-limit status.
- [ ] Commit `feat: solve simultaneous contact islands`.

### Task 5: Integrate the Event-Driven Fixed-Tick Stepper

**Files:** `physics.cpp/.h`, `game_runtime.cpp`, direct/runtime/automation tests.

- [ ] Repeat earliest-event advance → island solve → remaining-time advance within one fixed tick.
- [ ] Include ball-ball, straight rail, jaw, throat, and capture candidates; preserve pocket event ordering.
- [ ] RED-test high-speed no-tunnel, multiple events per tick, break rack, jaw+ball simultaneity, tick subdivision equivalence, render/load independence, and explicit limit failure.
- [ ] Remove traversal-ordered per-ball collision mutation after equivalence gates.
- [ ] Commit `feat: run event-driven multi-contact physics`.

### Task 6: Expose Solver Telemetry and Commit Stress Matrices

**Files:** telemetry/protocol/analyzer, create `solver_stress.py`, tests, committed full matrix.

- [ ] Serialize event/island IDs, canonical keys, candidate counts, iteration counts, accumulated impulses, residuals, projection, limits, and failure code.
- [ ] Scan line chains, symmetric impacts, racks, rail/jaw mixtures, speeds, spins, tick subdivisions, input permutations, and fixed seeds.
- [ ] Cross-check finite state, nonincreasing impact energy, maximum penetration, duplicate impulses, symmetry, and deterministic hashes.
- [ ] Commit `feat: expose and stress multi-contact solver`.

### Task 7: Calibrate and Freeze Solver Candidate v1

**Files:** grade-C analytic contract package/adapter, calibration artifacts, profile/freeze, production assertions, reference documentation.

- [ ] Prepartition CALIBRATION/HOLDOUT for exact TOI, symmetry, momentum, passivity, and limit cases.
- [ ] Prove HOLDOUT mutation cannot alter profile bytes or CALIBRATION scenarios.
- [ ] Execute CALIBRATION only, run full checks, freeze profile/package/report/stress/executable/source revision, then switch production ID.
- [ ] Declare missing chaotic trajectory and real break-shot evidence explicitly.
- [ ] Commit `feat: freeze multi-contact solver candidate v1` before HOLDOUT.

### Task 8: Execute Frozen Solver HOLDOUT Once

**Files:** immutable candidate validation directory and reference documentation.

- [ ] Require a clean worktree, valid freeze, empty output, and lifecycle `validation` state.
- [ ] Execute HOLDOUT exactly once; preserve every row, trace, provenance, report, and receipt even on failure.
- [ ] Verify freeze, reconstruction, encoding, and full checks without invoking candidate HOLDOUT again.
- [ ] Document counts, failures, grade-C scope, hashes, and replay governance.
- [ ] Commit `test: preserve multi-contact solver validation v1`.

## Theme 6 Acceptance

- High-speed ball/rail/jaw events do not tunnel; simultaneous contacts are island-solved rather than traversal-ordered.
- Ball-index, candidate-order, render-cadence, host-load, tick-subdivision, mirror, and repeat-seed gates pass within committed deterministic tolerances.
- Dense rack and break scenarios stay finite, bounded, passive at impacts, and below penetration/residual limits, or return an explicit replayable numerical failure.
- Complete stress matrices plus grade-C analytic calibration/validation artifacts are committed without chaotic real-world trajectory overclaim.
- Theme 1–5 freezes and first HOLDOUT reports remain unchanged and are never replayed.
