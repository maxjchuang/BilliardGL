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
| fig7_experimental_07 | fig7_experimental_07 | 115.3559 | True | cushion_rebound_speed_cm_s | 106.68949183076619 | 106.88035 | [106.3800860071922, 107.38061399280781] | PASSED |
| fig7_experimental_08 | fig7_experimental_08 | 129.918 | True | cushion_rebound_speed_cm_s | 120.15757788717747 | 123.6813 | [123.16895320338661, 124.19364679661338] | MODEL_MISMATCH_KNOWN |
| fig7_experimental_09 | fig7_experimental_09 | 152.6487 | True | cushion_rebound_speed_cm_s | 141.18055743724108 | 145.56535 | [145.06303692780298, 146.067663072197] | MODEL_MISMATCH_KNOWN |
| fig7_experimental_10 | fig7_experimental_10 | 175.5444 | True | cushion_rebound_speed_cm_s | 162.35617111995816 | 164.52385 | [164.02216772754065, 165.02553227245937] | MODEL_MISMATCH_KNOWN |
| fig7_experimental_11 | fig7_experimental_11 | 199.1247 | True | cushion_rebound_speed_cm_s | 184.16491727158427 | 182.4138 | [181.9138, 182.9138] | MODEL_MISMATCH_KNOWN |
| fig7_experimental_12 | fig7_experimental_12 | 227.6377 | True | cushion_rebound_speed_cm_s | 210.53582763671875 | 203.1109 | [202.6108437531637, 203.61095624683628] | MODEL_MISMATCH_KNOWN |
| fig7_experimental_13 | fig7_experimental_13 | 252.95399999999998 | False | cushion_rebound_speed_cm_s | 233.95019568502903 | 227.9685 | [227.4670957219967, 228.4699042780033] | MODEL_MISMATCH_KNOWN |
| fig7_experimental_14 | fig7_experimental_14 | 270.0844 | False | cushion_rebound_speed_cm_s | 249.79364060238004 | 226.51885 | [226.01777203949882, 227.01992796050115] | MODEL_MISMATCH_KNOWN |
| fig7_experimental_15 | fig7_experimental_15 | 289.2779 | False | cushion_rebound_speed_cm_s | 267.54516610875726 | 238.2759 | [237.7759, 238.7759] | MODEL_MISMATCH_KNOWN |
| fig7_experimental_16 | fig7_experimental_16 | 298.4683 | False | cushion_rebound_speed_cm_s | 276.0450744628906 | 246.2069 | [245.7069, 246.7069] | MODEL_MISMATCH_KNOWN |
| fig7_experimental_17 | fig7_experimental_17 | 310.6437 | False | cushion_rebound_speed_cm_s | - | 262.0175 | [261.5148547473615, 262.5201452526385] | INTEGRATION_MISMATCH |
| fig7_experimental_18 | fig7_experimental_18 | 318.51 | False | cushion_rebound_speed_cm_s | - | 256.84955 | [256.3473411764017, 257.35175882359835] | INTEGRATION_MISMATCH |
| fig7_experimental_19 | fig7_experimental_19 | 340.454 | False | cushion_rebound_speed_cm_s | - | 273.23275 | [272.7162895247456, 273.7492104752544] | INTEGRATION_MISMATCH |

### Group error summary

| Group | Points | RMSE | Maximum absolute error | Pass rate |
|---|---:|---:|---:|---:|
| incident_extreme | 7 | 24.10655074184234 | 29.838174462890635 | 0.0 |
| incident_high | 1 | 7.424927636718763 | 7.424927636718763 | 0.0 |
| incident_middle | 5 | 2.8087254623193005 | 4.384792562758918 | 0.2 |

## Failure accounting

- Known model mismatches: mathavan_2010_cushion:fig7_experimental_08:MODEL_MISMATCH:cushion_rebound_speed_cm_s, mathavan_2010_cushion:fig7_experimental_09:MODEL_MISMATCH:cushion_rebound_speed_cm_s, mathavan_2010_cushion:fig7_experimental_10:MODEL_MISMATCH:cushion_rebound_speed_cm_s, mathavan_2010_cushion:fig7_experimental_11:MODEL_MISMATCH:cushion_rebound_speed_cm_s, mathavan_2010_cushion:fig7_experimental_12:MODEL_MISMATCH:cushion_rebound_speed_cm_s, mathavan_2010_cushion:fig7_experimental_13:MODEL_MISMATCH:cushion_rebound_speed_cm_s, mathavan_2010_cushion:fig7_experimental_14:MODEL_MISMATCH:cushion_rebound_speed_cm_s, mathavan_2010_cushion:fig7_experimental_15:MODEL_MISMATCH:cushion_rebound_speed_cm_s, mathavan_2010_cushion:fig7_experimental_16:MODEL_MISMATCH:cushion_rebound_speed_cm_s
- New model mismatches: none
- Missing model mismatches: mathavan_2010_cushion:fig7_experimental_07:MODEL_MISMATCH:cushion_rebound_speed_cm_s, mathavan_2010_cushion:fig7_experimental_17:MODEL_MISMATCH:cushion_rebound_speed_cm_s, mathavan_2010_cushion:fig7_experimental_18:MODEL_MISMATCH:cushion_rebound_speed_cm_s, mathavan_2010_cushion:fig7_experimental_19:MODEL_MISMATCH:cushion_rebound_speed_cm_s
- Known reference limitations: none
- New reference limitations: none
- Missing reference limitations: none
- Unallowlistable failures: mathavan_2010_cushion:fig7_experimental_17:INTEGRATION_MISMATCH:cushion_rebound_speed_cm_s, mathavan_2010_cushion:fig7_experimental_18:INTEGRATION_MISMATCH:cushion_rebound_speed_cm_s, mathavan_2010_cushion:fig7_experimental_19:INTEGRATION_MISMATCH:cushion_rebound_speed_cm_s

## Reference limitation details

### mathavan_2010_cushion:experimental_spin_change_unavailable

- Metric: post_collision_angular_velocity_rad_s
- Missing evidence: The source experiment did not measure spin change through cushion impact.
- Resolution condition: Acquire synchronized pre/post-impact angular-velocity measurements.
- Affected points: 0

### mathavan_2010_cushion:oblique_experimental_rebound_angle_unavailable

- Metric: cushion_rebound_angle_degrees
- Missing evidence: The source reports no experimental oblique-incidence rebound-angle series.
- Resolution condition: Acquire synchronized experimental incident/rebound angles for oblique cushion impacts.
- Affected points: 0

### mathavan_2010_cushion:rigid_cushion_domain_warning

- Metric: rigid_cushion_domain
- Missing evidence: The authors identify incident speeds above 2.5 m/s as outside the reliable rigid-cushion model domain.
- Resolution condition: Measure cushion deformation and contact response above 2.5 m/s.
- Affected points: 0

### mathavan_2010_cushion:snooker_cushion_to_pool_material_conversion_missing

- Metric: equipment_conversion
- Missing evidence: Riley Renaissance snooker cushion, cloth, and 52.5 mm balls are not the production Chinese Pool apparatus.
- Resolution condition: Publish a validated cushion/cloth/ball material conversion to production equipment.
- Affected points: 0
