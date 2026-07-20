# Reference Physics Validation Report

Build: sha256:95a387424bf50294312ca001520fa9a9951cf7d4507ed918fac8103571be3b0f

## CALIBRATION

| Point | Case | Incident | Domain | Metric | Prediction | Experiment | Interval | Status |
|---|---|---:|---|---|---:|---:|---|---|
| fig7_experimental_01 | fig7_experimental_01 | 26.299899999999997 | True | cushion_rebound_speed_cm_s | 24.323247466778522 | 23.4401 | [22.94003276452061, 23.940167235479393] | MODEL_MISMATCH_KNOWN |
| fig7_experimental_02 | fig7_experimental_02 | 35.4983 | True | cushion_rebound_speed_cm_s | 32.83080218293101 | 32.3511 | [31.84718404470587, 32.855015955294135] | PASSED |
| fig7_experimental_03 | fig7_experimental_03 | 41.8037 | True | cushion_rebound_speed_cm_s | 38.662588462449484 | 38.3074 | [37.80640873261104, 38.80839126738896] | PASSED |
| fig7_experimental_04 | fig7_experimental_04 | 65.0776 | True | cushion_rebound_speed_cm_s | 60.18977436841216 | 58.3087 | [57.80762531494796, 58.80977468505204] | MODEL_MISMATCH_KNOWN |
| fig7_experimental_05 | fig7_experimental_05 | 70.4133 | True | cushion_rebound_speed_cm_s | 65.12451190502232 | 66.51045 | [66.00875132300355, 67.01214867699646] | MODEL_MISMATCH_KNOWN |
| fig7_experimental_06 | fig7_experimental_06 | 84.4922 | True | cushion_rebound_speed_cm_s | 78.1454964868872 | 79.08925 | [78.5741635388112, 79.60433646118881] | MODEL_MISMATCH_KNOWN |

### Group error summary

| Group | Points | RMSE | Maximum absolute error | Pass rate |
|---|---:|---:|---:|---:|
| incident_low | 6 | 1.1170018167967672 | 1.8810743684121576 | 0.3333333333333333 |

## HOLDOUT

| Point | Case | Incident | Domain | Metric | Prediction | Experiment | Interval | Status |
|---|---|---:|---|---|---:|---:|---|---|
| - | - | - | - | - | - | - | - | - |

### Group error summary

| Group | Points | RMSE | Maximum absolute error | Pass rate |
|---|---:|---:|---:|---:|
| - | 0 | - | - | - |

## Failure accounting

- Known model mismatches: mathavan_2010_cushion:fig7_experimental_01:MODEL_MISMATCH:cushion_rebound_speed_cm_s, mathavan_2010_cushion:fig7_experimental_04:MODEL_MISMATCH:cushion_rebound_speed_cm_s, mathavan_2010_cushion:fig7_experimental_05:MODEL_MISMATCH:cushion_rebound_speed_cm_s, mathavan_2010_cushion:fig7_experimental_06:MODEL_MISMATCH:cushion_rebound_speed_cm_s
- New model mismatches: none
- Missing model mismatches: none
- Known reference limitations: none
- New reference limitations: none
- Missing reference limitations: none
- Unallowlistable failures: none

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
