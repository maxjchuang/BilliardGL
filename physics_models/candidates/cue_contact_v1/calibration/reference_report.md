# Reference Physics Validation Report

Build: sha256:2ffa2fa2648438ad751e181799525c22c0930ec912364635d5898599304d7e53

## CALIBRATION

| Point | Case | Incident | Domain | Metric | Prediction | Experiment | Interval | Status |
|---|---|---:|---|---|---:|---:|---|---|
| center_hit_energy_efficiency | center_hit | - | - | cue_contact_energy_efficiency | 0.7462686547247279 | 0.746268656716418 | [0.7462611940298508, 0.7462761194029851] | PASSED |
| center_hit_linear_speed | center_hit | - | - | cue_impact_linear_speed_cm_s | 74.62686920166016 | 74.6268656716418 | [74.62611940298508, 74.62761194029851] | PASSED |
| center_hit_normal_impulse | center_hit | - | - | cue_contact_normal_impulse_ns | 0.12686567263763607 | 0.12686567164179105 | [0.12686440298507462, 0.12686694029850748] | PASSED |
| center_hit_tangential_impulse | center_hit | - | - | cue_contact_tangential_impulse_ns | 0 | 0.0 | [-1e-07, 1e-07] | PASSED |
| negative_vertical_stick_angular_speed | negative_vertical_stick | - | - | cue_impact_angular_speed_rad_s | 12.15125846862793 | 12.151258870418975 | [12.15113735783027, 12.151380383007679] | PASSED |
| negative_vertical_stick_energy_efficiency | negative_vertical_stick | - | - | cue_contact_energy_efficiency | 0.7638888869917503 | 0.7638888888888888 | [0.76388125, 0.7638965277777777] | PASSED |
| negative_vertical_stick_linear_speed | negative_vertical_stick | - | - | cue_impact_linear_speed_cm_s | 69.44444274902344 | 69.44444444444444 | [69.44375, 69.44513888888889] | PASSED |
| negative_vertical_stick_normal_impulse | negative_vertical_stick | - | - | cue_contact_normal_impulse_ns | 0.11567034989416548 | 0.1156703489647612 | [0.11566919226127155, 0.11567150566825084] | PASSED |
| negative_vertical_stick_tangential_impulse | negative_vertical_stick | - | - | cue_contact_tangential_impulse_ns | 0.023611111300824973 | 0.02361111111111111 | [0.023610875, 0.02361134722222222] | PASSED |
| positive_vertical_stick_angular_speed | positive_vertical_stick | - | - | cue_impact_angular_speed_rad_s | -12.15125846862793 | -12.151258870418975 | [-12.151380383007679, -12.15113735783027] | PASSED |
| positive_vertical_stick_energy_efficiency | positive_vertical_stick | - | - | cue_contact_energy_efficiency | 0.7638888869917503 | 0.7638888888888888 | [0.76388125, 0.7638965277777777] | PASSED |
| positive_vertical_stick_linear_speed | positive_vertical_stick | - | - | cue_impact_linear_speed_cm_s | 69.44444274902344 | 69.44444444444444 | [69.44375, 69.44513888888889] | PASSED |
| positive_vertical_stick_normal_impulse | positive_vertical_stick | - | - | cue_contact_normal_impulse_ns | 0.11567034989416548 | 0.1156703489647612 | [0.11566919226127155, 0.11567150566825084] | PASSED |
| positive_vertical_stick_tangential_impulse | positive_vertical_stick | - | - | cue_contact_tangential_impulse_ns | 0.023611111300824973 | 0.02361111111111111 | [0.023610875, 0.02361134722222222] | PASSED |
| stick_boundary_inner_angular_speed | stick_boundary_inner | - | - | cue_impact_angular_speed_rad_s | 22.486534118652344 | 22.486534794626355 | [22.48630992927841, 22.4867596599743] | PASSED |
| stick_boundary_inner_energy_efficiency | stick_boundary_inner | - | - | cue_contact_energy_efficiency | 0.8301498640978695 | 0.8301498655809846 | [0.8301415640823288, 0.8301581670796404] | PASSED |
| stick_boundary_inner_linear_speed | stick_boundary_inner | - | - | cue_impact_linear_speed_cm_s | 49.9559211730957 | 49.95592188794573 | [49.95542232872685, 49.95642144716461] | PASSED |
| stick_boundary_inner_normal_impulse | stick_boundary_inner | - | - | cue_contact_normal_impulse_ns | 0.07282264549324594 | 0.07282264485736564 | [0.07282191663091707, 0.07282337308381422] | PASSED |
| stick_boundary_inner_tangential_impulse | stick_boundary_inner | - | - | cue_contact_tangential_impulse_ns | 0.043693586140966655 | 0.04369358575943847 | [0.04369314882358088, 0.04369402269529606] | PASSED |

### Group error summary

| Group | Points | RMSE | Maximum absolute error | Pass rate |
|---|---:|---:|---:|---:|
| center_hit | 4 | 1.7650095317569184e-06 | 3.53001836117528e-06 | 1.0 |
| negative_vertical_stick | 5 | 7.792166487053722e-07 | 1.6954210053654606e-06 | 1.0 |
| positive_vertical_stick | 5 | 7.792166487053722e-07 | 1.6954210053654606e-06 | 1.0 |
| stick_boundary_inner | 5 | 4.399895841059892e-07 | 7.148500245079958e-07 | 1.0 |

## HOLDOUT

| Point | Case | Incident | Domain | Metric | Prediction | Experiment | Interval | Status |
|---|---|---:|---|---|---:|---:|---|---|
| - | - | - | - | - | - | - | - | - |

### Group error summary

| Group | Points | RMSE | Maximum absolute error | Pass rate |
|---|---:|---:|---:|---:|
| - | 0 | - | - | - |

## Failure accounting

- Known model mismatches: none
- New model mismatches: none
- Missing model mismatches: none
- Known reference limitations: none
- New reference limitations: none
- Missing reference limitations: none
- Unallowlistable failures: none

## Reference limitation details

None.
