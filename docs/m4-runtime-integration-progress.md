# M4 runtime integration checkpoint — 2026-09-07

The completion criteria remain the original Phase 0–5 plan in
`superpowers/specs/2026-07-27-billiardgl-visual-gameplay-optimization-plan.md`.
Later suggested participant counts, device matrices and fixed AI percentage
bands are not additional mandatory acceptance criteria.

## Latest checkpoint — automatic checks and six-scene review pass; human play pending

Current status (the paragraphs below retain chronological findings):

- Final six-scene capture and actual/diff review are complete. Root and TA both
  accept all six original scene purposes. Each final process attests the correct
  default glTF/manifest, with no load errors; full runtime states match all six
  corresponding pre-asset scenes. CPU-render P95 is 2.269–2.410 ms and P99 is
  2.367–2.566 ms at 1080p Medium (not including display swap). Remaining known
  visual polish includes the cue ball's IBL light/dark band; it was not silently
  declared fixed. The concise current handoff is `m4-final-verification-handoff.md`.
  Human play of the 12 exercises is the outstanding external acceptance step;
  the Goal remains active and M4 is not declared fully accepted.
- Final root `scripts/check.sh` now completes with **exit code 0**:
  **127/127 CTests**, **414/414 Python physics tests** (32.256 seconds), followed
  by release and standalone validation. The Phase 3 historical candidate remains
  REJECTED/not promoted. The final application binary and renderer source hashes
  are unchanged from the frozen references. Six final actual scenes are being
  captured into `/tmp/billiardgl-m4-final-six.PfmMvT/capture` after all builds and
  CPU test runs have finished. Human training play remains NOT_RUN.
- The promoted pocket geometry/material passed root corner, side and overhead
  review. TA reports **17/17 modular tests PASS** (47.688 seconds). Root's first
  post-promotion original check reached **126/127 PASS**, with only the old
  exact draw-package counts failing. Those assertions were updated to verified
  defaults (59 meshes, 92 nodes, 8,064 source triangles; LOD0 4,644/24 draws,
  LOD1 2,276/24, LOD2 1,264/21), without relaxing budgets; developer fresh target
  passes. Root also passed **6/6 actual rendered regressions** against the final
  default asset (8.32 seconds). A second complete original check is now running.
  This current paragraph supersedes the older failure chronology below.
- Goal remains active; M4 is **not yet accepted**. The latest full original
  check rebuilt successfully but stopped with **125/127 CTests passing**:
  ModernProductPointerSessionE2E (`validated replay enters playback`) and the
  in-progress Phase2ModularGltf candidate geometry contract. These are assigned
  to the content developer and technical artist respectively. Exit code is 8;
  subsequent Python/release stages were not run by that invocation.
  Root subsequently ran all **414 Python physics tests successfully** (36.015
  seconds) and **6/6 fresh rendered regressions** (8.10 seconds): rendered
  automation, full-power break camera/settle, production renderer, seamless
  cubemap state, glTF shadows and modern application. These separate passes do
  not turn the failed original invocation into a green one.
  The pointer regression is subsequently repaired: its fixture now uses the
  explicit product profile, and cold-start trusted replay lookup supplies the
  matched training card's validated profile instead of the cold live runtime's
  legacy profile. Root reran pointer, product shell and training controller:
  **3/3 PASS** (4.91 seconds), preserving strict checks and live isolation.
  The next full original invocation is still pending the asset repair.
- All 12 training exercises have real shipping-profile successful actions;
  root independently reproduced the multi-shot routes with exact stopped states.
- Twelve bound reference files are frozen after two-pass recording with the
  actual shipping Release `-O3 -DNDEBUG` configuration. All 12 product Watch
  references reach their strict endpoint hashes in developer verification.
  Debug/O0 self-record/self-replay tests are separate from shipping-asset validation.
  Cross-optimization final-state equality is not claimed or enforced by
  weakening the replay hash.
  After the pointer repair, the final two-pass provenance refresh completed
  against root's Release executable SHA-256
  `b6aa17f80fb8d32542a25023a22790c16af546d93b0eb986e7f20c0d0cfd892c`.
  Root independently matched this executable hash to `reference_generation.json`.
  The developer verified all 12 action/trace mappings and replay-file bytes are
  unchanged, all two-pass matches are true, the three training targets pass,
  offline validation passes 12 scenarios plus five bad fixtures, and 14 Python
  reference tests pass. Source and reference content are frozen for integration.
- Replay scene/camera/frame-time integration, real glTF shadows, terminal
  off-table-eight protection and scenario-load authority isolation have targeted
  root verification. A fresh complete original check is still required after
  the current content/asset changes settle.
- Pocket geometry is unpromoted. Object-ID images identify the central solid
  block as the broad `liner_rim` strip. Removing it exposed table-body fill in
  the opening; technical art is correcting the full exposed footprint. Actual
  candidate loading is now bound by process-side asset attestation. Physics
  and the accepted renderer remain unchanged.
  Root checked the actual shipping profile: mouth and throat widths are both
  13.2 cm for corners and 8.6 cm for sides; throat/capture depth is 6 cm.
  The earlier visual D-shaped single-point throat was an incorrect candidate
  assumption, not a product requirement. The visible opening must not obstruct
  that actual passable band or the exposed cloth-cut footprint.
  Subsequent root ray checks verified the closed deep cavity: five diagnostic
  opening rays hit pocket-void geometry before the cabinet, whose bottom had
  previously remained visible through an incomplete lining. Root accepted the
  restored outer frame and, after three-image review, the nose-only jaw candidate
  `/tmp/billiardgl-pocket-nose-only.yoxZVt` (7,788 triangles). The broad growing
  jaw top ribbon caused the visible triangular wings; it has been removed without
  changing physical anchors. The remaining scoped art adjustment is a rounded
  visual closure beyond the 6 cm capture band. The final candidate, all 17 asset
  tests and default promotion remain pending; these partial reviews are not M2
  acceptance.
  The next rounded-back candidate
  `/tmp/billiardgl-pocket-rounded-back.RndJRW` was reviewed in corner, side and
  overhead images but was not promoted: only its lower shell was rounded, while
  the upper rear boundary remained a straight 6 cm chord. The corner still read
  as a square box. The requested correction extends the upper rear arc beyond
  capture while retaining the complete shipping passable band. A developer is
  independently identifying the gray surface visible in the overhead pocket.
  Root subsequently accepted the rounded-upper geometry candidate
  `/tmp/billiardgl-pocket-rounded-upper.YurVwc` after corner and overhead review
  (8,064 triangles). Geometry is now frozen. The remaining gray-cap appearance
  was independently identified as the actual 145 mm-deep bottom fan receiving
  unoccluded dielectric environment reflection, not a top cover. The existing
  runtime supports a data-driven pocket texture set, so the assigned final
  adjustment is asset-only local cavity occlusion, with the original 16 MiB
  texture budget retained. No renderer, shader or physical geometry changes are
  authorized by this adjustment. Default promotion and integrated checks remain
  pending this material presentation fix.
  Root reviewed the bottom-only AO candidate
  `/tmp/billiardgl-pocket-depth-ao.eVr3Bw` in actual corner and overhead images
  at `/tmp/billiardgl-pocket-depth-ao-views.nNI4ZW/report.json`: the gray-cap
  appearance is removed while the accepted opening and surrounding materials
  remain intact. This scoped material fix is accepted. TA is authorized to
  complete the matching side view, 17 asset tests and default promotion with
  rollback preserved. The candidate validates at 8,064 triangles, 29 materials
  and 22 textures; PBR textures occupy 16,777,188 bytes (below 16 MiB).
  The matching side view also passed root review. Default assets have now been
  promoted: glTF SHA-256
  `c51da380b9a528a1f0671f4617e4f6e1a22e06e6a6a5f20e13cddedaf572a158`,
  manifest `dc3ebc3767dd2596437044d8818167715cc5c897e2de31b7dea8281c0f4b2bb6`.
  Root independently read both hashes. Previous defaults are preserved at
  `/tmp/billiardgl-phase2-default-before-pocket-promotion.IdHt3r`.
  Developer resource verification passed 11 PBR Python tests and fresh C++
  foundation/importer targets (2/2); two remaining modular test failures refer
  to the previous default generation mode/count and are being synchronized
  before the final original check. Promotion alone is not integrated M4 acceptance.
- Human training play remains NOT_RUN; see `m4-training-manual-checklist.md`.

Reference generation provenance is recorded in
`assets/training/phase4/reference_generation.json` (Apple clang 17,
`-O3 -DNDEBUG`). Root independently passed the pack validator (12 scenarios,
5 invalid fixtures) and has started a fresh original `scripts/check.sh` after
the developer froze training source/content. The later build directory named
`build-ai-physical-o2` actually used Release `-O3`; its name is not evidence of
an O2 verification. True Debug used `-g` without `NDEBUG`. The observed
cross-configuration difference was ball 0 visual rotation angle, with unchanged
shot actions, settle tick and rules; strict replay hashes were not weakened.

The previous original `scripts/check.sh` completed with exit code 0 using
`/tmp/billiardgl-v19-closure-release.UFVB27`: **127/127 CTests passed**, followed
by **414/414 Python physics-validation tests** (35.109 seconds) and the release
and standalone validation stages. The historical Phase 3 candidate remains
**REJECTED (not promoted)**; no frozen acceptance evidence was rewritten.
Earlier failing runs below are chronological diagnosis, not current status.

This is not yet M4 visual acceptance. The corrected public-camera six-scene
baseline completed 6/6 at
`/tmp/billiardgl-m4-corrected-six-before-20260907/report.json`, recording exact
commands, full state, asset/image hashes and CPU/GPU timing. Its frozen binary
predates the pending default seamless-cubemap change. Compilation overlapped
this capture, so its timings are observations, not an isolated performance
comparison. Pocket-opening geometry and the seamless reflection fix still
require integrated images, visual review and final regression after promotion.

A subsequent bounded plan-to-production review found a real coverage gap:
ReplayTransport's presented-frame rate scheduler was tested in isolation but
not connected to the product frame loop. The product playback bridge and its
entry-point regression now pass a fresh root rebuild of
`BilliardsModernProductPointerSessionE2E` (0.22 seconds). Root review additionally
requires the rendered ball/camera presentation to come from replay rather than
the unchanged live authority; that integration is being checked, not inferred
from a ticking transport. Original Phase 4 also asks
for manual play verification of all twelve exercises; automated fixtures do
not establish that human result. No participant counts, external-observer
signatures or device matrices are added to that original requirement.
The twelve scenario reference-replay fields remain unbound placeholders;
actual recorded successful attempts and a usable content entry point are still
required. Static disabled capability defaults in the asset README alone do not
prove locked product cards: the current product injects horizontal and
longitudinal tip capabilities.

Seamless cubemap filtering is now default-on with explicit
`BILLIARDGL_CUBEMAP_SEAMLESS_DIAGNOSTIC=0` rollback. Production default/rollback
images exactly reproduce the prior diagnostic on/off images. Root independently
passed the real-GL default/override/initial-state/failure/shutdown restoration
test; it is registered as `BilliardsModernRendererSeamlessCubemapState` with the
rendered label. This fixes felt reflection seams, not every lighting concern.

Original P2-B shadow-map coverage is also genuinely incomplete: production glTF
returns zero from its depth-only draw path, and the mesh receiver has no shadow
sampler. Existing screen-space ball contact patches and material AO do not
provide rail/pocket cast shadows. The graphics developer is implementing the
bounded light-space caster/receiver path and actual GPU regressions. Quality
metadata or an empty named shadow pass is not accepted as implementation.

The open-apron pocket candidate was reviewed in corner, side and overhead
views and **not promoted**: it replaces walnut fill with outward trapezoidal
chutes, but does not yet read as a compact recessed pocket. Root identified a
coordinate-contract concern: physics pocket depth is
`-dot(relative, inward)`, whereas the legacy visual generator places its drop
center at positive inward Z and rejects outward liner/void vertices. Technical
art is checking the actual shipping throat/capture coordinates before proposing
a corrected vertical cavity. Physics is not to be changed to fit visual errors.

The replay presentation source now selects the isolated replay runtime in
ReplayPlayback; the fresh root pointer-session regression passes again
(0.22 seconds), including replay ball motion and unchanged live authority.
Elapsed-time scheduling and dedicated free/broadcast presentation camera frames
are now implemented too. A fresh root pointer-session rebuild passes (0.24
seconds), with 30/60/120 Hz equal-duration progress, quarter-speed scaling,
pause without catch-up, camera changes and unchanged live authority. The
developer's O0/O2 ModernApplicationE2E also passes; final whole integration is
still required after the pending renderer/content changes.

The first actual glTF shadow candidate now reports 27 shadow draws instead of
zero. Root reviewed ball close-up with contact compensation off, original
strength and balanced (25% of original). Balanced is approved for six-scene
verification, not full visual acceptance; the original-strength override is
`BILLIARDGL_GLTF_CONTACT_SHADOW_COMPENSATION=1`, and zero isolates shadow-map
contribution. Moving-caster/world-space stability/quality/performance evidence
is still pending. Technical art confirmed the pocket coordinate mismatch and
is implementing a compact outward-depth cavity tied to the shipping 6 cm
throat/capture plane, instead of the rejected long apron slope.

Root now approves the glTF shadow/authored-key implementation as the renderer
integration baseline after viewing all six final scenes. This does not approve
the still-old pocket asset. Root reran the final real-GPU regression successfully
(`static_draws=11`, `receiver_changed_pixels=872`), including off-table/frustum
isolation. The actual/diff bundle is
`/tmp/billiardgl-gltf-shadow-root-six-diff-20260907/report.json`; all six runtime
states are exactly unchanged. Final captures are in
`/tmp/billiardgl-gltf-shadow-six-final.hUjEMa/capture`; CPU render P95 ranges
2.252–2.486 ms and shadow GPU P95 0.003–0.040 ms. These are not full UI/swap
latencies, and the cross-run decrease is not attributed causally to the change.

Root's actual training calibration exposed a prerequisite missed by synthetic
AttemptFacts tests: all twelve embedded initial scenarios still select legacy
physics with `time_step_seconds=0.1` and legacy boundary defaults. Session load
replaces the production profile with that scenario profile. Legacy successful
shots (including root follow/draw candidates) are therefore diagnostic only,
not production acceptance or reference replay inputs. The content developer is
upgrading embedded canonical scenarios to explicitly bind the full shipping
120 Hz profile/hash and ProductionTable boundary, retaining initial layouts
and scoring targets, before redoing real successful-action searches.

The explicit shipping-schema-12 candidate pack at
`/private/tmp/billiardgl-shipping-training.PjCU9o` now loads successfully.
Root verified actual successful follow and draw shots without changing either
layout or score target, then repeated each in a new process with exactly equal
complete final-state JSON:

- Follow: yaw -1.6127963267948966, power 160, tip (0,+0.25), 1325 ticks,
  score 88; cue final (-7.2555957,-29.2528934) cm.
- Draw: yaw -1.6167963267948966, power 159, tip (0,-0.4), 1196 ticks,
  score 93; cue final (5.8564920,28.3677177) cm.

Evidence directories are `/tmp/billiardgl-follow-shipping-real-20260907`,
`/tmp/billiardgl-follow-shipping-repeat-20260907`,
`/tmp/billiardgl-draw-shipping-refine-20260907` and
`/tmp/billiardgl-draw-shipping-repeat-20260907`. Their reports bind candidate
manifest and executable hashes. These are public-automation real simulation
results, not human play verification or yet the final recorded replay files.

A bounded current-production rules review identified a real cross-branch
defect: an eight-ball off-table break is adjudicated as a loss but can still
create a break-foul offer, permitting a rerack to undo the terminal state.
Terminal guards and the real D6 regression are now implemented. Root rebuilt
the three affected targets and independently passed all three CTests in 2.81
seconds (table-departure observation, rules-runtime integration, product shell).
Terminal adjudication suppresses all decision offers; submitting a retained
break-foul offer after completion returns `game_over` without mutation. The existing
17(e) non-eight/cue off-table policy is otherwise implemented; a disabled
cue-elevation capability is not treated as an enabled jump feature or grounds
for adding a new physics mode.

Shipping cut-break calibration also succeeds with unchanged layout/scoring:
yaw -1.5450235670294887, power 178, centered tip, 927 ticks, score 84.
Canonical trace facts establish first contact with ball 1 and cushion contacts
by two object balls (2 and 11); cue finishes at (11.2664814,65.8444748) cm.
The winner records in `/tmp/billiardgl-cut-break-shipping-fine-20260907.json`
and `/tmp/billiardgl-cut-break-shipping-repeat-20260907.json` compare exactly
equal in fresh processes. A subsequent search-harness review discovered that
consecutive `training_load` calls may retain non-GameState runtime lifecycle
data. The other seven single-shot search winners are therefore withdrawn to
candidate status pending independent-process/controller verification: only the
three root fresh-process results above are currently confirmed. The developer
is checking whether actual product retry/load has the same lifecycle defect.
The two multi-ball routes and bound reference-replay integration remain in
progress. Automated success does not assert human play.

Root then independently rechecked all seven withdrawn candidates, starting a
fresh executable process for each. All seven succeeded with the original
ticks/scores, using actual canonical trace facts. Evidence is
`/tmp/billiardgl-seven-training-fresh-20260907.json`, including scenario and
binary hashes. Thus ten exercises again have isolated real successful actions;
the suspected consecutive-load lifecycle issue remains under diagnosis and
this does not replace final Controller/Recorder reference verification.

The two-ball route now also passes an independent root fresh-process replay
of both semantic shots: yaw/power (-1.9334165703363158,200), then
(-2.2174037217455727,160), both centered tip. Score is 94 at tick 2840,
actual pocket order [1,2]; both complete stopped-state snapshots exactly equal
the developer's clean search winner. Evidence:
`/tmp/billiardgl-two-ball-root-fresh-20260907.json`. Eleven of twelve exercises
now have isolated real successful actions; the three-ball route remains.

The final three-ball route also passes root fresh-process verification:
yaw/power (-2.0969540466522996,100), (1.3450324790464807,200),
(1.2116120197536073,180), centered tips. Shot-end ticks are 585/1988/2185,
actual pocket order [3,4,5], score 71. All three stopped-state snapshots exactly
equal the clean search winner. Evidence:
`/tmp/billiardgl-three-ball-root-fresh-20260907.json`. All twelve exercises now
have isolated actual shipping successes; binding and validating their Recorder
references, final product integration and human play verification remain.

Scenario-load lifecycle isolation is now independently root-verified after a
fresh rebuild: GameRuntimeTests and Phase4ProductControllerTests both pass
(0.21/0.38 seconds). Cue-tip observation is staged locally before commit;
failed replacement preserves old authority, while successful replacement clears
obsolete observers and advances/clears the semantic action journal. Tests
compare A-to-B replacement against clean B at every tick. This fixes observed
residual authority but is not asserted as the cause of unisolated search drift.

The jaw/apron refinement remains unpromoted after root review of all three
images: the body top still visibly obstructs pocket cavities and the jaws have
isolated pointed wings. Technical art is diagnosing intersecting body geometry
and implementing actual openings within the existing six pocket envelopes,
without changing physics, cloth targets or renderer.

## Implemented and independently exercised

- Contact solver position projections now report ordered **actual float-state
  before/after mutations**, including iteration, contact and participant IDs.
  Rounded-away requested corrections produce no mutation receipt.
- Existing solver-event telemetry contains these receipts and their tick time.
  The composed trajectory validator consumes exact chains and reconciles each
  participant's accumulated correction with its contact record. Missing whole
  receipt bundles are rejected even when the displacement is below penetration
  slop. These are additive in-memory fields; existing fields and the six
  top-level evidence ledgers remain intact.
- Zero-duration legacy handoffs use the actual event/island solver, not the
  older discrete fallback that had bypassed these receipts.
- The private-consumer runtime test exercises 104 positive/negative checks at
  both O0 and O2. Contact-solver tests cover actual projections and sub-ULP
  requested corrections with no actual movement.
- D6 query/advance/resolve are separate APIs. Cross-contact queries accept
  actual piecewise analytic trajectories, including legacy surface motion,
  rather than assuming constant horizontal speed. Supported stop state is
  canonicalized to exact zero before its stationary tail is recorded.
- The independent D6 observer/adapter accepts real producer trajectories and
  distinguishes private events from analytic stop transitions. A pocket entry
  is not incorrectly treated as an authoritative capture.

## Integration in progress — not a release claim

Earlier full original `scripts/check.sh` run built successfully but stopped at
CTest: 109/125 passed, 16 failed. The subsequent standalone Python physics
validation/release stages were not executed. This is **not** a green release.
The failing set included physics instrumentation, ordinary equivalence,
full-game stress/performance/component/gameplay/equivalence, two glTF tests,
two UI preflights, first-shot E2E, full AI match, physics validation/release,
and an intermittent screenshot crash.

Newer targeted closure run: **10/10 passed** in 6.68 seconds — PhysicsTests,
CueContactTests, exact OrdinaryShotEquivalence, FullGameStress, FullGamePerformance,
FullGameComponentCases, FullGameGameplayCases, FullGameEquivalence,
PhysicsValidationE2E and Phase3PhysicsReleaseGate. The old profile's vertical
cushion component no longer implicitly opts its planar state into D6; only the
shipping profile or an explicitly noncloth domain enables that scheduler.
The frozen Phase 3 convergence-contract test was restored byte-for-byte
(`b439f5a6…d777`), with all added canonical-tip interval assertions moved to
the current CueContactTests and freshly executed. No frozen assessment,
rejection, golden result, or physics parameter was changed. The historical
candidate remains REJECTED, not promoted.

The subsequent complete original check built successfully and reached
**122/126 passing CTests in 108.06 seconds**. Four failures remained:
GameRuntimeTableDepartureObservation, TableDepartureGeometry (a legacy-profile
fixture still expecting shipping support), AiRulesRuntimeAcceptance (the chosen
eight-ball shot actually missed without a rail), and AiTurnController (a small
mixed-domain overlap rejected at the next swept-query boundary).
The full production AI match passed in the root build (8.61 seconds).
The script still stopped at CTest; its later stages did not run.

Follow-up root physics changes now produce actual zero-time solver projection
receipts for separating mixed-domain overlaps before the swept handoff, using
the same float-storage lower bound as the query. The shared helper has a strict
next-float rejection boundary, not an enlarged physical penetration tolerance.
Fresh PhysicsTests and AiTurnControllerTests pass, including the new real
zero-impulse correction regression and native weak-break decision flow.
An airborne nose contact now explicitly uses radial sphere/plane contact
geometry; the old elevated supported-cushion arm had let spin falsely classify
an approaching ball as separating. The legacy geometry remains unchanged.
The root Release rebuild now passes all nine targeted tests (10.14 seconds):
runtime ordinary/eight/cue off-table observation, PhysicsTests, AI planner,
physical acceptance, rules-runtime acceptance, full-match acceptance, async
planner, preview evaluator and turn controller. This includes the real
zero-time projection and over-limit atomic-rollback negative regression.

AI candidates are now scored after applying their seeded execution error, and
the actual preview facts must show the intended first contact plus a pot or
subsequent rail. The chosen action is the same action previewed. Developer O0/O2
runs each complete a legal 15-shot match; repeated same-build execution agrees.
Cross-optimization visual-rotation state hashes are not claimed identical.
The root full-match acceptance test independently passes in 5.01 seconds.

RenderedAutomationE2E passed, but BreakCameraE2E initially failed for a 200-power
shot. A minimal rendered reproduction identified cue ball tick 52 entering
rail-top geometry below its supported plane. This is a separate swept rail-edge
collision defect, not the repaired spin/nose contact or mixed-ball overlap.
It is now repaired with a swept quarter-circle upper inner rail edge and an
explicit `RoundedUpperRailEdge` radial 3D contact type. The original vertical
nose rejects oblique normals as before; only genuine rounded-edge candidates
select the new factory. Root ContactSolverTests cover four sides, passive
impulse, exact repeated solution and invalid endpoint/downward/nonunit normals.
Fresh root RuntimeDeparture, Geometry, Phase3Release and rendered BreakCamera
all pass (4/4, 3.94 seconds). BreakCamera completes the actual 200-power shot
through all balls stopping, including live camera input. No penetration
allowance, positional clamp, physics parameter or frozen golden was changed.
An independent root Debug build also passes ContactSolver, Geometry and
RuntimeDeparture (3/3, 7.91 seconds), including the actual rounded contact's
passive impulse and repeated contact identity/normal/velocity evidence.

ModernProductFirstShotEvidenceE2E also passed freshly after the real-stroke
classification fix. A new complete original check remains required. Remaining
replay and visual defects still preclude M4 completion.

Follow-up verification repaired the glTF count/semantic checks (each repeated
five times), both preflight command arguments, and exact frozen ordinary-shot
equivalence without rewriting its 195557-byte baseline. The shipping interactive
physics profile opts into coupled support; preserved planar evidence profiles
keep their original path unless an explicit noncloth domain requires the
composed scheduler. Their frozen full-game tests now pass as listed above.

The screenshot crash report showed detached legacy audio initialization racing
static audio-resource destruction during offline screenshot exit. Initialization
now queues SDL_mixer playback on the main thread, only for interactive launches;
the mixer itself remains asynchronous. Five fresh post-fix ScreenshotDefault
repetitions passed. Broader lifecycle and whole-suite verification remain due.

Fixed AI work caps and cooperative cancellation have targeted O0/O2 and latency
evidence. A new two-turn recording test captures ten real public AI actions and
replays them through ReplayTransport to the exact final tick/hash without
mutating live authority. Developer O0/O2 pass.
Replay v1 does not represent revisioned rule-decision offers across a complete
match; this bounded test does not claim that extension. The root rebuilt
AiTurnControllerTests and it passes (2.26 seconds), alongside both automation
controller/protocol tests. Missing generation
arguments in discrete free-motion telemetry are repaired and covered by a
regression, and all three runtime off-table variants now pass in the root build.

The first current six-scene 1080p Medium capture completed 6/6 at
`/tmp/billiardgl-m4-six-scene-before-pocket-20260907/report.json`.
Its CPU frame P95 range is 2.410–4.065 ms (120 warmup, 300 measured frames).
This is rendered automation, not the full product HUD. The render-performance
callback now resizes to the requested drawable before measuring, reports its
actual width/height and exports P50/P95/P99 plus pass breakdowns. Root review
also found GPU sample availability was falsely created by a map insertion and
cached values were counted repeatedly. The correction is freshly built and
ModernRendererProductionE2E passes its 1080p resolution, sampling schema,
percentile-order and unchanged-runtime assertions. ModernApplicationE2E also
passes. The original bundle's GPU pass percentiles are superseded, not accepted.
The replacement `/tmp/billiardgl-m4-six-scene-timing-fixed-20260907/report.json`
completes 6/6 with actual new GPU queries (298–300 per pass), CPU frame P95
2.249–2.771 ms. Close-up visual review then exposed that the historical pocket
camera missed its intended subject. The current runner now uses explicit
public-camera recipes targeting the corner and cue ball; historical registries
and goldens remain unchanged. Those corrected views need final paired captures
after the pocket asset repair.
Ball/pocket visual review is still open. The camera projection and reflection
uniform now share eye-plus-projection-pan; zero-pan captures are unchanged, so
this concrete fix is not claimed to explain all existing highlight appearance.

Replay review identified two actual unclosed product defects: final-hash drift
was not automatically reported at playback completion, and a cold-start browser
could not bind a previously saved training replay to its trusted content.
These are under implementation with negative and product-entry tests. A
Replay-local versioned D6 authority digest will supplement (not change) the
frozen global state hash for newly recorded files; existing valid v1 files keep
their original hash contract. Complete match referee-decision replay remains
outside this bounded change.

Standalone full Python validation initially exposed a test that called
`write_v5_candidate(ROOT)`, rewriting the frozen inventory with current source
hashes. The test now generates in an isolated temporary tree and separately
verifies protected inputs; historical source entries are checked against their
actual selected revision. The two rewritten inventory hashes were restored;
its SHA is again `87bfe894…caf4`, with no physics-model diff. Fresh full Python
validation passes **414/414** in 36.209 seconds. Standalone scenario validation
exits successfully with **6 passed, 2 pre-existing known model mismatches,
0 new failures**. No known-failure entry was added or changed.

Replay fixes now have developer O0/O2 6/6 evidence, including terminal mismatch,
cold-start trusted-content loading, legacy base-only compatibility, and new D6
digest negative cases. Root has started a new complete original check with
these changes; whole-suite results are pending.
Root review additionally caught failed replay replacement mutating the active
Player before returning an error. Loading is now staged in a candidate runtime;
developer O0/O2 regressions verify that a bad tick-0 endpoint leaves the old
replay's tick/hash, playback controls, camera and trajectory intact and playable.
That final source delta is included in the ongoing integration check.

First-shot adjudication errors are no longer silently consumed. Follow-up code
now supplies the actual native non-miscue cue-contact result privately from
`finishRulesShot`, allowing a real underpowered no-rack-contact break to complete
its foul adjudication. The fresh RulesRuntimeIntegration suite passes, including
that real weak shot and unchanged refusal of ambiguous imported ShotFacts.
Errors lacking sufficient evidence remain retained until reset. First-shot
product E2E is being rebuilt; no whole-product result is inferred yet.

Fresh PhysicsTests pass after making coupled-support fixtures explicitly use
the shipping profile and clearing uncommitted canonical telemetry on legacy
failed-step rollback. The new generation regression verifies nonzero post-reset
lineage even on discrete free motion. PhysicsInstrumentationTests also pass:
fixtures now use valid 0.1 cm initial overlap rather than exceeding the solver's
maximum penetration, and an elastic energy comparison admits only 16 double
machine epsilons of relative rounding (observed excess 1.6e-18 J), not a physical
energy increase. OrdinaryShotEquivalence passed again after retaining the old
planar profile's original handling of downward surface velocity.

Technical-art component captures isolate the remaining ball brightness band to
environment specular reflection: unlit albedo and normal views are continuous,
and changing finite direct-light sampling does not change the isolated reflected
images. Cubemap seamless-filter A/B did not remove the band and was not promoted.
Specular-map direction/LOD investigation continues. Neither the finite-light
diagnostic nor a new material/exposure adjustment has been promoted by this test.

Latest integration delta: GameRuntime now starts, consumes and finishes the
native D6 observer for production Chinese-eight-ball shots. Incomplete native
evidence fails closed instead of manufacturing off-table facts. The tick-21
`table_departure_adapter_event_witness_incomplete` failure is fixed by exact
half-open analytic segment ownership and typed immediate support release.
An independently built Release run completed 31 shots / 17190 ticks with a
non-foul eight-ball win for player 1. Called-eight was disabled in that run;
this does not verify the called-pocket-required variant. Earlier adapter-only
break checks did not cover the persistent observer path. Full production
AiPlayerController decision integration is being verified separately.

Rules-adjudicated off-table removal now has a separate `BallState.outOfPlay`
flag, not a fabricated pocket capture. Placement, group counts, rendering,
scenario input and automation export are being integrated with this state.
Full Release rebuild is in progress after fixing one aggregate initializer;
the five focused test executables pass, but not all were freshly rebuilt yet.
No broad release result is inferred from that interim run.

The terminal-aligned pocket candidate has been independently viewed in low,
elevated and overhead cameras and approved for a two-file asset promotion with
recoverable backup. This is a bounded geometry improvement, not complete
visual acceptance. The mother-ball brightness band remains visibly unresolved.

The two-file pocket promotion is now applied. Dynamic Help now derives its
seven lines from actual ProductSettings bindings; targeted Debug/Release CPU
tests and independent GPU micro/extreme-DPI rebound captures pass. Root also
viewed the long GamepadRightStick binding at 144 DPI / UI 135% without clipping.

One full Release build reached 100% after fixing standalone hash and trajectory
inset UI target link dependencies. Subsequent physics/runtime changes require
another incremental build before any whole-suite claim. The production AI
controller now handles actor-matched decisions and retained ball-in-hand;
shipping break powers expose a newer tick-107 zero-duration support-to-landing
witness case. A native pre-resolution velocity witness now closes that case;
the actual product-controller test progresses to its 96-shot bound but has
not yet produced a terminal result. AI selection/stagnation is being diagnosed.
Real pending landing Support contacts are retained even for
separating velocities within the event batch; the adapter still rejects an
unresolved witness. Full product-controller acceptance remains pending.

Shot-end adjudication errors are now returned, retained and journaled instead
of silently discarded. A real weak-break regression verifies the diagnostic,
prevents overwriting the unresolved shot, and verifies explicit reset recovery;
fresh RulesRuntimeIntegrationTests pass. Fresh PhysicsTests also pass a tiny
airborne return requiring a real linked Support solver receipt.

Off-table removal is also excluded from preview-visible sample positions and
is preserved as optional boolean `out_of_play` in Replay v1 initial scenarios.
Absent means false for existing files; a non-boolean value or simultaneous
`pocketed=true` is rejected. Fresh ReplayRecorderTests pass, including optional
field parsing/serialization and invalid values, alongside ProductHudTests,
ProductUiFrameCoordinatorTests and ProductUiGpuRendererTests (4/4).
The 104 private-lineage checks pass again at O0/O2, and fresh Release
SimultaneousLandingReleaseTests, AiPhysicalAcceptanceTests and
AiRulesRuntimeAcceptanceTests pass (3/3). Their scopes remain as described
above; they do not substitute for whole-product acceptance.

`runComposedEventScheduler` replaces the full-tick probe and separately replayed
tails with earliest-event advancement and fresh per-ball ownership. Coupled
cross-contact candidates use the existing island solver, producing canonical
contact and projection evidence.

The focused Release checkpoint now passes **PhysicsTests, ContactSolverTests,
PhysicsTelemetryTests, InteractivePhysicsTests and
PhysicsTelemetryExactHelperTests (5/5)**.
The full 120 Hz break runs to rest and exactly replays every tick's state and
complete telemetry. A dedicated initial-overlap fixture guarantees real t=0
projection coverage instead of requiring incidental penetration during a rack.
One local run completed 367 ticks (3.05833 simulated seconds); measured physics
tick P50/P95/P99 were 17.084/27.583/218.375 microseconds, excluding oracle replay.
These are local CPU measurements, not GPU frame-time or product acceptance.
Capture/reset generations and authoritative reset receipts are connected;
the high-speed cue scratch regression also passes.
The scheduler now preserves typed departure-event evidence and uses the frozen
queried legacy event instead of recalculating its time with a shorter horizon.
Downward supported velocity now produces a real Support contact rather than a
silent clamp. Landing intent preserves incoming velocity for a shared support /
ball-contact island; a simultaneous landing-plus-ball fixture verifies nonzero
support impulse and common solver event/island. Supports are scoped to event
participants and their touching-ball closure. A real immediate post-landing
release fixture now verifies positive vertical velocity and an actual shared
Support/BallBall event, including event-budget rollback. The canonical rules
bridge still needs whole-match review and runtime integration.
The incorrect all-D6
manual-tail oracle has been replaced by exact public per-tick replay; production
performance measurements exclude the oracle's extra execution time.

RulesRuntimeIntegrationTests exposed stale PocketChannel state after a rerack.
Rerack, respot and ball-in-hand placement now restore ClothSupported explicitly.
The refreshed rules runtime, Chinese-eight-ball rules, settings runtime and
AI parameter-distribution tests pass (4/4). The AI parameter test is only a yaw
error distribution check; it is not evidence of actual pot or match win rates.

The preview candidate generator now uses the physical pocket frames (including
the actual side-pocket positions and IDs), not a separate incorrect six-point
array. Its refreshed Release evaluator test passes. Automation export now
distinguishes support contacts and includes ordered actual projection receipts
and capture/reset generations; full-width integer export tests pass.

The fixed-candidate physical AI test now uses the shipping
`chinese_pool_interactive_120hz_v5` profile. O0/O2 agree: Rookie, Club and Expert
complete 48/48 shots each, with 10/13/17 cue-safe pots respectively. This measures
execution-error differences for a fixed ideal candidate, not complete candidate
selection, rule legality or match win rates. A separate real AI-controller
endgame test passes in root Release: public shot actions produce actual physics
facts and runtime-adjudicated foul/turn transfer or legal called-eight victory.
A full initial-rack-to-terminal-match test is being built; no full-match claim
is made yet.

Two production lifecycle defects were reproduced with failing regressions and
fixed in the composed scheduler. Airborne nose contact now uses the real cushion
island impulse and binds its private event to the canonical Rail contact.
Vertical pocket entry no longer freezes at the mouth: its accepted open shaft
advances under gravity, crosses the cloth-plane throat, and captures only when
the entire sphere clears the cloth. Cue/object × corner/side tests verify later
tick capture and actual cue reset generations. Vertical topology records are
explicitly tagged `pocketVerticalShaft`; horizontal pocket depth is not confused
with downward plane distances. This simplified shaft model is not a detailed
leather/net collision simulation. Cross-tick observer binding remains in work.

The Help panel is content-sized from its seven rendered lines. Targeted GPU
micro capture completed 6/6 frames with all sampled machine checks satisfied;
the root independently inspected the Help image. The report is
`/private/tmp/billiardgl-help-layout-micro/phase3-gpu-capture-micro-v1.json`.
The report script now supports external build directories and explicit asset
provenance; 21 Python tests pass.
This targeted UI check does not accept the remaining ball/pocket materials,
geometry, full six-scene visual suite, or whole-product performance.
The root also corrected mesh reflection view direction to use the actual camera
and removed the ball-only overbright boost. Fresh micro capture is available at
`/private/tmp/billiardgl-ball-lighting-micro/phase3-gpu-capture-micro-v1.json`.
Ball color separation improves, but remaining shading bands and pocket shape
still require visual review. Smooth-pocket and darker-liner asset candidates
remain separate; the default asset package has not been replaced.

Useful verification commands (run from the repository root):

```sh
python3 tests/test_m4_d6_a_private_lineage_consumer_runtime.py
cmake --build /tmp/billiardgl-v19-closure-release.UFVB27 --target BilliardsInteractivePhysicsTests BilliardsPhysicsTests -j 2
ctest --test-dir /tmp/billiardgl-v19-closure-release.UFVB27 -R '^(BilliardsInteractivePhysicsTests|BilliardsPhysicsTests)$' --output-on-failure
```

Fresh full checks, physics release/equivalence checks, actual product play and
the original visual/performance acceptance remain required before M4 is done.
