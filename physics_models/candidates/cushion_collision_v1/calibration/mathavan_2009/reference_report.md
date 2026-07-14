# Reference Physics Validation Report

Build: sha256:4dfaf475b85c6959e0e387afc8af58a6deb1e3d19cff98cd286bc0867f99cdef

## CALIBRATION

| Point | Case | Incident | Domain | Metric | Prediction | Experiment | Interval | Status |
|---|---|---:|---|---|---:|---:|---|---|
| rolling_deceleration_range | rolling_deceleration | - | - | rolling_deceleration_cm_s2 | 12.499999813735489 | 12.5 | [12.4, 12.6] | PASSED |
| table1_shot_01_cue_speed | table1_shot_01 | - | - | post_collision_linear_velocity_cm_s | 92.20242309570312 | 81.6 | [75.23603899999999, 87.963961] | MODEL_MISMATCH_KNOWN |
| table1_shot_01_object_speed | table1_shot_01 | - | - | post_collision_linear_velocity_cm_s | 49.44283676147461 | 83.6 | [77.23603899999999, 89.963961] | MODEL_MISMATCH_KNOWN |
| table1_shot_02_cue_speed | table1_shot_02 | - | - | post_collision_linear_velocity_cm_s | 54.71088790893555 | 52.0 | [45.636039, 58.363961] | PASSED |
| table1_shot_02_object_speed | table1_shot_02 | - | - | post_collision_linear_velocity_cm_s | 34.58061981201172 | 62.9 | [56.536039, 69.263961] | MODEL_MISMATCH_KNOWN |

### Group error summary

| Group | Points | RMSE | Maximum absolute error | Pass rate |
|---|---:|---:|---:|---:|
| rolling_summary | 1 | 1.8626451137038202e-07 | 1.8626451137038202e-07 | 1.0 |
| table1_mid | 4 | 22.84983251300874 | 34.157163238525385 | 0.25 |

## HOLDOUT

| Point | Case | Incident | Domain | Metric | Prediction | Experiment | Interval | Status |
|---|---|---:|---|---|---:|---:|---|---|
| - | - | - | - | - | - | - | - | - |

### Group error summary

| Group | Points | RMSE | Maximum absolute error | Pass rate |
|---|---:|---:|---:|---:|
| - | 0 | - | - | - |

## Failure accounting

- Known model mismatches: mathavan_2009_high_speed:table1_shot_01:MODEL_MISMATCH:post_collision_linear_velocity_cm_s, mathavan_2009_high_speed:table1_shot_02:MODEL_MISMATCH:post_collision_linear_velocity_cm_s
- New model mismatches: none
- Missing model mismatches: none
- Known reference limitations: none
- New reference limitations: none
- Missing reference limitations: none
- Unallowlistable failures: none

## Reference limitation details

### mathavan_2009_high_speed:fig9_unresolved_markers

- Metric: cushion_rebound_speed_cm_s
- Missing evidence: Twenty-three reported Fig. 9 shots overlap and lack independent coordinates.
- Resolution condition: Obtain an author-verified per-shot numerical table.
- Affected points: 0

### mathavan_2009_high_speed:snooker_to_pool_material_conversion_missing

- Metric: equipment_conversion
- Missing evidence: The source used 52.4 mm snooker balls and a Riley snooker table, not WPA Pool equipment.
- Resolution condition: Measure or publish a validated material and geometry conversion to WPA Pool.
- Affected points: 0

### mathavan_2009_high_speed:unmeasured_initial_spin

- Metric: initial_spin_rad_s
- Missing evidence: The experiment did not measure initial ball spin for the collision series.
- Resolution condition: Acquire synchronized translational and rotational measurements for each shot.
- Affected points: 0
