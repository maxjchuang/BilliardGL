# M4 current rule coverage review — 2026-09-22

## Purpose and evidence status

This is a source-level successor review of the current tree. It does not replace or modify
`phase5-wpa-heyball-as-built-reconciliation-audit-v10.json` or any frozen gate/receipt.
The v10 audit and the earlier WPA-II-12-3/17(e) defer documents describe the tree before
the D6 table-departure producer, observer, runtime consumer, and product policy wording
were integrated. Their historical claims remain intact; where they disagree with the
current source, this document records the newer source evidence.

No test was built or executed while preparing this review. All test entries below mean
"test source inspected", not "freshly verified". Root's current integration run must be
recorded in the verification ledger before any release or row-closure claim is made.

## Selected product policy

The selected policy authority remains
`docs/rules/m4-wpa-policy-bundle-root-selected-v1.json`:

- `WPA-II-12-3`: `SIGNED_TOURNAMENT_PROFILE_DEFAULT_17E_FOUL`.
- `WPA-II-16`: `KEEP_JUMP_DISABLED_FOR_M4`.
- `WPA-II-17`: `EVIDENCE_BACKED_SUBSET_PLUS_SIGNED_REFEREE_EVENTS`.

For the named shipping profile, an object ball other than the 8 that comes to rest off
the table is therefore a WPA II 17(e) general foul and is removed from play. The 8-ball
off table is an independent immediate loss. This review does not authorize or implement
the mutually exclusive WPA-II-12-3 "legal disappearance" interpretation.

## WPA-II-12-3 / WPA-II-17(e): current source chain

### Physical evidence and fact construction

- `GameRuntime::finishRulesShot` in `src/Billiards/game_runtime.cpp` refuses to finish an
  active table-departure observation unless the observer reports both valid and complete.
  It converts only complete `cameToRestOffTable` measurements into
  `ShotFacts::cueBallOffTable` or `ShotFacts::objectBallsOffTable`.
- `GameRuntime::applyRulesAdjudication` passes those facts to the rules engine, marks the
  affected `BallState::outOfPlay`, and does not invent a pocket capture. Its terminal
  adjudication guard prevents an 8-ball loss from creating later group, break, eight-ball,
  or ball-in-hand decision offers.
- The producer/observer/adapter implementation is outside the narrow rules function, but
  the runtime consumes the committed observation rather than a position-only guess or an
  imported boolean.

### Rules adjudication

- `ChineseEightBallRules::adjudicate` in
  `src/Billiards/rules/chinese_eight_ball_rules.cpp` adds
  `FoulReason::CueBallOffTable` and `FoulReason::ObjectBallOffTable` for the selected
  general-foul behavior.
- The same function detects ball 8 in `objectBallsOffTable`, calls the illegal-eight
  terminal path, and returns immediately. This preserves the independent 8-ball loss
  instead of treating it as only an ordinary ball-in-hand foul.
- `GameRuntime::applyRulesAdjudication` supplies the existing ordinary-foul ball-in-hand
  transaction for the non-terminal cue/non-8 cases.

### Product disclosure

- `ProductShellController::itemsForScreen`'s Help content in
  `src/Billiards/product_shell_controller.cpp` states:
  `Selected profile: WPA II 17(e) non-8 off-table foul; 8-ball off-table loses`.
- This wording identifies the chosen conflict resolution to the player; it does not claim
  that both incompatible interpretations are simultaneously supported.

### Inspected tests (fresh execution pending)

- `tests/game_runtime_table_departure_observation_tests.cpp`
  - `runObjectOffTableObservation(3)`: real physical departure produces a complete
    measurement, an object-off-table fact, `outOfPlay && !pocketed`, and a foul.
  - `runObjectOffTableObservation(8)`: the same physical chain produces terminal loss,
    no post-shot decision authority, and a blocked attempt to resume play.
  - `runCueOffTableObservation()`: cue departure produces the cue fact, removal without
    capture, and a foul.
- `tests/chinese_eight_ball_rules_tests.cpp`: direct facts cover 8-ball terminal loss,
  the selected non-8 general-foul behavior, duplicate rejection, pocket/off-table
  conflicts, and invalid cue/object identities.
- `tests/rules_runtime_integration_tests.cpp`
  - `testRulesCoreAOutcomes()` covers direct rules/runtime outcomes.
  - `testAutomationRulesCoreARoundTrip()` covers automation round trips for 8 and non-8
    off-table facts.
- `tests/product_shell_tests.cpp::testScreenFlowAndApprovedProductEntry()` locks the
  selected-policy Help wording.
- `tests/shot_fact_collector_tests.cpp` covers the removal boundary: an already adjudicated
  off-table ball is not re-reported as remaining, newly pocketed, or repeatedly off table.

### Current assessment

The old assertions that D6 physical evidence is absent and that the 12.3/17(e) product
choice is unresolved are stale relative to the source listed above. At source-review
level, the selected 17(e) behavior has a physical producer-to-product chain. This document
does **not** mark WPA-II-12-3 or WPA-II-17 verified or closed until the fresh tests below
are reported. WPA-II-17 also has independent remaining gaps even if this subchain passes.

One source comment adjacent to the 8-ball branch still calls the general off-table policy
"unresolved". That comment predates the selected policy and current Help disclosure; it is
not treated as authority or as evidence that the implemented behavior is absent.

## WPA-II-16: real remaining boundary

The selected M4 product behavior is to keep jump shots disabled, not to claim complete
jump-shot legality:

- `src/Billiards/cue_contact.cpp` and
  `src/Billiards/coupled_cue_contact.cpp` reject non-zero cue elevation with
  `cue_elevation_requires_3d`.
- Shot-control/product capability paths keep cue elevation unavailable unless an explicit
  capability is promoted.
- Current airborne table-departure telemetry does not prove the cue-tip strike hemisphere,
  a legal non-scoop launch, obstacle overflight, or a miscue classification.

Minimum future implementation boundary, if jump is ever enabled:

1. A D7 design/implementation gate for real elevated cue contact and vertical impulse,
   including scoop and miscue evidence.
2. Ordered takeoff, obstacle-overflight, and landing evidence bound to one shot identity.
3. A rules classifier that consumes only complete evidence and fails closed on missing or
   conflicting intervals.
4. Capability-gated product input plus positive, negative, and replay-determinism tests.

Merely observing that a ball became airborne, or deriving a jump from tip offset, is not
an acceptable substitute. `KEEP_JUMP_DISABLED_FOR_M4` leaves the row unimplemented by
design.

## WPA-II-17: implemented subset and real remaining gaps

Current source includes ordinary physical foul paths (scratch, wrong first contact,
no-contact/no-rail, D5 double-hit/push/frozen-cushion classifications, and the D6 off-table
subchain above). It also includes the signed referee subset in
`NonphysicalGeneralFoulAuthority`:

- both feet off the ground during a shot;
- illegal body/clothing/equipment ball touch;
- one-hand shot;
- auxiliary equipment placed on the table to shoot;
- mobile-device disturbance as a general foul.

The following remain real and must not be hidden by the now-present D6 chain:

- **Repeated mobile-device violation / loss of rack — core implemented, product pending.**
  The current authority recognizes both first and repeated violations, tracks each
  player within the rack, and commits the repeated violation as rack-only terminal
  state. Authority tests cover the sequence and player isolation. A legacy consumer
  unable to represent structured rack-loss outcomes still correctly rejects it; that
  negative test is not evidence that the modern authority lacks repeat support.
  Secret-free replay and live HUD/coordinator consumers have CPU coverage. Trusted
  product referee input and native terminal pixels remain unverified.
- **Remaining §17 inventory.** Only the evidence-backed automatic and signed-referee
  allowlist is implemented. Unobservable human/referee acts must not be inferred from
  physics, UI input, or absence of evidence.
- **Whole-row closure evidence.** The implemented subsets do not by themselves prove every
  §17 item, product transaction, idempotent retry, replay, and rack-lifecycle boundary.

Remaining product slice for mobile-repeat loss of rack:

1. Provide a provisioned, trusted referee adapter; ordinary player input and physics
   must not manufacture signed observations of nonphysical acts.
2. Route validated first and repeat declarations through the existing authority,
   preserving actor/rack/parent bindings, idempotency and stale rejection.
3. Exercise first offense through the existing FullTable placement UI and repeat
   through the rack-terminal HUD in the actual visible product window.
4. Verify rejected, conflicting and cross-rack input has no authority or replay
   side effects; retain existing CPU authority, recorder and seek regressions.

This slice must not silently equate a rack loss with a match loss unless a separately
selected match-format policy supplies that meaning.

## Fresh verification ledger

Root baseline execution, 2026-09-22: Release directory
`build/m4-current-audit-20260922`; full `scripts/check.sh` reached CTest.
The five targets listed below all passed: table departure 1.37 s, Chinese eight-ball
0.14 s, rules runtime integration 0.25 s, product shell 1.30 s, fact collector 0.13 s.
The overall non-rendered suite was 127/128, not PASS (unrelated first-shot proxy input
assertion failed). Log retained at
`/Users/bytedance/Projects/github/BilliardGL-evidence/m4/20260922-baseline-r1/logs/billiardgl-m4-20260922-check-resume.log`.
This verifies the cited selected 17(e) chain on the pre-product-interaction baseline;
it does not close all of §17 or promote the historical v10 audit. Subsequent session/UI
changes require fresh regression. Historical initial ledger follows.

Status at document creation: **PENDING — source inspected only; no build or test executed
for this review.**

Root should append the exact build directory, command, target results, and artifact/source
revision after the current integration run. At minimum, the evidence set should include:

- `BilliardsGameRuntimeTableDepartureObservationTests`
- `BilliardsChineseEightBallRulesTests`
- `BilliardsRulesRuntimeIntegrationTests`
- `BilliardsProductShellTests`
- `BilliardsShotFactCollectorTests`

Until those results are recorded, the permitted conclusion is limited to: current source
contains the selected WPA-II-12-3/17(e) chain and the cited test coverage; fresh execution
and any formal audit-row promotion remain pending.

## Product-entry audit after native 06C verification

This supplements, rather than replaces, the original full-rule requirement. Shipping
06C now has fresh evidence: `BilliardsRulesRuntimeIntegrationTests` and
`BilliardsModernVisibleDrawableE2E` both passed in the main Release build (r9).
The native fixture uses a real semantic shot and collector/adjudication, then verifies
rendered options, arm/confirm, focus-loss cancellation, recovery, and no duplicate
commit. Local two-player actor setup is an explicit test fixture, not a menu test.
Log: `20260922-baseline-r1/logs/billiardgl-m4-r9-tests-20260922.log` under the evidence root
above. This does not verify every choice branch or a complete human match.

| Requirement | Current source evidence | Remaining work |
| --- | --- | --- |
| WPA-II-06-C illegal break | Typed shipping capability, real native accept-table path, all three choices in CPU product tests | Full integrated regression and native rerack outcomes |
| WPA-II-06-E break foul | Shipping typed capability and native scratch → parent choice → BehindHeadString pointer/explicit-confirm flow; preserved r3 evidence hashes rechecked 2026-09-23 | Final integrated regression and full display matrix; BehindHeadString evidence does not prove ordinary FullTable BIH |
| WPA-II-06-F eight on break | Shipping typed legal/foul variants and native 8-ball+scratch → spot/BehindHeadString child; r3 modal/hatch/camera evidence and hashes preserved | Final integrated regression and full display matrix; not every rerack branch or whole human match is proved by this slice |
| Group choice and ordinary BIH | Typed session and app capabilities now implemented; native group arm/confirm progressed; ordinary scratch/FullTable native validation in progress | FullTable native run currently fails placement_occlusion_exceeded; must fix layout without relaxing its gate, then verify complete local human-match flow |
| WPA-II-06-G no rack contact classification | Signed shot-bound declaration + private native Miscue result + complete no-contact collector now produce exactly two rerack choices and no BIH; unsigned/imported/proposal-only cases remain classification-pending | CPU boundary only: shipping tip clamp does not normally expose this miscue; no native UI closure or replay support. Recorder explicitly rejects non-Inactive 06G timelines |
| §17 repeated mobile disturbance | Signed first/repeat history and rack-only terminal; replay secret-free receipt/seek tests; live shell + RulesPresentation + actual HUD/coordinator consumers now covered by CPU tests, including 9 viewport/scale draw-list cases | Trusted product referee input remains unavailable; final native terminal HUD pixels not yet verified. Do not call this full product rule closure |

`internal.rules_decision_ui_implementation_v1` is not a shipping authorization switch.
Open only typed, explicitly tested branches; completing a parent choice while leaving
its BIH child unavailable does not close that rule. Production placement must use the
actual rendered camera and pointer coordinates, retain runtime legality checks, and
require an explicit confirmation rather than mutating authority on hover.
