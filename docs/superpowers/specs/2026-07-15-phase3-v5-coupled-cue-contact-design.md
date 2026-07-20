# Phase 3 v5 Coupled Cue Contact Design

**Date:** 2026-07-15

**Status:** Approved design; implementation not started

## Context

The frozen-shot Alciatore TP A.15 confirmation rejected
`phase3_integrated_v4`. Execution and artifact integrity passed, but only two
of nine points met their pointwise expectations. The seven interior angular
points produced an RMSE of 38.753578 degrees and a maximum absolute error of
69.902117 degrees, far beyond the pre-registered limits of 3 and 5 degrees.
Han 2005 therefore remained unopened and unexecuted.

The failure is physical rather than an automation or schema failure. The
current cue-contact path applies one instantaneous impulse before the regular
ball-contact solver runs. When the cue ball initially touches one or more
balls, the cue, cue ball, and frozen neighbours should exchange force during
the same finite contact interval. The one-shot architecture cannot represent
that coupled interaction.

The successor candidate is `phase3_integrated_v5`. It introduces a bounded,
finite-duration cue-contact island only for frozen-contact shots. Ordinary
shots remain on the v4 path with strict physical equivalence. Alciatore is now
spent calibration and regression evidence for v5. Han remains unopened, and
a new independent public frozen-contact experiment must be admitted before
v5 can be promoted as physically confirmed.

## Goals

- Represent simultaneous cue--cue-ball--object-ball force transfer during a
  frozen shot.
- Support a cue ball initially touching multiple balls, subject to the
  existing maximum contact-island size.
- Preserve the complete v4 physical result for every non-frozen shot.
- Make the finite contact interval observable at every local microstep.
- Fail atomically and diagnostically when the local solve is unsafe or does
  not converge.
- Fit only identifiable quantities and preserve complete residual and
  sensitivity evidence.
- Require two independent real-world confirmations before promotion.
- Commit the complete numerical data needed for later verification and
  tuning, subject only to explicit third-party redistribution restrictions.
- Preserve a transport-neutral automation boundary so telemetry and control
  can later be exposed through sockets or other interfaces.

## Non-goals

- Replacing ordinary v4 cue impacts with a compliant solver.
- Making all ball, cushion, or table contacts compliant.
- Adding a persistent rigid cue body to the game world or renderer.
- Simulating cue flex, tip deformation fields, acoustic vibration, or player
  biomechanics.
- Supporting elevated-cue jump shots, masse shots, or vertical cue impulse.
- Using Han before the separately authorized confirmation transaction.
- Relaxing Alciatore gates because v4 failed them.
- Claiming real-world confirmation when no suitable replacement public
  experiment has been acquired and passed.

## Scope and routing

`applyCueImpact` performs a static frozen-topology scan before regular
time-of-impact processing:

```text
cue command
  -> validate cue-ball state and detect touching topology
       -> no frozen contact: execute the unchanged v4 resolveCueContact path
       -> frozen contact: build and execute CoupledCueContactIsland
```

A frozen topology exists when the cue ball is already touching a ball or
cushion at cue-tip contact within the versioned geometric tolerance. The
island contains the transient cue state, cue ball, all connected touching
balls, and their active ball--ball and ball--cushion constraints. It is
bounded by `maximum_island_size`. A captured cue ball, contradictory initial
geometry, or oversized island is rejected before any game state is changed.

The frozen path runs fixed local microsteps inside one normal physics tick.
Each microstep computes cue-tip forces, advances the transient cue and cue
ball, solves the existing rigid ball and cushion constraints, then evaluates
release, convergence, penetration, and energy conditions. Only the final
successful state is committed. No partial shot event reaches the rules layer.

## Transient cue state

The cue exists only inside the frozen-contact transaction. Its state includes:

- effective mass;
- axial position, velocity, and acceleration;
- tip compression and compression rate;
- cue-ball normal and tangential forces;
- accumulated normal and tangential impulse;
- contact start, peak-compression, and release times;
- tangential displacement history for stick/slip response;
- kinetic energy and cumulative work delivered to the island.

It is neither a long-lived rigid body nor a rendered object. The transaction
destroys it after commit or rollback.

## Contact model

### Normal response

The cue-tip normal force uses a Hunt--Crossley compliant contact:

```text
F_n = max(0, k_n * delta^1.5 * (1 + alpha * delta_dot))
```

`delta` is non-negative tip compression and `delta_dot` is its rate. The
Hertzian exponent is fixed at 1.5 and is not a fitted parameter. `k_n` and
`alpha` are versioned, unit-bearing material parameters. The implementation
must prevent the dissipative factor from producing an attractive normal
force.

### Tangential response

Tangential force uses a history-dependent spring--damper trial response:

```text
F_t_trial = -k_t * xi_t - c_t * v_t
|F_t| <= mu_tip * F_n
```

`xi_t` is accumulated tangential displacement in the current contact frame.
When the trial force exceeds the friction cone, the contact enters slip and
the history is projected back to the cone. The signed tip offset and
tangential force generate cue-ball angular impulse. Stick, slip, transition,
and release are explicit telemetry regimes.

### Coupled rigid constraints

Ball--ball and ball--cushion contacts retain the current rigid constraint
formulation. They are iterated in the same microstep loop as the compliant
cue contact so impulse can propagate through the whole frozen island while
the cue remains loaded. This hybrid design confines new compliance to the
missing physical interaction and avoids changing unrelated gameplay.

## Parameters and identifiability

Every new parameter is named, versioned, and stored with an SI unit:

- cue-tip normal stiffness and dissipation;
- tangential stiffness and damping;
- tip friction coefficient;
- local microstep duration;
- maximum contact duration;
- release compression/force threshold;
- maximum compression and maximum normal force;
- passive-energy error tolerance;
- penetration, residual, and iteration limits.

The Alciatore angle curve cannot identify this entire parameter set. It may
calibrate the observable frozen-shot trajectory response, but it must not be
used to freely fit every compliance parameter. A public contact-duration or
force--time experiment must provide calibration data or defensible bounds for
the normal compliance. Parameters that remain non-identifiable are fixed or
narrowly bounded from sourced material evidence, and their sensitivity is
reported. Parameter selection, bounds, objective functions, and data
partitions are registered before fitting.

## Numerical execution and failure semantics

Each local solve must end in either an atomic commit or atomic rollback.
Success requires all of the following:

- the cue tip releases without immediate recontact;
- every scalar and vector remains finite;
- no passive subsystem creates energy beyond its registered tolerance;
- compression, force, penetration, residual, iteration, and duration limits
  are respected;
- repeated executions produce an identical normalized microtrace.

Stable error categories include:

- `nonfinite_state`;
- `passive_energy_gain`;
- `compression_limit`;
- `contact_island_limit`;
- `cue_contact_no_release`;
- `cue_contact_nonconvergence`.

On failure, all ball, rule, event, and cue state is restored to its pre-shot
value. Failure telemetry is retained. Frozen shots do not silently fall back
to the v4 instantaneous impulse because that would conceal the rejected
physical model.

## Telemetry contract

The complete local microtrace records, for every microstep:

- cue position, velocity, acceleration, kinetic energy, and cumulative work;
- tip compression, compression rate, normal force, and tangential force;
- stick/slip regime, tangential history, and cumulative impulse;
- position, linear velocity, angular velocity, and derived acceleration for
  every island ball;
- normal, gap/penetration, relative velocity, impulse, and regime for every
  ball and cushion contact;
- island linear momentum, angular momentum, translational energy, rotational
  energy, elastic energy, dissipated energy, and energy residual;
- solver residual, iteration count, and convergence flags;
- contact start, peak compression, regime transition, release, commit, and
  rollback events.

The telemetry schema is versioned and transport-neutral. Existing E2E file
transport may consume it immediately; a future socket or other transport can
expose the same load, act, step, observe, reset, and trace semantics without
changing the physics model.

## Verification design

### Mathematical and component tests

- analytic compliant-contact limit cases;
- zero-damping energy conservation within numerical tolerance;
- positive-damping passivity;
- stick/slip friction-cone enforcement and history projection;
- centered-hit spin symmetry and signed-offset spin direction;
- rotation, mirror, ball-order, and contact-order invariance;
- convergence under microstep halving;
- deterministic normalized microtrace generation.

### Coupled and failure tests

- cue ball frozen to one object ball;
- cue ball frozen to multiple connected balls;
- simultaneous ball and cushion contact;
- island-size rejection;
- non-release, excessive compression, non-finite, energy, and convergence
  failures;
- complete rollback of physics, events, and rules state;
- retention of byte-identical diagnostic traces after deterministic failure.

### Ordinary-shot compatibility

All non-frozen scenarios execute the v4 solver. Excluding only the candidate
identifier, formula-version metadata, and new empty/optional diagnostics,
their physical states, event ordering, rule outcomes, and determinism hashes
must equal v4. Any difference rejects the candidate.

## Alciatore calibration and regression

Alciatore TP A.15 is spent evidence for v5. All nine published points remain
in the repository and are used under a pre-registered calibration/regression
contract. The seven interior angle points retain the v4 gates:

- angular RMSE no greater than 3 degrees;
- maximum absolute point error no greater than 5 degrees;
- expected contact and separation occur;
- every state is finite and passive;
- normalized repeated executions are byte-identical.

The endpoint contract is corrected to avoid inventing a direction for a
stopped ball:

- at 0 degrees, report cue-ball residual-speed ratio and lateral-speed ratio;
- at 90 degrees, report target-ball speed ratio;
- at both endpoints, require the intended contact, finite state, valid
  impulse direction, and passive energy behaviour;
- a trajectory direction is evaluated only when the relevant ball has enough
  speed to define one.

The full per-point signed residuals, absolute residuals, normalized residuals,
fit diagnostics, parameter covariance or equivalent uncertainty evidence,
and sensitivity results are committed even if aggregate gates pass.

## Independent evidence and confirmation lifecycle

Alciatore cannot serve as independent confirmation after informing v5. Before
v5 freeze, the repository must admit a new public frozen-shot or finite cue
contact experiment that is independent of both Alciatore and the compliance
calibration source. Admission requires source provenance, compatible measured
quantities, usable numerical resolution, clear coordinate semantics, and a
redistribution/licensing audit.

The lifecycle is:

1. preserve v4 freeze, Alciatore result, receipt, and rejection artifacts;
2. mark Alciatore as spent calibration/regression evidence for v5;
3. keep Han 2005 unopened and unexecuted;
4. calibrate compliance bounds from public contact-duration or force--time
   evidence and fit only registered observable quantities;
5. pass all mathematical, coupled, compatibility, game, performance,
   convergence, determinism, and Alciatore regression gates;
6. freeze the complete v5 identity;
7. with separate authorization, reserve and execute the new frozen-contact
   confirmation exactly once;
8. only if it passes, request separate authorization to reserve and execute
   Han exactly once;
9. accept v5 only if both independent confirmations pass.

A parse error, crash, timeout, integrity error, or physical mismatch consumes
the opened confirmation partition and rejects the candidate. A failure must
not be repaired by weakening Alciatore or confirmation thresholds. If no
suitable replacement source is available, v5 may remain an engineering
candidate but cannot be promoted as real-world confirmed.

## Data and provenance assets

All numerical material used for calibration, validation, and tuning is
committed whenever redistribution permits:

- complete extracted source numbers;
- normalized machine-readable data;
- units, coordinate systems, field definitions, and extraction notes;
- source identity, locator, version, licence, and acquisition date;
- hashes of raw and normalized material;
- deterministic transformation scripts and transformation logs;
- fitting inputs, outputs, residuals, sensitivity results, and full traces.

The project does not retain only summaries when complete numerical evidence
is available. If a third-party licence prohibits redistribution of a raw PDF,
video, or data file, the exception must be documented and the repository must
instead contain a reproducible acquisition procedure, expected raw-file hash,
complete legally redistributable normalized numbers, and every repository-
generated artifact. A source that cannot support reproducible audit is not
eligible for a promotion-critical gate.

## Candidate identity

The v5 identity binds all of the following:

- solver and formula versions;
- every physical parameter and unit;
- microstep, convergence, release, and safety configuration;
- calibration, regression, and confirmation package hashes;
- raw/normalized data and transformation-script hashes;
- validation tool version and executable hash;
- build profile and deterministic expected-result hashes.

Changing any bound element creates a new candidate. A changed candidate may
not reuse a confirmation transaction or the accepted/rejected identity of its
predecessor.

## Implementation boundaries and sequence

The finite-contact solver is a dedicated component rather than an expansion
of the existing instantaneous function. Cue-impact routing owns only topology
detection and path selection; the coupled island owns transient state,
microsteps, rigid-constraint coordination, convergence, and transactional
commit; telemetry owns versioned observation and serialization.

Implementation proceeds in this order:

1. freeze interfaces, unit-bearing configuration, telemetry, and failure
   categories;
2. add failing mathematical, routing, compatibility, and rollback tests;
3. implement transient cue state and single-contact finite-duration response;
4. extend the island to multiple balls and cushion constraints;
5. integrate transaction rollback and complete microstep telemetry;
6. add complete public compliance data and reproducible transformations;
7. perform registered fitting and Alciatore regression;
8. run ordinary-shot equivalence, full-game, performance, convergence,
   determinism, and clean-build verification;
9. acquire and freeze the new independent confirmation package;
10. execute the two separately authorized confirmation transactions in order.

## Completion criteria

v5 is complete only when implementation, tests, full numerical assets,
licence/provenance records, reproducible transformations, verification report,
and both independent real-world confirmations pass their pre-registered gates.
Implementation that passes engineering tests but lacks the replacement public
confirmation remains an engineering candidate and must not be described as a
validated real-world model.
