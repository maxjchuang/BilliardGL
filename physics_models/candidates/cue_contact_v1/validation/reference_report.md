# Reference Physics Validation Report

Build: sha256:2ffa2fa2648438ad751e181799525c22c0930ec912364635d5898599304d7e53

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
| horizontal_slip_angular_speed | horizontal_slip | - | - | cue_impact_angular_speed_rad_s | 14.615132331848145 | 14.615131880386283 | [14.61498572906748, 14.615278031705087] | PASSED |
| horizontal_slip_energy_efficiency | horizontal_slip | - | - | cue_contact_energy_efficiency | 0.8570297888173641 | 0.8570297903578755 | [0.8570212200599719, 0.8570383606557791] | PASSED |
| horizontal_slip_linear_speed | horizontal_slip | - | - | cue_impact_linear_speed_cm_s | 32.4688720703125 | 32.46886988484525 | [32.4685451961464, 32.469194573544094] | PASSED |
| horizontal_slip_normal_impulse | horizontal_slip | - | - | cue_contact_normal_impulse_ns | 0.04733110502648972 | 0.047331104594630975 | [0.04733063128358503, 0.04733157790567692] | PASSED |
| horizontal_slip_tangential_impulse | horizontal_slip | - | - | cue_contact_tangential_impulse_ns | 0.028398664144355313 | 0.028398662756778586 | [0.028398378770151018, 0.028398946743406154] | PASSED |
| left_mirror_angular_speed | left_mirror | - | - | cue_impact_angular_speed_rad_s | -12.15125846862793 | -12.151258870418975 | [-12.151380383007679, -12.15113735783027] | PASSED |
| left_mirror_energy_efficiency | left_mirror | - | - | cue_contact_energy_efficiency | 0.7638888869917503 | 0.7638888888888888 | [0.76388125, 0.7638965277777777] | PASSED |
| left_mirror_linear_speed | left_mirror | - | - | cue_impact_linear_speed_cm_s | 69.44444274902344 | 69.44444444444444 | [69.44375, 69.44513888888889] | PASSED |
| left_mirror_normal_impulse | left_mirror | - | - | cue_contact_normal_impulse_ns | 0.11567034989416548 | 0.1156703489647612 | [0.11566919226127155, 0.11567150566825084] | PASSED |
| left_mirror_tangential_impulse | left_mirror | - | - | cue_contact_tangential_impulse_ns | 0.023611111300824973 | 0.02361111111111111 | [0.023610875, 0.02361134722222222] | PASSED |
| miscue_classification | miscue | - | - | cue_contact_energy_efficiency | 1.0 | 1.0 | [1.0, 1.0] | PASSED |
| right_mirror_angular_speed | right_mirror | - | - | cue_impact_angular_speed_rad_s | 12.15125846862793 | 12.151258870418975 | [12.15113735783027, 12.151380383007679] | PASSED |
| right_mirror_energy_efficiency | right_mirror | - | - | cue_contact_energy_efficiency | 0.7638888869917503 | 0.7638888888888888 | [0.76388125, 0.7638965277777777] | PASSED |
| right_mirror_linear_speed | right_mirror | - | - | cue_impact_linear_speed_cm_s | 69.44444274902344 | 69.44444444444444 | [69.44375, 69.44513888888889] | PASSED |
| right_mirror_normal_impulse | right_mirror | - | - | cue_contact_normal_impulse_ns | 0.11567034989416548 | 0.1156703489647612 | [0.11566919226127155, 0.11567150566825084] | PASSED |
| right_mirror_tangential_impulse | right_mirror | - | - | cue_contact_tangential_impulse_ns | 0.023611111300824973 | 0.02361111111111111 | [0.023610875, 0.02361134722222222] | PASSED |

### Group error summary

| Group | Points | RMSE | Maximum absolute error | Pass rate |
|---|---:|---:|---:|---:|
| horizontal_slip | 5 | 9.980069548205952e-07 | 2.185467252502349e-06 | 1.0 |
| left_mirror | 5 | 7.792166487053722e-07 | 1.6954210053654606e-06 | 1.0 |
| miscue | 1 | 0.0 | 0.0 | 1.0 |
| right_mirror | 5 | 7.792166487053722e-07 | 1.6954210053654606e-06 | 1.0 |

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
