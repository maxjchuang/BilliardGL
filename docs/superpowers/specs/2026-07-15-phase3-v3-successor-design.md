# Phase 3 Integrated v3 Successor Design

**Date:** 2026-07-15

**Scope:** Phase 3 successor fitting, independent confirmation, and strict
release closure after the rejected `phase3_integrated_v2` candidate

**Status:** Approved design

## 1. Context and objective

Phase 3 themes 0–7 have implemented the parameter architecture, surface
motion, cue impact, ball collision, cushion response, pocket boundary,
multi-contact solver, and executable full-game acceptance matrix. Candidate
`phase3_integrated_v2` completed fitting, integration, full-game execution,
and a reproducible freeze, but its sole authorized Sudo confirmation
invocation returned without an atomic output directory, receipt, or
consumption ledger. The repository correctly preserved that outcome as a
fail-closed `FAILED` receipt and rejected v2.

This design continues Phase 3 through a new candidate,
`phase3_integrated_v3`. It does not reopen v2, replay Sudo as confirmation,
or reinterpret the failed receipt. It uses the now-spent Sudo public values
for diagnosis and representable calibration, preserves Derby–Fuller as an
unopened confirmation source, admits Han 2005 as an independent but
equipment-transfer-limited cushion confirmation source, fixes the
exactly-once transaction boundary, freezes one v3 candidate, and releases
only when every strict gate passes.

## 2. Non-negotiable constraints

- Preserve every v2 freeze, trace, receipt, ledger record, and failure byte
  unchanged. V2 remains rejected permanently.
- Never execute the v2 Sudo confirmation again.
- Commit every numeric input and output used for extraction, fitting,
  diagnosis, confirmation, comparison, or release. Do not commit copyrighted
  publication PDFs, figures, or page images.
- Derby and Han confirmation values never influence formula selection,
  parameter fitting, or candidate ranking. Their source precision and
  documented uncertainty determine acceptance intervals before candidate
  freeze; candidate predictions and residuals can never widen those intervals
  or change tolerances.
- Commit the candidate formula, parameters, metrics, intervals, applicability
  rules, scenario contracts, and budgets before freeze.
- Freeze exactly one v3 candidate. A confirmation failure rejects that
  candidate ID; it never triggers an edit under the same ID.
- Successful release status is exactly `PASSED`.
- Each implementation task ends in one independently reviewable commit.

## 3. Candidate and evidence lifecycle

### 3.1 Immutable v2 disposition

`phase3_integrated_v2` remains an immutable historical candidate with a
`FAILED` Sudo receipt. Its confirmation ledger remains the authoritative
record that the Sudo partition was consumed once. No command may regenerate
its output, complete its missing physical metrics, or replace its fail-closed
receipt.

### 3.2 Sudo transition to spent

A dedicated reviewed commit changes Sudo version `1.0.0` from
`confirmation` to `spent`. The transition records the v2 candidate, failed
receipt SHA-256, consumption ledger SHA-256, reason, and source revision.
After that transition, v3 tools may read the committed Sudo scalar values as
calibration or diagnostic evidence. They must use new v3 scenario IDs and
must not invoke the v2 confirmation command or write beneath the v2 candidate
directory.

### 3.3 New confirmation sources

Derby–Fuller version `1.0.0` stays `confirmation` and unopened. It confirms
surface/collision behavior through sliding time, final speeds, momentum, and
energy loss.

The Han package is based on Inhwan Han, “Dynamics in carom and three cushion
billiards,” *Journal of Mechanical Science and Technology* 19(4), 976–984
(2005), DOI `10.1007/BF02919180`. The paper reports high-speed-camera
validation and an empirical velocity-dependent cushion restitution relation.
The package commits bibliographic metadata, source URL and audited-file hash,
apparatus details, printed coefficients and precision, valid velocity domain,
normalization equations, uncertainties, applicability classification, and
all derived full-precision numeric points. The publication itself is not
redistributed.

Han used carom/three-cushion equipment rather than the production Chinese-pool
apparatus. The package therefore classifies absolute restitution values as
`TRANSFER_LIMITED`. They remain visible diagnostics. Hard confirmation gates
are limited to preregistered normalized curve error and constitutive
properties that can transfer honestly: finite output, bounds, continuity,
speed-response behavior over the source domain, and non-increasing total
energy. A successful result may claim cross-equipment trend confirmation,
not direct validation of absolute Chinese-pool cushion parameters.

## 4. v3 fitting and model scope

### 4.1 Surface motion

Sudo contains no independent surface-deceleration series. The surface model
and parameters inherit v2 unless the already-spent surface packages support a
deterministic refit that improves the preregistered spent-data objective
without regression. Derby cannot influence that decision.

### 4.2 Ball collision

The v3 ball-collision fitter combines representable Sudo ball restitution,
separation-angle, and transverse-momentum observations with the existing
spent Mathavan and Doménech series. Residuals are normalized by committed
uncertainties. Each experimental series contributes equal objective weight,
so a dense series cannot dominate through point count. Deterministic
tie-breaking uses the committed parameter order after objective value.

### 4.3 Cushion response

The v3 cushion fitter combines representable Sudo cushion restitution values
with existing spent Mathavan cushion series. It may select only from formula
families and bounds committed before fitting. The fit report includes every
row, full-precision prediction and residual, per-series objective,
apparatus/applicability label, parameter bounds, grid or optimizer settings,
and deterministic selection key.

Sudo's approximately `8 ms` contact-time plateau remains a structural model
gap while the production solver uses instantaneous rigid impulses. It must be
reported as an out-of-model spent residual. The implementation must not fake
agreement by adding a telemetry-only duration. A genuine finite-duration
compliant contact model would require a separate design because it changes
contact state, island solving, and time integration.

### 4.4 One selected candidate

Fitting produces one integrated v3 profile containing the selected surface,
ball, and cushion parameters plus the bounded cue, pocket, and solver
components. Pre-freeze verification executes all component and full-game
tests. Only that one profile may be frozen. No shadow candidate may be chosen
after either confirmation result is known.

## 5. Exactly-once confirmation transaction

### 5.1 Transaction boundary

The irreversible runtime-evaluation boundary moves before any confirmation
package content is read by the candidate evaluator. Package-admission and
schema tests may reproduce committed source rows before freeze, but they
cannot run a candidate or compute candidate residuals. The confirmation
transaction performs these steps in order:

1. Read only the frozen candidate's declared confirmation package path and
   expected manifest digest.
2. Exclusively create and persist a `STARTED` record identified by candidate,
   dataset, version, partition, freeze digest, and expected manifest digest.
3. Only after reservation succeeds, open and validate the package, construct
   scenarios, and start the frozen executable.
4. Write all scenarios, traces, provenance, CSVs, and reports to a sibling
   temporary directory; verify completeness and hashes; atomically rename it
   into place.
5. Exclusively create the hash-bound receipt and append a `COMPLETED` ledger
   record.

Reservation and receipt creation must fsync the file and parent directory.
Concurrent processes race on exclusive creation; exactly one may reserve,
and every loser exits before package access or executable launch.

### 5.2 Failure semantics

A package error, runner exception, incomplete trace, nondeterminism, failed
metric, output error, or receipt error fails closed. When the process remains
alive, it writes a `FAILED` receipt and final ledger record. If the process is
terminated after reservation, `STARTED` remains permanently consumed. A
recovery command may inspect filesystem state and finalize that attempt as
`FAILED_INTERRUPTED`; it cannot read confirmation values, start the game, or
permit replay.

The access-check command validates freeze structure, expected package path,
expected manifest digest, and unconsumed state without opening the real
confirmation package. CI may validate committed Derby/Han metadata and
extraction reproducibility, but candidate-evaluation and transaction tests use
synthetic fixtures and never execute a real confirmation partition.

### 5.3 Execution order

Derby executes first. A non-passing or interrupted Derby receipt stops the
plan before Han reservation. Han executes exactly once only after Derby is
`PASSED_OR_ACCOUNTED`. Any Han failure stops release.

Every receipt binds candidate ID, source revision, freeze, executable,
profile, dataset/version, package manifest, transaction state, and every
output file hash.

## 6. Verification strategy

### 6.1 Unit and fault-injection tests

Unit tests cover formulas, bounds, conservation/dissipation, phase
transitions, deterministic fitting, lifecycle legality, and source isolation.
The transaction suite injects failures before reservation, during package
open, before/after process launch, during trace output, before atomic rename,
during receipt creation, and during ledger finalization. It proves that any
post-reservation failure blocks replay and that concurrent calls launch at
most one runner.

### 6.2 Data and scenario tests

Extraction tests reproduce every Sudo/Han numeric row and conversion
byte-for-byte. Static dependency tests prove Derby and Han paths are absent
from fitters. Executable scenarios cover surface motion, ball impact, straight
cushion impact, pockets, multi-contact islands, and boundary modes while
preserving complete tick telemetry.

### 6.3 Full-game acceptance

The existing 12-case E2E matrix runs against v3 and preserves complete
numeric scenarios, traces, summaries, indexes, and provenance. Gates cover
determinism, cadence equivalence, host-load equivalence, legal randomized
sequences, scratches, pockets, breaks, contact stability, no numerical
failure, and existing performance budgets. A v2-to-v3 comparison lists every
change and rejects unexplained regression.

### 6.4 Reproducible freeze

Two clean Release builds from the selected source revision run at the same
stable path. Both executable hashes and emitted canonical profile hashes must
match. The freeze binds all fit inputs/outputs, source packages, scenario
contracts, calibration reports, full-game evidence, budgets, profile,
executable, source revision, and preregistered confirmation package manifests.

## 7. Irreversible confirmation checkpoint

Before Derby reservation, a human-reviewed checkpoint proves:

- exactly one v3 candidate and profile exist;
- every fit input is `spent` or `calibration`;
- Derby and Han have not been imported by fitters or evaluated against any
  candidate; admission/schema tests are the only pre-freeze readers;
- metrics, intervals, applicability rules, and scenario contracts are
  committed;
- two clean-build executable/profile digests match;
- confirmation ledger and output directories do not exist;
- CI contains no command that executes a real confirmation package.

Approval authorizes the one Derby transaction only. Han remains unopened
until Derby produces a passing receipt.

## 8. Release semantics

If Derby fails, the repository commits its immutable failure evidence and
does not execute Han. If Derby passes and Han fails, it commits both receipts
and stops. A failed v3 does not automatically create v4 or change tolerances;
it returns to a separate reviewed design.

If both pass, tooling generates a complete v1/v2/v3 comparison, artifact
inventory, and `phase3_release_v3.json`. Release validation requires:

- every required receipt is `PASSED_OR_ACCOUNTED`;
- no missing artifact, unexplained regression, new unaccounted mismatch, or
  post-freeze rule change exists;
- all full-game and performance gates pass;
- every artifact hash and source ancestry check passes;
- the production executable emits the exact frozen v3 profile.

The only successful status is `PASSED`. CI verifies committed evidence and
must never rerun confirmation. Phase 3 is complete only after this manifest
passes the strict release checker.

## 9. Implementation boundaries

Implementation is split into independently reviewable tasks for lifecycle
transition, Han evidence admission, v3 spent-data fitting, transaction
hardening, integrated candidate/full-game execution, freeze, Derby
confirmation, conditional Han confirmation, and final release. Each task uses
test-first development and one commit. All irreversible confirmation commands
remain separate tasks with explicit human checkpoints.
