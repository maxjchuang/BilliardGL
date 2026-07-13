# Reference data packages

Reference packages turn published or otherwise public experimental measurements into versioned, offline inputs for BilliardGL physics validation. They preserve the numerical evidence needed for later validation and tuning without making CI depend on websites, paper PDFs, or local files.

The `synthetic_reference_v1` package tests infrastructure only. It is not a real-world physics benchmark. The `mathavan_2009_high_speed` package is the first real experimental benchmark; later packages cover Doménech 2023, Mathavan 2010, and Cross 2023.

## Repository policy

- Every code, JSON, CSV, and Markdown file is UTF-8.
- Every numerical value used by validation or later tuning is committed to the repository at full available precision.
- Runtime validation and CI are fully offline.
- Source PDFs, screenshots, or figures are committed only when redistribution is explicitly permitted.
- A package may store independently extracted factual values and transformation metadata when source media cannot be redistributed, subject to the recorded license audit.
- Experimental intervals are fixed before viewing simulation results. Do not widen uncertainty or engineering tolerances to make the current model pass.
- Calibration and holdout membership is committed and versioned. The runner cannot override it.
- Foundation packages and synthetic fixtures do not authorize production physics changes.

## Package layout

Each package is one directory containing:

```text
manifest.json
raw_extracted.csv
normalized.csv
split.json
extraction.json
scenario_template.json
expected_model_mismatches.json
expected_reference_limitations.json
```

Additional committed extraction scripts or redistributable source files may be declared in `manifest.json`. All declared files are hashed. The seven files above are required.

## `manifest.json`

The manifest uses schema version `1` and these top-level fields:

| Field | Meaning |
|---|---|
| `schema_version` | Integer `1`. |
| `dataset_id` | Stable safe ID for the source package. |
| `dataset_version` | Version changed whenever normalized data, provenance, or split changes materially. |
| `adapter_id` | Registered adapter that maps package cases to canonical game scenarios. |
| `source` | Authors, title, year, DOI/public URL, exact locator, and source kind. |
| `acquisition` | Acquisition method/date, license conclusion, and redistribution status. |
| `evidence` | Evidence grade and rationale. |
| `apparatus` | Experimental equipment and WPA Pool applicability. |
| `extraction_review` | Operator, independent reviewer, date, and review method. |
| `files` | Logical file ID, relative path, and SHA-256 for every package dependency. |

Stable IDs match `[A-Za-z0-9][A-Za-z0-9_.-]*`. Paths must be relative, remain inside the package after symlink resolution, exist, and resolve uniquely. Hashes use `sha256:` followed by 64 lowercase hexadecimal characters.

Loading is atomic from the caller's perspective: no package is returned until the complete manifest, path set, file set, hashes, and extraction metadata pass validation.

## `raw_extracted.csv`

This file preserves source values without overwriting them with converted values. Its source-specific columns must retain at least:

- dataset, series, group, case, and point IDs;
- original quantity, value, and unit;
- exact page/table/figure/row locator;
- digitization coordinates and axis calibration when values came from a figure;
- any source-provided uncertainty or confidence information.

Outliers remain present. Exclusion decisions belong in extraction metadata with a documented preregistered rule; they are not silently deleted.

## `normalized.csv`

Schema version 1 has this exact ordered header:

```text
dataset_id,series_id,group_id,case_id,point_id,partition,metric,expected,unit,measurement_uncertainty,digitization_uncertainty,conversion_uncertainty,coverage_factor,engineering_absolute_tolerance,engineering_relative_tolerance,source_locator,pool_applicability
```

| Field | Meaning |
|---|---|
| `dataset_id` | Must equal the manifest dataset ID. |
| `series_id` | Stable experimental series ID. |
| `group_id` | Atomic split group; a group cannot cross partitions. |
| `case_id` | Canonical experimental case mapped by the adapter. |
| `point_id` | Unique point ID within the package. |
| `partition` | `CALIBRATION` or `HOLDOUT`; must agree with `split.json`. |
| `metric` | Trace-derived comparison metric. |
| `expected` | Experimental central value in the declared normalized unit. |
| `unit` | Version-1 unit: `cm`, `cm/s`, `cm/s^2`, `s`, `degree`, `rad/s`, or `dimensionless`. |
| `measurement_uncertainty` | Absolute standard uncertainty from the experiment. |
| `digitization_uncertainty` | Absolute standard uncertainty introduced by graph digitization. |
| `conversion_uncertainty` | Absolute standard uncertainty introduced by apparatus/unit conversion. |
| `coverage_factor` | Multiplier for combined standard uncertainty; blank means `2.0`. |
| `engineering_absolute_tolerance` | Preregistered absolute engineering floor. |
| `engineering_relative_tolerance` | Preregistered relative engineering floor. |
| `source_locator` | Exact page/table/figure/row or synthetic fixture locator. |
| `pool_applicability` | `DIRECT`, `CONVERTED`, `TREND_ONLY`, or `NOT_APPLICABLE`. |

All numeric cells must be finite. Uncertainty components, coverage factors, and engineering tolerances must be nonnegative. Duplicate point IDs are invalid.

Independent uncertainty components combine by root-sum-square:

```text
combined_standard_uncertainty = sqrt(
  measurement_uncertainty²
  + digitization_uncertainty²
  + conversion_uncertainty²
)
```

The committed acceptance half-width is:

```text
max(
  engineering_absolute_tolerance,
  engineering_relative_tolerance * abs(expected),
  coverage_factor * combined_standard_uncertainty
)
```

The interval includes both endpoints. Simulation failure never justifies changing this formula or its inputs after the fact.

## `split.json`

Schema version 1 records:

```json
{
  "schema_version": 1,
  "dataset_id": "dataset_id",
  "dataset_version": "1.0.0",
  "calibration_groups": ["complete_series_a"],
  "holdout_groups": ["complete_series_b"]
}
```

The two arrays must be duplicate-free, disjoint, and exactly cover every normalized `group_id`. A case cannot span groups, and a group cannot span normalized partitions. `normalized.csv` redundantly records the partition so accidental disagreement is detected.

Changing group membership requires a dataset version change and review before simulation results are examined. There is no CLI flag, environment variable, or adapter option for repartitioning.

## `extraction.json`

Schema version 1 records:

- extraction method, tool name/version, date, and operator;
- independent reviewer, review date, and review method;
- input logical file IDs and their exact hashes;
- every unit/digitization transformation ID, formula, input unit, and output unit;
- rounding policy.

When `raw_extracted.csv` changes, update its hash inside `extraction.json` before refreshing manifest hashes. A changed normalized output must remain reproducible from committed raw values and transformations.

Schema version `2` additionally locks the non-redistributed source SHA-256, normalized output SHA-256, extractor module/version, and uncertainty interpretation. This allows a bounded reported range to remain distinguishable from a Gaussian standard deviation.

## Mathavan 2009 high-speed package

`mathavan_2009_high_speed` records the experiment in S. Mathavan, M. R. Jackson, and R. M. Parkin, “Application of high-speed imaging to determine the dynamics of billiards,” DOI `10.1119/1.3157159`.

The committed numerical evidence contains 20 acceptance points: two Fig. 6 deceleration summaries, eight independently distinguishable Fig. 9 cushion markers, and both measured post-slip speeds from each of the five Table I collision shots. The raw inventory also preserves all 31 reported Fig. 9 shot IDs. Twenty-three overlapping markers cannot be assigned independent coordinates and therefore remain a structured reference limitation instead of being reconstructed from the fitted theoretical curve. Fig. 12 is excluded from numerical acceptance because initial spin was not measured.

The apparatus was a Riley Renaissance snooker table with 52.4 mm balls, so every point is `TREND_ONLY` for the current Chinese/WPA Pool runtime. The package separately records the missing material/geometry conversion and unmeasured initial spin. Calibration contains the rolling summary and the two preregistered Table I groups; sliding, all visible Fig. 9 markers, and the remaining Table I shots are holdout. No command-line split override exists.

Reconstruct and verify the committed normalized bytes offline:

```bash
python3 -m tools.physics_validation.extract_mathavan_2009 \
  --package tests/physics_validation/reference_data/mathavan_2009_high_speed \
  --check
python3 -m tools.physics_validation.reference_package \
  tests/physics_validation/reference_data/mathavan_2009_high_speed
```

Run the complete production regression:

```bash
python3 -m tools.physics_validation.reference_run \
  --executable build/check/Billiards \
  --package tests/physics_validation/reference_data/mathavan_2009_high_speed \
  --output build/physics-reference/mathavan-2009
```

Replay one holdout Table I shot without altering its committed partition:

```bash
python3 -m tools.physics_validation.reference_run \
  --executable build/check/Billiards \
  --package tests/physics_validation/reference_data/mathavan_2009_high_speed \
  --output build/physics-reference/mathavan-2009-table1-shot-04 \
  --case table1_shot_04
```

## Doménech 2023 ball-collision package

`domenech_2023_ball_collision` records the experimental marker series in Antonio Doménech-Carbó, “Independent friction-restitution description of billiard ball collisions,” DOI `10.1016/j.mechrescom.2023.104149`. The version-of-record page declares CC BY-NC-ND 4.0 at `https://creativecommons.org/licenses/by-nc-nd/4.0/`. The repository commits extracted factual coordinates and reconstruction metadata, but not the publisher PDF or figure images. Their exact digests are recorded in the manifest.

The package commits all 214 admitted experimental markers and both coordinate readings for every point: 52 billiard `delta2`, 47 billiard `alpha1`, 19 brass `alpha1`, 33 steel immediate `alpha1`, 10 steel post-transition `beta1`, 30 rubber `delta2`, and 23 rubber post-transition `lambda2`. Open markers use white-interior centroid and bounding-box-center passes. Solid markers require agreement between a disk-minus-ring template peak and eroded-black-core centroid. IFR theory curves and fitted restitution/friction coefficients are explicitly excluded from experimental expected values.

The source reports 0.80 ± 0.05 m/s launch speed, a PVC laboratory bench, a camera 75 cm above contact, and a track ending 50 cm before contact. Billiard, brass, steel, and rubber data retain separate IDs and split groups. Low-angle groups are calibration; middle/high groups are holdout. All non-billiard data are `TREND_ONLY`.

The article says data are available on request. No request has been sent because that external communication requires explicit user authorization; this remains a declared limitation. A publisher automation challenge also prevented a complete version-of-record PDF audit, so that audit remains separately declared.

No Doménech runtime scenario is currently emitted. Production fixes ball diameter at 5.715 cm and cannot install the source diameters (6.1, 2.5, and 4.6 cm), masses, contact materials, or PVC support surface per scenario. The full numerical dataset remains available for later schema/physics work, while strict runtime accounting requires one visible geometry/material limitation for each of the seven series. This is deliberately not treated as a model pass, a skipped test, or a relabeled Pool experiment.

Reconstruct, verify, and account for the package offline:

```bash
python3 -m tools.physics_validation.extract_domenech_2023 \
  --package tests/physics_validation/reference_data/domenech_2023_ball_collision \
  --check
python3 -m tools.physics_validation.reference_package \
  tests/physics_validation/reference_data/domenech_2023_ball_collision
python3 -m tools.physics_validation.reference_run \
  --executable build/check/Billiards \
  --package tests/physics_validation/reference_data/domenech_2023_ball_collision \
  --output build/physics-reference/domenech-2023
```

The last command executes no fabricated substitute scenarios; it produces the strict nine-entry limitation audit. A single-case replay command will become valid only after the scenario schema can express the source apparatus.

## Mathavan 2010 cushion package

`mathavan_2010_cushion` records the perpendicular rolling-ball cushion experiment in S. Mathavan, M. R. Jackson, and R. M. Parkin, “A theoretical analysis of billiard ball dynamics under cushion impacts,” DOI `10.1243/09544062JMES1964`. The Loughborough record identifies a submitted-for-publication author version. The audited publisher-layout copy and its extracted figure are not redistributed; their SHA-256 and acquisition/version status are locked in the manifest.

Only the 19 experimental diamonds in Fig. 7(a) are admitted. Fig. 7(b) repeats experimental points beside numerical squares, so those repeated diamonds are provenance-only and the numerical squares are excluded. Figs. 8–10 are entirely numerical algorithm output and cannot become experimental expected values. Each admitted marker has a grayscale-component centroid pass and a component-bounding-box-center pass at the embedded figure's native 1100×530 resolution.

The raw data marks incident speeds below 150 cm/s as the paper's parameter-fit subset and speeds above 250 cm/s as outside the authors' reliable rigid-cushion domain. `incident_low` is calibration; middle, high, and every extreme marker are holdout. All results remain `TREND_ONLY` because the Riley Renaissance snooker cushion, cloth, and 52.5 mm balls are not production Chinese Pool equipment.

Each executable case approaches the rail in perpendicular pure roll with zero sidespin. Three pre-impact and three post-impact telemetry samples are fit locally to evaluate incident and rebound speed at the first rail event. The current production rail response preserves normal speed magnitude, so all 19 finite results are locked as model mismatches rather than being hidden by wider experimental intervals. Reports include incident speed, fit/domain flags, per-band RMSE/maximum error, partition, provenance, and replay command. Four source/equipment limitations remain strictly accounted.

Reconstruct and run the full offline audit:

```bash
python3 -m tools.physics_validation.extract_mathavan_2010 \
  --package tests/physics_validation/reference_data/mathavan_2010_cushion \
  --check
python3 -m tools.physics_validation.reference_run \
  --executable build/check/Billiards \
  --package tests/physics_validation/reference_data/mathavan_2010_cushion \
  --output build/physics-reference/mathavan-2010
```

Replay the highest-speed extreme holdout:

```bash
python3 -m tools.physics_validation.reference_run \
  --executable build/check/Billiards \
  --package tests/physics_validation/reference_data/mathavan_2010_cushion \
  --output build/physics-reference/mathavan-2010-replay \
  --case fig7_experimental_19
```

## `scenario_template.json` and adapters

The template contains canonical production-runtime inputs plus explicit per-case mappings. An adapter:

- reads only committed package files;
- performs no network access and no physics simulation;
- never estimates missing apparatus or experimental inputs;
- emits stable scenario IDs as `<dataset_id>__<case_id>`;
- emits canonical UTF-8 JSON with deterministic key ordering;
- preserves point intervals, units, source locators, package hashes, and committed partition;
- reports a structured package error when a mapping cannot be expressed.

Production execution continues through the existing automation process and `GameRuntime`; reference validation does not implement a second physics engine.

## Expected failure manifests

`expected_model_mismatches.json` contains only `MODEL_MISMATCH` entries:

```json
{
  "schema_version": 1,
  "failures": [{
    "dataset_id": "dataset_id",
    "case_id": "case_id",
    "code": "MODEL_MISMATCH",
    "metric": "metric_id",
    "rationale": "Why the current model is expected to miss this measurement."
  }]
}
```

`expected_reference_limitations.json` contains only `REFERENCE_LIMITATION` entries:

```json
{
  "schema_version": 1,
  "failures": [{
    "dataset_id": "dataset_id",
    "case_id": "case_id",
    "code": "REFERENCE_LIMITATION",
    "metric": "metric_id",
    "missing_evidence": "Evidence or condition that is unavailable.",
    "resolution_condition": "Evidence required to remove the limitation."
  }]
}
```

The actual and expected tuple sets must match exactly. New failures and unexpected disappearance both fail CI. Known mismatches and limitations remain visibly failed in reports; they are never shown as passed or skipped.

`NUMERICAL_FAILURE`, `INTEGRATION_MISMATCH`, and `NON_DETERMINISTIC` cannot be allowlisted in either manifest.

## Failure taxonomy

| Code | Meaning |
|---|---|
| `MODEL_MISMATCH` | Expressible, stable simulation output lies outside the committed experimental interval or the physical response is not modeled. |
| `REFERENCE_LIMITATION` | Evidence, apparatus conversion, or scenario input is insufficient to make the comparison. |
| `INTEGRATION_MISMATCH` | Process execution, capabilities, trace continuity, schema mapping, or artifact integration failed. |
| `NUMERICAL_FAILURE` | Nonfinite state, energy pathology, missed collision, or other numerical invariant failed. |
| `NON_DETERMINISTIC` | Identical fresh process runs produced different traces. |

## Reports and replay

Each run produces:

- `reference_report.json`: authoritative complete numerical/provenance report;
- `reference_points.csv`: full-precision point export with spreadsheet formula protection for textual cells;
- `reference_report.md`: human-readable partition and accounting summary;
- `traces/<scenario_id>.json`: complete production trace when serializable;
- `provenance/<scenario_id>.json`: dataset, adapter, point, source, and package hashes.

Calibration and holdout are separate report sections. Each point includes prediction, experimental value, signed error, interval, all uncertainty components, status, dataset version, source locator, trace path, build SHA-256, and replay command. Nonfinite predictions are represented by JSON `null` plus `prediction_nonfinite` (`NaN`, `+Infinity`, or `-Infinity`) so reports remain strict JSON without losing the failure state.

Verify a package:

```bash
python3 -m tools.physics_validation.reference_package \
  tests/physics_validation/fixtures/reference_package_v1
```

Refresh manifest hashes after an audited edit:

```bash
python3 -m tools.physics_validation.reference_package --update-hashes \
  tests/physics_validation/fixtures/reference_package_v1
```

Run all cases offline:

```bash
python3 -m tools.physics_validation.reference_run \
  --executable build/Billiards \
  --package tests/physics_validation/fixtures/reference_package_v1 \
  --output build/physics-reference-validation
```

Replay one committed case without changing its partition:

```bash
python3 -m tools.physics_validation.reference_run \
  --executable build/Billiards \
  --package tests/physics_validation/fixtures/reference_package_v1 \
  --output build/physics-reference-replay \
  --case free_roll_holdout
```

## Data-admission checklist

Before a real source package receives numerical points, record and review:

- [ ] Exact pages, tables, figures, supplementary files, series, and point counts.
- [ ] Public acquisition location and license/redistribution conclusion.
- [ ] Original apparatus, dimensions, mass, materials, environment, and sampling method.
- [ ] Original values/units and full committed raw numerical data.
- [ ] Digitization calibration, residuals, and repeat extraction comparison where applicable.
- [ ] Unit/apparatus conversion formulas and propagated uncertainty.
- [ ] Independent extraction review and disagreement threshold.
- [ ] Preregistered complete-series calibration/holdout groups, with extremes preferentially held out.
- [ ] WPA Pool applicability (`DIRECT`, `CONVERTED`, `TREND_ONLY`, or `NOT_APPLICABLE`) and rationale.
- [ ] Every unavailable condition entered as a limitation with a concrete resolution condition.
- [ ] Raw-to-normalized reconstruction is byte-reproducible.
- [ ] Package verification, unit tests, and production-process reference E2E pass offline.

Do not add unaudited paper-derived values to the synthetic foundation package.
