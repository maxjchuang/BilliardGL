# Full-Game Physics Acceptance Implementation Plan

Date: 2026-07-14

## Goal

Promote the already frozen and validated Phase 3 models through one final,
replayable full-game gate without inventing a second physics path or upgrading
the evidence grade of analytic/behavior-only scenarios.

### Task 1: Inventory Frozen Models and Promotion Preconditions

**Files:** promotion manifest/schema, validator, tests.

- [x] Record every Phase 3 candidate, profile, freeze, validation receipt, evidence grade, and immutable hash.
- [x] Reject missing, mutable, unvalidated, or overstated candidates.
- [x] Commit `test: inventory phase 3 promotion inputs`.

### Task 2: Define the Full-Game Acceptance Matrix

**Files:** versioned matrix, loader, tests, documentation.

- [x] Cover cue offsets, surface transitions, ball/rail/pocket contact, break, scoring sequence, foul, frame cadence, host load, and fixed seeds.
- [x] Classify each case as reality golden, analytic golden, or behavior snapshot.
- [x] Commit `test: define full-game physics acceptance matrix`.

### Task 3: Prove Direct and User-Operation Paths Share One Core

**Files:** runtime/automation E2E harness and tests.

- [x] Construct equivalent first authoritative states via exact scenario and user-operation control.
- [x] Compare subsequent canonical traces and failure classification.
- [x] Commit `test: prove full-game physics path equivalence`.

### Task 4: Add Deterministic Full-Game Stress Runs

**Files:** seeded stress runner, complete committed matrix/results, tests.

- [x] Run repeated breaks and randomized legal shots across fixed seeds, cadence, and load schedules.
- [x] Gate finite state, bounded penetration/residual, event uniqueness, determinism, and replay hashes.
- [x] Commit `test: stress deterministic full-game physics`.

### Task 5: Preregister and Enforce Performance Budgets

**Files:** benchmark runner, budget manifest, baseline artifact, tests.

- [x] Measure fixed-tick throughput, p95/p99 step time, memory/artifact volume under a reproducible headless workload.
- [x] Fail closed on budget regression while keeping timing evidence separate from physical truth claims.
- [x] Commit `perf: gate full-game physics budgets`.

### Task 6: Preserve Golden Trajectories with Evidence Labels

**Files:** golden registry/artifacts, verifier, tests.

- [x] Admit only validated analytic/experimental cases as reality/analytic goldens.
- [x] Mark unvalidated full-game trajectories as behavior snapshots and prohibit evidence promotion by snapshot update.
- [x] Commit `test: preserve evidence-labeled physics goldens`.

### Task 7: Generate the Final Promotion Report

**Files:** report generator, committed JSON/Markdown report, reconstruction tests.

- [x] Summarize model parameters, calibration/validation state, hard gates, old/new differences, performance, determinism, limitations, and replay commands.
- [x] Reconstruct the report byte-for-byte from committed inputs.
- [x] Commit `docs: generate phase 3 physics promotion report`.

### Task 8: Install the Release Gate and Close Phase 3

**Files:** CI/release gate, final acceptance tests and documentation.

- [x] Run fast PR checks and expose complete/long-run commands without silently invoking spent HOLDOUT partitions.
- [x] Require clean immutable inputs, zero unexplained regression, passing E2E/stress/performance, and explicit limitations.
- [x] Commit `test: complete phase 3 physics acceptance`.

## Final Verification

- Build and run all C++/E2E tests.
- Run all Python validation and reconstruction tests.
- Verify UTF-8, candidate freezes, promotion hashes, and a clean worktree.
- Do not rerun any spent HOLDOUT partition from Themes 1–6.
