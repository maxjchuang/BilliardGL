# Phase 3 v4 Confirmation Recovery Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Build a physics-identical Phase 3 v4 candidate whose confirmation scenarios traverse the real automation path, replace spent Derby evidence with Alciatore TP A.15, and accept the candidate only after separately authorized Alciatore and Han confirmations pass.

**Architecture:** A registry maps each confirmation package to one adapter that creates schema-valid scenarios and evaluates complete traces. Confirmation transactions remain fail-closed and transport-neutral; a synthetic package proves the parser/executable path before freeze, while real package predictions remain unopened until their one-time authorized runs.

**Tech Stack:** Python 3 standard library and `unittest`, C++17 scenario/automation executable, CMake/CTest, canonical JSON and CSV artifacts, Git lifecycle evidence.

## Global Constraints

- Preserve every committed `phase3_integrated_v3` artifact byte-for-byte.
- Transition `derby_fuller_1999` 1.0.0 from `confirmation` to `spent`; never execute it again.
- `phase3_integrated_v4` and `chinese_pool_full_game_v4` must have physics formulas and parameter values canonically identical to v3.
- Keep the C++ rule `expectations must be a nonempty array`; repair the generator, not the parser.
- Before v4 freeze, do not compute candidate predictions or residuals from `alciatore_2005_tp_a15` or `han_2005`.
- Commit all extracted numerical data and all generated scenarios, traces, metrics, logs, ledgers, and receipts; do not vendor third-party PDFs or videos.
- Alciatore acceptance is interior-angle RMSE `<= 3 degrees`, maximum absolute interior error `<= 5 degrees`, with the pre-registered endpoint invariants.
- Han acceptance remains normalized-curve RMSE `<= 0.15` plus its four existing hard invariants.
- Reservation consumes a confirmation partition even if parsing, execution, or evaluation subsequently fails.
- Steps that reserve Alciatore or Han require fresh, explicit user approval immediately before the command.
- Each task below ends in exactly one focused commit; do not combine task commits.

---

## File Structure

### New focused modules

- `tools/physics_validation/phase3_v4_lifecycle.py`: derive the immutable v3 rejection binding and Derby spent transition.
- `tools/physics_validation/confirmation_adapters.py`: adapter protocol, registry, common scenario primitives, and base expectations.
- `tools/physics_validation/confirmation_contract_fixture.py`: build and execute the synthetic real-path preflight without referring to real confirmation packages.
- `tools/physics_validation/extract_alciatore_2005_tp_a15.py`: deterministically generate and verify the complete public numeric package.
- `tools/physics_validation/alciatore_confirmation.py`: Alciatore scenario construction and point/aggregate evaluation only.
- `tools/physics_validation/han_confirmation.py`: Han scenario construction and normalized-trend evaluation only.
- `tools/physics_validation/build_v4_profile.py`: clone v3 physics exactly while changing only governed v4 identity and inventory metadata.
- `tools/physics_validation/phase3_v4_assessment.py`: bind both one-time receipts into the final acceptance/rejection document.

### Existing modules to modify

- `tools/physics_validation/confirmation_run.py`: delegate scenario and metric work to the adapter registry; preserve canonical artifact emission.
- `tools/physics_validation/confirmation_readiness.py`: support v4 and bind the synthetic contract proof.
- `tools/physics_validation/freeze_candidate.py`: admit the v4 candidate identifier.
- `tools/physics_validation/build_v3_fit_inputs.py`: reject Alciatore as confirmation evidence in addition to Derby and Han.
- `tests/physics_validation/validation_data_status.json`: mark Derby spent and admit Alciatore as confirmation.
- `src/Billiards/generated/phase3_v4_profile.inc`: generated v4 identity with v3 numeric physics.

### Generated governed artifacts

- `physics_models/promotion/phase3_integrated_v3_derby_spent_transition.json`
- `tests/physics_validation/reference_data/alciatore_2005_tp_a15/*`
- `physics_models/profiles/chinese_pool_full_game_v4.json`
- `physics_models/promotion/{phase3_candidates_v4,full_game_matrix_v4,full_game_performance_budget_v4}.json`
- `physics_models/candidates/phase3_integrated_v4/{freeze.json,full_game/,confirmation_readiness.json}`
- Authorized only: `physics_models/candidates/phase3_integrated_v4/confirmation_consumption.json`, `confirmation/*`, and `final_assessment.json`.

---

### Task 1: Spend Derby and Bind the Immutable v3 Failure

**Files:**
- Create: `tools/physics_validation/phase3_v4_lifecycle.py`
- Create: `tests/physics_validation/test_phase3_v4_lifecycle.py`
- Create: `physics_models/promotion/phase3_integrated_v3_derby_spent_transition.json`
- Modify: `tests/physics_validation/validation_data_status.json`
- Modify: `tests/physics_validation/test_phase3_v3_freeze.py`
- Modify: `tests/physics_validation/test_phase3_v2_confirmation.py`
- Modify: `tests/physics_validation/test_phase3_v2_source_packages.py`
- Modify: `tests/physics_validation/test_validation_run.py`

**Interfaces:**
- Consumes: committed v3 `freeze.json`, Derby `failure.json`, `validation_receipt.json`, `confirmation_consumption.json`, rejection, and package manifest.
- Produces: `build_derby_spent_transition(root: Path) -> dict`.

- [ ] **Step 1: Capture immutable v3 hashes and write the failing lifecycle test**

Verify the six pre-recorded constants without modifying files:

```bash
shasum -a 256 \
  physics_models/candidates/phase3_integrated_v3/freeze.json \
  physics_models/candidates/phase3_integrated_v3/confirmation/derby_fuller_1999/failure.json \
  physics_models/candidates/phase3_integrated_v3/confirmation/derby_fuller_1999/validation_receipt.json \
  physics_models/candidates/phase3_integrated_v3/confirmation_consumption.json \
  physics_models/promotion/phase3_integrated_v3_rejection.json \
  tests/physics_validation/reference_data/derby_fuller_1999/manifest.json
```

Create the test with these literal immutable values:

```python
V3_DIGESTS = {
    "freeze.json": "cc7cd5f2b583fc13550de1677fc5ea13c5460786794ef0bc7ff1364b834c9c08",
    "confirmation/derby_fuller_1999/failure.json": "07a3d014cdcfa2fb35ea4398769ca7848f6e0e30844735fe29e50538fee25c7b",
    "confirmation/derby_fuller_1999/validation_receipt.json": "c001f4693ab24cb50ef84b1434ab693355499b374cd388480064764a5b1d9ce7",
    "confirmation_consumption.json": "27239d6285f0cbf26593d5b9f49733882c4e627e0c675db00428a0b37cea3aa3",
}
REJECTION_SHA256 = "0c8a179288fe712f292f1f61047be2ba4ac8f2f8b3f3bc828c55b5eb247062b5"
DERBY_MANIFEST_SHA256 = "3e8c979e48e207317384dc63e47d4d5f28bae9b21cc8486183cd35ea9fe2b3d1"

class Phase3V4LifecycleTests(unittest.TestCase):
    def test_derby_is_spent_and_v3_evidence_is_immutable(self):
        entry = load_data_lifecycle(STATUS).entry("derby_fuller_1999", "1.0.0")
        self.assertEqual((entry.calibration_status, entry.holdout_status),
                         ("spent", "spent"))
        self.assertEqual({name: digest(V3 / name) for name in V3_DIGESTS},
                         V3_DIGESTS)

    def test_transition_binds_every_failed_attempt_artifact(self):
        transition = build_derby_spent_transition(ROOT)
        self.assertEqual(transition["from"], "confirmation")
        self.assertEqual(transition["to"], "spent")
        self.assertEqual(transition["failure_message"],
                         "invalid_scenario: expectations must be a nonempty array")
        self.assertEqual(set(transition["evidence_sha256"]), {
            "failure", "freeze", "ledger", "package_manifest", "receipt",
            "rejection",
        })
```

Replace the stale current-worktree absence assertions in
`test_phase3_v3_freeze.py` with `git cat-file -e` checks proving that the
frozen `SELECTED_REVISION` itself has neither confirmation output path. Keep
the source scan proving `rebuild_frozen.py` cannot open reference packages.
Move historical transaction-mechanics tests from the now-spent Derby package
to `fixtures/confirmation_transaction_v1` with a repository-local temporary
freeze and lifecycle registry. Update historical lifecycle assertions to
expect Derby `spent`; never override Derby itself back to confirmation.

- [ ] **Step 2: Run the focused test and verify it fails**

Run:

```bash
python3 -m unittest tests.physics_validation.test_phase3_v4_lifecycle -v
```

Expected: `ERROR` importing `tools.physics_validation.phase3_v4_lifecycle` or `FAIL` because Derby remains `confirmation`.

- [ ] **Step 3: Implement the canonical transition builder**

Create this public function; use `_sha256(path)` for each path and validate the receipt/failure fields before returning:

```python
def build_derby_spent_transition(root):
    root = Path(root).resolve()
    candidate = root / "physics_models/candidates/phase3_integrated_v3"
    paths = {
        "freeze": candidate / "freeze.json",
        "failure": candidate / "confirmation/derby_fuller_1999/failure.json",
        "receipt": candidate / "confirmation/derby_fuller_1999/validation_receipt.json",
        "ledger": candidate / "confirmation_consumption.json",
        "rejection": root / "physics_models/promotion/phase3_integrated_v3_rejection.json",
        "package_manifest": root / "tests/physics_validation/reference_data/derby_fuller_1999/manifest.json",
    }
    receipt = json.loads(paths["receipt"].read_text(encoding="utf-8"))
    failure = json.loads(paths["failure"].read_text(encoding="utf-8"))
    if (receipt.get("candidate_id"), receipt.get("dataset_id"), receipt.get("result")) != \
            ("phase3_integrated_v3", "derby_fuller_1999", "FAILED"):
        raise ValueError("v3 Derby receipt is not the immutable failure")
    message = "invalid_scenario: expectations must be a nonempty array"
    if failure.get("message") != message:
        raise ValueError("v3 Derby failure does not match the diagnosed root cause")
    return {
        "schema_version": 1,
        "dataset_id": "derby_fuller_1999",
        "dataset_version": "1.0.0",
        "from": "confirmation",
        "to": "spent",
        "rejected_candidate": "phase3_integrated_v3",
        "failure_category": "SCENARIO_CONTRACT_INTEGRATION_FAILURE",
        "failure_message": message,
        "evidence_sha256": {key: _sha256(path) for key, path in sorted(paths.items())},
        "reason": "the sole Derby transaction is immutable and cannot be reused",
    }
```

Change both Derby lifecycle fields to `spent`, render the builder output as sorted, indented UTF-8 JSON with one trailing newline, and write it to the promotion path.

- [ ] **Step 4: Prove v3 immutability and canonical output**

Run:

```bash
python3 -m unittest tests.physics_validation.test_phase3_v4_lifecycle tests.physics_validation.test_phase3_v3_confirmation -v
git diff --check
```

Expected: all tests pass; the diff contains no path below `physics_models/candidates/phase3_integrated_v3/`.

- [ ] **Step 5: Commit the lifecycle transaction**

```bash
git add tools/physics_validation/phase3_v4_lifecycle.py \
  tests/physics_validation/test_phase3_v4_lifecycle.py \
  tests/physics_validation/test_phase3_v3_freeze.py \
  tests/physics_validation/test_phase3_v2_confirmation.py \
  tests/physics_validation/test_phase3_v2_source_packages.py \
  tests/physics_validation/test_validation_run.py \
  tests/physics_validation/validation_data_status.json \
  physics_models/promotion/phase3_integrated_v3_derby_spent_transition.json
git commit -m "data: spend failed phase 3 v3 Derby confirmation"
```

---

### Task 2: Introduce the Adapter Registry and Repair Scenario Expectations

**Files:**
- Create: `tools/physics_validation/confirmation_adapters.py`
- Create: `tests/physics_validation/test_confirmation_adapters.py`
- Modify: `tools/physics_validation/confirmation_run.py`
- Modify: `tests/physics_validation/test_phase3_v2_confirmation.py`

**Interfaces:**
- Produces: `ConfirmationAdapter`, `register_confirmation_adapter(adapter)`, `confirmation_adapter(dataset_id)`, `base_scenario(...)`, and `execute_deterministically(...)`.
- `ConfirmationAdapter.build_scenarios(profile: dict, package: ReferencePackage) -> dict[str, dict]`.
- `ConfirmationAdapter.evaluate(traces: dict[str, list[dict]], profile: dict, package: ReferencePackage) -> ConfirmationEvaluation`.
- `ConfirmationEvaluation` contains `rows: tuple[dict, ...]`, `summary_metrics: dict`, and `diagnostics: tuple[dict, ...]`.

- [ ] **Step 1: Write registry and contract tests first**

```python
def test_base_scenario_has_real_expectations(self):
    scenario = base_scenario(PROFILE, "fixture", BALLS, "unbounded", 2,
                             "contract fixture")
    self.assertEqual(scenario["expectations"], [
        {"metric": "finite_state", "operator": "eq", "value": True},
        {"metric": "nonincreasing_translational_energy", "operator": "eq",
         "value": True},
    ])

def test_unknown_adapter_fails_closed(self):
    with self.assertRaisesRegex(ValueError,
                                "unsupported confirmation package: unknown"):
        confirmation_adapter("unknown")

def test_duplicate_adapter_registration_fails(self):
    register_confirmation_adapter(FakeAdapter("fixture_unique"))
    with self.assertRaisesRegex(ValueError, "duplicate confirmation adapter"):
        register_confirmation_adapter(FakeAdapter("fixture_unique"))
```

Retain a regression assertion in `test_phase3_v2_confirmation.py` that every generated legacy Sudo/Derby scenario now has a nonempty expectations array.

- [ ] **Step 2: Verify the new tests fail**

```bash
python3 -m unittest tests.physics_validation.test_confirmation_adapters \
  tests.physics_validation.test_phase3_v2_confirmation -v
```

Expected: import failure for `confirmation_adapters` or an assertion showing `expectations == []`.

- [ ] **Step 3: Add the exact registry types and common scenario builder**

```python
@dataclass(frozen=True)
class ConfirmationEvaluation:
    rows: tuple
    summary_metrics: dict
    diagnostics: tuple = ()

@dataclass(frozen=True)
class ConfirmationAdapter:
    dataset_id: str
    build_scenarios: Callable[[dict, object], dict]
    evaluate: Callable[[dict, dict, object], ConfirmationEvaluation]

_ADAPTERS = {}

def register_confirmation_adapter(adapter):
    if adapter.dataset_id in _ADAPTERS:
        raise ValueError(f"duplicate confirmation adapter: {adapter.dataset_id}")
    _ADAPTERS[adapter.dataset_id] = adapter

def confirmation_adapter(dataset_id):
    try:
        return _ADAPTERS[dataset_id]
    except KeyError as error:
        raise ValueError(
            f"unsupported confirmation package: {dataset_id}") from error

def base_scenario(profile, scenario_id, balls, boundary_mode, ticks,
                  description, *, evidence_source="confirmation_contract"):
    return {
        "schema_version": 11,
        "id": scenario_id,
        "description": description,
        "boundary_mode": boundary_mode,
        "physics_profile": copy.deepcopy(profile),
        "balls": balls,
        "simulation": {"ticks": ticks,
                       "time_step_seconds": profile["solver"]["time_step_seconds"]},
        "expectations": [
            {"metric": "finite_state", "operator": "eq", "value": True},
            {"metric": "nonincreasing_translational_energy", "operator": "eq",
             "value": True},
        ],
        "evidence": {"equipment": "WPA_POOL", "grade": "B",
                     "pool_applicability": "DIRECT", "source": evidence_source},
    }
```

Move deterministic double execution into `execute_deterministically` without changing its byte-equality and frame-count checks. Register the existing Sudo and Derby builders/evaluators so legacy tests remain executable, but do not add v4 packages yet.

- [ ] **Step 4: Delegate the runner to the registry**

Replace the dataset `if` statements in `build_confirmation_result` with:

```python
adapter = confirmation_adapter(dataset_id)
scenarios = adapter.build_scenarios(profile, package)
traces = execute_deterministically(
    executable, scenarios, execute_once or _execute_once)
evaluation = adapter.evaluate(traces, profile, package)
rows = list(evaluation.rows)
```

Add `summary_metrics` to `reference_report.json`, retain the existing complete scenario/trace/provenance files, and continue returning only `PASSED_OR_ACCOUNTED` when there are no `FAILED` rows and all summary gates pass.

- [ ] **Step 5: Run focused and legacy transaction tests**

```bash
python3 -m unittest \
  tests.physics_validation.test_confirmation_adapters \
  tests.physics_validation.test_phase3_v2_confirmation \
  tests.physics_validation.test_confirmation_transaction -v
```

Expected: all pass, including the old Sudo/Derby evaluator behavior.

- [ ] **Step 6: Commit the shared contract repair**

```bash
git add tools/physics_validation/confirmation_adapters.py \
  tools/physics_validation/confirmation_run.py \
  tests/physics_validation/test_confirmation_adapters.py \
  tests/physics_validation/test_phase3_v2_confirmation.py
git commit -m "fix: enforce confirmation scenario expectations"
```

---

### Task 3: Prove the Real Parser and Automation Path Before Freeze

**Files:**
- Create: `tools/physics_validation/confirmation_contract_fixture.py`
- Create: `tests/physics_validation/test_confirmation_contract_fixture.py`
- Modify: `tests/e2e/automation_client.py`
- Modify: `tools/physics_validation/run.py`
- Modify: `tests/physics_validation/fixtures/confirmation_transaction_v1/scenario_template.json`
- Modify: `tools/physics_validation/confirmation_readiness.py`

**Interfaces:**
- Produces: `build_contract_proof(root: Path, executable: Path, fixture: Path) -> dict[str, bytes]`.
- Produces: `ExecutionEvidence(frames, protocol_transcript, stderr, return_code)` from `_execute_once_with_evidence(executable, scenario)` while preserving `_execute_once(...) -> list[dict]` for existing callers.
- Readiness consumes committed `confirmation_contract_proof.json` and verifies its executable, manifest, scenario, and trace hashes.

- [ ] **Step 1: Write a test that invokes the actual executable**

```python
def test_fixture_traverses_real_parser_and_runner(self):
    files = build_contract_proof(ROOT, Path(os.environ["BILLIARDGL_EXECUTABLE"]),
                                 FIXTURE)
    proof = json.loads(files["confirmation_contract_proof.json"])
    self.assertEqual(proof["result"], "PASSED")
    self.assertTrue(proof["parse_succeeded"])
    self.assertGreater(proof["frames"], 0)
    self.assertEqual(proof["first_trace_sha256"],
                     proof["second_trace_sha256"])
    self.assertEqual(proof["return_code"], 0)
    self.assertEqual(proof["stderr"], "")
    self.assertTrue(proof["protocol_transcript_sha256"])

def test_empty_expectations_reproduces_v3_failure(self):
    with self.assertRaisesRegex(RuntimeError,
                                "expectations must be a nonempty array"):
        build_contract_proof(ROOT, EXECUTABLE, FIXTURE,
                             mutate=lambda scenario: scenario.update(
                                 expectations=[]))
```

- [ ] **Step 2: Build the test executable and verify RED**

```bash
cmake -S . -B build-v4-plan -DCMAKE_BUILD_TYPE=Release
cmake --build build-v4-plan --target Billiards -j2
BILLIARDGL_EXECUTABLE="$PWD/build-v4-plan/Billiards/Billiards" \
  python3 -m unittest tests.physics_validation.test_confirmation_contract_fixture -v
```

Expected: import failure for `confirmation_contract_fixture`.

- [ ] **Step 3: Capture the real stdio protocol and process outcome**

Initialize `AutomationClient.transcript` before reading the ready event. Append
canonical `{direction: "stdin", message: ...}` and
`{direction: "stdout", message: ...}` entries around every request/read. After
`close()`, set `return_code` and read the remaining UTF-8 stderr into
`stderr_text`. Add this immutable result type in `run.py`:

```python
@dataclass(frozen=True)
class ExecutionEvidence:
    frames: tuple
    protocol_transcript: tuple
    stderr: str
    return_code: int

def _execute_once(executable, scenario):
    return list(_execute_once_with_evidence(executable, scenario).frames)
```

`_execute_once_with_evidence` performs the same ready/capability/load/trace/step
operations as the old function, exits the client context, and then returns all
four fields. Existing validation behavior must remain unchanged.

- [ ] **Step 4: Implement the synthetic adapter and proof**

The fixture adapter must call `base_scenario`, then call the existing
`tools.physics_validation.run._execute_once` twice. Canonicalize each trace and
return these exact proof fields:

```python
proof = {
    "schema_version": 1,
    "dataset_id": "confirmation_transaction_fixture",
    "result": "PASSED",
    "parse_succeeded": True,
    "frames": len(first),
    "executable_sha256": sha256(executable.read_bytes()).hexdigest(),
    "fixture_manifest_sha256": file_sha256(fixture / "manifest.json"),
    "scenario_sha256": sha256(canonical(scenario)).hexdigest(),
    "first_trace_sha256": sha256(canonical(first)).hexdigest(),
    "second_trace_sha256": sha256(canonical(second)).hexdigest(),
    "protocol_transcript_sha256": sha256(canonical(evidence.protocol_transcript)).hexdigest(),
    "stderr": evidence.stderr,
    "return_code": evidence.return_code,
}
```

The mutation hook exists only in the test helper and runs before execution.
Do not import or name Derby, Alciatore, or Han in this module.

- [ ] **Step 5: Make readiness validate proof bytes**

Extend `build_readiness` with an optional `contract_proof` path. Record
`confirmation_contract_real_path` only when result is `PASSED`, hashes are
64 lowercase hex characters, both trace hashes match, frames are positive,
the proof executable hash equals the freeze executable hash, return code is
zero, and stderr is empty. A missing or invalid proof appends
`confirmation contract real-path proof is invalid`. Add CLI argument
`--contract-proof` and pass it to `build_readiness`.

- [ ] **Step 6: Run real-path, C++ parser, and readiness tests**

```bash
BILLIARDGL_EXECUTABLE="$PWD/build-v4-plan/Billiards/Billiards" \
  python3 -m unittest tests.physics_validation.test_confirmation_contract_fixture -v
ctest --test-dir build-v4-plan -R 'BilliardsPhysicsScenarioTests|BilliardsAutomationPhysicsScenariosTests' --output-on-failure
python3 -m unittest tests.physics_validation.test_phase3_v3_confirmation -v
```

Expected: all pass; the negative fixture receives the C++ `invalid_scenario` response.

- [ ] **Step 7: Commit the real-path contract gate**

```bash
git add tools/physics_validation/confirmation_contract_fixture.py \
  tools/physics_validation/run.py \
  tests/e2e/automation_client.py \
  tools/physics_validation/confirmation_readiness.py \
  tests/physics_validation/test_confirmation_contract_fixture.py \
  tests/physics_validation/fixtures/confirmation_transaction_v1/scenario_template.json
git commit -m "test: prove confirmation through real automation path"
```

---

### Task 4: Admit the Complete Alciatore TP A.15 Numerical Package

**Files:**
- Create: `tools/physics_validation/extract_alciatore_2005_tp_a15.py`
- Create: `tests/physics_validation/test_alciatore_2005_extraction.py`
- Create: `tests/physics_validation/reference_data/alciatore_2005_tp_a15/raw_extracted.csv`
- Create: `tests/physics_validation/reference_data/alciatore_2005_tp_a15/normalized.csv`
- Create: `tests/physics_validation/reference_data/alciatore_2005_tp_a15/scalars.csv`
- Create: `tests/physics_validation/reference_data/alciatore_2005_tp_a15/{extraction,scenario_template,source_access_audit,manifest,split,expected_model_mismatches,expected_reference_limitations}.json`
- Modify: `tests/physics_validation/validation_data_status.json`

**Interfaces:**
- Produces: `generated_files() -> dict[str, bytes]`, `verify_package(path: Path) -> list[str]`, and CLI `python3 -m tools.physics_validation.extract_alciatore_2005_tp_a15 --verify PACKAGE_DIR`.

- [ ] **Step 1: Write extraction tests with all nine literal pairs**

```python
PAIRS = ((0, 0), (8, 13), (20, 34), (34, 50), (46, 61),
         (57, 70), (67, 78), (77, 87), (90, 90))

def test_all_published_pairs_are_preserved(self):
    rows = list(csv.DictReader((PACKAGE / "raw_extracted.csv").open()))
    self.assertEqual(tuple((int(row["cut_angle_degrees"]),
                            int(row["target_angle_degrees"])) for row in rows),
                     PAIRS)

def test_package_is_confirmation_only(self):
    entry = load_data_lifecycle(STATUS).entry(
        "alciatore_2005_tp_a15", "1.0.0")
    self.assertEqual((entry.calibration_status, entry.holdout_status),
                     ("confirmation", "confirmation"))

def test_generated_package_is_byte_reproducible(self):
    self.assertEqual(verify_package(PACKAGE), [])
```

Also assert the scenario template contains `interior_rmse_degrees_maximum: 3`,
`interior_absolute_error_degrees_maximum: 5`, head-on ratios `1e-3` and
direction error `1`, and grazing speed ratio `1e-3`.

- [ ] **Step 2: Verify RED before creating package files**

```bash
python3 -m unittest tests.physics_validation.test_alciatore_2005_extraction -v
```

Expected: package or extractor import is missing.

- [ ] **Step 3: Implement deterministic generation**

Use these immutable constants:

```python
DATASET_ID = "alciatore_2005_tp_a15"
DATASET_VERSION = "1.0.0"
SOURCE_URL = "https://drdavepoolinfo.com/technical_proofs/new/TP_A-15.pdf"
PAIRS = ((0, 0), (8, 13), (20, 34), (34, 50), (46, 61),
         (57, 70), (67, 78), (77, 87), (90, 90))
```

Generate one raw and normalized row for every pair. Use point IDs
`alciatore_cut_000` through `alciatore_cut_090`, `partition=CONFIRMATION`,
`metric=target_ball_angle`, `unit=degree`, source numeric resolution `0.5`,
engineering absolute tolerance `3`, and no relative tolerance. Keep the
normalized CSV on the repository's standard reference-point header. The
scenario template maps the two endpoint point IDs to
`evaluation_role=endpoint_invariant` and the seven others to
`evaluation_role=interior_angle`. `scalars.csv` records the centered horizontal
cue contract and source apparatus facts but contains no candidate prediction.

The source audit must state that the PDF/video are not vendored, record the
source URL, access date `2026-07-15`, document title, author, high-speed-video
method, and an audit SHA-256 over the canonical extracted rows rather than over
an uncommitted third-party file.

- [ ] **Step 4: Generate, verify, and inspect the package**

```bash
python3 -m tools.physics_validation.extract_alciatore_2005_tp_a15 \
  --output tests/physics_validation/reference_data/alciatore_2005_tp_a15
python3 -m tools.physics_validation.extract_alciatore_2005_tp_a15 \
  --verify tests/physics_validation/reference_data/alciatore_2005_tp_a15
python3 -m unittest tests.physics_validation.test_alciatore_2005_extraction -v
```

Expected: verifier exits 0; all nine rows and every manifest hash reproduce.

- [ ] **Step 5: Obtain independent review of the source-coordinate mapping**

Present the nine committed pairs, the report's two original column labels, the
definition of source cut angle, the definition of target-ball angle, and the
engine coordinate transform. Record the review outcome and reviewed extraction
SHA-256 in `source_access_audit.json`, regenerate the manifest, and rerun
`--verify`. Do not continue if the reviewer disputes the column orientation.

- [ ] **Step 6: Prove no fitting path reads the new package**

Add `alciatore_2005_tp_a15` to the explicit confirmation-deny set in
`build_v3_fit_inputs.py`, then run:

```bash
python3 -m unittest tests.physics_validation.test_phase3_v3_fit \
  tests.physics_validation.test_alciatore_2005_extraction -v
```

Expected: all pass and fit outputs remain byte-identical.

- [ ] **Step 7: Commit the complete source package**

```bash
git add tools/physics_validation/extract_alciatore_2005_tp_a15.py \
  tools/physics_validation/build_v3_fit_inputs.py \
  tests/physics_validation/test_alciatore_2005_extraction.py \
  tests/physics_validation/reference_data/alciatore_2005_tp_a15 \
  tests/physics_validation/validation_data_status.json
git commit -m "data: admit Alciatore TP A.15 confirmation package"
```

---

### Task 5: Add Alciatore and Han Scenario/Evaluation Adapters

**Files:**
- Create: `tools/physics_validation/alciatore_confirmation.py`
- Create: `tools/physics_validation/han_confirmation.py`
- Create: `tests/physics_validation/test_alciatore_confirmation.py`
- Create: `tests/physics_validation/test_han_confirmation.py`
- Modify: `tools/physics_validation/confirmation_adapters.py`
- Modify: `tools/physics_validation/confirmation_run.py`
- Modify: `tools/physics_validation/extract_han_2005.py`
- Modify: `tests/physics_validation/reference_data/han_2005/scenario_template.json`
- Regenerate: `tests/physics_validation/reference_data/han_2005/manifest.json`

**Interfaces:**
- Produces registered adapters for `alciatore_2005_tp_a15` and `han_2005`.
- Produces pure evaluators `evaluate_alciatore(traces, profile, package)` and `evaluate_han(traces, profile, package)` suitable for synthetic unit traces without opening a real transaction.

- [ ] **Step 1: Write synthetic-trace metric tests**

```python
def test_alciatore_threshold_boundaries_are_inclusive(self):
    evaluation = evaluate_alciatore(
        traces_with_interior_errors([3, -3, 3, -3, 3, -3, 3]), PROFILE,
        PACKAGE)
    self.assertEqual(evaluation.summary_metrics["interior_rmse_degrees"], 3)
    self.assertTrue(evaluation.summary_metrics["interior_rmse_passed"])

def test_alciatore_maximum_error_rejects_above_five(self):
    evaluation = evaluate_alciatore(
        traces_with_interior_errors([0, 0, 0, 0, 0, 0, 5.0001]), PROFILE,
        PACKAGE)
    self.assertFalse(evaluation.summary_metrics["interior_maximum_passed"])

def test_han_contract_is_unchanged(self):
    evaluation = evaluate_han(HAN_PASSING_TRACES, PROFILE, HAN_PACKAGE)
    self.assertEqual(evaluation.summary_metrics["normalized_curve_rmse_maximum"],
                     0.15)
    self.assertEqual(set(evaluation.summary_metrics["hard_metrics"]), {
        "finite_bounded_response", "continuous_response",
        "source_domain_response", "nonincreasing_total_energy",
    })
```

Add failure cases for missing contact, nonfinite state, nondeterministic input,
head-on lateral ratio over `1e-3`, head-on direction error over `1 degree`, and
grazing target-speed ratio over `1e-3`.

- [ ] **Step 2: Verify both adapter suites fail**

```bash
python3 -m unittest tests.physics_validation.test_alciatore_confirmation \
  tests.physics_validation.test_han_confirmation -v
```

Expected: imports for both modules fail.

- [ ] **Step 3: Implement Alciatore scenarios and evaluator**

For each source row, build a schema-v11 unbounded scenario with two touching
balls and a centered horizontal cue impact. Set the contact geometry from the
source cut angle, use `initial_contact_epsilon_cm` no larger than the frozen
solver's accepted epsilon, and evaluate the first stable separating target
velocity. Use:

```python
error = observed_target_angle - expected_target_angle
rmse = math.sqrt(sum(value * value for value in interior_errors) /
                 len(interior_errors))
maximum = max(abs(value) for value in interior_errors)
```

Emit every point even when aggregate gates fail. For the 90-degree endpoint,
emit `observed=None` and evaluate only the target/incident speed ratio when the
direction is undefined.

- [ ] **Step 4: Implement Han scenarios and evaluator**

Build one production-table rail approach for each committed speed. Compute
observed restitution from the first rail contact, normalize all observations by
the observed value at 0.5 m/s, and calculate the curve RMSE against the
committed normalized equation values. Evaluate boundedness, continuity,
domain coverage, and nonincreasing total energy exactly as named in the Han
template. Do not compare absolute restitution as a direct pass/fail metric.

- [ ] **Step 5: Register both adapters and enforce aggregate failure**

Register at module import using:

```python
register_confirmation_adapter(ConfirmationAdapter(
    "alciatore_2005_tp_a15", build_alciatore_scenarios,
    evaluate_alciatore))
register_confirmation_adapter(ConfirmationAdapter(
    "han_2005", build_han_scenarios, evaluate_han))
```

Import both adapter modules from `confirmation_run.py` before registry lookup;
the imports exist solely to execute these deterministic registrations.

In `confirmation_run.py`, set report result to `FAILED` if any point row is
`FAILED` **or** any boolean summary field ending in `_passed` is false.
For production execution, call `_execute_once_with_evidence` twice, compare the
two frame sequences, and write `execution/{scenario_id}-first.json` and
`execution/{scenario_id}-second.json` containing the canonical protocol
transcript, stderr, return code, frame count, and trace SHA-256. Test-injected
executors that return only frame lists receive a clearly marked
`fixture_executor: true` summary and are never accepted by v4 readiness.

- [ ] **Step 6: Regenerate Han's package after fixing its empty template expectations**

Update both `extract_han_2005.py` and the committed base scenario to emit the
same two nonempty base expectations, then run:

```bash
python3 -m tools.physics_validation.extract_han_2005 \
  --package tests/physics_validation/reference_data/han_2005
python3 -m tools.physics_validation.extract_han_2005 \
  --package tests/physics_validation/reference_data/han_2005 --check
```

Expected: verification exits 0 and the normalized numerical curve is unchanged.

- [ ] **Step 7: Run adapter and legacy regression tests**

```bash
python3 -m unittest \
  tests.physics_validation.test_alciatore_confirmation \
  tests.physics_validation.test_han_confirmation \
  tests.physics_validation.test_han_2005_extraction \
  tests.physics_validation.test_phase3_v2_confirmation \
  tests.physics_validation.test_confirmation_transaction -v
```

Expected: all pass.

- [ ] **Step 8: Commit the two evaluators**

```bash
git add tools/physics_validation/alciatore_confirmation.py \
  tools/physics_validation/han_confirmation.py \
  tools/physics_validation/confirmation_adapters.py \
  tools/physics_validation/confirmation_run.py \
  tools/physics_validation/extract_han_2005.py \
  tests/physics_validation/test_alciatore_confirmation.py \
  tests/physics_validation/test_han_confirmation.py \
  tests/physics_validation/reference_data/han_2005
git commit -m "feat: evaluate Alciatore and Han confirmation traces"
```

---

### Task 6: Generate the Physics-Identical v4 Candidate

**Files:**
- Create: `tools/physics_validation/build_v4_profile.py`
- Create: `tests/physics_validation/test_phase3_v4_candidate.py`
- Create: `physics_models/profiles/chinese_pool_full_game_v4.json`
- Create: `src/Billiards/generated/phase3_v4_profile.inc`
- Create: `physics_models/promotion/phase3_candidates_v4.json`
- Create: `physics_models/promotion/phase3_v4_physics_equivalence.json`
- Create: `physics_models/promotion/full_game_matrix_v4.json`
- Create: `physics_models/promotion/full_game_performance_budget_v4.json`
- Modify: `src/Billiards/physics_profile.cpp`
- Modify: `tools/physics_validation/freeze_candidate.py`

**Interfaces:**
- Produces: `build_v4_profile(v3_profile: dict) -> dict`, `physics_values(profile: dict) -> dict`, `write_v4_candidate(root: Path) -> dict`.

- [ ] **Step 1: Write equality, identity, and inventory tests**

```python
def test_v4_changes_identity_but_not_physics(self):
    v3 = json.loads(V3_PROFILE.read_text())
    v4 = json.loads(V4_PROFILE.read_text())
    self.assertEqual(physics_values(v4), physics_values(v3))
    self.assertEqual(v4["runtime_profile"]["id"],
                     "chinese_pool_full_game_v4")
    self.assertEqual(v4["runtime_profile"]["formula_version"],
                     v3["runtime_profile"]["formula_version"])

def test_v4_has_exact_confirmation_packages(self):
    inventory = json.loads(INVENTORY.read_text())
    self.assertEqual({row["package_id"] for row in
                      inventory["confirmation_packages"]},
                     {"alciatore_2005_tp_a15", "han_2005"})
```

Also hash every v3 candidate file and assert the generator does not alter any
of them.

- [ ] **Step 2: Verify RED**

```bash
python3 -m unittest tests.physics_validation.test_phase3_v4_candidate -v
```

Expected: `build_v4_profile` or v4 artifacts are missing.

- [ ] **Step 3: Implement the identity-only candidate builder**

```python
def physics_values(profile):
    runtime = profile["runtime_profile"]
    return {section: copy.deepcopy(runtime[section]) for section in (
        "ball", "surface", "cue", "cushion", "table_boundary", "solver")}

def build_v4_profile(v3_profile):
    result = copy.deepcopy(v3_profile)
    result["runtime_profile"]["id"] = "chinese_pool_full_game_v4"
    result["runtime_query"]["id"] = "chinese_pool_full_game_v4"
    result["applicability"]["notes"] = (
        "Phase 3 v4 confirmation-contract recovery; numerical physics is "
        "identical to phase3_integrated_v3.")
    return result
```

Keep `formula_version` unchanged. Recompute only canonical identity-dependent
hashes. Clone the v3 full-game matrix and budget, changing artifact root,
profile ID, and candidate ID only. Inventory exactly the Alciatore and Han
manifests and contracts, `phase3_v4_physics_equivalence.json`, and all
pre-freeze inputs. The equivalence artifact stores both profile hashes plus a
canonical hash for each of the six physics sections and requires every paired
section hash to match.
The post-freeze synthetic proof is bound by readiness, not by the freeze, which
avoids a circular dependency on the frozen executable hash.

- [ ] **Step 4: Wire the generated runtime identity**

Generate `phase3_v4_profile.inc` with identical numeric assignments and v4 ID,
then make `physics_profile.cpp` include it as the selected production profile.
Allow `phase3_integrated_v4` in `freeze_candidate.phase3_freeze_document`.

- [ ] **Step 5: Generate and verify candidate artifacts**

```bash
python3 -m tools.physics_validation.build_v4_profile --root "$PWD"
python3 -m unittest tests.physics_validation.test_phase3_v4_candidate \
  tests.physics_validation.test_phase3_v3_candidate -v
git diff --check
```

Expected: all pass; no v3 candidate artifact changes; physics-value equality is exact.

- [ ] **Step 6: Commit candidate selection**

```bash
git add tools/physics_validation/build_v4_profile.py \
  tools/physics_validation/freeze_candidate.py \
  src/Billiards/physics_profile.cpp \
  src/Billiards/generated/phase3_v4_profile.inc \
  physics_models/profiles/chinese_pool_full_game_v4.json \
  physics_models/promotion/phase3_candidates_v4.json \
  physics_models/promotion/phase3_v4_physics_equivalence.json \
  physics_models/promotion/full_game_matrix_v4.json \
  physics_models/promotion/full_game_performance_budget_v4.json \
  tests/physics_validation/test_phase3_v4_candidate.py
git commit -m "feat: select physics-identical phase 3 candidate v4"
```

---

### Task 7: Clean-Build, Full-Game, Freeze, and Produce Confirmation Readiness

**Files:**
- Create: `tests/physics_validation/test_phase3_v4_freeze.py`
- Create: `tests/physics_validation/test_phase3_v4_full_game_run.py`
- Create: `tests/physics_validation/test_phase3_v4_confirmation.py`
- Create: `tools/physics_validation/phase3_v4_assessment.py`
- Create: `tests/physics_validation/test_phase3_v4_assessment.py`
- Generate: `physics_models/candidates/phase3_integrated_v4/freeze.json`
- Generate: `physics_models/candidates/phase3_integrated_v4/full_game/**`
- Generate: `physics_models/candidates/phase3_integrated_v4/confirmation_contract_proof.json`
- Generate: `physics_models/candidates/phase3_integrated_v4/confirmation_readiness.json`
- Generate: `physics_models/promotion/full_game_performance_baseline_v4.json`
- Generate: `physics_models/promotion/full_game_stress_v4.csv`

**Interfaces:**
- Produces a frozen candidate and a non-consuming readiness document whose package attempts are both `UNOPENED`.

- [ ] **Step 1: Verify the candidate source revision is clean**

```bash
git status --porcelain
SOURCE_REVISION=$(git rev-parse HEAD)
test -n "$SOURCE_REVISION"
```

Expected: `git status --porcelain` prints nothing. `SOURCE_REVISION` is the
Task 6 candidate commit and must never be amended.

- [ ] **Step 2: Freeze v4 with two independent clean builds**

```bash
python3 -m tools.physics_validation.freeze_candidate \
  --phase3-inventory physics_models/promotion/phase3_candidates_v4.json \
  --two-clean-builds \
  --source-revision "$SOURCE_REVISION" \
  --output physics_models/candidates/phase3_integrated_v4/freeze.json \
  --jobs 2
```

Expected: exit 0; `clean_build_sha256` has two equal values and
`clean_profile_sha256` has two equal values.

- [ ] **Step 3: Rebuild the exact frozen executable and runner**

```bash
FROZEN_BUILD=$(python3 -m tools.physics_validation.rebuild_frozen \
  --root "$PWD" \
  --freeze physics_models/candidates/phase3_integrated_v4/freeze.json \
  --jobs 2 --print-build-dir)
test -x "$FROZEN_BUILD/Billiards"
test -x "$FROZEN_BUILD/BilliardsFullGameStress"
```

Expected: both executables exist and `rebuild_frozen` has already checked the
frozen executable and canonical profile hashes.

- [ ] **Step 4: Generate the real-path proof with the frozen executable**

```bash
python3 -m tools.physics_validation.confirmation_contract_fixture \
  --root "$PWD" --executable "$FROZEN_BUILD/Billiards" \
  --fixture tests/physics_validation/fixtures/confirmation_transaction_v1 \
  --output physics_models/candidates/phase3_integrated_v4/confirmation_contract_proof.json
```

Expected: `result` is `PASSED`, trace hashes match, and no Alciatore or Han
package path appears in the proof.

- [ ] **Step 5: Run and reproduce the full 12-case game matrix**

```bash
"$FROZEN_BUILD/BilliardsFullGameStress" \
  --matrix physics_models/promotion/full_game_matrix_v4.json \
  --write physics_models/candidates/phase3_integrated_v4/full_game
rm -rf build/phase3-v4-repeat
"$FROZEN_BUILD/BilliardsFullGameStress" \
  --matrix physics_models/promotion/full_game_matrix_v4.json \
  --write build/phase3-v4-repeat
for trace in physics_models/candidates/phase3_integrated_v4/full_game/*/trace.json; do
  case_id=$(basename "$(dirname "$trace")")
  cmp "$trace" "build/phase3-v4-repeat/$case_id/trace.json"
done
python3 -m tools.physics_validation.generate_full_game_baseline \
  --freeze physics_models/candidates/phase3_integrated_v4/freeze.json \
  --matrix physics_models/promotion/full_game_matrix_v4.json \
  --budget physics_models/promotion/full_game_performance_budget_v4.json \
  --output-root physics_models/candidates/phase3_integrated_v4/full_game \
  --runner "$FROZEN_BUILD/BilliardsFullGameStress" \
  --baseline physics_models/promotion/full_game_performance_baseline_v4.json \
  --csv physics_models/promotion/full_game_stress_v4.csv
```

Expected: both runs are byte-identical; all twelve cases and
`matrix_summary.json` report `passed: true`.

- [ ] **Step 6: Generate non-consuming readiness**

```bash
python3 -m tools.physics_validation.confirmation_readiness \
  --root "$PWD" \
  --freeze physics_models/candidates/phase3_integrated_v4/freeze.json \
  --inventory physics_models/promotion/phase3_candidates_v4.json \
  --full-game physics_models/candidates/phase3_integrated_v4/full_game \
  --contract-proof physics_models/candidates/phase3_integrated_v4/confirmation_contract_proof.json \
  --output physics_models/candidates/phase3_integrated_v4/confirmation_readiness.json
```

Expected: exit 0 and `status: READY`; no ledger or real confirmation output
directory exists.

- [ ] **Step 7: Write artifact and readiness tests**

```python
def test_freeze_has_two_identical_clean_builds(self):
    freeze = json.loads(FREEZE.read_text())
    self.assertEqual(freeze["candidate_id"], "phase3_integrated_v4")
    self.assertEqual(len(freeze["clean_build_sha256"]), 2)
    self.assertEqual(len(set(freeze["clean_build_sha256"])), 1)

def test_readiness_has_real_contract_proof_and_unopened_packages(self):
    readiness = json.loads(READINESS.read_text())
    self.assertEqual(readiness["status"], "READY")
    self.assertTrue(readiness["checks"]["confirmation_contract_real_path"])
    self.assertEqual(set(readiness["confirmation_packages"]),
                     {"alciatore_2005_tp_a15", "han_2005"})
    self.assertTrue(all(row["attempt"] == "UNOPENED" for row in
                        readiness["confirmation_packages"].values()))
    self.assertFalse(LEDGER.exists())
```

Add the v3-style completeness assertions for all 12 case directories and the
v4 baseline/CSV reproducibility assertions using `generate_documents`. Create
`test_phase3_v4_assessment.py` with synthetic candidate trees covering absent
Alciatore, absent Han after an Alciatore pass, an Alciatore failure, a Han
failure, and two passes; assert exact disposition and tree hashes.

- [ ] **Step 8: Verify the assessment tests fail before implementation**

```bash
python3 -m unittest tests.physics_validation.test_phase3_v4_assessment -v
```

Expected: import failure for `tools.physics_validation.phase3_v4_assessment`.

- [ ] **Step 9: Implement the precommitted final-assessment policy**

Before any real transaction, implement
`build_final_assessment(root: Path) -> dict`. It must validate the candidate,
freeze, executable, full-game, readiness, ledger, distinct attempt IDs, receipt
results, and confirmation tree hashes. The live pre-confirmation test must
raise `ValueError("Alciatore confirmation is absent")`; synthetic ledgers must
prove that one failed receipt produces `REJECTED` and exactly two passing
receipts produce `ACCEPTED`. Its CLI accepts `--root`, `--output`, and optional
`--rejection-output` and writes canonical JSON.

Use this exact decision core after all paths and hashes have been validated:

```python
def _receipt_result(receipt):
    return receipt.get("result") in {"PASSED", "PASSED_OR_ACCOUNTED"}

def build_final_assessment(root):
    evidence = validated_confirmation_evidence(Path(root).resolve())
    alciatore = evidence["alciatore_2005_tp_a15"]
    han = evidence.get("han_2005")
    if han is None and _receipt_result(alciatore["receipt"]):
        raise ValueError("Han confirmation is absent")
    accepted = _receipt_result(alciatore["receipt"]) and \
        han is not None and _receipt_result(han["receipt"])
    return {
        "schema_version": 1,
        "candidate_id": "phase3_integrated_v4",
        "disposition": "ACCEPTED" if accepted else "REJECTED",
        "confirmations": evidence,
        "policy": "two_independent_one_time_confirmations_required",
    }
```

- [ ] **Step 10: Run the complete pre-confirmation verification**

```bash
python3 -m unittest discover -s tests/physics_validation -p 'test_*.py' -v
cmake --build build-v4-plan -j2
ctest --test-dir build-v4-plan --output-on-failure
git diff --check
git status --short
```

Expected: all tests pass; status lists only governed v4 tests, freeze, proof,
full-game, baseline, and CSV outputs.

- [ ] **Step 11: Commit frozen v4 evidence**

```bash
git add physics_models/candidates/phase3_integrated_v4 \
  physics_models/promotion/full_game_performance_baseline_v4.json \
  physics_models/promotion/full_game_stress_v4.csv \
  tests/physics_validation/test_phase3_v4_freeze.py \
  tests/physics_validation/test_phase3_v4_full_game_run.py \
  tests/physics_validation/test_phase3_v4_confirmation.py \
  tools/physics_validation/phase3_v4_assessment.py \
  tests/physics_validation/test_phase3_v4_assessment.py
git commit -m "release: freeze physics-identical phase 3 candidate v4"
```

Stop and present readiness, exact executable hash, test results, and the exact
one-time Alciatore command. Do not proceed without fresh explicit approval.

---

### Task 8: Execute the Sole Alciatore Confirmation Transaction

**Files:**
- Generate: `physics_models/candidates/phase3_integrated_v4/confirmation_consumption.json`
- Generate: `physics_models/candidates/phase3_integrated_v4/confirmation/alciatore_2005_tp_a15/**`
- Conditional create: `physics_models/promotion/phase3_integrated_v4_rejection.json`

**Interfaces:**
- Consumes the frozen executable, freeze, unopened package declaration, and explicit user authorization.
- Produces an immutable Alciatore receipt and complete numerical artifacts.

- [ ] **Step 1: Obtain explicit authorization immediately before execution**

Show the exact command, candidate ID, package/version, executable SHA-256,
freeze SHA-256, readiness SHA-256, and state that any post-reservation failure
consumes the source. Continue only after the user explicitly approves this
single transaction.

- [ ] **Step 2: Execute exactly once**

```bash
FROZEN_BUILD=$(python3 -c 'import json; from pathlib import Path; from tools.physics_validation.rebuild_frozen import frozen_build_paths; print(frozen_build_paths(json.loads(Path("physics_models/candidates/phase3_integrated_v4/freeze.json").read_text())).build_dir)')
python3 -m tools.physics_validation.validation_run \
  --freeze physics_models/candidates/phase3_integrated_v4/freeze.json \
  --package-key alciatore_2005_tp_a15 \
  --ledger physics_models/candidates/phase3_integrated_v4/confirmation_consumption.json \
  --output physics_models/candidates/phase3_integrated_v4/confirmation/alciatore_2005_tp_a15 \
  --executable "$FROZEN_BUILD/Billiards"
```

Do not retry, delete, or overwrite any output regardless of exit status.

- [ ] **Step 3: Account for the immutable result**

Verify receipt, ledger, scenario, trace, metrics, provenance, and report hashes.
If result is not `PASSED_OR_ACCOUNTED`, generate a v4 rejection binding the
failure and explicitly recording `han_2005: NOT_EXECUTED`; stop the plan.
If it passes, prove Han has no ledger entry and no output directory.

On failure, use the already committed assessment code:

```bash
python3 -m tools.physics_validation.phase3_v4_assessment \
  --root "$PWD" \
  --output physics_models/candidates/phase3_integrated_v4/final_assessment.json \
  --rejection-output physics_models/promotion/phase3_integrated_v4_rejection.json
```

- [ ] **Step 4: Commit the complete Alciatore outcome**

```bash
git add physics_models/candidates/phase3_integrated_v4/confirmation_consumption.json \
  physics_models/candidates/phase3_integrated_v4/confirmation/alciatore_2005_tp_a15
test ! -e physics_models/candidates/phase3_integrated_v4/final_assessment.json || \
  git add physics_models/candidates/phase3_integrated_v4/final_assessment.json
test ! -e physics_models/promotion/phase3_integrated_v4_rejection.json || \
  git add physics_models/promotion/phase3_integrated_v4_rejection.json
git commit -m "data: preserve phase 3 v4 Alciatore confirmation"
```

Omit the rejection path from `git add` when the transaction passes and the file
does not exist. If Alciatore passes, stop and request separate Han authorization.

---

### Task 9: Execute Han and Bind the Final Assessment

**Files:**
- Modify: `physics_models/candidates/phase3_integrated_v4/confirmation_consumption.json`
- Generate: `physics_models/candidates/phase3_integrated_v4/confirmation/han_2005/**`
- Generate: `physics_models/candidates/phase3_integrated_v4/final_assessment.json`
- Conditional create: `physics_models/promotion/phase3_integrated_v4_rejection.json`

**Interfaces:**
- Consumes the precommitted `build_final_assessment(root: Path) -> dict` and produces the second immutable receipt plus the final disposition.

- [ ] **Step 1: Obtain separate explicit Han authorization**

Show the exact command and hashes, confirm Alciatore passed, and state that Han
will be consumed by reservation. Continue only after explicit approval for Han.

- [ ] **Step 2: Execute Han exactly once**

```bash
FROZEN_BUILD=$(python3 -c 'import json; from pathlib import Path; from tools.physics_validation.rebuild_frozen import frozen_build_paths; print(frozen_build_paths(json.loads(Path("physics_models/candidates/phase3_integrated_v4/freeze.json").read_text())).build_dir)')
python3 -m tools.physics_validation.validation_run \
  --freeze physics_models/candidates/phase3_integrated_v4/freeze.json \
  --package-key han_2005 \
  --ledger physics_models/candidates/phase3_integrated_v4/confirmation_consumption.json \
  --output physics_models/candidates/phase3_integrated_v4/confirmation/han_2005 \
  --executable "$FROZEN_BUILD/Billiards"
```

Never retry or overwrite.

- [ ] **Step 3: Generate and verify the final assessment**

```bash
python3 -m tools.physics_validation.phase3_v4_assessment \
  --root "$PWD" \
  --output physics_models/candidates/phase3_integrated_v4/final_assessment.json \
  --rejection-output physics_models/promotion/phase3_integrated_v4_rejection.json
python3 -m unittest discover -s tests/physics_validation -p 'test_*.py' -v
ctest --test-dir build-v4-plan --output-on-failure
git diff --check
```

Expected: tests pass. Assessment is `ACCEPTED` only if both real confirmations
passed; otherwise it is `REJECTED` with every failure and limitation retained.

- [ ] **Step 4: Commit all final numerical evidence**

```bash
git add physics_models/candidates/phase3_integrated_v4/confirmation_consumption.json \
  physics_models/candidates/phase3_integrated_v4/confirmation/han_2005 \
  physics_models/candidates/phase3_integrated_v4/final_assessment.json
test ! -e physics_models/promotion/phase3_integrated_v4_rejection.json || \
  git add physics_models/promotion/phase3_integrated_v4_rejection.json
git commit -m "data: preserve phase 3 v4 Han confirmation assessment"
```

Omit the rejection path when it does not exist. Report the actual disposition,
all numerical gates, limitations, commit IDs, and test evidence without
describing a rejected candidate as real-world validated.
