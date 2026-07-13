# BilliardGL Physics Validation Foundation Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Build the first independently usable slice of the physics-reality validation system: authoritative per-tick telemetry, shared versioned scenarios, automated invariant analysis, reproducible reports, and CI detection of both regressions and the engine's current known failures.

**Architecture:** Instrument the production `updatePhysics` path without changing its numerical behavior, retain bounded traces in `GameRuntime`, and expose those traces through protocol-v1-compatible commands. A dependency-free Python runner loads the same JSON scenario files used by direct C++ tests, drives the real executable, classifies failures, and writes JSON plus Markdown reports.

**Tech Stack:** C++11, existing `GameState`/`GameRuntime`/automation JSON codec, CMake/CTest, Python 3 standard library, JSON/CSV/Markdown artifacts.

## Global Constraints

- WPA American Pool is the sole primary equipment baseline; snooker evidence may validate trends but may not supply unconverted Pool parameters.
- Scope is table-plane 2.5D: two-dimensional center motion and three-dimensional angular velocity; jump, masse, airborne, and off-table motion are excluded.
- Physics telemetry is sampled once per fixed physics tick, never from render frames.
- Headless and rendered automation must execute the same production `GameRuntime` and physics code.
- Protocol version remains integer `1`; additions are optional commands and fields, not a breaking version change.
- Same executable, input, and tick count must reproduce identical serialized trace values on the same platform/build.
- No new package-manager dependency; C++ remains C++11 and Python uses only the standard library.
- A known failure remains a failed physics result in reports. CI may assert its exact classification, but may not relabel it as passing or widen its tolerance.
- This phase records angular velocity but does not yet implement spin dynamics; zero angular velocity is an explicit current-engine limitation.

## Delivery Roadmap

This plan is phase 1 of three independently reviewable projects:

1. **Validation foundation — this plan:** telemetry, shared scenarios, analyzers, reproducible reports, and strict known-failure accounting.
2. **Public experimental benchmarks — later plan:** acquire and license-check cited datasets, digitize plots with uncertainty metadata, split calibration/holdout cases, and add experimental acceptance metrics.
3. **Physics model optimization — later plans by subsystem:** continuous collision/contact solving, sliding/rolling/spin, cushions, cue impact, and pocket jaws. Each removes corresponding known failures only after its blind benchmarks pass.

### Spec Coverage Boundary

- Design sections 4, 7, 8, and 9 are implemented by this foundation: scenario flow, authoritative recording, analysis, execution tiers, reports, failure classification, and regression snapshots.
- Design section 5's analytic hard gates begin here; experiment-specific tolerances are implemented when each phase-2 dataset is added with its uncertainty.
- Design section 3's reference dataset catalog and calibration/holdout split are phase 2 because exact figures, licensing, and digitization uncertainty must be reviewed together with the imported numbers.
- Design section 6's full scenario matrix is divided between phases 2 and 3. This phase installs one passing analytic case and four numerical edge cases; later model plans add each subsystem's complete matrix before claiming that subsystem compliant.
- The overall completion criteria in design section 10 are reached only after all three projects, not at the end of this foundation plan.

## File Structure

- `src/Billiards/physics_telemetry.h/.cpp`: contact records, per-ball/per-step samples, energy/momentum calculations, and bounded trace storage.
- `src/Billiards/physics.h/.cpp`: returns step telemetry from the existing production solver while preserving current motion results.
- `src/Billiards/game_state.h`: adds authoritative three-dimensional `angularVelocity` to `BallState` and the current model's configurable ball mass.
- `src/Billiards/game_runtime.h/.cpp`: owns trace enablement, retention, clearing, and per-tick recording.
- `src/Billiards/physics_scenario.h/.cpp`: parses and atomically applies canonical version-1 scenarios.
- `src/Billiards/automation_protocol.h/.cpp`: serializes telemetry and the new angular velocity state.
- `src/Billiards/automation_controller.cpp`: adds bounded trace/scenario commands while keeping transport replaceable.
- `tools/physics_validation/`: Python scenario runner, invariant analyzer, strict known-failure matcher, and JSON/Markdown report writer.
- `tests/physics_validation/scenarios/`: canonical versioned scenario inputs shared by C++ and process E2E tests.
- `tests/physics_validation/known_failures.json`: auditable exact set of current failures, each with the code location and removal condition.
- `tests/physics_validation/`: C++ and Python tests for telemetry, scenario parsing, analysis, reports, and process integration.
- `docs/physics-validation.md`: operator commands, artifact schema, evidence meanings, and failure-removal workflow.

---

### Task 1: Add authoritative telemetry value types

**Files:**
- Create: `src/Billiards/physics_telemetry.h`
- Create: `src/Billiards/physics_telemetry.cpp`
- Create: `tests/physics_telemetry_tests.cpp`
- Modify: `src/Billiards/game_state.h`
- Modify: `src/Billiards/game_state.cpp`
- Modify: `src/Billiards/automation_protocol.cpp`
- Modify: `tests/automation_protocol_tests.cpp`
- Modify: `CMakeLists.txt`

**Interfaces:**
- Produces `PhysicsContactKind`, `PhysicsContactRecord`, `PhysicsBallSample`, `PhysicsControlSample`, `PhysicsStepTelemetry`, `PhysicsFrame`, `PhysicsTrace`, `capturePhysicsFrame`, `translationalMomentumKgMps`, and `translationalKineticEnergyJ`.
- Adds `BallState::angularVelocity` in radians/second and serializes it as `angular_velocity`.
- Uses `kDefaultBallMassKg = 0.17f` only as the current configurable model default; reference datasets retain their own mass.

- [ ] **Step 1: Write the failing telemetry tests**

Create `tests/physics_telemetry_tests.cpp` with this test body and the repository's existing `expect` helper style:

```cpp
#include "game_state.h"
#include "physics_telemetry.h"

#include <cmath>
#include <cstdlib>
#include <iostream>

namespace {
void expect(bool value, const char* message)
{
    if (!value) { std::cerr << message << '\n'; std::exit(1); }
}
bool close(double a, double b, double epsilon = 1e-6)
{
    return std::fabs(a - b) <= epsilon;
}
}

int main()
{
    billiardgl::GameState before;
    billiardgl::initializeBalls(before);
    for (int i = 1; i < billiardgl::kBallCount; ++i) before.balls[i].pocketed = true;
    billiardgl::setBallVelocity(before.balls[0], 100.0f, 0.0f, 0.0f);
    before.balls[0].angularVelocity = billiardgl::Point3{0.0f, 2.0f, 0.0f};

    billiardgl::GameState after = before;
    after.balls[0].position.x += 10.0f;
    after.balls[0].velocity.x = 96.0f;
    after.balls[0].speed = 96.0f;

    billiardgl::PhysicsStepTelemetry step;
    const billiardgl::PhysicsFrame frame =
        billiardgl::capturePhysicsFrame(7, 0.7, 0.1f, before, after, step);

    expect(frame.tick == 7 && close(frame.timeSeconds, 0.7), "tick and time");
    expect(close(frame.balls[0].acceleration.x, -40.0), "acceleration derives from velocity delta");
    expect(close(frame.balls[0].angularVelocity.y, 2.0), "angular velocity is authoritative state");
    expect(close(frame.control.shotPower, before.input.shotPower), "available shot input is recorded");
    expect(close(frame.linearMomentum.x, 0.17 * 0.96), "momentum uses SI units");
    expect(close(frame.translationalKineticEnergyJ, 0.5 * 0.17 * 0.96 * 0.96), "energy uses SI units");

    billiardgl::PhysicsTrace trace(2);
    trace.append(frame); trace.append(frame); trace.append(frame);
    expect(trace.frames().size() == 2 && trace.droppedFrames() == 1, "bounded trace reports drops");
    return 0;
}
```

Extend `tests/automation_protocol_tests.cpp` to assert that every serialized ball contains `angular_velocity` with x/y/z values.

- [ ] **Step 2: Run the tests to verify RED**

Run:

```bash
cmake -S . -B build/check
cmake --build build/check --target BilliardsPhysicsTelemetryTests BilliardsAutomationProtocolTests
```

Expected: compilation fails because `physics_telemetry.h` and `BallState::angularVelocity` do not exist.

- [ ] **Step 3: Define telemetry types and units**

Create `physics_telemetry.h` with these public declarations:

```cpp
#pragma once

#include "game_state.h"

#include <array>
#include <cstddef>
#include <cstdint>
#include <deque>
#include <vector>

namespace billiardgl {

constexpr float kDefaultBallMassKg = 0.17f;

enum class PhysicsContactKind { BallBall, Rail, Pocket };

struct PhysicsContactRecord {
    PhysicsContactKind kind = PhysicsContactKind::BallBall;
    int firstBall = -1;
    int secondBall = -1;
    Point3 normal;
    double normalImpulseNs = 0.0;
    double penetrationCm = 0.0;
};

struct PhysicsBallSample {
    Point3 position;
    Point3 velocity;
    Point3 acceleration;
    Point3 angularVelocity;
    float speed = 0.0f;
    bool pocketed = false;
};

struct PhysicsControlSample {
    float aimYaw = 0.0f;
    float shotPower = 0.0f;
    bool shotTaken = false;
};

struct PhysicsStepTelemetry {
    std::vector<PhysicsContactRecord> contacts;
    double maximumPenetrationCm = 0.0;
};

struct PhysicsFrame {
    std::uint64_t tick = 0;
    double timeSeconds = 0.0;
    float deltaSeconds = 0.0f;
    std::array<PhysicsBallSample, kBallCount> balls;
    PhysicsControlSample control;
    std::vector<PhysicsContactRecord> contacts;
    Point3 linearMomentum;
    double translationalKineticEnergyJ = 0.0;
    double maximumPenetrationCm = 0.0;
};

Point3 translationalMomentumKgMps(const GameState& state, float ballMassKg = kDefaultBallMassKg);
double translationalKineticEnergyJ(const GameState& state, float ballMassKg = kDefaultBallMassKg);
PhysicsFrame capturePhysicsFrame(std::uint64_t tick, double timeSeconds, float deltaSeconds,
    const GameState& before, const GameState& after, const PhysicsStepTelemetry& telemetry);

class PhysicsTrace {
public:
    explicit PhysicsTrace(std::size_t capacity = 10000) : capacity_(capacity) {}
    void clear();
    void append(const PhysicsFrame& frame);
    const std::deque<PhysicsFrame>& frames() const { return frames_; }
    std::size_t droppedFrames() const { return droppedFrames_; }
private:
    std::size_t capacity_;
    std::size_t droppedFrames_ = 0;
    std::deque<PhysicsFrame> frames_;
};

}  // namespace billiardgl
```

Implement conversions from cm/s to m/s before momentum and energy calculations. Derive acceleration as `(after.velocity - before.velocity) / deltaSeconds`; when `deltaSeconds <= 0`, store a zero vector. Copy `aim.yaw`, `input.shotPower`, and `players.shotTaken` into `control`; these are the complete authoritative shot inputs currently available, while cue-tip offset and cue velocity belong to the later cue-impact model. Exclude pocketed balls from totals. `PhysicsTrace::append` evicts exactly one oldest frame at capacity and increments `droppedFrames_`; capacity zero records no frames and counts every append as dropped.

Add `Point3 angularVelocity;` to `BallState`, copy/reset it in `game_state.cpp`, and serialize it beside `rotation_axis`. Do not derive it from the visual `rotationAngle`.

- [ ] **Step 4: Register and run the focused tests**

Add `physics_telemetry.cpp` to `BILLIARDGL_CORE_SOURCES` and register `BilliardsPhysicsTelemetryTests` through `billiardgl_add_core_test`.

Run:

```bash
cmake --build build/check --parallel
ctest --test-dir build/check --output-on-failure -R 'Billiards(PhysicsTelemetry|AutomationProtocol|GameState)Tests'
```

Expected: all selected tests pass.

- [ ] **Step 5: Commit the telemetry model**

```bash
git add CMakeLists.txt src/Billiards/game_state.h src/Billiards/game_state.cpp \
  src/Billiards/physics_telemetry.h src/Billiards/physics_telemetry.cpp \
  src/Billiards/automation_protocol.cpp tests/physics_telemetry_tests.cpp \
  tests/automation_protocol_tests.cpp
git commit -m "feat: add authoritative physics telemetry types"
```

---

### Task 2: Instrument the production physics step without changing behavior

**Files:**
- Modify: `src/Billiards/physics.h`
- Modify: `src/Billiards/physics.cpp`
- Modify: `src/Billiards/game_runtime.cpp`
- Modify: `tests/physics_tests.cpp`
- Create: `tests/physics_instrumentation_tests.cpp`
- Modify: `CMakeLists.txt`

**Interfaces:**
- Changes `updatePhysics(GameState&, float)` to return `PhysicsStepTelemetry`; existing callers may ignore the return value.
- Produces one `PhysicsContactRecord` for every current solver ball-ball, rail, and pocket event.
- Contact instrumentation observes pre/post state; it must not alter collision thresholds, positions, velocities, ordering, or event flags in this phase.

- [ ] **Step 1: Write behavior-preservation and event tests**

Create `tests/physics_instrumentation_tests.cpp`. For each case, clone the initial state, call the public collision helper directly on one clone and `updatePhysics` with zero friction-equivalent setup on the other, then assert the legacy post-collision velocity remains unchanged. Add these telemetry assertions:

```cpp
const billiardgl::PhysicsStepTelemetry result = billiardgl::updatePhysics(state, 0.0f);
expect(result.contacts.size() == 1, "one overlapping approaching pair emits one contact");
expect(result.contacts[0].kind == billiardgl::PhysicsContactKind::BallBall, "ball contact kind");
expect(result.contacts[0].firstBall == 0 && result.contacts[0].secondBall == 1, "stable ball ids");
expect(result.contacts[0].penetrationCm > 0.0, "penetration recorded before correction");
expect(result.contacts[0].normalImpulseNs >= 0.0, "nonnegative impulse magnitude");
```

Add separate rail and pocket cases. Add a no-contact case that expects an empty vector and zero maximum penetration. Preserve a pre-instrumentation golden state for a five-tick shot and compare every ball position/velocity before and after this task.

- [ ] **Step 2: Verify RED**

Run:

```bash
cmake --build build/check --target BilliardsPhysicsInstrumentationTests
```

Expected: compilation fails because `updatePhysics` still returns `void`.

- [ ] **Step 3: Capture ball-ball contacts around the legacy solver**

Before `collideBalls`, compute distance, unit normal, penetration `max(0, 2R-distance)`, and both pre-collision velocities. If the legacy helper returns true, append a ball-ball record. Compute the equal-mass normal impulse from the magnitude of the first ball's velocity change projected onto the normal:

```cpp
const double deltaMetersPerSecond = std::fabs(
    (ball.velocity.x - beforeVelocity.x) * normal.x +
    (ball.velocity.z - beforeVelocity.z) * normal.z) / 100.0;
contact.normalImpulseNs = kDefaultBallMassKg * deltaMetersPerSecond;
```

Record penetration even though the legacy solver corrects only the second ball. Update `maximumPenetrationCm` with the pre-correction value. Exact coincident centers remain unhandled by the current solver and therefore produce no contact here; the canonical coincident-ball scenario in Task 5 will classify that behavior as a numerical failure.

- [ ] **Step 4: Capture rail and pocket contacts**

For each ball, snapshot position, velocity, and pocket state around `collideWithTableEdge` and `updatePocketedBall`.

- A changed x velocity creates a rail normal `(-sign(beforeX), 0, 0)`.
- A changed z velocity creates a rail normal `(0, 0, -sign(beforeZ))`.
- If both change in a corner, emit two rail records so each normal impulse is independently inspectable.
- A successful `updatePocketedBall` emits a pocket record with zero impulse and zero penetration because the current pocket model teleports rather than resolves contact.

Return the collected `PhysicsStepTelemetry` after the existing `ballsMoving` and `shotEnded` updates. Do not change the order `ball pairs → rail → pocket → friction/move` in this task.

- [ ] **Step 5: Verify focused tests and the legacy golden state**

Register the new test and run:

```bash
cmake --build build/check --parallel
ctest --test-dir build/check --output-on-failure -R 'Billiards(PhysicsInstrumentation|Physics|GameRuntime)Tests'
```

Expected: telemetry tests pass and the legacy golden state is byte-for-byte unchanged for serialized position/velocity fields.

- [ ] **Step 6: Commit production instrumentation**

```bash
git add CMakeLists.txt src/Billiards/physics.h src/Billiards/physics.cpp \
  src/Billiards/game_runtime.cpp tests/physics_tests.cpp tests/physics_instrumentation_tests.cpp
git commit -m "feat: instrument production physics contacts"
```

---

### Task 3: Retain and expose bounded per-tick traces

**Files:**
- Modify: `src/Billiards/game_runtime.h`
- Modify: `src/Billiards/game_runtime.cpp`
- Modify: `src/Billiards/automation_protocol.h`
- Modify: `src/Billiards/automation_protocol.cpp`
- Modify: `src/Billiards/automation_controller.cpp`
- Modify: `tests/game_runtime_tests.cpp`
- Modify: `tests/automation_controller_tests.cpp`
- Modify: `tests/e2e/automation_client.py`
- Modify: `docs/automation-protocol.md`

**Interfaces:**
- Adds `GameRuntime::{setPhysicsTraceEnabled,physicsTraceEnabled,clearPhysicsTrace,physicsTrace}`.
- Adds commands `start_physics_trace`, `stop_physics_trace`, `clear_physics_trace`, and `get_physics_trace`.
- `get_physics_trace` accepts `after_tick` default `0` and `limit` range `1..1000`; result includes `frames`, `dropped_frames`, and `has_more`.

- [ ] **Step 1: Write failing runtime and controller tests**

Add to `tests/game_runtime_tests.cpp`:

```cpp
runtime.setPhysicsTraceEnabled(true);
runtime.step(3);
expect(runtime.physicsTrace().frames().size() == 3, "one trace frame per physics tick");
expect(runtime.physicsTrace().frames()[0].tick == 1, "first completed tick is one");
expect(runtime.physicsTrace().frames()[2].tick == 3, "third completed tick is three");
runtime.setPhysicsTraceEnabled(false);
runtime.step(1);
expect(runtime.physicsTrace().frames().size() == 3, "disabled trace does not append");
runtime.clearPhysicsTrace();
expect(runtime.physicsTrace().frames().empty(), "clear removes retained frames");
```

Add controller tests for all four commands, pagination, invalid limit `0`, invalid limit `1001`, and a trace response containing acceleration, angular velocity, contacts, momentum, energy, and penetration.

- [ ] **Step 2: Verify RED**

Run:

```bash
cmake --build build/check --target BilliardsGameRuntimeTests BilliardsAutomationControllerTests
```

Expected: missing runtime trace methods and unknown automation commands.

- [ ] **Step 3: Record frames at the authoritative runtime boundary**

Add these members to `GameRuntime`:

```cpp
bool physicsTraceEnabled_ = false;
PhysicsTrace physicsTrace_{10000};
```

For every loop iteration in `step`, copy `const GameState before = state_`, call `updatePhysics`, increment `tick_`, and then append:

```cpp
if (physicsTraceEnabled_) {
    physicsTrace_.append(capturePhysicsFrame(
        tick_, static_cast<double>(tick_) * kDefaultTimeStep,
        kDefaultTimeStep, before, state_, telemetry));
}
```

Reset clears the trace and disables capture. `clearPhysicsTrace` preserves game state and tick.

- [ ] **Step 4: Serialize and paginate trace frames**

Add `serializePhysicsContact` and `serializePhysicsFrame` to `automation_protocol`. Use stable strings `ball_ball`, `rail`, and `pocket`. Keep numeric field names and units explicit:

```json
{
  "tick": 8,
  "time_seconds": 0.8,
  "delta_seconds": 0.1,
  "linear_momentum_kg_mps": {"x": 0.1, "y": 0.0, "z": 0.0},
  "translational_kinetic_energy_j": 0.05,
  "maximum_penetration_cm": 0.2,
  "control": {"aim_yaw_rad": 1.57, "shot_power": 60.0, "shot_taken": true},
  "balls": [],
  "contacts": []
}
```

Pagination returns frames with `tick > after_tick`, stops at `limit`, and sets `has_more` if another retained matching frame exists. A request older than retained data still succeeds and reports the cumulative `dropped_frames` count.

- [ ] **Step 5: Extend the Python automation client and protocol documentation**

Add methods:

```python
def start_physics_trace(self): return self.command("start_physics_trace")
def stop_physics_trace(self): return self.command("stop_physics_trace")
def clear_physics_trace(self): return self.command("clear_physics_trace")
def physics_trace(self, after_tick=0, limit=1000):
    return self.command("get_physics_trace", {"after_tick": after_tick, "limit": limit})
```

Document retention, pagination, units, same-build determinism, and that angular velocity remains zero until the spin model is implemented.

- [ ] **Step 6: Verify and commit**

```bash
cmake --build build/check --parallel
ctest --test-dir build/check --output-on-failure -R 'Billiards(GameRuntime|AutomationController|AutomationProtocol)Tests'
git add src/Billiards/game_runtime.h src/Billiards/game_runtime.cpp \
  src/Billiards/automation_protocol.h src/Billiards/automation_protocol.cpp \
  src/Billiards/automation_controller.cpp tests/game_runtime_tests.cpp \
  tests/automation_controller_tests.cpp tests/e2e/automation_client.py docs/automation-protocol.md
git commit -m "feat: expose bounded per-tick physics traces"
```

---

### Task 4: Define canonical scenarios shared by direct and process tests

**Files:**
- Create: `src/Billiards/physics_scenario.h`
- Create: `src/Billiards/physics_scenario.cpp`
- Create: `tests/physics_scenario_tests.cpp`
- Create: `tests/physics_validation/scenarios/free_roll_v1.json`
- Create: `tests/physics_validation/scenarios/receding_overlap_v1.json`
- Create: `tests/physics_validation/scenarios/high_speed_tunneling_v1.json`
- Create: `tests/physics_validation/scenarios/reverse_cradle_v1.json`
- Modify: `src/Billiards/automation_controller.cpp`
- Modify: `CMakeLists.txt`

**Interfaces:**
- Produces `PhysicsScenario`, `PhysicsExpectation`, `parsePhysicsScenario(json::Value)`, and `applyPhysicsScenario(GameRuntime&, PhysicsScenario)`.
- Replaces the controller's inline `load_scenario` parsing with this atomic shared parser.
- Scenario schema version is `1`; coordinates use cm, velocities cm/s, angular velocities rad/s, and times seconds.

- [ ] **Step 1: Add a canonical passing fixture and failing parser tests**

Create `free_roll_v1.json`:

```json
{
  "schema_version": 1,
  "id": "free_roll_v1",
  "description": "Single ball rolling along +x under the current cloth model",
  "evidence": {"grade": "C", "source": "analytic", "equipment": "WPA_POOL"},
  "simulation": {"ticks": 10, "time_step_seconds": 0.1},
  "balls": [
    {"index": 0, "position_cm": [0.0, 92.715, 0.0], "velocity_cm_s": [20.0, 0.0, 0.0], "angular_velocity_rad_s": [0.0, 0.0, -7.0], "pocketed": false}
  ],
  "expectations": [
    {"metric": "finite_state", "operator": "eq", "value": true},
    {"metric": "nonincreasing_translational_energy", "operator": "eq", "value": true}
  ]
}
```

The parser treats omitted balls as pocketed and stationary, validates unique indices `0..15`, finite numeric values, exact equipment string `WPA_POOL`, evidence grade `A|B|C`, positive time step, and ticks `1..1000000`. Unknown schema versions fail with `unsupported_scenario_version`. Invalid input leaves runtime unchanged.

Write tests that load this file using `BILLIARDGL_SOURCE_ROOT`, parse it, apply it directly to `GameRuntime`, and assert only ball 0 is active. Add malformed in-memory objects for duplicate index, NaN-equivalent non-number, unknown evidence grade, and unknown schema version.

- [ ] **Step 2: Verify RED**

Run:

```bash
cmake --build build/check --target BilliardsPhysicsScenarioTests
```

Expected: compilation fails because `physics_scenario.h` is absent.

- [ ] **Step 3: Implement atomic parsing and application**

Use these result interfaces:

```cpp
struct PhysicsExpectation {
    std::string metric;
    std::string comparison;
    json::Value value;
    double absoluteTolerance = 0.0;
    double relativeTolerance = 0.0;
};

struct PhysicsScenario {
    int schemaVersion = 1;
    std::string id;
    std::string description;
    std::string evidenceGrade;
    std::string evidenceSource;
    int ticks = 0;
    float timeStepSeconds = kDefaultTimeStep;
    std::array<BallState, kBallCount> balls;
    std::vector<PhysicsExpectation> expectations;
};

struct PhysicsScenarioResult {
    bool ok = false;
    PhysicsScenario scenario;
    std::string errorCode;
    std::string errorMessage;
};
```

Initialize all balls from `GameState{}` plus `initializeBalls`, then mark them pocketed before applying listed entries. Set `speed` from horizontal velocity only, because this phase excludes airborne motion. `applyPhysicsScenario` constructs a complete copied `GameState`, calls `replaceState` once, clears events and traces, and never partially mutates runtime.

Map the JSON field `operator` directly into `PhysicsExpectation::comparison`; accepted values in phase 1 are `eq`, `lte`, and `gte`. Reject any other value during parsing.

- [ ] **Step 4: Add the three known-failure scenarios**

- `receding_overlap_v1`: two overlapping balls moving apart; expects no ball-ball impulse and unchanged velocity directions.
- `high_speed_tunneling_v1`: a cue ball moves more than one diameter per tick through a stationary object ball; expects a collision event before separation.
- `reverse_cradle_v1`: three touching balls tested in forward and reversed index layouts; expects permutation-invariant final velocities after one tick.

Use positions derived from `kChineseBallRadiusCm` as literal centimeter values in the fixture metadata and document that the generator must update fixtures when WPA geometry changes. Each scenario must reproduce one previously observed failure without relying on rendering.

- [ ] **Step 5: Route automation through the shared parser**

Change `load_scenario` to accept the canonical document under `params.scenario`, call `parsePhysicsScenario`, and return its stable error code on failure. Preserve backward compatibility for the existing `params.balls` form by translating it to a `PhysicsScenario` before atomic application. Advertise `physics_scenario_v1` in capabilities.

- [ ] **Step 6: Verify direct and controller paths and commit**

```bash
cmake --build build/check --parallel
ctest --test-dir build/check --output-on-failure -R 'Billiards(PhysicsScenario|AutomationController|AutomationPhysicsScenario)Tests'
git add CMakeLists.txt src/Billiards/physics_scenario.h src/Billiards/physics_scenario.cpp \
  src/Billiards/automation_controller.cpp tests/physics_scenario_tests.cpp \
  tests/physics_validation/scenarios
git commit -m "feat: add shared versioned physics scenarios"
```

---

### Task 5: Build the analyzer, reports, and strict known-failure accounting

**Files:**
- Create: `tools/physics_validation/__init__.py`
- Create: `tools/physics_validation/analyzer.py`
- Create: `tools/physics_validation/report.py`
- Create: `tools/physics_validation/run.py`
- Create: `tests/physics_validation/test_analyzer.py`
- Create: `tests/physics_validation/test_report.py`
- Create: `tests/physics_validation/known_failures.json`
- Create: `tests/e2e/test_physics_validation.py`
- Modify: `tests/e2e/automation_client.py`
- Modify: `CMakeLists.txt`

**Interfaces:**
- Produces `analyze_scenario(scenario, frames) -> ScenarioResult` and failure codes `NUMERICAL_FAILURE`, `MODEL_MISMATCH`, `NON_DETERMINISTIC`, `INTEGRATION_MISMATCH`, `REFERENCE_LIMITATION`.
- CLI: `python3 -m tools.physics_validation.run --executable PATH --scenarios DIR --output DIR --known-failures FILE`.
- Exit `0` only when every scenario passes or fails exactly as declared by the strict known-failure manifest; reports still preserve declared cases as failed physics results.

- [ ] **Step 1: Write failing analyzer unit tests**

Use small in-memory traces to assert:

```python
def test_energy_increase_is_numerical_failure(self):
    result = analyze_scenario(
        scenario("nonincreasing_translational_energy"),
        [frame(1, energy=1.0), frame(2, energy=1.01)])
    self.assertFalse(result.passed)
    self.assertEqual(result.failures[0].code, "NUMERICAL_FAILURE")

def test_nonfinite_state_is_numerical_failure(self):
    result = analyze_scenario(scenario("finite_state"), [frame(1, x=float("nan"))])
    self.assertEqual(result.failures[0].metric, "finite_state")

def test_experimental_tolerance_is_model_mismatch(self):
    result = analyze_scenario(
        scenario("final_speed_cm_s", value=90.0, absolute_tolerance=2.0),
        [frame(1, speed=95.0)])
    self.assertEqual(result.failures[0].code, "MODEL_MISMATCH")
```

Also test missing A/B reference data becomes `REFERENCE_LIMITATION`, identical traces pass determinism comparison, a changed value becomes `NON_DETERMINISTIC`, and core/process result disagreement becomes `INTEGRATION_MISMATCH`.

- [ ] **Step 2: Verify RED**

Run:

```bash
python3 -m unittest discover -s tests/physics_validation -p 'test_*.py' -v
```

Expected: import failure because `tools.physics_validation` does not exist.

- [ ] **Step 3: Implement typed analysis results**

Define immutable dataclasses:

```python
@dataclass(frozen=True)
class Failure:
    code: str
    metric: str
    message: str
    expected: object
    actual: object

@dataclass(frozen=True)
class ScenarioResult:
    scenario_id: str
    passed: bool
    evidence_grade: str
    metrics: dict
    failures: tuple
```

Implement the phase-1 metrics `finite_state`, `nonincreasing_translational_energy`, `maximum_penetration_cm`, `contact_count`, `final_velocity_cm_s`, `final_speed_cm_s`, and `trace_equal`. Reject missing fields rather than silently defaulting them. Apply absolute tolerance first and relative tolerance as `abs(expected) * relative_tolerance`; a numeric comparison passes when error is within the larger explicitly declared bound.

- [ ] **Step 4: Implement deterministic process execution and pagination**

For each scenario, start one headless process, load the canonical scenario, enable tracing, step the declared count, fetch all pages until `has_more` is false, and save raw trace JSON. Run each deterministic scenario twice in fresh processes and compare canonical JSON with sorted keys and compact separators.

Never sleep. Use the existing response timeout and fail with `INTEGRATION_MISMATCH` if the process exits, returns a protocol error, skips ticks, reports dropped frames, or emits fewer frames than requested.

- [ ] **Step 5: Implement strict known-failure matching**

Create `known_failures.json` with exactly these initial entries:

```json
{
  "schema_version": 1,
  "failures": [
    {"scenario_id": "receding_overlap_v1", "code": "NUMERICAL_FAILURE", "metric": "unexpected_ball_ball_impulse", "source": "src/Billiards/physics.cpp:collideBalls", "remove_when": "separating contacts receive no collision impulse"},
    {"scenario_id": "high_speed_tunneling_v1", "code": "NUMERICAL_FAILURE", "metric": "missed_collision", "source": "src/Billiards/physics.cpp:updatePhysics", "remove_when": "continuous collision detection or bounded substeps detect the impact"},
    {"scenario_id": "reverse_cradle_v1", "code": "NON_DETERMINISTIC", "metric": "permutation_invariance", "source": "src/Billiards/physics.cpp:updatePhysics", "remove_when": "multi-contact solving is independent of ball iteration order"}
  ]
}
```

CI succeeds only when the actual set of `(scenario_id, code, metric)` tuples exactly equals the manifest. A new failure fails CI. An unexpected pass also fails CI and instructs the implementer to verify the fix and delete the matching manifest entry. The generated report always displays these scenarios under `FAILED (KNOWN)`, never `PASSED` or `SKIPPED`.

- [ ] **Step 6: Write JSON and Markdown reports**

The JSON report includes build identifier, scenario source path, raw-trace path, evidence grade, metrics, tolerances, failures, and known/unknown status. The Markdown summary contains counts for passed, failed-known, failed-new, and reference-limited cases, followed by one table row per scenario. Sort by scenario id for stable diffs.

Unit tests assert exact JSON keys, stable Markdown ordering, and that `REFERENCE_LIMITATION` is never included in the passed count.

- [ ] **Step 7: Add process E2E and verify**

Register `BilliardsPhysicsValidationE2E` with a 60-second timeout and run:

```bash
cmake --build build/check --parallel
python3 -m unittest discover -s tests/physics_validation -p 'test_*.py' -v
ctest --test-dir build/check --output-on-failure -R BilliardsPhysicsValidationE2E
```

Expected: unit tests pass; E2E exits 0 only because the three actual failures exactly match the manifest, while its Markdown report lists all three as `FAILED (KNOWN)`.

- [ ] **Step 8: Commit the acceptance runner**

```bash
git add CMakeLists.txt tools/physics_validation tests/physics_validation \
  tests/e2e/automation_client.py tests/e2e/test_physics_validation.py
git commit -m "test: add physics acceptance runner"
```

---

### Task 6: Integrate the foundation into repository checks and document operation

**Files:**
- Modify: `scripts/check.sh`
- Modify: `.github/workflows/ci.yml`
- Create: `docs/physics-validation.md`
- Modify: `README.md`

**Interfaces:**
- Adds CTest label `physics-validation` and CI artifact directory `${BILLIARDGL_BUILD_DIR}/physics-validation-report`.
- Documents the exact workflow for adding evidence, reproducing a failure, and removing a known-failure entry after a real fix.

- [ ] **Step 1: Write the operator documentation before wiring CI**

Document these commands verbatim:

```bash
./scripts/check.sh
python3 -m tools.physics_validation.run \
  --executable build/check/Billiards \
  --scenarios tests/physics_validation/scenarios \
  --known-failures tests/physics_validation/known_failures.json \
  --output build/check/physics-validation-report
```

Document units, schema versioning, evidence grades A/B/C, trace retention, pagination, same-build determinism, error classifications, and the rule that known failures remain visibly failed. Include a reproduction example that passes one scenario path and writes its raw trace.

- [ ] **Step 2: Add the validation run to local checks**

After the existing non-rendered CTest command in `scripts/check.sh`, invoke the runner with the built executable and repository paths. Use `${BUILD_DIR}/physics-validation-report` as output. The check must fail for any new failure, missing expected failure, unexpected pass, dropped frame, or report-generation error.

- [ ] **Step 3: Upload reports from GitHub Actions even on failure**

Add this step after `Configure, build, and test`:

```yaml
      - name: Upload physics validation report
        if: always()
        uses: actions/upload-artifact@v4
        with:
          name: physics-validation-report
          path: ${{ runner.temp }}/billiardgl-build/physics-validation-report
          if-no-files-found: error
```

- [ ] **Step 4: Run the complete fresh verification**

Run:

```bash
rm -rf build/physics-validation-plan-check
BILLIARDGL_BUILD_DIR="$PWD/build/physics-validation-plan-check" ./scripts/check.sh
git diff --check
git status --short
```

Expected:

- encoding checks pass;
- configure and build exit 0;
- all non-rendered CTest tests pass;
- analyzer unit and process E2E tests pass;
- report contains one passing analytic free-roll scenario and exactly three `FAILED (KNOWN)` scenarios;
- no new or missing known failures;
- `git diff --check` reports no whitespace errors.

- [ ] **Step 5: Commit CI and documentation**

```bash
git add scripts/check.sh .github/workflows/ci.yml docs/physics-validation.md README.md
git commit -m "ci: publish physics validation reports"
```

## Phase-1 Completion Gate

Do not start public-data digitization or physics model changes until all of these are demonstrated:

- the same canonical fixture runs through the direct C++ and process E2E paths;
- every requested tick produces exactly one authoritative frame with no dropped records;
- repeated same-build runs serialize identically;
- all new failures fail CI;
- all removed failures also fail CI until their manifest entries are deliberately deleted;
- known failures remain visibly failed in JSON and Markdown;
- the complete repository check passes from a clean build directory.

After this gate, write the phase-2 plan for the Mathavan, Doménech, cushion, and cue-impact public datasets. That plan must specify exact figures/tables, licensing, digitization uncertainty, and calibration/holdout allocation before adding numeric reference points.
