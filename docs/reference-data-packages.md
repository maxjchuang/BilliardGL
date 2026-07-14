# Reference data packages

Reference packages turn published or otherwise public experimental measurements into versioned, offline inputs for BilliardGL physics validation. They preserve the numerical evidence needed for later validation and tuning without making CI depend on websites, paper PDFs, or local files.

The `synthetic_reference_v1` package tests infrastructure only. It is not a real-world physics benchmark. The `mathavan_2009_high_speed` package is the first real experimental benchmark; later packages cover Doménech 2023, Mathavan 2010, and Cross 2023.

The `cue_contact_analytic_contract` package is a grade-C equation contract generated independently with Python `Decimal`. It checks rigid-impulse invariants and reproducibility only: it is not a paper-derived dataset, equipment measurement, or real-world validation claim. Its complete precision inputs, normalized values, calibration/holdout split, equations, and hashes are committed. The generator does not import or execute the production contact implementation.

Regenerate or byte-check the package offline, then expose only its committed calibration cases:

```bash
python3 -m tools.physics_validation.generate_cue_contact_analytic \
  --package tests/physics_validation/reference_data/cue_contact_analytic_contract \
  --check
python3 -m tools.physics_validation.calibration_run \
  --executable build/Billiards \
  --package tests/physics_validation/reference_data/cue_contact_analytic_contract \
  --output build/candidates/cue_contact_v1/calibration
```

Calibration covers center impact, mirrored vertical offsets, and the preregistered inner stick boundary. HOLDOUT contains left/right sidespin mirrors, horizontal slip-cone clamping, and the traceable zero-impulse miscue outcome. Nonzero cue elevation and a vertical slip impulse remain explicit 2.5D hard-constraint tests rather than analytic reference cases.

### Cue-contact candidate v1 result

The frozen `cue_contact_v1` formula converts the versioned UI power scale into physical cue speed, then resolves one SI-unit rigid impulse at the declared contact arm. It classifies stick versus Coulomb-limited slip, produces full 3D angular velocity, records zero-impulse miscues, and atomically rejects results requiring vertical ball translation in the 2.5D runtime. The profile retains cue mass `0.5 kg`, normal restitution `0`, chalked/unchalked friction `0.6/0.1`, maximum reliable offset fraction `0.8`, and compatibility mapping `1.34 cm/s` per power unit. Their exact evidence classifications are in `physics_models/profiles/chinese_pool_cue_contact_v1.json`.

Before freeze, all 19 CALIBRATION points passed and no HOLDOUT rows were exposed. The single post-freeze HOLDOUT execution then passed all 16 points: left/right sidespin mirrors, horizontal slip impulse/energy/speed, and the zero-impulse miscue. The immutable calibration, freeze, validation report, full-precision CSV, Markdown report, and receipt are under `physics_models/candidates/cue_contact_v1/`; the receipt result is `PASSED_OR_ACCOUNTED` with no known or new failures.

This result establishes analytic consistency only. Cross 2023 still has no admitted numerical points, so real-world cue-impact accuracy remains unvalidated. The candidate profile explicitly records `analytic_contract_passed=true`, `experimental_validation_blocked=true`, and `experimental_validation=false` for the UI power mapping. Replaying HOLDOUT requires a new governed validation event; do not overwrite the committed first receipt.

Verify the frozen local artifacts without exposing HOLDOUT:

```bash
python3 -m tools.physics_validation.freeze_candidate \
  --verify physics_models/candidates/cue_contact_v1/freeze.json \
  --profile physics_models/profiles/chinese_pool_cue_contact_v1.json \
  --executable build/Billiards \
  --calibration-report physics_models/candidates/cue_contact_v1/calibration/reference_report.json \
  --dataset-manifest tests/physics_validation/reference_data/cue_contact_analytic_contract/manifest.json
```

Only after authorizing a new governed validation event, use `validation_run` with that freeze, profile, executable, package, and a new output directory. The committed first receipt under `physics_models/candidates/cue_contact_v1/validation/` is immutable.

## Repository policy

- Every code, JSON, CSV, and Markdown file is UTF-8.
- Every numerical value used by validation or later tuning is committed to the repository at full available precision.
- Runtime validation and CI are fully offline.
- Source PDFs, screenshots, or figures are committed only when redistribution is explicitly permitted.
- A package may store independently extracted factual values and transformation metadata when source media cannot be redistributed, subject to the recorded license audit.
- Experimental intervals are fixed before viewing simulation results. Do not widen uncertainty or engineering tolerances to make the current model pass.
- Calibration and holdout membership is committed and versioned. The runner cannot override it.
- Foundation packages and synthetic fixtures do not authorize production physics changes.

## Calibrated candidate lifecycle

Candidate tuning uses five explicit states: calibration, freeze, validation, receipt, and—if validation results influence another model choice—spent. Committed values provide process isolation rather than secrecy: all full-precision numerical data remain reviewable in Git, while separate commands and committed partitions prevent validation values from entering calibration accidentally.

Run only the package's committed calibration cases. This command exposes no case, partition, holdout, or split override:

```bash
python3 -m tools.physics_validation.calibration_run \
  --executable build/Billiards \
  --package tests/physics_validation/reference_data/mathavan_2009_high_speed \
  --output build/candidates/surface_motion_v1/calibration
```

Copy the complete calibration report into the candidate directory as `calibration/reference_report.json`. The profile manifest must be canonical UTF-8 JSON with top-level `runtime_profile`, `parameter_sources`, and `applicability` objects. Every numeric runtime leaf needs a unit plus a nonempty evidence or limitation statement. Freeze the exact source revision, profile bytes, executable bytes, calibration report bytes, data package hashes, and calibration metric intervals before any holdout execution:

```bash
python3 -m tools.physics_validation.freeze_candidate \
  --candidate-id chinese_pool_surface_motion_v1 \
  --formula-version surface_motion_v1 \
  --source-revision "$(git rev-parse HEAD)" \
  --profile physics_models/profiles/chinese_pool_surface_motion_v1.json \
  --executable build/Billiards \
  --calibration-report physics_models/candidates/surface_motion_v1/calibration/reference_report.json \
  --dataset-manifest tests/physics_validation/reference_data/mathavan_2009_high_speed/manifest.json \
  --created-at 2026-07-14T00:00:00Z \
  --output physics_models/candidates/surface_motion_v1/freeze.json
```

The freeze is deterministic for identical inputs. Verify its three local artifacts before validation; package hashes are then verified by the validation command itself:

```bash
python3 -m tools.physics_validation.freeze_candidate \
  --verify physics_models/candidates/surface_motion_v1/freeze.json \
  --profile physics_models/profiles/chinese_pool_surface_motion_v1.json \
  --executable build/Billiards \
  --calibration-report physics_models/candidates/surface_motion_v1/calibration/reference_report.json
```

`tests/physics_validation/validation_data_status.json` is the reviewed lifecycle registry. Its only legal values are `calibration`, `validation`, `spent`, and `confirmation`. Candidate validation requires the selected package's holdout status to be `validation`, derives the committed HOLDOUT cases internally, and exposes no parameter or split override:

```bash
python3 -m tools.physics_validation.validation_run \
  --freeze physics_models/candidates/surface_motion_v1/freeze.json \
  --executable build/Billiards \
  --package tests/physics_validation/reference_data/mathavan_2009_high_speed \
  --profile physics_models/profiles/chinese_pool_surface_motion_v1.json \
  --output physics_models/candidates/surface_motion_v1/validation
```

Validation writes the unchanged reference reports plus canonical `validation_receipt.json`. The receipt binds the candidate and freeze hashes, dataset/version, HOLDOUT partition, report hash, and accounted result; it never edits the model or lifecycle registry. If anyone later uses those holdout results to tune or choose a successor candidate, changing that dataset's holdout status to `spent` is a separate reviewed commit. `confirmation` is reserved for data admitted in advance solely for final confirmation of an already selected model.

### Surface-motion candidate v1 result

The frozen `surface_motion_v1` formula uses rolling resistance `12.5 cm/s²`, a preregistered sliding-friction coefficient of `0.20`, and zero torsional-spin decay because no admitted sidespin-decay experiment is available. The complete parameter provenance is in `physics_models/profiles/chinese_pool_surface_motion_v1.json`; the calibration report and freeze are in `physics_models/candidates/surface_motion_v1/`.

The calibration partition executed five points: rolling deceleration and both Table I shot 02 velocities passed, while both Table I shot 01 velocities remained one known theme-3 collision mismatch. The previously blocked post-collision pure-roll transition is now executable; unresolved Fig. 9 markers, snooker-to-pool equipment conversion, and unmeasured initial spin remain explicit evidence limitations.

The one-time frozen HOLDOUT execution produced 15 complete point rows: 12 passed and three remained known model mismatches. Fig. 9 cases 07 and 08 retain cushion-response mismatches. Sliding deceleration predicted `29.274413626277244 cm/s²` against the experimental `207.5 cm/s²`, so candidate v1 failed the sliding experimental validation and must not be described as a fully validated surface model. No parameter, interval, formula branch, or sampling window was changed after this result.

The immutable first validation report and receipt are stored under `physics_models/candidates/surface_motion_v1/validation/`. Its receipt says `FAILED` because the first-run accounting still contained obsolete expected mismatches for Fig. 9 cases 05 and 06; both points actually passed and those two stale entries were removed only after preserving the report. There were no new mismatches, integration failures, numerical failures, or nondeterministic results. The calibration and validation commands above are the canonical replay commands; HOLDOUT replay requires a new governed validation event rather than overwriting the committed first receipt.

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
| `unit` | Version-1 unit: `cm`, `cm/s`, `cm/s^2`, `s`, `degree`, `rad/s`, `N*s`, `J`, or `dimensionless`. |
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

Digitized packages reconstruct source-axis values from committed pixel coordinates and explicit axis calibrations. Stored converted columns are audited duplicates: a mismatch fails reconstruction rather than becoming a new numeric truth source.

## Mathavan 2009 high-speed package

`mathavan_2009_high_speed` records the experiment in S. Mathavan, M. R. Jackson, and R. M. Parkin, “Application of high-speed imaging to determine the dynamics of billiards,” DOI `10.1119/1.3157159`.

The committed numerical evidence contains 20 acceptance points: two Fig. 6 deceleration summaries, eight independently distinguishable Fig. 9 cushion markers, and both measured post-slip speeds from each of the five Table I collision shots. The raw inventory also preserves all 31 reported Fig. 9 shot IDs. Twenty-three overlapping markers cannot be assigned independent coordinates and therefore remain a structured reference limitation instead of being reconstructed from the fitted theoretical curve. Fig. 12 is excluded from numerical acceptance because initial spin was not measured.

The apparatus was a Riley Renaissance snooker table with 52.4 mm balls, so every point is `TREND_ONLY` for the current Chinese/WPA Pool runtime. The package separately records the missing material/geometry conversion and unmeasured initial spin. Calibration contains the rolling summary and the two preregistered Table I groups; sliding, all visible Fig. 9 markers, and the remaining Table I shots are holdout. No command-line split override exists. The ten Table I points remain present in reports as point-level limitations: production currently provides no post-collision sliding-to-pure-rolling transition, so sampling an immediate post-impact frame would use the wrong experimental phase.

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

The article says data are available on request. No author request has been sent; the current authorization covers institutional-access attempts but not contacting an author. This remains a declared limitation. A publisher automation challenge also prevented a complete version-of-record PDF audit, so that audit remains separately declared.

Scenario v5 now installs the source diameter, mass, inertia factor, fitted ball-contact restitution/friction, material ID, and PVC support hypothesis without changing the production Chinese Pool profile. Every one of the 214 points emits an executable case from its committed raw `impact_angle_degrees`: the cue sphere starts at `80 cm/s` in pure roll, the object sphere starts at rest, and their center separation is one source diameter minus `10^-6 cm`. Immediate and first-stable-pure-roll phases remain distinct. Non-billiard cases stay `TREND_ONLY`; billiard cases remain `CONVERTED`, not direct Pool validation.

`physics_models/calibration/ball_collision_material_fit_v1.json` preserves the full deterministic calibration-only fit. The bounded two-level search uses `0 <= e, mu <= 1`, an unweighted mean squared angular residual, and stable `(objective,e,mu)` tie-breaking. Its fitted `(e, mu)` values are billiard `(0.36, 0.25)`, brass `(0.71, 0.10)`, rubber `(0.00, 0.33)`, and steel `(0.97, 0.04)`. Mutation tests prove that changing every HOLDOUT expected value cannot change these parameters or any CALIBRATION scenario byte. The PVC sliding coefficient is an explicit unmeasured apparatus hypothesis of `0.002`; post-transition cases use 400 ticks so the low-friction transition is observed without a discrete repeat impulse.

The governed CALIBRATION execution contains 75 complete numeric rows. All 75 are finite, deterministic, and pass the approach/separation, friction-cone, energy, and no-repeat integration gates, but all 75 remain known experimental `MODEL_MISMATCH` entries at the committed digitization intervals. This is evidence that one constant restitution/friction pair per material is not sufficient to reproduce the admitted curves; it is not converted into a limitation or a pass. No HOLDOUT scenario has been executed at this stage. Only the author-data request and version-of-record PDF audit remain reference limitations.

Reconstruct, verify, and account for the package offline:

```bash
python3 -m tools.physics_validation.extract_domenech_2023 \
  --package tests/physics_validation/reference_data/domenech_2023_ball_collision \
  --check
python3 -m tools.physics_validation.reference_package \
  tests/physics_validation/reference_data/domenech_2023_ball_collision
python3 -m tools.physics_validation.fit_ball_collision \
  --package tests/physics_validation/reference_data/domenech_2023_ball_collision \
  --output physics_models/calibration/ball_collision_material_fit_v1.json
python3 -m tools.physics_validation.calibration_run \
  --executable build/check/Billiards \
  --package tests/physics_validation/reference_data/domenech_2023_ball_collision \
  --output build/physics-reference/domenech-2023-calibration
```

The last command selects only the committed CALIBRATION groups. Candidate HOLDOUT execution is separately governed and must not be substituted with `reference_run` while the candidate is still being calibrated and frozen.

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

## Cross 2023 cue-impact admission package

`cross_2023_cue_impact` identifies Rod Cross, “Impact of a cue with a billiard ball,” DOI `10.1177/17543371231184011`, first online 29 June 2023 and published in volume 239(4), pages 647–651 in December 2025.

As re-audited on 14 July 2026, SAGE marks the article restricted, redirects the PDF endpoint to the abstract, and offers institutional authentication or paid access; the scholarly indexes still expose no repository full text. Therefore the package deliberately contains zero normalized numerical points. It commits the complete access audit, exact empty numeric dataset, scenario-v2 template, and three strict limitations. Production now records cue contact relative velocity, impulse, friction cone, energy, and stick/slip/miscue regime, so the former telemetry limitation is resolved. Values from the abstract, snippets, or Cross 2008 remain prohibited substitutes. This is an admission-gated package, not a claim that the Cross experimental dataset has been completed.

Offline verification:

```bash
python3 -m tools.physics_validation.reference_package \
  tests/physics_validation/reference_data/cross_2023_cue_impact
python3 -m tools.physics_validation.extract_cross_2023 \
  --package tests/physics_validation/reference_data/cross_2023_cue_impact --check
python3 -m tools.physics_validation.reference_run \
  --executable build/check/Billiards \
  --package tests/physics_validation/reference_data/cross_2023_cue_impact \
  --output build/physics-reference/cross-2023
```

The remaining blockers can only be removed after a lawful full text is hashed and reviewed, every experimental marker/table is separated from model output and committed with uncertainties/splits, and the compatibility UI-power scale is replaced or supported by an independently validated physical cue-speed mapping. The source PDF itself must not be redistributed without permission.

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

The cue-impact v2 `direction` is a unit heading in the table plane. `elevation_degrees` is the sole vertical-angle field, so contradictory direction/elevation encodings are rejected.

The manual and weekly `.github/workflows/physics-reference-full.yml` workflow runs every calibration and holdout point without a case filter and uploads the complete numeric reports. Pull-request CI retains representative-case runs for latency.

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
