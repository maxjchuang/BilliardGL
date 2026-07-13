# Reference Data Foundation Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Build the source-agnostic, offline reference-data infrastructure that later plans will populate with audited public experimental data.

**Architecture:** Add a strict loader for versioned reference packages, immutable normalized reference points, pre-registered split validation, an adapter registry that emits canonical physics scenarios, separate failure accounting, and reference-specific reports. The reference runner reuses the existing production-process executor and analyzer rather than introducing a second simulation path.

**Tech Stack:** Python 3 standard library, existing C++11 BilliardGL automation executable, JSON, CSV, Markdown, CMake/CTest, GitHub Actions.

---

## Global constraints

- This is plan 1 of 5. It adds no values from the four papers and makes no production physics, material-parameter, or solver changes.
- Commit every numerical input used by future validation. Runtime and CI must not access the network or require paper PDFs.
- Use only the Python standard library in `tools/physics_validation`.
- Keep all first-phase scenario, analyzer, report, and runner behavior backward compatible.
- Schema version is integer `1`. Stable IDs must match `[A-Za-z0-9][A-Za-z0-9_.-]*`.
- File digests use `sha256:<64 lowercase hex characters>`. A package is not returned until every schema, path, and digest check succeeds.
- All measured values and uncertainties must be finite. Uncertainty components and engineering tolerances must be nonnegative.
- Combine independent uncertainty components by root-sum-square. Default coverage factor is `2.0`.
- Acceptance half-width is `max(engineering_absolute_tolerance, engineering_relative_tolerance * abs(expected), coverage_factor * combined_standard_uncertainty)`.
- Calibration and holdout are exhaustive, disjoint, grouped by `group_id`, and immutable from the CLI.
- `MODEL_MISMATCH`, `REFERENCE_LIMITATION`, `INTEGRATION_MISMATCH`, and `NUMERICAL_FAILURE` remain visible failures in reports.
- Expected model mismatches and expected reference limitations use separate manifests. Neither may allowlist numerical or integration failures.
- All JSON is serialized with UTF-8, `ensure_ascii=False`, sorted keys, indentation, a final newline, and `allow_nan=False` where supported.

## Delivery boundary

This plan delivers the shared machinery and one deliberately synthetic package named `synthetic_reference_v1`. The synthetic package proves schema validation, hashing, normalization, split isolation, scenario adaptation, process execution, accounting, and reports. The four source-specific packages are delivered by plans 2–5 only after their individual data-admission audits.

## File structure

```text
tools/physics_validation/
  reference_package.py          # Atomic manifest/path/hash validation and package CLI
  reference_point.py            # Normalized CSV parsing and uncertainty intervals
  reference_split.py            # Immutable calibration/holdout validation
  reference_adapter.py          # Adapter registry and canonical ReferenceCase output
  reference_accounting.py       # Strict mismatch/limitation reconciliation
  reference_report.py           # JSON/CSV/Markdown reference reports
  reference_run.py              # Offline package runner using production process execution
tests/physics_validation/
  test_reference_package.py
  test_reference_point.py
  test_reference_split.py
  test_reference_adapter.py
  test_reference_accounting.py
  test_reference_report.py
  test_reference_run.py
  fixtures/reference_package_v1/
    manifest.json
    raw_extracted.csv
    normalized.csv
    split.json
    extraction.json
    scenario_template.json
    expected_model_mismatches.json
    expected_reference_limitations.json
tests/e2e/
  test_reference_validation.py
docs/
  reference-data-packages.md
```

## Task 1: Load and verify a reference package atomically

**Files:**

- Create: `tools/physics_validation/reference_package.py`
- Create: `tests/physics_validation/test_reference_package.py`
- Create: `tests/physics_validation/fixtures/reference_package_v1/raw_extracted.csv`
- Create: `tests/physics_validation/fixtures/reference_package_v1/normalized.csv`
- Create: `tests/physics_validation/fixtures/reference_package_v1/split.json`
- Create: `tests/physics_validation/fixtures/reference_package_v1/extraction.json`
- Create: `tests/physics_validation/fixtures/reference_package_v1/scenario_template.json`
- Create: `tests/physics_validation/fixtures/reference_package_v1/expected_model_mismatches.json`
- Create: `tests/physics_validation/fixtures/reference_package_v1/expected_reference_limitations.json`
- Create: `tests/physics_validation/fixtures/reference_package_v1/manifest.json`

- [ ] **Step 1: Write failing loader tests**

Define tests for these public types and functions:

```python
class ReferencePackageError(ValueError):
    code: str

@dataclass(frozen=True)
class ReferencePackage:
    root: Path
    manifest: dict
    files: dict

def load_reference_package(path: Path) -> ReferencePackage: ...
def update_reference_hashes(path: Path) -> Path: ...
```

Assert that the fixture loads, exposes absolute resolved file paths, and rejects: schema version other than `1`, unsafe dataset/version/file IDs, duplicate logical file IDs, absolute paths, `..` traversal, symlink escape, missing files, malformed hashes, a one-byte content change, and extraction metadata missing its input hashes, transformations, operator, or independent review record. Assert the error codes are stable strings such as `UNSUPPORTED_SCHEMA`, `UNSAFE_ID`, `UNSAFE_PATH`, `MISSING_FILE`, `HASH_MISMATCH`, and `INVALID_EXTRACTION_METADATA`.

- [ ] **Step 2: Run the tests and confirm the missing module failure**

Run:

```bash
python3 -m unittest tests.physics_validation.test_reference_package -v
```

Expected: `ModuleNotFoundError: No module named 'tools.physics_validation.reference_package'`.

- [ ] **Step 3: Create the complete synthetic package payload**

Use dataset ID `synthetic_reference`, version `1.0.0`, and adapter ID `synthetic_free_roll_v1`. The package manifest must contain source metadata, acquisition/license metadata, evidence grade, apparatus/applicability, extraction review, and a `files` array with logical IDs for all seven non-manifest files. `extraction.json` must record schema version, extraction method/tool/date/operator, reviewed-by/date/method, input file IDs and hashes, explicit unit transformations, and rounding policy. The normalized fixture contains two cases in different groups: one calibration case and one holdout case. The expected mismatch and limitation manifests start with empty `failures` arrays.

After all non-manifest fixture bytes are final, run:

```bash
shasum -a 256 tests/physics_validation/fixtures/reference_package_v1/{raw_extracted.csv,normalized.csv,split.json,extraction.json,scenario_template.json,expected_model_mismatches.json,expected_reference_limitations.json}
```

Insert those exact lowercase digests into `manifest.json` with the `sha256:` prefix. Do not retain temporary or sentinel hash values in the commit.

- [ ] **Step 4: Implement the strict loader and hash-update CLI**

Parse the manifest first, validate its closed required-key set, resolve every declared relative path with `Path.resolve()`, require it to remain below the resolved package root, and verify every digest by streaming 1 MiB blocks. Reject undeclared required files and duplicate resolved paths. Implement `python3 -m tools.physics_validation.reference_package --update-hashes PACKAGE` so maintainers can deliberately refresh all declared hashes after an audited edit; normal loading never rewrites files.

- [ ] **Step 5: Run loader tests and fixture verification**

Run:

```bash
python3 -m unittest tests.physics_validation.test_reference_package -v
python3 -m tools.physics_validation.reference_package tests/physics_validation/fixtures/reference_package_v1
```

Expected: tests pass; CLI prints dataset ID, version, and `verified` and exits `0`.

- [ ] **Step 6: Commit**

```bash
git add tools/physics_validation/reference_package.py tests/physics_validation/test_reference_package.py tests/physics_validation/fixtures/reference_package_v1
git commit -m "feat: validate offline reference packages"
```

## Task 2: Parse normalized points and compute evidence-backed intervals

**Files:**

- Create: `tools/physics_validation/reference_point.py`
- Create: `tests/physics_validation/test_reference_point.py`
- Modify: `tests/physics_validation/fixtures/reference_package_v1/normalized.csv`

- [ ] **Step 1: Write failing parsing and uncertainty tests**

Specify this immutable API:

```python
@dataclass(frozen=True)
class ReferencePoint:
    dataset_id: str
    series_id: str
    group_id: str
    case_id: str
    point_id: str
    partition: str
    metric: str
    expected: float
    unit: str
    measurement_uncertainty: float
    digitization_uncertainty: float
    conversion_uncertainty: float
    coverage_factor: float
    engineering_absolute_tolerance: float
    engineering_relative_tolerance: float
    source_locator: str
    pool_applicability: str

    @property
    def combined_standard_uncertainty(self) -> float: ...

    @property
    def acceptance_half_width(self) -> float: ...

    @property
    def acceptance_interval(self) -> tuple[float, float]: ...

def read_reference_points(path: Path, dataset_id: str) -> tuple[ReferencePoint, ...]: ...
```

Test RSS with components `0.3`, `0.4`, `0.0` producing `0.5`; coverage `2.0` producing uncertainty width `1.0`; absolute tolerance winning at `1.25`; relative tolerance winning for expected `100.0` and relative `0.02`; and negative, NaN, infinity, duplicate point ID, dataset mismatch, invalid partition, blank source locator, unsupported unit, or extra/missing column rejection.

- [ ] **Step 2: Run the focused test and confirm failure**

```bash
python3 -m unittest tests.physics_validation.test_reference_point -v
```

Expected: import failure for `reference_point`.

- [ ] **Step 3: Implement strict CSV parsing**

Use exactly this header, in this order:

```text
dataset_id,series_id,group_id,case_id,point_id,partition,metric,expected,unit,measurement_uncertainty,digitization_uncertainty,conversion_uncertainty,coverage_factor,engineering_absolute_tolerance,engineering_relative_tolerance,source_locator,pool_applicability
```

Allow the initial units needed by the foundation fixture and planned metrics: `cm`, `cm/s`, `s`, `degree`, `rad/s`, and `dimensionless`. Require `partition` to be `CALIBRATION` or `HOLDOUT`, and `pool_applicability` to be `DIRECT`, `CONVERTED`, `TREND_ONLY`, or `NOT_APPLICABLE`. Return points in CSV order.

- [ ] **Step 4: Refresh the fixture hash intentionally**

```bash
python3 -m tools.physics_validation.reference_package --update-hashes tests/physics_validation/fixtures/reference_package_v1
python3 -m tools.physics_validation.reference_package tests/physics_validation/fixtures/reference_package_v1
```

Expected: hash updater rewrites only `manifest.json`; verification exits `0`.

- [ ] **Step 5: Run focused and regression tests**

```bash
python3 -m unittest tests.physics_validation.test_reference_point tests.physics_validation.test_reference_package -v
```

Expected: all pass.

- [ ] **Step 6: Commit**

```bash
git add tools/physics_validation/reference_point.py tests/physics_validation/test_reference_point.py tests/physics_validation/fixtures/reference_package_v1
git commit -m "feat: model reference uncertainty intervals"
```

## Task 3: Enforce immutable calibration and holdout groups

**Files:**

- Create: `tools/physics_validation/reference_split.py`
- Create: `tests/physics_validation/test_reference_split.py`
- Modify: `tests/physics_validation/fixtures/reference_package_v1/split.json`

- [ ] **Step 1: Write failing split-validation tests**

Define:

```python
@dataclass(frozen=True)
class ReferenceSplit:
    dataset_id: str
    dataset_version: str
    calibration_groups: frozenset[str]
    holdout_groups: frozenset[str]

    def partition_for(self, point: ReferencePoint) -> str: ...

def load_reference_split(path: Path, points: tuple[ReferencePoint, ...],
                         dataset_id: str, dataset_version: str) -> ReferenceSplit: ...
```

Test the valid fixture plus duplicate groups, overlap, missing groups, unknown groups, one case spanning groups, one group spanning partitions, a normalized row whose `partition` disagrees with `split.json`, wrong dataset/version, unsafe IDs, and any point that cannot be assigned exactly once.

- [ ] **Step 2: Run the test and confirm failure**

```bash
python3 -m unittest tests.physics_validation.test_reference_split -v
```

Expected: import failure for `reference_split`.

- [ ] **Step 3: Implement exhaustive group validation**

Use this split schema:

```json
{
  "schema_version": 1,
  "dataset_id": "synthetic_reference",
  "dataset_version": "1.0.0",
  "calibration_groups": ["series_calibration"],
  "holdout_groups": ["series_holdout"]
}
```

Treat `split.json` as authoritative and require every normalized row's redundant partition label to match it. Do not add a runner flag, environment variable, or API parameter that overrides the committed partition.

- [ ] **Step 4: Refresh hashes and run regression tests**

```bash
python3 -m tools.physics_validation.reference_package --update-hashes tests/physics_validation/fixtures/reference_package_v1
python3 -m unittest tests.physics_validation.test_reference_split tests.physics_validation.test_reference_point tests.physics_validation.test_reference_package -v
```

Expected: all pass.

- [ ] **Step 5: Commit**

```bash
git add tools/physics_validation/reference_split.py tests/physics_validation/test_reference_split.py tests/physics_validation/fixtures/reference_package_v1
git commit -m "feat: lock reference calibration splits"
```

## Task 4: Adapt reference cases into canonical scenarios

**Files:**

- Create: `tools/physics_validation/reference_adapter.py`
- Create: `tests/physics_validation/test_reference_adapter.py`
- Modify: `tests/physics_validation/fixtures/reference_package_v1/scenario_template.json`

- [ ] **Step 1: Write failing adapter-contract tests**

Specify:

```python
@dataclass(frozen=True)
class ReferenceCase:
    dataset_id: str
    dataset_version: str
    case_id: str
    partition: str
    scenario_json: str
    points: tuple[ReferencePoint, ...]
    provenance_json: str

class ReferenceAdapterRegistry:
    def register(self, adapter_id: str, factory: Callable) -> None: ...
    def adapt(self, package: ReferencePackage, split: ReferenceSplit,
              points: tuple[ReferencePoint, ...]) -> tuple[ReferenceCase, ...]: ...
```

Test stable case ordering, one output per distinct case ID, no mixed calibration/holdout points, byte-identical canonical JSON on repeated fresh registry runs, immutable string payloads isolated from the template, unknown adapter rejection, duplicate registration rejection, and structured provenance containing dataset/version, point IDs, source locators, and package hashes.

- [ ] **Step 2: Run the test and confirm failure**

```bash
python3 -m unittest tests.physics_validation.test_reference_adapter -v
```

Expected: import failure for `reference_adapter`.

- [ ] **Step 3: Implement an instance-scoped registry and synthetic adapter**

The registry must not use mutable module-global registrations. Register `synthetic_free_roll_v1` from an explicit `default_reference_registry()` factory. The synthetic adapter loads `scenario_template.json`, applies only declared case inputs, produces scenario IDs as `synthetic_reference__<case_id>`, and emits canonical serialized `scenario_json` and `provenance_json`. Its analyzer expectations use metric `value_within_interval`, the reference point ID, expected value, interval lower/upper, and unit. Missing mappings become a `ReferencePackageError` rather than an estimated input.

- [ ] **Step 4: Verify canonical output and package hashes**

```bash
python3 -m tools.physics_validation.reference_package --update-hashes tests/physics_validation/fixtures/reference_package_v1
python3 -m unittest tests.physics_validation.test_reference_adapter tests.physics_validation.test_reference_split -v
```

Expected: all pass and two cases retain their committed partitions.

- [ ] **Step 5: Commit**

```bash
git add tools/physics_validation/reference_adapter.py tests/physics_validation/test_reference_adapter.py tests/physics_validation/fixtures/reference_package_v1
git commit -m "feat: adapt reference data into scenarios"
```

## Task 5: Reconcile failures without hiding physics gaps

**Files:**

- Create: `tools/physics_validation/reference_accounting.py`
- Create: `tests/physics_validation/test_reference_accounting.py`
- Modify: `tests/physics_validation/fixtures/reference_package_v1/expected_model_mismatches.json`
- Modify: `tests/physics_validation/fixtures/reference_package_v1/expected_reference_limitations.json`

- [ ] **Step 1: Write failing strict-accounting tests**

Define:

```python
@dataclass(frozen=True, order=True)
class ReferenceFailureKey:
    dataset_id: str
    case_id: str
    code: str
    metric: str

@dataclass(frozen=True)
class ReferenceAccounting:
    known_model_mismatches: frozenset[ReferenceFailureKey]
    new_model_mismatches: frozenset[ReferenceFailureKey]
    missing_model_mismatches: frozenset[ReferenceFailureKey]
    known_limitations: frozenset[ReferenceFailureKey]
    new_limitations: frozenset[ReferenceFailureKey]
    missing_limitations: frozenset[ReferenceFailureKey]
    unallowlistable_failures: frozenset[ReferenceFailureKey]

    @property
    def ci_passed(self) -> bool: ...

def reconcile_reference_failures(results, model_manifest, limitation_manifest,
                                 dataset_id) -> ReferenceAccounting: ...
```

Test exact match, new failure, unexpectedly missing failure, duplicate manifest item, cross-dataset item, wrong code in either manifest, and attempts to allowlist `NUMERICAL_FAILURE` or `INTEGRATION_MISMATCH`. Confirm model mismatches and limitations remain failures even when known; `ci_passed` means only that both expected sets reconcile exactly and no unallowlistable failure occurred.

- [ ] **Step 2: Run and confirm failure**

```bash
python3 -m unittest tests.physics_validation.test_reference_accounting -v
```

Expected: import failure for `reference_accounting`.

- [ ] **Step 3: Implement closed-schema manifests and four-way accounting**

Both manifests use schema version `1` and entries keyed by `(dataset_id, case_id, code, metric)`. The model manifest accepts only `MODEL_MISMATCH`; the limitation manifest accepts only `REFERENCE_LIMITATION`. Any numerical or integration failure is recorded in `unallowlistable_failures` and makes `ci_passed` false.

- [ ] **Step 4: Refresh hashes and run tests**

```bash
python3 -m tools.physics_validation.reference_package --update-hashes tests/physics_validation/fixtures/reference_package_v1
python3 -m unittest tests.physics_validation.test_reference_accounting tests.physics_validation.test_reference_adapter -v
```

Expected: all pass.

- [ ] **Step 5: Commit**

```bash
git add tools/physics_validation/reference_accounting.py tests/physics_validation/test_reference_accounting.py tests/physics_validation/fixtures/reference_package_v1
git commit -m "feat: reconcile reference failures strictly"
```

## Task 6: Generate partitioned, auditable reports

**Files:**

- Create: `tools/physics_validation/reference_report.py`
- Create: `tests/physics_validation/test_reference_report.py`

- [ ] **Step 1: Write failing report tests**

Define `write_reference_reports(cases, results, accounting, output_directory, metadata)` returning paths for `reference_report.json`, `reference_points.csv`, and `reference_report.md`. Tests must assert:

- calibration and holdout have separate summaries and rows;
- each point records prediction, experimental value, signed error, acceptance interval, uncertainty components, status, dataset version, exact source locator, scenario ID, trace path, build hash, and replay command;
- statuses are exactly `PASSED`, `MODEL_MISMATCH_KNOWN`, `MODEL_MISMATCH_NEW`, `REFERENCE_LIMITATION_KNOWN`, `REFERENCE_LIMITATION_NEW`, `INTEGRATION_MISMATCH`, `NUMERICAL_FAILURE`, or `NON_DETERMINISTIC`;
- known mismatch/limitation rows are never labeled passed or skipped;
- per-series summary includes count, RMSE, maximum absolute error, and pass rate;
- output ordering and bytes are deterministic;
- CSV cells neutralize leading `=`, `+`, `-`, or `@` in textual provenance to prevent spreadsheet formula execution.

- [ ] **Step 2: Run and confirm failure**

```bash
python3 -m unittest tests.physics_validation.test_reference_report -v
```

Expected: import failure for `reference_report`.

- [ ] **Step 3: Implement the three report formats**

Treat JSON as the authoritative report. CSV is the complete point-level numeric export. Markdown is a readable summary with separate calibration and holdout tables plus explicit accounting sections for known/new/missing mismatch, known/new/missing limitation, and unallowlistable failures. Never round JSON/CSV numeric values; Markdown may format display values while linking them to point IDs.

- [ ] **Step 4: Run report and first-phase regression tests**

```bash
python3 -m unittest tests.physics_validation.test_reference_report tests.physics_validation.test_report -v
```

Expected: all pass and first-phase report bytes remain unchanged.

- [ ] **Step 5: Commit**

```bash
git add tools/physics_validation/reference_report.py tests/physics_validation/test_reference_report.py
git commit -m "feat: report public reference comparisons"
```

## Task 7: Execute reference cases through the production process

**Files:**

- Create: `tools/physics_validation/reference_run.py`
- Create: `tests/physics_validation/test_reference_run.py`
- Modify: `tools/physics_validation/analyzer.py`
- Modify: `tests/physics_validation/test_analyzer.py`

- [ ] **Step 1: Add failing generic interval-metric tests**

Add `value_within_interval` support to the existing analyzer. Its expectation value is:

```json
{
  "point_id": "stop_distance_cal_01",
  "observed_metric": "stopping_distance_cm",
  "ball_index": 0,
  "expected": 42.0,
  "lower": 40.5,
  "upper": 43.5,
  "unit": "cm"
}
```

Test an in-range prediction, both boundaries, below/above range as `MODEL_MISMATCH`, malformed/inverted ranges as `REFERENCE_LIMITATION`, missing observed metric as `INTEGRATION_MISMATCH`, and NaN/Infinity as `NUMERICAL_FAILURE`. The actual value must come from metrics derived from trace frames, never from the reference expectation itself.

- [ ] **Step 2: Run analyzer tests and observe the unsupported metric failure**

```bash
python3 -m unittest tests.physics_validation.test_analyzer -v
```

Expected: new cases fail because `value_within_interval` is not implemented.

- [ ] **Step 3: Implement the smallest analyzer extension**

Factor trace-derived scalar metrics into a mapping used by both first-phase and reference expectations, passing the scenario document into the evaluator without changing the public `analyze_scenario(scenario, frames, comparison_frames=None)` signature. Add only the synthetic fixture's `stopping_distance_cm` derivation in this plan: planar displacement from the selected ball's scenario start position to its final trace position. Preserve every existing failure code and first-phase metric result.

- [ ] **Step 4: Write failing runner tests**

Specify:

```python
def run_reference_validation(executable: Path, package: Path,
                             output: Path, execute_once=None) -> int: ...
```

Unit tests inject `execute_once` and assert the sequence: atomically load package, parse points, validate committed split, adapt cases, execute each case twice, detect non-determinism, analyze, reconcile both manifests, write trace/provenance/report artifacts, and return `0` only when accounting passes with no new/missing/unallowlistable failures. Assert that no split override exists in the function or CLI and that an execution exception becomes `INTEGRATION_MISMATCH` rather than aborting remaining cases.

- [ ] **Step 5: Run and confirm runner import failure**

```bash
python3 -m unittest tests.physics_validation.test_reference_run -v
```

Expected: import failure for `reference_run`.

- [ ] **Step 6: Implement the runner by reusing the production executor**

Default `execute_once` to `tools.physics_validation.run._execute_once`; do not copy `AutomationClient` process logic. Compute the executable SHA-256 with the existing `_build_id`. Store trace files under `OUTPUT/traces/<scenario_id>.json` and emit a replay command using the committed package and case ID. CLI arguments are exactly `--executable`, `--package`, `--output`, and optional repeatable `--case`; case filtering may select committed cases but cannot alter their partition.

- [ ] **Step 7: Run unit and first-phase regression suites**

```bash
python3 -m unittest discover -s tests/physics_validation -p 'test_*.py' -v
```

Expected: all tests pass.

- [ ] **Step 8: Commit**

```bash
git add tools/physics_validation/reference_run.py tools/physics_validation/analyzer.py tests/physics_validation/test_reference_run.py tests/physics_validation/test_analyzer.py
git commit -m "feat: run reference cases through production"
```

## Task 8: Add real-process smoke coverage, documentation, and CI artifacts

**Files:**

- Create: `tests/e2e/test_reference_validation.py`
- Create: `docs/reference-data-packages.md`
- Modify: `CMakeLists.txt`
- Modify: `.github/workflows/ci.yml`

- [ ] **Step 1: Write the real-process smoke test**

Model it on `tests/e2e/test_physics_validation.py`. Accept the built Billiards executable as `sys.argv[1]` and a stable output directory as `sys.argv[2]`; clear and recreate only that directory at test start. Run `run_reference_validation` against `synthetic_reference_v1`, then assert exit `0`, exactly two scenario traces, separate nonempty calibration/holdout summaries, the executable `sha256:` build ID, committed package hashes, and both point IDs in JSON and CSV.

- [ ] **Step 2: Register and run the integration test**

Add a CTest entry named `BilliardsReferenceValidationE2E` with the same runtime environment and timeout pattern as `BilliardsPhysicsValidationE2E`. Pass `${CMAKE_CURRENT_BINARY_DIR}/physics-reference-validation` as the stable output directory. Build and run:

```bash
cmake --build build --parallel
ctest --test-dir build --output-on-failure -R '^BilliardsReferenceValidationE2E$'
```

Expected: the new CTest passes because Task 7 already completed the runner contract. If it fails, preserve the exact failure and fix only process/path/metadata integration in the next step.

- [ ] **Step 3: Complete only the integration needed by the smoke test**

Fix path handling and metadata wiring in the new reference modules. Do not modify game physics or weaken expected intervals. Keep the synthetic values chosen so the current production executable passes; they validate plumbing, not real-world accuracy.

- [ ] **Step 4: Document the package contract and maintainer workflow**

In `docs/reference-data-packages.md`, document every manifest/CSV/split field, uncertainty formula, exact failure taxonomy, permission rule, hash-update command, package verification command, case replay command, split-change versioning rule, and data-admission checklist. Explicitly state UTF-8 encoding, offline operation, full numerical-data commitment, no tolerance expansion after observing simulation, and that source packages require plans 2–5.

- [ ] **Step 5: Upload reference artifacts in CI**

Extend `.github/workflows/ci.yml` to upload `build/physics-reference-validation/` so the generated JSON, CSV, Markdown, and traces are retained on both success and failure using `if: always()`. Do not download source material or install new Python packages. Preserve the existing physics validation artifact.

- [ ] **Step 6: Run the complete verification set from a clean build**

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --parallel
python3 -m unittest discover -s tests/physics_validation -p 'test_*.py' -v
ctest --test-dir build --output-on-failure -R 'Billiards(Physics|Reference)ValidationE2E'
python3 -m tools.physics_validation.reference_package tests/physics_validation/fixtures/reference_package_v1
git diff --check
```

Expected: configure/build succeeds, all Python tests pass, both CTests pass, package verification exits `0`, and `git diff --check` prints nothing.

- [ ] **Step 7: Audit the plan boundary before committing**

Run:

```bash
git diff --name-only HEAD~7
rg -n 'Mathavan|Dom.nech|Cross' tests/physics_validation/fixtures/reference_package_v1
git diff HEAD~7 -- src include
```

Expected: the fixture contains no paper-derived names or values; the `src`/`include` diff is empty; all changes belong to reference infrastructure, tests, docs, CMake, or CI.

- [ ] **Step 8: Commit**

```bash
git add tests/e2e/test_reference_validation.py docs/reference-data-packages.md CMakeLists.txt .github/workflows/ci.yml
git commit -m "test: integrate offline reference validation"
```

## Final acceptance checklist

- [ ] A corrupt, incomplete, unsafe, or hash-mismatched package fails before any case executes.
- [ ] Every normalized number is finite, unit-tagged, provenance-linked, and included in a reproducible interval.
- [ ] Calibration and holdout are disjoint, exhaustive, grouped, versioned, and not CLI-overridable.
- [ ] Adapters are deterministic, source-agnostic, and never infer missing experimental conditions.
- [ ] Production physics runs through the existing automation process; no second physics implementation exists.
- [ ] Model mismatch, reference limitation, integration failure, and numerical failure are independently visible and strictly reconciled.
- [ ] JSON/CSV retain complete numerical precision and enough metadata for offline replay and later tuning.
- [ ] First-phase tests and reports remain compatible.
- [ ] No published experimental values or physics tuning entered this foundation plan.
