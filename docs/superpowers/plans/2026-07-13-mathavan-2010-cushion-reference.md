# Mathavan 2010 Cushion Reference Implementation Plan

> **Required subskill:** Use `superpowers:test-driven-development` for every implementation task and `superpowers:verification-before-completion` before committing the implementation.

**Goal:** Establish an offline, source-faithful cushion-impact package from Mathavan, Jackson, and Parkin 2010, admitting the paper's high-speed-camera rebound measurements while explicitly excluding its numerical prediction plots from real-world ground truth.

**Architecture:** Add a cushion-specific extractor and adapter on top of the common reference framework. Digitize the experimental markers in Fig. 7, preserve the numerical curves in an excluded-evidence inventory, and drive vertical rolling-ball rail impacts through the production runtime. Unmeasured oblique-angle/spin outcomes remain strict source limitations.

**Tech Stack:** Python 3 standard library, existing reference-package/adapters/analyzer modules, canonical physics scenario v1, production automation runtime, `unittest`, CTest.

## Global Constraints

- Implement after the Mathavan 2009 plan; reuse its cushion event selection, units, and snooker apparatus metadata where identical.
- Do not implement the paper's differential-equation model in the reference layer and do not tune the game toward `e_e=0.98`, `μ_w=0.14`, or any plotted curve.
- Fig. 7 experimental markers are the only new numerical ground truth in this source. Figs. 8–10 are outputs of the paper's numerical algorithm and must never enter `normalized.csv` as expected observations.
- Keep the 2009 experimental source and 2010 reuse distinct. A value cited from Mathavan 2009 must retain the 2009 dataset ID rather than being duplicated as a 2010 experiment.
- Commit all admitted numeric markers, pixel/axis coordinates, conversion/error records, split assignments, and hashes. CI remains offline.
- Treat Riley Renaissance snooker cushion/ball/cloth as non-WPA material. The nearly matching `h=7R/5` contact geometry does not establish material equivalence.
- Only impacts with experimental incident speed below `1.5 m/s` participate in the paper's parameter-estimation subset; markers above that range remain measured rebound observations but cannot be labeled part of that fit. Speeds above `2.5 m/s` are extreme holdout and carry the rigid-cushion limitation described by the authors.
- Freeze grouped calibration/holdout assignments before the first game run.

## Source Admission Record

- Citation: S. Mathavan, M. R. Jackson, R. M. Parkin, “A theoretical analysis of billiard ball dynamics under cushion impacts,” *Proceedings of the Institution of Mechanical Engineers, Part C* 224(9), 1863–1873 (2010), DOI `10.1243/09544062JMES1964`.
- Public author manuscript metadata: Loughborough institutional repository record `https://dspace.lboro.ac.uk/2134/15087`; record the acquired manuscript SHA-256 and publisher-version DOI.
- Experimental source, journal pp. 1869–1870, Fig. 7: Riley Renaissance snooker table, stationary high-speed camera, no sidespin, perpendicular incidence (`α=90°`), pure rolling (`ωT0=V0/R`), rebound speed versus incident speed.
- Parameter-search subset: experimental incident speeds below `1.5 m/s`; the authors report minimum RMS at `e_e=0.98`, `μ_w=0.14`.
- Applicability boundary: numerical and experimental results deviate above `2.5 m/s`; the authors identify this as a likely rigid-cushion assumption limit.
- Theory-only evidence, journal pp. 1870–1872, Figs. 8–10: rebound speed/angle curves for incident angle, topspin, and sidespin are numerical algorithm output. They are excluded from acceptance data.

## File Structure

**Create:**

- `tools/physics_validation/adapters/mathavan_2010.py`
- `tools/physics_validation/extract_mathavan_2010.py`
- `tests/physics_validation/test_mathavan_2010_extraction.py`
- `tests/physics_validation/test_mathavan_2010_adapter.py`
- `tests/physics_validation/test_mathavan_2010_metrics.py`
- `tests/physics_validation/reference_data/mathavan_2010_cushion/manifest.json`
- `tests/physics_validation/reference_data/mathavan_2010_cushion/raw_extracted.csv`
- `tests/physics_validation/reference_data/mathavan_2010_cushion/digitization.csv`
- `tests/physics_validation/reference_data/mathavan_2010_cushion/normalized.csv`
- `tests/physics_validation/reference_data/mathavan_2010_cushion/extraction.json`
- `tests/physics_validation/reference_data/mathavan_2010_cushion/split.json`
- `tests/physics_validation/reference_data/mathavan_2010_cushion/scenario_template.json`
- `tests/physics_validation/reference_data/mathavan_2010_cushion/expected_model_mismatches.json`
- `tests/physics_validation/reference_data/mathavan_2010_cushion/expected_reference_limitations.json`

**Modify:**

- `tools/physics_validation/reference_adapter.py` — register `mathavan_2010_v1`.
- `tools/physics_validation/analyzer.py` — add pre-impact/rebound event-window pairing if not already available.
- `tests/physics_validation/test_analyzer.py`
- `docs/reference-data-packages.md`
- `.github/workflows/ci.yml`

## Task 1: Prove Which Marks Are Experimental

**Files:** Create manifest/raw/digitization/limitation files and extraction tests.

1. Write a failing manifest test for dataset `mathavan_2010_cushion`, version `1.0.0`, adapter `mathavan_2010_v1`, citation, repository URL, source SHA-256, manuscript/version status, apparatus, and redistribution conclusion.
2. Require an evidence inventory with Fig. 7 panels labeled `EXPERIMENT_PLUS_THEORY` and Figs. 8–10 labeled `THEORY_ONLY`. Require each legend entry/marker/line to state whether it is admitted, provenance-only, or excluded.
3. Inspect Fig. 7 at native resolution, count its experimental markers, and store that immutable count in the manifest. The tests must reject any raw/digitized count mismatch.
4. Digitize only Fig. 7 experimental markers twice. Store x/y pixels, four axis anchors, derived incident/rebound speeds, extraction pass, marker identity, and per-axis residual.
5. Record the plotted numerical line separately as `excluded_model_output`; use it only to verify marker/line discrimination. Do not serialize line samples to normalized data.
6. Register `oblique_experimental_rebound_angle_unavailable`, `experimental_spin_change_unavailable`, and `snooker_cushion_to_pool_material_conversion_missing`, each with affected metric and concrete release evidence.
7. Run `python3 -m unittest tests.physics_validation.test_mathavan_2010_extraction -v`; expect evidence admission to pass and normalization to remain failing.

## Task 2: Rebuild the Fig. 7 Dataset and Split

**Files:** Create extractor, normalized/extraction/split files; extend tests.

1. Add failing exact-byte tests for:

```python
def normalize_rows(raw_path: Path, digitization_path: Path,
                   extraction_path: Path) -> tuple[dict, ...]: ...

def write_normalized(package_path: Path) -> bytes: ...
```

2. Convert both axes from m/s to cm/s. Combine camera/axis/digitization error without importing RMS error from the paper's fitted numerical model.
3. Tag each marker with `fit_subset` (`incident < 150 cm/s`), `rigid_cushion_domain` (`incident <= 250 cm/s`), and `pool_applicability=TREND_ONLY`.
4. Group adjacent incident-speed bands before simulator output is viewed. Put one representative low/mid band in calibration; put the remaining bands and every band containing `>250 cm/s` in holdout. Never split adjacent markers from the same predeclared band.
5. Store conversion formulas, axis anchors, extraction pass hashes, output rounding, and source hash in `extraction.json`; require byte-identical regeneration.
6. Run:

```bash
python3 -m tools.physics_validation.extract_mathavan_2010 \
  --package tests/physics_validation/reference_data/mathavan_2010_cushion --check
```

Expected: exit 0, exact marker count, and no normalized diff.

## Task 3: Measure Incident and Immediate Rebound Speed Reliably

**Files:** Modify analyzer/tests and create `test_mathavan_2010_metrics.py`.

1. Add synthetic traces with multiple pre-impact and post-impact frames and a single `rail_collision` contact.
2. Define a paired metric expectation:

```json
{
  "observed_metric": "cushion_rebound_speed_cm_s",
  "ball_index": 0,
  "event_kind": "rail_collision",
  "incident_window_ticks": 3,
  "rebound_window_ticks": 3,
  "sample_phase": "immediate_post_impact"
}
```

3. Fit speed locally on each side of the first rail event and evaluate the immediate pre/post values at event time. Reject a second rail event inside either fitting window.
4. Validate perpendicular incidence and negligible sidespin from scenario initial conditions, not from the observed rebound value.
5. Classify missing/ambiguous rail events as `INTEGRATION_MISMATCH`, missing source phase as `REFERENCE_LIMITATION`, non-finite fit as `NUMERICAL_FAILURE`, and finite interval failure as `MODEL_MISMATCH`.
6. Run `python3 -m unittest tests.physics_validation.test_mathavan_2010_metrics tests.physics_validation.test_analyzer -v`; expect all tests to pass.

## Task 4: Build a Source-Faithful Cushion Adapter

**Files:** Create adapter/template/tests; modify registry.

1. Write failing tests for deterministic row-order-independent adaptation and exact partition propagation.
2. Implement:

```python
def adapt_mathavan_2010(package: ReferencePackage,
                        split: ReferenceSplit,
                        points: tuple[ReferencePoint, ...]) -> tuple[ReferenceCase, ...]: ...
```

3. Create one-ball, perpendicular-to-rail scenarios using each experimental incident speed, pure-roll angular velocity from the scenario ball radius, zero sidespin, and enough approach/rebound ticks for both fit windows.
4. Preserve source ball/cushion dimensions and `fit_subset`/`rigid_cushion_domain` flags in provenance. Do not install `e_e` or `μ_w` into the runtime.
5. Emit no scenarios for Figs. 8–10. Confirm tests fail if any theory-only evidence ID reaches an expectation.
6. Register `mathavan_2010_v1` and prove all earlier adapters remain byte-identical.

## Task 5: Run Production Physics and Lock Accounting

**Files:** Populate strict manifests and verify reporting.

1. Run:

```bash
python3 -m tools.physics_validation.reference_run \
  --executable build/check/Billiards \
  --package tests/physics_validation/reference_data/mathavan_2010_cushion \
  --output build/physics-reference/mathavan-2010
```

2. Record exact finite rebound-speed failures in `expected_model_mismatches.json`; never adjust extracted intervals after this run.
3. Require limitation accounting for theory-only angle/spin coverage, snooker material conversion, and the authors' `>250 cm/s` model-domain warning. The last remains visible even though measured markers may still run.
4. Require per-speed-band RMSE, maximum error, partition, domain flag, provenance, and replay command in all report formats.
5. Replay the highest-speed holdout case; verify scenario and trace hashes match the full run.

## Task 6: Document and Verify Offline

**Files:** Modify docs and CI.

1. Document why Fig. 7 markers are admitted and Figs. 8–10 are excluded, plus exact marker count, extraction, split, full-run, and replay commands.
2. Add package hash/reconstruction/unit tests and one sub-`150 cm/s` representative case to PR-fast CI. Keep the highest-speed case in full manual/scheduled regression.
3. Run:

```bash
python3 -m unittest discover -s tests/physics_validation -p 'test_*.py' -v
ctest --test-dir build/check --output-on-failure
git diff --exit-code -- tests/physics_validation/reference_data/mathavan_2010_cushion/normalized.csv
```

Expected: all tests pass, normalized data reproduces, every limitation/mismatch is strictly reconciled, and no theory curve is present as an expected observation.

## Plan Self-Review Checklist

- The evidence inventory prevents a theoretical curve from masquerading as experiment.
- Fig. 7 marker count and two extraction passes are locked before simulator execution.
- Fit subset and rigid-cushion domain are metadata, not runtime parameters.
- Oblique rebound angle and spin-change gaps remain explicit limitations.
- Snooker equipment is never labeled direct WPA Pool evidence.
- Every runtime number is committed; no production physics change is planned.
- A scan for unfinished drafting markers returns no matches.
