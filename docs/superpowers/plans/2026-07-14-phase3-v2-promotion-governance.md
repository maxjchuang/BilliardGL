# Phase 3 v2 Promotion Governance Implementation Plan

**Execution status: Completed and archived.** Unchecked boxes below preserve the original execution script; current disposition and evidence are indexed in [Phase 3 current status](../../phase3-physics-current-status.md).

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Reject the historical v1 release without changing its evidence and make every v2 promotion, artifact, source, and release check fail closed.

**Architecture:** Keep v1 bytes immutable and add a hash-bound rejection record beside them. Introduce schema-v2 validators that accept only `PASSED_OR_ACCOUNTED` candidate receipts and only `PASSED` releases, with Git ancestry/default-profile verification delegated through explicit command inputs so unit tests remain deterministic.

**Tech Stack:** Python 3 standard library, `unittest`, Git CLI, canonical JSON, SHA-256.

## Global Constraints

- Never regenerate or edit a v1 report, receipt, split, freeze, trace, or release manifest.
- `FAILED` is never translated into a successful disposition.
- Every required artifact is repository-relative, hash-bound, and present in the complete inventory.
- Release source revision must be an ancestor of `HEAD` and contain the production default profile used by the executable.
- Each task below ends in one independently reviewable commit.

---

### Task 1: Make candidate receipt validation fail closed

**Files:**
- Modify: `tools/physics_validation/promotion.py`
- Modify: `tests/physics_validation/test_promotion.py`
- Create: `tests/physics_validation/fixtures/promotion_v2.json`

**Interfaces:**
- Consumes: candidate manifest `schema_version`, `candidates[*].receipts[*]`, and each receipt's canonical JSON.
- Produces: `validate_promotion_manifest(path: Path, root: Path) -> list[str]`, where success requires receipt result `PASSED_OR_ACCOUNTED` and empty failure/accounting arrays.

- [ ] **Step 1: Write a failing test that proves `FAILED` cannot be accounted away**

```python
def test_failed_receipt_is_never_promotable(self):
    document = json.loads(MANIFEST.read_text(encoding="utf-8"))
    candidate = document["candidates"][0]
    candidate["validation_disposition"] = "limitations_preserved"
    with tempfile.TemporaryDirectory() as directory:
        path = Path(directory) / "promotion.json"
        path.write_text(json.dumps(document), encoding="utf-8")
        failures = validate_promotion_manifest(path, ROOT)
    self.assertTrue(any("receipt did not pass" in value for value in failures))

def test_v2_receipt_requires_empty_failure_accounting(self):
    document = json.loads(FIXTURE.read_text(encoding="utf-8"))
    receipt = document["candidates"][0]["receipts"][0]
    receipt_path = ROOT / receipt["path"]
    original = json.loads(receipt_path.read_text(encoding="utf-8"))
    self.assertEqual(original["result"], "PASSED_OR_ACCOUNTED")
    self.assertEqual(original["unallowlistable_failures"], [])
    self.assertEqual(original["new_model_mismatches"], [])
    self.assertEqual(original["new_limitations"], [])
    self.assertEqual(original["missing_expected_failures"], [])
```

- [ ] **Step 2: Run the focused tests and observe the historical acceptance**

Run: `python3 -m unittest tests.physics_validation.test_promotion -v`

Expected: `test_failed_receipt_is_never_promotable` fails because the current validator maps `FAILED` to `limitations_preserved`.

- [ ] **Step 3: Replace disposition inference with an exact receipt contract**

```python
REQUIRED_EMPTY_RECEIPT_FIELDS = (
    "unallowlistable_failures",
    "new_model_mismatches",
    "new_limitations",
    "missing_expected_failures",
)


def _validate_receipt(receipt_path):
    receipt = json.loads(receipt_path.read_text(encoding="utf-8"))
    failures = []
    if receipt.get("result") != "PASSED_OR_ACCOUNTED":
        failures.append(f"receipt did not pass: {receipt_path.as_posix()}")
    for field in REQUIRED_EMPTY_RECEIPT_FIELDS:
        if receipt.get(field) != []:
            failures.append(f"receipt {field} is not empty: {receipt_path.as_posix()}")
    return failures
```

In `validate_promotion_manifest`, call `_validate_receipt(target)` for every receipt and require `candidate["validation_disposition"] == "passed"`; remove the branch that converts `FAILED` to `limitations_preserved`.

- [ ] **Step 4: Run focused and release-gate regressions**

Run: `python3 -m unittest tests.physics_validation.test_promotion tests.physics_validation.test_phase3_release_gate -v`

Expected: all tests pass; the historical inventory test now explicitly expects the known receipt failures rather than claiming the v1 inventory is promotable.

- [ ] **Step 5: Commit the strict receipt semantics**

```bash
git add tools/physics_validation/promotion.py tests/physics_validation/test_promotion.py tests/physics_validation/fixtures/promotion_v2.json
git commit -m "fix: make candidate promotion fail closed"
```

### Task 2: Preserve and explicitly reject the historical v1 release

**Files:**
- Create: `tools/physics_validation/historical_rejection.py`
- Create: `tests/physics_validation/test_historical_rejection.py`
- Create: `physics_models/promotion/phase3_release_v1_rejection.json`

**Interfaces:**
- Consumes: immutable `physics_models/promotion/phase3_release_v1.json`, v1 receipts, and rejecting Git revision.
- Produces: `validate_historical_rejection(path: Path, root: Path) -> list[str]` and a schema-v1 rejection record.

- [ ] **Step 1: Write failing tests for byte binding and complete finding inventory**

```python
class HistoricalRejectionTests(unittest.TestCase):
    def test_committed_rejection_binds_original_release(self):
        self.assertEqual(validate_historical_rejection(REJECTION, ROOT), [])

    def test_changed_release_hash_is_rejected(self):
        document = json.loads(REJECTION.read_text(encoding="utf-8"))
        document["rejected_release_sha256"] = "0" * 64
        with tempfile.TemporaryDirectory() as directory:
            path = Path(directory) / "rejection.json"
            path.write_text(json.dumps(document), encoding="utf-8")
            failures = validate_historical_rejection(path, ROOT)
        self.assertIn("rejected release hash mismatch", failures)

    def test_every_failed_receipt_is_named(self):
        document = json.loads(REJECTION.read_text(encoding="utf-8"))
        self.assertEqual(
            set(document["failed_receipts"]),
            discover_failed_v1_receipts(ROOT),
        )
```

- [ ] **Step 2: Run the test and verify the module is absent**

Run: `python3 -m unittest tests.physics_validation.test_historical_rejection -v`

Expected: import failure for `tools.physics_validation.historical_rejection`.

- [ ] **Step 3: Implement deterministic rejection validation**

```python
def discover_failed_v1_receipts(root):
    receipts = set()
    for path in sorted((Path(root) / "physics_models/candidates").glob("*_v1/validation/validation_receipt.json")):
        if json.loads(path.read_text(encoding="utf-8")).get("result") == "FAILED":
            receipts.add(path.relative_to(root).as_posix())
    return receipts


def validate_historical_rejection(path, root):
    root = Path(root)
    document = json.loads(Path(path).read_text(encoding="utf-8"))
    failures = []
    release = root / document.get("rejected_release_path", "")
    if document.get("schema_version") != 1 or document.get("status") != "REJECTED":
        failures.append("historical rejection status or schema is invalid")
    if not release.is_file() or hashlib.sha256(release.read_bytes()).hexdigest() != document.get("rejected_release_sha256"):
        failures.append("rejected release hash mismatch")
    if set(document.get("failed_receipts", [])) != discover_failed_v1_receipts(root):
        failures.append("failed receipt inventory mismatch")
    if not document.get("unallowlistable_integration_failures"):
        failures.append("integration failure inventory is empty")
    if not document.get("review_findings"):
        failures.append("review finding inventory is empty")
    if len(document.get("rejecting_source_revision", "")) != 40:
        failures.append("rejecting source revision is not immutable")
    return failures
```

Populate the rejection manifest from already committed v1 receipts only. Record the Mathavan 2009 cross-phase selection, Mathavan 2010 invalid initial geometry, Doménech second collision, solver incompleteness, partial-state fallback, and non-executable full-game cases as explicit cause-coded findings.

- [ ] **Step 4: Verify v1 files were not modified**

Run: `git diff --exit-code ddc3b82 -- physics_models/candidates '*phase3_release_v1.json' && python3 -m unittest tests.physics_validation.test_historical_rejection -v`

Expected: no diff for preserved v1 evidence and all rejection tests pass.

- [ ] **Step 5: Commit the immutable rejection record**

```bash
git add tools/physics_validation/historical_rejection.py tests/physics_validation/test_historical_rejection.py physics_models/promotion/phase3_release_v1_rejection.json
git commit -m "docs: reject historical phase 3 v1 release"
```

### Task 3: Validate complete artifacts and frozen input bindings

**Files:**
- Modify: `tools/physics_validation/validation_artifacts.py`
- Create: `tools/physics_validation/freeze_verifier.py`
- Modify: `tests/physics_validation/test_complete_validation_artifacts.py`
- Create: `tests/physics_validation/test_freeze_verifier.py`

**Interfaces:**
- Consumes: candidate freeze, artifact inventory, candidate profile, executable, package manifests, calibration reports, supplemental numeric inputs.
- Produces: `validate_freeze(freeze_path: Path, root: Path, executable: Path | None) -> list[str]`.

- [ ] **Step 1: Write failing tests for an omitted supplemental input and mutated executable**

```python
def test_inventory_requires_every_declared_supplemental_input(self):
    inventory = build_validation_artifact_manifest(ROOT, INVENTORY)
    paths = {entry["path"] for entry in inventory["artifacts"]}
    self.assertIn("tests/physics_validation/reference_data/sudo_2002/scalars.csv", paths)

def test_freeze_rejects_changed_executable(self):
    with tempfile.TemporaryDirectory() as directory:
        executable = Path(directory) / "physics-scenario"
        executable.write_bytes(b"changed")
        failures = validate_freeze(FREEZE, ROOT, executable)
    self.assertIn("freeze executable hash mismatch", failures)
```

- [ ] **Step 2: Run the focused tests and observe missing v2 bindings**

Run: `python3 -m unittest tests.physics_validation.test_complete_validation_artifacts tests.physics_validation.test_freeze_verifier -v`

Expected: failures because supplemental roles and `validate_freeze` are not implemented.

- [ ] **Step 3: Implement role-complete inventory and freeze verification**

```python
REQUIRED_ARTIFACT_ROLES = {
    "profile", "executable", "calibration_report", "source_manifest",
    "source_numeric_input", "split", "metric_contract", "receipt",
    "trace", "provenance", "full_game_matrix", "performance_budget",
}


def validate_freeze(freeze_path, root, executable=None):
    root = Path(root)
    freeze = json.loads(Path(freeze_path).read_text(encoding="utf-8"))
    failures = []
    roles = {item.get("role") for item in freeze.get("artifacts", [])}
    missing = REQUIRED_ARTIFACT_ROLES - roles
    if missing:
        failures.append(f"freeze roles missing: {sorted(missing)}")
    for item in freeze.get("artifacts", []):
        target = root / item.get("path", "")
        if not target.is_file() or _sha256(target) != item.get("sha256"):
            failures.append(f"freeze artifact mismatch: {item.get('path')}")
    if executable is not None and _sha256(Path(executable)) != freeze.get("executable_sha256"):
        failures.append("freeze executable hash mismatch")
    return failures
```

Extend artifact manifest validation so every declared freeze path occurs exactly once with the same digest and role; reject duplicate paths, undeclared files under frozen output directories, absolute paths, symlinks, and `..` traversal.

- [ ] **Step 4: Run artifact and freeze regressions**

Run: `python3 -m unittest tests.physics_validation.test_complete_validation_artifacts tests.physics_validation.test_freeze_verifier -v`

Expected: all tests pass without executing any HOLDOUT or confirmation partition.

- [ ] **Step 5: Commit freeze and inventory validation**

```bash
git add tools/physics_validation/validation_artifacts.py tools/physics_validation/freeze_verifier.py tests/physics_validation/test_complete_validation_artifacts.py tests/physics_validation/test_freeze_verifier.py
git commit -m "feat: verify complete frozen validation inputs"
```

### Task 4: Enforce strict v2 release source and default-profile checks

**Files:**
- Modify: `tools/physics_validation/promotion.py`
- Modify: `tools/physics_validation/phase3_release_gate.py`
- Modify: `scripts/check_phase3_physics_release.py`
- Modify: `tests/physics_validation/test_phase3_release_gate.py`
- Create: `tests/physics_validation/fixtures/phase3_release_v2.json`

**Interfaces:**
- Consumes: release schema v2, repository `HEAD`, executable `--print-physics-profile`, promotion/freeze/artifact validators.
- Produces: `validate_release_manifest(path, root, executable=None, head_revision=None) -> list[str]`; successful status is exactly `PASSED`.

- [ ] **Step 1: Write failing source-ancestry, status, and runtime-profile tests**

```python
def test_v2_release_accepts_only_passed(self):
    document = json.loads(FIXTURE.read_text(encoding="utf-8"))
    document["status"] = "PASSED_WITH_DECLARED_LIMITATIONS"
    failures = validate_document(document, head_revision=document["source_revision"])
    self.assertIn("v2 release status must be PASSED", failures)

def test_release_source_must_be_ancestor(self):
    failures = validate_phase3_release(
        ROOT, release_path=FIXTURE, head_revision="f" * 40,
        is_ancestor=lambda source, head: False,
    )
    self.assertIn("release source revision is not an ancestor of HEAD", failures)

def test_executable_default_profile_must_match_freeze(self):
    failures = validate_phase3_release(
        ROOT, release_path=FIXTURE,
        executable_profile_id="chinese_pool_full_game_v1",
    )
    self.assertIn("production default profile differs from frozen profile", failures)
```

- [ ] **Step 2: Run the release-gate tests and observe permissive v1 semantics**

Run: `python3 -m unittest tests.physics_validation.test_phase3_release_gate -v`

Expected: the three new tests fail because schema v2, ancestry injection, and runtime profile verification do not exist.

- [ ] **Step 3: Implement explicit source and runtime checks**

```python
def _git_is_ancestor(root, source, head):
    completed = subprocess.run(
        ["git", "merge-base", "--is-ancestor", source, head],
        cwd=root, check=False, capture_output=True, text=True,
    )
    return completed.returncode == 0


def _executable_profile_id(executable):
    completed = subprocess.run(
        [str(executable), "--print-physics-profile"],
        check=True, capture_output=True, text=True,
    )
    return json.loads(completed.stdout)["id"]
```

For schema v2, require `status == "PASSED"`, `unexplained_regressions == 0`, every nested validator to return no failures, source ancestry to hold, and the executable profile ID/digest to match the release profile. Keep schema-v1 parsing only to report that `phase3_release_v1` is historical and rejected.

- [ ] **Step 4: Run governance tests and the non-HOLDOUT release checker**

Run: `python3 -m unittest discover -s tests/physics_validation -p 'test_*promotion*.py' -v && python3 -m unittest tests.physics_validation.test_phase3_release_gate tests.physics_validation.test_historical_rejection -v`

Expected: all tests pass; no validation runner is invoked.

- [ ] **Step 5: Commit strict release governance**

```bash
git add tools/physics_validation/promotion.py tools/physics_validation/phase3_release_gate.py scripts/check_phase3_physics_release.py tests/physics_validation/test_phase3_release_gate.py tests/physics_validation/fixtures/phase3_release_v2.json
git commit -m "feat: enforce strict phase 3 v2 release governance"
```

## Plan Verification

- Run `rg -n 'limitations_preserved|PASSED_WITH_DECLARED_LIMITATIONS' tools/physics_validation scripts tests/physics_validation` and verify successful schema-v2 paths contain neither value.
- Run `git diff --exit-code ddc3b82 -- physics_models/candidates physics_models/promotion/phase3_release_v1.json` to prove preserved v1 bytes are untouched.
- Run `python3 -m unittest tests.physics_validation.test_promotion tests.physics_validation.test_historical_rejection tests.physics_validation.test_complete_validation_artifacts tests.physics_validation.test_freeze_verifier tests.physics_validation.test_phase3_release_gate -v` and expect all tests to pass.
