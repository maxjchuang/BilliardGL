# Doménech 2023 Ball-Collision Reference Implementation Plan

> **Required subskill:** Use `superpowers:test-driven-development` for every implementation task and `superpowers:verification-before-completion` before committing the implementation.

**Goal:** Turn the open-access Doménech-Carbó 2023 ball-collision experiments into a reproducible offline benchmark for impact/scattering angles, with regulation billiard, steel, brass, and rubber series kept materially distinct.

**Architecture:** Build a versioned source package from the article's experimental markers and requested author data, then map only directly observable experimental conditions into canonical two-ball scenarios. Reuse the adapter package and event-window metric layer introduced by the Mathavan 2009 plan. Theory curves and fitted IFR coefficients remain provenance context, never experimental expected values.

**Tech Stack:** Python 3 standard library, existing reference-package APIs, canonical physics scenario v1, production automation runtime, `unittest`, CTest.

## Global Constraints

- Implement after `2026-07-13-mathavan-2009-high-speed-reference.md`; do not duplicate common adapter or angle-metric code.
- Do not change production collision response, coefficients of restitution/friction, ball dimensions, table surface, or solver behavior.
- Commit every admitted numeric point and all reconstruction metadata. Runtime and CI remain offline.
- The ScienceDirect landing page declares the article open access under a Creative Commons license, but the implementation must record the exact license URI from the version of record before deciding whether the PDF or figure images are redistributable.
- Experimental markers are evidence; IFR equations, predicted curves, and fitted coefficients are not ground truth.
- Keep regulation billiard, steel, brass, and rubber records in separate `series_id` and `group_id` namespaces. Never use a non-billiard material to pass a WPA Pool case.
- Treat the PVC laboratory bench as a material difference from a cloth-covered Pool table. Regulation-ball angle observations may be `CONVERTED` only when the measured phase is isolated before surface friction changes direction; otherwise use `TREND_ONLY` with a registered limitation.
- Pre-register split membership before running BilliardGL. Whole material/impact-angle series are indivisible; high-obliquity and stick/slip-boundary series belong to holdout.

## Source Admission Record

- Citation: Antonio Doménech-Carbó, “Independent friction-restitution description of billiard ball collisions,” *Mechanics Research Communications* 131, 104149 (2023), DOI `10.1016/j.mechrescom.2023.104149`.
- Version-of-record URL: `https://www.sciencedirect.com/science/article/pii/S0093641323001076`; the page states open access, Creative Commons licensing, and “Data will be made available on request.”
- Experimental section: regulation billiard balls `61 mm`, `205.0 g`; steel `25.0 mm`, `70.30 g`; brass `25.0 mm`, `68.20 g`; rubber `46.0 mm`, `46.40 g`; PVC supporting surface.
- Cue sphere launch: `0.80 ± 0.05 m/s`, released from a slanted track ending 50 cm before contact to establish pure rolling.
- Measurement: impact and scattering angles from zenithal photographs, camera 75 cm above contact.
- Admission gate: obtain the lawful version-of-record PDF and, because the article offers data on request, send/store an author-data request outside the repository. The package may normalize digitized markers immediately, but author-supplied tables supersede digitization only through a dataset version bump with both versions retained in history.

## File Structure

**Create:**

- `tools/physics_validation/adapters/domenech_2023.py`
- `tools/physics_validation/extract_domenech_2023.py`
- `tests/physics_validation/test_domenech_2023_extraction.py`
- `tests/physics_validation/test_domenech_2023_adapter.py`
- `tests/physics_validation/test_domenech_2023_metrics.py`
- `tests/physics_validation/reference_data/domenech_2023_ball_collision/manifest.json`
- `tests/physics_validation/reference_data/domenech_2023_ball_collision/raw_extracted.csv`
- `tests/physics_validation/reference_data/domenech_2023_ball_collision/digitization.csv`
- `tests/physics_validation/reference_data/domenech_2023_ball_collision/normalized.csv`
- `tests/physics_validation/reference_data/domenech_2023_ball_collision/extraction.json`
- `tests/physics_validation/reference_data/domenech_2023_ball_collision/split.json`
- `tests/physics_validation/reference_data/domenech_2023_ball_collision/scenario_template.json`
- `tests/physics_validation/reference_data/domenech_2023_ball_collision/expected_model_mismatches.json`
- `tests/physics_validation/reference_data/domenech_2023_ball_collision/expected_reference_limitations.json`

**Modify:**

- `tools/physics_validation/reference_adapter.py` — register `domenech_2023_v1`.
- `tools/physics_validation/analyzer.py` — add explicit immediate-post-impact versus post-transition angle selection if not fully supplied by the first plan.
- `tests/physics_validation/test_analyzer.py`
- `docs/reference-data-packages.md`
- `.github/workflows/ci.yml`

## Task 1: Complete the License and Evidence Admission Gate

**Files:** Create manifest/raw/digitization/limitation files and `test_domenech_2023_extraction.py`.

1. Write a failing test requiring dataset ID `domenech_2023_ball_collision`, version `1.0.0`, adapter `domenech_2023_v1`, DOI, exact Creative Commons URI, acquisition date/URL, source SHA-256, apparatus, camera geometry, and data-request status.
2. Require an `evidence_inventory` entry for every experimental figure/table with journal page, figure panel, axis definitions, material, marker legend, counted marker total, and whether values came from author tables or dual digitization.
3. Manually inspect the version-of-record PDF before entering numbers. Record every experimental panel and marker count in `manifest.json`; reject normalization if a raw series count differs from its inventory.
4. Populate raw rows only from experimental markers/tables. Record IFR curves in `extraction.json` as excluded evidence so a later contributor cannot silently import them.
5. For each digitized panel, record two independent pixel-coordinate passes, axis anchors, residuals, marker shape/color, and material mapping. If a marker cannot be assigned unambiguously, register `ambiguous_figure_marker` with the exact panel and release condition.
6. Run `python3 -m unittest tests.physics_validation.test_domenech_2023_extraction -v`; expect admission tests to pass and normalization tests to remain failing.

## Task 2: Normalize Material-Series Data and Freeze the Split

**Files:** Create extractor, normalized/extraction/split files; extend extraction tests.

1. Add failing tests for:

```python
def normalize_rows(raw_path: Path, digitization_path: Path,
                   extraction_path: Path) -> tuple[dict, ...]: ...

def write_normalized(package_path: Path) -> bytes: ...
```

2. Normalize length to cm, speed to cm/s, mass to kg in scenario metadata, and all angles to degrees with a declared positive orientation. Preserve the source's impact-angle and scattering-angle definitions verbatim in provenance.
3. Propagate launch-speed uncertainty (`±5 cm/s` as a reported bound), camera/digitization uncertainty, and angular calibration residual independently. Do not infer an angular error from the model fit.
4. Assign complete experimental groups: representative low/middle obliquity regulation-ball groups to calibration; remaining regulation-ball groups and all boundary/extreme groups to holdout. Mark steel, brass, and rubber groups `TREND_ONLY`; split roughly one-third/two-thirds within each material without mixing adjacent points from one plotted curve.
5. Require at least one holdout group per material and forbid CLI split overrides.
6. Generate stable sorted bytes, update package hashes, and run:

```bash
python3 -m tools.physics_validation.extract_domenech_2023 \
  --package tests/physics_validation/reference_data/domenech_2023_ball_collision --check
```

Expected: exit 0 and byte-identical `normalized.csv`.

## Task 3: Distinguish Collision-Time and Post-Transition Observations

**Files:** Modify analyzer/tests and create `test_domenech_2023_metrics.py`.

1. Write frame/contact fixtures for `post_collision_linear_velocity_cm_s`, `post_collision_angular_velocity_rad_s`, `separation_angle_degrees`, and `stick_slip_classification` at `immediate_post_impact` and `first_pure_roll_after_event` phases.
2. Extend expectation selection with explicit `sample_phase`, two ball indices, angle reference axis, and minimum stable-window ticks. Never infer phase from the expected experimental value.
3. Derive scattering angles from velocity vectors. For separation angle, compute the smaller angle between cue/object velocity vectors in `[0,180]`. Classify stick/slip only from finite relative contact velocity and a declared epsilon recorded in scenario provenance.
4. Missing angular velocity/contact samples are `INTEGRATION_MISMATCH`; absent experimental phase/definition is `REFERENCE_LIMITATION`; finite disagreement is `MODEL_MISMATCH`; non-finite output is `NUMERICAL_FAILURE`.
5. Run `python3 -m unittest tests.physics_validation.test_domenech_2023_metrics tests.physics_validation.test_analyzer -v`; expect all tests to pass.

## Task 4: Map Regulation and Comparison Materials Without Conflation

**Files:** Create adapter/template/tests; modify registry.

1. Write failing deterministic-adapter tests for all four material namespaces and rejected mixed-material cases.
2. Implement:

```python
def adapt_domenech_2023(package: ReferencePackage,
                        split: ReferenceSplit,
                        points: tuple[ReferencePoint, ...]) -> tuple[ReferenceCase, ...]: ...
```

3. Generate two equal spheres with source diameter/mass metadata, object ball at rest, cue ball at `80 cm/s` in pure roll, and lateral offset derived from the source impact-angle convention. The engine's fixed ball geometry remains unchanged; if a source diameter cannot be expressed without altering production constants, emit `source_geometry_not_expressible` rather than relabeling it.
4. Admit regulation-ball cases only when geometry and measured phase are expressible. Emit structured limitations for PVC-to-cloth surface conversion and for all steel/brass/rubber material properties that the scenario schema cannot install.
5. Preserve comparison-material normalized data and reports even when no runtime scenario is emitted. The report must count these as registered limitations, not skipped tests.
6. Register `domenech_2023_v1`; re-run all prior adapter tests to prove no registry regression.

## Task 5: Execute, Account, and Report

**Files:** Populate strict manifests and update report tests if a material-series summary field is needed.

1. Run:

```bash
python3 -m tools.physics_validation.reference_run \
  --executable build/check/Billiards \
  --package tests/physics_validation/reference_data/domenech_2023_ball_collision \
  --output build/physics-reference/domenech-2023
```

2. Lock exact model mismatches without changing acceptance intervals. Lock reference limitations for unexpressible geometry/material/surface/phase only.
3. Require JSON/CSV/Markdown to show material, impact angle, measured phase, experimental/predicted angle, interval, partition, applicability, source panel, and replay command.
4. Require strict equality of actual and declared mismatch/limitation tuples and zero integration/numerical failures.
5. Replay one regulation holdout case and verify identical scenario/trace/report hashes.

## Task 6: Add Offline Fast and Full Verification

**Files:** Modify docs and CI.

1. Document exact license conclusion, admitted panel/point counts, author-data request status, material applicability, split, extraction, full run, and replay.
2. Add PR-fast checks for schema/hash/reconstruction/unit tests and one regulation-ball holdout case. No source fetch is allowed.
3. Document the full all-material audit as a scheduled/manual command; its artifact must preserve limitation-only series.
4. Run:

```bash
python3 -m unittest discover -s tests/physics_validation -p 'test_*.py' -v
ctest --test-dir build/check --output-on-failure
git diff --exit-code -- tests/physics_validation/reference_data/domenech_2023_ball_collision/normalized.csv
```

Expected: all tests pass, normalized bytes reproduce, no undeclared accounting entry exists.

## Plan Self-Review Checklist

- The exact Creative Commons license and every experimental panel are audited before numeric admission.
- Material series never share IDs, split groups, scenario assumptions, or Pool acceptance status.
- Experimental markers and IFR predictions are structurally distinguishable.
- PVC surface and fixed-engine geometry differences remain visible limitations.
- Immediate-post-impact and post-transition metrics cannot be confused.
- All runtime inputs are committed and offline; no production physics changes are planned.
- A scan for unfinished drafting markers returns no matches.
