# Reference Physics Validation Report

Build: sha256:4b21760ee380f5d4d39ec55a2c26b216f8b409bee120f71a64a42ee0954f1123

## CALIBRATION

| Point | Case | Incident | Domain | Metric | Prediction | Experiment | Interval | Status |
|---|---|---:|---|---|---:|---:|---|---|
| corner_left_bottom_capture | corner_left_bottom_center | - | - | pocket_capture_event_count | 1 | 1.0 | [1.0, 1.0] | PASSED |
| side_left_capture | side_left_center | - | - | pocket_capture_event_count | 1 | 1.0 | [1.0, 1.0] | PASSED |
| side_left_jaw_event | side_left_jaw | - | - | pocket_jaw_event_count | 1 | 1.0 | [1.0, 1.0] | PASSED |
| side_left_narrow_capture | side_left_narrow | - | - | pocket_capture_event_count | 0 | 0.0 | [0.0, 0.0] | PASSED |

### Group error summary

| Group | Points | RMSE | Maximum absolute error | Pass rate |
|---|---:|---:|---:|---:|
| corner_left_bottom_center | 1 | 0.0 | 0.0 | 1.0 |
| side_left_center | 1 | 0.0 | 0.0 | 1.0 |
| side_left_jaw | 1 | 0.0 | 0.0 | 1.0 |
| side_left_narrow | 1 | 0.0 | 0.0 | 1.0 |

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
