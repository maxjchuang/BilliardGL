# Phase 3 v5 Coupled Cue Contact Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Build `phase3_integrated_v5` with a finite-duration, transactional cue-contact island for frozen shots, preserve byte-equivalent v4 physics for ordinary shots, and promote only after two separately authorized independent confirmations pass.

**Architecture:** `applyCueShot` detects an initial frozen topology and routes only that topology to a dedicated hybrid solver: Hunt--Crossley cue compliance is microstepped while the existing rigid ball/cushion constraints are solved inside the same local island. The solver returns one versioned summary plus a complete microtrace; `GameRuntime` commits successful candidate state atomically and retains diagnostics on rollback. Python governance admits complete calibration and confirmation packages, generates the v5 profile from registered fit outputs, freezes all hashes, and consumes Cross 2016 then Han exactly once.

**Tech Stack:** C++17, existing `GameState`/contact-island solver/automation JSON, CMake and CTest, Python 3 standard library and `unittest`, canonical JSON/CSV, public experimental evidence and Git-bound promotion receipts.

## Global Constraints

- Only shots where the cue ball is already touching a ball or cushion at cue contact use v5; every non-frozen shot executes the unchanged v4 `resolveCueContact` path.
- Excluding candidate/formula identity and new empty diagnostics, non-frozen physical states, event order, rule outcomes, and determinism hashes must equal v4.
- Use `F_n = max(0, k_n * delta^1.5 * (1 + alpha * delta_dot))`; the exponent is fixed at `1.5` and is never fitted.
- Use `F_t_trial = -k_t * xi_t - c_t * v_t` and clamp `|F_t| <= mu_tip * F_n`; project tangential history to the friction cone during slip.
- Cue state is transaction-local and is never added to `GameState` or rendering.
- Elevated cue, jump, masse, and vertical cue impulse remain explicitly unsupported.
- A frozen solve either commits completely or rolls back physics, rule, and event state while retaining failure telemetry; it never silently falls back to v4.
- Stable frozen-contact failures are `nonfinite_state`, `passive_energy_gain`, `compression_limit`, `contact_island_limit`, `cue_contact_no_release`, and `cue_contact_nonconvergence`.
- Alciatore TP A.15 is spent calibration/regression evidence for v5; retain interior RMSE `<= 3 degrees` and maximum absolute error `<= 5 degrees`.
- At the Alciatore endpoints, evaluate speed ratios only; never fabricate a direction for a ball below the stable-speed threshold.
- Cross 2016 and Han 2005 remain unopened until v5 is frozen and each execution receives separate explicit user authorization.
- Commit complete extracted and normalized numbers, hashes, transformation scripts, residuals, sensitivity results, traces, metrics, and receipts. Do not vendor third-party media without a recorded redistribution grant.
- A missing or failed source licence audit blocks promotion rather than deleting numerical evidence needed for verification.
- Every task ends in one focused commit; confirmation tasks stop before reservation until the user explicitly authorizes that transaction.

---

## File Structure

### New C++ components

- `src/Billiards/frozen_cue_topology.{h,cpp}`: deterministic initial-touch graph construction and validation.
- `src/Billiards/coupled_cue_contact.{h,cpp}`: transient cue state, compliant force law, local microsteps, hybrid rigid solve, limits, and rollback result.
- `tests/frozen_cue_topology_tests.cpp`: topology, ordering, cushion, contradiction, and size-limit tests.
- `tests/coupled_cue_contact_tests.cpp`: analytic, passivity, symmetry, convergence, multi-contact, and failure tests.

### Existing C++ files

- `src/Billiards/physics_profile.{h,cpp}`: unit-bearing coupled-contact configuration, validation, and canonical identity.
- `src/Billiards/game_runtime.{h,cpp}`: frozen routing, candidate-state transaction, result retention, and rule commit.
- `src/Billiards/cue_contact.h`: versioned summary and microtrace record types; keep `resolveCueContact` unchanged.
- `src/Billiards/automation_protocol.cpp`: serialize complete summary/microtrace with explicit units.
- `CMakeLists.txt`: compile focused components and register their tests.

### New validation modules and governed assets

- `tools/physics_validation/extract_shimamura_2006_cue_contact.py`: reproduce the published experimental cue--ball contact time/force trace and source audit.
- `tools/physics_validation/fit_frozen_cue_contact.py`: fit identifiable compliance parameters and emit residual/sensitivity artifacts.
- `tools/physics_validation/extract_cross_2016_newtons_cradle.py`: build the independent frozen two-ball confirmation package from the author manuscript and video 4582 measurements.
- `tools/physics_validation/cross_2016_confirmation.py`: scenarios and uncertainty-aware equal-speed metric.
- `tools/physics_validation/build_v5_profile.py`: bind v4 physics plus the new coupled-contact fit into v5 identity and inventory.
- `tools/physics_validation/phase3_v5_assessment.py`: enforce Cross-first/Han-second confirmation and final disposition.
- `tests/physics_validation/reference_data/shimamura_2006_cue_contact/*`: complete digitized trace, source scalars, normalized rows, provenance, audit, and manifest.
- `tests/physics_validation/reference_data/cross_2016_newtons_cradle/*`: complete extracted trajectories/speeds, uncertainty, scenario contract, audit, and manifest.
- `physics_models/calibration/frozen_cue_contact_v1_*`: fit inputs, fit report, residuals, and sensitivity table.
- `physics_models/profiles/chinese_pool_full_game_v5.json` and `src/Billiards/generated/phase3_v5_profile.inc`: selected runtime profile.
- `physics_models/promotion/phase3_candidates_v5.json`: complete v5 inventory.
- Authorized only after freeze: `physics_models/candidates/phase3_integrated_v5/confirmation/*`, ledger, receipts, and final assessment.

---

### Task 1: Add the Unit-Bearing Coupled-Contact Profile Contract

**Files:**
- Modify: `src/Billiards/physics_profile.h`
- Modify: `src/Billiards/physics_profile.cpp`
- Modify: `tests/physics_profile_tests.cpp`
- Modify: `src/Billiards/physics_scenario.cpp`
- Modify: `tests/physics_scenario_tests.cpp`

**Interfaces:**
- Produces `FrozenCueContactProperties` and `PhysicsProfile::frozenCueContact`.
- Produces canonical keys prefixed by `frozen_cue_contact.`.
- Scenario schema version 12 requires the exact `frozen_cue_contact` object;
  schema versions 3--11 retain their existing exact-key contracts.
- Existing v4 generated include uses disabled defaults, so runtime behaviour does not change in this task.

- [ ] **Step 1: Write failing validation and canonicalization tests**

Add assertions that a valid enabled profile round-trips every exact value and invalid limits fail:

```cpp
billiardgl::PhysicsProfile coupled = billiardgl::defaultChinesePoolPhysicsProfile();
coupled.frozenCueContact.enabled = true;
coupled.frozenCueContact.normalStiffnessNPerM32 = 1.25e7;
coupled.frozenCueContact.normalDissipationSPerM = 0.05;
coupled.frozenCueContact.tangentialStiffnessNPerM = 4.0e5;
coupled.frozenCueContact.tangentialDampingNsPerM = 25.0;
coupled.frozenCueContact.microstepSeconds = 0.00001;
coupled.frozenCueContact.maximumContactSeconds = 0.006;
coupled.frozenCueContact.releaseCompressionM = 1e-8;
coupled.frozenCueContact.maximumCompressionM = 0.004;
coupled.frozenCueContact.maximumNormalForceN = 10000.0;
expect(billiardgl::validatePhysicsProfile(coupled).ok,
       "finite ordered frozen-contact controls are valid");
const std::string text = billiardgl::canonicalPhysicsProfileText(coupled);
expect(text.find("frozen_cue_contact.normal_stiffness_n_per_m32=12500000") != std::string::npos,
       "canonical profile binds normal stiffness with units");
coupled.frozenCueContact.microstepSeconds = coupled.frozenCueContact.maximumContactSeconds;
expect(!billiardgl::validatePhysicsProfile(coupled).ok,
       "microstep must be smaller than maximum contact duration");
```

- [ ] **Step 2: Run the focused test and verify it fails**

Run: `cmake --build build --target BilliardsPhysicsProfileTests -j2 && ctest --test-dir build -R BilliardsPhysicsProfileTests --output-on-failure`

Expected: compilation fails because `FrozenCueContactProperties` is absent.

- [ ] **Step 3: Implement the profile structure, validation, and canonical text**

Add this exact structure with disabled defaults:

```cpp
struct FrozenCueContactProperties {
    bool enabled = false;
    double normalStiffnessNPerM32 = 1.25e7;
    double normalDissipationSPerM = 0.05;
    double tangentialStiffnessNPerM = 4.0e5;
    double tangentialDampingNsPerM = 25.0;
    double microstepSeconds = 0.00001;
    double maximumContactSeconds = 0.006;
    double releaseCompressionM = 1e-8;
    double maximumCompressionM = 0.004;
    double maximumNormalForceN = 10000.0;
};
```

Validate every value as finite and nonnegative, require positive stiffness,
microstep, duration, compression and force limits, require
`microstepSeconds < maximumContactSeconds`, and require
`releaseCompressionM < maximumCompressionM`. Append all ten fields to
`canonicalPhysicsProfileText`; serialize the boolean as `0` or `1`.

Extend `parsePhysicsProfile` for schema version 12 with the exact keys
`enabled`, `normal_stiffness_n_per_m32`, `normal_dissipation_s_per_m`,
`tangential_stiffness_n_per_m`, `tangential_damping_ns_per_m`,
`microstep_seconds`, `maximum_contact_seconds`, `release_compression_m`,
`maximum_compression_m`, and `maximum_normal_force_n`. Add tests proving a
complete v12 profile parses and a missing or extra field is rejected. Keep
every older schema branch compatible.

- [ ] **Step 4: Run the profile tests**

Run: `cmake --build build --target BilliardsPhysicsProfileTests BilliardsPhysicsScenarioTests -j2 && ctest --test-dir build -R 'BilliardsPhysics(Profile|Scenario)Tests' --output-on-failure`

Expected: `100% tests passed, 0 tests failed`.

- [ ] **Step 5: Commit**

```bash
git add src/Billiards/physics_profile.h src/Billiards/physics_profile.cpp tests/physics_profile_tests.cpp src/Billiards/physics_scenario.cpp tests/physics_scenario_tests.cpp
git commit -m "feat: define frozen cue contact profile contract"
```

---

### Task 2: Build Deterministic Frozen Topology Without Changing Ordinary Shots

**Files:**
- Create: `src/Billiards/frozen_cue_topology.h`
- Create: `src/Billiards/frozen_cue_topology.cpp`
- Create: `tests/frozen_cue_topology_tests.cpp`
- Modify: `CMakeLists.txt`

**Interfaces:**
- Produces `FrozenCueTopology detectFrozenCueTopology(const GameState&, int, const PhysicsProfile&, PhysicsBoundaryMode)`.
- `FrozenCueTopology` contains `status`, `frozen`, `ContactIsland island`, and `std::string error`.
- Ball indices and contacts are sorted canonically; `status == Valid` with `frozen == false` is the ordinary route.

- [ ] **Step 1: Add failing single-, multi-, cushion-, permutation-, and limit tests**

Use one helper that pockets unused balls, places the cue ball at the origin,
and uses `2 * profile.ball.radiusCm` for exact touching. Assert:

```cpp
const auto ordinary = detectFrozenCueTopology(state, 0, profile,
    PhysicsBoundaryMode::Unbounded);
expect(ordinary.status == FrozenCueTopologyStatus::Valid && !ordinary.frozen,
       "separated balls select the v4 path");
state.balls[1].pocketed = false;
state.balls[1].position.x = 2.0f * profile.ball.radiusCm;
const auto pair = detectFrozenCueTopology(state, 0, profile,
    PhysicsBoundaryMode::Unbounded);
expect(pair.frozen && pair.island.ballIndices == std::vector<int>({0, 1}),
       "one frozen neighbour forms a two-ball island");
```

Add a chain `0-1-2`, shuffle the physical ball placement order, and assert the
same sorted contacts. Set `maximumIslandSize = 2` and assert
`LimitExceeded/contact_island_limit`. Place a ball at the cue-ball center and
assert `ContradictoryTopology`. Place the cue ball tangent to a production
rail and assert a rail candidate is present.

- [ ] **Step 2: Run and verify failure**

Run: `cmake -S . -B build && cmake --build build --target BilliardsFrozenCueTopologyTests -j2`

Expected: CMake has no `BilliardsFrozenCueTopologyTests` target.

- [ ] **Step 3: Implement topology construction**

Define:

```cpp
enum class FrozenCueTopologyStatus { Valid, IslandLimit, ContradictoryTopology };
struct FrozenCueTopology {
    FrozenCueTopologyStatus status = FrozenCueTopologyStatus::Valid;
    bool frozen = false;
    ContactIsland island;
    std::string error;
};
```

Use a breadth-first scan from the cue-ball index. A ball edge is active when
`abs(distance - 2 * radius) <= max(1e-6, solver.penetrationSlopCm)`;
an overlap deeper than `solver.maximumPenetrationCm` is contradictory. Reuse
the production table boundary geometry to add tangent rail contacts. Sort
ball indices ascending and contacts by `(kind, firstBall, secondBall,
featureId)`. Stop before mutation when the unique ball count exceeds
`maximumIslandSize`.

Register both new source and test target in `CMakeLists.txt`.

- [ ] **Step 4: Run topology and existing island tests**

Run: `cmake --build build --target BilliardsFrozenCueTopologyTests BilliardsContactIslandTests -j2 && ctest --test-dir build -R 'Billiards(FrozenCueTopology|ContactIsland)Tests' --output-on-failure`

Expected: both tests pass.

- [ ] **Step 5: Commit**

```bash
git add CMakeLists.txt src/Billiards/frozen_cue_topology.h src/Billiards/frozen_cue_topology.cpp tests/frozen_cue_topology_tests.cpp
git commit -m "feat: detect deterministic frozen cue topology"
```

---

### Task 3: Implement the Single-Contact Hunt--Crossley Microstep Solver

**Files:**
- Create: `src/Billiards/coupled_cue_contact.h`
- Create: `src/Billiards/coupled_cue_contact.cpp`
- Create: `tests/coupled_cue_contact_tests.cpp`
- Modify: `src/Billiards/cue_contact.h`
- Modify: `CMakeLists.txt`

**Interfaces:**
- Produces `CoupledCueContactResult solveCoupledCueContact(const GameState&, const FrozenCueTopology&, const CueImpactInput&, const PhysicsProfile&)`.
- `CoupledCueContactResult` owns `GameState state`, `CueContactResult contact`, `CoupledCueContactStatus status`, and `std::string error`; `contact.microsteps` owns the trace.
- The input state is const; only `result.state` can later be committed.

- [ ] **Step 1: Write failing analytic, friction, passivity, release, and determinism tests**

Construct a two-ball topology but temporarily pocket ball 1 after topology
creation so the component test isolates cue--cue-ball force. Assert normal
force follows the exact formula at a supplied compression/rate, tangential
force never exceeds `mu * normalForceN`, zero damping stays within
`1e-8 J`, positive damping does not create energy, left/right offsets mirror
spin, the result releases within `maximumContactSeconds`, and two result
microstep vectors compare field-for-field equal.

Expose a pure helper for the formula test:

```cpp
const double force = huntCrossleyNormalForce(
    0.001, 0.2, 1.25e7, 0.05);
expect(close(force, 1.25e7 * std::pow(0.001, 1.5) * 1.01, 1e-10),
       "Hunt-Crossley force uses the fixed 1.5 exponent");
expect(huntCrossleyNormalForce(0.001, -1000.0, 1.25e7, 0.05) == 0.0,
       "dissipation never creates attraction");
```

- [ ] **Step 2: Run and verify failure**

Run: `cmake -S . -B build && cmake --build build --target BilliardsCoupledCueContactTests -j2`

Expected: target or header is absent.

- [ ] **Step 3: Define transient state and complete microstep records**

Add `CueContactRegime::Released` and these self-contained unit-bearing records
to `cue_contact.h`; include `<vector>`. They deliberately use `Point3` and do
not depend on `physics_telemetry.h`, which already includes `cue_contact.h`:

```cpp
struct CueContactBallSample {
    int index = -1;
    Point3 positionCm;
    Point3 velocityCmS;
    Point3 accelerationCmS2;
    Point3 angularVelocityRadS;
};

struct CueContactConstraintSample {
    int kind = 0;
    int firstBall = -1;
    int secondBall = -1;
    int featureId = -1;
    Point3 normal;
    double normalImpulseNs = 0.0;
    double tangentialImpulseNs = 0.0;
    double penetrationCm = 0.0;
    double residualCmS = 0.0;
};

struct CueContactMicrostep {
    int index = 0;
    double timeSeconds = 0.0;
    double cuePositionM = 0.0;
    double cueVelocityMS = 0.0;
    double cueAccelerationMS2 = 0.0;
    double compressionM = 0.0;
    double compressionRateMS = 0.0;
    double normalForceN = 0.0;
    double tangentialForceN = 0.0;
    double normalImpulseNs = 0.0;
    double tangentialImpulseNs = 0.0;
    double kineticEnergyJ = 0.0;
    double elasticEnergyJ = 0.0;
    double dissipatedEnergyJ = 0.0;
    double energyResidualJ = 0.0;
    double maximumPenetrationCm = 0.0;
    double solverResidualCmS = 0.0;
    int solverIterations = 0;
    CueContactRegime regime = CueContactRegime::Stick;
    std::array<CueContactBallSample, kBallCount> balls;
    std::vector<CueContactConstraintSample> contacts;
};
```

Append `int microtraceSchemaVersion = 1` and
`std::vector<CueContactMicrostep> microsteps` to `CueContactResult`.

- [ ] **Step 4: Implement symplectic microsteps and release**

Normalize the horizontal cue direction, initialize the cue effective mass and
tip position at zero compression, and for each fixed microstep:

```cpp
const double fn = huntCrossleyNormalForce(delta, deltaDot,
    settings.normalStiffnessNPerM32, settings.normalDissipationSPerM);
const double ftTrial = -settings.tangentialStiffnessNPerM * tangentHistoryM
                     - settings.tangentialDampingNsPerM * tangentSpeedMS;
const double ft = std::clamp(ftTrial, -friction * fn, friction * fn);
if (std::fabs(ftTrial) > friction * fn && settings.tangentialStiffnessNPerM > 0.0)
    tangentHistoryM = -(ft + settings.tangentialDampingNsPerM * tangentSpeedMS)
                    / settings.tangentialStiffnessNPerM;
```

Apply equal/opposite cue and cue-ball impulses, update ball angular velocity
from the contact arm, record all energies, and release only after compression
falls below `releaseCompressionM`, normal force is nonpositive, and separation
speed is positive. Return explicit status/error for non-finite values,
compression/force excess, passive energy gain, or duration exhaustion.

- [ ] **Step 5: Run component and legacy cue tests**

Run: `cmake --build build --target BilliardsCoupledCueContactTests BilliardsCueContactTests -j2 && ctest --test-dir build -R 'Billiards(CoupledCueContact|CueContact)Tests' --output-on-failure`

Expected: both pass; legacy test expectations are unchanged.

- [ ] **Step 6: Commit**

```bash
git add CMakeLists.txt src/Billiards/cue_contact.h src/Billiards/coupled_cue_contact.h src/Billiards/coupled_cue_contact.cpp tests/coupled_cue_contact_tests.cpp
git commit -m "feat: solve finite cue contact with compliant microsteps"
```

---

### Task 4: Couple Rigid Ball and Cushion Constraints Inside Each Microstep

**Files:**
- Modify: `src/Billiards/coupled_cue_contact.cpp`
- Modify: `src/Billiards/contact_solver.h`
- Modify: `src/Billiards/contact_solver.cpp`
- Modify: `tests/coupled_cue_contact_tests.cpp`
- Modify: `tests/contact_solver_tests.cpp`

**Interfaces:**
- Produces `solveContactIslandIteration(GameState&, const ContactIsland&, const PhysicsProfile&, int velocityIterations, int positionIterations) -> ContactSolverResult`.
- Existing `solveContactIsland` delegates to the new bounded-iteration entry point with profile defaults, preserving existing output.

- [ ] **Step 1: Add failing coupled fixtures**

Add centered frozen pair, a three-ball chain, two symmetric neighbours, and a
ball-plus-cushion fixture. Assert every active neighbour receives finite
impulse during cue loading, symmetric fixtures remain mirrored, contact order
permutation yields identical final ball arrays, maximum penetration and
residual stay within profile limits, and halving microstep duration changes
each final velocity by no more than `0.5 cm/s`.

- [ ] **Step 2: Run and verify the pair still exhibits uncoupled behaviour**

Run: `cmake --build build --target BilliardsCoupledCueContactTests -j2 && ctest --test-dir build -R BilliardsCoupledCueContactTests --output-on-failure`

Expected: frozen neighbour velocity assertions fail because rigid contacts are not solved during loading.

- [ ] **Step 3: Expose a bounded rigid-solver iteration entry point**

Move the current solver body behind the new function and retain this wrapper:

```cpp
ContactSolverResult solveContactIsland(GameState& state,
    const ContactIsland& island, const PhysicsProfile& profile)
{
    return solveContactIslandIteration(state, island, profile,
        profile.solver.velocityIterations, profile.solver.positionIterations);
}
```

Reject nonpositive iteration arguments as `IterationLimit`; do not change
contact ordering, warm-start values, impulse equations, or status mapping.

- [ ] **Step 4: Execute rigid constraints in every compliant microstep**

After applying the cue impulse, call one deterministic velocity iteration and
one position iteration on the frozen island. Accumulate returned contact
diagnostics into the current `CueContactMicrostep`; propagate island,
penetration, nonfinite, and residual failures to the coupled status without
committing `result.state`.

- [ ] **Step 5: Run coupled and existing solver tests**

Run: `cmake --build build --target BilliardsCoupledCueContactTests BilliardsContactSolverTests BilliardsPhysicsTests -j2 && ctest --test-dir build -R 'Billiards(CoupledCueContact|ContactSolver|Physics)Tests' --output-on-failure`

Expected: all pass.

- [ ] **Step 6: Commit**

```bash
git add src/Billiards/coupled_cue_contact.cpp src/Billiards/contact_solver.h src/Billiards/contact_solver.cpp tests/coupled_cue_contact_tests.cpp tests/contact_solver_tests.cpp
git commit -m "feat: couple frozen rigid contacts during cue loading"
```

---

### Task 5: Route Frozen Shots Transactionally and Prove v4 Ordinary Equivalence

**Files:**
- Modify: `src/Billiards/game_runtime.h`
- Modify: `src/Billiards/game_runtime.cpp`
- Modify: `tests/game_runtime_tests.cpp`
- Create: `tests/ordinary_shot_equivalence_tests.cpp`
- Create: `physics_models/regression/phase3_v4_ordinary_shot_baseline.json`
- Modify: `CMakeLists.txt`

**Interfaces:**
- `applyCueShot(GameState&, ..., PhysicsBoundaryMode)` routes by topology.
- `CueShotApplication` returns `CueContactResult contact` whose `microsteps` is empty for v4 and complete for v5.
- `GameRuntime::applyCueImpact` retains the failed contact result but commits state and shot/rule flags only when `action.ok`.

- [ ] **Step 1: Generate and commit the v4 ordinary-shot baseline before routing changes**

Add a deterministic executable test helper containing centered, side-spin,
top/bottom-spin, moving-ball, unchalked, and miscue cases with every other ball
at least `0.1 cm` outside touching tolerance. Serialize pre/post ball bytes,
cue result excluding `microsteps`, rule flags, and the first eight frame/event
records into canonical JSON. Run it against current v4 and write
`phase3_v4_ordinary_shot_baseline.json`.

- [ ] **Step 2: Add failing frozen routing and rollback tests**

Enable `frozenCueContact`, install a frozen pair with
`replaceStateForScenario`, call `applyCueImpact`, and assert a non-empty
microtrace. Force `maximumContactSeconds = microstepSeconds` and assert the
action error is `cue_contact_no_release`, the complete `GameState` equals the
pre-shot state, no runtime event was appended, and failure microsteps remain
available through `cueContactResult()`.

- [ ] **Step 3: Implement routing and stable error mapping**

Call `detectFrozenCueTopology` before `resolveCueContact`. For non-frozen or
disabled profiles, execute the existing call without changing arguments. For
frozen enabled profiles, call `solveCoupledCueContact`; copy its state only on
success. Map statuses exactly:

```cpp
case CoupledCueContactStatus::NonfiniteState: return "nonfinite_state";
case CoupledCueContactStatus::PassiveEnergyGain: return "passive_energy_gain";
case CoupledCueContactStatus::CompressionLimit: return "compression_limit";
case CoupledCueContactStatus::IslandLimit: return "contact_island_limit";
case CoupledCueContactStatus::NoRelease: return "cue_contact_no_release";
case CoupledCueContactStatus::Nonconvergence: return "cue_contact_nonconvergence";
```

Apply player/rule/camera flags only after the selected physics path succeeds
or produces the existing modeled miscue.

- [ ] **Step 4: Prove strict ordinary equivalence and rollback**

Run: `cmake --build build --target BilliardsOrdinaryShotEquivalenceTests BilliardsGameRuntimeTests -j2 && ctest --test-dir build -R 'Billiards(OrdinaryShotEquivalence|GameRuntime)Tests' --output-on-failure`

Expected: the generated normalized v5 ordinary corpus equals the committed v4 baseline and all rollback assertions pass.

- [ ] **Step 5: Commit**

```bash
git add CMakeLists.txt src/Billiards/game_runtime.h src/Billiards/game_runtime.cpp tests/game_runtime_tests.cpp tests/ordinary_shot_equivalence_tests.cpp physics_models/regression/phase3_v4_ordinary_shot_baseline.json
git commit -m "feat: route frozen cue shots atomically"
```

---

### Task 6: Serialize the Complete Microtrace Through the Automation Boundary

**Files:**
- Modify: `src/Billiards/automation_protocol.cpp`
- Modify: `tests/automation_protocol_tests.cpp`
- Modify: `docs/automation-protocol.md`
- Modify: `tests/e2e/test_physics_validation.py`

**Interfaces:**
- `cue_contact.microtrace_schema_version == 1`.
- `cue_contact.microsteps` is always present: empty for v4 ordinary contact, populated for v5 frozen contact and frozen failures.
- Existing stdio commands and transport abstraction remain unchanged.

- [ ] **Step 1: Add failing schema and failed-shot observability tests**

Assert every microstep contains:

```text
index, time_seconds, cue_position_m, cue_velocity_m_s,
cue_acceleration_m_s2, compression_m, compression_rate_m_s,
normal_force_n, tangential_force_n, normal_impulse_n_s,
tangential_impulse_n_s, kinetic_energy_j, elastic_energy_j,
dissipated_energy_j, energy_residual_j, maximum_penetration_cm,
solver_residual_cm_s, solver_iterations, regime, balls, contacts
```

Use the no-release fixture and verify `get_state` returns the failure code and
microsteps even though no shot was committed. Verify two JSON serializations
are byte-identical.

- [ ] **Step 2: Run and verify failure**

Run: `cmake --build build --target BilliardsAutomationProtocolTests -j2 && ctest --test-dir build -R BilliardsAutomationProtocolTests --output-on-failure`

Expected: `microsteps` and schema version are absent.

- [ ] **Step 3: Implement canonical serialization**

Add `cueContactMicrostepValue` and append microsteps in stored order. Reuse
`pointValue`; serialize `CueContactConstraintSample` with the same explicit
unit suffixes as ordinary contact telemetry. Emit finite JSON numbers only; a non-finite diagnostic is represented
by the stable failed result before serialization, never by NaN/Infinity.
Document the new optional payload and state that any future socket transport
must forward the identical command and trace objects.

- [ ] **Step 4: Run protocol and headless E2E tests**

Run: `cmake --build build --target BilliardsAutomationProtocolTests Billiards -j2 && ctest --test-dir build -R 'Billiards(AutomationProtocolTests|PhysicsValidationE2E|HeadlessAutomationE2E)' --output-on-failure`

Expected: all selected tests pass.

- [ ] **Step 5: Commit**

```bash
git add src/Billiards/automation_protocol.cpp tests/automation_protocol_tests.cpp tests/e2e/test_physics_validation.py docs/automation-protocol.md
git commit -m "feat: expose frozen cue contact microtraces"
```

---

### Task 7: Admit Independent Contact-Time Data and Fit Identifiable Parameters

**Files:**
- Create: `tools/physics_validation/extract_shimamura_2006_cue_contact.py`
- Create: `tools/physics_validation/fit_frozen_cue_contact.py`
- Create: `tests/physics_validation/test_shimamura_2006_cue_contact.py`
- Create: `tests/physics_validation/test_fit_frozen_cue_contact.py`
- Create: `tests/physics_validation/reference_data/shimamura_2006_cue_contact/{raw_digitized.csv,normalized.csv,scalars.csv,extraction.json,source_access_audit.json,manifest.json}`
- Create: `physics_models/calibration/frozen_cue_contact_v1_{inputs.csv,fit.json,residuals.csv,sensitivity.csv}`
- Modify: `tests/physics_validation/validation_data_status.json`

**Interfaces:**
- `extract_shimamura_2006() -> dict[str, bytes]` reproduces all package files.
- `fit_frozen_contact(points, fixed, bounds) -> dict` fits stiffness to duration and reports dissipation sensitivity; it never reads confirmation packages.

- [ ] **Step 1: Add failing extraction and lifecycle tests**

Register `shimamura_2006_cue_contact`, version `1.0.0`, as calibration/spent.
Assert the package records DOI `10.1299/jsmekanto.2006.12.495`, the J-STAGE
free-access PDF URL, centered impact, published cue speed `3.0 m/s`, numerical
step `0.00005 s`, and complete digitized experimental contact-trace points
from Figure 5. Assert source media is not committed and regenerated bytes
equal every committed package file and manifest hash.

- [ ] **Step 2: Implement deterministic extraction**

Digitize Figure 5 against its printed axes and emit every selected point, not
only the inferred duration:

```csv
point_id,time_s,experimental_strain,source_locator
shimamura_fig5_000,0.000000,0.000000,Shimamura_2006_p496_Fig5
```

The first row fixes the origin; subsequent rows contain the reviewed pixel-to-
axis values at each `0.00005 s` analysis step through release. Record the
source URL `https://www.jstage.jst.go.jp/article/jsmekanto/2006.12/0/2006.12_495/_pdf/-char/en`,
PDF hash obtained during implementation, retrieval date, licence status
`free-access-no-redistribution-grant-recorded`, axis calibration points,
digitization residual, and the fact that only extracted numeric facts plus
repository-generated files are committed. Define contact duration as the
last time before the experimental trace returns to the registered zero-strain
band; propagate half a time step as timing uncertainty.

- [ ] **Step 3: Write the failing fit and sensitivity tests**

Use a fixed exponent `1.5`, fixed tangential values from the registered v4 cue
contract, and bounded dissipation values `{0.0, 0.025, 0.05, 0.075, 0.1} s/m`.
Assert the selected stiffness minimizes normalized duration/trace residual,
simulated duration is within the package's `0.000025 s` timing uncertainty,
all candidates are finite/passive,
and `sensitivity.csv` retains every tested pair rather than only the winner.

- [ ] **Step 4: Implement a deterministic bounded grid/refinement fit**

Scan log-spaced stiffness `1e5..1e9 N/m^(3/2)` at 161 points for each fixed
dissipation, refine the best bracket with 80 bisection iterations, and select
the lexicographically smallest `(absolute residual, stiffness, dissipation)`.
Invoke the same compliant equations as the C++ analytic fixture through a
small pure Python mirror and cross-check the selected parameters by executing
the C++ scenario path. Emit inputs, winner, every residual, and finite
difference sensitivities for `0.5x`, `1x`, and `2x` of each non-fixed value.

- [ ] **Step 5: Run extraction and fit tests**

Run: `python3 -m unittest tests.physics_validation.test_shimamura_2006_cue_contact tests.physics_validation.test_fit_frozen_cue_contact -v`

Expected: all tests pass and rerunning both generators leaves `git diff --exit-code` clean.

- [ ] **Step 6: Commit**

```bash
git add tools/physics_validation/extract_shimamura_2006_cue_contact.py tools/physics_validation/fit_frozen_cue_contact.py tests/physics_validation/test_shimamura_2006_cue_contact.py tests/physics_validation/test_fit_frozen_cue_contact.py tests/physics_validation/reference_data/shimamura_2006_cue_contact tests/physics_validation/validation_data_status.json physics_models/calibration/frozen_cue_contact_v1_inputs.csv physics_models/calibration/frozen_cue_contact_v1_fit.json physics_models/calibration/frozen_cue_contact_v1_residuals.csv physics_models/calibration/frozen_cue_contact_v1_sensitivity.csv
git commit -m "data: fit frozen cue compliance from public duration evidence"
```

---

### Task 8: Convert Alciatore to Spent Regression and Correct Endpoint Metrics

**Files:**
- Modify: `tools/physics_validation/alciatore_confirmation.py`
- Modify: `tests/physics_validation/test_alciatore_confirmation.py`
- Modify: `tests/physics_validation/validation_data_status.json`
- Create: `physics_models/calibration/alciatore_frozen_contact_v5_{inputs.csv,residuals.csv,sensitivity.csv,report.json}`

**Interfaces:**
- `evaluate_alciatore` returns interior angle gates plus `head_on_cue_residual_speed_ratio`, `head_on_cue_lateral_speed_ratio`, and `grazing_target_speed_ratio`.
- No endpoint angle is emitted when the selected ball speed is `<= 1e-6 cm/s`.

- [ ] **Step 1: Write failing endpoint semantic tests**

Provide synthetic traces where the cue ball is stopped at 0 degrees and the
target ball is stopped at 90 degrees. Assert evaluation succeeds without
calling `atan2` for those balls, emits `observed: null` for undefined angles,
and evaluates only the three named speed ratios. Assert all nine rows and all
seven interior signed errors remain present.

- [ ] **Step 2: Run and verify the old endpoint contract fails**

Run: `python3 -m unittest tests.physics_validation.test_alciatore_confirmation -v`

Expected: old target-direction/object-speed endpoint fields differ from the corrected contract.

- [ ] **Step 3: Implement corrected endpoints and spent lifecycle**

Change Alciatore lifecycle fields to `spent`. Preserve source files and v4
confirmation evidence byte-for-byte. Use `_stable_velocity` only after a
speed check; compute ratios against the incident cue-ball speed. Keep
interior RMSE `<= 3` and maximum absolute error `<= 5`, contact completeness,
finite state, passivity, and deterministic trace gates unchanged.

- [ ] **Step 4: Generate complete v5 regression artifacts**

Run all nine v5 cases twice and write input parameters, every raw/normalized
residual, aggregate report, and the same `0.5x/1x/2x` parameter sensitivity
matrix used by the compliance fit. No file may contain only aggregate values.

- [ ] **Step 5: Run focused tests**

Run: `python3 -m unittest tests.physics_validation.test_alciatore_confirmation tests.physics_validation.test_phase3_v4_confirmation -v`

Expected: corrected v5 evaluator tests pass and immutable v4 evidence tests still pass.

- [ ] **Step 6: Commit**

```bash
git add tools/physics_validation/alciatore_confirmation.py tests/physics_validation/test_alciatore_confirmation.py tests/physics_validation/validation_data_status.json physics_models/calibration/alciatore_frozen_contact_v5_inputs.csv physics_models/calibration/alciatore_frozen_contact_v5_residuals.csv physics_models/calibration/alciatore_frozen_contact_v5_sensitivity.csv physics_models/calibration/alciatore_frozen_contact_v5_report.json
git commit -m "data: regress v5 against spent Alciatore evidence"
```

---

### Task 9: Admit Cross 2016 as the Independent Frozen-Shot Confirmation

**Files:**
- Create: `tools/physics_validation/extract_cross_2016_newtons_cradle.py`
- Create: `tools/physics_validation/cross_2016_confirmation.py`
- Create: `tests/physics_validation/test_cross_2016_newtons_cradle.py`
- Create: `tests/physics_validation/reference_data/cross_2016_newtons_cradle/{raw_frame_tracks.csv,normalized.csv,scalars.csv,extraction.json,split.json,scenario_template.json,expected_model_mismatches.json,expected_reference_limitations.json,source_access_audit.json,manifest.json}`
- Modify: `tools/physics_validation/confirmation_adapters.py`
- Modify: `tests/physics_validation/validation_data_status.json`

**Interfaces:**
- Dataset ID/version: `cross_2016_newtons_cradle` / `1.0.0`.
- Produces a centered horizontal two-ball frozen scenario.
- Primary metric: `abs(v_back / v_front - 1) <= max(0.05, 2 * ratio_uncertainty)` after stable separation.

- [ ] **Step 1: Add failing source, licence, extraction, and unopened-state tests**

Assert DOI `10.1088/0031-9120/51/6/065020`, author manuscript URL
`https://www.oxfordcroquet.com/tech/cross2/`, supplementary video identifier
`4582`, apparatus statement “two billiard balls initially in contact”, and
published observation that both leave at essentially the same speed. Require
frame number, time, pixel coordinates, scale, velocities, and uncertainty for
both balls in committed CSV. Register both lifecycle fields as `confirmation`
and assert no candidate prediction/residual file exists before authorization.

- [ ] **Step 2: Implement deterministic video-track normalization**

Do not vendor the video without a redistribution grant. Commit the complete
reviewed frame tracks extracted from video 4582, the media URL/hash, frame
rate, spatial calibration, point-selection method, and reviewer record.
Compute each velocity by ordinary least squares over the registered stable
post-release frame window. Propagate one-pixel coordinate and half-frame time
uncertainty into `ratio_uncertainty`; emit all intermediate slopes and
residuals in `scalars.csv`.

- [ ] **Step 3: Implement the confirmation adapter and hard gates**

Build the frozen pair through `base_scenario`, use the source cue direction,
centered tip, source-equivalent ball dimensions where published and declared
WPA transfer values otherwise. Require contact, release, finite/passive
microtrace, no recontact, complete frames, deterministic repeated execution,
and the uncertainty-aware equal-speed gate. Emit both individual normalized
speeds and their signed ratio residual.

Update `base_scenario` to emit schema version 12 when the supplied profile has
`frozen_cue_contact`, and version 11 otherwise. This keeps all existing
confirmation fixtures on their original parser contract while both v5
Alciatore regression and Cross confirmation traverse the new exact schema.

- [ ] **Step 4: Run package tests and prove no confirmation access**

Run: `python3 -m unittest tests.physics_validation.test_cross_2016_newtons_cradle tests.physics_validation.test_confirmation_adapters -v`

Expected: package regeneration is byte-identical; adapter registration passes; access counters remain `UNOPENED`.

- [ ] **Step 5: Commit**

```bash
git add tools/physics_validation/extract_cross_2016_newtons_cradle.py tools/physics_validation/cross_2016_confirmation.py tools/physics_validation/confirmation_adapters.py tests/physics_validation/test_cross_2016_newtons_cradle.py tests/physics_validation/reference_data/cross_2016_newtons_cradle tests/physics_validation/validation_data_status.json
git commit -m "data: admit Cross frozen-shot confirmation"
```

---

### Task 10: Build and Verify the v5 Engineering Candidate

**Files:**
- Create: `tools/physics_validation/build_v5_profile.py`
- Create: `tests/physics_validation/test_build_v5_profile.py`
- Modify: `tools/physics_validation/freeze_candidate.py`
- Modify: `scripts/check_phase3_physics_release.py`
- Modify: `src/Billiards/physics_profile.cpp`
- Modify: `tests/physics_profile_tests.cpp`
- Modify: `tests/game_runtime_tests.cpp`
- Modify: `tests/automation_protocol_tests.cpp`
- Modify: `tests/physics_scenario_tests.cpp`
- Modify: `tests/physics_validation/test_candidate_workflow.py`
- Create: generated v5 profile, inventory, matrix, budget, and full-game artifacts declared in File Structure.

**Interfaces:**
- `write_v5_candidate(root: Path) -> dict` consumes v4 plus the two v5 calibration reports.
- `write_v5_candidate` produces every pre-freeze artifact consumed by Task 11.

- [ ] **Step 1: Write failing candidate identity and inventory tests**

Assert v5 retains v4 `ball`, `surface`, `cushion`, `table_boundary`, and
ordinary cue values; enables only `frozen_cue_contact`; uses profile ID
`chinese_pool_full_game_v5` and formula version
`phase3_integrated_v5_coupled_cue_contact_v1`; inventories every fit input,
residual, sensitivity, source manifest, ordinary baseline, protocol schema,
and both confirmation manifests by hash.

- [ ] **Step 2: Implement deterministic candidate generation**

Copy v4, inject the selected `frozen_cue_contact_v1_fit.json` values, update
identity/limitations, generate `phase3_v5_profile.inc`, clone the full-game
matrix/performance budget with v5 paths, and write canonical equivalence and
inventory documents. Extend freeze allowlists to v5 without weakening clean
tree, two-clean-build, executable hash, or artifact hash checks. Change
`defaultChinesePoolPhysicsProfile` to include `generated/phase3_v5_profile.inc`
and update only the literal profile-ID/formula-version expectations in C++
tests; no expected ball state, contact, event, or rule value may change.

- [ ] **Step 3: Run all pre-freeze engineering gates**

Run:

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build -j2
ctest --test-dir build --output-on-failure -E BilliardsPhase3PhysicsReleaseGate
python3 -m unittest discover -s tests/physics_validation -p 'test_*.py' -v
build/BilliardsFullGameStress --matrix physics_models/promotion/full_game_matrix_v5.json --output physics_models/candidates/phase3_integrated_v5/full_game
build/BilliardsFullGamePerformance --budget physics_models/promotion/full_game_performance_budget_v5.json
```

Expected: CTest and Python suites pass; all full-game cases pass; performance
is within budget; ordinary equivalence is exact; microstep-halving convergence
and deterministic microtrace gates pass.

- [ ] **Step 4: Commit the selected pre-freeze candidate**

Stage the generated profile, matrix, budget, inventory, full-game results, and
candidate-builder tests, then commit:

```bash
git add tools/physics_validation/build_v5_profile.py tests/physics_validation/test_build_v5_profile.py tools/physics_validation/freeze_candidate.py scripts/check_phase3_physics_release.py src/Billiards/physics_profile.cpp src/Billiards/generated/phase3_v5_profile.inc tests/physics_profile_tests.cpp tests/game_runtime_tests.cpp tests/automation_protocol_tests.cpp tests/physics_scenario_tests.cpp tests/physics_validation/test_candidate_workflow.py physics_models/profiles/chinese_pool_full_game_v5.json physics_models/promotion/phase3_candidates_v5.json physics_models/promotion/full_game_matrix_v5.json physics_models/promotion/full_game_performance_budget_v5.json physics_models/candidates/phase3_integrated_v5/full_game
git commit -m "feat: select phase 3 v5 coupled cue contact candidate"
```

---

### Task 11: Freeze the v5 Candidate and Prove Confirmation Readiness

**Files:**
- Create: `tests/physics_validation/test_phase3_v5_readiness.py`
- Modify: `tools/physics_validation/confirmation_readiness.py`
- Create: `physics_models/candidates/phase3_integrated_v5/freeze.json`
- Create: `physics_models/candidates/phase3_integrated_v5/confirmation_contract_proof.json`
- Create: `physics_models/candidates/phase3_integrated_v5/confirmation_readiness.json`

**Interfaces:**
- `build_readiness` requires Cross 2016 and Han both `UNOPENED` and binds the real-path synthetic contract proof.

- [ ] **Step 1: Write failing v5 readiness tests**

Assert the v5 candidate ID is admitted, every inventory hash is bound, the
source revision is clean, two clean build executable/profile hashes match,
the synthetic scenario traverses the real parser and automation executable,
and both confirmation packages remain `UNOPENED`.

- [ ] **Step 2: Run and verify v5 is not yet admitted**

Run: `python3 -m unittest tests.physics_validation.test_phase3_v5_readiness -v`

Expected: readiness rejects the unknown v5 candidate or missing freeze.

- [ ] **Step 3: Extend readiness without weakening earlier candidates**

Add v5 inventory/profile IDs to exact allowlists, select package keys
`cross_2016_newtons_cradle` and `han_2005` for v5, and retain existing v2--v4
branches unchanged. Bind the ordinary equivalence baseline, microtrace schema,
fit residual/sensitivity files, and synthetic contract proof.

- [ ] **Step 4: Freeze from the clean selected revision and create readiness**

Run two clean builds through `freeze_candidate.py`, run the synthetic real
parser/automation contract, and build readiness. Assert both Cross 2016 and
Han are `UNOPENED`, all failures are empty, and every executable/profile/data/
script/result hash is bound.

- [ ] **Step 5: Run readiness tests**

Run: `python3 -m unittest tests.physics_validation.test_phase3_v5_readiness tests.physics_validation.test_confirmation_contract_fixture -v`

Expected: all tests pass and both real packages are still `UNOPENED`.

- [ ] **Step 6: Commit the immutable freeze checkpoint**

```bash
git add tools/physics_validation/confirmation_readiness.py tests/physics_validation/test_phase3_v5_readiness.py physics_models/candidates/phase3_integrated_v5/freeze.json physics_models/candidates/phase3_integrated_v5/confirmation_contract_proof.json physics_models/candidates/phase3_integrated_v5/confirmation_readiness.json
git commit -m "data: freeze phase 3 v5 candidate"
```

---

### Task 12: Execute the Sole Cross 2016 Confirmation Transaction

**Files:**
- Modify only generated transaction artifacts below `physics_models/candidates/phase3_integrated_v5/confirmation/` and `confirmation_consumption.json`.
- Create: `tests/physics_validation/test_phase3_v5_confirmation.py`

**Interfaces:**
- Consumes frozen executable/profile/package hashes.
- Produces one final Cross attempt record, full traces, metrics, diagnostics, provenance, and receipt.

- [ ] **Step 1: Verify readiness without opening the package**

Run the v5 readiness and freeze-verifier tests. Expected: Cross and Han both
report `UNOPENED`; no ledger attempt exists.

- [ ] **Step 2: Stop and request explicit user authorization**

State the exact command, candidate freeze SHA-256, package manifest SHA-256,
and that reservation is irreversible. Do not run the command in the same turn
unless the user explicitly authorizes the Cross 2016 transaction.

- [ ] **Step 3: Reserve and execute Cross exactly once after authorization**

Use `confirmation_run.py` with candidate `phase3_integrated_v5`, dataset
`cross_2016_newtons_cradle`, the frozen executable, and the governed output
directory. Never delete or retry an opened attempt.

- [ ] **Step 4: Validate and commit the complete result**

Require byte-identical repeated traces, all point and hard gates, complete
microsteps, hash-bound provenance and a finalized receipt. If any check fails,
write and commit v5 rejection and leave Han unopened. If it passes, commit:

```bash
git add physics_models/candidates/phase3_integrated_v5/confirmation_consumption.json physics_models/candidates/phase3_integrated_v5/confirmation/cross_2016_newtons_cradle tests/physics_validation/test_phase3_v5_confirmation.py
git commit -m "data: record phase 3 v5 Cross confirmation"
```

---

### Task 13: Execute Han and Bind the Final Assessment

**Files:**
- Create/modify authorized Han transaction artifacts under the frozen v5 candidate.
- Create: `tools/physics_validation/phase3_v5_assessment.py`
- Create: `physics_models/candidates/phase3_integrated_v5/final_assessment.json`
- Create on failure: `physics_models/promotion/phase3_integrated_v5_rejection.json`
- Create on success: `physics_models/promotion/phase3_integrated_v5_acceptance.json`
- Modify: `tests/physics_validation/test_phase3_v5_confirmation.py`
- Modify: `docs/phase3-physics-promotion-report.md`

**Interfaces:**
- `build_final_assessment(root: Path) -> dict` accepts only when Cross and Han receipts both pass and all hashes bind to one freeze.

- [ ] **Step 1: Prove the assessment refuses premature Han or acceptance**

Test that Han is absent after a failed Cross result, that an unfinalized or
duplicate attempt is rejected, and that Cross-only evidence cannot produce
`ACCEPTED`.

- [ ] **Step 2: Stop and request separate explicit Han authorization**

Only after committed Cross success, report the frozen candidate and Han
manifest hashes and the irreversible command. Do not infer authorization from
the earlier Cross approval.

- [ ] **Step 3: Reserve and execute Han exactly once after authorization**

Run the unchanged Han adapter and gates. Preserve every trace, metric,
limitation, provenance file, ledger transition, and receipt whether it passes
or fails.

- [ ] **Step 4: Build the final disposition and full verification report**

Bind freeze, readiness, both receipts, both output tree hashes, complete
engineering suite results, ordinary equivalence, Alciatore regression,
performance, convergence, and clean-build evidence. Report Cross as direct
frozen-contact confirmation and retain Han's cross-equipment cushion transfer
limitation.

- [ ] **Step 5: Run final release verification**

Run:

```bash
cmake --build build -j2
ctest --test-dir build --output-on-failure
python3 -m unittest discover -s tests/physics_validation -p 'test_*.py' -v
python3 scripts/check_phase3_physics_release.py --root .
git diff --check
```

Expected: all tests and release gates pass only for an accepted two-receipt
assessment; otherwise the immutable rejection path is the expected outcome.

- [ ] **Step 6: Commit the final evidence**

```bash
git add tools/physics_validation/phase3_v5_assessment.py tests/physics_validation/test_phase3_v5_confirmation.py physics_models/candidates/phase3_integrated_v5 physics_models/promotion/phase3_integrated_v5_*.json docs/phase3-physics-promotion-report.md
git commit -m "data: assess phase 3 v5 confirmations"
```

---

## Execution Notes

- Tasks 1--11 are engineering, calibration, regression, and freeze work; they
  must not open either confirmation partition.
- Task 12 is a hard authorization boundary for Cross 2016.
- Task 13 is a second hard authorization boundary for Han.
- Any confirmation failure ends this candidate. Diagnose it from committed
  evidence, design a successor candidate, and acquire fresh independent
  confirmation evidence before another promotion attempt.
