# Reference Physics Validation Report

Build: sha256:bc2b49719309247eb63061c9a5581019fb898483d9c3c542b22085be47962e52

## CALIBRATION

| Point | Case | Incident | Domain | Metric | Prediction | Experiment | Interval | Status |
|---|---|---:|---|---|---:|---:|---|---|
| - | - | - | - | - | - | - | - | - |

### Group error summary

| Group | Points | RMSE | Maximum absolute error | Pass rate |
|---|---:|---:|---:|---:|
| - | 0 | - | - | - |

## HOLDOUT

| Point | Case | Incident | Domain | Metric | Prediction | Experiment | Interval | Status |
|---|---|---:|---|---|---:|---:|---|---|
| fig9_visible_01 | fig9_visible_01 | - | - | cushion_rebound_speed_cm_s | 21.675514221191406 | 21.7341 | [0.5196561274468259, 42.948543872553174] | PASSED |
| fig9_visible_02 | fig9_visible_02 | - | - | cushion_rebound_speed_cm_s | 138.86769104003906 | 145.2768 | [124.06356999272131, 166.4900300072787] | PASSED |
| fig9_visible_03 | fig9_visible_03 | - | - | cushion_rebound_speed_cm_s | 161.67892456054688 | 163.8257 | [142.6118803396534, 185.03951966034663] | PASSED |
| fig9_visible_04 | fig9_visible_04 | - | - | cushion_rebound_speed_cm_s | 182.72509765625 | 182.1712 | [160.957821863569, 203.384578136431] | PASSED |
| fig9_visible_05 | fig9_visible_05 | - | - | cushion_rebound_speed_cm_s | 209.61790466308594 | 203.2521 | [182.03871468030837, 224.46548531969165] | PASSED |
| fig9_visible_06 | fig9_visible_06 | - | - | cushion_rebound_speed_cm_s | 232.80575561523438 | 228.2271 | [207.01370840832593, 249.4404915916741] | PASSED |
| fig9_visible_07 | fig9_visible_07 | - | - | cushion_rebound_speed_cm_s | 248.7294158935547 | 226.1217 | [204.9082141308245, 247.33518586917552] | MODEL_MISMATCH_KNOWN |
| fig9_visible_08 | fig9_visible_08 | - | - | cushion_rebound_speed_cm_s | 313.56005859375 | 273.7346 | [252.51989013533984, 294.94930986466017] | MODEL_MISMATCH_KNOWN |
| sliding_deceleration_range | sliding_deceleration | - | - | sliding_deceleration_cm_s2 | 29.274413626277244 | 207.5 | [175.0, 240.0] | MODEL_MISMATCH_KNOWN |
| table1_shot_03_cue_speed | table1_shot_03 | - | - | post_collision_linear_velocity_cm_s | 90.1800765991211 | 92.5 | [86.136039, 98.863961] | PASSED |
| table1_shot_03_object_speed | table1_shot_03 | - | - | post_collision_linear_velocity_cm_s | 40.610511779785156 | 70.0 | [63.636039, 76.363961] | MODEL_MISMATCH_NEW |
| table1_shot_04_cue_speed | table1_shot_04 | - | - | post_collision_linear_velocity_cm_s | 124.70863342285156 | 127.5 | [121.136039, 133.863961] | PASSED |
| table1_shot_04_object_speed | table1_shot_04 | - | - | post_collision_linear_velocity_cm_s | 48.3087043762207 | 78.7 | [72.336039, 85.063961] | MODEL_MISMATCH_NEW |
| table1_shot_05_cue_speed | table1_shot_05 | - | - | post_collision_linear_velocity_cm_s | 43.95305252075195 | 36.5 | [30.136039, 42.863961] | MODEL_MISMATCH_NEW |
| table1_shot_05_object_speed | table1_shot_05 | - | - | post_collision_linear_velocity_cm_s | 33.20519256591797 | 58.1 | [51.736039000000005, 64.463961] | MODEL_MISMATCH_NEW |

### Group error summary

| Group | Points | RMSE | Maximum absolute error | Pass rate |
|---|---:|---:|---:|---:|
| fig9_visible | 8 | 16.600674247349236 | 39.82545859375 | 0.75 |
| sliding_summary | 1 | 178.22558637372276 | 178.22558637372276 | 0.0 |
| table1_extreme | 6 | 20.3135671762707 | 30.3912956237793 | 0.3333333333333333 |

## Failure accounting

- Known model mismatches: mathavan_2009_high_speed:fig9_visible_07:MODEL_MISMATCH:cushion_rebound_speed_cm_s, mathavan_2009_high_speed:fig9_visible_08:MODEL_MISMATCH:cushion_rebound_speed_cm_s, mathavan_2009_high_speed:sliding_deceleration:MODEL_MISMATCH:sliding_deceleration_cm_s2
- New model mismatches: mathavan_2009_high_speed:table1_shot_03:MODEL_MISMATCH:post_collision_linear_velocity_cm_s, mathavan_2009_high_speed:table1_shot_04:MODEL_MISMATCH:post_collision_linear_velocity_cm_s, mathavan_2009_high_speed:table1_shot_05:MODEL_MISMATCH:post_collision_linear_velocity_cm_s
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
