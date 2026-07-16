# Phase 3 v5 Physics Assessment Report

Status: **REJECTED — CONFIRMATION CONTRACT INTEGRATION FAILURE**

Candidate: `phase3_integrated_v5`
Profile assessed: `chinese_pool_full_game_v5`
Frozen source revision: `0245f115850e94a94917e65c49556c354eba20f3`
Frozen executable SHA-256: `b86fd2460ddecc8f9ba4eec500dedfdc8047e6badc29da314acab295b4390b9e`

## Decision

The v5 candidate is rejected and cannot be promoted. Cross 2016 was consumed
once from the immutable freeze and its governed receipt is `FAILED`. Han 2005
was not opened because policy requires Cross to pass first. The Cross attempt
must never be deleted, retried, or reinterpreted as passing.

This rejection identifies an incompatibility between the preregistered
observation gate and the production surface model, not an unstable or
energy-creating solve. A future candidate needs a newly frozen confirmation
contract and cannot reuse the consumed Cross partition.

## Engineering evidence

All pre-confirmation engineering gates passed:

- two clean Release builds produced identical executable and profile hashes
- ordinary v4 ball, surface, cue, cushion, boundary, and solver behavior remained exact
- spent Alciatore regression, residuals, and sensitivity evidence were bound
- all 12 full-game stress scenarios passed with complete committed traces
- the preregistered performance budget passed
- microstep halving changed final pair velocity by no more than 0.5 cm/s
- deterministic microtrace, finite-state, passive-energy, and release tests passed

The final assessment binds the freeze, readiness checkpoint, inventory,
profile, full-game tree, performance budget, ordinary equivalence evidence,
Alciatore regression artifacts, and convergence contract.

## Cross 2016 one-time confirmation

Attempt: `87849a0361083701d471e79528c282dcd3626c8d3fead35f77545c3ddb88dab9`
Receipt: `FAILED`
Package manifest SHA-256: `f1f9397a4f3bdf936434c628fb74c320c15bdea1ae25c5faf6a313ed9d016086`

Complete frames, the 544-step microtrace, repeated execution, finite state,
passive energy, coupled frozen contact, release, and no recontact all passed.

The committed trace's final back/front speed ratio is `0.9999981407287482`.
Its residual from unity is `-0.000001859271251847261`, inside the Cross limit
of `±0.0522185764993973`.

The formal evaluator nevertheless failed `stable_release_passed`: it requires
each absolute speed to change by no more than 0.1% between consecutive
0.1-second frames. Production rolling resistance produced a 1.3657418% change
while preserving the ratio. With no stable observation,
`uncertainty_aware_equal_speed_passed` also failed and the receipt finalized as
`FAILED`.

Root cause:
`CONFIRMATION_EVALUATION_CONTRACT_INTEGRATION_FAILURE` in
`cross_2016_confirmation.py::_stable_release_speeds`.

## Han 2005

Status: **NOT_EXECUTED**

No Han reservation, ledger entry, output, trace, metric, or receipt exists for
v5. Cross failure permanently closes the Han branch for this candidate.

## Evidence boundary and follow-up

The trace is diagnostic evidence that the solver transfers nearly equal speeds,
but it cannot override the failed predeclared receipt. A successor should
separate ratio stability from absolute cloth deceleration, freeze that rule
before opening new evidence, and retain rolling resistance unless independent
data justifies changing it.

Remaining limitations include elevated cues, jump and masse shots, measured
miscue boundaries, complete spin measurements, Chinese Pool cushion material
transfer, real pocket trajectories, and chaotic break-shot datasets.

## Verification

- `ctest --test-dir build --output-on-failure`
- `python3 -m unittest discover -s tests/physics_validation -p 'test_*.py'`
- `python3 scripts/check_phase3_physics_release.py --root .`
- `git diff --check`

The assessment generator exits 1 by design for this immutable rejection. The
release gate accepts only the byte-reproducible rejected checkpoint and reports
it as not promoted.
