# Phase 3 v2 Joint Contact Solver Implementation Plan

**Execution status: Completed and archived.** Unchecked boxes below preserve the original execution script; current disposition and evidence are indexed in [Phase 3 current status](../../phase3-physics-current-status.md).

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Solve every same-TOI physical contact jointly with friction and rotation, order topology transitions deterministically, and roll back the complete tick on any safety failure.

**Architecture:** Represent earliest events as a deterministic batch containing physical constraints and topology transitions. Build islands by shared ball identity across ball-ball, straight-rail, and jaw constraints; solve accumulated normal/tangent impulses and positional projection; commit transitions only after convergence inside a full-state transaction.

**Tech Stack:** C++17, existing fixed-step runtime, projected sequential impulses, CMake/CTest, Python stress harness.

## Global Constraints

- Stable ordering is `(event kind, first ball, second ball, feature ID)` and never pointer/insertion order.
- Tangential impulse obeys the accumulated cone `abs(lambda_t) <= mu * lambda_n`.
- Energy means translational plus rotational kinetic energy.
- No solver-limit path may return partially mutated state or call the discrete fallback.
- Tick time/counters do not advance after failure.
- Each task below ends in one independently reviewable commit.

---

### Task 1: Build complete deterministic earliest-event batches

**Files:**
- Modify: `src/Billiards/continuous_collision.h`
- Modify: `src/Billiards/continuous_collision.cpp`
- Modify: `src/Billiards/contact_island.h`
- Modify: `src/Billiards/contact_island.cpp`
- Modify: `tests/continuous_collision_tests.cpp`
- Modify: `tests/contact_island_tests.cpp`

**Interfaces:**
- Produces: `ContinuousEventBatch buildEarliestEventBatch(candidates, toiToleranceSeconds, maximumIslandSize)` containing `physicalIslands`, `topologyTransitions`, and `earliestTimeSeconds`.
- Consumes: swept `BallBall`, `StraightRail`, `Jaw`, `Throat`, and `Capture` candidates.

- [ ] **Step 1: Write failing mixed-contact and ordering tests**

```cpp
std::vector<ContinuousContactCandidate> candidates = {
    candidate(ContinuousContactKind::StraightRail, 0, -1, 4, 0.01),
    candidate(ContinuousContactKind::BallBall, 0, 1, -1, 0.01),
    candidate(ContinuousContactKind::Capture, 1, -1, 3, 0.01),
};
const ContinuousEventBatch batch = buildEarliestEventBatch(candidates, 1e-7, 16);
expect(batch.physicalIslands.size() == 1, "ball and rail share one island");
expect(batch.physicalIslands[0].ballIndices == std::vector<int>({0, 1}),
       "island membership is stable");
expect(batch.topologyTransitions.size() == 1, "capture remains topological");

std::reverse(candidates.begin(), candidates.end());
const ContinuousEventBatch reversed = buildEarliestEventBatch(candidates, 1e-7, 16);
expect(batch == reversed, "candidate insertion order cannot change a batch");
```

- [ ] **Step 2: Build the focused tests and observe missing batch type**

Run: `cmake --build build --target continuous-collision-tests contact-island-tests -j2`

Expected: compile failure for `ContinuousEventBatch` and `buildEarliestEventBatch`.

- [ ] **Step 3: Implement physical/topological partitioning and shared-ball union**

```cpp
bool isPhysicalConstraint(ContinuousContactKind kind) {
    return kind == ContinuousContactKind::BallBall ||
           kind == ContinuousContactKind::StraightRail ||
           kind == ContinuousContactKind::Jaw;
}

std::tuple<int, int, int, int> stableContactKey(
        const ContinuousContactCandidate& value) {
    return {static_cast<int>(value.kind), value.firstBall,
            value.secondBall, value.featureId};
}

struct ContinuousEventBatch {
    double earliestTimeSeconds = 0.0;
    std::vector<ContactIsland> physicalIslands;
    std::vector<ContinuousContactCandidate> topologyTransitions;
    int duplicateCandidatesRemoved = 0;
    bool limitExceeded = false;
};
```

Select all candidates within `toiToleranceSeconds` of the minimum finite non-negative TOI, sort/deduplicate by `stableContactKey`, union physical constraints through every non-negative ball index, and sort islands by their first contact key. Reject contradictory topology transitions for one ball at one TOI by setting a stable batch failure code.

- [ ] **Step 4: Run collision/island tests at two input orderings**

Run: `cmake --build build --target continuous-collision-tests contact-island-tests -j2 && ctest --test-dir build -R 'continuous-collision|contact-island' --output-on-failure`

Expected: all selected tests pass.

- [ ] **Step 5: Commit deterministic event batching**

```bash
git add src/Billiards/continuous_collision.h src/Billiards/continuous_collision.cpp src/Billiards/contact_island.h src/Billiards/contact_island.cpp tests/continuous_collision_tests.cpp tests/contact_island_tests.cpp
git commit -m "feat: batch joint continuous contacts"
```

### Task 2: Solve accumulated tangential impulses and angular response

**Files:**
- Modify: `src/Billiards/contact_solver.h`
- Modify: `src/Billiards/contact_solver.cpp`
- Modify: `tests/contact_solver_tests.cpp`
- Modify: `tests/ball_ball_contact_tests.cpp`
- Modify: `tests/cushion_contact_tests.cpp`

**Interfaces:**
- Extends `ContactImpulseDiagnostic` with signed tangent impulse, contact arms, effective masses, stick/slip regime, and rotational energy.
- Produces: a converged `solveContactIsland(GameState&, const ContactIsland&, const PhysicsProfile&)` that updates linear and angular velocities for ball-ball and boundary contacts.

- [ ] **Step 1: Write failing friction-cone, spin-transfer, and passive-energy tests**

```cpp
GameState state = obliqueBallBallState();
const ContactSolverResult result = solveContactIsland(state, oneContactIsland(), frictionProfile());
expect(result.status == ContactSolverStatus::Converged, "oblique contact converges");
expect(std::abs(result.contacts[0].accumulatedTangentialImpulseNs) <=
       frictionProfile().ball.frictionCoefficient *
       result.contacts[0].accumulatedNormalImpulseNs + 1e-12,
       "accumulated Coulomb cone holds");
expect(length(state.balls[0].angularVelocity) > 0.0f, "friction changes spin");
expect(result.totalKineticEnergyAfterJ <= result.totalKineticEnergyBeforeJ + 1e-10,
       "passive contact cannot create total energy");
```

- [ ] **Step 2: Run solver tests and observe missing tangent/angular behavior**

Run: `cmake --build build --target contact-solver-tests ball-ball-contact-tests cushion-contact-tests -j2 && ctest --test-dir build -R 'contact-solver|ball-ball-contact|cushion-contact' --output-on-failure`

Expected: failures because the current island solver accumulates only normal impulse.

- [ ] **Step 3: Implement contact state and projected normal/tangent sweeps**

```cpp
struct SolverConstraint {
    ContinuousContactCandidate candidate;
    Point3 normal;
    Point3 tangent;
    Point3 firstArmM;
    Point3 secondArmM;
    double targetNormalSpeedMps = 0.0;
    double lambdaNormalNs = 0.0;
    double lambdaTangentNs = 0.0;
    double normalEffectiveMassKg = 0.0;
    double tangentEffectiveMassKg = 0.0;
    double restitution = 0.0;
    double friction = 0.0;
    ContactFrictionRegime regime = ContactFrictionRegime::Sticking;
};

void applyImpulse(BallState& ball, const Point3& impulseNs,
                  const Point3& armM, double inverseMass,
                  double inverseInertia) {
    ball.velocity += impulseNs * static_cast<float>(100.0 * inverseMass);
    ball.angularVelocity += cross(armM, impulseNs) *
                            static_cast<float>(inverseInertia);
}
```

For every forward and reverse sweep: compute relative contact velocity including `omega cross r`; project accumulated normal impulse to `[0, infinity)`; recompute tangent residual; project total signed tangent impulse to `[-mu*lambda_n, +mu*lambda_n]`; apply only the delta impulses. A boundary has zero inverse mass/inertia. Derive a deterministic tangent from relative tangential velocity, falling back to a fixed perpendicular basis when its magnitude is below epsilon.

- [ ] **Step 4: Run solver/contact regressions and multiple time-step energy cases**

Run: `cmake --build build --target contact-solver-tests ball-ball-contact-tests cushion-contact-tests -j2 && ctest --test-dir build -R 'contact-solver|ball-ball-contact|cushion-contact' --output-on-failure`

Expected: all selected tests pass for `dt = 1/60`, `1/120`, and `1/240` seconds.

- [ ] **Step 5: Commit the frictional angular island solver**

```bash
git add src/Billiards/contact_solver.h src/Billiards/contact_solver.cpp tests/contact_solver_tests.cpp tests/ball_ball_contact_tests.cpp tests/cushion_contact_tests.cpp
git commit -m "feat: solve frictional angular contact islands"
```

### Task 3: Generate rail and jaw constraints in the same solver path

**Files:**
- Modify: `src/Billiards/physics.cpp`
- Modify: `src/Billiards/pocket_boundary.cpp`
- Modify: `src/Billiards/contact_solver.cpp`
- Modify: `tests/physics_tests.cpp`
- Modify: `tests/pocket_boundary_tests.cpp`
- Modify: `tests/contact_solver_tests.cpp`

**Interfaces:**
- Consumes: `buildEarliestEventBatch` and cushion/profile material parameters.
- Produces: boundary `SolverConstraint` instances with feature IDs, normals, measured contact arms, speed-dependent restitution hook, and cushion friction.

- [ ] **Step 1: Write failing simultaneous ball-ball/rail and dual-jaw tests**

```cpp
GameState state = ballBallRailSameToiState();
const PhysicsStepTelemetry telemetry = updatePhysics(state, 0.01f, jointProfile());
expect(telemetry.solverEvents.size() == 1, "same TOI forms one solver event");
expect(telemetry.solverEvents[0].contactCount == 2, "ball and rail solve together");
expect(telemetry.contacts[0].solverIslandId == telemetry.contacts[1].solverIslandId,
       "shared ball connects boundary and ball constraints");

GameState jaws = symmetricDualJawState();
const auto first = updatePhysics(jaws, 0.01f, jointProfile());
expect(first.contacts.size() == 2, "both equal-TOI jaws are retained");
```

- [ ] **Step 2: Run physics and pocket tests and observe separate boundary handling**

Run: `cmake --build build --target physics-tests pocket-boundary-tests contact-solver-tests -j2 && ctest --test-dir build -R 'physics-tests|pocket-boundary|contact-solver' --output-on-failure`

Expected: new assertions fail because boundary contacts are resolved outside ball-ball islands.

- [ ] **Step 3: Route every physical candidate through the island solver**

```cpp
std::vector<ContinuousContactCandidate> candidates =
    generateBallBallCandidates(state, remaining, radius, tolerance, true);
append(candidates, generateStraightRailCandidates(state, remaining, profile));
append(candidates, generatePocketJawCandidates(state, remaining, profile));
append(candidates, generateThroatCandidates(state, remaining, profile));
append(candidates, generateCaptureCandidates(state, remaining, profile));
const ContinuousEventBatch batch = buildEarliestEventBatch(
    candidates, profile.solver.toiToleranceSeconds,
    profile.solver.maximumIslandSize);
```

Remove direct rail/jaw impulse application from the event-driven loop. Keep throat/capture candidates uncommitted until every physical island in the batch converges. Emit one `PhysicsContactRecord` per solver constraint with the same event/island identity and its complete normal/tangent/energy diagnostics.

- [ ] **Step 4: Run integrated contact regressions**

Run: `cmake --build build --target physics-tests pocket-boundary-tests contact-solver-tests physics-instrumentation-tests -j2 && ctest --test-dir build -R 'physics-tests|pocket-boundary|contact-solver|physics-instrumentation' --output-on-failure`

Expected: all selected tests pass and no boundary contact uses the old direct impulse path.

- [ ] **Step 5: Commit joint boundary contact solving**

```bash
git add src/Billiards/physics.cpp src/Billiards/pocket_boundary.cpp src/Billiards/contact_solver.cpp tests/physics_tests.cpp tests/pocket_boundary_tests.cpp tests/contact_solver_tests.cpp
git commit -m "feat: solve ball and boundary contacts jointly"
```

### Task 4: Separate positional projection and ordered topology commits

**Files:**
- Modify: `src/Billiards/contact_solver.cpp`
- Modify: `src/Billiards/physics.cpp`
- Modify: `src/Billiards/pocket_state_machine.cpp`
- Modify: `tests/contact_solver_tests.cpp`
- Modify: `tests/pocket_state_machine_tests.cpp`
- Modify: `tests/continuous_collision_tests.cpp`

**Interfaces:**
- Produces: fixed-count geometry projection after velocity convergence and `applyTopologyTransitions(...)` after all physical islands succeed.
- Consumes: current geometry, penetration slop, inverse-mass sharing, capture sequence identity.

- [ ] **Step 1: Write failing projection/topology ordering tests**

```cpp
GameState state = contactAtThroatState();
const auto telemetry = updatePhysics(state, 0.01f, jointProfile());
expect(telemetry.solverEvents[0].failureCode == std::string("converged"),
       "physical solve converges first");
expect(state.pocketInteractions[0].phase == PocketInteractionPhase::InsideThroat,
       "throat transition commits after projection");

const unsigned long long sequence = state.pocketInteractions[0].captureSequence;
updatePhysics(state, 0.01f, jointProfile());
expect(state.pocketInteractions[0].captureSequence == sequence,
       "capture sequence is irreversible and applied once");
```

- [ ] **Step 2: Run solver/pocket tests and observe interleaved topology behavior**

Run: `cmake --build build --target contact-solver-tests pocket-state-machine-tests continuous-collision-tests -j2 && ctest --test-dir build -R 'contact-solver|pocket-state-machine|continuous-collision' --output-on-failure`

Expected: ordering/idempotence assertions fail on the current interleaved path.

- [ ] **Step 3: Implement a two-phase event commit**

```cpp
bool solvePhysicalBatch(GameState& state, const ContinuousEventBatch& batch,
                        const PhysicsProfile& profile,
                        PhysicsStepTelemetry& telemetry);

bool applyTopologyTransitions(
    GameState& state,
    const std::vector<ContinuousContactCandidate>& transitions,
    PhysicsStepTelemetry& telemetry);
```

`solvePhysicalBatch` finishes velocity sweeps, then performs exactly `positionIterations` projections from current geometry with slop and inverse-mass sharing. Only after every island returns `Converged` does `applyTopologyTransitions` process sorted throat then capture events. It rejects two incompatible transitions for the same ball/event and ignores an already committed capture sequence without generating a second event.

- [ ] **Step 4: Run projection, pocket, and full physics tests**

Run: `cmake --build build --target contact-solver-tests pocket-state-machine-tests continuous-collision-tests physics-tests -j2 && ctest --test-dir build -R 'contact-solver|pocket-state-machine|continuous-collision|physics-tests' --output-on-failure`

Expected: all selected tests pass.

- [ ] **Step 5: Commit projection and topology sequencing**

```bash
git add src/Billiards/contact_solver.cpp src/Billiards/physics.cpp src/Billiards/pocket_state_machine.cpp tests/contact_solver_tests.cpp tests/pocket_state_machine_tests.cpp tests/continuous_collision_tests.cpp
git commit -m "fix: commit pocket topology after contact convergence"
```

### Task 5: Make the complete physics tick transactional and observable

**Files:**
- Modify: `src/Billiards/physics_telemetry.h`
- Modify: `src/Billiards/physics_telemetry.cpp`
- Modify: `src/Billiards/physics_profile.h`
- Modify: `src/Billiards/physics_profile.cpp`
- Modify: `src/Billiards/physics.cpp`
- Modify: `src/Billiards/game_runtime.cpp`
- Modify: `src/Billiards/automation_json.cpp`
- Modify: `tests/physics_tests.cpp`
- Modify: `tests/game_runtime_tests.cpp`
- Modify: `tests/physics_telemetry_tests.cpp`
- Modify: `tests/physics_profile_tests.cpp`

**Interfaces:**
- Produces: `PhysicsStepStatus { Succeeded, Failed }`, stable `PhysicsFailureCode`, failing event/island IDs, uncommitted diagnostics, and exact rollback of `GameState` plus runtime tick/time/movement counters.
- Consumes: every solver/batch safety result and configured `passiveEnergyToleranceJ`.

- [ ] **Step 1: Write failing rollback tests for every safety class**

```cpp
for (const FailureFixture& fixture : failureFixtures()) {
    GameRuntime runtime = fixture.runtime;
    const GameRuntime before = runtime;
    const PhysicsStepTelemetry telemetry = runtime.advancePhysics(fixture.timeStep);
    expect(telemetry.stepStatus == PhysicsStepStatus::Failed, fixture.name);
    expect(telemetry.failureCode == fixture.expectedCode, fixture.name);
    expect(runtime.state() == before.state(), fixture.name + " state rollback");
    expect(runtime.tick() == before.tick(), fixture.name + " tick rollback");
    expect(runtime.timeSeconds() == before.timeSeconds(), fixture.name + " time rollback");
}
```

Fixtures cover event budget, island size, residual, hard penetration, non-finite state/impulse/energy/time, passive energy creation, and contradictory topology.

- [ ] **Step 2: Run physics/runtime/telemetry tests and observe partial mutation/fallback**

Run: `cmake --build build --target physics-tests game-runtime-tests physics-telemetry-tests -j2 && ctest --test-dir build -R 'physics-tests|game-runtime|physics-telemetry' --output-on-failure`

Expected: rollback assertions fail because current failure paths retain partial changes or use discrete fallback.

- [ ] **Step 3: Implement snapshot/commit semantics and stable failure telemetry**

```cpp
PhysicsStepTelemetry updatePhysics(GameState& state, float timeStep,
                                   const PhysicsProfile& profile) {
    const GameState snapshot = state;
    PhysicsStepTelemetry telemetry = updatePhysicsEventDriven(state, timeStep, profile);
    if (telemetry.stepStatus == PhysicsStepStatus::Failed) {
        state = snapshot;
    }
    return telemetry;
}
```

Remove event-driven calls to `updatePhysicsDiscrete` used as safety fallback. In `GameRuntime`, snapshot the tick, time, pending movement, pocket interactions, event counters, and gameplay movement flags before invoking physics; advance them only on `Succeeded`. Serialize `step_status`, `failure_code`, `failing_event_id`, `failing_island_id`, and diagnostic energy/residual/penetration values for both outcomes.

Add `double passiveEnergyToleranceJ = 1e-10` to `SolverSettings`, require it to be finite and non-negative in `validatePhysicsProfile`, and use that field for every island/tick energy gate so the tested tolerance is part of the frozen profile.

- [ ] **Step 4: Run C++ regressions and the solver stress harness**

Run: `cmake --build build -j2 && ctest --test-dir build -R 'physics-tests|game-runtime|physics-telemetry|contact-solver|contact-island|pocket' --output-on-failure && python3 tests/test_solver_stress.py build/contact-solver-stress`

Expected: all selected tests pass, every injected failure restores byte-equivalent mutable state, and stress output contains no fallback status.

- [ ] **Step 5: Commit transactional tick failure handling**

```bash
git add src/Billiards/physics_telemetry.h src/Billiards/physics_telemetry.cpp src/Billiards/physics_profile.h src/Billiards/physics_profile.cpp src/Billiards/physics.cpp src/Billiards/game_runtime.cpp src/Billiards/automation_json.cpp tests/physics_tests.cpp tests/game_runtime_tests.cpp tests/physics_telemetry_tests.cpp tests/physics_profile_tests.cpp
git commit -m "fix: roll back failed physics ticks atomically"
```

## Plan Verification

- Run `cmake --build build -j2 && ctest --test-dir build -R 'continuous-collision|contact-island|contact-solver|ball-ball-contact|cushion-contact|pocket|physics-tests|game-runtime|physics-telemetry' --output-on-failure`.
- Run `python3 tests/test_solver_stress.py build/contact-solver-stress`.
- Search `rg -n 'updatePhysicsDiscrete' src/Billiards/physics.cpp` and verify no solver safety failure invokes it.
- Confirm diagnostics report both translational and rotational energy and every failed tick leaves runtime counters unchanged.
