# Phase 3 v4 Confirmation Recovery Design

**Date:** 2026-07-15

**Status:** Approved design; implementation not started

## Context

The sole Derby–Fuller confirmation transaction for
`phase3_integrated_v3` was reserved and opened exactly once. It failed before
producing physical frames because the generated scenario contained an empty
`expectations` array. The C++ scenario parser correctly rejected that input
with `invalid_scenario: expectations must be a nonempty array`.

That result is an interface-contract failure, not evidence for or against the
candidate's physical fidelity. The committed v3 receipt, ledger, rejection,
full-game evidence, and frozen artifacts remain immutable. Derby is consumed
and cannot be reused as independent confirmation.

The recovery candidate is `phase3_integrated_v4`, with full-game suite
`chinese_pool_full_game_v4`. Its physics formulas and parameter values must be
numerically and canonically identical to v3. The only candidate changes are
the repaired confirmation interface, candidate identity, lifecycle evidence,
and a replacement independent confirmation source.

## Goals

- Preserve and correctly account for the v3 failure without overwriting it.
- Repair the root cause in scenario generation while retaining strict C++
  schema validation.
- Exercise confirmation through the same parser and automation path used by
  real gameplay rather than through mocks alone.
- Restore two independent, one-time confirmation sources for v4:
  Alciatore TP A.15 followed by Han 2005.
- Commit the complete numerical reference data, transformations, traces,
  metrics, and receipts needed for later verification and tuning.
- Pre-register all metrics and thresholds before candidate predictions are
  computed from either confirmation source.

## Non-goals

- Tuning physics parameters from Derby, Alciatore, or Han confirmation data.
- Changing the v3 physics model while creating v4.
- Weakening the C++ parser to accept empty expectations.
- Treating Han's cross-equipment absolute cushion restitution as a direct
  Chinese-pool parameter measurement.
- Committing third-party PDFs or videos whose redistribution is not granted.
- Treating design or implementation approval as authorization to consume a
  real confirmation transaction.

## Evidence sources

### Alciatore TP A.15

David G. Alciatore's public technical proof TP A.15 reports a high-speed-video
experiment relating the source-defined cut angle and target-ball direction for
a frozen cue-ball shot. The published integer-degree pairs are retained in
full:

| Source cut angle (degrees) | Target-ball angle (degrees) |
| ---: | ---: |
| 0 | 0 |
| 8 | 13 |
| 20 | 34 |
| 34 | 50 |
| 46 | 61 |
| 57 | 70 |
| 67 | 78 |
| 77 | 87 |
| 90 | 90 |

The package must record the exact source locator, source-coordinate
definitions, extraction method, unit conversion, public-source audit hash,
and an independently reviewed interpretation of the two angle columns before
it is frozen. The source's integer presentation provides a numeric resolution
of half a degree; it is not represented as a published confidence interval.

The source PDF and high-speed video are referenced but not vendored. Extracted
numbers and all repository-generated data are committed.

### Han 2005

The existing `han_2005` package remains the second confirmation source. It
checks the normalized velocity dependence of cushion restitution over the
pre-registered 0.5–2.5 m/s evaluation domain. It retains its transfer
limitation: the experiment used 65.5 mm, 230 g carom balls and a pocketless
three-cushion table, so absolute restitution is not direct Chinese-pool
evidence.

## Data lifecycle

1. Preserve every committed v3 artifact byte-for-byte.
2. Transition Derby–Fuller 1.0.0 from `confirmation` to `spent` in a reviewed
   lifecycle commit.
3. Bind that transition to the hashes of the v3 attempt, receipt, ledger, and
   rejection artifacts. Do not infer a physical result from the failed run.
4. Admit Alciatore TP A.15 and retain Han 2005 as v4 confirmation packages.
5. Before v4 freeze, code may verify package structure, extraction
   reproducibility, and the synthetic execution contract, but may not compute
   v4 predictions or residuals from Alciatore or Han.
6. After freeze and explicit authorization, reserve and execute Alciatore
   exactly once.
7. Execute Han exactly once only if Alciatore passes and a separate execution
   authorization is received.
8. Reservation consumes a partition. A subsequent parse error, crash,
   timeout, evaluator exception, or physical mismatch cannot make it reusable.

If either confirmation result informs any later selection or tuning, that
evidence is spent for the successor and a new independently confirmed
candidate is required.

## Unified confirmation architecture

Confirmation uses a registry rather than dataset-specific branches embedded
in the runner:

```text
reference package
  -> source adapter
  -> normalized experiment scalars
  -> shared confirmation scenario builder
  -> C++ scenario parser
  -> gameplay automation execution
  -> complete per-frame physical trace
  -> dataset metric evaluator
  -> transaction-bound result and receipt
```

The adapter owns only source semantics, coordinates, units, and case
expansion. Scenario schema construction, base expectations, deterministic
execution, artifact writing, and transaction accounting are shared.

The automation boundary remains transport-neutral. A future socket or other
transport can implement the same load, act, step, observe, and reset commands
without changing scenario semantics or evaluation contracts.

## Scenario contract and root-cause fix

Every generated confirmation scenario has a nonempty `expectations` array.
The base contract is:

- `finite_state == true` for every scenario;
- `nonincreasing_translational_energy == true` after the initial cue input or
  for scenarios initialized directly in free motion;
- dataset-specific numerical comparisons remain in the confirmation metric
  evaluator and are not disguised as generic schema expectations.

Cue work is applied before the evaluated free-evolution interval so the energy
invariant does not incorrectly reject legitimate external input.

The C++ parser continues rejecting empty expectations. A negative regression
test must reproduce the v3 `invalid_scenario` failure and prove that readiness
cannot pass with such a scenario.

## Frozen-preflight execution proof

A synthetic, non-public fixture must traverse the same implementation path as
real confirmation:

1. reserve and open a synthetic confirmation transaction;
2. load it through the generic source adapter registry;
3. build scenarios through the shared builder;
4. parse them with the real C++ schema parser;
5. run them through the gameplay automation executable;
6. perform physical steps and evaluate the registered metrics;
7. complete a hash-bound synthetic receipt.

This test is not allowed to substitute a mock parser or mock executable. Mocks
may be used only for isolated failure injection around the real-contract test.
Readiness records the executable SHA-256, fixture manifest hash, canonical
scenario hashes, trace hashes, and successful parse/step result.

Static and mutation tests prove that fitting, candidate selection, routine CI,
and the synthetic preflight do not open or evaluate Alciatore or Han.

## Alciatore scenario and metrics

Each of the nine rows creates an independent frozen-shot scene. The adapter
maps source-defined angles into the engine coordinate system, places touching
cue and object balls, applies a centered horizontal cue input, and records:

- commanded cue direction and cut angle;
- cue position, contact point, direction, and impulse;
- per-frame ball position, linear velocity, angular velocity, and derived
  acceleration;
- ball-ball contact time, normal, impulse, restitution, and friction response;
- the target ball's earliest stable separating trajectory angle;
- energy and momentum diagnostics.

The seven non-degenerate interior points are the primary angular comparison.
Their pre-registered gates are:

- angular RMSE no greater than 3 degrees;
- maximum absolute point error no greater than 5 degrees;
- every expected contact occurs and every state is finite;
- no missing frames or non-physical energy gain;
- two normalized executions are byte-identical.

The 3/5 degree limits are engineering acceptance thresholds, not claimed
experimental uncertainty. Every raw, signed, absolute, and normalized error
is emitted even when the aggregate passes.

The endpoints remain in the package and report:

- at 0 degrees, lateral speed divided by incident speed no greater than
  `1e-3`, and target direction error no greater than 1 degree;
- at 90 degrees, target speed divided by incident speed no greater than
  `1e-3`;
- at both endpoints, finite state, valid contact-impulse direction, and no
  non-physical energy increase.

The 90-degree trajectory angle is not fabricated when target speed is too
small to define a stable direction.

## Han metrics

The current pre-registered Han contract remains unchanged:

- `normalized_curve_rmse <= 0.15`;
- `finite_bounded_response == true`;
- `continuous_response == true`;
- `source_domain_response == true`;
- `nonincreasing_total_energy == true`.

The result must continue distinguishing trend confirmation from the known
absolute-equipment transfer limitation.

## Execution integrity and final decision

Each real confirmation requires all of the following:

- executable SHA-256 matches the freeze;
- candidate profile, scenarios, package, and data hashes match the freeze;
- every planned scenario and frame is present;
- repeated normalized traces are deterministic;
- all base expectations pass;
- parsing, gameplay execution, and evaluation complete without exception;
- full trace, per-point metrics, logs, transaction record, and receipt are
  committed.

An integrity error is a confirmation failure, never a known physical mismatch.

The decision sequence is fixed:

```text
freeze v4
  -> execute Alciatore once
       -> failure: reject v4; do not open Han
       -> pass: request separate authorization and execute Han once
            -> failure: reject v4
            -> pass: accept v4
```

`phase3_integrated_v4` passes only when both packages pass every numerical,
invariant, integrity, and determinism gate; neither result has been used to
select parameters; and the final assessment lists all errors, limitations,
and consumption records. Otherwise it must not be described as validated
against the real world.

## Repository artifacts

The implementation adds the following governed outputs:

```text
tests/physics_validation/reference_data/alciatore_2005_tp_a15/
  raw_extracted.csv
  normalized.csv
  extraction.json
  scenario_template.json
  source_access_audit.json
  manifest.json
  split.json
  expected_reference_limitations.json

physics_models/candidates/phase3_integrated_v4/
  candidate.json
  freeze.json
  full_game/
  confirmation_readiness.json
  confirmation_consumption.json
  confirmation/alciatore_2005_tp_a15/
  confirmation/han_2005/
  final_assessment.json
```

Each confirmation directory contains canonical input scenarios, complete
per-frame traces, point-level CSV/JSON metrics, standard-output/error summary,
transaction state, and a receipt binding the candidate, executable, package,
and outputs. Complete repository-generated numerical data are committed.

## Test matrix

Before freeze, tests must prove:

1. Alciatore extraction is reproducible and every manifest hash is valid.
2. Every generated confirmation scenario has valid nonempty expectations.
3. Empty expectations reproduce the v3 failure and block readiness.
4. The synthetic fixture passes the real parser and automation path.
5. Repeated scenario execution is deterministic.
6. Fitting, selection, routine CI, and preflight cannot open Alciatore or Han.
7. Derby is spent and its transition binds immutable v3 evidence.
8. Canonical v4 physics values equal v3 physics values field-for-field.
9. All committed v3 artifact hashes remain unchanged.
10. Transaction retry, concurrency, crash, and duplicate execution tests fail
    closed.

## Implementation and review sequence

Use independently reviewable commits in this order:

1. commit this recovery design;
2. migrate Derby lifecycle and add immutable-evidence tests;
3. introduce the adapter registry and repair the scenario contract;
4. add synthetic real-path and negative regression tests;
5. add the complete Alciatore package and source audit;
6. add Alciatore and Han execution/evaluation adapters;
7. generate v4 and prove canonical physics equality with v3;
8. perform two clean builds, full-game validation, and freeze;
9. generate confirmation readiness and request execution authorization;
10. after explicit approval, execute Alciatore exactly once;
11. only after it passes and separate approval is received, execute Han once;
12. preserve results and produce the final assessment.

Steps 1–9 do not authorize steps 10–12. Real confirmation consumption remains
an explicit, separately approved irreversible action.
