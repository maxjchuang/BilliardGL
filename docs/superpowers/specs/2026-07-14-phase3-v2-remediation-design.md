# Phase 3 Strict Successor v2 Remediation Design

**Status:** Approved design, pending written-spec review

**Date:** 2026-07-14

**Scope:** Phase 3 themes 1-7 remediation and final acceptance

**Supersedes:** The release conclusion in `phase3_release_v1`; it does not
rewrite or delete any v1 evidence.

## 1. Context

Phase 3 implemented the planned surface, cue, ball-ball, cushion, pocket,
multi-contact, and full-game work. A completion review found that the release
claim was stronger than the evidence:

1. `promotion.py` accepted validation receipts whose result was `FAILED` by
   renaming the outcome `limitations_preserved`.
2. The multi-contact solver accumulated only normal impulse. It did not solve
   tangential friction, angular response, or rotational energy, and it did not
   put simultaneous ball-boundary contacts in the same island.
3. Solver-limit failures returned partially mutated state or fell back to the
   discrete path instead of failing the complete step.
4. The full-game matrix named cases that the executable could not select or
   execute. The executable ran only repeated break variants.
5. The release manifest pointed at an older source revision and did not contain
   a production old/new comparison.

Read-only diagnosis also identified three concrete validation-integration
defects:

- The Mathavan 2009 sliding metric fitted five fixed ticks even though the ball
  transitioned from sliding to rolling after about 0.146 seconds. The trace's
  actual sliding acceleration was `196.13296508789062 cm/s^2`, within the
  source interval `[175, 240] cm/s^2`; the cross-phase fit produced the false
  prediction `29.274413626277244 cm/s^2`.
- The Mathavan 2010 adapter reserved a fixed 0.4-second approach. Its fastest
  cases therefore initialized the ball outside the table, causing first-frame
  penetration correction and multiple unintended rail rebounds.
- The Doménech 2023 PVC-bench scenarios waited up to 400 ticks for pure roll
  while retaining game-table rails. The balls rebounded from those rails and
  collided a second time, creating the reported duplicate impulse.

Candidate v1 reports, receipts, traces, and manifests are immutable historical
evidence. They remain committed and are explicitly rejected for production
promotion. No spent v1 HOLDOUT is executed again.

## 2. Goals and Non-goals

### 2.1 Goals

- Make every promotion and release gate fail closed.
- Correct validation apparatus and phase-selection errors without modifying
  the preserved v1 receipts.
- Build version-2 surface, ball-collision, cushion, and multi-contact
  candidates from calibration data that are already spent or explicitly
  designated for calibration.
- Validate the selected v2 candidate against newly admitted independent public
  experimental facts after the candidate is frozen.
- Solve simultaneous ball-ball and physical boundary contacts with normal and
  tangential impulses, rotation, and deterministic contact islands.
- Roll back a complete physics tick on any solver safety failure.
- Execute every advertised full-game case through a real `--case` interface.
- Commit every numeric input and output used for fitting, validation,
  comparison, or release, subject only to copyright restrictions on source
  publications themselves.
- Preserve one independently reviewable commit per implementation task.

### 2.2 Non-goals

- Jump, masse, elevated-cue, ball flight, cushion deformation meshes, cloth
  weave simulation, and acoustic response remain outside Phase 3.
- Phase 3 does not claim universal accuracy across every table, cloth, ball,
  cushion, humidity, or temperature combination.
- Copyrighted PDFs, source figures, and page images are not redistributed.
- Validation does not hide a failed point by widening a tolerance, changing a
  split, or adding an expected mismatch after candidate freeze.

## 3. Historical Evidence and Promotion Semantics

### 3.1 Immutable v1 disposition

`phase3_release_v1` becomes a rejected historical release. The repository must
retain its original bytes and record a separate rejection manifest that lists:

- the v1 release-manifest SHA-256;
- every `FAILED` receipt;
- every unallowlistable integration failure;
- every new model mismatch;
- the review finding that caused rejection;
- the rejecting source revision.

No v1 report, receipt, split, freeze, or trace may be regenerated.

### 3.2 Fail-closed promotion rules

A candidate is promotable only when all of the following are true:

- every required receipt exists and is hash-bound;
- every receipt result is exactly `PASSED_OR_ACCOUNTED`;
- `unallowlistable_failures`, `new_model_mismatches`,
  `new_limitations`, and `missing_expected_failures` are empty;
- every accounted mismatch or limitation was preregistered before freeze;
- the candidate profile, source revision, executable, calibration reports,
  source packages, and supplemental numeric inputs match the freeze;
- the validation artifact inventory contains every report, CSV, receipt,
  trace, provenance file, and declared supplemental file;
- the release source revision is an ancestor of `HEAD` and contains the exact
  production default profile selected by the built executable.

`FAILED` is never translated to a successful disposition. A release cannot use
`PASSED_WITH_DECLARED_LIMITATIONS` merely because a failure is documented.
Declared limitations are allowed only when the corresponding receipt passed
the preregistered accounting contract.

## 4. Experimental Evidence Strategy

### 4.1 Lifecycle

The lifecycle values retain their existing meanings:

- `calibration`: may influence formula and parameter selection;
- `spent`: previously revealed data that may be used to diagnose or fit a
  successor but can never be called fresh validation again;
- `validation`: a preregistered partition executed exactly once after freeze;
- `confirmation`: independent facts reserved to confirm an already selected
  candidate.

Public data isolation is procedural rather than secret. The split, metrics,
uncertainties, candidate-selection objective, and acceptance rules are
committed before fitting. Confirmation facts are not inputs to the fitter and
cannot select between candidate formulas.

### 4.2 Existing spent evidence

The following v1 experimental HOLDOUT results are marked `spent` in a separate
reviewed lifecycle commit and may be used for v2 diagnosis or calibration
without rerunning them:

- Mathavan 2009 high-speed surface and ball/cushion observations;
- Doménech 2023 ball-collision observations;
- Mathavan 2010 cushion observations.

The fitter consumes the committed normalized values, not a newly generated v1
report. Old integration failures are repaired by new v2 scenarios and produce
new calibration reports with new scenario IDs.

### 4.3 New independent confirmation evidence

Two independent sources are admitted:

1. M. Sudo, N. Ninomiya, M. Akiyama, and N. Yanaoka, "Collision
   Analysis of Quasi-Rigid Balls by High-Speed Camera," 2002,
   DOI `10.3154/jvs.22.1Supplement_153`.
2. N. Derby and R. Fuller, "Reality and Theory in a Collision," 1999,
   *The Physics Teacher* 37, 24-27, repository record
   `https://digitalcommons.unl.edu/physicsfuller/3/`.

The Sudo experiment used real Brunswick phenolic-resin balls with mean mass
`168.62 g` (standard deviation `0.47 g`) and mean diameter `57.172 mm`
(standard deviation `0.021 mm`) on a real slate-and-wool billiard table. The
source-reported scalar observations admitted for confirmation are:

- straight cushion restitution `0.856` below `1.8 m/s`;
- all-speed straight cushion regression restitution `0.745`;
- high-speed cushion contact-time plateau approximately `8 ms`;
- head-on ball-ball restitution `0.897`;
- mean oblique post-collision separation angle `85.05 degrees`;
- transverse momentum deficit approximately `4 percent` under the source's
  sign convention.

The Derby-Fuller experiment used `170.3 g`, `5.24 cm` balls and a nominal
`3000 fps` recording. Its admitted scalar observations are:

- initial cue-ball velocity `0.98 m/s`;
- cue-ball sliding acceleration `+3.13 m/s^2` with about `10 percent`
  standard error;
- target-ball sliding acceleration `-1.83 m/s^2` with about `5 percent`
  standard error;
- cue-ball and target-ball sliding times `90 ms` and `150 ms`;
- final speeds `0.251 m/s` and `0.583 m/s`;
- post-collision linear momentum `0.151 kg*m/s` versus `0.167 kg*m/s`
  before impact;
- total kinetic-energy loss `17 percent`.

### 4.4 Copyright and complete-data policy

The repository commits all numeric data actually used by extraction, fitting,
validation, and release. It does not commit the Sudo or Derby PDF, page image,
or graph image. Each evidence package commits:

- bibliographic metadata and public URL;
- acquisition date and SHA-256 of the locally audited source file;
- exact page, figure, paragraph, and unit locator;
- every used source-reported scalar with its original precision;
- conversion equations and full-precision converted values;
- measurement, rounding, conversion, and engineering uncertainties;
- apparatus description and applicability classification;
- deterministic extraction/normalization code and output hashes;
- split and lifecycle records.

No numeric point may be used unless it is present in those committed files.
This satisfies the project's complete-data requirement without redistributing
copyrighted expression.

## 5. Validation Apparatus and Metric Selection

### 5.1 Boundary modes

Physics scenarios gain an explicit boundary mode:

- `production_table`: the configured table, rails, jaws, throats, and capture
  planes are active;
- `unbounded`: no rail, jaw, throat, or capture event is generated, while
  surface and ball-ball physics remain active.

The default is `production_table`. Only source packages whose apparatus is an
open bench may request `unbounded`. Scenario parsing rejects an unknown mode.
Telemetry records the selected mode so a trace cannot silently substitute one
apparatus for another.

### 5.2 Initial-geometry validation

Before a scenario runs, every active ball must be finite, non-overlapping
except for an explicitly declared contact epsilon, and inside the selected
apparatus domain. An invalid initial state is an `INTEGRATION_MISMATCH`; it is
never corrected by the first production tick.

The Mathavan 2010 v2 adapter computes the longest approach duration that keeps
the initial center inside the source table. It requires at least three
pre-impact samples. If the configured time step cannot satisfy both conditions,
the adapter reduces its scenario time step; it never moves the ball outside the
table.

### 5.3 Phase-aware selection

Surface metrics select samples by telemetry phase, not a fixed tick count:

- sliding deceleration uses the maximal contiguous `sliding` segment;
- rolling deceleration uses the maximal contiguous `rolling` segment;
- transition time is interpolated from the `SurfaceMotionStep` transition
  timestamp;
- fewer than three in-phase samples produces `INTEGRATION_MISMATCH`.

Collision metrics bind to one solver event ID and reject an unrelated later
collision. An open-bench scenario cannot contain a boundary contact. Immediate
post-impact and first-stable-roll measurements remain distinct.

## 6. Candidate v2 Physical Models

### 6.1 Surface candidate v2

The surface equations remain the rigid-sphere Coulomb sliding model plus
constant rolling resistance already implemented by `surface_motion_v1`.
Candidate v2 corrects the validation selection and refits only from spent
surface data. It does not introduce a new formula merely to match the old
cross-phase regression error.

The fitter reports sliding coefficient, rolling resistance, each source row's
normalized residual, covariance or bounded interval, and the exact objective.
Derby-Fuller sliding times and final-speed facts contribute to integrated
confirmation together with the collision metrics in Section 6.2. The two
reported acceleration values are preregistered diagnostic facts, not passing
surface-fit targets: the authors explicitly report that their unequal
magnitudes were unexpected, estimate only one experiment, and state that they
did not establish reproducibility. The comparison must still show both values
and both predictions and may not replace them with an average. A future
repeatable dataset is required before adding velocity-dependent cloth friction
solely to explain this difference.

### 6.2 Ball-collision candidate v2

The single-contact model remains a rigid impulse with:

- normal restitution `e`;
- Coulomb tangential friction `mu`;
- translational and angular contact velocity;
- uniform solid-sphere inertia;
- explicit stick/slip regime;
- non-increasing total translational plus rotational kinetic energy.

Candidate selection uses spent Mathavan 2009 direct post-collision velocities
and spent Doménech billiard observations. Residuals are normalized by their
declared uncertainties, then averaged per physical series before series are
averaged. This prevents the dense Doménech angular plot from overwhelming the
sparser direct velocity measurements. Stable tie-breaking is
`(objective, e, mu)`.

Sudo head-on restitution, Sudo separation angle/momentum deficit, and
Derby-Fuller final velocities/momentum/energy are confirmation-only. They are
not imported by the fitter.

### 6.3 Cushion candidate v2

The cushion model retains the measured contact arm, nose-height ratio,
tangential friction, and rotational coupling. Constant normal restitution is
replaced by the minimal speed-dependent law:

```text
e(v_n) = clamp(e_intercept - e_slope * v_n, e_min, e_max)
```

where `v_n` is incident normal speed in `m/s`. `e_slope` is constrained to be
non-negative and all four values are finite with
`0 <= e_min <= e_max <= 1`. Stable fitting uses spent Mathavan 2010 and
Mathavan 2009 cushion values, uncertainty-normalized residuals, and
`(objective, e_intercept, e_slope, e_min, e_max)` tie-breaking.

The profile retains an explicit domain warning above the source-supported
rigid-cushion range. Sudo low-speed and all-speed restitution regressions and
contact-time plateau are confirmation-only. Contact time is a reported
limitation unless the runtime gains a compliant finite-duration contact model;
an instantaneous impulse may not claim to predict `8 ms`.

### 6.4 Cue and pocket candidates

Cue-contact v1 and pocket-boundary v1 remain bounded components because their
analytic contracts passed. They are re-frozen into the integrated v2 release
to bind the new source revision and executable, but their experimental
limitations remain explicit. They cannot be described as experimentally
validated.

## 7. Joint Continuous Contact Solver v2

### 7.1 Event batch

For every remaining portion of a fixed tick, candidate generation includes:

- swept ball-ball contacts;
- straight rails;
- pocket jaws;
- throat crossings;
- capture crossings.

All candidates within `toi_tolerance_seconds` of the earliest event form one
deterministic event batch. Ball-ball, rail, and jaw contacts are impulse
constraints. Throat and capture candidates are ordered topological transitions
and do not synthesize an impulse.

Equal-TOI physical constraints are connected by ball identity and solved as
contact islands. A ball touching a ball and a rail at the same TOI belongs to
one island. Stable ordering uses event kind, ball indices, and feature ID only;
pointer order or container insertion order cannot affect a result.

### 7.2 Constraint state

Each physical contact stores:

- normal and deterministic tangent basis;
- contact arms in meters;
- target normal velocity;
- accumulated normal impulse;
- accumulated signed tangential impulse;
- normal and tangential effective mass;
- restitution and friction coefficient;
- stick/slip regime;
- residual and positional projection.

Each forward/reverse projected sequential-impulse sweep updates both linear
and angular velocity. Tangential impulse is clamped to the accumulated Coulomb
cone `|lambda_t| <= mu * lambda_n`, not clamped per incremental sweep.

Energy telemetry includes translational and rotational kinetic energy before
and after every island. No passive contact may create energy beyond a fixed
floating-point tolerance declared in the profile and tested at multiple time
steps.

### 7.3 Position and topology

Velocity impulses and positional projection remain separate. Projection uses
the current geometry, configured slop, inverse-mass sharing, and deterministic
iteration count. A throat/capture transition is applied only after all
same-TOI physical islands converge. Capture is irreversible and is processed
once per capture sequence.

### 7.4 Fail-closed tick transaction

`updatePhysics` snapshots the complete mutable physics state before a tick.
The following conditions fail the transaction:

- event budget exhausted;
- island-size limit exceeded;
- velocity residual above tolerance after the iteration limit;
- penetration above the hard limit;
- non-finite position, velocity, angular velocity, impulse, energy, or time;
- passive-contact energy creation beyond tolerance;
- ambiguous or contradictory same-TOI topology.

On failure, all ball, pocket-interaction, event, counter, and movement state is
restored. The tick returns telemetry with `step_status=failed`, one stable
failure code, failing event/island identifiers, and the uncommitted diagnostic
values. Runtime time and tick counters do not advance. There is no discrete
fallback and no partially mutated state.

## 8. Full-game Acceptance

### 8.1 Executable interface

`full-game-stress` supports:

```text
full-game-stress --list-cases
full-game-stress --case <id> --seed <uint32> [--write <directory>]
full-game-stress --matrix <path> --write <directory>
```

Unknown cases, missing seeds, duplicate case IDs, and unused arguments fail.
Each case writes canonical JSON summary plus full frame/contact/solver trace.
CSV is a derived index, not the only numeric artifact.

### 8.2 Required cases

The matrix must execute and preserve at least:

- `cue_center_hit`;
- `cue_near_miscue`;
- `sliding_to_rolling`;
- `oblique_ball_collision`;
- `rail_rebound`;
- `side_pocket_capture`;
- `seeded_break`;
- `continuous_scoring`;
- `cue_ball_scratch`;
- `randomized_legal_sequence`;
- `cadence_equivalence`;
- `host_load_equivalence`.

The first six bind component behavior to integrated runtime traces. A seeded
break uses a real rack and user-equivalent cue action. Continuous scoring
loads a deterministic legal layout, issues user-equivalent aim/power/shot
actions, waits for `shotEnded`, and proves at least three object-ball captures
with correct player-state updates. Scratch proves cue-ball capture, foul state,
and turn transfer. Randomized legal sequence uses a committed PRNG algorithm
and seed, emits only operationally legal shot actions, and runs until the
declared shot count or game over. "Operationally legal" means that all balls
are stationary, the game is not over, the cue ball is active, no shot is
pending, and the current player is allowed to aim and shoot. The shot outcome
may still be a gameplay foul; that foul and the resulting turn transfer are
part of the asserted event sequence.

Cadence and host-load cases replay the same action stream and compare every
physics state and gameplay event at common tick boundaries. They may differ in
wall-clock duration only.

### 8.3 Invariants and budgets

Every case checks:

- finite complete state;
- successful physics-step status;
- no duplicate impulse within one event/contact identity;
- penetration and residual limits;
- no passive energy creation beyond tolerance;
- deterministic hash across identical runs;
- expected scoring, foul, turn, pocket, and game-over transitions;
- no dropped trace frames;
- declared wall-clock and memory budgets.

Performance baselines and budgets are regenerated only for the frozen v2
executable. A slower result fails unless a separately reviewed budget change
precedes freeze.

## 9. Freeze, One-time Validation, and Release

### 9.1 Pre-freeze sequence

Before any new confirmation/HOLDOUT execution:

1. commit source packages, lifecycle records, splits, metrics, and acceptance
   intervals;
2. commit candidate formula and fitter;
3. commit all calibration reports and numeric fit artifacts;
4. select one integrated production profile;
5. build the release executable from a clean tree;
6. freeze source revision, executable, profile, calibration reports, package
   manifests, full-game matrix, performance budget, and supplemental inputs;
7. verify the freeze from a second clean build.

### 9.2 Validation execution

Each new validation or confirmation partition is executed exactly once for the
frozen candidate. The first result is immutable whether it passes or fails.
Every output directory is committed in full and included in a SHA-256
inventory. A failed result rejects v2 and does not trigger tolerance or formula
changes under the same candidate ID.

### 9.3 Old/new comparison

The release contains a machine-readable and Markdown comparison of v1 and v2:

- every common experimental point and status;
- series RMSE, maximum absolute normalized error, and pass counts;
- solver residual, penetration, energy, iteration, and event statistics;
- full-game gameplay outcomes and deterministic hashes;
- wall-clock and memory performance;
- profile parameter changes and provenance;
- limitations gained, resolved, or retained.

No failed v1 row is omitted. Corrected integration scenarios are paired with
the original failure and an explicit cause code.

### 9.4 Release status

The only successful v2 release status is `PASSED`. It requires:

- all candidate gates pass;
- all one-time validation/confirmation receipts pass;
- all full-game cases pass;
- performance gates pass;
- the complete-artifact inventory passes;
- the source revision and executable checks pass;
- the production default profile is the frozen v2 profile;
- the old/new comparison is complete.

Bounded limitations remain in the profile and report, but cannot change a
failed gate into success.

## 10. Implementation Decomposition

Implementation is split into five sequential plans, each committed separately:

1. **Promotion governance and v1 rejection** - fail-closed manifest schemas,
   historical rejection, artifact/source verification.
2. **Experimental apparatus and v2 data** - lifecycle updates, independent
   source packages, boundary modes, initial-state validation, phase-aware
   metrics, and corrected calibration scenarios.
3. **Joint contact solver v2** - complete candidate generation, tangential and
   angular island solve, energy accounting, and transactional failure.
4. **Executable full-game acceptance** - real case dispatcher, gameplay cases,
   traces, invariants, and performance budgets.
5. **Candidate fit, freeze, validation, and release** - v2 fits, integrated
   profile, clean freeze, one-time confirmation, old/new comparison, complete
   inventory, and final release gate.

Within each plan, every task starts with a failing test, makes the smallest
coherent implementation pass, runs its focused regression set, and ends in one
commit. HOLDOUT/confirmation execution is never combined with formula or
parameter changes in the same commit.

## 11. Acceptance Checklist

Phase 3 is complete only when repository evidence proves all of the following:

- [ ] v1 is preserved and explicitly rejected.
- [ ] `FAILED` receipts cannot pass any promotion or release validator.
- [ ] all used experimental numbers and transformations are committed.
- [ ] no copyrighted source publication or figure is redistributed.
- [ ] spent v1 HOLDOUTs were not rerun.
- [ ] corrected v2 calibration scenarios pass apparatus and phase-selection
      integrity checks.
- [ ] the joint solver includes boundary contacts, tangential impulse,
      rotation, and rotational energy.
- [ ] every solver safety failure rolls back the whole tick and is observable.
- [ ] every matrix case is selectable and actually executed.
- [ ] scoring, scratch/foul, legal randomized play, cadence, and load are
      covered by committed full traces.
- [ ] the frozen v2 candidate executes new confirmation exactly once.
- [ ] all validation receipts, full-game gates, performance gates, artifact
      inventories, source checks, and comparisons pass.
- [ ] the production default is the frozen v2 profile.
- [ ] a final independent code review finds no blocking issue.
