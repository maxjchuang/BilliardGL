# Phase 3 v2 Executable Full-Game Acceptance Implementation Plan

**Execution status: Completed and archived.** Unchecked boxes below preserve the original execution script; current disposition and evidence are indexed in [Phase 3 current status](../../phase3-physics-current-status.md).

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Make every advertised full-game case executable through user-equivalent actions, enforce gameplay/physics/performance invariants, and preserve canonical summaries plus full numeric traces.

**Architecture:** Split the stress executable into a strict CLI, a registry of deterministic case functions, and an artifact writer. Cases drive `GameRuntime` through the same action API as automation; shared invariant collectors hash canonical tick state and emit JSON traces, while matrix execution aggregates only already-written case summaries into CSV indexes.

**Tech Stack:** C++17, existing `GameRuntime`/automation actions, JSON, CMake/CTest, Python `unittest` artifact validators.

## Global Constraints

- Supported commands are exactly `--list-cases`, `--case <id> --seed <uint32> [--write <directory>]`, and `--matrix <path> --write <directory>`.
- Unknown cases, missing seeds, duplicate case IDs, and unused arguments fail with non-zero status.
- Every case writes canonical summary JSON and complete frame/contact/solver trace JSON; CSV is derived only.
- Physics state and gameplay events must match at common tick boundaries across cadence/load variants.
- Each task below ends in one independently reviewable commit.

---

### Task 1: Build a strict case registry and CLI dispatcher

**Files:**
- Create: `tests/full_game_case_registry.h`
- Create: `tests/full_game_case_registry.cpp`
- Create: `tests/full_game_cli.h`
- Create: `tests/full_game_cli.cpp`
- Modify: `tests/full_game_stress.cpp`
- Modify: `CMakeLists.txt`
- Create: `tests/full_game_cli_tests.cpp`

**Interfaces:**
- Produces: `parseFullGameCommand(argc, argv) -> FullGameCommand`, `fullGameCases() -> const vector<FullGameCase>&`, and `findFullGameCase(id)`.
- Case signature: `FullGameCaseResult run(GameRuntime&, uint32_t seed, const FullGameRunOptions&)`.

- [ ] **Step 1: Write failing parser/registry tests**

```cpp
const FullGameCommand list = parseArgs({"full-game-stress", "--list-cases"});
expect(list.mode == FullGameMode::ListCases, "list mode parses");

const FullGameCommand one = parseArgs({
    "full-game-stress", "--case", "cue_center_hit", "--seed", "42"});
expect(one.caseId == "cue_center_hit" && one.seed == 42u, "single case parses");

expectThrows({"full-game-stress", "--case", "missing"}, "missing --seed");
expectThrows({"full-game-stress", "--case", "unknown", "--seed", "1"}, "unknown case");
expectThrows({"full-game-stress", "--list-cases", "unused"}, "unused argument");

const auto ids = fullGameCaseIds();
expect(ids.size() == std::set<std::string>(ids.begin(), ids.end()).size(),
       "case IDs are unique");
```

- [ ] **Step 2: Build and observe the missing CLI modules**

Run: `cmake --build build --target full-game-cli-tests -j2`

Expected: compile failure because the registry/parser files do not exist.

- [ ] **Step 3: Implement exact parsing and case lookup**

```cpp
enum class FullGameMode { ListCases, SingleCase, Matrix };

struct FullGameCommand {
    FullGameMode mode = FullGameMode::ListCases;
    std::string caseId;
    std::uint32_t seed = 0;
    bool hasSeed = false;
    std::filesystem::path matrixPath;
    std::filesystem::path writeDirectory;
};

struct FullGameCase {
    std::string id;
    FullGameCaseResult (*run)(GameRuntime&, std::uint32_t,
                              const FullGameRunOptions&);
};
```

Parse arguments once, consume each token exactly once, parse seeds with `std::from_chars`, reject overflow/negative text, require `--write` for matrix mode, and sort `--list-cases` output lexically. Replace the old positional seed/cadence/load loop in `main` with registry dispatch.

- [ ] **Step 4: Run CLI tests and smoke each command form**

Run: `cmake --build build --target full-game-cli-tests full-game-stress -j2 && ctest --test-dir build -R full-game-cli --output-on-failure && build/full-game-stress --list-cases && ! build/full-game-stress --case cue_center_hit`

Expected: tests pass, list is stable, and missing seed exits non-zero.

- [ ] **Step 5: Commit the executable case interface**

```bash
git add tests/full_game_case_registry.h tests/full_game_case_registry.cpp tests/full_game_cli.h tests/full_game_cli.cpp tests/full_game_stress.cpp tests/full_game_cli_tests.cpp CMakeLists.txt
git commit -m "feat: add executable full-game case dispatcher"
```

### Task 2: Add canonical artifacts and shared physics invariants

**Files:**
- Create: `tests/full_game_artifacts.h`
- Create: `tests/full_game_artifacts.cpp`
- Create: `tests/full_game_invariants.h`
- Create: `tests/full_game_invariants.cpp`
- Modify: `tests/full_game_stress.cpp`
- Create: `tests/full_game_artifact_tests.cpp`
- Modify: `tests/physics_validation/test_full_game_stress_artifact.py`

**Interfaces:**
- Produces: `writeFullGameArtifacts(result, directory)`, `evaluateFullGameInvariants(frames, events, budgets)`, canonical SHA-256 state hash, `summary.json`, `trace.json`, and derived `index.csv`.
- Consumes: complete `PhysicsFrame`, gameplay event stream, wall-clock peak RSS sampler, and configured numeric limits.

- [ ] **Step 1: Write failing complete-trace and invariant tests**

```cpp
const FullGameCaseResult result = fixtureResult();
writeFullGameArtifacts(result, directory);
const json::Value summary = readJson(directory / "summary.json");
const json::Value trace = readJson(directory / "trace.json");
expect(summary["frame_count"].integer() == trace["frames"].size(), "all frames retained");
expect(summary["dropped_trace_frames"].integer() == 0, "no dropped frames");
expect(summary["step_failures"].integer() == 0, "all physics steps succeed");
expect(summary["deterministic_hash"].string() == hashCanonicalFrames(trace),
       "summary binds complete trace");
```

- [ ] **Step 2: Build/run artifact tests and observe absent JSON writer**

Run: `cmake --build build --target full-game-artifact-tests -j2 && ctest --test-dir build -R full-game-artifact --output-on-failure`

Expected: compile failure for missing artifact/invariant modules.

- [ ] **Step 3: Implement canonical state, invariant, and artifact output**

```cpp
struct FullGameBudgets {
    double maximumPenetrationCm = 0.5;
    double maximumResidualCmS = 0.001;
    double passiveEnergyToleranceJ = 1e-10;
    double maximumWallSeconds = 30.0;
    std::uint64_t maximumPeakRssBytes = 512ull * 1024ull * 1024ull;
};

struct FullGameInvariantResult {
    bool passed = false;
    std::vector<std::string> failures;
    std::string deterministicHash;
};
```

Canonicalize every frame by fixed field order and `std::to_chars` round-trip precision. Include all ball position/velocity/acceleration/angular velocity/motion/pocket state, contact impulses/IDs, solver status/residual/penetration/iterations/energy, control action, gameplay events, profile ID, tick, and simulation time. Reject non-finite values, failed steps, duplicate `(event_id,island_id,contact identity)` impulses, budget excess, gameplay expectation mismatch, and dropped frames. Write to a temporary file, flush/close, then atomically rename.

- [ ] **Step 4: Run C++ and Python artifact validators**

Run: `cmake --build build --target full-game-artifact-tests full-game-stress -j2 && ctest --test-dir build -R full-game-artifact --output-on-failure && python3 -m unittest tests.physics_validation.test_full_game_stress_artifact -v`

Expected: all tests pass and Python independently recomputes the same trace hash.

- [ ] **Step 5: Commit complete full-game artifacts**

```bash
git add tests/full_game_artifacts.h tests/full_game_artifacts.cpp tests/full_game_invariants.h tests/full_game_invariants.cpp tests/full_game_stress.cpp tests/full_game_artifact_tests.cpp tests/physics_validation/test_full_game_stress_artifact.py
git commit -m "feat: preserve complete full-game physics traces"
```

### Task 3: Implement six integrated component cases

**Files:**
- Create: `tests/full_game_component_cases.cpp`
- Modify: `tests/full_game_case_registry.cpp`
- Create: `tests/full_game_component_cases_tests.cpp`
- Create: `physics_models/promotion/full_game_matrix_v2.json`

**Interfaces:**
- Produces cases `cue_center_hit`, `cue_near_miscue`, `sliding_to_rolling`, `oblique_ball_collision`, `rail_rebound`, and `side_pocket_capture`.
- Consumes: `GameRuntime::applyAction`, scenario setup API, `waitUntilStationary`, and artifact/invariant collectors.

- [ ] **Step 1: Write failing registry and physical-event assertions**

```cpp
for (const std::string& id : {
        "cue_center_hit", "cue_near_miscue", "sliding_to_rolling",
        "oblique_ball_collision", "rail_rebound", "side_pocket_capture"}) {
    expect(findFullGameCase(id) != nullptr, id + " is registered");
    const FullGameCaseResult result = runCaseTwice(id, 7u);
    expect(result.passed, id + " passes invariants");
    expect(result.firstHash == result.secondHash, id + " is deterministic");
}
expect(runCase("sliding_to_rolling", 7).surfaceTransitions >= 1,
       "surface transition is observed");
expect(runCase("side_pocket_capture", 7).objectBallCaptures == 1,
       "side pocket capture is observed");
```

- [ ] **Step 2: Run component-case tests and observe unregistered cases**

Run: `cmake --build build --target full-game-component-cases-tests -j2 && ctest --test-dir build -R full-game-component --output-on-failure`

Expected: failures because none of the six functions are registered.

- [ ] **Step 3: Implement user-equivalent actions and component expectations**

```cpp
ActionResult aimAndShoot(GameRuntime& runtime, float yawDegrees, float power,
                         float offsetX, float offsetY) {
    ActionResult result = runtime.applyAction(GameAction::setAimYaw(yawDegrees));
    if (!result.ok) return result;
    result = runtime.applyAction(GameAction::setCueOffset(offsetX, offsetY));
    if (!result.ok) return result;
    result = runtime.applyAction(GameAction::setShotPower(power));
    if (!result.ok) return result;
    return runtime.applyAction(GameAction::shoot());
}
```

Each case initializes a deterministic legal layout, uses only the public action API to aim/power/shoot, advances fixed ticks until `shotEnded`, and asserts its named event: central cue normal response, bounded near-miscue, sliding→rolling transition, one oblique solver event, one straight-rail rebound, or one side-pocket capture. Add v2 matrix entries with fixed seed, expected gameplay outcome, and exact coverage tags.

- [ ] **Step 4: Execute each case through the real CLI**

Run: `cmake --build build --target full-game-stress full-game-component-cases-tests -j2 && for id in cue_center_hit cue_near_miscue sliding_to_rolling oblique_ball_collision rail_rebound side_pocket_capture; do build/full-game-stress --case "$id" --seed 7 --write "build/full-game/$id" || exit 1; done`

Expected: six zero exit codes, six summaries, six full traces, and deterministic reruns.

- [ ] **Step 5: Commit integrated component cases**

```bash
git add tests/full_game_component_cases.cpp tests/full_game_case_registry.cpp tests/full_game_component_cases_tests.cpp physics_models/promotion/full_game_matrix_v2.json
git commit -m "test: execute integrated physics component cases"
```

### Task 4: Implement scoring, scratch, break, and randomized gameplay cases

**Files:**
- Create: `tests/full_game_gameplay_cases.cpp`
- Create: `tests/full_game_prng.h`
- Modify: `tests/full_game_case_registry.cpp`
- Create: `tests/full_game_gameplay_cases_tests.cpp`
- Modify: `physics_models/promotion/full_game_matrix_v2.json`

**Interfaces:**
- Produces: `seeded_break`, `continuous_scoring`, `cue_ball_scratch`, and `randomized_legal_sequence` using committed xorshift32 PRNG.
- Consumes: operational legality predicate and gameplay events for scoring, foul, turn, capture, shot end, and game over.

- [ ] **Step 1: Write failing gameplay-transition tests**

```cpp
const auto scoring = runCase("continuous_scoring", 100u);
expect(scoring.objectBallCaptures >= 3, "three objects are captured");
expect(scoring.scoreEvents >= 3, "score updates follow captures");

const auto scratch = runCase("cue_ball_scratch", 101u);
expect(scratch.cueBallCaptures == 1, "cue ball enters pocket");
expect(scratch.foulEvents == 1 && scratch.turnTransfers == 1,
       "scratch causes foul and turn transfer");

const auto random = runCase("randomized_legal_sequence", 0x12345678u);
expect(random.illegalActionAttempts == 0, "only operationally legal actions emit");
expect(random.completedShots == random.declaredShots || random.gameOver,
       "sequence reaches its declared terminal condition");
```

- [ ] **Step 2: Run gameplay tests and observe missing cases**

Run: `cmake --build build --target full-game-gameplay-cases-tests -j2 && ctest --test-dir build -R full-game-gameplay --output-on-failure`

Expected: failures because the four gameplay cases are absent.

- [ ] **Step 3: Implement deterministic legal action generation**

```cpp
class XorShift32 {
public:
    explicit XorShift32(std::uint32_t seed) : state_(seed == 0 ? 0x6d2b79f5u : seed) {}
    std::uint32_t next() {
        state_ ^= state_ << 13;
        state_ ^= state_ >> 17;
        state_ ^= state_ << 5;
        return state_;
    }
    float unit() { return static_cast<float>(next()) / 4294967295.0f; }
private:
    std::uint32_t state_;
};

bool operationallyLegal(const GameRuntime& runtime) {
    return runtime.allBallsStationary() && !runtime.gameOver() &&
           runtime.cueBallActive() && !runtime.shotPending() &&
           runtime.currentPlayerMayShoot();
}
```

Use a real rack and a public aim/power/shoot sequence for `seeded_break`. Use deterministic legal layouts and three separate public shots for `continuous_scoring`. Aim the cue ball into a side pocket for `cue_ball_scratch`. In the randomized case, wait for operational legality before each action, generate yaw/power/offset from xorshift32, record every generated action, accept gameplay fouls as outcomes, and assert the corresponding turn/event stream.

- [ ] **Step 4: Execute all gameplay cases and compare rerun hashes**

Run: `cmake --build build --target full-game-stress full-game-gameplay-cases-tests -j2 && ctest --test-dir build -R full-game-gameplay --output-on-failure && for id in seeded_break continuous_scoring cue_ball_scratch randomized_legal_sequence; do build/full-game-stress --case "$id" --seed 305419896 --write "build/full-game/$id" || exit 1; done`

Expected: all cases pass and a second execution produces identical canonical hashes.

- [ ] **Step 5: Commit full-game gameplay cases**

```bash
git add tests/full_game_gameplay_cases.cpp tests/full_game_prng.h tests/full_game_case_registry.cpp tests/full_game_gameplay_cases_tests.cpp physics_models/promotion/full_game_matrix_v2.json
git commit -m "test: cover deterministic full-game transitions"
```

### Task 5: Add cadence/load equivalence, matrix execution, and frozen budgets

**Files:**
- Create: `tests/full_game_equivalence_cases.cpp`
- Modify: `tests/full_game_case_registry.cpp`
- Modify: `tests/full_game_stress.cpp`
- Create: `tests/full_game_equivalence_tests.cpp`
- Modify: `tools/physics_validation/promotion.py`
- Modify: `tests/physics_validation/test_full_game_matrix.py`
- Modify: `tests/physics_validation/test_full_game_performance.py`
- Create: `physics_models/promotion/full_game_performance_budget_v2.json`

**Interfaces:**
- Produces: `cadence_equivalence`, `host_load_equivalence`, matrix runner, and common-tick comparator.
- Consumes: the same recorded action stream, fixed simulation tick, different render cadence/host workload, and pre-freeze wall/RSS budgets.

- [ ] **Step 1: Write failing common-tick and matrix-completeness tests**

```cpp
const auto cadence = runCase("cadence_equivalence", 77u);
expect(cadence.stateMismatches == 0 && cadence.eventMismatches == 0,
       "render cadence changes wall time only");
const auto load = runCase("host_load_equivalence", 77u);
expect(load.stateMismatches == 0 && load.eventMismatches == 0,
       "host load changes wall time only");
```

```python
def test_v2_matrix_executes_every_required_case(self):
    document = json.loads(MATRIX.read_text(encoding="utf-8"))
    self.assertEqual({case["id"] for case in document["cases"]}, REQUIRED_CASES)
    self.assertTrue(all(case["replay"].startswith("full-game-stress --case ")
                        for case in document["cases"]))
```

- [ ] **Step 2: Run equivalence/matrix tests and observe missing execution paths**

Run: `cmake --build build --target full-game-equivalence-tests -j2 && ctest --test-dir build -R full-game-equivalence --output-on-failure && python3 -m unittest tests.physics_validation.test_full_game_matrix tests.physics_validation.test_full_game_performance -v`

Expected: failures because equivalence cases and schema-v2 matrix rules are absent.

- [ ] **Step 3: Implement action replay and strict matrix aggregation**

```cpp
struct TickSnapshot {
    std::uint64_t tick = 0;
    std::string physicsStateHash;
    std::string gameplayEventHash;
};

EquivalenceResult compareCommonTicks(
    const std::vector<TickSnapshot>& first,
    const std::vector<TickSnapshot>& second) {
    EquivalenceResult result;
    for (std::size_t i = 0; i < first.size(); ++i) {
        if (first[i].tick != second[i].tick ||
            first[i].physicsStateHash != second[i].physicsStateHash)
            ++result.stateMismatches;
        if (first[i].gameplayEventHash != second[i].gameplayEventHash)
            ++result.eventMismatches;
    }
    return result;
}
```

Replay one recorded action stream at two render cadences and under a deterministic CPU-work callback; compare every common fixed tick. Matrix mode rejects duplicate IDs before execution, invokes each registered case once with its fixed seed, stops on first failure, and writes a matrix summary plus derived CSV after all per-case JSON files are safely closed. Budget v2 records maximum wall seconds and peak RSS per case and is committed before candidate freeze.

- [ ] **Step 4: Execute the complete v2 matrix twice**

Run: `cmake --build build --target full-game-stress full-game-equivalence-tests -j2 && ctest --test-dir build -R 'full-game-(cli|artifact|component|gameplay|equivalence)' --output-on-failure && build/full-game-stress --matrix physics_models/promotion/full_game_matrix_v2.json --write build/full-game/matrix-a && build/full-game-stress --matrix physics_models/promotion/full_game_matrix_v2.json --write build/full-game/matrix-b`

Expected: all twelve cases pass; corresponding case hashes match; wall time may differ; RSS/wall values remain within v2 budgets.

- [ ] **Step 5: Commit matrix equivalence and budgets**

```bash
git add tests/full_game_equivalence_cases.cpp tests/full_game_case_registry.cpp tests/full_game_stress.cpp tests/full_game_equivalence_tests.cpp tools/physics_validation/promotion.py tests/physics_validation/test_full_game_matrix.py tests/physics_validation/test_full_game_performance.py physics_models/promotion/full_game_matrix_v2.json physics_models/promotion/full_game_performance_budget_v2.json
git commit -m "test: enforce executable full-game acceptance matrix"
```

## Plan Verification

- Run `ctest --test-dir build -R 'full-game-' --output-on-failure`.
- Run `python3 -m unittest tests.physics_validation.test_full_game_matrix tests.physics_validation.test_full_game_stress_artifact tests.physics_validation.test_full_game_performance -v`.
- Execute `build/full-game-stress --matrix physics_models/promotion/full_game_matrix_v2.json --write build/full-game/final` and verify twelve summary/trace pairs, zero dropped frames, zero failed physics steps, and all budgets pass.
