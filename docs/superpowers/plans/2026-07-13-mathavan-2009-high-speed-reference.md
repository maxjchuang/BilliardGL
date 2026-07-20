# Mathavan 2009 High-Speed Reference Implementation Plan

> **Required subskill:** Use `superpowers:test-driven-development` for every implementation task and `superpowers:verification-before-completion` before committing the implementation.

**Goal:** Convert the experimentally reported Mathavan, Jackson, and Parkin 2009 high-speed-camera results into a complete, offline, auditable reference package that drives the production runtime without changing physics formulas or parameters.

**Architecture:** Add one source-specific package under `tests/physics_validation/reference_data/` and one pure adapter module. Extend the common point/analyzer layer only for trace-derived quantities actually measured by this paper. Raw transcription and graph digitization are reconstructed by a deterministic extraction script; normalized rows remain the only CI runtime input.

**Tech Stack:** Python 3 standard library, existing `tools.physics_validation` package, canonical physics scenario v1, production `Billiards --automation --transport stdio --headless`, `unittest`, CTest.

## Global Constraints

- This is the first source-specific phase-2 plan and must be implemented before the other three source plans.
- Do not modify collision, friction, spin, cushion, pocket, cue-impact formulas, solver time stepping, or material constants.
- Commit every numerical value used by validation. CI and replay must not require a PDF, website, network request, image editor, or spreadsheet application.
- Do not commit the article PDF or screenshots unless a separate redistribution audit proves permission. Record its SHA-256 and lawful retrieval URL in metadata.
- Treat this paper's Riley Renaissance snooker table, 52.4 mm snooker balls, cloth, and cushion as non-WPA equipment. Only dimensionless or explicitly justified geometry-independent observations may be `DIRECT`; otherwise use `TREND_ONLY` or `REFERENCE_LIMITATION`.
- Never substitute the paper's theoretical curves for experimental observations. In Table I, admit only the measured columns as reference values; keep theoretical columns solely as transcription cross-checks.
- Pre-register split assignments before running BilliardGL. Group whole experimental series, target approximately one-third calibration and two-thirds holdout, and keep high-speed/extreme-impact groups in holdout.
- Every admitted value must trace to DOI `10.1119/1.3157159`, a journal page, figure/table, extraction method, uncertainty basis, and immutable package hash.

## Source Admission Record

The implementation must encode these already-audited facts and reject contradictory extraction metadata:

- Citation: S. Mathavan, M. R. Jackson, R. M. Parkin, “Application of high-speed imaging to determine the dynamics of billiards,” *American Journal of Physics* 77(9), 788–794 (2009), DOI `10.1119/1.3157159`.
- Acquisition URL: `https://doi.org/10.1119/1.3157159`; retrieval copy used for extraction must be hashed locally but is not a CI dependency.
- Apparatus, journal pp. 788–789: 10 ft × 5 ft Riley Renaissance snooker table, 52.4 mm balls, overhead PixeLINK PL-B776F camera, up to 1000 fps, 1 mm spatial resolution.
- Surface-friction observations, journal p. 790, Fig. 6: rolling deceleration `0.124–0.126 m/s²`; sliding deceleration `1.75–2.40 m/s²`. These are reported ranges, not Gaussian standard deviations.
- Normal cushion observations, journal pp. 790–791, Figs. 7–9: 31 no-sidespin, approximately pure-rolling shots; incident range `0.28–3.5 m/s`; fitted experimental relation `v_rebound = -0.0877 v_incident² + 1.131 v_incident - 0.0953`; the fit is supporting metadata, not a replacement for the 31 digitized markers.
- Oblique ball collision observations, journal pp. 792–793, Fig. 12 and Table I: Table I contains five shots and measured post-slip cue/object speeds. Its exact rows are `(1.539,33.83,0.816,0.836)`, `(1.032,26.36,0.520,0.629)`, `(1.364,40.52,0.925,0.700)`, `(1.731,46.50,1.275,0.787)`, `(0.942,18.05,0.365,0.581)` in `(incoming m/s, cut degree, cue m/s, object m/s)` order.
- Known evidence limit, journal pp. 792–793: ball spin was not measured. Fig. 12 separation-angle markers may be preserved as `TREND_ONLY`, but cannot become Pool numerical acceptance points; general spin-dependent cushion impact is also explicitly unavailable.

## File Structure

**Create:**

- `tools/physics_validation/adapters/__init__.py` — source adapter package.
- `tools/physics_validation/adapters/mathavan_2009.py` — deterministic normalized-row to scenario mapping.
- `tools/physics_validation/extract_mathavan_2009.py` — raw transcription/digitization reconstruction and normalization CLI.
- `tests/physics_validation/test_mathavan_2009_extraction.py` — exact reconstruction, uncertainty, and source-locator tests.
- `tests/physics_validation/test_mathavan_2009_adapter.py` — mapping, partition, determinism, and limitation tests.
- `tests/physics_validation/test_mathavan_2009_metrics.py` — trace-derived metric tests.
- `tests/physics_validation/reference_data/mathavan_2009_high_speed/manifest.json`
- `tests/physics_validation/reference_data/mathavan_2009_high_speed/raw_extracted.csv`
- `tests/physics_validation/reference_data/mathavan_2009_high_speed/digitization.csv`
- `tests/physics_validation/reference_data/mathavan_2009_high_speed/normalized.csv`
- `tests/physics_validation/reference_data/mathavan_2009_high_speed/extraction.json`
- `tests/physics_validation/reference_data/mathavan_2009_high_speed/split.json`
- `tests/physics_validation/reference_data/mathavan_2009_high_speed/scenario_template.json`
- `tests/physics_validation/reference_data/mathavan_2009_high_speed/expected_model_mismatches.json`
- `tests/physics_validation/reference_data/mathavan_2009_high_speed/expected_reference_limitations.json`

**Modify:**

- `tools/physics_validation/reference_adapter.py` — register the new adapter without moving synthetic behavior.
- `tools/physics_validation/reference_point.py` — admit `cm/s^2` and preserve bounded-range uncertainty semantics.
- `tools/physics_validation/analyzer.py` — calculate deceleration, post-transition speed, separation angle, and normal cushion rebound from traces.
- `tests/physics_validation/test_reference_point.py` — cover the new unit and bounded-range validation.
- `tests/physics_validation/test_analyzer.py` — cover metric failure classification and event-window selection.
- `docs/reference-data-packages.md` — document the source package, reconstruction command, replay command, and snooker applicability.
- `.github/workflows/ci.yml` — add one representative offline package reconstruction and replay after the foundation fixture.

## Task 1: Lock the Admission Metadata and Raw Evidence

**Files:** Create the Mathavan package metadata/raw files and `tests/physics_validation/test_mathavan_2009_extraction.py`.

1. Write a failing test that loads `manifest.json` and requires dataset ID `mathavan_2009_high_speed`, version `1.0.0`, adapter ID `mathavan_2009_v1`, the DOI, apparatus fields, journal page locators, acquisition URL, local source hash, `redistributable: false`, and extraction/reviewer identities.
2. Require `raw_extracted.csv` to contain exactly two reported-range records, 31 Fig. 9 marker records, five Table I shot records with both measured speeds, and Fig. 12 markers only when their status is `TREND_ONLY`.
3. Require `digitization.csv` rows to carry figure ID, series ID, x/y pixel coordinates, four axis calibration anchors, converted x/y values, and extraction pass ID. Reject a Fig. 9 series unless two independent passes contain the same 31 point IDs.
4. Require the two passes' converted difference to be at most `0.5 mm` in position-equivalent axis error or one source-pixel, whichever is wider; otherwise require the point ID in `expected_reference_limitations.json` and exclude it from normalized data.
5. Run `python3 -m unittest tests.physics_validation.test_mathavan_2009_extraction -v` and confirm failure because the package is absent.
6. Add the exact metadata and raw records. Do not infer individual trajectories from Fig. 6 and do not enter theoretical values as observations.
7. Re-run the test; expect metadata/raw-evidence assertions to pass while reconstruction assertions still fail.

## Task 2: Reconstruct Normalized Data Deterministically

**Files:** Create `tools/physics_validation/extract_mathavan_2009.py`, `normalized.csv`, `extraction.json`, and `split.json`; modify `reference_point.py` and its tests.

1. Add failing tests for `normalize_rows(raw_path, digitization_path, extraction_path)` and `write_normalized(package_path)` with exact byte output.
2. Define explicit interfaces:

```python
def normalize_rows(raw_path: Path, digitization_path: Path,
                   extraction_path: Path) -> tuple[dict, ...]: ...

def write_normalized(package_path: Path) -> bytes: ...
```

3. Convert m/s to cm/s and m/s² to cm/s² using decimal source strings before final serialization. Encode a reported bounded range as midpoint plus half-range with `coverage_factor=1`; record `uncertainty_interpretation="reported_bounded_range"` in `extraction.json` so it is never described as a standard deviation.
4. Extend the unit allowlist with `cm/s^2`; reject negative uncertainty and non-finite conversions exactly as the common reader already does.
5. Pre-register these groups in `split.json`: rolling summary and two middle-speed Table I shots are calibration; sliding summary, all Fig. 9 high/low-speed groups, the remaining three Table I shots, and every extreme cut-angle group are holdout. Keep each Fig. 9 digitization curve group wholly in one partition.
6. Generate `normalized.csv` sorted by `(series_id, group_id, case_id, point_id)`. Require byte-for-byte regeneration and an extraction record containing source SHA-256, raw/digitization input hashes, script version, conversion formulas, rounding precision, and output hash.
7. Run `python3 -m tools.physics_validation.extract_mathavan_2009 --package tests/physics_validation/reference_data/mathavan_2009_high_speed --check`; expect exit 0 and no diff.

## Task 3: Add Trace-Derived Experimental Metrics

**Files:** Modify `analyzer.py` and `test_analyzer.py`; create `test_mathavan_2009_metrics.py`.

1. Write synthetic-frame tests for `rolling_deceleration_cm_s2`, `sliding_deceleration_cm_s2`, `post_collision_linear_velocity_cm_s`, `separation_angle_degrees`, `cushion_rebound_speed_cm_s`, and `cushion_rebound_angle_degrees`.
2. Specify event selection in expectation metadata rather than hidden heuristics:

```json
{
  "event_kind": "ball_ball",
  "sample_phase": "first_pure_roll_after_event",
  "ball_index": 0,
  "minimum_window_ticks": 3
}
```

3. For deceleration, fit speed versus trace time over the declared tick window and return the nonnegative magnitude. For post-collision speed/angle, select the first declared pure-roll sample after the first matching contact. For cushion rebound, select the first sample after `rail_collision`; define angle relative to the cushion tangent and document the sign convention.
4. Return `INTEGRATION_MISMATCH` for missing/jumping ticks or missing declared events, `REFERENCE_LIMITATION` for incomplete selection metadata, `NUMERICAL_FAILURE` for non-finite samples, and `MODEL_MISMATCH` only for a finite value outside the admitted interval.
5. Run `python3 -m unittest tests.physics_validation.test_mathavan_2009_metrics tests.physics_validation.test_analyzer -v`; expect all tests to pass.

## Task 4: Build the Mathavan Adapter

**Files:** Create the adapter/template/tests; modify `reference_adapter.py`.

1. Write failing tests requiring `adapt_mathavan_2009(package, split, points)` to return canonical, byte-identical `ReferenceCase` objects on repeated calls and regardless of input row order.
2. Define the adapter interface:

```python
def adapt_mathavan_2009(package: ReferencePackage,
                        split: ReferenceSplit,
                        points: tuple[ReferencePoint, ...]) -> tuple[ReferenceCase, ...]: ...
```

3. Map Table I incoming speed and cut angle into two equal 52.4 mm balls: cue velocity along +x, object-ball lateral offset `D * sin(cut_angle)`, pure-roll angular velocity, and enough pre-impact distance/ticks to observe collision and post-slip rolling. Preserve the paper's snooker diameter in scenario metadata.
4. Map Fig. 9 points to a single rolling ball approaching a vertical cushion with no sidespin. Map summary deceleration series to one-ball free-motion cases.
5. Do not map Fig. 12 points to expectations. Emit structured limitations for `unmeasured_initial_spin`, `snooker_to_pool_material_conversion_missing`, and any rejected double-digitization point, including affected metrics and concrete release conditions.
6. Add `mathavan_2009_v1` to `default_reference_registry()` and verify the synthetic adapter remains byte-identical.
7. Run `python3 -m unittest tests.physics_validation.test_mathavan_2009_adapter tests.physics_validation.test_reference_adapter -v`; expect all tests to pass.

## Task 5: Reconcile Real Runtime Results Strictly

**Files:** Populate mismatch/limitation manifests; update extraction/adapter tests as needed.

1. Run all admitted cases once through the production binary:

```bash
python3 -m tools.physics_validation.reference_run \
  --executable build/check/Billiards \
  --package tests/physics_validation/reference_data/mathavan_2009_high_speed \
  --output build/physics-reference/mathavan-2009
```

2. Classify every finite out-of-interval result as `MODEL_MISMATCH`. Never widen intervals after seeing simulation output.
3. Enter the exact `(dataset_id, case_id, code, metric)` tuples in `expected_model_mismatches.json`. Enter only evidence/schema limitations in `expected_reference_limitations.json`; never use it for a model failure.
4. Re-run the command and require strict set equality, nonzero physical-failure counts in the report where applicable, complete per-point provenance, committed replay commands, and zero unregistered integration/numerical failures.
5. Replay one calibration and one holdout case with `--case`; compare their scenario JSON and trace hashes to the full-run artifacts.

## Task 6: Wire Offline CI and Documentation

**Files:** Modify `docs/reference-data-packages.md` and `.github/workflows/ci.yml`.

1. Document source facts, exact admitted series/counts, excluded Fig. 12 use, split policy, reconstruction command, full-run command, and single-case replay.
2. Add a PR-fast CI step that runs package validation, `extract_mathavan_2009 --check`, all Mathavan unit tests, and one representative holdout Table I case. Do not fetch any source.
3. Keep the full 31-point package run available as the documented full regression command and artifact path.
4. Run:

```bash
python3 -m unittest discover -s tests/physics_validation -p 'test_*.py' -v
ctest --test-dir build/check --output-on-failure
git diff --exit-code -- tests/physics_validation/reference_data/mathavan_2009_high_speed/normalized.csv
```

Expected: all Python tests and CTests pass; normalized data has no regeneration diff; strict reports contain no unregistered failure.

## Plan Self-Review Checklist

- Every experimentally admitted number is offline, hashed, and linked to journal pp. 790–793, Fig. 6/9, or Table I.
- The 31 Fig. 9 markers have two extraction passes; Table I values are direct transcription; theory columns never become expected output.
- Snooker/Pool applicability and unmeasured spin remain visible limitations.
- Calibration/holdout membership is immutable and extremes stay in holdout.
- All six new metrics have finite, missing-data, and classification tests.
- No production physics formula, parameter, or solver behavior changes.
- A scan for unfinished drafting markers returns no matches.
